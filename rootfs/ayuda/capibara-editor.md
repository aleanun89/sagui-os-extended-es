<!-- categoria: Programación/Capibara; orden: 10; titulo: Editor de Capibara -->
# Capibara: el editor

El **editor de Capibara** (`/apps/bin/capibara-editor`, en **Inicio > Programación**) sirve para hacer juegos con el motor
Capibara (ver `capibara.md`) sin escribir las escenas a mano: se ve el nivel, se eligen y mueven los nodos con el ratón, se
pintan las casillas, se editan las propiedades en un panel y los guiones de Lua en un editor de código, y se prueba el juego
con un botón. Lo que guarda son los mismos ficheros de texto de siempre (`.esc`, `.lua`, `capibara.cfg`): **solo cambian las
líneas que tocas**, y los comentarios, el orden y los espacios del resto del fichero se quedan como estaban.

    capibara-editor                           abre el editor vacío
    capibara-editor /home/yo/mi_juego         abre un proyecto (una carpeta con capibara.cfg)
    capibara-editor --ejemplo=plataformas     copia el ejemplo a ~/capibara/plataformas y lo abre (o --ejemplo=disparos)

## La ventana

- **Árbol de la escena** (izquierda): los nodos del fichero de escena. Un clic elige; se arrastra un nodo sobre otro para
  hacerlo su hijo, o sobre el borde de arriba o de abajo de una fila para ponerlo antes o después. `[Instancia]` marca a los
  que son otra escena.
- **Lienzo** (centro): la escena dibujada por el propio motor, con la cuadrícula, las cajas de colisión y el marco de la
  pantalla del juego en verde. Rueda del ratón: zoom; botón central, o Espacio y arrastrar: desplazar la vista; **Inicio**:
  encuadrar la escena. Un clic elige el nodo de debajo (lo que hay dentro de una instancia se elige como la instancia entera);
  arrastrar en un hueco dibuja un rectángulo que elige todo lo que toca; Mayús o Ctrl y clic suman o quitan de la selección.
- **Inspector** (derecha): las propiedades del nodo elegido, sacadas de la tabla del motor con su tipo y sus límites
  (números con flechas o arrastrando, casillas, colores con selector, listas, texto y ficheros); además las **variables
  propias** (`var.nombre`, que en Lua son `self.nombre`) y las **señales** conectadas desde ese nodo (`destino:método`; vacío
  la quita).
- **Código** (bajo el lienzo, se despliega al abrir un guion): editor de Lua con resaltado. Ctrl+S guarda y comprueba la sintaxis.
- **Recursos** (abajo a la izquierda): las escenas, los guiones, las texturas y los sonidos del proyecto; doble clic abre una
  escena o un guion; arrastrar una textura al lienzo crea un `Sprite`, y una escena, una instancia.
- **Paleta** (abajo): las casillas de la textura del `MapaCasillas` elegido; se elige una o un bloque arrastrando.
- **Consola** (abajo a la derecha): los avisos del editor y la salida del juego, con filtros por nivel; el doble clic en un error de
  Lua lleva al guion y a su línea.

Los separadores entre paneles se arrastran, y la pastilla del medio pliega y despliega.

## Herramientas (barra)

| Botón | Qué hace |
| --- | --- |
| Guardar, Abrir | guarda la escena y el guion abiertos / abre otro proyecto |
| Deshacer, Rehacer | Ctrl+Z y Ctrl+Y (fuera del editor de código y del inspector); cada orden es un paso, y escribir seguido en el mismo campo, uno solo |
| Mover, Girar, Escalar | qué hacen las asas del nodo elegido: el cuerpo lo mueve (con **imán** a la rejilla), el círculo de arriba lo gira, las asas de los bordes lo escalan |
| Pintar, Goma | con un `MapaCasillas` elegido: el botón izquierdo pinta las casillas elegidas en la paleta, el derecho borra; **Goma** borra con los dos. La capa se cambia en **Ver > Capa de casillas siguiente** |
| Rejilla, Imán | enseñan la cuadrícula / ajustan lo que se mueve a ella |
| Jugar, Parar | guarda y lanza el juego en otra ventana (F5; F6 empieza por la escena abierta); Parar lo cierra (F7) |
| Exportar | crea un paquete `.capi` con todo el proyecto (Ctrl+E) |

## Menús y teclas

- **Archivo**: Nuevo proyecto (Ctrl+N, con una escena de plantilla y su guion), Abrir proyecto (Ctrl+O), los dos ejemplos, Nueva
  escena, Guardar (Ctrl+S), Exportar.
- **Editar**: Duplicar (Ctrl+D), Borrar (Supr), Renombrar (F2; arrastra a los hijos y a las conexiones), Añadir variable (F3),
  Conectar señal (F4).
- **Añadir**: un nodo de cada tipo (`Nodo`, `Nodo2D`, `Sprite`, `MapaCasillas`, `Camara2D`, `Etiqueta`, `Boton`, `Area`, `Cuerpo`,
  `Particulas`, `ReproductorAudio`, `Temporizador`) como hijo del elegido, y la instancia de otra escena.
- **Proyecto**: Jugar, Parar, **Crear guion del nodo** (con una plantilla con `_listo` y `_proceso`; Ctrl+G edita el guion del
  nodo o lo crea si falta) y Comprobar guion.
- **Ver**: rejilla, imán, colisiones, encuadrar, capa de casillas.

## Jugar y depurar

**Jugar** lanza `/apps/bin/capibara` con el proyecto y recoge lo que escribe (`print` de Lua, avisos del motor, errores): sale
en la consola, y un error de Lua se marca como error con su línea (`guiones/jugador.lua:12: ...`); doble clic y el código se
abre en esa línea. Mientras juega, el editor sigue usable; Parar cierra el juego.

## Cosas que conviene saber

- Los ejemplos de `/apps/capibara` son de solo lectura: `--ejemplo=` o Archivo > Abrir ejemplo los copian a tu carpeta y editas
  la copia.
- Las escenas se muestran **sin ejecutar los guiones** (así el editor no hace nada que el juego sí haría): para ver cómo se
  mueve, Jugar.
- El editor **no cambia los nombres de los ficheros ni reformatea nada**; si editas un `.esc` a mano mientras está abierto,
  vuelve a abrirlo.
- Todavía no hay editor de animaciones (la propiedad `animacion` se edita a mano en el fichero), ni depurador paso a paso.

La guía de las escenas y de la API de Lua es `capibara.md`; el diseño del editor, `docs/capibara.md` (apartado «El editor»).
