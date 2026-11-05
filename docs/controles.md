# Controles

*[Read in English](controles.en.md) · Vuelve al [README](../README.md)*

## Botones

Sin `[gamepad]` en el `.cfg`:

| Botón | Tecla del PC |
|---|---|
| Cruceta | Flechas |
| A | Espacio |
| B | Intro |
| TIME | Esc |
| START | Y |
| SELECT | N |

START y SELECT solo existen en las consolas que los tienen. Cualquier botón
salvo GAME se puede cambiar en el `.cfg` (ver [cfg.md](cfg.md)).

Siempre:

| Botón | Qué hace |
|---|---|
| **GAME** | Teclado en pantalla → ratón → cerrar (cada pulsación pasa al siguiente) |
| **PAUSE/SET** | Menú de retro-go: velocidad de la CPU (CPU Speed), salir… |
| **PAUSE/SET + izquierda/derecha** | Volumen |
| **PAUSE/SET + arriba/abajo** | Brillo |

## Teclado en pantalla

Ocupa dos filas arriba y dos abajo; el juego no se mueve.

- Cruceta para moverse (mantenida sigue avanzando), **A** pulsa la tecla,
  **B** es Retroceso, **TIME** es Shift.
- **Shift, Ctrl y Alt** se quedan pulsados para la siguiente tecla (en verde).
  Shift pulsado dos veces se queda bloqueado (en rojo, como Bloq Mayús); una
  tercera vez lo suelta.
- La tecla **`:`** manda los dos puntos de una vez, sin pasar por Shift.
- La **`X`** verde cierra el teclado.

## Cambiar de disco

La tecla verde **`A:`** del teclado en pantalla abre la lista de `.dsk` de la
carpeta: arriba/abajo elige, **A** lo carga en lugar del disco actual, **B**
vuelve. Sirve para los juegos de varios disquetes.

## Ratón

Desde el teclado en pantalla, otra pulsación de GAME pasa al ratón: la cruceta
mueve el puntero, **B** y **A** son los botones izquierdo y derecho y **TIME**
vuelve al teclado.

Para jugar todo el rato con ratón es más cómodo ponerlo en el `.cfg`
(`mouse = on` en `[gamepad]`): la cruceta mueve el puntero sin abrir nada.

Más detalle: [input/03-onscreen-keyboard.md](input/03-onscreen-keyboard.md).
