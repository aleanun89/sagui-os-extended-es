<!-- categoria: Programación/sagsh; orden: 60; titulo: sagsh -->
# sagsh: a linguagem do shell

O **sagsh** é o interpretador de comandos do SaguiOS: o que você digita no Terminal. Mas ele também é uma **linguagem de
programação**: com variáveis, condições, laços e funções você pode escrever um **roteiro** (um arquivo com comandos) que
faz um trabalho inteiro: renomear fotos, buscar em muitos arquivos, avisar quando algo muda...

É a mesma linguagem do `sh` e quase a mesma do `bash` do Linux. O que você aprender aqui serve em outros sistemas também.
A referência curta está no próprio sistema: digite `ayuda sh` no Terminal.

## Primeiro roteiro, passo a passo

**1.** No Terminal, crie uma pasta e entre nela:

    mkdir guiones
    cd guiones

**2.** Abra o editor com o arquivo novo:

    mico saludo.sh

**3.** Escreva:

```sh
#!/bin/sh
# saludo.sh: el primer guion
nombre=${1:-mundo}
echo "¡Hola, $nombre!"
echo "Me has dado $# argumentos."
```

A primeira linha (`#!/bin/sh`) diz qual interpretador o roteiro usa. As linhas que começam por `#` são comentários.

**4.** Salve com **Ctrl+S** e execute de duas formas:

    sh saludo.sh Tito
    chmod +x saludo.sh      # lo hace ejecutable (una vez)
    ./saludo.sh Ana

Com `./saludo.sh` o sistema lê a primeira linha e usa o `sagsh`. Dentro do roteiro, `$1` é o primeiro argumento (`Tito`),
`$#` quantos são e `$0` o nome do próprio roteiro.

## Variáveis e aspas

```sh
nombre="Sagui"           # sin espacios alrededor del =
echo "Hola $nombre"      # las comillas dobles dejan leer la variable
echo 'Hola $nombre'      # las simples lo dejan tal cual: Hola $nombre
echo "letras: ${#nombre}"           # 5
echo "sin definir: ${sin_valor:-valor por defecto}"
echo "sin extensión: ${archivo%.txt}"
```

Variáveis especiais: `$?` (código de saída do último comando), `$$` (número do processo), `$1`...`$9` e `$@` (todos os
argumentos). Para calcular, use `$(( ))`:

```sh
x=7
echo $((x * 6))          # 42
echo $(( (x + 3) / 2 ))  # 5
```

Para guardar a saída de um comando numa variável, `$( )`:

```sh
fecha=$(data)
echo "Ahora son las $fecha"
```

## Condições

```sh
if [ -f notas.txt ]; then
    echo "hay un archivo de notas"
elif [ -d notas ]; then
    echo "hay una carpeta notas"
else
    echo "no hay nada de eso"
fi
```

O `[ ]` (ou `test`) verifica coisas: `-f` arquivo, `-d` pasta, `-e` existe, `-s` não está vazio, `-z` cadeia vazia,
`-n` cadeia não vazia; e para números: `-eq`, `-ne`, `-lt`, `-le`, `-gt`, `-ge`. E para cadeias, `=` e `!=`.

```sh
edad=12
if [ "$edad" -ge 18 ]; then
    echo "adulto"
else
    echo "menor"
fi
```

Qualquer comando também serve como condição: `if grep -q hola notas.txt; then ...`. O `case` é mais cômodo para várias
opções:

```sh
case "$1" in
    *.txt)  echo "un texto" ;;
    *.c)    echo "código en C" ;;
    *)      echo "otra cosa" ;;
esac
```

## Laços

```sh
for i in 1 2 3; do
    echo "vuelta $i"
done

for f in /ejemplos/*; do          # los comodines * y ? se expanden a los archivos
    echo "$f"
done

n=3
while [ "$n" -gt 0 ]; do
    echo "cuenta: $n"
    n=$((n - 1))
done
```

O `break` sai do laço e o `continue` pula para a próxima volta. O `seq 1 5` escreve os números de 1 a 5, útil com
`for i in $(seq 1 5)`.

## Funções

```sh
doble() {
    local x=$1           # local: la variable solo vale dentro de la función
    echo $((x * 2))
}

echo "el doble de 21 es $(doble 21)"
```

A função é chamada como um comando: com os seus argumentos e sem parênteses. O que ela escreve com `echo` é o que quem
a chamou recebe com `$( )`. `return N` dá um código de saída.

## Tubulações e redirecionamentos

Uma **tubulação** (`|`) passa a saída de um comando para a entrada do seguinte:

    ls /ejemplos | sort
    cat notas.txt | grep plátano | wc -l
    seq 1 10 | head -3

Os **redirecionamentos** mandam a saída para um arquivo ou a tomam de um:

    echo "primera" > lista.txt       # escribe (borra lo anterior)
    echo "segunda" >> lista.txt      # añade al final
    sort < lista.txt                 # toma la entrada del archivo
    orden 2> errores.txt             # los errores a un archivo
    orden > salida.txt 2>&1          # salida y errores juntos

Um **documento aqui** (`<<FIN`) escreve várias linhas de uma vez:

```sh
cat > tarea.txt << FIN
comprar pan
llamar a Ana
FIN
```

Um comando com `&` no fim roda em segundo plano (`cuenta.sh &`); o `jobs` lista os que estão rodando e o `wait` espera
que terminem.

## Roteiros úteis

Um roteiro que conta as palavras diferentes de um arquivo, ordenadas por frequência:

```sh
#!/bin/sh
# palabras.sh: cuántas veces sale cada palabra en un archivo
if [ -z "$1" ]; then
    echo "uso: palabras.sh archivo"
    exit 1
fi
awk '{ for (i = 1; i <= NF; i++) print $i }' "$1" | sort | uniq -c | sort -rn | head -10
```

E o do jogo de adivinhar de `/ejemplos/adivina.sh`, que lê números com `read` até acertar. Os exemplos do sistema estão em
`/ejemplos/`: `hola.sh`, `funciones.sh` e `adivina.sh`.

## Sua configuração: ~/.sagshrc

Cada shell que você abre lê primeiro o `/etc/sagshrc` (de todo o sistema) e depois o **`~/.sagshrc`** (o seu): aliases,
variáveis, o prompt (`PS1`) e funções. Um exemplo:

```sh
alias ll='ls -l'
export EDITOR=mico
mkcd() { mkdir -p "$1" && cd "$1"; }
```

Para testar uma mudança sem fechar o Terminal: `. ~/.sagshrc`. Detalhes em [O Terminal](../terminal.md).

## Erros típicos

| O que aparece | O que acontece | O que fazer |
|---------------|----------------|-------------|
| `error de sintaxis cerca de 'fi'` | falta um `then`, `do` ou um `;` antes de `fi`/`done` | olhe a linha que a mensagem indica e a anterior |
| `orden no encontrada` | você escreveu o nome errado, ou não está instalado | o `ayuda` lista os comandos; `ls /bin` mostra os que existem |
| O roteiro não roda com `./` | falta a permissão de execução | `chmod +x guion.sh` |
| As variáveis com espaços se partem | faltam aspas: `$archivo` tem um espaço dentro | escreva sempre `"$archivo"` |
| `if [ $x = 1 ]` dá erro se `x` estiver vazia | sem aspas, o comando perde um argumento | use `"$x"` |
| O roteiro faz coisas estranhas | você não sabe o que está acontecendo | `sh -x guion.sh` mostra cada comando antes de executá-lo |

Útil ao depurar: `set -e` faz o roteiro parar no primeiro erro; `set -u` avisa sobre variáveis não definidas; `set -x`
mostra cada comando.

## O que vem depois

- Roteiros maiores, com janelas ou cálculos: [Python](python_PT.md) ou [C](c_PT.md).
- O Terminal: [O Terminal](../terminal.md) e os comandos de `ayuda ls`, `ayuda grep`...
- Outras linguagens que também rodam no terminal: [awk](../awk.md) (processar colunas), [Lua](../lua.md), [make](../make.md).
- O índice com todas as linguagens: [Programar no SaguiOS](indice_PT.md).
