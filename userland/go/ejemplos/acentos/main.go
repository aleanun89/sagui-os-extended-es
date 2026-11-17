// acentos: texto con acentos y eñes (UTF-8 en Go, Latin-1 en el terminal).
//
// La capa de SaguiOS convierte en las terminales (salida: UTF-8 a Latin-1; entrada: Latin-1 a UTF-8);
// los ficheros y las tuberías quedan en UTF-8 byte a byte.
package main

import (
	"bufio"
	"fmt"
	"os"
)

func main() {
	fmt.Println("año y canción: ñandú, ¿qué tal? (€ no cabe en Latin-1)")

	// Un fichero queda en UTF-8: "año canción" mide 13 bytes (ñ y ó ocupan 2).
	os.WriteFile("/tmp/acentos.txt", []byte("año canción"), 0o644)
	b, _ := os.ReadFile("/tmp/acentos.txt")
	fmt.Println("bytes en el fichero:", len(b), "runas:", len([]rune(string(b))))

	// Entrada del teclado: una línea con ñ llega en UTF-8 (ñ = 2 bytes, 1 runa).
	fmt.Print("escribe algo: ")
	sc := bufio.NewScanner(os.Stdin)
	if sc.Scan() {
		t := sc.Text()
		fmt.Printf("leí %q: %d bytes, %d runas\n", t, len(t), len([]rune(t)))
	}
}
