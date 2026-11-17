#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H366 - programas de Go (TinyGo) dentro de SaguiOS (D364, docs/go.md).

Si TinyGo no está instalado (tools/tinygo_entorno.sh instalar) la prueba se salta con un
aviso. Si lo está: compila con tools/go-sagui cada ejemplo de userland/go/ejemplos/, los
pone en /apps/go de la imagen, arranca QEMU con el terminal de pantalla completa, ejecuta
cada uno y comprueba su salida."""
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, APPS_RAIZ, SAIDA, RAIZ  # noqa: E402

TINYGO = os.path.expanduser("~/.local/saguitools/tinygo/bin/tinygo")
EJEMPLOS = ["hola", "argumentos", "ficheros", "goroutines", "tiempo", "contar", "adivinar", "json", "acentos"]

# (comando, [textos que debe contener la salida])
COMANDOS = [
    ("/apps/go/hola", ["hola desde Go en SaguiOS", "2 + 3 = 5", "1.414"]),
    ("/apps/go/argumentos uno dos", ['arg 1 = "uno"', 'arg 2 = "dos"', "SAGUI_GO = si"]),
    ("/apps/go/ficheros", ["leidos 28 bytes", "suma de los cuadrados: 91",
                           "carpeta borrada", "ficheros: terminado"]),
    ("/apps/go/goroutines", ["suma 1..100 = 5050", "contador = 8000",
                             "suma de cuadrados 1..10 = 385", "tiempo agotado", "goroutines: terminado"]),
    ("/apps/go/tiempo", ["tic 4", "cancelado.Stop() = true", "2027-02-24", "tiempo: terminado"]),
    ("/apps/go/contar /bin/sagsh", ["/bin/sagsh"]),
    ("/apps/go/json", ['"nombre":"Ana"', "Luis tiene 42", "json: terminado"]),
]


def main():
    if not os.path.exists(TINYGO):
        print("AVISO: TinyGo no está instalado (tools/tinygo_entorno.sh instalar): h366 se salta")
        return 0
    os.makedirs(SAIDA, exist_ok=True)
    apps = os.path.join(SAIDA, "apps-go")
    if os.path.isdir(apps):
        shutil.rmtree(apps)
    if os.path.isdir(APPS_RAIZ):
        shutil.copytree(APPS_RAIZ, apps)
    os.makedirs(os.path.join(apps, "go"), exist_ok=True)
    os.makedirs(os.path.join(apps, "bin"), exist_ok=True)
    for e in EJEMPLOS:
        r = subprocess.run([os.path.join(RAIZ, "tools", "go-sagui"), "build", "-o", os.path.join(apps, "go", e),
                            os.path.join(RAIZ, "userland", "go", "ejemplos", e)],
                           capture_output=True, text=True)
        verificar(r.returncode == 0, f"go-sagui compila {e} ({r.stderr[-300:]})")

    img = imagem_de_teste("", ["--vesa", "--console", "--apps", apps])
    with Maquina("h366", "ide", imagem=img) as m:
        m.esperar_serial("sagui:", 120)
        for cmd, textos in COMANDOS:
            out = m.comando(cmd, 90)
            for t in textos:
                verificar(t in out, f"{cmd}: aparece {t!r}")
        # ficheros ajenos: contar cuenta bien un fichero conocido
        m.comando("echo uno dos tres > /tmp/p.txt")
        out = m.comando("/apps/go/contar /tmp/p.txt")
        verificar(" 1 " in out and " 3 " in out, f"contar da 1 línea y 3 palabras ({out!r})")
        # salida con código de error
        out = m.comando("/apps/go/argumentos falla")
        verificar("terminou com codigo 7" in out, f"os.Exit(7) da el código de salida ({out!r})")
        # el juego, con número fijado y entrada por tubería
        out = m.comando("echo 50 | /apps/go/adivinar 42")
        verificar("bajo" in out, f"adivinar lee la entrada estándar ({out!r})")
        # D364: acentos. Salida UTF-8 -> Latin-1 en el terminal (la serial lleva los bytes del terminal).
        m.digitar("/apps/go/acentos", 0.04)
        m.tecla("ret")
        m.esperar_serial("escribe algo:", 60)
        m.digitar("ñ", 0.04)
        m.tecla("ret")
        m.esperar_serial("runas", 60)
        out = m.serial().replace("\r", "")
        verificar("año y canción" in out, f"el terminal recibe Latin-1 (año y canción) ({out[-400:]!r})")
        verificar("bytes en el fichero: 13 runas: 11" in out, "el fichero queda en UTF-8 (13 bytes)")
        verificar("2 bytes, 1 runas" in out, "la ñ del teclado llega a Go como UTF-8")
        verificar("€" not in out and "no cabe" in out, "lo que no cabe en Latin-1 sale como ?")
        m.captura("H366-go")
    return 0


if __name__ == "__main__":
    sys.exit(main())
