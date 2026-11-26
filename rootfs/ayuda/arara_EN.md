<!-- categoria: Juegos y aplicaciones; orden: 30; titulo: Arara -->
# Arara: diagrams

**Arara** (`/apps/bin/arara`, in **Start > Accessories**) makes diagrams in the style of Microsoft Visio, but simpler: flowcharts,
networks, organization charts and sketches made of shapes joined by connectors. Its files are `.arara` (readable JSON text); you
can export a diagram to **PNG**, **SVG** and **PDF**.

    arara                       opens an empty diagram
    arara /ejemplos/arara/flujo.arara
    arara --abrir f.arara --png f.png --svg f.svg --pdf f.pdf     converts without opening the window

## The screen

- **Left, the palette**: four palettes chosen from the list at the top. *Basic* (rectangle, rounded rectangle, ellipse, diamond,
  triangle and text), *Flowchart* (start or end, process, decision, input or output, document and database), *Network* (PC, server,
  router, cloud and printer) and *Org chart* (person box).
- **Centre, the canvas**, with the **page** tabs above it and the grid behind.
- **Right, the inspector**: the properties of the selection (text, position, size, fill, line and width; for connectors, the type and
  the arrows). With nothing selected it shows the page properties.
- **Bottom, the status bar**: zoom, page, how many things are selected and a hint.

## Drawing

1. **Drag a shape** from the palette to the canvas, or click it and then click where you want it. With snap to grid on it lands aligned.
2. **Double-click** a shape to type inside it. *Enter* starts a new line; *Esc* or a click outside finishes.
3. Pick the **Straight** or **Elbow** tool and drag from one shape to another: the connector glues to the nearest connection point
   (top, right, bottom or left). The connection points are shown with those tools. *Esc* goes back to the pointer.
   **When you move a shape, its connectors follow it.** The *Connector* menu sets the arrows of new connectors.

## Selecting and editing

| Action | How |
| --- | --- |
| Select a shape or a connector | click; **Shift**+click adds or removes |
| Select several | drag a rectangle over empty space, or **Ctrl+A** |
| Move | drag (with snap to grid) or use the arrow keys |
| Resize | drag the handles of the selection box |
| Fill colour, line colour, width | in the inspector (the colour picker has transparency) |
| Delete | **Del** (connectors of a deleted shape stay loose where they were) |
| Copy, cut, paste, duplicate | **Ctrl+C**, **Ctrl+X**, **Ctrl+V**, **Ctrl+D** |
| Undo and redo | **Ctrl+Z**, **Ctrl+Y** (up to 60 steps) |

The **Arrange** menu has **bring to front** and **send to back**, **align** (left, centre, right, top, middle, bottom) and
**distribute** horizontally or vertically (three shapes or more).

## Zoom, grid and pages

- The mouse **wheel** zooms around the pointer; to pan, drag with the middle button or with the space bar. *View > Fit to window*
  frames the whole diagram.
- *Grid* shows or hides the grid and *Snap* makes what you move jump to it.
- A diagram has up to **8 pages**: the **+** tab creates one, the **x** on a tab deletes it and tabs can be reordered by dragging.
  *Page > Rename* or the inspector rename it.

## Exporting

*File > Export to PNG* saves the current page as an image (at double resolution unless it is huge), *Export to SVG* as a vector
drawing and *Export to PDF* all pages, one per landscape A4 sheet.

## The `.arara` format

JSON with a list of pages; each has its `figuras` (`tipo`, `x`, `y`, `l`, `a`, `relleno`, `linea`, `grosor`, `texto`) and its
`conectores` (the shapes they join and the point of each one: 0 top, 1 right, 2 bottom, 3 left). Colours are `#RRGGBB` or
`#AARRGGBB`. It can be read and edited by hand. Examples in `/ejemplos/arara/`: `flujo.arara`, `red.arara` and `organigrama.arara`.
