<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 1; titulo: Qué es un sistema operativo -->
# Qué es un sistema operativo

[Español](01-que-es-un-so.md) · [Português](01-que-es-un-so_PT.md) · [English](01-que-es-un-so_EN.md)

> Objetivo: entender qué hace un sistema operativo, por qué se organiza en capas y dónde encaja SaguiOS.

## 1. Teoría

### 1.1 Un programa que gestiona el hardware

Un ordenador es hardware: procesador, memoria, discos, red, pantalla. Los programas de usuario quieren usar ese
hardware sin conocer sus detalles y, además, varios programas quieren usarlo a la vez. El **sistema operativo** es el
software que hace de intermediario. Tiene cuatro funciones clásicas:

- **Abstraer.** Ofrece modelos más simples que el hardware: un *proceso* en vez de una CPU, un *archivo* en vez de
  sectores de disco, un *socket* en vez de un controlador de red.
- **Repartir.** Reparte la CPU, la memoria, el disco y la red entre los programas, con reglas de equidad, prioridad y
  cuotas.
- **Proteger.** Impide que un programa pise la memoria de otro, o que un usuario lea los archivos de otro sin permiso.
- **Coordinar.** Permite que los programas se comuniquen y se esperen (tuberías, señales, cerrojos).

Ese conjunto se llama **núcleo** (*kernel*): la parte que se ejecuta con todos los privilegios del procesador. Lo demás
(shell, editor, navegador) son programas normales que le piden servicios.

### 1.2 Núcleo monolítico frente a micronúcleo

Hay dos grandes familias de diseño:

| Diseño | Qué pone dentro del núcleo | Ventajas | Inconvenientes | Ejemplos |
| --- | --- | --- | --- | --- |
| **Monolítico** | Planificador, memoria, sistemas de archivos, drivers y pila de red, todo en un mismo espacio privilegiado. | Las llamadas entre partes son funciones directas: rápido. Es más fácil de depurar con un solo binario. | Un fallo en un driver puede tumbar todo el sistema. El código privilegiado es enorme. | Linux, FreeBSD, OpenBSD |
| **Micronúcleo** | Solo lo mínimo: procesos, direcciones, IPC. Los drivers y los sistemas de archivos son procesos de usuario. | Más aislamiento: un driver que falla se reinicia por separado. El núcleo es pequeño y auditable. | Cada servicio exige mensajes entre procesos: más costoso si no se diseña con cuidado. | MINIX 3, QNX, L4 |

En la práctica, casi todos los sistemas reales son **híbridos** o monolíticos con módulos. El debate sobre cuál es mejor
tiene que ver con la fiabilidad frente a la velocidad, y con cuánto código de terceros se quiere aislar.

### 1.3 Llamadas al sistema y modo privilegiado

Un programa no toca el hardware directamente. Pide servicios con **llamadas al sistema** (*system calls*): `read`,
`write`, `fork`, `open`. La CPU impone la frontera con dos modos de ejecución (los veremos en el capítulo 3). Esa frontera
es lo que hace posible proteger el sistema.

### 1.4 Otros sistemas, para comparar

- **Linux**: monolítico con módulos cargables; todo el núcleo corre en modo privilegiado.
- **Windows NT**: núcleo híbrido, con capas (HAL, ejecutivo, gestor de E/S) y muchos servicios fuera del núcleo.
- **BSD (FreeBSD, OpenBSD)**: monolíticos, con documentación y código muy legibles; OpenBSD prioriza la seguridad.
- **MINIX 3**: micronúcleo con drivers como procesos que se reinician solos. Es el ejemplo clásico de la asignatura.

### 1.5 Un cálculo sencillo: el coste de separar el núcleo de los servicios

Supón que un servicio cuesta **0,8 µs** dentro de un núcleo monolítico (entrar, hacer el trabajo y salir). En un
micronúcleo, el mismo servicio exige un mensaje al servidor: un cambio de proceso de ida y otro de vuelta de 2 µs cada
uno, más copiar el mensaje (0,3 µs). Total: 2 × 2 + 0,3 = **4,3 µs**, unas cinco veces más. Los números son ilustrativos
(es un ejercicio, no una medición), pero el razonamiento es el real: el aislamiento tiene un precio medido en tiempo, y
los micronúcleos lo reducen evitando cambios de proceso innecesarios.

### 1.6 Qué se pone dentro del núcleo: comparativa

| Sistema | Modelo | Dónde viven los drivers | Se pueden cargar en caliente |
| --- | --- | --- | --- |
| Linux | monolítico con módulos | en el núcleo, como módulos | sí |
| FreeBSD | monolítico con módulos del núcleo | en el núcleo | sí |
| Windows NT | híbrido por capas | en modo núcleo, con el modelo de controladores | sí |
| MINIX 3 | micronúcleo | procesos de usuario | sí (se reinician) |
| SaguiOS | monolítico en una sola imagen | dentro de `kernel/dispositivos/`, compilados en el núcleo | no es el objetivo del curso |

### 1.7 Otras estructuras: capas, máquinas virtuales y cliente-servidor

Además del monolítico y el micronúcleo hay otras formas de organizar un sistema:

- **Capas.** El sistema se divide en niveles y cada uno solo usa los servicios del inferior (hardware, memoria, E/S,
  programas de usuario...). Cada nivel se puede verificar por separado y es el modelo de las pilas de red (capítulo 10).
  Su problema: las funciones reales cruzan capas (la memoria virtual necesita el disco, y el disco necesita memoria) y
  atravesar muchas capas cuesta tiempo.
- **Máquinas virtuales.** Un **hipervisor** (el primero fue VM/370, de IBM) multiplica una máquina física en varias
  máquinas virtuales que se comportan como el hardware, y cada una ejecuta su propio sistema operativo «invitado».
  QEMU y KVM son hoy su versión para PC. SaguiOS se prueba casi siempre como invitado de QEMU (con KVM o con la CPU
  emulada de QEMU, llamada TCG). Eso condiciona el diseño: un invitado no sabe cuándo el hipervisor le quita la CPU
  virtual, y por eso el cerrojo general del kernel cambia su forma de repartir turnos si detecta un hipervisor
  (capítulo 7).
- **Cliente-servidor.** Es la idea del micronúcleo llevada a todo el sistema: un cliente pide un servicio a un proceso
  servidor con mensajes, y da igual que el servidor esté en la misma máquina o en otra (capítulo 10).

La estructura decide qué se puede aislar y cuánto cuesta cada servicio. Ninguna es la mejor en todo, y los sistemas
reales mezclan varias.

### 1.8 Breve historia: de los lotes al PC

- **Lotes** (años 50 y 60): un operador carga trabajos y la máquina los ejecuta de uno en uno; la CPU espera mientras se
  lee una cinta.
- **Multiprogramación**: varios trabajos a la vez en memoria; mientras uno espera la E/S, la CPU atiende a otro
  (capítulos 5 y 6). Obliga a tener protección de memoria.
- **Tiempo compartido**: muchos usuarios interactivos sobre una misma máquina, con rodajas cortas de CPU. De ahí nació
  Unix.
- **Ordenador personal**: sistemas pequeños de una sola tarea (como MS-DOS) que con los años recuperaron la multitarea,
  la memoria virtual y la protección de los grandes.

SaguiOS es un sistema de PC de la última etapa: multitarea apropiativa, memoria protegida, usuarios y red.

## 2. Así lo hace SaguiOS

SaguiOS-Extended es un sistema **educativo de 32 bits (i686)**. Su núcleo es **monolítico**: el planificador, la memoria,
los sistemas de archivos, los drivers y la pila de red se compilan en una sola imagen que corre con privilegios de
núcleo. El árbol lo muestra: `kernel/nucleo/` (procesos), `kernel/mem/` (memoria), `kernel/fs/`
(archivos), `kernel/dispositivos/` (drivers) y `kernel/rede/` (red) son carpetas del mismo núcleo.

Algunos puntos concretos:

- **Un solo espacio privilegiado.** El código de `kernel/fs/vfs.c` llama directamente a `kernel/fs/ext2.c` o a
  `kernel/fs/fat32.c`, sin mensajes. Es la ventaja del monolítico que se nota en el camino de una llamada
  (capítulo 3).
- **Las bibliotecas de usuario son aparte.** La libc (`userland/libc/`) y la libsagui (`userland/libsagui/`) son código
  de programa, no del núcleo. Traducen funciones como `write` a la llamada `int 0x80` del capítulo 3. Desde la decisión
  **D462** casi todos los programas de `/bin` las cargan como bibliotecas compartidas (`/lib/libsagui.so.1`,
  `/lib/libc.so.1`; capítulo 6) y solo lo esencial para arrancar y reparar va enlazado de forma estática.
- **Cómo se construye.** El sistema se compila en el PC con `make` (GCC 13 o GCC 15: la decisión **D464** arregló los
  pocos puntos que fallaban con el C23 de GCC 15) y se prueba como invitado de QEMU.
- **Código de otros proyectos aislado.** Los drivers tomados de otros proyectos viven en `kernel/terceros/`, con su
  licencia registrada (decisiones D034 y D073). Así se aísla lo que no es del proyecto, aunque siga siendo parte del
  núcleo.
- **El núcleo se documenta en el propio sistema.** La guía del kernel está en `/ayuda/guias/guia-kernel.md`; el
  repositorio la tiene en `docs/guia-kernel.md`.

**Dónde está cada cosa en el repositorio:**

| Parte | Carpeta | Ejemplo de fichero |
| --- | --- | --- |
| Arranque | `boot/` | `boot/mbr.asm` |
| Núcleo (procesos, llamadas) | `kernel/nucleo/` | `kernel/nucleo/processos.c` |
| Memoria | `kernel/mem/` | `kernel/mem/paginacao.c` |
| Sistemas de archivos | `kernel/fs/` | `kernel/fs/vfs.c` |
| Drivers | `kernel/dispositivos/` | `kernel/dispositivos/ahci.c` |
| Red | `kernel/rede/` | `kernel/rede/tcp.c` |
| Biblioteca C y gráfica de usuario | `userland/libc/`, `userland/libsagui/` | `userland/libc/posix.c` |
| Programas | `userland/programas/` | `userland/programas/ps.c` |

> **Decisión relevante.** D157, D164, D165 y D169 (SMP): el núcleo sigue siendo un solo programa y, con varias CPU, cada
> una atiende sus propias llamadas al sistema, pero **entran al núcleo de una en una**, bajo un cerrojo general que
> reparte los turnos por orden de llegada. Lo veremos en los capítulos 5 y 7.

### 2.1 Decisiones, alternativas y límites actuales

- **Decisión: núcleo monolítico en un solo binario de 32 bits.** Permite leer el sistema entero, seguir una llamada de
  principio a fin (capítulo 3) y no pagar el coste de los mensajes en máquinas antiguas. La guía del kernel lo explica
  desde su primer apartado (`docs/guia-kernel.md`).
- **Límite: solo i686.** Los modos de 64 bits no se usan (guía del kernel, apartado 1). La decisión de ampliar a 64 bits
  está en la hoja de ruta, no en esta versión.
- **Límite: un fallo en un driver afecta a todo.** SaguiOS mitiga los fallos graves con una pantalla de excepción
  (`kernel/arch/excecoes.c`), no con aislamiento de procesos. Lo que sí aísla es el código de otros proyectos, pero por
  **licencia** (decisiones D034 y D073), no por privilegio.
- **Límite: el núcleo se escribió para un procesador.** La decisión **D165** pone un cerrojo general: cada CPU atiende sus
  llamadas, pero solo una está dentro del núcleo a la vez (capítulo 7). Es más simple y seguro, a costa de escalar.

## 3. Experimento en SaguiOS

1. Abre el **Terminal** (Inicio > Accesorios) y escribe `ps`. Verás los procesos con su PID, estado y memoria. Cada
   línea es un **proceso** del modelo de la teoría.
2. Escribe `uname` y `uptime`. Son programas de usuario que piden datos al núcleo con llamadas al sistema.
3. Abre el **Administrador de tareas** (Inicio > Sistema > Administrador de tareas). Compara lo que muestra con `ps`.
4. Escribe `ayuda ps` para ver la documentación del programa dentro del propio sistema.

![Terminal con la orden ps mostrando procesos del sistema](img/cap-terminal-ps.png)

**Qué observar:** los procesos del kernel aparecen en la lista junto a los programas de usuario, porque ambos forman parte
del mismo sistema, pero los del núcleo no ocupan memoria de usuario.

**Ejercicio práctico adicional.** En el repositorio (en el PC), ejecuta `du -sh kernel/*` y ordena las carpetas por
tamaño. Compara `kernel/dispositivos/` con `kernel/nucleo/`. Explica por qué los drivers ocupan tanto en un núcleo
monolítico y qué consecuencia tiene para la fiabilidad del sistema.

## 4. Preguntas de repaso

1. ¿Qué diferencia hay entre el **núcleo** y un programa de usuario? Da un ejemplo de cada uno en SaguiOS.
2. Un driver de sonido falla y provoca una excepción en el núcleo. ¿Qué pasa en un monolítico y en un micronúcleo?
3. ¿Por qué un sistema puede ser monolítico y aun así tener partes aisladas, como `kernel/terceros/`?
4. Indica dos funciones de un sistema operativo que cumpla un intérprete de órdenes como `sagsh` (no el núcleo).

5. Con los números del apartado 1.5, ¿qué coste de cada cambio de proceso haría que la llamada por mensajes costara lo
   mismo que la monolítica (0,8 µs)? ¿Es realista?
6. Un driver de disco dentro del núcleo corrompe la memoria que usa el planificador. Enumera las consecuencias en un
   monolítico y en un micronúcleo.
7. Un hipervisor reparte una máquina entre dos sistemas invitados. Indica dos ventajas y dos inconvenientes frente a
   instalar cada sistema en su propia máquina.
8. ¿Por qué la multiprogramación necesita protección de memoria y un sistema que ejecuta un solo programa a la vez no?

## 5. Soluciones

1. El núcleo corre con privilegios de hardware y gestiona recursos (por ejemplo `kernel/nucleo/processos.c`). Un programa
   de usuario (por ejemplo `ps`, en `userland/programas/ps.c`) solo pide servicios.
2. En un monolítico, el fallo ocurre en el espacio privilegiado y puede tumbar todo el sistema. En un micronúcleo, el driver
   es un proceso: puede reiniciarse sin que se caiga el resto.
3. El aislamiento puede ser de **código y licencia**, no solo de privilegio: `kernel/terceros/` separa lo ajeno para
   gestionar su licencia, aunque siga compilado dentro del núcleo.
4. Por ejemplo: ejecutar órdenes en secuencia y con tuberías (usa `fork`, `execve` y `pipe`), y dar un entorno de trabajo
   con variables y redirecciones. El intérprete usa las mismas llamadas que cualquier programa.

### Respuestas a las preguntas adicionales

5. Hay que igualar 2c + 0,3 = 0,8, así que c = 0,25 µs por cambio de proceso. Con cambios de 2 µs es imposible igualar:
   el mensaje siempre será más caro, salvo que se eviten los cambios de proceso. Por eso los micronúcleos modernos
   reducen los cambios y copian menos.
6. En el monolítico, el driver y el planificador comparten espacio: el sistema entero puede caer o comportarse de forma
   incorrecta. En el micronúcleo, el driver es un proceso: su fallo no toca el planificador (si el núcleo mínimo es
   correcto) y el servicio puede reiniciarse.
7. Ventajas: se aprovecha mejor el hardware (varias máquinas lógicas en una física) y es fácil probar, copiar o restaurar un
   sistema sin tocar el resto (así se prueba SaguiOS en QEMU). Inconvenientes: hay una capa más que cuesta rendimiento
   (las instrucciones privilegiadas del invitado las atiende el hipervisor) y el invitado no controla cuándo corre su
   CPU virtual.
8. Con un solo programa no hay a quién estropear salvo el propio sistema; con varios, un error de uno podría escribir en la
   memoria de otro o del núcleo. Hace falta hardware que lo impida (modo núcleo y paginación; capítulos 3 y 6).
