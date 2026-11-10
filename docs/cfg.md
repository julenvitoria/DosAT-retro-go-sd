# El `.cfg` de cada juego

*[Read in English](cfg.en.md)*

*Todo lo que se puede poner en el `.cfg` de un juego para el núcleo TinyEMU
(`dosat.bin`). Vuelve al [README](../README.md).*

## Dónde va

En la carpeta `/cores/dosat/`, con el mismo nombre que la imagen:
`/roms/dos/MONKEY.dsk` → `/cores/dosat/MONKEY.cfg`. Así no sale en el menú de
juegos, y el otro núcleo (DosXT, `dosxt.bin`, que lee `/cores/dosxt/`) puede tener su
propio `.cfg` para el mismo juego sin que se mezclen. Un `.cfg` junto a la
imagen ya no se lee. Todo es opcional: sin `.cfg` el juego arranca con lo
que diga el `AUTOEXEC.BAT` de su `.dsk` y con los botones por defecto.

## Formato

Es el de DOSBox, así que se puede copiar casi tal cual un `dosbox.conf`:
secciones entre corchetes y `clave = valor` dentro. Mayúsculas o minúsculas, da
igual.

- `;` o `#` al principio de una línea, o detrás de un espacio, empiezan un
  comentario. En `[autoexec]` solo al principio de la línea, porque una orden
  de DOS puede llevar esos caracteres.
- Una línea que no se entiende se cuenta como error y esa opción se queda como
  estaba; el resto del archivo se aplica igual.

## Opciones

### `[cpu]`

| Clave | Valores | Qué hace |
|---|---|---|
| `cycles` | `max`, o un número | Velocidad de la CPU en miles de instrucciones por segundo, como en DOSBox. Se redondea al perfil más cercano de la tabla de abajo, salvo con `[video] vsync_lock = 1`: entonces se usa el número exacto (por ejemplo `1800`), hasta que elijas otro perfil en el menú. |
| `cpu_profile` | `0` … `5` | El perfil por su número, directamente. |

Perfiles (los mismos que el menú de retro-go, PAUSE/SET → CPU Speed):

| Nº | Perfil | `cycles` |
|---|---|---|
| 0 | Max (lo que dé la consola) | `max` |
| 1 | 8088 a 4,77 MHz (PC XT) | 330 |
| 2 | 286 a 8 MHz | 800 |
| 3 | 286 a 12 MHz | 1200 |
| 4 | 386SX a 16 MHz | 2500 |
| 5 | 386DX a 25 MHz | 4500 |

**Max no frena nada: la CPU va todo lo rápido que da la consola**, unas
5.500 k en el mejor caso (como un 386DX a unos 30 MHz). Es el perfil
«vitaminado» para los juegos pesados; uno fijo de 5500 no sumaría nada, porque
la consola no puede ir más deprisa que Max, y en las escenas pesadas da menos.
Por eso no hay perfiles por encima del 386DX 25: un `cycles` de 5500 o más es
Max, y un `cpu_profile` antiguo de 6 a 8 también pasa a Max.

Algunos juegos de la época fallan o van demasiado rápidos con una CPU rápida;
para esos, un perfil lento.

### `[dos]`

| Clave | Valores | Qué hace |
|---|---|---|
| `xms` | `true` / `false` | Memoria XMS (4 MB con `true`). |
| `ems` | `true` / `false` | Memoria EMS (1 MB con `true`). |
| `xms_kb` | KB | XMS con un tamaño concreto. |
| `ems_kb` | KB, múltiplo de 16 | EMS con un tamaño concreto. |

### `[sblaster]`

El núcleo trae dos tarjetas de sonido fijas y estas dos claves solo sirven
para **quitarlas**:

| Clave | Valores | Qué hace |
|---|---|---|
| `oplmode` | `none` para quitarlo; cualquier otra cosa lo deja | El chip FM (OPL): la música AdLib y la parte FM de la Sound Blaster. |
| `sbtype` | `none` para quitarla; cualquier otra cosa la deja | La Sound Blaster (voces y efectos digitalizados). Sin ella tampoco se anuncia la variable `BLASTER`. |

Los nombres son los de DOSBox para que un `.cfg` copiado de allí funcione,
pero aquí no se elige modelo: `sb16`, `sbpro2`, `opl3`… valen lo mismo que no
poner nada. Sin estas claves están las dos tarjetas.

La Sound Blaster es una **Sound Blaster 16** (DSP 4.05) en la configuración
estándar, la que casi todos los juegos proponen por defecto:

| Ajuste | Valor |
|---|---|
| Puerto | 220 |
| IRQ | 5 |
| DMA (8 bits) | 1 |
| DMA (16 bits) | 5 |

Es decir, `BLASTER=A220 I5 D1 H5 T6`. Si el instalador o el `SETUP` del juego
pregunta, elige Sound Blaster (o Sound Blaster 16) con esos valores, y AdLib
para la música si la ofrece aparte.

Una tarjeta quitada no responde, como una ranura vacía, y el juego pasa a la
siguiente que conozca (normalmente el altavoz del PC). Sirve para juegos que se
cuelgan o suenan mal con una de ellas.

### `[video]`

| Clave | Valores | Qué hace |
|---|---|---|
| `vga_pages` | `0` a `3` | Memoria de vídeo extra para los juegos que usan el modo VGA «sin encadenar» (Mode X/Y). Sin la clave vale `1`, que va bien para casi todos. `2` le da tres pantallas completas (Wolfenstein 3D), a cambio de algo de memoria del juego. `3` es para los juegos que **desplazan la pantalla por toda la memoria de vídeo**, como **Epic Pinball**: sin ella la mesa sale doble, con rayas negras o en negro por abajo. |
| `vga_hz` | `50` a `75` | Frecuencia del retrazado vertical que ve el juego. Sin la clave vale `70`, como una VGA real. La pantalla va a 60 Hz: con `60`, un juego que dibuja un fotograma por retrazado (Epic Pinball) se mueve más uniforme, pero va un poco más lento. |
| `vsync_lock` | `0` o `1` | Con `1`, cada refresco de la pantalla lleva exactamente un retrazado del juego, sin desfase: el movimiento queda uniforme del todo. Sin la clave vale `0`. Solo funciona con una velocidad fija en `[cpu] cycles` (no con `max`) y con `vga_hz = 60`. Ejemplo para **Pinball Dreams**: `cycles = 1000`, `vga_hz = 60`, `vsync_lock = 1`. |
| `retrace_skip` | `0` o `1` | Con `1`, cuando el juego se queda en un bucle esperando el retrazado vertical, el núcleo salta esa espera en vez de ejecutarla: al juego le parece que el tiempo ha pasado, pero no gasta CPU. Sirve para juegos que se pasan buena parte de cada fotograma esperando, como **Pinball Dreams 2**, y permite darles una velocidad más alta en `[cpu] cycles`. Sin la clave vale `0`. Solo funciona con una velocidad fija (no con `max`). |

Si la pones, el núcleo no le quita esa memoria aunque el juego vaya justo de
RAM: has elegido la imagen para ese juego. Si un juego carga más lento con
ella, quítala.

### `[autoexec]`

Qué programas se arrancan. No es un archivo `.bat` completo: el núcleo trae
su propio DOS, así que de esta sección toma dos cosas:

- `cd <carpeta>` (o `cd\carpeta`): el directorio de trabajo.
- Cualquier otra línea es un programa a ejecutar, con sus parámetros. La
  última es el juego; las anteriores se ejecutan antes, en orden, y cada una
  empieza cuando la anterior termina o se queda residente (si una no existe, se
  salta). Sirve para cargar un
  driver residente antes del juego, por ejemplo el `SOUND.COM` de AdLib que
  pide Rick Dangerous II:

  ```ini
  [autoexec]
  sound
  rick2
  ```
- `mount`, `imgmount`, `echo`, `cls`, `pause`, `exit`, `set`, `keyb`, `c:`…
  se ignoran sin dar error, para poder pegar un `[autoexec]` de DOSBox.

Sin `[autoexec]` manda el `AUTOEXEC.BAT` del `.dsk`.

Lo mismo se puede escribir con claves sueltas, en cualquier sección:
`entry` (el programa), `args` (sus parámetros), `cwd` (la carpeta), `pre`
(con `entry`: programas a ejecutar antes, separados por `;`, p. ej. `pre = sound`) y
`use_cmdtail` (`true`/`false`: si se pasan los parámetros que traiga el
`AUTOEXEC.BAT` del juego cuando el `.cfg` no pone ninguno).

### `[gamepad]`

| Clave | Valores | Qué hace |
|---|---|---|
| `up` `down` `left` `right` (o `arriba` `abajo` `izquierda` `derecha`), `a`, `b`, `time`, `start` (o `x`), `select` (o `y`) | nombre de tecla, o hasta 3 con `+` | Qué tecla del PC manda cada botón. `game` no se puede cambiar: abre el teclado en pantalla. Lista de nombres: [`input/06-gamepad-keys.md`](input/06-gamepad-keys.md). |
| `repeat` | `on` / `off` | Repetición al mantener un botón pulsado (por defecto `on`). |
| `mouse` | `on` / `off` | Ratón: la cruceta mueve el puntero y A/B son los botones izquierdo y derecho (salvo que los cambies). |

## Opciones de los propios juegos (sonido, vídeo…)

Esto ya no es del emulador sino de cada juego: muchos se configuran con una
letra o una palabra detrás del nombre del ejecutable, o con un programa de
instalación. Aquí se ponen en `[autoexec]` (por ejemplo `monkey a`).

- **Juegos de LucasArts de la época (Monkey Island 1, Loom, Indiana Jones…)**:
  una letra detrás del ejecutable elige la tarjeta de sonido: `a` = AdLib,
  `r` = Roland MT-32 (no está emulada: sin música), `i` = altavoz del PC. Sin
  letra, el juego busca la tarjeta solo. Con una Sound Blaster se usa `a`: la
  música de estos juegos es FM, y la Sound Blaster lleva el mismo chip que la
  AdLib.
- **Juegos con programa de configuración** (`SETUP.EXE`, `INSTALL.EXE`,
  `SOUND.EXE`…): pon ese programa en `[autoexec]`, arráncalo una vez, elige la
  tarjeta (AdLib, o Sound Blaster en 220h / IRQ 5 / DMA 1), y luego vuelve a
  poner el programa del juego: la elección queda guardada en su carpeta.
- **Para saber qué opciones tiene un juego**: el manual o el `README`/`LEEME`
  de su carpeta.

Qué emula este núcleo: AdLib, Sound Blaster (220h, IRQ 5, DMA 1, DMA alto 5:
`BLASTER=A220 I5 D1 H5 T6`, que el juego ya encuentra puesto) y altavoz del
PC. No emula Roland MT-32, Gravis Ultrasound ni Tandy/PCjr.
