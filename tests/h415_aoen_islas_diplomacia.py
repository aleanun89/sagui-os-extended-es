#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H415 - AoEN: islas que terminan, regla de caída y diplomacia con la máquina (D415).

Partes (por defecto: islas caida diplomacia fuentes; «compat» aparte porque tarda):
  islas       en el PC (tests/aoen_ia.c, sin ventana y a toda velocidad): partidas de IA contra IA en mapas de islas cuyos
              jugadores NO se alcanzan andando (el arnés lo dice: «mapa separadas=1»), con varias semillas y límite de 60
              minutos de juego. Terminan todas o casi todas (la difícil contra la fácil, y normal contra normal), sin
              unidades en terreno ajeno, y una semilla repetida da exactamente el mismo resultado (determinismo).
  caida       la regla de conquista (D415): sin edificios, un jugador con unidades cae a los 180 s y se rinde; con solo
              barcos de pesca, mercantes o transportes cae enseguida; si levanta un edificio, no cae.
  diplomacia  el humano (jugador 0) y la máquina: propuestas aceptadas o rechazadas según el caso (tributos, fuerza,
              traición), peticiones de tributo, la traición que se recuerda y las propuestas de la máquina al humano.
  fuentes     D415, la regla en game.c, el chat en la interfaz, la ayuda y docs/aoen.md lo cuentan.
  compat      h389 (ia, reglas, terreno), h398 (pc, determinismo-pc: el reparto en hilos no cambia la partida).
              h392 (TCC compila AoEN) se lanza aparte: python3 tests/h392_tcc.py
Variables: H415_PARTIDAS (semillas de la serie de islas, por defecto 6), H415_J (partidas a la vez, 4).
Nunca usa datos del juego original: todo es sintético.
"""
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))

# semillas de mapas de islas de 100x100 con dos jugadores que no se alcanzan andando (el arnés lo comprueba)
SEMILLAS_SEPARADAS = [2, 5, 6, 7, 8, 10, 13, 14, 18, 21]


def arnes():
    import h389_aoen
    return h389_aoen.compilar_arnes()


def partida(exe, semilla, da, db, minutos=60, mapa=0, tam=100, jug=2, **entorno):
    r = subprocess.run([exe, str(semilla), str(da), str(db), str(mapa), str(tam), str(minutos), str(jug)],
                       capture_output=True, text=True, timeout=3000, env=dict(os.environ, **entorno))
    return r.stdout


def serie(exe, semillas, da, db, **entorno):
    j = int(os.environ.get("H415_J", "4"))
    with ThreadPoolExecutor(max_workers=j) as ex:
        return list(ex.map(lambda s: (s, partida(exe, s, da, db, **entorno)), semillas))


def resumen(salida):
    g = re.search(r"resultado ganador=(\d) tiempo=(\d+) ganador_puntos=(\d) como=(\w+)", salida)
    tm = re.search(r"terreno malas=(\d+)", salida)
    sep = re.search(r"mapa separadas=(\d)", salida)
    return {"ganador": int(g.group(1)), "tiempo": int(g.group(2)), "como": g.group(4),
            "malas": int(tm.group(1)) if tm else -1, "separadas": int(sep.group(1)) if sep else -1}


def islas():
    from qemu import verificar
    exe = arnes()
    n = int(os.environ.get("H415_PARTIDAS", "6"))
    semillas = SEMILLAS_SEPARADAS[:n]
    # la difícil contra la fácil
    res = [(s, resumen(o)) for s, o in serie(exe, semillas, 2, 0)]
    for s, r in res:
        print("    semilla %d: ganador %d en %d s (%s), separadas=%d" % (s, r["ganador"], r["tiempo"], r["como"], r["separadas"]))
    verificar(all(r["separadas"] == 1 for s, r in res), "las %d semillas son mapas con los jugadores separados por agua o bosque" % n)
    verificar(all(r["malas"] == 0 for s, r in res), "ninguna unidad en terreno ajeno (barcos en tierra, tropa en el agua)")
    terminan = sum(1 for s, r in res if r["ganador"] == 1)
    verificar(terminan >= n - 1, "en islas la difícil gana a la fácil antes de 60 min (%d de %d)" % (terminan, n))
    conquista = sum(1 for s, r in res if r["ganador"] == 1 and r["como"] == "conquista")
    verificar(conquista >= (n + 1) // 2, "la mayoría por conquista, sin esperar a la Maravilla (%d de %d)" % (conquista, n))
    media = sum(r["tiempo"] for s, r in res if r["ganador"] == 1) // max(1, terminan)
    verificar(media < 50 * 60, "duración media de las que terminan: %d s (menos de 50 min)" % media)
    # determinismo: la misma semilla da el mismo final y la misma suma del mundo
    s = semillas[0]
    a = partida(exe, s, 2, 0, AOEN_IA_SUMA="1")
    b = partida(exe, s, 2, 0, AOEN_IA_SUMA="1")
    sa = re.findall(r"^suma (?:final )?t=\d+ [0-9a-f]{8}", a, re.M)
    sb = re.findall(r"^suma (?:final )?t=\d+ [0-9a-f]{8}", b, re.M)
    verificar(len(sa) >= 5 and sa == sb, "semilla %d repetida: las mismas %d sumas del mundo (determinista)" % (s, len(sa)))
    # normal contra normal en las mismas semillas: terminan casi todas (antes ninguna)
    res = [(s, resumen(o)) for s, o in serie(exe, semillas, 1, 1)]
    for s, r in res:
        print("    normal contra normal, semilla %d: ganador %d en %d s (%s)" % (s, r["ganador"], r["tiempo"], r["como"]))
    verificar(all(r["malas"] == 0 for s, r in res), "normal contra normal: ninguna unidad en terreno ajeno")
    terminan = sum(1 for s, r in res if r["ganador"] != 0)
    verificar(terminan * 3 >= n, "normal contra normal: al menos un tercio terminan antes de 60 min (%d de %d; antes ninguna)" % (terminan, n))


def caida():
    from qemu import verificar
    exe = arnes()

    def modo(m):
        o = subprocess.run([exe, "5", "1", "1", "1", "100", "10", "2"], capture_output=True, text=True, timeout=600,
                           env=dict(os.environ, AOEN_IA_CAIDA=str(m))).stdout
        g = re.search(r"caida: modo=\d unidades_antes=(\d+) unidades_despues=(\d+) cae_en=(-?\d+) vivo=(\d) fin=(\d)", o)
        return [int(x) for x in g.groups()]
    antes, despues, cae, vivo, fin = modo(1)
    verificar(antes >= 1 and cae == 183 and vivo == 0 and fin == 1 and despues == 0,
              "sin edificios y con unidades: cae a los 180 s (en %d s), sus %d unidades se rinden y la partida acaba" % (cae, antes))
    antes, despues, cae, vivo, fin = modo(2)
    verificar(0 <= cae <= 6 and vivo == 0 and fin == 1, "solo un barco de pesca: cae enseguida (en %d s)" % cae)
    antes, despues, cae, vivo, fin = modo(3)
    verificar(cae == -1 and vivo == 1 and fin == 0, "si levanta un edificio a los 100 s no cae")


def diplomacia():
    from qemu import verificar
    exe = arnes()
    o = subprocess.run([exe, "5", "1", "2", "0", "100", "60", "3"], capture_output=True, text=True, timeout=900,
                       env=dict(os.environ, AOEN_IA_DIPLO="1")).stdout
    paso = dict(re.findall(r"^diplo: (.+) -> (si|no)$", o, re.M))
    for k, v in paso.items():
        print("    %s: %s" % (k, v))
    verificar(re.search(r"personalidad jugador 1 = 1", o), "el jugador 1 es de personalidad equilibrada (la prueba lo fija)")
    verificar(paso["alianza sin tributos"] == "no", "la alianza sin haber enviado nada se rechaza")
    verificar(paso["tributo de 200 de oro"] == "si", "el tributo a un neutral se envía")
    verificar(paso["alianza con un solo tributo"] == "no", "un solo tributo no basta para la alianza")
    verificar(paso["alianza tras 800 en tributos"] == "si", "la alianza tras varios tributos se acepta")
    verificar(re.search(r"Tus regalos cuentan", o), "la máquina dice en el chat por qué acepta")
    verificar(re.search(r"Todavía no me fío: envíame un tributo", o), "la máquina dice en el chat por qué rechaza")
    verificar(paso["pide 200 de madera al aliado"] == "si" and re.search(r"madera del humano \+160", o),
              "un aliado con sobra envía el tributo pedido (llegan 160 de 200)")
    verificar(paso["pide 200 de comida al aliado sin comida"] == "no", "sin recursos de sobra no envía lo pedido")
    verificar(paso["declara la guerra al aliado (traición)"] == "si" and re.search(r"Traidor", o), "romper la alianza se puede, y la máquina lo toma por una traición")
    verificar(paso["paz justo después de traicionar"] == "no" and re.search(r"Me traicionaste hace poco", o),
              "tras la traición la máquina rechaza la paz y lo recuerda")
    verificar(paso["pide madera al traicionado"] == "no", "tampoco envía tributo a quien la traicionó")
    verificar(paso["paz con la traición reciente y el humano más fuerte"] == "no", "ni siquiera siendo el humano mucho más fuerte")
    verificar(re.search(r"No olvido tu traición", o), "la máquina se lo recuerda por el chat")
    verificar(re.search(r"propuesta pendiente del jugador 3, nivel Neutral", o), "una máquina sin agravios propone la paz al humano, que es mucho más fuerte")
    verificar(paso["el humano acepta la propuesta"] == "si" and re.search(r"relación con el jugador 3 = Neutral", o), "al aceptar, la relación cambia")


def fuentes():
    from qemu import verificar
    def leer(r):
        with open(os.path.join(RAIZ, r), encoding="utf-8") as f:
            return f.read()
    d = leer("docs/decisoes.md")
    verificar("## D415." in d, "D415 está en docs/decisoes.md")
    g = leer("userland/juegos/aoen/game.c")
    verificar("CAIDA_SIN_EDIFICIOS" in g and "reglas_caida" in g, "la regla de caída sin edificios está en game.c")
    verificar("void world_chat" in g and "cmd_pedir" in g and "cmd_responder_propuesta" in g, "el chat, las peticiones y las propuestas están en game.c")
    hud = leer("userland/juegos/aoen/ui_hud.c")
    verificar("W.chat_n" in hud, "la interfaz muestra el chat de la partida como avisos")
    pan = leer("userland/juegos/aoen/ui_paneles.c")
    verificar("Aceptar" in pan and "Modo: pedir" in pan, "la diplomacia (F3) tiene Aceptar/Rechazar y el modo pedir")
    ia = leer("userland/juegos/aoen/ia.c")
    verificar("sitio_orilla" in ia and "rescatar" in ia and "ia_responde" in ia, "la IA planifica el desembarco, rescata varados y responde a la diplomacia")
    verificar("Quién cae" in leer("rootfs/ayuda/aoen.md"), "la ayuda del juego cuenta la regla de caída y la diplomacia")
    verificar("## v8.5.5.3: islas" in leer("docs/aoen.md"), "docs/aoen.md tiene la sección de islas y diplomacia")


def compat():
    from qemu import verificar
    for args in (["tests/h389_aoen.py", "ia", "reglas", "terreno"], ["tests/h398_aoen_multinucleo.py", "pc", "determinismo-pc"]):
        r = subprocess.run([sys.executable] + args, cwd=RAIZ, capture_output=True, text=True, timeout=7200)
        verificar(r.returncode == 0, "%s %s pasa" % (os.path.basename(args[0]), " ".join(args[1:])))


if __name__ == "__main__":
    partes = sys.argv[1:] or ["islas", "caida", "diplomacia", "fuentes"]
    for p in partes:
        print("== " + p)
        globals()[p.replace("-", "_")]()
    print("H415 ok")
