# 110 - AMD moderna en 32 bits: aceleración parcial (niveles 1-3)

- **Estado:** aprobada (**v11**, junto con 107 y 108; decidido por el mantenedor el 2026-11-15)
- **Prioridad:** media
- **Alta:** 2026-11-15
- **Relacionadas:** amdgpu (107), Vulkan (108), 64 bits (106), capa 3D común (D284, D285), varias pantallas (060)

## Motivación
Dar aceleración, aunque sea parcial, a GPU AMD modernas también en la versión de 32 bits (la GPU tiene su
propia memoria virtual de 48 bits; el procesador solo necesita ver ventanas pequeñas por debajo de 4 GB, con
«Above 4G Decoding» y «Resizable BAR» desactivados).

## Propuesta (por niveles)
1. **Pantalla nativa**: modos de la tarjeta (resolución del panel/monitor, varias pantallas).
2. **2D con el motor SDMA** (copias y rellenos por DMA de la GPU), como el R600.
3. **3D con la capa común** por el anillo GFX y sombreadores sencillos (SaguiGL y SDL 2 aceleradas).
- Vulkan completo (RADV) queda para 64 bits (108).
- Empezar por **GCN: Polaris (RX 470/480/570/580) o Vega** (firmware de carga directa, buena documentación);
  RDNA (carga por el PSP, firmware firmado) después. Código de referencia: radeon/amdgpu de Linux (MIT,
  fichero a fichero) y Mesa (MIT).

## Criterios de aceptación
- [ ] Niveles 1 y 2 en una Radeon GCN real con la imagen de 32 bits
- [ ] Nivel 3 con la capa 3D común
- [ ] Decisión registrada en `docs/decisoes.md`
