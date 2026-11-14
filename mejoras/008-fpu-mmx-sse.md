# 008 - FPU x87, MMX y SSE para los programas

- **Estado:** hecha (falta probar en el G470)
- **Prioridad:** alta
- **Alta:** 2026-11-06

## Problema / motivación

`CR0.EM` estaba siempre encendido: cualquier instrucción de coma flotante, MMX o SSE
terminaba el programa. Sin `double` ni SIMD no se pueden portar MicroPython, juegos ni
gráficos con rendimiento.

## Propuesta (aplicada)

- `kernel/arch/fpu.c` + `kernel/inc/fpu.h`: detección con CPUID, CR0/CR4 y el estado de
  cada proceso con cambio perezoso (CR0.TS + #NM + FXSAVE/FXRSTOR). Detalle en
  `docs/decisoes.md`, D032.
- `processo_t` tiene `fpu[512]` y `fpu_usada`; el planificador llama a `fpu_trocar()`.
- El arranque muestra `[ ok ] CPU GenuineIntel: FPU MMX SSE SSE2 ...`.
- `userland/libsagui/inicio.asm`: pila alineada a 16 en el `call principal`.
- Programa `/bin/simd` (compilado con `-msse2 -mfpmath=sse`) y test `tests/h35_simd.py`.

## Criterios de aceptación

- [x] x87, MMX, SSE y SSE2 funcionan en un programa
- [x] Dos procesos con XMM distintos no se pisan (`simd estres`)
- [x] Una excepción de coma flotante solo cierra el programa
- [x] Arranca en una CPU sin SSE (`-cpu pentium2`)
- [ ] Probado en el Lenovo G470
- [x] Batería completa de tests en verde (38/38)

## Ideas siguientes

- Opción `make SIMD=1` para compilar libsagui y todos los programas con SSE2 (más
  rápido, pero ya no arrancaría en CPUs sin SSE2).
- ~~Usar SSE2 en las rutinas calientes de libsagui: rellenar y copiar píxeles en la GUI y
  en Sagui 3D (`memcpy`/`memset` con `movdqa`), detectándolo en tiempo de ejecución.~~
  **Hecho** en la mejora 015 (D042), también en el compositor del kernel
  (`fpu_kernel_inicio/fim`).
- AVX (necesita XSAVE y CR4.OSXSAVE; el G470, Sandy Bridge, lo tiene).
- ~~Que MicroPython (004) use `float` por hardware~~: hecho (D038, `double` por SSE2).
- Hecho: el compilador `cc` ya usa la FPU: `float`/`double` con SSE2 por defecto y con la
  x87 usando `cc -x87` (D039).
