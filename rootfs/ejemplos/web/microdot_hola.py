# microdot_hola.py - Microdot directamente (D478): python /ejemplos/web/microdot_hola.py [puerto]
import sys
from microdot import Microdot, Response

app = Microdot()


@app.route("/")
async def inicio(request):
    return Response("<h1>Hola desde Microdot en SaguiOS</h1>", headers={"Content-Type": "text/html"})


@app.route("/json")
async def datos(request):
    return {"sistema": "SaguiOS", "lenguaje": "MicroPython"}


@app.post("/suma")
async def suma(request):
    d = request.json
    return {"suma": d["a"] + d["b"]}


app.run(host="0.0.0.0", port=int(sys.argv[1]) if len(sys.argv) > 1 else 5000)
