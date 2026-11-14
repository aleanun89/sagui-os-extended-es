// SPDX-License-Identifier: GPL-2.0-or-later
/* paleta.c - la paleta de acciones (Ctrl+Mayús+P) y "abrir por nombre" (Ctrl+P).
 *
 * Una caja sobre el editor con un campo de texto y una lista filtrada. El
 * filtro acepta las letras en orden, no tiene que ser seguido ("gdt" encuentra
 * "Guardar todo"), sin distinguir mayúsculas ni acentos simples; los
 * resultados que contienen el texto seguido vienen primero. Las flechas eligen, Intro
 * ejecuta, Esc cierra. */
#include "mico.h"

#define MAX_ITENS 1000
#define VIS 12
#define ALT_ITEM 20

bool paleta_aberta;
static bool de_arquivos;
static char q[96];
static char (*arqs)[CAMINHO_MAX];
static int narqs;
static int filtrados[MAX_ITENS], nf, sel, topo;

static unsigned char base(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
    switch (c) {   /* las vocales acentuadas valen como las simples */
    case 0xE1: case 0xC1: case 0xE0: case 0xC0: return 'a';
    case 0xE9: case 0xC9: case 0xE8: case 0xC8: return 'e';
    case 0xED: case 0xCD: return 'i';
    case 0xF3: case 0xD3: case 0xF2: case 0xD2: return 'o';
    case 0xFA: case 0xDA: case 0xFC: case 0xDC: return 'u';
    case 0xF1: case 0xD1: return 'n';
    }
    return c;
}

/* 0 = no sirve; 2 = contiene el texto seguido; 1 = las letras en orden. */
static int nota(const char *s, const char *f)
{
    if (!f[0]) return 1;
    int n = (int)tam_texto(s), k = (int)tam_texto(f);
    for (int i = 0; i + k <= n; i++) {
        int m = 0;
        while (m < k && base((unsigned char)s[i + m]) == base((unsigned char)f[m])) m++;
        if (m == k) return 2;
    }
    int m = 0;
    for (int i = 0; i < n && m < k; i++) if (base((unsigned char)s[i]) == base((unsigned char)f[m])) m++;
    return m == k ? 1 : 0;
}

static const char *texto_item(int i)
{
    if (!de_arquivos) return acoes[i].nome;
    const char *c = arqs[i];
    int pn = (int)tam_texto(pasta);
    if (pn > 1 && compara_texto_n(c, pasta, (size_t)pn) == 0 && c[pn] == '/') return c + pn + 1;
    return c;
}

static void filtrar(void)
{
    int total = de_arquivos ? narqs : QTD_ACOES;
    nf = 0;
    for (int passo = 2; passo >= 1; passo--)
        for (int i = 0; i < total && nf < MAX_ITENS; i++) {
            if (!de_arquivos && !acao_ativa(i)) continue;
            if (nota(texto_item(i), q) == passo) filtrados[nf++] = i;
        }
    sel = 0;
    topo = 0;
}

ret_t paleta_ret(void)
{
    int l = 520, a = 34 + VIS * ALT_ITEM + 8;
    if (l > r_editor.l + r_lateral.l - 20) l = r_editor.l + r_lateral.l - 20;
    int x = (j->larg - l) / 2, y = r_abas.y + 4;
    if (y + a > j->alt - 4) a = j->alt - 4 - y;
    return (ret_t){ x, y, l, a };
}

void paleta_abrir(bool arquivos)
{
    de_arquivos = arquivos;
    q[0] = 0;
    if (arquivos) {
        if (!pasta[0]) { avisar("Abre una carpeta primero (Archivo > Abrir carpeta)"); return; }
        if (!arqs) arqs = aloca(sizeof(*arqs) * MAX_ITENS);
        if (!arqs) return;
        narqs = expl_arquivos(arqs, MAX_ITENS);
    }
    filtrar();
    paleta_aberta = true;
    sujo |= R_TUDO;
    registrar("mico: paleta abierta (%s, %d elementos)", arquivos ? "archivos" : "ordenes", arquivos ? narqs : nf);
}

void paleta_fechar(void)
{
    if (!paleta_aberta) return;
    paleta_aberta = false;
    sujo |= R_TUDO;
    vista_sujar_tudo();
}

void paleta_desenhar(void)
{
    if (!paleta_aberta) return;
    ret_t r = paleta_ret();
    recorte = (ret_t){ 0, 0, j->larg, j->alt };
    d_ret(r.x + 3, r.y + 3, r.l, r.a, 0x0A0F0C);   /* sombra */
    recorte = r;
    d_ret(r.x, r.y, r.l, r.a, tema->painel);
    d_moldura(r.x, r.y, r.l, r.a, tema->destaque);
    d_ret(r.x + 8, r.y + 6, r.l - 16, 22, tema->fundo);
    d_moldura(r.x + 8, r.y + 6, r.l - 16, 22, tema->destaque);
    const char *dica = de_arquivos ? "Archivo:" : ">";
    int dn = (int)tam_texto(dica);
    d_texto(r.x + 13, r.y + 9, dica, dn, tema->margem_texto);
    int qx = r.x + 13 + (dn + 1) * LETRA_L;
    d_texto(qx, r.y + 9, q, (int)tam_texto(q), tema->texto);
    d_ret(qx + (int)tam_texto(q) * LETRA_L, r.y + 9, 2, LETRA_A, tema->cursor);
    int vis = (r.a - 34 - 8) / ALT_ITEM;
    if (!nf) d_texto(r.x + 14, r.y + 38, "Nada coincide", 13, tema->margem_texto);
    for (int k = 0; k < vis && topo + k < nf; k++) {
        int i = filtrados[topo + k], y = r.y + 34 + k * ALT_ITEM;
        if (topo + k == sel) d_ret(r.x + 2, y, r.l - 4, ALT_ITEM, tema->lateral_sel);
        const char *t = texto_item(i);
        if (!de_arquivos && acoes[i].atalho) {
            int an = (int)tam_texto(acoes[i].atalho);
            d_texto(r.x + r.l - 10 - an * LETRA_L, y + 2, acoes[i].atalho, an, tema->margem_texto);
            d_texto_ate(r.x + 14, y + 2, t, r.l - 40 - an * LETRA_L, tema->painel_texto);
        } else d_texto_ate(r.x + 14, y + 2, t, r.l - 28, tema->painel_texto);
    }
    d_mostrar((ret_t){ r.x, r.y, r.l + 3, r.a + 3 });
}

static void escolher(void)
{
    if (sel < 0 || sel >= nf) { paleta_fechar(); return; }
    int i = filtrados[sel];
    paleta_fechar();
    if (de_arquivos) { registrar("mico: paleta -> %s", arqs[i]); abrir_arquivo(arqs[i], true); }
    else { registrar("mico: paleta -> %s", acoes[i].nome); executar_acao(i); }
}

bool paleta_tecla(unsigned t, unsigned mods)
{
    (void)mods;
    if (!paleta_aberta) return false;
    ret_t r = paleta_ret();
    int vis = (r.a - 34 - 8) / ALT_ITEM, n = (int)tam_texto(q);
    if (t == TECLA_ESC) paleta_fechar();
    else if (t == '\n') escolher();
    else if (t == TECLA_CIMA) { if (sel > 0) sel--; }
    else if (t == TECLA_BAIXO) { if (sel < nf - 1) sel++; }
    else if (t == TECLA_PGUP) { sel -= vis; if (sel < 0) sel = 0; }
    else if (t == TECLA_PGDN) { sel += vis; if (sel > nf - 1) sel = nf - 1; }
    else if (t == '\b') { if (n) q[n - 1] = 0; filtrar(); }
    else if (t >= 0x20 && t < 0x100 && t != 0x7F && n < (int)sizeof q - 1) { q[n] = (char)t; q[n + 1] = 0; filtrar(); }
    if (sel < topo) topo = sel;
    if (sel >= topo + vis) topo = sel - vis + 1;
    if (paleta_aberta) paleta_desenhar();
    return true;
}

bool paleta_clique(int x, int y)
{
    ret_t r = paleta_ret();
    if (!dentro(x, y, r)) { paleta_fechar(); return false; }
    int k = (y - r.y - 34) / ALT_ITEM;
    if (y >= r.y + 34 && topo + k < nf) { sel = topo + k; escolher(); }
    return true;
}
