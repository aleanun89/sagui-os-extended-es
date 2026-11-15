# 127 - Bases de datos visuales: conector y rejilla en VisualPythonS y Explorador de bases de datos

- **Estado:** hecha (v8.3.2, rama `v8.3.2-bd`)
- **Prioridad:** media
- **Alta:** 2026-11-15

## Problema / motivación

SaguiOS tiene dos motores de bases de datos (SQLite, D116, y lite-monkey, D117), pero cada uno se usa con su
propia API y no hay forma gráfica de mirar o editar sus datos: hay que escribir SQL en el terminal o código Python.
Una aplicación de VisualPythonS que enseñe una tabla (una agenda, un inventario) obliga a escribir a mano los
bucles de lectura y escritura. El mantenedor pidió un conector visual, un componente y un explorador de bases de
datos para los dos motores.

## Propuesta

- **Biblioteca `basedatos`** (`rootfs/lib/python/basedatos/`): una API pequeña y la misma para SQLite y
  lite-monkey (`conectar`, `tablas`, `esquema`, `consultar`, `contar`, `insertar`, `actualizar`, `borrar`;
  `ejecutar` y transacciones en SQLite). Parámetros siempre enlazados. Funciona en MicroPython y en CPython.
- **Componentes de VisualPythonS** (`vps/_datos.py`): `ConectorBD` (sin ventana), `RejillaDatos` (paginada,
  ordenable, editable, con barra de navegación) y los controles `CuadroTexto`, `Casilla`, `Numerico` y `Combo`
  enlazados con `fuente` y `campo`. En el diseñador: iconos, diálogo del `origen`, combo de tablas y campos, y
  autocompletado. Ejemplo `/ejemplos/vps/agenda-bd`.
- **Explorador de bases de datos** (`basedatos/explorador.py`, Inicio > Accesorios; doble clic en un `.db` de
  Archivos): árbol de conexiones, tablas, columnas e índices; pestañas Datos, Estructura y Consulta; crear tabla,
  exportar a CSV y abrir en Macaco; ofrece arrancar el servicio lite-monkey.
- **Seguridad:** `DROP` y `DELETE`/`UPDATE` sin `WHERE` piden confirmación; el explorador no ejecuta nada al abrir
  un fichero.
- Guía `docs/bases-de-datos.md`; decisión D304.

## Criterios de aceptación

- [x] Pruebas de unidad de `basedatos` en el anfitrión (CPython, `sqlite3` y el servidor `litemonkey` del PC)
- [x] `tests/h304_bd_visual.py` en QEMU: autoprueba de la biblioteca y los componentes, el Explorador (abrir,
      árbol, editar una celda y comprobarla en la base, consulta, confirmación, exportar CSV y abrir en Macaco),
      lite-monkey (arrancar el servicio y ver documentos) y la agenda de VisualPythonS
- [x] Decisión registrada en `docs/decisoes.md` (D304)

## Notas

- `fuente` ya era la propiedad de la fuente tipográfica en los controles de VisualPythonS; en los controles
  enlazados (`CuadroTexto`, `Casilla`, `Numerico`, `Combo`) y en `RejillaDatos` pasa a ser el conector (que no
  usan para otra cosa). En los demás controles sigue siendo la tipografía.
- No hay transacciones en lite-monkey ni consultas SQL: el explorador usa en su lugar un filtro JSON.
