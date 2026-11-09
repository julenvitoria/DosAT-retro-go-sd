# DosAT: MS-DOS para la Game & Watch (retro-go SD)

*[Read in English](README.en.md)*

Un PC con MS-DOS como núcleo externo del firmware **retro-go SD**. Es el
[TinyEMU de Fabrice Bellard](docs/tinyemu-original.md) con la CPU x86 reescrita
en ensamblador para la consola.

- **No hace falta instalar MS-DOS**: el núcleo trae su propio DOS y cada juego
  arranca directamente.
- CPU 386, gráficos CGA, EGA y VGA, sonido AdLib y Sound Blaster, ratón, y
  memoria EMS y XMS.
- Para juegos de PC XT (8088, CGA) está el otro núcleo,
  [DosXT-retro-go-sd](https://github.com/julenvitoria/DosXT-retro-go-sd)
  (`dosxt.bin`). Los dos conviven en la misma tarjeta.

## En la tarjeta SD

```
/cores/dosat.bin           <- el núcleo
/roms/dos/MONKEY.dsk       <- cada juego, como imagen de disco .dsk
/cores/dosat/MONKEY.cfg    <- su configuración (opcional), con el mismo nombre
```

## Crear el `.dsk` de un juego

Un `.dsk` es una imagen de disco con los archivos del juego dentro. Con la
carpeta del juego en el PC:

```
python3 scripts/make_dsk.py --src MONKEY --out MONKEY.dsk --autoexec MONKEY
```

`--autoexec` es el programa que arranca el juego. Una imagen de disquete
(`.img`) también vale: basta con cambiarle la extensión a `.dsk`.

Juegos grandes, de CD, de varios disquetes o con instalador:
[**docs/discos.md**](docs/discos.md).

## Configurar un juego (`.cfg`)

El `.cfg` es opcional y usa el formato de DOSBox. Un ejemplo completo, con lo
que más se usa:

```ini
; /cores/dosat/MONKEY.cfg

[cpu]
cycles = max          ; velocidad: max, o 330 (PC XT) … 4500 (386DX a 25 MHz)

[dos]
xms = true            ; memoria extendida, para juegos que la piden
ems = false           ; memoria expandida

[autoexec]
monkey a              ; el programa, con sus opciones (a = música AdLib)

[gamepad]
mouse  = on           ; la cruceta mueve el puntero del ratón
b      = click        ; B: botón izquierdo del ratón
a      = click2       ; A: botón derecho
start  = enter        ; START: tecla Intro
select = esc          ; SELECT: tecla Esc
time   = f5           ; TIME: F5 (en Monkey Island, guardar partida)
```

- Si el juego está en una carpeta del `.dsk`, pon antes `cd \CARPETA` en
  `[autoexec]`. Sin `[autoexec]` se usa el `AUTOEXEC.BAT` del `.dsk`.
- Sin `[gamepad]` quedan los botones por defecto: cruceta = flechas,
  A = Espacio, B = Intro, TIME = Esc.
- Un botón puede mandar hasta 3 teclas a la vez: `a = ctrl+x`.
- **Todas las opciones**, perfiles de CPU, sonido y opciones de los propios
  juegos: [**docs/cfg.md**](docs/cfg.md).
- **Nombres de las teclas**, y cómo saber cuáles usa un juego:
  [docs/input/06-gamepad-keys.md](docs/input/06-gamepad-keys.md).

## Controles

| Botón | Qué hace |
|---|---|
| **GAME** | Abre el teclado en pantalla; otra vez, el ratón; otra vez, lo cierra |
| **PAUSE/SET** | Menú de retro-go: velocidad de la CPU, salir… |
| **PAUSE/SET + cruceta** | Volumen (izquierda/derecha) y brillo (arriba/abajo) |

Teclado en pantalla, ratón y cambio de disco:
[docs/controles.md](docs/controles.md).

## Partidas guardadas

Desde el menú de retro-go (**PAUSE/SET**) puedes guardar la partida en
cualquiera de sus huecos y volver a ella cuando quieras. Al lanzar un juego que
tiene partidas guardadas, retro-go te deja elegir entre **Continuar** y empezar
de nuevo.

- **Qué se guarda:** el PC emulado entero: CPU, memoria (también la parte que
  el núcleo tenía en la tarjeta SD), vídeo, sonido y el estado del DOS. Cada
  guardado ocupa algo más de 1 MB; los juegos con DOS/4GW (Doom, Dark Forces)
  bastante más, porque usan mucha memoria extendida.
- **Tarda unos segundos** en guardar y en cargar: todo eso va y viene de la SD.
- **Solo vale con el mismo `dosat.bin` y el mismo `.cfg`.** Si actualizas el
  núcleo o cambias algo que reserva memoria (`vga_pages`, las tarjetas de
  sonido…), los guardados de antes se rechazan: no se carga nada y el juego
  sigue como estaba.
- **El disco del juego no vuelve atrás.** Lo que el juego haya escrito en su
  `.dsk` (sus propias partidas, récords, configuración) se queda como esté.
  Lo más seguro es usar o los guardados de retro-go o las partidas del propio
  juego, no mezclarlos.
- **No funciona con «Cold RAM->NOR»** activado en el menú (viene apagado).

## Más

- [docs/desarrollo.md](docs/desarrollo.md): compilar el núcleo, ramas del
  repositorio y documentación técnica.
- Licencia MIT, como el TinyEMU original ([MIT-LICENSE.txt](MIT-LICENSE.txt)).
