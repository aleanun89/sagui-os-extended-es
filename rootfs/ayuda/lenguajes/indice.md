<!-- categoria: Programación; orden: 1; titulo: Programar en SaguiOS -->
# Programar en SaguiOS

SaguiOS trae varios lenguajes de programación. Todos se usan desde el **Terminal**, y casi todos se pueden
escribir con el editor **Mico** (Inicio > Accesorios). Esta página compara los lenguajes y dice cuál elegir para cada cosa.
Cada lenguaje tiene su guía con el primer programa paso a paso.

## Cuál elegir

- **¿Nunca has programado?** Empieza por **Python**: es el más fácil de leer, tiene ventanas sencillas y
  funciona bien en la terminal. Si prefieres bloques de colores, prueba antes [Jabuti](../jabuti.md).
- **¿Quieres entender cómo funciona el sistema?** **C**. Es el lenguaje de SaguiOS: con `cc` escribes programas
  de consola y con `tcc` ventanas y juegos.
- **¿Quieres automatizar tareas en la terminal?** **sagsh** (el lenguaje del shell): guiones que llaman a los
  programas del sistema, con variables, bucles y funciones.
- **¿Tienes un archivo de texto con columnas o números?** **awk**: una sola orden suma, filtra y reordena.
- **¿Clases de Java en el colegio o la universidad?** **Java**: `java` y `javac` funcionan dentro de SaguiOS, y
  con `sagui.gui` puedes hacer ventanas.
- **¿Te gusta Go o quieres probar goroutines?** **Go** (TinyGo): los programas se compilan en el PC y en SaguiOS
  ya hay ejemplos listos en `/apps/go/`.
- **¿Quieres un lenguaje pequeño para incrustar en tus programas?** **Lua**.
- **¿Quieres aprender la programación de antes?** **Pascal**, con su orden `pascal` y la unidad `sagui` para gráficos.

## Los lenguajes de un vistazo

| Lenguaje | Para qué sirve | Cómo se ejecuta | Ventanas | Red | Guía |
|----------|----------------|-----------------|----------|-----|------|
| **sagsh** | automatizar, guiones del sistema | `sh guion.sh` | no | con órdenes (`curl`, `ssh`) | [sagsh](sagsh.md) |
| **C** (`cc`) | programas de consola rápidos | `cc a.c -o a` y `./a` | no | no tratada aquí | [C](c.md) |
| **C** (`tcc`) | ventanas, juegos, errores de memoria (`-b`) | `tcc -run a.c` | sí (libsagui, SDL 2, OpenGL ES) | no tratada aquí | [C](c.md), [TCC](../tcc.md) |
| **Python** | scripts, datos, ventanas, red | `python a.py` | sí (`sagui`, tkinter) | sí (`socket`, `requests`) | [Python](python.md) |
| **Java** | programas de clase, ventanas `sagui.gui` | `java A.java` | sí (`sagui.gui`) | sí (`java.net`) | [Java](java.md) |
| **Go** (TinyGo) | programas con goroutines | se compila en el PC; en SaguiOS, `/apps/go/` | no | aún no | [Go](go.md) |
| **Lua 5.4** | scripts pequeños, incrustable | `lua a.lua` | no | no | [Lua](../lua.md) |
| **awk** | texto por columnas | `awk '{ print $1 }' f` | no | no | [awk](../awk.md) |
| **Pascal** | programas estilo Turbo Pascal | `pascal a.pas -o a` | sí (unidad `sagui`) | no | [Pascal](../pascal.md) |

**make** no es un lenguaje para escribir programas, pero sí es el que compila proyectos enteros con muchos archivos:
[make](../make.md).

## Dónde escribir el código

- **Mico** (el editor de código): colorea C, Python, sh, Markdown y CHIP-8. Con **F5** ejecuta el archivo que
  tienes abierto (C con `cc`, Python con `python`, sh con `sh`). Ver [Mico](../mico.md).
- **Pluma** es un procesador de textos (guarda RTF por defecto): sirve para notas, no para programas.
- **Laboratorio de C**: en Inicio > Educación. Un editor con ejemplos y botón de ejecutar, para empezar en C.
- Para programas que corren en ventana, **VisualPythonS** te deja diseñar el formulario con el ratón y escribir
  solo las funciones de cada botón: [VisualPythonS](../visualpythons.md).

## Otros caminos para empezar

- [Jabuti](../jabuti.md): programar arrastrando bloques, sin escribir texto.
- [CHIP-8](../chip8.md): un ensamblador y un emulador de una máquina de 1977, para entender cómo funciona un
  procesador por dentro.
- [Mico](../mico.md) y el [Terminal](../terminal.md): las herramientas de cada día.

## Guías de esta sección

- [C: tu primer programa](c.md)
- [Go con TinyGo](go.md)
- [Python con MicroPython](python.md)
- [Java con miniJVM](java.md)
- [sagsh: el lenguaje del shell](sagsh.md)
- Ya en la ayuda: [Lua](../lua.md), [awk](../awk.md), [Pascal](../pascal.md), [make](../make.md),
  [TCC](../tcc.md), [Laboratorio de C](../laboratorio-c.md), [Ventanas en Python](../python-tk.md).
