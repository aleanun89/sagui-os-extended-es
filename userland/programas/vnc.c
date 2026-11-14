// SPDX-License-Identifier: GPL-2.0-or-later
/* vnc - cliente de escritorio remoto VNC (mejora 097 fase 2, D254).
 *
 *   vnc [-p puerto] [--solo-ver] [--pantalla-completa] [--color 8|16|32] [--calidad 0-9]
 *       [--clave-fichero F] [--uno-a-uno] host[:display]
 *
 * Abre una ventana que muestra y maneja el escritorio de un servidor VNC (otro SaguiOS
 * con vncd, TightVNC, TigerVNC, x11vnc...). Protocolo RFB 3.8, 3.7 y 3.3 (RFC 6143), seguridad
 * «None» y «VNC Authentication» (la contraseña se pide en la ventana, o se lee de un fichero
 * con --clave-fichero). Codificaciones: Raw, CopyRect, Hextile, ZRLE y Tight (zlib en sus
 * cuatro flujos, filtros copy, palette y gradient, relleno y JPEG), y las
 * pseudocodificaciones DesktopSize y Cursor. Los decodificadores están en vnc_decod.h y se
 * prueban en el anfitrión. Código propio a partir de la RFC 6143 y la especificación pública de
 * Tight; no hay nada portado de TightVNC ni de otro cliente.
 *
 * La red no bloquea la interfaz: se lee solo lo que hay (disponivel) y cada mensaje se
 * descodifica cuando llega entero; los rectángulos grandes de Raw y Hextile avanzan por filas
 * y baldosas. Solo conectar (resolver el nombre y abrir el TCP) espera, hasta 5 s.
 *
 * Teclado: los códigos de SaguiOS pasan a keysyms de X11 con sus modificadores (SaguiOS no da
 * eventos de tecla soltada: cada pulsación se manda como pulsar y soltar). Ratón: posición,
 * botones y rueda (botones 4 y 5). Si el escritorio remoto es mayor que la ventana se escala
 * (botón «1:1» para verlo sin escalar: la vista se desplaza acercando el puntero al borde).
 * F11 es la pantalla completa del sistema. Si el servidor manda la forma del cursor
 * (pseudocodificación Cursor) se dibuja esa y se oculta el puntero local. */
#include "sagui.h"
#include "gui.h"
#include "imagem.h"
#include "des.h"
#include "compressao.h"
#include "vnc_decod.h"

#define LARG_INICIO 800
#define ALT_INICIO  600
#define BARRA_A     26
#define COR_FONDO_V 0x20252A
#define RX_MAX      (48u * 1024 * 1024)
#define LEER_MAX    (256u * 1024)
#define PLAZO_SALUDO 15000u

enum { E_DESC, E_VERSION, E_SEGLIST, E_SEG33, E_RETO, E_PIDE_CLAVE, E_RESULTADO, E_INIT, E_ACTIVO };

static jan_t *j;
static vnc_dec_t *dec;
static int sk = -1;
static char host[128];
static int puerto = 5900;
static bool solo_ver, quiere_completa, uno_a_uno_opt;
static int color_bits = 32, calidad = -1;
static char clave[16];
static bool tiene_clave, clave_de_fichero;
static int estado = E_DESC;
static char aviso[200];
static char nombre_escr[64];
static int ver_rfb;
static uint8_t reto[16];
static uint8_t *rx;
static unsigned rx_ini, rx_fin, rx_cap;
static unsigned t_estado;
static unsigned long recibidos;
static bool completa;
static caixa_t caja_clave;

/* mensaje FramebufferUpdate en curso */
static int  rects_pend = -1;      /* < 0: ninguno */
static bool rect_vivo;
static int  r_enc, r_x, r_y, r_w, r_h;
static bool suc_vacio = true;
static int  suc_x0, suc_y0, suc_x1, suc_y1;
static uint32_t enc_logados;

/* geometría de la vista */
static bool ajustar = true;
static int zoom = 1000, ox, oy, sl, sa, vx, vy;

/* puntero */
static int ptr_wx = -1000, ptr_wy = -1000, ptr_rx, ptr_ry;
static unsigned ptr_mask, ptr_enviada_mask;
static bool ptr_pend, ptr_dentro;
static unsigned t_borde;

/* cursor remoto (pseudocodificación Cursor) */
static bool cur_remoto;
static int cur_w, cur_h, cur_hx, cur_hy;
static uint32_t cur_px[256 * 256];
static uint8_t cur_mascara[256 * 256];

static botao_t bt[] = {
    { 8, 0, 76, 24, "1:1", false },
    { 92, 0, 120, 24, "Ctrl+Alt+Supr", false },
    { 220, 0, 92, 24, "Reconectar", false },
};
#define BT_ESCALA 0
#define BT_CAD    1
#define BT_RECON  2
#define QTD_BT    3

static void conectar_servidor(void);
static void desenhar_todo(void);

/* ------------------------------------------------------------ geometría y dibujo */

static int area_l(void) { return j->larg; }
static int area_a(void) { int a = j->alt - (completa ? 0 : BARRA_A); return a > 1 ? a : 1; }

static void geometria(void)
{
    if (!dec->fb) { zoom = 1000; sl = sa = ox = oy = 0; return; }
    int al = area_l(), aa = area_a();
    if (ajustar) {
        int zl = al * 1000 / dec->fw, za = aa * 1000 / dec->fh;
        int z = zl < za ? zl : za;
        if (z > 1000) z = 1000;
        if (z < 20) z = 20;
        zoom = z;
    } else zoom = 1000;
    sl = dec->fw * zoom / 1000;
    sa = dec->fh * zoom / 1000;
    if (sl < 1) sl = 1;
    if (sa < 1) sa = 1;
    if (sl <= al) { ox = (al - sl) / 2; vx = 0; }
    else { if (vx > sl - al) vx = sl - al; if (vx < 0) vx = 0; ox = -vx; }
    if (sa <= aa) { oy = (aa - sa) / 2; vy = 0; }
    else { if (vy > sa - aa) vy = sa - aa; if (vy < 0) vy = 0; oy = -vy; }
}

static void cursor_pintar(int x, int y, int l, int a)
{
    if (!cur_remoto || !cur_w || !ptr_dentro) return;
    int cx = ptr_wx - cur_hx, cy = ptr_wy - cur_hy;
    for (int jj = 0; jj < cur_h; jj++) {
        int py = cy + jj;
        if (py < y || py >= y + a || py >= j->alt) continue;
        for (int i = 0; i < cur_w; i++) {
            int px = cx + i;
            if (px < x || px >= x + l || px >= j->larg || !cur_mascara[jj * cur_w + i]) continue;
            j->px[py * j->larg + px] = cur_px[jj * cur_w + i];
        }
    }
}

/* Repinta un trozo del área de la imagen (coordenadas de la ventana). */
static void pintar_area(int x, int y, int l, int a)
{
    int al = area_l(), aa = area_a();
    if (x < 0) { l += x; x = 0; }
    if (y < 0) { a += y; y = 0; }
    if (x + l > al) l = al - x;
    if (y + a > aa) a = aa - y;
    if (l <= 0 || a <= 0) return;
    g_retangulo(j, x, y, l, a, COR_FONDO_V);
    if (dec->fb) {
        imagem_t im;
        im.larg = dec->fw; im.alt = dec->fh; im.px = dec->fb; im.formato[0] = 0;
        imagem_desenhar(j->px, j->larg, x, y, l, a, &im, ox, oy, sl, sa, zoom < 1000);
    }
    cursor_pintar(x, y, l, a);
    jan_mostrar_area(j, x, y, l, a);
}

static void pintar_remoto(int x, int y, int w, int h)
{
    int x0 = ox + x * zoom / 1000 - 1, y0 = oy + y * zoom / 1000 - 1;
    int x1 = ox + ((x + w) * zoom + 999) / 1000 + 1, y1 = oy + ((y + h) * zoom + 999) / 1000 + 1;
    pintar_area(x0, y0, x1 - x0, y1 - y0);
}

static void centrado(int y, const char *t, uint32_t cor)
{
    int l = g_largura_texto(t);
    g_texto(j, (j->larg - l) / 2 < 4 ? 4 : (j->larg - l) / 2, y, t, cor);
}

static void dibujar_barra(void)
{
    if (completa) return;
    int y = j->alt - BARRA_A;
    g_retangulo(j, 0, y, j->larg, BARRA_A, COR_FUNDO);
    g_retangulo(j, 0, y, j->larg, 1, COR_BORDA);
    for (int i = 0; i < QTD_BT; i++) bt[i].y = y + 1;
    bt[BT_ESCALA].texto = ajustar ? "1:1" : "Ajustar";
    botao_desenhar(j, &bt[BT_ESCALA]);
    if (estado == E_ACTIVO && !solo_ver) botao_desenhar(j, &bt[BT_CAD]);
    if (estado == E_DESC) botao_desenhar(j, &bt[BT_RECON]);
    char t[160];
    if (estado == E_ACTIVO)
        formatar_texto(t, sizeof t, "%dx%d  %d%%%s  %lu KB", dec->fw, dec->fh, zoom / 10, solo_ver ? "  solo ver" : "", recibidos / 1024);
    else if (estado == E_DESC) copia_texto(t, "desconectado", sizeof t);
    else copia_texto(t, "conectando...", sizeof t);
    int x = estado == E_ACTIVO && !solo_ver ? 226 : estado == E_DESC ? 322 : 100;
    if (x + 80 < j->larg) g_texto_n(j, x, y + 5, t, (j->larg - x - 8) / LETRA_L, COR_TEXTO_FRACO);
    jan_mostrar_area(j, 0, y, j->larg, BARRA_A);
}

static void dibujar_clave(void)
{
    int al = area_l(), aa = area_a();
    g_retangulo(j, 0, 0, al, aa, COR_FUNDO);
    int y = aa / 2 - 50;
    char t[200];
    formatar_texto(t, sizeof t, "%s:%d pide contraseña", host, puerto);
    centrado(y, t, COR_TEXTO);
    centrado(y + 22, "(Intro para entrar, Esc para cancelar)", COR_TEXTO_FRACO);
    caja_clave.l = 260;
    caja_clave.x = (al - caja_clave.l) / 2;
    caja_clave.y = y + 50;
    caja_clave.oculta = true;
    caja_clave.foco = true;
    caixa_desenhar(j, &caja_clave);
    jan_mostrar_area(j, 0, 0, al, aa);
}

static void dibujar_estado_conexion(void)
{
    int al = area_l(), aa = area_a();
    g_retangulo(j, 0, 0, al, aa, COR_FONDO_V);
    char t[200];
    const char *paso = estado == E_VERSION ? "saludo" : estado == E_RESULTADO ? "autenticación" : estado == E_INIT ? "iniciando la sesión" : "negociando";
    formatar_texto(t, sizeof t, "Conectado con %s:%d: %s...", host, puerto, paso);
    centrado(aa / 2 - 8, t, 0xE6E8EA);
    jan_mostrar_area(j, 0, 0, al, aa);
}

static void desenhar_todo(void)
{
    geometria();
    if (estado == E_PIDE_CLAVE) dibujar_clave();
    else if (estado != E_ACTIVO && estado != E_DESC) dibujar_estado_conexion();
    else {
        pintar_area(0, 0, area_l(), area_a());
        if (estado == E_DESC) {
            int l = area_l();
            g_retangulo(j, 0, 0, l, 40, 0x8B1A1A);
            g_texto_n(j, 10, 4, aviso[0] ? aviso : "Conexión cerrada", (l - 20) / LETRA_L, COR_BRANCO);
            g_texto_n(j, 10, 21, "Pulsa R, Intro o el botón Reconectar para volver a conectar", (l - 20) / LETRA_L, 0xF2D0D0);
            jan_mostrar_area(j, 0, 0, l, 40);
        }
    }
    dibujar_barra();
}

/* ------------------------------------------------------------ red */

static const char *msg_red(int e)
{
    switch (e) {
    case ERRO_TEMPO: return "el servidor no contesta";
    case ERRO_RECUSADA: return "conexión rechazada (¿está el servidor en marcha y el puerto abierto?)";
    case ERRO_INALCANCAVEL: return "sin ruta hacia el servidor";
    case ERRO_SEM_REDE: return "no hay red";
    case ERRO_NAO_EXISTE: return "no se encuentra ese nombre";
    default: return "error de red";
    }
}

static void cerrar_red(void)
{
    if (sk >= 0) { fechar(sk); sk = -1; }
}

static void caer(const char *motivo)
{
    cerrar_red();
    estado = E_DESC;
    rects_pend = -1; rect_vivo = false;
    copia_texto(aviso, motivo, sizeof aviso);
    registrar("vnc: desconectado: %s\n", motivo);
    desenhar_todo();
}

static void tx(const void *d, unsigned n)
{
    if (sk < 0) return;
    if (escrever_tudo(sk, d, n) < 0) caer("se perdió la conexión al enviar");
}

static void tx_u8(uint8_t v) { tx(&v, 1); }

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static int rd16(const uint8_t *p) { return p[0] << 8 | p[1]; }
static void wr16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

static void pedir_actualizacion(bool incremental)
{
    uint8_t m[10] = { 3, incremental, 0, 0, 0, 0, 0, 0, 0, 0 };
    wr16(m + 6, (unsigned)dec->fw); wr16(m + 8, (unsigned)dec->fh);
    tx(m, sizeof m);
}

static void enviar_formato_y_codificaciones(void)
{
    uint8_t m[20];
    memset(m, 0, sizeof m);
    m[0] = 0;
    m[4] = (uint8_t)color_bits;
    if (color_bits == 32) { m[5] = 24; wr16(m + 8, 255); wr16(m + 10, 255); wr16(m + 12, 255); m[14] = 16; m[15] = 8; m[16] = 0; }
    else if (color_bits == 16) { m[5] = 16; wr16(m + 8, 31); wr16(m + 10, 63); wr16(m + 12, 31); m[14] = 11; m[15] = 5; m[16] = 0; }
    else { m[5] = 8; wr16(m + 8, 7); wr16(m + 10, 7); wr16(m + 12, 3); m[14] = 5; m[15] = 2; m[16] = 0; }
    m[6] = 0;       /* little endian */
    m[7] = 1;       /* color verdadero */
    tx(m, sizeof m);
    vnc_pf_t pf;
    memset(&pf, 0, sizeof pf);
    pf.bpp = m[4]; pf.profundidad = m[5]; pf.big = 0; pf.color_verdadero = 1;
    for (int c = 0; c < 3; c++) { pf.max[c] = (uint16_t)rd16(m + 8 + 2 * c); pf.desp[c] = m[14 + c]; }
    vnc_dec_formato(dec, &pf);
    vnc_dec_reiniciar_flujos(dec);

    int enc[16], n = 0;
    enc[n++] = VNC_ENC_TIGHT; enc[n++] = VNC_ENC_ZRLE; enc[n++] = VNC_ENC_HEXTILE;
    enc[n++] = VNC_ENC_COPYRECT; enc[n++] = VNC_ENC_RAW;
    enc[n++] = VNC_ENC_CURSOR; enc[n++] = VNC_ENC_TAMANO; enc[n++] = VNC_ENC_ULTIMO;
    enc[n++] = -256 + 6;                         /* nivel de compresión de Tight */
    if (calidad >= 0) enc[n++] = -32 + calidad;  /* calidad JPEG (sin ella el servidor no manda JPEG) */
    uint8_t e[4 + 16 * 4];
    e[0] = 2; e[1] = 0; wr16(e + 2, (unsigned)n);
    for (int i = 0; i < n; i++) wr32(e + 4 + 4 * i, (uint32_t)enc[i]);
    tx(e, 4 + 4 * (unsigned)n);
}

static void enviar_puntero(void)
{
    if (estado != E_ACTIVO || solo_ver) { ptr_pend = false; return; }
    uint8_t m[6] = { 5, 0, 0, 0, 0, 0 };
    unsigned b = 0;
    if (ptr_mask & 1) b |= 1;
    if (ptr_mask & 4) b |= 2;
    if (ptr_mask & 2) b |= 4;
    m[1] = (uint8_t)b;
    wr16(m + 2, (unsigned)ptr_rx); wr16(m + 4, (unsigned)ptr_ry);
    tx(m, sizeof m);
    ptr_pend = false;
}

static void enviar_tecla_x(uint32_t ks, bool abajo)
{
    uint8_t m[8] = { 4, abajo, 0, 0, 0, 0, 0, 0 };
    wr32(m + 4, ks);
    tx(m, sizeof m);
}

static void enviar_tecla(unsigned t, unsigned mods)
{
    uint32_t ks = vnc_keysym(t);
    /* Ctrl + letra llega como código de control: se manda la letra con Ctrl */
    if ((mods & 2) && t >= 1 && t <= 26 && t != 8 && t != 9 && t != 10 && t != 13) ks = 'a' + t - 1;
    if (!ks) return;
    bool shift = (mods & 1) && ks >= 0xFF00;
    if (mods & 2) enviar_tecla_x(0xFFE3, true);
    if (mods & 4) enviar_tecla_x(0xFFE9, true);
    if (shift) enviar_tecla_x(0xFFE1, true);
    enviar_tecla_x(ks, true);
    enviar_tecla_x(ks, false);
    if (shift) enviar_tecla_x(0xFFE1, false);
    if (mods & 4) enviar_tecla_x(0xFFE9, false);
    if (mods & 2) enviar_tecla_x(0xFFE3, false);
}

static void ctrl_alt_supr(void)
{
    enviar_tecla_x(0xFFE3, true); enviar_tecla_x(0xFFE9, true); enviar_tecla_x(0xFFFF, true);
    enviar_tecla_x(0xFFFF, false); enviar_tecla_x(0xFFE9, false); enviar_tecla_x(0xFFE3, false);
}

/* La posición del puntero de la ventana en la pantalla remota. */
static void puntero_a_remoto(int wx, int wy)
{
    int fx = zoom ? (wx - ox) * 1000 / zoom : 0, fy = zoom ? (wy - oy) * 1000 / zoom : 0;
    if (fx < 0) fx = 0;
    if (fy < 0) fy = 0;
    if (dec->fw && fx >= dec->fw) fx = dec->fw - 1;
    if (dec->fh && fy >= dec->fh) fy = dec->fh - 1;
    ptr_rx = fx; ptr_ry = fy;
}

static bool jpeg_libsagui(const uint8_t *d, int n, int w, int h, uint32_t *px)
{
    imagem_t im;
    if (imagem_ler_jpeg(d, n, &im) != 0) return false;
    bool ok = im.larg == w && im.alt == h;
    if (ok) memcpy(px, im.px, (size_t)w * h * 4);
    imagem_liberar(&im);
    return ok;
}

static void sucio_sumar(int x, int y, int w, int h)
{
    if (suc_vacio) { suc_x0 = x; suc_y0 = y; suc_x1 = x + w; suc_y1 = y + h; suc_vacio = false; return; }
    if (x < suc_x0) suc_x0 = x;
    if (y < suc_y0) suc_y0 = y;
    if (x + w > suc_x1) suc_x1 = x + w;
    if (y + h > suc_y1) suc_y1 = y + h;
}

static void registrar_codificaciones(void)
{
    static const char *const nombres[] = { "Raw", "CopyRect", "Hextile", "ZRLE", "Tight-copy", "Tight-paleta", "Tight-gradiente",
                                            "Tight-relleno", "Tight-JPEG", "Cursor" };
    uint32_t nuevos = dec->visto & ~enc_logados;
    for (int i = 0; i < 10; i++)
        if (nuevos & (1u << i)) registrar("vnc: primera vez %s\n", nombres[i]);
    enc_logados |= nuevos;
}

/* Inicia la parte de después del apretón de manos. */
static void iniciar_sesion(void)
{
    estado = E_INIT;
    t_estado = tempo_ms();
}

/* Interpreta lo que hay en el búfer. Devuelve true si avanzó (consumió algo o cambió de estado). */
static bool paso(void)
{
    unsigned n = rx_fin - rx_ini;
    uint8_t *p = rx + rx_ini;
    switch (estado) {
    case E_VERSION: {
        if (n < 12) return false;
        if (memcmp(p, "RFB ", 4) != 0 || p[7] != '.' || p[11] != '\n') { caer("el servidor no habla VNC (RFB)"); return false; }
        int mayor = (p[4] - '0') * 100 + (p[5] - '0') * 10 + (p[6] - '0'), menor = (p[8] - '0') * 100 + (p[9] - '0') * 10 + (p[10] - '0');
        rx_ini += 12;
        if (mayor != 3) { caer("versión de RFB no admitida"); return false; }
        ver_rfb = menor >= 8 ? 8 : menor == 7 ? 7 : 3;
        const char *v = ver_rfb == 8 ? "RFB 003.008\n" : ver_rfb == 7 ? "RFB 003.007\n" : "RFB 003.003\n";
        tx(v, 12);
        registrar("vnc: servidor RFB 3.%d, se usa 3.%d\n", menor, ver_rfb);
        estado = ver_rfb == 3 ? E_SEG33 : E_SEGLIST;
        return true;
    }
    case E_SEGLIST: {
        if (n < 1) return false;
        int k = p[0];
        if (k == 0) {
            if (n < 5) return false;
            unsigned l = rd32(p + 1);
            if (n < 5 + l) return false;
            char t[160];
            unsigned m = l < sizeof t - 40 ? l : (unsigned)sizeof t - 40;
            memcpy(t, p + 5, m); t[m] = 0;
            char r[200];
            formatar_texto(r, sizeof r, "el servidor rechaza la conexión: %s", t);
            rx_ini += 5 + l;
            caer(r);
            return false;
        }
        if (n < 1u + (unsigned)k) return false;
        int elegido = 0;
        for (int i = 0; i < k; i++) if (p[1 + i] == 2) elegido = 2;
        for (int i = 0; i < k && !elegido; i++) if (p[1 + i] == 1) elegido = 1;
        rx_ini += 1 + (unsigned)k;
        if (!elegido) { caer("el servidor no ofrece «None» ni «VNC Authentication»"); return false; }
        tx_u8((uint8_t)elegido);
        registrar("vnc: seguridad %d\n", elegido);
        if (elegido == 2) estado = E_RETO;
        else if (ver_rfb == 8) estado = E_RESULTADO;
        else { tx_u8(1); iniciar_sesion(); }
        return true;
    }
    case E_SEG33: {
        if (n < 4) return false;
        uint32_t t = rd32(p);
        if (t == 0) {
            if (n < 8) return false;
            unsigned l = rd32(p + 4);
            if (n < 8 + l) return false;
            char tt[160], r[200];
            unsigned m = l < sizeof tt - 40 ? l : (unsigned)sizeof tt - 40;
            memcpy(tt, p + 8, m); tt[m] = 0;
            formatar_texto(r, sizeof r, "el servidor rechaza la conexión: %s", tt);
            rx_ini += 8 + l;
            caer(r);
            return false;
        }
        rx_ini += 4;
        if (t == 1) { tx_u8(1); iniciar_sesion(); }
        else if (t == 2) estado = E_RETO;
        else { caer("tipo de seguridad no admitido"); return false; }
        return true;
    }
    case E_RETO: {
        if (n < 16) return false;
        memcpy(reto, p, 16);
        rx_ini += 16;
        if (!tiene_clave) {
            estado = E_PIDE_CLAVE;
            memset(&caja_clave, 0, sizeof caja_clave);
            desenhar_todo();
            return false;
        }
        uint8_t resp[16];
        vnc_respuesta(clave, reto, resp);
        tx(resp, 16);
        estado = E_RESULTADO;
        return true;
    }
    case E_PIDE_CLAVE: return false;
    case E_RESULTADO: {
        if (n < 4) return false;
        uint32_t r = rd32(p);
        if (r == 0) { rx_ini += 4; tx_u8(1); iniciar_sesion(); return true; }
        if (ver_rfb == 8) {
            if (n < 8) return false;
            unsigned l = rd32(p + 4);
            if (n < 8 + l) return false;
        }
        rx_ini = rx_fin;
        if (clave_de_fichero) caer("contraseña incorrecta (la del fichero)");
        else { tiene_clave = false; caer("contraseña incorrecta"); }
        return false;
    }
    case E_INIT: {
        if (n < 24) return false;
        unsigned l = rd32(p + 20);
        if (l > 4096) { caer("nombre de escritorio absurdo"); return false; }
        if (n < 24 + l) return false;
        int w = rd16(p), h = rd16(p + 2);
        unsigned m = l < sizeof nombre_escr - 1 ? l : (unsigned)sizeof nombre_escr - 1;
        memcpy(nombre_escr, p + 24, m); nombre_escr[m] = 0;
        rx_ini += 24 + l;
        if (!vnc_dec_pantalla(dec, w, h)) { caer("pantalla remota demasiado grande o sin memoria"); return false; }
        vx = vy = 0;
        enviar_formato_y_codificaciones();
        pedir_actualizacion(false);
        estado = E_ACTIVO;
        char t[120];
        formatar_texto(t, sizeof t, "%s - VNC", nombre_escr[0] ? nombre_escr : host);
        titulo_janela(j->id, t);
        registrar("vnc: conectado, escritorio %dx%d \"%s\" (%d bpp, RFB 3.%d)\n", w, h, nombre_escr, color_bits, ver_rfb);
        desenhar_todo();
        return true;
    }
    case E_ACTIVO: break;
    default: return false;
    }

    /* sesión activa: mensajes del servidor */
    if (rects_pend < 0) {
        if (n < 1) return false;
        switch (p[0]) {
        case 0:
            if (n < 4) return false;
            rects_pend = rd16(p + 2);
            rx_ini += 4;
            return true;
        case 1: {   /* SetColourMapEntries: no hay paleta, se descarta */
            if (n < 6) return false;
            unsigned t = 6 + 6u * (unsigned)rd16(p + 4);
            if (n < t) return false;
            rx_ini += t;
            return true;
        }
        case 2: rx_ini += 1; return true;   /* Bell */
        case 3: {   /* ServerCutText */
            if (n < 8) return false;
            unsigned l = rd32(p + 4);
            if (l > 1024 * 1024) { caer("texto del portapapeles absurdo"); return false; }
            if (n < 8 + l) return false;
            if (l) copiar_texto((const char *)(p + 8), l < PORTAPAPEIS_MAX ? l : PORTAPAPEIS_MAX);
            rx_ini += 8 + l;
            return true;
        }
        default: {
            char t[80];
            formatar_texto(t, sizeof t, "mensaje %d desconocido del servidor", p[0]);
            caer(t);
            return false;
        }
        }
    }
    if (rects_pend == 0) {   /* fin de la actualización */
        rects_pend = -1;
        if (!suc_vacio) {
            pintar_remoto(suc_x0, suc_y0, suc_x1 - suc_x0, suc_y1 - suc_y0);
            suc_vacio = true;
        }
        registrar_codificaciones();
        dibujar_barra();
        pedir_actualizacion(true);
        return true;
    }
    if (!rect_vivo) {
        if (n < 12) return false;
        r_x = rd16(p); r_y = rd16(p + 2); r_w = rd16(p + 4); r_h = rd16(p + 6);
        r_enc = (int)rd32(p + 8);
        rx_ini += 12;
        if (r_enc == VNC_ENC_ULTIMO) { rects_pend = 0; return true; }
        if (r_enc == VNC_ENC_TAMANO) {
            if (!vnc_dec_pantalla(dec, r_w, r_h)) { caer("pantalla remota demasiado grande o sin memoria"); return false; }
            vx = vy = 0;
            registrar("vnc: el escritorio cambia a %dx%d\n", r_w, r_h);
            desenhar_todo();
            rects_pend--;
            return true;
        }
        vnc_dec_empezar(dec);
        rect_vivo = true;
        return true;
    }
    /* los datos del rectángulo */
    bool hecho = false;
    int r;
    if (r_enc == VNC_ENC_CURSOR) {
        if (r_w > 256 || r_h > 256) { caer("cursor demasiado grande"); return false; }
        if (r_w == 0 || r_h == 0) { r = 0; hecho = true; }
        else {
            r = vnc_dec_cursor(dec, r_w, r_h, p, (int)n, cur_px, cur_mascara);
            if (r == 0) return false;
            if (r > 0) hecho = true;
        }
        if (hecho) {
            if (cur_remoto && cur_w) { int w0 = cur_w, h0 = cur_h; cur_w = 0; pintar_area(ptr_wx - cur_hx, ptr_wy - cur_hy, w0, h0); }
            cur_remoto = true;
            cur_w = r_w; cur_h = r_h; cur_hx = r_x; cur_hy = r_y;
            jan_cursor_visivel(j, false);
            dec->visto |= 1u << VNC_VISTO_CURSOR;
            if (cur_w) pintar_area(ptr_wx - cur_hx, ptr_wy - cur_hy, cur_w, cur_h);
        }
    } else {
        r = vnc_dec_rect(dec, r_enc, r_x, r_y, r_w, r_h, p, (int)n, &hecho);
    }
    if (r < 0) {
        char t[120];
        formatar_texto(t, sizeof t, "error al descodificar (codificación %d, código %d)", r_enc, r);
        caer(t);
        return false;
    }
    rx_ini += (unsigned)r;
    if (hecho) {
        if (r_enc != VNC_ENC_CURSOR) sucio_sumar(r_x, r_y, r_w, r_h);
        rect_vivo = false;
        rects_pend--;
        return true;
    }
    return r > 0;
}

/* Lee lo que haya y lo interpreta. Devuelve true si hizo algo. */
static bool red_servir(void)
{
    bool hizo = false;
    int d = disponivel(sk);
    if (d < 0) { caer(estado == E_ACTIVO ? "el servidor cerró la conexión" : "el servidor cerró la conexión durante el apretón de manos"); return true; }
    if (d > 0) {
        if (rx_ini == rx_fin) rx_ini = rx_fin = 0;
        if (rx_fin == rx_cap) {
            if (rx_ini) { memmove(rx, rx + rx_ini, rx_fin - rx_ini); rx_fin -= rx_ini; rx_ini = 0; }
            else {
                if (rx_cap >= RX_MAX) { caer("mensaje del servidor demasiado grande"); return true; }
                unsigned nc = rx_cap * 2;
                uint8_t *nr = realoca(rx, nc);
                if (!nr) { caer("sin memoria"); return true; }
                rx = nr; rx_cap = nc;
            }
        }
        unsigned quiero = (unsigned)d < LEER_MAX ? (unsigned)d : LEER_MAX;
        if (quiero > rx_cap - rx_fin) quiero = rx_cap - rx_fin;
        tempo_limite(sk, 1000);
        int k = ler(sk, rx + rx_fin, quiero);
        if (k <= 0) { caer("el servidor cerró la conexión"); return true; }
        rx_fin += (unsigned)k;
        recibidos += (unsigned)k;
        hizo = true;
        t_estado = tempo_ms();
    }
    while (sk >= 0 && paso()) hizo = true;
    if (sk >= 0 && rx_ini && rx_ini == rx_fin) rx_ini = rx_fin = 0;
    if (sk >= 0 && estado != E_ACTIVO && estado != E_PIDE_CLAVE && tempo_ms() - t_estado > PLAZO_SALUDO) {
        caer("tiempo agotado esperando al servidor");
        return true;
    }
    return hizo;
}

static void conectar_servidor(void)
{
    cerrar_red();
    rx_ini = rx_fin = 0;
    rects_pend = -1; rect_vivo = false; suc_vacio = true;
    recibidos = 0;
    aviso[0] = 0;
    estado = E_VERSION;
    t_estado = tempo_ms();
    desenhar_todo();
    registrar("vnc: conectando a %s:%d\n", host, puerto);
    int fd = conectar_tcp(host, puerto, 5000);
    if (fd < 0) {
        char t[200];
        formatar_texto(t, sizeof t, "no se pudo conectar con %s:%d: %s", host, puerto, msg_red(fd));
        caer(t);
        return;
    }
    sk = fd;
    t_estado = tempo_ms();
}

/* ------------------------------------------------------------ eventos */

static void cambiar_escala(void)
{
    ajustar = !ajustar;
    if (!ajustar && dec->fb) { vx = (ptr_rx > 0 ? ptr_rx : dec->fw / 2) - area_l() / 2; vy = (ptr_ry > 0 ? ptr_ry : dec->fh / 2) - area_a() / 2; }
    desenhar_todo();
    registrar("vnc: escala %s (vista %d,%d)\n", ajustar ? "ajustada" : "1:1", vx, vy);
}

static bool tratar_clave(evento_t *e)
{
    if (e->tipo == EV_TECLA && e->tecla == TECLA_ESC) {
        cerrar_red();
        copia_texto(aviso, "conexión cancelada: no se dio la contraseña", sizeof aviso);
        estado = E_DESC;
        desenhar_todo();
        return true;
    }
    if (caixa_tratar(j, &caja_clave, e)) {
        copia_texto(clave, caja_clave.texto, sizeof clave);
        tiene_clave = true;
        uint8_t resp[16];
        vnc_respuesta(clave, reto, resp);
        tx(resp, 16);
        estado = E_RESULTADO;
        t_estado = tempo_ms();
        desenhar_todo();
    }
    return true;
}

static bool tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) return false;
    if (e->tipo == EV_REDIMENSIONAR) {
        bool cheia = e->tecla == JAN_ESTADO_TELA_CHEIA;
        if (jan_redimensionar(j, e)) {
            completa = cheia;
            registrar("vnc: ventana %dx%d%s\n", j->larg, j->alt, completa ? " (pantalla completa)" : "");
            desenhar_todo();
        }
        return true;
    }
    if (estado == E_PIDE_CLAVE) return tratar_clave(e);
    if (!completa && (e->tipo == EV_CLIQUE || e->tipo == EV_SOLTOU))
        for (int i = 0; i < QTD_BT; i++) {
            if (i == BT_CAD && !(estado == E_ACTIVO && !solo_ver)) continue;
            if (i == BT_RECON && estado != E_DESC) continue;
            if (botao_tratar(j, &bt[i], e)) {
                if (i == BT_ESCALA) cambiar_escala();
                else if (i == BT_CAD) ctrl_alt_supr();
                else conectar_servidor();
                return true;
            }
        }
    if (estado == E_DESC) {
        if (e->tipo == EV_TECLA && (e->tecla == 'r' || e->tecla == 'R' || e->tecla == '\n')) conectar_servidor();
        return true;
    }
    if (estado != E_ACTIVO || solo_ver) return true;
    if (e->tipo == EV_TECLA) { enviar_tecla(e->tecla, e->modificadores); return true; }
    bool en_area = e->x >= 0 && e->y >= 0 && e->x < area_l() && e->y < area_a();
    if (e->tipo == EV_MOVER || e->tipo == EV_CLIQUE || e->tipo == EV_SOLTOU) {
        if (!en_area && ptr_mask == 0 && e->tipo != EV_SOLTOU) { ptr_dentro = false; return true; }
        /* con un botón apretado el sistema sigue mandando el movimiento aunque salga de la ventana */
        int nx = e->x, ny = e->y;
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        if (nx >= area_l()) nx = area_l() - 1;
        if (ny >= area_a()) ny = area_a() - 1;
        int ax = ptr_wx, ay = ptr_wy;
        bool vieja = ptr_dentro;
        ptr_wx = nx; ptr_wy = ny; ptr_dentro = true;
        puntero_a_remoto(nx, ny);
        if (e->tipo == EV_MOVER) ptr_mask = e->botoes;
        else if (e->tipo == EV_CLIQUE) ptr_mask |= e->botoes;
        else ptr_mask &= ~e->botoes;
        if (cur_remoto && cur_w) {
            if (vieja) pintar_area(ax - cur_hx, ay - cur_hy, cur_w, cur_h);
            pintar_area(nx - cur_hx, ny - cur_hy, cur_w, cur_h);
        }
        if (e->tipo == EV_MOVER) ptr_pend = true;
        else enviar_puntero();
        return true;
    }
    if (e->tipo == EV_RODA && en_area) {
        int n = (int)e->cliques;
        unsigned boton = n < 0 ? 8 : 16;   /* RFB: 4 = arriba, 5 = abajo */
        if (n < 0) n = -n;
        if (n > 8) n = 8;
        puntero_a_remoto(e->x, e->y);
        unsigned mascara = ptr_mask;
        for (int i = 0; i < n; i++) {
            ptr_mask = mascara; enviar_puntero();
            uint8_t m[6] = { 5, 0, 0, 0, 0, 0 };
            m[1] = (uint8_t)((mascara & 1 ? 1 : 0) | (mascara & 4 ? 2 : 0) | (mascara & 2 ? 4 : 0) | boton);
            wr16(m + 2, (unsigned)ptr_rx); wr16(m + 4, (unsigned)ptr_ry);
            tx(m, sizeof m);
        }
        ptr_mask = mascara;
        enviar_puntero();
        return true;
    }
    return true;
}

/* Con la vista sin escalar y mayor que la ventana, acercar el puntero al borde la desplaza. */
static void desplazar_borde(void)
{
    if (estado != E_ACTIVO || ajustar || !ptr_dentro) return;
    if (sl <= area_l() && sa <= area_a()) return;
    unsigned t = tempo_ms();
    if (t - t_borde < 30) return;
    t_borde = t;
    int dx = 0, dy = 0, z = 24;
    if (ptr_wx < z) dx = -24; else if (ptr_wx >= area_l() - z) dx = 24;
    if (ptr_wy < z) dy = -24; else if (ptr_wy >= area_a() - z) dy = 24;
    if (!dx && !dy) return;
    int ovx = vx, ovy = vy;
    vx += dx; vy += dy;
    geometria();
    if (vx == ovx && vy == ovy) return;
    puntero_a_remoto(ptr_wx, ptr_wy);
    ptr_pend = true;
    pintar_area(0, 0, area_l(), area_a());
}

/* ------------------------------------------------------------ programa */

static void uso(void)
{
    imprimir("uso: vnc [-p puerto] [--solo-ver] [--pantalla-completa] [--color 8|16|32] [--calidad 0-9]\n"
             "           [--clave-fichero F] [--uno-a-uno] host[:display]\n"
             "Escritorio remoto VNC (RFB 3.8, 3.7 y 3.3). display 1 = puerto 5901; host::5900 da el puerto.\n"
             "Codificaciones: Tight, ZRLE, Hextile, CopyRect y Raw. --color baja los bits por píxel (redes lentas);\n"
             "--calidad activa el JPEG de Tight (0 peor, 9 mejor). Para un servidor SaguiOS: ssh -L 5900:localhost:5900.\n");
}

static void leer_clave_fichero(const char *f)
{
    int fd = abrir(f, ABRIR_LER);
    if (fd < 0) { imprimir_em(ERROS, "vnc: no se puede abrir %s\n", f); return; }
    char b[64];
    int n = ler(fd, b, sizeof b - 1);
    fechar(fd);
    if (n < 0) n = 0;
    b[n] = 0;
    for (int i = 0; i < n; i++) if (b[i] == '\n' || b[i] == '\r') { b[i] = 0; break; }
    copia_texto(clave, b, sizeof clave);
    tiene_clave = true;
    clave_de_fichero = true;
}

int principal(int argc, char **argv)
{
    static const opcao_longa_t largas[] = {
        { "solo-ver", 's' }, { "pantalla-completa", 'f' }, { "color", 'c' }, { "calidad", 'q' },
        { "clave-fichero", 'k' }, { "uno-a-uno", '1' }, { "puerto", 'p' }, { NULL, 0 }
    };
    opcoes_t o;
    memset(&o, 0, sizeof o);
    o.longas = largas;
    int r;
    while ((r = obter_opcao(argc, argv, "p:c:q:k:sf1", &o)) != -1) {
        int v;
        switch (r) {
        case OPC_AJUDA: uso(); return 0;
        case 'p': if (!texto_para_numero(o.valor, &v) || v < 1 || v > 65535) { imprimir_em(ERROS, "vnc: puerto no válido\n"); return 2; } puerto = v; break;
        case 'c': if (!texto_para_numero(o.valor, &v) || (v != 8 && v != 16 && v != 32)) { imprimir_em(ERROS, "vnc: --color admite 8, 16 o 32\n"); return 2; } color_bits = v; break;
        case 'q': if (!texto_para_numero(o.valor, &v) || v > 9) { imprimir_em(ERROS, "vnc: --calidad admite de 0 a 9\n"); return 2; } calidad = v; break;
        case 'k': leer_clave_fichero(o.valor); break;
        case 's': solo_ver = true; break;
        case 'f': quiere_completa = true; break;
        case '1': uno_a_uno_opt = true; break;
        default: return opcao_invalida("vnc", r, &o);
        }
    }
    if (o.indice >= argc) { uso(); return 2; }
    {
        const char *a = argv[o.indice];
        copia_texto(host, a, sizeof host);
        char *dp = acha_char(host, ':');
        if (dp) {
            *dp++ = 0;
            bool doble = *dp == ':';
            if (doble) dp++;
            int v;
            if (!texto_para_numero(dp, &v) || v > 65535) { imprimir_em(ERROS, "vnc: display o puerto no válido: %s\n", a); return 2; }
            puerto = (!doble && v < 100) ? 5900 + v : v;
        }
        if (!host[0]) copia_texto(host, "localhost", sizeof host);
    }
    if (uno_a_uno_opt) ajustar = false;

    dec = aloca_zerado(sizeof *dec);
    rx_cap = 64 * 1024;
    rx = aloca(rx_cap);
    if (!dec || !rx) { imprimir_em(ERROS, "vnc: sin memoria\n"); return 1; }
    dec->jpeg = jpeg_libsagui;
    vnc_pf_t pf0;
    vnc_pf_nativo(&pf0);
    vnc_dec_formato(dec, &pf0);

    char titulo[160];
    formatar_texto(titulo, sizeof titulo, "VNC %s:%d", host, puerto);
    j = jan_abrir(titulo, LARG_INICIO, ALT_INICIO);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, 320, 200);
    if (quiere_completa) jan_tela_cheia(j, true);
    registrar("vnc: pronto\n");
    conectar_servidor();

    evento_t e;
    bool seguir = true;
    unsigned visto = (unsigned)esperar_rede(0, 0);
    while (seguir) {
        bool hizo = false;
        while (seguir && proximo_evento(j->id, &e, false) == 1) { seguir = tratar(&e); hizo = true; }
        if (!seguir) break;
        if (sk >= 0 && red_servir()) hizo = true;
        if (ptr_pend) { enviar_puntero(); hizo = true; }
        desplazar_borde();
        if (hizo) continue;
        if (sk >= 0) visto = (unsigned)esperar_rede(visto, 10);
        else if (esperar_evento(j->id, &e, 100) == 1) seguir = tratar(&e);
    }
    registrar("vnc: cerrado (%lu KB recibidos)\n", recibidos / 1024);
    cerrar_red();
    vnc_dec_liberar(dec);
    jan_fechar(j);
    return 0;
}
