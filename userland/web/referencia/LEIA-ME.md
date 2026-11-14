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

El repositorio de origen **no tiene fichero LICENSE**. Su propio `README` dice
que su código es «sem código de outros sistemas», y que lo que trae de terceros
son solo tablas de datos de especificaciones. El código lo escribió una IA a
partir del `prd.md` del mismo proyecto.

Mientras la licencia de ese repositorio no esté aclarada (está pendiente el
permiso de los autores, como el de la rama `licencia` de este repo), este
fichero se guarda **como referencia**, con su procedencia escrita aquí, y no se
usa para nada más. Lo mismo dice `docs/terceros.md`.

Si algún día la licencia se aclara o los autores dan permiso, esto se puede
quitar sin tocar nada más: el motor que se usa ya está en `userland/web/` y en
`userland/programas/cipo.c`, y no dependen de este fichero.
