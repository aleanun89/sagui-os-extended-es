# 023 - Menú contextual con el botón derecho

- **Estado:** hecha
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

El botón derecho casi no hacía nada: en el escritorio, solo «quitar con seguridad» directo
(sin avisar) sobre el icono del pendrive; en las ventanas llegaba a los programas como un
`EV_CLIQUE` con `botoes = 2` que ninguno usaba. Quien viene de Windows o de Linux espera un
menú con las acciones del objeto que tiene debajo.

## Propuesta (aplicada)

- **Kernel:** componente de menú emergente en `kernel/gui/menu.c` (entradas, separadores,
  entradas desactivadas, atajos alineados a la derecha, resaltado al pasar el ratón, clic para
  elegir, clic fuera o Esc para cancelar, flechas + Enter, recolocación para no salirse de la
  pantalla, sombra). `kernel/gui/janelas.c` lo pinta por encima de todo dentro de
  `gui_mostrar` y le da el ratón y el teclado mientras está abierto.
- **Menús del sistema:** escritorio vacío (Terminal, Archivos, Editor, Calculadora,
  Administrador de tareas, Acerca de SaguiOS; desde la mejora 031 también «Configuración de
  pantalla...» y el submenú «Resolución»), iconos (Abrir; en el pendrive, Quitar con
  seguridad) y barra de título (Restaurar, Minimizar, Maximizar, Pantalla completa, Cerrar).
- **ABI:** `SIS_MENU_CONTEXTUAL` (90) y `EV_MENU` (12) en `kernel/inc/abi.h`;
  `menu_contextual()` en la libsagui.
- **Programas:** Archivos (Abrir, Renombrar, Borrar, Propiedades; Nueva carpeta, Actualizar,
  Atrás), Terminal (Limpiar pantalla, Nueva ventana, Cerrar), Editor y Markdown (las acciones
  de la barra).

Detalle en `docs/decisoes.md`, D051.

## Criterios de aceptación

- [x] Menú del escritorio que lanza programas
- [x] Menú de los iconos, con «Quitar con seguridad» en el pendrive
- [x] Menú de la barra de título; «Cerrar» envía `EV_FECHAR` como la X
- [x] Llamada al sistema para que los programas abran su menú y evento `EV_MENU` con el índice o -1
- [x] Archivos, Terminal, Editor y Markdown usan el menú
- [x] Teclado: flechas, Enter y Esc; clic fuera cancela; se recoloca en los bordes
- [x] Test automático `tests/h55_menu_contextual.py` (capturas `docs/capturas/H55-*.png`)
- [x] Decisión registrada en `docs/decisoes.md` (D051)

## Notas

- No hay portapapeles ni selección de texto: Terminal, Editor y Markdown no tienen
  Cortar/Copiar/Pegar, y Archivos no tiene «Copiar ruta». Cuando exista un portapapeles, son
  líneas nuevas en esos menús.
- ~~No hay submenús~~: hechos en la mejora 031 (D064). `>Texto` abre un submenú con las
  líneas siguientes hasta una línea `<`, y `*Texto` sale marcada; los programas los tienen
  por la misma `SIS_MENU_CONTEXTUAL`, ahora con hasta 64 líneas. Los usan el menú Inicio y el
  submenú «Resolución» del escritorio; «Abrir con...» en Archivos sigue sin hacer.
- La tecla «Menú» del teclado (y Mayús+F10) podría abrir el menú de la ventana con foco; no
  está hecho.
