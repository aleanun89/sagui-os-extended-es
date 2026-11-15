#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H289 - El cliente de correo con interfaz de Outlook clásico (mejora 113, D289).

Tres paneles (árbol de carpetas, lista de mensajes en columnas, vista previa),
barra de herramientas, menú y barra de estado. Se usa el servidor falso de
correo (tests/servidor_falso_correo.py) por IMAP: la Bandeja de entrada trae
tres mensajes y la carpeta «Trabajo» uno.

Lo que se comprueba (las capturas H289-*.png enseñan la ventana):

  - «Enviar y recibir» (barra de herramientas) llena la bandeja y la carpeta
    IMAP «Trabajo»; los no leídos salen contados;
  - elegir un mensaje lo enseña en la vista previa y lo da por leído; el que
    trae HTML se ve como texto;
  - ordenar por columna (clic en la cabecera);
  - cambiar de carpeta en el árbol;
  - Responder (Ctrl+R): Para y Asunto vienen rellenos y el cuerpo va citado;
    Reenviar (Ctrl+F); Nuevo con CC y un adjunto elegido en el diálogo de
    archivos (D272): lo que llega al servidor SMTP va bien formado;
  - los enviados quedan en «Enviados»; Supr mueve a «Elementos eliminados»;
  - cerrar la redacción guarda un borrador; la libreta de direcciones;
  - el separador arrastrable.
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

PORTA_SMTP = 4225
PORTA_IMAP = 42143

# La geometría de la ventana principal (820x540), igual que en correo.c.
TB = ["Nuevo", "Responder", "Resp. a todos", "Reenviar", "Eliminar",
      "Enviar y recibir", "Libreta", "Buscar"]
TB_Y = 20 + 22
CUERPO_Y = 68


def barra_x():
    """El centro (x) de cada botón de la barra de herramientas."""
    x, centros = 4, []
    for i, t in enumerate(TB):
        ancho = max(len(t) * 8 + 16, 56)
        centros.append(x + ancho // 2)
        x += ancho
        if i in (3, 4, 5, 6):
            x += 10
    return centros


BX = barra_x()
SPLIT_X = 240


def fila_y(r):
    return CUERPO_Y + 18 + 2 + r * 18 + 9


def pasta_y(k):
    return CUERPO_Y + 4 + (k + 1) * 20 + 9


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]), y=int(g[3]))


def ponto(j, cx, cy):
    return (j["x"] + 2 + cx, j["y"] + 26 + cy)


def esperar(m, texto, tempo=40, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        if texto in m.serial().replace("\r", "")[desde:]:
            return True
        time.sleep(0.2)
    raise AssertionError(f"tiempo agotado esperando {texto!r} en la serial")


def marca(m):
    return len(m.serial().replace("\r", ""))


def esperar_envio(antes, tempo=60):
    fim = time.time() + tempo
    while time.time() < fim and len(servidor_falso_correo.ENVIADOS) == antes:
        time.sleep(0.3)
    verificar(len(servidor_falso_correo.ENVIADOS) > antes, "el servidor SMTP recibe el mensaje")
    return servidor_falso_correo.ENVIADOS[-1]


def analizar(env):
    return email.message_from_string(env["corpo"], policy=email.policy.default)


def main():
    smtp = servidor_falso_correo.servidor_falso_correo("smtp", porta_fixa=PORTA_SMTP).arrancar()
    imap = servidor_falso_correo.servidor_falso_correo("imap", porta_fixa=PORTA_IMAP).arrancar()
    imagem = imagem_de_teste("", ["--vesa"])
    try:
        with Maquina("h289", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("sagui:/$", 120)
            m.comando("echo conteudo do anexo > /tmp/nota.txt")
            m.comando(f"correo --proto imap --host 10.0.2.2 --porta {PORTA_IMAP} "
                      f"--porta-envio {PORTA_SMTP} --utilizador eu@example.org --clave chave &")
            esperar(m, '"Correo"', 40)
            time.sleep(1.5)
            j = janela(m.serial(), "Correo")
            verificar(j["l"] == 820 and j["a"] == 540, f"la ventana abre con su tamaño ({j['l']}x{j['a']})")
            esperar(m, "correo: carpeta Bandeja de entrada: 0 mensajes, 0 sin leer", 10)
            m.captura("H289-1-vacia")

            # ---- Enviar y recibir (botón de la barra)
            k = marca(m)
            m.clique(*ponto(j, BX[5], TB_Y))
            esperar(m, "correo: recibidos 3 mensajes por imap (3 nuevos)", 90, k)
            esperar(m, "correo: carpeta IMAP Trabajo: 1 mensajes (1 nuevos)", 30, k)
            esperar(m, "correo: carpeta Bandeja de entrada: 3 mensajes, 3 sin leer", 20, k)
            time.sleep(1.0)
            m.captura("H289-2-tres-paneles")
            verificar(True, "Enviar y recibir llena la bandeja y la carpeta IMAP «Trabajo»")

            # ---- elegir mensajes: vista previa y leído
            k = marca(m)
            m.clique(*ponto(j, 400, fila_y(0)))
            esperar(m, "correo: mensaje 3 (Con archivo)", 20, k)
            verificar("Adjunto: nota.txt" in m.serial() and "Va un archivo adjunto" in m.serial(),
                      "el mensaje más reciente (primera fila) sale en la vista previa con su adjunto")
            time.sleep(0.5)
            k = marca(m)
            m.clique(*ponto(j, 400, fila_y(1)))
            esperar(m, "correo: mensaje 2 (Con HTML)", 20, k)
            time.sleep(0.5)
            m.captura("H289-3-vista-previa")
            ser = m.serial()[k:]
            verificar("La versi" in ser and "<html>" not in ser, "el HTML se ve como texto en la vista previa")

            # ---- ordenar por la columna «De»
            k = marca(m)
            m.clique(*ponto(j, 300, CUERPO_Y + 9))
            esperar(m, "correo: orden por columna 2 (ascendente)", 20, k)
            time.sleep(0.5)
            k = marca(m)
            m.clique(*ponto(j, 400, fila_y(0)))
            esperar(m, "correo: mensaje 1 (Hola)", 20, k)
            m.captura("H289-4-ordenado-por-de")
            verificar(True, "ordenar por De pone primero a «ana» (mensaje 1)")
            k = marca(m)
            m.clique(*ponto(j, 300, CUERPO_Y + 9))   # otra vez: descendente
            esperar(m, "correo: orden por columna 2 (descendente)", 20, k)
            time.sleep(0.4)

            # ---- carpeta IMAP «Trabajo» (cuarta tras las fijas: índice 4)
            k = marca(m)
            m.clique(*ponto(j, 100, pasta_y(4)))
            esperar(m, "correo: carpeta Trabajo: 1 mensajes, 1 sin leer", 20, k)
            time.sleep(0.4)
            k = marca(m)
            m.clique(*ponto(j, 400, fila_y(0)))
            esperar(m, "correo: mensaje 1 (Informe trimestral)", 20, k)
            time.sleep(0.5)
            m.captura("H289-5-carpeta-trabajo")
            verificar(True, "la carpeta «Trabajo» se ve en el árbol y su mensaje en la vista previa")

            # ---- volver a la Bandeja y responder (Ctrl+R)
            k = marca(m)
            m.clique(*ponto(j, 100, pasta_y(0)))
            esperar(m, "correo: carpeta Bandeja de entrada: 3 mensajes, 0 sin leer", 20, k)
            m.clique(*ponto(j, 400, fila_y(0)))      # ordenado por De descendente: carlos primero
            time.sleep(0.8)
            k = marca(m)
            m.tecla("ctrl-r", 0.5)
            esperar(m, '"Redactar"', 30, k)
            time.sleep(1.0)
            r = janela(m.serial(), "Redactar")
            m.captura("H289-6-responder")
            antes = len(servidor_falso_correo.ENVIADOS)
            m.clique(*ponto(r, 445, 455))            # Enviar
            env = esperar_envio(antes)
            msg = analizar(env)
            verificar("carlos@example.org" in msg.get("To", ""), f"Responder: Para relleno ({msg.get('To')!r})")
            verificar(msg.get("Subject") == "RE: Con archivo", f"Responder: asunto con RE: ({msg.get('Subject')!r})")
            cuerpo = msg.get_body(preferencelist=("plain",)).get_content()
            verificar("> Va un archivo adjunto" in cuerpo, "Responder: el cuerpo original va citado con «> »")
            esperar(m, "correo: mensaje enviado a carlos@example.org", 20)
            time.sleep(1.0)

            # ---- reenviar (Ctrl+F)
            k = marca(m)
            m.tecla("ctrl-f", 0.5)
            esperar(m, '"Redactar"', 30, k)
            time.sleep(1.0)
            r = janela(m.serial(), "Redactar")
            m.clique(*ponto(r, 186, 18))
            time.sleep(0.4)
            m.digitar("luis@example.org", 0.04)
            antes = len(servidor_falso_correo.ENVIADOS)
            m.clique(*ponto(r, 445, 455))
            env = esperar_envio(antes)
            msg = analizar(env)
            verificar(msg.get("Subject") == "RV: Con archivo", f"Reenviar: asunto con RV: ({msg.get('Subject')!r})")
            verificar("Mensaje original" in msg.get_body(preferencelist=("plain",)).get_content(),
                      "Reenviar: el cuerpo lleva el mensaje original")
            time.sleep(1.0)

            # ---- Nuevo con CC y adjunto (botón «Nuevo» de la barra)
            k = marca(m)
            m.clique(*ponto(j, BX[0], TB_Y))
            esperar(m, '"Redactar"', 30, k)
            time.sleep(1.0)
            r = janela(m.serial(), "Redactar")
            m.clique(*ponto(r, 186, 18))
            m.digitar("ana@example.org", 0.04)
            m.clique(*ponto(r, 186, 48))
            m.digitar("copia@example.org", 0.04)
            m.clique(*ponto(r, 186, 78))
            m.digitar("Prueba Outlook", 0.04)
            m.clique(*ponto(r, 200, 200))
            m.digitar("Hola con adjunto", 0.04)
            time.sleep(0.4)
            k = marca(m)
            m.clique(*ponto(r, 60, 455))             # Adjuntar: el diálogo de archivos
            esperar(m, "dialogo: abrir en", 20, k)
            time.sleep(0.8)
            m.captura("H289-7-dialogo-adjuntar")
            m.digitar("/tmp/nota.txt", 0.04)
            m.tecla("ret", 0.3)
            esperar(m, "correo: adjunto nota.txt", 20, k)
            time.sleep(0.6)
            m.captura("H289-8-redactar-con-adjunto")
            antes = len(servidor_falso_correo.ENVIADOS)
            m.clique(*ponto(r, 445, 455))
            env = esperar_envio(antes)
            msg = analizar(env)
            verificar(msg.get("Cc") == "copia@example.org", f"Nuevo: la cabecera Cc va en el mensaje ({msg.get('Cc')!r})")
            verificar("copia@example.org" in " ".join(env["para"]) and "ana@example.org" in " ".join(env["para"]),
                      f"SMTP: un RCPT TO por cada destinatario ({env['para']})")
            anexos = list(msg.iter_attachments())
            verificar(len(anexos) == 1 and anexos[0].get_filename() == "nota.txt", "Nuevo: lleva el adjunto elegido en el diálogo")
            verificar(msg.get("Date") is not None, "el mensaje lleva la cabecera Date")
            time.sleep(1.0)

            # ---- Enviados
            k = marca(m)
            m.clique(*ponto(j, 100, pasta_y(1)))
            esperar(m, "correo: carpeta Enviados: 3 mensajes", 20, k)
            time.sleep(0.5)
            m.captura("H289-9-enviados")
            verificar(True, "los tres mensajes enviados quedan en «Enviados»")

            # ---- Supr: a Elementos eliminados
            m.clique(*ponto(j, 100, pasta_y(0)))
            time.sleep(0.8)
            m.clique(*ponto(j, 400, fila_y(0)))
            time.sleep(0.8)
            k = marca(m)
            m.tecla("delete", 0.5)
            esperar(m, "movido a Elementos eliminados", 20, k)
            time.sleep(0.5)
            k = marca(m)
            m.clique(*ponto(j, 100, pasta_y(3)))
            esperar(m, "correo: carpeta Elementos eliminados: 1 mensajes", 20, k)
            time.sleep(0.5)
            m.captura("H289-10-eliminados")
            verificar(True, "Supr mueve el mensaje a «Elementos eliminados»")

            # ---- borrador: cerrar la redacción con texto y guardar
            m.clique(*ponto(j, 100, pasta_y(0)))
            time.sleep(0.6)
            k = marca(m)
            m.tecla("ctrl-n", 0.5)
            esperar(m, '"Redactar"', 30, k)
            time.sleep(1.0)
            r = janela(m.serial(), "Redactar")
            m.clique(*ponto(r, 186, 78))
            m.digitar("Borrador de prueba", 0.04)
            m.clique(*ponto(r, 545, 455))            # Cancelar
            time.sleep(1.0)
            m.captura("H289-11-guardar-borrador")
            m.tecla("ret", 0.5)                      # «Sí» es el primer botón
            esperar(m, "correo: borrador guardado (asunto Borrador de prueba)", 20, k)
            time.sleep(0.8)
            k = marca(m)
            m.clique(*ponto(j, 100, pasta_y(2)))
            esperar(m, "correo: carpeta Borradores: 1 mensajes", 20, k)
            verificar(True, "cerrar la redacción con texto guarda un borrador")

            # ---- libreta de direcciones (botón de la barra)
            k = marca(m)
            m.clique(*ponto(j, BX[6], TB_Y))
            esperar(m, '"Libreta de direcciones"', 20, k)
            time.sleep(1.0)
            lb = janela(m.serial(), "Libreta de direcciones")
            m.clique(*ponto(lb, 50, 303))            # Nuevo
            time.sleep(1.0)
            m.digitar("Eva Prueba", 0.04)
            m.tecla("ret", 0.8)
            time.sleep(0.8)
            m.digitar("eva@example.org", 0.04)
            m.tecla("ret", 0.5)
            esperar(m, "correo: contacto añadido: Eva Prueba <eva@example.org>", 20, k)
            time.sleep(0.5)
            m.captura("H289-12-libreta")
            m.clique(*ponto(lb, 430, 303))           # Cerrar
            time.sleep(0.8)
            verificar(True, "la libreta guarda un contacto nuevo")

            # ---- columnas: el borde derecho de «De» se arrastra (De mide 160 al principio)
            k = marca(m)
            borde = 242 + 40 + 160
            m.arrastar(j["x"] + 2 + borde, j["y"] + 26 + CUERPO_Y + 9, j["x"] + 2 + borde + 40, j["y"] + 26 + CUERPO_Y + 9)
            esperar(m, "correo: columna De: 200 px", 20, k)
            time.sleep(0.6)
            m.captura("H289-14-columnas")
            verificar(True, "el borde de la columna De se arrastra")

            # ---- separador arrastrable
            k = marca(m)
            m.arrastar(j["x"] + 2 + SPLIT_X, j["y"] + 26 + 300, j["x"] + 2 + 260, j["y"] + 26 + 300)
            esperar(m, "correo: separador vertical en 260", 20, k)
            time.sleep(0.6)
            m.captura("H289-13-separador")
            verificar(True, "el separador del árbol se arrastra")

            m.tecla("alt-f4", 0.5)
            time.sleep(1.5)
            out = m.comando("ls /home/correo/Enviados /home/correo/Eliminados /home/correo/Borradores /home/correo/imap")
            verificar(len(re.findall(r"m\d+\.eml", out)) >= 5, "las carpetas quedan en el disco")
            out = m.comando("cat /home/.config/correo.libreta")
            verificar("eva@example.org" in out, "la libreta queda en /home/.config/correo.libreta")
    finally:
        smtp.parar()
        imap.parar()
    return 0


if __name__ == "__main__":
    sys.exit(main())
