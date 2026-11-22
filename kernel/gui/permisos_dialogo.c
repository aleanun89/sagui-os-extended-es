// SPDX-License-Identifier: GPL-2.0-or-later
/* permisos_dialogo.c - el diálogo de permisos de administrador (D269, mejora 102, issue #16).
 *
 * Un programa gráfico que necesita ser root pide SERV_PEDIR con PED_GRAFICO. El supervisor.c del kernel
 * deja el pedido esperando a la persona y este módulo, que forma parte del gestor de ventanas (el
 * componente de confianza), pinta aquí el diálogo, recoge la contraseña de root y se la entrega al
 * gestor de servicios. Garantías:
 *   - el programa que pide no recibe ni ve la contraseña (nunca sale de este módulo y del búfer del
 *     pedido en el kernel, que se borra al entregarse) y no recibe teclas ni ratón mientras dura;
 *   - el nombre del programa lo pone el kernel (processo_t), no el programa;
 *   - ninguna ventana puede imitarlo del todo: este diálogo atenúa también la barra superior y el
 *     cursor, que ninguna ventana puede cubrir, y va por encima de las demás ventanas, del aviso y de
 *     los menús;
 *   - modal: mientras está abierto, el ratón y el teclado son solo suyos (incluido Alt+Tab).
 * Esc = No. Primera vez (root sin contraseña): pide elegirla dos veces. */
#include "permisos_dialogo.h"
#include "abi.h"
#include "gui.h"
#include "aparencia.h"
#include "idioma.h"
#include "console.h"
#include "mouse.h"
#include "supervisor.h"
#include "teclado.h"
#include "texto.h"
#include "video.h"
#include "telas.h"

#define DW        500
#define SENHA_MIN 4
#define COR_AZUL  (esquema()->barra_propia ? esquema()->titulo_a1 : 0x1F5FAF)   /* D428: con un esquema propio, los colores del esquema */
#define COR_FUNDO (esquema()->barra_propia ? esquema()->fundo : 0xF2F4F7)

static bool ativo, comprobando, enviado;
static uint32_t id;
static dialogo_vista_t vista;
static char senha[2][PED_SENHA_MAX];
static int  slen[2];
static int  foco;
static char msg[80];   /* error o ayuda bajo los campos */

typedef struct { int x, y, l, a; } ret_t;
typedef struct {
    ret_t caixa, campo[2], si, nao;
    int   y_motivo, n_motivo, y_msg, y_rotulo[2];
    int   campos;
} lay_t;

static const char *rotulo_senha(int k)
{
    if (!vista.primeira) return _("Contraseña de administrador (root):");
    return k == 0 ? _("Elige la contraseña de administrador (root):") : _("Repítela:");
}

/* Parte el motivo en líneas de n columnas: devuelve el número (máx. 3) y los cortes en ini/tam. */
static int partir(const char *t, int cols, int ini[3], int tam[3])
{
    int q = 0;
    while (*t && q < 3) {
        int n = 0;
        while (t[n] && n < cols) n++;
        int salto = 0;
        if (t[n]) {
            int k = n;
            while (k > 0 && t[k] != ' ') k--;
            if (k > 0) { n = k; salto = 1; }
        }
        ini[q] = (int)(t - vista.motivo);
        tam[q] = n;
        q++;
        t += n + salto;
    }
    return q;
}

static void layout(lay_t *L)
{
    int sw, sh;
    telas_principal_rect(&sw, &sh);   /* D290: el diálogo va en la pantalla principal */
    L->campos = vista.primeira ? 2 : 1;
    int ini[3], tam[3];
    L->n_motivo = vista.motivo[0] ? partir(vista.motivo, (DW - 96) / 8, ini, tam) : 0;
    int h = 30 + 16 + 20 + 20 + L->n_motivo * 16 + 22 + L->campos * 50 + 24 + 52 + 22;
    if (h < 250) h = 250;
    L->caixa = (ret_t){ (sw - DW) / 2, (sh - h) / 2, DW, h };
    int y = L->caixa.y + 30 + 16 + 40;
    L->y_motivo = y;
    y += L->n_motivo * 16 + 14;
    for (int k = 0; k < L->campos; k++) {
        L->y_rotulo[k] = y;
        L->campo[k] = (ret_t){ L->caixa.x + 24, y + 20, DW - 48, 24 };
        y += 50;
    }
    L->y_msg = y;
    L->nao = (ret_t){ L->caixa.x + DW - 24 - 90, L->caixa.y + h - 60, 90, 28 };
    L->si = (ret_t){ L->nao.x - 12 - 130, L->nao.y, 130, 28 };
}

bool pd_ativo(void) { return ativo; }

static void tela_toda(void) { gui_redesenhar(0, 0, (int)video_largura(), (int)video_altura()); }

static void apagar_campos(void)
{
    memset(senha, 0, sizeof senha);
    slen[0] = slen[1] = 0;
    foco = 0;
}

static void texto_erro(int e)
{
    msg[0] = 0;
    if (e == ERRO_ACESSO) txt_copia_n(msg, _("Contraseña incorrecta. Inténtalo otra vez."), sizeof msg);
    else if (e == ERRO_ESPERA) txt_copia_n(msg, _("Demasiados fallos: espera un minuto para volver a intentarlo."), sizeof msg);
    else if (e) txt_copia_n(msg, _("No se pudo comprobar la contraseña."), sizeof msg);
}

static void abrir(const dialogo_vista_t *v)
{
    vista = *v;
    id = v->id;
    apagar_campos();
    enviado = false;
    comprobando = v->comprobando;
    texto_erro(v->erro);
    ativo = true;
    klog("[GUI] dialogo de permisos: %s (pid %d)%s\n", v->nome, v->pid, v->primeira ? ", primera contraseña de root" : "");
    tela_toda();
}

static void fechar(void)
{
    apagar_campos();
    ativo = false;
    comprobando = enviado = false;
    klog("[GUI] dialogo de permisos cerrado\n");
    tela_toda();
}

void pd_tique(void)
{
    dialogo_vista_t v;
    bool tem = supervisor_dialogo_vista(&v);
    if (!ativo) { if (tem) abrir(&v); return; }
    if (!tem) { fechar(); return; }
    if (v.id != id) { fechar(); abrir(&v); return; }
    bool mudou = false;
    if (v.comprobando != comprobando) { comprobando = v.comprobando; mudou = true; }
    if (enviado && !v.comprobando) {   /* el supervisor lo devolvió: fallo, se pregunta de nuevo */
        enviado = false;
        apagar_campos();
        texto_erro(v.erro);
        mudou = true;
    }
    vista = v;
    if (mudou) tela_toda();
}

static void negar(void)
{
    if (comprobando) return;
    supervisor_dialogo_negar(id);
    fechar();
}

static void aceitar(void)
{
    if (comprobando) return;
    if (vista.primeira) {
        if (slen[0] < SENHA_MIN) { txt_copia_n(msg, _("Elige al menos 4 caracteres."), sizeof msg); tela_toda(); return; }
        if (slen[0] != slen[1] || txt_compara(senha[0], senha[1]) != 0) {
            txt_copia_n(msg, _("Las dos contraseñas no coinciden."), sizeof msg);
            memset(senha, 0, sizeof senha); slen[0] = slen[1] = 0; foco = 0;
            tela_toda();
            return;
        }
        supervisor_dialogo_enviar(id, "", senha[0]);
    } else {
        if (slen[0] == 0) { txt_copia_n(msg, _("Escribe la contraseña de administrador."), sizeof msg); tela_toda(); return; }
        supervisor_dialogo_enviar(id, senha[0], "");
    }
    memset(senha, 0, sizeof senha);
    slen[0] = slen[1] = 0;
    msg[0] = 0;
    enviado = true;
    comprobando = true;
    tela_toda();
}

bool pd_tecla(int t)
{
    if (!ativo) return false;
    if (t == TECLA_ESC) { negar(); return true; }
    if (comprobando) return true;
    int campos = vista.primeira ? 2 : 1;
    if (t == '\n' || t == '\r') {
        if (campos == 2 && foco == 0) foco = 1; else aceitar();
        tela_toda();
        return true;
    }
    if (t == '\t' || t == TECLA_BAIXO || t == TECLA_CIMA) { foco = campos == 2 ? 1 - foco : 0; tela_toda(); return true; }
    if (t == 8 || t == 127) {
        if (slen[foco] > 0) senha[foco][--slen[foco]] = 0;
        tela_toda();
        return true;
    }
    if (t >= 32 && t < 256 && t != 127 && slen[foco] < PED_SENHA_MAX - 1) {
        senha[foco][slen[foco]++] = (char)t;
        tela_toda();
    }
    return true;   /* nada más llega a las ventanas */
}

static bool dentro(ret_t r, int x, int y) { return x >= r.x && x < r.x + r.l && y >= r.y && y < r.y + r.a; }

bool pd_mouse(int x, int y, int botoes, int mudaram)
{
    if (!ativo) return false;
    if (!(mudaram & botoes & BOTAO_ESQUERDO)) return true;
    lay_t L;
    layout(&L);
    if (dentro(L.nao, x, y)) negar();
    else if (dentro(L.si, x, y)) aceitar();
    else for (int k = 0; k < L.campos; k++)
        if (dentro(L.campo[k], x, y) && !comprobando) { foco = k; tela_toda(); }
    return true;
}

static void botao(superficie_t *t, ret_t r, const char *txt, uint32_t cor, bool ativo_b)
{
    sup_retangulo(t, r.x, r.y, r.l, r.a, ativo_b ? cor : 0x9AA3AE);
    sup_moldura(t, r.x, r.y, r.l, r.a, (esquema()->barra_propia ? esquema()->texto : 0x101820));
    sup_texto(t, r.x + (r.l - texto_largura(txt)) / 2, r.y + (r.a - 16) / 2, txt, 0xFFFFFF);
}

void pd_desenhar(superficie_t *t)
{
    if (!ativo) return;
    /* Atenúa todo lo que hay debajo (ventanas, barra superior, avisos, menús). */
    for (int y = t->cy0; y < t->cy1; y++) {
        uint32_t *f = t->px + (size_t)y * (size_t)t->larg;
        for (int x = t->cx0; x < t->cx1; x++) f[x] = (f[x] >> 1) & 0x7F7F7F;
    }
    lay_t L;
    layout(&L);
    ret_t c = L.caixa;
    sup_retangulo(t, c.x, c.y, c.l, c.a, COR_FUNDO);
    sup_moldura(t, c.x - 1, c.y - 1, c.l + 2, c.a + 2, 0x000000);
    sup_moldura(t, c.x, c.y, c.l, c.a, COR_AZUL);
    sup_retangulo(t, c.x, c.y, c.l, 30, COR_AZUL);
    sup_texto(t, c.x + 14, c.y + 7, _("Control de permisos del sistema"), (esquema()->barra_propia ? esquema()->titulo_a_texto : 0xFFFFFF));
    /* Icono y nombre del programa (el nombre lo pone el kernel). */
    const desenho_t *ic = barra_icone_processo(vista.nome);
    int ix = c.x + 24, iy = c.y + 30 + 16;
    if (ic) sup_desenho(t, ix, iy, ic, 1);
    else { sup_retangulo(t, ix, iy, 32, 32, COR_AZUL); sup_texto(t, ix + 12, iy + 8, "?", 0xFFFFFF); }
    int tx = c.x + 72;
    char l1[48];
    txt_copia_n(l1, vista.nome, sizeof l1);
    sup_texto(t, tx, iy, l1, (esquema()->barra_propia ? esquema()->texto : 0x101820));
    sup_texto(t, tx + 1, iy, l1, (esquema()->barra_propia ? esquema()->texto : 0x101820));   /* negrita */
    sup_texto(t, tx, iy + 18, _("quiere hacer cambios en el sistema."), (esquema()->barra_propia ? esquema()->texto_fraco : 0x303840));
    if (L.n_motivo) {
        int ini[3], tam[3];
        partir(vista.motivo, (DW - 96) / 8, ini, tam);
        for (int i = 0; i < L.n_motivo; i++) {
            char b[64];
            int n = tam[i] < 63 ? tam[i] : 63;
            for (int k = 0; k < n; k++) b[k] = vista.motivo[ini[i] + k];
            b[n] = 0;
            sup_texto(t, tx, L.y_motivo + i * 16, b, (esquema()->barra_propia ? esquema()->texto_fraco : 0x303840));
        }
    }
    for (int k = 0; k < L.campos; k++) {
        sup_texto(t, c.x + 24, L.y_rotulo[k], rotulo_senha(k), (esquema()->barra_propia ? esquema()->texto : 0x101820));
        ret_t f = L.campo[k];
        sup_retangulo(t, f.x, f.y, f.l, f.a, (esquema()->barra_propia ? esquema()->campo_fundo : 0xFFFFFF));
        sup_moldura(t, f.x, f.y, f.l, f.a, k == foco && !comprobando ? COR_AZUL : 0x808890);
        char pts[PED_SENHA_MAX];
        int n = slen[k];
        for (int i = 0; i < n; i++) pts[i] = '*';
        pts[n] = 0;
        int max = (f.l - 12) / 8;
        sup_texto(t, f.x + 6, f.y + 4, n > max ? pts + (n - max) : pts, (esquema()->barra_propia ? esquema()->texto : 0x101820));
        if (k == foco && !comprobando) sup_retangulo(t, f.x + 6 + (n > max ? max : n) * 8, f.y + 4, 2, 16, (esquema()->barra_propia ? esquema()->texto : 0x101820));
    }
    if (comprobando) sup_texto(t, c.x + 24, L.y_msg, _("Comprobando..."), COR_AZUL);
    else if (msg[0]) sup_texto(t, c.x + 24, L.y_msg, msg, 0xC0261B);
    botao(t, L.si, _("Sí, continuar"), 0x2E7D32, !comprobando);
    botao(t, L.nao, _("No"), 0x5A6470, !comprobando);
    sup_texto(t, c.x + 24, c.y + c.a - 22, _("Solo el sistema muestra esta ventana. Esc = No."), (esquema()->barra_propia ? esquema()->texto_fraco : 0x6A727C));
}
