#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H38 - sagsh completo (D037): um shell tipo sh/bash.

Critério: no terminal de tela cheia, o sagsh entende variáveis, $?, && || ;,
if/for/while/case, funções, $(...), $((...)), glob, 2>&1, here-doc (com o
PS2 "> " de continuação), scripts com #!, ambiente herdado pelos programas
(env), read, trabalhos (jobs/wait) e erros de sintaxe claros. No Terminal em
janela (terminal virtual, D036), o Ctrl+C encerra o trabalho em primeiro
plano inteiro (todas as etapas do pipe, ou o script e o que ele roda), o $?
fica 130, e um laço interno do próprio shell também é interrompido.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402


def linhas(out):
    return [l.rstrip() for l in out.splitlines()]


def tem_linha(out, texto):
    return texto in linhas(out)


def console():
    with Maquina("h38", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)

        # Variáveis, $? e listas.
        out = m.comando('x=42; echo "valor=$x ${x}" \'$x\'')
        verificar(tem_linha(out, "valor=42 42 $x"), "variáveis: $x, ${x} e aspas simples literais")
        out = m.comando('false; echo "st=$?"; true && echo y1 || echo n1; false && echo y2 || echo n2')
        verificar(tem_linha(out, "st=1") and tem_linha(out, "y1") and tem_linha(out, "n2") and not tem_linha(out, "y2"),
                  "$?, && e || com ;")
        out = m.comando("noexiste_xyz; echo \"st=$?\"")
        verificar("comando no encontrado" in out and tem_linha(out, "st=127"), "comando inexistente: $? = 127")

        # Controle de fluxo.
        out = m.comando('for i in 1 2 3; do echo "n$i"; done')
        verificar(tem_linha(out, "n1") and tem_linha(out, "n3"), "for ... in ... do ... done")
        out = m.comando('i=0; while [ $i -lt 5 ]; do i=$((i+1)); [ $i = 2 ] && continue; [ $i = 4 ] && break; echo "w$i"; done; echo "i=$i"')
        verificar(tem_linha(out, "w1") and tem_linha(out, "w3") and not tem_linha(out, "w2") and tem_linha(out, "i=4"),
                  "while com [ ], $((...)), continue e break")
        out = m.comando('if [ -d /bin ]; then echo e_pasta; elif true; then echo nao; else echo nunca; fi')
        verificar(tem_linha(out, "e_pasta") and not tem_linha(out, "nunca"), "if/elif/else/fi com test -d")
        out = m.comando('for f in hola.c x.txt y; do case $f in *.c) echo "$f C";; *.txt|*.md) echo "$f TXT";; *) echo "$f outro";; esac; done')
        verificar(tem_linha(out, "hola.c C") and tem_linha(out, "x.txt TXT") and tem_linha(out, "y outro"),
                  "case com padrões glob e |")
        # Bloco em várias linhas: o shell pede o resto com o PS2 "> ".
        out = m.comando("if true\nthen\necho bloco_ok\nfi")
        verificar(tem_linha(out, "bloco_ok") and "> then" in out, "if em várias linhas: continuação com PS2 \"> \"")

        # Funções, local, return, recursão.
        out = m.comando('doble() { local r=$(( $1 * 2 )); echo "doble=$r"; return 3; }; doble 21; echo "ret=$? r=[$r]"')
        verificar(tem_linha(out, "doble=42") and tem_linha(out, "ret=3 r=[]"), "função com local e return")
        out = m.comando('fat() { if [ $1 -le 1 ]; then echo 1; else echo $(( $1 * $(fat $(($1-1))) )); fi; }; fat 6')
        verificar(tem_linha(out, "720"), "função recursiva com $(...) aninhado: fat 6 = 720")

        # Expansões.
        out = m.comando('s=arquivo.tar.gz; echo "${s%.gz} ${s##*.} ${#s} ${nada:-padrao} $((7*6)) $((17 % 5))"')
        verificar(tem_linha(out, "arquivo.tar gz 14 padrao 42 2"), "${x%..}, ${x##..}, ${#x}, ${x:-..}, aritmética")
        out = m.comando('n=$(echo abc | conta); echo "[$n]"')
        verificar(re.search(r"^\[1 líneas?, 1 palabras?", out, re.M), "$(...) captura a saída de um pipe")
        out = m.comando("echo /ejemplos/*.sh; echo /nada*")
        verificar(tem_linha(out, "/ejemplos/adivina.sh /ejemplos/funciones.sh /ejemplos/hola.sh") and tem_linha(out, "/nada*"),
                  "glob * (ordenado); sem coincidência fica o padrão")
        out = m.comando("echo /bin/s?gsh /bin/[ef]nv")
        verificar(tem_linha(out, "/bin/sagsh /bin/env"), "glob ? e [..]")

        # Redirecionamentos.
        out = m.comando("cat /noexiste 2>&1 | conta")
        verificar(re.search(r"^1 líneas?", out, re.M), "2>&1 manda os erros para o pipe")
        out = m.comando("cat /noexiste 2> /tmp/e.txt; echo \"st=$?\"; cat /tmp/e.txt")
        verificar(tem_linha(out, "st=1") and "no existe" in out, "2> arquivo")
        out = m.comando('echo para_erros >&2 | conta')
        verificar("para_erros" in out and re.search(r"^0 líneas?", out, re.M), ">&2 não passa pelo pipe")
        out = m.comando("cat <<FIN\nhola $x\nlinea dos\nFIN")
        verificar(tem_linha(out, "hola 42") and tem_linha(out, "linea dos") and "> FIN" in out, "here-doc <<FIN expande $x")
        out = m.comando("cat <<'FIN'\nsin $x\nFIN")
        verificar(tem_linha(out, "sin $x"), "here-doc com delimitador entre aspas não expande")

        # Pipes com etapas internas.
        out = m.comando('for i in a b c; do echo $i; done | conta')
        verificar(re.search(r"^3 líneas?", out, re.M), "laço interno numa etapa de pipe")
        out = m.comando('echo "uno dos tres" | while read a b; do echo "a=$a b=$b"; done')
        verificar(tem_linha(out, "a=uno b=dos tres"), "read num laço while no fim do pipe")
        out = m.comando('( cd /tmp; x=dentro; echo "sub=$x" ); pwd; echo "fora=$x"')
        verificar(tem_linha(out, "sub=dentro") and tem_linha(out, "/") and tem_linha(out, "fora=42"),
                  "( subshell ) não muda a pasta nem as variáveis de fora")

        # Ambiente herdado pelos programas (envp no SIS_EXECUTAR).
        out = m.comando('export SAGUI_COR=verde; NAO_EXP=1; env SAGUI_COR; env NAO_EXP; echo "rc=$?"')
        verificar(tem_linha(out, "verde") and tem_linha(out, "rc=1"), "export chega ao programa filho; variável sem export não")
        out = m.comando("SO_ESTE=temporal env SO_ESTE; env | conta")
        verificar(tem_linha(out, "temporal") and re.search(r"^\d+ líneas?", out, re.M), "VAR=x comando: só para aquele comando")
        out = m.comando("env PATH; env HOME; env PWD")
        # D194: el PATH por defecto es /bin:/apps/bin (las apps de SAGUI-APPS).
        verificar(tem_linha(out, "/bin:/apps/bin") and tem_linha(out, "/"), "PATH=/bin:/apps/bin e HOME=/ iniciais")

        # Scripts.
        m.comando("echo '#!/bin/sh' > /tmp/s.sh")
        m.comando("echo 'echo \"script:$0:$1:$#\"; exit 5' >> /tmp/s.sh")
        out = m.comando('/tmp/s.sh uno dos; echo "st=$?"')
        verificar(tem_linha(out, "script:/tmp/s.sh:uno:2") and tem_linha(out, "st=5"), "script com #!/bin/sh, argumentos e exit")
        out = m.comando("/ejemplos/hola.sh Tito")
        verificar("Hola, Tito!" in out and "argumento 1: Tito" in out, "/ejemplos/hola.sh (UTF-8 convertido)")
        out = m.comando("sagsh /ejemplos/funciones.sh")
        verificar(tem_linha(out, "6! = 720") and "funciones.sh: script de sagsh" in out, "sagsh script.sh (funções, while, case)")
        out = m.comando("sh -c 'echo \"desde sh $1\"' nome arg1")
        verificar(tem_linha(out, "desde sh arg1"), "/bin/sh -c")

        # read do terminal.
        n = m.serial().count("sagui:")
        m.digitar("read -p 'Nombre: ' nome\n", 0.04)
        time.sleep(1.5)
        m.digitar("Tito Sagui\n", 0.04)
        m.esperar_serial("Nombre: ", 10)
        fim = time.time() + 20
        while m.serial().count("sagui:") <= n and time.time() < fim:
            time.sleep(0.2)
        out = m.comando('echo "Hola, $nome"')
        verificar(tem_linha(out, "Hola, Tito Sagui"), "read lê uma linha do terminal")

        # Trabalhos.
        # (numa linha só: um prompt redesenhado no meio confundiria m.comando)
        out = m.comando('contador 2 & jobs; wait; echo "esperado $?"')
        verificar(re.search(r"^\[1\] \d+$", out, re.M), "& mostra [1] pid")
        verificar(re.search(r"^\[1\]\+ Ejecutando\s+contador 2 &", out, re.M), "jobs lista o trabalho")
        verificar(tem_linha(out, "esperado 0") and "2 de 2 segundos" in out, "wait espera o trabalho")

        # Erros claros e robustez.
        out = m.comando("if true; then echo x; fi fi")
        verificar("error de sintaxis cerca de 'fi'" in out, "erro de sintaxe cerca de 'fi'")
        out = m.comando("echo $((1/0)); echo \"st=$?\"")
        verificar("división por cero" in out and tem_linha(out, "st=1"), "divisão por zero avisada")
        out = m.comando("echo \"aberto\nfechado\"")
        verificar(tem_linha(out, "aberto") and tem_linha(out, "fechado"), "aspas abertas continuam na linha seguinte")
        out = m.comando('printf "%03d|%-4s|%x\\n" 7 ab 255; type cd ls')
        verificar(tem_linha(out, "007|ab  |ff") and "cd es un comando interno" in out and "ls es /bin/ls" in out,
                  "printf e type")
        out = m.comando("PS1='sagui:\\w> '")
        out = m.comando("cd /historia")
        verificar("sagui:/historia> " in m.serial(), "PS1 configurável (\\w = pasta)")
        m.comando("cd /; PS1='sagui:\\w$ '")
        m.captura("H38-shell-completo")
        out = m.comando("ajuda sh")
        verificar("El lenguaje de sagsh" in out, "ajuda sh resume o lenguaje")
        out = m.comando("echo o shell segue vivo")
        verificar(tem_linha(out, "o shell segue vivo"), "o shell continua funcionando")


def ctrl_c(m, comando, espera_serial):
    n = m.serial().count("sagui:")
    m.digitar(comando + "\n", 0.04)
    if espera_serial:
        m.esperar_serial(espera_serial, 15)
    time.sleep(0.8)
    m.tecla("ctrl-c")
    fim = time.time() + 20
    while m.serial().count("sagui:") <= n and time.time() < fim:
        time.sleep(0.2)
    return m.serial().count("sagui:") > n


def janela():
    with Maquina("h38-tv", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        voltou = ctrl_c(m, "contador 60", "executar /bin/contador")
        out = m.comando('echo "st=$?"')
        verificar(voltou and tem_linha(out, "st=130"), "Ctrl+C num programa: $? = 130")

        antes = m.serial()
        voltou = ctrl_c(m, "contador 60 | conta", "executar /bin/wc")
        depois = m.serial()[len(antes):]
        out = m.comando('echo "st=$?"')
        verificar(voltou and "(contador) terminou com codigo 130" in depois and "(wc) terminou com codigo 130" in depois
                  and tem_linha(out, "st=130"), "Ctrl+C encerra todas as etapas do pipe (grupo de processos)")

        voltou = ctrl_c(m, "while true; do :; done", None)
        out = m.comando('echo "st=$?"')
        verificar(voltou and tem_linha(out, "st=130"), "Ctrl+C interrompe um laço interno do shell")

        m.comando("echo 'contador 60; echo nao_devia' > /tmp/c.sh")
        antes = m.serial()
        voltou = ctrl_c(m, "sagsh /tmp/c.sh", "executar /bin/contador")
        depois = m.serial()[len(antes):]
        verificar(voltou and "(sagsh) terminou com codigo 130" in depois and "nao_devia" not in depois.replace("echo nao_devia", ""),
                  "Ctrl+C encerra o script e o programa que ele roda")

        voltou = ctrl_c(m, "read algo", None)
        out = m.comando('echo "st=$?"; echo vivo')
        verificar(voltou and tem_linha(out, "vivo"), "Ctrl+C durante um read interno volta ao prompt")
        m.captura("H38-ctrl-c-terminal")


def main():
    console()
    janela()
    return 0


if __name__ == "__main__":
    sys.exit(main())
