// SPDX-License-Identifier: GPL-2.0-or-later
/* chip8 [opções] [rom] - o emulador de CHIP-8, SUPER-CHIP e XO-CHIP (D070).
 *
 * A janela mostra a tela da máquina ampliada por um fator inteiro e
 * centralizada (redimensionável, maximizável, F11 = tela cheia), com uma
 * barra de estado embaixo e, com F10, um painel de depuração à direita.
 *
 * O tempo: os 60 Hz vêm do relógio do sistema (tempo_ms), não de um laço
 * cego. Desde o início (ou a última pausa) deveriam ter passado
 * (agora - t0) * 60 / 1000 quadros; o laço roda os que faltam, cada um com
 * as instruções do quadro e um tique dos temporizadores, e dorme até o
 * próximo. O relógio do kernel anda de 10 em 10 ms, então às vezes saem
 * dois quadros juntos: a média fica nos 60 por segundo.
 *
 * O teclado: as 16 teclas do CHIP-8 são lidas como teclas seguradas
 * (SIS_TECLAS_SEGURADAS, a mesma chamada do Sagui 3D), pelo código de
 * varredura, isto é, pela posição física. 1234/QWER/ASDF/ZXCV ficam no
 * mesmo lugar no teclado espanhol e no ABNT2, e o EX9E/EXA1 enxergam a
 * tecla enquanto ela está apertada, não só no instante do evento.
 *
 * O som: com placa de som (D088-D090), o padrão de 128 bits do XO-CHIP
 * toca de verdade: a cada quadro em que o temporizador de som passa de
 * zero saem 800 amostras (48 kHz / 60) lendo o padrão bit a bit na taxa
 * do FX3A (1 = +A, 0 = -A); sem padrão (CHIP-8, SUPER-CHIP), uma onda
 * quadrada de 440 Hz. Os quadros sem som mandam silêncio, então o tempo
 * entre um som e outro sai exato. Sem placa, o alto-falante do PC
 * (SIS_BIP) toca a frequência fundamental do padrão (ver c8_freq_som). */
#include "sagui.h"
#include "gui.h"
#include "chip8.h"

#define AUDIO_QUADRO 800     /* amostras por quadro: 48000 / 60 */
#define LARG_INICIO 640
#define ALT_INICIO  (320 + ESTADO)
#define LARG_MIN 320
#define ALT_MIN  200
#define ESTADO 22
#define PAINEL 240
#define MAX_ROMS 128
#define PASTA_ROMS "/jogos/chip8"
/* Teclas de função (o abi.h só define as que o sistema usa). */
#define TECLA_F2  0x10B
#define TECLA_F5  0x10E
#define TECLA_F10 0x113

typedef struct { const char *nome; uint32_t cor[4]; uint32_t borda; } paleta_t;

/* A primeira tem as cores do SaguiOS (o verde e o amarelo da banana). Na
 * ordem: apagado, plano 1, plano 2, os dois planos (XO-CHIP). */
static const paleta_t paletas[] = {
    { "SaguiOS",   { 0x10201A, 0x3CDC6E, 0xFFD52E, 0xF5F5F0 }, 0x0A1510 },
    { "Clásica",   { 0x000000, 0xFFFFFF, 0xAAAAAA, 0x555555 }, 0x1A1A1A },
    { "Ámbar",     { 0x1C1000, 0xFFB000, 0x9C5A00, 0xFFE0A0 }, 0x120A00 },
    { "Fósforo",   { 0x001400, 0x33FF66, 0x14802E, 0xB0FFC0 }, 0x000C00 },
    { "Papel",     { 0xF4F4F0, 0x1E2328, 0x0B7A3E, 0x2A6FD8 }, 0xD8DAD4 },
    { "Atardecer", { 0x2B1B3A, 0xFF7F50, 0xFFD166, 0x8EE3EF }, 0x1E1229 },
};
#define QTD_PALETAS (int)(sizeof paletas / sizeof paletas[0])

/* Código de varredura (conjunto 1) de cada tecla do CHIP-8: 0-F. */
static const uint8_t cv_tecla[16] = {
    0x2D, 0x02, 0x03, 0x04,   /* X 1 2 3 */
    0x10, 0x11, 0x12, 0x1E,   /* Q W E A */
    0x1F, 0x20, 0x2C, 0x2E,   /* S D Z C */
    0x05, 0x13, 0x21, 0x2F,   /* 4 R F V */
};
/* Teclado numérico (-k numerico): 0-9 nas teclas de mesmo número. */
static const uint8_t cv_numerico[10] = { 0x52, 0x4F, 0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47, 0x48, 0x49 };

static jan_t *j;
static c8_t maq;
static c8_opcoes_t op;
static uint8_t *rom;
static int tam_rom;
static char caminho[200];
static bool tem_rom;
static int modo = C8_CHIP8;
static int ipf;
static int paleta;
static bool tela_cheia, pausado, depurador, numerico;
static unsigned t0, feitos;           /* relógio dos 60 Hz */
static bool redesenhar_tudo = true;
static uint32_t freq_tocando;
static unsigned bip_desde;
static uint16_t teclas_antes;
static char aviso[120];

/* medida de instruções por segundo */
static unsigned ips, ips_desde, ips_instr;

/* seletor de ROMs */
static lista_t seletor;
static char roms[MAX_ROMS][64];

/* ------------------------------------------------------------ geometria */

static int area_l(void) { int l = j->larg - (depurador && !tela_cheia ? PAINEL : 0); return l > 1 ? l : 1; }
static int area_a(void) { int a = j->alt - (tela_cheia ? 0 : ESTADO); return a > 1 ? a : 1; }

static int escala(void)
{
    int e1 = area_l() / maq.larg, e2 = area_a() / maq.alt;
    int e = e1 < e2 ? e1 : e2;
    return e < 1 ? 1 : e;
}

/* ------------------------------------------------------------ desenho */

/* Redesenha as linhas da máquina que mudaram (todas, depois de mudar o
 * tamanho ou a paleta): no QEMU sem KVM, ampliar a tela inteira a cada
 * quadro custava mais que os 16 ms do quadro. */
static void desenhar_tela(void)
{
    const paleta_t *p = &paletas[paleta];
    int al = area_l(), aa = area_a(), e = escala();
    int l = maq.larg * e, a = maq.alt * e;
    int ox = (al - l) / 2, oy = (aa - a) / 2;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    uint32_t borda = tela_cheia ? 0 : p->borda;
    uint64_t sujas = redesenhar_tudo ? ~0ull : maq.linhas_sujas;
    if (redesenhar_tudo) {
        g_retangulo(j, 0, 0, al, oy, borda);
        g_retangulo(j, 0, oy + a, al, aa - oy - a, borda);
        g_retangulo(j, 0, oy, ox, a, borda);
        g_retangulo(j, ox + l, oy, al - ox - l, a, borda);
    }
    int vis_l = l < al ? l : al;
    int prim = -1, ult = -1;
    for (int y = 0; y < maq.alt; y++) {
        if (!(sujas >> y & 1)) continue;
        int py = oy + y * e;
        if (py >= aa) break;
        if (prim < 0) prim = y;
        ult = y;
        uint32_t *linha = j->px + py * j->larg + ox;
        const uint8_t *t = &maq.tela[y * maq.larg];
        for (int x = 0, px = 0; x < maq.larg && px < vis_l; x++, px += e)
            preencher32(linha + px, p->cor[t[x] & 3], (size_t)(px + e <= vis_l ? e : vis_l - px));
        for (int k = 1; k < e && py + k < aa; k++) memcpy(linha + k * j->larg, linha, (size_t)vis_l * 4);
    }
    if (redesenhar_tudo) jan_mostrar_area(j, 0, 0, al, aa);
    else if (prim >= 0) {
        int y0 = oy + prim * e, y1 = oy + (ult + 1) * e;
        if (y1 > aa) y1 = aa;
        jan_mostrar_area(j, ox, y0, vis_l, y1 - y0);
    }
    maq.tela_mudou = false;
    maq.linhas_sujas = 0;
}

static const char *nome_curto(const char *c)
{
    const char *b = acha_ultimo_char(c, '/');
    return b ? b + 1 : c;
}

static void desenhar_estado(void)
{
    if (tela_cheia) return;
    int y = j->alt - ESTADO;
    g_retangulo(j, 0, y, j->larg, ESTADO, 0xE6E8EA);
    g_retangulo(j, 0, y, j->larg, 1, COR_BORDA);
    char t[200];
    uint32_t cor = COR_TEXTO;
    if (aviso[0]) { copia_texto(t, aviso, sizeof t); cor = 0xB02020; }
    else if (!tem_rom) copia_texto(t, "Ninguna ROM abierta  |  Ctrl+O abrir  |  clic derecho: menú", sizeof t);
    else if (maq.estado == C8_ERRO) { formatar_texto(t, sizeof t, "%s  |  F5 reinicia", maq.erro); cor = 0xB02020; }
    else if (maq.estado == C8_TERMINOU) formatar_texto(t, sizeof t, "%s  |  programa terminado (00FD)  |  F5 reinicia", nome_curto(caminho));
    else
        formatar_texto(t, sizeof t, "%s  |  %d inst/cuadro  |  %u IPS  |  %s%s  |  P pausa  F5 reinicio  F10 depurador",
                       c8_nome_modo(modo), ipf, pausado ? 0 : ips, nome_curto(caminho), pausado ? "  |  PAUSA" : "");
    g_texto_n(j, 8, y + 3, t, (j->larg - 16) / LETRA_L, cor);
    jan_mostrar_area(j, 0, y, j->larg, ESTADO);
}

static void linha_painel(int x, int *y, uint32_t cor, const char *t)
{
    g_texto_n(j, x, *y, t, (PAINEL - 16) / LETRA_L, cor);
    *y += LETRA_A;
}

static void desenhar_painel(void)
{
    if (!depurador || tela_cheia) return;
    int x0 = j->larg - PAINEL, a = area_a();
    g_retangulo(j, x0, 0, PAINEL, a, 0x1B2128);
    g_retangulo(j, x0, 0, 1, a, 0x3A434C);
    int x = x0 + 10, y = 8;
    uint32_t claro = 0xE8ECEF, fraco = 0x8C96A0, verde = 0x3CDC6E;
    char t[64];
    linha_painel(x, &y, verde, "Depurador (F10)");
    y += 4;
    formatar_texto(t, sizeof t, "PC %03x   I %04x", maq.pc, maq.i);
    linha_painel(x, &y, claro, t);
    for (int r = 0; r < 16; r += 4) {
        formatar_texto(t, sizeof t, "V%x %02x V%x %02x V%x %02x V%x %02x", r, maq.v[r], r + 1, maq.v[r + 1],
                       r + 2, maq.v[r + 2], r + 3, maq.v[r + 3]);
        linha_painel(x, &y, claro, t);
    }
    formatar_texto(t, sizeof t, "DT %02x  ST %02x  SP %d", maq.dt, maq.st, maq.sp);
    linha_painel(x, &y, claro, t);
    int n = formatar_texto(t, sizeof t, "Pila:");
    for (int k = maq.sp - 1; k >= 0 && k >= maq.sp - 4; k--) n += formatar_texto(t + n, sizeof t - (size_t)n, " %03x", maq.pilha[k]);
    linha_painel(x, &y, claro, t);
    y += 6;
    /* Desmontagem: começa umas instruções antes do PC (o CHIP-8 não marca
     * onde cada instrução começa; os opcodes alinhados a 2 bytes quase
     * sempre acertam). */
    int linhas = (a - y - 40) / LETRA_A;
    if (linhas > 16) linhas = 16;
    uint32_t end = (maq.pc - 2 * (uint32_t)(linhas / 3)) & maq.mascara;
    if (maq.pc < 2 * (linhas / 3) + C8_INICIO && maq.pc >= C8_INICIO) end = C8_INICIO;
    for (int k = 0; k < linhas; k++) {
        char ins[40];
        int tam = c8_desmontar(maq.mem, maq.mascara, end, ins, sizeof ins);
        bool atual = end == maq.pc;
        formatar_texto(t, sizeof t, "%c%03x %s", atual ? '>' : ' ', end, ins);
        if (atual) g_retangulo(j, x0 + 4, y, PAINEL - 8, LETRA_A, 0x2A4E3A);
        linha_painel(x, &y, atual ? 0xFFFFFF : claro, t);
        end = (end + (uint32_t)tam) & maq.mascara;
    }
    y = a - 36;
    linha_painel(x, &y, fraco, pausado ? "N: paso  P: continuar" : "P: pausa (y paso a paso)");
    jan_mostrar_area(j, x0, 0, PAINEL, a);
}

static void desenhar_seletor(void)
{
    int al = area_l(), aa = area_a();
    g_retangulo(j, 0, 0, al, aa, COR_FUNDO);
    g_texto(j, 16, 10, "Elige una ROM (doble clic o Intro) o pulsa Ctrl+O.", COR_TEXTO);
    seletor.x = 16;
    seletor.y = 34;
    seletor.l = al - 32 < 160 ? 160 : al - 32;
    seletor.a = aa - 44 < 44 ? 44 : aa - 44;
    if (seletor.qtd == 0) g_texto(j, 16, 38, "No hay ROM en " PASTA_ROMS, COR_TEXTO_FRACO);
    else lista_desenhar(j, &seletor);
    jan_mostrar_area(j, 0, 0, al, aa);
}

static void desenhar_tudo(void)
{
    redesenhar_tudo = true;
    if (tem_rom) desenhar_tela(); else desenhar_seletor();
    redesenhar_tudo = false;
    desenhar_painel();
    desenhar_estado();
}

static void atualizar_titulo(void)
{
    char t[120];
    if (tem_rom) formatar_texto(t, sizeof t, "%s - CHIP-8", nome_curto(caminho));
    else copia_texto(t, "CHIP-8", sizeof t);
    titulo_janela(j->id, t);
}

/* ------------------------------------------------------------ som */

static int audio = -1;                    /* fluxo na placa de som; -1 = alto-falante */
static uint32_t audio_fase;               /* posição no padrão, em bits, 16.16 */
static int16_t audio_buf[2 * AUDIO_QUADRO];
static const uint8_t quadrada[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static void calar(void)
{
    if (audio >= 0) { audio_controle(audio, AUDIO_DESCARTAR, 0); return; }
    if (freq_tocando) silenciar();
    freq_tocando = 0;
}

/* Um quadro de som (ou de silêncio) na placa. Se a fila está curta (o
 * começo, ou depois de um engasgo), manda dois quadros para recompor a
 * folga; se está longa demais, pula este. */
static void som_quadro(void)
{
    if (audio < 0) return;
    int pend = audio_pendente(audio);
    if (pend > 250) return;
    int n = pend < 140 ? 2 * AUDIO_QUADRO : AUDIO_QUADRO;
    bool ativo = maq.st && !pausado;
    const uint8_t *pad = quadrada;
    uint32_t taxa = 440 * 128;
    if (maq.modo == C8_XOCHIP && maq.tem_padrao) { pad = maq.padrao; taxa = c8_taxa_padrao(&maq); }
    uint32_t passo = (taxa / 48000 << 16) + (taxa % 48000 << 16) / 48000;
    for (int i = 0; i < n; i++) {
        if (ativo) {
            uint32_t b = (audio_fase >> 16) & 127;
            audio_buf[i] = (pad[b >> 3] >> (7 - (b & 7))) & 1 ? 6000 : -6000;
            audio_fase += passo;
        } else audio_buf[i] = 0;
    }
    audio_escrever_sem_bloquear(audio, audio_buf, (unsigned)n * 2);
}

static void atualizar_som(void)
{
    if (audio >= 0) return;
    uint32_t f = pausado ? 0 : c8_freq_som(&maq);
    unsigned agora = tempo_ms();
    if (f == freq_tocando && (!f || agora - bip_desde < 500)) return;
    if (!f) { calar(); return; }
    /* Nota longa renovada a cada meio segundo: se o programa morrer, o som para sozinho. */
    bip((int)f, 1000);
    freq_tocando = f;
    bip_desde = agora;
}

/* ------------------------------------------------------------ ROM */

static void reiniciar_relogio(void)
{
    t0 = tempo_ms();
    feitos = 0;
    ips_desde = t0;
    ips_instr = maq.instrucoes;
}

static bool preparar(void)
{
    char erro[96];
    c8_opcoes_t o = op;
    if (!o.semente) o.semente = tempo_ms() | 1;
    if (!c8_preparar(&maq, &o, rom, tam_rom, modo, erro, sizeof erro)) {
        copia_texto(aviso, erro, sizeof aviso);
        tem_rom = false;
        return false;
    }
    calar();
    reiniciar_relogio();
    redesenhar_tudo = true;
    return true;
}

static bool abrir_rom(const char *c, int modo_forcado)
{
    int n;
    uint8_t *novo = c8_ler_arquivo(c, &n);
    if (!novo) {
        formatar_texto(aviso, sizeof aviso, "No se ha podido abrir %s", c);
        registrar("chip8: erro ao abrir %s\n", c);
        return false;
    }
    if (rom) libera(rom);
    rom = novo;
    tam_rom = n;
    copia_texto(caminho, c, sizeof caminho);
    modo = modo_forcado >= 0 ? modo_forcado : op.modo >= 0 ? op.modo : c8_modo_por_extensao(c);
    if (modo < 0) modo = C8_CHIP8;
    ipf = op.ipf ? op.ipf : c8_ipf_padrao(modo);
    aviso[0] = 0;
    tem_rom = true;
    pausado = false;
    if (!preparar()) return false;
    char q[160];
    c8_quirks_descrever(&maq.q, q, sizeof q);
    registrar("chip8: rom %s (%d bytes) modo %s, %d instr/cuadro, %s\n", caminho, tam_rom, c8_id_modo(modo), ipf, q);
    atualizar_titulo();
    return true;
}

/* Carpetas de ROM bajadas con chip8-roms (D123): en /home si es persistente y, si no, en /tmp. */
#define PASTA_BAIXADAS_1 "/home/chip8/roms"
#define PASTA_BAIXADAS_2 "/tmp/chip8/roms"

static char caminhos[MAX_ROMS][96];   /* ruta completa de cada ítem del selector */

/* Inserta un ítem en orden alfabético entre las posiciones inicio y qtd. */
static void inserir_rom(const char *texto, const char *caminho, int inicio)
{
    if (seletor.qtd >= MAX_ROMS) return;
    int k = seletor.qtd++;
    while (k > inicio && compara_texto(roms[k - 1], texto) > 0) {
        copia_texto(roms[k], roms[k - 1], 64);
        copia_texto(caminhos[k], caminhos[k - 1], 96);
        k--;
    }
    copia_texto(roms[k], texto, 64);
    copia_texto(caminhos[k], caminho, 96);
}

/* Busca "titulo": "..." en el .json que escribe chip8-roms; solo ASCII visible. */
static bool titulo_do_json(const char *caminho, char *saida, int max)
{
    int tam;
    uint8_t *d = c8_ler_arquivo(caminho, &tam);
    if (!d) return false;
    static const char chave[] = "\"titulo\": \"";
    bool ok = false;
    for (int i = 0; i + (int)sizeof chave < tam; i++) {
        int j = 0;
        while (j < (int)sizeof chave - 1 && d[i + j] == (uint8_t)chave[j]) j++;
        if (j != (int)sizeof chave - 1) continue;
        int n = 0;
        for (int p = i + j; p < tam && d[p] != '"' && n < max - 1; p++) {
            if (d[p] == '\\') { p++; continue; }
            saida[n++] = d[p] >= 32 && d[p] < 127 ? (char)d[p] : '?';
        }
        saida[n] = 0;
        ok = n > 0;
        break;
    }
    libera(d);
    return ok;
}

static void listar_pasta_baixadas(const char *pasta)
{
    int inicio = seletor.qtd;
    entrada_pasta_t e;
    for (int i = 0; listar(pasta, i, &e) > 0 && seletor.qtd < MAX_ROMS; i++) {
        if (e.tipo == TIPO_PASTA || c8_modo_por_extensao(e.nome) < 0) continue;
        char cam[96], chave[64], json[96], texto[64], titulo[48];
        formatar_texto(cam, sizeof cam, "%s/%s", pasta, e.nome);
        copia_texto(chave, e.nome, sizeof chave);
        char *ponto = acha_ultimo_char(chave, '.');
        if (ponto) *ponto = 0;
        formatar_texto(json, sizeof json, "%s/%s.json", pasta, chave);
        if (titulo_do_json(json, titulo, sizeof titulo)) formatar_texto(texto, sizeof texto, "+ %s", titulo);
        else formatar_texto(texto, sizeof texto, "+ %s", chave);
        inserir_rom(texto, cam, inicio);
    }
}

static void listar_roms(void)
{
    seletor.qtd = 0;
    entrada_pasta_t e;
    for (int i = 0; listar(PASTA_ROMS, i, &e) > 0 && seletor.qtd < MAX_ROMS; i++) {
        if (e.tipo == TIPO_PASTA || c8_modo_por_extensao(e.nome) < 0) continue;
        char cam[96];
        formatar_texto(cam, sizeof cam, "%s/%s", PASTA_ROMS, e.nome);
        inserir_rom(e.nome, cam, 0);
    }
    /* las bajadas con chip8-roms van después, marcadas con "+" y con su título */
    listar_pasta_baixadas(PASTA_BAIXADAS_1);
    listar_pasta_baixadas(PASTA_BAIXADAS_2);
    seletor.itens = roms;
    seletor.sel = seletor.qtd ? 0 : -1;
    seletor.topo = 0;
    seletor.icone = NULL;
}

static void abrir_do_seletor(void)
{
    if (seletor.sel < 0) return;
    abrir_rom(caminhos[seletor.sel], -1);
    desenhar_tudo();
}

static void pedir_arquivo(void)
{
    char c[200];
    if (tem_rom) copia_texto(c, caminho, sizeof c);
    else copia_texto(c, PASTA_ROMS "/", sizeof c);
    bool estava = pausado;
    pausado = true;
    calar();
    if (dialogo_abrir("Abrir ROM", "ROM de CHIP-8 (*.ch8;*.sc8;*.xo8)|*.ch8;*.sc8;*.xo8|Todos los archivos (*.*)|*", c, c, sizeof c) && c[0])
        abrir_rom(c, -1);
    else pausado = estava;
    reiniciar_relogio();
    desenhar_tudo();
}

/* ------------------------------------------------------------ comandos */

static void pausar(bool p)
{
    if (!tem_rom || p == pausado) return;
    pausado = p;
    if (p) calar();
    else reiniciar_relogio();
    registrar("chip8: %s, pantalla %08x\n", p ? "pausa" : "continua", c8_soma_tela(&maq));
    desenhar_painel();
    desenhar_estado();
}

static void reiniciar_rom(void)
{
    if (!tem_rom) return;
    pausado = false;   /* também depois de um erro ou do 00FD, que pausam */
    preparar();
    registrar("chip8: reinicio\n");
    desenhar_tudo();
}

static void mudar_modo(int m)
{
    if (!tem_rom) return;
    char c[200];
    copia_texto(c, caminho, sizeof c);
    abrir_rom(c, m);
    registrar("chip8: modo %s\n", c8_id_modo(m));
    desenhar_tudo();
}

static void mudar_paleta(int p)
{
    paleta = (p + QTD_PALETAS) % QTD_PALETAS;
    registrar("chip8: paleta %d %s\n", paleta, paletas[paleta].nome);
    desenhar_tudo();
}

static void mudar_velocidade(int sentido)
{
    if (!tem_rom) return;
    int n = ipf;
    /* Passos de uns 25%: de 1 até 100000 em algumas dezenas de toques. */
    if (sentido > 0) n = n < 4 ? n + 1 : n + n / 4;
    else n = n <= 4 ? n - 1 : n - n / 5;
    if (n < 1) n = 1;
    if (n > 100000) n = 100000;
    ipf = n;
    registrar("chip8: velocidad %d instr/cuadro\n", ipf);
    desenhar_estado();
}

static void alternar_depurador(void)
{
    depurador = !depurador;
    registrar("chip8: depurador %s\n", depurador ? "si" : "no");
    desenhar_tudo();
}

static void passo_a_passo(void)
{
    if (!tem_rom || !pausado || maq.estado != C8_RODANDO) return;
    maq.vblank = false;
    if (c8_executar(&maq, 1) == 0 && maq.espera_reg >= 0) copia_texto(aviso, "Esperando una tecla (FX0A)", sizeof aviso);
    registrar("chip8: paso, PC %03x\n", maq.pc);
    if (maq.tela_mudou) desenhar_tela();
    desenhar_painel();
    desenhar_estado();
    aviso[0] = 0;
}

#define MENU_ITENS 18
static void abrir_menu(int x, int y)
{
    char m[600];
    int n = formatar_texto(m, sizeof m, "Abrir ROM...\tCtrl+O\n%sReiniciar\tF5\n%s\tP\nDepurador\tF10\n-\n",
                           tem_rom ? "" : "~", !tem_rom ? "~Pausa" : pausado ? "Continuar" : "Pausa");
    for (int k = 0; k < C8_QTD_MODOS; k++)
        n += formatar_texto(m + n, sizeof m - (size_t)n, "%s%s %s\n", tem_rom ? "" : "~", k == modo && tem_rom ? "*" : "  ",
                            c8_nome_modo(k));
    n += formatar_texto(m + n, sizeof m - (size_t)n, "-\n");
    for (int k = 0; k < QTD_PALETAS; k++)
        n += formatar_texto(m + n, sizeof m - (size_t)n, "%s Paleta %s%s\n", k == paleta ? "*" : "  ", paletas[k].nome,
                            k == 0 ? "\tF2" : "");
    formatar_texto(m + n, sizeof m - (size_t)n, "-\n%sMás rápido\t+\n%sMás lento\t-", tem_rom ? "" : "~", tem_rom ? "" : "~");
    menu_contextual(j->id, x, y, m, 1);
}

static void escolha_menu(int i)
{
    switch (i) {
    case 0: pedir_arquivo(); break;
    case 1: reiniciar_rom(); break;
    case 2: pausar(!pausado); break;
    case 3: alternar_depurador(); break;
    case 5: case 6: case 7: mudar_modo(i - 5); break;
    default:
        if (i >= 9 && i < 9 + QTD_PALETAS) mudar_paleta(i - 9);
        else if (i == 10 + QTD_PALETAS) mudar_velocidade(1);
        else if (i == 11 + QTD_PALETAS) mudar_velocidade(-1);
    }
}

/* ------------------------------------------------------------ eventos */

static void tecla(unsigned t)
{
    if (t == 15) { pedir_arquivo(); return; }   /* Ctrl+O */
    if (t == TECLA_F10) { alternar_depurador(); return; }
    if (t == TECLA_F2) { mudar_paleta(paleta + 1); return; }
    if (!tem_rom) {
        evento_t e = { .tipo = EV_TECLA, .tecla = t };
        if (lista_tratar(j, &seletor, &e) == LISTA_ATIVOU) abrir_do_seletor();
        return;
    }
    switch (t) {
    case 'p': case 'P': pausar(!pausado); break;
    case 'n': case 'N': passo_a_passo(); break;
    case TECLA_F5: reiniciar_rom(); break;
    case '+': mudar_velocidade(1); break;
    case '-': mudar_velocidade(-1); break;
    }
}

/* Devolve false para fechar. */
static bool tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) return false;
    if (e->tipo == EV_REDIMENSIONAR) {
        bool cheia = e->tecla == JAN_ESTADO_TELA_CHEIA;
        if (jan_redimensionar(j, e)) {
            tela_cheia = cheia;
            registrar("chip8: ventana %dx%d escala %d%s\n", j->larg, j->alt, escala(),
                      tela_cheia ? " (pantalla completa)" : "");
            desenhar_tudo();
        }
        return true;
    }
    if (e->tipo == EV_MENU) { if (e->x >= 0) escolha_menu(e->x); return true; }
    if (e->tipo == EV_CLIQUE && (e->botoes & 2)) { abrir_menu(e->x, e->y); return true; }
    if (e->tipo == EV_TECLA) { tecla(e->tecla); return true; }
    if (!tem_rom && e->x < area_l() && e->y < area_a()) {
        if (lista_tratar(j, &seletor, e) == LISTA_ATIVOU) abrir_do_seletor();
    }
    return true;
}

static uint16_t ler_teclado(void)
{
    unsigned char m[32];
    if (teclas_seguradas(m) != 1) return 0;
    uint16_t t = 0;
    for (int k = 0; k < 16; k++) if (SEGURADA(m, cv_tecla[k])) t |= (uint16_t)(1 << k);
    /* As setas valem 5, 7, 8 e 9 (cima, esquerda, baixo, direita), como W A S D. */
    if (SEGURADA(m, CV_CIMA)) t |= 1 << 5;
    if (SEGURADA(m, CV_ESQUERDA)) t |= 1 << 7;
    if (SEGURADA(m, CV_BAIXO)) t |= 1 << 8;
    if (SEGURADA(m, CV_DIREITA)) t |= 1 << 9;
    if (numerico)
        for (int k = 0; k < 10; k++) if (SEGURADA(m, cv_numerico[k])) t |= (uint16_t)(1 << k);
    return t;
}

/* Roda os quadros que o relógio pede. */
static void emular(void)
{
    unsigned agora = tempo_ms();
    unsigned devidos = (agora - t0) * 60 / 1000;
    if (devidos - feitos > 6) feitos = devidos - 6;   /* depois de um engasgo, não corre atrás */
    while (feitos < devidos && maq.estado == C8_RODANDO) {
        uint16_t t = ler_teclado();
        if (t != teclas_antes) { registrar("chip8: teclas %04x\n", t); teclas_antes = t; }
        c8_teclas(&maq, t);
        c8_executar(&maq, ipf);
        som_quadro();
        c8_quadro(&maq);
        feitos++;
    }
    if (maq.estado != C8_RODANDO) {
        calar();
        registrar("chip8: %s\n", maq.estado == C8_ERRO ? maq.erro : "terminado (00FD)");
        pausado = true;
        desenhar_painel();
        desenhar_estado();
    }
    if (agora - ips_desde >= 1000) {
        ips = (maq.instrucoes - ips_instr) * 1000 / (agora - ips_desde);
        ips_desde = agora;
        ips_instr = maq.instrucoes;
        registrar("chip8: %u IPS, pantalla %08x\n", ips, c8_soma_tela(&maq));
        desenhar_estado();
        if (depurador) desenhar_painel();
    }
    atualizar_som();
}

int principal(int argc, char **argv)
{
    if (!c8_ler_opcoes(argc, argv, &op)) return 2;
    if (op.quadros) return c8_relatorio(&op);
    if (op.paleta) {
        int n = texto_para_int(op.paleta);
        paleta = n >= 0 && n < QTD_PALETAS && op.paleta[0] >= '0' && op.paleta[0] <= '9' ? n : 0;
        for (int k = 0; k < QTD_PALETAS; k++) if (compara_texto(op.paleta, paletas[k].nome) == 0) paleta = k;
        if (compara_texto(op.paleta, "saguios") == 0) paleta = 0;
        if (compara_texto(op.paleta, "clasica") == 0) paleta = 1;
    }
    if (op.teclado) {
        if (compara_texto(op.teclado, "numerico") == 0) numerico = true;
        else if (compara_texto(op.teclado, "es") != 0 && compara_texto(op.teclado, "abnt2") != 0) {
            imprimir_em(ERROS, "chip8: -k es, abnt2 o numerico\n");
            return 2;
        }
    }
    j = jan_abrir("CHIP-8", LARG_INICIO, ALT_INICIO);
    if (!j) return 1;
    audio = audio_abrir(48000, 1, 16);
    registrar("chip8: som %s\n", audio >= 0 ? "pela placa (padrao XO-CHIP)" : "pelo alto-falante");
    jan_estilo(j, JAN_REDIMENSIONAVEL, LARG_MIN, ALT_MIN);
    listar_roms();
    if (op.rom) abrir_rom(op.rom, -1);
    atualizar_titulo();
    desenhar_tudo();
    registrar("chip8: pronto (%s) janela %d\n", tem_rom ? caminho : "sin rom", j->id);

    evento_t e;
    bool continuar = true;
    while (continuar) {
        bool rodando = tem_rom && !pausado && maq.estado == C8_RODANDO;
        if (!rodando) {
            /* Parado: só acorda com um evento. */
            if (proximo_evento(j->id, &e, true) == 1) continuar = tratar(&e);
        }
        while (continuar && proximo_evento(j->id, &e, false) == 1) continuar = tratar(&e);
        if (!continuar) break;
        if (tem_rom && !pausado && maq.estado == C8_RODANDO) {
            emular();
            if (maq.tela_mudou) desenhar_tela();
            unsigned prox = t0 + (feitos + 1) * 1000 / 60, agora = tempo_ms();
            if ((int)(prox - agora) > 0) dormir(prox - agora);
        }
    }
    calar();
    if (audio >= 0) fechar(audio);
    registrar("chip8: fechado\n");
    jan_fechar(j);
    return 0;
}
