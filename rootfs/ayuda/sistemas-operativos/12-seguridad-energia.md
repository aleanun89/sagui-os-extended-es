<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 12; titulo: Seguridad, energía y apagado -->
# Seguridad, energía y apagado

[Español](12-seguridad-energia.md) · [Português](12-seguridad-energia_PT.md) · [English](12-seguridad-energia_EN.md)

> Objetivo: entender qué protege un sistema operativo y frente a qué, cómo se usan los usuarios y los permisos, qué
> papel tiene ACPI en la energía y por qué apagar bien un sistema es un problema de orden y de plazos.

## 1. Teoría

### 1.1 Qué es la seguridad en un sistema operativo

La seguridad no es una sola función. Se apoya en tres ideas:

- **Identidad**: saber quién hace cada cosa. Cada proceso y cada archivo tienen un dueño (usuario y grupo).
- **Autorización**: decidir qué puede hacer esa identidad. Son los permisos de archivo y las llamadas privilegiadas.
- **Aislamiento**: que un fallo o un programa malicioso no pueda tocar lo que no le corresponde. El espacio de direcciones
  separado (capítulo 6) y el modo núcleo (capítulo 3) son la base.

El **principio de mínimo privilegio** dice que cada proceso debe tener solo los permisos que necesita. El administrador
(`root`) es el caso extremo: puede casi todo, y por eso se usa con cuidado.

### 1.2 Usuarios y permisos

Unix y sus descendientes usan `uid` y `gid` para identificar a cada proceso y cada archivo. El administrador es el uid 0.
Los permisos de archivo son lectura, escritura y ejecución para el dueño, el grupo y el resto. Otros modelos, como las
listas de control de acceso (ACL) de Windows, dan permisos más finos a costa de complejidad.

### 1.3 Llamadas privilegiadas

No todas las llamadas están abiertas a todos: cambiar la hora, reiniciar la máquina o montar discos son operaciones que
el núcleo solo permite a un administrador. El núcleo lo comprueba en la propia llamada, antes de ejecutarla.

### 1.4 Energía y ACPI

**ACPI** (*advanced configuration and power interface*) es la especificación con la que el firmware describe la
placa: fuentes de energía, sensores, apagado, reinicio y baterías. El sistema operativo ejecuta métodos de un lenguaje
interno llamado **AML** para usarlos. Un apagado correcto usa el estado **S5** (apagado sin memoria).

### 1.5 Apagar con orden

Apagar mal puede perder datos: archivos a medio escribir en la caché de disco, un servicio que no ha cerrado su
base de datos, un procesador que sigue corriendo código mientras la energía cae. El apagado ordenado sigue estos pasos:

1. Pedir a los programas y servicios que terminen, con un plazo.
2. Vaciar la caché de disco y desmontar los sistemas de archivos.
3. Detener los demás procesadores y los drivers, en orden inverso al de arranque.
4. Apagar o reiniciar con el mecanismo más fiable que haya.

Cada paso necesita un **plazo**: si un paso se cuelga, el sistema no puede quedarse esperando para siempre. Se anota
el fallo y se sigue con el siguiente.

### 1.6 Alternativas

- Linux: `systemd` coordina el apagado de los servicios y la orden `shutdown`.
- Windows: el apagado rápido guarda parte del estado en disco (hibernación híbrida).
- BSD: `shutdown` y el gestor de `rc` paran los servicios en orden.

### 1.7 Cálculo: el plazo del peor caso en un reinicio

El apagado de SaguiOS (decisión D450) tiene plazos acotados. Sumando el peor caso de cada paso que tiene plazo fijo, con un
solo NVMe, el reinicio tarda como mucho: servicios 20 s + ACPI 3 s + procesadores 0,1 s + NVMe 5 s + `RESET_REG` 1 s + 8042
0,5 s + `0xCF9` 0,5 s = **30,1 s**. Quedan fuera los plazos de los discos, que dependen de su respuesta (se miden con el
reloj TSC). Con dos NVMe, el peor caso sube en 5 s más. El plazo acotado es lo que impide que el sistema se quede colgado
para siempre, porque sin plazo un paso bloqueado retrasaría el apagado indefinidamente.

### 1.8 Comparativa: apagado en otros sistemas

- **Linux** con `systemd` para los servicios: cada unidad tiene un tiempo de parada (por defecto 90 s) y después se mata.
- **Windows** da a cada servicio un tiempo para cerrarse (por defecto 5 s en el registro) y después fuerza la terminación.
- **FreeBSD** usa `shutdown` y el sistema de scripts `rc`, que para los servicios en orden inverso al arranque.
- Todos comparten la idea: cada paso tiene un tiempo máximo, y el sistema sigue aunque un servicio no responda.

### 1.9 Contraseñas: *hash*, sal e iteraciones

Un sistema no debe guardar las contraseñas, sino su **huella** (*hash*): una función de un solo sentido. Al entrar se calcula la
huella de lo escrito y se compara. Dos defensas más: la **sal** (un valor aleatorio distinto por cuenta, que obliga a atacar cada
cuenta por separado y deja inútiles las tablas precalculadas) y las **iteraciones** (repetir el cálculo muchas veces para que cada
intento sea caro). Cálculo: una contraseña de 8 caracteres de 62 símbolos tiene 62^8 aprox. 2,2 × 10^14 posibilidades. Con un hash simple a
10^9 intentos por segundo se agota en unos 2,5 días; con 20 000 iteraciones cada intento cuesta 20 000 veces más (5 × 10^4 por
segundo) y son unos **140 años** (cifras ilustrativas). Además, el sistema puede frenar a quien prueba en línea (una espera tras
cada fallo y un bloqueo tras varios).

### 1.10 Dominios de protección: ACL y capacidades

Se puede ver la protección como una **matriz** de dominios (usuarios o procesos) por objetos (archivos, dispositivos), con los
derechos en cada casilla. Guardarla por **columnas** da una **lista de control de acceso** (ACL): cada objeto dice quién puede
hacer qué; responde fácilmente «¿quién puede leer este archivo?». Guardarla por **filas** da **capacidades**: cada proceso lleva
una lista de lo que puede usar, como un descriptor de archivo abierto; responde a «¿qué puede tocar este proceso?» y se pasa
fácilmente entre procesos. Los permisos de Unix (`rwx` para dueño, grupo y resto) son una ACL reducida a tres entradas; un
descriptor de archivo abierto es una capacidad.

### 1.11 Principios de diseño seguro

- **Mínimo privilegio**: cada proceso solo con lo que necesita.
- **Valores seguros por defecto**: lo que no está permitido expresamente, se deniega.
- **Mediación completa**: se comprueba cada acceso, no solo el primero.
- **Mecanismo económico**: lo más pequeño y sencillo posible, para poder revisarlo entero.
- **Diseño abierto**: la seguridad no depende de que el atacante ignore el funcionamiento (sí de que ignore la clave).
- **Aceptación psicológica**: si es incómodo, la gente lo desactiva.

### 1.12 Ataques típicos y defensas

- **Desbordamiento de búfer.** Un programa copia datos sin comprobar el tamaño y pisa lo que hay detrás, como la dirección de
  retorno de la pila. Defensas: comprobar tamaños, páginas de datos no ejecutables (**NX**, capítulo 6), una página de guarda
  junto a la pila y aleatorizar las direcciones (**ASLR**). El gusano de Internet de 1988 se propagó, entre otras cosas, con un
  desbordamiento de búfer en un servicio de red.
- **Malware.** Un **virus** se pega a otro programa; un **gusano** se copia solo por la red; un **troyano** es un programa útil en
  apariencia que hace otra cosa. Defensa: mínimo privilegio y que lo que corre en nombre de un usuario no pueda cambiar el sistema.
- **Canales ocultos.** Dos procesos que no pueden hablar pueden comunicarse a través de algo que sí comparten, como el tiempo que
  tarda cada uno en obtener la CPU. Se cierran muy difícilmente.

### 1.13 Energía: estados de la CPU

La CPU gasta energía de dos formas que se pueden ajustar. Con **estados de rendimiento** (P-states) se baja a la vez la frecuencia
*f* y la tensión *V*: la potencia dinámica es proporcional a *C* × *V*² × *f*. Con **estados de reposo** (C-states) se apagan partes
del chip cuando no hay trabajo; el más simple es la instrucción `hlt`, que para la CPU hasta la siguiente interrupción. Bajar la
frecuencia un 25 % y la tensión un 15 % deja la potencia en 0,75 × 0,85² aprox. **0,54**: un ahorro del 46 %, mientras que el trabajo
tarda un tercio más.

## 2. Así lo hace SaguiOS

- **Usuarios y permisos.** `kernel/nucleo/usuarios.c` (quién es cada proceso, su uid y gid) y `kernel/nucleo/permisos.c`
  (qué llamadas puede hacer cada uno), con las decisiones D255 (llamadas privilegiadas) y D282 (dueño, grupo y `rwx`).
  Las cuentas y el administrador tienen su diseño en la decisión D371 (`/root`, carpetas propias y configuración de
  sistema frente a la de la persona).
- **Bloqueo de sesión.** `kernel/gui/sessao.c` gestiona el inicio y el cierre de sesión; `kernel/gui/ocio.c` el bloqueo y
  el salvapantallas. El menú de la barra de tareas (`kernel/gui/barra.c`) ofrece «Bloquear» cuando hay cuentas.
- **Aislamiento.** Cada proceso tiene su espacio de direcciones (capítulo 6), las llamadas validan los punteros del
  programa con `usuario_ok` (capítulo 3) y los permisos se comprueban en cada `open`.
- **Contraseñas.** Las cuentas guardan una huella **PBKDF2-SHA256** con sal de 16 bytes y 20 000 iteraciones
  (`userland/inc/pbkdf2.h`); la comprueba `login` y el gestor de servicios, y solo root puede leer el archivo de sombra. Tras un
  fallo, la respuesta tarda 2 s, y tras cinco seguidos del mismo usuario se bloquea un minuto.
- **Elevación sin `setuid`.** `admin` y `su` no son programas con privilegios propios: piden la elevación al gestor de servicios
  (decisión D255), que mira la sombra y, si la concede, el kernel cambia la identidad del proceso que la pidió.
- **Cerrado por defecto.** En `kernel/nucleo/permisos.c`, una llamada al sistema que nadie ha clasificado queda **solo para root**:
  olvidarse de clasificarla deniega, no permite.
- **Programas del sistema y enlace dinámico.** Un programa de `/bin` se carga con `AT_SECURE` (decisión D378): no lee la variable
  `SAGUI_LIB_CAMINHO` y solo carga bibliotecas de root sin escritura de otros, para que nadie cambie lo que ejecuta con
  privilegios.
- **Energía de la CPU.** `kernel/dispositivos/frequencia.c` elige entre los P-states que la BIOS declaró (decisión D106, y por CPU
  con SMP en la D175), con un límite de temperatura de 79 °C; una CPU ociosa duerme con `hlt` (capítulo 7).
- **Servicios.** `kernel/nucleo/supervisor.c` arranca, reinicia y para los servicios (decisión D216). La orden `servicios`
  los muestra.
- **ACPI.** `kernel/dispositivos/uacpi_sagui.c` es la capa del kernel para la biblioteca uACPI (código de terceros, MIT, en
  `kernel/terceros/uacpi/`), y `kernel/dispositivos/acpi.c` hace el apagado por S5 (decisión D113). `bateria.c` lee
  el estado de la batería.
- **Apagado y reinicio ordenados.** `kernel/nucleo/energia.c` (decisión D450) tiene un solo camino para reiniciar y apagar,
  en la tarea de apagado del kernel. Su orden fijo es:
  1. inicio;
  2. servicios (como mucho 20 s, en `supervisor.c`);
  3. discos: grabar lo pendiente y desmontar (cada orden de disco tiene su plazo);
  4. ACPI: `_TTS(5)` y, al apagar, `_PTS(5)` (como mucho 3 s); después, las interrupciones cerradas;
  5. procesadores secundarios detenidos (como mucho 100 ms) y drivers apagados al revés que al arrancar;
  6. reiniciar por el registro `RESET_REG` de la FADT (1 s), después por el controlador de teclado, por el puerto 0xCF9 y,
     si todo falla, un triple fallo; apagar por `SLP_TYP|SLP_EN` de `\_S5_` (3 s).

  Cada paso deja una marca en el registro serie y en un rastro que sobrevive al reinicio en caliente.
- **Órdenes.** `apagar-pc` y `reiniciar` son órdenes internas del intérprete (`userland/sagsh/internos.c`).

![Visor de registros con las marcas del apagado y la energía](img/cap-visorlogs.png)

### 2.1 Decisiones, alternativas y límites actuales

- **Decisión: usuarios y llamadas privilegiadas.** La decisión **D255** reserva a un administrador las llamadas que cambian el
  sistema; la decisión **D282** da permisos de archivo por dueño, grupo y `rwx`.
- **Decisión: cuentas y `/root`.** La decisión **D371** separa la configuración del sistema de la de cada persona.
- **Decisión: servicios supervisados.** La decisión **D216** arranca, vigila y reinicia los servicios
  (`kernel/nucleo/supervisor.c`).
- **Decisión: un solo camino de apagado con orden y plazos.** La decisión **D450** (`kernel/nucleo/energia.c`).
- **Límite: el aislamiento es el de la memoria y de las llamadas.** Un programa no puede tocar la memoria de otro, pero no hay
  jaulas (*jails*) ni contenedores: un usuario administrador tiene acceso a todo.
- **Límite: los plazos están fijados en el código.** Cambiarlos exige recompilar, no hay un fichero de configuración para ello.

## 3. Experimento en SaguiOS

1. Escribe `whoami` e `id`. Compara el uid y los grupos de tu usuario con los de `root`.
2. Escribe `ls -l /ayuda` y observa los permisos de los archivos. Intenta `cat` de un archivo sin permiso de lectura, si
   tienes uno, y observa el error.
3. Escribe `servicios` y mira los servicios que el supervisor mantiene vivos.
4. Abre el **Visor de registros**, elige la categoría **Energía** y busca `apagado` o `reinicio`. Si el registro
   conserva marcas de un apagado anterior, las verás en esa categoría.
5. **No ejecutes `apagar-pc` ni `reiniciar` por hacer el experimento**: son las órdenes que cierran el sistema. Léelas en
   `energia.c` y en la decisión D450, y compara el orden con la teoría.

**Qué observar:** un apagado es un programa más, con pasos, plazos y registros. Por eso puede depurarse cuando falla.

**Ejercicio práctico adicional.** Lee la cabecera de `kernel/nucleo/energia.c` y dibuja con tus palabras la secuencia de
pasos del apagado, con su plazo en cada uno. Después abre el Visor de registros, elige la categoría Energía y explica qué
significa cada marca si la hay. No ejecutes `apagar-pc` para hacerlo.

## 4. Preguntas de repaso

1. ¿Qué es el principio de mínimo privilegio? Pon un ejemplo de un servicio que lo cumpla y otro que no.
2. ¿Por qué el apagado debe desmontar los discos antes que apagar la energía, y qué pasa si no lo hace?
3. ¿Por qué cada paso del apagado tiene un plazo? ¿Qué problema evita?
4. Un driver de red tarda 30 segundos en cerrarse durante el apagado. ¿Qué debe hacer el sistema según el orden de SaguiOS?

5. Si el apagado tiene un paso que tarda 40 s y no tiene plazo, ¿qué pasa con el sistema? ¿Y con plazo de 5 s?
6. ¿Por qué un programa de usuario no puede leer la contraseña de otro usuario aunque conozca su dirección de memoria?
7. ¿Qué diferencia hay entre el apagado por ACPI (S5) y un reinicio por el controlador de teclado?
8. Una contraseña de 6 letras minúsculas se prueba a 5 × 10^4 intentos por segundo (con iteraciones). ¿Cuánto tarda en agotarse el
   espacio de búsqueda?
9. ¿Qué diferencia hay entre una ACL y una capacidad? ¿Cuál responde mejor a «¿quién puede leer este archivo?»?
10. Si la frecuencia baja un 50 % y la tensión un 20 %, ¿qué fracción de la potencia dinámica queda?
11. ¿Por qué `permisos.c` deja cerradas las llamadas que nadie ha clasificado?

## 5. Soluciones

1. Un proceso debe tener solo los permisos que necesita. Un servicio de impresión que solo escribe en su cola cumple el
   principio. Un programa que corre siempre como administrador no lo cumple, aunque sea cómodo.
2. La caché de disco guarda datos que aún no están escritos. Si se corta la energía antes de volcarla, se pierden o quedan
   incoherentes. Desmontar fuerza la escritura y deja el sistema de archivos en un estado limpio.
3. Porque un paso puede colgarse (un disco que no responde, un driver atascado). Sin plazo, el apagado se quedaría esperando
   para siempre. Con plazo, se anota el fallo y se sigue.
4. Según D450, cada paso tiene un plazo: pasado el tiempo se anota el fallo y el apagado continúa con el siguiente paso.
   El driver no bloquea el resto del apagado, y el registro queda con la marca del paso que falló.

### Respuestas a las preguntas adicionales

5. Sin plazo, el apagado espera 40 s o más, y si el paso se cuelga, espera para siempre. Con plazo de 5 s, se anota el fallo
   a los 5 s y el apagado sigue con el siguiente paso.
6. Porque el espacio de direcciones de cada proceso es distinto: su dirección no existe en el espacio del otro. Además, el
   núcleo valida los punteros de cada llamada (`usuario_ok`).
7. El apagado por ACPI pide al firmware que pase al estado S5 (apagado); el reinicio por teclado (0xFE en el 8042) pide al
   controlador de teclado que reinicie el procesador. Son mecanismos distintos: uno apaga, el otro reinicia.
8. 26^6 = 308 915 776 posibilidades / 50 000 por segundo aprox. 6178 s, es decir, **1,7 horas**.
9. Una ACL está en cada objeto y dice quién puede usarlo (responde mejor a «¿quién puede leer este archivo?»); una capacidad la lleva
   cada proceso y dice qué puede usar (responde a «¿qué puede tocar este proceso?»).
10. 0,5 × 0,8² = 0,5 × 0,64 = **0,32**: queda el 32 % (se ahorra el 68 %).
11. Por el principio de valores seguros por defecto: si alguien añade una llamada y se olvida de clasificarla, lo que ocurre es
    que nadie salvo root puede usarla (un fallo visible y sin riesgo), y no que todos puedan usarla sin darse cuenta.
