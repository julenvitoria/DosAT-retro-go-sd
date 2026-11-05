# Development

*[Leer en español](desarrollo.md) · Back to the [README](../README.en.md)*

## Building

```
make -f Makefile.core pack
```

It produces `dosat.bin`, which goes in `/cores/` on the card. What it
needs, the scripts and the tests: [TOOLING.md](../TOOLING.md).

## Branches

- **`main`**: the release, the one handed out as `dosat.bin`. Since 30-10-2026
  the AdLib chip, the mouse, the flash memory and the swap file live in the
  memory the display leaves free in palette mode (LUT8), and the game has 158
  pages out of 160. It needs a retro-go firmware that loads that part of the
  core.
- **`normal`**: the previous version, without that extra memory (152 pages),
  for firmwares that do not load it. No longer developed.
- The other branches are tests: they reach `main` once tested on the console.

## Technical documentation

- [STATUS.md](../STATUS.md): current state. [PLAN.md](../PLAN.md): roadmap.
- [TOOLING.md](../TOOLING.md): the scripts and the three test tiers (gwemu,
  gwemu + retro-go, console).
- [docs/](.): the design of each part (memory, CPU, video, timer…).
- [CLAUDE.md](../CLAUDE.md): the project's rules.
- [tinyemu-original.md](tinyemu-original.md): Fabrice Bellard's TinyEMU it
  started from.
