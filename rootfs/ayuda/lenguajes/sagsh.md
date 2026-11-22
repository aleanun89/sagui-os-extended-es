<!-- categoria: Programación/sagsh; orden: 60; titulo: sagsh -->
# sagsh: el lenguaje del shell

**sagsh** es el intérprete de órdenes de SaguiOS: lo que escribes en el Terminal. Pero también es un **lenguaje de
programación**: con variables, condiciones, bucles y funciones puedes escribir un **guion** (un archivo con órdenes)
que hace un trabajo entero: renombrar fotos, buscar en muchos archivos, avisar cuando algo cambia...

Es el mismo lenguaje que `sh` y casi el mismo que `bash` de Linux. Lo que aprendas aquí sirve en otros sistemas.
La referencia corta está en el propio sistema: escribe `ayuda sh` en el Terminal.

## Primer guion, paso a paso

**1.** En el Terminal, crea una carpeta y entra en ella:

    mkdir guiones
    cd guiones

**2.** Abre el editor con el archivo nuevo:

    mico saludo.sh

**3.** Escribe:

```sh
#!/bin/sh
# saludo.sh: el primer guion
nombre=${1:-mundo}
echo "¡Hola, $nombre!"
echo "Me has dado $# argumentos."
```

La primera línea (`#!/bin/sh`) dice qué intérprete usa el guion. Las líneas que empiezan por `#` son comentarios.

**4.** Guarda con **Ctrl+S** y ejecútalo de dos formas:

    sh saludo.sh Tito
    chmod +x saludo.sh      # lo hace ejecutable (una vez)
    ./saludo.sh Ana

Con `./saludo.sh` el sistema lee la primera línea y usa `sagsh`. Dentro del guion, `$1` es el primer argumento
(`Tito`), `$#` cuántos son y `$0` el nombre del propio guion.

## Variables y comillas

```sh
nombre="Sagui"           # sin espacios alrededor del =
echo "Hola $nombre"      # las comillas dobles dejan leer la variable
echo 'Hola $nombre'      # las simples lo dejan tal cual: Hola $nombre
echo "letras: ${#nombre}"           # 5
echo "sin definir: ${sin_valor:-valor por defecto}"
echo "sin extensión: ${archivo%.txt}"
```

Variables especiales: `$?` (código de salida de la última orden), `$$` (número del proceso), `$1`...`$9` y `$@`
(todos los argumentos). Para calcular, usa `$(( ))`:

```sh
x=7
echo $((x * 6))          # 42
echo $(( (x + 3) / 2 ))  # 5
```

Para guardar la salida de una orden en una variable, `$( )`:

```sh
fecha=$(data)
echo "Ahora son las $fecha"
```

## Condiciones

```sh
if [ -f notas.txt ]; then
    echo "hay un archivo de notas"
elif [ -d notas ]; then
    echo "hay una carpeta notas"
else
    echo "no hay nada de eso"
fi
```

`[ ]` (o `test`) comprueba cosas: `-f` archivo, `-d` carpeta, `-e` existe, `-s` no está vacío, `-z` cadena vacía,
`-n` cadena no vacía; y para números: `-eq`, `-ne`, `-lt`, `-le`, `-gt`, `-ge`. Y para cadenas, `=` y `!=`.

```sh
edad=12
if [ "$edad" -ge 18 ]; then
    echo "adulto"
else
    echo "menor"
fi
```

Una orden cualquiera también sirve como condición: `if grep -q hola notas.txt; then ...`. `case` es más cómodo
para varias opciones:

```sh
case "$1" in
    *.txt)  echo "un texto" ;;
    *.c)    echo "código en C" ;;
    *)      echo "otra cosa" ;;
esac
```

## Bucles

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

`break` sale del bucle y `continue` salta a la siguiente vuelta. `seq 1 5` escribe los números del 1 al 5, útil
con `for i in $(seq 1 5)`.

## Funciones

```sh
doble() {
    local x=$1           # local: la variable solo vale dentro de la función
    echo $((x * 2))
}

echo "el doble de 21 es $(doble 21)"
```

La función se llama como una orden: con sus argumentos y sin paréntesis. Lo que escribe con `echo` es lo que
recoge quien la llama con `$( )`. `return N` da un código de salida.

## Tuberías y redirecciones

Una **tubería** (`|`) pasa la salida de una orden a la entrada de la siguiente:

    ls /ejemplos | sort
    cat notas.txt | grep plátano | wc -l
    seq 1 10 | head -3

Las **redirecciones** mandan la salida a un archivo o la toman de uno:

    echo "primera" > lista.txt       # escribe (borra lo anterior)
    echo "segunda" >> lista.txt      # añade al final
    sort < lista.txt                 # toma la entrada del archivo
    orden 2> errores.txt             # los errores a un archivo
    orden > salida.txt 2>&1          # salida y errores juntos

Un **documento aquí** (`<<FIN`) escribe varias líneas de una vez:

```sh
cat > tarea.txt << FIN
comprar pan
llamar a Ana
FIN
```

Un programa con `&` al final corre en segundo plano (`cuenta.sh &`); `jobs` lista los que hay y `wait` espera a
que acaben.

## Guiones útiles

Un guion que cuenta palabras distintas de un archivo, ordenadas por frecuencia:

```sh
#!/bin/sh
# palabras.sh: cuántas veces sale cada palabra en un archivo
if [ -z "$1" ]; then
    echo "uso: palabras.sh archivo"
    exit 1
fi
awk '{ for (i = 1; i <= NF; i++) print $i }' "$1" | sort | uniq -c | sort -rn | head -10
```

Y el de la adivinanza de `/ejemplos/adivina.sh`, que lee números con `read` hasta acertar. Los ejemplos del
sistema están en `/ejemplos/`: `hola.sh`, `funciones.sh` y `adivina.sh`.

## Tu configuración: ~/.sagshrc

Cada shell que abres lee primero `/etc/sagshrc` (de todo el sistema) y después **`~/.sagshrc`** (el tuyo): alias,
variables, el prompt (`PS1`) y funciones. Un ejemplo:

```sh
alias ll='ls -l'
export EDITOR=mico
mkcd() { mkdir -p "$1" && cd "$1"; }
```

Para probar un cambio sin cerrar el Terminal: `. ~/.sagshrc`. Detalle en [El Terminal](../terminal.md).

## Errores típicos

| Lo que ves | Qué pasa | Qué hacer |
|------------|----------|-----------|
| `error de sintaxis cerca de 'fi'` | falta un `then`, `do` o un `;` antes de `fi`/`done` | mira la línea que indica el mensaje y la anterior |
| `orden no encontrada` | escribiste mal el nombre, o no está instalada | `ayuda` lista las órdenes; `ls /bin` muestra las que hay |
| El guion no se ejecuta con `./` | falta permiso de ejecución | `chmod +x guion.sh` |
| Las variables con espacios se parten | falta comillas: `$archivo` con un espacio dentro | escribe `"$archivo"` siempre |
| `if [ $x = 1 ]` da error si `x` está vacía | sin comillas, la orden se queda sin argumento | usa `"$x"` |
| El guion hace cosas raras | no sabes qué ocurre | `sh -x guion.sh` muestra cada orden antes de ejecutarla |

Útil al depurar: `set -e` hace que el guion se pare en el primer error; `set -u` avisa de variables sin definir;
`set -x` muestra cada orden.

## Qué sigue

- Scripts más grandes, con ventanas o cálculos: [Python](python.md) o [C](c.md).
- El Terminal: [El Terminal](../terminal.md) y las órdenes de `ayuda ls`, `ayuda grep`...
- Otros lenguajes que también corren en la terminal: [awk](../awk.md) (procesar columnas), [Lua](../lua.md), [make](../make.md).
- El índice con todos los lenguajes: [Programar en SaguiOS](indice.md).
