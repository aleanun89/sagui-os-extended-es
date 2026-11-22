<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 92; titulo: Anexo: ensamblador x86 -->
# Anexo: ensamblador x86 en SaguiOS

El ensamblador es el lenguaje más cercano a la máquina: cada instrucción es una operación que el procesador
ejecuta directamente. Este anexo explica lo que hace falta para entender cómo arranca SaguiOS, cómo pasa de un
modo de trabajo del procesador a otro y cómo se reparten el trabajo el kernel y los programas. Sigue el orden
histórico de la arquitectura x86: primero el modo real de 16 bits, después el modo protegido de 32 bits, después
una panorámica de los 64 bits. Los 16 y 32 bits se explican con detalle y con referencias a los ficheros reales del
sistema; los 64 bits, solo como conceptos.

Las referencias tienen la forma `fichero:línea`, con la ruta desde la raíz del repositorio. Las líneas se pueden
comprobar abriendo el fichero en el PC.

## 1. Qué es el ensamblador y por qué lo necesita un sistema operativo

Un programa en C o en Pascal se traduce a instrucciones de máquina con un compilador. El ensamblador hace lo mismo
con un paso casi uno a uno: cada línea es un **mnemónico** (`mov`, `cli`, `int`) con sus operandos, y se
convierte en uno o varios bytes. El ensamblador es particular de cada familia de procesadores. Las instrucciones de
x86 de 32 bits se ejecutan igual en un Pentium II que en un Athlon o en un i7 de 7.ª generación, que es la base de
que SaguiOS funcione en máquinas tan distintas.

Un sistema operativo necesita ensamblador (o, en la práctica, unas pocas líneas de ensamblador) porque hay cosas
que el lenguaje C no puede expresar:

| Qué | Por qué no basta C | Dónde aparece en SaguiOS |
|-----|--------------------|--------------------------|
| **Arranque** | Al encender, la CPU está en modo real y no hay pila ni C | `boot/mbr.asm`, `boot/estagio2.asm` |
| **Cambio de modo** | Hay que cargar tablas del procesador (GDT) y escribir en registros de control (CR0) | `boot/estagio2.asm:294-300` |
| **Interrupciones** | El procesador apila un marco concreto y hay que volver con `iret` | `kernel/arch/vetores.asm` |
| **Cambio de contexto** | Cambiar de pila y de tablas de página entre procesos | `kernel/arch/vetores.asm:51-53`, `kernel/mem/paginacao.c:490-492` |
| **Instrucciones privilegiadas** | Solo el kernel puede usar `cli`, `lgdt` o escribir en CR3 | `kernel/inc/cpu.h:81-82`, `kernel/arch/anel3_testes.asm:46` |

### Anillos de privilegio

El procesador x86 tiene cuatro niveles de privilegio, llamados **anillos** (0 a 3). SaguiOS usa solo dos: el
anillo 0 para el kernel y el anillo 3 para los programas de usuario. El nivel de privilegio actual (CPL) está en
el selector del segmento de código. En el anillo 3, las instrucciones que cambian el estado global de la máquina
(`cli`, `lgdt`, escribir en CR0 o CR3) generan una excepción de protección general (#GP), y el kernel termina el
programa que la provocó. Así un programa no puede apagar las interrupciones ni leer la memoria del kernel.

## 2. Modo real de 16 bits

Cuando el procesador se enciende, está en **modo real**, el modo del 8086 de 1978. Es el que usa la BIOS y el
que ve el primer sector del disco.

### Registros

- Cuatro registros generales de 16 bits que se pueden partir en dos de 8 bits: **AX** (AH y AL), **BX**, **CX** y
  **DX**.
- Dos registros de índice y dos de puntero: **SI**, **DI**, **BP** y **SP**. SP es la pila.
- El contador de programa **IP**, y los indicadores (**FLAGS**), con bits como CF (acarreo, que la BIOS usa para
  señalar errores) o IF (interrupciones permitidas).
- Cuatro **registros de segmento**: **CS** (código), **DS** (datos), **SS** (pila) y **ES** (datos extra).

### Direccionamiento segmento:desplazamiento

Un registro de 16 bits solo llega a 64 KB. Para alcanzar 1 MB, el procesador suma dos valores:

    dirección física = segmento × 16 + desplazamiento

Por ejemplo, `0x07C0:0x0005` vale `0x07C0 × 16 + 0x0005 = 0x7C05`. Muchas combinaciones de segmento y
desplazamiento señalan la misma dirección física, y de ahí viene que el código de arranque ponga CS a cero en
cuanto puede (ver `boot/mbr.asm:39`: el salto largo `jmp 0x0000:.cs_zerado` deja CS = 0 para que las direcciones
del `org 0x7C00` valgan siempre).

Con 20 líneas de dirección, las direcciones por encima de 1 MB se "envuelven" a cero en el 8086. Las máquinas
modernas tienen una puerta llamada **línea A20** que lo puede activar o desactivar; SaguiOS la activa en la etapa 2
(`boot/estagio2.asm`, sección `[1/6] A20`, línea 132, y el método por el puerto 0x92 en las líneas 413-418).

### La BIOS y sus interrupciones

Las BIOS ofrecen servicios de bajo nivel llamados con `int n`. Son el único "sistema operativo" disponible antes
del kernel. SaguiOS usa, entre otras:

| Interrupción | Función (AH) | Uso en SaguiOS |
|--------------|--------------|----------------|
| `int 0x10` | `0x0E`: escribir un carácter en modo teletipo | Mensajes del cargador (`boot/mbr.asm:112`) |
| `int 0x13` | `0x41`: ¿hay extensiones LBA? | Elegir cómo leer el disco (`boot/mbr.asm:52-55`) |
| `int 0x13` | `0x08`: geometría del disco | Lectura CHS si no hay LBA (`boot/mbr.asm:60-64`) |
| `int 0x13` | `0x42`: leer sectores por LBA | Leer la etapa 2 y el kernel (`boot/mbr.asm:150-152`) |
| `int 0x15` | `0xE820`: mapa de memoria | Cuánta RAM hay (`boot/estagio2.asm:508-512`) |

Las interrupciones de la BIOS se llaman en modo real y dejan los resultados en los registros (el acarreo CF
indica error). Cuando el sistema pasa a modo protegido, ya no hay BIOS: por eso el cargador hace todas las
llamadas antes del salto.

### El MBR: 512 bytes y la firma 0x55AA

Al arrancar desde un disco, la BIOS lee el **primer sector** (512 bytes) del disco a la dirección `0x7C00`,
comprueba que sus dos últimos bytes son `0x55 0xAA` y salta a ella. Ese sector es el **MBR** (*Master Boot
Record*). Como caben solo 512 bytes, el MBR solo puede hacer una cosa: cargar la etapa siguiente, que no tiene ese
límite.

El fichero `boot/mbr.asm` lo describe en su cabecera (líneas 10-17):

- `0x000-0x002`: salto corto y NOP, el formato que esperan algunas BIOS.
- `0x003-0x059`: área del BPB (la geometría de disco de un FAT) puesta a cero. Algunas BIOS escriben aquí la
  geometría al arrancar desde USB, por eso no hay código en esa zona.
- `0x05A-0x1B7`: código y mensajes.
- `0x1BE-0x1FD`: tabla de particiones, rellenada por `tools/mkimagem.py`.
- `0x1FE-0x1FF`: la firma. En el fichero es `dw 0xAA55` (línea 191), y como el x86 es little-endian, en disco
  aparecen los bytes `55 AA`.

Los pasos del MBR de SaguiOS, en orden:

1. **Estado conocido** (`boot/mbr.asm:31-36`): desactivar interrupciones, poner DS, ES y SS a cero y la pila
   en `0x7C00`, que crece hacia abajo, en la memoria baja libre.

        cli
        xor ax, ax
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0x7C00

2. **Recordar el disco**: la BIOS deja en DL el número del disco de arranque; el MBR lo guarda en `disco`
   (`boot/mbr.asm:43`) porque lo necesitarán todas las lecturas y la etapa 2.

3. **Elegir el modo de lectura**: se pregunta a la BIOS si entiende las extensiones LBA (`AH=0x41`, línea 55). Si
   no, se pide la geometría (`AH=0x08`, líneas 60-64) y se leerá por cilindro, cabeza y sector.

4. **Leer la cabecera de arranque** del sector LBA 1 y comprobar su firma (líneas 76-79). La cabecera dice en qué
   sector empieza la etapa 2 y cuántos sectores ocupa.

5. **Leer la etapa 2 a `0x8000`** (líneas 82-91) y saltar a ella con el disco en DL (líneas 93-94):

        mov dl, [disco]                 ; la etapa 2 recibe el disco en DL, como nosotros
        jmp 0x0000:ESTAGIO2_ENDERECO

Si algo falla, el MBR escribe un mensaje con la BIOS y se queda en un `hlt` (la etiqueta `parar`, en
`boot/mbr.asm:165-169`). La rutina que escribe ese texto (`escrever`, `boot/mbr.asm:106-115`) recorre la cadena
byte a byte con `lodsb` y llama a `int 0x10` por cada carácter:

    escrever:
            lodsb                       ; AL = [DS:SI], SI se incrementa
            test al, al
            jz .fim
            mov ah, 0x0E
            mov bx, 0x0007
            int 0x10
            jmp escrever

### La etapa 2 en modo real

`boot/estagio2.asm` no tiene límite de 512 bytes, y hace en modo real lo que la BIOS todavía permite: comprueba la
CPU (`conferir_cpu`, que exige CPUID y CMOV), activa A20, lee el mapa de memoria con `int 0x15`, carga el kernel
y el ramdisk por encima de 1 MB y elige el modo de vídeo VESA con `int 0x10`. Para copiar por encima de 1 MB sin
salir del modo real, usa el truco del **modo irreal** (`entrar_unreal`, línea 1161): entra en modo protegido solo
un instante para cargar en DS y ES un descriptor de 4 GB, y vuelve a modo real. El límite del descriptor queda en
la caché del procesador y las direcciones de 32 bits siguen funcionando.

## 3. Paso al modo protegido de 32 bits

El **modo protegido** es el modo de trabajo de SaguiOS. Cambian varias cosas a la vez:

- Los registros pasan a tener 32 bits. Los generales son **EAX, EBX, ECX, EDX, ESI, EDI, EBP** y **ESP**
  (la pila). El contador de programa es **EIP** y los indicadores son **EFLAGS** (IF sigue en el bit 9).
- La dirección de un segmento deja de ser `segmento × 16`. Ahora el selector es un índice en una tabla de
  **descriptores**, y cada descriptor dice base, límite, tipo y **nivel de privilegio** (DPL).
- Se pueden usar **interrupciones** protegidas (una tabla IDT con puertas que dicen a qué anillo se puede llamar) y
  **paginación** (la memoria virtual).

### La GDT

La **tabla global de descriptores** (GDT) es una lista de descriptores de 8 bytes. Un descriptor de código o datos
tiene:

- la **base** (32 bits) y el **límite** (20 bits, en bytes o en páginas de 4 KB);
- el **byte de acceso**: bit de presente, DPL (dos bits, anillo 0 a 3), tipo (código o datos, lectura o escritura);
- los **bits de granularidad**: el 4 KB y el modo de 32 bits.

El cargador define una GDT mínima con un descriptor nulo (obligatorio), código y datos de 32 bits con base 0 y
límite de 4 GB (`boot/estagio2.asm:1787-1795`):

    gdt:
            dq 0                            ; descriptor nulo (obligatorio)
            dw 0xFFFF, 0x0000               ; 0x08 código: límite 0-15, base 0-15
            db 0x00, 0x9A, 0xCF, 0x00       ;      base 16-23, presente/anillo 0/código, 4 KB + 32 bits, base 24-31
            dw 0xFFFF, 0x0000               ; 0x10 datos
            db 0x00, 0x92, 0xCF, 0x00

Los selectores son desplazamientos dentro de la tabla: `0x08` es el código de anillo 0 y `0x10` los datos. Con
base 0 y límite de 4 GB, el **modo plano** equivale a no tener segmentación: todas las direcciones son lineales.
Así lo hace el kernel, y por eso el C no necesita pensar en segmentos.

### El salto al modo protegido

El paso se hace con seis instrucciones, en `boot/estagio2.asm:294-300`:

    .entrar_pm:
            cli                             ; ninguna interrupción de la BIOS durante el cambio
            lgdt [gdt_descritor]            ; carga la GDT (dirección y tamaño)
            mov eax, cr0
            or eax, 1                       ; bit 0 = PE, protección activada
            mov cr0, eax
            jmp SEL_CODIGO:modo_protegido   ; salto largo: recarga CS con el selector de 32 bits

Hay que fijarse en dos cosas:

- **El salto largo es obligatorio.** Al poner PE, el procesador sigue con el CS del modo real en la caché. Un salto
  lejano con el selector `0x08` recarga CS con el descriptor de código de 32 bits y vacía la cola de instrucciones
  que el procesador ya había leído en modo real.
- **Los demás registros de segmento se recargan después.** `modo_protegido` (`boot/estagio2.asm:1735`) carga DS,
  ES, FS, GS y SS con `0x10` y pone la pila provisional en `0x7C00` (líneas 1736-1742). Hasta esa carga, DS y SS
  siguen con valores de modo real.

### La línea A20 y el mapa de memoria

La línea A20 no es un detalle del modo protegido: sin ella, las direcciones de 1 MB en adelante se envuelven. El
cargador la activa antes del salto (sección `[1/6]`, línea 132), y si no se consigue, se queda parado con un mensaje
(`msg_falha_a20`). El mapa de memoria del E820 dice al kernel qué rangos son RAM utilizable y cuáles están
reservados para la BIOS o el hardware.

### Del cargador al kernel

`kernel/arch/entrada.asm` es la primera instrucción del kernel. Recibe en EAX la firma del cargador y en EBX la
dirección de `boot_info`, y antes de cualquier C hace tres cosas:

1. Pone a cero la sección BSS, porque el binario plano no la contiene y el C supone variables a cero (líneas 17-24:
   `rep stosb`).
2. Cambia a una pila propia, dentro de la BSS (línea 25).
3. Llama a `inicio_kernel` con los dos valores como argumentos (líneas 27-29).

### La convención de llamada cdecl

En modo protegido de 32 bits, SaguiOS (como la mayoría de C de la época) usa la convención **cdecl**:

- Los argumentos se meten en la pila de derecha a izquierda. Por eso `push edx; push esi; call inicio_kernel`
  (`kernel/arch/entrada.asm:27-29`) deja `boot_info` y la firma como segundo y primer argumento.
- El valor de retorno va en **EAX** (y en EDX:EAX si son 64 bits).
- Quien llama **limpia la pila** después de la llamada (`add esp, 4` en `kernel/arch/vetores.asm:60`).
- EBX, ESI, EDI, EBP y ESP los conserva la función llamada; EAX, ECX y EDX pueden cambiar.

El marco de pila de una función típica queda así (texto ilustrativo, no es código que se ensamble):

```text
  [ebp+12]   segundo argumento
  [ebp+8]    primer argumento
  [ebp+4]    dirección de retorno (la mete el "call")
  [ebp]      EBP anterior (la función lo guarda con "push ebp")
  [ebp-4]    primera variable local
```

El compilador de C genera esto solo. Lo que importa para leer el ensamblador de SaguiOS es que el marco de cada
interrupción tiene un formato fijo que el C conoce (ver la sección siguiente).

### Interrupciones y excepciones: la IDT

La **tabla de descriptores de interrupción** (IDT) tiene 256 entradas, una por vector. Cada entrada dice la
dirección del manejador, el selector de código y el **DPL**, que decide si el programa de anillo 3 puede llamarla
con `int n`:

- `kernel/arch/idt.c:70` pone todas las entradas con `0x8E`: puerta de interrupción de 32 bits, DPL 0. Un programa
  de usuario que ejecute `int 0x20`, por ejemplo, recibe #GP.
- `kernel/arch/idt.c:72` pone la llamada al sistema, `VETOR_SISTEMA` (`kernel/inc/cpu.h:27`, valor `0x80`), con
  `0xEE`: DPL 3, para que el anillo 3 pueda usarla.

El procesador, al recibir una interrupción, apila el marco que el código de entrada debe respetar, y si hubo
cambio de anillo también ESP y SS del usuario. El C lo ve como una estructura (`quadro_t`, en
`kernel/inc/cpu.h:36-42`). Como no todas las excepciones apilan un código de error, cada vector tiene su trozo
de entrada que hace que el marco siempre tenga el mismo tamaño:

- `kernel/arch/vetores.asm:27-36` es un bucle `%rep` de 256 vueltas que apila un 0 si el procesador no apiló código
  de error (la condición `%if`, líneas 30-32), apila el número del vector y salta al trozo común.
- `entrada_comum` (líneas 38-59) guarda todos los registros con `pushad`, carga los segmentos del kernel y llama a
  `tratar_interrupcao` (`kernel/arch/idt.c:297`).
- `retomar_quadro` (líneas 63-70) restaura los registros y termina con `iretd`, que desapila EIP, CS y EFLAGS, y
  si el marco es de otro anillo, también ESP y SS.

### Llamadas al sistema con int 0x80

Un programa de usuario pide servicios al kernel con `int 0x80`. El número de llamada va en EAX y los argumentos
en EBX, ECX, EDX (y ESI, EDI); el comentario de `kernel/inc/abi.h:5` lo explica. Un ejemplo de SaguiOS
(`kernel/arch/anel3_testes.asm:19-27`) escribe un texto con la llamada de escritura:

    mov eax, SIS_ESCREVER               ; número de la llamada
    mov ebx, 1                          ; descriptor 1 (la salida)
    lea ecx, [esi + (.msg - .aqui)]     ; dirección del texto
    mov edx, .msg_fim - .msg            ; longitud
    int 0x80                            ; el kernel toma el control

El mismo fichero prueba la protección (líneas 42-50): `cli` en el anillo 3 hace que el kernel termine el proceso,
y un intento de leer `0x00100000` (memoria del kernel, `anel3_testes.asm:55-59`) provoca un fallo de página, porque
las páginas del kernel no tienen el bit de usuario.

### Anillo 3: cómo arranca un programa

`processo_preparar_usuario` (`kernel/nucleo/processos.c:714-728`) prepara el marco de un programa nuevo como si
viniera de una interrupción de anillo 3. Pone los selectores de usuario (`SEL_UCODIGO` 0x1B y `SEL_UDADOS` 0x23,
`kernel/inc/cpu.h:11-12`), la pila del programa y el punto de entrada. El EFLAGS es `0x202`: IF a 1 e IOPL a 0, de
modo que el programa no puede usar `cli`, `sti` ni los puertos de E/S. Después, `retomar_quadro` hace el `iretd` y
la CPU pasa al anillo 3 sin ninguna instrucción especial de "bajar de privilegio".

### Paginación: CR3, CR0 y CR4

La paginación da a cada proceso su propio espacio de direcciones. La CPU la controla con tres registros:

- **CR3** apunta a la tabla de directorio (las tablas de páginas). Cambiar CR3 cambia de espacio de direcciones.
  Es lo que hace `espaco_ativar` (`kernel/mem/paginacao.c:490-492`): `mov %0, %%cr3`.
- **CR0.PG** (bit 31) enciende la paginación. El cargador de SaguiOS la enciende en los procesadores secundarios
  (`kernel/arch/ap_trampolim.asm:145`, `mov cr0, eax`, con la tabla ya en CR3 de la línea 136).
- **CR4** activa extensiones: PSE (páginas de 4 MB, que usa el mapa del kernel), PAE (entradas de 64 bits) y PGE
  (páginas globales). `ap_trampolim.asm:97` copia el CR4 del procesador principal antes de encender la paginación,
  porque el orden importa.

El cambio de contexto en el kernel reúne todo esto: cuando `tratar_interrupcao` devuelve el marco de otro proceso,
`entrada_comum` carga su pila (`mov esp, eax`, `vetores.asm:53`). Al cambiar de proceso, el núcleo llama a
`espaco_ativar` con el espacio del proceso nuevo (`kernel/nucleo/processos.c:603`), es decir, carga CR3. Así una
interrupción puede terminar en otro programa con otra memoria.

### Los procesadores secundarios: de 16 a 32 bits otra vez

El modo SMP (opcional, mkimagem.py `--smp`) enciende las CPU secundarias con una secuencia INIT-SIPI. Cada
secundario arranca en modo real en una página que el principal ha copiado a `0x7000`, y el trampolín repite el
camino del cargador: GDT local, CR0.PE, salto largo al código de 32 bits y, luego, la paginación. Es un buen
ejemplo de que el paso de modo se repite en cada CPU:

    protegido:                          ; kernel/arch/ap_trampolim.asm:80, ya en modo de 32 bits
            mov ax, 0x10                ; (extracto: se omiten los comentarios de las líneas 81-83)
            mov ds, ax
            mov es, ax
            mov fs, ax
            mov gs, ax
            mov ss, ax

La cabecera del fichero dice que el trampolín hace lo mínimo para llegar a C: la GDT, la TSS y la IDT de cada CPU
se preparan después en C, con la paginación ya activa (`kernel/arch/ap_trampolim.asm:16-21`).

### El cargador UEFI: de 64 bits a 32

El cargador UEFI de x64 (`boot/uefi/trampolim.asm`) hace el camino contrario: el firmware deja la CPU en modo
largo, y el trampolín baja a 32 bits para el kernel. Tiene dos entradas, `entrada64` (línea 48) y `entrada32`
(línea 73). En la de 64 bits, el código carga una GDT propia, hace un retorno lejano (`o64 retf`, línea 57) para
pasar al segmento de compatibilidad, apaga la paginación (CR0, línea 65) y borra los bits LME y NXE de EFER
(línea 68). El 64 bits se explica en la sección 6.

## 4. Ensamblador en línea en C

Muchas partes del kernel no pueden escribirse en C, pero hay instrucciones sueltas que sí conviene usar desde el C:
leer un puerto, consultar CPUID, cambiar las interrupciones. El compilador permite escribir ensamblador **en
línea** con `__asm__ volatile("...")`.

### Dos sintaxis

El ensamblador de SaguiOS se escribe de dos formas, según el compilador:

| | Intel (NASM, este anexo para los ejercicios) | AT&T (GCC y TCC en línea) |
|--|---------------------------------------------|---------------------------|
| Orden | destino, origen: `mov eax, ebx` | origen, destino: `movl %ebx, %eax` |
| Registros | sin `%`: `eax` | con `%`: `%eax` |
| Constantes | sin signo de dólar: `mov eax, 5` | con `$`: `movl $5, %eax` |
| Tamaño | implícito o `dword` | sufijo: `movl`, `movw`, `movb` |

El C de SaguiOS (kernel y programas) lo compila GCC en el PC, y el kernel también contiene ficheros `.asm` en NASM.
Por eso en el mismo código hay dos sintaxis: las `__asm__` van en AT&T y los `.asm` en Intel.

### Restricciones de los operandos

En `__asm__` los operandos de C se conectan a registros con restricciones. Un ejemplo de `kernel/inc/portas.h`:

    static inline uint8_t porta_le8(uint16_t porta)
    {
        uint8_t v;
        __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(porta));
        return v;
    }

Quiere decir: la salida `v` va en AL (`"=a"`); la entrada `porta` va en DX o es una constante de 8 bits (`"Nd"`).
Así `inb` lee el puerto dado y deja el valor en `v`. `porta_escreve8` (líneas 7-9) hace lo mismo con `outb`.
`volatile` impide que el compilador quite o mueva la instrucción; si además la instrucción toca memoria, se añade
`"memory"` para que el compilador no reordene los accesos.

### Ejemplos de SaguiOS

| Instrucción | Qué hace | Dónde se usa |
|-------------|----------|--------------|
| `cpuid` | Consulta las capacidades de la CPU (hoja en EAX) | `kernel/arch/msr.c:105-107`, `kernel/dispositivos/sensores.c:74`, `userland/programas/simd.c:26` |
| `rdtsc` | Lee el contador de ciclos (TSC) en EDX:EAX | `kernel/dispositivos/ahci.c:167`, `kernel/dispositivos/sensores.c:80` |
| `in` / `out` | Leer y escribir un puerto de E/S | `kernel/inc/portas.h:7-15` (`outb`, `inb`) |
| `cli` / `sti` | Apagar y encender las interrupciones | `kernel/inc/cpu.h:81-82` (`int_liga`, `int_desliga`) |
| `cli; hlt` | Parar la CPU para siempre | `kernel/arch/excecoes.c:34` (tras un fallo fatal) |
| `sti; nop; nop; cli` | Dejar entrar una interrupción en una ventana concreta | `kernel/dispositivos/ata.c:594` |
| `rep insw` | Leer un bloque de palabras de un puerto | `kernel/dispositivos/ata.c:317` |
| `wbinvd` | Escribir y vaciar la caché de la CPU | `kernel/dispositivos/acel_radeon.c:825` |

Dos ejemplos merecen atención. `cpuid` tiene muchas hojas, y el resultado va en cuatro registros: `sensores.c:72-74`
la envuelve en una función con cuatro salidas. `ata.c:594` muestra un truco: la instrucción que sigue a `sti` se
ejecuta antes de que llegue una interrupción, así que el `nop` deja una ventana para que la interrupción entre
antes del siguiente `cli`.

## 5. SIMD: MMX, SSE y AVX en concepto

**SIMD** (*Single Instruction, Multiple Data*) quiere decir que una instrucción opera sobre varios datos a la vez,
empaquetados en un registro grande. Ejemplos:

- **MMX** (años noventa): registros de 64 bits (MM0 a MM7) que comparten el espacio de la FPU x87. Sumar cuatro
  enteros de 16 bits en una instrucción.
- **SSE** (Pentium III, 1999): registros de 128 bits (XMM0 a XMM7 en 32 bits, XMM0 a XMM15 en 64 bits). Operaciones
  en coma flotante de 4 números de 32 bits a la vez. **SSE2** (Pentium 4, 2001) añade enteros de 128 bits y números
  de doble precisión.
- **AVX** (Sandy Bridge, 2011): registros de 256 bits (YMM), con un prefijo de codificación nuevo (VEX). Es el
  que usa el estado de la FPU de SaguiOS cuando la CPU lo tiene (ver más abajo).

Para usarlas, el programa primero pregunta a la CPU con `cpuid`. `userland/programas/simd.c` define los bits
que mira: MMX en el bit 23 de EDX de la hoja 1 (`R_MMX`, línea 19), FXSR en el 24 (`R_FXSR`, línea 20), SSE en el 25
y SSE2 en el 26 (líneas 21-22). El mismo programa prueba cada conjunto con una instrucción suya, en ensamblador en línea:

- x87 con `fsqrt` (línea 42);
- MMX con `paddw` y `emms` (línea 52), que vacía el estado MMX;
- SSE con `movaps` y `mulps` (línea 64);
- la configuración de SSE con `ldmxcsr` (línea 138).

### Dónde usa SaguiOS SIMD

- **Copias y rellenos grandes en la interfaz gráfica**: `kernel/lib/sse2.c` (copia de 16 bytes por instrucción;
  el comentario de la cabecera explica por qué se alinea el destino). El ejemplo del acelerador es
  `kernel/dispositivos/gpu3d_r600.c:1193`, que lee la ventana de la tarjeta de 16 en 16 bytes: esa ventana es memoria sin caché
  (`kernel/dispositivos/gpu3d_evergreen.c:775`) y cada lectura es lenta. La función `sse2_copiar` está declarada en `kernel/inc/sse2.h:13`.
- **El estado de la FPU y de SSE por proceso**: cada proceso tiene sus registros x87, MMX y XMM, y el kernel no
  los guarda en cada cambio de proceso porque costaría caro. En su lugar, el cambio es **perezoso** (`kernel/arch/fpu.c:9-20`):
  el planificador activa CR0.TS y la primera instrucción de punto flotante del proceso nuevo provoca la excepción
  7 (#NM), y el manejador guarda el estado del dueño anterior y carga el del nuevo.
- **Activar SSE**: `kernel/arch/fpu.c:295` pone CR4.OSFXSR, sin el que el procesador da #UD con cualquier
  instrucción SSE, y `fpu.c:296` añade CR4.OSXMMEXCPT (errores SSE a la excepción 19). El guardado del estado usa `fxsave`
  (`kernel/arch/fpu.c:107`) o `xsave` si hay AVX (D381: 832 bytes, los YMM enteros, `kernel/arch/fpu.c:28-33`).
- **Reglas del kernel**: el kernel se compila con `-mgeneral-regs-only` (`kernel/arch/fpu.c:18-19`), así que el
  C normal no usa XMM; `sse2.c` sí, y por eso solo se llama entre `fpu_kernel_inicio()` y `fpu_kernel_fim()`, que
  guardan y restauran el estado del proceso dueño (`kernel/lib/sse2.c` lo cuenta en su cabecera, líneas 5-10).

Por eso el estado de la FPU y de SSE pertenece a cada proceso: si el kernel no lo guardara, un programa vería los
registros de otro.

## 6. 64 bits (x86-64): solo conceptos

La versión 10 de SaguiOS será de 64 bits (mejora 106, `mejoras/106-saguios-64-bits.md`). Hoy el kernel y los
programas son de 32 bits, y `docs/arquitecturas.md` lo resume: el x86 de 32 bits se mantiene después de la v10
para los PC sin 64 bits, y la v10 añade el x86-64. Esta sección explica lo que cambia, sin entrar en el detalle de
cada instrucción.

### Registros y modo largo

- Los registros generales pasan a tener **64 bits** (RAX a RSP, RBP, RSI, RDI, y RIP), y se añaden **R8 a R15**:
  16 registros generales en lugar de 8. Un prefijo de instrucción (REX) indica el tamaño de 64 bits.
- El procesador está en el **modo largo** cuando la paginación está activa y el bit LME del registro EFER (MSR
  `0xC0000080`, bit 8) está encendido. El trampolín UEFI de la sección 3 hace lo contrario: `entrada64`
  (`boot/uefi/trampolim.asm:48`) apaga la paginación (línea 65) y borra LME y NXE (línea 68) para volver al modo de
  32 bits.
- En modo largo, la segmentación casi no existe: los segmentos de código y datos se tratan como planos y los límites
  no se comprueban. El descriptor de código lleva un bit L que dice que es de 64 bits.

### Paginación de cuatro niveles

Con 64 bits, la dirección virtual tiene 48 bits útiles y la paginación usa cuatro niveles de tablas: **PML4**,
**PDPT**, **PD** y **PT**. Cada tabla tiene 512 entradas de 8 bytes; las cuatro son una cadena de índices que sale
de los bits 39-47, 30-38, 21-29 y 12-20 de la dirección, y el desplazamiento de página son los 12 bits bajos
(páginas de 4 KB). CR3 apunta a la PML4. Hay páginas de 2 MB (se para en el nivel PD) y de 1 GB (en PDPT). Intel
añade un quinto nivel (LA57) para 57 bits, que aquí no hace falta.

### Convención de llamada System V

En Linux y en casi todos los sistemas de 64 bits, la convención es **System V AMD64**: los primeros argumentos van
en registros (RDI, RSI, RDX, RCX, R8, R9), el valor de retorno en RAX y la pila debe estar alineada a 16 bytes en
la llamada. Esto cambia el marco de la sección 3: ya no hay argumentos en la pila (salvo los que no caben en los
registros), y las variables pueden vivir debajo de RSP en una zona de 128 bytes llamada *red zone*, sin mover RSP.

### Llamadas al sistema: syscall y sysret

En 32 bits, SaguiOS entra al kernel con `int 0x80`, que pasa por la IDT, apila el marco y comprueba permisos. En
64 bits existen las instrucciones **syscall** y **sysret**, que cambian de anillo sin pasar por la IDT: la dirección
de entrada está en un MSR (`LSTAR`), y los registros de la llamada van en RAX (número), RDI, RSI, RDX, R10, R8 y
R9. Es más rápido porque no pasa por la IDT. Lo de SaguiOS (`int 0x80`,
`kernel/inc/abi.h:5`) seguirá funcionando en la versión de 32 bits.

### Qué cambiaría en SaguiOS

Según `docs/arquitecturas.md` (filas de x86-64), la v10 pide separar el código por arquitectura en
`kernel/arch/<arquitectura>/` (arranque, cambio de contexto, paginación, interrupciones, llamadas al sistema) y
compiladores propios con otro destino. Con lo visto arriba, los cambios más visibles son:

- la IDT tiene entradas de 16 bytes, no de 8, y el retorno de una interrupción es `iretq`;
- el código de arranque de los secundarios y el de `entrada.asm` pasan por el modo largo, con la tabla de cuatro
  niveles, y el marco de `vetores.asm` cambia de tamaño (RSP y SS siempre se apilan);
- los punteros de los drivers pasan a ser de 64 bits, y el DMA y los BAR de 64 bits de las GPU modernas, que es
  el motivo de la mejora 106;
- el compilador tiene que generar código de 64 bits (la v10 es un objetivo nuevo, no una opción).

## 7. Ejercicios

### Ejercicio 1: un sector de arranque en NASM (en el PC)

**Objetivo:** escribir un MBR de 512 bytes que muestre un texto con `int 0x10` y se pare, y comprobar que el
ensamblador le pone la firma. Se ensambla en el PC con NASM (`sudo apt install nasm`, según `docs/CONTINUAR.md`).

Guarda este código como `sector.asm`:

<!-- nasm: sector.asm -->
```nasm
; sector.asm - sector de arranque de 512 bytes que escribe un texto con la BIOS.
bits 16
org 0x7C00

inicio:
        cli
        xor ax, ax
        mov ds, ax
        mov ss, ax
        mov sp, 0x7C00
        sti
        mov si, mensaje
.bucle:
        lodsb                           ; AL = [DS:SI], y SI avanza
        test al, al
        jz .fin
        mov ah, 0x0E                    ; teletipo de la BIOS
        mov bx, 0x0007
        int 0x10
        jmp .bucle
.fin:
.parar:
        hlt
        jmp .parar

mensaje: db "Hola desde NASM", 13, 10, 0

        times 510 - ($ - $$) db 0       ; relleno hasta el byte 510
        dw 0xAA55                       ; firma del MBR (bytes 55 AA en el disco)
```

Ensámblalo y comprueba el resultado:

    nasm -f bin sector.asm -o sector.bin
    ls -l sector.bin
    od -A d -t x1 sector.bin | tail -2

El tamaño debe ser 512 bytes y los dos últimos, `55 aa`.

**Solución (qué hace cada parte):**

- `bits 16` y `org 0x7C00` dicen a NASM que genere código de modo real y que las etiquetas empiecen en la dirección
  donde la BIOS carga el sector.
- Los primeros pasos son los del MBR de SaguiOS: interrupciones apagadas, segmentos a cero, pila en `0x7C00`, y se
  vuelven a encender con `sti`, como hace el MBR de SaguiOS (`boot/mbr.asm:41`).
- `lodsb` lee el byte de `[DS:SI]` y avanza SI. Como el texto acaba en 0, `test al, al` y `jz` salen del bucle.
- `int 0x10` con AH = `0x0E` escribe el carácter en AL. BL es el color, que solo importa en modos gráficos.
- `times 510 - ($ - $$) db 0` rellena hasta el byte 510, y `dw 0xAA55` escribe la firma en los dos últimos bytes.
  Si el código pasara de 510 bytes, NASM daría un error de «times» negativo: es la forma de comprobar el límite.

Para verlo funcionar en el PC, se puede usar QEMU: `qemu-system-i386 -drive format=raw,file=sector.bin`.

### Ejercicio 2: CPUID dentro de SaguiOS con TCC

**Objetivo:** leer el fabricante de la CPU desde un programa de SaguiOS.

SaguiOS no trae NASM, pero **TCC** (`/apps/bin/tcc`) acepta `__asm__` en línea con sintaxis AT&T, y también puede
ensamblar ficheros `.s` (los ensambla el propio TCC; `userland/terceros/tcc/fuentes/libtcc.c` lo trata en las
líneas 1204-1206). Guarda este código como `cpuid.c` dentro de SaguiOS (también está hecho en `/ejemplos/asm/cpuid.c`) y compílalo o ejecútalo con `tcc -run cpuid.c`:

<!-- tcc: cpuid.c -->
```c
#include <stdio.h>

/* Lee CPUID con la hoja 0: EAX = hoja máxima, EBX/EDX/ECX = el fabricante. */
int main(void)
{
    unsigned int a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0), "c"(0));
    printf("hoja maxima: %u\n", a);
    printf("ebx=%08x edx=%08x ecx=%08x\n", b, d, c);
    return 0;
}
```

**Solución:** `cpuid` con EAX = 0 devuelve en EAX la hoja más alta soportada, y el nombre del fabricante está en
EBX, EDX y ECX en ese orden. Por eso se imprimen en ese orden. Las restricciones `"=a"`, `"=b"`... unen cada
registro a una variable de C, y `"a"(0), "c"(0)` mandan los valores de entrada.

**Estado:** probado en QEMU con TCC (`tests/curso_ejemplos.py`): con `tcc -run` y compilado a un ejecutable escribe la hoja máxima y los tres
registros. En el QEMU sin KVM sale `GenuineIntel` (EBX = `756e6547`, EDX = `49656e69`, ECX = `6c65746e`); con KVM, el fabricante del anfitrión
(en AMD, `AuthenticAMD`).

### Preguntas de repaso

1. **¿Qué dirección física es `0x1234:0x0010`?**
   Solución: `0x1234 × 16 + 0x0010 = 0x12340 + 0x10 = 0x12350`.

2. **¿Por qué el MBR de SaguiOS no ejecuta el cargador completo, sino que lee una etapa 2?**
   Solución: el sector tiene 512 bytes y la BIOS solo carga ese. El código del MBR tiene que caber en el hueco entre
   la BPB y la tabla de particiones (`boot/mbr.asm:10-17`), así que solo sabe leer la cabecera y la etapa 2, y
   saltar a ella.

3. **¿Por qué el paso a modo protegido necesita un salto lejano después de poner PE?**
   Solución: al encender PE, la CPU sigue ejecutando con el CS del modo real en su caché y las instrucciones ya
   leídas. El salto lejano recarga CS con el selector del código de 32 bits y vacía la cola de instrucciones.

4. **¿Qué pasa si un programa de anillo 3 ejecuta `cli`?**
   Solución: la CPU genera #GP porque en el anillo 3 `cli` no está permitido. El kernel termina solo ese proceso
   (`kernel/arch/anel3_testes.asm:42-43` lo describe).

5. **¿Por qué `int 0x80` tiene DPL 3 (`0xEE`) y `int 0x20` no?**
   Solución: el DPL de la puerta dice qué anillo puede llamarla con `int`. Las llamadas al sistema tienen que
   poder hacerlas los programas, y las IRQ las dispara el hardware (`kernel/arch/idt.c:66-72`).

6. **¿Quién limpia la pila en la convención cdecl?**
   Solución: quien llama, con un `add esp, N` tras el `call` (`kernel/arch/vetores.asm:60` es un ejemplo).

7. **¿Por qué el estado de la FPU se guarda de forma perezosa?**
   Solución: guardar y restaurar 512 bytes en cada cambio de proceso cuesta caro, y casi ningún proceso usa la
   FPU. Con CR0.TS, el primer uso provoca #NM y solo entonces se hace el cambio (`kernel/arch/fpu.c:12-16`).

8. **¿Cuántos registros generales de 64 bits tiene x86-64 y cómo se llaman?**
   Solución: 16, RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP y R8 a R15.

## 8. Ejemplos del curso relacionados

`rootfs/ejemplos/mico/parpadeo.asm` es un programa de ejemplo que se escribe en **ensamblador**, pero **no es x86**.
Es el ensamblador de la máquina virtual CHIP-8, que se ensambla con `chip8asm` (lo dice el comentario de sus
primeras líneas). Sirve de contraste: el mismo concepto (mnemónicos, etiquetas, saltos), en otra máquina. El editor Mico
colorea ese lenguaje; la máquina está explicada en [la guía de CHIP-8](../chip8.md), y el editor en
[la guía de Mico](../mico.md).

Para la compilación de C dentro de SaguiOS, véase [TCC](../tcc.md) y [C en SaguiOS](../lenguajes/c.md).

## 9. Bibliografía

- A. S. Tanenbaum y H. Bos, *Sistemas operativos modernos*. Libro general de referencia para el arranque, las
  interrupciones y la gestión de la memoria, con un enfoque más teórico que el de este anexo.
- Intel, *Intel 64 and IA-32 Architectures Software Developer's Manual*, volúmenes 1 a 3: la referencia de las
  instrucciones, de la GDT, de la IDT, de la paginación y de los registros de control.
- AMD, *AMD64 Architecture Programmer's Manual*: la misma información para x86-64, con el modo largo y la
  convención de llamada.
- Documentación de las extensiones del INT 13h (LBA, EDD) y del servicio E820 del INT 15h, que el cargador de
  SaguiOS usa tal como se describe arriba.

Los manuales de Intel y AMD son públicos. Este anexo no copia texto, figuras ni ejercicios de ningún libro.
