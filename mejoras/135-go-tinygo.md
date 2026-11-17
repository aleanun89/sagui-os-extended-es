# 135 - Soporte de Go con TinyGo (D364)

- **Estado:** hecha (fase 1: TinyGo; la fase 2, Go oficial, queda propuesta)
- **Prioridad:** media
- **Alta:** 2026-11-16

## Problema / motivación

SaguiOS solo tenía C, Pascal y MicroPython/JS como lenguajes de programas. Go es un lenguaje
muy pedido, con goroutines y una biblioteca estándar amplia. El Go oficial necesita cosas que
el kernel no tiene (TLS por hilo, poll/epoll, señales); TinyGo, con su planificador cooperativo
y su recolector propio, no.

## Propuesta (hecha)

- `tools/tinygo_entorno.sh instalar`: TinyGo 0.44.0 y Go 1.26.11 en `~/.local/saguitools`
  (SHA-256 comprobado, sin sudo, idempotente) y dos copias parcheadas (`go-sagui`, `tinygo-sagui`).
- Objetivo de TinyGo `userland/go/alvos/saguios.json` (i386, Pentium sin SSE; variante SSE2).
- Paquete `syscall` propio (`userland/go/syscall/`) sobre la capa C `userland/go/runtime/glue.c`
  y la libc de SaguiOS; el runtime «Linux con libc» de TinyGo con un parche del montón.
- `tools/go-sagui build`, `userland/go/go.mk` (a `/apps/go/`), ocho ejemplos y `docs/go.md`.

## Criterios de aceptación

- [x] Los ejemplos compilan y corren en QEMU (`tests/h366_tinygo.py`).
- [x] Sin TinyGo, `make` y la prueba se saltan con aviso.
- [x] D364 en `docs/decisoes.md`, fila en `docs/terceros.md`.

## Notas

Fase 2 (Go oficial `GOOS=saguios`): ver `docs/go.md`.
