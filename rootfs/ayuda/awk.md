# awk: procesar texto por columnas

**awk** (`/bin/awk`) es *The One True AWK*, el de Brian Kernighan (versión 20261106). Lee un fichero o la entrada
estándar línea a línea, parte cada línea en campos (`$1`, `$2`... separados por espacios; `$0` es la línea entera,
`NF` el número de campos y `NR` el de la línea) y ejecuta el programa: una lista de `patrón { acción }`.

## Lo básico

    awk '{ print $2, $1 }' datos.txt              intercambia dos columnas
    awk -F: '{ print $1 }' /etc/passwd            otro separador de campos
    awk '$3 > 100 { n++ } END { print n }' f      patrón con condición; END corre al final
    awk 'BEGIN { printf "%.2f\n", 2/3 }'          BEGIN corre antes de leer nada
    awk -f guion.awk datos.txt                    el programa en un fichero
    awk -v iva=0.21 '{ print $1 * (1 + iva) }' f variable desde la línea de órdenes

## Lo que trae

- Arreglos asociativos (`cuenta[$1]++`, `for (k in cuenta)`), variables de cadena y de número (dobles), `printf` con
  `%d %s %f %.2f %x %c`.
- Cadenas: `length`, `substr`, `index`, `split`, `sprintf`, `toupper`, `tolower`, `sub`, `gsub`, `match`, con
  expresiones regulares extendidas.
- Entrada y salida: `getline`, `getline < fichero`, `"orden" | getline`, `print > fichero`, `print | "orden"`,
  `close()`, `system("orden")`, `fflush()`. Los pipes y `system` usan `/bin/sh` (sagsh).
- `ENVIRON["HOME"]`, `ARGV`, `ARGC`, `srand`/`rand`, `int`, `sqrt`, `exp`, `log`, `sin`, `cos`, `atan2`.

El texto se trata por bytes (Latin-1, como el resto del terminal): `length("año")` es 3.

## Ejemplo

`/ejemplos/awk/informe.awk` suma por columnas, usa `gsub`, `printf`, `getline` desde un pipe y un `sort` por pipe:

    cd /ejemplos/awk
    awk -f informe.awk ventas.txt

Origen y licencia (permisiva, de Lucent): `docs/terceros.md`, D440. Manual completo: `man awk` del upstream
(github.com/onetrueawk/awk).
