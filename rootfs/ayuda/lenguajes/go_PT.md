<!-- categoria: Programación/Go; orden: 30; titulo: Go -->
# Go no SaguiOS: programas com TinyGo

O **Go** é uma linguagem moderna do Google, pensada para programas simples e rápidos que fazem várias coisas ao mesmo
tempo (as **goroutines**). No SaguiOS usa-se o **TinyGo**, uma versão do Go para máquinas pequenas que gera programas
de 32 bits, como os outros programas de `/apps`.

Antes de começar, uma coisa importante:

> **O SaguiOS não tem compilador de Go.** Os programas em Go são compilados no **PC** com o TinyGo, e o resultado (um
> programa já pronto) roda no SaguiOS. Os exemplos do sistema já estão compilados em `/apps/go/`.

Assim, este guia tem duas partes: como um programa Go é **usado** no SaguiOS (o que você pode fazer hoje sem instalar
nada) e como ele é **compilado** no PC (para quem tem o repositório). Para programar sem sair do SaguiOS, use [C](c_PT.md),
[Python](python_PT.md) ou [Java](java_PT.md).

## Executar um programa Go no SaguiOS

Os exemplos estão em `/apps/go/`. No Terminal:

    ls /apps/go
    /apps/go/hola
    /apps/go/argumentos uno dos tres
    /apps/go/goroutines

Os programas Go se comportam como qualquer comando: aceitam argumentos, escrevem na tela, leem do teclado e devolvem
um código de saída (`echo $?` mostra o último).

Lista de exemplos e o que ensinam (os fontes estão em `userland/go/ejemplos/`):

| Exemplo | O que ensina |
|---------|--------------|
| `hola` | o programa mais pequeno, `fmt.Println`, cálculos com decimais |
| `argumentos` | `os.Args` e o código de saída |
| `contar` | ler do teclado linha a linha com `bufio` |
| `adivinar` | um jogo com entrada padrão |
| `ficheros` | criar, escrever, ler, listar e apagar arquivos com `os` |
| `goroutines` | várias tarefas ao mesmo tempo com canais e `sync.WaitGroup` |
| `json` | codificar e ler JSON com `encoding/json` |
| `tiempo` | `time.Now`, esperas e temporizadores |
| `acentos` | texto com acentos no terminal |

## Seu primeiro programa (no PC)

Para escrever um programa próprio é preciso o repositório do SaguiOS e o TinyGo instalado no PC:

    tools/tinygo_entorno.sh instalar      # TinyGo y Go, sin sudo (descarga unos 190 MB)
    . ~/.local/saguitools/env.sh
    tools/go-sagui build -o hola ./userland/go/ejemplos/hola

O `go-sagui` é o TinyGo com o alvo do SaguiOS. O resultado, `hola`, é um programa de 32 bits que roda no SaguiOS. Se o
programa tiver de estar em `/apps/go/`, o `make` o coloca lá quando o TinyGo estiver instalado.

Um programa mínimo, na pasta `saludo` com o seu `main.go` e um `go.mod` (crie-o com `go mod init saludo`, usando o `go`
que esse script instala):

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

Compile como o exemplo é compilado (com o caminho da sua pasta), leve o arquivo resultante ao SaguiOS pelo meio que você
tiver (por exemplo, `scp` por SSH: veja [SSH](../../../docs/ssh.md)) e execute-o a partir da sua pasta:

    ./saludo Tito

Os exemplos de `/apps/go/` chegam ao sistema com o `make`; um programa seu não precisa estar ali. Os detalhes da
instalação estão em [docs/go.md](../../../docs/go.md).

## O básico da linguagem

### Variáveis, tipos e funções

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

O Go não tem `;` no fim da linha (ele os coloca sozinho). Um `:=` declara uma variável nova com o tipo do valor.

### Condições e laços

O Go só tem `for`, mas ele serve para tudo:

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

### Listas e mapas

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

### Goroutines e canais

Uma **goroutine** é uma tarefa que roda ao mesmo tempo que as outras; um **canal** é um tubo pelo qual os valores passam
de uma para a outra:

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

No TinyGo, as goroutines rodam em **uma única thread** do sistema: uma que não espera (sem canais, sem `time.Sleep`)
toma conta do processador. Por isso o `goroutines` usa canais e esperas.

## Arquivos e texto

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

Os arquivos são salvos como estão (UTF-8). No terminal, o Go mostra os acentos corretamente: o programa converte UTF-8
para Latin-1 apenas ao escrever na tela.

## Rede

A rede (`net`) **ainda não** está disponível no TinyGo para o SaguiOS: é a fase seguinte. Para fazer pedidos web ou um
servidor simples hoje, use [Python](python_PT.md) (`socket`, `requests`) ou [C](c_PT.md).

## Janelas

O Go não tem janelas no SaguiOS hoje. Para janelas: [C com tcc](c_PT.md), [Python com o módulo sagui](python_PT.md) ou
[Java com sagui.gui](java_PT.md).

## Erros típicos

| O que aparece | O que acontece | O que fazer |
|---------------|----------------|-------------|
| `declared and not used: x` | você declarou uma variável e não a usa | use-a ou apague-a (`_` se não lhe interessa o valor) |
| `imported and not used` | você importou um pacote e não o usa | tire-o do `import` |
| Um programa parece travado | uma goroutine não cede a vez | adicione uma espera (`time.Sleep`) ou um canal |
| O programa não inicia no SaguiOS | foi compilado para outro sistema ou CPU | compile com o `tools/go-sagui` do repositório, não com o `go` do PC |
| `os/exec` e `os/signal` não existem | o SaguiOS não tem `fork` nem sinais | para executar comandos, escreva o roteiro em [sagsh](sagsh_PT.md) |

## Limites conhecidos

- Ainda sem `net`, sem `plugin`, sem cgo, `reflect` parcial.
- Sem `fork`, `os/exec` nem sinais (`os/signal`).
- O relógio tem resolução de 1 ms.
- A biblioteca padrão tem pacotes grandes incompletos: o que faltar dará erro de compilação.

## O que vem depois

- O que você já sabe de C e Python: [C](c_PT.md), [Python](python_PT.md), [Java](java_PT.md).
- Roteiros no terminal: [sagsh](sagsh_PT.md).
- O índice com todas as linguagens: [Programar no SaguiOS](indice_PT.md).
