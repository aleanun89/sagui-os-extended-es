# 004 - MicroPython dentro de SaguiOS

- **Estado:** hecha, con `float` en double por SSE2 (ramas `compilador-micropython` y `python-float`);
  biblioteca estándar, archivos, unicode y red en la mejora 032 (rama `python-libs`)
- **Prioridad:** media
- **Alta:** 2026-11-06

## Problema / motivación

Un intérprete de Python dentro del sistema permite programar sin compilar fuera.

## Propuesta

Portar MicroPython (puerto `minimal`/`embed`) sobre libsagui: un REPL en el terminal y
`python archivo.py`. Los programas no usan la FPU (`-mgeneral-regs-only`), así que habrá que
compilarlo sin `float` o con `float` por software.

## Criterios de aceptación

- [x] REPL funcionando en el Terminal
- [x] Ejecuta `/ejemplos/hola.py`
- [x] Test automático en QEMU (`tests/b6_micropython.py`)
- [x] Licencia (MIT) registrada en `docs/decisoes.md` (D034)
- [x] Impacto en el tamaño de la imagen documentado (~123 KB; `make MICROPYTHON=0` lo quita)
- [x] `float` por hardware (D038): `/` verdadera, `math`, `cmath`, formato `%.2f`/f-strings,
  módulo `time` mínimo, ejemplos `/ejemplos/mandelbrot.py` y `/ejemplos/flotante.py`

## Resultados (float, D038)

- `double` de 64 bits con SSE2 (`-msse2 -mfpmath=sse`); libm de musl/FreeBSD copiada de
  MicroPython (`userland/micropython/libm_dbl/`, licencias en `LICENCA-LIBM.txt`), `sqrt`
  con `SQRTSD`.
- En una CPU sin SSE2, `python` avisa ("este procesador no tiene SSE2") y sale con código 1.
- QEMU: Mandelbrot 78x24 en 110-170 ms (igual que en punto fijo con enteros); bucle de
  300.000 `x = x * 0.999 + 1.5` en 380 ms (punto fijo con `//`: 710 ms).
- Tamaño: `/bin/python` 123,7 KB -> 159,5 KB; imagen 797.184 -> 836.096 bytes (56,7 %).

## Ideas siguientes

- ~~Activar `float`~~ hecho (D038).
- ~~`import` de archivos `.py` del sistema de archivos~~ hecho (mejora 032, D067).
- ~~Módulo `os`~~ hecho (mejora 032), con `open()`, `json`, `re`, `socket`, `ssl`, `requests`...
- Módulo `sagui` (ventanas, teclado, pitido): fase 3 de la mejora 032.
- Decidir si sigue activado por defecto (es la única excepción a la regla de autoría).
