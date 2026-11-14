# 042 - Más componentes en la libsagui (lienzo con imágenes, barra de progreso...) y tkinter sobre ellos

- **Estado:** en curso (v6.1): **hechos** el lienzo, la barra de progreso y la casilla de
  verificación (D126, rama `v6.1-componentes`, prueba h98). Quedan los demás componentes y
  el `tkinter` sobre ellos. Como parte de lo mismo (lo que dibuja la libsagui) se hizo
  además la **biblioteca de iconos** (D135, rama `v6.2-icones`, prueba h106): 112 iconos
  de 16x16 generados de los "Fluent UI System Icons" de Microsoft (MIT).
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

El kit gráfico de la libsagui (`userland/libsagui/gui.c`, demostración en `componentes`, H22)
solo tiene:
- rótulo;
- botón;
- caja de texto;
- área de texto;
- lista;
- diálogos.

Cada programa grande (Paint, visor, Mico, tareas, Markdown) se ha hecho sus propios
controles: pestañas, barras de desplazamiento, árboles, tablas. tkinter (033) los vuelve a
dibujar en Python, más lento y con otro aspecto. Faltan, sobre todo, un **lienzo que admita
imágenes** y una **barra de progreso**.

## Propuesta

Componentes nuevos en la libsagui, en C, con el aspecto de SaguiOS, rápidos (SSE2 donde
toque) y documentados en `sagui.h`. Cada uno sigue el patrón actual: estructura +
`x_desenhar` + `x_tratar(evento)`.

| Componente | Qué hace |
| --- | --- |
| **Lienzo** | superficie con elementos retenidos: líneas, rectángulos, elipses, polígonos, texto e **imágenes** (biblioteca de D054: BMP/PNG/PPM), con escala, recorte y transparencia; desplazamiento y zoom; capas u orden z; detección de qué elemento hay bajo el ratón; redibujo solo de la zona sucia |
| **Barra de progreso** | determinada (0-100 %, con texto opcional) e indeterminada (animada); horizontal y vertical |
| Casilla de verificación y botón de opción | con grupos |
| Deslizador | horizontal y vertical, con marcas |
| Barra de desplazamiento | la de Mico/Markdown, como componente común |
| Pestañas | como las del Administrador de tareas |
| Desplegable (*combo*) | sobre el menú contextual (D051/D064) |
| Tabla | lista con columnas, cabecera que ordena y anchos ajustables |
| Árbol | nodos plegables, como el explorador de Mico |
| Selector numérico (*spinbox*) | — |
| Información emergente (*tooltip*) | — |
| Barra de estado y separador | — |
| Ayuda de colocación | filas y columnas sencillas, para no colocar todo a mano |

**Integración:**

- `componentes` (demostración) enseña todos; la prueba h22 se amplía o se crea una nueva.
- Uso inmediato:
  - la descarga de `wget` en ventana o el diálogo de copia de Archivos con barra de progreso;
  - las pestañas y la tabla del Administrador de tareas;
  - el explorador de Mico;
  - el visor sobre el lienzo, si mejora.
  Cada adopción va con su prueba, sin romper las existentes.
- **Módulo `sagui` de Python:** exponer el lienzo y la barra de progreso (y lo que sea barato).
- **tkinter (033): modificarlo para usar estos componentes**:
  - `Canvas` sobre el lienzo de la libsagui (más rápido; imágenes con `PhotoImage`, zoom y
    `scrollregion`);
  - `ttk.Progressbar` con la barra de progreso nativa (modos *determinate* e
    *indeterminate*, `start`/`stop`/`step`);
  - `ttk.Notebook` (pestañas) y `ttk.Treeview` (árbol y tabla), que hoy dan error;
  - `Checkbutton`, `Radiobutton`, `Scale`, `Scrollbar`, `Spinbox` y `Combobox` con los
    nativos donde mejore el aspecto o la velocidad;
  - mantener la API de tkinter y que los ejemplos de `/ejemplos/tk` sigan funcionando.

## Criterios de aceptación

- [ ] Componentes nuevos en la libsagui con su documentación en `sagui.h`
      — **en curso**: lienzo, barra de progreso y casilla hechos (D126)
- [ ] Lienzo con imágenes, zoom, desplazamiento y detección de elementos
      — **hecho** (D126; el zoom y el desplazamiento están, las imágenes también)
- [ ] Barra de progreso determinada e indeterminada, usada al menos en un programa real
      — la barra está hecha; **falta usarla en un programa real** (wget, Archivos)
- [ ] tkinter: `Canvas` sobre el lienzo; `ttk.Progressbar`, `ttk.Notebook` y `ttk.Treeview`
      funcionando; los ejemplos y h68 siguen pasando — **pendiente**
- [x] Test automático en `tests/` (componentes en C y en tkinter, por capturas y eventos)
      — `tests/h98_componentes_v61.py` (en C; la parte de tkinter queda)
- [x] Decisión registrada en `docs/decisoes.md` — D126
