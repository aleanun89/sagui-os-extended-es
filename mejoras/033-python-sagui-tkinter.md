# 033 - Python: módulo `sagui` (ventanas y dibujo), tkinter y Python sin SSE2

- **Estado:** hecha
- **Prioridad:** media
- **Alta:** 2026-11-07
- **Relacionadas:** 032 (fase 3: módulo `sagui`), 036 (fase A: Python sin SSE2), 037 (portapapeles,
  llamadas 94-95)

## Problema / motivación

El `python` de SaguiOS (mejoras 004 y 032) ya tenía archivos, biblioteca estándar y red, pero
no podía abrir una ventana: quedaba fuera de lo más vistoso del sistema (el escritorio, el
ratón, los menús). La forma habitual de hacer ventanas en Python es tkinter, y muchos
programas de ejemplo y de clase lo usan. Además, `python` no funcionaba con `float` en un
Pentium III o un Athlon XP (SSE sin SSE2), justo las máquinas de la mejora 036.

## Propuesta (aplicada)

Detalle en `docs/decisoes.md`, D082 (Python sin SSE2), D083 (módulo `sagui`) y D084 (tkinter).

**Parte 1: Python sin SSE2 (D082).** Dos binarios a partir de los mismos fuentes:
`/bin/python` (SSE2, como antes) y `/bin/python-x87` (`-mfpmath=387`, la x87 con precisión de
53 bits). `/bin/python` mira el CPUID antes de tocar un `float` y, sin SSE2, ejecuta la
variante x87 con los mismos argumentos. Los resultados coinciden con los de SSE2. Un
`float.h` propio (`FLT_EVAL_METHOD 0`) evita que el `rint` de la libm_dbl redondee mal con la
x87.

**Parte 2: módulo `sagui` en C (D083),** `userland/micropython/janela_sagui.c`:
- `sagui.Ventana` con `pixel`, `linea`, `rect`, `elipse`, `poligono`, `texto` (fuente 8x16),
  `imagen`, `copiar_area`, `limpiar`, `recorte`, `actualizar` (solo el rectángulo sucio),
  `tamano`, `titulo`, `cerrar`, `menu_contextual`, `evento(timeout_ms)`, `teclas`...;
- `sagui.Imagen` (`cargar` BMP/PNM/PNG, `nueva`, `guardar`, `escalar`, `recortar`, `pixel`,
  `ancho`, `alto`, los mismos dibujos, `memoryview`);
- `sagui.evento`, `ticks_ms`, `dormir`, `pantalla`, `bip`, `portapapeles`.
- Para dormir hasta el próximo evento o temporizador, `SIS_PROXIMO_EVENTO` acepta un plazo
  (`EVENTO_PRAZO | instante`), sin número de llamada nuevo, y la libsagui tiene
  `esperar_evento(id, &e, ms)`.

**Parte 3: tkinter (D084),** `/lib/python/tkinter/`, escrito en Python sobre `sagui`: `Tk`,
`Toplevel`, `Frame`, `LabelFrame`, `Label`, `Message`, `Button`, `Entry`, `Text`, `Checkbutton`,
`Radiobutton`, `Listbox`, `Scrollbar`, `Scale`, `Spinbox`, `OptionMenu`, `Menubutton`,
`Canvas`, `Menu`, `PhotoImage`; `pack`, `grid` y `place`; `StringVar`/`IntVar`/`DoubleVar`/
`BooleanVar`; `bind` con los patrones de Tk; `after`; `messagebox`, `simpledialog`,
`filedialog`, `scrolledtext`, `font` y un `ttk` mínimo. Aspecto del SaguiOS y redibujado solo
de lo que cambia. Lista completa en `/ayuda/python-tk.md`.

**Ejemplos:** `/ejemplos/sagui_ventana.py` y `/ejemplos/tk/`: `calculadora.py` (grid),
`tareas.py` (Entry, Listbox, botones, barra de menús, messagebox), `dibujo.py` (Canvas con el
ratón, menú emergente), `reloj.py` (after), `formulario.py` (Check/Radio/Scale/Spinbox/
OptionMenu y messagebox) y `juego.py` («Lluvia de bananas», en un Canvas).

## Criterios de aceptación

- [x] `python` con `float` en `-cpu pentium3` (y `pentium2`), con los mismos resultados que con
      SSE2 en `math`, `mandelbrot.py` y `flotante.py`
- [x] Módulo `sagui`: ventana, dibujo comprobado por píxeles, imágenes, eventos (ratón,
      teclado, rueda, menú, plazo, cerrar) y rendimiento razonable en QEMU
- [x] tkinter: los ejemplos manejados con el ratón y el teclado (botones, Entry, Listbox,
      Checkbutton, Radiobutton, Scale, Spinbox, pack/grid/place, Canvas con bind, after, Menu,
      messagebox, Text, PhotoImage)
- [x] Test automático: `tests/h68_python_tk.py`; `tests/h35_simd.py` actualizado
- [x] Decisiones D082-D084 en `docs/decisoes.md`; ayuda en `/ayuda/python-tk.md`
- [ ] Probado en un PC real sin SSE2 (Pentium III o Athlon XP) y en el G470

## Cifras

- `/bin/python`: 287.468 -> 320.236 bytes (+32 KB: el módulo `sagui`, la biblioteca de imágenes
  de la libsagui con su inflate y los nombres qstr).
- `/bin/python-x87`: 312.044 bytes (nuevo).
- `/lib/python/tkinter`: 248 KB de Python en 11 archivos; ejemplos 20 KB; ayuda 9 KB.
- Imagen: 2.412.032 -> 3.036.672 bytes útiles (38,3 % -> 48,3 % de la meta de 6 MB): +312 KB
  `python-x87`, +33 KB `python`, +248 KB tkinter, +30 KB ejemplos y ayuda.
- Velocidad (QEMU sin KVM): `import tkinter` ~220 ms; 10.000 `rect` ~40 ms; un cuadro de
  640x480 calculado en Python ~190 ms; x87 frente a SSE2: +19 % en un bucle de `float`, +40 %
  en `sin`/`exp`/`sqrt`, igual con enteros (D082).

## Diferencias conocidas con el tkinter de CPython

- Una sola fuente de 8x16; `font=` se acepta y se ignora.
- Sin Tcl (`eval`/`call`), sin colores en las tags del `Text`, sin deshacer, sin
  `ttk.Notebook`/`Treeview`, sin `PhotoImage(data=...)` ni `BitmapImage`.
- `winfo_rootx()` y `event.x_root` son relativos a la ventana.
- El portapapeles usa las llamadas 94-95 de la mejora 037 cuando el kernel las tenga; hasta
  entonces vale dentro del mismo programa.
- Mientras un menú del sistema está abierto, los `after` esperan.
