// SPDX-License-Identifier: GPL-3.0-only
/* discos - los discos y el espacio que ocupa cada uno (H34, D040, D121).
 *
 *   discos                lista en texto los pendrives, los discos internos y los CD/DVD (D207), con las
 *                         particiones: nombre, fabricante, tamaño, formato, etiqueta, dónde
 *                         están montadas y, si lo están, "usado/total" y el porcentaje
 *   discos -u             además, una barra de texto por partición montada: [#####.....] 52 %
 *   discos --ventana      la ventana "Discos": un bloque por disco y una barra horizontal de
 *                         uso por partición, con los botones Abrir y Montar/Desmontar (en un
 *                         CD/DVD, D233: Montar en /cd y Expulsar, que desmonta y abre la bandeja)
 *   discos --particiones [disco]
 *                         la misma ventana en la vista "Particiones" (D179): la tabla del
 *                         disco a escala y las operaciones (crear, borrar, redimensionar,
 *                         formatear, activa, TRIM, deshacer)
 *
 * Criterio ventana/terminal (D121): "--ventana" (o -v/-g) abre siempre la ventana (el menú
 * Inicio lo pasa así); "-t" y "-u" fuerzan texto. Sin argumentos, hay ventana solo si hay
 * escritorio y la salida NO es un terminal ni un pipe ni un archivo (es decir, lo lanzó el
 * icono del escritorio); desde un terminal o un script sigue saliendo el texto de siempre. */
#include "sagui.h"
#include "gui.h"
#include "contas.h"
#include "particoes.h"

#define TECLA_F5_ 0x10E

static void tamanho(unsigned setores, char *tam, int max)
{
    unsigned mb = setores / 2048;
    if (mb >= 1024) formatar_texto(tam, (size_t)max, "%u,%u GB", mb / 1024, (mb % 1024) * 10 / 1024);
    else formatar_texto(tam, (size_t)max, "%u MB", mb);
}

/* Tamaño en KB -> "12,3 GB", "512,0 MB" o "640 KB". */
static void tamanho_kb(unsigned kb, char *s, int max)
{
    if (kb >= 1024u * 1024u) formatar_texto(s, (size_t)max, "%u,%u GB", kb / (1024u * 1024u), (kb % (1024u * 1024u)) / (1024u * 1024u / 10));
    else if (kb >= 1024u) formatar_texto(s, (size_t)max, "%u,%u MB", kb / 1024u, (kb % 1024u) * 10u / 1024u);
    else formatar_texto(s, (size_t)max, "%u KB", kb);
}

/* Porcentaje usado, redondeado al más cercano. */
static unsigned porcentaje(unsigned total_kb, unsigned livre_kb)
{
    if (!total_kb) return 0;
    unsigned usado = total_kb >= livre_kb ? total_kb - livre_kb : 0;
    while (total_kb > 20000000u) { total_kb >>= 4; usado >>= 4; }   /* sin 64 bits: evita desbordar usado * 100 */
    if (!total_kb) return 100;
    return (usado * 100u + total_kb / 2) / total_kb;
}

/* La montagem de un dispositivo ("usb0p1"), o NULL. */
static info_montagem_t montagens[16];
static int qtd_montagens;
static void ler_montagens(void)
{
    qtd_montagens = info_sistema(INFO_MONTAGENS, montagens, 16);
    if (qtd_montagens < 0) qtd_montagens = 0;
}
static const info_montagem_t *montagem_de(const char *nome)
{
    for (int i = 0; i < qtd_montagens; i++)
        if (compara_texto(montagens[i].dispositivo, nome) == 0) return &montagens[i];
    return NULL;
}

/* D388: la de un volumen. En la live, la base SAGUI-RAIZ y la capa SAGUI-PERSIST salen las dos «montadas en
 * /»: el uso de la base es el del sistema de serie (/sistema/serie) y el de la capa, el de «/». */
static const info_montagem_t *montagem_vol(const info_disco_t *d)
{
    const char *papel = pt_papel_raiz(d);
    if (papel && (comeca_com(papel, "sistema de serie") || comeca_com(papel, "tus cambios")))
        for (int i = 0; i < qtd_montagens; i++)
            if (papel[0] == 's' ? compara_texto(montagens[i].tipo, "serie") == 0 : compara_texto(montagens[i].ponto, "/") == 0)
                return &montagens[i];
    return montagem_de(d->nome);
}

/* ------------------------------------------------------------ texto */

static void barra_texto(unsigned pct, char *s, int max)
{
    char b[11];
    unsigned n = (pct * 10 + 5) / 100;
    if (n > 10) n = 10;
    for (unsigned i = 0; i < 10; i++) b[i] = i < n ? '#' : '.';
    b[10] = 0;
    formatar_texto(s, (size_t)max, "[%s] %u %%", b, pct);
}

static bool barras;   /* -u */

static void uso_texto(const info_disco_t *d)
{
    const info_montagem_t *m = d->montado ? montagem_vol(d) : NULL;
    if (!m || !m->total_kb) return;
    char u[16], t[16];
    tamanho_kb(m->total_kb - m->livre_kb, u, sizeof u);
    tamanho_kb(m->total_kb, t, sizeof t);
    unsigned pct = porcentaje(m->total_kb, m->livre_kb);
    imprimir("  %s/%s %u%%", u, t, pct);
    if (barras) { char b[24]; barra_texto(pct, b, sizeof b); imprimir("  %s", b); }
}

static void estado(const info_disco_t *d, char *s, int max)
{
    if (!d->montado) copia_texto(s, "no montado", (size_t)max);
    else formatar_texto(s, (size_t)max, "montado en %s%s", d->ponto, (d->flags & DISCO_SO_LEITURA) ? ", solo lectura" : "");
}

enum { G_PENDRIVES, G_INTERNOS, G_CDS };

static int grupo_de(const info_disco_t *d)
{
    if (d->flags & DISCO_CD) return G_CDS;   /* D207: el lector y la imagen de arranque de su CD */
    return (d->flags & DISCO_INTERNO) ? G_INTERNOS : G_PENDRIVES;
}

static int listar_grupo(int grupo)
{
    info_disco_t d;
    int n = 0;
    for (int i = 0; listar_discos(i, &d) == 1; i++) {
        if (grupo_de(&d) != grupo) continue;
        char tam[24], est[96];
        tamanho(d.setores, tam, sizeof tam);
        estado(&d, est, sizeof est);
        if (d.particao) {
            if (d.suportado) {
                imprimir("   %-7s %s  %s  \"%s\"  (%s)", d.nome, tam, d.formato, d.rotulo, est); uso_texto(&d);
                if (compara_texto(d.rotulo, "SAGUI-CONF") == 0) imprimir("  [configuración del sistema: no la borres]");   /* D256 */
                const char *papel = pt_papel_raiz(&d);   /* D388: la raíz doble de la live */
                if (papel) imprimir("  [%s]", papel);
                imprimir("\n");
            }
            else imprimir("   %-7s %s  %s\n", d.nome, tam, d.formato);
            continue;
        }
        n++;
        /* D214: por dónde va un pendrive del xHCI (los del EHCI, como siempre). */
        const char *bus = (d.flags & DISCO_USB3) ? " [pendrive USB 3.0 (xHCI)]" :
                          (d.flags & DISCO_XHCI) ? " [pendrive USB (xHCI)]" : "";
        if (d.suportado) {
            imprimir("%-5s %s %s%s  %s  %s  \"%s\"  (%s)", d.nome, d.fabricante, d.produto, bus, tam, d.formato, d.rotulo, est);
            uso_texto(&d);
            imprimir("\n");
        } else
            imprimir("%-5s %s %s%s  %s  %s\n", d.nome, d.fabricante, d.produto, bus, tam, d.formato);
    }
    return n;
}

/* ------------------------------------------------------------ ventana */

#define MAX_LINHAS 64
#define ALT_DISCO  30        /* cabecera de un disco */
#define ALT_VOL    72        /* una partición: título, barra y texto de uso */
#define BARRA_TOPO 38        /* barra de herramientas fija */
#define BT_L       92
#define BT_A       26
#define MENU_VOL   1

enum { L_DISCO, L_VOL };
typedef struct {
    int      tipo;
    char     nome[12];       /* "usb0p1", "hda1", "ramfs" */
    char     descr[112];     /* cabecera de disco, o el título del volumen */
    char     detalhe[112];   /* debajo de la barra */
    unsigned total_kb, livre_kb;
    bool     montado, so_leitura, suportado, interno, ram;
    bool     cd;             /* D233: CD/DVD (o la imagen de arranque de dentro) */
    char     ponto[64];
} linha_t;

static linha_t lin[MAX_LINHAS], ant[MAX_LINHAS];
static int qtd, qtd_ant;
static jan_t *j;
static int rolagem;          /* píxeles que se desplazó el contenido */
static int sel = -1;         /* partición elegida con el botón derecho */
static char aviso[96];

static bool adicionar(const linha_t *l)
{
    if (qtd >= MAX_LINHAS) return false;
    lin[qtd++] = *l;
    return true;
}

/* Vuelve a leer discos y montajes (el estado se compara para redibujar solo si cambió). */
static void ler_estado(void)
{
    qtd = 0;
    ler_montagens();
    linha_t l;
    /* La RAM: la raíz "/" (y /tmp, que va dentro). */
    info_memoria_t mem;
    memset(&l, 0, sizeof l);
    l.tipo = L_DISCO;
    copia_texto(l.descr, "Memoria (RAM)  -  sistema de archivos en RAM", sizeof l.descr);
    adicionar(&l);
    memset(&l, 0, sizeof l);
    l.tipo = L_VOL; l.ram = true; l.montado = true; l.suportado = true;
    copia_texto(l.nome, "ramfs", sizeof l.nome);
    copia_texto(l.ponto, "/", sizeof l.ponto);
    const info_montagem_t *mr = NULL;
    for (int i = 0; i < qtd_montagens; i++) if (compara_texto(montagens[i].ponto, "/") == 0) mr = &montagens[i];
    if (mr) { l.total_kb = mr->total_kb; l.livre_kb = mr->livre_kb; }
    else if (info_sistema(INFO_MEMORIA, &mem, sizeof mem) == 0) { l.total_kb = mem.gerida_kb; l.livre_kb = mem.livre_kb; }
    char rb[16], rc[16];
    if (pt_raiz_live(rb, rc, sizeof rb)) {   /* D388: la live: la base y la capa (el espacio es el de la capa) */
        if (rc[0]) formatar_texto(l.descr, sizeof l.descr, "/  live: sistema de serie (%s) + tus cambios (%s)", rb, rc);
        else formatar_texto(l.descr, sizeof l.descr, "/  live: sistema de serie (%s) + tus cambios en la RAM", rb);
    } else copia_texto(l.descr, "/  (incluye /tmp)   ramfs", sizeof l.descr);
    adicionar(&l);

    info_disco_t d, p;
    for (int i = 0; listar_discos(i, &d) == 1; i++) {
        if (d.particao) continue;
        char tam[24], tipo[40];
        tamanho(d.setores, tam, sizeof tam);
        if (d.flags & DISCO_CD)   /* D207 */
            copia_texto(tipo, (d.flags & DISCO_BOOT) ? "imagen de arranque del CD" : "lector de CD/DVD (solo lectura)", sizeof tipo);
        else if (d.flags & DISCO_INTERNO) copia_texto(tipo, (d.flags & DISCO_BOOT) ? "disco interno (arranque)" : "disco interno", sizeof tipo);
        else copia_texto(tipo, (d.flags & DISCO_USB3) ? "pendrive USB 3.0 (xHCI)" : (d.flags & DISCO_XHCI) ?
                         "pendrive USB (xHCI)" : "pendrive USB", sizeof tipo);
        memset(&l, 0, sizeof l);
        l.tipo = L_DISCO;
        copia_texto(l.nome, d.nome, sizeof l.nome);
        formatar_texto(l.descr, sizeof l.descr, "%s  -  %s %s  -  %s  -  %s", d.nome, d.fabricante, d.produto, tam, tipo);
        l.interno = (d.flags & DISCO_INTERNO) != 0;
        adicionar(&l);
        int hijas = 0;
        for (int k = 0; listar_discos(k, &p) == 1; k++) {
            if (!p.particao || compara_texto(p.pai, d.nome) != 0) continue;
            hijas++;
            /* D213: la extendida es solo el contenedor de las lógicas (que salen debajo): en la
             * vista de uso no se muestra (sí en Particiones). */
            if (!p.suportado && comeca_com(p.formato, "extendida")) continue;
            info_disco_t *v = &p;
            memset(&l, 0, sizeof l);
            l.tipo = L_VOL;
            copia_texto(l.nome, v->nome, sizeof l.nome);
            l.interno = (d.flags & DISCO_INTERNO) != 0;
            l.cd = (d.flags & DISCO_CD) != 0;
            l.suportado = v->suportado != 0;
            l.montado = v->montado != 0;
            l.so_leitura = (v->flags & DISCO_SO_LEITURA) != 0;
            copia_texto(l.ponto, v->ponto, sizeof l.ponto);
            tamanho(v->setores, tam, sizeof tam);
            const char *papel = pt_papel_raiz(v);   /* D388: la raíz doble de la live */
            if (v->suportado && papel) formatar_texto(l.descr, sizeof l.descr, "%s  %s  \"%s\"  -  %s", v->nome, tam, v->rotulo, papel);
            else if (v->suportado) formatar_texto(l.descr, sizeof l.descr, "%s  %s  %s  \"%s\"", v->nome, tam, v->formato, v->rotulo);
            else formatar_texto(l.descr, sizeof l.descr, "%s  %s  %s", v->nome, tam, v->formato);
            const info_montagem_t *m = l.montado ? montagem_vol(v) : NULL;
            if (m) { l.total_kb = m->total_kb; l.livre_kb = m->livre_kb; }
            adicionar(&l);
        }
        if (!hijas) {   /* disco sin tabla de particiones: el propio disco es el volumen */
            memset(&l, 0, sizeof l);
            l.tipo = L_VOL;
            copia_texto(l.nome, d.nome, sizeof l.nome);
            l.interno = (d.flags & DISCO_INTERNO) != 0;
            l.cd = (d.flags & DISCO_CD) != 0;
            l.suportado = d.suportado != 0;
            l.montado = d.montado != 0;
            l.so_leitura = (d.flags & DISCO_SO_LEITURA) != 0;
            copia_texto(l.ponto, d.ponto, sizeof l.ponto);
            if (d.suportado) formatar_texto(l.descr, sizeof l.descr, "%s  %s  %s  \"%s\"", d.nome, tam, d.formato, d.rotulo);
            else formatar_texto(l.descr, sizeof l.descr, "%s  %s  %s", d.nome, tam, d.formato);
            const info_montagem_t *m = l.montado ? montagem_de(d.nome) : NULL;
            if (m) { l.total_kb = m->total_kb; l.livre_kb = m->livre_kb; }
            adicionar(&l);
        }
    }
    /* Texto bajo la barra. */
    for (int i = 0; i < qtd; i++) {
        linha_t *v = &lin[i];
        if (v->tipo != L_VOL) continue;
        if (v->montado && v->total_kb) {
            char u[16], t[16], f[16];
            unsigned usado = v->total_kb - v->livre_kb;
            tamanho_kb(usado, u, sizeof u);
            tamanho_kb(v->total_kb, t, sizeof t);
            tamanho_kb(v->livre_kb, f, sizeof f);
            formatar_texto(v->detalhe, sizeof v->detalhe, "%s usados de %s (%u %%) - libre %s", u, t, porcentaje(v->total_kb, v->livre_kb), f);
        } else if (v->cd && !v->suportado) copia_texto(v->detalhe, "nada que montar (Expulsar abre la bandeja)", sizeof v->detalhe);
        else if (!v->suportado) copia_texto(v->detalhe, "no se puede montar (formato no reconocido)", sizeof v->detalhe);
        else copia_texto(v->detalhe, "no montada", sizeof v->detalhe);
    }
}

static int altura_total(void)
{
    int h = 8;
    for (int i = 0; i < qtd; i++) h += lin[i].tipo == L_DISCO ? ALT_DISCO : ALT_VOL;
    return h + 8;
}

/* Barra rayada para lo que no está montado. */
static void barra_rayada(int x, int y, int l, int a)
{
    g_retangulo(j, x, y, l, a, 0xE4E7EA);
    for (int k = -a; k < l; k += 8) {
        for (int t = 0; t < 3; t++) {
            int x0 = x + k + t, x1 = x + k + a + t;
            int y0 = y + a - 1, y1 = y;
            int c0 = x0, c1 = x1;
            /* recorta la diagonal al rectángulo */
            if (c0 < x) { y0 -= x - c0; c0 = x; }
            if (c1 > x + l - 1) { y1 += c1 - (x + l - 1); c1 = x + l - 1; }
            if (c0 <= c1 && y0 >= y1) g_linha(j, c0, y0, c1, y1, 0xC9CED3);
        }
    }
}

static uint32_t cor_uso(unsigned pct) { return pct > 90 ? 0xD03A2F : pct >= 70 ? 0xE8891C : 0x2E9E4F; }

/* Dónde caen la barra y los botones de la fila que empieza en y (rolado). */
static int bt_x(void) { return j->larg - 12 - 14 - 2 * BT_L - 8; }
static int barra_l(void) { int l = bt_x() - 24; return l < 120 ? 120 : l; }

static bool bt_abrir_ok(const linha_t *v) { return v->suportado; }
/* D233: en un CD/DVD el segundo botón es Expulsar si está montado o no hay nada que montar. */
static bool bt_expulsar(const linha_t *v) { return v->cd && (v->montado || !v->suportado); }
static const char *bt_texto_montar(const linha_t *v) { return bt_expulsar(v) ? "Expulsar" : v->montado ? "Desmontar" : "Montar"; }

static void botao_fila_l(int x, int y, int l, const char *t, bool ativo)
{
    botao_t b = { x, y, l, BT_A, t, false };
    botao_desenhar(j, &b);
    if (!ativo) {   /* "apagado": velo gris sobre el texto */
        for (int k = 0; k < BT_A; k += 2) g_retangulo(j, x + 2, y + k, l - 4, 1, 0xE2E6EA);
    }
}

static void botao_fila(int x, int y, const char *t, bool ativo) { botao_fila_l(x, y, BT_L, t, ativo); }

static void desenhar(void)
{
    g_retangulo(j, 0, 0, j->larg, j->alt, COR_FUNDO);
    int y = BARRA_TOPO + 8 - rolagem;
    int nvol = 0;
    for (int i = 0; i < qtd; i++) {
        linha_t *v = &lin[i];
        if (v->tipo == L_DISCO) {
            g_retangulo(j, 8, y + 4, j->larg - 16 - 14, ALT_DISCO - 8, 0xE2E7EC);
            g_texto(j, 14, y + 6, v->descr, COR_TEXTO);
            y += ALT_DISCO;
            continue;
        }
        nvol++;
        unsigned pct = porcentaje(v->total_kb, v->livre_kb);
        if (i == sel) g_retangulo(j, 8, y, j->larg - 16 - 14, ALT_VOL - 2, 0xE7EEF8);
        g_texto(j, 20, y + 2, v->descr, COR_TEXTO);
        if (v->montado) {
            char c[96];
            formatar_texto(c, sizeof c, "en %s%s", v->ponto, v->so_leitura ? " (solo lectura)" : "");
            g_texto(j, 20 + g_largura_texto(v->descr) + 16, y + 2, c, COR_TEXTO_FRACO);
        }
        int bx = 20, bl = barra_l() - 8, by = y + 22;
        if (v->montado && v->total_kb) {
            int ul = (int)((unsigned)bl * pct / 100u);
            if (pct > 0 && ul < 2) ul = 2;
            g_retangulo(j, bx, by, bl, 18, 0xD5D9DD);
            g_retangulo(j, bx, by, ul, 18, cor_uso(pct));
        } else barra_rayada(bx, by, bl, 18);
        g_moldura(j, bx, by, bl, 18, COR_BORDA);
        g_texto(j, 20, y + 44, v->detalhe, v->montado ? COR_TEXTO : COR_TEXTO_FRACO);
        if (!v->ram) {
            int x2 = bt_x();
            botao_fila(x2, y + 2, "Abrir", bt_abrir_ok(v));
            botao_fila(x2 + BT_L + 8, y + 2, bt_texto_montar(v), v->suportado || v->cd);
        } else {
            botao_fila(bt_x(), y + 2, "Abrir", true);
        }
        g_retangulo(j, 12, y + ALT_VOL - 3, j->larg - 24 - 14, 1, 0xDDE1E5);
        y += ALT_VOL;
    }
    /* Barra de desplazamiento. */
    int total = altura_total(), vis = j->alt - BARRA_TOPO;
    if (total > vis) {
        int sx = j->larg - 12, sh = vis - 4;
        g_retangulo(j, sx, BARRA_TOPO + 2, 10, sh, 0xE2E6EA);
        int th = sh * vis / total;
        if (th < 20) th = 20;
        int ty = BARRA_TOPO + 2 + (sh - th) * rolagem / (total - vis);
        g_retangulo(j, sx, ty, 10, th, 0x8A939C);
    }
    /* Barra de herramientas encima del contenido. */
    g_retangulo(j, 0, 0, j->larg, BARRA_TOPO, 0xE6E9EC);
    g_retangulo(j, 0, BARRA_TOPO - 1, j->larg, 1, COR_BORDA);
    botao_t ba = { 8, 6, 130, BT_A, "Actualizar (F5)", false };
    botao_desenhar(j, &ba);
    botao_t bp = { 144, 6, 110, BT_A, "Particiones", false };   /* D179 */
    botao_desenhar(j, &bp);
    if (aviso[0]) g_texto(j, 266, 11, aviso, COR_TEXTO_FRACO);
    else {
        char t[64];
        formatar_texto(t, sizeof t, "%d volúmenes", nvol);
        g_texto(j, 266, 11, t, COR_TEXTO_FRACO);
    }
    jan_mostrar(j);
}

static void limitar_rolagem(void)
{
    int m = altura_total() - (j->alt - BARRA_TOPO);
    if (m < 0) m = 0;
    if (rolagem > m) rolagem = m;
    if (rolagem < 0) rolagem = 0;
    /* La prueba h92 lee este registro para saber dónde pulsar tras desplazar. */
    static int registrada = -1;
    if (rolagem != registrada) { registrada = rolagem; registrar("discos: rolagem %d", rolagem); }
}

/* Registro en el log serie: lo lee la prueba h92 en lugar de adivinar píxeles. */
/* Sólo se registra un volumen cuando cambia algo que se ve (montado, uso en %,
 * punto de montaje o posición): el espacio libre del ramdisk varía a cada
 * actualización y llenaba el log del kernel (informe del Toshiba, 2026-11-13). */
static char registrado[MAX_LINHAS][96];

static void registrar_estado(void)
{
    int y = BARRA_TOPO + 8;   /* sin desplazamiento: la prueba calcula dónde pulsar */
    for (int i = 0; i < qtd; i++) {
        linha_t *v = &lin[i];
        if (v->tipo != L_VOL) { y += ALT_DISCO; continue; }
        char chave[96];
        formatar_texto(chave, sizeof chave, "%s %d %u %u %s %d", v->nome, v->montado ? 1 : 0,
                       porcentaje(v->total_kb, v->livre_kb), v->total_kb, v->ponto, y);
        if (compara_texto(chave, registrado[i]) == 0) { y += ALT_VOL; continue; }
        copia_texto(registrado[i], chave, sizeof registrado[i]);
        registrar("discos: vol %s %s uso=%u%% total_kb=%u livre_kb=%u ponto=%s y=%d",
                  v->nome, v->montado ? "montado" : "no_montado", porcentaje(v->total_kb, v->livre_kb), v->total_kb, v->livre_kb, v->ponto, y);
        y += ALT_VOL;
    }
}

static void atualizar(bool forcar)
{
    memcpy(ant, lin, sizeof lin);
    qtd_ant = qtd;
    ler_estado();
    if (sel >= qtd || (sel >= 0 && lin[sel].tipo != L_VOL)) sel = -1;
    if (!forcar && qtd == qtd_ant && memcmp(ant, lin, sizeof(linha_t) * (size_t)qtd) == 0) return;
    limitar_rolagem();
    desenhar();
    registrar_estado();
}

static void abrir_en_archivos(const linha_t *v)
{
    char *args[3] = { "arquivos", (char *)v->ponto, NULL };
    int pid = executar("/bin/arquivos", args);
    registrar("discos: abrir %s en Archivos (pid %d)", v->ponto, pid);
}

/* Monta un volumen. Pendrives en /usb (como el icono del escritorio) o en /mnt/<nombre>; los discos
 * internos van en solo lectura, salvo que rw sea un pedido explícito (D040). */
static bool montar_volume(const linha_t *v, bool rw)
{
    int r;
    char ponto[32];
    formatar_texto(ponto, sizeof ponto, "/mnt/%s", v->nome);
    if (rw && !v->interno) rw = false;   /* los pendrives ya montan con escritura */
    if (v->cd) {   /* D233: un CD/DVD en /cd, como al meterlo; si está ocupado, en /mnt/cdN */
        rw = false;
        r = COM_ADMIN("montar un disco", montar_em(v->nome, "/cd", MONTAR_SO_LEITURA));
        if (r == ERRO_OCUPADO) r = COM_ADMIN("montar un disco", montar_em(v->nome, ponto, MONTAR_SO_LEITURA));
    } else if (v->interno) r = COM_ADMIN("montar un disco", montar_em(v->nome, ponto, rw ? MONTAR_LEITURA_ESCRITA : MONTAR_SO_LEITURA));
    else {
        r = COM_ADMIN("montar un disco", montar_em(v->nome, "/usb", 0));
        if (r == ERRO_OCUPADO) r = COM_ADMIN("montar un disco", montar_em(v->nome, ponto, 0));
    }
    if (r < 0) formatar_texto(aviso, sizeof aviso, "No se pudo montar %s: %s", v->nome, r == ERRO_FORMATO ? "formato no soportado" : nome_erro(r));
    else formatar_texto(aviso, sizeof aviso, "%s montado%s", v->nome, v->interno && !rw ? " (solo lectura)" : "");
    registrar("discos: montar %s -> %d", v->nome, r);
    return r >= 0;
}

/* D233: desmonta el CD/DVD y abre la bandeja. */
static void expulsar(const linha_t *v)
{
    int r = COM_ADMIN("expulsar un disco", expulsar_disco(v->nome, 0));
    if (r == ERRO_OCUPADO) formatar_texto(aviso, sizeof aviso, "%s está en uso (¿el instalador?): no se expulsa", v->nome);
    else if (r < 0) formatar_texto(aviso, sizeof aviso, "No se pudo expulsar %s: %s", v->nome, nome_erro(r));
    else formatar_texto(aviso, sizeof aviso, "%s expulsado: ya puedes sacar el disco", v->nome);
    registrar("discos: expulsar %s -> %d", v->nome, r);
}

static void alternar_montaje(const linha_t *v)
{
    if (bt_expulsar(v)) expulsar(v);
    else if (v->montado) {
        int r = COM_ADMIN("desmontar un disco", desmontar(v->nome));
        if (r < 0) formatar_texto(aviso, sizeof aviso, "No se pudo desmontar %s: %s", v->nome, nome_erro(r));
        else formatar_texto(aviso, sizeof aviso, "%s desmontado", v->nome);
        registrar("discos: desmontar %s -> %d", v->nome, r);
    } else montar_volume(v, false);
    atualizar(true);
}

static void fila_clique(int i, bool abrir, bool montar_btn, bool direito, const evento_t *e)
{
    linha_t v = lin[i];
    sel = i;
    if (abrir) {
        if (!v.montado && !v.ram && !montar_volume(&v, false)) { atualizar(true); return; }
        if (!v.montado && !v.ram) { atualizar(true); v = lin[i]; }
        abrir_en_archivos(&v);
        atualizar(true);
    } else if (montar_btn) alternar_montaje(&v);
    else if (direito) {
        atualizar(true);
        if (v.ram) menu_contextual(j->id, e->x, e->y, "Abrir en Archivos\n~Montar\n~Montar con escritura\n~Desmontar\n-\nActualizar", MENU_VOL);
        else {
            char m[192];
            formatar_texto(m, sizeof m, "%sAbrir en Archivos\n%sMontar\n%sMontar con escritura\n%sDesmontar\n-\nActualizar%s",
                           v.suportado ? "" : "~", (v.suportado && !v.montado) ? "" : "~",
                           (v.suportado && !v.montado && v.interno && !v.cd) ? "" : "~", v.montado ? "" : "~",
                           v.cd ? "\n-\nExpulsar" : "");   /* D233 */
            menu_contextual(j->id, e->x, e->y, m, MENU_VOL);
        }
    } else atualizar(true);
}

static void tratar_menu(int indice)
{
    if (indice < 0) return;
    if (indice == 5) { aviso[0] = 0; atualizar(true); return; }
    if (sel < 0 || sel >= qtd) return;
    linha_t v = lin[sel];
    if (indice == 0) { if (v.montado || v.ram) abrir_en_archivos(&v); else if (montar_volume(&v, false)) { atualizar(true); abrir_en_archivos(&lin[sel]); } }
    else if (indice == 1 && !v.montado) { montar_volume(&v, false); atualizar(true); }
    else if (indice == 2 && !v.montado && v.interno) {
        if (confirmar("Montar con escritura", "Este disco es interno. ¿Montarlo con escritura? Podría dañarse su contenido.")) montar_volume(&v, true);
        atualizar(true);
    } else if (indice == 3 && v.montado) {
        int r = COM_ADMIN("desmontar un disco", desmontar(v.nome));
        if (r < 0) formatar_texto(aviso, sizeof aviso, "No se pudo desmontar %s: %s", v.nome, nome_erro(r));
        else formatar_texto(aviso, sizeof aviso, "%s desmontado", v.nome);
        registrar("discos: desmontar %s -> %d", v.nome, r);
        atualizar(true);
    } else if (indice == 7 && v.cd) { expulsar(&v); atualizar(true); }   /* D233 */
}

/* ------------------------------------------------------------ particiones (D179)
 *
 * La vista "Particiones" de la misma ventana: arriba un selector de discos, la barra
 * del disco a escala (cada partición con el color de su tipo; la extendida es un
 * marco con sus lógicas dentro; lo libre, rayado), una leyenda, la lista con todos
 * los datos y los botones de cada operación (también con el botón derecho sobre la
 * barra o la lista). Las operaciones son las de "particiones" (pt_executar): antes de
 * cada una, un diálogo que dice exactamente qué se pierde; el disco de arranque pide
 * dos confirmaciones y lo montado se ofrece desmontar.
 * D213 (v7.3): con particiones montadas (el disco de arranque, con /home y /apps) se puede
 * cambiar el resto: solo se ofrece desmontar la partición que se va a tocar. "Ampliar"
 * lleva la extendida hasta el final del disco, y "Tamaño" hace crecer o encoger también
 * el ext2 (y crecer el FAT32), con una comprobación del sistema de archivos antes.
 * D266 (v8.0): también discos GPT (crear, borrar y formatear; sus entradas 1 a 4 se ven como las
 * primarias del MBR), la partición EFI (ESP) con su aviso fuerte y «Tabla nueva» MBR o GPT. */
#define MENU_PART   4
#define MENU_CRIAR  5
#define MENU_FMT    6
#define PV_W_BT     92
enum { PV_CRIAR, PV_BORRAR, PV_REDIM, PV_FORMATEAR, PV_ACTIVA, PV_TRIM, PV_DESHACER, PV_AMPLIAR, PV_REPARAR, PV_NBT };
static const char *const pv_bt_texto[PV_NBT] = { "Crear...", "Borrar", "Tamaño...", "Formatear...", "Activa", "TRIM", "Deshacer",
                                                 "Ampliar", "Reparar..." };

typedef struct {
    int      num;            /* 0 = espacio libre */
    uint32_t inicio, setores;
    bool     logica, ext;
    int      x, l;           /* en la barra */
} pv_seg_t;

static bool vista_part;
static char pv_disco[12];
static pt_tabela_t pv_t;
static info_disco_t pv_info;
static info_bloco_t pv_bloco;
static pv_seg_t pv_seg[4 + PT_MAX_LOG + 24];
static int pv_nseg, pv_sel = -1, pv_passos;
static char pv_msg[200];
static bool pv_msg_erro;
static char pv_discos[12][12];
static int pv_ndiscos;
static int pv_prog = -1;     /* progreso de un formateo (-1 = ninguno) */

static bool pv_info_de(const char *nome, info_disco_t *out)
{
    for (int i = 0; listar_discos(i, out) == 1; i++) if (compara_texto(out->nome, nome) == 0) return true;
    return false;
}

static uint32_t pv_cor(uint8_t t)
{
    switch (t) {
    case 0x0B: case 0x0C: return 0x3A7BD5;
    case 0x83: return 0x2E9E4F;
    case 0xDA: return 0xE8891C;
    case 0xEF: return 0xC9A60C;   /* D266: la partición EFI (ESP) */
    case 0x07: return 0x8E5CC2;
    case 0x82: return 0x95A0A8;
    case 0x01: case 0x04: case 0x06: case 0x0E: return 0x5DADE2;
    default: return pt_tipo_extendida(t) ? 0x41505F : 0x7F8C8D;
    }
}

/* Divide un texto en líneas de como mucho n letras (los diálogos no cortan solos). */
static void pv_quebrar(const char *t, char *s, int max, int n)
{
    int k = 0, col = 0;
    while (*t && k < max - 1) {
        const char *p = t;
        while (*p && *p != ' ' && *p != '\n') p++;
        int len = (int)(p - t);
        if (col && col + 1 + len > n) { s[k++] = '\n'; col = 0; }
        else if (col && k < max - 1) { s[k++] = ' '; col++; }
        for (int i = 0; i < len && k < max - 1; i++) s[k++] = t[i];
        col += len;
        t = p;
        if (*t == '\n') { if (k < max - 1) s[k++] = '\n'; col = 0; t++; }
        else if (*t == ' ') t++;
    }
    s[k] = 0;
}

static bool pv_confirmar(const char *titulo, const char *texto)
{
    char q[600];
    pv_quebrar(texto, q, sizeof q, 42);
    return confirmar(titulo, q);
}

static void pv_mensagem(const char *titulo, const char *texto)
{
    char q[600];
    pv_quebrar(texto, q, sizeof q, 42);
    mensagem(titulo, q);
}

static int pv_pedir_admin(void);

static void pv_ler(void)
{
    pv_ndiscos = 0;
    info_disco_t d;
    for (int i = 0; listar_discos(i, &d) == 1 && pv_ndiscos < 12; i++)
        if (!d.particao) copia_texto(pv_discos[pv_ndiscos++], d.nome, 12);
    if (!pv_disco[0] && pv_ndiscos) {   /* por defecto, el primero que no es el de arranque */
        copia_texto(pv_disco, pv_discos[0], sizeof pv_disco);
        for (int i = 0; i < pv_ndiscos; i++)
            if (pv_info_de(pv_discos[i], &d) && !(d.flags & DISCO_ARRANQUE_REAL)) { copia_texto(pv_disco, pv_discos[i], sizeof pv_disco); break; }
    }
    pv_nseg = 0;
    memset(&pv_t, 0, sizeof pv_t);
    if (!pv_disco[0] || !pv_info_de(pv_disco, &pv_info)) return;
    int rl = pt_ler(pv_disco, &pv_t);
    if (rl == ERRO_ACESSO && pv_pedir_admin() == 0) rl = pt_ler(pv_disco, &pv_t);   /* D269: diálogo de permisos */
    if (rl < 0) {
        if (rl == ERRO_ACESSO) copia_texto(pv_msg, "Hacen falta permisos de administrador para ver los discos.", sizeof pv_msg);
        return;
    }
    blocos_info(pv_disco, &pv_bloco);
    pv_passos = pt_passos_deshacer(pv_disco);
    /* Los segmentos, por orden: primarias y huecos; dentro de la extendida, sus lógicas y huecos. */
    pt_livre_t v[24];
    int nl = pt_livres(&pv_t, v, 24);
    bool usado[24] = { false };
    for (;;) {
        uint32_t melhor = 0xFFFFFFFFu;
        int qual = -1, tipo = 0;
        for (int i = 0; i < 4; i++) {
            if (!pv_t.prim[i].tipo) continue;
            bool ja = false;
            for (int k = 0; k < pv_nseg; k++) if (pv_seg[k].num == i + 1) ja = true;
            if (!ja && pv_t.prim[i].inicio < melhor) { melhor = pv_t.prim[i].inicio; qual = i; tipo = 1; }
        }
        for (int i = 0; i < nl; i++)
            if (!usado[i] && !v[i].logica && v[i].inicio < melhor) { melhor = v[i].inicio; qual = i; tipo = 2; }
        if (qual < 0) break;
        pv_seg_t *s = &pv_seg[pv_nseg++];
        memset(s, 0, sizeof *s);
        if (tipo == 2) { usado[qual] = true; s->inicio = v[qual].inicio; s->setores = v[qual].setores; continue; }
        const pt_part_t *p = &pv_t.prim[qual];
        s->num = qual + 1; s->inicio = p->inicio; s->setores = p->setores; s->ext = pt_tipo_extendida(p->tipo);
        if (!s->ext) continue;
        /* Lo de dentro de la extendida, ordenado. */
        for (;;) {
            uint32_t m2 = 0xFFFFFFFFu;
            int q2 = -1, t2 = 0;
            for (int k = 0; k < pv_t.nlog; k++) {
                bool ja = false;
                for (int z = 0; z < pv_nseg; z++) if (pv_seg[z].num == k + 5) ja = true;
                if (!ja && pv_t.log[k].inicio < m2) { m2 = pv_t.log[k].inicio; q2 = k; t2 = 1; }
            }
            for (int i = 0; i < nl; i++)
                if (!usado[i] && v[i].logica && v[i].inicio < m2) { m2 = v[i].inicio; q2 = i; t2 = 2; }
            if (q2 < 0) break;
            pv_seg_t *x = &pv_seg[pv_nseg++];
            memset(x, 0, sizeof *x);
            x->logica = true;
            if (t2 == 2) { usado[q2] = true; x->inicio = v[q2].inicio; x->setores = v[q2].setores; }
            else { x->num = q2 + 5; x->inicio = pv_t.log[q2].inicio; x->setores = pv_t.log[q2].setores; }
        }
    }
    if (pv_sel >= pv_nseg) pv_sel = -1;
}

/* Geometría de la vista (depende del tamaño de la ventana). */
#define PV_CAB_Y    (BARRA_TOPO + 8)
#define PV_BARRA_Y  (BARRA_TOPO + 50)
#define PV_BARRA_A  64
#define PV_LEG_Y    (PV_BARRA_Y + PV_BARRA_A + 10)
#define PV_TAB_Y    (PV_LEG_Y + 28)
#define PV_LIN_A    22
static int pv_bt_y(void) { return j->alt - 76; }
static int pv_bt_x(int i) { return 12 + i * (PV_W_BT + 6); }

static uint32_t pv_mb(uint32_t s) { return s / 2048; }

static void pv_geometria(void)
{
    int bx = 12, bw = j->larg - 24;
    uint32_t total = pv_mb(pv_t.setores);
    if (!total) total = 1;
    for (int i = 0; i < pv_nseg; i++) {
        pv_seg_t *s = &pv_seg[i];
        s->x = bx + (int)(pv_mb(s->inicio) * (uint32_t)bw / total);
        int fim = bx + (int)(pv_mb(s->inicio + s->setores) * (uint32_t)bw / total);
        s->l = fim - s->x;
        if (s->l < 4) s->l = 4;
    }
}

static void pv_rayado(int x, int y, int l, int a, uint32_t fundo, uint32_t raya)
{
    g_retangulo(j, x, y, l, a, fundo);
    for (int k = -a; k < l; k += 7)
        for (int t = 0; t < a; t++) {
            int px = x + k + t, py = y + a - 1 - t;
            if (px >= x && px < x + l) g_retangulo(j, px, py, 1, 1, raya);
        }
}

static void pv_nome_seg(const pv_seg_t *s, char *n, int max)
{
    if (!s->num) copia_texto(n, "libre", (size_t)max);
    else pt_nome_particao(pv_disco, s->num, n, max);
}

/* D256: ¿es SAGUI-CONF, la configuración del sistema? Para el aviso fuerte al borrarla o formatearla. */
static bool pv_es_conf(const pv_seg_t *s)
{
    char n[16];
    info_disco_t d;
    if (!s || !s->num) return false;
    pv_nome_seg(s, n, sizeof n);
    return pv_info_de(n, &d) && compara_texto(d.rotulo, "SAGUI-CONF") == 0;
}

/* D266: ¿es la partición EFI (ESP)? Aviso fuerte, como el de SAGUI-CONF. */
static bool pv_es_esp(const pv_seg_t *s)
{
    if (!s || !s->num || s->num > 4) return false;
    return pv_t.prim[s->num - 1].tipo == 0xEF;
}

static void pv_tamanho(uint32_t setores, char *s, int max)
{
    uint32_t mb = setores / 2048;
    if (mb >= 1024) formatar_texto(s, (size_t)max, "%u,%u GB", mb / 1024, (mb % 1024) * 10 / 1024);
    else formatar_texto(s, (size_t)max, "%u MB", mb);
}

static const pt_part_t *pv_part(const pv_seg_t *s) { return s->num ? pt_achar(&pv_t, s->num) : NULL; }

static void pv_desenhar_seg(int i, int y, int a)
{
    pv_seg_t *s = &pv_seg[i];
    const pt_part_t *p = pv_part(s);
    if (!p) pv_rayado(s->x, y, s->l, a, 0xEEF0F2, 0xD3D8DD);
    else g_retangulo(j, s->x, y, s->l, a, pv_cor(p->tipo));
    g_moldura(j, s->x, y, s->l, a, 0xFFFFFF);
    char n[16], t[16];
    pv_nome_seg(s, n, sizeof n);
    pv_tamanho(s->setores, t, sizeof t);
    uint32_t c = p ? 0xFFFFFF : COR_TEXTO_FRACO;
    int lt = g_largura_texto(n), lt2 = g_largura_texto(t);
    if (s->l > lt + 8 && a >= 36) {
        g_texto(j, s->x + 4, y + a / 2 - 17, n, c);
        if (s->l > lt2 + 8) g_texto(j, s->x + 4, y + a / 2 + 1, t, c);
    } else if (s->l > lt + 6) g_texto(j, s->x + 3, y + (a - 16) / 2, n, c);
    if (p && p->ativa && s->l > 14) g_texto(j, s->x + s->l - 12, y + 2, "*", 0xFFFFFF);
    if (i == pv_sel) {
        g_moldura(j, s->x - 1, y - 1, s->l + 2, a + 2, COR_SELECAO);
        g_moldura(j, s->x - 2, y - 2, s->l + 4, a + 4, COR_SELECAO);
    }
}

/* D213: hasta dónde puede llegar la extendida (el final del disco o la primaria que la siga);
 * 0 si no hay extendida o ya llega. */
static uint32_t pv_fim_ampliar(void)
{
    int x = pt_extendida(&pv_t);
    if (x < 0) return 0;
    const pt_part_t *e = &pv_t.prim[x];
    uint32_t lim = pv_t.setores;
    for (int i = 0; i < 4; i++)
        if (i != x && pv_t.prim[i].tipo && pv_t.prim[i].inicio >= e->inicio + e->setores && pv_t.prim[i].inicio < lim) lim = pv_t.prim[i].inicio;
    lim = lim / 2048 * 2048;
    return lim > e->inicio + e->setores ? lim : 0;
}

static bool pv_pode(int bt)
{
    pv_seg_t *s = pv_sel >= 0 ? &pv_seg[pv_sel] : NULL;
    const pt_part_t *p = s ? pv_part(s) : NULL;
    bool mbr_ok = pv_t.esquema == PT_MBR || pv_t.esquema == PT_SEM_TABELA;
    bool tabela_ok = mbr_ok || (pv_t.esquema == PT_GPT && !pv_t.gpt_mala);   /* D266 */
    switch (bt) {
    case PV_CRIAR: return tabela_ok && s && !s->num;
    case PV_BORRAR: return tabela_ok && p;
    case PV_REDIM: return mbr_ok && p;
    case PV_FORMATEAR: return tabela_ok && p && !pt_tipo_extendida(p->tipo);
    case PV_ACTIVA: return mbr_ok && p && s->num <= 4 && !pt_tipo_extendida(p->tipo) && !p->ativa;
    case PV_TRIM: return tabela_ok && pv_bloco.trim == TRIM_SIM && s && !(p && pt_tipo_extendida(p->tipo));
    case PV_DESHACER: return pv_passos > 0;
    case PV_AMPLIAR: return pv_t.esquema == PT_MBR && pv_fim_ampliar() != 0;
    case PV_REPARAR: {   /* D397: solo un ext2 */
        char n[16];
        info_disco_t d;
        if (!p || !s->num || pt_tipo_extendida(p->tipo)) return false;
        pv_nome_seg(s, n, sizeof n);
        return pv_info_de(n, &d) && d.suportado && (compara_texto(d.tipo_fs, "ext2") == 0 || compara_texto(d.formato, "ext2") == 0);
    }
    }
    return false;
}

static void pv_desenhar(void)
{
    g_retangulo(j, 0, 0, j->larg, j->alt, COR_FUNDO);
    /* Barra de herramientas: volver, actualizar y un botón por disco. */
    g_retangulo(j, 0, 0, j->larg, BARRA_TOPO, 0xE6E9EC);
    g_retangulo(j, 0, BARRA_TOPO - 1, j->larg, 1, COR_BORDA);
    botao_t bv = { 8, 6, 84, BT_A, "< Uso", false };
    botao_desenhar(j, &bv);
    botao_t ba = { 98, 6, 100, BT_A, "Actualizar", false };
    botao_desenhar(j, &ba);
    g_texto(j, 210, 11, "Disco:", COR_TEXTO_FRACO);
    for (int i = 0; i < pv_ndiscos; i++) {
        int x = 262 + i * 68;
        bool sel = compara_texto(pv_discos[i], pv_disco) == 0;
        g_retangulo(j, x, 6, 62, BT_A, sel ? COR_SELECAO : 0xF7F8F9);
        g_moldura(j, x, 6, 62, BT_A, sel ? COR_SELECAO : COR_BORDA);
        g_texto(j, x + (62 - g_largura_texto(pv_discos[i])) / 2, 11, pv_discos[i], sel ? 0xFFFFFF : COR_TEXTO);
    }
    if (!pv_disco[0] || !pv_t.setores) {
        g_texto(j, 16, PV_CAB_Y + 8, "No hay discos.", COR_TEXTO_FRACO);
        jan_mostrar(j);
        return;
    }
    /* Cabecera del disco. */
    char tam[16], c[200];
    pv_tamanho(pv_t.setores, tam, sizeof tam);
    static const char *const esq[] = { "sin tabla de particiones", "tabla MBR", "tabla GPT", "volumen sin tabla" };
    formatar_texto(c, sizeof c, "%s  -  %s %s  -  %s  -  %s", pv_disco, pv_info.fabricante, pv_info.produto, tam, esq[pv_t.esquema]);
    g_texto(j, 14, PV_CAB_Y, c, COR_TEXTO);
    uint32_t cor_aviso = COR_TEXTO_FRACO;
    if (pv_t.esquema == PT_GPT && pv_t.gpt_mala) { formatar_texto(c, sizeof c, "GPT no válida (%s): no se toca; solo vale una tabla nueva.", pv_t.gpt_aviso); cor_aviso = 0xC0392B; }
    else if (pv_t.esquema == PT_GPT && pv_t.gpt_respaldo) { copia_texto(c, "GPT: la cabecera principal está dañada, se lee el respaldo (se repara al cambiar algo).", sizeof c); cor_aviso = 0xC0392B; }
    else if (pv_t.esquema == PT_GPT && pv_info.montado) { copia_texto(c, "GPT con particiones montadas: desmóntalas para cambiar la tabla (el kernel no deja tocarla montada).", sizeof c); cor_aviso = 0xB9770E; }
    else if (pv_t.esquema == PT_GPT) {
        formatar_texto(c, sizeof c, "GPT: se puede crear, borrar y formatear (hasta cuatro particiones). Una GPT ajena no se toca hasta que lo pidas.%s",
                       pv_t.ngpt > 4 ? " Las entradas más allá de la cuarta no se gestionan." : "");
        cor_aviso = 0xB9770E;
    }
    else if (pv_info.flags & DISCO_ARRANQUE_REAL) { copia_texto(c, "Es el disco de arranque de SaguiOS: cualquier cambio pide confirmarlo dos veces.", sizeof c); cor_aviso = 0xC0392B; }
    else if (pv_info.montado) { copia_texto(c, "Tiene particiones montadas: no se tocan; para cambiar una se ofrecerá desmontarla.", sizeof c); cor_aviso = 0xB9770E; }
    else if (pv_t.esquema == PT_VOLUME) copia_texto(c, "El disco entero es un volumen: para particionarlo, crea antes una tabla nueva (menú).", sizeof c);
    else formatar_texto(c, sizeof c, "TRIM: %s.   Deshacer: %d cambio%s guardado%s.",
                        pv_bloco.trim == TRIM_SIM ? "sí" : pv_bloco.trim == TRIM_SEM_DRIVER ? "el disco sí, el driver IDE no" : "no",
                        pv_passos, pv_passos == 1 ? "" : "s", pv_passos == 1 ? "" : "s");
    g_texto(j, 14, PV_CAB_Y + 18, c, cor_aviso);

    /* La barra del disco. */
    pv_geometria();
    int bx = 12, bw = j->larg - 24, by = PV_BARRA_Y, ba2 = PV_BARRA_A;
    if (pv_t.esquema == PT_VOLUME) {
        g_retangulo(j, bx, by, bw, ba2, 0x3A7BD5);
        g_texto(j, bx + 8, by + 24, pv_info.formato, 0xFFFFFF);
    } else {
        g_retangulo(j, bx, by, bw, ba2, 0xF7F8F9);
        for (int i = 0; i < pv_nseg; i++) {
            if (pv_seg[i].logica) continue;
            if (pv_seg[i].ext) {   /* marco de la extendida, con su nombre */
                pv_seg_t *s = &pv_seg[i];
                g_retangulo(j, s->x, by, s->l, ba2, pv_cor(0x0F));
                char n[16];
                pv_nome_seg(s, n, sizeof n);
                if (s->l > 90) { char t[40]; formatar_texto(t, sizeof t, "%s extendida", n); g_texto(j, s->x + 4, by + 1, t, 0xFFFFFF); }
                if (i == pv_sel) { g_moldura(j, s->x - 1, by - 1, s->l + 2, ba2 + 2, COR_SELECAO); g_moldura(j, s->x - 2, by - 2, s->l + 4, ba2 + 4, COR_SELECAO); }
            } else pv_desenhar_seg(i, by, ba2);
        }
        for (int i = 0; i < pv_nseg; i++) if (pv_seg[i].logica) pv_desenhar_seg(i, by + 18, ba2 - 22);
    }
    g_moldura(j, bx - 1, by - 1, bw + 2, ba2 + 2, COR_BORDA);

    /* Leyenda. */
    static const struct { uint8_t t; const char *n; } ley[] = {
        { 0x0C, "FAT32" }, { 0x83, "Linux/ext2" }, { 0xDA, "SaguiOS" }, { 0xEF, "EFI" }, { 0x07, "exFAT/NTFS" }, { 0x0F, "extendida" }, { 0x01, "otras" }, { 0, "libre" } };
    int lx = 14;
    for (unsigned i = 0; i < sizeof ley / sizeof ley[0]; i++) {
        if (ley[i].t) g_retangulo(j, lx, PV_LEG_Y + 3, 12, 12, ley[i].t == 0x01 ? pv_cor(0x99) : pv_cor(ley[i].t));
        else pv_rayado(lx, PV_LEG_Y + 3, 12, 12, 0xEEF0F2, 0xC9CED3);
        g_moldura(j, lx, PV_LEG_Y + 3, 12, 12, COR_BORDA);
        g_texto(j, lx + 16, PV_LEG_Y, ley[i].n, COR_TEXTO_FRACO);
        lx += 16 + g_largura_texto(ley[i].n) + 16;
    }

    /* La lista. */
    static const int col[] = { 14, 48, 118, 196, 270, 436, 556, 666 };
    static const char *const tit[] = { "Nº", "Disp.", "Inicio", "Tamaño", "Tipo", "Formato", "Etiqueta", "Montada en" };
    int ty = PV_TAB_Y;
    g_retangulo(j, 8, ty - 2, j->larg - 16, PV_LIN_A, 0xE2E7EC);
    for (int k = 0; k < 8; k++) if (col[k] < j->larg - 20) g_texto(j, col[k], ty + 1, tit[k], COR_TEXTO);
    ty += PV_LIN_A;
    int max_lin = (pv_bt_y() - 8 - ty) / PV_LIN_A;
    {
        for (int i = 0; i < pv_nseg && i < max_lin; i++, ty += PV_LIN_A) {
            pv_seg_t *s = &pv_seg[i];
            const pt_part_t *p = pv_part(s);
            if (i == pv_sel) g_retangulo(j, 8, ty - 3, j->larg - 16, PV_LIN_A, 0xDCE8FA);
            char a[16], n[16], ini[16], tm[16], tp[32];
            info_disco_t d;
            pv_nome_seg(s, n, sizeof n);
            formatar_texto(a, sizeof a, s->num ? "%d" : "", s->num);
            formatar_texto(ini, sizeof ini, "%u MB", s->inicio / 2048);
            pv_tamanho(s->setores, tm, sizeof tm);
            int ind = s->logica ? 12 : 0;
            if (p) {
                g_retangulo(j, col[0] + ind - 2, ty + 3, 3, 10, pv_cor(p->tipo));
                formatar_texto(tp, sizeof tp, "%s%s", pt_nome_tipo(p->tipo), p->ativa ? " (activa)" : "");
            } else copia_texto(tp, s->logica ? "espacio libre (lógica)" : "espacio libre", sizeof tp);
            uint32_t ct = p ? COR_TEXTO : COR_TEXTO_FRACO;
            g_texto(j, col[0] + ind + 4, ty, a, ct);
            g_texto(j, col[1] + ind, ty, n, ct);
            g_texto(j, col[2], ty, ini, ct);
            g_texto(j, col[3], ty, tm, ct);
            g_texto(j, col[4], ty, tp, ct);
            if (p && !s->ext && pv_info_de(n, &d)) {
                char f[24];
                copia_texto(f, d.suportado ? d.formato : "-", sizeof f);
                if (col[5] < j->larg - 20) g_texto(j, col[5], ty, f, ct);
                if (col[6] < j->larg - 20) g_texto(j, col[6], ty, d.rotulo, ct);
                if (col[7] < j->larg - 20) g_texto(j, col[7], ty, d.montado ? d.ponto : "", ct);
            }
            g_retangulo(j, 12, ty + PV_LIN_A - 4, j->larg - 24, 1, 0xE4E7EA);
        }
    }

    /* Botones y estado. */
    for (int i = 0; i < PV_NBT; i++) botao_fila_l(pv_bt_x(i), pv_bt_y(), PV_W_BT, pv_bt_texto[i], pv_pode(i));
    int sy = j->alt - 40;
    g_retangulo(j, 0, sy - 4, j->larg, 1, 0xDDE1E5);
    if (pv_prog >= 0) {
        g_retangulo(j, 12, sy + 4, j->larg - 24, 18, 0xD5D9DD);
        g_retangulo(j, 12, sy + 4, (j->larg - 24) * pv_prog / 100, 18, COR_DESTAQUE);
        g_moldura(j, 12, sy + 4, j->larg - 24, 18, COR_BORDA);
        char t[32];
        formatar_texto(t, sizeof t, "Formateando... %d %%", pv_prog);
        g_texto(j, 20, sy + 5, t, COR_TEXTO);
    } else if (pv_msg[0]) {
        g_retangulo(j, 12, sy + 6, 10, 10, pv_msg_erro ? 0xD03A2F : COR_DESTAQUE);
        g_texto(j, 28, sy + 2, pv_msg, pv_msg_erro ? 0xC0392B : COR_TEXTO);
    } else g_texto(j, 14, sy + 2, "Elige una partición o un espacio libre (botón derecho: menú).", COR_TEXTO_FRACO);
    jan_mostrar(j);
}

/* Registro en el log serie (para las pruebas): dónde cae cada segmento y cada botón. */
static void pv_registrar(void)
{
    registrar("discos: particiones de %s, esquema %d, %d segmentos, %d deshacer", pv_disco, pv_t.esquema, pv_nseg, pv_passos);
    for (int i = 0; i < pv_nseg; i++)
        registrar("discos: seg %d num=%d x=%d l=%d y=%d inicio=%u setores=%u logica=%d", i, pv_seg[i].num, pv_seg[i].x, pv_seg[i].l,
                  pv_seg[i].logica ? PV_BARRA_Y + 18 : PV_BARRA_Y, pv_seg[i].inicio, pv_seg[i].setores, pv_seg[i].logica);
    registrar("discos: botones y=%d x0=%d paso=%d", pv_bt_y(), pv_bt_x(0), PV_W_BT + 6);
}

static void pv_atualizar(void)
{
    pv_ler();
    pv_desenhar();
    pv_registrar();
}

static void pv_progresso(int pc, void *ctx)
{
    (void)ctx;
    if (pc == pv_prog) return;
    pv_prog = pc;
    pv_desenhar();
}

/* ¿Hay que desmontar la partición n para esta operación? (D213: solo lo que se toca.) */
static bool pv_afeta(const pt_acao_t *a, int n)
{
    if (a->op == PA_TABELA_NOVA || a->op == PA_TABELA_GPT) return true;
    if (a->op != PA_APAGAR && a->op != PA_REDIMENSIONAR && a->op != PA_FORMATAR && a->op != PA_TRIM) return false;
    if (!a->numero) return false;
    if (n == a->numero) return true;
    const pt_part_t *p = a->numero <= 4 ? &pv_t.prim[a->numero - 1] : NULL;
    return a->op == PA_APAGAR && p && pt_tipo_extendida(p->tipo) && n >= 5;   /* borrar la extendida: sus lógicas */
}

/* Antes de cambiar: lo montado que se va a tocar se desmonta (con permiso) y el disco de
 * arranque pide dos síes. */
static bool pv_preparar(pt_acao_t *a)
{
    info_disco_t d;
    if (!pv_info_de(pv_disco, &d)) return false;
    if (d.flags & DISCO_ARRANQUE_REAL) {
        char t[300];
        formatar_texto(t, sizeof t, "%s es el disco del que ha arrancado SaguiOS. Si te equivocas, el sistema puede no volver a arrancar. ¿Seguir?", pv_disco);
        if (!pv_confirmar("Disco de arranque", t)) return false;
        if (!pv_confirmar("Disco de arranque (segunda confirmación)", "¿De verdad quieres cambiar el disco de arranque? Esta es la última pregunta.")) return false;
        a->conf |= DBLOCO_CONF_ARRANQUE;
    }
    if (d.montado) {
        char t[300], lista[160] = "";
        char nomes[8][16];
        int nn = 0;
        info_disco_t p;
        for (int i = 0; listar_discos(i, &p) == 1; i++)
            if (p.particao && compara_texto(p.pai, pv_disco) == 0 && p.montado && pv_afeta(a, (int)p.particao) && nn < 8) {
                char x[48];
                formatar_texto(x, sizeof x, "%s%s (%s)", lista[0] ? ", " : "", p.nome, p.ponto);
                junta_texto(lista, x, sizeof lista);
                copia_texto(nomes[nn++], p.nome, sizeof nomes[0]);
            }
        if (!nn) return true;   /* lo montado no se toca: el kernel lo protege (D213) */
        formatar_texto(t, sizeof t, "Para esto hay que desmontar antes: %s. ¿Desmontar ahora?", lista);
        if (!pv_confirmar("Particiones montadas", t)) return false;
        for (int i = 0; i < nn; i++) {
            int r = COM_ADMIN("desmontar para particionar", desmontar(nomes[i]));
            registrar("discos: desmontar %s para particionar -> %d", nomes[i], r);
            if (r < 0) {
                formatar_texto(t, sizeof t, "No se pudo desmontar %s: %s. No se ha cambiado nada.", nomes[i], nome_erro(r));
                pv_mensagem("Particiones", t);
                return false;
            }
        }
    }
    return true;
}

/* D269: la vista de particiones lee y escribe los discos en crudo: pide permisos con el diálogo del sistema
 * y sigue siendo administrador mientras esté abierta (se suelta al salir de ella). */
static int pv_pedir_admin(void)
{
    if (sou_root()) return 0;
    return elevar_grafico("leer y modificar las particiones y los discos");
}

static void pv_executar(pt_acao_t *a)
{
    copia_texto(a->disco, pv_disco, sizeof a->disco);
    if (pv_pedir_admin() < 0) { copia_texto(pv_msg, "Cancelado: hacen falta permisos de administrador.", sizeof pv_msg); pv_msg_erro = true; pv_atualizar(); return; }
    if (!pv_preparar(a)) { copia_texto(pv_msg, "Cancelado: no se ha cambiado nada.", sizeof pv_msg); pv_msg_erro = false; pv_atualizar(); return; }
    a->progresso = pv_progresso;
    pv_prog = -1;
    int r = pt_executar(a, pv_msg, sizeof pv_msg);
    pv_prog = -1;
    pv_msg_erro = r < 0;
    if (r < 0 && !pv_msg[0]) copia_texto(pv_msg, nome_erro(r), sizeof pv_msg);
    registrar("discos: particiones op %d en %s -> %d: %s", a->op, pv_disco, r, pv_msg);
    if (r < 0) pv_mensagem("No se ha podido", pv_msg);
    pv_atualizar();
}

/* Qué tiene una partición, dicho para el diálogo. */
static void pv_descrever(const pv_seg_t *s, char *t, int max)
{
    char n[16], tm[16];
    info_disco_t d;
    pv_nome_seg(s, n, sizeof n);
    pv_tamanho(s->setores, tm, sizeof tm);
    const pt_part_t *p = pv_part(s);
    if (p && pt_tipo_extendida(p->tipo)) {
        formatar_texto(t, (size_t)max, "%s (%s, extendida con %d lógica%s)", n, tm, pv_t.nlog, pv_t.nlog == 1 ? "" : "s");
        return;
    }
    if (pv_info_de(n, &d) && d.suportado) formatar_texto(t, (size_t)max, "%s (%s, %s%s%s%s)", n, tm, d.formato,
                                                         d.rotulo[0] ? " \"" : "", d.rotulo, d.rotulo[0] ? "\"" : "");
    else formatar_texto(t, (size_t)max, "%s (%s, %s)", n, tm, p ? pt_nome_tipo(p->tipo) : "libre");
}

static int pv_pedir_mb(const char *titulo, const char *pergunta, uint32_t padrao)
{
    char r[16];
    formatar_texto(r, sizeof r, "%u", padrao);
    char q[300];
    pv_quebrar(pergunta, q, sizeof q, 42);
    if (!pedir_texto(titulo, q, r, sizeof r)) return -1;
    int v;
    if (!texto_para_numero(r, &v) || v <= 0) { pv_mensagem(titulo, "El tamaño tiene que ser un número de MB."); return -1; }
    return v;
}


static void pv_criar(int escolha)
{
    if (pv_sel < 0) return;
    pv_seg_t s = pv_seg[pv_sel];
    pt_acao_t a;
    memset(&a, 0, sizeof a);
    a.op = PA_CRIAR;
    a.logica = s.logica;
    a.inicio = s.inicio;
    a.fs = escolha == 0 || escolha == 4 ? PA_FS_FAT32 : escolha == 5 ? PA_FS_EXFAT : escolha == 1 ? PA_FS_EXT2 : PA_FS_NENHUM;
    if (escolha == 3) a.tipo = 0x0F;
    if (escolha == 4) a.tipo = 0xEF;   /* D266: partición EFI (ESP), FAT32 */
    char p[200];
    formatar_texto(p, sizeof p, "Tamaño de la partición nueva en MB (hay %u MB libres aquí):", pv_mb(s.setores));
    int mb = pv_pedir_mb("Crear partición", p, pv_mb(s.setores));
    if (mb < 0) return;
    if (a.fs != PA_FS_NENHUM) {
        char r[20] = "DATOS";
        if (escolha == 4) copia_texto(r, "SAGUI-ESP", sizeof r);
        if (!pedir_texto("Crear partición", "Etiqueta del volumen (puede quedar vacía):", r, sizeof r)) return;
        copia_texto(a.rotulo, r, sizeof a.rotulo);
    }
    a.setores = (uint32_t)mb * 2048;
    char t[300];
    formatar_texto(t, sizeof t, "Se creará una partición %s de %d MB%s%s en el espacio libre que empieza en %u MB de %s. No se borra nada que esté en uso.",
                   escolha == 3 ? "extendida" : escolha == 4 ? "EFI (ESP, para el cargador UEFI)" : a.logica ? "lógica" : pv_t.esquema == PT_GPT ? "GPT" : "primaria", mb,
                   a.fs ? ", formateada en " : "", a.fs ? pt_nome_fs(a.fs) : "", s.inicio / 2048, pv_disco);
    if (!pv_confirmar("Crear partición", t)) return;
    pv_executar(&a);
}

static void pv_formatear(int escolha)
{
    if (pv_sel < 0 || !pv_seg[pv_sel].num) return;
    pv_seg_t s = pv_seg[pv_sel];
    pt_acao_t a;
    memset(&a, 0, sizeof a);
    a.op = PA_FORMATAR;
    a.numero = s.num;
    a.fs = escolha == 0 ? PA_FS_FAT32 : escolha == 2 ? PA_FS_EXFAT : PA_FS_EXT2;
    char r[20] = "";
    info_disco_t d;
    char n[16];
    pv_nome_seg(&s, n, sizeof n);
    if (pv_info_de(n, &d)) copia_texto(r, d.rotulo, sizeof r);
    if (!pedir_texto("Formatear", "Etiqueta del volumen (puede quedar vacía):", r, sizeof r)) return;
    copia_texto(a.rotulo, r, sizeof a.rotulo);
    char q[120], t[460];
    pv_descrever(&s, q, sizeof q);
    formatar_texto(t, sizeof t, "Se va a formatear %s en %s. SE BORRA TODO LO QUE HAY DENTRO y no se puede deshacer.%s%s",
                   q, pt_nome_fs(a.fs), pv_bloco.trim == TRIM_SIM ? " (El disco admite TRIM: el contenido viejo se descarta.)" : "",
                   pv_es_conf(&s) ? " ATENCION: es SAGUI-CONF, la configuración del sistema: se pierden usuarios, contraseñas y claves."
                   : pv_es_esp(&s) ? " ATENCION: es la partición EFI (ESP): lleva el cargador UEFI; sin él este disco no arranca por UEFI." : "");
    if (!pv_confirmar("Formatear", t)) return;
    pv_executar(&a);
}

/* D397: «Reparar...»: pasa «reparar -n» (dice qué está mal, sin escribir), enseña el resultado y, si hay algo y
 * se confirma, «reparar -y» (desmonta, repara, comprueba y vuelve a montar). Las dos van por /bin/reparar con la
 * salida a un archivo de /tmp, que se muestra en el diálogo. */
static int pv_reparar_correr(const char *vol, const char *modo, char *saida, int max)
{
    const char *arq = "/tmp/reparar-discos.txt";
    char *argv[] = { "reparar", (char *)modo, (char *)vol, "--salida", (char *)arq, NULL };
    int pid = executar("/bin/reparar", argv), cod = 8;
    saida[0] = 0;
    if (pid < 0) { formatar_texto(saida, (size_t)max, "No se pudo ejecutar reparar: %s", nome_erro(pid)); return 8; }
    esperar(pid, &cod);
    int fd = abrir(arq, ABRIR_LER);
    if (fd >= 0) {
        char *grande = aloca(8192);
        int n = grande ? ler(fd, grande, 8191) : 0;
        if (n < 0) n = 0;
        if (grande) {
            grande[n] = 0;
            /* lo importante está al final (resumen, resultado): se enseña el último trozo */
            int ini = n > max - 1 ? n - (max - 1) : 0;
            while (ini < n && ini > 0 && grande[ini - 1] != '\n') ini++;
            copia_texto(saida, grande + ini, (size_t)max);
            libera(grande);
        }
        fechar(fd);
    }
    apagar(arq);
    return cod;
}

static void pv_reparar(const pv_seg_t *s)
{
    char vol[16], txt[380];
    pv_nome_seg(s, vol, sizeof vol);
    if (pv_pedir_admin() < 0) { pv_mensagem("Reparar", "Hacen falta permisos de administrador."); return; }
    int cod = pv_reparar_correr(vol, "-n", txt, sizeof txt);
    registrar("discos: reparar -n %s -> %d", vol, cod);
    if (cod == 0) { pv_mensagem("Reparar", "Todo cuadra: no hay nada que reparar."); return; }
    if (cod != 4) { pv_mensagem("Reparar", txt[0] ? txt : "No se pudo comprobar el volumen."); return; }
    char q[600];
    formatar_texto(q, sizeof q, "%s\n\n¿Reparar %s? Se desmontará, se arreglará y se volverá a montar. Antes se guarda una copia de lo que cambia en /tmp/reparar.", txt, vol);
    if (!pv_confirmar("Reparar", q)) return;
    cod = pv_reparar_correr(vol, "-y", txt, sizeof txt);
    registrar("discos: reparar -y %s -> %d", vol, cod);
    formatar_texto(q, sizeof q, "%s", txt[0] ? txt : (cod == 1 ? "Reparado." : "No se pudo reparar."));
    pv_msg_erro = cod != 1 && cod != 0;
    copia_texto(pv_msg, cod == 1 ? "Reparado: el volumen está limpio." : "Reparar no ha podido arreglarlo del todo (mira el registro).", sizeof pv_msg);
    pv_mensagem("Reparar", q);
    pv_atualizar();
}

static void pv_acao(int bt)
{
    if (!pv_pode(bt)) return;
    pv_seg_t *s = pv_sel >= 0 ? &pv_seg[pv_sel] : NULL;
    pt_acao_t a;
    memset(&a, 0, sizeof a);
    char q[120], t[460];
    if (s) { a.numero = s->num; pv_descrever(s, q, sizeof q); }
    switch (bt) {
    case PV_CRIAR: {
        bool ext_ok = s && !s->logica && pt_extendida(&pv_t) < 0 && pv_t.esquema != PT_GPT;
        char m[128];
        formatar_texto(m, sizeof m, "Crear FAT32\nCrear ext2\nCrear sin formato\n%sCrear extendida\nCrear partición EFI (ESP)\nCrear exFAT", ext_ok ? "" : "~");
        menu_contextual(j->id, pv_bt_x(PV_CRIAR), pv_bt_y() - 4, m, MENU_CRIAR);
        return;
    }
    case PV_REPARAR:
        pv_reparar(s);
        return;
    case PV_FORMATEAR:
        menu_contextual(j->id, pv_bt_x(PV_FORMATEAR), pv_bt_y() - 4, "Formatear en FAT32\nFormatear en ext2\nFormatear en exFAT", MENU_FMT);
        return;
    case PV_BORRAR: {
        a.op = PA_APAGAR;
        const pt_part_t *p = pv_part(s);
        if (p && pt_tipo_extendida(p->tipo) && pv_t.nlog)
            formatar_texto(t, sizeof t, "Se borrará %s Y TODAS SUS LÓGICAS. Lo que tienen dentro deja de verse. Se puede deshacer mientras no se escriba en ese espacio.", q);
        else
            formatar_texto(t, sizeof t, "Se borrará %s. Lo que tiene dentro deja de verse. Se puede deshacer (botón Deshacer) mientras no se escriba en ese espacio.%s", q,
                           pv_es_conf(s) ? " ATENCION: es SAGUI-CONF, la configuración del sistema: sin ella se pierden usuarios, contraseñas y claves."
                           : pv_es_esp(s) ? " ATENCION: es la partición EFI (ESP): lleva el cargador UEFI; sin ella este disco no arranca por UEFI." : "");
        if (!pv_confirmar("Borrar partición", t)) return;
        break;
    }
    case PV_REDIM: {
        a.op = PA_REDIMENSIONAR;
        char p[300], tipo_fs[16], n[16];
        pv_nome_seg(s, n, sizeof n);
        uint32_t ocupa = s->ext ? 0 : fs_tamanho(n, tipo_fs, sizeof tipo_fs);
        /* Hasta dónde puede crecer: hasta el siguiente segmento ocupado. */
        uint32_t lim = s->inicio + s->setores;
        for (int i = pv_sel + 1; i < pv_nseg; i++) {
            if (pv_seg[i].logica != s->logica && !s->ext) break;
            if (s->ext && pv_seg[i].logica) continue;
            if (pv_seg[i].num) break;
            lim = pv_seg[i].inicio + pv_seg[i].setores;
            break;
        }
        bool fat = ocupa && compara_texto(tipo_fs, "FAT32") == 0;
        uint32_t minimo = !ocupa ? 0 : fat ? ocupa : fs_ext2_minimo(n);
        if (ocupa) formatar_texto(p, sizeof p, "Tamaño nuevo de %s en MB (ahora %u; como mucho unos %u; el %s %s %u MB):",
                                  n, pv_mb(s->setores), pv_mb(lim - s->inicio), fat ? "FAT32" : "ext2",
                                  fat ? "no se encoge por debajo de" : "se puede encoger sin mover datos hasta unos", (minimo + 2047) / 2048);
        else formatar_texto(p, sizeof p, "Tamaño nuevo de %s en MB (ahora %u; como mucho unos %u):", n, pv_mb(s->setores), pv_mb(lim - s->inicio));
        int mb = pv_pedir_mb("Redimensionar", p, pv_mb(s->setores));
        if (mb < 0) return;
        a.setores = (uint32_t)mb * 2048;
        formatar_texto(t, sizeof t, "%s pasará a medir %d MB. %s Antes se comprueba el sistema de archivos; si no está sano, no se toca nada.", q, mb,
                       (uint32_t)mb < pv_mb(s->setores) ? "Un ext2 se encoge con ella si lo que se corta está libre; un FAT32 no se encoge."
                                                       : "Un ext2 crece con ella; un FAT32, si su FAT da para ello.");
        if (!pv_confirmar("Redimensionar", t)) return;
        break;
    }
    case PV_ACTIVA:
        a.op = PA_ATIVAR;
        formatar_texto(t, sizeof t, "%s será la partición activa (la que arranca la BIOS con un MBR clásico).", q);
        if (!pv_confirmar("Partición activa", t)) return;
        break;
    case PV_TRIM:
        a.op = PA_TRIM;
        if (s->num) formatar_texto(t, sizeof t, "Se descartará (TRIM) TODO el contenido de %s. El SSD lo borra de verdad: no se puede recuperar ni deshacer.", q);
        else formatar_texto(t, sizeof t, "Se descartará (TRIM) el espacio libre de %s. Las particiones no se tocan.", pv_disco);
        if (!pv_confirmar("Descartar (TRIM)", t)) return;
        if (!s->num) a.numero = 0;
        break;
    case PV_AMPLIAR: {
        a.op = PA_AMPLIAR;
        a.numero = 0;
        int x = pt_extendida(&pv_t);
        formatar_texto(t, sizeof t, "La extendida de %s pasará de %u a %u MB (hasta el final del disco). No se mueve ni se borra nada; "
                       "en el hueco nuevo se pueden crear lógicas.", pv_disco, pv_mb(pv_t.prim[x].setores),
                       pv_mb(pv_fim_ampliar() - pv_t.prim[x].inicio));
        if (!pv_confirmar("Ampliar la extendida", t)) return;
        break;
    }
    case PV_DESHACER:
        a.op = PA_DESHACER;
        a.numero = 0;
        formatar_texto(t, sizeof t, "Se volverá a la tabla de %s de antes del último cambio. Los datos que borró un formateo no vuelven.", pv_disco);
        if (!pv_confirmar("Deshacer", t)) return;
        break;
    }
    pv_executar(&a);
}

static void pv_menu_contexto(const evento_t *e)
{
    char m[360];
    formatar_texto(m, sizeof m, "%sCrear...\n%sBorrar\n%sRedimensionar...\n%sFormatear...\n%sMarcar como activa\n%sDescartar (TRIM)\n-\n%sDeshacer el último cambio\n%sTabla nueva MBR (vacía)...\n%sTabla nueva GPT (vacía)...\n%sAmpliar la extendida hasta el final",
                   pv_pode(PV_CRIAR) ? "" : "~", pv_pode(PV_BORRAR) ? "" : "~", pv_pode(PV_REDIM) ? "" : "~",
                   pv_pode(PV_FORMATEAR) ? "" : "~", pv_pode(PV_ACTIVA) ? "" : "~", pv_pode(PV_TRIM) ? "" : "~",
                   pv_pode(PV_DESHACER) ? "" : "~", pv_disco[0] ? "" : "~", pv_disco[0] ? "" : "~", pv_pode(PV_AMPLIAR) ? "" : "~");
    menu_contextual(j->id, e->x, e->y, m, MENU_PART);
}

static void pv_tabela_nova(bool gpt)
{
    char t[420];
    formatar_texto(t, sizeof t, "Se creará una tabla %s vacía en %s: TODAS sus particiones desaparecen.%s%s",
                   gpt ? "GPT (con MBR protector)" : "MBR", pv_disco,
                   pv_t.esquema == PT_GPT ? " El disco es GPT: también se borra la GPT." : " (Con MBR se puede deshacer mientras no se escriba nada.)",
                   gpt ? " Una GPT necesita un cargador UEFI para arrancar (la BIOS clásica no la usa)." : "");
    if (!pv_confirmar("Tabla nueva", t)) return;
    pt_acao_t a;
    memset(&a, 0, sizeof a);
    a.op = gpt ? PA_TABELA_GPT : PA_TABELA_NOVA;
    if (pv_t.esquema == PT_GPT && !pv_confirmar("Tabla nueva", "El disco es GPT. ¿Seguro que quieres borrar la GPT entera?")) return;
    pv_executar(&a);
}

static void pv_tratar_menu(int codigo, int indice)
{
    if (indice < 0) return;
    if (codigo == MENU_CRIAR) { pv_criar(indice); return; }
    if (codigo == MENU_FMT) { pv_formatear(indice); return; }
    if (codigo != MENU_PART) return;
    static const int mapa[] = { PV_CRIAR, PV_BORRAR, PV_REDIM, PV_FORMATEAR, PV_ACTIVA, PV_TRIM, -1, PV_DESHACER, -2, -3, PV_AMPLIAR };
    if (indice >= (int)(sizeof mapa / sizeof mapa[0])) return;
    if (mapa[indice] == -2) pv_tabela_nova(false);
    else if (mapa[indice] == -3) pv_tabela_nova(true);
    else if (mapa[indice] >= 0) pv_acao(mapa[indice]);
}

/* Un clic en la vista de particiones. */
static void pv_clique(const evento_t *e)
{
    if (e->y < BARRA_TOPO) {
        if (e->botoes != 1 || e->y < 6 || e->y >= 6 + BT_A) return;
        if (e->x >= 8 && e->x < 92) { vista_part = false; elevacao_soltar(); atualizar(true); registrar("discos: vista de uso"); return; }
        if (e->x >= 98 && e->x < 198) { pv_msg[0] = 0; pv_atualizar(); return; }
        for (int i = 0; i < pv_ndiscos; i++) {
            int x = 262 + i * 68;
            if (e->x >= x && e->x < x + 62) {
                copia_texto(pv_disco, pv_discos[i], sizeof pv_disco);
                pv_sel = -1; pv_msg[0] = 0;
                pv_atualizar();
                return;
            }
        }
        return;
    }
    /* Botones de abajo. */
    if (e->y >= pv_bt_y() && e->y < pv_bt_y() + BT_A && e->botoes == 1) {
        for (int i = 0; i < PV_NBT; i++)
            if (e->x >= pv_bt_x(i) && e->x < pv_bt_x(i) + PV_W_BT) { pv_acao(i); return; }
        return;
    }
    int novo = -2;
    /* La barra: primero lo de dentro de la extendida. */
    if (e->y >= PV_BARRA_Y && e->y < PV_BARRA_Y + PV_BARRA_A) {
        for (int i = 0; i < pv_nseg && novo == -2; i++)
            if (pv_seg[i].logica && e->y >= PV_BARRA_Y + 18 && e->x >= pv_seg[i].x && e->x < pv_seg[i].x + pv_seg[i].l) novo = i;
        for (int i = 0; i < pv_nseg && novo == -2; i++)
            if (!pv_seg[i].logica && e->x >= pv_seg[i].x && e->x < pv_seg[i].x + pv_seg[i].l) novo = i;
    }
    /* La lista. */
    int ty = PV_TAB_Y + PV_LIN_A;
    if (e->y >= ty - 3 && novo == -2) {
        int k = (e->y - ty + 3) / PV_LIN_A;
        if (k >= 0 && k < pv_nseg) novo = k;
    }
    if (novo == -2) return;
    pv_sel = novo;
    pv_desenhar();
    registrar("discos: particion elegida %d (num %d)", pv_sel, pv_sel >= 0 ? pv_seg[pv_sel].num : -1);
    if (e->botoes == 2) pv_menu_contexto(e);
}

static void pv_abrir(const char *disco)
{
    vista_part = true;
    if (disco) copia_texto(pv_disco, disco, sizeof pv_disco);
    pv_sel = -1;
    pv_msg[0] = 0;
    pv_atualizar();
}

static int janela(const char *disco_part, bool part)
{
    /* Alto (D257): el que pide el contenido (hay filas para todas las particiones:
     * juegos, apps, SAGUI-CONF...), recortado a la pantalla; lo que no cabe se
     * desplaza (rueda, barra, flechas, RePág/AvPág). La vista de particiones (D179)
     * es más ancha. */
    int alto = 640;
    if (!part) {
        info_video_t v;
        memset(&v, 0, sizeof v);
        info_sistema(INFO_VIDEO, &v, sizeof v);
        ler_estado();
        alto = BARRA_TOPO + altura_total();
        /* título (24) y barra de tareas: se dejan 100 px de la pantalla */
        if (v.altura > 200 && alto > (int)v.altura - 100) alto = (int)v.altura - 100;
        if (alto < 300) alto = 300;
        if (alto > 900) alto = 900;
    }
    j = jan_abrir("Discos", part ? 900 : 700, part ? 600 : alto);
    if (!j) { imprimir("discos: no se pudo abrir la ventana (¿hay escritorio?)\n"); return 1; }
    jan_estilo(j, JAN_REDIMENSIONAVEL, 520, 240);
    qtd = 0;
    if (part) pv_abrir(disco_part);
    else atualizar(true);
    registrar("discos: janela aberta, %d filas%s", qtd, part ? ", vista de particiones" : "");
    evento_t e;
    unsigned ultimo = tempo_ms();
    for (;;) {
        if (esperar_evento(j->id, &e, 200) != 1) {
            if (!vista_part && tempo_ms() - ultimo >= 2000) { ultimo = tempo_ms(); atualizar(false); }   /* refresco automático (y pendrives conectados) */
            continue;
        }
        if (e.tipo == EV_FECHAR) break;
        if (vista_part) {   /* D179 */
            if (jan_redimensionar(j, &e)) { pv_desenhar(); continue; }
            if (e.tipo == EV_MENU) { pv_tratar_menu((int)e.tecla, (int)e.x); continue; }
            if (e.tipo == EV_CLIQUE) { pv_clique(&e); continue; }
            if (e.tipo == EV_TECLA) {
                if (e.tecla == TECLA_F5_) { pv_msg[0] = 0; pv_atualizar(); }
                else if (e.tecla == TECLA_ESC) { vista_part = false; elevacao_soltar(); atualizar(true); }
                else if (e.tecla == TECLA_DELETE) pv_acao(PV_BORRAR);
                else if ((e.tecla == TECLA_CIMA || e.tecla == TECLA_BAIXO) && pv_nseg) {
                    pv_sel = pv_sel < 0 ? 0 : pv_sel + (e.tecla == TECLA_BAIXO ? 1 : -1);
                    if (pv_sel < 0) pv_sel = 0;
                    if (pv_sel >= pv_nseg) pv_sel = pv_nseg - 1;
                    pv_desenhar();
                }
            }
            continue;
        }
        if (jan_redimensionar(j, &e)) { limitar_rolagem(); desenhar(); continue; }
        if (e.tipo == EV_TELA) continue;
        if (e.tipo == EV_MENU) { tratar_menu((int)e.x); continue; }
        if (e.tipo == EV_RODA) {
            rolagem -= (int)e.cliques * RODA_LINHAS * 16;
            limitar_rolagem();
            desenhar();
            continue;
        }
        if (e.tipo == EV_TECLA) {
            if (e.tecla == TECLA_F5_) { aviso[0] = 0; atualizar(true); registrar("discos: actualizado (F5)"); }
            else if (e.tecla == TECLA_CIMA) { rolagem -= 24; limitar_rolagem(); desenhar(); }
            else if (e.tecla == TECLA_BAIXO) { rolagem += 24; limitar_rolagem(); desenhar(); }
            else if (e.tecla == TECLA_PGUP) { rolagem -= j->alt - BARRA_TOPO - 24; limitar_rolagem(); desenhar(); }
            else if (e.tecla == TECLA_PGDN) { rolagem += j->alt - BARRA_TOPO - 24; limitar_rolagem(); desenhar(); }
            continue;
        }
        if (e.tipo != EV_CLIQUE) continue;
        if (e.y < BARRA_TOPO) {
            if (e.botoes == 1 && e.x >= 144 && e.x < 254 && e.y >= 6 && e.y < 6 + BT_A) { pv_abrir(NULL); continue; }
            if (e.botoes == 1 && e.x >= 8 && e.x < 138 && e.y >= 6 && e.y < 6 + BT_A) {
                aviso[0] = 0;
                atualizar(true);
                registrar("discos: actualizado (boton)");
            }
            continue;
        }
        /* ¿Qué fila? */
        int y = BARRA_TOPO + 8 - rolagem;
        for (int i = 0; i < qtd; i++) {
            int h = lin[i].tipo == L_DISCO ? ALT_DISCO : ALT_VOL;
            if (e.y >= y && e.y < y + h && lin[i].tipo == L_VOL) {
                int rel = e.y - y;
                bool en_bt = rel >= 2 && rel < 2 + BT_A;
                bool abrir = e.botoes == 1 && en_bt && e.x >= bt_x() && e.x < bt_x() + BT_L;
                bool mnt = e.botoes == 1 && en_bt && !lin[i].ram && e.x >= bt_x() + BT_L + 8 && e.x < bt_x() + 2 * BT_L + 8
                           && (lin[i].suportado || lin[i].cd);
                if (abrir && !lin[i].suportado && !lin[i].ram) abrir = false;
                fila_clique(i, abrir, mnt, e.botoes == 2, &e);
                break;
            }
            y += h;
        }
    }
    elevacao_soltar();
    jan_fechar(j);
    return 0;
}

/* ¿Lo lanzó el escritorio (icono) y no un shell? El padre de un programa del terminal es sagsh. */
static bool lanzado_por_escritorio(void)
{
    static info_processo_t v[64];
    int n = info_sistema(INFO_PROCESSOS, v, 64);
    int yo = meu_pid(), pai = -1;
    for (int i = 0; i < n; i++) if (v[i].pid == yo) pai = v[i].pai;
    for (int i = 0; i < n; i++)
        if (v[i].pid == pai) return !comeca_com(v[i].nome, "sagsh");
    return true;
}

int principal(int argc, char **argv)
{
    bool ventana = false, texto = false, part = false;
    const char *disco_part = NULL;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--particiones") == 0) {
            ventana = part = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') disco_part = argv[++i];
        } else if (compara_texto(argv[i], "--ventana") == 0 || compara_texto(argv[i], "-v") == 0 || compara_texto(argv[i], "-g") == 0) ventana = true;
        else if (compara_texto(argv[i], "-t") == 0) texto = true;
        else if (compara_texto(argv[i], "-u") == 0) { barras = true; texto = true; }
        else { imprimir_em(ERROS, "uso: discos [-t] [-u] | discos --ventana | discos --particiones [disco]\n"); return compara_texto(argv[i], "-h") == 0 ? 0 : 1; }
    }
    if (!ventana && !texto && argc == 1) {
        /* Sin argumentos: ventana solo si lo lanzó el escritorio (la salida no es terminal/pipe/archivo). */
        info_video_t v;
        memset(&v, 0, sizeof v);
        info_sistema(INFO_VIDEO, &v, sizeof v);
        int t = tipo_descritor(SAIDA);
        if (v.escritorio && t != TIPO_PIPE && t != TIPO_ARQUIVO && lanzado_por_escritorio()) {
            ventana = true;
            registrar("discos: sin argumentos, lanzado por el escritorio (salida tipo %d): ventana", t);
        }
    }
    if (ventana) return janela(disco_part, part);
    ler_montagens();
    if (listar_grupo(G_PENDRIVES) == 0) imprimir("No hay ningún pendrive conectado.\n");
    info_disco_t d;
    bool tem = false, tem_cd = false;
    for (int i = 0; listar_discos(i, &d) == 1; i++) {
        if (grupo_de(&d) == G_INTERNOS) tem = true;
        if (grupo_de(&d) == G_CDS) tem_cd = true;
    }
    if (tem) {
        imprimir("Discos internos (solo lectura salvo \"mount -o rw\"):\n");
        listar_grupo(G_INTERNOS);
    }
    if (tem_cd) {   /* D207 */
        imprimir("CD/DVD (solo lectura; el de datos se monta solo en /cd; \"eject\" lo expulsa):\n");
        listar_grupo(G_CDS);
    }
    return 0;
}
