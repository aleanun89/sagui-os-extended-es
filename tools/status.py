#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""
status.py - mantiene el estado del proyecto SaguiOS.

Fuente única de verdad: dashboard/status.json.
A partir de él este script genera:
  - dashboard/status.js   (el mismo JSON envuelto en JS, para que el panel se
                           abra directamente desde el disco, sin servidor: los
                           navegadores bloquean fetch() en file://)
  - status.md             (resumen legible en la raíz del repositorio)
  - docs/historias/HXX.md (una página por historia, si aún no existe)

También recalcula datos medidos: líneas de código, tamaño de la imagen y la
lista de capturas de pantalla.

Uso:
  status.py iniciar-historia H02
  status.py concluir-historia H02 "resultado en una línea"
  status.py acao "texto"
  status.py erro "síntoma" "causa" "corrección"
  status.py tanenbaum "Procesos" feito|parcial|a fazer
  status.py aguardar-fase 1 "mensaje de fin de fase"
  status.py confirmar-fase 1 "fase 1 ok"
  status.py bloqueio "texto"   (texto vacío borra el bloqueo)
  status.py atualizar          (solo recalcula y regenera)
"""
import json
import os
import sys
import datetime

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ARQ_JSON = os.path.join(RAIZ, "dashboard", "status.json")
ARQ_JS = os.path.join(RAIZ, "dashboard", "status.js")
ARQ_MD = os.path.join(RAIZ, "status.md")
PASTA_HIST = os.path.join(RAIZ, "docs", "historias")
PASTA_CAPT = os.path.join(RAIZ, "docs", "capturas")
META_BYTES = 38 * 1024 * 1024  # 38 MB (D181; antes 16 MB, D122; 8 MB, D111)


def agora():
    return datetime.datetime.now().replace(microsecond=0).isoformat()


def carregar():
    with open(ARQ_JSON, encoding="utf-8") as f:
        return json.load(f)


def contar_linhas(pastas, extensoes):
    total = 0
    for pasta in pastas:
        base = os.path.join(RAIZ, pasta)
        for dirpath, _dirs, arquivos in os.walk(base):
            for nome in arquivos:
                if os.path.splitext(nome)[1] in extensoes:
                    with open(os.path.join(dirpath, nome), encoding="utf-8", errors="replace") as f:
                        total += sum(1 for _ in f)
    return total


def medir(estado):
    ext = {".c", ".h", ".asm", ".inc", ".ld", ".S"}
    estado["linhas"] = {
        "boot": contar_linhas(["boot"], ext),
        "kernel": contar_linhas(["kernel"], ext),
        "userland": contar_linhas(["userland"], ext),
        "ferramentas": contar_linhas(["tools", "tests"], {".py", ".sh"}),
    }
    # Tamaño útil = bytes realmente ocupados (MBR + cabecera + etapa 2 +
    # kernel + ramdisk). El build graba ese número en build/tamanho.txt.
    util = 0
    arq_tam = os.path.join(RAIZ, "build", "tamanho.txt")
    if os.path.exists(arq_tam):
        with open(arq_tam) as f:
            util = int(f.read().strip() or 0)
    img = os.path.join(RAIZ, "build", "saguios.img")
    estado["imagem"] = {
        "bytes_util": util,
        "bytes_arquivo": os.path.getsize(img) if os.path.exists(img) else 0,
        "meta_bytes": META_BYTES,
    }
    capturas = []
    if os.path.isdir(PASTA_CAPT):
        for nome in sorted(os.listdir(PASTA_CAPT)):
            if nome.lower().endswith(".png"):
                caminho = os.path.join(PASTA_CAPT, nome)
                capturas.append({"arquivo": "docs/capturas/" + nome,
                                 "nome": nome,
                                 "data": datetime.datetime.fromtimestamp(os.path.getmtime(caminho)).replace(microsecond=0).isoformat()})
    capturas.sort(key=lambda c: c["data"], reverse=True)
    estado["capturas"] = capturas


def progresso(estado):
    hs = [h for h in estado["historias"] if not h.get("bonus")]
    feitas = sum(1 for h in hs if h["estado"] in ("feito", "aguardando teste"))
    estado["progresso_geral"] = round(100.0 * feitas / len(hs), 1) if hs else 0
    for fase in estado["fases"]:
        dela = [h for h in hs if h["fase"] == fase["numero"]]
        if not dela:   # fase bonus: cuenta las historias bonus que no se cancelaron
            dela = [h for h in estado["historias"] if h["fase"] == fase["numero"] and h["estado"] != "cancelado"]
        f = sum(1 for h in dela if h["estado"] in ("feito", "aguardando teste"))
        fase["progresso"] = round(100.0 * f / len(dela), 1) if dela else 0


# Nombres visibles de los estados (las claves internas no cambian).
ESTADOS_VISIVEIS = {"feito": "hecho", "em andamento": "en curso", "aguardando teste": "esperando prueba",
                    "a fazer": "pendiente", "parcial": "parcial", "cancelado": "cancelado"}


def estado_visivel(e):
    return ESTADOS_VISIVEIS.get(e, e)


def gerar_md(estado):
    linhas = []
    if estado.get("mensagem_fase"):
        linhas += ["```", estado["mensagem_fase"].rstrip(), "```", ""]
    linhas += ["# Estado de SaguiOS", "",
               f"_Actualizado el {estado['atualizado']}_", "",
               f"- **Situación:** {estado['situacao']}",
               f"- **Fase actual:** {estado['fase_atual']}",
               f"- **Historia actual:** {estado.get('historia_atual') or '-'}",
               f"- **Progreso general:** {estado['progresso_geral']}%",
               f"- **Bloqueo:** {estado.get('bloqueio') or 'ninguno'}",
               f"- **Imagen:** {estado['imagem']['bytes_util']} bytes útiles "
               f"(meta {META_BYTES} bytes = 38 MB)",
               f"- **Líneas de código:** boot {estado['linhas']['boot']}, kernel {estado['linhas']['kernel']}, "
               f"userland {estado['linhas']['userland']}", "",
               "## Fases", "", "| Fase | Nombre | Progreso | Estado |", "| --- | --- | --- | --- |"]
    for f in estado["fases"]:
        linhas.append(f"| {f['numero']} | {f['nome']} | {f['progresso']}% | {estado_visivel(f['estado'])} |")
    linhas += ["", "## Historias", "", "| ID | Título | Estado | Minutos |", "| --- | --- | --- | --- |"]
    for h in estado["historias"]:
        estado_h = estado_visivel(h['estado'])
        if h['estado'] == "cancelado":
            estado_h = "Cancelado por Eli: " + h.get("resultado", "").replace("Cancelado por Eli: ", "")
        linhas.append(f"| {h['id']} | {h['titulo']} | {estado_h} | {h.get('minutos') or ''} |")
    linhas += ["", "## Últimas acciones", ""]
    for a in estado["acoes"][:15]:
        linhas.append(f"- {a['data']}: {a['texto']}")
    linhas.append("")
    with open(ARQ_MD, "w", encoding="utf-8") as f:
        f.write("\n".join(linhas))


def gerar_paginas(estado):
    os.makedirs(PASTA_HIST, exist_ok=True)
    for h in estado["historias"]:
        arq = os.path.join(PASTA_HIST, h["id"] + ".md")
        if os.path.exists(arq):
            continue
        with open(arq, "w", encoding="utf-8") as f:
            f.write(f"# {h['id']}. {h['titulo']}\n\n**Fase:** {h['fase']}\n\n"
                    f"## Objetivo\n\n{h['objetivo']}\n\n## Criterio de aceptación\n\n{h['aceite']}\n\n"
                    "## Resultado\n\n_Pendiente._\n")


def salvar(estado):
    estado["atualizado"] = agora()
    medir(estado)
    progresso(estado)
    texto = json.dumps(estado, ensure_ascii=False, indent=2)
    with open(ARQ_JSON, "w", encoding="utf-8") as f:
        f.write(texto + "\n")
    with open(ARQ_JS, "w", encoding="utf-8") as f:
        f.write("// Generado por tools/status.py a partir de status.json. No editar.\n")
        f.write("window.SAGUI_STATUS = " + texto + ";\n")
    gerar_md(estado)
    gerar_paginas(estado)


def historia(estado, hid):
    for h in estado["historias"]:
        if h["id"].lower() == hid.lower():
            return h
    sys.exit(f"la historia {hid} no existe")


def registrar_acao(estado, texto):
    estado["acoes"].insert(0, {"data": agora(), "texto": texto})
    del estado["acoes"][60:]


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    cmd, args = sys.argv[1], sys.argv[2:]
    estado = carregar()
    if cmd == "iniciar-historia":
        h = historia(estado, args[0])
        h["estado"] = "em andamento"
        h["inicio"] = agora()
        estado["historia_atual"] = h["id"]
        estado["fase_atual"] = h["fase"]
        estado["situacao"] = f"Trabajando en la {h['id']}: {h['titulo']}"
        for f in estado["fases"]:
            if f["numero"] == h["fase"] and f["estado"] == "a fazer":
                f["estado"] = "em andamento"
        registrar_acao(estado, f"Inicio de la {h['id']}: {h['titulo']}")
    elif cmd == "concluir-historia":
        h = historia(estado, args[0])
        h["estado"] = "feito"
        h["fim"] = agora()
        if h.get("inicio"):
            ini = datetime.datetime.fromisoformat(h["inicio"])
            fim = datetime.datetime.fromisoformat(h["fim"])
            h["minutos"] = max(1, round((fim - ini).total_seconds() / 60))
        if len(args) > 1:
            h["resultado"] = args[1]
        registrar_acao(estado, f"{h['id']} concluida" + (f": {args[1]}" if len(args) > 1 else ""))
    elif cmd == "acao":
        registrar_acao(estado, args[0])
    elif cmd == "erro":
        estado["erros"].insert(0, {"data": agora(), "sintoma": args[0], "causa": args[1], "correcao": args[2]})
        registrar_acao(estado, "Error corregido: " + args[0])
    elif cmd == "tanenbaum":
        for t in estado["tanenbaum"]:
            if t["conceito"].lower() == args[0].lower():
                t["estado"] = args[1]
                break
        else:
            sys.exit("concepto no encontrado: " + args[0])
    elif cmd == "aguardar-fase":
        n = int(args[0])
        for f in estado["fases"]:
            if f["numero"] == n:
                f["estado"] = "aguardando teste"
        for h in estado["historias"]:
            if h["fase"] == n and h["estado"] == "feito":
                h["estado"] = "aguardando teste"
        estado["situacao"] = f"Esperando la prueba de Eli: fase {n}"
        estado["historia_atual"] = None
        estado["mensagem_fase"] = args[1] if len(args) > 1 else ""
        registrar_acao(estado, f"Fase {n} entregada. Esperando la prueba de Eli.")
    elif cmd == "confirmar-fase":
        n = int(args[0])
        for f in estado["fases"]:
            if f["numero"] == n:
                f["estado"] = "feito"
                f["confirmado"] = agora()
        for h in estado["historias"]:
            if h["fase"] == n and h["estado"] == "aguardando teste":
                h["estado"] = "feito"
        estado["mensagem_fase"] = ""
        estado["situacao"] = f"Fase {n} confirmada en el hardware. Pasando a la fase {n + 1}."
        registrar_acao(estado, f"Eli confirmó: {args[1] if len(args) > 1 else 'fase ok'}")
    elif cmd == "bloqueio":
        estado["bloqueio"] = args[0] if args else ""
        if estado["bloqueio"]:
            registrar_acao(estado, "Bloqueo: " + estado["bloqueio"])
    elif cmd != "atualizar":
        sys.exit("comando desconocido: " + cmd)
    salvar(estado)


if __name__ == "__main__":
    main()
