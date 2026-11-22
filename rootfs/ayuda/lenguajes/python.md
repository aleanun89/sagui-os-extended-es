<!-- categoria: Programación/Python; orden: 40; titulo: Python -->
# Python en SaguiOS: MicroPython y sus módulos

**Python** es el lenguaje más usado para aprender a programar: se lee casi como inglés o español escrito, y sirve
para scripts, juegos, ventanas, redes y datos. En SaguiOS el intérprete es **MicroPython 1.24**: la misma
sintaxis de Python 3, pensada para equipos pequeños. Se abre con la orden `python`.

Diferencias con el Python de un PC: algunas bibliotecas enteras no están y otras son pequeñas versiones (ver
[Límites](#límites) al final). Lo que sí está, funciona igual.

## Primer programa, paso a paso

**1.** En el Terminal, crea una carpeta y entra en ella:

    mkdir python
    cd python

**2.** Abre el editor de código con el archivo nuevo:

    mico hola.py

**3.** Escribe esto:

```python
# hola.py: el primer programa en Python
nombre = "mundo"
print("Hola,", nombre, "desde Python en SaguiOS")
print("2 + 3 =", 2 + 3)
print("2 elevado a 100 =", 2 ** 100)
```

**4.** Guarda con **Ctrl+S** y ejecútalo. En el Terminal:

    python hola.py

Con **F5** en Mico se hace lo mismo (ejecuta `python` sobre el archivo y muestra la salida abajo).

Para escribir una línea suelta y ver el resultado, sin archivo, escribe `python` y luego una orden. Sale con
**Ctrl+D** en una línea vacía. Para una sola orden: `python -c "print(7 * 6)"`.

Los ejemplos del sistema están en `/ejemplos`: `hola.py`, `archivos.py`, `sagui_ventana.py`, `red.py`,
`json_http.py`, `flotante.py`, `hilos.py`, y la carpeta `tk/` con programas de ventanas.

## Lo básico del lenguaje

### Números, texto y listas

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

Las cadenas pueden llevar acentos y se escriben bien en la pantalla.

### Condiciones y bucles

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

La sangría (los espacios al principio de la línea) es obligatoria: marca qué pertenece a cada bloque.

### Funciones

```python
def factorial(n):
    """Multiplica todos los números de 1 a n."""
    total = 1
    for i in range(2, n + 1):
        total *= i
    return total

print([factorial(i) for i in range(1, 8)])
```

### Diccionarios y clases

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

## Archivos

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

Los textos se guardan en Latin-1, como el resto del terminal. `os` permite listar (`os.listdir`), borrar
(`os.remove`) y renombrar (`os.rename`) archivos; `os.path` ayuda con las rutas.

## Ventanas y dibujo

Hay dos caminos. Para juegos y gráficos, el módulo **`sagui`** (rápido, en C):

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

Para formularios con botones y cuadros de texto, el **tkinter** de siempre, que SaguiOS trae sobre `sagui`:

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

Todo el detalle de funciones, eventos y componentes está en [Ventanas en Python](../python-tk.md). Y para diseñar
formularios arrastrando componentes, [VisualPythonS](../visualpythons.md).

## Red y datos

```python
import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(10)
s.connect(("example.com", 80))           # necesita red
s.send(b"GET / HTTP/1.0\r\nHost: example.com\r\n\r\n")
print(s.recv(200))
s.close()
```

Para HTTP y HTTPS es más cómodo `requests` (en `/lib/python/`):

```python
import requests

r = requests.get("https://api.github.com/repos/micropython/micropython",
                 headers={"Accept": "application/json"}, timeout=15)
print(r.status_code, r.headers.get("content-type"))
print(r.json()["stargazers_count"])
```

`json`, `hashlib` (sumas de comprobación) y `sqlite3` (bases de datos, ver [Bases de datos](../bases-de-datos.md))
también están. Un servidor web sencillo de ejemplo: `/ejemplos/red.py`.

## Errores típicos

| Lo que ves | Qué pasa | Qué hacer |
|------------|----------|-----------|
| `IndentationError` o «unexpected indent» | la sangría no cuadra | usa cuatro espacios por nivel y no mezcles tabuladores |
| `NameError: name 'x' isn't defined` | usaste una variable antes de darle valor, o con otro nombre | revisa la ortografía y el orden de las líneas |
| `ImportError` / `No module named` | el módulo no existe en SaguiOS | mira la lista de [Límites](#límites) y busca una alternativa |
| El programa de ventanas no responde | no llamas a `evento` ni a `mainloop` | el bucle de eventos debe estar en el programa |
| `ZeroDivisionError` | divides entre cero | comprueba antes el divisor |

## Límites

- **No hay `pip` ni se instalan bibliotecas**: lo que hay está en `/lib/python` y en los módulos incorporados.
- Módulos incorporados: `sys`, `os`, `time`, `math`, `random`, `json`, `re`, `socket`, `ssl`, `hashlib`, `select`,
  `sqlite3`, `binascii`, `heapq`, `gc`, `io`, `sagui`. Y en `/lib/python`: `requests`, `collections`,
  `threading`, `urllib`, `tkinter`.
- Pequeñas diferencias de comportamiento con el Python del PC (por ejemplo, la forma de escribir algunos
  flotantes). Si un resultado no cuadra con lo que esperas, compruébalo con `python -c` antes de dar la culpa al programa.
- Los hilos tienen un GIL: un solo hilo ejecuta Python a la vez (ver `/ejemplos/hilos.py`).

## Qué sigue

- Scripts de la terminal, más cortos que un programa: [sagsh](sagsh.md).
- C para programas rápidos: [C](c.md). Java: [Java](java.md). Go: [Go](go.md).
- El índice con todos los lenguajes: [Programar en SaguiOS](indice.md).
