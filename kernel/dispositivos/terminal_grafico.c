// SPDX-License-Identifier: GPL-3.0-only
/* terminal_grafico.c - el terminal dibujado en modo gráfico con la fuente propia (H18).
 *
 * La pantalla se convierte en una cuadrícula de celdas de 8x16 píxeles (128x48 en 1024x768; la
 * cuadrícula se rehace cuando cambia el modo de vídeo, D059).
 * Cada celda guarda el carácter y los colores, para que el cursor pueda ser
 * dibujado y borrado sin perder lo que estaba debajo. Desplazar la pantalla es mover
 * los píxeles una línea de texto hacia arriba en el búfer de atrás.
 *
 * Además de los caracteres comunes, '\n', '\r', '\t' y '\b' (vuelve y borra), el
 * terminal entiende tres secuencias propias, usadas por el shell para editar
 * a mitad de la línea (flechas laterales):
 *   ESC [ D   cursor una posición a la izquierda (sin borrar)
 *   ESC [ C   cursor una posición a la derecha
 *   ESC [ K   borra desde el cursor hasta el fin de la línea */
#include "console.h"
#include "texto.h"
#include "memoria.h"
#include "video.h"

extern const unsigned char fonte_sagui[256][16];

#define CEL_L 8
#define CEL_A 16

/* Los 16 colores de la consola (mismos índices del modo texto), en tonos propios. */
static const cor_t paleta[16] = {
    RGB(18, 22, 30),    RGB(40, 80, 200),   RGB(40, 160, 70),   RGB(40, 160, 170),
    RGB(200, 45, 45),   RGB(160, 60, 170),  RGB(180, 120, 30),  RGB(205, 210, 215),
    RGB(110, 115, 125), RGB(100, 150, 255), RGB(110, 230, 120), RGB(110, 230, 230),
    RGB(255, 110, 110), RGB(240, 120, 240), RGB(255, 220, 60),  RGB(255, 255, 255),
};

typedef struct { uint8_t c, cor; } celula_t;

static celula_t *grade;
static int colunas, linhas, col, lin;
static uint8_t cor_atual = (COR_PRETO << 4) | COR_CINZA;
static int estado_esc;   /* 0 normal, 1 vio ESC, 2 vio ESC [ */

static void desenhar_celula(int x, int y, bool cursor)
{
    celula_t *k = &grade[y * colunas + x];
    cor_t frente = paleta[k->cor & 15], fundo = paleta[k->cor >> 4];
    if (cursor) { cor_t t = frente; frente = fundo; fundo = t; }   /* cursor = colores intercambiados */
    uint32_t *buf = video_buffer();
    uint32_t largura = video_largura();
    const unsigned char *g = fonte_sagui[k->c];
    for (int j = 0; j < CEL_A; j++) {
        uint32_t *p = buf + (uint32_t)(y * CEL_A + j) * largura + (uint32_t)(x * CEL_L);
        unsigned char bits = g[j];
        for (int i = 0; i < CEL_L; i++)
            p[i] = (bits & (0x80 >> i)) ? frente : fundo;
    }
    video_marcar(x * CEL_L, y * CEL_A, CEL_L, CEL_A);
}

static void mostrar_cursor(bool sim)
{
    if (col < colunas && lin < linhas)
        desenhar_celula(col, lin, sim);
}

bool termgraf_iniciar(void)
{
    if (!video_iniciar()) return false;
    colunas = (int)video_largura() / CEL_L;
    linhas = (int)video_altura() / CEL_A;
    grade = kmalloc((uint32_t)(colunas * linhas) * sizeof(celula_t));
    if (!grade) return false;
    for (int i = 0; i < colunas * linhas; i++) { grade[i].c = ' '; grade[i].cor = cor_atual; }
    video_retangulo(0, 0, (int)video_largura(), (int)video_altura(), paleta[COR_PRETO]);
    col = lin = 0;
    mostrar_cursor(true);
    video_mostrar_tudo();
    return true;
}

int termgraf_colunas(void) { return colunas; }
int termgraf_coluna(void)  { return col; }

void termgraf_cor(uint8_t frente, uint8_t fundo)
{
    cor_atual = (uint8_t)((fundo << 4) | (frente & 15));
}

void termgraf_limpar(void)
{
    for (int i = 0; i < colunas * linhas; i++) { grade[i].c = ' '; grade[i].cor = cor_atual; }
    video_retangulo(0, 0, colunas * CEL_L, linhas * CEL_A, paleta[cor_atual >> 4]);
    col = lin = 0;
    mostrar_cursor(true);
}

static void rolar(void)
{
    memmove(grade, grade + colunas, (uint32_t)(colunas * (linhas - 1)) * sizeof(celula_t));
    for (int i = 0; i < colunas; i++) {
        grade[(linhas - 1) * colunas + i].c = ' ';
        grade[(linhas - 1) * colunas + i].cor = cor_atual;
    }
    video_rolar(0, linhas * CEL_A, CEL_A, paleta[cor_atual >> 4]);
    lin = linhas - 1;
}

static void por(uint8_t c)
{
    grade[lin * colunas + col].c = c;
    grade[lin * colunas + col].cor = cor_atual;
    desenhar_celula(col, lin, false);
}

void termgraf_char(char ch)
{
    uint8_t c = (uint8_t)ch;
    mostrar_cursor(false);
    if (estado_esc == 1) {
        estado_esc = c == '[' ? 2 : 0;
    } else if (estado_esc == 2) {
        /* D317: los parámetros (ESC [ 1 ; 32 m, ESC [ 18 t...) se saltan hasta la letra
         * final; antes se veían en pantalla. Solo se atienden las letras de siempre. */
        if ((c >= '0' && c <= '9') || c == ';' || c == '?') { mostrar_cursor(true); return; }
        estado_esc = 0;
        if (c == 'D' && col > 0) col--;
        else if (c == 'C' && col < colunas - 1) col++;
        else if (c == 'J') { termgraf_limpar(); return; }
        else if (c == 'K')
            for (int i = col; i < colunas; i++) {
                grade[lin * colunas + i].c = ' ';
                grade[lin * colunas + i].cor = cor_atual;
                desenhar_celula(i, lin, false);
            }
    } else if (c == 27) {
        estado_esc = 1;
    } else if (c == '\n') {
        col = 0;
        lin++;
    } else if (c == '\r') {
        col = 0;
    } else if (c == '\t') {
        col = (col + 8) & ~7;
    } else if (c == '\b') {
        if (col > 0) { col--; por(' '); }
    } else {
        por(c);
        col++;
    }
    if (col >= colunas) { col = 0; lin++; }
    if (lin >= linhas) rolar();
    mostrar_cursor(true);
}

void termgraf_mostrar(void)
{
    video_mostrar();
}

/* El modo de vídeo cambió (D059): cuadrícula nueva del tamaño de la pantalla. El texto se
 * copia desde la esquina de arriba a la izquierda; si las líneas no caben, quedan las
 * de abajo (las más nuevas, donde está el cursor). */
void termgraf_redimensionar(bool desenhar)
{
    int nc = (int)video_largura() / CEL_L, nl = (int)video_altura() / CEL_A;
    celula_t *nova = kmalloc((uint32_t)(nc * nl) * sizeof(celula_t));
    if (!nova) return;
    for (int i = 0; i < nc * nl; i++) { nova[i].c = ' '; nova[i].cor = cor_atual; }
    int desloc = lin >= nl ? lin - nl + 1 : 0;
    for (int y = 0; y < nl && y + desloc < linhas; y++)
        for (int x = 0; x < nc && x < colunas; x++)
            nova[y * nc + x] = grade[(y + desloc) * colunas + x];
    kfree(grade);
    grade = nova;
    colunas = nc; linhas = nl;
    lin -= desloc;
    if (col >= colunas) col = colunas - 1;
    if (!desenhar) return;
    video_retangulo(0, 0, (int)video_largura(), (int)video_altura(), paleta[COR_PRETO]);
    for (int y = 0; y < linhas; y++)
        for (int x = 0; x < colunas; x++)
            desenhar_celula(x, y, false);
    mostrar_cursor(true);
}
