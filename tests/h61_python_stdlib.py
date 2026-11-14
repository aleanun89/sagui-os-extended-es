#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H61 - MicroPython: archivos, os, import y biblioteca estándar (mejora 032,
D067-D068).

Criterio:
  1. Archivos: open() en w/r/a/b/+/x, with, readline, iteración, seek/tell,
     io.StringIO/BytesIO, print(file=sys.stderr) y sys.stdout.write.
  2. os y os.path: listdir, mkdir, makedirs, rename, remove, rmdir, stat,
     getcwd/chdir, environ/getenv/putenv, urandom, system (con el código de
     salida y el entorno), join/exists/isdir/isfile/basename/dirname/split/
     splitext/abspath.
  3. import de un módulo propio (carpeta del script en sys.path), de un
     paquete con __init__.py y de /lib/python (saludos); sys.path y argv.
  4. json de ida y vuelta, re, random (semilla del núcleo), struct,
     collections (deque, namedtuple, OrderedDict, defaultdict), hashlib (md5,
     sha1, sha256, sha512 comparados con el hashlib del anfitrión), binascii
     (base64, hexlify), errno, array, heapq y time (localtime, strftime,
     mktime, ticks).
  5. Unicode: len("ñ") == 1, print con acentos, input() con acentos tecleados
     en el terminal, un archivo Latin-1 escrito por el shell se lee bien, el
     modo texto escribe Latin-1 y encoding="utf-8" escribe UTF-8; el REPL
     con acentos.
  6. Memoria: una lista de varios MB con el heap que crece solo, y -X heap
     fijo que da MemoryError.
"""
import hashlib
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA  # noqa: E402

DADOS = "SaguiOS y MicroPython: año, ñandú, café".encode("utf-8")

SCRIPTS = {
    "archivos.py": """
import sys, io
with open("/tmp/a.txt", "w") as f:
    f.write("uno\\n")
    print("dos", file=f)
with open("/tmp/a.txt", "a") as f:
    f.write("tres\\n")
with open("/tmp/a.txt") as f:
    print("lineas:", [l.rstrip() for l in f])
f = open("/tmp/a.txt")
print("readline:", f.readline().strip(), f.readline().strip())
f.seek(0)
print("seek:", f.read(3), f.tell())
f.close()
print("cerrado:", f.closed)
with open("/tmp/b.bin", "wb") as f:
    f.write(bytes(range(256)))
d = open("/tmp/b.bin", "rb").read()
print("binario:", len(d), d[0], d[255], type(d).__name__)
with open("/tmp/b.bin", "r+b") as f:
    f.seek(10)
    f.write(b"XY")
    f.seek(9)
    print("r+b:", f.read(4))
try:
    open("/tmp/a.txt", "x")
except OSError as e:
    print("x existe:", e.errno)
try:
    open("/no/hay/nada.txt")
except OSError as e:
    print("no existe:", e.errno, e)
s = io.StringIO()
s.write("hola ")
print("mundo", file=s)
print("stringio:", repr(s.getvalue()))
b = io.BytesIO(b"abc\\ndef\\n")
print("bytesio:", b.readline(), b.read())
print("a stderr", file=sys.stderr)
sys.stdout.write("stdout.write ok\\n")
sys.stdout.buffer.write(b"buffer ok\\n")
""",
    "sistema.py": """
import os, os.path
os.chdir("/tmp")
print("cwd:", os.getcwd())
os.mkdir("d1")
os.makedirs("d1/d2/d3")
os.makedirs("d1/d2", exist_ok=True)
open("d1/d2/f.txt", "w").write("12345")
print("listdir:", sorted(os.listdir("d1")), sorted(os.listdir("d1/d2")))
print("ilistdir:", sorted((n, t) for n, t, *_ in os.ilistdir("d1/d2")))
st = os.stat("d1/d2/f.txt")
print("stat:", st.st_size, st[6], st.st_mode & 0x8000 != 0, os.stat("d1").st_mode & 0x4000 != 0)
os.rename("d1/d2/f.txt", "d1/g.txt")
print("rename:", os.path.exists("d1/g.txt"), os.path.exists("d1/d2/f.txt"))
os.remove("d1/g.txt")
try:
    os.rmdir("d1")
except OSError as e:
    print("rmdir no vacia:", e.errno)
os.rmdir("d1/d2/d3"); os.rmdir("d1/d2"); os.rmdir("d1")
print("borrado:", os.path.exists("d1"))
print("path:", os.path.join("a", "b", "c.txt"), os.path.join("a", "/b"), os.path.basename("/x/y.txt"),
      os.path.dirname("/x/y.txt"), os.path.split("/x/y.txt"), os.path.splitext("f.tar.gz"))
print("path2:", os.path.isdir("/tmp"), os.path.isfile("/tmp"), os.path.isfile("/bin/python"),
      os.path.abspath("x/../y"), os.path.normpath("/a//b/./c/.."), os.sep)
print("urandom:", len(os.urandom(64)), os.urandom(8) != os.urandom(8))
os.environ["SAGUI_PRUEBA"] = "tito"
os.putenv("OTRA", "1")
print("getenv:", os.getenv("SAGUI_PRUEBA"), os.getenv("NO_HAY", "nada"), "PATH" in os.environ)
r = os.system("echo hijo ve $SAGUI_PRUEBA; exit 7")
print("system:", r)
print("uname:", os.uname().sysname)
""",
    "importa.py": """
import sys
import mimodulo
import paquete
from paquete.sub import doble
import saludos
print("sys.path:", sys.path)
print("argv:", sys.argv)
print("modulo:", mimodulo.valor(), "paquete:", paquete.NOMBRE, doble(21))
print("lib:", saludos.hola("Tito"))
""",
    "mimodulo.py": "def valor():\n    return 'módulo propio'\n",
    "paquete/__init__.py": "NOMBRE = 'paquete ok'\n",
    "paquete/sub.py": "def doble(x):\n    return 2 * x\n",
    "stdlib.py": """
import json, re, random, struct, collections, hashlib, binascii, errno, array, heapq, time
d = {"nombre": "sagui", "lista": [1, 2.5, None, True], "anidado": {"ñ": "año"}}
t = json.dumps(d)
print("json:", json.loads(t) == d, json.loads('{"a": [1, {"b": null}]}')["a"][1]["b"])
print("re:", re.sub(r"[aeiou]", "_", "murcielago"), re.match(r"(\\d+)-(\\d+)", "10-20").groups(),
      re.search(r"o+", "foooo").group(0), re.compile(r",\\s*").split("a, b,c"))
random.seed(5); a = random.random(); random.seed(5); b = random.random()
print("random:", a == b, 1 <= random.randint(1, 6) <= 6, random.choice([7]), len(set(random.getrandbits(30) for _ in range(5))) > 1)
print("struct:", struct.pack("<HIb", 1, 2, -1), struct.unpack(">I", b"\\x00\\x00\\x01\\x00"), struct.calcsize("<HI"))
dq = collections.deque((), 3)
for i in range(5):
    dq.append(i)
print("deque:", list(dq), dq.popleft())
P = collections.namedtuple("P", ["x", "y"])
p = P(1, y=2)
print("namedtuple:", p, p.x + p.y)
od = collections.OrderedDict()
od["z"] = 1; od["a"] = 2
print("ordereddict:", list(od.keys()))
dd = collections.defaultdict(list)
dd["a"].append(1); dd["a"].append(2)
print("defaultdict:", dd["a"], len(dd["b"]))
D = open("/t/dados.bin", "rb").read()
for nome in ("md5", "sha1", "sha256", "sha512"):
    print("hash", nome, hashlib.new(nome, D).hexdigest())
h = hashlib.sha256(); h.update(D[:5]); h.update(D[5:])
print("hash incremental", h.hexdigest(), h.digest_size)
print("base64:", binascii.b2a_base64(b"SaguiOS").strip(), binascii.a2b_base64(b"U2FndWlPUw=="), binascii.hexlify(b"\\x00\\xfe"))
print("errno:", errno.ENOENT, errno.errorcode[errno.EEXIST])
ar = array.array("i", [1, 2, 3]); ar.append(4)
print("array:", sum(ar), len(ar), ar[3])
h = [5, 1, 4]; heapq.heapify(h); heapq.heappush(h, 0)
print("heapq:", [heapq.heappop(h) for _ in range(4)])
lt = time.localtime()
print("time:", lt[0] >= 2025, len(lt), time.mktime(lt) - int(time.time()) in (-1, 0, 1),
      time.strftime("%Y-%m-%d", lt) == "%04d-%02d-%02d" % lt[:3], time.gmtime(1700000000)[:6])
t0 = time.ticks_ms(); time.sleep_ms(120)
print("ticks:", time.ticks_diff(time.ticks_ms(), t0) >= 100, time.time() > 1.7e9, type(time.time()).__name__)
""",
    "unicode.py": """
import sys
s = "ñandú café año"
print("len:", len("ñ"), len(s), s[0], s[-3:], s.split()[1])
print("terminal: año, ñandú, ¿qué?")
print("ord:", ord("ñ"), chr(241), "ñ".encode(), b"\\xc3\\xb1".decode())
open("/tmp/u.txt", "w").write(s)
print("latin1 disco:", open("/tmp/u.txt", "rb").read())
print("lee texto:", open("/tmp/u.txt").read() == s, len(open("/tmp/u.txt").read()))
open("/tmp/u8.txt", "w", encoding="utf-8").write(s)
print("utf8 disco:", len(open("/tmp/u8.txt", "rb").read()), open("/tmp/u8.txt", encoding="utf-8").read() == s)
shell = open("/tmp/shell.txt").read().strip()
print("del shell:", shell, len(shell))
nombre = input("¿Cómo te llamas? ")
print("hola", nombre, "(%d letras)" % len(nombre))
""",
}


def main():
    b = os.path.join(RAIZ, "build")
    raiz = os.path.join(SAIDA, "h61-rootfs")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(b, "rootfs"), raiz)
    for nome, texto in SCRIPTS.items():
        caminho = os.path.join(raiz, "t", nome)
        os.makedirs(os.path.dirname(caminho), exist_ok=True)
        with open(caminho, "w", encoding="utf-8") as f:
            f.write(texto.lstrip())
    with open(os.path.join(raiz, "t", "dados.bin"), "wb") as f:
        f.write(DADOS)
    sgfs = os.path.join(SAIDA, "h61.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(SAIDA, "h61.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", "--console"], check=True, stdout=subprocess.DEVNULL)

    with Maquina("h61", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)

        # 1. arquivos
        out = m.comando("python /t/archivos.py", tempo=60)
        verificar("lineas: ['uno', 'dos', 'tres']" in out, "open: w, print(file=), a, with e iteração por linhas")
        verificar("readline: uno dos" in out and "seek: uno 3" in out and "cerrado: True" in out, "readline, seek, tell e closed")
        verificar("binario: 256 0 255 bytes" in out and "r+b: b'\\tXY\\x0c'" in out, "modo binário e r+b")
        verificar("x existe: 17" in out, "modo 'x' com o arquivo existente: EEXIST")
        verificar("no existe: 2 [Errno 2] ENOENT: no existe: /no/hay/nada.txt" in out, "arquivo inexistente: OSError ENOENT com o texto do SaguiOS")
        verificar("stringio: 'hola mundo\\n'" in out and "bytesio: b'abc\\n' b'def\\n'" in out, "io.StringIO e io.BytesIO")
        verificar("a stderr" in out and "stdout.write ok" in out and "buffer ok" in out, "sys.stderr, sys.stdout.write e sys.stdout.buffer")
        out = m.comando("python /t/archivos.py 2>/tmp/err.txt > /tmp/out.txt; cat /tmp/err.txt", tempo=60)
        verificar("a stderr" in out and "lineas:" not in out, "print(file=sys.stderr) vai para o descritor 2 (2> redireciona)")

        # 2. os
        out = m.comando("python /t/sistema.py", tempo=60)
        verificar("cwd: /tmp" in out, "os.chdir e os.getcwd")
        verificar("listdir: ['d2'] ['d3', 'f.txt']" in out and "ilistdir: [('d3', 16384), ('f.txt', 32768)]" in out, "mkdir, makedirs, listdir e ilistdir")
        verificar("stat: 5 5 True True" in out, "os.stat (st_size, índice e st_mode)")
        verificar("rename: True False" in out and "rmdir no vacia: 39" in out and "borrado: False" in out,
                  "rename, remove, rmdir (pasta não vazia = ENOTEMPTY)")
        verificar("path: a/b/c.txt /b y.txt /x ('/x', 'y.txt') ('f.tar', '.gz')" in out, "os.path: join, basename, dirname, split, splitext")
        verificar("path2: True False True /tmp/y /a/b /" in out, "os.path: isdir, isfile, abspath, normpath e os.sep")
        verificar("urandom: 64 True" in out, "os.urandom")
        verificar("getenv: tito nada True" in out, "os.environ, os.putenv e os.getenv")
        verificar("hijo ve tito" in out and "system: 7" in out, "os.system: o filho vê os.environ e o código de saída volta")

        # 3. import
        out = m.comando("python /t/importa.py um dois", tempo=60)
        verificar("sys.path: ['/t', '/lib/python']" in out and "argv: ['/t/importa.py', 'um', 'dois']" in out,
                  "sys.path tem a pasta do script e /lib/python; sys.argv")
        verificar("modulo: módulo propio paquete: paquete ok 42" in out, "import de um módulo e de um pacote (__init__.py)")
        verificar("lib: ¡Hola, Tito! Saludos desde /lib/python." in out, "import de /lib/python (saludos)")
        out = m.comando("cd /t; python -c 'import mimodulo; print(mimodulo.valor())'; cd /", tempo=60)
        verificar("módulo propio" in out, "python -c: '' (pasta atual) em sys.path")

        # 4. biblioteca
        out = m.comando("python /t/stdlib.py", tempo=90)
        verificar("json: True None" in out, "json: dumps/loads de ida e volta")
        verificar("re: m_rc__l_g_ ('10', '20') oooo ['a', 'b', 'c']" in out, "re: sub, match/groups, search e split")
        verificar("random: True True 7 True" in out, "random: seed, randint, choice, getrandbits")
        verificar("struct: b'\\x01\\x00\\x02\\x00\\x00\\x00\\xff' (256,) 6" in out, "struct: pack, unpack e calcsize")
        verificar("deque: [2, 3, 4] 2" in out and "namedtuple: P(x=1, y=2) 3" in out and "ordereddict: ['z', 'a']" in out
                  and "defaultdict: [1, 2] 0" in out, "collections: deque, namedtuple, OrderedDict e defaultdict")
        for nome in ("md5", "sha1", "sha256", "sha512"):
            verificar(f"hash {nome} {hashlib.new(nome, DADOS).hexdigest()}" in out, f"hashlib.{nome} igual ao do anfitrião")
        verificar(f"hash incremental {hashlib.sha256(DADOS).hexdigest()} 32" in out, "hashlib: update em partes e digest_size")
        verificar("base64: b'U2FndWlPUw==' b'SaguiOS' b'00fe'" in out, "binascii: base64 e hexlify")
        verificar("errno: 2 EEXIST" in out and "array: 10 4 4" in out and "heapq: [0, 1, 4, 5]" in out, "errno, array e heapq")
        verificar("time: True 8 True True (2023, 11, 14, 22, 13, 20)" in out and "ticks: True True float" in out,
                  "time: localtime, mktime, strftime, gmtime, ticks e time() em float")

        # 5. unicode
        m.comando("echo año ñandú > /tmp/shell.txt")
        antes = m.serial()
        m.digitar("python /t/unicode.py\n", 0.04)
        m.esperar_serial("¿Cómo te llamas?", 60)
        time.sleep(0.5)
        m.digitar("Íñigo Peña\n", 0.06)
        m.esperar_serial("letras)", 30)
        time.sleep(1)
        out = m.serial()[len(antes):].replace("\r", "")
        verificar("len: 1 14 ñ año café" in out, "str unicode: len('ñ') == 1, índices e split")
        verificar("terminal: año, ñandú, ¿qué?" in out, "print com acentos chega ao terminal em Latin-1")
        verificar("ord: 241 ñ b'\\xc3\\xb1' ñ" in out, "ord/chr/encode/decode")
        verificar("latin1 disco: b'\\xf1and\\xfa caf\\xe9 a\\xf1o'" in out, "modo texto grava em Latin-1 (como o Editor)")
        verificar("lee texto: True 14" in out, "modo texto lê Latin-1 de volta")
        verificar("utf8 disco: 18 True" in out, "encoding='utf-8' grava e lê UTF-8")
        verificar("del shell: año ñandú 9" in out, "arquivo gravado pelo shell (Latin-1) lido como texto")
        verificar("hola Íñigo Peña (10 letras)" in out, "input() com acentos digitados no terminal")
        m.captura("H61-python-unicode", guardar=False)

        antes = m.serial().count(">>> ")
        m.digitar("python\n", 0.04)
        fim = time.time() + 30
        while m.serial().count(">>> ") <= antes and time.time() < fim:
            time.sleep(0.2)
        m.digitar("print(len('ñandú'), 'ñandú'.upper())\n", 0.05)
        time.sleep(2)
        m.tecla("ctrl-d")
        m.esperar_serial("(python) terminou com codigo 0", 20)
        verificar("5 ñANDú" in m.serial() or "5 ÑANDÚ" in m.serial(), "REPL com acentos digitados")

        # 6. memória
        out = m.comando("python -c 'import gc; l = list(range(2000000)); b = bytearray(8*1024*1024); print(\"grande:\", len(l), len(b))'", tempo=120)
        verificar("grande: 2000000 8388608" in out, "lista de 2 milhões (8 MB) e 8 MB de bytearray: o heap cresce sozinho")
        out = m.comando("python -X heap=2M -c 'b = bytearray(3*1024*1024)'", tempo=60)
        verificar("MemoryError" in out, "-X heap=2M fixo: MemoryError ao passar do limite")
        out = m.comando("PYTHONHEAP=32M python -c 'b = bytearray(24*1024*1024); print(\"fijo:\", len(b))'", tempo=60)
        verificar("fijo: 25165824" in out, "PYTHONHEAP=32M")
    return 0


if __name__ == "__main__":
    sys.exit(main())
