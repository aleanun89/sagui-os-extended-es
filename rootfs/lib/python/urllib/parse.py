# SPDX-License-Identifier: GPL-2.0-or-later
# urllib/parse.py - partes de endereços (URL) para o MicroPython do SaguiOS.
#
# O pedaço do urllib.parse do CPython que os programas mais usam: quote,
# quote_plus, unquote, unquote_plus, urlencode, urlparse/urlsplit,
# urlunparse/urlunsplit, urljoin, parse_qs e parse_qsl. O texto é str do
# Python (UTF-8): o que não é ASCII vira %XX dos bytes UTF-8, como no CPython.

__all__ = ["quote", "quote_plus", "unquote", "unquote_plus", "urlencode", "urlparse", "urlsplit",
           "urlunparse", "urlunsplit", "urljoin", "parse_qs", "parse_qsl", "ParseResult", "SplitResult"]

_SEGUROS = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-~"
_HEXA = "0123456789ABCDEF"


def _bytes(s, encoding="utf-8"):
    if isinstance(s, (bytes, bytearray)):
        return bytes(s)
    return str(s).encode(encoding)


def quote(texto, safe="/", encoding="utf-8", errors=None):
    seguros = _SEGUROS + _bytes(safe)
    partes = []
    for b in _bytes(texto, encoding):
        if b in seguros:
            partes.append(chr(b))
        else:
            partes.append("%" + _HEXA[b >> 4] + _HEXA[b & 15])
    return "".join(partes)


def quote_plus(texto, safe="", encoding="utf-8", errors=None):
    if " " in texto if isinstance(texto, str) else b" " in texto:
        return quote(texto, safe + " ", encoding).replace(" ", "+")
    return quote(texto, safe, encoding)


def unquote_to_bytes(texto):
    b = _bytes(texto)
    if b"%" not in b:
        return b
    saida = bytearray()
    i, n = 0, len(b)
    while i < n:
        c = b[i]
        if c == 37 and i + 2 < n:   # "%XX"
            try:
                saida.append(int(chr(b[i + 1]) + chr(b[i + 2]), 16))
                i += 3
                continue
            except ValueError:
                pass
        saida.append(c)
        i += 1
    return bytes(saida)


def unquote(texto, encoding="utf-8", errors="replace"):
    b = unquote_to_bytes(texto)
    try:
        return b.decode(encoding)
    except UnicodeError:
        return "".join(chr(c) for c in b)   # não é UTF-8: cada byte vira um caractere (Latin-1)


def unquote_plus(texto, encoding="utf-8", errors="replace"):
    return unquote(texto.replace("+", " "), encoding, errors)


def urlencode(query, doseq=False, safe="", encoding="utf-8", errors=None, quote_via=quote_plus):
    if hasattr(query, "items"):
        query = list(query.items())
    partes = []
    for k, v in query:
        k = quote_via(k if isinstance(k, (str, bytes)) else str(k), safe)
        if doseq and isinstance(v, (list, tuple)):
            for x in v:
                partes.append(k + "=" + quote_via(x if isinstance(x, (str, bytes)) else str(x), safe))
        else:
            partes.append(k + "=" + quote_via(v if isinstance(v, (str, bytes)) else str(v), safe))
    return "&".join(partes)


def parse_qsl(qs, keep_blank_values=False, strict_parsing=False, encoding="utf-8", errors="replace", separator="&"):
    pares = []
    for parte in qs.split(separator):
        if not parte:
            continue
        if "=" in parte:
            k, v = parte.split("=", 1)
        else:
            k, v = parte, ""
        if v or keep_blank_values:
            pares.append((unquote_plus(k, encoding), unquote_plus(v, encoding)))
    return pares


def parse_qs(qs, keep_blank_values=False, strict_parsing=False, encoding="utf-8", errors="replace", separator="&"):
    d = {}
    for k, v in parse_qsl(qs, keep_blank_values, strict_parsing, encoding, errors, separator):
        d.setdefault(k, []).append(v)
    return d


class _Resultado:
    """Uma tupla com nomes (como a namedtuple do CPython) e as propriedades
    hostname, port, username e password tiradas do netloc."""
    _campos = ()

    def __init__(self, *valores):
        self._v = tuple(valores)
        for nome, v in zip(self._campos, valores):
            setattr(self, nome, v)

    def __getitem__(self, i):
        return self._v[i]

    def __len__(self):
        return len(self._v)

    def __iter__(self):
        return iter(self._v)

    def __eq__(self, outro):
        return tuple(self) == tuple(outro)

    def __repr__(self):
        return self.__class__.__name__ + "(" + ", ".join(n + "=" + repr(v) for n, v in zip(self._campos, self._v)) + ")"

    def _hospedeiro(self):
        rede = self.netloc
        if "@" in rede:
            rede = rede.rsplit("@", 1)[1]
        if rede.startswith("["):   # IPv6: [::1]:8080
            fim = rede.find("]")
            return rede[1:fim], rede[fim + 1:]
        if ":" in rede:
            h, p = rede.rsplit(":", 1)
            return h, ":" + p
        return rede, ""

    @property
    def hostname(self):
        h = self._hospedeiro()[0]
        return h.lower() if h else None

    @property
    def port(self):
        p = self._hospedeiro()[1]
        if len(p) > 1:
            n = int(p[1:])
            if not 0 <= n <= 65535:
                raise ValueError("Port out of range 0-65535")
            return n
        return None

    @property
    def username(self):
        if "@" in self.netloc:
            return unquote(self.netloc.rsplit("@", 1)[0].split(":", 1)[0])
        return None

    @property
    def password(self):
        if "@" in self.netloc:
            u = self.netloc.rsplit("@", 1)[0]
            if ":" in u:
                return unquote(u.split(":", 1)[1])
        return None

    def geturl(self):
        return urlunsplit(self) if len(self._v) == 5 else urlunparse(self)


class SplitResult(_Resultado):
    _campos = ("scheme", "netloc", "path", "query", "fragment")


class ParseResult(_Resultado):
    _campos = ("scheme", "netloc", "path", "params", "query", "fragment")


def urlsplit(url, scheme="", allow_fragments=True):
    esquema, netloc, query, frag = scheme, "", "", ""
    i = url.find(":")
    if i > 0 and url[:i].replace("+", "").replace("-", "").replace(".", "").isalpha() and url[0].isalpha():
        esquema, url = url[:i].lower(), url[i + 1:]
    if url[:2] == "//":
        fim = len(url)
        for c in "/?#":
            k = url.find(c, 2)
            if k >= 0 and k < fim:
                fim = k
        netloc, url = url[2:fim], url[fim:]
    if allow_fragments and "#" in url:
        url, frag = url.split("#", 1)
    if "?" in url:
        url, query = url.split("?", 1)
    return SplitResult(esquema, netloc, url, query, frag)


def urlparse(url, scheme="", allow_fragments=True):
    s = urlsplit(url, scheme, allow_fragments)
    caminho, params = s.path, ""
    if ";" in caminho:
        caminho, params = caminho.split(";", 1)
    return ParseResult(s.scheme, s.netloc, caminho, params, s.query, s.fragment)


def urlunsplit(partes):
    esquema, netloc, caminho, query, frag = partes
    url = caminho
    if netloc or (esquema and caminho[:2] != "//" and esquema in ("http", "https", "ftp", "file")):
        if url and url[:1] != "/":
            url = "/" + url
        url = "//" + netloc + url
    if esquema:
        url = esquema + ":" + url
    if query:
        url += "?" + query
    if frag:
        url += "#" + frag
    return url


def urlunparse(partes):
    esquema, netloc, caminho, params, query, frag = partes
    if params:
        caminho += ";" + params
    return urlunsplit((esquema, netloc, caminho, query, frag))


def _normalizar(caminho):
    saida = []
    partes = caminho.split("/")
    for p in partes[1:-1] if caminho.startswith("/") else partes[:-1]:
        if p == "..":
            if saida:
                saida.pop()
        elif p not in (".", ""):
            saida.append(p)
    ultimo = partes[-1]
    if ultimo == "..":
        if saida:
            saida.pop()
        ultimo = ""
    elif ultimo == ".":
        ultimo = ""
    return "/" + "/".join(saida + [ultimo])


def urljoin(base, url, allow_fragments=True):
    if not base:
        return url
    if not url:
        return base
    b = urlsplit(base)
    u = urlsplit(url, b.scheme)
    if u.scheme != b.scheme or u.netloc:
        return urlunsplit((u.scheme, u.netloc, _normalizar(u.path) if u.path else u.path, u.query, u.fragment))
    if not u.path:
        return urlunsplit((b.scheme, b.netloc, b.path, u.query or b.query, u.fragment))
    if u.path.startswith("/"):
        caminho = u.path
    else:
        caminho = b.path[:b.path.rfind("/") + 1] + u.path if "/" in b.path else "/" + u.path
    return urlunsplit((b.scheme, b.netloc, _normalizar(caminho), u.query, u.fragment))
