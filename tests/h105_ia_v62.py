#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H105 - cliente de chat com IA: o comando `ia` em streaming (D134).

Criterio:
  1. (e1000, no console) com o servidor falso da API da OpenAI (do
     anfitrión, sem Internet e sem modelo real), `ia --modelos` lista os
     modelos e `ia --provar` dá a conexão por boa; o endereço vem do
     ambiente (IA_URL), não do código;
  2. `ia "pergunta"` escreve a resposta em streaming: cada pedaço aparece
     assim que chega, e a resposta completa sai inteira;
  3. o "Detener" respeita-se: com `--limite 2` a resposta para a meio,
     avisa que ficou interrompida e o processo acaba bem;
  4. os erros do servidor (401, 429, 500) aparecem como mensagem, sem
     queda do sistema;
  5. a chave de API vai no cabecalho "Authorization": o servidor falso
     exige-a, e sem ela devolve 401;
  6. por HTTPS (TLS) a resposta tambem chega: o caminho do HTTPS usa a
     libtls, como o do curl.

O servidor falso (tests/servidor_falso_ia.py) e o mesmo que se pode
arrancar a mao: python3 tests/servidor_falso_ia.py. Nao ha Internet, nem
modelo, nem chave de verdade.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402
from servidor_falso_ia import servidor_falso_ia  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESPOSTA = "uma resposta de exemplo do servidor falso."


def esperar_serial(m, texto, n=1, tempo=60):
    fim = time.time() + tempo
    while time.time() < fim:
        if m.serial().count(texto) >= n:
            return True
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {n}x {texto!r}")


def n_prompts(m):
    """Quantos prompts "sagui:/$" ja apareceram na serie."""
    return m.serial().count("sagui:/$")


def esperar_prompt(m, antes, tempo=60):
    """Espera que apareca um prompt NOVO (o anterior ja nao serve)."""
    fim = time.time() + tempo
    while time.time() < fim:
        if n_prompts(m) > antes:
            return True
        time.sleep(0.2)
    raise AssertionError("tempo esgotado esperando o prompt seguinte")


def parte_console():
    # O servidor falso escuta em 127.0.0.1; na rede de utilizador do QEMU o
    # anfitrion ve-se em 10.0.2.2 (e o curl da h44 usa o mesmo endereco).
    srv = servidor_falso_ia().arrancar()
    url = f"http://10.0.2.2:{srv.porta}/v1"
    try:
        with Maquina("h105", "ide", imagem=imagem_console(), extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("sagui:/$", 120)
            time.sleep(1)

            # O endereco do servidor falso entra no ambiente da prova, nao
            # no codigo do programa.
            m.comando(f"export IA_URL={url}", tempo=30)
            m.comando("export IA_MODELO=modelo-de-prova", tempo=30)

            out = m.comando("ia --config", tempo=30)
            verificar(url in out, "ia --config mostra o endereco configurado")
            verificar("(sem chave)" in out, "ia --config nao mostra a chave (nao ha)")

            out = m.comando("ia --modelos", tempo=40)
            verificar("modelo-de-prova" in out and "modelo-pequeno" in out,
                      "ia --modelos lista os modelos do servidor falso")

            out = m.comando("ia --provar", tempo=40)
            verificar("Conexao correcta" in out and "2 modelos" in out,
                      "ia --provar confirma a conexao e conta os modelos")

            # A conversa: a resposta sai inteira.
            antes = len(m.serial())
            m.comando('ia "hola"', tempo=60)
            saida = m.serial()[antes:].replace("\r", "")
            verificar(RESPOSTA in saida, "ia escreve a resposta completa")
            verificar("[SAGUI] EXCECAO" not in saida, "sem excecoes no kernel")

            # O streaming: cada pedaco aparece assim que chega. O servidor
            # falso manda os pedacos com 0,4 s de intervalo; olha-se a
            # serie a meio da resposta, que ja tem o primeiro pedaco mas
            # ainda nao o ultimo.
            antes = len(m.serial())
            prompts_antes = n_prompts(m)
            m.digitar('ia "LENTO"', 0.04)
            m.tecla("ret", 0.04)
            ver_ao_menor = False
            fim = time.time() + 30
            while time.time() < fim:
                parcial = m.serial()[antes:].replace("\r", "")
                if "Hola, " in parcial and RESPOSTA not in parcial:
                    ver_ao_menor = True
                    break
                if n_prompts(m) > prompts_antes:
                    break
                time.sleep(0.1)
            verificar(ver_ao_menor,
                      "a resposta chega em streaming: o primeiro pedaco aparece antes do fim")
            esperar_prompt(m, prompts_antes)

            # O "Detener": com --limite 2 a resposta para a meio e avisa.
            antes = len(m.serial())
            out = m.comando('ia --limite 2 "LENTO"', tempo=60)
            saida = m.serial()[antes:].replace("\r", "")
            verificar("[resposta interrompida]" in saida,
                      "o Detener respeita-se: a resposta interrompida avisa")
            verificar(RESPOSTA not in saida, "com o Detener a resposta nao chega ao fim")

            # Os erros do servidor.
            antes = len(m.serial())
            m.comando('ia "429"', tempo=40)
            saida = m.serial()[antes:].replace("\r", "")
            verificar("demasiados pedidos" in saida, "o erro 429 do servidor aparece na saida")

            antes = len(m.serial())
            m.comando('ia "500"', tempo=40)
            saida = m.serial()[antes:].replace("\r", "")
            verificar("erro do servidor" in saida, "o erro 500 do servidor aparece na saida")

            # A chave de API: com ela o servidor aceita; sem ele, 401.
            antes = len(m.serial())
            m.comando('export IA_CHAVE=chave-de-prova', tempo=30)
            out = m.comando('ia "hola"', tempo=60)
            saida = m.serial()[antes:].replace("\r", "")
            verificar(RESPOSTA in saida, "com a chave de API a resposta chega")
            verificar("[SAGUI] EXCECAO" not in saida, "sem excecoes no kernel com a chave")
    finally:
        srv.parar()


def parte_chave_requerida():
    """O mesmo servidor, mas a exigir a chave: sem ela, 401."""
    srv = servidor_falso_ia(chave="segredo-da-prova").arrancar()
    url = f"http://10.0.2.2:{srv.porta}/v1"
    try:
        with Maquina("h105-chave", "ide", imagem=imagem_console(), extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("sagui:/$", 120)
            time.sleep(1)
            m.comando(f"export IA_URL={url}", tempo=30)
            antes = len(m.serial())
            m.comando('ia "hola"', tempo=40)
            saida = m.serial()[antes:].replace("\r", "")
            verificar("chave errada" in saida, "sem a chave de API o servidor responde 401")
            m.comando('export IA_CHAVE=segredo-da-prova', tempo=30)
            antes = len(m.serial())
            m.comando('ia "hola"', tempo=60)
            saida = m.serial()[antes:].replace("\r", "")
            verificar(RESPOSTA in saida, "com a chave certa o servidor responde o texto")
    finally:
        srv.parar()


def parte_https():
    """O mesmo servidor, mas por HTTPS: o caminho do TLS tambem funciona."""
    srv = servidor_falso_ia(tls=True).arrancar()
    url = f"https://10.0.2.2:{srv.porta}/v1"
    try:
        with Maquina("h105-https", "ide", imagem=imagem_console(), extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("sagui:/$", 120)
            time.sleep(1)
            m.comando(f"export IA_URL={url}", tempo=30)
            # "-k": o certificado do servidor falso e' autoassinado (so para
            # a prova); o que se ve e' que o caminho do HTTPS funciona.
            antes = len(m.serial())
            m.comando('ia -k "hola"', tempo=90)
            saida = m.serial()[antes:].replace("\r", "")
            verificar(RESPOSTA in saida, "por HTTPS a resposta chega (com -k, certificado de prova)")
            verificar("[SAGUI] EXCECAO" not in saida, "sem excecoes no kernel com HTTPS")
    finally:
        srv.parar()


def main():
    parte_console()
    parte_chave_requerida()
    parte_https()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as e:
        print("FALLO:", e)
        sys.exit(1)
