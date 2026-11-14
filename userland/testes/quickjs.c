// SPDX-License-Identifier: GPL-2.0-or-later
/* quickjs.c - programa de prova do QuickJS dentro do SaguiOS (D149).
 *
 * Corre o QuickJS (bellard/quickjs, MIT, portado em userland/web/quickjs/)
 * e escreve no registo o que cada expressao deu. Sem argumentos, corre uma
 * serie de pruebas: a aritmetica, os objectos, as funcoes, as classes, as
 * expressoes regulares, o JSON, a data, o console, o alert, o document e os
 * temporizadores. Com um argumento, corre o script que se der.
 */
#include "sagui.h"
#include "qjs_bridge.h"

/* O porte do QuickJS traz o strlen (o SO nao tem libc). */
size_t strlen(const char *s);

static void hook_titulo(const char *t)  { registrar("quickjs: titulo = %s\n", t); }
static void hook_corpo(const char *html) { registrar("quickjs: corpo recebido: %s\n", html); }
static void hook_alerta(const char *t)  { registrar("quickjs: alerta recebida: %s\n", t); }
static void hook_log(const char *t)      { registrar("quickjs: console: %s\n", t); }
static void hook_navegar(const char *url) { registrar("quickjs: navegar: %s\n", url); }

/* As pruebas: cada uma com o que se espera ver no registo. */
static const struct { const char *nome; const char *codigo; } provas[] = {
    { "aritmetica", "1 + 2 * 3" },
    { "texto", "'ola ' + 'mundo'" },
    { "math", "Math.round(Math.PI * 100) / 100" },
    { "sqrt", "Math.sqrt(144)" },
    { "potencia", "Math.pow(2, 16)" },
    { "objeto", "({a: 1, b: 'dois'}).b" },
    { "array", "[3,1,2].sort().join('-')" },
    { "seta", "((x) => x * 2)(21)" },
    { "fecho", "(function(){ var n = 0; return function(){ return ++n; }; })()()" },
    { "classe", "class P { constructor(n){ this.n = n; } dobro(){ return this.n * 2; } } new P(21).dobro()" },
    { "json", "JSON.stringify({a: 1, b: [2, 3]})" },
    { "regex", "'abc123'.replace(/[0-9]+/, '#')" },
    { "data", "typeof Date.now()" },
    { "console", "console.log('mensagem', 42); 'feito'" },
    { "alerta", "alert('Aviso do script'); 'alerta enviada'" },
    { "titulo", "document.title = 'Titulo do script'; document.title" },
    { "corpo", "document.body.innerHTML = '<p>ola</p>'; document.body.innerHTML" },
    { "temporizador", "var x = 0; setTimeout(function(){ x = 7; console.log('temporizador disparou'); }, 10); 'agendado'" },
    { "janela", "typeof window" },
    { "endereco", "location.href" },
    { "url_doc", "document.URL" },
    { "agente", "navigator.userAgent" },
    { "reenvio", "window.parent.location.replace('https://exemplo.test/pagina'); 'pedido'" },
    { "atribuir", "location.assign('https://exemplo.test/outra'); 'pedido'" },
};

int principal(int argc, char **argv)
{
    /* A ordem dos campos de qjs_ganchos_t é: titulo, corpo, alerta, log e
     * navegar. Se põem os cinco, porque um campo de menos desloca os outros. */
    qjs_ganchos_t g = { hook_titulo, hook_corpo, hook_alerta, hook_log, hook_navegar };
    qjs_ponte_t *p = qjs_criar(&g);
    if (!p) { registrar("quickjs: nao ha memoria para o motor\n"); return 1; }
    qjs_definir_url(p, "https://exemplo.test/inicio");

    if (argc > 1) {
        int e = qjs_executar(p, argv[1], (int)strlen(argv[1]), argv[1]);
        registrar("quickjs: codigo %d\n", e);
        qjs_destruir(p);
        return e ? 1 : 0;
    }

    int erros = 0;
    for (unsigned i = 0; i < sizeof(provas) / sizeof(provas[0]); i++) {
        const char *codigo = provas[i].codigo;
        int n = (int)strlen(codigo);
        char valor[256];
        if (qjs_executar_texto(p, codigo, n, provas[i].nome, valor, sizeof valor) != 0) {
            erros++;
            continue;
        }
        registrar("quickjs: %s = %s\n", provas[i].nome, valor);
    }

    /* o temporizador: passa o tempo e o script escreve no console */
    qjs_tique(p, 100);
    registrar("quickjs: temporizadores ok\n");

    qjs_destruir(p);
    registrar("quickjs: erros %d\n", erros);
    return erros ? 1 : 0;
}
