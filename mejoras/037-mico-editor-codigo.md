# 037 - Mico, editor de código

- **Estado:** hecha
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

Dentro de SaguiOS ya se programa en C (`cc`), Python, sh y ensamblador CHIP-8 (`chip8asm`),
pero el único editor es el Editor básico de una sola ventana. Falta un **editor de código**
ligero, con la distribución clásica de un IDE pequeño:
- explorador de archivos a la izquierda;
- pestañas;
- terminal y lista de problemas abajo.
Aspecto propio de SaguiOS: no imita la marca, los iconos ni el tema de ningún editor
existente.

## Propuesta

Programa `mico` (nombre visible «Mico»), en `userland/programas/mico/` (varios .c).

| Área | Funciones |
| --- | --- |
| Edición | pestañas; números de línea; deshacer y rehacer ilimitados (con límite de memoria); autoindentación; tabulador configurable (espacios o tab); emparejar paréntesis, corchetes y llaves; comentar o descomentar (Ctrl+/); duplicar y mover líneas (Ctrl+D, Alt+↑/↓); selección con ratón y Mayús; doble y triple clic (palabra y línea) |
| Resaltado | C, Python, sh, Markdown, ensamblador CHIP-8 y texto; reglas sencillas por lenguaje (palabras clave, cadenas, comentarios, números, preprocesador); estado por línea para comentarios de varias líneas |
| Explorador | árbol de la carpeta abierta; abrir, nuevo archivo o carpeta, renombrar y borrar con el menú contextual (D051) |
| Buscar | buscar y reemplazar (mayúsculas, palabra completa); buscar en todos los archivos de la carpeta; ir a línea (Ctrl+G) |
| Ejecutar | F5 según la extensión: `.c` → `cc` y ejecutar; `.py` → `python`; `.sh` → `sh`; `.asm` → `chip8asm` y `chip8`. Configurable por carpeta con un archivo `.mico` |
| Panel inferior | terminal integrado (terminal virtual, D036) y lista de **problemas**: `archivo:línea` de `cc` y `chip8asm` y trazas de Python; un clic salta al sitio |
| Paleta | Ctrl+Mayús+P: todas las acciones por nombre; Ctrl+P: abrir archivo por nombre |
| Ventana | redimensionable, maximizar, F11, rueda; panel lateral y panel inferior plegables; tema claro y oscuro |
| Datos | guardado atómico (escribir `x.nuevo` y renombrar); copia de recuperación de lo no guardado cada pocos segundos; aviso si el archivo cambió en disco; pregunta al cerrar con cambios |

**Necesita del sistema:**

- **Portapapeles** (pendiente desde la 023): dos llamadas, copiar y pegar texto, más su
  envoltorio en la libsagui. Úsalo también en Editor, Markdown y Terminal.
- Submenús (031) para la barra de menús.

**Rendimiento:**
- *gap buffer* por documento;
- índice de inicio de línea;
- solo se redibujan las líneas cambiadas;
- archivos de decenas de miles de líneas fluidos incluso en un Pentium III.

**Fuera de alcance:** extensiones, depurador, autocompletado inteligente (solo palabras del
documento), git, multicursor y minimapa.

## Lo que se hizo

- **Mico** (`/bin/mico`, `userland/programas/mico/`, nueve archivos, unas 4.900 líneas, D075):
  todo lo de la tabla de arriba. Barra de menús (Archivo, Editar, Buscar, Ver, Ejecutar,
  Ayuda) con submenús; paleta de órdenes; temas claro y oscuro propios; `mico -t archivo`
  vuelca la clase de cada letra. Proyecto de ejemplo en `/ejemplos/mico` y manual en
  `/ayuda/mico.md` (F1).
- **Portapapeles del sistema** (D074): `SIS_COPIAR_TEXTO` (94) y `SIS_COLAR_TEXTO` (95), un
  texto Latin-1 de hasta 64 KB en el kernel; `copiar_texto()` y `colar_texto()` en la
  libsagui. Selección y Cortar/Copiar/Pegar en el área de texto de la libsagui (y, por ella,
  en el Editor), en el editor del Markdown y en el Terminal (selección con el ratón;
  Ctrl+Mayús+C/V). Pegar también en las cajas de texto de los diálogos.
- **Gestor de ventanas:** `EV_TECLA` lleva también el bit de Alt (para Alt+↑/↓).
- Entrada «Mico (editor de código)» en Inicio > Accesorios, con icono propio de 16x16 (también
  en la barra de tareas), y `ajuda mico`.

## Criterios de aceptación

- [x] Abrir una carpeta, editar varios archivos en pestañas, guardar y recuperar tras un
      cierre brusco
- [x] Resaltado correcto de C, Python, sh, Markdown y CHIP-8 (prueba por capturas o por
      volcado de los colores por línea)
- [x] F5 compila y ejecuta C y Python; un error de `cc` aparece en Problemas y el clic salta
      a la línea
- [x] Portapapeles entre Mico, el Editor y el Terminal
- [x] Archivo grande (p. ej. 20.000 líneas) sin bloqueos
- [x] Test automático en `tests/` (`tests/h64_mico.py`, capturas `docs/capturas/H64-*.png`)
- [x] Decisión registrada en `docs/decisoes.md` (D074 portapapeles, D075 Mico)

## Notas

- Unas 4.900 líneas de C, más el portapapeles en el kernel (unas 50).
- F5 en `.asm` usa `chip8asm` y `chip8` (mejora 034): sin ellos en la imagen, el terminal
  dice que no existen; el resaltado de CHIP-8 sigue la sintaxis de D071.
- **Pendiente / sin hacer:**
  - el aviso de «cambió en disco» compara tamaño y hash (hasta 256 KB) porque el sistema de
    archivos no guarda la fecha de modificación; en archivos mayores solo el tamaño;
  - el portapapeles es solo texto (sin imágenes para el Paint) y no avisa de cambios;
  - la selección del Terminal es lineal y solo de lo que está a la vista (no del historial);
  - en el Mico, Ctrl+Espacio completa con palabras del documento, sin lista desplegable;
  - sin cursor parpadeante, sin ajuste de líneas largas (se desplaza en horizontal) y sin
    arrastrar pestañas ni el borde entre paneles para cambiar su tamaño;
  - la búsqueda no admite expresiones regulares;
  - las preferencias (tema, paneles) no se guardan entre sesiones fuera del `.mico`.
