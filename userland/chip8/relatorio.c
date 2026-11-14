// SPDX-License-Identifier: GPL-2.0-or-later
/* relatorio.c - opções da linha de comando do chip8 e o modo de texto.
 *
 * "chip8 -t N rom" roda N quadros (cada um com as instruções do quadro e
 * um tique dos temporizadores), sem janela e sem teclas, e escreve o
 * estado: registros, pilha, a soma da tela e a tela em ASCII. O
 * aleatório começa sempre da mesma semente, então a saída é a mesma no
 * SaguiOS e no host: as provas comparam as duas (D070). */
#include "chip8.h"

int c8_ipf_padrao(int modo)
{
    /* ~660 instruções/s no CHIP-8 (o COSMAC fazia algo assim); o SCHIP no
     * HP-48 era mais rápido; o XO-CHIP (Octo) pressupõe uma máquina folgada. */
    return modo == C8_XOCHIP ? 1000 : modo == C8_SCHIP ? 30 : 11;
}

void c8_uso(void)
{
    c8_erro("uso: chip8 [-m chip8|schip|xochip] [-v instr/cuadro] [-q quirk=valor]... [-p paleta]\n"
            "            [-k es|abnt2|numerico] [-t cuadros] [rom]\n"
            "  -m   modo (por defecto, según la extensión: .ch8 .sc8 .xo8)\n"
            "  -v   instrucciones por cuadro (60 cuadros por segundo)\n"
            "  -q   desplazar=vy|vx memoria=incrementa|fija salto=v0|vx borde=recorta|envuelve\n"
            "       logicas=reinicia|conserva vblank=espera|no\n"
            "  -t   sin ventana: ejecuta N cuadros y muestra registros y pantalla\n"
            "  -e   con -t: en el cuadro Q las teclas pasan a ser T (hex, bit k = tecla k): -e Q:T\n"
            "Sin rom, muestra las de /jogos/chip8. Teclas y quirks en /ayuda/chip8.md.\n");
}

static bool numero(const char *s, int *v)
{
    if (!*s) return false;
    int n = 0;
    for (; *s; s++) { if (*s < '0' || *s > '9') return false; n = n * 10 + (*s - '0'); }
    *v = n;
    return true;
}

bool c8_ler_opcoes(int argc, char **argv, c8_opcoes_t *o)
{
    memset(o, 0, sizeof *o);
    o->modo = -1;
    for (int k = 1; k < argc; k++) {
        const char *a = argv[k];
        bool tem = k + 1 < argc;
        if (compara_texto(a, "-h") == 0 || compara_texto(a, "--ayuda") == 0) { c8_uso(); return false; }
        if (a[0] != '-' || !a[1]) {
            if (o->rom) { c8_errof("chip8: sobra el argumento %s\n", a); return false; }
            o->rom = a;
            continue;
        }
        if (!tem) { c8_errof("chip8: falta el valor de %s\n", a); return false; }
        const char *v = argv[++k];
        int n;
        if (compara_texto(a, "-m") == 0) {
            o->modo = c8_modo_por_nome(v);
            if (o->modo < 0) { c8_errof("chip8: modo desconocido '%s' (chip8, schip o xochip)\n", v); return false; }
        } else if (compara_texto(a, "-v") == 0) {
            if (!numero(v, &n) || n < 1 || n > 100000) { c8_errof("chip8: -v necesita un número de 1 a 100000\n"); return false; }
            o->ipf = n;
        } else if (compara_texto(a, "-t") == 0) {
            if (!numero(v, &n) || n < 1) { c8_errof("chip8: -t necesita un número de cuadros\n"); return false; }
            o->quadros = n;
        } else if (compara_texto(a, "-s") == 0) {
            if (!numero(v, &n)) { c8_errof("chip8: -s necesita un número\n"); return false; }
            o->semente = (uint32_t)n;
        } else if (compara_texto(a, "-q") == 0) {
            c8_quirks_t prova;
            if (!c8_quirk_texto(&prova, v)) { c8_errof("chip8: quirk no válido '%s'\n", v); return false; }
            if (o->qtd_quirks < 12) o->quirks[o->qtd_quirks++] = v;
        } else if (compara_texto(a, "-e") == 0) {
            if (o->qtd_eventos < 32) o->eventos[o->qtd_eventos++] = v;
        } else if (compara_texto(a, "-p") == 0) {
            o->paleta = v;
        } else if (compara_texto(a, "-k") == 0) {
            o->teclado = v;
        } else {
            c8_errof("chip8: opción desconocida %s\n", a);
            return false;
        }
    }
    return true;
}

bool c8_preparar(c8_t *c, const c8_opcoes_t *o, const uint8_t *rom, int tam, int modo, char *erro, int max)
{
    c8_quirks_t q;
    c8_quirks_padrao(modo, &q);
    for (int k = 0; k < o->qtd_quirks; k++) c8_quirk_texto(&q, o->quirks[k]);
    c8_iniciar(c, modo, &q, o->semente);
    if (!c8_carregar(c, rom, tam)) {
        formatar_texto(erro, (size_t)max, "La ROM tiene %d bytes y en %s caben %d", tam, c8_nome_modo(modo), c8_rom_max(modo));
        return false;
    }
    return true;
}

static c8_t maquina;

int c8_relatorio(const c8_opcoes_t *o)
{
    if (!o->rom) { c8_errof("chip8: -t necesita una ROM\n"); return 2; }
    int tam;
    uint8_t *rom = c8_ler_arquivo(o->rom, &tam);
    if (!rom) { c8_errof("chip8: no se ha podido leer %s\n", o->rom); return 1; }
    int modo = o->modo >= 0 ? o->modo : c8_modo_por_extensao(o->rom);
    if (modo < 0) modo = C8_CHIP8;
    c8_t *c = &maquina;
    char erro[96];
    if (!c8_preparar(c, o, rom, tam, modo, erro, sizeof erro)) { c8_errof("chip8: %s\n", erro); return 1; }
    int ipf = o->ipf ? o->ipf : c8_ipf_padrao(modo);
    int q = 0;
    for (; q < o->quadros && c->estado == C8_RODANDO; q++) {
        /* -e Q:T: a partir do quadro Q, as teclas apertadas são T. */
        for (int k = 0; k < o->qtd_eventos; k++) {
            const char *e = o->eventos[k];
            int quadro = 0, t = 0;
            for (; *e >= '0' && *e <= '9'; e++) quadro = quadro * 10 + (*e - '0');
            if (*e++ != ':' || quadro != q) continue;
            for (; *e; e++) t = t * 16 + (*e >= 'a' ? *e - 'a' + 10 : *e >= 'A' ? *e - 'A' + 10 : *e - '0');
            c8_teclas(c, (uint16_t)t);
        }
        c8_executar(c, ipf);
        c8_quadro(c);
    }
    const char *nome = acha_ultimo_char(o->rom, '/');
    char quirks[160];
    c8_quirks_descrever(&c->q, quirks, sizeof quirks);
    c8_saidaf("chip8: %s (%d bytes), modo %s, %d instrucciones por cuadro\n", nome ? nome + 1 : o->rom, tam,
              c8_nome_modo(modo), ipf);
    c8_saidaf("quirks: %s\n", quirks);
    const char *est = c->estado == C8_TERMINOU ? "terminado (00FD)" : c->estado == C8_ERRO ? "error"
                      : c->espera_reg >= 0 ? "esperando una tecla" : "ejecutando";
    c8_saidaf("cuadros: %d  instrucciones: %u  estado: %s\n", q, c->instrucoes, est);
    if (c->estado == C8_ERRO) c8_saidaf("error: %s\n", c->erro);
    c8_saidaf("PC %03x  I %04x  SP %d  DT %d  ST %d  planos %d  tono %d  sonido %u Hz\n", c->pc, c->i, c->sp, c->dt,
              c->st, c->planos, c->tom, c8_freq_som(c));
    char l[200];
    int n = formatar_texto(l, sizeof l, "V:");
    for (int k = 0; k < 16; k++) n += formatar_texto(l + n, sizeof l - (size_t)n, " %02x", c->v[k]);
    c8_saidaf("%s\n", l);
    n = formatar_texto(l, sizeof l, "pila:");
    for (int k = 0; k < c->sp; k++) n += formatar_texto(l + n, sizeof l - (size_t)n, " %03x", c->pilha[k]);
    c8_saidaf("%s\n", l);
    n = formatar_texto(l, sizeof l, "rpl:");
    for (int k = 0; k < 16; k++) n += formatar_texto(l + n, sizeof l - (size_t)n, " %02x", c->rpl[k]);
    c8_saidaf("%s\n", l);
    c8_saidaf("pantalla %dx%d suma %08x\n", c->larg, c->alt, c8_soma_tela(c));
    /* A tela: '.' apagado, '#' plano 1, e no XO-CHIP '2' só no plano 2 e '3' nos dois. */
    for (int y = 0; y < c->alt; y++) {
        for (int x = 0; x < c->larg; x++) l[x] = ".#23"[c->tela[y * c->larg + x] & 3];
        l[c->larg] = '\n';
        l[c->larg + 1] = 0;
        c8_saida(l);
    }
    libera(rom);
    return c->estado == C8_ERRO ? 1 : 0;
}

int c8_principal_texto(int argc, char **argv)
{
    c8_opcoes_t o;
    if (!c8_ler_opcoes(argc, argv, &o)) return 2;
    if (!o.quadros) { c8_errof("chip8: esta versión solo tiene el modo -t\n"); return 2; }
    return c8_relatorio(&o);
}
