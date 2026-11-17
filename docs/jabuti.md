# Jabuti: programar con bloques

**Jabuti** (programa `jabuti`, menú Inicio → Educación → «Jabuti (programar jugando)») es una aplicación de SaguiOS
para enseñar los fundamentos de la programación a niños de unos 6 a 14 años. Se arrastran bloques de colores, se encajan
unos con otros y los personajes del escenario hacen lo que dicen los bloques. Es una aplicación **original** de SaguiOS:
no usa código, nombres ni gráficos de ningún otro programa (la mascota es el sagui del escritorio). Funciona en equipos antiguos
(Pentium II/III, 128 MB) y a 800x600. Decisión: D377. Prueba: `tests/h379_jabuti.py`.

## Guía rápida para el maestro o la familia

1. Abrir **Jabuti**. Aparece el sagui en el escenario y, a la izquierda, las categorías de bloques.
2. **Elegir una categoría** (cada una tiene su color) y **arrastrar** un bloque al área de programa (el espacio en blanco del centro).
3. Arrastrar otros bloques y **acercarlos**: se encajan solos, con una barra blanca que avisa de dónde van a quedar.
4. **Pulsar la bandera verde** para ver qué pasa. El botón rojo para todo.
5. Probar cosas: cambiar los números (se pulsa en el hueco blanco y se escribe), añadir un personaje con el **+** de la derecha,
   pintar un traje en la pestaña **Trajes**, cambiar el fondo en **Fondos**.
6. **Guardar** con el botón o Ctrl+S: el proyecto es un archivo `.jbt` que se abre con doble clic en Archivos.

Lo más importante para empezar: *«al pulsar la bandera verde»* (categoría Eventos) es el bloque que dice «cuando empiece el programa»;
lo que se pone debajo, en fila, se ejecuta de arriba abajo.

### Qué se aprende con cada categoría

| Categoría | Qué enseña |
|-----------|------------|
| **Eventos** (amarillo) | Que un programa empieza cuando algo ocurre: la bandera, una tecla, un clic, un mensaje de otro personaje |
| **Movimiento** (azul) | Coordenadas (x, y), ángulos, direcciones, distancias |
| **Apariencia** (morado) | Cambiar el aspecto: decir, pensar, trajes, tamaño, mostrar y ocultar |
| **Sonido** (rosa) | Notas musicales (60 = do central) y efectos |
| **Lápiz** (verde azulado) | Dibujar con el programa: figuras geométricas y patrones |
| **Control** (naranja) | **Secuencia, repetición y decisión**: esperar, repetir, por siempre, si / si no, repetir hasta |
| **Sensores** (celeste) | Preguntar al usuario, leer el ratón y el teclado, saber si dos personajes se tocan |
| **Operadores** (verde) | Cálculos, números al azar, comparaciones, lógica (y / o / no), texto |
| **Variables** (naranja oscuro) | Guardar datos: puntos, vidas, nombres; listas |

## Actividades

Cada actividad cabe en 20-30 minutos. Los ejemplos de **Ejemplos** (barra superior) sirven de modelo o de punto de partida.

1. **Hola, sagui** (secuencia). «Al pulsar la bandera verde» → «decir ¡Hola! durante 2 segundos» → «mover 50 pasos» → «decir ¡Ya llegué!». ¿Qué pasa si se cambian de orden los bloques?
2. **El cuadrado** (repetición). Con el lápiz: «bajar el lápiz» → «repetir 4 { mover 80 pasos, girar 90 grados }». Después un triángulo (`repetir 3`, 120 grados) y un círculo (`repetir 36`, mover 6, girar 10). *Ejemplo: «Dibujar formas».*
3. **El sagui que salta** (eventos y variables de posición). «Al pulsar la tecla espacio» → repetir 12 { sumar 9 a y }, repetir 12 { sumar -9 a y }. *Ejemplo: «El sagui que salta».*
4. **¿Cómo te llamas?** (entrada y salida). «preguntar ¿Cómo te llamas?» → «decir unir "¡Hola, " con la respuesta». *Ejemplo: «Conversación».*
5. **Atrapa la fruta** (variables, condiciones, colisiones). Una cesta que se mueve con las flechas y una manzana que cae; cuando la cesta la toca, +1 punto. *Ejemplo: «Atrapa la fruta».*
6. **La carrera** (varios personajes y mensajes). Un personaje da la salida con «enviar mensaje1»; los demás empiezan con «al recibir mensaje1». *Ejemplo: «Carrera con mensajes».*
7. **Cuenta atrás** (bucles con condición). Una variable que baja de 5 a 0 con «repetir hasta que cuenta = 0» y un cohete que despega. *Ejemplo: «Cuenta atrás del cohete».*
8. **Mi propio juego.** Cada niño pinta su personaje en **Trajes** (o importa un dibujo), le pone un fondo y programa cómo se mueve.

Para el maestro: conviene que los niños *se equivoquen y lo deshagan* (Ctrl+Z recupera lo anterior) y que expliquen en voz alta qué hace cada bloque
antes de pulsar la bandera.

## La ventana

- **Barra superior:** bandera verde, botón rojo, Nuevo, Abrir, Guardar, Deshacer, Rehacer, Ejemplos, A- y A+ (tamaño de los jabuti: pequeño, normal, grande) y Ayuda (F1).
- **Pestañas:** **Programa**, **Trajes** y **Fondos**.
- **Izquierda:** categorías y paleta de bloques (con la rueda del ratón se desplaza).
- **Centro:** área de programa. Se desplaza arrastrando el espacio vacío o con la rueda; con el botón derecho: *Ordenar los grupos de bloques*.
- **Derecha:** el escenario (480x360 puntos lógicos; se muestra en cuatro tamaños con el botón **Tamaño**), **Pantalla** (a pantalla completa; Esc o F11 para volver) y debajo los personajes (clic para elegir, botón derecho: cambiar nombre, duplicar, borrar; **+** añade uno de la biblioteca, uno para pintar o una imagen).
- **Abajo:** barra de estado con mensajes en palabras sencillas.

### Arrastrar y soltar

- Un bloque se **engancha** debajo de otro, dentro del brazo de un bloque en «C» (repetir, por siempre, si, si no, repetir hasta) o encima de un grupo.
- Un bloque **redondo** (un número o texto) o **hexagonal** (una condición) va **dentro de un hueco**.
- Soltar un bloque sobre la paleta lo **borra**. Botón derecho sobre un bloque: *Duplicar* o *Borrar* (solo ese bloque).
- Un **clic** en un bloque ejecuta ese grupo (sirve para probar).
- Los huecos blancos se escriben pulsando en ellos (Intro o Tab acepta, Esc cancela); los de color son listas desplegables.

### Teclas

| Tecla | Acción |
|-------|--------|
| Ctrl+N, Ctrl+O, Ctrl+S | Nuevo, Abrir, Guardar |
| Ctrl+Z, Ctrl+Y | Deshacer, Rehacer |
| F11 | Pantalla completa |
| F1 | Ayuda |

## Trajes y fondos

Un **traje** es un dibujo de 32x32 cuadritos con una paleta de 31 colores; un **fondo**, de 48x36. El editor tiene **Lápiz**, **Goma**, **Cubo** (relleno),
31 colores, y los botones Nuevo, Duplicar, Borrar, Voltear, Limpiar, **Importar PNG** (también JPG, BMP y GIF; la imagen se reduce a la rejilla y, en los trajes, el color
liso del borde se vuelve transparente), Biblioteca y Nombre. Un personaje puede tener hasta 8 trajes (se cambian con *cambiar el traje a* y *siguiente traje*) y el
escenario hasta 6 fondos (*cambiar el fondo a*).

## Los bloques

(Los números llevan hasta tres decimales; las posiciones del escenario van de -240 a 240 en x y de -180 a 180 en y; la dirección 90 es hacia la derecha, 0 hacia arriba.)

- **Eventos:** al pulsar la bandera verde · al pulsar la tecla @ · al hacer clic en este personaje · al recibir @ · enviar @
- **Movimiento:** mover # pasos · girar a la derecha/izquierda # grados · ir a x: y: · ir a @ (ratón, azar, personaje) · deslizar # s a x: y: · apuntar en la dirección # · apuntar hacia @ · sumar # a x/y · fijar x/y a # · rebotar si toca un borde · estilo de giro · posición x, posición y, dirección
- **Apariencia:** decir/pensar (con o sin tiempo) · cambiar el traje a @ · siguiente traje · cambiar el fondo a @ · cambiar/fijar el tamaño · mostrar · ocultar · número de traje · tamaño
- **Sonido:** tocar la nota # durante # pulsos · tocar el sonido @ (pitido, salto, moneda, bocina, aplauso) · parar los sonidos
- **Lápiz:** bajar · subir · borrar todo · poner el color (12 colores con nombre) · poner el grosor · sellar · siguiente color
- **Control:** esperar # segundos · repetir # · por siempre · si · si / si no · esperar hasta que · repetir hasta que · detener (todo o este guion)
- **Sensores:** ¿tocando @? (borde, ratón, otro personaje) · ¿tocando el color @? · ¿tecla @ pulsada? · ¿ratón pulsado? · ratón x/y · preguntar y esperar · respuesta · cronómetro · distancia a @
- **Operadores:** + − × ÷ · número al azar entre # y # · > < = · y / o / no · unir · letra # de · longitud de · resto · redondear · valor absoluto · raíz cuadrada
- **Variables:** crear (botón en la paleta) · fijar · cambiar por · mostrar/ocultar (se ven en el escenario) · el valor · listas: agregar, vaciar, elemento # de, longitud

## Formato del proyecto (`.jbt`; los `.blq` de las primeras versiones siguen abriéndose)

Un `.jbt` es un **ZIP corriente** con dos archivos de texto: `proyecto.txt` (el proyecto, en Latin-1) y `leeme.txt`. El programa también abre un `proyecto.txt` suelto
(los ejemplos de `/ejemplos/jabuti` van así, sin comprimir). El texto se escribe a mano o se genera sin dificultad:

```
JABUTI 1                          # la primera línea; el resto de «#» es comentario
fondo 1                            # índice del fondo que se ve (desde 0)
f "Cielo" biblioteca cielo         # un fondo de la biblioteca (blanco, cielo, pasto, noche, espacio, cancha, mar)
f "Mi fondo" pintado               # un fondo propio: debajo, 36 filas de 48 caracteres
  ................................................
variable "puntos" 0 1              # nombre, valor y si se muestra en el escenario (1/0)
lista "cosas" "a" "b"              # nombre y elementos
personaje "Sagui" biblioteca sagui # o solo el nombre, con trajes «pintado» debajo
  pos 0 -110 90 130 1 2 0          # x y dirección tamaño(%) visible(1/0) giro(0 todo, 1 izquierda-derecha, 2 ninguno) traje
  traje "mono" pintado             # debajo, 32 filas de 32 caracteres
    ................................
  guion 20 40                      # un grupo de bloques; x y en el área de programa
    bandera                        # una línea por bloque; la sangría (dos espacios) indica lo que está dentro de un bloque en C
    repetir 10
      mover 10
      girar-der 15
    si-no (tocando "borde")        # entre paréntesis, los bloques redondos y hexagonales de los huecos
      decir "¡Ay!"
    sino                           # la rama «si no»
      mover 1
```

Los caracteres de las filas de píxeles son `.` (transparente) y `0`-`9`, `a`-`u` (los 31 colores de la paleta, en el orden de `jb_paleta` en `userland/jabuti/jb_datos.c`).
Los números y los textos entre comillas; un texto lleva `\"` y `\\` para la comilla y la barra. Cada bloque tiene un nombre fijo en ASCII (`mover`, `girar-der`, `repetir-hasta`, `lapiz-bajar`,
`fijar-var`...); la tabla completa está en `userland/programas/jabuti_ops.h`.

### Línea de órdenes

`jabuti --ejecutar proyecto.jbt [--cuadros N] [--tecla NOMBRE@CUADRO] [--clic X,Y@CUADRO] [--respuesta TEXTO] [--guardar salida.jbt]` corre el proyecto sin ventana
(30 cuadros por segundo simulados) y escribe la posición de los personajes y el valor de las variables; `jabuti --abrir proyecto.jbt [--texto] [--guardar salida.jbt]` lo valida,
lo escribe legible (`--texto`) o lo convierte a ZIP. Es lo que usan las pruebas.

## Cómo funciona por dentro

Cada grupo que arranca es un **hilo verde** del intérprete: un puntero al bloque actual y una pila de marcos para los bloques en C. Cada cuadro (1/30 s) cada hilo avanza hasta que cede el turno:
al final de cada vuelta de un bucle o en un bloque que espera. Así varios personajes (hasta 16) corren a la vez, sin hilos reales y sin FPU. La interfaz recuerda,
por cada pasada de dibujo, dónde está cada bloque, cada hueco y cada punto de conexión; con ellos resuelve el clic, el arrastre y el encaje. Ver D377 en `docs/decisoes.md`.
