# 111 - Intel moderna (Gen5-Gen12): aceleración y Vulkan

- **Estado:** aprobada (**v11**, decidido por el mantenedor el 2026-11-15); dentro de la v11, **Intel primero**
  (hay hardware para probar: HD 4000 del L850 y Xe-LP del mini PC i3-1220P)
- **Prioridad:** alta
- **Alta:** 2026-11-15
- **Relacionadas:** Intel Gen3 3D (v8.3, D285), capa 3D común (D284, D285), 64 bits (106), AMD moderna (107, 108,
  110), varias pantallas (060), firmware (D073)

## Motivación
Las gráficas integradas Intel usan la RAM del sistema (una ventana de 256 MB por debajo de 4 GB), Intel publica
los manuales completos (PRM) de cada generación y hasta Gen9 no necesitan firmware: son el camino más corto a
una GPU moderna acelerada, también en 32 bits. Máquinas de prueba: **Toshiba L850** (HD 4000, Gen7 Ivy Bridge)
y **mini PC Core i3-1220P** (UHD Xe-LP, Gen12 Alder Lake-P).

## Propuesta (por niveles, como la 110)
1. **Pantalla nativa**: modos de la tarjeta (eDP/HDMI/DP, varias pantallas); en Gen12, firmware **DMC** (opcional).
2. **2D con el blitter** (BLT) por el anillo de órdenes.
3. **3D con la capa común** (Gen7 y Gen12; envío por *execlists*, sin GuC al principio).
4. **Vulkan** en 64 bits: Mesa **ANV** (Gen8+, Gen12 completo) y **hasvk** (Gen7), OpenGL con **iris**/**crocus**;
   firmware GuC/HuC opcional.
- Referencias: PRM de Intel, i915 de Linux (MIT, fichero a fichero), Mesa (MIT).

## Criterios de aceptación
- [ ] Niveles 1-3 en la HD 4000 (32 bits) y en el Xe-LP del i3-1220P
- [ ] Vulkan (vkcube) en el i3-1220P con la versión de 64 bits
- [ ] Decisión registrada en `docs/decisoes.md`
