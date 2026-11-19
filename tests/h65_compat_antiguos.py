#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H65 - compatibilidad con PC antiguos, fase A (mejora 036, D076-D078).

Critério:
  - com -cpu pentium3 e -cpu athlon,-sse2 (SSE sem SSE2; o "athlon" do QEMU
    tem SSE2) o sistema chega à área de trabalho; o rapido.c fica sem SSE2;
    o cc usa a x87 sozinho e /ejemplos/flotante.c e mandelbrot.c dão a mesma
    saída que o GCC; cc -sse2 avisa; o simd não tenta SSE2;
  - a ISO (make iso, El Torito com emulação de disco rígido) arranca com
    -cdrom: área de trabalho, um comando e o aviso claro de
    "pantalla --guardar" (o CD é só leitura);
  - com mkimagem.py --bpp 16 o carregador liga 1024x768x16 (RGB565): a
    captura tem as cores da de 32 bits dentro do erro do 565; a troca a
    quente para 800x600 fica em 16 bits e também confere; --bpp 15 liga
    RGB555 e o terminal de tela cheia funciona;
  - "infopc --informe" grava em /home (partição de dados) um .txt com as
    seções, os identificadores PCI do QEMU e o log do kernel; com um
    caminho, grava nele.
As partes rodam em paralelo (uma máquina cada) para caber no tempo da bateria.
Python com float numa CPU sem SSE2 é falha conhecida (mejora 036): aqui só se
confere que ele recusa com uma mensagem, sem derrubar nada.
"""
import hashlib
import os
import re
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, ler_ppm, extrair_particao, debugfs, RAIZ, SAIDA,  # noqa: E402
                  FalhaTeste, IMAGEM, particoes_img, opcoes_apps)
from b7_cc_flotante import salida_gcc, salida_programa  # noqa: E402

ISO = os.path.join(RAIZ, "build", "saguios.iso")
DADOS_MB = 8


def imagem(nome, *opcoes):
    """build/testes/<nome>.img: a imagem normal com outras opções do
    mkimagem.py (cada parte com o seu arquivo, porque rodam juntas)."""
    b = os.path.join(RAIZ, "build")
    destino = os.path.join(SAIDA, nome + ".img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", os.path.join(b, "ramdisk.sgfs"), "--saida", destino, "--vesa", *opcoes],
                   check=True, stdout=subprocess.DEVNULL)
    return destino


def esperar(m, padrao, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise FalhaTeste(f"tempo esgotado esperando {padrao!r}")


def area_de_trabalho(m, rotulo):
    m.esperar_serial("area de trabalho no ar", 150)
    m.esperar_serial("sagui:/$", 60)
    time.sleep(1)
    verificar(True, f"[{rotulo}] chegou à área de trabalho com o Terminal")


def comparar_gcc(m, comando, fonte, nome):
    """A saída do programa vai para um arquivo e se compara o SHA-256 com o
    da saída do mesmo fonte compilado com GCC: na área de trabalho o
    Terminal copia a saída para a serial depois do kernel avisar que o
    programa terminou, e as linhas se misturariam."""
    m.comando(f"{comando} > saida.txt", tempo=150)
    out = m.comando("sha256sum saida.txt")
    esperada = salida_gcc(fonte)
    h = hashlib.sha256(esperada.encode("latin-1")).hexdigest()
    if h not in out:
        print("    " + "\n    ".join(salida_programa(m.comando("cat saida.txt"))[:8]))
    verificar(h in out, nome)


def tela_ppm(m, nome):
    """Captura crua (PPM, sem passar pelo PNG): largura, altura, pixels RGB."""
    ppm = os.path.join(SAIDA, f"{m.nome}-{nome}.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    return ler_ppm(ppm)


def parecidas(a, b, tol, y0=24):
    """Fração dos pixels (abaixo da barra superior, que tem o relógio) cuja
    diferença em cada canal cabe na tolerância (r, g, b)."""
    (l, h, pa), (l2, h2, pb) = a, b
    if (l, h) != (l2, h2):
        return 0.0
    ok = total = 0
    for i in range(y0 * l * 3, l * h * 3, 3):
        total += 1
        if (abs(pa[i] - pb[i]) <= tol[0] and abs(pa[i + 1] - pb[i + 1]) <= tol[1]
                and abs(pa[i + 2] - pb[i + 2]) <= tol[2]):
            ok += 1
    return ok / total


# ---------------------------------------------------------------- CPU sem SSE2

def cpu_sem_sse2(cpu, rotulo, completo):
    with Maquina(f"h65{rotulo[:3]}", "ide", extras=["-cpu", cpu]) as m:
        area_de_trabalho(m, rotulo)
        s = m.serial()
        # D208: com SSE (sem SSE2) o kernel pode usar a variante SSE; nunca a SSE2 nem a SSE3.
        escolhido = re.search(r"\[RAPIDO\] escolhido: (.*)", s)
        verificar("[RAPIDO] sem SSE" in s or (escolhido and "sse2" not in escolhido.group(1)
                                              and "sse3" not in escolhido.group(1)),
                  f"[{rotulo}] o kernel não usa SSE2 nos gráficos")
        m.comando("cd /tmp")
        out = m.comando("cc /ejemplos/flotante.c", tempo=150)
        verificar("coma flotante con x87 (la CPU no tiene SSE2)" in out and "(cc) terminou com codigo 0" in out,
                  f"[{rotulo}] cc escolhe a x87 sozinho e compila flotante.c")
        comparar_gcc(m, "./flotante", "ejemplos/flotante.c", f"[{rotulo}] flotante.c com x87 = GCC")
        if completo:
            out = m.comando("cc /ejemplos/mandelbrot.c", tempo=150)
            verificar("(cc) terminou com codigo 0" in out, f"[{rotulo}] cc compila mandelbrot.c")
            comparar_gcc(m, "./mandelbrot", "ejemplos/mandelbrot.c", f"[{rotulo}] mandelbrot.c com x87 = GCC")
            out = m.comando("cc -sse2 /ejemplos/flotante.c -o f2", tempo=150)
            verificar("esta CPU no tiene SSE2" in out, f"[{rotulo}] cc -sse2 avisa que a CPU não tem SSE2")
            out = m.comando("./f2")
            verificar("este programa usa SSE2 y la CPU no lo tiene" in out,
                      f"[{rotulo}] o programa com -sse2 recusa com uma mensagem (sem instrução inválida)")
            out = m.comando("simd")
            verificar("SSE2   no disponible" in out and "simd: todo correcto" in out,
                      f"[{rotulo}] simd testa x87, MMX e SSE e não tenta SSE2")
            m.captura(f"H65-{rotulo}")
        else:
            out = m.comando("simd")
            verificar("SSE2   no disponible" in out and "simd: todo correcto" in out, f"[{rotulo}] simd sem SSE2")
            # Sem SSE2 o /apps/bin/python passa para o /apps/bin/python-x87 (D082).
            out = m.comando("python -c 'print(1.5 * 2, 1/3)'", tempo=60)
            print("    python:", " | ".join(l for l in salida_programa(out)))
            verificar("3.0 0.3333333333333333" in out and "excecao" not in out.lower(),
                      f"[{rotulo}] python sem SSE2 (variante x87) calcula com float")
        out = m.comando("echo fim")
        verificar("fim" in out, f"[{rotulo}] o sistema continua respondendo")


# ---------------------------------------------------------------- CD

def cd():
    r = subprocess.run(["make", "-s", "-C", RAIZ, "iso"], capture_output=True, text=True)
    verificar(r.returncode == 0 and os.path.exists(ISO), "make iso gera build/saguios.iso (tools/mkiso.py)")
    iso = open(ISO, "rb").read(20 * 2048)
    verificar(iso[16 * 2048 + 1:16 * 2048 + 6] == b"CD001" and iso[17 * 2048 + 7:17 * 2048 + 30] == b"EL TORITO SPECIFICATION",
              "a ISO tem o descritor ISO 9660 e o de boot El Torito")
    verificar(iso[19 * 2048 + 32] == 0x88 and iso[19 * 2048 + 33] == 4,
              "o catálogo de boot pede emulação de disco rígido")
    with Maquina("h65cd", "cd", imagem=ISO) as m:
        area_de_trabalho(m, "CD")
        s = m.serial()
        verificar("Arranque desde CD (El Torito)" in s and "[BOOT] cd=1 leitura_chs=1" in s,
                  "o kernel sabe que veio do CD e que o carregador leu por CHS")
        out = m.comando("echo hola desde el CD")
        verificar("hola desde el CD" in out, "um comando roda no sistema arrancado do CD")
        out = m.comando("ls /ejemplos")
        verificar("mandelbrot.c" in out, "o ramdisk funciona como sempre")
        out = m.comando("pantalla --guardar")
        verificar("no se puede guardar: SaguiOS ha arrancado desde un CD" in out,
                  "pantalla --guardar explica que o CD é só leitura")
        out = m.comando("pantalla")
        verificar("arranque desde CD: no se puede guardar" in out, "pantalla mostra que a preferência não se grava")
        m.captura("H65-cd")


# ---------------------------------------------------------------- 16 e 15 bits

capturas = {}
pronto = {k: threading.Event() for k in ("32", "16")}


def video(bpp):
    """bpp = "32" (referência, a imagem normal) ou "16"."""
    # con /apps igual que la imagen normal: sin ella sale el aviso de «no se encontró /apps» y faltan iconos
    img = IMAGEM if bpp == "32" else imagem("h65-16bits", "--bpp", "16", *opcoes_apps())
    with Maquina(f"h65v{bpp}", "ide", imagem=img) as m:
        area_de_trabalho(m, f"{bpp} bits")
        if bpp == "16":
            s = m.serial()
            verificar("[VIDEO] 1024x768, 16 bits, pitch 2048" in s and "cores R5:11 G6:5 B5:0" in s,
                      "com --bpp 16 o carregador liga 1024x768 em RGB565")
            verificar("[BOOT] cd=0 leitura_chs=0 bpp_pref=16" in s, "a preferência de profundidade chega ao kernel")
            verificar(re.search(r"tela \S+16\b|tela c16|tela sse2-16", s) is not None,
                      "o rapido.c escolhe a conversão para 16 bits (C ou SSE2)")
            m.captura("H65-16bits")
        capturas[bpp + "-1024"] = tela_ppm(m, "1024")
        marca = len(m.serial())
        out = m.comando("pantalla 800x600")
        verificar("Pantalla cambiada a 800x600." in out, f"[{bpp} bits] troca a quente para 800x600")
        esperar(m, rf"\[VIDEO\] modo trocado: 800x600, {bpp} bits", desde=marca)
        verificar(True, f"[{bpp} bits] a troca a quente mantém {bpp} bits")
        esperar(m, r"\[GUI\] barra 800:", desde=marca)
        time.sleep(2)
        capturas[bpp + "-800"] = tela_ppm(m, "800")
        if bpp == "16":
            m.captura("H65-16bits-800x600")
            out = m.comando("pantalla")
            verificar("Pantalla actual:      800x600, 16 bits" in out, "pantalla mostra 800x600, 16 bits")
    pronto[bpp].set()


def bits15_informe():
    """15 bits (RGB555) no terminal de tela cheia, com a partição de dados em
    /home: o informe vai para lá e é lido depois do disco, no computador."""
    img = imagem("h65-informe", "--console", "--bpp", "15", "--dados-mb", str(DADOS_MB))
    ini = particoes_img(img)[-1][2]   # D388: /home (SAGUI-DATOS) es la última lógica
    with Maquina("h65inf", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 150)
        s = m.serial()
        verificar("[VIDEO] 1024x768, 15 bits, pitch 2048" in s and "cores R5:10 G5:5 B5:0" in s,
                  "com --bpp 15 o carregador liga 1024x768 em RGB555")
        m.esperar_tela("sagui:/$", 30)
        verificar(True, "o terminal de tela cheia aparece em 15 bits (texto reconhecido na captura)")
        out = m.comando("infopc --informe", tempo=90)
        g = re.search(r"Informe de compatibilidad escrito en (/home/informe-\d{8}-\d{4}\.txt) \((\d+) bytes\)", out)
        verificar(g is not None, "infopc --informe grava em /home (partição de dados persistente)")
        nome = g.group(1)
        verificar("Aviso:" not in out, "sem aviso: /home é persistente")
        out = m.comando("infopc --informe /tmp/meu.txt", tempo=90)
        verificar("escrito en /tmp/meu.txt" in out, "com um caminho, o informe vai para ele")
        out = m.comando("wc /tmp/meu.txt")
        g = re.search(r"(\d+) l.neas, \d+ palabras, (\d+) letras", out)
        verificar(g is not None and int(g.group(2)) > 4000, "o arquivo existe e tem conteúdo")
        m.captura("H65-15bits-informe")
        m.digitar("reiniciar\n", 0.04)   # desmonta /home: tudo gravado no disco
        fim = time.time() + 60
        while m.proc.poll() is None and time.time() < fim:
            time.sleep(0.3)
    p = extrair_particao(img, ini, DADOS_MB)
    txt = debugfs(p, "cat /" + os.path.basename(nome)).decode("latin-1")
    verificar(len(txt) > 4000, f"o informe está no disco ({len(txt)} bytes)")
    for secao in ("== Resumen ==", "== CPU ==", "== Memoria ==", "== Placa/BIOS ==", "== PCI ==", "== Vídeo ==",
                  "Modos de la BIOS (VBE)", "== Arranque ==", "== Discos y USB ==", "== ACPI ==",
                  "== Log del kernel =="):
        verificar(secao in txt, f"o informe tem a seção {secao!r}")
    for ident, oque in (("8086:1237", "i440FX"), ("8086:7000", "PIIX3 ISA"), ("8086:7010", "PIIX3 IDE"),
                        ("8086:7113", "PIIX4 ACPI"), ("1234:1111", "VGA do QEMU"), ("8086:100e", "e1000")):
        verificar(ident in txt, f"o informe lista o PCI {ident} ({oque})")
    verificar("driver: e1000 (soportado)" in txt and "driver: ata" in txt and "acpi (apagado S5 y botón, D113)" in txt,
              "a tabela do que se sabe: e1000 e ata com driver, PIIX4 ACPI com o apagado (D113)")
    verificar(re.search(r"0x117 +1024x768 +16 bits", txt) is not None and "15 bits  (actual)" in txt,
              "os modos VESA com bpp, e o atual marcado")
    verificar(re.search(r"FACP +0x[0-9a-f]{8}", txt) is not None and "DSDT" in txt, "as tabelas ACPI (FACP, DSDT)")
    verificar("[BOOT] cd=0" in txt and "[VIDEO] 1024x768, 15 bits" in txt, "o fim do log do kernel")
    verificar("SSE2:              sí" in txt, "o resumo diz se há SSE2")


# ---------------------------------------------------------------- principal

def rodar(nome, f, *args):
    try:
        f(*args)
        resultados[nome] = None
    except Exception as e:   # noqa: BLE001 - o relatório final diz qual parte falhou
        resultados[nome] = e
        print(f"  FALHA [{nome}]: {e}", flush=True)


resultados = {}


def main():
    partes = [("pentium3", cpu_sem_sse2, "pentium3", "pentium3", True),
              ("athlon", cpu_sem_sse2, "athlon,-sse2", "athlon", False),
              ("cd", cd), ("video32", video, "32"), ("video16", video, "16"),
              ("informe", bits15_informe)]
    fios = [threading.Thread(target=rodar, args=p) for p in partes]
    for f in fios:
        f.start()
        time.sleep(2)   # não liga todas as máquinas no mesmo instante
    for f in fios:
        f.join()
    if "32-1024" in capturas and "16-1024" in capturas:
        # RGB565: 3 bits a menos no vermelho e no azul, 2 no verde.
        fr = parecidas(capturas["16-1024"], capturas["32-1024"], (8, 4, 8))
        print(f"    1024x768: {fr * 100:.2f}% dos pixels dentro do erro do 565")
        rodar("cores 1024", verificar, fr > 0.995, "16 bits: as cores da área de trabalho conferem com as de 32 bits")
    if "32-800" in capturas and "16-800" in capturas:
        fr = parecidas(capturas["16-800"], capturas["32-800"], (8, 4, 8))
        print(f"    800x600: {fr * 100:.2f}% dos pixels dentro do erro do 565")
        rodar("cores 800", verificar, fr > 0.99, "16 bits a quente: as cores conferem com as de 32 bits")
    falhas = [n for n, e in resultados.items() if e is not None]
    if falhas:
        print("Falharam:", ", ".join(falhas))
        return 1
    print("H65 ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
