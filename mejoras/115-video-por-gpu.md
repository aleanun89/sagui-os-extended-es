# 115 - Aceleración de vídeo por GPU: biblioteca propia (estilo VA-API)

- **Estado:** propuesta (versión por decidir; idea del mantenedor del 2026-11-15: «VA-API no lo descartes»)
- **Prioridad:** media
- **Alta:** 2026-11-15
- **Relacionadas:** vídeo H.264/AAC (D276, mejora 100), Mesa (114), GPU (docs/gpus.md), firmware (D073)

## Motivación
Muchos PC antiguos sin CPU para descodificar H.264 por software **sí tienen decodificador de vídeo en la GPU**:
UVD en las Radeon R6xx/R7xx/Evergreen (dv7, L635), el motor de vídeo de Intel Gen5-Gen7 (L635 Intel, HD 4000 del
L850). Una **biblioteca compartida propia de aceleración de vídeo**, con una API sencilla al estilo VA-API (o la
propia API VA-API si sale a cuenta, con los drivers de Mesa/intel-vaapi-driver con licencia MIT), daría a esos
equipos vídeo fluido en el reproductor y en el Cipó.

## Propuesta
- API común (abrir decodificador, enviar NAL/porciones, recibir cuadros en superficies de la GPU, mostrar o
  copiar) usada por `video_mp4` (D276), el reproductor y el Cipó; vuelta al software si no hay hardware.
- Drivers: UVD de Radeon (firmware de linux-firmware, opcional), Intel (MFX de Gen5-Gen7), y lo de Mesa
  (`va` de Gallium) si se trae con la 114. Formatos: H.264, MPEG-2, (VC-1).
- Mostrar sin copiar a la CPU cuando el compositor pueda usar superficies de la GPU.

## Criterios de aceptación
- [ ] H.264 por la GPU en al menos una Radeon UVD y una Intel
- [ ] Vuelta limpia al software
- [ ] Decisión registrada en `docs/decisoes.md`
