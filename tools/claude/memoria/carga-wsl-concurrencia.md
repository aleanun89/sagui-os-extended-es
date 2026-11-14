---
name: carga-wsl-concurrencia
description: "WSL con 16 núcleos/36 GB desde el 2026-11-13 (-j 4, audio de una en una); antes el PC (Ryzen 9 5900X) se reiniciaba por un fallo de hardware de un núcleo (WHEA 18 Cache Hierarchy Error, APIC 4/5); límites de concurrencia y protección de git"
metadata:
  node_type: memory
  type: project
  originSessionId: 70ab6765-4570-4f0d-a587-96a40f1d4263
  modified: 2026-11-12T19:45:05.699Z
---

El 2026-11-12 el PC del mantenedor se reinició 3 veces (19:24, 20:23, 20:41). El visor de eventos dio **WHEA-Logger 18, Machine Check Exception, Cache Hierarchy Error, APIC 4 y 5** = los dos hilos del **núcleo físico 2** del 5900X. Causa probable: Curve Optimizer/PBO demasiado agresivo (el tercero ocurrió casi sin carga, con boost alto) o núcleo degradado. **No es el código de SaguiOS ni KVM**; el trabajo continuo solo lo destapa. Se le recomendó: BIOS sin PBO/CO (o ese núcleo a 0/positivo), actualizar BIOS, CoreCycler/OCCT, garantía si falla a valores por defecto.

**Why:** cada reinicio corta agentes y dejó objetos de git vacíos (main apuntó a un commit vacío; se recuperó volviendo a `28383a5` y guardando los vacíos aparte).

**Actualización 2026-11-13:** el mantenedor subió WSL a **16 procesadores lógicos, ~36 GB de RAM** y autorizó más pruebas a la vez: batería con `-j 4`, 3-4 agentes compilando (`make -j2` cada uno). **Las pruebas de audio, solo una cada vez** (en `tests/rodar_paralelo.py` ya se detectan y se serializan solas; al lanzar pruebas sueltas a mano, no juntar dos de audio). Sigue valiendo: nunca la batería completa con agentes compilando.

**How to apply:**
- Con 16 núcleos: hasta `-j 4` y 3-4 agentes; si vuelve a reiniciarse, bajar otra vez a 2 agentes y sospechar del núcleo 2 (WHEA 18).
- Commitear a menudo; en el repo está `core.fsync=committed` (git 2.43) para que un reinicio no deje objetos vacíos.
- Tras un reinicio: `uptime`, `git fsck --full`, buscar objetos vacíos (`find .git/objects -type f -empty`), revisar `git worktree list` y refs vacías, **y objetos de compilación vacíos** (`find build* -type f -size 0 -name '*.o'` en cada copia: `make` los da por buenos y luego falla el enlace; el 2026-11-12 hubo 88 en `build-sem-gpl`). Para saber la causa: pedirle al mantenedor `Get-WinEvent` del registro System (IDs 41, 1001, 6008, WHEA 18/19); desde WSL no se pueden ejecutar `.exe` (interop desactivado).

Relacionado: [[pruebas-por-capa]]
