# Webs con Microdot y bottle.py

MicroPython trae **asyncio** (`import asyncio`), **Microdot** (un servidor web pequeño) y **bottle.py**, una capa encima de Microdot para
escribir como en Bottle. Todo en `/lib/python`; ejemplos en `/ejemplos/web/`.

## Con bottle.py

    from bottle import route, get, post, request, response, run, template, static_file, redirect, abort

    @route("/hola/<nombre>")
    def hola(nombre):
        return template("Hola {{nombre}}", nombre=nombre)

    @post("/enviar")
    def enviar():
        return {"recibido": request.forms.get("texto")}     # un dict sale como JSON

    run(host="0.0.0.0", port=8080)

Ejecútalo con `python app.py &` y abre `http://127.0.0.1:8080/`. Desde el propio SaguiOS: `wget -q -O /tmp/r.txt http://127.0.0.1:8080/hola/mundo; cat /tmp/r.txt`.
Plantillas: `{{x}}` (escapado), `{{!x}}` (sin escapar) y líneas `% for ...:`, `% if ...:`, `% else:`, `% end`; `template("pagina")` busca
`pagina.tpl` en `TEMPLATE_PATH`. Sin hooks, plugins ni subida de ficheros.

## Con Microdot

    from microdot import Microdot
    app = Microdot()
    @app.route("/")
    async def inicio(request):
        return "hola"
    app.run(port=5000)

El resto de Microdot (WebSocket, SSE, formularios multipart, CORS) está en `/lib/python/microdot/`. Licencia MIT.
