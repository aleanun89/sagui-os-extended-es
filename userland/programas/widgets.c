// SPDX-License-Identifier: GPL-3.0-only
/* widgets - el gestor de widgets de escritorio (D365): lista los disponibles, activa o quita cada uno y
 * cambia sus opciones; lo que queda activo se guarda en /home/.config/widgets.conf y arranca solo al
 * iniciar la sesión.
 *
 *   widgets              abre la ventana del gestor (menú del escritorio «Añadir widgets...», Configuración)
 *   widgets --arrancar   lo usa el arranque de la sesión: lanza los widgets activos y termina
 *   widgets --lista      escribe la lista y su estado en el terminal */
#include "widget.h"

#define LARG 540
#define ALT  360

static const char *base_de(const char *c)
{
    const char *b = c;
    for (const char *p = c; *p; p++) if (*p == '/') b = p + 1;
    return b;
}

/* ¿Hay un proceso con el nombre del programa? Devuelve su pid o 0. */
static int pid_de(const char *programa)
{
    static info_processo_t v[64];
    int n = info_sistema(INFO_PROCESSOS, v, 64);
    const char *nome = base_de(programa);
    for (int i = 0; i < n; i++)
        if (v[i].estado != 4 && compara_texto(v[i].nome, nome) == 0) return v[i].pid;
    return 0;
}

static void lancar(const widget_def_t *w)
{
    if (pid_de(w->programa)) return;
    char *argv[] = { (char *)base_de(w->programa), NULL };
    int pid = executar(w->programa, argv);
    registrar("[WIDGETS] %s -> %s (pid %d)\n", w->chave, w->programa, pid);
}

static void parar(const widget_def_t *w)
{
    int pid;
    while ((pid = pid_de(w->programa)) != 0) {
        matar(pid);
        dormir(50);
        if (pid_de(w->programa) == pid) break;
    }
}

static int arrancar(void)
{
    int n = 0;
    for (int i = 0; i < widget_qtd; i++)
        if (widget_ativo(widget_lista[i].chave)) { lancar(&widget_lista[i]); n++; }
    registrar("[WIDGETS] arranque de la sesión: %d activos\n", n);
    return 0;
}

static jan_t *jn;
static lista_t lista;
static char itens[8][64];
static botao_t b_toggle = { 16, 212, 160, 28, "Activar", false };
static botao_t b_opac = { 188, 212, 160, 28, "Opacidad", false };
static botao_t b_extra = { 360, 212, 164, 28, "Modo", false };
static botao_t b_fechar = { LARG - 116, ALT - 44, 100, 28, "Cerrar", false };
static int ult_estado[8];

static int estado_de(int i)
{
    return widget_ativo(widget_lista[i].chave) ? (pid_de(widget_lista[i].programa) ? 2 : 1) : 0;
}

static void montar_lista(void)
{
    for (int i = 0; i < widget_qtd; i++) {
        int e = estado_de(i);
        ult_estado[i] = e;
        formatar_texto(itens[i], sizeof itens[i], "%s%s", e ? (e == 2 ? "[x] " : "[!] ") : "[ ] ", _(widget_lista[i].nome));
    }
    lista.qtd = widget_qtd;
}

static void desenhar(void)
{
    g_retangulo(jn, 0, 0, LARG, ALT, COR_FUNDO);
    g_texto(jn, 16, 12, _("Widgets de escritorio"), COR_TEXTO);
    g_texto(jn, 16, 30, _("Elige cuáles quieres ver. Los activos arrancan al iniciar la sesión."), COR_TEXTO_FRACO);
    montar_lista();
    lista_desenhar(jn, &lista);
    const widget_def_t *w = &widget_lista[lista.sel];
    g_texto(jn, 16, 186, _(w->descricao), COR_TEXTO);
    int e = ult_estado[lista.sel];
    b_toggle.texto = e ? _("Quitar") : _("Añadir");
    botao_desenhar(jn, &b_toggle);
    char t[40];
    char k[40];
    formatar_texto(k, sizeof k, "%s.opacidad", w->chave);
    int op = widget_ler_int(k, 100);
    formatar_texto(t, sizeof t, "%s: %d%%", _("Opacidad"), op);
    b_opac.texto = t;
    botao_desenhar(jn, &b_opac);
    bool reloj = compara_texto(w->chave, "relogio") == 0, conky = compara_texto(w->chave, "conky") == 0;
    char modo[16] = "";
    if (reloj) { widget_ler("relogio.modo", modo, sizeof modo); b_extra.texto = compara_texto(modo, "digital") == 0 ? _("Reloj: digital") : _("Reloj: analógico"); }
    if (conky) b_extra.texto = _("Editar conky.conf");
    if (reloj || conky) botao_desenhar(jn, &b_extra);
    g_texto(jn, 16, 262, _("Para mover un widget: Alt + arrastrar, o «Editar widgets» en el menú del escritorio."), COR_TEXTO_FRACO);
    g_texto(jn, 16, 280, _("Clic derecho sobre un widget: moverlo o quitarlo."), COR_TEXTO_FRACO);
    g_texto(jn, 16, 298, _("[x] en el escritorio   [!] activo pero parado   [ ] sin usar"), COR_TEXTO_FRACO);
    botao_desenhar(jn, &b_fechar);
    jan_mostrar(jn);
}

static void abrir_conky_conf(void)
{
    char *argv[] = { "editor", "/home/.config/conky.conf", NULL };
    executar("/bin/editor", argv);
}

static int gestor(void)
{
    jn = jan_abrir(_("Widgets"), LARG, ALT);
    if (!jn) return 1;
    lista.x = 16; lista.y = 56; lista.l = LARG - 32; lista.a = 120;
    lista.itens = itens;
    lista.qtd = widget_qtd;
    lista.sel = 0;
    desenhar();
    evento_t e;
    for (;;) {
        int r = esperar_evento(jn->id, &e, 2000);
        if (r != 1) {   /* cada 2 s: ¿cambió el estado de algún widget (arrancó o murió)? */
            bool mudou = false;
            for (int i = 0; i < widget_qtd; i++) if (estado_de(i) != ult_estado[i]) mudou = true;
            if (mudou) desenhar();
            continue;
        }
        if (e.tipo == EV_FECHAR) break;
        const widget_def_t *w = &widget_lista[lista.sel];
        bool redib = false;
        if (lista_tratar(jn, &lista, &e) != LISTA_NADA) redib = true;
        if (botao_tratar(jn, &b_fechar, &e)) break;
        if (botao_tratar(jn, &b_toggle, &e)) {
            if (widget_ativo(w->chave)) { widget_ativar(w->chave, false); parar(w); }
            else { widget_ativar(w->chave, true); lancar(w); }
            redib = true;
        }
        if (botao_tratar(jn, &b_opac, &e)) {
            char k[40];
            formatar_texto(k, sizeof k, "%s.opacidad", w->chave);
            int op = widget_ler_int(k, 100);
            op = op > 90 ? 90 : op > 75 ? 75 : op > 50 ? 50 : 100;
            widget_gravar_int(k, op);
            if (pid_de(w->programa)) { parar(w); lancar(w); }   /* la opacidad se aplica al abrir la ventana */
            redib = true;
        }
        if (botao_tratar(jn, &b_extra, &e)) {
            if (compara_texto(w->chave, "relogio") == 0) {
                char modo[16] = "";
                widget_ler("relogio.modo", modo, sizeof modo);
                widget_gravar("relogio.modo", compara_texto(modo, "digital") == 0 ? "analogico" : "digital");
                if (pid_de(w->programa)) { parar(w); lancar(w); }
            } else if (compara_texto(w->chave, "conky") == 0) abrir_conky_conf();
            redib = true;
        }
        if (redib) desenhar();
    }
    jan_fechar(jn);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc >= 2 && compara_texto(argv[1], "--arrancar") == 0) return arrancar();
    if (argc >= 2 && compara_texto(argv[1], "--lista") == 0) {
        for (int i = 0; i < widget_qtd; i++) {
            int e = estado_de(i);
            imprimir("%-12s %s\n", widget_lista[i].chave, e == 2 ? "activo (en marcha)" : e == 1 ? "activo (parado)" : "inactivo");
        }
        return 0;
    }
    return gestor();
}
