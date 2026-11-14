// SPDX-License-Identifier: GPL-2.0-or-later
/* dnsbloqueo - servidor DNS mínimo con el filtro del bloqueo de publicidad (D218):
 * el «Pi-hole» de la casa.
 *
 *   dnsbloqueo [-p PUERTO] [-r SERVIDOR[:PUERTO]] [-v]
 *
 * Escucha en UDP (puerto 53 por defecto) las preguntas de los demás equipos
 * de la red local. Si el nombre está en las listas del bloqueo (el mismo
 * filtro del kernel, BLOQ_CONSULTAR), contesta él mismo: 0.0.0.0 para A, ::
 * para AAAA y una respuesta vacía para los demás tipos. El resto lo reenvía
 * al DNS de verdad (el del DHCP o el de -r) y devuelve la respuesta tal cual.
 *
 * Filtra solo si el bloqueo está activado («bloqueo activar»); si no, reenvía
 * todo. Solo atiende a la red local (y a 127.x): no es un resolvedor abierto.
 * Sin DNS por TCP: un cliente que necesite respuestas grandes las pide por
 * TCP y eso no se atiende (la carga útil EDNS de las preguntas se limita a
 * 1400 bytes para que casi nunca haga falta).
 *
 * Como servicio (mejora 074): /etc/servicios/dnsbloqueo.conf, desactivado por
 * defecto. */
#include "sagui.h"
#include "bloqueo_tabla.h"

#define MAX_PENDENTES 64
#define PRAZO_MS      4000
#define EDNS_MAX      1400

static uint8_t pkt[1536];
static int verbose;

typedef struct {
    bool     usado;
    uint16_t id_novo, id_orig;
    uint32_t ip;
    uint16_t porta;
    unsigned desde;
} pendente_t;
static pendente_t pend[MAX_PENDENTES];

static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static void poe16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

/* El nombre de la pregunta («ads.ejemplo.com»); devuelve la posición tras él
 * o -1 si está mal formado (o comprimido: en una pregunta no debe estarlo). */
static int ler_nome(const uint8_t *m, int n, int i, char *nome, int max)
{
    int k = 0;
    for (int voltas = 0; voltas < 128; voltas++) {
        if (i >= n) return -1;
        unsigned t = m[i++];
        if (t == 0) { nome[k] = 0; return k ? i : -1; }
        if (t > 63 || i + (int)t > n || k + (int)t + 1 >= max) return -1;
        if (k) nome[k++] = '.';
        for (unsigned j = 0; j < t; j++) {
            char c = (char)m[i++];
            if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
            nome[k++] = c;
        }
    }
    return -1;
}

/* Salta un nombre (comprimido o no) en una sección de recursos. */
static int pular_nome(const uint8_t *m, int n, int i)
{
    for (int voltas = 0; i < n && voltas < 128; voltas++) {
        if (m[i] == 0) return i + 1;
        if ((m[i] & 0xC0) == 0xC0) return i + 2 <= n ? i + 2 : -1;
        if (m[i] & 0xC0) return -1;
        i += 1 + m[i];
    }
    return -1;
}

/* Limita la carga útil EDNS (el registro OPT de la sección adicional). */
static void limitar_edns(uint8_t *m, int n, int i)
{
    unsigned an = be16(m + 6), ns = be16(m + 8), ar = be16(m + 10);
    for (unsigned r = 0; r < an + ns + ar && i >= 0 && i < n; r++) {
        i = pular_nome(m, n, i);
        if (i < 0 || i + 10 > n) return;
        unsigned tipo = be16(m + i), classe = be16(m + i + 2), t = be16(m + i + 8);
        if (r >= an + ns && tipo == 41 && classe > EDNS_MAX) poe16(m + i + 2, EDNS_MAX);
        i += 10 + (int)t;
    }
}

static void responder_erro(int fd, const uint8_t *q, int fim_pergunta, unsigned rcode, uint32_t ip, unsigned porta)
{
    static uint8_t r[600];
    if (fim_pergunta > (int)sizeof r) fim_pergunta = 12;
    memcpy(r, q, (size_t)fim_pergunta);
    r[2] = (uint8_t)(0x80 | (q[2] & 0x79));      /* QR, el opcode y RD de la pregunta */
    r[3] = (uint8_t)(0x80 | rcode);              /* RA */
    if (fim_pergunta == 12) poe16(r + 4, 0);
    poe16(r + 6, 0); poe16(r + 8, 0); poe16(r + 10, 0);
    enviar_para(fd, r, (unsigned)fim_pergunta, ip, (int)porta);
}

/* La respuesta del sumidero: la pregunta y, para A o AAAA, la dirección nula. */
static void responder_bloqueado(int fd, const uint8_t *q, int fim_pergunta, unsigned tipo, uint32_t ip, unsigned porta)
{
    static uint8_t r[600];
    memcpy(r, q, (size_t)fim_pergunta);
    r[2] = (uint8_t)(0x80 | (q[2] & 0x79));
    r[3] = 0x80;                                 /* RA, sin error */
    int k = fim_pergunta;
    unsigned tam = tipo == 1 ? 4 : tipo == 28 ? 16 : 0;
    poe16(r + 6, tam ? 1 : 0);
    poe16(r + 8, 0); poe16(r + 10, 0);
    if (tam) {
        r[k++] = 0xC0; r[k++] = 12;              /* el nombre: puntero a la pregunta */
        poe16(r + k, tipo); poe16(r + k + 2, 1);
        r[k + 4] = 0; r[k + 5] = 0; r[k + 6] = 0; r[k + 7] = 60;   /* TTL 60 s */
        poe16(r + k + 8, tam);
        k += 10;
        memset(r + k, 0, tam);
        k += (int)tam;
    }
    enviar_para(fd, r, (unsigned)k, ip, (int)porta);
}

static bool da_rede_local(uint32_t ip, const config_rede_t *c)
{
    if ((ip >> 24) == 127) return true;
    return c->ip && c->mascara && (ip & c->mascara) == (c->ip & c->mascara) && ip != (c->ip | ~c->mascara);
}

static void pergunta(int fd, int sup, uint32_t sup_ip, int sup_porta, int n, const endereco_rede_t *de)
{
    config_rede_t c;
    memset(&c, 0, sizeof c);
    config_rede(REDE_CONSULTAR, &c);
    if (n < 12 || (pkt[2] & 0x80) || !de->porta) return;      /* respuestas o basura: nada */
    if (!da_rede_local(de->ip, &c)) return;
    char nome[256];
    int fim = -1;
    unsigned tipo = 0;
    if (be16(pkt + 4) == 1 && ((pkt[2] >> 3) & 15) == 0) {
        fim = ler_nome(pkt, n, 12, nome, sizeof nome);
        if (fim > 0 && fim + 4 <= n) { tipo = be16(pkt + fim); fim += 4; }
        else fim = -1;
    }
    if (fim < 0) { responder_erro(fd, pkt, 12, 1, de->ip, de->porta); return; }   /* FORMERR */
    int r = bloqueo_dns(BLOQ_CONSULTAR, nome, BLOQC_CONTAR);
    if (r == BLOQ_BLOQUEADO) {
        if (verbose) imprimir("bloqueado: %s (tipo %u, de %u.%u.%u.%u)\n", nome, tipo, IP_PARTES(de->ip));
        responder_bloqueado(fd, pkt, fim, tipo, de->ip, de->porta);
        return;
    }
    uint32_t ip_up = sup_ip ? sup_ip : c.dns;
    if (!ip_up || (ip_up == c.ip && sup_porta == 53)) { responder_erro(fd, pkt, fim, 2, de->ip, de->porta); return; }   /* SERVFAIL */
    /* reenviar con un ID nuestro, para reconocer la respuesta */
    unsigned agora = tempo_ms();
    int livre = -1, velho = 0;
    for (int i = 0; i < MAX_PENDENTES; i++) {
        if (pend[i].usado && agora - pend[i].desde > PRAZO_MS) pend[i].usado = false;
        if (!pend[i].usado && livre < 0) livre = i;
        if (pend[i].desde < pend[velho].desde) velho = i;
    }
    if (livre < 0) livre = velho;                 /* llena: se olvida la más antigua */
    pendente_t *p = &pend[livre];
    uint16_t id;
    for (;;) {
        bytes_aleatorios(&id, sizeof id);
        bool repetido = false;
        for (int i = 0; i < MAX_PENDENTES; i++) repetido |= pend[i].usado && pend[i].id_novo == id;
        if (!repetido) break;
    }
    p->usado = true;
    p->id_novo = id;
    p->id_orig = be16(pkt);
    p->ip = de->ip;
    p->porta = (uint16_t)de->porta;
    p->desde = agora;
    poe16(pkt, id);
    limitar_edns(pkt, n, fim);
    if (verbose) imprimir("reenviado: %s (tipo %u)\n", nome, tipo);
    if (enviar_para(sup, pkt, (unsigned)n, ip_up, sup_porta) < 0) {
        p->usado = false;
        poe16(pkt, p->id_orig);
        responder_erro(fd, pkt, fim, 2, de->ip, de->porta);
    }
}

static void resposta(int fd, uint32_t sup_ip, int sup_porta, int n, const endereco_rede_t *de)
{
    if (n < 12 || !(pkt[2] & 0x80)) return;
    config_rede_t c;
    memset(&c, 0, sizeof c);
    config_rede(REDE_CONSULTAR, &c);
    uint32_t ip_up = sup_ip ? sup_ip : c.dns;
    if (de->ip != ip_up || (int)de->porta != sup_porta) return;   /* solo del servidor al que se preguntó */
    uint16_t id = be16(pkt);
    for (int i = 0; i < MAX_PENDENTES; i++) {
        pendente_t *p = &pend[i];
        if (!p->usado || p->id_novo != id) continue;
        p->usado = false;
        poe16(pkt, p->id_orig);
        enviar_para(fd, pkt, (unsigned)n, p->ip, p->porta);
        return;
    }
}

/* Lee lo que haya en un socket; con «forzar», una lectura con plazo de 1 ms
 * aunque disponivel() diga 0 (un datagrama vacío al frente de la cola). */
static int ler_socket(int fd, endereco_rede_t *de, bool forzar)
{
    int d = disponivel(fd);
    if (d <= 0 && !forzar) return -1;
    int n = receber_de(fd, pkt, sizeof pkt, de);
    return n;
}

static int uso(int cod)
{
    imprimir_em(cod ? ERROS : SAIDA,
        "uso: dnsbloqueo [-p PUERTO] [-r SERVIDOR[:PUERTO]] [-v]\n"
        "Servidor DNS para los demás equipos de la red con el filtro del bloqueo de\n"
        "publicidad (bloqueo activar). Lo que no se bloquea se pregunta al DNS del\n"
        "DHCP (o a -r) y se devuelve tal cual. -v: una línea por pregunta.\n");
    return cod;
}

int principal(int argc, char **argv)
{
    int porta = 53, sup_porta = 53;
    uint32_t sup_ip = 0;
    for (int i = 1; i < argc; i++) {
        if (!compara_texto(argv[i], "--ayuda") || !compara_texto(argv[i], "--help") || !compara_texto(argv[i], "-h")) return uso(0);
        if (!compara_texto(argv[i], "-v")) { verbose = 1; continue; }
        if (!compara_texto(argv[i], "-p") && i + 1 < argc) {
            porta = texto_para_int(argv[++i]);
            if (porta <= 0 || porta > 65535) return uso(2);
            continue;
        }
        if (!compara_texto(argv[i], "-r") && i + 1 < argc) {
            char s[64];
            copia_texto(s, argv[++i], sizeof s);
            char *dois = acha_char(s, ':');
            if (dois) { *dois = 0; sup_porta = texto_para_int(dois + 1); }
            if (!texto_para_ip(s, &sup_ip) || sup_porta <= 0 || sup_porta > 65535) {
                imprimir_em(ERROS, "dnsbloqueo: servidor no válido: %s\n", argv[i]);
                return 2;
            }
            continue;
        }
        return uso(2);
    }
    int fd = criar_socket(SOCK_UDP);
    int sup = criar_socket(SOCK_UDP);
    if (fd < 0 || sup < 0) { imprimir_em(ERROS, "dnsbloqueo: no se pudo crear el socket\n"); return 1; }
    int r = escutar(fd, porta, 0);
    if (r < 0) { imprimir_em(ERROS, "dnsbloqueo: no se puede escuchar en el puerto UDP %d: %s\n", porta, nome_erro(r)); return 1; }
    tempo_limite(fd, 1);
    tempo_limite(sup, 1);
    info_bloqueo_t inf;
    memset(&inf, 0, sizeof inf);
    bloqueo_dns(BLOQ_ESTADO, &inf, sizeof inf);
    char t[16];
    imprimir("dnsbloqueo: escuchando en el puerto UDP %d; reenvía a %s:%d; filtro %s\n", porta,
             sup_ip ? ip_para_texto(sup_ip, t) : "(el DNS del DHCP)", sup_porta,
             inf.activo ? "activado" : "DESACTIVADO (bloqueo activar): se reenvía todo");
    unsigned visto = (unsigned)esperar_rede(0, 0);
    for (;;) {
        endereco_rede_t de;
        bool algo = false;
        for (int k = 0; k < 32; k++) {           /* lo que haya, sin dejar a uno solo acaparar */
            int n = ler_socket(fd, &de, false);
            if (n >= 0) { algo = true; pergunta(fd, sup, sup_ip, sup_porta, n, &de); }
            int m = ler_socket(sup, &de, false);
            if (m >= 0) { algo = true; resposta(fd, sup_ip, sup_porta, m, &de); }
            if (n < 0 && m < 0) break;
        }
        unsigned novo = (unsigned)esperar_rede(visto, 1000);
        if (novo != visto && !algo) {            /* hubo algo que disponivel() no ve */
            int n = ler_socket(fd, &de, true);
            if (n >= 0) pergunta(fd, sup, sup_ip, sup_porta, n, &de);
            int m = ler_socket(sup, &de, true);
            if (m >= 0) resposta(fd, sup_ip, sup_porta, m, &de);
        }
        visto = novo;
    }
}
