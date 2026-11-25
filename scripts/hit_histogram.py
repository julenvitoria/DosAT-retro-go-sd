#!/usr/bin/env python3
"""
Stop on one interpreter address N times and histogram what was in a register.

A profile says how much an opcode costs; it cannot say WHICH of the things the
handler does the guest was asking for. `FF` is seven instructions in one byte
(INC/DEC/CALL/CALLF/JMP/JMPF/PUSH, chosen by the ModRM reg field), and a
handler's cheap and expensive arms differ by hundreds of cycles, so "FF costs
278 cyc" is unactionable until the MIX is known. This answers it: a hardware
breakpoint on the handler's dispatch point, one stop per execution, and a
histogram of a register field read at each stop.

gwemu ONLY in practice (tier 2). The COUNTS are exact; nothing here is timing.
Each stop is a GDB round trip, so a few thousand hits take seconds of host time
and the guest is frozen meanwhile -- which is fine, because what is read is the
instruction stream, not anything that depends on time.

The breakpoint is armed only once the workload is the one you mean: `--after`
waits a number of seconds, and `--climb SYM` then waits until that counter has
moved (x86_modey_presents climbing is "Wolf3D is in gameplay"). Without that the
histogram is of whatever the title was doing at boot.

    # which GRP5 sub-op does Wolf3D gameplay run? (r1 = ModRM, reg = bits 5:3)
    python3 scripts/hit_histogram.py --retrogo --sd build/run/sd-wolf.img \\
        --timeline timelines/wolf3d-play.tl --after 22 --climb x86_modey_presents \\
        --at GRP5_DISPATCH_16 --field r1:3:3 --names INC,DEC,CALL,CALLF,JMP,JMPF,PUSH \\
        --site r10 --hits 3000

`--site REG` also tallies the hits by that register's value: r10 is the guest's
LINEAR EIP while the interpreter runs (past the bytes already fetched), so the
top entries name the guest instructions responsible.
"""

import argparse
import collections
import os
import sys
import time

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from scripts.common.probe import load_symbol_maps, wait_for_value  # noqa: E402
from scripts.common.target import Image, add_target_args, open_target  # noqa: E402
from scripts.common.watchpoint import (                  # noqa: E402
    clear_code_breakpoint, drain_stop_replies, set_code_breakpoint,
    step_over_breakpoint,
    wait_for_stop,
)

FW_DIR = "references/game-and-watch-retro-go-sd/build"
CORE_MAGIC = 0x54494E59


def parse_at(syms, spec):
    name, plus, off = spec.partition("+")
    base = int(name, 0) if name.startswith("0x") else syms.get(name)
    if base is None:
        raise SystemExit(f"--at: no symbol {name!r} in the core ELF")
    return base + (int(off, 0) if plus else 0)


def parse_field(spec):
    """REG[:LSB:WIDTH] -> (reg, lsb, mask)."""
    parts = spec.split(":")
    reg = parts[0]
    lsb = int(parts[1], 0) if len(parts) > 1 else 0
    width = int(parts[2], 0) if len(parts) > 2 else 32
    return reg, lsb, (1 << width) - 1


def main():
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    add_target_args(p)
    p.add_argument("--retrogo", action="store_true",
                   help="boot retro-go + our packed core")
    p.add_argument("--firmware-dir", default=FW_DIR)
    p.add_argument("--sd", default="sdcard_retrogo.img")
    p.add_argument("--elf", default=None)
    p.add_argument("--at", required=True, metavar="SYM[+OFF]|0xADDR",
                   help="interpreter address to stop at (e.g. GRP5_DISPATCH_16)")
    p.add_argument("--field", required=True, metavar="REG[:LSB:WIDTH]",
                   help="register bit field to histogram at each stop")
    p.add_argument("--names", default="",
                   help="comma-separated labels for field values 0,1,2,...")
    p.add_argument("--site", default="", metavar="REG",
                   help="also tally hits by this register's value (r10 = guest "
                        "linear EIP)")
    p.add_argument("--next", default="", metavar="SYM[+OFF]|0xADDR",
                   help="the instruction executed right after --at, used to "
                        "step off the breakpoint (default: --at + 4, i.e. --at "
                        "is a 32-bit Thumb instruction that falls through)")
    p.add_argument("--hits", type=int, default=2000)
    p.add_argument("--after", type=float, default=0.0,
                   help="seconds to let the target run before arming")
    p.add_argument("--climb", default="",
                   help="then wait until this counter changes before arming")
    p.add_argument("--timeout", type=float, default=10.0,
                   help="seconds to wait for each hit")
    p.add_argument("--top", type=int, default=12)
    args = p.parse_args()

    if args.retrogo:
        fw = args.firmware_dir
        elf = args.elf or "build/core/dos_core.elf"
        image = Image(bank1=os.path.join(fw, "qemu_bank1.bin"),
                      bank2=os.path.join(fw, "qemu_bank2.bin"),
                      extflash=os.path.join(fw, "extflash.bin"),
                      sdcard=args.sd, elf=elf, intflash_bank=1)
        if args.timeline is None:
            args.timeline = "timelines/start-preselected-game.tl"
        fw_elf = os.path.join(fw, "gw_retro_go.elf")
    else:
        elf = args.elf or "test_core.elf"
        image = Image(bank1="qemu_bank1.bin", bank2="qemu_bank2.bin",
                      extflash="extflash.bin", sdcard="sdcard.img",
                      elf=elf, intflash_bank=1)
        fw_elf = None

    syms, _ = load_symbol_maps(fw_elf, elf)
    _, core = load_symbol_maps(elf)
    at = parse_at(core[0], args.at)
    nxt = parse_at(core[0], args.next) if args.next else at + 4
    reg, lsb, mask = parse_field(args.field)
    names = [n for n in args.names.split(",") if n]

    target = open_target(args, image)
    target.start()
    hist = collections.Counter()
    sites = collections.Counter()
    got = 0
    try:
        b = target.backend
        if "core_magic" in syms:
            wait_for_value(b, syms["core_magic"], CORE_MAGIC, 30.0)
        if args.after:
            time.sleep(args.after)
        if args.climb:
            sym = syms.get(args.climb)
            if sym is None:
                raise SystemExit(f"--climb: no symbol {args.climb!r}")
            v0 = b.read_uint32(sym)
            deadline = time.time() + 30.0
            while b.read_uint32(sym) == v0 and time.time() < deadline:
                time.sleep(0.25)
            if b.read_uint32(sym) == v0:
                raise SystemExit(f"{args.climb} never moved: the workload you "
                                 f"meant is not running; nothing armed")
            print(f"{args.climb}: {v0} -> {b.read_uint32(sym)}, arming")
        b.halt()
        drain_stop_replies(b)
        set_code_breakpoint(b, at)
        for _ in range(args.hits):
            if wait_for_stop(b, args.timeout) is None:
                print(f"no hit within {args.timeout}s; stopping at {got}")
                break
            pc = b.read_register("pc")
            if (pc & ~1) != (at & ~1):
                print(f"stopped at 0x{pc:08x}, not at the breakpoint; "
                      f"giving up after {got} hits")
                break
            v = (b.read_register(reg) >> lsb) & mask
            hist[v] += 1
            if args.site:
                sites[b.read_register(args.site)] += 1
            got += 1
            if not step_over_breakpoint(b, at, nxt, args.timeout):
                print("never reached --next: wrong successor address")
                break
        b.halt()
        drain_stop_replies(b)
        try:
            clear_code_breakpoint(b, at)
        except Exception:
            pass
    finally:
        target.stop()

    print(f"\n{got} hits at {args.at} (0x{at:08x}), field {args.field}")
    for v, n in hist.most_common():
        label = names[v] if v < len(names) else f"0x{v:x}"
        print(f"  {label:>10}  {n:7d}  {100.0 * n / max(got, 1):5.1f}%")
    if args.site:
        print(f"\ntop {args.top} by {args.site}:")
        for v, n in sites.most_common(args.top):
            print(f"  0x{v:08x}  {n:7d}  {100.0 * n / max(got, 1):5.1f}%")
    print("\nCOUNTS ONLY: gwemu does not model timing (docs/06).")
    return 0 if got else 1


if __name__ == "__main__":
    sys.exit(main())
