# 134 — 3D en las 3dfx Voodoo Graphics y Voodoo2 (SST-1 / SST-2)

- **Estado:** pendiente (v8.5 o posterior; versión por decidir)
- **Prioridad:** media
- **Alta:** 2026-11-16

## Motivación

El mantenedor tiene en 86Box una máquina con una ViRGE/GX2 y una **Voodoo 1 o 2** (121a:0001/0002), el
acelerador 3D por excelencia de los PC de 1997-1999, que encaja con el foco del proyecto en equipos antiguos.
Son tarjetas **solo 3D**: van en cadena con la de vídeo (paso de la señal VGA) y no tienen 2D.

## Propuesta

- Un driver 3D para el SST-1 y el SST-2, cuyas especificaciones de registros están publicadas:
  - inicialización (memoria de cuadro y de texturas, DAC, relojes, paso a la salida de la Voodoo);
  - cola de órdenes;
  - preparación de triángulos (`triangleCMD`/`ftriangleCMD`) con Gouraud, Z, texturas (TMU), niebla y mezcla;
  - doble búfer.
- **SaguiGL** (D284/D287) con este dispositivo como motor: modo **a pantalla completa** a 640x480 o
  800x600 a 16 bits, porque la Voodoo sustituye la imagen de la tarjeta 2D mientras está activa. Al salir,
  vuelve a la salida VGA.
- Sagui 3D, sgldemo y los juegos SDL con GL (si los hay) lo usan cuando exista; si no, software, como ahora.
- Configuración > Aceleración gráfica: la fila 3D dice «Voodoo» y permite apagarlo.

## Pruebas

- 86Box: un preset con la Voodoo (1 y 2) más una tarjeta 2D.
- Autoprueba de triángulos comparando píxeles con SaguiGL por software, y la medida en fotogramas por segundo.

## Riesgos

- Pantalla negra si falla el paso de la salida VGA: el modo seguro (Mayús) tiene que desactivarlo.
- 86Box es la única referencia hasta que haya hardware real.
