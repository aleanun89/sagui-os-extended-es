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
 * (pseudocodificación Cursor) se dibuja esa y se oculta el puntero local.
 *
 * Sin argumentos abre la ventana de conexión (D273, mejora 104): equipo, puerto, contraseña, colores,
 * calidad, solo ver y pantalla completa, con una lista de conexiones guardadas en
 * /home/.config/vnc.conf (una por línea: nombre|equipo|puerto|colores|calidad|solo_ver|completa|clave).
 * La contraseña solo se guarda si se marca «Recordar», ofuscada con DES y una clave fija: impide leerla
 * de pasada, pero NO es cifrado (cualquiera con acceso al archivo puede deshacerlo). «vnc --conexion
 * NOMBRE» abre una guardada directamente. */
#include "sagui.h"
#include "gui.h"
#include "imagem.h"
#include "des.h"
#include "compressao.h"
#include "vnc_decod.h"
#include "servicos.h"

#define LARG_INICIO 800
#define ALT_INICIO  600
#define BARRA_A     26
#define COR_FONDO_V 0x20252A
#define RX_MAX      (48u * 1024 * 1024)
#define LEER_MAX    (256u * 1024)
#define PLAZO_SALUDO 15000u

enum { E_DESC, E_VERSION, E_SEGLIST, E_SEG33, E_RETO, E_PIDE_CLAVE, E_RESULTADO, E_INIT, E_ACTIVO, E_MENU };

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
    { 0, 0, 120, 24, "Ctrl+Alt+Supr", false },
    { 0, 0, 150, 24, "Pantalla completa", false },
    { 0, 0, 100, 24, "Desconectar", false },
    { 0, 0, 100, 24, "Reconectar", false },
    { 0, 0, 100, 24, "Conexiones", false },
};
#define BT_ESCALA 0
#define BT_CAD    1
#define BT_COMPL  2
#define BT_DESC   3
#define BT_RECON  4
#define BT_MENU   5
#define QTD_BT    6

static char conexion_arg[32];
static bool color_dado;
static bool desde_menu;      /* se abrió sin equipo: la ventana de conexión es la pantalla de inicio */

static void conectar_servidor(void);
static void dibujar_menu(void);
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

/* Qué botones de la barra de abajo valen en cada estado (D273: barra de herramientas de la sesión). */
static bool boton_visible(int i)
{
    bool conectando = estado == E_VERSION || estado == E_SEGLIST || estado == E_SEG33 || estado == E_RETO ||
                      estado == E_RESULTADO || estado == E_INIT;
    switch (i) {
    case BT_ESCALA: return estado == E_ACTIVO || estado == E_DESC;
    case BT_CAD: return estado == E_ACTIVO && !solo_ver;
    case BT_COMPL: return estado == E_ACTIVO;
    case BT_DESC: return estado == E_ACTIVO || conectando;
    case BT_RECON: return estado == E_DESC;
    case BT_MENU: return estado == E_DESC && desde_menu;
    }
    return false;
}

static void dibujar_barra(void)
{
    if (completa || estado == E_MENU) return;
    int y = j->alt - BARRA_A;
    g_retangulo(j, 0, y, j->larg, BARRA_A, COR_FUNDO);
    g_retangulo(j, 0, y, j->larg, 1, COR_BORDA);
    int x = 8;
    for (int i = 0; i < QTD_BT; i++) {
        if (!boton_visible(i)) continue;
        bt[i].x = x; bt[i].y = y + 1;
        x += bt[i].l + 8;
        if (i == BT_ESCALA) bt[i].texto = ajustar ? "1:1" : "Ajustar";
        botao_desenhar(j, &bt[i]);
    }
    char t[160];
    if (estado == E_ACTIVO)
        formatar_texto(t, sizeof t, "%dx%d  %d%%%s  %lu KB", dec->fw, dec->fh, zoom / 10, solo_ver ? "  solo ver" : "", recibidos / 1024);
    else if (estado == E_DESC) copia_texto(t, "desconectado", sizeof t);
    else if (estado == E_PIDE_CLAVE) copia_texto(t, "esperando la contraseña", sizeof t);
    else copia_texto(t, "conectando...", sizeof t);
    if (x + 80 < j->larg) g_texto_n(j, x + 4, y + 5, t, (j->larg - x - 12) / LETRA_L, COR_TEXTO_FRACO);
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
    if (estado == E_MENU) { dibujar_menu(); return; }
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

/* ------------------------------------------------------------ ventana de conexión (D273) */

#define CON_MAX   32
#define ARQ_CONF  "/home/.config/vnc.conf"
#define MENU_ANCHO_MIN 720
#define MENU_ALTO_MIN  440

typedef struct {
    char nombre[32], host[64];
    int  puerto, color, calidad;      /* color: 8, 16 o 32; calidad: -1 = sin JPEG, 0 a 9 */
    bool solo_ver, completa, recordar;
    char clave[16];                   /* solo cuenta si recordar */
} con_t;

static con_t cons[CON_MAX];
static int   ncons;
static char  itens_con[CON_MAX][64];
static lista_t lista_con;
static caixa_t cx_nombre, cx_host, cx_puerto, cx_clave;
static casilla_t ck_recordar, ck_solo, ck_compl;
static int   m_color = 32, m_calidad = -1, m_edita = -1;
static char  m_msg[160];
static bool  m_primera = true;
static botao_t bm[8] = {
    { 0, 0, 96, 28, "Conectar", false },      /* 0..2: sobre la lista */
    { 0, 0, 96, 28, "Editar", false },
    { 0, 0, 96, 28, "Borrar", false },
    { 0, 0, 160, 28, "", false },             /* 3: colores */
    { 0, 0, 190, 28, "", false },             /* 4: calidad */
    { 0, 0, 100, 28, "Guardar", false },      /* 5..7: formulario */
    { 0, 0, 100, 28, "Nueva", false },
    { 0, 0, 130, 28, "Conectar", false },
};
#define M_CONECTAR_LISTA 0
#define M_EDITAR  1
#define M_BORRAR  2
#define M_COLOR   3
#define M_CALIDAD 4
#define M_GUARDAR 5
#define M_NUEVA   6
#define M_CONECTAR_FORM 7

/* Clave fija de la ofuscación de las contraseñas guardadas: no es un secreto ni da seguridad (ver la cabecera). */
static const uint8_t CLAVE_OFUSCAR[8] = { 23, 82, 107, 6, 35, 78, 88, 7 };

static void ofuscar_clave(const char *c, char *hexa)
{
    uint8_t ent[8], sal[8];
    memset(ent, 0, sizeof ent);
    for (int i = 0; i < 8 && c[i]; i++) ent[i] = (uint8_t)c[i];
    des_bloque(CLAVE_OFUSCAR, ent, sal, true);
    for (int i = 0; i < 8; i++) { hexa[2 * i] = "0123456789abcdef"[sal[i] >> 4]; hexa[2 * i + 1] = "0123456789abcdef"[sal[i] & 15]; }
    hexa[16] = 0;
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool desofuscar_clave(const char *hexa, char *c, size_t max)
{
    if (tam_texto(hexa) != 16) return false;
    uint8_t ent[8], sal[8];
    for (int i = 0; i < 8; i++) {
        int a = hexval(hexa[2 * i]), b = hexval(hexa[2 * i + 1]);
        if (a < 0 || b < 0) return false;
        ent[i] = (uint8_t)(a << 4 | b);
    }
    des_bloque(CLAVE_OFUSCAR, ent, sal, false);
    size_t n = 0;
    for (int i = 0; i < 8 && sal[i] && n + 1 < max; i++) c[n++] = (char)sal[i];
    c[n] = 0;
    return true;
}

/* Quita lo que rompería el formato de una línea. */
static void limpiar_campo(char *t)
{
    for (; *t; t++) if (*t == '|' || *t == '\n' || *t == '\r') *t = '_';
}

static void con_rotulo(int i, char *dst, size_t max)
{
    formatar_texto(dst, max, "%s  (%s:%d)", cons[i].nombre, cons[i].host, cons[i].puerto);
}

static void lista_reconstruir(int sel)
{
    for (int i = 0; i < ncons; i++) con_rotulo(i, itens_con[i], sizeof itens_con[i]);
    lista_con.qtd = ncons;
    lista_con.sel = sel < ncons ? sel : -1;
}

static void conexiones_leer(void)
{
    ncons = 0;
    int fd = abrir(ARQ_CONF, ABRIR_LER);
    if (fd < 0) return;
    static char t[16 * 1024];
    int n = ler(fd, t, sizeof t - 1);
    fechar(fd);
    if (n <= 0) return;
    t[n] = 0;
    for (char *l = t; *l && ncons < CON_MAX;) {
        char *fin = acha_char(l, '\n');
        if (fin) *fin = 0;
        char *c[8];
        int nc = 0;
        for (char *q = l; nc < 8;) {
            c[nc++] = q;
            q = acha_char(q, '|');
            if (!q) break;
            *q++ = 0;
        }
        if (nc >= 7 && c[0][0] && l[0] != '#') {
            con_t *k = &cons[ncons];
            memset(k, 0, sizeof *k);
            copia_texto(k->nombre, c[0], sizeof k->nombre);
            copia_texto(k->host, c[1], sizeof k->host);
            int v = texto_para_int(c[2]);
            k->puerto = v > 0 && v < 65536 ? v : 5900;
            v = texto_para_int(c[3]);
            k->color = v == 8 || v == 16 ? v : 32;
            v = texto_para_int(c[4]);
            k->calidad = v >= 0 && v <= 9 ? v : -1;
            k->solo_ver = texto_para_int(c[5]) != 0;
            k->completa = texto_para_int(c[6]) != 0;
            if (nc >= 8 && c[7][0] && desofuscar_clave(c[7], k->clave, sizeof k->clave)) k->recordar = true;
            ncons++;
        }
        if (!fin) break;
        l = fin + 1;
    }
}

static int conexiones_guardar(void)
{
    static char t[16 * 1024];
    int n = formatar_texto(t, sizeof t, "# conexiones de «vnc» (D273): nombre|equipo|puerto|colores|calidad|solo_ver|completa|clave\n"
                                          "# la clave, si hay, está ofuscada (no cifrada)\n");
    for (int i = 0; i < ncons && n < (int)sizeof t - 400; i++) {
        char h[20] = "";
        if (cons[i].recordar && cons[i].clave[0]) ofuscar_clave(cons[i].clave, h);
        n += formatar_texto(t + n, sizeof t - (unsigned)n, "%s|%s|%d|%d|%d|%d|%d|%s\n", cons[i].nombre, cons[i].host, cons[i].puerto,
                            cons[i].color, cons[i].calidad, cons[i].solo_ver, cons[i].completa, h);
    }
    criar_pasta("/home/.config");
    int fd = abrir(ARQ_CONF, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    escrever_tudo(fd, t, (unsigned)n);
    fechar(fd);
    return 0;
}

static void texto_calidad(char *d, size_t max, int q)
{
    if (q < 0) copia_texto(d, "Calidad: sin pérdida", max);
    else formatar_texto(d, max, "Calidad JPEG: %d", q);
}

static void form_limpiar(void)
{
    caixa_definir(&cx_nombre, "");
    caixa_definir(&cx_host, "");
    caixa_definir(&cx_puerto, "5900");
    caixa_definir(&cx_clave, "");
    ck_recordar.marcada = ck_solo.marcada = ck_compl.marcada = false;
    m_color = 32; m_calidad = -1; m_edita = -1;
}

static void form_cargar(int i)
{
    caixa_definir(&cx_nombre, cons[i].nombre);
    caixa_definir(&cx_host, cons[i].host);
    char t[16];
    formatar_texto(t, sizeof t, "%d", cons[i].puerto);
    caixa_definir(&cx_puerto, t);
    caixa_definir(&cx_clave, cons[i].recordar ? cons[i].clave : "");
    ck_recordar.marcada = cons[i].recordar;
    ck_solo.marcada = cons[i].solo_ver;
    ck_compl.marcada = cons[i].completa;
    m_color = cons[i].color; m_calidad = cons[i].calidad;
    m_edita = i;
}

/* Lo que hay en el formulario, o false (con el motivo en m_msg) si no sirve. */
static bool form_leer(con_t *k)
{
    memset(k, 0, sizeof *k);
    copia_texto(k->host, cx_host.texto, sizeof k->host);
    copia_texto(k->nombre, cx_nombre.texto, sizeof k->nombre);
    limpiar_campo(k->host);
    limpiar_campo(k->nombre);
    if (!k->host[0]) { copia_texto(m_msg, "Escribe el equipo (nombre o dirección IP).", sizeof m_msg); return false; }
    int v;
    if (!texto_para_numero(cx_puerto.texto, &v) || v < 1 || v > 65535) { copia_texto(m_msg, "El puerto debe ser un número de 1 a 65535.", sizeof m_msg); return false; }
    k->puerto = v;
    k->color = m_color; k->calidad = m_calidad;
    k->solo_ver = ck_solo.marcada; k->completa = ck_compl.marcada; k->recordar = ck_recordar.marcada;
    copia_texto(k->clave, cx_clave.texto, 9);
    if (!k->nombre[0]) formatar_texto(k->nombre, sizeof k->nombre, "%s:%d", k->host, k->puerto);
    return true;
}

static void conectar_con(const con_t *k)
{
    copia_texto(host, k->host, sizeof host);
    puerto = k->puerto;
    color_bits = k->color;
    calidad = k->calidad;
    solo_ver = k->solo_ver;
    clave[0] = 0;
    clave_de_fichero = false;
    tiene_clave = false;
    if (k->clave[0]) { copia_texto(clave, k->clave, sizeof clave); tiene_clave = true; }
    jan_estilo(j, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, 320, 200);
    estado = E_DESC;
    cur_remoto = false;
    if (k->completa) jan_tela_cheia(j, true);
    registrar("vnc: conexión «%s» a %s:%d (colores %d, calidad %d%s%s)\n", k->nombre, host, puerto, color_bits, calidad,
              solo_ver ? ", solo ver" : "", k->completa ? ", pantalla completa" : "");
    conectar_servidor();
}

/* Disposición de la ventana de conexión; la usan el dibujo y los clics. */
static void menu_disponer(void)
{
    int W = j->larg, H = j->alt;
    int lw = 300, x0 = 340, rl = W - x0 - 16;
    lista_con.x = 16; lista_con.y = 64; lista_con.l = lw; lista_con.a = H - 64 - 84;
    for (int i = 0; i < 3; i++) { bm[i].x = 16 + i * 102; bm[i].y = H - 74; }
    cx_nombre.x = x0; cx_nombre.y = 82; cx_nombre.l = rl;
    cx_host.x = x0; cx_host.y = 138; cx_host.l = rl - 110;
    cx_puerto.x = W - 16 - 100; cx_puerto.y = 138; cx_puerto.l = 100;
    cx_clave.x = x0; cx_clave.y = 194; cx_clave.l = rl;
    cx_clave.oculta = true;
    ck_recordar.x = x0; ck_recordar.y = 228;
    bm[M_COLOR].x = x0; bm[M_COLOR].y = 260;
    bm[M_CALIDAD].x = x0 + 170; bm[M_CALIDAD].y = 260;
    ck_solo.x = x0; ck_solo.y = 304;
    ck_compl.x = x0; ck_compl.y = 328;
    bm[M_GUARDAR].x = x0; bm[M_GUARDAR].y = 366;
    bm[M_NUEVA].x = x0 + 110; bm[M_NUEVA].y = 366;
    bm[M_CONECTAR_FORM].x = x0 + 220; bm[M_CONECTAR_FORM].y = 366;
}

static void dibujar_menu(void)
{
    int W = j->larg, H = j->alt, x0 = 340;
    menu_disponer();
    g_retangulo(j, 0, 0, W, H, COR_FUNDO);
    g_texto(j, 16, 14, "Escritorio remoto (VNC)", COR_TEXTO);
    g_texto(j, 17, 14, "Escritorio remoto (VNC)", COR_TEXTO);
    g_texto(j, 16, 40, "Conexiones guardadas", COR_TEXTO_FRACO);
    g_texto(j, x0, 40, "Datos de la conexión", COR_TEXTO_FRACO);
    g_retangulo(j, x0 - 12, 40, 1, H - 100, COR_BORDA);
    lista_desenhar(j, &lista_con);
    if (!ncons) g_texto_n(j, 24, 72, "(ninguna todavía: rellena el formulario y pulsa Guardar)", (lista_con.l - 16) / LETRA_L, COR_TEXTO_FRACO);
    for (int i = 0; i < 3; i++) botao_desenhar(j, &bm[i]);
    g_texto(j, x0, 64, "Nombre de la conexión:", COR_TEXTO);
    caixa_desenhar(j, &cx_nombre);
    g_texto(j, x0, 120, "Equipo (nombre o dirección IP):", COR_TEXTO);
    g_texto(j, cx_puerto.x, 120, "Puerto:", COR_TEXTO);
    caixa_desenhar(j, &cx_host);
    caixa_desenhar(j, &cx_puerto);
    g_texto(j, x0, 176, "Contraseña (si el servidor la pide):", COR_TEXTO);
    caixa_desenhar(j, &cx_clave);
    casilla_desenhar(j, &ck_recordar);
    g_texto(j, x0 + 22, ck_recordar.y - 1, "Recordar la contraseña (se guarda ofuscada)", COR_TEXTO);
    char t[64];
    formatar_texto(t, sizeof t, "Colores: %d bits", m_color);
    bm[M_COLOR].texto = t;
    botao_desenhar(j, &bm[M_COLOR]);
    char q[48];
    texto_calidad(q, sizeof q, m_calidad);
    bm[M_CALIDAD].texto = q;
    botao_desenhar(j, &bm[M_CALIDAD]);
    casilla_desenhar(j, &ck_solo);
    g_texto(j, x0 + 22, ck_solo.y - 1, "Solo ver (sin teclado ni ratón)", COR_TEXTO);
    casilla_desenhar(j, &ck_compl);
    g_texto(j, x0 + 22, ck_compl.y - 1, "Pantalla completa (F11 la quita)", COR_TEXTO);
    for (int i = M_GUARDAR; i <= M_CONECTAR_FORM; i++) botao_desenhar(j, &bm[i]);
    bm[M_COLOR].texto = "";
    bm[M_CALIDAD].texto = "";
    g_texto_n(j, 16, H - 30, m_msg, (W - 32) / LETRA_L, COR_TEXTO_FRACO);
    jan_mostrar(j);
    if (m_primera) {
        m_primera = false;
        registrar("vnc: menu lista=(%d,%d,%d,%d) conectar=(%d,%d) editar=(%d,%d) borrar=(%d,%d) equipo=(%d,%d) puerto=(%d,%d) clave=(%d,%d) "
                  "colores=(%d,%d) guardar=(%d,%d) nueva=(%d,%d) conectar2=(%d,%d) recordar=(%d,%d) solover=(%d,%d)\n",
                  lista_con.x, lista_con.y, lista_con.l, lista_con.a, bm[0].x, bm[0].y, bm[1].x, bm[1].y, bm[2].x, bm[2].y,
                  cx_host.x, cx_host.y, cx_puerto.x, cx_puerto.y, cx_clave.x, cx_clave.y, bm[M_COLOR].x, bm[M_COLOR].y,
                  bm[M_GUARDAR].x, bm[M_GUARDAR].y, bm[M_NUEVA].x, bm[M_NUEVA].y, bm[M_CONECTAR_FORM].x, bm[M_CONECTAR_FORM].y,
                  ck_recordar.x, ck_recordar.y, ck_solo.x, ck_solo.y);
    }
}

static void menu_iniciar(void)
{
    memset(&lista_con, 0, sizeof lista_con);
    lista_con.itens = itens_con;
    lista_con.sel = -1;
    cx_nombre.foco = false; cx_host.foco = true; cx_puerto.foco = cx_clave.foco = false;
    form_limpiar();
    conexiones_leer();
    lista_reconstruir(-1);
    estado = E_MENU;
    jan_estilo(j, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, MENU_ANCHO_MIN, MENU_ALTO_MIN);
    m_primera = true;
    m_msg[0] = 0;
}

static void guardar_formulario(void)
{
    con_t k;
    if (!form_leer(&k)) return;
    int i = m_edita;
    if (i < 0 || i >= ncons || compara_texto(cons[i].nombre, k.nombre) != 0) {
        i = -1;
        for (int n = 0; n < ncons; n++) if (compara_texto(cons[n].nombre, k.nombre) == 0) i = n;
        if (i < 0) {
            if (ncons >= CON_MAX) { copia_texto(m_msg, "Hay demasiadas conexiones guardadas: borra alguna.", sizeof m_msg); return; }
            i = ncons++;
        }
    }
    cons[i] = k;
    if (!k.recordar) cons[i].clave[0] = 0;   /* sin «Recordar» la contraseña no se guarda ni se queda en memoria */
    int r = conexiones_guardar();
    m_edita = i;
    lista_reconstruir(i);
    if (r < 0) formatar_texto(m_msg, sizeof m_msg, "No se pudo guardar en " ARQ_CONF ": %s", nome_erro(r));
    else if (!servicos_montado("/home")) copia_texto(m_msg, "Guardada, pero /home no es un disco: se perderá al apagar.", sizeof m_msg);
    else formatar_texto(m_msg, sizeof m_msg, "Conexión «%s» guardada%s.", k.nombre, k.recordar && k.clave[0] ? " (con la contraseña ofuscada)" : "");
}

static void menu_cambiar_foco(void)
{
    caixa_t *c[4] = { &cx_nombre, &cx_host, &cx_puerto, &cx_clave };
    int a = 1;
    for (int i = 0; i < 4; i++) if (c[i]->foco) a = i;
    for (int i = 0; i < 4; i++) c[i]->foco = false;
    c[(a + 1) % 4]->foco = true;
}

/* Eventos de la ventana de conexión. Devuelve false para salir del programa. */
static bool tratar_menu(evento_t *e)
{
    menu_disponer();
    if (e->tipo == EV_TECLA && e->tecla == TECLA_ESC) return false;
    if (e->tipo == EV_TECLA && e->tecla == '\t') { menu_cambiar_foco(); dibujar_menu(); return true; }
    m_msg[0] = 0;
    caixa_t *cs[4] = { &cx_nombre, &cx_host, &cx_puerto, &cx_clave };
    bool enter = false;
    for (int i = 0; i < 4; i++) if (caixa_tratar(j, cs[i], e)) enter = true;
    casilla_tratar(&ck_recordar, e);
    casilla_tratar(&ck_solo, e);
    casilla_tratar(&ck_compl, e);
    int at = lista_tratar(j, &lista_con, e);
    int ir = -1;      /* conexión guardada por abrir */
    if (at == LISTA_ATIVOU && lista_con.sel >= 0) ir = lista_con.sel;
    if (botao_tratar(j, &bm[M_CONECTAR_LISTA], e)) {
        if (lista_con.sel < 0) copia_texto(m_msg, "Elige una conexión de la lista (o usa el formulario).", sizeof m_msg);
        else ir = lista_con.sel;
    }
    if (botao_tratar(j, &bm[M_EDITAR], e)) {
        if (lista_con.sel < 0) copia_texto(m_msg, "Elige una conexión de la lista para editarla.", sizeof m_msg);
        else { form_cargar(lista_con.sel); formatar_texto(m_msg, sizeof m_msg, "Editando «%s»: cambia lo que quieras y pulsa Guardar.", cons[lista_con.sel].nombre); }
    }
    if (botao_tratar(j, &bm[M_BORRAR], e)) {
        if (lista_con.sel < 0) copia_texto(m_msg, "Elige una conexión de la lista para borrarla.", sizeof m_msg);
        else {
            char t[120];
            formatar_texto(t, sizeof t, "¿Borrar la conexión «%s»?", cons[lista_con.sel].nombre);
            if (confirmar("Borrar conexión", t)) {
                int k = lista_con.sel;
                for (int i = k; i + 1 < ncons; i++) cons[i] = cons[i + 1];
                ncons--;
                conexiones_guardar();
                if (m_edita == k) m_edita = -1;
                else if (m_edita > k) m_edita--;
                lista_reconstruir(-1);
                copia_texto(m_msg, "Conexión borrada.", sizeof m_msg);
            }
        }
    }
    if (botao_tratar(j, &bm[M_COLOR], e)) m_color = m_color == 32 ? 16 : m_color == 16 ? 8 : 32;
    if (botao_tratar(j, &bm[M_CALIDAD], e)) m_calidad = m_calidad < 0 ? 3 : m_calidad < 6 ? 6 : m_calidad < 9 ? 9 : -1;
    if (botao_tratar(j, &bm[M_GUARDAR], e)) guardar_formulario();
    if (botao_tratar(j, &bm[M_NUEVA], e)) { form_limpiar(); cx_host.foco = true; cx_nombre.foco = cx_puerto.foco = cx_clave.foco = false; lista_con.sel = -1; }
    if (enter || botao_tratar(j, &bm[M_CONECTAR_FORM], e)) {
        con_t k;
        if (form_leer(&k)) { conectar_con(&k); return true; }
    }
    if (ir >= 0) { con_t k = cons[ir]; conectar_con(&k); return true; }
    dibujar_menu();
    return true;
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
    if (estado == E_MENU) return tratar_menu(e);
    if (estado == E_PIDE_CLAVE) return tratar_clave(e);
    if (!completa && (e->tipo == EV_CLIQUE || e->tipo == EV_SOLTOU))
        for (int i = 0; i < QTD_BT; i++) {
            if (!boton_visible(i)) continue;
            if (botao_tratar(j, &bt[i], e)) {
                if (i == BT_ESCALA) cambiar_escala();
                else if (i == BT_CAD) ctrl_alt_supr();
                else if (i == BT_COMPL) jan_tela_cheia(j, true);
                else if (i == BT_DESC) {
                    cerrar_red();
                    registrar("vnc: desconectado por la persona\n");
                    if (!desde_menu) return false;
                    menu_iniciar();
                    desenhar_todo();
                } else if (i == BT_MENU) { menu_iniciar(); desenhar_todo(); }
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
    imprimir("uso: vnc                    ventana de conexión, con las conexiones guardadas (" ARQ_CONF ")\n"
             "     vnc --conexion NOMBRE   abre una conexión guardada\n"
             "     vnc [-p puerto] [--solo-ver] [--pantalla-completa] [--color 8|16|32] [--calidad 0-9]\n"
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
        { "clave-fichero", 'k' }, { "uno-a-uno", '1' }, { "puerto", 'p' }, { "conexion", 'n' }, { NULL, 0 }
    };
    opcoes_t o;
    memset(&o, 0, sizeof o);
    o.longas = largas;
    int r;
    while ((r = obter_opcao(argc, argv, "p:c:q:k:n:sf1", &o)) != -1) {
        int v;
        switch (r) {
        case OPC_AJUDA: uso(); return 0;
        case 'p': if (!texto_para_numero(o.valor, &v) || v < 1 || v > 65535) { imprimir_em(ERROS, "vnc: puerto no válido\n"); return 2; } puerto = v; break;
        case 'c': if (!texto_para_numero(o.valor, &v) || (v != 8 && v != 16 && v != 32)) { imprimir_em(ERROS, "vnc: --color admite 8, 16 o 32\n"); return 2; } color_bits = v; color_dado = true; break;
        case 'n': copia_texto(conexion_arg, o.valor, sizeof conexion_arg); break;
        case 'q': if (!texto_para_numero(o.valor, &v) || v > 9) { imprimir_em(ERROS, "vnc: --calidad admite de 0 a 9\n"); return 2; } calidad = v; break;
        case 'k': leer_clave_fichero(o.valor); break;
        case 's': solo_ver = true; break;
        case 'f': quiere_completa = true; break;
        case '1': uno_a_uno_opt = true; break;
        default: return opcao_invalida("vnc", r, &o);
        }
    }
    if (conexion_arg[0]) {
        conexiones_leer();
        int k = -1;
        for (int i = 0; i < ncons; i++) if (compara_texto(cons[i].nombre, conexion_arg) == 0) k = i;
        if (k < 0) { imprimir_em(ERROS, "vnc: no hay una conexión guardada llamada «%s» (%s)\n", conexion_arg, ARQ_CONF); return 2; }
        copia_texto(host, cons[k].host, sizeof host);
        puerto = cons[k].puerto;
        if (!color_dado) color_bits = cons[k].color;
        if (calidad < 0) calidad = cons[k].calidad;
        solo_ver = solo_ver || cons[k].solo_ver;
        quiere_completa = quiere_completa || cons[k].completa;
        if (cons[k].clave[0] && !tiene_clave) { copia_texto(clave, cons[k].clave, sizeof clave); tiene_clave = true; }
    } else if (o.indice >= argc) desde_menu = true;
    else {
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
    if (desde_menu) copia_texto(titulo, "Escritorio remoto (VNC)", sizeof titulo);
    else formatar_texto(titulo, sizeof titulo, "VNC %s:%d", host, puerto);
    j = jan_abrir(titulo, LARG_INICIO, ALT_INICIO);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, 320, 200);
    if (quiere_completa) jan_tela_cheia(j, true);
    registrar("vnc: pronto\n");
    if (desde_menu) { menu_iniciar(); desenhar_todo(); }
    else conectar_servidor();

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
