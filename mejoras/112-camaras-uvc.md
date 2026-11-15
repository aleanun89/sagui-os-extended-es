# 112 - Cámaras web integradas (UVC), isócrono de entrada y aplicación «Cámara»

- **Estado:** hecha en software (**v8.1**, D277); pendiente de probar en hardware (L635, L850)
- **Prioridad:** alta
- **Alta:** 2026-11-15
- **Relacionadas:** transferencias isócronas y audio USB (D275), diálogos de archivo (mejora 103, D272), decodificador JPEG (mejora 047)

## Problema / motivación

Las cámaras web integradas de los portátiles (Toshiba L635, el de pruebas del mantenedor) son dispositivos USB de clase vídeo (UVC)
que transmiten por transferencias isócronas **de entrada**; la API isócrona de la D275 solo hacía salida (altavoz).

## Propuesta

- API `usb_iso_*` con entrada en xHCI, UHCI, OHCI y EHCI (iTD, y siTD para plena velocidad tras un hub de alta velocidad).
- Driver UVC 1.1 (`kernel/usb/uvc.c`, parte pura en `uvc_nucleo.c`): descriptores, PROBE/COMMIT, YUY2 y MJPEG, montaje de cuadros.
- `INFO_CAMARA` (una llamada con una estructura), la capa `camara.h` de la libsagui y la aplicación **Cámara** (vista previa, foto en PNG o
  JPEG con el diálogo de guardar, tamaños, brillo y contraste), con una fuente de prueba para trabajar sin cámara.

## Criterios de aceptación

- [x] Isócrono de entrada en los cuatro controladores (compila; sin hardware que lo ejercite en QEMU 8.2)
- [x] Descriptores, PROBE/COMMIT y montaje de cuadros con pruebas del anfitrión (ASan/UBSan)
- [x] Aplicación «Cámara» con fuente de prueba y menú Inicio
- [x] Test automático `tests/h277_uvc.py`
- [x] Decisión D277 en `docs/decisoes.md`
- [ ] Prueba con cámara real (L635/L850): ver `docs/camara.md`

## Notas

Las cámaras USB externas siguen fuera de esta versión (D257); hasta ahora solo se trata la primera cámara UVC que aparezca.
