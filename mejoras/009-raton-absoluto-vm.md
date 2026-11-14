# 009 - Ratón con posición absoluta en máquinas virtuales

- **Estado:** hecha
- **Prioridad:** alta
- **Alta:** 2026-11-06

## Problema / motivación

En QEMU con ventana (WSLg) el ratón "se movía raro": el ratón PS/2 solo da desplazamientos,
así que el cursor de SaguiOS y el de Windows se separaban, y capturar el ratón en WSLg
funciona mal.

## Propuesta (aplicada)

Driver vmmouse (puerta trasera de VMware, puerto 0x5658) en `kernel/dispositivos/mouse.c`.
Solo se activa bajo un hipervisor; en el G470 todo sigue igual. Detalle en
`docs/decisoes.md`, D035.

## Criterios de aceptación

- [x] En QEMU el cursor de SaguiOS sigue al de la ventana sin capturarlo
- [x] Clic absoluto por QMP llega en el mismo píxel (`tests/h36_vmmouse.py`)
- [x] Sin vmport, el ratón PS/2 relativo sigue funcionando (h19 y pruebas de la GUI)
- [ ] Comprobado por el mantenedor con `./run_in_qemu.sh`

## Ideas siguientes

- ~~Rueda del ratón (el vmmouse ya la da, en la cuarta palabra) para desplazar en Archivos,
  Editor y Terminal.~~ **Hecha** en la mejora 021 (D048): la cuarta palabra del vmmouse y el
  modo IntelliMouse del PS/2.
- Ratón absoluto en VirtualBox (dispositivo VMMDev) o con una tableta USB (necesita un
  driver USB HID).
