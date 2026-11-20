# Laboratorio de C

El **Laboratorio de C** (menú Inicio > Educación > Laboratorio de C, junto a Jabuti) es un sitio para aprender C: escribes un
programa, pulsas **Ejecutar (F5)** y ves lo que hace. Compila con TCC (`tcc.md`) dentro del propio programa.

## Cómo se usa

1. Elige un **ejemplo** en la lista de arriba a la derecha (Hola mundo, Bucles, Arrays, Punteros, Estructuras,
   Ficheros, Entrada de teclado, Una ventana, Error de límites, Errores de compilación) o escribe el tuyo.
2. Pulsa **Ejecutar (F5)**. La salida del programa sale en la pestaña **Salida**, abajo.
3. Si el programa pide datos (`fgets(..., stdin)`), escríbelos en el cuadro **Entrada** y pulsa Intro.
4. **Detener (F6 o Esc)** para un programa que no termina (un bucle infinito no cuelga el Laboratorio: el programa
   corre en un proceso aparte).

## Errores

Si el programa no compila, se abre la pestaña **Errores** con cada mensaje y su línea. Un **clic** en el mensaje lleva
el cursor a esa línea (y las líneas con error llevan una marca roja en el margen). El ejemplo «Errores de
compilación» sirve para probarlo. Los avisos (`warning`) no impiden ejecutar.

## Comprobar límites (-b)

Con la casilla **Comprobar límites (-b)** el programa se compila con comprobación de memoria: escribir fuera de un
array o usar memoria liberada para el programa y dice en qué línea ocurre. Sin la casilla el mismo error pasa
en silencio (el ejemplo «Error de límites» lo enseña). Con -b el programa va más despacio.

## El editor

Es el del Mico: colores, números de línea, paréntesis que se corresponden, sangrado automático, deshacer (Ctrl+Z),
cortar/copiar/pegar, comentar (Ctrl+/), completar (Ctrl+Espacio).

| Atajo | Qué hace |
|---|---|
| F5 | Ejecutar |
| F6 / Esc | Detener el programa |
| Ctrl+N / Ctrl+O / Ctrl+S | Nuevo, Abrir, Guardar (Ctrl+Mayús+S: guardar como) |

## Con la libsagui

Un programa puede abrir ventanas (`#include "sagui.h"` y `#include "gui.h"`, y empezar en `principal(argc, argv)` o en
`main`): mira el ejemplo «Una ventana». También funcionan `fopen`, `malloc`, `math.h`, etc. Para proyectos de
varios ficheros usa `tcc` y `make` en el Terminal (`tcc.md`, `make.md`).
