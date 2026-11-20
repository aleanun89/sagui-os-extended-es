#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H412 - IA local, fase 2 (D380, mejora 139): descargador de modelos, herramientas en el servidor
y visión (proyector de LFM3-VL).

Los modelos reales (LFM Open License) no van a git ni a las imágenes: se usan el LFM3 sintético y
el proyector sintético de tools/llm_modelo_prueba.py (--mmproj), y un servidor HTTPS del anfitrión
con ficheros falsos y su suma. En el PC (sin QEMU):
  1. la plantilla de chat con herramientas, llamadas, respuestas e imágenes que escribe el motor es
     la de la plantilla jinja de LFM3 (con jinja2, si están el modelo real en input_share/modelos y
     jinja2; si no, contra el texto esperado escrito aquí);
  2. la lectura de las llamadas <tool_call> de una respuesta (tipos según el esquema);
En SaguiOS (2 GB, red, un pendrive):
  3. `ia descargar`: lista; descarga por HTTPS (TLS propio) con una redirección a una URL de más de
     1 KB, por trozos (Range), con una conexión cortada a medias (se reintenta y sigue), el fichero
     que va con el modelo (mmproj); suma mala (se borra); sin sitio; una descarga matada a medias que
     sigue después desde donde iba (el servidor ve el Range que no empieza en 0);
  4. Configuración > IA > «IA local: modelos»: la lista, una descarga con la barra y «Usar este
     modelo» (capturas H412-ia-local-*);
  5. la plantilla en SaguiOS es la del PC; el servidor con herramientas (tool_choice forzado) y con
     una imagen en el mensaje contesta lo mismo que el del PC; los vectores de la imagen y los tokens
     de `llm --imagen` son los del PC."""
import base64
import hashlib
import http.client
import http.server
import json
import os
import re
import socket
import ssl
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu, SAIDA, RAIZ, BUILD  # noqa: E402
import conf_arbol as ca  # noqa: E402

TRAB = os.path.join(SAIDA, "h412")
LLM_PC = os.path.join(BUILD, "host", "llm-pc")
MODELOS = os.path.join(RAIZ, "input_share", "modelos")

# ------------------------------------------------------------ ficheros de la prueba


def ppm(w, h):
    """Una imagen sintética: degradado y un círculo."""
    b = bytearray()
    for y in range(h):
        for x in range(w):
            if (x - w // 2) ** 2 + (y - h // 2) ** 2 < (h // 3) ** 2:
                b += bytes((220, 30, 30))
            else:
                b += bytes((40, 80 + y * 100 // h, 200 - x * 120 // w))
    return b"P6\n%d %d\n255\n" % (w, h) + bytes(b)


HERRAMIENTAS = [
    {"type": "function", "function": {"name": "listar_carpeta", "description": "Lista una carpeta",
                                      "parameters": {"type": "object", "properties": {
                                          "ruta": {"type": "string"}, "max": {"type": "integer"}}, "required": ["ruta"]}}},
    {"type": "function", "function": {"name": "leer", "description": "Lee un fichero",
                                      "parameters": {"type": "object", "properties": {"ruta": {"type": "string"}}}}},
]

# Una conversación de agente (D295): sistema, herramientas, una llamada con su respuesta y una imagen.
PET_PLANTILLA = {
    "model": "local", "stream": True,
    "messages": [
        {"role": "system", "content": "Eres un asistente.\nUsa las herramientas."},
        {"role": "user", "content": "Que hay en /home? Usa \"listar\""},
        {"role": "assistant", "content": "", "tool_calls": [{"id": "call_1", "type": "function", "function": {
            "name": "listar_carpeta", "arguments": json.dumps({"ruta": "/home", "ocultos": True, "max": 20, "filtro": {"ext": [".txt"]}})}}]},
        {"role": "tool", "tool_call_id": "call_1", "content": "a.txt\nb.md"},
        {"role": "assistant", "content": "<think>x</think>\n Hay dos ficheros.  "},
        {"role": "user", "content": [{"type": "text", "text": "Mira: "},
                                     {"type": "image_url", "image_url": {"url": "data:image/png;base64,AAAA"}},
                                     {"type": "text", "text": " que es?"}]},
    ],
    "tools": HERRAMIENTAS, "tool_choice": "auto",
}

PLANTILLA_ESPERADA = (
    '<|startoftext|><|im_start|>system\nEres un asistente.\nUsa las herramientas.\n\n<tools>\n'
    '{"name": "listar_carpeta", "description": "Lista una carpeta", "parameters": {"type": "object", "properties": '
    '{"ruta": {"type": "string"}, "max": {"type": "integer"}}, "required": ["ruta"]}}\n'
    '{"name": "leer", "description": "Lee un fichero", "parameters": {"type": "object", "properties": {"ruta": {"type": "string"}}}}\n'
    '</tools><|im_end|>\n<|im_start|>user\nQue hay en /home? Usa "listar"<|im_end|>\n'
    '<|im_start|>assistant\n<think></think><tool_call>\n<function=listar_carpeta>\n'
    '<parameter=ruta>\n/home\n</parameter>\n<parameter=ocultos>\ntrue\n</parameter>\n<parameter=max>\n20\n</parameter>\n'
    '<parameter=filtro>\n{"ext": [".txt"]}\n</parameter>\n</function>\n</tool_call><|im_end|>\n'
    '<|im_start|>tool\n<tool_response>\na.txt\nb.md\n</tool_response><|im_end|>\n'
    '<|im_start|>assistant\n<think></think>Hay dos ficheros.<|im_end|>\n'
    '<|im_start|>user\nMira: <|vision_start|><|image|><|vision_end|> que es?<|im_end|>\n'
    '<|im_start|>assistant\n<think></think>')

RESPUESTA = ('Voy a mirar.\n<tool_call>\n<function=listar_carpeta>\n<parameter=ruta>\n/home/"x"\n</parameter>\n'
             '<parameter=max>\n20\n</parameter>\n<parameter=otro>\nhola mundo\n</parameter>\n</function>\n</tool_call>\n\n'
             '<tool_call>\n<function=leer>\n</function>\n</tool_call>')


def jinja_referencia(pet):
    """La plantilla del GGUF real con jinja2 (como transformers); None si no hay modelo o jinja2."""
    try:
        import jinja2
    except ImportError:
        return None
    gg = [f for f in sorted(os.listdir(MODELOS)) if f.startswith("LFM3") and f.endswith(".gguf")] if os.path.isdir(MODELOS) else []
    if not gg:
        return None
    tpl = gguf_texto(os.path.join(MODELOS, gg[0]), "tokenizer.chat_template")
    if not tpl:
        return None
    tpl = re.sub(r"\{%-?\s*(end)?generation\s*-?%\}", "", tpl)
    env = jinja2.Environment(trim_blocks=True, lstrip_blocks=True, extensions=["jinja2.ext.loopcontrols"])
    env.filters["tojson"] = lambda x, **k: json.dumps(x, ensure_ascii=False)

    def rex(m):
        raise Exception(m)
    env.globals["raise_exception"] = rex
    msgs = []
    for m in pet["messages"]:
        m = json.loads(json.dumps(m))
        for c in m.get("tool_calls") or []:
            a = c["function"].get("arguments")
            if isinstance(a, str):
                c["function"]["arguments"] = json.loads(a) if a.strip() else {}
        if isinstance(m.get("content"), list):
            m["content"] = [{"type": "image"} if p.get("type") in ("image_url", "image") else p for p in m["content"]]
        msgs.append(m)
    return env.from_string(tpl).render(messages=msgs, tools=pet.get("tools"), bos_token="<|startoftext|>",
                                       add_generation_prompt=True, enable_thinking=False)


def gguf_texto(caminho, chave):
    """Un valor de texto de la cabecera de un GGUF (sin numpy)."""
    import struct
    with open(caminho, "rb") as f:
        d = f.read(64 << 20)
    p = 4

    def u(n, fmt):
        nonlocal p
        v = struct.unpack_from(fmt, d, p)[0]
        p += n
        return v

    def txt():
        n = u(8, "<Q")
        nonlocal p
        s = d[p:p + n]
        p += n
        return s.decode("utf-8", "replace")
    tam = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 1, 10: 8, 11: 8, 12: 8}

    def saltar(t):
        nonlocal p
        if t == 8:
            txt()
        elif t == 9:
            st, n = u(4, "<I"), u(8, "<Q")
            for _ in range(n):
                saltar(st)
        else:
            p += tam[t]
    u(4, "<I")
    u(8, "<Q")
    nkv = u(8, "<Q")
    for _ in range(nkv):
        k = txt()
        t = u(4, "<I")
        if k == chave and t == 8:
            return txt()
        saltar(t)
    return None

# ------------------------------------------------------------ servidor HTTPS del anfitrión


def crear_pki():
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.x509.oid import NameOID
    import datetime
    import ipaddress
    ahora = datetime.datetime.now(datetime.timezone.utc)
    ca_k = ec.generate_private_key(ec.SECP256R1())
    nombre = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "CA de prueba h412")])
    cac = (x509.CertificateBuilder().subject_name(nombre).issuer_name(nombre).public_key(ca_k.public_key())
           .serial_number(x509.random_serial_number()).not_valid_before(ahora - datetime.timedelta(days=1))
           .not_valid_after(ahora + datetime.timedelta(days=30))
           .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True)
           .add_extension(x509.KeyUsage(True, False, False, False, False, True, True, False, False), critical=True)
           .sign(ca_k, hashes.SHA256()))
    k = ec.generate_private_key(ec.SECP256R1())
    sn = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "servidor h412")])
    cert = (x509.CertificateBuilder().subject_name(sn).issuer_name(nombre).public_key(k.public_key())
            .serial_number(x509.random_serial_number()).not_valid_before(ahora - datetime.timedelta(days=1))
            .not_valid_after(ahora + datetime.timedelta(days=30))
            .add_extension(x509.SubjectAlternativeName([x509.IPAddress(ipaddress.ip_address("10.0.2.2"))]), critical=False)
            .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
            .sign(ca_k, hashes.SHA256()))
    pem = os.path.join(TRAB, "servidor.pem")
    with open(pem, "wb") as f:
        f.write(cert.public_bytes(serialization.Encoding.PEM))
        f.write(k.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.TraditionalOpenSSL,
                                serialization.NoEncryption()))
    capem = os.path.join(TRAB, "ca.pem")
    with open(capem, "wb") as f:
        f.write(cac.public_bytes(serialization.Encoding.PEM))
    return pem, capem


FICHEROS = {}          # nombre -> bytes
PETICIONES = []        # (nombre, inicio, fin)
ESTADO = {"cortado": False, "lento": True}
FIRMA = "X" * 1400     # la URL firmada de la «CDN» mide más de 1 KB (como las de Hugging Face)


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def do_GET(self):
        ruta, _, consulta = self.path.partition("?")
        if ruta == "/ca.pem":
            d = open(os.path.join(TRAB, "ca.pem"), "rb").read()
            self.send_response(200)
            self.send_header("Content-Length", str(len(d)))
            self.end_headers()
            self.wfile.write(d)
            return
        if ruta.startswith("/resolve/"):   # como huggingface.co/.../resolve/main/X: 302 a la CDN
            self.send_response(302)
            self.send_header("Location", "/cdn/%s?firma=%s" % (ruta[9:], FIRMA))
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        nome = ruta[5:] if ruta.startswith("/cdn/") else None
        if nome not in FICHEROS:
            self.send_response(404)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if nome == "prueba.gguf" and consulta != "firma=" + FIRMA:
            self.send_response(403)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        d = FICHEROS[nome]
        ini, fim = 0, len(d) - 1
        g = re.match(r"bytes=(\d+)-(\d*)", self.headers.get("Range", ""))
        if g:
            ini = int(g.group(1))
            fim = min(int(g.group(2)) if g.group(2) else len(d) - 1, len(d) - 1)
        PETICIONES.append((nome, ini, fim))
        parte = d[ini:fim + 1]
        self.send_response(206 if g else 200)
        if g:
            self.send_header("Content-Range", "bytes %d-%d/%d" % (ini, fim, len(d)))
        self.send_header("Content-Length", str(len(parte)))
        self.send_header("Connection", "close")
        self.end_headers()
        if nome == "prueba.gguf" and ini > 0 and not ESTADO["cortado"]:
            ESTADO["cortado"] = True      # una vez: la conexión se corta a la mitad del trozo
            self.wfile.write(parte[:len(parte) // 2])
            self.wfile.flush()
            self.connection.shutdown(socket.SHUT_RDWR)
            return
        if nome == "lento.gguf" and ESTADO["lento"]:
            try:
                for i in range(0, len(parte), 16384):
                    self.wfile.write(parte[i:i + 16384])
                    self.wfile.flush()
                    time.sleep(0.25)
                    if not ESTADO["lento"]:
                        break
            except (OSError, ssl.SSLError):
                pass   # el cliente se mató a propósito (la descarga a medias)
            return
        self.wfile.write(parte)


def servidor(pem=None):
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    srv.daemon_threads = True
    if pem:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(pem)
        srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv.server_address[1]


def datos(nome, n):
    b = bytearray()
    i = 0
    while len(b) < n:
        b += hashlib.sha256(("%s-%d" % (nome, i)).encode()).digest()
        i += 1
    return bytes(b[:n])


def catalogo(p_https):
    FICHEROS.update({"prueba.gguf": datos("prueba", 1200 * 1024), "mmproj-prueba.gguf": datos("mmproj", 300 * 1024 + 7),
                     "malo.gguf": datos("malo", 400 * 1024), "lento.gguf": datos("lento", 900 * 1024),
                     "gui.gguf": datos("gui", 700 * 1024)})
    base = "https://10.0.2.2:%d" % p_https

    def linha(id_, nome, f, url, extra="", sha=None, n=None, ram=512, nota=""):
        d = FICHEROS.get(f, b"")
        return "|".join([id_, nome, f, str(n if n is not None else len(d)), sha or hashlib.sha256(d).hexdigest(), url, str(ram), extra, nota])
    lin = ["# lista de prueba (h412)",
           linha("prueba", "Modelo de prueba", "prueba.gguf", base + "/resolve/prueba.gguf", "mmproj-prueba", nota="con su proyector"),
           linha("mmproj-prueba", "Proyector de prueba", "mmproj-prueba.gguf", base + "/cdn/mmproj-prueba.gguf"),
           linha("malo", "Suma mala", "malo.gguf", base + "/cdn/malo.gguf", sha="0" * 64),
           linha("lento", "Lento", "lento.gguf", base + "/cdn/lento.gguf"),
           linha("enorme", "No cabe", "enorme.gguf", base + "/cdn/enorme.gguf", sha="pendiente", n=1900000000),
           linha("gui", "Para la ventana", "gui.gguf", base + "/cdn/gui.gguf", ram=999999, nota="pide mucha memoria")]
    return "\n".join(lin) + "\n"

# ------------------------------------------------------------ el motor en el PC


def pc(*args, entrada=None):
    r = subprocess.run([LLM_PC] + list(args), capture_output=True, text=True, errors="replace", input=entrada)
    return r.stdout, r.stderr


def servidor_pc(args):
    """Arranca `llm-pc --servidor` en un puerto libre y devuelve (proceso, puerto)."""
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    porta = s.getsockname()[1]
    s.close()
    p = subprocess.Popen([LLM_PC] + args + ["--servidor", "--puerto", str(porta)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    fim = time.time() + 30
    while time.time() < fim:
        try:
            socket.create_connection(("127.0.0.1", porta), 0.5).close()
            return p, porta
        except OSError:
            time.sleep(0.2)
    p.kill()
    raise AssertionError("llm-pc --servidor no arranca")


def pedir_pc(porta, pet):
    c = http.client.HTTPConnection("127.0.0.1", porta, timeout=120)
    c.request("POST", "/v1/chat/completions", body=json.dumps(pet).encode(), headers={"Content-Type": "application/json"})
    return c.getresponse().read().decode("utf-8", "replace")


def preparar():
    os.makedirs(TRAB, exist_ok=True)
    subprocess.run(["make", "-s", "llm-pc"], cwd=RAIZ, check=True, stdout=subprocess.DEVNULL)
    gen = os.path.join(RAIZ, "tools", "llm_modelo_prueba.py")
    sint, mmp = os.path.join(TRAB, "sint.gguf"), os.path.join(TRAB, "mmproj-sint.gguf")
    subprocess.run([sys.executable, gen, sint], check=True, stdout=subprocess.DEVNULL)
    subprocess.run([sys.executable, gen, mmp, "--mmproj"], check=True, stdout=subprocess.DEVNULL)
    img = ppm(120, 88)
    with open(os.path.join(TRAB, "img.ppm"), "wb") as f:
        f.write(img)
    # 1. la plantilla
    pet = os.path.join(TRAB, "pet_plantilla.json")
    with open(pet, "w") as f:
        json.dump(PET_PLANTILLA, f)
    out, err = pc("--probar-plantilla", pet)
    verificar(out == PLANTILLA_ESPERADA, "la plantilla con herramientas, llamadas, respuestas e imagen es la esperada:\n" + out)
    ref = jinja_referencia(PET_PLANTILLA)
    if ref is None:
        print("(sin el modelo real o sin jinja2: no se compara con la plantilla jinja del GGUF)")
    else:
        verificar(out == ref, "la plantilla es la de la jinja del GGUF real (tokenizer.chat_template)")
    # 2. la respuesta del modelo
    resp = os.path.join(TRAB, "respuesta.txt")
    with open(resp, "w") as f:
        f.write(RESPUESTA)
    out, err = pc("--probar-respuesta", pet, resp)
    lin = dict(l.split(": ", 1) for l in out.strip().splitlines())
    verificar(json.loads(lin["CONTENIDO"]) == "Voy a mirar.", "el texto de antes de las llamadas")
    n, _, ch = lin["LLAMADAS"].partition(" ")
    ch = json.loads(ch)
    verificar(n == "2" and ch[0]["function"]["name"] == "listar_carpeta" and ch[1]["function"]["name"] == "leer",
              "dos llamadas leídas: %s" % lin["LLAMADAS"])
    verificar(json.loads(ch[0]["function"]["arguments"]) == {"ruta": '/home/"x"', "max": 20, "otro": "hola mundo"},
              "argumentos con su tipo (ruta texto, max entero)")
    # 5 (lo del PC). Vectores de la imagen, tokens con --imagen y respuestas del servidor
    base = ["-m", sint, "--mmproj", mmp, "--contexto", "1024", "--ruta", "sse2"]
    out, err = pc(*(base + ["--imagen", os.path.join(TRAB, "img.ppm"), "--volcar-imagen", os.path.join(TRAB, "vec.f32")]))
    suma = re.findall(r"SUMA_IMAGEN: (\S+)", err)
    verificar(suma and "VECTORES:" in out, "el motor del PC codifica la imagen: " + err[-200:])
    out, err = pc(*(base + ["--imagen", os.path.join(TRAB, "img.ppm"), "-p", "que ves", "--temp", "0.8", "--semilla", "7",
                            "-n", "20", "--mostrar-ids"]))
    gerados = re.findall(r"GERADOS:([ 0-9]*)", out)
    verificar(gerados, "el PC genera tokens con --imagen")
    pets = {
        "pet_herr.json": {"model": "x", "max_tokens": 24, "seed": 3, "temperature": 0.8,
                          "messages": [{"role": "user", "content": "hola sagui"}], "tools": HERRAMIENTAS,
                          "tool_choice": {"type": "function", "function": {"name": "leer"}}},
        "pet_img.json": {"model": "x", "max_tokens": 12, "seed": 5, "temperature": 0.8,
                         "messages": [{"role": "user", "content": [
                             {"type": "image_url", "image_url": {"url": "data:image/x-portable-pixmap;base64," + base64.b64encode(img).decode()}},
                             {"type": "text", "text": "que ves"}]}]},
    }
    pets["pet_herr_stream.json"] = dict(pets["pet_herr.json"], stream=True)
    esperado = {}
    proc, porta = servidor_pc(base)
    try:
        for nome in ("pet_herr.json", "pet_herr_stream.json", "pet_img.json"):
            with open(os.path.join(TRAB, nome), "w") as f:
                json.dump(pets[nome], f)
            esperado[nome] = pedir_pc(porta, pets[nome])
    finally:
        proc.kill()
        proc.wait()
    j = json.loads(esperado["pet_herr.json"])
    verificar(j["choices"][0]["finish_reason"] == "tool_calls" and j["choices"][0]["message"]["tool_calls"][0]["function"]["name"] == "leer",
              "con tool_choice forzado, el servidor del PC devuelve la llamada: " + esperado["pet_herr.json"][:300])
    verificar('"tool_calls":[{"index":0' in esperado["pet_herr_stream.json"], "en streaming, la llamada va en un delta con «index»")
    verificar("choices" in esperado["pet_img.json"], "el servidor del PC acepta la imagen en el mensaje")
    return dict(suma=suma[-1], gerados=gerados[-1].split(), esperado=esperado, img=img, pets=pets)

# ------------------------------------------------------------ SaguiOS


def esperar(m, patron, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(patron, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.25)
    raise AssertionError("tiempo agotado esperando /%s/\n%s" % (patron, m.serial()[-2500:]))


def corpo_http(t):
    """El cuerpo de una respuesta HTTP que salió por el terminal."""
    i = t.find("\n{")
    j = t.find("\ndata: ")
    k = min(x for x in (i, j) if x >= 0) if (i >= 0 or j >= 0) else -1
    return t[k + 1:].strip() if k >= 0 else ""


def descargador(m, p_http):
    m.comando("curl -s -o /tmp/ca.pem http://10.0.2.2:%d/ca.pem" % p_http, 60)
    out = m.comando("certificados añadir /tmp/ca.pem", 30)
    verificar("Añadido" in out, "certificados añadir la CA de prueba")
    CAT = "--catalogo /usb/cat.lista --carpeta /tmp/mod"
    out = m.comando("ia descargar %s" % CAT, 30)
    verificar(all(x in out for x in ("prueba", "mmproj-prueba", "malo", "lento", "enorme", "gui")), "ia descargar lista los modelos")
    out = m.comando("env IA_DESCARGA_TROZO=262144 ia descargar prueba %s" % CAT, 240)
    print(out[-800:])
    verificar("Bajado y comprobado: /tmp/mod/prueba.gguf" in out, "prueba (y su proyector) bajados y comprobados")
    verificar("se reintenta" in out, "la conexión cortada a medias se reintenta")
    verificar(any(n == "prueba.gguf" and i > 0 and i % 262144 for n, i, f in PETICIONES),
              "tras el corte se pidió desde la mitad del trozo (Range que no cae en 256 KB)")
    verificar(sum(1 for n, i, f in PETICIONES if n == "prueba.gguf") >= 5, "por trozos de 256 KB")
    verificar(any(n == "mmproj-prueba.gguf" for n, i, f in PETICIONES), "también el fichero que va con el modelo (mmproj)")
    out = m.comando("ls -l /tmp/mod", 30)
    verificar(str(len(FICHEROS["prueba.gguf"])) in out and str(len(FICHEROS["mmproj-prueba.gguf"])) in out and ".parte" not in out,
              "los ficheros tienen su tamaño y no quedan .parte")
    out = m.comando("ia descargar malo %s" % CAT, 120)
    verificar("no coincide" in out, "suma mala: error claro")
    verificar("malo" not in m.comando("ls /tmp/mod", 30), "con la suma mala se borra lo bajado")
    out = m.comando("ia descargar enorme %s" % CAT, 60)
    verificar("no hay sitio" in out, "sin sitio: error antes de bajar nada")
    # una descarga matada a medias sigue después desde donde iba
    n0 = len(m.serial())
    m.comando("ia descargar lento %s &" % CAT, 20)
    pid = esperar(m, r"executar \S*/ia -> processo (\d+)", 20, n0).group(1)
    fim = time.time() + 60
    while time.time() < fim and not any(n == "lento.gguf" for n, i, f in PETICIONES):
        time.sleep(0.5)
    time.sleep(4)
    m.comando("kill %s" % pid, 20)
    time.sleep(1)
    out = m.comando("ia descargar %s" % CAT, 30)
    verificar(re.search(r"lento.*a medias", out), "la lista dice que lento está a medias")
    ESTADO["lento"] = False
    antes = len(PETICIONES)
    out = m.comando("ia descargar lento %s" % CAT, 120)
    verificar("se sigue la descarga a medias" in out and "Bajado y comprobado" in out, "lento sigue y acaba")
    verificar(any(n == "lento.gguf" and i > 0 for n, i, f in PETICIONES[antes:]), "la segunda vez el Range empieza donde se quedó")


def ventana_ialocal(m):
    n = len(m.serial())
    m.comando("env IA_CATALOGO=/usb/cat.lista configuracion --ventana ialocal &", 20)
    esperar(m, r'"Configuraci.n"', 40, n)
    g = esperar(m, r"ia local posiciones filas=(\d+) paso=40 x=(\d+) botones=(\d+)", 30, n)
    time.sleep(1.5)
    v = ca.ventana(m, n)
    filas, x, botones = int(g.group(1)), int(g.group(2)), int(g.group(3))
    m.captura("H412-ia-local-lista")
    # «Para la ventana» es la sexta fila; después «Descargar»
    m.clique(*ca.panel(v, x + 5, filas + 5 * 40 + 6))
    time.sleep(0.5)
    n1 = len(m.serial())
    m.clique(*ca.panel(v, x + 65, botones + 13))
    esperar(m, r"ia local descarga gui -> \d+", 20, n1)
    esperar(m, r"ia local fin \[fin /home/compartido/modelos/gui.gguf\]", 120, n1)
    time.sleep(1)
    m.captura("H412-ia-local-bajado")
    m.clique(*ca.panel(v, x + 280 + 85, botones + 13))
    esperar(m, r"ia local activo gui.gguf", 20, n1)
    time.sleep(1)
    m.captura("H412-ia-local-activo")
    m.clique(*ca.boton_pie(v, "aceptar"))
    time.sleep(1)
    out = m.comando("cat ~/.config/llm/modelo; ls -l /home/compartido/modelos", 20)
    verificar("/home/compartido/modelos/gui.gguf" in out and str(len(FICHEROS["gui.gguf"])) in out,
              "la ventana bajó el modelo y lo dejó como activo")


def motor(m, pc_):
    out = m.comando("llm --probar-plantilla /usb/pet_plantilla.json", 30)
    verificar(PLANTILLA_ESPERADA.replace("\n", "") in out.replace("\n", ""), "la plantilla en SaguiOS es la del PC")
    base = "-m /usb/sint.gguf --mmproj /usb/mmproj-sint.gguf --contexto 1024 --ruta sse2"
    out = m.comando("llm %s --imagen /usb/img.ppm --volcar-imagen x" % base, 120)
    s = re.findall(r"SUMA_IMAGEN: (\S+)", out)
    print("PC: %s  SaguiOS: %s" % (pc_["suma"], s[-1] if s else "?"))
    verificar(s and s[-1] == pc_["suma"], "los vectores de la imagen son los del PC")
    out = m.comando("llm %s --imagen /usb/img.ppm -p 'que ves' --temp 0.8 --semilla 7 -n 20 --mostrar-ids" % base, 180)
    g = re.findall(r"GERADOS:([ 0-9]*)", out)
    verificar(g and g[-1].split() == pc_["gerados"], "los tokens con --imagen son los del PC")
    n0 = len(m.serial())
    m.comando("llm %s --servidor &" % base, 20)
    esperar(m, r"IA local en http://127.0.0.1:8080", 120, n0)
    pid = re.findall(r"executar /apps/bin/llm -> processo (\d+)", m.serial()[n0:])
    for nome in ("pet_herr.json", "pet_herr_stream.json", "pet_img.json"):
        out = m.comando("llm --probar-servidor /usb/%s" % nome, 240)
        c = corpo_http(out)
        e = pc_["esperado"][nome].strip()
        verificar(e.replace("\n", "") in c.replace("\n", ""), "%s: el servidor de SaguiOS contesta lo mismo que el del PC\n%s\n%s" % (nome, c[:400], e[:400]))
    if pid:
        m.comando("kill %s" % pid[-1], 20)


def main():
    pc_ = preparar()
    pem, capem = crear_pki()
    p_https, p_http = servidor(pem), servidor(None)
    arq = {"cat.lista": catalogo(p_https).encode()}
    for nome in ("sint.gguf", "mmproj-sint.gguf", "img.ppm", "pet_plantilla.json", "pet_herr.json", "pet_herr_stream.json", "pet_img.json"):
        with open(os.path.join(TRAB, nome), "rb") as f:
            arq[nome] = f.read()
    pen = criar_pendrive("h412-pen", mb=64, rotulo="IA", arquivos=arq)
    extras = ["-device", "usb-ehci,id=ehci", "-nic", "user,model=e1000"] + pendrive_qemu(pen)
    with Maquina("h412", "ide", memoria="2G", extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        fim = time.time() + 60
        while time.time() < fim and "(DHCP)" not in m.serial():
            time.sleep(0.3)
        out = m.comando("montar usb0")
        verificar("/usb" in out, "el pendrive está montado")
        motor(m, pc_)
        descargador(m, p_http)
        ventana_ialocal(m)
        verificar("EXCECAO" not in m.serial(), "sin excepciones del kernel")
        m.comando("umount /usb")
    print("H412 OK")


if __name__ == "__main__":
    main()
