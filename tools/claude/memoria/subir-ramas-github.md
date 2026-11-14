---
name: subir-ramas-github
description: "Subir a GitHub no solo main sino también las ramas de trabajo de los agentes, como copia de seguridad"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 70ab6765-4570-4f0d-a587-96a40f1d4263
  modified: 2026-11-13T08:15:05.814Z
---

Además de `main` y las etiquetas, subir a GitHub (`git push origin <ramas>`) las ramas de trabajo `v*` de los agentes en cuanto tengan commits, aunque no estén fusionadas. Los agentes no hacen push (se lo prohíbo en el encargo); lo hago yo.

**Why:** el mantenedor preguntó (2026-11-13) «¿te estás acordando de subir a GitHub por si acaso?»: había 25 ramas solo en local; con los reinicios del PC ([[carga-wsl-concurrencia]]) se puede perder trabajo.

**How to apply:** tras cada informe de un agente, `git push -q origin main <su rama>`; de vez en cuando comprobar con `git fetch` que no queda ninguna rama `v*` por subir.
