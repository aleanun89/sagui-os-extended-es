<!-- categoria: Programación/Go; orden: 30; titulo: Go -->
# Go on SaguiOS: programs with TinyGo

**Go** is a modern language from Google, designed for simple and fast programs that do several things at once
(the **goroutines**). On SaguiOS it is used through **TinyGo**, a version of Go for small machines that produces
32-bit programs, like the other programs in `/apps`.

Before you start, one important thing:

> **SaguiOS has no Go compiler.** Go programs are compiled on the **PC** with TinyGo, and the result (a program
> already built) runs on SaguiOS. The system examples are already compiled in `/apps/go/`.

So this guide has two parts: how a Go program is **used** on SaguiOS (what you can do today without installing
anything) and how it is **compiled** on the PC (for those who have the repository). To program without leaving
SaguiOS, use [C](c_EN.md), [Python](python_EN.md) or [Java](java_EN.md).

## Running a Go program on SaguiOS

The examples are in `/apps/go/`. In the Terminal:

    ls /apps/go
    /apps/go/hola
    /apps/go/argumentos uno dos tres
    /apps/go/goroutines

Go programs behave like any other command: they accept arguments, write to the screen, read from the keyboard and
return an exit code (`echo $?` shows the last one).

List of examples and what they teach (the sources are in `userland/go/ejemplos/`):

| Example | What it teaches |
|---------|-----------------|
| `hola` | the smallest program, `fmt.Println`, calculations with decimals |
| `argumentos` | `os.Args` and the exit code |
| `contar` | reading the keyboard line by line with `bufio` |
| `adivinar` | a game with standard input |
| `ficheros` | creating, writing, reading, listing and deleting files with `os` |
| `goroutines` | several tasks at once with channels and `sync.WaitGroup` |
| `json` | encoding and reading JSON with `encoding/json` |
| `tiempo` | `time.Now`, waits and timers |
| `acentos` | text with accents in the terminal |

## Your first program (on the PC)

To write your own program you need the SaguiOS repository and TinyGo installed on the PC:

    tools/tinygo_entorno.sh instalar      # TinyGo y Go, sin sudo (descarga unos 190 MB)
    . ~/.local/saguitools/env.sh
    tools/go-sagui build -o hola ./userland/go/ejemplos/hola

`go-sagui` is TinyGo with the SaguiOS target. The result, `hola`, is a 32-bit program that runs on SaguiOS. If the
program must be in `/apps/go/`, `make` puts it there when TinyGo is installed.

A minimal program, in the folder `saludo` with its `main.go` and a `go.mod` (create it with `go mod init saludo`,
using the `go` that this script installs):

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

Compile it as the example is compiled (with the path of your folder), move the resulting file to SaguiOS by any
means you have (for example, `scp` over SSH: see [SSH](../../../docs/ssh.md)) and run it from its folder:

    ./saludo Tito

The `/apps/go/` examples arrive on the system with `make`; your own program does not have to be there. The details of
the installation are in [docs/go.md](../../../docs/go.md).

## The basics of the language

### Variables, types and functions

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

Go has no `;` at the end of a line (it adds them itself). A `:=` declares a new variable with the type of its value.

### Conditions and loops

Go only has `for`, but it does for everything:

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

### Lists and maps

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

### Goroutines and channels

A **goroutine** is a task that runs at the same time as the others; a **channel** is a pipe through which values
pass between them:

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

In TinyGo the goroutines run on **a single thread** of the system: one that does not wait (no channels, no
`time.Sleep`) takes over the processor. That is why `goroutines` uses channels and waits.

## Files and text

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

Files are saved as they are (UTF-8). In the terminal, Go shows accents correctly: the program converts UTF-8 to
Latin-1 only when it writes to the screen.

## Network

The network (`net`) is **not yet** available in TinyGo for SaguiOS: it is the next phase. To make web requests or
a simple server today, use [Python](python_EN.md) (`socket`, `requests`) or [C](c_EN.md).

## Windows

Go has no windows on SaguiOS today. For windows: [C with tcc](c_EN.md),
[Python with the sagui module](python_EN.md), or [Java with sagui.gui](java_EN.md).

## Typical errors

| What you see | What happens | What to do |
|--------------|--------------|------------|
| `declared and not used: x` | you declared a variable and do not use it | use it or delete it (`_` if you do not care about the value) |
| `imported and not used` | you imported a package and do not use it | remove it from the `import` |
| A program seems stuck | a goroutine does not give up the turn | add a wait (`time.Sleep`) or a channel |
| The program does not start on SaguiOS | it was compiled for another system or CPU | compile with `tools/go-sagui` from the repository, not with the `go` of the PC |
| `os/exec` and `os/signal` do not exist | SaguiOS has no `fork` and no signals | to run commands, write the script in [sagsh](sagsh_EN.md) |

## Known limits

- No `net` yet, no `plugin`, no cgo, partial `reflect`.
- No `fork`, `os/exec` or signals (`os/signal`).
- The clock has a resolution of 1 ms.
- The standard library has large packages that are incomplete: what is missing gives a compile error.

## What comes next

- What you already know from C and Python: [C](c_EN.md), [Python](python_EN.md), [Java](java_EN.md).
- Scripts in the terminal: [sagsh](sagsh_EN.md).
- The index with all the languages: [Programming on SaguiOS](indice_EN.md).
