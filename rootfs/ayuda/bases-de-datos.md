# Bases de datos en SaguiOS: `basedatos`, VisualPythonS y el Explorador

SaguiOS trae dos motores de bases de datos: **SQLite** (relacional, un fichero `.db`; D116, `docs/sqlite.md`) y
**lite-monkey** (de documentos, un servidor en el puerto 7979; D117, `docs/litemonkey.md`). Esta guía cuenta las tres
piezas que los manejan por igual (mejora 127, D304):

1. la biblioteca de Python **`basedatos`**: una API pequeña y la misma para los dos motores;
2. los componentes de **VisualPythonS**: `ConectorBD`, `RejillaDatos` y los controles enlazados;
3. el **Explorador de bases de datos** (Inicio > Accesorios): una aplicación para mirar y editar.

## 1. La biblioteca `basedatos`

```python
import basedatos

bd = basedatos.conectar("sqlite:/home/datos.db")                 # o ":memory:"
bd = basedatos.conectar("litemonkey://127.0.0.1:7979/mibase")    # lite-monkey
```

La cadena de conexión: `sqlite:/ruta/fichero.db` (también `sqlite:///ruta`, `:memory:` o una ruta que acabe en `.db`) y
`litemonkey://[usuario:clave@]host[:puerto]/base` (por defecto `127.0.0.1:7979` y la base `test`). Funciona en el
`python` de SaguiOS y en CPython (para probar en el PC: pon `rootfs/lib/python` y `litemonkey/python` en el `sys.path`).

### La API, en diez líneas

```python
bd.tablas()            # nombres de las tablas (SQLite) o colecciones (lite-monkey; también bd.colecciones())
bd.esquema(t)          # [{"nombre", "tipo", "clave", "obligatorio", "defecto"}]; en lite-monkey, los campos deducidos de una muestra
bd.consultar(t, filtro, orden, limite, desplazamiento)   # lista de diccionarios
bd.contar(t, filtro)                                      # cuántas filas cumplen el filtro
bd.insertar(t, {"campo": valor})                          # devuelve la clave de la fila nueva
bd.actualizar(t, {"campo": valor}, filtro=...)  # o clave=...; devuelve cuántas filas cambió
bd.borrar(t, filtro=...)                        # o clave=...; sin filtro ni clave se niega (salvo todos=True)
bd.ejecutar("SELECT * FROM t WHERE id = ?", [3])   # solo SQLite: parámetros enlazados, nunca concatenados
with bd.transaccion(): ...                         # solo SQLite (también bd.empezar() / confirmar() / revertir())
bd.cerrar()                                        # o `with basedatos.conectar(...) as bd:`
```

Además: `bd.indices(t)`, `bd.clave(t)` (la columna que identifica una fila: la clave primaria, o `rowid`; `_id` en
lite-monkey), `bd.columnas(t)` (en orden), `bd.crear_tabla(nombre, [(columna, tipo, clave)])`, `bd.borrar_tabla(nombre)` y,
en SQLite, `bd.sql(sentencia, parametros)` -> `(columnas, filas, afectadas)` (la usa el Explorador).

### Filtros

El mismo diccionario sirve en los dos motores (el de lite-monkey es el suyo; en SQLite se traduce a `WHERE` con
parámetros enlazados):

```python
{"ciudad": "Lima"}                                   # igualdad ({"campo": None} es «sin valor»)
{"edad": {"$gte": 18, "$lt": 65}}                    # $eq $ne $gt $gte $lt $lte
{"ciudad": {"$in": ["Lima", "Quito"]}}               # $in $nin
{"nombre": {"$contiene": "ana"}}                     # contiene (sin distinguir mayúsculas)
{"$or": [{"ciudad": "Lima"}, {"edad": {"$gt": 60}}]} # $and $or
```

Solo en SQLite también: una tupla `("edad > ? AND nombre LIKE ?", [18, "A%"])` (condición con parámetros) o un texto `"edad > 18"`
(la condición tal cual: para código del programador, **nunca** para lo que escribe un usuario). En lite-monkey, un texto es el
JSON del filtro: `'{"edad": {"$gt": 18}}'`. Un `_id` escrito como 24 cifras hexadecimales se convierte en `ObjectId`.

`orden` es `"nombre"`, `"-nombre"` (descendente), `["ciudad", "-edad"]` o `[("edad", -1)]`.

### Ejemplo con SQLite

```python
import basedatos
bd = basedatos.conectar("sqlite:/home/ejemplo.db")
bd.ejecutar("CREATE TABLE contactos (id INTEGER PRIMARY KEY, nombre TEXT NOT NULL, ciudad TEXT, edad INTEGER)")
with bd.transaccion():                                  # o todo o nada
    bd.insertar("contactos", {"nombre": "O'Brien", "ciudad": "Lima", "edad": 41})   # la comilla no rompe nada
    bd.insertar("contactos", {"nombre": "Ana", "ciudad": "Madrid", "edad": 30})
for c in bd.consultar("contactos", {"edad": {"$gt": 35}}, orden="nombre", limite=10):
    print(c["id"], c["nombre"])
bd.actualizar("contactos", {"ciudad": "Quito"}, clave=1)
print(bd.ejecutar("SELECT ciudad, count(*) AS n FROM contactos GROUP BY ciudad"))
bd.cerrar()
```

`/ejemplos/bd/crear_ejemplo.py` crea una base de ejemplo con 60 contactos y una tabla de productos.

### Ejemplo con lite-monkey

```python
import basedatos
bd = basedatos.conectar("litemonkey://127.0.0.1:7979/tienda")   # antes: servicio iniciar litemonkey
bd.insertar("productos", {"nombre": "café", "precio": 4.5, "tags": ["bebida", "caliente"]})
bd.insertar("productos", {"nombre": "pan", "precio": 1.2})
print(bd.esquema("productos"))        # campos deducidos de una muestra: nombre texto, precio real, tags lista...
print(bd.consultar("productos", {"precio": {"$lt": 4}}, orden="-precio"))
bd.actualizar("productos", {"precio": 1.3}, {"nombre": "pan"})
bd.cerrar()
```

Los documentos pueden tener campos distintos: `esquema` mira hasta 200 y dice el tipo de cada campo (`entero`, `real`, `texto`,
`logico`, `lista`, `documento`, `objectid`...; si un campo cambia de tipo, `entero|texto`) y en qué porcentaje de
documentos aparece. `/ejemplos/bd/lmk_ejemplo.py` inserta tres documentos de ejemplo.

### Errores

Todos heredan de `basedatos.ErrorBD`: `ErrorConexion` (no se abre el fichero o el servidor no contesta),
`ErrorConsulta` (SQL mal escrito, tabla que no existe, filtro no válido, `ejecutar()` en lite-monkey) y
`ErrorIntegridad` (clave repetida, `NOT NULL`...; es una `ErrorConsulta`). Los mensajes son claros:
`no se pudo conectar con lite-monkey en 127.0.0.1:7979 (...)`. Un error en una sentencia deshace la transacción
implícita que abrió, para que la siguiente no choque con ella.

### Seguridad

Los valores **nunca** se concatenan al SQL: van como parámetros enlazados (`?`). Los nombres de tablas y columnas se
entrecomillan, así que un nombre con comillas o con `; DROP TABLE` no es SQL. Un `actualizar` o un `borrar` sin filtro ni
clave se niega, para no vaciar una tabla por un descuido.

## 2. Componentes de VisualPythonS

Los componentes de datos están en el módulo `vps` (`from vps import *`) y en la caja de herramientas del diseñador.

### `ConectorBD` (sin ventana, como el `Temporizador`)

| Propiedad | Qué es |
|---|---|
| `origen` | la cadena de conexión; en el diseñador, un diálogo para elegir un fichero `.db` o escribir el servidor de lite-monkey |
| `tabla` | la tabla o colección; en el diseñador, un menú con las que tiene la base |
| `filtro` | condición SQL (el `WHERE` sin la palabra) en SQLite, JSON en lite-monkey |
| `orden` | `"nombre"` o `"-nombre"` (varios, separados por comas) |
| `tam_pagina` | filas por página (50) |

Métodos: `abrir()`, `cerrar()`, `recargar()`, `guardar()`, `nuevo()`, `borrar()`, `primero()`, `anterior()`, `siguiente()`,
`ultimo()`, `ir_a(posicion)`, `ordenar(campo)`, `valor(campo)`, `poner(campo, valor)` y `tablas()`. Datos: `registro` (el
diccionario del registro actual), `filas` (la página), `columnas`, `total`, `posicion`, `pagina`, `paginas`, `modificado`,
`abierta`, `error` y `bd` (el objeto de `basedatos`, para consultas propias).

Eventos: **`conectado`** (se abrió y cargó) y **`error`** (con el texto en `error`; si no hay manejador, sale un mensaje).
También `registro` (cambió el registro actual) y `datos` (se cargó otra página).

El conector abre solo al empezar si tiene `origen`; si no, `ConectorBD1.abrir()`. **Moverse guarda**: si el registro tiene
cambios sin guardar, `siguiente()`, `ir_a()` o `nuevo()` los guardan antes (un fallo se queda en el registro y dispara `error`).

### `RejillaDatos`

Se enlaza con `fuente=ConectorBD1` (en el diseñador, un menú con los conectores del formulario). Enseña la página actual, con
paginación para tablas grandes, **orden por columna** (un clic en la cabecera; otro, para invertirlo) y una **barra** con
primero, anterior, siguiente, último, nuevo, borrar (con confirmación) y guardar. Con `editable=True`, **doble clic en una celda**
(o escribir sobre ella) la edita y **escribe en la base**; la columna clave no se edita. Eventos: `seleccion` y `cambio`.

### Controles enlazados

`CuadroTexto`, `Casilla`, `Numerico` y `Combo` aceptan `fuente` (el conector) y `campo`: al moverse por la rejilla o por la
barra se **rellenan** con el registro actual y, cuando el usuario los cambia, **escriben** en él; `guardar()` (o moverse) lo lleva a la
base. Así se hacen las fichas («maestro»). Un texto vacío en una columna numérica es `NULL`; el texto se convierte al tipo de la
columna (`INTEGER` a entero, `REAL` a número...).

> En estos controles, `fuente` es el conector (en el resto sigue siendo la tipografía).

### Una agenda de contactos con rejilla y ficha, en 5 minutos

El proyecto terminado está en `/ejemplos/vps/agenda-bd` (ábrelo desde el diseñador, Archivo > Abrir proyecto).

![La agenda de contactos](capturas/H304-9-agenda.png)

1. **Inicio > Accesorios > VisualPythonS (diseñador)**; formulario nuevo de 540 x 372.
2. Caja de herramientas: elige **ConectorBD** y pulsa en una esquina del formulario. En propiedades, `origen`: se abre el
   diálogo; pulsa **Elegir fichero...** y busca tu `.db` (o escribe `127.0.0.1:7979/base` para lite-monkey). Después, `tabla`:
   un menú con las tablas de la base; elige `contactos`.
3. Elige **RejillaDatos** y dibújala en la mitad de arriba. En `fuente` elige `ConectorBD1` y pon `editable` a `True`.
4. Pon un **Marco** («Ficha») debajo y dentro tres controles: un **CuadroTexto** con `fuente = ConectorBD1` y
   `campo = nombre` (el menú de `campo` enseña las columnas de la tabla), otro con `campo = telefono` y un **Combo** con
   `campo = ciudad`; una **Casilla** con `campo = favorito`.
5. Escribe el código (doble clic en el formulario): abre el conector al cargar.

```python
def Formulario_cargar():
    ConectorBD1.abrir()

def ConectorBD1_error():
    Etiqueta4.texto = "Error: " + ConectorBD1.error
```

6. **F5**. Pulsa filas de la rejilla: la ficha se rellena; escribe en la ficha y pulsa **Guardar** de la barra (o cambia de fila:
   se guarda sola); **Nuevo** vacía la ficha para un contacto nuevo; doble clic en una celda de la rejilla la edita.

Con **Generar aplicación** queda en Inicio > Mis aplicaciones.

### En el diseñador

Los componentes tienen su icono en la caja de herramientas; `origen` abre su diálogo, `tabla` y `campo` sacan un menú con lo que hay
en la base (el diseñador se conecta un momento para leerlo) y `fuente` uno con los conectores del formulario. El autocompletado
conoce los métodos (`ConectorBD1.` ofrece `abrir`, `guardar`, `siguiente`...). La rejilla se dibuja en el diseño como una maqueta.

## 3. El Explorador de bases de datos

**Inicio > Accesorios > Explorador de bases de datos**, o un doble clic en un `.db` (o `.sqlite`) en Archivos. También
`python /lib/python/basedatos/explorador.py [fichero.db | litemonkey://host:7979/base]`.

![El Explorador con una base SQLite](capturas/H304-1-explorador.png)

**Árbol (a la izquierda).** Las conexiones (las que abriste otras veces se recuerdan en `/home/.config/exploradorbd.json`) ->
tablas o colecciones -> columnas e índices. Un clic en una tabla enseña sus datos; doble clic en una conexión la abre o la cierra y doble clic
en una tabla despliega sus columnas y sus índices; el botón derecho abre un menú (abrir, cerrar, quitar, nueva tabla, exportar...).

![El árbol con las columnas de una tabla](capturas/H304-3-arbol.png)

**Pestaña Datos.** La tabla con **paginación** (50 filas por página), **filtro** (condición SQL en SQLite; JSON en lite-monkey) y
**orden** (un clic en la cabecera). **Doble clic en una celda para editarla** (Intro acepta, Esc cancela): se escribe en la base con
parámetros enlazados. **Fila nueva...** abre un formulario con un campo por columna; **Borrar fila** pide confirmación.

![Datos: editar una celda y ordenar](capturas/H304-4-editar-ordenar.png)

**Pestaña Estructura.** Las columnas con su tipo, clave, si son obligatorias y su valor por defecto, y los índices. En
lite-monkey, los campos deducidos de una muestra con el porcentaje de documentos que los tienen.

![Estructura de una tabla](capturas/H304-5-estructura.png)

**Pestaña Consulta.** Un editor de **SQL** con resaltado sencillo (palabras clave, cadenas, números y comentarios). **F5** ejecuta
(o el botón); el resultado sale en una rejilla con el número de filas y el tiempo (`4 filas en 12 ms`), y **Historial** recuerda las
últimas 30 consultas. Se pueden escribir varias sentencias separadas por `;`. En lite-monkey, la pestaña recibe el **filtro en
JSON** con la sintaxis del cliente (`{"precio": {"$lt": 4}}`) y muestra los documentos de la colección elegida.

![Consulta SQL con su resultado](capturas/H304-6-consulta.png)

**Tabla > Nueva tabla...** crea una tabla (nombre y hasta cinco columnas con su tipo y clave; en lite-monkey, una colección vacía);
**Exportar a CSV...** guarda la tabla (con el filtro y el orden) o, con el botón de la pestaña Consulta, el resultado; después ofrece
**abrirlo en Macaco**. **Borrar tabla** pide confirmación.

![El CSV exportado, en Macaco](capturas/H304-8-macaco.png)

### lite-monkey

**Archivo > Conectar a lite-monkey...** pide el servidor (`127.0.0.1:7979`) y la base. Si el servidor no está arrancado, el
Explorador lo dice y **ofrece arrancarlo** con el gestor de servicios (`servicio iniciar litemonkey`); si el servicio no
arranca (por ejemplo, porque no hay `/home`), lanza `litemonkey &` a mano (sus datos van a `/home/litemonkey` o, sin `/home`, a `/tmp/litemonkey`). Después se ven las colecciones de la base; los documentos con campos distintos
salen en una sola rejilla con la unión de sus campos.

![lite-monkey en el Explorador](capturas/H304-10-lmk.png)

### Seguridad del Explorador

- Una sentencia **`DROP`**, o un **`DELETE`/`UPDATE` sin `WHERE`**, pide confirmación (con la sentencia delante) antes de
  ejecutarse; contestar que no la cancela sin tocar nada.

  ![Confirmación de un DELETE sin WHERE](capturas/H304-7-confirmar.png)
- Al **abrir un fichero no se ejecuta nada**: solo se leen la lista de tablas y su esquema.
- Los cambios de la rejilla usan parámetros enlazados, igual que `basedatos`.

## 4. Pruebas

- `python3 tests/basedatos_unidad.py`: pruebas de unidad de `basedatos` en el PC (CPython, `sqlite3` y el servidor
  `litemonkey` de `make -C litemonkey`): CRUD, filtros, parámetros hostiles, transacciones, conversiones, análisis de SQL peligroso.
- `tests/h304_bd_visual.py` (QEMU): la autoprueba de `basedatos` y de los componentes dentro de SaguiOS
  (`python /lib/python/basedatos/autoprueba.py`), el Explorador con el ratón (abrir, árbol, editar una celda y comprobarla en la
  base, estructura, consulta, confirmación, CSV y Macaco), lite-monkey (arrancar el servicio y ver los documentos) y la agenda de
  VisualPythonS. Capturas en `docs/capturas/H304-*.png`.
