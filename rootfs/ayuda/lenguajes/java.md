<!-- categoria: Programación/Java; orden: 50; titulo: Java -->
# Java en SaguiOS: programas con miniJVM

**Java** es uno de los lenguajes más usados del mundo: un mismo programa corre en cualquier sistema que tenga una
**máquina virtual** (la JVM). En SaguiOS la máquina virtual es **miniJVM**, pequeña y escrita en C, y entiende
Java 8. Hay dos órdenes: `java` (ejecuta) y `javac` (compila), y las dos funcionan **dentro** de SaguiOS.

Java es más largo que Python para empezar, pero enseña muy bien las ideas de clases, listas y ventanas. Si
quieres algo más corto, empieza por [Python](python.md); para un programa rápido, por [C](c.md).

## Primer programa, paso a paso

**1.** En el Terminal, crea una carpeta y entra en ella:

    mkdir java
    cd java

**2.** Escribe el archivo con el editor. El nombre del archivo tiene que ser igual que el de la clase
(`Hola.java` para `public class Hola`). En el Terminal:

    mico Hola.java

**3.** Escribe:

```java
// Hola.java: el primer programa en Java
public class Hola {
    public static void main(String[] args) {
        System.out.println("Hola desde Java en SaguiOS");
        System.out.println("2 + 3 = " + (2 + 3));
    }
}
```

**4.** Guarda con **Ctrl+S**. Hay dos formas de ejecutarlo:

    java Hola.java          # compila y ejecuta de una vez (lo más cómodo)

o en dos pasos:

    javac Hola.java         # compila: crea Hola.class
    java Hola               # ejecuta la clase

> Mico no ejecuta `.java` con F5 de serie: en el Terminal, la orden es la de arriba.

**Aviso de tiempo:** `javac` (que está hecho en Java con la biblioteca Janino) tarda bastante en arrancar,
unos 13 segundos en una máquina virtual sin aceleración. La segunda vez que ejecutes el mismo `.class` con
`java Hola` es casi inmediato.

## Ejecutar programas que ya existen

    java -cp /apps/java/ejemplos Hola mundo    # una clase de una carpeta, con argumentos
    java -jar app.jar                          # un .jar (usa la clase principal del manifiesto)
    java -Xmx64M Clase                         # limitar la memoria (por defecto 128 MB)
    java -version

Los ejemplos del sistema están en `/apps/java/ejemplos/` (en consola: `Hola`, `Argumentos`, `Ficheros`,
`Colecciones`, `Hilos`, `Excepciones`, `Adivinar`, `Acentos`, `Bucle`; con ventana: `Calculadora`, `BlocNotas`,
`Serpiente`). Los fuentes están en `userland/java/ejemplos/` si quieres leerlos.

`System.exit(n)` termina con el código `n` (míralo con `echo $?`). Si el programa lanza una excepción sin
capturar, sale con código 1 y muestra la pila de llamadas.

## Lo básico del lenguaje

### Variables y tipos

```java
public class Tipos {
    public static void main(String[] args) {
        int edad = 12;
        double altura = 1.45;
        char letra = 'S';
        String nombre = "Tito";                  // String es texto, con mayúscula
        System.out.println(nombre + " tiene " + edad + " años");
        System.out.println("mide " + altura + " m, inicial " + letra);
    }
}
```

Java es **estricto** con los tipos: `int`, `double`, `char`, `boolean`, y `String` para el texto (que no es un
tipo básico, sino una clase).

### Condiciones y bucles

```java
public class Bucles {
    public static void main(String[] args) {
        for (int i = 1; i <= 5; i++) {
            System.out.println(i + " x 2 = " + (i * 2));
        }

        int n = 3;
        while (n > 0) {
            System.out.println("cuenta atrás: " + n);
            n--;
        }

        if (n == 0) {
            System.out.println("¡despegue!");
        } else {
            System.out.println("todavía no");
        }
    }
}
```

### Métodos

```java
public class Metodos {
    static int cuadrado(int x) {
        return x * x;
    }

    public static void main(String[] args) {
        System.out.println("el cuadrado de 7 es " + cuadrado(7));
    }
}
```

### Listas y mapas

```java
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

// Sin genéricos: el javac de SaguiOS (Janino) no los entiende del todo.
public class Colecciones {
    public static void main(String[] args) {
        List notas = new ArrayList();
        notas.add(Integer.valueOf(7));
        notas.add(Integer.valueOf(9));
        notas.add(Integer.valueOf(10));
        int suma = 0;
        for (int i = 0; i < notas.size(); i++) {
            suma += ((Integer) notas.get(i)).intValue();
        }
        System.out.println("media: " + suma / notas.size());

        Map cuenta = new HashMap();
        String[] palabras = "el sagui sube y el sagui baja".split(" ");
        for (int i = 0; i < palabras.length; i++) {
            String p = palabras[i];
            Integer antes = (Integer) cuenta.get(p);
            cuenta.put(p, antes == null ? Integer.valueOf(1) : Integer.valueOf(antes.intValue() + 1));
        }
        System.out.println("el aparece " + cuenta.get("el") + " veces");
    }
}
```

El ejemplo no usa genéricos (`List<Integer>`, `Map<String, Integer>`): el `javac` de SaguiOS (Janino) falla con ellos.
Los programas que usan genéricos compílalos en el PC (`tools/javac-sagui`), que sí los acepta.

### Clases

```java
public class Sagui {
    private String nombre;

    public Sagui(String nombre) {
        this.nombre = nombre;
    }

    public String saludar() {
        return "Soy " + nombre;
    }

    public static void main(String[] args) {
        System.out.println(new Sagui("Tito").saludar());
    }
}
```

## Archivos

```java
import java.io.FileWriter;
import java.io.PrintWriter;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;

public class Archivos {
    public static void main(String[] args) throws IOException {
        PrintWriter w = new PrintWriter(new FileWriter("/tmp/notas.txt"));
        w.println("plátanos");
        w.println("piñas");
        w.close();

        BufferedReader r = new BufferedReader(new FileReader("/tmp/notas.txt"));
        String linea;
        while ((linea = r.readLine()) != null) {
            System.out.println("> " + linea);
        }
        r.close();
    }
}
```

`System.out` convierte el texto UTF-8 de Java a Latin-1 para la pantalla; lo que no existe en Latin-1 (como el
símbolo del euro) sale como `?`.

## Ventanas con `sagui.gui`

SaguiOS trae su propia biblioteca gráfica, `sagui.gui`. No es Swing ni AWT. Sus clases principales son:

| Clase | Qué es |
|-------|--------|
| `Ventana` | la ventana; `agregar` pone componentes y `ejecutar()` es el bucle de eventos |
| `Etiqueta`, `Boton`, `CampoTexto`, `Casilla`, `Lista` | componentes (el `Boton` ejecuta una `Accion`) |
| `Lienzo` | dibujo: `linea`, `rect`, `elipse`, `texto`, `imagen`; eventos del ratón y del teclado |
| `Dialogo` | `mensaje`, `confirmar`, `pedirTexto`, `abrirFichero`, `guardarFichero` |

```java
import sagui.gui.*;

public class Saludo {
    public static void main(String[] args) {
        final Ventana v = new Ventana("Hola", 300, 120);
        v.agregar(new Etiqueta("Hola desde Java", 20, 20));
        v.agregar(new Boton("Salir", 100, 60, 90, 28, new Accion() {
            public void ejecutar(Componente origen) {
                v.cerrar();
            }
        }));
        v.ejecutar();
    }
}
```

Compila y ejecuta igual (`java Saludo.java`). Las ventanas necesitan el escritorio (no funcionan en la consola).
Todo el código de la interfaz corre en el hilo que llama a `ejecutar()`. Las posiciones son absolutas: no hay
disposición automática. Para dibujar, `Lienzo` y `Grafico` son el sitio. Ejemplos completos: `Calculadora`,
`BlocNotas` y `Serpiente` en `/apps/java/ejemplos/`.

## Errores típicos

| Lo que ves | Qué pasa | Qué hacer |
|------------|----------|-----------|
| El compilador se queja de `public class` y el nombre del archivo | la clase pública tiene que llamarse como el archivo | cambia el nombre del archivo o de la clase, y compila otra vez |
| Un símbolo no se encuentra | nombre mal escrito (mayúsculas), o una clase que miniJVM no tiene | revisa la ortografía; mira «Qué hay y qué no» |
| La orden `java` no encuentra la clase | estás en otra carpeta, o falta el `.class` | entra en la carpeta donde está, o usa `java -cp .` |
| El programa se queda sin memoria | el montón por defecto (128 MB) es pequeño para lo que haces | prueba `java -Xmx256M Clase` si tu RAM lo permite |
| `javac` tarda mucho | Janino compila dentro de la máquina virtual | es normal; para programas grandes, compila en el PC |

## Qué hay y qué no

- **Hay:** lenguaje Java 8 (clases internas, `java.lang`, `java.util` (colecciones), `java.io`,
  hilos (cada hilo de Java es un hilo de SaguiOS), sockets TCP/UDP (`java.net`, sin TLS) y reflexión básica.
- **No hay:** Swing, AWT, `Runtime.exec` (no se pueden lanzar órdenes del sistema), TLS, JNI con bibliotecas
  dinámicas, depurador ni JIT (la máquina virtual interpreta el código).
- Con el `javac` de SaguiOS (Janino) fallan los genéricos (`Map<String, Integer>`) y algunas lambdas: para esos programas, compila en el PC.

## Qué sigue

- El lenguaje Python, más corto para empezar: [Python](python.md).
- C, para programas rápidos y ventanas con `tcc`: [C](c.md).
- Un programa de la terminal: [sagsh](sagsh.md).
- El índice con todos los lenguajes: [Programar en SaguiOS](indice.md).
