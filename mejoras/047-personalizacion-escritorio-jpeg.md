# 047 - Personalización del escritorio (fondo, colores) y JPEG en el visor

- **Estado:** parcial: fondo con imagen y color de acento hechos en la v5.2 (D112); faltan temas, degradado propio, vista previa y JPEG (v6)
- **Prioridad:** media
- **Alta:** 2026-11-07
- **Relacionadas:** 024 (visor y biblioteca de imágenes, D054), 031 (barra), 042 (componentes)

## Problema / motivación

El fondo (degradado) y los colores de ventanas, barra y menús están fijos en el kernel
(`kernel/gui/arte.c`, `area_trabalho.c`). El visor y la biblioteca de imágenes leen BMP, PNM
y PNG, pero **no JPEG**, que es el formato de casi todas las fotos (por ejemplo, las del móvil
que se copian a `/home` por la partición FAT32).

## Propuesta

### Fondo de escritorio
- Tipos:
  - color sólido;
  - degradado de dos colores (vertical u horizontal);
  - **imagen** (BMP, PNG, PPM y JPEG), con modos centrar, ajustar (sin deformar),
    rellenar (recortando), estirar y mosaico.
- La imagen se escala **una vez** al tamaño de pantalla y se guarda ya convertida. Al cambiar
  de resolución (D059) se vuelve a escalar. Redibujar el fondo cuesta lo mismo que ahora.
- El kernel no decodifica imágenes: el programa de ajustes las convierte con la libsagui y
  entrega los píxeles al kernel con una syscall (`SIS_FONDO`), o deja un archivo en bruto en
  `/home/.escritorio/fondo.raw` que el kernel carga al arrancar.

### Colores (temas)
- Paleta con nombre: barra, título activo/inactivo, bordes, menús, selección, texto y
  escritorio.
- Temas incluidos:
  - «SaguiOS» (el actual);
  - «Claro»;
  - «Oscuro»;
  - «Alto contraste» (accesibilidad);
  - «Retro», tipo 16 colores, para PC con vídeo de 16/15 bits (036).
- El kernel guarda la paleta en una tabla (en vez de constantes) y la libsagui la consulta
  (`INFO_TEMA`), para que los programas (componentes de la 042, tkinter, Mico) usen los
  mismos colores.

### Programa «Personalizar»
- Se abre desde el menú contextual del escritorio («Personalizar...») y desde Inicio →
  Sistema.
- Vista previa, selector de imagen (con miniaturas de `/imagenes` y `/home`), modo, colores y
  tema; botones «Aplicar» y «Guardar».
- Persistencia en `/home/.escritorio/config` si hay `/home` (pendrive con FAT32 o ext2).
- Opción de mostrar u ocultar los iconos y el lema de la barra.

### JPEG en la biblioteca de imágenes (D054)
- Decodificador **propio** a partir de ITU-T T.81 / JFIF:
  - *baseline* con Huffman;
  - submuestreo 4:4:4, 4:2:2 y 4:2:0;
  - escala de grises;
  - marcadores de reinicio;
  - EXIF ignorado, salvo la **orientación** (las fotos del móvil salen derechas);
  - **progresivo** si es razonable (muchas fotos web lo son).
- IDCT entera (rápida sin FPU) y, si la CPU lo tiene, versión SSE2.
- Decodificación con reducción ×1/2, ×1/4 o ×1/8 directamente en la IDCT, para miniaturas
  y fondos rápidos con fotos grandes de 12 MP.
- Lo usan el visor, Paint (abrir), Cipó, Markdown/HMKD (049), tkinter `PhotoImage` y el módulo
  `sagui` de Python.
- **Guardar JPEG** (codificador *baseline*) es opcional: útil en Paint, pero menos urgente.

## Criterios de aceptación

- [ ] Fondo con imagen (los cinco modos), color y degradado; persiste tras reiniciar
- [ ] Temas aplicados a ventanas, barra y menús; tkinter y los componentes los respetan
- [ ] JPEG *baseline* (y progresivo, si se hace) comparado píxel a píxel con un
      decodificador de referencia en el anfitrión (tolerancia de la IDCT), con imágenes de
      prueba generadas en el anfitrión; orientación EXIF
- [ ] Una foto de 12 MP se abre en el visor en un tiempo razonable en QEMU (reducción en la IDCT)
- [ ] Test automático en `tests/`
- [ ] Decisión registrada en `docs/decisoes.md`

## Parte hecha en la v5.2 (D112, rama `configuracion`)

- Aplicación **Configuración** (`configuracion.c`), sección Escritorio: fondo con una imagen BMP/PNG/PNM
  de `/imagenes` o `/home` (modos centrado, ajustado, rellenar, estirado y mosaico; degradado de
  siempre como opción) y **color de acento** entre ocho (barra de título y barra de tareas).
- El programa decodifica y escala; el kernel recibe los píxeles ya al tamaño de la pantalla por tandas
  de líneas (`SIS_FUNDO` 103) y el acento con `SIS_ACENTO` 104 (`INFO_APARENCIA` 58).
- Persistencia en `/home/.config/sagui.conf`, aplicada en el arranque por `configuracion --aplicar`.
- Test `tests/h83_configuracion.py`.

Pendiente de esta ficha: color sólido y degradado a elegir, temas con nombre (`INFO_TEMA`), vista
previa y miniaturas, reajuste del fondo al cambiar de resolución en caliente (hoy se reescala por
vecino más próximo) y todo lo de JPEG.
