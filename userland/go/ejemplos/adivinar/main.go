// adivinar: el juego de adivinar el número (del 1 al 100).
//
//	adivinar          número al azar; se teclea cada intento
//	adivinar 42       fija el número secreto (para probar)
package main

import (
	"bufio"
	"fmt"
	"math/rand"
	"os"
	"strconv"
	"strings"
	"time"
)

func main() {
	secreto := 0
	if len(os.Args) > 1 {
		n, err := strconv.Atoi(os.Args[1])
		if err != nil || n < 1 || n > 100 {
			fmt.Println("el número secreto debe estar entre 1 y 100")
			os.Exit(2)
		}
		secreto = n
	} else {
		// La semilla sale del reloj; math/rand de TinyGo no necesita más.
		azar := rand.New(rand.NewSource(time.Now().UnixNano()))
		secreto = azar.Intn(100) + 1
	}

	fmt.Println("Estoy pensando un número entre 1 y 100. ¿Cuál es?")
	entrada := bufio.NewScanner(os.Stdin)
	for intentos := 1; ; intentos++ {
		fmt.Printf("intento %d> ", intentos)
		if !entrada.Scan() {
			fmt.Println("\nHasta luego.")
			return
		}
		n, err := strconv.Atoi(strings.TrimSpace(entrada.Text()))
		switch {
		case err != nil:
			fmt.Println("escribe un número")
			intentos--
		case n < secreto:
			fmt.Println("más alto")
		case n > secreto:
			fmt.Println("más bajo")
		default:
			fmt.Printf("¡Acertaste en %d intentos!\n", intentos)
			return
		}
	}
}
