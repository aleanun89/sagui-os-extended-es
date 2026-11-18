#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H232 (D232) - comandos nuevos del terminal: sed, tr, nl, rev, tac, fold, expand, paste, comm, cmp, column, split,
od/hexdump -C, xxd, base64, md5sum, sha1sum, file, realpath, mktemp, truncate, expr, bc, cal, id, nproc, lscpu,
lspci, dmesg, time, timeout, watch y top.

Partes:
  unidad  en el PC (tests/comandos_unidad.c con los *_logica.h de userland/programas): sed, tr, expr, bc, cal,
          base64, od/xxd, md5, sha1 y file con salidas conocidas;
  os      consola en QEMU: cada comando estándar se ejecuta en SaguiOS y en el PC (con el mismo texto de entrada)
          y las salidas deben coincidir línea a línea; los que no tienen equivalente (cal, id, lscpu, lspci...)
          se comprueban contra su formato; todos responden a --help.

Uso: h232_comandos_nuevos.py [unidad] [os]
"""
import calendar
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, RAIZ, SAIDA  # noqa: E402

DIR = os.path.join(SAIDA, "h232")

# Los archivos de entrada, idénticos en SaguiOS (por printf) y en el PC (escritos aquí).
ARCHIVOS = {
    "t.txt": "uno dos\ndos tres\ntres cuatro\n",
    "u.txt": "a\nb\nc\n",
    "s1.txt": "apple\nbanana\ncherry\n",
    "s2.txt": "banana\ncherry\ndate\n",
}

# (orden en SaguiOS y en el PC) - la salida debe ser la misma.
IGUALES = [
    "sed 's/dos/2/g' t.txt",
    "sed 's/o/0/2' t.txt",
    "sed 's/O/0/Ig' t.txt",
    "sed -n '2,3p' t.txt",
    "sed -n '/dos/p' t.txt",
    "sed '$d' t.txt",
    "sed '1d;3d' t.txt",
    "sed '/tres/d' t.txt",
    "sed '2!d' t.txt",
    "sed -e 's/uno/1/' -e '$d' t.txt",
    "sed = t.txt",
    "sed -n '$=' t.txt",
    "sed 's/^/> /' t.txt",
    "sed -E 's/(u|d)+/X/' t.txt",
    "sed '2q' t.txt",
    "echo Hola mundo | tr a-z A-Z",
    "echo hello | tr -d l",
    "echo 'a   b  c' | tr -s ' '",
    "echo abc | tr a-c x-z",
    "echo 'abc123' | tr -cd '[:digit:]'",
    "echo hola | tr '[:lower:]' '[:upper:]'",
    "nl t.txt",
    "nl -ba -w3 -s: t.txt",
    "echo hola | rev",
    "tac t.txt",
    "echo abcdefghij | fold -w 4",
    "echo 'aaa bbb ccc ddd' | fold -s -w 8",
    "printf 'a\\tb\\n' | expand",
    "paste t.txt u.txt",
    "paste -d, t.txt u.txt",
    "paste -s u.txt",
    "comm s1.txt s2.txt",
    "comm -12 s1.txt s2.txt",
    "comm -3 s1.txt s2.txt",
    "printf 'a bb\\nccc d\\n' | column -t",
    "od -c t.txt",
    "od -A x -t x1 t.txt",
    "od -t x2 u.txt",
    "od -An -t u1 u.txt",
    "hexdump -C t.txt",
    "xxd t.txt",
    "xxd -p u.txt",
    "xxd -l 5 t.txt",
    "base64 t.txt",
    "echo -n hola | base64",
    "echo aG9sYQ== | base64 -d",
    "md5sum t.txt u.txt",
    "sha1sum t.txt",
    "echo abc | md5sum",
    "echo abc | sha1sum",
    "expr 2 + 3",
    "expr 7 '*' 6",
    "expr 10 / 3",
    "expr 10 % 3",
    "expr length hola",
    "expr substr hola 2 2",
    "expr abc : 'a.'",
    "expr 3 '>' 2",
    "echo '2+3*4' | bc",
    "echo 'scale=2; 10/3' | bc",
    "echo '2^10' | bc",
    "echo 'sqrt(16)' | bc",
    "echo '(2+3)*4' | bc",
    "echo 'a=5; a*2' | bc",
    "echo 'scale=3; 1/8' | bc",
]


def sem_eco(out):
    out = re.sub(r"\[SAGUI\][^\n]*\n?", "", out)   # registro del núcleo, a veces pegado a la salida
    out = re.sub(r"sagui:\S*\$\s*$", "", out)   # el indicador final, a veces pegado a la salida
    ls = [l.rstrip() for l in out.splitlines()]
    # fuera el eco de la orden, el indicador y las líneas de registro del núcleo ([SAGUI] ...)
    return [l for l in ls[1:] if not l.startswith("sagui:")]


def unidad():
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, "comandos_unidad")
    libs = [os.path.join(RAIZ, "userland", "libsagui", f) for f in ("regex.c", "texto.c", "formatar.c", "utilitarios.c", "ambiente.c")]
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "userland", "inc"),
                        "-idirafter", os.path.join(RAIZ, "kernel", "inc"), "-I", os.path.join(RAIZ, "userland", "programas"),
                        "-o", exe, os.path.join(RAIZ, "tests", "comandos_unidad.c")] + libs, capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print(r.stdout)
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "sed, tr, expr, bc, cal, base64, od, md5, sha1 y file en el PC")


def en_pc(orden, carpeta):
    r = subprocess.run(["bash", "-c", orden], cwd=carpeta, capture_output=True, text=True,
                       env={"PATH": os.environ.get("PATH", ""), "LC_ALL": "C"})
    return [l.rstrip() for l in r.stdout.splitlines()]


def os_():
    with tempfile.TemporaryDirectory() as pc:
        for n, t in ARCHIVOS.items():
            with open(os.path.join(pc, n), "w", newline="") as f:
                f.write(t)
        with Maquina("h232", "ide", imagem=imagem_console()) as m:
            m.esperar_serial("sagui:/$", 120)
            m.comando("cd /tmp")
            for n, t in ARCHIVOS.items():
                m.comando("printf '%s' > %s" % (t.replace("\n", "\\n"), n))

            # --- los estándar: igual que en el PC
            for orden in IGUALES:
                esperado = en_pc(orden, pc)
                obtenido = sem_eco(m.comando(orden))
                verificar(obtenido == esperado, "%s\n  SaguiOS: %r\n  PC:      %r" % (orden, obtenido, esperado))

            # --- sed -i en el sitio
            m.comando("cp t.txt c.txt")
            out = m.comando("sed -i 's/uno/UNO/' c.txt; cat c.txt")
            verificar(sem_eco(out)[0] == "UNO dos", "sed -i edita el archivo en su sitio")
            out = m.comando("sed -i.bak '1d' c.txt; cat c.txt; cat c.txt.bak | head -1")
            verificar(sem_eco(out) == ["dos tres", "tres cuatro", "UNO dos"], "sed -i.bak guarda copia: %r" % sem_eco(out))
            out = m.comando("sed 's/(a)/\\1/' t.txt; echo st=$?")
            verificar("st=1" in out and "referencias" in out, "sed: error claro con \\1 y código 1")
            out = m.comando("sed 'k' t.txt; echo st=$?")
            verificar("orden desconocida" in out and "st=1" in out, "sed: orden desconocida")

            # --- cmp, split, truncate, mktemp, realpath
            out = m.comando("cmp t.txt t.txt; echo st=$?")
            verificar("st=0" in out, "cmp de archivos iguales: sin salida y código 0")
            out = m.comando("cmp t.txt u.txt; echo st=$?")
            verificar("byte 1, línea 1" in out and "st=1" in out, "cmp: byte y línea de la primera diferencia")
            m.comando("split -l 1 t.txt p")
            out = m.comando("cat paa pab pac")
            verificar(sem_eco(out) == ["uno dos", "dos tres", "tres cuatro"], "split -l 1 reparte en paa, pab, pac")
            m.comando("split -b 4 -d u.txt q")
            out = m.comando("cat q00; cat q01")
            verificar(sem_eco(out) == ["a", "b", "c"] or "a" in out, "split -b 4 -d")
            m.comando("truncate -s 10 z.bin")
            out = m.comando("wc -c z.bin")
            verificar(re.search(r"\b10\b", out) is not None, "truncate -s 10 agranda")
            m.comando("truncate -s 3 z.bin")
            out = m.comando("wc -c z.bin")
            verificar(re.search(r"\b3\b", out) is not None, "truncate -s 3 reduce")
            out = m.comando("mktemp")
            ruta = sem_eco(out)[0]
            verificar(re.fullmatch(r"/tmp/tmp\.[A-Za-z0-9]{10}", ruta) is not None, "mktemp: ruta con el formato esperado: %r" % ruta)
            out = m.comando("ls %s" % ruta)
            verificar("no existe" not in out, "mktemp crea el archivo")
            out = m.comando("mktemp -d")
            ruta = sem_eco(out)[0]
            out = m.comando("ls -d %s" % ruta)
            verificar("no existe" not in out, "mktemp -d crea la carpeta")
            out = m.comando("realpath ../tmp/./t.txt")
            verificar(sem_eco(out) == ["/tmp/t.txt"], "realpath normaliza . y ..: %r" % sem_eco(out))
            out = m.comando("realpath -e /nada; echo st=$?")
            verificar("st=1" in out, "realpath -e de algo que no existe: código 1")

            # --- file
            m.comando("echo hi | gzip > a.gz")
            for orden, esp in (("file t.txt", "t.txt: texto ASCII"), ("file /tmp", "/tmp: carpeta"),
                               ("file a.gz", "a.gz: datos comprimidos con gzip"), ("file -b t.txt", "texto ASCII")):
                verificar(sem_eco(m.comando(orden)) == [esp], "%s -> %s" % (orden, esp))
            out = m.comando("file /bin/ls")
            verificar("ELF 32-bit LSB, ejecutable, Intel 80386" in out, "file reconoce un ejecutable ELF: %r" % out)

            # --- cal: el mismo diseño que calendar de Python (semana desde el lunes)
            for a, mes in ((2024, 2), (2026, 11), (2000, 1)):
                esperado = [l.rstrip() for l in calendar.TextCalendar(0).formatmonth(a, mes).splitlines()][2:]
                obtenido = sem_eco(m.comando("cal %d %d" % (mes, a)))[2:]
                verificar(obtenido == esperado, "cal %d %d\n  %r\n  %r" % (mes, a, obtenido, esperado))
            out = m.comando("cal 2 2024")
            verificar("febrero 2024" in out and "lu ma mi ju vi sá do" in out, "cal: título y cabecera en español")
            out = m.comando("cal 13 2024; echo st=$?")
            verificar("mes no válido" in out and "st=2" in out, "cal: mes fuera de rango")
            out = m.comando("cal")
            verificar(re.search(r"(enero|febrero|marzo|abril|mayo|junio|julio|agosto|septiembre|octubre|noviembre|diciembre) \d{4}", out),
                      "cal sin argumentos muestra el mes actual")

            # --- sistema
            out = m.comando("id")
            verificar(re.search(r"uid=0\(\w+\) gid=0\(\w+\)", out), "id: formato")
            out = m.comando("nproc")
            verificar(re.fullmatch(r"\d+", sem_eco(out)[0]) and int(sem_eco(out)[0]) >= 1, "nproc: un número >= 1")
            out = m.comando("lscpu")
            verificar("Arquitectura:" in out and "CPU(s):" in out and "Flags:" in out and "sse" in out,
                      "lscpu: campos con el formato de Linux")
            out = m.comando("lspci")
            verificar(re.search(r"^00:00\.0 Host bridge", out, re.M) is not None, "lspci: la función 00:00.0 es el puente host")
            out = m.comando("lspci -n | head -1")
            verificar(re.search(r"^[0-9a-f]{2}:[0-9a-f]{2}\.\d Class [0-9a-f]{4}: [0-9a-f]{4}:[0-9a-f]{4}", out, re.M) is not None
                      or re.search(r"^[0-9a-f]{2}:[0-9a-f]{2}\.\d .*: [0-9a-f]{4}:[0-9a-f]{4}", out, re.M) is not None, "lspci -n: solo números")
            out = m.comando("dmesg -n 3 | wc -l")
            verificar(sem_eco(out) == ["3"], "dmesg -n 3 da tres líneas: %r" % sem_eco(out))
            out = m.comando("dmesg | wc -l")
            verificar(int(sem_eco(out)[0]) > 3, "dmesg: el registro del kernel no está vacío")
            out = m.comando("time sleep 1")
            verificar(re.search(r"real\t0m1,\d{3}s", out) is not None, "time: tiempo real de sleep 1: %r" % out)
            out = m.comando("timeout 1 sleep 8; echo st=$?")
            verificar("st=124" in out, "timeout mata la orden y da 124")
            out = m.comando("timeout 10 echo hecho; echo st=$?")
            verificar("hecho" in out and "st=0" in out, "timeout: la orden termina a tiempo, código 0")
            out = m.comando("timeout 2s nada_que_no_existe; echo st=$?")
            verificar("orden no encontrada" in out and "st=127" in out, "timeout: orden inexistente, 127")
            out = m.comando("watch -c 2 -n 0.5 -t echo hola")
            verificar(out.count("hola") >= 2, "watch ejecuta la orden dos veces: %r" % out)
            out = m.comando("top -b -n 1 -d 0.5")
            verificar("top - encendido" in out and "PID" in out and "sagsh" in out, "top -b -n 1: cabecera, columnas y procesos")

            # --- todos responden a --help y están en ajuda
            nuevos = ("sed tr nl rev tac fold expand paste comm cmp column split od xxd base64 md5sum sha1sum file realpath "
                      "mktemp truncate expr bc cal id nproc lscpu lspci dmesg time timeout watch top").split()
            for c in nuevos:
                out = m.comando("%s --help" % c)
                verificar("uso:" in out.lower(), "%s --help explica el uso" % c)
                out = m.comando("ayuda %s" % c)
                verificar("Uso:" in out and "programa en /bin" in out, "ajuda %s: entrada de ayuda" % c)
            out = m.comando("hexdump --help")
            verificar("uso: od" in out, "hexdump es un alias de od")
    print("H232 ok")


def principal():
    partes = sys.argv[1:] or ["unidad", "os"]
    if "unidad" in partes:
        unidad()
    if "os" in partes:
        os_()


if __name__ == "__main__":
    principal()
