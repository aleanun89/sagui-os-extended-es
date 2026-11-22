<!-- categoria: Programación/Go; orden: 30; titulo: Go -->
# Go en SaguiOS: programas con TinyGo

**Go** es un lenguaje moderno de Google, pensado para programas sencillos y rápidos que hacen varias cosas a la
vez (las **goroutines**). En SaguiOS se usa **TinyGo**, una versión de Go para equipos pequeños que genera
programas de 32 bits, como los demás de `/apps`.

Antes de empezar, una cosa importante:

> **En SaguiOS no hay compilador de Go.** Los programas Go se compilan en el **PC** con TinyGo y el resultado
> (un programa ya hecho) corre en SaguiOS. Los ejemplos del sistema están ya compilados en `/apps/go/`.

Así que esta guía tiene dos partes: cómo se **usa** un programa Go en SaguiOS (lo que puedes hacer hoy sin
instalar nada) y cómo se **compila** en el PC (para quien tiene el repositorio). Para programar sin salir de
SaguiOS, usa [C](c.md), [Python](python.md) o [Java](java.md).

## Ejecutar un programa Go en SaguiOS

Los ejemplos están en `/apps/go/`. En el Terminal:

    ls /apps/go
    /apps/go/hola
    /apps/go/argumentos uno dos tres
    /apps/go/goroutines

Los programas Go se comportan como cualquier orden: aceptan argumentos, escriben en la pantalla, leen del
teclado y devuelven un código de salida (`echo $?` te enseña el último).

Lista de ejemplos y lo que enseñan (los fuentes están en `userland/go/ejemplos/`):

| Ejemplo | Qué enseña |
|---------|------------|
| `hola` | el programa más pequeño, `fmt.Println`, cálculos con decimales |
| `argumentos` | `os.Args` y el código de salida |
| `contar` | leer del teclado línea a línea con `bufio` |
| `adivinar` | un juego con entrada estándar |
| `ficheros` | crear, escribir, leer, listar y borrar archivos con `os` |
| `goroutines` | varias tareas a la vez con canales y `sync.WaitGroup` |
| `json` | codificar y leer JSON con `encoding/json` |
| `tiempo` | `time.Now`, esperas y temporizadores |
| `acentos` | texto con acentos en el terminal |

## Tu primer programa (en el PC)

Para escribir un programa propio hace falta el repositorio de SaguiOS y TinyGo instalado en el PC:

    tools/tinygo_entorno.sh instalar      # TinyGo y Go, sin sudo (descarga unos 190 MB)
    . ~/.local/saguitools/env.sh
    tools/go-sagui build -o hola ./userland/go/ejemplos/hola

`go-sagui` es TinyGo con el objetivo de SaguiOS. El resultado, `hola`, es un programa de 32 bits que corre en
SaguiOS. Si el programa tiene que estar en `/apps/go/`, lo deja `make` cuando TinyGo está instalado.

Un programa mínimo, en la carpeta `saludo` con su `main.go` y un `go.mod` (créalo con `go mod init saludo`, usando el `go` que instala ese script):

```go
// saludo: el primer programa en Go para SaguiOS.
package main

import (
	"fmt"
	"os"
)

func main() {
	nombre := "mundo"
	if len(os.Args) > 1 {
		nombre = os.Args[1]
	}
	fmt.Println("Hola,", nombre, "desde Go en SaguiOS")
	fmt.Printf("2 + 3 = %d\n", 2+3)
}
```

Compílalo igual que el ejemplo (con la ruta de tu carpeta), pasa el archivo resultado a SaguiOS por el medio que
tengas (por ejemplo, con `scp` por SSH: mira [SSH](../../../docs/ssh.md)) y ejecútalo desde su carpeta:

    ./saludo Tito

Los ejemplos de `/apps/go/` llegan al sistema con `make`; un programa propio no tiene que estar allí. Los detalles
de la instalación están en [docs/go.md](../../../docs/go.md).

## Lo básico del lenguaje

### Variables, tipos y funciones

```go
package main

import "fmt"

func cuadrado(x int) int {
	return x * x
}

func main() {
	edad := 12            // := declara y da valor
	var altura float64 = 1.45
	fmt.Println("edad:", edad, "altura:", altura)
	fmt.Println("el cuadrado de 7 es", cuadrado(7))
}
```

Go no tiene `;` al final de línea (las pone solo). Un `:=` declara una variable nueva con el tipo que tiene
el valor.

### Condiciones y bucles

Go solo tiene `for`, pero sirve para todo:

```go
for i := 1; i <= 5; i++ {
	fmt.Println(i, "x 2 =", i*2)
}

n := 3
for n > 0 {                 // igual que un while
	fmt.Println("cuenta atrás:", n)
	n--
}

if n == 0 {
	fmt.Println("¡despegue!")
} else {
	fmt.Println("todavía no")
}
```

### Listas y mapas

```go
notas := []int{7, 9, 5, 10}      // una lista (slice)
notas = append(notas, 8)          // añadir al final
fmt.Println("hay", len(notas), "notas")

cuenta := map[string]int{}        // un diccionario
for _, p := range []string{"el", "sagui", "el"} {
	cuenta[p]++
}
fmt.Println("el aparece", cuenta["el"], "veces")
```

### Goroutines y canales

Una **goroutine** es una tarea que corre a la vez que las demás; un **canal** es un tubo por el que se pasan
valores entre ellas:

```go
package main

import "fmt"

func cuadrados(n int, salida chan<- int) {
	for i := 1; i <= n; i++ {
		salida <- i * i       // envía un valor
	}
	close(salida)
}

func main() {
	c := make(chan int)
	go cuadrados(5, c)        // lanza la goroutine
	for v := range c {        // recibe hasta que se cierra el canal
		fmt.Println(v)
	}
}
```

En TinyGo las goroutines corren en **un solo hilo** del sistema: una que no espera (ni usa canales, ni
`time.Sleep`) acapara el procesador. Por eso `goroutines` usa canales y esperas.

## Archivos y texto

```go
package main

import (
	"fmt"
	"os"
)

func main() {
	os.MkdirAll("/tmp/notas", 0o755)
	err := os.WriteFile("/tmp/notas/compras.txt", []byte("plátanos\npiñas\n"), 0o644)
	if err != nil {
		fmt.Println("error:", err)
		os.Exit(1)
	}
	datos, err := os.ReadFile("/tmp/notas/compras.txt")
	if err != nil {
		fmt.Println("error:", err)
		os.Exit(1)
	}
	fmt.Print(string(datos))
}
```

Los archivos se guardan tal cual (UTF-8). En el terminal, Go escribe los acentos bien: el programa convierte
UTF-8 a Latin-1 solo al escribir en la pantalla.

## Red

La red (`net`) **no está todavía** en TinyGo para SaguiOS: es la siguiente fase. Para hacer peticiones o un
servidor sencillo hoy, usa [Python](python.md) (`socket`, `requests`) o [C](c.md).

## Ventanas

Go no tiene ventanas en SaguiOS hoy. Para ventanas: [C con tcc](c.md),
[Python con el módulo sagui](python.md), o [Java con sagui.gui](java.md).

## Errores típicos

| Lo que ves | Qué pasa | Qué hacer |
|------------|----------|-----------|
| `declared and not used: x` | declaraste una variable y no la usas | úsala o bórrala (`_` si no te importa) |
| `imported and not used` | importaste un paquete sin usarlo | quítalo del `import` |
| Un programa parece colgado | una goroutine no cede el turno | añade una espera (`time.Sleep`) o un canal |
| El programa no arranca en SaguiOS | se compiló para otro sistema o CPU | compila con `tools/go-sagui` del repositorio, no con el `go` del PC |
| No existen `os/exec` ni `os/signal` | SaguiOS no tiene `fork` ni señales | para lanzar órdenes, escribe el guion en [sagsh](sagsh.md) |

## Límites conocidos

- Sin `net` todavía, sin `plugin`, sin cgo, `reflect` parcial.
- Sin `fork`, `os/exec` ni señales (`os/signal`).
- El reloj tiene una resolución de 1 ms.
- La biblioteca estándar tiene paquetes grandes incompletos: lo que falte dará un error de compilación.

## Qué sigue

- Lo que ya sabes de C y Python: [C](c.md), [Python](python.md), [Java](java.md).
- Scripts en la terminal: [sagsh](sagsh.md).
- El índice con todos los lenguajes: [Programar en SaguiOS](indice.md).
