// SPDX-License-Identifier: GPL-3.0-only
/* sobre-o-sistema - la ventana "Acerca de SaguiOS-Extended" (H28, D101): versión, tamaño de
 * la imagen, procesos activos, memoria y créditos. Se actualiza cada segundo. */
#include "sagui.h"
#include "gui.h"
#include "versao.h"
#include "compilacao.h"   /* data, hora e commit da compilação (D126) */

static const char *const cabeca[16] = {
    "...........EEEEEEEEEE...........",
    ".........EEEEEEEEEEEEEE.........",
    "..BBB...EEEEEBBBBBEEEEEE..BBB...",
    ".BBBBB.EEEEEEEBBBEEEEEEEEBBBBB..",
    "BBBBBBBEEEEEEEEBEEEEEEEEEBBBBBB.",
    "BBBBBBEEERRRRRRRRRRRRRREEEBBBBB.",
    ".BBBBBEERRPPPRRRRRRPPPRREEBBBBB.",
    "..BBBBEERRPbPRRRRRRPbPRREEBBBB..",
    "...BBBEERRPPPRRRRRRPPPRREEBBB...",
    ".....EEERRRRRRRRRRRRRRRREEE.....",
    "......EERRRRRRPPPRRRRRRREE......",
    "......EEERRRRRRRRRRRRRREEE......",
    ".......EEERRRRPPPPRRRREEE.......",
    "........EEEERRRRRRRREEEE........",
    ".......WWEEEEEEEEEEEEEEWW.......",
    ".....wWWWWWWWWWWWWWWWWWWWWw.....",
};

static uint32_t paleta(char c)
{
    switch (c) {
    case 'E': return 0x3C2D23; case 'B': return 0xF5F5F0; case 'R': return 0xAA7864;
    case 'P': return 0x141210; case 'b': return 0xFFFFFF;
    case 'W': return 0xC8202A; case 'w': return 0x7A1218;   /* capa roja lisa (D101) */
    default: return 0xFF000000;
    }
}

static void informacoes(jan_t *j)
{
    info_versao_t v;
    info_memoria_t mem;
    static info_processo_t p[64];
    info_sistema(INFO_VERSAO, &v, sizeof v);
    info_sistema(INFO_MEMORIA, &mem, sizeof mem);
    int procs = info_sistema(INFO_PROCESSOS, p, 64);
    char t[96];
    g_retangulo(j, 20, 128, 400, 164, COR_FUNDO);
    formatar_texto(t, sizeof t, _("Versión:             %s"), v.versao);
    g_texto(j, 20, 128, t, COR_TEXTO);
    /* Tamaño en KB con una cifra decimal, solo con enteros. */
    /* meta de 38 MB = 39845888 bytes (D181): arranque + particiones del sistema; décimas de % */
    unsigned kb10 = v.imagem_bytes / 1024 * 10 + v.imagem_bytes % 1024 * 10 / 1024, pct10 = v.imagem_bytes / 39846;
    formatar_texto(t, sizeof t, _("Tamaño de imagen:    %u,%u KB (%u,%u%% de 38 MB)"), kb10 / 10, kb10 % 10, pct10 / 10, pct10 % 10);
    g_texto(j, 20, 146, t, COR_TEXTO);
    formatar_texto(t, sizeof t, _("Procesos activos:    %d"), procs);
    g_texto(j, 20, 164, t, COR_TEXTO);
    formatar_texto(t, sizeof t, _("Memoria libre:       %u MB de %u MB"), mem.livre_kb / 1024, mem.gerida_kb / 1024);
    g_texto(j, 20, 182, t, COR_TEXTO);
    formatar_texto(t, sizeof t, _("Pantalla:            %ux%u"), v.largura, v.altura);
    g_texto(j, 20, 200, t, COR_TEXTO);
    formatar_texto(t, sizeof t, _("Encendido hace:      %u min %02u s"), v.segundos_ligado / 60, v.segundos_ligado % 60);
    g_texto(j, 20, 218, t, COR_TEXTO);
    /* Datos de la compilación (D126): fecha y hora, commit y variante. */
    formatar_texto(t, sizeof t, _("Compilación:         %s %s"), SAGUI_BUILD_DATA, SAGUI_BUILD_HORA);
    g_texto(j, 20, 236, t, COR_TEXTO);
    formatar_texto(t, sizeof t, _("Commit:              %s"), SAGUI_BUILD_COMMIT);
    g_texto(j, 20, 254, t, COR_TEXTO);
    formatar_texto(t, sizeof t, _("Variante:            %s"), SAGUI_VARIANTE);
    g_texto(j, 20, 272, t, COR_TEXTO);
    jan_mostrar_area(j, 20, 128, 400, 164);
    static bool registrado;
    if (!registrado) registrar("sobre: versao %s, imagem %u bytes, %d processos, compilacao %s %s commit %s",
                                v.versao, v.imagem_bytes, procs, SAGUI_BUILD_DATA, SAGUI_BUILD_HORA, SAGUI_BUILD_COMMIT);
    registrado = true;
}

/* Dos páginas: información del sistema y créditos (D101). */

/* Las líneas de los créditos. El número de líneas sale del propio array, para
 * no tener que contarlas a mano: si se contaran mal, el bucle leería de más y
 * el programa se iría al traste (D126). */
static const char *const creditos[] = {   /* se traducen al dibujarlos (las que llevan # son nombres propios) */
    N_("Basado en SaguiOS, proyecto original de"),
    "#Eli Rigobeli (github.com/erigobeli/sagui-os),",
    N_("escrito por Claude (Anthropic)."),
    "",
    N_("Red (driver e1000, TCP/IP) y navegador Cipó:"),
    "#M4R4G0N",
    "",
    SAGUI_NOME ": mantenido por",
    "#Alejandro Anuncibay,",
    N_("con mejoras escritas con Claude (Anthropic)"),
    N_("y con un agente de IA (MiniMax Code)."),
    "",
    N_("Código de terceros: MicroPython (MIT), libm"),
    N_("(musl/FreeBSD), partes de drivers de Linux"),
    N_("(GPLv2). Ver docs/terceros.md."),
    "",
};

static void pagina_creditos(jan_t *j)
{
    /* Cada línea tiene como máximo 50 letras (400 píxeles); '#' al comienzo marca
     * un nombre propio, dibujado en destaque; "" deja un espacio. */
    const char *const *l = creditos;
    int n_linhas = (int)(sizeof creditos / sizeof creditos[0]);
    g_retangulo(j, 0, 112, 440, 300, COR_FUNDO);
    g_texto(j, 20, 118, _("Créditos"), COR_DESTAQUE);
    int y = 140;
    for (int i = 0; i < n_linhas; i++) {
        const char *t = l[i];
        if (t[0] && t[0] != '#') t = _(t);
        uint32_t cor = COR_TEXTO;
        if (t[0] == '#') { t++; cor = COR_DESTAQUE; }
        if (t[0]) g_texto(j, 20, y, t, cor);
        y += t[0] ? 15 : 7;
    }
    jan_mostrar_area(j, 0, 112, 440, 300);
}

static void pagina_info(jan_t *j)
{
    g_retangulo(j, 0, 112, 440, 300, COR_FUNDO);
    g_texto(j, 20, 302, _("Créditos"), COR_DESTAQUE);
    g_texto(j, 20, 320, _("Basado en SaguiOS, de Eli Rigobeli (Claude,"), COR_TEXTO);
    g_texto(j, 20, 338, _("Anthropic). Mantenido por Alejandro Anuncibay,"), COR_TEXTO);
    g_texto(j, 20, 356, _("con mejoras escritas con Claude (Anthropic)"), COR_TEXTO);
    g_texto(j, 20, 374, _("y con un agente de IA (MiniMax Code)."), COR_TEXTO);
    g_texto(j, 20, 392, _("Pulsa \"Créditos\" para ver los créditos"), COR_TEXTO);
    informacoes(j);
    jan_mostrar_area(j, 0, 112, 440, 300);
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    jan_t *j = jan_abrir(_("Acerca de SaguiOS-Extended"), 440, 460);
    if (!j) return 1;
    g_retangulo(j, 0, 0, 440, 110, 0x0B7A3E);
    g_desenho(j, 20, 14, cabeca, 32, 16, paleta, 5);
    g_texto(j, 200, 22, SAGUI_NOME, 0xFFDF4A);
    g_texto(j, 200, 42, SAGUI_VERSAO, 0xFFDF4A);
    g_texto(j, 200, 64, _("un sistema operativo"), 0xFFFFFF);
    g_texto(j, 200, 82, _("escrito desde cero"), 0xFFFFFF);
    bool creditos = false;
    botao_t fechar = { 440 - 110, 420, 90, 26, _("Cerrar"), false };
    botao_t trocar = { 20, 420, 110, 26, _("Créditos"), false };
    botao_desenhar(j, &fechar);
    botao_desenhar(j, &trocar);
    pagina_info(j);
    jan_mostrar(j);
    evento_t e;
    unsigned ultimo = tempo_ms();
    for (;;) {
        if (proximo_evento(j->id, &e, false) == 1) {
            if (e.tipo == EV_FECHAR) break;
            if (botao_tratar(j, &fechar, &e)) break;
            if (botao_tratar(j, &trocar, &e)) {
                creditos = !creditos;
                trocar.texto = creditos ? _("Información") : _("Créditos");
                g_retangulo(j, 20, 420, 110, 26, COR_FUNDO);
                botao_desenhar(j, &trocar);
                jan_mostrar_area(j, 20, 420, 110, 26);
                registrar("sobre: pagina %s", creditos ? "creditos" : "informacao");
                if (creditos) pagina_creditos(j); else pagina_info(j);
                continue;
            }
            if (e.tipo == EV_TECLA && (e.tecla == '\n' || e.tecla == TECLA_ESC)) break;
            continue;
        }
        if (!creditos && tempo_ms() - ultimo >= 1000) { ultimo = tempo_ms(); informacoes(j); }
        dormir(50);
    }
    jan_fechar(j);
    return 0;
}
