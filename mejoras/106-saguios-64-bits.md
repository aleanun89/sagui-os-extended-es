# 106 - SaguiOS de 64 bits (x86-64)

- **Estado:** aprobada (**v10.0**, decidido por el mantenedor el 2026-11-15)
- **Prioridad:** alta (requisito para GPU modernas y Vulkan)
- **Alta:** 2026-11-15
- **Relacionadas:** UEFI x64 (D264), GPU modernas (107, 108), memoria (069), SMP (017), compiladores (003, 040)

## Motivación
Las GPU modernas (AMD GCN/RDNA) tienen 4-16 GB de memoria de vídeo y registros por encima de 4 GB, y sus
drivers (amdgpu, Mesa RADV) suponen un sistema de 64 bits. El kernel actual, de 32 bits sin PAE, solo ve
unos 2 GB útiles. Pasar a x86-64 es el requisito para la línea de AMD moderna con Vulkan.

## Propuesta
- Kernel en **modo largo** (x86-64): paginación de 4 niveles, todo el mapa de memoria, BAR de 64 bits,
  IOMMU más adelante; el cargador UEFI x64 (D264) salta ya en 64 bits; por BIOS, paso a modo largo en la etapa 2.
- **libc, libsagui y programas** recompilados para x86-64 (ABI propia sencilla, System V para el C).
- Compatibilidad: ejecutar los programas de 32 bits existentes en modo compatibilidad si sale razonable.
- El `cc` y el Pascal propios generan x86-64.
- Imágenes de 32 bits para los PC antiguos mientras se mantenga (decidir).

## Criterios de aceptación
- [ ] Arranca en x86-64 por UEFI y BIOS, con escritorio, red, discos, USB y sonido
- [ ] Batería completa (cambio de versión mayor)
- [ ] Decisión registrada en `docs/decisoes.md`
