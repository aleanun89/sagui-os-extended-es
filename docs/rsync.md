# rsync en SaguiOS (D314, mejora 120)

`rsync` copia y sincroniza carpetas mandando solo lo que cambió. En SaguiOS (v8.4) hay dos usos:

1. **El PC (o cualquier Linux, macOS, WSL) se conecta a SaguiOS por SSH** y sube o baja carpetas con su
   propio `rsync`. SaguiOS hace de servidor: no abre ningún puerto nuevo ni lanza ningún demonio rsync,
   solo el `sshd` de siempre ([D250](decisoes.md)), que autentica con clave pública y ejecuta `rsync --server`.
2. **Copia local** dentro de SaguiOS (por ejemplo de `/home` a un pendrive): `rsync -a /home/ /usb/copia/`.

El programa es `/apps/bin/rsync` (partición SAGUI-APPS, en el `PATH` de las sesiones de `sshd`). Está
basado en **openrsync** de OpenBSD (licencia ISC, protocolo 27; fila en [`terceros.md`](terceros.md)).
Los parches y lo que se adaptó están en `userland/terceros/openrsync/PARCHES.md`.

## Preparar el SSH

Hace falta el servidor SSH activo y tu clave pública en SaguiOS (ver la guía de SSH, D250):

```sh
# en SaguiOS (terminal), una vez
servicio activar sshd
servicio iniciar sshd
# la clave pública del PC va en /home/.ssh/authorized_keys (o /etc/ssh/authorized_keys)
```

## Copia de seguridad de `/home` de SaguiOS al PC

Desde el PC (`IP` es la de SaguiOS; el usuario da igual, es siempre el mismo):

```sh
rsync -av -e ssh root@IP:/home/ ~/copia-saguios/
```

La primera vez baja todo; las siguientes solo mandan lo que cambió dentro de cada fichero (el delta
de rsync). Con `--stats` se ve cuánto se mandó de verdad:

```sh
rsync -av --stats -e ssh root@IP:/home/ ~/copia-saguios/
```

## Subir una carpeta del PC a SaguiOS

```sh
rsync -av -e ssh proyecto/ root@IP:/home/proyecto/     # el contenido de proyecto/ (barra final)
rsync -av -e ssh proyecto  root@IP:/home/              # la carpeta proyecto dentro de /home
```

Con `--delete` el destino queda igual que el origen (borra lo que sobra):

```sh
rsync -av --delete -e ssh proyecto/ root@IP:/home/proyecto/
```

Con un puerto distinto o una clave concreta: `-e "ssh -p 2222 -i ~/.ssh/clave_saguios"`.
Para probar sin tocar nada, `-n` (`--dry-run`).

## Copia local en SaguiOS

```sh
rsync -a /home/ /usb/copia/                 # de /home a un pendrive (copia el contenido)
rsync -av --delete /home/ /usb/copia/       # y deja el pendrive igual (borra lo que sobra)
rsync -a --dry-run -v /home/ /usb/copia/    # solo muestra qué haría
rsync -a --progress --stats /home/ /usb/copia/
```

Las barras finales son las de rsync: `origen/` copia el **contenido** de la carpeta y `origen` copia la
carpeta misma dentro del destino. Un fichero se copia si cambió; como los sistemas de archivos
escribibles de SaguiOS no guardan fechas, se comparan tamaño y contenido (lo igual no se reescribe).
Opciones: `-a -r -v -n/--dry-run --delete --progress --stats -c -q`; `-t -p -l -g -o` se aceptan.

## Desde SaguiOS: subir y bajar por ssh (v8.4.2)

```sh
rsync -av carpeta/ usuario@maquina:ruta/          # subir
rsync -av usuario@maquina:ruta/ carpeta/          # bajar
rsync -av --delete -e "ssh -p 2222 -i /home/.ssh/otra" carpeta/ usuario@maquina:ruta/
```

Hace falta el servidor SSH y `rsync` en la otra máquina (otro Linux o SaguiOS: `/apps/bin/rsync` está en el
`PATH` de las sesiones de `sshd`; con otra ruta, `--rsync-path=/ruta/rsync`), y la clave pública de
`/home/.ssh/id_ed25519.pub` (`ssh-keygen`) en el `authorized_keys` de allí. La primera vez, conecta a mano
(`ssh usuario@maquina`) para guardar la huella en `known_hosts`, o pasa
`-e "ssh -o StrictHostKeyChecking=accept-new"`: durante la copia `ssh` no puede preguntar. `-e` solo admite el
`ssh` de SaguiOS y sus opciones (`-p`, `-i`, `-o`, `-l`). No se mezclan origen y destino remotos.

Los **modos** (`rwx`) se copian con `-a`/`-p` en las tres direcciones y en la copia local (D282); los dueños no
(un uid de otra máquina no vale aquí) y las fechas no se pueden escribir todavía.

## Qué funciona y qué no (v8.4)

- **Funciona:** servidor por SSH (subir y bajar, con delta y `--delete`), copia local.
- **Fechas, permisos, dueños:** los sistemas de archivos de SaguiOS no guardan fechas de modificación
  (solo los de solo lectura); los permisos sí desde la v8.4.1 (D282; se copian con `-a`/`-p`); el dueño no se aplica. Por eso un fichero que ya
  existe se trata como «posiblemente distinto»: se recalculan sus sumas y solo se manda lo que
  difiere. Los ficheros bajados a SaguiOS... y los que SaguiOS manda llevan una fecha sin
  significado (el rsync del PC con `-t` puede ponerla); no se pierde contenido.
- **Enlaces simbólicos, dispositivos:** SaguiOS no los tiene; se omiten.
- **Compresión (`-z`):** no está soportada (el servidor lo dice y termina; no uses `-z`).
- **Ficheros grandes:** el delta carga el fichero en memoria (`mmap` emulado): decenas de MB van bien;
  cientos, no.
- **Cliente remoto en SaguiOS** (v8.4.2): `rsync -av carpeta/ usuario@maquina:ruta/` y al revés, con
  el `ssh` de SaguiOS (D317); ver «Desde SaguiOS» abajo. **Demonios `rsync://`**: no (SaguiOS no abre
  el 873; se rechaza con un mensaje).
- **Seguridad:** el servidor solo corre bajo el `sshd` (clave pública, red local, límites de D250).
  No hay puerto 873 ni `rsyncd`.

## Para quien toque el código

- Fuentes: `userland/terceros/openrsync/` (`fuentes/` es openrsync con tres parches; `compat/` es la
  libc que le falta, nuestra; `principal_rsync.c` y `local_rsync.c` son nuestros).
  `tools/openrsync_fuentes.sh --comprobar` verifica que las fuentes son las del upstream salvo los
  parches. Compila con `make` (`RSYNC=0` lo deja fuera).
- SaguiOS no tiene `fork`/`exec`: del main de openrsync solo queda el modo `--server`, que no necesita
  ninguno (el `sshd` lanza `rsync` con `executar()` y tuberías). El `poll` de openrsync se emula con
  `disponivel()` sobre las tuberías.
- Prueba: `python3 tests/h314_rsync.py` (el cliente por ssh contra el `sshd` de SaguiOS no necesita nada del anfitrión; la parte del servidor necesita `rsync` y `ssh`, y si no, «no aplica»).
