# 001 - Interfaz y documentación en español

- **Estado:** hecha
- **Prioridad:** alta
- **Alta:** 2026-11-06

## Problema / motivación

SaguiOS estaba entero en portugués de Brasil. Queremos usarlo y mejorarlo en español.

## Propuesta

Alcance acordado:

- **Se traduce:** todo lo que se ve en pantalla (kernel, boot, shell, programas, ventanas,
  Sagui 3D), el contenido de `rootfs/` y la documentación (`README.md`, `prd.md`, `docs/`,
  `status.md`, dashboard).
- **Se queda en portugués:** identificadores, comentarios del código, nombres de archivo y
  carpeta, nombres de comandos y el log del puerto serie (`klog`, `[TESTE]`...), porque los
  tests lo analizan.
- No se traduce `docs/regressao-final.txt` (es un log de ejecución).

## Criterios de aceptación

- [x] Textos del kernel y del boot en español
- [x] Textos de userland, Sagui 3D y rootfs en español
- [x] Documentación en español
- [x] Tests actualizados y `python3 tests/rodar_todos.py` en verde (38/38)
- [x] Decisión registrada en `docs/decisoes.md` (D031)

## Notas

- Glosario: Arquivos→Archivos, pasta→carpeta, Área de trabalho→Escritorio, janela→ventana,
  Sagui Pega Banana→Sagui Atrapa Bananas, macaco-prego→mono capuchino, estilingue→tirachinas,
  porteira→verja.
- Pendiente de decidir: ¿renombrar también archivos visibles (`leia-me.txt`, `/historia`,
  `/jogos`) y comandos (`sair`...)? Implica cambiar los tests.
- Quedan en portugués, a propósito, algunas citas literales de pantalla dentro de
  `docs/erros.md` y `docs/diario.md` (son registro histórico), el log del puerto serie y
  `docs/regressao-final.txt`.
- `tools/confirmar-hardware.sh` sigue añadiendo entradas a `docs/diario.md` en portugués.
