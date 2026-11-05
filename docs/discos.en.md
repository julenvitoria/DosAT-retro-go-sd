# Making the games' `.dsk` images

*[Leer en español](discos.md) · Back to the [README](../README.en.md)*

Each game goes on the card as **one `.dsk` disk image** in `/roms/dos/`: a file
that is a PC disk inside, holding the game's files. The core boots from that
image, and whatever the game saves (saved games, settings) is written into it.

## What you need

- The game's files in a folder on your computer (already installed: see
  below).
- **Python 3** and **mtools**, and this repository downloaded so you have
  `scripts/make_dsk.py`.
  - Linux: `sudo apt install mtools` (Debian/Ubuntu).
  - macOS: `brew install mtools`.
  - Windows: inside WSL (Ubuntu), as on Linux.

## Step by step

1. Put the game's files in a folder, for example `MONKEY/`. Whatever is inside
   the folder becomes the root of the disk.
2. Find the name of the program that starts the game (`MONKEY.EXE`,
   `DUNE2.EXE`, `GAME.BAT`…).
3. Make the image:

   ```
   python3 scripts/make_dsk.py --src MONKEY --out MONKEY.dsk --autoexec MONKEY
   ```

   `--autoexec` writes a one-line `AUTOEXEC.BAT` inside that starts that
   program. If you would rather say it in the `.cfg`, leave it out.
4. Copy `MONKEY.dsk` to `/roms/dos/` and, if it has one, its `MONKEY.cfg` to
   `/cores/dos386/`.

## Floppy or hard disk

The script picks by the game's size:

- **Up to 2.88 MB** it makes a floppy, and the game sees drive `A:`.
- **Larger** it makes a hard disk, and the game sees `C:`.

Some games refuse to start from a floppy (Risky Woods, for example). For those,
force a hard disk with `--size-mb`:

```
python3 scripts/make_dsk.py --src RISKY --out RISKY.dsk --autoexec RISKY --size-mb 3
```

It is also a way to leave free space for saved games. A floppy `.img` you
already have works as it is: rename it to `.dsk`.

**Without mtools** (for example on Windows without WSL): the other core's
`tools/fat12.py` ([dos-retro-go-sd](https://github.com/julenvitoria/dos-retro-go-sd))
only needs Python 3 and does the same:

```
python3 fat12.py todsk MONKEY MONKEY.dsk --run "MONKEY"
```

## Which program starts

In this order:

1. The one the `.cfg` names in `[autoexec]` (see [cfg.en.md](cfg.en.md)).
2. The one the `.dsk`'s `AUTOEXEC.BAT` names. `cd` and one line with the
   program are understood; `CAT` looks for `CAT.COM` and then `CAT.EXE`.

## Special cases

**Games with an installer.** Many games came compressed and had to be
installed. The simplest way is to install them on your computer with
[DOSBox](https://www.dosbox.com/) and make the `.dsk` from the installed
folder.

**Multi-floppy games.** If they install, as above: one `.dsk` with
everything. If they are played by swapping floppies, make one `.dsk` per floppy
with names that sort in order. In the game, the green **`A:`** key of the
on-screen keyboard opens the disk list to swap (see
[controles.en.md](controles.en.md)).

**CD games.** Copy everything the game uses from the CD into its folder. There
is no emulated CD drive and no CD music: games that can read everything from
the hard disk work (for example the "talkie" versions of Monkey Island 2 or
Indiana Jones and the Fate of Atlantis).

**LucasArts floppy games.** `scripts/lucasarts_unpack.py` extracts the files
from the install floppies without installing (see [TOOLING.md](../TOOLING.md)).
