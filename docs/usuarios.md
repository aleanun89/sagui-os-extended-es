# Usuarios y administrador (D255, mejora 098, fase 1)

SaguiOS tiene dos cuentas de persona y una de servicio:

| Cuenta     | uid  | Para qué |
|------------|------|----------|
| `root`     | 0    | El administrador. Tiene contraseña (o la tendrá la primera vez que se use). |
| `sagui`    | 1000 | El usuario del escritorio. Entra solo al arrancar: el escritorio, el Terminal y todo lo que se abre desde el menú son `sagui`. |
| `servicio` | 100  | Servicios que no necesitan ser root (`litemonkey`, `dnsbloqueo`). |

`id` y `whoami` dicen con qué cuenta corre cada programa.

## Qué puede hacer `sagui` y qué no

Todo lo normal: programas, ventanas, archivos (en esta fase no hay permisos de
archivos: ver más abajo), red, sonido, imprimir capturas, cambiar el fondo, el
teclado o la resolución, apagar y reiniciar desde el escritorio.

Hace falta ser administrador para: montar y desmontar discos, particionar,
formatear e instalar, leer o escribir discos en crudo, configurar la red, el
Wi-Fi, el DNS y el bloqueo de publicidad, manejar los servicios (`servicio
iniciar|parar|activar...`; consultar sí se puede), la hora y la zona del
sistema, el acelerador 2D, la frecuencia de la CPU, el ACPI, el acceso directo
al hardware (BAR de PCI, verbos del códec de sonido), terminar programas de
otra cuenta, y escribir en `/etc` y `/bin`. La tabla completa está en
`docs/decisoes.md` (D255) y en `kernel/nucleo/permisos.c`.

Un programa que lo intenta sin serlo recibe «operación no permitida (hace
falta ser administrador: usa «admin orden»)».

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
- **Diálogo gráfico:** todavía no hay ventana para la contraseña (pendiente);
  los programas con ventana que necesitan ser root lo dicen y se abren con
  `admin programa` desde el Terminal.

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

## Límites de esta fase (pendiente: fases 2 y 3, v8.5)

- **No hay permisos de archivos.** `sagui` puede leer y escribir todo menos unas
  rutas fijas que el kernel protege: no lee la sombra, la clave del servidor SSH,
  la contraseña del VNC ni `/sistema`; no escribe en `/etc`, `/bin`, `/sistema` ni
  en las órdenes del gestor de servicios. El resto de `/home`, `/logs`, `/tmp`,
  `/apps`, `/jogos` y los pendrives, sin barreras.
- Un solo usuario de escritorio y un solo `/home`.
- Puertos de red bajos (< 1024): cualquiera puede escuchar en ellos.
- La configuración de red del sistema (DNS, IP fija) sigue donde estaba.
- No hay diálogo gráfico de contraseña (la interfaz `SERV_PEDIR`/`pedir_conta`
  ya está para él).
