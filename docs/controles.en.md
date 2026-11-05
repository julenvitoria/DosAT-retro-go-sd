# Controls

*[Leer en español](controles.md) · Back to the [README](../README.en.md)*

## Buttons

Without `[gamepad]` in the `.cfg`:

| Button | PC key |
|---|---|
| D-pad | Arrows |
| A | Space |
| B | Enter |
| TIME | Esc |
| START | Y |
| SELECT | N |

START and SELECT only exist on the consoles that have them. Any button except
GAME can be changed in the `.cfg` (see [cfg.en.md](cfg.en.md)).

Always:

| Button | What it does |
|---|---|
| **GAME** | On-screen keyboard → mouse → close (each press moves to the next) |
| **PAUSE/SET** | retro-go menu: CPU speed (CPU Speed), quit… |
| **PAUSE/SET + left/right** | Volume |
| **PAUSE/SET + up/down** | Brightness |

## On-screen keyboard

It takes two rows at the top and two at the bottom; the game does not move.

- D-pad to move (held, it keeps going), **A** presses the key, **B** is
  Backspace, **TIME** is Shift.
- **Shift, Ctrl and Alt** stay pressed for the next key (green). Shift pressed
  twice locks (red, like Caps Lock); a third press releases it.
- The **`:`** key sends a colon in one go, without Shift.
- The green **`X`** closes the keyboard.

## Swapping disks

The green **`A:`** key of the on-screen keyboard opens the list of `.dsk` files
in the folder: up/down picks, **A** loads it in place of the current disk,
**B** goes back. It is for multi-floppy games.

## Mouse

From the on-screen keyboard, another press of GAME switches to the mouse: the
D-pad moves the pointer, **B** and **A** are the left and right buttons and
**TIME** goes back to the keyboard.

To play with the mouse all the time it is easier to set it in the `.cfg`
(`mouse = on` in `[gamepad]`): the D-pad moves the pointer without opening
anything.

More detail: [input/03-onscreen-keyboard.md](input/03-onscreen-keyboard.md).
