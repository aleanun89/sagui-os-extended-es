// json: codificar y decodificar JSON con encoding/json y structs.
//
// TinyGo tiene reflect parcial, pero lo que usa encoding/json con structs,
// slices, mapas y etiquetas funciona.
package main

import (
	"encoding/json"
	"fmt"
	"os"
)

type Direccion struct {
	Calle  string `json:"calle"`
	Ciudad string `json:"ciudad"`
}

type Persona struct {
	Nombre     string            `json:"nombre"`
	Edad       int               `json:"edad"`
	Activo     bool              `json:"activo"`
	Direccion  Direccion         `json:"direccion"`
	Lenguajes  []string          `json:"lenguajes"`
	Extra      map[string]string `json:"extra,omitempty"`
	Secreto    string            `json:"-"`
	Puntuacion float64           `json:"puntuacion"`
}

func main() {
	p := Persona{
		Nombre:     "Ana",
		Edad:       31,
		Activo:     true,
		Direccion:  Direccion{"Calle Mayor 1", "Madrid"},
		Lenguajes:  []string{"Go", "C", "Pascal"},
		Secreto:    "no se codifica",
		Puntuacion: 9.5,
	}
	b, err := json.Marshal(p)
	if err != nil {
		fmt.Println("error:", err)
		os.Exit(1)
	}
	fmt.Println(string(b))

	// Con sangría.
	bi, _ := json.MarshalIndent(p.Direccion, "", "  ")
	fmt.Println(string(bi))

	// Decodificar a un struct.
	var q Persona
	entrada := `{"nombre":"Luis","edad":42,"activo":false,"direccion":{"calle":"Gran Via 3","ciudad":"Madrid"},"lenguajes":["Rust"],"extra":{"k":"v"},"puntuacion":7.25}`
	if err := json.Unmarshal([]byte(entrada), &q); err != nil {
		fmt.Println("error:", err)
		os.Exit(1)
	}
	fmt.Printf("%s tiene %d años, vive en %s y habla %v (%s)\n",
		q.Nombre, q.Edad, q.Direccion.Ciudad, q.Lenguajes, q.Extra["k"])

	// Decodificar a un valor genérico.
	var v map[string]interface{}
	json.Unmarshal([]byte(`{"a":1,"b":[true,null,"x"]}`), &v)
	fmt.Println("a =", v["a"], " b =", v["b"])

	// Un JSON mal formado da error.
	if err := json.Unmarshal([]byte(`{"a":`), &v); err != nil {
		fmt.Println("error esperado:", err)
	}
	fmt.Println("json: terminado")
}
