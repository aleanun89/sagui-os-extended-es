<!-- categoria: Programación/Java; orden: 50; titulo: Java -->
# Java on SaguiOS: programs with miniJVM

**Java** is one of the most widely used languages in the world: the same program runs on any system that has a
**virtual machine** (the JVM). On SaguiOS the virtual machine is **miniJVM**, small and written in C, and it
understands Java 8. There are two commands: `java` (runs) and `javac` (compiles), and both work **inside** SaguiOS.

Java is longer than Python to start with, but it teaches the ideas of classes, lists and windows very well. If you
want something shorter, start with [Python](python_EN.md); for a quick program, with [C](c_EN.md).

## First program, step by step

**1.** In the Terminal, create a folder and go into it:

    mkdir java
    cd java

**2.** Write the file with the editor. The file name must be the same as the class name (`Hola.java` for
`public class Hola`). In the Terminal:

    mico Hola.java

**3.** Type:

```java
// Hola.java: el primer programa en Java
public class Hola {
    public static void main(String[] args) {
        System.out.println("Hola desde Java en SaguiOS");
        System.out.println("2 + 3 = " + (2 + 3));
    }
}
```

**4.** Save with **Ctrl+S**. There are two ways to run it:

    java Hola.java          # compila y ejecuta de una vez (lo más cómodo)

or in two steps:

    javac Hola.java         # compila: crea Hola.class
    java Hola               # ejecuta la clase

> Mico does not run `.java` with F5 out of the box: in the Terminal, use the command above.

**Time warning:** `javac` (which is written in Java with the Janino library) takes quite a while to start, about
13 seconds on a virtual machine without acceleration. The second time you run the same `.class` with `java Hola`
it is almost immediate.

## Running programs that already exist

    java -cp /apps/java/ejemplos Hola mundo    # una clase de una carpeta, con argumentos
    java -jar app.jar                          # un .jar (usa la clase principal del manifiesto)
    java -Xmx64M Clase                         # limitar la memoria (por defecto 128 MB)
    java -version

The system examples are in `/apps/java/ejemplos/` (console: `Hola`, `Argumentos`, `Ficheros`, `Colecciones`,
`Hilos`, `Excepciones`, `Adivinar`, `Acentos`, `Bucle`; with windows: `Calculadora`, `BlocNotas`, `Serpiente`).
The sources are in `userland/java/ejemplos/` if you want to read them.

`System.exit(n)` ends with the code `n` (check it with `echo $?`). If the program throws an exception that is not
caught, it exits with code 1 and shows the call stack.

## The basics of the language

### Variables and types

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

Java is **strict** about types: `int`, `double`, `char`, `boolean`, and `String` for text (which is not a basic type
but a class).

### Conditions and loops

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

### Methods

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

### Lists and maps

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

The example does not use generics (`List<Integer>`, `Map<String, Integer>`): SaguiOS's `javac` (Janino) fails on them.
Compile programs that use generics on the PC (`tools/javac-sagui`), which accepts them.

### Classes

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

## Files

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

`System.out` converts Java's UTF-8 text to Latin-1 for the screen; what does not exist in Latin-1 (such as the euro
symbol) comes out as `?`.

## Windows with `sagui.gui`

SaguiOS has its own graphics library, `sagui.gui`. It is not Swing or AWT. Its main classes are:

| Class | What it is |
|-------|------------|
| `Ventana` | the window; `agregar` adds components and `ejecutar()` is the event loop |
| `Etiqueta`, `Boton`, `CampoTexto`, `Casilla`, `Lista` | components (the `Boton` runs an `Accion`) |
| `Lienzo` | drawing: `linea`, `rect`, `elipse`, `texto`, `imagen`; mouse and keyboard events |
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

Compile and run it the same way (`java Saludo.java`). Windows need the desktop (they do not work in the console).
All the interface code runs in the thread that calls `ejecutar()`. Positions are absolute: there is no automatic
layout. To draw, `Lienzo` and `Grafico` are the place. Complete examples: `Calculadora`, `BlocNotas` and `Serpiente`
in `/apps/java/ejemplos/`.

## Typical errors

The table describes what happens; the exact messages depend on the compiler.

| What you see | What happens | What to do |
|--------------|--------------|------------|
| The compiler complains about `public class` and the file name | the public class must have the same name as the file | rename the file or the class, and compile again |
| A symbol is not found | a name written wrong (capitals), or a class that miniJVM does not have | check the spelling; see "What is there and what is not" |
| The `java` command does not find the class | you are in another folder, or the `.class` is missing | go into the folder where it is, or use `java -cp .` |
| The program runs out of memory | the default heap (128 MB) is small for what you do | try `java -Xmx256M Clase` if your RAM allows it |
| `javac` takes a long time | Janino compiles inside the virtual machine | this is normal; for large programs, compile on the PC |

## What is there and what is not

- **There is:** most of the Java 8 language (inner classes, `java.lang`, `java.util` (collections),
  `java.io`, threads (each Java thread is a SaguiOS thread), TCP/UDP sockets (`java.net`, without TLS) and basic
  reflection.
- **There is not:** Swing, AWT, `Runtime.exec` (you cannot run system commands), TLS, JNI with dynamic libraries,
  debugger or JIT (the virtual machine interprets the code).
- With SaguiOS's `javac` (Janino), generics (`Map<String, Integer>`) and some lambdas fail: compile those programs on the PC.

## What comes next

- The Python language, shorter to start: [Python](python_EN.md).
- C, for fast programs and windows with `tcc`: [C](c_EN.md).
- A terminal program: [sagsh](sagsh_EN.md).
- The index with all the languages: [Programming on SaguiOS](indice_EN.md).
