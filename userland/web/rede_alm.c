// SPDX-License-Identifier: GPL-2.0-or-later
/* rede_alm.c - localStorage (persistente por usuario y origen) y sessionStorage (D487).
 *
 * Cada origen (esquema + host + puerto) tiene su propio almacén: no se mezclan entre sitios.
 * localStorage se guarda en <home>/.config/cipo/storage/<huella-del-origen>, un archivo por
 * origen; sessionStorage vive en memoria mientras dure el Cipó. Tope de 1 MB por origen. */
#include "rede.h"

#define ALM_ORIGENES 24
typedef struct { char *k, *v; } par_t;
typedef struct {
    char origem[200];
    par_t *p;
    int n, cap;
    int bytes;
    bool carregado;
    uint32_t uso;
} alm_t;

static alm_t locais[ALM_ORIGENES], sesion[ALM_ORIGENES];
static mutex_t trava = MUTEX_INICIAL;

static void alm_esvaziar(alm_t *a)
{
    for (int i = 0; i < a->n; i++) { libera(a->p[i].k); libera(a->p[i].v); }
    libera(a->p);
    a->p = NULL; a->n = a->cap = 0; a->bytes = 0;
}

static void caminho_do(const char *origem, char *o, int max)
{
    uint32_t h1 = 2166136261u, h2 = 0x9E3779B9u;
    for (const char *p = origem; *p; p++) {
        h1 = (h1 ^ (uint8_t)*p) * 16777619u;
        h2 = (h2 + (uint8_t)*p) * 2654435761u;
        h2 ^= h2 >> 15;
    }
    char dir[96];
    rede_pasta_cipo(dir, sizeof dir);
    char d2[128];
    formatar_texto(d2, sizeof d2, "%s/storage", dir);
    criar_pasta(d2);
    formatar_texto(o, (size_t)max, "%s/%x%x", d2, h1, h2);
}

static void esc(const char *s, int fd)
{
    char b[512];
    int k = 0;
    for (; *s; s++) {
        if (k > (int)sizeof b - 4) { escrever_tudo(fd, b, (unsigned)k); k = 0; }
        if (*s == '\\') { b[k++] = '\\'; b[k++] = '\\'; }
        else if (*s == '\t') { b[k++] = '\\'; b[k++] = 't'; }
        else if (*s == '\n') { b[k++] = '\\'; b[k++] = 'n'; }
        else if (*s == '\r') { b[k++] = '\\'; b[k++] = 'r'; }
        else b[k++] = *s;
    }
    if (k) escrever_tudo(fd, b, (unsigned)k);
}

static void des(char *s)
{
    char *d = s;
    for (; *s; s++) {
        if (*s == '\\' && s[1]) {
            s++;
            *d++ = *s == 't' ? '\t' : *s == 'n' ? '\n' : *s == 'r' ? '\r' : *s;
        } else *d++ = *s;
    }
    *d = 0;
}

static int alm_pos(alm_t *a, const char *k)
{
    for (int i = 0; i < a->n; i++) if (compara_texto(a->p[i].k, k) == 0) return i;
    return -1;
}

static bool alm_anadir(alm_t *a, const char *k, const char *v)
{
    if (a->n == a->cap) {
        int nc = a->cap ? a->cap * 2 : 8;
        par_t *np = realoca(a->p, sizeof *np * (size_t)nc);
        if (!np) return false;
        a->p = np; a->cap = nc;
    }
    size_t lk = tam_texto(k), lv = tam_texto(v);
    char *kk = aloca(lk + 1), *vv = aloca(lv + 1);
    if (!kk || !vv) { libera(kk); libera(vv); return false; }
    memcpy(kk, k, lk + 1); memcpy(vv, v, lv + 1);
    a->p[a->n].k = kk; a->p[a->n].v = vv;
    a->n++;
    a->bytes += (int)(lk + lv);
    return true;
}

static void alm_carregar(alm_t *a)
{
    a->carregado = true;
    char caminho[160];
    caminho_do(a->origem, caminho, sizeof caminho);
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return;
    int max = ALM_QUOTA * 2 + 512;
    char *l = aloca((size_t)max);
    if (!l) { fechar(fd); return; }
    int n = ler_linha(fd, l, (unsigned)max);
    if (n >= 0 && l[0] == '#' && compara_texto(l + 1, a->origem) == 0) {
        while ((n = ler_linha(fd, l, (unsigned)max)) >= 0) {
            char *t = acha_char(l, '\t');
            if (!t) continue;
            *t = 0;
            des(l); des(t + 1);
            if (a->bytes + (int)tam_texto(l) + (int)tam_texto(t + 1) > ALM_QUOTA) break;
            alm_anadir(a, l, t + 1);
        }
    }
    libera(l);
    fechar(fd);
}

static void alm_gravar(alm_t *a)
{
    char caminho[160];
    caminho_do(a->origem, caminho, sizeof caminho);
    if (a->n == 0) { apagar(caminho); return; }
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    escrever_tudo(fd, "#", 1);
    escrever_tudo(fd, a->origem, (unsigned)tam_texto(a->origem));
    escrever_tudo(fd, "\n", 1);
    for (int i = 0; i < a->n; i++) {
        esc(a->p[i].k, fd);
        escrever_tudo(fd, "\t", 1);
        esc(a->p[i].v, fd);
        escrever_tudo(fd, "\n", 1);
    }
    fechar(fd);
}

/* El almacén del origen de `url` (se crea si falta). Con el cerrojo tomado. */
static alm_t *alm_de(int area, const char *url)
{
    char og[200];
    if (!rede_origem(url, og, sizeof og)) return NULL;
    alm_t *t = area == ALM_LOCAL ? locais : sesion;
    alm_t *libre = NULL, *viejo = &t[0];
    for (int i = 0; i < ALM_ORIGENES; i++) {
        if (t[i].origem[0] && compara_texto(t[i].origem, og) == 0) { t[i].uso = tempo_ms(); return &t[i]; }
        if (!t[i].origem[0] && !libre) libre = &t[i];
        if ((int)(t[i].uso - viejo->uso) < 0) viejo = &t[i];
    }
    alm_t *a = libre;
    if (!a) {   /* tabla llena: sale el menos usado (el local ya está en disco; el de sesión se pierde) */
        a = viejo;
        alm_esvaziar(a);
        memset(a, 0, sizeof *a);
    }
    copia_texto(a->origem, og, sizeof a->origem);
    a->uso = tempo_ms();
    if (area == ALM_LOCAL) alm_carregar(a); else a->carregado = true;
    return a;
}

char *rede_alm_obter(int area, const char *url, const char *chave)
{
    char *o = NULL;
    mutex_tomar(&trava);
    alm_t *a = alm_de(area, url);
    int i = a ? alm_pos(a, chave) : -1;
    if (i >= 0) { o = aloca(tam_texto(a->p[i].v) + 1); if (o) memcpy(o, a->p[i].v, tam_texto(a->p[i].v) + 1); }
    mutex_largar(&trava);
    return o;
}

char *rede_alm_chave(int area, const char *url, int idx)
{
    char *o = NULL;
    mutex_tomar(&trava);
    alm_t *a = alm_de(area, url);
    if (a && idx >= 0 && idx < a->n) { o = aloca(tam_texto(a->p[idx].k) + 1); if (o) memcpy(o, a->p[idx].k, tam_texto(a->p[idx].k) + 1); }
    mutex_largar(&trava);
    return o;
}

int rede_alm_tamanho(int area, const char *url)
{
    mutex_tomar(&trava);
    alm_t *a = alm_de(area, url);
    int n = a ? a->n : 0;
    mutex_largar(&trava);
    return n;
}

int rede_alm_definir(int area, const char *url, const char *chave, const char *valor)
{
    int rc = 0;
    mutex_tomar(&trava);
    alm_t *a = alm_de(area, url);
    if (!a) { mutex_largar(&trava); return -1; }
    int i = alm_pos(a, chave);
    int lk = (int)tam_texto(chave), lv = (int)tam_texto(valor);
    int nuevo = a->bytes - (i >= 0 ? (int)(tam_texto(a->p[i].k) + tam_texto(a->p[i].v)) : 0) + lk + lv;
    if (nuevo > ALM_QUOTA) rc = -1;
    else if (i >= 0) {
        char *vv = aloca((size_t)lv + 1);
        if (!vv) rc = -1;
        else {
            memcpy(vv, valor, (size_t)lv + 1);
            a->bytes += lv - (int)tam_texto(a->p[i].v);
            libera(a->p[i].v);
            a->p[i].v = vv;
        }
    } else if (!alm_anadir(a, chave, valor)) rc = -1;
    if (rc == 0 && area == ALM_LOCAL) alm_gravar(a);
    mutex_largar(&trava);
    return rc;
}

void rede_alm_remover(int area, const char *url, const char *chave)
{
    mutex_tomar(&trava);
    alm_t *a = alm_de(area, url);
    int i = a ? alm_pos(a, chave) : -1;
    if (i >= 0) {
        a->bytes -= (int)(tam_texto(a->p[i].k) + tam_texto(a->p[i].v));
        libera(a->p[i].k); libera(a->p[i].v);
        memmove(a->p + i, a->p + i + 1, sizeof a->p[0] * (size_t)(a->n - i - 1));
        a->n--;
        if (area == ALM_LOCAL) alm_gravar(a);
    }
    mutex_largar(&trava);
}

void rede_alm_limpar(int area, const char *url)
{
    mutex_tomar(&trava);
    alm_t *a = alm_de(area, url);
    if (a) {
        alm_esvaziar(a);
        if (area == ALM_LOCAL) alm_gravar(a);
    }
    mutex_largar(&trava);
}

void rede_alm_recarregar(void)
{
    mutex_tomar(&trava);
    for (int i = 0; i < ALM_ORIGENES; i++) { alm_esvaziar(&locais[i]); memset(&locais[i], 0, sizeof locais[i]); }
    mutex_largar(&trava);
}

int rede_alm_em_disco(void)
{
    char dir[96], d2[128];
    rede_pasta_cipo(dir, sizeof dir);
    formatar_texto(d2, sizeof d2, "%s/storage", dir);
    entrada_pasta_t e;
    int n = 0;
    for (int i = 0; i < 256 && listar(d2, i, &e) > 0; i++) n++;
    return n;
}
