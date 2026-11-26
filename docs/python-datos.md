# Python para datos: ulab, «tablas» y los compiladores native y viper (D479, v8.5.6.1)

Manual corto. Ejemplos en `/ejemplos/datos/` (`python /ejemplos/datos/<nombre>.py`); prueba `tests/h479_python_datos.py`.

## ulab: arrays, estadística, álgebra lineal y FFT
`ulab` (MIT) es el «NumPy» de MicroPython y va dentro del `python` del sistema.

```python
from ulab import numpy as np
a = np.array([2, 4, 4, 4, 5, 5, 7, 9], dtype=np.float)
np.mean(a), np.median(a), np.std(a)             # 5.0  4.5  2.0
A = np.array([[2.0, 1.0], [1.0, 3.0]])
np.dot(np.linalg.inv(A), np.array([5.0, 10.0])) # array([1.0, 3.0])
z = np.fft.fft(np.sin(2 * 3.14159265 * 5 * np.arange(64) / 64))
abs(z)                                          # el módulo del espectro (pico en 5)
```
Hay `numpy` (arange, linspace, sort, dot, polyfit, interp, trapz, where, linalg.det/inv/eig/cholesky..., fft), `scipy`
(optimize, signal, integrate, linalg, special) y `utils` (spectrogram). Con complejos, `fft` devuelve un array complejo
(`dtype=np.complex`; para el módulo, `abs(z)`; `np.real` y `np.imag`). Hasta 3 dimensiones. Sin `numpy.load/save/loadtxt/savetxt`
(no hay VFS en el MicroPython del sistema): para ficheros, el módulo «tablas».

## tablas: un «DataFrame» ligero
```python
from tablas import Tabla, leer_csv, leer_xlsx, leer_sqlite
v = leer_csv("/ejemplos/datos/ventas.csv")     # detecta , ; o tabulador; int/float/texto; vacío = falta (None)
v.forma, v.columnas, v.contar_nulos(), v.cabeza(), v.describir()
v.filtrar(lambda f: f["unidades"] > 30)        # o v.donde("unidades", ">", 30) (==, !=, >, >=, <, <=, en, contiene)
v.ordenar(["region", "unidades"], descendente=True)
v.groupby("region").agg({"unidades": "suma", "precio": ["media", "max"]})   # suma media mediana min max std contar
v.merge(productos, en="producto", como="left") # inner, left, right, outer
v.pivot("region", "mes", "unidades", agg="suma")
v.rellenar({"unidades": "media", "precio": 2.5}) ; v.quitar_nulos()
v["total"] = lambda f: f["unidades"] * f["precio"]
v.columna("unidades")                          # la columna numérica como array de ulab
```
Las operaciones devuelven una tabla nueva (`poner`/`agregar` cambian en el sitio). Los valores que faltan son `None` o `NaN`; las
estadísticas los saltan.

**Formatos.** `leer_csv`/`a_csv`, `leer_json`/`a_json` (lista de objetos o objeto de listas), `leer_xlsx`/`a_xlsx` y
`leer_mxs`/`a_mxs` (los hace Macaco con `macaco --convertir`: primera hoja, primera fila = nombres), `leer_sqlite(ruta, tabla o SELECT, parámetros)`
y `a_sqlite(ruta, tabla, si_existe="reemplazar"|"agregar"|"fallar")` (con el `sqlite3` del sistema).

**Valkey (opcional).** `a_valkey(clave)` y `leer_valkey(clave)` guardan y leen la tabla como JSON en una clave si existe el módulo cliente
`valkey` (API tipo `redis-py`: `Valkey(host, port)`, `set`, `get`); si no, levantan `TablasNoDisponible` («Valkey no está disponible»).
`valkey_disponible()` lo dice sin probar; también se acepta `cliente=` con cualquier objeto con `set` y `get`.

**Gráficos** (sobre `sagui_grafico`, D469): `t.grafico("barras", x="region", y="total", titulo="...", ruta="/tmp/g.png")` exporta a
PNG o BMP; con `ventana=True` abre una ventana (redimensionable, resalta con el ratón) y espera a que se cierre. Tipos: barras,
barras_h, lineas, area, dispersion (`x` y `y` numéricas), histograma (`y`), tarta, anillo, velas.

## @micropython.native y @micropython.viper
Activados en x86 (el emisor x86 de MicroPython). `native` compila la función a código máquina con los mismos objetos de Python;
`viper` usa enteros de 32 bits y punteros (`ptr8`, `ptr16`, `ptr32`) sin objetos: mucho más rápido, pero los enteros se desbordan.
Medida (KVM, bucle de 30 millones de pasos con suma y máscara): Python 900 ms, `native` 370 ms (2,4 veces), `viper` 30 ms (30 veces).
Ejemplo: `/ejemplos/datos/rendimiento.py`.
```python
@micropython.viper
def llenar(buf: ptr8, n: int, valor: int):
    i = 0
    while i < n:
        buf[i] = (valor + i) & 255
        i += 1
```
