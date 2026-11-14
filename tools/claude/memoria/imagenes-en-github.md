---
name: imagenes-en-github
description: En GitHub solo se suben las dos imágenes básicas de cada versión; el resto se genera con make variantes en el equipo
metadata:
  node_type: memory
  type: project
  originSessionId: 70ab6765-4570-4f0d-a587-96a40f1d4263
  modified: 2026-11-13T13:19:01.842Z
---

Decisión del mantenedor (2026-11-13, desde la v7.4): de cada versión solo van a git `entrega/vX/saguios.img` y `saguios-smp.img` (más `SHA256SUMS.txt` y `VERSION.txt`), con la partición de juegos del tamaño justo. Las persistentes, la de particiones, la sin GPL, la ISO y el .vdi se generan con `make variantes` en el equipo de quien descarga. La partición SAGUI-JOGOS es de 800 MB y de lectura y escritura (en `/jogos`, una carpeta por juego) en las imágenes generadas y el instalador. La carpeta `input_share/` (en `.gitignore`) lleva el contenido propio del que compila (p. ej. `input_share/games/Jazz/`, el Jazz Jackrabbit completo del mantenedor) y entra en las imágenes generadas.

**Why:** GitHub rechaza ficheros de más de 100 MB y el repositorio no debe crecer cientos de MB por versión; lo que no se puede distribuir (juegos completos) no debe subirse nunca.

**How to apply:** al publicar, `git add` solo las dos básicas y las hojas; nunca imágenes de 800 MB ni nada de `input_share/`. Está en el README («Qué imágenes hay en el repositorio») y en `docs/input_share.md`.

Relacionado: [[base-vs-apps-particiones]], [[subir-ramas-github]]
