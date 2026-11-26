# bottle_hola.py - una aplicación web con bottle.py sobre Microdot (D478).
#
#   python /ejemplos/web/bottle_hola.py            y abre http://127.0.0.1:8080/ (o el puerto que reenvíes)
#   wget -q -O /tmp/r.txt 'http://127.0.0.1:8080/api/suma?a=2&b=3'; cat /tmp/r.txt
import sys
import bottle
from bottle import route, get, post, request, response, run, template, static_file, redirect, abort

RAIZ = "/ejemplos/web"
bottle.TEMPLATE_PATH.insert(0, RAIZ + "/views/")


@route("/")
def inicio():
    return template("pagina", titulo="Hola desde SaguiOS", items=["Microdot", "bottle.py", "MicroPython"])


@get("/hola/<nombre>")
def hola(nombre):
    return "Hola, %s" % nombre


@get("/api/suma")
def suma():
    a = request.query.get("a", 0, type=int)
    b = request.query.get("b", 0, type=int)
    return {"a": a, "b": b, "suma": a + b}


@get("/formulario")
def formulario():
    return ('<form method="post" action="/formulario"><input name="texto"><input type="submit"></form>')


@post("/formulario")
def recibir():
    return template("Recibido: {{texto}}", texto=request.forms.get("texto", ""))


@post("/api/eco")
def eco():
    response.status = 201
    return {"eco": request.json}


@get("/estaticos/<ruta:path>")
def estaticos(ruta):
    return static_file(ruta, root=RAIZ + "/estaticos")


@get("/viejo")
def viejo():
    redirect("/")


@get("/prohibido")
def prohibido():
    abort(403, "Aquí no")


if __name__ == "__main__":
    puerto = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    run(host="0.0.0.0", port=puerto)
