#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H392 - TCC (Tiny C Compiler) dentro de SaguiOS y AoEN compilable dentro (D384, mejora 142).

Partes (por defecto todas; o las que se pidan: h392_tcc.py pc saguios):
  pc       en el PC: /apps/bin/tcc es un ELF32 estático; /apps/tcc tiene cabeceras, libc.a, libSDL2.a,
           libtcc1.a y crt1.o; el cruzado del PC (build/tcc/anfitrion/i386-tcc) compila los ejemplos y AoEN
           a ELF32 estáticos a 0x80000000 sin intérprete ni segmentos RWX; documentación y licencia.
  saguios  en SaguiOS (escritorio, DRS sintéticos de tools/aoen_drs_sintetico.py en /jogos/AOE/DATA):
           tcc compila y ejecuta un «hola» (stdio), un programa de varios ficheros (también por partes, -c),
           uno con la libsagui (ventana) y uno con la SDL 2; «tcc -run» (stdio y libsagui con principal());
           -b (comprobación de límites: fuera de un array y memoria liberada, con fichero y línea, también con -run);
           libtcc (un programa compila C en memoria y lo llama); TCC se compila a sí mismo dentro y el nuevo compila
           un «hola»; y compila AoEN con /apps/aoen/fuentes/compilar.sh: el ./aoen resultante arranca, dibuja su
           menú y juega una escaramuza. Mide la autocompilación y la de AoEN (TCG o KVM, según SAGUI_KVM).
Capturas: docs/capturas/H392-*.png.
"""
import os
import re
import subprocess
import sys
import tempfile
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ, BUILD  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
FUENTES_TCC = os.path.join(RAIZ, "userland", "terceros", "tcc", "fuentes", ".preparado")
CRUZADO = os.path.join(BUILD, "tcc", "anfitrion", "i386-tcc")
TCC_RAIZ = os.path.join(BUILD, "tcc", "raiz")
OPC_AOEN = ["-DSAGUI_AOEN", "-DAOEN_FONTE_SAGUI", "-DSTBI_NO_THREAD_LOCALS", "-DSTBI_NO_SIMD", "-DNDEBUG"]


def readelf(opc, f):
    return subprocess.run(["readelf", opc, "-W", f], capture_output=True, text=True).stdout


def elf_estatico_saguios(f, nome):
    h, seg = readelf("-h", f), readelf("-l", f)
    cargas = [l.split() for l in seg.splitlines() if l.strip().startswith("LOAD")]
    verificar("ELF32" in h and "Intel 80386" in h and "EXEC" in h, "%s: ELF32 ejecutable de i386" % nome)
    verificar("INTERP" not in seg, "%s: estático (sin PT_INTERP)" % nome)
    verificar(cargas and all(int(c[2], 16) >= 0x80000000 for c in cargas), "%s: segmentos desde 0x80000000" % nome)
    verificar(not [c for c in cargas if "RWE" in " ".join(c)], "%s: ningún segmento RWX (W^X)" % nome)


def pc():
    tcc = os.path.join(APPS_RAIZ, "bin", "tcc")
    verificar(os.path.isfile(tcc), "la imagen lleva /apps/bin/tcc")
    elf_estatico_saguios(os.path.join(BUILD, "userland", "bin", "tcc.elf"), "/apps/bin/tcc")
    for f in ("lib/crt1.o", "lib/crti.o", "lib/crtn.o", "lib/libc.a", "lib/libm.a", "lib/libSDL2.a", "lib/libSDL2_mixer.a",
              "lib/libtcc1.a", "lib/runmain.o", "include/stdio.h", "include/stdint.h", "include/stddef.h",
              "include/tccdefs.h", "include/sagui/sagui.h", "include/kernel/abi.h", "include/SDL2/SDL.h",
              "include/SDL2/SDL2/SDL.h", "ejemplos/hola.c", "LICENCIA-TCC.txt", "leeme.txt"):
        verificar(os.path.isfile(os.path.join(APPS_RAIZ, "tcc", f)), "/apps/tcc/%s" % f)
    ao = os.path.join(APPS_RAIZ, "aoen", "fuentes")
    verificar(os.path.isfile(os.path.join(ao, "compilar.sh")) and os.path.isfile(os.path.join(ao, "game.c"))
              and os.path.isfile(os.path.join(ao, "stb_image.h")), "/apps/aoen/fuentes lleva los fuentes y compilar.sh")
    # el cruzado del PC hace lo mismo que el de dentro: los ejemplos y AoEN salen ELF de SaguiOS
    d = tempfile.mkdtemp(prefix="h392-")
    ej = os.path.join(TCC_RAIZ, "ejemplos")
    casos = [("hola", ["hola.c"], []), ("calculos", ["principal.c", "calculos.c"], []),
             ("ventana", ["ventana.c"], []), ("sdl", ["sdl.c"], ["-lSDL2"])]
    for nome, fs, libs in casos:
        r = subprocess.run([CRUZADO, "-B" + TCC_RAIZ, "-o", os.path.join(d, nome)] + [os.path.join(ej, f) for f in fs] + libs,
                           capture_output=True, text=True)
        verificar(r.returncode == 0, "el cruzado compila el ejemplo %s (%s)" % (nome, r.stderr.strip()[-200:]))
        elf_estatico_saguios(os.path.join(d, nome), nome)
    linhas = open(os.path.join(ao, "compilar.sh"), encoding="utf-8").read()
    fontes = re.search(r'FUENTES="([^"]+)"', linhas).group(1).split()
    t0 = time.time()
    r = subprocess.run([CRUZADO, "-B" + TCC_RAIZ] + OPC_AOEN + ["-I.", "-o", os.path.join(d, "aoen")] + fontes + ["-lSDL2"],
                       cwd=ao, capture_output=True, text=True)
    verificar(r.returncode == 0, "el cruzado compila AoEN en %.2f s en el PC (%s)" % (time.time() - t0, r.stderr.strip()[-300:]))
    elf_estatico_saguios(os.path.join(d, "aoen"), "aoen (TCC)")
    # documentación y licencias
    t = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("TCC" in t and "LGPL-2.1-or-later" in t, "docs/terceros.md tiene la fila de TCC (LGPL-2.1-or-later)")
    t = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D384" in t, "docs/decisoes.md tiene D384")
    for f in ("mejoras/142-tcc.md", "docs/tcc.md", "docs/aoen-modificar.md", "rootfs/ayuda/tcc.md", "rootfs/ayuda/modificar-aoen.md"):
        verificar(os.path.isfile(os.path.join(RAIZ, f)), "existe %s" % f)
    ig = subprocess.run(["git", "ls-files", "userland/terceros/tcc"], cwd=RAIZ, capture_output=True, text=True).stdout
    verificar("fuentes/" not in ig, "las fuentes de TCC no van en git")


def esperar(m, padrao, tempo, desde):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.3)
    raise AssertionError("tiempo agotado esperando %r (%r)" % (padrao, m.serial()[desde:][-600:]))


def orden(m, texto, tempo):
    """Escribe una orden en el Terminal y espera al prompt siguiente; devuelve lo que salió."""
    marca = len(m.serial())
    m.digitar(texto, 0.03)
    m.tecla("ret")
    esperar(m, r"sagui:[^\n]*\$", tempo, marca + len(texto))
    return m.serial()[marca:].replace("\r", "")


def saguios():
    import h136_opentyrian as h136
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "tcc")):
        print("AVISO: la imagen no lleva tcc (tools/tcc_fuentes.sh y make): se salta la parte «saguios»")
        return
    lento = 1 if KVM else 4
    jogos = tempfile.mkdtemp(prefix="h392-jogos-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), os.path.join(jogos, "AOE", "DATA")],
                   check=True, stdout=subprocess.DEVNULL)
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    with Maquina("h392", "ide", imagem=img, memoria="512M") as m:
        esperar(m, r"sagui:/\$", 150, 0)
        time.sleep(4)
        h136.focar_terminal(m)
        orden(m, "cd /tmp", 30)
        # 1. hola (stdio)
        out = orden(m, "tcc /apps/tcc/ejemplos/hola.c -o hola", 60 * lento)
        verificar("error" not in out, "tcc compila hola.c (%r)" % out[-200:])
        out = orden(m, "./hola uno dos", 30 * lento)
        verificar("Hola desde TCC (3 argumentos)" in out and "argumento 2: dos" in out, "./hola escribe por stdio (%r)" % out[-200:])
        # 2. -run
        out = orden(m, "tcc -run /apps/tcc/ejemplos/hola.c x", 60 * lento)
        verificar("Hola desde TCC (2 argumentos)" in out, "tcc -run hola.c (%r)" % out[-200:])
        # 3. varios ficheros, de una vez y por partes
        out = orden(m, "tcc -o calc /apps/tcc/ejemplos/principal.c /apps/tcc/ejemplos/calculos.c && ./calc", 60 * lento)
        verificar("factorial(20) = 2432902008176640000" in out, "varios ficheros: long long (%r)" % out[-300:])
        verificar("factorial(20) / 1000003 = 2432894709492 (resto 511524)" in out, "varios ficheros: división de 64 bits (libtcc1)")
        verificar("raiz(2) = 1.414214" in out and "primos hasta 1000: 168" in out, "varios ficheros: x87 y bucles")
        out = orden(m, "tcc -c /apps/tcc/ejemplos/calculos.c -o calculos.o && tcc -c /apps/tcc/ejemplos/principal.c -o principal.o"
                       " && tcc -o calc2 principal.o calculos.o && ./calc2 10", 90 * lento)
        verificar("factorial(10) = 3628800" in out, "por partes (-c y enlazar los .o) (%r)" % out[-300:])
        # 4. libsagui: una ventana; y la misma con -run (principal() en vez de main())
        out = orden(m, "tcc /apps/tcc/ejemplos/ventana.c -o ventana", 60 * lento)
        verificar("error" not in out, "tcc compila ventana.c (%r)" % out[-200:])
        marca = len(m.serial())
        m.digitar("./ventana 5", 0.03)
        m.tecla("ret")
        esperar(m, r"ventana TCC: abierta", 60, marca)
        time.sleep(1.5)
        m.captura("H392-1-ventana-libsagui")
        esperar(m, r"ventana TCC: cerrada", 60, marca)
        esperar(m, r"sagui:[^\n]*\$", 30, marca + 12)
        verificar(True, "la ventana de la libsagui se abre y se cierra")
        marca = len(m.serial())
        m.digitar("tcc -run /apps/tcc/ejemplos/ventana.c 2", 0.03)
        m.tecla("ret")
        esperar(m, r"ventana TCC: cerrada", 120 * lento, marca)
        esperar(m, r"sagui:[^\n]*\$", 30, marca + 40)
        verificar(True, "tcc -run ventana.c: principal() y la libsagui del propio tcc")
        # 5. SDL 2
        out = orden(m, "tcc /apps/tcc/ejemplos/sdl.c -o sdl -lSDL2", 60 * lento)
        verificar("error" not in out, "tcc compila sdl.c con -lSDL2 (%r)" % out[-200:])
        marca = len(m.serial())
        m.digitar("./sdl 5", 0.03)
        m.tecla("ret")
        esperar(m, r"sdl TCC: ventana abierta", 60, marca)
        time.sleep(2)
        m.captura("H392-2-sdl")
        c = esperar(m, r"sdl TCC: (\d+) cuadros", 60, marca)
        esperar(m, r"sagui:[^\n]*\$", 30, marca + 12)
        verificar(int(c) > 5, "el programa SDL 2 dibuja %s cuadros" % c)
        # 6. -b: comprobación de límites (fuera de un array, memoria liberada) con fichero y línea
        out = orden(m, "tcc -b /apps/tcc/ejemplos/limites.c -o limites && ./limites", 90 * lento)
        verificar(re.search(r"limites\.c:13", out) and "BCHECK" in out and "puntos[9]" not in out,
                  "tcc -b: escribir fuera de un array se para en limites.c:13 (%r)" % out[-400:])
        out = orden(m, "tcc -b /apps/tcc/ejemplos/liberada.c -o liberada && ./liberada", 90 * lento)
        verificar(re.search(r"liberada\.c:13", out) and "BCHECK" in out and "primera letra" not in out,
                  "tcc -b: usar memoria liberada se para en liberada.c:13 (%r)" % out[-400:])
        out = orden(m, "tcc /apps/tcc/ejemplos/limites.c -o limites2 && ./limites2", 90 * lento)
        verificar("puntos[9] = 81" in out, "sin -b el mismo error pasa en silencio (%r)" % out[-200:])
        out = orden(m, "tcc -b -run /apps/tcc/ejemplos/liberada.c", 90 * lento)
        verificar(re.search(r"liberada\.c:13", out) and "BCHECK" in out, "tcc -b -run también lo detecta (%r)" % out[-400:])
        # 7. libtcc: un programa que compila C en memoria y lo llama
        out = orden(m, "tcc /apps/tcc/ejemplos/libtcc_hola.c -o libtcc_hola -ltcc && ./libtcc_hola", 120 * lento)
        verificar("compilado en memoria: suma de 2*i*i hasta 10" in out and "calcula(10) = 770" in out,
                  "libtcc: compila una cadena en memoria y la llama (%r)" % out[-300:])
        # 8. TCC se compila a sí mismo dentro y el nuevo compila y ejecuta un «hola»
        orden(m, "cp -r /apps/tcc/fuentes /tmp/tccfuentes", 120 * lento)
        orden(m, "cd /tmp/tccfuentes", 30)
        t0 = time.time()
        out = orden(m, "time sh compilar.sh", 900 if not KVM else 300)
        segundos = time.time() - t0
        verificar("listo: ./tcc" in out, "TCC se compila a sí mismo dentro de SaguiOS (%r)" % out[-400:])
        tr = re.search(r"real\s+(\S+)", out)
        print("    autocompilación de TCC: %.1f s de reloj (%s), time: %s" % (segundos, "KVM" if KVM else "TCG", tr.group(1) if tr else "?"))
        out = orden(m, "./tcc -run /apps/tcc/ejemplos/hola.c auto && ./tcc /apps/tcc/ejemplos/hola.c -o hola2 && ./hola2", 120 * lento)
        verificar(out.count("Hola desde TCC (2 argumentos)") == 1 and "Hola desde TCC (1 argumentos)" in out,
                  "el TCC compilado dentro compila y ejecuta hola.c (%r)" % out[-300:])
        orden(m, "cd /tmp", 30)
        # 9. AoEN compilado dentro
        orden(m, "cp -r /apps/aoen/fuentes /tmp/aoen", 120 * lento)
        orden(m, "cd /tmp/aoen", 30)
        t0 = time.time()
        out = orden(m, "time sh compilar.sh", 900 if not KVM else 300)
        segundos = time.time() - t0
        verificar("listo: ./aoen" in out, "compilar.sh compila AoEN dentro de SaguiOS (%r)" % out[-400:])
        tr = re.search(r"(\d+(?:[.,]\d+)?)\s*(ms|s)\b", out.split("listo: ./aoen")[-1])
        print("    AoEN compilado dentro en %.1f s de reloj (%s)%s" % (segundos, "KVM" if KVM else "TCG",
              ", time: %s %s" % tr.groups() if tr else ""))
        marca = len(m.serial())
        # AOEN_SHOT + AOEN_FRAMES: dibuja ese número de cuadros del menú, guarda uno y sale solo
        m.digitar("AOEN_NOSOUND=1 AOEN_SHOT=/tmp/menu.bmp AOEN_FRAMES=%d ./aoen" % (150 if KVM else 60), 0.03)
        m.tecla("ret")
        esperar(m, r"\[AOEN\] inicio: ventana \d+x\d+, datos /jogos/AOE/DATA", 120 * lento, marca)
        verificar(True, "el AoEN compilado dentro arranca y encuentra los DRS sintéticos")
        esperar(m, r"\[AOEN\] pantalla \d+x\d+", 120 * lento, marca)
        time.sleep(1.5)
        m.captura("H392-3-aoen-tcc-menu")
        esperar(m, r"\[AOEN\] captura guardada en el cuadro", 300 * lento, marca)
        esperar(m, r"sagui:[^\n]*\$", 120, marca + 60)
        # y una escaramuza: la lógica del juego (coma flotante x87, IA) compilada por TCC
        marca = len(m.serial())
        m.digitar("AOEN_NOSOUND=1 AOEN_SCENE=game AOEN_SELECT=1 AOEN_SHOT=/tmp/partida.bmp AOEN_FRAMES=%d ./aoen"
                  % (200 if KVM else 60), 0.03)
        m.tecla("ret")
        esperar(m, r"\[AOEN\] partida iniciada: 4 jugadores", 180 * lento, marca)
        e = esperar(m, r"\[AOEN\] estado t=\d+ ents=(\d+) sel=3", 180 * lento, marca)
        verificar(int(e) > 20, "el AoEN de TCC juega una escaramuza (%s entidades, 3 aldeanos elegidos)" % e)
        time.sleep(1)
        m.captura("H392-4-aoen-tcc-partida")
        esperar(m, r"\[AOEN\] captura guardada en el cuadro", 400 * lento, marca)
        esperar(m, r"sagui:[^\n]*\$", 120, marca + 60)
        # y con -b (la guía lo recomienda para buscar errores): la escaramuza corre sin avisos de BCHECK
        out = orden(m, "sh compilar.sh -b", 900 if not KVM else 300)
        verificar("listo: ./aoen" in out, "compilar.sh -b compila AoEN con comprobación de límites (%r)" % out[-300:])
        marca = len(m.serial())
        m.digitar("AOEN_NOSOUND=1 AOEN_SCENE=game AOEN_SELECT=1 AOEN_SHOT=/tmp/b.bmp AOEN_FRAMES=%d ./aoen" % (60 if KVM else 15), 0.03)
        m.tecla("ret")
        esperar(m, r"\[AOEN\] partida iniciada", 300 * lento, marca)
        esperar(m, r"sagui:[^\n]*\$", 900 if not KVM else 300, marca + 60)
        sb = m.serial()[marca:]
        bcheck = [l for l in sb.replace("\r", "").split("\n") if "BCHECK" in l]
        if bcheck:
            print("    AVISO para AoEN: -b encontró:\n      " + "\n      ".join(bcheck[:5]))
        verificar("[AOEN] captura guardada en el cuadro" in sb and not bcheck,
                  "AoEN con -b juega la escaramuza sin errores de memoria (%d avisos)" % len(bcheck))
        verificar("EXCECAO" not in m.serial(), "sin excepciones del kernel")


def main():
    if not os.path.exists(FUENTES_TCC):
        print("AVISO: faltan las fuentes de TCC (tools/tcc_fuentes.sh): h392 se salta")
        return 0
    partes = sys.argv[1:] or ["pc", "saguios"]
    for p in partes:
        {"pc": pc, "saguios": saguios}[p]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
