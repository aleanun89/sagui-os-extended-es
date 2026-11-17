# Java en SaguiOS (miniJVM, D376)

SaguiOS ejecuta programas de Java con **miniJVM**, una JVM pequeña en C con su propia biblioteca de clases (subconjunto de
Java 8). El programa es `/apps/bin/java`; la biblioteca, `/apps/java/lib/minijvm_rt.jar`.

## Instalar (en el anfitrión, sin sudo)

```bash
tools/java_entorno.sh instalar      # Temurin JDK 17.0.21 en ~/.local/saguitools/jdk (SHA-256 comprobado)
tools/minijvm_fuentes.sh            # las fuentes de miniJVM (commit fijo, SHA-256) en userland/terceros/minijvm/fuentes
. ~/.local/saguitools/env.sh && make
```
Es idempotente y nada de esto va a git. `env.sh` añade `jdk/bin` al **final** del `PATH`. Sin fuentes o sin JDK, `make` salta Java
con un aviso. `JAVA=0` lo desactiva (y la variante sin GPL, `SEM_GPL=1`, lo deja fuera: la biblioteca de clases lleva partes de OpenJDK bajo GPL-2.0).

## Ejecutar

```
java Hola                          # Hola.class en el directorio actual
java -cp /apps/java/ejemplos Hola mundo
java -cp x.jar:otro.jar paquete.Clase
java -jar app.jar                  # usa Main-Class del manifiesto
java -Xmx64M Clase                 # limita el montón (por defecto 128 MB)
java -version
```
`System.exit(n)` da el código de salida; una excepción sin capturar sale con 1. Los avisos `[INFO]` de la VM se ocultan; con
`JAVA_INFO=1` se ven.

## Compilar `.java`

En el PC: `tools/javac-sagui -d salida Fuente.java`. Es el `javac` del JDK con `-source 8 -target 8` y
`-bootclasspath build/java-res/lib/minijvm_rt.jar`: si usas una clase que miniJVM no tiene, no compila. `make` compila
`userland/java/ejemplos/*.java` y los instala en `/apps/java/ejemplos/`.

**Dentro de SaguiOS (sin el PC):** la orden `javac` usa **Janino 3.1.12** (BSD-3) sobre la propia miniJVM
(`tools/java_entorno.sh janino` baja los jars; `make` los pone en `/apps/java/lib/`):

```
javac [-d directorio] [-cp ruta:ruta] Fuente.java...
java Hola.java [argumentos]          # compila a /tmp/java-run-PID y ejecuta la clase «Hola»
```
Janino cubre casi todo Java 8 pero no todo (genéricos avanzados, algunas lambdas); es lento en una máquina emulada (unos 13 s
de arranque en QEMU sin KVM). Para programas grandes, compilar en el PC con `tools/javac-sagui`. `java X.java` supone que la
clase principal se llama como el fichero.

## Ejemplos (`userland/java/ejemplos/`, comentados en español)

Consola: `Hola`, `Argumentos` (código de salida), `Ficheros`, `Colecciones`, `Hilos` (synchronized, wait/notify),
`Excepciones`, `Adivinar` (juego con entrada estándar), `Acentos`, `Bucle` (rendimiento).
Con ventana (`sagui.gui`): `Calculadora`, `BlocNotas` (menú y diálogos de fichero) y `Serpiente` (Lienzo y temporizador).

## Biblioteca gráfica `sagui.gui`

Paquete propio (no es Swing ni AWT) en `userland/java/sagui/gui/`, compilado a `/apps/java/lib/sagui.jar`, que `java` y `javac`
añaden siempre al classpath. Las clases son Java; solo la clase `Nativo` tiene métodos nativos (`gui_nativos.c`), que llaman a la
libsagui (ventana, rectángulos, líneas, elipses, texto, imágenes PNG/JPEG/BMP/GIF, fuentes TrueType/OpenType, diálogos y
portapapeles). Mismo modelo que el módulo `sagui`/`tkinter` de MicroPython: el programa pinta en el búfer de la ventana y recibe
eventos.

| Clase | Qué es |
|---|---|
| `Ventana` | ventana con componentes; `ejecutar()` es el bucle de eventos; `cadaMilisegundos(ms, tarea)` temporizadores; `barraMenu` |
| `Etiqueta`, `Boton`, `CampoTexto`, `Casilla`, `Lista` | componentes (el `Boton` ejecuta una `Accion`) |
| `BarraMenu`, `Menu` | menús desplegables con acciones y separadores |
| `Lienzo` + `Grafico` | dibujo 2D: `linea`, `rect`, `rectRelleno`, `elipse`, `elipseRellena`, `texto`, `imagen`; eventos `ratonPulsado`, `ratonMovido`, `teclaPulsada` |
| `Dialogo` | `mensaje`, `confirmar`, `pedirTexto`, `abrirFichero`, `guardarFichero` (los diálogos del sistema) |
| `Imagen` | PNG/JPEG/BMP/GIF; `Fuente` | la del sistema (8x16) o una vectorial (`Fuente.delSistema()` lista `/apps/fuentes`) |
| `Eventos`, `Colores`, `Accion` | constantes de teclas y eventos, tema y el interfaz de acciones |

```java
Ventana v = new Ventana("Hola", 300, 120);
v.agregar(new Etiqueta("Hola desde Java", 20, 20));
v.agregar(new Boton("Salir", 100, 60, 90, 28, new Accion() {
    public void ejecutar(Componente origen) { v.cerrar(); }
}));
v.ejecutar();
```
Las ventanas necesitan el escritorio (no el modo `--console`). Todo el código de interfaz corre en el hilo que llama a
`ejecutar()`. Límites: sin disposiciones automáticas (posiciones absolutas), el recorte no afecta al texto de las fuentes
vectoriales, el campo de texto es de una línea.

## Acentos (Latin-1)

El terminal espera Latin-1 y Java usa UTF-8/UTF-16. `System.out` convierte de UTF-8 a Latin-1 (lo que no cabe, como el euro,
sale como `?`); `System.in` no se convierte (Java decodifica la entrada estándar como ISO-8859-1 por defecto, igual que manda el terminal: la `ñ` llega como un solo carácter); los argumentos de la línea de órdenes se pasan de Latin-1 a UTF-8. Es lo mismo que hace TinyGo
(D364). Los ficheros se escriben tal cual los pida el programa (`getBytes("UTF-8")`).

## Qué hay y qué no

- Hay: lenguaje Java 8 (lambdas, genéricos, clases internas), `java.lang`, `java.util` (colecciones), `java.io` (ficheros),
  hilos (cada hilo de Java es un hilo de SaguiOS, D167), `java.net` con sockets TCP/UDP (sin TLS), reflexión básica.
- No hay: Swing, AWT, JNI por bibliotecas dinámicas, `Runtime.exec`, TLS, depurador JDWP, JIT (miniJVM no lo tiene en 32 bits).

## Cómo funciona

`userland/terceros/minijvm/` (ver D376): `java.mk` (compilación), `plataforma/` (código propio: pthreads sobre hilos y futex,
sockets, consola Latin-1, `java_main.c`), `parches/` (cambios a miniJVM: `System.in` y los lectores de texto devuelven por líneas) y las fuentes bajadas.

## Licencias

miniJVM es MIT (c) 2023 Gust. `minijvm_rt.jar` incluye ficheros derivados de OpenJDK bajo GPL-2.0-only con Classpath-exception
(el jar lleva `META-INF/MIT_LICENSE`, `LICENSE`, `CLASSPATH_EXCEPTION`, `NOTICE`); los `.class` de tus programas no heredan esa
licencia. Registrado en `docs/terceros.md`.

## Rendimiento y memoria

Medido en QEMU **sin KVM** (TCG, así que en máquina real es bastante más rápido), 256 MB y 512 MB de RAM, `h378`/`tests` con el
terminal de consola:

| Prueba | 256 MB | 512 MB |
|---|---|---|
| `java Hola` completo (orden en el shell, arranque de la VM, salida, fin) | 2,4 s | 2,6 s |
| `Bucle 5`: 5 millones de vueltas enteras (`suma += i % 7`) | 1720 ms | 1760 ms |
| flotante: 500 000 divisiones | 210 ms | 240 ms |
| 500 000 `new int[8]` | 460 ms | 440 ms |
| `javac` de un fichero vacío (Janino: arranque y carga) | 13,3 s | 13,4 s |

La VM es un intérprete (sin JIT: miniJVM no lo tiene en 32 bits): unos 3 millones de bucles/s en TCG. El montón por defecto
es de 128 MB (`-Xmx`); con 256 MB de RAM caben los ejemplos y Janino (`javac` compila `Hi.java` sin problema). El kernel marca
`RAM del PC` 261 631 / 523 775 KB y la VM solo toma memoria al crecer el montón (Immix por bloques; los avisos con `JAVA_INFO=1`).

## Límites y pendientes conocidos

- `Runtime.exec`, TLS desde Java, JNI por bibliotecas dinámicas: no soportados.
- Los nombres de fichero con acentos pasan tal cual (UTF-8 de Java a bytes del sistema).
- Las capturas de la prueba están en `docs/capturas/H378-*.png`.
