#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H70 (D088-D090) - som: placas AC'97 e Intel HDA, misturador do kernel, reproducir e volumen.

Para cada placa (QEMU -device AC97 e -device intel-hda + hda-duplex), com a
saída de áudio num .wav do anfitrião (-audiodev wav, 48 kHz):
  - o boot acha a placa e o códec; volumen e infopc mostram a placa;
  - reproducir toca WAVs gerados aqui num pendrive: 48 kHz/16 bits/mono,
    8 kHz/8 bits/mono (conversão de taxa e de 8 bits), 44,1 kHz/16 bits/
    estéreo (canal esquerdo e direito diferentes) e 22,05 kHz; o .wav
    gravado tem cada tom na frequência certa, com a duração e o nível
    esperados (a 48 kHz e volume 100 o nível sai igual ao do arquivo);
  - "volumen 50" baixa o nível para ~1/4 (curva quadrática) e "volumen
    silencio" cala (o tom não aparece no .wav);
  - dois reproducir ao mesmo tempo (um com -v 50) saem misturados: as duas
    frequências no mesmo trecho, com a proporção de nível do volume;
  - matar um reproducir (kill) corta o som na hora (o trecho fica curto).
Sem placa: reproducir e volumen dizem que só há o alto-falante.
CHIP-8: a ROM AURORA (XO-CHIP) toca o padrão de áudio pela placa HDA.

A análise do .wav é em Python puro (Goertzel): trechos com som separados
por silêncio, frequência dominante em cada canal, duração e RMS.

Streaming: um WAV de 5 MB num pendrive começa a soar em menos de 2 s.

Uso: h70_sonido.py [ac97] [hda] [sem] [chip8] [grande]
"""
import array
import math
import re
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, criar_pendrive, pendrive_qemu  # noqa: E402

TAXA = 48000


# ------------------------------------------------------------ WAVs de prova

def gerar_wav(taxa, bits, canais, segundos, freqs, amp):
    """freqs: uma frequência por canal; amp em fração da escala (0-1)."""
    n = int(taxa * segundos)
    dados = bytearray()
    for i in range(n):
        for c in range(canais):
            v = amp * math.sin(2 * math.pi * freqs[c] * i / taxa)
            if bits == 16:
                dados += struct.pack("<h", int(round(v * 32767)))
            else:
                dados.append(int(round(128 + v * 127)))
    bq = canais * bits // 8
    cab = b"RIFF" + struct.pack("<I", 36 + len(dados)) + b"WAVE"
    cab += b"fmt " + struct.pack("<IHHIIHH", 16, 1, canais, taxa, taxa * bq, bq, bits)
    # um bloco desconhecido antes dos dados (como o LIST dos editores de áudio)
    cab += b"LIST" + struct.pack("<I", 4) + b"INFO"
    return cab + b"data" + struct.pack("<I", len(dados)) + bytes(dados)


ARQUIVOS = {
    # nome: (taxa, bits, canais, segundos, freqs, amp)
    "a440.wav": (48000, 16, 1, 1.0, [440], 0.5),
    "b1000.wav": (8000, 8, 1, 0.8, [1000], 0.6),
    "c_est.wav": (44100, 16, 2, 1.2, [600, 1500], 0.4),
    "d700.wav": (22050, 16, 1, 1.0, [700], 0.5),
    "e800.wav": (48000, 16, 1, 0.8, [800], 0.5),
    "f500.wav": (32000, 16, 1, 1.5, [500], 0.4),
    "g2000.wav": (32000, 16, 1, 1.5, [2000], 0.4),
    "h300.wav": (16000, 16, 1, 6.0, [300], 0.4),
}


# sagui.Sonido (D088): um tom de 1200 Hz gerado em Python.
TONO_PY = """import sagui, math, array
s = sagui.Sonido(16000)
a = array.array('h', (int(12000 * math.sin(2 * math.pi * 1200 * i / 16000)) for i in range(11200)))
s.escribir(a)
s.cerrar(True)
print('python: volumen', sagui.volumen())
"""


def rms_esperado(amp, bits=16):
    escala = 32767 if bits == 16 else 127 * 256
    return amp * escala / math.sqrt(2)


# ------------------------------------------------------------ análise do .wav gravado

def ler_gravacao(caminho):
    """O cabeçalho do .wav do QEMU só ganha os tamanhos se o QEMU fechar o
    áudio ao sair, o que nem sempre acontece com o "quit" do monitor: os
    tamanhos zerados valem "até o fim do arquivo"."""
    with open(caminho, "rb") as f:
        d = f.read()
    verificar(d[:4] == b"RIFF" and d[8:12] == b"WAVE", f"o QEMU gravou um .wav ({os.path.basename(caminho)})")
    p, fmt, dados = 12, None, None
    while p + 8 <= len(d):
        nome, tam = d[p:p + 4], struct.unpack("<I", d[p + 4:p + 8])[0]
        if nome == b"fmt ":
            fmt = struct.unpack("<HHIIHH", d[p + 8:p + 24])
        elif nome == b"data":
            dados = d[p + 8:p + 8 + tam] if tam else d[p + 8:]
            break
        p += 8 + tam + (tam & 1)
    verificar(fmt and fmt[1] == 2 and fmt[2] == TAXA and fmt[5] == 16,
              f"o .wav gravado é 48 kHz, 16 bits, estéreo ({len(dados or b'') // 4 / TAXA:.1f} s)")
    a = array.array("h")
    a.frombytes(dados[:len(dados) // 4 * 4])
    return a[0::2], a[1::2]


def rms(x):
    return math.sqrt(sum(v * v for v in x) / len(x)) if len(x) else 0.0


def trechos(esq, dir_, janela=480, limiar=200, folga=5, minimo=10):
    """Trechos com som: (início, fim) em amostras, juntando buracos de até
    `folga` janelas de 10 ms; descarta os menores que `minimo` janelas."""
    ativos = []
    for i in range(0, len(esq) - janela, janela):
        ativos.append(max(rms(esq[i:i + janela]), rms(dir_[i:i + janela])) > limiar)
    res, ini, buraco = [], None, 0
    for k, a in enumerate(ativos + [False] * (folga + 1)):
        if a:
            if ini is None:
                ini = k
            buraco = 0
        elif ini is not None:
            buraco += 1
            if buraco > folga:
                fim = k - buraco + 1
                if fim - ini >= minimo:
                    res.append((ini * janela, fim * janela))
                ini, buraco = None, 0
    return res


def goertzel(x, f):
    w = 2 * math.pi * f / TAXA
    c = 2 * math.cos(w)
    s1 = s2 = 0.0
    for v in x:
        s1, s2 = v + c * s1 - s2, s1
    return s1 * s1 + s2 * s2 - c * s1 * s2


def espectro(x, de=100, ate=4000, passo=20):
    return {f: goertzel(x, f) for f in range(de, ate + 1, passo)}


def dominante(x):
    """Frequência dominante: varredura de 20 em 20 Hz e depois fina, de 1 em 1."""
    x = x[:4800]
    e = espectro(x)
    f0 = max(e, key=e.get)
    fina = {f: goertzel(x, f) for f in range(f0 - 20, f0 + 21)}
    return max(fina, key=fina.get)


def picos(x, n=2):
    """As n frequências mais fortes (separadas por mais de 100 Hz)."""
    e = espectro(x[:4800])
    res = []
    for f in sorted(e, key=e.get, reverse=True):
        if all(abs(f - g) > 100 for g in res):
            res.append(f)
        if len(res) == n:
            break
    return sorted(res)


class Trecho:
    def __init__(self, esq, dir_, ini, fim):
        meio = (ini + fim) // 2
        self.ini, self.fim = ini, fim
        self.segundos = (fim - ini) / TAXA
        corte = slice(ini + (fim - ini) // 4, fim - (fim - ini) // 4)
        self.esq, self.dir = esq[corte], dir_[corte]
        self.f_esq, self.f_dir = dominante(self.esq), dominante(self.dir)
        self.rms_esq, self.rms_dir = rms(self.esq), rms(self.dir)
        self.meio = meio

    def __repr__(self):
        return (f"<{self.ini / TAXA:.2f}s {self.segundos:.2f}s E {self.f_esq} Hz {self.rms_esq:.0f} "
                f"D {self.f_dir} Hz {self.rms_dir:.0f}>")


def perto(a, b, tol):
    return abs(a - b) <= tol


# ------------------------------------------------------------ uma placa

def provar(placa, pen):
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina(f"h70-{placa}", "ide", imagem=imagem_console(), extras=extras, som=placa) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[AUDIO] " + ("ac97" if placa == "ac97" else "hda") + ":" in s, f"[{placa}] o kernel acha a placa e o códec")
        verificar("Sonido: " in s and "kHz" in s, f"[{placa}] a linha do som aparece no arranque")
        out = m.comando("volumen")
        verificar("Volumen: 100 %" in out and "Tarjeta:" in out, f"[{placa}] volumen mostra 100 % e a placa")
        out = m.comando("ayuda reproducir")
        verificar("reproducir [-v 0-100] archivo.wav" in out, f"[{placa}] ajuda reproducir explica o uso")
        out = m.comando("infopc")
        verificar("Sonido:" in out and ("driver ac97" in out or "driver hda" in out),
                  f"[{placa}] infopc mostra a placa de som")
        m.comando("montar usb0p1")
        out = m.comando("ls /usb")
        verificar("a440.wav" in out.lower(), f"[{placa}] os WAVs de prova estão no pendrive")
        # As provas de som tocam da RAM (/tmp): ler o pendrive no QEMU deixa as
        # interrupções desligadas ~70 ms por 4 KB e não acompanha um WAV de
        # 44,1 kHz estéreo (o streaming do pendrive tem a prova "grande").
        m.comando("mkdir /tmp/s")
        for nome in list(ARQUIVOS) + ["tono.py"]:
            m.comando(f"cp /usb/{nome} /tmp/s/{nome}", 120)

        out = m.comando("reproducir /tmp/s/a440.wav", 60)
        verificar("48000 Hz, 16 bits, mono, 1,0 s" in out, f"[{placa}] reproducir descreve o WAV de 48 kHz")
        m.comando("reproducir /tmp/s/b1000.wav", 60)
        m.comando("reproducir /tmp/s/c_est.wav", 60)
        m.comando("volumen 50")
        m.comando("reproducir /tmp/s/d700.wav", 60)
        m.comando("volumen silencio")
        m.comando("reproducir /tmp/s/e800.wav", 60)
        out = m.comando("volumen 100")
        verificar("Volumen: 100 %" in out, f"[{placa}] volumen 100 tira o silêncio")
        # Dois fluxos ao mesmo tempo: o primeiro em segundo plano.
        m.comando("reproducir /tmp/s/f500.wav & reproducir -v 50 /tmp/s/g2000.wav", 60)
        time.sleep(1.5)
        # kill no meio de um WAV de 6 s: o fluxo fecha com o processo e o som para na hora.
        antes = len(m.serial())
        m.comando("reproducir /tmp/s/h300.wav &")
        m.esperar_serial("16000 Hz, 1 canal", 30)
        pid = re.search(r"fluxo \d+ do processo (\d+): 16000 Hz", m.serial()[antes:]).group(1)
        time.sleep(2.0)
        m.comando(f"kill {pid}")
        time.sleep(1.0)
        out = m.comando("python /tmp/s/tono.py", 90)
        verificar("python: volumen 100" in out, f"[{placa}] sagui.Sonido toca um tom gerado em Python")
        out = m.comando("infopc -t sistema")
        irq = re.search(r"IRQ \d+ \((\d+) atendidas\)", out)
        verificar(irq and int(irq.group(1)) > 0, f"[{placa}] a placa interrompe ({irq.group(1) if irq else 0} IRQs atendidas)")
        s = m.serial()
        verificar("[AUDIO] tocando" in s, f"[{placa}] o DMA da placa liga com o primeiro fluxo")
        verificar("[AUDIO] fluxo 1 do processo" in s, f"[{placa}] dois fluxos abertos ao mesmo tempo")
        m.comando("infopc --informe > /tmp/i.txt")
    esq, dir_ = ler_gravacao(m.wav)
    tr = [Trecho(esq, dir_, a, b) for a, b in trechos(esq, dir_)]
    print(f"  [{placa}] trechos:", tr)

    # O AC'97 do QEMU aplica o volume PCM do códec de forma linear: 0x08
    # (0 dB pela especificação) vira 190/255 do nível. O HDA sai inalterado.
    k = 190 / 255 if placa == "ac97" else 1.0

    def achar(f, canal="esq"):
        return [t for t in tr if perto(getattr(t, "f_" + canal), f, 8)]

    a = achar(440)
    verificar(len(a) == 1 and perto(a[0].f_dir, 440, 8), f"[{placa}] 440 Hz (48 kHz mono) nos dois canais")
    a = a[0]
    verificar(perto(a.segundos, 1.0, 0.08), f"[{placa}] duração do tom de 1,0 s: {a.segundos:.3f} s")
    verificar(perto(a.rms_esq, k * rms_esperado(0.5), 150) and perto(a.rms_dir, k * rms_esperado(0.5), 150),
              f"[{placa}] nível igual ao do arquivo: RMS {a.rms_esq:.0f} (esperado {k * rms_esperado(0.5):.0f})")
    b = achar(1000)
    verificar(len(b) == 1, f"[{placa}] 1000 Hz do WAV de 8 kHz e 8 bits (conversão de taxa)")
    b = b[0]
    verificar(perto(b.segundos, 0.8, 0.08), f"[{placa}] duração de 0,8 s: {b.segundos:.3f} s")
    verificar(perto(b.rms_esq, k * rms_esperado(0.6, 8), 0.1 * k * rms_esperado(0.6, 8)),
              f"[{placa}] 8 bits viram 16: RMS {b.rms_esq:.0f} (esperado {k * rms_esperado(0.6, 8):.0f})")
    c = achar(600)
    verificar(len(c) == 1 and perto(c[0].f_dir, 1500, 8),
              f"[{placa}] estéreo de 44,1 kHz: 600 Hz à esquerda e 1500 Hz à direita")
    verificar(perto(c[0].segundos, 1.2, 0.08), f"[{placa}] duração de 1,2 s: {c[0].segundos:.3f} s")
    d = achar(700)
    verificar(len(d) == 1, f"[{placa}] 700 Hz do WAV de 22,05 kHz")
    razao = d[0].rms_esq / (k * rms_esperado(0.5))
    verificar(0.2 < razao < 0.3, f"[{placa}] volumen 50 dá ~1/4 do nível (curva quadrática): {razao:.3f}")
    verificar(not achar(800), f"[{placa}] com volumen silencio o tom de 800 Hz não sai")
    fg = [t for t in tr if perto(t.f_esq, 500, 8) or perto(t.f_esq, 2000, 8)]
    juntos = []
    for t in fg:
        x = esq[t.ini:t.fim]
        for i in range(0, len(x) - 4800, 4800):
            if picos(x[i:i + 4800]) == [500, 2000]:
                juntos.append(x[i:i + 4800])
    verificar(len(juntos) >= 5, f"[{placa}] mistura: 500 e 2000 Hz juntos no mesmo trecho ({len(juntos) / 10:.1f} s)")
    razao = sum(math.sqrt(goertzel(x, 2000) / goertzel(x, 500)) for x in juntos) / len(juntos)
    verificar(0.2 < razao < 0.3, f"[{placa}] mistura: o fluxo com -v 50 sai com ~1/4 da amplitude: {razao:.3f}")
    py = achar(1200)
    verificar(len(py) == 1 and perto(py[0].segundos, 0.7, 0.08),
              f"[{placa}] o tom do Python: 1200 Hz, {py[0].segundos if py else 0:.2f} s (esperado 0,70)")
    h = achar(300)
    verificar(len(h) == 1 and 1.0 < h[0].segundos < 3.5,
              f"[{placa}] kill corta o WAV de 6 s ({h[0].segundos if h else 0:.2f} s tocados)")


def sem_placa():
    with Maquina("h70-sem", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        verificar("Sonido:" not in m.serial(), "sem placa: nenhuma linha de som no arranque")
        out = m.comando("volumen")
        verificar("no hay tarjeta de sonido" in out, "sem placa: volumen avisa")
        out = m.comando("reproducir /nada.wav")
        verificar("reproducir: /nada.wav:" in out, "reproducir avisa do arquivo que não existe")


def chip8():
    """AURORA (XO-CHIP) na área de trabalho com a placa HDA: o padrão de
    áudio sai pela placa (vários tons diferentes no .wav) e não pelo bip."""
    with Maquina("h70-chip8", "ide", som="hda") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        marca = len(m.serial())
        m.digitar("chip8 /jogos/chip8/aurora.xo8\n", 0.04)
        m.esperar_serial("chip8: rom /jogos/chip8/aurora.xo8", 30)
        verificar("chip8: som pela placa" in m.serial()[marca:], "chip8 abre um fluxo na placa de som")
        time.sleep(2)
        m.monitor("sendkey x 200")
        time.sleep(5)
        verificar("[BIP]" not in m.serial()[marca:], "com placa, o CHIP-8 não usa o alto-falante")
    esq, dir_ = ler_gravacao(m.wav)
    freqs = set()
    for a, b in trechos(esq, dir_, folga=2, minimo=3):
        for i in range(a, b - 2400, 2400):
            if rms(esq[i:i + 2400]) > 1000:
                freqs.add(round(dominante(esq[i:i + 2400]) / 10) * 10)
    print("  frequências do AURORA:", sorted(freqs))
    verificar(len(freqs) >= 3, f"AURORA toca a melodia do padrão XO-CHIP pela placa ({len(freqs)} tons)")


def gerar_wav_rapido(taxa, canais, segundos, freq, amp):
    """Como gerar_wav (16 bits), mas rápido o bastante para dezenas de segundos."""
    n = int(taxa * segundos)
    periodo = [int(round(amp * 32767 * math.sin(2 * math.pi * freq * i / taxa))) for i in range(taxa)]
    a = array.array("h")
    for i in range(n):
        v = periodo[i % taxa]
        a.append(v)
        if canais == 2:
            a.append(v)
    dados = a.tobytes()
    bq = canais * 2
    cab = b"RIFF" + struct.pack("<I", 36 + len(dados)) + b"WAVE"
    cab += b"fmt " + struct.pack("<IHHIIHH", 16, 1, canais, taxa, taxa * bq, bq, 16)
    return cab + b"data" + struct.pack("<I", len(dados)) + dados


def grande():
    """Um WAV de 5 MB (30 s, 44,1 kHz, estéreo) num pendrive: reproducir toca
    enquanto lê (começa com ~0,3 s lidos), e o som sai em menos de 2 s
    depois do Enter. Um fundo
    (200 Hz, baixo) mantém o DMA ligado, para que o tempo do .wav gravado
    seja o tempo real desde "[AUDIO] tocando"."""
    arqs = {"fondo.wav": gerar_wav(8000, 8, 1, 20.0, [200], 0.1),
            "grande.wav": gerar_wav_rapido(44100, 2, 30.0, 880, 0.4)}
    pen = criar_pendrive("h70-grande", rotulo="GRANDE", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h70-grande", "ide", imagem=imagem_console(), extras=extras, som="hda") as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("montar usb0p1")
        m.comando("reproducir /usb/fondo.wav &")
        fim = time.time() + 30
        while "[AUDIO] tocando" not in m.serial() and time.time() < fim:
            time.sleep(0.05)
        t0 = time.time()
        time.sleep(4)   # o fundo termina de ler o arquivo dele
        m.digitar("reproducir /usb/grande.wav", 0.04)
        marca = len(m.serial())
        t1 = time.time()
        m.tecla("ret", 0.01)
        fim = time.time() + 20
        while "comeca a tocar" not in m.serial()[marca:] and time.time() < fim:
            time.sleep(0.02)
        t2 = time.time()
        time.sleep(8)
        s = m.serial()[marca:]
    comeco = re.search(r"comeca a tocar com (\d+) KB lidos em (\d+) ms", s)
    verificar(comeco is not None, "reproducir começa a tocar sem ler o arquivo inteiro")
    kb, ms = int(comeco.group(1)), int(comeco.group(2))
    # O relógio do SaguiOS atrasa enquanto o EHCI lê (interrupções desligadas):
    # a velocidade real sai do relógio do anfitrião (do Enter até o log).
    print(f"  leitura do pendrive (EHCI): {kb} KB em {(t2 - t1) * 1000:.0f} ms reais = {kb / (t2 - t1):.0f} KB/s "
          f"(o SaguiOS mediu {ms} ms)")
    esq, dir_ = ler_gravacao(m.wav)
    ini = int((t1 - t0 - 0.5) * TAXA)
    inicio = None
    for i in range(max(ini, 0), len(esq) - 2400, 1200):
        x = esq[i:i + 2400]
        if math.sqrt(goertzel(x, 880)) / 2400 > 1000:
            inicio = i / TAXA
            break
    verificar(inicio is not None, "o tom do WAV grande aparece na gravação")
    atraso = inicio - (t1 - t0)
    verificar(atraso < 2.0, f"o WAV de 5 MB começa a soar {atraso:.2f} s depois do Enter (< 2 s)")


def main():
    quais = sys.argv[1:] or ["ac97", "hda", "sem", "chip8", "grande"]
    arqs = {n: gerar_wav(*p) for n, p in ARQUIVOS.items()}
    arqs["tono.py"] = TONO_PY
    pen = criar_pendrive("h70-pen", rotulo="SONIDOS", arquivos=arqs)
    for placa in ("ac97", "hda"):
        if placa in quais:
            provar(placa, pen)
    if "sem" in quais:
        sem_placa()
    if "chip8" in quais:
        chip8()
    if "grande" in quais:
        grande()
    return 0


if __name__ == "__main__":
    sys.exit(main())
