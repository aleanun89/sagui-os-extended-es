# openrsync en SaguiOS (D314)

Origen: https://github.com/kristapsdz/openrsync, etiqueta `VERSION_0_5_0`
(commit `22b37c9ae3cf63e5788bfe28dc4baa0ea33abb6f`, 2019-02-12), licencia **ISC**
(`fuentes/LICENSE.md`). Descarga y verificación: `tools/openrsync_fuentes.sh --comprobar`
(SHA-256 del tarball fijado en el guion).

Se usa solo el emisor y el receptor del protocolo 27 (modo `rsync --server`, que lanza el
`sshd` de SaguiOS). No se compilan `main.c`, `child.c`, `client.c`, `fargs.c` ni `socket.c`
(`fork`, `exec`, `socketpair`: SaguiOS no los tiene). La adaptación a la libc de SaguiOS
(`poll`, `fts`, `hsearch`, las funciones `...at`) está en `compat/`, que es código nuestro.

## Parches sobre el upstream (solo estos cuatro ficheros)

- `extern.h`: campo `preserve_uids` en `struct opts` (`-o`).
- `flist.c`: soporte de `-o` en el protocolo (el `rsync` del PC siempre lo manda con `-a`):
  el uid va antes del gid en cada entrada y hay una lista de uid tras la de ficheros (vacía al
  enviar, ignorada al recibir). Como SaguiOS no tiene usuarios ni grupos, `flist_copy_stat`
  envía siempre uid y gid 0.
- `receiver.c`: la carpeta raíz del destino es un descriptor simulado (`rs_abrir_raiz`) en lugar
  de abrir la carpeta (SaguiOS no abre carpetas como ficheros); sirve también en `--dry-run`.

Para el resto (`poll`, `pledge`, `unveil`...) no se toca el upstream: `compat/rsync_sagui.h` se
incluye a la fuerza y redefine los nombres.
- `session.c`: el emisor servidor manda **siempre** las estadísticas finales y el cliente receptor las lee
  siempre (el upstream solo con `-v`: el `rsync` 3.x del PC sin `-v` se quedaba esperándolas); solo se
  imprimen con `-v`.

## Cliente por ssh y modos (v8.4.2)

Sin tocar el upstream: `cliente_rsync.c` (propio) hace lo de `client.c`/`fargs.c` con `executar()` y dos tuberías;
`compat_sagui.c` añade `rs_read`/`rs_write` y un `poll` que vacían la salida de `ssh` a un búfer propio (tuberías de
512 bytes del kernel sin interbloqueo), y conecta los modos de D282 (`stat`, `fchmodat`, modo al crear). Ver D314 (ampliación).
