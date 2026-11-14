# PRD: SaguiOS, un sistema operativo escrito desde cero por IA

> Nombre del sistema: **SaguiOS**.
> Hardware objetivo: **Lenovo G470 (modelo 20078)** y máquina virtual (QEMU / VirtualBox).
> Entorno de trabajo: VPS Hostinger KVM 8 con Ubuntu 26.04, accesible por SSH, sesión tmux.

---

## 0. Cómo usar este documento (léelo primero, agente)

1. Este PRD es la única especificación del proyecto. Sigue el orden de las fases y de las historias.
2. Antes de cada historia, vuelve a leer su sección y los criterios de aceptación. Márcala como hecha solo cuando todos los criterios pasen en las pruebas automáticas.
3. Regístralo todo (sección 5). Eli lo sigue a través de `status.md` y del dashboard, y lo va a mostrar en un vídeo.
4. Trabaja en modo autónomo (sección 10) **dentro de cada fase**. Al terminar una fase, **detente** y avisa a Eli (sección 8). Empieza la fase siguiente solo después de que él la pruebe y dé luz verde.
5. Toda decisión que este PRD no cubra: elige la opción más sencilla que funcione en el G470, anótala en `docs/decisoes.md` con el motivo y sigue adelante.

---

## 1. Objetivo

Escribir, desde cero, un sistema operativo mínimo para PC x86 que:

1. **Arranque** desde un pendrive, en el Lenovo G470 real, con cargador de arranque propio.
2. Tenga un **kernel de verdad**: interrupciones, gestión de memoria con paginación, procesos con planificador, llamadas al sistema y programas que se ejecutan en modo usuario (ring 3), cargados desde archivos ELF.
3. Muestre un **terminal** donde se puedan escribir comandos y navegar por los archivos. Los comandos son programas independientes, no funciones del kernel.
4. Abra un **modo gráfico sencillo**, con ratón (touchpad), ventanas y **programas gráficos en ring 3**: Archivos, Terminal, Editor de texto, Calculadora y el juego Sagui Atrapa Bananas.
5. **Lea y escriba archivos en un pendrive FAT32**, con driver USB propio, para que los archivos sobrevivan al apagar.

No tiene por qué ser bonito ni completo. La meta es demostrar que la IA es capaz de escribir las partes fundamentales de un sistema operativo.

**Meta de tamaño:** la imagen final debe caber en **6 MB** (D050). La meta original era **1,44 MB**, el tamaño de un disquete; se amplió cuando entraron el compilador de C, MicroPython, el shell completo y las aplicaciones nuevas. No es obligatorio, pero es el objetivo.

### 1.1 Qué hace que esto sea un sistema operativo (criterio de Tanenbaum)

El público va a exigir que esto sea un sistema operativo de verdad, no una distribución. La referencia es el libro *Modern Operating Systems*, de Andrew Tanenbaum, usado en las carreras de informática de todo el mundo. Para él, el sistema operativo tiene dos papeles:

1. **Máquina extendida:** oculta los detalles del hardware y ofrece abstracciones más sencillas a los programas (procesos, espacios de direcciones, archivos).
2. **Gestor de recursos:** reparte la CPU, la memoria y los dispositivos entre los programas, con protección entre ellos.

El sistema operativo, en este sentido, es el software que se ejecuta en **modo kernel**. El shell y la interfaz gráfica son programas que *usan* el sistema operativo. Por eso, en este proyecto, lo que importa es el kernel, y el shell se ejecuta como un programa normal en ring 3.

Al final, todos los elementos siguientes tienen que estar implementados y demostrados con pruebas y capturas de pantalla. La organización sigue los conceptos básicos del capítulo 1 del libro y los temas de los capítulos siguientes:

| Concepto del libro | Qué implementa SaguiOS | Historia |
| --- | --- | --- |
| Modo kernel y modo usuario | Kernel en ring 0, programas en ring 3 | H03, H12 |
| Procesos | Creación, terminación, estados, PID, proceso padre e hijo | H11, H13 |
| Planificación | Round-robin expropiativo con el reloj | H11 |
| Comunicación entre procesos | Pipes entre programas (`ls \| conta`) | H15 |
| Espacios de direcciones | Paginación con un espacio por proceso | H10 |
| Gestión de memoria | Memoria física, asignador del kernel, memoria de los procesos | H06, H10, H13 |
| Archivos y directorios | Abrir, leer, escribir, crear y borrar archivos y carpetas, en la RAM y en un pendrive FAT32 | H08, H14, H32, H33 |
| Entrada y salida | Interrupciones y drivers propios: teclado, ratón, vídeo, reloj, USB y almacenamiento | H05, H07, H17, H19, H29 a H31 |
| Protección | Un proceso no accede a la memoria de otro ni al kernel | H12, H16 |
| Llamadas al sistema | Procesos, archivos, directorios y varias (sección 4.5) | H12 |
| El shell | `sagsh`, un programa en ring 3, con redirección y pipes | H16 |
| Carga de programas | Ejecutables ELF leídos del sistema de archivos | H13 |
| Interfaz gráfica (extra) | Gestor de ventanas en el kernel; programas gráficos en ring 3 | H20 a H28 |

Fuera del alcance, por ser temas avanzados del libro: hilos, multiprocesador, memoria virtual con intercambio a disco, virtualización y usuarios con permisos.

---

## 2. Reglas del desafío

Estas reglas existen porque el público va a examinar el vídeo con lupa. Síguelas al pie de la letra.

### Se puede

- Usar **herramientas** en la VPS: compilador (GCC), ensamblador (NASM), enlazador, `make`, Python para scripts de build, QEMU para probar, ImageMagick para convertir capturas de pantalla. Las herramientas no van dentro del sistema.
- Leer **especificaciones y manuales oficiales** para entender cómo funcionan el hardware y los formatos: manuales de Intel (Software Developer's Manual), especificación VESA BIOS Extensions, especificación del formato ELF, documentación del controlador PS/2 (i8042), del PIT, del PIC y del CMOS, especificación PCI, especificación USB 2.0, especificación EHCI de Intel, especificación USB Mass Storage (Bulk-Only Transport), comandos SCSI básicos y la especificación del sistema de archivos FAT32 de Microsoft.
- Escribir scripts auxiliares que se ejecuten en la VPS (por ejemplo, el empaquetador del sistema de archivos).

### No se puede

- **Acceder al código fuente de otros sistemas operativos.** No abrir, clonar, leer ni buscar repositorios de ningún sistema operativo, kernel o cargador de arranque, en ningún sitio (GitHub, GitLab, Codeberg, SourceForge, etc.). Esto incluye proyectos personales y de aficionados, Linux, Minix, KolibriOS y cualquier proyecto citado en los comentarios del vídeo. Si dudas de si un enlace es código de un sistema operativo, no lo abras.
- **Copiar código** de ningún sitio: tutoriales, wikis (incluida la OSDev Wiki), respuestas de foros, blogs. Todo el código de la imagen se escribe en este proyecto. Se puede leer documentación conceptual; nunca se copian fragmentos de código.
- Usar **bibliotecas ya hechas** dentro del sistema (ni siquiera libc). Todo lo que usen el kernel y los programas se escribe aquí, incluida la biblioteca de los programas de usuario (`libsagui`).
- Usar **GRUB, Limine, Syslinux** ni ningún cargador de arranque ya hecho. El cargador es nuestro.
- Usar **fuentes o imágenes ya hechas**. La fuente bitmap y cualquier dibujo (incluida la mascota sagüí) los crea la IA en este proyecto, como datos en el código.
- Usar nombres de componentes de otros proyectos. Los nombres de SaguiOS son propios y en portugués (p. ej.: "Arquivos", "Terminal", `sagsh`); en esta versión traducida, la interfaz los muestra en español (p. ej.: "Archivos").
- Crear programas o juegos que imiten los de proyectos citados en los comentarios del vídeo (por ejemplo: la serpiente, arkanoid, space invaders). Los programas de SaguiOS son solo los definidos en este PRD.

Si en algún momento parece necesario saltarse una regla, **no te la saltes**: regístralo en `docs/decisoes.md` y busca otra salida.

### 2.1 Principio de autoría

Este es el punto más importante del PRD. SaguiOS tiene que ser **de autoría propia**: solo los **conceptos** se aprenden de fuera (qué es un proceso, cómo funciona la paginación, qué hace un pipe). La **implementación** es enteramente nuestra.

- **No reproducir código de memoria.** Conoces código de proyectos famosos (Linux, Minix, xv6, ejemplos de libros y tutoriales). No escribas nada a partir de ese recuerdo. Para cada parte, piensa la solución a partir del concepto y de la especificación del hardware, y escribe tu propia versión.
- **Los libros de texto valen solo por los conceptos.** El libro de Tanenbaum incluye el código de Minix, y otros libros incluyen el código de xv6. Ese código no se puede usar ni siquiera como modelo.
- **Estructura, nombres y comentarios propios.** La organización de carpetas, los nombres de funciones, de estructuras y de variables, y los comentarios se crean en este proyecto. Comentarios en portugués, explicando el porqué de cada decisión.
- **Lo que es igual por obligación no es problema.** Los valores definidos por el hardware o por las especificaciones (direcciones de puertos, números de interrupciones, formato de las cabeceras ELF, bits de las tablas de páginas, la firma 0x55AA) son iguales en cualquier sistema. Eso no es copia: es el hardware el que lo exige.
- **Registrar el razonamiento.** Para cada parte central (cargador de arranque, paginación, planificador, llamadas al sistema, sistema de archivos, pipes, gestor de ventanas, driver USB, FAT32), escribir en `docs/decisoes.md` cómo se pensó la solución y por qué se eligió. Esa es la prueba de autoría para el público.
- **En caso de duda, hazlo distinto.** Si una solución se parece demasiado a algo conocido, cambia el enfoque y regístralo.

---

## 3. Hardware objetivo y entorno de pruebas

### 3.1 Lenovo G470 (modelo 20078)

Portátil de 2011, plataforma Intel Sandy Bridge. Supuestos (a confirmar en la prueba real):

| Pieza | Supuesto | Cómo lo usa el sistema |
| --- | --- | --- |
| Firmware | BIOS Insyde con arranque tradicional (legacy) | Cargador de arranque propio, en modo real |
| Arranque desde el pendrive | La BIOS emula el pendrive como disco duro | Lectura con `INT 13h` extendida (AH=42h) |
| Teclado interno | Controlador PS/2 (i8042), teclado **ABNT2** | Driver PS/2 propio, IRQ1, distribución ABNT2 (en esta versión, también ES, que es la distribución por defecto) |
| Touchpad | Modo ratón PS/2 estándar | Driver de ratón PS/2 propio, IRQ12 |
| Vídeo | Intel HD Graphics con VESA VBE en la BIOS | Framebuffer lineal mediante VBE |
| Memoria | 2 GB o más | Mapa de memoria mediante `INT 15h, E820` |
| Disco interno | SATA, modo IDE o AHCI en la BIOS | Solo en el extra, y **solo lectura** |
| USB | Solo USB 2.0 (controladores EHCI del chipset Intel serie 6, con hub interno en cada controlador) | Fase 6: driver EHCI propio, solo para un **pendrive de datos** |

### 3.2 Estrategia central: todo en la RAM, más un pendrive de datos

El cargador de arranque usa la BIOS (`INT 13h`) para copiar el kernel y el sistema de archivos (ramdisk) del pendrive de arranque a la memoria **antes** de entrar en modo protegido. Después, todo el sistema se ejecuta desde la RAM y nunca vuelve a leer el pendrive de arranque. Esto permite que las fases 1 a 5 funcionen sin ningún driver USB.

En la fase 6, el kernel incorpora un driver USB propio para un **segundo pendrive, de datos**, formateado en FAT32 (por ejemplo, en Windows). En él, el usuario guarda y abre archivos que sobreviven al apagado.

**Seguridad de los datos:** el sistema nunca escribe en el disco interno del portátil. Solo escribe en pendrives USB que el usuario haya montado explícitamente con `montar`. Antes de borrar nada del pendrive, los programas piden confirmación.

### 3.3 Entorno de pruebas en la VPS

- QEMU sin KVM (la VPS no tiene virtualización anidada). Es lento, pero suficiente para un sistema pequeño.
- Ejecutar siempre sin ventana (`-display none`), con salida serie a un archivo y el monitor de QEMU para enviar teclas, mover el ratón y hacer capturas de pantalla (`screendump`).
- Probar **dos vías de arranque**:
  1. Imagen como disco IDE: `-drive format=raw,file=build/saguios.img`
  2. Imagen como **pendrive USB** (simula el G470): controlador EHCI + `usb-storage`, arranque con la BIOS de QEMU.
- A partir de la fase 6, un **segundo dispositivo USB** de datos: una imagen FAT32 creada en la VPS con `mkfs.vfat` (herramienta), conectada al mismo controlador EHCI. Después de la prueba, comprobar el contenido grabado con `mtools` en la VPS.

---

## 4. Arquitectura técnica (decisiones fijas)

| Elemento | Decisión | Motivo |
| --- | --- | --- |
| Arquitectura | x86 32 bits, modo protegido | Más sencillo que 64 bits, funciona en el G470 y en cualquier VM |
| Lenguajes | Ensamblador (NASM) en el arranque y donde haga falta; C (freestanding) en el resto | Lo habitual en kernels pequeños |
| Compilador | Preferencia: compilador cruzado `i686-elf` (binutils + GCC compilados desde el código fuente en la VPS). Alternativa: GCC del sistema con `-m32 -ffreestanding -nostdlib -fno-pic -fno-stack-protector` | El compilador es una herramienta, no va en la imagen |
| Formato de la imagen | Imagen de disco en bruto (`.img`) con MBR y tabla de particiones, más una copia convertida a `.vdi` | El `.img` es lo que la BIOS del G470 acepta desde el pendrive; el `.vdi` es para VirtualBox |
| Grabación en el pendrive | Rufus (con `.img` graba byte a byte, en modo DD), balenaEtcher o `dd` | En el proyecto anterior Rufus falló con una ISO; con un `.img` en bruto no altera la imagen. Si da problemas, usar balenaEtcher |
| Modo de ejecución | Kernel en ring 0; programas en ring 3 a partir de la fase 3 | Separación real entre kernel y programas |
| Paginación | Obligatoria a partir de la fase 3: paginación de 2 niveles (4 KB), un directorio de páginas por proceso, kernel mapeado en todos | Aislamiento entre procesos |
| Multitarea | Expropiativa, round-robin, con el tic del PIT | Planificador sencillo y demostrable |
| Llamadas al sistema | `int 0x80`, número en EAX, argumentos en EBX, ECX, EDX (sección 4.5) | Fácil de depurar |
| Programas | ELF32 estático, enlazado en la dirección virtual 0x40000000 | Formato estándar, cargador escrito por nosotros |
| Interfaz gráfica | Gestor de ventanas en el kernel; todos los programas gráficos en ring 3, usando llamadas al sistema de ventanas | El kernel solo dibuja los marcos y entrega los eventos; los programas son programas de verdad |
| Números en la calculadora y en el juego | Enteros y punto fijo, sin usar la unidad de coma flotante del procesador | Evita tener que guardar el estado de la FPU en cada cambio de proceso |
| USB | Driver EHCI propio (fase 6), con soporte de hub, para almacenamiento masivo | Solo para el pendrive de datos; nada de teclado ni ratón USB |

### 4.1 Disposición de la imagen de disco

| Región | Contenido |
| --- | --- |
| LBA 0 | **MBR**: código de la etapa 1 + tabla de particiones con 1 partición activa que cubre la imagen. Reservar el área de un BPB falso al principio (algunas BIOS escriben en esa área al arrancar por USB). |
| LBA 1 | **Cabecera de arranque** de SaguiOS: firma, LBA y tamaño de la etapa 2, del kernel y del ramdisk |
| A continuación | **Etapa 2** del cargador (ensamblador, modo real) |
| A continuación | **Kernel** (binario plano) |
| A continuación | **Ramdisk** (sistema de archivos SGFS, sección 4.3) |

Todos los desplazamientos los genera el script de build. Nada fijo en el código salvo el LBA 1.

### 4.2 Secuencia de arranque

1. **Etapa 1 (MBR, 512 bytes):** guarda el número de unidad recibido en `DL`, prepara pila y segmentos, lee la cabecera (LBA 1) y la etapa 2 con `INT 13h AH=42h` y salta a la etapa 2. Muestra un mensaje de error breve si la lectura falla.
2. **Etapa 2 (modo real):**
   1. Habilita la línea A20 (probar el método rápido por el puerto 0x92 y el del controlador de teclado, y comprobar que ha funcionado).
   2. Obtiene el mapa de memoria (`INT 15h, E820`).
   3. Lee el kernel y el ramdisk con `INT 13h AH=42h`, por bloques, mostrando el progreso en pantalla.
   4. Consulta los modos VESA (`INT 10h AX=4F00/4F01`) y elige, en este orden: 1024x768, 800x600, 640x480, con 32 o 24 bits por píxel y framebuffer lineal. Guarda la información del modo. **Hasta la fase 3 no cambia de modo**: se queda en modo texto.
   5. Monta la GDT, entra en modo protegido y salta al kernel, pasándole una estructura `boot_info` (mapa de memoria, unidad, dirección y tamaño del ramdisk, datos del modo VESA).
3. **Carga por encima de 1 MB:** `INT 13h` solo escribe por debajo de 1 MB. Estrategia recomendada: cargar por bloques en un búfer bajo y copiarlos por encima de 1 MB usando *unreal mode*. Alternativa aceptada: si kernel + ramdisk caben con holgura por debajo de 0x9F000, cargarlos directamente en 0x10000. Registrar la elección.

### 4.3 Sistema de archivos SGFS (formato propio)

Formato sencillo, creado en este proyecto. La imagen del pendrive es de solo lectura; encima de ella, el kernel mantiene una **capa escribible en la RAM** (historia H14), donde se pueden crear, modificar y borrar archivos y carpetas durante la sesión. Al apagar, los cambios se pierden, como en un sistema live.

Una **capa de archivos virtual** en el kernel lo une todo en una sola ruta: `/` es el SGFS con la capa de la RAM, y `/usb` es el pendrive de datos cuando está montado (fase 6). Los programas no necesitan saber dónde está el archivo.

- Cabecera: firma `SGFS`, versión, número de entradas.
- Tabla de entradas: ruta completa (p. ej.: `docs/sobre.txt`, hasta 56 bytes), tipo (archivo, carpeta o programa), tamaño, desplazamiento de los datos.
- Área de datos.

Un script de Python en la VPS (`tools/mksgfs.py`) monta el SGFS a partir de la carpeta `rootfs/`. Contenido de `rootfs/`, escrito por la IA:

- `leia-me.txt`: bienvenida a SaguiOS y cómo usar los comandos.
- `sobre.txt`: cómo se hizo el sistema, quién lo hizo y las reglas del desafío.
- `historia/linux.txt`: un resumen de la historia de Minix, Linux y GNU.
- `historia/saguios.txt`: el diario resumido de este proyecto, generado al final.
- `arte/sagui.txt`: la mascota en arte ASCII.
- `bin/`: los programas ELF de la fase 3, generados a partir de la carpeta `userland/` en el build.
- `tmp/`: carpeta vacía, para pruebas de escritura.

### 4.4 Texto, acentos y teclado

- Todo el sistema en **portugués de Brasil** (en esta versión traducida, la interfaz está en **español de España**).
- La codificación interna del texto es **Latin-1 (ISO-8859-1)**, para tener acentos sin implementar UTF-8. Los archivos de `rootfs/` se convierten a Latin-1 en el build.
- En las fases 1 a 3 (modo texto VGA) los acentos pueden salir mal por culpa de la fuente de la tarjeta de vídeo. Es aceptable. En la fase 4, la fuente propia ya tiene los acentos.
- **Distribución ABNT2** con scan code set 1 (el estándar traducido por el controlador). En esta versión traducida, el teclado admite además la **distribución ES** (española), que es la predeterminada; se cambia en tiempo de ejecución con el programa `teclado` (`teclado es` / `teclado abnt2`). Correspondencia de las teclas ABNT2 que cambian con respecto al teclado estadounidense:

| Scan code | Normal | Con Shift |
| --- | --- | --- |
| 0x29 | ' | " |
| 0x07 (tecla 6) | 6 | ¨ (tecla muerta) |
| 0x1A | ´ (tecla muerta) | ` (tecla muerta) |
| 0x1B | [ | { |
| 0x27 | ç | Ç |
| 0x28 | ~ (tecla muerta) | ^ (tecla muerta) |
| 0x2B | ] | } |
| 0x56 | \ | \| |
| 0x33 | , | < |
| 0x34 | . | > |
| 0x35 | ; | : |
| 0x73 | / | ? |

- Las teclas muertas se combinan con vocales: á é í ó ú, à, â ê ô, ã õ, ü. Una combinación inválida muestra el acento seguido de la letra.
- Si alguna correspondencia está mal en el G470, Eli lo indica en la prueba real y la corrección se convierte en una entrada en `docs/erros.md`.

### 4.5 Llamadas al sistema (mínimo)

Organizadas en los mismos grupos que usa el libro de Tanenbaum:

| Grupo | Llamadas | Función |
| --- | --- | --- |
| Procesos | `executar`, `esperar`, `sair`, `pid`, `matar` | Crear un proceso a partir de un ELF, esperar al hijo, terminar, consultar el PID, terminar otro proceso |
| Archivos | `abrir`, `ler`, `escrever`, `fechar`, `posicionar`, `info_arquivo` | Leer y escribir archivos; el terminal también es un "archivo" (entrada 0, salida 1) |
| Directorios | `criar_pasta`, `remover_pasta`, `apagar`, `listar`, `mudar_pasta` | Manipular carpetas y entradas |
| Comunicación | `pipe`, `duplicar` | Conectar la salida de un proceso a la entrada de otro |
| Varias | `dormir`, `tempo`, `data`, `info_sistema` | Pausa, reloj, memoria y lista de procesos |
| Ventanas (fase 5) | `criar_janela`, `fechar_janela`, `atualizar_janela`, `proximo_evento` | El programa dibuja en su propia área de memoria y pide al kernel que la muestre; recibe clics, movimientos y teclas de su ventana |
| Dispositivos (fase 6) | `listar_discos`, `montar`, `desmontar` | Ver pendrives y particiones, montar en `/usb`, grabar todo y liberar para extraer |

Convención: `int 0x80`, número de la llamada en EAX, argumentos en EBX, ECX, EDX, retorno en EAX (negativo indica error). Los nombres en el código pueden estar en inglés; los que ve el usuario están en portugués (en esta versión traducida, en español). Los punteros que vienen del ring 3 siempre los valida el kernel antes de usarlos.

---

## 5. Estructura del repositorio y documentación

```
saguios/
├── prd.md                 # este documento
├── status.md              # resumo vivo: fase, história atual, progresso, bloqueios
├── Makefile
├── boot/                  # estágio 1 e estágio 2 (NASM)
├── kernel/                # código do kernel (C + NASM)
├── userland/              # libsagui e os programas de usuário (sagsh, ls, cat, editor, calculadora, jogo...)
├── tools/                 # mksgfs.py, scripts de teste, build da imagem
├── rootfs/                # arquivos que vão para o ramdisk
├── build/                 # saída do build (saguios.img)
├── tests/                 # scripts de teste no QEMU por história
├── docs/
│   ├── historias/         # uma página por história: objetivo, critérios, resultado
│   ├── decisoes.md        # toda decisão tomada e o motivo
│   ├── diario.md          # log cronológico do que foi feito
│   ├── erros.md           # cada erro: sintoma, causa, correção
│   ├── capturas/          # PNGs das telas do QEMU, nomeados por história
│   ├── como-gravar.md     # como gravar o pendrive e testar no G470
│   └── teste-faseX.md     # roteiro de teste de cada fase para o Eli
├── dashboard/
│   ├── index.html         # painel visual (sem dependências externas)
│   └── status.json        # dados do painel, atualizados a cada história
└── entrega/               # imagens finais por fase + SHA-256
```

- Inicializar un repositorio **git** y hacer commit al final de cada historia, con el mensaje `H0X: descripción`.
- `status.md` y `dashboard/status.json` se actualizan **al final de cada historia** y siempre que haya un bloqueo.
- Las capturas de pantalla de QEMU son importantes: Eli las va a usar en el vídeo.

---

## 6. Fases e historias

Progreso del dashboard: todas las historias pesan lo mismo. Las historias extra no cuentan para el 100%.

Hay 6 fases. **Cada fase termina en una parada obligatoria**: el agente entrega la imagen, avisa a Eli y espera a que la pruebe en el G470 antes de empezar la siguiente (sección 8). Los fallos que aparezcan en esas pruebas forman parte del proyecto y del vídeo: no los escondas ni los esquives; regístralos y corrígelos.

| Fase | Objetivo | Historias |
| --- | --- | --- |
| 1. Arranque | Arrancar el G470 con cargador propio y mostrar el mensaje del kernel | H01 a H04 |
| 2. Kernel y terminal | Interrupciones, reloj, memoria, teclado ABNT2, archivos y un terminal provisional | H05 a H09 |
| 3. Procesos y programas | Paginación, procesos, ring 3, llamadas al sistema, ELF, archivos escribibles, pipes y el shell | H10 a H16 |
| 4. Gráficos y ratón | Modo gráfico, fuente propia y touchpad | H17 a H19 |
| 5. Ventanas y programas | Escritorio, ventanas y los programas: Archivos, Terminal, Editor, Calculadora y Sagui Atrapa Bananas | H20 a H28 |
| 6. Pendrive de datos | Driver USB y FAT32 para guardar y abrir archivos en un pendrive | H29 a H34 |

### Fase 1: Arranque (dar señales de vida)

**Objetivo:** arrancar el G470 con un cargador de arranque propio y mostrar el primer mensaje del kernel.

**H01. Preparar el entorno.** Instalar NASM, QEMU, ImageMagick, Python. Montar el compilador cruzado o validar el GCC del sistema con las flags freestanding. Crear la estructura de carpetas, el git, el `status.md` y el dashboard vacío.
*Aceptación:* `make` se ejecuta sin errores con un objetivo vacío; el dashboard se abre en el navegador.

**H02. Etapa 1 (MBR).** Código de 512 bytes con la firma 0x55AA, tabla de particiones válida con partición activa y área de BPB reservada.
*Aceptación:* en QEMU (IDE y USB), aparece el mensaje `SaguiOS: carregando...`.

**H03. Etapa 2 y entrada en modo protegido.** A20, mapa E820, lectura del kernel y del ramdisk, GDT, modo protegido, salto al kernel con `boot_info`.
*Aceptación:* el kernel en C escribe en pantalla en modo texto VGA: `Ola! Eu sou o SaguiOS.` y la cantidad de memoria detectada.

**H04. Imagen para el pendrive y para VirtualBox.** El script de build genera `build/saguios.img` con todas las regiones y el SHA-256, y convierte la misma imagen a `build/saguios.vdi` con `qemu-img convert -f raw -O vdi` (VirtualBox no abre `.img` directamente). Escribir `docs/como-gravar.md` con: grabación en el pendrive (balenaEtcher en Windows, `dd` en Linux), arranque en el G470 (desactivar Secure Boot si existe, elegir arranque legacy desde el pendrive, F12 en el menú de arranque) y prueba en VirtualBox (nueva VM de tipo "Other", sin EFI, usando el `.vdi` como disco).
*Aceptación:* el arranque funciona en los dos modos de QEMU; el `.vdi` se genera sin errores; archivos de entrega en `entrega/fase1/`. El `como-gravar.md` explica la grabación con Rufus (elegir la imagen `.img`; si pregunta el modo, elegir DD) y con balenaEtcher como alternativa.

> **PARADA OBLIGATORIA: fin de la fase 1.** Ver sección 8.

### Fase 2: Kernel y terminal

**Objetivo:** el kernel reacciona al hardware (interrupciones y reloj), organiza la memoria, lee el teclado ABNT2, encuentra archivos y ofrece un terminal provisional para probarlo todo.

**H05. Interrupciones y reloj.** IDT, tratamiento de las excepciones de la CPU (mostrando en pantalla el nombre de la excepción y los registros), reasignación del PIC, temporizador (PIT) a 100 Hz, contador de tiempo desde el arranque.
*Aceptación:* una prueba que provoca una división por cero muestra la pantalla de error de SaguiOS; el contador de segundos avanza.

**H06. Memoria física.** Gestor de memoria física a partir del mapa E820 (bitmap de páginas de 4 KB) y un asignador del kernel (`kmalloc`/`kfree`).
*Aceptación:* una prueba asigna y libera bloques de varios tamaños sin corromper nada; un comando de diagnóstico muestra la memoria total y la libre.

**H07. Teclado PS/2 con ABNT2.** Driver en IRQ1, búfer de teclas, Shift, Bloq Mayús, Ctrl, teclas muertas y el mapa de la sección 4.4.
*Aceptación:* en QEMU, las teclas enviadas por el monitor aparecen correctamente en pantalla, incluida la `ç`.

**H08. Sistema de archivos SGFS.** `tools/mksgfs.py` + lector en el kernel: listar carpeta, abrir archivo, leer contenido.
*Aceptación:* una prueba interna lista las entradas de `rootfs/` y lee el `leia-me.txt` entero.

**H09. Terminal provisional del kernel.** Prompt `sagui:/$` ejecutándose dentro del kernel, solo para validar teclado y archivos antes de los procesos: `ajuda`, `ls`, `cd`, `cat`, `limpar`, `mem`, `data` (reloj CMOS), `reiniciar` (reinicio mediante el controlador de teclado).
*Aceptación:* un guion de prueba escribe cada comando en QEMU y comprueba la salida con la captura de pantalla y con el puerto serie.

> **PARADA OBLIGATORIA: fin de la fase 2.** Ver sección 8.

### Fase 3: Procesos y programas en modo usuario

**Objetivo:** ejecutar varios programas a la vez, cada uno aislado en su espacio de memoria y en ring 3, pidiendo servicios al kernel mediante llamadas al sistema. Esta es la fase que cubre el núcleo de la definición de Tanenbaum (sección 1.1). No saltársela ni simplificarla.

**H10. Paginación.** Directorio y tablas de páginas, kernel mapeado en todos los espacios de direcciones, tratamiento de fallos de página (page fault) con un mensaje claro, creación y destrucción de espacios de direcciones.
*Aceptación:* una prueba crea dos espacios de direcciones, escribe en la misma dirección virtual en cada uno y confirma valores distintos; un acceso inválido genera la pantalla de page fault.

**H11. Procesos y planificador.** Estructura de proceso (PID, estado, registros, espacio de direcciones, pila del kernel), cambio de contexto en ensamblador, planificador round-robin expropiativo con el tic del PIT, proceso ocioso.
*Aceptación:* dos tareas de prueba imprimen `A` y `B` alternados por el puerto serie sin cooperar entre sí.

**H12. Ring 3 y llamadas al sistema.** TSS con la pila del kernel, segmentos de usuario en la GDT, entrada en ring 3, `int 0x80` con las llamadas de la sección 4.5, validación de punteros.
*Aceptación:* un programa de prueba en ring 3 escribe `Ola do ring 3!` mediante la llamada `escrever` y sale con `sair`; una instrucción privilegiada (`cli`) en ring 3 termina solo ese proceso.

**H13. Cargador ELF y libsagui.** Lector de ELF32 estático (cabeceras de programa, mapeo de las secciones en el espacio del proceso, pila de usuario con argumentos). Biblioteca `libsagui` escrita en este proyecto: llamadas al sistema, funciones de texto y memoria, un `printf` sencillo.
*Aceptación:* el kernel carga `bin/ola` del SGFS y el programa imprime los argumentos recibidos.

**H14. Archivos y carpetas escribibles.** Capa escribible en la RAM encima del SGFS: crear, escribir, sobrescribir y borrar archivos; crear y eliminar carpetas; tabla de archivos abiertos por proceso; el terminal como entrada 0 y salida 1 de cada proceso.
*Aceptación:* una prueba en ring 3 crea `tmp/teste.txt`, escribe, cierra, vuelve a abrir, lee el mismo contenido, lo borra y confirma que ha desaparecido; lo mismo con una carpeta.

**H15. Pipes (comunicación entre procesos).** Llamadas `pipe` y `duplicar`, búfer en el kernel, bloqueo de quien lee cuando el pipe está vacío y de quien escribe cuando está lleno, fin de archivo cuando el escritor cierra.
*Aceptación:* una prueba conecta dos procesos con un pipe y transfiere un texto mayor que el búfer sin perder ni duplicar datos.

**H16. Shell y comandos como programas.** El kernel inicia `bin/sagsh` como primer proceso. El terminal provisional de la H09 se elimina. El shell admite redirección (`>`, `>>`, `<`), pipe (`|`), historial con la flecha arriba y autocompletado de nombres con Tab.

Los comandos clásicos usan los nombres que cualquier usuario de Linux conoce, y cada uno tiene además un alias en portugués. Los comandos propios de SaguiOS tienen nombre en portugués.

| Comando | Alias | Tipo | Función |
| --- | --- | --- | --- |
| `ls [pasta]` | `listar` | programa | Lista archivos y carpetas |
| `cd <pasta>` | `entrar` | interno | Entra en una carpeta |
| `pwd` | `onde` | interno | Muestra la carpeta actual |
| `cat <arquivo>` | `ver` | programa | Muestra el contenido de un archivo |
| `echo <texto>` | `eco` | programa | Escribe un texto (con `>` lo graba en un archivo) |
| `cp <origem> <destino>` | `copiar` | programa | Copia un archivo (también entre la RAM y el pendrive) |
| `mv <origem> <destino>` | `mover` | programa | Mueve o renombra |
| `rm <arquivo>` | `apagar` | programa | Borra un archivo (pide confirmación en el pendrive) |
| `mkdir <pasta>` | `criar-pasta` | programa | Crea una carpeta |
| `rmdir <pasta>` | `remover-pasta` | programa | Elimina una carpeta vacía |
| `wc` | `conta` | programa | Cuenta líneas, palabras y letras |
| `ps` | `processos` | programa | Lista los procesos en ejecución |
| `kill <pid>` | `matar` | programa | Termina un proceso |
| `clear` | `limpar` | interno | Limpia la pantalla |
| `ajuda [comando]` | | interno | Lista todos los comandos con una línea de explicación en portugués, o explica un comando |
| `mem` | | programa | Memoria total, usada y libre |
| `data` | | programa | Fecha y hora |
| `sobre` | | programa | Versión y créditos de SaguiOS |
| `ola` | | programa | Imprime los argumentos recibidos (prueba) |
| `contador` | | programa | Cuenta segundos, para mostrar la multitarea |
| `falha` | | programa | Provoca un error a propósito, para mostrar la protección |
| `reiniciar` | | interno | Reinicia el ordenador |
| `sair` | | interno | Cierra el shell |

En la fase 5 se añaden los comandos que abren los programas gráficos (`arquivos`, `editor [arquivo]`, `calculadora`, `banana`). En la fase 6, `discos`, `montar` y `desmontar`.

*Aceptación:* un guion ejecuta todos los comandos y alias, incluidos `echo ola > tmp/a.txt`, `cat tmp/a.txt`, `cp tmp/a.txt tmp/b.txt`, `ls bin | conta` y `contador` ejecutándose junto con otro comando; `falha` tumba solo su propio proceso y el shell sigue funcionando; `ps` muestra los procesos; `ajuda` lo lista todo. Captura de pantalla de cada caso en `docs/capturas/`.

> **PARADA OBLIGATORIA: fin de la fase 3.** Ver sección 8.

### Fase 4: Modo gráfico y ratón

**Objetivo:** sustituir el modo texto por una pantalla gráfica dibujada píxel a píxel, con fuente propia y el touchpad funcionando.

**H17. Framebuffer VESA.** La etapa 2 cambia al modo VESA elegido. El kernel dibuja en un búfer en memoria y lo copia a la pantalla (doble búfer). Primitivas: píxel, rectángulo, línea, rectángulo con borde.
*Aceptación:* una captura muestra un fondo de color y formas dibujadas en la resolución detectada. Si VESA falla, el sistema vuelve al modo texto y avisa.

**H18. Fuente propia.** Fuente bitmap 8x16 dibujada en este proyecto, con ASCII completo y los caracteres acentuados del portugués en Latin-1. El terminal pasa a funcionar en modo gráfico.
*Aceptación:* una captura muestra el terminal gráfico con el texto `Ação, vovô, pão, ÇÃO, à é í ó ú` correcto.

**H19. Ratón / touchpad PS/2.** Habilitar el puerto auxiliar del i8042, IRQ12, paquetes de 3 bytes, cursor dibujado por nosotros, botones izquierdo y derecho.
*Aceptación:* los movimientos enviados por el monitor de QEMU mueven el cursor; el clic se detecta y se registra por el puerto serie.

> **PARADA OBLIGATORIA: fin de la fase 4.** Ver sección 8.

### Fase 5: Ventanas y programas

**Objetivo:** escritorio con ventanas y los programas gráficos en ring 3: Archivos, Terminal, Editor de texto, Calculadora y Sagui Atrapa Bananas.

**H20. Escritorio.** Fondo con degradado en los colores de Brasil, la mascota sagüí en pixel art dibujada por la IA en el código, barra superior con el nombre del sistema y el reloj (CMOS).
*Aceptación:* captura del escritorio en `docs/capturas/`.

**H21. Gestor de ventanas y llamadas al sistema gráficas.** En el kernel: lista de ventanas, marco con barra de título y botón de cerrar, arrastrar con el ratón, ventana activa encima, foco del teclado. Llamadas de la sección 4.5 (grupo Ventanas): cada programa dibuja en un área de memoria propia y pide al kernel que la muestre; el kernel entrega a cada programa solo los eventos de su ventana.
*Aceptación:* dos programas de prueba en ring 3 abren una ventana cada uno; la prueba arrastra una, cierra la otra, y el programa cerrado termina por sí solo.

**H22. Kit gráfico de libsagui.** Componentes dibujados por el propio programa, en ring 3: texto con la fuente propia, botón, etiqueta, cuadro de texto de una línea, área de texto de varias líneas con desplazamiento, lista con selección, cuadro de confirmación ("¿Seguro?").
*Aceptación:* un programa de demostración muestra todos los componentes funcionando con ratón y teclado; captura en `docs/capturas/`.

**H23. Archivos.** Programa en ring 3 que lista carpetas con iconos de carpeta, archivo y programa. Doble clic: en una carpeta entra, un `.txt` se abre en el Editor, un programa se ejecuta. Botones para volver atrás, crear carpeta y borrar (con confirmación).
*Aceptación:* la prueba navega hasta `historia/`, abre `linux.txt` en el Editor, crea una carpeta en `tmp/` y la borra.

**H24. Terminal en ventana.** Programa en ring 3 que abre una ventana y ejecuta `sagsh` dentro de ella, conectado por pipes. Desde el terminal, los comandos `arquivos`, `editor [arquivo]`, `calculadora` y `banana` abren los programas gráficos.
*Aceptación:* captura con el terminal en ventana ejecutando `ps` y `ls bin | conta`.

**H25. Editor de texto.** Programa en ring 3: nuevo, abrir, guardar, guardar como; escritura con acentos (ABNT2; en esta versión, también ES), flechas, Inicio, Fin, Retroceso, Supr, Intro, desplazamiento; Ctrl+S guarda; el título de la ventana muestra el nombre del archivo y un asterisco cuando hay cambios sin guardar; aviso al cerrar sin guardar.
*Aceptación:* la prueba crea `tmp/nota.txt` con el texto `Olá, SaguiOS! Ação e coração.`, lo guarda, lo cierra, lo vuelve a abrir desde Archivos y comprueba el contenido.

**H26. Calculadora.** Programa en ring 3 con botones pulsables y teclado: dígitos, coma decimal, suma, resta, multiplicación, división, igual, borrar y borrar el último dígito. Números en punto fijo (sección 4), con aviso de división por cero y de número demasiado grande.
*Aceptación:* la prueba calcula `12,5 × 4 = 50`, `7 ÷ 2 = 3,5`, y `1 ÷ 0` muestra un aviso; captura en `docs/capturas/`.

**H27. Sagui Atrapa Bananas.** Juego de autoría propia en ring 3: el sagüí se mueve por la parte de abajo de la ventana con las flechas, caen bananas desde arriba, cada banana atrapada vale un punto, cada banana que cae al suelo quita una vida (3 vidas) y la velocidad aumenta con la puntuación. Marcador, pantalla de inicio, pantalla de fin de partida y récord de la sesión. Todos los dibujos en pixel art hechos por la IA en el código. Si hay un pendrive montado (fase 6), el récord se guarda en `/usb/sagui-recorde.txt`.
*Aceptación:* una prueba automática juega unos segundos enviando flechas por el monitor de QEMU; capturas de la pantalla de juego y de la pantalla de fin de partida.

**H28. Acabado.** Iconos en el escritorio para Archivos, Terminal, Editor, Calculadora y Sagui Atrapa Bananas. Pantalla "Acerca de SaguiOS" con la versión, el tamaño de la imagen, el número de procesos activos y los créditos.
*Aceptación:* captura con tres programas abiertos a la vez; imagen medida y registrada (meta: hasta 1,44 MB).

> **PARADA OBLIGATORIA: fin de la fase 5.** Ver sección 8.

### Fase 6: Pendrive de datos (USB y FAT32)

**Objetivo:** guardar, abrir y editar archivos en un pendrive FAT32, con driver USB propio, y que el pendrive aparezca solo al conectarlo. Esta es la parte de drivers más difícil del proyecto. Ir con calma, probar cada capa antes de la siguiente y registrarlo todo en `docs/erros.md`.

**H29. PCI y controlador EHCI.** Recorrer el bus PCI y encontrar los controladores USB 2.0 (EHCI). Tomar el control del controlador que estaba usando la BIOS (procedimiento de traspaso de control de la especificación EHCI), reiniciar el controlador y activar los puertos. Recordar que el sistema ya está entero en la RAM, así que la BIOS ya no necesita el pendrive de arranque.
*Aceptación:* un comando de diagnóstico lista los controladores EHCI encontrados y los puertos con un dispositivo conectado, en QEMU.

**H30. Enumeración USB, hubs y conexión en caliente.** Transferencias de control, lectura de los descriptores, asignación de direcciones a los dispositivos. Soporte de hub: en el chipset del G470, cada controlador EHCI tiene un hub interno, y todos los dispositivos quedan detrás de él. Detectar cuándo se conecta o se extrae un pendrive con el sistema ya en marcha, consultando los cambios de estado de los puertos y de los hubs.
*Aceptación:* en QEMU, con un hub USB virtual entre el controlador y el pendrive, el sistema lista el fabricante, el producto y la clase del dispositivo; conectar y extraer el pendrive desde el monitor de QEMU genera los eventos de conexión y extracción, registrados por el puerto serie.

**H31. Almacenamiento masivo.** Protocolo Bulk-Only Transport con los comandos SCSI básicos: identificar, capacidad, leer y escribir bloques. Tratamiento de errores y de extracción del dispositivo.
*Aceptación:* una prueba lee y escribe sectores en una imagen de prueba en QEMU y los compara byte a byte con la VPS.

**H32. Particiones y FAT32: lectura.** Tabla de particiones MBR, lectura de FAT32 (nombres cortos y nombres largos), montaje en `/usb` a través de la capa de archivos virtual.
*Aceptación:* un pendrive de prueba creado en la VPS con carpetas, acentos y nombres largos se lista y se lee correctamente con `ls` y `cat`.

**H33. FAT32: escritura.** Crear, sobrescribir, ampliar y borrar archivos; crear y eliminar carpetas; actualizar las dos copias de la tabla FAT y la información de espacio libre; grabar todo lo pendiente en `desmontar`.
*Aceptación:* la prueba graba archivos y carpetas en QEMU; después de `desmontar`, la VPS comprueba con `mtools` y con `fsck.vfat` que el sistema de archivos está íntegro y con el contenido correcto.

**H34. Integración: el pendrive aparece en el sistema.**

Cuando el usuario conecta el pendrive con el sistema en marcha:

1. Aparece una notificación en la esquina de la pantalla: `Pendrive conectado: <nome> (<tamanho>)`.
2. Aparece un icono "Pendrive" en el escritorio y en el lateral de Archivos.
3. Al hacer clic en el icono se monta el pendrive en `/usb` y se abre Archivos en él. El Editor pasa a abrir y guardar archivos allí.
4. Clic con el botón derecho en el icono (o el botón "Quitar de forma segura" en Archivos) graba todo lo pendiente, desmonta y muestra `Pode remover o pendrive`.
5. Si se extrae el pendrive sin desmontarlo, el sistema avisa de que se pueden haber perdido datos y retira el icono.

Desde el terminal, el mismo flujo:

```
sagui:/$ discos
usb0  <fabricante> <produto>  16 GB  FAT32  "MEUPEN"  (não montado)
sagui:/$ montar usb0
Pendrive montado em /usb
sagui:/$ ls /usb
sagui:/$ cp /leia-me.txt /usb/
sagui:/$ desmontar usb0
Pode remover o pendrive.
```

Un pendrive que no sea FAT32 aparece en `discos` con el formato detectado y el mensaje `formato não suportado`, sin montarse.

*Aceptación:* guion completo en QEMU, por las dos vías (interfaz y terminal): conectar el pendrive con el sistema en marcha, montar, crear una nota en el Editor, guardarla en `/usb`, copiar un archivo desde el terminal, desmontar, extraer; la VPS comprueba el contenido con `mtools`. Capturas de cada paso.

> **PARADA OBLIGATORIA: fin de la fase 6 (final).** Ver sección 8.

### Extras (solo después del 100%, si hay tiempo y límite de uso)

**B1. Ver el disco duro interno, sin abrirlo ni escribir en él.** Driver de disco SATA de solo lectura, para que el comando `discos` muestre que el disco duro existe, su tamaño y las particiones con el tipo de cada una (por ejemplo, `ext3 (Linux): formato não suportado`). El sistema no monta, no abre y **nunca escribe** en el disco interno. Probar primero en la VM con un disco virtual. En el G470 el disco duro usa ext3 y Eli no quiere que se altere de ninguna manera.

**B2. Arranque por UEFI.** Segundo cargador para UEFI (vídeo mediante GOP), para funcionar en PC más modernos.

**B3. Pitido.** Sonido sencillo por el altavoz del PC (canal 2 del PIT): un pitido al atrapar una banana en Sagui Atrapa Bananas y otro al perder una vida, más un comando `bip`. En QEMU, probar con el altavoz emulado. En el G470 puede que no suene, según cómo conecte el portátil esa señal al altavoz; registrar el resultado.

---

## 7. Pruebas automáticas en QEMU

- Un script por historia en `tests/`, ejecutado con `make test`.
- El kernel escribe un log en el **puerto serie COM1**. Las pruebas leen ese log para comprobar los resultados.
- El script usa el **monitor de QEMU** para: enviar teclas (`sendkey`), mover el ratón (`mouse_move`, `mouse_button`) y capturar la pantalla (`screendump`, convertida a PNG con ImageMagick).
- **Mira las capturas de pantalla** para confirmar visualmente lo que pide el criterio. Guarda las buenas en `docs/capturas/HXX-descricao.png`.
- Toda prueba se ejecuta en los dos modos de arranque (IDE y USB) cuando la historia toca el arranque.
- Al final de cada fase, volver a ejecutar **todas** las pruebas anteriores, para garantizar que no se ha roto nada.
- Tiempo máximo por ejecución de prueba: 3 minutos. Si se cuelga, matar QEMU y registrarlo en `docs/erros.md`.

---

## 8. Pruebas en el hardware real (con Eli)

El G470 no tiene salida de depuración. Si se cuelga, la pantalla simplemente se queda en negro. Por eso:

1. **Mensajes de progreso** en cada paso del arranque (`[1/6] A20 ok`, `[2/6] memoria ok`...). Si se cuelga, Eli fotografía la pantalla y la foto muestra dónde se detuvo.
2. **Al final de cada fase**, el agente:
   1. Ejecuta todas las pruebas automáticas de todas las fases anteriores.
   2. Genera en `entrega/faseX/` la imagen `.img` (pendrive) y la `.vdi` (VirtualBox), con SHA-256.
   3. Actualiza `docs/como-gravar.md`.
   4. Escribe `docs/teste-faseX.md` con el **guion de prueba** de la fase, en dos pasos, en este orden:
      1. **Máquina virtual (VirtualBox, con el `.vdi`):** cómo crear o actualizar la VM, qué hacer y qué debe aparecer en pantalla.
      2. **G470 físico (pendrive con el `.img`):** cómo grabarlo, cómo arrancar, qué debe aparecer y qué fotografiar si algo sale mal.
      Eli prueba primero en la VM. Si hay un error, manda la captura y el agente lo corrige antes de la prueba física. Si pasa en la VM, lo prueba en el G470.
   5. Marca el dashboard como "Esperando la prueba de Eli: fase X".
   6. **Se detiene y escribe el mensaje siguiente**, en el terminal y al principio de `status.md`.
3. **Mensaje de fin de fase** (modelo):

```
Eli, terminei a fase X: <nome da fase>. Pode testar!

O que foi feito: <resumo em 3 a 5 linhas, em português simples>
Histórias concluídas: HXX a HYY
Imagem: entrega/faseX/saguios.vdi (VirtualBox, testar primeiro) e saguios.img (pendrive, G470)
Roteiro de teste: docs/teste-faseX.md
O que você deve ver no G470: <1 a 3 itens>
Problemas conhecidos: <lista, ou "nenhum">

Quando testar, me diga "fase X ok" ou descreva o que aconteceu.
```

4. **Después de la prueba:**
   - Si Eli dice "fase X ok", ejecutar `tools/confirmar-hardware.sh "fase X ok"`, que registra la confirmación en el diario y en el dashboard, y empezar la fase siguiente.
   - Si Eli informa de un problema, crear una historia de corrección (`HXX-fix`), registrarla en `docs/erros.md` con síntoma, causa y corrección, generar una nueva imagen de la fase y volver a enviar el mismo mensaje, indicando qué se ha corregido. Solo seguir después del "fase X ok".
5. **En la prueba de la fase 6**, Eli usa un pendrive de datos sin nada importante, formateado en FAT32.

---

## 9. Dashboard

- `dashboard/index.html` en HTML, CSS y JS puros, sin ninguna dependencia externa. Eli descarga la carpeta por FTP y la abre en local.
- Lee `dashboard/status.json` y muestra:
  - El título `SaguiOS: um sistema operacional escrito do zero por IA`.
  - Barra de progreso con el porcentaje general y por fase.
  - Lista de historias con su estado (por hacer, en curso, hecha, esperando prueba).
  - La tabla de la sección 1.1 (criterio de Tanenbaum), con cada elemento marcado a medida que se implementa.
  - Últimas acciones, errores y correcciones recientes (causa y corrección).
  - Galería con las capturas de pantalla más recientes.
  - Tamaño actual de la imagen comparado con la meta de 1,44 MB.
  - Tiempo dedicado a cada historia y número de líneas de código escritas (kernel, boot, userland).

---

## 10. Modo autónomo, límites y ahorro de tokens

- El agente se ejecuta con `/loop` en Claude Code, con una suscripción dedicada a este proyecto, dentro de una sesión tmux.
- Al alcanzar el límite de uso, **esperar a que se renueve y continuar** desde donde se quedó. Al retomar, leer primero `status.md`.
- **Ahorro de tokens:**
  - Este proyecto escribe mucho código, así que gasta más que el anterior. Planifica cada historia antes de escribir.
  - No leas archivos enteros sin necesidad; es mejor buscar fragmentos.
  - No te quedes vigilando procesos largos; ejecútalos, espera y lee solo el resultado.
  - Cuando un error se repita más de 3 veces con el mismo enfoque, detente, regístralo y cambia de estrategia.
- No borrar nunca trabajo validado. Antes de cambios grandes en el arranque, la paginación, el planificador o el driver USB, crear una etiqueta en git.

---

## 11. Entregables

1. `entrega/fase1/` a `entrega/fase6/`: para cada fase, la imagen `.img` (pendrive) y la `.vdi` (VirtualBox), con SHA-256.
2. `entrega/final/`: `saguios.img` y `saguios.vdi` de la versión final.
3. Todo el código fuente y los scripts de build en el repositorio, listos para publicarse.
4. `docs/` completo: historias, decisiones, diario, errores y capturas.
5. `docs/resumo-final.md`: qué se hizo, tiempo de cada fase, número de ciclos de corrección, líneas de código, tamaño final de la imagen, qué funcionó en el G470 y qué se quedó fuera, y la tabla de la sección 1.1 completada.

---

## 12. Definición de éxito

| Nivel | Qué ocurre en el G470 |
| --- | --- |
| Mínimo | Arranca con cargador propio y muestra el mensaje del kernel |
| Bueno | Terminal con teclado ABNT2 (y ES en esta versión) y navegación por los archivos |
| Muy bueno | Todos los conceptos de la tabla 1.1: programas ELF en ring 3, multitarea, protección de memoria, archivos escribibles y pipes |
| Óptimo | Modo gráfico con ratón |
| Excelente | Escritorio con ventanas y los programas: Archivos, Terminal, Editor, Calculadora y Sagui Atrapa Bananas |
| Completo | Guarda y abre archivos en un pendrive FAT32, que siguen ahí después de apagar |
| Extra | Ve el disco duro interno sin tocarlo, arranca por UEFI o hace pitidos en el juego |
