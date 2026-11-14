#!/usr/bin/env python3
"""cnc_extraer.py - saca de las ISO de Command & Conquer lo que necesita Vanilla Conquer (D230).

Las dos ISO de C&C Gold para Windows 95 (las que EA regaló en 2007, volúmenes
GDI95 y NOD95) se ponen en input_share/games/CnC/ (fuera de git). Esta
herramienta del PC las lee con un lector de ISO 9660 propio y descomprime el
instalador (INSTALL/SETUP.Z, un archivo de InstallShield 3 con el «implode»
de PKWARE) con un descompresor propio: sin dependencias, solo la biblioteca
estándar de Python. Copia a una carpeta, que el Makefile pone en /jogos/CnC
de la partición de juegos, solo los ficheros que usa el motor, como los
dejaría el instalador de Windows:

  CnC/                  lo común a los dos discos (21 MB)
    CONQUER.MIX DESERT.MIX TEMPERAT.MIX WINTER.MIX SOUNDS.MIX   (de la raíz)
    UPDATE.MIX UPDATEC.MIX CCLOCAL.MIX LOCAL.MIX SPEECH.MIX TRANSIT.MIX
    DESEICNH.MIX TEMPICNH.MIX WINTICNH.MIX                      (de SETUP.Z)
    SCORES.MIX          la música (39 MB; igual en los dos discos)
  CnC/gdi/GENERAL.MIX   las misiones de GDI (2,5 MB)
  CnC/nod/GENERAL.MIX   las de NOD, si está la ISO de NOD
  CnC/gdi/MOVIES.MIX, CnC/nod/MOVIES.MIX   las películas (449 y 432 MB)

Las carpetas gdi/ y nod/ son las de Vanilla Conquer para tener los dos discos
copiados (tiberiandawn/conquer.cpp, Change_Local_Dir): el motor pasa de una a
otra como si se cambiara de CD. Las películas son el 90 % del total y no
caben en la partición de juegos de 800 MB con lo demás: van solo con
--videos (el motor juega igual sin ellas: se salta las escenas).
--sin-musica omite SCORES.MIX.

El resto de la ISO (el instalador, los .EXE y .DLL de Windows, DirectX,
Acrobat Reader, los SIZZLE.VQA de presentación, los manuales en PDF...) no
se copia. Los nombres van en MAYÚSCULAS, como en la ISO (el motor los busca
sin distinguir mayúsculas, con fnmatch y FNM_CASEFOLD).

  tools/cnc_extraer.py --listar ISO
  tools/cnc_extraer.py [--videos] [--sin-musica] ISO_GDI [ISO_NOD] DESTINO
  tools/cnc_extraer.py --buscar CARPETA DESTINO [opciones]   (las ISO en CARPETA,
                        reconocidas por su volumen, GDI95 y NOD95)
  tools/cnc_extraer.py --comprobar   (las pruebas del descompresor, sin ISO)

Escribe en DESTINO un LEEME-SAGUIOS.TXT con lo que copió y de dónde, y no
reescribe un fichero que ya está igual (mismo tamaño), para que make no
recopie cientos de MB cada vez.
"""
import os
import struct
import sys

SECTOR = 2048

# De la raíz de la ISO (iguales en las dos).
RAIZ_COMUM = ["CONQUER.MIX", "DESERT.MIX", "TEMPERAT.MIX", "WINTER.MIX", "SOUNDS.MIX"]
MUSICA = "SCORES.MIX"
# Del instalador (INSTALL/SETUP.Z): lo que el C&C95 dejaba en el disco duro.
DO_INSTALADOR = ["UPDATE.MIX", "UPDATEC.MIX", "CCLOCAL.MIX", "LOCAL.MIX", "SPEECH.MIX",
                 "TRANSIT.MIX", "DESEICNH.MIX", "TEMPICNH.MIX", "WINTICNH.MIX"]
# Lo propio de cada disco, en su carpeta (gdi/ o nod/).
POR_DISCO = ["GENERAL.MIX"]
VIDEOS = "MOVIES.MIX"


class Iso:
    """Un lector mínimo de ISO 9660 (nivel 1/2, sin Joliet ni Rock Ridge)."""

    def __init__(self, caminho):
        self.caminho = caminho
        self.f = open(caminho, "rb")
        self.volume = None
        self.raiz = None
        setor = 16
        while True:
            d = self.ler(setor, 1)
            tipo, ident = d[0], d[1:6]
            if ident != b"CD001":
                raise ValueError("%s: no es una ISO 9660 (sector %d)" % (caminho, setor))
            if tipo == 1:
                self.volume = d[40:72].decode("ascii", "replace").strip()
                self.raiz = self.registro(d[156:156 + 34])
            if tipo == 255:
                break
            setor += 1
        if self.raiz is None:
            raise ValueError("%s: sin descriptor primario" % caminho)

    def ler(self, setor, n):
        self.f.seek(setor * SECTOR)
        return self.f.read(n * SECTOR)

    @staticmethod
    def registro(r):
        tam, = struct.unpack_from("<B", r, 0)
        ext, = struct.unpack_from("<I", r, 2)
        comp, = struct.unpack_from("<I", r, 10)
        flags = r[25]
        nlen = r[32]
        nome = r[33:33 + nlen]
        return {"tam": tam, "ext": ext, "comp": comp, "pasta": bool(flags & 2), "nome": nome}

    def listar_pasta(self, reg):
        dados = self.ler(reg["ext"], (reg["comp"] + SECTOR - 1) // SECTOR)[:reg["comp"]]
        i = 0
        while i < len(dados):
            tam = dados[i]
            if tam == 0:
                # El resto del sector está vacío: el siguiente registro va en el siguiente.
                i = (i // SECTOR + 1) * SECTOR
                continue
            r = self.registro(dados[i:i + tam])
            i += tam
            if r["nome"] in (b"\x00", b"\x01"):
                continue
            nome = r["nome"].decode("ascii", "replace")
            if ";" in nome:
                nome = nome.split(";")[0]
            if nome.endswith(".") and not r["pasta"]:
                nome = nome[:-1]
            r["nome"] = nome
            yield r

    def arvore(self, reg=None, prefixo=""):
        reg = reg or self.raiz
        for r in self.listar_pasta(reg):
            caminho = prefixo + r["nome"]
            if r["pasta"]:
                yield caminho + "/", r
                for x in self.arvore(r, caminho + "/"):
                    yield x
            else:
                yield caminho, r

    def raiz_por_nome(self):
        return {r["nome"].upper(): r for r in self.listar_pasta(self.raiz)}

    def copiar_para(self, reg, tmp):
        """Copia un fichero por trozos (MOVIES.MIX pasa de 400 MB)."""
        restante = reg["comp"]
        setor = reg["ext"]
        with open(tmp, "wb") as o:
            while restante > 0:
                n = min(restante, 512 * SECTOR)
                bloco = self.ler(setor, (n + SECTOR - 1) // SECTOR)[:n]
                if len(bloco) != n:
                    raise IOError("%s: ISO truncada" % self.caminho)
                o.write(bloco)
                restante -= n
                setor += (n + SECTOR - 1) // SECTOR


class Blast:
    """Descompresor del «implode» de PKWARE (Data Compression Library), el que
    usa el instalador de InstallShield 3 (SETUP.Z). El formato es el que
    describe el blast.c de zlib/contrib (Mark Adler); esta es una
    implementación propia, con tablas de 13 bits para decodificar rápido."""

    # Las longitudes de los códigos de Huffman, en la forma compacta del
    # formato: cada byte es (repeticiones - 1) << 4 | longitud.
    LITLEN = [11, 124, 8, 7, 28, 7, 188, 13, 76, 4, 10, 8, 12, 10, 12, 10, 8, 23, 8,
              9, 7, 6, 7, 8, 7, 6, 55, 8, 23, 24, 12, 11, 7, 9, 11, 12, 6, 7, 22, 5,
              7, 24, 6, 11, 9, 6, 7, 22, 7, 11, 38, 7, 9, 8, 25, 11, 8, 11, 9, 12,
              8, 12, 5, 38, 5, 38, 5, 11, 7, 5, 6, 21, 6, 10, 53, 8, 7, 24, 10, 27,
              44, 253, 253, 253, 252, 252, 252, 13, 12, 45, 12, 45, 12, 61, 12, 45,
              44, 173]
    LENLEN = [2, 35, 36, 53, 38, 23]
    DISTLEN = [2, 20, 53, 230, 247, 151, 248]
    BASE = [3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264]
    EXTRA = [0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8]
    MAXBITS = 13

    @classmethod
    def tabela(cls, compacta):
        """Una tabla de 2^13 entradas: los 13 bits siguientes (el primero en
        el bit 0) -> (símbolo, longitud). Los códigos del formato son los
        canónicos con los bits invertidos, leídos del más significativo al
        menos significativo."""
        comp = []
        for b in compacta:
            comp += [b & 15] * ((b >> 4) + 1)
        n = len(comp)
        # Códigos canónicos: por longitud y, dentro de ella, por símbolo.
        conta = [0] * (cls.MAXBITS + 1)
        for l in comp:
            conta[l] += 1
        conta[0] = 0
        codigo = 0
        prox = [0] * (cls.MAXBITS + 2)
        for l in range(1, cls.MAXBITS + 1):
            codigo = (codigo + conta[l - 1]) << 1
            prox[l] = codigo
        t = [None] * (1 << cls.MAXBITS)
        for sim in range(n):
            l = comp[sim]
            if l == 0:
                continue
            c = prox[l]
            prox[l] += 1
            # Bits leídos: el i-ésimo (desde 0) es el bit (l-1-i) del código, invertido.
            lidos = 0
            for i in range(l):
                if not (c >> (l - 1 - i)) & 1:
                    lidos |= 1 << i
            for alto in range(1 << (cls.MAXBITS - l)):
                t[lidos | (alto << l)] = (sim, l)
        return t

    _tabs = None

    @classmethod
    def tabelas(cls):
        if cls._tabs is None:
            cls._tabs = (cls.tabela(cls.LITLEN), cls.tabela(cls.LENLEN), cls.tabela(cls.DISTLEN))
        return cls._tabs

    @classmethod
    def descomprimir(cls, dados, tamanho=None):
        tlit, tlen, tdist = cls.tabelas()
        base, extra = cls.BASE, cls.EXTRA
        pos = 0
        buf = 0
        cnt = 0
        nd = len(dados)
        out = bytearray()

        def encher(k):
            nonlocal pos, buf, cnt
            while cnt < k:
                b = dados[pos] if pos < nd else 0
                buf |= b << cnt
                cnt += 8
                pos += 1

        def bits(k):
            nonlocal buf, cnt
            if cnt < k:
                encher(k)
            v = buf & ((1 << k) - 1)
            buf >>= k
            cnt -= k
            return v

        def decodificar(t):
            nonlocal buf, cnt
            if cnt < 13:
                encher(13)
            sim, l = t[buf & 0x1FFF]
            buf >>= l
            cnt -= l
            return sim

        lit = bits(8)
        dic = bits(8)
        if lit > 1 or dic < 4 or dic > 6:
            raise ValueError("blast: cabecera no válida (%d, %d)" % (lit, dic))
        while True:
            if pos > nd + 4:
                raise ValueError("blast: datos truncados")
            if bits(1):
                s = decodificar(tlen)
                comp = base[s] + (bits(extra[s]) if extra[s] else 0)
                if comp == 519:
                    break
                k = 2 if comp == 2 else dic
                dist = (decodificar(tdist) << k) + bits(k) + 1
                if dist > len(out):
                    raise ValueError("blast: distancia fuera de la ventana")
                ini = len(out) - dist
                if dist >= comp:
                    out += out[ini:ini + comp]
                else:
                    for i in range(comp):
                        out.append(out[ini + i])
            else:
                out.append(decodificar(tlit) if lit else bits(8))
        if tamanho is not None and len(out) != tamanho:
            raise ValueError("blast: %d bytes en vez de %d" % (len(out), tamanho))
        return bytes(out)


class ArquivoIS3:
    """Un archivo de InstallShield 3 («SETUP.Z», firma 8C655D13): un índice
    de carpetas y ficheros al final, y cada fichero comprimido con blast."""

    FIRMA = 0x8C655D13

    def __init__(self, dados):
        self.d = dados
        firma, = struct.unpack_from("<I", dados, 0)
        if firma != self.FIRMA:
            raise ValueError("no es un archivo de InstallShield 3")
        nfich, = struct.unpack_from("<H", dados, 0x0C)
        indice, = struct.unpack_from("<I", dados, 0x29)
        npastas, = struct.unpack_from("<H", dados, 0x31)
        p = indice
        for _ in range(npastas):
            _, tam, _ = struct.unpack_from("<HHH", dados, p)
            p += tam
        self.fich = {}
        for _ in range(nfich):
            (_, _, descomp, comp, desl, _, _, tam, _, _, _, _, nl) = struct.unpack_from("<BHIIIIIHBBBBB", dados, p)
            nome = dados[p + 30:p + 30 + nl].decode("latin-1")
            self.fich[nome.upper()] = (desl, comp, descomp)
            p += tam

    def ler(self, nome):
        desl, comp, descomp = self.fich[nome.upper()]
        return Blast.descomprimir(self.d[desl:desl + comp], descomp)


def mb(n):
    return "%.1f MB" % (n / 1048576.0)


def escrever_se_mudou(destino, tamanho, gerar):
    """Escribe destino con gerar() (que da los bytes o copia el fichero) si
    no existe ya con ese tamaño."""
    if os.path.exists(destino) and os.path.getsize(destino) == tamanho:
        return
    tmp = destino + ".parcial"
    gerar(tmp)
    os.replace(tmp, destino)


def extrair(iso_gdi, destino, videos=False, musica=True, iso_nod=None):
    gdi = Iso(iso_gdi)
    raiz = gdi.raiz_por_nome()
    if "CONQUER.MIX" not in raiz or "INSTALL" not in raiz:
        raise SystemExit("cnc_extraer: %s (volumen %s) no es la ISO de C&C Gold para Windows 95 "
                         "(falta CONQUER.MIX o INSTALL/)" % (iso_gdi, gdi.volume))
    os.makedirs(destino, exist_ok=True)
    copiados = []

    def da_iso(iso, reg, nome_dest):
        d = os.path.join(destino, nome_dest)
        os.makedirs(os.path.dirname(d), exist_ok=True)
        escrever_se_mudou(d, reg["comp"], lambda tmp: iso.copiar_para(reg, tmp))
        copiados.append((nome_dest, reg["comp"], iso.volume))

    for nome in RAIZ_COMUM + ([MUSICA] if musica else []):
        da_iso(gdi, raiz[nome], nome)

    # El instalador: se descomprime solo si falta algo.
    inst = {r["nome"].upper(): r for r in gdi.listar_pasta(raiz["INSTALL"])}
    if "SETUP.Z" not in inst:
        raise SystemExit("cnc_extraer: %s no tiene INSTALL/SETUP.Z" % iso_gdi)
    arq = None
    for nome in DO_INSTALADOR:
        d = os.path.join(destino, nome)
        if arq is None:
            # Hay que abrir SETUP.Z para saber el tamaño (y para sacar el fichero).
            reg = inst["SETUP.Z"]
            gdi.f.seek(reg["ext"] * SECTOR)
            arq = ArquivoIS3(gdi.f.read(reg["comp"]))
        if nome.upper() not in arq.fich:
            continue
        tam = arq.fich[nome.upper()][2]
        escrever_se_mudou(d, tam, lambda tmp, n=nome: open(tmp, "wb").write(arq.ler(n)))
        copiados.append((nome, tam, gdi.volume + "/INSTALL/SETUP.Z"))

    discos = [("gdi", gdi)]
    if iso_nod:
        nod = Iso(iso_nod)
        if "GENERAL.MIX" not in nod.raiz_por_nome():
            raise SystemExit("cnc_extraer: %s (volumen %s) no parece la ISO de NOD" % (iso_nod, nod.volume))
        discos.append(("nod", nod))
    for pasta, iso in discos:
        r = iso.raiz_por_nome()
        for nome in POR_DISCO + ([VIDEOS] if videos else []):
            if nome in r:
                da_iso(iso, r[nome], pasta + "/" + nome)

    # Lo que ya no se pide (p. ej. al quitar --videos) se borra: los .MIX
    # que no están en la lista, aquí y en gdi/ y nod/.
    pedidos = {c[0] for c in copiados}
    for pasta in ["", "gdi", "nod"]:
        cp = os.path.join(destino, pasta)
        if not os.path.isdir(cp):
            continue
        for f in os.listdir(cp):
            rel = (pasta + "/" + f) if pasta else f
            if f.upper().endswith((".MIX", ".PARCIAL")) and rel not in pedidos:
                os.remove(os.path.join(cp, f))

    total = sum(c[1] for c in copiados)
    with open(os.path.join(destino, "LEEME-SAGUIOS.TXT"), "w", encoding="latin-1", newline="\r\n") as o:
        o.write("Command & Conquer (Tiberian Dawn) para Vanilla Conquer en SaguiOS\n")
        o.write("================================================================\n\n")
        o.write("Copiado por tools/cnc_extraer.py de las ISO de C&C Gold para Windows 95\n")
        o.write("(%s). Son datos de Electronic Arts / Westwood\n" % ", ".join(i.volume for _, i in discos))
        o.write("Studios: no son libres y no van en las imagenes publicadas del SaguiOS.\n\n")
        for nome, comp, vol in copiados:
            o.write("  %-18s %10d  (%s)\n" % (nome, comp, vol))
        o.write("\nTotal: %s\n" % mb(total))
        if not videos:
            o.write("Sin las peliculas (MOVIES.MIX): el juego se salta las escenas.\n")
        if not musica:
            o.write("Sin la musica (SCORES.MIX).\n")
        if not iso_nod:
            o.write("Sin la ISO de NOD: solo la campana de GDI (gdi/GENERAL.MIX).\n")
    return copiados, total


def buscar_isos(pasta):
    """Las ISO de una carpeta, reconocidas por su volumen (GDI95 / NOD95)."""
    gdi = nod = None
    for f in sorted(os.listdir(pasta)):
        if not f.lower().endswith(".iso"):
            continue
        c = os.path.join(pasta, f)
        try:
            v = Iso(c).volume.upper()
        except (ValueError, OSError):
            continue
        if v.startswith("GDI") and not gdi:
            gdi = c
        elif v.startswith("NOD") and not nod:
            nod = c
    return gdi, nod


def comprobar():
    """Pruebas del descompresor con el ejemplo de blast.c (zlib/contrib):
    los bytes 00 04 82 24 25 8f 80 7f dan «AIAIAIAIAIAIA»."""
    ok = Blast.descomprimir(bytes([0x00, 0x04, 0x82, 0x24, 0x25, 0x8f, 0x80, 0x7f])) == b"AIAIAIAIAIAIA"
    print("cnc_extraer: descompresor %s" % ("bien" if ok else "MAL"))
    return 0 if ok else 1


def main(argv):
    args = list(argv[1:])
    if not args or args[0] in ("-h", "--help", "--ayuda"):
        print(__doc__)
        return 0
    if args[0] == "--comprobar":
        return comprobar()
    if args[0] == "--listar":
        iso = Iso(args[1])
        print("volumen %s" % iso.volume)
        for caminho, r in iso.arvore():
            print("%10d  %s" % (0 if r["pasta"] else r["comp"], caminho))
        return 0
    videos = "--videos" in args
    musica = "--sin-musica" not in args
    args = [a for a in args if a not in ("--videos", "--sin-musica")]
    if args[0] == "--buscar":
        gdi, nod = buscar_isos(args[1])
        if not gdi:
            print("cnc_extraer: no hay ISO de C&C (volumen GDI95) en %s" % args[1], file=sys.stderr)
            return 1
        destino = args[2]
    elif len(args) == 3:
        gdi, nod, destino = args
    else:
        gdi, destino = args
        nod = None
    copiados, total = extrair(gdi, destino, videos, musica, nod)
    print("cnc_extraer: %d ficheros, %s en %s" % (len(copiados), mb(total), destino))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
