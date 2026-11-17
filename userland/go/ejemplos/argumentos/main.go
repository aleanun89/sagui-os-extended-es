// argumentos: lee los argumentos de la línea de órdenes y el entorno.
//
//	argumentos uno dos tres
package main

import (
	"fmt"
	"os"
	"sort"
	"strings"
)

func main() {
	// os.Args[0] es el nombre con el que se lanzó el programa.
	fmt.Println("programa:", os.Args[0])
	fmt.Println("argumentos:", len(os.Args)-1)
	for i, a := range os.Args[1:] {
		fmt.Printf("  arg %d = %q\n", i+1, a)
	}

	// Variables de entorno: Getenv, LookupEnv, Setenv y Environ.
	if v, ok := os.LookupEnv("PATH"); ok {
		fmt.Println("PATH =", v)
	} else {
		fmt.Println("PATH no está definida")
	}
	os.Setenv("SAGUI_GO", "si")
	fmt.Println("SAGUI_GO =", os.Getenv("SAGUI_GO"))

	var nombres []string
	for _, kv := range os.Environ() {
		nombres = append(nombres, strings.SplitN(kv, "=", 2)[0])
	}
	sort.Strings(nombres)
	fmt.Println("variables definidas:", len(nombres))
	fmt.Println("primeras:", nombres[:min(3, len(nombres))])

	// Código de salida: 7 si se pasó el argumento «falla».
	for _, a := range os.Args[1:] {
		if a == "falla" {
			fmt.Fprintln(os.Stderr, "saliendo con código 7")
			os.Exit(7)
		}
	}
}
