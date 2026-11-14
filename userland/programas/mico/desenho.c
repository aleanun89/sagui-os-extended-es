// SPDX-License-Identifier: GPL-2.0-or-later
/* desenho.c - temas y el dibujo de bajo nivel del Mico.
 *
 * Todo se dibuja directo en los píxeles de la ventana, con un rectángulo de recorte
 * global ("recorte"): cada panel ajusta el recorte antes de dibujar, y lo
 * que pase del borde de este no aparece. Las letras vienen de la fuente 8x16 del
 * sistema (Latin-1), una fila de 8 bits por vez.
 *
 * Los dos temas son propios de SaguiOS: el claro usa el verde del sistema
 * en las palabras clave; el oscuro ("musgo") es un verde grisáceo profundo
 * con colores cálidos en las cadenas y los números. */
#include "mico.h"

const tema_t tema_claro = {
    .fundo = 0xFBFBF7, .texto = 0x1E2328, .linha_atual = 0xF0F3EC, .selecao = 0xC7DDF5, .busca = 0xF6E39A, .par = 0xD6E8D2,
    .margem_fundo = 0xF1F2EC, .margem_texto = 0x9AA19A, .margem_atual = 0x3E5A44, .cursor = 0x0B7A3E,
    .barra = 0xE4E8E1, .barra_texto = 0x1E2328, .barra_borda = 0xB9C0B6,
    .aba = 0xE9ECE5, .aba_ativa = 0xFBFBF7, .aba_texto = 0x2F3A33, .aba_marca = 0x0B7A3E,
    .lateral = 0xEEF1EA, .lateral_texto = 0x26302A, .lateral_sel = 0xCFE3D2, .lateral_pasta = 0x8A6A1E,
    .painel = 0xF4F5F1, .painel_texto = 0x1E2328, .term_fundo = 0x16201B, .term_texto = 0xD8E2DA,
    .estado = 0x0B7A3E, .estado_texto = 0xFFFFFF, .destaque = 0x0B7A3E, .erro = 0xC0392B, .aviso = 0xB7791F,
    .cl = { [CL_NORMAL] = 0x1E2328, [CL_CHAVE] = 0x0B7A3E, [CL_TIPO] = 0x1F5E8C, [CL_CADEIA] = 0x9A5B13,
            [CL_COMENTARIO] = 0x7E8A80, [CL_NUMERO] = 0xB0452A, [CL_PREPROC] = 0x7A3E9D, [CL_FUNCAO] = 0x245A7A,
            [CL_TITULO] = 0x14532D, [CL_ENFASE] = 0x6B3FA0, [CL_REGISTRO] = 0x1F5E8C, [CL_ROTULO] = 0xA0522D,
            [CL_VARIAVEL] = 0x8B4A9C },
};

const tema_t tema_escuro = {
    .fundo = 0x1D2621, .texto = 0xD6DED7, .linha_atual = 0x25302A, .selecao = 0x3A5A4A, .busca = 0x6B5A1E, .par = 0x3C5546,
    .margem_fundo = 0x19211C, .margem_texto = 0x5E6C62, .margem_atual = 0xB5D3BC, .cursor = 0x8FE3A6,
    .barra = 0x151C18, .barra_texto = 0xD6DED7, .barra_borda = 0x2E3A33,
    .aba = 0x18201B, .aba_ativa = 0x1D2621, .aba_texto = 0xB9C6BC, .aba_marca = 0x6FCF8A,
    .lateral = 0x18201B, .lateral_texto = 0xC3CEC5, .lateral_sel = 0x2D4436, .lateral_pasta = 0xD9B45A,
    .painel = 0x18201B, .painel_texto = 0xD6DED7, .term_fundo = 0x111713, .term_texto = 0xD8E2DA,
    .estado = 0x2E5E3E, .estado_texto = 0xE8F2EA, .destaque = 0x6FCF8A, .erro = 0xF07A6A, .aviso = 0xE0B45A,
    .cl = { [CL_NORMAL] = 0xD6DED7, [CL_CHAVE] = 0x7FD08F, [CL_TIPO] = 0x74B9D6, [CL_CADEIA] = 0xE5B56E,
            [CL_COMENTARIO] = 0x748A7B, [CL_NUMERO] = 0xE39272, [CL_PREPROC] = 0xC79BD8, [CL_FUNCAO] = 0x9FD3E8,
            [CL_TITULO] = 0x9FE3AE, [CL_ENFASE] = 0xD7A6E8, [CL_REGISTRO] = 0x74B9D6, [CL_ROTULO] = 0xE6A37A,
            [CL_VARIAVEL] = 0xD39BE0 },
};

const tema_t *tema = &tema_claro;
jan_t *j;
ret_t recorte;

void d_ret(int x, int y, int l, int a, uint32_t cor)
{
    int x1 = x + l, y1 = y + a;
    if (x < recorte.x) x = recorte.x;
    if (y < recorte.y) y = recorte.y;
    if (x1 > recorte.x + recorte.l) x1 = recorte.x + recorte.l;
    if (y1 > recorte.y + recorte.a) y1 = recorte.y + recorte.a;
    if (x1 > j->larg) x1 = j->larg;
    if (y1 > j->alt) y1 = j->alt;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 <= x || y1 <= y) return;
    for (int yy = y; yy < y1; yy++) preencher32(j->px + yy * j->larg + x, cor, (size_t)(x1 - x));
}

void d_moldura(int x, int y, int l, int a, uint32_t cor)
{
    d_ret(x, y, l, 1, cor);
    d_ret(x, y + a - 1, l, 1, cor);
    d_ret(x, y, 1, a, cor);
    d_ret(x + l - 1, y, 1, a, cor);
}

/* Una letra: solo los píxeles encendidos de la fuente (el fondo ya fue pintado). La
 * letra entera dentro del recorte es el caso común y va sin verificar píxel
 * a píxel. */
void d_letra(int x, int y, unsigned char c, uint32_t cor)
{
    if (c == ' ' || c == 0) return;
    int rx1 = recorte.x + recorte.l, ry1 = recorte.y + recorte.a;
    if (rx1 > j->larg) rx1 = j->larg;
    if (ry1 > j->alt) ry1 = j->alt;
    if (x + 8 <= recorte.x || y + 16 <= recorte.y || x >= rx1 || y >= ry1) return;
    const unsigned char *g = fonte_sagui[c];
    bool inteira = x >= recorte.x && x >= 0 && y >= recorte.y && y >= 0 && x + 8 <= rx1 && y + 16 <= ry1;
    for (int lin = 0; lin < 16; lin++) {
        unsigned b = g[lin];
        if (!b) continue;
        int yy = y + lin;
        if (!inteira && (yy < recorte.y || yy < 0 || yy >= ry1)) continue;
        uint32_t *p = j->px + yy * j->larg + x;
        for (int col = 0; col < 8; col++)
            if (b & (0x80u >> col)) {
                if (inteira || (x + col >= recorte.x && x + col >= 0 && x + col < rx1)) p[col] = cor;
            }
    }
}

void d_texto(int x, int y, const char *s, int n, uint32_t cor)
{
    for (int i = 0; i < n && s[i]; i++) d_letra(x + i * LETRA_L, y, (unsigned char)s[i], cor);
}

/* Escribe hasta max_px de ancho; si no entra, termina en "..". Devuelve el
 * ancho usado. */
int d_texto_ate(int x, int y, const char *s, int max_px, uint32_t cor)
{
    int n = (int)tam_texto(s), cabe = max_px / LETRA_L;
    if (cabe <= 0) return 0;
    if (n <= cabe) { d_texto(x, y, s, n, cor); return n * LETRA_L; }
    if (cabe > 2) { d_texto(x, y, s, cabe - 2, cor); d_texto(x + (cabe - 2) * LETRA_L, y, "..", 2, cor); }
    return cabe * LETRA_L;
}

void d_mostrar(ret_t r)
{
    if (r.l > 0 && r.a > 0) jan_mostrar_area(j, r.x, r.y, r.l, r.a);
}
