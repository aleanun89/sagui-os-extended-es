# VisualPythonS

Un entorno al estilo del **Visual Basic** clásico, pero con **Python** (el MicroPython del
sistema): se dibuja el formulario, se escribe una función por evento y se ejecuta. Es más
sencillo que el Visual Basic a propósito: un solo módulo (`vps`), propiedades en español y
manejadores sin parámetros.

- Diseñador: **Inicio > Accesorios > VisualPythonS** (o `python /lib/python/vps/disenador.py`).
- Aplicaciones hechas: **Inicio > Mis aplicaciones**.
- **Manual para empezar, con capturas: [visualpythons-manual.md](visualpythons-manual.md)**
  (en el sistema, `/ayuda/visualpythons-manual.md`): primeros pasos, propiedades, eventos, quince ejemplos
  completos, herramientas y errores frecuentes.
- Ejemplos: `/ejemplos/vps/` (veintiún proyectos y `hola.py`, una aplicación solo con código).
- Decisiones D297 (el entorno), D299 (la versión avanzada) y D303 (componentes nuevos y autocompletado de
  sintaxis), mejoras 123 y 124.

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

**Explorador del proyecto (v8.4.1, D315).** **Ver > Explorador** (o **Ctrl+R**) muestra, encima de las propiedades, un árbol
Proyecto > Formularios > controles (con Marcos, páginas de las Pestanas y controles sin ventana) y, al final, los manejadores de
`codigo.py`. Clic: selecciona el control en el diseñador (cambia de formulario y de pestaña); doble clic: abre su evento por defecto
(o salta a la línea del manejador); clic derecho: ver código, renombrar (con la refactorización de D299), eliminar, traer al frente,
enviar atrás y agregar formulario; **F2** y **Supr** renombran y eliminan; el cuadro **Buscar** filtra por nombre. Se mantiene al
día al colocar, borrar, renombrar y deshacer.

Teclas: **Supr** borra, **Ctrl+C/X/V** copia, corta y pega (con los hijos de un Marco), **Ctrl+A**
selecciona todo, las **flechas** mueven el control (una rejilla; con Ctrl, un píxel), **Ctrl+Z** y
**Ctrl+Y** deshacen y rehacen, **F5** ejecuta, **F8** ejecuta paso a paso, **F9** pone un punto de
parada, **F7** abre el código, **Ctrl+S** guarda, **Ctrl+O** abre. **Edición > Alinear** iguala
izquierda, derecha, arriba, abajo, centros, ancho o alto respecto al primer control seleccionado.

### Deshacer y rehacer

**Ctrl+Z** y **Ctrl+Y** (o **Edición > Deshacer / Rehacer**) valen para todo lo que se hace en el
diseñador: colocar, mover, redimensionar, cambiar una propiedad, alinear, borrar, cortar, pegar,
agregar o quitar un formulario y renombrar. Hasta 100 pasos. Cada paso guarda una copia de los
formularios **y** del código, así que deshacer un renombrado devuelve el control y el código a la vez.

El editor de código tiene su propio **Ctrl+Z / Ctrl+Y**: las teclas seguidas del mismo tipo (escribir,
borrar) forman un solo paso mientras no pase más de un segundo; Intro y pegar son pasos aparte.

### Renombrar un control

Cambiar la propiedad `nombre` actualiza el código: los manejadores (`Boton1_click` pasa a
`Aceptar_click`) y las referencias (`Boton1.texto` pasa a `Aceptar.texto`), con un análisis de
tokens de Python, **no** un reemplazo ciego: no toca las cadenas (`"Boton1 listo"`), los
comentarios, los atributos de otros objetos (`otro.Boton1`) ni los argumentos con nombre
(`f(Boton1=2)`); sí cambia lo que haya dentro de las llaves de una f-cadena. Antes de aplicar se
enseña **la lista de líneas que cambian** (antes y después) para aceptarla o cancelar. Si el código
ya usa el nombre nuevo (una variable `Aceptar`, un `def Aceptar_click`), se rechaza el cambio. Con el
formulario seleccionado vale igual (`Formulario_cargar` pasa a `Principal_cargar`).

### Formularios secundarios y modales

**Proyecto > Agregar formulario** añade un formulario con **su propio tamaño** (propiedades `ancho` y
`alto`, hasta 1000x700). Cada formulario es una variable del código y sus controles son atributos
suyos (`Formulario2.CuadroTexto1.texto`).

```python
def Boton1_click():
    nombre = Formulario2.mostrar(modal=True, datos={"sugerido": "Ana"})   # espera
    Etiqueta2.texto = "Hola, " + nombre if nombre else "(cancelado)"

def Formulario2_cargar():                       # se dispara cada vez que se abre
    CuadroTexto1.texto = Formulario2.datos.get("sugerido", "")

def Boton2_click():
    Formulario2.cerrar(CuadroTexto1.texto)      # el valor que devuelve mostrar(modal=True)
```

- `mostrar(modal=False, datos=None)`: abre el formulario. Con `modal=True` no vuelve hasta que se
  cierra (el resto de ventanas no responde, como `ShowDialog`) y **devuelve el valor de
  `cerrar(valor)`**, o `None` si se cerró con la X o con `cerrar()` a secas. Sin `modal`, el
  formulario es independiente.
- `cerrar(resultado=None)`: un formulario **secundario solo se esconde** (se puede volver a abrir y
  se vuelve a disparar `cargar`); cerrar el principal termina la aplicación. `ocultar()` esconde
  sin devolver valor.
- `datos` es un diccionario del formulario para pasar datos de ida (`mostrar(datos=...)`) y de vuelta
  (`Formulario2.datos["x"] = 1` antes de cerrar); `resultado` guarda lo último devuelto.
- Ejemplo: **Nuevo > Formularios modales** (`/ejemplos/vps/formularios-modales`).

### Autocompletado y ayuda en el editor

Al escribir un punto tras un nombre, el editor abre una lista: tras `Boton1.` las **propiedades**
(`p`), **métodos** (`m`) y **eventos** (`e`, que insertan `conectar("click", `) del control; tras
`vps.` la API del módulo; tras el nombre de un formulario, sus controles y métodos. Se sigue escribiendo
para filtrar; **Tab** o **Intro** aceptan, **Flechas** eligen y **Esc** cierra. **Ctrl+Espacio** la
abre a mano. La barra de estado enseña la firma y una ayuda breve de la opción marcada; al abrir un
paréntesis aparece la firma de la función (`Lista1.agregar(texto) - Añade un elemento al final`).

Desde la v8.3.2 (D303) también ofrece, **tras dos caracteres** (o con Ctrl+Espacio con uno):

- las **palabras clave** de Python (`k`) y las **funciones integradas** que tiene el MicroPython del
  sistema (`i`: `print`, `len`, `range`...);
- los **nombres del propio código** (`v` variables y parámetros, `f` funciones, `C` clases, `M` módulos
  importados), con la firma y la primera línea del texto de ayuda (`"""..."""` bajo el `def`);
- la **firma de las funciones del usuario** al abrir el paréntesis (`sumar(a, b) - Suma dos numeros`).

No molesta: no sale dentro de un texto entre comillas ni de un comentario, no ofrece lo que ya está
escrito entero y **Esc** la cierra hasta que se empiece otra palabra.

**Plantillas:** `def`, `if`, `elif`, `else`, `for`, `while`, `try`, `class` o `with` + **Tab** escriben el bloque
(con la sangría de la línea) y dejan el cursor en el primer hueco (`def |():`). Aceptar la palabra clave de
la lista también la expande.

**Sangría automática:** Intro tras `:` sangra cuatro espacios (aunque haya un comentario detrás); tras
`return`, `pass`, `break`, `continue` o `raise` quita un nivel; entre `(` y `)` el cierre baja a su línea.
**Cierre automático:** `(`, `[`, `{`, `"` y `'` escriben el par con el cursor en medio; escribir el cierre salta
el que ya está y BackSpace entre un par vacío borra los dos (no cierra comillas tras una letra ni en un
comentario).

### Depurador

Dos teclas y una ventana, sin salir del diseñador:

- **F9** pone o quita un **punto de parada** en la línea del cursor del editor (se marca en amarillo).
- **F5** ejecuta y, si hay puntos de parada, **para** en ellos; **F8** ejecuta **paso a paso**: para en
  la primera sentencia y en cada una de las siguientes.
- Al parar, el editor marca la línea en azul y se abre la ventana **Depurador** con las **variables
  locales** de la función (y, debajo, las globales) y los botones **Continuar (F5)**, **Paso (F8)** y
  **Detener**.
- `vps.depurar()` en el propio código para el programa en ese punto y enseña las variables
  globales (`vps.depurar(locals())` enseña las de la función).

No usa `sys.settrace`: el MicroPython del sistema se compila sin él. En su lugar el código se
**instrumenta**: delante de cada sentencia simple se añade una llamada que decide si hay que parar
(los números de línea de los errores se traducen a los del código original).

### Anclajes y orden de tabulación

- `anclaje="izq|der|arr|abj"` (propiedad de todos los controles): al **redimensionar el formulario**
  (o el Marco que lo contiene) el control se mueve o se estira. Por defecto `izq|arr` (no cambia).
  `der` solo: sigue al borde derecho; `izq|der`: se estira a lo ancho; `abj` y `arr|abj` hacen lo
  mismo en vertical. Pon `redimensionable` en el formulario para que el usuario pueda cambiarlo.
- **Ver > Orden de tabulación...**: una lista de los controles que se sube y se baja; fija la propiedad
  `orden_tab` (1, 2, 3...). El Tab recorre los controles en ese orden (los que no la tienen siguen el
  orden de creación).

### Fuentes

La propiedad `fuente` de **Etiqueta** y **Boton** admite un tamaño y estilos:
`fuente="DejaVu Sans 18 negrita subrayado"`. El tamaño normal es **12**; con otro, el texto se pinta
a 8x16 y se escala con suavizado (9 lo reduce, 24 lo duplica); `negrita` engorda el trazo y
`subrayado` lo subraya. Ver «Límites actuales» para la familia.

### Diseño fiel

El diseñador dibuja cada control **con el mismo código que la aplicación al ejecutarse**: crea
los controles reales de `vps` en un formulario oculto y los pinta sobre la superficie, así que
lo que se ve (colores, relieves, fuentes, texto) es lo que saldrá. Si algo falla con un control, usa
el dibujo aproximado de siempre (Win98).

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
| `fuente` | Tamaño y estilo del texto (Etiqueta y Boton): `"DejaVu Sans 18 negrita"`. Ver «Fuentes» |
| `anclaje` | `izq|der|arr|abj`: qué hace el control al redimensionar el formulario. Por defecto `izq|arr` |
| `orden_tab` | Posición en el orden del Tab (la fija Ver > Orden de tabulación) |
| `pagina` | Solo dentro de unas `Pestanas`: la página del control (0 es la primera) |

Los constructores admiten por posición `(padre, texto, x, y, ancho, alto)` y el resto por nombre.

### Controles

| Control | Propiedades propias | Eventos | Notas |
| --- | --- | --- | --- |
| `Formulario(texto, ancho, alto)` | `texto` (título), `ancho`, `alto`, `color`, `redimensionable`, `resultado`, `datos`, `cancelar` | `cargar`, `cerrando`, `cerrado`, `redimensionar`, `activar`, `desactivar`, `tecla`, `click`, `cerrar` | `mostrar(modal=False, datos=None)`, `ocultar()`, `cerrar(resultado=None)`. Los controles son atributos (`Formulario2.Boton1`). En `Formulario_cerrando`, `Formulario.cancelar = True` impide cerrar (el antiguo `cerrar` devolviendo `False` sigue valiendo). No hay `soltar_archivo`: el sistema aún no tiene arrastrar y soltar archivos |
| `Etiqueta` | `texto`, `alineacion` (izquierda, centro, derecha) | `click` | |
| `Boton` | `texto`, `defecto` (Intro lo pulsa) | `click` | `pulsar()` simula la pulsación |
| `CuadroTexto` | `texto`, `multilinea`, `clave` (muestra `*`), `solo_lectura` | `cambio`, `tecla`, `click`, `intro`, `validar` | `multilinea` se fija al crearlo. `validar` salta al perder el foco; con `CuadroTexto1.cancelar = True` el foco vuelve al cuadro |
| `Casilla` | `texto`, `valor` (`True`/`False`) | `click`, `cambio` | |
| `Opcion` | `texto`, `valor`, `grupo` | `click`, `cambio` | Las del mismo `grupo` en el mismo formulario o Marco son excluyentes |
| `Lista` | `elementos`, `seleccion` (índice o -1), `valor` (texto elegido, solo lectura) | `seleccion`, `doble_click`, `click` | `agregar(t)`, `quitar(i)`, `limpiar()`, `contar()` |
| `Combo` | `elementos`, `texto`, `editable`, `seleccion` | `seleccion`, `cambio` | `agregar(t)`, `limpiar()` |
| `Imagen` | `archivo` (BMP, PNG, PPM), `ajustar` | `click` | `ajustar` reduce por un entero para que quepa |
| `Marco` | `texto` | `click` | Contenedor: los hijos se crean con `Marco` como padre |
| `Deslizador` (`BarraDesplazamiento`) | `valor`, `minimo`, `maximo`, `vertical` | `cambio` | |
| `BarraProgreso` | `valor`, `maximo` | | |
| `Temporizador` | `intervalo` (ms), `activo` | `tic` | Sin ventana: dispara `tic` cada `intervalo` mientras `activo` |
| `Menu` | `elementos`: una línea por título (sin sangría) y por opción (con sangría, `Abrir\|MnuAbrir`; `-` es una línea) | cada opción: `MnuAbrir_click` | Barra de menús del formulario |
| `Pestanas` | `elementos` (títulos), `seleccion` (página que se ve), `valor` (su título) | `cambio`, `click` | Contenedor con páginas: los controles hijos eligen página con `pagina` |
| `Tabla` | `columnas`, `filas` (listas de celdas; en el diseñador una línea `a;b;c` por fila), `seleccion`, `editable`, `ordenable`, `valor` (fila elegida), `columna`, `fila`, `descendente` | `seleccion`, `cambio`, `doble_click`, `click`, `celda_cambio` (`fila`, `columna`), `ordenar` (`columna`, `descendente`) | Clic en la cabecera ordena; doble clic edita. `agregar(fila)`, `quitar(i)`, `limpiar()`, `contar()`, `celda(f, c)`, `poner_celda(f, c, v)`, `ordenar(col, descendente)`, `cargar_csv(ruta)`, `guardar_csv(ruta)` |
| `Numerico` | `valor`, `minimo`, `maximo`, `paso`, `decimales` | `cambio` | Cuadro con flechas; `valor` siempre queda entre el mínimo y el máximo |
| `Lienzo` | `raton_x`, `raton_y` (solo lectura) | `click`, `arrastrar`, `soltar`, `mover` | `linea`, `rectangulo`, `circulo`, `texto`, `imagen`, `limpiar` |
| `Enlace` | `texto`, `url` | `click` | Etiqueta azul y subrayada |
| `BarraEstado` | `texto`, `elementos` (paneles de la derecha) | `click` | `poner(i, texto)` |
| `SelectorColor` | `valor` (`0xRRGGBB`) | `cambio`, `click` | Abre una paleta; `elegir()` la abre por código |
| `Reloj` | `intervalo`, `activo`; solo lectura: `anio`, `mes`, `dia`, `hora`, `minuto`, `segundo`, `dia_semana`, `fecha`, `hora_texto`, `nombre_dia`, `nombre_mes` | `tic` | Sin ventana. `ahora()`, `formatear("%d/%m/%Y %H:%M")` |
| `Archivo` | `ruta`, `separador` | | Sin ventana. `existe()`, `leer()`, `escribir(t)`, `agregar(t)`, `leer_lineas()`, `escribir_lineas(l)`, `leer_csv()`, `escribir_csv(filas)`, `borrar()` |
| `Calendario` | `valor` (o `fecha`, «AAAA-MM-DD»), `anio`, `mes`, `dia`, `minimo` y `maximo` («AAAA-MM-DD», vacío = sin límite) | `cambio`, `doble_click` | Un mes con flechas: `<` `>` cambian de mes y `<<` `>>` de año (también Re Pág y Av Pág); un clic elige el día, un doble clic dispara `doble_click`, las flechas del teclado mueven el día. Los días fuera de `minimo`/`maximo` salen en gris y no se eligen. `hoy()`, `ir_a(anio, mes)`, `mes_visible()`, `en_rango(fecha)` |
| `SelectorFecha` | `valor` («AAAA-MM-DD») | `cambio` | Cuadro con la fecha y un botón que despliega un calendario; `desplegar()` |
| `Arbol` | `nodos` (una línea por nodo, dos espacios de sangría por nivel, `texto\|clave`), `expandido` (si los nodos empiezan abiertos), `seleccion` (índice en el orden de recorrido, -1 si ninguno); `clave` y `valor` (texto) del nodo elegido; `nodo` (el último abierto o cerrado) | `seleccion`, `click`, `doble_click`, `expandir`, `contraer` | Vista en árbol. `agregar(padre, texto, clave=None)` (padre `None` = primer nivel; devuelve la clave), `quitar(clave)` (con sus hijos), `limpiar()`, `expandir(clave=None)`, `contraer(clave=None)`, `alternar(clave)`, `elegir(clave)` (sin eventos), `contar()`, `claves()`, `existe(clave)`, `texto_de(clave)`, `padre_de(clave)`, `hijos(clave=None)`, `ruta(clave=None)` («Trabajo/Reuniones»). Un clic en el cuadrito `[+]`/`[-]` abre o cierra; con el teclado, arriba/abajo recorren, derecha/izquierda abren, cierran o suben al padre, Intro dispara `doble_click` |
| `Notificacion` | `texto`, `tipo` (`info`, `exito`, `aviso`, `error`), `duracion` (ms; 0 = hasta cerrarla), `posicion` (`abajo_derecha`, `arriba_derecha`, `abajo_centro`, `arriba_centro`), `titulo`, `accion` (texto de un botón dentro del aviso); solo lectura: `motivo` | `click`, `accion`, `cerrado` | Sin ventana: `mostrar(texto=None)` enseña un toast con esas propiedades; `cerrar()`. `click` salta al pulsar el aviso, `accion` al pulsar su botón y `cerrado` al irse (`motivo`: `tiempo`, `click`, `boton`, `accion` o `api`) |
| `Grafico` | `tipo` (`lineas`, `area`, `barras`, `barras_h`, `tarta`, `anillo`, `dispersion`, `histograma`, `velas`), `titulo`, `series` (una línea por serie: `nombre: 1; 2; 3`; en dispersión `x,y[,tamaño]`; en velas cuatro series: apertura, máximo, mínimo, cierre), `categorias`, `leyenda` (`ninguna`, `abajo`, `arriba`, `derecha`, `izquierda`), `rejilla`, `animado`, `apilado`, `suave`, `puntos`, `valores`, `formato` (`auto`, `0.0`, `#,##0`, `0%`, `$0.00`...), `colores`, `eje_x`, `eje_y`, `ventana`, `duracion`, `hueco`, `bins`, `minimo`, `maximo`; datos del evento: `serie`, `indice`, `valor`, `categoria`, `nombre_serie`, `oculta` | `click`, `pasar`, `leyenda` | Gráficos con animación, tooltip y leyenda interactiva. `agregar_serie(nombre, datos=None, color=None, xs=None, tam=None)`, `poner_datos(datos, serie=0)`, `agregar_punto(valor, serie=0)` (en vivo), `limpiar()`, `ocultar_serie(i)`, `serie_oculta(i)`, `reiniciar_animacion()`, `valores_de(i)`, `exportar(ruta, ancho=None, alto=None)` (.png o .bmp), `tooltip()` |

### Eventos comunes (v8.4.1, D315)

Todo control con ventana tiene además: `doble_click`, `raton_abajo`, `raton_arriba`, `raton_entra`, `raton_sale`, `raton_mueve`,
`rueda`, `foco_gana`, `foco_pierde`, `tecla_abajo`, `tecla_arriba` y `menu_contextual`. Los datos están en el control
(manejadores sin parámetros): `raton_x`, `raton_y` y `boton` (1 izquierdo, 2 central, 3 derecho) en los de ratón, `rueda_delta`
(positivo hacia arriba) en `rueda`, y `tecla`, `caracter`, `ctrl` y `mayus` en los de teclado. Solo se calculan si hay un
manejador conectado. El sistema no avisa de cuándo se suelta una tecla: `tecla_arriba` se dispara justo después de `tecla_abajo`.
La tabla completa por control y los ejemplos (guardar al cerrar, botón con el ratón, validar al perder el foco) están en el
[manual](visualpythons-manual.md#3-eventos).

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
| `depurar(variables=None)` | Para el programa y enseña las variables en la ventana del depurador |
| `notificar(texto, tipo="info", duracion=3000, titulo="")` | Aviso tipo toast, no modal, en una esquina del formulario activo (abajo a la derecha). `tipo`: `info`, `exito`, `aviso`, `error`; `duracion` en ms (0 = hasta que se cierre). Se apila si hay varios, se va solo, se cierra con un clic o con su «x» y no roba el foco. Otros argumentos por nombre: `accion="Deshacer"` con `al_accion=funcion`, `posicion="arriba_centro"`, `formulario=Formulario2`, `al_click=` y `al_cerrar=`. Devuelve la notificación (`.cerrar()`, `.abierta`, `.motivo`) |
| `notificaciones(formulario=None)` | Las notificaciones que se ven ahora (la más antigua primero) |
| `cerrar_notificaciones(formulario=None)` | Las cierra todas |

### Notificaciones (toast)

`notificar()` y el control `Notificacion` (v8.5.6, D466) dan avisos que no bloquean la ventana, a diferencia de `msgbox`. El
aspecto es el del toast de la libsagui (fondo oscuro del tema y un punto de color por tipo). La libsagui lo tiene en C
(`toast_*`), pero el módulo `sagui` de MicroPython no lo ofrece y añadir nombres pide regenerar los qstr (D034): por eso se dibuja
desde Python con un componente de `tkinter` puesto encima del formulario.

```python
notificar("Guardado", "exito")                                  # se va solo a los 3 segundos
notificar("No se pudo guardar", "error", 0, "Fallo")            # se queda hasta que se cierre
notificar("Nota borrada", accion="Deshacer", al_accion=deshacer)
```

### Gráficos

`Grafico` (v8.5.6, D469) dibuja con el componente de C de la libsagui (`docs/grafico.md`) a través del módulo `sagui_grafico`: el
gráfico pinta en una `Imagen` del tamaño del control y el control la enseña; la animación avanza por tiempo con `after` (no bloquea).

```python
Grafico1 = Grafico(f, 8, 8, 360, 220, tipo="barras", titulo="Ventas por mes", categorias=["Ene", "Feb", "Mar"])
Grafico1.agregar_serie("2026", [120, 150, 90])
def Grafico1_click(): Etiqueta1.texto = "%s: %s" % (Grafico1.categoria, Grafico1.valor)
Grafico4.agregar_punto(carga)          # en vivo: con ventana=30 conserva los últimos 30 puntos
Grafico1.exportar("/home/ventas.png")
```

`tipo` es a la vez la propiedad y el nombre del tipo de control (como en `Notificacion`, D466): vive en el objeto y un cambio por
código (`Grafico1.tipo = "tarta"`) se aplica en menos de 150 ms (un vigilante del control). Ejemplo en Nuevo > «Gráficos».

### Sobre tkinter

`vps` está hecho sobre el `tkinter` del sistema: cada control tiene su componente en `.w`
(`Boton1.w`) por si hace falta algo que `vps` no ofrece. Lo que `tkinter` no tiene (colores por
trozos de texto, tipos de letra vectoriales) tampoco está en `vps`.

## Límites actuales

- **Familia de fuente:** el sistema tiene una sola tipografía de mapa de bits (8x16). `fuente`
  cambia el **tamaño** (escalado suave), la **negrita** y el **subrayado**, pero no la familia
  (se guarda y se ve en el diseñador, y no cambia el dibujo), y no hay cursiva. Las fuentes
  vectoriales de la libfontes (las del WordArt de Pluma) son de C y no llegan a `sagui`: enseñarlas
  pide un nombre nuevo en el módulo y regenerar los qstr de MicroPython (D034), que no se puede sin
  su repositorio. Solo **Etiqueta** y **Boton** aceptan tamaño; el texto de un cuadro de texto, una
  lista o un combo es siempre 8x16.
- **Depurador:** instrumenta `codigo.py`, así que solo para en el código del proyecto (no entra
  en `vps` ni en las bibliotecas), enseña las variables de la función en la que está (no las de las
  funciones que la encierran) y los puntos de parada siguen a las líneas que se añaden o se quitan
  con el teclado pero no a lo que se pega o a lo que cambia un deshacer. No hay «ver expresión».
- **Orden de tabulación:** se aplica reordenando los hijos del contenedor, así que también cambia
  qué control se dibuja encima si dos se solapan.
- Los formularios no pasan de 1000x700 (el tamaño máximo de las ventanas de los programas de
  `tkinter`).
- La `Tabla` (v8.3.2) es una rejilla sencilla de texto: sin columnas redimensionables, sin tipos de dato
  por columna y sin enlace a una base de datos (para eso está la `RejillaDatos`, ver «Bases de datos» al final; BaseDatos, Descarga
  y AgenteIA son de la fase 2, mejora 124). Todavía no hay `Pestanas` anidadas dentro de un `Marco` que
  esté en otra página (los controles de una página oculta se esconden con ella, pero el diseñador solo
  deja elegir la página de las Pestanas de primer nivel).
- Las **Pestanas** y la **Tabla** no están hechas con ventanas del sistema sino con componentes de
  `tkinter` que dibujan ellos mismos (`vps/_widgets.py`): no hay `ttk.Notebook` ni `ttk.Treeview` en el
  `tkinter` del sistema.

## Bases de datos (SQLite y lite-monkey)

Tres componentes de datos (mejora 127, D304): **`ConectorBD`** (sin ventana: `origen`, `tabla`, `filtro`; métodos `abrir`,
`cerrar`, `recargar`, `guardar`; eventos `conectado` y `error`), **`RejillaDatos`** (`fuente=ConectorBD1`: paginación, orden por
columna, edición de celdas y barra de navegación) y los controles enlazados **`CuadroTexto`**, **`Casilla`**, **`Numerico`** y
**`Combo`** con `fuente` y `campo` (fichas «maestro»). Guía con una agenda de contactos paso a paso: [`docs/bases-de-datos.md`](bases-de-datos.md);
el proyecto terminado está en `/ejemplos/vps/agenda-bd`.
