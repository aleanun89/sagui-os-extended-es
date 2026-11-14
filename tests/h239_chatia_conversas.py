#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H239 - el chat con IA, ampliado: conversaciones, Markdown, preferencias (D239).

Con el servidor falso de la API (tests/servidor_falso_ia.py, sin Internet ni
modelo), la prueba abre `chatia` y comprueba:

  - la respuesta en streaming con Markdown (título, negrita, cursiva, código en
    línea, lista y un bloque de código): el programa dice en el registro qué
    encontró y dónde está el botón «Copiar» del bloque;
  - «Copiar» deja el código en el portapapeles: se pega en la pregunta
    siguiente y el servidor lo recibe tal cual;
  - la conversación se guarda en un fichero legible (título automático con las
    primeras palabras) y la nueva aparece arriba en la lista;
  - «Regenerar» vuelve a pedir la última respuesta con el mismo contexto;
  - «Adjuntar» manda un fichero de texto como bloque dentro del mensaje;
  - «Preferencias»: los modelos del servidor, el del chat elegido de la lista y
    el mensaje de sistema editado viajan en la petición siguiente, y el modelo
    queda en la configuración compartida (D145) como modelo_chatia;
  - abrir, renombrar y borrar conversaciones, y que al volver a abrir el
    programa la lista sale del disco.

Las capturas quedan en docs/capturas/H239-*.png.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402
import servidor_falso_ia  # noqa: E402

# La disposición de la ventana (coordenadas del cliente; 860x600, como en chatia.c).
LATERAL, TOPO, BASE, MARGEM = 200, 56, 96, 12
LARG, ALT = 860, 600
X0 = LATERAL + MARGEM
W = LARG - X0 - MARGEM
BT_ADJUNTAR = (X0 + 48, ALT - BASE + 14)
BT_REGENERAR = (X0 + 102 + 50, ALT - BASE + 14)
ENTRADA = (X0 + 40, ALT - BASE + 32 + 14)
BT_PREFS = (LARG - MARGEM - 60, 16)
BT_NOVA = (100, 21)
BT_RENOMBRAR = (58, ALT - 22)
BT_BORRAR = (152, ALT - 22)
# El panel de preferencias.
PX, PY = X0 + 12, TOPO + 8
CW = W - 24 - 150 - 136
BT_CARGAR = (PX + 150 + CW + 8 + 64, PY + 86 + 12)
LISTA_MOD = (PX + 150 + 30, PY + 116 + 2)
AR_SISTEMA = (PX + 20, PY + 230 + 8)
BT_GUARDAR = (PX + 50, PY + 310 + 14)


def item_lista(k):
    return (60, 40 + 2 + k * 20 + 10)


def esperar(m, patron, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        achados = re.findall(patron, m.serial()[desde:])
        if achados:
            return achados[-1]
        time.sleep(0.2)
    raise AssertionError(f"tiempo agotado esperando {patron!r}")


def esperar_pedidos(n, tempo=30):
    fim = time.time() + tempo
    while time.time() < fim and len(servidor_falso_ia.PEDIDOS) < n:
        time.sleep(0.2)
    return len(servidor_falso_ia.PEDIDOS) >= n


def ultimo_user(pedido):
    msgs = [x for x in pedido.get("messages", []) if x.get("role") == "user"]
    return msgs[-1].get("content", "") if msgs else ""


def sistema(pedido):
    for x in pedido.get("messages", []):
        if x.get("role") == "system":
            return x.get("content", "")
    return ""


def main():
    servidor = servidor_falso_ia.servidor_falso_ia().arrancar()
    url = f"http://10.0.2.2:{servidor.porta}/v1"
    imagem = imagem_de_teste("", ["--vesa"])
    try:
        with Maquina("h239", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("sagui:/$", 120)
            # Un fichero de texto para adjuntar.
            m.comando("echo linea secreta 42 > /tmp/nota.txt")

            antes = len(m.serial())
            m.comando(f"chatia --url {url} --modelo prova &")
            esperar(m, r"chatia: janela (\d+)x(\d+) pronta", 60, antes)
            g = re.findall(r'criada por \d+: "Chat IA" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial())[-1]
            jx, jy = int(g[2]) + 2, int(g[3]) + 26
            verificar((int(g[0]), int(g[1])) == (LARG, ALT), f"la ventana abre con {LARG}x{ALT}")
            sem_home = "chatia: sin /home" in m.serial()

            def clicar(p, **k):
                m.clique(jx + p[0], jy + p[1], **k)

            time.sleep(1.0)
            m.captura("H239-1-ventana")

            # 1) La respuesta en Markdown, en streaming (lenta, para verla a medias).
            antes = len(m.serial())
            n0 = len(servidor_falso_ia.PEDIDOS)
            m.digitar("Explica MARKDOWN LENTO con un ejemplo", 0.04)
            m.tecla("ret", 0.2)
            esperar(m, r"chatia: pergunta enviada", 30, antes)
            time.sleep(1.4)
            m.captura("H239-2-streaming")
            esperar(m, r"chatia: resposta completa \((\d+) bytes", 40, antes)
            md = esperar(m, r"chatia: markdown negrita=(\d+) cursiva=(\d+) titulos=(\d+) codigo=(\d+) "
                            r"listas=(\d+) bloques=(\d+)", 10, antes)
            neg, cur, tit, cod, lis, blo = (int(x) for x in md)
            verificar(neg >= 1 and cur >= 1, f"negrita y cursiva ({neg}, {cur})")
            verificar(tit == 1, f"un título ({tit})")
            verificar(cod == 1, f"código en línea ({cod})")
            verificar(lis == 2, f"dos elementos de lista ({lis})")
            verificar(blo == 1, f"un bloque de código ({blo})")
            verificar(len(servidor_falso_ia.PEDIDOS) == n0 + 1, "una petición al servidor")
            p = servidor_falso_ia.PEDIDOS[-1]
            verificar(p.get("model") == "prova", f"el modelo de la línea de órdenes ({p.get('model')})")
            verificar("castellano" in sistema(p).lower(), "el mensaje de sistema por omisión pide castellano")
            time.sleep(0.5)
            m.captura("H239-3-markdown")

            # La conversación se guardó con el título automático.
            guardada = esperar(m, r"chatia: conversacion guardada en (\S+conversa-0001\.txt)", 10, antes)
            nova = esperar(m, r"chatia: conversacion nueva conversa-0001\.txt: ([^\n\r]+)", 10, antes)
            verificar(nova.strip().startswith("Explica MARKDOWN LENTO"), f"título automático ({nova.strip()})")

            # 2) «Copiar» el bloque de código y pegarlo en la pregunta siguiente.
            bx, by = (int(x) for x in esperar(m, r"chatia: bloque 0 copiar en \((\d+),(\d+)\)", 5, antes))
            antes = len(m.serial())
            clicar((bx, by))
            copiados = int(esperar(m, r"chatia: copiado (\d+) bytes", 10, antes))
            codigo = servidor_falso_ia.CODIGO_MD
            verificar(copiados == len(codigo), f"«Copiar» deja el bloque entero ({copiados} de {len(codigo)} bytes)")
            m.captura("H239-4-copiado")
            clicar(ENTRADA)
            m.tecla("ctrl-v", 0.3)
            m.tecla("ret", 0.3)
            verificar(esperar_pedidos(n0 + 2), "la pregunta con el código pegado llega al servidor")
            esperar(m, r"chatia: resposta completa", 30, antes)
            p = servidor_falso_ia.PEDIDOS[-1]
            verificar(codigo in ultimo_user(p), "el servidor recibe el código copiado, tal cual")
            papeis = [x.get("role") for x in p.get("messages", [])]
            verificar(papeis == ["system", "user", "assistant", "user"], f"el contexto ({papeis})")

            # 3) «Regenerar» repite la petición con el mismo contexto.
            antes = len(m.serial())
            anterior = servidor_falso_ia.PEDIDOS[-1]
            clicar(BT_REGENERAR)
            esperar(m, r"chatia: regenerar", 10, antes)
            verificar(esperar_pedidos(n0 + 3), "«Regenerar» vuelve a preguntar")
            esperar(m, r"chatia: resposta completa", 30, antes)
            p = servidor_falso_ia.PEDIDOS[-1]
            verificar(p.get("messages") == anterior.get("messages"),
                      "la petición regenerada lleva el mismo contexto (sin la respuesta descartada)")
            m.captura("H239-5-regenerar")

            # 4) Adjuntar un fichero de texto.
            antes = len(m.serial())
            clicar(BT_ADJUNTAR)
            time.sleep(1.0)
            m.digitar("/tmp/nota.txt", 0.04)
            m.tecla("ret", 0.5)
            esperar(m, r"chatia: adjunto /tmp/nota\.txt \((\d+) bytes\)", 10, antes)
            time.sleep(0.5)
            m.captura("H239-6-adjunto")
            clicar(ENTRADA)
            m.digitar("Resume el adjunto", 0.04)
            m.tecla("ret", 0.3)
            verificar(esperar_pedidos(n0 + 4), "la pregunta con el adjunto llega")
            esperar(m, r"chatia: resposta completa", 30, antes)
            u = ultimo_user(servidor_falso_ia.PEDIDOS[-1])
            verificar("Resume el adjunto" in u and "linea secreta 42" in u and "nota.txt" in u,
                      "el adjunto va como bloque en el mensaje")

            # 5) Preferencias: el modelo de la lista y el mensaje de sistema.
            antes = len(m.serial())
            clicar(BT_PREFS)
            esperar(m, r"chatia: preferencias abiertas", 10, antes)
            nm = len(servidor_falso_ia.PEDIDOS_MODELOS)
            clicar(BT_CARGAR)
            esperar(m, r"chatia: (\d+) modelos cargados", 20, antes)
            verificar(len(servidor_falso_ia.PEDIDOS_MODELOS) > nm, "«Cargar modelos» pide la lista al servidor")
            clicar((LISTA_MOD[0], LISTA_MOD[1] + 20 + 10))     # el segundo: modelo-pequeno
            clicar(AR_SISTEMA)
            m.tecla("end", 0.2)
            m.digitar(" PRUEBA-SISTEMA", 0.04)
            time.sleep(0.5)
            m.captura("H239-7-preferencias")
            clicar(BT_GUARDAR)
            mod = esperar(m, r"chatia: preferencias guardadas \(modelo (\S+), (-?\d+)\)", 10, antes)
            verificar(mod[0] == "modelo-pequeno", f"el modelo elegido de la lista ({mod[0]})")
            verificar(mod[1] == "0", "la configuración se guarda")
            clicar(ENTRADA)
            m.digitar("Tercera", 0.04)
            m.tecla("ret", 0.3)
            verificar(esperar_pedidos(n0 + 5), "la pregunta con el modelo nuevo llega")
            esperar(m, r"chatia: resposta completa", 30, antes)
            p = servidor_falso_ia.PEDIDOS[-1]
            verificar(p.get("model") == "modelo-pequeno", f"la petición lleva el modelo nuevo ({p.get('model')})")
            s = sistema(p)
            verificar("PRUEBA-SISTEMA" in s and "castellano" in s.lower(),
                      f"la petición lleva el mensaje de sistema editado ({s[-40:]!r})")

            # 6) Otra conversación: aparece arriba en la lista.
            antes = len(m.serial())
            clicar(BT_NOVA)
            time.sleep(0.5)
            clicar(ENTRADA)
            m.digitar("Otra conversacion distinta", 0.04)
            m.tecla("ret", 0.3)
            esperar(m, r"chatia: conversacion nueva conversa-0002\.txt", 20, antes)
            esperar(m, r"chatia: resposta completa", 30, antes)
            p = servidor_falso_ia.PEDIDOS[-1]
            papeis = [x.get("role") for x in p.get("messages", [])]
            verificar(papeis == ["system", "user"], f"la conversación nueva empieza sin contexto ({papeis})")
            time.sleep(0.5)
            m.captura("H239-8-dos-conversaciones")

            # 7) Abrir la primera (ahora la segunda de la lista), con su sistema.
            antes = len(m.serial())
            clicar(item_lista(1))
            ab = esperar(m, r"chatia: conversacion abierta conversa-0001\.txt \((\d+) mensajes\)", 10, antes)
            verificar(int(ab) == 8, f"la conversación abierta tiene sus mensajes ({ab})")
            time.sleep(0.5)
            m.captura("H239-9-abierta")
            clicar(ENTRADA)
            m.digitar("Sigue", 0.04)
            m.tecla("ret", 0.3)
            esperar(m, r"chatia: resposta completa", 30, antes)
            p = servidor_falso_ia.PEDIDOS[-1]
            verificar("PRUEBA-SISTEMA" in sistema(p), "la conversación guardada recuerda su mensaje de sistema")
            verificar(len(p.get("messages", [])) == 1 + 9, f"y su contexto ({len(p.get('messages', []))} mensajes)")

            # 8) Renombrar la abierta.
            antes = len(m.serial())
            clicar(BT_RENOMBRAR)
            time.sleep(1.0)
            for _ in range(45):
                m.tecla("backspace", 0.02)
            m.digitar("Renombrada", 0.04)
            m.tecla("ret", 0.5)
            esperar(m, r"chatia: conversacion renombrada conversa-0001\.txt: Renombrada", 10, antes)

            # 9) Borrar la otra (la de arriba): se abre y se borra.
            clicar(item_lista(0))
            esperar(m, r"chatia: conversacion abierta conversa-0002\.txt", 10, antes)
            clicar(BT_BORRAR)
            time.sleep(1.0)
            m.tecla("ret", 0.5)          # «Sí»
            esperar(m, r"chatia: conversacion borrada conversa-0002\.txt \(0\)", 10, antes)
            time.sleep(0.5)
            m.captura("H239-10-borrada")

            # Se cierra y se mira el fichero.
            m.tecla("alt-f4", 1.0)
            time.sleep(1.0)
            pasta = os.path.dirname(guardada)
            out = m.comando(f"cat {pasta}/conversa-0001.txt")
            verificar("titulo: Renombrada" in out, "el fichero tiene el título nuevo")
            verificar("--- system" in out and "PRUEBA-SISTEMA" in out, "y el mensaje de sistema")
            verificar(out.count("--- user") == 5 and out.count("--- assistant") == 5,
                      f"y los mensajes ({out.count('--- user')} user, {out.count('--- assistant')} assistant)")
            out = m.comando(f"ls {pasta}")
            verificar("conversa-0002" not in out, "la conversación borrada ya no está")
            out = m.comando("cat /tmp/ia/provedor")
            if "url=" not in out:
                out += m.comando("cat /home/.config/ia/provedor")
            verificar("modelo_chatia=modelo-pequeno" in out, "el modelo del chat queda en la configuración (D145)")
            verificar(f"url={url}" in out, "y el servidor")
            if sem_home:
                verificar(pasta.startswith("/tmp/"), "sin /home las conversaciones van a /tmp (y se avisa)")

            # Al volver a abrir, la lista sale del disco.
            antes = len(m.serial())
            m.comando("chatia &")
            n = esperar(m, r"chatia: (\d+) conversaciones en", 60, antes)
            verificar(int(n) == 1, f"la lista se lee del disco ({n})")
            esperar(m, r"chatia: janela", 30, antes)
            time.sleep(1.0)
            m.captura("H239-11-reabierto")
            m.tecla("alt-f4", 1.0)
    finally:
        servidor.parar()
    return 0


if __name__ == "__main__":
    sys.exit(main())
