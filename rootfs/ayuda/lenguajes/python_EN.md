<!-- categoria: Programación/Python; orden: 40; titulo: Python -->
# Python on SaguiOS: MicroPython and its modules

**Python** is the most used language for learning to program: it reads almost like plain written English, and it
serves for scripts, games, windows, networks and data. On SaguiOS the interpreter is **MicroPython 1.24**: the same
Python 3 syntax, designed for small machines. It is opened with the command `python`.

Differences from Python on a PC: some whole libraries are missing and others are small versions (see [Limits](#limits)
at the end). What is there works the same way.

## First program, step by step

**1.** In the Terminal, create a folder and go into it:

    mkdir python
    cd python

**2.** Open the code editor with the new file:

    mico hola.py

**3.** Type this:

```python
# hola.py: el primer programa en Python
nombre = "mundo"
print("Hola,", nombre, "desde Python en SaguiOS")
print("2 + 3 =", 2 + 3)
print("2 elevado a 100 =", 2 ** 100)
```

**4.** Save with **Ctrl+S** and run it. In the Terminal:

    python hola.py

With **F5** in Mico you get the same result (it runs `python` on the file and shows the output below).

To write a single line and see the result, without a file, type `python` and then a command. Exit with **Ctrl+D**
on an empty line. For a single command: `python -c "print(7 * 6)"`.

The system examples are in `/ejemplos`: `hola.py`, `archivos.py`, `sagui_ventana.py`, `red.py`, `json_http.py`,
`flotante.py`, `hilos.py`, and the folder `tk/` with window programs.

## The basics of the language

### Numbers, text and lists

```python
# basico.py
x = 7
y = 2
print(x + y, x - y, x * y, x / y)      # 9 5 14 3.5
print(x // y, x % y, x ** y)           # 3 1 49

nombre = "sagui"
print(nombre.upper(), len(nombre))     # SAGUI 5
print(f"El {nombre} tiene {len(nombre)} letras")   # f-strings: texto con variables

frutas = ["pera", "plátano", "piña"]
frutas.append("uva")
print(frutas[0], frutas[-1], len(frutas))
for f in sorted(frutas):
    print("-", f)
```

Text can contain accents and is written correctly on the screen.

### Conditions and loops

```python
for i in range(1, 6):
    print(i, "x 2 =", i * 2)

n = 3
while n > 0:
    print("cuenta atrás:", n)
    n -= 1

if n == 0:
    print("¡despegue!")
elif n < 0:
    print("ya ha pasado")
else:
    print("todavía no")
```

Indentation (the spaces at the start of a line) is required: it shows which lines belong to each block.

### Functions

```python
def factorial(n):
    """Multiplica todos los números de 1 a n."""
    total = 1
    for i in range(2, n + 1):
        total *= i
    return total

print([factorial(i) for i in range(1, 8)])
```

### Dictionaries and classes

```python
cuenta = {}
for p in "el sagui sube y el sagui baja".split():
    cuenta[p] = cuenta.get(p, 0) + 1
print(sorted(cuenta.items()))

class Sagui:
    def __init__(self, nombre):
        self.nombre = nombre
    def saludar(self):
        return "Soy " + self.nombre

print(Sagui("Tito").saludar())
```

## Files

```python
import os

carpeta = "/tmp/notas"
if not os.path.isdir(carpeta):
    os.mkdir(carpeta)

ruta = carpeta + "/compras.txt"
with open(ruta, "w") as f:           # "w": escribir (borra lo anterior)
    f.write("plátanos\n")
    f.write("piñas\n")

with open(ruta, "a") as f:           # "a": añadir al final
    print("café", file=f)

with open(ruta) as f:                # leer
    for linea in f:
        print("> " + linea.rstrip())
```

Texts are stored in Latin-1, as the rest of the terminal. `os` lets you list (`os.listdir`), delete (`os.remove`)
and rename (`os.rename`) files; `os.path` helps with paths.

## Windows and drawing

There are two ways. For games and graphics, the **`sagui`** module (fast, written in C):

```python
import sagui

v = sagui.Ventana(320, 200, "Hola")
v.rect(10, 10, 100, 50, 0x2A6FD8)
v.texto(20, 30, "¡Hola!", 0xFFFFFF)
v.actualizar()
while True:
    e = v.evento(1000)               # espera hasta 1 s; None si no pasa nada
    if e and e.tipo == sagui.CERRAR:
        break
```

For forms with buttons and text boxes, the classic **tkinter**, which SaguiOS provides on top of `sagui`:

```python
import tkinter as tk
from tkinter import messagebox

raiz = tk.Tk()
raiz.title("Hola")
tk.Label(raiz, text="¿Cómo te llamas?").pack(padx=10, pady=5)
nombre = tk.Entry(raiz)
nombre.pack(padx=10)
tk.Button(raiz, text="Saludar",
          command=lambda: messagebox.showinfo("Hola", "¡Hola, %s!" % nombre.get())).pack(pady=8)
raiz.mainloop()
```

All the details of functions, events and components are in [Windows in Python](../python-tk.md). And to design
forms by dragging components, [VisualPythonS](../visualpythons.md).

## Network and data

```python
import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(10)
s.connect(("example.com", 80))           # necesita red
s.send(b"GET / HTTP/1.0\r\nHost: example.com\r\n\r\n")
print(s.recv(200))
s.close()
```

For HTTP and HTTPS, `requests` (in `/lib/python/`) is more convenient:

```python
import requests

r = requests.get("https://api.github.com/repos/micropython/micropython",
                 headers={"Accept": "application/json"}, timeout=15)
print(r.status_code, r.headers.get("content-type"))
print(r.json()["stargazers_count"])
```

`json`, `hashlib` (checksums) and `sqlite3` (databases, see [Databases](../bases-de-datos.md)) are also there. A simple
example web server: `/ejemplos/red.py`.

## Typical errors

| What you see | What happens | What to do |
|--------------|--------------|------------|
| `IndentationError` or «unexpected indent» | the indentation does not line up | use four spaces per level and do not mix tabs |
| `NameError: name 'x' isn't defined` | you used a variable before giving it a value, or with another name | check the spelling and the order of the lines |
| `ImportError` / `No module named` | the module does not exist in SaguiOS | check the [Limits](#limits) list and look for an alternative |
| The window program does not respond | you do not call `evento` or `mainloop` | the event loop must be in the program |
| `ZeroDivisionError` | you divide by zero | check the divisor first |

## Limits

- **There is no `pip` and you cannot install libraries**: what exists is in `/lib/python` and in the built-in modules.
- Built-in modules: `sys`, `os`, `time`, `math`, `random`, `json`, `re`, `socket`, `ssl`, `hashlib`, `select`,
  `sqlite3`, `binascii`, `heapq`, `gc`, `io`, `sagui`. And in `/lib/python`: `requests`, `collections`, `threading`,
  `urllib`, `tkinter`.
- Small differences in behavior from Python on a PC (for example, how some floating-point numbers are written). If a
  result does not match what you expect, check it with `python -c` before blaming the program.
- Threads have a GIL: only one thread runs Python at a time (see `/ejemplos/hilos.py`).

## What comes next

- Scripts in the terminal, shorter than a program: [sagsh](sagsh_EN.md).
- C for fast programs: [C](c_EN.md). Java: [Java](java_EN.md). Go: [Go](go_EN.md).
- The index with all the languages: [Programming on SaguiOS](indice_EN.md).
