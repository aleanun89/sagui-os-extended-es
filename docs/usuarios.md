# Usuarios y administrador (D255, D282 y D340, mejora 098)

SaguiOS tiene dos cuentas de persona y una de servicio:

| Cuenta     | uid  | Para qué |
|------------|------|----------|
| `root`     | 0    | El administrador. Tiene contraseña (o la tendrá la primera vez que se use). |
| `sagui`    | 1000 | El usuario del escritorio. Entra solo al arrancar: el escritorio, el Terminal y todo lo que se abre desde el menú son `sagui`. |
| `servicio` | 100  | Servicios que no necesitan ser root ni escribir en `/home` (`dnsbloqueo`). `litemonkey`, que guarda sus datos en `/home`, corre como `sagui` (D282). |

`id` y `whoami` dicen con qué cuenta corre cada programa.

## Qué puede hacer `sagui` y qué no

Todo lo normal: programas, ventanas, sus archivos (los de `/home`, `/tmp`, los
pendrives; ver «Permisos de archivos»), red, sonido, imprimir capturas, cambiar el
fondo, el teclado o la resolución, apagar y reiniciar desde el escritorio, y **conectar y
desconectar el Wi-Fi** (clic en el icono Wi-Fi de la bandeja, o `wifi conectar`; D354; solo la sesión
local: un servicio o una sesión SSH que no es de root no puede). Las redes recordadas son de cada
usuario (su `/home/.config/wifi.conf`).

Hace falta ser administrador para: montar y desmontar discos, particionar,
formatear e instalar, leer o escribir discos en crudo, configurar la red, el
DNS y el bloqueo de publicidad, manejar los servicios (`servicio
iniciar|parar|activar...`; consultar sí se puede), la hora y la zona del
sistema, el acelerador 2D, la frecuencia de la CPU, el ACPI, el acceso directo
al hardware (BAR de PCI, verbos del códec de sonido), terminar programas de
otra cuenta, y escribir en `/etc` y `/bin`. La tabla completa está en
`docs/decisoes.md` (D255) y en `kernel/nucleo/permisos.c`.

Un programa que lo intenta sin serlo recibe «operación no permitida (hace
falta ser administrador: usa «admin orden»)».

## Varias cuentas (D340)

Además de `sagui` se pueden crear más cuentas de persona en **Configuración > Usuarios**
(`configuracion --usuarios`): «Nueva cuenta...», «Cambiar la contraseña...», «Cambiar el nombre...»,
«Administrador: sí / no» y «Borrar la cuenta...». Ver la lista no pide nada; cada cambio pide la
contraseña de administrador con el diálogo del sistema.

- **Inicio de sesión:** si solo hay una cuenta y no tiene contraseña, el escritorio entra solo, como
  siempre. Si hay más, o la única tiene contraseña, al arrancar aparece la pantalla «Iniciar sesión»:
  eliges la cuenta (flechas o ratón), escribes la contraseña e Intro. Tiene también Apagar y Reiniciar.
  La pinta el propio sistema (como el diálogo de permisos): ningún programa ve la contraseña. Tras 5
  fallos, un minuto de espera.
- **Cerrar sesión y cambiar de usuario:** en el menú Inicio. Se cierran todos los programas de la sesión
  (lo que no esté guardado se pierde: pregunta antes) y vuelve la pantalla de inicio de sesión. No hay
  dos sesiones a la vez.
- **Carpeta personal:** cada cuenta nueva tiene `/home/<nombre>` (solo suya, 0700), pero sus programas
  la ven como `/home`: su configuración va en su `/home/.config`, sin cambiar ningún programa, y no
  puede ver ni nombrar los archivos de las demás cuentas. La cuenta principal, `sagui`, sigue usando
  `/home` tal cual (sus archivos no se mueven) y ve las carpetas de las demás, pero no entra. Root (o un
  programa con permisos de administrador) ve el árbol real.
- **Administradores:** las cuentas marcadas como administrador (grupo `admin`, en `/etc/grupos`) dan
  **su propia contraseña** en el diálogo de permisos y en `admin`/`su` (como `sudo`); la de root sigue
  valiendo. Por defecto no hay ninguna.
- **Borrar una cuenta:** pregunta también si se borran sus archivos (`/home/<nombre>`); si no, se
  conservan, y su número no se da a otra cuenta mientras la carpeta exista. No se pueden borrar ni
  renombrar `root`, `sagui`, las cuentas de servicio ni la de la sesión abierta.
- **Protecciones (D371):** lo que cuelga directamente de `/home` (la carpeta de cada cuenta) solo lo borra o
  renombra su dueño o root, aunque `/home` sea de `sagui`; y al entrar, si la carpeta personal no es de la
  cuenta o otros pueden escribir en ella, no se entra (aviso en la pantalla: lo arregla el administrador). La
  primera contraseña de una cuenta la pone ella misma o root (desde otra cuenta, `su sagui` o `passwd sagui` no
  dejan elegirla).
- **Necesita** `/home` en un disco que guarde dueños (ext2, o la RAM en vivo): con `/home` FAT32 no se
  crean cuentas.
- **Aún no:** SSH y `crontab` por cuenta (siguen siendo de `sagui` y root) e inicio de sesión en la
  consola de texto.

## `admin`, `su` y `passwd`

```
admin montar usb0 /datos        una orden como root (pide la contraseña de root)
admin servicio iniciar sshd
admin instalar --ventana        también los programas con ventana
su                              un shell de root («exit» para volver)
su -c 'orden'                   una orden en un shell de root
passwd root                     cambiar la contraseña de root
passwd                          la de tu cuenta
```

No hay `setuid`: `admin` y `su` piden al **gestor de servicios** (que corre
como root) que compruebe la contraseña; si es buena, el kernel convierte a ese
mismo programa en root y la orden corre con el mismo terminal. Si la
contraseña llega por una tubería (`echo clave | admin id`, o por SSH sin
terminal), se lee una línea.

- **La primera vez:** root no tiene contraseña. El primer `admin`, `su` o
  `passwd root` **en la sesión local** (el escritorio o la consola, no por SSH)
  pide elegirla (al menos 4 caracteres, dos veces). Hasta entonces, una bandeja
  avisa al arrancar.
- **Intentos:** cada fallo tarda 2 s en contestarse; tras 5 fallos seguidos, un
  minuto sin poder intentarlo (ni con la buena).
- **Registro:** cada elevación y cada negativa quedan en el registro del
  sistema (`dmesg`, líneas `[USUARIOS]` y `[PERMISOS]`) y en
  `/logs/servicios.log`; nunca las contraseñas.
- **Diálogo gráfico:** ver la sección siguiente. Los programas con ventana de administración
  ya no mandan al Terminal: piden permisos con una ventana del sistema.

## El diálogo de permisos (D269)

Cuando un programa con ventana necesita ser administrador (abrir Particiones o formatear en Discos,
Instalar SaguiOS, finalizar un proceso de otra cuenta o manejar servicios en el Administrador de tareas,
cambiar la zona horaria o el bloqueo de publicidad en Configuración), el sistema **atenúa el escritorio**
y muestra la ventana «Control de permisos del sistema»: el icono y el nombre del programa, qué quiere
hacer, un campo para la **contraseña de administrador (root)** y los botones «Sí, continuar» / «No»
(Esc = No; Tab cambia de campo).

- **Primera vez:** si root aún no tiene contraseña, el diálogo pide elegirla dos veces (al menos 4
  caracteres).
- **Errores:** «contraseña incorrecta» (se puede reintentar; cada fallo tarda 2 s), «las dos contraseñas no
  coinciden» y, tras 5 fallos, «espera un minuto» (el mismo bloqueo que `admin`).
- **Recordar 5 minutos:** si acabas de autorizar un programa del sistema, abrirlo otra vez en los
  siguientes 5 minutos no vuelve a preguntar. Se pierde al reiniciar.
- **Seguridad:** lo pinta el propio gestor de ventanas, no el programa; el programa no recibe ni ve la
  contraseña (se la entrega el sistema al gestor de servicios), mientras está abierto el teclado y el ratón
  son solo suyos, y atenúa también la barra superior, que ninguna ventana puede cubrir. **Desconfía de
  cualquier ventana que te pida la contraseña de administrador sin que se atenúe toda la pantalla.**
- Sin escritorio, por SSH o desde un servicio no hay diálogo: se sigue usando `admin` y `su`.
- Para programadores: `elevar_grafico("qué quiere hacer")` y `elevacao_soltar()` (libsagui, `contas.h`),
  o `COM_ADMIN("motivo", llamada)` que repite una llamada denegada tras pedir permisos.

## Dónde se guarda: la partición SAGUI-CONF

La configuración del sistema vive en una partición ext2 pequeña (16 MB) con la
etiqueta **SAGUI-CONF**, en el disco de arranque. Se monta en `/sistema/conf`
y solo root puede leerla o escribirla. Al arrancar, su carpeta `etc/` se copia
sobre el `/etc` de la RAM: los programas leen `/etc` como siempre. Lo que se
guarda ahí:

- `etc/usuarios` (las cuentas) y `etc/sombra` (las contraseñas, PBKDF2-HMAC-SHA256
  con sal y 20 000 iteraciones);
- `etc/ssh/authorized_keys` (las claves que entran por SSH **como root**) y la
  clave del servidor SSH (`etc/ssh/ssh_host_ed25519_key`);
- `etc/vnc/vncd.passwd` (la contraseña del escritorio remoto);
- `etc/servicios/activados` (qué servicios del sistema arrancan solos).

La primera vez que arrancan con SAGUI-CONF, `sshd` y `vncd` pasan ahí su clave
y su contraseña de antes (`/home/.ssh/...`, `/home/.config/vncd.passwd`), y el
gestor de servicios el arranque automático de los servicios del sistema: la
huella SSH no cambia y lo que arrancaba solo sigue arrancando.

**Sin SAGUI-CONF («modo en vivo»:** CD, imagen sin la partición, disco antiguo):
nada del sistema se guarda; root no tiene contraseña y `admin` funciona sin
pedirla, pero **solo desde la sesión local**: por SSH no se puede ser root.
Una bandeja lo avisa al arrancar.

## SSH y VNC

- La clave en `/home/.ssh/authorized_keys` entra como **sagui** (también si el
  cliente pide `root@`: se anota). La de `/etc/ssh/authorized_keys` (en
  SAGUI-CONF) entra como **root**. Para administrar por SSH sin clave de root:
  entrar como sagui y `admin orden` con la contraseña de root.
- Desde una sesión SSH no se puede apagar ni reiniciar sin ser root, ni fijar
  la primera contraseña de root.
- `sshd` y `vncd` corren como root (el `vncd` necesita la inyección de entrada);
  el shell, las órdenes, el SFTP y los túneles de cada sesión SSH ya corren con
  la cuenta con la que se entró.
- `vncd --clave` es cosa del administrador: `admin vncd --clave CLAVE`.

## Modo rescate

Si algo sale mal (contraseña olvidada, configuración rota): **mantener pulsada
la tecla Ctrl al encender** (en cuanto termina la BIOS). El cargador escribe
«Ctrl pulsado: MODO RESCATE» y arranca la consola de texto con un **shell de
root sin contraseña**, sin escritorio y sin servicios. Desde ahí:

```
passwd root               otra contraseña de root
mount                     ver qué hay montado (SAGUI-CONF en /sistema/conf)
reiniciar                 volver al arranque normal
```

Para pruebas: `mkimagem.py --rescate`. Quien tiene acceso físico a la máquina
puede usarlo: es lo mismo que en cualquier sistema sin cifrado de disco.

## Servicios

Cada `.conf` de `/etc/servicios` dice con qué cuenta corre (`usuario=root`,
`usuario=servicio`...; sin la clave, root). Los de `/home/.config/servicios`
(los que crea el usuario) corren siempre como `sagui`. Las tareas de
`/etc/crontab` corren como root y las de `crontab` del usuario como `sagui`.

## Si algo está dañado

- `/etc/usuarios` falta o tiene líneas malas: se usan las cuentas de serie para
  lo que falte (root y sagui siempre existen).
- La sombra falta o la línea de una cuenta no se entiende: esa cuenta queda sin
  contraseña (se vuelve a elegir desde la sesión local) y el gestor lo anota.
  Nunca queda bloqueada.
- El gestor de servicios no está (no arrancó o se desactivó con
  `--sin-servicios`): `admin` y `su` no pueden comprobar contraseñas y lo dicen;
  queda el modo rescate.

## Permisos de archivos (D282, fase 2)

Cada archivo y carpeta tiene **dueño**, **grupo** y permisos `rwx` para el dueño,
el grupo y los demás (más el bit **pegajoso** de `/tmp`: cada uno borra solo lo
suyo). `ls -l` los muestra y Archivos, en «Propiedades»:

```
drwxr-xr-x root     root      15/11/2026 10:00  <carpeta>  bin/
drwx------ root     root      15/11/2026 10:00  <carpeta>  sistema/
drwxrwxrwt root     root      15/11/2026 10:00  <carpeta>  tmp/
-rw-r--r-- sagui    sagui     15/11/2026 10:02         5  nota.txt
```

- **Para leer** un archivo hace falta `r`; **para cambiarlo**, `w`; **para
  ejecutarlo**, `x`. En una carpeta, `r` deja **ver** lo que hay, `x` deja
  **entrar** (y llegar a lo de dentro) y `w` deja **crear, borrar y renombrar**.
- **root lo puede todo.** `sagui` no puede cambiar `/etc`, `/bin` ni el resto del
  sistema, ni entrar en `/sistema`; sí todo lo de `/home`, `/tmp` y los pendrives.
- **Lo nuevo** nace `rw-r--r--` (archivos) o `rwxr-xr-x` (carpetas) con la máscara
  de serie, `umask 022`. `umask 077` en el Terminal hace que nazcan privados
  (`rw-------`); la heredan los programas que se lanzan desde ahí. Un programa
  recién compilado o copiado (un ELF) nace ya ejecutable.
- **Cambiarlos:** `chmod 640 nota.txt`, `chmod u+x,go-w script`, `chmod -R ...`
  (el dueño o root); `chgrp` (al grupo propio); `chown` solo root
  (`admin chown sagui:sagui archivo`).

Según dónde esté el archivo:

| Dónde | Dueño y permisos |
|-------|------------------|
| El sistema (el ramdisk: `/bin`, `/etc`...) | de root (0755 y 0644); `/home` de `sagui`, `/tmp` 1777, `/sistema` 0711 (lo de dentro 0700, salvo `/sistema/idiomas`, que lee cualquiera: D313) |
| ext2 (`/home` persistente, `/jogos`, `/apps`, pendrives de Linux) | los suyos, como en Linux; se guardan |
| FAT32, CD y DVD (ISO 9660, UDF) | no los guardan: los da el montaje (`uid=`, `gid=`, `umask=`); de serie, de `sagui` con 022 |

En el `/etc/fstab` o con `mount`:

```
LABEL=DATOS   /datos   auto   rw,uid=root,gid=root,umask=022
admin mount -o rw,uid=sagui,umask=077 usb0p1 /privado
```

Mientras haya un solo usuario, **todo lo de `/home` es de `sagui`**: lo que creó root
ahí (o lo que se guardó antes de esta versión, cuando todo era de root) se ve como
de `sagui`. Unos pocos secretos se protegen además por su ruta, también en un
`/home` FAT32: la sombra, la clave privada del servidor SSH y la contraseña del VNC.

## Límites (pendiente: fase 3, v8.4.3)

- Sin setuid, setgid ni ACL; un solo grupo por cuenta.
- Un pendrive ext2 formateado con SaguiOS antes de la v8.4 es todo de root: `sagui`
  lo lee pero no escribe en él hasta un `admin chown -R sagui /usb`.
- Los programas portados (su `libc`: `stat`, `chmod`) aún no ven los permisos reales.
- Un solo usuario de escritorio y un solo `/home`.
- Puertos de red bajos (< 1024): cualquiera puede escuchar en ellos.
- La configuración de red del sistema (DNS, IP fija) sigue donde estaba.
- El diálogo gráfico (D269) eleva solo al programa que lo pide y mientras no suelte la elevación.
