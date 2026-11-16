#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H351 - PAE (hasta 64 GB) con doble modo, y NX (D349, fase 2 de la mejora 125).

Partes (por defecto todas; o las de H351_SO, separadas por comas):
  6G, 12G, 16G  BIOS, CPU por defecto (qemu32: PAE sin NX): el arranque elige PAE y ve la zona
                superior (por encima de 4 GB); `free` y `mem` la muestran; varios procesos a la
                vez llenan más de 4 GB entre todos, esperan a que el sistema tenga en uso lo de
                todos y comprueban el patrón y la suma FNV de todo su contenido (iguales antes y
                después); la memoria vuelve entera; el cc compila y su ejecutable funciona.
  smp           6G con --smp, 4 CPU y NX (qemu32,+nx): las secundarias ponen PAE y EFER.NXE
                en el trampolín (si no, fallo de página por bit reservado en cuanto arrancan).
  uefi          6G con UEFI (OVMF de 64 bits; la CPU qemu64 trae NX).
  sinpae        CPU sin PAE (qemu32,-pae) con 3G (el QEMU no deja más sin PAE): modo de siempre
                (2 niveles), la zona alta bajo 4 GB y los programas funcionan.
  opcion        imagen con --sem-pae en una CPU con PAE y 6G: modo de siempre igual, y lo de
                encima de 4 GB queda sin usar (con un aviso en el registro).
  nx            CPU con NX (qemu32,+nx), 2G: un programa que salta a la pila muere con un
                mensaje claro (NX), lo mismo en el montón; el código generado tras pedir
                MEM_PROT_EXEC funciona; el cc compila y su ejecutable corre; con --sem-nx la
                pila vuelve a ejecutarse.
  auto          autopruebas del kernel (memoria, paginacao, anel3) con PAE, NX y 6G.
Con SAGUI_KVM=1 se prueba con KVM (la CPU es la del anfitrión, filtrada por el modelo).
Ojo: 12G y 16G piden esa memoria al anfitrión; no lanzar varias a la vez.
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, imagem_de_teste, achar_ovmf, BUILD  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UEFI_DIR = os.path.join(BUILD, "boot", "uefi")
CPU_NX = ["-cpu", "qemu32,+nx"]


def free_m(m):
    """{"mem", "baja", "alta", "sup"}: (total, usado, libre) en MB, de `free -m`."""
    out = m.comando("free -m")
    r = {}
    for nome, rot in (("mem", r"Mem:"), ("baja", r"baja:"), ("alta", r"alta:"), ("sup", r">4 GB:")):
        g = re.search(rot + r"\s+(\d+)\s+(\d+)\s+(\d+)", out)
        r[nome] = tuple(int(x) for x in g.groups()) if g else (0, 0, 0)
    return r, out


def sem_erros(s, et):
    verificar("EXCECAO" not in s, f"[{et}] sin excepciones del kernel")
    verificar("ERRO memalta" not in s, f"[{et}] sin errores de las ventanas temporales")
    verificar("liberando pagina fisica invalida" not in s, f"[{et}] ninguna página liberada dos veces")


def modo(s):
    g = re.search(r"paginacao: modo (PAE|sem PAE), NX (sim|nao)", s)
    return (g.group(1), g.group(2)) if g else ("?", "?")


def com_pae(nome, memoria, et, k, mb, imagem=None, nucleos=1, firmware=None, extras=None, nx=None):
    """Arranca con "memoria", comprueba el modo PAE y llena más de 4 GB con k procesos de mb MB."""
    img = imagem or imagem_console()
    with Maquina(nome, "ide", imagem=img, memoria=memoria, nucleos=nucleos, firmware=firmware, extras=extras) as m:
        m.esperar_serial("sagui:/$", 400)
        s = m.serial()
        pm, pnx = modo(s)
        verificar(pm == "PAE", f"[{et}] el arranque elige PAE ({pm})")
        if nx is not None:
            verificar(pnx == ("sim" if nx else "nao"), f"[{et}] NX {'activo' if nx else 'no activo'} ({pnx})")
        g = re.search(r"memoria superior: (\d+) paginas livres \((\d+) MB\)", s)
        sup_mb = int(g.group(2)) if g else 0
        verificar(sup_mb > 1000, f"[{et}] el arranque ve memoria por encima de 4 GB ({sup_mb} MB)")
        if nucleos != 1:
            g = re.search(r"SMP: (\d+) procesador\(es\) trabajando", s)
            verificar(g is not None and int(g.group(1)) == nucleos and "no arrancó" not in s,
                      f"[{et}] las {nucleos} CPU trabajan ({g.group(1) if g else '?'}): PAE y NX en el trampolín")
        f0, out = free_m(m)
        verificar(f0["sup"][0] > 1000, f"[{et}] free: {f0['sup'][0]} MB por encima de 4 GB")
        verificar(f0["mem"][0] > 4200, f"[{et}] free: {f0['mem'][0]} MB gestionados (más de 4 GB)")
        verificar("PAE" in out, f"[{et}] free dice el modo de paginación")
        out = m.comando("mem")
        verificar(re.search(r"por encima de 4 GB:\s+\d{3,} MB", out) is not None and "Paginacion:" in out,
                  f"[{et}] mem muestra la memoria por encima de 4 GB y el modo")
        out = m.comando(f"/testes/memoria varios {k} {mb}", 3600)
        picos = [int(x) for x in re.findall(r"pico del sistema (\d+) MB", out)]
        sups = [int(x) for x in re.findall(r"superior usada (\d+) MB", out)]
        pico = max(picos) if picos else -1
        verificar(f"varios: {k} de {k} procesos con {mb} MB cada uno: todos ok" in out,
                  f"[{et}] {k} procesos de {mb} MB: patrón y suma correctos tras llenar la memoria")
        verificar(out.count("igual, 0 errores") == k, f"[{et}] las {k} sumas de comprobación coinciden")
        verificar(pico > 4096, f"[{et}] en uso a la vez: {pico} MB (más de 4 GB)")
        verificar(sups and max(sups) > 1000, f"[{et}] páginas por encima de 4 GB en uso: {max(sups) if sups else -1} MB")
        f1, _ = free_m(m)
        verificar(abs(f1["mem"][2] - f0["mem"][2]) <= 8, f"[{et}] al terminar vuelve la memoria ({f0['mem'][2]} -> {f1['mem'][2]} MB libres)")
        verificar(abs(f1["sup"][2] - f0["sup"][2]) <= 1, f"[{et}] vuelve toda la memoria superior")
        out = m.comando("cc /ejemplos/hola.c -o /tmp/hola", tempo=180)
        out2 = m.comando("/tmp/hola Tito")
        verificar("(cc) terminou com codigo 0" in out and "Hola, Tito!" in out2, f"[{et}] cc y su ejecutable funcionan")
        sem_erros(m.serial(), et)
        m.captura(f"H351-{nome}")


def sem_pae(nome, et, memoria, imagem=None, extras=None, opcao=False):
    """Modo de siempre (2 niveles): la alta bajo 4 GB y nada por encima (con 6G, lo de
    encima de 4 GB queda sin usar). El QEMU no deja dar más de 4 GB a una CPU sin PAE."""
    img = imagem or imagem_console()
    with Maquina(nome, "ide", imagem=img, memoria=memoria, extras=extras) as m:
        m.esperar_serial("sagui:/$", 300)
        s = m.serial()
        pm, pnx = modo(s)
        verificar(pm == "sem PAE" and pnx == "nao", f"[{et}] modo de siempre, sin PAE ni NX ({pm}, NX {pnx})")
        if opcao:
            verificar("--sem-pae" in s, f"[{et}] el registro dice que se desactivó con --sem-pae")
        verificar("2 niveis" in s and "paginas de 4 MB" in s, f"[{et}] tablas de 2 niveles y páginas de 4 MB")
        verificar("memoria superior:" not in s, f"[{et}] sin zona superior")
        if memoria == "6G":
            verificar(re.search(r"por encima de 4 GB no se usan \(sin PAE\)", s) is not None,
                      f"[{et}] avisa de la RAM por encima de 4 GB que no se usa")
        f0, out = free_m(m)
        verificar(f0["alta"][0] > 800 and f0["sup"][0] == 0, f"[{et}] free: alta {f0['alta'][0]} MB, nada por encima de 4 GB")
        verificar("sin PAE" in out, f"[{et}] free dice «sin PAE»")
        out = m.comando("/testes/memoria varios 4 600", 1800)
        verificar("varios: 4 de 4 procesos con 600 MB cada uno: todos ok" in out, f"[{et}] cuatro procesos de 600 MB")
        out = m.comando("/testes/memoria nxpila", 60)
        verificar("sin NX, se ejecuta" in out, f"[{et}] sin NX la pila se ejecuta como siempre")
        out = m.comando("/testes/memoria exec", 60)
        verificar("devolvio 42: ok" in out, f"[{et}] memoria_proteger con MEM_PROT_EXEC funciona sin NX")
        sem_erros(m.serial(), et)


def nx():
    et = "NX"
    with Maquina("h351-nx", "ide", imagem=imagem_console(), memoria="2G", extras=CPU_NX) as m:
        m.esperar_serial("sagui:/$", 300)
        s = m.serial()
        verificar(modo(s) == ("PAE", "sim"), f"[{et}] PAE con NX activo ({modo(s)})")
        verificar("NX ativo" in s, f"[{et}] la paginación dice NX activo")
        out = m.comando("/testes/memoria nxpila", 60)
        verificar("ejecutar memoria no ejecutable (NX)" in out and "devolvio" not in out,
                  f"[{et}] saltar a la pila mata el programa con un mensaje claro")
        verificar("[SAGUI] NX: proceso" in m.serial(), f"[{et}] el registro del kernel apunta el intento")
        out = m.comando("/testes/memoria nxmonton", 60)
        verificar("ejecutar memoria no ejecutable (NX)" in out and "devolvio" not in out,
                  f"[{et}] saltar al montón también")
        out = m.comando("/testes/memoria exec", 60)
        verificar("devolvio 42: ok" in out, f"[{et}] el código generado tras pedir MEM_PROT_EXEC se ejecuta")
        verificar("sin asignar -> -" in out and "error, ok" in out,
                  f"[{et}] MEM_PROTEGER con páginas sin asignar da error (como el ENOMEM de mprotect)")
        out = m.comando("cc /ejemplos/hola.c -o /tmp/hola", tempo=180)
        out2 = m.comando("/tmp/hola Tito")
        verificar("(cc) terminou com codigo 0" in out and "Hola, Tito!" in out2, f"[{et}] cc y su ejecutable funcionan")
        out = m.comando("/testes/memoria 200", 300)
        verificar("patron ok en 200 MB" in out, f"[{et}] un programa normal usa su montón")
        out = m.comando("free", 30)
        verificar("NX activo" in out, f"[{et}] free dice NX activo")
        sem_erros(m.serial(), et)
        m.captura("H351-nx")
    et = "--sem-nx"
    img = imagem_de_teste("", ["--vesa", "--console", "--sem-nx"])
    with Maquina("h351-semnx", "ide", imagem=img, memoria="2G", extras=CPU_NX) as m:
        m.esperar_serial("sagui:/$", 300)
        s = m.serial()
        verificar(modo(s) == ("PAE", "nao") and "--sem-nx" in s, f"[{et}] PAE sin NX ({modo(s)})")
        out = m.comando("/testes/memoria nxpila", 60)
        verificar("sin NX, se ejecuta" in out, f"[{et}] la pila vuelve a ejecutarse")
        sem_erros(m.serial(), et)


def autotestes():
    for t in ("memoria", "paginacao", "anel3"):
        with Maquina(f"h351-auto-{t}", "ide", imagem=imagem_de_teste(t), memoria="6G", extras=CPU_NX) as m:
            m.esperar_serial(f"[TESTE] {t} ", 240)
            s = m.serial()
            verificar(f"[TESTE] {t} ok" in s and "[TESTE] falha" not in s, f"[6G NX] autoprueba del kernel '{t}' pasa")
            verificar(modo(s) == ("PAE", "sim"), f"[6G NX] autoprueba '{t}' con PAE y NX")
            sem_erros(s, f"6G {t}")


def main():
    r = subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "auditar_memalta.py")],
                       capture_output=True, text=True)
    verificar(r.returncode == 0, "auditor: ninguna dirección física de programa usada como puntero" +
              ("" if r.returncode == 0 else "\n" + r.stdout))

    so = os.environ.get("H351_SO", "")
    partes = [
        ("nx", nx),
        ("sinpae", lambda: sem_pae("h351-sinpae", "CPU sin PAE", "3G", extras=["-cpu", "qemu32,-pae"])),
        ("opcion", lambda: sem_pae("h351-opcion", "--sem-pae", "6G", opcao=True,
                                   imagem=imagem_de_teste("", ["--vesa", "--console", "--sem-pae"]))),
        ("auto", autotestes),
        ("6G", lambda: com_pae("h351-6G", "6G", "6G", k=5, mb=1000)),
        ("smp", lambda: com_pae("h351-smp", "6G", "6G --smp NX", k=5, mb=1000, nucleos=4, extras=CPU_NX, nx=True,
                                imagem=imagem_de_teste("", ["--vesa", "--console", "--smp"]))),
        ("12G", lambda: com_pae("h351-12G", "12G", "12G", k=9, mb=1100)),
        ("16G", lambda: com_pae("h351-16G", "16G", "16G", k=12, mb=1100)),
    ]
    if achar_ovmf("x64") and os.path.isdir(UEFI_DIR):
        partes.append(("uefi", lambda: com_pae("h351-uefi", "6G", "6G UEFI", k=5, mb=1000, firmware="x64", nx=True,
                                                imagem=imagem_de_teste("", ["--vesa", "--console", "--uefi", UEFI_DIR]))))
    for nome, f in partes:
        if so and nome not in so.split(","):
            continue
        print(f"--- {nome}")
        f()
    return 0


if __name__ == "__main__":
    sys.exit(main())
