<!-- categoria: Programación/Python; orden: 40; titulo: Python -->
# Python no SaguiOS: MicroPython e seus módulos

O **Python** é a linguagem mais usada para aprender a programar: lê-se quase como um texto escrito em português, e
serve para roteiros, jogos, janelas, redes e dados. No SaguiOS o interpretador é o **MicroPython 1.24**: a mesma
sintaxe do Python 3, pensada para máquinas pequenas. Abre-se com o comando `python`.

Diferenças em relação ao Python de um PC: algumas bibliotecas inteiras não existem e outras são versões pequenas (veja
[Limites](#limites) no fim). O que existe funciona da mesma forma.

## Primeiro programa, passo a passo

**1.** No Terminal, crie uma pasta e entre nela:

    mkdir python
    cd python

**2.** Abra o editor de código com o arquivo novo:

    mico hola.py

**3.** Escreva isto:

```python
# hola.py: el primer programa en Python
nombre = "mundo"
print("Hola,", nombre, "desde Python en SaguiOS")
print("2 + 3 =", 2 + 3)
print("2 elevado a 100 =", 2 ** 100)
```

**4.** Salve com **Ctrl+S** e execute. No Terminal:

    python hola.py

Com **F5** no Mico você obtém o mesmo resultado (ele executa o `python` sobre o arquivo e mostra a saída embaixo).

Para escrever uma linha só e ver o resultado, sem arquivo, digite `python` e depois um comando. Sai-se com **Ctrl+D**
numa linha vazia. Para um comando só: `python -c "print(7 * 6)"`.

Os exemplos do sistema estão em `/ejemplos`: `hola.py`, `archivos.py`, `sagui_ventana.py`, `red.py`, `json_http.py`,
`flotante.py`, `hilos.py`, e a pasta `tk/` com programas de janelas.

## O básico da linguagem

### Números, texto e listas

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

O texto pode ter acentos e aparece corretamente na tela.

### Condições e laços

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

A indentação (os espaços no início da linha) é obrigatória: é ela que mostra quais linhas pertencem a cada bloco.

### Funções

```python
def factorial(n):
    """Multiplica todos los números de 1 a n."""
    total = 1
    for i in range(2, n + 1):
        total *= i
    return total

print([factorial(i) for i in range(1, 8)])
```

### Dicionários e classes

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

## Arquivos

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

Os textos são guardados em Latin-1, como o resto do terminal. O `os` permite listar (`os.listdir`), apagar
(`os.remove`) e renomear (`os.rename`) arquivos; o `os.path` ajuda com os caminhos.

## Janelas e desenho

Há dois caminhos. Para jogos e gráficos, o módulo **`sagui`** (rápido, em C):

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

Para formulários com botões e caixas de texto, o **tkinter** clássico, que o SaguiOS traz sobre o `sagui`:

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

Todos os detalhes de funções, eventos e componentes estão em [Janelas em Python](../python-tk.md). E para desenhar
formulários arrastando componentes, o [VisualPythonS](../visualpythons.md).

## Rede e dados

```python
import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(10)
s.connect(("example.com", 80))           # necesita red
s.send(b"GET / HTTP/1.0\r\nHost: example.com\r\n\r\n")
print(s.recv(200))
s.close()
```

Para HTTP e HTTPS é mais cômodo o `requests` (em `/lib/python/`):

```python
import requests

r = requests.get("https://api.github.com/repos/micropython/micropython",
                 headers={"Accept": "application/json"}, timeout=15)
print(r.status_code, r.headers.get("content-type"))
print(r.json()["stargazers_count"])
```

O `json`, o `hashlib` (somas de verificação) e o `sqlite3` (bancos de dados, veja [Bancos de dados](../bases-de-datos.md))
também estão aqui. Um servidor web simples de exemplo: `/ejemplos/red.py`.

## Erros típicos

| O que aparece | O que acontece | O que fazer |
|---------------|----------------|-------------|
| `IndentationError` ou «unexpected indent» | a indentação não está alinhada | use quatro espaços por nível e não misture tabulações |
| `NameError: name 'x' isn't defined` | você usou uma variável antes de dar um valor a ela, ou com outro nome | confira a ortografia e a ordem das linhas |
| `ImportError` / `No module named` | o módulo não existe no SaguiOS | veja a lista de [Limites](#limites) e procure uma alternativa |
| O programa de janelas não responde | você não chama `evento` nem `mainloop` | o laço de eventos tem de estar no programa |
| `ZeroDivisionError` | você dividiu por zero | verifique o divisor antes |

## Limites

- **Não há `pip` nem instalação de bibliotecas**: o que existe está em `/lib/python` e nos módulos embutidos.
- Módulos embutidos: `sys`, `os`, `time`, `math`, `random`, `json`, `re`, `socket`, `ssl`, `hashlib`, `select`,
  `sqlite3`, `binascii`, `heapq`, `gc`, `io`, `sagui`. E em `/lib/python`: `requests`, `collections`, `threading`,
  `urllib`, `tkinter`.
- Pequenas diferenças de comportamento em relação ao Python do PC (por exemplo, a forma de escrever alguns números de
  ponto flutuante). Se um resultado não bater com o que você espera, verifique com `python -c` antes de culpar o programa.
- As threads têm um GIL: só uma thread executa Python de cada vez (veja `/ejemplos/hilos.py`).

## O que vem depois

- Roteiros no terminal, mais curtos que um programa: [sagsh](sagsh_PT.md).
- C para programas rápidos: [C](c_PT.md). Java: [Java](java_PT.md). Go: [Go](go_PT.md).
- O índice com todas as linguagens: [Programar no SaguiOS](indice_PT.md).
