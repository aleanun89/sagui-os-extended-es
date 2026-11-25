#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H73 - USB rápido: copiar do/para o pendrive sem congelar o sistema (D095).

Critério: com um pendrive FAT32 de prueba (mkfs.vfat + mcopy, como no H34):
  1. copia um arquivo de 256 KB e outro de ~8 MB do pendrive para /tmp e de
     volta para o pendrive, e confere o conteúdo com sha256sum (e, no fim,
     com o mtools no host);
  2. mede os KB/s de leitura e de gravação (relógio do host: do Enter ao
     próximo prompt) e os deixa em build/testes/h73-resultados.txt;
  3. enquanto a cópia grande roda, um processo em segundo plano (contador)
     continua avançando: o sistema não congela;
  4. a leitura passa de 2 MB/s (objetivo da melhoria 045, ponto 6).
SAGUI_H73_ANTES=1 só mede (sem exigir velocidade nem fluidez): serve para
guardar o "antes" de uma mudança.
"""
import hashlib
import os
import random
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_console, criar_pendrive, pendrive_qemu, mtools, SAIDA  # noqa: E402

PEQUENO = 256 * 1024
GRANDE = 8 * 1024 * 1024 + 12345          # não múltiplo de setor nem de cluster
SO_MEDIR = os.environ.get("SAGUI_H73_ANTES") == "1"
MIN_LEITURA_KBS = 2000 if os.environ.get("SAGUI_KVM") == "1" else 1000   # sem KVM o QEMU emula a CPU: o USB não é o limite


def conteudo(n, semente):
    r = random.Random(semente)
    return r.randbytes(n)


def cronometrar(m, comando, tempo=900):
    """Digita o comando e devolve (segundos até acabar, saída). O fim é uma
    marca que só aparece na saída (o eco do que se digita leva aspas no
    meio); o prompt não serve: o contador em segundo plano o redesenha."""
    antes = m.serial()
    m.digitar(comando + " ; echo FI''M73", 0.02)
    m.tecla("ret", 0.02)
    ini = time.time()
    while "\nFIM73" not in m.serial()[len(antes):].replace("\r", ""):
        if time.time() - ini > tempo:
            raise FalhaTeste(f"o comando {comando!r} não acabou em {tempo}s")
        if m.proc.poll() is not None:
            raise FalhaTeste("QEMU terminou")
        time.sleep(0.05)
    dt = time.time() - ini
    time.sleep(0.5)
    return dt, m.serial()[len(antes):].replace("\r", "")


def avancos_do_contador(serial):
    """Quantos segundos o contador em segundo plano já contou (maior número visto)."""
    n = [int(x) for x in re.findall(r"\[contador \d+\] (\d+) de 600", serial)]
    return max(n) if n else 0


def sha_no_sagui(m, caminho):
    _, out = cronometrar(m, f"sha256sum {caminho}", 900)
    r = re.search(r"([0-9a-f]{64})\s+" + re.escape(caminho) + r"\s", out)
    return r.group(1) if r else None


def robustez():
    """Cópia interrompida e pendrive arrancado no meio da cópia: o sistema continua de pé."""
    grande = conteudo(24 * 1024 * 1024, 3)
    sha = hashlib.sha256(grande).hexdigest()
    pen = criar_pendrive("h73-pen2", mb=96, rotulo="ROBUSTO", arquivos={"g24.bin": grande})
    # D468: con órdenes de 1 MB la copia de 24 MB acababa antes de poder escribir «matar»: el
    # pendrive de esta parte va frenado por el QEMU (8 MB/s), así la copia dura unos 3 s.
    freno = "throttling.bps-total=8000000"
    disco_lento = ["-drive", f"if=none,id=p1,format=raw,file={pen},{freno}",
                   "-device", "usb-storage,bus=ehci.0,drive=p1,removable=on,id=dev-p1"]
    with Maquina("h73-robustez", "ide", imagem=imagem_console(), memoria="768M",
                 extras=["-device", "usb-ehci,id=ehci"] + disco_lento) as m:
        m.esperar_serial("sagui:/$", 120)
        m.esperar_serial("[DISCO] usb0", 30)
        cronometrar(m, "montar usb0", 60)

        # 1. "matar" uma cópia que está dormindo no meio de uma leitura do pendrive: a trava e o
        #    anel do controlador são recuperados e a próxima cópia sai certa.
        for i in range(3):
            antes = len(m.serial())
            m.digitar(f"cp /usb/g24.bin /tmp/m{i}.bin &", 0.02)
            m.tecla("ret", 0.02)
            time.sleep(0.8 + 0.4 * i)
            pid = re.findall(r"cp -> processo (\d+)", m.serial()[antes:])[-1]
            cronometrar(m, f"matar {pid}", 60)
            time.sleep(0.5)
            novo = m.serial()[antes:]
            verificar("encerrado pelo comando matar" in novo, f"cópia {i} encerrada no meio com 'matar'")
            verificar("liberado duas vezes" not in novo, f"matar {pid}: nenhum bloco liberado duas vezes")
        _, out = cronometrar(m, "cp /usb/g24.bin /tmp/depois.bin")
        verificar("cp:" not in out, "depois de matar cópias, uma cópia nova termina sem erro")
        verificar(sha_no_sagui(m, "/tmp/depois.bin") == sha, "sha256 da cópia feita depois de matar as outras")

        # 2. Arrancar o pendrive no meio da cópia: a cópia falha, o volume sai e o USB volta a funcionar.
        m.digitar("cp /usb/g24.bin /tmp/arrancado.bin", 0.02)
        m.tecla("ret", 0.02)
        time.sleep(1.2)
        m.monitor("device_del dev-p1")
        m.esperar_serial("usb0 removido", 30)
        verificar("sin desmontarlo" in m.serial(), "arrancado no meio da cópia: o sistema avisa e continua")
        # O QEMU só solta o arquivo de imagem quando a remoção termina: tenta de novo até abrir.
        m.monitor("drive_del p1")
        for _ in range(20):
            time.sleep(0.5)
            if "Error" not in m.monitor(f"drive_add 0 if=none,id=p2,format=raw,file={pen}"):
                break
        m.monitor("device_add usb-storage,bus=ehci.0,drive=p2,id=dev-p2,removable=on")
        m.esperar_serial("[DISCO] usb1", 30)
        cronometrar(m, "montar usb1", 60)
        verificar(sha_no_sagui(m, "/usb/g24.bin") == sha, "pendrive recolocado: o arquivo continua íntegro")
        cronometrar(m, "desmontar usb1", 60)


def main():
    pequeno, grande = conteudo(PEQUENO, 1), conteudo(GRANDE, 2)
    sha = {"p": hashlib.sha256(pequeno).hexdigest(), "g": hashlib.sha256(grande).hexdigest()}
    pen = criar_pendrive("h73-pen", mb=96, rotulo="RAPIDO", arquivos={"p256.bin": pequeno, "g8.bin": grande})
    resultados = []

    with Maquina("h73", "ide", imagem=imagem_console(), memoria="768M",
                 extras=["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen, "p1")) as m:
        m.esperar_serial("sagui:/$", 120)
        m.esperar_serial("[DISCO] usb0", 30)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "pendrive montado em /usb")

        # ---------------- leitura: pendrive -> /tmp ----------------
        for nome, tam, chave in (("p256.bin", PEQUENO, "p"), ("g8.bin", GRANDE, "g")):
            dt, out = cronometrar(m, f"cp /usb/{nome} /tmp/{nome}")
            kbs = tam / 1024 / dt
            resultados.append(f"leitura {nome} ({tam // 1024} KB): {dt:.2f} s = {kbs:.0f} KB/s")
            print("  medida:", resultados[-1])
            verificar("cp:" not in out, f"cp /usb/{nome} /tmp/ sem erro")
            if chave == "g":
                leit_kbs = kbs
            verificar(sha_no_sagui(m, f"/tmp/{nome}") == sha[chave], f"sha256 de /tmp/{nome} igual ao original")

        # ---------------- o sistema não congela durante a cópia ----------------
        # Um contador em segundo plano imprime uma linha por segundo. Várias
        # cópias seguidas mantêm o USB ocupado por vários segundos; antes do
        # D095 a CPU ficava presa na espera do pendrive e o contador parava.
        m.comando("contador 600 &", tempo=10)
        time.sleep(1.5)
        antes_cont = avancos_do_contador(m.serial())
        dt_cont, out = cronometrar(m, "cp /usb/g8.bin /tmp/f1 ; cp /usb/g8.bin /tmp/f2 ; cp /usb/g8.bin /tmp/f3 ; "
                                      "cp /usb/g8.bin /tmp/f4 ; cp /usb/g8.bin /tmp/f5 ; cp /usb/g8.bin /tmp/f6")
        cont_leitura = (avancos_do_contador(m.serial()) - antes_cont, dt_cont)
        resultados.append(f"6 copias de 8 MB seguidas: {dt_cont:.1f} s; o contador avancou {cont_leitura[0]} vezes")
        print("  medida:", resultados[-1])
        verificar("cp:" not in out, "as cópias seguidas terminam sem erro")
        cronometrar(m, "rm /tmp/f1 /tmp/f2 /tmp/f3 /tmp/f4 /tmp/f5 /tmp/f6", 30)

        # ---------------- gravação: /tmp -> pendrive ----------------
        antes_cont = avancos_do_contador(m.serial())
        dt, out = cronometrar(m, "cp /tmp/g8.bin /usb/copia8.bin")
        no_meio_esc = avancos_do_contador(m.serial()) - antes_cont
        escr_kbs = GRANDE / 1024 / dt
        resultados.append(f"gravacao g8.bin ({GRANDE // 1024} KB): {dt:.2f} s = {escr_kbs:.0f} KB/s")
        print("  medida:", resultados[-1])
        verificar("cp:" not in out, "cp /tmp/g8.bin /usb/copia8.bin sem erro")
        dt, out = cronometrar(m, "cp /tmp/p256.bin /usb/copia256.bin")
        resultados.append(f"gravacao p256.bin ({PEQUENO // 1024} KB): {dt:.2f} s = {PEQUENO / 1024 / dt:.0f} KB/s")
        print("  medida:", resultados[-1])
        _, out = cronometrar(m, "desmontar usb0", 120)
        verificar("Ya puedes retirar el pendrive." in out, "desmontar grava tudo: " + out[-300:])
        cronometrar(m, "montar usb0", 60)
        verificar(sha_no_sagui(m, "/usb/copia8.bin") == sha["g"], "sha256 de /usb/copia8.bin (relido do pendrive) igual")
        verificar(sha_no_sagui(m, "/usb/copia256.bin") == sha["p"], "sha256 de /usb/copia256.bin (relido do pendrive) igual")

        print(f"  medida: o contador avançou {no_meio_esc} vezes durante a gravação de 8 MB ({GRANDE / 1024 / escr_kbs:.1f} s)")
        cronometrar(m, "desmontar usb0", 120)

    for nome, conteudo_ in (("copia8.bin", grande), ("copia256.bin", pequeno)):
        destino = os.path.join(SAIDA, "_h73_volta.bin")
        mtools(pen, "mcopy", "-o", f"p:/{nome}", destino)
        with open(destino, "rb") as f:
            verificar(hashlib.sha256(f.read()).hexdigest() == hashlib.sha256(conteudo_).hexdigest(),
                      f"mtools no host: {nome} idêntico ao original")

    os.makedirs(SAIDA, exist_ok=True)
    with open(os.path.join(SAIDA, "h73-resultados.txt"), "w") as f:
        f.write("\n".join(resultados) + "\n")
        f.write(f"contador durante a leitura grande: {cont_leitura[0]} avanços em {cont_leitura[1]:.1f} s\n")
        f.write(f"contador durante a gravação grande: {no_meio_esc} avanços\n")
    if SO_MEDIR:
        print("  (modo SAGUI_H73_ANTES: sem exigir velocidade)")
        return 0
    robustez()
    verificar(leit_kbs >= MIN_LEITURA_KBS, f"leitura >= {MIN_LEITURA_KBS} KB/s (foi {leit_kbs:.0f})")
    n, dt = cont_leitura
    verificar(n >= int(dt) - 2, f"o contador seguiu avançando durante a cópia ({n} avanços em {dt:.1f} s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
