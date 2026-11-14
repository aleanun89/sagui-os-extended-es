#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""
mksgfs.py - monta o ramdisk SGFS (formato próprio do SaguiOS) a partir de uma pasta.

Uso: mksgfs.py <pasta_rootfs> <saida.sgfs>

Formato (todos os inteiros em little-endian):

  Cabeçalho, 32 bytes
    0  char[4]  "SGFS"
    4  u16      versão (1)
    6  u16      tamanho de cada entrada (72)
    8  u32      número de entradas
   12  u32      deslocamento da tabela de entradas (32)
   16  u32      deslocamento da área de dados
   20  u32      tamanho total do SGFS
   24  u32      soma de verificação: soma dos bytes de tudo depois do cabeçalho
   28  u32      reservado

  Entrada, 72 bytes
    0  char[56] caminho completo sem "/" inicial, terminado em zero (ex.: "docs/sobre.txt")
   56  u8       tipo: 1 = arquivo, 2 = pasta, 3 = programa
   57  u8[3]    reservado
   60  u32      tamanho em bytes (0 para pastas)
   64  u32      deslocamento dos dados a partir do início do SGFS
   68  u32      reservado

As entradas ficam ordenadas pelo caminho, então todo o conteúdo de uma pasta
aparece logo depois dela. Os textos (.txt) são convertidos de UTF-8 para
Latin-1, a codificação interna do sistema (PRD 4.4). Arquivos .gitkeep são
ignorados, mas a pasta que os contém entra na tabela.

D147: si la carpeta de origen no existe o queda vacía, el script termina con
error en vez de escribir un SGFS sin entradas. Un ramdisk vacío hace que el
kernel arranque sin ningún programa y que ninguna aplicación abra (D147). La
escritura es atómica (fichero temporal y renombrar) y el resultado se
verifica después.
"""
import os
import struct
import sys

TIPO_ARQUIVO, TIPO_PASTA, TIPO_PROGRAMA = 1, 2, 3
TAM_CAB, TAM_ENTRADA, MAX_CAMINHO = 32, 72, 56
EXT_TEXTO = {".txt", ".md", ".cfg", ".asm", ".pas"}


def coletar(raiz):
    # Una carpeta que no existe o que se quedó a medias deja el ramdisk vacío
    # (0 entradas): el kernel arranca sin ningún programa y ninguna aplicación
    # se puede abrir. Mejor fallar aquí, con un mensaje claro.
    if not os.path.isdir(raiz):
        sys.exit(f"mksgfs: no existe la carpeta {raiz}")
    itens = []
    for dirpath, dirs, arquivos in os.walk(raiz):
        dirs.sort()
        rel_dir = os.path.relpath(dirpath, raiz)
        if rel_dir != ".":
            itens.append((rel_dir.replace(os.sep, "/"), TIPO_PASTA, b""))
        for nome in sorted(arquivos):
            if nome == ".gitkeep":
                continue
            cam = os.path.join(dirpath, nome)
            rel = os.path.relpath(cam, raiz).replace(os.sep, "/")
            with open(cam, "rb") as f:
                dados = f.read()
            if dados[:4] == b"\x7fELF":
                tipo = TIPO_PROGRAMA
            else:
                tipo = TIPO_ARQUIVO
                if os.path.splitext(nome)[1].lower() in EXT_TEXTO:
                    dados = dados.decode("utf-8").encode("latin-1")
            itens.append((rel, tipo, dados))
    itens.sort(key=lambda i: i[0])
    return itens


def montar(itens):
    for caminho, _t, _d in itens:
        if len(caminho.encode("latin-1")) >= MAX_CAMINHO:
            sys.exit(f"mksgfs: caminho longo demais: {caminho}")
    tab_off = TAM_CAB
    dados_off = tab_off + TAM_ENTRADA * len(itens)
    dados_off = (dados_off + 15) & ~15
    tabela = b""
    area = b""
    for caminho, tipo, dados in itens:
        desloc = dados_off + len(area) if dados else 0
        tabela += struct.pack("<56sB3xIII", caminho.encode("latin-1"), tipo, len(dados), desloc, 0)
        area += dados
        area += b"\0" * ((-len(area)) % 16)  # cada arquivo começa alinhado em 16
    corpo = tabela + b"\0" * (dados_off - tab_off - len(tabela)) + area
    total = TAM_CAB + len(corpo)
    soma = sum(corpo) & 0xFFFFFFFF
    cab = struct.pack("<4sHHIIIIII", b"SGFS", 1, TAM_ENTRADA, len(itens), tab_off, dados_off, total, soma, 0)
    return cab + corpo


def validar_sgfs(dados):
    """Mira un SGFS: cabecera, número de entradas y suma de verificación.

    Devuelve (entradas, tamaño_total). Si el SGFS no vale, termina con el
    motivo: es la última puerta antes de escribir la imagen de disco.
    """
    if len(dados) < TAM_CAB:
        sys.exit(f"mksgfs: SGFS truncado ({len(dados)} bytes)")
    magic, versao, tam_ent, entradas, tab_off, dados_off, total, soma, _r = \
        struct.unpack("<4sHHIIIIII", dados[:TAM_CAB])
    if magic != b"SGFS":
        sys.exit(f"mksgfs: no es un SGFS (empieza por {magic!r})")
    if versao != 1:
        sys.exit(f"mksgfs: versión de SGFS desconocida: {versao}")
    if tam_ent != TAM_ENTRADA:
        sys.exit(f"mksgfs: tamaño de entrada inesperado: {tam_ent}")
    if entradas == 0:
        sys.exit("mksgfs: el SGFS no tiene entradas (el ramdisk quedaría vacío)")
    if total != len(dados):
        sys.exit(f"mksgfs: el tamaño del SGFS no cuadra ({total} en la cabecera, {len(dados)} en el fichero)")
    if tab_off != TAM_CAB or dados_off < TAM_CAB + TAM_ENTRADA * entradas or dados_off > len(dados):
        sys.exit("mksgfs: las tablas del SGFS no cuadran")
    if (sum(dados[TAM_CAB:]) & 0xFFFFFFFF) != soma:
        sys.exit("mksgfs: la suma de verificación no cuadra")
    return entradas, total


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    itens = coletar(sys.argv[1])
    if not itens:
        sys.exit(f"mksgfs: la carpeta {sys.argv[1]} está vacía: el ramdisk no puede quedar sin ficheros")
    img = montar(itens)
    saida = os.path.abspath(sys.argv[2])
    os.makedirs(os.path.dirname(saida), exist_ok=True)
    # Escritura atómica: se escribe aparte y se renombra. Si el proceso se
    # corta a mitad (la batería reiniciando el PC, por ejemplo), el fichero
    # final sigue siendo el anterior y no un trozo sin cabecera.
    tmp = saida + ".tmp"
    with open(tmp, "wb") as f:
        f.write(img)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, saida)
    with open(saida, "rb") as f:
        validar_sgfs(f.read())
    print(f"mksgfs: {len(itens)} entradas, {len(img)} bytes -> {sys.argv[2]}")


if __name__ == "__main__":
    main()
