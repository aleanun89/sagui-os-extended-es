// SPDX-License-Identifier: GPL-3.0-only
/* unidades_red.c - la lista de unidades de red de la cuenta (D485; ver unidades_red.h). */
#include "unidades_red.h"

bool unidade_red_nome_valido(const char *n)
{
    if (!n[0] || tam_texto(n) >= PONTE_NOME_MAX) return false;
    for (const char *p = n; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_')) return false;
    return true;
}

/* El campo "i" (desde 0) de una línea con tabuladores. */
static void campo(const char *l, int i, char *s, int max)
{
    while (i > 0 && *l && *l != '\n') { if (*l == '\t') i--; l++; }
    int k = 0;
    while (*l && *l != '\t' && *l != '\n' && *l != '\r') { if (k < max - 1) s[k++] = *l; l++; }
    s[k] = 0;
}

int unidades_red_ler(unidade_red_t *v, int max)
{
    int fd = abrir(UNIDADES_RED_ARQ, ABRIR_LER);
    if (fd < 0) return 0;
    char *b = aloca(8192);
    if (!b) { fechar(fd); return 0; }
    int t = 0, k;
    while (t < 8191 && (k = ler(fd, b + t, 8191u - (unsigned)t)) > 0) t += k;
    fechar(fd);
    b[t] = 0;
    int n = 0;
    for (const char *l = b; *l && n < max; ) {
        if (*l != '#' && *l != '\n') {
            unidade_red_t *u = &v[n];
            memset(u, 0, sizeof *u);
            char x[8];
            campo(l, 0, u->nome, sizeof u->nome);
            campo(l, 1, u->url, sizeof u->url);
            campo(l, 2, u->usuario, sizeof u->usuario);
            campo(l, 3, x, sizeof x);
            u->automatica = x[0] == '1';
            campo(l, 4, u->cacert, sizeof u->cacert);
            if (compara_texto(u->cacert, "-") == 0) u->cacert[0] = 0;
            campo(l, 5, x, sizeof x);
            u->inseguro = x[0] == '1';
            if (compara_texto(u->usuario, "-") == 0) u->usuario[0] = 0;
            if (unidade_red_nome_valido(u->nome) && u->url[0]) n++;
        }
        l = acha_char(l, '\n');
        if (l) l++; else break;
    }
    libera(b);
    return n;
}

static void sem_tab(char *d, const char *s, int max)
{
    int k = 0;
    for (; *s && k < max - 1; s++) d[k++] = (*s == '\t' || *s == '\n' || *s == '\r') ? ' ' : *s;
    d[k] = 0;
}

int unidades_red_gravar(const unidade_red_t *v, int n)
{
    char *b = aloca(8192);
    if (!b) return ERRO_SEM_MEMORIA;
    int t = formatar_texto(b, 8192, "# Unidades de red (D485): nombre, url, usuario, conectar al iniciar, cacert, inseguro.\n"
                                    "# Las contraseñas no van aquí: están cifradas en el llavero.\n");
    for (int i = 0; i < n && t < 7800; i++) {
        char url[UR_URL_MAX], us[UR_USUARIO_MAX], ca[128];
        sem_tab(url, v[i].url, sizeof url);
        sem_tab(us, v[i].usuario, sizeof us);
        sem_tab(ca, v[i].cacert, sizeof ca);
        t += formatar_texto(b + t, (size_t)(8192 - t), "%s\t%s\t%s\t%d\t%s\t%d\n", v[i].nome, url, us[0] ? us : "-",
                            v[i].automatica ? 1 : 0, ca[0] ? ca : "-", v[i].inseguro ? 1 : 0);
    }
    criar_pasta("/home/.config");
    const char *novo = UNIDADES_RED_ARQ ".nuevo";
    apagar(novo);
    int fd = abrir(novo, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { libera(b); return fd; }
    mudar_permisos(novo, 0600);
    int r = escrever_tudo(fd, b, (unsigned)t);
    fechar(fd);
    libera(b);
    if (r < 0) { apagar(novo); return r; }
    r = renomear(novo, UNIDADES_RED_ARQ);
    if (r < 0) { apagar(UNIDADES_RED_ARQ); r = renomear(novo, UNIDADES_RED_ARQ); }
    if (r >= 0) mudar_permisos(UNIDADES_RED_ARQ, 0600);
    return r < 0 ? r : 0;
}

int unidade_red_achar(const unidade_red_t *v, int n, const char *nome)
{
    for (int i = 0; i < n; i++) if (compara_texto(v[i].nome, nome) == 0) return i;
    return -1;
}

void unidade_red_rotulo(const char *url, char *s, int max)
{
    const char *p = url, *ini = url;
    if (comeca_com(p, "http://")) p += 7;
    else if (comeca_com(p, "https://")) p += 8;
    const char *host = p, *arroba = NULL;
    for (const char *q = p; *q && *q != '/'; q++) if (*q == '@') arroba = q;
    int k = 0;
    for (const char *q = ini; q < host && k < max - 1; q++) s[k++] = *q;
    for (const char *q = arroba ? arroba + 1 : host; *q && k < max - 1; q++) s[k++] = *q;
    s[k] = 0;
}

int unidade_red_estado(const char *nome, ponte_info_t *info)
{
    ponte_info_t v[PONTE_MAX];
    int n = ponte_op(PONTE_LISTAR, (unsigned)v, PONTE_MAX, 0, 0);
    for (int i = 0; i < n; i++)
        if (compara_texto(v[i].nome, nome) == 0) { if (info) *info = v[i]; return (int)v[i].estado; }
    return -1;
}

const char *unidade_red_estado_texto(int e)
{
    switch (e) {
    case PONTE_EST_CONECTADA:    return _("conectada");
    case PONTE_EST_DESCONECTADA: return _("desconectada");
    case PONTE_EST_CONECTANDO:   return _("conectando");
    default:                     return _("sin montar");
    }
}
