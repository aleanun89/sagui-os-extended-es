#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H461 - AoEN multijugador en red local (D461): lockstep determinista, sala y caída de jugadores.

Partes (por defecto todas; o las pedidas: h461_aoen_multijugador.py pc fuentes qemu):
  pc       En el PC: tests/aoen_red.c compila el motor sin ventana y varias copias hablan por UDP en 127.0.0.1, dando
           órdenes al azar por los cmd_* normales (en red se apuntan y viajan por el anfitrión). Todas tienen que dar la
           misma suma de sincronía en todos los turnos:
             - 2 personas + 1 jugador de la máquina; 3 personas + 2 de la máquina (5 jugadores); un cliente con 1 hilo y
               el resto con los de siempre;
             - con pérdida del 10 %, retardo de 25 ms y jitter de 15 ms en cada sentido;
             - un jugador que se va a mitad de partida pasa a la máquina y el resto sigue con la misma suma;
             - una desincronización provocada (AOEN_RED_FALLA) se detecta, se registra y deja el volcado;
             - pausa común (el reloj de la partida se detiene) y velocidad común;
             - descubrimiento (búsqueda) y chat; el anfitrión que desaparece deja a los clientes jugando solos.
  fuentes  Lo que tiene que haber: ficheros, D461, documentos, ayuda y la interfaz.
  qemu     Dos SaguiOS unidos por un cable de mentira (sockets UDP de QEMU): uno crea la sala, el otro la encuentra por
           difusión, entran, juegan unos turnos con órdenes guionadas y las sumas coinciden (datos DRS sintéticos).
Nunca se usan los datos del juego original.
"""
import os
import re
import socket
import subprocess
import sys
import tempfile
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import verificar, SAIDA  # noqa: E402

AOEN = os.path.join(RAIZ, "userland", "juegos", "aoen")


def ler(rel):
    return open(os.path.join(RAIZ, rel), encoding="utf-8").read()


def compilar():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "aoen_red")
    fuentes = [os.path.join(AOEN, f) for f in sorted(os.listdir(AOEN))
               if f.endswith(".c") and f not in ("aoen_config.c", "puente.c", "main.c", "rendimiento.c") and not f.startswith("ui_")]
    sdl = subprocess.run(["sdl2-config", "--cflags"], capture_output=True, text=True).stdout.split()
    libs = subprocess.run(["sdl2-config", "--libs"], capture_output=True, text=True).stdout.split()
    r = subprocess.run(["gcc", "-std=gnu99", "-O2", "-w", "-ffp-contract=off", "-I", AOEN, "-I", os.path.join(RAIZ, "userland", "terceros", "stbimage")] + sdl +
                       ["-o", exe, os.path.join(RAIZ, "tests", "aoen_red.c")] + fuentes + libs + ["-lm", "-lpthread"], capture_output=True, text=True)
    verificar(r.returncode == 0, "aoen_red.c compila con el motor: " + r.stderr[-1500:])
    return exe


def puertos(n):
    """n puertos UDP libres y consecutivos no hace falta: cada copia usa el suyo."""
    ss, ps = [], []
    for _ in range(n):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.bind(("127.0.0.1", 0))
        ss.append(s)
        ps.append(s.getsockname()[1])
    for s in ss:
        s.close()
    return ps


class Copia:
    def __init__(self, exe, args, entorno=None, nombre="?", carpeta=None):
        env = dict(os.environ, **(entorno or {}))
        env.setdefault("AOEN_RED_VOLCADO", carpeta or tempfile.mkdtemp(prefix="h461-"))
        env.setdefault("HOME", carpeta or env["AOEN_RED_VOLCADO"])
        self.nombre = nombre
        self.p = subprocess.Popen([exe] + [str(a) for a in args], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
        self.out = self.err = ""

    def esperar(self, tempo=180):
        try:
            self.out, self.err = self.p.communicate(timeout=tempo)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.out, self.err = self.p.communicate()
            self.out += "\nTIMEOUT"
        return self.p.returncode

    def sumas(self):
        return {int(t): h for t, h in re.findall(r"red: suma turno (\d+): ([0-9a-f]{8})", self.err)}

    def fin(self):
        m = re.search(r"FIN estado=(\d+) turno=(\d+) suma=([0-9a-f]{8}) t=([\d.]+) nents=(\d+) desinc=(-?\d+) tics=\d+ pausa=(\d+) vel=(\d+) chat=(\d+)", self.out)
        return m and dict(estado=int(m.group(1)), turno=int(m.group(2)), suma=m.group(3), t=float(m.group(4)), nents=int(m.group(5)),
                          desinc=int(m.group(6)), pausa=int(m.group(7)), vel=int(m.group(8)), chat=int(m.group(9)))


def partida(exe, humanos, ia, turnos, entorno=None, extras=(), extras_cli=None, anfitrion_extras=(), entorno_cli=None, tempo=240):
    """Lanza un anfitrión y humanos-1 clientes; devuelve la lista de Copias terminadas (la primera es el anfitrión)."""
    ps = puertos(humanos)
    carpeta = tempfile.mkdtemp(prefix="h461-")
    base = dict(entorno or {})
    anf = Copia(exe, ["anf", ps[0], humanos, ia, turnos] + list(extras) + list(anfitrion_extras), base, "anfitrión", carpeta)
    time.sleep(0.4)
    clis = []
    for k in range(1, humanos):
        env = dict(base, **((entorno_cli or {}).get(k, {})))
        ex = list(extras) + list((extras_cli or {}).get(k, []))
        clis.append(Copia(exe, ["cli", ps[0], ps[k], turnos, 0] + ex, env, "cliente %d" % k, carpeta))
    copias = [anf] + clis
    for c in copias:
        c.esperar(tempo)
    return copias, carpeta


def comparar(copias, desde=0, descripcion="", minimo=50):
    """Las copias que llegaron al final tienen la misma suma en todos los turnos que tienen en común."""
    s0 = copias[0].sumas()
    for c in copias[1:]:
        sc = c.sumas()
        comunes = sorted(t for t in s0 if t in sc and t >= desde)
        malas = [t for t in comunes if s0[t] != sc[t]]
        verificar(len(comunes) >= minimo, "%s: %s y el anfitrión tienen %d turnos con suma en común (mínimo %d)" % (descripcion, c.nombre, len(comunes), minimo))
        verificar(not malas, "%s: misma suma en %d turnos con %s (primera diferencia en el turno %s)" % (descripcion, len(comunes), c.nombre, malas[0] if malas else "-"))
    return s0


def pc():
    exe = compilar()
    # 1. dos personas y un jugador de la máquina
    cs, _ = partida(exe, 2, 1, 150)
    verificar(all(c.p.returncode == 0 for c in cs), "2 personas + 1 máquina: las dos terminan bien (%s)" % [c.p.returncode for c in cs])
    comparar(cs, descripcion="2 personas + 1 máquina")
    f = [c.fin() for c in cs]
    verificar(all(f) and f[0]["suma"] == f[1]["suma"] and f[0]["turno"] == 150, "suma final igual y turno 150: %s" % f)
    verificar(all("[AOEN] DESINCRONIZACION" not in c.err for c in cs), "sin desincronización")
    print("    2 personas + 1 máquina: %d turnos con suma igual; suma final %s, mundo de %d entidades" % (len(cs[0].sumas()), f[0]["suma"], f[0]["nents"]))
    # 2. tres personas y dos de la máquina; un cliente con un solo hilo
    cs, _ = partida(exe, 3, 2, 150, entorno_cli={2: {"AOEN_HILOS": "1"}})
    verificar(all(c.p.returncode == 0 for c in cs), "3 personas + 2 máquinas: todas terminan bien (%s)" % [c.p.returncode for c in cs])
    comparar(cs, descripcion="3 personas + 2 máquinas (un cliente con 1 hilo)")
    verificar(all(c.fin() and c.fin()["suma"] == cs[0].fin()["suma"] for c in cs), "suma final igual en las tres")
    # 3. pérdida y retardo
    ent = {"AOEN_RED_PERDIDA": "10", "AOEN_RED_RETARDO": "25", "AOEN_RED_JITTER": "15"}
    t0 = time.time()
    cs, _ = partida(exe, 2, 1, 120, entorno=ent)
    verificar(all(c.p.returncode == 0 for c in cs), "con pérdida del 10 %% y retardo: terminan bien (%s, %.0f s)" % ([c.p.returncode for c in cs], time.time() - t0))
    comparar(cs, descripcion="pérdida 10 % y retardo 25+-15 ms")
    # 4. un jugador se va
    cs, _ = partida(exe, 3, 1, 160, extras_cli={2: ["salir=60"]})
    verificar(cs[2].p.returncode == 0 and re.search(r"SALGO turno=\d+", cs[2].out) is not None, "el cliente 2 sale hacia el turno 60")
    verificar(cs[0].p.returncode == 0 and cs[1].p.returncode == 0, "el anfitrión y el otro cliente terminan bien (%s)" % [c.p.returncode for c in cs])
    comparar(cs[:2], descripcion="tras irse un jugador", minimo=100)
    verificar("pasa a la máquina" in cs[0].err and "pasa a la máquina" in cs[1].err, "los dos que quedan registran que su civilización pasa a la máquina")
    f = [c.fin() for c in cs[:2]]
    verificar(all(f) and f[0]["suma"] == f[1]["suma"] and f[0]["turno"] == 160, "suma final igual y turno 160 tras la caída: %s" % f)
    verificar(len(re.findall(r"JUG \d ia=1", cs[0].out)) == 2, "acaban dos jugadores de la máquina: la plaza de la máquina y la de quien se fue")
    # 5. desincronización provocada
    cs, car = partida(exe, 2, 1, 100, entorno_cli={1: {"AOEN_RED_FALLA": "40", "AOEN_RED_FALLA_JUG": "1"}})
    verificar("[AOEN] DESINCRONIZACION en el turno 40" in cs[0].err, "el anfitrión detecta la desincronización en el turno 40")
    verificar("[AOEN] DESINCRONIZACION" in cs[1].err, "el cliente también lo registra (aviso del anfitrión)")
    vol = [f for f in os.listdir(car) if f.startswith("desinc-j")]
    verificar(len(vol) >= 2, "se escribieron los volcados de depuración: %s" % vol)
    verificar(re.search(r"grupos distintos: 1", cs[0].err) is not None, "el registro dice que el grupo distinto es el de los jugadores (1): %s" % re.findall(r"grupos distintos:[^\n]*", cs[0].err)[:1])
    verificar(all(c.fin() and c.fin()["desinc"] == 40 for c in cs), "red_desincronizado() devuelve el turno 40")
    # 5b. el anfitrión desaparece: los clientes siguen solos contra la máquina
    cs, _ = partida(exe, 2, 1, 150, extras_cli={}, anfitrion_extras=["salir=50"])
    verificar(re.search(r"SALGO turno=\d+", cs[0].out) is not None, "el anfitrión se va hacia el turno 50")
    verificar("la partida sigue sola contra la máquina" in cs[1].err and "SOLO suma0=" in cs[1].out, "el cliente se queda solo y sigue jugando contra la máquina: %s" % re.findall(r"SOLO[^\n]*", cs[1].out))
    verificar(cs[1].p.returncode == 0, "el cliente termina bien (%s)" % cs[1].p.returncode)
    # 6. pausa común: el anfitrión pausa en el turno 30 y sigue en el 50; 20 turnos sin avanzar el reloj
    cs, _ = partida(exe, 2, 1, 120, extras=["pausa=30"])
    f = [c.fin() for c in cs]
    verificar(all(f) and f[0]["suma"] == f[1]["suma"], "con pausa: misma suma final")
    completo = 120 * 4 / 30.0
    verificar(all(0 < x["t"] <= completo - 15 * 4 / 30.0 for x in f), "pausa común: el reloj del juego va por %.2f s de %.2f (al menos 15 turnos parados)" % (f[0]["t"], completo))
    comparar(cs, descripcion="con pausa")
    # 7. velocidad común
    cs, _ = partida(exe, 2, 1, 100, extras=["vel=3@20"])
    f = [c.fin() for c in cs]
    verificar(all(f) and f[0]["vel"] == 3 and f[1]["vel"] == 3, "velocidad común x3 en las dos: %s" % [x and x["vel"] for x in f])
    comparar(cs, descripcion="con velocidad x3")
    # 8. búsqueda por red y chat
    cs, _ = partida(exe, 2, 1, 60, extras_cli={1: ["buscar=1", "chat=1"]}, anfitrion_extras=["chat=1"])
    verificar("ENCONTRADA Anfitrion" in cs[1].out, "el cliente encuentra la sala con la búsqueda (anuncio): %s" % re.findall(r"ENCONTRADA[^\n]*", cs[1].out))
    f = [c.fin() for c in cs]
    verificar(all(f) and all(x["chat"] >= 3 for x in f), "el chat llega a los dos (el aviso de entrada y un mensaje de cada uno): %s" % [x and x["chat"] for x in f])
    comparar(cs, descripcion="tras la búsqueda", minimo=30)
    # 8b. ritmo real (sin AOEN_RED_RAPIDO): 45 turnos de 4 tics = 6 s de juego, que tardan unos 6 s
    t0 = time.time()
    cs, _ = partida(exe, 2, 1, 45, entorno={"AOEN_RED_RAPIDO": "0"})
    dur = time.time() - t0
    verificar(all(c.p.returncode == 0 for c in cs), "a ritmo real: terminan bien")
    verificar(5.0 <= dur <= 14.0, "a ritmo real el reloj manda: 45 turnos tardaron %.1f s (unos 6 s de juego más el arranque)" % dur)
    comparar(cs, descripcion="a ritmo real", minimo=40)
    # 8c. ida y vuelta de 300 ms: el anfitrión alarga el turno (70 % del ida y vuelta en tics, hasta 6)
    cs, _ = partida(exe, 2, 1, 90, entorno={"AOEN_RED_RETARDO": "150"})
    verificar(all(c.p.returncode == 0 for c in cs), "con 150 ms de retardo por sentido: terminan bien")
    verificar(re.search(r"red: turnos de [56] tics", cs[0].err) is not None, "el anfitrión alarga el turno a 5 o 6 tics: %s" % re.findall(r"red: turnos de \d tics[^\n]*", cs[0].err)[:2])
    comparar(cs, descripcion="con 300 ms de ida y vuelta", minimo=40)
    # 9. fuego amigo (D447) viaja con la partida
    cs, _ = partida(exe, 2, 1, 40, extras=["fuego=1"])
    comparar(cs, descripcion="con fuego amigo", minimo=20)
    print("    PC: sumas iguales en todos los casos")


def fuentes():
    for f in ("red.c", "red.h", "ordenes.c", "ordenes.h", "ui_red.c"):
        verificar(os.path.isfile(os.path.join(AOEN, f)), "existe userland/juegos/aoen/%s" % f)
        t = ler("userland/juegos/aoen/" + f)
        verificar(t.splitlines()[0].startswith("/* SPDX-License-Identifier: GPL-3.0-or-later"), "%s lleva SPDX GPL-3.0-or-later" % f)
        verificar(not re.search(r"[Ͱ-ϿЀ-ӿ　-鿿]", t), "%s sin otros alfabetos" % f)
    d = ler("docs/decisoes.md")
    verificar(re.search(r"^## D461\. .*\(decidido el 2026-11-21; v8\.5\.5\.3, rama `v8\.5\.5\.3-aoen-multijugador`\)", d, re.M) is not None, "docs/decisoes.md tiene D461 con el formato pedido")
    a = ler("docs/aoen.md")
    verificar("D461" in a and "lockstep" in a.lower() and "multijugador" in a.lower(), "docs/aoen.md documenta el multijugador (D461)")
    h = ler("rootfs/ayuda/aoen.md")
    verificar("Multijugador" in h and "sala" in h.lower(), "rootfs/ayuda/aoen.md explica el multijugador")
    u = ler("userland/juegos/aoen/ui_menu.c")
    verificar("Multijugador (red local)" in u, "el menú principal tiene «Multijugador (red local)»")
    ui = ler("userland/juegos/aoen/ui_red.c")
    for t in ("Crear sala", "Salas en la red local", "Unirse por IP", "Empezar partida", "Estoy listo", "Enviar", "Esperando a", "Desincronizados"):
        verificar(t in ui, "ui_red.c tiene «%s»" % t)
    verificar("Fuego amigo" in ler("userland/juegos/aoen/ui_menu.c") and "sala_columna_mapa" in ler("userland/juegos/aoen/ui_menu.c"), "la sala del anfitrión reutiliza los controles de mapa y reglas (fuego amigo incluido)")
    verificar("DESINCRONIZACION" in ler("userland/juegos/aoen/red.c"), "red.c registra las desincronizaciones")
    verificar("-ffp-contract=off" in ler("tests/h461_aoen_multijugador.py"), "el arnés del PC se compila sin contracciones de coma flotante")


def jogos_sinteticos():
    jogos = tempfile.mkdtemp(prefix="aoen-red-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), os.path.join(jogos, "AOE", "DATA")],
                   check=True, stdout=subprocess.DEVNULL)
    return jogos


def qemu():
    """Dos SaguiOS unidos por un cable de mentira (sockets UDP de QEMU, como h437). El anfitrión crea la sala por las
    pantallas del juego, el cliente la encuentra por difusión, entran, juegan 120 turnos con órdenes al azar (guion
    AOEN_RED_AUTO) y las sumas de sincronía de cada turno coinciden. Datos DRS sintéticos."""
    from qemu import Maquina, imagem_de_teste, APPS_RAIZ
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "aoen")):
        print("AVISO: la imagen no lleva aoen (make): se salta «qemu»")
        return
    pa, pb = puertos(2)
    jogos = jogos_sinteticos()
    ia = imagem_de_teste("h461-a", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    ib = imagem_de_teste("h461-b", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    ea = ["-netdev", f"socket,id=n0,udp=127.0.0.1:{pb},localaddr=127.0.0.1:{pa}", "-device", "e1000,netdev=n0,mac=52:54:00:46:10:0a"]
    eb = ["-netdev", f"socket,id=n0,udp=127.0.0.1:{pa},localaddr=127.0.0.1:{pb}", "-device", "e1000,netdev=n0,mac=52:54:00:46:10:0b"]
    TURNOS = int(os.environ.get("H461_TURNOS", "120"))
    base = "AOEN_NOSOUND=1 AOEN_SEED=5 "   # a ritmo real: los turnos van al paso del reloj
    with Maquina("h461-a", "ide", imagem=ia, memoria="512M", extras=ea) as a, Maquina("h461-b", "ide", imagem=ib, memoria="512M", extras=eb) as b:
        for m in (a, b):
            esperar(m, r"sagui:/\$", 200)
        time.sleep(4)
        for m, ipc in ((a, "10.9.0.1/24"), (b, "10.9.0.2/24")):
            h136.focar_terminal(m)
            m.digitar("ifconfig eth0 " + ipc, 0.03)
            m.tecla("ret")
            time.sleep(1.5)
        ma, mb = len(a.serial()), len(b.serial())
        a.digitar(base + "AOEN_RED_AUTO_SALA=15000 AOEN_RED_AUTO=anf,%d,1 /apps/bin/aoen" % TURNOS, 0.03)
        a.tecla("ret")
        time.sleep(3)
        h136.focar_terminal(b)
        b.digitar(base + "AOEN_RED_AUTO=cli,%d /apps/bin/aoen" % TURNOS, 0.03)
        b.tecla("ret")
        esperar(a, r"\[AOEN\] red-auto: sala completa", 240, desde=ma)
        t_sala = time.time()
        a.captura("H461-sala-anfitrion")
        esperar(b, r"\[AOEN\] red-auto: listo en la sala", 60, desde=mb)
        b.captura("H461-sala-cliente")
        print("    capturas de la sala hechas %.1f s después de que el anfitrión la viera completa" % (time.time() - t_sala))
        esperar(a, r"\[AOEN\] red-auto: en juego como jugador 1", 240, desde=ma)
        esperar(b, r"\[AOEN\] red-auto: en juego como jugador 2", 240, desde=mb)
        time.sleep(3)
        a.captura("H461-partida-anfitrion")
        b.captura("H461-partida-cliente")
        fa = esperar(a, r"\[AOEN\] red-auto: FIN turno=(\d+) suma=([0-9a-f]{8}) t=([\d.]+) nents=(\d+) desinc=(-?\d+)", 600, desde=ma)
        fb = esperar(b, r"\[AOEN\] red-auto: FIN turno=(\d+) suma=([0-9a-f]{8}) t=([\d.]+) nents=(\d+) desinc=(-?\d+)", 600, desde=mb)
        esperar(a, r"sagui:/\$", 120, desde=ma)
        esperar(b, r"sagui:/\$", 120, desde=mb)
        sa, sb = a.serial()[ma:], b.serial()[mb:]
        print("    anfitrión: FIN turno=%s suma=%s t=%s nents=%s; cliente: FIN turno=%s suma=%s t=%s nents=%s" % (fa[0], fa[1], fa[2], fa[3], fb[0], fb[1], fb[2], fb[3]))
        verificar(fa[0] == fb[0] == str(TURNOS), "los dos llegan al turno %d" % TURNOS)
        verificar(fa[1] == fb[1], "suma final igual entre los dos SaguiOS (%s)" % fa[1])
        verificar(fa[4] == "-1" and fb[4] == "-1", "sin desincronización detectada")
        sm_a = dict(re.findall(r"red: suma turno (\d+): ([0-9a-f]{8})", sa))
        sm_b = dict(re.findall(r"red: suma turno (\d+): ([0-9a-f]{8})", sb))
        comunes = [t for t in sm_a if t in sm_b]
        verificar(len(comunes) >= TURNOS - 3, "%d turnos con suma en los dos" % len(comunes))
        verificar(all(sm_a[t] == sm_b[t] for t in comunes), "misma suma de sincronía en cada uno de los %d turnos (dos SaguiOS en QEMU)" % len(comunes))
        verificar("[AOEN] red-auto: sala encontrada: Anfitrion" in sb, "el cliente encontró la sala por difusión UDP")
        verificar("[AOEN] red: Cliente entra en la plaza 2" in sa, "el anfitrión vio entrar al cliente")
        verificar("DESINCRONIZACION" not in sa + sb, "sin registros de desincronización")
        verificar("EXCECAO" not in a.serial() + b.serial() and "PANICO" not in a.serial() + b.serial(), "sin excepciones ni pánicos")
        print("    QEMU: %d turnos, la misma suma en todos" % len(comunes))


def main():
    partes = sys.argv[1:] or ["pc", "fuentes", "qemu"]
    for p in partes:
        print("== %s" % p)
        {"pc": pc, "fuentes": fuentes, "qemu": qemu}[p]()
    print("h461: OK")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:  # noqa: BLE001
        print("FALHA: %s" % e)
        sys.exit(1)
