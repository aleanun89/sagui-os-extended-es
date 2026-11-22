# Enlace dinámico de bibliotecas en SaguiOS (D378, mejora 138)

Estado: **decidido** el 2026-11-17 (el mantenedor acepta todas las recomendaciones de §11; la 9: GPL-3.0 como hoy).
Fase 1 (auditoría, diseño y prototipo, rama
`v8.5.4.1-dinamico`). Prototipo funcionando: `userland/libdl/ligador.c` (dlopen/dlsym en espacio de usuario) y la
prueba `tests/h385_dlopen.py`.

Índice: 1. Estado actual medido · 2. Qué se gana y qué cuesta · 3. Diseño propuesto · 4. Memoria y kernel ·
5. Seguridad · 6. Bibliotecas y convivencia con lo estático · 7. Rendimiento del PIC · 8. Compiladores ·
9. Plan por fases · 10. Licencias · 11. Preguntas para el mantenedor · 12. El prototipo · 13. Fase 2 ·
14. Revisión de seguridad de la fase 2 · 15. Fase 3 (páginas compartidas) · 16. Fase 3b (`/bin` dinámico).

---

## 1. Estado actual (medido en el código y en `build/`)

**Cargador del kernel** (`kernel/nucleo/carregador.c`, D013):
- Solo acepta **ELF32 ET_EXEC de i386** (`tipo == 2`); un ET_DYN se rechaza con `ERRO_FORMATO`. No mira `PT_INTERP`,
  `PT_DYNAMIC` ni `PT_GNU_RELRO`.
- Lee el fichero **entero** a un búfer del kernel (`vfs_ler_inteiro`) y **copia** cada `PT_LOAD` a páginas nuevas
  del proceso (`espaco_alocar` + `fisica_escrever`). No hay paginación por demanda del fichero ni páginas compartidas:
  dos procesos del mismo programa tienen dos copias completas, también del código.
- **Toda la BSS se asigna al cargar** (solo la pila crece por demanda, D186; el montón se reserva con `MEM_RESERVAR` y
  se asigna con `MEM_ASIGNAR`).
- Permisos por segmento (D349): sin `PF_X` lleva NX; `PF_W` da escritura. El W^X de los ELF lo garantizan los guiones de
  enlace (D351: `.init_array` tras `ALIGN(4096)` en `programa.ld`, `programa_cxx.ld`, `netsurf.ld`, `programa_gp.ld`) y
  lo comprueba `h353`.
- Arranque del programa: `[esp]=argc`, `[esp+4]=argv`, `[esp+8]=envp` (no es la pila SysV con argv en línea ni hay
  vector auxiliar `auxv`). El punto de entrada es `_inicio` (`inicio.asm`), que llama a `principal`.

**Espacio de direcciones** (D049, `kernel/inc/paginacao.h`): programa en `0x80000000`; área de datos/montón hasta
`0xDBFFFFFF` (`DADOS_LIMITE`); pilas de hilos `0xDC000000-0xDFFFFFFF` (D167/D186); ventanas `0xE0000000`; BAR PCI
`0xF0000000`; pila principal arriba. Los hilos comparten el espacio de su líder (D167). **No hay `fork`** (solo
`executar`), así que no hace falta copia al escribir para procesos. **No hay TLS** (ni `%gs` por hilo ni `PT_TLS`).

**Memoria física** (`kernel/mem/fisica.c`): mapa de bits por zonas (baja, alta, superior con PAE), **sin cuenta de
referencias por página**. Las páginas «prestadas» (`PG_EMPRESTADA`, bit 9: píxeles de ventanas, BAR) no se liberan al
destruir el espacio, no las toca `MEM_DEVOLVER` y la swap comprimida (D352) no las saca. Bits libres de la entrada de
página: el 9 (`PG_EMPRESTADA`) y el 11 (`PG_INCOMPRIMIVEL`) están usados; el 10 está libre en el hardware (`PG_NX` es
una bandera del API que se convierte en el bit 63 con PAE); con PAE hay además los bits 52-62.

**Llamadas de memoria del usuario** (D209, D349, D352): `MEM_RESERVAR`, `MEM_ASIGNAR`, `MEM_DEVOLVER`, `MEM_PROTEGER`
(como `mprotect`, también RX y solo lectura) y `MEM_SACAR`, solo dentro del área de datos. **Con eso basta para un
cargador de bibliotecas en espacio de usuario** (lo demuestra el prototipo): no hay que tocar el kernel para la fase 1.

**Cómo se enlaza hoy:** todo estático con `ld -T userland/programa.ld --gc-sections` contra los objetos sueltos de la
libsagui (93 `.o`), y según el programa la libc común, libtls, libfontes, libsgl, libcxx, SDL2 y sus capas (D374). TinyGo
enlaza con `ld.lld` y el mismo guion contra `libgo.a` (D364). `make sdk_externo` (D375) deja banderas, guion y objetos en
`build/sdk/` para repositorios aparte. `--gc-sections` hace que cada programa lleve **solo lo que usa**.

**Tamaños medidos** (compilación de esta rama; bytes de símbolos con tamaño, sin BSS, atribuidos por nombre a cada
biblioteca: es una cota **por debajo**, porque no cuenta los textos literales sin símbolo):

| Dónde | ELF | En disco | libsagui | libtls | SDL2 | libc | libfontes | Total bibliotecas |
|---|---|---|---|---|---|---|---|---|
| ramdisk `/bin` | 180 | 6,7 MB | 3,2 MB | 0,28 MB | 0,34 MB | — | 0,13 MB | **≈3,9 MB (58 %)** |
| `/apps/bin` | 50 | 19,9 MB | 3,5 MB | 0,68 MB | 0,36 MB | 0,35 MB | 0,28 MB | **≈5,3 MB (27 %)** |

Una libsagui **entera** mide 848 KB de código y datos de solo lectura, 1,4 KB de datos y **2,3 MB de BSS**
(`saguimark.o` 1 MB, `compressao.o` 390 KB, `particoes.o` 160 KB, `audio_flac.o` 137 KB, `formatacao.o` 128 KB...),
con 1048 símbolos globales (≈35 KB de tabla dinámica). Mediana por programa: 13 KB de libsagui; máximo 304 KB
(`reproductor`).

**RAM con N programas abiertos** (lo duplicado hoy): una sesión típica de 10 programas (terminal, sagsh, arquivos,
configuracion, pluma, cipo, reproductor, tareas, servicios, editor) lleva **≈1,5 MB** de código de bibliotecas
repetido. Compartido, eso sería ≈1 MB una vez (libsagui + libtls + libfontes enteras). **La RAM no es la gran ganancia**;
lo es el ramdisk, que **se copia entero a la RAM al arrancar** (D324): quitar ≈2,5 MB netos del ramdisk ahorra esa RAM
siempre, y otros ≈4 MB en `/apps`.

**Compiladores internos:**
- `cc` (D033, D039, D108, D128, D133; `userland/programas/cc.c`, `gerador_x86.h`, `elf_saida.h`): una pasada, sin
  ensamblador ni enlazador; escribe un ET_EXEC con tres `PT_LOAD` a direcciones fijas (código `0x80000100`, datos
  `0x88000000`, BSS `0x8C000000`). Su biblioteca (`cc_biblioteca.h`) es **código C que se compila con cada programa**
  y se poda lo no alcanzable; hace las llamadas al sistema directamente (`__syscall`). Direcciones absolutas emitidas
  en 8 sitios de `cc.c` y 3 de `gerador_x86.h`; EBX solo se usa en el envoltorio de las llamadas al sistema (lo guarda).
  Con `-sse2` devuelve `double` en XMM0, **no** en ST(0) como el ABI i386.
- Pascal (D085/D086) comparte el generador y el escritor de ELF; 12 sitios con direcciones absolutas.
- TinyGo (D364): LLVM + `ld.lld` en el PC.
- Java/miniJVM (D376) y Python (MicroPython, D034): hoy sin `dlopen` (`os_sagui.c` lo deja fuera).

---

## 2. Qué se gana y qué cuesta

**Gana:** ramdisk más pequeño (= RAM al arrancar), `/apps` más pequeño; corregir un fallo de la libsagui sin recompilar
todos los programas; `dlopen` para módulos nativos (Java JNI, Python, extensiones, complementos de Cipó...); que
programas de fuera (paquetes D375, `cc`) usen las bibliotecas del sistema sin copiarlas.

**Cuesta:** un ABI de bibliotecas que hay que **mantener estable** (si cambia una estructura de la libsagui, los programas
viejos fallan: hace falta soname con versión); código PIC algo más lento (§7); más complejidad al arrancar un programa
(abrir y relocar); y, mientras no haya páginas compartidas ni BSS por demanda, **más RAM** para los programas pequeños
(un `cat` que hoy lleva 5 KB de libsagui cargaría la biblioteca entera y su BSS de 2,3 MB). Por eso el orden de las fases
importa: ni `/bin` entero a dinámico antes de la fase 3, ni libsagui.so sin quitar antes su BSS grande.

---

## 3. Diseño propuesto

### 3.1 Formato

- Bibliotecas: **ELF32 ET_DYN de i386**, `-fPIC -shared`, con `DT_SONAME` versionado (`libsagui.so.1`), `DT_GNU_HASH`
  (y `DT_HASH` mientras convenga), `-z now -z relro`, `-z noexecstack`, sin `DT_TEXTREL`.
- Ejecutables dinámicos: **ET_EXEC a dirección fija** (como hoy, `0x80000000`) con `PT_INTERP` = `/lib/ld-sagui.so` y
  `PT_DYNAMIC`. PIE (ET_DYN ejecutable) se admite después, solo si se quiere ASLR (§5). Ventaja de empezar con ET_EXEC:
  el código del programa no paga el PIC y sus llamadas a bibliotecas van por la PLT clásica (`jmp *GOT[n]`).
- Los ejecutables estáticos **siguen igual** y siguen siendo lo normal.

### 3.2 ¿Intérprete en espacio de usuario o todo en el kernel?

| | `ld-sagui.so` en usuario, cargado por el kernel (como Linux) | Todo en el kernel |
|---|---|---|
| Código en el anillo 0 | Mínimo: ver `PT_INTERP`, cargar dos ELF, pasar un `auxv` | Relocaciones, búsqueda de símbolos, rutas, versiones: mucho código que lee ficheros no fiables |
| Un fallo en el cargador | Mata al proceso | Puede tumbar el sistema |
| `dlopen` | El mismo código (ya existe: el prototipo) | Haría falta una llamada nueva igualmente |
| Constructores | En usuario, naturales | El kernel tendría que saltar al usuario y volver |
| Rendimiento | Igual en la práctica (el coste son las relocaciones, igual en los dos) | — |

**Recomendado: intérprete en usuario.** El kernel solo: (a) reconoce `PT_INTERP`, (b) carga el ejecutable y el
intérprete (el intérprete es un ET_DYN pequeño y autocontenido, cargado en una base fija alta, p. ej. `0xDB000000`),
(c) añade un **cuarto** valor a la pila de arranque: `[esp+12]` = puntero a un vector auxiliar (`AT_PHDR`, `AT_PHNUM`,
`AT_ENTRY`, `AT_BASE`, `AT_PAGESZ`, `AT_SECURE`), compatible con los programas actuales (que solo leen tres), y (d)
salta a la entrada del intérprete. `AT_SECURE=1` si el proceso es root o un ejecutable del sistema con permisos
(`EXE_SERVICIOS`, `EXE_BIN`, D251/D269): el intérprete ignora entonces las variables de entorno.

### 3.3 El cargador (ld-sagui / libdl)

- **Relocaciones i386 (REL, con sumando implícito):** `R_386_RELATIVE` (B+A), `R_386_32` (S+A), `R_386_PC32` (S+A-P),
  `R_386_GLOB_DAT` y `R_386_JMP_SLOT` (S), `R_386_COPY` (solo en ejecutables: copia el dato de la biblioteca a la BSS del
  programa; necesario para que un ET_EXEC no PIC use `errno`, `environ`...). Rechazar RELA, TLS (`R_386_TLS_*`,
  `PT_TLS`), IFUNC y cualquier tipo desconocido con un mensaje claro. El prototipo ya hace todas menos COPY.
- **Símbolos:** `DT_GNU_HASH` (más rápido: filtro de Bloom) y `DT_HASH`; ámbito global = ejecutable, después las
  bibliotecas en orden de carga en anchura (cada una antes que sus dependencias), como el gABI. Débiles sin definir = 0.
- **Enlace inmediato + RELRO completo** (recomendado): todas las relocaciones al cargar (`-z now`), después la GOT y
  `.data.rel.ro` pasan a solo lectura. No hay trampolín perezoso (`_dl_runtime_resolve`) que escribir, depurar ni
  proteger; el coste de arranque con libsagui (≈1000 símbolos) es de milisegundos. Perezoso solo ahorra en programas
  enormes que llaman a pocas funciones, y deja la GOT escribible (vector de ataque).
- **Rutas:** `DT_RUNPATH` (con `$ORIGIN`), `SAGUI_LIB_CAMINHO` (como `LD_LIBRARY_PATH`), `/lib`, `/apps/lib`. Con
  `AT_SECURE` (root, elevados, `/bin` con permisos): sin variable; del `DT_RUNPATH` solo `/lib`, `/apps/lib` o
  `$ORIGIN` (la carpeta de una biblioteca que ya se cargó desde una ruta de confianza: el programa la eligió).
- **Versiones:** `soname` con número mayor (`libsagui.so.1`); un cambio incompatible sube el número y las dos conviven.
  Versionado de símbolos (`.gnu.version`) no hace falta al principio: se ignora.
- **`dlopen/dlsym/dlclose/dlerror`** con cuenta de referencias; `RTLD_LAZY` = `RTLD_NOW`; `RTLD_LOCAL/GLOBAL` (el
  prototipo hace todo global); `dlopen(NULL)` = el programa (cuando el ejecutable tenga `.dynsym`).
- **Constructores/destructores:** `DT_INIT`, `DT_INIT_ARRAY` (dependencias antes), `DT_FINI_ARRAY` al revés y `DT_FINI`
  en `dlclose` y al salir (`atexit` del intérprete). Llamar a `dlopen` desde un constructor necesita un cerrojo
  recursivo (el prototipo usa uno simple: anotado).
- **Hilos:** un cerrojo global del cargador (D167); sin TLS, una biblioteca con `__thread` se rechaza.

---

## 4. Memoria y kernel (fase 3, v8.5.4.5: lo delicado)

**Objetivo:** que el código y los datos de solo lectura de una biblioteca (y del ejecutable) estén **una vez** en la RAM
aunque los usen N procesos.

> **Hecho en la v8.5.4.5 (2026-11-18):** ver §15 — lo que se hizo y en qué difiere de esta propuesta (comparación byte a
> byte en vez de confiar solo en la clave, objeto liberado al llegar a cero sin LRU, ejecutables sin compartir todavía).

**Diseño recomendado: «imágenes compartidas» de solo lectura, sin copia al escribir.**
1. Una tabla del kernel de **objetos imagen**: clave = (disco, nº de fichero o primer bloque, tamaño, fecha de
   modificación, desplazamiento, longitud); valor = vector de páginas físicas y una **cuenta de referencias**.
2. Llamada nueva `SIS_MAPEAR_COMPARTIDO(fd, desloc, bytes, dirección, prot)`: solo **R o RX, nunca W**. Si el objeto
   existe, mapea sus páginas (cuenta +1); si no, lo crea leyendo el fichero en páginas nuevas (pueden ser **altas o por
   encima de 4 GB con PAE**: se escriben por ventana con `fisica_escrever`, como ya hace el cargador, D348/D349).
3. Las entradas se marcan `PG_EMPRESTADA` + bit 10 («compartida»): así `espaco_destruir`, `MEM_DEVOLVER`, `espaco_tirar`
   y la swap (D352) ya las dejan en paz **sin tocar su código**. Cada proceso lleva una lista corta de sus objetos
   mapeados; al destruirse el espacio (o desmapear), la cuenta baja.
4. Lo escribible (`.data`, `.got` antes de RELRO, `.bss`) **siempre es privado**: el intérprete lo pide con
   `MEM_ASIGNAR` y lo copia. Como el código PIC no se reloca nunca, **no hace falta copia al escribir**; sin `fork`
   tampoco. Esto evita el cambio más arriesgado (fallos de página que copian).
5. Un objeto con cuenta 0 se queda en una LRU y se libera con el reclamador (`memfisica_registar_reclamo`), igual que la
   caché de disco cede memoria (D350). Es independiente de la caché de bloques: no se mapean bloques de la caché (podrían
   desalojarse); se copian una vez.
6. Swap comprimida: las páginas compartidas no se sacan (son limpias y recargables; más adelante se podrían «soltar» y
   volver a leer del fichero, pero no hace falta al principio).
7. Si el fichero cambia (actualización), la clave cambia: los procesos viejos siguen con las páginas viejas, los nuevos
   cargan las nuevas.

**BSS por demanda:** antes de pasar `/bin` a dinámico hay que (a) sacar los búferes estáticos grandes de la libsagui a
`aloca` (saguimark, compresión, particiones, FLAC, formatos: 2,2 MB) o (b) que la BSS de un segmento se reserve sin
asignar y llegue por fallo de página, como la pila (D186). (a) es seguro y local; (b) toca el fallo de página (como
`pilha.c`). Recomendado (a) primero.

**Qué tocar del kernel:** `carregador.c` (ET_DYN/`PT_INTERP`/auxv: ~150 líneas), una llamada nueva y un módulo
`kernel/mem/imagens.c` (~400 líneas), `espaco_destruir` (bajar cuentas), `processo_t` (lista de objetos), el reclamador.
**Riesgos:** cuenta de referencias mal llevada = página liberada en uso (corrupción entre procesos) o fuga; hilos en
otra CPU con la TLB vieja al desmapear (usar `espaco_sincronizar`, D165); la barrera de D351 (no desmapear mientras otro
hilo duerme en una llamada que usa esas páginas). Por eso va en su propia fase, con pruebas de estrés (muchos procesos
abriendo y cerrando la misma biblioteca, con `--smp`, con swap y con memoria alta) y con KVM.

---

## 5. Seguridad

- **W^X:** las bibliotecas son `-fPIC` obligatorio; `DT_TEXTREL` se rechaza (lo hace el prototipo); ningún segmento
  W+X; el código nunca es escribible ni siquiera durante la carga (con páginas compartidas, las escribe el kernel por
  ventana, no el proceso).
- **NX:** datos, GOT, BSS y montón sin ejecución (D349).
- **RELRO completo:** GOT y `.data.rel.ro` de solo lectura tras relocar (comprobado en h385: escribir ahí mata al
  proceso).
- **Rutas seguras:** con `AT_SECURE` (root, servicios, programas de `/bin` que el diálogo de permisos recuerda por ruta,
  D269) solo `/lib`, `/apps/lib` (que solo root escribe) y `$ORIGIN`; nunca `SAGUI_LIB_CAMINHO` ni un `DT_RUNPATH` literal
  de otras carpetas. Si no,
  una biblioteca puesta en `/home` heredaría la autorización de un programa de `/bin`.
- **ASLR:** opcional y más adelante: base aleatoria de las bibliotecas (gratis con PIC) y, con PIE, del ejecutable. El
  coste es depuración más difícil (direcciones distintas en cada arranque) y la entropía en 32 bits es pequeña.
- **Validación:** el cargador comprueba límites de todo desplazamiento leído del fichero (relocaciones, tablas, cadenas);
  hay que pasarle un fuzzer en el PC (el código compila también fuera de SaguiOS).

---

## 6. Qué bibliotecas primero y convivencia con lo estático

Orden recomendado:
1. **libsagui.so.1** (la que más se repite: 6,7 MB entre `/bin` y `/apps`), tras sacar su BSS grande y separar en
   `.so` aparte lo pesado y poco usado (`h264`, `aac`, `mp4`, `presentacion_pptx`, `saguimark`): una **libsagui-medios.so**.
2. **libtls.so** y **libfontes.so** (≈1 MB y ≈0,4 MB repetidos).
3. **libc.so** (la común: pequeña hoy, 0,35 MB; es la base del `cc` dinámico) y **libsdl2.so** con sus capas.
4. **libcxx.so** al final (C++: `.eh_frame` y excepciones entre bibliotecas, RTTI: más delicado).

Convivencia: los programas estáticos siguen funcionando siempre; el dinámico es **opcional por programa**
(`DINAMICO=1` en su regla del Makefile). Primero `/apps` (no está en el ramdisk crítico para arrancar); `/bin` después
de la fase 3 y con un plan de recuperación: `servicios`, `sagsh`, `login`, `admin`, `su` e instalador **siempre
estáticos**, para que una biblioteca rota no deje el sistema sin arrancar ni sin shell. `ld-sagui.so` es estático (no
depende de nada).

---

## 7. Rendimiento del PIC en i386 (medido)

En i386 el PIC reserva un registro (EBX) para la GOT, que se carga en el prólogo de cada función con `call/pop`, y las
llamadas entre funciones exportadas van por la PLT. Caso real medido en SaguiOS (h385): el **SHA-256 de
`kernel/lib/sha256.c`** (el mismo fuente, `-O2`), 24 MB, con RDTSC, bajo KVM (Ryzen):

| Variante | Mciclos | Diferencia |
|---|---|---|
| Estático (como hoy) | 227 | — |
| `.so` `-fPIC` por defecto (llamadas internas por la PLT) | 232 | **+2,2 %** |
| `.so` `-fPIC -Wl,-Bsymbolic-functions` | 229 | **+0,9 %** |

Código: 1480 → 1499 bytes (+1,3 %). Conclusión: el coste es pequeño si las llamadas internas no pasan por la PLT:
compilar las bibliotecas con `-fvisibility=hidden` y exportar solo el API (o `-Bsymbolic-functions`). En CPU antiguas
con menos registros útiles (Pentium II/III) el coste de perder EBX puede ser mayor (la literatura da 5-10 % en código
con mucha presión de registros): repetir la medida en la Amilo/L635 antes de pasar código caliente (decodificadores de
vídeo, `memcpy` SSE2) a `.so`. El código caliente de los programas (que siguen siendo ET_EXEC) no paga nada.

---

## 8. Compiladores

**gcc/ld del PC:** bibliotecas con `-fPIC -fvisibility=hidden` y `ld -shared -soname libX.so.N -z now -z relro
-z noexecstack --hash-style=gnu`; ejecutables con `ld -T programa_din.ld --dynamic-linker /lib/ld-sagui.so -z now
-z relro libsagui.so ...`. El guion `programa_din.ld` añade `.interp`, `.dynamic`, `.dynsym`, `.plt`, `.got`. Para los
repositorios externos, `make sdk_externo` añadiría las `.so` (solo para enlazar: bastan sus tablas) y el guion.

**TinyGo/lld:** el objetivo `saguios.json` admite `"relocation-model": "pic"` para bibliotecas; para ejecutables basta
pasar `--dynamic-linker` y las `.so` a `ld.lld` (GNU y lld producen el mismo formato). Trabajo estimado: pequeño una
vez exista ld-sagui (1-2 días).

**`cc` interno, fase A — ejecutables que llaman a bibliotecas compartidas** (≈ 1-2 semanas):
- Una función declarada y no definida (o marcada con un `#pragma biblioteca "libc.so.1"`) se llama por una **entrada de
  PLT** del propio programa: `jmp *[GOT+4n]` (direccionamiento absoluto: el programa sigue siendo ET_EXEC a dirección
  fija, así que la PLT es la más simple posible, sin EBX). `chamar_funcao`/`endereco_funcao` ya separan definidas y no
  definidas: el cambio es dónde apuntan.
- Datos externos (`errno`, `environ`): `R_386_COPY` (la variable vive en la BSS del programa: el generador no cambia).
- `elf_saida.h` escribe además `PT_INTERP`, `PT_DYNAMIC`, `.dynsym`, `.dynstr`, `.hash` (el SysV es más fácil de
  generar), `.rel.plt` con `R_386_JMP_SLOT`, `.rel.dyn` con `R_386_COPY`, la GOT y `DT_NEEDED`/`DT_BIND_NOW`.
- Su biblioteca estándar deja de compilarse con cada programa y pasa a ser **libc.so** (la libc común de SaguiOS,
  compilada con gcc). Atención al ABI: el `cc` con `-sse2` devuelve `double` en XMM0; para llamadas a bibliotecas tiene
  que usar el convenio i386 (ST(0)), y no admite structs por valor (las funciones de libc que los usan quedan fuera).
- El `cc` sigue generando estático por defecto hasta que libc.so esté en todas las imágenes (pregunta 6).

**`cc`, fase B — generar bibliotecas compartidas (`cc -shared`)** (≈ 2-3 semanas):
- Código PIC: EBX = GOT, cargado en el prólogo de cada función con `call x; x: pop ebx; add ebx, GOT-x`; EBX es
  preservado por el llamado en el ABI i386 y el `cc` solo lo usa en el envoltorio de `__syscall` (ya lo guarda).
- Los 11 sitios que emiten direcciones absolutas (`DADOS_BASE+off`, `BSS_BASE+off`, `COD_BASE+pos`) pasan a
  `lea eax, [ebx+off@GOTOFF]`; las globales exportadas y las funciones de otras bibliotecas, por la GOT (`mov eax,
  [ebx+n@GOT]`) y llamadas por la PLT con EBX.
- Los inicializadores de datos con direcciones (tablas de punteros) generan `R_386_RELATIVE`.
- ELF ET_DYN con `.dynsym` de lo exportado (lo no `static`), `DT_SONAME`, `-z now` implícito, segmentos alineados a
  página (código RX, datos RW, RELRO).

**Pascal:** comparte generador y escritor: fase A ≈ 3-5 días más tras la del `cc` (llamadas a `external` y unidades en
`.so`); fase B ≈ 1 semana (12 sitios con direcciones absolutas).

**Java (miniJVM) y Python:** solo necesitan `dlopen`/`dlsym` (fase 1 + bibliotecas). `System.loadLibrary("x")` →
`dlopen("/apps/java/lib/libx.so")` y `Java_<clase>_<método>` por `dlsym`; Python, un módulo `ffi` o de extensiones C
nativas cargadas por `dlopen`. Las bibliotecas que carguen necesitan libsagui/libc como `.so` (o recibir el API por
tabla, como `dl_exportar` del prototipo).

---

## 9. Plan por fases

| Fase | Qué | Prueba | Riesgo |
|---|---|---|---|
| 1 (hecha) | `libdl` en usuario: `dlopen/dlsym/dlclose/dlerror`, relocaciones, RELRO, W^X; sin compartir páginas | h385 | Ninguno para el sistema: solo lo usa quien lo enlaza |
| 2a (hecha) | Ejecutables con `PT_INTERP`: kernel acepta ET_EXEC con intérprete y pasa `auxv`; `ld-sagui.so` (el código de la fase 1 + `R_386_COPY` + arranque) | h386: programa dinámico de prueba, programa estático igual que antes, AT_SECURE con root | Bajo: cambio pequeño en `carregador.c`; los estáticos no pasan por él |
| 2b (hecha en parte) | Sacar la BSS grande de la libsagui; `libsagui.so.1`, `libtls.so.1`, `libfontes.so.1`, `libc.so.1` con visibilidad oculta; reglas `DINAMICO=1`; algunos programas de `/apps` dinámicos | h385 + las pruebas de esos programas; medida de arranque | Medio: ABI; un fallo de la `.so` afecta a todos sus programas |
| 3 | Páginas compartidas en el kernel (objetos imagen, cuenta de referencias, LRU, sin COW) | Estrés con `--smp`, swap y memoria alta; KVM | **Alto** (núcleo de memoria): fase propia, revisión de Opus |
| 3b (hecha, D462) | `/bin` dinámico salvo lo esencial (servicios, sagsh, login, admin, su, instalador, el initramfs y lo de reparar); las `.so` del sistema en `/lib` | h462 + reducida; batería completa | Medio: arranque; plan de vuelta atrás = `DINAMICO=0` (imagen estática) |
| 3c (a futuro) | Ejecutables e intérprete compartidos en memoria (una copia del código en la RAM para todos los procesos) | Estrés con `--smp`, swap y memoria alta | Alto (núcleo de memoria): fuera de la 3b |
| 4 | `cc` fase A (ejecutables que llaman a libc.so) | b5/b7/b8 + prueba nueva | Medio: ABI x87/SSE2 del `cc` |
| 5 | `cc` fase B (`cc -shared`) y Pascal A y B | Prueba nueva: biblioteca del `cc` llamada desde gcc y viceversa | Medio |
| 6 | TinyGo dinámico; JNI en miniJVM; extensiones C de Python | h366, h378, b6 | Bajo |
| 7 (opcional) | PIE y ASLR | — | Bajo-medio |

---

## 10. Licencias

- El enlace dinámico **no cambia la situación legal** de un programa GPL-2.0-only frente a la libsagui con ficheros
  GPL-3.0-only (D375): para la FSF, enlazar dinámicamente también forma una obra combinada. OpenTTD, NetSurf y GNUBoy
  siguen igual (solo fuentes y receta) mientras la libsagui que cargan tenga partes GPL-3.0-only. Lo que sí ayuda es lo
  de siempre: una capa propia con licencia compatible con las dos, o una excepción de enlace del titular.
- El cargador (`libdl`, `ld-sagui`) es propio (GPL-3.0-only como el resto del sistema). Si en el futuro se quiere que
  programas con otras licencias lo usen, conviene decidir su licencia (p. ej. GPL-3.0-or-later con excepción de enlace,
  como la de la libgcc) **antes** de que haya programas de fuera que dependan de él.

---

## 11. Preguntas para el mantenedor (contestadas el 2026-11-17)

**Respuestas:** 1a, 2a, 3a, 4a, 5a, 6a, 7a, 8a (las recomendadas) y 9a (GPL-3.0 como hoy). La fase 3 va aparte, con
revisión.

1. **¿Dónde va el cargador de ejecutables dinámicos?**
   a) `ld-sagui.so` en espacio de usuario, cargado por el kernel (como Linux). **(recomendado)**
   b) Todo en el kernel.
   c) Solo `dlopen` (bibliotecas cargadas a mano por los programas que quieran), sin ejecutables dinámicos.
2. **¿Enlace inmediato o perezoso?**
   a) Inmediato + RELRO completo (GOT de solo lectura). **(recomendado)**
   b) Perezoso (arranque algo más rápido en programas enormes; GOT escribible).
   c) Inmediato por defecto y perezoso opcional por programa.
3. **¿Qué bibliotecas primero?**
   a) libsagui (partida en base + medios) y libc. **(recomendado)**
   b) libsagui, libtls, libfontes, libc a la vez.
   c) Empezar por SDL2 + capas (ports grandes de `/apps`) y la libsagui después.
4. **¿Hasta dónde llega lo dinámico en el sistema?**
   a) Solo `/apps` hasta tener páginas compartidas; después `/bin` salvo lo esencial (servicios, sagsh, login, admin,
      su, instalador siempre estáticos). **(recomendado)**
   b) Solo `/apps`, siempre; `/bin` estático para siempre.
   c) Todo dinámico en cuanto funcione.
5. **¿Páginas compartidas en el kernel (fase 3)?**
   a) Sí, solo lectura sin copia al escribir (objetos imagen con cuenta de referencias). **(recomendado)**
   b) No por ahora: solo el ahorro de disco y ramdisk (cada proceso copia la biblioteca).
   c) Sí, y además copia al escribir general (más trabajo; no hace falta sin `fork`).
6. **¿El `cc` genera dinámico por defecto?**
   a) Estático por defecto; `cc -dinamico` opcional; cambiar el defecto cuando libc.so esté en todas las imágenes.
      **(recomendado)**
   b) Dinámico por defecto en cuanto exista la fase A.
   c) Solo estático (no hacer las fases 4-5).
7. **¿ASLR?**
   a) No por ahora; más adelante, base aleatoria de las bibliotecas como opción. **(recomendado)**
   b) Sí desde el principio (bibliotecas y PIE).
   c) Nunca (depuración más sencilla, direcciones estables).
8. **¿Compatibilidad de ABI entre versiones?**
   a) `soname` con número mayor (`libsagui.so.1`); incompatible = número nuevo y las dos conviven en `/lib`; se comprueba
      el ABI en la batería (lista de símbolos exportados). **(recomendado)**
   b) Sin promesa: cada versión de SaguiOS reconstruye todo (los paquetes externos se recompilan).
   c) Versionado de símbolos al estilo glibc (más trabajo).
9. **¿Licencia del cargador y de las `.so` del sistema para programas de fuera?**
   a) Dejar GPL-3.0-only como todo (lo de fuera tiene que ser compatible con GPL v3). **(lo actual)**
   b) Añadir una excepción de enlace (tipo libgcc) a ld-sagui y a las capas que se reescriban.
   c) Decidirlo cuando se resuelva el bloqueo de D375.

---

## 12. El prototipo (fase 1, hecho)

- `userland/inc/ligador.h`, `userland/libdl/ligador.c` (~600 líneas): `dlopen`, `dlsym`, `dlclose`, `dlerror`,
  `dl_exportar` (el programa estático ofrece símbolos propios a las bibliotecas) y `dl_info`. Carga en páginas del
  montón (`mapear_memoria`), aplica `RELATIVE/32/PC32/GLOB_DAT/JMP_SLOT`, `DT_GNU_HASH` y `DT_HASH`, `DT_NEEDED` con
  `DT_RUNPATH=$ORIGIN`, constructores y destructores, protege código RX, solo lectura R y RELRO R, y rechaza
  `DT_TEXTREL`, W+X, TLS, IFUNC y RELA. Antes de devolver las páginas al `malloc` las vuelve a RW.
- **Sin cambios en el kernel.**
- Bibliotecas de prueba (`userland/testes/dl/`): `libdlbase.so` (GNU hash, RELRO, constructor), `libdldep.so` (SysV
  hash, `DT_NEEDED` + `$ORIGIN`, PLT, GOT, llamada al programa), `libdlsha.so` y `libdlsha_simb.so` (medida del PIC) y
  `libdlmal.so` (sin `-fPIC`: se tiene que rechazar). Van a `/testes/lib/`.
- `/testes/dlprueba` y `tests/h385_dlopen.py`: 22 comprobaciones dentro de SaguiOS, todas bien con TCG y con KVM.
- Límites conocidos: un constructor que llame a `dlopen` se bloquea (cerrojo no recursivo); `RTLD_LOCAL` se trata como
  global; `dlopen(NULL)` no se admite (el programa no tiene `.dynsym`); `$ORIGIN` solo al principio de `DT_RUNPATH`.

---

## 13. Fase 2 (hecha, 2026-11-17)

**Paso 1 — BSS y partición de la libsagui.** Los búferes estáticos grandes pasan al malloc la primera vez que se
usan (mismo comportamiento): saguimark 1 MB, compresor 384 KB, particiones 128 KB, formateo 128 KB, FLAC 128 KB,
AAC 80 KB y la pila de la regex 32 KB (≈1,8 MB menos de BSS). La libsagui se parte en el `Makefile`:
`LIBSAGUI_BASE_OBJ` (los 39 objetos que usan 10 o más programas, cerrados por dependencias: arranque, llamadas,
texto, memoria, ventanas, imágenes, red, compresión, regex) y `LIBSAGUI_MEDIOS_OBJ` (el resto). Los estáticos siguen
enlazando `LIBSAGUI_OBJ` entero.

**Paso 2a — intérprete.**
- Kernel (`carregador.c`, unas 120 líneas): si el ejecutable trae `PT_INTERP` (solo rutas de `/lib/`, sin `..`),
  carga además el intérprete, un ELF ET_EXEC estático en `[INTERP_BASE=0xD8000000, DADOS_LIMITE)`, y empieza en él.
  En la pila, `[esp+12]` apunta al vector auxiliar (`AT_PHDR` —copia de las cabeceras de programa en la pila—,
  `AT_PHENT`, `AT_PHNUM`, `AT_PAGESZ`, `AT_BASE`, `AT_ENTRY`, `AT_SECURE`). `AT_SECURE=1` si el proceso es root o el
  programa es de los de `/bin`/servicios (`exe_sistema`). El área de datos de esos procesos acaba en `INTERP_BASE`
  (`processo_t.dados_teto`). `MEM_PROTEGER` admite además dejar **en solo lectura** (nunca dar W o X) páginas de la
  imagen del programa: la RELRO de su GOT.
- **Intérprete a dirección fija en vez de ET_DYN** (diferencia con Linux): así el kernel lo carga con el mismo código
  que cualquier programa y el intérprete no tiene que relocarse a sí mismo.
- `/lib/ld-sagui.so.1` (25 KB) = `ligador.c` compilado con `LD_INTERPRETE` + `ld_sagui.c`. Sin malloc: estructuras en
  una reserva fija y páginas con `MEM_RESERVAR`/`MEM_ASIGNAR`. Abre los `DT_NEEDED`, reloca el programa (con
  `R_386_COPY`, p. ej. `environ`), aplica su RELRO (también las páginas que la RELRO comparte con la parte de solo
  lectura del segmento de datos), llama a los constructores de las bibliotecas (cuando ya está todo
  relocado) y salta a `_inicio` con la pila intacta. Publica su tabla de funciones en `__ld_sagui_tabela` (definida
  en `dl_ponte.c`, dentro de libsagui.so), y así `dlopen` en un programa dinámico usa el mismo cargador.
- **PLT canónica:** en un programa no PIC, la dirección de una función de biblioteca es su entrada de la PLT del
  programa; el intérprete la usa para todo salvo `JMP_SLOT` (igualdad de punteros entre programa y bibliotecas,
  como glibc).

**Paso 2b — bibliotecas del sistema.** Una submake (`PIC=1 B=build/pic`) recompila con `-fPIC` los mismos objetos y
banderas. `-shared -z now -z relro -z text --hash-style=gnu -Bsymbolic-functions`, y `strip --strip-unneeded`.

| Fichero | Dónde | Tamaño |
|---|---|---|
| `ld-sagui.so.1` | `/lib` (ramdisk) | 25 KB |
| `libsagui.so.1` | `/apps/lib` | 263 KB (245 KB de código, 38 KB de BSS, 472 símbolos) |
| `libc.so.1` (con la libm x87 del MicroPython) | `/apps/lib` | 100 KB (374 símbolos) |
| `libsagui_medios.so.1` (con el SHA-256) | `/apps/lib` | 719 KB |

Mientras solo haya programas dinámicos en `/apps` (decisión 4a), las tres van a `/apps/lib`: el ramdisk tiene un
límite de 12 MB (h324) y hoy está a 60 KB de él; solo el intérprete va a `/lib` (+25 KB). Cuando `/bin` pase a dinámico
(después de la fase 3), libsagui.so.1 y libc.so.1 se mueven a `/lib` y el ramdisk adelgaza. Las pruebas del enlace
dinámico (`dlprueba`, `dinamico` y sus `.so`) van a `/apps/testes` por lo mismo.

Primer programa de `/apps` dinámico: **buscaminas** (53,6 KB estático → 21,4 KB). Un programa dinámico se enlaza
con el guion por defecto del `ld` a `0x80000000` (`-Ttext-segment`), `inicio.asm.o` como arranque y
`--dynamic-linker /lib/ld-sagui.so.1` (`$(call enlazar_dinamico,...)` en el `Makefile`).

**ABI (decisión 8a):** `userland/libsagui.so.1.simbolos` y `userland/libc.so.1.simbolos` guardan lo exportado; h386
falla si falta alguno (quitar un símbolo obliga a subir el soname) e informa de los nuevos.

**Pruebas:** `tests/h386_dinamico.py` (`/testes/dinamico`: argc/argv, `environ` por COPY, malloc, `snprintf` y
`strtol` de libc.so, `dlopen` por el intérprete con llamada al programa, y W^X/RELRO: escribir en el código de
libsagui.so o en la GOT del programa mata al proceso), h247 (buscaminas dinámico), h385 y las de siempre.

**Límites que quedan:** los destructores de las bibliotecas no se llaman al salir con `sair` (sí con `dlclose`);
`dlopen(NULL)` no se admite; las bibliotecas se cargan en páginas propias de cada proceso (compartirlas es la fase 3);
el ahorro de disco llega cuando más programas pasen a dinámicos.

## 14. Revisión de seguridad de la fase 2 (2026-11-17)

Revisión independiente, como atacante, antes de fusionar (después de la 8.5.4). Lo corregido:

- **`$ORIGIN` del programa salía de `argv[0]`** (`ld_sagui.c`). `argv[0]` lo elige quien lanza el programa, y en un
  proceso seguro `$ORIGIN` se admite: lanzar un programa de `/bin` (que el diálogo de permisos recuerda, D269) o uno
  de root con `argv[0]="/home/x/p"` habría llevado su `DT_RUNPATH=$ORIGIN` a `/home/x`. Ahora el kernel pone la ruta
  resuelta del ejecutable en **`AT_EXECFN`** (31) y el intérprete solo usa esa; sin ella, sin `$ORIGIN`.
- **Bibliotecas de confianza en procesos seguros** (`de_confianza`): además de no leer `SAGUI_LIB_CAMINHO`, solo se
  carga un fichero de root sin escritura de grupo ni de otros, con todas sus carpetas hasta la raíz igual, de un
  sistema de archivos con dueños. Cubre: `/apps` instalada sin `debugfs` (sus ficheros quedarían del uid de quien
  compila, 1000 = sagui), un `/apps` en FAT, un `$ORIGIN` que acabe en `/tmp` (1777) o en `/home`, y `dlopen` con
  ruta explícita. h386 lo comprueba como root (rechaza `/tmp/libdltmp.so`, ignora `SAGUI_LIB_CAMINHO`) y como sagui
  (sí carga).
- **El intérprete también:** el kernel solo carga un `PT_INTERP` que sea de root sin escritura de grupo ni de otros, él
  y sus carpetas desde `/lib`, en un sistema de archivos con dueños (si no, `ERRO_ACESSO` y un aviso en el registro).
- **Proceso seguro = `AT_SECURE` o root ahora o programa del sistema.** Un proceso que se eleva después de arrancar
  (D255: `admin`, diálogo de permisos, `USU_FIJAR`) tenía `AT_SECURE=0` para siempre: sus `dlopen` posteriores
  habrían leído `SAGUI_LIB_CAMINHO` como root. El ligador lo vuelve a mirar en cada búsqueda (`sou_root`) y
  `USU_ESTADO` dice `USU_EST_SISTEMA` (programa de `/bin`/servicios/vncd), para el `libdl` de un programa estático, que
  antes solo miraba root.
- **Rutas que no caben** (`dlopen` de más de 95 caracteres, `$ORIGIN` + resto, carpeta + nombre): antes se cortaban y
  podían abrir **otro** fichero; ahora son un error o la carpeta se salta.
- **Tablas del fichero:** `DT_HASH`/`DT_GNU_HASH` con todas las lecturas dentro de la imagen y sin bucles sin fin
  (cadena cíclica), la tabla de nombres tiene que acabar en cero, `DT_FINI_ARRAY` dentro de la imagen y `R_386_COPY`
  lee solo dentro de la biblioteca que define el símbolo. Más de 64 bibliotecas nuevas en una operación es un error
  (antes se saltaban constructores).
- Kernel: `AT_SECURE` se busca por su clave (no por posición fija), el vector auxiliar va alineado a 16 y no se
  pierde memoria si `processo_novo` falla.

**Comprobado y correcto:** `PT_INTERP` (≤ 64 bytes, terminado en cero, `/lib/`, sin `..`; no hay enlaces simbólicos en
el VFS), un intérprete no puede traer `PT_INTERP`, los segmentos del programa en `[0x80000000, INTERP_BASE)` y los del
intérprete en `[INTERP_BASE, DADOS_LIMITE)` (sin solaparse entre sí, con la pila ni con los hilos; el área de datos
acaba en `INTERP_BASE`), copia de cabeceras acotada (`PH_MAX` = 32), los estáticos no cambian (`lim = DADOS_LIMITE`,
3 valores en la pila). `MEM_PROTEGER` en la imagen solo con `d == 0`: quita escritura y pone NX, nunca da W ni X; el
intérprete no se puede tocar (está fuera de la imagen y del área de datos). Relocaciones: destino y símbolo dentro de
la imagen; `TEXTREL`, TLS, IFUNC y RELA rechazados; dependencias cíclicas resueltas por la lista (una sola copia,
sin recursión infinita; la cuenta de referencias del ciclo no baja a cero: fuga, no fallo). La BSS al malloc: todas
las rutas comprueban el `NULL` y devuelven error.

**Riesgos que quedan (no corregidos aquí):**
- Un programa dinámico de `/apps` lanzado por una cuenta normal (`AT_SECURE=0`) con `SAGUI_LIB_CAMINHO` de esa cuenta
  puede pedir después elevarse por el diálogo: si la persona acepta, la biblioteca de la cuenta corre como root. No es
  peor que un programa propio de esa cuenta con el mismo nombre (el diálogo muestra el nombre, no la ruta, y solo los de
  `/bin`, que ya son seguros, se recuerdan sin diálogo); si se quiere cerrar, que el diálogo marque los procesos
  dinámicos no seguros o que el kernel guarde «cargó bibliotecas con `AT_SECURE=0`» y lo diga.
- La comprobación de dueños es por ruta (no por descriptor): solo vale porque las carpetas también son de root sin
  escritura de otros.
- Falta un fuzzer en el PC del cargador (§5).

**Para la 8.5.3.1 (`/lib` en la raíz en disco):**
- La raíz ext2 tiene que dejar `/`, `/lib`, `/lib/ld-sagui.so.1` (y `/apps`, `/apps/lib` y las `.so`) **de root, sin
  escritura de grupo ni de otros** (`normalizar_donos` de `mkimagem.py` y el instalador): si no, el kernel no carga el
  intérprete y los programas dinámicos no arrancan (fallo seguro, pero visible en h386/h247).
- Una raíz o un `/apps` sin dueños (FAT, ISO 9660 de un CD en vivo) **no sirve** para programas dinámicos: o se monta
  con dueños, o el CD en vivo lleva su ramdisk (con dueños) para `/lib` y `/apps/lib`.
- Si `/lib` deja de ser del ramdisk, el ramdisk ya no limita a `/apps/lib`: `libsagui.so.1` y `libc.so.1` pueden pasar
  a `/lib` (cambiar `SO_APPS_DIR`/`SO_SISTEMA` en el `Makefile` y las rutas de h386).
- `interp_confiavel` (kernel) y `de_confianza` (ligador) suben hasta `/lib` y hasta `/`: no hace falta cambiarlos, pero
  sí que el punto de montaje de la raíz devuelva dueños (`com_donos`).

**Hecho al integrar la 8.5.3.1 (D378, nota de integración):** la raíz SAGUI-RAIZ sale con esos dueños (`normalizar_donos`,
y sin debugfs no se hace); el instalador fuerza `/lib`, `/apps` y `/apps/lib` a root sin escritura de otros
(`blindar_raiz`); el ligador excluye `/apps/vps` y las carpetas con `..` de la búsqueda por nombre (`carpeta_excluida`).

## 15. Fase 3: páginas compartidas (v8.5.4.5, hecha el 2026-11-18)

**Qué se comparte:** las páginas de una biblioteca que solo tocan segmentos sin escritura, enteras dentro de la parte
del fichero (sin BSS) y con el mismo desplazamiento fichero-memoria. Con el `-z separate-code` por defecto del `ld`, eso
son las cabeceras y `.dynsym`/`.dynstr`/`.gnu.hash`/`.rel.*` (R), el código y la PLT (RX) y `.rodata`/`.eh_frame` (R).
Lo escribible (`.data`, `.got`, `.data.rel.ro` antes de la RELRO, `.bss`) sigue privado de cada proceso, como antes: el
código PIC no se reloca, así que **no hace falta copia al escribir** (tampoco hay `fork`).

**Kernel (`kernel/mem/imagens.c`, llamada `SIS_MEMORIA` / `MEM_COMPARTIR`):**
`memoria(MEM_COMPARTIR, dirección, bytes, fd << 8 | MEM_PROT_EXEC o 0, desplazamiento)` mapea de solo lectura las páginas
`[desloc, desloc+bytes)` del fichero abierto `fd` en un tramo **sin asignar** del área de datos. Pasos:
1. Comprobaciones sin tocar nada: el descriptor es un fichero (árbol en RAM o volumen montado) abierto para leer; el
   fichero y todas sus carpetas hasta `/` son de root, sin escritura de grupo ni de otros, en un sistema con dueños (si
   no, `ERRO_ACESSO` y el intérprete carga esas páginas como propias); el tramo está sin asignar y sin mapas viejos.
2. Se asignan páginas **privadas** del proceso en el tramo (de solo lectura para él, con NX salvo `MEM_PROT_EXEC`) y se
   lee el fichero en ellas por un búfer puente del kernel (protegido por un `trava_t`, a prueba de dueño muerto). Es lo
   único que duerme: si el proceso muere aquí, las páginas son suyas y se van con su espacio; no queda nada a medias.
3. Ya sin dormir (cerrojo general, D165): se busca un objeto con la misma clave (hash de la ruta, tamaño y fecha del
   fichero, desplazamiento, páginas y ejecución) y se **compara byte a byte** con lo leído. Si es igual, se mapean sus
   páginas (`PG_USUARIO | PG_EMPRESTADA`, sin `PG_ESCRITA`; NX si no es código), se sincroniza la TLB de las demás CPU y
   se liberan las privadas. Si la clave coincide pero el contenido no (fichero cambiado sin cambiar su fecha ni su
   tamaño), ese objeto se aparta (nadie más lo encuentra; los procesos que ya lo tienen siguen con él) y las páginas
   recién leídas pasan a ser un objeto nuevo (solo se marca `PG_EMPRESTADA`, sin pasar por P=0).

**Por qué la comparación:** con ella, una página compartida nunca tiene otro contenido que el que el proceso habría leído
él mismo del fichero: ni una biblioteca sustituida en el disco (aunque sea en el mismo segundo y con el mismo tamaño) ni
un fichero modificado a la vez se pueden «colar» a otro proceso. Cuesta lo mismo que leer el fichero (que ya se hacía) más
un `memcmp` por página; ahorra la RAM, no el tiempo.

**Cuenta de referencias:** una lista de mapas (espacio, dirección, páginas, páginas aún mapeadas, objeto); cada mapa es una
referencia. Se sueltan en `espaco_destruir` (cuando ninguna CPU usa ya el espacio: lo garantiza quien destruye, D165/D167)
y con `MEM_DEVOLVER` (quita solo las entradas con el marco del objeto, sincroniza la TLB y después baja la cuenta; un
mapa queda vivo mientras le quede alguna página). Con la cuenta a cero, el objeto y sus páginas se liberan enseguida (sin
LRU por ahora: un programa que se abre y cierra vuelve a leer, como antes). `PG_EMPRESTADA` hace que `espaco_destruir`,
`espaco_tirar` (`MEM_DEVOLVER`), `espaco_ajustar` (`MEM_PROTEGER`: no se puede dar escritura ni ejecución), la swap (D352)
y `MEM_SACAR` no las toquen. Los hilos comparten el espacio de su líder: la lista va por espacio, no por proceso.

**Swap:** las páginas compartidas no se sacan (son limpias y se podrían soltar y releer, pero no hace falta todavía). Las
privadas del tramo sí pueden salir mientras se lee el fichero: se vuelven a traducir después de cada lectura.

**PAE / sin PAE / memoria alta:** las páginas salen de `memfisica_alocar_usuario` (superior, alta o baja) y se tocan solo
por ventanas (`fisica_escrever`, `fisica_mapear`); las entradas se escriben con `espaco_mapear`/`pte_mudar`/`pte_esvaziar`,
que saben el tamaño de la entrada. **SMP:** todo con el cerrojo general; un hilo en otra CPU que toque el tramo justo
cuando cambia de marco ve un fallo de página que, con el cerrojo, encuentra la entrada presente y repite la instrucción
(`troca_trazer` devuelve `TROCA_PRESENTE`). **W^X:** las compartidas nunca tienen escritura (ni el kernel con `CR0.WP`,
ni `usuario_ok(..., true)`); las de datos llevan NX.

**Intérprete (`ligador.c`, solo en `ld-sagui`):** reserva las direcciones de la biblioteca, pide con `MEM_COMPARTIR` los
tramos compartibles (`pagina_compartivel`: mismo criterio que arriba; hasta 8 tramos por biblioteca), asigna como propias
las demás páginas y solo lee del fichero lo que cae en ellas. Una relocación dentro de un tramo compartido es un error
(con `-z text` no hay ninguna). Si el kernel no comparte (no es de confianza, kernel anterior, sin memoria), todo es
propio como en la fase 2. El `libdl` estático (programas estáticos con `dlopen`) no comparte: sus páginas salen del malloc.

**Cuentas:** `INFO_MEMORIA` añade `compartida_kb` (lo que ocupan, una vez), `compartida_mapeada_kb` (sumando todos los
procesos), `compartida_objetos` y `compartida_reutilizadas` (versión 5 de la estructura; los programas anteriores piden
72 bytes y no los reciben). `INFO_PROCESSOS` cuenta a cada proceso solo su parte de cada objeto (lo mapeado entre los
procesos que lo usan; uno que solo usa él se le cuenta entero): así la suma no las repite. `free` («Compartida»), `mem`, `infopc` y el Administrador de tareas («Bibliotecas compartidas») las muestran.

**Medido (h390, KVM, 2 GB):** el propio programa de prueba comparte 1032 KB en 9 objetos (libsagui 248 KB, medios
700 KB, libc 84 KB; tres tramos cada una). Con 8 hijos más (que también usan `libcomp.so`, 268 KB): lo compartido pasa a
1300 KB y no crece con los hijos; cada hijo gasta **1150 KB** de verdad en vez de **2450 KB** (−53 %; lo que queda es
sobre todo la BSS de 376 KB de libsagui_medios, la pila y el montón). El Administrador de tareas le cuenta 1216 KB (su parte de lo compartido incluida).

**Pruebas:** `tests/h390_paginas_compartidas.py` con `/apps/testes/compartidas` y `libcomp_a.so`/`libcomp_b.so` (iguales
salvo una letra): `MEM_COMPARTIR` directo (un fichero de `/tmp` no se comparte, nunca con escritura, no encima de páginas
asignadas, `MEM_SACAR` no las saca, `MEM_DEVOLVER` las quita); 8 hijos; sustitución en el disco con el mismo tamaño y la
misma fecha (el hijo nuevo ve la B, los viejos siguen con la A); escribir en texto y código compartidos (también tras
`MEM_PROTEGER`) mata al hijo; al terminar todo vuelve; 20 vueltas de 4 hijos sin fugas. Variantes: `smp` (4 CPU),
`pae6g` (páginas por encima de 4 GB), `sempae` (3 GB) y `presion` (512 MB: 640 MB del padre, la swap saca 160 MB de
privadas mientras 4 hijos comprueban sus páginas compartidas).

**Huecos entre segmentos:** la última página de un segmento compartido lleva lo que el fichero tenga después (p. ej. el
principio de `.data` en la de `.rodata`), no ceros como en la fase 2. Nadie lo lee (está fuera de todo segmento) y son
bytes que el proceso ya podía leer del fichero.

**Revisión independiente (2026-11-18) y lo corregido:**
- **B1 (bloqueante): alias escribible.** Con dos hilos, mientras uno dormía en `MEM_COMPARTIR` leyendo el fichero, el otro
  podía dar escritura a las páginas privadas del tramo con `MEM_PROTEGER` (dar permisos no espera a la barrera D351); el
  objeto nuevo se quedaba con esas entradas escribibles y, más tarde, otro proceso (también root) lo reutilizaba: código
  parcheado. Ahora cada entrada tiene que ser **exactamente** la puesta (presente, no en la swap, propia, sin escritura y
  con la NX pedida) después de cada lectura y otra vez antes de compartir; si no, `ERRO_INVALIDO` y el tramo queda sin
  asignar. El objeto nuevo reescribe sus entradas con sus permisos y sincroniza la TLB de todas las CPU antes de entrar
  en la lista. h390 (parte 7): un hilo pide escritura sin parar mientras otro comparte 150 veces; con `--smp` y sin caché
  de disco la carrera se da las 150 veces y las 150 se rechazan; ningún alias; un nieto reutiliza el objeto y lo ve igual
  al fichero.
- **I1: fijar memoria con cualquier fichero de root.** Ahora solo un ELF ET_DYN de i386 y solo páginas dentro de lo que el
  fichero guarda de un PT_LOAD sin escritura (redondeado a páginas); tope global de una cuarta parte de la RAM
  (`ERRO_LIMITE`, y `ld-sagui` carga esas páginas como propias); y `INFO_PROCESSOS` cobra a cada proceso su parte.
- **Segunda pasada:** el tope se vuelve a mirar al crear el objeto; y, si la llamada falla después de dormir, las páginas
  del tramo **no se quitan** (otro hilo podía estar dormido en una llamada con un puntero ya comprobado dentro: fallo de
  página del kernel, visto en h390 `smp`): quedan como propias normales (RW, NX) y quien llama las devuelve con
  `MEM_DEVOLVER`, que sí espera a la barrera D351 (`ld-sagui` lo hace siempre que hay error).
- Menores: `kernel/fs/arquivos.c` entero en UTF-8; búfer de lectura por llamada (sin cerrojo); mapas en listas por espacio
  (`MEM_DEVOLVER` y la destrucción solo miran las del suyo); los objetos se sueltan antes de liberar el directorio.

**Riesgos y límites que quedan:**
- Las páginas compartidas no se pueden sacar a la swap: un programa puede fijar como mucho una cuarta parte de la RAM con
  partes de solo lectura de `.so` de root (y se le cobran en el Administrador de tareas).
- Sin LRU: un objeto se libera con su último proceso; abrir y cerrar el mismo programa vuelve a leer la biblioteca.
- Los ejecutables y el intérprete (que carga el kernel con `vfs_ler_inteiro`) no se comparten todavía: el siguiente paso
  natural, con el mismo módulo, si `/bin` pasa a dinámico (fase 3b).
- La comprobación de confianza es por la ruta con la que se abrió el descriptor; como el contenido se compara, solo decide
  qué se comparte, no qué contenido ve un proceso.

## 16. Fase 3b: `/bin` dinámico salvo lo esencial (v8.5.5.3, hecha el 2026-11-22; D462)

- **Makefile:** `DINAMICO ?= 1`. La regla común de los programas enlaza `inicio.asm.o` y las `.so` del sistema en vez
  de los objetos de la libsagui, con `--as-needed`; `libc.so.1` solo si el programa enlazaba la libc común. `DINAMICO=0`
  vuelve a todo estático (plan de vuelta atrás). `BIN_ESTATICOS`: el initramfs entero, servicios, login, admin, su,
  passwd, instalar, reparar, arranque, kconfig, chmod, chown, chgrp, kill, terminal y sshd (el porqué, en D462).
  `tools/mkinitramfs.py` rechaza un ELF con `PT_INTERP`.
- **Dónde:** `libsagui.so.1`, `libsagui_medios.so.1` y `libc.so.1` pasan de `/apps/lib` a **`/lib`** (en la raíz
  SAGUI-RAIZ y en el ramdisk de siempre), como preveía §13; `/apps/lib` se queda para las de las aplicaciones
  (`SO_APPS`). La búsqueda (`/lib` y después `/apps/lib`) no cambia.
- **Base/medios:** widget, cron, raiz_live, planificador, sensores, regional, gui_componentes, pdf_escribir, des y
  casa_migrar pasan a `libsagui.so.1` (sus búferes grandes, al malloc), para que los programas siempre abiertos no
  carguen `libsagui_medios.so.1`.
- **Resultado (KVM):** 159 dinámicos y 34 estáticos en `/bin`; `/bin` 8,98 → 5,52 MB, `/apps/bin` 34,7 → 31,9 MB, el
  ramdisk de siempre 16,6 → 14,4 MB; arranque igual; +0,25 ms por lanzamiento; RAM de la sesión +0,5 MB en reposo y
  +3,0 MB con 8 programas de ventana distintos (lo privado de cada proceso dinámico, sobre todo intérprete y BSS de las
  `.so`, pesa más que lo compartido con pocos procesos de cada tipo: análisis y lo que lo bajaría, en D462).
- **Prueba:** `tests/h462_dinamico_3b.py` (+ `/testes/dinamico3b`): imagen de serie, ramdisk de siempre y vuelta atrás.

**Fase 3c (a futuro, fuera de la 3b por decisión del mantenedor):** ejecutables e intérprete compartidos en memoria. Hoy
el núcleo los lee enteros (`vfs_ler_inteiro`) y copia cada `PT_LOAD` a páginas propias del proceso; compartir sus partes
de solo lectura con `imagens.c` (como las `.so`) ahorraría unos 24 KB de código del intérprete y el código del
ejecutable por proceso. Toca el cargador y el núcleo de la memoria (riesgo alto): fase propia con revisión.
