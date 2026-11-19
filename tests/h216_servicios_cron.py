#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H216 (D216, mejora 074) - gestor de servicios y cron.

La imagen es la normal con una partición de datos ext2 en /home (persistente)
y, en un ramdisk propio, unos servicios y una tabla de cron de prueba:

  eco     script que escribe su PID en /home/eco.pid y «inicio/parado PID» en
          /home/eco.txt; espera a que se cierre su entrada (el aviso de parada)
  dep     espera=eco: solo arranca con eco en marcha y apunta el PID de eco
  cuelga  no atiende el aviso (bucle con sleep): al pararlo, se mata a los 2 s
  falla   termina con código 3 enseguida: se reinicia con espera 1 s, 2 s y,
          al tercer fallo, queda «fallido»
  /etc/crontab: "* * * * *" (escribe cada minuto), @reboot y @daily

Primer arranque (reloj del QEMU a las 10:00:30, para que el minuto cambie pronto):
  - los servicios arrancan solos, eco antes que dep (dependencias);
  - servicio listar/estado; parar (eco escribe «parado») e iniciar (PID nuevo);
  - kill del PID de eco: el supervisor lo vuelve a lanzar (PID nuevo);
  - falla queda «fallido» tras 3 fallos; parar cuelga lo mata a los 2 s;
  - cron: @reboot una vez, @daily (nunca ejecutada: tarea perdida) una vez,
    la de cada minuto se ejecuta; servicio reiniciar cron no repite las dos
    primeras; crontab rechaza una tabla con errores e instala una buena;
  - cron --proxima con la zona UTC contra una referencia en Python;
  - apagar-pc: el kernel espera al supervisor, dep se para antes que eco, eco
    deja «parado PID» en /home/eco.txt (debugfs en el PC) y el QEMU se apaga.
Segundo arranque, tres días después: la @daily perdida se ejecuta otra vez al
arrancar y la @reboot también; el cron del usuario sigue instalado.
"""
import calendar
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, extrair_particao, debugfs, particoes_img, RAIZ, SAIDA,  # noqa: E402
                  FalhaTeste)

DADOS_MB = 16

ECO_SH = """echo $$ > /home/eco.pid
echo "inicio $$" >> /home/eco.txt
while read linea; do :; done
echo "parado $$" >> /home/eco.txt
"""
DEP_SH = """while [ ! -f /home/eco.pid ]; do sleep 1; done
echo "dep $$ eco=$(cat /home/eco.pid)" >> /home/dep.txt
while read linea; do :; done
"""
CUELGA_SH = """echo $$ > /home/cuelga.pid
while true; do sleep 1; done
"""
SERVICIOS = {
    "eco.conf": "nombre=eco\ndescripcion=Servicio de prueba (escribe su PID)\n"
                "comando=/bin/sagsh /etc/servicios/eco.sh\narrancar=si\nreiniciar=fallo\nespera=/home\n",
    "dep.conf": "nombre=dep\ndescripcion=Depende de eco\ncomando=/bin/sagsh /etc/servicios/dep.sh\n"
                "arrancar=si\nreiniciar=fallo\nespera=eco,/home\n",
    "cuelga.conf": "nombre=cuelga\ncomando=/bin/sagsh /etc/servicios/cuelga.sh\narrancar=si\n"
                   "reiniciar=fallo\nparar_espera=2\n",
    "falla.conf": "nombre=falla\ncomando=/bin/sagsh -c \"exit 3\"\narrancar=si\nreiniciar=fallo\nmax_fallos=3\n",
}
CRONTAB = """# tabla de prueba (h216)
* * * * * echo minuto >> /home/cron-minuto.txt
@reboot echo arranque >> /home/cron-reboot.txt
@daily echo diaria >> /home/cron-diaria.txt
"""


def construir_imagen():
    b = os.path.join(RAIZ, "build")
    raiz = os.path.join(SAIDA, "h216-rootfs")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(b, "rootfs"), raiz)
    serv = os.path.join(raiz, "etc", "servicios")
    os.makedirs(serv, exist_ok=True)
    for nome, texto in SERVICIOS.items():
        with open(os.path.join(serv, nome), "w") as f:
            f.write(texto)
    for nome, texto in (("eco.sh", ECO_SH), ("dep.sh", DEP_SH), ("cuelga.sh", CUELGA_SH)):
        with open(os.path.join(serv, nome), "w") as f:
            f.write(texto)
    with open(os.path.join(raiz, "etc", "crontab"), "w") as f:
        f.write(CRONTAB)
    sgfs = os.path.join(SAIDA, "h216.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(SAIDA, "h216.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", "--console", "--dados-mb", str(DADOS_MB)],
                   check=True, stdout=subprocess.DEVNULL)
    return img


# ------------------------------------------------------------ ayudas

def linhas(out):
    return [l.rstrip() for l in out.replace("\r", "").splitlines()]


def ler(m, arq):
    """Las líneas de un archivo del SaguiOS: lo que sale entre «executar /bin/cat ->
    processo N» y «processo N (cat) terminou» en la serie, sin las líneas del log
    del kernel («[SAGUI] ...», «[cron 6] ...»), que también salen por ella y pueden
    partir el eco del comando tecleado."""
    out = m.comando(f"cat {arq}")
    # El último cat: un servicio también puede usar cat (dep.sh) mientras se teclea.
    todos = list(re.finditer(r"\[SAGUI\] executar /bin/cat -> processo (\d+)\n", out))
    if not todos:
        return []
    a = todos[-1]
    b = out.find(f"[SAGUI] processo {a.group(1)} (cat) terminou", a.end())
    trecho = out[a.end():b if b >= 0 else len(out)]
    return [l for l in linhas(trecho) if l and "sagui:" not in l and not re.match(r"^\[[^\]]+\] ", l)]


def pid_de(m, arq):
    for l in ler(m, arq):
        if l.strip().isdigit():
            return int(l.strip())
    return None


def esperar_re(m, padrao, tempo, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        a = re.findall(padrao, m.serial()[desde:])
        if a:
            return a[-1]
        if m.proc.poll() is not None:
            raise FalhaTeste(f"QEMU terminou esperando {padrao!r}")
        time.sleep(0.3)
    raise FalhaTeste(f"tempo esgotado esperando {padrao!r}")


def terminou(m, tempo):
    try:
        m.proc.wait(timeout=tempo)
        return True
    except subprocess.TimeoutExpired:
        return False


# ------------------------------------------------------------ cron: referencia en Python (UTC)

def campo_py(txt, mn, mx, nomes=None, semana=False):
    vals = set()
    for item in txt.split(","):
        passo = 1
        if "/" in item:
            item, p = item.split("/")
            passo = int(p)
        if item == "*":
            a, b = mn, mx
        else:
            def v(x):
                if nomes and x.lower() in nomes:
                    return nomes.index(x.lower()) + (1 if nomes[0] == "jan" else 0)
                return int(x)
            if "-" in item:
                a, b = (v(x) for x in item.split("-"))
            else:
                a = v(item)
                b = mx if passo > 1 else a
        vals.update(x % 7 if semana else x for x in range(a, b + 1, passo))
    return vals


MESES = ["jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"]
DIAS = ["sun", "mon", "tue", "wed", "thu", "fri", "sat"]


def proximas_py(expr, desde, n):
    c = expr.split()
    mi, ho = campo_py(c[0], 0, 59), campo_py(c[1], 0, 23)
    di, me = campo_py(c[2], 1, 31), campo_py(c[3], 1, 12, MESES)
    se = campo_py(c[4], 0, 7, DIAS, semana=True)
    livre = c[2].startswith("*") or c[4].startswith("*")
    t = (desde // 60 + 1) * 60
    res = []
    while len(res) < n:
        g = time.gmtime(t)
        dow = (g.tm_wday + 1) % 7   # Python: lunes = 0
        dia_ok = (g.tm_mday in di and dow in se) if livre else (g.tm_mday in di or dow in se)
        if g.tm_min in mi and g.tm_hour in ho and g.tm_mon in me and dia_ok:
            res.append(t)
        t += 60
    return res


EXPRESIONES = ["*/15 9-17 * * 1-5", "0 0 31 * *", "30 4 1,15 * fri", "5/20 * * jan-mar sun", "0 12 28 feb *",
               "0 0 * * 7", "59 23 * * *"]


# ------------------------------------------------------------ primer arranque

def primeira(img):
    with Maquina("h216-a", "ide", imagem=img, snapshot=False, extras=["-rtc", "base=2026-11-18T10:00:30"]) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[SERVICOS] supervisor lanzado" in s, "el kernel lanza /bin/servicios")
        pid_eco = int(esperar_re(m, r"servicios: eco: en marcha \(pid (\d+)\)", 60))
        esperar_re(m, r"servicios: dep: en marcha \(pid (\d+)\)", 30)
        esperar_re(m, r"servicios: cron: en marcha \(pid (\d+)\)", 30)
        s = m.serial()
        verificar(s.index("servicios: eco: en marcha") < s.index("servicios: dep: en marcha"),
                  "dependencias: dep arranca después de eco")
        verificar(pid_de(m, "/home/eco.pid") == pid_eco, f"eco arrancó solo y escribió su PID ({pid_eco})")
        dep = ler(m, "/home/dep.txt")
        verificar(any(f"eco={pid_eco}" in l for l in dep), "dep vio a eco ya en marcha (su PID en /home/eco.pid)")

        out = m.comando("servicio listar")
        for nome in ("eco", "dep", "cron", "cuelga"):
            verificar(re.search(rf"^{nome}\s+en marcha\s+\d+\s+sí", out, re.M), f"servicio listar: {nome} en marcha")
        out = m.comando("servicio estado eco")
        verificar("Servicio de prueba" in out and f"PID:        {pid_eco}" in out and "/servicios/eco.log" in out,
                  "servicio estado eco")
        m.captura("H216-servicio-listar")

        # parar e iniciar
        out = m.comando("servicio parar eco", 30)
        verificar("eco: parado" in out, "servicio parar eco")
        verificar(f"parado {pid_eco}" in ler(m, "/home/eco.txt"), "eco recibió el aviso (fin de su entrada) y terminó limpio")
        out = m.comando("servicio iniciar eco", 30)
        n = re.search(r"eco: en marcha \(pid (\d+)\)", out)
        verificar(n and int(n.group(1)) != pid_eco, "servicio iniciar eco: PID nuevo")
        pid_eco = int(n.group(1))
        verificar(pid_de(m, "/home/eco.pid") == pid_eco, "el PID nuevo en /home/eco.pid")

        # matar: el supervisor lo reinicia
        marca = len(m.serial())
        m.comando(f"kill {pid_eco}")
        esperar_re(m, r"servicios: eco: terminó \(terminado por el sistema\)[^\n]*se reinicia en 1 s", 20, marca)
        novo = int(esperar_re(m, r"servicios: eco: en marcha \(pid (\d+)\)", 20, marca))
        verificar(novo != pid_eco, f"tras kill, eco vuelve a estar en marcha (pid {novo})")
        pid_eco = novo
        time.sleep(1)
        verificar(pid_de(m, "/home/eco.pid") == pid_eco, "y escribió su PID nuevo")
        out = m.comando("servicio estado eco")
        verificar("Reinicios:  1" in out, "servicio estado cuenta el reinicio")

        # falla: espera creciente y «fallido»
        esperar_re(m, r"servicios: falla: fallido tras 3 fallos seguidos", 60)
        s = m.serial()
        verificar(re.search(r"falla: terminó \(código 3\) tras \d+ s; se reinicia en 1 s", s) and
                  re.search(r"falla: terminó \(código 3\) tras \d+ s; se reinicia en 2 s", s), "falla: espera de 1 s y de 2 s")
        out = m.comando("servicio listar")
        verificar(re.search(r"^falla\s+fallido", out, re.M), "servicio listar: falla fallido")

        # cuelga: no atiende el aviso, se mata a los 2 s
        out = m.comando("servicio parar cuelga", 30)
        verificar("cuelga: parado" in out and "no terminó a tiempo" in m.serial(), "cuelga: se mata al agotar parar_espera")

        # cron: @reboot y @daily perdida, la de cada minuto
        esperar_re(m, r"cron: inicio pid \d+ \(@reboot\)", 30)
        esperar_re(m, r"cron: inicio pid \d+ \(tarea perdida\)", 30)
        time.sleep(1)
        verificar(ler(m, "/home/cron-reboot.txt") == ["arranque"], "@reboot se ejecutó una vez")
        verificar(ler(m, "/home/cron-diaria.txt") == ["diaria"], "@daily nunca ejecutada: se ejecuta al arrancar (tarea perdida)")
        esperar_re(m, r"cron: inicio pid \d+ \(a su hora\): echo minuto", 100)
        esperar_re(m, r"cron: fin pid \d+, código 0[^\n]*echo minuto", 20)
        verificar("minuto" in ler(m, "/home/cron-minuto.txt"), "la tarea * * * * * escribió en /home/cron-minuto.txt")
        out = m.comando("cat /tmp/.servicios/cron.log")   # sin partición /logs, los registros van a /tmp/.servicios
        verificar("fin pid" in out and "código 0" in out, "cron.log: inicio, fin, código y duración")

        marca = len(m.serial())
        out = m.comando("servicio reiniciar cron", 30)
        verificar(re.search(r"cron: en marcha \(pid \d+\)", out), "servicio reiniciar cron")
        esperar_re(m, r"cron: ya se había ejecutado en este arranque", 20, marca)
        time.sleep(2)
        verificar(ler(m, "/home/cron-reboot.txt") == ["arranque"] and ler(m, "/home/cron-diaria.txt") == ["diaria"],
                  "reiniciar el servicio cron no repite @reboot ni la @daily")

        # crontab
        out = m.comando("crontab -l; echo st=$?")
        verificar("no hay tabla" in out and "st=1" in out, "crontab -l sin tabla")
        m.comando("echo '61 * * * * echo mal' > /tmp/mala")
        out = m.comando("crontab /tmp/mala; echo st=$?")
        verificar("línea 1" in out and "fuera de rango" in out and "NO se ha instalado" in out and "st=1" in out,
                  "crontab rechaza una tabla con errores")
        m.comando("echo '*/1 * * * * echo usuario >> /home/cron-usuario.txt' > /tmp/buena")
        out = m.comando("crontab /tmp/buena")
        verificar("tabla instalada (1 tarea(s))" in out, "crontab instala una tabla correcta")
        out = m.comando("crontab -l")
        verificar("echo usuario" in out, "crontab -l la muestra")
        out = m.comando("cron proximas")
        verificar("echo minuto" in out and "echo usuario" in out and "al arrancar" in out, "cron proximas")

        # la próxima hora contra Python (UTC)
        desde = calendar.timegm((2026, 11, 18, 10, 7, 0))
        for e in EXPRESIONES:
            out = m.comando(f'TZ=UTC0 cron --proxima "{e}" {desde} 5')
            vistas = [int(x) for x in re.findall(r"^proxima: (\d+)", out, re.M)]
            ref = proximas_py(e, desde, 5)
            verificar(vistas == ref, f"cron --proxima '{e}': {vistas[:2]}... = referencia {ref[:2]}...")

        esperar_re(m, r"cron: inicio pid \d+ \(a su hora\): echo usuario", 100)

        # apagado: el supervisor para los servicios antes de desmontar
        marca = len(m.serial())
        m.digitar("apagar-pc\n", 0.05)
        verificar(terminou(m, 90), "apagar-pc: el QEMU se apaga (ACPI S5)")
        s = m.serial()[marca:]
        verificar("[SERVICOS] apagado: parando" in s and "[SERVICOS] servicios parados" in s,
                  "el kernel avisa al supervisor y espera a que pare los servicios")
        verificar(s.index("servicios: dep: parado") < s.index("servicios: eco: parado"),
                  "parada ordenada: dep (que depende de eco) antes que eco")
        verificar(s.index("[SERVICOS] servicios parados") < s.index("[ACPI] S5"), "los servicios paran antes de apagar")
        return pid_eco


def segunda(img):
    with Maquina("h216-b", "ide", imagem=img, snapshot=False, extras=["-rtc", "base=2026-11-21T10:00:30"]) as m:
        m.esperar_serial("sagui:/$", 120)
        esperar_re(m, r"cron: inicio pid \d+ \(tarea perdida\)", 60)
        esperar_re(m, r"cron: inicio pid \d+ \(@reboot\)", 30)
        time.sleep(2)
        verificar(ler(m, "/home/cron-diaria.txt") == ["diaria", "diaria"],
                  "tres días después: la @daily perdida se ejecuta al arrancar")
        verificar(ler(m, "/home/cron-reboot.txt") == ["arranque", "arranque"], "@reboot en el segundo arranque")
        out = m.comando("crontab -l")
        verificar("echo usuario" in out, "la tabla del usuario sigue instalada (/home persistente)")
        out = m.comando("servicio listar")
        verificar(re.search(r"^eco\s+en marcha", out, re.M), "eco vuelve a arrancar solo")
        m.comando("umount /home")


def main():
    img = imagem = construir_imagen()
    pid_eco = primeira(img)
    dados = [p for p in particoes_img(imagem) if p[1] != 0xDA]
    p = extrair_particao(imagem, dados[-1][2], DADOS_MB)  # D388: /home es la última lógica
    eco = debugfs(p, "cat /eco.txt").decode("latin-1").split("\n")
    verificar(f"parado {pid_eco}" in eco, f"debugfs: eco dejó «parado {pid_eco}» al apagar")
    segunda(img)
    return 0


if __name__ == "__main__":
    sys.exit(main())
