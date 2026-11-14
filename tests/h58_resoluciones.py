#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H58 - write-combining, resolución al arrancar y cambio de modo en caliente (mejora 026, D057-D059).

Critério:
  - o boot padrão continua em 1024x768 (captura 1024x768) e o log mostra o
    framebuffer em write-combining pela PAT (o qemu32 tem PAT);
  - "pantalla" lista os modos (1280x1024 e 800x600 entre eles), a resolução
    atual, a troca a quente e o write-combining;
  - com o Terminal maximizado e um programa em tela cheia, "pantalla
    1280x1024" troca a quente: captura 1280x1024, o programa em tela cheia
    recebe EV_TELA e passa a 1280x1024, o Terminal maximizado a 1276x974;
  - o mouse alcança a área nova (clique no botão restaurar em x > 1024);
  - "pantalla 800x600": captura 800x600, o Terminal é empurrado para dentro
    da tela e maximizado ocupa 796x550; de volta a 1024x768;
  - "pantalla 1280x1024 --guardar" grava no cabeçalho do disco de arranque;
    depois de reiniciar, o carregador já liga 1280x1024; "pantalla auto"
    apaga a preferência;
  - uma imagem com --sem-wc não liga o WC (e o rapido.c mede a placa nos
    dois casos: os números vão para a saída do teste).
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, cor_do_pixel, tamanho_png, imagem_de_teste, RAIZ, SAIDA,  # noqa: E402
                  IMAGEM)

BORDA, TITULO, BARRA = 2, 24, 24
FUNDO_TERMINAL = (0x12, 0x16, 0x1E)


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]), y=int(g[4]))


def botao(j, n):
    """Centro do n-ésimo botão da barra de título contando da direita (0 = X)."""
    return j["x"] + j["l"] + 2 * BORDA - BORDA - 20 * (n + 1) + 8, j["y"] + 12


def esperar(m, padrao, tempo=20, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


def preferencia_no_disco(img):
    """CAB_RES_LARG e CAB_RES_ALT: offset 128 do cabeçalho (LBA 1)."""
    with open(img, "rb") as f:
        f.seek(512 + 128)
        return struct.unpack("<HH", f.read(4))


def rapido_tela(serial):
    g = re.findall(r"tela rep (\d+) sse2 (\d+) nt (\d+)", serial)
    return tuple(int(v) for v in g[0]) if g else None


def troca_a_quente():
    with Maquina("h58", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        s = m.serial()
        verificar("[VIDEO] WC ativo via PAT" in s, "o framebuffer está em write-combining (PAT, entrada 1 = WC)")
        verificar("[VIDEO] 1024x768, 32 bits" in s, "sem preferência gravada, o boot continua em 1024x768")
        verificar("troca de modo a quente disponivel" in s, "a placa do QEMU (-vga std) tem o BGA (dispi)")
        png = m.captura("H58-1-1024x768")
        verificar(tamanho_png(png) == (1024, 768), "captura padrão em 1024x768")
        t = janela(s, "Terminal")

        out = m.comando("pantalla")
        # A lista é longa (com 15 e 16 bits, D078): as linhas do kernel na
        # serial ("[pantalla 5] ...", "[SAGUI] processo ... terminou") podem
        # cair no meio da cópia que o Terminal faz da saída. Tiram-se.
        out = re.sub(r"\[(?:pantalla \d+|SAGUI|GUI)\][^\n]*\n", "", out)
        verificar("Pantalla actual:      1024x768, 32 bits" in out, "pantalla mostra a resolução atual")
        verificar(re.search(r"\n\* 1024x768 +32 ", out) is not None, "1024x768 de 32 bits marcado como atual")
        for r in ("1280x1024", "800x600", "640x480", "1920x1080"):
            verificar(re.search(rf"\n  {r} +32 .*en caliente", out) is not None, f"pantalla lista {r} (troca a quente)")
        verificar("Write-combining:      activo (PAT)" in out, "pantalla mostra o write-combining ativo")
        verificar("Cambio en caliente:   sí" in out, "e que dá para trocar sem reiniciar")
        m.captura("H58-2-pantalla-lista")

        # ---- Terminal maximizado + programa em tela cheia, troca para 1280x1024
        m.clique(*botao(t, 1))
        esperar(m, r"terminal: janela 1020x718")
        verificar(True, "Terminal maximizado em 1024x768: 1020x718")
        marca = len(m.serial())
        m.comando("/testes/tela-cheia & contador 3; pantalla 1280x1024", tempo=60)
        esperar(m, r"\[VIDEO\] modo trocado: 1280x1024", desde=marca)
        verificar(True, "pantalla 1280x1024 trocou o modo a quente")
        esperar(m, r"\[MOUSE\] limites 1280x1024", desde=marca)
        verificar(True, "o mouse passou a 1280x1024")
        esperar(m, r"tela-cheia: tela nova 1280x1024", desde=marca)
        verificar(True, "o programa recebeu EV_TELA com a resolução nova")
        esperar(m, r"tela-cheia: tamanho 1280x1024 estado 2", desde=marca)
        verificar(True, "o programa em tela cheia passou a 1280x1024 (janela de 5 MB: cabe nos 8 MB por janela)")
        esperar(m, r"terminal: janela 1276x974,", desde=marca)
        verificar(True, "o Terminal maximizado passou a 1276x974")
        time.sleep(1)
        png = m.captura("H58-3-1280x1024-tela-cheia")
        verificar(tamanho_png(png) == (1280, 1024), "captura em 1280x1024")
        verificar(cor_do_pixel(png, 1279, 1023) == (0xFF, 0xFF, 0xFF) and cor_do_pixel(png, 900, 900) == (0x2A, 0x6F, 0xD8),
                  "a tela cheia ocupa os 1280x1024, 1:1 (moldura branca no canto)")
        m.tecla("q")
        m.esperar_serial("(tela-cheia) terminou", 10)
        time.sleep(1)
        png = m.captura("H58-4-1280x1024-maximizado")
        verificar(cor_do_pixel(png, 1270, 1015) == FUNDO_TERMINAL, "o Terminal maximizado chega ao canto de 1280x1024")
        verificar(cor_do_pixel(png, 640, 5) == (0x18, 0x20, 0x2A), "a barra superior cobre a largura nova")

        # ---- o mouse alcança a área nova: botão restaurar em x = 1246
        tm = dict(t, x=0, y=BARRA, l=1276, a=974)
        bx, by = botao(tm, 1)
        verificar(bx > 1024, f"o botão restaurar está fora da área antiga (x = {bx})")
        m.clique(bx, by)
        esperar(m, rf"janela {t['id']} restaurada")
        esperar(m, rf"terminal: janela {t['l']}x{t['a']},")
        verificar(True, f"clique em ({bx},{by}) restaura o Terminal ({t['l']}x{t['a']})")

        # ---- 800x600: a janela é empurrada para dentro; maximizar usa o tamanho novo
        marca = len(m.serial())
        out = m.comando("pantalla 800x600")
        verificar("Pantalla cambiada a 800x600." in out, "pantalla 800x600")
        g = esperar(m, rf"janela {t['id']} em \((\d+),(\d+)\) {t['l']}x{t['a']} depois da troca para 800x600", desde=marca)
        nx, ny = int(g[0]), int(g[1])
        verificar(nx + t["l"] + 2 * BORDA <= 800 and ny + t["a"] + TITULO + BORDA <= 600,
                  f"o Terminal foi recolocado dentro da tela, em ({nx},{ny})")
        time.sleep(1)
        png = m.captura("H58-5-800x600")
        verificar(tamanho_png(png) == (800, 600), "captura em 800x600")
        verificar(cor_do_pixel(png, nx + 20, ny + 100) == FUNDO_TERMINAL, "o Terminal aparece na posição nova")
        t8 = dict(t, x=nx, y=ny)
        m.clique(*botao(t8, 1))
        esperar(m, r"terminal: janela 796x550,", desde=marca)
        verificar(True, "maximizado em 800x600: 796x550 (o clique achou o botão na posição nova)")
        time.sleep(0.5)
        png = m.captura("H58-6-800x600-maximizado")
        verificar(cor_do_pixel(png, 795, 595) == FUNDO_TERMINAL, "o Terminal maximizado chega ao canto de 800x600")

        # ---- de volta a 1024x768
        marca = len(m.serial())
        m.comando("pantalla 1024x768")
        esperar(m, r"terminal: janela 1020x718,", desde=marca)
        verificar(True, "de volta a 1024x768, o Terminal maximizado volta a 1020x718")
        time.sleep(0.5)
        png = m.captura("h58-de-volta", guardar=False)
        verificar(tamanho_png(png) == (1024, 768), "captura em 1024x768 de novo")
        out = m.comando("pantalla 1366x768")
        verificar("no está en la lista" in out, "uma resolução fora da lista (1366 não é múltiplo de 8) é recusada")
        return rapido_tela(m.serial())


def preferencia_reiniciando():
    img = os.path.join(SAIDA, "h58-persistente.img")
    shutil.copy(IMAGEM, img)
    verificar(preferencia_no_disco(img) == (0, 0), "a imagem nova não tem resolução preferida")
    with Maquina("h58-a", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("pantalla 1280x1024 --guardar")
        verificar("Pantalla cambiada a 1280x1024." in out and "Guardado: el próximo arranque usará 1280x1024." in out,
                  "pantalla 1280x1024 --guardar troca agora e grava")
        # D179: se escribe en el LBA 1 del disco de arranque (hda); en la imagen,
        # la partición del SaguiOS (hda1) empieza justo ahí: es el mismo sector.
        verificar(re.search(r"preferencia 1280x1024 gravada em hda1?: 0", m.serial()), "gravou o cabeçalho no LBA 1 do disco de arranque (hda/hda1)")
        m.digitar("reiniciar\n", 0.04)
        fim = time.time() + 60
        while m.proc.poll() is None and time.time() < fim:
            time.sleep(0.3)
        verificar(m.proc.poll() is not None, "reiniciar: a máquina reiniciou")
    verificar(preferencia_no_disco(img) == (1280, 1024), "no disco: CAB_RES_LARG/ALT = 1280x1024")
    with Maquina("h58-b", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        s = m.serial()
        verificar("resolucao preferida no cabecalho: 1280x1024" in s, "o kernel recebeu a preferência do carregador")
        verificar("[VIDEO] 1280x1024, 32 bits" in s, "o carregador ligou 1280x1024 direto (VBE), sem troca a quente")
        verificar("modo trocado" not in s, "nenhuma troca a quente no boot")
        png = m.captura("H58-7-arranque-1280x1024")
        verificar(tamanho_png(png) == (1280, 1024), "captura do boot em 1280x1024")
        out = m.comando("pantalla")
        verificar("Preferida al arrancar: 1280x1024" in out, "pantalla mostra a preferida")
        verificar(re.search(r"\n\* 1280x1024 +32 .*actual preferida", out) is not None, "1280x1024 é a atual e a preferida")
        out = m.comando("pantalla auto")
        verificar("elegirá la resolución automáticamente" in out, "pantalla auto apaga a preferência")
    verificar(preferencia_no_disco(img) == (0, 0), "no disco: preferência zerada de novo")
    # Uma preferência que a BIOS não oferece: o carregador volta ao 1024x768.
    with open(img, "r+b") as f:
        f.seek(512 + 128)
        f.write(struct.pack("<HH", 1366, 768))
    with Maquina("h58-c", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        verificar("[VIDEO] 1024x768, 32 bits" in m.serial(), "preferência 1366x768 (fora da lista do VBE): volta a 1024x768")
    os.remove(img)


def sem_wc():
    img = imagem_de_teste("", ["--vesa", "--console", "--sem-wc"])
    with Maquina("h58-sem-wc", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("WC desligado pelo cabecalho de boot" in s and "WC inativo: desactivado" in s,
                  "com --sem-wc o framebuffer fica sem write-combining")
        return rapido_tela(s)


def main():
    com = troca_a_quente()
    preferencia_reiniciando()
    sem = sem_wc()
    # No QEMU sem KVM o tipo de memória não muda nada (a placa é emulada): os
    # números são só registro, a diferença de verdade só aparece no G470.
    print(f"  [medida] tela (ciclos/1024, rep/sse2/nt): com WC {com}, sem WC {sem}")
    print("H58: todos os critérios passaram")
    return 0


if __name__ == "__main__":
    sys.exit(main())
