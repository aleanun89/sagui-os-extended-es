# VisualPythonS

Un entorno al estilo del **Visual Basic** clásico, pero con **Python** (el MicroPython del
sistema): se dibuja el formulario, se escribe una función por evento y se ejecuta. Es más
sencillo que el Visual Basic a propósito: un solo módulo (`vps`), propiedades en español y
manejadores sin parámetros.

- Diseñador: **Inicio > Accesorios > VisualPythonS** (o `python /lib/python/vps/disenador.py`).
- Aplicaciones hechas: **Inicio > Mis aplicaciones**.
- Ejemplos: `/ejemplos/vps/` (cinco proyectos y `hola.py`, una aplicación solo con código).
- Decisión D297, mejora 123, prueba `tests/h297_visualpython.py`.

## Tutorial de 5 minutos: una calculadora

1. **Nuevo > Calculadora sencilla** ya trae todo hecho; aquí se construye desde cero para ver cada
   paso. Elige **Nuevo > Proyecto vacío**.
2. En la **caja de herramientas** (izquierda) pulsa **Etiqueta** y arrastra en el formulario. Pon
   dos etiquetas («Primero:», «Segundo:»): en la ventana de **propiedades** (derecha) elige la fila
   `texto`, escribe el texto en el cuadro de abajo y pulsa Intro.
3. Dos **CuadroTexto** (herramienta de la segunda fila) a su lado, un **Combo** con
   `elementos` = `+; -; *; /` (la fila `elementos` abre una lista: una línea por elemento) y un
   **Boton** con el texto «Calcular». Por último otra **Etiqueta** para el resultado, a la que
   pones el nombre `Resultado` (fila `nombre`).
4. **Doble clic en el botón**: se abre el editor de código en `def Boton1_click():`. Escribe:

   ```python
   def Boton1_click():
       a = float(CuadroTexto1.texto)
       b = float(CuadroTexto2.texto)
       op = Combo1.texto
       if op == "+":
           r = a + b
       elif op == "-":
           r = a - b
       elif op == "*":
           r = a * b
       else:
           r = a / b
       Resultado.texto = "Resultado: %g" % r
   ```
5. **F5** ejecuta la aplicación en su ventana. Si Python da un error, el editor salta a la línea
   con el error y la marca en rojo.
6. **Archivo > Guardar** (Ctrl+S) crea una carpeta de proyecto con `formulario.vpf` y `codigo.py`.
   **Archivo > Generar aplicación** la deja en `Inicio > Mis aplicaciones`, y se abre sin el
   diseñador.

## El diseñador

| Zona | Qué hace |
| --- | --- |
| Caja de herramientas (izquierda) | Elige el control y haz clic (tamaño por defecto) o arrastra (el tamaño que dibujes) en el formulario. El **puntero** vuelve solo tras colocar un control. |
| Formulario (centro) | Rejilla de puntos de 8 píxeles. Clic en un control: lo selecciona; arrastrarlo lo mueve; los 8 **tiradores** lo redimensionan; Mayús o Ctrl+clic suma a la selección; un recuadro arrastrado en el vacío selecciona lo que encierra. |
| Propiedades (derecha) | Una fila por propiedad. Las de texto o número se editan en el cuadro de abajo (Intro aplica); las lógicas se alternan con un clic; las de opciones y los colores abren un menú; `elementos` abre un cuadro con una línea por elemento. Con varios controles seleccionados, el cambio vale para todos. |
| Código (F7, o doble clic en un control) | Editor de Python con resaltado y, encima, los combos **Control** y **Evento**: elegir uno crea `def Control_evento():` si no existe y lleva el cursor allí. El doble clic en un control abre su evento por defecto (`click`, `cambio`, `seleccion`, `tic`...); en el formulario, `Formulario_cargar`. |

Teclas: **Supr** borra, **Ctrl+C/X/V** copia, corta y pega (con los hijos de un Marco), **Ctrl+A**
selecciona todo, las **flechas** mueven el control (una rejilla; con Ctrl, un píxel), **F5**
ejecuta, **F7** abre el código, **Ctrl+S** guarda, **Ctrl+O** abre. **Edición > Alinear** iguala
izquierda, derecha, arriba, abajo, centros, ancho o alto respecto al primer control seleccionado.

Un **Marco** contiene a los controles que se dibujen dentro de él (sus coordenadas son relativas
al Marco). **Proyecto > Agregar formulario** añade otro formulario (el principal es el primero;
los demás se abren con `Nombre.mostrar()`).

### Proyectos

Un proyecto es una **carpeta** con:

- `formulario.vpf`: los formularios y sus controles, en texto legible:

  ```
  # VisualPythonS 1
  formulario Formulario
    texto = 'Calculadora'
    ancho = 250
  control Boton Boton1
    x = 16
    y = 112
    texto = 'Calcular'
  control Etiqueta Resultado en Marco1
    texto = 'Resultado: '
  ```

  Solo se guardan las propiedades que no valen lo de por defecto; el valor es un literal de Python
  (`'texto'`, `12`, `True`, `['a', 'b']`).
- `codigo.py`: el código de los eventos. Se ejecuta con todos los controles ya creados y con sus
  nombres como variables globales (`Boton1`, `Etiqueta1`, `Formulario`...) y todo `from vps import *`.

**Generar aplicación** copia esas dos cosas y un `lanzador.py` a `/apps/vps/<Nombre>/` (a
`/home/vps/<Nombre>/` si no hay partición de apps). `python /apps/vps/<Nombre>/lanzador.py` la
abre; el menú Inicio las lista en **Mis aplicaciones**.

## Escribir aplicaciones solo con código

```python
from vps import *

f = Formulario("Hola", 300, 150)
etiqueta = Etiqueta(f, "Pulsa el botón", 20, 20, 250, 20)
boton = Boton(f, "Saludar", 20, 50, 120, 28, nombre="Boton1")

def Boton1_click():                 # se conecta sola: Nombre_evento
    etiqueta.texto = "¡Hola!"

@evento(boton, "click")            # o con el decorador
def otro():
    print("pulsado")

ejecutar(globals())                 # muestra el formulario y no vuelve hasta cerrarlo
```

Dos formas de conectar un evento, que se pueden mezclar:

- **Por el nombre:** una función `NombreDelControl_evento()` (`Boton1_click`, `CuadroTexto1_cambio`,
  `Formulario_cargar`, `Temporizador1_tic`, `Lista1_seleccion`, `Formulario_tecla`...). El nombre de
  un control es su propiedad `nombre` (`Boton(f, "Ok", nombre="Boton1")`; si no se da, es el tipo y
  un número: `Boton1`, `Boton2`...). El formulario se llama `Formulario` salvo que se le dé otro.
- **Con decorador:** `@evento(control, "evento")`; también `control.conectar("evento", funcion)`.

Los manejadores **no llevan parámetros**: los datos del evento están en el control
(`Formulario.tecla`, `Formulario.caracter`, `CuadroTexto1.tecla`, `Lista1.valor`...). Un error de
Python dentro de un manejador se informa (ventana con el error y traceback por la salida de
errores) y la aplicación sigue, como el Visual Basic sin `On Error`. En el diseñador, además,
el editor salta a la línea.

## Referencia

### Propiedades comunes (todos los controles visibles)

| Propiedad | Qué es |
| --- | --- |
| `nombre` | El nombre con el que se conectan los eventos |
| `x`, `y`, `ancho`, `alto` | Posición y tamaño en píxeles (relativos al formulario o al Marco) |
| `visible`, `activo` | Si se ve y si responde |
| `color` | Color del texto (`0xRRGGBB`, `"#rrggbb"` o un nombre: `NEGRO`, `ROJO`...) |
| `colorfondo` | Color de fondo |
| `fuente` | Se guarda (para futuras fuentes); hoy hay una sola fuente 8x16 |

Los constructores admiten por posición `(padre, texto, x, y, ancho, alto)` y el resto por nombre.

### Controles

| Control | Propiedades propias | Eventos | Notas |
| --- | --- | --- | --- |
| `Formulario(texto, ancho, alto)` | `texto` (título), `ancho`, `alto`, `color`, `redimensionable` | `cargar`, `cerrar`, `tecla`, `click`, `redimensionar` | `mostrar()`, `ocultar()`, `cerrar()`. En `Formulario_cerrar`, devolver `False` impide cerrar |
| `Etiqueta` | `texto`, `alineacion` (izquierda, centro, derecha) | `click` | |
| `Boton` | `texto`, `defecto` (Intro lo pulsa) | `click` | `pulsar()` simula la pulsación |
| `CuadroTexto` | `texto`, `multilinea`, `clave` (muestra `*`), `solo_lectura` | `cambio`, `tecla`, `click` | `multilinea` se fija al crearlo |
| `Casilla` | `texto`, `valor` (`True`/`False`) | `click` | |
| `Opcion` | `texto`, `valor`, `grupo` | `click` | Las del mismo `grupo` en el mismo formulario o Marco son excluyentes |
| `Lista` | `elementos`, `seleccion` (índice o -1), `valor` (texto elegido, solo lectura) | `seleccion`, `doble_click`, `click` | `agregar(t)`, `quitar(i)`, `limpiar()`, `contar()` |
| `Combo` | `elementos`, `texto`, `editable`, `seleccion` | `seleccion`, `cambio` | `agregar(t)`, `limpiar()` |
| `Imagen` | `archivo` (BMP, PNG, PPM), `ajustar` | `click` | `ajustar` reduce por un entero para que quepa |
| `Marco` | `texto` | `click` | Contenedor: los hijos se crean con `Marco` como padre |
| `Deslizador` (`BarraDesplazamiento`) | `valor`, `minimo`, `maximo`, `vertical` | `cambio` | |
| `BarraProgreso` | `valor`, `maximo` | | |
| `Temporizador` | `intervalo` (ms), `activo` | `tic` | Sin ventana: dispara `tic` cada `intervalo` mientras `activo` |
| `Menu` | `elementos`: una línea por título (sin sangría) y por opción (con sangría, `Abrir\|MnuAbrir`; `-` es una línea) | cada opción: `MnuAbrir_click` | Barra de menús del formulario |

### Funciones

| Función | Qué hace |
| --- | --- |
| `ejecutar(globals())` | Muestra el primer formulario, conecta los eventos y no vuelve hasta cerrarlo |
| `salir()` | Cierra todos los formularios |
| `msgbox(texto, titulo="", botones="aceptar", icono="info")` | Mensaje modal; `botones`: `aceptar`, `aceptar_cancelar`, `si_no`, `si_no_cancelar`, `reintentar_cancelar`; `icono`: `info`, `aviso`, `error`, `pregunta`. Devuelve el botón pulsado: `"aceptar"`, `"cancelar"`, `"si"`, `"no"`, `"reintentar"` |
| `inputbox(texto, titulo="", defecto="")` | Pide un texto; `None` si se cancela |
| `dialogo_abrir(titulo, carpeta, tipos)` | El diálogo de abrir del sistema (D272). `tipos=[("Imágenes", "*.bmp *.png"), ("Todo", "*")]`. Devuelve la ruta o `""` |
| `dialogo_guardar(titulo, carpeta, tipos, nombre)` | El de guardar (pregunta al sobrescribir) |
| `dialogo_carpeta(titulo, carpeta)` | El de elegir carpeta |
| `pausa(ms)` | Espera sin congelar la ventana |

### Sobre tkinter

`vps` está hecho sobre el `tkinter` del sistema: cada control tiene su componente en `.w`
(`Boton1.w`) por si hace falta algo que `vps` no ofrece. Lo que `tkinter` no tiene (varias
fuentes, colores por trozos de texto) tampoco está en `vps`.

## Límites actuales

- Una sola fuente (8x16): `fuente` se guarda pero no cambia nada.
- El diseñador dibuja los controles de forma aproximada (con aspecto Win98); en la ejecución los
  componentes son los de `tkinter` del sistema.
- Un formulario secundario no hereda el tamaño máximo: como cualquier ventana, hasta 1000x700.
- Renombrar un control no actualiza el código (como en el Visual Basic antiguo).
- Sin deshacer en el diseñador ni en el editor.
