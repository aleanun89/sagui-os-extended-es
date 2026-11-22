# Ayuda: leer la documentación de SaguiOS desde el propio sistema

**Ayuda** (`/bin/ayuda-gui`) es el lector de los documentos Markdown del sistema: las guías para aprender cómo está
hecho SaguiOS, los manuales de cada programa y las guías de los lenguajes de programación. Se abre con **F1** en el
escritorio, desde el menú Inicio (Sistema > Ayuda) o con la orden `ayuda -g TEMA` del terminal.

## Cómo se usa

- **Índice a la izquierda**: tres categorías, «Conoce SaguiOS», «Programación» (un apartado por lenguaje) y
  «Juegos y aplicaciones». Un clic despliega o abre; Ctrl+Arriba y Ctrl+Abajo pasan al documento anterior o siguiente.
- **Enlaces**: los de un documento a otro (`otro.md`, `otro.md#apartado`) y los `#apartado` del mismo documento se
  siguen con un clic. `<` y `>` (o Alt+Izquierda, Alt+Derecha, Retroceso) van atrás y adelante.
- **Buscar** (Ctrl+F): al escribir se resaltan las coincidencias del documento; Intro o F3 saltan a la siguiente
  (con Mayús, a la anterior; botones «Ant.» y «Sig.»). **Todo** (o Ctrl+Intro, Ctrl+G) busca en toda la ayuda y
  muestra una lista de resultados a la izquierda; un clic abre el documento en esa coincidencia y Esc vuelve al índice.
- **Idioma**: el botón ES/EN/PT cambia de idioma. Si existe `X_EN.md` o `X_PT.md`, se muestra esa versión; si no, la
  española. Al arrancar se usa el idioma del sistema.
- **Tamaño de la letra**: A- y A+ (o las teclas - y +).
- **Código**: cada bloque tiene un botón «Copiar» que lo deja en el portapapeles.
- Sigue el esquema de colores del sistema (Configuración > Estilos), también el oscuro.

Aún no se dibujan imágenes (salen como `[imagen: texto]`) ni se colorea la sintaxis del código.

## Cómo añadir un documento

Basta con dejar un archivo `.md` en `/ayuda` (en la imagen: `rootfs/ayuda/`) o en una carpeta dentro: aparece solo en
el índice. El título es el del primer `#` (lo que va tras «:» se corta). Para elegir categoría y orden, la primera
línea puede ser una cabecera que no se muestra:

    <!-- categoria: Programación/Go; orden: 10; titulo: Go -->

`categoria` es `Categoría` o `Categoría/Apartado` (el apartado es el lenguaje o el tema); `orden` ordena dentro del
apartado; `titulo` es un título corto para el índice. Sin cabecera, un archivo de `lenguajes/` va a «Programación» y
cualquier otro a «Conoce SaguiOS». Las versiones en otro idioma se llaman `nombre_EN.md` y `nombre_PT.md`.

Las guías de `docs/guia-kernel*.md` y `docs/guia-aplicaciones*.md` no se copian a mano: la compilación las deja en
`/ayuda/guias`.

## Desde el terminal

`ayuda -g TEMA` abre la Ayuda en ese tema (`ayuda -g lua`, `ayuda -g guia-kernel#el-arranque`); sin escritorio imprime
el documento con `markdown -t`. `ayuda TEMA` sigue explicando una orden y, si no hay tal orden pero sí un documento,
lo abre igual.
