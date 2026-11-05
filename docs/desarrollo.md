# Desarrollo

*[Read in English](desarrollo.en.md) · Vuelve al [README](../README.md)*

## Compilar

```
make -f Makefile.core pack
```

Sale `dos.bin`; en la tarjeta se copia como `/cores/dos386.bin`. Lo que hace
falta, los scripts y las pruebas: [TOOLING.md](../TOOLING.md).

## Ramas

- **`main`**: la versión de uso, la que se reparte como `dos386.bin`. Desde el
  30-10-2026 el chip AdLib, el ratón, la memoria flash y el archivo de
  intercambio van en la memoria que la pantalla deja libre en modo paleta
  (LUT8), y el juego tiene 158 páginas de 160. Necesita un firmware retro-go que
  cargue esa parte del núcleo.
- **`normal`**: la versión anterior, sin esa memoria extra (152 páginas), para
  firmwares que no la carguen. Ya no se desarrolla.
- Las demás ramas son pruebas: pasan a `main` cuando están probadas en la
  consola.

## Documentación técnica

- [STATUS.md](../STATUS.md): estado actual. [PLAN.md](../PLAN.md): hoja de
  ruta.
- [TOOLING.md](../TOOLING.md): los scripts y los tres niveles de prueba
  (gwemu, gwemu + retro-go, consola).
- [docs/](.): el diseño de cada parte (memoria, CPU, vídeo, temporizador…).
- [CLAUDE.md](../CLAUDE.md): las reglas del proyecto.
- [tinyemu-original.md](tinyemu-original.md): el TinyEMU de Fabrice Bellard
  del que parte.
