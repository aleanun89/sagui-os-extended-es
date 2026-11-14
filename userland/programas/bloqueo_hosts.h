// SPDX-License-Identifier: GPL-2.0-or-later
/* bloqueo_hosts.h - el analizador de las listas del bloqueo por DNS (D218) y
 * la ordenación de la tabla. Sin dependencias: lo usa el programa «bloqueo»
 * y lo prueba tests/h218_bloqueo_dns.py en el PC, con listas generadas y
 * con texto basura.
 *
 * Formatos que se aceptan, una entrada por línea («#» empieza un comentario):
 *   0.0.0.0 dominio [dominio...]      el de StevenBlack y los «hosts» clásicos
 *   127.0.0.1 dominio                 (cualquier IPv4 o IPv6 delante vale igual)
 *   dominio                           una lista simple, un dominio por línea
 *   ||dominio^                        la forma básica de las listas de Adblock
 * Un dominio vale si tiene al menos un punto, rótulos de 1 a 63 caracteres
 * [a-z0-9-_], 253 como mucho en total y no es una dirección numérica. Lo de
 * los «hosts» que no es publicidad (localhost, broadcasthost...) se ignora.
 *
 * Quien lo incluya ya debe tener uint32_t y uint64_t (y bloqueo_hash, de
 * kernel/inc/bloqueo_tabla.h). */
#ifndef SAGUI_BLOQUEO_HOSTS_H
#define SAGUI_BLOQUEO_HOSTS_H

static inline int bh_minuscula(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static inline int bh_espaco(int c) { return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f'; }

/* ¿Es un nombre de dominio que tenga sentido bloquear? */
static inline int bh_dominio_valido(const char *s, int n)
{
    if (n <= 0 || n > 253) return 0;
    int rotulo = 0, pontos = 0, so_numeros = 1;
    for (int i = 0; i < n; i++) {
        int c = bh_minuscula((unsigned char)s[i]);
        if (c == '.') {
            if (rotulo == 0) return 0;
            rotulo = 0;
            pontos++;
            continue;
        }
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
        if (!(c >= '0' && c <= '9')) so_numeros = 0;
        if (++rotulo > 63) return 0;
    }
    if (rotulo == 0 || pontos == 0 || so_numeros) return 0;
    /* lo de un /etc/hosts que no es publicidad */
    static const char *const fuera[] = { "localhost.localdomain", "local.localdomain", "ip6-localhost.localdomain" };
    for (unsigned k = 0; k < sizeof fuera / sizeof fuera[0]; k++) {
        const char *f = fuera[k];
        int i = 0;
        while (i < n && f[i] && bh_minuscula((unsigned char)s[i]) == f[i]) i++;
        if (i == n && !f[i]) return 0;
    }
    return 1;
}

/* ¿Es una dirección (lo que va delante en un «hosts»)? */
static inline int bh_endereco(const char *s, int n)
{
    int dig = 0, pontos = 0, dois_pontos = 0;
    for (int i = 0; i < n; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') dig++;
        else if (c == '.') pontos++;
        else if (c == ':') dois_pontos++;
        else if (!((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '%')) return 0;
    }
    if (dois_pontos) return 1;                 /* IPv6: ::1, fe80::1%lo0... */
    return dig > 0 && pontos == 3 && dig + pontos == n;
}

typedef void (*bh_saida_t)(void *ctx, const char *dominio, int n);

/* Analiza una línea (sin el '\n'). Devuelve cuántos dominios dio (y llama a
 * «saida» por cada uno), 0 si no había nada (vacía o comentario) o -1 si la
 * línea no tiene sentido (para contar las malas y rechazar lo que no es una
 * lista). */
static inline int bh_linha(const char *l, int n, bh_saida_t saida, void *ctx)
{
    for (int i = 0; i < n; i++)
        if (l[i] == '#') { n = i; break; }
    const char *tok[16];
    int tam[16], nt = 0;
    for (int i = 0; i < n && nt < 16;) {
        while (i < n && bh_espaco((unsigned char)l[i])) i++;
        if (i >= n) break;
        int a = i;
        while (i < n && !bh_espaco((unsigned char)l[i])) i++;
        tok[nt] = l + a;
        tam[nt++] = i - a;
    }
    if (!nt) return 0;
    int primeiro = 0;
    if (bh_endereco(tok[0], tam[0])) {
        primeiro = 1;
        if (nt == 1) return -1;                /* una dirección sola */
    }
    int bons = 0, maus = 0;
    for (int k = primeiro; k < nt; k++) {
        const char *d = tok[k];
        int t = tam[k];
        if (t > 3 && d[0] == '|' && d[1] == '|' && d[t - 1] == '^') { d += 2; t -= 3; }   /* ||dominio^ */
        if (t > 1 && d[t - 1] == '.') t--;     /* «dominio.» */
        if (bh_dominio_valido(d, t)) { saida(ctx, d, t); bons++; }
        else maus++;
    }
    if (bons) return bons;
    /* «0.0.0.0 localhost» o «127.0.0.1 broadcasthost»: lo de siempre en un
     * hosts, no es un error; un nombre sin punto tras una dirección tampoco. */
    if (primeiro && maus) {
        for (int k = 1; k < nt; k++)
            for (int i = 0; i < tam[k]; i++)
                if (tok[k][i] == '.') return -1;
        return 0;
    }
    return -1;
}

/* ------------------------------------------------------------ ordenación */

/* Montículo (heapsort): sin memoria extra y sin casos malos. */
static inline void bh_descer(uint64_t *v, uint32_t i, uint32_t n)
{
    for (;;) {
        uint32_t f = 2 * i + 1;
        if (f >= n) return;
        if (f + 1 < n && v[f + 1] > v[f]) f++;
        if (v[i] >= v[f]) return;
        uint64_t t = v[i]; v[i] = v[f]; v[f] = t;
        i = f;
    }
}

/* Ordena y quita los repetidos; devuelve cuántos quedan. */
static inline uint32_t bh_ordenar_unicos(uint64_t *v, uint32_t n)
{
    if (n < 2) return n;
    for (uint32_t i = n / 2; i-- > 0;) bh_descer(v, i, n);
    for (uint32_t k = n - 1; k > 0; k--) {
        uint64_t t = v[0]; v[0] = v[k]; v[k] = t;
        bh_descer(v, 0, k);
    }
    uint32_t m = 1;
    for (uint32_t i = 1; i < n; i++)
        if (v[i] != v[m - 1]) v[m++] = v[i];
    return m;
}

#endif
