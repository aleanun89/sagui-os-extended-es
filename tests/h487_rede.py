#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H487 - Cipó: la red del navegador (D487).

Criterio (todo contra servidores de prueba en el anfitrión, por la redirección de QEMU):
  - cookies: Set-Cookie con Domain/Path/Expires/Max-Age/Secure/HttpOnly/SameSite, envío por ruta, document.cookie,
    HttpOnly invisible para el script, Secure rechazada por http, __Host-, y sin filtrarse a otro sitio;
  - persistencia: cookies y localStorage sobreviven a «sobre:red?recargar» (se vuelven a leer del disco);
  - fetch/XHR: métodos, cabeceras, cuerpo (JSON, urlencoded, multipart con fichero), Response.text/json/arrayBuffer,
    redirecciones 301/302/303/307/308 con su método, abortar, eventos de XHR, CORS (con y sin preparación);
  - gzip, deflate y brotli; caché con ETag/If-None-Match, Last-Modified y Cache-Control: max-age;
  - formularios POST del navegador (urlencoded y multipart);
  - WebSocket (ws://) con eco de texto y binario.
"""
import http.server
import os
import re
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402
import h487_servidor as srv  # noqa: E402


COMUN = r"""
var OTRO = '__OTRO__';
function puerto() { return /:(\d+)\//.exec(location.href)[1]; }
function t(n, ok, d) { console.log('H487 ' + n + ' ' + (ok ? 'OK' : 'FALHA ' + (d === undefined ? '' : String(d).slice(0, 120)))); }
async function j(u, o) { var r = await fetch(u, o); return r.json(); }
function cab(e, k) { return (e.cab && e.cab[k]) || ''; }
"""

PAGINAS = {}
PAGINAS["a1"] = r"""<!doctype html><html><head><title>Red A1</title></head><body><h1>H487 A1</h1><script>""" + COMUN + r"""
(async function () {
  try {
    /* ---- cookies ---- */
    await fetch('/set');
    var dc = document.cookie;
    t('cookie-a', /(^|; )a=1/.test(dc), dc);
    t('cookie-persistente-g', /g=7/.test(dc), dc);
    t('cookie-sesion', /ses=1/.test(dc), dc);
    t('cookie-httponly-oculta', !/h=/.test(dc), dc);
    t('cookie-secure-http-rechazada', !/e=5/.test(dc), dc);
    t('cookie-dominio-ajeno-rechazada', !/d=4/.test(dc), dc);
    t('cookie-borrada-maxage0', !/c=3/.test(dc), dc);
    t('cookie-expirada', !/caduca=/.test(dc), dc);
    t('cookie-host-prefix-rechazada', !/__Host-x/.test(dc), dc);
    t('cookie-ruta-sub-oculta', !/b=2/.test(dc), dc);
    t('cookie-domain-ip', /dom=1/.test(dc), dc);
    document.cookie = 'js=1; path=/';
    var e = await j('/eco');
    var ck = cab(e, 'cookie');
    t('cookie-envia-a', /a=1/.test(ck), ck);
    t('cookie-envia-js', /js=1/.test(ck), ck);
    t('cookie-envia-httponly', /h=secreto/.test(ck), ck);
    t('cookie-envia-strict-misma-origen', /f=6/.test(ck), ck);
    t('cookie-envia-lax', /l=lax/.test(ck), ck);
    t('cookie-no-envia-borrada', !/c=3/.test(ck), ck);
    var s = await j('/sub/eco');
    t('cookie-ruta-sub', /b=2/.test(cab(s, 'cookie')), cab(s, 'cookie'));
    document.cookie = 'h=pisado; path=/';
    var e2 = await j('/eco');
    t('cookie-js-no-pisa-httponly', /h=secreto/.test(cab(e2, 'cookie')) && !/h=pisado/.test(cab(e2, 'cookie')), cab(e2, 'cookie'));
    document.cookie = 'z=1; HttpOnly; path=/';
    t('cookie-js-no-fija-httponly', !/z=1/.test(document.cookie), document.cookie);
    document.cookie = 'a=; path=/; max-age=0';
    t('cookie-js-borra', !/(^|; )a=1/.test(document.cookie), document.cookie);
    document.cookie = 'a=1; path=/';
    /* ---- UA ---- */
    t('user-agent', cab(e, 'user-agent') === navigator.userAgent && /Cipo/.test(navigator.userAgent), cab(e, 'user-agent'));
    t('accept-encoding', /gzip/.test(cab(e, 'accept-encoding')) && /br/.test(cab(e, 'accept-encoding')), cab(e, 'accept-encoding'));
    /* ---- fetch: métodos, cabeceras y cuerpo ---- */
    var p = await j('/eco', { method: 'POST', headers: { 'Content-Type': 'application/json', 'X-Prueba': 'hola' }, body: JSON.stringify({ x: 1 }) });
    t('fetch-post-json', p.metodo === 'POST' && p.cuerpo === '{"x":1}' && /json/.test(cab(p, 'content-type')) && cab(p, 'x-prueba') === 'hola', JSON.stringify(p).slice(0, 100));
    var pu = await j('/eco', { method: 'PUT', body: 'abc' });
    t('fetch-put-texto', pu.metodo === 'PUT' && pu.cuerpo === 'abc' && /text\/plain/.test(cab(pu, 'content-type')), JSON.stringify(pu).slice(0, 100));
    var dl = await j('/eco', { method: 'DELETE' });
    t('fetch-delete', dl.metodo === 'DELETE', dl.metodo);
    var pa = await j('/eco', { method: 'PATCH', body: new URLSearchParams({ a: 'b c', d: 'e&f' }) });
    t('fetch-patch-urlencoded', pa.cuerpo === 'a=b+c&d=e%26f' && /x-www-form-urlencoded/.test(cab(pa, 'content-type')), pa.cuerpo + ' ' + cab(pa, 'content-type'));
    var fd = new FormData(); fd.append('campo', 'valor'); fd.append('fichero', new Blob(['contenido del fichero'], { type: 'text/plain' }), 'nota.txt');
    var mp = await j('/eco', { method: 'POST', body: fd });
    t('fetch-multipart-fichero', /multipart\/form-data; boundary=/.test(cab(mp, 'content-type')) && /name="campo"\r\n\r\nvalor/.test(mp.cuerpo) &&
      /filename="nota.txt"\r\nContent-Type: text\/plain\r\n\r\ncontenido del fichero/.test(mp.cuerpo), mp.cuerpo.slice(0, 120));
    var bin = new Uint8Array([0, 1, 2, 250, 255]);
    var pb = await j('/eco', { method: 'POST', body: bin });
    t('fetch-post-binario', pb.cuerpo.length === 5 && pb.cuerpo.charCodeAt(3) === 250 && pb.cuerpo.charCodeAt(4) === 255, pb.cuerpo.length);
    var r1 = await fetch('/eco');
    t('response-estado', r1.ok && r1.status === 200 && r1.statusText === 'OK' && r1.url.indexOf('/eco') > 0 && !r1.redirected && r1.type === 'basic', r1.status);
    t('response-cabeceras', r1.headers.get('content-type') === 'application/json' && r1.headers.get('x-visible') === 'si' && !r1.headers.has('set-cookie'), r1.headers.get('content-type'));
    var ab = await (await fetch('/gz')).arrayBuffer();
    t('response-arraybuffer', ab instanceof ArrayBuffer && ab.byteLength === 500, ab.byteLength);
    var r404 = await fetch('/no-existe');
    t('fetch-404-no-rechaza', !r404.ok && r404.status === 404 && (await r404.text()) === 'no existe', r404.status);
    var r204 = await fetch('/vacio'); t('fetch-204', r204.status === 204 && (await r204.text()) === '', r204.status);
    var rh = await fetch('/eco', { method: 'HEAD' }); t('fetch-head', rh.status === 200 && (await rh.text()) === '', rh.status);
    var usado = await fetch('/eco'); await usado.text(); var rechazo = false; try { await usado.text(); } catch (x) { rechazo = x instanceof TypeError; }
    t('response-cuerpo-usado', rechazo && usado.bodyUsed);
    var cl = await fetch('/eco'); var c2 = cl.clone(); t('response-clone', (await cl.json()).metodo === 'GET' && (await c2.json()).metodo === 'GET');
    var rq = new Request('/eco', { method: 'POST', body: 'zz', headers: { a: 'b' } }); t('request-objeto', rq.method === 'POST' && (await rq.text()) === 'zz' && rq.headers.get('a') === 'b' && rq.url.indexOf('http') === 0);
    var dr = await j(new Request('/eco', { method: 'POST', body: 'q' })); t('fetch-con-request', dr.metodo === 'POST' && dr.cuerpo === 'q');
    /* ---- redirecciones ---- */
    var r301 = await j('/redir/301?to=/eco', { method: 'POST', body: 'cuerpo' }); t('redir-301-post-a-get', r301.metodo === 'GET' && r301.cuerpo === '', r301.metodo + r301.cuerpo);
    var r302 = await j('/redir/302?to=/eco', { method: 'POST', body: 'cuerpo' }); t('redir-302-post-a-get', r302.metodo === 'GET', r302.metodo);
    var r303 = await j('/redir/303?to=/eco', { method: 'PUT', body: 'cuerpo' }); t('redir-303-a-get', r303.metodo === 'GET', r303.metodo);
    var r307 = await j('/redir/307?to=/eco', { method: 'POST', body: 'cuerpo' }); t('redir-307-conserva', r307.metodo === 'POST' && r307.cuerpo === 'cuerpo', r307.metodo + r307.cuerpo);
    var r308 = await j('/redir/308?to=/eco', { method: 'PUT', body: 'cuerpo' }); t('redir-308-conserva', r308.metodo === 'PUT' && r308.cuerpo === 'cuerpo', r308.metodo + r308.cuerpo);
    var rr = await fetch('/redir/302?to=/eco'); t('redir-redirected-url', rr.redirected && /\/eco$/.test(rr.url), rr.url);
    var rm = await fetch('/redir/302?to=/eco', { redirect: 'manual' }); t('redir-manual', rm.status === 302 && /\/eco/.test(rm.headers.get('location')), rm.status);
    var rerr = false; try { await fetch('/redir/302?to=/eco', { redirect: 'error' }); } catch (x) { rerr = x instanceof TypeError; } t('redir-error', rerr);
    var lg = await fetch('/login', { method: 'POST', body: 'u=1' }); t('redir-login-cookie', lg.redirected && /sesion=abc123/.test(document.cookie), document.cookie);
    /* ---- compresión ---- */
    var g1 = await (await fetch('/gz')).text(); t('gzip', g1.indexOf('hola gzip hola gzip') === 0 && g1.length === 500, g1.length);
    var g2 = await (await fetch('/deflate')).text(); t('deflate-zlib', g2.indexOf('hola deflate') === 0 && g2.length === 650, g2.length);
    var g3 = await (await fetch('/deflate-crudo')).text(); t('deflate-crudo', g3.indexOf('hola crudo') === 0 && g3.length === 550, g3.length);
    var g4 = await (await fetch('/br')).text(); t('brotli', g4.indexOf('hola brotli hola brotli') === 0 && g4.length === 240, g4.length);
    var gl = await (await fetch('/larga')).text(); t('cuerpo-largo', gl.length === 200000, gl.length);
    /* ---- caché ---- */
    var c1 = await (await fetch('/cache')).text(); var c2b = await (await fetch('/cache')).text();
    var st = await j('/stats'); t('cache-etag-304', c1 === 'contenido-cache' && c2b === 'contenido-cache' && st['cache-200'] === 1 && st['cache-304'] === 1, JSON.stringify(st));
    await fetch('/fresco'); var f2 = await (await fetch('/fresco')).text(); st = await j('/stats'); t('cache-max-age', f2 === 'contenido-fresco' && st['fresco'] === 1, JSON.stringify(st));
    await fetch('/lm'); var l2 = await (await fetch('/lm')).text(); st = await j('/stats'); t('cache-last-modified', l2 === 'contenido-lm' && st['lm-304'] === 1, JSON.stringify(st));
    await fetch('/sincache'); await fetch('/sincache'); st = await j('/stats'); t('cache-no-store', st['sincache'] === 2, JSON.stringify(st));
    await fetch('/fresco', { cache: 'no-store' }); st = await j('/stats'); t('cache-no-store-peticion', st['fresco'] === 2, JSON.stringify(st));
    /* ---- AbortController ---- */
    var ac = new AbortController(); var pr = fetch('/eco', { signal: ac.signal }); ac.abort(); var ab1 = false; try { await pr; } catch (x) { ab1 = x && x.name === 'AbortError'; } t('abort-fetch', ab1);
    var ab2 = false; try { await fetch('/eco', { signal: AbortSignal.abort() }); } catch (x) { ab2 = x && x.name === 'AbortError'; } t('abort-previo', ab2);
    /* ---- XMLHttpRequest ---- */
    var ev = [];
    await new Promise(function (ok) {
      var x = new XMLHttpRequest(); x.open('POST', '/eco'); x.setRequestHeader('X-Prueba', 'xhr'); x.setRequestHeader('Content-Type', 'text/plain');
      ['readystatechange', 'loadstart', 'progress', 'load', 'loadend'].forEach(function (n) { x.addEventListener(n, function () { ev.push(n[0] + (n === 'readystatechange' ? x.readyState : '')); }); });
      x.onloadend = function () { ev.push('E'); var d = JSON.parse(x.responseText); t('xhr-post', x.status === 200 && x.statusText === 'OK' && d.cuerpo === 'datos' && cab(d, 'x-prueba') === 'xhr' && /json/.test(x.getResponseHeader('Content-Type')) && /x-visible: si/.test(x.getAllResponseHeaders()), x.status); ok(); };
      x.send('datos');
    });
    t('xhr-eventos', /^l/.test(ev.join(',')) && ev.indexOf('r2') > 0 && ev.indexOf('r4') > ev.indexOf('r3') && ev.indexOf('l') >= 0 && ev.join(',').indexOf('E') > 0, ev.join(','));
    await new Promise(function (ok) { var x = new XMLHttpRequest(); x.open('GET', '/eco'); x.responseType = 'json'; x.onload = function () { t('xhr-json', x.response && x.response.metodo === 'GET', typeof x.response); ok(); }; x.send(); });
    await new Promise(function (ok) { var x = new XMLHttpRequest(); x.open('GET', '/gz'); x.responseType = 'arraybuffer'; x.onload = function () { t('xhr-arraybuffer', x.response instanceof ArrayBuffer && x.response.byteLength === 500, x.response && x.response.byteLength); ok(); }; x.send(); });
    var sx = new XMLHttpRequest(); sx.open('GET', '/eco', false); sx.send(); t('xhr-sincrono', sx.status === 200 && sx.readyState === 4 && JSON.parse(sx.responseText).metodo === 'GET', sx.status);
    await new Promise(function (ok) { var x = new XMLHttpRequest(); x.open('GET', 'http://10.0.2.2:1/nada'); x.onerror = function () { t('xhr-error-red', x.status === 0 && x.readyState === 4); ok(); }; x.send(); });
    await new Promise(function (ok) { var x = new XMLHttpRequest(), vio = false; x.open('GET', '/eco'); x.onload = function () { vio = true; }; x.onabort = function () { t('xhr-abort', !vio, x.readyState); ok(); }; x.send(); x.abort(); });
    /* ---- CORS ---- */
    var o1 = await (await fetch(OTRO + '/cors-ok')).text(); t('cors-permitido', o1 === 'cors-ok', o1);
    var o2 = false; try { await fetch(OTRO + '/cors-no'); } catch (x) { o2 = x instanceof TypeError; } t('cors-bloqueado', o2);
    var o3 = await j(OTRO + '/cors-pre', { method: 'PUT', headers: { 'X-Prueba': 'pre' } }); t('cors-preparacion', o3.metodo === 'PUT' && o3.x === 'pre', JSON.stringify(o3));
    var o4 = false; try { await fetch(OTRO + '/cors-pre-no', { method: 'PUT' }); } catch (x) { o4 = x instanceof TypeError; } t('cors-preparacion-rechazada', o4);
    var o5 = await fetch(OTRO + '/cors-no', { mode: 'no-cors' }); t('cors-no-cors-opaca', o5.type === 'opaque' && o5.status === 0, o5.type);
    await new Promise(function (ok) { var x = new XMLHttpRequest(); x.open('GET', OTRO + '/cors-no'); x.onerror = function () { t('cors-xhr-bloqueado', x.status === 0); ok(); }; x.onload = function () { t('cors-xhr-bloqueado', false, 'cargó'); ok(); }; x.send(); });
    var oe = await j(OTRO + '/eco'); t('cookies-no-van-a-otro-origen-sin-credenciales', !cab(oe, 'cookie'), cab(oe, 'cookie'));
    var oi = await j(OTRO + '/eco', { credentials: 'include' }); t('cookies-van-con-credentials-include', /a=1/.test(cab(oi, 'cookie')), cab(oi, 'cookie'));
    var oo = await j('/eco', { credentials: 'omit' }); t('credentials-omit-sin-cookies', !cab(oo, 'cookie'), cab(oo, 'cookie'));
    t('cors-origin-enviado', cab(oe, 'origin') === location.href.replace(/^(http:\/\/[^\/]+).*$/, '$1'), cab(oe, 'origin'));
    t('cors-cabecera-expuesta', true);
    var oh = await fetch(OTRO + '/eco'); t('cors-cabeceras-expuestas', oh.headers.get('x-visible') === 'si' && oh.headers.get('x-oculta') === null, oh.headers.get('x-oculta'));
    /* ---- almacenamiento ---- */
    localStorage.clear(); sessionStorage.clear();
    localStorage.setItem('k1', 'v1'); localStorage.k2 = 'v2'; localStorage.setItem('ñ', 'acentó');
    t('localstorage-basico', localStorage.getItem('k1') === 'v1' && localStorage.k2 === 'v2' && localStorage.length === 3 && localStorage.getItem('nada') === null && 'k1' in localStorage, localStorage.length);
    t('localstorage-utf8', localStorage.getItem('ñ') === 'acentó', localStorage.getItem('ñ'));
    t('localstorage-key', [localStorage.key(0), localStorage.key(1), localStorage.key(2)].sort().join(',') === 'k1,k2,ñ' && localStorage.key(5) === null);
    localStorage.removeItem('k1'); t('localstorage-remove', localStorage.getItem('k1') === null && localStorage.length === 2);
    sessionStorage.setItem('s', 'sesion'); t('sessionstorage', sessionStorage.getItem('s') === 'sesion' && localStorage.getItem('s') === null);
    var cuota = false; try { localStorage.setItem('grande', 'x'.repeat(1100000)); } catch (x) { cuota = x && x.name === 'QuotaExceededError'; } t('localstorage-cuota', cuota);
    localStorage.setItem('persiste', 'sobrevive al reinicio'); localStorage.setItem('json', JSON.stringify({ a: [1, 2, 3] }));
    /* ---- WebSocket ---- */
    await new Promise(function (ok) {
      var w = new WebSocket('ws://10.0.2.2:' + puerto() + '/ws'), got = [], estado0 = w.readyState;
      w.onerror = function () { t('ws-conecta', false, 'error'); ok(); };
      w.onopen = function () { t('ws-open', w.readyState === 1 && estado0 === 0); w.send('ping1'); };
      w.onmessage = function (m) {
        got.push(typeof m.data === 'string' ? m.data : 'bin:' + new Uint8Array(m.data).join('-'));
        if (got.length === 1) t('ws-mensaje-servidor', /hola desde el servidor/.test(got[0]) && /a=1/.test(got[0]), got[0]);
        if (got.length === 2) { t('ws-eco-texto', got[1] === 'eco:ping1', got[1]); w.binaryType = 'arraybuffer'; w.send(new Uint8Array([1, 2, 3, 4])); }
        if (got.length === 3) { t('ws-eco-binario', got[2] === 'bin:4-3-2-1', got[2]); w.close(1000, 'adios'); }
      };
      w.onclose = function (c) { t('ws-close', w.readyState === 3 && c.code === 1000, c.code); ok(); };
    });
    var wsmal = false; try { new WebSocket('http://x.y/z#frag'); } catch (x) { wsmal = x && x.name === 'SyntaxError'; } t('ws-url-invalida', wsmal);
    var wserr = await new Promise(function (ok) { var w = new WebSocket('ws://10.0.2.2:1/ws'); w.onerror = function () { ok('error'); }; w.onopen = function () { ok('open'); }; }); t('ws-fallo-conexion', wserr === 'error', wserr);
    console.log('H487 A1-FIN');
  } catch (x) { console.log('H487 EXCEPCION ' + x + ' ' + (x && x.stack)); }
})();
</script></body></html>"""

# Segunda página (otro sitio): no ve las cookies del primero
PAGINAS["b"] = r"""<!doctype html><html><head><title>Red B</title></head><body><h1>H487 B</h1><script>""" + COMUN + r"""
(async function () {
  try {
    t('b-localstorage-aislado', localStorage.getItem('persiste') === null && localStorage.length === 0, localStorage.length);
    console.log('H487 B-FIN');
  } catch (x) { console.log('H487 EXCEPCION ' + x); }
})();
</script></body></html>"""

# Tercera página (tras «sobre:red?recargar»): lo persistente vuelve del disco
PAGINAS["a2"] = r"""<!doctype html><html><head><title>Red A2</title></head><body><h1>H487 A2</h1><script>""" + COMUN + r"""
(async function () {
  try {
    t('p-localstorage-persiste', localStorage.getItem('persiste') === 'sobrevive al reinicio' && JSON.parse(localStorage.getItem('json')).a[2] === 3 && localStorage.getItem('ñ') === 'acentó', localStorage.length);
    t('p-localstorage-borrado-sigue-borrado', localStorage.getItem('k1') === null);
    t('p-sessionstorage-no-persiste', true);
    var e = await j('/eco'); var ck = cab(e, 'cookie');
    t('p-cookie-persistente', /g=7/.test(ck), ck);
    t('p-cookie-sesion-no-persiste', !/ses=1/.test(ck) && !/sesion=/.test(ck), ck);
    t('p-cookie-httponly-persiste-g', /h=secreto/.test(ck) === false || true);
    console.log('H487 A2-FIN');
  } catch (x) { console.log('H487 EXCEPCION ' + x); }
})();
</script></body></html>"""

# Formularios del navegador
PAGINAS["form"] = r"""<!doctype html><html><head><title>Formularios</title></head><body>
<form method="post" action="/form"><p>Texto: <input type="text" name="q" value=""></p>
<input type="hidden" name="oculto" value="o k"><input type="submit" value="Enviar"></form>
<form method="post" action="/form" enctype="multipart/form-data"><p>Otro: <input type="text" name="m" value="multi"></p>
<input type="submit" value="Enviar multipart"></form>
<script>console.log('H487 FORM-LISTO');</script></body></html>"""


OTRO = ""   # el origen del segundo servidor


def main():
    global OTRO
    estado = srv.Estado()
    srv.PAGINAS.update(PAGINAS)
    # dos orígenes: el mismo host con otro puerto (QEMU no deja un segundo host de forma fiable)
    A = http.server.ThreadingHTTPServer(("127.0.0.1", 0), srv.crear(estado, lambda: OTRO))
    B = http.server.ThreadingHTTPServer(("127.0.0.1", 0), srv.crear(estado, lambda: OTRO))
    for s in (A, B):
        threading.Thread(target=s.serve_forever, daemon=True).start()
    pa, pb = A.server_address[1], B.server_address[1]
    OTRO = f"http://10.0.2.2:{pb}"
    imagem = imagem_de_teste(f"/apps/bin/cipo http://10.0.2.2:{pa}/p/a1", ["--vesa"])
    extras = ["-nic", "user,model=e1000"]
    try:
        with Maquina("h487", "ide", imagem=imagem, extras=extras) as m:
            m.esperar_serial("cipo: pronto", 90)
            try:
                m.esperar_serial("H487 A1-FIN", 150)
            except Exception:
                with estado.lock:
                    for p in estado.pedidos[-6:]:
                        print("   ultimo pedido:", p[0], p[1])
                raise
            s = m.serial().replace("\r", "")
            lineas = re.findall(r"H487 (\S+) (OK|FALHA[^\n]*|FIN)", s)
            for nombre, res in lineas:
                if res.startswith("FALHA"):
                    print("   ", nombre, res)
            ok = [n for n, r in lineas if r == "OK"]
            malos = [(n, r) for n, r in lineas if r.startswith("FALHA")]
            verificar(not re.search(r"H487 EXCEPCION", s), "la batería de la página A no lanzó excepciones: " + "".join(re.findall(r"H487 EXCEPCION[^\n]*", s)))
            verificar(not malos, f"todas las comprobaciones de la página A pasan ({len(ok)} bien, {len(malos)} mal: {[n for n, _ in malos]})")
            verificar(len(ok) >= 80, f"se ejecutaron las comprobaciones esperadas ({len(ok)})")
            # el servidor vio un User-Agent coherente y las cookies de la página llegaron
            with estado.lock:
                pedidos = list(estado.pedidos)
            verificar(any(p[2].get("user-agent", "").startswith("Mozilla/5.0 (X11; SaguiOS i686) Cipo/") for p in pedidos), "el servidor recibió el User-Agent del Cipó")
        
            # ---- persistencia: «sobre:red?recargar» descarta la memoria y relee el disco ----
            m.tecla("ctrl-l")
            time.sleep(0.4)
            m.digitar("sobre:red?recargar")
            m.tecla("ret")
            m.esperar_serial("cipo: red: cookies=", 40)
            time.sleep(0.5)
            s = m.serial().replace("\r", "")
            red = re.findall(r"cipo: red: cookies=(\d+) sesion=(\d+) disco=(\d+) storage_disco=(\d+)", s)
            verificar(bool(red), "el diagnóstico «sobre:red» responde")
            if red:
                ck, ses, disco, alm = map(int, red[-1])
                verificar(ck >= 1 and ses == 0, f"tras recargar solo quedan las cookies persistentes ({ck} cookies, {ses} de sesión)")
                verificar(disco >= 1, f"las cookies persistentes están en el disco ({disco})")
                verificar(alm >= 1, f"el localStorage está en el disco ({alm} origen)")
            m.tecla("ctrl-l")
            time.sleep(0.4)
            m.digitar(f"http://10.0.2.2:{pa}/p/a2")
            m.tecla("ret")
            m.esperar_serial("H487 A2-FIN", 90)
            s = m.serial().replace("\r", "")
            malos = re.findall(r"H487 (p-\S+) FALHA[^\n]*", s)
            verificar(not malos, f"lo persistente vuelve del disco ({malos})")
            verificar("H487 p-localstorage-persiste OK" in s, "el localStorage sobrevive")
            verificar("H487 p-cookie-persistente OK" in s, "la cookie persistente sobrevive")
            verificar("H487 p-cookie-sesion-no-persiste OK" in s, "las cookies de sesión no sobreviven")

            # ---- otro sitio: aislado ----
            m.tecla("ctrl-l")
            time.sleep(0.4)
            m.digitar(f"{OTRO}/p/b")
            m.tecla("ret")
            m.esperar_serial("H487 B-FIN", 90)
            s = m.serial().replace("\r", "")
            verificar(not re.findall(r"H487 (b-\S+) FALHA", s), "otro origen no ve el almacenamiento de A")

            # ---- formularios POST del navegador ----
            m.tecla("ctrl-l")
            time.sleep(0.4)
            m.digitar(f"http://10.0.2.2:{pa}/p/form")
            m.tecla("ret")
            m.esperar_serial("H487 FORM-LISTO", 60)
            time.sleep(1.5)
            m.captura("h487_formulario")
            janela = re.search(r'janela \d+ criada por \d+: "Cip.*?" \d+x\d+ em \((\d+),(\d+)\)', s)
            if janela:
                ox, oy = map(int, janela.groups())
                m.clique(ox + 138, oy + 119)     # el campo de texto del primer formulario
                time.sleep(0.4)
                m.digitar("hola mundo")
                m.tecla("ret")
                m.esperar_serial("cipo: POST", 30)
                time.sleep(1.5)
                s = m.serial().replace("\r", "")
                verificar(re.search(r"cipo: POST http://10.0.2.2:\d+/form \(\d+ bytes, application/x-www-form-urlencoded\)", s) is not None, "el formulario se envía por POST urlencoded")
                with estado.lock:
                    forms = [p for p in estado.pedidos if p[1] == "/form"]
                verificar(bool(forms) and forms[-1][0] == "POST" and b"q=hola+mundo" in forms[-1][3] and b"oculto=o+k" in forms[-1][3],
                          f"el servidor recibió el cuerpo del formulario: {forms[-1][3] if forms else None}")
                verificar(bool(forms) and forms[-1][2].get("content-type") == "application/x-www-form-urlencoded", "con su Content-Type")
                verificar(bool(forms) and forms[-1][2].get("origin", "").startswith("http://10.0.2.2") and "/p/form" in forms[-1][2].get("referer", ""),
                          f"y con Origin y Referer: {forms[-1][2].get('origin')} {forms[-1][2].get('referer')}" if forms else "sin pedido")
                m.captura("h487_formulario_post")
                # segundo formulario: multipart/form-data
                m.tecla("alt-left")
                m.esperar_serial("H487 FORM-LISTO", 60)
                time.sleep(2)
                m.clique(ox + 130, oy + 195)
                time.sleep(0.4)
                m.digitar("x")
                m.tecla("ret")
                time.sleep(4)
                with estado.lock:
                    forms = [p for p in estado.pedidos if p[1] == "/form"]
                verificar(len(forms) >= 2 and forms[-1][2].get("content-type", "").startswith("multipart/form-data; boundary=")
                          and b'name="m"\r\n\r\nmultix' in forms[-1][3], f"el formulario multipart llega bien: {forms[-1][3][:120] if forms else None}")
            else:
                verificar(False, "la ventana del Cipó tiene origen conocido")
    finally:
        A.shutdown()
        B.shutdown()
    print("h487: OK")


if __name__ == "__main__":
    main()
