/*
 * x86_opl.c -- YM3812 (OPL2) FM tone generation: the ADAPTER.
 *
 * The synthesis is emu8950 (lib/emu8950/, vendored unmodified from the owner's
 * G&W Doom port; its README-tinyemu.md carries the provenance, the build
 * configuration and the measurements that chose it). This file is the whole of
 * what THIS core adds: the register forward, the "is anything sounding"
 * early-out, the resampler, the mix, and the counters the tests read.
 *
 * ---------------------------------------------------------------------------
 * WHY emu8950 AND NOT THE HAND-REDUCED NUKED-OPL3 THAT WAS HERE FIRST
 *
 * The first version of this file was Nuked-OPL3 cut down to an OPL2 by hand.
 * It worked -- WOLF3D played its music at tier 2 with the golden at 0.000% --
 * and it was replaced anyway, on measurement. Both compiled -O2, same nine-
 * voice patch, same 49,716 Hz, 994,320 chip samples:
 *
 *   9 channels   emu8950 36.5 ns/sample   Nuked-derived 88.0   2.41x
 *   5 channels   emu8950 26.6 ns/sample   Nuked-derived 48.8   1.84x
 *
 * A 2.4x on the axis that made FM "does not fit" in the first place is not a
 * close call. Two more reasons, both about risk rather than speed: emu8950 IS
 * a YM3812, which is the chip x86_io.c reports, where the other was an OPL3
 * core with the OPL3 removed BY ME -- 600 lines of my own transcription to be
 * subtly wrong in; and emu8950 is already running on this exact part under
 * retro-go in the owner's Doom port, so its behaviour on this silicon is
 * evidence rather than a hope.
 *
 * IT COST RAM AND THE NUMBER IS NOT SMALL: 22,951 B against 4,679 B, +18,272 B
 * of RAM_EMU, which is FOUR guest page frames. docs/03 4f has the accounting
 * and the reason it was still judged worth paying. `X86_OPL_FM=0` gives every
 * byte of it back.
 *
 * ---------------------------------------------------------------------------
 * THE RESAMPLER IS LINEAR, AND THAT IS NOT A PREFERENCE
 *
 * 49,716 Hz (the YM3812's own clk/72) into 48,000 Hz, 16.16 accumulator,
 * fraction carried across frame boundaries, and the two bracketing chip
 * samples LINEARLY INTERPOLATED. Taken whole from the owner's opl_gnw.c, which
 * records what the cheap version sounds like:
 *
 *     "nearest-neighbour resampling produced an audible ~1.7 kHz buzz on held
 *      notes"
 *
 * This file's first version used a zero-order hold -- exactly that scheme --
 * because x86_sb_render_mix() next door uses one and it seemed consistent. It
 * is not the same problem: the SB path holds a 6-11 kHz sample stream where
 * the ear is forgiving, and a sustained FM note is where it is not. One
 * multiply-add per output sample, 800 per frame, in host C.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS MODELLED, AND WHAT IS NOT
 *
 * Nine channels, two operators each, four waveforms, the ADSR envelope, key
 * scaling of level and rate, vibrato and tremolo, feedback and both connection
 * modes -- everything an AdLib melodic track uses.
 *
 * PERCUSSION MODE (register BDh bit 5) is NOT synthesised: emu8950 is built
 * with EMU8950_NO_PERCUSSION_MODE, following the owner's Doom configuration.
 * When a guest turns it on, x86_opl_rhythm_muted counts it and the three
 * affected channels go quiet rather than playing the rhythm registers as if
 * they were notes. THE TIMERS AND THE STATUS BYTE ARE NOT emu8950's EITHER
 * (EMU8950_NO_TIMER): x86_io.c has owned them since the detection work and
 * still does -- two chips answering the same status port is how a detection
 * probe gets an answer that agrees with neither.
 *
 * There is no per-instruction cost. The only clock is the frame loop, exactly
 * as for the DMA engine: x86_opl_render_mix() is called once per rendered
 * frame from src/main.c's audio phase and the chip advances by the samples
 * that call produced.
 */

#include <string.h>
#include "x86_opl.h"
#include "emu8950.h"

/* Published counters. */
volatile uint32_t x86_opl_samples;
volatile uint32_t x86_opl_keyons;
volatile uint32_t x86_opl_chip_steps;
volatile uint32_t x86_opl_slots_run;
volatile uint32_t x86_opl_clips;
volatile uint32_t x86_opl_rhythm_muted;
volatile uint32_t x86_opl_frame_cycles;

/* The chip. OPL_new() returns the single static instance under -DDOOMX and
   allocates nothing -- see lib/emu8950/README-tinyemu.md, and note that this
   core has no malloc to give it if that flag ever comes off. */
static OPL *opl;
static uint8_t inited;

/* Chip samples are pulled one chunk at a time rather than one at a time:
   OPL_calc_buffer_linear() amortises its per-call setup over the chunk, which
   is most of what EMU8950_LINEAR buys. It does NOT have to hold a whole
   frame: 256 int32 is 1,024 B where a frame's 829 chip samples would be 3,316,
   and on this part a kilobyte of RAM_EMU is a quarter of a guest page frame.
   Four refills per frame amortise the per-call setup perfectly well; sizing
   this to the frame would buy a few hundred cycles and cost the guest most of
   a page. The resampler needs the chip samples either side of each output
   sample, which is what s0/s1 are -- they carry across the refill boundary so
   the chunking is inaudible. */
/* The chunk is a fixed span of CHIP TIME (256 chip samples, ~5.15 ms), not a
   fixed count: decimated, a refill renders OPL_CHUNK >> emu8950_decim_shift
   samples, so a register write still lands within the same ~5 ms and the
   idle-channel check still runs as often. OPL_CHUNK sizes the buffer. */
#define OPL_CHUNK 256u
#if EMU8950_EXT_MEM
/*
 * THE CHIP'S MEMORY IS HANDED OVER, NOT STATIC (2026-10-26). One block holds
 * emu8950's OPL instance, its two per-call buffers (sized to OPL_CHUNK through
 * EMU8950_SAMPLE_BUF) and this file's chunk. Who provides it:
 *
 *   X86_OPL_DYNAMIC=1  src/main.c allocates it from the firmware's RAM pool
 *                      BEFORE the guest's frame pool is claimed -- and only if
 *                      the title's .cfg did not say `oplmode = none`. That is
 *                      the point: a title with no FM chip gives those bytes to
 *                      the guest instead (the main build, where this .bss is
 *                      RAM_EMU).
 *   X86_OPL_DYNAMIC=0  the static block below. The overlays build places this
 *                      file's .bss in the RAM_UC bonus area, so it costs the
 *                      guest nothing there, and the bare-metal harness needs no
 *                      host to call anything.
 *
 * NULL means NO CHIP: x86_opl_present() answers 0 and x86_io.c makes the AdLib
 * ports open bus, so a title probes, finds nothing and falls back.
 */
#ifndef X86_OPL_DYNAMIC
#define X86_OPL_DYNAMIC 0
#endif
#define X86_OPL_MEM_BYTES (EMU8950_EXT_MEM_BYTES + OPL_CHUNK * 4u)
static int32_t *chunk;
static uint8_t  mem_decided;
#if !X86_OPL_DYNAMIC
static uint32_t opl_static_mem[X86_OPL_MEM_BYTES / 4u];
#endif

uint32_t x86_opl_mem_bytes(void) { return X86_OPL_MEM_BYTES; }

void *x86_opl_static_mem(void)
{
#if X86_OPL_DYNAMIC
    return 0;
#else
    return opl_static_mem;
#endif
}

void x86_opl_set_mem(void *p)
{
    emu8950_ext_mem = p;
    chunk = p ? (int32_t *)(void *)((uint8_t *)p + EMU8950_EXT_MEM_BYTES) : 0;
    opl = 0;                            /* OPL_new() again, in this block */
    inited = 0;
    mem_decided = 1;
}

int x86_opl_present(void)
{
    if (!mem_decided)
        return X86_OPL_DYNAMIC ? 0 : 1; /* static: present until told not */
    return emu8950_ext_mem != 0;
}
#else
static int32_t chunk[OPL_CHUNK];
int x86_opl_present(void) { return 1; }
#endif
static unsigned chunk_len, chunk_pos;

static uint32_t frac;          /* the 16.16 resampler accumulator */
static int16_t  s0, s1;        /* the two chip samples it interpolates between */
static uint8_t  rhythm_on;

/* EG_MAX is emu8950's "this operator contributes nothing"; a slot at or above
   it is silent whatever its phase. Kept here rather than reaching into the
   core's private header, and asserted against it by the shape of the test:
   if this constant were wrong the idle-channel early-out would either never
   engage (x86_opl_slots_run == 2 x chip_steps, which tests/test_sound.py
   FAILS on) or engage while a note was still sounding (the pitch arm fails). */
#define OPL_EG_MAX 0x1FF

/* `[sblaster] oplrate`, DOSBox's key: the rate the chip is synthesised at.
   This core has two: the chip's own 49,716 Hz, and half of it -- each
   rendered sample then advances two chip steps, so pitch and note timing are
   unchanged and only the waveform is sampled less (emu8950.c). A request
   under 37,287 Hz (three quarters of the chip rate) takes the half rate, so
   22050 and 24858 are half and 44100, 48000 and 49716 are full. Measured on
   the console (WOLF3D, oc 0): the FM render runs inside the guest's budget,
   and half rate gave +8-11% fps. The audible cost is the top octave: the
   waveform above ~12 kHz, and some aliasing of bright patches.
   Takes effect at the next refill; the chunk in flight is dropped. */
void x86_opl_set_rate(unsigned hz)
{
    uint8_t sh = (hz && hz < (X86_OPL_CLOCK_HZ * 3u) / 4u) ? 1u : 0u;
    if (sh != emu8950_decim_shift) {
        emu8950_decim_shift = sh;
        chunk_pos = chunk_len;      /* next sample refills at the new rate */
    }
}

unsigned x86_opl_rate(void)
{
    return X86_OPL_CLOCK_HZ >> emu8950_decim_shift;
}

void x86_opl_reset(void)
{
#if EMU8950_EXT_MEM && !X86_OPL_DYNAMIC
    if (!mem_decided)
        x86_opl_set_mem(opl_static_mem);
#endif
    if (!opl)
        opl = OPL_new(X86_OPL_CLOCK_HZ * 72u, X86_OPL_CLOCK_HZ);
    if (opl)
        OPL_reset(opl);
    chunk_len = chunk_pos = 0;
    frac = 0;
    s0 = s1 = 0;
    rhythm_on = 0;
    inited = 1;
}

/* How many operators could sound. Also the render's early-out: a title that
   never writes an FM register pays this and nothing else. */
static unsigned opl_live_slots(void)
{
    unsigned live = 0;
    if (!opl)
        return 0;
    for (unsigned i = 0; i < 18; i++)
        if (opl->slot[i].eg_out < OPL_EG_MAX)
            live++;
    return live;
}

int x86_opl_active(void)
{
    return opl && (opl->slot_key_status || opl_live_slots());
}

static uint16_t rst_mask;   /* channels keyed on by a muting reset, not yet aged */

/*
 * DIGITISED SOUND THROUGH THE CHIP (Pinball Dreams 2's ADLIB.SDR, console,
 * 2026-11-10). The driver keys a note on, waits 2.5 ms by the PIT, then
 * retunes it to frequency 0 with the key still down -- the operator is now
 * PARKED part-way up its sine -- and from then on writes that operator's
 * total level (register 40h) once per timer interrupt, 11,000 times a second:
 * a 6-bit DAC. Two things here made that a hum:
 *
 *   - the chip only advances inside x86_opl_render_mix(), so no chip time
 *     passed in those 2.5 ms and the operator was parked at phase 0, i.e. at
 *     nothing. A retune-to-zero of a held note therefore runs the chip
 *     forward first by the guest time it is short of since that key-on
 *     (x86_opl_now is x86_io.c's clock at this write), discarding the output;
 *   - ~185 level writes landed between two renders and only the last one
 *     counted. A level register written PCM_MIN times inside one render
 *     interval is a sample stream: from then on its values are QUEUED and
 *     the render spreads them evenly over the frame, refilling a few chip
 *     samples at a time so each lands where it was written.
 *
 * The queue is the unused tail of `chunk` -- a stream refills at most
 * PCM_CHUNK samples -- because RAM_UC has ~600 B left and this is not worth
 * a guest page. Everything that is not such a stream is untouched.
 */
#define PCM_MIN    8u                      /* writes per render that make a stream */
#define PCM_CHUNK  64u                     /* chip samples per refill while streaming */
#define PCM_CAP    ((OPL_CHUNK - PCM_CHUNK) * 4u)
uint32_t x86_opl_now;
static uint32_t kon_tick, kon_steps;       /* the last key-on: when, and chip time then */
static uint16_t pcm_n;                     /* queued level writes */
static uint8_t  pcm_reg;                   /* the streamed register, 0 = none */
static uint8_t  tl_last, tl_run;           /* same-register level writes this interval */
static uint8_t  kon_ch = 9u;

static uint8_t *pcm_q(void) { return (uint8_t *)(void *)(chunk + PCM_CHUNK); }

void x86_opl_write(uint8_t reg, uint8_t val)
{
    /* BSS zero is not this chip's reset state and x86_io.c has no init entry
       point to hang a call off. One predictable-not-taken branch on a path
       that runs a few hundred times a second; it must NOT become a pattern on
       the per-sample path. */
    if (!inited)
        x86_opl_reset();
    if (!opl)
        return;

    /* Register 4 is the TIMER control and register 2/3 the presets: x86_io.c
       owns those and emu8950 is built without them. Forwarding them anyway
       would be harmless today and a second opinion on the status byte the day
       EMU8950_NO_TIMER comes off, so they stop here. */
    if (reg == 0x02u || reg == 0x03u || reg == 0x04u)
        return;

    /* A level register: part of a sample stream, or about to become one. */
    if (reg >= 0x40u && reg <= 0x55u) {
        if (reg == pcm_reg) {
            if (pcm_n < PCM_CAP) {
                pcm_q()[pcm_n++] = val;
                return;
            }
        } else if (reg != tl_last) {
            tl_last = reg;
            tl_run = 1;
        } else if (++tl_run >= PCM_MIN && !pcm_reg) {
            pcm_reg = reg;
            pcm_n = 0;
            chunk_pos = chunk_len = 0;     /* the queue lives past PCM_CHUNK */
        }
    }

    /* A held note retuned to nothing: park it where real time would have. */
    if ((reg & 0xF0u) == 0xB0u && (reg & 0x0Fu) == kon_ch
        && (val & 0x3Fu) == 0x20u && (opl->reg[reg] & 0x20u)) {
        /* 1,193,182 Hz over 49,716 Hz is 24: both divide the 14.31818 MHz
           crystal, by 12 and by 288. */
        uint32_t want = (x86_opl_now - kon_tick) / 24u;
        uint32_t ran = x86_opl_chip_steps - kon_steps;
        if (want > ran && want - ran <= OPL_CHUNK) {
            /* want/ran are chip steps; the render takes samples */
            uint32_t ns = (want - ran) >> emu8950_decim_shift;
            if (ns)
                OPL_calc_buffer_linear(opl, chunk, ns);
            x86_opl_chip_steps += ns << emu8950_decim_shift;
            chunk_pos = chunk_len = 0;
            pcm_n = 0;                     /* `chunk` was the queue too */
        }
        kon_ch = 9u;
    }

    if (reg == 0xBDu) {
        uint8_t want = (uint8_t)((val >> 5) & 1u);
        if (want && !rhythm_on) {
            x86_opl_rhythm_muted++;   /* asked for drums; see the header */
            /* The three rhythm channels go quiet: key them off now. */
            for (unsigned c = 6u; c < 9u; c++)
                OPL_writeReg(opl, 0xB0u + c, opl->reg[0xB0u + c] & 0xDFu);
        }
        rhythm_on = want;
        /* emu8950 is built with EMU8950_NO_PERCUSSION_MODE, and that build
           ASSERTS that bit 5 is clear -- a fatal exception on the console
           (measured in the XTulator core with Ironman on AdLib). Only the
           AM/vibrato depth bits go through. */
        val &= 0xC0u;
    }
    /* While the guest is in rhythm mode, channels 6-8 are drums we do not
       play: never key them on as melodic voices. */
    if (rhythm_on && reg >= 0xB6u && reg <= 0xB8u)
        val &= 0xDFu;

    /*
     * A MUTING KEY-ON. Westwood's AdLib driver (Dune II) resets a channel by
     * keying it on with AR=DR=15 and SL=15 on both operators, then loads the
     * real instrument (new DR, slow RR) and keys it OFF a millisecond or two
     * later. On the chip the envelope falls at DR 15 -- 4 units of 511 per
     * chip sample -- until the new DR lands: after ~1.6 ms the note is gone,
     * after ~0.6 ms it is still ~-20 dB and rings on. Here every one of those
     * writes lands between two renders and NO chip time passes, so each
     * release started from FULL volume at the leftover fnum: ~15 Hz notes,
     * the owner's "petardeo" after the Westwood logo (checked against
     * Nuked-OPL3 replaying the same register log, 2026-11-04). So the reset
     * is remembered, and the first envelope or frequency write to that
     * channel applies the decay the elapsed guest time would have done.
     *
     * AND THAT DECAY IS NOW TOTAL (2026-11-04). It was aged by x86_pit_ticks(),
     * but that clock is the NOMINAL one (instructions at 5,500 kips), and in
     * the logo it ran ~3.5x short of the music's own tempo in a gwemu trace:
     * the ~2 ms the driver takes came out as well under the 2.6 ms a DR-15
     * fall needs to reach the floor. What was left rang on at full pitch --
     * channel 5 alone kept 300-530 Hz tones at ~15x the level a full mute
     * leaves, 3-5 s into the logo, the window the owner still heard
     * "petardeo" in with the 140 Hz high-pass in. The driver's intent is
     * plainly "silence this channel", so silence it.
     */
    if (rst_mask) {
        static const uint8_t op_ch[22] = { 0, 1, 2, 0, 1, 2, 9, 9,
                                           3, 4, 5, 3, 4, 5, 9, 9,
                                           6, 7, 8, 6, 7, 8 };
        unsigned ch = (reg >= 0xA0u && reg < 0xB9u) ? (reg & 0x0Fu)
                    : (reg >= 0x60u && reg < 0x96u && ((reg - 0x60u) & 0x1Fu) < 22u)
                      ? op_ch[(reg - 0x60u) & 0x1Fu] : 9u;
        if (ch < 9u && ((rst_mask >> ch) & 1u)) {
            opl->slot[ch * 2u].eg_out = opl->slot[ch * 2u + 1u].eg_out = OPL_EG_MAX;
            rst_mask &= (uint16_t)~(1u << ch);
        }
    }

    unsigned on = 9u;
    if ((reg & 0xF0u) == 0xB0u && (reg & 0x0Fu) < 9u) {
        unsigned ci = reg & 0x0Fu;
        uint8_t was = (uint8_t)((opl->reg[0xB0u + ci] >> 5) & 1u);
        if (((val >> 5) & 1u) && !was) {
            x86_opl_keyons++;
            on = ci;
            kon_ch = (uint8_t)ci;
            kon_tick = x86_opl_now;
            kon_steps = x86_opl_chip_steps;
        }
    }

    OPL_writeReg(opl, reg, val);

    if (on < 9u) {
        unsigned m = (on % 3u) + (on / 3u) * 8u;     /* modulator offset */
        if ((opl->reg[0x60u + m] & 0x0Fu) == 0x0Fu &&
            (opl->reg[0x63u + m] & 0x0Fu) == 0x0Fu &&
            (opl->reg[0x80u + m] & 0xF0u) == 0xF0u &&
            (opl->reg[0x83u + m] & 0xF0u) == 0xF0u) {
            rst_mask |= (uint16_t)(1u << on);
        }
    }
}

/* One chunk of chip samples at 49,716 Hz. */
static void opl_refill(unsigned n)
{
    unsigned live = opl_live_slots();
    OPL_calc_buffer_linear(opl, chunk, n);
    chunk_len = n;
    chunk_pos = 0;
    /* CHIP steps, not rendered samples: kon_steps (x86_opl_write) compares
       this against guest time at 49,716 Hz whatever the render rate is. */
    x86_opl_chip_steps += n << emu8950_decim_shift;
    /* What the render ACTUALLY did, so the idle-channel skip is observed and
       not assumed. emu8950's EMU8950_LINEAR_SKIP drops finished slots inside
       the loop; this is that count, sampled at the chunk boundary. */
    x86_opl_slots_run += live * (n << emu8950_decim_shift);
}

unsigned x86_opl_render_mix(int16_t *dst, unsigned frames,
                            unsigned sample_rate, int16_t amplitude)
{
    uint32_t step;
    unsigned qn = pcm_n, qi = 0, refill = OPL_CHUNK >> emu8950_decim_shift;

    rst_mask = 0;      /* chip time is passing now: nothing left to age */
    /* The sample stream, if there is one (see x86_opl_write). Fewer than
       PCM_MIN level writes since the last render means it has stopped: the
       last value stands and the register is an ordinary one again. */
    pcm_n = 0;
    tl_run = 0;
    if (pcm_reg && (qn < PCM_MIN || !opl || !frames || !sample_rate || !dst)) {
        if (qn && opl)
            OPL_writeReg(opl, pcm_reg, pcm_q()[qn - 1u]);
        pcm_reg = 0;
        qn = 0;
    }
    if (!opl || !frames || !sample_rate || !dst || !x86_opl_active()) {
        if (qn)
            OPL_writeReg(opl, pcm_reg, pcm_q()[qn - 1u]);
        s0 = s1 = 0;               /* never hold a DC offset over silence */
        return 0;
    }

    /* The chip is synthesised at X86_OPL_CLOCK_HZ >> emu8950_decim_shift
       (x86_opl_set_rate, `[sblaster] oplrate`): fewer, longer chip samples, the
       same pitch and timing, and the linear interpolation below carries
       them to the output rate. */
    step = (uint32_t)(((uint64_t)(X86_OPL_CLOCK_HZ >> emu8950_decim_shift) << 16) / sample_rate);
    if (!step)
        step = 1;
    if (qn) {
        /* chip samples between two level writes, so each refill ends at one */
        /* frames x step fits 32 bits: a frame is under 2,000 samples */
        refill = ((frames * step) >> 16) / qn;
        if (refill < 1u) refill = 1u;
        if (refill > PCM_CHUNK) refill = PCM_CHUNK;
    }

    for (unsigned i = 0; i < frames; i++) {
        int32_t v, o;

        if (qn) {
            unsigned due = (unsigned)(((uint32_t)i * qn) / frames);
            while (qi <= due)
                OPL_writeReg(opl, pcm_reg, pcm_q()[qi++]);
        }
        frac += step;
        while (frac >= 0x10000u) {
            int32_t s;
            frac -= 0x10000u;
            s0 = s1;
            if (chunk_pos >= chunk_len)
                opl_refill(refill);
            s = chunk[chunk_pos++];        /* the sum of up to nine voices */
            if (s > 32767) s = 32767;
            else if (s < -32768) s = -32768;
            s1 = (int16_t)s;
        }
        /* LINEAR, not a hold. See the header: the hold buzzes. */
        v = s0 + (((int32_t)(s1 - s0) * (int32_t)(frac >> 8)) >> 8);
        /* THE MUSIC GAIN IS THE OWNER'S: opl_gnw.c keeps emu8950's raw level
           (no upstream << 3) and "the mixer applies the music*2 balance gain",
           which is this >> 14 against a 1.15 amplitude. Chosen there against a
           real mix of Doom's music and its digitised sound, which is exactly
           the balance question here, so it is taken rather than re-derived.
           One operator at total level 0 lands at ~1,012 of full scale, a
           nine-voice chord at ~8,000, against the ~4,080 a full-scale Sound
           Blaster sample reaches at maximum volume. */
        v = (v * amplitude) >> 14;

        o = (int32_t)dst[i] + v;
        if (o > 32767) { o = 32767; x86_opl_clips++; }
        else if (o < -32768) { o = -32768; x86_opl_clips++; }
        dst[i] = (int16_t)o;
    }
    while (qi < qn)                /* more writes than output samples */
        OPL_writeReg(opl, pcm_reg, pcm_q()[qi++]);
    x86_opl_samples += frames;
    return frames;
}

/* ---------------------------------------------------------------------------
 * MEASUREMENT ARM (X86_OPL_BENCH=1, never shipped at 1). See x86_opl.h.
 */
void x86_opl_bench_all_on(void)
{
    x86_opl_reset();
    x86_opl_write(0x01, 0x20);
    for (unsigned c = 0; c < 9; c++) {
        uint8_t o0 = (uint8_t)((c / 3) * 8 + (c % 3));
        for (unsigned k = 0; k < 2; k++) {
            uint8_t o = (uint8_t)(o0 + 3 * k);
            x86_opl_write((uint8_t)(0x20 + o), 0x21);  /* sustaining, mult 1 */
            x86_opl_write((uint8_t)(0x40 + o), 0x00);  /* total level 0      */
            x86_opl_write((uint8_t)(0x60 + o), 0xF0);  /* AR 15, DR 0        */
            x86_opl_write((uint8_t)(0x80 + o), 0x00);  /* SL 0, RR 0         */
        }
        x86_opl_write((uint8_t)(0xC0 + c), 0x01);      /* AM: both heard     */
        x86_opl_write((uint8_t)(0xA0 + c), (uint8_t)(0x40 + c * 8));
        x86_opl_write((uint8_t)(0xB0 + c), (uint8_t)(0x20 | (4 << 2) | 1));
    }
}

/* THE BASS THE SPEAKER CANNOT PLAY IS TAKEN OUT. Two one-pole high-passes
   at ~140 Hz (12 dB/octave) over the finished mix. The Game & Watch speaker
   reproduces nothing that low, but it is still DRIVEN by it: Dune II's
   Westwood logo is 57% energy under 100 Hz (plus a DC offset) and Monkey
   Island's theme half under 200 Hz, and that excursion is the "petardeo" /
   "distorsion" heard on the device (2026-11-04) -- while the FM render itself
   matches Nuked-OPL3 band for band on the same register logs. State in Q8 so
   the slow pole does not stall. It lives HERE and not in src/main.c for the
   memory map alone: this file's .bss is in RAM_UC, which had the 16 B, and
   the code is in ITCM -- RAM_EMU had no 200 B to spare. */
static int32_t hpf_xp1, hpf_y1, hpf_xp2, hpf_y2;
__attribute__((section(".itcram_hot.hpf"), noinline, long_call))
void x86_audio_highpass(int16_t *buf, unsigned n)
{
    const int32_t A = 32172;                 /* exp(-2*pi*140/48000), Q15 */
    int32_t xp1 = hpf_xp1, y1 = hpf_y1, xp2 = hpf_xp2, y2 = hpf_y2;
    for (unsigned i = 0; i < n; i++) {
        int32_t x = (int32_t)buf[i] << 8;
        y1 = (int32_t)(((int64_t)A * (y1 + x - xp1)) >> 15);
        xp1 = x;
        y2 = (int32_t)(((int64_t)A * (y2 + y1 - xp2)) >> 15);
        xp2 = y1;
        int32_t o = y2 >> 8;
        buf[i] = (int16_t)(o > 32767 ? 32767 : o < -32768 ? -32768 : o);
    }
    hpf_xp1 = xp1; hpf_y1 = y1; hpf_xp2 = xp2; hpf_y2 = y2;
}
