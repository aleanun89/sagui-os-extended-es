// SPDX-License-Identifier: GPL-2.0-or-later
/* hid_informe.c - analizador de descriptores de informe HID y decodificación de mandos (D307).
 *
 * Escrito a partir de la especificación («Device Class Definition for HID 1.11», 6.2.2:
 * items cortos, globales, locales y principales; «HID Usage Tables 1.12»). Sin hardware:
 * tests/h307_mandos.py lo compila en el anfitrión con -DSAGUI_HOST.
 *
 * El analizador recorre los items y guarda cada campo de entrada (Input) con su posición en
 * bits, su tamaño, el rango lógico y la colección de aplicación donde está. No necesita
 * memoria dinámica: todo va en el hid_desc_t que se le pasa. Lo que no hace falta para leer
 * un mando (unidades, exponentes, designadores, cadenas) se salta. */
#include "hid_informe.h"

#define PILHA_MAX 4   /* Push/Pop anidados */

typedef struct {
    uint32_t pagina, tam, cont, id;
    int32_t  lmin;
    uint32_t lmax_v;       /* el máximo lógico tal como vino (sin signo) y su tamaño en bytes: */
    int      lmax_t;       /* el signo se decide al usarlo, cuando ya se sabe el mínimo */
} globais_t;

/* El máximo lógico se lee con signo, pero muchos descriptores ponen 255 en un byte (0xFF)
 * con el mínimo en 0: si el mínimo no es negativo, el máximo se toma sin signo. Se decide en
 * cada Input y no al leer el item: un descriptor puede dar el máximo antes que el mínimo, y
 * el mínimo que hubiera entonces puede ser el negativo de un campo anterior. */
static int32_t lmax_efetivo(const globais_t *g)
{
    uint32_t v = g->lmax_v;
    int t = g->lmax_t;
    int32_t sv = t == 1 ? (int8_t)v : t == 2 ? (int16_t)v : (int32_t)v;
    if (g->lmin >= 0 && sv < 0 && t < 4) return (int32_t)v;
    return sv;
}

int hid_analisar(const uint8_t *d, int n, hid_desc_t *out)
{
    globais_t g = { 0, 0, 0, 0, 0, 0, 0 };
    globais_t pilha[PILHA_MAX];
    int n_pilha = 0;
    uint32_t usos[HID_CAMPOS_MAX], n_usos = 0, uso_min = 0, uso_max = 0;
    bool tem_min = false;
    int prof = 0;
    uint32_t aplicacao = 0;
    uint16_t desloc[256];          /* bits ya ocupados en cada informe (por identificador) */
    for (int i = 0; i < 256; i++) desloc[i] = 0;
    out->n = 0;
    out->usa_ids = false;

    for (int i = 0; i < n;) {
        uint8_t p = d[i++];
        if (p == 0xFE) {           /* item largo: tamaño en el byte siguiente; no se usan */
            if (i >= n) return -1;
            i += 2 + d[i];
            if (i > n) return -1;
            continue;
        }
        int t = p & 3;
        t = t == 3 ? 4 : t;
        if (i + t > n) return -1;
        uint32_t v = 0;
        for (int k = 0; k < t; k++) v |= (uint32_t)d[i + k] << (8 * k);
        int32_t sv = t == 1 ? (int8_t)v : t == 2 ? (int16_t)v : (int32_t)v;
        i += t;
        uint8_t tipo = (p >> 2) & 3, tag = p >> 4;
        if (tipo == 1) {           /* global */
            switch (tag) {
            case 0: g.pagina = v; break;
            case 1: g.lmin = sv; break;
            case 2: g.lmax_v = v; g.lmax_t = t; break;
            case 7: g.tam = v; break;
            case 8:
                if (v == 0 || v > 255) return -1;
                g.id = v;
                out->usa_ids = true;
                break;
            case 9: g.cont = v; break;
            case 10:               /* Push */
                if (n_pilha >= PILHA_MAX) return -1;
                pilha[n_pilha++] = g;
                break;
            case 11:               /* Pop */
                if (!n_pilha) return -1;
                g = pilha[--n_pilha];
                break;
            default: break;        /* Physical, Unit, Exponent: no hacen falta */
            }
        } else if (tipo == 2) {    /* local */
            uint32_t u = (t == 4) ? v : (g.pagina << 16) | v;
            if (tag == 0) { if (n_usos < HID_CAMPOS_MAX) usos[n_usos++] = u; }
            else if (tag == 1) { uso_min = u; tem_min = true; }
            else if (tag == 2) uso_max = u;
        } else if (tipo == 0) {    /* principal */
            if (tag == 10) {       /* Collection */
                uint32_t u = n_usos ? usos[0] : 0;
                prof++;
                if (v == 1 && aplicacao == 0) aplicacao = u;   /* aplicación (la de más fuera) */
            } else if (tag == 12) {   /* End Collection */
                if (prof == 0) return -1;
                prof--;
                if (prof == 0) aplicacao = 0;
            } else if (tag == 8) {    /* Input */
                bool constante = v & 1, variavel = (v >> 1) & 1, relativo = (v >> 2) & 1;
                int32_t lmax = lmax_efetivo(&g);
                uint16_t *ds = &desloc[g.id & 0xFF];
                if (g.tam > 32 || g.cont > 255) return -1;
                if (!constante && g.tam) {
                    if (variavel) {
                        for (uint32_t k = 0; k < g.cont && out->n < HID_CAMPOS_MAX; k++) {
                            uint32_t u = k < n_usos ? usos[k] : (tem_min && !n_usos) ? uso_min + k
                                       : n_usos ? usos[n_usos - 1] : 0;
                            if (tem_min && uso_max && !n_usos && u > uso_max) u = uso_max;
                            hid_campo_t *c = &out->c[out->n++];
                            c->uso = u; c->uso_max = u; c->aplicacao = aplicacao;
                            c->lmin = g.lmin; c->lmax = lmax;
                            c->bit = (uint16_t)(*ds + k * g.tam);
                            c->tam = (uint8_t)g.tam; c->id = (uint8_t)g.id; c->cont = 1;
                            c->matriz = 0; c->relativo = relativo; c->sinal = g.lmin < 0;
                        }
                    } else if (out->n < HID_CAMPOS_MAX) {
                        hid_campo_t *c = &out->c[out->n++];
                        c->uso = tem_min ? uso_min : n_usos ? usos[0] : 0;
                        c->uso_max = tem_min ? (uso_max ? uso_max : uso_min) : n_usos ? usos[n_usos - 1] : 0;
                        c->aplicacao = aplicacao;
                        c->lmin = g.lmin; c->lmax = lmax;
                        c->bit = (uint16_t)*ds;
                        c->tam = (uint8_t)g.tam; c->id = (uint8_t)g.id; c->cont = (uint8_t)g.cont;
                        c->matriz = 1; c->relativo = relativo; c->sinal = g.lmin < 0;
                    }
                }
                uint32_t novo = *ds + g.tam * g.cont;
                if (novo > 0xFFFF) return -1;
                *ds = (uint16_t)novo;
            } else if (tag == 9 || tag == 11) {
                /* Output y Feature: no ocupan sitio en los informes de entrada */
            }
            n_usos = 0; uso_min = uso_max = 0; tem_min = false;
        }
    }
    if (prof != 0) return -1;
    return out->n;
}

int32_t hid_ler_bits(const uint8_t *r, int n, uint32_t bit, uint8_t tam, bool sinal)
{
    uint32_t v = 0;
    if (!tam) return 0;
    for (uint32_t i = 0; i < tam && i < 32; i++) {
        uint32_t b = bit + i;
        if ((int)(b >> 3) >= n) break;
        if (r[b >> 3] & (1u << (b & 7))) v |= 1u << i;
    }
    if (sinal && tam < 32 && (v & (1u << (tam - 1)))) v |= ~0u << tam;
    return (int32_t)v;
}

int16_t hid_normalizar_eixo(int32_t v, int32_t lmin, int32_t lmax)
{
    if (lmax <= lmin) return 0;
    if (v < lmin) v = lmin;
    if (v > lmax) v = lmax;
    /* Solo con 32 bits (el kernel no enlaza la libgcc): se reduce el rango hasta que la
     * multiplicación por 65535 quepa. */
    uint32_t rango = (uint32_t)(lmax - lmin), x = (uint32_t)(v - lmin);
    while (rango > 0xFFFF) { rango >>= 1; x >>= 1; }
    if (!rango) return 0;
    int32_t r = (int32_t)(x * 65535u / rango) - 32768;
    if (r > 32767) r = 32767;
    return (int16_t)r;
}

uint8_t hid_normalizar_hat(int32_t v, int32_t lmin, int32_t lmax)
{
    static const uint8_t oito[8] = {
        HAT_CIMA, HAT_CIMA | HAT_DIREITA, HAT_DIREITA, HAT_BAIXO | HAT_DIREITA,
        HAT_BAIXO, HAT_BAIXO | HAT_ESQUERDA, HAT_ESQUERDA, HAT_CIMA | HAT_ESQUERDA,
    };
    static const uint8_t quatro[4] = { HAT_CIMA, HAT_DIREITA, HAT_BAIXO, HAT_ESQUERDA };
    if (v < lmin || v > lmax) return HAT_CENTRO;   /* fuera del rango: suelto (estado nulo) */
    int32_t k = v - lmin;
    if (lmax - lmin == 7) return oito[k];
    if (lmax - lmin == 3) return quatro[k];
    return HAT_CENTRO;
}

static bool eh_eixo(uint32_t u)
{
    return (u >= HID_USO_X && u <= HID_USO_RODA)
        || u == 0x000200BAu || u == 0x000200BBu      /* Simulation: timón y acelerador */
        || u == 0x000200C4u || u == 0x000200C5u      /* acelerador y freno de volante */
        || u == 0x000200C8u;                         /* dirección */
}

static bool eh_mando(uint32_t a)
{
    return a == HID_USO_JOYSTICK || a == HID_USO_GAMEPAD || a == HID_USO_MULTIEIXO;
}

bool hid_mando_extrair(const hid_desc_t *d, hid_mando_t *m)
{
    uint8_t *z = (uint8_t *)m;
    for (unsigned i = 0; i < sizeof *m; i++) z[i] = 0;
    m->usa_ids = d->usa_ids;
    for (int i = 0; i < d->n; i++)
        if (eh_mando(d->c[i].aplicacao)) { m->aplicacao = d->c[i].aplicacao; break; }
    if (!m->aplicacao) return false;
    for (int i = 0; i < d->n; i++) {
        const hid_campo_t *c = &d->c[i];
        if (c->aplicacao != m->aplicacao) continue;
        hid_eixo_t e = { c->uso, c->lmin, c->lmax, c->bit, c->tam, c->sinal, c->id };
        if (c->matriz) {
            if ((c->uso >> 16) == HID_PAGINA_BOTAO && !m->botao_matriz_cont) {
                m->botao_matriz = e;
                m->botao_matriz_cont = c->cont;
                uint32_t ult = c->uso_max & 0xFFFF;
                if (ult > HID_MANDO_BOTOES) ult = HID_MANDO_BOTOES;
                if (ult > m->n_botoes) m->n_botoes = (uint8_t)ult;
            }
            continue;
        }
        if (c->uso == HID_USO_HAT) {
            if (m->n_hats < HID_MANDO_HATS) m->hat[m->n_hats++] = e;
        } else if (eh_eixo(c->uso) && !c->relativo && c->tam > 1) {
            if (m->n_eixos < HID_MANDO_EIXOS) {
                /* En orden de uso (X, Y, Z, Rx...), como los numera SDL; los repetidos detrás. */
                int k = m->n_eixos++;
                while (k > 0 && m->eixo[k - 1].uso > e.uso) { m->eixo[k] = m->eixo[k - 1]; k--; }
                m->eixo[k] = e;
            }
        } else if ((c->uso >> 16) == HID_PAGINA_BOTAO && c->tam == 1) {
            uint32_t b = c->uso & 0xFFFF;   /* Button 1 es el primero */
            if (b >= 1 && b <= HID_MANDO_BOTOES) {
                m->botao_bit[b - 1] = c->bit;
                m->botao_id[b - 1] = c->id;
                if (b > m->n_botoes) m->n_botoes = (uint8_t)b;
            }
        }
    }
    /* Los huecos (números de botón que el descriptor no nombra) se marcan con el id 0xFF,
     * que no llega nunca en un informe con identificador ni sin él: no se leen. */
    for (int b = 0; b < m->n_botoes; b++) {
        bool achado = false;
        for (int i = 0; i < d->n && !achado; i++) {
            const hid_campo_t *c = &d->c[i];
            if (c->aplicacao == m->aplicacao && !c->matriz && (c->uso >> 16) == HID_PAGINA_BOTAO
                && (c->uso & 0xFFFF) == (uint32_t)(b + 1) && c->tam == 1) achado = true;
        }
        if (!achado) m->botao_id[b] = 0xFF;
    }
    return m->n_eixos || m->n_botoes || m->n_hats;
}

bool hid_mando_decodificar(const hid_mando_t *m, const uint8_t *r, int n, hid_estado_t *e)
{
    uint8_t id = 0;
    if (m->usa_ids) {
        if (n < 1) return false;
        id = r[0];
        r++; n--;
    }
    bool algum = false;
    for (int i = 0; i < m->n_eixos; i++) {
        const hid_eixo_t *x = &m->eixo[i];
        if (x->id != id) continue;
        e->eixo[i] = hid_normalizar_eixo(hid_ler_bits(r, n, x->bit, x->tam, x->sinal), x->lmin, x->lmax);
        algum = true;
    }
    for (int i = 0; i < m->n_hats; i++) {
        const hid_eixo_t *x = &m->hat[i];
        if (x->id != id) continue;
        e->hat[i] = hid_normalizar_hat(hid_ler_bits(r, n, x->bit, x->tam, x->sinal), x->lmin, x->lmax);
        algum = true;
    }
    bool botoes_aqui = false;
    uint32_t b = e->botoes;
    for (int i = 0; i < m->n_botoes; i++) {
        if (m->botao_id[i] != id) continue;
        botoes_aqui = true;
        if (hid_ler_bits(r, n, m->botao_bit[i], 1, false)) b |= 1u << i;
        else b &= ~(1u << i);
    }
    if (m->botao_matriz_cont && m->botao_matriz.id == id) {
        if (!botoes_aqui) b = 0;
        botoes_aqui = true;
        for (int k = 0; k < m->botao_matriz_cont; k++) {
            int32_t v = hid_ler_bits(r, n, m->botao_matriz.bit + (uint32_t)k * m->botao_matriz.tam,
                                     m->botao_matriz.tam, false);
            int32_t num = v - m->botao_matriz.lmin + (int32_t)(m->botao_matriz.uso & 0xFFFF);
            if (v >= m->botao_matriz.lmin && v <= m->botao_matriz.lmax && num >= 1 && num <= HID_MANDO_BOTOES)
                b |= 1u << (num - 1);
        }
    }
    if (botoes_aqui) { e->botoes = b; algum = true; }
    return algum;
}

/* ------------------------------------------------------------ Xbox 360 con cable */

static int16_t invertir(int16_t v) { return v == -32768 ? 32767 : (int16_t)-v; }

bool xbox360_decodificar(const uint8_t *r, int n, hid_estado_t *e)
{
    if (n < 14 || r[0] != 0x00 || r[1] < 14) return false;   /* otros mensajes: luces, auriculares */
    uint8_t b0 = r[2], b1 = r[3];
    uint8_t h = 0;
    if (b0 & 0x01) h |= HAT_CIMA;
    if (b0 & 0x02) h |= HAT_BAIXO;
    if (b0 & 0x04) h |= HAT_ESQUERDA;
    if (b0 & 0x08) h |= HAT_DIREITA;
    e->hat[0] = h;
    uint32_t b = 0;
    if (b1 & 0x10) b |= 1u << 0;    /* A */
    if (b1 & 0x20) b |= 1u << 1;    /* B */
    if (b1 & 0x40) b |= 1u << 2;    /* X */
    if (b1 & 0x80) b |= 1u << 3;    /* Y */
    if (b1 & 0x01) b |= 1u << 4;    /* LB */
    if (b1 & 0x02) b |= 1u << 5;    /* RB */
    if (b0 & 0x20) b |= 1u << 6;    /* Back */
    if (b0 & 0x10) b |= 1u << 7;    /* Start */
    if (b1 & 0x04) b |= 1u << 8;    /* Guía (el botón de Xbox) */
    if (b0 & 0x40) b |= 1u << 9;    /* stick izquierdo pulsado */
    if (b0 & 0x80) b |= 1u << 10;   /* stick derecho pulsado */
    e->botoes = b;
    e->eixo[0] = (int16_t)(r[6] | (r[7] << 8));
    e->eixo[1] = invertir((int16_t)(r[8] | (r[9] << 8)));
    e->eixo[2] = hid_normalizar_eixo(r[4], 0, 255);
    e->eixo[3] = (int16_t)(r[10] | (r[11] << 8));
    e->eixo[4] = invertir((int16_t)(r[12] | (r[13] << 8)));
    e->eixo[5] = hid_normalizar_eixo(r[5], 0, 255);
    return true;
}
