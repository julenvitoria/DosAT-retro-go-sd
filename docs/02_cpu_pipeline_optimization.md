# Cortex-M7 Pipeline & CPU Optimization Strategy

## 1. The Cortex-M7 Microarchitecture
The STM32H7B0VB utilizes an ARM Cortex-M7 core, which features a **6-stage, superscalar, dual-issue pipeline**. 
This means the CPU can execute two instructions simultaneously per clock cycle, provided there are no data dependencies or resource conflicts.

Writing raw Thumb v2 assembly is not enough; we must actively structure the code to prevent pipeline stalls and branch mispredictions.

## 2. ITCM and DTCM Pinning
- **ITCM (64KB)**: The main `cpu_exec` loop (which fetches, decodes, and executes x86 instructions) will be entirely pinned to the ITCM. This ensures instructions are fed into the pipeline with 0 wait states, completely bypassing the L1 instruction cache overhead.
- **DTCM (128KB)**: The primary CPU state structure (x86 registers, EIP, EFLAGS, segment descriptors) will reside at the very beginning of the DTCM. The 8KB Memory Page Table (see `01_memory_architecture.md`) will also reside here, guaranteeing 0-wait-state data access for memory operations.

## 3. Dual-Issue Optimization
To maximize IPC (Instructions Per Clock), we must avoid Read-After-Write (RAW) dependencies between consecutive instructions.

**Bad (Stalls pipeline):**
```assembly
LDR R0, [R1]
ADD R0, R0, #1  @ Stalls: depends on the load completing
```

**Good (Dual-issue capable):**
```assembly
LDR R0, [R1]
LDR R2, [R3]    @ Independent: executes alongside or hides load latency
ADD R0, R0, #1
```
The x86 emulator state machine will be hand-interleaved to keep both issue ports busy, particularly during the instruction decoding phase.

## 4. Branch Prediction
The M7 has sophisticated static and dynamic branch prediction. 
- Avoid using `IT` (If-Then) blocks for anything other than extremely short 1-2 instruction conditionals. On the M7, `IT` blocks can sometimes perform worse than standard branches because they serialize execution.
- Order conditional branches such that the most common path (e.g., no page fault, no interrupt pending) falls through without branching.

## 5. Register Mapping
Given ARM's limited register file compared to the heavy state of an x86 CPU, we will permanently map the most accessed x86 state variables to high ARM registers (`R8`-`R11`) within the main execution loop:
- `R8`: Pointer to the x86 CPU context in DTCM.
- `R9`: Pointer to the DTCM Page Table.
- `R10`: Current x86 Instruction Pointer (EIP).
- `R11`: Current x86 EFLAGS cache.
This reduces memory loads/stores dramatically during tight loops.

## 6. CPU State Structure (DTCM)
To enforce this architecture, the entire CPU context struct (which holds EAX, EBX, EIP, EFLAGS, segment descriptors, etc.) must be explicitly mapped to the `.dtcm_bss` section in C:
```c
typedef struct {
    uint32_t regs[8]; // EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    uint32_t eip;
    uint32_t eflags;
    // ... Segment descriptors, CR registers, etc.
} X86CPUState;

// Force the entire x86 register state into 0-wait-state DTCM memory
X86CPUState cpu_state __attribute__((section(".dtcm_bss")));
```

## 7. Assembly Core Loop (ITCM)
The outer `cpu_exec` dispatch loop is written in pure ARM Thumb v2 assembly and pinned to the `.itcram_hot` section. The entry prologue establishes the high-register mapping to our DTCM structures.
```assembly
    .syntax unified
    .cpu cortex-m7
    .thumb

    .section .itcram_hot, "ax", %progbits
    .global cpu_exec
    .type cpu_exec, %function

cpu_exec:
    // PROLOGUE: Save ARM host state
    PUSH {R4-R11, LR}

    // Load static pointers into high registers
    LDR R8, =cpu_state          // R8 = CPU Context in DTCM
    LDR R9, =guest_page_table   // R9 = Page Table in DTCM
    LDR R10, [R8, #EIP_OFFSET]  // R10 = Cached EIP
    LDR R11, [R8, #EFLAGS_OFFSET] // R11 = Cached EFLAGS

.fetch_decode_loop:
    // ... Thumb v2 optimized fetch and decode logic ...
    
    // EPILOGUE: Restore state
    STR R10, [R8, #EIP_OFFSET]
    STR R11, [R8, #EFLAGS_OFFSET]
    POP {R4-R11, PC}
```

## 8. Where an ordinary instruction's host instructions go (2026-11-19)

**Tier 0: static disassembly of `build/core/dos_core.elf`, 16-bit pass.** Host
instruction counts are exact; no cycle figure here is a measurement. The
weights are **tier 2 counts** (`X86_PROFILE=1` under gwemu, counts exact,
cycles meaningless): WOLF3D 15.8 M instructions of menus/attract loop -- NOT
gameplay, the instrumented core does not reach it inside a 60 s timeline --
and KEEN4 10.9 M of startup/intro.

| component | host insns | note |
|---|---|---|
| dispatch, every instruction | 20 | one shared `LDR PC`; 335 handlers return with `B.W` |
| -- code fold + reloc proof | 8 | recomputed per instruction, same page almost always |
| -- PIT counter + seam flush | 6 | load, sub, store, two branches |
| 16-bit ModRM decoder, `[bp+d8]` | ~29 | data-dependent branch tree shared by all sites |
| operand fold + seam check | 7 | already minimal |
| flags after an ALU op | 16-17 | `MRS` + 5 serial `BFI` on R11, PF always computed; 96 sites |
| ALU sub-op select | 2 | second shared indirect branch (`TBH`) |
| `Jcc` condition (before `dc35080`) | 13 (`JZ`) .. 37 (`JLE`) | `EVAL_COND` compare chain |
| register index | 3 per operand | `AND`/`LSL #2`/`ADD #8`; 90 sites |

Totals with dispatch: `MOV r,r` 34, `MOV ax,[bp+6]` 70, `MOV [bp+6],ax` 74,
`CMP r,[mem]` 94, `ADD r,r` 65, `INC r` 44, `PUSH`/`POP` 40, `CALL` 47,
`RET` 38. At ~57 cyc/insn that is about one host instruction per cycle on a
dual-issue core: stalls and mispredicts, not instruction count alone.

Most frequent opcodes, % of dispatched instructions (WOLF3D / KEEN4):
`83` GRP1 Ev,Ib 13.8 / 10.4, `8B` 12.7 / 10.5, `74` 6.1 / 4.7, `75` 5.8 / 3.0,
`89` 5.0 / 4.5, `8A` 4.9 / -, `D1` 4.2 / 1.4, `72` 3.1 / 3.6, `26` 2.9 / 2.3,
`3B` 2.7 / 2.0. Short `Jcc` is ~17% in WOLF3D; `83`+`8B` another 26%.

**The planar store hook (171 cyc, `PL_PLANE` byte).** Per store it calls
`x86_vga_planar_active()` through a veneer out of ITCM, calls `memcpy` through
another veneer for the 4-byte all-planes case, bumps three `volatile` globals,
calls `vga_cell` (70 insns, not inlined) and saves 9 registers + 36 B of stack
for the generic size loop. `docs/09` §9 put the cell itself at 33 of the 171.
This is restructuring the C, not the rejected "planar_store in assembly".

### Speed plan, in order. Every saving is an ESTIMATE until tier 3

| # | change | est. saving | ITCM | state |
|---|---|---|---|---|
| A1 | dedicated short `Jcc` for 72-77, 7C-7F | 15-35 cyc per Jcc | +424 B | DONE `dc35080`. Tier 3: 11.62 -> 12.03 fps (+3.5%); dedicated Jcc 18 cyc, generic 77 |
| A2 | register index in 1 insn + narrow encodings in hot handlers | 1-2 cyc/insn | -700 B | next |
| A3 | table-driven 16-bit ModRM (24 straight-line entries) | 10-18 cyc per memory operand | +500 B | |
| B1 | planar hook: no veneers, leaf entry, section anchors | 171 -> 80-100 cyc, up to ~10% of wall | +144 B | DONE (with B2a). Tier 3: `88` 209 -> 152 cyc; alone +5% ips, fps flat |
| B2a | the leaf takes word/dword stores too (contiguous cells) | not estimated | 0 | DONE. Tier 3 with B1: 13.0 -> 17.0 fps (+31%), 1.445 -> 1.822 M ips |
| A4 | flags: break the `BFI` chain, lazy PF/AF | ~6 cyc per ALU op | -300 B | |
| A5 | reuse `code_k` within a page; seam flush via the PIT slow path | 3-5 cyc/insn | 0 | high risk (fold invalidation) |
| A6 | replicated dispatch tail in the hottest handlers | uncertain | +600 B | |
| B2 | `REP STOSW`/`STOSB`: one cell decision per run | ~3% of wall | +150 B | |
| B3/B4 | aperture reads, VGA `OUT` | re-measure first (code changed since the 1,242 figure) | | |

Considered and dropped before building: skipping the VGA check in
`WRITEBACK_REG` for register destinations (~0.25 cyc/insn, needs duplicated ALU
bodies). If ITCM runs out, `cga_mode4_blit` (1,508 B, per frame) leaves first,
then `font8x8` (2,048 B). **To measure a step:** `guest_ips_achieved` and
`x86_modey_presents`/s over `timelines/wolf3d-hw.tl`, oc level 0, base core
`7ebc85d` against the step's commit. A step that gains nothing measurable is
reverted and listed under REJECTED in `CLAUDE.md` with its number.

### Tier 3 profile of WOLF3D GAMEPLAY (2026-11-20) -- it reorders the plan

`X86_PROFILE=1` on the console, oc 0, gate `x86_modey_presents` 3 -> 162 in a
14 s window, 18.1 M instructions; overhead-corrected (23 cyc/dispatch). The
weights above were menus under gwemu; these are gameplay cycles:

| share of guest cycles | |
|---|---|
| `88` MOV Eb,Gb | 30.6% -- 2.41 M x 209 cyc, the aperture byte store |
| `AB` REP STOSW | 13.2% -- 20.6 k x 10,518 cyc, the hook once per element |
| `8B` + `8A` + `89` | 17.4% at 66 cyc |
| ALU r/m (01-3B) | 8.7% at 83 cyc; GRP1 imm 5.1%; GRP3 3.5% |
| `FF` GRP5 | 4.0% at **278 cyc** each (238 k) -- a MIX, see "FF, diagnosed" below |
| `EC` IN AL,DX | 3.7% at **372 cyc** each, 11.5 k/s -- port not identified |
| short Jcc | 2.1%; segment prefixes 13.7% of dispatches, 0.3% of cycles |

So the video store was 44% of the guest's cycles and B1/B2 went first. What
that left: the `88` store is still 152 cyc against 57 for `89` to ordinary
memory (the asm stub's six-register save and the plane write itself), `REP
STOSW` still re-enters the handler per element (B2 proper: one call per run),
and `FF`/`EC` have not been looked at. FM music is a separate 25% of wall
(`docs/03` 4g, `[sblaster] oplrate`). JSON: `build/artifacts/day2-profile-wolf3d-play.json`.

### `FF`, diagnosed (2026-11-24) -- what the 278 cycles are an average OF

**Tier 2 counts** (`scripts/hit_histogram.py` on `GRP5_DISPATCH_16`, 2,000
consecutive hits in WOLF3D gameplay, `x86_modey_presents` climbing):

| sub-op | share | where (guest) |
|---|---|---|
| `/6` PUSH m16 | **67.1%** | four `push word [mem]` per wall column in the height calculation at lin `F4FD`-`F52D` (Borland C pushing two longs for `FixedByFrac`) |
| `/3` CALL FAR m16:16 | **21.2%** | `call far [bp+0]` in `ScalePost` (lin `F5C5`), one per wall post, into a compiled scaler |
| `/0` INC, `/1` DEC | 8.1%, 3.2% | scattered |
| `/4` JMP | 0.4% | |

**Tier 0, host instructions per execution** (disassembly, 16-bit pass, dispatch
included): PUSH m16 ~85, CALL FAR ~138, INC m16 ~70 -- a weighted ~95. The
handler's own share of that, for every arm: a SEVEN-WAY compare chain on the
sub-op (PUSH is the last arm, 14 instructions), and for CALL FAR the far
pointer's offset folded and loaded TWICE (`GRP5_MEM` already did it), two
separate `PUSH_V` folds for CS and IP, and a `BL` into the general `SET_SREG`.

**What does NOT close:** `E8` runs ~47 host instructions in 30 cycles; ~95 at
that rate is ~60, not 278. The rest is charged to `FF` but is not visibly its
code -- the profiler closes a slot at the NEXT dispatch, after that dispatch's
fold and `.code_reloc`, so the next instruction's fetch (for CALL FAR, the
first byte of a scaler that is a different block of guest memory every post)
lands in this row; or D-cache misses; or mispredicts on the compare chain.
**Tier 0/2 cannot separate these.** `X86_PROFILE=1` now splits `FF` by sub-op
(rows `FF /0`..`FF /6`): one console run says which arm holds the cycles, and
that decides the change -- a sub-op jump table (all arms), a CALL FAR without
the double fetch and with one 4-byte push (the 21%), or nothing in `FF` at all
if the cost is the scaler fetch.
