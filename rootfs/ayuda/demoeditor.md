# demoeditor: demostración de los componentes de editor

**demoeditor** (`/bin/demoeditor`) monta con los componentes de editor de la libsagui (D454) una disposición de editor:
árbol de la escena | lienzo y código | inspector, y abajo los recursos (paleta de casillas) y la consola. Los datos son
sintéticos. La API está en `docs/componentes-editor.md`.

## Qué se puede probar

- **Paneles:** arrastrar un separador; la pastilla del medio pliega y despliega; «Guardar disposición» y «Restaurar».
- **Lienzo:** rueda = zoom (centrado en el ratón); botón central, o Espacio y arrastrar = desplazar; arrastrar en vacío = rectángulo de
  selección; con un objeto elegido, arrastrar el cuerpo lo mueve, las asas lo escalan y el círculo de arriba lo gira (Mayús: de 15 en 15).
  «Imán» y «Rejilla» activan o quitan el ajuste a la rejilla.
- **Inspector:** números con flechas, arrastre horizontal o tecleando; casilla, lista, deslizador, texto, color (selector con paleta, RGB,
  tono y alfa) y archivo.
- **Recursos:** elegir casillas (arrastrar un bloque); arrastrar las elegidas al lienzo crea un objeto por casilla.
- **Árbol:** elegir un objeto; arrastrarlo al lienzo lo coloca allí.
- **Código:** Lua con resaltado, sangría automática, Ctrl+F para buscar, Ctrl+Z / Ctrl+Y; «Comprobar código» marca el bloque sin cerrar.
- **Consola:** filtros por nivel, Copiar, Limpiar; doble clic en un error lleva el código a su línea.
- **Deshacer / Rehacer** (botones, o Ctrl+Z / Ctrl+Y fuera del código) cubren los movimientos, los cambios del inspector, los objetos
  creados y «Alinear todo» (un solo paso).
