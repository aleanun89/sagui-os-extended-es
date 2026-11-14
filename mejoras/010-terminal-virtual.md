# 010 - Terminal virtual (pty) para el Terminal en ventana

- **Estado:** hecha
- **Prioridad:** alta
- **Alta:** 2026-11-06

## Problema / motivación

En la ventana Terminal no se veía lo que se escribía dentro de `python` (ni en `input()`,
`cat` sin archivo o programas de `cc` con `fgets`): el eco y la edición de línea solo
existían en la consola del kernel. Tampoco había Ctrl+C para parar un programa.

## Propuesta (aplicada)

Pseudoterminal en el kernel (`kernel/fs/tv.c`, `SIS_ABRIR_TV`, `SIS_PRIMEIRO_PLANO`), que
el Terminal usa en lugar de dos pipes. Detalle en `docs/decisoes.md`, D036.

## Criterios de aceptación

- [x] El REPL de `python` e `input()` muestran lo que se escribe en la ventana Terminal
- [x] Retroceso, Intro y Ctrl+D en modo línea
- [x] Ctrl+C termina el programa en primer plano; el shell sigue
- [x] Test `tests/h37_terminal_virtual.py`
- [ ] Ctrl+C también en la consola del kernel
- [ ] `KeyboardInterrupt` en MicroPython en lugar de terminarlo
- [x] Grupos de procesos: Ctrl+C a todo un pipe (`cat | wc`), no solo al último (D037)
