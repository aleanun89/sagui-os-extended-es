# Detector de problemas de la GUI (D300)

Ayuda para depurar: avisa de componentes que se tapan, salen de la ventana, tienen el texto cortado o son demasiado pequeños.
**No es un criterio de fallo**: un aviso no siempre es un error (un marco con controles dentro, un menú desplegado o un tirador sobre
un objeto se solapan a propósito). Los avisos se leen como pistas, con una captura al lado.

## Activarlo

- En una shell: `SAGUI_GUI_REVISAR=1 programa` (o `export SAGUI_GUI_REVISAR=1`). Con `2` además dibuja un **borde rojo** en los
  componentes con aviso (se ve en las capturas, `docs/capturas/H300-1-modo-visual.png`).
- Para todo lo que se lanza desde el escritorio y los menús: `echo 1 > /etc/gui-revisar.modo` (o `2`); `rm /etc/gui-revisar.modo` lo apaga.
- Apagado no cuesta nada más que un `if` por dibujo de componente. Se lee una vez, en el primer `jan_abrir` del programa.

Los avisos salen por el registro serie (`dmesg`, o la serie del QEMU):

```
[GUI-REVISAR] pluma Buscar: solape botao(20,20 120x28 "Aceptar") con botao(100,30 120x28 "Cancelar"), zona 40 px x 18 px
[GUI-REVISAR] gui-revisar Prueba GUI: texto cortado: ancho necesario/disponible botao(20,100 80x28 "Texto larguisimo") [128/78]
[GUI-REVISAR] gui-revisar Prueba GUI: demasiado pequeño: tamaño/mínimo botao(290,20 100x8 "Bajo") [8/16]
[GUI-REVISAR] discos Discos: fuera de la ventana botao(582,672 92x26 "Expulsar") [0/30]
[GUI-REVISAR] reproductor Reproductor de vídeo: texto cortado por el borde de la ventana en (506,9): "..." necesita 312 px y quedan 294
[GUI-REVISAR] macaco Macaco - Libro1: cobertura, 10 componentes registrados
```

Cada aviso lleva las coordenadas (x,y anchoxalto, relativas al área cliente de la ventana) para localizarlo en la captura. El mismo
aviso no se repite aunque se redibuje (hasta 80 por ventana).

## Qué avisa

| Aviso | Cuándo |
|---|---|
| `solape` | dos componentes **hermanos** se cruzan en más de 1 px por cada lado. Si uno está dentro del otro (con 1 px de tolerancia) es padre e hijo y no cuenta. |
| `fuera de la ventana` | el componente sale del área cliente (los números son los píxeles que sobran por x e y). |
| `texto cortado` | el texto de un botón o combo no cabe: ancho necesario / ancho disponible. |
| `texto cortado por el borde de la ventana` | un `g_texto` sin límite que se sale de la ventana por la derecha. |
| `demasiado pequeño` | botón, caja o combo más bajo que la fuente (16 px) o más estrecho que una letra. |
| `cobertura` | cuántos componentes de la libsagui tiene la ventana. **Lo dibujado a mano con `g_retangulo`/`g_texto` y sin componentes no se vigila** (Paint, Mico, Terminal...). |

## Cómo funciona

`userland/libsagui/gui_revisar.c`. Cada función de dibujo de un componente (`botao_desenhar`, `boton98`, `rotulo`, `caixa_desenhar`,
`area_desenhar`, `lista_desenhar`, `combo_desenhar`, `casilla_desenhar`/`casilla98`, `pestanas_desenhar`, barras de menús, de herramientas
y de estado, `regua_desenhar`, `progresso_desenhar`, `lenzo_desenhar`, `rico_desenhar`, `rte_desenhar`) llama al terminar a
`GR_REG(j, tipo, objeto, x, y, l, a, texto, ancho_disponible, flags)`. El registro es por ventana; una entrada nueva sustituye a la del
mismo objeto, y un relleno posterior que cubre un rectángulo entero lo borra (lo que se pinta después tapa lo anterior: un cambio de
pestaña no deja componentes fantasma). Al terminar `jan_mostrar` (y, como mucho cada 250 ms, en `jan_mostrar_area`) se analiza.

Un programa que dibuja a mano puede registrar sus piezas: `gui_revisar_rect(j, "mi_panel", x, y, l, a, "texto", ancho_disponible)`.

## Excepciones (solapes a propósito)

- En el código: `gui_revisar_ignorar(&componente)` tras crear el componente (por puntero; sirve para `botao_t`, `caixa_t`, `lista_t`...).
  Ejemplo: `userland/testes/gui-revisar.c`.
- Sin tocar el programa: `/etc/gui-revisar.ignorar`, una línea `programa:tipo:texto` por regla; `*` es comodín y se puede dejar vacío
  lo que no importa. `#` comenta. Ejemplos: `pluma:botao:Cancelar`, `*:lista:*`, `macaco:rotulo:*`. Los tipos son `botao`, `caixa`, `area`,
  `lista`, `combo`, `casilla`, `pestanas`, `barra_menu`, `barra_ferr`, `barra_estado`, `seccion`, `regua`, `progresso`, `lenzo`, `rico`,
  `rte`, `rotulo` y los que ponga `gui_revisar_rect`.

## Herramienta: `tests/gui_revisar.py`

```
. ~/.local/saguitools/env.sh
python3 tests/gui_revisar.py [--modo 1|2] [--solo configuracion,pluma] [--kvm]
```

Arranca el sistema en QEMU con `/etc/gui-revisar.modo` activo, abre una a una Configuración (todas las pestañas), Archivos, Pluma (con
Abrir y Buscar), Macaco (con Guardar como), Correo, Chat IA, Mico (con Abrir), Recreativa, VisualPythonS, Grabadora, Cámara, Visor de PDF,
Reproductor, Música, Terminal, Calculadora, Paint (con Abrir), Información del PC y Discos (vista de uso y de Particiones). Saca una captura de
cada paso y escribe `build/gui-revisar/informe.md` (con las capturas en `build/gui-revisar/capturas/`) con los avisos agrupados por
aplicación. **Sale con 0** y resume «N avisos en M aplicaciones»; solo sale con 1 si una aplicación no abre ventana. Dura unos ocho minutos
(con KVM). Cierra cada aplicación con `kill` desde el terminal. Los diálogos de Archivos (D272) se ven desde Pluma, Macaco, Mico y Paint,
que usan el mismo diálogo.

## Estado del primer uso (2026-11-15)

**1 aviso en 1 aplicación de 20 abiertas** (tras corregir lo de abajo; antes eran 2 en 2):

| Aplicación | Avisos | Componentes (cobertura) |
|---|---|---|
| Configuración (10 pestañas) | 0 | 13 |
| Calculadora, Chat IA, Correo, Música, Pluma (+Buscar), Macaco, Reproductor | 0 | 8 a 18 |
| Archivos, Discos y Particiones | 1 (Discos: lista con desplazamiento) | 2 a 3 |
| Paint, Mico, Recreativa, Terminal, Información del PC, Cámara, Grabadora, Visor de PDF | 0 | **0 (dibujan a mano)** |
| VisualPythonS | sin detector (tkinter) | - |

**Corregido:**
- Macaco: un número que no cabe en la columna se mostraba truncado («$1,234.5» por «$1,234.50»); ahora muestra `###` como Excel.
- Reproductor: el texto de ayuda de la barra («Espacio: pausa   F11: pantalla completa») se cortaba por el borde con la ventana de 800 px;
  ahora se elige la versión larga, la corta o ninguna según lo que cabe.
- Del propio detector: las casillas de verificación (13 px) no se marcan como «demasiado pequeñas» (son así).

## Pendiente, por prioridad

1. **tkinter y VisualPythonS (alta).** Las aplicaciones Python (`rootfs/lib/python/tkinter/`, `userland/micropython/janela_sagui.c`) tienen su
   propia capa de widgets y su gestor de geometría (`pack`/`grid`/`place`) y no pasan por la libsagui: no hay detector. Haría falta una
   comprobación equivalente en Python, tras la colocación (hermanos que se cruzan, fuera del contenedor, texto más ancho que el widget).
   Es donde más probable es que los usuarios de VisualPythonS vean controles tapados.
2. **Cobertura de lo dibujado a mano (media).** Paint, Mico, Terminal, Archivos (vistas de iconos y detalles), Información del PC,
   Recreativa, Cámara y Grabadora dibujan con `g_retangulo`/`g_texto` sin componentes: llamar a `gui_revisar_rect` en sus funciones de
   maquetación daría esa cobertura (barras de herramientas, paneles, columnas).
3. **Vistas con desplazamiento (media).** La lista de Discos avisa de «fuera de la ventana» por las filas que el desplazamiento oculta
   (falso positivo). Haría falta `gui_revisar_recorte(j, x, y, l, a)` para decir «lo que sale de este rectángulo está recortado a propósito»
   y que no se avise por ello (ni por solape con la barra de abajo).
4. **Texto cortado en las listas y celdas (baja).** Las listas y las cajas desplazan o recortan a propósito; no se avisa del texto largo de
   sus ítems. Una opción `SAGUI_GUI_REVISAR=3` podría avisar también de eso, pero con mucho ruido.
5. **Corte «a mitad de carácter» (baja).** La fuente es de 8x16 y los componentes recortan por caracteres enteros (`g_texto_n`), así que
   un texto no se parte a mitad de letra; solo se avisa de que no cabe.
6. **Solapes entre ventanas** (diálogos que tapan a su padre) quedan fuera: es cosa del gestor de ventanas, no de los componentes.
