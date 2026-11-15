# 132 — Componente «cinta» (menú Ribbon) con el estilo de SaguiOS, en la libsagui y en VisualPythonS (v8.4.4)

- **Estado:** pendiente (v8.4.4)
- **Prioridad:** media-alta
- **Alta:** 2026-11-15

## Petición

El mantenedor (2026-11-15): «hay un componente que me gusta mucho, los menús Ribbon. ¿Podrías tener ese
componente, pero con un estilo que cuadre con el de nuestro OS, tanto en VisualPython como en la biblioteca
estándar, para aplicarlo a herramientas?»

## Qué hay

La libsagui (`userland/libsagui/gui_barras.c`, mejora 116, D291) tiene:

- barra de menús (`barra_menu_*`);
- barra de herramientas (`barra_ferr_*`);
- pestañas (`gui_pestanas.c`).

Todo tiene aspecto Windows 98 (`g_bisel`, colores `W98_*`). No hay cinta.

## Propuesta

**Cinta (`cinta_t`) en la libsagui, en C**, en un `gui_cinta.c` nuevo:

- **Pestañas.** Una fila arriba (Inicio, Insertar, Diseño…), más:
  - una **pestaña «Archivo»** de otro color que abre un panel o menú (Nuevo, Abrir, Guardar, Imprimir, Salir);
  - **pestañas contextuales** que solo salen con algo elegido (p. ej. «Tabla» o «Imagen»).
- **Grupos.** Cada pestaña lleva grupos con su rótulo debajo y un separador. Algunos tienen el botón «lanzador»
  en la esquina, que abre el diálogo completo.
- **Controles** dentro de un grupo:
  - botón grande (icono de 32 px y texto debajo) y botones pequeños (16 px, con o sin texto) en columnas de 3;
  - botón con desplegable y botón dividido;
  - botón de dos estados y grupo de botones exclusivos (alinear);
  - lista desplegable y lista de fuentes (`combo_fuentes`, D327), campo numérico;
  - selector de color con flecha (`dialogo_cor`, D327);
  - galería (estilos), con flecha para abrirla entera.
- **Barra de acceso rápido** (Guardar, Deshacer, Rehacer) arriba o debajo de la cinta. **Minimizar la cinta**
  con doble clic en una pestaña, para dejar solo las pestañas.
- **Teclado:**
  - Alt muestra letras de acceso (KeyTips) sobre las pestañas y luego sobre los controles;
  - flechas y Tab;
  - Ctrl+F1 minimiza.
- **Tamaño adaptable:** si la ventana estrecha, los grupos se reducen por orden de prioridad (grande → pequeño
  con texto → solo icono → el grupo entero como un botón desplegable).
- **Información sobre la herramienta:** título, descripción y atajo.
- **Estilo SaguiOS**, no una copia del de Office:
  - colores del tema y del **acento** del sistema (Configuración, D112) y la letra del sistema;
  - pestaña activa unida al panel;
  - grupos con biseles suaves coherentes con `barra.c`/`arte.c`;
  - los iconos del conjunto de arte del sistema, con más si hace falta.
  - Se ve bien en 800×600.
- **API declarativa**: la cinta se describe con tablas de C (pestaña → grupo → control con id, texto, icono,
  atajo y tamaño). Como `barra_ferr_tratar`, `cinta_tratar` devuelve el id elegido, y `cinta_marcar` y
  `cinta_activar` cambian el estado.
- Multiidioma (D313): los textos pasan por `_()`.

**VisualPythonS:**

- un componente `Cinta` con `PestanaCinta`, `GrupoCinta` y los controles de arriba, expuestos desde la cinta de C
  (`sagui`) o dibujados en Python con el mismo aspecto;
- en el diseñador:
  - se coloca arriba;
  - editor visual de pestañas, grupos y botones;
  - iconos del sistema a elegir;
  - evento `clic` por control;
- manual y ejemplo `/ejemplos/vps/cinta`.

**Aplicarlo a herramientas.** Primero, como prueba de que sirve, **Pluma** y **Macaco**, con
Inicio/Insertar/Diseño/Fórmulas/Datos/Vista. Debe poder volverse a los menús clásicos en Configuración o en
Vista, para quien prefiera el estilo 98. Más adelante, Paint, PlanoMini y el Correo.

## Pruebas

- Una prueba hNN con capturas de la cinta en Pluma y Macaco, a 800×600 y a 1024×768, que compruebe:
  - clic en un botón grande, uno pequeño y uno desplegable;
  - KeyTips con Alt;
  - minimizar y reducción de grupos al estrechar.
- Un ejemplo de VisualPythonS en h299.
- Las pruebas de Pluma y Macaco siguen pasando.
