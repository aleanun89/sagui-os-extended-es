#!/usr/bin/env python3
"""netsurf_gerar.py - genera los ficheros de NetSurf que el upstream saca de
herramientas del anfitrión que aquí no hay (D184). Lo llama
tools/netsurf_fuentes.sh.

  elementos GPERF SALIDA     la tabla de elementos HTML de libhubbub. El
                             upstream la genera con gperf (un hash perfecto);
                             aquí es una tabla ordenada con búsqueda binaria,
                             con la misma interfaz (wordlist,
                             hubbub_element_type_generated_lookup).
  imagens MAKEFILE RES SAIDA las imágenes de la barra del frontend framebuffer
                             (FB_IMAGE_* del Makefile del frontend). El
                             upstream usa tools/convert_image.c con libpng;
                             aquí un lector de PNG en Python (zlib) que
                             escribe el mismo C, byte a byte.
  mensagens FAT LANG PLAT SALIDA
                             el fichero Messages de un idioma y una plataforma
                             desde resources/FatMessages (el upstream usa
                             split-messages.pl, que pide HTML::Entities, o
                             split-messages.c, que pide zlib); sin comprimir.
  testament SALIDA VERSION   testament.h con valores fijos (el upstream mira
                             git, el usuario y el nombre de la máquina; aquí
                             la compilación tiene que salir igual siempre).
"""
import re
import struct
import sys
import zlib


# ------------------------------------------------------------------ elementos

def elementos(gperf, saida):
    texto = open(gperf, encoding="utf-8").read()
    partes = texto.split("%%")
    if len(partes) < 2:
        sys.exit("netsurf_gerar.py: %s no tiene %%%%" % gperf)
    pares = []
    for linha in partes[1].splitlines():
        linha = linha.strip()
        if not linha or linha.startswith("#"):
            continue
        nome, tipo = [c.strip() for c in linha.split(",", 1)]
        pares.append((nome.lower(), tipo))
    pares.sort()
    out = []
    out.append("/* Generado por tools/netsurf_gerar.py desde element-type.gperf (D184).\n"
               " * Sustituye la salida de gperf: tabla ordenada y búsqueda binaria,\n"
               " * sin distinguir mayúsculas, con la misma interfaz. */\n")
    out.append("#include <string.h>\n\n#include \"treebuilder/element-type.h\"\n\n")
    out.append("static const struct element_type_map wordlist[] = {\n")
    for nome, tipo in pares:
        out.append('\t{ "%s", %s },\n' % (nome, tipo))
    out.append("};\n\n")
    out.append(r"""static int hubbub_element_type_cmp(const char *nome, const char *s, size_t n)
{
	size_t i;
	for (i = 0; i < n; i++) {
		unsigned char a = (unsigned char)nome[i];
		unsigned char b = (unsigned char)s[i];
		if (b >= 'A' && b <= 'Z')
			b = b - 'A' + 'a';
		if (a == 0)
			return -1;
		if (a != b)
			return a < b ? -1 : 1;
	}
	return nome[n] == 0 ? 0 : 1;
}

static const struct element_type_map *
hubbub_element_type_generated_lookup(const char *str, size_t len)
{
	size_t ini = 0, fim = sizeof(wordlist) / sizeof(wordlist[0]);
	while (ini < fim) {
		size_t meio = (ini + fim) / 2;
		int c = hubbub_element_type_cmp(wordlist[meio].name, str, len);
		if (c == 0)
			return &wordlist[meio];
		if (c < 0)
			ini = meio + 1;
		else
			fim = meio;
	}
	return NULL;
}
""")
    open(saida, "w", encoding="utf-8").write("".join(out))


# ------------------------------------------------------------------ imagens

def ler_png(caminho):
    """Devuelve (largura, altura, filas RGBA) como las da libpng con las
    transformaciones de convert_image.c (paleta y gris a RGB, tRNS a alfa,
    relleno 0xff). La gamma 2.2/0.45455 de convert_image es la identidad."""
    d = open(caminho, "rb").read()
    if d[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit("netsurf_gerar.py: %s no es PNG" % caminho)
    pos = 8
    idat = b""
    plte = None
    trns = None
    while pos < len(d):
        n, tipo = struct.unpack(">I4s", d[pos:pos + 8])
        dados = d[pos + 8:pos + 8 + n]
        pos += 12 + n
        if tipo == b"IHDR":
            larg, alt, prof, cor, _c, _f, entrelaz = struct.unpack(">IIBBBBB", dados)
        elif tipo == b"PLTE":
            plte = dados
        elif tipo == b"tRNS":
            trns = dados
        elif tipo == b"IDAT":
            idat += dados
        elif tipo == b"IEND":
            break
    if entrelaz:
        sys.exit("netsurf_gerar.py: %s: PNG entrelazado no soportado" % caminho)
    canais = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[cor]
    bpp_bits = canais * prof
    linha_bytes = (larg * bpp_bits + 7) // 8
    bpp = max(1, bpp_bits // 8)
    cru = zlib.decompress(idat)
    filas = []
    ant = bytearray(linha_bytes)
    p = 0
    for _ in range(alt):
        filtro = cru[p]
        linha = bytearray(cru[p + 1:p + 1 + linha_bytes])
        p += 1 + linha_bytes
        for i in range(linha_bytes):
            a = linha[i - bpp] if i >= bpp else 0
            b = ant[i]
            c = ant[i - bpp] if i >= bpp else 0
            if filtro == 1:
                linha[i] = (linha[i] + a) & 255
            elif filtro == 2:
                linha[i] = (linha[i] + b) & 255
            elif filtro == 3:
                linha[i] = (linha[i] + ((a + b) >> 1)) & 255
            elif filtro == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                linha[i] = (linha[i] + pr) & 255
        ant = linha
        # Las muestras de la fila.
        amostras = []
        if prof == 8:
            amostras = list(linha)
        elif prof < 8:
            por_byte = 8 // prof
            mask = (1 << prof) - 1
            for i in range(larg * canais):
                byte = linha[i // por_byte]
                desl = 8 - prof * (i % por_byte + 1)
                amostras.append((byte >> desl) & mask)
        else:  # 16 bits: se queda el byte alto (png_set_strip_16)
            amostras = [linha[i] for i in range(0, len(linha), 2)]
        rgba = bytearray()
        for x in range(larg):
            s = amostras[x * canais:(x + 1) * canais]
            if cor == 3:
                i = s[0]
                r, g, bl = plte[3 * i:3 * i + 3]
                al = trns[i] if trns is not None and i < len(trns) else 255
            elif cor == 0:
                v = s[0]
                if prof < 8:
                    v = v * 255 // ((1 << prof) - 1)
                r = g = bl = v
                al = 255
                if trns is not None and len(trns) >= 2 and s[0] == struct.unpack(">H", trns[:2])[0]:
                    al = 0
            elif cor == 4:
                r = g = bl = s[0]
                al = s[1]
            elif cor == 2:
                r, g, bl = s
                al = 255
                if trns is not None and len(trns) >= 6 and tuple(s) == struct.unpack(">HHH", trns[:6]):
                    al = 0
            else:
                r, g, bl, al = s
            rgba += bytes((r, g, bl, al))
        filas.append(rgba)
    return larg, alt, filas


def converter_imagem(png, saida, nome):
    larg, alt, filas = ler_png(png)
    # La detección del puntero (hotspot), como en convert_image.c: bytes
    # en el orden de la libpng (R, G, B, A) leídos con sus desplazamientos.
    def px(x, y, off):
        return filas[y][4 * x + off]
    R, G, B, A = 2, 1, 0, 3
    cursor = True
    hot_x = hot_y = 0
    verdes = 0
    for i in range(larg):
        if px(i, 0, A) == 255:
            if px(i, 0, G) == 255:
                verdes += 1
                hot_x = i
            if px(i, 0, B) != 0 or px(i, 0, R) != 0:
                cursor = False
                break
        elif px(i, 0, A) != 0:
            cursor = False
            break
    if cursor and verdes != 1:
        cursor = False
    if cursor:
        for i in range(alt):
            if px(0, i, A) == 255:
                if px(0, i, G) == 255:
                    verdes += 1
                    hot_y = i
                if px(0, i, B) != 0 or px(0, i, R) != 0:
                    cursor = False
                    break
            elif px(0, i, A) != 0:
                cursor = False
                break
        if cursor and verdes != 2:
            cursor = False
    w = larg - 1 if cursor else larg
    h = alt - 1 if cursor else alt
    o = []
    o.append("/* This file is auto-generated from %s\n" % png.split("/netsurf/", 1)[-1])
    o.append(" *\n * Do not edit this file directly.\n */\n\n")
    o.append("#include <sys/types.h>\n\n")
    o.append("#include <stdint.h>\n\n")
    o.append("#include <stdbool.h>\n\n")
    o.append("#include <libnsfb.h>\n\n")
    o.append("#include \"netsurf/plot_style.h\"\n")
    o.append("#include \"framebuffer/gui.h\"\n")
    o.append("#include \"framebuffer/fbtk.h\"\n\n")
    o.append("static uint8_t %s_pixdata[] = {\n" % nome)
    for y in range(h):
        fila = filas[y + 1] if cursor else filas[y]
        ini = 4 if cursor else 0
        o.append("\t")
        for x in range(w):
            for c in range(4):
                o.append("0x%02x, " % fila[ini + 4 * x + c])
        o.append("\n")
    o.append("};\n\n")
    o.append("struct fbtk_bitmap %s = {\n" % nome)
    o.append("\t.width\t\t= %d,\n" % w)
    o.append("\t.height\t\t= %d,\n" % h)
    o.append("\t.hot_x\t\t= %d,\n" % (hot_x - 1 if cursor else 0))
    o.append("\t.hot_y\t\t= %d,\n" % (hot_y - 1 if cursor else 0))
    o.append("\t.pixdata\t= %s_pixdata,\n" % nome)
    o.append("};\n\n")
    open(saida, "w", encoding="utf-8").write("".join(o))


def imagens(makefile, res, saida):
    n = 0
    for linha in open(makefile, encoding="utf-8"):
        m = re.match(r"^FB_IMAGE_(\w+)\s*:=\s*(\S+)", linha)
        if m:
            nome, rel = m.groups()
            converter_imagem("%s/%s" % (res, rel), "%s/image-%s.c" % (saida, nome), nome)
            n += 1
    if n == 0:
        sys.exit("netsurf_gerar.py: no hay FB_IMAGE_* en %s" % makefile)


# ------------------------------------------------------------------ mensagens

def mensagens(fat, lingua, plataforma, saida):
    """Como split-messages -f messages: para cada clave, la traducción del
    idioma pedido o, si no la hay, la inglesa; solo las de la plataforma
    "all" o la pedida. El orden es el del fichero."""
    valores = {}
    ordem = []
    for linha in open(fat, encoding="utf-8"):
        linha = linha.rstrip("\n")
        if not linha or linha.startswith("#") or ":" not in linha:
            continue
        chave, valor = linha.split(":", 1)
        partes = chave.split(".", 2)
        if len(partes) != 3:
            continue
        lang, plat, tok = partes
        if plat != "all" and plat != plataforma:
            continue
        if lang not in (lingua, "en"):
            continue
        if tok not in valores:
            ordem.append(tok)
            valores[tok] = (lang, valor)
        elif lang == lingua and valores[tok][0] != lingua:
            valores[tok] = (lang, valor)
    with open(saida, "w", encoding="utf-8") as f:
        f.write("# This messages file is automatically generated from %s\n" % fat)
        f.write("# at build-time.  Please go and edit that instead of this.\n\n")
        for tok in ordem:
            f.write("%s:%s\n" % (tok, valores[tok][1]))


# ------------------------------------------------------------------ testament

def testament(saida, versao):
    t = []
    t.append("/* testament.h generado por tools/netsurf_gerar.py (D184): valores fijos. */\n")
    t.append("#ifndef NETSURF_REVISION_TESTAMENT\n")
    t.append("#define NETSURF_REVISION_TESTAMENT \"netsurf-all-%s\"\n" % versao)
    t.append("#define USERNAME \"saguios\"\n")
    t.append("#define GECOS \"SaguiOS\"\n")
    t.append("#define WT_ROOT \"netsurf-all-%s\"\n" % versao)
    t.append("#define WT_HOSTNAME \"saguios\"\n")
    t.append("#define WT_COMPILEDATE \"netsurf-all-%s\"\n" % versao)
    t.append("#define WT_BRANCHPATH \"release/%s\"\n" % versao)
    t.append("#define WT_BRANCHISTAG 1\n")
    t.append("#define WT_TAGIS \"release/%s\"\n" % versao)
    t.append("#define WT_NO_GIT 1\n")
    t.append("#define WT_REVID \"netsurf-all-%s\"\n" % versao)
    t.append("#define WT_MODIFIED 0\n")
    t.append("#define WT_MODIFICATIONS { { 0, 0 } }\n")
    t.append("#endif\n")
    open(saida, "w", encoding="utf-8").write("".join(t))


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    op = sys.argv[1]
    if op == "elementos" and len(sys.argv) == 4:
        elementos(sys.argv[2], sys.argv[3])
    elif op == "imagens" and len(sys.argv) == 5:
        imagens(sys.argv[2], sys.argv[3], sys.argv[4])
    elif op == "mensagens" and len(sys.argv) == 6:
        mensagens(sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5])
    elif op == "testament" and len(sys.argv) == 4:
        testament(sys.argv[2], sys.argv[3])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
