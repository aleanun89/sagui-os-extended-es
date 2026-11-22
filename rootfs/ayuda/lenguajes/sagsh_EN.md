<!-- categoria: Programación/sagsh; orden: 60; titulo: sagsh -->
# sagsh: the shell language

**sagsh** is SaguiOS's command interpreter: what you type in the Terminal. But it is also a **programming language**:
with variables, conditions, loops and functions you can write a **script** (a file with commands) that does a whole
job: renaming photos, searching many files, warning you when something changes...

It is the same language as `sh` and almost the same as Linux's `bash`. What you learn here works on other systems too.
The short reference is in the system itself: type `ayuda sh` in the Terminal.

## First script, step by step

**1.** In the Terminal, create a folder and go into it:

    mkdir guiones
    cd guiones

**2.** Open the editor with the new file:

    mico saludo.sh

**3.** Type:

```sh
#!/bin/sh
# saludo.sh: el primer guion
nombre=${1:-mundo}
echo "¡Hola, $nombre!"
echo "Me has dado $# argumentos."
```

The first line (`#!/bin/sh`) says which interpreter the script uses. Lines that start with `#` are comments.

**4.** Save with **Ctrl+S** and run it in two ways:

    sh saludo.sh Tito
    chmod +x saludo.sh      # lo hace ejecutable (una vez)
    ./saludo.sh Ana

With `./saludo.sh` the system reads the first line and uses `sagsh`. Inside the script, `$1` is the first argument
(`Tito`), `$#` how many there are and `$0` the name of the script itself.

## Variables and quotes

```sh
nombre="Sagui"           # sin espacios alrededor del =
echo "Hola $nombre"      # las comillas dobles dejan leer la variable
echo 'Hola $nombre'      # las simples lo dejan tal cual: Hola $nombre
echo "letras: ${#nombre}"           # 5
echo "sin definir: ${sin_valor:-valor por defecto}"
echo "sin extensión: ${archivo%.txt}"
```

Special variables: `$?` (exit code of the last command), `$$` (process number), `$1`...`$9` and `$@` (all the
arguments). To calculate, use `$(( ))`:

```sh
x=7
echo $((x * 6))          # 42
echo $(( (x + 3) / 2 ))  # 5
```

To store the output of a command in a variable, `$( )`:

```sh
fecha=$(data)
echo "Ahora son las $fecha"
```

## Conditions

```sh
if [ -f notas.txt ]; then
    echo "hay un archivo de notas"
elif [ -d notas ]; then
    echo "hay una carpeta notas"
else
    echo "no hay nada de eso"
fi
```

`[ ]` (or `test`) checks things: `-f` file, `-d` folder, `-e` exists, `-s` not empty, `-z` empty string, `-n`
non-empty string; and for numbers: `-eq`, `-ne`, `-lt`, `-le`, `-gt`, `-ge`. And for strings, `=` and `!=`.

```sh
edad=12
if [ "$edad" -ge 18 ]; then
    echo "adulto"
else
    echo "menor"
fi
```

Any command can also serve as a condition: `if grep -q hola notas.txt; then ...`. `case` is more convenient for
several options:

```sh
case "$1" in
    *.txt)  echo "un texto" ;;
    *.c)    echo "código en C" ;;
    *)      echo "otra cosa" ;;
esac
```

## Loops

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

`break` leaves the loop and `continue` jumps to the next round. `seq 1 5` writes the numbers from 1 to 5, useful
with `for i in $(seq 1 5)`.

## Functions

```sh
doble() {
    local x=$1           # local: la variable solo vale dentro de la función
    echo $((x * 2))
}

echo "el doble de 21 es $(doble 21)"
```

The function is called like a command: with its arguments and without parentheses. What it writes with `echo` is
what the caller gets with `$( )`. `return N` gives an exit code.

## Pipes and redirections

A **pipe** (`|`) passes the output of one command to the input of the next:

    ls /ejemplos | sort
    cat notas.txt | grep plátano | wc -l
    seq 1 10 | head -3

**Redirections** send the output to a file or take the input from one:

    echo "primera" > lista.txt       # escribe (borra lo anterior)
    echo "segunda" >> lista.txt      # añade al final
    sort < lista.txt                 # toma la entrada del archivo
    orden 2> errores.txt             # los errores a un archivo
    orden > salida.txt 2>&1          # salida y errores juntos

A **here-document** (`<<FIN`) writes several lines at once:

```sh
cat > tarea.txt << FIN
comprar pan
llamar a Ana
FIN
```

A command with `&` at the end runs in the background (`cuenta.sh &`); `jobs` lists those running and `wait` waits
for them to finish.

## Useful scripts

A script that counts the distinct words of a file, sorted by frequency:

```sh
#!/bin/sh
# palabras.sh: cuántas veces sale cada palabra en un archivo
if [ -z "$1" ]; then
    echo "uso: palabras.sh archivo"
    exit 1
fi
awk '{ for (i = 1; i <= NF; i++) print $i }' "$1" | sort | uniq -c | sort -rn | head -10
```

And the guessing game in `/ejemplos/adivina.sh`, which reads numbers with `read` until you get it right. The system
examples are in `/ejemplos/`: `hola.sh`, `funciones.sh` and `adivina.sh`.

## Your configuration: ~/.sagshrc

Every shell you open reads `/etc/sagshrc` first (for the whole system) and then **`~/.sagshrc`** (yours): aliases,
variables, the prompt (`PS1`) and functions. An example:

```sh
alias ll='ls -l'
export EDITOR=mico
mkcd() { mkdir -p "$1" && cd "$1"; }
```

To test a change without closing the Terminal: `. ~/.sagshrc`. Details in [The Terminal](../terminal.md).

## Typical errors

| What you see | What happens | What to do |
|--------------|--------------|------------|
| `error de sintaxis cerca de 'fi'` (syntax error near 'fi') | a `then`, `do` or a `;` before `fi`/`done` is missing | look at the line the message points to, and the one before it |
| `orden no encontrada` (command not found) | you wrote the name wrong, or it is not installed | `ayuda` lists the commands; `ls /bin` shows the ones there are |
| The script does not run with `./` | it lacks execute permission | `chmod +x guion.sh` |
| Variables with spaces get split | quotes are missing: `$archivo` contains a space | always write `"$archivo"` |
| `if [ $x = 1 ]` gives an error if `x` is empty | without quotes, the command loses an argument | use `"$x"` |
| The script does strange things | you do not know what is happening | `sh -x guion.sh` shows each command before running it |

Useful when debugging: `set -e` stops the script at the first error; `set -u` warns about undefined variables;
`set -x` shows each command.

## What comes next

- Larger scripts, with windows or calculations: [Python](python_EN.md) or [C](c_EN.md).
- The Terminal: [The Terminal](../terminal.md) and the commands in `ayuda ls`, `ayuda grep`...
- Other languages that also run in the terminal: [awk](../awk.md) (processing columns), [Lua](../lua.md), [make](../make.md).
- The index with all the languages: [Programming on SaguiOS](indice_EN.md).
