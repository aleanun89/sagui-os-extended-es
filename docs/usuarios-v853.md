# Usuarios en la v8.5.3: auditoría y diseño (D371, propuesta pendiente de decisión)

Pedido del mantenedor: «Usuario root (SIEMPRE EXISTE) y luego alguna manera de hacer login, que cada usuario
tenga su directorio… Saber bien los permisos de usuario y configuraciones de usuario vs configuraciones de
sistema», sobre todo **instalado** (no en vivo). Añadido después: **root con su carpeta en `/root`**, como en Unix.

Este documento es la **fase 1** (auditoría y diseño). Lo único que ya se cambió son cuatro fallos de seguridad
entre cuentas (sección 2.1, prueba `tests/h371_usuarios_seguridad.py`). El resto espera a las respuestas de la
sección 5.

Base: rama `v8.5.3-usuarios` sobre `6658fc8d`. Lo comprobado en QEMU se marca **[QEMU]**. Lo leído en el código
lleva `fichero:línea`; las líneas pueden moverse un poco con los cambios de esta rama.

---

## 1. Estado real hoy

### 1.1 Cuentas

| Cuenta | uid | Carpeta en `/etc/usuarios` | Qué ve como `/home` | Contraseña |
|---|---|---|---|---|
| `root` | 0 | `/home` (`rootfs/etc/usuarios:6`) | el árbol real: `/home` es el de sagui | Ninguna de serie. Se elige la primera vez que alguien de la sesión local usa `admin`, `su`, `passwd root` o el diálogo de permisos (D255, `servicios.c` `tratar_pedido`). El instalador la pide, pero es opcional («sin contraseña todavía», `instalar_asistente.h:753`). |
| `sagui` | 1000 | `/home` | `/home` tal cual (`casa_conta` vacía, D340.2) | Ninguna de serie; el instalador la pide, también opcional. |
| `servicio` | 100 | `/` | `/home` tal cual | No entra en el escritorio. |
| Cuentas nuevas (`ana`) | 1001+ | `/home/<nombre>` 0700 | su carpeta (vista por proceso, D340.1) | Al crearla se elige (o «sin contraseña»). |

- **`root` y `sagui` existen siempre:** si falta `/etc/usuarios` o la línea está mal, se usan las de serie
  (`contas.c`, `sessao.c` `carregar_contas`). Root nunca sale en la pantalla de inicio de sesión: solo cuentas
  con uid 1000-59999 y shell (`sessao.c:137`).
- **Grupo `admin`** (`/etc/grupos`, D340.6): sus miembros pasan a root con **su propia** contraseña. Vacío de
  serie: `sagui` no es administrador.
- **Las contraseñas** las comprueba el gestor de servicios (proceso root, `servicios.c`), con PBKDF2-SHA256 y
  20 000 vueltas. Están en `/etc/sombra` (0600), en SAGUI-CONF.

### 1.2 Cómo se entra

| Modo | Qué hay | Cómo se entra | Root |
|---|---|---|---|
| **En vivo de verdad** (CD, `--sin-conf`, disco sin SAGUI-CONF) | `/etc` en la RAM: nada de usuarios se guarda | Escritorio directo como `sagui` | **Sin contraseña.** `admin` funciona sin pedirla desde la sesión local (`servicios.c`, rama `USU_EST_EN_VIVO`) y no funciona por SSH. Una bandeja lo avisa. |
| **Imagen publicada** (`saguios.img` grabada en un USB) | **Lleva SAGUI-CONF** (mkimagem la pone de serie, D256) y `/home` en ext2 | Igual que «instalado» | La primera vez se elige. **No es un modo «vivo»** aunque la gente lo use así. |
| **Instalado** (instalador, D149/D256/D318) | Particiones: arranque, `/home` (SAGUI-DATOS, ext2), SAGUI-CONF 16 MB, juegos y apps | Una sola cuenta sin contraseña: directo. Si no, pantalla de inicio de sesión (`kernel/gui/sessao.c`): lista de cuentas, contraseña, Apagar y Reiniciar | La que se puso al instalar, o la primera que se elija. |
| **Consola de texto** (`--console`, o sin modo gráfico) | Lo mismo | **No hay inicio de sesión:** entra `sagui` (D340, límites) | — |
| **Modo rescate** (Ctrl al encender) | Consola, sin servicios | Shell de **root sin contraseña** | Puede cambiar la contraseña de root. |
| **Imágenes de prueba** (`--sesion-root`) | — | La sesión es root | — |

- **Cerrar sesión o cambiar de usuario:** desde Inicio. Se cierran todos los programas de la sesión y vuelve la
  pantalla. No hay sesiones a la vez.
- **Bloqueo de pantalla:** no existe. El «bloqueo» de D218 es el de publicidad por DNS.

### 1.3 Qué ve cada proceso en `/home`

`permisos_casa_traduzir` (`permisos.c:366`) se aplica a lo que llega por llamada al sistema
(`arquivos.c:178`, `permisos.c:509`, `carregador.c:275`). Si el proceso no es root y su líder tiene
`casa_conta`, cambia `/home/...` por `casa_conta/...`.

- **Lo que hereda `casa_conta`:**
  - Lo que lanza el kernel para la sesión (Terminal, menú, escritorio, `configuracion --aplicar --sesion`, widgets) recibe la carpeta de la sesión (`carregador.c:190`).
  - Lo que lanza el kernel con otra identidad (el gestor de servicios, `configuracion --aplicar` al arrancar) recibe `casa_conta` vacía: ve el `/home` real (`carregador.c:185`).
  - Los hijos heredan la del padre (`carregador.c:184`). Por eso `su ana` desde la sesión de sagui da una ana **sin** carpeta: ve el `/home` real (D340, límites).
- **Root y los programas elevados** (diálogo de D269, `admin`, `su`) ven el árbol real, y su carpeta efectiva es `/home`, es decir, la de sagui. `permisos_casa_mudar_uid` (`permisos.c:376`) traduce la carpeta actual al subir y al bajar de root.
- **El kernel**, cuando lee o escribe por su cuenta (`vfs_ler_inteiro`, `escrever_atomico`), usa la ruta literal: un `/home/...` en el kernel es **siempre el de sagui**, salvo lo que monta con `permisos_sessao_caminho()`.
- **Dueños en `/home`** (D282.7, `vfs.c:395`): lo que es de root dentro de `/home` se ve como de sagui. El propio `/home` es de sagui, 0755: en la RAM (`vfs.c:67`) y como punto de montaje (`montagem.c:234`).

### 1.4 Configuración: tabla por fichero

Columnas: quién lo escribe y quién lo lee (con qué identidad), y adónde debería ir: **U** = de la persona
(`~/.config`), **S** = del sistema (SAGUI-CONF `/etc`, solo root o el diálogo de administrador),
**S+U** = valor del sistema que la persona puede cambiar para su sesión.

| Fichero hoy | Escribe | Lee | Contenido | Debería |
|---|---|---|---|---|
| `/home/.config/sagui.conf` | `configuracion` (sesión); `ecualizador`, `mandos` (libsagui); `configuracion --aplicar` como **root** al arrancar (`configuracion.c:649`) | `configuracion --aplicar` como **root** al arrancar, del fichero de **sagui** (`principal.c:180`); por sesión, el de la cuenta (`--sesion`); `libfontes/texto.c:280`, `captura.c:27` | **Mezclado**, ver 1.5 | Partir en S y U |
| `/home/.config/acel-arranque` | `configuracion` (root, al arrancar) | ídem | Marca «el arranque anterior no llegó al escritorio» (D323) | S (`/sistema/conf` o `/var`) |
| `/home/.config/sonido.conf` | `sonido` (sesión) | `configuracion --aplicar` (root, al arrancar) | Conectores del códec por modelo | S |
| `/home/.config/pantallas.conf` | **el kernel** (`telas.c:207`, `TELAS_OP_GUARDAR`, llamada libre) | el kernel al arrancar (`telas.c:223`) | Espejo/extender, resoluciones | S (ver P6) |
| `/home/.config/escritorio.conf` | el kernel (`area_trabalho.c:379`), con el dueño de la cuenta | el kernel por sesión (`area_sessao_nova`) | Iconos del escritorio | U (bien) |
| `<casa>/vps` | el kernel | el kernel (`barra.c:1419`), por sesión | Los más usados del menú | U (bien) |
| `/home/.zona` (luego `/etc/zona`) | `zona` (se eleva, `zona.c:58`) en el `/home` real | **el kernel** (`hora.c:203`) | Zona, RTC UTC/local, 12/24 h | S. Hoy sagui puede escribir `/home/.zona` sin ser root |
| `/home/.frecuencia` (luego `/etc/frecuencia`) | `frecuencia --guardar` (quien lo ejecute, sin elevar) | **el kernel** (`frequencia.c:542`) | Política de CPU, límite térmico | S. Hoy sagui la cambia sin ser root; otra cuenta escribe en su carpeta y no vale |
| `/home/.config/bloqueo/*` | `bloqueo` (sesión) | **el kernel** (`bloqueo.c:53,98`) y `BLOQ_RECARGAR` | Filtro DNS de toda la máquina | S. Hoy sagui lo apaga sin ser root; otra cuenta escribe el suyo y no vale |
| `/home/.config/wifi.conf` | `wifi` (sesión) | el kernel con `permisos_sessao_caminho` (`wifi.c:1293`) | Redes y PMK de cada cuenta | U. Si se pide una red compartida, añadir una S (`/etc/wifi.conf`) |
| `/home/.config/wifi-desempenho.conf` | a mano | el kernel (`wifi.c:428`) | HT, colas, cifrado por hardware | S |
| `/home/.config/wireguard/*.conf` y `.clave` | `wg_ventana.h` (sesión) | `wg-quick` (sesión o root) | Túneles con clave privada | U + permiso. D363 deja encender el túnel a la sesión local |
| `/home/.config/debug-app/debug-app.conf` y `.log` | `debug-app` (**root**, elevado) | `debug-app` (root) | Puerto, minutos, IP permitidas | S. Hoy lo lee root de la carpeta de sagui; D362 ya ignora `publicas` en `--texto` |
| `/home/.config/vncd.conf` | a mano / `vncd --ajustar` | `vncd` (**root**) | Puerto, escuchar, seguridad… | S. **Fallo 2.1-D, corregido:** hoy solo `fps_max` |
| `/home/.config/vncd.passwd`, `/home/.ssh/ssh_host_ed25519_key` | migración antigua | `vncd`/`sshd` la pasan a `/etc` | Secretos | S (ya en `/etc`); en `/home` solo en vivo |
| `/home/.ssh/authorized_keys` | a mano / `ssh-keygen` | `sshd` (root) → entra como **sagui** | Claves de entrada | U, por cuenta (hoy solo sagui) |
| `/etc/ssh/authorized_keys` | a mano | `sshd` → entra como **root** | — | S (bien) |
| `/home/.config/servicios/*.conf`, `activados` | `servicio` (sesión) | gestor (root): solo los de **sagui**; corren como sagui | Servicios de la persona | U, por cuenta (hoy solo sagui) |
| `/etc/servicios/*.conf`, `activados` | rootfs / `servicio activar` (root) | gestor | Servicios del sistema | S (bien) |
| `/home/.config/crontab` | `crontab` (sesión: la de ana va a su carpeta) | `cron` (root): solo el de **sagui**, corre como sagui | Tareas | U, por cuenta (hoy solo sagui) |
| `/home/.config/cron/estado` | `cron` (root) | `cron` | Estado anacron | S (`/var/cron` o `/sistema/conf`) |
| `/etc/crontab` | rootfs | `cron` | — | S (bien) |
| `<casa>/.config/htoprc` | `htop` | `htop` | Columnas | U. **Fallo funcional:** `htop.c:250` monta `/home/ana/.config/htoprc` y el kernel lo traduce otra vez (`/home/ana/ana/...`): ana no guarda nada. Root usa el de sagui |
| `/home/.config/widgets.conf`, `widget-notas.txt` | `widgets`, `widget-notas` (sesión) | `widgets --arrancar` (sesión) | Widgets activos | U (bien). **Ojo:** lanza órdenes, es una vía de autoarranque |
| `/home/.config/conky.conf` | `conky`, `widgets` | `conky` (sesión) | Paneles con órdenes `exec` | U (bien), igual que lo anterior |
| `/home/.config/terminal.conf`, `colores.conf`, `dialogos.rec`, `visorpdf.conf`, `vnc.conf`, `buscaminas.conf`, `ia/`, `agentes`, `correo*`, `traductor`, `sintonia`, `dosbox.conf`, `recreativa*`… | cada programa | cada programa | Preferencias | U (bien) |
| `/home/.config/ampliar-home.no` | `ampliar-home` | servicio root | «No preguntar por ampliar /home» | S |
| `/home/fuentes/` + `fuente_*` en `sagui.conf` + `/tmp/fuentes.conf` | `fuentes`, `configuracion` | `libfontes` | Fuentes instaladas y elegidas | Fuentes del sistema en `/sistema` o `/apps` (S); las de la persona en `~/.local/fuentes` (U); la elección es U |
| `/etc/idioma` | `configuracion idioma X --sistema` (root) | `configuracion --aplicar` | Idioma del sistema | S (bien) |
| `idioma=` en `sagui.conf` | `configuracion` | `--aplicar` → `idioma_fijar` (global en el kernel) | Idioma de la persona | S+U (bien con una sesión a la vez) |
| `teclado=` en `sagui.conf` | `configuracion` | `--aplicar` (global) | Distribución | S+U. Hoy la pantalla de inicio de sesión usa el teclado de **sagui** |
| `/etc/usuarios`, `/etc/sombra`, `/etc/grupos` | `contas.c` (root) en `/etc` y SAGUI-CONF | kernel (`sessao.c`), gestor | Cuentas | S (bien) |
| `/etc/hostname`, `/etc/zona` (instalador) | instalador, `hostname` | kernel y programas | — | S (bien) |
| `/etc/fstab` | rootfs (nadie lo escribe) | kernel (`montagem.c:386`) | Montajes | S |
| `/etc/bluetooth.conf` | el kernel (`usbbt.c:105`) | el kernel | Emparejados | S (bien) |
| kconfig | bytes 192-255 de la cabecera (`kconf.c`) | kernel | Opciones del kernel | S (bien: `INFO_KCONF_GRAVAR` solo administrador, D353) |
| Red: DNS, IP fija | **nada se guarda** (`ifconfig`, `dhcp` no escriben ficheros) | — | — | S, cuando exista (`/etc/red.conf`) |
| `/home/.sagsh_historial`, `.benchmark`, `.certificados`, `.mico` | cada programa | cada programa | — | U |
| `/home/dos`, `/home/quake`, `/home/gnuboy`, `/home/openjazz`, `/home/redalert`, `/home/cnc` | ports | ports | Datos de juegos | U (con la vista de compatibilidad sigue igual) |

Rutas fijas `"/home` contadas con `grep -a` (son líneas, no rutas distintas): kernel 27 (unas 16 de configuración),
libsagui 30, otras bibliotecas 14, programas 144 (51 ficheros), IA 20, ports 52 (15 ficheros). En todo `userland/`
hay unas **102 rutas distintas**; 38 son el prefijo `"/home/.config"`.

### 1.5 `sagui.conf` por clave

- **De la máquina** (S):
  - `acel2d`, `gpu3d`, `sdl2_gpu` y `disco_dma_cache`. Se aplican **como root al arrancar, desde el fichero de sagui**; `disco_dma_cache` escribe además en la cabecera del disco.
  - `salida` (tarjeta de sonido), `latencia`, `balance`, `micro_entrada`, `micro_ganancia`, `micro_silencio` y `mando_puerto` (calibración).
  - `zona`, que para aplicarse pide elevación (`configuracion.c:356`).
- **De la persona, con un valor del sistema de partida** (S+U): `teclado`, `idioma`, `volumen`, `silencio` y `ecualizador`.
- **De la persona** (U):
  - `acento`, `fondo` y `fondo_modo`.
  - Las 18 claves de barra y menú: `barra_*`, `bandeja_*`, `inicio_*`.
  - `capturas_carpeta`, `capturas_sonido`, `fuente_documentos` y `fuente_titulos`.

---

## 2. Problemas y riesgos

### 2.1 Fallos de seguridad encontrados y **corregidos ya** en esta rama (h371)

**A. sagui podía quitarle la carpeta a otra cuenta y ponerle una suya** (`permisos.c`, `permisos_remover`)

- **El fallo:** `/home` es de sagui, así que sagui podía hacer `mv /home/ana /home/x` o `rmdir` aunque `/home/ana`
  fuera 0700: basta con poder escribir en la carpeta de encima. Después podía crear su propia `/home/ana` con
  permisos 0777 y dejar dentro `.config/widgets.conf`, `conky.conf` o servicios. Al entrar, ana corría lo que sagui
  hubiera preparado, y todo lo que ana guardara lo podía leer sagui.
- **El arreglo:** lo que cuelga directamente de `/home` solo lo borra o renombra su dueño (o root).
- **[QEMU]:** el `mv` y el `rmdir` de sagui se niegan con «carpeta de otra cuenta», y sus propios archivos de
  `/home` los sigue moviendo y borrando.
- **Efecto secundario:** `mv`, cuando se le niega el renombrado, prueba a copiar y deja una `/home/x` vacía. No es
  grave; ver 2.2-12.

**B. El kernel entraba en una carpeta personal que no era de la cuenta** (`sessao.c`, `preparar_casa`)

- **El fallo:** «si existe, no se toca». Con `/home` en la RAM (SAGUI-CONF sí, partición `/home` no), la carpeta
  de ana no existe al arrancar, así que sagui podía crearla antes que ella.
- **El arreglo:**
  - El kernel solo entra si la carpeta es de la cuenta, está en un sistema de archivos que guarda dueños y nadie más puede escribir en ella (`modo & 022`). Si no, vuelve la pantalla con «Tu carpeta personal no es tuya o otros la pueden cambiar: avisa al administrador.».
  - Si falta `/home`, la crea. Antes, en la RAM no existía y `/home/ana` no se creaba (error -2): ana entraba sin carpeta.
- **[QEMU]:** con una `/home/ana` puesta por sagui no se entra. Sin ella, se crea de ana con 0700 y se entra.

**C. Otra cuenta podía ponerle la primera contraseña a sagui y quedarse con su cuenta** (`servicios.c`, `tratar_pedido`)

- **El fallo:** la regla «sin contraseña, la elige la sesión local» de D255 valía para **cualquier** cuenta de
  destino. Desde la sesión de ana, `su sagui` o `passwd sagui` pedían «elige una» (sagui de serie no tiene), ana
  pasaba a ser sagui y además le cambiaba la contraseña, así que entraba como sagui en la pantalla y leía todo su
  `/home`.
- **El arreglo:** la primera contraseña de una cuenta la pone ella misma o root. La de root la sigue pudiendo
  elegir la sesión local, como en D255 (ver P3).
- **[QEMU]:** `su sagui` y `passwd sagui` desde ana se niegan con «solo ella misma o root se la ponen», y ana sí
  elige la suya.

**D. Desde su carpeta, sagui podía quitarle la contraseña al VNC y abrirlo a la red** (`vncd.c`)

- **El fallo:** `vncd` corre como root y enseña el escritorio de quien haya entrado. Leía `/home/.config/vncd.conf`,
  que escribe sagui sin ser administrador, y ese fichero **ganaba** a `/etc/vnc/vncd.conf`. Con
  `seguridad=ninguna` y `escuchar=red-local` (o `aviso=no`), el escritorio de cualquier cuenta quedaba abierto a la
  red local sin contraseña en cuanto el administrador encendía el servicio.
- **El arreglo:** con SAGUI-CONF, de ese fichero solo vale `fps_max`; en vivo, sigue valiendo entero.
- **Cambios que arrastra:** `h250` pasa a escribir `/etc/vnc/vncd.conf`, y se actualizan `docs/vnc.md` y el
  comentario de `rootfs/etc/vnc/vncd.conf`.

**E. Wi-Fi: otra cuenta se conectaba sola con las redes de sagui** (`wifi.c`, `ruta_config`)

- **El fallo:** si la cuenta no tenía `wifi.conf`, el kernel usaba el de sagui (claves incluidas). No es una fuga de
  la clave, pero contradice D354 («una sesión nueva no usa las claves de otro»).
- **El arreglo:** ahora usa solo el de la sesión.
- **Sin probar en QEMU:** no hay pincho Wi-Fi emulado; para sagui la ruta no cambia.

### 2.2 Problemas que quedan (para el diseño)

1. **Configuración de la máquina en la carpeta de sagui, aplicada como root o por el kernel.** sagui cambia sin
   ser administrador cosas que la tabla de D255 reserva al administrador:
   - la aceleración 2D/3D y la caché del disco: `sagui.conf` se aplica como root al arrancar (`configuracion.c:601-660`, `principal.c:180`);
   - la frecuencia: `/home/.frecuencia` → `frequencia.c:542`;
   - el bloqueo DNS: `bloqueo.conf` → `bloqueo.c:53`;
   - la zona: `/home/.zona` → `hora.c:203`;
   - los conectores de sonido: `sonido.conf`;
   - el Wi-Fi: `wifi-desempenho.conf`.

   El riesgo va de bajo a medio. La aceleración forzada puede colgar la GPU (D346), aunque la marca de arranque la
   restablece.
2. **Con varias cuentas, esa configuración de la máquina solo la cambia sagui:**
   - ana, desde Configuración, escribe la suya en su carpeta y no vale (frecuencia, bloqueo; también la aceleración, que `--sesion` salta);
   - `pantallas.conf` lo escribe el kernel en la carpeta de sagui sea quien sea quien guarde (`telas.c:207`, llamada libre);
   - la pantalla de inicio de sesión usa el teclado de sagui.
3. **sagui = `/home` entero (D340.2).**
   - sagui ve los nombres de las demás cuentas y es dueño de `/home`; el fallo A venía de ahí, y queda cerrado solo por la regla nueva.
   - Lo de root dentro de `/home` se ve como de sagui (D282.7).
   - Si mañana sagui deja de ser la cuenta principal (renombrada o borrada; hoy está protegida), `/home` sigue siendo «suyo».
4. **Root no tiene carpeta propia.** Su `~` es el `/home` de sagui:
   - `su` pone `HOME=/home` (`su.c:39`);
   - un programa elevado que guarda preferencias (`debug-app`, `htop` como root, el Terminal de `su`) escribe en la configuración de sagui, con dueño root, que D282.7 disfraza de sagui.
5. **SSH, cron y servicios de usuario solo para sagui.**
   - `sshd` lee `/home/.ssh/authorized_keys` del `/home` real (el de sagui) y entra como sagui.
   - `cron` y el gestor leen el `crontab` y los servicios de sagui.
   - El `crontab` y el `servicio` de ana escriben en su carpeta y **nunca corren**, sin avisar.
6. **Root en vivo, y root sin contraseña en una imagen con SAGUI-CONF.**
   - En vivo, cualquier programa de la sesión local pasa a root sin contraseña. Es así por diseño, pero no hay ninguna barrera.
   - Con SAGUI-CONF y sin contraseña de root (instalación sin ella, o la imagen grabada en un USB), **cualquier programa** de la sesión local puede elegir la contraseña de root antes que la persona y quedarse con la máquina (D255, «la elige quien está delante»).
   - Si la línea de root en la sombra se estropea, pasa lo mismo: «se trata como sin contraseña».
7. **No hay inicio de sesión en la consola** (`--console`, máquinas sin modo gráfico). Entra sagui sin contraseña
   aunque la tenga: una instalación en modo texto **no tiene contraseña de entrada**.
8. **No hay bloqueo de pantalla con contraseña.**
9. **Wi-Fi y VPN** (D354, D363): conectar o encender un túnel es de cualquier programa de la sesión local. Es un
   riesgo ya anotado: un programa de la sesión puede sacar el tráfico por un túnel suyo. Con varias cuentas es
   aceptable (es la persona que está delante). Lo que falta es que un túnel o una red **de la máquina** (para todas
   las cuentas, o antes de entrar) los configure solo el administrador.
10. **`su ana` desde sagui da una ana sin carpeta** (ve el `/home` real), y `HOME` sale de `/etc/usuarios`, que
    para la vista es incoherente.
11. **Pocas cuentas y poca lista:** como mucho 8 en la pantalla y 16 en total; un solo grupo por cuenta (más el de
    `admin`).
12. **Detalles:**
    - `htop` no guarda nada para otras cuentas (doble traducción).
    - `mv` copia si se le niega el renombrado: debería copiar solo entre sistemas de archivos distintos (`ERRO_*` de otro dispositivo).
    - `rmdir` de root no sabe si la carpeta es la actual de un proceso de otra cuenta (D340).
13. **La partición SAGUI-CONF es de 16 MB:** basta para `/etc`, no para una carpeta de root con datos.

---

## 3. Propuesta de diseño (sistema instalado bien montado, estilo Unix pero sencillo)

### 3.1 Cuentas y carpetas

| Cuenta | Carpeta real | Dueño, modo | Dónde vive |
|---|---|---|---|
| `root` (siempre existe) | `/root` | root, 0700 | **Instalado:** la carpeta `raiz/` de SAGUI-CONF, vista en `/root` (ver 3.2). **En vivo:** en la RAM. |
| `sagui` y las demás personas | `/home/<nombre>` | la cuenta, 0700 | Partición `/home` (ext2). En vivo, la RAM. |
| `servicio` y las de servicio | ninguna (`/`) | — | — |
| `/home` | — | **root, 0755** (hoy es de sagui) | — |

- **Vista de compatibilidad** (la de D340, extendida a **todas** las cuentas de persona, también sagui): un
  programa que no es root y pide `/home/...` llega a `/home/<su cuenta>/...`. Así las ~100 rutas fijas siguen
  valiendo sin tocar ningún programa. El mecanismo ya está probado (h340).
- **Además, `HOME` en el entorno de cada proceso**, con la ruta que el programa ve (`/home`), y una función de la
  libsagui `carpeta_personal()`. Los programas se pasan poco a poco a `HOME`. La vista solo se podría quitar
  cuando no quede ninguna ruta fija; puede no quitarse nunca.
- **Root** ve el árbol real (`/home` con las carpetas de todos) y su `~` es `/root`. Su configuración va en
  `/root/.config`.

### 3.2 Root y `/root` (indicación del mantenedor)

**Dónde se guarda.** La raíz es un ramdisk y no se guarda nada en ella. Opciones:

| Opción | Ventajas | Pegas |
|---|---|---|
| **(a) `raiz/` dentro de SAGUI-CONF, vista en `/root`** | Una partición que ya existe, es ext2 (guarda permisos), es de root y no se monta en FAT32. Existe sin partición `/home`, como en Unix: `/root` no depende de `/home`. | 16 MB: hace falta un límite (p. ej. 4 MB para `/root`, comprobado al escribir) o agrandar SAGUI-CONF a 32-64 MB en las instalaciones nuevas. Lo que ocupe `/root` compite con la sombra (un disco lleno no puede dejar sin guardar una contraseña: el límite lo evita). |
| (b) `/home/.root` (o `/home/root`) en la partición `/home` | Espacio | No hay `/home` en vivo ni en las máquinas sin esa partición, y `/home` puede ser FAT32 (sin permisos). |
| (c) Una partición SAGUI-ROOT nueva | Limpia | Una partición más en el instalador y en mkimagem, sin necesidad real. |

Recomendación: **(a)**, con un límite de 4 MB.

**Cómo se ve en `/root`.** El VFS no tiene enlaces ni montajes de una subcarpeta. Hay dos maneras:

1. un alias fijo en el kernel, `/root` → `/sistema/conf/raiz`, en el mismo sitio que la traducción de D340 y para todos los procesos;
2. un montaje con subcarpeta.

Recomendación: el **alias**. Es una comparación de prefijo, como `casa_conta`. Hay que tener en cuenta que
`/sistema/conf` es 0700 y que `permisos_ruta` niega `/sistema/conf` a quien no es root; con `/root` 0700 da igual.

**En vivo**, `/root` está en la RAM (0700), creada por el kernel al arrancar.

**Los programas elevados.**

- Ahora mismo, el diálogo, `admin` y `su` ven el árbol real y su `~` es el `/home` de sagui.
- Opciones:
  1. `HOME` de la cuenta que eleva (como `sudo` antiguo);
  2. `HOME=/root` para todo lo elevado (como `sudo -H` o `su -`);
  3. mixto: **`su` (un shell de root) → `/root`**; **`admin orden` y el diálogo de permisos → la vista y el `HOME` de la cuenta que lo pidió**.
- **Recomendación: la 3.** Con el diálogo se eleva **un programa de la persona** durante un momento (Configuración, Discos): sus preferencias deben seguir siendo las de la persona. `su` es «hacerse root»: las de root.
- **Lo que hace falta en el kernel para la 3:**
  - Hoy `permisos_casa_traduzir` no traduce a root (`l->uid == 0`). Con la 3, un proceso elevado que conserva `casa_conta` seguiría traduciendo **solo las rutas con punto**: `/home/.config`, `/home/.ssh`, `/home/.sagsh_historial`… Las demás de `/home` se quedan en el árbol real, porque las herramientas de administración (Usuarios, `chown`) necesitan ver `/home/ana`. Es una regla más, en el mismo sitio.
  - Lo que el elevado crea en `/home/<cuenta>/.config` se pasa a la cuenta. D282.7 se generaliza: dentro de `/home/<cuenta>`, lo de root se ve como de la cuenta. Así no quedan ficheros de root en la carpeta de la persona.
- **Más sencillo y también válido:** «elevado = árbol real y `HOME=/root`» (la 2). Las preferencias de los programas elevados irían a `/root/.config` y la persona no las vería. Se puede aceptar si se prefiere no tocar la traducción. Va a la pregunta P2.

### 3.3 Inicio de sesión

- **Gráfico**: el de D340.
  - Root **no** sale en la lista (como en GNOME o Windows). Se entra como root con `su`, el diálogo o el modo rescate. Opcional: «Otra cuenta…» para escribir un nombre, también root.
  - **Inicio automático:** opción del sistema (S), «entrar solo como `<cuenta>`». De serie, en vivo sí (sagui) y en instalado no (ver P5).
- **Consola de texto:**
  - un `login` (programa del sistema lanzado por el kernel en lugar del shell, que comprueba con el gestor como `PED_ENTRAR`) con nombre y contraseña;
  - en vivo, directo;
  - el modo rescate sigue igual.
- **Cambiar de usuario y cerrar sesión:** como en D340 (una sesión a la vez).
- **Bloquear la pantalla** (Win+L, menú Inicio y tras N minutos, opcional): la misma pantalla de `sessao.c` en modo «bloqueo», con la cuenta fija, que pide su contraseña (`PED_ENTRAR`). Los programas siguen corriendo. Con una cuenta sin contraseña, bloquear solo apaga la pantalla.

### 3.4 Configuración del sistema y de la persona

- **Sistema** (S): ficheros en `/etc` dentro de SAGUI-CONF (copiados a la RAM al arrancar, como ahora).
  - Solo los escribe root: Configuración los cambia con el diálogo de permisos.
  - El kernel deja de leer `/home` para estos ajustes.
  - Lista:
    - `/etc/sistema.conf` (nombre por decidir): aceleración (`acel2d`, `gpu3d`, `sdl2_gpu`), `disco_dma_cache`, `salida` y los ajustes del micro y de latencia, `mando_puerto`, teclado del sistema (el de la pantalla de inicio de sesión), idioma del sistema (ya en `/etc/idioma`), inicio automático;
    - `/etc/zona`, `/etc/frecuencia`, `/etc/pantallas.conf`, `/etc/sonido.conf`;
    - `/etc/bloqueo/`. El filtro DNS es de la máquina: lo cambia el administrador. Sus listas pueden pesar MB: comprobar el tamaño o dejarlas en `/home/.sistema`, de root (ver P9);
    - `/etc/wifi-desempenho.conf`, `/etc/vnc/`, `/etc/debug-app.conf`;
    - `/etc/red.conf`, cuando haya IP fija o DNS;
    - `/etc/wireguard/` para los túneles de la máquina, si se quieren.
- **Persona** (U, `~/.config`):
  - aspecto: fondo, acento, tema, barra, menú, iconos;
  - widgets, conky, Terminal, preferencias de cada programa, fuentes elegidas;
  - redes Wi-Fi recordadas y túneles WireGuard propios;
  - `idioma` y `teclado` de su sesión, que sustituyen a los del sistema mientras está dentro;
  - `volumen` y `ecualizador`;
  - `crontab`, servicios propios y `.ssh/authorized_keys`.
- **Migración**, una vez, al arrancar la primera versión nueva, por `configuracion --aplicar` (root):
  - lee las claves de la máquina de `/home/.config/sagui.conf` (el de sagui) y de `/home/.zona`, `.frecuencia`, `bloqueo/`, `pantallas.conf`, `sonido.conf` y `wifi-desempenho.conf`, y las escribe en SAGUI-CONF **solo si allí no hay nada**;
  - no borra las de `/home` (si se vuelve a una versión anterior, siguen ahí);
  - después, el kernel y `--aplicar` leen solo `/etc`.
  - Sin SAGUI-CONF (en vivo), se sigue leyendo `/home` como hoy.

### 3.5 Permisos de serie

- `umask 022` como hoy. Las carpetas personales, 0700, así que lo de dentro no lo ve nadie más aunque sea 0644.
  Basta y no rompe nada. Alternativa: `umask 077` para las personas (ver P8).
- **Grupos:**
  - Se mantiene `admin` (como `sudo`/`wheel`).
  - No se crean `audio`, `video` ni `red` todavía. Hoy el sonido, la GPU y la red se reparten por «sesión local o no» (`de_servico`), que con una sesión a la vez cumple lo mismo.
  - Si un día hay sesiones simultáneas (SSH con escritorio remoto por cuenta), entonces sí.
- **SSH:** `sshd` lee `~/.ssh/authorized_keys` de **cada** cuenta (la real, `/home/<n>/.ssh`, comprobando que es suya y no se puede escribir en grupo ni otros, como OpenSSH `StrictModes`) y entra con esa cuenta y su vista. Root solo con `/etc/ssh/authorized_keys` o `/root/.ssh/authorized_keys`.
- **cron y servicios de persona:** `cron` recorre las cuentas y lee `/home/<n>/.config/crontab`, y el gestor `/home/<n>/.config/servicios`. Cada tarea corre con su cuenta y su vista.
  - Para empezar, solo la cuenta de la sesión y las que tengan «permitir tareas sin sesión» (S).
  - Si no, las tareas de ana correrían con ana ausente. En Unix es normal; aquí se puede decidir (ver P10).

### 3.6 Instalador

1. Pide el **nombre de la cuenta** (de serie `sagui`, editable), su contraseña y **la de root, obligatoria**. La
   opción «la misma para las dos» la marca como administradora y no pide otra (ver P1).
2. Crea `/home/<cuenta>` (0700, suya) y `/root` (`raiz/` en SAGUI-CONF, 0700), escribe `/etc/usuarios`, la sombra y
   `/etc/grupos`, y graba el inicio automático según la casilla.
3. **Reinstalación con SAGUI-CONF vieja:** se conserva, como hoy. Si no tenía contraseña de root, la pide.

### 3.7 Vivo y rescate

- **En vivo** (sin SAGUI-CONF): `sagui` entra solo; root sin contraseña desde la sesión local, como hoy; `/root`
  en la RAM. Además, un aviso fijo en la bandeja.
- **La imagen publicada con SAGUI-CONF** (el USB «vivo» que se usa como instalado): el primer arranque pide la
  contraseña de root en la pantalla del sistema (la de `sessao.c`, antes de entrar), en vez de dejarla a quien
  llegue primero (ver P3).
- **Modo rescate:** sin cambios: Ctrl al encender da un shell de root sin contraseña. Añadir `usuarios` (listar
  cuentas, `passwd`, reparar dueños de `/home/<n>`) y la orden de migrar o deshacer la migración.

### 3.8 sagui a `/home/sagui`: migración automática y segura

Es lo más delicado: los datos de la persona están hoy **sueltos** en la raíz de `/home`.

- **Cuándo y quién:** al arrancar, el **kernel** o `configuracion --aplicar` como root, antes de abrir la sesión.
  Solo si `/home` está en ext2 (con dueños), hay SAGUI-CONF, y no existe `/home/sagui`.
- **Cómo, sin poder perder nada:**
  1. Escribe la marca `etc/migracion-home` en SAGUI-CONF («empezada»).
  2. Crea `/home/sagui.nuevo` (de sagui, 0700).
  3. Mueve **por renombrado** (en el mismo ext2 no se copia nada, así que no hace falta espacio) cada entrada de `/home` que no sea una carpeta de otra cuenta ni `lost+found` ni `sagui.nuevo`.
  4. Renombra `sagui.nuevo` → `sagui`.
  5. Cambia la línea de sagui en `/etc/usuarios` a `/home/sagui`.
  6. Pone la marca a «hecha».

  Si se corta a medias, al arrancar se ve la marca «empezada» y se termina (los renombrados son idempotentes).
  Nunca se borra nada.
- **Si no se puede** (FAT32, sin SAGUI-CONF, `/home` de solo lectura): no se migra. sagui sigue con `casa_conta`
  vacía como hoy, y lo dice el registro.
- **Compatibilidad:** los programas siguen pidiendo `/home/...` (la vista). Los servicios de sistema que corren
  como sagui (`litemonkey`) reciben la vista de sagui. Las pruebas que escriben en `/home` con la sesión root de
  pruebas (`--sesion-root`) ven el árbol real: o se les da la vista de sagui también a root en las imágenes de
  prueba, o se adaptan. Es lo más caro y se decide en P2/P4.
- **Volver atrás:** una versión anterior vería `/home/sagui` como una carpeta más y sagui «perdería» su
  configuración, pero no sus datos. El rescate tendría una orden para deshacer la migración (renombrados al
  revés).

---

## 4. Plan en fases pequeñas (cada una con su prueba)

| Fase | Qué | Núcleo tocado | Prueba | Riesgo |
|---|---|---|---|---|
| 0 (hecha) | Fallos A-E | `permisos.c`, `sessao.c`, `wifi.c` (pocas líneas); `servicios.c`, `vncd.c` | h371, más h254, h282, h340, h250 | Bajo |
| 1 | Configuración del sistema en SAGUI-CONF: `/etc/sistema.conf` y compañía, migración de claves, el kernel y `--aplicar` leen `/etc`, Configuración escribe con el diálogo | `hora.c`, `frequencia.c`, `telas.c`, `bloqueo.c`, `wifi.c`: cambian las rutas (con vuelta a `/home` sin SAGUI-CONF) | h3xx nueva: ana cambia la aceleración con el diálogo y vale al reiniciar; sagui ya no puede sin él; migración desde un `sagui.conf` viejo. Siguen h323, h83, h333, h353 | Medio (muchas pruebas leen `sagui.conf`) |
| 2 | `/root` (alias a SAGUI-CONF `raiz/`, o a la RAM en vivo), `HOME` en el entorno, `su` → `/root`, `htop` arreglado, límite de tamaño | `permisos.c` (alias), `carregador.c` (`HOME`) | `su` → `pwd` = `/root`; `/root` 0700, sagui no entra; tras reiniciar sigue; en vivo, en la RAM | Bajo |
| 3 | Instalador: nombre de cuenta, contraseña de root obligatoria, `/home/<cuenta>`; primer arranque de la imagen con SAGUI-CONF pide la de root | instalador, `sessao.c` (modo «elegir la contraseña de root») | h149 y h318 adaptadas; h nueva del primer arranque | Medio |
| 4 | sagui → `/home/sagui` con migración; `/home` de root 0755; D282.7 solo dentro de cada carpeta personal; elevados según P2 | `permisos.c`, `vfs.c`, `montagem.c`, `sessao.c` | h nueva: imagen con `/home` «viejo» (sagui suelto) → arranca → migrado, nada perdido (`debugfs` en el anfitrión), corte a medias simulado; batería con la sesión de prueba | **Alto**: es el que puede «perder» datos a la vista; va solo y con la marca |
| 5 | SSH, cron y servicios por cuenta | `sshd`, `cron`, `servicios` (userland) | h254/h248 con dos cuentas | Medio |
| 6 | `login` en la consola de texto, bloqueo de pantalla, inicio automático configurable | `principal.c`, `sessao.c` | h nueva en consola; h340 ampliada | Bajo-medio |

Las fases 1, 2 y 3 son independientes entre sí. La 4 depende de la 2 (`HOME`) y conviene hacerla después de la 1,
para que `/home/.config` ya no tenga nada de la máquina cuando se mueva.

---

## 5. Preguntas para el mantenedor

**P1. Contraseña de root al instalar.**

- (a) Obligatoria, aparte de la de la cuenta.
- (b) Obligatoria, con la casilla «usar la misma que mi cuenta» (la cuenta pasa a `admin`).
- (c) Opcional, como hoy.

*Recomiendo la (b):* tan sencillo como Ubuntu y root nunca queda sin contraseña.

**P2. Qué ven los programas elevados** (diálogo de permisos, `admin orden`).

- (a) El árbol real y `HOME=/root`: lo que guarden va a `/root/.config`.
- (b) El árbol real, pero `/home/.*` (la configuración) sigue siendo la de la persona que elevó: lo que guarden va a sus preferencias.
- (c) Como hoy, el `/home` de sagui. No vale con la migración.

`su` daría siempre `/root`. *Recomiendo la (b) para el diálogo y `admin`, y `/root` para `su`.* Si se prefiere no
tocar más el kernel, la (a).

**P3. Root sin contraseña en una imagen con SAGUI-CONF** (la imagen del USB, o una instalación antigua sin ella).

- (a) Como hoy: la elige el primero que usa `admin` desde la sesión local.
- (b) La pide el sistema en el primer arranque, en su pantalla, antes de entrar.
- (c) Sin contraseña, root no se puede usar (solo el modo rescate).

*Recomiendo la (b).*

**P4. ¿Migrar sagui a `/home/sagui`?**

- (a) Sí, automático y una sola vez, con marca y reanudable (3.8).
- (b) Solo en instalaciones nuevas; las existentes siguen con sagui = `/home`.
- (c) No: sagui sigue siendo `/home` y solo las demás cuentas tienen carpeta.

*Recomiendo la (a):* es lo que pide «cada usuario su directorio», y la vista hace que los programas no lo noten.
La (b) deja dos modelos que mantener.

**P5. Inicio automático en un sistema instalado.**

- (a) Nunca.
- (b) Opción del sistema, apagada de serie, que se puede encender para una cuenta.
- (c) Encendido si la cuenta no tiene contraseña (hoy).

*Recomiendo la (b)*, y en vivo siempre encendido.

**P6. Pantallas** (espejo o extender, resolución de cada una).

- (a) De la máquina (`/etc`, administrador).
- (b) De cada persona.
- (c) De la máquina, que la persona puede cambiar para su sesión sin guardar.

*Recomiendo la (a)*: depende de los monitores conectados, no de quién entra. La resolución principal elegida en
Configuración igual.

**P7. Idioma y teclado.**

- (a) Solo del sistema.
- (b) Del sistema y de cada persona (lo de la persona manda en su sesión, y la pantalla de inicio de sesión usa el del sistema).
- (c) Solo de la persona.

*Recomiendo la (b)*: ya es casi así; falta el «del sistema» para el teclado.

**P8. Máscara de serie para las personas.**

- (a) `022`, como hoy (con la carpeta personal 0700 basta).
- (b) `077`.

*Recomiendo la (a)*: lo compartido entre cuentas se haría con una carpeta común (`/home/compartido`, 1777).

**P9. Quién cambia la aceleración, kconfig, la frecuencia, el bloqueo DNS, la zona y el sonido de la máquina.**

- (a) Solo el administrador (diálogo de permisos).
- (b) La cuenta principal sin diálogo (hoy, de hecho) y las demás con él.

*Recomiendo la (a)*, con la migración de lo que ya haya guardado.

**P10. Red, Wi-Fi y VPN.**

- (a) Como hoy: conectar o encender lo hace la sesión local; las redes y los túneles son de cada persona.
- (b) Lo mismo, más redes y túneles «de la máquina» (en `/etc`, solo el administrador, que se conectan antes de entrar).
- (c) Todo solo el administrador.

*Recomiendo la (b)*, en una fase posterior. La IP fija y el DNS, de la máquina (administrador).

**P11. Tareas de cron y servicios de una cuenta que no ha entrado.**

- (a) Corren siempre (como Unix).
- (b) Solo mientras esa cuenta tiene la sesión abierta.
- (c) Siempre, pero solo si el administrador lo permite para esa cuenta.

*Recomiendo la (c)*, y de serie solo la cuenta principal (lo de hoy).

**P12. Inicio de sesión en la consola de texto** (instalaciones sin modo gráfico).

- (a) `login` con nombre y contraseña.
- (b) Entra la cuenta principal (hoy).

*Recomiendo la (a)*: sin ella, una instalación de texto no tiene ninguna contraseña de entrada.

**P13. ¿Root en la lista de la pantalla de inicio de sesión?**

- (a) No: se usa `su`, el diálogo o el rescate.
- (b) Detrás de «Otra cuenta…».
- (c) Sí, en la lista.

*Recomiendo la (a)* para la v8.5.3, y la (b) más adelante.

**P14. Dónde vive `/root` en un sistema instalado.**

- (a) La carpeta `raiz/` de SAGUI-CONF, con un límite de 4 MB.
- (b) Lo mismo, agrandando SAGUI-CONF a 64 MB en las instalaciones nuevas.
- (c) `/home/.root` en la partición `/home`.

*Recomiendo la (a)*, y la (b) cuando haga falta.

**P15. Bloqueo de pantalla con contraseña.**

- (a) En la v8.5.3 (Win+L y menú).
- (b) Más adelante.

*Recomiendo la (a)*: reutiliza la pantalla de D340 y es poco código.
