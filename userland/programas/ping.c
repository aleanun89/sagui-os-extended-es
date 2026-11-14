// SPDX-License-Identifier: GPL-2.0-or-later
/* ping [-c N] [-s bytes] [-W ms] host - eco ICMP (D046).
 *
 * Envía una petición de eco por segundo (4 por defecto; Ctrl+C para parar) y
 * muestra el tiempo de ida y vuelta medido por el kernel con el TSC. */
#include "sagui.h"

static void ms(char *s, unsigned us) { formatar_texto(s, 24, "%u.%03u", us / 1000, us % 1000); }

int principal(int argc, char **argv)
{
    int quantos = 4, dados = 56, prazo = 2000;
    const char *host = NULL;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "-c") == 0 && i + 1 < argc) quantos = texto_para_int(argv[++i]);
        else if (compara_texto(argv[i], "-s") == 0 && i + 1 < argc) dados = texto_para_int(argv[++i]);
        else if (compara_texto(argv[i], "-W") == 0 && i + 1 < argc) prazo = texto_para_int(argv[++i]);
        else if (argv[i][0] == '-') host = NULL, i = argc;
        else host = argv[i];
    }
    if (!host || quantos < 1 || dados < 0 || dados > 1400) {
        imprimir_em(ERROS, "uso: ping [-c veces] [-s bytes] [-W ms] <host>\n");
        return 2;
    }
    uint32_t ip;
    int r = resolver(host, &ip);
    if (r < 0) { imprimir_em(ERROS, "ping: %s: %s\n", host, nome_erro(r)); return 2; }
    char t[16];
    imprimir("PING %s (%s): %d bytes de datos\n", host, ip_para_texto(ip, t), dados);
    int recebidos = 0;
    unsigned minimo = 0xFFFFFFFFu, maximo = 0, soma = 0;
    for (int seq = 1; seq <= quantos; seq++) {
        unsigned ini = tempo_ms();
        info_ping_t p = { (unsigned)seq, (unsigned)dados, 0, 0 };
        if (dados == 0) p.tamanho = 1;   /* el kernel entiende 0 como "predeterminado" */
        r = ping(ip, &p, (unsigned)prazo);
        if (r == 0) {
            char a[24];
            ms(a, p.us);
            imprimir("%d bytes desde %s: icmp_seq=%d ttl=%u tiempo=%s ms\n", dados + 8, t, seq, p.ttl, a);
            recebidos++;
            soma += p.us;
            if (p.us < minimo) minimo = p.us;
            if (p.us > maximo) maximo = p.us;
        } else if (r == ERRO_TEMPO) {
            imprimir("Sin respuesta de %s: icmp_seq=%d\n", t, seq);
        } else {
            imprimir_em(ERROS, "ping: %s\n", nome_erro(r));
            if (r == ERRO_SEM_REDE || r == ERRO_INVALIDO) return 2;
        }
        unsigned gasto = tempo_ms() - ini;
        if (seq < quantos && gasto < 1000) dormir(1000 - gasto);
    }
    imprimir("--- %s: estadísticas de ping ---\n", host);
    imprimir("%d paquetes enviados, %d recibidos, %d%% perdidos\n", quantos, recebidos, (quantos - recebidos) * 100 / quantos);
    if (recebidos) {
        char a[24], b[24], c[24];
        ms(a, minimo); ms(b, soma / (unsigned)recebidos); ms(c, maximo);
        imprimir("rtt mín/media/máx = %s/%s/%s ms\n", a, b, c);
    }
    return recebidos ? 0 : 1;
}
