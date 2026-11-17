# Tucán: editor de presentaciones (D369)

Programa `tucan` (en `/apps/bin`, como Pluma y Macaco; nombre de ave brasileña, como el resto de la suite). Diseño propio, sin
código, iconos ni gráficos de terceros. Fuentes: `userland/programas/tucan*.c|h` (interfaz), `userland/libsagui/presentacion*.c`
(modelo, dibujo, ficheros) y `userland/inc/presentacion.h`.

## Qué hace
- Editor: lista de miniaturas (arrastrar para reordenar, menú con duplicar, eliminar, ocultar, subir, bajar), lienzo 16:9 o 4:3,
  notas del orador, cinta con Inicio, Insertar, Diseño, Transiciones, Presentación y Ver. Panel del asistente IA (F12, no tapa nada).
- Objetos: cuadros de texto (fuente del sistema, cuerpo, N/K/S, color, alineación, viñetas y numeración con niveles: Tab y Mayús+Tab),
  formas (rectángulo, redondeado, elipse, triángulo, rombo, flecha, línea, estrella) con relleno y contorno, imágenes
  (BMP, PNG, JPEG, GIF), WordArt de Pluma, tablas, gráficos (barras, líneas, sectores; datos `etiqueta=valor,...`),
  mover, redimensionar con manijas, selección múltiple, agrupar, alinear, orden, copiar/pegar, deshacer/rehacer (40 pasos).
- Diseños: título, título y contenido, dos columnas, solo título, en blanco. Diez temas (colores, fuentes, fondo con degradado y
  decoración propia), fondo por diapositiva (color, degradado, imagen) y «maestra» sencilla: fuente de títulos y de cuerpo,
  número y pie de todas las diapositivas.
- Presentación (F5, Mayús+F5): pantalla completa, flechas/espacio/clic/rueda, transiciones (corte, fundido, desplazar,
  barrido), animaciones de entrada por objeto (aparecer, fundido) con un clic por objeto, láser (L), pantalla negra (B) o blanca (W),
  número + Intro para ir a una diapositiva, avance automático con tiempos (Presentación > Tiempos automáticos).
- Archivos: `.tcn` (propio), `.pptx` (leer y escribir), PDF (una página por diapositiva, o con notas), PNG (una o todas).
  Archivos abre `.pptx` y `.tcn` con Tucán.

## Formato .tcn
ZIP con `tucan.txt` (texto Latin-1, una línea por elemento) y `media/mN.png|jpg`. Líneas: `TUCAN 1`; `deck ancho= alto= tema= num= pie=
auto= ftit= fcue=` (nombre real de las fuentes); `pietxt`; `diap layout= trans= oculta= fondo= f1= f2= fm= auto= dur=`; `notas`;
`obj tipo= forma= rol= anim= alin= va= lista= est= tam= borde= rell= wa= graf= filas= cols= x= y= w= h= g= medio= relleno= bcol= col=
fuente=` seguido de `txt` con el texto. Escapes: `\s` espacio, `\n`, `\t`, `\xHH`, `\\`. Unidades: puntos (960x540 o 720x540).
El mismo texto sirve de instantánea para deshacer.

## PPTX
Escribe un paquete mínimo válido (maestra, maqueta, tema, notas, transiciones, fuentes por nombre). Lee lo de PowerPoint y
LibreOffice: textos con niveles y viñetas, formas básicas, imágenes, tablas, fondos, notas, pie y número, posición heredada de
la maqueta, transiciones. Se ignoran con aviso: animaciones, gráficos/objetos incrustados, formas a medida (simplificadas),
grupos (se aplanan). Los gráficos de Tucán se exportan como imagen y el WordArt como texto.

## Límites (decisiones)
Un solo formato por objeto (no por letra); sin rotación; sin vista de moderador ni ODP; gráficos propios, no los de Macaco
(Macaco es un único fichero de 10 000 líneas; extraerlos queda pendiente). Máximo 300 diapositivas y 80 objetos por diapositiva.
El PPTX no se ha podido abrir en PowerPoint real: se valida con ZIP/XML (tests/tucan_pptx_validar.py).

## Rendimiento (equipos antiguos)
El lienzo y las miniaturas (36) se guardan en caché y solo se redibujan al cambiar la diapositiva; arrastrar dibuja solo contornos;
las fotos grandes se reducen a 1600x1200 en memoria. `tucan --bench` (QEMU TCG): dibujo 1024x576 ~4 ms, 746x420 ~2 ms,
repintar desde la caché ~0,3 ms, PDF de 5 páginas 390 ms, PPTX 80 ms (reloj de 10 ms).

## Pruebas
`python3 tests/h380_presentaciones.py [a|b]`: A = formatos (sin ventana, validados en el anfitrión), B = interfaz y presentación
(capturas `docs/capturas/H380-*.png`). `python3 tests/tucan_pptx_validar.py`: unidad del validador.
Modos sin ventana: `tucan --convertir ent sal [--notas] [--diap N] [--ancho PX]`, `--info`, `--demo`, `--fuentes`, `--bench`.
