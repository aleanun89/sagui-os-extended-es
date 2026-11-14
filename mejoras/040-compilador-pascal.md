# 040 - Compilador de Pascal dentro de SaguiOS

- **Estado:** hecha (v5), con dos limitaciones de la unidad `crt` pendientes del kernel/terminal; optimizaciones del generador común hechas en la v5.1 (D108)
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

Pascal es el lenguaje con el que se aprendió a programar en los PC de los 80 y 90 (Turbo
Pascal). Con `cc` ya hay un compilador propio; uno de Pascal convierte SaguiOS en un entorno
de aprendizaje completo y aprovecha el generador de código x86 que ya existe.

## Propuesta

`pascal programa.pas [-o nombre] [-x87 | -sse2]`: un compilador propio de una pasada (a la
manera de Wirth) que genera directamente un ELF i386, como `cc`, sin ensamblador ni enlazador.
El generador x86 y la escritura del ELF se extraen de `cc` a cabeceras comunes
(`userland/programas/gerador_x86.h`, `elf_saida.h`) sin que `cc` cambie un solo byte. La
biblioteca (write/read, cadenas, conjuntos, archivos, memoria, `crt` y la unidad gráfica
`sagui`) está escrita en el propio Pascal y solo entra lo que el programa usa.

## Criterios de aceptación

- [x] Compila los ejemplos de `/ejemplos/pascal` (hola, primos, fibonacci, cadenas,
  conjuntos, ordenar, lista, archivos, colores, fractal) y su salida es la esperada
- [x] Tipos ordinales, `real` (SSE2 o x87), string corta, arrays, records con variante,
  conjuntos, punteros, archivos de texto y con tipo, procedimientos anidados
- [x] Errores de compilación con `archivo:línea:columna`; `{$R+}` con error 201 y línea
- [x] Unidad `crt` **solo en espacio de usuario** (sin tocar kernel ni terminal, D087)
- [x] Unidad `sagui`: gráficos en una ventana (`fractal.pas`)
- [x] `cc` genera exactamente los mismos binarios que antes de la extracción (7 programas x
  SSE2/x87, comparados con `sha256sum` dentro de SaguiOS: idénticos)
- [x] El hola mundo ocupa menos de 4 KB (2.447 bytes)
- [x] Test automático: `tests/h69_pascal.py` (también con `-cpu pentium3`, x87)
- [x] Mico: F5 en un `.pas` compila con `pascal` y ejecuta; los errores van a Problemas
- [x] Decisiones D085-D087; manual en `/ayuda/pascal.md`; `ajuda pascal`

## Pendiente

- **Colores en el terminal** (`textcolor`/`textbackground`): el terminal (consola gráfica y
  terminal en ventana) solo entiende `ESC [ C/D/K/J`. Hace falta que entienda `ESC [ n m`;
  mientras tanto la biblioteca solo guarda `textattr`.
- **`keypressed` en el terminal**: `disponivel()` no responde para el terminal ni para el
  esclavo del terminal virtual; hoy `keypressed` solo funciona con la entrada en un pipe y en
  el terminal devuelve siempre `false` (un `repeat ... until keypressed` no termina). Hace
  falta que el kernel responda `disponivel(0)` en modo bruto.
- Con esas dos piezas en el terminal, `gotoxy` podría usar `ESC [ l ; c H` en vez de la
  pantalla virtual (que redibuja la pantalla entera cuando el programa espera).
- Resaltado de sintaxis de Pascal en Mico (F5 ya compila y ejecuta los `.pas`).
- Objetos, `unit` propias, `ansistring`, sobrecarga.

## Notas

El trabajo empezó con un WIP que añadía al kernel las secuencias `ESC [ l;c H`, `ESC [ n m`
y `disponivel()` del terminal; se deshizo para no tocar el kernel en esta mejora (D087).

## Optimizaciones del generador común (v5.1, D108, test h80)

Rama `generador-opt`. Mirilla, operandos inmediatos, comparar y saltar en `&&`/`||` y reducción
de fuerza, con `-O0`/`-O1` y `-fno-mirilla|inmediatos|saltos|reduccion` en `cc` y `pascal`.
`-O0` reproduce los binarios anteriores byte a byte. Cifras (h80, dentro de SaguiOS con KVM,
tamaño del ejecutable `-O0` -> `-O1`):

| | -O0 | -O1 | |
| --- | ---: | ---: | ---: |
| `cc` (6 programas) | 5305 a 18185 | 3563 a 10975 | -28 a -40 % |
| Pascal (11 programas) | 2447 a 12975 | 2350 a 11812 | -3 a -17 % |
| total (17 programas) | 140846 | 114294 | -18,9 % |
| bucle de enteros `cc` (ms) | 170 | 150 | -12 % |
| bucle de enteros Pascal (ms) | 260 | 250 | -4 % |

Detalle por grupo y límites en la decisión D108. Pendiente: saltos a saltos, variables en
registros (v6).
