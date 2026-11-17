// contar: cuenta líneas, palabras y bytes, como wc.
//
//	contar fichero...        cuenta cada fichero y el total
//	contar                   cuenta la entrada estándar (acabar con Ctrl+D)
package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"unicode"
)

type cuenta struct{ lineas, palabras, bytes int }

func contar(r io.Reader) (c cuenta, err error) {
	lector := bufio.NewReader(r)
	enPalabra := false
	for {
		b, err := lector.ReadByte()
		if err == io.EOF {
			return c, nil
		}
		if err != nil {
			return c, err
		}
		c.bytes++
		if b == '\n' {
			c.lineas++
		}
		if unicode.IsSpace(rune(b)) {
			enPalabra = false
		} else if !enPalabra {
			enPalabra = true
			c.palabras++
		}
	}
}

func main() {
	var total cuenta
	codigo := 0
	if len(os.Args) == 1 {
		c, err := contar(os.Stdin)
		if err != nil {
			fmt.Fprintln(os.Stderr, "contar:", err)
			os.Exit(1)
		}
		fmt.Printf("%7d %7d %7d\n", c.lineas, c.palabras, c.bytes)
		return
	}
	for _, nombre := range os.Args[1:] {
		f, err := os.Open(nombre)
		if err != nil {
			fmt.Fprintln(os.Stderr, "contar:", err)
			codigo = 1
			continue
		}
		c, err := contar(f)
		f.Close()
		if err != nil {
			fmt.Fprintln(os.Stderr, "contar:", nombre+":", err)
			codigo = 1
			continue
		}
		fmt.Printf("%7d %7d %7d %s\n", c.lineas, c.palabras, c.bytes, nombre)
		total.lineas += c.lineas
		total.palabras += c.palabras
		total.bytes += c.bytes
	}
	if len(os.Args) > 2 {
		fmt.Printf("%7d %7d %7d total\n", total.lineas, total.palabras, total.bytes)
	}
	os.Exit(codigo)
}
