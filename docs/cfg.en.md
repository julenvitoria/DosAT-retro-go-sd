# Each game's `.cfg`

*[Leer en español](cfg.md)*

*Everything that can go in a game's `.cfg` for the TinyEMU core
(`dosat.bin`). Back to the [README](../README.en.md).*

## Where it goes

Next to the image, with the same name: `/roms/dos/MONKEY.dsk` →
`/cores/dosat/MONKEY.cfg`, in the core's folder, so it does not show up in the
game menu and the other core (DosXT, `dosxt.bin`, which reads `/cores/dosxt/`) can keep
its own `.cfg` for the same game. A `.cfg` next to the image is no longer read. All of it is optional: with no `.cfg` the game starts
with whatever its `.dsk`'s `AUTOEXEC.BAT` says, and with the default buttons.

## Format

It is DOSBox's, so a `dosbox.conf` can be copied almost as is: sections in
square brackets with `key = value` inside. Upper or lower case, it does not
matter.

- `;` or `#` at the start of a line, or after a space, start a comment. In
  `[autoexec]` only at the start of the line, because a DOS command may contain
  those characters.
- A line that is not understood counts as an error and that option stays as it
  was; the rest of the file still applies.

## Options

### `[cpu]`

| Key | Values | What it does |
|---|---|---|
| `cycles` | `max`, or a number | CPU speed in thousands of instructions per second, as in DOSBox. Rounded to the nearest profile in the table below. |
| `cpu_profile` | `0` … `5` | The profile by its number, directly. |

Profiles (the same as the retro-go menu, PAUSE/SET → CPU Speed):

| No. | Profile | `cycles` |
|---|---|---|
| 0 | Max (whatever the console gives) | `max` |
| 1 | 8088 at 4.77 MHz (PC XT) | 330 |
| 2 | 286 at 8 MHz | 800 |
| 3 | 286 at 12 MHz | 1200 |
| 4 | 386SX at 16 MHz | 2500 |
| 5 | 386DX at 25 MHz | 4500 |

**Max does not hold anything back: the CPU runs as fast as the console can**,
about 5,500 k at best (like a 386DX at about 30 MHz). It is the "boosted"
profile for heavy games; a fixed 5500 one would add nothing, because the
console cannot go faster than Max, and gives less in heavy scenes. That is why
there are no profiles above the 386DX 25: a `cycles` of 5500 or more is Max,
and an old `cpu_profile` of 6 to 8 also becomes Max.

Some games of the period break or run too fast on a fast CPU; use a slow
profile for those.

### `[dos]`

| Key | Values | What it does |
|---|---|---|
| `xms` | `true` / `false` | XMS memory (4 MB with `true`). |
| `ems` | `true` / `false` | EMS memory (1 MB with `true`). |
| `xms_kb` | KB | XMS of a specific size. |
| `ems_kb` | KB, multiple of 16 | EMS of a specific size. |

### `[sblaster]`

The core has two fixed sound cards, and these two keys only **remove** them:

| Key | Values | What it does |
|---|---|---|
| `oplmode` | `none` removes it; anything else keeps it | The FM chip (OPL): AdLib music and the Sound Blaster's FM half. |
| `sbtype` | `none` removes it; anything else keeps it | The Sound Blaster (digitised speech and effects). Without it the `BLASTER` variable is not set either. |

The names are DOSBox's so a `.cfg` copied from there works, but no model is
chosen here: `sb16`, `sbpro2`, `opl3`… mean the same as leaving the key out.
Without these keys both cards are there.

The Sound Blaster is a **Sound Blaster 16** (DSP 4.05) at the standard
settings, the ones almost every game offers by default:

| Setting | Value |
|---|---|
| Port | 220 |
| IRQ | 5 |
| DMA (8-bit) | 1 |
| DMA (16-bit) | 5 |

That is `BLASTER=A220 I5 D1 H5 T6`. If the game's installer or `SETUP` asks,
pick Sound Blaster (or Sound Blaster 16) with those values, and AdLib for music
if it is offered separately.

A removed card does not answer, like an empty slot, and the game moves on to
the next one it knows (usually the PC speaker). Useful for games that hang or
sound wrong with one of them.

### `[video]`

| Key | Values | What it does |
|---|---|---|
| `vga_pages` | `0` to `3` | Extra video memory for games that use the "unchained" VGA mode (Mode X/Y). Without the key it is `1`, which suits almost all of them. `2` gives three whole screens (Wolfenstein 3D), at the cost of some of the game's memory. `3` is for games that **scroll the display through all of video memory**, like **Epic Pinball**: without it the table comes out doubled, with black lines, or black at the bottom. |

When it is set, the core does not take that memory back even if the game is
short of RAM: you chose the picture for that game. If a game loads more
slowly with it, remove it.

### `[autoexec]`

Which programs start. It is not a full `.bat` file: the core carries its own
DOS, so it takes two things from this section:

- `cd <folder>` (or `cd\folder`): the working directory.
- Any other line is a program to run, with its parameters. The last one is the
  game; the earlier ones run first, in order, each starting when the previous
  one exits or stays resident (a missing one is
  skipped). Use it to load a resident driver before the
  game, for example the AdLib `SOUND.COM` that Rick Dangerous II needs:

  ```ini
  [autoexec]
  sound
  rick2
  ```
- `mount`, `imgmount`, `echo`, `cls`, `pause`, `exit`, `set`, `keyb`, `c:`…
  are ignored without an error, so a DOSBox `[autoexec]` can be pasted in.

Without `[autoexec]`, the `.dsk`'s `AUTOEXEC.BAT` decides.

The same can be written with separate keys, in any section: `entry` (the
program), `args` (its parameters), `cwd` (the folder), `pre` (with `entry`:
programs to run first, separated by `;`, e.g. `pre = sound`) and `use_cmdtail`
(`true`/`false`: whether the parameters in the game's `AUTOEXEC.BAT` are passed
when the `.cfg` gives none).

### `[gamepad]`

| Key | Values | What it does |
|---|---|---|
| `up` `down` `left` `right` (or `arriba` `abajo` `izquierda` `derecha`), `a`, `b`, `time`, `start` (or `x`), `select` (or `y`) | key name, or up to 3 joined with `+` | Which PC key each button sends. `game` cannot be changed: it opens the on-screen keyboard. List of names: [`input/06-gamepad-keys.en.md`](input/06-gamepad-keys.en.md). |
| `repeat` | `on` / `off` | Repeat while a button is held (default `on`). |
| `mouse` | `on` / `off` | Mouse: the d-pad moves the pointer and A/B are the left and right buttons (unless you change them). |

## The games' own options (sound, video…)

This is not part of the emulator but of each game: many are configured with a
letter or a word after the executable's name, or with a setup program. Here they
go in `[autoexec]` (for example `monkey a`).

- **LucasArts games of the period (Monkey Island 1, Loom, Indiana Jones…)**:
  a letter after the executable picks the sound card: `a` = AdLib,
  `r` = Roland MT-32 (not emulated: no music), `i` = PC speaker. With no letter
  the game looks for the card itself. With a Sound Blaster use `a`: these games'
  music is FM, and the Sound Blaster carries the same chip as the AdLib.
- **Games with a setup program** (`SETUP.EXE`, `INSTALL.EXE`, `SOUND.EXE`…):
  put that program in `[autoexec]`, start it once, pick the card (AdLib, or
  Sound Blaster at 220h / IRQ 5 / DMA 1), then put the game's program back: the
  choice stays saved in its folder.
- **To find out what options a game has**: the manual or the `README`/`LEEME`
  in its folder.

What this core emulates: AdLib, Sound Blaster (220h, IRQ 5, DMA 1, high DMA 5:
`BLASTER=A220 I5 D1 H5 T6`, which the game finds already set) and the PC
speaker. It does not emulate Roland MT-32, Gravis Ultrasound or Tandy/PCjr.
