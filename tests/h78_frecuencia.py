#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H78 (D106, mejora 061) - driver de frecuencia de la CPU con límite de temperatura.

Criterio:
  - en el anfitrión: la lógica de kernel/lib/frequencia_pol.c (compilada con
    -DSAGUI_HOST) con secuencias de temperatura y uso: políticas, límite de
    79 °C con vuelta por debajo de 72 °C, límite que se baja pero no se sube,
    sin temperatura no se pasa del estado de la BIOS (salvo forzado) y nunca
    más rápido que PStateCurLim;
  - en QEMU sin simulación: «control de frecuencia no disponible (máquina
    virtual)», sin errores; las órdenes de cambio fallan con código 1;
  - en QEMU con el driver simulado (mkimagem.py --frecuencia-simulada:
    P0 2100, P1 1600, P2 1050 MHz, la BIOS en P2): sube a P0 al arrancar; con
    79 °C baja a P2 y lo dice (log, «frecuencia», «sensores»); sigue limitada
    a 75 y a 72 °C y vuelve a P0 a 71 °C; el límite no pasa de 79 °C; sin
    temperatura se queda en el de la BIOS salvo «--forzar»; ahorro y a demanda;
    --desactivar vuelve al estado de la BIOS; --guardar; la escritura con #GP
    (MSR inexistente: #GP real con KVM, simulado sin KVM) desactiva el driver
    y el sistema sigue;
  - la opción de arranque (mkimagem.py --sin-frecuencia) deja la CPU como la
    dejó la BIOS;
  - Administrador de tareas, pestaña Rendimiento: «limitada por temperatura»
    junto al gráfico de frecuencia (captura docs/capturas/H78-tareas-limitada.png).
"""
import os
import re
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_console, imagem_de_teste, RAIZ, SAIDA  # noqa: E402

HARNESS = r"""
#include <stdio.h>
#include "frequencia_pol.h"
static fpol_t p;
static void passo(const char *rot, int ok, int t, unsigned uso) {
    int a = fpol_decidir(&p, ok, t, uso);
    printf("%s=%d,%d\n", rot, a, p.limitada);
}
int main(void) {
    /* Como el simulado: 3 estados, la BIOS en el 2 (el más lento). */
    fpol_iniciar(&p, 3, 2);
    passo("rend50", 1, 50000, 0);
    passo("t78", 1, 78999, 0);
    passo("t79", 1, 79000, 0);
    passo("t75", 1, 75000, 0);
    passo("t72", 1, 72000, 0);
    passo("t71", 1, 71999, 0);
    printf("vezes=%u\n", p.vezes_limitada);
    printf("lim85=%d lim80=%d lim30=%d lim70=%d\n", fpol_mudar_limite(&p, 85000), fpol_mudar_limite(&p, 79001),
           fpol_mudar_limite(&p, 30000), fpol_mudar_limite(&p, 70000));
    passo("l70t70", 1, 70000, 0);
    passo("l70t64", 1, 64000, 0);
    passo("l70t62", 1, 62999, 0);
    printf("lim79=%d\n", fpol_mudar_limite(&p, 79000));
    /* Sin temperatura: el de la BIOS; si estaba limitada, sigue limitada. */
    passo("nd", 0, 0, 0);
    passo("absurda", 1, 255875, 0);
    passo("cero", 1, 0, 0);
    p.forcado = 1;
    passo("forzado", 0, 0, 0);
    p.forcado = 0;
    passo("t80", 1, 80000, 0);
    passo("nd_limitada", 0, 0, 0);
    passo("t60", 1, 60000, 0);
    /* PStateCurLim: la plataforma no deja pasar del 1. */
    p.rapido = 1;
    passo("curlim", 1, 50000, 0);
    p.rapido = 0;
    /* Ahorro */
    p.politica = FPOL_ECONOMIA;
    passo("ahorro", 1, 50000, 100);
    /* A demanda: sube con 60 %; baja un estado tras 4 periodos por debajo del 25 %. */
    p.politica = FPOL_DEMANDA;
    passo("d90", 1, 50000, 90);
    passo("d10a", 1, 50000, 10);
    passo("d10b", 1, 50000, 10);
    passo("d10c", 1, 50000, 10);
    passo("d10d", 1, 50000, 10);
    passo("d40", 1, 50000, 40);
    passo("d10e", 1, 50000, 10);
    passo("d10f", 1, 50000, 10);
    passo("d10g", 1, 50000, 10);
    passo("d10h", 1, 50000, 10);
    passo("d10i", 1, 50000, 10);
    passo("d10j", 1, 50000, 10);
    passo("d10k", 1, 50000, 10);
    passo("d10l", 1, 50000, 10);
    passo("d60", 1, 50000, 60);
    passo("d60_79", 1, 79500, 100);
    /* Un solo estado y BIOS fuera de rango. */
    fpol_iniciar(&p, 1, 5);
    passo("uno", 1, 50000, 0);
    return 0;
}
"""

ESPERADO = {
    "rend50": "0,0", "t78": "0,0", "t79": "2,1", "t75": "2,1", "t72": "2,1", "t71": "0,0", "vezes": "1",
    "lim85": "0 lim80=0 lim30=0 lim70=1",
    "l70t70": "2,1", "l70t64": "2,1", "l70t62": "0,0", "lim79": "1",
    "nd": "2,0", "absurda": "2,0", "cero": "2,0", "forzado": "0,0", "t80": "2,1", "nd_limitada": "2,1", "t60": "0,0",
    "curlim": "1,0", "ahorro": "2,0",
    "d90": "0,0", "d10a": "0,0", "d10b": "0,0", "d10c": "0,0", "d10d": "1,0", "d40": "1,0",
    "d10e": "1,0", "d10f": "1,0", "d10g": "1,0", "d10h": "2,0", "d10i": "2,0", "d10j": "2,0", "d10k": "2,0", "d10l": "2,0",
    "d60": "0,0", "d60_79": "2,1", "uno": "0,0",
}


# D175: varias CPU. Tres estados (P0, P1, P2); el texto es el estado de cada
# CPU («x» = aún sin decidir) y si está limitada.
HARNESS_NUCLEOS = r"""
#include <stdio.h>
#include "frequencia_pol.h"
static fpol_t q;
static fpol_nucleo_t nu[4];
static int al[4];
static void m(const char *rot, int ok, int t, unsigned a, unsigned b, unsigned c, unsigned d, const int *dom, int fora) {
    unsigned u[4] = { a, b, c, d };
    fpol_decidir_nucleos(&q, nu, dom, 4, ok, t, fora ? NULL : u, al);
    printf("%s=", rot);
    for (int i = 0; i < 4; i++) { if (al[i] < 0) printf("x"); else printf("%d", al[i]); }
    printf(",%d\n", q.limitada);
}
static void bios(int a, int b, int c, int d) {
    fpol_nucleo_iniciar(&nu[0], a); fpol_nucleo_iniciar(&nu[1], b);
    fpol_nucleo_iniciar(&nu[2], c); fpol_nucleo_iniciar(&nu[3], d);
}
int main(void) {
    static const int smt[4] = { 0, 0, 1, 1 };   /* dos núcleos con dos hilos cada uno */
    fpol_iniciar(&q, 3, 2);
    q.politica = FPOL_DEMANDA;
    /* Cada CPU según su carga; la BIOS dejó la 1 en P1 y la 3 aún no se leyó. */
    bios(2, 1, 2, -1);
    m("m1", 1, 50000, 90, 5, 5, 90, NULL, 0);
    m("m2", 1, 50000, 90, 5, 5, 90, NULL, 0);
    m("m3", 1, 50000, 90, 5, 5, 90, NULL, 0);
    m("m4", 1, 50000, 90, 5, 5, 90, NULL, 0);
    nu[3].bios = 2;   /* ya leída: empieza en la de la BIOS */
    m("m5", 1, 50000, 90, 5, 5, 90, NULL, 0);
    /* El límite de temperatura manda sobre todas, con histéresis. */
    m("t80", 1, 80000, 90, 90, 90, 90, NULL, 0);
    m("t75", 1, 75000, 90, 90, 90, 90, NULL, 0);
    m("t71", 1, 71000, 90, 5, 5, 90, NULL, 0);
    /* Sin temperatura: ninguna pasa del estado en que la dejó la BIOS. */
    q.politica = FPOL_RENDIMENTO;
    m("nd", 0, 0, 90, 90, 90, 90, NULL, 0);
    m("rend", 1, 50000, 0, 0, 0, 0, NULL, 0);
    /* SMT: los dos hilos de un núcleo, la carga del más cargado y el mismo estado. */
    q.politica = FPOL_DEMANDA;
    bios(2, 2, 2, 2);
    m("s0", 1, 50000, 90, 5, 5, 5, smt, 0);
    m("s1", 1, 50000, 5, 5, 5, 70, smt, 0);
    m("s2", 1, 50000, 5, 5, 5, 70, smt, 0);
    m("s3", 1, 50000, 5, 5, 5, 70, smt, 0);
    m("s4", 1, 50000, 5, 5, 5, 70, smt, 0);
    m("s_t79", 1, 79000, 5, 5, 5, 70, smt, 0);
    m("s_t60", 1, 60000, 5, 5, 5, 70, smt, 0);
    /* SMT sin temperatura: el más lento de la BIOS de los dos hilos. */
    q.politica = FPOL_RENDIMENTO;
    bios(2, 1, 1, 1);
    m("s_nd", 0, 0, 90, 90, 90, 90, smt, 0);
    /* Paquete: todas el estado de la más cargada. */
    q.politica = FPOL_DEMANDA;
    q.coordenacao = FPOL_PAQUETE;
    bios(2, 2, 2, 2);
    m("p0", 1, 50000, 5, 5, 5, 95, NULL, 0);
    m("p_fora", 1, 50000, 0, 0, 0, 0, NULL, 1);
    m("p1", 1, 50000, 5, 5, 5, 5, NULL, 0);
    m("p2", 1, 50000, 5, 5, 5, 5, NULL, 0);
    m("p3", 1, 50000, 5, 5, 5, 5, NULL, 0);
    m("p4", 1, 50000, 5, 5, 5, 5, NULL, 0);
    /* Fuera de periodo (una llamada al sistema) no cuenta periodos tranquilos. */
    q.coordenacao = FPOL_POR_NUCLEO;
    bios(2, 2, 2, 2);
    m("f0", 1, 50000, 90, 90, 90, 90, NULL, 0);
    m("f1", 1, 50000, 5, 5, 5, 5, NULL, 0);
    m("f2", 1, 50000, 5, 5, 5, 5, NULL, 0);
    m("f3", 1, 50000, 5, 5, 5, 5, NULL, 0);
    m("f_fora1", 1, 50000, 0, 0, 0, 0, NULL, 1);
    m("f_fora2", 1, 50000, 0, 0, 0, 0, NULL, 1);
    m("f4", 1, 50000, 5, 5, 5, 5, NULL, 0);
    /* PStateCurLim: ninguna más rápida que la plataforma. */
    q.politica = FPOL_RENDIMENTO;
    q.rapido = 1;
    m("curlim", 1, 50000, 90, 90, 90, 90, NULL, 0);
    q.rapido = 0;
    /* Con una CPU, lo mismo que fpol_decidir en una secuencia larga. */
    {
        fpol_t a, b;
        fpol_nucleo_t n1;
        int igual = 1;
        unsigned x = 12345;
        fpol_iniciar(&a, 3, 2); fpol_iniciar(&b, 3, 2);
        fpol_nucleo_iniciar(&n1, 2);
        for (int k = 0; k < 2000; k++) {
            x = x * 1103515245u + 12345u;
            int t = 40000 + (int)((x >> 8) % 45000), ok = ((x >> 4) % 13) != 0;
            unsigned u = (x >> 16) % 101;
            if (k % 300 == 0) { a.politica = b.politica = (int)((x >> 3) % 3); }
            int ra = fpol_decidir(&a, ok, t, u), rb;
            fpol_decidir_nucleos(&b, &n1, NULL, 1, ok, t, &u, &rb);
            if (ra != rb || a.limitada != b.limitada) igual = 0;
        }
        printf("equiv=%d\n", igual);
    }
    return 0;
}
"""

ESPERADO_NUCLEOS = {
    "m1": "012x,0", "m2": "012x,0", "m3": "012x,0", "m4": "022x,0", "m5": "0220,0",
    "t80": "2222,1", "t75": "2222,1", "t71": "0000,0", "nd": "2122,0", "rend": "0000,0",
    "s0": "0022,0", "s1": "0000,0", "s2": "0000,0", "s3": "0000,0", "s4": "1100,0",
    "s_t79": "2222,1", "s_t60": "1100,0", "s_nd": "2211,0",
    "p0": "0000,0", "p_fora": "0000,0", "p1": "0000,0", "p2": "0000,0", "p3": "0000,0", "p4": "1111,0",
    "f0": "0000,0", "f1": "0000,0", "f2": "0000,0", "f3": "0000,0", "f_fora1": "0000,0", "f_fora2": "0000,0",
    "f4": "1111,0", "curlim": "1111,0", "equiv": "1",
}


def compilar_e_correr(fonte, nome):
    d = tempfile.mkdtemp(dir=SAIDA, prefix="h78-")
    c, exe = os.path.join(d, nome + ".c"), os.path.join(d, nome)
    with open(c, "w") as f:
        f.write(fonte)
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "kernel", "inc"), "-o",
                        exe, c, os.path.join(RAIZ, "kernel", "lib", "frequencia_pol.c")], capture_output=True, text=True)
    verificar(r.returncode == 0 and "warning" not in r.stderr, f"frequencia_pol.c compila en el anfitrión {r.stderr[:300]}")
    saida = subprocess.run([exe], capture_output=True, text=True, check=True).stdout
    return dict(l.split("=", 1) for l in saida.strip().splitlines())


def logica_nucleos_no_anfitriao():
    obtido = compilar_e_correr(HARNESS_NUCLEOS, "nucleos")
    erros = [f"{k}: {obtido.get(k)} != {v}" for k, v in ESPERADO_NUCLEOS.items() if obtido.get(k) != v]
    verificar(not erros, f"varias CPU en el anfitrión: por núcleo, SMT, paquete, temperatura, BIOS "
                         f"({len(ESPERADO_NUCLEOS)} pasos) {erros}")


def logica_no_anfitriao():
    d = tempfile.mkdtemp(dir=SAIDA, prefix="h78-")
    c, exe = os.path.join(d, "harness.c"), os.path.join(d, "harness")
    with open(c, "w") as f:
        f.write(HARNESS)
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "kernel", "inc"), "-o",
                        exe, c, os.path.join(RAIZ, "kernel", "lib", "frequencia_pol.c")], capture_output=True, text=True)
    verificar(r.returncode == 0 and "warning" not in r.stderr, f"frequencia_pol.c compila en el anfitrión {r.stderr[:300]}")
    saida = subprocess.run([exe], capture_output=True, text=True, check=True).stdout
    obtido = dict(l.split("=", 1) for l in saida.strip().splitlines())
    erros = [f"{k}: {obtido.get(k)} != {v}" for k, v in ESPERADO.items() if obtido.get(k) != v]
    verificar(not erros, f"políticas y límite de temperatura en el anfitrión ({len(ESPERADO)} pasos) {erros}")


def kvm():
    return os.environ.get("SAGUI_KVM") == "1"


def estado(m):
    """Ejecuta «frecuencia» y devuelve (salida, estado actual, limitada)."""
    out = m.comando("frecuencia", 30)
    g = re.search(r"frecuencia: estado (\d+) mec (\d+) pol (\d+) atual (-?\d+) bios (-?\d+) temp (-?\d+) limite (\d+) "
                  r"limitada (\d+)", out)
    verificar(g is not None, f"frecuencia registra su estado ({out[-300:]!r})")
    return out, int(g.group(4)), g.group(8) == "1"


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24)


def sem_simulacao():
    with Maquina("h78", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        verificar("[FREQ] control de frecuencia no disponible: m" in m.serial(),
                  "QEMU: el kernel registra «control de frecuencia no disponible»")
        out = m.comando("frecuencia", 30)
        verificar(re.search(r"^Control de frec\.:\s+no disponible \(máquina virtual", out, re.M) and
                  "terminou com codigo 0" in out, "frecuencia: no disponible (máquina virtual), código 0")
        out = m.comando("frecuencia ahorro", 30)
        verificar("no disponible en esta CPU" in out and "terminou com codigo 1" in out,
                  "frecuencia ahorro: no disponible, código 1")
        out = m.comando("sensores", 60)
        verificar(re.search(r"^Control de frec\.:\s+no disponible", out, re.M) and "terminou com codigo 0" in out,
                  "sensores muestra el control de frecuencia «no disponible»")
        verificar("EXCECAO" not in m.serial() and "[MSR] 0xc0010062" not in m.serial(),
                  "ningún error ni escritura de MSR en la máquina virtual")


def com_simulacao():
    img = imagem_de_teste("", ["--vesa", "--console", "--frecuencia-simulada"])
    with Maquina("h78s", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[FREQ] simulado (imagen de prueba, sin hardware); estados: P0 2100 MHz, P1 1600 MHz, P2 1050 MHz; "
                  "BIOS P2" in s, "simulado: tres estados, la BIOS en P2")
        verificar("[FREQ] P2 -> P0 (rendimiento, 50 C" in s, "rendimiento por defecto: sube a P0 al arrancar")
        out, a, lim = estado(m)
        verificar(a == 0 and not lim and "activo, política rendimiento" in out and "P0 2100 MHz (actual)" in out
                  and "P2 1050 MHz (BIOS)" in out, "frecuencia: activo, rendimiento, P0 actual")

        # Límite de 79 °C con histéresis hasta 72 °C.
        m.comando("frecuencia --simular-temp 78", 20)
        verificar(estado(m)[1] == 0, "78 °C: sigue en P0")
        m.comando("frecuencia --simular-temp 79", 20)
        out, a, lim = estado(m)
        verificar(a == 2 and lim and "LIMITADA POR TEMPERATURA" in out, "79 °C: baja a P2, «LIMITADA POR TEMPERATURA»")
        verificar("[FREQ] limitada por temperatura: 79 C >= limite 79 C" in m.serial(), "79 °C: lo dice el log del kernel")
        out = m.comando("sensores", 60)
        verificar("LIMITADA POR TEMPERATURA" in out, "sensores: «LIMITADA POR TEMPERATURA»")
        out = m.comando("sensores -c -n 1", 30)
        verificar("LIMITADA POR TEMPERATURA" in out, "sensores -c: indicador en la línea")
        for t in (75, 72):
            m.comando(f"frecuencia --simular-temp {t}", 20)
            a, lim = estado(m)[1:]
            verificar(a == 2 and lim, f"{t} °C: sigue limitada (histéresis)")
        m.comando("frecuencia --simular-temp 71", 20)
        a, lim = estado(m)[1:]
        verificar(a == 0 and not lim and "[FREQ] fin de la limitacion por temperatura: 71 C < 72 C" in m.serial(),
                  "71 °C: vuelve a P0")

        # El límite se baja pero no se sube.
        out = m.comando("frecuencia --limite 85", 20)
        verificar("no puede pasar de 79" in out and "terminou com codigo 1" in out, "--limite 85: rechazado")
        out = m.comando("frecuencia --limite 70", 20)
        verificar("Límite de temperatura: 70" in out, "--limite 70: aceptado")
        a, lim = estado(m)[1:]
        verificar(a == 2 and lim, "con límite 70 °C, 71 °C limita")
        m.comando("frecuencia --simular-temp 62", 20)
        verificar(estado(m)[1] == 0, "62 °C < 63 °C: vuelve a P0")
        m.comando("frecuencia --limite 79", 20)
        out = estado(m)[0]
        verificar(re.search(r"^Límite de temp\.:\s+79 °C", out, re.M), "--limite 79: vuelve al de serie")

        # Sin temperatura: el estado de la BIOS, salvo forzado.
        m.comando("frecuencia --simular-temp nd", 20)
        out, a, _ = estado(m)
        verificar(a == 2 and "sin temperatura: no pasa del estado de la BIOS" in out, "sin temperatura: P2 (BIOS)")
        out = m.comando("frecuencia --forzar", 20)
        verificar("AVISO" in out and estado(m)[1] == 0, "--forzar: sube sin temperatura (con aviso)")
        m.comando("frecuencia --no-forzar", 20)
        verificar(estado(m)[1] == 2, "--no-forzar: vuelve a P2")
        m.comando("frecuencia --simular-temp 50", 20)

        # Ahorro y a demanda.
        m.comando("frecuencia ahorro", 20)
        verificar(estado(m)[1] == 2, "ahorro: P2")
        m.comando("frecuencia --simular-uso 90", 20)
        m.comando("frecuencia demanda", 20)
        m.esperar_serial("(a demanda, 50 C, uso 90%", 10)
        verificar(estado(m)[1] == 0, "a demanda con 90 % de uso: P0")
        m.comando("frecuencia --simular-uso 5", 20)
        m.esperar_serial("[FREQ] P1 -> P2 (a demanda", 20)
        verificar("[FREQ] P0 -> P1 (a demanda" in m.serial() and estado(m)[1] == 2,
                  "a demanda en reposo: baja por escalones P0 -> P1 -> P2")
        m.comando("frecuencia --simular-uso real", 20)
        m.comando("frecuencia rendimiento", 20)
        verificar(estado(m)[1] == 0, "rendimiento: P0 otra vez")

        # Desactivar / activar / guardar.
        out = m.comando("frecuencia --desactivar", 20)
        out, a, _ = estado(m)
        verificar(a == 2 and "desactivado, política" in out and "[FREQ] desactivado (comando): la CPU queda en P2" in m.serial(),
                  "--desactivar: vuelve al estado de la BIOS (P2)")
        m.comando("frecuencia --simular-temp 85", 20)
        verificar(estado(m)[1] == 2, "desactivado: no escribe nada")
        m.comando("frecuencia --simular-temp 50", 20)
        m.comando("frecuencia --activar", 20)
        verificar(estado(m)[1] == 0, "--activar: P0")
        m.comando("frecuencia --limite 75", 20)
        out = m.comando("frecuencia --guardar", 20)
        verificar("no hay /home" in out and "terminou com codigo 1" in out, "--guardar sin /home: lo explica, código 1")
        m.comando("mkdir /home", 20)
        out = m.comando("frecuencia --guardar", 20)
        out = m.comando("cat /home/.frecuencia", 20)
        verificar("politica=rendimiento" in out and "limite=75" in out and "desactivar=no" in out,
                  "--guardar escribe /home/.frecuencia")
        m.captura("H78-frecuencia-texto")

        # Escritura con #GP: el driver se desactiva y el sistema sigue.
        out = m.comando("frecuencia --prueba-gp", 20)
        s = m.serial()
        if kvm():
            verificar("[FREQ] prueba: escritura en 0xbadf00d -> #GP absorbido" in s and
                      "[MSR] 0xbadf00d: #GP en la escritura" in s, "KVM: el wrmsr da #GP y se absorbe")
        else:
            verificar("[FREQ] prueba: escritura en 0xbadf00d -> " in s, "sin KVM: el wrmsr no da #GP; fallo simulado")
        out = estado(m)[0]
        verificar("desactivado por un fallo" in out and "[FREQ] #GP al escribir" in m.serial(),
                  "tras el #GP el driver queda desactivado")
        out = m.comando("frecuencia --activar", 20)
        verificar("no permitido" in out, "después de un #GP no se puede reactivar")
        out = m.comando("echo sigo vivo", 20)
        verificar("sigo vivo" in out and "EXCECAO" not in m.serial(), "el sistema sigue funcionando")

    img = imagem_de_teste("", ["--vesa", "--console", "--frecuencia-simulada", "--sin-frecuencia"])
    with Maquina("h78d", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        verificar("[FREQ] desactivado (opcion de arranque): la CPU queda en P2" in m.serial() and
                  "[FREQ] P2 -> P0" not in m.serial(), "opción de arranque: desactivado, no se toca la CPU")
        out, a, _ = estado(m)
        verificar(a == 2 and "desactivado" in out, "frecuencia: desactivado, P2")


def tareas():
    img = imagem_de_teste("", ["--vesa", "--frecuencia-simulada"])
    with Maquina("h78g", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("frecuencia --simular-temp 80")
        m.comando("tareas &")
        m.esperar_serial("tareas: pronto", 30)
        j = janela(m.serial(), "Administrador de tareas")
        m.clique(j["x"] + 112 + 58, j["y"] + 20)
        m.esperar_serial("tareas: pestana rendimiento", 15)
        fim = time.time() + 20
        while time.time() < fim and len(re.findall(r"tareas: frecuencia estado 1 atual 2 1050000 kHz limitada 1",
                                                     m.serial())) < 3:
            time.sleep(0.3)
        verificar(len(re.findall(r"tareas: frecuencia estado 1 atual 2 1050000 kHz limitada 1", m.serial())) >= 3,
                  "Rendimiento: frecuencia del driver (1050 MHz) y «limitada por temperatura»")
        time.sleep(0.5)
        m.captura("H78-tareas-limitada")
        # Botón «Política: ...» (abajo a la derecha): cada clic pasa a la siguiente.
        m.clique(j["x"] + j["l"] - 12 - 85, j["y"] + j["a"] - 40 + 14)
        m.esperar_serial("tareas: politica de frecuencia ahorro -> 0", 15)
        verificar("[FREQ] politica: ahorro" in m.serial(), "Rendimiento: el botón cambia la política (ahorro)")
        time.sleep(1.5)
        m.captura("H78-tareas-politica")
        verificar("EXCECAO" not in m.serial(), "ningún error del kernel")


def main():
    logica_no_anfitriao()
    logica_nucleos_no_anfitriao()
    if os.environ.get("H78_SO_ANFITRIAO") == "1":
        return 0
    sem_simulacao()
    com_simulacao()
    tareas()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
