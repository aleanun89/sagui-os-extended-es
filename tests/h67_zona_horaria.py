#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H67 (D080, D081) - zona horária: intérprete POSIX TZ, UTC no kernel, RTC em
UTC ou local, /etc/zona persistente, TZ, date, ls -l, barra e "Fecha y hora".

Parte 1 (no anfitrião, sem QEMU): compila o MESMO kernel/lib/fuso.c com o gcc
do anfitrião (tests/recursos/fuso_anfitriao.c) e compara, para milhares de
instantes, com dois oráculos independentes:
  - o zoneinfo do Python (base tz do anfitrião) para cada zona da tabela
    (userland/libsagui/zonas.c), incluindo os instantes em volta de cada
    mudança de horário e a conversão de hora local para UTC na hora repetida
    e na inexistente (fold=0);
  - o tzset() da glibc para cadeias POSIX feitas à mão (formas Jn, n,
    Mm.w.d, horas negativas e acima de 24, nomes <...>).

Parte 2 (QEMU, com -rtc base=...): RTC em UTC e em local, zona Europe/Madrid
no inverno e no verão, date/date -u, TZ=America/Sao_Paulo date, a barra (log
e captura, mudança de horário ao vivo, 12 horas), ls -l e as datas no ext2
(debugfs) e no FAT32 (mtools), persistência de /home/.zona depois de
reiniciar, a janela "Fecha y hora" (pelo botão direito no relógio), ntp
contra um servidor SNTP do anfitrião, e TLS com o RTC em hora local
(certificado de 40 minutos e o h59 inteiro).

Uso: h67_zona_horaria.py [--unitarias] [unitarias relogio persistencia tls]
"""
import datetime as dt
import os
import random
import re
import subprocess
import sys
import threading
import time
import zoneinfo

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import RAIZ, SAIDA, FalhaTeste, verificar, particao_dados_img  # noqa: E402

UTC = dt.timezone.utc
# A tabela tem a regra de HOJE de cada zona (sem histórico). A última mudança
# de regra entre as zonas da tabela é a de Vancouver (horário de verão
# permanente desde novembro de 2026); a comparação com a base tz começa depois.
ANO_INI = 2027


# ------------------------------------------------------------ parte 1: no anfitrião

def compilar():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "fuso_anfitriao")
    subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-Werror", "-DFUSO_ANFITRIAO", "-I", os.path.join(RAIZ, "kernel", "inc"),
                    "-o", exe, os.path.join(RAIZ, "tests", "recursos", "fuso_anfitriao.c"),
                    os.path.join(RAIZ, "kernel", "lib", "fuso.c")], check=True)
    return exe


def perguntar(exe, linhas):
    """Manda todas as perguntas de uma vez; devolve as respostas na ordem."""
    r = subprocess.run([exe], input="\n".join(linhas) + "\n", capture_output=True, text=True, check=True)
    saida = r.stdout.splitlines()
    verificar(len(saida) == len(linhas), f"o programa do anfitrião respondeu as {len(linhas)} perguntas")
    return saida


def tabela_zonas():
    src = open(os.path.join(RAIZ, "userland", "libsagui", "zonas.c"), encoding="utf-8").read()
    return re.findall(r'\{\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)"\s*\}', src)


def transicoes(z, ano):
    """Instantes UTC (inteiros) em que o desvio da zona muda no ano."""
    ini = int(dt.datetime(ano, 1, 1, tzinfo=UTC).timestamp())
    fim = int(dt.datetime(ano + 1, 1, 1, tzinfo=UTC).timestamp())
    achados = []

    def desvio(t):
        return dt.datetime.fromtimestamp(t, z).utcoffset()

    t, d = ini, desvio(ini)
    passo = 6 * 3600
    while t < fim:
        t2 = min(t + passo, fim)
        d2 = desvio(t2)
        if d2 != d:
            a, b = t, t2   # busca binária: desvio(a) == d, desvio(b) != d
            while b - a > 1:
                m = (a + b) // 2
                if desvio(m) == d:
                    a = m
                else:
                    b = m
            achados.append(b)
        t, d = t2, d2
    return achados


def esperado_local(z, t):
    l = dt.datetime.fromtimestamp(t, z)
    desvio = int(l.utcoffset().total_seconds())
    return (l.year, l.month, l.day, l.hour, l.minute, l.second, (l.isoweekday() % 7),
            l.timetuple().tm_yday - 1, desvio, l.tzname())


def comparar_zonas(exe):
    zonas = tabela_zonas()
    verificar(40 <= len(zonas) <= 100, f"a tabela tem {len(zonas)} zonas (40-100)")
    nomes = [n for n, _, _ in zonas]
    verificar(len(set(nomes)) == len(nomes), "sem nomes repetidos na tabela")
    for obrig in ("Europe/Madrid", "Atlantic/Canary", "Europe/Lisbon", "America/Sao_Paulo", "America/Mexico_City",
                  "America/Argentina/Buenos_Aires", "America/Santiago", "America/Bogota", "America/New_York",
                  "America/Los_Angeles", "Europe/London", "Europe/Berlin", "Asia/Kolkata", "Asia/Shanghai",
                  "Asia/Tokyo", "Australia/Sydney", "UTC"):
        verificar(obrig in nomes, f"a tabela tem {obrig}")
    disponiveis = zoneinfo.available_timezones()
    rnd = random.Random(67)
    perguntas, esperados, rotulos = [], [], []
    puladas = []
    for nome, _, tz in zonas:
        if nome not in disponiveis and nome != "UTC":
            puladas.append(nome)
            continue
        z = zoneinfo.ZoneInfo(nome)
        instantes = set()
        for ano in range(ANO_INI, 2038):
            for tt in transicoes(z, ano):
                for d in (-3601, -3600, -1801, -1, 0, 1, 1800, 3599, 3600):
                    instantes.add(tt + d)
        for _ in range(150):
            instantes.add(rnd.randrange(int(dt.datetime(ANO_INI, 1, 1, tzinfo=UTC).timestamp()),
                                        int(dt.datetime(2037, 12, 31, tzinfo=UTC).timestamp())))
        for t in sorted(instantes):
            perguntas.append(f"L {tz} {t}")
            esperados.append(esperado_local(z, t))
            rotulos.append((nome, t))
        # hora local -> UTC, inclusive na hora repetida e na inexistente (fold=0)
        for ano in range(ANO_INI, 2038):
            for tt in transicoes(z, ano):
                antes = int(dt.datetime.fromtimestamp(tt - 1, z).utcoffset().total_seconds())
                depois = int(dt.datetime.fromtimestamp(tt, z).utcoffset().total_seconds())
                parede = tt + antes   # hora de parede no instante da mudança (pelo relógio de antes)
                for d in (-5400, -3600, -1800, -1, 0, 1, 1800, 3599, 3600, 5400):
                    w = parede + d
                    ingenua = dt.datetime(1970, 1, 1) + dt.timedelta(seconds=w)
                    l = ingenua.replace(tzinfo=z, fold=0)
                    u = int(l.timestamp())
                    # situação: repetida se fold=1 dá outro instante; inexistente se a ida e volta não fecha
                    u1 = int(ingenua.replace(tzinfo=z, fold=1).timestamp())
                    volta = dt.datetime.fromtimestamp(u, z).replace(tzinfo=None)
                    sit = 2 if volta != ingenua else (1 if u1 != u else 0)
                    perguntas.append(f"U {tz} {w}")
                    esperados.append((u, sit))
                    rotulos.append((nome, f"local {ingenua} (mudança {antes}->{depois})"))
    respostas = perguntar(exe, perguntas)
    erros = []
    n_local = n_utc = 0
    for p, e, r, rot in zip(perguntas, esperados, respostas, rotulos):
        c = r.split()
        if p.startswith("L "):
            n_local += 1
            obtido = tuple(int(x) for x in c[:9]) + (c[10] if len(c) > 10 else "",)
            ok = obtido[:9] == e[:9] and obtido[9] == e[9].lstrip()   # abreviatura igual à da base tz
        else:
            n_utc += 1
            obtido = (int(c[0]), int(c[1]))
            ok = obtido == e
        if not ok:
            erros.append(f"{rot}: esperado {e}, obtido {obtido}")
    for e in erros[:15]:
        print("   ", e)
    por_zona = {}
    for e in erros:
        z = e.split("'")[1]
        por_zona[z] = por_zona.get(z, 0) + 1
    if por_zona:
        print("    erros por zona:", por_zona)
    if puladas:
        print(f"    (zonas que o anfitrião não conhece, puladas: {', '.join(puladas)})")
    verificar(not erros, f"fuso.c = zoneinfo em {n_local} instantes (UTC -> local) e {n_utc} horas locais "
                         f"(-> UTC, com hora repetida e inexistente) de {len(zonas) - len(puladas)} zonas, {ANO_INI}-2037")


def comparar_glibc(exe):
    """Cadeias feitas à mão contra o tzset() da glibc do anfitrião."""
    cadeias = [
        "CET-1CEST,M3.5.0,M10.5.0/3", "<-03>3", "<+0545>-5:45", "EST5EDT", "XXX3YYY,J60/2,J300/2",
        "XXX3YYY,59/2,299/2", "QQQ-2RRR,M3.4.4/26,M10.5.0",
        "ABC+5DEF+4,M4.1.0/-1,M10.5.0/+3", "<+1245>-12:45<+1345>,M9.5.0/2:45,M4.1.0/3:45",
        "NST3:30NDT,M3.2.0/0:01,M11.1.0/0:01", "<UTC+3>-3", "AEST-10AEDT,M10.1.0,M4.1.0/3", "MMM-1NNN-3,M2.5.6/167,M11.5.6/-167",
    ]
    rnd = random.Random(1997)
    perguntas, esperados = [], []
    t0 = int(dt.datetime(2020, 1, 1, tzinfo=UTC).timestamp())
    t1 = int(dt.datetime(2040, 1, 1, tzinfo=UTC).timestamp())
    tz_antes = os.environ.get("TZ")
    try:
        for c in cadeias:
            os.environ["TZ"] = c
            time.tzset()
            instantes = [rnd.randrange(t0, t1) for _ in range(400)]
            # e em volta de cada mudança (achada pelo próprio glibc)
            t, d = t0, time.localtime(t0).tm_gmtoff
            while t < t1:
                t2 = t + 6 * 3600
                d2 = time.localtime(t2).tm_gmtoff
                if d2 != d:
                    a, b = t, t2
                    while b - a > 1:
                        m = (a + b) // 2
                        if time.localtime(m).tm_gmtoff == d:
                            a = m
                        else:
                            b = m
                    instantes += [b - 1, b, b + 1]
                t, d = t2, d2
            for t in instantes:
                l = time.localtime(t)
                perguntas.append(f"L {c} {t}")
                esperados.append((l.tm_year, l.tm_mon, l.tm_mday, l.tm_hour, l.tm_min, l.tm_sec, (l.tm_wday + 1) % 7,
                                  l.tm_yday - 1, l.tm_gmtoff, l.tm_isdst, l.tm_zone))
    finally:
        if tz_antes is None:
            os.environ.pop("TZ", None)
        else:
            os.environ["TZ"] = tz_antes
        time.tzset()
    respostas = perguntar(exe, perguntas)
    erros = []
    for p, e, r in zip(perguntas, esperados, respostas):
        c = r.split()
        obtido = tuple(int(x) for x in c[:10]) + (c[10] if len(c) > 10 else "",)
        if obtido != e:
            erros.append(f"{p}: glibc {e}, fuso.c {obtido}")
    for e in erros[:15]:
        print("   ", e)
    por_cadeia = {}
    for e in erros:
        c = e.split()[1]
        por_cadeia[c] = por_cadeia.get(c, 0) + 1
    if por_cadeia:
        print("    erros por cadeia:", por_cadeia)
    verificar(not erros, f"fuso.c = tzset da glibc em {len(perguntas)} instantes de {len(cadeias)} cadeias "
                         "(Jn, n, Mm.w.d, horas negativas e >24, <...>)")
    # Horário de verão o ano todo (RFC 8536: começa em 1/1 às 0h e acaba em
    # 31/12 às 24h + a diferença). A glibc calcula as regras pelo ano UTC e
    # deixa umas horas de horário padrão na virada do ano; o fuso.c usa o ano
    # local e fica em verão o tempo todo, que é o que a regra quer dizer.
    perm = []
    for c in ("AAA-10BBB-11,J1/0,J365/25", "ZZZ0YYY,0/0,J365/25", "EST5EDT,0/0,J365/25"):
        perm += [f"L {c} {t}" for t in range(t0, t1, 86400 * 7 + 3607)]
    r = perguntar(exe, perm)
    fora = [p for p, x in zip(perm, r) if x.split()[9] != "1"]
    verificar(not fora, f"horário de verão permanente (J1/0,J365/25) em {len(perm)} instantes {fora[:3] or ''}")
    # Cadeias inválidas e válidas
    casos = {"CET-1CEST,M3.5.0,M10.5.0/3": "ok", "<-03>3": "ok", "UTC0": "ok", "EST5EDT": "ok",
             "": "inv", "CE": "inv", "CET": "inv", "CET-1CEST,M13.1.0,M10.5.0": "inv", "CET-1CEST,M3.6.0,M10.5.0": "inv",
             "<-03": "inv", "CET-1CEST,J0,J100": "inv", "CET-25": "inv", "CET-1CEST,M3.5.0": "inv",
             "CET-1CEST,M3.5.0,M10.5.0/168": "inv", ":Europe/Madrid": "inv", "CET-1CEST,M3.5.0,M10.5.0x": "inv"}
    r = perguntar(exe, [f"P {c}" if c else "P <>" for c in casos])
    erradas = [(c, e, o) for (c, e), o in zip(casos.items(), r) if e != o]
    verificar(not erradas, f"aceita as cadeias válidas e recusa as inválidas ({len(casos)} casos) {erradas or ''}")


def unitarias():
    exe = compilar()
    comparar_glibc(exe)
    comparar_zonas(exe)


# ------------------------------------------------------------ parte 2: no QEMU

RTC_INVERNO = "2026-01-15T10:00:00"        # 10:00 UTC = 11:00 CET em Madrid
UTC_INVERNO = int(dt.datetime(2026, 1, 15, 10, 0, tzinfo=UTC).timestamp())
LINHA_MENU = 22                            # kernel/gui/menu.c
DIREITO = 2


def esperar(m, padrao, desde=0, tempo=30):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise FalhaTeste(f"tempo esgotado esperando {padrao!r}")


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24, fx=int(g[2]), fy=int(g[3]))


def parte_relogio():
    """RTC do QEMU em UTC e depois em hora local; date, TZ, barra, datas de
    arquivos (RAM e FAT32), acerto manual, mudança de horário ao vivo e a
    janela "Fecha y hora"."""
    from qemu import Maquina, criar_pendrive, pendrive_qemu, mtools
    pen = criar_pendrive("h67-pen", rotulo="FECHAS")
    extras = ["-rtc", f"base={RTC_INVERNO}", "-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h67", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        verificar("[HORA] inicio: zona UTC (UTC0), rtc utc (auto), 2026-01-15 10:00" in m.serial(),
                  "no boot: zona UTC e RTC em UTC escolhido sozinho numa máquina virtual")
        out = m.comando("date")
        verificar(re.search(r"jueves, 15/01/2026 10:0\d:\d\d UTC", out), "date sem zona: a hora do RTC, em UTC")
        out = m.comando("zona")
        verificar("guarda la hora UTC (automático: máquina virtual)" in out, "zona: RTC em UTC (automático)")

        # --- Madrid, RTC em UTC
        out = m.comando("zona Europe/Madrid")
        # D371: com SAGUI-CONF a zona é da máquina e fica em /etc/zona (SAGUI-CONF), sem aviso
        verificar("Aviso: /home no es un disco persistente" in out or "gravada em /etc/zona (SAGUI-CONF)" in m.serial(),
                  "sem /home persistente: avisa que fica só na RAM (ou vai para SAGUI-CONF)")
        verificar("Próximo cambio:  29/03/2026, 02:00 CET -> 03:00 CEST" in out, "zona mostra o próximo cambio de horario")
        out = m.comando("date")
        verificar(re.search(r"jueves, 15/01/2026 11:0\d:\d\d CET", out), "date em Madrid no inverno: 11:0x CET")
        out = m.comando("date -u")
        verificar(re.search(r"15/01/2026 10:0\d:\d\d UTC", out), "date -u: 10:0x UTC")
        out = m.comando("date +%z_%Z_%s")
        g = re.search(r"\+0100_CET_(\d+)", out)
        verificar(g and abs(int(g.group(1)) - UTC_INVERNO) < 600, "date +%z_%Z_%s: +0100, CET e o instante UTC")
        out = m.comando("TZ=America/Sao_Paulo date")
        verificar(re.search(r"15/01/2026 07:0\d:\d\d -03", out), "TZ=America/Sao_Paulo date: 07:0x -03")
        out = m.comando("TZ='<+0545>-5:45' date +%H:%M_%Z")
        verificar(re.search(r"15:4\d_\+0545", out), "TZ com uma regra POSIX à mão (<+0545>-5:45)")
        g = esperar(m, r"\[GUI\] relogio: (jue 15/01/2026  11:0\d CET)")
        verificar(True, f"a barra mostra a hora de Madrid: {g}")

        # --- datas de arquivos: RAM e FAT32 (que guarda hora local)
        m.comando("echo hola > /tmp/fecha.txt")
        out = m.comando("ls -l /tmp")
        verificar(re.search(r"15/01/2026 11:0\d\s+5\s+fecha\.txt", out), "ls -l: data local do arquivo na RAM")
        out = m.comando("montar", tempo=30)
        m.comando("echo fat > /usb/fat.txt")
        out = m.comando("ls -l /usb")
        verificar(re.search(r"15/01/2026 11:0\d\s+4\s+fat\.txt", out), "ls -l no FAT32: 11:0x (a hora local volta igual)")
        out = m.comando("desmontar", tempo=30)
        lista, _ = mtools(pen, "mdir", "p:/")
        g = re.search(r"\s4\s+(\d{4}-\d\d-\d\d)\s+(\d\d:\d\d)\s+fat\.txt", lista)
        verificar(g and g.group(1) == "2026-01-15" and g.group(2).startswith("11:0"),
                  f"no disco FAT32 ficou a hora LOCAL de Madrid, como no Windows ({g and g.group(0)})")

        # --- RTC em hora local (PC com Windows): 10:00 no RTC = 10:00 CET = 09:00 UTC
        out = m.comando("zona --rtc local")
        verificar("guarda la hora LOCAL" in out, "zona --rtc local")
        out = m.comando("date")
        verificar(re.search(r"15/01/2026 10:0\d:\d\d CET", out), "com o RTC em hora local, date: 10:0x CET")
        out = m.comando("date -u")
        verificar(re.search(r"15/01/2026 09:0\d:\d\d UTC", out), "e date -u: 09:0x UTC")

        # --- acerto manual no verão (grava o RTC em hora local)
        out = m.comando("zona --ajustar '2026-07-15 12:00:00'")
        verificar("Hora ajustada: 15/07/2026 12:00:00 CEST" in out, "zona --ajustar no verão")
        out = m.comando("date")
        verificar(re.search(r"miércoles, 15/07/2026 12:00:\d\d CEST", out), "date no verão: CEST")
        out = m.comando("date -u")
        verificar(re.search(r"15/07/2026 10:00:\d\d UTC", out), "date -u no verão: duas horas a menos")
        out = m.comando("zona")
        verificar(re.search(r"guarda la hora LOCAL; ahora tiene 15/07/2026 12:00:\d\d", out), "o RTC foi gravado em hora local")
        esperar(m, r"\[GUI\] relogio: mié 15/07/2026  12:0\d CEST")
        verificar(True, "a barra passa para CEST")

        # --- hora inexistente e repetida; a mudança de horário ao vivo
        out = m.comando("zona --ajustar '2026-03-29 02:30:00'")
        verificar("29/03/2026 03:30:00 CEST (esa hora no existe ese día: se adelantó)" in out,
                  "hora inexistente (02:30 do dia da mudança): vira 03:30 CEST")
        k = len(m.serial())
        out = m.comando("zona --ajustar '2026-10-25 02:59:45'")
        verificar("25/10/2026 02:59:45 CEST (hora repetida: se tomó la primera)" in out,
                  "hora repetida (02:59 do fim do verão): fica a primeira, CEST")
        g = esperar(m, r"\[GUI\] relogio: (dom 25/10/2026  02:00 CET)", k, 40)
        verificar(True, f"sem reiniciar, a barra volta de 02:59 CEST para 02:00 CET: {g}")
        time.sleep(0.5)
        m.captura("H67-barra-cambio-horario")

        # --- botão direito no relógio: "Ajustar fecha y hora..." abre a janela
        m.comando("zona --ajustar '2026-01-15 11:00:00'")
        barra = re.findall(r"\[GUI\] barra (\d+): .*?relogio (\d+)", m.serial())[-1]
        k = len(m.serial())
        m.clique(int(barra[1]) + 40, 10, botao=DIREITO)
        menu = esperar(m, r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", k, 10)
        verificar("[GUI] relogio: menu de contexto" in m.serial()[k:] and menu[4] == "1",
                  "botão direito no relógio abre o menu (Ajustar fecha y hora...)")
        time.sleep(0.4)
        m.captura("H67-menu-reloj")
        m.clique(int(menu[0]) + int(menu[2]) // 2, int(menu[1]) + 2 + LINHA_MENU // 2)
        esperar(m, r"zona: ventana pronta \(zona Europe/Madrid, rtc local\)", k, 20)
        w = janela(m.serial(), "Fecha y hora")
        verificar(True, "a janela \"Fecha y hora\" abre")
        time.sleep(1.5)
        m.captura("H67-fecha-y-hora")
        m.clique(w["x"] + 90, w["y"] + 72 + 8)   # caixa de busca
        k = len(m.serial())
        m.digitar("sao")
        g = esperar(m, r'zona: busca "sao": (\d+) zonas', k, 10)
        verificar(g == "1", f"a busca filtra a lista (\"sao\": {g} zona)")
        time.sleep(0.6)
        m.captura("H67-fecha-y-hora-busca")
        m.tecla("ret")
        esperar(m, r"zona: aplicada America/Sao_Paulo", k, 10)
        verificar(True, "Enter aplica a zona escolhida")
        # Com o RTC em hora local, o RTC É a hora de parede da zona escolhida
        # (D080): a barra continua 11:00, agora "-03", e o UTC passa a 14:00.
        esperar(m, r"\[GUI\] relogio: jue 15/01/2026  11:0\d -03", k, 10)
        verificar("2026-01-15 14:00" in m.serial()[k:], "a barra muda na hora para São Paulo (RTC local reinterpretado: UTC 14:00)")
        time.sleep(1.2)
        m.captura("H67-fecha-y-hora-sao-paulo")
        m.clique(w["x"] + w["l"] - 16 - 55, w["y"] + w["a"] - 40 + 14)   # Cerrar
        esperar(m, r"processo \d+ \(zona\) terminou", k, 10)

        # --- 12 horas na barra
        t = janela(m.serial(), "Terminal")
        m.clique(t["fx"] + 60, t["fy"] + 10)
        time.sleep(0.4)
        k = len(m.serial())
        m.comando("zona --formato 12")
        g = esperar(m, r"\[GUI\] relogio: (jue 15/01/2026  11:\d\d a\. m\. -03)", k, 10)
        verificar(True, f"formato de 12 horas na barra: {g}")
        time.sleep(0.4)
        m.captura("H67-barra-12h")


def imagem_persistente(nome):
    b = os.path.join(RAIZ, "build")
    destino = os.path.join(SAIDA, nome + ".img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", os.path.join(b, "ramdisk.sgfs"), "--saida", destino, "--vesa", "--console",
                    "--dados-mb", "16", "--sin-conf"], check=True, stdout=subprocess.DEVNULL)
    return destino


def parte_persistencia():
    """/home/.zona sobrevive a um reinício; o ext2 grava UTC."""
    from qemu import Maquina, extrair_particao, debugfs
    img = imagem_persistente("h67-persistente")
    extras = ["-rtc", f"base={RTC_INVERNO}"]
    with Maquina("h67-pa", "ide", imagem=img, snapshot=False, extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("zona America/Sao_Paulo")
        verificar("Aviso" not in out, "com /home persistente não há aviso de \"solo en RAM\"")
        out = m.comando("zona --formato 12")
        out = m.comando("zona")
        verificar("Zona:            America/Sao_Paulo" in out, "zona America/Sao_Paulo")
        out = m.comando("cat /home/.zona")
        verificar("zona=America/Sao_Paulo" in out and "tz=<-03>3" in out and "formato=12" in out,
                  "o arquivo /home/.zona tem a zona, a regra e o formato")
        m.comando("echo hola > /home/fecha.txt")
        out = m.comando("ls -l /home")
        verificar(re.search(r"15/01/2026 07:0\d\s+5\s+fecha\.txt", out), "ls -l /home: hora local de São Paulo")
        m.digitar("reiniciar\n", 0.04)
        fim = time.time() + 60
        while m.proc.poll() is None and time.time() < fim:
            time.sleep(0.3)
        verificar(m.proc.poll() is not None, "reiniciar")
    with Maquina("h67-pb", "ide", imagem=img, snapshot=False, extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        verificar(re.search(r"\[HORA\] /home/\.zona: zona America/Sao_Paulo \(<-03>3\)", m.serial()),
                  "depois de reiniciar, o kernel lê /home/.zona")
        out = m.comando("zona")
        verificar("Configuración:   /home/.zona (disco persistente)" in out and "12 horas" in out,
                  "a zona e o formato de 12 horas continuam")
        out = m.comando("date")
        verificar(re.search(r"15/01/2026 07:0\d:\d\d -03", out), "date depois de reiniciar: São Paulo")
        m.comando("umount /home")
    # ext2: o mtime em UTC, qualquer que seja a zona
    ini = particao_dados_img(img)[0]
    part = extrair_particao(img, ini, 16)
    st = debugfs(part, "stat /fecha.txt").decode("latin-1")
    g = re.search(r"mtime: (0x[0-9a-f]+)", st)
    verificar(g and 0 <= int(g.group(1), 16) - UTC_INVERNO < 600,
              f"no ext2, o mtime de fecha.txt está em UTC ({g and int(g.group(1), 16)} ~ {UTC_INVERNO})")


def parte_tls():
    """TLS compara a validade em UTC: com o RTC em hora local (como um PC com
    Windows), um certificado válido só por ±20 minutos funciona depois de
    "zona --rtc local" e falha (caducado) se o kernel ler o RTC como UTC."""
    import h59_tls as h59
    import functools
    import http.server
    import ssl
    import threading
    from qemu import Maquina, imagem_console
    from cryptography.hazmat.primitives.asymmetric import rsa
    pasta = h59.PASTA
    os.makedirs(pasta, exist_ok=True)
    ca_k = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    ca = h59.emitir("CA de prueba H67", ca_k.public_key(), h59.nome("CA de prueba H67"), ca_k, ca=True)
    agora = dt.datetime.now(UTC)
    k = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    c = h59.emitir("servidor 40 minutos", k.public_key(), ca.subject, ca_k, ips=["10.0.2.2"],
                   inicio=agora - dt.timedelta(minutes=20), fim=agora + dt.timedelta(minutes=20))
    www = os.path.join(SAIDA, "h67-www")
    os.makedirs(www, exist_ok=True)
    h59.gravar(os.path.join(www, "ca.pem"), ca)
    h59.gravar(os.path.join(pasta, "h67-curto.pem"), c, k)
    with open(os.path.join(www, "hola.txt"), "w") as f:
        f.write("Hola por TLS con el RTC en hora local\n")
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(h59.Tratador, directory=www))
    srv.daemon_threads = True
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    p_http = srv.server_address[1]
    srvs = http.server.ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(h59.Tratador, directory=www))
    srvs.daemon_threads = True
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(os.path.join(pasta, "h67-curto.pem"))
    srvs.socket = ctx.wrap_socket(srvs.socket, server_side=True)
    threading.Thread(target=srvs.serve_forever, daemon=True).start()
    p_https = srvs.server_address[1]
    madrid = dt.datetime.now(zoneinfo.ZoneInfo("Europe/Madrid")).strftime("%Y-%m-%dT%H:%M:%S")
    try:
        with Maquina("h67-tls", "ide", imagem=imagem_console(), extras=["-rtc", f"base={madrid}"]) as m:
            m.esperar_serial("sagui:/$", 120)
            m.comando(f"curl -s -o /tmp/ca.pem http://10.0.2.2:{p_http}/ca.pem", tempo=30)
            m.comando("zona Europe/Madrid")
            out = m.comando(f"curl --cacert /tmp/ca.pem https://10.0.2.2:{p_https}/hola.txt", tempo=60)
            verificar("certificado caducado" in out and "Hola por TLS" not in out,
                      "RTC em hora local lido como UTC: o relógio adianta e o certificado de 40 min parece caducado")
            out = m.comando("zona --rtc local")
            out = m.comando(f"curl --cacert /tmp/ca.pem https://10.0.2.2:{p_https}/hola.txt", tempo=60)
            verificar("Hola por TLS con el RTC en hora local" in out,
                      "com zona --rtc local, o TLS recebe o UTC certo e o certificado de 40 min vale")
    finally:
        srv.shutdown()
        srvs.shutdown()
    # E o h59 inteiro (parte principal) com o RTC em hora local
    env = dict(os.environ, H59_RTC_LOCAL="Europe/Madrid")
    r = subprocess.run([sys.executable, os.path.join(RAIZ, "tests", "h59_tls.py"), "principal"], env=env,
                       capture_output=True, text=True, timeout=1500)
    falhas = [l for l in r.stdout.splitlines() if "FALHOU" in l or "Traceback" in l]
    verificar(r.returncode == 0 and not falhas, f"h59_tls (parte principal) passa com o RTC em hora local {falhas[:2] or ''}")


NTP_HORA = int(dt.datetime(2026, 5, 1, 12, 0, 0, tzinfo=UTC).timestamp())   # 14:00 CEST


def servidor_sntp(parar):
    """Um servidor SNTP mínimo no anfitrião (sem internet): responde sempre
    com NTP_HORA mais o tempo que passou desde que começou."""
    import socket
    import struct
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    s.settimeout(0.3)
    inicio = time.time()

    def rodar():
        while not parar.is_set():
            try:
                pedido, de = s.recvfrom(512)
            except OSError:
                continue
            if len(pedido) < 48 or (pedido[0] & 7) != 3:
                continue
            agora = NTP_HORA + (time.time() - inicio)
            seg, frac = int(agora) + 2208988800, int((agora % 1) * 2 ** 32)
            r = struct.pack("!BBbb11I", (4 << 3) | 4, 2, 6, -20, 0, 0, 0x7F000001,
                            seg, 0, struct.unpack("!I", pedido[40:44])[0], struct.unpack("!I", pedido[44:48])[0],
                            seg, frac, seg, frac)
            s.sendto(r, de)
        s.close()
    threading.Thread(target=rodar, daemon=True).start()
    return s.getsockname()[1]


def parte_ntp():
    """ntp (SNTP sobre UDP) contra um servidor no anfitrião: acerta o relógio
    do sistema e, com --rtc, o da placa."""
    from qemu import Maquina, imagem_console
    parar = threading.Event()
    porta = servidor_sntp(parar)
    try:
        with Maquina("h67-ntp", "ide", imagem=imagem_console(), extras=["-rtc", f"base={RTC_INVERNO}"]) as m:
            m.esperar_serial("sagui:/$", 120)
            m.comando("zona Europe/Madrid")
            out = m.comando(f"ntp -n 10.0.2.2:{porta}", tempo=30)
            verificar(re.search(r"Servidor 10\.0\.2\.2 \(estrato 2\): 01/05/2026 14:00:\d\d CEST, diferencia \+\d+ s", out),
                      "ntp -n mostra a hora do servidor e a diferença, sem acertar")
            out = m.comando("date")
            verificar("15/01/2026" in out, "com -n o relógio fica como estava")
            out = m.comando(f"ntp 10.0.2.2:{porta}", tempo=30)
            verificar("Hora del sistema ajustada (el reloj del PC no cambia" in out, "ntp acerta o relógio do sistema")
            out = m.comando("date")
            verificar(re.search(r"viernes, 01/05/2026 14:00:\d\d CEST", out), "date depois do ntp: 01/05/2026 14:00 CEST")
            out = m.comando("zona")
            verificar(re.search(r"ahora tiene 15/01/2026 10:0\d", out), "sem --rtc, o RTC continua com a hora antiga")
            out = m.comando(f"ntp --rtc 10.0.2.2:{porta}", tempo=30)
            verificar("guardada en el reloj del PC" in out, "ntp --rtc")
            out = m.comando("zona")
            verificar(re.search(r"guarda la hora UTC.*ahora tiene 01/05/2026 12:00", out), "com --rtc, o RTC fica em UTC: 12:00")
    finally:
        parar.set()


def main():
    args = sys.argv[1:]
    partes = [a for a in args if not a.startswith("--")] or ["unitarias", "relogio", "persistencia", "ntp", "tls"]
    if "--unitarias" in args:
        partes = ["unitarias"]
    if "unitarias" in partes:
        unitarias()
    if "relogio" in partes:
        parte_relogio()
    if "persistencia" in partes:
        parte_persistencia()
    if "ntp" in partes:
        parte_ntp()
    if "tls" in partes:
        parte_tls()
    print("H67 ok")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print(f"FALHOU: {e}")
        sys.exit(1)
