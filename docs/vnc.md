# Escritorio remoto VNC: servidor `vncd` (mejora 097, D252)

`vncd` es un servidor VNC pequeño, escrito para SaguiOS, que deja **ver y manejar la pantalla**
desde el PC con un cliente normal (TightVNC 2.x, TigerVNC, RealVNC, UltraVNC, `vncviewer`).
Sirve sobre todo para **depurar en las máquinas reales** (portátiles de prueba): los fallos
gráficos hay que verlos. Protocolo RFB 3.8 (RFC 6143), con 3.7 y 3.3 por compatibilidad.
Código propio a partir de la especificación; no hay nada portado de TightVNC ni de ningún
otro servidor.

## En dos minutos

Receta probada de extremo a extremo (`tests/h252_vnc_tunel.py`, con el OpenSSH del anfitrión).

**En SaguiOS** (una vez; la clave pública del PC va en `/etc/ssh/authorized_keys` o `/home/.ssh/authorized_keys`, ver `docs/ssh.md`):

```
vncd --clave secreto            # la contraseña (el protocolo solo usa los 8 primeros caracteres)
servicio activar sshd           # arrancarán solos en cada arranque
servicio activar vncd
servicio iniciar sshd           # y ahora
servicio iniciar vncd
```

**Desde el PC**, por un túnel SSH (de serie `vncd` solo acepta conexiones de la propia máquina,
que es lo que ve cuando el `sshd` reenvía el puerto):

```
ssh -N -L 5900:localhost:5900 root@192.168.1.50     # deja este terminal abierto; Ctrl+C lo cierra
vncviewer localhost:5900                            # o TightVNC Viewer -> localhost:5900
```

Si el 5900 del PC está ocupado, otro puerto local: `ssh -N -L 5901:localhost:5900 ...` y
`vncviewer localhost:5901`. Al cerrar el `ssh` la sesión VNC se corta y el `vncd` sigue
esperando otra; abrir un túnel nuevo basta para volver a entrar.

Por defecto `vncd` **solo acepta conexiones que vengan de la propia máquina** (127.0.0.1). Para entrar directamente por la red local
(sin túnel; el tráfico VNC **no va cifrado**, solo la contraseña va protegida por el reto):

```
echo escuchar=red-local > /home/.config/vncd.conf
servicio reiniciar vncd
vncviewer 192.168.1.50:5900
```

## Qué hay y qué no

| | |
|---|---|
| Protocolo | RFB 3.8 (y 3.7, 3.3) |
| Seguridad | «VNC Authentication» (DES del reto de 16 bytes con la contraseña, bits de cada byte invertidos, como pide la RFC). «None» solo con `seguridad=ninguna` |
| Codificaciones | **ZRLE** (flujo zlib continuo), **Hextile**, **Raw**; pseudocodificación **DesktopSize** (cambios de resolución). El servidor usa la primera de ZRLE, Hextile y Raw que el cliente ofrezca en su lista |
| Formatos de píxel | color verdadero de 8, 16 y 32 bits, little o big endian (SetPixelFormat). Sin paleta |
| Pantalla | `capturar_tela` (D125), comparada por bloques de 16 x 16 con una suma; solo se envía lo que cambió. Hasta 1920 x 1200 |
| Entrada | KeyEvent (keysyms de X11 a las teclas de SaguiOS) y PointerEvent (posición absoluta, botones, rueda) con `inyectar_entrada` (`SIS_INYECTAR`) |
| No hay | CopyRect, cursor remoto (el cliente pinta el suyo), portapapeles (ClientCutText se descarta), Tight, cifrado (usar `ssh -L`), varias pantallas |

## Seguridad

- **Desactivado de serie** (`arrancar=no` en `/etc/servicios/vncd.conf`).
- **Solo desde la propia máquina** por defecto. `escuchar=red-local` añade las direcciones
  privadas y la subred de la tarjeta (la misma lógica que el `sshd`).
- **Contraseña**: `/home/.config/vncd.passwd` (o `/etc/vnc/vncd.passwd`). Sin contraseña nadie
  entra (el servidor lo dice al conectar y en el registro). DES con 8 caracteres es débil: por
  eso el túnel SSH es el modo normal de uso.
- **Fuerza bruta**: una contraseña mala espera un segundo antes de contestar; tras
  `bloqueo_fallos` conexiones seguidas fallidas (3), esa dirección se ignora
  `bloqueo_segundos` (60) segundos. Máximo `max_clientes` (2) a la vez.
- **Entrada inyectada** solo para este servicio (D251): el kernel concede `INY_ESTADO` únicamente a
  `/bin/vncd` lanzado por el gestor de servicios, y a un proceso a la vez. Por eso el permiso lo
  pide **quien escucha**, y las sesiones (hijas suyas) le mandan lo que hay que inyectar por una
  tubería (su salida estándar, mensajes de 12 bytes). Si el kernel lo niega (p. ej. `vncd` lanzado
  a mano desde el Terminal), `vncd` sigue en **solo ver** y lo anota. Al cerrar una sesión se
  sueltan los modificadores y los botones.
- Cada conexión sale en pantalla (aviso, `aviso=no` lo quita) y en `/logs/vncd.log`. La
  contraseña nunca se anota.

## Opciones

En `/etc/vnc/vncd.conf` y `/home/.config/vncd.conf` (gana la segunda); `clave=valor`:

| Clave | Valor de serie | |
|---|---|---|
| `puerto` | 5900 | puerto TCP |
| `escuchar` | `local` | `local` o `red-local` |
| `solo_ver` | `no` | el cliente mira pero no toca |
| `max_clientes` | 2 | 1 a 4 |
| `bloqueo_fallos`, `bloqueo_segundos` | 3, 60 | |
| `tiempo_entrada` | 30 | segundos para autenticarse |
| `fps_max` | 12 | actualizaciones por segundo como mucho (1 a 30) |
| `aviso` | `si` | aviso en pantalla al entrar alguien |
| `seguridad` | clave | `ninguna` quita la contraseña (solo pruebas) |

## Cómo funciona

- **Procesos**: quien escucha lanza un `vncd --sesion IP PUERTO` por conexión, con el socket
  como entrada, como el `sshd`. Un fallo en una sesión no tumba el servicio.
- **Cambios**: cada captura se divide en bloques de 16 x 16; una suma (FNV) por bloque se
  compara con la anterior. Los bloques sucios se juntan en rectángulos (rachas horizontales
  fundidas hacia abajo) y solo se envían los que caen dentro de la última petición del cliente.
  Si la pantalla está quieta, la cadencia de captura baja hasta 4 por segundo; con cambios, sube
  a `fps_max`. Una petición incremental sin cambios no recibe respuesta (el cliente espera).
- **ZRLE**: baldosas de 64 x 64 con color sólido, paleta empaquetada, RLE llano o RLE con
  paleta, el que menos ocupe; todo el rectángulo pasa por un **flujo zlib que se mantiene
  abierto** durante la conexión (`zflujo_*` en `userland/libsagui/deflado_continuo.c`: ventana
  de 32 KB, Huffman fijo, vaciado de sincronización por rectángulo). El cliente usa un único
  inflate; por eso las repeticiones entre rectángulos casi no ocupan.
- **Hextile**: baldosas de 16 x 16 con fondo y subrectángulos de color; baldosa cruda si no
  compensa.
- **DES**: `userland/libsagui/des.c`, con las tablas del estándar; se prueba con vectores
  conocidos y contra `cryptography` de Python.

## Teclado y ratón

Los keysyms de X11 se traducen a las teclas de SaguiOS (`vnc_tecla`): Latin-1 tal cual,
Unicode directo solo si cabe en Latin-1, Enter, Retroceso, Tab, Esc, flechas, Inicio/Fin,
Re Pág/Av Pág, Insertar, Suprimir, F1-F12 y la tecla Windows. Mayús, Ctrl y Alt (sin AltGr ni teclas muertas: el cliente manda el carácter final)
no son teclas aparte: van en el campo de modificadores (`INY_MOD_*`) de cada tecla. Los
botones 4 y 5 del ratón (rueda) llegan como `INY_RUEDA`. Lo que se inyecta pasa por el mismo
camino que el hardware real (lo implementa el kernel, ver `SIS_INYECTAR` en `abi.h`).

## Pruebas

`tests/h250_vnc.py` (y en el anfitrión `tests/vnc_unidad.c`): DES, deflate continuo contra
`zlib` de Python, Raw/Hextile/ZRLE contra decodificadores de Python en 6 formatos de píxel, y
en QEMU un cliente RFB propio (`tests/rfb_cliente.py`): versiones, contraseña buena y mala,
bloqueo, máximo de clientes, pantalla completa igual a la de QEMU, incremental, formatos de 16 y
8 bits, rendimiento y rechazo desde fuera con la configuración por defecto.

`tests/h252_vnc_tunel.py` hace el camino real con la configuración de serie: `vncd --clave`,
`servicio activar/iniciar sshd` y `vncd`; una conexión directa al 5900 se rechaza; `ssh -N -L`
del OpenSSH del anfitrión; por el túnel, contraseña, pantalla completa igual a QEMU,
**teclado y ratón reales** (una orden en el Terminal, Ctrl+C a un `sleep`, abrir Archivos, un
clic que devuelve el foco, Alt+F4), incremental; al cerrar el `ssh` el VNC se corta y un túnel
nuevo vuelve a entrar. Visor real (`vncviewer`, TightVNC...): no había ninguno en el anfitrión de
pruebas (WSL2), así que solo se ha probado con el cliente RFB de Python.

## Rendimiento: coste en el escritorio

Medido por `h252` en QEMU con KVM (1024 x 768, ZRLE), con el Terminal ejecutando `bench mem` y
`bench gui` sin visor y con un visor VNC por el túnel pidiendo actualizaciones sin parar (dos rondas
de cada; el visor recibió unas 260 actualizaciones y ~220 KB por ronda):

| | sin visor | con visor |
|---|---|---|
| `bench mem`, memcpy de 1 MB (MB/s) | 75 971 | 73 484 (97 %) |
| `bench gui`, redibujar y mostrar (cuadros/s) | 3 916 | 4 465 (114 %, dentro del ruido) |
| `bench gui`, solo mostrar (cuadros/s) | 5 694 | 7 355 (129 %, ruido: 5 755 y 8 877 en las dos rondas con visor) |

Un visor conectado no se nota en el escritorio con KVM (menos de un 5 % en memoria, nada medible
en dibujado: la captura de pantalla se hace a 12 por segundo como mucho y la pantalla quieta baja a
4). La prueba exige solo que no sea más del doble de lento. Sin KVM (TCG) y en las máquinas
reales los números serán otros: hay que repetirlo allí.

## Límites conocidos

- El servidor no usa CopyRect: mover una ventana reenvía su contenido (con ZRLE y el diccionario compartido,
  barato). Sin pseudocodificación de cursor: el cursor lo pinta el cliente (el cliente `vnc` de SaguiOS dibuja el suyo).
- Pantallas de más de 1920 x 1200 no se capturan.
- Solo se atiende la pantalla compuesta (como `captura`): una aplicación a pantalla completa
  con aceleración directa de la GPU puede no verse.
- El portapapeles no se comparte.
- El cliente `vnc` (fase 2) está más abajo.

# Cliente `vnc` (mejora 097 fase 2, D254)

`vnc` abre una ventana con el escritorio de un servidor VNC: otro SaguiOS con `vncd`,
TightVNC, TigerVNC, RealVNC, x11vnc... Código propio a partir de la RFC 6143 y la
especificación pública de Tight (nada portado de TightVNC ni de otro cliente).

```
vnc [-p puerto] [--solo-ver] [--pantalla-completa] [--color 8|16|32] [--calidad 0-9]
    [--clave-fichero F] [--uno-a-uno] host[:display]
```

`host:1` es el puerto 5901 (`host::5900` da el puerto). Está en el menú Inicio, en Internet
(«Escritorio remoto (VNC)»), y en la ayuda del terminal (`ayuda vnc`). Para entrar en un
SaguiOS con `vncd` por el túnel: `ssh -L 5900:localhost:5900 root@IP` y después
`vnc localhost`.

| | |
|---|---|
| Protocolo | RFB 3.8, 3.7 y 3.3 (contesta con la menor que entiende) |
| Seguridad | «None» y «VNC Authentication» (usa la segunda si el servidor ofrece las dos). La contraseña se pide en un cuadro de la ventana (escondida con asteriscos) o se lee de un fichero con `--clave-fichero` (la primera línea). Si el servidor la rechaza, la ventana lo dice y no se reintenta sola |
| Codificaciones | **Tight** (zlib en sus cuatro flujos con reinicios, filtros copy, palette de 2 y de hasta 256 colores y gradient, relleno y JPEG), **ZRLE**, **Hextile**, **CopyRect**, **Raw**; pseudocodificaciones **DesktopSize**, **Cursor** y LastRect. Las pide en ese orden: el servidor usa la primera que sepa. Sin PNG de Tight |
| Formato de píxel | 32 bpp (0x00RRGGBB) de serie; `--color 16` (565) y `--color 8` (332) para redes lentas |
| JPEG | solo con `--calidad N` (0 a 9): sin ella el servidor manda Tight sin pérdida. El JPEG lo decodifica `imagem_ler_jpeg` (baseline) |
| Teclado | los códigos de SaguiOS (Latin-1 y `TECLA_*`) a keysyms de X11, con Ctrl y Alt (Mayús solo hace falta fuera de las letras: la mayúscula ya va en el carácter). SaguiOS no da eventos de tecla soltada: cada pulsación se manda como pulsar y soltar |
| Ratón | posición, botón izquierdo, derecho y rueda (botones 4 y 5 del RFB) |
| Escalado | si el escritorio remoto es mayor que la ventana se reduce (bilineal) sin pasar del 100 %; si es menor, se centra. El botón «1:1» (o `--uno-a-uno`) lo enseña sin escalar y la vista se desplaza acercando el puntero a un borde. Los clics se traducen a la posición remota |
| Pantalla completa | `F11`, como en el resto de ventanas del sistema (o `--pantalla-completa`); la barra de abajo desaparece |
| Cursor | si el servidor manda la forma del cursor (Cursor), se dibuja esa y se oculta el puntero local sobre la ventana; si no, se ve el del sistema |
| Portapapeles | del servidor al cliente (`ServerCutText` va al portapapeles del sistema); al revés no |
| Red lenta | el bucle no se bloquea esperando: lee lo que haya (`disponivel`), guarda lo recibido y descodifica cuando el mensaje está entero; Raw y Hextile avanzan por filas y baldosas. Solo conectar (resolver y abrir el TCP) espera, hasta 5 s |
| Si se corta | franja roja en la ventana con el motivo; `R`, Intro o el botón «Reconectar» vuelven a conectar |

La barra de abajo trae «1:1» / «Ajustar», «Ctrl+Alt+Supr» (manda esa combinación al servidor) y «Reconectar», y el tamaño remoto, el escalado y los KB recibidos. `--solo-ver` no manda
ni teclado ni ratón. El registro del sistema (`dmesg`) lleva líneas `vnc: ...` (conexión, versión,
formato, la primera vez que se descodifica cada codificación).

## Cómo está hecho

- `userland/programas/vnc_decod.h`: todo lo que no toca el sistema, para probarlo en el
  anfitrión: formato de píxel del servidor, los decodificadores de cada codificación (devuelven
  los bytes consumidos, 0 si faltan datos y < 0 si el servidor manda algo inválido; un
  rectángulo ZRLE o Tight no toca la pantalla hasta tenerlo entero), Cursor y los keysyms.
- `userland/libsagui/inflado_continuo.c`: `zinflar_*` (en `compressao.h`), el lector del flujo
  zlib continuo: un inflate propio (tablas rápidas de 9 bits) cuya ventana de 32 KB pasa de un
  trozo al siguiente; es la pareja de `zflujo_*` del servidor.
- `userland/programas/vnc.c`: la ventana (libsagui), el apretón de manos como máquina de
  estados sobre un búfer de recepción, el bucle y la entrada.

## Límites conocidos (cliente)

- Sin PNG de Tight, sin ExtendedDesktopSize, sin portapapeles del cliente al servidor, sin
  cifrado propio (RFB va en claro: usar el túnel SSH; la contraseña DES es de 8 caracteres).
- Sin eventos de tecla soltada en el sistema: no se puede mantener una tecla apretada en el
  servidor (las repeticiones salen como pulsaciones sueltas); los atajos con Ctrl o Alt sí.
- AltGr y las teclas muertas dependen de lo que SaguiOS entrega ya compuesto (Latin-1); un
  carácter fuera de Latin-1 no se puede escribir.
- El escritorio remoto se guarda entero en memoria (0x00RRGGBB): hasta 8192 x 8192 y 8 millones
  de píxeles (32 MB).
- Conectar espera hasta 5 s y la ventana no atiende eventos mientras (después todo es sin bloqueo).

## Pruebas del cliente

`tests/h253_vnc_cliente.py` y, en el anfitrión, `tests/vnc_cliente_unidad.c` (con y sin
ASan/UBSan): inflado continuo contra `zlib` (flujo único, niveles 0, 1, 6 y 9); los
decodificadores con imágenes de 8 formas y 6 formatos de píxel, con los datos en trozos de 1, 7
y 100 bytes, truncados y corruptos; CopyRect con solape; Tight con rectángulos seguidos en los
mismos flujos y con reinicios; el JPEG de `tests/datos/vnc_prueba.jpg` (generado una vez con
ffmpeg) contra la salida de la libsagui y contra la referencia. En QEMU, un servidor RFB mínimo
en Python (10.0.2.2) que manda una imagen conocida con todas las codificaciones y se compara con
la captura de pantalla de QEMU píxel a píxel; teclas y clics hacia el servidor; cursor;
DesktopSize; caída y reconexión; contraseña mala y pedida en la ventana; 3.3 sin seguridad, 3.7,
`--color 16`/`8`, `--solo-ver`; escritorio de 1200 x 900 escalado; y contra el `vncd` de la misma
máquina.
