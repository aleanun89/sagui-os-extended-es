#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H403 - procesos POSIX en el núcleo (D401): fork con copia al escribir, execve, waitpid, señales,
poll/select y descriptores no bloqueantes.

Criterio (/testes/posix, en C con la libc común):
  1. fork + execve + waitpid: el programa nuevo conserva pid, padre y descriptores (menos el
     FD_CLOEXEC); execve de algo que no existe vuelve con ENOENT; WNOHANG, wait, system y popen.
  2. Copia al escribir: fork de un proceso con 8 MB no los duplica; al escribir el hijo, sí; cada
     uno ve lo suyo; Tareas reparte lo compartido.
  3. 200 fork seguidos sin perder memoria.
  4. fork con hilos (al hijo solo el que llama; también desde un hilo) y un hilo que suma sin perder
     ninguna escritura mientras se hace fork una y otra vez (TLB de las demás CPU).
  5. Tuberías padre-hijo (100 KB).
  6. Señales: tratador con siginfo, máscara y pendientes, SIGCHLD, SIGSEGV/SIGFPE/SIGILL atrapadas con
     siglongjmp, muerte por señal en waitpid, kill, SIGPIPE, alarm/pause, EINTR y SA_RESTART,
     sigsuspend, a qué hilo va, la FPU (xmm0/ymm0 y x87) intacta tras el tratador, Ctrl+C (SIGINT)
     por un terminal virtual.
  7. poll/select con plazo y con datos; POLLHUP, POLLERR, POLLNVAL; O_NONBLOCK; sockets con connect y
     accept no bloqueantes.
  8. Revisión aparte (D401, «revision»): lo que intentaría un programa malicioso (sigreturn con
     selectores del kernel, IOPL 3, NT, VM y una FPU con basura; fpregs en el kernel o desalineado;
     ESP en el kernel o dando la vuelta al entregar; tratador en el kernel; SA_NODEFER sin fin), el
     tratador de un SIGFPE de la x87 con la FPU limpia, sigaltstack, el kernel escribiendo en las
     páginas COW del hijo, SIGCHLD ignorada con más fork que la tabla, nietos huérfanos e hijos con
     hilos matados sin perder memoria, getpgrp, exec desde un proceso con hilos (pilas distintas) y
     join que no da EINTR.
Variantes (H403_SO=normal,smp,pae6g): normal (1 CPU), smp (--smp, 4 CPU), pae6g (memoria por encima de
4 GB). Con SAGUI_KVM=1, con KVM.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
TEMPO = 240 if KVM else 900


def rodar(et, extras=(), memoria=None, nucleos=1):
    with Maquina(f"h403-{et}", "ide", imagem=imagem_de_teste("/testes/posix", list(extras)), memoria=memoria,
                 nucleos=nucleos) as m:
        m.esperar_serial("RESULTADO posix:", TEMPO)
        s = m.serial().replace("\r", "")
    for l in s.split("\n"):
        if l.startswith("POSIX") or "[FALHA]" in l or "EXCECAO" in l or "[COW]" in l:
            print("    %s" % l.strip())
    verificar("RESULTADO posix: SUCESSO (0 falhas)" in s, f"[{et}] /testes/posix pasó todas sus pruebas")
    verificar("EXCECAO" not in s, f"[{et}] sin excepciones del kernel")
    verificar("liberando pagina fisica invalida" not in s, f"[{et}] ninguna página liberada dos veces")
    verificar("[COW] ERRO" not in s, f"[{et}] la tabla de referencias sin errores")
    verificar("[SWAP] ERRO" not in s, f"[{et}] la swap sin trozos corruptos")
    return s


def normal():
    rodar("normal")


def smp():
    s = rodar("smp", ["--smp"], nucleos=4)
    verificar(re.search(r"\[SMP\].*(4|cuatro)", s) is not None or "smp" in s.lower(), "[smp] con varias CPU")


def pae6g():
    rodar("pae6g", memoria="6G")


def main():
    so = os.environ.get("H403_SO", "")
    for nome, f in (("normal", normal), ("smp", smp), ("pae6g", pae6g)):
        if so and nome not in so.split(","):
            continue
        f()
    return 0


if __name__ == "__main__":
    sys.exit(main())
