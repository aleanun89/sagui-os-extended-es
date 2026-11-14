#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""servidor_falso_correo.py - un servidor falso de correo (SMTP, POP3 e IMAP).

Sirve para la prueba del cliente de correo (mejora 082) sin Internet y sin
cuenta de verdad: habla los tres protocolos lo bastante bien como para que el
programa `correo` los use tal cual.

  SMTP (enviar)     : EHLO, HELO, STARTTLS, AUTH LOGIN/PLAIN, MAIL FROM,
                       RCPT TO, DATA, QUIT, RSET, NOOP.
  POP3 (recibir)    : USER, PASS, STAT, LIST, UIDL, RETR, TOP, DELE, QUIT.
  IMAP (recibir)    : CAPABILITY, LOGIN, SELECT, FETCH (con literales),
                       LIST, LOGOUT.

Los mensajes que entrega están en la lista `MENSAGENS` (uno de texto plano, uno
con texto plano y HTML, y uno con un adjunto en base64). Lo que se manda se
guarda en `ENVIADOS` y el cuerpo en `CORPO_RECIDO`, para que la prueba lo
compruebe.

El servidor escucha en 127.0.0.1 en un puerto libre y no escribe nada en disco.
"""
import socket
import socketserver
import threading

# Los mensajes que se sirven. Texto plano: el primero; con HTML: el segundo;
# con adjunto: el tercero.
MENSAGENS = [
    """From: ana@example.org
To: eu@example.org
Subject: Hola
Date: Tue, 11 Nov 2025 10:00:00 +0000
Content-Type: text/plain; charset=utf-8

¡Hola! Esto es un mensaje en texto plano.
""",
    """From: bia@example.org
To: eu@example.org
Subject: Con HTML
Date: Tue, 11 Nov 2025 11:30:00 +0000
Content-Type: multipart/alternative; boundary="sep123"

--sep123
Content-Type: text/plain; charset=utf-8

La versión en texto plano.
--sep123
Content-Type: text/html; charset=utf-8

<html><body><p>La versión <b>en HTML</b>.</p><p>Línea &amp; más.</p></body></html>
--sep123--
""",
    """From: carlos@example.org
To: eu@example.org
Subject: Con archivo
Date: Tue, 11 Nov 2025 12:00:00 +0000
Content-Type: multipart/mixed; boundary="sep456"

--sep456
Content-Type: text/plain; charset=utf-8

Va un archivo adjunto.
--sep456
Content-Type: text/plain; charset=utf-8; name="nota.txt"
Content-Disposition: attachment; filename="nota.txt"
Content-Transfer-Encoding: base64

ZXN0byBlcyB1biBhcmNoaXZvIGNvbiBlbCBjb250ZW5pZG8uCg==
--sep456--
""",
]

# Lo que el servidor recibió (la prueba lo mira).
ENVIADOS = []
CORPO_RECIDO = []


class _Sessao(socketserver.BaseRequestHandler):
    """Lo común a los tres protocolos: líneas de texto en los dos sentidos."""

    def setup(self):
        self.rfile = self.request.makefile("rb")
        self.wfile = self.request.makefile("wb")

    def handle(self):
        try:
            self.trata()
        except SystemExit:
            pass
        except OSError:
            pass
        finally:
            self.fechar()

    def linha(self):
        """Lee una línea terminada en CRLF (o LF) y la devuelve sin el final."""
        dados = self.rfile.readline()
        if not dados:
            return ""
        return dados.decode("utf-8", "replace").rstrip("\r\n")

    def escrever(self, texto):
        self.wfile.write((texto + "\r\n").encode("utf-8"))
        self.wfile.flush()

    def escrever_bruto(self, dados):
        self.wfile.write(dados)
        self.wfile.flush()

    def fechar(self):
        try:
            self.request.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        try:
            self.request.close()
        except OSError:
            pass


class Pop3(_Sessao):
    """POP3 (RFC 1939) para la prueba."""

    def __init__(self, *a, **k):
        self.borrados = set()
        super().__init__(*a, **k)

    def trata(self):
        self.escrever("+OK servidor falso de correo")
        while True:
            linha = self.linha()
            if not linha:
                break
            partes = linha.split()
            if not partes:
                self.escrever("-ERR falta el comando")
                continue
            nome = partes[0].upper()
            resto = partes[1:]
            getattr(self, "cmd_" + nome, self.cmd_desconocida)(resto)

    def cmd_desconocida(self, resto):
        self.escrever("-ERR comando desconocido")

    def cmd_USER(self, resto):
        self.escrever("+OK usuario aceptado")

    def cmd_PASS(self, resto):
        self.escrever("+OK buzón listo")

    def cmd_STAT(self, resto):
        self.escrever("+OK %d %d" % (len(MENSAGENS), sum(len(m) for m in MENSAGENS)))

    def cmd_LIST(self, resto):
        self.escrever("+OK %d mensajes" % len(MENSAGENS))
        for i, m in enumerate(MENSAGENS, 1):
            self.escrever("%d %d" % (i, len(m)))
        self.escrever(".")

    def cmd_UIDL(self, resto):
        self.escrever("+OK")
        for i, _ in enumerate(MENSAGENS, 1):
            self.escrever("%d uid%04d" % (i, i))
        self.escrever(".")

    def cmd_RETR(self, resto):
        n = self.numero(resto)
        if n is None:
            return
        if n < 1 or n > len(MENSAGENS):
            self.escrever("-ERR no existe ese mensaje")
            return
        self.escrever("+OK %d octetos" % len(MENSAGENS[n - 1]))
        # Punto a punto: las líneas que empiezan por "." van con otro "." delante.
        for l in MENSAGENS[n - 1].split("\n"):
            self.escrever("." + l if l.startswith(".") else l)
        self.escrever(".")

    def cmd_TOP(self, resto):
        n = self.numero(resto)
        if n is None:
            return
        if len(resto) < 2:
            self.escrever("-ERR uso: TOP <n> <linhas>")
            return
        try:
            linhas = int(resto[1])
        except ValueError:
            self.escrever("-ERR uso: TOP <n> <linhas>")
            return
        if n < 1 or n > len(MENSAGENS):
            self.escrever("-ERR no existe ese mensaje")
            return
        pedazos = MENSAGENS[n - 1].split("\n\n", 1)
        corpo = pedazos[1] if len(pedazos) > 1 else ""
        self.escrever("+OK")
        for l in corpo.split("\n")[:linhas]:
            self.escrever("." + l if l.startswith(".") else l)
        self.escrever(".")

    def cmd_DELE(self, resto):
        n = self.numero(resto)
        if n is None:
            return
        self.borrados.add(n)
        self.escrever("+OK mensaje %d borrado" % n)

    def cmd_NOOP(self, resto):
        self.escrever("+OK")

    def cmd_QUIT(self, resto):
        self.escrever("+OK")
        raise SystemExit

    def numero(self, resto):
        if not resto:
            self.escrever("-ERR falta el número")
            return None
        try:
            return int(resto[0])
        except ValueError:
            self.escrever("-ERR número inválido")
            return None


class Smtp(_Sessao):
    """SMTP (RFC 5321) para la prueba."""

    def __init__(self, *a, **k):
        self.de = ""
        self.para = []
        self.corpo = []
        super().__init__(*a, **k)

    def trata(self):
        self.escrever("220 servidor falso de correo")
        while True:
            linha = self.linha()
            if not linha:
                break
            partes = linha.split()
            if not partes:
                self.escrever("500 falta el comando")
                continue
            nome = partes[0].upper()
            resto = partes[1:]
            getattr(self, "cmd_" + nome, self.cmd_desconocida)(resto)

    def cmd_desconocida(self, resto):
        self.escrever("500 comando no reconocido")

    def cmd_EHLO(self, resto):
        self.escrever("250-servidor falso de correo")
        self.escrever("250-AUTH LOGIN PLAIN")
        self.escrever("250 8BITMIME")

    def cmd_HELO(self, resto):
        self.escrever("250 servidor falso de correo")

    def cmd_STARTTLS(self, resto):
        # Sin TLS de verdad: el servidor dice que no lo tiene, como los de prueba.
        self.escrever("454 TLS no disponible en el servidor falso")

    def cmd_AUTH(self, resto):
        if not resto:
            self.escrever("504 falta el mecanismo")
            return
        mecanismo = resto[0].upper()
        if mecanismo == "PLAIN":
            self.escrever("235 aceptado")
        elif mecanismo == "LOGIN":
            self.escrever("334 VXNlcm5hbWU6")   # "Username:"
            self.linha()
            self.escrever("334 UGFzc3dvcmQ=")   # "Password:"
            self.linha()
            self.escrever("235 aceptado")
        else:
            self.escrever("504 mecanismo no soportado")

    def cmd_MAIL(self, resto):
        self.de = (resto[0].split(":", 1)[-1].strip()) if resto else ""
        self.escrever("250 remitente ok")

    def cmd_RCPT(self, resto):
        self.para.append(resto[0].split(":", 1)[-1].strip() if resto else "")
        self.escrever("250 destinatario ok")

    def cmd_DATA(self, resto):
        self.escrever("354 manda el mensaje; termina con una línea con un punto")
        self.corpo = []
        while True:
            # Se lee a pelo: una linea vacia es parte del mensaje y no el fin.
            bruta = self.rfile.readline()
            if not bruta:
                break
            l = bruta.decode("utf-8", "replace").rstrip("\r\n")
            if l == ".":
                break
            if l.startswith(".."):
                l = l[1:]
            self.corpo.append(l)
        texto = "\n".join(self.corpo) + "\n"
        CORPO_RECIDO.append(texto)
        ENVIADOS.append({"de": self.de, "para": list(self.para), "corpo": texto})
        self.escrever("250 mensaje aceptada")

    def cmd_RSET(self, resto):
        self.de = ""
        self.para = []
        self.escrever("250 ok")

    def cmd_NOOP(self, resto):
        self.escrever("250 ok")

    def cmd_QUIT(self, resto):
        self.escrever("221 hasta luego")
        raise SystemExit


class Imap(_Sessao):
    """IMAP4rev1 (RFC 3501) para la prueba, con literales."""

    def trata(self):
        self.escrever("* OK [CAPABILITY IMAP4rev1 AUTH=PLAIN] servidor falso de correo")
        while True:
            linha = self.linha()
            if not linha:
                break
            # En IMAP toda orden lleva delante una etiqueta: "a1 FETCH 1:3 (BODY[])".
            partes = linha.split(" ", 2)
            etiqueta = partes[0] if partes else ""
            nome = partes[1].upper() if len(partes) > 1 else ""
            resto = partes[2] if len(partes) > 2 else ""
            if not nome:
                self.escrever("%s BAD falta el comando" % etiqueta)
                continue
            getattr(self, "cmd_" + nome, self.cmd_desconocida)(etiqueta, resto)

    def cmd_desconocida(self, etiqueta, resto):
        self.escrever("%s BAD comando desconocido" % etiqueta)

    def cmd_CAPABILITY(self, etiqueta, resto):
        self.escrever("* CAPABILITY IMAP4rev1 AUTH=PLAIN")
        self.escrever("%s OK CAPABILITY completada" % etiqueta)

    def cmd_LOGIN(self, etiqueta, resto):
        self.escrever("%s OK LOGIN completado" % etiqueta)

    def cmd_SELECT(self, etiqueta, resto):
        self.escrever("* %d EXISTS" % len(MENSAGENS))
        self.escrever("* 0 RECENT")
        self.escrever("* OK [UIDVALIDITY 1] UIDs validos")
        self.escrever("%s OK [READ-WRITE] SELECT completado" % etiqueta)

    def cmd_LIST(self, etiqueta, resto):
        self.escrever('* LIST (\\HasNoChildren) "." "INBOX"')
        self.escrever("%s OK LIST completado" % etiqueta)

    def cmd_FETCH(self, etiqueta, resto):
        # Acepta "1", "1:3" y "2:*" (los tres los usa el cliente).
        numeros = []
        for pedazo in resto.split(" "):
            if pedazo.startswith("(") or pedazo == "":
                continue
            if ":" in pedazo:
                a, b = pedazo.split(":", 1)
                try:
                    a = int(a)
                except ValueError:
                    continue
                if b == "*" or b == "":
                    b = len(MENSAGENS)
                else:
                    try:
                        b = int(b)
                    except ValueError:
                        continue
                numeros.extend(range(a, b + 1))
            else:
                try:
                    numeros.append(int(pedazo))
                except ValueError:
                    continue
        for n in numeros:
            if n < 1 or n > len(MENSAGENS):
                continue
            msg = MENSAGENS[n - 1].encode("utf-8")
            self.escrever("* %d FETCH (BODY[] {%d}" % (n, len(msg)))
            self.escrever_bruto(msg)
            self.escrever_bruto(b")\r\n")
        self.escrever("%s OK FETCH completado" % etiqueta)

    def cmd_LOGOUT(self, etiqueta, resto):
        self.escrever("* BYE sale del servidor falso")
        self.escrever("%s OK LOGOUT completado" % etiqueta)
        raise SystemExit

    def cmd_NOOP(self, etiqueta, resto):
        self.escrever("%s OK NOOP" % etiqueta)


class _Servidor(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


class servidor_falso_correo:
    """Servidor falso de correo: guarda el socket.

    `porta_fixa` sirve para las pruebas, que necesitan saber el puerto antes de
    arrancar la máquina (la línea de arranque del programa tiene un límite de
    63 caracteres)."""

    def __init__(self, modo="pop3", host="127.0.0.1", porta_fixa=0):
        self.modo = modo
        classe = {"pop3": Pop3, "smtp": Smtp, "imap": Imap}[modo]
        self.httpd = _Servidor((host, porta_fixa), classe)
        self.hilo = threading.Thread(target=self.httpd.serve_forever, daemon=True)

    @property
    def porta(self):
        return self.httpd.server_address[1]

    def arrancar(self):
        self.hilo.start()
        return self

    def parar(self):
        self.httpd.shutdown()
        self.httpd.server_close()


def principal():
    import sys
    modo = sys.argv[1] if len(sys.argv) > 1 else "pop3"
    s = servidor_falso_correo(modo).arrancar()
    print("servidor falso de correo (%s) en 127.0.0.1:%d" % (modo, s.porta), flush=True)
    try:
        while True:
            threading.Event().wait(1)
    except KeyboardInterrupt:
        s.parar()


if __name__ == "__main__":
    principal()
