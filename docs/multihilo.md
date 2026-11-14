# Multihilo y multinúcleo: lo que hay y lo que falta

Este documento recoge cómo usa el SaguiOS varios procesadores y varios hilos
dentro de un programa: qué está hecho, cómo se usa desde C y qué le falta a
Python.

## El estado de hoy (D167, D170, D174)

| Qué | Estado | Dónde |
| --- | --- | --- |
| Procesos | **Hecho**: un programa arranca otro con `executar()` y lo espera con `esperar()`; cada hilo listo recibe rebanadas de 20 ms | D047, `kernel/nucleo/processos.c` |
| Planificadores | **Cuatro, seleccionables (D174)**: `rr` (por defecto), `afinidad`, `equilibrio` y `prioridad`; al arrancar (`mkimagem.py --planificador`) o en caliente (`planificador NOMBRE`); máscara de CPU por proceso (`afinidad PID MASCARA`) y nice (`planificador nice PID N`); topología SMT por CPUID | D174, `kernel/nucleo/planificadores.c` |
| Procesadores | **Varios, opcional (D157, D164)**: con `mkimagem.py --smp` el kernel enciende los que anuncia la MADT; cada CPU tiene su GDT, TSS, GS, pila, temporizador y planificador. Sin la opción, uno | D157, D164 |
| Kernel en varias CPU | **Cerrojo general (D165)**: cada CPU atiende las llamadas al sistema de sus programas, una CPU dentro del kernel a la vez | D165, `kernel/nucleo/trava_geral.c` |
| Hilos de usuario | **Hechos (D167)**: un proceso tiene varios hilos que comparten la memoria, los ficheros, la carpeta y las ventanas, y corren a la vez en varias CPU | D167, `kernel/nucleo/hilos.c`, `userland/libsagui/hilos.c` |
| Mutex y semáforo | **Hechos y bloqueantes**: `mutex_t`, `semaforo_t`; quien espera duerme en el kernel (futex) en vez de girar | D150, D167, `userland/libsagui/sincronia.c` |
| Atómicas | **Hechas**: `atomico_cas32`, `atomico_add32`, `atomico_troca32`… | D150 |
| C de la libsagui | **Hecho**: la API de abajo en `sagui.h` | D167 |
| C del `cc` del sistema | **Hecho**: la misma API en su biblioteca; `/ejemplos/hilos.c` | D167 |
| Python | **Hecho con GIL (D170)**: `_thread` y un `threading` mínimo; los hilos se turnan para ejecutar Python y sueltan el GIL al esperar | D170, `userland/micropython/mpthreadport.c`, `/lib/python/threading.py` |
| Pascal | **Hecho (D171)**: `uses Hilos` | D171, `userland/programas/pascal_biblioteca.h` |

## La API en C

En `sagui.h` (libsagui) y en la biblioteca del `cc` del sistema:

```c
typedef struct thread thread_t;
thread_t *thread_create(void *(*funcion)(void *), void *arg, unsigned pila);  /* pila <= 960 KB (o 0): la del kernel, con guarda (D186) */
int        thread_join(thread_t *t);     /* espera que acabe; devuelve (int) lo que devolvió */
void       thread_yield(void);           /* cede el turno */
void       thread_sair(void);            /* acaba este hilo (en el principal, el proceso) */
int        thread_id(void);              /* el número de este hilo (tid) */
```

Para coordinar hilos, lo de la D150, ahora con espera en el kernel:

```c
mutex_t m;      semaforo_t s;
mutex_criar(&m);        mutex_tomar(&m);      mutex_largar(&m);     mutex_tentar(&m);
semaforo_criar(&s, 0);  semaforo_tomar(&s);   semaforo_sinalizar(&s);
```

Reglas que conviene saber:

- `salir()` en cualquier hilo, o un fallo en cualquier hilo, terminan el
  proceso entero con todos sus hilos.
- `meu_pid()` da el pid del proceso en todos los hilos; `thread_id()`, el del
  hilo.
- `aloca`/`libera` (y `malloc`/`free` del `cc`) son seguros entre hilos.
- No hay variables por hilo (TLS): la libsagui no las necesita (no tiene
  `errno`).
- La pila de un hilo (D186) la da el kernel: 960 KB de un hueco de 1 MB que
  se asignan al tocarlos, con 64 KB de guarda debajo; un desbordamiento
  termina el proceso con «Desbordamiento de pila (hilo N)» en vez de pisar el
  heap. Si se piden más de 960 KB, la pila sale del `malloc` (sin guarda). La
  del principal tiene 8 MB, también bajo demanda y con guarda. La biblioteca
  del `cc` hace lo mismo; la del Pascal sigue con 64 KB de `GetMem`.

Por debajo están las llamadas al sistema 120-126 (`hilo_criar`,
`hilo_sair`, `hilo_esperar`, `hilo_ceder`, `futex_esperar`,
`futex_acordar`, `thread_id`); ver la D167.

## Lo que necesita cada lenguaje

### C

Hecho. El ejemplo `rootfs/ejemplos/hilos.c`:

```
cc /ejemplos/hilos.c -o /tmp/hilos
/tmp/hilos 4
```

### Python

Hecho (D170). El MicroPython del sistema tiene `_thread` y, encima, un
`threading` mínimo en `/lib/python/threading.py` (`Thread`, `Lock`,
`RLock`, `Event`, `current_thread`):

```python
import threading
cerrojo = threading.Lock()
def trabajo(n):
    with cerrojo:
        print("hilo", n)
hs = [threading.Thread(target=trabajo, args=(i,)) for i in range(4)]
for h in hs: h.start()
for h in hs: h.join()
```

Lo que conviene saber:

- **Hay un GIL**: un solo hilo ejecuta Python a la vez. Los hilos sirven
  para esperar (`time.sleep`, `socket`, `input()`, `os.system`, un `Lock`)
  mientras otro trabaja; **el cálculo puro no va más rápido con varias CPU**
  (para eso, varios procesos o C). El intérprete suelta el GIL cada 1024
  saltos hacia atrás, por turnos (nadie se queda sin entrar).
- Al acabar el programa principal acaba el proceso con todos sus hilos
  (como en MicroPython; CPython esperaría a los que no son *daemon*): use
  `join()`.
- La pila de cada hilo es de 64 KB del heap (sin guarda: el estado del hilo
  se halla por la pila y el recolector la recorre entera, D186);
  `_thread.stack_size()` no la cambia. El principal llega a 1 MB de pila
  (8 MB bajo demanda, D186).
- Los ficheros normales y el TLS (`https`) se leen y escriben con el GIL
  cogido; los `socket` sin TLS y la entrada estándar, sin él.

`/ejemplos/hilos.py 4` es el ejemplo de `hilos.c` en Python; la prueba es
`tests/h141_python_hilos.py`.

### Pascal

Hecho (D171). Unidad `Hilos` (`uses Hilos`):

```pascal
procedure Trabajo(arg: Pointer);          { un procedimiento de primer nivel, un parametro }
h := CrearHilo(@Trabajo, Pointer(3));      { THilo; arg se recupera con integer(arg) }
codigo := EsperarHilo(h);                  { lo que dio SalirHilo (0 si acabo sin mas) }
CederTurno;  SalirHilo(codigo);  id := IdHilo;
var m: TMutex;      CrearMutex(m);     TomarMutex(m);     SoltarMutex(m);     IntentarMutex(m): boolean
var s: TSemaforo;   CrearSemaforo(s, 0); TomarSemaforo(s);  SoltarSemaforo(s);  IntentarSemaforo(s): boolean
```

- `@Rutina` es una extension pequena del compilador: da la direccion de un
  procedimiento (`Pointer`); solo de primer nivel, sin resultado, con un
  parametro, y definido antes.
- `New`/`Dispose`/`GetMem`/`FreeMem` y cada `write`/`writeln` (una traba
  recursiva en la salida) son seguros entre hilos; hasta crear el primer hilo
  las trabas no hacen nada (los programas sin hilos no cambian).
- `read` y los archivos compartidos por varios hilos los coordina el
  programa (con un `TMutex`).
- `Halt` o un error de ejecucion en un hilo terminan el proceso entero.
- La pila de cada hilo (64 KB) sale de `GetMem`; no hay pagina de guarda.

## Los planificadores (D174)

`escalonar()` (`kernel/nucleo/processos.c`) hace el cambio de proceso: el
cuadro, el CR3, la FPU, `na_cpu`, los zombis. **Qué** proceso toma cada CPU
lo decide la política activa (`kernel/nucleo/planificadores.c`). Las cuatro
miran la misma tabla de procesos: una "cola" de una CPU no es una lista,
sino los procesos listos cuya `casa` es esa CPU. Así ningún sitio del kernel
que cambia el estado de un proceso (bloquear, despertar, el reloj, un futex,
terminar) tiene que acordarse de meterlo o sacarlo de una lista.

| Planificador | Cómo elige | Para qué |
| --- | --- | --- |
| `rr` | Cada CPU recorre la tabla desde el último que eligió y toma el primero listo | El de siempre y el de por defecto: reparte bien el tiempo, pero un proceso salta de núcleo cada vez que le toca |
| `afinidad` | Cada CPU toma primero los suyos (los que corrieron en ella la última vez) y los que aún no tienen CPU; si no tiene ninguno, roba el que lleva más tiempo sin correr, pero solo si lleva 3 tics (30 ms) parado: antes su caché sigue caliente en la otra CPU | Cálculo largo: casi no migra |
| `equilibrio` | Una cola por CPU; el proceso nuevo va a la CPU con menos procesos; cada 50 ms la CPU 0 pasa uno listo de la más cargada a la menos cargada si se llevan 2 o más; una CPU sin nada roba enseguida de la cola más cargada | Muchos procesos que empiezan y acaban: la carga queda repartida |
| `prioridad` | Gana el de menor `nice*2 + uso/2 - espera/2`; el *uso* es la CPU que gastó hace poco (se suma por tic y se divide entre dos cada 250 ms) y la *espera*, los tics que lleva listo sin correr (hasta 1,2 s, para que nadie se quede sin turno). Al despertar, si todas las CPU están ocupadas y una corre algo con 4 puntos más, se le pide cambiar ya (IPI). El nice cambia la rebanada: -10 o menos, 40 ms; negativo, 30 ms; 0 a 9, 20 ms; 10 o más, 10 ms | El escritorio y los programas que esperan teclado, red o un temporizador pasan delante del cálculo de fondo |

**Interactivos.** Un proceso que se bloquea antes de gastar la rebanada (el
terminal, el escritorio, uno que duerme 10 ms) apenas acumula *uso*, así
que en `prioridad` tiene mejor número que los que calculan y, al despertar,
les quita la CPU. Con KVM, cuatro procesos calculando en cuatro CPU y uno
que duerme 10 ms en bucle, la espera de despertar a correr es ~20 µs con
`prioridad`, ~2 ms con `rr` (espera a que a alguna CPU se le acabe la
rebanada) y ~9 ms con `afinidad` (espera a SU CPU).

**Topología (Hyper-Threading/SMT).** Cada CPU lee con CPUID qué núcleo
físico y qué paquete es (hoja 0xB; en AMD, la 0x8000001E; en Intel antiguo,
las hojas 1 y 4; sin nada de eso, cada CPU es su propio núcleo):
`cpu_nucleo_fisico(cpu)` en `kernel/nucleo/cpu_dados.c`. `afinidad` y
`equilibrio` no ponen un proceso en el hilo hermano de un núcleo ocupado si
hay un núcleo entero ocioso: lo pasan a la cola de ese núcleo y lo
despiertan. El Administrador de tareas (`tareas -t`) y el `infopc` muestran
el núcleo y el hilo de cada CPU.

**Máscara de CPU.** `afinidad PID 0x5` (o `afinidad PID -c 0,2`) deja al
proceso, con todos sus hilos, solo en las CPU 0 y 2; 0 = todas. La heredan
los hijos y los hilos nuevos. Una máscara sin ninguna CPU que trabaje se
rechaza. Si el proceso está corriendo en una CPU que ya no puede usar, esa
CPU cambia de proceso enseguida.

**Elegirlo.**

- Al arrancar: `tools/mkimagem.py --planificador afinidad` (bits 9-11 de
  las opciones de arranque). Sin la opción, `rr`.
- En caliente: `planificador` (lista y marca el activo), `planificador
  equilibrio`. Desde C: `planificador_escolher(PLANIFICADOR_PRIORIDAD)`.
- Por proceso: `afinidad PID MASCARA`, `planificador nice PID N`,
  `planificador -p` (CPU, máscara, nice, migraciones y prioridad de cada
  uno); desde C, `afinidade_cpu`, `prioridade_processo`,
  `planificacion_info`.
- El log dice cuál está activo (`[SAGUI] planificador: ...`).

Las medidas comparadas están en la D174 (`tests/h144_planificadores.py`).

## Lo que falta en el kernel

1. **Cerrojos por subsistema**: hoy hay un cerrojo general (D165); los hilos
   que hacen muchas llamadas al sistema esperan unos por otros.
2. **IRQ repartidas**: el IOAPIC (D166) las tiene listas, todas van a la
   CPU 0.
3. **Más entradas**: `MAX_PROCESSOS` es 64 y cada hilo ocupa una.
4. **Referencias en todas las llamadas sobre descriptores** que duermen
   dentro del kernel (hoy solo leer y escribir; ver la D167).

## Los ejemplos

- `rootfs/ejemplos/hilos.c` (hecho): la misma cuenta repartida en N hilos,
  con un mutex para el total.
- `rootfs/ejemplos/hilos.py` (hecho, D170): lo mismo en Python, y un hilo
  que espera mientras el principal calcula.
- `rootfs/ejemplos/hilos.pas` (hecho, D171): lo mismo en Pascal
  (`pascal /ejemplos/hilos.pas -o /tmp/hilos; /tmp/hilos 4`).

## Cómo se mide la ganancia

- `run_in_qemu.sh --nucleos N` arranca la máquina con N procesadores (QEMU
  `-smp N`; hace falta además la imagen con `--smp`), y el arnés de las
  pruebas tiene el parámetro `nucleos`.
- `/testes/bench N c` mide el cálculo en N procesos; `/testes/bench N h`, el
  mismo cálculo en N hilos de un proceso.
- Con KVM y cuatro CPU, `bench 4 h` gana **3,77x** (la h140 exige 1,5x) y
  `bench 4 c` entre 2,66x y 3,20x (h133).

## Resumen

| | |
| --- | --- |
| Hecho | Varias CPU (D164, opcional), kernel en todas con cerrojo general (D165, por turnos en hardware real: D169), hilos de usuario con mutex y semáforos bloqueantes (D167), IRQ repartidas (D168), la API en C (libsagui y `cc`), `_thread` y `threading` en Python con GIL (D170), `uses Hilos` en Pascal (D171), cuatro planificadores con máscara de CPU, nice y topología SMT (D174) |
| Falta | Cerrojos finos en el kernel; el cálculo en Python no escala con las CPU por el GIL |
| Ejemplo | `rootfs/ejemplos/hilos.c`, `hilos.py` y `hilos.pas` |
