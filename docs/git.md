# git en SaguiOS (D400)

`git` (`/apps/bin/git`) es la orden de control de versiones de SaguiOS: repositorios locales y remotos
por **HTTPS** y por **SSH**. Ayuda para el usuario: `ayuda git` ([`rootfs/ayuda/git.md`](../rootfs/ayuda/git.md)).

## Qué hace

`init`, `clone`, `fetch`, `pull`, `push`, `status` (formato corto `XY ruta`), `add`, `commit -m`, `log`,
`diff` (`--cached`, `--stat`, `--name-only`), `branch` (`-a`, `-d`), `checkout` (`-b`, `-- archivos`, ramas
solo remotas con seguimiento, HEAD desacoplado), `remote` (`add`, `remove`, `set-url`, `get-url`, `rename`,
`-v`) y `config` (`--global`, `--list`, `--unset`). Códigos de salida como git (0, 1, 128, 129).

No hay: editor (los mensajes van con `-m`), `rebase`, `stash`, `merge` suelto, `tag`, submódulos, hooks, proxies,
`.gitattributes` con filtros externos, protocolo `git://` con autenticación. Un `pull` con divergencia hace una
fusión si no hay conflictos; con conflictos deja el árbol marcado (`git add` + `git commit` lo termina).

## Diseño

- **No es el git original.** El git del proyecto Git es GPL-2.0-only y no puede enlazarse con la libc y la
  libsagui (GPL-3.0). Se usa **libgit2 1.9.1** (GPL-2.0 *con excepción de enlace*, compatible) y una orden propia
  (`userland/terceros/git/git.c`, GPL-3.0-only, escrita contra la API pública de libgit2; no se copia código del
  ejemplo `lg2`).
- **Fuentes fuera de git**, como TCC: `tools/git_fuentes.sh` baja el tarball de la etiqueta (SHA-256 fijo) a
  `userland/terceros/git/fuentes/` (sin tests, ejemplos, pcre, ntlmclient ni winhttp). `make GIT=0` lo salta;
  sin fuentes, el `Makefile` lo avisa y sigue. Va en `/apps/bin` (D379: pasa de 700 KB; el ramdisk no tiene sitio)
  y, como TCC, **no** en la variante `SEM_GPL` (enlaza con la libsagui GPL-3.0-only).
- **Se compila con la libc común** (`userland/libc`) sin tocar libgit2: `plataforma/git2_features.h` sustituye al
  de CMake. Sin hilos (`GIT_THREADS` no definido: un solo hilo), regex de la libc (`regcomp`), zlib de
  `deps/zlib` de libgit2 (la de la libc solo descomprime; el enlazado deja fuera `libc/zlib.o` como Freeciv),
  SHA-1 con detección de colisiones (sha1dc) y SHA-256 propios, `llhttp` para HTTP, `xdiff` para los diffs.
  Se añadió a la libc lo pequeño que faltaba: `getpwuid_r` (sin usuarios: nunca encuentra), `getpgid`, `getsid` y
  `_SC_GETPW_R_SIZE_MAX`. `mmap` (emulado), `rename`, `ftruncate`, `fsync`, `lstat` ya estaban.
- **HTTPS sin OpenSSL ni mbedTLS.** libgit2 permite registrar su propio flujo de red
  (`git_stream_register`): `plataforma/flujo_sagui.c` registra uno TCP (`conectar_tcp` de la libsagui, sin
  `select`/`poll`, que SaguiOS no tiene) y otro **TLS con la libtls propia** (`tls_novo` + `tls_conectar`, la misma
  de `curl`, `wget` y Cipó): TLS 1.2/1.3, ECDSA y RSA, verificación de la cadena y del nombre contra el almacén de
  certificados del sistema (`certificados`, `/etc/ssl/certs`). La verificación ocurre al conectar y un fallo da
  `GIT_ECERTIFICATE`. Como en git: `GIT_SSL_CAINFO` (un PEM con las únicas raíces válidas) y `GIT_SSL_NO_VERIFY`.
  El transporte HTTP, la autenticación básica (usuario y contraseña de la URL, de `GIT_USERNAME`/`GIT_PASSWORD`
  o preguntados) y el protocolo «smart» son los de libgit2.
- **SSH: el transporte «exec» sobre `/bin/ssh`** (el mismo que usa git en Linux con `GIT_SSH`). libgit2 trae dos
  transportes SSH: libssh2 (otra biblioteca de terceros con su propia criptografía y que habría que enlazar con
  OpenSSL/mbedTLS) y «exec», que lanza el programa `ssh` y habla por sus tuberías. Se eligió **exec** porque es lo
  más robusto: reutiliza entero el cliente ssh de SaguiOS (D317: claves de `/home/.ssh`, `known_hosts` con su
  política estricta, ed25519, chacha20-poly1305, modo estricto contra Terrapin), que ya está probado contra
  OpenSSH, y no duplica criptografía ni la política de claves. Lo único que faltaba era `git_process`, que en
  libgit2 usa `fork`+`exec` y aquí se sustituye por `plataforma/proceso.c` + `ponte.c` sobre `executar()`:
  se redirigen la entrada y la salida del propio proceso a dos tuberías, se lanza `/bin/ssh` y se restauran
  (lo que hace el sagsh en sus tuberías); como el hijo solo hereda los descriptores 0, 1 y 2 (D010), los otros
  extremos de las tuberías no se filtran y cerrar la escritura da el fin de archivo al `ssh`. La línea de
  órdenes de `GIT_SSH` / `core.sshCommand` se parte como un shell mínimo (comillas y `\`).
  `git clone ssh://usuario@host/ruta` y `usuario@host:ruta` funcionan. `ssh` pregunta la huella la primera vez
  (necesita un terminal): se acepta antes con un `ssh host` normal, o con
  `GIT_SSH="ssh -o StrictHostKeyChecking=accept-new"`.
- **Licencias de lo que entra** (la fila completa está en [`terceros.md`](terceros.md)): libgit2 GPL-2.0 con excepción de
  enlace; `deps/xdiff` LGPL-2.1-or-later; `deps/llhttp` MIT; `deps/zlib` licencia zlib; `sha1dc` MIT; `rfc6234`,
  `wildmatch` y `git_fs_path_basename_r` BSD; xoroshiro y partes de utf8.h, dominio público. Todas compatibles
  con la GPL v3. No entran PCRE (se usa la regex de la libc), ntlmclient, winhttp, openssl ni libssh2.

## Limitaciones conocidas

- El conjunto de órdenes es el de arriba; el formato de `status` y `log` es el propio (no idéntico al de git).
- Un solo hilo: cada transferencia bloquea la orden hasta acabar. Sin barra de progreso fina (cuatro avisos).
- Los dos servidores de la prueba corren en el anfitrión; contra GitHub o GitLab real hace falta red y, para HTTPS,
  un *token* (`GIT_USERNAME`/`GIT_PASSWORD`). SSH con claves RSA o ECDSA no (el cliente ssh es solo ed25519).
- Los repositorios grandes van lentos en TCG y con poca RAM (libgit2 mapea los paquetes con `mmap` emulado).

## Prueba

`tests/h402_git.py [pc|local|ssh|https]` (con KVM: `sg kvm -c "SAGUI_KVM=1 python3 tests/h402_git.py"`). Parte `pc`:
ELF, documentación, licencias. `local`: init, config, add, commit, log, diff, branch, checkout, remote, clone de
archivo. `ssh`: un servidor SSH escrito en la prueba (el de h317) que ante `git-upload-pack` y `git-receive-pack`
lanza el git del anfitrión sobre un repositorio desnudo; SaguiOS hace clone, commit, push, fetch y pull con su propia
clave, y el git del anfitrión comprueba `log`, `show` y `fsck`. `https`: `git http-backend` del anfitrión tras un
servidor TLS con una CA de prueba (sin la CA, error de certificado; con `certificados añadir`, clone, push y pull),
y lo mismo por HTTP plano.
