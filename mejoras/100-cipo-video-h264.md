# 100 - Vídeo H.264/AAC en el Cipó (reproductor de robson-apk)

- **Estado:** aprobada (**v8.0.1**, decidido por el mantenedor el 2026-11-14)
- **Prioridad:** media
- **Alta:** 2026-11-14
- **Relacionadas:** motor web del Cipó (D141, `userland/web/ORIGEM.md`), licencias del motor (D262),
  reproductor y códecs propios (`reproductor`, MPEG-1), audio HDA

## Motivación

El motor web del Cipó vino de la rama `cipo-web` de robson-apk (commit `03906c8`), que también tiene
un reproductor de vídeo **H.264 (Baseline y Main, CABAC, cuadros B) y AAC**: con él el Cipó reproduce
vídeos de YouTube. Ese trabajo está en el PR #6 del SaguiOS original (`erigobeli/sagui-os`), con
licencia GPL-3.0. No lo trajimos en D141.

## Propuesta

- Traer el decodificador H.264/AAC y el reproductor de vídeo del Cipó desde la rama del PR #6,
  con las tablas CABAC/CAVLC (de FFmpeg, LGPL-2.1+) y los libros de Huffman del AAC (de FAAD2,
  GPL-2.0+) separados en `terceros/` con su licencia, como ordena D262.
- Que el reproductor del sistema (`reproductor`) también pueda abrir MP4 (H.264/AAC) locales.
- Medir el rendimiento en las máquinas de prueba (dv7, Amilo): en un PC de 2006 el H.264 por software
  puede no llegar a tiempo real; si no llega, resolución o cuadros reducidos.

## Pruebas
- Un MP4 de prueba propio (generado con ffmpeg en el anfitrión) reproducido en QEMU: cuadros
  comparados con los del anfitrión y audio por la tarjeta emulada.
- Las del Cipó y el motor web siguen pasando.

## Criterios de aceptación
- [ ] H.264 Baseline/Main y AAC-LC decodificados igual que ffmpeg (con tolerancia)
- [ ] Vídeo en el Cipó y MP4 en el reproductor
- [ ] Licencias de las tablas registradas (D262)
- [ ] Test automático en `tests/`
- [ ] Decisión registrada en `docs/decisoes.md`

## Notas
- Modelo sugerido: Sonnet (port y adaptación, con pruebas medibles).
