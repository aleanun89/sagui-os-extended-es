# DEBUG-APP: depuración remota con un código de un solo uso (D362)

DEBUG-APP deja que otro equipo de la red local entre por SSH en un SaguiOS, **como administrador
(root)** y durante un rato, para pasarle pruebas (dmesg, `infopc -t`, `acel2d probar --detalle`,
`gpu3d probar --detalle`...) sin pendrives ni fotos de la pantalla. En SaguiOS se pulsa un botón y
sale un **código**; en el PC, `tools/depurar-remoto.py CÓDIGO ...` lo usa para entrar.

No hace falta activar el servicio `sshd` ni copiar claves: DEBUG-APP abre su propia sesión, en
otro puerto, que acepta **una sola clave, una sola vez**.

## En SaguiOS

**Con ventana:** menú Inicio → Sistema → «DEBUG-APP (depuración remota)».

1. Revisa las opciones (se guardan para la próxima vez en `/home/.config/debug-app/debug-app.conf`):
   - **Dirección del código:** la de la tarjeta de red (cable o Wi-Fi). Se cambia solo si el otro
     equipo tiene que llegar por otra (un reenvío de puertos en el router, por ejemplo).
   - **Puerto:** 0 = 2222 o, si está ocupado, el primero libre de los 9 siguientes.
   - **Duración:** de 15 minutos a 8 horas (de serie, 2 horas).
   - **Direcciones permitidas:** quién puede conectar. `subred` (de serie) es la red de la tarjeta
     en ese momento (p. ej. 192.168.10.0/24). Se pueden añadir redes (`192.168.1.0/24`) o
     direcciones sueltas (`10.8.0.7`), separadas por comas, si el PC está detrás de otro router o
     de una VPN. Solo direcciones privadas (10/8, 172.16/12, 192.168/16, 169.254/16), salvo que se
     marque **«Permitir también direcciones públicas»** (o se escriba `publicas` en la lista), que
     pide confirmación aparte.
2. Pulsa **«Permitir depuración remota»**. Avisa de que quien tenga el código será root y pide la
   contraseña de administrador (el mismo diálogo que cambiar la GPU).
3. La ventana se pone roja («SESIÓN REMOTA ACTIVA») y enseña el **código** (40 letras en 8 grupos;
   «Copiar código» lo deja en el portapapeles), la dirección y el puerto, la **huella** del servidor
   (`SHA256:...`, la misma que `sshd --huella`), el tiempo que queda y quién está conectado.
4. **«Cortar»** o cerrar la ventana termina la sesión y echa a quien esté dentro.

**En el terminal o por SSH** (por ejemplo, en una máquina sin escritorio). En modo texto,
`publicas` solo vale si se escribe en `--permitir`, no desde las opciones guardadas:

```
admin debug-app --texto                       # con las opciones guardadas
admin debug-app --texto --minutos 30 --permitir subred,192.168.1.0/24
```

Escribe el código, la dirección y la huella, y se queda en primer plano: **Intro** (o Ctrl+C)
corta. `--puerto`, `--minutos`, `--permitir`, `--ip` y `--guardar` (las guarda como las de serie).

**Si no hay red** (sin dirección IP) o la dirección del equipo **no es privada**, no se abre (lo dice).

## En el PC (Linux, WSL u otro con OpenSSH)

```
tools/depurar-remoto.py CÓDIGO info                 # qué dirección, puerto y huella lleva
tools/depurar-remoto.py CÓDIGO ejecutar "dmesg -n 50"
tools/depurar-remoto.py CÓDIGO shell                # un sagsh interactivo
tools/depurar-remoto.py CÓDIGO recoger              # dmesg, infopc -t, acel2d/gpu3d probar --detalle,
                                                    # uname -a, lspci y el registro de DEBUG-APP
tools/depurar-remoto.py CÓDIGO subir prueba.bin /tmp/
tools/depurar-remoto.py CÓDIGO bajar /logs/sshd.log .
tools/depurar-remoto.py CÓDIGO cerrar               # al acabar (la sesión de SaguiOS se cierra)
```

(`tools/depurar-remoto.sh` es lo mismo.) El código se puede escribir con o sin guiones, en
mayúsculas o minúsculas; `I`/`L` valen 1 y `O` vale 0. Para que no quede en el historial del
shell ni en lo que enseña `ps`: `DEBUG_APP_CODIGO=... tools/depurar-remoto.py env ejecutar "..."`
o `-` (lo lee de la entrada; en un terminal lo pide sin eco). Basta en la primera orden: después
el código ya está gastado.

- La primera orden conecta; las siguientes **reutilizan esa conexión** (ControlMaster de
  OpenSSH), así que se pueden encadenar muchas órdenes, `subir` y `bajar` con un solo código.
  Si la conexión se cae (Wi-Fi, suspensión del PC), el código ya no vale: genera otro.
- `recoger` deja los ficheros en `pruebas-remotas/<máquina>-<fecha>/` (fuera de git y del
  repositorio público). `--resultados CARPETA` y `--maquina NOMBRE` lo cambian.
- `--host H` y `--puerto N` conectan a otra dirección o puerto que los del código (un NAT o un
  reenvío de puertos por medio). La huella se sigue comprobando.
- No toca `~/.ssh` ni la configuración del usuario (`ssh -F /dev/null`); la clave de un solo uso
  solo está en disco (0600, en `$XDG_RUNTIME_DIR` o `/tmp`) mientras se abre la conexión.

### Desde WSL

Con la red de WSL en **NAT** (lo normal) funciona sin abrir nada en Windows: la conexión **sale**
del PC hacia el portátil, y Windows la reenvía con su propia dirección. Para el portátil, quien
conecta es la dirección del PC Windows en esa red (p. ej. 192.168.10.121 si el portátil es
192.168.10.37): si están en la misma subred, `subred` basta. Si el PC tiene varias redes
(cable y Wi-Fi), Windows elige la que llega al portátil. En modo **espejo** (mirrored) igual.

### Otra red, un VPN, otro router

Si el PC no está en la misma subred que el portátil, añade su red o su dirección en «Direcciones
permitidas» (la dirección **que ve el portátil**, después de cualquier NAT). Si hay un reenvío de
puertos de por medio, conecta con `--host` y `--puerto`.

### Si no conecta

- **«no contesta un servidor SSH»**: la sesión se cortó o caducó, la dirección no es esa, o la
  red no deja que los equipos se vean. **Muchos routers aíslan a los clientes Wi-Fi** (sobre todo
  las redes de invitados): conecta el portátil (o el PC) **por cable** al router, o desactiva el
  aislamiento. Desde otro equipo, `ping` a la dirección del portátil debe contestar.
- **«Conexión rechazada: ... dirección no permitida»** en la ventana de SaguiOS: la dirección con
  la que llega el PC no está en la lista (mira cuál dice y añádela).
- **«la clave del servidor ... NO es la del código»**: en esa dirección contesta otro equipo, o
  alguien está en medio. No se conecta. Comprueba la dirección y la huella que enseña SaguiOS.
- **«SaguiOS no acepta la clave»**: el código ya se usó, se anuló (tres intentos con claves malas)
  o es de una sesión anterior. Genera otro.
- **«la suma de control no cuadra»**: alguna letra está mal copiada.

## Cómo probar un kernel por DEBUG-APP (D392)

`arranque probar` deja un kernel en la ranura de prueba del disco de arranque y lo arranca **una sola vez**: la
etapa 2 borra la marca antes de cargarlo, así que si el kernel nuevo se cuelga, basta con reiniciar (o que alguien
apriete el botón) para volver al **último bueno** (el último confirmado; si no hay, el de la imagen). Solo por BIOS.

Hay tres kernels, y `arranque estado` los muestra: el **original** (el de la imagen, que `arranque` no cambia nunca),
el **último bueno** (lo deja `confirmar`) y el **en prueba**. Se puede probar otro aunque haya un último bueno: si
falla, vuelve ese. `arranque cancelar` quita la prueba (vuelve el último bueno) y `arranque original` quita también
el último bueno (vuelve el de la imagen).

```
export DEBUG_APP_CODIGO=...                    # el código de la ventana (no queda en el historial)
R=tools/depurar-remoto.py
$R env subir build/kernel.bin /tmp/
$R env ejecutar "arranque estado"              # etapa 2, kernel, ramdisk, sitio libre, con qué arrancó
$R env ejecutar "arranque probar /tmp/kernel.bin"
$R env ejecutar "reiniciar"                    # arranca la prueba (una vez); la conexión se corta
# ... nueva sesión de DEBUG-APP (otro código):
$R env ejecutar "arranque estado"              # «Este arranque: el KERNEL DE PRUEBA (una vez)»
$R env ejecutar "dmesg | grep 'kernel de prueba'"
$R env ejecutar "arranque confirmar"           # si va bien: queda como el último bueno
# si va mal: reiniciar (vuelve el último bueno), o «arranque cancelar» si aún no se reinició
# volver al kernel de la imagen: «arranque original» y reiniciar
```

**Un disco grabado antes de D392** (la etapa 2 no sabe de la ranura: `arranque estado` dice «ANTIGUA») recibe la
etapa 2 nueva con `arranque etapa2 /tmp/estagio2.bin` (va a sitio libre y la cabecera pasa a apuntar a ella; la
vieja no se toca). Si el disco no tiene sitio en la partición de SaguiOS (las imágenes de antes de D392), la prueba
va como fichero en la ESP. Con un kernel de antes de D392, `arranque` solo puede escribir si no hay **nada montado**
del disco de arranque: arrancar con **M** pulsada → «2 todo a la RAM», desmontar lo que quede (`mount`, `umount`) y
repetir. Lo dice el propio `arranque` si hace falta. Detalles y riesgos: D392 en `docs/decisoes.md`.

## Qué protege y qué no

- **Un secreto de un solo uso** (64 bits del generador aleatorio del kernel, D061). No viaja por la
  red: es la semilla de una clave ed25519 del cliente; SaguiOS solo conoce la pública. Vale para
  **una** conexión autenticada: cuando esa conexión termina, la sesión se cierra. Tres conexiones
  con una clave mala anulan el código. Con eso, adivinarlo es imposible en la práctica.
- **Una conexión a la vez**; caduca sola (15 min - 8 h); «Cortar», cerrar la ventana o que
  DEBUG-APP muera la terminan (el `sshd` de la sesión lo nota porque se cierra la tubería de su
  entrada) y matan lo que esté corriendo.
- **Huella del servidor en el código** (8 bytes atados al secreto: SHA-256 del secreto y de la
  clave del servidor): el PC la comprueba antes de entregar nada, contra un equipo que se haga
  pasar por el portátil, y fija esa clave para la conexión de verdad. Quien no tenga el código no
  sabe qué huella buscar.
- **Solo direcciones privadas** de la lista (de serie, la misma subred); nunca la propia máquina.
- **Registro:** todo lo que se ejecuta (y las conexiones, rechazos y cierres) va a
  `/home/.config/debug-app/debug-app.log`; `dmesg` tiene una línea al abrir y otra al cerrar.
  Nunca el secreto ni el código.
- **No protege** de quien tenga el código mientras valga (es root: puede hacerlo todo), ni de
  alguien que pueda ocupar el único hueco conectándose sin autenticarse (se suelta a los 30 s).
  El código en el portapapeles lo pueden leer otros programas de la sesión.

## Cómo está hecho

- `userland/programas/debug-app.c`: la ventana y el modo `--texto`. Genera el secreto, calcula la
  clave pública, lanza `sshd --depuracion` como root con una tubería en su entrada (cerrarla corta)
  y otra en su salida (avisos `@puerto`, `@huella`, `@fp`, `@conexion`, `@conectado`, `@fin`...).
- `userland/programas/sshd.c`, modo `--depuracion`: escucha, filtra direcciones
  (`dapp_permitido`) y lanza `sshd --sesion-depuracion` por conexión (la sesión normal de D250 con
  una sola clave autorizada, como root, sin túneles, 30 s para autenticarse). Por el código de
  salida de la sesión sabe si entró (el código se gasta), si probó una clave mala (cuenta para
  anularlo) o si no llegó a intentarlo.
- `userland/programas/debug_app_logica.h`: el código (base32 de Crockford, 25 bytes:
  versión, IP, puerto, secreto, huella y 2 bytes de suma SHA-256), la semilla y las listas de
  direcciones. `tools/depurar-remoto.py` hace lo mismo en Python.
- Prueba: `tests/h364_debug_app.py` (y `tests/debug_app_unidad.c` en el anfitrión). Decisión: D362.
