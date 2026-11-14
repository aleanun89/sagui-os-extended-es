# Referencia: el Cipó original del repositorio de origen

Este directorio guarda el **fichero original** del que partió la adaptación del
navegador: `cipo_robson.c`, el programa del Cipó tal como estaba en el
repositorio de origen.

## Qué es y qué no es

- **No se compila.** El `Makefile` solo coge los `.c` de primer nivel de
  `userland/web/` (`$(wildcard $(WEB_DIR)/*.c)`), así que este fichero no entra en
  la imagen ni en el programa `cipo`. Es documentación viva: sirve para
  comparar el original con lo adaptado.
- **No es el código que corre.** El que corre es `userland/programas/cipo.c`,
  adaptado al motor de verdad (CSS, JavaScript, Canvas, SVG, WebGL) y a nuestro
  `sagui.h`. Este fichero es el punto de partida.

## De dónde viene

| | |
| --- | --- |
| Repositorio | <https://github.com/robson-apk/sagui-os> |
| Rama | `cipo-web` |
| Commit | `03906c8` |
| Fichero | `cipo_robson.c` (2 716 líneas) |

El motor entero se trajo de ahí (ver `../ORIGEM.md` para el detalle de cada
fichero). Este `cipo_robson.c` es el **programa** del navegador de aquel
repositorio: el que usaba el motor antes de que nosotros adaptásemos el motor y
el navegador a nuestro sistema.

## Licencia

El repositorio de origen no tenía fichero LICENSE, pero su autor propuso este código al proyecto
original (GPL-3.0) como PR #6 de `erigobeli/sagui-os`: se trata como **GPL-3.0-only** (D262,
`userland/web/ORIGEM.md`). Se guarda **como referencia** y no se compila.
