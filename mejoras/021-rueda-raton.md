# 021 - Rueda del ratón

- **Estado:** hecha
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

No había rueda del ratón: para desplazarse en el Editor, en Archivos, en el visor de
Markdown o en el Terminal había que usar el teclado o la barra de desplazamiento. El Terminal
ni siquiera guardaba las líneas que salían por arriba. Además, el visor de Markdown tenía
tamaño fijo (800x600) y no se podía maximizar.

## Propuesta (aplicada)

- **Kernel:** modo IntelliMouse en `kernel/dispositivos/i8042.c` (tasa 200, 100, 80 + ID; con
  ID 3 los paquetes son de 4 bytes) y `kernel/dispositivos/mouse.c` (cuarto byte, cuarta
  palabra del vmmouse, descarte de paquetes a medias tras 0,5 s). Evento nuevo `EV_RODA` (11)
  en `kernel/inc/abi.h`, entregado por `kernel/gui/janelas.c` a la ventana bajo el cursor.
- **libsagui:** `area_tratar` y `lista_tratar` desplazan 3 líneas por muesca.
- **Programas:** Terminal con historial de 500 líneas; Markdown con rueda por panel y ventana
  redimensionable/maximizable; Editor, Archivos e infopc a través de la libsagui.

Detalle en `docs/decisoes.md`, D048.

## Criterios de aceptación

- [x] El kernel detecta IntelliMouse (ID 3) en QEMU y sigue con 3 bytes si el ID es otro
- [x] `EV_RODA` llega a la ventana bajo el cursor (PS/2 y vmmouse)
- [x] Área de texto y listas de la libsagui se desplazan 3 líneas por muesca
- [x] Terminal con historial (500 líneas) navegable con la rueda; una tecla o la salida vuelven abajo
- [x] Markdown: rueda en visor y editor; redimensionable (esquina) y maximizable (botón)
- [x] Test automático `tests/h45_rueda.py` (capturas `docs/capturas/H45-*.png`)
- [x] Decisión registrada en `docs/decisoes.md` (D048)
- [ ] Comprobado en el G470 (touchpad): que responde ID 0 y el ratón sigue bien

## Notas

- No se intenta el ID 4 (IntelliMouse Explorer, botones 4 y 5).
- El desplazamiento con dos dedos o por el borde del touchpad Synaptics necesita su protocolo
  nativo; queda como idea.
- Rueda horizontal: no existe en el protocolo IntelliMouse básico.
- Sagui 3D ignora la rueda.
