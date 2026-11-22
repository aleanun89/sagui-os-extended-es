# informe.awk - suma por columnas, gsub, printf, pipes y getline (D440). Uso: awk -f informe.awk ventas.txt
BEGIN { print "== informe ==" }
{
    cant[$1] += $2
    imp[$1] += $2 * $3
    total += $2 * $3
    filas++
}
END {
    for (k in cant)
        printf "%-8s %3d %8.2f\n", k, cant[k], imp[k] | "sort"
    close("sort")
    printf "total %.2f en %d filas, media %.3f\n", total, filas, total / filas
    s = "hola mundo, hola awk"
    n = gsub(/hola/, "adios", s)
    print n, s
    sub(/mundo/, "[&]", s)
    print s
    m = split("a:b:c:d", partes, ":")
    print m, partes[1] partes[4]
    "echo desde-un-pipe" | getline linea
    close("echo desde-un-pipe")
    print linea, length(linea), toupper(substr(linea, 1, 5)), index(linea, "un")
    cmd = "printf '3\n1\n2\n'"
    while ((cmd | getline v) > 0) suma += v
    close(cmd)
    print "suma del pipe", suma
    print sqrt(16), int(7.9), exp(0), log(1), sin(0), 2 ^ 10, 7 % 3, 1 / 4
    print (ENVIRON["AWKPRUEBA"] == "" ? "sin variable" : ENVIRON["AWKPRUEBA"])
    if ("abc123" ~ /^[a-z]+[0-9]+$/) print "regex ok"
    r = system("exit 3")
    print "system devolvio", r
    x = sprintf("%05.1f|%x|%c|%s", 3.14159, 255, 65, "fin")
    print x
}
