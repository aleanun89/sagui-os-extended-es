# 015 - SSE2 en las rutas calientes de gráficos y memoria

- **Estado:** hecha (falta medir en el G470)
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

La composición de ventanas y la copia del búfer trasero a la tarjeta (en el kernel) usaban
bucles de 4 bytes en C, y `memcpy`/`memset`/`memmove` de libsagui iban byte a byte. En el
Sagui 3D, dos tercios de cada cuadro eran copiar píxeles. La idea venía de la mejora 008
(FPU/MMX/SSE): aprovechar SSE2 sin perder los i686 sin SSE2.

## Propuesta (aplicada)

Detalle y medidas en `docs/decisoes.md`, D042.

- `kernel/arch/fpu.c`: `fpu_kernel_inicio()`/`fpu_kernel_fim()` (como
  `kernel_fpu_begin/end` de Linux): FXSAVE del dueño, `dono = NULL`, CLTS, sin
  interrupciones; al final CR0.TS.
- `kernel/lib/sse2.c` (único archivo con `-msse2`, compartido con libsagui) y
  `kernel/lib/rapido.c`: el arranque mide `rep movsd` frente a SSE2/`movntdq` y elige para
  la composición, los rectángulos y la copia a la tarjeta.
- `kernel/gui/janelas.c`: no compone el fondo ni las ventanas tapadas por una ventana que
  cubre todo el recorte.
- `memcpy`/`memset` del kernel con `rep movsd`/`rep stosd`.
- `userland/libsagui/rapido.c`: `memcpy`/`memset`/`memmove` y `preencher32`,
  `ampliar2x32`, `escurecer32` con `rep` o SSE2 (CPUID + medida en las primeras llamadas de
  cada proceso, umbral de 256 bytes); `mem_metodo()` para forzar un método.
- Sagui 3D: ampliación y pausa con esas funciones, cielo sin prueba por píxel; imagen
  idéntica.
- Medida: programa `bench` y `sagui3d --medir [n]`. Test: `tests/h40_sse2.py` y
  `/testes/sse2`.

## Criterios de aceptación

- [x] Resultados idénticos a los bucles en C en muchos tamaños y alineaciones (`/testes/sse2`)
- [x] Sagui 3D píxel a píxel igual que antes, con y sin SSE2 (suma de `sagui3d --medir 200`)
- [x] Dos procesos con XMM y el compositor con SSE2 no se corrompen (`simd estres`)
- [x] Escritorio funcionando con `-cpu pentium2` (sin SSE2)
- [x] Más rápido en el QEMU: Sagui 3D de 472 a ~800 cuadros/s, ventana 640x480 de 463 a ~860
- [x] Test automático en `tests/h40_sse2.py`
- [x] Decisión registrada en `docs/decisoes.md` (D042)
- [ ] Medido en el Lenovo G470 (`bench` y `sagui3d --medir 500`)

## Notas

- Las cifras del QEMU sin KVM no valen para el PC real: el SSE se emula. La elección se
  hace midiendo en la propia máquina (arranque en el kernel, primeras llamadas en cada
  programa).
- Siguiente paso con más impacto en el G470: marcar el framebuffer como write-combining
  (PAT o MTRR). Con la memoria de vídeo sin caché, cada escritura va sola por el bus.
- La elección del kernel (`[RAPIDO]`) solo sale por el puerto serie: para verla en el
  portátil haría falta un `INFO_*` nuevo (o mostrarla en el arranque).
- Encontrado de paso: un programa en segundo plano que escribe en el Terminal mientras
  sagsh espera una orden lo cierra (`tv_ler` devuelve 0 a una lectura de 1 byte cuando hay
  que avisar de redibujar). Ver D042.
- AVX (registros de 32 bytes) necesitaría XSAVE y CR4.OSXSAVE; no compensa para copias.
