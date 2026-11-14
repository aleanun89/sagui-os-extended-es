# Ventanas en Python: el módulo sagui y tkinter

El `python` de SaguiOS (MicroPython 1.24) puede abrir ventanas de dos formas:

- **`sagui`**: el módulo del sistema, en C. Una ventana, dibujo rápido, eventos.
  Ideal para juegos, gráficos y demos.
- **`tkinter`**: el tkinter de siempre (`Button`, `pack`, `mainloop`...), escrito
  en Python sobre `sagui`, con el aspecto de SaguiOS. Los programas sencillos de
  Tk funcionan sin cambios o casi.

Ejemplos: `/ejemplos/sagui_ventana.py` y la carpeta `/ejemplos/tk`
(calculadora, tareas, dibujo, reloj, formulario y un juego).

    python /ejemplos/tk/calculadora.py

## El módulo sagui

    import sagui
    v = sagui.Ventana(320, 200, "Hola")
    v.rect(10, 10, 100, 50, 0x2A6FD8)
    v.texto(20, 30, "¡Hola!", 0xFFFFFF)
    v.actualizar()
    while True:
        e = v.evento(1000)          # espera hasta 1 s; None si no pasa nada
        if e and e.tipo == sagui.CERRAR:
            break

| Función | Qué hace |
|---------|----------|
| `Ventana(ancho, alto, titulo, redimensionable=True, dialogo=False, minimo=None)` | abre una ventana |
| `pixel(x, y)` / `pixel(x, y, color)` | lee o pinta un punto |
| `linea(x0, y0, x1, y1, color, grosor=1)` | una línea |
| `rect(x, y, ancho, alto, color, relleno=True, grosor=1)` | un rectángulo |
| `elipse(x, y, ancho, alto, color, relleno=True, grosor=1)` | la elipse dentro del rectángulo |
| `poligono(puntos, color, relleno=True)` | `[(x, y), ...]` o `[x0, y0, x1, y1...]` |
| `texto(x, y, s, color, fondo=None)` | texto con la fuente 8x16; devuelve el ancho |
| `imagen(x, y, img, ancho=None, alto=None, suave=False, transparente=None)` | dibuja una `Imagen` |
| `copiar_area(x, y, ancho, alto, dx, dy)` | copia un trozo (para desplazar) |
| `limpiar(color)` | pinta todo lo que está dentro del recorte |
| `recorte(x, y, ancho, alto)` / `recorte()` | limita el dibujo / lo quita |
| `actualizar()` | muestra lo dibujado (solo la zona que cambió) |
| `evento(timeout_ms=None)` | el próximo evento, o `None` si pasa el plazo |
| `tamano()`, `titulo(s)`, `cerrar()` | tamaño, título y cerrar |
| `menu_contextual(x, y, items)` | menú del sistema; `(texto, [subitems])` es un submenú; devuelve el índice de la línea o `None` |
| `teclas()` | las teclas apretadas (para juegos) o `None` sin foco |

Los colores son enteros `0xRRGGBB` o tuplas `(r, g, b)`.

El **evento** tiene `tipo` (`sagui.CLIC`, `SOLTAR`, `MOVER`, `TECLA`, `CERRAR`,
`REDIMENSIONAR`, `RUEDA`, `MENU`, `PANTALLA`), `x`, `y`, `botones`
(`BOTON_IZQUIERDO`, `BOTON_DERECHO`), `tecla` (el código: `ord("a")`, 10 para
Intro, `TECLA_ARRIBA`, `TECLA_ESC`, `TECLA_F1`...), `caracter`, `cliques` (2 en
el doble clic; en la rueda, los pasos con signo), `mods` (`MAYUSCULAS`,
`CONTROL`), `ancho`, `alto` y `ventana`. Si el usuario cambia el tamaño, la
ventana ya tiene el nuevo cuando llega `REDIMENSIONAR`: solo hay que redibujar.

**Imágenes:** `sagui.Imagen.cargar("/imagenes/sagui.bmp")` (BMP, PPM/PGM o
PNG), `Imagen.nueva(ancho, alto, color)`, `img.guardar("x.bmp")`,
`img.escalar(w, h)`, `img.recortar(x, y, w, h)`, `img.ancho`, `img.alto`. Una
imagen acepta los mismos dibujos que una ventana, y `memoryview(img)` da sus
píxeles como números para calcular un cuadro entero en Python.

**Otras:** `sagui.evento(timeout)` (de cualquier ventana), `sagui.ticks_ms()`,
`sagui.dormir(ms)`, `sagui.pantalla()` (tamaño de la pantalla),
`sagui.bip(frecuencia, ms)` y `sagui.portapapeles()` /
`sagui.portapapeles(texto)`.

Un bucle de juego que no gasta CPU: calcula cuándo toca el próximo cuadro y
espera con `v.evento(ese_plazo)`; el programa duerme hasta entonces o hasta
que llegue un evento.

## tkinter

    import tkinter as tk
    from tkinter import messagebox

    raiz = tk.Tk()
    raiz.title("Hola")
    tk.Label(raiz, text="¿Cómo te llamas?").pack(padx=10, pady=5)
    nombre = tk.Entry(raiz)
    nombre.pack(padx=10)
    tk.Button(raiz, text="Saludar",
              command=lambda: messagebox.showinfo("Hola", "¡Hola, %s!" % nombre.get())).pack(pady=8)
    raiz.mainloop()

### Lo que hay

- **Ventanas:** `Tk`, `Toplevel`, `mainloop`, `title`, `geometry("400x300")`,
  `resizable`, `minsize`, `destroy`, `after`/`after_idle`/`after_cancel`,
  `protocol("WM_DELETE_WINDOW", f)`, `update`, `update_idletasks`,
  `withdraw`/`deiconify`, `wait_window`, `grab_set` (dentro del programa),
  `winfo_*` (`width`, `height`, `x`, `y`, `rootx`, `reqwidth`, `children`...).
- **Componentes:** `Frame`, `LabelFrame`, `Label`, `Message`, `Button`,
  `Entry` (cursor, selección con el ratón y con Mayús+flechas, Ctrl+A/C/X/V,
  `show="*"`, `validatecommand`), `Text` (insert, get, delete, índices `"1.0"`,
  `"end"`, `"end-1c"`, `"insert"`, `+Nc`, `-Nl`, `linestart`, `lineend`;
  marcas, selección, búsqueda, rodar, `wrap`), `Checkbutton`, `Radiobutton`,
  `Listbox` (selección `browse`, `single`, `multiple`, `extended`; rodar;
  `<<ListboxSelect>>`), `Scrollbar`, `Scale`, `Spinbox`, `OptionMenu`,
  `Menubutton`, `Canvas`, `PanedWindow` (sencillo) y `scrolledtext.ScrolledText`.
- **Canvas:** `create_line` (con `width`, `arrow`, `smooth`), `create_rectangle`,
  `create_oval`, `create_polygon`, `create_arc`, `create_text`,
  `create_image`, `create_window`; `coords`, `move`, `moveto`, `scale`,
  `itemconfig`, `itemcget`, `delete`, `tag_bind`, `bbox`, `find_withtag`,
  `find_overlapping`, `find_closest`, `gettags`, `addtag_withtag`,
  `tag_raise`/`tag_lower`, `scrollregion` con `xview`/`yview`.
- **Opciones comunes:** `text`, `textvariable`, `command`, `bg`/`background`,
  `fg`, `width`, `height`, `font` (se acepta y se ignora), `relief` (`flat`,
  `raised`, `sunken`, `groove`, `ridge`, `solid`), `bd`, `padx`/`pady`,
  `anchor`, `justify`, `state`, `image`, `compound`, `wraplength`;
  `config`/`configure`, `cget` y `w["opcion"]`.
- **Geometría:** `pack` (`side`, `fill`, `expand`, `padx`, `pady`, `ipadx`,
  `anchor`, `before`/`after`), `grid` (`row`, `column`, `rowspan`, `columnspan`,
  `sticky`, `padx`, `pady`; `rowconfigure`/`columnconfigure` con `weight`,
  `minsize`, `uniform`) y `place` (`x`, `y`, `relx`, `rely`, `relwidth`,
  `relheight`, `anchor`). No se pueden mezclar `pack` y `grid` en el mismo
  contenedor (como en Tk).
- **Variables:** `StringVar`, `IntVar`, `DoubleVar` y `BooleanVar` con `get`,
  `set` y `trace_add` (y el antiguo `trace`).
- **Eventos con `bind`:** `<Button-1>`, `<Button-3>`, `<Double-Button-1>`,
  `<ButtonRelease-1>`, `<Motion>`, `<B1-Motion>`, `<Key>`, `<KeyPress-a>`,
  `<Control-s>`, `<Return>`, `<Escape>`, `<Up>`..., `<MouseWheel>`,
  `<Configure>`, `<Enter>`, `<Leave>`, `<FocusIn>`, `<FocusOut>`, `<Destroy>` y
  eventos virtuales (`<<...>>`). El objeto `event` trae `x`, `y`, `x_root`,
  `y_root`, `char`, `keysym`, `num`, `delta`, `width`, `height`, `state` y
  `widget`. `bind_all`, `bind_class`, `"break"` y Tab para pasar el foco.
- **Menús:** `Menu` con `add_command`, `add_cascade`, `add_separator`,
  `add_checkbutton`, `add_radiobutton`, `entryconfig`, `delete`, `invoke`;
  barra con `raiz.config(menu=barra)`; menú emergente con
  `menu.tk_popup(event.x_root, event.y_root)`. Son los menús del sistema.
- **Diálogos:** `messagebox` (`showinfo`, `showwarning`, `showerror`,
  `askyesno`, `askokcancel`, `askyesnocancel`, `askretrycancel`,
  `askquestion`), `simpledialog` (`askstring`, `askinteger`, `askfloat` y la
  clase `Dialog`) y `filedialog` (`askopenfilename`, `asksaveasfilename`,
  `askdirectory`, `askopenfile`, `asksaveasfile`, con `filetypes`).
- **Imágenes:** `PhotoImage(file=...)` con BMP, PNG o PPM; `width()`,
  `height()`, `subsample`, `zoom`, `copy`, `get`, `put`, `write`.
- **ttk:** un alias mínimo: `ttk.Button`, `Label`, `Entry`, `Frame`,
  `Checkbutton`, `Radiobutton`, `Scale`, `Scrollbar`, `Spinbox`, `LabelFrame`
  son los componentes normales (aceptan e ignoran `style` y `padding`), más
  `Progressbar`, `Separator`, un `Combobox` sencillo y un `Style` que no hace
  nada. `Notebook` y `Treeview` no existen y lo dicen.
- **font:** `tkinter.font.Font` existe, pero hay una sola fuente (8x16).

### Lo que no hay

Tcl (`eval`, `call`), `<KeyRelease>` (el sistema solo avisa al apretar una
tecla), fuentes y tamaños de letra, colores en las etiquetas
del `Text`, deshacer en el `Text`, `wm` avanzado (la posición de la ventana la
elige el sistema), `grab` global, `PhotoImage(data=...)`, `BitmapImage`,
`Canvas.postscript`, `ttk.Notebook` y `ttk.Treeview`. Los caracteres fuera de
Latin-1 salen como `?`.

### Diferencias con el tkinter de CPython

- `winfo_rootx()`/`event.x_root` son relativos a la ventana (el sistema no
  dice dónde está); `tk_popup(event.x_root, event.y_root)` funciona igual.
- Un error en un callback se muestra (en la salida de errores) y el programa
  sigue, como en Tk.
- Mientras un menú está abierto, los `after` esperan.
