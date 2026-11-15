# Pluma: el procesador de textos (mejora 116, D291)

Pluma (`/bin/pluma`, Inicio > Accesorios) es un procesador de textos con el aspecto de un Word 97
simplificado y sin macros. Lee y escribe **RTF** y texto plano y **exporta a PDF**.

## Uso

- `pluma [archivo]`: abre un `.rtf` o un texto (se detecta por el contenido). Sin argumentos, un documento nuevo.
- **Barra Estándar:** Nuevo, Abrir, Guardar, Exportar a PDF, Cortar, Copiar, Pegar, Deshacer, Rehacer, Buscar y zoom.
- **Barra Formato:** fuente (Arial, Times New Roman, Courier New y todas las del sistema, ver «Fuentes») y tamaño (también se puede escribir), **N**egrita, **K** cursiva,
  **S**ubrayado, alineación (izquierda, centro, derecha, justificado), viñetas y sangrías.
- **Regla:** arrastrar las sangrías (triángulos: primera línea arriba, izquierda y derecha abajo) cambia el párrafo; arrastrar el
  borde gris/blanco cambia los márgenes de la hoja.
- **Atajos:** Ctrl+N/O/S nuevo, abrir, guardar; Ctrl+P exportar a PDF; Ctrl+F buscar; Ctrl+H reemplazar; Ctrl+B/I/U formato;
  Ctrl+Z/Y deshacer y rehacer; Ctrl+X/C/V; Ctrl+A; Ctrl+Intro salto de página; Ctrl+rueda zoom.
- **Menús:** Archivo (incluye papel A4 o Carta), Edición, Ver (zoom, regla), Insertar (fecha, salto de página), Formato (Fuente,
  Párrafo, viñetas, alineación, sangrías), Herramientas (contar palabras), Ayuda.
- Guardar: el tipo del diálogo decide el formato (`.rtf` por defecto, `.txt` para texto plano).

## Imágenes y WordArt (D296, mejora 122)

- **Insertar > Imagen...** (BMP, PNG, JPEG baseline, GIF) y **Insertar > WordArt...** (también en la barra Dibujo, que se oculta con Ver).
- Un clic selecciona el objeto (tiradores: arrastrar cambia el tamaño; en las esquinas de una imagen se conserva la proporción salvo con
  Mayús); arrastrarlo desde dentro lo mueve; **doble clic** abre Formato de imagen (o la galería, si es un WordArt).
- **Formato de imagen** (menú Formato o doble clic): tamaño en cm y en %, proporción, ajuste (en línea o arriba y abajo),
  alineación, borde y pie. «Cuadrado» y recortar no están hechos.
- **WordArt:** 20 estilos, texto, fuente, tamaño, negrita y cursiva. Ctrl+V pega también imágenes del portapapeles del sistema.
- RTF: `\pict` con `\pngblip`/`\jpegblip` (Word y LibreOffice las ven); PDF: imágenes como XObject (el JPEG con DCTDecode).

## Asistente IA (D298, mejora 121)

**Ver > Asistente IA** (o el botón del robot de la barra Estándar, o **F12**) abre a la derecha el panel de agentes con el «Pluma IA». Sus herramientas:
leer el documento o la selección (texto plano o estructura de párrafos y estilos), insertar texto en el cursor, reemplazar la selección, aplicar formato a la selección
(negrita, cursiva, subrayado, tamaño, alineación, viñetas) e insertar un título WordArt. Lo que modifica el documento pide confirmación mostrando el texto, y se
deshace con **un solo Ctrl+Z** (`rte_grupo_iniciar` / `rte_grupo_fim` en la libsagui agrupan las operaciones).

Clic derecho sobre el documento (o menú Herramientas): **Asistente IA: Resumir, Corregir, Traducir al inglés, Reescribir más formal / más informal** sobre la selección.
Opciones para pruebas: `pluma --url http://host:puerto/v1 --modelo m archivo`. Detalles en `docs/agentes-ia.md`.

## Fuentes (D319)

Pluma usa las fuentes que hay instaladas, no solo las tres clásicas:

- **De dónde salen:** `/fontes` (DejaVu, en el ramdisk), `/apps/fuentes` (las Typodermic y otras, D196) y `/home/fuentes` (las del
  usuario). Se leen los `.ttf` y `.otf` y se agrupan por **familia** con el nombre de la tabla `name` (Regular, Negrita, Cursiva,
  Negrita cursiva; el nombre del fichero si no hay tabla). La función reutilizable es `fuentes_sistema_listar()` (`fontes.h`).
- **Elegirlas:** pulsar el combo de fuentes de la barra Formato abre el selector con todas las familias, **cada una dibujada con su
  fuente**; el diálogo Formato > Fuente lleva la misma lista. **«Actualizar lista»** (y abrir el diálogo Fuente) vuelven a leer las
  carpetas: una fuente copiada a `/home/fuentes` con Pluma abierto aparece sin reiniciar. Las tres clásicas siguen al principio.
- **Pantalla:** antialias con la libfontes (caché de glifos por fuente y tamaño) y maquetado con los anchos reales de la fuente.
  Arial, Times New Roman y Courier New se dibujan con DejaVu Sans, Serif y Mono pero **se maquetan con Helvetica, Times y Courier**
  (la paginación de pantalla y PDF es la misma); DejaVu se escala (Sans 0,93; Serif 0,85; Mono 1,00) para que el ancho medio coincida.
  Sin fuente cargable se vuelve a la 8x16.
- **PDF:** las fuentes del sistema usadas **se incrustan**: TrueType con `FontFile2` y subconjunto de los caracteres usados (unos
  3 KB por fuente y estilo en un texto corto); OpenType CFF entero con `FontFile3`. WinAnsi y `Widths` reales. Las clásicas van como
  Helvetica, Times y Courier estándar, sin incrustar. La cursiva que la familia no trae se inclina 12 grados; la negrita que falta se
  dibuja con la regular. Licencias: DejaVu permite incrustar; las Typodermic son dominio público (CC0).
- **RTF:** `\fonttbl` lleva el nombre real de la familia. Al abrir un RTF con una fuente que no está, se sustituye por la clásica
  más parecida (serif, sans o mono) y la barra de estado avisa («Fuente no instalada, sustituida: ...»).
- **Rendimiento** (`pluma --bench familia|8x16 [páginas]`, 25 páginas, KVM): maquetar todo el documento tras una tecla cuesta
  0,47 ms con Helvetica y 0,85 ms con una fuente del sistema; maquetar y redibujar la vista, 1,3-1,7 ms contra 1,7 ms de la 8x16 de
  antes: no hay diferencia apreciable.

## Modos sin ventana (para las pruebas)

```
pluma --convertir entrada.rtf salida.pdf [--carta]     # .rtf/.txt -> .rtf/.txt/.pdf
pluma --info archivo.rtf                               # páginas, palabras, caracteres, párrafos, objetos
pluma --wordart salida.rtf|pdf "texto" estilo [pt]    # documento con un WordArt
pluma --imagem imagen salida.rtf|pdf [cm]              # documento con una imagen centrada
pluma --fuentes                                        # familias que ofrece (0..2 clásicas, 3.. del sistema)
pluma --medir "familia" estilo pt "texto"              # ancho en twips con las métricas de esa familia
pluma --bench familia|8x16 [páginas]                   # ms por tecla (maquetar y dibujar) en un documento largo
```

## Formatos

- **RTF** (`userland/libsagui/rtf.c`): fuentes y colores, tamaño, negrita, cursiva, subrayado, tachado, alineación, sangrías,
  interlineado, espaciado, viñetas simples, saltos de página, tabuladores, `\'hh`, `\uN`, hoja y márgenes. Se descartan imágenes,
  tablas, estilos, cabeceras y campos; las listas numeradas pasan a viñetas.
- **PDF** (`userland/libsagui/pdf_escribir.c`): PDF 1.4 con las 14 fuentes estándar y WinAnsi, Flate, A4 o Carta, más las fuentes del
  sistema incrustadas (D319). La paginación es la de la pantalla (misma maquetación, con los anchos de Helvetica/Times/Courier
  para las clásicas y los reales para las del sistema).

## Componentes de la libsagui que se pueden reutilizar

Todos en `userland/inc/gui.h` (sección «Componentes de aplicaciones de escritorio»), con aspecto Windows 98:

| Componente | Para qué |
|---|---|
| `barra_menu_t` | barra de menús; elegir devuelve `BARRA_MENU_ITEM(menú, línea)` |
| `barra_ferr_t` / `barra_item_t` | barra de herramientas con iconos, botones conmutables, separadores y huecos |
| `combo_t` | lista desplegable (editable o no) |
| `regua_t` | regla horizontal con márgenes y sangrías en twips |
| `barra_estado_t` | barra de estado con secciones |
| `rte_t` | control de texto con formato (vista de página, selección, portapapeles, deshacer, buscar, PDF) |
| `dialogo_fonte`, `dialogo_paragrafo`, `dialogo_buscar` | diálogos modales |
| `rtf_ler`, `rtf_escrever` (`rtf.h`) | RTF sobre un `rte_t` |
| `pdfw_*` (`pdf_escribir.h`) | escritor de PDF independiente del control |
| `g_bisel`, `g_flecha`, `W98_*` | dibujo Win98 |

La demo `componentes --escritorio` los enseña (ver `userland/programas/componentes.c`).

## Qué probar en hardware real

Abrir un RTF de Word o LibreOffice y comprobar que se lee bien; escribir y guardar RTF y abrirlo en Word/LibreOffice; exportar a PDF y abrirlo en un
visor externo; comprobar la velocidad al escribir en un documento de varias páginas (el redibujado es completo en cada tecla); zoom con la rueda.
