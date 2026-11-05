# Crear los `.dsk` de los juegos

*[Read in English](discos.en.md) · Vuelve al [README](../README.md)*

Cada juego va en la tarjeta como **una imagen de disco `.dsk`** en
`/roms/dos/`: un archivo que por dentro es un disco de PC con los archivos del
juego. El núcleo arranca desde esa imagen, y lo que el juego guarda (partidas,
configuración) se escribe dentro de ella.

## Lo que necesitas

- Los archivos del juego en una carpeta del PC (ya instalado: ver más abajo).
- **Python 3** y **mtools**, y este repositorio descargado para tener
  `scripts/make_dsk.py`.
  - Linux: `sudo apt install mtools` (Debian/Ubuntu).
  - macOS: `brew install mtools`.
  - Windows: dentro de WSL (Ubuntu), igual que en Linux.

## Paso a paso

1. Pon los archivos del juego en una carpeta, por ejemplo `MONKEY/`. Lo que
   haya dentro de la carpeta será la raíz del disco.
2. Mira cómo se llama el programa que arranca el juego (`MONKEY.EXE`,
   `DUNE2.EXE`, `GAME.BAT`…).
3. Crea la imagen:

   ```
   python3 scripts/make_dsk.py --src MONKEY --out MONKEY.dsk --autoexec MONKEY
   ```

   `--autoexec` crea dentro un `AUTOEXEC.BAT` de una línea que arranca ese
   programa. Si prefieres decirlo en el `.cfg`, puedes omitirlo.
4. Copia `MONKEY.dsk` a `/roms/dos/` y, si lo tiene, su `MONKEY.cfg` a
   `/cores/dosat/`.

## Disquete o disco duro

El script elige solo según lo que ocupe el juego:

- **Hasta 2,88 MB** crea un disquete, y el juego se ve en la unidad `A:`.
- **Más grande** crea un disco duro, y el juego se ve en `C:`.

Algunos juegos no quieren arrancar desde un disquete (Risky Woods, por
ejemplo). Para esos fuerza un disco duro con `--size-mb`:

```
python3 scripts/make_dsk.py --src RISKY --out RISKY.dsk --autoexec RISKY --size-mb 3
```

También sirve para dejar sitio libre a las partidas guardadas. Una imagen de
disquete `.img` que ya tengas vale tal cual: cámbiale la extensión a `.dsk`.

**Sin mtools** (por ejemplo en Windows sin WSL): el `tools/fat12.py` del otro
núcleo ([DosXT-retro-go-sd](https://github.com/julenvitoria/DosXT-retro-go-sd))
solo necesita Python 3 y hace lo mismo:

```
python3 fat12.py todsk MONKEY MONKEY.dsk --run "MONKEY"
```

## Qué programa arranca

Por orden:

1. El que diga el `.cfg`, en `[autoexec]` (ver [cfg.md](cfg.md)).
2. El que diga el `AUTOEXEC.BAT` del `.dsk`. Se entienden `cd` y una línea con
   el programa; `CAT` busca `CAT.COM` y luego `CAT.EXE`.

## Casos especiales

**Juegos con instalador.** Muchos juegos venían comprimidos y había que
instalarlos. Lo más sencillo es instalarlos en el PC con
[DOSBox](https://www.dosbox.com/) y hacer el `.dsk` con la carpeta ya
instalada.

**Juegos de varios disquetes.** Si se instalan, como arriba: un único `.dsk`
con todo. Si se juegan cambiando de disquete, haz un `.dsk` por disquete con
nombres que vayan seguidos. En la partida, la tecla verde **`A:`** del teclado
en pantalla abre la lista de discos para cambiar (ver
[controles.md](controles.md)).

**Juegos de CD.** Copia a la carpeta del juego todo lo que use del CD. No hay
unidad de CD emulada ni música de CD: funcionan los juegos que pueden leerlo
todo desde el disco duro (por ejemplo las versiones «talkie» de Monkey Island 2
o de Indiana Jones and the Fate of Atlantis).

**Juegos de LucasArts en disquetes.** `scripts/lucasarts_unpack.py` saca los
archivos de los disquetes de instalación sin tener que instalar (ver
[TOOLING.md](../TOOLING.md)).
