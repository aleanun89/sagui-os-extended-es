# 108 - Vulkan: Mesa RADV (y OpenGL encima)

- **Estado:** aprobada (**v11**, tras 106 y 107; decidido por el mantenedor el 2026-11-15)
- **Prioridad:** alta
- **Alta:** 2026-11-15
- **Relacionadas:** 64 bits (106), amdgpu (107), SaguiGL, SDL 2 por GPU (v8.3)

## Propuesta
- Portar **Mesa** (MIT) al userland: **RADV** (Vulkan para AMD) con su compilador **ACO**, `libdrm` mínimo
  sobre la capa de la 107, el **cargador Vulkan** de Khronos (Apache-2.0, compatible con GPL v3).
- OpenGL encima de Vulkan (**Zink**) o radeonsi; SaguiGL y la SDL 2 por GPU sobre ello.
- Ejemplos y benchmarks Vulkan (vkcube…), y Vulkan en la pestaña GPU de Información del PC.

## Criterios de aceptación
- [ ] vkcube y una demo Vulkan en una Radeon RX
- [ ] Decisión registrada en `docs/decisoes.md`
