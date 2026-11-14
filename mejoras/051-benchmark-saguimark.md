# 051 - SaguiMark: benchmark de CPU y FPU con puntuación en infopc

- **Estado:** hecha en QEMU (D093, D094; grupo Gráficos 2D en D102) con referencia provisional; falta medir el HP dv7 (referencia definitiva) y los demás PC
- **Prioridad:** media
- **Alta:** 2026-11-07
- **Relacionadas:** 014 (infopc), 015 (SSE2, `bench`), 050 (frecuencias), 017 (SMP), 036 (PC antiguos)

## Problema / motivación

Hay un `bench` para comparar rutas SSE2 de gráficos, pero no una medida global y comparable
entre máquinas. Para el objetivo de «resucitar» PC de distintas épocas (Pentium III, Core
Duo, Athlon, Ryzen...) sirve una **puntuación**: cuánto rinde cada PC con SaguiOS, y si una
mejora del sistema lo acelera.

## Propuesta

Nueva pestaña **«Benchmark»** en `infopc` y comando `saguimark` (el mismo motor, sin
ventana, para las pruebas y para usarlo por terminal).

### Pruebas (diseño propio, cargas deterministas con resultado comprobable)

| Grupo | Prueba | Qué mide |
| --- | --- | --- |
| **Enteros** | criba de primos; ordenación (quicksort/mergesort) de millones de enteros; compresión y descompresión LZ propia de un texto generado; SHA-256 (la del kernel) sobre un búfer; búsqueda en árbol o tabla hash | ALU, saltos y predicción, caché |
| **FPU x87** | Mandelbrot en doble precisión; integración numérica (sin/exp/sqrt); multiplicación de matrices pequeñas | coma flotante clásica (todo PC desde el 486) |
| **SSE/SSE2** | las mismas cargas vectorizadas (Mandelbrot 2 píxeles por instrucción, matrices, FFT en float) | SIMD; se salta (puntuación «n/d») sin SSE2 |
| **Memoria** | ancho de banda (copia, escritura, lectura de bloques grandes) y **latencia** (recorrido de lista enlazada aleatoria) | RAM y cachés |
| **Sistema** (opcional) | cambio de contexto entre dos procesos; llamada al sistema vacía; `cc` compilando un programa fijo | el propio SaguiOS |

### Medida

- Tiempo con **RDTSC**, convertido con la frecuencia medida del TSC, y comprobado con el reloj
  del sistema.
- Cada prueba:
  - un calentamiento;
  - varias repeticiones;
  - se toma la **mediana**;
  - se muestra la **variación** (±%).
- Duración total configurable: «rápida» ~15 s y «precisa» ~60-120 s (la del botón por defecto).
- Durante la medida, la ventana deja de redibujarse salvo una barra de progreso (la de la
  042, si existe) para no medir el escritorio. Se recomienda cerrar otros programas.
- Cada prueba verifica su resultado (suma o hash conocidos): una CPU o FPU que calcula mal da
  **«ERROR»**, no una puntuación. Así detecta también fallos de hardware o de
  *overclock*.
- Un solo núcleo hasta tener SMP (017); después, puntuación mono y multinúcleo.

### Puntuación

- Por prueba: `referencia_tiempo / tiempo × 1000`.
- Por grupo (Enteros, FPU, SIMD, Memoria): **media geométrica** de sus pruebas.
- **Total**: media geométrica de los grupos. Si un grupo no está disponible (sin SSE2), se
  indica y se calcula el total sin él, marcado.
- **Máquina de referencia = 1000 puntos:** el **HP dv7 (Athlon X2 QL-64)**, el primer PC real
  donde se mida. Hasta medirlo, una referencia provisional fijada en QEMU/KVM (documentada) que
  se sustituye después. La versión del benchmark va en el resultado: cambiar las pruebas
  cambia la versión, y solo se comparan puntuaciones de la misma versión.

### Resultados

- Pantalla:
  - puntuación total y por grupo;
  - tabla por prueba (tiempo, puntos, ±%);
  - comparación con **máquinas de referencia** guardadas: los PC probados
    (G470, dv7, Amilo, Ryzen B450...) en `/usr/share/saguimark/referencias.txt` (o
    `/ayuda`), como barras.
- Guardar el resultado en `/home/saguimark-AAAAMMDD-HHMM.txt` (CPU, frecuencia, versión del
  benchmark, todas las cifras) para traerlo y añadirlo a `docs/compatibilidad.md` y a la
  lista de referencias.
- El informe de compatibilidad (`infopc --informe`, 036) puede incluir el último resultado.

## Pruebas automáticas

- `saguimark --rapido` en QEMU:
  - todas las pruebas terminan;
  - los resultados de comprobación son correctos;
  - la puntuación es estable entre dos ejecuciones con KVM (±10 %);
  - sin SSE2 (`-cpu pentium3`), el grupo SIMD sale «n/d» y el resto funciona.
- La pestaña de infopc, con captura.

## Criterios de aceptación

- [x] Pestaña Benchmark en infopc y comando `saguimark`, con modos rápido y preciso
- [x] Grupos Enteros, FPU x87, SIMD y Memoria con resultados verificados
- [x] Puntuación con referencia documentada y versión del benchmark (referencia **provisional**)
- [x] Resultados guardados en `/home` y comparación con las máquinas probadas
- [x] Test automático en `tests/` (`h72_saguimark.py`)
- [x] Decisión registrada en `docs/decisoes.md` (D093, D094)

## Lo hecho (2026-11-07)

- **Motor** en `userland/libsagui/saguimark.c` (el mismo para `saguimark` y la pestaña de
  infopc), compilado sin `-mgeneral-regs-only`; el SSE2 solo en funciones con
  `__attribute__((target("sse2,fpmath=sse")))`, llamadas tras el CPUID (D076).
- **17 pruebas** propias con resultado verificado:
  - *Enteros:* criba, quicksort, LZ, SHA-256 y tabla hash;
  - *FPU x87:* Mandelbrot, integral (`fsin`, `f2xm1`, `fsqrt`) y matrices;
  - *SIMD (SSE2):* Mandelbrot de 2 píxeles, matrices y un filtro FIR de 4 muestras (en lugar
    de la FFT: con valores enteros el resultado es exacto);
  - *Memoria:* copia, escritura, lectura y latencia;
  - *Sistema:* llamada vacía y tubería, fuera del total.
  La FPU trabaja con 53 bits, así que x87 y SSE2 dan el mismo resultado; un fallo da «ERROR».
- **Medida:** RDTSC, un calentamiento, mediana de 3 (`--rapido`) o 9 (`--preciso`)
  repeticiones, ±%, comprobación con el reloj del sistema, media geométrica por grupo y total.
- **Referencia provisional** (D093): QEMU 8.2 con KVM en el PC de desarrollo = 1000 (los tiempos
  están en `provas[]`). La definitiva será el HP dv7 = 1000 cuando se mida. Versión del
  benchmark: 1.
- **Resultados** en `/home/saguimark-AAAAMMDD-HHMM.txt` (o `/tmp`), con la línea para
  `/lib/saguimark/referencias.txt`; comparación con barras; `infopc --informe` incluye el último.
- **infopc:** pestaña «Benchmark» con «Medir (rápido)» y «Medir (preciso)» (teclas R y P) y la
  barra de progreso entre pruebas.
- **Pruebas:** `tests/h72_saguimark.py` (todas verificadas; estabilidad con KVM; `-cpu
  pentium3` con SIMD «n/d»; la pestaña con capturas).
  Con KVM: rápido en ~3 s; dos ejecuciones del modo preciso dieron 990 y 964 (2,6 %). Sin KVM
  (TCG): rápido en ~40 s, total ~70.

### Pendiente

- Medir el **HP dv7** en modo preciso y pasar sus tiempos a `provas[]` (dv7 = 1000); medir el
  G470, el Amilo y el Ryzen B450 y añadirlos a `referencias.txt` y a `docs/compatibilidad.md`.
- Puntuación multinúcleo cuando haya SMP (017).
- El `cc` compilando un programa fijo en el grupo Sistema (opcional).

## Grupo «Gráficos 2D» (versión 2 del benchmark, D102)

Mide la velocidad 2D **real**, la que ve el usuario, con las mismas funciones de la libsagui
que usan los programas. El grupo abre una **ventana de 128x128** («SaguiMark 2D») y la lleva
a la esquina de abajo a la izquierda (nueva llamada `SIS_MOVER_JANELA`, 100) para no tapar la
ventana de infopc; se queda 1,5 s parada, con una figura, antes de medir, y se cierra al
acabar. Entra en el total (media geométrica de cinco grupos).

| Prueba | Qué hace | Qué mide | Resultado |
| --- | --- | --- | --- |
| Líneas (búfer) | 200 000 líneas pseudoaleatorias (semilla fija, algunas salen de la ventana: recorte) con `g_linha`, sin enviarlas a la pantalla | dibujo puro en CPU | líneas/s |
| Líneas a la pantalla | 12 000 líneas, con `jan_mostrar` cada 10 (1200 cuadros) | tubería completa: llamada al sistema, compositor, copia al framebuffer con write-combining (D057), conversión a 16 bits (D078) | cuadros/s y líneas/s |
| Rectángulos (relleno) | 60 000 `g_retangulo` de tamaño y color aleatorios (96 millones de píxeles) | *fill rate* | rectángulos/s y píxeles/s |
| Copia de bloques | 20 000 `g_copiar_area` de bloques de 32-95 píxeles (80 millones de píxeles; se solapan) | *blit* en memoria | copias/s y píxeles/s |

- **Verificación:** al acabar cada prueba, una suma FNV de los 16384 píxeles del búfer se compara
  con el valor esperado. Los esperados salen de una implementación independiente en Python
  (`tests/saguimark_ref2d.py`), que el test h72 vuelve a ejecutar y compara con lo que dio
  SaguiOS. Si no coincide, «ERROR» (sin puntos).
- **Sin escritorio** (terminal de pantalla completa) o sin poder abrir la ventana: las cuatro
  pruebas y el grupo salen «n/d» (motivo «sin escritorio») y el total queda **parcial**.
- **Opciones:** `saguimark --solo 2d` (solo este grupo) y `--sin-2d` (todo menos él, sin
  ventana). Pestaña Benchmark de infopc: lo incluye siempre que haya escritorio (infopc corre
  en él).
- **Qué se cuenta:** además de los puntos (referencia / tiempo), el resultado lista las tasas
  por segundo (sección «Gráficos 2D» de la tabla y del archivo guardado).
- **Versión 2:** el total cambia, así que `SM_VERSAO` pasa a 2; el texto del resultado avisa de
  que las puntuaciones de versiones distintas **no se comparan**, `referencias.txt` solo usa las
  líneas de la misma versión y su formato añade la columna `2d`.
- **Referencia provisional (1000 por prueba):** mediana de tres ejecuciones del modo preciso en
  QEMU 8.2 con KVM (`-cpu qemu32`, pantalla de 1024x768, PC de desarrollo): líneas 23,0 ms,
  líneas a la pantalla 40,0 ms, rectángulos 16,3 ms, bloques 13,7 ms. Cifras: 8,6 millones de
  líneas/s en el búfer; ~30 000 cuadros/s (300 000 líneas/s) hasta la pantalla; ~6 000 millones de
  píxeles/s de relleno; 1,4-2,0 millones de copias/s. Sin KVM (TCG): 1,3 millones de líneas/s,
  15 300 cuadros/s, 0,5 millones de rectángulos/s y 140 000 copias/s (grupo ~165 puntos; total ~76).
  Cuando se mida el HP dv7 se rehará toda la referencia (dv7 = 1000).
- La copia de bloques de una ventana a sí misma entra en la libsagui como `g_copiar_area`.
- **Pruebas:** `tests/h72_saguimark.py` ampliado (21 pruebas; en la consola 2D «n/d» y total
  parcial; en el escritorio la ventana de 128x128 en la esquina, captura `H72-saguimark-2d`, y
  las sumas contra Python; `H72-infopc-2d` con infopc).

## Notas

- Las pruebas son de diseño propio: no reproducen benchmarks comerciales ni sus
  puntuaciones, y la escala (1000 = HP dv7) es propia de SaguiOS.
- En máquinas virtuales sin KVM (TCG) la puntuación no es representativa; se indica.
