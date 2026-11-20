# 139 - IA local: modelos de lenguaje dentro de SaguiOS

- **Estado:** fase de texto hecha en v8.5.4.4 (rama `v8.5.4.4-ia-local`, D380); fase 2 (descargador, herramientas, visión) en v8.5.5.1; pesos por ventanas, pendiente
- **Prioridad:** alta
- **Alta:** 2026-11-17

## Problema / motivación

Los agentes de IA de SaguiOS (`ia`, `chatia`, Pluma, Macaco, Correo) necesitan un servidor fuera del equipo. Con
modelos pequeños actuales (LFM3 1.7B de Liquid AI, ~1 GB en Q4_K_M) un PC de hace 10-15 años con 2 GB puede
ejecutarlos él mismo, sin red ni nube.

## Propuesta

Motor propio en C (`userland/llm/`, D380) que lee GGUF y ejecuta LFM3 en la CPU (C/x87, SSE2, SSSE3 por CPUID), con
caché K/V en Q8_0 y contexto de 12k a 48k (4k-48k sin SSSE3); el programa `llm` (`/apps/bin`) con chat, `-p`, `--info`,
`--modelos`/`--activar`; logo «IA Powered» en el escritorio cuando el equipo puede (más de 1,8 GB y un modelo).
Los modelos nunca van dentro del sistema: `/home/compartido/modelos` o `~/modelos`. Ver `docs/llm.md`.

Fases:
1. Motor de texto LFM3 con logits correctos contra llama.cpp — **hecho**.
2. Q4_0 — **hecho** (kernels C/SSE2/SSSE3; falta probarlo con un modelo Q4_0 real).
3. Pesos por ventanas (PAE) para 48k con cualquier modelo (kernel, con revisión de seguridad) — pendiente.
4. Descargador en Configuración > IA (Hugging Face, progreso, reanudación, SHA-256, modelo activo) — **hecho** en la v8.5.5.1 (`ia descargar`, D380 fase 2).
5. Logo «IA Powered» y la regla de 1,8 GB — **hecho**.
6. Servidor local compatible con OpenAI (`llm --servidor`) para que los agentes usen el modelo local — **hecho**; con herramientas (`tools`/`tool_calls`) desde la v8.5.5.1.
7. VL: codificador de visión (`mmproj`) y `--imagen` — **hecho** en la v8.5.5.1 (LFM3-VL; imágenes también en el servidor).
8. Perfil para equipos antiguos: hilos de serie (uno por CPU) y estimación en `--info` — **hecho**; bloques para la L2 — pendiente (con SSE la lectura está limitada por el cálculo, ver docs/llm.md).

## Criterios de aceptación

- [x] Los mismos tokens que llama.cpp con el modelo real en el PC (`tools/llm_comparar.py`)
- [x] `llm` funciona en SaguiOS con el modelo sintético y da los mismos tokens que en el PC por cada ruta (h388)
- [x] Error claro si el modelo con ese contexto no cabe, y con 1,8 GB o menos de RAM (h388)
- [x] Logo «IA Powered» con 2 GB y sin él con 1 GB (h388, capturas)
- [x] Medida con el modelo real en QEMU/KVM a 12k: 6,6 tok/s de lectura y 5,6 de generación (SSE2, un núcleo; docs/llm.md)
- [x] Servidor local y `ia` usándolo sin cambios (h388); herramientas («tools») desde la v8.5.5.1 (h412)
- [x] Descargador y VL (h412)
- [ ] Pesos por ventanas
- [x] Test automático en `tests/` que lo comprueba (`tests/h388_llm.py`)
- [x] Decisión registrada en `docs/decisoes.md` (D380)

## Notas

- El modelo real (LFM Open License 1.0) no va a git, ni a las imágenes, ni al repositorio público.
- La referencia (llama.cpp) solo se compila en el PC, fuera de git, para comparar.
