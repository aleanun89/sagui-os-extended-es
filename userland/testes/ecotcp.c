// SPDX-License-Identifier: GPL-2.0-or-later
/* ecotcp - servidor TCP de prueba para los túneles del sshd (h251, D253).
 *
 *   ecotcp PUERTO     atiende hasta 8 conexiones a la vez, hasta que lo maten
 *
 * Cada conexión, según lo primero que llegue:
 *   "DAME n\n"   manda n bytes de un patrón conocido (el byte i vale i % 251)
 *                y cierra él: prueba el sentido servidor -> cliente y el
 *                cierre desde este lado;
 *   otra cosa    devuelve tal cual lo que reciba (eco) y, cuando el otro lado
 *                cierra su envío, añade "\nFIN n\n" (n = bytes recibidos) y
 *                cierra: prueba el otro sentido y que el fin de envío llega
 *                sin cortar la vuelta.
 * Escribe una línea por conexión abierta y cerrada. */
#include "sagui.h"

#define MAX_CON 8

static struct { int fd; bool usado, eco; unsigned recibidos; } con[MAX_CON];
static char buf[4096];

static void cerrar(int i, const char *como)
{
    imprimir("ecotcp: conexión %d cerrada (%s, %u bytes recibidos)\n", i, como, con[i].recibidos);
    fechar(con[i].fd);
    con[i].usado = false;
}

int principal(int argc, char **argv)
{
    int porta = argc > 1 ? texto_para_int(argv[1]) : 0;
    if (porta <= 0 || porta > 65535) { imprimir_em(ERROS, "uso: ecotcp PUERTO\n"); return 2; }
    int e = criar_socket(SOCK_TCP);
    int r = e < 0 ? e : escutar(e, porta, 8);
    if (r < 0) { imprimir_em(ERROS, "ecotcp: puerto %d: %s\n", porta, nome_erro(r)); return 1; }
    imprimir("ecotcp: escuchando en el puerto %d\n", porta);
    unsigned visto = (unsigned)esperar_rede(0, 0);
    for (;;) {
        bool fez = false;
        if (disponivel(e) > 0) {
            int s = aceitar_conexao(e, NULL);
            int livre = -1;
            for (int i = 0; i < MAX_CON && livre < 0; i++)
                if (!con[i].usado) livre = i;
            if (s >= 0 && livre < 0) fechar(s);
            else if (s >= 0) {
                con[livre].fd = s;
                con[livre].usado = true;
                con[livre].eco = false;
                con[livre].recibidos = 0;
                tempo_limite(s, 20000);
                imprimir("ecotcp: conexión %d abierta\n", livre);
            }
            fez = true;
        }
        for (int i = 0; i < MAX_CON; i++) {
            if (!con[i].usado) continue;
            int d = disponivel(con[i].fd);
            if (d == 0) continue;
            fez = true;
            if (d < 0) {   /* el otro lado cerró su envío */
                char fim[32];
                formatar_texto(fim, sizeof fim, "\nFIN %u\n", con[i].recibidos);
                if (con[i].eco || !con[i].recibidos) escrever_tudo(con[i].fd, fim, (unsigned)tam_texto(fim));
                cerrar(i, "fin del otro lado");
                continue;
            }
            int n = ler(con[i].fd, buf, sizeof buf - 1);
            if (n <= 0) { cerrar(i, "error al leer"); continue; }
            if (!con[i].recibidos && !con[i].eco && n > 5 && memcmp(buf, "DAME ", 5) == 0) {
                buf[n] = 0;
                unsigned total = (unsigned)texto_para_int(buf + 5), feitos = 0;
                con[i].recibidos = (unsigned)n;
                while (feitos < total) {
                    unsigned k = total - feitos < sizeof buf ? total - feitos : sizeof buf;
                    for (unsigned j = 0; j < k; j++) buf[j] = (char)((feitos + j) % 251);
                    if (escrever_tudo(con[i].fd, buf, k) != (int)k) break;
                    feitos += k;
                }
                cerrar(i, feitos == total ? "patrón enviado" : "patrón a medias");
                continue;
            }
            con[i].eco = true;
            con[i].recibidos += (unsigned)n;
            if (escrever_tudo(con[i].fd, buf, (unsigned)n) != n) cerrar(i, "error al escribir");
        }
        if (!fez) visto = (unsigned)esperar_rede(visto, 200);
    }
}
