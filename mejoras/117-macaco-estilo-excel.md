# 117 - Macaco con el estilo de Excel 97

- **Estado:** hecha (**v8.3**, D293)
- **Prioridad:** media
- **Alta:** 2026-11-15
- **Relacionadas:** hoja de cálculo Macaco (078, D140/D142), cliente de correo estilo Outlook (113, D289), procesador de textos Pluma (116, D291), diálogos de archivo (103, D272/D278)

## Problema / motivación

Macaco (D142) tenía una barra de seis botones de texto, una barra de fórmulas básica, ningún formato de celda
(solo texto y números en bruto) y una sola forma de guardar. El mantenedor pidió que tuviera el aspecto del nuevo procesador
de textos y del cliente de correo: Excel 97 / Office clásico simplificado (gris, biseles, iconos de 16x16 propios).

## Propuesta

- **Interfaz** con los componentes de la libsagui de Pluma (`barra_menu`, `barra_ferr`, `combo`, `barra_estado`): menús Archivo,
  Edición, Ver, Insertar, Formato, Herramientas, Datos y Ayuda; barra Estándar (Nuevo, Abrir, Guardar, PDF, cortar/copiar/pegar,
  deshacer, Autosuma, ordenar A-Z/Z-A, gráfico); barra de Formato (fuente, tamaño, N K S, alineación, moneda, porcentaje, miles,
  decimales, bordes, relleno y color de texto); barra de fórmulas con cuadro de nombre editable (escribir `B1:B3` y Intro
  selecciona) y botón `=`; cabeceras de filas y columnas biseladas, con la selección hundida y el borde de las columnas
  arrastrable; pestañas de hojas con menú contextual (insertar, eliminar, cambiar nombre); barra de estado con SUMA, PROMEDIO
  y CUENTA de la selección; menú contextual de la cuadrícula.
- **Modelo de celdas:** cada celda lleva un índice a una tabla de formatos del libro (hasta 255 distintos; el 0 es el formato por
  omisión): número (general, número, moneda con símbolo `$`/`EUR`/`£`, porcentaje, miles; 0 a 4 decimales), alineación, estilo
  (negrita, cursiva, subrayado), fuente, tamaño, color de texto, relleno y bordes. En pantalla se ven el tamaño (la fuente de mapa
  de bits se amplía), la cursiva (se inclinan las filas), la negrita y el color; la **familia** (Arial, Times, Courier) solo cambia
  el PDF, porque la pantalla tiene una sola fuente.
- **Diálogo Formato de celdas** (Ctrl+1) con las pestañas Número, Alineación, Fuente, Bordes y Tramas; aplica solo lo que el
  usuario cambió, sobre toda la selección.
- **Archivos:** `.mxs` con líneas `formato REF nf=.. dec=.. ...` (los lectores antiguos las ignoran, así que los `.mxs` existentes
  siguen valiendo); CSV de entrada (también se abre desde Archivo > Abrir; detecta `,` `;` y tabulador; UTF-8 a Latin-1) y de salida;
  exportar a PDF con el escritor de la libsagui (`pdf_escribir`, de Pluma): A4, filas por páginas y luego columnas, con relleno,
  bordes, fuente, tamaño, estilo, color y alineación. Sin ventana: `macaco --convertir entrada salida` (`.mxs`/`.csv` a
  `.mxs`/`.csv`/`.pdf`).
- **Teclado:** F2 edita, F4 alterna referencias absolutas (`A1`, `$A$1`, `A$1`, `$A1`; el analizador de fórmulas admite ya el `$`),
  Mayús+flechas selecciona (antes solo ampliaba dos celdas), Ctrl+flechas e Inicio, Ctrl+A/B/I/U/G/N/O/S/P/1, Supr.
- **Componentes nuevos en la libsagui:** `iconos98.c` (11 iconos 16x16 de barra: nuevo, abrir, guardar, PDF, cortar, copiar, pegar,
  deshacer, alinear) y `gui_pestanas.c` (`pestanas_t`, fila de pestañas de diálogos). Se exportan `boton98`, `casilla98` y
  `lista_en_marco` (antes `static` en `edicao_rica_dialogos.c`).

## Criterios de aceptación

- [x] Menús, barras, fórmulas, cabeceras, pestañas y barra de estado (capturas `docs/capturas/H293-*.png`)
- [x] Formato de celdas por barra, atajo y diálogo, y guardado y recarga del `.mxs` con formatos
- [x] CSV de salida y de entrada, PDF validado en el anfitrión (`tests/pdf_validar.py`)
- [x] Test automático `tests/h293_macaco_excel.py`; `tests/h110_macaco.py` adaptado a las coordenadas nuevas
- [x] Decisión D293 en `docs/decisoes.md`

## Notas

- Queda para más adelante: inmovilizar paneles, alto de filas arrastrable, formato condicional, funciones nuevas, que los formatos
  acompañen a las filas al ordenar, y las fuentes reales en pantalla (con la libfontes).
- Las iconos de Pluma y de Macaco son copias del mismo dibujo (`pluma.c` conserva los suyos); `pluma.c` puede pasar a usar los
  `ic98_*` cuando se quiera quitar la duplicación.
