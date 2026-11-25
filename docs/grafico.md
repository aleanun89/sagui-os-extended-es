# Gráficos: componente de la libsagui, módulo `sagui_grafico` y control `Grafico` (D469, v8.5.6)

Un componente de gráficos **reutilizable**: en C dentro de la libsagui (`userland/inc/grafico.h`,
`userland/libsagui/gui_grafico.c` y la parte de cálculo `grafico_calc.c`), expuesto a MicroPython como el módulo
`sagui_grafico` (`userland/micropython/grafico_sagui.c`) y usado por el control `Grafico` de VisualPythonS.

## Tipos
`GR_LINEAS` (con o sin puntos, curva suave opcional), `GR_AREA` (apilada o no), `GR_BARRAS` y `GR_BARRAS_H` (agrupadas y
apiladas), `GR_TARTA`, `GR_ANILLO` (con el total en el centro), `GR_DISPERSION` (x, y y tamaño), `GR_HISTOGRAMA` (intervalos
por Sturges o `GR_O_BINS`) y `GR_VELAS` (cuatro series en este orden: apertura, máximo, mínimo, cierre). Hasta 12 series y
512 puntos por serie.

## API C (`grafico.h`, solo tipos básicos de C)
```c
grafico_t *g = grafico_nuevo();
grafico_tipo(g, GR_BARRAS);  grafico_titulo(g, "Ventas");
grafico_categoria_agregar(g, "Ene");  /* ... */
int s = grafico_agregar_serie(g, "2026", 0 /* color de la paleta */);
double v[] = { 3, 5, 2 };  grafico_datos(g, s, v, 3);          /* sustituye y anima */
grafico_opcion(g, GR_O_LEYENDA, GR_LEY_DERECHA);                /* también REJILLA, ANIMADO, APILADO, SUAVE, PUNTOS... */
grafico_formato(g, "#,##0");                                    /* 0, 0.0, #,##0, 0%, 0.0%, $0.00, «0 kg», auto */
/* en cada tic de la ventana: */
if (grafico_avanzar(g, tempo_ms()) || grafico_sucio(g)) grafico_dibujar(g, j->px, j->larg, j->alt);
grafico_mover(g, x, y);  grafico_pulsar(g, x, y, &ev);         /* resalte+tooltip, clic y leyenda */
grafico_agregar_punto(g, s, y);                                 /* en vivo; GR_O_VENTANA limita los puntos */
grafico_exportar(g, "/tmp/g.png", 640, 400);                    /* .png o .bmp */
```
- **Dibuja en un búfer de píxeles** 0x00RRGGBB (el de una `jan_t`, el de una `sagui.Imagen` o uno temporal).
- **Animación** sin bloquear: `grafico_avanzar(g, ahora_ms)` lleva dos progresos con curva de suavizado (`GR_O_EASE`,
  salida por omisión): la entrada (barras que crecen, líneas que se dibujan, tarta que se abre) y el cambio de datos (cada
  valor y la escala pasan de lo que se veía a lo nuevo; en una ventana deslizante los puntos se desplazan). Devuelve 1 mientras
  siga animándose. `GR_O_ANIMADO` 0 la quita.
- **Ejes**: escala automática con marcas 1-2-5 (`gr_escala`), rejilla, etiquetas numéricas y de categorías (se saltan las que no
  caben), título y etiquetas de ejes, leyenda abajo/arriba/derecha/izquierda o ninguna. Pulsar la leyenda oculta una serie
  (una porción en tarta y anillo).
- **Colores**: paleta categórica propia de 12 colores (`gr_color_serie`, claros y oscuros, distinguibles con daltonismo);
  fondo, texto y rejilla del tema del sistema (`mod_tema`, D423/D428).
- **Cálculo puro** (`grafico_calc.c`: escalas, formato, apilado, geometría de barras, histograma, Catmull-Rom, colisiones de
  rectángulo, círculo y sector, seno/coseno propios): sin libc, se prueba en el PC con `tests/grafico_unidad.c`.
- Sin coma flotante de SSE: el Makefile compila las dos unidades con la x87 (`-mfpmath=387`), como el ecualizador.

## Módulo de MicroPython `sagui_grafico`
`Grafico()` con `tipo`, `titulo`, `ejes`, `formato`, `separadores`, `rango`, `opcion(nombre[, valor])`, `categorias`,
`agregar_serie(nombre, datos, color, x, tam)`, `datos`, `agregar_punto`, `limpiar`, `quitar_series`, `color`, `nombre`,
`ocultar`, `oculto`, `avanzar(ms)`, `sucio`, `reiniciar`, `dibujar(imagen, ancho, alto)`, `mover`, `salir`, `pulsar`,
`resaltado`, `tooltip`, `exportar`; y las funciones `escala`, `formatear` y `color_serie`. Los qstr nuevos se añaden con
`tools/mp_qstr_agregar.py` (equivale a regenerar con `micropython_embed.mk` cuando no hay repositorio de MicroPython).

## Control `Grafico` de VisualPythonS
Ver `docs/visualpythons.md` (fila del control y sección «Gráficos»). Ejemplo: Nuevo > «Gráficos».
