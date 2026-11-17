# SSH de administración: entrar en SaguiOS desde el PC (D250)

`sshd` es un servidor SSH pequeño, escrito para SaguiOS, que permite **entrar desde otro
ordenador de la red local** (con el `ssh`, `scp` y `sftp` de OpenSSH: Linux, WSL, macOS o
Windows 10/11) para **depurar**: ejecutar órdenes y ver su salida, abrir un `sagsh`
interactivo y traer o llevar archivos (registros, capturas, programas).

Es un acceso de **administración**: quien entra ve el sistema entero, como en el Terminal.
No es el servidor de *jails* (cuentas encerradas en una carpeta) de la mejora 052, que
sigue pendiente. Para ir en el otro sentido, de SaguiOS a otra máquina, está el **cliente `ssh`**
(D317, v8.4.1): ver [Cliente ssh](#cliente-ssh-de-saguios-a-otra-máquina-d317).

## En dos minutos

En el PC, si aún no tienes una clave ed25519:

```
ssh-keygen -t ed25519
cat ~/.ssh/id_ed25519.pub        # una línea: ssh-ed25519 AAAA... tu@pc
```

En SaguiOS, pon **esa línea** en `/home/.ssh/authorized_keys` (con el editor, o copiando el
`.pub` en un pendrive: `mkdir /home/.ssh` y `cp /usb/id_ed25519.pub /home/.ssh/authorized_keys`)
y enciende el servicio:

```
servicio activar sshd      # arrancará solo en cada arranque
servicio iniciar sshd      # y ahora
sshd --huella              # la huella de la clave del servidor
ifconfig                   # la IP de la máquina
```

Desde el PC:

```
ssh root@192.168.1.50                    # sagsh interactivo
ssh root@192.168.1.50 'dmesg'            # una orden: su salida, sus errores y su código
ssh root@192.168.1.50 'cat /logs/sshd.log' > sshd.log
scp root@192.168.1.50:/logs/sshd.log .   # traer un archivo
scp programa root@192.168.1.50:/home/    # llevarlo
sftp root@192.168.1.50                   # ls, get, put, mkdir, rm, rename
ssh -p 2222 root@192.168.1.50            # si se cambió el puerto
ssh -L 5900:localhost:5900 root@192.168.1.50   # túnel a un servicio de la máquina (el VNC)
```

La primera vez, `ssh` enseña la huella del servidor (`SHA256:...`): tiene que ser la de
`sshd --huella`. El nombre de usuario da igual (SaguiOS tiene un solo usuario); `root` es
costumbre.

## Qué hay y qué no

| | |
|---|---|
| Protocolo | SSH-2 (RFC 4253, 4252, 4254) |
| Intercambio de claves | `curve25519-sha256` (y `curve25519-sha256@libssh.org`), con el modo estricto de OpenSSH 9.6 (`kex-strict`, contra el ataque «Terrapin») |
| Clave del servidor | `ssh-ed25519` |
| Cifrado | `chacha20-poly1305@openssh.com` |
| Compresión | ninguna |
| Autenticación | **solo clave pública `ssh-ed25519`**. No hay contraseñas |
| Canales | `session`: `exec`, `shell` (con o sin terminal) y el subsistema `sftp`; `direct-tcpip`: redirección local de puertos (`ssh -L`, D253) |
| No hay | redirección remota (`ssh -R`) ni dinámica hacia fuera (`ssh -D` solo llega a esta máquina), agente, X11, RSA/ECDSA, AES, contraseñas, `scp -O` (el protocolo antiguo) |

Un cliente sin esos algoritmos (OpenSSH anterior a la 6.5, de 2014) recibe una desconexión
con el motivo; cualquier OpenSSH, PuTTY o WinSCP actual los tiene.

### Órdenes (`ssh IP 'orden'`)

Se ejecutan con `sagsh -c`, en la carpeta `/`, con la entrada, la salida y los errores por
separado, y `ssh` termina con el **código de salida** de la orden. Valen tuberías y
redirecciones dentro de las comillas. Si el cliente se va (Ctrl+C en el PC), la orden y todo
lo que lanzó se terminan.

**Acentos.** SaguiOS escribe en Latin-1 y el PC espera UTF-8. Sin terminal, la salida va
**tal cual** (sirve para datos binarios: `ssh IP 'tee /home/x' < x`), así que los acentos
se ven mal. Tres formas de arreglarlo:

- `ssh -t IP 'orden'`: con terminal, se convierte a UTF-8 (pero salida y errores van juntos);
- `ssh IP 'orden' | iconv -f latin1`;
- `exec_utf8=si` en la configuración: se convierte siempre (y entonces no mandar binarios
  por ahí: para archivos, `scp`/`sftp`).

Ojo: el `cat` de SaguiOS convierte UTF-8 a Latin-1 (D027), así que **no** es transparente
para binarios; `tee`, `gzip -c` o, mejor, `scp` sí lo son.

### Terminal interactivo (`ssh IP`)

Un `sagsh` sobre una PTY (D036), con edición de línea, historial, Ctrl+C y Ctrl+D. Las
teclas de xterm (flechas, Inicio, Fin, Supr, RePág/AvPág) se traducen a las de SaguiOS y la
salida a UTF-8. Límites: la PTY no tiene tamaño (el cambio de tamaño de la ventana se
acepta y se ignora; `LINES` y `COLUMNS` se ponen al entrar) y los programas de pantalla
completa que esperan el Terminal de SaguiOS pueden no verse bien. Pegar mucho texto de golpe
puede perder caracteres (la PTY tiene 512 bytes de cola).

### Archivos (`scp`, `sftp`)

El `scp` de OpenSSH 9 y posteriores y `sftp` usan el subsistema SFTP (versión 3): abrir, leer,
escribir, listar, `stat`, borrar, `mkdir`, `rmdir`, renombrar. Las rutas relativas salen de
`/home`. Los nombres con acentos se convierten entre UTF-8 y Latin-1. SaguiOS no tiene
permisos, dueños ni enlaces simbólicos: los permisos y las fechas que manda el cliente
(`scp -p`) se aceptan y se ignoran. Con un `scp` antiguo, o con `scp -O`, no funciona: usar
`sftp`. Archivos de hasta 2 GB.

### Túneles (`ssh -L`): entrar cifrado a un servicio de la máquina (D253)

Hay servicios de SaguiOS que, por seguridad, **solo escuchan en `127.0.0.1`**: desde la red no
se ven. El caso principal es el escritorio remoto (`vncd`, mejora 097), cuyo protocolo no va
cifrado. Para usarlos desde el PC se abre un túnel por el SSH:

```
ssh -L 5900:localhost:5900 root@192.168.1.50      # deja la sesión abierta...
vncviewer localhost:5900                          # ...y en otra ventana del PC, el visor
```

`-L PUERTO_DEL_PC:localhost:PUERTO_EN_SAGUIOS`: lo que se conecte al puerto del PC sale, ya
dentro de SaguiOS, hacia `127.0.0.1` y ese puerto, con todo el camino por la red cifrado y
autenticado con la clave SSH. Variantes:

```
ssh -N -L 5900:localhost:5900 root@192.168.1.50   # solo el túnel, sin terminal (Ctrl+C para cortar)
ssh -f -N -L 5900:localhost:5900 root@192.168.1.50   # lo mismo, en segundo plano
ssh -L 5901:localhost:5900 -L 8080:localhost:80 root@192.168.1.50   # varios a la vez
```

- **Solo hacia la propia máquina** (de serie, `redirigir=local`): el destino tiene que ser
  `localhost` o `127.x.x.x`. Un túnel hacia otra máquina (`-L 8080:192.168.1.1:80`), hacia la
  dirección de la tarjeta o hacia un nombre se rechaza («administratively prohibited» en el
  PC) y queda anotado. Con `redirigir=red-local` se permiten también las direcciones de la red
  local (las mismas de `solo_red_local`), siempre **escritas con números**: los nombres no se
  resuelven. Con `redirigir=no` no hay túneles. Hacia Internet, nunca.
- **`ssh -R` no existe** (abrir puertos en SaguiOS que salgan por el PC): se rechaza.
- Hasta **8 túneles a la vez por conexión** (cada conexión del visor es uno), además de los 4
  canales de sesión; el noveno se rechaza hasta que se cierre alguno. Cada túnel gasta 64 KB.
- Cada túnel abierto (con su destino y de qué puerto del PC venía), rechazado (con el motivo)
  y cerrado (con los bytes de cada sentido) queda en `/logs/sshd.log`.
- El terminal o la orden de la misma conexión siguen funcionando mientras pasan datos.
- Una conexión `ssh -N` sin ningún túnel en uso se corta a la media hora (con
  `redirigir=no`, al minuto, como antes); con un terminal abierto no se corta.
- Límites: el fin de envío de un lado pasa al otro (FIN), pero si el servicio de destino no
  lee, sus datos esperan en el túnel (64 KB) y el PC deja de mandar hasta que haya sitio; el
  kernel tiene 32 conexiones TCP en total y cada túnel local usa dos.

## Cliente ssh: de SaguiOS a otra máquina (D317)

Desde la v8.4.1, SaguiOS trae `ssh`, `ssh-keygen` y `scp` para entrar en otra máquina (un Linux,
un Mac o un Windows con OpenSSH, u otro SaguiOS con `sshd`).

```
ssh-keygen -t ed25519                      # una vez: crea /home/.ssh/id_ed25519 y .pub
cat /home/.ssh/id_ed25519.pub              # esa línea va en ~/.ssh/authorized_keys del servidor
ssh usuario@192.168.1.10                   # sesión interactiva (terminal remoto)
ssh usuario@192.168.1.10 'df -h'           # una orden: su salida y su código de salida
ssh -p 2222 -i /home/.ssh/otra usuario@pc  # otro puerto, otra clave
echo hola | ssh usuario@pc 'cat > /tmp/x'  # por tuberías, desde otros programas
scp usuario@pc:/var/log/syslog /home/      # traer un archivo
scp informe.txt usuario@pc:/home/usuario/  # llevarlo
ssh-keygen -l                              # la huella de tu clave
ssh-keygen -R 192.168.1.10                 # olvidar una máquina (si cambió de clave a propósito)
```

**La primera vez** que se entra en una máquina, `ssh` enseña la huella de su clave
(`SHA256:...`) y pregunta «¿Seguro que quieres conectar? (sí/no)». Compárala con la que da el
servidor (`ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub` en un Linux, `sshd --huella` en
SaguiOS) antes de decir «sí»: queda guardada en `/home/.ssh/known_hosts`. Si otro día la
máquina presenta **otra clave**, la conexión se **rechaza** con un aviso grande: puede ser un
ataque, o la máquina se reinstaló. No hay opción para saltárselo: si el cambio es legítimo,
borra su línea de `known_hosts` a mano (el aviso dice cuál) o con `ssh-keygen -R máquina`.

Si el servidor admite contraseña, `ssh` la pide sin eco cuando la clave no basta (o no hay).
Las claves con **frase de paso** todavía no se admiten (`ssh-keygen` las crea sin ella: guarda
la privada como guardarías una contraseña).

**En la sesión:** Ctrl+C y Ctrl+D van a la otra máquina; `~.` al principio de una línea corta
la conexión (si el otro lado no responde), `~?` enseña la ayuda. El tamaño del terminal remoto
es el de la ventana del Terminal y cambia con ella. Colores, flechas, Inicio/Fin, RePág/AvPág y
la edición de bash funcionan; los programas de pantalla completa (`vim`, `less`, `htop`) aún se
ven mal (al Terminal le faltan regiones de desplazamiento y la pantalla alternativa) y las
teclas de función no se mandan.

**Algoritmos:** los del `sshd` de SaguiOS (curve25519-sha256 con el modo estricto, ssh-ed25519,
chacha20-poly1305@openssh.com), que tiene cualquier OpenSSH desde la 6.5. Un servidor que solo
tenga claves RSA o ECDSA (muy antiguo, o configurado así) se rechaza con el motivo.

**Opciones:** `-p PUERTO`, `-i CLAVE`, `-l USUARIO`, `-t`/`-T` (pedir o no terminal remoto),
`-n` (no leer la entrada), `-q`, `-o StrictHostKeyChecking=ask|accept-new|yes` (`accept-new`
añade sin preguntar las máquinas nuevas pero sigue rechazando las cambiadas; útil en scripts)
y `-o BatchMode=yes` (no preguntar nada). Sin usuario, el de la cuenta de SaguiOS (`sagui` o `root`).

`scp` copia **un archivo** cada vez (sin `-r`) usando `cat` en el otro lado: no necesita `scp`
ni `sftp` en el servidor. Lo que se trae se escribe primero en `ARCHIVO.scp-parcial` y solo
toma su nombre si la copia terminó bien.

### vim, less, htop, nano y mc desde el Terminal (D320)

Desde la D320 el Terminal de ventana entiende lo que esas aplicaciones necesitan: regiones de desplazamiento, insertar y
borrar líneas, pantalla alternativa (al salir de vim vuelve lo que había), 256 colores aproximados a la paleta, cajas,
atributos, DSR/DA, teclas de función, Inicio/Fin/RePág/AvPág/Insert/Supr, Ctrl+flechas y Alt+tecla. `ssh` anuncia
`TERM=xterm-256color` y, al empezar la sesión con terminal, enciende el modo xterm del Terminal (`CSI ? 7700 h`); lo apaga al
salir. Las cajas que mandan htop y mc en UTF-8 las pasa al juego de dibujo de líneas DEC. La tabla de secuencias y lo que
falta (ratón, caracteres anchos, color real) está en `docs/terminal.md`.

## Depuración remota con un código: DEBUG-APP (D362)

Para que otro equipo entre un rato sin activar este servicio ni copiar claves, DEBUG-APP abre una
sesión aparte (`sshd --depuracion`, puerto 2222) con un código de un solo uso que se le pasa a
`tools/depurar-remoto.py`. Ver [debug-app.md](debug-app.md).

## Seguridad

- **Desactivado de serie.** Sin `servicio activar sshd` (o `servicio iniciar sshd`), no hay
  nadie en el puerto 22. También se ve y se maneja en el Administrador de tareas (Servicios).
- **Solo clave pública.** Sin la clave privada de una de las líneas de `authorized_keys` no
  se entra; no hay contraseña que adivinar. Se miran `/home/.ssh/authorized_keys` y
  `/etc/ssh/authorized_keys` (este, para imágenes preparadas). Solo valen líneas
  `ssh-ed25519 AAAA... comentario`; las de otro tipo (RSA, ECDSA) y las que llevan opciones
  delante (`from=`, `command=`...) **no valen** (`sshd --claves` dice cuántas hay y cuántas
  se han descartado).
- **Solo la red local** (`solo_red_local=si`): se atiende a 10.x, 172.16-31.x, 192.168.x,
  169.254.x, 127.x y la subred de la tarjeta; a los demás se les cierra sin contestar.
- **Límites:** 4 sesiones a la vez, 6 intentos de autenticación por conexión, 60 s para
  autenticarse; tras 5 conexiones seguidas sin autenticarse, esa dirección se ignora 2
  minutos. Una sesión autenticada sin ningún canal abierto se corta a la media hora (al minuto
  con `redirigir=no`), y a un cliente
  que calla más de un minuto se le pregunta si sigue ahí (a la tercera sin respuesta, se corta).
- **Túneles solo hacia dentro:** `ssh -L` solo llega a los servicios de la propia máquina
  (`redirigir=local`); la sesión SSH no sirve de trampolín hacia la red ni hacia Internet.
- **Protocolo:** la etiqueta de cada paquete (Poly1305) se comprueba antes de descifrar y de
  procesar nada; tamaños de paquete acotados (35 000 bytes); números de secuencia por
  paquete; ventanas de canal; renovación de claves cuando la pide el cliente (OpenSSH, cada
  1 GB) y desconexión si se llegara a 2^31 paquetes con las mismas claves.
- **Clave del servidor:** Ed25519, creada en el primer arranque del servicio (o con
  `sshd --huella`) en `/home/.ssh/ssh_host_ed25519_key` (la pública, en `.pub`). Si `/home`
  no es persistente (imagen sin partición de datos), la clave **cambia en cada arranque** y
  el cliente avisará de que ha cambiado (`ssh-keygen -R IP` en el PC para olvidar la vieja);
  el registro y `sshd --huella` lo avisan. Si `/home` no se puede escribir, va a `/etc/ssh`.
- **Registro:** cada conexión, autenticación (con la huella de la clave usada), orden,
  archivo abierto por SFTP y desconexión, con fecha, en `/logs/sshd.log` (o
  `/tmp/.servicios/sshd.log` si no hay `/logs`) y en el registro del sistema (`dmesg`). Nunca
  se anotan claves privadas ni las contraseñas que alguien pruebe.
- **Aleatoriedad:** la del kernel (D061). En una CPU sin RDRAND sale de las interrupciones:
  conviene crear la clave del servidor con la máquina ya en uso (teclado, ratón, red), no en
  el primer segundo del primer arranque; el registro lo recuerda.

Lo que **no** protege: quien entra es administrador (no hay *jail*); no conviene abrir el
puerto a Internet ni poner `solo_red_local=no` salvo en una red de pruebas.

## Configuración

`/etc/ssh/sshd.conf` (los valores de serie, comentados) y `/home/.ssh/sshd.conf` (gana este;
sobrevive a los reinicios si `/home` es persistente). `clave=valor`:

| clave | de serie | |
|---|---|---|
| `puerto` | 22 | también `sshd -p PUERTO` |
| `solo_red_local` | si | `no`: atiende a cualquier dirección |
| `max_conexiones` | 4 | sesiones a la vez (1-8) |
| `max_intentos` | 6 | autenticaciones fallidas por conexión |
| `tiempo_entrada` | 60 | segundos para autenticarse |
| `bloqueo_fallos` | 5 | conexiones seguidas sin autenticarse para ignorar la dirección |
| `bloqueo_segundos` | 120 | cuánto se la ignora (0 = nunca) |
| `exec_utf8` | no | convertir a UTF-8 la salida de las órdenes sin terminal |
| `redirigir` | local | túneles `ssh -L`: `local` (solo a esta máquina), `red-local` (también a la red local) o `no` |

`puerto`, `solo_red_local`, `max_conexiones` y los bloqueos se leen al arrancar el servicio
(`servicio reiniciar sshd`); las claves autorizadas, `max_intentos`, `tiempo_entrada`,
`exec_utf8` y `redirigir`, en cada conexión. Un valor de `redirigir` que no se entienda
equivale a `no`.

## Si no entra

| lo que dice `ssh` | qué mirar |
|---|---|
| `Connection refused` / se queda esperando | `servicio estado sshd`; `ifconfig` (¿hay IP?); el puerto |
| se cierra nada más conectar | la dirección del PC no es de la red local, está bloqueada por fallos (2 min) o hay 4 sesiones: `tail /logs/sshd.log` |
| `Permission denied (publickey)` | la clave del PC no está en `authorized_keys` (`sshd --claves`), o es RSA: hace falta ed25519 |
| `REMOTE HOST IDENTIFICATION HAS CHANGED` | `/home` no es persistente y el servidor creó otra clave: `ssh-keygen -R IP` |
| `no matching cipher / key exchange found` | cliente muy antiguo |
| `channel N: open failed: administratively prohibited` | el túnel (`-L`) no va a `localhost`, o `redirigir=no`: `tail /logs/sshd.log` dice el motivo |
| `channel N: open failed: connect failed` | en ese puerto de SaguiOS no escucha nadie (¿está arrancado el servicio, p. ej. `vncd`?) |
| `scp` falla con «scp: no existe» o parecido | es `scp -O` o un scp antiguo (no hay programa `scp` en SaguiOS): usar `sftp` |

`ssh -v` en el PC y `tail /logs/sshd.log` (o `dmesg | grep sshd`) en SaguiOS cuentan el resto.

## Cómo está hecho

- `userland/libtls/ed25519.c`: Ed25519 (RFC 8032) propio, sobre la aritmética de campo del
  X25519 (`campo25519.h`, ahora compartida) y el SHA-512 de la libtls; la multiplicación por
  el escalar hace los mismos pasos para cualquier clave. `sshcifra.c`: el cifrado de paquetes
  de OpenSSH con el ChaCha20 y el Poly1305 que ya había.
- `userland/programas/sshd.c`: el proceso que escucha lanza un `sshd --sesion` por conexión,
  con el socket como entrada (un hijo solo hereda los descriptores 0, 1 y 2): un fallo en
  una sesión no tumba el servicio. Cada sesión es un bucle que mira el socket, las tuberías
  (o la PTY) de sus procesos, los sockets de sus túneles y los hijos terminados.
  `sshd_sftp.h`: el SFTP.
  `sshd_logica.h`: lo que no toca el sistema (campos, listas de algoritmos,
  `authorized_keys`, red local, teclas, rutas), probado en el anfitrión.
- Pruebas: `tests/h248_ssh.py` (vectores de la RFC 8032, comparación con una implementación
  independiente, y el OpenSSH del anfitrión contra el servidor en QEMU: órdenes, terminal,
  `scp`, `sftp`, rechazos, paquetes manipulados, bloqueo), `tests/h251_ssh_tunel.py` (los
  túneles `ssh -L` contra `/testes/ecotcp`, un servidor de eco) y `/testes/pruebacripto` (h59).
