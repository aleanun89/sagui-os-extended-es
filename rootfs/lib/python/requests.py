# SPDX-License-Identifier: GPL-2.0-or-later
# requests.py - um "requests" mínimo para o MicroPython do SaguiOS (D069).
#
# A interface é a da biblioteca requests do CPython, no que um script costuma
# usar:
#     import requests
#     r = requests.get("http://10.0.2.2:8000/dados", params={"q": "sagui"}, timeout=5)
#     r.status_code, r.ok, r.headers["content-type"], r.text, r.content, r.json()
#     requests.post(url, json={"a": 1}); requests.put/delete/head/patch(...)
#
# HTTP/1.1 com "Connection: close" sobre o módulo socket, e HTTPS pelo módulo
# ssl (a libtls do SaguiOS, que valida o certificado com /etc/ssl/certs;
# verify=False não valida, verify="/caminho/ca.pem" usa essa CA). Segue
# redirecionamentos (até 10), entende "Transfer-Encoding: chunked" e
# Content-Length, e timeout= vale para conectar e para cada leitura.
# Não tem: sessões com cookies, streaming, proxies, compressão.

import socket
import json as _json
from urllib.parse import urlsplit, urlencode, urljoin, quote

try:
    import ssl
except ImportError:
    ssl = None

__version__ = "2.0-sagui"
_AGENTE = "python-requests/" + __version__ + " (SaguiOS; MicroPython)"


# ------------------------------------------------------------ exceções

class RequestException(OSError):
    def __init__(self, *args, response=None, request=None):
        super().__init__(*args)
        self.response = response
        self.request = request


class ConnectionError(RequestException):
    pass


class SSLError(ConnectionError):
    pass


class Timeout(RequestException):
    pass


# No requests do CPython ConnectTimeout também é ConnectionError, mas o
# MicroPython não deixa uma classe herdar de duas exceções nativas.
class ConnectTimeout(Timeout):
    pass


class ReadTimeout(Timeout):
    pass


class HTTPError(RequestException):
    pass


class TooManyRedirects(RequestException):
    pass


class InvalidURL(RequestException):
    pass


class JSONDecodeError(ValueError):
    pass


exceptions = __import__("requests")   # requests.exceptions.Timeout etc. (o próprio módulo)


# ------------------------------------------------------------ cabeçalhos

class CaseInsensitiveDict:
    """Dicionário de cabeçalhos: r.headers["Content-Type"] e
    r.headers["content-type"] dão o mesmo."""

    def __init__(self, dados=None):
        self._d = {}
        if dados:
            for k, v in (dados.items() if hasattr(dados, "items") else dados):
                self[k] = v

    def __setitem__(self, k, v):
        self._d[k.lower()] = (k, v)

    def __getitem__(self, k):
        return self._d[k.lower()][1]

    def __delitem__(self, k):
        del self._d[k.lower()]

    def __contains__(self, k):
        return k.lower() in self._d

    def __iter__(self):
        return (k for k, _ in self._d.values())

    def __len__(self):
        return len(self._d)

    def get(self, k, padrao=None):
        par = self._d.get(k.lower())
        return par[1] if par else padrao

    def items(self):
        return [(k, v) for k, v in self._d.values()]

    def keys(self):
        return [k for k, _ in self._d.values()]

    def values(self):
        return [v for _, v in self._d.values()]

    def __repr__(self):
        return repr(dict(self.items()))


# ------------------------------------------------------------ resposta

class Response:
    def __init__(self):
        self.status_code = 0
        self.reason = ""
        self.headers = CaseInsensitiveDict()
        self.url = ""
        self.content = b""
        self.encoding = None
        self.history = []
        self.elapsed_ms = 0
        self.request = None

    @property
    def ok(self):
        return self.status_code < 400

    @property
    def text(self):
        cod = self.encoding or _charset(self.headers.get("content-type", "")) or "utf-8"
        try:
            return self.content.decode(cod)
        except (UnicodeError, LookupError):
            # Não é UTF-8 (ou o charset é desconhecido): cada byte vira um
            # caractere, como o Latin-1 do SaguiOS.
            return "".join(chr(b) for b in self.content)

    def json(self, **kw):
        try:
            return _json.loads(self.text)
        except ValueError as e:
            raise JSONDecodeError("la respuesta no es JSON válido: " + str(e))

    def raise_for_status(self):
        if 400 <= self.status_code < 600:
            tipo = "Client" if self.status_code < 500 else "Server"
            raise HTTPError("%d %s Error: %s for url: %s" % (self.status_code, tipo, self.reason, self.url), response=self)

    def iter_content(self, chunk_size=1024, decode_unicode=False):
        for i in range(0, len(self.content), chunk_size):
            yield self.content[i:i + chunk_size]

    def iter_lines(self, chunk_size=512, decode_unicode=False, delimiter=None):
        for linha in self.content.split(delimiter or b"\n"):
            yield linha.rstrip(b"\r")

    def close(self):
        pass

    def __bool__(self):
        return self.ok

    def __repr__(self):
        return "<Response [%d]>" % self.status_code


def _decodificar(b):
    """Bytes de um cabeçalho: UTF-8 se der, senão um caractere por byte."""
    try:
        return b.decode()
    except UnicodeError:
        return "".join(chr(c) for c in b)


def _charset(tipo):
    for parte in tipo.split(";")[1:]:
        parte = parte.strip()
        if parte.lower().startswith("charset="):
            return parte[8:].strip('"').lower()
    return None


# ------------------------------------------------------------ conexão

class _Leitor:
    """Lê de um socket (ou SSLSocket) por pedaços: linhas do cabeçalho e o
    corpo, sem ler um byte por vez."""

    def __init__(self, s):
        self.s = s
        self.buf = b""

    def _mais(self):
        d = self.s.recv(4096)
        if d:
            self.buf += d
        return d

    def linha(self):
        while b"\n" not in self.buf:
            if not self._mais():
                break
        i = self.buf.find(b"\n")
        if i < 0:
            linha, self.buf = self.buf, b""
        else:
            linha, self.buf = self.buf[:i + 1], self.buf[i + 1:]
        return linha

    def exato(self, n):
        partes = [self.buf[:n]]
        falta = n - len(partes[0])
        self.buf = self.buf[n:]
        while falta > 0:
            d = self.s.recv(min(falta, 16384))
            if not d:
                raise ConnectionError("la conexión se cerró antes de terminar la respuesta")
            partes.append(d)
            falta -= len(d)
        return b"".join(partes)

    def tudo(self):
        partes = [self.buf]
        self.buf = b""
        while True:
            d = self.s.recv(16384)
            if not d:
                break
            partes.append(d)
        return b"".join(partes)


def _timeout(timeout):
    if isinstance(timeout, (tuple, list)):   # (conectar, ler)
        return timeout[0], timeout[1]
    return timeout, timeout


def _abrir(esquema, host, porta, timeout, verify, cert):
    t_con, t_ler = _timeout(timeout)
    try:
        ender = socket.getaddrinfo(host, porta, 0, socket.SOCK_STREAM)[0][-1]
    except OSError as e:
        raise ConnectionError("no se pudo resolver %s: %s" % (host, _texto_erro(e)))
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.settimeout(t_con)
        s.connect(ender)
        s.settimeout(t_ler)
        if esquema == "https":
            if ssl is None:
                raise SSLError("este MicroPython no tiene el módulo ssl")
            if verify is False:
                ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
                ctx.check_hostname = False
                ctx.verify_mode = ssl.CERT_NONE
            elif isinstance(verify, str):
                ctx = ssl.create_default_context(cafile=verify)
            else:
                ctx = ssl.create_default_context()
            try:
                s = ctx.wrap_socket(s, server_hostname=host)
            except ssl.SSLError as e:
                raise SSLError(_texto_erro(e))
    except socket.timeout:
        s.close()
        raise ConnectTimeout("tiempo agotado al conectar con %s:%d" % (host, porta))
    except OSError as e:
        s.close()
        if isinstance(e, RequestException):
            raise
        raise ConnectionError("%s:%d: %s" % (host, porta, _texto_erro(e)))
    return s


def _texto_erro(e):
    a = getattr(e, "args", ())
    if len(a) >= 2 and isinstance(a[1], str):
        return a[1]
    if len(a) == 1:
        return str(a[0])
    return str(e)


def _corpo_chunked(leitor):
    partes = []
    while True:
        linha = leitor.linha()
        if not linha:
            raise ConnectionError("respuesta chunked cortada")
        tam = int(linha.split(b";")[0].strip() or b"0", 16)
        if tam == 0:
            while leitor.linha().strip():   # trailers até a linha vazia
                pass
            break
        partes.append(leitor.exato(tam))
        leitor.linha()   # o \r\n depois do pedaço
    return b"".join(partes)


def _um_pedido(metodo, url, cabecalhos, corpo, timeout, verify, cert):
    partes = urlsplit(url)
    esquema = partes.scheme.lower()
    if esquema not in ("http", "https"):
        raise InvalidURL("dirección no soportada (solo http y https): " + url)
    host = partes.hostname
    if not host:
        raise InvalidURL("dirección sin servidor: " + url)
    porta = partes.port or (443 if esquema == "https" else 80)
    caminho = partes.path or "/"
    if partes.query:
        caminho += "?" + partes.query
    caminho = quote(caminho, safe="/?&=%:@!$'()*+,;~#[]")

    h = CaseInsensitiveDict()
    h["Host"] = host if porta in (80, 443) else "%s:%d" % (host, porta)
    h["User-Agent"] = _AGENTE
    h["Accept"] = "*/*"
    h["Accept-Encoding"] = "identity"
    h["Connection"] = "close"
    for k, v in cabecalhos.items():
        h[k] = v
    if corpo is not None or metodo in ("POST", "PUT", "PATCH"):
        h["Content-Length"] = str(len(corpo or b""))
    pedido = metodo + " " + caminho + " HTTP/1.1\r\n"
    for k, v in h.items():
        pedido += k + ": " + str(v) + "\r\n"
    pedido += "\r\n"

    s = _abrir(esquema, host, porta, timeout, verify, cert)
    r = Response()
    r.url = url
    try:
        s.sendall(pedido.encode())
        if corpo:
            s.sendall(corpo)
        leitor = _Leitor(s)
        estado = leitor.linha()
        while estado.startswith(b"HTTP/1.1 100"):   # 100 Continue: vem outra linha de estado
            while leitor.linha().strip():
                pass
            estado = leitor.linha()
        partes_estado = _decodificar(estado).strip().split(" ", 2)
        if len(partes_estado) < 2 or not partes_estado[0].startswith("HTTP/"):
            raise ConnectionError("respuesta HTTP no válida: %r" % estado[:60])
        r.status_code = int(partes_estado[1])
        r.reason = partes_estado[2] if len(partes_estado) > 2 else ""
        while True:
            linha = leitor.linha()
            if not linha or not linha.strip():
                break
            if b":" in linha:
                k, v = linha.split(b":", 1)
                r.headers[_decodificar(k).strip()] = _decodificar(v).strip()
        if metodo == "HEAD" or r.status_code in (204, 304) or 100 <= r.status_code < 200:
            r.content = b""
        elif "chunked" in r.headers.get("transfer-encoding", "").lower():
            r.content = _corpo_chunked(leitor)
        elif "content-length" in r.headers:
            r.content = leitor.exato(int(r.headers["content-length"]))
        else:
            r.content = leitor.tudo()
    except socket.timeout:
        raise ReadTimeout("tiempo agotado esperando la respuesta de %s" % host)
    except OSError as e:
        if isinstance(e, RequestException):
            raise
        raise ConnectionError("%s: %s" % (host, _texto_erro(e)))
    finally:
        s.close()
    return r


# ------------------------------------------------------------ interface

def request(method, url, params=None, data=None, headers=None, cookies=None, files=None, auth=None,
            timeout=None, allow_redirects=True, proxies=None, hooks=None, stream=None, verify=True,
            cert=None, json=None):
    metodo = method.upper()
    if params:
        consulta = params if isinstance(params, str) else urlencode(params, doseq=True)
        url += ("&" if "?" in url else "?") + consulta
    cab = dict(headers or {})
    corpo = None
    if json is not None:
        corpo = _json.dumps(json).encode()
        cab.setdefault("Content-Type", "application/json")
    elif data is not None:
        if isinstance(data, (dict, list, tuple)):
            corpo = urlencode(data, doseq=True).encode()
            cab.setdefault("Content-Type", "application/x-www-form-urlencoded")
        elif isinstance(data, str):
            corpo = data.encode()
        else:
            corpo = bytes(data)
    if auth is not None:
        import binascii
        usuario, senha = auth
        par = ("%s:%s" % (usuario, senha)).encode()
        cab.setdefault("Authorization", "Basic " + binascii.b2a_base64(par).decode().strip())
    if cookies:
        cab.setdefault("Cookie", "; ".join("%s=%s" % (k, v) for k, v in cookies.items()))

    historico = []
    for _ in range(11):
        r = _um_pedido(metodo, url, cab, corpo, timeout, verify, cert)
        destino = r.headers.get("location")
        if not allow_redirects or r.status_code not in (301, 302, 303, 307, 308) or not destino:
            r.history = historico
            return r
        historico.append(r)
        url = urljoin(url, destino)
        if r.status_code == 303 or (r.status_code in (301, 302) and metodo == "POST"):
            # como os navegadores (e o requests): vira GET sem corpo
            metodo = "GET" if metodo != "HEAD" else "HEAD"
            corpo = None
            for k in list(cab):
                if k.lower() in ("content-type", "content-length"):
                    del cab[k]
    raise TooManyRedirects("demasiadas redirecciones (más de 10)", response=r)


def get(url, params=None, **kw):
    return request("GET", url, params=params, **kw)


def head(url, **kw):
    kw.setdefault("allow_redirects", False)
    return request("HEAD", url, **kw)


def post(url, data=None, json=None, **kw):
    return request("POST", url, data=data, json=json, **kw)


def put(url, data=None, **kw):
    return request("PUT", url, data=data, **kw)


def patch(url, data=None, **kw):
    return request("PATCH", url, data=data, **kw)


def delete(url, **kw):
    return request("DELETE", url, **kw)


def options(url, **kw):
    return request("OPTIONS", url, **kw)


class Session:
    """Sessão mínima: guarda cabeçalhos comuns (não guarda cookies nem
    reaproveita a conexão)."""

    def __init__(self):
        self.headers = {}

    def request(self, method, url, headers=None, **kw):
        h = dict(self.headers)
        h.update(headers or {})
        return request(method, url, headers=h, **kw)

    def get(self, url, **kw):
        return self.request("GET", url, **kw)

    def post(self, url, **kw):
        return self.request("POST", url, **kw)

    def put(self, url, **kw):
        return self.request("PUT", url, **kw)

    def delete(self, url, **kw):
        return self.request("DELETE", url, **kw)

    def head(self, url, **kw):
        kw.setdefault("allow_redirects", False)
        return self.request("HEAD", url, **kw)

    def close(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *a):
        pass
