#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H260 - Publicación al repositorio público (tools/publicar.sh, D260).

Prueba ligera en el anfitrión (sin QEMU): crea un repositorio git de juguete y comprueba que
`preparar` rechaza un secreto falso, un fichero de más de 5 MB, un correo y una IP públicos, una
MAC y un número de serie; que excluye `entrega/`, `.env*` e `input_share/`; que con un árbol
limpio pasa y no deja el directorio a medias cuando falla; y que `subir` sin confirmación
(entrada no interactiva) no empuja nada.
"""
import os
import shutil
import subprocess
import sys
import tempfile

RAIZ = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SCRIPT = os.path.join(RAIZ, "tools", "publicar.sh")
fallos = []


def comprobar(cond, texto):
    print(("ok   " if cond else "FALLO ") + texto)
    if not cond:
        fallos.append(texto)


def git(cwd, *args):
    return subprocess.run(["git", "-C", cwd, *args], check=True, capture_output=True, text=True).stdout


def escribir(base, ruta, contenido, binario=False):
    p = os.path.join(base, ruta)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "wb" if binario else "w") as f:
        f.write(contenido)


def repo_limpio(tmp, nombre):
    r = os.path.join(tmp, nombre)
    os.makedirs(r)
    git(r, "init", "-q", "-b", "main")
    git(r, "config", "user.name", "Prueba")
    git(r, "config", "user.email", "prueba@prueba.invalid")
    escribir(r, "LICENSE", "GNU GENERAL PUBLIC LICENSE\nVersion 3, 29 June 2007\n")
    escribir(r, "AUTORES.md", "Basado en https://github.com/erigobeli/sagui-os\n")
    escribir(r, "kernel/main.c", "int main(void) { return 0; }\n")
    escribir(r, "docs/notas.md", "Red de ejemplo: 192.168.1.5 y MAC de QEMU 52:54:00:12:34:56.\n")
    # Lo que debe excluirse siempre
    escribir(r, "entrega/v1/saguios.img", b"\0" * (6 * 1024 * 1024), binario=True)
    escribir(r, ".env_agents", "GT=" + "x" * 10 + "\n")
    escribir(r, "input_share/juego.bin", "datos\n")
    escribir(r, "ayudame.md", "notas internas\n")
    escribir(r, "kernel/terceros/linux/r8169/x.c", "// SPDX-License-Identifier: GPL-2.0-only\n")
    escribir(r, "userland/terceros/gnuboy/g.c", "int g;\n")
    # D259: lo GPL-2.0-or-later de kernel/terceros/linux/ sí va
    escribir(r, "kernel/terceros/linux/hda/idt.h", "// SPDX-License-Identifier: GPL-2.0-or-later\n")
    git(r, "add", "-A")
    git(r, "commit", "-q", "-m", "base")
    return r


def preparar(repo, tmp, destino, extra=None):
    env = dict(os.environ, PUBLICAR_RAIZ=repo, PUBLICAR_RAMA="main", PUBLICAR_TRABAJO=os.path.join(tmp, "trabajo"))
    p = subprocess.run(["bash", SCRIPT, "preparar", "v1.0", destino, *(extra or [])], env=env,
                       capture_output=True, text=True, stdin=subprocess.DEVNULL)
    return p.returncode, p.stdout + p.stderr


def caso_malo(tmp, nombre, ruta, contenido, patron, binario=False):
    r = repo_limpio(tmp, "repo_" + nombre)
    escribir(r, ruta, contenido, binario)
    git(r, "add", "-A")
    git(r, "commit", "-q", "-m", nombre)
    dest = os.path.join(tmp, "dest_" + nombre)
    rc, salida = preparar(r, tmp, dest)
    comprobar(rc != 0 and patron in salida, f"{nombre}: preparar lo rechaza")
    comprobar(not os.path.exists(dest) and not os.path.exists(dest + ".parcial") and not os.path.exists(dest + ".ok"),
              f"{nombre}: no deja nada a medias")


def main():
    tmp = tempfile.mkdtemp(prefix="h260_")
    try:
        # 1. Árbol limpio: pasa y excluye lo que toca
        r = repo_limpio(tmp, "limpio")
        dest = os.path.join(tmp, "dest_limpio")
        rc, salida = preparar(r, tmp, dest)
        comprobar(rc == 0, "árbol limpio: preparar pasa\n" + (salida if rc else ""))
        comprobar(os.path.isfile(os.path.join(dest, "kernel/main.c")), "limpio: el código va")
        for excluido in ("entrega", ".env_agents", "input_share", "ayudame.md", "kernel/terceros/linux/r8169", "userland/terceros/gnuboy"):
            comprobar(not os.path.exists(os.path.join(dest, excluido)), f"limpio: se excluye {excluido}")
        comprobar(os.path.isfile(os.path.join(dest, "kernel/terceros/linux/hda/idt.h")), "limpio: el «or later» de linux/ va")
        comprobar(os.path.isfile(dest + ".ok"), "limpio: deja la marca de lo comprobado")
        # un fichero sin versionar (del árbol de trabajo) nunca se exporta
        escribir(r, "sin_versionar.txt", "no está en git\n")
        dest2 = os.path.join(tmp, "dest_limpio2")
        rc, _ = preparar(r, tmp, dest2)
        comprobar(rc == 0 and not os.path.exists(os.path.join(dest2, "sin_versionar.txt")),
                  "limpio: solo salen los ficheros versionados")
        # un DESTINO no vacío se rechaza
        rc, _ = preparar(r, tmp, dest)
        comprobar(rc != 0, "limpio: un DESTINO ya existente se rechaza")

        # 2. Casos malos (el secreto falso se arma aquí, no queda como literal en un fichero)
        caso_malo(tmp, "secreto", "kernel/clave.c", "// " + "ghp_" + "A1b2C3d4E5f6G7h8I9j0K1l2M3n4O5p6Q7r8" + "\n", "secretos")
        caso_malo(tmp, "grande", "kernel/grande.bin", b"\1" * (6 * 1024 * 1024), "más de 5 MB", binario=True)
        caso_malo(tmp, "correo", "docs/contacto.md", "Escríbeme a alguien" + "@" + "empresa-real.com\n", "correo")
        caso_malo(tmp, "ip", "docs/red.md", "Servidor en 93.184.216.34\n", "IP públicas")
        caso_malo(tmp, "mac", "docs/mac.md", "Tarjeta 3c:97:0e:aa:bb:cd\n", "MAC")
        caso_malo(tmp, "serie", "docs/informe.md", "Serial Number: ABC123XYZ9\n", "serie")
        caso_malo(tmp, "linuxsolov2", "kernel/terceros/linux/hwmon/t.h", "// SPDX-License-Identifier: GPL-2.0-only\n", "sin «or later»")
        caso_malo(tmp, "clave", "docs/k.md", "-----BEGIN RSA " + "PRIVATE KEY-----\n", "secretos")

        # 3. Un commit que no está en main se rechaza
        git(r, "checkout", "-q", "-b", "otra")
        escribir(r, "otra.txt", "x\n")
        git(r, "add", "-A")
        git(r, "commit", "-q", "-m", "fuera de main")
        rc, salida = preparar(r, tmp, os.path.join(tmp, "dest_otra"), ["--commit", "otra"])
        comprobar(rc != 0 and "no está en main" in salida, "un commit fuera de main se rechaza")

        # 4. subir sin confirmación interactiva: no empuja nada
        publico = os.path.join(tmp, "publico.git")
        subprocess.run(["git", "init", "-q", "--bare", "-b", "main", publico], check=True)
        env = dict(os.environ, PUBLICAR_RAIZ=r, PUBLICAR_RAMA="main", PUBLICAR_TRABAJO=os.path.join(tmp, "trabajo"))
        p = subprocess.run(["bash", SCRIPT, "subir", "v1.0", dest, publico], env=env, capture_output=True, text=True,
                           stdin=subprocess.DEVNULL)
        comprobar(p.returncode != 0 and "no se subió nada" in p.stdout + p.stderr, "subir sin PUBLICAR no empuja")
        comprobar(subprocess.run(["git", "-C", publico, "rev-parse", "--verify", "-q", "refs/heads/main"],
                                 capture_output=True).returncode != 0, "el repo público sigue vacío")
        clon = os.path.join(tmp, "trabajo", "repo-publico")
        autor = git(clon, "log", "-1", "--format=%an <%ae>|%s").strip()
        comprobar(autor == "Prueba <prueba@prueba.invalid>|Publicación v1.0", "el commit local lleva el autor de git config y el mensaje")
        comprobar("v1.0" in git(clon, "tag").split(), "la etiqueta existe en el clon local")
        # modificar lo preparado invalida la marca
        escribir(dest, "kernel/main.c", "int main(void) { return 1; }\n")
        p = subprocess.run(["bash", SCRIPT, "subir", "v1.0", dest, publico], env=env, capture_output=True, text=True,
                           stdin=subprocess.DEVNULL)
        comprobar(p.returncode != 0 and "cambió" in p.stdout + p.stderr, "subir detecta un directorio modificado")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if fallos:
        print(f"\nH260: {len(fallos)} fallo(s)")
        sys.exit(1)
    print("\nH260: todo correcto")


if __name__ == "__main__":
    main()
