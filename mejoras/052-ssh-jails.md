# 052 - SSH: cliente `ssh` y servidor de *jails* (cuentas encerradas en una carpeta)

- **Estado:** en curso: el **servidor de administración** está hecho (D250, v8.0b5, 2026-11-14);
  pendientes el cliente `ssh` y el servidor de *jails*
- **Prioridad:** media
- **Alta:** 2026-11-07
- **Relacionadas:** 027 (libtls: X25519, ChaCha20-Poly1305, AES-GCM, SHA-2, HMAC, RSA),
  D061 (aleatoriedad), D036 (pty), 020/045 (red), 042 (componentes), 035 (VFS)

## Hecho en la v8.0b5 (D250): servidor de administración

No es el servidor de jails de esta ficha: quien entra ve **todo el sistema**. Está pensado
para depurar desde el PC en la red local. Guía en `docs/ssh.md`.

- `sshd` (`userland/programas/sshd.c`, servicio `/etc/servicios/sshd.conf`, **desactivado de
  serie**): SSH-2 con `curve25519-sha256`, `ssh-ed25519` y `chacha20-poly1305@openssh.com`,
  modo estricto de intercambio, renovación de claves a petición del cliente.
- **Ed25519** nuevo en la libtls (`ed25519.c`, sobre `campo25519.h` y el SHA-512) y el
  cifrado de paquetes de OpenSSH (`sshcifra.c`).
- Solo **clave pública** (`/home/.ssh/authorized_keys`, `/etc/ssh/authorized_keys`).
- Canales `session`: `exec`, `shell` con PTY (teclas de xterm y UTF-8 traducidos) y
  subsistema **`sftp`** versión 3 (lo usan `sftp` y `scp`).
- Solo red local, límites de conexiones e intentos, bloqueo temporal por fallos, registro
  en `/logs/sshd.log`, huella con `sshd --huella`.
- Pruebas contra el OpenSSH del anfitrión: `tests/h248_ssh.py`.

De la «parte común» de abajo quedan sin hacer: `aes256-gcm`, `aes128-ctr` + HMAC y
`rsa-sha2-256` (solo harán falta en el cliente, para servidores que no tengan los
algoritmos modernos), `window-change` de verdad (la PTY no tiene tamaño) y la prueba del
cliente contra un `sshd` del anfitrión.

**Lo que falta para el cliente `ssh`** (segunda fase): el lado cliente del transporte
(reutiliza `ed25519.c`, `sshcifra.c` y `sshd_logica.h`), `known_hosts` con TOFU, leer la
clave privada en el formato de OpenSSH o un `ssh-keygen` propio, autenticación por
contraseña y teclado interactivo, y la emulación VT100/xterm en el Terminal.

**Lo que falta para los jails:** todo lo de «Servidor de jails» (raíz por proceso en el
VFS, proceso restringido, cuentas con contraseña, interfaz gráfica).

## Objetivo

1. **Cliente `ssh`** para entrar desde SaguiOS en otras máquinas (Linux, el PC del
   usuario...), con `scp`/`sftp` opcionales.
2. **Servidor SSH de *jails***: no da acceso al sistema completo. Cada **cuenta** se define en
   una interfaz gráfica con una **carpeta**, un **usuario**, una **contraseña** y los
   **tipos de autenticación permitidos**. Quien entra con esa cuenta ve esa carpeta como
   **`/`** y **no puede salir de ella** de ninguna forma.

## Parte común: protocolo SSH-2

- RFC 4251-4254 y extensiones habituales de OpenSSH.
- **Algoritmos:**
  - intercambio de claves `curve25519-sha256` (X25519 + SHA-256 de la libtls);
  - claves de servidor y de usuario **`ssh-ed25519`**: **Ed25519 nuevo**, sobre la aritmética
    de X25519 y el SHA-512 existentes; `rsa-sha2-256` solo para verificar en el cliente;
  - cifrado `chacha20-poly1305@openssh.com` y `aes256-gcm@openssh.com`, con
    `aes128-ctr` + `hmac-sha2-256` como reserva;
  - sin compresión.
- Canales `session` con `pty-req`, `shell`, `exec`, `window-change` y el subsistema
  `sftp`; ventana y control de flujo. Sin redirección de puertos, agente ni X11.
- **Pruebas** contra **OpenSSH** del anfitrión, sin Internet: el cliente contra un `sshd`
  lanzado como usuario en un puerto alto con configuración propia (si está instalado), y el
  servidor con el `ssh`/`sftp` del anfitrión entrando por la redirección de puertos de QEMU.

## Cliente

- `ssh [-p puerto] [-i clave] usuario@host [orden]`.
- Autenticación por contraseña, por teclado interactivo y por clave Ed25519
  (`ssh-keygen -t ed25519` propio, en `/home/.ssh/`).
- `known_hosts` con TOFU: avisa si cambia la clave de un servidor y se niega a seguir.
- **Emulación de terminal VT100/xterm** ampliada en el Terminal: colores (16/256), mover el
  cursor, borrar, regiones de desplazamiento, pantalla alternativa y teclas de función. Así
  funcionan `vim`, `nano`, `htop` o `less` remotos. Sirve también para programas propios.
- `scp` y `sftp` (cliente) opcionales.

## Servidor de jails

### Concepto
- Una **cuenta** (jail):
  - nombre de usuario;
  - **carpeta raíz** (p. ej. `/home/compartido/ana`);
  - contraseña;
  - **métodos permitidos**: contraseña, clave pública o ambos;
  - claves públicas autorizadas;
  - **servicios permitidos**: shell interactivo, `exec`, `sftp`/`scp`;
  - solo lectura (sí/no);
  - sesiones simultáneas máximas;
  - activa (sí/no).
- Quien entra queda en un proceso **encerrado**: su `/` es la carpeta de la cuenta.

### Garantía en el kernel (la parte compleja y la que manda)
- **Raíz por proceso** (*chroot* aplicado de verdad), en la resolución de rutas del VFS
  (`vfs.c`, montajes de la D040):
  - toda ruta absoluta se resuelve desde la raíz del proceso;
  - `..` en la raíz se queda en la raíz;
  - la carpeta actual no puede quedar fuera;
  - se hereda en `executar` y no se puede deshacer (no hay syscall para ampliar la raíz).
  Lo mismo para los montajes que haya debajo de la carpeta.
- **Proceso restringido** (bandera heredable): sin acceso a las syscalls de administración:
  - montar y desmontar;
  - reiniciar y apagar;
  - configurar la red;
  - cambiar la hora o la zona;
  - matar o ver procesos fuera de su jail;
  - modo de vídeo;
  - leer el log del kernel e `INFO_*` sensibles;
  - abrir dispositivos (`/dev`, salvo `null`/`urandom`/su pty);
  - crear ventanas en el escritorio local.
  Una lista **blanca** de syscalls permitidas es más segura que una negra.
- **Programas dentro del jail:** el jail ve **`/bin` y `/lib` del sistema en solo lectura**
  (una vista o montaje de solo lectura que el kernel superpone), para tener `sagsh`, `ls`,
  `cat`, `cc`, `python`... sin copiarlos en cada carpeta. Lo demás es la carpeta de la cuenta.
  La lista de programas visibles se puede limitar por cuenta.
- **Solo lectura**: si la cuenta lo es, el kernel rechaza toda escritura dentro del jail.
- **Herencia (todos los procesos lanzados dentro quedan enjaulados):** la raíz y la marca de
  proceso restringido pasan a **cada hijo** en `executar`: shell, órdenes, procesos en
  segundo plano, cada etapa de un pipe, `python` y lo que lance con `os.system`, los
  programas compilados con `cc`... No existe syscall para ampliar la raíz ni quitar la
  marca, ni siquiera al propio proceso.
- **Aislamiento entre procesos:** un proceso enjaulado solo ve, espera o mata procesos de su
  **mismo jail**; pipes y terminales virtuales solo entre ellos.
- **Fin de sesión:** al desconectar se terminan **todos** los procesos de la sesión, también los
  de segundo plano, salvo que la cuenta permita expresamente dejarlos corriendo.
- **Red desde dentro (opción por cuenta):** **sin red** (por defecto), solo conexiones
  salientes, o sin restricciones. Nunca puede abrir puertos de escucha (no se convierte en
  servidor ni en trampolín).
- **Límites:** procesos por cuenta (una «bomba de procesos» no tumba el sistema), memoria por
  proceso y tiempo inactivo.

### Contraseñas y claves
- Las contraseñas se guardan **solo como hash con sal**, con PBKDF2-HMAC-SHA256 y muchas
  iteraciones (las funciones existen en la libtls), en `/etc/ssh/cuentas` (o `/home/.ssh/`
  si es persistente).
- La clave del servidor (Ed25519) se genera la primera vez, mezclando entropía del uso
  (teclado, ratón, red) en máquinas sin RDRAND (G470). Se muestra la huella para
  comprobarla desde el cliente.
- **Protección:** retraso creciente tras cada fallo de autenticación y bloqueo temporal de la
  IP tras N fallos; registro de entradas y fallos.

### Interfaz gráfica «Servidor SSH»
- Activar/desactivar el servicio. **Desactivado por defecto.**
- Puerto e interfaz de escucha. Por defecto, **solo la red local**.
- Lista de cuentas con añadir/editar/borrar: usuario, **selector de carpeta**, contraseña (dos
  veces), casillas de métodos de autenticación y servicios, solo lectura, sesiones máximas,
  claves autorizadas (pegar o importar un `.pub`).
- Sesiones activas (quién, desde dónde, desde cuándo) con «Desconectar».
- Registro de conexiones.
- Huella de la clave del servidor.
- Usa los componentes de la 042 (tabla, casillas, pestañas) si ya existen.
- Configuración también por texto (`/etc/ssh/sshd.conf`) y comando `sshd` para
  arrancar/parar y ver el estado.

### Servicios
- **shell**: `sagsh` encerrado, con pty.
- **exec**: una orden sin pty.
- **sftp**: subsistema SFTP v3 (servidor) dentro del jail. Es el uso típico: «una carpeta
  compartida para alguien», desde cualquier cliente SFTP (FileZilla, WinSCP...). `scp`
  clásico encima, si es barato.

## Pruebas (además de las del protocolo)

- **Intentos de fuga** automatizados desde dentro del jail:
  - `cd ..` repetido;
  - rutas absolutas;
  - `../../` en `open`/`executar`/`renombrar`/`mkdir`;
  - nombres con barras o puntos raros;
  - montar;
  - matar procesos ajenos;
  - leer `/dev`, el log o `INFO_*`;
  - escribir en `/bin`;
  - escribir en una cuenta de solo lectura;
  - abrir ventanas;
  - lanzar procesos hijos, en segundo plano o con `python os.system`, y comprobar que siguen
    enjaulados;
  - ver o matar procesos de otro jail o del sistema;
  - abrir sockets o puertos de escucha según la opción de red de la cuenta;
  - procesos que siguen vivos tras desconectar.
  Todo debe fallar con `ERRO_ACESSO` y el sistema debe seguir intacto (sumas de archivos
  fuera del jail antes y después).
- **Pruebas en el anfitrión** de la resolución de rutas con rutas aleatorias (*fuzzing*): el
  resultado siempre queda dentro de la raíz.
- **Autenticación:**
  - método no permitido rechazado;
  - contraseña incorrecta con retraso;
  - bloqueo tras N fallos;
  - clave no autorizada.
- `sftp` del anfitrión: subir, bajar, listar y borrar dentro del jail; fuera, imposible.

## Criterios de aceptación

- [ ] Cliente `ssh` contra OpenSSH: shell, orden, clave Ed25519 y `known_hosts`; `vim`/`htop`
      remotos se ven bien
- [ ] Servidor: cuentas con carpeta como `/`, métodos y servicios configurables desde la
      interfaz; `ssh` y `sftp` de OpenSSH funcionan
- [ ] Todas las pruebas de fuga fallan como deben; *fuzzing* de rutas limpio
- [ ] Contraseñas con PBKDF2; bloqueo por fallos; servicio desactivado por defecto
- [ ] Tests automáticos en `tests/`
- [ ] Decisiones registradas en `docs/decisoes.md` (protocolo, jails en el kernel, cuentas)

## Notas

- **Esfuerzo:** cliente + protocolo + Ed25519 + VT100: ~4-5 h de hilo. El servidor de jails
  (raíz por proceso y proceso restringido en el kernel, cuentas, interfaz, SFTP): otras
  ~5-7 h. Conviene hacerlo en dos fases: cliente primero, servidor después.
- **Seguridad:** criptografía y protocolo propios y sin auditar. Adecuado para la red local
  y para aprender; **no para exponer el servidor a Internet**. El aislamiento depende de que el
  kernel compruebe **todas** las syscalls con rutas o privilegios: cualquier syscall nueva
  futura debe declarar si se permite a procesos restringidos (la lista blanca obliga a ello).
- Encaja con la mejora 035 (arquitectura de almacenamiento): la raíz por proceso es un cambio
  del VFS que conviene diseñar junto con ella.
