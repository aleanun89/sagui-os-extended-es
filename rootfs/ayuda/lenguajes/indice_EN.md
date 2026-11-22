<!-- categoria: Programación; orden: 1; titulo: Programar en SaguiOS -->
# Programming on SaguiOS

SaguiOS ships several programming languages. All of them are used from the **Terminal**, and almost all can be
written with the **Mico** editor (Inicio > Accesorios, the Start menu). This page compares the languages and says which
one to choose for each task. Each language has its own guide with the first program step by step.

## Which one to choose

- **Never programmed before?** Start with **Python**: it is the easiest to read, it has simple windows and it works
  well in the terminal. If you prefer colored blocks, try [Jabuti](../jabuti.md) first.
- **Want to understand how the system works?** **C**. It is SaguiOS's language: with `cc` you write console
  programs and with `tcc` windows and games.
- **Want to automate tasks in the terminal?** **sagsh** (the shell language): scripts that call the system programs,
  with variables, loops and functions.
- **Have a text file with columns or numbers?** **awk**: a single command can add up, filter and reorder.
- **Java classes at school or university?** **Java**: `java` and `javac` work inside SaguiOS, and with `sagui.gui`
  you can make windows.
- **Like Go or want to try goroutines?** **Go** (TinyGo): programs are compiled on the PC, and SaguiOS already has
  ready examples in `/apps/go/`.
- **Want a small language to embed in your programs?** **Lua**.
- **Want to learn the programming of the past?** **Pascal**, with its `pascal` command and the `sagui` unit for graphics.

## The languages at a glance

| Language | What it is for | How it runs | Windows | Network | Guide |
|----------|----------------|-------------|---------|---------|-------|
| **sagsh** | automation, system scripts | `sh guion.sh` | no | through commands (`curl`, `ssh`) | [sagsh](sagsh_EN.md) |
| **C** (`cc`) | fast console programs | `cc a.c -o a` and `./a` | no | not covered here | [C](c_EN.md) |
| **C** (`tcc`) | windows, games, memory errors (`-b`) | `tcc -run a.c` | yes (libsagui, SDL 2, OpenGL ES) | not covered here | [C](c_EN.md), [TCC](../tcc.md) |
| **Python** | scripts, data, windows, network | `python a.py` | yes (`sagui`, tkinter) | yes (`socket`, `requests`) | [Python](python_EN.md) |
| **Java** | class-based programs, `sagui.gui` windows | `java A.java` | yes (`sagui.gui`) | yes (`java.net`) | [Java](java_EN.md) |
| **Go** (TinyGo) | programs with goroutines | compiled on the PC; on SaguiOS, `/apps/go/` | no | not yet | [Go](go_EN.md) |
| **Lua 5.4** | small scripts, embeddable | `lua a.lua` | no | no | [Lua](../lua.md) |
| **awk** | text by columns | `awk '{ print $1 }' f` | no | no | [awk](../awk.md) |
| **Pascal** | Turbo Pascal style programs | `pascal a.pas -o a` | yes (`sagui` unit) | no | [Pascal](../pascal.md) |

**make** is not a language for writing programs, but it is the tool that compiles whole projects with many files:
[make](../make.md).

## Where to write the code

- **Mico** (the code editor): colors C, Python, sh, Markdown and CHIP-8. With **F5** it runs the file you have open
  (C with `cc`, Python with `python`, sh with `sh`). See [Mico](../mico.md).
- **Pluma** is a word processor (it saves RTF by default): it is for notes, not for programs.
- **C Laboratory**: in Inicio > Educación. An editor with examples and a run button, to start in C.
- For programs that run in a window, **VisualPythonS** lets you design the form with the mouse and write only the
  functions for each button: [VisualPythonS](../visualpythons.md).

## Other ways to start

- [Jabuti](../jabuti.md): programming by dragging blocks, without typing text.
- [CHIP-8](../chip8.md): an assembler and an emulator of a machine from 1977, to understand how a processor works inside.
- [Mico](../mico.md) and the [Terminal](../terminal.md): the everyday tools.

## Guides in this section

- [C: your first program](c_EN.md)
- [Go with TinyGo](go_EN.md)
- [Python with MicroPython](python_EN.md)
- [Java with miniJVM](java_EN.md)
- [sagsh: the shell language](sagsh_EN.md)
- Already in the help: [Lua](../lua.md), [awk](../awk.md), [Pascal](../pascal.md), [make](../make.md),
  [TCC](../tcc.md), [C Laboratory](../laboratorio-c.md), [Windows in Python](../python-tk.md).
