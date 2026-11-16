# SPDX-License-Identifier: GPL-2.0-or-later
"""exfat_lib - lector y escritor mínimo de exFAT en Python para las pruebas (D325).

Hecho con la especificación pública de exFAT de Microsoft (2019), sin código de otros
sistemas. Sirve para tres cosas que el anfitrión no trae hechas:
  - poblar un volumen creado con mkfs.exfat (carpetas anidadas, nombres largos con
    acentos, un archivo fragmentado), porque no hay exfat-fuse ni montaje sin root;
  - leer lo que SaguiOS escribió y comparar el contenido byte a byte;
  - comprobar sumas (arranque, grupo de entradas, tabla de mayúsculas) y fechas.
fsck.exfat -n sigue siendo el juez de la estructura; esto es el juez del contenido.
"""
import struct
import time

EOC = 0xFFFFFFFF


def suma_arranque(datos):
    """Suma de verificación de la región de arranque (sectores 0-10), sin VolumeFlags ni PercentInUse."""
    s = 0
    for i, b in enumerate(datos):
        if i in (106, 107, 112):
            continue
        s = (((s & 1) << 31) + (s >> 1) + b) & 0xFFFFFFFF
    return s


def suma_grupo(g):
    """SetChecksum de un grupo de entradas (sin los bytes 2 y 3 de la primera)."""
    s = 0
    for i, b in enumerate(g):
        if i in (2, 3):
            continue
        s = (((s & 1) << 15) + (s >> 1) + b) & 0xFFFF
    return s


def suma_tabla(datos):
    s = 0
    for b in datos:
        s = (((s & 1) << 31) + (s >> 1) + b) & 0xFFFFFFFF
    return s


def desc_tabla(datos):
    """Tabla de mayúsculas (comprimida o no) -> lista de 65536 entradas."""
    t = list(range(65536))
    e = struct.unpack("<%dH" % (len(datos) // 2), datos)
    pos = i = 0
    while i < len(e) and pos < 65536:
        if e[i] == 0xFFFF and i + 1 < len(e):
            pos += e[i + 1]
            i += 2
        else:
            t[pos] = e[i]
            pos += 1
            i += 1
    return t


class Entrada:
    def __init__(self):
        self.nome = ""
        self.attr = 0
        self.primeiro = 0
        self.tam = 0
        self.vdl = 0
        self.sem_cadeia = False
        self.idx = 0          # entrada File dentro de la carpeta
        self.qtd = 0
        self.mod = 0          # marca de tiempo de modificación (32 bits exFAT)
        self.crea = 0
        self.mod_desv = 0
        self.mod_cs = 0       # D341: centésimas de la modificación (0..199)

    @property
    def eh_pasta(self):
        return bool(self.attr & 0x10)


class ExFat:
    def __init__(self, caminho, desloc=0, escrita=False):
        self.f = open(caminho, "r+b" if escrita else "rb")
        self.off = desloc
        b = self.ler(0, 512)
        assert b[3:11] == b"EXFAT   ", "no es exFAT"
        self.bps = 1 << b[108]
        self.spc = 1 << b[109]
        self.bpc = self.bps * self.spc
        self.fat_off, self.fat_len, self.heap_off, self.n_cl, self.raiz = struct.unpack_from("<IIIII", b, 80)
        self.vol_len = struct.unpack_from("<Q", b, 72)[0]
        self.flags = struct.unpack_from("<H", b, 106)[0]
        self.n_fats = b[110]
        self.serial = struct.unpack_from("<I", b, 100)[0]
        self.erros = []
        self.verificar_arranque()
        self.fat = list(struct.unpack("<%dI" % (self.n_cl + 2),
                                      self.ler(self.fat_off * self.bps, (self.n_cl + 2) * 4)))
        self.bitmap = None
        self.maiusc = None
        self.rotulo = ""
        self._ler_raiz()

    # --- E/S bruta
    def ler(self, pos, n):
        self.f.seek(self.off + pos)
        return self.f.read(n)

    def gravar(self, pos, dados):
        self.f.seek(self.off + pos)
        self.f.write(dados)

    def pos_cluster(self, c):
        return self.heap_off * self.bps + (c - 2) * self.bpc

    def verificar_arranque(self):
        r = self.ler(0, 11 * self.bps)
        s = suma_arranque(r)
        c = self.ler(11 * self.bps, self.bps)
        esperado = struct.pack("<I", s) * (self.bps // 4)
        if c != esperado:
            self.erros.append("suma de la región de arranque mala")
        return c == esperado

    def sujo(self):
        b = self.ler(106, 2)
        return bool(struct.unpack("<H", b)[0] & 2)

    def poner_sujo(self, sujo=True):
        v = struct.unpack("<H", self.ler(106, 2))[0]
        v = v | 2 if sujo else v & ~2
        self.gravar(106, struct.pack("<H", v))

    # --- cadenas
    def cadeia(self, primeiro, tam, sem_cadeia):
        n = (tam + self.bpc - 1) // self.bpc
        if not primeiro or not n:
            return []
        if sem_cadeia:
            return list(range(primeiro, primeiro + n))
        out, c = [], primeiro
        while c >= 2 and c != EOC and len(out) < self.n_cl + 1 and c < len(self.fat):
            out.append(c)
            c = self.fat[c]
        return out

    def ler_fluxo(self, primeiro, tam, sem_cadeia, vdl=None):
        dados = bytearray()
        for c in self.cadeia(primeiro, tam, sem_cadeia):
            dados += self.ler(self.pos_cluster(c), self.bpc)
        dados = bytes(dados[:tam])
        if vdl is not None and vdl < tam:
            dados = dados[:vdl] + b"\0" * (tam - vdl)
        return dados

    # --- raíz: mapa de bits, mayúsculas, etiqueta
    def _ler_raiz(self):
        for e in self._entradas_cruas(self.raiz, self.n_cl * 0 + self._tam_cadeia(self.raiz), False):
            t = e[0]
            if t == 0x83:
                self.rotulo = e[2:2 + e[1] * 2].decode("utf-16-le")
            elif t == 0x81 and self.bitmap is None:
                c, ln = struct.unpack_from("<IQ", e, 20)
                self.bm_cluster = c
                self.bm_len = ln
                self.bitmap = bytearray(self.ler(self.pos_cluster(c), ln))
            elif t == 0x82:
                cs, c, ln = struct.unpack_from("<I", e, 4)[0], *struct.unpack_from("<IQ", e, 20)
                dados = self.ler(self.pos_cluster(c), ln)
                if suma_tabla(dados) != cs:
                    self.erros.append("suma de la tabla de mayúsculas mala")
                self.maiusc = desc_tabla(dados)
        if self.maiusc is None:
            self.maiusc = list(range(65536))

    def _tam_cadeia(self, primeiro):
        return len(self.cadeia(primeiro, 1 << 40, False)) * self.bpc

    def _entradas_cruas(self, primeiro, tam, sem_cadeia):
        d = self.ler_fluxo(primeiro, tam, sem_cadeia)
        for i in range(0, len(d), 32):
            yield d[i:i + 32]

    def hash_nome(self, nome):
        h = 0
        for ch in nome:
            c = self.maiusc[ord(ch)] if ord(ch) < 65536 else ord(ch)
            for b in struct.pack("<H", c):
                h = (((h & 1) << 15) + (h >> 1) + b) & 0xFFFF
        return h

    # --- carpetas
    def listar(self, primeiro, tam, sem_cadeia):
        """Entradas de una carpeta. Falla (erros) si alguna suma no cuadra."""
        d = self.ler_fluxo(primeiro, tam, sem_cadeia)
        i = 0
        out = []
        while i * 32 + 32 <= len(d):
            e = d[i * 32:i * 32 + 32]
            if e[0] == 0:
                break
            if e[0] == 0x85:
                n = 1 + e[1]
                g = d[i * 32:(i + n) * 32]
                if struct.unpack_from("<H", g, 2)[0] != suma_grupo(g):
                    self.erros.append("SetChecksum malo en la entrada %d" % i)
                else:
                    x = Entrada()
                    x.attr = struct.unpack_from("<H", g, 4)[0]
                    x.crea, x.mod = struct.unpack_from("<II", g, 8)[0], struct.unpack_from("<I", g, 12)[0]
                    x.mod_desv = g[23]
                    x.mod_cs = g[21]

                    s = g[32:64]
                    x.sem_cadeia = bool(s[1] & 2)
                    nl = s[3]
                    x.vdl, x.primeiro, x.tam = struct.unpack_from("<Q", s, 8)[0], struct.unpack_from("<I", s, 20)[0], struct.unpack_from("<Q", s, 24)[0]
                    if struct.unpack_from("<H", s, 4)[0] != self.hash_nome("".join(
                            chr(c) for c in struct.unpack("<%dH" % nl, b"".join(g[64 + k * 32 + 2:64 + k * 32 + 32] for k in range(n - 2))[:nl * 2]))):
                        self.erros.append("NameHash malo en la entrada %d" % i)
                    nome = b"".join(g[64 + k * 32 + 2:64 + k * 32 + 32] for k in range(n - 2))[:nl * 2]
                    x.nome = nome.decode("utf-16-le")
                    x.idx, x.qtd = i, n
                    out.append(x)
                i += n
            else:
                i += 1
        return out

    def listar_raiz(self):
        return self.listar(self.raiz, self._tam_cadeia(self.raiz), False)

    def listar_pasta(self, e):
        return self.listar(e.primeiro, e.tam, e.sem_cadeia)

    def achar(self, caminho):
        if caminho in ("", "/"):
            return None
        atual = self.listar_raiz()
        partes = [p for p in caminho.split("/") if p]
        for k, p in enumerate(partes):
            m = [e for e in atual if self.hash_nome(e.nome) == self.hash_nome(p) and
                 [self.maiusc[ord(c)] for c in e.nome] == [self.maiusc[ord(c)] for c in p]]
            if not m:
                return False
            if k == len(partes) - 1:
                return m[0]
            atual = self.listar_pasta(m[0])
        return False

    def ler_arq(self, caminho):
        e = self.achar(caminho)
        assert e, "no existe: " + caminho
        return self.ler_fluxo(e.primeiro, e.tam, e.sem_cadeia, e.vdl)

    def arvore(self):
        """{ruta: bytes (archivos) o None (carpetas)} de todo el volumen."""
        out = {}

        def rec(ents, pre):
            for e in ents:
                r = pre + "/" + e.nome
                if e.eh_pasta:
                    out[r] = None
                    rec(self.listar_pasta(e), r)
                else:
                    out[r] = self.ler_fluxo(e.primeiro, e.tam, e.sem_cadeia, e.vdl)
        rec(self.listar_raiz(), "")
        return out

    def livres(self):
        n = 0
        for i in range(self.n_cl):
            if not (self.bitmap[i >> 3] >> (i & 7)) & 1:
                n += 1
        return n

    # --- escritura (solo para poblar volúmenes de prueba)
    def _alocar(self, c):
        i = c - 2
        self.bitmap[i >> 3] |= 1 << (i & 7)

    def _livre(self, c):
        i = c - 2
        return not (self.bitmap[i >> 3] >> (i & 7)) & 1

    def _achar_livres(self, n, passo=1):
        out, c = [], 2
        while len(out) < n and c < self.n_cl + 2:
            if self._livre(c):
                out.append(c)
                c += passo
            else:
                c += 1
        assert len(out) == n, "volumen lleno"
        return out

    def _gravar_fat(self):
        self.gravar(self.fat_off * self.bps, struct.pack("<%dI" % len(self.fat), *self.fat))
        self.gravar(self.pos_cluster(self.bm_cluster), bytes(self.bitmap))

    def _alocar_cadeia(self, n, fragmentado=False):
        cl = self._achar_livres(n, 2 if fragmentado else 1)
        for c in cl:
            self._alocar(c)
        return cl

    def _ts(self, t=None):
        t = time.localtime(t if t is not None else time.time())
        return ((t.tm_year - 1980) << 25) | (t.tm_mon << 21) | (t.tm_mday << 16) | (t.tm_hour << 11) | (t.tm_min << 5) | (t.tm_sec // 2)

    def _grupo(self, nome, attr, primeiro, tam, sem_cadeia, t=None):
        u = nome.encode("utf-16-le")
        nl = len(u) // 2
        partes = (nl + 14) // 15
        n = 2 + partes
        g = bytearray(n * 32)
        ts = self._ts(t)
        g[0] = 0x85
        g[1] = n - 1
        struct.pack_into("<H", g, 4, attr)
        struct.pack_into("<III", g, 8, ts, ts, ts)
        g[22] = g[23] = g[24] = 0x80
        g[32] = 0xC0
        g[33] = 1 | (2 if sem_cadeia else 0)
        g[35] = nl
        struct.pack_into("<H", g, 36, self.hash_nome(nome))
        struct.pack_into("<Q", g, 40, tam)
        struct.pack_into("<I", g, 52, primeiro)
        struct.pack_into("<Q", g, 56, tam)
        for k in range(partes):
            g[64 + k * 32] = 0xC1
            ch = u[k * 30:(k + 1) * 30]
            g[64 + k * 32 + 2:64 + k * 32 + 2 + len(ch)] = ch
        struct.pack_into("<H", g, 2, suma_grupo(g))
        return bytes(g)

    def _pasta_de(self, caminho):
        """(primeiro, tam, sem_cadeia, entrada) de la carpeta "caminho"."""
        if caminho in ("", "/"):
            return self.raiz, self._tam_cadeia(self.raiz), False, None
        e = self.achar(caminho)
        assert e and e.eh_pasta, "no es carpeta: " + caminho
        return e.primeiro, e.tam, e.sem_cadeia, e

    def _inserir(self, pasta, grupo):
        primeiro, tam, sem_cadeia, ent = self._pasta_de(pasta)
        n = len(grupo) // 32
        d = bytearray(self.ler_fluxo(primeiro, tam, sem_cadeia))
        i = 0
        while i * 32 < len(d):
            if d[i * 32] == 0:
                break
            i += 1
        # n entradas libres desde la primera marca de fin
        cl = self.cadeia(primeiro, tam, sem_cadeia)
        while (i + n) * 32 > len(cl) * self.bpc:
            assert not sem_cadeia, "carpeta contigua llena: no se amplía en la prueba"
            novo = self._alocar_cadeia(1)[0]
            self.fat[cl[-1]] = novo
            self.fat[novo] = EOC
            self.gravar(self.pos_cluster(novo), b"\0" * self.bpc)
            cl.append(novo)
            if ent is not None:
                raise AssertionError("ampliar subcarpetas no soportado en la prueba")
        pos = i * 32
        for k in range(n):
            c = cl[(pos + k * 32) // self.bpc]
            self.gravar(self.pos_cluster(c) + (pos + k * 32) % self.bpc, grupo[k * 32:(k + 1) * 32])
        self._gravar_fat()

    def mkdir(self, caminho):
        pai, _, nome = caminho.rpartition("/")
        c = self._alocar_cadeia(1)[0]
        self.gravar(self.pos_cluster(c), b"\0" * self.bpc)
        self._inserir(pai, self._grupo(nome, 0x10, c, self.bpc, True))

    def add_arq(self, caminho, dados, fragmentado=False, sem_cadeia=None, t=None):
        pai, _, nome = caminho.rpartition("/")
        n = (len(dados) + self.bpc - 1) // self.bpc
        cl = self._alocar_cadeia(n, fragmentado) if n else []
        contiguo = all(cl[i] + 1 == cl[i + 1] for i in range(len(cl) - 1))
        if sem_cadeia is None:
            sem_cadeia = contiguo
        assert not sem_cadeia or contiguo
        for i, c in enumerate(cl):
            if not sem_cadeia:
                self.fat[c] = cl[i + 1] if i + 1 < len(cl) else EOC
            ch = dados[i * self.bpc:(i + 1) * self.bpc]
            self.gravar(self.pos_cluster(c), ch + b"\0" * (self.bpc - len(ch)))
        self._inserir(pai, self._grupo(nome, 0x20, cl[0] if cl else 0, len(dados), sem_cadeia and bool(cl), t))

    def add_arq_esparso(self, caminho, tam, marcas):
        """Un archivo contiguo (sin cadena) de "tam" bytes en el que solo se escriben las "marcas"
        {posición: bytes}; el resto es el hueco disperso de la imagen (ceros)."""
        pai, _, nome = caminho.rpartition("/")
        n = (tam + self.bpc - 1) // self.bpc
        cl = self._achar_livres(n)
        for c in cl:
            self._alocar(c)
        for pos, dados in marcas.items():
            self.gravar(self.pos_cluster(cl[0]) + pos, dados)
        self._inserir(pai, self._grupo(nome, 0x20, cl[0], tam, True))
