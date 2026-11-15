# Licencia: GPL v3 explicada (D258)

Este documento explica, para personas, con qué licencia se distribuye SaguiOS-Extended y qué
significa para quien lo usa, lo modifica o lo redistribuye. No es asesoramiento jurídico: es
lo que el proyecto decide y cómo lo aplica. El texto legal es [`LICENSE`](../LICENSE) (GPL v3).

## De dónde viene

SaguiOS es obra de **Eli Rigobeli** (https://github.com/erigobeli/sagui-os) y es GPL v3. Eli
autorizó este fork en una issue del repositorio original: «Yes, go for it! SaguiOS is GPL v3, so
feel free to translate it, try it on your laptop and publish your version, as long as it stays
GPL v3 with the source available. A link back to SaguiOS would be much appreciated.» Y: «SaguiOS is
written from scratch, without copying code from other systems.» La condición es doble: seguir en
GPL v3 y dar el código fuente. El enlace de vuelta está en el `README.md`.

## Las capas

| Capa | Licencia (`SPDX-License-Identifier`) |
| --- | --- |
| El conjunto: el repositorio y las imágenes | GPL v3 |
| Ficheros que existían en el commit `dba0fffc` («SaguiOS original») y cualquier derivado de ellos, aunque estén muy cambiados o renombrados | `GPL-3.0-only` |
| Ficheros nuevos de este fork | `GPL-2.0-or-later` |
| Terceros | la suya, en [`terceros.md`](terceros.md) |

**Por qué el código nuevo es `GPL-2.0-or-later`:** «o posterior» incluye la v3, así que el
conjunto sigue siendo GPL v3 sin problema (el código nuevo se puede combinar con el de Eli y se
distribuye bajo la v3). Pero deja la puerta abierta a combinarlo con código GPL v2 «o posterior»
(Quake, OpenJazz, OpenTyrian...) y a que, si algún día el proyecto cambia de rumbo, el código
nuevo no quede atado a una sola versión. El código de Eli no se puede relicenciar: queda en
`GPL-3.0-only` (no hemos podido comprobar sin red si su repositorio dice «o posterior»; si lo dice,
se podrá pasar a `GPL-3.0-or-later`).

## Qué significa la GPL v3

**Si usas SaguiOS** (lo arrancas, lo pruebas, lo instalas): nada que cumplir. Es libre y gratis.

**Si lo modificas para ti**: nada que cumplir mientras no lo entregues a otros.

**Si lo redistribuyes** (imágenes, ISO, un fork, tu propia versión), con o sin cambios:
1. Mantén la GPL v3 (para lo que es de Eli) y no añadas restricciones.
2. Entrega el código fuente correspondiente a lo que distribuyes, o una oferta escrita de dárselo.
3. Conserva los avisos de copyright, las líneas SPDX y el texto de `LICENSE`.
4. Marca tus cambios (el historial de git sirve).
5. Cada receptor recibe los mismos derechos. Hay una cláusula de patentes: no puedes demandar por
   patentes sobre el código y seguir distribuyéndolo.
6. Es de cortesía (y lo pide Eli) enlazar al SaguiOS original.

**Usar en privado no es distribuir.** Compilar para ti una imagen con un componente que no se podría
publicar es legal; entregársela a otra persona ya es distribuir.

## Marcar cada fichero (SPDX)

Cada fichero propio lleva en su **primera línea de comentario** (después de `#!` o de la línea de
codificación en los guiones):

```
// SPDX-License-Identifier: GPL-2.0-or-later     (C, C++, cabeceras)
; SPDX-License-Identifier: GPL-2.0-or-later      (ensamblador NASM)
# SPDX-License-Identifier: GPL-3.0-only          (Python, shell, Makefile, ensamblador GAS)
```

`tools/spdx.py` lo hace solo: elige `GPL-3.0-only` si el fichero (o aquel del que procede según el
historial de git) existía en `dba0fffc`, y `GPL-2.0-or-later` si no. Funciona con bytes, así que no
cambia la codificación (`arquivos.c` y `cipo.c` son Latin-1) ni los finales de línea; es idempotente;
`--comprobar` solo informa. **Un fichero nuevo**: ponle `GPL-2.0-or-later` (o ejecuta `tools/spdx.py`
antes de subirlo). **Un fichero que sustituye a uno de Eli** (lo reescribes, lo partes en varios):
sigue siendo derivado, va en `GPL-3.0-only` (para un derivado que git no detecte como renombrado,
añádelo a `DERIVADOS_EXTRA` de `tools/spdx.py`).

Lo que no se marca: `userland/terceros/`, `kernel/terceros/`, `userland/micropython/`,
`userland/web/quickjs/` y lo registrado en [`terceros.md`](terceros.md), que conserva su licencia.

## Qué NO se puede meter

- **Código `GPL-2.0-only`** (el típico de Linux: «under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation», sin «or later», o `SPDX: GPL-2.0` /
  `GPL-2.0-only`). La v2 sola y la v3 son incompatibles: no se pueden combinar en un mismo binario. Lo
  que aún queda de Linux en `kernel/terceros/linux/` está en retirada (rama `v8.0b7-sin-linux`) y no
  debe reaparecer.
- Código sin licencia clara, o con restricciones («solo no comercial», «sin redistribuir»).
- Código con requisitos que la GPL v3 no permite (la cláusula de publicidad de BSD antiguo, p. ej.).

`tools/auditar_licencias.py` falla si ve `GPL-2.0-only` fuera de lo permitido, si falta la línea SPDX
en un fichero propio (con `--estricto`) o si NetSurf entra en lo publicable.

## Un port nuevo (un programa de terceros)

1. Comprueba su licencia **en las cabeceras de los ficheros**, no solo en el README: «version 2 or
   (at your option) any later version» sirve; «version 2 of the License.» a secas, no.
2. Si es MIT, BSD de dos o tres cláusulas, ISC, zlib, dominio público, LGPL-2.1+, GPL-2.0-or-later,
   GPL-3.0 o Apache-2.0: aíslalo en `userland/terceros/<nombre>/`, sin tocar sus ficheros, y
   regístralo en [`terceros.md`](terceros.md) con la licencia exacta, **en el mismo commit**.
3. Si es solo v2 y se enlaza con la libsagui o la libc común: **no entra en lo publicado** (ver
   NetSurf). Puede vivir como uso privado, como NetSurf, si el mantenedor lo decide.
4. Si es un programa independiente que no se enlaza con nuestras bibliotecas ni se compila a un mismo
   binario, el criterio es más laxo, pero hoy todos los ports se enlazan con la libsagui: se aplica el
   punto 3.

## Por qué NetSurf no va en las imágenes publicadas

NetSurf es **GPL-2.0 solo la v2** (sus ficheros dicen «version 2 of the License.» sin «or later»).
En SaguiOS se compila enlazado con la libsagui y la libc común, que contienen código `GPL-3.0-only`
de Eli. Un solo binario no puede ser a la vez v2-solo y v3-solo. Por eso, desde esta decisión:

- `make variantes` (lo publicable, `gerar_versao.sh` sin `--con-input-share`) pasa `NETSURF=0` y
  **no** incluye NetSurf.
- `make priv_variantes` (con `input_share/`, uso propio) **sí** lo incluye. Usarlo en privado no es
  distribuir. **No publicar esas imágenes.**
- `make NETSURF=0` lo quita de cualquier compilación; las fuentes del upstream no van en git
  (`tools/netsurf_fuentes.sh` las baja).

**Uso propio de NetSurf y GNUBoy con solo el repositorio público (D261).** El repositorio público lleva
nuestro pegamento (`netsurf.mk`, `gnuboy.mk`, `plataforma/`, `libc/`: propio, `GPL-2.0-or-later`) y los
guiones que descargan el código original, pero **no** ese código. `make priv_variantes` lo descarga y lo
compila para quien lo ejecuta; esas imágenes **no se redistribuyen**.

## La variante `SEM_GPL=1`

Ya no significa «sin nada GPL» (el código propio es GPL v3). Significa **sin ports de terceros con
licencia GPL**: deja fuera `kernel/terceros/linux/` (mientras exista), OpenTyrian, NetSurf, GNU Prolog,
OpenJazz, Vanilla Conquer, Quake, GNUBoy y DOSBox. Sirve para una imagen mínima cuyo único código de terceros
es permisivo (MIT, BSD, dominio público) y para comprobar que lo propio no depende de esos ports
(`tests/h81_licencias.py`). La carpeta es `build-sem-gpl/` y `make entrega-sin-gpl` la prepara.

## Licencias de los ports GPL (comprobado en las cabeceras, D258)

| Port | Licencia exacta | Compatible con la libsagui (GPL-3.0) | Nota |
| --- | --- | --- | --- |
| OpenTyrian | GPL-2.0-or-later (98 de 104 ficheros C; el resto: cJSON MIT y tres sin cabecera) | Sí | |
| GNUBoy (SDL2-GNUBoy) | GPL-2.0; solo unos pocos ficheros dicen «or later», casi todos no tienen cabecera y `COPYING`/README dicen v2 | **Dudosa: tratada como solo v2** | **Decidido (D259): fuera de lo publicable, dentro de lo privado** |
| Quake (WinQuake) | GPL-2.0-or-later (cada fichero) | Sí | |
| OpenJazz | GPL-2.0-or-later (cabeceras y `licenses.txt`) | Sí | miniz, argparse, stb: MIT; psmplug: dominio público |
| DOSBox 0.74-3 (D294) | GPL-2.0-or-later (cabecera de cada fichero: «either version 2 of the License, or (at your option) any later version»; `COPYING`) | Sí | `src/hardware/mame/` (SAA1099, SN76496): BSD-3-Clause (MAME) y `src/hardware/dbopl.cpp`: GPL-2.0-or-later |
| GNU Prolog 1.5.0 | LGPL-3.0-or-later o GPL-2.0-or-later, a elegir | Sí (se elige la v3) | |
| Vanilla Conquer | GPL-3.0-or-later con términos adicionales de EA (sección 7) | Sí | |
| NetSurf 3.11 | GPL-2.0 solo la v2 (unos 390 ficheros; solo uno dice «or later») | **No** | fuera de lo publicado |

## Qué va en lo publicable y qué solo en lo privado (D259)

`PUBLICABLE=1` (lo pasa `gerar_versao.sh` sin `--con-input-share`, es decir `make variantes`) deja fuera todo
lo que es GPL-2.0 solo la v2; `make priv_variantes` y el `make all` de desarrollo lo incluyen.

| Pieza | Licencia | Publicable | Privado | Cómo se deja fuera |
| --- | --- | --- | --- | --- |
| NetSurf | GPL-2.0-only | No | Sí | `NETSURF=0` |
| GNUBoy | GPL-2.0 (tratada como solo v2) | No | Sí | `GNUBOY=0` |
| r8169 de Linux (`kernel/terceros/linux/r8169/`) | GPL-2.0-only | No (inicio clásico) | Sí | `-DSAGUI_PUBLICABLE` |
| Códec IDT y desvío Tctl/Tdie (`kernel/terceros/linux/{hda,hwmon}/`) | GPL-2.0-or-later | Sí | Sí | |
| TjMax de FreeBSD (`kernel/terceros/bsd/coretemp/`) | BSD-2-Clause | Sí | Sí | |
| Código de M4R4G0N (e1000, pila TCP/IP, Cipó) | GPL-3.0-only | Sí | Sí | |
| OpenTyrian, OpenJazz, Quake, GNU Prolog, Vanilla Conquer, DOSBox | or-later / GPL-3.0 | Sí | Sí | |
| Gearsystem (D321) | GPL-3.0 | Sí | Sí | |
| LaiNES (D321) | BSD-2-Clause (APU: LGPL-2.1+) | Sí | Sí | |

El repositorio público excluye `kernel/terceros/linux/r8169/` y `userland/terceros/gnuboy/`
(`tools/publicar-excluir.txt`). Sustituto limpio del r8169: mejora 099.

## Tabla de compatibilidad

«Con GPL v3» = ¿se puede combinar con el conjunto (GPL v3) y distribuir el resultado bajo la v3? «Con
la libsagui» = ¿se puede enlazar con la libsagui, que tiene código `GPL-3.0-only`, en un mismo binario?

| Licencia | Con GPL v3 | Con la libsagui | Comentario |
| --- | --- | --- | --- |
| MIT | Sí | Sí | Conserva el aviso de copyright (MicroPython, uACPI, SQLite*) |
| BSD de 2 y 3 cláusulas | Sí | Sí | Igual. La BSD de 4 cláusulas (con publicidad) no |
| ISC | Sí | Sí | Equivale a la MIT |
| LGPL-2.1 o posterior | Sí | Sí | «O posterior» permite pasarla a la v3; solo LGPL-2.1 sin «o posterior»: sí como biblioteca enlazada dinámicamente, no mezclada en el mismo ejecutable estático con la v3 |
| GPL-2.0-or-later | Sí | Sí | Se usa bajo la v3 y el resultado es GPL v3 (el código nuevo del fork) |
| GPL-2.0-only | **No** | **No** | v2 sola y v3 incompatibles (Linux, NetSurf, GNUBoy en la práctica) |
| GPL-3.0 | Sí | Sí | La misma licencia (Vanilla Conquer, con los términos de EA) |
| Apache-2.0 | Sí (con la v3, no con la v2 sola) | Sí | Compatible con la v3 por su cláusula de patentes; no con la GPL-2.0-only |

\* SQLite está en el dominio público, sin condiciones.
