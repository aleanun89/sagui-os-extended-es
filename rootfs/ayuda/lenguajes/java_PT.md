<!-- categoria: Programación/Java; orden: 50; titulo: Java -->
# Java no SaguiOS: programas com miniJVM

O **Java** é uma das linguagens mais usadas do mundo: o mesmo programa roda em qualquer sistema que tenha uma
**máquina virtual** (a JVM). No SaguiOS a máquina virtual é o **miniJVM**, pequena e escrita em C, e entende o Java 8.
Há dois comandos: `java` (executa) e `javac` (compila), e os dois funcionam **dentro** do SaguiOS.

O Java é mais longo que o Python para começar, mas ensina muito bem as ideias de classes, listas e janelas. Se quiser
algo mais curto, comece por [Python](python_PT.md); para um programa rápido, por [C](c_PT.md).

## Primeiro programa, passo a passo

**1.** No Terminal, crie uma pasta e entre nela:

    mkdir java
    cd java

**2.** Escreva o arquivo com o editor. O nome do arquivo tem de ser igual ao da classe (`Hola.java` para
`public class Hola`). No Terminal:

    mico Hola.java

**3.** Escreva:

```java
// Hola.java: el primer programa en Java
public class Hola {
    public static void main(String[] args) {
        System.out.println("Hola desde Java en SaguiOS");
        System.out.println("2 + 3 = " + (2 + 3));
    }
}
```

**4.** Salve com **Ctrl+S**. Há duas formas de executar:

    java Hola.java          # compila y ejecuta de una vez (lo más cómodo)

ou em dois passos:

    javac Hola.java         # compila: crea Hola.class
    java Hola               # ejecuta la clase

> O Mico não executa `.java` com F5 por padrão: no Terminal, o comando é o de cima.

**Aviso de tempo:** o `javac` (escrito em Java com a biblioteca Janino) demora bastante para iniciar, uns 13 segundos
numa máquina virtual sem aceleração. Na segunda vez que você executar o mesmo `.class` com `java Hola`, é quase imediato.

## Executar programas que já existem

    java -cp /apps/java/ejemplos Hola mundo    # una clase de una carpeta, con argumentos
    java -jar app.jar                          # un .jar (usa la clase principal del manifiesto)
    java -Xmx64M Clase                         # limitar la memoria (por defecto 128 MB)
    java -version

Os exemplos do sistema estão em `/apps/java/ejemplos/` (em console: `Hola`, `Argumentos`, `Ficheros`, `Colecciones`,
`Hilos`, `Excepciones`, `Adivinar`, `Acentos`, `Bucle`; com janelas: `Calculadora`, `BlocNotas`, `Serpiente`). Os
fontes estão em `userland/java/ejemplos/` se quiser lê-los.

`System.exit(n)` termina com o código `n` (veja com `echo $?`). Se o programa lançar uma exceção que não é capturada,
sai com código 1 e mostra a pilha de chamadas.

## O básico da linguagem

### Variáveis e tipos

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

O Java é **rigoroso** com os tipos: `int`, `double`, `char`, `boolean`, e `String` para o texto (que não é um tipo básico,
mas uma classe).

### Condições e laços

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

### Listas e mapas

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

O exemplo não usa genéricos (`List<Integer>`, `Map<String, Integer>`): o `javac` do SaguiOS (Janino) falha com eles.
Compile no PC (`tools/javac-sagui`) os programas que usam genéricos, que o aceita.

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

## Arquivos

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

O `System.out` converte o texto UTF-8 do Java para Latin-1 na tela; o que não existe em Latin-1 (como o símbolo do euro)
sai como `?`.

## Janelas com `sagui.gui`

O SaguiOS tem a sua própria biblioteca gráfica, `sagui.gui`. Não é o Swing nem o AWT. As classes principais são:

| Classe | O que é |
|--------|---------|
| `Ventana` | a janela; `agregar` põe componentes e `ejecutar()` é o laço de eventos |
| `Etiqueta`, `Boton`, `CampoTexto`, `Casilla`, `Lista` | componentes (o `Boton` executa uma `Accion`) |
| `Lienzo` | desenho: `linea`, `rect`, `elipse`, `texto`, `imagen`; eventos do mouse e do teclado |
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

Compile e execute do mesmo jeito (`java Saludo.java`). As janelas precisam da área de trabalho (não funcionam no console).
Todo o código da interface roda na thread que chama `ejecutar()`. As posições são absolutas: não há disposição
automática. Para desenhar, o `Lienzo` e o `Grafico` são o lugar. Exemplos completos: `Calculadora`, `BlocNotas` e
`Serpiente` em `/apps/java/ejemplos/`.

## Erros típicos

A tabela descreve o que acontece; as mensagens exatas dependem do compilador.

| O que aparece | O que acontece | O que fazer |
|---------------|----------------|-------------|
| O compilador reclama do `public class` e do nome do arquivo | a classe pública tem de ter o mesmo nome do arquivo | renomeie o arquivo ou a classe, e compile de novo |
| Um símbolo não é encontrado | nome escrito errado (maiúsculas), ou uma classe que o miniJVM não tem | confira a ortografia; veja «O que há e o que não há» |
| O comando `java` não encontra a classe | você está em outra pasta, ou falta o `.class` | entre na pasta onde ele está, ou use `java -cp .` |
| O programa fica sem memória | o heap padrão (128 MB) é pequeno para o que você faz | tente `java -Xmx256M Clase` se a sua RAM permitir |
| O `javac` demora muito | o Janino compila dentro da máquina virtual | é normal; para programas grandes, compile no PC |

## O que há e o que não há

- **Há:** a maior parte da linguagem Java 8 (classes internas, `java.lang`, `java.util` (coleções), `java.io`,
  threads (cada thread Java é uma thread do SaguiOS), sockets TCP/UDP (`java.net`, sem TLS) e reflexão básica.
- **Não há:** Swing, AWT, `Runtime.exec` (não é possível executar comandos do sistema), TLS, JNI com bibliotecas
  dinâmicas, depurador nem JIT (a máquina virtual interpreta o código).
- Com o `javac` do SaguiOS (Janino) falham os genéricos (`Map<String, Integer>`) e algumas lambdas: compile esses programas no PC.

## O que vem depois

- A linguagem Python, mais curta para começar: [Python](python_PT.md).
- C, para programas rápidos e janelas com `tcc`: [C](c_PT.md).
- Um programa de terminal: [sagsh](sagsh_PT.md).
- O índice com todas as linguagens: [Programar no SaguiOS](indice_PT.md).
