# Traspaso para el asistente (Claude Code) en otro PC

Esta carpeta lleva lo que el asistente usaba en el PC original y que no estaba en git:

- `memoria/`: sus notas de trabajo (políticas del mantenedor, límites de carga, máquinas de
  prueba, planes). En el PC nuevo se copian a la carpeta de memoria del proyecto de Claude Code
  (`~/.claude/projects/<ruta-del-repo-con-guiones>/memory/`, p. ej.
  `~/.claude/projects/-home-USUARIO-miniOS-miniOS-master/memory/`), o basta con pedirle que
  lea este directorio al empezar.
- `nota.sh "texto"`: añade una línea con fecha y hora a `../notas-revisor/RESPUESTA_CLAUDE.md`
  (resultados de pruebas: `Test hNN para Release vX.Y.Z: ...` o `Test rama <rama> hNN: ...`).
- `resolver_docs.py fichero.md...`: resuelve conflictos de fusión en documentación
  conservando las dos partes.

Reglas principales (detalle en `memoria/` y en `AGENTS.md`):

- En GitHub solo las dos imágenes básicas por versión (`saguios.img`, `saguios-smp.img`,
  `SHA256SUMS.txt`, `VERSION.txt`); el resto con `make variantes`. Nunca `input_share/`.
- Versiones X.Y.Z: solo pruebas de lo afectado y generales; X.Y: batería completa en main sin
  agentes trabajando.
- Cada característica en su copia `../miniOS_master_os_<x>` (`tools/criar-os.sh`); los agentes
  no hacen push, se suben todas las ramas `v*` como copia.
- Pruebas de audio, una cada vez (`tests/rodar_paralelo.py` ya las serializa).
- Nunca matar el QEMU de `run_in_qemu.sh` (la sesión del mantenedor); solo los de pruebas.
- En el PC nuevo hace falta `tools/entorno-local.sh` (saguitools) y, para los juegos completos,
  su propio `input_share/` (fuera de git).
