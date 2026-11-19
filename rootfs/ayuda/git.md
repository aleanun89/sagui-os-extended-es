# git: control de versiones dentro de SaguiOS

`git` (`/apps/bin/git`) guarda el historial de tus proyectos y los sincroniza con otras máquinas por
**HTTPS** y por **SSH**. No es el git original (su licencia GPL-2.0-only no se puede enlazar con el
sistema): es una orden propia sobre **libgit2**. Hace lo habitual del día a día; lo raro (rebase,
stash, submódulos, hooks) no está.

## Lo básico

    git config --global user.name "Tu Nombre"      tu identidad (una vez)
    git config --global user.email tu@correo
    git init proyecto                              repositorio nuevo (rama main)
    git add .                                      prepara todo (o: git add archivo)
    git commit -m "primer commit"                  confirma (sin editor: siempre -m)
    git status                                     estado corto: XY ruta
    git log --oneline                              historial
    git diff                                       cambios sin preparar (--cached: los preparados, --stat: resumen)

`status` usa el formato corto: `??` archivo nuevo sin seguir, `A ` añadido, ` M` modificado sin preparar,
`M ` preparado, `D ` borrado. La primera línea es `## rama`.

## Ramas

    git branch                  lista (la actual lleva *)
    git branch nueva            crea una rama
    git checkout nueva          cambia de rama (también: git checkout -b nueva)
    git checkout -- archivo     deshace los cambios de un archivo
    git branch -d nueva         borra una rama

## Remotos: HTTPS y SSH

    git clone https://servidor/usuario/repo.git
    git clone ssh://usuario@servidor/ruta/repo.git
    git clone usuario@servidor:ruta/repo.git
    git remote add origin URL          git remote -v
    git fetch                          trae lo nuevo sin tocar tus archivos
    git pull                           fetch + avance rápido o fusión sin conflictos
    git push -u origin main            envía la rama y la deja siguiendo a origin/main

- **HTTPS** usa el TLS de SaguiOS y las raíces del almacén de certificados (`certificados`). Un
  certificado propio se añade con `certificados añadir ca.pem`, o solo para una orden con
  `GIT_SSL_CAINFO=ca.pem git clone ...`. `GIT_SSL_NO_VERIFY=1` no verifica (solo para pruebas).
  Si el servidor pide usuario y contraseña, `git` los pregunta; o `GIT_USERNAME` y `GIT_PASSWORD`
  (con GitHub, un *token*). Los proxies no están soportados.
- **SSH** lanza `/bin/ssh` y habla el protocolo de git por sus tuberías: usa tu clave de
  `/home/.ssh/id_ed25519` (`ssh-keygen`) y `/home/.ssh/known_hosts`. La primera vez, `ssh` enseña la
  huella del servidor y pregunta; para no preguntar: `GIT_SSH="ssh -o StrictHostKeyChecking=accept-new"`
  o `git config core.sshCommand ...`. Solo claves ed25519 y el servidor del otro lado necesita `git`.

## Otras órdenes

    git remote add|remove|set-url|get-url|rename ...
    git config [--global] clave [valor]    git config --list    git config --unset clave
    git -C carpeta orden                   ejecuta la orden en otra carpeta

Los textos de los commits se guardan en UTF-8. Códigos de salida como en git (0 bien, 1 fallo, 128 error
grave, 129 uso).
