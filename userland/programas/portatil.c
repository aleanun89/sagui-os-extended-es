// SPDX-License-Identifier: GPL-2.0-or-later
/* portatil - lo propio de los portátiles por ACPI (D403).
 *
 *   portatil                     qué se encontró: brillo, ACPI Video, Fujitsu,
 *                                Toshiba (teclas, ventilador, radio), modo avión
 *                                y los últimos avisos del firmware
 *   portatil radio               enciende o apaga la radio (modo avión), como la tecla
 *   portatil accion NOMBRE       volumen+ volumen- silencio brillo+ brillo- radio pantalla
 *   portatil tablas [ARCHIVO]    las tablas ACPI en el formato de texto de acpidump
 *                                (root; para mandar el volcado de una máquina real:
 *                                se lee con acpixtract/iasl o tools/uacpi_volcado)
 *   portatil notificar DESTINO VALOR
 *                                como si el firmware hiciera Notify (root, para pruebas):
 *                                video, pantalla, fujitsu-brillo, fujitsu, toshiba */
#include "sagui.h"
#include "portatil.h"

static const char *AYUDA =
    "uso: portatil [radio | accion NOMBRE | tablas [ARCHIVO] | notificar DESTINO VALOR]\n"
    "  accion: volumen+ volumen- silencio brillo+ brillo- radio pantalla\n"
    "  notificar (root, pruebas): video pantalla fujitsu-brillo fujitsu toshiba; VALOR en hex (0x86)\n";

static const char *fonte(unsigned f)
{
    switch (f) {
    case PORTATIL_FONTE_VIDEO:   return "ACPI Video";
    case PORTATIL_FONTE_FUJITSU: return "Fujitsu";
    case PORTATIL_FONTE_TOSHIBA: return "Toshiba";
    }
    return "ninguno";
}

static const char *uacpi_txt(unsigned u)
{
    switch (u) {
    case UACPI_ATIVO:     return "activo";
    case UACPI_INICIANDO: return "iniciando";
    case UACPI_FALHOU:    return "falló";
    }
    return "desactivado";
}

static const char *const acoes[ACAO_MAX] = { "", "volumen+", "volumen-", "silencio", "brillo+", "brillo-", "radio",
                                             "pantalla" };
static const char *const origens[] = { "?", "teclado", "ACPI Video", "Fujitsu", "Toshiba", "orden", "HP" };

static int estado(void)
{
    info_portatil_t p;
    if (portatil_estado(&p) < 0) { imprimir_em(ERROS, "portatil: el kernel no responde\n"); return 1; }
    imprimir("Intérprete ACPI: %s%s%s%s\n", uacpi_txt(p.uacpi), p.uacpi != UACPI_ATIVO && p.motivo[0] ? " (" : "",
             p.uacpi != UACPI_ATIVO ? p.motivo : "", p.uacpi != UACPI_ATIVO && p.motivo[0] ? ")" : "");
    if (p.brilho_fonte != PORTATIL_FONTE_NADA && p.brilho_pct >= 0)
        imprimir("Brillo: %d %% (nivel %u de %u, valores %u-%u, ahora %u), por %s\n", p.brilho_pct, p.brilho_nivel + 1,
                 p.brilho_niveis, p.brilho_min, p.brilho_max, p.brilho_valor, fonte(p.brilho_fonte));
    else
        imprimir("Brillo: sin control%s%s%s\n", p.motivo[0] ? " (" : "", p.motivo, p.motivo[0] ? ")" : "");
    imprimir("ACPI Video: %u dispositivo(s), %u salida(s) con brillo%s%s%s%s\n", p.video_buses, p.video_saidas,
             p.video_lcd[0] ? ", pantalla " : "", p.video_lcd, p.dos_chamado ? ", _DOS(0)" : "",
             p.bqc_indice ? ", _BQC da el índice" : "");
    if (p.fujitsu)
        imprimir("Fujitsu: %s (%s%s%s; funciones %x)\n", p.fujitsu_no, p.fujitsu & 1 ? "brillo FUJ02B1" : "",
                 p.fujitsu == 3 ? ", " : "", p.fujitsu & 2 ? "teclas FUJ02E3" : "", p.fujitsu_funcoes);
    else
        imprimir("Fujitsu: no\n");
    if (p.toshiba) {
        imprimir("Toshiba: %s, teclas %s\n", p.toshiba_no,
                 p.toshiba_teclas == 1 ? "por INFO" : p.toshiba_teclas == 2 ? "por la cola HCI" : "no");
        if (p.toshiba_ventilador >= 0) imprimir("Ventilador: %s (%d)\n", p.toshiba_ventilador ? "en marcha" : "parado", p.toshiba_ventilador);
        if (p.toshiba_radio >= 0) imprimir("Interruptor de la radio: %s\n", p.toshiba_radio ? "encendido" : "apagado");
    } else {
        imprimir("Toshiba: no\n");
    }
    imprimir("Radio: %s\n", p.radio_apagada ? "apagada (modo avión)" : "encendida");
    imprimir("Avisos del firmware: %u", p.eventos);
    if (p.eventos) imprimir(" (último: destino %u, valor %x)", p.ultimo_evento >> 16, p.ultimo_evento & 0xFFFF);
    imprimir("\nAcciones: %u", p.acoes);
    unsigned a = p.ultima_acao >> 8, o = p.ultima_acao & 0xFF;
    if (p.acoes && a < ACAO_MAX) imprimir(" (última: %s, por %s)", acoes[a], o <= ORIGEM_ACAO_HP ? origens[o] : "?");
    imprimir("\n");
    return 0;
}

static int acao(const char *n)
{
    for (int i = 1; i < ACAO_MAX; i++)
        if (compara_texto(n, acoes[i]) == 0) {
            int r = portatil_acao(i);
            if (r < 0) { imprimir_em(ERROS, "portatil: %s: %s\n", n, nome_erro(r)); return 1; }
            return 0;
        }
    imprimir_em(ERROS, "portatil: acción desconocida: %s\n%s", n, AYUDA);
    return 2;
}

/* ------------------------------------------------------------ tablas */

static const char HEX[] = "0123456789ABCDEF";

static int volcar(int fd, const char *ass, unsigned endereco, const unsigned char *t, unsigned n)
{
    char l[96];
    int k = formatar_texto(l, sizeof l, "%s @ 0x00000000%08x\n", ass, endereco);
    if (escrever_tudo(fd, l, (unsigned)k) < 0) return -1;
    for (unsigned i = 0; i < n; i += 16) {
        int p = 0;
        l[p++] = ' '; l[p++] = ' '; l[p++] = ' '; l[p++] = ' ';
        int alto = 12;   /* como acpidump: 4 cifras como mínimo, más si la tabla pasa de 64 KB */
        while (alto < 28 && (i >> (alto + 4))) alto += 4;
        for (int d = alto; d >= 0; d -= 4) l[p++] = HEX[(i >> d) & 15];
        l[p++] = ':';
        for (unsigned j = 0; j < 16; j++) {
            if (i + j < n) { l[p++] = ' '; l[p++] = HEX[t[i + j] >> 4]; l[p++] = HEX[t[i + j] & 15]; }
            else { l[p++] = ' '; l[p++] = ' '; l[p++] = ' '; }
        }
        l[p++] = ' '; l[p++] = ' ';
        for (unsigned j = 0; j < 16 && i + j < n; j++) l[p++] = t[i + j] >= 0x20 && t[i + j] < 0x7F ? (char)t[i + j] : '.';
        l[p++] = '\n';
        if (escrever_tudo(fd, l, (unsigned)p) < 0) return -1;
    }
    return escrever_tudo(fd, "\n", 1) < 0 ? -1 : 0;
}

static int tablas(const char *arq)
{
    info_acpi_t a;
    if (info_sistema(INFO_ACPI, &a, sizeof a) < 0 || !a.achou) { imprimir_em(ERROS, "portatil: no hay tablas ACPI\n"); return 1; }
    int fd = SAIDA;
    if (arq) {
        fd = abrir(arq, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fd < 0) { imprimir_em(ERROS, "portatil: %s: %s\n", arq, nome_erro(fd)); return 1; }
    }
    int erros = 0, feitas = 0;
    unsigned total = 0;
    for (unsigned i = 0; i < a.qtd; i++) {
        unsigned n = a.tabela[i].tamanho;
        if (n < 36 || n > (4u << 20)) continue;
        unsigned char *b = aloca(n);
        if (!b) { erros++; continue; }
        int r = portatil_tabela((int)i, b, n);
        if (r == ERRO_ACESSO) {
            imprimir_em(ERROS, "portatil: leer las tablas es solo de root (admin portatil tablas)\n");
            libera(b);
            if (arq) fechar(fd);
            return 1;
        }
        if (r < 0) { imprimir_em(ERROS, "portatil: tabla %s: %s\n", a.tabela[i].assinatura, nome_erro(r)); erros++; }
        else if (volcar(fd, a.tabela[i].assinatura, a.tabela[i].endereco, b, n) < 0) erros++;
        else { feitas++; total += n; }
        libera(b);
    }
    if (arq) {
        fechar(fd);
        imprimir("%d tablas (%u bytes) en %s\n", feitas, total, arq);
    }
    return erros ? 1 : 0;
}

/* ------------------------------------------------------------ notificar */

static bool hex(const char *s, int *v)
{
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (!*s) return false;
    int n = 0;
    for (; *s; s++) {
        int d = *s >= '0' && *s <= '9' ? *s - '0' : *s >= 'a' && *s <= 'f' ? *s - 'a' + 10 :
                *s >= 'A' && *s <= 'F' ? *s - 'A' + 10 : -1;
        if (d < 0 || n > 0xFFF) return false;
        n = n * 16 + d;
    }
    *v = n;
    return true;
}

static int notificar(const char *d, const char *v)
{
    static const char *const nomes[] = { "", "video", "pantalla", "fujitsu-brillo", "fujitsu", "toshiba" };
    int alvo = 0, valor;
    for (int i = 1; i <= PORTATIL_ALVO_TOSHIBA; i++) if (compara_texto(d, nomes[i]) == 0) alvo = i;
    if (!alvo || !hex(v, &valor)) { escrever_texto(ERROS, AYUDA); return 2; }
    int r = portatil_notificar(alvo, valor);
    if (r < 0) { imprimir_em(ERROS, "portatil: notificar: %s\n", nome_erro(r)); return 1; }
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc < 2) return estado();
    const char *o = argv[1];
    if (compara_texto(o, "-h") == 0 || compara_texto(o, "--ayuda") == 0) { escrever_texto(SAIDA, AYUDA); return 0; }
    if (compara_texto(o, "radio") == 0 && argc == 2) return acao("radio");
    if (compara_texto(o, "accion") == 0 && argc == 3) return acao(argv[2]);
    if (compara_texto(o, "tablas") == 0 && argc <= 3) return tablas(argc == 3 ? argv[2] : NULL);
    if (compara_texto(o, "notificar") == 0 && argc == 4) return notificar(argv[2], argv[3]);
    escrever_texto(ERROS, AYUDA);
    return 2;
}
