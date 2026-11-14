/* r600_dma_pacotes.c - paquetes del motor DMA de las Radeon R6xx/R7xx y el
 * reparto de los rectángulos en copias lineales (D223). Lógica pura: se prueba
 * también en el PC (tests/r600_dma_unidad.c). Ver r600_dma.h. */
#include "r600_dma.h"

int r600_pkt_copiar(uint32_t *p, uint32_t dst, uint32_t src, uint32_t ndw, bool r7xx)
{
    p[0] = R600_DMA_CABECA(R600_DMA_COPIAR, ndw);
    p[1] = dst & 0xFFFFFFFCu;
    p[2] = src & 0xFFFFFFFCu;
    p[3] = 0;                      /* bits 39:32: siempre por debajo de 4 GB */
    if (!r7xx) return 4;           /* R6xx: los dos en la misma palabra */
    p[4] = 0;                      /* R7xx: los del origen, aparte */
    return 5;
}

int r600_pkt_escrever(uint32_t *p, uint32_t dst, uint32_t valor)
{
    p[0] = R600_DMA_CABECA(R600_DMA_ESCREVER, 1);
    p[1] = dst & 0xFFFFFFFCu;
    p[2] = 0;
    p[3] = valor;
    return 4;
}

int r600_pkt_valla(uint32_t *p, uint32_t dst, uint32_t valor)
{
    p[0] = R600_DMA_CABECA(R600_DMA_VALLA, 0);
    p[1] = dst & 0xFFFFFFFCu;
    p[2] = 0;
    p[3] = valor;
    return 4;
}

/* Una copia lineal de n palabras, partida si pasa del máximo de un paquete.
 * Solo se usa con origen y destino que no se tocan. */
static bool linear(const r600_saida_t *s, uint32_t dst, uint32_t src, uint32_t n)
{
    while (n) {
        uint32_t k = n > R600_DMA_MAX_DW ? R600_DMA_MAX_DW : n;
        if (!s->copia(s->ctx, dst, src, k)) return false;
        dst += k * 4;
        src += k * 4;
        n -= k;
    }
    return true;
}

static inline uint32_t endereco(const r600_geo_t *g, int x, int y)
{
    return g->mc_tela + (uint32_t)y * g->pitch + (uint32_t)x * 4u;
}

bool r600_copiar_ret(const r600_geo_t *g, const r600_saida_t *s, int ox, int oy, int dx, int dy, int l, int a)
{
    if (l <= 0 || a <= 0 || (ox == dx && oy == dy)) return true;
    uint32_t n = (uint32_t)l;
    int d = dx - ox;
    if (oy == dy && d < l && -d < l) {
        /* Misma fila y se solapan: por la zona de paso, en franjas de filas. */
        if (g->filas_rascunho < 1) return false;
        for (int j0 = 0; j0 < a; j0 += g->filas_rascunho) {
            int fa = a - j0 < g->filas_rascunho ? a - j0 : g->filas_rascunho;
            for (int j = 0; j < fa; j++)
                if (!linear(s, g->mc_rascunho + (uint32_t)j * g->pitch, endereco(g, ox, oy + j0 + j), n)) return false;
            if (!s->barreira(s->ctx)) return false;
            for (int j = 0; j < fa; j++)
                if (!linear(s, endereco(g, dx, dy + j0 + j), g->mc_rascunho + (uint32_t)j * g->pitch, n)) return false;
            if (!s->barreira(s->ctx)) return false;   /* la próxima franja vuelve a escribir en la zona de paso */
        }
        return true;
    }
    /* D246: ancho completo (las filas son contiguas en la memoria): una copia por
     * bloque de filas en vez de una por fila. Cada bloque tiene como mucho
     * |dy - oy| filas, así que su origen y su destino no se tocan; si el
     * desplazamiento es mayor que la altura, todo el rectángulo es un solo
     * paquete. Los bloques van en el orden que no pisa lo que falta por leer. */
    if (ox == 0 && dx == 0 && (uint32_t)l * 4u == g->pitch && dy != oy) {
        int sh = dy > oy ? dy - oy : oy - dy;
        if (dy > oy) {
            for (int j1 = a; j1 > 0; j1 -= sh) {
                int m = j1 < sh ? j1 : sh, j0 = j1 - m;
                if (!linear(s, endereco(g, 0, dy + j0), endereco(g, 0, oy + j0), n * (uint32_t)m)) return false;
            }
        } else {
            for (int j0 = 0; j0 < a; j0 += sh) {
                int m = a - j0 < sh ? a - j0 : sh;
                if (!linear(s, endereco(g, 0, dy + j0), endereco(g, 0, oy + j0), n * (uint32_t)m)) return false;
            }
        }
        return true;
    }
    /* Filas distintas (o la misma fila sin solape): una copia por fila. Si el
     * destino está más abajo, de la última fila a la primera. */
    if (dy > oy) {
        for (int j = a - 1; j >= 0; j--)
            if (!linear(s, endereco(g, dx, dy + j), endereco(g, ox, oy + j), n)) return false;
    } else {
        for (int j = 0; j < a; j++)
            if (!linear(s, endereco(g, dx, dy + j), endereco(g, ox, oy + j), n)) return false;
    }
    return true;
}

bool r600_preencher_ret(const r600_geo_t *g, const r600_saida_t *s, uint32_t mc_linha_cor, int x, int y, int l, int a)
{
    for (int j = 0; j < a; j++)
        if (!linear(s, endereco(g, x, y + j), mc_linha_cor, (uint32_t)l)) return false;
    return true;
}
