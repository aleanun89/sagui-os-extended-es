#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H117 - la página de la IA en «Configuración» (D145).

La ventana de Configuración tiene una sección «IA» (la séptima) donde se elige
el proveedor (OpenAI, OpenRouter, llama.cpp, Ollama, LM Studio, Groq,
DeepSeek o el personalizado), la dirección del servidor, la clave de API y el
modelo de cada programa: el general, el del terminal, el del chat con ventana
y el del Mico. Lo que se guarda en /home/.config/ia lo leen los tres
programas, cada uno con su modelo.

La prueba usa el servidor falso de la API (tests/servidor_falso_ia.py): sin
Internet y sin modelo de verdad.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402
import servidor_falso_ia  # noqa: E402

# Las pestañas de la ventana: cada una es del ancho de su título (D145).
TITULOS_PAG = ("Teclado", "Fecha y hora", "Pantalla", "Sonido", "Escritorio", "Capturas", "IA")

# La página de la IA, en coordenadas del cliente (como en el programa).
Y_PAG = 50
CX_URL = 250
CY_URL = Y_PAG + 22
CX_GERAL = 250
CY_GERAL = Y_PAG + 118
CX_CHAT = 250
CY_CHAT = Y_PAG + 166
CX_GUARDAR, CY_GUARDAR, LARG_GUARDAR = 250, Y_PAG + 206, 120
CX_CARGAR, LARG_CARGAR = 380, 170
LISTA_PROV_X, LISTA_PROV_Y = 24, Y_PAG + 24
LISTA_MOD_X, LISTA_MOD_Y = 250, Y_PAG + 280
ITEM = 20          # LISTA_ALTURA_ITEM
ABA_TAB = 24       # dentro de la pestaña (los botones tienen 28 de alto, en y=10)


def pos_paginas():
    """La posición y la anchura de cada pestaña, como las calcula el programa."""
    x = 10
    pos = []
    for t in TITULOS_PAG:
        l = len(t) * 8 + 10   # como la ventana y h83: anchura del título + 10
        pos.append((x, l))
        x += l + 2
    return pos


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]), y=int(g[3]))


def ponto(j, cx, cy):
    """Las coordenadas de pantalla de un punto del cliente de la ventana."""
    return (j["x"] + 2 + cx, j["y"] + 26 + cy)


def esperar(m, texto, tempo=30):
    fim = time.time() + tempo
    while time.time() < fim:
        if texto in m.serial():
            return True
        time.sleep(0.2)
    raise AssertionError(f"tiempo agotado esperando {texto!r}")


def main():
    servidor = servidor_falso_ia.servidor_falso_ia().arrancar()
    porta = servidor.porta
    url = f"http://10.0.2.2:{porta}/v1"
    imagem = imagem_de_teste("", ["--vesa"])
    try:
        with Maquina("h117", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("sagui:/$", 120)
            # La ventana de Configuración, en segundo plano.
            m.comando("configuracion &")
            esperar(m, '"Configuración"')
            time.sleep(1.0)
            j = janela(m.serial(), "Configuración")
            verificar(j["l"] == 840 and j["a"] == 500, f"la ventana abre con su tamaño ({j['l']}x{j['a']})")

            # La pestaña «IA» es la séptima.
            pos = pos_paginas()
            m.clique(*ponto(j, pos[6][0] + pos[6][1] // 2, ABA_TAB))
            time.sleep(0.8)
            m.captura("H117-1-pagina-ia")
            verificar(True, "la página de la IA abre")

            # El proveedor «Personalizado» vacía la caja de la dirección.
            m.clique(*ponto(j, LISTA_PROV_X + 30, LISTA_PROV_Y + 2 + 7 * ITEM + 10))
            time.sleep(0.6)
            m.captura("H117-2-provedor-personalizado")

            # Escribir la dirección del servidor falso en la caja de la URL.
            m.clique(*ponto(j, CX_URL + 20, CY_URL + 10))
            time.sleep(0.3)
            m.digitar(url, 0.03)
            time.sleep(0.5)
            m.captura("H117-3-url")

            # «Cargar modelos» pide la lista al servidor.
            antes = len(servidor_falso_ia.PEDIDOS_MODELOS)
            m.clique(*ponto(j, CX_CARGAR + LARG_CARGAR // 2, CY_GUARDAR + 14))
            fim = time.time() + 30
            while time.time() < fim and len(servidor_falso_ia.PEDIDOS_MODELOS) == antes:
                time.sleep(0.2)
            verificar(len(servidor_falso_ia.PEDIDOS_MODELOS) > antes, "el botón pide los modelos al servidor")
            time.sleep(1.0)
            m.captura("H117-4-modelos")
            verificar(True, "los modelos del servidor llegan y la lista se llena")

            # El primer modelo va a la casilla «Modelo por omisión» (el destino
            # por omisión; no hay que hacer clic antes en ella).
            m.clique(*ponto(j, LISTA_MOD_X + 30, LISTA_MOD_Y + 2 + 10))
            time.sleep(0.5)
            # El segundo va al «Chat»: se elige el destino con su botón.
            m.clique(*ponto(j, 529 + 44, Y_PAG + 240 + 14))
            time.sleep(0.4)
            m.clique(*ponto(j, LISTA_MOD_X + 30, LISTA_MOD_Y + 2 + ITEM + 10))
            time.sleep(0.5)
            m.captura("H117-5-modelos-escolhidos")

            # Guardar.
            m.clique(*ponto(j, CX_GUARDAR + LARG_GUARDAR // 2, CY_GUARDAR + 14))
            time.sleep(1.0)
            m.captura("H117-6-guardado")

            # Se cierra la ventana: con una ventana abierta el teclado va a ella
            # y la consola serial deja de recibir teclas.
            m.clique(*ponto(j, j["l"] - 110 + 48, j["a"] - 40 + 14))
            time.sleep(1.5)

            # Lo que quedó en el fichero: si /home no es persistente, la
            # configuración va a /tmp (vale solo para la sesión).
            out = m.comando("cat /tmp/ia/provedor")
            if "url=" not in out:
                out += m.comando("cat /home/.config/ia/provedor")
            verificar(f"url={url}" in out, f"el fichero tiene la url ({out.strip()[:80]})")
            verificar("modelo=modelo-de-prova" in out, "el modelo por omisión es el primero de la lista")
            verificar("modelo_chatia=modelo-pequeno" in out, "el chat tiene su propio modelo")
            verificar("modelos=modelo-de-prova,modelo-pequeno" in out, "el catálogo del servidor queda guardado")
            # La clave se guarda en su propio fichero: como no se escribió
            # ninguna, el fichero está vacío (solo el salto de línea).
            tam = m.comando("wc -c /tmp/ia/chave")
            numeros = re.findall(r"\d+", tam)
            verificar(numeros and int(numeros[-1]) <= 1,
                      f"sin clave, el fichero de la clave está vacío ({tam.strip()[-40:]})")

            # El terminal usa el modelo general.
            out = m.comando("ia --config")
            verificar(f"URL:    {url}" in out, f"el terminal lee la url ({out.strip()[:80]})")
            verificar("Modelo: modelo-de-prova" in out, "el terminal usa el modelo por omisión")

            # Y manda ese modelo al servidor.
            antes = len(servidor_falso_ia.PEDIDOS)
            m.comando('ia "ola"')
            time.sleep(1.5)
            verificar(len(servidor_falso_ia.PEDIDOS) > antes, "el terminal habla con el servidor")
            pedido = servidor_falso_ia.PEDIDOS[-1]
            verificar(pedido.get("model") == "modelo-de-prova",
                      f"el terminal manda el modelo por omisión ({pedido.get('model')})")

            # El chat con ventana usa el suyo (D145).
            m.comando("chatia &")
            esperar(m, '"Chat IA"')
            time.sleep(1.0)
            c = janela(m.serial(), "Chat IA")
            m.captura("H117-7-chat")
            # La caja de texto del chat está abajo: hay que hacer clic en ella.
            m.clique(c["x"] + 2 + 12 + 40, c["y"] + 26 + (600 - 96 + 32) + 10)
            time.sleep(0.5)
            antes = len(servidor_falso_ia.PEDIDOS)
            m.digitar("ola do chat", 0.05)
            m.tecla("ret", 0.3)
            time.sleep(2.0)
            verificar(len(servidor_falso_ia.PEDIDOS) > antes, "el chat habla con el servidor")
            pedido = servidor_falso_ia.PEDIDOS[-1]
            verificar(pedido.get("model") == "modelo-pequeno",
                      f"el chat manda el modelo que le asignaron ({pedido.get('model')})")
    finally:
        servidor.parar()
    return 0


if __name__ == "__main__":
    sys.exit(main())
