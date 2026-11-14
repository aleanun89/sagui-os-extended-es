// SPDX-License-Identifier: GPL-2.0-or-later
/* nucleo.c - a CPU, a memória e a tela do CHIP-8, SUPER-CHIP 1.1 e XO-CHIP.
 *
 * Escrito a partir das especificações públicas (a referência técnica do
 * Cowgod para o CHIP-8, a descrição do SUPER-CHIP 1.1 e a do XO-CHIP de John
 * Earnest), sem código de outros emuladores (D070).
 *
 * A tela é um byte por pixel com um bit por plano: no CHIP-8 e no SCHIP só
 * o bit 0 existe; no XO-CHIP os dois planos dão 4 cores. Ela tem sempre o
 * tamanho do modo atual (64x32 ou 128x64) e é apagada ao trocar de modo,
 * como no XO-CHIP e no "SCHIP moderno"; o rolamento anda pixels do modo
 * atual (o SCHIP 1.1 original andava meio pixel no modo baixo: não imitado).
 *
 * As instruções fora do modo escolhido são erro (uma ROM de CHIP-8 que usa
 * 00FF provavelmente é de SCHIP), menos 0NNN no CHIP-8, que as máquinas
 * antigas mandavam para código de máquina e aqui é ignorado. */
#include "chip8.h"

/* A fonte 4x5 de sempre: são os valores da referência do Cowgod, que muitas
 * ROMs pressupõem (desenham dígitos com FX29). */
static const uint8_t fonte_pequena[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,  0x20, 0x60, 0x20, 0x20, 0x70,  0xF0, 0x10, 0xF0, 0x80, 0xF0,
    0xF0, 0x10, 0xF0, 0x10, 0xF0,  0x90, 0x90, 0xF0, 0x10, 0x10,  0xF0, 0x80, 0xF0, 0x10, 0xF0,
    0xF0, 0x80, 0xF0, 0x90, 0xF0,  0xF0, 0x10, 0x20, 0x40, 0x40,  0xF0, 0x90, 0xF0, 0x90, 0xF0,
    0xF0, 0x90, 0xF0, 0x10, 0xF0,  0xF0, 0x90, 0xF0, 0x90, 0x90,  0xE0, 0x90, 0xE0, 0x90, 0xE0,
    0xF0, 0x80, 0x80, 0x80, 0xF0,  0xE0, 0x90, 0x90, 0x90, 0xE0,  0xF0, 0x80, 0xF0, 0x80, 0xF0,
    0xF0, 0x80, 0xF0, 0x80, 0x80,
};

/* A fonte grande 8x10 (FX30) é desenho nosso, de 0 a F (o SCHIP só tinha 0-9). */
static const uint8_t fonte_grande[160] = {
    0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0xC3, 0xC3, 0xE7, 0x7E, 0x3C,   /* 0 */
    0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x7E,   /* 1 */
    0x3C, 0x7E, 0xC3, 0x03, 0x0E, 0x38, 0x60, 0xC0, 0xFF, 0xFF,   /* 2 */
    0x3C, 0x7E, 0xC3, 0x03, 0x1E, 0x1E, 0x03, 0xC3, 0x7E, 0x3C,   /* 3 */
    0x06, 0x0E, 0x1E, 0x36, 0x66, 0xC6, 0xFF, 0xFF, 0x06, 0x06,   /* 4 */
    0xFF, 0xFF, 0xC0, 0xC0, 0xFC, 0x7E, 0x03, 0xC3, 0x7E, 0x3C,   /* 5 */
    0x3C, 0x7E, 0xC0, 0xC0, 0xFC, 0xFE, 0xC3, 0xC3, 0x7E, 0x3C,   /* 6 */
    0xFF, 0xFF, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x30,   /* 7 */
    0x3C, 0x7E, 0xC3, 0xC3, 0x7E, 0x7E, 0xC3, 0xC3, 0x7E, 0x3C,   /* 8 */
    0x3C, 0x7E, 0xC3, 0xC3, 0x7F, 0x3F, 0x03, 0x03, 0x7E, 0x3C,   /* 9 */
    0x3C, 0x7E, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3,   /* A */
    0xFC, 0xFE, 0xC3, 0xC3, 0xFE, 0xFE, 0xC3, 0xC3, 0xFE, 0xFC,   /* B */
    0x3C, 0x7E, 0xC3, 0xC0, 0xC0, 0xC0, 0xC0, 0xC3, 0x7E, 0x3C,   /* C */
    0xFC, 0xFE, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFE, 0xFC,   /* D */
    0xFF, 0xFF, 0xC0, 0xC0, 0xFC, 0xFC, 0xC0, 0xC0, 0xFF, 0xFF,   /* E */
    0xFF, 0xFF, 0xC0, 0xC0, 0xFC, 0xFC, 0xC0, 0xC0, 0xC0, 0xC0,   /* F */
};

/* 1587,4 x 2^(r/48): o XO-CHIP toca o padrão a 4000 x 2^((tom-64)/48)
 * amostras por segundo; com tom = 48q + r isso é tabela[r] << q. */
static const uint16_t taxa_tom[48] = {
    1587, 1610, 1634, 1658, 1682, 1706, 1731, 1756, 1782, 1808, 1834, 1861, 1888, 1915, 1943, 1971,
    2000, 2029, 2059, 2089, 2119, 2150, 2181, 2213, 2245, 2278, 2311, 2344, 2378, 2413, 2448, 2484,
    2520, 2556, 2594, 2631, 2670, 2709, 2748, 2788, 2828, 2870, 2911, 2954, 2997, 3040, 3084, 3129,
};

static const char *const nomes_modo[C8_QTD_MODOS] = { "CHIP-8", "SUPER-CHIP", "XO-CHIP" };
static const char *const ids_modo[C8_QTD_MODOS] = { "chip8", "schip", "xochip" };

const char *c8_nome_modo(int m) { return m >= 0 && m < C8_QTD_MODOS ? nomes_modo[m] : "?"; }
const char *c8_id_modo(int m) { return m >= 0 && m < C8_QTD_MODOS ? ids_modo[m] : "?"; }

int c8_modo_por_nome(const char *s)
{
    for (int m = 0; m < C8_QTD_MODOS; m++)
        if (compara_texto(s, ids_modo[m]) == 0) return m;
    if (compara_texto(s, "superchip") == 0 || compara_texto(s, "sc8") == 0) return C8_SCHIP;
    if (compara_texto(s, "xo") == 0 || compara_texto(s, "xo8") == 0) return C8_XOCHIP;
    if (compara_texto(s, "ch8") == 0) return C8_CHIP8;
    return -1;
}

static int minuscula(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

int c8_modo_por_extensao(const char *arq)
{
    int n = (int)tam_texto(arq);
    if (n < 4 || arq[n - 4] != '.') return -1;
    char e[4] = { (char)minuscula(arq[n - 3]), (char)minuscula(arq[n - 2]), (char)minuscula(arq[n - 1]), 0 };
    if (compara_texto(e, "ch8") == 0) return C8_CHIP8;
    if (compara_texto(e, "sc8") == 0) return C8_SCHIP;
    if (compara_texto(e, "xo8") == 0) return C8_XOCHIP;
    return -1;
}

void c8_quirks_padrao(int modo, c8_quirks_t *q)
{
    /* COSMAC VIP para o CHIP-8; HP-48 para o SCHIP 1.1; Octo para o XO-CHIP. */
    q->desloc_vy = modo != C8_SCHIP;
    q->incrementa_i = modo != C8_SCHIP;
    q->salto_vx = modo == C8_SCHIP;
    q->recorta = modo != C8_XOCHIP;
    q->zera_vf = modo == C8_CHIP8;
    q->espera_vblank = modo == C8_CHIP8;
}

/* "nome=valor": desplazar=vy|vx, memoria=incrementa|fija, salto=v0|vx,
 * borde=recorta|envuelve, logicas=reinicia|conserva, vblank=espera|no. */
bool c8_quirk_texto(c8_quirks_t *q, const char *op)
{
    static const struct { const char *nome, *sim, *nao; int campo; } tab[] = {
        { "desplazar", "vy", "vx", 0 }, { "memoria", "incrementa", "fija", 1 },
        { "salto", "vx", "v0", 2 }, { "borde", "recorta", "envuelve", 3 },
        { "logicas", "reinicia", "conserva", 4 }, { "vblank", "espera", "no", 5 },
    };
    const char *ig = op;
    while (*ig && *ig != '=') ig++;
    if (!*ig) return false;
    int n = (int)(ig - op);
    const char *val = ig + 1;
    for (unsigned k = 0; k < sizeof tab / sizeof tab[0]; k++) {
        if ((int)tam_texto(tab[k].nome) != n || compara_texto_n(op, tab[k].nome, (size_t)n) != 0) continue;
        bool v;
        if (compara_texto(val, tab[k].sim) == 0 || compara_texto(val, "1") == 0 || compara_texto(val, "si") == 0) v = true;
        else if (compara_texto(val, tab[k].nao) == 0 || compara_texto(val, "0") == 0 || compara_texto(val, "no") == 0) v = false;
        else return false;
        bool *campos[6] = { &q->desloc_vy, &q->incrementa_i, &q->salto_vx, &q->recorta, &q->zera_vf, &q->espera_vblank };
        *campos[tab[k].campo] = v;
        return true;
    }
    return false;
}

void c8_quirks_descrever(const c8_quirks_t *q, char *buf, int max)
{
    formatar_texto(buf, (size_t)max, "desplazar=%s memoria=%s salto=%s borde=%s logicas=%s vblank=%s",
                   q->desloc_vy ? "vy" : "vx", q->incrementa_i ? "incrementa" : "fija", q->salto_vx ? "vx" : "v0",
                   q->recorta ? "recorta" : "envuelve", q->zera_vf ? "reinicia" : "conserva",
                   q->espera_vblank ? "espera" : "no");
}

int c8_rom_max(int modo) { return (modo == C8_XOCHIP ? 65536 : 4096) - C8_INICIO; }

static void mudar_resolucao(c8_t *c, bool alta)
{
    c->alta = alta;
    c->larg = alta ? 128 : 64;
    c->alt = alta ? 64 : 32;
    memset(c->tela, 0, sizeof c->tela);
    c->tela_mudou = true;
    c->linhas_sujas = ~0ull;
}

void c8_iniciar(c8_t *c, int modo, const c8_quirks_t *q, uint32_t semente)
{
    memset(c, 0, sizeof *c);
    c->modo = modo;
    if (q) c->q = *q; else c8_quirks_padrao(modo, &c->q);
    c->mascara = modo == C8_XOCHIP ? 0xFFFF : 0xFFF;
    memcpy(c->mem + C8_FONTE, fonte_pequena, sizeof fonte_pequena);
    memcpy(c->mem + C8_FONTE_G, fonte_grande, sizeof fonte_grande);
    c->pc = C8_INICIO;
    c->planos = 1;
    c->tom = 64;
    c->espera_reg = -1;
    c->semente = semente ? semente : 0x5A6E1u;
    mudar_resolucao(c, false);
}

bool c8_carregar(c8_t *c, const uint8_t *rom, int n)
{
    if (n < 0 || n > c8_rom_max(c->modo)) return false;
    memcpy(c->mem + C8_INICIO, rom, (size_t)n);
    c->tam_rom = n;
    return true;
}

void c8_teclas(c8_t *c, uint16_t t) { c->teclas = t; }

void c8_quadro(c8_t *c)
{
    if (c->dt) c->dt--;
    if (c->st) c->st--;
    c->vblank = false;
}

uint32_t c8_freq_som(const c8_t *c)
{
    if (!c->st || c->estado != C8_RODANDO) return 0;
    if (c->modo != C8_XOCHIP || !c->tem_padrao) return 440;
    /* O alto-falante do PC só faz onda quadrada: em vez de tocar o padrão,
     * toca a fundamental dele, contando quantas vezes o sinal sobe nos 128
     * bits (um padrão 0000FFFF... repetido dá taxa x 8 / 128). */
    int subidas = 0;
    for (int b = 0; b < 128; b++) {
        int a = (c->padrao[b >> 3] >> (7 - (b & 7))) & 1;
        int bb = (b + 127) & 127;
        int ant = (c->padrao[bb >> 3] >> (7 - (bb & 7))) & 1;
        if (a && !ant) subidas++;
    }
    if (!subidas) return 0;
    uint32_t taxa = c8_taxa_padrao(c);
    uint32_t f = taxa * (uint32_t)subidas / 128;
    if (f < 30) f = 30;
    if (f > 12000) f = 12000;
    return f;
}

/* Amostras por segundo do padrão de áudio do XO-CHIP (FX3A). */
uint32_t c8_taxa_padrao(const c8_t *c)
{
    return (uint32_t)taxa_tom[c->tom % 48] << (c->tom / 48);
}

uint32_t c8_soma_tela(const c8_t *c)
{
    uint32_t h = 2166136261u;
    for (int k = 0; k < c->larg * c->alt; k++) { h ^= c->tela[k]; h *= 16777619u; }
    return h;
}

/* ------------------------------------------------------------ desenho */

static uint8_t ler8(const c8_t *c, uint32_t end) { return c->mem[end & c->mascara]; }

/* DXYN. O ponto de partida dá sempre a volta (VX módulo a largura); o que
 * passa da borda é cortado ou dá a volta conforme o quirk. Cada plano
 * ligado consome o seu próprio pedaço do sprite, em sequência (XO-CHIP). */
static void desenhar(c8_t *c, int x, int y, int n)
{
    int x0 = c->v[x] % c->larg, y0 = c->v[y] % c->alt;
    bool grande = n == 0 && c->modo != C8_CHIP8;
    int l = grande ? 16 : 8, a = grande ? 16 : n, bytes = grande ? 32 : n;
    uint32_t end = c->i;
    bool colisao = false;
    for (int p = 0; p < 2; p++) {
        if (!(c->planos & (1 << p))) continue;
        uint8_t bit = (uint8_t)(1 << p);
        for (int lin = 0; lin < a; lin++) {
            int py = y0 + lin;
            if (py >= c->alt) { if (c->q.recorta) break; py -= c->alt; }
            c->linhas_sujas |= 1ull << py;
            uint32_t linha = grande ? (uint32_t)ler8(c, end + 2 * lin) << 8 | ler8(c, end + 2 * lin + 1)
                                    : (uint32_t)ler8(c, end + lin) << 8;
            for (int col = 0; col < l; col++) {
                if (!(linha & (0x8000u >> col))) continue;
                int px = x0 + col;
                if (px >= c->larg) { if (c->q.recorta) break; px -= c->larg; }
                uint8_t *t = &c->tela[py * c->larg + px];
                if (*t & bit) colisao = true;
                *t ^= bit;
            }
        }
        end += (uint32_t)bytes;
    }
    c->v[15] = colisao;
    c->tela_mudou = true;
    if (c->q.espera_vblank) c->vblank = true;
}

/* Rola os planos ligados: dx > 0 para a direita, dy > 0 para baixo. */
static void rolar(c8_t *c, int dx, int dy)
{
    uint8_t m = c->planos & 3;
    if (!m) return;
    static uint8_t copia[C8_LARG_MAX * C8_ALT_MAX];
    memcpy(copia, c->tela, (size_t)(c->larg * c->alt));
    for (int y = 0; y < c->alt; y++)
        for (int x = 0; x < c->larg; x++) {
            int ox = x - dx, oy = y - dy;
            uint8_t de = (ox >= 0 && ox < c->larg && oy >= 0 && oy < c->alt) ? copia[oy * c->larg + ox] & m : 0;
            uint8_t *t = &c->tela[y * c->larg + x];
            *t = (uint8_t)((*t & ~m) | de);
        }
    c->tela_mudou = true;
    c->linhas_sujas = ~0ull;
}

static void apagar_tela(c8_t *c)
{
    uint8_t m = c->planos & 3;
    for (int k = 0; k < c->larg * c->alt; k++) c->tela[k] &= (uint8_t)~m;
    c->tela_mudou = true;
    c->linhas_sujas = ~0ull;
}

/* ------------------------------------------------------------ CPU */

static uint32_t aleatorio(c8_t *c)
{
    uint32_t s = c->semente;   /* xorshift32 */
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    c->semente = s;
    return s >> 8;
}

static void falhar(c8_t *c, uint16_t pc, uint16_t op, const char *motivo)
{
    c->estado = C8_ERRO;
    c->pc = pc;
    formatar_texto(c->erro, sizeof c->erro, "%s: %04x en %03x", motivo, op, pc);
}

/* Pula a próxima instrução: no XO-CHIP o F000 NNNN tem 4 bytes. */
static void pular(c8_t *c)
{
    uint16_t prox = (uint16_t)(ler8(c, c->pc) << 8 | ler8(c, c->pc + 1));
    c->pc = (uint16_t)((c->pc + (c->modo == C8_XOCHIP && prox == 0xF000 ? 4 : 2)) & c->mascara);
}

static bool passo(c8_t *c)
{
    if (c->espera_reg >= 0) {
        /* FX0A: como no COSMAC, a tecla conta quando é solta. */
        c->espera_vistas |= c->teclas;
        uint16_t soltas = c->espera_vistas & (uint16_t)~c->teclas;
        if (!soltas) return false;
        int k = 0;
        while (!(soltas & (1 << k))) k++;
        c->v[c->espera_reg] = (uint8_t)k;
        c->espera_reg = -1;
    }
    uint16_t pc = c->pc;
    uint16_t op = (uint16_t)(ler8(c, pc) << 8 | ler8(c, pc + 1));
    c->pc = (uint16_t)((pc + 2) & c->mascara);
    int x = (op >> 8) & 0xF, y = (op >> 4) & 0xF, n = op & 0xF;
    uint8_t nn = op & 0xFF;
    uint16_t nnn = op & 0xFFF;
    bool sc = c->modo != C8_CHIP8, xo = c->modo == C8_XOCHIP;
    uint8_t *v = c->v;
    c->instrucoes++;

    switch (op >> 12) {
    case 0x0:
        if (op == 0x00E0) { apagar_tela(c); break; }
        if (op == 0x00EE) {
            if (c->sp == 0) { falhar(c, pc, op, "Retorno con la pila vacía"); return false; }
            c->pc = c->pilha[--c->sp];
            break;
        }
        if (sc && (op & 0xFFF0) == 0x00C0 && n) { rolar(c, 0, n); break; }
        if (xo && (op & 0xFFF0) == 0x00D0 && n) { rolar(c, 0, -n); break; }
        if (sc && op == 0x00FB) { rolar(c, 4, 0); break; }
        if (sc && op == 0x00FC) { rolar(c, -4, 0); break; }
        if (sc && op == 0x00FD) { c->estado = C8_TERMINOU; c->pc = pc; return false; }
        if (sc && op == 0x00FE) { mudar_resolucao(c, false); break; }
        if (sc && op == 0x00FF) { mudar_resolucao(c, true); break; }
        if (!sc) break;   /* 0NNN: código de máquina do COSMAC; ignorado */
        falhar(c, pc, op, "Instrucción no válida");
        return false;
    case 0x1: c->pc = nnn; break;
    case 0x2:
        if (c->sp == 16) { falhar(c, pc, op, "Pila llena"); return false; }
        c->pilha[c->sp++] = c->pc;
        c->pc = nnn;
        break;
    case 0x3: if (v[x] == nn) pular(c); break;
    case 0x4: if (v[x] != nn) pular(c); break;
    case 0x5:
        if (n == 0) { if (v[x] == v[y]) pular(c); break; }
        if (xo && (n == 2 || n == 3)) {
            /* 5XY2/5XY3: guarda ou carrega VX..VY (em qualquer ordem) sem mexer em I. */
            int passo_r = x <= y ? 1 : -1;
            for (int k = 0, r = x;; k++, r += passo_r) {
                if (n == 2) c->mem[(c->i + k) & c->mascara] = v[r];
                else v[r] = ler8(c, c->i + (uint32_t)k);
                if (r == y) break;
            }
            break;
        }
        falhar(c, pc, op, "Instrucción no válida");
        return false;
    case 0x6: v[x] = nn; break;
    case 0x7: v[x] = (uint8_t)(v[x] + nn); break;
    case 0x8: {
        uint8_t vx = v[x], vy = v[y], f;
        switch (n) {
        case 0x0: v[x] = vy; break;
        case 0x1: v[x] = vx | vy; if (c->q.zera_vf) v[15] = 0; break;
        case 0x2: v[x] = vx & vy; if (c->q.zera_vf) v[15] = 0; break;
        case 0x3: v[x] = vx ^ vy; if (c->q.zera_vf) v[15] = 0; break;
        /* VF por último: se X for F, vale o vai-um, não o resultado. */
        case 0x4: v[x] = (uint8_t)(vx + vy); v[15] = vx + vy > 255; break;
        case 0x5: v[x] = (uint8_t)(vx - vy); v[15] = vx >= vy; break;
        case 0x7: v[x] = (uint8_t)(vy - vx); v[15] = vy >= vx; break;
        case 0x6: f = c->q.desloc_vy ? vy : vx; v[x] = f >> 1; v[15] = f & 1; break;
        case 0xE: f = c->q.desloc_vy ? vy : vx; v[x] = (uint8_t)(f << 1); v[15] = f >> 7; break;
        default: falhar(c, pc, op, "Instrucción no válida"); return false;
        }
        break;
    }
    case 0x9:
        if (n != 0) { falhar(c, pc, op, "Instrucción no válida"); return false; }
        if (v[x] != v[y]) pular(c);
        break;
    case 0xA: c->i = nnn; break;
    case 0xB: c->pc = (uint16_t)((nnn + (c->q.salto_vx ? v[x] : v[0])) & c->mascara); break;
    case 0xC: v[x] = (uint8_t)(aleatorio(c) & nn); break;
    case 0xD: desenhar(c, x, y, n); break;
    case 0xE:
        if (nn == 0x9E) { if (c->teclas & (1 << (v[x] & 15))) pular(c); break; }
        if (nn == 0xA1) { if (!(c->teclas & (1 << (v[x] & 15)))) pular(c); break; }
        falhar(c, pc, op, "Instrucción no válida");
        return false;
    case 0xF:
        if (xo && op == 0xF000) {   /* I = NNNN (instrução de 4 bytes) */
            c->i = (uint16_t)(ler8(c, c->pc) << 8 | ler8(c, c->pc + 1));
            c->pc = (uint16_t)((c->pc + 2) & c->mascara);
            break;
        }
        if (xo && nn == 0x01) { c->planos = (uint8_t)(x & 3); break; }
        if (xo && op == 0xF002) {
            for (int k = 0; k < 16; k++) c->padrao[k] = ler8(c, c->i + (uint32_t)k);
            c->tem_padrao = true;
            break;
        }
        switch (nn) {
        case 0x07: v[x] = c->dt; break;
        case 0x0A: c->espera_reg = x; c->espera_vistas = 0; break;
        case 0x15: c->dt = v[x]; break;
        case 0x18: c->st = v[x]; break;
        case 0x1E: c->i = (uint16_t)((c->i + v[x]) & c->mascara); break;
        case 0x29: c->i = (uint16_t)(C8_FONTE + (v[x] & 15) * 5); break;
        case 0x33:
            c->mem[c->i & c->mascara] = v[x] / 100;
            c->mem[(c->i + 1) & c->mascara] = v[x] / 10 % 10;
            c->mem[(c->i + 2) & c->mascara] = v[x] % 10;
            break;
        case 0x55: case 0x65:
            for (int k = 0; k <= x; k++) {
                if (nn == 0x55) c->mem[(c->i + k) & c->mascara] = v[k];
                else v[k] = ler8(c, c->i + (uint32_t)k);
            }
            if (c->q.incrementa_i) c->i = (uint16_t)((c->i + x + 1) & c->mascara);
            break;
        default:
            if (sc && nn == 0x30) { c->i = (uint16_t)(C8_FONTE_G + (v[x] & 15) * 10); break; }
            if (sc && (nn == 0x75 || nn == 0x85)) {
                int ate = xo ? x : (x > 7 ? 7 : x);   /* o HP-48 tinha 8 "flags" */
                for (int k = 0; k <= ate; k++) {
                    if (nn == 0x75) c->rpl[k] = v[k]; else v[k] = c->rpl[k];
                }
                break;
            }
            if (xo && nn == 0x3A) { c->tom = v[x]; break; }
            falhar(c, pc, op, "Instrucción no válida");
            return false;
        }
        break;
    }
    return true;
}

int c8_executar(c8_t *c, int n)
{
    int feitas = 0;
    while (feitas < n && c->estado == C8_RODANDO && !c->vblank) {
        if (!passo(c)) break;
        feitas++;
    }
    return feitas;
}
