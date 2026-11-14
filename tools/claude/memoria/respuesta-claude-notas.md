---
name: respuesta-claude-notas
description: Registrar resultados de pruebas y respuestas con fecha y hora en ../notas-revisor/RESPUESTA_CLAUDE.md
metadata:
  node_type: memory
  type: feedback
  originSessionId: 70ab6765-4570-4f0d-a587-96a40f1d4263
  modified: 2026-11-13T09:21:52.790Z
---

El mantenedor (2026-11-13) pide que vaya escribiendo en `/home/alejandro/miniOS/notas-revisor/RESPUESTA_CLAUDE.md` mis respuestas, cada una con fecha y hora delante (`- [AAAA-MM-DD HH:MM] ...`), y sobre todo los resultados de las pruebas con este formato:
- `Test hNN para Release vX.Y[.Z]: <resultado>` (en main antes de publicar)
- `Test rama <rama> hNN: <resultado>` (en la copia de una rama)

**Why:** hay otro agente revisor (MiniMax) que toma notas en `../notas-revisor/` y deja encargos (`PEDIDO-AGENTE-*.md`); el fichero es el canal de vuelta y el registro de lo probado.

**How to apply:** tras cada tanda de pruebas, cada batería y cada respuesta a un pedido del revisor, añadir líneas (no reescribir lo anterior). Los encargos del revisor se comprueban contra el código antes de aplicarlos: varios tenían afirmaciones caducadas.
