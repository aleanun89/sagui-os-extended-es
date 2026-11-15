// SPDX-License-Identifier: GPL-3.0-only
/* area_trabalho.c - el fondo, la mascota, los íconos y el menú del
 * escritorio (H20, H28, D051, D066).
 *
 * El fondo no cambia: se dibuja una vez en una superficie guardada (degradado
 * verde, amarillo y azul, el sagui en la esquina y los íconos), y en cada
 * redibujado sólo se copia la parte necesaria. La barra superior (barra.c) se
 * dibuja por encima, porque el reloj y las tareas cambian. */
#include "gui.h"
#include "arte.h"
#include "console.h"
#include "memoria.h"
#include "processo.h"
#include "relogio.h"
#include "texto.h"
#include "versao.h"
#include "video.h"
#include "disco.h"
#include "montagem.h"
#include "sistema.h"
#include "modos_video.h"
#include "vfs.h"
#include "montagem.h"
#include "teclado.h"
#include "mouse.h"

static uint32_t *fundo;
static uint32_t *base;   /* la imagen de fondo elegida (v5.2), del tamaño de la pantalla; NULL = degradado */
static int larg, alt;

typedef struct {
    const char *nome;
    const char *programa;
    const desenho_t *desenho;
    const char *chave;   /* clave estable en escritorio.conf (ASCII, minúsculas) */
    int tipo;            /* para ordenar por tipo: 0 carpetas y unidades, 1 documentos, 2 programas (D268) */
} icone_t;

static const icone_t icones[] = {
    { "Archivos",    "/bin/arquivos",    &arte_icone_arquivos,   "archivos",  0 },
    { "Terminal",    "/bin/terminal",    &arte_icone_terminal,   "terminal",  2 },
    { "Editor",      "/bin/editor",      &arte_icone_editor,     "editor",    1 },
    { "Calculadora", "/bin/calculadora", &arte_icone_calculadora, "calculadora", 2 },
    { "Banana",      "/bin/banana",      &arte_icone_banana,     "banana",    2 },
    { "Sagui 3D",    "/bin/sagui3d",     &arte_icone_sagui3d,    "sagui3d",   2 },   /* bonus B4 */
    { "Acerca de",   "/bin/sobre-o-sistema",   &arte_icone_sobre, "acerca",   2 },
    { "Pendrive",    "/bin/arquivos",    &arte_icone_pendrive,   "pendrive",  0 },   /* sólo aparece con un pendrive conectado (H34) */
    { "Tareas",      "/bin/tareas",      &arte_icone_tarefas,    "tareas",    2 },   /* segunda columna (D043) */
    { "Visor",       "/bin/visor",       &arte_icone_visor,      "visor",     1 },   /* "Visor de imágenes" (D055): el nombre entero chocaría con el de la columna 1 */
    { "Paint",       "/bin/paint",       &arte_icone_paint,      "paint",     1 },   /* D056 */
    { "CHIP-8",      "/bin/chip8",       &arte_icone_chip8,      "chip8",     2 },   /* D070 */
    { "Configuración", "/bin/configuracion", &arte_icone_configuracion, "configuracion", 2 },   /* D112 */
    { "Discos",      "/bin/discos",      &arte_icone_discos,     "discos",    0 },   /* D121: sin argumentos y con salida no-terminal abre la ventana */
};
#define QTD_ICONES (int)(sizeof icones / sizeof icones[0])
#define ICONE_PENDRIVE 7
static bool pendrive_visivel;
static char nome_pendrive[24] = "Pendrive";

static bool icone_visivel(int i) { return i != ICONE_PENDRIVE || pendrive_visivel; }
#define ICONE_X 20
#define ICONE_Y0 (BARRA_SUPERIOR + 16)
#define ICONE_PASSO 84
#define ICONE_L 96
#define ICONE_A 88

/* Cuántos íconos caben en una columna (los demás van a la columna de al lado):
 * 8 en 768 líneas, como siempre; depende de la altura porque la pantalla puede
 * cambiar de modo (D059). */
static int icones_por_coluna(void)
{
    int n = (alt - ICONE_Y0) / ICONE_PASSO;
    return n < 1 ? 1 : n;
}

int area_qtd_icones(void) { return QTD_ICONES; }

/* ---- Casillas, selección y orden de los iconos (D268, mejora 101) ----
 *
 * Cada icono ocupa una casilla (columna, fila) de una rejilla de ICONE_L+8 por ICONE_PASSO
 * píxeles. «ic_*» es la casilla efectiva en esta resolución; «id_*» es la que la persona pidió
 * (la que se guarda): si al cambiar de modo (D059) todas siguen cabiendo, se usan tal cual, y si no
 * se recolocan todas respetando el orden relativo (columna a columna). Mientras nadie toque nada
 * («manual» falso) valen las casillas de siempre, por índice. */
#define CEL_L (ICONE_L + 8)
#define N_ICONES ((int)(sizeof icones / sizeof icones[0]))
enum { ORD_MANUAL, ORD_NOMBRE, ORD_TIPO, ORD_FECHA };
static int ic_c[sizeof icones / sizeof icones[0]], ic_f[sizeof icones / sizeof icones[0]];
static int id_c[sizeof icones / sizeof icones[0]], id_f[sizeof icones / sizeof icones[0]];
static bool manual;            /* hay casillas guardadas o movidas a mano */
static int criterio;           /* ORD_* */
static bool automatica;        /* organización automática: se reordena tras cada cambio */
static bool sel[sizeof icones / sizeof icones[0]];
static bool foco_esc;          /* el teclado es del escritorio (flechas e Intro) */
static int foco_ic = -1;
/* Arrastre: 1 = pulsado sobre un icono (aún sin mover), 2 = moviendo iconos, 3 = rectángulo de selección. */
static int arr_estado, arr_ic, arr_x0, arr_y0, arr_dc, arr_df;
static bool arr_sozinho;       /* pulsó un icono ya seleccionado de un grupo: al soltar sin mover queda solo él */
static bool banda_vis;
static int banda_x0, banda_y0, banda_x1, banda_y1;
static bool sel_base[sizeof icones / sizeof icones[0]];
static bool arr_ctrl;
#define CONF_ESCRITORIO "/home/.config/escritorio.conf"
#define CONF_NOVO       "/home/.config/escritorio.conf.nuevo"

static int colunas(void)
{
    int n = (larg - ICONE_X) / CEL_L;
    return n < 1 ? 1 : n;
}

static bool cel_valida(int c, int f) { return c >= 0 && c < colunas() && f >= 0 && f < icones_por_coluna(); }

/* Rectángulo de la casilla (c, f). */
static void cel_rect(int c, int f, int *x, int *y)
{
    *x = ICONE_X + c * CEL_L;
    *y = ICONE_Y0 + f * ICONE_PASSO;
}

/* Casilla visual: durante el arrastre, los seleccionados van con el desplazamiento del ratón. */
static void cel_visual(int i, int *c, int *f)
{
    *c = ic_c[i]; *f = ic_f[i];
    if (arr_estado == 2 && sel[i]) { *c += arr_dc; *f += arr_df; }
}

void area_icone_retangulo(int i, int *x, int *y, int *l, int *a)
{
    int c, f;
    cel_visual(i, &c, &f);
    cel_rect(c, f, x, y);
    *l = ICONE_L; *a = ICONE_A;
}

static void casillas_padrao(void)
{
    int n = icones_por_coluna();
    for (int i = 0; i < N_ICONES; i++) {
        ic_c[i] = id_c[i] = i / n;
        ic_f[i] = id_f[i] = i % n;
    }
}

/* ¿Hay otro icono visible en la casilla? Devuelve su índice o -1. */
static int ocupante(int c, int f, int exceto)
{
    for (int i = 0; i < N_ICONES; i++)
        if (i != exceto && icone_visivel(i) && ic_c[i] == c && ic_f[i] == f) return i;
    return -1;
}

/* La casilla libre más cercana a (c, f); con empate, la primera por columnas. */
static bool livre_perto(int c, int f, int exceto, int *oc, int *of)
{
    int melhor = -1, mc = 0, mf = 0;
    for (int cc = 0; cc < colunas(); cc++)
        for (int ff = 0; ff < icones_por_coluna(); ff++) {
            if (ocupante(cc, ff, exceto) >= 0) continue;
            int d = (cc - c) * (cc - c) * 4 + (ff - f) * (ff - f);
            if (melhor < 0 || d < melhor) { melhor = d; mc = cc; mf = ff; }
        }
    if (melhor < 0) return false;
    *oc = mc; *of = mf;
    return true;
}

static int minuscula(int ch) { return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch; }

static const char *nome_do_icone(int i) { return i == ICONE_PENDRIVE ? nome_pendrive : icones[i].nome; }

static int compara_nomes(int a, int b)
{
    const unsigned char *x = (const unsigned char *)nome_do_icone(a), *y = (const unsigned char *)nome_do_icone(b);
    while (*x && minuscula(*x) == minuscula(*y)) { x++; y++; }
    return minuscula(*x) - minuscula(*y);
}

static uint32_t data_do_icone(int i)
{
    no_t *n = vfs_achar(icones[i].programa);
    return n ? n->modificado : 0;
}

/* ¿a va antes que b según el criterio? */
static bool antes(int a, int b, int crit)
{
    if (crit == ORD_TIPO && icones[a].tipo != icones[b].tipo) return icones[a].tipo < icones[b].tipo;
    if (crit == ORD_FECHA) {
        uint32_t da = data_do_icone(a), db = data_do_icone(b);
        if (da != db) return da > db;   /* lo más reciente primero */
    }
    return compara_nomes(a, b) < 0;
}

/* Reparte los iconos visibles, en el orden dado, por las casillas columna a columna. */
static void repartir(const int *ordem, int n)
{
    int linhas = icones_por_coluna();
    for (int k = 0; k < n; k++) {
        ic_c[ordem[k]] = k / linhas;
        ic_f[ordem[k]] = k % linhas;
    }
}

static int visiveis(int *v)
{
    int n = 0;
    for (int i = 0; i < N_ICONES; i++) if (icone_visivel(i)) v[n++] = i;
    return n;
}

static void fixar_casillas(void)
{
    for (int i = 0; i < N_ICONES; i++) { id_c[i] = ic_c[i]; id_f[i] = ic_f[i]; }
}

/* Ordena por el criterio (ORD_NOMBRE, ORD_TIPO, ORD_FECHA). */
static void ordenar(int crit)
{
    int v[N_ICONES], n = visiveis(v);
    for (int i = 1; i < n; i++) {
        int x = v[i], j = i - 1;
        while (j >= 0 && antes(x, v[j], crit)) { v[j + 1] = v[j]; j--; }
        v[j + 1] = x;
    }
    repartir(v, n);
    criterio = crit;
    manual = true;
    fixar_casillas();
}

/* «Alinear a la rejilla»: quita los huecos, conservando el orden actual (columna a columna). */
static void compactar(bool pelo_pedido)
{
    int v[N_ICONES], n = visiveis(v);
    for (int i = 1; i < n; i++) {
        int x = v[i], j = i - 1;
        int kx = pelo_pedido ? id_c[x] * 1000 + id_f[x] : ic_c[x] * 1000 + ic_f[x];
        while (j >= 0 && (pelo_pedido ? id_c[v[j]] * 1000 + id_f[v[j]] : ic_c[v[j]] * 1000 + ic_f[v[j]]) > kx) {
            v[j + 1] = v[j]; j--;
        }
        v[j + 1] = x;
    }
    repartir(v, n);
}

/* Tras cargar o cambiar la resolución: las casillas pedidas si caben todas, y si no, recolocadas. */
static void ajustar_casillas(void)
{
    if (!manual) { casillas_padrao(); return; }
    if (automatica && criterio != ORD_MANUAL) { int c = criterio; ordenar(c); return; }
    bool cabe = true;
    for (int i = 0; i < N_ICONES && cabe; i++) {
        if (!icone_visivel(i)) continue;
        if (!cel_valida(id_c[i], id_f[i])) { cabe = false; break; }
        for (int j = 0; j < i; j++)
            if (icone_visivel(j) && id_c[j] == id_c[i] && id_f[j] == id_f[i]) cabe = false;
    }
    if (cabe) {
        for (int i = 0; i < N_ICONES; i++) { ic_c[i] = id_c[i]; ic_f[i] = id_f[i]; }
    } else {
        compactar(true);
    }
}

static void registrar_casillas(const char *motivo)
{
    char t[320];
    int n = snformatar(t, sizeof t, "[ICONOS] %s:", motivo);
    for (int i = 0; i < N_ICONES; i++) {
        if (!icone_visivel(i) || n + 24 >= (int)sizeof t) continue;
        n += snformatar(t + n, sizeof t - (size_t)n, " %s=%d,%d", icones[i].chave, ic_c[i], ic_f[i]);
    }
    klog("%s\n", t);
}

/* ---- Persistencia: /home/.config/escritorio.conf ----
 *   orden=manual|nombre|tipo|fecha
 *   auto=si|no
 *   <clave>=<columna>,<fila>        (una línea por icono) */
static int escrever_atomico(const char *dest, const char *tmp, const void *dados, uint32_t n)
{
    const char *rd, *rt;
    montagem_t *m = montagem_de(dest, &rd);
    if (m) {
        if (m->opcoes & MONTAR_SO_LEITURA) return ERRO_SO_LEITURA;
        if (!montagem_de(tmp, &rt)) return ERRO_ACESSO;
        const char *rp;
        if (montagem_de("/home/.config", &rp) == m && *rp) m->fs->criar_pasta(m->priv, rp);
        arq_montado_t *a = kcalloc(sizeof(arq_montado_t) + m->fs->tam_arq);
        if (!a) return ERRO_SEM_MEMORIA;
        uint32_t tam = 0;
        int r = m->fs->abrir(m->priv, rt, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR, a->dados, &tam);
        if (r >= 0) {
            r = m->fs->escrever(m->priv, a->dados, 0, dados, n, &tam);
            if (r >= 0 && m->fs->sincronizar) m->fs->sincronizar(m->priv, a->dados);
            m->fs->fechar(m->priv, a->dados);
            if (r >= 0) r = m->fs->renomear(m->priv, rt, rd);
        }
        kfree(a);
        return r;
    }
    if (!vfs_achar("/home/.config")) vfs_criar("/home/.config", TIPO_PASTA, NULL);
    no_t *x = vfs_achar(tmp);
    if (x) vfs_truncar(x);
    else {
        int r = vfs_criar(tmp, TIPO_ARQUIVO, &x);
        if (r < 0) return r;
    }
    int r = vfs_escrever(x, 0, dados, n);
    if (r < 0) return r;
    return vfs_renomear(tmp, dest);
}

static void guardar_conf(void)
{
    static const char *const nomes_ord[] = { "manual", "nombre", "tipo", "fecha" };
    char t[640];
    int n = snformatar(t, sizeof t, "# Escritorio de SaguiOS (D268): orden, organización automática y casilla de cada icono\n"
                       "orden=%s\nauto=%s\n", nomes_ord[criterio], automatica ? "si" : "no");
    for (int i = 0; i < N_ICONES; i++)
        n += snformatar(t + n, sizeof t - (size_t)n, "%s=%d,%d\n", icones[i].chave, id_c[i], id_f[i]);
    int r = escrever_atomico(CONF_ESCRITORIO, CONF_NOVO, t, (uint32_t)n);
    klog("[ICONOS] guardado en %s (%d bytes): %d\n", CONF_ESCRITORIO, n, r);
}

static bool le_inteiro(const char **p, const char *fim, int *v)
{
    int n = 0;
    bool algum = false;
    while (*p < fim && **p >= '0' && **p <= '9') { n = n * 10 + (**p - '0'); (*p)++; algum = true; if (n > 100000) return false; }
    *v = n;
    return algum;
}

/* Lee el archivo. Las claves que ya no existen se ignoran; los iconos nuevos (o repetidos) van a la
 * primera casilla libre. */
static void carregar_conf(void)
{
    uint8_t *d;
    uint32_t tam;
    if (!vfs_existe_arquivo(CONF_ESCRITORIO) || vfs_ler_inteiro(CONF_ESCRITORIO, &d, &tam) < 0) return;
    bool tem[N_ICONES];
    for (int i = 0; i < N_ICONES; i++) tem[i] = false;
    casillas_padrao();
    int lidos = 0;
    const char *p = (const char *)d, *fim = p + tam;
    while (p < fim) {
        const char *l = p, *le = p;
        while (le < fim && *le != '\n') le++;
        p = le < fim ? le + 1 : le;
        if (le > l && le[-1] == '\r') le--;
        if (l >= le || *l == '#') continue;
        const char *eq = l;
        while (eq < le && *eq != '=') eq++;
        if (eq >= le) continue;
        size_t kl = (size_t)(eq - l);
        const char *v = eq + 1;
        if (kl == 5 && txt_compara_n(l, "orden", 5) == 0) {
            if (txt_comeca_com(v, "nombre")) criterio = ORD_NOMBRE;
            else if (txt_comeca_com(v, "tipo")) criterio = ORD_TIPO;
            else if (txt_comeca_com(v, "fecha")) criterio = ORD_FECHA;
            else criterio = ORD_MANUAL;
            continue;
        }
        if (kl == 4 && txt_compara_n(l, "auto", 4) == 0) { automatica = v < le && *v == 's'; continue; }
        for (int i = 0; i < N_ICONES; i++) {
            if (txt_tamanho(icones[i].chave) != kl || txt_compara_n(l, icones[i].chave, kl) != 0) continue;
            int c, f;
            const char *q = v;
            if (!le_inteiro(&q, le, &c) || q >= le || *q != ',') break;
            q++;
            if (!le_inteiro(&q, le, &f)) break;
            bool repetida = false;
            for (int j = 0; j < N_ICONES; j++)
                if (j != i && tem[j] && id_c[j] == c && id_f[j] == f) repetida = true;
            if (repetida || tem[i]) break;
            id_c[i] = c; id_f[i] = f;
            tem[i] = true;
            lidos++;
            break;
        }
    }
    kfree(d);
    /* Los que no estaban en el archivo (iconos nuevos): la primera casilla libre, por columnas. */
    for (int i = 0; i < N_ICONES; i++) {
        if (tem[i]) continue;
        bool achou = false;
        for (int c = 0; c < colunas() + 4 && !achou; c++)
            for (int f = 0; f < icones_por_coluna() && !achou; f++) {
                bool uso = false;
                for (int j = 0; j < N_ICONES; j++)
                    if (j != i && (tem[j] || j < i) && id_c[j] == c && id_f[j] == f) uso = true;
                if (!uso) { id_c[i] = c; id_f[i] = f; achou = true; }
            }
        tem[i] = true;
    }
    manual = true;
    klog("[ICONOS] cargado %s: %d casillas, orden %d, automatica %d\n", CONF_ESCRITORIO, lidos, criterio, automatica);
}

/* Interpolación de un canal en enteros. */
static uint32_t mistura(uint32_t a, uint32_t b, int num, int den)
{
    uint32_t r = 0;
    for (int s = 0; s <= 16; s += 8) {
        int ca = (int)((a >> s) & 255), cb = (int)((b >> s) & 255);
        r |= (uint32_t)(ca + (cb - ca) * num / den) << s;
    }
    return r;
}

static void texto_com_sombra(superficie_t *s, int x, int y, const char *t, uint32_t cor)
{
    sup_texto(s, x + 1, y + 1, t, 0x101010);
    sup_texto(s, x, y, t, cor);
}

/* Aclara los píxeles de un rectángulo (selección de iconos y rectángulo de selección). */
static void realcar(superficie_t *s, int x, int y, int l, int a, int pct)
{
    int x0 = x < s->cx0 ? s->cx0 : x, y0 = y < s->cy0 ? s->cy0 : y;
    int x1 = x + l > s->cx1 ? s->cx1 : x + l, y1 = y + a > s->cy1 ? s->cy1 : y + a;
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++)
            s->px[j * s->larg + i] = mistura(s->px[j * s->larg + i], 0xFFFFFF, pct, 100);
}

/* Redibuja en la superficie del fondo solo el rectángulo dado (D268: así mover un icono no
 * repinta todo el escritorio). Con la imagen elegida se copian solo esas líneas. */
static void desenhar_fundo_em(int rx, int ry, int rl, int ra)
{
    superficie_t s;
    sup_iniciar(&s, fundo, larg, alt);
    sup_recorte(&s, rx, ry, rl, ra);
    int x0 = s.cx0, y0 = s.cy0, x1 = s.cx1, y1 = s.cy1;
    if (x0 >= x1 || y0 >= y1) return;
    if (base) {   /* imagen elegida en Configuración: ya viene escalada a la pantalla */
        for (int y = y0; y < y1; y++) memcpy(fundo + y * larg + x0, base + y * larg + x0, (uint32_t)(x1 - x0) * 4);
        goto iconos;
    }
    /* Degradado vertical en los colores de Brasil: verde arriba, amarillo en el medio,
     * azul abajo, en tonos más suaves que los de la bandera para que el texto y los
     * íconos sigan siendo legibles. */
    const uint32_t verde = 0x0B7A3E, amarelo = 0xE0B92A, azul = 0x14337E;
    for (int y = y0; y < y1; y++) {
        uint32_t c = y < alt / 2 ? mistura(verde, amarelo, y, alt / 2)
                                 : mistura(amarelo, azul, y - alt / 2, alt - alt / 2);
        uint32_t *p = fundo + y * larg;
        for (int x = x0; x < x1; x++) p[x] = c;
    }
    /* La mascota, grande, en la esquina de abajo a la derecha. */
    int esc = larg >= 1000 ? 5 : 3;
    int mx = larg - 32 * esc - 30, my = alt - 32 * esc - 40;
    sup_desenho(&s, mx, my, &arte_sagui, esc);
    texto_com_sombra(&s, mx + 16 * esc - texto_largura(SAGUI_NOME) / 2, my + 32 * esc + 6, SAGUI_NOME, 0xFFFFFF);
iconos:
    /* Íconos con el nombre abajo, cada uno en su casilla. */
    for (int i = 0; i < QTD_ICONES; i++) {
        if (!icone_visivel(i)) continue;
        int x, y, l, a;
        area_icone_retangulo(i, &x, &y, &l, &a);
        if (x + l <= x0 || x >= x1 || y + a <= y0 || y >= y1) continue;
        if (sel[i]) realcar(&s, x + 2, y - 2, l - 4, ICONE_A - 2, 28);
        sup_desenho(&s, x + (l - 64) / 2, y, icones[i].desenho, 2);
        const char *nome = nome_do_icone(i);
        texto_com_sombra(&s, x + (l - texto_largura(nome)) / 2, y + 68, nome, 0xFFFFFF);
        if (foco_esc && foco_ic == i) sup_moldura(&s, x + 2, y - 2, l - 4, ICONE_A - 2, 0xFFFFFF);
    }
    if (banda_vis) {   /* rectángulo de selección */
        int bx = banda_x0 < banda_x1 ? banda_x0 : banda_x1, by = banda_y0 < banda_y1 ? banda_y0 : banda_y1;
        int bl = (banda_x0 < banda_x1 ? banda_x1 - banda_x0 : banda_x0 - banda_x1) + 1;
        int ba = (banda_y0 < banda_y1 ? banda_y1 - banda_y0 : banda_y0 - banda_y1) + 1;
        realcar(&s, bx, by, bl, ba, 18);
        sup_moldura(&s, bx, by, bl, ba, 0xFFFFFF);
    }
}

static void desenhar_fundo(void) { desenhar_fundo_em(0, 0, larg, alt); }

/* Rectángulo sucio acumulado de un cambio de iconos: se repinta el fondo ahí y se avisa al gestor. */
static int suja_x0, suja_y0, suja_x1, suja_y1;
static void suja_limpar(void) { suja_x0 = suja_y0 = 100000; suja_x1 = suja_y1 = -100000; }
static void suja_rect(int x, int y, int l, int a)
{
    if (x < suja_x0) suja_x0 = x;
    if (y < suja_y0) suja_y0 = y;
    if (x + l > suja_x1) suja_x1 = x + l;
    if (y + a > suja_y1) suja_y1 = y + a;
}
static void suja_icone(int i)
{
    int x, y, l, a;
    area_icone_retangulo(i, &x, &y, &l, &a);
    suja_rect(x - 2, y - 4, l + 4, a + 6);
}
static void suja_todos(void) { for (int i = 0; i < N_ICONES; i++) if (icone_visivel(i)) suja_icone(i); }
static void suja_aplicar(void)
{
    if (suja_x0 >= suja_x1) return;
    desenhar_fundo_em(suja_x0, suja_y0, suja_x1 - suja_x0, suja_y1 - suja_y0);
    gui_redesenhar(suja_x0, suja_y0, suja_x1 - suja_x0, suja_y1 - suja_y0);
    suja_limpar();
}

bool area_iniciar(void)
{
    larg = (int)video_largura();
    alt = (int)video_altura();
    fundo = kmalloc((uint32_t)(larg * alt) * 4);
    if (!fundo) return false;
    suja_limpar();
    carregar_conf();
    ajustar_casillas();
    registrar_casillas("casillas al arrancar");
    desenhar_fundo();
    return true;
}

/* El modo de video cambió (D059): fondo nuevo del tamaño de la pantalla. */
bool area_tela_mudou(void)
{
    int l = (int)video_largura(), a = (int)video_altura();
    uint32_t *novo = kmalloc((uint32_t)(l * a) * 4);
    if (!novo) return false;
    kfree(fundo);
    fundo = novo;
    if (base) {   /* se reescala (vecino más próximo); el próximo arranque la ajusta bien */
        uint32_t *nb = kmalloc((uint32_t)(l * a) * 4);
        if (nb) {
            for (int y = 0; y < a; y++) {
                const uint32_t *o = base + (y * alt / a) * larg;
                for (int x = 0; x < l; x++) nb[y * l + x] = o[x * larg / l];
            }
        }
        kfree(base);
        base = nb;
    }
    larg = l; alt = a;
    arr_estado = 0; banda_vis = false;
    ajustar_casillas();
    registrar_casillas("casillas tras cambiar de resolucion");
    desenhar_fundo();
    return true;
}

/* Fondo personalizado (D112, SIS_FUNDO): la imagen llega ya escalada, por tandas de líneas. */
bool area_fundo_personalizado(void) { return base != NULL; }

void area_fundo_padrao(void)
{
    if (!base) return;
    kfree(base);
    base = NULL;
    desenhar_fundo();
}

int area_fundo_linhas(const uint32_t *px, int y, int n)
{
    if (y < 0 || n <= 0 || y + n > alt) return ERRO_INVALIDO;
    if (!base) {
        base = kmalloc((uint32_t)(larg * alt) * 4);
        if (!base) return ERRO_SEM_MEMORIA;
        memset(base, 0, (uint32_t)(larg * alt) * 4);
    }
    memcpy(base + y * larg, px, (uint32_t)(n * larg) * 4);
    return 0;
}

void area_fundo_fim(void)
{
    desenhar_fundo();
    gui_redesenhar(0, 0, larg, alt);
    gui_mostrar();
}

void area_fundo(superficie_t *s)
{
    sup_copiar(s, 0, 0, fundo, larg, alt, larg);
}

int area_icone_em(int x, int y)
{
    for (int i = 0; i < QTD_ICONES; i++) {
        if (!icone_visivel(i)) continue;
        int ix, iy, l, a;
        area_icone_retangulo(i, &ix, &iy, &l, &a);
        if (x >= ix && x < ix + l && y >= iy && y < iy + a) return i;
    }
    return -1;
}

/* ---- Ratón y teclado sobre los iconos (D268) ---- */
static void mudar_foco(int i)
{
    if (foco_ic >= 0 && icone_visivel(foco_ic)) suja_icone(foco_ic);
    foco_ic = i;
    if (i >= 0) suja_icone(i);
}

static void desmarcar_todos(void)
{
    for (int i = 0; i < N_ICONES; i++)
        if (sel[i]) { sel[i] = false; if (icone_visivel(i)) suja_icone(i); }
}

static void selecionar_so(int i)
{
    desmarcar_todos();
    sel[i] = true;
    suja_icone(i);
}

/* Botón izquierdo pulsado en el escritorio: ic = icono bajo el ratón o -1. */
void area_icone_clique(int ic, int x, int y, bool ctrl)
{
    suja_limpar();
    foco_esc = true;
    arr_ctrl = ctrl;
    arr_sozinho = false;
    if (ic >= 0) {
        mudar_foco(ic);
        if (ctrl) {
            sel[ic] = !sel[ic];
            suja_icone(ic);
        } else if (!sel[ic]) {
            selecionar_so(ic);
        } else {
            int n = 0;
            for (int i = 0; i < N_ICONES; i++) if (sel[i]) n++;
            arr_sozinho = n > 1;
        }
        arr_estado = sel[ic] ? 1 : 0;
        arr_ic = ic;
    } else {
        mudar_foco(-1);
        if (!ctrl) desmarcar_todos();
        for (int i = 0; i < N_ICONES; i++) sel_base[i] = sel[i];
        arr_estado = 3;
        banda_vis = false;
        banda_x0 = banda_x1 = x; banda_y0 = banda_y1 = y;
    }
    arr_x0 = x; arr_y0 = y;
    arr_dc = arr_df = 0;
    suja_aplicar();
}

static int celulas_de(int v, int passo)
{
    return v >= 0 ? (v + passo / 2) / passo : -((-v + passo / 2) / passo);
}

/* Suelta los iconos seleccionados en las casillas del arrastre. Uno solo que cae encima de otro se
 * intercambia con él; un grupo desplaza a los ocupantes a la casilla libre más cercana. */
static void soltar_icones(void)
{
    int dc = arr_dc, df = arr_df;
    suja_limpar();
    for (int i = 0; i < N_ICONES; i++) if (sel[i] && icone_visivel(i)) suja_icone(i);   /* donde se ven ahora */
    arr_estado = 0;
    arr_dc = arr_df = 0;
    if (dc == 0 && df == 0) { suja_aplicar(); return; }
    int nsel = 0, unico = -1, oc[N_ICONES], of[N_ICONES];
    for (int i = 0; i < N_ICONES; i++) {
        oc[i] = ic_c[i]; of[i] = ic_f[i];
        if (sel[i] && icone_visivel(i)) { nsel++; unico = i; }
    }
    for (int i = 0; i < N_ICONES; i++)
        if (sel[i] && icone_visivel(i)) { ic_c[i] += dc; ic_f[i] += df; }
    bool movido[N_ICONES];
    int pendentes[N_ICONES], np = 0;
    for (int j = 0; j < N_ICONES; j++) {
        movido[j] = sel[j] && icone_visivel(j);
        if (movido[j] || !icone_visivel(j)) continue;
        bool choca = false;
        for (int i = 0; i < N_ICONES; i++)
            if (sel[i] && icone_visivel(i) && ic_c[i] == ic_c[j] && ic_f[i] == ic_f[j]) choca = true;
        if (!choca) continue;
        if (nsel == 1) { ic_c[j] = oc[unico]; ic_f[j] = of[unico]; movido[j] = true; }
        else pendentes[np++] = j;
    }
    for (int k = 0; k < np; k++) {
        int j = pendentes[k], c, f;
        if (livre_perto(ic_c[j], ic_f[j], j, &c, &f)) {
            /* livre_perto ve las casillas de los demás ya colocados; la de j sigue ocupada por un seleccionado */
            ic_c[j] = c; ic_f[j] = f;
        }
        movido[j] = true;
    }
    manual = true;
    criterio = ORD_MANUAL;
    for (int i = 0; i < N_ICONES; i++) if (movido[i] && icone_visivel(i)) suja_icone(i);
    if (automatica) { ordenar(ORD_NOMBRE); suja_todos(); }
    fixar_casillas();
    if (nsel == 1) klog("[ICONOS] arrastre: %s -> %d,%d\n", icones[unico].chave, ic_c[unico], ic_f[unico]);
    else klog("[ICONOS] arrastre de %d iconos (%+d,%+d casillas)\n", nsel, dc, df);
    registrar_casillas("casillas");
    guardar_conf();
    suja_aplicar();
}

/* Movimiento y soltado del botón con un arrastre en marcha. Devuelve true si el evento ya es suyo. */
bool area_icone_arrasto(int x, int y, uint8_t botoes)
{
    if (!arr_estado) return false;
    bool largou = !(botoes & BOTAO_ESQUERDO);
    int dx = x - arr_x0, dy = y - arr_y0;
    bool longe = dx >= 4 || dx <= -4 || dy >= 4 || dy <= -4;
    suja_limpar();
    if (arr_estado == 1) {
        if (largou) {
            if (arr_sozinho && !arr_ctrl) { selecionar_so(arr_ic); suja_aplicar(); }
            arr_estado = 0;
            return true;
        }
        if (longe) arr_estado = 2;
    }
    if (arr_estado == 2) {
        if (largou) { soltar_icones(); return true; }
        int minc = 1000, maxc = -1, minf = 1000, maxf = -1;
        for (int i = 0; i < N_ICONES; i++) {
            if (!sel[i] || !icone_visivel(i)) continue;
            if (ic_c[i] < minc) minc = ic_c[i];
            if (ic_c[i] > maxc) maxc = ic_c[i];
            if (ic_f[i] < minf) minf = ic_f[i];
            if (ic_f[i] > maxf) maxf = ic_f[i];
        }
        int dc = celulas_de(dx, CEL_L), df = celulas_de(dy, ICONE_PASSO);
        if (dc < -minc) dc = -minc;
        if (dc > colunas() - 1 - maxc) dc = colunas() - 1 - maxc;
        if (df < -minf) df = -minf;
        if (df > icones_por_coluna() - 1 - maxf) df = icones_por_coluna() - 1 - maxf;
        if (dc != arr_dc || df != arr_df) {
            for (int i = 0; i < N_ICONES; i++) if (sel[i] && icone_visivel(i)) suja_icone(i);
            arr_dc = dc; arr_df = df;
            for (int i = 0; i < N_ICONES; i++) if (sel[i] && icone_visivel(i)) suja_icone(i);
            suja_aplicar();
        }
        return true;
    }
    /* arr_estado == 3: rectángulo de selección */
    if (banda_vis) {
        int bx = banda_x0 < banda_x1 ? banda_x0 : banda_x1, by = banda_y0 < banda_y1 ? banda_y0 : banda_y1;
        suja_rect(bx - 1, by - 1, (banda_x0 < banda_x1 ? banda_x1 - banda_x0 : banda_x0 - banda_x1) + 3,
                  (banda_y0 < banda_y1 ? banda_y1 - banda_y0 : banda_y0 - banda_y1) + 3);
    }
    if (largou) {
        banda_vis = false;
        suja_aplicar();
        int n = 0;
        for (int i = 0; i < N_ICONES; i++) if (sel[i] && icone_visivel(i)) n++;
        klog("[ICONOS] seleccion por rectangulo: %d iconos\n", n);
        arr_estado = 0;
        return true;
    }
    if (!banda_vis && longe) banda_vis = true;
    if (banda_vis) {
        banda_x1 = x < 0 ? 0 : (x >= larg ? larg - 1 : x);
        banda_y1 = y < 0 ? 0 : (y >= alt ? alt - 1 : y);
        int bx = banda_x0 < banda_x1 ? banda_x0 : banda_x1, by = banda_y0 < banda_y1 ? banda_y0 : banda_y1;
        int bl = (banda_x0 < banda_x1 ? banda_x1 - banda_x0 : banda_x0 - banda_x1) + 1;
        int ba = (banda_y0 < banda_y1 ? banda_y1 - banda_y0 : banda_y0 - banda_y1) + 1;
        suja_rect(bx - 1, by - 1, bl + 2, ba + 2);
        for (int i = 0; i < N_ICONES; i++) {
            if (!icone_visivel(i)) continue;
            int ix, iy, il, ia;
            area_icone_retangulo(i, &ix, &iy, &il, &ia);
            bool dentro = ix < bx + bl && ix + il > bx && iy < by + ba && iy + ia > by;
            bool nuevo = arr_ctrl ? (sel_base[i] != dentro) : dentro;   /* con Ctrl el rectángulo invierte */
            if (nuevo != sel[i]) { sel[i] = nuevo; suja_icone(i); }
        }
        suja_aplicar();
    }
    return true;
}

/* El teclado deja de ser del escritorio (otra ventana tomó el foco). */
void area_foco_perder(void)
{
    if (!foco_esc) return;
    foco_esc = false;
    suja_limpar();
    if (foco_ic >= 0 && icone_visivel(foco_ic)) suja_icone(foco_ic);
    suja_aplicar();
}

static void navegar(int dir)
{
    suja_limpar();
    int cur = foco_ic >= 0 && icone_visivel(foco_ic) ? foco_ic : -1, melhor = -1, custo = 0;
    if (cur < 0) {   /* sin foco: el primero por columnas */
        for (int i = 0; i < N_ICONES; i++) {
            if (!icone_visivel(i)) continue;
            if (melhor < 0 || ic_c[i] * 1000 + ic_f[i] < ic_c[melhor] * 1000 + ic_f[melhor]) melhor = i;
        }
    } else {
        for (int j = 0; j < N_ICONES; j++) {
            if (j == cur || !icone_visivel(j)) continue;
            int dc = ic_c[j] - ic_c[cur], df = ic_f[j] - ic_f[cur], prim, perp;
            if (dir == TECLA_DIREITA) { prim = dc; perp = df; }
            else if (dir == TECLA_ESQUERDA) { prim = -dc; perp = df; }
            else if (dir == TECLA_BAIXO) { prim = df; perp = dc; }
            else { prim = -df; perp = dc; }
            if (prim <= 0) continue;
            int c = prim + 3 * (perp < 0 ? -perp : perp);
            if (melhor < 0 || c < custo) { melhor = j; custo = c; }
        }
    }
    if (melhor >= 0) {
        selecionar_so(melhor);
        mudar_foco(melhor);
        klog("[ICONOS] foco: %s\n", icones[melhor].chave);
    }
    suja_aplicar();
}

/* Tecla con el foco en el escritorio: flechas para moverse, Intro para abrir, Esc para soltar el foco. */
bool area_icone_tecla(int t, uint32_t mods)
{
    if (!foco_esc || (mods & 7)) return false;
    if (t == TECLA_CIMA || t == TECLA_BAIXO || t == TECLA_ESQUERDA || t == TECLA_DIREITA) { navegar(t); return true; }
    if ((t == '\n' || t == '\r') && foco_ic >= 0 && icone_visivel(foco_ic)) { area_icone_abrir(foco_ic); return true; }
    if (t == TECLA_ESC) {
        suja_limpar();
        desmarcar_todos();
        mudar_foco(-1);
        foco_esc = false;
        suja_aplicar();
        return true;
    }
    return false;
}

/* Muestra u oculta el ícono del pendrive (H34). Devuelve true si cambió. */
bool area_pendrive(bool presente, const char *rotulo)
{
    char nome[24];
    txt_copia_n(nome, rotulo && rotulo[0] ? rotulo : "Pendrive", sizeof nome);
    if (presente == pendrive_visivel && txt_compara(nome, nome_pendrive) == 0) return false;
    bool aparece = presente && !pendrive_visivel;
    pendrive_visivel = presente;
    txt_copia_n(nome_pendrive, nome, sizeof nome_pendrive);
    if (aparece && manual) {   /* su casilla pedida si sigue libre; si no, la libre más próxima */
        if (automatica && criterio != ORD_MANUAL) { int c = criterio; ordenar(c); }
        else if (!cel_valida(ic_c[ICONE_PENDRIVE], ic_f[ICONE_PENDRIVE]) ||
                 ocupante(ic_c[ICONE_PENDRIVE], ic_f[ICONE_PENDRIVE], ICONE_PENDRIVE) >= 0) {
            int c, f;
            if (livre_perto(ic_c[ICONE_PENDRIVE], ic_f[ICONE_PENDRIVE], ICONE_PENDRIVE, &c, &f)) {
                ic_c[ICONE_PENDRIVE] = id_c[ICONE_PENDRIVE] = c;
                ic_f[ICONE_PENDRIVE] = id_f[ICONE_PENDRIVE] = f;
            }
        }
    }
    if (!presente && foco_ic == ICONE_PENDRIVE) { foco_ic = -1; foco_esc = false; }
    sel[ICONE_PENDRIVE] = sel[ICONE_PENDRIVE] && presente;
    desenhar_fundo();
    return true;
}

static disco_t *primeiro_disco(void)
{
    for (int i = 0; i < MAX_DISCOS; i++) {
        disco_t *d = disco_indice(i);
        if (d && d->presente && !d->pai && d->removivel && !d->de_boot) return d;
    }
    return NULL;
}

/* "Quitar con seguridad" del pendrive: graba lo que falta y desmonta. */
static void remover_pendrive(void)
{
    disco_t *d = disco_pendrive_montado();
    if (d) disco_desmontar(d);
    else gui_aviso("El pendrive no está montado: puedes retirarlo.");
}

/* Respuesta del menú de un ícono (D051): "dado" es el ícono. */
static void resposta_icone(int indice, int janela, int i)
{
    (void)janela;
    if (indice == 0) area_icone_abrir(i);
    else if (indice == 2 && i == ICONE_PENDRIVE) remover_pendrive();
}

/* Botón derecho en un ícono: menú con "Abrir" y, en el pendrive, "Quitar con
 * seguridad" (antes el botón derecho en el pendrive ya desmontaba directo, sin
 * preguntar nada; ahora la persona ve lo que va a pasar). */
void area_icone_direito(int i, int x, int y)
{
    if (i < 0 || i >= QTD_ICONES) return;
    if (!sel[i]) { suja_limpar(); selecionar_so(i); suja_aplicar(); }
    const char *itens = i == ICONE_PENDRIVE ? "Abrir\n-\nQuitar con seguridad" : "Abrir";
    menu_abrir(x, y, itens, resposta_icone, 0, i);
}

/* Menú del escritorio vacío (D051): los programas que ya tienen ícono,
 * el Administrador de tareas, la configuración de la pantalla con el submenú de las
 * resoluciones (D066) y el "Acerca de". Cada línea lleva una acción; las de los
 * íconos son el propio índice del ícono. */
enum { AF_TAREFAS = 100, AF_PANTALLA, AF_RESOLUCAO = 200, AF_ORD_NOME = 300, AF_ORD_TIPO, AF_ORD_FECHA, AF_ALINHAR, AF_AUTO };
#define RES_MAX 24

static menu_lista_t menu_fundo;
static modo_video_t res_modos[RES_MAX];

/* "Configuración de pantalla": la ventana del programa pantalla, una sola. */
void area_abrir_pantalla(void)
{
    char *argv[] = { "pantalla", "--ventana", NULL };
    gui_abrir_unico("pantalla", "/bin/pantalla", 2, argv);
}

/* Submenú "Resolución": los modos de 32 bits que cambian en caliente, con el
 * actual marcado (D066). Sin cambio en caliente (la placa real, sólo VBE), no hay
 * submenú: la ventana "Pantalla" lo explica y graba para el próximo arranque. Si
 * la lista no cabe en la altura de la pantalla, quedan las menores, y la actual siempre. */
void area_menu_resolucao(menu_lista_t *ml)
{
    static modo_video_t todos[192];
    int n = modos_lista(todos, 192), k = 0, cabe = ((int)video_altura() - 8) / 22 - 1;
    bool tem_atual = false;
    if (cabe > RES_MAX) cabe = RES_MAX;
    for (int i = 0; i < n; i++) {
        if (todos[i].bpp != video_bpp_a_quente() || !(todos[i].flags & (MODO_A_QUENTE | MODO_ATUAL))) continue;
        bool e_atual = (todos[i].flags & MODO_ATUAL) != 0;
        if (k < cabe) { res_modos[k++] = todos[i]; tem_atual |= e_atual; }
        else if (e_atual && !tem_atual) { res_modos[cabe - 1] = todos[i]; tem_atual = true; }
    }
    if (k < 2) return;   /* sólo el modo actual: no hay qué elegir */
    char t[MENU_MAX_TEXTO + 1];
    snformatar(t, sizeof t, ">Resolución\t%ux%u", video_largura(), video_altura());
    menu_lista_linha(ml, t, MENU_NADA, NULL);
    for (int i = 0; i < k; i++) {
        modo_video_t *mv = &res_modos[i];
        bool a_quente = (mv->flags & MODO_A_QUENTE) != 0;
        snformatar(t, sizeof t, "%s%s%ux%u", (mv->flags & MODO_ATUAL) ? "*" : "", a_quente ? "" : "~", mv->largura,
                   mv->altura);
        menu_lista_linha(ml, t, AF_RESOLUCAO + i, NULL);
    }
    menu_lista_linha(ml, "<", MENU_NADA, NULL);
}

/* Cambia la resolución elegida en el submenú, por el mismo camino del
 * SIS_MUDAR_MODO (MUDAR_AGORA, sin grabar). */
bool area_resposta_resolucao(int acao)
{
    if (acao < AF_RESOLUCAO || acao >= AF_RESOLUCAO + RES_MAX) return false;
    modo_video_t *mv = &res_modos[acao - AF_RESOLUCAO];
    if (mv->flags & MODO_ATUAL) return true;
    int r = sis_mudar_modo((uint32_t)mv->largura | (uint32_t)mv->altura << 16, MUDAR_AGORA, 0, 0, 0);
    klog("[GUI] resolucao pelo menu: %ux%u -> %d\n", mv->largura, mv->altura, r);
    if (r < 0) gui_aviso("No se pudo cambiar la resolución.");
    return true;
}

/* «Ordenar iconos» (D268): ordena, alinea o activa la organización automática, y lo guarda. */
static void ordenar_por_menu(int acao)
{
    suja_limpar();
    suja_todos();
    const char *que = "";
    if (acao == AF_ORD_NOME) { ordenar(ORD_NOMBRE); que = "por nombre"; }
    else if (acao == AF_ORD_TIPO) { ordenar(ORD_TIPO); que = "por tipo"; }
    else if (acao == AF_ORD_FECHA) { ordenar(ORD_FECHA); que = "por fecha"; }
    else if (acao == AF_ALINHAR) { compactar(false); manual = true; fixar_casillas(); que = "alineado a la rejilla"; }
    else {
        automatica = !automatica;
        if (automatica) ordenar(criterio != ORD_MANUAL ? criterio : ORD_NOMBRE);
        que = automatica ? "organizacion automatica: si" : "organizacion automatica: no";
    }
    suja_todos();
    klog("[ICONOS] ordenar: %s\n", que);
    registrar_casillas("casillas");
    guardar_conf();
    suja_aplicar();
}

static void resposta_fundo(int indice, int janela, int dado)
{
    (void)janela; (void)dado;
    int acao = menu_lista_acao(&menu_fundo, indice);
    if (acao == AF_TAREFAS) gui_abrir_tarefas();
    else if (acao == AF_PANTALLA) area_abrir_pantalla();
    else if (area_resposta_resolucao(acao)) {}
    else if (acao >= AF_ORD_NOME && acao <= AF_AUTO) ordenar_por_menu(acao);
    else if (acao >= 0 && acao < QTD_ICONES) area_icone_abrir(acao);
}

void area_menu_fundo(int x, int y)
{
    menu_lista_t *ml = &menu_fundo;
    menu_lista_iniciar(ml);
    menu_lista_linha(ml, "Terminal", 1, NULL);
    menu_lista_linha(ml, "Archivos", 0, NULL);
    menu_lista_linha(ml, "Editor", 2, NULL);
    menu_lista_linha(ml, "Calculadora", 3, NULL);
    menu_lista_linha(ml, "-", MENU_NADA, NULL);
    menu_lista_linha(ml, "Administrador de tareas", AF_TAREFAS, NULL);
    menu_lista_linha(ml, "Configuración de pantalla...", AF_PANTALLA, NULL);
    area_menu_resolucao(ml);
    menu_lista_linha(ml, ">Ordenar iconos", MENU_NADA, NULL);   /* D268 */
    menu_lista_linha(ml, criterio == ORD_NOMBRE ? "*Por nombre" : "Por nombre", AF_ORD_NOME, NULL);
    menu_lista_linha(ml, criterio == ORD_TIPO ? "*Por tipo" : "Por tipo", AF_ORD_TIPO, NULL);
    menu_lista_linha(ml, criterio == ORD_FECHA ? "*Por fecha" : "Por fecha", AF_ORD_FECHA, NULL);
    menu_lista_linha(ml, "Alinear a la rejilla", AF_ALINHAR, NULL);
    menu_lista_linha(ml, automatica ? "*Organización automática" : "Organización automática", AF_AUTO, NULL);
    menu_lista_linha(ml, "<", MENU_NADA, NULL);
    menu_lista_linha(ml, "-", MENU_NADA, NULL);
    menu_lista_linha(ml, "Acerca de " SAGUI_NOME, 6, NULL);
    menu_abrir(x, y, ml->texto, resposta_fundo, 0, 0);
}

/* Abre el programa del ícono (H28). No tiene padre: nadie espera por él. */
void area_icone_abrir(int i)
{
    if (i < 0 || i >= QTD_ICONES) return;
    if (i == ICONE_PENDRIVE) {
        disco_t *d = primeiro_disco();
        if (!d) return;
        int r = disco_montar(d);
        if (r == ERRO_FORMATO) { gui_aviso("Este pendrive no está en FAT32 ni ext2/ext4: formato no soportado."); return; }
        if (r < 0 && r != ERRO_OCUPADO) { gui_aviso("No se pudo montar el pendrive."); return; }
        /* Abre donde el volumen esté montado (normalmente /usb, D040). */
        static char ponto[CAMINHO_MAX];
        txt_copia_n(ponto, "/usb", sizeof ponto);
        disco_t *v = d->volume;
        if (v && v->montagem) txt_copia_n(ponto, v->montagem->ponto, sizeof ponto);
        char *argv[] = { "arquivos", ponto, NULL };
        int pid = processo_executar("/bin/arquivos", 2, argv, NULL);
        klog("[GUI] icone Pendrive -> /bin/arquivos %s (pid %d)\n", ponto, pid);
        return;
    }
    char *argv[] = { (char *)icones[i].programa + 5, NULL };
    int pid = processo_executar(icones[i].programa, 1, argv, NULL);
    klog("[GUI] icone %s -> %s (pid %d)\n", icones[i].nome, icones[i].programa, pid);
}
