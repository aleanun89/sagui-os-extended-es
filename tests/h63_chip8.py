#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H63 - Emulador CHIP-8 / SUPER-CHIP / XO-CHIP e ensamblador chip8asm (D070-D072).

Critério:
 (a) o núcleo no host: as ROMs de prova passam nos três modos e cada quirk
     trocado faz falhar exatamente a prova que o mede;
 (b) chip8asm dentro do SaguiOS monta as fontes dos jogos e das provas (com
     include) com os mesmos bytes que o montador do host usou no build;
     erros com arquivo:linha em espanhol; o desensamblador (-d) dá o mesmo
     texto do host e montar de novo a saída reproduz a ROM;
 (c) "chip8 -t" no SaguiOS dá exatamente a saída do host para as três ROMs
     de prova (registros, pilha, soma e a tela em ASCII) e com quirks;
 (d) na janela: RAMAS abre, uma tecla começa o jogo, a tecla D segurada
     move o sagui (teclas mantidas), P pausa (a tela para), F2 e o menu do
     botão direito trocam a paleta (cor medida na tela), o menu troca o
     modo, F10 mostra o depurador, N executa um passo, F11 tela cheia;
 (e) o ícone da área de trabalho abre o seletor; Enter abre a primeira ROM
     (AURORA, XO-CHIP), que toca a melodia pelo alto-falante (frequências
     do FX3A, não o bip fixo); duplo clique numa .ch8 no Archivos abre o
     emulador.
"""
import hashlib
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, SAIDA, RAIZ  # noqa: E402

BORDA, TITULO = 2, 24
LINHA_A, SEP_A = 22, 9      # kernel/gui/menu.c
ESTADO = 22                 # barra de estado do chip8
HOST_ASM = os.path.join(RAIZ, "build", "host", "chip8asm")
HOST_EMU = os.path.join(RAIZ, "build", "host", "chip8")
B = os.path.join(RAIZ, "build", "chip8")

# (rom, quadros, provas, estado esperado)
PROVAS = [("prueba-chip8.ch8", 600, 33, "ejecutando"),
          ("prueba-schip.sc8", 300, 18, "terminado (00FD)"),
          ("prueba-xochip.xo8", 60, 16, "ejecutando")]
# quirk trocado -> (primeira prova que falha, quantas falhas)
QUIRKS = {
    "prueba-chip8.ch8": {"desplazar=vx": (14, 3), "memoria=fija": (17, 2), "salto=vx": (21, 1),
                         "borde=envuelve": (24, 2), "logicas=conserva": (7, 3)},
    "prueba-schip.sc8": {"desplazar=vy": (1, 4), "memoria=incrementa": (3, 2), "salto=v0": (4, 1),
                         "logicas=reinicia": (5, 1), "borde=envuelve": (14, 1)},
    "prueba-xochip.xo8": {"desplazar=vx": (11, 1), "memoria=fija": (12, 1), "salto=vx": (16, 1),
                          "logicas=reinicia": (13, 1), "borde=recorta": (4, 2)},
}
FONTES = [("/jogos/chip8/fuentes/ramas.asm", "jogos/ramas.ch8"),
          ("/jogos/chip8/fuentes/sumas.asm", "jogos/sumas.ch8"),
          ("/jogos/chip8/fuentes/canoa.asm", "jogos/canoa.sc8"),
          ("/jogos/chip8/fuentes/aurora.asm", "jogos/aurora.xo8"),
          ("/testes/chip8/prueba-schip.asm", "testes/prueba-schip.sc8")]


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]) + BORDA, y=int(g[4]) + TITULO,
                fx=int(g[3]), fy=int(g[4]))


def esperar(m, padrao, tempo=20, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


def limpo(saida):
    """Tira da saída do terminal as linhas do log do kernel que caem no meio
    dela na serial (o relógio, os processos, o bip): podem cortar uma linha
    do programa ao meio."""
    return re.sub(r"\[(?:GUI|SAGUI|BIP)\][^\n]*\n", "", saida.replace("\r", ""))


def host(*args):
    return subprocess.run([HOST_EMU, *args], capture_output=True, text=True).stdout


def registros(saida):
    v = re.search(r"^V:((?: [0-9a-f]{2}){16})$", saida, re.M)
    return [int(x, 16) for x in v.group(1).split()]


def bloco_t(saida, rom):
    """O relatório de "chip8 -t" dentro da saída do terminal: do cabeçalho
    até a última linha da tela."""
    linhas = limpo(saida).split("\n")
    ini = next(i for i, l in enumerate(linhas) if l.startswith(f"chip8: {rom} ("))
    alt = int(re.search(r"pantalla \d+x(\d+)", saida).group(1))
    fim = next(i for i in range(ini, len(linhas)) if linhas[i].startswith("pantalla ")) + alt + 1
    return "\n".join(linhas[ini:fim]) + "\n"


def tela(m):
    ppm = os.path.join(SAIDA, "h63-tela.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    return ler_ppm(ppm)


class Emulador:
    """Geometria da pantalla do CHIP-8 dentro da janela (como chip8.c calcula)."""
    def __init__(self, j, larg=64, alt=32, cheia=False):
        self.j = j
        al = j["l"]
        aa = j["a"] - (0 if cheia else ESTADO)
        self.e = max(1, min(al // larg, aa // alt))
        self.ox = j["x"] + (al - larg * self.e) // 2
        self.oy = j["y"] + (aa - alt * self.e) // 2
        self.larg, self.alt = larg, alt

    def cor(self, img, cx, cy):
        l, _, px = img
        x, y = self.ox + cx * self.e + self.e // 2, self.oy + cy * self.e + self.e // 2
        i = (y * l + x) * 3
        return tuple(px[i:i + 3])

    def cores(self, img):
        """As cores da pantalla da mais comum (o fundo) para a menos comum:
        vale em qualquer momento do jogo, até na tela de fim."""
        from collections import Counter
        c = Counter(self.cor(img, x, y) for y in range(self.alt) for x in range(self.larg))
        return [k for k, _ in c.most_common()]

    def soma(self, img):
        l, _, px = img
        h = hashlib.md5()
        for y in range(self.alt * self.e):
            i = ((self.oy + y) * l + self.ox) * 3
            h.update(px[i:i + self.larg * self.e * 3])
        return h.hexdigest()

    def x_sagui(self, img, fundo):
        """Média das colunas acesas nas linhas do sagui (25 a 30) em RAMAS."""
        xs = [x for y in range(25, 31) for x in range(64) if self.cor(img, x, y) != fundo]
        return sum(xs) / len(xs) if xs else -1


def main():
    # ---- (a) o núcleo no host
    for rom, quadros, provas, estado in PROVAS:
        out = host("-t", str(quadros), os.path.join(B, "testes", rom))
        v = registros(out)
        verificar(v[14] == provas and v[13] == 0 and v[11] == 0 and f"estado: {estado}" in out,
                  f"host: {rom}: {provas} provas, nenhuma falha, estado {estado}")
        for q, (primeira, falhas) in QUIRKS[rom].items():
            v = registros(host("-t", str(quadros), "-q", q, os.path.join(B, "testes", rom)))
            verificar(v[11] == primeira and v[13] == falhas,
                      f"host: {rom} com {q}: falha a prova {primeira} ({falhas} verificações)")
    out = host("-t", "300", "-m", "chip8", os.path.join(B, "testes", "prueba-schip.sc8"))
    verificar("Instrucción no válida: f030" in out, "host: FX30 (SCHIP) no modo CHIP-8 para com erro (00FF é 0NNN, ignorado)")

    with Maquina("h63", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("ayuda chip8")
        verificar("chip8 [-m chip8|schip|xochip]" in out, "ajuda chip8 explica o uso")

        # ---- (b) o montador dentro do SaguiOS
        for fonte, rom in FONTES:
            nome = os.path.basename(rom)
            out = m.comando(f"chip8asm {fonte} -o /tmp/{nome}", 60)
            tam = os.path.getsize(os.path.join(B, rom))
            verificar(f"{tam} bytes en /tmp/{nome}" in out, f"chip8asm monta {fonte} ({tam} bytes)")
            esperado = hashlib.sha256(open(os.path.join(B, rom), "rb").read()).hexdigest()
            out = m.comando(f"sha256sum /tmp/{nome}", 30)
            verificar(esperado in out, f"{nome} montado no SaguiOS = o do host (SHA-256)")
        m.comando('echo "  ld v0, bananas" > /tmp/mal.asm')
        m.comando('echo "  ldx v1, 2" >> /tmp/mal.asm')
        out = m.comando("chip8asm /tmp/mal.asm", 30)
        verificar("/tmp/mal.asm:1: símbolo no definido 'bananas'" in out and
                  "/tmp/mal.asm:2: instrucción desconocida 'ldx'" in out and "2 errores" in out,
                  "erros do montador com arquivo:linha em espanhol")
        out = m.comando("chip8asm -d /jogos/chip8/sumas.ch8 | cat", 60)
        des = subprocess.run([HOST_ASM, "-d", os.path.join(B, "jogos", "sumas.ch8")], capture_output=True,
                             text=True).stdout
        verificar(des.strip() in limpo(out), "chip8asm -d no SaguiOS = o do host")
        verificar("    ld v7, k                ; 21" in des or "ld v7, k" in des, "a desmontagem mostra as instruções")
        m.comando("chip8asm -d /jogos/chip8/sumas.ch8 -o /tmp/sumas-d.asm", 60)
        m.comando("chip8asm /tmp/sumas-d.asm -o /tmp/sumas-d.ch8", 60)
        esperado = hashlib.sha256(open(os.path.join(B, "jogos", "sumas.ch8"), "rb").read()).hexdigest()
        out = m.comando("sha256sum /tmp/sumas-d.ch8", 30)
        verificar(esperado in out, "desmontar e montar de novo reproduz a ROM")

        # ---- (c) chip8 -t no SaguiOS = no host
        for rom, quadros, provas, _ in PROVAS:
            out = m.comando(f"chip8 -t {quadros} /testes/chip8/{rom} | cat", 120)
            h = host("-t", str(quadros), os.path.join(B, "testes", rom))
            verificar(bloco_t(out, rom) == h, f"chip8 -t {quadros} {rom}: saída idêntica à do host ({provas} provas)")
        out = m.comando("chip8 -t 600 -q salto=vx /testes/chip8/prueba-chip8.ch8 | cat", 120)
        v = registros(bloco_t(out, "prueba-chip8.ch8"))
        verificar(v[11] == 21 and v[13] == 1, "chip8 -q salto=vx: falha só a prova do BNNN")
        out = m.comando("chip8 -t 60 /testes/chip8/prueba-xochip.xo8 | cat", 60)
        verificar("sonido 250 Hz" in out and "tono 64" in out, "XO-CHIP: padrão 00FF e tom 64 = 250 Hz")

        # ---- (d) a janela com RAMAS
        ini = len(m.serial())
        m.comando("chip8 /jogos/chip8/ramas.ch8")
        esperar(m, r"chip8: pronto \(/jogos/chip8/ramas.ch8\)", 30, ini)
        verificar("modo chip8, 11 instr/cuadro" in m.serial()[ini:], "RAMAS abre no modo CHIP-8 (pela extensão)")
        j = janela(m.serial(), "CHIP-8")
        emu = Emulador(j)
        time.sleep(4)
        m.captura("H63-chip8-ramas-titulo")
        titulo = emu.soma(tela(m))
        m.monitor("sendkey x 200")
        esperar(m, r"chip8: teclas 0001", 10, ini)
        time.sleep(1.5)   # o cenário é desenhado; ainda não deu tempo de perder vidas
        img = tela(m)
        verificar(emu.soma(img) != titulo, "uma tecla (X = 0 do CHIP-8) começa o jogo")
        cores = emu.cores(img)
        fundo = cores[0]
        verificar(cores[:2] == [(0x10, 0x20, 0x1A), (0x3C, 0xDC, 0x6E)], f"paleta SaguiOS: fundo e pixels verdes {cores[:2]}")
        antes = emu.x_sagui(img, fundo)
        marca = len(m.serial())
        m.monitor("sendkey d 1000")
        esperar(m, r"chip8: teclas 0200", 10, marca)
        esperar(m, r"chip8: teclas 0000", 10, marca)
        img = tela(m)
        depois = emu.x_sagui(img, fundo)
        verificar(depois > antes + 8, f"D segurada (tecla 9) move o sagui: x {antes:.1f} -> {depois:.1f}")
        verificar(True, "soltar a tecla aparece no mapa (teclas mantidas)")
        m.captura("H63-chip8-ramas-jogo")

        marca = len(m.serial())
        m.tecla("p")
        esperar(m, r"chip8: pausa, pantalla [0-9a-f]{8}", 10, marca)
        s1 = emu.soma(tela(m))
        time.sleep(1.5)
        s2 = emu.soma(tela(m))
        verificar(s1 == s2, "P pausa: a tela fica parada")
        m.tecla("n")
        esperar(m, r"chip8: paso, PC [0-9a-f]{3}", 10, marca)
        verificar(True, "N em pausa executa uma instrução")
        m.tecla("f10")
        esperar(m, r"chip8: depurador si", 10, marca)
        time.sleep(1)
        m.captura("H63-chip8-depurador")
        m.tecla("f10")
        esperar(m, r"chip8: depurador no", 10, marca)
        m.tecla("p")
        esperar(m, r"chip8: continua", 10, marca)
        m.monitor("sendkey x 200")   # se o jogo tinha acabado, recomeça; jogando, o 0 não faz nada
        time.sleep(2)
        verificar(emu.soma(tela(m)) != s2, "P de novo: o jogo continua")

        marca = len(m.serial())
        m.tecla("f2")
        esperar(m, r"chip8: paleta 1 ", 10, marca)
        time.sleep(0.8)
        img = tela(m)
        verificar(emu.cores(img)[:2] == [(0, 0, 0), (255, 255, 255)], "F2: paleta Clásica (preto e branco) medida na tela")
        # menu do botão direito: "Paleta Ámbar" (linha 11, depois de 2 separadores)
        m.clique(emu.ox + 200, emu.oy + 100, botao=2)
        g = esperar(m, r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", 10, marca)
        mx, my, ml = int(g[0]), int(g[1]), int(g[2])
        verificar(int(g[4]) == 18, "botão direito abre o menu do emulador (18 linhas)")
        m.clique(mx + ml // 2, my + 2 + 9 * LINHA_A + 2 * SEP_A + LINHA_A // 2)
        esperar(m, r"chip8: paleta 2 ", 10, marca)
        time.sleep(0.8)
        verificar(emu.cores(tela(m))[:2] == [(0x1C, 0x10, 0x00), (0xFF, 0xB0, 0x00)], "menu: paleta Ámbar medida na tela")
        m.captura("H63-chip8-ambar")

        marca = len(m.serial())
        m.tecla("f11")
        g = esperar(m, r"chip8: ventana (\d+)x(\d+) escala (\d+) \(pantalla completa\)", 10, marca)
        verificar(int(g[2]) == min(int(g[0]) // 64, int(g[1]) // 32), f"F11: tela cheia {g[0]}x{g[1]}, escala {g[2]}")
        time.sleep(1.5)
        m.captura("H63-chip8-pantalla-completa")
        m.tecla("f11")
        esperar(m, rf"chip8: ventana {j['l']}x{j['a']} escala 10", 10, marca)
        verificar(True, "F11 de novo volta à janela")

        marca = len(m.serial())
        m.clique(emu.ox + 200, emu.oy + 100, botao=2)
        g = esperar(m, r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", 10, marca)
        mx, my, ml = int(g[0]), int(g[1]), int(g[2])
        m.clique(mx + ml // 2, my + 2 + 6 * LINHA_A + 1 * SEP_A + LINHA_A // 2)   # "XO-CHIP"
        esperar(m, r"chip8: rom /jogos/chip8/ramas.ch8 \(888 bytes\) modo xochip, 1000 instr/cuadro", 10, marca)
        verificar(True, "menu: modo XO-CHIP reinicia a ROM com 1000 instr/cuadro e os quirks do XO-CHIP")
        m.tecla("bracket_right")   # o "+" no teclado ES
        esperar(m, r"chip8: velocidad 1250 instr/cuadro", 10, marca)
        verificar(True, "+ aumenta a velocidade")
        m.tecla("f5")
        esperar(m, r"chip8: reinicio", 10, marca)
        verificar(True, "F5 reinicia a ROM")
        m.clique(j["fx"] + j["l"] + 4 - 2 - 20 + 8, j["fy"] + 12)
        esperar(m, r"chip8: fechado", 10, marca)
        time.sleep(0.5)

        # ---- (e) ícone, seletor, AURORA com som e o Archivos
        marca = len(m.serial())
        m.clique(150, 40 + 3 * 84 + 30, duplo=True)
        esperar(m, r"\[GUI\] icone CHIP-8 -> /apps/bin/chip8", 15, marca)
        esperar(m, r"chip8: pronto \(sin rom\)", 20, marca)
        verificar(True, "o ícone da área de trabalho abre o emulador")
        time.sleep(1)
        m.captura("H63-chip8-selector")
        m.tecla("ret")
        esperar(m, r"chip8: rom /jogos/chip8/aurora.xo8 \(\d+ bytes\) modo xochip", 10, marca)
        verificar(True, "Enter no seletor abre a primeira ROM (AURORA, XO-CHIP pela extensão)")
        time.sleep(2)
        m.captura("H63-chip8-aurora-titulo")
        m.monitor("sendkey x 200")
        time.sleep(4)
        m.captura("H63-chip8-aurora")
        freqs = set(int(f) for f in re.findall(r"\[BIP\] (\d+) Hz 1000 ms", m.serial()[marca:]))
        verificar(len(freqs) >= 3 and 440 not in freqs, f"AURORA toca a melodia com o tom do XO-CHIP: {sorted(freqs)} Hz")
        a = janela(m.serial()[marca:], "CHIP-8")
        m.clique(a["fx"] + a["l"] + 4 - 2 - 20 + 8, a["fy"] + 12)
        esperar(m, r"chip8: fechado", 10, marca)
        time.sleep(0.5)

        marca = len(m.serial())
        m.comando("arquivos /jogos/chip8")
        esperar(m, r"arquivos: pronto", 20, marca)
        time.sleep(0.5)
        a = janela(m.serial(), "Archivos")
        nomes = ["fuentes", "aurora.xo8", "canoa.sc8", "ramas.ch8", "sumas.ch8"]   # pastas primeiro
        i = nomes.index("sumas.ch8")
        m.clique(a["x"] + 146 + 120, a["y"] + 98 + 2 + i * 20 + 8, duplo=True)
        esperar(m, r"chip8: rom /jogos/chip8/sumas.ch8 \(\d+ bytes\) modo chip8", 20, marca)
        verificar(True, "duplo clique numa .ch8 no Archivos abre o emulador")
        time.sleep(3)
        m.captura("H63-chip8-sumas")
    return 0


if __name__ == "__main__":
    sys.exit(main())
