// SPDX-License-Identifier: GPL-2.0-or-later
/* od - vuelca un archivo en octal, hexadecimal o caracteres (D232); con -C da el formato de "hexdump -C".
 *   od -c archivo      od -A x -t x1 archivo      od -tx2 -N 32 archivo      hexdump -C archivo (el mismo programa) */
#include "od_logica.h"

static const char *AYUDA =
    "uso: od [-A d|o|x|n] [-t TIPO] [-bcdoxC] [-v] [-j SALTO] [-N BYTES] [-w ANCHO] [archivo]\n"
    "  -t TIPO  formato de cada valor: o1 o2 o4 (octal), x1 x2 x4 (hex), d1 d2 d4 (con signo), u1 u2 u4 (sin signo), c (caracteres)\n"
    "  -A BASE  base de la dirección: o (por defecto), d, x o n (sin dirección)\n"
    "  -b = -t o1   -c = -t c   -d = -t u2   -o = -t o2   -x = -t x2\n"
    "  -C  formato canónico de hexdump -C: dirección, 16 bytes en hex y el texto entre barras\n"
    "  -j N  salta los primeros N bytes   -N N  muestra solo N bytes   -w N  bytes por línea (16 por defecto)\n"
    "  -v  no resume con * las líneas repetidas\n";

int principal(int argc, char **argv)
{
    char fmt = 'o';
    int tam = 2, base = 8, ancho = 16, canon = 0;
    int salto = 0, limite = -1;
    bool todo = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "A:t:bcdoxCvj:N:w:", &o)) >= 0) {
        switch (r) {
        case 'A':
            if (o.valor[0] == 'o') base = 8; else if (o.valor[0] == 'd') base = 10; else if (o.valor[0] == 'x') base = 16;
            else if (o.valor[0] == 'n') base = 0;
            else { imprimir_em(ERROS, "od: -A: use o, d, x o n\n"); return 2; }
            break;
        case 't': {
            char f = o.valor[0];
            int t = 1;
            if (f == 'c') { fmt = 'c'; tam = 1; break; }
            if (f != 'o' && f != 'x' && f != 'd' && f != 'u') { imprimir_em(ERROS, "od: -t %s: tipo no válido (od --help)\n", o.valor); return 2; }
            if (o.valor[1]) { t = o.valor[1] - '0'; if (o.valor[2] || (t != 1 && t != 2 && t != 4)) { imprimir_em(ERROS, "od: -t %s: el tamaño debe ser 1, 2 o 4\n", o.valor); return 2; } }
            else t = f == 'o' ? 2 : 4;
            fmt = f; tam = t;
            break;
        }
        case 'b': fmt = 'o'; tam = 1; break;
        case 'c': fmt = 'c'; tam = 1; break;
        case 'd': fmt = 'u'; tam = 2; break;
        case 'o': fmt = 'o'; tam = 2; break;
        case 'x': fmt = 'x'; tam = 2; break;
        case 'C': canon = 1; break;
        case 'v': todo = true; break;
        case 'j': if (!texto_para_numero(o.valor, &salto)) { imprimir_em(ERROS, "od: -j: número no válido\n"); return 2; } break;
        case 'N': if (!texto_para_numero(o.valor, &limite)) { imprimir_em(ERROS, "od: -N: número no válido\n"); return 2; } break;
        case 'w': if (!texto_para_numero(o.valor, &ancho) || ancho < 1 || ancho > 64) { imprimir_em(ERROS, "od: -w: de 1 a 64\n"); return 2; } break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("od", r, &o);
        }
    }
    if (canon) { base = 16; ancho = 16; }
    if (argc - o.indice > 1) { imprimir_em(ERROS, "od: un solo archivo (od --help)\n"); return 2; }
    int fd = abrir_entrada("od", o.indice < argc ? argv[o.indice] : "-");
    if (fd < 0) return 1;
    uint8_t lin[64], ant[64];
    char t[512];
    uint32_t off = 0;
    int n_ant = -1;
    bool resumido = false;
    /* salto de entrada: leyendo y descartando (valen también las tuberías) */
    for (int s = salto; s > 0;) {
        int k = ler(fd, lin, (unsigned)(s > 64 ? 64 : s));
        if (k <= 0) break;
        s -= k; off += (uint32_t)k;
    }
    for (;;) {
        int n = 0, quiero = ancho;
        if (limite >= 0 && quiero > limite) quiero = limite;
        while (n < quiero) {
            int k = ler(fd, lin + n, (unsigned)(quiero - n));
            if (k <= 0) break;
            n += k;
        }
        if (n == 0) break;
        if (limite >= 0) limite -= n;
        if (!todo && n == ancho && n_ant == ancho && memcmp(lin, ant, (size_t)n) == 0) {
            if (!resumido) { escrever_texto(SAIDA, "*\n"); resumido = true; }
        } else {
            resumido = false;
            if (canon) od_linea_hex_c(t, sizeof t, off, lin, n);
            else od_linea_od(t, sizeof t, off, lin, n, fmt, tam, base);
            if (saida_linha(t, (int)tam_texto(t)) < 0) { fechar_entrada(fd); return 0; }
        }
        memcpy(ant, lin, (size_t)n);
        n_ant = n;
        off += (uint32_t)n;
        if (n < quiero || limite == 0) break;
    }
    fechar_entrada(fd);
    if (base) {
        if (canon) formatar_texto(t, sizeof t, "%08x", off);
        else od_direccion(t, sizeof t, off, base);
        saida_linha(t, (int)tam_texto(t));
    }
    return 0;
}
