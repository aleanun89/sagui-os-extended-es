#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
mkiso.py - monta la ISO de CD de SaguiOS (build/saguios.iso), sin xorriso ni
genisoimage (D077).

El CD arranca por El Torito con EMULACIÓN DE DISCO DURO: la BIOS lee la imagen
de disco de SaguiOS (hecha por mkimagem.py --cd) desde dentro del CD y la
presenta como el disco 0x80. El MBR y la etapa 2 son los de siempre: leen el
"disco" con INT 13h AH=42h en sectores de 512 bytes, y la BIOS convierte a los
sectores de 2048 bytes del CD. Nada del cargador cambia.

Distribución (sectores de 2048 bytes):
  0-15   área de sistema (ceros)
  16     descriptor primario del volumen (ISO 9660)
  17     descriptor de arranque (El Torito), apunta al catálogo
  18     terminador de los descriptores
  19     catálogo de arranque: entrada de validación + entrada por defecto (medio 4 = disco duro)
  20     tabla de rutas (orden little-endian)
  21     tabla de rutas (orden big-endian)
  22     directorio raíz: ".", ".." y SAGUIOS.IMG (la misma imagen, para quien quiera copiarla)
  23...  la imagen de disco

Uso: mkiso.py --imagem build/saguios-cd.img --saida build/saguios.iso
"""
import argparse
import os
import struct
import sys
import time

BLOCO = 2048
LBA_PVD, LBA_BOOT, LBA_FIM, LBA_CATALOGO, LBA_TAB_L, LBA_TAB_M, LBA_RAIZ, LBA_IMAGEM = 16, 17, 18, 19, 20, 21, 22, 23
TIPO_PARTICAO = 0xDA   # el mismo de la partición de SaguiOS (mkimagem.py)


def ambos16(v):
    return struct.pack("<H", v) + struct.pack(">H", v)


def ambos32(v):
    return struct.pack("<I", v) + struct.pack(">I", v)


def texto(s, n, preencher=b" "):
    b = s.encode("ascii")[:n]
    return b + preencher * (n - len(b))


def data_registro(t):
    """Fecha de 7 bytes de los registros de directorio (años desde 1900, ..., huso 0)."""
    return bytes([t.tm_year - 1900, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, 0])


def data_volume(t):
    """Fecha de 17 bytes del descriptor del volumen: "AAAAMMDDHHMMSScc" + huso."""
    return time.strftime("%Y%m%d%H%M%S00", t).encode("ascii") + b"\0"


def registro(lba, tamanho, nome, diretorio, t):
    corpo = (bytes([0]) + ambos32(lba) + ambos32(tamanho) + data_registro(t)
             + bytes([2 if diretorio else 0, 0, 0]) + ambos16(1) + bytes([len(nome)]) + nome)
    reg = bytes([len(corpo) + 1]) + corpo
    if len(reg) % 2:
        reg += b"\0"
    return reg


def tabela_caminhos(lba_raiz, grande):
    fmt = ">" if grande else "<"
    return bytes([1, 0]) + struct.pack(fmt + "I", lba_raiz) + struct.pack(fmt + "H", 1) + b"\0\0"


def catalogo(lba_imagem):
    validacao = bytearray(32)
    validacao[0] = 1                         # cabecera
    validacao[1] = 0                         # plataforma 80x86
    validacao[4:28] = texto("SaguiOS", 24, b"\0")
    validacao[30], validacao[31] = 0x55, 0xAA
    soma = sum(struct.unpack("<16H", bytes(validacao))) & 0xFFFF
    validacao[28:30] = struct.pack("<H", (-soma) & 0xFFFF)   # las 16 palabras suman cero
    padrao = bytearray(32)
    padrao[0] = 0x88                         # arrancable
    padrao[1] = 4                            # emulación de disco duro
    padrao[2:4] = struct.pack("<H", 0)       # segmento de carga 0 = 0x7C0 (el por defecto)
    padrao[4] = TIPO_PARTICAO                # tipo de la partición de la imagen
    padrao[6:8] = struct.pack("<H", 1)       # carga 1 sector virtual (el MBR)
    padrao[8:12] = struct.pack("<I", lba_imagem)
    return bytes(validacao) + bytes(padrao)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--imagem", required=True, help="imagem de disco feita com mkimagem.py --cd")
    ap.add_argument("--saida", required=True)
    ap.add_argument("--volume", default="SAGUIOS")
    a = ap.parse_args()

    img = open(a.imagem, "rb").read()
    if len(img) < 1024 or img[510:512] != b"\x55\xAA" or img[512:516] != b"SGHD":
        sys.exit("mkiso: a imagem não parece do SaguiOS (MBR ou cabeçalho)")
    opcoes = struct.unpack_from("<H", img, 512 + 6)[0]
    if not opcoes & 8:
        sys.exit("mkiso: a imagem não foi feita com mkimagem.py --cd (falta OPC_CD)")
    # La BIOS calcula la geometría por el CHS final de la partición 1: la imagen tiene
    # de ser un número entero de cilindros (mkimagem.py --cd se encarga de ello).
    if (len(img) // 512) % (255 * 63):
        sys.exit("mkiso: o tamanho da imagem não é um número inteiro de cilindros (use mkimagem.py --cd)")

    blocos_img = (len(img) + BLOCO - 1) // BLOCO
    total = LBA_IMAGEM + blocos_img
    t = time.gmtime(int(os.environ.get("SOURCE_DATE_EPOCH", time.time())))

    raiz = (registro(LBA_RAIZ, BLOCO, b"\0", True, t) + registro(LBA_RAIZ, BLOCO, b"\1", True, t)
            + registro(LBA_IMAGEM, len(img), b"SAGUIOS.IMG;1", False, t))
    tab_l, tab_m = tabela_caminhos(LBA_RAIZ, False), tabela_caminhos(LBA_RAIZ, True)

    pvd = bytearray(BLOCO)
    pvd[0:7] = b"\x01CD001\x01"
    pvd[8:40] = texto("", 32)
    pvd[40:72] = texto(a.volume, 32)
    pvd[80:88] = ambos32(total)
    pvd[120:124] = ambos16(1)                # volúmenes en el conjunto
    pvd[124:128] = ambos16(1)                # número de este volumen
    pvd[128:132] = ambos16(BLOCO)
    pvd[132:140] = ambos32(len(tab_l))
    pvd[140:144] = struct.pack("<I", LBA_TAB_L)
    pvd[148:152] = struct.pack(">I", LBA_TAB_M)
    pvd[156:190] = registro(LBA_RAIZ, BLOCO, b"\0", True, t)
    pvd[190:318] = texto("SAGUIOS", 128)
    pvd[318:446] = texto("", 128)
    pvd[446:574] = texto("", 128)
    pvd[574:702] = texto("SAGUIOS MKISO.PY", 128)
    pvd[702:813] = texto("", 111)
    pvd[813:830] = data_volume(t)
    pvd[830:847] = data_volume(t)
    pvd[847:864] = b"0" * 16 + b"\0"
    pvd[864:881] = b"0" * 16 + b"\0"
    pvd[881] = 1                             # versión de la estructura de archivos

    boot = bytearray(BLOCO)
    boot[0:7] = b"\x00CD001\x01"
    boot[7:39] = texto("EL TORITO SPECIFICATION", 32, b"\0")
    boot[71:75] = struct.pack("<I", LBA_CATALOGO)

    fim = bytearray(BLOCO)
    fim[0:7] = b"\xffCD001\x01"

    def bloco(b):
        return bytes(b) + b"\0" * (BLOCO - len(b))

    iso = (b"\0" * (16 * BLOCO) + bloco(pvd) + bloco(boot) + bloco(fim) + bloco(catalogo(LBA_IMAGEM))
           + bloco(tab_l) + bloco(tab_m) + bloco(raiz) + img + b"\0" * (blocos_img * BLOCO - len(img)))
    assert len(iso) == total * BLOCO
    with open(a.saida, "wb") as f:
        f.write(iso)
    print(f"mkiso: {a.saida}: {len(iso)} bytes, imagem de {len(img)} bytes no bloco {LBA_IMAGEM} "
          f"(El Torito, emulação de disco rígido)")


if __name__ == "__main__":
    main()
