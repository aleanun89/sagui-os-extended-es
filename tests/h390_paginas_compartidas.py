#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H390 - páginas compartidas de las bibliotecas (D378, fase 3; mejora 138).

Criterio (/apps/testes/compartidas, programa dinámico que corre como root):
  1. MEM_COMPARTIR: un fichero de /tmp no se comparte; nunca con escritura; no encima de
     páginas ya asignadas; MEM_DEVOLVER las quita y MEM_SACAR no las saca.
  2. 8 hijos con libsagui.so, medios, libc y libcomp.so: lo compartido no crece con los hijos
     (sí lo mapeado) y cada hijo gasta menos de lo que comparte; el contador de .data es
     privado; el Administrador de tareas no cuenta las compartidas a cada proceso.
  3. libcomp.so sustituida en el disco (mismo tamaño, misma fecha): un hijo nuevo ve la versión
     nueva y los viejos siguen con la vieja.
  4. Escribir en el texto o el código compartidos (también tras MEM_PROTEGER) mata al hijo.
  5. Al terminar los hijos y tras 20 vueltas de 4 hijos que abren y cierran: sin fugas.
  6. (revisión B1) un hilo da escritura con MEM_PROTEGER sin parar mientras otro hace
     MEM_COMPARTIR: nunca queda un alias escribible y otro proceso ve el objeto igual al fichero.
  7. (revisión I1) MEM_COMPARTIR solo de un ELF ET_DYN y dentro de sus PT_LOAD sin escritura.
  Variantes (H390_SO=parte1,parte2): normal (2G), smp (--smp, 4 CPU, sin caché de disco: la lectura duerme y la
  carrera de B1 tiene hueco), pae6g (memoria por
  encima de 4 GB), sempae (--sem-pae, 3G), presion (512M: la swap saca las privadas mientras
  los hijos comprueban sus páginas compartidas).
Con SAGUI_KVM=1 se prueba con KVM.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
TEMPO = 300 if KVM else 1200


def sem_erros(s, et):
    verificar("EXCECAO" not in s, f"[{et}] sin excepciones del kernel")
    verificar("liberando pagina fisica invalida" not in s, f"[{et}] ninguna página liberada dos veces")
    verificar("ERRO memalta" not in s, f"[{et}] sin errores de las ventanas temporales")
    verificar("[SWAP] ERRO" not in s and "[SWAP] AVISO" not in s, f"[{et}] la swap sin trozos corruptos")


def rodar(et, teste, extras=(), memoria=None, nucleos=1, cpu=None):
    maq_extras = ["-cpu", cpu] if cpu else None
    with Maquina(f"h390-{et}", "ide", imagem=imagem_de_teste(teste, list(extras)), memoria=memoria,
                 nucleos=nucleos, extras=maq_extras) as m:
        m.esperar_serial("RESULTADO compartidas:", TEMPO)
        s = m.serial().replace("\r", "")
    for l in s.split("\n"):
        if l.startswith("COMPARTIDAS") or "[FALHA]" in l or "EXCECAO" in l or "imagens:" in l:
            print("    %s" % l.strip())
    verificar("RESULTADO compartidas: SUCESSO (0 falhas)" in s, f"[{et}] /apps/testes/compartidas pasó todas sus pruebas")
    sem_erros(s, et)
    return s


def normal():
    s = rodar("normal", "/apps/testes/compartidas")
    g = re.search(r"COMPARTIDAS por hijo: (\d+) KB gastados de verdad, (\d+) KB compartidos", s)
    if g:
        print("    medida: %s KB por hijo con páginas compartidas; %d KB sin ellas" % (g.group(1), int(g.group(1)) + int(g.group(2))))


def smp():
    s = rodar("smp", "/apps/testes/compartidas", ["--smp", "--sem-cache-disco"], nucleos=4)
    g = re.search(r"COMPARTIDAS carrera: (\d+) compartidas, (\d+) rechazadas.*?escritura (\d+) veces", s)
    verificar(g is not None and int(g.group(2)) > 0 and int(g.group(3)) > 0,
              "[smp] la carrera de B1 se dio (%s rechazadas, escritura dada %s veces) y se rechazó"
              % ((g.group(2), g.group(3)) if g else ("?", "?")))


def pae6g():
    s = rodar("pae6g", "/apps/testes/compartidas", memoria="6G")
    verificar("modo PAE" in s or "PAE" in s, "[pae6g] paginación PAE")


def sempae():
    s = rodar("sempae", "/apps/testes/compartidas", ["--sem-pae"], memoria="3G")
    verificar("--sem-pae" in s, "[sempae] el kernel desactivó PAE (--sem-pae)")


def presion():
    rodar("presion", "/apps/testes/compartidas presion 4 640", memoria="512M")


def main():
    so = os.environ.get("H390_SO", "")
    partes = [("normal", normal), ("smp", smp), ("pae6g", pae6g), ("sempae", sempae), ("presion", presion)]
    for nome, f in partes:
        if so and nome not in so.split(","):
            continue
        f()
    return 0


if __name__ == "__main__":
    sys.exit(main())
