# 002 - Teclado ES y ABNT2 seleccionable

- **Estado:** hecha
- **Prioridad:** alta
- **Alta:** 2026-11-06

## Problema / motivación

El driver solo tenía la distribución ABNT2 (Brasil). En un teclado español los símbolos
salían cambiados y no había `ñ`, `¿` ni `¡`.

## Propuesta

- `kernel/dispositivos/teclado.c`: dos mapas (`TECLADO_ES` por defecto y `TECLADO_ABNT2`),
  con AltGr propio para cada uno. En ES, AltGr+4 es la tecla muerta `~` (como en Windows),
  para poder escribir `ã`/`õ`.
- Nueva llamada de sistema `SIS_TECLADO` (60) en `kernel/inc/abi.h`, la función
  `teclado_layout()` en libsagui y el programa `/bin/teclado`:
  `teclado` (muestra), `teclado es`, `teclado abnt2`.
- Tests: `tests/qemu.py` escribe con la distribución ES. `tests/h07_teclado.py` prueba las dos
  distribuciones (autotests `teclado` y `teclado_es`).

## Criterios de aceptación

- [x] h07 pasa con ABNT2 y ES (51 comprobaciones)
- [x] Test del programa `teclado` desde el shell (h16)
- [x] Decisión registrada en `docs/decisoes.md` (D031)
- [ ] Probado en hardware real con teclado español
