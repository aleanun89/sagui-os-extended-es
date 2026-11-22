# Archivos: el explorador de SaguiOS

Archivos (`arquivos [carpeta]`) es el explorador de carpetas. Desde la v8.5.5.2 tiene pestañas, panel doble,
vista previa y más; las ideas vienen de «Files» (el explorador moderno de Windows), con el aspecto de SaguiOS.

## Pestañas y paneles

- **Ctrl+T** abre una pestaña (en la página de Inicio), **Ctrl+W** la cierra, **Ctrl+Tab** pasa a la siguiente
  (con Mayús, a la anterior). Las pestañas se mueven arrastrándolas; la «x» o el botón central las cierran. La fila de
  pestañas aparece cuando hay más de una.
- **F3** (o el botón «Panel doble») muestra dos paneles a la vez. **Tab** pasa de uno a otro. Arrastra archivos de un
  panel a una carpeta del otro, a una pestaña o a una carpeta de la barra lateral: **copia** (con **Mayús**, mueve).

## Barra lateral

- **Inicio**: favoritos, unidades con su barra de espacio usado y lo abierto últimamente.
- **Favoritos**: arrastra una carpeta al título «FAVORITOS» (o **Ctrl+D**) para anclarla; botón derecho para quitarla.
- **Unidades**: cada volumen montado, con lo lleno que está; el pendrive y el CD/DVD, si los hay; la Papelera.
- **Etiquetas de color**: seis colores. Arrastra archivos a un color (o usa el botón de la etiqueta) para marcarlos; al
  pulsar un color se ven todos los marcados. Se guardan por cuenta en `~/.config/arquivos.etiquetas`.

## Ruta, vistas y búsqueda

- La ruta son **migas de pan**: pulsa una parte para ir a ella y la **flecha** que sigue para elegir una subcarpeta.
  **Ctrl+L** la edita; con **Tab** se completa.
- Vistas (**Ctrl+1** lista, **Ctrl+2** detalles, **Ctrl+3** iconos, **Ctrl+4** mosaicos) y el menú «Ver» para
  agrupar por tipo o por fecha y ordenar. En detalles, las columnas se ordenan al pulsarlas y se ensanchan arrastrando su
  borde. Las imágenes salen con **miniatura** en iconos y mosaicos.
- **Alt+P** muestra el panel de **vista previa**: imágenes, texto y código (con colores) y las propiedades (tamaño,
  fecha, permisos, dueño).
- **Ctrl+F** busca en la carpeta y en todas sus subcarpetas, en segundo plano; el botón «Filtros» limita por tipo, tamaño
  y fecha. Sin comodines, el texto es un trozo del nombre; con `*` y `?`, un patrón.
- En una carpeta de un **repositorio de git**, la barra de órdenes muestra la rama y cada archivo cambiado lleva `M`
  (modificado), `A` (añadido), `D` (borrado) o `?` (nuevo).

## Atajos

| Atajo | Hace |
|---|---|
| Ctrl+C, Ctrl+X, Ctrl+V | copiar, cortar, pegar |
| F2, Supr | renombrar, borrar |
| Ctrl+Mayús+N, Ctrl+N | carpeta nueva, ventana nueva |
| Alt+←, Alt+→, Alt+↑, Alt+Inicio | atrás, adelante, arriba, Inicio |
| Ctrl+H | mostrar u ocultar los archivos ocultos |
| F5 | actualizar |
