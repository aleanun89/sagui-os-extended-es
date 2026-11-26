// SPDX-License-Identifier: GPL-3.0-only
/* telas.c - varias pantallas: registro de salidas, disposición del escritorio y configuración (D290).
 *
 * Ver telas.h. Aquí no se dibuja nada: este módulo sabe cuántas cabezas hay, de qué tamaño y dónde
 * caen en el escritorio (EXTENDER) o que todas enseñan lo mismo (ESPEJO). video.c copia el búfer de
 * atrás a cada cabeza con esos rectángulos; el gestor de ventanas pregunta aquí qué pantalla
 * contiene una ventana para maximizarla en ella.
 *
 * La configuración (modo, lado, principal y la resolución de cada cabeza) se guarda en
 * /home/.config/pantallas.conf, como la del escritorio (D268), y se aplica al arrancar el
 * escritorio si hay más de una salida. Con una sola cabeza este módulo no hace nada. */
#include "telas.h"
#include "abi.h"
#include "bga.h"
#include "console.h"
#include "cpu.h"
#include "gui.h"
#include "memoria.h"
#include "modos_video.h"
#include "mouse.h"
#include "sistema.h"
#include "texto.h"
#include "vfs.h"
#include "video.h"
#include "acel2d.h"
#include "conf_sistema.h"

#define CONF_PANTALLAS "/home/.config/pantallas.conf"
#define CONF_NOVO      "/home/.config/pantallas.conf.nuevo"

static saida_t sai[TELAS_MAX];   /* sai[0]: la cabeza del arranque (solo largura y altura son suyas) */
static int ncab = 1;
static int modo = TELAS_MODO_EXTENDER, lado = TELAS_LADO_DIREITA, principal;
static int pos_x[TELAS_MAX], pos_y[TELAS_MAX];
static uint32_t res_guardada[TELAS_MAX][2];   /* la resolución del archivo (0 = la que tenga) */

int telas_qtd(void) { return ncab; }
int telas_modo(void) { return modo; }
int telas_lado(void) { return lado; }
int telas_principal(void) { return principal; }
const saida_t *telas_saida(int cab) { return cab >= 1 && cab < ncab ? &sai[cab] : NULL; }

void telas_cabeca0(uint32_t l, uint32_t a)
{
    sai[0].largura = l; sai[0].altura = a;
    if (!sai[0].nome[0]) snformatar(sai[0].nome, sizeof sai[0].nome, "Principal (arranque)");
}

int telas_registrar_saida(const saida_t *s)
{
    if (ncab >= TELAS_MAX || !s->fb || !s->largura || !s->altura) return -1;
    sai[ncab] = *s;
    return ncab++;
}

/* ---- disposición */

static uint32_t larg_de(int i, int cab, uint32_t l) { return i == cab ? l : sai[i].largura; }
static uint32_t alt_de(int i, int cab, uint32_t a) { return i == cab ? a : sai[i].altura; }

/* Posiciones y tamaño del escritorio con la cabeza `cab` en l x a (cab < 0: como están). */
static void calcular(int cab, uint32_t l, uint32_t a, int *px, int *py, uint32_t *dl, uint32_t *da)
{
    int p = principal < ncab ? principal : 0;
    uint32_t rl = 0, ra = 0;
    if (modo == TELAS_MODO_ESPELHO) {
        for (int i = 0; i < ncab; i++) px[i] = py[i] = 0;
        *dl = larg_de(p, cab, l); *da = alt_de(p, cab, a);
        return;
    }
    px[p] = py[p] = 0;
    uint32_t acum = lado == TELAS_LADO_ABAIXO ? alt_de(p, cab, a) : larg_de(p, cab, l);
    for (int i = 0; i < ncab; i++) {
        if (i == p) continue;
        if (lado == TELAS_LADO_ABAIXO) { px[i] = 0; py[i] = (int)acum; acum += alt_de(i, cab, a); }
        else { px[i] = (int)acum; py[i] = 0; acum += larg_de(i, cab, l); }
    }
    for (int i = 0; i < ncab; i++) {
        uint32_t fx = (uint32_t)px[i] + larg_de(i, cab, l), fy = (uint32_t)py[i] + alt_de(i, cab, a);
        if (fx > rl) rl = fx;
        if (fy > ra) ra = fy;
    }
    *dl = rl; *da = ra;
}

void telas_desktop(int cab, uint32_t l, uint32_t a, uint32_t *dl, uint32_t *da)
{
    int x[TELAS_MAX], y[TELAS_MAX];
    calcular(cab, l, a, x, y, dl, da);
}

bool telas_rect(int cab, int *x, int *y, int *l, int *a)
{
    if (cab < 0 || cab >= ncab) return false;
    uint32_t dl, da;
    int px[TELAS_MAX], py[TELAS_MAX];
    calcular(-1, 0, 0, px, py, &dl, &da);
    if (x) *x = px[cab];
    if (y) *y = py[cab];
    if (l) *l = (int)sai[cab].largura;
    if (a) *a = (int)sai[cab].altura;
    return true;
}

void telas_principal_rect(int *l, int *a)
{
    int p = principal < ncab ? principal : 0;
    if (ncab <= 1) { *l = (int)video_largura(); *a = (int)video_altura(); return; }
    *l = (int)sai[p].largura; *a = (int)sai[p].altura;
}

int telas_da_posicao(int x, int y)
{
    if (ncab <= 1) return 0;
    int melhor = 0;
    long dmin = -1;
    for (int i = 0; i < ncab; i++) {
        int rx, ry, rl, ra;
        telas_rect(i, &rx, &ry, &rl, &ra);
        int cx = x < rx ? rx : (x >= rx + rl ? rx + rl - 1 : x);
        int cy = y < ry ? ry : (y >= ry + ra ? ry + ra - 1 : y);
        long d = (long)(x - cx) * (x - cx) + (long)(y - cy) * (y - cy);
        if (dmin < 0 || d < dmin) { dmin = d; melhor = i; }
    }
    return melhor;
}

void telas_prender(int *x, int *y)
{
    if (ncab <= 1) return;
    int i = telas_da_posicao(*x, *y), rx, ry, rl, ra;
    telas_rect(i, &rx, &ry, &rl, &ra);
    if (*x < rx) *x = rx;
    if (*x >= rx + rl) *x = rx + rl - 1;
    if (*y < ry) *y = ry;
    if (*y >= ry + ra) *y = ry + ra - 1;
}

/* ---- aplicar */

/* Rehace el escritorio con la disposición actual y avisa a quien depende del tamaño (mismo orden
 * que el cambio de modo en caliente, modos_video.c). Con las interrupciones desactivadas: la IRQ
 * del ratón también dibuja. */
static int aplicar(void)
{
    bool i = int_salva_desliga();
    if (!video_refazer()) { int_restaura(i); return ERRO_SEM_MEMORIA; }
    console_tela_mudou();
    mouse_tela_mudou((int)video_largura(), (int)video_altura());
    if (gui_ativa()) {
        area_tela_mudou();
        gui_tela_mudou();
    } else {
        video_mostrar_tudo();
    }
    int_restaura(i);
    klog("[TELAS] %s, %d pantallas, principal %d, escritorio %ux%u\n", modo == TELAS_MODO_ESPELHO ? "espejo" : "extendido", ncab,
         principal, video_largura(), video_altura());
    return 0;
}

/* En espejo, las demás cabezas intentan el modo de la principal; si no, se quedan con el suyo (se recortan). */
static void igualar_espelho(void)
{
    int p = principal < ncab ? principal : 0;
    for (int i = 0; i < ncab; i++) {
        if (i == p || i == 0) continue;
        if (sai[i].largura != sai[p].largura || sai[i].altura != sai[p].altura)
            if (sai[i].mudar_modo) sai[i].mudar_modo(&sai[i], sai[p].largura, sai[p].altura);
    }
}

int telas_configurar(int m, int l, int p)
{
    if (m != TELAS_MODO_EXTENDER && m != TELAS_MODO_ESPELHO) return ERRO_INVALIDO;
    if (l != TELAS_LADO_DIREITA && l != TELAS_LADO_ABAIXO) return ERRO_INVALIDO;
    if (p < 0 || p >= ncab) return ERRO_INVALIDO;
    if (ncab <= 1) return ERRO_NAO_EXISTE;
    int m0 = modo, l0 = lado, p0 = principal;
    modo = m; lado = l; principal = p;
    if (modo == TELAS_MODO_ESPELHO) igualar_espelho();
    int r = aplicar();
    if (r < 0) { modo = m0; lado = l0; principal = p0; }
    return r;
}

int telas_resolucao(int cab, uint32_t l, uint32_t a)
{
    if (cab < 0 || cab >= ncab) return ERRO_INVALIDO;
    if (cab == 0) return sis_mudar_modo(l | a << 16, MUDAR_AGORA, 0, 0, 0) < 0 ? ERRO_INVALIDO : 0;
    saida_t *s = &sai[cab];
    if (!s->mudar_modo) return ERRO_NAO_SUPORTADO;
    if (l == s->largura && a == s->altura) return 0;
    if (!s->mudar_modo(s, l, a)) return ERRO_INVALIDO;
    return aplicar();
}

/* ---- configuración guardada */

int telas_guardar(void)
{
    char t[320];
    int n = snformatar(t, sizeof t, "# Pantallas de SaguiOS (D290): modo, lado, principal y resolucion de cada una\n"
                       "modo=%s\nlado=%s\nprincipal=%d\n", modo == TELAS_MODO_ESPELHO ? "espejo" : "extender",
                       lado == TELAS_LADO_ABAIXO ? "abajo" : "derecha", principal);
    for (int i = 0; i < ncab; i++)
        n += snformatar(t + n, sizeof t - (size_t)n, "res%d=%ux%u\n", i, sai[i].largura, sai[i].altura);
    /* D371: con la configuración de la máquina separada, en /etc (SAGUI-CONF); solo el administrador (permisos.c). */
    bool sep = conf_sistema_separada();
    int r = sep ? conf_sistema_gravar("pantallas.conf", t, (uint32_t)n) : escrever_atomico(CONF_PANTALLAS, CONF_NOVO, t, (uint32_t)n);
    klog("[TELAS] guardado en %s (%d bytes): %d\n", sep ? "/etc/pantallas.conf" : CONF_PANTALLAS, n, r);
    return r < 0 ? r : 0;
}

static uint32_t num(const char **p, const char *fim)
{
    uint32_t n = 0;
    while (*p < fim && **p >= '0' && **p <= '9') { n = n * 10 + (uint32_t)(**p - '0'); (*p)++; if (n > 100000) return 0; }
    return n;
}

static void carregar(void)
{
    uint8_t *d;
    uint32_t tam;
    const char *arq = conf_sistema_separada() ? "/etc/pantallas.conf" : CONF_PANTALLAS;   /* D371 */
    if (!vfs_existe_arquivo(arq) || vfs_ler_inteiro(arq, &d, &tam) < 0) return;
    const char *p = (const char *)d, *fim = p + tam;
    while (p < fim) {
        const char *l = p, *le = p;
        while (le < fim && *le != '\n') le++;
        p = le < fim ? le + 1 : le;
        if (le > l && le[-1] == '\r') le--;
        if (l >= le || *l == '#') continue;
        if (txt_comeca_com(l, "modo=")) modo = txt_comeca_com(l + 5, "espejo") ? TELAS_MODO_ESPELHO : TELAS_MODO_EXTENDER;
        else if (txt_comeca_com(l, "lado=")) lado = txt_comeca_com(l + 5, "abajo") ? TELAS_LADO_ABAIXO : TELAS_LADO_DIREITA;
        else if (txt_comeca_com(l, "principal=")) {
            const char *q = l + 10;
            principal = (int)num(&q, le);
        } else if (txt_comeca_com(l, "res") && l[3] >= '0' && l[3] <= '9' && l[4] == '=') {
            int i = l[3] - '0';
            const char *q = l + 5;
            uint32_t rl = num(&q, le), ra = 0;
            if (q < le && *q == 'x') { q++; ra = num(&q, le); }
            if (i < TELAS_MAX) { res_guardada[i][0] = rl; res_guardada[i][1] = ra; }
        }
    }
    kfree(d);
    klog("[TELAS] leido %s: modo %d, lado %d, principal %d\n", CONF_PANTALLAS, modo, lado, principal);
}

void telas_iniciar(void)
{
    if (!video_ativo()) return;
    telas_cabeca0(video_prim_largura(), video_prim_altura());
    if (bga_extra_detectar() == 0) return;
    carregar();
    if (principal < 0 || principal >= ncab) principal = 0;
    for (int i = 1; i < ncab; i++)   /* la resolución guardada, si la salida la acepta */
        if (res_guardada[i][0] && sai[i].mudar_modo &&
            (res_guardada[i][0] != sai[i].largura || res_guardada[i][1] != sai[i].altura))
            sai[i].mudar_modo(&sai[i], res_guardada[i][0], res_guardada[i][1]);
    if (modo == TELAS_MODO_ESPELHO) igualar_espelho();
    aplicar();
}

/* ---- SIS_TELAS */

int32_t sis_telas(uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t e)
{
    (void)c; (void)e;
    switch (op) {
    case TELAS_OP_INFO: {
        info_telas_t t;
        memset(&t, 0, sizeof t);
        t.qtd = (uint32_t)ncab; t.modo = (uint32_t)modo; t.lado = (uint32_t)lado; t.principal = (uint32_t)principal;
        t.desk_largura = video_largura(); t.desk_altura = video_altura();
        for (int i = 0; i < ncab; i++) {
            int x = 0, y = 0;
            telas_rect(i, &x, &y, NULL, NULL);
            t.t[i].x = x; t.t[i].y = y;
            t.t[i].largura = sai[i].largura; t.t[i].altura = sai[i].altura;
            if (i == 0) {
                t.t[i].a_quente = video_troca_a_quente();
                t.t[i].max_largura = video_max_largura(); t.t[i].max_altura = video_max_altura();
            } else {
                t.t[i].a_quente = sai[i].mudar_modo != NULL;
                t.t[i].max_largura = sai[i].max_l; t.t[i].max_altura = sai[i].max_a;
            }
            txt_copia_n(t.t[i].nome, sai[i].nome, sizeof t.t[i].nome);
        }
        uint32_t n = b < sizeof t ? b : (uint32_t)sizeof t;
        if (!usuario_ok((void *)a, n, true)) return ERRO_ENDERECO;
        memcpy((void *)a, &t, n);
        return ncab;
    }
    case TELAS_OP_CONFIG:
        return telas_configurar((int)(a & 0xFF), (int)((a >> 8) & 0xFF), (int)((a >> 16) & 0xFF));
    case TELAS_OP_RESOLUCAO:
        return telas_resolucao((int)a, b & 0xFFFF, b >> 16);
    case TELAS_OP_GUARDAR:
        return telas_guardar();
    }
    return ERRO_INVALIDO;
}
