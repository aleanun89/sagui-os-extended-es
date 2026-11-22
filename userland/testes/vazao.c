// SPDX-License-Identifier: GPL-3.0-only
/* vazao - mide la velocidad de una conexión TCP (h437, D437).
 *
 *   vazao baixar HOST PUERTO          pide "DAME\n" y lee hasta que el otro lado cierre
 *   vazao subir HOST PUERTO BYTES     manda BYTES de un patrón, cierra el envío y lee la respuesta
 *   vazao escutar PUERTO [VECES]      servidor: cuenta lo que llega en cada conexión y contesta
 *                                     "RECIBIDO n\n" (para medir el bucle local 127.0.0.1)
 *
 * Escribe una línea "VAZAO modo: n bytes en m ms, k KB/s; RX p paquetes, TX q paquetes
 * (x,y por tic de 10 ms)" con los contadores de la eth0 durante la transferencia. */
#include "sagui.h"

static char buf[16384];

static void contadores(unsigned *rx, unsigned *tx)
{
    info_rede_t r;
    *rx = *tx = 0;
    if (info_sistema(INFO_REDE, &r, sizeof r) > 0) { *rx = r.rx_pacotes; *tx = r.tx_pacotes; }
}

static void informar(const char *modo, unsigned total, unsigned ms, unsigned rx, unsigned tx)
{
    if (!ms) ms = 1;
    unsigned kbs = (total / ms) * 1000u / 1024u + ((total % ms) * 1000u / ms) / 1024u;
    unsigned tiques = ms / 10 ? ms / 10 : 1;
    unsigned por = (rx * 10u) / tiques;
    imprimir("VAZAO %s: %u bytes en %u ms, %u KB/s; RX %u paquetes, TX %u paquetes (%u,%u por tic de 10 ms)\n",
             modo, total, ms, kbs, rx, tx, por / 10, por % 10);
}

static int baixar(const char *host, int porta)
{
    int s = conectar_tcp(host, porta, 15000);
    if (s < 0) { imprimir("VAZAO baixar: ERRO %s\n", nome_erro(s)); return 1; }
    tempo_limite(s, 20000);
    unsigned rx0, tx0, rx1, tx1;
    contadores(&rx0, &tx0);
    unsigned t0 = tempo_ms(), total = 0;
    escrever_tudo(s, "DAME\n", 5);
    int n;
    while ((n = ler(s, buf, sizeof buf)) > 0) total += (unsigned)n;
    unsigned ms = tempo_ms() - t0;
    contadores(&rx1, &tx1);
    fechar(s);
    if (n < 0) imprimir("VAZAO baixar: cortada (%s)\n", nome_erro(n));
    informar("baixar", total, ms, rx1 - rx0, tx1 - tx0);
    return n < 0;
}

static int subir(const char *host, int porta, unsigned bytes)
{
    int s = conectar_tcp(host, porta, 15000);
    if (s < 0) { imprimir("VAZAO subir: ERRO %s\n", nome_erro(s)); return 1; }
    tempo_limite(s, 20000);
    for (unsigned i = 0; i < sizeof buf; i++) buf[i] = (char)('a' + i % 26);
    unsigned rx0, tx0, rx1, tx1;
    contadores(&rx0, &tx0);
    unsigned t0 = tempo_ms(), enviado = 0;
    while (enviado < bytes) {
        unsigned k = bytes - enviado < sizeof buf ? bytes - enviado : (unsigned)sizeof buf;
        if (escrever_tudo(s, buf, k) < 0) { imprimir("VAZAO subir: la conexión se cerró\n"); fechar(s); return 1; }
        enviado += k;
    }
    opcao_socket(s, OPC_FECHAR_ENVIO, 0);
    char resp[64];
    unsigned r = 0;
    int n;
    while (r + 1 < sizeof resp && (n = ler(s, resp + r, sizeof resp - 1 - r)) > 0) r += (unsigned)n;
    resp[r] = 0;
    unsigned ms = tempo_ms() - t0;
    contadores(&rx1, &tx1);
    fechar(s);
    char *fim = acha_char(resp, '\n');
    if (fim) *fim = 0;
    imprimir("VAZAO subir: respuesta «%s»\n", resp);
    informar("subir", enviado, ms, rx1 - rx0, tx1 - tx0);
    return 0;
}

static int escutar_porta(int porta, int vezes)
{
    int e = criar_socket(SOCK_TCP);
    int r = e < 0 ? e : escutar(e, porta, 2);
    if (r < 0) { imprimir("VAZAO escutar: puerto %d: %s\n", porta, nome_erro(r)); return 1; }
    imprimir("VAZAO escutar: puerto %d\n", porta);
    for (int v = 0; v < vezes; v++) {
        int s = aceitar_conexao(e, NULL);
        if (s < 0) { imprimir("VAZAO escutar: %s\n", nome_erro(s)); break; }
        tempo_limite(s, 20000);
        unsigned total = 0, t0 = tempo_ms();
        int n;
        while ((n = ler(s, buf, sizeof buf)) > 0) total += (unsigned)n;
        char resp[48];
        int k = formatar_texto(resp, sizeof resp, "RECIBIDO %u\n", total);
        escrever_tudo(s, resp, (unsigned)k);
        fechar(s);
        informar("escutar", total, tempo_ms() - t0, 0, 0);
    }
    fechar(e);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc >= 4 && compara_texto(argv[1], "baixar") == 0) return baixar(argv[2], texto_para_int(argv[3]));
    if (argc >= 5 && compara_texto(argv[1], "subir") == 0)
        return subir(argv[2], texto_para_int(argv[3]), (unsigned)texto_para_int(argv[4]));
    if (argc >= 3 && compara_texto(argv[1], "escutar") == 0)
        return escutar_porta(texto_para_int(argv[2]), argc >= 4 ? texto_para_int(argv[3]) : 1);
    imprimir_em(ERROS, "uso: vazao baixar HOST PUERTO | subir HOST PUERTO BYTES | escutar PUERTO [VECES]\n");
    return 2;
}
