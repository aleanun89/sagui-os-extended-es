// SPDX-License-Identifier: GPL-3.0-only
/* video.c - framebuffer VESA con doble búfer.
 *
 * La etapa 2 eligió y activó el modo (la resolución preferida de la cabecera
 * o, sin ella, 1024x768, 800x600 o 640x480, con 32, 24, 16 o 15 bits;
 * D058, D078) y
 * dejó en boot_info la dirección física del framebuffer lineal, el ancho de
 * cada línea en bytes (pitch) y la posición de cada color en el píxel. La ventana del
 * kernel para el framebuffer es write-combining cuando la CPU lo permite (wc.c,
 * D057). En la tarjeta de Bochs/QEMU se puede cambiar de modo sin reiniciar
 * (video_trocar_modo, bga.c, D059): la ventana ya cubre toda la memoria de
 * vídeo y solo se rehace el búfer de atrás.
 *
 * Varias pantallas (D290, telas.c): el búfer de atrás es el escritorio entero (larg x alt) y cada
 * salida de vídeo ("cabeza") recibe el rectángulo que le toca. La cabeza 0 es la del arranque
 * (con su formato de píxel, más abajo); las demás las registran sus drivers, siempre en 32 bits.
 * prim_l y prim_a son el modo de la cabeza 0; con una sola pantalla el escritorio es ese modo.
 *
 * Todo se dibuja en un búfer en RAM ("búfer de atrás"), siempre con 32 bits
 * por píxel en formato 0x00RRGGBB, y solo después se copia a la tarjeta. Así:
 *   - quien dibuja no ve la imagen a medias (sin parpadeo);
 *   - el resto del kernel no necesita saber si la tarjeta usa 15, 16, 24 o 32
 *     bits ni el orden de los colores: la conversión ocurre solo en la copia (en 16 y
 *     15 bits, rapido_converter16: en C o con SSE2);
 *   - leer un píxel (para desplazar el texto, por ejemplo) es leer RAM, y no la
 *     memoria de la tarjeta, que es muy lenta para leer.
 * La copia se hace solo del rectángulo que cambió desde la última vez, con
 * rep movsd o SSE2, lo que eligió la medida del arranque (rapido.c, D042). */
#include "video.h"
#include "bootinfo.h"
#include "console.h"
#include "memoria.h"
#include "paginacao.h"
#include "rapido.h"
#include "texto.h"
#include "wc.h"
#include "bga.h"
#include "info_hardware.h"
#include "acel2d.h"
#include "cpu.h"
#include "telas.h"

static bool ativo;
static uint32_t larg, alt, pitch, bpp, bytes_px;   /* larg y alt: el escritorio; pitch, bpp y bytes_px: la cabeza 0 (bpp = profundidad, 15 ocupa 2 bytes) */
static uint32_t prim_l, prim_a;   /* modo de la cabeza 0 */
static uint8_t *tela;          /* framebuffer de la tarjeta (ventana del kernel) */
static uint32_t *tras;         /* búfer de atrás */
static uint8_t pos_r, pos_g, pos_b;
static bool formato_direto;    /* 32 bits con R en 16, G en 8, B en 0: copia sin convertir */
static uint8_t tam_r, tam_g, tam_b;   /* bits de cada color en la tarjeta */
static int conv16;             /* 0 = genérico; 1 = RGB565 por defecto; 2 = RGB555 por defecto (rapido_converter16) */
static int suj_x0, suj_y0, suj_x1, suj_y1;   /* rectángulo alterado; vacío si x0 >= x1 */
static uint32_t fb_fisico, fb_mapeado;        /* framebuffer: dirección física y bytes en la ventana del kernel */

/* Cursor del mouse (H19): flecha 12x19, '#' contorno, '.' interior. Dibujado aquí. */
static int cur_x, cur_y, cur_ant_x, cur_ant_y;
static bool cur_visivel, cur_ant_visivel;
/* D230: donde el cursor no se dibuja (la punta de la flecha dentro = oculto). */
static int zona_x0, zona_y0, zona_x1, zona_y1;
#define CUR_L 12
#define CUR_A 19
static const char *const seta[CUR_A] = {
    "#           ", "##          ", "#.#         ", "#..#        ", "#...#       ",
    "#....#      ", "#.....#     ", "#......#    ", "#.......#   ", "#........#  ",
    "#.........# ", "#......#####", "#...#..#    ", "#..# #..#   ", "#.#  #..#   ",
    "##    #..#  ", "#     #..#  ", "       ##   ", "            ",
};

bool video_ativo(void)      { return ativo; }
uint32_t video_largura(void) { return larg; }
uint32_t video_altura(void)  { return alt; }
uint32_t *video_buffer(void) { return tras; }
uint8_t *video_placa(void)   { return tela; }
uint32_t video_pitch(void)   { return pitch; }
bool video_formato_direto(void) { return formato_direto; }
uint32_t video_bpp(void) { return bpp; }
uint32_t video_prim_largura(void) { return prim_l; }
uint32_t video_prim_altura(void) { return prim_a; }

/* Formato de la tarjeta a partir de boot_info. El VBE 1.2 no informa las máscaras
 * (quedan a cero): valen las de siempre, 8-8-8, 5-6-5 o 5-5-5. */
static void ler_formato(void)
{
    tam_r = info_boot.verm_tam; tam_g = info_boot.verde_tam; tam_b = info_boot.azul_tam;
    pos_r = info_boot.verm_pos; pos_g = info_boot.verde_pos; pos_b = info_boot.azul_pos;
    if (!tam_r || !tam_g || !tam_b || tam_r > 8 || tam_g > 8 || tam_b > 8) {
        if (bpp >= 24)      { tam_r = tam_g = tam_b = 8; pos_r = 16; pos_g = 8; pos_b = 0; }
        else if (bpp == 16) { tam_r = 5; tam_g = 6; tam_b = 5; pos_r = 11; pos_g = 5; pos_b = 0; }
        else                { tam_r = tam_g = tam_b = 5; pos_r = 10; pos_g = 5; pos_b = 0; }
        info_boot.verm_tam = tam_r; info_boot.verde_tam = tam_g; info_boot.azul_tam = tam_b;
        info_boot.verm_pos = pos_r; info_boot.verde_pos = pos_g; info_boot.azul_pos = pos_b;
    }
    bytes_px = (bpp + 7) / 8;
    formato_direto = bpp == 32 && pos_r == 16 && pos_g == 8 && pos_b == 0;
    conv16 = 0;
    if (bpp == 16 && tam_r == 5 && tam_g == 6 && tam_b == 5 && pos_r == 11 && pos_g == 5 && pos_b == 0) conv16 = 1;
    if (bpp == 15 && tam_r == 5 && tam_g == 5 && tam_b == 5 && pos_r == 10 && pos_g == 5 && pos_b == 0) conv16 = 2;
}

/* Un píxel 0x00RRGGBB en el formato de la tarjeta: cada color pierde los bits bajos
 * que no caben (en 16 bits, 3 del rojo, 2 del verde y 3 del azul). */
static inline uint32_t na_placa(uint32_t c)
{
    return ((((c >> 16) & 255) >> (8 - tam_r)) << pos_r) | ((((c >> 8) & 255) >> (8 - tam_g)) << pos_g) |
           (((c & 255) >> (8 - tam_b)) << pos_b);
}

/* D305: lo mismo para la capa del 2D por la GPU, que rellena directamente en la tarjeta. */
uint32_t video_cor_placa(cor_t c) { return na_placa(c); }

bool video_iniciar(void)
{
    uint32_t p = info_boot.vesa_bpp;
    if (!info_boot.vesa_ativo || (p != 32 && p != 24 && p != 16 && p != 15))
        return false;
    larg = prim_l = info_boot.vesa_largura;
    alt = prim_a = info_boot.vesa_altura;
    pitch = info_boot.vesa_pitch;
    bpp = p;
    ler_formato();
    if (!larg || !alt || pitch < larg * bytes_px) return false;
    /* Con la tarjeta de Bochs/QEMU, la ventana cubre toda la memoria de vídeo:
     * cambiar de modo después no necesita remapear (ni rehacer el WC). */
    fb_fisico = info_boot.vesa_fb;
    fb_mapeado = pitch * alt;
    if (bga_detectar(hw_video_memoria_kb() * 1024) && bga_memoria() > fb_mapeado) fb_mapeado = bga_memoria();
    tela = (uint8_t *)wc_mapear(fb_fisico, fb_mapeado);
    tras = kmalloc(larg * alt * 4);
    if (!tela || !tras) return false;
    memset(tras, 0, larg * alt * 4);
    ativo = true;
    suj_x0 = suj_y0 = 0; suj_x1 = (int)larg; suj_y1 = (int)alt;
    klog("[VIDEO] %ux%u, %u bits, pitch %u, framebuffer %x, cores R%u:%u G%u:%u B%u:%u\n",
         larg, alt, bpp, pitch, info_boot.vesa_fb, tam_r, pos_r, tam_g, pos_g, tam_b, pos_b);
    return true;
}

void video_marcar(int x, int y, int l, int a)
{
    if (l <= 0 || a <= 0) return;
    int x1 = x + l, y1 = y + a;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > (int)larg) x1 = (int)larg;
    if (y1 > (int)alt) y1 = (int)alt;
    if (x >= x1 || y >= y1) return;
    if (suj_x0 >= suj_x1) { suj_x0 = x; suj_y0 = y; suj_x1 = x1; suj_y1 = y1; return; }
    if (x < suj_x0) suj_x0 = x;
    if (y < suj_y0) suj_y0 = y;
    if (x1 > suj_x1) suj_x1 = x1;
    if (y1 > suj_y1) suj_y1 = y1;
}

void video_pixel(int x, int y, cor_t c)
{
    if ((uint32_t)x >= larg || (uint32_t)y >= alt) return;
    tras[(uint32_t)y * larg + (uint32_t)x] = c;
    video_marcar(x, y, 1, 1);
}

void video_retangulo(int x, int y, int l, int a, cor_t c)
{
    int x1 = x + l, y1 = y + a;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > (int)larg) x1 = (int)larg;
    if (y1 > (int)alt) y1 = (int)alt;
    if (x >= x1 || y >= y1) return;
    acel2d_contar_sw(ACEL_SW_RELLENO, (uint32_t)((x1 - x) * (y1 - y)));   /* D346 */
    bool sse = (x1 - x) * (y1 - y) >= 4096 && rapido_inicio();
    for (int j = y; j < y1; j++)
        rapido_preencher32(tras + (uint32_t)j * larg + (uint32_t)x, c, (size_t)(x1 - x));
    rapido_fim(sse);
    video_marcar(x, y, x1 - x, y1 - y);
}

/* Línea entre dos puntos: avanza siempre un píxel en el eje más largo y
 * acumula el "error" del otro eje, solo con enteros. */
void video_linha(int x0, int y0, int x1, int y1, cor_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1, sy = y0 < y1 ? 1 : -1;
    int passos = dx > dy ? dx : dy;
    int acum = 0;
    for (int k = 0; k <= passos; k++) {
        video_pixel(x0, y0, c);
        if (dx >= dy) {
            x0 += sx;
            acum += dy;
            if (2 * acum >= dx) { y0 += sy; acum -= dx; }
        } else {
            y0 += sy;
            acum += dx;
            if (2 * acum >= dy) { x0 += sx; acum -= dy; }
        }
    }
}

void video_moldura(int x, int y, int l, int a, int e, cor_t c)
{
    video_retangulo(x, y, l, e, c);
    video_retangulo(x, y + a - e, l, e, c);
    video_retangulo(x, y, e, a, c);
    video_retangulo(x + l - e, y, e, a, c);
}

void video_caixa(int x, int y, int l, int a, cor_t fundo, cor_t borda)
{
    video_retangulo(x, y, l, a, fundo);
    video_moldura(x, y, l, a, 1, borda);
}

void video_degrade_vertical(int x, int y, int l, int a, cor_t topo, cor_t base)
{
    for (int j = 0; j < a; j++) {
        /* Interpolación de cada canal en enteros: tope + (base - tope) * j / a. */
        int r = (int)((topo >> 16) & 255) + (((int)((base >> 16) & 255) - (int)((topo >> 16) & 255)) * j) / a;
        int g = (int)((topo >> 8) & 255) + (((int)((base >> 8) & 255) - (int)((topo >> 8) & 255)) * j) / a;
        int b = (int)(topo & 255) + (((int)(base & 255) - (int)(topo & 255)) * j) / a;
        video_retangulo(x, y + j, l, 1, RGB(r, g, b));
    }
}

/* D195: después de que la GPU movió o rellenó la región (x, y, l, a) de la
 * pantalla, el cursor (dibujado solo en la pantalla, no en el búfer de atrás)
 * pudo quedar pisado o copiado (desplazado en dx, dy): esas zonas se marcan para
 * que video_mostrar las rehaga desde el búfer de atrás y vuelva a dibujarlo. */
static void cursor_depois_da_gpu(int x, int y, int l, int a, int dx, int dy)
{
    if (!cur_ant_visivel) return;
    int cx = cur_ant_x, cy = cur_ant_y;
    bool no_destino = cx < x + l && cx + CUR_L > x && cy < y + a && cy + CUR_A > y;
    bool na_origem = cx < x - dx + l && cx + CUR_L > x - dx && cy < y - dy + a && cy + CUR_A > y - dy;
    if (no_destino || na_origem) {
        video_marcar(cx, cy, CUR_L, CUR_A);
        video_marcar(cx + dx, cy + dy, CUR_L, CUR_A);
    }
}

/* Mueve las filas del búfer de atrás como memmove (con solape). Filas distintas
 * no se pisan dentro de la fila: se copian con rapido_copiar (rep movsd o SSE2) en
 * el orden que no pisa lo que falta. En la misma fila hacia la derecha, de atrás
 * hacia delante de 4 en 4 bytes (el memmove del kernel lo haría byte a byte). */
static void mover_tras(int ox, int oy, int dx, int dy, int l, int a)
{
    if (dy != oy) {
        bool sse = rapido_inicio();
        if (dy > oy)
            for (int j = a - 1; j >= 0; j--)
                rapido_copiar(tras + (uint32_t)(dy + j) * larg + (uint32_t)dx, tras + (uint32_t)(oy + j) * larg + (uint32_t)ox, (uint32_t)l * 4);
        else
            for (int j = 0; j < a; j++)
                rapido_copiar(tras + (uint32_t)(dy + j) * larg + (uint32_t)dx, tras + (uint32_t)(oy + j) * larg + (uint32_t)ox, (uint32_t)l * 4);
        rapido_fim(sse);
        return;
    }
    for (int j = 0; j < a; j++) {
        uint32_t *d = tras + (uint32_t)(oy + j) * larg + (uint32_t)dx, *o = tras + (uint32_t)(oy + j) * larg + (uint32_t)ox;
        if (dx <= ox) { memmove(d, o, (uint32_t)l * 4); continue; }
        void *dp = d + l - 1;
        const void *op = o + l - 1;
        uint32_t n = (uint32_t)l;
        __asm__ volatile("std; rep movsl; cld" : "+D"(dp), "+S"(op), "+c"(n) : : "memory");
    }
}

/* D195: mueve el rectángulo (x, y, l, a) en (ddx, ddy). En el búfer de atrás lo
 * hace la CPU; con el acelerador 2D, la pantalla la mueve la GPU y no hace falta
 * volver a copiarla desde la RAM. Sin él (o si falla), el destino queda marcado
 * para la copia de la CPU. Lo que se recorta fuera de la pantalla no se mueve.
 * Devuelve true si la GPU dejó la pantalla hecha. */
bool video_mover(int x, int y, int l, int a, int ddx, int ddy)
{
    if (!ativo) return false;
    int ox = x, oy = y, dx = x + ddx, dy = y + ddy, d;
    if (ox < 0) { d = -ox; ox += d; dx += d; l -= d; }
    if (oy < 0) { d = -oy; oy += d; dy += d; a -= d; }
    if (dx < 0) { d = -dx; ox += d; dx += d; l -= d; }
    if (dy < 0) { d = -dy; oy += d; dy += d; a -= d; }
    if (ox + l > (int)larg) l = (int)larg - ox;
    if (dx + l > (int)larg) l = (int)larg - dx;
    if (oy + a > (int)alt) a = (int)alt - oy;
    if (dy + a > (int)alt) a = (int)alt - dy;
    if (l <= 0 || a <= 0) return true;
    bool sim = int_salva_desliga();
    bool gpu = acel2d_ativo();
    if (gpu) video_mostrar();   /* la pantalla igual al búfer de atrás antes de mover en ella */
    acel_sincronizar();         /* D387: y la GPU sin leer el búfer de atrás, que va a cambiar */
    mover_tras(ox, oy, dx, dy, l, a);
    if (gpu) gpu = acel_copiar(ox, oy, dx, dy, l, a) && acel_esperar();
    if (gpu) cursor_depois_da_gpu(dx, dy, l, a, ddx, ddy);
    else { acel2d_contar_sw(ACEL_SW_MOVER, (uint32_t)(l * a)); video_marcar(dx, dy, l, a); }
    int_restaura(sim);
    return gpu;
}

void video_rolar(int y, int altura, int linhas, cor_t fundo)
{
    if (linhas <= 0 || linhas >= altura) { video_retangulo(0, y, (int)larg, altura, fundo); return; }
    if (acel2d_ativo()) {   /* D195: la GPU sube la franja y borra la línea nueva en la pantalla */
        bool sim = int_salva_desliga();
        video_mostrar();
        acel_sincronizar();   /* D387: la GPU sin leer el búfer de atrás, que va a cambiar */
        mover_tras(0, y + linhas, 0, y, (int)larg, altura - linhas);
        for (int j = y + altura - linhas; j < y + altura; j++)
            rapido_preencher32(tras + (uint32_t)j * larg, fundo, larg);
        bool gpu = acel_copiar(0, y + linhas, 0, y, (int)larg, altura - linhas) &&
                   acel_rellenar(0, y + altura - linhas, (int)larg, linhas, fundo) && acel_esperar();
        if (gpu) cursor_depois_da_gpu(0, y, (int)larg, altura, 0, -linhas);
        else { acel2d_contar_sw(ACEL_SW_ROLAR, larg * (uint32_t)altura); video_marcar(0, y, (int)larg, altura); }
        int_restaura(sim);
        return;
    }
    acel2d_contar_sw(ACEL_SW_ROLAR, larg * (uint32_t)altura);   /* D346 */
    memmove(tras + (uint32_t)y * larg, tras + (uint32_t)(y + linhas) * larg, (uint32_t)(altura - linhas) * larg * 4);
    video_retangulo(0, y + altura - linhas, (int)larg, linhas, fundo);
    video_marcar(0, y, (int)larg, altura);
}

/* Copia una franja de píxeles del búfer de atrás a la cabeza 0, convirtiendo. (ox, oy) es donde
 * empieza esa cabeza dentro del escritorio. */
static void copiar_linha(int y, int x0, int x1, int ox, int oy)
{
    uint32_t *o = tras + (uint32_t)y * larg + (uint32_t)x0;
    uint8_t *d = tela + (uint32_t)(y - oy) * pitch + (uint32_t)(x0 - ox) * bytes_px;
    int n = x1 - x0;
    if (formato_direto) { rapido_copiar_tela(d, o, (uint32_t)n * 4); return; }
    if (conv16) { rapido_converter16((uint16_t *)d, o, (uint32_t)n, conv16 == 2); return; }
    for (int i = 0; i < n; i++) {
        uint32_t v = na_placa(o[i]);
        if (bytes_px == 4) { *(uint32_t *)d = v; d += 4; }
        else if (bytes_px == 3) { d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); d[2] = (uint8_t)(v >> 16); d += 3; }
        else { *(uint16_t *)d = (uint16_t)v; d += 2; }
    }
}

/* El rectángulo (x0, y0)-(x1, y1) del escritorio a todas las cabezas que lo muestran. Con una
 * sola pantalla es la copia de siempre. */
static void copiar_regiao(int x0, int y0, int x1, int y1)
{
    int nc = telas_qtd();
    if (nc <= 1) {
        for (int y = y0; y < y1; y++) copiar_linha(y, x0, x1, 0, 0);
        return;
    }
    for (int i = 0; i < nc; i++) {
        int hx, hy, hl, ha;
        telas_rect(i, &hx, &hy, &hl, &ha);
        int ax = x0 > hx ? x0 : hx, ay = y0 > hy ? y0 : hy;
        int bx = x1 < hx + hl ? x1 : hx + hl, by = y1 < hy + ha ? y1 : hy + ha;
        if (ax >= bx || ay >= by) continue;
        if (i == 0) {
            for (int y = ay; y < by; y++) copiar_linha(y, ax, bx, hx, hy);
        } else {
            const saida_t *sa = telas_saida(i);
            for (int y = ay; y < by; y++)
                rapido_copiar_tela(sa->fb + (uint32_t)(y - hy) * sa->pitch + (uint32_t)(ax - hx) * 4,
                                   tras + (uint32_t)y * larg + (uint32_t)ax, (uint32_t)(bx - ax) * 4);
        }
    }
}

/* Un píxel del escritorio en la pantalla (el cursor se dibuja solo ahí, no en el búfer de atrás),
 * en todas las cabezas que lo muestran. */
static void escrever_pixel_tela(int x, int y, uint32_t c)
{
    if ((uint32_t)x >= larg || (uint32_t)y >= alt) return;
    int nc = telas_qtd();
    for (int i = 0; i < (nc > 0 ? nc : 1); i++) {
        int hx = 0, hy = 0, hl = (int)larg, ha = (int)alt;
        if (nc > 1) telas_rect(i, &hx, &hy, &hl, &ha);
        if (x < hx || y < hy || x >= hx + hl || y >= hy + ha) continue;
        if (i >= 1) {
            const saida_t *sa = telas_saida(i);
            *(uint32_t *)(sa->fb + (uint32_t)(y - hy) * sa->pitch + (uint32_t)(x - hx) * 4) = c;
            continue;
        }
        uint8_t *d = tela + (uint32_t)(y - hy) * pitch + (uint32_t)(x - hx) * bytes_px;
        uint32_t v = formato_direto ? c : na_placa(c);
        if (bytes_px == 4) *(uint32_t *)d = v;
        else if (bytes_px == 3) { d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); d[2] = (uint8_t)(v >> 16); }
        else *(uint16_t *)d = (uint16_t)v;
    }
}

static void desenhar_cursor(int x, int y)
{
    for (int j = 0; j < CUR_A; j++)
        for (int i = 0; i < CUR_L; i++) {
            char p = seta[j][i];
            if (p == '#') escrever_pixel_tela(x + i, y + j, 0x000000);
            else if (p == '.') escrever_pixel_tela(x + i, y + j, 0xFFFFFF);
        }
}

void video_mostrar(void)
{
    if (!ativo) return;
    acel_sincronizar();   /* D246: la CPU no escribe en la pantalla mientras el motor DMA aún copia */
    /* D230: sobre el contenido de una ventana con JAN_SEM_CURSOR, la flecha no se ve. */
    bool visivel = cur_visivel && !(cur_x >= zona_x0 && cur_x < zona_x1 && cur_y >= zona_y0 && cur_y < zona_y1);
    /* Donde estaba el cursor (o va a estar) también necesita rehacerse. */
    if (cur_ant_visivel && (cur_ant_x != cur_x || cur_ant_y != cur_y || !visivel))
        video_marcar(cur_ant_x, cur_ant_y, CUR_L, CUR_A);
    if (suj_x0 >= suj_x1 && cur_ant_visivel == visivel && cur_ant_x == cur_x && cur_ant_y == cur_y)
        return;
    bool toca_cursor = false, pela_gpu = false;
    if (suj_x0 < suj_x1) {
        /* D387: con una sola pantalla, la GPU puede copiar el rectángulo leyendo el búfer de atrás
         * (GART): la CPU solo pone la orden y sigue. Si no (o si es pequeño), la copia de siempre. */
        pela_gpu = telas_qtd() <= 1 && acel_volcar(suj_x0, suj_y0, suj_x1 - suj_x0, suj_y1 - suj_y0);
        if (!pela_gpu) {
            bool sse = rapido_inicio();
            copiar_regiao(suj_x0, suj_y0, suj_x1, suj_y1);
            acel2d_contar_sw(ACEL_SW_MOSTRAR, (uint32_t)((suj_x1 - suj_x0) * (suj_y1 - suj_y0)));   /* D346 */
            rapido_fim(sse);
            acel_atualizar(suj_x0, suj_y0, suj_x1 - suj_x0, suj_y1 - suj_y0);   /* D195: VMware en modo SVGA */
        }
        toca_cursor = cur_x < suj_x1 && cur_x + CUR_L > suj_x0 && cur_y < suj_y1 && cur_y + CUR_A > suj_y0;
    }
    if (visivel && (toca_cursor || cur_ant_x != cur_x || cur_ant_y != cur_y || !cur_ant_visivel)) {
        if (pela_gpu) acel_sincronizar();   /* D387: el cursor va encima de lo que la GPU está copiando */
        desenhar_cursor(cur_x, cur_y);
        acel_atualizar(cur_x, cur_y, CUR_L, CUR_A);
    }
    cur_ant_x = cur_x; cur_ant_y = cur_y; cur_ant_visivel = visivel;
    suj_x0 = suj_x1 = 0;
}

/* D387: la copia de la CPU de un rectángulo del escritorio a la pantalla, sin tocar lo marcado
 * (para medirla contra la de la GPU: «acel2d volcado medir»). */
void video_volcar_cpu(int x0, int y0, int x1, int y1)
{
    if (!ativo) return;
    bool sse = rapido_inicio();
    copiar_regiao(x0, y0, x1, y1);
    rapido_fim(sse);
    acel_atualizar(x0, y0, x1 - x0, y1 - y0);
}

void video_mostrar_tudo(void)
{
    video_marcar(0, 0, (int)larg, (int)alt);
    video_mostrar();
}

void video_cursor(int x, int y, bool visivel)
{
    cur_x = x; cur_y = y; cur_visivel = visivel;
}

bool video_zona_sem_cursor(int x, int y, int l, int a)
{
    if (l <= 0 || a <= 0) x = y = l = a = 0;
    if (zona_x0 == x && zona_y0 == y && zona_x1 == x + l && zona_y1 == y + a) return false;
    zona_x0 = x; zona_y0 = y; zona_x1 = x + l; zona_y1 = y + a;
    return true;
}

/* Cambio de modo sin reiniciar (D059): solo en la tarjeta de Bochs/QEMU. El búfer
 * de atrás nuevo se pide antes de tocar la tarjeta; si falta memoria o la
 * tarjeta lo rechaza, nada cambia. Quien llama rehace el resto (terminal, área de
 * trabajo, mouse) y corre con las interrupciones desactivadas, porque la IRQ del
 * mouse también dibuja (video_mostrar). */
bool video_trocar_modo(uint32_t l, uint32_t a)
{
    if (!ativo || !bga_presente()) return false;
    if (l == prim_l && a == prim_a) return true;
    uint32_t novo_bpp = video_bpp_a_quente();
    if (l * a * 4 > fb_mapeado) return false;
    if (acel2d_ativo()) acel2d_desligar("cambio de modo de video");   /* D195: el acelerador conoce el modo de antes */
    acel_volcado_buffer(true);   /* D387: la GPU (o la simulada) deja de leer el búfer de atrás viejo */
    uint32_t dl, da;
    telas_desktop(0, l, a, &dl, &da);   /* D290: con varias pantallas el escritorio es más que este modo */
    uint32_t *novo = kmalloc(dl * da * 4);
    if (!novo) return false;
    uint32_t p = bga_mudar(l, a, novo_bpp, prim_l, prim_a, bpp);
    if (!p || p * a > fb_mapeado) { kfree(novo); return false; }
    kfree(tras);
    tras = novo;
    memset(tras, 0, dl * da * 4);
    prim_l = l; prim_a = a; larg = dl; alt = da; pitch = p; bpp = novo_bpp;
    acel_volcado_buffer(false);   /* D387: el nuevo, a la GART (la simulada; la de verdad se apagó con el 2D) */
    telas_cabeca0(l, a);
    /* Quien lee boot_info (INFO_VERSAO, infopc) pasa a ver el modo nuevo.
     * La tarjeta de Bochs/QEMU usa siempre el formato por defecto de cada profundidad. */
    info_boot.vesa_largura = l;
    info_boot.vesa_altura = a;
    info_boot.vesa_pitch = p;
    info_boot.vesa_bpp = novo_bpp;
    info_boot.verm_tam = info_boot.verde_tam = info_boot.azul_tam = 0;   /* ler_formato pone el valor por defecto */
    ler_formato();
    if (cur_x >= (int)dl) cur_x = (int)dl - 1;
    if (cur_y >= (int)da) cur_y = (int)da - 1;
    cur_ant_visivel = false;
    suj_x0 = suj_y0 = 0; suj_x1 = (int)dl; suj_y1 = (int)da;
    klog("[VIDEO] modo trocado: %ux%u, %u bits, pitch %u (escritorio %ux%u)\n", l, a, bpp, p, dl, da);
    return true;
}

/* D290: el escritorio cambió de forma (otra disposición, otro modo de una salida extra, espejo):
 * el búfer de atrás pasa al tamaño nuevo, las salidas se borran y todo se vuelve a copiar. Quien
 * llama (telas.c) rehace el resto, con las interrupciones desactivadas. false = sin memoria y todo igual. */
bool video_refazer(void)
{
    if (!ativo) return false;
    uint32_t dl, da;
    telas_desktop(-1, 0, 0, &dl, &da);
    if (telas_qtd() > 1 && acel2d_ativo()) acel2d_desligar("varias pantallas");   /* el acelerador solo conoce la cabeza 0 */
    if (dl != larg || da != alt) {
        uint32_t *novo = kmalloc(dl * da * 4);
        if (!novo) return false;
        acel_volcado_buffer(true);   /* D387 */
        kfree(tras);
        tras = novo;
        larg = dl; alt = da;
        acel_volcado_buffer(false);
    }
    memset(tras, 0, larg * alt * 4);
    acel_sincronizar();
    memset(tela, 0, pitch * prim_a);
    for (int i = 1; i < telas_qtd(); i++) {
        const saida_t *sa = telas_saida(i);
        if (sa) memset(sa->fb, 0, sa->pitch * sa->altura);
    }
    if (cur_x >= (int)larg) cur_x = (int)larg - 1;
    if (cur_y >= (int)alt) cur_y = (int)alt - 1;
    cur_ant_visivel = false;
    suj_x0 = suj_y0 = 0; suj_x1 = (int)larg; suj_y1 = (int)alt;
    return true;
}

/* En 15 y 16 bits el cambio en caliente se queda en la misma profundidad: quien
 * eligió 16 bits (tarjeta lenta, poca memoria) sigue con ellos. */
uint32_t video_bpp_a_quente(void) { return bpp == 15 || bpp == 16 ? bpp : 32; }

bool video_troca_a_quente(void) { return ativo && bga_presente(); }
uint32_t video_memoria_mapeada(void) { return fb_mapeado; }
