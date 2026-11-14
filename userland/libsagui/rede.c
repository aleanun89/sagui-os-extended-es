// SPDX-License-Identifier: GPL-2.0-or-later
/* rede.c - las llamadas de red de libsagui (D046). */
#include "sagui.h"

static inline int chamar5(unsigned n, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e) : "memory");
    return r;
}

int criar_socket(int tipo)                     { return chamar5(SIS_SOCKET, (unsigned)tipo, 0, 0, 0, 0); }
int conectar(int fd, uint32_t ip, int porta)   { return chamar5(SIS_CONECTAR, (unsigned)fd, ip, (unsigned)porta, 0, 0); }
int escutar(int fd, int porta, int fila)       { return chamar5(SIS_ESCUTAR, (unsigned)fd, (unsigned)porta, (unsigned)fila, 0, 0); }
int aceitar_conexao(int fd, endereco_rede_t *de)       { return chamar5(SIS_ACEITAR, (unsigned)fd, (unsigned)de, 0, 0, 0); }
int enviar_para(int fd, const void *buf, unsigned n, uint32_t ip, int porta)
{
    return chamar5(SIS_ENVIAR_PARA, (unsigned)fd, (unsigned)buf, n, ip, (unsigned)porta);
}
int receber_de(int fd, void *buf, unsigned n, endereco_rede_t *de)
{
    return chamar5(SIS_RECEBER_DE, (unsigned)fd, (unsigned)buf, n, (unsigned)de, 0);
}
int opcao_socket(int fd, int opcao, unsigned valor) { return chamar5(SIS_OPCAO_SOCKET, (unsigned)fd, (unsigned)opcao, valor, 0, 0); }
int tempo_limite(int fd, unsigned ms)          { return opcao_socket(fd, OPC_TEMPO_LIMITE, ms); }
int config_rede(int op, config_rede_t *c)      { return chamar5(SIS_CONFIG_REDE, (unsigned)op, (unsigned)c, 0, 0, 0); }
int ping(uint32_t ip, info_ping_t *info, unsigned prazo_ms) { return chamar5(SIS_PING, ip, (unsigned)info, prazo_ms, 0, 0); }

int resolver_em(const char *nome, uint32_t *ip, uint32_t servidor, int porta)
{
    if (texto_para_ip(nome, ip)) return 0;
    return chamar5(SIS_RESOLVER, (unsigned)nome, (unsigned)ip, servidor, (unsigned)porta, 0);
}

int resolver(const char *nome, uint32_t *ip) { return resolver_em(nome, ip, 0, 0); }

bool texto_para_ip(const char *s, uint32_t *ip)
{
    uint32_t r = 0;
    for (int parte = 0; parte < 4; parte++) {
        if (*s < '0' || *s > '9') return false;
        uint32_t v = 0;
        for (int d = 0; *s >= '0' && *s <= '9'; d++, s++) {
            if (d == 3) return false;
            v = v * 10 + (uint32_t)(*s - '0');
        }
        if (v > 255) return false;
        r = r << 8 | v;
        if (parte < 3 && *s++ != '.') return false;
    }
    if (*s) return false;
    *ip = r;
    return true;
}

char *ip_para_texto(uint32_t ip, char *buf)
{
    formatar_texto(buf, 16, "%u.%u.%u.%u", IP_PARTES(ip));
    return buf;
}

int conectar_tcp(const char *host, int porta, unsigned tempo_ms)
{
    uint32_t ip;
    int r = ERRO_SEM_REDE;
    /* Justo después del arranque la dirección todavía puede estar llegando (DHCP). */
    for (int t = 0; r == ERRO_SEM_REDE && t < 10; t++) {
        if (t) dormir(500);
        r = resolver(host, &ip);
    }
    if (r < 0) return r;
    int fd = criar_socket(SOCK_TCP);
    if (fd < 0) return fd;
    if (tempo_ms) tempo_limite(fd, tempo_ms);
    r = conectar(fd, ip, porta);
    if (r < 0) { fechar(fd); return r; }
    tempo_limite(fd, 0);
    return fd;
}
