// SPDX-License-Identifier: GPL-3.0-only
/* chaveiro.c - el llavero de la cuenta (D485; ver chaveiro.h).
 *
 * Formato de ~/.config/llavero (0600), todo texto:
 *   SAGUI-LLAVERO 1 <modo>          la cabecera, en claro y autenticada (AAD del GCM)
 *   sal <32 hex>                    aleatoria, nueva en cada guardado
 *   iv <24 hex>                     nonce del GCM, nuevo en cada guardado
 *   tag <32 hex>
 *   datos <hex>                     servicio=valor por línea (\\ y \n escapados), cifrado
 * La clave del fichero es HMAC-SHA256(K, sal). Se guarda en un fichero nuevo que luego sustituye al viejo:
 * un corte a mitad no deja el llavero a medias. */
#include "chaveiro.h"
#include "contas.h"
#include "cripto.h"

#define CABECA "SAGUI-LLAVERO 1 "
#define TEXTO_MAX (CHAVEIRO_ENTRADAS * (CHAVEIRO_SERVICO_MAX + 2 * CHAVEIRO_VALOR_MAX + 4))

static const char *const nomes_modo[] = { "vacia", "cuenta", "propia" };

const char *chaveiro_modo_texto(int modo)
{
    switch (modo) {
    case CHAV_MODO_CONTA:  return "cifrado con la contraseña de la cuenta";
    case CHAV_MODO_PROPIA: return "cifrado con la contraseña propia del llavero";
    default:               return "cifrado sin contraseña: protege poco";
    }
}

static bool k_sessao(unsigned char k[32]) { return chaveiro_op(CHAVEIRO_TOMAR, (unsigned)k, NULL) == 0; }

bool chaveiro_tem_chave_sessao(void)
{
    unsigned char k[32];
    bool r = chaveiro_op(CHAVEIRO_TOMAR, (unsigned)k, NULL) == 0;
    cripto_apagar(k, sizeof k);
    return r;
}

static void k_vazia(unsigned char k[32])
{
    char nome[CONTA_NOME_MAX];
    conta_nome(meu_uid(), nome, sizeof nome);
    chaveiro_derivar("", nome, k);
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* "n" bytes de "s" (hexadecimal) en "b"; devuelve cuántos o -1. */
static int de_hex(const char *s, unsigned char *b, int max)
{
    int n = 0;
    while (s[0] && s[0] != '\n' && s[0] != '\r') {
        int a = hex_val(s[0]), z = hex_val(s[1]);
        if (a < 0 || z < 0 || n >= max) return -1;
        b[n++] = (unsigned char)(a << 4 | z);
        s += 2;
    }
    return n;
}

static void a_hex(const unsigned char *b, int n, char *s)
{
    static const char d[] = "0123456789abcdef";
    for (int i = 0; i < n; i++) { s[2 * i] = d[b[i] >> 4]; s[2 * i + 1] = d[b[i] & 15]; }
    s[2 * n] = 0;
}

/* El valor de la línea "nome ..." del texto, o NULL. */
static const char *campo(const char *t, const char *nome)
{
    int n = (int)tam_texto(nome);
    for (const char *l = t; l && *l; ) {
        if (compara_texto_n(l, nome, (size_t)n) == 0 && l[n] == ' ') return l + n + 1;
        l = acha_char(l, '\n');
        if (l) l++;
    }
    return NULL;
}

static char *ler_tudo(const char *caminho, int *n)
{
    info_arquivo_t inf;
    if (info_arquivo(caminho, &inf) < 0 || inf.tipo != TIPO_ARQUIVO || inf.tamanho > 4 * TEXTO_MAX) return NULL;
    char *b = aloca(inf.tamanho + 1);
    if (!b) return NULL;
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) { libera(b); return NULL; }
    int t = 0, k;
    while (t < (int)inf.tamanho && (k = ler(fd, b + t, inf.tamanho - (unsigned)t)) > 0) t += k;
    fechar(fd);
    b[t] = 0;
    *n = t;
    return b;
}

/* El texto descifrado -> las entradas. */
static void analisar(chaveiro_t *c, const char *t, int n)
{
    c->n = 0;
    int i = 0;
    while (i < n && c->n < CHAVEIRO_ENTRADAS) {
        chaveiro_entrada_t *e = &c->e[c->n];
        int k = 0;
        while (i < n && t[i] != '=' && t[i] != '\n') { if (k < CHAVEIRO_SERVICO_MAX - 1) e->servico[k++] = t[i]; i++; }
        e->servico[k] = 0;
        if (i >= n || t[i] != '=') { while (i < n && t[i] != '\n') i++; i++; continue; }
        i++;
        k = 0;
        while (i < n && t[i] != '\n') {
            char ch = t[i++];
            if (ch == '\\' && i < n) { ch = t[i++]; if (ch == 'n') ch = '\n'; }
            if (k < CHAVEIRO_VALOR_MAX - 1) e->valor[k++] = ch;
        }
        e->valor[k] = 0;
        i++;
        if (e->servico[0]) c->n++;
    }
}

/* Intenta descifrar el fichero con K. 0 = abierto; CHAV_FECHADO si la etiqueta no cuadra; CHAV_DANADO. */
static int descifrar(chaveiro_t *c, const char *arq, const unsigned char k[32])
{
    if (compara_texto_n(arq, CABECA, sizeof CABECA - 1) != 0) return CHAV_DANADO;
    const char *fim_cab = acha_char(arq, '\n');
    if (!fim_cab) return CHAV_DANADO;
    int modo = CHAV_MODO_VAZIA;
    const char *m = arq + sizeof CABECA - 1;
    for (int i = 0; i < 3; i++) if (compara_texto_n(m, nomes_modo[i], tam_texto(nomes_modo[i])) == 0) modo = i;
    unsigned char sal[16], iv[12], tag[16];
    const char *s = campo(arq, "sal"), *v = campo(arq, "iv"), *g = campo(arq, "tag"), *d = campo(arq, "datos");
    if (!s || !v || !g || !d || de_hex(s, sal, 16) != 16 || de_hex(v, iv, 12) != 12 || de_hex(g, tag, 16) != 16) return CHAV_DANADO;
    int nd = 0;
    while (d[nd] && d[nd] != '\n') nd++;
    if (nd % 2 || nd / 2 > TEXTO_MAX) return CHAV_DANADO;
    unsigned char *cif = aloca((unsigned)nd / 2 + 1), *claro = aloca((unsigned)nd / 2 + 1);
    if (!cif || !claro) { libera(cif); libera(claro); return CHAV_DANADO; }
    int n = de_hex(d, cif, nd / 2);
    unsigned char kf[32];
    hmac(&HASH_SHA256, k, 32, sal, sizeof sal, kf);
    gcm_t gcm;
    gcm_chave(&gcm, kf, 32);
    bool ok = n >= 0 && gcm_decifrar(&gcm, iv, (const uint8_t *)arq, (unsigned)(fim_cab - arq), cif, claro, (unsigned)n, tag);
    cripto_apagar(kf, sizeof kf);
    cripto_apagar(&gcm, sizeof gcm);
    if (ok) {
        analisar(c, (const char *)claro, n);
        c->modo = modo;
        memcpy(c->k, k, 32);
    }
    cripto_apagar(claro, (unsigned)nd / 2 + 1);
    libera(claro);
    libera(cif);
    return ok ? CHAV_ABERTO : CHAV_FECHADO;
}

int chaveiro_abrir(chaveiro_t *c)
{
    memset(c, 0, sizeof *c);
    unsigned char k[32];
    bool sess = k_sessao(k);
    int n;
    char *arq = ler_tudo(CHAVEIRO_ARQ, &n);
    if (!arq) {   /* no hay llavero: se creará al guardar, con la K de la sesión si la hay */
        if (sess) { memcpy(c->k, k, 32); c->modo = CHAV_MODO_CONTA; }
        else { k_vazia(c->k); c->modo = CHAV_MODO_VAZIA; }
        cripto_apagar(k, sizeof k);
        return CHAV_ABERTO;
    }
    c->existia = true;
    int r = CHAV_FECHADO;
    if (sess) r = descifrar(c, arq, k);
    if (r == CHAV_FECHADO) {
        unsigned char kv[32];
        k_vazia(kv);
        r = descifrar(c, arq, kv);
        cripto_apagar(kv, sizeof kv);
    }
    cripto_apagar(k, sizeof k);
    cripto_apagar(arq, (unsigned)n);
    libera(arq);
    return r;
}

int chaveiro_abrir_com(chaveiro_t *c, const char *senha)
{
    memset(c, 0, sizeof *c);
    char nome[CONTA_NOME_MAX];
    conta_nome(meu_uid(), nome, sizeof nome);
    unsigned char k[32];
    chaveiro_derivar(senha, nome, k);
    int n;
    char *arq = ler_tudo(CHAVEIRO_ARQ, &n);
    int r;
    if (!arq) { memcpy(c->k, k, 32); c->modo = CHAV_MODO_PROPIA; r = CHAV_ABERTO; }
    else {
        c->existia = true;
        r = descifrar(c, arq, k);
        cripto_apagar(arq, (unsigned)n);
        libera(arq);
    }
    if (r == CHAV_ABERTO) chaveiro_op(CHAVEIRO_POR, meu_uid(), k);   /* para el resto de la sesión */
    cripto_apagar(k, sizeof k);
    return r;
}

const char *chaveiro_ler(const chaveiro_t *c, const char *servico)
{
    for (int i = 0; i < c->n; i++) if (compara_texto(c->e[i].servico, servico) == 0) return c->e[i].valor;
    return NULL;
}

int chaveiro_guardar(chaveiro_t *c)
{
    char *claro = aloca(TEXTO_MAX);
    if (!claro) return ERRO_SEM_MEMORIA;
    int n = 0;
    for (int i = 0; i < c->n; i++) {
        const chaveiro_entrada_t *e = &c->e[i];
        for (const char *p = e->servico; *p && n < TEXTO_MAX - 4; p++) if (*p != '=' && *p != '\n') claro[n++] = *p;
        claro[n++] = '=';
        for (const char *p = e->valor; *p && n < TEXTO_MAX - 4; p++) {
            if (*p == '\\' || *p == '\n') { claro[n++] = '\\'; claro[n++] = *p == '\n' ? 'n' : '\\'; }
            else claro[n++] = *p;
        }
        claro[n++] = '\n';
    }
    unsigned char sal[16], iv[12], tag[16], kf[32];
    if (bytes_aleatorios(sal, sizeof sal) != (int)sizeof sal || bytes_aleatorios(iv, sizeof iv) != (int)sizeof iv) {
        cripto_apagar(claro, TEXTO_MAX); libera(claro); return ERRO_INVALIDO;
    }
    char cab[48];
    formatar_texto(cab, sizeof cab, CABECA "%s", nomes_modo[c->modo >= 0 && c->modo <= 2 ? c->modo : 0]);
    unsigned char *cif = aloca((unsigned)n + 1);
    char *saida = aloca((unsigned)(2 * n + 256));
    if (!cif || !saida) { libera(cif); libera(saida); cripto_apagar(claro, TEXTO_MAX); libera(claro); return ERRO_SEM_MEMORIA; }
    hmac(&HASH_SHA256, c->k, 32, sal, sizeof sal, kf);
    gcm_t gcm;
    gcm_chave(&gcm, kf, 32);
    gcm_cifrar(&gcm, iv, (const uint8_t *)cab, (unsigned)tam_texto(cab), (const uint8_t *)claro, cif, (unsigned)n, tag);
    cripto_apagar(kf, sizeof kf);
    cripto_apagar(&gcm, sizeof gcm);
    cripto_apagar(claro, TEXTO_MAX);
    libera(claro);
    char hs[33], hv[25], ht[33];
    a_hex(sal, 16, hs); a_hex(iv, 12, hv); a_hex(tag, 16, ht);
    int k = formatar_texto(saida, (size_t)(2 * n + 256), "%s\nsal %s\niv %s\ntag %s\ndatos ", cab, hs, hv, ht);
    a_hex(cif, n, saida + k);
    k += 2 * n;
    saida[k++] = '\n';
    libera(cif);
    /* Fichero nuevo con 0600 antes de escribir nada, y luego en el sitio del viejo. */
    criar_pasta("/home/.config");
    const char *novo = CHAVEIRO_ARQ ".nuevo";
    apagar(novo);
    int fd = abrir(novo, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { libera(saida); return fd; }
    mudar_permisos(novo, 0600);
    int r = escrever_tudo(fd, saida, (unsigned)k);
    if (r >= 0) r = sincronizar(fd);
    fechar(fd);
    libera(saida);
    if (r < 0) { apagar(novo); return r; }
    r = renomear(novo, CHAVEIRO_ARQ);
    if (r < 0) { apagar(CHAVEIRO_ARQ); r = renomear(novo, CHAVEIRO_ARQ); }
    if (r >= 0) { mudar_permisos(CHAVEIRO_ARQ, 0600); c->existia = true; }
    return r < 0 ? r : 0;
}

int chaveiro_por(chaveiro_t *c, const char *servico, const char *valor)
{
    if (!servico[0] || tam_texto(servico) >= CHAVEIRO_SERVICO_MAX || tam_texto(valor) >= CHAVEIRO_VALOR_MAX) return ERRO_INVALIDO;
    int i = 0;
    while (i < c->n && compara_texto(c->e[i].servico, servico) != 0) i++;
    if (i == c->n) {
        if (c->n >= CHAVEIRO_ENTRADAS) return ERRO_LIMITE;
        c->n++;
        copia_texto(c->e[i].servico, servico, sizeof c->e[i].servico);
    }
    copia_texto(c->e[i].valor, valor, sizeof c->e[i].valor);
    return chaveiro_guardar(c);
}

int chaveiro_tirar(chaveiro_t *c, const char *servico)
{
    for (int i = 0; i < c->n; i++)
        if (compara_texto(c->e[i].servico, servico) == 0) {
            cripto_apagar(&c->e[i], sizeof c->e[i]);
            for (int k = i + 1; k < c->n; k++) c->e[k - 1] = c->e[k];
            c->n--;
            cripto_apagar(&c->e[c->n], sizeof c->e[c->n]);
            return chaveiro_guardar(c);
        }
    return ERRO_NAO_EXISTE;
}

int chaveiro_mudar_senha(chaveiro_t *c, const char *nova)
{
    unsigned char k[32];
    if (nova && nova[0]) {
        char nome[CONTA_NOME_MAX];
        conta_nome(meu_uid(), nome, sizeof nome);
        chaveiro_derivar(nova, nome, k);
        chaveiro_op(CHAVEIRO_POR, meu_uid(), k);
        c->modo = CHAV_MODO_PROPIA;
    } else if (k_sessao(k) && c->modo != CHAV_MODO_PROPIA) {
        c->modo = CHAV_MODO_CONTA;
    } else {
        k_vazia(k);
        chaveiro_op(CHAVEIRO_ESQUECER, meu_uid(), NULL);
        c->modo = CHAV_MODO_VAZIA;
    }
    memcpy(c->k, k, 32);
    cripto_apagar(k, sizeof k);
    return chaveiro_guardar(c);
}

void chaveiro_fechar(chaveiro_t *c) { cripto_apagar(c, sizeof *c); }
