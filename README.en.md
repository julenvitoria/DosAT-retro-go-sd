# DosAT: MS-DOS for the Game & Watch (retro-go SD)

*[Leer en español](README.md)*

An MS-DOS PC as an external core for the **retro-go SD** firmware. It is
[Fabrice Bellard's TinyEMU](docs/tinyemu-original.md) with the x86 CPU
rewritten in assembly for the console.

- **No need to install MS-DOS**: the core carries its own DOS and every game
  boots straight away.
- A 386 CPU, CGA, EGA and VGA graphics, AdLib and Sound Blaster sound, mouse,
  and EMS and XMS memory.
- For PC XT games (8088, CGA) there is the other core,
  [DosXT-retro-go-sd](https://github.com/julenvitoria/DosXT-retro-go-sd)
  (`dosxt.bin`). Both live on the same card.

## On the SD card

```
/cores/dosat.bin           <- the core
/roms/dos/MONKEY.dsk       <- each game, as a .dsk disk image
/cores/dosat/MONKEY.cfg    <- its configuration (optional), same name
```

## Making a game's `.dsk`

A `.dsk` is a disk image with the game's files inside. With the game's folder
on your computer:

```
python3 scripts/make_dsk.py --src MONKEY --out MONKEY.dsk --autoexec MONKEY
```

`--autoexec` is the program that starts the game. A floppy image (`.img`) also
works: just rename it to `.dsk`.

Large games, CD games, multi-floppy games or games with an installer:
[**docs/discos.en.md**](docs/discos.en.md).

## Configuring a game (`.cfg`)

The `.cfg` is optional and uses the DOSBox format. A complete example with the
most used options:

```ini
; /cores/dosat/MONKEY.cfg

[cpu]
cycles = max          ; speed: max, or 330 (PC XT) … 4500 (386DX at 25 MHz)

[dos]
xms = true            ; extended memory, for games that ask for it
ems = false           ; expanded memory

[autoexec]
monkey a              ; the program, with its options (a = AdLib music)

[gamepad]
mouse  = on           ; the D-pad moves the mouse pointer
b      = click        ; B: left mouse button
a      = click2       ; A: right mouse button
start  = enter        ; START: Enter key
select = esc          ; SELECT: Esc key
time   = f5           ; TIME: F5 (in Monkey Island, save game)
```

- If the game is in a folder of the `.dsk`, put `cd \FOLDER` first in
  `[autoexec]`. Without `[autoexec]` the `.dsk`'s `AUTOEXEC.BAT` is used.
- Without `[gamepad]` the default buttons apply: D-pad = arrows, A = Space,
  B = Enter, TIME = Esc.
- A button can send up to 3 keys at once: `a = ctrl+x`.
- **Every option**, CPU profiles, sound and the games' own options:
  [**docs/cfg.en.md**](docs/cfg.en.md).
- **Key names**, and how to find out which keys a game uses:
  [docs/input/06-gamepad-keys.en.md](docs/input/06-gamepad-keys.en.md).

## Controls

| Button | What it does |
|---|---|
| **GAME** | Opens the on-screen keyboard; again, the mouse; again, closes it |
| **PAUSE/SET** | retro-go menu: CPU speed, quit… |
| **PAUSE/SET + D-pad** | Volume (left/right) and brightness (up/down) |

On-screen keyboard, mouse and disk swapping:
[docs/controles.en.md](docs/controles.en.md).

## More

- [docs/desarrollo.en.md](docs/desarrollo.en.md): building the core, the
  repository's branches and the technical documentation.
- MIT licence, like the original TinyEMU ([MIT-LICENSE.txt](MIT-LICENSE.txt)).
