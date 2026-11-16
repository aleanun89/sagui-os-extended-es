#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H74 - comandos del Terminal (D097).

Criterio: en el terminal de pantalla completa, los comandos nuevos (head, tail,
grep, find, sort, uniq, touch, du, df, less, more, which, tree, cut, tee, seq,
basename, dirname, stat, uptime, free, uname, hostname, whoami, yes, sleep,
xargs, diff) y las opciones añadidas a ls, cp, mv, rm, mkdir, rmdir, cat, wc,
env, ps, kill e history funcionan como en Unix, también en pipes
(ls | grep x | wc -l, sort | uniq -c, find / -name "*.md" | head). Los errores
siguen el estilo «comando: archivo: no existe» y el código de salida es el
correcto. less pagina con las teclas y Ctrl+R busca en el historial.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402


def linhas(out):
    return [l.rstrip() for l in out.splitlines()]


def tem_linha(out, texto):
    return texto in linhas(out)


def sem_eco(out):
    """Quita la línea con el comando que el shell repite al teclearlo."""
    ls = linhas(out)
    return "\n".join(ls[1:]) if ls else ""


def principal():
    with Maquina("h74", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)

        # ---------------------------------------------------------------- ls
        m.comando("mkdir /tmp/d; echo aaaa > /tmp/d/a.txt; echo bb > /tmp/d/b.md; echo c > /tmp/d/.oculto")
        out = m.comando("ls /tmp/d")
        verificar(re.search(r"^a\.txt\s+5 bytes", out, re.M) and "oculto" not in out,
                  "ls: formato del terminal y sin ocultos")
        out = m.comando("ls -a /tmp/d")
        verificar(".oculto" in out, "ls -a muestra los ocultos")
        out = m.comando("ls -1 /tmp/d")
        verificar(tem_linha(out, "a.txt") and tem_linha(out, "b.md"), "ls -1: un nombre por línea")
        out = m.comando("ls /tmp/d | wc -l")
        verificar(tem_linha(out, "2"), "ls | wc -l cuenta nombres (2)")
        out = m.comando("ls -l /tmp/d")
        verificar(re.search(r"\d\d/\d\d/\d{4} \d\d:\d\d\s+5\s+a\.txt", out), "ls -l: fecha, tamaño y nombre")
        out = m.comando("ls -S /tmp/d | head -1")
        verificar(tem_linha(out, "a.txt"), "ls -S | head: el mayor primero")
        out = m.comando("ls -r /tmp/d | head -1")
        verificar(tem_linha(out, "b.md"), "ls -r invierte")
        out = m.comando("ls /tmp/d /nada; echo st=$?")
        verificar("/tmp/d:" in out and "ls: /nada: no existe" in out and tem_linha(out, "st=1"),
                  "ls con varios argumentos y un error: cabecera y código 1")
        out = m.comando("ls -x; echo st=$?")
        verificar("ls: opción desconocida: -x" in out and tem_linha(out, "st=2"), "ls -x: opción desconocida, código 2")
        out = m.comando("ls --help")
        verificar("uso: ls" in out and "-R" in out, "ls --help en español")

        # -------------------------------------------- mkdir, cp, mv, rm, rmdir
        m.comando("mkdir -p /tmp/e/f/g; echo uno > /tmp/e/f/x.txt")
        out = m.comando("ls -R /tmp/e")
        verificar("/tmp/e/f/g:" in out and "x.txt" in out, "mkdir -p crea la cadena y ls -R baja")
        out = m.comando("mkdir /tmp/e; echo st=$?")
        verificar("mkdir: /tmp/e: ya existe" in out and tem_linha(out, "st=1"), "mkdir sin -p: ya existe")
        out = m.comando("mkdir -p /tmp/e/f; echo st=$?")
        verificar(tem_linha(out, "st=0"), "mkdir -p sobre una carpeta existente: sin error")
        out = m.comando("cp -rv /tmp/e /tmp/e2")
        verificar("'/tmp/e/f/x.txt' -> '/tmp/e2/f/x.txt'" in out, "cp -rv copia el árbol y lo explica")
        out = m.comando("cp /tmp/e/f/x.txt /tmp/d/a.txt /tmp/e2; ls /tmp/e2")
        verificar(re.search(r"^x\.txt", out, re.M) and re.search(r"^a\.txt", out, re.M), "cp con varios orígenes a una carpeta")
        out = m.comando("cp /tmp/e /tmp/e3; echo st=$?")
        verificar("es una carpeta (usa -r" in out and tem_linha(out, "st=1"), "cp de carpeta sin -r: error")
        out = m.comando("cp -n /tmp/d/b.md /tmp/e2/a.txt; cat /tmp/e2/a.txt")
        verificar(tem_linha(out, "aaaa"), "cp -n no sobrescribe")
        out = m.comando("cp /nada /tmp/zz; echo st=$?")
        verificar("cp: /nada: no existe" in out and tem_linha(out, "st=1"), "cp: origen inexistente")
        out = m.comando("mv -v /tmp/e2/a.txt /tmp/e2/x.txt /tmp/d; ls /tmp/d")
        verificar("'/tmp/e2/a.txt' -> '/tmp/d/a.txt'" in out and re.search(r"^x\.txt", out, re.M), "mv con varios orígenes y -v")
        out = m.comando("mv /tmp/d/b.md /tmp/d/c.md; ls /tmp/d")
        verificar(re.search(r"^c\.md", out, re.M) and not re.search(r"^b\.md", out, re.M), "mv renombra")
        out = m.comando("rm /tmp/e2; echo st=$?")
        verificar("es una carpeta (usa -r)" in out and tem_linha(out, "st=1"), "rm de carpeta sin -r")
        out = m.comando("rm -rv /tmp/e2 | tail -1")
        verificar("carpeta '/tmp/e2' borrada" in out, "rm -rv borra el árbol")
        out = m.comando("rm -f /tmp/nada; echo st=$?")
        verificar(tem_linha(out, "st=0"), "rm -f de un archivo inexistente: sin error")
        out = m.comando("rm /tmp/nada; echo st=$?")
        verificar("rm: /tmp/nada: no existe" in out and tem_linha(out, "st=1"), "rm: error y código 1")
        out = m.comando("rmdir -pv /tmp/e/f/g; ls /tmp")
        verificar("rmdir: /tmp/e/f: la carpeta no está vacía" in out, "rmdir -p sube hasta que no está vacía")
        out = m.comando("rm -r /tmp/e; rmdir /tmp/e; echo st=$?")
        verificar("rmdir: /tmp/e: no existe" in out and tem_linha(out, "st=1"), "rmdir de lo inexistente")

        # -------------------------------------------------------------- cat, wc
        m.comando("printf 'uno dos\\n\\ntres\\n' > /tmp/t.txt")
        out = m.comando("cat -n /tmp/t.txt")
        verificar(re.search(r"^\s+1\tuno dos", out, re.M) and re.search(r"^\s+3\ttres", out, re.M), "cat -n numera todas")
        out = m.comando("cat -b /tmp/t.txt")
        verificar(re.search(r"^\s+2\ttres", out, re.M), "cat -b numera solo las no vacías")
        out = m.comando("cat /tmp/t.txt | cat - /tmp/d/a.txt | wc -l")
        verificar(tem_linha(out, "4"), "cat - lee la entrada estándar entre archivos")
        out = m.comando("cat /tmp/nada /tmp/d/a.txt; echo st=$?")
        verificar("cat: /tmp/nada: no existe" in out and tem_linha(out, "aaaa") and tem_linha(out, "st=1"),
                  "cat sigue tras un error y devuelve 1")
        out = m.comando("wc /tmp/t.txt")
        verificar(tem_linha(out, "3 líneas, 3 palabras, 14 letras") or "3 líneas, 3 palabras" in out,
                  "wc sin opciones: formato clásico")
        out = m.comando("wc -l /tmp/t.txt /tmp/d/a.txt")
        verificar(re.search(r"^\s+3 /tmp/t\.txt", out, re.M) and re.search(r"^\s+4 total", out, re.M), "wc -l con varios archivos y total")
        out = m.comando("echo uno dos tres | wc -w")
        verificar(tem_linha(out, "3"), "wc -w por entrada estándar")
        out = m.comando("wc -c /tmp/t.txt; echo st=$?")
        verificar(tem_linha(out, "14 /tmp/t.txt"), "wc -c")

        # --------------------------------------------------------- head, tail
        m.comando("seq 20 > /tmp/n.txt")
        out = m.comando("head /tmp/n.txt | wc -l")
        verificar(tem_linha(out, "10"), "head: 10 líneas por defecto")
        out = m.comando("head -n 3 /tmp/n.txt")
        verificar(tem_linha(out, "1") and tem_linha(out, "3") and not tem_linha(out, "4"), "head -n 3")
        out = m.comando("head -2 /tmp/n.txt | wc -l")
        verificar(tem_linha(out, "2"), "head -2 (forma antigua)")
        out = m.comando("head -c 4 /tmp/n.txt; echo")
        verificar("1\n2\n" in out.replace("\r", ""), "head -c 4 (bytes)")
        out = m.comando("tail -n 2 /tmp/n.txt")
        verificar(tem_linha(out, "19") and tem_linha(out, "20") and not tem_linha(out, "18"), "tail -n 2")
        out = m.comando("tail -n +19 /tmp/n.txt | wc -l")
        verificar(tem_linha(out, "2"), "tail -n +19: desde la línea 19")
        out = m.comando("seq 100 | tail -3 | head -1")
        verificar(tem_linha(out, "98"), "seq | tail | head en pipes")
        out = m.comando("head -n 1 /tmp/n.txt /tmp/t.txt")
        verificar("==> /tmp/n.txt <==" in out and "==> /tmp/t.txt <==" in out, "head con varios archivos: cabeceras")
        out = m.comando("head /nada; echo st=$?")
        verificar("head: /nada: no existe" in out and tem_linha(out, "st=1"), "head: error y código")
        out = m.comando("yes | head -n 2")
        verificar(out.count("\ny\n") >= 1 and "sagui:" in out, "yes | head termina (pipe cerrado)")

        # ------------------------------------------------------------- grep
        m.comando("printf 'Manzana\\nnaranja\\nPera\\nmanzana verde\\n' > /tmp/f.txt")
        out = m.comando("grep nar /tmp/f.txt")
        verificar(tem_linha(out, "naranja"), "grep texto")
        out = m.comando("grep -i manzana /tmp/f.txt | wc -l")
        verificar(tem_linha(out, "2"), "grep -i")
        out = m.comando("grep -n era /tmp/f.txt")
        verificar(tem_linha(out, "3:Pera") or tem_linha(out, "3:Pera"), "grep -n")
        out = m.comando("grep -c a /tmp/f.txt")
        verificar(tem_linha(out, "4"), "grep -c")
        out = m.comando("grep -v a /tmp/f.txt; echo st=$?")
        verificar(tem_linha(out, "st=1") and not tem_linha(out, "naranja"), "grep -v sin resultados: código 1")
        out = m.comando("grep -E 'verde|nar' /tmp/f.txt | wc -l")
        verificar(tem_linha(out, "2"), "grep -E con alternancia")
        out = m.comando("grep 'an*ja' /tmp/f.txt")
        verificar(tem_linha(out, "naranja"), "grep: expresión básica con *")
        out = m.comando("grep '[A-Z]' /tmp/f.txt | wc -l")
        verificar(tem_linha(out, "2"), "grep con clase [A-Z]")
        out = m.comando("grep -o 'an[a-z]*' /tmp/f.txt | head -1")
        verificar(tem_linha(out, "anzana"), "grep -o")
        out = m.comando("grep -w manzana /tmp/f.txt")
        verificar(tem_linha(out, "manzana verde"), "grep -w")
        out = m.comando("grep -F 'a v' /tmp/f.txt")
        verificar(tem_linha(out, "manzana verde"), "grep -F literal")
        out = m.comando("grep -r aaaa /tmp/d")
        verificar(tem_linha(out, "/tmp/d/a.txt:aaaa"), "grep -r con nombre de archivo")
        out = m.comando("grep -l aaaa /tmp/d/a.txt /tmp/f.txt")
        verificar(tem_linha(out, "/tmp/d/a.txt") and not tem_linha(out, "/tmp/f.txt"), "grep -l")
        out = m.comando("grep -A 1 Pera /tmp/f.txt")
        verificar(tem_linha(out, "Pera") and tem_linha(out, "manzana verde"), "grep -A 1 (contexto)")
        out = m.comando("grep x /nada; echo st=$?")
        verificar("grep: /nada: no existe" in out and tem_linha(out, "st=2"), "grep: archivo inexistente, código 2")
        out = m.comando("ls /bin | grep c | wc -l")
        verificar(re.search(r"^\d+$", out, re.M), "ls | grep | wc -l")
        out = m.comando("cat /tmp/f.txt | grep -c -i MANZANA")
        verificar(tem_linha(out, "2"), "grep lee la entrada estándar")

        # ------------------------------------------------------------- find
        m.comando("mkdir -p /tmp/w/a/b; echo 1 > /tmp/w/x.md; echo 22 > /tmp/w/a/y.md; echo 333 > /tmp/w/a/b/z.txt")
        out = m.comando("find /tmp/w -name '*.md'")
        verificar(tem_linha(out, "/tmp/w/x.md") and tem_linha(out, "/tmp/w/a/y.md") and not tem_linha(out, "/tmp/w/a/b/z.txt"),
                  "find -name con comodín")
        out = m.comando("find /tmp/w -type d")
        verificar(tem_linha(out, "/tmp/w") and tem_linha(out, "/tmp/w/a/b") and not tem_linha(out, "/tmp/w/x.md"), "find -type d")
        out = m.comando("find /tmp/w -type f -maxdepth 1")
        verificar(tem_linha(out, "/tmp/w/x.md") and not tem_linha(out, "/tmp/w/a/y.md"), "find -maxdepth 1")
        out = m.comando("find /tmp/w -iname 'X.MD'")
        verificar(tem_linha(out, "/tmp/w/x.md"), "find -iname")
        out = m.comando("find /tmp/w -size +2c -type f")
        verificar(tem_linha(out, "/tmp/w/a/b/z.txt") and not tem_linha(out, "/tmp/w/x.md"), "find -size +2c")
        out = m.comando("find /tmp/w -name z.txt -exec cat {} \\;")
        verificar(tem_linha(out, "333"), "find -exec orden {} ;")
        out = m.comando("find / -name '*.md' | head -3")
        verificar(len([l for l in linhas(out) if l.endswith(".md")]) >= 1, "find / -name *.md | head")
        out = m.comando("find /nada; echo st=$?")
        verificar("find: /nada: no existe" in out and tem_linha(out, "st=1"), "find: ruta inexistente")
        out = m.comando("find /tmp/w -name '*.md' | xargs wc -c")
        verificar("total" in out and re.search(r"^\s+5 total", out, re.M), "find | xargs wc -c")
        out = m.comando("find /tmp/w -name '*.txt' -delete; ls /tmp/w/a/b")
        verificar("z.txt" not in out, "find -delete")

        # -------------------------------------------------------- sort, uniq
        m.comando("printf 'pera\\nmanzana\\npera\\nkiwi\\nmanzana\\npera\\n' > /tmp/fr.txt; printf '10\\n9\\n100\\n2\\n' > /tmp/nu.txt")
        out = m.comando("sort /tmp/fr.txt | head -1")
        verificar(tem_linha(out, "kiwi"), "sort alfabético")
        out = m.comando("sort -r /tmp/fr.txt | head -1")
        verificar(tem_linha(out, "pera"), "sort -r")
        out = m.comando("sort -u /tmp/fr.txt | wc -l")
        verificar(tem_linha(out, "3"), "sort -u")
        out = m.comando("sort -n /tmp/nu.txt | head -1")
        verificar(tem_linha(out, "2"), "sort -n: 2 antes que 10")
        out = m.comando("sort -nr /tmp/nu.txt | head -1")
        verificar(tem_linha(out, "100"), "sort -nr")
        out = m.comando("sort /tmp/fr.txt | uniq -c")
        verificar(re.search(r"^\s+1 kiwi$", out, re.M) and re.search(r"^\s+2 manzana$", out, re.M) and re.search(r"^\s+3 pera$", out, re.M),
                  "sort | uniq -c")
        out = m.comando("sort /tmp/fr.txt | uniq -d | wc -l")
        verificar(tem_linha(out, "2"), "uniq -d")
        out = m.comando("printf 'b 2\\na 3\\nc 1\\n' | sort -k2 -n | head -1")
        verificar(tem_linha(out, "c 1"), "sort -k2 -n")
        out = m.comando("sort /nada; echo st=$?")
        verificar("sort: /nada: no existe" in out, "sort: archivo inexistente")

        # ----------------------------------- cut, tee, seq, basename, dirname
        out = m.comando("echo a:b:c | cut -d: -f2")
        verificar(tem_linha(out, "b"), "cut -d -f")
        out = m.comando("echo abcdef | cut -c 2-4")
        verificar(tem_linha(out, "bcd"), "cut -c 2-4")
        out = m.comando("echo uno | tee /tmp/tee.txt | wc -c; cat /tmp/tee.txt")
        verificar(tem_linha(out, "4") and tem_linha(out, "uno"), "tee copia a archivo y a la salida")
        out = m.comando("echo dos | tee -a /tmp/tee.txt > /tmp/descarte; wc -l /tmp/tee.txt")
        verificar("2 /tmp/tee.txt" in out, "tee -a añade")
        out = m.comando("seq 3")
        verificar(tem_linha(out, "1") and tem_linha(out, "3"), "seq 3")
        out = m.comando("seq 2 2 8 | wc -l")
        verificar(tem_linha(out, "4"), "seq 2 2 8")
        out = m.comando("seq -s , 4")
        verificar(tem_linha(out, "1,2,3,4"), "seq -s")
        out = m.comando("seq -w 8 10")
        verificar(tem_linha(out, "08") and tem_linha(out, "10"), "seq -w")
        out = m.comando("basename /a/b/c.txt .txt; dirname /a/b/c.txt; dirname c.txt")
        verificar(tem_linha(out, "c") and tem_linha(out, "/a/b") and tem_linha(out, "."), "basename y dirname")

        # ------------------------- touch, stat, du, df, which, tree, sistema
        out = m.comando("touch /tmp/nuevo.txt; ls /tmp/nuevo.txt")
        verificar("nuevo.txt" in out, "touch crea un archivo vacío")
        out = m.comando("touch -c /tmp/no_se_crea; ls /tmp/no_se_crea; echo st=$?")
        verificar("no existe" in out, "touch -c no crea")
        out = m.comando("stat /tmp/t.txt")
        verificar("Tamaño:  14 bytes" in out and "Tipo:    archivo" in out, "stat")
        out = m.comando("du -b /tmp/w | tail -1")
        verificar(re.search(r"^\d+\t/tmp/w$", out, re.M), "du -b")
        out = m.comando("du -s /tmp/w")
        verificar(re.search(r"^\d+\t/tmp/w$", out, re.M), "du -s")
        out = m.comando("du -sh /bin")
        verificar(re.search(r"^[\d.]+[KMG]\t/bin$", out, re.M), "du -sh")
        out = m.comando("df")
        verificar("Montado en" in out and re.search(r"ramfs\s+ramfs\s+\d+", out), "df con la RAM")
        out = m.comando("df -h /tmp")
        verificar(re.search(r"/$", out, re.M) is not None, "df -h con una ruta")
        out = m.comando("which ls; which nada; echo st=$?")
        verificar(tem_linha(out, "/bin/ls") and "which: nada: no encontrado" in out and tem_linha(out, "st=1"), "which")
        out = m.comando("tree -L 2 /tmp/w")
        verificar("|-- a/" in out and "`-- x.md" in out and "carpetas" in out, "tree")
        out = m.comando("free -m")
        verificar("Mem:" in out and "alta:" in out, "free -m")   # D348: total, baja y alta
        out = m.comando("uptime")
        verificar(re.search(r"hasta .+, \d+ procesos", out) is not None, "uptime")
        out = m.comando("uname -s; uname -m")
        verificar(tem_linha(out, "SaguiOS") and tem_linha(out, "i686"), "uname")
        out = m.comando("hostname; whoami")
        # D255: las pruebas corren con la sesión como root (whoami = root, salvo SAGUI_SESION_ROOT=0)
        quem = "root" if os.environ.get("SAGUI_SESION_ROOT", "1") == "1" else "sagui"
        verificar("\nsagui\n" in out and f"\n{quem}\n" in out, "hostname y whoami")
        out = m.comando("sleep 0.3; echo despierto")
        verificar(tem_linha(out, "despierto"), "sleep")
        out = m.comando("sleep xx; echo st=$?")
        verificar("sleep: xx: tiempo no válido" in out and tem_linha(out, "st=2"), "sleep: argumento inválido")

        # ----------------------------------------------------- env, ps, kill
        out = m.comando("env FOO=hola env | grep FOO")
        verificar(tem_linha(out, "FOO=hola"), "env VAR=valor orden")
        out = m.comando("env -i FOO=1 env | wc -l")
        verificar(tem_linha(out, "1"), "env -i")
        out = m.comando("env nada_xyz x; echo st=$?")
        verificar("env: nada_xyz: orden no encontrada" in out and tem_linha(out, "st=127"), "env: orden inexistente")
        out = m.comando("ps -n | head -2")
        verificar(re.search(r"^\s+0\s+0\s", out, re.M) and "PID" not in sem_eco(out), "ps -n sin cabecera")
        # El PID 1 es la primera tarea del kernel: era «rede», pero desde la
        # v7.4.1 la crean antes «usb» (D227) y «cdrom» (D233). Basta con que
        # «ps -p 1» muestre una sola tarea del kernel.
        out = m.comando("ps -p 1")
        verificar(len(re.findall(r"\(kernel\)", sem_eco(out))) == 1, "ps -p 1")
        out = m.comando("ps -u | wc -l")
        verificar(re.search(r"^\d+$", out, re.M) is not None, "ps -u en un pipe")
        out = m.comando("kill -l")
        verificar("9) SIGKILL" in out, "kill -l")
        out = m.comando("kill -0 1; echo st=$?; kill -0 9999; echo st=$?")
        verificar(tem_linha(out, "st=0") and tem_linha(out, "st=1"), "kill -0")
        out = m.comando("contador 30 > /tmp/descarte & sleep 0.5; kill -TERM %1; echo st=$?")
        verificar("terminado" in out and tem_linha(out, "st=0"), "kill -TERM %trabajo")
        out = m.comando("kill -FOO 1; echo st=$?")
        verificar("señal desconocida" in out, "kill: señal desconocida")

        # ----------------------------------------------------------- diff
        m.comando("printf 'a\\nb\\nc\\n' > /tmp/d1.txt; printf 'a\\nx\\nc\\nd\\n' > /tmp/d2.txt")
        out = m.comando("diff /tmp/d1.txt /tmp/d2.txt; echo st=$?")
        verificar(tem_linha(out, "2c2") and tem_linha(out, "< b") and tem_linha(out, "> x") and tem_linha(out, "3a4") and tem_linha(out, "st=1"),
                  "diff formato normal")
        out = m.comando("diff -u /tmp/d1.txt /tmp/d2.txt")
        verificar(tem_linha(out, "-b") and tem_linha(out, "+x") and "@@ -1,3 +1,4 @@" in out, "diff -u")
        out = m.comando("diff /tmp/d1.txt /tmp/d1.txt; echo st=$?")
        verificar(tem_linha(out, "st=0"), "diff iguales: código 0")

        # ----------------------------------------------------------- xargs
        out = m.comando("echo a b c | xargs -n 1 echo X")
        verificar(tem_linha(out, "X a") and tem_linha(out, "X c"), "xargs -n 1")
        out = m.comando("echo uno | xargs")
        verificar(tem_linha(out, "uno"), "xargs sin orden usa echo")
        out = m.comando("ls /tmp/d | xargs -I{} echo [{}]")
        verificar(tem_linha(out, "[a.txt]"), "xargs -I{}")

        # --------------------------------------- pipes de varias etapas
        out = m.comando("ls /bin | grep -c .")
        verificar(re.search(r"^\d{2,}$", out, re.M) is not None, "ls /bin | grep -c .")
        out = m.comando("cat /tmp/fr.txt | sort | uniq -c | sort -rn | head -1")
        verificar(re.search(r"^\s+3 pera$", out, re.M) is not None, "cat | sort | uniq -c | sort -rn | head -1")
        out = m.comando("grep -r a /tmp/d | cut -d: -f1 | sort -u | wc -l")
        verificar(re.search(r"^\d+$", out, re.M) is not None, "grep -r | cut | sort -u | wc -l")

        # --------------------------------------------------- ajuda y --help
        out = m.comando("ajuda")
        for c in ("head", "tail", "grep", "find", "sort", "uniq", "touch", "xargs", "diff", "tree", "stat"):
            verificar(re.search(r"\b" + c + r"\b", out) is not None, f"ajuda lista {c}")
        out = m.comando("ajuda grep")
        verificar("Uso: grep" in out and "procurar" in out, "ajuda grep")
        for c in ("cp", "mv", "rm", "cat", "wc", "head", "tail", "grep", "find", "sort", "uniq", "du", "tree", "cut", "seq", "diff", "xargs"):
            out = m.comando(f"{c} --help")
            verificar(re.search(r"^uso: " + c, out, re.M) is not None, f"{c} --help")
        out = m.comando("procurar nar /tmp/f.txt")
        verificar(tem_linha(out, "naranja"), "alias en portugués: procurar = grep")

        # ------------------------------------------------ less y Ctrl+R
        m.comando("seq 60 > /tmp/n60.txt")
        antes = m.serial()
        m.digitar("less -n 10 /tmp/n60.txt\n", 0.04)
        time.sleep(3)
        s = m.serial()[len(antes):].replace("\r", "")
        verificar(tem_linha(s, "9") and not tem_linha(s, "10"), "less muestra la primera página (n-1 líneas)")
        m.digitar(" ", 0.1)
        time.sleep(1.5)
        s = m.serial()[len(antes):].replace("\r", "")
        verificar(tem_linha(s, "18") and "q salir" in s, "less: espacio avanza una página")
        m.digitar("q", 0.1)
        m.esperar_serial("sagui:", 30)
        out = m.comando("echo tras_less")
        verificar(tem_linha(out, "tras_less"), "less: q vuelve al prompt")
        antes = m.serial()
        m.digitar("seq 30 | less -n 5\n", 0.04)
        time.sleep(3)
        m.digitar("G", 0.1)
        time.sleep(1.5)
        s = m.serial()[len(antes):].replace("\r", "")
        verificar(tem_linha(s, "30"), "less con la entrada de un pipe: teclas por el terminal (G = final)")
        m.digitar("q", 0.1)
        m.esperar_serial("sagui:", 30)
        m.comando("echo cadena_unica_r")
        m.comando("echo otra")
        antes = m.serial()
        m.tecla("ctrl-r")
        m.digitar("unica", 0.1)
        time.sleep(1)
        m.tecla("ret")
        time.sleep(0.5)
        m.tecla("ret")
        time.sleep(1.5)
        s = m.serial()[len(antes):].replace("\r", "")
        verificar(tem_linha(s, "cadena_unica_r"), "Ctrl+R busca en el historial y la línea se ejecuta")
        out = m.comando("history 2")
        verificar(re.search(r"^\s+\d+\s+echo cadena_unica_r", out, re.M) is not None, "history N")
        out = m.comando("history -c; history")
        verificar(not re.search(r"echo cadena_unica_r", sem_eco(out)), "history -c borra el historial")

    print("H74 ok")


if __name__ == "__main__":
    principal()
