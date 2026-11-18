#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H16 - shell e comandos como programas.

Critério: roteiro executa todos os comandos e apelidos, incluindo
`echo ola > tmp/a.txt`, `cat tmp/a.txt`, `cp tmp/a.txt tmp/b.txt`,
`ls bin | conta` e `contador` rodando junto com outro comando; `falha`
derruba só o próprio processo e o shell continua; `ps` mostra os processos;
`ajuda` lista tudo. Captura de tela de cada caso em docs/capturas/.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import NOME_VERSAO, Maquina, verificar, RAIZ, BUILD, imagem_console  # noqa: E402

# La lista real de /bin, tal como entra en la imagen (build/rootfs/bin): los programas de
# userland/programas, los de carpeta propia, python y python-x87 (si hay MicroPython) y los
# scripts como /bin/lmk (lite-monkey, D117). Así un programa nuevo no obliga a tocar la prueba.
PROGRAMAS = sorted(f for f in os.listdir(os.path.join(BUILD, "rootfs", "bin")) if not f.startswith("."))   # sin .gitkeep (ls no lo muestra)


def main():
    with Maquina("h16", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("[SAGUI] shell iniciado: processo", 120)
        m.esperar_serial("sagui:/$", 30)
        s = m.serial()
        # Antes del shell solo hay tareas del kernel (rede D046, apagar D113,
        # acpi-aml D192, usb D227, cdrom D233...) y el «configuracion --aplicar»
        # del arranque (D112): su número cambia con cada versión, así que no se
        # cuentan. Lo que importa es que el shell es el primer programa que el
        # kernel arranca después de la configuración.
        sh = int(re.search(r"\[SAGUI\] shell iniciado: processo (\d+)", s).group(1))
        progs = [p for p in re.findall(r"executar (/bin/\S+) -> processo \d+", s) if p != "/bin/configuracion"]
        verificar(f"executar /bin/sagsh -> processo {sh}" in s and progs[:1] == ["/bin/sagsh"],
                  f"o kernel inicia /bin/sagsh como primeiro programa (PID {sh})")
        verificar("Terminal provis" not in s, "o terminal provisório do kernel foi removido")

        # D385: «ayuda» (antes «ajuda») lista las órdenes por secciones; los alias en español, «ayuda alias»;
        # los nombres en portugués de antes siguen funcionando (ajuda, conta, limpar, apagar...).
        out = m.comando("ayuda")
        for c in ("ls", "cd", "pwd", "cat", "echo", "cp", "mv", "rm", "mkdir", "rmdir", "wc", "ps", "kill", "clear",
                  "ayuda", "mem", "data", "sobre", "ola", "contador", "falha", "reiniciar", "salir"):
            verificar(re.search(r"(^|\s)" + re.escape(c) + r"\s", out, re.M), f"ayuda lista {c}")
        verificar("Archivos y carpetas" in out and "El shell" in out, "ayuda: por secciones")
        m.captura("H16-ajuda")
        out = m.comando("ayuda alias")
        for a, c in (("listar", "ls"), ("entrar", "cd"), ("donde", "pwd"), ("ver", "cat"), ("eco", "echo"), ("copiar", "cp"),
                     ("mover", "mv"), ("borrar", "rm"), ("crear-carpeta", "mkdir"), ("borrar-carpeta", "rmdir"), ("contar", "wc"),
                     ("procesos", "ps"), ("matar", "kill"), ("limpiar", "clear")):
            verificar(f"{a} = {c}" in out, f"ayuda alias: {a} = {c}")
        out = m.comando("ajuda conta")
        verificar("Uso: wc" in out and "Otros nombres" in out and "conta (el de antes)" in out,
                  "ajuda (el nombre de antes) <comando> explica el comando y sus otros nombres")
        out = m.comando("apagar; echo st=$?")
        verificar("apagar-pc" in out and "st=1" in out, "«apagar» solo (el alias de rm de antes) explica y no hace nada")

        m.comando("limpar")
        out = m.comando("ls")
        verificar("bin/" in out and "leia-me.txt" in out, "ls")
        out = m.comando("listar -s /bin")   # D385: -s, el formato de antes (con «programa»)
        verificar("sagsh" in out and "programa" in out, "listar (apelido de ls) /bin")
        out = m.comando("ls bin | conta")
        verificar(f"{len(PROGRAMAS)} líneas" in out, f"ls bin | conta = {len(PROGRAMAS)} linhas (um por programa)")
        out = m.comando("listar bin|wc")
        verificar(f"{len(PROGRAMAS)} líneas" in out, "pipe colado nas palavras (listar bin|wc)")

        m.comando("echo ola > tmp/a.txt")
        out = m.comando("cat tmp/a.txt")
        verificar(re.search(r"^ola$", out, re.M), "echo ola > tmp/a.txt; cat tmp/a.txt")
        m.comando("cp tmp/a.txt tmp/b.txt")
        out = m.comando("ver tmp/b.txt")
        verificar(re.search(r"^ola$", out, re.M), "cp tmp/a.txt tmp/b.txt; ver (apelido de cat) tmp/b.txt")
        m.comando("eco \"mais uma linha\" >> tmp/a.txt")
        out = m.comando("conta < tmp/a.txt")
        verificar("2 líneas, 4 palabras" in out, "eco com aspas e >>; conta < arquivo (redirecionar a entrada)")
        m.comando("copiar tmp/a.txt /tmp/c.txt")
        m.comando("mv tmp/b.txt tmp/d.txt")
        m.comando("mover tmp/c.txt tmp/e.txt")
        out = m.comando("ls tmp")
        verificar("a.txt" in out and "d.txt" in out and "e.txt" in out and "b.txt" not in out and "c.txt" not in out,
                  "copiar, mv e mover")
        m.comando("rm tmp/d.txt")
        m.comando("apagar tmp/e.txt")
        m.comando("mkdir tmp/pasta1")
        m.comando("criar-pasta tmp/pasta2")
        out = m.comando("ls tmp")
        verificar("pasta1/" in out and "pasta2/" in out and "d.txt" not in out and "e.txt" not in out,
                  "rm, apagar, mkdir, criar-pasta")
        m.comando("rmdir tmp/pasta1")
        m.comando("remover-pasta tmp/pasta2")
        out = m.comando("ls tmp")
        verificar("pasta" not in out, "rmdir, remover-pasta")
        m.captura("H16-arquivos-redirecionamento-pipe")

        m.comando("cd historia")
        out = m.comando("pwd")
        verificar("/historia" in out, "cd historia; pwd")
        m.comando("entrar ..")
        out = m.comando("onde")
        verificar(re.search(r"^/$", out, re.M), "entrar .. (apelido de cd); onde (apelido de pwd)")
        out = m.comando("cd /nao-existe")
        verificar("no existe" in out, "cd para pasta inexistente avisa")
        out = m.comando("xyz")
        verificar("comando no encontrado" in out, "comando desconhecido avisa")

        # Multitarefa: contador em segundo plano junto com outros comandos.
        out = m.comando("contador 4 &")
        pid = int(re.search(r"^\[\d+\] (\d+)$", out, re.M).group(1))   # "[1] 7", como no bash
        out = m.comando("ps")
        verificar(re.search(rf"^\s*{pid}\s+{sh}\s+\w+.*contador", out, re.M), f"ps mostra o contador (PID {pid}) rodando junto")
        verificar("sagsh" in out and "ocioso" in out and "ps" in out, "ps mostra sagsh, ocioso e o próprio ps")
        m.captura("H16-contador-e-ps")
        out = m.comando("processos")
        verificar("contador" in out, "processos (apelido de ps)")
        m.esperar_serial(f"[contador {pid}] 4 de 4 segundos", 30)
        time.sleep(1)   # deixa o shell redesenhar o prompt (D018) antes de digitar
        out = m.comando("ola de novo")
        verificar(re.search(r"^\[\d+\]\+ Hecho\s+contador 4$", out, re.M), "o shell avisa quando o contador termina ([1]+ Hecho)")
        verificar('argv[1] = "de"' in out, "ola recebe os argumentos")

        out = m.comando("contador 60 &")
        pid = int(re.search(r"^\[\d+\] (\d+)$", out, re.M).group(1))
        out = m.comando(f"kill {pid}")
        verificar(f"Proceso {pid} terminado" in out, "kill encerra o contador")
        m.comando("contador 60 &")
        out = m.comando("ps")
        pid2 = int(re.findall(rf"^\s*(\d+)\s+{sh}\s+\w+.*contador", out, re.M)[0])
        out = m.comando(f"matar {pid2}")
        verificar(f"Proceso {pid2} terminado" in out, "matar (apelido de kill)")
        out = m.comando("ps")
        verificar("contador" not in out, "nenhum contador sobrou")

        # Proteção: falha derruba só o próprio processo.
        for tipo, esperado in (("", "Fallo de p"), ("kernel", "Fallo de p"), ("instrucao", "Prote"), ("divisao", "Divis")):
            out = m.comando(f"falha {tipo}".strip())
            verificar("finalizado" in out and esperado in out, f"falha {tipo or '(ponteiro nulo)'}: só o processo foi encerrado")
        out = m.comando("echo o shell continua vivo")
        verificar("o shell continua vivo" in out, "o shell continua funcionando depois das falhas")
        m.captura("H16-falha-protecao")

        out = m.comando("mem")
        verificar("Libre:" in out and "MB" in out, "mem")
        out = m.comando("data")
        verificar(re.search(r"\d\d/\d\d/20\d\d \d\d:\d\d:\d\d", out), "data")
        # Layout do teclado (D031): ES por padrão, troca para ABNT2 e volta.
        # As letras e o espaço ficam no mesmo lugar nos dois layouts.
        out = m.comando("teclado")
        verificar("Distribución del teclado: es" in out, "teclado: ES é o padrão")
        out = m.comando("teclado abnt2")
        verificar("Distribución del teclado: abnt2" in out, "teclado abnt2 troca o layout")
        out = m.comando("teclado es")
        verificar("Distribución del teclado: es" in out, "teclado es volta ao ES")
        out = m.comando("teclado xx")
        verificar("uso: teclado" in out, "teclado com layout desconhecido mostra o uso")

        out = m.comando("sobre")
        verificar(NOME_VERSAO in out and "procesos" in out, "sobre")

        # Histórico (seta para cima) e completar com Tab.
        out = m.comando("", teclas_finais=("up", "ret"))
        verificar(NOME_VERSAO in out, "seta para cima repete o último comando (sobre)")
        out = m.comando("cat /his", teclas_finais=("tab", "l", "i", "tab", "ret"))
        verificar("cat /historia/linux.txt" in out and "Minix" in out, "Tab completa /his -> /historia/ e li -> linux.txt")
        out = m.comando("sob", teclas_finais=("tab", "ret"))
        verificar("executar /bin/sobre" in out and NOME_VERSAO in out, "Tab completa nomes de comandos (sob -> sobre)")
        out = m.comando("conta", teclas_finais=("d", "tab", "1", "ret"))
        verificar("1 de 1 segundos" in out, "Tab completa contad -> contador (\"con\" é ambíguo com conta)")
        m.comando("limpar")
        m.comando("cat /historia/saguios.txt | conta")
        m.captura("H16-tab-historico")

        # sair: o kernel abre um shell novo.
        m.digitar("sair\n", 0.04)
        m.esperar_serial("Se ha cerrado el shell", 20)
        m.esperar_serial("shell iniciado: processo", 20)
        time.sleep(1)
        out = m.comando("pwd")
        verificar(re.search(r"^/$", out, re.M), "sair fecha o shell e o sistema abre outro")

        m.digitar("reiniciar\n", 0.04)
        m.esperar_serial("[SAGUI] reiniciando", 20)
        try:
            m.proc.wait(timeout=30)
            reiniciou = True
        except Exception:
            reiniciou = False
        verificar(reiniciou, "reiniciar reinicia a máquina")
    return 0


if __name__ == "__main__":
    sys.exit(main())
