/* qjs_rede.js - la API de red del navegador sobre los nativos __sagui_* (D487).
 * Corre una vez por pagina, despues del prelude del puente. Sustituye fetch, XMLHttpRequest,
 * localStorage, sessionStorage y document.cookie por versiones completas, y anade Headers,
 * Request, Response, FormData, Blob, File, AbortController y WebSocket. Texto en ASCII. */
(function (g) {
  'use strict';
  var N = {
    http: g.__sagui_http, enc: g.__sagui_utf8_enc, dec: g.__sagui_utf8_dec, alm: g.__sagui_alm,
    cookie: g.__sagui_cookie, wsAbrir: g.__sagui_ws_abrir, wsEstado: g.__sagui_ws_estado,
    wsEnviar: g.__sagui_ws_enviar, wsFechar: g.__sagui_ws_fechar, wsRecibir: g.__sagui_ws_receber,
    wsLiberar: g.__sagui_ws_liberar, wsProto: g.__sagui_ws_protocolo
  };
  function def(o, n, v) { Object.defineProperty(o, n, { value: v, writable: true, configurable: true, enumerable: false }); }
  function defs(o, m) { for (var k in m) def(o, k, m[k]); }
  function ahora() { return Date.now(); }
  function aviso(e) { try { console.error(String(e && e.stack || e)); } catch (x) {} }

  /* ---- errores ---- */
  if (!g.DOMException) {
    g.DOMException = function DOMException(msg, name) { this.message = msg === undefined ? '' : String(msg); this.name = name === undefined ? 'Error' : String(name); };
    g.DOMException.prototype = Object.create(Error.prototype);
    g.DOMException.prototype.constructor = g.DOMException;
  }
  function abortErr() { return new g.DOMException('The operation was aborted.', 'AbortError'); }

  /* ---- texto y bytes ---- */
  function toU8(p) {
    if (typeof p === 'string') return new Uint8Array(N.enc(p));
    if (p instanceof g.Blob) return p.__b;
    if (p instanceof ArrayBuffer) return new Uint8Array(p);
    if (ArrayBuffer.isView(p)) return new Uint8Array(p.buffer, p.byteOffset, p.byteLength);
    return new Uint8Array(N.enc(String(p)));
  }
  function u8ab(u) { return u.byteOffset === 0 && u.byteLength === u.buffer.byteLength ? u.buffer : u.buffer.slice(u.byteOffset, u.byteOffset + u.byteLength); }
  function dec(u) { return N.dec(u8ab(u)); }
  if (!g.TextEncoder) {
    g.TextEncoder = function TextEncoder() { this.encoding = 'utf-8'; };
    g.TextEncoder.prototype.encode = function (t) { return new Uint8Array(N.enc(t === undefined ? '' : String(t))); };
    g.TextEncoder.prototype.encodeInto = function (t, d) { var e = this.encode(t); var n = Math.min(e.length, d.length); d.set(e.subarray(0, n)); return { read: t.length, written: n }; };
  }
  if (!g.TextDecoder) {
    g.TextDecoder = function TextDecoder(l) { this.encoding = (l || 'utf-8').toLowerCase(); };
    g.TextDecoder.prototype.decode = function (b) {
      if (b === undefined) return '';
      var u = toU8(b);
      if (this.encoding === 'latin1' || this.encoding === 'iso-8859-1' || this.encoding === 'windows-1252') { var s = ''; for (var i = 0; i < u.length; i++) s += String.fromCharCode(u[i]); return s; }
      return dec(u);
    };
  }
  if (!g.btoa) {
    var B64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
    g.btoa = function (s) { s = String(s); var r = ''; for (var i = 0; i < s.length; i += 3) { var a = s.charCodeAt(i), b = s.charCodeAt(i + 1), c = s.charCodeAt(i + 2); if (a > 255 || b > 255 || c > 255) throw new g.DOMException('Invalid character', 'InvalidCharacterError'); var n = a << 16 | (b || 0) << 8 | (c || 0); r += B64.charAt(n >> 18) + B64.charAt(n >> 12 & 63) + (i + 1 < s.length ? B64.charAt(n >> 6 & 63) : '=') + (i + 2 < s.length ? B64.charAt(n & 63) : '='); } return r; };
    g.atob = function (s) { s = String(s).replace(/[\t\n\f\r ]/g, '').replace(/=+$/, ''); var r = '', acc = 0, bits = 0; for (var i = 0; i < s.length; i++) { var v = B64.indexOf(s.charAt(i)); if (v < 0) throw new g.DOMException('Invalid character', 'InvalidCharacterError'); acc = acc << 6 | v; bits += 6; if (bits >= 8) { bits -= 8; r += String.fromCharCode(acc >> bits & 255); } } return r; };
  }

  /* ---- EventTarget sencillo ---- */
  function evento(tipo, props) {
    var e = g.Event ? new g.Event(tipo) : { type: tipo };
    if (props) for (var k in props) e[k] = props[k];
    return e;
  }
  function ET() {}
  defs(ET.prototype, {
    addEventListener: function (t, f) { if (!f) return; var L = this.__L; if (!L) { def(this, '__L', {}); L = this.__L; } var l = L[t] || (L[t] = []); if (l.indexOf(f) < 0) l.push(f); },
    removeEventListener: function (t, f) { var l = (this.__L || {})[t]; if (l) { var i = l.indexOf(f); if (i >= 0) l.splice(i, 1); } },
    dispatchEvent: function (e) {
      try { e.target = this; e.currentTarget = this; } catch (x) {}
      var h = this['on' + e.type];
      if (typeof h === 'function') { try { h.call(this, e); } catch (x) { aviso(x); } }
      var l = (this.__L || {})[e.type];
      if (l) l.slice().forEach(function (f) { try { if (typeof f === 'function') f.call(this, e); else if (f && f.handleEvent) f.handleEvent(e); } catch (x) { aviso(x); } }, this);
      return !e.defaultPrevented;
    }
  });
  function emit(o, tipo, props) { return o.dispatchEvent(evento(tipo, props)); }

  /* ---- URL y URLSearchParams ---- */
  function loc() { return String(g.location && g.location.href || 'about:blank'); }
  function partes(u) { var m = /^([a-zA-Z][a-zA-Z0-9+.\-]*:)\/\/([^\/?#]*)([^?#]*)(\?[^#]*)?(#.*)?$/.exec(u); return m ? { protocolo: m[1].toLowerCase(), host: m[2].toLowerCase(), ruta: m[3] || '/', consulta: m[4] || '', hash: m[5] || '' } : null; }
  function normRuta(r) {
    var seg = r.split('/'), o = [];
    for (var i = 0; i < seg.length; i++) { var s = seg[i]; if (s === '.') { if (i === seg.length - 1) o.push(''); continue; } if (s === '..') { if (o.length > 1) o.pop(); if (i === seg.length - 1) o.push(''); continue; } o.push(s); }
    var p = o.join('/'); return p.charAt(0) === '/' ? p : '/' + p;
  }
  function resolver(u, base) {
    u = String(u).replace(/^\s+|\s+$/g, '');
    if (/^[a-zA-Z][a-zA-Z0-9+.\-]*:/.test(u) && !/^[a-zA-Z][a-zA-Z0-9+.\-]*:\/\//.test(u)) return u;
    var b = partes(String(base === undefined ? loc() : base));
    if (partes(u)) { var p = partes(u); return p.protocolo + '//' + p.host + normRuta(p.ruta) + p.consulta + p.hash; }
    if (!b) return u;
    if (u.indexOf('//') === 0) return b.protocolo + u;
    var ruta, cons = '', hash = '', m = /^([^?#]*)(\?[^#]*)?(#.*)?$/.exec(u);
    if (m[1] === '') { ruta = b.ruta; cons = m[2] === undefined ? (u.charAt(0) === '#' ? b.consulta : '') : m[2]; if (u === '') hash = ''; }
    else if (m[1].charAt(0) === '/') { ruta = m[1]; cons = m[2] || ''; }
    else { ruta = b.ruta.replace(/[^\/]*$/, '') + m[1]; cons = m[2] || ''; }
    hash = m[3] || '';
    return b.protocolo + '//' + b.host + normRuta(ruta) + cons + hash;
  }
  if (!g.URLSearchParams) {
    g.URLSearchParams = function URLSearchParams(q) {
      def(this, '__p', []);
      if (q === undefined || q === null) return;
      if (q instanceof g.URLSearchParams) { this.__p = q.__p.map(function (x) { return [x[0], x[1]]; }); return; }
      if (typeof q === 'object') { if (Array.isArray(q)) q.forEach(function (p) { this.__p.push([String(p[0]), String(p[1])]); }, this); else Object.keys(q).forEach(function (k) { this.__p.push([k, String(q[k])]); }, this); return; }
      q = String(q).replace(/^\?/, '');
      if (q) q.split('&').forEach(function (x) { if (!x) return; var i = x.indexOf('='), k = i < 0 ? x : x.slice(0, i), v = i < 0 ? '' : x.slice(i + 1); this.__p.push([dq(k), dq(v)]); }, this);
    };
    var dq = function (s) { s = s.replace(/\+/g, ' '); try { return decodeURIComponent(s); } catch (e) { return s; } };
    var eq = function (s) { return encodeURIComponent(s).replace(/%20/g, '+').replace(/[!'()~]/g, function (c) { return '%' + c.charCodeAt(0).toString(16).toUpperCase(); }); };
    var P = g.URLSearchParams.prototype;
    defs(P, {
      get: function (k) { for (var i = 0; i < this.__p.length; i++) if (this.__p[i][0] === k) return this.__p[i][1]; return null; },
      getAll: function (k) { return this.__p.filter(function (x) { return x[0] === k; }).map(function (x) { return x[1]; }); },
      set: function (k, v) { var d = false; this.__p = this.__p.filter(function (x) { if (x[0] !== k) return true; if (!d) { x[1] = String(v); d = true; return true; } return false; }); if (!d) this.__p.push([String(k), String(v)]); },
      append: function (k, v) { this.__p.push([String(k), String(v)]); },
      has: function (k) { return this.get(k) !== null; },
      'delete': function (k) { this.__p = this.__p.filter(function (x) { return x[0] !== k; }); },
      sort: function () { this.__p.sort(function (a, b) { return a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0; }); },
      toString: function () { return this.__p.map(function (x) { return eq(x[0]) + '=' + eq(x[1]); }).join('&'); },
      forEach: function (f, t) { this.__p.slice().forEach(function (x) { f.call(t, x[1], x[0], this); }, this); },
      keys: function () { return this.__p.map(function (x) { return x[0]; })[Symbol.iterator](); },
      values: function () { return this.__p.map(function (x) { return x[1]; })[Symbol.iterator](); },
      entries: function () { return this.__p.map(function (x) { return [x[0], x[1]]; })[Symbol.iterator](); }
    });
    P[Symbol.iterator] = P.entries;
    Object.defineProperty(P, 'size', { get: function () { return this.__p.length; }, configurable: true });
  }
  if (!g.URL) {
    g.URL = function URL(u, base) {
      var h = resolver(u, base), p = partes(h);
      if (!p) throw new TypeError("Failed to construct 'URL': Invalid URL");
      var hp = p.host.split(':');
      this.href = h; this.protocol = p.protocolo; this.host = p.host; this.hostname = hp[0]; this.port = hp[1] || '';
      this.pathname = p.ruta; this.search = p.consulta === '?' ? '' : p.consulta; this.hash = p.hash === '#' ? '' : p.hash;
      this.origin = p.protocolo + '//' + p.host; this.username = ''; this.password = '';
      this.searchParams = new g.URLSearchParams(this.search);
    };
    g.URL.prototype.toString = function () { return this.href; };
    g.URL.prototype.toJSON = function () { return this.href; };
    g.URL.canParse = function (u, b) { try { new g.URL(u, b); return true; } catch (e) { return false; } };
    g.URL.createObjectURL = function (b) { var u = 'blob:' + (g.location ? g.location.href.replace(/^(\w+:\/\/[^\/]*).*$/, '$1') : '') + '/' + (++blobN); blobs[u] = b; return u; };
    g.URL.revokeObjectURL = function (u) { delete blobs[u]; };
  }
  var blobN = 0, blobs = {};

  /* ---- Blob, File, FormData ---- */
  g.Blob = function Blob(partes, o) {
    var bs = [], n = 0;
    (partes || []).forEach(function (p) { var b = toU8(p); bs.push(b); n += b.length; });
    var u = new Uint8Array(n), k = 0;
    bs.forEach(function (b) { u.set(b, k); k += b.length; });
    def(this, '__b', u);
    this.size = n; this.type = o && o.type ? String(o.type).toLowerCase() : '';
  };
  defs(g.Blob.prototype, {
    text: function () { return Promise.resolve(dec(this.__b)); },
    arrayBuffer: function () { return Promise.resolve(u8ab(this.__b.slice())); },
    bytes: function () { return Promise.resolve(this.__b.slice()); },
    slice: function (a, b, t) { var x = new g.Blob([], { type: t }); x.__b = this.__b.slice(a, b); x.size = x.__b.length; return x; }
  });
  g.File = function File(partes, nombre, o) { g.Blob.call(this, partes, o); this.name = String(nombre); this.lastModified = o && o.lastModified !== undefined ? o.lastModified : ahora(); };
  g.File.prototype = Object.create(g.Blob.prototype);
  g.File.prototype.constructor = g.File;

  g.FormData = function FormData(form) {
    def(this, '__e', []);
    if (form && form.elements) {
      for (var i = 0; i < form.elements.length; i++) {
        var el = form.elements[i];
        if (!el.name || el.disabled) continue;
        var t = (el.type || '').toLowerCase();
        if ((t === 'checkbox' || t === 'radio') && !el.checked) continue;
        if (t === 'submit' || t === 'button' || t === 'reset' || t === 'file') continue;
        this.__e.push([String(el.name), String(el.value === undefined ? '' : el.value)]);
      }
    }
  };
  function fdval(v, nombre) {
    if (v instanceof g.File) return nombre === undefined ? v : new g.File([v], nombre, { type: v.type });
    if (v instanceof g.Blob) return new g.File([v], nombre === undefined ? 'blob' : nombre, { type: v.type });
    return String(v);
  }
  defs(g.FormData.prototype, {
    append: function (k, v, n) { this.__e.push([String(k), fdval(v, n)]); },
    set: function (k, v, n) { k = String(k); var d = false, nv = fdval(v, n); this.__e = this.__e.filter(function (x) { if (x[0] !== k) return true; if (!d) { x[1] = nv; d = true; return true; } return false; }); if (!d) this.__e.push([k, nv]); },
    get: function (k) { for (var i = 0; i < this.__e.length; i++) if (this.__e[i][0] === k) return this.__e[i][1]; return null; },
    getAll: function (k) { return this.__e.filter(function (x) { return x[0] === k; }).map(function (x) { return x[1]; }); },
    has: function (k) { return this.get(k) !== null; },
    'delete': function (k) { this.__e = this.__e.filter(function (x) { return x[0] !== k; }); },
    forEach: function (f, t) { this.__e.slice().forEach(function (x) { f.call(t, x[1], x[0], this); }, this); },
    keys: function () { return this.__e.map(function (x) { return x[0]; })[Symbol.iterator](); },
    values: function () { return this.__e.map(function (x) { return x[1]; })[Symbol.iterator](); },
    entries: function () { return this.__e.map(function (x) { return [x[0], x[1]]; })[Symbol.iterator](); }
  });
  g.FormData.prototype[Symbol.iterator] = g.FormData.prototype.entries;

  /* multipart/form-data: devuelve { b: Uint8Array, t: 'multipart/...; boundary=...' } */
  function multipart(fd) {
    var bd = '----CipoFormBoundary' + Math.random().toString(16).slice(2) + ahora().toString(16), partes = [], enc = function (s) { return new Uint8Array(N.enc(s)); };
    fd.__e.forEach(function (x) {
      var nombre = x[0].replace(/"/g, '%22').replace(/\r?\n/g, '%0D%0A');
      if (typeof x[1] === 'string') partes.push(enc('--' + bd + '\r\nContent-Disposition: form-data; name="' + nombre + '"\r\n\r\n' + x[1].replace(/\r?\n/g, '\r\n') + '\r\n'));
      else { partes.push(enc('--' + bd + '\r\nContent-Disposition: form-data; name="' + nombre + '"; filename="' + x[1].name.replace(/"/g, '%22') + '"\r\nContent-Type: ' + (x[1].type || 'application/octet-stream') + '\r\n\r\n')); partes.push(x[1].__b); partes.push(enc('\r\n')); }
    });
    partes.push(enc('--' + bd + '--\r\n'));
    var n = 0; partes.forEach(function (p) { n += p.length; });
    var u = new Uint8Array(n), k = 0; partes.forEach(function (p) { u.set(p, k); k += p.length; });
    return { b: u, t: 'multipart/form-data; boundary=' + bd };
  }

  /* ---- Headers ---- */
  g.Headers = function Headers(h) {
    def(this, '__m', {});
    if (!h) return;
    if (h instanceof g.Headers) h.forEach(function (v, k) { this.append(k, v); }, this);
    else if (Array.isArray(h) || (typeof h[Symbol.iterator] === 'function' && typeof h !== 'string')) Array.from(h).forEach(function (p) { this.append(p[0], p[1]); }, this);
    else Object.keys(h).forEach(function (k) { this.append(k, h[k]); }, this);
  };
  function hn(k) { k = String(k); if (!/^[!#$%&'*+\-.^_`|~0-9A-Za-z]+$/.test(k)) throw new TypeError('Invalid header name: ' + k); return k.toLowerCase(); }
  defs(g.Headers.prototype, {
    get: function (k) { var v = this.__m[hn(k)]; return v === undefined ? null : v.join(', '); },
    set: function (k, v) { this.__m[hn(k)] = [String(v).replace(/^\s+|\s+$/g, '')]; },
    append: function (k, v) { k = hn(k); (this.__m[k] = this.__m[k] || []).push(String(v).replace(/^\s+|\s+$/g, '')); },
    has: function (k) { return hn(k) in this.__m; },
    'delete': function (k) { delete this.__m[hn(k)]; },
    forEach: function (f, t) { Object.keys(this.__m).sort().forEach(function (k) { f.call(t, this.__m[k].join(', '), k, this); }, this); },
    keys: function () { return Object.keys(this.__m).sort()[Symbol.iterator](); },
    values: function () { return Object.keys(this.__m).sort().map(function (k) { return this.__m[k].join(', '); }, this)[Symbol.iterator](); },
    entries: function () { return Object.keys(this.__m).sort().map(function (k) { return [k, this.__m[k].join(', ')]; }, this)[Symbol.iterator](); },
    getSetCookie: function () { return []; }
  });
  g.Headers.prototype[Symbol.iterator] = g.Headers.prototype.entries;
  function cabsTexto(h) { var r = ''; h.forEach(function (v, k) { r += k + ': ' + v + '\r\n'; }); return r; }
  function cabsDe(texto) {
    var h = new g.Headers();
    String(texto || '').split(/\r?\n/).forEach(function (l, i) { var p = l.indexOf(':'); if (i > 0 && p > 0) { try { h.append(l.slice(0, p), l.slice(p + 1)); } catch (e) {} } });
    return h;
  }

  /* ---- cuerpos ---- */
  function extraer(b) {
    if (b === undefined || b === null) return { b: null, t: null };
    if (typeof b === 'string') return { b: new Uint8Array(N.enc(b)), t: 'text/plain;charset=UTF-8' };
    if (b instanceof g.URLSearchParams) return { b: new Uint8Array(N.enc(b.toString())), t: 'application/x-www-form-urlencoded;charset=UTF-8' };
    if (b instanceof g.FormData) { var m = multipart(b); return { b: m.b, t: m.t }; }
    if (b instanceof g.Blob) return { b: b.__b, t: b.type || null };
    if (b instanceof ArrayBuffer || ArrayBuffer.isView(b)) return { b: toU8(b).slice(), t: null };
    return { b: new Uint8Array(N.enc(String(b))), t: 'text/plain;charset=UTF-8' };
  }
  var cuerpoMetodos = {
    text: function () { return this.__leer().then(dec); },
    json: function () { return this.__leer().then(function (u) { return JSON.parse(dec(u)); }); },
    arrayBuffer: function () { return this.__leer().then(function (u) { return u8ab(u.slice()); }); },
    bytes: function () { return this.__leer().then(function (u) { return u.slice(); }); },
    blob: function () { var t = this.headers.get('content-type') || ''; return this.__leer().then(function (u) { return new g.Blob([u], { type: t }); }); },
    formData: function () {
      var t = this.headers.get('content-type') || '';
      return this.__leer().then(function (u) {
        var fd = new g.FormData(), s = dec(u);
        if (/application\/x-www-form-urlencoded/i.test(t)) new g.URLSearchParams(s).forEach(function (v, k) { fd.append(k, v); });
        else if (/multipart\/form-data/i.test(t)) {
          var m = /boundary=([^;]+)/i.exec(t); if (!m) throw new TypeError('Could not parse content as FormData.');
          s.split('--' + m[1].replace(/^"|"$/g, '')).forEach(function (p) { var mm = /^\r\nContent-Disposition: form-data; name="([^"]*)"(?:; filename="([^"]*)")?[^\r]*(?:\r\n[^\r]+)*\r\n\r\n([\s\S]*)\r\n$/.exec(p); if (mm) { if (mm[2] !== undefined) fd.append(mm[1], new g.File([mm[3]], mm[2])); else fd.append(mm[1], mm[3]); } });
        } else throw new TypeError('Could not parse content as FormData.');
        return fd;
      });
    }
  };
  function conCuerpo(C) {
    defs(C.prototype, cuerpoMetodos);
    def(C.prototype, '__leer', function () {
      if (this.bodyUsed) return Promise.reject(new TypeError('body stream already read'));
      this.bodyUsed = true;
      return Promise.resolve(this.__b || new Uint8Array(0));
    });
    Object.defineProperty(C.prototype, 'body', { get: function () {
      if (!this.__b) return null;
      var u = this.__b, self = this, dado = false;
      return { locked: false, getReader: function () { return { read: function () { if (dado) return Promise.resolve({ done: true, value: undefined }); dado = true; self.bodyUsed = true; return Promise.resolve({ done: false, value: u.slice() }); }, releaseLock: function () {}, cancel: function () { return Promise.resolve(); }, closed: Promise.resolve() }; }, cancel: function () { return Promise.resolve(); } };
    }, configurable: true });
  }

  /* ---- AbortController ---- */
  if (!g.AbortSignal) {
    g.AbortSignal = function AbortSignal() { this.aborted = false; this.reason = undefined; this.onabort = null; };
    g.AbortSignal.prototype = Object.create(ET.prototype);
    g.AbortSignal.prototype.throwIfAborted = function () { if (this.aborted) throw this.reason; };
    g.AbortSignal.prototype.__abortar = function (r) { if (this.aborted) return; this.aborted = true; this.reason = r === undefined ? abortErr() : r; emit(this, 'abort'); };
    g.AbortSignal.abort = function (r) { var s = new g.AbortSignal(); s.__abortar(r); return s; };
    g.AbortSignal.timeout = function (ms) { var s = new g.AbortSignal(); setTimeout(function () { s.__abortar(new g.DOMException('The operation timed out.', 'TimeoutError')); }, ms); return s; };
    g.AbortSignal.any = function (l) { var s = new g.AbortSignal(); Array.from(l).forEach(function (x) { if (x.aborted) s.__abortar(x.reason); else x.addEventListener('abort', function () { s.__abortar(x.reason); }); }); return s; };
    g.AbortController = function AbortController() { this.signal = new g.AbortSignal(); };
    g.AbortController.prototype.abort = function (r) { this.signal.__abortar(r); };
  }

  /* ---- Request y Response ---- */
  var estados = { 200: 'OK', 201: 'Created', 202: 'Accepted', 204: 'No Content', 206: 'Partial Content', 301: 'Moved Permanently', 302: 'Found', 303: 'See Other', 304: 'Not Modified', 307: 'Temporary Redirect', 308: 'Permanent Redirect', 400: 'Bad Request', 401: 'Unauthorized', 403: 'Forbidden', 404: 'Not Found', 405: 'Method Not Allowed', 409: 'Conflict', 410: 'Gone', 413: 'Payload Too Large', 418: "I'm a teapot", 422: 'Unprocessable Entity', 429: 'Too Many Requests', 500: 'Internal Server Error', 501: 'Not Implemented', 502: 'Bad Gateway', 503: 'Service Unavailable', 504: 'Gateway Timeout' };
  g.Request = function Request(u, o) {
    if (u instanceof g.Request) { var r0 = u; o = Object.assign({ method: r0.method, headers: r0.headers, body: r0.__b, signal: r0.signal, credentials: r0.credentials, mode: r0.mode, redirect: r0.redirect, cache: r0.cache, __t: r0.__t }, o || {}); u = r0.url; }
    o = o || {};
    this.url = resolver(String(u));
    if (!partes(this.url)) throw new TypeError("Failed to construct 'Request': Invalid URL");
    this.method = String(o.method || 'GET').toUpperCase();
    if (this.method === 'CONNECT' || this.method === 'TRACE' || this.method === 'TRACK') throw new TypeError('Forbidden method: ' + this.method);
    this.headers = new g.Headers(o.headers);
    var ex = extraer(o.body);
    if (ex.b && (this.method === 'GET' || this.method === 'HEAD')) throw new TypeError('Request with GET/HEAD method cannot have body.');
    def(this, '__b', ex.b);
    def(this, '__t', ex.t || o.__t || null);
    this.signal = o.signal || new g.AbortSignal();
    this.credentials = o.credentials || 'same-origin';
    this.mode = o.mode || 'cors';
    this.redirect = o.redirect || 'follow';
    this.cache = o.cache || 'default';
    this.referrer = 'about:client'; this.bodyUsed = false; this.keepalive = !!o.keepalive; this.integrity = ''; this.destination = '';
  };
  conCuerpo(g.Request);
  g.Request.prototype.clone = function () { return new g.Request(this); };

  g.Response = function Response(cuerpo, o) {
    o = o || {};
    var ex = extraer(cuerpo);
    def(this, '__b', ex.b || new Uint8Array(0));
    this.status = o.status === undefined ? 200 : o.status | 0;
    if (this.status < 200 || this.status > 599) throw new RangeError("Failed to construct 'Response': The status provided (" + this.status + ') is outside the range [200, 599].');
    this.ok = this.status >= 200 && this.status < 300;
    this.statusText = o.statusText === undefined ? (estados[this.status] || '') : String(o.statusText);
    this.headers = new g.Headers(o.headers);
    if (ex.t && !this.headers.has('content-type')) this.headers.set('content-type', ex.t);
    this.url = o.url || ''; this.type = o.type || 'default'; this.redirected = !!o.redirected; this.bodyUsed = false;
  };
  conCuerpo(g.Response);
  g.Response.prototype.clone = function () {
    if (this.bodyUsed) throw new TypeError('Response: body is already used');
    var r = new g.Response(this.__b.slice(), { status: this.status, statusText: this.statusText, headers: this.headers, url: this.url, type: this.type, redirected: this.redirected });
    return r;
  };
  g.Response.error = function () { var r = new g.Response(null, { status: 200 }); r.status = 0; r.ok = false; r.type = 'error'; return r; };
  g.Response.json = function (d, o) { o = Object.assign({}, o); o.headers = new g.Headers(o.headers); if (!o.headers.has('content-type')) o.headers.set('content-type', 'application/json'); return new g.Response(JSON.stringify(d), o); };
  g.Response.redirect = function (u, s) { return new g.Response(null, { status: s || 302, headers: { location: resolver(String(u)) } }); };

  /* ---- el pedido real ---- */
  function origenDe(u) { var p = partes(u); return p ? p.protocolo + '//' + p.host : ''; }
  /* rq: { metodo, url, cabs: Headers, cuerpo: Uint8Array|null, tipo, cred, redirect, cache, modo } -> [codigo, url, cab, ab, flags] | texto de error */
  function pedir(rq) {
    var h = rq.cabs;
    if (rq.cuerpo && rq.tipo && !h.has('content-type')) h.set('content-type', rq.tipo);
    var flags = 0;
    if (rq.cred === 'include') flags |= 1;
    if (rq.redirect === 'manual' || rq.redirect === 'error') flags |= 2;
    if (rq.cache === 'no-store' || rq.cache === 'reload' || rq.cache === 'no-cache') flags |= 4;
    if (rq.cred === 'omit') flags |= 8;
    if (rq.modo === 'no-cors') flags |= 16;
    var cuerpo = rq.cuerpo && rq.cuerpo.length ? u8ab(rq.cuerpo.slice()) : null;
    try { console.log('rede: ' + rq.metodo + ' ' + rq.url.slice(0, 140)); } catch (e) {}
    return N.http(rq.metodo, rq.url, cabsTexto(h), cuerpo, flags);
  }
  function hacerRespuesta(r, rq) {
    var cab = cabsDe(r[2]);
    var cruzado = origenDe(r[1]) !== origenDe(loc());
    var resp = new g.Response(null, { status: 200 });
    def(resp, '__b', new Uint8Array(r[3]));
    resp.status = r[0]; resp.ok = r[0] >= 200 && r[0] < 300; resp.statusText = estados[r[0]] || '';
    resp.headers = cab; resp.url = r[1]; resp.redirected = (r[4] & 1) !== 0; resp.type = cruzado ? 'cors' : 'basic';
    if (rq.metodo === 'HEAD' || r[0] === 204 || r[0] === 304) def(resp, '__b', new Uint8Array(0));
    return resp;
  }
  g.fetch = function fetch(input, init) {
    return new Promise(function (ok, no) {
      var rq;
      try { rq = new g.Request(input, init); } catch (e) { no(e); return; }
      var sg = rq.signal;
      if (sg && sg.aborted) { no(sg.reason); return; }
      var cancelado = false;
      if (sg && sg.addEventListener) sg.addEventListener('abort', function () { cancelado = true; no(sg.reason); });
      setTimeout(function () {
        if (cancelado) return;
        if (sg && sg.aborted) { no(sg.reason); return; }
        var r = pedir({ metodo: rq.method, url: rq.url, cabs: rq.headers, cuerpo: rq.__b, tipo: rq.__t, cred: rq.credentials, redirect: rq.redirect, cache: rq.cache, modo: rq.mode });
        if (typeof r === 'string') { no(new TypeError('Failed to fetch' + (r === 'cors' ? ' (CORS: ' + rq.url + ' no permite el origen)' : ''))); return; }
        if (rq.redirect === 'error' && r[0] >= 300 && r[0] < 400) { no(new TypeError('Failed to fetch (redirect)')); return; }
        var resp = hacerRespuesta(r, rq);
        if (rq.mode === 'no-cors' && origenDe(rq.url) !== origenDe(loc())) { resp = g.Response.error(); resp.type = 'opaque'; resp.status = 0; }
        ok(resp);
      }, 0);
    });
  };

  /* ---- XMLHttpRequest ---- */
  var X = g.XMLHttpRequest = function XMLHttpRequest() {
    this.readyState = 0; this.status = 0; this.statusText = ''; this.responseURL = ''; this.responseType = ''; this.timeout = 0; this.withCredentials = false; this.responseXML = null;
    def(this, '__h', new g.Headers()); def(this, '__rh', new g.Headers()); def(this, '__txt', ''); def(this, '__ab', null);
    def(this, 'upload', new ET());
  };
  X.prototype = Object.create(ET.prototype);
  X.prototype.constructor = X;
  X.UNSENT = 0; X.OPENED = 1; X.HEADERS_RECEIVED = 2; X.LOADING = 3; X.DONE = 4;
  ['UNSENT', 'OPENED', 'HEADERS_RECEIVED', 'LOADING', 'DONE'].forEach(function (k, i) { X.prototype[k] = i; });
  function estado(x, n) { x.readyState = n; emit(x, 'readystatechange'); }
  function progreso(x, tipo, c, t) { emit(x, tipo, { loaded: c, total: t, lengthComputable: t > 0 }); }
  defs(X.prototype, {
    open: function (m, u, asinc, usr, pw) {
      this.__m = String(m).toUpperCase(); this.__u = resolver(String(u)); this.__async = asinc === undefined ? true : !!asinc;
      this.__h = new g.Headers(); this.__rh = new g.Headers(); this.__enviado = false; this.__abortado = false; this.status = 0; this.statusText = ''; this.responseURL = ''; this.__txt = ''; this.__ab = null;
      if (this.__m === 'CONNECT' || this.__m === 'TRACE' || this.__m === 'TRACK') throw new g.DOMException('Forbidden method', 'SecurityError');
      estado(this, 1);
    },
    setRequestHeader: function (k, v) { if (this.readyState !== 1 || this.__enviado) throw new g.DOMException('The object is in an invalid state.', 'InvalidStateError'); this.__h.append(k, v); },
    getResponseHeader: function (k) { return this.readyState < 2 ? null : this.__rh.get(k); },
    getAllResponseHeaders: function () { if (this.readyState < 2) return ''; var r = ''; this.__rh.forEach(function (v, k) { r += k + ': ' + v + '\r\n'; }); return r; },
    overrideMimeType: function (t) { this.__mime = t; },
    abort: function () {
      this.__abortado = true;
      if (this.readyState === 1 && this.__enviado || this.readyState === 2 || this.readyState === 3) {
        this.readyState = 4; emit(this, 'readystatechange'); progreso(this, 'abort', 0, 0); progreso(this, 'loadend', 0, 0);
      }
      this.readyState = 0; this.status = 0;
    },
    send: function (cuerpo) {
      if (this.readyState !== 1 || this.__enviado) throw new g.DOMException('The object is in an invalid state.', 'InvalidStateError');
      this.__enviado = true; this.__abortado = false;
      var x = this, ex = (this.__m === 'GET' || this.__m === 'HEAD') ? { b: null, t: null } : extraer(cuerpo);
      progreso(x, 'loadstart', 0, 0);
      if (ex.b) progreso(x.upload, 'loadstart', 0, ex.b.length);
      function hacer() {
        if (x.__abortado) return;
        var r = pedir({ metodo: x.__m, url: x.__u, cabs: x.__h, cuerpo: ex.b, tipo: ex.t, cred: x.withCredentials ? 'include' : 'same-origin', redirect: 'follow', cache: 'default', modo: 'cors' });
        if (x.__abortado) return;
        if (typeof r === 'string') {
          x.status = 0; x.statusText = ''; x.readyState = 4; emit(x, 'readystatechange');
          if (ex.b) { progreso(x.upload, 'error', 0, 0); progreso(x.upload, 'loadend', 0, 0); }
          progreso(x, 'error', 0, 0); progreso(x, 'loadend', 0, 0); return;
        }
        x.status = r[0]; x.statusText = estados[r[0]] || ''; x.responseURL = r[1].replace(/#.*$/, ''); x.__rh = cabsDe(r[2]);
        var ab = r[3], total = ab.byteLength;
        if (ex.b) { progreso(x.upload, 'progress', ex.b.length, ex.b.length); progreso(x.upload, 'load', ex.b.length, ex.b.length); progreso(x.upload, 'loadend', ex.b.length, ex.b.length); }
        estado(x, 2);
        if (x.__abortado) return;
        estado(x, 3);
        if (x.__abortado) return;
        x.__ab = ab;
        x.__txt = (x.responseType === '' || x.responseType === 'text') ? N.dec(ab) : '';
        progreso(x, 'progress', total, total);
        x.readyState = 4; emit(x, 'readystatechange');
        progreso(x, 'load', total, total);
        progreso(x, 'loadend', total, total);
      }
      if (this.__async) setTimeout(hacer, 0); else hacer();
    }
  });
  Object.defineProperty(X.prototype, 'responseText', { get: function () { return this.readyState === 4 ? (this.__txt || (this.__ab && (this.responseType === '' || this.responseType === 'text') ? N.dec(this.__ab) : '')) : ''; }, configurable: true });
  Object.defineProperty(X.prototype, 'response', { get: function () {
    if (this.readyState !== 4) return this.responseType === '' || this.responseType === 'text' ? '' : null;
    var ab = this.__ab || new ArrayBuffer(0);
    switch (this.responseType) {
      case 'arraybuffer': return ab;
      case 'blob': return new g.Blob([ab], { type: this.__rh.get('content-type') || '' });
      case 'json': try { return JSON.parse(N.dec(ab)); } catch (e) { return null; }
      case 'document': return null;
      default: return N.dec(ab);
    }
  }, configurable: true });

  /* ---- sendBeacon ---- */
  if (g.navigator) {
    try {
      g.navigator.userAgent = g.__sagui_ua();
      g.navigator.cookieEnabled = true;
      g.navigator.sendBeacon = function (u, d) { try { g.fetch(u, { method: 'POST', body: d === undefined ? null : d, keepalive: true }).catch(function () {}); return true; } catch (e) { return false; } };
    } catch (e) {}
  }

  /* ---- document.cookie ---- */
  if (g.document) {
    Object.defineProperty(g.document, 'cookie', { get: function () { return N.cookie(loc()); }, set: function (v) { N.cookie(loc(), String(v)); }, configurable: true, enumerable: true });
  }

  /* ---- localStorage y sessionStorage por origen ---- */
  function Almacen(area) { def(this, '__a', area); }
  defs(Almacen.prototype, {
    getItem: function (k) { return N.alm(this.__a, 0, String(k)); },
    setItem: function (k, v) {
      var r = N.alm(this.__a, 1, String(k), String(v));
      if (r !== 0) throw new g.DOMException("Failed to execute 'setItem' on 'Storage': Setting the value of '" + k + "' exceeded the quota.", 'QuotaExceededError');
    },
    removeItem: function (k) { N.alm(this.__a, 2, String(k)); },
    clear: function () { N.alm(this.__a, 3); },
    key: function (i) { return N.alm(this.__a, 4, i | 0); }
  });
  Object.defineProperty(Almacen.prototype, 'length', { get: function () { return N.alm(this.__a, 5); }, configurable: true });
  function proxyAlm(area) {
    return new Proxy(new Almacen(area), {
      get: function (t, k, r) { if (typeof k === 'symbol' || k in t) return Reflect.get(t, k, r); var v = t.getItem(k); return v === null ? undefined : v; },
      set: function (t, k, v) { if (typeof k === 'symbol' || k in Almacen.prototype) return Reflect.set(t, k, v); t.setItem(k, v); return true; },
      has: function (t, k) { return k in t || (typeof k === 'string' && t.getItem(k) !== null); },
      deleteProperty: function (t, k) { t.removeItem(k); return true; },
      ownKeys: function (t) { var r = [], n = t.length; for (var i = 0; i < n; i++) r.push(t.key(i)); return r; },
      getOwnPropertyDescriptor: function (t, k) { if (typeof k !== 'string') return undefined; var v = t.getItem(k); return v === null ? undefined : { value: v, writable: true, enumerable: true, configurable: true }; }
    });
  }
  var loc_ = proxyAlm(0), ses_ = proxyAlm(1);
  Object.defineProperty(g, 'localStorage', { get: function () { return loc_; }, configurable: true, enumerable: true });
  Object.defineProperty(g, 'sessionStorage', { get: function () { return ses_; }, configurable: true, enumerable: true });
  g.Storage = Almacen;

  /* ---- WebSocket ---- */
  var vivos = [];
  var W = g.WebSocket = function WebSocket(url, protos) {
    var u = String(url), m = /^(wss?):\/\//i.exec(resolver(u));
    var r = resolver(u);
    if (/^https?:\/\//i.test(r)) r = r.replace(/^http/i, 'ws');
    if (!/^wss?:\/\//i.test(r)) throw new g.DOMException("Failed to construct 'WebSocket': The URL '" + u + "' is invalid.", 'SyntaxError');
    if (/#/.test(r)) throw new g.DOMException("Failed to construct 'WebSocket': The URL contains a fragment identifier.", 'SyntaxError');
    this.url = r; this.readyState = 0; this.bufferedAmount = 0; this.extensions = ''; this.protocol = ''; this.binaryType = 'blob';
    this.onopen = this.onmessage = this.onclose = this.onerror = null;
    def(this, '__id', -1);
    var pl = protos === undefined ? '' : Array.isArray(protos) ? protos.join(', ') : String(protos), self = this;
    vivos.push(this);
    setTimeout(function () {
      if (self.readyState !== 0) return;
      var id = N.wsAbrir(r, pl);
      if (typeof id === 'string') { cerrarWS(self, 1006, '', false, true); return; }
      self.__id = id; self.readyState = 1; self.protocol = N.wsProto(id) || '';
      emit(self, 'open');
    }, 0);
  };
  W.prototype = Object.create(ET.prototype);
  W.prototype.constructor = W;
  W.CONNECTING = 0; W.OPEN = 1; W.CLOSING = 2; W.CLOSED = 3;
  ['CONNECTING', 'OPEN', 'CLOSING', 'CLOSED'].forEach(function (k, i) { W.prototype[k] = i; });
  function cerrarWS(w, cod, razon, limpio, error) {
    if (w.readyState === 3) return;
    w.readyState = 3;
    if (w.__id >= 0) { N.wsLiberar(w.__id); }
    var i = vivos.indexOf(w); if (i >= 0) vivos.splice(i, 1);
    if (error) emit(w, 'error');
    emit(w, 'close', { code: cod, reason: razon || '', wasClean: !!limpio });
  }
  defs(W.prototype, {
    send: function (d) {
      if (this.readyState === 0) throw new g.DOMException("Failed to execute 'send' on 'WebSocket': Still in CONNECTING state.", 'InvalidStateError');
      if (this.readyState !== 1) return;
      if (typeof d === 'string') N.wsEnviar(this.__id, d, 0);
      else N.wsEnviar(this.__id, u8ab(toU8(d).slice()), 1);
    },
    close: function (cod, razon) {
      if (cod !== undefined && cod !== 1000 && !(cod >= 3000 && cod <= 4999)) throw new g.DOMException('The close code must be either 1000, or between 3000 and 4999.', 'InvalidAccessError');
      if (this.readyState === 3 || this.readyState === 2) return;
      if (this.readyState === 0) { cerrarWS(this, 1006, '', false, true); return; }
      this.readyState = 2;
      N.wsFechar(this.__id, cod === undefined ? 1000 : cod, razon === undefined ? '' : String(razon));
    }
  });
  /* El navegador llama a esto desde el bucle principal mientras haya un WebSocket abierto. */
  g.__sagui_ws_poll = function () {
    vivos.slice().forEach(function (w) {
      if (w.__id < 0) return;
      for (var k = 0; k < 32; k++) {
        var m = N.wsRecibir(w.__id);
        if (m === null) break;
        if (m[0] === 8) { cerrarWS(w, m[2], '', w.readyState === 2 || m[2] === 1000 || m[2] === 1005 || m[2] === 1001, m[2] === 1006); break; }
        var d = m[1];
        if (m[0] === 2) d = w.binaryType === 'arraybuffer' ? d : new g.Blob([d]);
        emit(w, 'message', { data: d, origin: origenDe(w.url.replace(/^ws/i, 'http')), lastEventId: '', source: null, ports: [] });
      }
    });
  };
  g.__sagui_ws_vivos = function () { return vivos.length; };
})(globalThis);
