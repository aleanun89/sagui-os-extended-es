---
name: pruebas-por-capa
description: "En las ramas de trabajo solo se pasan las pruebas de lo tocado; la batería completa, una vez en main tras mezclar"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 70ab6765-4570-4f0d-a587-96a40f1d4263
  modified: 2026-11-12T07:33:22.887Z
---

En las ramas de trabajo (copias `miniOS_master_os_*`), probar solo lo que afecta a lo tocado (p. ej. cambio de kernel/SMP: h133 + arranque + h05/h10/h28). La batería completa se pasa una sola vez, en main, después de mezclar.

**Versiones de arreglos X.Y.Z (Z ≥ 1, p. ej. v7.1.1):** ni siquiera en main se pasa la batería completa antes de publicar: solo las pruebas de lo afectado por los cambios más algunas generales (arranque, escritorio, shell, discos: p. ej. h01, h04, h05, h16, h38). La batería completa queda para las versiones X.Y (pedido del mantenedor, 2026-11-13, «para tardar menos»).

**Why:** el usuario lo pidió (2026-11-12) para no gastar horas repitiendo la regresión completa en cada rama; cada prueba QEMU tarda minutos y van de una en una.

**How to apply:** al encargar trabajo a subagentes, indicar en el brief la capa reducida de pruebas según lo tocado, y reservar la regresión completa para después del merge a main.
