<!-- categoria: Juegos y aplicaciones; orden: 30; titulo: Arara -->
# Arara: diagramas

**Arara** (`/apps/bin/arara`, en **Inicio > Accesorios**) hace diagramas al estilo de Microsoft Visio, pero sencillos: diagramas de
flujo, redes, organigramas y esquemas con figuras que se unen con conectores. Sus archivos son `.arara` (un JSON de texto legible);
puedes exportar el diagrama a **PNG**, **SVG** y **PDF**.

    arara                       abre un diagrama vacío
    arara /ejemplos/arara/flujo.arara
    arara --abrir f.arara --png f.png --svg f.svg --pdf f.pdf     convierte sin abrir la ventana

## La pantalla

- **Izquierda, la paleta**: cuatro paletas que se eligen en la lista de arriba. *Básicas* (rectángulo, rectángulo redondeado, elipse,
  rombo, triángulo y texto), *Diagrama de flujo* (inicio o fin, proceso, decisión, entrada o salida, documento y base de datos), *Red*
  (PC, servidor, router, nube e impresora) y *Organigrama* (caja de persona).
- **Centro, el lienzo**, con las pestañas de las **páginas** encima y la rejilla de fondo.
- **Derecha, el inspector**: las propiedades de lo que está elegido (texto, posición, tamaño, relleno, línea y grosor; en los
  conectores, el tipo y las flechas). Sin nada elegido muestra las de la página.
- **Abajo, la barra de estado**: zoom, página, cuántas cosas hay elegidas y una pista.

## Dibujar

1. **Arrastra una figura** desde la paleta hasta el lienzo, o haz clic en ella y luego en el lugar donde la quieras. Con el
   ajuste a la rejilla activado cae alineada.
2. **Doble clic** en una figura para escribir dentro de ella. *Intro* hace un salto de línea; *Esc* o un clic fuera terminan.
3. Elige la herramienta **Recto** o **Ángulo** y arrastra de una figura a otra: el conector se pega al punto de conexión más
   cercano (arriba, derecha, abajo o izquierda). Con esas herramientas se ven los puntos de conexión. *Esc* vuelve al puntero.
   **Cuando mueves una figura, sus conectores la siguen.** El menú *Conector* decide las flechas de los conectores nuevos.

## Elegir y editar

| Acción | Cómo |
| --- | --- |
| Elegir una figura o un conector | clic; **Mayús**+clic añade o quita |
| Elegir varias | arrastra un rectángulo en el vacío, o **Ctrl+A** |
| Mover | arrastra (con ajuste a la rejilla) o usa las flechas del teclado |
| Cambiar el tamaño | arrastra las asas del recuadro de la selección |
| Color de relleno y de línea, grosor | en el inspector (el selector de color tiene transparencia) |
| Borrar | **Supr** (los conectores de una figura borrada quedan sueltos donde estaban) |
| Copiar, cortar, pegar, duplicar | **Ctrl+C**, **Ctrl+X**, **Ctrl+V**, **Ctrl+D** |
| Deshacer y rehacer | **Ctrl+Z**, **Ctrl+Y** (hasta 60 pasos) |

El menú **Organizar** trae **al frente** y **atrás**, **alinear** (izquierda, centro, derecha, arriba, medio, abajo) y **distribuir**
en horizontal o en vertical (tres figuras o más).

## Zoom, rejilla y páginas

- La **rueda** del ratón acerca y aleja alrededor del puntero; para desplazar, arrastra con el botón central o con la barra espaciadora.
  *Ver > Ajustar a la ventana* encuadra todo el diagrama.
- *Rejilla* muestra u oculta la cuadrícula y *Ajuste* hace que lo que muevas salte a ella.
- Un diagrama tiene hasta **8 páginas**: la pestaña **+** crea una, la **x** de la pestaña la elimina y se pueden ordenar arrastrándolas.
  *Página > Cambiar el nombre* o el inspector la renombran.

## Exportar

*Archivo > Exportar a PNG* guarda la página actual como imagen (a doble resolución si no es enorme), *Exportar a SVG* como dibujo
vectorial y *Exportar a PDF* todas las páginas, una por hoja A4 apaisada.

## El formato `.arara`

JSON con una lista de páginas; cada una trae sus `figuras` (`tipo`, `x`, `y`, `l`, `a`, `relleno`, `linea`, `grosor`, `texto`) y sus
`conectores` (las figuras que unen y el punto de cada una, 0 arriba, 1 derecha, 2 abajo y 3 izquierda). Los colores son `#RRGGBB`
o `#AARRGGBB`. Se puede leer y editar a mano. Ejemplos en `/ejemplos/arara/`: `flujo.arara`, `red.arara` y `organigrama.arara`.
