# Jabuti: programar con bloques

**Jabuti** (la tortuga brasileña) es una aplicación de SaguiOS para aprender a programar jugando. Se arrastran bloques de colores,
se encajan unos con otros y se ve cómo los personajes hacen lo que dicen los bloques. Es una aplicación
original de SaguiOS, hecha para ir bien en ordenadores antiguos (funciona a 800x600 y con 128 MB de memoria).

## La ventana

- **Arriba:** la bandera verde (empieza el programa), el botón rojo (para todo) y los botones de Nuevo,
  Abrir, Guardar, Deshacer, Rehacer, Ejemplos, A- / A+ (tamaño de los bloques) y Ayuda.
- **A la izquierda:** las categorías de bloques, cada una de un color: Eventos, Movimiento, Apariencia,
  Sonido, Lápiz, Control, Sensores, Operadores y Variables.
- **En el centro:** el área de programa, donde se arrastran los bloques.
- **A la derecha:** el escenario y debajo los personajes. El botón **Pantalla** muestra el escenario a pantalla
  completa (Esc para volver) y **Tamaño** lo cambia entre cuatro tamaños.
- Las pestañas **Programa**, **Trajes** y **Fondos** cambian entre programar y pintar.

## Programar

1. Elige una categoría y arrastra un bloque al área de programa.
2. Los bloques se **encajan** solos cuando los acercas: verás una barra blanca donde van a quedar.
3. Los bloques en forma de «C» (**repetir**, **por siempre**, **si**, **si no**) llevan otros bloques dentro.
4. Los huecos blancos son para escribir números o texto: pulsa en ellos y escribe. Los huecos de colores
   son listas desplegables. Los bloques redondos (números) y los hexagonales (condiciones) se arrastran
   **dentro de los huecos**.
5. Pulsa la **bandera verde** para ejecutar todos los grupos que empiezan con «al pulsar la bandera verde».
   Pulsando un bloque se ejecuta solo ese grupo.
6. Para borrar un bloque, arrástralo otra vez a la paleta. Con el botón derecho: Duplicar o Borrar.
7. **Deshacer** (Ctrl+Z) y **Rehacer** (Ctrl+Y) recuperan lo que hayas hecho o deshecho.

Cada personaje tiene sus propios grupos de bloques, y todos corren a la vez.

## Atajos

| Tecla | Acción |
|-------|--------|
| Ctrl+N / Ctrl+O / Ctrl+S | Nuevo, Abrir, Guardar |
| Ctrl+Z / Ctrl+Y | Deshacer, Rehacer |
| F11 | Pantalla completa |
| F1 | Esta ayuda |

## Personajes, trajes y fondos

El botón **+** del panel de personajes añade uno de la biblioteca (sagui, pelota, cohete, estrella,
manzana, cesta, pez, flor), uno para pintar o una imagen importada. En **Trajes** y **Fondos** hay un editor
de pixel art con lápiz, borrador y relleno, 31 colores, y botones para importar un PNG, JPG o BMP.

## Guardar y abrir

Los proyectos se guardan como archivos `.jbt` (un ZIP con `proyecto.txt`, un texto legible). El formato está
descrito en `docs/jabuti.md`. En **Ejemplos** hay siete proyectos listos para abrir y modificar.

## Si algo no sale

Los mensajes aparecen abajo, en la barra de estado, con palabras sencillas. Cualquier cambio se puede
deshacer con Ctrl+Z.
