# 131 — Dos aplicaciones de bases de datos: SQLite (relacional) y lite-monkey (documental) (v8.4.4)

- **Estado:** pendiente (v8.4.4)
- **Prioridad:** alta
- **Alta:** 2026-11-15

## Problema

El Explorador de bases de datos de la v8.3.2 (mejora 127, D304, `rootfs/lib/python/basedatos/explorador.py`)
trata igual una base relacional (SQLite) y una documental (lite-monkey, D117), y el mantenedor lo encuentra
«raro». La idea le parece bien, pero cada tipo se edita y se usa de otra manera. Pide **dos aplicaciones**
(2026-11-15).

## Propuesta

La biblioteca común `basedatos` y los componentes de VisualPythonS (`ConectorBD`, `RejillaDatos`) se quedan; lo
que se parte es la aplicación.

### 1. Explorador SQLite (relacional)

Inspirado en el estilo de DB Browser for SQLite:

- **Árbol:** tablas, vistas, índices y disparadores.
- **Pestaña Datos:** rejilla editable con filtro por columna, orden y paginación; NULL distinguido del texto vacío.
- **Pestaña Estructura:**
  - crear y modificar tablas con un asistente de columnas: tipo, PK, NOT NULL, DEFAULT, UNIQUE y FK;
  - índices.
- **Pestaña SQL:**
  - editor con resaltado y autocompletado de tablas y columnas;
  - varias sentencias;
  - resultado en rejilla y tiempo de ejecución;
  - `EXPLAIN QUERY PLAN`;
  - historial.
- **Importar y exportar:** CSV y SQL (volcado), y abrir en Macaco.
- **Transacciones:** «Escribir cambios» y «Deshacer cambios».
- **Integridad:** `PRAGMA integrity_check`, compactar (`VACUUM`).

### 2. Explorador lite-monkey (documental)

Tomando como ejemplo de uso MongoDB Compass (nombre y aspecto propios):

- **Conexión** al servicio (puerto 7979; ofrece arrancarlo si está parado), con la lista de bases y colecciones con
  su número de documentos y su tamaño.
- **Documentos**, con tres vistas:
  - lista de documentos como árbol plegable;
  - JSON;
  - tabla, con las claves de primer nivel como columnas.
- **Barra de consulta:** filtro como documento (`{"edad": {"$gt": 30}}`), proyección, orden, saltar y límite,
  con historial y consultas favoritas.
- **Editar:**
  - un documento en su sitio (árbol o JSON, con validación antes de guardar);
  - insertar (JSON o formulario);
  - clonar y borrar (con confirmación);
  - actualizar o borrar los que casan con el filtro, enseñando antes cuántos son.
- **Esquema:** muestreo de la colección; por campo, sus tipos con porcentajes, valores frecuentes y huecos
  (campos que faltan).
- **Índices:** listar, crear y borrar.
- **Agregación:** constructor de etapas (`$match`, `$group`, `$sort`, `$project`, `$limit`) con la vista previa
  de cada etapa, si lite-monkey las soporta. Comprobarlo antes en D117 y, si falta, anotarlo como mejora del
  motor, no inventarlo en la aplicación.
- **Importar y exportar:** JSON y JSON por líneas (y CSV aplanando).

### Común

- Dos entradas en Inicio > Accesorios con iconos distintos.
- Doble clic en un `.db` o `.sqlite` abre el de SQLite.
- El explorador actual se queda solo como alias del de SQLite, o se retira, sin romper h-pruebas ni ejemplos.
- Seguridad (D304): confirmación en `DROP` y en `DELETE`/`UPDATE` sin `WHERE`, y en borrados masivos de documentos.
- Multiidioma (D313): textos con `_()`.

## Pruebas

- Una prueba hNN por aplicación, con una sesión de ratón y teclado en QEMU:
  - SQLite: crear tabla, insertar, filtrar, consulta SQL y exportar CSV;
  - lite-monkey: insertar un documento, filtrar con `$gt`, editar en JSON, ver el esquema y crear un índice.
- Capturas.
- Las pruebas de la mejora 127 siguen pasando.
