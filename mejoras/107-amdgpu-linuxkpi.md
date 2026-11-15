# 107 - GPU AMD modernas: capa de compatibilidad tipo LinuxKPI y amdgpu

- **Estado:** aprobada (**v11**, tras la 106; decidido por el mantenedor el 2026-11-15)
- **Prioridad:** alta
- **Alta:** 2026-11-15
- **Relacionadas:** 64 bits (106), Vulkan (108), capa 3D común (D284, D285), varias pantallas (060), firmware (D073)

## Motivación
Soportar Radeon modernas (GCN 2012+, RDNA 1/2/3) con aceleración y, encima, Vulkan (108).

## Propuesta
- Una **capa de compatibilidad con la API interna de Linux** (como la LinuxKPI de FreeBSD: workqueues,
  mutex, spinlocks, kref, DMA, DRM/TTM mínimos) para traer el driver **amdgpu**, que es **MIT** en su
  mayor parte (comprobar fichero a fichero; lo GPL-2.0 no entra). Firmware de linux-firmware
  (redistribuible), opcional.
- Primero imagen y modos (DC: resolución nativa, varios monitores), luego las colas de órdenes (GFX/compute/SDMA).
- Referencia: el `drm-kmod` de FreeBSD.

## Criterios de aceptación
- [ ] Una Radeon RX (p. ej. en el Ryzen B450) con imagen nativa y la capa 3D común por amdgpu
- [ ] Licencias registradas en `docs/terceros.md`
- [ ] Decisión registrada en `docs/decisoes.md`
