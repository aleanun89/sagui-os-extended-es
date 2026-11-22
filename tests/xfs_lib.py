# SPDX-License-Identifier: GPL-3.0-only
"""xfs_lib.py - imágenes XFS para las pruebas (D446), hechas en el PC con mkfs.xfs (xfsprogs).

- `crear(caminho, mb, opcoes, arvore)`: formatea con mkfs.xfs y llena el volumen con un
  fichero «proto» (mkfs.xfs -p), sin montar nada (no hace falta root). `arvore` es un dict
  {nombre: contenido}: bytes = fichero, dict = carpeta, Ligacao("destino") = enlace simbólico.
- `fragmentar(caminho, ruta, plano)`: rehace la lista de extents de un fichero que mkfs dejó en
  un solo tramo, para tener muchos extents (y árbol bmbt si no caben en el inodo), huecos y
  extents sin escribir. Usa solo los bloques que ya eran del fichero (no cambia el espacio libre,
  así que xfs_repair -n sigue limpio si el volumen no tiene rmapbt) y devuelve el contenido que
  hay que leer. La lectura independiente la hacen xfs_db (bmap) y xfs_repair -n en la prueba.
- `diario_sucio(caminho)`: el registro de desmontar del diario deja de serlo (como tras un
  apagado incorrecto) y se recalcula su suma.
- `xfs_db(caminho, *ordenes)`, `repair(caminho)`, `forma_pasta(caminho, ruta)`.

Escrito para SaguiOS a partir de la especificación pública del formato; no usa código de xfsprogs.
"""
import os
import shutil
import struct
import subprocess

_T = []
for _i in range(256):
    _c = _i
    for _ in range(8):
        _c = (_c >> 1) ^ 0x82F63B78 if _c & 1 else _c >> 1
    _T.append(_c)


def crc32c(dados, crc=0xFFFFFFFF):
    for b in dados:
        crc = _T[(crc ^ b) & 0xFF] ^ (crc >> 8)
    return crc


def selar(buf, desl):
    """Pone la suma CRC32C de XFS (invertida, little-endian) en buf[desl:desl+4]."""
    buf[desl:desl + 4] = b"\0\0\0\0"
    struct.pack_into("<I", buf, desl, ~crc32c(buf) & 0xFFFFFFFF)


def da_hash(nome):
    rol = lambda x, n: ((x << n) | (x >> (32 - n))) & 0xFFFFFFFF  # noqa: E731
    h, n = 0, nome
    while len(n) >= 4:
        h = (n[0] << 21) ^ (n[1] << 14) ^ (n[2] << 7) ^ n[3] ^ rol(h, 28)
        n = n[4:]
    if len(n) == 3:
        return ((n[0] << 14) ^ (n[1] << 7) ^ n[2] ^ rol(h, 21)) & 0xFFFFFFFF
    if len(n) == 2:
        return ((n[0] << 7) ^ n[1] ^ rol(h, 14)) & 0xFFFFFFFF
    if len(n) == 1:
        return (n[0] ^ rol(h, 7)) & 0xFFFFFFFF
    return h & 0xFFFFFFFF


class Ligacao:
    def __init__(self, destino):
        self.destino = destino if isinstance(destino, bytes) else destino.encode("utf-8")


def hay_xfsprogs():
    return all(shutil.which(x) for x in ("mkfs.xfs", "xfs_db", "xfs_repair"))


def _nome(n):
    return n if isinstance(n, bytes) else n.encode("utf-8")


def crear(caminho, mb, opcoes, arvore, rotulo=None):
    """Formatea "caminho" (imagen dispersa de "mb" MB) con mkfs.xfs y el contenido de "arvore"."""
    datos = caminho + ".datos"
    shutil.rmtree(datos, ignore_errors=True)
    os.makedirs(datos)
    linhas = [b"/sagui-no-usado", b"0 0", b"d--755 0 0"]
    cont = [0]

    def fichero(conteudo):
        cont[0] += 1
        p = os.path.join(datos, str(cont[0]))
        with open(p, "wb") as f:
            f.write(conteudo)
        return p.encode()

    compartidos = {}
    ligacoes = []

    def pasta(d, nivel, ruta):
        for nome, x in d.items():
            n = _nome(nome)
            if b" " in n or b"\n" in n:
                raise ValueError("los nombres del proto no pueden llevar espacios")
            if isinstance(x, dict):
                linhas.append(n + b" d--755 1000 1000")
                pasta(x, nivel + 1, ruta + "/" + n.decode("utf-8"))
                linhas.append(b"$")
            elif isinstance(x, Ligacao):
                linhas.append(n + b" l--777 1000 1000 " + x.destino)
                ligacoes.append((ruta + "/" + n.decode("utf-8"), x.destino))
            else:
                modo = b"---755" if n.endswith(b".sh") else b"---644"
                # el mismo contenido, el mismo fichero del PC (10 000 ficheros iguales no hacen 10 000 copias)
                if x not in compartidos:
                    compartidos[x] = fichero(x)
                linhas.append(n + b" " + modo + b" 1000 1000 " + compartidos[x])
    pasta(arvore, 0, "")
    linhas.append(b"$")
    proto = caminho + ".proto"
    with open(proto, "wb") as f:
        f.write(b"\n".join(linhas) + b"\n")
    if os.path.exists(caminho):
        os.remove(caminho)
    with open(caminho, "wb") as f:
        f.truncate(mb * 1024 * 1024)
    cmd = ["mkfs.xfs", "-q", "-f"] + list(opcoes) + (["-L", rotulo] if rotulo else []) + ["-p", proto, caminho]
    subprocess.run(cmd, check=True, capture_output=True)
    shutil.rmtree(datos, ignore_errors=True)
    os.remove(proto)
    for ruta, destino in ligacoes:
        selar_ligacao(caminho, ruta, destino)


def selar_ligacao(caminho, ruta, destino):
    """El mkfs.xfs 6.6 escribe los enlaces simbólicos en bloques (remotos) de un v5 sin la
    cabecera XSLM ni su suma (xfs_repair -n lo marca). Se pone aquí, según el formato del disco:
    magia, desplazamiento, bytes, suma, uuid, dueño, bloque (en sectores) y lsn; después, el destino.
    xfs_repair -n comprueba luego que quedó bien."""
    x = Img(caminho)
    if not x.v5:
        x.fechar()
        return
    c = campos(caminho, f"path {ruta}", "p core.format")
    if not c.get("core.format", "").startswith("2"):
        x.fechar()
        return
    ino = numero_inodo(caminho, ruta)
    bm = xfs_db(caminho, f"inode {ino}", "bmap").split()
    fsb, n = int(bm[bm.index("startblock") + 1]), int(bm[bm.index("count") + 1])
    if len(destino) > n * x.bs - 56:
        x.fechar()
        raise ValueError(f"{ruta}: el destino no cabe en {n} bloque(s) con la cabecera v5")
    b = bytearray(n * x.bs)
    struct.pack_into(">III", b, 0, 0x58534C4D, 0, len(destino))
    b[16:32] = x.uuid
    struct.pack_into(">QQQ", b, 32, ino, x.fsb_byte(fsb) // 512, 0)
    b[56:56 + len(destino)] = destino
    selar(b, 12)
    x.gravar(x.fsb_byte(fsb), bytes(b))
    x.fechar()


def xfs_db(caminho, *ordenes, escrita=False):
    cmd = ["xfs_db"] + (["-x"] if escrita else ["-r"])
    for o in ordenes:
        cmd += ["-c", o]
    r = subprocess.run(cmd + [caminho], capture_output=True, text=True, errors="replace")
    return r.stdout + r.stderr


def repair(caminho):
    """xfs_repair -n: (código, salida). 0 = limpio."""
    r = subprocess.run(["xfs_repair", "-n", "-f", caminho], capture_output=True, text=True, errors="replace")
    return r.returncode, r.stdout + r.stderr


def campos(caminho, *ordenes):
    """Los «campo = valor» que imprime xfs_db."""
    d = {}
    for l in xfs_db(caminho, *ordenes).splitlines():
        if " = " in l:
            k, v = l.split(" = ", 1)
            d[k.strip()] = v.strip()
    return d


def forma_pasta(caminho, ruta):
    """La forma de una carpeta: shortform, block, leaf, node (con extents) o btree (node con bmbt)."""
    c = campos(caminho, f"path {ruta}", "p core.format core.size")
    fmt = c.get("core.format", "")
    if fmt.startswith("1"):
        return "shortform"
    sb = campos(caminho, "sb 0", "p blocksize dirblklog")
    bs, dbl = int(sb["blocksize"]), int(sb["dirblklog"])
    if int(c["core.size"]) == bs << dbl:
        return "block"
    folha = (1 << 35) // bs
    t = xfs_db(caminho, f"path {ruta}", f"dblock {folha}", "p")
    if "0x3df1" in t or "0xd2f1" in t:
        return "leaf"
    return "btree" if fmt.startswith("3") else "node"


class Img:
    """Lo justo para editar inodos de una imagen XFS (big-endian)."""

    def __init__(self, caminho):
        self.caminho = caminho
        self.f = open(caminho, "r+b")
        s = self.ler(0, 512)
        (self.bs,) = struct.unpack(">I", s[4:8])
        self.agblocks, self.agcount = struct.unpack(">II", s[84:92])
        self.versao = struct.unpack(">H", s[100:102])[0] & 0xF
        (self.isz,) = struct.unpack(">H", s[104:106])
        self.blocklog, self.inodelog, self.inopblog, self.agblklog = s[120], s[122], s[123], s[124]
        self.v5 = self.versao == 5
        inc = struct.unpack(">I", s[216:220])[0] if self.v5 else 0
        self.uuid = s[248:264] if inc & 4 else s[32:48]
        self.logstart = struct.unpack(">Q", s[48:56])[0]
        self.logblocks = struct.unpack(">I", s[96:100])[0]

    def ler(self, off, n):
        self.f.seek(off)
        return self.f.read(n)

    def gravar(self, off, dados):
        self.f.seek(off)
        self.f.write(dados)

    def fsb_byte(self, fsb):
        ag, b = fsb >> self.agblklog, fsb & ((1 << self.agblklog) - 1)
        return (ag * self.agblocks + b) * self.bs

    def ino_byte(self, ino):
        ag = ino >> (self.agblklog + self.inopblog)
        b = (ino >> self.inopblog) & ((1 << self.agblklog) - 1)
        i = ino & ((1 << self.inopblog) - 1)
        return (ag * self.agblocks + b) * self.bs + i * self.isz

    def fechar(self):
        self.f.close()


def numero_inodo(caminho, ruta):
    """El número de inodo de "ruta", de «xfs_db ls» (posición, inodo, tipo, hash, longitud, nombre)."""
    pasta, nome = ruta.rsplit("/", 1)
    for l in xfs_db(caminho, f"ls {pasta or '/'}").splitlines():
        p = l.split()
        if len(p) >= 6 and p[5] == nome:
            return int(p[1])
    raise ValueError(f"no se encuentra {ruta}")


def _ext(ini, bloco, n, sem_escrever=False):
    l0 = (1 << 63 if sem_escrever else 0) | (ini << 9) | (bloco >> 43)
    l1 = ((bloco & ((1 << 43) - 1)) << 21) | n
    return struct.pack(">QQ", l0, l1)


def fragmentar(caminho, ruta, plano, conteudo_de, buraco_final=0):
    """Rehace la lista de extents de "ruta" (un fichero de un solo tramo hecho por mkfs).

    plano: lista de (hueco_antes, bloques, sin_escribir) en bloques. Los bloques físicos son los
    del tramo original, repartidos al revés (el primer extent del fichero es el último del disco)
    para que ninguno quede seguido de otro. Si la lista no cabe en el inodo se hace un bmbt de un
    nivel con las hojas necesarias (tomadas también del tramo). "conteudo_de(i)" da el bloque i
    del fichero (bytes de un bloque). "buraco_final": bloques de hueco al final (di_size más allá
    del último extent). Devuelve (contenido esperado, número de extents, inodo)."""
    ino = numero_inodo(caminho, ruta)
    bm = xfs_db(caminho, f"inode {ino}", "bmap").split()
    # «data offset 0 startblock 24 (0/24) count 2401 flag 0»
    ini_fis, total = int(bm[bm.index("startblock") + 1]), int(bm[bm.index("count") + 1])
    x = Img(caminho)
    bs = x.bs
    nucleo = 176 if x.v5 else 100
    off_ino = x.ino_byte(ino)
    inode = bytearray(x.ler(off_ino, x.isz))
    forkoff = inode[82] * 8
    fork_tam = forkoff or (x.isz - nucleo)
    nrec = len(plano)
    cab = 72 if x.v5 else 24
    por_folha = (bs - cab) // 16
    cabe_no_inodo = nrec <= fork_tam // 16
    nfolhas = 0 if cabe_no_inodo else -(-nrec // por_folha)
    dados_blocos = sum(p[1] for p in plano)
    if dados_blocos + nfolhas > total:
        raise ValueError(f"el plano pide {dados_blocos + nfolhas} bloques y el fichero tiene {total}")
    # los bloques que sobran del tramo se quedan como último extent (escrito), para no liberar nada
    sobra = total - dados_blocos - nfolhas
    if sobra:
        plano = list(plano) + [(0, sobra, False)]
        nrec += 1
        if not cabe_no_inodo:
            nfolhas = -(-nrec // por_folha)
    if cabe_no_inodo and nrec > fork_tam // 16:
        raise ValueError("el plano ya no cabe en el inodo")
    fis_folhas = [ini_fis + i for i in range(nfolhas)]
    livre = ini_fis + nfolhas
    # reparto al revés: el extent k usa los bloques físicos desde el final del tramo
    fim = ini_fis + total
    recs, esperado, log = [], bytearray(), 0
    for buraco, n, sem in plano:
        log += buraco
        esperado += b"\0" * (buraco * bs)
        fim -= n
        fis = fim
        recs.append((log, fis, n, sem))
        for k in range(n):
            bloco = conteudo_de(log + k)
            x.gravar(x.fsb_byte(fis + k), bloco if not sem else bytes([0xA5]) * bs)
            esperado += b"\0" * bs if sem else bloco
        log += n
    assert fim == livre, (fim, livre)
    esperado += b"\0" * (buraco_final * bs)
    tam = len(esperado) - 100   # el último bloque no entero
    esperado = bytes(esperado[:tam])
    struct.pack_into(">Q", inode, 56, tam)                    # di_size
    struct.pack_into(">I", inode, 76, nrec)                   # di_nextents
    fork = bytearray(fork_tam)
    if cabe_no_inodo:
        inode[5] = 2
        for i, (lo, fi, n, sem) in enumerate(recs):
            fork[i * 16:(i + 1) * 16] = _ext(lo, fi, n, sem)
    else:
        inode[5] = 3
        maxr = (fork_tam - 4) // 16
        struct.pack_into(">HH", fork, 0, 1, nfolhas)
        for k in range(nfolhas):
            parte = recs[k * por_folha:(k + 1) * por_folha]
            struct.pack_into(">Q", fork, 4 + k * 8, parte[0][0])
            struct.pack_into(">Q", fork, 4 + maxr * 8 + k * 8, fis_folhas[k])
            b = bytearray(bs)
            esq = fis_folhas[k - 1] if k else 0xFFFFFFFFFFFFFFFF
            dirt = fis_folhas[k + 1] if k + 1 < nfolhas else 0xFFFFFFFFFFFFFFFF
            struct.pack_into(">IHHQQ", b, 0, 0x424D4133 if x.v5 else 0x424D4150, 0, len(parte), esq, dirt)
            if x.v5:
                struct.pack_into(">QQ", b, 24, x.fsb_byte(fis_folhas[k]) // 512, 0)
                b[40:56] = x.uuid
                struct.pack_into(">Q", b, 56, ino)
            for i, (lo, fi, n, sem) in enumerate(parte):
                b[cab + i * 16:cab + (i + 1) * 16] = _ext(lo, fi, n, sem)
            if x.v5:
                selar(b, 64)
            x.gravar(x.fsb_byte(fis_folhas[k]), bytes(b))
    inode[nucleo:nucleo + fork_tam] = fork
    if x.v5:
        selar(inode, 100)
    x.gravar(off_ino, bytes(inode))
    x.fechar()
    return esperado, nrec, ino


def diario_sucio(caminho):
    """El último registro del diario (el de desmontar que deja mkfs) pasa a ser uno normal: el
    diario ya no está limpio, como tras un apagado incorrecto. Se recalcula la suma del registro."""
    x = Img(caminho)
    base = x.fsb_byte(x.logstart)
    cab = bytearray(x.ler(base, 512))
    assert struct.unpack(">I", cab[:4])[0] == 0xFEEDBABE, "el diario no empieza por un registro"
    h_len = struct.unpack(">I", cab[12:16])[0]
    dados = bytearray(x.ler(base + 512, h_len))
    assert dados[9] & 0x20, "el primer registro no es el de desmontar"
    dados[9] &= ~0x20 & 0xFF
    x.gravar(base + 512, bytes(dados))
    # h_crc (como las demás sumas de XFS): la cabecera y los datos con la primera palabra de cada bloque
    if struct.unpack("<I", cab[32:36])[0]:
        cab[32:36] = b"\0\0\0\0"
        c = crc32c(bytes(cab[:512]))
        restaurado = bytearray(dados)
        for i in range(0, len(restaurado), 512):
            restaurado[i:i + 4] = cab[44 + (i // 512) * 4:48 + (i // 512) * 4]
        c = crc32c(bytes(restaurado), c)
        struct.pack_into("<I", cab, 32, ~c & 0xFFFFFFFF)
        x.gravar(base, bytes(cab))
    x.fechar()
