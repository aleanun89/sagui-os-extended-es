# 114 - OpenGL con Mesa (Gallium): r300, r600, i915, crocus y softpipe, con EGL y SDL 2

- **Estado:** aprobada (**v8.6**, decidido por el mantenedor el 2026-11-15)
- **Prioridad:** alta
- **Alta:** 2026-11-15
- **Relacionadas:** capa 3D común gpu3d (D284, D285, D287), SaguiGL, SDL 2 por GPU (D288), GPU modernas (107-111), vídeo por GPU (115)

## Motivación
Dar a SaguiOS la API 3D que usan casi todas las aplicaciones de 1998-2015 (OpenGL 1.x-2.1 de compatibilidad, más
OpenGL ES 2.0), para portar programas con 3D acelerado sin reescribirlos, y aprovechar los drivers de Mesa (MIT)
para las GPU antiguas de las máquinas de prueba (r300: X1400 del Amilo; r600: dv7 y L635; i915: Intel Gen3; crocus: HD 4000).

## Propuesta
- **Mesa en un repositorio aparte** (decisión del mantenedor): un fork **recortado** a lo necesario, donde viven
  también nuestros cambios (winsys, plataforma EGL), con su licencia MIT; la compilación de SaguiOS lo **descarga
  y compila** como NetSurf (`tools/mesa_fuentes.sh`, commit fijo y suma). Se queda: núcleo de OpenGL, compilador
  GLSL (NIR/TGSI), Gallium, softpipe, r300, r600, i915 (y crocus después), EGL. Fuera: LLVM/llvmpipe, Vulkan,
  radeonsi/iris (v11), drivers ARM/móviles, GLX/X11/DRI. **VA-API no se descarta** (ver mejora 115).
- **Winsys de SaguiOS sobre gpu3d** (búferes, envío de órdenes, vallas) para los drivers de GPU; **EGL** con una
  plataforma de SaguiOS (ventanas); **SDL 2** con contextos OpenGL (`SDL_GL_*`).
- Fases: (1) softpipe + EGL + SDL 2 y una demo (glxgears o similar) y algún port 3D (ioquake3…) — Sonnet;
  (2) drivers de GPU sobre el winsys (r600 primero: dv7 y L635) — Opus o especialista-drivers (el mantenedor elige
  el modelo); revisión con Opus.
- SaguiGL se mantiene (3D ligero) o pasa a ser una capa sobre OpenGL.

## Criterios de aceptación
- [ ] OpenGL 2.1 por software (softpipe) con EGL y SDL 2; una demo y un port 3D funcionando
- [ ] r600 (y r300/i915) por la GPU en hardware real
- [ ] Repositorio aparte de Mesa y descarga reproducible
- [ ] Test automático en `tests/`; licencias en `docs/terceros.md`; decisión en `docs/decisoes.md`
