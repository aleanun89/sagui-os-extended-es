# Autores y créditos

## Proyecto original

**SaguiOS** es un proyecto creado por **Eli Rigobeli**
([`erigobeli/sagui-os`](https://github.com/erigobeli/sagui-os)), publicado con licencia **GPL v3**:
la idea, la especificación ([`prd.md`](prd.md)), la dirección de cada fase y las pruebas en el
hardware real (Lenovo G470, fotos en [`testes_eli/`](testes_eli/)). El código del proyecto
original lo escribió Claude (Anthropic) para el proyecto de Eli.

Este repositorio es un **fork** de ese proyecto. Se publica con el permiso expreso de Eli, dado
en una issue del repositorio original (respuesta del 2026-11, en inglés, cita literal):

> «Yes, go for it! SaguiOS is GPL v3, so feel free to translate it, try it on your laptop and
> publish your version, as long as it stays GPL v3 with the source available. A link back to
> SaguiOS would be much appreciated.»

> «SaguiOS is written from scratch, without copying code from other systems.»

Por eso este proyecto mantiene la GPL v3 y enlaza de vuelta al original desde el
[`README.md`](README.md). Eli ha anunciado además una versión 2.0 del original y acepta PRs.

## Contribuciones al proyecto original

| Autor | Contribución |
| --- | --- |
| **Eli Rigobeli** | creador del proyecto, especificación, validación en el G470 |
| **robson-apk** (Robson de Lima Barbosa) | PR #6 del repositorio original (abierto el 2026-11-10): el navegador Cipó con motor web (HTML/CSS, JavaScript, Canvas, SVG, WebGL, fuentes) y el reproductor de vídeo. El motor está integrado aquí (D141, `userland/web/ORIGEM.md`); su código es **GPL-3.0-only** (D262): aportado al repositorio original (GPL-3.0) con ese PR, Términos de GitHub D.6; confirmación explícita pedida al autor. Reconoció (2026-11-14) trozos que siguen font-rs, libwebp, V8, FFmpeg y FAAD2: ver `docs/terceros.md` Confirmado el 2026-11-14: aceptado para fusionarse en el SaguiOS original (GPL-3.0). |
| **M4R4G0N** | PR #3 del repositorio original: driver e1000, base de la pila TCP/IP y el navegador Cipó (ver D046 y D047 en [`docs/decisoes.md`](docs/decisoes.md)), aportados bajo la licencia del original (GPL v3); su código es **GPL-3.0-only** (D259) |

## Este fork

| Autor | Contribución |
| --- | --- |
| **Alejandro Anuncibay** | mantenedor del fork; dirección de las mejoras (ver [`mejoras/`](mejoras/README.md)) |
| **Claude (Anthropic)** | herramienta con la que se ha escrito el código de las mejoras, bajo la dirección del mantenedor; los commits lo indican con `Co-Authored-By` |

El historial de git conserva la autoría de cada cambio.

## Código y datos de terceros

Las piezas que no son de este proyecto están aisladas y registradas, con su licencia original,
en [`docs/terceros.md`](docs/terceros.md).

## Licencia

El conjunto se distribuye bajo la **GNU General Public License, versión 3** (archivo
[`LICENSE`](LICENSE)). El código que ya estaba en el SaguiOS original es `GPL-3.0-only`; el
código nuevo de este fork es `GPL-2.0-or-later`, que se puede combinar con él bajo la GPL v3.
Las piezas de terceros conservan su licencia. Todo en [`docs/licencia.md`](docs/licencia.md) y
D258 de [`docs/decisoes.md`](docs/decisoes.md). La rama antigua `licencia` (GPL v2) está
obsoleta.
