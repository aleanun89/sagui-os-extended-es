// SPDX-License-Identifier: GPL-2.0-or-later
/* nc (netcat) básico - conexiones TCP por la línea de comandos (D046).
 *
 *   nc host puerto     conecta; manda lo que llegue de la entrada y muestra lo que llegue
 *   nc -l puerto       espera UNA conexión y hace lo mismo
 *   -w segundos        tiempo límite para conectar o quedarse sin recibir nada
 *
 * Como la entrada de la terminal no tiene "disponible", el nc es por líneas: manda
 * cada línea de la entrada y muestra lo que ya haya llegado; al final de la entrada
 * (Ctrl+D, o el fin de un pipe) cierra el envío y muestra el resto hasta que el otro
 * lado cierre. Ej.: echo hola | nc 10.0.2.2 7. */
#include "sagui.h"

static char buf[4096];

static int despejar(int s, bool ate_fechar)
{
    for (;;) {
        if (!ate_fechar) {
            int d = disponivel(s);
            if (d == ERRO_PIPE_QUEBRADO) return 1;   /* el otro lado cerró */
            if (d <= 0) return 0;
        }
        int n = ler(s, buf, sizeof buf);
        if (n == 0) return 1;
        if (n < 0) { imprimir_em(ERROS, "nc: %s\n", nome_erro(n)); return -1; }
        size_t sobra;
        size_t k = utf8_para_latin1(buf, (size_t)n, &sobra);
        escrever_tudo(SAIDA, buf, (unsigned)(k + sobra));
    }
}

int principal(int argc, char **argv)
{
    bool ouvir = false;
    int espera_s = 0;
    const char *host = NULL, *porta_txt = NULL;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "-l") == 0) ouvir = true;
        else if (compara_texto(argv[i], "-w") == 0 && i + 1 < argc) espera_s = texto_para_int(argv[++i]);
        else if (!ouvir && !host) host = argv[i];
        else porta_txt = argv[i];
    }
    if (ouvir && host && !porta_txt) { porta_txt = host; host = NULL; }
    int porta = porta_txt ? texto_para_int(porta_txt) : 0;
    if (porta <= 0 || porta > 65535 || (!ouvir && !host)) {
        imprimir_em(ERROS, "uso: nc [-w segundos] <host> <puerto>\n     nc -l <puerto>\n");
        return 2;
    }
    int s;
    if (ouvir) {
        int e = criar_socket(SOCK_TCP);
        if (e < 0) { imprimir_em(ERROS, "nc: %s\n", nome_erro(e)); return 1; }
        int r = escutar(e, porta, 1);
        if (r < 0) { imprimir_em(ERROS, "nc: puerto %d: %s\n", porta, nome_erro(r)); return 1; }
        if (espera_s) tempo_limite(e, (unsigned)espera_s * 1000);
        imprimir_em(ERROS, "nc: esperando conexión en el puerto %d...\n", porta);
        endereco_rede_t de;
        s = aceitar_conexao(e, &de);
        fechar(e);
        if (s < 0) { imprimir_em(ERROS, "nc: %s\n", nome_erro(s)); return 1; }
        char t[16];
        imprimir_em(ERROS, "nc: conexión de %s:%u\n", ip_para_texto(de.ip, t), de.porta);
    } else {
        s = conectar_tcp(host, porta, espera_s ? (unsigned)espera_s * 1000 : 15000);
        if (s < 0) { imprimir_em(ERROS, "nc: %s:%d: %s\n", host, porta, nome_erro(s)); return 1; }
    }
    if (espera_s) tempo_limite(s, (unsigned)espera_s * 1000);
    bool fechou = false;
    for (;;) {
        int n = ler(ENTRADA, buf, sizeof buf);
        if (n <= 0) break;
        if (escrever_tudo(s, buf, (unsigned)n) < 0) { imprimir_em(ERROS, "nc: la conexión se cerró\n"); fechou = true; break; }
        int r = despejar(s, false);
        if (r != 0) { fechou = true; break; }
    }
    if (!fechou) {
        opcao_socket(s, OPC_FECHAR_ENVIO, 0);
        despejar(s, true);
    }
    fechar(s);
    return 0;
}
