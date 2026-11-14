# 094 - Carencias del sistema que reveló el port de NetSurf: libc, memoria, red, tiempo y teclado

- **Estado:** aprobada (reparto v7.1 / v7.2)
- **Prioridad:** alta
- **Alta:** 2026-11-12

## Problema / motivación

Al portar NetSurf 3.11 (D184) se vio qué espera un programa grande de C escrito para
POSIX y el SaguiOS todavía no da. La capa del port (`userland/terceros/netsurf/`) tuvo que
poner una parte entera de sistema: una libc (`libc/`), un fetcher propio y una superficie
de ventana. El inventario completo son **38 puntos numerados**, con quién los pide, qué
hay hoy y cómo se cubrió en el port. Están en [`docs/carencias-netsurf.md`](../docs/carencias-netsurf.md),
que llega con la v7.1 (D184).

Los puntos más graves, en resumen:

- **No hay una libc común.** Cada port trae la suya: MicroPython (`libc_sagui.c`), SQLite
  (`sqlite_libc.c`), OpenTyrian (`libc_ot.c`, 1.100 líneas), NetSurf (`libc/`) y ahora
  QuickJS y GNU Prolog. Lo mismo ocurre con la división de 64 bits (punto 9), copiada de
  port en port.
- **La memoria no vuelve al sistema.** `aloca`/`libera` es una lista con búsqueda desde el
  principio (O(n) por reserva) y lo liberado no vuelve al kernel hasta que el programa acaba.
- **La red bloquea la ventana.** El fetcher propio espera cada recurso y congela la
  interfaz; no hay `select`/`poll` ni descriptores sin bloqueo.
- **El tiempo y el teclado no dan lo necesario.** `tempo_ms` da la vuelta cada 49,7 días;
  `time_t` tiene 32 bits (2038); el teclado entrega el carácter ya traducido y no hay tecla
  soltada, así que ñ, á o ç no se pueden escribir.
- **Pila y constructores.** La pila de usuario es fija (64 KB) y `inicio.asm` no llama a los
  constructores globales (`.init_array`); el port se monta una pila en el heap y su propio
  guion de enlace.
- **Descriptores de compilación.** `CPATH` del entorno hace que una cabecera que falte se
  coja de la glibc del anfitrión sin aviso (punto 2).

## Propuesta

El trabajo se reparte en dos versiones, según lo aprobado por el mantenedor.

### v7.1 (pocas cosas)

| Punto | Qué | Estado |
| --- | --- | --- |
| 4 | Pila de usuario que crece bajo demanda, con página de guarda (D186) | hecha en la rama `v7.1-pila-dinamica`; **pendiente de mezclar** |
| 2 | Que las cabeceras del anfitrión no se cuelen en la compilación: `CPATH` (D191) | hecha en la rama `v7.0-cpath`; **pendiente de mezclar** |
| 10 | Constructores globales al arrancar: `.init_array` en `inicio.asm` y `programa.ld` | por hacer |
| 9 | Ayudantes de división de 64 bits (`__divdi3`, `__udivdi3`, `__moddi3`...) en la libsagui, una sola copia | por hacer |

Además, en la v7.1 van las publicaciones que ya tienen número: JClic (D185), NetSurf (D184),
la pila dinámica (D186), el reproductor MIDI (D190), `CPATH` (D191) y la partición
`SAGUI-APPS` de lectura y escritura en `/apps` para apps nuevas y no fundamentales (política
del mantenedor).

### v7.2 (el grueso, en cinco bloques)

Los bloques **A y B van primero y juntos**: la libc nueva trae su `malloc` y su `stdio`, y
cambiarlas por separado obliga a dos rondas de pruebas sobre los mismos ports. Después, **C,
D y E**, de dos en dos agentes como mucho.

- **A. Libc común en `userland/libc/`** (puntos 1, 5, 6, 7, 8, 11, 12, 14, 17, 18, 22, 33, 34,
  35, 36, 37): `stdio` con búfer, `printf` completo (`%f`, 64 bits), `strtod`, `errno`,
  `qsort`, `stat`, `opendir`, fechas, `getenv`, `atexit`, `realpath` y la libm de precisión
  simple. La base es la libc del port de NetSurf (`userland/terceros/netsurf/libc/`, rama
  `v7.1-netsurf`). Sustituye a las libc sueltas de OpenTyrian, SQLite, MicroPython, QuickJS y
  GNU Prolog.
- **B. Memoria** (puntos 3 y 13): `malloc` rápido por clases de tamaño que **devuelve la RAM
  al sistema** al liberar trozos grandes (hoy `userland/libsagui/memoria.c` solo crece), y
  mapas de memoria y de ficheros (`mmap`).
- **C. Red sin bloquear** (puntos 24, 25 y 21): `select`/`poll` y descriptores sin bloqueo, para
  que las descargas no congelen la ventana; sockets estilo BSD sobre la API de red actual; zlib
  por trozos (contenido `gzip`).
- **D. Tiempo** (puntos 15 y 16): reloj monótono de 64 bits, `time_t` de 64 bits (2038) y
  `nanosleep`.
- **E. Teclado y ventanas** (puntos 29 y 30): eventos con código de tecla y evento de tecla
  soltada, para poder escribir ñ y acentos en NetSurf; ventanas mayores de 1000×700.

Además, en la v7.2 van `qjs` (D188), GNU Prolog (D189) y el indicador de batería con uACPI
(D192).

### Después de la v7.2

- 19 y 20: `iconv` completo y regex POSIX con subexpresiones.
- 23 y 31: señales y `pthread` como capa de compatibilidad sobre la D167.
- 27 y 28: PNG con canal alfa y FreeType.
- 26 y 32: libcurl y `dlopen`. No hacen falta por ahora.
- 38: herramientas del anfitrión para generar código. Ya están sustituidas por
  `tools/netsurf_gerar.py`; no queda nada que hacer.

Los 38 puntos están asignados:

- **v7.1:** 2, 4, 9 y 10.
- **v7.2:** los 25 de los bloques A-E (A: 1, 5, 6, 7, 8, 11, 12, 14, 17, 18, 22, 33, 34, 35, 36,
  37; B: 3, 13; C: 21, 24, 25; D: 15, 16; E: 29, 30).
- **Después:** 19, 20, 23, 26, 27, 28, 31, 32 y 38.

## Criterios de aceptación

Por bloque. Cada uno se hace y se prueba por separado. Las pruebas concretas se escriben al
empezar cada bloque; aquí queda lo que debe comprobarse.

- [ ] **v7.1, punto 4 (pila):** un programa con recursión profunda sigue vivo al crecer la
      pila, y un desbordamiento real salta en la página de guarda (D186).
- [ ] **v7.1, punto 2 (`CPATH`):** compilar el userland con una cabecera que falta da error y
      no se coge de la glibc del anfitrión (D191).
- [ ] **v7.1, punto 10 (constructores):** un constructor global corre antes de `main` y su
      efecto se ve en el programa.
- [ ] **v7.1, punto 9 (división de 64 bits):** `uint64_t` dividido y resto dan el mismo
      resultado que el anfitrión; los ports usan la copia de la libsagui, no la suya.
- [x] **A (libc):** NetSurf compila y arranca con la libc común (`tests/h153_netsurf.py`), y
      OpenTyrian, SQLite, MicroPython, QuickJS y GNU Prolog siguen pasando sus pruebas con ella.
      *Rama `v7.2-libc-malloc` (D210):* hecho para NetSurf, OpenTyrian, SQLite y MicroPython.
      *Rama `v7.3-libc-qjs-prolog` (D210, puntos 4-6):* `strtod`/`strtof` con redondeo correcto y
      `%e/%f/%g` exactos en la libc (`numeros.c`, comparados con la glibc en
      `tests/h210_numeros_libc.py`), y QuickJS (`qjs`, Cipó) y GNU Prolog pasan a ella (h125, h155,
      h135, h128, h156). Ningún port tiene ya libc propia.
- [x] **B (memoria):** lo liberado en trozos grandes vuelve al sistema, y `mmap` de un fichero
      da el mismo contenido que leerlo. *Rama `v7.2-libc-malloc` (D209), prueba `tests/h170_malloc.py`.*
- [ ] **C (red):** `select`/`poll` sobre varios sockets devuelve sin bloquear la ventana, y el
      fetcher de NetSurf descarga por trozos (`tests/h153_netsurf.py`).
- [ ] **D (tiempo):** el reloj monótono sigue creciendo más allá de 49,7 días, y `time_t` de
      64 bits da fechas posteriores a 2038.
- [ ] **E (teclado y ventanas):** ñ y á se escriben en NetSurf, y una ventana mayor de
      1000×700 se abre sin recortarse.
- [ ] Decisiones registradas en `docs/decisoes.md` para cada bloque (si aplica).

## Notas

- **Riesgos:**
  - Cambiar la libc rompe los ports que la usan: por eso se hace bloque a bloque, con la
    prueba de cada port delante.
  - `time_t` de 64 bits y el evento de teclado nuevo cambian la ABI: los programas ya
    compilados hay que volver a compilarlos.
  - La D186 y la D191 están en ramas sin mezclar: si cambian antes de mezclarse, el plan se
    revisa.
- Ejecución: como mucho dos agentes compilando o probando a la vez, y la batería completa no se
  lanza con agentes trabajando (AGENTS.md).
- La libc de NetSurf se escribió para ser general, no para NetSurf: es el punto de partida del
  bloque A, no un parche del port.
