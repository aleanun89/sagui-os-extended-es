// qjs_prelude.js - la parte de qjs escrita en JavaScript (D188): console,
// print e inspect (lo que muestra el REPL). Se ejecuta antes del script.
// __salida(fd, texto) es nativa (convierte UTF-8 a Latin-1 del terminal).
(function (g) {
  var fmtStr = function (s) {
    return "'" + s.replace(/\\/g, "\\\\").replace(/'/g, "\\'").replace(/\n/g, "\\n") + "'";
  };
  function nombreClase(v) {
    var p = Object.getPrototypeOf(v);
    if (p === null) return "[Object: null prototype]";
    var c = p.constructor;
    return c && c.name ? c.name : "Object";
  }
  function ins(v, prof, vistos, raiz) {
    var t = typeof v;
    if (t === "string") return raiz ? v : fmtStr(v);
    if (t === "bigint") return v + "n";
    if (t === "symbol") return v.toString();
    if (v === null) return "null";
    if (t === "number") return Object.is(v, -0) ? "-0" : String(v);
    if (t === "function") {
      var d = Function.prototype.toString.call(v);
      if (/^class[\s{]/.test(d)) return "[class " + (v.name || "(anonymous)") + "]";
      return "[Function: " + (v.name || "(anonymous)") + "]";
    }
    if (t !== "object") return String(v);
    if (vistos.indexOf(v) >= 0) return "[Circular]";
    if (v instanceof Error) return v.stack ? String(v.stack).replace(/\n$/, "") : String(v);
    if (v instanceof Date) return isNaN(v.getTime()) ? "Invalid Date" : v.toISOString();
    if (v instanceof RegExp) return String(v);
    if (prof > 3) return Array.isArray(v) ? "[Array]" : "[Object]";
    vistos.push(v);
    var partes = [], ab = "{", ce = "}", pre = "", i;
    if (Array.isArray(v)) {
      ab = "["; ce = "]";
      var huecos = 0;
      for (i = 0; i < v.length && partes.length < 100; i++) {
        if (!(i in v)) { huecos++; continue; }
        if (huecos) { partes.push("<" + huecos + " empty item" + (huecos > 1 ? "s" : "") + ">"); huecos = 0; }
        partes.push(ins(v[i], prof + 1, vistos, false));
      }
      if (huecos) partes.push("<" + huecos + " empty item" + (huecos > 1 ? "s" : "") + ">");
      if (v.length > 100) partes.push("... " + (v.length - 100) + " more items");
      for (var k in v) if (!/^\d+$/.test(k) && Object.prototype.hasOwnProperty.call(v, k))
        partes.push(k + ": " + ins(v[k], prof + 1, vistos, false));
    } else if (v instanceof Map) {
      pre = "Map(" + v.size + ") ";
      v.forEach(function (val, key) {
        partes.push(ins(key, prof + 1, vistos, false) + " => " + ins(val, prof + 1, vistos, false));
      });
    } else if (v instanceof Set) {
      pre = "Set(" + v.size + ") ";
      v.forEach(function (val) { partes.push(ins(val, prof + 1, vistos, false)); });
    } else if (v instanceof Promise) {
      pre = "Promise ";
      partes.push("<pending>");
    } else {
      var nc = nombreClase(v);
      if (nc !== "Object") pre = nc + " ";
      var ks = Object.keys(v);
      for (i = 0; i < ks.length; i++) {
        var kk = /^[A-Za-z_$][\w$]*$/.test(ks[i]) ? ks[i] : fmtStr(ks[i]);
        partes.push(kk + ": " + ins(v[ks[i]], prof + 1, vistos, false));
      }
    }
    vistos.pop();
    if (partes.length === 0) return pre + ab + ce;
    var una = pre + ab + " " + partes.join(", ") + " " + ce;
    if (una.length <= 72 && una.indexOf("\n") < 0) return una;
    var sang = "  ";
    for (i = 0; i < prof; i++) sang += "  ";
    var cierre = sang.slice(2);
    return pre + ab + "\n" + sang + partes.join(",\n" + sang) + "\n" + cierre + ce;
  }
  function inspect(v) { return ins(v, 0, [], false); }
  function formatear(args) {
    var r = [];
    for (var i = 0; i < args.length; i++) r.push(ins(args[i], 0, [], true));
    return r.join(" ");
  }
  var sangria = "";
  function escribir(fd, args) {
    var s = formatear(args);
    if (sangria) s = s.split("\n").map(function (l) { return sangria + l; }).join("\n");
    __salida(fd, s + "\n");
  }
  var cuentas = {};
  g.console = {
    log: function () { escribir(1, arguments); },
    info: function () { escribir(1, arguments); },
    debug: function () { escribir(1, arguments); },
    error: function () { escribir(2, arguments); },
    warn: function () { escribir(2, arguments); },
    trace: function () { escribir(2, arguments); },
    assert: function (c) {
      if (!c) { var a = Array.prototype.slice.call(arguments, 1); a.unshift("Assertion failed:"); escribir(2, a); }
    },
    group: function () { if (arguments.length) escribir(1, arguments); sangria += "  "; },
    groupEnd: function () { sangria = sangria.slice(2); },
    count: function (e) { e = e === undefined ? "default" : String(e); cuentas[e] = (cuentas[e] || 0) + 1; escribir(1, [e + ": " + cuentas[e]]); },
    dir: function (v) { __salida(1, inspect(v) + "\n"); },
    table: function (v) { __salida(1, inspect(v) + "\n"); }
  };
  g.print = function () {
    var r = [];
    for (var i = 0; i < arguments.length; i++) r.push(typeof arguments[i] === "string" ? arguments[i] : String(arguments[i]));
    __salida(1, r.join(" ") + "\n");
  };
  g.__inspect = inspect;
})(globalThis);
