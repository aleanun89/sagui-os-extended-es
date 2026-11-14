#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H125 - o QuickJS roda dentro do SaguiOS (D149).

Critério: o programa /testes/quickjs avalia uma série de scripts e escreve
no log o valor de cada um. O teste confirma que o motor de JavaScript do
QuickJS (de terceiros, bellard/quickjs) funciona dentro do sistema: a
aritmética, os objetos, as funções, as classes, as expressões regulares, o
JSON, a data, o console, o alert, o document (title e body) e os
temporizadores.

O port do QuickJS ao SaguiOS está em userland/web/quickjs/; desde a D210 usa a
libc comum (userland/libc), com o printf e o strtod exatos.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402


def main():
    with Maquina("h125", "ide", imagem=imagem_de_teste("/testes/quickjs")) as m:
        m.esperar_serial("quickjs: erros", 180)
        s = m.serial().replace("\r", "")
        valores = {}
        for linha in s.splitlines():
            mm = re.search(r"quickjs: (\w+) = (.*)$", linha.strip())
            if mm:
                valores[mm.group(1)] = mm.group(2).strip()
        for k in sorted(valores):
            print("   ", k, "=", valores[k])

        # ------------------------------------------------ a linguagem
        verificar(valores.get("aritmetica") == "7", "a aritmética works (7)")
        verificar(valores.get("texto") == "ola mundo", "as cadeias works")
        verificar(valores.get("sqrt") == "12", "a raiz quadrada works (12)")
        verificar(valores.get("potencia") == "65536", "a potência works (65536)")
        verificar(valores.get("math") == "3.14", "as mates works (3.14)")
        verificar(valores.get("objeto") == "dois", "os objetos works")
        verificar(valores.get("array") == "1-2-3", "os arrays works")
        verificar(valores.get("seta") == "42", "as setas works (42)")
        verificar(valores.get("fecho") == "1", "os fechos works")
        verificar(valores.get("classe") == "42", "as classes works (42)")
        verificar(valores.get("regex") == "abc#", "as expressões regulares works")
        verificar('"a":1' in (valores.get("json") or "").replace(" ", ""),
                  "o JSON works (%s)" % valores.get("json"))
        verificar(valores.get("data") == "number", "a data works")

        # ------------------------------------------------ o DOM mínimo
        verificar(valores.get("titulo") == "Titulo do script",
                  "o document.title works (%s)" % valores.get("titulo"))
        verificar(valores.get("corpo") == "<p>ola</p>",
                  "o document.body.innerHTML works (%s)" % valores.get("corpo"))
        verificar("quickjs: console: mensagem 42" in s, "o console.log works")
        verificar("alerta recebida: Aviso do script" in s, "o alert works")
        verificar("temporizador disparou" in s, "o setTimeout works")

        # ------------------------------------------------ a janela (D156)
        verificar(valores.get("janela") == "object", "o window works (object)")
        verificar(valores.get("endereco") == "https://exemplo.test/inicio",
                  "o location.href works (%s)" % valores.get("endereco"))
        verificar(valores.get("url_doc") == "https://exemplo.test/inicio",
                  "o document.URL works (%s)" % valores.get("url_doc"))
        verificar("SaguiOS" in (valores.get("agente") or ""),
                  "o navigator.userAgent works (%s)" % valores.get("agente"))
        verificar("quickjs: navegar: https://exemplo.test/pagina" in s,
                  "o window.parent.location.replace works")
        verificar("quickjs: navegar: https://exemplo.test/outra" in s,
                  "o location.assign works")

        erros = re.search(r"quickjs: erros (\d+)", s)
        verificar(erros is not None and erros.group(1) == "0",
                  "nenhum script deu erro (%s)" % (erros.group(1) if erros else "?"))
        m.captura("H125-quickjs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
