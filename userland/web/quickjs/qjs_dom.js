// SPDX-License-Identifier: GPL-2.0-or-later
/* qjs_dom.js - el DOM de las páginas del Cipó, en JavaScript (D488).
 *
 * Corre una vez al crear el motor de la página, después del prelude
 * (qjs_prelude.h). El árbol de verdad lo tiene el motor de la web (motor.c);
 * aquí hay una copia en objetos JavaScript, con los mismos identificadores
 * (D160), y cada cambio que hace el script se le pasa al motor por los nativos
 * de __sagui_nat (qjs_bridge.c), que llaman a los ganchos del navegador:
 *
 *   criar(tag, texto) -> id      nodo suelto nuevo en el motor
 *   inserir(pai, no, antes)      lo mete (o mueve) antes de «antes» (-1: al final)
 *   tirar(no)                    lo deja suelto
 *   dado(no, texto)              el texto de un nodo de texto
 *   html(no, html) -> lista      innerHTML: el motor lee el HTML y devuelve los nodos nuevos
 *   atr(no, nombre, valor|null)  un atributo
 *   caixa(no) -> [16]            la caja (el navegador maqueta antes si hay cambios pendientes)
 *   estilo(no, prop) -> texto    getComputedStyle
 *   em(x, y) -> id               elementFromPoint (coordenadas de la ventana)
 *   janela() -> [6]              innerWidth, innerHeight, scrollX, scrollY, ancho y alto de la página
 *   rolar(x, y)                  scrollTo de la página
 *   historia(url, sustituir)     pushState / replaceState: cambia la dirección sin navegar
 *   ir(delta)                    history.back / forward / go
 *   quer(tipo)                   avisa al navegador de que hay oyentes de ratón o de teclado
 *   navegar(url)                 location = url (otra página)
 *   temporizador(fn, ms, rep, args) -> id, parar(id), quadro(fn) -> id, sem_quadro(id)
 *
 * El navegador manda los eventos con __sagui_evento(id, tipo, datos) y la lista
 * de nodos con __sagui_montar(texto). El formato de la lista está en qjs_bridge.h.
 */
(function (g) {
  'use strict';
  var N = g.__sagui_nat, doc = g.document, nos = [], O = Object, defP = O.defineProperty;
  function def(o, n, get, set) { defP(o, n, { get: get, set: set, configurable: true, enumerable: true }); }
  function oculto(o, n, v) { defP(o, n, { value: v, writable: true, configurable: true, enumerable: false }); }
  function erro(onde, x) { console.log('error en ' + onde + ': ' + (x && x.stack ? x + ' ' + x.stack.split('\n')[0] : x)); }
  function DOMException(msg, nome) { this.message = msg; this.name = nome || 'Error'; }
  DOMException.prototype = O.create(Error.prototype);
  g.DOMException = DOMException;

  /* ---------------------------------------------------------------- EventTarget */
  function EventTarget() {}
  var ET = EventTarget.prototype;
  function opcoes(o) { return typeof o === 'object' && o ? { cap: !!o.capture, once: !!o.once, passive: !!o.passive, sinal: o.signal } : { cap: !!o, once: false, passive: false }; }
  var RATO = { mousemove: 1, mouseover: 1, mouseout: 1, mouseenter: 1, mouseleave: 1, pointermove: 1, pointerover: 1, pointerout: 1 };
  var TECLA = { keydown: 1, keyup: 1, keypress: 1 };
  function avisar(t) { if (RATO[t]) N.quer('rato'); else if (TECLA[t]) N.quer('tecla'); }
  ET.addEventListener = function (t, f, o) {
    var e = this == null ? g : this;
    if (!f || (typeof f !== 'function' && typeof f.handleEvent !== 'function')) return;
    t = String(t); var op = opcoes(o);
    if (!e._ouv) oculto(e, '_ouv', {});
    var L = e._ouv[t] || (e._ouv[t] = []);
    for (var i = 0; i < L.length; i++) if (L[i].f === f && L[i].cap === op.cap) return;
    var reg = { f: f, cap: op.cap, once: op.once, passive: op.passive, fora: false };
    L.push(reg);
    if (op.sinal && typeof op.sinal.addEventListener === 'function') op.sinal.addEventListener('abort', function () { e.removeEventListener(t, f, op.cap); });
    avisar(t);
  };
  ET.removeEventListener = function (t, f, o) {
    var e = this == null ? g : this, L = e._ouv && e._ouv[String(t)], cap = opcoes(o).cap;
    if (!L) return;
    for (var i = 0; i < L.length; i++) if (L[i].f === f && L[i].cap === cap) { L[i].fora = true; L.splice(i, 1); return; }
  };
  ET.dispatchEvent = function (ev) {
    var e = this == null ? g : this;
    if (!ev || typeof ev.type !== 'string') throw new TypeError('dispatchEvent: no es un evento');
    ev.isTrusted = false;
    return despachar(e, ev);
  };
  /* El manejador onX: la propiedad, o el atributo onX="..." (compilado una vez) */
  function manejador(no, t) {
    var h = no['on' + t];
    if (typeof h === 'function') return h;
    if (no._atr && no._atr['on' + t] !== undefined) {
      if (!no._inl) oculto(no, '_inl', {});
      var c = no._inl[t];
      if (!c || c.src !== no._atr['on' + t]) {
        try { c = { src: no._atr['on' + t], f: new Function('event', no._atr['on' + t]) }; } catch (x) { erro('on' + t, x); c = { src: no._atr['on' + t], f: null }; }
        no._inl[t] = c;
      }
      return c.f;
    }
    return null;
  }
  function invocar(no, ev, cap) {
    ev.currentTarget = no;
    var L = no._ouv && no._ouv[ev.type];
    if (L && L.length) {
      L = L.slice();
      for (var i = 0; i < L.length; i++) {
        var l = L[i];
        if (l.cap !== cap || l.fora) continue;
        if (l.once) no.removeEventListener(ev.type, l.f, l.cap);
        ev._passivo = l.passive;
        try { if (typeof l.f === 'function') l.f.call(no, ev); else l.f.handleEvent(ev); } catch (x) { erro(ev.type, x); }
        ev._passivo = false;
        if (ev._pararJa) return;
      }
    }
    if (!cap) {
      var h = manejador(no, ev.type);
      if (h) { var r; try { r = h.call(no, ev); } catch (x) { erro('on' + ev.type, x); } if (r === false) ev.preventDefault(); }
    }
  }
  function caminho(alvo) {
    var p = [];
    for (var n = alvo; n; n = n.parentNode) p.push(n);
    if (p.length && p[p.length - 1] === doc && alvo !== g) p.push(g);
    return p;
  }
  function despachar(alvo, ev) {
    ev.target = alvo; ev._caminho = caminho(alvo);
    var p = ev._caminho, i;
    ev.eventPhase = 1;
    for (i = p.length - 1; i > 0 && !ev._parar; i--) invocar(p[i], ev, true);
    ev.eventPhase = 2;
    if (!ev._parar) invocar(alvo, ev, true);
    if (!ev._parar) invocar(alvo, ev, false);
    if (ev.bubbles) { ev.eventPhase = 3; for (i = 1; i < p.length && !ev._parar; i++) invocar(p[i], ev, false); }
    ev.eventPhase = 0; ev.currentTarget = null;
    return !ev.defaultPrevented;
  }
  g.EventTarget = EventTarget;
  ['addEventListener', 'removeEventListener', 'dispatchEvent'].forEach(function (k) { g[k] = ET[k]; doc[k] = ET[k]; });

  /* ---------------------------------------------------------------- eventos */
  var t0 = Date.now();
  function Event(t, o) {
    if (!(this instanceof Event)) throw new TypeError("Event: falta 'new'");
    o = o || {};
    this.type = String(t); this.bubbles = !!o.bubbles; this.cancelable = !!o.cancelable; this.composed = !!o.composed;
    this.defaultPrevented = false; this.target = null; this.currentTarget = null; this.eventPhase = 0;
    this.timeStamp = Date.now() - t0; this.isTrusted = false;
    oculto(this, '_parar', false); oculto(this, '_pararJa', false); oculto(this, '_passivo', false); oculto(this, '_caminho', []);
  }
  Event.NONE = 0; Event.CAPTURING_PHASE = 1; Event.AT_TARGET = 2; Event.BUBBLING_PHASE = 3;
  var EP = Event.prototype;
  EP.preventDefault = function () { if (this.cancelable && !this._passivo) this.defaultPrevented = true; };
  EP.stopPropagation = function () { this._parar = true; };
  EP.stopImmediatePropagation = function () { this._parar = true; this._pararJa = true; };
  EP.composedPath = function () { return this._caminho.slice(); };
  EP.initEvent = function (t, b, c) { this.type = String(t); this.bubbles = !!b; this.cancelable = !!c; };
  def(EP, 'returnValue', function () { return !this.defaultPrevented; }, function (v) { if (!v) this.preventDefault(); });
  def(EP, 'cancelBubble', function () { return this._parar; }, function (v) { if (v) this._parar = true; });
  def(EP, 'srcElement', function () { return this.target; });
  function sub(nome, campos) {
    var C = function (t, o) { Event.call(this, t, o); o = o || {}; for (var k in campos) this[k] = o[k] !== undefined ? o[k] : campos[k]; };
    C.prototype = O.create(EP); C.prototype.constructor = C;
    g[nome] = C;
    return C;
  }
  sub('CustomEvent', { detail: null });
  g.CustomEvent.prototype.initCustomEvent = function (t, b, c, d) { this.initEvent(t, b, c); this.detail = d; };
  var UI = { view: null, detail: 0 };
  var RAT = { view: null, detail: 0, x: 0, y: 0, screenX: 0, screenY: 0, clientX: 0, clientY: 0, pageX: 0, pageY: 0, offsetX: 0, offsetY: 0, movementX: 0, movementY: 0,
              button: 0, buttons: 0, ctrlKey: false, shiftKey: false, altKey: false, metaKey: false, relatedTarget: null };
  sub('UIEvent', UI); sub('MouseEvent', RAT); sub('PointerEvent', RAT); sub('WheelEvent', RAT);
  g.PointerEvent.prototype.pointerId = 1; g.PointerEvent.prototype.pointerType = 'mouse'; g.PointerEvent.prototype.isPrimary = true;
  g.MouseEvent.prototype.getModifierState = function (k) { return k === 'Shift' ? this.shiftKey : k === 'Control' ? this.ctrlKey : k === 'Alt' ? this.altKey : false; };
  sub('KeyboardEvent', { view: null, key: '', code: '', keyCode: 0, which: 0, charCode: 0, location: 0, repeat: false, isComposing: false,
                         ctrlKey: false, shiftKey: false, altKey: false, metaKey: false });
  g.KeyboardEvent.prototype.getModifierState = g.MouseEvent.prototype.getModifierState;
  sub('FocusEvent', { view: null, detail: 0, relatedTarget: null });
  sub('InputEvent', { view: null, detail: 0, data: null, inputType: '', isComposing: false });
  sub('PopStateEvent', { state: null });
  sub('HashChangeEvent', { oldURL: '', newURL: '' });
  sub('SubmitEvent', { submitter: null });
  sub('ProgressEvent', { lengthComputable: false, loaded: 0, total: 0 });
  sub('ErrorEvent', { message: '', filename: '', lineno: 0, colno: 0, error: null });
  sub('StorageEvent', { key: null, oldValue: null, newValue: null, url: '', storageArea: null });
  sub('AnimationEvent', { animationName: '', elapsedTime: 0 }); sub('TransitionEvent', { propertyName: '', elapsedTime: 0 });
  g.Event = Event;
  doc.createEvent = function (t) { t = String(t); var C = g[t.replace(/s$/, '')] || Event; return new C(''); };

  /* ---------------------------------------------------------------- nodos */
  function Node() { throw new TypeError('Illegal constructor'); }
  Node.prototype = O.create(ET);
  Node.ELEMENT_NODE = 1; Node.TEXT_NODE = 3; Node.COMMENT_NODE = 8; Node.DOCUMENT_NODE = 9; Node.DOCUMENT_FRAGMENT_NODE = 11;
  Node.DOCUMENT_POSITION_DISCONNECTED = 1; Node.DOCUMENT_POSITION_PRECEDING = 2; Node.DOCUMENT_POSITION_FOLLOWING = 4;
  Node.DOCUMENT_POSITION_CONTAINS = 8; Node.DOCUMENT_POSITION_CONTAINED_BY = 16;
  var NP = Node.prototype;
  function Element() { throw new TypeError('Illegal constructor'); }
  Element.prototype = O.create(NP);
  var EL = Element.prototype;
  function HTMLElement() { throw new TypeError('Illegal constructor'); }
  HTMLElement.prototype = O.create(EL);
  var HE = HTMLElement.prototype;
  function CharacterData() { throw new TypeError('Illegal constructor'); }
  CharacterData.prototype = O.create(NP);
  function Text(t) { var n = novoTexto(t === undefined ? '' : String(t)); return n; }
  Text.prototype = O.create(CharacterData.prototype);
  function Comment(t) { var n = O.create(Comment.prototype); iniciar(n, -1, 8, '#comment'); n._dado = t === undefined ? '' : String(t); return n; }
  Comment.prototype = O.create(CharacterData.prototype);
  function DocumentFragment() { var n = O.create(DocumentFragment.prototype); iniciar(n, -1, 11, '#document-fragment'); return n; }
  DocumentFragment.prototype = O.create(NP);
  g.Node = Node; g.Element = Element; g.HTMLElement = HTMLElement; g.CharacterData = CharacterData; g.Text = Text; g.Comment = Comment;
  g.DocumentFragment = DocumentFragment;
  ['HTMLDivElement', 'HTMLSpanElement', 'HTMLAnchorElement', 'HTMLInputElement', 'HTMLButtonElement', 'HTMLFormElement', 'HTMLImageElement',
   'HTMLParagraphElement', 'HTMLUListElement', 'HTMLLIElement', 'HTMLTableElement', 'HTMLSelectElement', 'HTMLOptionElement', 'HTMLTextAreaElement',
   'HTMLTemplateElement', 'HTMLCanvasElement', 'HTMLVideoElement', 'HTMLAudioElement', 'HTMLMediaElement', 'HTMLHeadingElement', 'HTMLBodyElement',
   'HTMLHtmlElement', 'HTMLScriptElement', 'HTMLStyleElement', 'HTMLLinkElement', 'HTMLIFrameElement', 'HTMLLabelElement', 'SVGElement', 'HTMLUnknownElement']
    .forEach(function (k) { g[k] = HTMLElement; });

  function lista(a) { oculto(a, 'item', function (i) { return this[i] || null; }); return a; }
  function iniciar(n, id, tipo, nome) {
    oculto(n, '_id', id); oculto(n, '_tipo', tipo); oculto(n, '_nome', nome);
    oculto(n, '_pai', null); oculto(n, '_filhos', lista([]));
    if (id >= 0) nos[id] = n;
  }
  function novoElemento(id, tag) {
    var n = O.create(HE);
    iniciar(n, id, 1, tag);
    oculto(n, '_atr', {}); oculto(n, '_ordem', []);
    return n;
  }
  function novoTexto(t, id) {
    var n = O.create(Text.prototype);
    if (id === undefined) id = N.criar('#texto', t);
    iniciar(n, id, 3, '#text');
    oculto(n, '_dado', t);
    return n;
  }

  /* ---- montar la copia del árbol a partir de la lista del motor ---- */
  function desesc(s) { return s.indexOf('\\') < 0 ? s : s.replace(/\\(.)/g, function (_, c) { return c === 't' ? '\t' : c === 'n' ? '\n' : c === 'r' ? '\r' : c; }); }
  var corpoId = -1, htmlId = -1;
  function montar(texto, raiz) {
    var L = texto.split('\n'), novos = [];
    for (var i = 0; i < L.length; i++) {
      var l = L[i];
      if (!l) continue;
      var f = l.split('\t');
      if (f[0] === 'D') { if (+f[1] >= 0) corpoId = +f[1]; if (+f[2] >= 0) htmlId = +f[2]; continue; }
      if (f[0] !== 'N' || f.length < 8) continue;
      var id = +f[1], pai = +f[2], tag = desesc(f[3]), na = +f[6], n;
      if (tag === '#texto') n = novoTexto(desesc(f.slice(7).join('\t')), id);
      else if (tag === '#doc') { n = doc; iniciar(doc, id, 9, '#document'); }
      else {
        n = novoElemento(id, tag);
        for (var k = 0; k < na && i + 1 < L.length; k++) {
          var a = L[++i].split('\t');
          if (a[0] !== 'A') { i--; break; }
          var nm = desesc(a[1]);
          if (!(nm in n._atr)) n._ordem.push(nm);
          n._atr[nm] = desesc(a.slice(2).join('\t'));
        }
        if (n._atr.id) indexar(n);
      }
      var P = nos[pai] && nos[pai] !== n ? nos[pai] : raiz;
      if (P) { n._pai = P; P._filhos.push(n); }
      else if (n !== doc) novos.push(n);
      if (P === raiz) novos.push(n);
    }
    return novos;
  }
  g.__sagui_montar = function (texto) {
    nos = []; corpoId = htmlId = -1;
    iniciar(doc, -1, 9, '#document');
    porId = {}; geracao++;
    montar(String(texto), null);
  };

  /* ---- MutationObserver: registros que se entregan en una microtarea ---- */
  var obsAtivos = [], entregaMarcada = false;
  function MutationObserver(cb) { this._cb = cb; this._alvos = []; this._fila = []; }
  MutationObserver.prototype.observe = function (alvo, o) {
    o = o || {};
    if (o.attributeOldValue || o.attributeFilter) o.attributes = true;
    if (o.characterDataOldValue) o.characterData = true;
    for (var i = 0; i < this._alvos.length; i++) if (this._alvos[i].n === alvo) { this._alvos[i].o = o; return; }
    this._alvos.push({ n: alvo, o: o });
    if (obsAtivos.indexOf(this) < 0) obsAtivos.push(this);
  };
  MutationObserver.prototype.disconnect = function () { this._alvos = []; this._fila = []; var i = obsAtivos.indexOf(this); if (i >= 0) obsAtivos.splice(i, 1); };
  MutationObserver.prototype.takeRecords = function () { var f = this._fila; this._fila = []; return f; };
  g.MutationObserver = MutationObserver;
  function mutou(tipo, alvo, d) {
    if (!obsAtivos.length) return;
    for (var i = 0; i < obsAtivos.length; i++) {
      var ob = obsAtivos[i], quer = null;
      for (var k = 0; k < ob._alvos.length && !quer; k++) {
        var r = ob._alvos[k], o = r.o;
        if (r.n !== alvo && !(o.subtree && contem(r.n, alvo))) continue;
        if (tipo === 'childList' && !o.childList) continue;
        if (tipo === 'attributes' && (!o.attributes || (o.attributeFilter && o.attributeFilter.indexOf(d.attributeName) < 0))) continue;
        if (tipo === 'characterData' && !o.characterData) continue;
        quer = o;
      }
      if (!quer) continue;
      ob._fila.push({ type: tipo, target: alvo, addedNodes: d.add || [], removedNodes: d.rem || [], previousSibling: d.ant || null, nextSibling: d.prox || null,
                      attributeName: d.attributeName || null, attributeNamespace: null,
                      oldValue: (tipo === 'attributes' && quer.attributeOldValue) || (tipo === 'characterData' && quer.characterDataOldValue) ? d.velho : null });
      if (!entregaMarcada) {
        entregaMarcada = true;
        Promise.resolve().then(function () {
          entregaMarcada = false;
          obsAtivos.slice().forEach(function (ob) { var f = ob.takeRecords(); if (f.length) { try { ob._cb(f, ob); } catch (x) { erro('MutationObserver', x); } } });
        });
      }
    }
  }

  /* ---- la forma del árbol ---- */
  function contem(a, b) { for (var n = b; n; n = n._pai) if (n === a) return true; return false; }
  function ligado(n) { while (n._pai) n = n._pai; return n === doc; }
  function tirarDe(n) {
    var p = n._pai;
    if (!p) return;
    var i = p._filhos.indexOf(n);
    if (i >= 0) p._filhos.splice(i, 1);
    n._pai = null;
    geracao++;
    if (n._id >= 0) N.tirar(n._id);
    if (ativo && contem(n, ativo)) ativo = null;
    mutou('childList', p, { rem: [n], ant: p._filhos[i - 1], prox: p._filhos[i] });
  }
  function validar(p, n) {
    if (!n || typeof n._tipo !== 'number') throw new TypeError('el argumento no es un nodo');
    if (contem(n, p)) throw new DOMException('el nodo contiene al padre', 'HierarchyRequestError');
    if (p._tipo === 3 || p._tipo === 8) throw new DOMException('un texto no tiene hijos', 'HierarchyRequestError');
  }
  /* Mete n en p antes de ref (null: al final). Con un fragmento, sus hijos. */
  function meter(p, n, ref) {
    validar(p, n);
    if (ref && ref._pai !== p) throw new DOMException('la referencia no es hija', 'NotFoundError');
    if (n._tipo === 11) { var fs = n._filhos.slice(); for (var i = 0; i < fs.length; i++) meter(p, fs[i], ref); return n; }
    if (n === ref) return n;
    if (n._pai) tirarDe(n);
    if (n._id < 0 && n._tipo === 8) { /* comentario: solo en la copia */ }
    var k = ref ? p._filhos.indexOf(ref) : p._filhos.length;
    p._filhos.splice(k, 0, n);
    n._pai = p;
    geracao++;
    if (n._tipo === 1 && ligado(p)) indexarArvore(n);
    if (p._id >= 0 && n._id >= 0) {
      var rid = -1;
      if (ref) for (var j = k + 1; j < p._filhos.length; j++) if (p._filhos[j]._id >= 0) { rid = p._filhos[j]._id; break; }
      N.inserir(p._id, n._id, rid);
    }
    mutou('childList', p, { add: [n], ant: p._filhos[k - 1], prox: p._filhos[k + 1] });
    return n;
  }
  function comoNo(x) { return x && typeof x._tipo === 'number' ? x : novoTexto(String(x)); }
  function varios(args) {
    if (args.length === 1) return comoNo(args[0]);
    var f = DocumentFragment();
    for (var i = 0; i < args.length; i++) meter(f, comoNo(args[i]), null);
    return f;
  }

  def(NP, 'nodeType', function () { return this._tipo; });
  def(NP, 'nodeName', function () { return this._tipo === 1 ? this._nome.toUpperCase() : this._nome; });
  def(NP, 'parentNode', function () { return this._pai; });
  def(NP, 'parentElement', function () { return this._pai && this._pai._tipo === 1 ? this._pai : null; });
  def(NP, 'childNodes', function () { return this._filhos; });
  def(NP, 'firstChild', function () { return this._filhos[0] || null; });
  def(NP, 'lastChild', function () { return this._filhos[this._filhos.length - 1] || null; });
  function irmao(n, d) { var p = n._pai; if (!p) return null; return p._filhos[p._filhos.indexOf(n) + d] || null; }
  function irmaoEl(n, d) { var p = n._pai; if (!p) return null; var f = p._filhos; for (var i = f.indexOf(n) + d; i >= 0 && i < f.length; i += d) if (f[i]._tipo === 1) return f[i]; return null; }
  def(NP, 'nextSibling', function () { return irmao(this, 1); });
  def(NP, 'previousSibling', function () { return irmao(this, -1); });
  def(NP, 'ownerDocument', function () { return this === doc ? null : doc; });
  def(NP, 'isConnected', function () { return ligado(this); });
  def(NP, 'nodeValue', function () { return this._tipo === 3 || this._tipo === 8 ? this._dado : null; }, function (v) { if (this._tipo === 3 || this._tipo === 8) this.data = v; });
  NP.hasChildNodes = function () { return this._filhos.length > 0; };
  NP.appendChild = function (n) { return meter(this, n, null); };
  NP.insertBefore = function (n, ref) { return meter(this, n, ref || null); };
  NP.removeChild = function (n) { if (!n || n._pai !== this) throw new DOMException('no es hijo', 'NotFoundError'); tirarDe(n); return n; };
  NP.replaceChild = function (n, velho) { if (!velho || velho._pai !== this) throw new DOMException('no es hijo', 'NotFoundError'); if (n === velho) return velho; var ref = irmao(velho, 1); if (ref === n) ref = irmao(n, 1); tirarDe(velho); meter(this, n, ref); return velho; };
  NP.contains = function (o) { return !!o && contem(this, o); };
  NP.getRootNode = function () { var n = this; while (n._pai) n = n._pai; return n; };
  NP.isSameNode = function (o) { return this === o; };
  NP.compareDocumentPosition = function (o) {
    if (o === this) return 0;
    if (contem(this, o)) return 20; if (contem(o, this)) return 10;
    if (this.getRootNode() !== o.getRootNode()) return 1;
    var a = [], b = [], n;
    for (n = this; n; n = n._pai) a.unshift(n);
    for (n = o; n; n = n._pai) b.unshift(n);
    var i = 0; while (a[i] === b[i]) i++;
    return a[i - 1]._filhos.indexOf(a[i]) < a[i - 1]._filhos.indexOf(b[i]) ? 4 : 2;
  };
  NP.cloneNode = function (fundo) { return clonar(this, !!fundo); };
  NP.normalize = function () {};
  function texto(n) {
    if (n._tipo === 3) return n._dado;
    if (n._tipo === 8) return '';
    var s = '';
    for (var i = 0; i < n._filhos.length; i++) { var c = n._filhos[i]; s += c._tipo === 3 ? c._dado : c._tipo === 1 ? texto(c) : ''; }
    return s;
  }
  function esvaziar(n) {
    var rem = n._filhos.slice();
    if (!rem.length) return;
    if (n._id >= 0) { N.html(n._id, ''); for (var i = 0; i < rem.length; i++) rem[i]._pai = null; n._filhos.length = 0; }
    else for (var j = 0; j < rem.length; j++) tirarDe(rem[j]);
    if (ativo && !ligado(ativo)) ativo = null;
    mutou('childList', n, { rem: rem });
  }
  def(NP, 'textContent', function () { return this === doc ? null : texto(this); }, function (v) {
    v = v == null ? '' : String(v);
    if (this._tipo === 3 || this._tipo === 8) { this.data = v; return; }
    if (this._filhos.length === 1 && this._filhos[0]._tipo === 3) { this._filhos[0].data = v; return; }
    esvaziar(this);
    if (v !== '') meter(this, novoTexto(v), null);
  });

  /* ---- texto ---- */
  var CD = CharacterData.prototype;
  def(CD, 'data', function () { return this._dado; }, function (v) {
    v = v == null ? '' : String(v);
    var velho = this._dado;
    this._dado = v;
    if (this._id >= 0) N.dado(this._id, v);
    mutou('characterData', this, { velho: velho });
  });
  def(CD, 'length', function () { return this._dado.length; });
  def(CD, 'wholeText', function () { return this._dado; });
  CD.appendData = function (s) { this.data = this._dado + s; };
  CD.remove = function () { if (this._pai) tirarDe(this); };
  CD.substringData = function (a, n) { return this._dado.substr(a, n); };
  CD.before = function () { var p = this._pai; if (p) meter(p, varios(arguments), this); };
  CD.after = function () { var p = this._pai; if (p) meter(p, varios(arguments), irmao(this, 1)); };
  CD.replaceWith = function () { var p = this._pai; if (!p) return; var ref = irmao(this, 1); tirarDe(this); meter(p, varios(arguments), ref); };
  def(CD, 'nextElementSibling', function () { return irmaoEl(this, 1); });
  def(CD, 'previousElementSibling', function () { return irmaoEl(this, -1); });

  /* ---- atributos ---- */
  var REFLEX = { className: 'class', htmlFor: 'for', id: 'id', title: 'title', lang: 'lang', dir: 'dir', name: 'name', type: 'type', alt: 'alt',
                 placeholder: 'placeholder', rel: 'rel', target: 'target', action: 'action', method: 'method', role: 'role', tabIndex: 'tabindex',
                 width: 'width', height: 'height', colSpan: 'colspan', rowSpan: 'rowspan', min: 'min', max: 'max', step: 'step', pattern: 'pattern',
                 accessKey: 'accesskey', download: 'download', autocomplete: 'autocomplete', enctype: 'enctype', label: 'label' };
  var BOOL = { disabled: 1, checked: 1, selected: 1, hidden: 1, required: 1, readOnly: 'readonly', multiple: 1, autofocus: 1, open: 1, noValidate: 'novalidate',
               defer: 1, async: 1, controls: 1, autoplay: 1, loop: 1, muted: 1, draggable: 0 };
  function porAtr(n, nm, v) {
    nm = String(nm).toLowerCase();
    var velho = n._atr[nm];
    if (v === null) { if (velho === undefined) return; delete n._atr[nm]; var i = n._ordem.indexOf(nm); if (i >= 0) n._ordem.splice(i, 1); }
    else { v = String(v); if (velho === v) return; if (velho === undefined) n._ordem.push(nm); n._atr[nm] = v; }
    if (nm === 'id') { if (v) indexar(n); geracao++; }
    if (n._id >= 0) N.atr(n._id, nm, v);
    mutou('attributes', n, { attributeName: nm, velho: velho === undefined ? null : velho });
  }
  EL.getAttribute = function (nm) { var v = this._atr[String(nm).toLowerCase()]; return v === undefined ? null : v; };
  EL.setAttribute = function (nm, v) { porAtr(this, nm, v); };
  EL.removeAttribute = function (nm) { porAtr(this, nm, null); };
  EL.hasAttribute = function (nm) { return this._atr[String(nm).toLowerCase()] !== undefined; };
  EL.hasAttributes = function () { return this._ordem.length > 0; };
  EL.getAttributeNames = function () { return this._ordem.slice(); };
  EL.toggleAttribute = function (nm, f) { var on = f === undefined ? !this.hasAttribute(nm) : !!f; if (on) { if (!this.hasAttribute(nm)) porAtr(this, nm, ''); } else porAtr(this, nm, null); return on; };
  EL.getAttributeNS = function (ns, nm) { return this.getAttribute(nm); };
  EL.setAttributeNS = function (ns, nm, v) { this.setAttribute(nm, v); };
  def(EL, 'attributes', function () { var e = this; return lista(e._ordem.map(function (k) { return { name: k, value: e._atr[k], nodeName: k, nodeValue: e._atr[k] }; })); });
  for (var r in REFLEX) (function (p, a) { def(EL, p, function () { var v = this._atr[a]; return p === 'tabIndex' ? (v === undefined ? (/^(a|button|input|select|textarea)$/.test(this._nome) ? 0 : -1) : +v) : v === undefined ? '' : v; }, function (v) { porAtr(this, a, v); }); })(r, REFLEX[r]);
  for (var b in BOOL) (function (p, a) { def(EL, p, function () { return this._atr[a] !== undefined; }, function (v) { if (v) porAtr(this, a, ''); else porAtr(this, a, null); }); })(b, BOOL[b] === 1 || BOOL[b] === 0 ? b.toLowerCase() : BOOL[b]);
  ['href', 'src', 'action'].forEach(function (k) {
    def(EL, k, function () { var v = this._atr[k]; return v === undefined ? '' : absoluta(v); }, function (v) { porAtr(this, k, v); });
  });
  def(EL, 'tagName', function () { return this._nome.toUpperCase(); });
  def(EL, 'localName', function () { return this._nome; });
  def(EL, 'namespaceURI', function () { return 'http://www.w3.org/1999/xhtml'; });

  /* classList: lista viva sobre el atributo class */
  function TokenList(el, at) { oculto(this, '_el', el); oculto(this, '_at', at); }
  var TL = TokenList.prototype;
  TL._ler = function () { return (this._el._atr[this._at] || '').split(/\s+/).filter(Boolean); };
  TL._por = function (a) { porAtr(this._el, this._at, a.join(' ')); };
  TL.contains = function (c) { return this._ler().indexOf(String(c)) >= 0; };
  TL.add = function () { var a = this._ler(), m = false; for (var i = 0; i < arguments.length; i++) { var c = String(arguments[i]); if (a.indexOf(c) < 0) { a.push(c); m = true; } } if (m || this._el._atr[this._at] === undefined) this._por(a); };
  TL.remove = function () { var a = this._ler(), m = false; for (var i = 0; i < arguments.length; i++) { var k = a.indexOf(String(arguments[i])); while (k >= 0) { a.splice(k, 1); m = true; k = a.indexOf(String(arguments[i])); } } if (m) this._por(a); };
  TL.toggle = function (c, f) { c = String(c); var a = this._ler(), k = a.indexOf(c), on = f === undefined ? k < 0 : !!f; if (on && k < 0) a.push(c); else if (!on && k >= 0) a.splice(k, 1); else return on; this._por(a); return on; };
  TL.replace = function (x, y) { var a = this._ler(), k = a.indexOf(String(x)); if (k >= 0) { a[k] = String(y); this._por(a); } return k >= 0; };
  TL.item = function (i) { var v = this._ler()[i]; return v === undefined ? null : v; };
  TL.forEach = function (f, t) { this._ler().forEach(f, t); };
  TL.toString = function () { return this._el._atr[this._at] || ''; };
  TL.supports = function () { return true; };
  TL[Symbol.iterator] = function () { return this._ler()[Symbol.iterator](); };
  def(TL, 'length', function () { return this._ler().length; });
  def(TL, 'value', function () { return this.toString(); }, function (v) { porAtr(this._el, this._at, v); });
  def(EL, 'classList', function () { return this._cl || (oculto(this, '_cl', new TokenList(this, 'class')), this._cl); }, function (v) { porAtr(this, 'class', v); });
  def(EL, 'relList', function () { return new TokenList(this, 'rel'); });

  /* style: el atributo style como objeto */
  function cssNome(n) { n = String(n); if (n === 'cssFloat') return 'float'; if (n.indexOf('-') >= 0) return n.toLowerCase(); return n.replace(/[A-Z]/g, function (c) { return '-' + c.toLowerCase(); }).replace(/^(webkit|moz|ms)-/, '-$1-'); }
  function lerEstilo(el) {
    var o = {}, s = el._atr.style || '', d = s.split(';');
    for (var i = 0; i < d.length; i++) { var k = d[i].indexOf(':'); if (k > 0) { var nm = d[i].slice(0, k).trim().toLowerCase(); if (nm) o[nm] = d[i].slice(k + 1).trim(); } }
    return o;
  }
  function porEstilo(el, o) { var s = ''; for (var k in o) if (o[k] !== '' && o[k] != null) s += (s ? ' ' : '') + k + ': ' + o[k] + ';'; porAtr(el, 'style', s); }
  var ESTILO_M = {
    setProperty: function (n, v, pr) { var o = lerEstilo(this._el); n = cssNome(n); v = v == null ? '' : String(v) + (pr ? ' !important' : ''); if (v === '') delete o[n]; else o[n] = v; porEstilo(this._el, o); },
    getPropertyValue: function (n) { return lerEstilo(this._el)[cssNome(n)] || ''; },
    getPropertyPriority: function (n) { return /!important/.test(lerEstilo(this._el)[cssNome(n)] || '') ? 'important' : ''; },
    removeProperty: function (n) { var o = lerEstilo(this._el), k = cssNome(n), v = o[k] || ''; if (k in o) { delete o[k]; porEstilo(this._el, o); } return v; },
    item: function (i) { return O.keys(lerEstilo(this._el))[i] || ''; }
  };
  function estiloDe(el) {
    var alvo = { _el: el };
    return new Proxy(alvo, {
      get: function (t, n) {
        if (typeof n !== 'string') return undefined;
        if (n === '_el') return el;
        if (ESTILO_M[n]) return ESTILO_M[n];
        if (n === 'cssText') return el._atr.style || '';
        if (n === 'length') return O.keys(lerEstilo(el)).length;
        if (n === 'parentRule') return null;
        return lerEstilo(el)[cssNome(n)] || '';
      },
      set: function (t, n, v) {
        if (typeof n !== 'string') return true;
        if (n === 'cssText') { porAtr(el, 'style', v == null ? '' : String(v)); return true; }
        var o = lerEstilo(el), k = cssNome(n), s = v == null ? '' : String(v);
        if (typeof v === 'number' && v !== 0 && /^(width|height|left|top|right|bottom|margin|padding|font-size)/.test(k)) s = v + 'px';
        if (s === '') { if (!(k in o)) return true; delete o[k]; } else { if (o[k] === s) return true; o[k] = s; }
        porEstilo(el, o);
        return true;
      },
      has: function (t, n) { return typeof n === 'string'; }
    });
  }
  def(HE, 'style', function () { return this._st || (oculto(this, '_st', estiloDe(this)), this._st); }, function (v) { porAtr(this, 'style', String(v)); });
  /* dataset */
  function dsNome(n) { return 'data-' + String(n).replace(/[A-Z]/g, function (c) { return '-' + c.toLowerCase(); }); }
  def(HE, 'dataset', function () {
    var el = this;
    return this._ds || (oculto(this, '_ds', new Proxy({}, {
      get: function (t, n) { if (typeof n !== 'string') return undefined; var v = el._atr[dsNome(n)]; return v; },
      set: function (t, n, v) { porAtr(el, dsNome(n), String(v)); return true; },
      has: function (t, n) { return typeof n === 'string' && el._atr[dsNome(n)] !== undefined; },
      deleteProperty: function (t, n) { porAtr(el, dsNome(n), null); return true; },
      ownKeys: function () { return el._ordem.filter(function (k) { return k.slice(0, 5) === 'data-'; }).map(function (k) { return k.slice(5).replace(/-([a-z])/g, function (_, c) { return c.toUpperCase(); }); }); },
      getOwnPropertyDescriptor: function (t, n) { var v = el._atr[dsNome(n)]; return v === undefined ? undefined : { value: v, writable: true, enumerable: true, configurable: true }; }
    })), this._ds);
  });

  /* value de los campos: lo que se escribió, o el atributo */
  def(HE, 'value', function () {
    if (this._valor !== undefined) return this._valor;
    if (this._nome === 'textarea') return texto(this);
    if (this._nome === 'select') { var o = this.querySelector('option[selected]') || this.querySelector('option'); return o ? o.value : ''; }
    if (this._nome === 'option') { var v = this._atr.value; return v === undefined ? texto(this).trim() : v; }
    var a = this._atr.value;
    if (a === undefined) return (this._nome === 'input' && /^(checkbox|radio)$/.test(this._atr.type || '')) ? 'on' : '';
    return a;
  }, function (v) {
    v = v == null ? '' : String(v);
    if (this._nome === 'select') { var os = this.querySelectorAll('option'); for (var i = 0; i < os.length; i++) os[i].selected = os[i].value === v; return; }
    oculto(this, '_valor', v);
    if (this._nome === 'textarea') this.textContent = v; else porAtr(this, 'value', v);
  });
  def(HE, 'defaultValue', function () { return this._atr.value || ''; }, function (v) { porAtr(this, 'value', v); });
  def(HE, 'valueAsNumber', function () { var v = parseFloat(this.value); return isNaN(v) ? NaN : v; }, function (v) { this.value = String(v); });
  def(HE, 'selectedIndex', function () { var os = this.querySelectorAll('option'); for (var i = 0; i < os.length; i++) if (os[i].selected) return i; return os.length ? 0 : -1; },
      function (k) { var os = this.querySelectorAll('option'); for (var i = 0; i < os.length; i++) os[i].selected = i === k; });
  def(HE, 'options', function () { return this.querySelectorAll('option'); });
  def(HE, 'form', function () { return this.closest('form'); });
  def(HE, 'elements', function () { return this.querySelectorAll('input,select,textarea,button'); });
  def(HE, 'text', function () { return texto(this); }, function (v) { this.textContent = v; });
  def(HE, 'files', function () { return lista([]); });
  def(HE, 'validity', function () { return { valid: true, valueMissing: false }; });
  HE.checkValidity = HE.reportValidity = function () { return true; };
  HE.setCustomValidity = function () {};
  HE.select = HE.setSelectionRange = function () {};
  HE.submit = function () { if (this._nome === 'form') N.navegar(this.action || location.href); };
  HE.requestSubmit = function () { if (this._nome === 'form' && despachar(this, new g.SubmitEvent('submit', { bubbles: true, cancelable: true }))) this.submit(); };
  HE.reset = function () {};

  /* ---- árbol de elementos ---- */
  def(EL, 'children', function () { return lista(this._filhos.filter(function (c) { return c._tipo === 1; })); });
  def(EL, 'firstElementChild', function () { var f = this._filhos; for (var i = 0; i < f.length; i++) if (f[i]._tipo === 1) return f[i]; return null; });
  def(EL, 'lastElementChild', function () { var f = this._filhos; for (var i = f.length - 1; i >= 0; i--) if (f[i]._tipo === 1) return f[i]; return null; });
  def(EL, 'childElementCount', function () { return this.children.length; });
  def(EL, 'nextElementSibling', function () { return irmaoEl(this, 1); });
  def(EL, 'previousElementSibling', function () { return irmaoEl(this, -1); });
  EL.remove = function () { if (this._pai) tirarDe(this); };
  NP.append = function () { meter(this, varios(arguments), null); };
  NP.prepend = function () { meter(this, varios(arguments), this._filhos[0] || null); };
  EL.before = CD.before; EL.after = CD.after; EL.replaceWith = CD.replaceWith;
  NP.replaceChildren = function () { esvaziar(this); if (arguments.length) meter(this, varios(arguments), null); };
  EL.insertAdjacentElement = function (onde, n) {
    onde = String(onde).toLowerCase();
    if (onde === 'beforebegin') { if (this._pai) meter(this._pai, n, this); }
    else if (onde === 'afterbegin') meter(this, n, this._filhos[0] || null);
    else if (onde === 'beforeend') meter(this, n, null);
    else if (onde === 'afterend') { if (this._pai) meter(this._pai, n, irmao(this, 1)); }
    return n;
  };
  EL.insertAdjacentText = function (onde, t) { this.insertAdjacentElement(onde, novoTexto(String(t))); };
  EL.insertAdjacentHTML = function (onde, h) { this.insertAdjacentElement(onde, fragmentoHTML(String(h))); };

  /* ---- innerHTML y outerHTML ---- */
  var VAZIOS = /^(area|base|br|col|embed|hr|img|input|link|meta|source|track|wbr|param|keygen)$/;
  function escTexto(s) { return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/ /g, '&nbsp;'); }
  function escAtr(s) { return s.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/ /g, '&nbsp;'); }
  function serial(n) {
    if (n._tipo === 3) return n._pai && /^(script|style)$/.test(n._pai._nome) ? n._dado : escTexto(n._dado);
    if (n._tipo === 8) return '<!--' + n._dado + '-->';
    if (n._tipo !== 1) return interno(n);
    var s = '<' + n._nome;
    for (var i = 0; i < n._ordem.length; i++) s += ' ' + n._ordem[i] + '="' + escAtr(n._atr[n._ordem[i]]) + '"';
    s += '>';
    if (VAZIOS.test(n._nome)) return s;
    return s + interno(n) + '</' + n._nome + '>';
  }
  function interno(n) { var s = ''; for (var i = 0; i < n._filhos.length; i++) s += serial(n._filhos[i]); return s; }
  /* el motor lee el HTML dentro del elemento y devuelve los nodos nuevos */
  function porHTML(el, h) {
    var rem = el._filhos.slice();
    for (var i = 0; i < rem.length; i++) rem[i]._pai = null;
    el._filhos.length = 0;
    var lst = N.html(el._id, h);
    var novos = typeof lst === 'string' ? montar(lst, el) : [];
    geracao++;
    if (ativo && !ligado(ativo)) ativo = null;
    mutou('childList', el, { rem: rem, add: novos });
  }
  function fragmentoHTML(h) {
    var t = novoElemento(N.criar('template', null), 'template');
    porHTML(t, h);
    var f = DocumentFragment();
    var fs = t._filhos.slice();
    for (var i = 0; i < fs.length; i++) meter(f, fs[i], null);
    return f;
  }
  def(EL, 'innerHTML', function () { return interno(this); }, function (h) {
    h = h == null ? '' : String(h);
    if (this._id < 0) return;
    porHTML(this, h);
  });
  def(EL, 'outerHTML', function () { return serial(this); }, function (h) { var p = this._pai; if (!p) return; var ref = irmao(this, 1); var f = fragmentoHTML(String(h)); tirarDe(this); meter(p, f, ref); });
  def(HE, 'innerText', function () { return texto(this); }, function (v) { this.textContent = v; });
  def(HE, 'outerText', function () { return texto(this); });
  /* <template>.content: un fragmento con copias de lo que hay dentro */
  def(HE, 'content', function () {
    if (this._nome !== 'template') return undefined;
    if (!this._conteudo) { var f = DocumentFragment(); for (var i = 0; i < this._filhos.length; i++) meter(f, clonar(this._filhos[i], true), null); oculto(this, '_conteudo', f); }
    return this._conteudo;
  });

  function clonar(n, fundo) {
    var c;
    if (n._tipo === 3) return novoTexto(n._dado);
    if (n._tipo === 8) return Comment(n._dado);
    if (n._tipo === 11) c = DocumentFragment();
    else if (n._tipo === 9) return n;
    else {
      c = novoElemento(N.criar(n._nome, null), n._nome);
      for (var i = 0; i < n._ordem.length; i++) { var k = n._ordem[i]; c._ordem.push(k); c._atr[k] = n._atr[k]; N.atr(c._id, k, n._atr[k]); }
      if (n._valor !== undefined) oculto(c, '_valor', n._valor);
    }
    if (fundo) for (var j = 0; j < n._filhos.length; j++) meter(c, clonar(n._filhos[j], true), null);
    return c;
  }

  /* ---------------------------------------------------------------- selectores */
  var cacheSel = {};
  function erroSel(s) { throw new DOMException("'" + s + "' no es un selector válido", 'SyntaxError'); }
  /* Lee una lista de selectores: [[{comb, tag, id, cls[], atr[], ps[]}...], ...] */
  function lerSel(src) {
    if (cacheSel[src]) return cacheSel[src];
    var s = String(src), i = 0, n = s.length, lst = [], cad = [], cmp = null, comb = ' ';
    function esp() { while (i < n && /\s/.test(s[i])) i++; }
    function ident() { var a = i; while (i < n && /[\w\- -￿\\]/.test(s[i])) { if (s[i] === '\\') i++; i++; } return s.slice(a, i).replace(/\\(.)/g, '$1'); }
    function novo() { if (!cmp) { cmp = { comb: comb, tag: null, id: null, cls: [], atr: [], ps: [] }; comb = ' '; } return cmp; }
    function fechar() { if (cmp) { cad.push(cmp); cmp = null; } }
    function arg() {
      var p = 1, a = ++i;
      while (i < n && p) { if (s[i] === '(') p++; else if (s[i] === ')') p--; if (p) i++; }
      var r = s.slice(a, i); i++; return r;
    }
    esp();
    while (i < n) {
      var c = s[i];
      if (c === ',') { fechar(); if (!cad.length) erroSel(src); lst.push(cad); cad = []; comb = ' '; i++; esp(); continue; }
      if (/\s/.test(c)) { esp(); if (i < n && !/[,>+~]/.test(s[i])) { fechar(); comb = ' '; } continue; }
      if (c === '>' || c === '+' || c === '~') { fechar(); comb = c; i++; esp(); continue; }
      if (c === '*') { novo(); i++; continue; }
      if (c === '#') { i++; novo().id = ident(); continue; }
      if (c === '.') { i++; novo().cls.push(ident()); continue; }
      if (c === '[') {
        var f = s.indexOf(']', i); if (f < 0) erroSel(src);
        var m = /^\s*([\w\-:]+)\s*(?:([~|^$*]?=)\s*(?:"([^"]*)"|'([^']*)'|([^\s\]]+)))?\s*([iI])?\s*$/.exec(s.slice(i + 1, f));
        if (!m) erroSel(src);
        novo().atr.push({ n: m[1].toLowerCase(), op: m[2] || '', v: m[3] !== undefined ? m[3] : m[4] !== undefined ? m[4] : m[5], ci: !!m[6] });
        i = f + 1; continue;
      }
      if (c === ':') {
        i++; if (s[i] === ':') i++;
        var p = ident().toLowerCase(), a = null;
        if (s[i] === '(') a = arg();
        var q = { n: p, a: a };
        if (/^(not|is|where|matches|has)$/.test(p) && a === null) erroSel(src);
        if (/^(not|is|where|matches|has)$/.test(p)) q.l = lerSel(a.trim().replace(/^[>+~]\s*/, function (x) { q.rel = x.trim(); return ''; }));
        if (/^nth-/.test(p)) q.f = nth(a);
        novo().ps.push(q); continue;
      }
      if (/[\w\-\\]/.test(c) || c > '\u007f') { var t = ident().toLowerCase(); novo().tag = t; continue; }
      erroSel(src);
    }
    fechar();
    if (!cad.length) erroSel(src);
    lst.push(cad);
    cacheSel[src] = lst;
    return lst;
  }
  function nth(a) {
    a = String(a || '').trim().toLowerCase().split(/\s+of\s+/)[0];
    if (a === 'odd') return [2, 1]; if (a === 'even') return [2, 0];
    var m = /^([+-]?\d*)?n\s*([+-]\s*\d+)?$/.exec(a);
    if (m) { var k = m[1] === '' || m[1] === '+' ? 1 : m[1] === '-' ? -1 : parseInt(m[1], 10); return [k, m[2] ? parseInt(m[2].replace(/\s/g, ''), 10) : 0]; }
    return [0, parseInt(a, 10) || 0];
  }
  function nthOk(f, pos) { var a = f[0], b = f[1]; if (!a) return pos === b; var k = (pos - b) / a; return k >= 0 && Math.floor(k) === k; }
  function posicao(el, deFim, mesmoTipo) {
    var p = el._pai; if (!p) return 1;
    var f = p._filhos, k = 0, i, ini = deFim ? f.length - 1 : 0, d = deFim ? -1 : 1;
    for (i = ini; i >= 0 && i < f.length; i += d) { var c = f[i]; if (c._tipo !== 1 || (mesmoTipo && c._nome !== el._nome)) continue; k++; if (c === el) return k; }
    return k;
  }
  function atrOk(el, a) {
    var v = el._atr[a.n];
    if (v === undefined) return false;
    if (!a.op) return true;
    var x = a.v, y = v;
    if (a.ci) { x = x.toLowerCase(); y = y.toLowerCase(); }
    switch (a.op) {
      case '=': return y === x;
      case '~=': return y.split(/\s+/).indexOf(x) >= 0;
      case '|=': return y === x || y.indexOf(x + '-') === 0;
      case '^=': return x !== '' && y.indexOf(x) === 0;
      case '$=': return x !== '' && y.slice(-x.length) === x;
      case '*=': return x !== '' && y.indexOf(x) >= 0;
    }
    return false;
  }
  function psOk(el, q, escopo) {
    switch (q.n) {
      case 'first-child': return posicao(el, false) === 1;
      case 'last-child': return posicao(el, true) === 1;
      case 'only-child': return posicao(el, false) === 1 && posicao(el, true) === 1;
      case 'first-of-type': return posicao(el, false, true) === 1;
      case 'last-of-type': return posicao(el, true, true) === 1;
      case 'only-of-type': return posicao(el, false, true) === 1 && posicao(el, true, true) === 1;
      case 'nth-child': return nthOk(q.f, posicao(el, false));
      case 'nth-last-child': return nthOk(q.f, posicao(el, true));
      case 'nth-of-type': return nthOk(q.f, posicao(el, false, true));
      case 'nth-last-of-type': return nthOk(q.f, posicao(el, true, true));
      case 'not': return !casaLista(el, q.l, escopo);
      case 'is': case 'where': case 'matches': return casaLista(el, q.l, escopo);
      case 'has':
        if (q.rel === '+' || q.rel === '~') { for (var s2 = irmaoEl(el, 1); s2; s2 = q.rel === '+' ? null : irmaoEl(s2, 1)) if (casaLista(s2, q.l, escopo)) return true; return false; }
        return !!acharEm(el, q.l, true, q.rel === '>' ? el : null);
      case 'empty': return !el._filhos.some(function (c) { return c._tipo === 1 || (c._tipo === 3 && c._dado !== ''); });
      case 'root': return el._pai === doc;
      case 'scope': return escopo ? el === escopo : el._pai === doc;
      case 'checked': return el._atr.checked !== undefined || el._atr.selected !== undefined;
      case 'disabled': return el._atr.disabled !== undefined;
      case 'enabled': return /^(input|button|select|textarea|option)$/.test(el._nome) && el._atr.disabled === undefined;
      case 'required': return el._atr.required !== undefined;
      case 'optional': return /^(input|select|textarea)$/.test(el._nome) && el._atr.required === undefined;
      case 'link': case 'any-link': return (el._nome === 'a' || el._nome === 'area') && el._atr.href !== undefined;
      case 'focus': case 'focus-visible': return el === ativo;
      case 'focus-within': return !!ativo && contem(el, ativo);
      case 'target': return !!el._atr.id && location.hash === '#' + el._atr.id;
      case 'defined': return true;
      case 'placeholder-shown': return el._atr.placeholder !== undefined && !el.value;
      default: return false;   /* :hover, :active, :visited y pseudoelementos: nunca */
    }
  }
  function compOk(el, c, escopo) {
    if (c.tag && c.tag !== '*' && el._nome !== c.tag) return false;
    if (c.id !== null && el._atr.id !== c.id) return false;
    if (c.cls.length) { var cl = ' ' + (el._atr['class'] || '').replace(/\s+/g, ' ') + ' '; for (var i = 0; i < c.cls.length; i++) if (cl.indexOf(' ' + c.cls[i] + ' ') < 0) return false; }
    for (var j = 0; j < c.atr.length; j++) if (!atrOk(el, c.atr[j])) return false;
    for (var k = 0; k < c.ps.length; k++) if (!psOk(el, c.ps[k], escopo)) return false;
    return true;
  }
  /* de derecha a izquierda */
  function cadOk(el, cad, k, escopo) {
    if (!compOk(el, cad[k], escopo)) return false;
    if (k === 0) return true;
    var comb = cad[k].comb, a;
    if (comb === '>') { a = el._pai; return !!a && a._tipo === 1 && cadOk(a, cad, k - 1, escopo); }
    if (comb === '+') { a = irmaoEl(el, -1); return !!a && cadOk(a, cad, k - 1, escopo); }
    if (comb === '~') { for (a = irmaoEl(el, -1); a; a = irmaoEl(a, -1)) if (cadOk(a, cad, k - 1, escopo)) return true; return false; }
    for (a = el._pai; a && a._tipo === 1; a = a._pai) if (cadOk(a, cad, k - 1, escopo)) return true;
    return false;
  }
  function casaLista(el, lst, escopo) { for (var i = 0; i < lst.length; i++) if (cadOk(el, lst[i], lst[i].length - 1, escopo)) return true; return false; }
  /* Recorre los elementos de dentro de raiz en orden de documento. */
  function acharEm(raiz, lst, so1, soFilhosDe) {
    var r = [], pilha = [], f = raiz._filhos, i = 0;
    for (;;) {
      if (i >= f.length) { if (!pilha.length) break; var t = pilha.pop(); f = t[0]; i = t[1]; continue; }
      var c = f[i++];
      if (c._tipo !== 1) continue;
      if ((!soFilhosDe || c._pai === soFilhosDe) && casaLista(c, lst, raiz._tipo === 1 ? raiz : null)) { if (so1) return c; r.push(c); }
      if (c._filhos.length && !soFilhosDe) { pilha.push([f, i]); f = c._filhos; i = 0; }
    }
    return so1 ? null : r;
  }
  function qs(raiz, s) { return acharEm(raiz, lerSel(String(s)), true); }
  function qsa(raiz, s) { return lista(acharEm(raiz, lerSel(String(s)), false)); }
  [NP].forEach(function (P) {
    P.querySelector = function (s) { return qs(this, s); };
    P.querySelectorAll = function (s) { return qsa(this, s); };
    P.getElementsByTagName = function (t) { t = String(t).toLowerCase(); return lista(acharEm(this, t === '*' ? lerSel('*') : [[{ comb: ' ', tag: t, id: null, cls: [], atr: [], ps: [] }]], false)); };
    P.getElementsByClassName = function (c) { var cs = String(c).trim().split(/\s+/).filter(Boolean); if (!cs.length) return lista([]); return lista(acharEm(this, [[{ comb: ' ', tag: null, id: null, cls: cs, atr: [], ps: [] }]], false)); };
  });
  EL.matches = EL.webkitMatchesSelector = EL.msMatchesSelector = function (s) { return casaLista(this, lerSel(String(s)), null); };
  EL.closest = function (s) { var l = lerSel(String(s)); for (var e = this; e && e._tipo === 1; e = e._pai) if (casaLista(e, l, null)) return e; return null; };

  /* ---- el document ---- */
  /* getElementById: un índice que se llena al montar, al poner un id y al meter un subárbol en la
   * página. Se comprueba al usarlo (el elemento sigue con ese id y en la página); si la entrada
   * caducó (otro elemento con el mismo id, o se quitó), se recorre la página. Sin entrada: no hay. */
  var porId = {}, geracao = 0;
  function indexar(n) { var i = n._atr && n._atr.id, v = i && porId[i]; if (i && (!v || v._atr.id !== i || !ligado(v))) porId[i] = n; }
  function indexarArvore(n) { if (n._tipo !== 1) return; indexar(n); for (var i = 0; i < n._filhos.length; i++) indexarArvore(n._filhos[i]); }
  function acharId(id) {
    id = String(id);
    var n = porId[id];
    if (!n) return null;
    if (n._atr.id === id && ligado(n)) return n;
    n = acharEm(doc, [[{ comb: ' ', tag: null, id: id, cls: [], atr: [], ps: [] }]], true);
    if (n) porId[id] = n; else delete porId[id];
    return n;
  }
  doc.getElementById = acharId;
  ['querySelector', 'querySelectorAll', 'getElementsByTagName', 'getElementsByClassName', 'appendChild', 'insertBefore', 'removeChild', 'contains',
   'hasChildNodes', 'compareDocumentPosition', 'replaceChild', 'append', 'prepend', 'replaceChildren'].forEach(function (k) { doc[k] = NP[k]; });
  doc.getElementsByName = function (nm) { return qsa(doc, '[name="' + String(nm).replace(/"/g, '\\"') + '"]'); };
  ['childNodes', 'firstChild', 'lastChild', 'parentNode', 'nodeType', 'nodeName', 'textContent', 'isConnected'].forEach(function (k) { defP(doc, k, O.getOwnPropertyDescriptor(NP, k)); });
  def(doc, 'documentElement', function () { return nos[htmlId] || doc._filhos.filter(function (c) { return c._tipo === 1; })[0] || null; });
  def(doc, 'body', function () { return nos[corpoId] || g.__sagui_corpo || null; }, function () {});
  def(doc, 'head', function () { var h = doc.documentElement; return h ? h.querySelector('head') : null; });
  def(doc, 'children', function () { return lista(doc._filhos.filter(function (c) { return c._tipo === 1; })); });
  def(doc, 'firstElementChild', function () { return doc.documentElement; });
  def(doc, 'scrollingElement', function () { return doc.documentElement; });
  def(doc, 'forms', function () { return qsa(doc, 'form'); });
  def(doc, 'images', function () { return qsa(doc, 'img'); });
  def(doc, 'links', function () { return qsa(doc, 'a[href],area[href]'); });
  def(doc, 'scripts', function () { return qsa(doc, 'script'); });
  def(doc, 'activeElement', function () { return ativo || doc.body; });
  def(doc, 'defaultView', function () { return g; });
  def(doc, 'ownerDocument', function () { return null; });
  doc.createElement = function (t) { t = String(t).toLowerCase(); if (!/^[a-z][\w\-:.]*$/.test(t)) throw new DOMException('nombre de etiqueta no válido', 'InvalidCharacterError'); return novoElemento(N.criar(t, null), t); };
  doc.createElementNS = function (ns, t) { return doc.createElement(String(t).replace(/^.*:/, '')); };
  doc.createTextNode = function (t) { return novoTexto(String(t)); };
  doc.createComment = function (t) { return Comment(t); };
  doc.createDocumentFragment = function () { return DocumentFragment(); };
  doc.importNode = function (n, fundo) { return clonar(n, !!fundo); };
  doc.adoptNode = function (n) { if (n._pai) tirarDe(n); return n; };
  doc.hasFocus = function () { return true; };
  doc.elementFromPoint = function (x, y) { var id = N.em(Math.floor(x), Math.floor(y)); return nos[id] || null; };
  doc.elementsFromPoint = function (x, y) { var r = [], e = doc.elementFromPoint(x, y); for (; e && e._tipo === 1; e = e._pai) r.push(e); return r; };
  doc.createRange = function () { return { selectNodeContents: function () {}, setStart: function () {}, setEnd: function () {}, collapse: function () {}, getBoundingClientRect: function () { return rect(0, 0, 0, 0); }, createContextualFragment: function (h) { return fragmentoHTML(String(h)); } }; };
  g.getSelection = doc.getSelection = function () { return { rangeCount: 0, removeAllRanges: function () {}, addRange: function () {}, toString: function () { return ''; } }; };
  g.Document = function () {}; g.HTMLDocument = g.Document;
  O.setPrototypeOf(doc, NP);

  /* ---------------------------------------------------------------- medidas */
  function rect(x, y, l, a) { return { x: x, y: y, left: x, top: y, width: l, height: a, right: x + l, bottom: y + a, toJSON: function () { return this; } }; }
  g.DOMRect = function (x, y, l, a) { return rect(x || 0, y || 0, l || 0, a || 0); };
  function caixa(el) { var c = el._id >= 0 && ligado(el) ? N.caixa(el._id) : null; return c && c.length >= 16 ? c : null; }
  function janela() { return N.janela(); }
  EL.getBoundingClientRect = function () {
    var c = caixa(this), j = janela();
    if (!c || !c[14]) return rect(0, 0, 0, 0);
    return rect(c[0] - j[2], c[1] - j[3], c[2], c[3]);
  };
  EL.getClientRects = function () { var r = this.getBoundingClientRect(); return lista(r.width || r.height ? [r] : []); };
  function eRaiz(el) { return el === doc.documentElement || el === doc.body; }
  def(HE, 'offsetWidth', function () { var c = caixa(this); return c ? c[2] : 0; });
  def(HE, 'offsetHeight', function () { var c = caixa(this); return c ? c[3] : 0; });
  def(HE, 'offsetParent', function () {
    if (!caixa(this) || !caixa(this)[14]) return null;
    for (var e = this._pai; e && e._tipo === 1; e = e._pai) { if (e === doc.body) return e; var p = N.estilo(e._id, 'position'); if (p && p !== 'static') return e; }
    return doc.body;
  });
  def(HE, 'offsetLeft', function () { var c = caixa(this), p = this.offsetParent, d = p && p !== doc.body ? caixa(p) : null; return c ? c[0] - (d ? d[0] + d[7] : 0) : 0; });
  def(HE, 'offsetTop', function () { var c = caixa(this), p = this.offsetParent, d = p && p !== doc.body ? caixa(p) : null; return c ? c[1] - (d ? d[1] + d[4] : 0) : 0; });
  def(EL, 'clientWidth', function () { if (eRaiz(this) && this === doc.documentElement) return janela()[0]; var c = caixa(this); return c && !c[15] ? c[2] - c[5] - c[7] : 0; });
  def(EL, 'clientHeight', function () { if (eRaiz(this) && this === doc.documentElement) return janela()[1]; var c = caixa(this); return c && !c[15] ? c[3] - c[4] - c[6] : 0; });
  def(EL, 'clientLeft', function () { var c = caixa(this); return c ? c[7] : 0; });
  def(EL, 'clientTop', function () { var c = caixa(this); return c ? c[4] : 0; });
  /* el desplazamiento propio de los bloques con overflow (D489): [x, y, máximo x, máximo y] */
  function rolagem(el) { var r = el._id >= 0 && ligado(el) ? N.rolagem(el._id) : null; return r && r.length >= 4 ? r : [0, 0, 0, 0]; }
  def(EL, 'scrollWidth', function () { if (eRaiz(this)) return Math.max(janela()[4], janela()[0]); var r = rolagem(this); if (r[2]) return this.clientWidth + r[2]; var c = caixa(this); return c ? c[12] : 0; });
  def(EL, 'scrollHeight', function () { if (eRaiz(this)) return Math.max(janela()[5], janela()[1]); var r = rolagem(this); if (r[3]) return this.clientHeight + r[3]; var c = caixa(this); return c ? c[13] : 0; });
  def(EL, 'scrollTop', function () { return eRaiz(this) ? janela()[3] : rolagem(this)[1]; }, function (v) { if (eRaiz(this)) g.scrollTo(g.scrollX, +v || 0); else N.rolar_caixa(this._id, rolagem(this)[0], Math.round(+v || 0)); });
  def(EL, 'scrollLeft', function () { return eRaiz(this) ? janela()[2] : rolagem(this)[0]; }, function (v) { if (eRaiz(this)) g.scrollTo(+v || 0, g.scrollY); else N.rolar_caixa(this._id, Math.round(+v || 0), rolagem(this)[1]); });
  EL.scrollIntoView = function (o) {
    var c = caixa(this), j = janela();
    if (!c || !c[14]) return;
    var y = c[1];
    if (o && typeof o === 'object' && o.block === 'center') y = c[1] - (j[1] - c[3]) / 2;
    else if (o === false || (o && o.block === 'end')) y = c[1] + c[3] - j[1];
    g.scrollTo(j[2], y);
  };
  EL.scrollTo = EL.scroll = function (x, y) { if (x && typeof x === 'object') { y = x.top; x = x.left; } if (eRaiz(this)) g.scrollTo(x, y); else { var r = rolagem(this); N.rolar_caixa(this._id, Math.round(x === undefined ? r[0] : +x || 0), Math.round(y === undefined ? r[1] : +y || 0)); } };
  EL.scrollBy = function (x, y) { if (x && typeof x === 'object') { y = x.top; x = x.left; } this.scrollTo((this.scrollLeft || 0) + (+x || 0), (this.scrollTop || 0) + (+y || 0)); };
  g.scrollTo = g.scroll = function (x, y) {
    if (x && typeof x === 'object') { y = x.top; x = x.left; }
    var j = janela();
    x = x === undefined ? j[2] : +x || 0; y = y === undefined ? j[3] : +y || 0;
    N.rolar(Math.round(x), Math.round(y));
  };
  g.scrollBy = function (x, y) { if (x && typeof x === 'object') { y = x.top; x = x.left; } var j = janela(); g.scrollTo(j[2] + (+x || 0), j[3] + (+y || 0)); };
  ['innerWidth', 'innerHeight', 'scrollX', 'scrollY', 'pageXOffset', 'pageYOffset', 'outerWidth', 'outerHeight'].forEach(function (k, i) {
    var idx = [0, 1, 2, 3, 2, 3, 0, 1][i];
    try { delete g[k]; } catch (x) {}
    def(g, k, function () { return janela()[idx]; }, function () {});
  });
  g.screenX = g.screenY = g.screenLeft = g.screenTop = 0;
  /* getComputedStyle: lo que el motor calculó (y el atributo style para lo que no sabe) */
  var CAMEL = /[A-Z]/g;
  g.getComputedStyle = function (el, pseudo) {
    if (!el || el._tipo !== 1) throw new TypeError('getComputedStyle: no es un elemento');
    function val(n) {
      n = cssNome(n);
      var v = el._id >= 0 && ligado(el) ? N.estilo(el._id, n) : null;
      if (v === null || v === undefined || v === '') { v = lerEstilo(el)[n]; if (v === undefined) v = ''; }
      return v;
    }
    var m = { getPropertyValue: val, getPropertyPriority: function () { return ''; }, item: function () { return ''; }, length: 0, cssText: '' };
    return new Proxy(m, { get: function (t, n) { if (typeof n !== 'string') return undefined; if (n in t) return t[n]; return val(n); }, set: function () { return true; } });
  };

  /* ---------------------------------------------------------------- temporizadores */
  function chamavel(f) { return typeof f === 'function' ? f : new Function(String(f)); }
  g.setTimeout = function (f, ms) { return N.temporizador(chamavel(f), +ms || 0, 0, Array.prototype.slice.call(arguments, 2)); };
  g.setInterval = function (f, ms) { return N.temporizador(chamavel(f), +ms || 0, 1, Array.prototype.slice.call(arguments, 2)); };
  g.clearTimeout = g.clearInterval = function (id) { if (id != null) N.parar(+id || 0); };
  g.requestAnimationFrame = function (f) { if (typeof f !== 'function') throw new TypeError('requestAnimationFrame: no es una función'); return N.quadro(f); };
  g.cancelAnimationFrame = function (id) { N.sem_quadro(+id || 0); };
  g.requestIdleCallback = function (f) { return g.setTimeout(function () { f({ didTimeout: false, timeRemaining: function () { return 5; } }); }, 1); };
  g.cancelIdleCallback = g.clearTimeout;
  g.queueMicrotask = function (f) { Promise.resolve().then(f); };
  g.performance = { now: function () { return Date.now() - t0; }, timeOrigin: t0, mark: function () {}, measure: function () {}, getEntriesByName: function () { return []; }, getEntriesByType: function () { return []; } };
  /* Promesas que resuelve el navegador (red: D487): [id, promesa]; el navegador llama a __sagui_resolver(id, ok, valor). */
  var pend = {}, nPend = 0;
  g.__sagui_pendente = function () { var id = ++nPend, r; var p = new Promise(function (a, b) { r = { a: a, b: b }; }); pend[id] = r; return [id, p]; };
  g.__sagui_resolver = function (id, ok, v) { var r = pend[id]; if (!r) return; delete pend[id]; if (ok) r.a(v); else r.b(v instanceof Error ? v : new TypeError(String(v))); };

  /* ---------------------------------------------------------------- foco y eventos del navegador */
  var ativo = null, valorAoFocar = null, sobre = null;
  function focavel(e) {
    for (; e && e._tipo === 1; e = e._pai) {
      if (e._atr.tabindex !== undefined || e._atr.contenteditable !== undefined) return e;
      if (/^(input|select|textarea|button)$/.test(e._nome) && e._atr.disabled === undefined) return e;
      if (e._nome === 'a' && e._atr.href !== undefined) return e;
    }
    return null;
  }
  function focar(e) {
    if (e === ativo) return;
    var velho = ativo;
    if (velho) {
      if (valorAoFocar !== null && velho.value !== valorAoFocar && /^(input|textarea|select)$/.test(velho._nome)) despachar(velho, new Event('change', { bubbles: true }));
      despachar(velho, new g.FocusEvent('blur', { relatedTarget: e }));
      despachar(velho, new g.FocusEvent('focusout', { bubbles: true, relatedTarget: e }));
    }
    ativo = e;
    valorAoFocar = e ? e.value : null;
    if (e) {
      despachar(e, new g.FocusEvent('focus', { relatedTarget: velho }));
      despachar(e, new g.FocusEvent('focusin', { bubbles: true, relatedTarget: velho }));
    }
  }
  HE.focus = function () { if (ligado(this)) focar(this); };
  HE.blur = function () { if (ativo === this) focar(null); };
  HE.click = function () { var ev = new g.MouseEvent('click', { bubbles: true, cancelable: true, detail: 1 }); return despachar(this, ev); };
  function ratoEv(tipo, d, extra) {
    var C = /^pointer/.test(tipo) ? g.PointerEvent : g.MouseEvent;
    var o = { bubbles: !/^(mouseenter|mouseleave|pointerenter|pointerleave)$/.test(tipo), cancelable: !/^(mouseenter|mouseleave|pointerenter|pointerleave)$/.test(tipo),
              view: g, clientX: d.x, clientY: d.y, pageX: d.x + janela()[2], pageY: d.y + janela()[3], screenX: d.x, screenY: d.y, x: d.x, y: d.y,
              button: d.botao || 0, buttons: d.botoes || 0, detail: d.detalhe || 0, shiftKey: !!d.shift, ctrlKey: !!d.ctrl, altKey: !!d.alt };
    for (var k in extra) o[k] = extra[k];
    var ev = new C(tipo, o);
    ev.isTrusted = true;
    return ev;
  }
  function trusted(ev) { ev.isTrusted = true; return ev; }
  /* El ratón pasa de un elemento a otro: out/leave en el que deja, over/enter en el nuevo. */
  function moverPara(e, d) {
    if (e === sobre) return;
    var velho = sobre, i;
    sobre = e;
    var vs = [], ns = [];
    for (i = velho; i && i._tipo === 1; i = i._pai) vs.push(i);
    for (i = e; i && i._tipo === 1; i = i._pai) ns.push(i);
    if (velho && ligado(velho)) {
      despachar(velho, ratoEv('mouseout', d, { relatedTarget: e }));
      for (i = 0; i < vs.length; i++) if (ns.indexOf(vs[i]) < 0) despachar(vs[i], ratoEv('mouseleave', d, { relatedTarget: e }));
    }
    if (e) {
      despachar(e, ratoEv('mouseover', d, { relatedTarget: velho }));
      for (i = ns.length - 1; i >= 0; i--) if (vs.indexOf(ns[i]) < 0) despachar(ns[i], ratoEv('mouseenter', d, { relatedTarget: velho }));
    }
  }
  var TECLAS = { Enter: 13, Backspace: 8, Tab: 9, Escape: 27, ' ': 32, ArrowLeft: 37, ArrowUp: 38, ArrowRight: 39, ArrowDown: 40, Delete: 46,
                 Home: 36, End: 35, PageUp: 33, PageDown: 34, F1: 112 };
  function codigo(k) {
    if (/^[a-zA-Z]$/.test(k)) return 'Key' + k.toUpperCase();
    if (/^[0-9]$/.test(k)) return 'Digit' + k;
    if (k === ' ') return 'Space';
    return k.length > 1 ? k : '';
  }
  /* __sagui_evento(id, tipo, datos): lo que hizo la persona. Devuelve 1 si el
   * script canceló la acción del navegador (preventDefault). */
  g.__sagui_evento = function (id, tipo, d) {
    if (typeof d === 'string') { try { d = d ? JSON.parse(d) : {}; } catch (x) { d = {}; } }
    d = d || {};
    var e = id >= 0 ? nos[id] : null, ok = true;
    if (e && e._tipo === 3) e = e._pai;
    switch (tipo) {
      case 'mousemove':
        if (!e) e = doc.body;
        moverPara(e, d);
        if (e) ok = despachar(e, ratoEv('mousemove', d));
        break;
      case 'mousedown':
        if (!e) e = doc.body;
        if (!e) break;
        ok = despachar(e, ratoEv('mousedown', d));
        if (ok) focar(focavel(e));
        break;
      case 'mouseup': case 'click': case 'dblclick': case 'contextmenu': case 'auxclick':
        if (!e) e = doc.body;
        if (!e) break;
        ok = despachar(e, ratoEv(tipo, d));
        break;
      case 'keydown': case 'keyup': case 'keypress':
        e = ativo && ligado(ativo) ? ativo : doc.body || doc;
        var k = d.tecla || '';
        ok = despachar(e, trusted(new g.KeyboardEvent(tipo, { bubbles: true, cancelable: true, key: k, code: codigo(k), keyCode: TECLAS[k] || (k.length === 1 ? k.toUpperCase().charCodeAt(0) : 0),
          which: TECLAS[k] || (k.length === 1 ? k.toUpperCase().charCodeAt(0) : 0), charCode: tipo === 'keypress' && k.length === 1 ? k.charCodeAt(0) : 0,
          shiftKey: !!d.shift, ctrlKey: !!d.ctrl, altKey: !!d.alt, view: g })));
        break;
      case 'input':
        if (!e) break;
        if (d.valor !== undefined) oculto(e, '_valor', String(d.valor));
        if (ativo !== e) focar(e);
        ok = despachar(e, trusted(new g.InputEvent('input', { bubbles: true, data: d.dado || null, inputType: d.tipo || 'insertText' })));
        break;
      case 'change':
        if (!e) break;
        valorAoFocar = e.value;
        ok = despachar(e, trusted(new Event('change', { bubbles: true })));
        break;
      case 'submit':
        var f = e ? e.closest('form') : null;
        if (!f) break;
        ok = despachar(f, trusted(new g.SubmitEvent('submit', { bubbles: true, cancelable: true, submitter: e !== f && /^(button|input)$/.test(e._nome) ? e : null })));
        break;
      case 'focus': if (e) focar(e); break;
      case 'blur': focar(null); break;
      case 'popstate':
        hist.pos = Math.max(0, Math.min(hist.lista.length - 1, hist.pos + (d.delta || 0)));
        var ent = hist.lista[hist.pos];
        if (ent) urlAtual = ent.url;
        despachar(g, trusted(new g.PopStateEvent('popstate', { state: ent ? ent.estado : null })));
        if (d.hash) despachar(g, trusted(new g.HashChangeEvent('hashchange', { oldURL: d.velha || '', newURL: urlAtual })));
        break;
      case 'hashchange':
        var velha = urlAtual; urlAtual = String(d.url || urlAtual);
        hist.lista.splice(hist.pos + 1); hist.lista.push({ url: urlAtual, estado: null }); hist.pos++;
        var anc = urlAtual.indexOf('#') >= 0 ? acharId(decodeURIComponent(urlAtual.replace(/^[^#]*#/, ''))) : null;
        if (anc) anc.scrollIntoView();
        despachar(g, trusted(new g.HashChangeEvent('hashchange', { oldURL: velha, newURL: urlAtual })));
        break;
      case 'resize': despachar(g, trusted(new Event('resize'))); break;
      case 'scroll': despachar(doc, trusted(new Event('scroll', { bubbles: true }))); obsTick(); break;
      default:
        if (e) ok = despachar(e, trusted(new Event(tipo, { bubbles: !!d.bubbles, cancelable: true })));
    }
    return ok ? 0 : 1;
  };

  /* ---------------------------------------------------------------- location e history */
  var urlAtual = String(doc.URL || '');
  function partes(u) {
    var m = /^([a-z][a-z0-9+.\-]*:)?(?:\/\/([^\/?#@]*@)?([^\/?#:]*)(?::(\d+))?)?([^?#]*)(\?[^#]*)?(#.*)?$/i.exec(u) || [];
    return { protocol: (m[1] || '').toLowerCase(), hostname: m[3] || '', port: m[4] || '', pathname: m[5] || '/', search: m[6] && m[6] !== '?' ? m[6] : '', hash: m[7] && m[7] !== '#' ? m[7] : '' };
  }
  function absoluta(r) {
    r = String(r);
    if (/^[a-z][a-z0-9+.\-]*:/i.test(r)) return r;
    var b = partes(urlAtual), orig = b.protocol + '//' + b.hostname + (b.port ? ':' + b.port : '');
    if (r.slice(0, 2) === '//') return b.protocol + r;
    if (r[0] === '#') return urlAtual.replace(/#.*$/, '') + r;
    if (r[0] === '?') return orig + b.pathname + r;
    var cam = r[0] === '/' ? r : b.pathname.replace(/[^\/]*$/, '') + r, seg = [];
    var q = cam.search(/[?#]/), resto = q >= 0 ? cam.slice(q) : '';
    (q >= 0 ? cam.slice(0, q) : cam).split('/').forEach(function (s, i, a) { if (s === '..') { if (seg.length > 1) seg.pop(); } else if (s !== '.' || i === a.length - 1) seg.push(s === '.' ? '' : s); });
    return orig + seg.join('/') + resto;
  }
  g.__sagui_absoluta = absoluta;
  function mesmaOrigem(u) { var a = partes(absoluta(u)), b = partes(urlAtual); return a.protocol === b.protocol && a.hostname === b.hostname && a.port === b.port; }
  var loc = g.location;
  function defLoc(k, f, set) { try { delete loc[k]; } catch (x) {} def(loc, k, f, set); }
  defLoc('href', function () { return urlAtual; }, function (v) { irPara(String(v)); });
  ['protocol', 'hostname', 'port', 'pathname', 'search'].forEach(function (k) { defLoc(k, function () { return partes(urlAtual)[k]; }, function (v) { var p = partes(urlAtual); p[k] = String(v); irPara(p.protocol + '//' + p.hostname + (p.port ? ':' + p.port : '') + p.pathname + p.search + p.hash); }); });
  defLoc('host', function () { var p = partes(urlAtual); return p.hostname + (p.port ? ':' + p.port : ''); });
  defLoc('origin', function () { var p = partes(urlAtual); return p.protocol + '//' + p.hostname + (p.port ? ':' + p.port : ''); });
  defLoc('hash', function () { return partes(urlAtual).hash; }, function (v) { v = String(v); irPara(urlAtual.replace(/#.*$/, '') + (v && v[0] !== '#' ? '#' : '') + v); });
  loc.assign = function (u) { irPara(String(u)); };
  loc.replace = function (u) { irPara(String(u), true); };
  loc.reload = function () { N.navegar(urlAtual); };
  loc.toString = function () { return urlAtual; };
  /* Cambiar solo el #: la misma página (hashchange); si no, otra página. */
  function irPara(u, subst) {
    var nova = absoluta(u);
    if (nova.replace(/#.*$/, '') === urlAtual.replace(/#.*$/, '') && nova.indexOf('#') >= 0) {
      if (nova === urlAtual) return;
      var velha = urlAtual;
      urlAtual = nova;
      if (subst) hist.lista[hist.pos] = { url: nova, estado: null };
      else { hist.lista.splice(hist.pos + 1); hist.lista.push({ url: nova, estado: null }); hist.pos++; }
      N.historia(nova, subst ? 1 : 0);
      var alvo = acharId(decodeURIComponent(nova.replace(/^[^#]*#/, '')));
      if (alvo) alvo.scrollIntoView();
      g.setTimeout(function () { despachar(g, trusted(new g.HashChangeEvent('hashchange', { oldURL: velha, newURL: nova }))); }, 0);
      return;
    }
    N.navegar(nova);
  }
  var hist = { lista: [{ url: urlAtual, estado: null }], pos: 0 };
  function clonarEstado(s) { try { return s === undefined ? null : JSON.parse(JSON.stringify(s)); } catch (x) { return s; } }
  function mudarHist(s, u, subst) {
    var nova = u === undefined || u === null ? urlAtual : absoluta(String(u));
    if (!mesmaOrigem(nova)) throw new DOMException('pushState: otra origen', 'SecurityError');
    var e = { url: nova, estado: clonarEstado(s) };
    if (subst) hist.lista[hist.pos] = e; else { hist.lista.splice(hist.pos + 1); hist.lista.push(e); hist.pos++; }
    urlAtual = nova;
    N.historia(nova, subst ? 1 : 0);
  }
  g.history = {
    pushState: function (s, t, u) { mudarHist(s, u, false); },
    replaceState: function (s, t, u) { mudarHist(s, u, true); },
    back: function () { N.ir(-1); }, forward: function () { N.ir(1); }, go: function (d) { N.ir(+d || 0); },
    scrollRestoration: 'auto'
  };
  def(g.history, 'state', function () { var e = hist.lista[hist.pos]; return e ? e.estado : null; });
  def(g.history, 'length', function () { return hist.lista.length; });
  def(doc, 'URL', function () { return urlAtual; });
  def(doc, 'documentURI', function () { return urlAtual; });
  def(doc, 'baseURI', function () { return urlAtual; });
  def(doc, 'domain', function () { return partes(urlAtual).hostname; });
  def(doc, 'referrer', function () { return ''; });
  g.__sagui_url = function (u) { urlAtual = String(u); hist = { lista: [{ url: urlAtual, estado: null }], pos: 0 }; };

  /* ---------------------------------------------------------------- observadores y carga */
  var obsI = [];
  g.IntersectionObserver = function (cb, o) { this._cb = cb; this._els = []; this._ant = new Map(); obsI.push(this); this.root = null; this.rootMargin = (o && o.rootMargin) || '0px'; this.thresholds = [0]; };
  var IO = g.IntersectionObserver.prototype;
  IO.observe = function (el) { if (this._els.indexOf(el) < 0) { this._els.push(el); var s = this; g.setTimeout(function () { obsCorrer(s, true); }, 0); } };
  IO.unobserve = function (el) { var i = this._els.indexOf(el); if (i >= 0) this._els.splice(i, 1); this._ant.delete(el); };
  IO.disconnect = function () { this._els = []; this._ant = new Map(); };
  IO.takeRecords = function () { return []; };
  function obsCorrer(ob, sempre) {
    if (!ob._els.length) return;
    var j = janela(), recs = [];
    ob._els.forEach(function (el) {
      var r = el.getBoundingClientRect(), dentro = r.bottom > 0 && r.top < j[1] && r.right > 0 && r.left < j[0] && (r.width > 0 || r.height > 0);
      if (!sempre && ob._ant.get(el) === dentro) return;
      if (sempre && ob._ant.has(el) && ob._ant.get(el) === dentro) return;
      ob._ant.set(el, dentro);
      recs.push({ target: el, isIntersecting: dentro, intersectionRatio: dentro ? 1 : 0, time: Date.now() - t0, boundingClientRect: r, intersectionRect: dentro ? r : rect(0, 0, 0, 0), rootBounds: rect(0, 0, j[0], j[1]) });
    });
    if (recs.length) { try { ob._cb(recs, ob); } catch (x) { erro('IntersectionObserver', x); } }
  }
  function obsTick() { obsI.forEach(function (o) { obsCorrer(o, false); }); }
  g.ResizeObserver = function (cb) { this._cb = cb; this._els = []; };
  g.ResizeObserver.prototype.observe = function (el) {
    var s = this; s._els.push(el);
    g.setTimeout(function () { var r = el.getBoundingClientRect(); try { s._cb([{ target: el, contentRect: r, borderBoxSize: [{ inlineSize: r.width, blockSize: r.height }], contentBoxSize: [{ inlineSize: r.width, blockSize: r.height }] }], s); } catch (x) { erro('ResizeObserver', x); } }, 0);
  };
  g.ResizeObserver.prototype.unobserve = function (el) { var i = this._els.indexOf(el); if (i >= 0) this._els.splice(i, 1); };
  g.ResizeObserver.prototype.disconnect = function () { this._els = []; };
  var cargado = false;
  g.__sagui_carga = function () {
    if (cargado) return;
    cargado = true;
    doc.readyState = 'interactive';
    despachar(doc, trusted(new Event('readystatechange')));
    despachar(doc, trusted(new Event('DOMContentLoaded', { bubbles: true })));
    doc.readyState = 'complete';
    despachar(doc, trusted(new Event('readystatechange')));
    despachar(g, trusted(new Event('load')));
    var b = doc.body;
    if (b && b._atr.onload !== undefined && typeof g.onload !== 'function') { var h = manejador(b, 'load'); if (h) { try { h.call(g, new Event('load')); } catch (x) { erro('onload', x); } } }
  };
  g.Window = function () {};
})(this);
