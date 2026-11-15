# 128 - GebraMini: geometría dinámica y funciones

- **Estado:** hecha (**v8.3.2**, D309)
- **Prioridad:** media
- **Alta:** 2026-11-15
- **Relacionadas:** 078 (Macaco), 116 (Pluma, aspecto Win98), 103 (diálogos de archivo, D272), D032 (FPU)

## Motivación
El mantenedor quiere «una especie de GeoGebra propio»: construir con el ratón puntos, rectas, circunferencias
y polígonos que dependan unos de otros (el punto medio sigue a sus extremos, la intersección se recalcula),
dibujar funciones y escribir órdenes. Parte de su prototipo **GebraMini** (C++/SDL, GPL-3.0-or-later,
© 2026 Alejandro Anuncibay), que ya tenía plano, cuadrícula, puntos arrastrables, segmentos, circunferencias,
zoom y desplazamiento. Inspirado en la idea de esos programas, independiente: sin código, iconos ni marcas de
terceros.

## Hecho
- Aplicación nativa `gebramini` (C sobre la libsagui, estilo Win98): vista gráfica con ejes graduados,
  cuadrícula, zoom a la rueda hacia el cursor, desplazamiento y «Ver todo»; antialias en líneas, curvas y puntos.
- 22 herramientas en una barra con botones conmutables: mover, punto, segmento, recta, semirrecta, vector,
  circunferencia (centro y punto / centro y radio), punto medio, intersección, perpendicular, paralela,
  mediatriz, bisectriz, polígono, polígono regular, ángulo, distancia, área, texto, deslizador y borrar.
- Geometría dinámica con grafo de dependencias y recálculo ordenado; puntos sobre objetos que se deslizan.
- Vista algebraica (ocultar, mostrar, renombrar, propiedades) y barra de entrada con órdenes en español,
  expresiones (`A = (1, 2)`, `B = A + (3, 0)`, `f(x) = sin(x) + x^2/4`, `a = 2`), autocompletado y historial.
- Funciones con muestreo adaptativo y cortes en las discontinuidades; derivada simbólica; raíces, extremos y
  cortes numéricos.
- Ficheros `.gbm` (texto legible), PNG y PDF vectorial; deshacer y rehacer; cuatro ejemplos en
  `/ejemplos/gebramini/`.
- Entrada en el menú Inicio («Educación») con icono propio.

## Pendiente (ideas)
Cónicas implícitas, tangentes, transformaciones (simetría, giro, traslación), lugares geométricos, rastro,
animación de deslizadores, tablas de valores, rango de búsqueda de raíces según la vista.

## Criterios de aceptación
- [x] Pruebas de unidad en el anfitrión (`tests/gebramini_unidad.c`)
- [x] Test automático `tests/h309_gebramini.py` (ratón, órdenes, ficheros, PDF)
- [x] Decisión D309 en `docs/decisoes.md` y manual `docs/gebramini.md`
