#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Roda a bateria de testes com N provas ao mesmo tempo (D105).

Como o rodar_todos.py, mas em paralelo:
  - compila uma vez no começo (make nunca roda em paralelo);
  - cada prova tem a sua pasta de saída (variável SAGUI_SAIDA, lida por
    tests/qemu.py): build/paralelo/saida/<prova>/. Assim não colidem os
    .serial, .mon, .img, .wav, .ppm nem o mtoolsrc. As portas do anfitrião
    já são livres e escolhidas pelo sistema (bind a 0) nas provas de rede;
  - as provas da lista EXCLUSIVAS rodam sozinhas, no fim, sem outras ao mesmo tempo;
  - as provas de áudio (as que gravam o som do QEMU) nunca rodam duas ao mesmo tempo;
  - um log por prova: build/paralelo/<prova>.log.

Uso: tests/rodar_paralelo.py [-j N] [--kvm] [--repetir-falhas] [-k padrão]... [prefixo...]
O resumo final tem o mesmo formato do rodar_todos.py.
"""
import argparse
import glob
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import threading
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
PASTA = os.path.join(RAIZ, "build", "paralelo")
TEMPO_MAX = 900   # por prova (a serial usa 600; com várias em paralelo o TCG vai mais lento)

# Provas que não podem rodar com outras ao mesmo tempo, e o motivo.
EXCLUSIVAS = {
    "h01_ambiente.py": "roda make vazio",
    "h04_imagens.py": "roda make vdi: reescreve build/ (img/vdi/sha256)",
    "h65_compat_antiguos.py": "roda make iso: reescreve build/saguios.iso",
}


# Provas de áudio: gravam ou medem o som do QEMU. Pedido do mantenedor
# (2026-11-13): só uma de cada vez, nunca duas em paralelo; as outras provas
# podem rodar junto delas.
_AUDIO_RE = re.compile(r"audiodev|\.wav\b|dominante\(|capturar_audio|-audio")


def e_audio(nome):
    try:
        return _AUDIO_RE.search(open(os.path.join(AQUI, nome), encoding="utf-8").read()) is not None
    except OSError:
        return False


def listar(filtros, regexes):
    # D301: solo h<dos dígitos>*.py y b<dígito>_*.py; las pruebas de 86Box (emu_*) NUNCA entran en la batería.
    testes = sorted(glob.glob(os.path.join(AQUI, "h[0-9][0-9]*.py"))) + sorted(glob.glob(os.path.join(AQUI, "b[0-9]_*.py")))
    nomes = [os.path.basename(t) for t in testes]
    if filtros:
        nomes = [n for n in nomes if any(n.startswith(f.lower()) for f in filtros)]
    if regexes:
        nomes = [n for n in nomes if any(re.search(r, n) for r in regexes)]
    return nomes


def rodar_uma(nome, kvm, sufixo="", memoria="512M"):
    """Roda uma prova num processo próprio. Devolve (ok, segundos)."""
    base = nome[:-3] + sufixo
    saida = os.path.join(PASTA, "saida", base)
    shutil.rmtree(saida, ignore_errors=True)
    os.makedirs(saida)
    env = dict(os.environ, SAGUI_SAIDA=saida, PYTHONUNBUFFERED="1", SAGUI_MEM=memoria)
    env.pop("SAGUI_KVM", None)
    if kvm:
        env["SAGUI_KVM"] = "1"
    ini = time.time()
    with open(os.path.join(PASTA, base + ".log"), "w") as log:
        p = subprocess.Popen([sys.executable, os.path.join(AQUI, nome)], stdout=log, stderr=subprocess.STDOUT,
                             env=env, cwd=RAIZ, start_new_session=True)
        registrar(p)
        try:
            ok = p.wait(timeout=TEMPO_MAX) == 0
        except subprocess.TimeoutExpired:
            log.write("\nTEMPO ESGOTADO\n")
            matar(p)
            ok = False
    # A pasta de saída só ocupa espaço (imagens de dezenas de MB): fora se passou.
    if ok:
        shutil.rmtree(saida, ignore_errors=True)
    return ok, time.time() - ini


_vivos = set()
_trava = threading.Lock()


def registrar(p):
    with _trava:
        _vivos.add(p)


def matar(p):
    """Mata a prova e os QEMU dela (mesmo grupo de processos), por PID."""
    try:
        os.killpg(p.pid, signal.SIGKILL)
    except (ProcessLookupError, PermissionError):
        pass
    try:
        p.wait(timeout=10)
    except subprocess.TimeoutExpired:
        pass


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("prefixos", nargs="*", help="só as provas que começam por isto (como no rodar_todos.py)")
    ap.add_argument("-j", type=int, default=min(8, max(1, (os.cpu_count() or 2) // 2)), help="provas simultâneas")
    ap.add_argument("-k", action="append", default=[], metavar="PADRAO", help="só as provas cujo nome casa com a regex")
    ap.add_argument("--kvm", action="store_true", help="SAGUI_KVM=1 (rodar com: sg kvm -c ...)")
    ap.add_argument("--repetir-falhas", action="store_true", help="repete em série e sem KVM as que falharam")
    ap.add_argument("--sem-make", action="store_true", help="não compila no começo")
    ap.add_argument("--mem", default="512M",
                    help="memoria de cada QEMU (por omissão 512M: com várias em paralelo, "
                         "2 GB de cada vez carregam o PC; a máquina sozinha usa 2 GB)")
    a = ap.parse_args()

    nomes = listar(a.prefixos, a.k)
    if not nomes:
        print("nenhuma prova casa com o filtro")
        return 1
    if not a.sem_make and subprocess.run(["make", "-s", "-C", RAIZ]).returncode != 0:
        print("ERRO: o build falhou; nenhum teste rodou")
        return 1
    os.makedirs(PASTA, exist_ok=True)

    # Durações da vez anterior: as mais longas primeiro (menos cauda no final).
    arq_dur = os.path.join(PASTA, "duracoes.json")
    try:
        dur = json.load(open(arq_dur))
    except (OSError, ValueError):
        dur = {}
    comuns = sorted((n for n in nomes if n not in EXCLUSIVAS), key=lambda n: -dur.get(n, 120))
    exclusivas = [n for n in nomes if n in EXCLUSIVAS]

    total = len(nomes)
    feitas = [0]
    resultados = {}
    imprime = threading.Lock()
    ini_geral = time.time()

    def terminar(nome, ok, seg):
        with imprime:
            feitas[0] += 1
            resultados[nome] = ok
            dur[nome] = round(seg)
            print(f"[{feitas[0]}/{total}] {nome[:-3]} {'PASSOU' if ok else 'FALHOU'} {seg:.0f}s", flush=True)

    audio = {n for n in comuns if e_audio(n)}
    audio_ocupado = [False]

    def trabalhador(fila):
        while True:
            with imprime:
                if not fila:
                    return
                # A primeira da fila que puder rodar: uma de áudio só se não
                # houver outra de áudio em curso.
                i = next((k for k, n in enumerate(fila) if n not in audio or not audio_ocupado[0]), None)
                if i is not None:
                    nome = fila.pop(i)
                    if nome in audio:
                        audio_ocupado[0] = True
            if i is None:
                time.sleep(2)
                continue
            try:
                ok, seg = rodar_uma(nome, a.kvm, memoria=a.mem)
            finally:
                if nome in audio:
                    with imprime:
                        audio_ocupado[0] = False
            terminar(nome, ok, seg)

    print(f"{total} provas, {a.j} ao mesmo tempo" + (" (KVM)" if a.kvm else "") +
          f"; exclusivas no fim: {len(exclusivas)}; de áudio (uma de cada vez): {len(audio)}; logs em build/paralelo/", flush=True)
    try:
        fila = list(comuns)
        ths = [threading.Thread(target=trabalhador, args=(fila,)) for _ in range(max(1, a.j))]
        for t in ths:
            t.start()
        for t in ths:
            t.join()
        for nome in exclusivas:
            ok, seg = rodar_uma(nome, a.kvm, memoria=a.mem)
            terminar(nome, ok, seg)
    except KeyboardInterrupt:
        with _trava:
            for p in list(_vivos):
                matar(p)
        print("\ninterrompido")
        return 130
    try:
        json.dump(dur, open(arq_dur, "w"))
    except OSError:
        pass

    falhas = [n for n in nomes if not resultados.get(n)]
    if falhas and a.repetir_falhas:
        print(f"\nrepetindo em série e sem KVM: {', '.join(falhas)}", flush=True)
        ainda = []
        for nome in falhas:
            ok, seg = rodar_uma(nome, False, ".repeticao", memoria=a.mem)
            print(f"   {nome[:-3]} {'PASSOU' if ok else 'FALHOU'} na repetição {seg:.0f}s", flush=True)
            if not ok:
                ainda.append(nome)
        if len(ainda) < len(falhas):
            print("Passaram na repetição:", ", ".join(n for n in falhas if n not in ainda))
        falhas = ainda

    print(f"\ntempo total: {(time.time() - ini_geral) / 60:.1f} min")
    print(f"{total - len(falhas)}/{total} testes passaram")
    if falhas:
        print("Falharam:", ", ".join(falhas))
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main())
