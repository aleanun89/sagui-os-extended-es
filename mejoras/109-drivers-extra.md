# 109 - Drivers extra: interfaz estable y repositorio aparte que el usuario compila

- **Estado:** aprobada (**v9.x**, decidido por el mantenedor el 2026-11-15)
- **Prioridad:** media
- **Alta:** 2026-11-15
- **Relacionadas:** licencia (D258, D259), r8169 (099), publicación (D260), GPU (107)

## Motivación
Hay código útil que no puede ir en SaguiOS (GPL v3): p. ej. lo «solo GPL v2» de Linux (r8169). Idea del
mantenedor: ponerlo en **otro repositorio**, con su licencia, que **el usuario descarga y compila** en su
propia imagen (uso privado, no se distribuye la mezcla).

## Propuesta
- **Interfaz estable de drivers** (`kernel/inc/driver_api.h`): lo que un driver puede usar del kernel
  (PCI, memoria, DMA, interrupciones, framebuffer, capa 2D/3D, red, bloques). **Código nuevo en
  GPL-2.0-or-later**, sin código heredado de Eli, para que un driver GPL-2.0-only pueda usarla.
  Versionada; compatibilidad entre versiones menores.
- **Módulos de compilación** (no cargables): `make DRIVERS_EXTRA=URL[@commit]` descarga el repositorio,
  comprueba la suma, y compila sus drivers dentro del kernel de esa imagen; `gerar_versao.sh` lo admite
  solo en lo privado (nunca en `make variantes`) y la auditoría de licencias lo vigila.
- Primer inquilino: el inicio moderno del r8169 (hoy solo en lo privado).

## Criterios de aceptación
- [ ] `driver_api.h` documentada y usada por al menos un driver interno
- [ ] `DRIVERS_EXTRA` funcionando con un repositorio de ejemplo
- [ ] Decisión registrada en `docs/decisoes.md`
