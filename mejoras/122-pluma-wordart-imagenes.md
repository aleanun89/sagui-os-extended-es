# 122 - Pluma: WordArt e imágenes mejoradas

- **Estado:** hecha (v8.3.1, D296)
- **Prioridad:** media
- **Alta:** 2026-11-15

## Problema / motivación

Pluma (mejora 116) no tenía imágenes ni títulos decorativos. Un procesador de textos al estilo de Word 97 los echa de menos, y el
resto del sistema (Macaco, el Visor de PDF) se beneficia de un escritor de PDF que admita imágenes.

## Propuesta

- **Objetos en el texto** (`edicao_rica.c`, `edicao_rica_obj.c`): una imagen o un WordArt ocupa un carácter (`RTE_OBJ_CHAR`) con su
  entrada en la tabla de formatos. Se seleccionan con un clic (8 tiradores; con Mayús las esquinas redimensionan sin proporción), se
  mueven arrastrando y el doble clic abre su diálogo. Cada cambio crea una versión nueva del descriptor que comparte los datos
  pesados, así que deshacer lo recupera todo.
- **WordArt** (`wordart.c`): galería de 20 estilos propios (degradados, arco iris, rayas, metal, contorno, sombra, 3D, arco arriba y
  abajo, onda, subida, cursiva, perspectiva) y diálogo con texto, fuente, tamaño, N y C. Letras vectoriales DejaVu con la libfontes
  si el programa la enlaza (Pluma), o la fuente 8x16 ampliada con suavizado si no.
- **Imágenes**: Insertar > Imagen (BMP, PNG, JPEG baseline y GIF) con el diálogo común, «Formato de imagen» (cm y %, proporción,
  ajuste en línea o arriba y abajo, alineación, borde, pie) y pegado desde el portapapeles del sistema (`SIS_COLAR_IMAGEM`).
- **RTF**: `\pict` con `\pngblip` o `\jpegblip` (el JPEG y el PNG originales se incrustan sin tocar; BMP, GIF y WordArt se codifican
  como PNG con el nuevo `imagem_codificar_png`), más `{\*\saguiobj}` y `{\*\saguiwordart}` para reeditarlos. Lee `\shppict`,
  `\nonshppict`, `\picscale*`, `\picwgoal` y `\bin` de Word y LibreOffice.
- **PDF**: `pdfw_imagem_rgb` (RGB de 8 bits con Flate) y `pdfw_imagem_jpeg` (DCTDecode tal cual) en `pdf_escribir.c`.
- Modos sin ventana: `pluma --wordart`, `pluma --imagem`, y `--info` cuenta objetos.

## Criterios de aceptación

- [x] 20 estilos de WordArt generan un PNG válido y variado en el RTF.
- [x] PNG y JPEG se incrustan con los mismos bytes; BMP y GIF salen como PNG sin pérdida.
- [x] Se leen los RTF de Word y de LibreOffice con imágenes.
- [x] El PDF lleva XObject de imagen validado por `tests/pdf_validar.py`.
- [x] Prueba `tests/h296_pluma_wordart.py` con capturas `docs/capturas/H296-*.png`.
- [x] Decisión D296.

## Notas (no hecho)

- Ajuste «cuadrado» (el texto rodea la imagen): exige que la maquetación admita líneas de ancho variable; se dejó fuera.
- Recortar imagen: no hecho.
- El pie de imagen y el ajuste solo se guardan en el grupo propio: Word ve la imagen pero no el pie.
- Vista previa en el diálogo de abrir imagen: no hecha.
