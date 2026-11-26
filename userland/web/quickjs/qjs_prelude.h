// SPDX-License-Identifier: GPL-2.0-or-later
/* qjs_prelude.h - lo que el puente del QuickJS pone antes del DOM (D271, D488).
 *
 * Es JavaScript que corre una vez al crear el motor de la pagina. Desde la D488 casi todo lo que
 * habia aqui (temporizadores, eventos, observadores, history, los elementos) esta en qjs_dom.js, y
 * desde la D487 la red y el almacenamiento (fetch, XMLHttpRequest, localStorage, document.cookie)
 * en qjs_rede.js: aqui quedan matchMedia, screen y navigator, que no dependen del DOM.
 *
 * El texto esta en ASCII, una linea de JavaScript por linea de C. */
#ifndef SAGUI_QJS_PRELUDE_H
#define SAGUI_QJS_PRELUDE_H

static const char qjs_prelude[] =
    "(function (g) {\n"
    "  'use strict';\n"
    "  if (!g.innerWidth) { g.innerWidth = 800; g.innerHeight = 600; }\n"
    "  g.devicePixelRatio = 1;\n"
    "  /* ---- matchMedia: anchura, altura, orientacion y esquema de color ---- */\n"
    "  g.matchMedia = function (q) {\n"
    "    q = String(q);\n"
    "    function una(s) {\n"
    "      var ok = !/\\b(print|speech)\\b/.test(s), nao = /^\\s*not\\b/.test(s);\n"
    "      s.replace(/\\(\\s*([a-z-]+)\\s*(?::\\s*([^)]+))?\\)/g, function (_, n, v) {\n"
    "        var x = parseFloat(v);\n"
    "        if (/r?em\\s*$/.test(v || '')) x *= 16;\n"
    "        if (n === 'min-width') ok = ok && g.innerWidth >= x;\n"
    "        else if (n === 'max-width') ok = ok && g.innerWidth <= x;\n"
    "        else if (n === 'min-height') ok = ok && g.innerHeight >= x;\n"
    "        else if (n === 'max-height') ok = ok && g.innerHeight <= x;\n"
    "        else if (n === 'prefers-color-scheme') ok = ok && /light/.test(v);\n"
    "        else if (n === 'orientation') ok = ok && (/landscape/.test(v) === (g.innerWidth >= g.innerHeight));\n"
    "        else if (n === 'prefers-reduced-motion') ok = ok && /no-preference/.test(v);\n"
    "        else if (n !== 'hover' && n !== 'pointer') ok = false;\n"
    "        return '';\n"
    "      });\n"
    "      return nao ? !ok : ok;\n"
    "    }\n"
    "    var r = { matches: q.split(',').some(una), media: q, onchange: null, addListener: function () {}, removeListener: function () {} };\n"
    "    r.addEventListener = r.removeEventListener = function () {};\n"
    "    r.dispatchEvent = function () { return true; };\n"
    "    return r;\n"
    "  };\n"
    "  g.screen = { width: g.innerWidth, height: g.innerHeight, availWidth: g.innerWidth, availHeight: g.innerHeight, colorDepth: 24 };\n"
    "  if (g.navigator) { g.navigator.language = 'es'; g.navigator.languages = ['es']; g.navigator.onLine = true; }\n"
    "})(this);\n";

#endif
