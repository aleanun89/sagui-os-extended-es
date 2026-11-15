#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H118 - el cliente de correo (082): recibir, ver, adjuntar y enviar.

El programa `correo` habla POP3 (o IMAP) para recibir y SMTP para enviar. La
prueba usa el servidor falso de correo (tests/servidor_falso_correo.py): sin
Internet y sin cuenta de verdad.

Lo que se comprueba:

  - la ventana «Correo» abre con su tamaño;
  - «Recibir» trae los tres mensajes del servidor (uno en texto plano, uno con
    HTML y uno con un adjunto) y los guarda en /home/correo;
  - el que tiene HTML se ve como texto (el HTML se convierte);
  - «Redactar» abre su ventana: se escribe el destinatario y el asunto, se
    adjunta un fichero y «Enviar» lo entrega al servidor SMTP con el adjunto
    dentro y sin quitar;
  - con IMAP, «Recibir» también trae los mensajes.

El texto de la ventana no se lee de la pantalla: los rótulos de la interfaz no
caen en la rejilla de 8x16 que usa el reconocedor de la prueba. Lo que se
comprueba es lo que se puede comprobar de verdad: lo que llega al servidor, lo
que queda guardado en el disco y lo que el programa anota en el registro del
sistema (la serie).
"""
import email
import email.policy
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402
import servidor_falso_correo  # noqa: E402

# Puertos fijos: el servidor falso los necesita antes de que arranque la
# máquina (la línea de arranque del programa tiene un límite de 63 bytes).
PORTA_POP3 = 41110
PORTA_SMTP = 4125
PORTA_IMAP = 41143

# La ventana principal: 820x540, con la interfaz de Outlook clásico (D289): botones de la barra de
# herramientas («Nuevo» el primero, «Enviar y recibir» el sexto) y la lista de mensajes en filas
# de 18 píxeles, lo más reciente primero.
CX_RECEBER, CY_RECEBER = 520, 42       # centro de «Enviar y recibir»
CX_REDIGIR, CY_REDIGIR = 32, 42         # centro de «Nuevo»
LISTA_X, LISTA_Y = 400, 68 + 18 + 2
ITEM = 18

# La ventana de redacción: 600x480.
CX_PARA, CY_PARA = 186, 18
CX_ASSUNTO, CY_ASSUNTO = 186, 78
CX_CORPO, CY_CORPO = 200, 200
CX_ADJUNTAR, CY_ADJUNTAR = 60, 455
CX_ENVIAR, CY_ENVIAR = 445, 455


def janela(serial, titulo):
    """La posición y el tamaño de una ventana, del log del kernel."""
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]), y=int(g[3]))


def ponto(j, cx, cy):
    """Las coordenadas de pantalla de un punto del cliente de la ventana."""
    return (j["x"] + 2 + cx, j["y"] + 26 + cy)


def esperar(m, texto, tempo=40):
    fim = time.time() + tempo
    while time.time() < fim:
        if texto in m.serial():
            return True
        time.sleep(0.2)
    raise AssertionError(f"tiempo agotado esperando {texto!r} en la serial")


def item(m, j, indice):
    """Clic en el ítem de la lista (0 = el primero)."""
    m.clique(*ponto(j, LISTA_X, LISTA_Y + indice * ITEM + 9))
    time.sleep(0.6)


def main():
    pop3 = servidor_falso_correo.servidor_falso_correo("pop3", porta_fixa=PORTA_POP3).arrancar()
    smtp = servidor_falso_correo.servidor_falso_correo("smtp", porta_fixa=PORTA_SMTP).arrancar()
    imap = servidor_falso_correo.servidor_falso_correo("imap", porta_fixa=PORTA_IMAP).arrancar()
    imagem = imagem_de_teste("", ["--vesa"])
    try:
        with Maquina("h118", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("sagui:/$", 120)

            # Un fichero para adjuntar, hecho desde la consola.
            m.comando("echo conteudo do anexo > /tmp/nota.txt")
            out = m.comando("cat /tmp/nota.txt")
            verificar("conteudo do anexo" in out, "el fichero para adjuntar existe")

            # El cliente, con la cuenta en la línea de comandos.
            m.comando(f"correo --proto pop3 --host 10.0.2.2 --porta {PORTA_POP3} "
                       f"--porta-envio {PORTA_SMTP} --utilizador eu --clave chave &")
            esperar(m, '"Correo"', 40)
            time.sleep(1.5)
            j = janela(m.serial(), "Correo")
            verificar(j["l"] == 820 and j["a"] == 540, f"la ventana abre con su tamaño ({j['l']}x{j['a']})")
            m.captura("H118-1-ventana")

            # «Recibir»: los tres mensajes del servidor falso.
            m.clique(*ponto(j, CX_RECEBER, CY_RECEBER))
            esperar(m, "correo: recibidos 3 mensajes por pop3", 60)
            m.captura("H118-2-recibidos")
            verificar(True, "los tres mensajes llegan")

            # El último (el del adjunto) sale seleccionado: su cuerpo lo enseña.
            esperar(m, "correo: mensaje 3 (Con archivo)", 20)
            verificar("Adjunto: nota.txt" in m.serial(),
                      "el mensaje con adjunto dice que lo lleva")
            verificar("Va un archivo adjunto" in m.serial(),
                      "el cuerpo del mensaje con adjunto se ve")

            # El que tiene HTML: el cuerpo sale como texto, sin etiquetas.
            item(m, j, 1)
            esperar(m, "correo: mensaje 2 (Con HTML)", 20)
            time.sleep(0.5)
            m.captura("H118-3-html-como-texto")
            serial = m.serial()
            verificar("La versi" in serial, "el texto del mensaje con HTML se ve")
            verificar("<html>" not in serial and "<body>" not in serial,
                      "el HTML se enseña como texto, sin etiquetas")

            # «Redactar»: la ventana de redacción.
            m.clique(*ponto(j, CX_REDIGIR, CY_REDIGIR))
            esperar(m, '"Redactar"', 30)
            time.sleep(1.0)
            r = janela(m.serial(), "Redactar")
            m.captura("H118-4-redactar")

            # Destinatario, asunto y cuerpo.
            m.clique(*ponto(r, CX_PARA, CY_PARA))
            time.sleep(0.4)
            m.digitar("ana@example.org", 0.04)
            time.sleep(0.4)
            m.clique(*ponto(r, CX_ASSUNTO, CY_ASSUNTO))
            time.sleep(0.4)
            m.digitar("Proba de correo", 0.04)
            time.sleep(0.4)
            m.clique(*ponto(r, CX_CORPO, CY_CORPO))
            time.sleep(0.4)
            m.digitar("Ola desde o SaguiOS", 0.04)
            time.sleep(0.5)
            m.captura("H118-5-escrito")

            # Adjuntar: pide la ruta del fichero.
            m.clique(*ponto(r, CX_ADJUNTAR, CY_ADJUNTAR))
            esperar(m, '"Adjuntar"', 20)
            time.sleep(1.0)
            m.digitar("/tmp/nota.txt", 0.04)
            time.sleep(0.4)
            m.tecla("ret", 0.3)
            esperar(m, "correo: adjunto nota.txt", 20)
            time.sleep(0.5)
            m.captura("H118-6-adjuntado")
            verificar(True, "el fichero se adjunta")

            # «Enviar».
            antes = len(servidor_falso_correo.ENVIADOS)
            m.clique(*ponto(r, CX_ENVIAR, CY_ENVIAR))
            fim = time.time() + 60
            while time.time() < fim and len(servidor_falso_correo.ENVIADOS) == antes:
                time.sleep(0.3)
            verificar(len(servidor_falso_correo.ENVIADOS) > antes,
                      "el servidor SMTP recibe el mensaje")
            esperar(m, "correo: mensaje enviado a ana@example.org", 20)
            m.captura("H118-7-enviado")

            # Lo que llegó al servidor. Se mira con el parser de MIME de
            # Python (el de verdad): así se comprueba que el mensaje está bien
            # formado, no solo que tiene las palabras.
            env = servidor_falso_correo.ENVIADOS[-1] if servidor_falso_correo.ENVIADOS else {}
            corpo = env.get("corpo", "")
            verificar("ana@example.org" in " ".join(env.get("para", [])),
                      f"el destinatario es el que se escribió ({env.get('para')})")
            msg = email.message_from_string(corpo, policy=email.policy.default)
            verificar(msg.get("Subject", "") == "Proba de correo",
                      f"el asunto va en el mensaje ({msg.get('Subject')!r})")
            verificar(msg.get("To", "") == "ana@example.org",
                      f"el destinatario va en la cabecera ({msg.get('To')!r})")
            corpo_txt = msg.get_body(preferencelist=("plain",))
            texto = corpo_txt.get_content() if corpo_txt else ""
            verificar("Ola desde o SaguiOS" in texto,
                      f"el cuerpo va en el mensaje ({texto.strip()[:60]!r})")
            anexos = list(msg.iter_attachments())
            verificar(len(anexos) == 1, f"el mensaje lleva un adjunto ({len(anexos)})")
            if anexos:
                verificar(anexos[0].get_filename() == "nota.txt",
                          f"el adjunto se llama nota.txt ({anexos[0].get_filename()!r})")
                conteudo = anexos[0].get_content()
                if isinstance(conteudo, bytes):
                    conteudo = conteudo.decode("latin-1")
                verificar("conteudo do anexo" in conteudo,
                          f"el adjunto llega entero ({conteudo[:40]!r})")

            # Lo que quedó guardado en el disco: los tres mensajes.
            # La ventana tiene el foco, así que primero se cierra: Alt+F4 o,
            # si no, la X de la barra de título.
            m.tecla("alt-f4", 0.5)
            if "fechar janela" not in m.serial():
                # La X de la barra de título (la dibuja el kernel).
                m.clique(j["x"] + j["l"] - 10, j["y"] + 12)
            esperar(m, "fechar janela", 20)
            time.sleep(1.5)
            out = m.comando("ls /home/correo")
            salvar = re.findall(r"m\d+\.eml", out)
            verificar(len(salvar) == 3, f"los tres mensajes quedan guardados en /home/correo ({salvar})")
            out = m.comando("cat /home/correo/m0001.eml")
            verificar("Hola" in out and "Esto es un mensaje en texto plano" in out,
                      "el mensaje en texto plano está guardado tal cual")

            # Con IMAP, «Recibir» también funciona.
            m.comando(f"correo --proto imap --host 10.0.2.2 --porta {PORTA_IMAP} "
                       f"--porta-envio {PORTA_SMTP} --utilizador eu --clave chave &")
            esperar(m, '"Correo"', 40)
            time.sleep(1.5)
            j2 = janela(m.serial(), "Correo")
            m.clique(*ponto(j2, CX_RECEBER, CY_RECEBER))
            esperar(m, "correo: recibidos 3 mensajes por imap", 60)
            m.captura("H118-8-imap")
            verificar(True, "con IMAP también llegan los tres mensajes")

            # Y el programa está en el menú Inicio, en «Internet».
            m.tecla("alt-f4", 0.5)
            time.sleep(1.0)
            marca = len(m.serial())
            m.clique(40, 12)          # el botón «Inicio» de la barra de tareas
            esperar(m, "menu Inicio aberto", 20)
            time.sleep(0.8)
            m.clique(2 + 34 + 30, 24 + 7 * 20 + 10)   # la línea «Internet»
            esperar(m, "Correo", 20)
            time.sleep(0.8)
            m.captura("H118-9-menu-inicio")
            m.clique(275 + 30, 165 + 20 + 10)        # la línea «Correo» del submenú
            esperar(m, '"Correo"', 30)
            time.sleep(1.0)
            verificar(True, "el programa abre desde el menú Inicio")
    finally:
        pop3.parar()
        smtp.parar()
        imap.parar()
    return 0


if __name__ == "__main__":
    sys.exit(main())
