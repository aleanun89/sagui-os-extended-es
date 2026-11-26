# SPDX-License-Identifier: GPL-3.0-only
# bottle.py - capa pequeña sobre Microdot para escribir como en Bottle (D478).
#
#   from bottle import route, get, post, request, response, run, template, static_file, redirect, abort
#
#   @route("/hola/<nombre>")
#   def hola(nombre):
#       return template("Hola {{nombre}}", nombre=nombre)
#
#   @post("/enviar")
#   def enviar():
#       return {"recibido": request.forms.get("texto")}      # un dict sale como JSON
#
#   run(host="0.0.0.0", port=8080)
#
# Qué hay: route/get/post/put/delete/patch, rutas con <nombre>, <n:int>, <ruta:path>; request (method, path, query, forms,
# params, json, body, headers, get_cookie, cookies, url, remote_addr); response (status, content_type, set_header, set_cookie);
# template (plantillas al estilo SimpleTemplate: {{expr}} escapada, {{!expr}} sin escapar, líneas «% ...» con for/if/elif/else/end
# y «% include("otra", x=1)» no; sin herencia ni «rebase»); static_file; redirect; abort; error(código); HTTPResponse y HTTPError; Bottle()
# para varias aplicaciones; run. Lo que devuelve una ruta: str, bytes, dict o lista (JSON), HTTPResponse, o un iterable de str.
# Qué no hay: hooks, plugins, subir ficheros (request.files), servidores WSGI. Es síncrono por petición, sobre el asyncio de MicroPython.
import json
import os
import re
from microdot import Microdot, Request, Response

__all__ = ["Bottle", "route", "get", "post", "put", "delete", "patch", "request", "response", "run", "template", "static_file",
           "redirect", "abort", "error", "HTTPResponse", "HTTPError", "default_app", "TEMPLATE_PATH", "debug"]

TEMPLATE_PATH = ["./", "./views/"]
_depuracion = False


def debug(valor=True):
    global _depuracion
    _depuracion = valor


_ESTADOS = {200: "OK", 201: "Created", 204: "No Content", 301: "Moved Permanently", 302: "Found", 303: "See Other",
            304: "Not Modified", 400: "Bad Request", 401: "Unauthorized", 403: "Forbidden", 404: "Not Found",
            405: "Method Not Allowed", 409: "Conflict", 410: "Gone", 413: "Payload Too Large", 418: "I'm a teapot",
            500: "Internal Server Error", 501: "Not Implemented", 503: "Service Unavailable"}

_TIPOS = {"html": "text/html", "htm": "text/html", "css": "text/css", "js": "application/javascript",
          "json": "application/json", "txt": "text/plain", "csv": "text/csv", "xml": "application/xml",
          "png": "image/png", "jpg": "image/jpeg", "jpeg": "image/jpeg", "gif": "image/gif", "bmp": "image/bmp",
          "svg": "image/svg+xml", "ico": "image/x-icon", "pdf": "application/pdf", "wav": "audio/wav",
          "mp3": "audio/mpeg", "ogg": "audio/ogg", "mp4": "video/mp4", "zip": "application/zip", "woff2": "font/woff2"}


class HTTPResponse(Exception):
    def __init__(self, body="", status=200, headers=None):
        self.body = body
        self.status = status
        self.headers = dict(headers or {})


class HTTPError(HTTPResponse):
    def __init__(self, status=500, body=None, headers=None):
        HTTPResponse.__init__(self, body, status, headers)


def abort(codigo=500, texto="Unknown Error."):
    raise HTTPError(codigo, texto)


def redirect(url, codigo=None):
    if not codigo:
        codigo = 302
    raise HTTPResponse("", codigo, {"Location": url})


# ---------------------------------------------------------------- request y response (por petición)

class FormsDict:
    """Un MultiDict de Microdot con acceso por atributo (request.forms.nombre), como el FormsDict de Bottle."""

    def __init__(self, md=None):
        self._m = md

    def get(self, clave, defecto=None, type=None):
        if self._m is None:
            return defecto
        v = self._m.get(clave, None)
        if v is None:
            return defecto
        return type(v) if type else v

    def getall(self, clave):
        return self._m.getlist(clave) if self._m is not None else []

    getlist = getall

    def __getitem__(self, clave):
        v = self.get(clave)
        if v is None:
            raise KeyError(clave)
        return v

    def __getattr__(self, nombre):
        v = self.get(nombre)
        return "" if v is None else v

    def __contains__(self, clave):
        return self._m is not None and self._m.get(clave, None) is not None

    def keys(self):
        return list(self._m.keys()) if self._m is not None else []

    def items(self):
        return [(k, self.get(k)) for k in self.keys()]

    def __len__(self):
        return len(self.keys())

    def __iter__(self):
        return iter(self.keys())

    def dict(self):
        return {k: self.get(k) for k in self.keys()}


class _Peticion:
    """El objeto global «request»: apunta a la petición de Microdot que se está atendiendo."""

    def __init__(self):
        self._r = None

    def _atar(self, r):
        self._r = r

    @property
    def method(self):
        return self._r.method

    @property
    def path(self):
        return self._r.path

    @property
    def url(self):
        return self._r.url

    @property
    def query_string(self):
        return self._r.query_string or ""

    @property
    def query(self):
        return FormsDict(self._r.args)

    GET = query

    @property
    def forms(self):
        return FormsDict(self._r.form)

    POST = forms

    @property
    def params(self):
        # la consulta y el formulario juntos, como en Bottle (gana la consulta)
        class _P(FormsDict):
            def __init__(s, a, b):
                FormsDict.__init__(s, a)
                s._b = FormsDict(b)

            def get(s, clave, defecto=None, type=None):
                v = FormsDict.get(s, clave, None)
                if v is None:
                    v = s._b.get(clave, None)
                if v is None:
                    return defecto
                return type(v) if type else v

            def keys(s):
                k = FormsDict.keys(s)
                return k + [x for x in s._b.keys() if x not in k]
        return _P(self._r.args, self._r.form)

    @property
    def json(self):
        return self._r.json

    @property
    def body(self):
        import io
        return io.BytesIO(self._r.body)

    @property
    def headers(self):
        return self._r.headers

    def get_header(self, nombre, defecto=None):
        return self._r.headers.get(nombre, defecto)

    @property
    def cookies(self):
        return self._r.cookies

    def get_cookie(self, nombre, defecto=None):
        return self._r.cookies.get(nombre, defecto)

    @property
    def content_type(self):
        return self._r.content_type

    @property
    def remote_addr(self):
        return self._r.client_addr[0] if self._r.client_addr else None

    environ = {}


class _Respuesta:
    """El objeto global «response»: lo que la ruta fija para la respuesta (estado, tipo, cabeceras, cookies)."""

    def __init__(self):
        self._reiniciar()

    def _reiniciar(self):
        self.status = 200
        self.content_type = None
        self._cab = {}
        self._cookies = []

    @property
    def status_code(self):
        return self.status

    @status_code.setter
    def status_code(self, v):
        self.status = v

    def set_header(self, nombre, valor):
        if nombre.lower() == "content-type":
            self.content_type = valor
        else:
            self._cab[nombre] = valor

    def get_header(self, nombre, defecto=None):
        return self._cab.get(nombre, defecto)

    def set_cookie(self, nombre, valor, **opc):
        self._cookies.append((nombre, valor, opc))


request = _Peticion()
response = _Respuesta()


# ---------------------------------------------------------------- aplicación

def _patron(ruta):
    """/obj/<id:int>/<resto:path> de Bottle -> /obj/<int:id>/<path:resto> de Microdot."""
    def cambia(m):
        nombre, filtro = m.group(1), m.group(2)
        if filtro in ("int", "path"):
            return "<%s:%s>" % (filtro, nombre)
        if filtro == "re" or filtro is None:
            return "<%s>" % nombre
        return "<%s>" % nombre
    return re.sub(r"<([A-Za-z_][A-Za-z0-9_]*)(?::([A-Za-z]+)[^>]*)?>", cambia, ruta)


def _texto_estado(c):
    return _ESTADOS.get(c, "Status %d" % c)


class Bottle:
    def __init__(self):
        self._md = Microdot()
        self._errores = {}
        outer = self

        @self._md.errorhandler(404)
        async def _no_hay(r):
            return outer._respuesta_error(404, None, r)

        @self._md.errorhandler(405)
        async def _metodo(r):
            return outer._respuesta_error(405, None, r)

    # --- errores
    def error(self, codigo=500):
        def deco(f):
            self._errores[codigo] = f
            return f
        return deco

    def _respuesta_error(self, codigo, texto, r=None):
        if r is not None:
            request._atar(r)
        f = self._errores.get(codigo)
        if f is not None:
            e = HTTPError(codigo, texto)
            e.status_line = _texto_estado(codigo)
            try:
                return self._convertir(f(e), codigo)
            except HTTPResponse as x:
                return self._convertir(x.body, x.status, x.headers)
        t = texto if texto else _texto_estado(codigo)
        cuerpo = "<h1>Error: %d %s</h1>\n<p>%s</p>\n" % (codigo, _texto_estado(codigo), _escapar(str(t)))
        return Response(cuerpo, codigo, {"Content-Type": "text/html; charset=UTF-8"})

    # --- conversión de lo que devuelve la ruta
    def _convertir(self, res, estado=None, cab=None):
        cabeceras = dict(response._cab)
        if cab:
            cabeceras.update(cab)
        codigo = estado if estado is not None else response.status
        tipo = response.content_type
        if isinstance(res, Response):
            return res
        if isinstance(res, HTTPError):
            return self._respuesta_error(res.status, res.body, None)
        if isinstance(res, HTTPResponse):
            return self._convertir(res.body, res.status, res.headers)
        if res is None:
            res = ""
        if isinstance(res, (dict, list)):
            cuerpo = json.dumps(res)
            tipo = tipo or "application/json"
        elif isinstance(res, str):
            cuerpo = res
            tipo = tipo or "text/html; charset=UTF-8"
        elif isinstance(res, (bytes, bytearray)):
            cuerpo = bytes(res)
            tipo = tipo or "application/octet-stream"
        else:
            try:
                cuerpo = "".join(x if isinstance(x, str) else x.decode() for x in res)
            except TypeError:
                cuerpo = str(res)
            tipo = tipo or "text/html; charset=UTF-8"
        if tipo and "Content-Type" not in cabeceras:
            if tipo.startswith("text/") and "charset" not in tipo:
                tipo += "; charset=UTF-8"
            cabeceras["Content-Type"] = tipo
        if codigo == 200 and cuerpo == "" and estado is None and response.status == 200:
            pass
        r = Response(cuerpo, codigo, cabeceras)
        if cuerpo == "" and codigo != 200:
            r.body = b""
        for n, v, o in response._cookies:
            r.set_cookie(n, v, **o)
        return r

    # --- rutas
    def route(self, ruta=None, method="GET", callback=None, name=None, **opc):
        metodos = [method] if isinstance(method, str) else list(method)
        metodos = [m.upper() for m in metodos]
        outer = self

        def registrar(fn):
            rutas = ruta if isinstance(ruta, (list, tuple)) else [ruta if ruta is not None else "/" + fn.__name__]
            for p in rutas:
                outer._anadir(p, metodos, fn)
            return fn
        if callback is not None:
            return registrar(callback)
        return registrar

    def _anadir(self, ruta, metodos, fn):
        outer = self

        async def manejador(r, **kw):
            request._atar(r)
            response._reiniciar()
            try:
                res = fn(**kw)
                if hasattr(res, "send") and hasattr(res, "throw") and not hasattr(res, "__next__"):
                    res = await res
                return outer._convertir(res)
            except HTTPError as e:
                return outer._respuesta_error(e.status, e.body, None)
            except HTTPResponse as e:
                return outer._convertir(e.body, e.status, e.headers)
            except Exception as e:
                if _depuracion:
                    import sys
                    sys.print_exception(e)
                return outer._respuesta_error(500, ("%s: %s" % (type(e).__name__, e)) if _depuracion else None, None)
        self._md.route(_patron(ruta), methods=metodos)(manejador)

    def get(self, ruta=None, **o):
        return self.route(ruta, "GET", **o)

    def post(self, ruta=None, **o):
        return self.route(ruta, "POST", **o)

    def put(self, ruta=None, **o):
        return self.route(ruta, "PUT", **o)

    def delete(self, ruta=None, **o):
        return self.route(ruta, "DELETE", **o)

    def patch(self, ruta=None, **o):
        return self.route(ruta, "PATCH", **o)

    def run(self, host="127.0.0.1", port=8080, debug=False, **otros):
        global _depuracion
        if debug:
            _depuracion = True
        self._md.run(host=host, port=port, debug=bool(debug))


_defecto = Bottle()


def default_app():
    return _defecto


def route(ruta=None, method="GET", callback=None, **o):
    return _defecto.route(ruta, method, callback, **o)


def get(ruta=None, **o):
    return _defecto.get(ruta, **o)


def post(ruta=None, **o):
    return _defecto.post(ruta, **o)


def put(ruta=None, **o):
    return _defecto.put(ruta, **o)


def delete(ruta=None, **o):
    return _defecto.delete(ruta, **o)


def patch(ruta=None, **o):
    return _defecto.patch(ruta, **o)


def error(codigo=500):
    return _defecto.error(codigo)


def run(app=None, host="127.0.0.1", port=8080, debug=False, **otros):
    (app or _defecto).run(host=host, port=port, debug=debug)


# ---------------------------------------------------------------- ficheros estáticos

def static_file(nombre, root, mimetype="auto", download=False, charset="UTF-8"):
    if not root.endswith("/"):
        root += "/"
    nombre = nombre.strip("/")
    if ".." in nombre.split("/"):
        return HTTPError(403, "Access denied.")
    ruta = root + nombre
    try:
        st = os.stat(ruta)
        if st[0] & 0x4000:
            return HTTPError(404, "File does not exist.")
        f = open(ruta, "rb")
        datos = f.read()
        f.close()
    except OSError:
        return HTTPError(404, "File does not exist.")
    if mimetype == "auto":
        ext = nombre.rsplit(".", 1)[-1].lower() if "." in nombre else ""
        mimetype = _TIPOS.get(ext, "application/octet-stream")
    cab = {"Content-Type": mimetype + ("; charset=" + charset if mimetype.startswith("text/") else "")}
    if download:
        cab["Content-Disposition"] = 'attachment; filename="%s"' % (nombre.rsplit("/", 1)[-1] if download is True else download)
    # la respuesta es un valor, no una excepción: se devuelve (como en Bottle)
    return HTTPResponse(datos, 200, cab)


# ---------------------------------------------------------------- plantillas (subconjunto de SimpleTemplate)

def _escapar(s):
    return str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;").replace("'", "&#039;")


_ABRE = ("for ", "if ", "while ", "try:", "with ", "def ")
_CONTINUA = ("elif ", "else:", "except", "finally:")
def _empieza(s, prefijos):
    # (el startswith de MicroPython no admite tuplas)
    for p in prefijos:
        if s.startswith(p):
            return True
    return False


_EXPR = re.compile(r"\{\{(.*?)\}\}")


def _compilar(fuente):
    cod = []
    nivel = 0

    def emitir(linea):
        cod.append("    " * nivel + linea)
    for linea in fuente.split("\n"):
        s = linea.strip()
        if s.startswith("%") and not s.startswith("%%"):
            s = s[1:].strip()
            if s == "end":
                nivel -= 1
                continue
            if _empieza(s, _CONTINUA):
                nivel -= 1
                emitir(s)
                nivel += 1
            elif _empieza(s, _ABRE):
                emitir(s)
                nivel += 1
            else:
                emitir(s)
            continue
        if linea.startswith("%%"):
            linea = linea[1:]
        resto = linea
        while True:
            m = _EXPR.search(resto)   # (el re de MicroPython no tiene finditer ni «pos»)
            if m is None:
                break
            if m.start(0) > 0:
                emitir("_w(%r)" % resto[:m.start(0)])
            e = m.group(1).strip()
            if e.startswith("!"):
                emitir("_w(str(%s))" % e[1:].strip())
            else:
                emitir("_w(_esc(%s))" % e)
            resto = resto[m.end(0):]
        if resto.endswith("\\"):
            emitir("_w(%r)" % resto[:-1])
        else:
            emitir("_w(%r)" % (resto + "\n"))
    return "\n".join(cod) + "\n"


_cache = {}


def _buscar(nombre):
    for d in TEMPLATE_PATH:
        for ext in ("", ".tpl", ".html"):
            ruta = (d if d.endswith("/") else d + "/") + nombre + ext
            try:
                f = open(ruta)
            except OSError:
                continue
            t = f.read()
            f.close()
            return t
    raise HTTPError(500, "Template not found: " + nombre)


def template(tpl, *args, **vars):
    if args:
        for a in args:
            if isinstance(a, dict):
                vars.update(a)
    if "\n" in tpl or "{{" in tpl or tpl.lstrip().startswith("%"):
        fuente = tpl
    else:
        fuente = _buscar(tpl)
    codigo = _cache.get(fuente)
    if codigo is None:
        codigo = _compilar(fuente)
        if len(_cache) > 50:
            _cache.clear()
        _cache[fuente] = codigo
    salida = []
    env = dict(vars)
    env["_w"] = salida.append
    env["_esc"] = _escapar
    env["template"] = template
    exec(codigo, env)
    r = "".join(salida)
    # la última línea de la plantilla no añade salto de línea si la fuente no terminaba en uno
    if not fuente.endswith("\n") and r.endswith("\n"):
        r = r[:-1]
    return r
