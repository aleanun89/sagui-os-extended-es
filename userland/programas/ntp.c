// SPDX-License-Identifier: GPL-2.0-or-later
/* ntp - pone el reloj en hora por SNTP (RFC 4330), sobre UDP (D080).
 *
 *   ntp [servidor[:puerto]]   pregunta (predeterminado: pool.ntp.org:123) y ajusta el
 *                             reloj del sistema (el RTC queda como estaba)
 *   ntp --rtc [servidor]      ajusta también el reloj de la placa
 *   ntp -n [servidor]         solo muestra la diferencia, sin ajustar
 *
 * Una petición de 48 bytes en modo cliente; de la respuesta valen el "transmit
 * timestamp" del servidor y el tiempo de ida y vuelta medido aquí: la hora correcta
 * es la del servidor más la mitad del viaje. Escrito a partir del RFC; sin
 * filtros ni varias muestras como el ntpd (para una PC de escritorio basta). */
#include "sagui.h"

#define EPOCA_NTP 2208988800u   /* segundos de 1/1/1900 a 1/1/1970 */

static uint32_t be32_de(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

int principal(int argc, char **argv)
{
    bool rtc = false, so_ver = false;
    const char *servidor = "pool.ntp.org";
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--rtc") == 0) rtc = true;
        else if (compara_texto(argv[i], "-n") == 0) so_ver = true;
        else if (argv[i][0] == '-') { imprimir_em(ERROS, "uso: ntp [--rtc] [-n] [servidor[:puerto]]\n"); return 2; }
        else servidor = argv[i];
    }
    char host[128];
    copia_texto(host, servidor, sizeof host);
    int porta = 123;
    char *dois = acha_ultimo_char(host, ':');
    if (dois) { *dois = 0; porta = texto_para_int(dois + 1); }
    uint32_t ip;
    int r = resolver(host, &ip);
    if (r < 0) { imprimir_em(ERROS, "ntp: %s: %s\n", host, nome_erro(r)); return 1; }
    int s = criar_socket(SOCK_UDP);
    if (s < 0) { imprimir_em(ERROS, "ntp: %s\n", nome_erro(s)); return 1; }
    tempo_limite(s, 3000);
    uint8_t p[48];
    int n = -1;
    unsigned ida = 0, volta = 0;
    for (int tentativa = 0; tentativa < 3 && n < 48; tentativa++) {
        memset(p, 0, sizeof p);
        p[0] = (0 << 6) | (4 << 3) | 3;   /* sin aviso de segundo extra, versión 4, modo cliente */
        ida = tempo_ms();
        r = enviar_para(s, p, sizeof p, ip, porta);
        if (r < 0) { imprimir_em(ERROS, "ntp: %s\n", nome_erro(r)); fechar(s); return 1; }
        n = receber_de(s, p, sizeof p, NULL);
        volta = tempo_ms();
        if (n >= 48 && ((p[0] & 7) != 4 || p[1] == 0)) n = -1;   /* no es respuesta de servidor, o "kiss of death" */
    }
    fechar(s);
    if (n < 48) { imprimir_em(ERROS, "ntp: %s no responde\n", host); return 1; }
    uint32_t seg = be32_de(p + 40), frac = be32_de(p + 44);
    if (seg < EPOCA_NTP) { imprimir_em(ERROS, "ntp: respuesta sin hora válida\n"); return 1; }
    unsigned viagem = volta - ida;
    /* La hora del servidor cuando la respuesta llega aquí: más la mitad del viaje. */
    unsigned ms = (frac >> 16) * 1000u / 65536u + viagem / 2;
    uint32_t certo = seg - EPOCA_NTP + ms / 1000;
    hora_t h;
    uint32_t agora = hora_utc(&h);
    int dif = (int)(certo - agora);
    char t[64];
    fuso_data_t l;
    hora_local(certo, &l);
    formatar_data(t, sizeof t, "%d/%m/%Y %H:%M:%S %Z", &l);
    imprimir("Servidor %s (estrato %u): %s, diferencia %c%d s, ida y vuelta %u ms\n", host, p[1], t,
             dif < 0 ? '-' : '+', dif < 0 ? -dif : dif, viagem);
    if (so_ver) return 0;
    r = ajustar_hora(certo, rtc ? HORA_GRAVAR_RTC : 0);
    if (r < 0) { imprimir_em(ERROS, "ntp: no se pudo ajustar la hora: %s\n", nome_erro(r)); return 1; }
    imprimir("Hora del sistema ajustada%s.\n", rtc ? " y guardada en el reloj del PC" : " (el reloj del PC no cambia: usa --rtc)");
    registrar("ntp: %s diferenca %d s, ajustada%s", host, dif, rtc ? " com o RTC" : "");
    return 0;
}
