#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H254 (D255, mejora 098, fase 1) - usuarios y llamadas privilegiadas.

Tres arranques, todos con la sesión como sagui (--sesion-sagui; el resto de la batería
corre como root, D255):

1. Disco con la partición SAGUI-CONF (ext2; la crea mkimagem.py por defecto, D256), consola de texto y red con hostfwd:
   - el shell es sagui (id, whoami) y el kernel anota la sesión;
   - /testes/usuarios --llamadas: TODAS las llamadas y operaciones clasificadas: las de
     root dan ERRO_ACESSO y las libres no; --rutas: sombra, /etc, /bin, /sistema...;
     --pbkdf2: vectores de la RFC 7914;
   - un servicio con usuario=sagui (lanzado por el gestor) no puede apagar ni reiniciar,
     ni fijar la contraseña de root (no es la sesión local); corre con uid 1000 y los del
     sistema con el suyo (cron como root);
   - «servicio parar» sin ser root: mensaje de administrador;
   - admin la primera vez: root no tiene contraseña y se elige; después la pide; mala y
     buena; su -c; passwd root (actual, nueva); la vieja ya no vale;
   - sshd: la clave de /etc/ssh/authorized_keys entra como root, la de
     /home/.ssh/authorized_keys como sagui (también si pide root@); desde SSH, reiniciar
     se niega y admin funciona con la contraseña;
   - sombra dañada y borrada: la cuenta queda sin contraseña y se puede volver a fijar
     desde la sesión local (nunca bloqueada);
   - límite de intentos: tras 5 fallos, bloqueado (también con la buena);
   - reinicio: la contraseña sigue (SAGUI-CONF) y la clave del sshd es la misma.
2. Sin SAGUI-CONF y con escritorio («modo en vivo»): el escritorio y el Terminal son
   sagui, ningún programa del arranque recibe una negativa, y admin no pide contraseña
   en la sesión local.
3. Modo rescate (mkimagem.py --rescate): consola con un shell de root y sin servicios.
"""
import os
import re
import shutil
import socket
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA, particoes_img  # noqa: E402

TRAB = os.path.join(SAIDA, "h254")
B = os.path.join(RAIZ, "build")
SENHA1, SENHA2, SENHA3 = "clave1234", "nueva5678", "otra9012"


def puerto_libre():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def gerar_chave(nome):
    arq = os.path.join(TRAB, nome)
    for f in (arq, arq + ".pub"):
        if os.path.exists(f):
            os.remove(f)
    subprocess.run(["ssh-keygen", "-q", "-t", "ed25519", "-N", "", "-C", nome, "-f", arq], check=True)
    return arq


def construir_imagen(nome, extras, chave_root=None, chave_sagui=None, conf=False):
    raiz = os.path.join(TRAB, "rootfs-" + nome)
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(B, "rootfs"), raiz)
    verificar(os.path.exists(os.path.join(raiz, "testes", "usuarios")), "/testes/usuarios está en el ramdisk")
    serv = os.path.join(raiz, "etc", "servicios")
    with open(os.path.join(serv, "pruebausu.conf"), "w") as f:
        f.write("# prueba h254: un servicio que no es root\nnombre=pruebausu\ndescripcion=prueba h254\n"
                "comando=/testes/usuarios --servicio\nusuario=sagui\narrancar=si\nreiniciar=no\n")
    if chave_root:
        with open(os.path.join(raiz, "etc", "ssh", "authorized_keys"), "w") as f:
            f.write("# clave de root (prueba h254)\n" + open(chave_root + ".pub").read())
    if chave_sagui:
        os.makedirs(os.path.join(raiz, "home", ".ssh"), exist_ok=True)
        with open(os.path.join(raiz, "home", ".ssh", "authorized_keys"), "w") as f:
            f.write("# clave de sagui (prueba h254)\n" + open(chave_sagui + ".pub").read())
    sgfs = os.path.join(TRAB, nome + ".sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(TRAB, nome + ".img")
    if not conf:
        extras = extras + ["--sin-conf"]      # sin SAGUI-CONF: modo en vivo (D256: por defecto la imagen la lleva)
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(B, "boot", "mbr.bin"), "--estagio2", os.path.join(B, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(B, "kernel.bin"), "--kernel-elf", os.path.join(B, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--sesion-sagui"] + extras,
                   check=True, stdout=subprocess.DEVNULL)
    return img


def ssh(porta, chave, usuario, orden, entrada=None, tempo=90):
    cmd = ["ssh", "-p", str(porta), "-F", "/dev/null", "-o", "StrictHostKeyChecking=no",
           "-o", "UserKnownHostsFile=/dev/null", "-o", "IdentitiesOnly=yes", "-o", "IdentityAgent=none",
           "-o", "BatchMode=yes", "-o", "LogLevel=ERROR", "-o", "ConnectTimeout=20", "-i", chave,
           f"{usuario}@127.0.0.1", orden]
    return subprocess.run(cmd, input=entrada, capture_output=True, timeout=tempo,
                          stdin=subprocess.DEVNULL if entrada is None else None)


def huella(porta):
    r = subprocess.run(["ssh-keyscan", "-p", str(porta), "-t", "ed25519", "127.0.0.1"], capture_output=True, text=True,
                       timeout=30)
    return r.stdout.split()[-1] if r.stdout.split() else ""


def esperar(m, padrao, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tiempo agotado esperando {padrao!r}")


def interativo(m, orden, pasos, tempo=60):
    """Escribe la orden y responde a cada pregunta (texto esperado, respuesta) en orden."""
    desde = len(m.serial())
    m.digitar(orden, 0.04)
    m.tecla("ret")
    for pregunta, resposta in pasos:
        esperar(m, re.escape(pregunta), tempo, desde)
        desde = len(m.serial())
        m.digitar(resposta, 0.04)
        m.tecla("ret")
    n = m.serial()[:desde].count("sagui:")
    fim = time.time() + tempo
    while m.serial().count("sagui:") <= n and time.time() < fim:
        time.sleep(0.2)
    return m.serial()[desde:].replace("\r", "")


def maquina_conf():
    chave_root = gerar_chave("clave_root")
    chave_sagui = gerar_chave("clave_sagui")
    img = construir_imagen("conf", ["--console"], chave_root, chave_sagui, conf=True)
    porta = puerto_libre()
    extras = ["-nic", f"user,model=e1000,hostfwd=tcp:127.0.0.1:{porta}-:22"]
    with Maquina("h254", "ide", imagem=img, snapshot=False, extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[PERMISOS] sesion como sagui" in s, "el kernel anota la sesión como sagui")
        verificar(re.search(r"\[CONF\] SAGUI-CONF en /sistema/conf", s), "monta SAGUI-CONF en /sistema/conf")
        verificar("no esta clasificada" not in s, "todas las llamadas del despachador están clasificadas")
        out = m.comando("id")
        verificar("uid=1000(sagui) gid=1000(sagui)" in out, "id: uid=1000(sagui)")
        out = m.comando("whoami")
        verificar(re.search(r"^sagui$", out, re.M), "whoami: sagui")

        # --- todas las llamadas
        out = m.comando("/testes/usuarios --llamadas", 120)
        g = re.search(r"usuarios: llamadas ok=(\d+) mal=(\d+)", out)
        verificar(g and g.group(2) == "0" and int(g.group(1)) > 150,
                  f"todas las llamadas y operaciones: las de root dan ERRO_ACESSO, las libres no ({g and g.group(0)})")
        verificar("[PERMISOS] denegado a usuarios" in m.serial(), "las negativas se anotan")
        verificar(re.search(r"\[PERMISOS\] \d+ denegaciones mas sin anotar", m.serial()) or
                  m.serial().count("[PERMISOS] denegado") <= 40, "el registro de negativas tiene límite")
        out = m.comando("/testes/usuarios --rutas", 60)
        g = re.search(r"usuarios: rutas ok=(\d+) mal=(\d+)", out)
        verificar(g and g.group(2) == "0", f"rutas protegidas ({g and g.group(0)})")
        out = m.comando("/testes/usuarios --pbkdf2", 120)
        g = re.search(r"usuarios: pbkdf2 ok=(\d+) mal=(\d+)", out)
        verificar(g and g.group(2) == "0", f"PBKDF2-HMAC-SHA256 ({g and g.group(0)})")

        # --- servicios
        g = esperar(m, r"usuarios: servicio ok=(\d+) mal=(\d+)", 60)
        verificar(g[1] == "0", f"un servicio no root no puede apagar, reiniciar ni fijar la contraseña ({g})")
        verificar(re.search(r"servicio elevar sin contrasena -> -19", m.serial()), "desde un servicio: ERRO_SEM_SENHA")
        verificar(re.search(r"pruebausu: en marcha \(pid \d+\): .* \(uid 1000\)", m.serial()), "pruebausu corre como sagui")
        verificar(re.search(r"cron: en marcha \(pid \d+\): .* \(uid 0\)", m.serial()), "cron corre como root (usuario=root)")
        out = m.comando("servicio parar cron", 30)
        verificar("administrador" in out, "servicio parar sin ser root: hace falta ser administrador")

        # --- admin la primera vez: se elige la contraseña
        out = interativo(m, "admin id", [("aún no tiene contraseña", ""), ("Contraseña nueva:", SENHA1),
                                         ("Repítela:", SENHA1)][1:], 90)
        verificar("uid=0(root)" in out, "admin la primera vez: se elige la contraseña de root y la orden corre como root")
        verificar(re.search(r"ELEVACION concedida: admin \(pid \d+\) pasa de uid 1000 a uid 0", m.serial()),
                  "el kernel anota la elevación")
        verificar("primera contraseña de root fijada" in m.serial(), "el gestor anota la primera contraseña")
        out = m.comando("id")
        verificar("uid=1000(sagui)" in out, "el shell sigue siendo sagui")
        out = interativo(m, "admin id", [("Contraseña de root:", "mala")], 90)
        verificar("Contraseña incorrecta" in out, "admin con la contraseña mala: incorrecta")
        m.tecla("ctrl-c")
        m.comando("", 30)
        out = interativo(m, "admin whoami", [("Contraseña de root:", SENHA1)], 90)
        verificar(re.search(r"^root$", out, re.M), "admin con la buena: whoami = root")
        out = m.comando(f"echo {SENHA1} | su -c id", 90)
        verificar("uid=0(root)" in out, "su -c con la contraseña por la entrada")
        out = m.comando(f"echo {SENHA1} > /tmp/p; echo {SENHA2} >> /tmp/p; echo {SENHA2} >> /tmp/p; passwd root < /tmp/p", 120)
        verificar("contraseña de root cambiada" in out, "passwd root: actual y nueva")
        out = m.comando(f"echo {SENHA1} | admin id", 90)
        verificar("uid=0(root)" not in out, "la contraseña vieja ya no vale")
        out = m.comando(f"echo {SENHA2} | admin id", 90)
        verificar("uid=0(root)" in out, "la nueva sí")
        verificar(SENHA2 not in m.serial().split("passwd root < /tmp/p")[-1].replace(f"echo {SENHA2}", ""),
                  "la contraseña no aparece en ningún registro")

        # --- sshd
        out = m.comando(f"echo {SENHA2} | admin servicio iniciar sshd", 90)
        esperar(m, r"sshd: escuchando en el puerto 22", 60)
        verificar(re.search(r"sshd: en marcha \(pid \d+\): .* \(uid 0\)", m.serial()), "sshd corre como root")
        r = ssh(porta, chave_root, "root", "id")
        verificar(r.returncode == 0 and b"uid=0(root)" in r.stdout, f"ssh root@ con la clave de root: root ({r.stdout!r})")
        r = ssh(porta, chave_sagui, "root", "id")
        verificar(r.returncode == 0 and b"uid=1000(sagui)" in r.stdout,
                  f"ssh root@ con la clave de /home/.ssh: entra como sagui ({r.stdout!r})")
        r = ssh(porta, chave_sagui, "sagui", "whoami")
        verificar(r.returncode == 0 and r.stdout.strip() == b"sagui", "ssh sagui@: sagui")
        r = ssh(porta, chave_sagui, "sagui", "reiniciar")
        verificar(b"administrador" in r.stderr + r.stdout, f"reiniciar por SSH sin ser root: se niega ({r.stderr!r})")
        r = ssh(porta, chave_sagui, "sagui", "admin id", entrada=(SENHA2 + "\n").encode())
        verificar(b"uid=0(root)" in r.stdout, "admin por SSH con la contraseña")
        huella1 = huella(porta)
        verificar(huella1, "huella del servidor")

        # --- sombra dañada y borrada: nunca bloqueada; se vuelve a fijar desde la sesión local
        r = ssh(porta, chave_root, "root", "echo root:basura > /etc/sombra")
        out = interativo(m, "admin id", [("Contraseña nueva:", SENHA3), ("Repítela:", SENHA3)], 90)
        verificar("uid=0(root)" in out, "sombra dañada: root queda sin contraseña y se elige otra")
        verificar("no se entiende" in m.serial(), "el gestor avisa de la línea dañada")
        r = ssh(porta, chave_root, "root", "rm /etc/sombra")
        r = ssh(porta, chave_sagui, "sagui", "admin id", entrada=b"\n")
        verificar(b"uid=0(root)" not in r.stdout, "sin sombra, por SSH no se puede fijar la de root")
        out = interativo(m, "admin id", [("Contraseña nueva:", SENHA2), ("Repítela:", SENHA2)], 90)
        verificar("uid=0(root)" in out, "sin sombra: se elige otra desde la sesión local")

        # --- límite de intentos
        for i in range(5):
            m.comando("echo mala | admin id", 90)
        out = m.comando(f"echo {SENHA2} | admin id", 60)
        verificar("demasiados intentos" in out and "uid=0(root)" not in out,
                  "tras 5 fallos, bloqueado un minuto (también con la buena)")
        verificar("bloqueado un minuto" in m.serial(), "el gestor lo anota")
        time.sleep(62)
        out = m.comando(f"echo {SENHA2} | admin id", 90)
        verificar("uid=0(root)" in out, "pasado el minuto, la buena vale otra vez")

        # --- reinicio (el QEMU de las pruebas sale al reiniciar: -no-reboot)
        try:
            ssh(porta, chave_root, "root", "reiniciar", tempo=30)
        except subprocess.TimeoutExpired:
            pass
        fim = time.time() + 60
        while m.proc.poll() is None and time.time() < fim:
            time.sleep(0.5)
        verificar("[MONTAGEM] hda5 desmontado de /sistema/conf" in m.serial() or "desmontado de /sistema/conf" in m.serial(),
                  "al reiniciar, SAGUI-CONF se desmonta con todo grabado")
    # --- otro arranque con el mismo disco: la contraseña y la clave del servidor siguen
    with Maquina("h254-2", "ide", imagem=img, snapshot=False, extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        verificar("[CONF] SAGUI-CONF en /sistema/conf" in m.serial(), "tras reiniciar, SAGUI-CONF otra vez")
        out = m.comando(f"echo {SENHA2} | admin id", 90)
        verificar("uid=0(root)" in out, "la contraseña de root sigue tras reiniciar (SAGUI-CONF)")
        out = m.comando("echo | admin id", 60)
        verificar("uid=0(root)" not in out, "y sin ella no se entra")
        m.comando(f"echo {SENHA2} | admin servicio iniciar sshd", 90)
        esperar(m, r"sshd: escuchando en el puerto 22", 60)
        verificar(huella(porta) == huella1, "la clave del servidor SSH es la misma (en SAGUI-CONF)")
        r = ssh(porta, chave_root, "root", "id")
        verificar(b"uid=0(root)" in r.stdout, "la clave de root sigue entrando como root")


def maquina_en_vivo():
    img = construir_imagen("vivo", ["--vesa"])
    with Maquina("h254-vivo", "ide", imagem=img) as m:
        m.esperar_serial("[SAGUI] terminal:", 120)
        esperar(m, r"supervisor en marcha", 60)
        time.sleep(3)
        s = m.serial()
        verificar("[PERMISOS] sesion como sagui" in s, "escritorio: la sesión es sagui")
        verificar("modo en vivo" in s, "sin SAGUI-CONF: modo en vivo")
        nega = re.findall(r"\[PERMISOS\] denegado a (\S+)", s)
        verificar(not [n for n in nega if n != "usuarios"], f"ningún programa del arranque recibe una negativa ({nega})")
        m.esperar_serial("sagui:/$", 60)
        out = m.comando("id")
        verificar("uid=1000(sagui)" in out, "el Terminal del escritorio es sagui")
        out = m.comando("echo | admin id", 90)
        verificar("uid=0(root)" in out, "modo en vivo: admin sin contraseña en la sesión local")
        m.captura("h254_escritorio_sagui", guardar=False)


def maquina_rescate():
    img = construir_imagen("rescate", ["--vesa", "--rescate"])
    with Maquina("h254-rescate", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("(MODO RESCATE)" in s and "[SAGUI] MODO RESCATE" in s, "modo rescate anotado")
        verificar("no se lanzan en el modo rescate" in s, "sin servicios")
        verificar("area de trabalho no ar" not in s, "sin escritorio")
        out = m.comando("id")
        verificar("uid=0(root)" in out, "el shell del modo rescate es root, sin contraseña")
        out = m.comando("echo | passwd root; echo fin", 30)
        verificar("fin" in out, "passwd no se queda colgado sin gestor de servicios")


def main():
    os.makedirs(TRAB, exist_ok=True)
    for prog in ("ssh", "ssh-keygen", "ssh-keyscan", "e2label"):
        if not shutil.which(prog):
            print(f"h254: falta {prog} en el anfitrión")
            return 1
    so = sys.argv[1:]   # para depurar: «conf», «vivo» o «rescate» (sin nada, las tres)
    if not so or "rescate" in so:
        maquina_rescate()
    if not so or "vivo" in so:
        maquina_en_vivo()
    if not so or "conf" in so:
        maquina_conf()
    print("H254 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
