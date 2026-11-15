#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H116 - Chat IA: janela, contexto e castelhano (D143).

A aplicacao `chatia` abre uma janela com a conversa, a pergunta em baixo e
os botoes "Enviar", "Detener" e "Nueva conversacion". Cada pergunta vai para o
servidor com a conversa anterior (o contexto), e tudo esta em castelhano.

A prova usa o servidor falso da API (tests/servidor_falso_ia.py): sem Internet
e sem modelo. Compara o que o servidor recebeu com o que se espera.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402
import servidor_falso_ia  # noqa: E402


def esperar(m, patron, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        achados = re.findall(patron, m.serial()[desde:])
        if achados:
            return achados[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {patron!r}")


def main():
    servidor = servidor_falso_ia.servidor_falso_ia().arrancar()
    porta = servidor.porta
    # A linha de ordem do auto-teste cabe em 64 bytes: o modelo e curto.
    imagem = imagem_de_teste(
        f"/apps/bin/chatia --url http://10.0.2.2:{porta}/v1 --modelo prova", ["--vesa"])
    try:
        with Maquina("h116", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("chatia: janela", 60)
            serial = m.serial()
            verificar("Chat IA" in serial, "a janela do chat abre com o nome em castelhano")
            janela = re.search(r'janela (\d+) criada por \d+: "Chat IA" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)
            verificar(janela is not None, "a posicao da janela aparece no registo")
            jx, jy = (int(janela.group(4)), int(janela.group(5))) if janela else (0, 0)

            # A interface esta em castelhano: os botoes e a instrucao.
            captura = m.captura("H116-chatia")
            verificar(os.path.getsize(captura) > 1000, "captura da janela do chat")
            m.esperar_serial("chatia: janela", 5)

            # A primeira pergunta: escreve-se na caixa de texto e pulsa-se Intro.
            antes = len(m.serial())
            m.digitar("Primera pregunta", 0.05)
            m.tecla("ret", 0.2)
            esperar(m, r"chatia: pergunta enviada \((\d+) mensagens", 30, antes)
            esperar(m, r"chatia: resposta completa \((\d+) bytes", 30, antes)
            verificar(True, "a pergunta vai ao servidor e a resposta aparece")

            # O que o servidor recebeu: a mensagem do utilizador e o systema.
            verificar(len(servidor_falso_ia.PEDIDOS) >= 1, "o servidor recebeu o pedido")
            pedido = servidor_falso_ia.PEDIDOS[-1]
            mensagens = pedido.get("messages", [])
            verificar(any(m.get("role") == "user" and "Primera pregunta" in m.get("content", "")
                          for m in mensagens),
                      "a pergunta do utilizador vai no pedido")
            verificar(any(m.get("role") == "system" and "castellano" in m.get("content", "").lower()
                          for m in mensagens),
                      "a instrucao ao modelo pede a resposta em castelhano")
            verificar(pedido.get("model") == "prova", "o modelo escolhido vai no pedido")

            # A segunda pergunta: o contexto e a conversa anterior.
            antes = len(m.serial())
            m.digitar("Segunda pregunta", 0.05)
            m.tecla("ret", 0.2)
            esperar(m, r"chatia: pergunta enviada \((\d+) mensagens", 30, antes)
            esperar(m, r"chatia: resposta completa", 30, antes)
            verificar(len(servidor_falso_ia.PEDIDOS) >= 2, "o servidor recebeu a segunda pergunta")
            pedido = servidor_falso_ia.PEDIDOS[-1]
            mensagens = pedido.get("messages", [])
            papeis = [m.get("role") for m in mensagens]
            verificar(papeis.count("user") >= 2,
                      f"o contexto leva as perguntas anteriores (papeis: {papeis})")
            verificar("assistant" in papeis,
                      f"o contexto leva a resposta do modelo (papeis: {papeis})")

            # A janela mostra a conversa: a captura final tem texto desenhado.
            captura = m.captura("H116-chatia-conversa")
            verificar(os.path.getsize(captura) > 1000, "captura com a conversa")
    finally:
        servidor.parar()
    return 0


if __name__ == "__main__":
    sys.exit(main())
