// ficheros: crear, escribir, leer, listar y borrar con el paquete os.
//
// Trabaja en /tmp/gofic (se crea y se borra al terminar).
package main

import (
	"bufio"
	"fmt"
	"os"
	"sort"
)

func falla(contexto string, err error) {
	if err != nil {
		fmt.Fprintln(os.Stderr, "error en", contexto+":", err)
		os.Exit(1)
	}
}

func main() {
	dir := "/tmp/gofic"
	falla("mkdir", os.MkdirAll(dir, 0o755))

	// WriteFile y ReadFile: todo el contenido de golpe.
	ruta := dir + "/nota.txt"
	falla("WriteFile", os.WriteFile(ruta, []byte("primera linea\nsegunda linea\n"), 0o644))
	datos, err := os.ReadFile(ruta)
	falla("ReadFile", err)
	fmt.Printf("leidos %d bytes: %q\n", len(datos), datos)

	// Create / Write / Close y abrir con OpenFile para añadir al final.
	f, err := os.Create(dir + "/numeros.txt")
	falla("Create", err)
	for i := 1; i <= 5; i++ {
		fmt.Fprintf(f, "%d\n", i*i)
	}
	falla("Close", f.Close())
	f, err = os.OpenFile(dir+"/numeros.txt", os.O_APPEND|os.O_WRONLY, 0o644)
	falla("OpenFile", err)
	f.WriteString("36\n")
	f.Close()

	// Leer línea a línea con bufio.
	f, err = os.Open(dir + "/numeros.txt")
	falla("Open", err)
	sc := bufio.NewScanner(f)
	suma := 0
	for sc.Scan() {
		var n int
		fmt.Sscanf(sc.Text(), "%d", &n)
		suma += n
	}
	f.Close()
	fmt.Println("suma de los cuadrados:", suma)

	// Stat: tamaño y tipo.
	st, err := os.Stat(ruta)
	falla("Stat", err)
	fmt.Printf("%s: %d bytes, carpeta=%v\n", st.Name(), st.Size(), st.IsDir())

	// Un fichero que no existe: errors.Is con os.ErrNotExist funciona.
	if _, err := os.Stat(dir + "/no-existe"); os.IsNotExist(err) {
		fmt.Println("no-existe: no existe (como se esperaba)")
	}

	// Listar una carpeta.
	entradas, err := os.ReadDir(dir)
	falla("ReadDir", err)
	var nombres []string
	for _, e := range entradas {
		nombres = append(nombres, e.Name())
	}
	sort.Strings(nombres)
	fmt.Println("en", dir+":", nombres)

	// Renombrar y borrar.
	falla("Rename", os.Rename(ruta, dir+"/nota2.txt"))
	falla("RemoveAll", os.RemoveAll(dir))
	if _, err := os.Stat(dir); os.IsNotExist(err) {
		fmt.Println("carpeta borrada")
	}
	fmt.Println("ficheros: terminado")
}
