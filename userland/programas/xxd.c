// SPDX-License-Identifier: GPL-2.0-or-later
/* xxd - volcado hexadecimal de un archivo, o su inverso con -r (D232).
 *   xxd archivo      xxd -l 32 archivo      xxd -p archivo (solo hex)      xxd archivo | xxd -r > copia */
#include "od_logica.h"

static const char *AYUDA =
    "uso: xxd [-l BYTES] [-s SALTO] [-p] [-r] [archivo]\n"
    "  Escribe dirección, 16 bytes en hex (en grupos de dos) y el texto.\n"
    "  -l N  solo N bytes     -s N  empieza en el byte N     -p  solo los bytes en hex, 30 por línea\n"
    "  -r  inverso: reconstruye los bytes a partir de un volcado (con o sin direcciones; con -p, solo hex)\n";

static int hexv(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int inverso(int fd, bool plano)
{
    leitor_linhas_t l;
    leitor_iniciar(&l, fd, false);
    char linea[512];
    uint8_t sal[256];
    int ns = 0;
    while (leitor_linha(&l, linea, sizeof linea, NULL) >= 0) {
        const char *p = linea;
        if (!plano) {   /* hasta los dos puntos de la dirección */
            const char *c = acha_char(p, ':');
            if (!c) continue;
            p = c + 1;
        }
        int alto = -1;
        for (; *p; p++) {
            if (!plano && p[0] == ' ' && p[1] == ' ') break;   /* tras el hex vienen dos espacios y el texto */
            int v = hexv(*p);
            if (v < 0) continue;
            if (alto < 0) alto = v;
            else {
                sal[ns++] = (uint8_t)(alto * 16 + v);
                alto = -1;
                if (ns == (int)sizeof sal) { if (escrever_tudo(SAIDA, sal, (unsigned)ns) < 0) return 0; ns = 0; }
            }
        }
    }
    if (ns) escrever_tudo(SAIDA, sal, (unsigned)ns);
    return 0;
}

int principal(int argc, char **argv)
{
    int limite = -1, salto = 0;
    bool plano = false, rev = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "l:s:pr", &o)) >= 0) {
        switch (r) {
        case 'l': if (!texto_para_numero(o.valor, &limite)) { imprimir_em(ERROS, "xxd: -l: número no válido\n"); return 2; } break;
        case 's': if (!texto_para_numero(o.valor, &salto)) { imprimir_em(ERROS, "xxd: -s: número no válido\n"); return 2; } break;
        case 'p': plano = true; break;
        case 'r': rev = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("xxd", r, &o);
        }
    }
    if (argc - o.indice > 1) { imprimir_em(ERROS, "xxd: un solo archivo (xxd --help)\n"); return 2; }
    int fd = abrir_entrada("xxd", o.indice < argc ? argv[o.indice] : "-");
    if (fd < 0) return 1;
    if (rev) { int rr = inverso(fd, plano); fechar_entrada(fd); return rr; }
    uint8_t lin[32];
    char t[256];
    uint32_t off = (uint32_t)salto;
    for (int s = salto; s > 0;) {
        int k = ler(fd, lin, (unsigned)(s > 32 ? 32 : s));
        if (k <= 0) break;
        s -= k;
    }
    int por_linea = plano ? 30 : 16;
    for (;;) {
        int n = 0, quiero = por_linea;
        if (limite >= 0 && quiero > limite) quiero = limite;
        while (n < quiero) {
            int k = ler(fd, lin + n, (unsigned)(quiero - n));
            if (k <= 0) break;
            n += k;
        }
        if (n == 0) break;
        if (limite >= 0) limite -= n;
        if (plano) {
            char *q = t;
            for (int i = 0; i < n; i++) q += formatar_texto(q, 4, "%02x", lin[i]);
        } else od_linea_xxd(t, sizeof t, off, lin, n);
        if (saida_linha(t, (int)tam_texto(t)) < 0) break;
        off += (uint32_t)n;
        if (n < quiero || limite == 0) break;
    }
    fechar_entrada(fd);
    return 0;
}
