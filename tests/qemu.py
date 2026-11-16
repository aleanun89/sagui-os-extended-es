# SPDX-License-Identifier: GPL-3.0-only
"""
qemu.py - controle do QEMU para os testes automáticos do SaguiOS.

Cada teste cria uma Maquina, que:
  - liga o QEMU sem janela (-display none), sem KVM;
  - manda a serial COM1 para um arquivo (o log do kernel);
  - abre o monitor do QEMU num socket Unix, usado para enviar teclas, mover o
    mouse, ler a memória e tirar capturas de tela (screendump -> PNG).

Três modos de boot (PRD 3.3, D077):
  "ide" - a imagem como disco IDE;
  "usb" - a imagem como pendrive num controlador EHCI (simula o G470);
  "cd"  - a ISO (build/saguios.iso) como CD-ROM IDE, arrancando por ele.

Tempo máximo de qualquer espera: 3 minutos (PRD seção 7).
"""
import hashlib
import os
import re
import shutil
import socket
import subprocess
import tempfile
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _nome_versao():
    """«SaguiOS-Extended vN» tal como lo define kernel/inc/versao.h (D101)."""
    with open(os.path.join(RAIZ, "kernel", "inc", "versao.h"), encoding="utf-8") as f:
        t = f.read()
    nome = re.search(r'#define SAGUI_NOME\s+"([^"]+)"', t).group(1)
    versao = re.search(r'#define SAGUI_VERSAO\s+"([^"]+)"', t).group(1)
    return f"{nome} {versao}"


NOME_VERSAO = _nome_versao()
# SAGUI_BUILD: carpeta de la compilación a probar (D109: build-sem-gpl, la variante sin GPL).
BUILD = os.environ.get("SAGUI_BUILD") or os.path.join(RAIZ, "build")
IMAGEM = os.path.join(BUILD, "saguios.img")
# SAGUI_SAIDA: pasta de saída própria (D105). O rodar_paralelo.py dá uma a cada
# teste para que duas provas simultâneas não pisem em .serial, .mon, .img...
SAIDA = os.environ.get("SAGUI_SAIDA") or os.path.join(RAIZ, "build", "testes")
CAPTURAS = os.path.join(RAIZ, "docs", "capturas")
TEMPO_MAX = 180

# O monitor do QEMU é um socket Unix e o kernel aceita no máximo 107 bytes de
# caminho (sun_path). Numa cópia de trabalho com o nome longo o caminho
# estoura ("AF_UNIX path too long") e a prova nem chega a arrancar: era o que
# passava com a h70 em ../miniOS_master_os_main_6x. Quando o caminho natural não
# cabe, usa-se um curto em /tmp, com um nome derivado da prova para que duas
# provas simultâneas não se choquem.
LIMITE_SOCKET = 107


def _caminho_monitor(nome):
    caminho = os.path.join(SAIDA, nome + ".mon")
    if len(caminho) <= LIMITE_SOCKET:
        return caminho
    marca = hashlib.sha1((SAIDA + "/" + nome).encode("utf-8")).hexdigest()[:12]
    return os.path.join(tempfile.gettempdir(), f"sagui-{marca}.mon")

# Como digitar cada caractere num teclado espanhol (ES, o layout padrão)
# pelo QEMU. O QEMU nomeia as teclas pela posição do teclado americano; o
# kernel aplica o mapa ES. Por isso o "-" é a tecla "slash", o "/" é
# Shift+7, o "@" é AltGr+2 etc. ` e ^ são teclas mortas: vão seguidas de
# espaço para sair o próprio acento; o ~ (AltGr+4) também é morto.
TECLAS = {" ": "spc", "\n": "ret", "\t": "tab", "-": "slash", "_": "shift-slash", ".": "dot",
          ":": "shift-dot", ",": "comma", ";": "shift-comma", "/": "shift-7", "?": "shift-minus",
          "'": "minus", "¡": "equal", "¿": "shift-equal", "=": "shift-0", "!": "shift-1",
          '"': "shift-2", "·": "shift-3", "$": "shift-4", "%": "shift-5", "&": "shift-6",
          "(": "shift-8", ")": "shift-9", "+": "bracket_right", "*": "shift-bracket_right",
          "<": "less", ">": "shift-less", "º": "grave_accent", "ª": "shift-grave_accent",
          "\\": "alt_r-grave_accent", "|": "alt_r-1", "@": "alt_r-2", "#": "alt_r-3",
          "¬": "alt_r-6", "[": "alt_r-bracket_left", "]": "alt_r-bracket_right",
          "{": "alt_r-apostrophe", "}": "alt_r-backslash"}


def ler_ppm(caminho):
    with open(caminho, "rb") as f:
        dados = f.read()
    partes = dados.split(None, 4)   # P6, largura, altura, máximo, pixels
    l, a = int(partes[1]), int(partes[2])
    return l, a, dados[len(dados) - l * a * 3:]   # os pixels são os últimos l*a*3 bytes


_FONTE = None


def _fonte():
    """bitmap de 16 bytes -> caractere, lido do C gerado da fonte."""
    global _FONTE
    if _FONTE is None:
        _FONTE = {}
        src = open(os.path.join(RAIZ, "build", "gerado", "fonte8x16.c")).read()
        linhas = [l for l in src.splitlines() if l.strip().startswith("{")]
        for codigo, l in enumerate(linhas):
            bits = bytes(int(v, 16) for v in re.findall(r"0x([0-9A-F]{2})", l.split("/*")[0]))
            if codigo >= 0x20 and bits not in _FONTE:
                _FONTE[bits] = chr(codigo)
    return _FONTE


def reconhecer_texto(larg, alt, pixels):
    fonte = _fonte()
    colunas, linhas = larg // 8, alt // 16
    tela = []
    for cy in range(linhas):
        linha = []
        for cx in range(colunas):
            cores = []
            for j in range(16):
                ini = ((cy * 16 + j) * larg + cx * 8) * 3
                cores.append([pixels[ini + 3 * i: ini + 3 * i + 3] for i in range(8)])
            fundo = cores[0][7]   # a coluna 8 de cima é sempre fundo na fonte
            frente = next((c for fila in cores for c in fila if c != fundo), None)
            if frente is None:
                linha.append(" ")
                continue
            achou = " "
            for f, b in ((frente, fundo), (fundo, frente)):   # cursor = cores trocadas
                bits = bytes(sum(0x80 >> i for i in range(8) if fila[i] == f) for fila in cores)
                if bits in fonte:
                    achou = fonte[bits]
                    break
            linha.append(achou)
        tela.append("".join(linha).rstrip())
    return tela


def cor_do_pixel(png, x, y):
    """Lê a cor de um pixel de uma captura (via ImageMagick): (r, g, b)."""
    saida = subprocess.run(["convert", png, "-crop", f"1x1+{x}+{y}", "-depth", "8", "txt:-"],
                           capture_output=True, text=True, check=True).stdout
    m = re.search(r"\((\d+),(\d+),(\d+)", saida.splitlines()[-1])
    return tuple(int(v) for v in m.groups())


def tamanho_png(png):
    saida = subprocess.run(["identify", "-format", "%w %h", png], capture_output=True, text=True, check=True).stdout
    l, a = saida.split()
    return int(l), int(a)


# SAGUI_OPCOES_IMAGEM: opciones de mkimagem.py que se añaden a TODAS las
# imágenes de las pruebas (p. ej. "--ioapic" para pasar las pruebas de
# dispositivos por el IOAPIC, D166). La imagen normal (build/saguios.img) se
# rehace entonces como imagen de prueba con "--vesa" y esas opciones.
OPCOES_IMAGEM = os.environ.get("SAGUI_OPCOES_IMAGEM", "").split()


# D255: en las pruebas, la sesión (escritorio y Terminal) es root, para que las
# órdenes de administración (montar, discos, servicio, red...) sigan funcionando
# sin «admin». Las imágenes que hace mkimagem.py desde una prueba llevan la
# opción por SAGUI_SESION_ROOT=1; la normal (build/saguios.img, la que se
# publica) se copia una vez con el bit puesto (imagem_sesion_root). Para pasar
# pruebas como el usuario normal: SAGUI_SESION_ROOT=0 (h254 lo hace solo).
os.environ.setdefault("SAGUI_SESION_ROOT", "1")
CAB_OPCOES_DESL = 512 + 6     # LBA 1, CAB_OPCOES (boot/cabecalho.inc)
OPC_SESION_RAIZ = 0x8000


def imagem_sesion_root(origem=None):
    """build/testes/saguios-sesion-root.img: la imagen normal con OPC_SESION_RAIZ en la
    cabecera (no tiene suma de comprobación). Se rehace cuando cambia la normal."""
    import struct
    origem = origem or IMAGEM
    # Una sola copia para toda la batería (no en SAIDA, que en paralelo es una carpeta por prueba).
    pasta = os.path.join(BUILD, "testes")
    os.makedirs(pasta, exist_ok=True)
    destino = os.path.join(pasta, "saguios-sesion-root.img")
    if os.path.exists(destino) and os.path.getmtime(destino) >= os.path.getmtime(origem):
        return destino
    tmp = f"{destino}.{os.getpid()}.tmp"
    subprocess.run(["cp", "--sparse=always", origem, tmp], check=True)
    with open(tmp, "r+b") as f:
        f.seek(CAB_OPCOES_DESL)
        (op,) = struct.unpack("<H", f.read(2))
        f.seek(CAB_OPCOES_DESL)
        f.write(struct.pack("<H", op | OPC_SESION_RAIZ))
    os.replace(tmp, destino)   # atómico: otra prueba en paralelo ve la vieja o la nueva entera
    return destino


def imagem_de_teste(teste, extras=(), apps=True):
    """Monta build/testes/teste-<nome>.img: a imagem normal com um autoteste
    do kernel ligado pelo cabeçalho de boot (campo CAB_TESTE)."""
    extras = list(extras) + [o for o in OPCOES_IMAGEM if o not in extras]
    b = BUILD
    os.makedirs(SAIDA, exist_ok=True)
    # D324: las aplicaciones ya no van en el ramdisk sino en SAGUI-APPS: las imagenes de prueba
    # la llevan (build/apps/raiz) salvo que la prueba pida otra (--apps) o ninguna (--sin-apps).
    apps_raiz = os.path.join(b, "apps", "raiz")
    if apps and "--apps" not in extras and os.path.isdir(apps_raiz) and os.listdir(apps_raiz):
        extras += ["--apps", apps_raiz]
    seguro = re.sub(r"[^A-Za-z0-9_.-]+", "_", teste).strip("_")[:40]
    if extras:   # con opciones distintas, imágenes distintas (p. ej. con y sin --smp)
        seguro += "-" + re.sub(r"[^A-Za-z0-9_.-]+", "_", "".join(extras)).strip("_")[:40]
    destino = os.path.join(SAIDA, f"teste-{seguro}.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", os.path.join(b, "ramdisk.sgfs"), "--saida", destino, "--teste", teste, *extras],
                   check=True, stdout=subprocess.DEVNULL)
    return destino


# Como digitar letras acentuadas num teclado ES pelo QEMU (que nomeia as
# teclas pela posição americana): tecla morta + letra, ou a tecla do ñ/ç.
import unicodedata as _ud
ACENTOS_ES = {"ñ": ["semicolon"], "Ñ": ["shift-semicolon"], "ç": ["backslash"], "Ç": ["shift-backslash"],
              "`": ["bracket_left", "spc"], "^": ["shift-bracket_left", "spc"],
              "´": ["apostrophe", "spc"], "¨": ["shift-apostrophe", "spc"], "~": ["alt_r-4", "spc"]}
for _acento, _tecla in (("´", "apostrophe"), ("`", "bracket_left"), ("^", "shift-bracket_left"),
                        ("¨", "shift-apostrophe")):
    _comb = {"´": "\u0301", "`": "\u0300", "^": "\u0302", "¨": "\u0308"}[_acento]
    for _l in "aeiou":
        ACENTOS_ES[_ud.normalize("NFC", _l + _comb)] = [_tecla, _l]
        ACENTOS_ES[_ud.normalize("NFC", _l.upper() + _comb)] = [_tecla, "shift-" + _l]
for _l in "aon":   # AltGr+4 é o ~ morto (como no Windows)
    ACENTOS_ES[_ud.normalize("NFC", _l + "\u0303")] = ["alt_r-4", _l]
    ACENTOS_ES[_ud.normalize("NFC", _l.upper() + "\u0303")] = ["alt_r-4", "shift-" + _l]


def criar_pendrive(nome, mb=64, rotulo="MEUPEN", arquivos=None, pastas=(), particionado=True, formato="fat32"):
    """Cria build/testes/<nome>.img: um pendrive de teste, como o Windows
    faria (tabela MBR com uma partição FAT32 a partir de 1 MB), e copia
    arquivos e pastas para ele com o mtools. Devolve o caminho."""
    os.makedirs(SAIDA, exist_ok=True)
    img = os.path.join(SAIDA, nome + ".img")
    if os.path.exists(img):
        os.remove(img)
    with open(img, "wb") as f:
        f.truncate(mb * 1024 * 1024)
    desloc = 0
    if particionado:
        tipo = {"fat32": "c", "ntfs": "7", "ext": "83"}[formato]
        subprocess.run(["sfdisk", "-q", img], input=f"start=2048, type={tipo}\n", text=True, check=True)
        desloc = 2048
    if formato == "fat32":
        subprocess.run(["mkfs.vfat", "-F", "32", "-n", rotulo, "--offset", str(desloc), img],
                       check=True, stdout=subprocess.DEVNULL)
        for p in pastas:
            mtools(img, "mmd", f"p:/{p}", particionado=particionado)
        for destino, conteudo in (arquivos or {}).items():
            origem = os.path.join(SAIDA, "_conteudo.tmp")
            with open(origem, "wb") as f:
                f.write(conteudo.encode("utf-8") if isinstance(conteudo, str) else conteudo)
            mtools(img, "mcopy", "-o", origem, f"p:/{destino}", particionado=particionado)
    return img


def mtools(img, comando, *args, particionado=True):
    """Roda um comando do mtools no pendrive de teste (letra p:)."""
    rc = os.path.join(SAIDA, "mtoolsrc")
    with open(rc, "w") as f:
        f.write(f'drive p: file="{img}"' + (" offset=1048576" if particionado else "") + "\nmtools_skip_check=1\n")
    env = dict(os.environ, MTOOLSRC=rc, LC_ALL="C.UTF-8")
    r = subprocess.run([comando, *args], env=env, capture_output=True)
    return r.stdout.decode("utf-8", "replace"), r.returncode


MKE2FS = "/usr/sbin/mke2fs"
E2FSCK = "/usr/sbin/e2fsck"
DEBUGFS = "/usr/sbin/debugfs"


def _popular(destino, arquivos, pastas):
    """Cria uma pasta do host com os arquivos e pastas (para o mke2fs -d)."""
    import shutil
    if os.path.exists(destino):
        shutil.rmtree(destino)
    os.makedirs(destino)
    for p in pastas:
        os.makedirs(os.path.join(destino, p), exist_ok=True)
    for nome, conteudo in (arquivos or {}).items():
        caminho = os.path.join(destino, nome)
        os.makedirs(os.path.dirname(caminho), exist_ok=True)
        with open(caminho, "wb") as f:
            f.write(conteudo.encode("utf-8") if isinstance(conteudo, str) else conteudo)


def criar_disco(nome, particoes, mb=None):
    """Cria build/testes/<nome>.img com uma tabela MBR e várias partições
    (D040). Cada partição é um dict: tipo ("ext2", "ext3", "ext4", "fat32" ou
    "raw"), mb, rotulo, arquivos {caminho: conteúdo}, pastas, opcoes (lista
    extra para o mke2fs, ex. ["-b", "1024", "-I", "128"]). A primeira começa
    em 1 MB. Devolve (caminho, [setor inicial de cada partição])."""
    os.makedirs(SAIDA, exist_ok=True)
    img = os.path.join(SAIDA, nome + ".img")
    total = mb or (1 + sum(p["mb"] for p in particoes) + 1)
    if os.path.exists(img):
        os.remove(img)
    with open(img, "wb") as f:
        f.truncate(total * 1024 * 1024)
    tabela, inicio, inicios = "", 2048, []
    for p in particoes:
        codigo = {"fat32": "c", "raw": "da"}.get(p["tipo"], "83")
        tabela += f"start={inicio}, size={p['mb'] * 2048}, type={codigo}\n"
        inicios.append(inicio)
        inicio += p["mb"] * 2048
    subprocess.run(["sfdisk", "-q", img], input=tabela, text=True, check=True)
    for p, ini in zip(particoes, inicios):
        if p["tipo"] == "raw":
            continue
        parte = os.path.join(SAIDA, f"{nome}.p.tmp")
        if os.path.exists(parte):
            os.remove(parte)
        with open(parte, "wb") as f:
            f.truncate(p["mb"] * 1024 * 1024)
        if p["tipo"] == "fat32":
            subprocess.run(["mkfs.vfat", "-F", "32", "-n", p.get("rotulo", "DADOS"), parte], check=True, stdout=subprocess.DEVNULL)
            for d in p.get("pastas", ()):
                subprocess.run(["mmd", "-i", parte, f"::/{d}"], check=True, env=dict(os.environ, MTOOLS_SKIP_CHECK="1"))
            for destino, conteudo in (p.get("arquivos") or {}).items():
                origem = os.path.join(SAIDA, "_conteudo.tmp")
                with open(origem, "wb") as f:
                    f.write(conteudo.encode("utf-8") if isinstance(conteudo, str) else conteudo)
                subprocess.run(["mcopy", "-o", "-i", parte, origem, f"::/{destino}"], check=True,
                               env=dict(os.environ, MTOOLS_SKIP_CHECK="1"))
        else:
            pasta = os.path.join(SAIDA, f"{nome}.conteudo")
            _popular(pasta, p.get("arquivos"), p.get("pastas", ()))
            subprocess.run([MKE2FS, "-q", "-F", "-t", p["tipo"], "-L", p.get("rotulo", "LINUX"), "-d", pasta,
                            *p.get("opcoes", []), parte], check=True, stdout=subprocess.DEVNULL,
                           env=dict(os.environ, E2FSPROGS_FAKE_TIME="1700000000"))
            if p.get("depois"):
                p["depois"](parte)
        subprocess.run(["dd", f"if={parte}", f"of={img}", "bs=1M", f"seek={ini // 2048}", "conv=notrunc", "status=none"],
                       check=True)
        os.remove(parte)
    return img, inicios


def extrair_particao(img, inicio, mb, destino=None):
    """Copia uma partição da imagem para um arquivo (para e2fsck/debugfs/mtools)."""
    destino = destino or img + f".p{inicio}"
    subprocess.run(["dd", f"if={img}", f"of={destino}", "bs=512", f"skip={inicio}", f"count={mb * 2048}", "status=none"],
                   check=True)
    return destino


def particoes_img(img):
    """Las particiones de una imagen MBR, como las numera el kernel (D120, D213): las primarias
    1-4 en su orden y luego las lógicas de la extendida (5, 6...) siguiendo la cadena de EBR.
    Devuelve [(número, tipo, inicio, sectores)] sin la extendida."""
    r = []
    with open(img, "rb") as f:
        mbr = f.read(512)
        ext = None
        for k in range(4):
            e = mbr[446 + 16 * k:462 + 16 * k]
            if not e[4]:
                continue
            ini, qtd = int.from_bytes(e[8:12], "little"), int.from_bytes(e[12:16], "little")
            if e[4] in (0x05, 0x0F, 0x85):
                ext = ini
            else:
                r.append((k + 1, e[4], ini, qtd))
        rel, num = 0, 5
        while ext is not None:
            f.seek((ext + rel) * 512)
            e = f.read(512)
            if len(e) < 512 or e[510:512] != b"\x55\xAA":
                break
            if e[446 + 4]:
                r.append((num, e[446 + 4], ext + rel + int.from_bytes(e[454:458], "little"),
                          int.from_bytes(e[458:462], "little")))
                num += 1
            if e[462 + 4] not in (0x05, 0x0F, 0x85):
                break
            sig = int.from_bytes(e[470:474], "little")
            if sig <= rel:
                break
            rel = sig
    return r


def particao_dados_img(img):
    """(inicio, sectores) de la primera partición de datos (la de /home, D040): la primera que no
    es la de SaguiOS (0xDA), sea primaria (imágenes de antes de la v7.3) o lógica (D213)."""
    for _num, tipo, ini, qtd in particoes_img(img):
        if tipo not in (0xDA, 0xEF):   # ni la de arranque ni la ESP (D264)
            return ini, qtd
    raise AssertionError(f"{img} no tiene partición de datos")


def e2fsck(particao):
    """e2fsck -fn: devolve (código, saída). 0 = íntegro."""
    r = subprocess.run([E2FSCK, "-fn", particao], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def debugfs(particao, comando):
    r = subprocess.run([DEBUGFS, "-R", comando, particao], capture_output=True)
    return r.stdout


def pendrive_qemu(img, ident="pen"):
    """Argumentos do QEMU para um pendrive de dados no controlador EHCI."""
    return ["-drive", f"if=none,id={ident},format=raw,file={img}",
            "-device", f"usb-storage,bus=ehci.0,drive={ident},removable=on,id=dev-{ident}"]


# D194: el árbol de /apps (la partición SAGUI-APPS) que hace el make. Las pruebas de
# las apps que viven ahí (NetSurf, sqlite3) lo añaden a su imagen con "--apps".
APPS_RAIZ = os.path.join(BUILD, "apps", "raiz")


def opcoes_apps():
    """Las opciones de mkimagem.py para llevar la partición SAGUI-APPS (si el make la hizo)."""
    return ["--apps", APPS_RAIZ] if os.path.isdir(os.path.join(APPS_RAIZ, "bin")) else []


def imagem_console(apps=False):
    """A imagem normal, mas com o terminal de tela cheia em vez da área de
    trabalho (opção --console). Usada pelos testes das fases 2 a 4.
    apps=True: con la partición SAGUI-APPS en /apps (D194)."""
    return imagem_de_teste("", ["--vesa", "--console"] + (opcoes_apps() if apps else []))


def achar_ovmf(firmware):
    """D264: (CODE, VARS) del OVMF de 64 bits ("x64") o de 32 ("ia32"). Se buscan en
    SAGUI_OVMF, en el entorno local (tools/entorno-local.sh los extrae de los paquetes ovmf y
    ovmf-ia32 sin sudo) y en el sistema. None si no está."""
    prefixo = "OVMF32" if firmware == "ia32" else "OVMF"
    pastas = [os.environ.get("SAGUI_OVMF", ""), os.path.expanduser("~/.local/saguitools/root/usr/share/OVMF"),
              "/usr/share/OVMF"]
    for p in pastas:
        code, vars_ = os.path.join(p, f"{prefixo}_CODE_4M.fd"), os.path.join(p, f"{prefixo}_VARS_4M.fd")
        if p and os.path.isfile(code) and os.path.isfile(vars_):
            return code, vars_
    return None


def qemu_binario(firmware):
    """BIOS e IA32: qemu-system-i386 (el de siempre). x64: qemu-system-x86_64 (el OVMF de 64
    bits necesita una CPU de 64 bits); en el entorno local hay un envoltorio con el mismo nombre."""
    return "qemu-system-x86_64" if firmware == "x64" else "qemu-system-i386"


def ovmf_argumentos(firmware, nome):
    """Argumentos del QEMU para arrancar con OVMF: el código de solo lectura y una copia de
    las variables para cada máquina (el firmware escribe en ella)."""
    par = achar_ovmf(firmware)
    if not par:
        raise FalhaTeste(f"no se encuentra el OVMF {firmware} (bash tools/entorno-local.sh o apt install ovmf ovmf-ia32)")
    os.makedirs(SAIDA, exist_ok=True)
    vars_ = os.path.join(SAIDA, f"{nome}-ovmf-vars.fd")
    shutil.copyfile(par[1], vars_)
    return ["-drive", f"if=pflash,format=raw,unit=0,readonly=on,file={par[0]}",
            "-drive", f"if=pflash,format=raw,unit=1,file={vars_}"]


class FalhaTeste(Exception):
    pass


class Maquina:
    def __init__(self, nome, modo="ide", imagem=IMAGEM, memoria=None, extras=None, formato="raw", vmmouse=False,
                 snapshot=True, som=None, nucleos=1, firmware=None):
        self.nome = f"{nome}-{modo}"
        # D264: firmware=None arranca por BIOS (SeaBIOS, lo de siempre); "x64" o "ia32" con
        # OVMF (UEFI de 64 o 32 bits), que solo arranca la imagen híbrida (--uefi).
        self.firmware = firmware
        self.modo = modo
        if OPCOES_IMAGEM and imagem == IMAGEM:
            imagem = imagem_de_teste("", ["--vesa"])   # la normal, con las opciones de SAGUI_OPCOES_IMAGEM
        elif imagem == IMAGEM and os.environ.get("SAGUI_SESION_ROOT") == "1" and os.path.exists(IMAGEM):
            imagem = imagem_sesion_root()   # D255
        self.imagem = imagem
        # A maquina arranca com 2 GB, que e tudo o que o kernel sabe gerir
        # (a fronteira esta em 0x78000000, D049). A bateria paralela baixa
        # isto com SAGUI_MEM, para nao pedir tanta memoria de cada vez.
        self.memoria = memoria or os.environ.get("SAGUI_MEM") or "2G"
        # Os nucleos da maquina (QEMU -smp). O SO usa so um enquanto nao houver
        # SMP (D150); com varios, a prova do bench ve o que se pode ganhar.
        self.nucleos = nucleos
        # SAGUI_NUCLEOS: pasa cualquier prueba con varias CPU (D168), junto
        # con SAGUI_OPCOES_IMAGEM="--ioapic --smp". Solo cambia las que piden una.
        if nucleos == 1 and os.environ.get("SAGUI_NUCLEOS"):
            self.nucleos = int(os.environ["SAGUI_NUCLEOS"])
        self.extras = extras or []
        self.formato = formato
        self.vmmouse = vmmouse
        self.snapshot = "on" if snapshot else "off"   # off: o que o SaguiOS grava fica na imagem (D040)
        # Placa de som (D088): "ac97", "hda" (intel-hda + hda-duplex) ou
        # "hda-output". O som sai num .wav do anfitrião (self.wav), a 48 kHz
        # como a placa: sem reamostragem do QEMU no meio. O arquivo só fica
        # completo (cabeçalho com o tamanho) depois de desligar.
        self.som = som
        self.wav = os.path.join(SAIDA, self.nome + ".wav") if som else None
        os.makedirs(SAIDA, exist_ok=True)
        self.arq_serial = os.path.join(SAIDA, self.nome + ".serial")
        self.sock_monitor = _caminho_monitor(self.nome)
        self.proc = None
        self.mon = None

    # ------------------------------------------------------------ ciclo de vida
    def ligar(self):
        for f in (self.arq_serial, self.sock_monitor):
            if os.path.exists(f):
                os.remove(f)
        cmd = [qemu_binario(self.firmware), "-m", self.memoria, "-display", "none",
               "-serial", "file:" + self.arq_serial,
               "-monitor", f"unix:{self.sock_monitor},server,nowait",
               "-no-reboot"]
        # nucleos puede ser un número o la topología del QEMU (D174), p. ej.
        # "4,cores=2,threads=2": dos núcleos con dos hilos cada uno.
        if str(self.nucleos) != "1":
            cmd += ["-smp", str(self.nucleos)]
        # Sem a "porta dos fundos" da VMware o mouse fica só PS/2 relativo,
        # como no G470; os testes movem o mouse por deslocamentos (D035).
        if self.vmmouse is False:
            cmd += ["-machine", "vmport=off"]
        # SAGUI_KVM=1: con aceleración KVM (hay que tener acceso a /dev/kvm;
        # sin reiniciar el WSL: sg kvm -c "SAGUI_KVM=1 python3 tests/...").
        # Por defecto TCG, que es con lo que se escribieron las pruebas.
        if os.environ.get("SAGUI_KVM") == "1":
            cmd += ["-accel", "kvm"]
        # D349: SAGUI_CPU=modelo (p. ej. "qemu32,+nx" para probar con NX, o "qemu32,-pae"):
        # la CPU de todas las máquinas que no elijan la suya con extras.
        if os.environ.get("SAGUI_CPU") and "-cpu" not in self.extras:
            cmd += ["-cpu", os.environ["SAGUI_CPU"]]
        if self.firmware:
            cmd += ovmf_argumentos(self.firmware, self.nome)
        if self.modo == "ide":
            cmd += ["-drive", f"format={self.formato},file={self.imagem},if=ide,index=0,snapshot={self.snapshot}"]
        elif self.modo == "usb":
            cmd += ["-device", "usb-ehci,id=ehci",
                    "-drive", f"if=none,id=pendrive,format={self.formato},file={self.imagem},snapshot={self.snapshot}",
                    "-device", "usb-storage,bus=ehci.0,drive=pendrive,bootindex=0"]
        elif self.modo in ("xhci", "uhci"):   # D338: el pendrive de arranque en un xHCI o un UHCI
            ctl = "qemu-xhci,id=xhci" if self.modo == "xhci" else "piix3-usb-uhci,id=uhci"
            cmd += ["-device", ctl,
                    "-drive", f"if=none,id=pendrive,format={self.formato},file={self.imagem},snapshot={self.snapshot}",
                    "-device", f"usb-storage,bus={self.modo}.0,drive=pendrive,bootindex=0"]
        elif self.modo == "cd":
            cmd += ["-cdrom", self.imagem, "-boot", "d"]
        else:
            raise ValueError(self.modo)
        if self.som:
            if os.path.exists(self.wav):
                os.remove(self.wav)
            cmd += ["-audiodev", f"wav,id=som0,path={self.wav},out.frequency=48000"]
            cmd += {"ac97": ["-device", "AC97,audiodev=som0"],
                    "hda": ["-device", "intel-hda", "-device", "hda-duplex,audiodev=som0"],
                    "hda-output": ["-device", "intel-hda", "-device", "hda-output,audiodev=som0"],
                    "sb16": ["-device", "sb16,audiodev=som0"]}[self.som]   # D302: puerto 220h, IRQ 5, DMA 1 y 5
        cmd += self.extras
        self.proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        for _ in range(100):
            if os.path.exists(self.sock_monitor):
                break
            time.sleep(0.05)
        self.mon = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.mon.connect(self.sock_monitor)
        self.mon.settimeout(10)
        self._ler_monitor()  # banner do monitor
        return self

    def desligar(self):
        try:
            if self.mon:
                self.mon.sendall(b"quit\n")
                self.mon.close()
        except OSError:
            pass
        if self.proc:
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                # D270: esperar a que muera de verdad; si no, la máquina siguiente con el mismo
                # disco no arranca (el QEMU viejo aún tiene el cerrojo de escritura de la imagen).
                try:
                    self.proc.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    pass
        # O socket em /tmp (quando o caminho é curto demais) fica aí se não se
        # apaga; na pasta de saída não importa, que se borra com ela.
        if os.path.dirname(self.sock_monitor) == tempfile.gettempdir():
            try:
                os.remove(self.sock_monitor)
            except OSError:
                pass

    def __enter__(self):
        return self.ligar()

    def __exit__(self, *a):
        self.desligar()

    # ------------------------------------------------------------ monitor
    def _ler_monitor(self):
        dados = b""
        while not dados.endswith(b"(qemu) "):
            parte = self.mon.recv(65536)
            if not parte:
                break
            dados += parte
        return dados.decode("utf-8", "replace")

    def monitor(self, comando):
        self.mon.sendall(comando.encode() + b"\n")
        resp = self._ler_monitor()
        # Remove o eco do comando e o prompt; tira sequências de terminal.
        resp = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", resp)
        linhas = resp.replace("\r", "").split("\n")
        return "\n".join(l for l in linhas[1:] if not l.startswith("(qemu)"))

    # ------------------------------------------------------------ serial
    def serial_bruto(self):
        if not os.path.exists(self.arq_serial):
            return ""
        with open(self.arq_serial, "rb") as f:
            return f.read().decode("latin-1")

    def serial(self):
        # D263: el prompt del Terminal es «sagui@saguios:/$» (o «root@saguios:/$» en las imágenes de
        # sesión root); las pruebas esperan «sagui:/$»: se normaliza. serial_bruto() es el texto tal cual.
        # (y «root:/#» del modo rescate, a principio de línea, a «sagui:/#»)
        return re.sub(r"(?m)^root:(?=/)", "sagui:", re.sub(r"(?:sagui|root)@saguios:", "sagui:", self.serial_bruto()))

    def esperar_serial(self, texto, tempo=TEMPO_MAX):
        fim = time.time() + tempo
        while time.time() < fim:
            if texto in self.serial():
                return True
            if self.proc.poll() is not None:
                raise FalhaTeste(f"QEMU terminou antes de aparecer {texto!r}")
            time.sleep(0.2)
        raise FalhaTeste(f"tempo esgotado esperando {texto!r} na serial")

    # ------------------------------------------------------------ tela
    def tela_texto(self):
        """Lê o texto da tela. No modo texto VGA, direto da memória (0xB8000).
        No modo gráfico (fase 4 em diante), reconhecendo cada célula de 8x16
        da captura pelo bitmap da fonte do próprio SaguiOS."""
        ppm = os.path.join(SAIDA, self.nome + "-tela.ppm")
        self.monitor(f"screendump {ppm}")
        for _ in range(50):
            if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
                break
            time.sleep(0.1)
        time.sleep(0.1)
        l, a, pixels = ler_ppm(ppm)
        if (l, a) != (720, 400):
            return reconhecer_texto(l, a, pixels)
        saida = self.monitor("xp /2000hx 0xb8000")
        celulas = []
        for linha in saida.splitlines():
            if ":" not in linha:
                continue
            for palavra in linha.split(":", 1)[1].split():
                celulas.append(int(palavra, 16))
        chars = "".join(chr(c & 0xFF) if 32 <= (c & 0xFF) < 127 else " " for c in celulas[:2000])
        return [chars[i:i + 80].rstrip() for i in range(0, len(chars), 80)]

    def esperar_tela(self, texto, tempo=60):
        fim = time.time() + tempo
        while time.time() < fim:
            if any(texto in l for l in self.tela_texto()):
                return True
            time.sleep(0.3)
        raise FalhaTeste(f"tempo esgotado esperando {texto!r} na tela")

    def captura(self, nome, guardar=True):
        """Tira uma captura de tela. guardar=True salva em docs/capturas/."""
        ppm = os.path.join(SAIDA, self.nome + "-" + nome + ".ppm")
        destino = os.path.join(CAPTURAS if guardar else SAIDA, nome + ".png")
        self.monitor(f"screendump {ppm}")
        for _ in range(50):
            if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
                break
            time.sleep(0.1)
        time.sleep(0.2)
        subprocess.run(["convert", ppm, destino], check=True)
        os.remove(ppm)
        return destino

    # ------------------------------------------------------------ shell
    def comando(self, texto, tempo=60, teclas_finais=("ret",)):
        """Digita um comando no sagsh, aperta Enter e espera o próximo prompt.
        Devolve o que saiu na serial durante o comando (sem '\r')."""
        antes = self.serial()
        n = antes.count("sagui:")
        self.digitar(texto, 0.04)
        for t in teclas_finais:
            self.tecla(t, 0.04)
        fim = time.time() + tempo
        while self.serial().count("sagui:") <= n:
            if time.time() > fim:
                raise FalhaTeste(f"o comando {texto!r} não voltou ao prompt")
            if self.proc.poll() is not None:
                break
            time.sleep(0.2)
        return self.serial()[len(antes):].replace("\r", "")

    # ------------------------------------------------------------ mouse
    def mouse_para(self, x, y):
        """Leva o cursor a (x, y): primeiro ao canto (0,0), depois em passos."""
        # Passos pequenos e com pausa: o QEMU junta movimentos seguidos, e um
        # pacote que estoura o limite do PS/2 é descartado pelo driver.
        for _ in range(14):
            self.monitor("mouse_move -80 -80")
            time.sleep(0.02)
        self._mx = self._my = 0
        self.mouse_mover(x, y)

    def mouse_mover(self, x, y, botao_apertado=False):
        """Move em passos pequenos até (x, y) a partir da posição conhecida."""
        while (self._mx, self._my) != (x, y):
            dx = max(-40, min(40, x - self._mx))
            dy = max(-40, min(40, y - self._my))
            self.monitor(f"mouse_move {dx} {dy}")
            self._mx += dx
            self._my += dy
            time.sleep(0.03)
        time.sleep(0.2)

    def clique(self, x, y, botao=1, duplo=False):
        self.mouse_para(x, y)
        for _ in range(2 if duplo else 1):
            self.monitor(f"mouse_button {botao}")
            time.sleep(0.05)
            self.monitor("mouse_button 0")
            time.sleep(0.1)
        time.sleep(0.4)

    def arrastar(self, x0, y0, x1, y1):
        self.mouse_para(x0, y0)
        self.monitor("mouse_button 1")
        time.sleep(0.2)
        self.mouse_mover(x1, y1)
        self.monitor("mouse_button 0")
        time.sleep(0.5)

    # ------------------------------------------------------------ teclado
    def tecla(self, nome, espera=0.08):
        self.monitor(f"sendkey {nome}")
        time.sleep(espera)

    def digitar(self, texto, espera=0.08):
        for c in texto:
            if c in ACENTOS_ES:
                for t in ACENTOS_ES[c]:
                    self.tecla(t, espera)
            elif c in TECLAS:
                self.tecla(TECLAS[c], espera)
            elif c.isupper():
                self.tecla("shift-" + c.lower(), espera)
            else:
                self.tecla(c, espera)


def verificar(condicao, mensagem):
    if not condicao:
        raise FalhaTeste(mensagem)
    print("  ok:", mensagem)
