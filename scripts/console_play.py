#!/usr/bin/env python3
"""Play a title on the REAL console from the PC, one step at a time, and look.

Tier 3 only -- this drives the device over the ST-Link and has no gwemu arm.
It exists because two things cannot be judged from counters: whether the
PICTURE is right, and what a title does once somebody actually plays it. A
draft of the lazy 0Dh read shadow passed tests/test_video.py and drew Keen 4's
masked tiles black; Dark Forces "loading for three minutes" was sitting at
"press any key"; Pinball Dreams 2 "at 55 fps" was the probe's own sampling.

    console_play.py save-config            # FIRST: keep the owner's /CONFIG
    console_play.py launch KEEN4.dsk       # preselect, reset, wait for launch
    console_play.py step ELF out.png "1 press start; 4 hold left 2; 5 press a"
    console_play.py shot ELF out.png       # capture the panel, nothing else
    console_play.py watch ELF [-n 12] [-i 10]   # speed / pager / SD, sparsely
    console_play.py restore-config         # LAST: put the owner's /CONFIG back

`step` attaches, replays the events (times in seconds from the attach; the
verbs are the timeline's own: press / hold KEY SECONDS), then captures what the
LTDC is scanning out with scripts/screenshot.py. Timeline `screenshot` events
do nothing on hardware, which is why the capture is a separate step. The
palette read is 256 entries: the default makes a 256-colour title look dark
grey.

WHY save-config / restore-config. `deploy_device.py --preselect` rewrites
/CONFIG and keeps ONE backup, which a second --preselect overwrites with a
preselecting copy. The owner's /CONFIG also changes whenever he plays. So the
session's first act is to save what is on the card NOW
(build/CONFIG.device-original, the previous one kept with a date), `launch`
re-points the tool's backup at it every time, and restore-config writes it
back and verifies it byte for byte.

`watch` samples few and far apart on purpose: every sample halts the CPU.
"""
import argparse
import datetime
import math
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIG = os.path.join(ROOT, "build", "CONFIG.device-original")
BACKUP = os.path.join(ROOT, "build", "CONFIG.device-backup")
RUN = os.path.join(ROOT, "build", "run", "console_play")
FW = []     # ["--firmware-dir", DIR] when the checkout has no references/ build

WATCH = ("guest_uptime_ms,frames_rendered,guest_insns_total,io_block_us,"
         "x86_pg_evictions,x86_pg_fill_file,x86_pg_drop_same,x86_pg_frames_total,"
         "guest_ticks,guest_ticks_dropped,guest_tick_divisor,max_slice,"
         "x86_vga_planar_stores,x86_audio_blocks_missed,x86_video_mode_byte,"
         "guest_halted,x86_seam_overflow,x86_pg_io_errors,pgf_errors")


def py(*args, **kw):
    return subprocess.run([sys.executable, *args], cwd=ROOT, text=True, **kw)


def sd():
    from gnwmanager.gnw import GnW
    from gnwmanager.ocdbackend.openocd_backend import OpenOCDBackend
    b = OpenOCDBackend()
    b.open()
    g = GnW(b)
    g.start_gnwmanager()
    return b, g


def reset():
    from gnwmanager.ocdbackend.openocd_backend import OpenOCDBackend
    b = OpenOCDBackend()
    b.open()
    b.reset_and_halt()
    b.resume()
    b.close()


def cmd_save_config(_):
    b, g = sd()
    cur = g.sd_read_file("/CONFIG")
    b.reset_and_halt()
    b.resume()
    if os.path.exists(ORIG) and open(ORIG, "rb").read() != cur:
        stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        shutil.copy(ORIG, f"{ORIG}.bak-{stamp}")
    open(ORIG, "wb").write(cur)
    print(f"saved /CONFIG ({len(cur)} B) to {os.path.relpath(ORIG, ROOT)}")


def cmd_restore_config(_):
    if not os.path.exists(ORIG):
        sys.exit("no saved /CONFIG: run save-config at the start of the session")
    shutil.copy(ORIG, BACKUP)
    r = py("scripts/deploy_device.py", "--no-firmware", "--no-core",
           "--restore-config", capture_output=True)
    b, g = sd()
    ok = g.sd_read_file("/CONFIG") == open(ORIG, "rb").read()
    b.reset_and_halt()
    b.resume()
    print("restored and verified" if ok else "RESTORE FAILED:\n" + r.stdout[-400:])
    sys.exit(0 if ok else 1)


def cmd_launch(a):
    if not os.path.exists(ORIG):
        sys.exit("no saved /CONFIG: run save-config first")
    r = py("scripts/deploy_device.py", "--no-firmware", "--no-core",
           "--preselect", a.rom, "--oc-level", str(a.oc),
           "--volume", str(a.volume), capture_output=True)
    print("\n".join(l for l in r.stdout.splitlines()
                    if "preselect" in l or "rror" in l))
    shutil.copy(ORIG, BACKUP)       # --restore-config must give back the owner's
    reset()
    import time
    time.sleep(a.wait)


def shot(elf, out):
    r = py("scripts/screenshot.py", "--target", "hardware", "--no-flash",
           "--core-elf", elf, "--wait", "0", "--clut-entries", "256",
           "--out", out, *FW, capture_output=True)
    if r.returncode:
        print("screenshot FAILED", (r.stdout + r.stderr)[-300:])


def cmd_shot(a):
    shot(a.elf, a.out)


def cmd_step(a):
    lines = [e.strip() for e in a.events.split(";") if e.strip()]
    end = 0.0
    for l in lines:
        p = l.split()
        end = max(end, float(p[0]) + (float(p[3]) if p[1] == "hold" else 0.3))
    os.makedirs(RUN, exist_ok=True)
    tl = os.path.join(RUN, f"step-{os.getpid()}.tl")
    try:
        with open(tl, "w") as fh:
            fh.write("\n".join(lines) + f"\n{end + 0.5:.1f} quit\n")
        r = py("scripts/probe.py", "--retrogo", "--target", "hardware",
               "--no-flash", "--elf", a.elf, *FW, "--timeline", tl,
               "--interval", "1", "--samples", str(int(math.ceil(end)) + 2),
               "--watch", "guest_halted,x86_seam_overflow,x86_pg_io_errors,"
                          "pgf_errors,x86_pg_evictions",
               capture_output=True)
        last = [l for l in r.stdout.splitlines() if "t+" in l][-1:]
        print(re.sub(r"PC=\S+ \S+ \S+\s+", "", last[0]).strip()
              if last else "NO SAMPLES " + (r.stdout + r.stderr)[-300:])
    finally:
        if os.path.exists(tl):
            os.remove(tl)
    shot(a.elf, a.out)


def cmd_watch(a):
    nm = subprocess.run(["arm-none-eabi-nm", a.elf], capture_output=True,
                        text=True).stdout.split()
    watch = ",".join(w for w in WATCH.split(",") if w in nm)
    r = py("scripts/probe.py", "--retrogo", "--target", "hardware", "--no-flash",
           "--elf", a.elf, *FW, "--interval", str(a.interval),
           "--samples", str(a.samples), "--watch", watch, capture_output=True)
    prev = None
    for l in r.stdout.splitlines():
        if not re.match(r"\s*t\+", l):
            continue
        d = {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", l)}
        if prev:
            s = (d["guest_uptime_ms"] - prev["guest_uptime_ms"]) / 1000 or 1

            def f(k):
                return (d.get(k, 0) - prev.get(k, 0)) / s
            print(f"t {d['guest_uptime_ms'] // 1000:4d}s  fps {f('frames_rendered'):4.1f}"
                  f"  insns/s {f('guest_insns_total') / 1e6:5.2f}M"
                  f"  SD {f('io_block_us') / 10000:4.1f}%"
                  f"  evict/s {f('x86_pg_evictions'):5.1f}"
                  f" (unwritten {f('x86_pg_drop_same'):5.1f})"
                  f"  ticks/s {f('guest_ticks'):5.0f} dropped {f('guest_ticks_dropped'):5.0f}"
                  f"  stores/s {f('x86_vga_planar_stores'):7.0f}"
                  f"  slice {d.get('max_slice', 0):6d}"
                  f"  mode {d.get('x86_video_mode_byte', 0):#x}"
                  f"  halted {d.get('guest_halted', 0)}")
        prev = d
    if prev is None:
        print("NO SAMPLES", (r.stdout + r.stderr)[-300:])


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--firmware-dir", help="retro-go build directory, for a "
                    "checkout (a git worktree) that has no references/ of its own")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("save-config").set_defaults(fn=cmd_save_config)
    sub.add_parser("restore-config").set_defaults(fn=cmd_restore_config)
    p = sub.add_parser("launch")
    p.add_argument("rom")
    p.add_argument("--wait", type=float, default=16.0)
    p.add_argument("--oc", type=int, default=0)
    p.add_argument("--volume", type=int, default=0,
                   help="launcher volume while testing, 0-9 (default 0: "
                        "silent -- the console sits on someone's desk and the "
                        "core mixes the same at any volume)")
    p.set_defaults(fn=cmd_launch)
    p = sub.add_parser("shot")
    p.add_argument("elf")
    p.add_argument("out")
    p.set_defaults(fn=cmd_shot)
    p = sub.add_parser("step")
    p.add_argument("elf")
    p.add_argument("out")
    p.add_argument("events")
    p.set_defaults(fn=cmd_step)
    p = sub.add_parser("watch")
    p.add_argument("elf")
    p.add_argument("-n", "--samples", type=int, default=7)
    p.add_argument("-i", "--interval", type=float, default=10.0)
    p.set_defaults(fn=cmd_watch)
    a = ap.parse_args()
    if a.firmware_dir:
        FW.extend(["--firmware-dir", os.path.abspath(a.firmware_dir)])
    a.fn(a)


if __name__ == "__main__":
    main()
