# 123 - VisualPythonS: un entorno al estilo del Visual Basic clásico con Python

- **Estado:** hecha (v8.3.1, rama `v8.3.1-visualpython`)
- **Prioridad:** media
- **Alta:** 2026-11-15

## Problema / motivación

Hacer una aplicación sencilla con ventana en SaguiOS pide C y la libsagui, o escribir `tkinter` a mano. El Visual Basic clásico
demostró que dibujar el formulario y escribir una función por evento basta para la mayoría de las aplicaciones pequeñas. El
mantenedor pidió ese estilo, con Python y más sencillo que el VB.

## Propuesta

- Módulo `vps` (`rootfs/lib/python/vps/`): `Formulario` y los controles `Etiqueta`, `Boton`, `CuadroTexto`, `Casilla`, `Opcion`,
  `Lista`, `Combo`, `Imagen`, `Marco`, `Deslizador`/`BarraDesplazamiento`, `BarraProgreso`, `Temporizador` y `Menu`, con propiedades
  sencillas en español y eventos por convención (`Boton1_click`) o con `@evento`. Utilidades `msgbox`, `inputbox` y los diálogos de
  archivos de la D272 (`sagui.dialogo`).
- Diseñador (`vps/disenador.py`, en Inicio > Accesorios) con aspecto Win98: caja de herramientas, formulario con rejilla, tiradores,
  selección múltiple, alinear, copiar y pegar, ventana de propiedades, editor de código con resaltado y combos «control» y «evento»,
  F5 con salto a la línea del error, proyectos (`formulario.vpf` + `codigo.py`) y **Generar aplicación** (Inicio > Mis aplicaciones).
- Plantillas y ejemplos en `/ejemplos/vps/` (Hola mundo, Calculadora, Lista de tareas, Cronómetro, Visor de imágenes).
- Guía `docs/visualpythons.md` con el tutorial de 5 minutos y la referencia; decisión D297.

## Criterios de aceptación

- [x] Un script crea un formulario con un botón y el evento cambia una etiqueta (sin diseñador)
- [x] Colocar un botón, escribir el evento, ejecutar, pulsar y comprobar; guardar y reabrir el proyecto
- [x] Generar la aplicación y lanzarla desde su entrada del menú Inicio
- [x] Los errores de Python se muestran con la línea y el cursor salta a ella
- [x] Test automático `tests/h297_visualpython.py` y capturas `docs/capturas/H297-*.png`
- [x] Decisión D297 en `docs/decisoes.md`

## Notas

- Hecho sobre `tkinter` (D084), sin controles nuevos en C. Cambios fuera de `rootfs/lib/python/vps/`: `sagui.dialogo` en
  `janela_sagui.c`, la entrada del menú Inicio y «Mis aplicaciones» en `barra.c`, el icono en `arte.c`, y un resaltado opcional
  en el `Text` de `tkinter`.
- Pendiente: deshacer, `fuente` real (hoy una sola fuente 8x16), icono de las aplicaciones en el escritorio, refactorizar el código al
  renombrar un control, ayuda contextual, y arrastrar con previsualización desde la caja.

## Ampliación v8.5.6 (D466): notificaciones toast, `Arbol` y `Calendario` ampliado

- **Notificaciones**: `notificar(texto, tipo, duracion, titulo)` y el control sin ventana `Notificacion` (texto, tipo, duracion, posicion,
  titulo, accion; `mostrar()`; eventos `click`, `accion`, `cerrado`). Avisos no modales en una esquina del formulario, apilados, con «x» y clic
  para cerrarlos, sin robar el foco. Se dibujan en Python (`vps/_notificar.py`) con el aspecto del toast de la libsagui, porque el módulo
  `sagui` no lo exporta y D034 impide añadir nombres sin regenerar los qstr.
- **`Arbol`**: vista en árbol (`agregar`, `quitar`, `limpiar`, `expandir`, `contraer`, `seleccion`/`valor`/`clave`; eventos `seleccion`,
  `doble_click`, `expandir`, `contraer`), dibujada en `vps/_widgets.py` (`_ArbolW`).
- **`Calendario`**: `minimo`, `maximo`, flechas de año, `doble_click` y teclado.
- Ejemplos `notificaciones` y `agenda-carpetas`; prueba `tests/h466_vps_toast.py`; decisión D466.
- Quedan fuera de «Límites actuales» `Calendario` y `Arbol`.
