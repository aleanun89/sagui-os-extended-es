// hola: el programa más pequeño en Go para SaguiOS (D364).
//
// Se compila con TinyGo: tools/go-sagui build -o hola ./userland/go/ejemplos/hola
// y con «make» (que lo deja en /apps/go/hola cuando TinyGo está instalado).
package main

import (
	"fmt"
	"runtime"
)

func main() {
	fmt.Println("hola desde Go en SaguiOS")
	fmt.Printf("compilador: %s, arquitectura: %s, goroutines: %d\n",
		runtime.Compiler, runtime.GOARCH, runtime.NumGoroutine())
	fmt.Println("2 + 3 =", 2+3, "y la raíz de 2 es", sqrt2())
}

// sqrt2 calcula la raíz de 2 por el método de Newton: usa la FPU x87
// (el código generado para Pentium II no usa SSE).
func sqrt2() float64 {
	x := 1.0
	for i := 0; i < 20; i++ {
		x = (x + 2/x) / 2
	}
	return x
}
