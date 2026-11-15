# Cipó: compatibilidad con CSS, HTML5 y el DOM

Inventario de lo que el motor de la web del Cipó (`userland/web/`) entiende de la web de hoy,
función por función, **antes y después** de la D271 (v8.0). «Antes» es el motor de la v8.0b8.

- **Cómo se mide.** Con las páginas de `tests/web/`: una fila por función; la casilla de la
  fila se pone verde solo si el motor entiende la función (ver `tests/web/generar.py`). El
  Cipó las abre en QEMU y `tests/h271_cipo_css.py` mira las capturas.
- **Cómo repetirlo.** `python3 tests/h271_cipo_css.py` deja el resultado en
  `build/testes/h271-resultado.txt`; la tabla de abajo sale de
  `python3 tests/web/inventario.py tests/web/resultado-antes.txt build/testes/h271-resultado.txt`.
- **Cómo añadir una función.** Una `fila(...)` más en `tests/web/generar.py`, al final de su
  página (las filas se nombran por su número), `python3 tests/web/generar.py`, y la prueba.
  Si el motor todavía no la pasa, una línea en `tests/web/pendientes.txt`.
- **Licencias.** Las páginas y sus herramientas son propias (GPL-2.0-or-later). Lo que
  esperan se contrastó con un navegador de escritorio usado como caja negra; el motor se ha
  escrito mirando las especificaciones (W3C, WHATWG) y MDN, sin copiar código de otros
  navegadores (D262).

## Resumen

| Página | Tema | Antes | Después | Filas |
| --- | --- | --- | --- | --- |
| `01-variables-calc.html` | Variables CSS y calc() | 9 | 16 | 16 |
| `02-selectores.html` | Selectores: atributos y combinadores | 15 | 16 | 16 |
| `03-pseudoclases.html` | Pseudoclases | 8 | 16 | 16 |
| `04-pseudoclases-2.html` | Pseudoclases de formulario y pseudoelementos | 9 | 13 | 15 |
| `05-media-reglas.html` | @media, @supports, @import, @layer | 14 | 16 | 16 |
| `06-unidades.html` | Unidades | 9 | 14 | 15 |
| `07-flex.html` | Flexbox | 15 | 16 | 16 |
| `08-flex-2.html` | Flexbox (2) | 12 | 14 | 14 |
| `09-grid.html` | Grid | 10 | 16 | 16 |
| `10-posicion-caja.html` | Posición y modelo de caja | 14 | 16 | 16 |
| `11-caja-2.html` | Caja, visibilidad y transformaciones | 11 | 16 | 16 |
| `12-colores-fondos.html` | Colores, bordes y sombras | 11 | 16 | 16 |
| `13-fondos-imagenes.html` | Fondos con imagen y object-fit | 10 | 14 | 16 |
| `14-texto.html` | Texto | 8 | 16 | 16 |
| `15-html5.html` | Elementos HTML5 | 13 | 16 | 16 |
| `16-html5-medios.html` | Medios y otros elementos | 11 | 16 | 16 |
| `17-formularios.html` | Formularios | 0 | 16 | 16 |
| `18-formularios-2.html` | Formularios (2) | 9 | 15 | 16 |
| `19-js-dom.html` | JavaScript: DOM | 0 | 13 | 16 |
| `20-js-api.html` | JavaScript: API de la ventana | 0 | 13 | 16 |
| | **Total** | **188** | **304** | **316** |

Capturas (del Cipó en QEMU): `docs/capturas/H271-01-variables-calc.png`, `H271-03-pseudoclases.png`,
`H271-07-flex.png`, `H271-09-grid.png`, `H271-12-colores-fondos.png`, `H271-15-html5.png`,
`H271-17-formularios.png` y `H271-19-js-dom.png`.

## Lo que falta, por orden de uso en la web

| Función | Estado | Qué haría falta |
| --- | --- | --- |
| Cambiar la forma del árbol desde el script (`createElement`, `appendChild`, `innerHTML` de un elemento, `insertAdjacentHTML`, `<template>.content`) | falta (`remove()` y `removeChild()` solo esconden el elemento) | Un DOM propio en el puente del QuickJS que se vuelva a serializar hacia el motor; hoy el script solo cambia atributos y texto (D271) y `document.body.innerHTML` (D149) |
| Medidas de la maquetación en el script (`getBoundingClientRect`, `offsetWidth`, `getComputedStyle`) | falta (devuelven 0 o el estilo en línea) | Pasar al puente las cajas del motor (`motor_caixas`, `motor_estilo_de` ya existen) |
| `position: sticky` al desplazar | parcial (se queda en su sitio; el motor tiene la capa, sin prueba) | Probarlo con desplazamiento |
| `overflow: auto/scroll` con desplazamiento propio | parcial (recorta) | Barras y rueda por caja |
| Bordes `dashed`, `dotted`, `double` | parcial (se pintan sólidos) | Estilo por lado en el comando de borde |
| `::placeholder`, `::first-letter`, `::marker`, `::selection` | falta | Estilos de pseudoelementos que no generan caja |
| `background-clip`, `background-origin`, `background-attachment` | falta | — |
| Anidamiento de CSS (`&`), `@container`, orden de `@layer`, `color-mix()`, `oklch()` | falta (las reglas se leen sin anidar; `@layer` y `@container` se aplican siempre) | — |
| Columnas (`columns`), `subgrid`, `writing-mode` | falta | — |
| `text-shadow` y `box-shadow` con desenfoque real | parcial (anillos; la sombra del texto, sin desenfocar) | — |
| Medida sin unidad (`width: 10`) | se acepta como px | Validar por propiedad |
| `<video>`/`<audio>` | marcador (póster o caja, sin reproducir) | Fuera del alcance del navegador |
| `localStorage` persistente, `fetch` con POST y cabeceras, `IntersectionObserver` de verdad | parcial (memoria, solo GET, avisa una vez) | — |
| Reparto de anchos de tabla con `colspan` (infobox de Wikipedia) | fallo anterior | Revisar `tabela()` |

## Tabla completa

### 01-variables-calc — Variables CSS y calc()

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | var() de una variable de :root | sí | sí |
| 2 | var() con valor de reserva | sí | sí |
| 3 | variable definida con otra var() | sí | sí |
| 4 | variable heredada del padre (no de :root) | sí | sí |
| 5 | la variable del elemento gana a la de :root | sí | sí |
| 6 | var() dentro de un atajo (border) | no | **sí** |
| 7 | var() como medida | sí | sí |
| 8 | var() con reserva que es otra var() | sí | sí |
| 9 | calc() con px + px | sí | sí |
| 10 | calc() con % y px | no | **sí** |
| 11 | calc() con resta de % y px | no | **sí** |
| 12 | calc() con multiplicación, división y paréntesis | sí | sí |
| 13 | calc() con var() | no | **sí** |
| 14 | min() y max() | no | **sí** |
| 15 | clamp() | no | **sí** |
| 16 | calc() con em | no | **sí** |

### 02-selectores — Selectores: atributos y combinadores

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | [atributo] | sí | sí |
| 2 | [atributo="valor"] | sí | sí |
| 3 | [atributo^="prefijo"] | sí | sí |
| 4 | [atributo$="sufijo"] | sí | sí |
| 5 | [atributo*="trozo"] | sí | sí |
| 6 | [atributo~="palabra"] | sí | sí |
| 7 | [atributo\|="es"] | sí | sí |
| 8 | [atributo="VALOR" i] (sin distinguir mayúsculas) | no | **sí** |
| 9 | hijo: a &gt; b | sí | sí |
| 10 | hermano adyacente: a + b | sí | sí |
| 11 | hermanos siguientes: a ~ b | sí | sí |
| 12 | dos clases: .a.b | sí | sí |
| 13 | universal: * | sí | sí |
| 14 | lista: a, b | sí | sí |
| 15 | etiqueta y clase sin distinguir mayúsculas en la etiqueta | sí | sí |
| 16 | no deben coincidir (trampas) | sí | sí |

### 03-pseudoclases — Pseudoclases

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | :first-child y :last-child | sí | sí |
| 2 | :nth-child(2n+1) | sí | sí |
| 3 | :nth-child(even) y (odd) | sí | sí |
| 4 | :nth-child(n+3) y (-n+2) | sí | sí |
| 5 | :nth-last-child(2) | no | **sí** |
| 6 | :nth-of-type(2) | no | **sí** |
| 7 | :first-of-type y :last-of-type | no | **sí** |
| 8 | :only-child | no | **sí** |
| 9 | :empty | sí | sí |
| 10 | :not(.x) | sí | sí |
| 11 | :not() con lista | sí | sí |
| 12 | :is(.p, .q) | no | **sí** |
| 13 | :where(.p, .q) | no | **sí** |
| 14 | :has(&gt; hijo) | no | **sí** |
| 15 | :has(+ hermano) | no | **sí** |
| 16 | :root | sí | sí |

### 04-pseudoclases-2 — Pseudoclases de formulario y pseudoelementos

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | :checked | no | **sí** |
| 2 | :disabled y :enabled | no | **sí** |
| 3 | :required y :optional | no | **sí** |
| 4 | :link y :any-link | sí | sí |
| 5 | ::before con content | sí | sí |
| 6 | ::after con content | sí | sí |
| 7 | :before (sintaxis antigua) | sí | sí |
| 8 | ::before y ::after a la vez | sí | sí |
| 9 | content: attr() | sí | sí |
| 10 | ::first-letter | no | no |
| 11 | ::marker | no | no |
| 12 | especificidad: #id gana a .clase | sí | sí |
| 13 | !important | sí | sí |
| 14 | :is() suma especificidad; :where() no | no | **sí** |
| 15 | no deben coincidir (trampas) | sí | sí |

### 05-media-reglas — @media, @supports, @import, @layer

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | @media (min-width: 200px) se aplica | sí | sí |
| 2 | @media (max-width: 200px) no se aplica | sí | sí |
| 3 | @media screen and (min-width: 200px) | sí | sí |
| 4 | @media print no se aplica | sí | sí |
| 5 | @media con «and» entre dos condiciones | sí | sí |
| 6 | @media con lista (coma) | sí | sí |
| 7 | @media con rango: (width &gt;= 200px) | sí | sí |
| 8 | @media con rango que no se cumple: (width &lt; 200px) | sí | sí |
| 9 | @media not print | sí | sí |
| 10 | @media (prefers-color-scheme: light) | sí | sí |
| 11 | @media (orientation: landscape) y (min-height) | sí | sí |
| 12 | @supports (display: grid) | sí | sí |
| 13 | @supports not / propiedad desconocida no se aplican | no | **sí** |
| 14 | @import de otra hoja | no | **sí** |
| 15 | &lt;link rel=stylesheet&gt; | sí | sí |
| 16 | @layer | sí | sí |

### 06-unidades — Unidades

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | px | sí | sí |
| 2 | em | sí | sí |
| 3 | rem | sí | sí |
| 4 | % | sí | sí |
| 5 | pt (30pt = 40px) | sí | sí |
| 6 | in (0.25in = 24px) | no | **sí** |
| 7 | cm y mm (10mm = 1cm = 37,8px) | no | **sí** |
| 8 | vw (10vw, ventana de 400 px o más) | sí | sí |
| 9 | vh (10vh, ventana de 160 px o más) | sí | sí |
| 10 | vmin y vmax | no | **sí** |
| 11 | ch (4ch en monoespaciada de 16px) | no | **sí** |
| 12 | ex (aprox. medio em) | sí | sí |
| 13 | dvh, svh, lvh | sí | sí |
| 14 | medida sin unidad distinta de 0 no vale | no | no |
| 15 | números con signo y exponente (+40px, 4e1px) | no | **sí** |

### 07-flex — Flexbox

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | display:flex en fila | sí | sí |
| 2 | flex-grow | sí | sí |
| 3 | flex: 1 (cuatro iguales) | sí | sí |
| 4 | flex-shrink (encogen para caber) | no | **sí** |
| 5 | flex-basis | sí | sí |
| 6 | gap | sí | sí |
| 7 | justify-content: space-between | sí | sí |
| 8 | justify-content: center | sí | sí |
| 9 | justify-content: space-evenly | sí | sí |
| 10 | align-items: center | sí | sí |
| 11 | align-items: stretch (de fábrica) | sí | sí |
| 12 | align-self: flex-end | sí | sí |
| 13 | flex-direction: column | sí | sí |
| 14 | flex-wrap: wrap | sí | sí |
| 15 | order | sí | sí |
| 16 | flex-direction: row-reverse | sí | sí |

### 08-flex-2 — Flexbox (2)

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | margin-left: auto en un elemento flexible | sí | sí |
| 2 | gap con flex-wrap (row-gap y column-gap) | sí | sí |
| 3 | flex-flow: column wrap | sí | sí |
| 4 | justify-content: flex-end | sí | sí |
| 5 | justify-content: space-around | sí | sí |
| 6 | columna con justify-content: center | sí | sí |
| 7 | columna con align-items: center | sí | sí |
| 8 | display: inline-flex | sí | sí |
| 9 | flex: 2 y flex: 1 (reparto 2 a 1) | sí | sí |
| 10 | min-width impide encoger | sí | sí |
| 11 | align-content: center (con wrap) | no | **sí** |
| 12 | flex anidado | sí | sí |
| 13 | % de ancho en hijos flexibles | sí | sí |
| 14 | display: contents | no | **sí** |

### 09-grid — Grid

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | grid-template-columns en px | sí | sí |
| 2 | unidad fr (1fr 3fr) | sí | sí |
| 3 | repeat(4, 1fr) | sí | sí |
| 4 | gap | sí | sí |
| 5 | grid-template-rows | no | **sí** |
| 6 | minmax(0, 1fr) | sí | sí |
| 7 | repeat(auto-fill, minmax(20px, 1fr)) | sí | sí |
| 8 | repeat(auto-fit, minmax(10px, 1fr)) | no | **sí** |
| 9 | grid-template-areas y grid-area | sí | sí |
| 10 | grid-column: span 2 | sí | sí |
| 11 | grid-column y grid-row con número | sí | sí |
| 12 | grid-column: 1 / -1 | sí | sí |
| 13 | place-items: center | no | **sí** |
| 14 | grid-auto-flow: column | no | **sí** |
| 15 | grid-auto-rows | no | **sí** |
| 16 | áreas en varias filas | no | **sí** |

### 10-posicion-caja — Posición y modelo de caja

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | position: relative con left | sí | sí |
| 2 | position: absolute con right y bottom | sí | sí |
| 3 | inset: 0 | sí | sí |
| 4 | absolute estirado con left+right+top+bottom | sí | sí |
| 5 | z-index | sí | sí |
| 6 | position: fixed | no | **sí** |
| 7 | position: sticky (sin desplazar queda en su sitio) | sí | sí |
| 8 | float: left y right | sí | sí |
| 9 | clear: both | sí | sí |
| 10 | overflow: hidden recorta | sí | sí |
| 11 | overflow: auto recorta | sí | sí |
| 12 | box-sizing: border-box | sí | sí |
| 13 | margin: 0 auto | sí | sí |
| 14 | max-width y min-height | sí | sí |
| 15 | aspect-ratio | no | **sí** |
| 16 | display: inline-block | sí | sí |

### 11-caja-2 — Caja, visibilidad y transformaciones

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | display: none y visibility: hidden | sí | sí |
| 2 | width: fit-content | no | **sí** |
| 3 | width: max-content y min-content | no | **sí** |
| 4 | transform: translate() | sí | sí |
| 5 | transform: scale() | sí | sí |
| 6 | transform: rotate(180deg) | no | **sí** |
| 7 | propiedades lógicas (inline-size, margin-inline) | sí | sí |
| 8 | padding-block y padding-inline | sí | sí |
| 9 | height: 100% | sí | sí |
| 10 | márgenes negativos | sí | sí |
| 11 | los márgenes verticales se funden | sí | sí |
| 12 | border por lados y atajo | no | **sí** |
| 13 | width en % de un padre con padding | sí | sí |
| 14 | overflow-x y overflow-y | sí | sí |
| 15 | float con texto alrededor (inline-block) | sí | sí |
| 16 | tabla con display: table / table-cell | no | **sí** |

### 12-colores-fondos — Colores, bordes y sombras

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | border-radius | sí | sí |
| 2 | border-radius: 50% (círculo) | sí | sí |
| 3 | box-shadow con extensión (spread) | no | **sí** |
| 4 | box-shadow desplazada | no | **sí** |
| 5 | box-shadow inset | no | **sí** |
| 6 | box-shadow difuminada | no | **sí** |
| 7 | outline | no | **sí** |
| 8 | opacity | sí | sí |
| 9 | rgba() y hsl() | sí | sí |
| 10 | rgb() moderno: rgb(0 128 0) y rgb(0 128 0 / 50%) | sí | sí |
| 11 | hexadecimal con alfa (#rrggbbaa y #rgba) | sí | sí |
| 12 | currentColor e inherit | sí | sí |
| 13 | linear-gradient | sí | sí |
| 14 | linear-gradient con paradas duras | sí | sí |
| 15 | radial-gradient | sí | sí |
| 16 | transparent y color con nombre | sí | sí |

### 13-fondos-imagenes — Fondos con imagen y object-fit

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | background-image con url() | sí | sí |
| 2 | background con data: URI | sí | sí |
| 3 | background-repeat, -position y -size | sí | sí |
| 4 | background-size: cover | sí | sí |
| 5 | background-size: contain | sí | sí |
| 6 | varios fondos | no | **sí** |
| 7 | object-fit: cover | no | **sí** |
| 8 | object-fit: contain | no | **sí** |
| 9 | object-position | no | **sí** |
| 10 | img con width/height en CSS | sí | sí |
| 11 | img con max-width: 100% y height: auto | sí | sí |
| 12 | filter: grayscale() | sí | sí |
| 13 | background-clip: content-box | no | no |
| 14 | border dashed / dotted se pintan | no | no |
| 15 | cursor, pointer-events, user-select no molestan | sí | sí |
| 16 | propiedad desconocida no rompe la regla | sí | sí |

### 14-texto — Texto

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | text-align: center | sí | sí |
| 2 | text-align: right | sí | sí |
| 3 | color del texto | sí | sí |
| 4 | line-height | sí | sí |
| 5 | font: tamaño/interlínea familia | no | **sí** |
| 6 | text-indent | no | **sí** |
| 7 | letter-spacing | no | **sí** |
| 8 | white-space: nowrap | sí | sí |
| 9 | overflow-wrap: anywhere | no | **sí** |
| 10 | word-break: break-all | no | **sí** |
| 11 | text-decoration con color | no | **sí** |
| 12 | text-shadow | no | **sí** |
| 13 | word-spacing | no | **sí** |
| 14 | vertical-align: middle | sí | sí |
| 15 | @font-face con url() | sí | sí |
| 16 | text-overflow: ellipsis (recorta con overflow) | sí | sí |

### 15-html5 — Elementos HTML5

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | &lt;header&gt; y &lt;footer&gt; son bloques | sí | sí |
| 2 | &lt;nav&gt; y &lt;main&gt; | sí | sí |
| 3 | &lt;section&gt; y &lt;article&gt; | sí | sí |
| 4 | &lt;aside&gt; y &lt;figcaption&gt; | sí | sí |
| 5 | &lt;hgroup&gt; y &lt;search&gt; | sí | sí |
| 6 | &lt;details&gt; cerrado esconde el contenido | sí | sí |
| 7 | &lt;details open&gt; lo muestra | sí | sí |
| 8 | &lt;dialog&gt; cerrado no se ve | no | **sí** |
| 9 | &lt;dialog open&gt; se ve | sí | sí |
| 10 | &lt;template&gt; no se pinta | sí | sí |
| 11 | atributo hidden | sí | sí |
| 12 | &lt;picture&gt; con &lt;source&gt; | no | **sí** |
| 13 | &lt;img srcset&gt; | no | **sí** |
| 14 | &lt;img&gt; con data: URI y loading=lazy | sí | sí |
| 15 | &lt;svg&gt; en línea | sí | sí |
| 16 | elemento personalizado con estilo | sí | sí |

### 16-html5-medios — Medios y otros elementos

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | &lt;video poster&gt; como marcador con el póster | no | **sí** |
| 2 | &lt;video&gt; sin póster ocupa su caja | no | **sí** |
| 3 | &lt;video&gt; no pinta su contenido alternativo | no | **sí** |
| 4 | &lt;audio controls&gt; ocupa una caja | sí | sí |
| 5 | &lt;audio&gt; sin controls no se ve | sí | sí |
| 6 | &lt;canvas&gt; ocupa su tamaño | sí | sí |
| 7 | &lt;iframe&gt; ocupa su caja | no | **sí** |
| 8 | &lt;progress&gt; ocupa su caja | sí | sí |
| 9 | &lt;meter&gt; ocupa su caja | sí | sí |
| 10 | &lt;mark&gt; resalta en amarillo | no | **sí** |
| 11 | &lt;hr&gt; con estilo | sí | sí |
| 12 | &lt;table&gt; con atributos antiguos | sí | sí |
| 13 | &lt;font color&gt; y &lt;center&gt; | sí | sí |
| 14 | &lt;br&gt; y &lt;wbr&gt; | sí | sí |
| 15 | &lt;noscript&gt; no se ve con JavaScript | sí | sí |
| 16 | &lt;figure&gt; con sus márgenes | sí | sí |

### 17-formularios — Formularios

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | &lt;input type="text"&gt; | no | **sí** |
| 2 | &lt;input type="search"&gt; | no | **sí** |
| 3 | &lt;input type="email"&gt; | no | **sí** |
| 4 | &lt;input type="url"&gt; | no | **sí** |
| 5 | &lt;input type="tel"&gt; | no | **sí** |
| 6 | &lt;input type="number"&gt; | no | **sí** |
| 7 | &lt;input type="password"&gt; | no | **sí** |
| 8 | &lt;input type="date"&gt; | no | **sí** |
| 9 | &lt;input type="time"&gt; | no | **sí** |
| 10 | &lt;input type="color"&gt; | no | **sí** |
| 11 | &lt;input type="range"&gt; | no | **sí** |
| 12 | &lt;input type="checkbox"&gt; | no | **sí** |
| 13 | &lt;input type="radio"&gt; | no | **sí** |
| 14 | &lt;input type="file"&gt; | no | **sí** |
| 15 | &lt;input type="submit"&gt; | no | **sí** |
| 16 | &lt;input type="datetime-local"&gt; | no | **sí** |

### 18-formularios-2 — Formularios (2)

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | &lt;button&gt; | no | **sí** |
| 2 | &lt;select&gt; | no | **sí** |
| 3 | &lt;textarea&gt; | no | **sí** |
| 4 | &lt;input type="month"&gt; y "week" | sí | sí |
| 5 | &lt;input&gt; de texto: fondo y borde del autor | no | **sí** |
| 6 | &lt;button&gt;: fondo y borde del autor | no | **sí** |
| 7 | placeholder (se pinta en gris) | sí | sí |
| 8 | ::placeholder con color | no | no |
| 9 | value de un campo de texto se pinta | sí | sí |
| 10 | &lt;fieldset&gt; y &lt;legend&gt; | sí | sí |
| 11 | &lt;label&gt; es en línea | sí | sí |
| 12 | &lt;datalist&gt; no se ve | sí | sí |
| 13 | casilla marcada: accent-color no molesta | sí | sí |
| 14 | &lt;output&gt; y &lt;time&gt; son en línea | sí | sí |
| 15 | campo deshabilitado conserva su caja | sí | sí |
| 16 | &lt;input type=hidden&gt; no ocupa | no | **sí** |

### 19-js-dom — JavaScript: DOM

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | getElementById y style.background | no | **sí** |
| 2 | querySelector | no | **sí** |
| 3 | querySelectorAll y forEach | no | **sí** |
| 4 | classList.add | no | **sí** |
| 5 | classList.remove y contains | no | **sí** |
| 6 | classList.toggle | no | **sí** |
| 7 | className | no | **sí** |
| 8 | dataset | no | **sí** |
| 9 | setAttribute y getAttribute | no | **sí** |
| 10 | createElement y appendChild | no | no |
| 11 | textContent de un elemento | no | **sí** |
| 12 | innerHTML de un elemento | no | no |
| 13 | remove() y removeChild | no | **sí** |
| 14 | insertAdjacentHTML | no | no |
| 15 | closest y matches | no | **sí** |
| 16 | style.cssText y style.setProperty | no | **sí** |

### 20-js-api — JavaScript: API de la ventana

| Fila | Función | Antes | Después |
| --- | --- | --- | --- |
| 1 | setTimeout | no | **sí** |
| 2 | requestAnimationFrame | no | **sí** |
| 3 | DOMContentLoaded | no | **sí** |
| 4 | window.onload y addEventListener('load') | no | **sí** |
| 5 | localStorage | no | **sí** |
| 6 | sessionStorage | no | **sí** |
| 7 | fetch() de un texto | no | **sí** |
| 8 | fetch() con json() | no | **sí** |
| 9 | XMLHttpRequest | no | **sí** |
| 10 | IntersectionObserver | no | **sí** |
| 11 | matchMedia | no | **sí** |
| 12 | getComputedStyle | no | no |
| 13 | getBoundingClientRect, offsetWidth, innerWidth | no | no |
| 14 | Promise, async/await y funciones flecha | no | **sí** |
| 15 | eventos: dispatchEvent y CustomEvent | no | **sí** |
| 16 | template.content y cloneNode | no | no |
