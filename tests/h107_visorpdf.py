#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H107 - o visor de PDF (melhoria 059, D138).

Critério:
  - o Visor abre um PDF e diz quantas páginas tem, o título e o tamanho;
  - a página desenha-se mesmo: o PPM tem o retângulo azul no sítio certo, o
    texto sai com tinta e as margens ficam brancas;
  - um PDF com uma fonte TrueType incrustada abre-se e desenha-se;
  - um PDF cifrado não abre sem a palavra-passe e abre com ela;
  - na janela, o PDF aparece e as teclas (página seguinte, zoom)Respondem.

Os PDF de teste são gerados aqui (tests/pdf_exemplos.py), escritos à mão a
partir da norma ISO 32000-1.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, criar_pendrive, pendrive_qemu, mtools,  # noqa: E402
                  cor_do_pixel, SAIDA)
import pdf_exemplos  # noqa: E402


def ler_ppm(caminho):
    """Lê um PPM (P6) e devolve (largura, altura, pixels RGB)."""
    with open(caminho, "rb") as f:
        dados = f.read()
    if not dados.startswith(b"P6"):
        raise AssertionError("não é um PPM: %r" % dados[:8])
    # O cabeçalho: P6, largura, altura, 255 (com comentários e espaços).
    pos = 2
    campos = []
    while len(campos) < 3:
        while pos < len(dados) and dados[pos:pos + 1].isspace():
            pos += 1
        if dados[pos:pos + 1] == b"#":
            while pos < len(dados) and dados[pos:pos + 1] != b"\n":
                pos += 1
            continue
        ini = pos
        while pos < len(dados) and not dados[pos:pos + 1].isspace():
            pos += 1
        campos.append(int(dados[ini:pos]))
    pos += 1   # o espaço depois do 255
    larg, alt, _max = campos
    px = dados[pos:pos + larg * alt * 3]
    return larg, alt, px


def cor(px, larg, x, y):
    i = (y * larg + x) * 3
    return px[i], px[i + 1], px[i + 2]


def main():
    # ---- os PDF de teste
    destino = os.path.join(SAIDA, "h107-pdf")
    os.makedirs(destino, exist_ok=True)
    with open(os.path.join(destino, "simples.pdf"), "wb") as f:
        f.write(pdf_exemplos.simples())
    tem_fonte = pdf_exemplos.com_fonte()
    if tem_fonte:
        with open(os.path.join(destino, "fonte.pdf"), "wb") as f:
            f.write(tem_fonte)
    with open(os.path.join(destino, "cifrado.pdf"), "wb") as f:
        f.write(pdf_exemplos.cifrado("sagui"))
    with open(os.path.join(destino, "comprimido.pdf"), "wb") as f:
        f.write(pdf_exemplos.comprimido())
    with open(os.path.join(destino, "aes256.pdf"), "wb") as f:
        f.write(pdf_exemplos.cifrado_aes256("sagui"))

    arquivos = {
        "simples.pdf": open(os.path.join(destino, "simples.pdf"), "rb").read(),
        "cifrado.pdf": open(os.path.join(destino, "cifrado.pdf"), "rb").read(),
        "comprimido.pdf": open(os.path.join(destino, "comprimido.pdf"), "rb").read(),
        "aes256.pdf": open(os.path.join(destino, "aes256.pdf"), "rb").read(),
    }
    if tem_fonte:
        arquivos["fonte.pdf"] = tem_fonte

    pen = criar_pendrive("h107-pen", rotulo="PDF", arquivos=arquivos)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)

    with Maquina("h107", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "o pendrive com os PDF está montado")
        out = m.comando("ls /usb")
        for nome in ("simples.pdf", "cifrado.pdf", "comprimido.pdf", "aes256.pdf"):
            verificar(nome in out, f"ls /usb mostra {nome}")

        # ---- o PDF simples: quantas páginas, que título
        out = m.comando("visorpdf -t /usb/simples.pdf")
        verificar("paginas: 1" in out, "o PDF simples tem uma página")
        verificar("titulo: Documento de teste" in out, "o título do PDF simples é lido")
        verificar("pagina 1: 300 x 200" in out, "o tamanho da página é 300 x 200 pontos")

        # ---- o desenho: o PPM tem o retângulo, o texto e as margens brancas
        out = m.comando("visorpdf --ppm /usb/simples.pdf /usb/saida.ppm 1 100")
        verificar("ppm:" in out, "o PPM da página foi escrito")
        local = os.path.join(SAIDA, "h107-saida.ppm")
        mtools(pen, "mcopy", "-o", "p:/saida.ppm", local)
        verificar(os.path.exists(local), "o PPM saiu do pendrive para o anfitrião")
        larg, alt, px = ler_ppm(local)
        verificar(larg == 300 and alt == 200, f"o PPM tem o tamanho da página ({larg}x{alt})")
        r, g, b = cor(px, larg, 60, 80)
        verificar(b > 150 and r < 120 and g < 120, f"o retângulo azul está no sítio ({r},{g},{b})")
        r, g, b = cor(px, larg, 5, 5)
        verificar(r > 240 and g > 240 and b > 240, f"a margem de cima é branca ({r},{g},{b})")
        # O texto: na faixa de cima tem de haver pixels escuros.
        escuros = 0
        for y in range(20, 55):
            for x in range(15, 200):
                r, g, b = cor(px, larg, x, y)
                if r < 120 and g < 120 and b < 120:
                    escuros += 1
        verificar(escuros > 20, f"o texto se desenhou ({escuros} pixels escuros)")

        # ---- a fonte incrustada
        if tem_fonte:
            out = m.comando("visorpdf -t /usb/fonte.pdf")
            verificar("paginas: 1" in out, "o PDF com a fonte TrueType incrustada abre")

        # ---- o PDF cifrado
        out = m.comando("visorpdf -t /usb/cifrado.pdf")
        verificar("cifrado" in out, "sem a palavra-passe, o PDF cifrado não abre")
        out = m.comando("visorpdf -t /usb/cifrado.pdf sagui")
        verificar("paginas: 1" in out, "com a palavra-passe, o PDF cifrado abre")

        # ---- o PDF cifrado com AES-256 (V=5, revisão 6)
        out = m.comando("visorpdf -t /usb/aes256.pdf")
        verificar("cifrado" in out, "sem a palavra-passe, o PDF com AES-256 não abre")
        out = m.comando("visorpdf -t /usb/aes256.pdf sagui")
        verificar("paginas: 1" in out, "com a palavra-passe, o PDF com AES-256 abre")
        out = m.comando("visorpdf --ppm /usb/aes256.pdf /usb/aes.ppm 1 100 sagui")
        verificar("ppm:" in out, "o PPM do PDF com AES-256 foi escrito")
        local_aes = os.path.join(SAIDA, "h107-aes.ppm")
        mtools(pen, "mcopy", "-o", "p:/aes.ppm", local_aes)
        if os.path.exists(local_aes):
            larg, alt, px = ler_ppm(local_aes)
            escuros = 0
            for y in range(20, alt):
                for x in range(10, larg - 10):
                    r, g, b = cor(px, larg, x, y)
                    if r < 120 and g < 120 and b < 120:
                        escuros += 1
            verificar(escuros > 20, f"o texto do PDF com AES-256 se desenhou ({escuros} pixels)")

        # ---- o PDF com o conteúdo comprimido (FlateDecode)
        out = m.comando("visorpdf -t /usb/comprimido.pdf")
        verificar("paginas: 1" in out, "o PDF comprimido abre")
        out = m.comando("visorpdf --ppm /usb/comprimido.pdf /usb/comp.ppm 1 100")
        verificar("ppm:" in out, "o PPM do PDF comprimido foi escrito")
        local_comp = os.path.join(SAIDA, "h107-comp.ppm")
        mtools(pen, "mcopy", "-o", "p:/comp.ppm", local_comp)
        if os.path.exists(local_comp):
            larg, alt, px = ler_ppm(local_comp)
            r, g, b = cor(px, larg, 60, 80)
            verificar(b > 150 and r < 120 and g < 120,
                     f"o retângulo azul do PDF comprimido está no sítio ({r},{g},{b})")

        # ---- a janela (o programa fica em primeiro plano: nao se espera o prompt)
        m.digitar("visorpdf /usb/simples.pdf", 0.04)
        m.tecla("ret", 0.04)
        m.esperar_serial("visorpdf: pronto", 60)
        time.sleep(1)
        serial = m.serial()
        abrir = re.search(r'criada por \d+: "Visor de PDF" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)
        verificar(abrir is not None, "a janela do visor abriu")
        # O PDF esta desenhado: a pagina fica centrada na janela, com o papel
        # branco y el texto en negro.
        x0, y0 = int(abrir.group(3)) + 2, int(abrir.group(4)) + 24
        larg, alt = int(abrir.group(1)), int(abrir.group(2))
        area_pag = larg - 8, alt - 36 - 26
        captura = m.captura("h107-visor")
        # A margem de cima da pagina e branca; mas arriba esta a barra.
        verificar(cor_do_pixel(captura, x0 + 40, y0 + 36 + 30) == (255, 255, 255),
                 "a pagina aparece na janela (papel branco)")
        # Uma tecla: mudar o zoom, e a janela continua de pe.
        # Só se olha o que sai depois da tecla: antes ja ha "terminou com
        # codigo" das outras execuções do visorpdf.
        antes = len(m.serial())
        m.tecla("+")
        time.sleep(1.5)
        m.captura("h107-visor-zoom")
        depois = m.serial()[antes:]
        verificar("terminou com codigo" not in depois,
                 "o visor continua aberto depois de uma tecla")

    print("H107: tudo bem")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:  # noqa: BLE001
        print("H107 FALHOU:", e)
        sys.exit(1)
