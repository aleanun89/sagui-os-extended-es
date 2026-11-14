---
name: no-matar-qemu-usuario
description: "Antes de matar QEMU colgados, comprobar que son de las pruebas; nunca matar el QEMU que el mantenedor lanzó con run_in_qemu.sh"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 70ab6765-4570-4f0d-a587-96a40f1d4263
  modified: 2026-11-13T12:38:36.960Z
---

Al limpiar QEMU colgados, matar solo los de las pruebas: los que en su línea de órdenes llevan `build/paralelo/`, `build/aisl_*`, `build/testes/` o `-serial file:`. **Nunca** uno lanzado por `run_in_qemu.sh` (lleva `-name SaguiOS`, `build/saguios-opciones.img` o `-audiodev pa,...` de WSLg): es la sesión del mantenedor.

**Why:** el 2026-11-13 maté con `kill` el QEMU de 45 minutos en el que el mantenedor estaba usando SaguiOS (me había mandado una captura de él), creyendo que era un resto de las pruebas.

**How to apply:** antes de `kill`, mirar `tr '\0' ' ' < /proc/PID/cmdline`; filtrar con `pgrep -f "build/(paralelo|aisl|testes)"` en vez de por el ejecutable de QEMU. Si no está claro de quién es, preguntar o dejarlo.

Relacionado: [[carga-wsl-concurrencia]]
