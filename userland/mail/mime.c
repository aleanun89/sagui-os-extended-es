// SPDX-License-Identifier: GPL-2.0-or-later
/* mime.c - base64, quoted-printable, HTML a texto y el análisis y la construcción de
 * mensajes MIME (RFC 2045-2047) para el cliente de correo (082).
 *
 * Todo en enteros, sin libc. Los búferes grandes van con aloca (la pila del proceso es
 * de 64 KB). */
#include "correo.h"

/* ---------------------------------------------------------------- base64 */

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

unsigned base64_codificar(const uint8_t *src, unsigned n, char *dst, unsigned max)
{
    unsigned o = 0, i = 0;
    /* Cada 3 bytes dan 4 caracteres; el resto (1 o 2 bytes) se rellena con "=". */
    while (i + 3 <= n && o + 4 < max) {
        uint32_t v = (uint32_t)src[i] << 16 | (uint32_t)src[i + 1] << 8 | src[i + 2];
        dst[o++] = B64[v >> 18 & 63];
        dst[o++] = B64[v >> 12 & 63];
        dst[o++] = B64[v >> 6 & 63];
        dst[o++] = B64[v & 63];
        i += 3;
    }
    unsigned sobra = n - i;
    if (sobra && o + 4 < max) {
        uint32_t v = (uint32_t)src[i] << 16;
        if (sobra == 2) v |= (uint32_t)src[i + 1] << 8;
        dst[o++] = B64[v >> 18 & 63];
        dst[o++] = B64[v >> 12 & 63];
        dst[o++] = sobra == 2 ? B64[v >> 6 & 63] : '=';
        dst[o++] = '=';
    }
    if (o < max) dst[o] = 0;
    return o;
}

/* Valor de un carácter del alfabeto; -1 si no pertenece. */
static int b64_valor(int c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

int base64_descodificar(const char *src, unsigned n, uint8_t *dst, unsigned max)
{
    unsigned o = 0;
    uint32_t acc = 0;
    int bits = 0;
    for (unsigned i = 0; i < n; i++) {
        int c = (uint8_t)src[i];
        if (c == '=') break;                 /* relleno: fin de los datos */
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
        int v = b64_valor(c);
        if (v < 0) return -1;                /* carácter fuera del alfabeto */
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (o < max) dst[o] = (uint8_t)(acc >> bits);
            o++;
        }
    }
    return o <= max ? (int)o : (int)max;
}

/* ------------------------------------------------------ quoted-printable */

static int hexa(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int quoted_printable_descodificar(const char *src, unsigned n, uint8_t *dst, unsigned max)
{
    unsigned o = 0;
    for (unsigned i = 0; i < n; i++) {
        char c = src[i];
        if (c == '=') {
            /* "=" al final de línea (con posibles espacios) = salto blando: no se escribe. */
            unsigned j = i + 1;
            while (j < n && (src[j] == ' ' || src[j] == '\t')) j++;
            if (j >= n || src[j] == '\r' || src[j] == '\n') {
                if (j < n && src[j] == '\r') j++;
                if (j < n && src[j] == '\n') j++;
                i = j - 1;
                continue;
            }
            int h = i + 2 < n ? hexa((uint8_t)src[i + 1]) : -1;
            int l = i + 2 < n ? hexa((uint8_t)src[i + 2]) : -1;
            if (h >= 0 && l >= 0) {
                if (o < max) dst[o] = (uint8_t)(h << 4 | l);
                o++;
                i += 2;
                continue;
            }
            /* "=" suelto que no es hexadecimal: se copia tal cual. */
            if (o < max) dst[o] = '=';
            o++;
            continue;
        }
        if (o < max) dst[o] = (uint8_t)c;
        o++;
    }
    return o <= max ? (int)o : (int)max;
}

/* ---------------------------------------------------------- HTML a texto */

/* Compara el texto de la etiqueta (sin "<", sin "/") con un nombre en minúsculas. */
static bool etiqueta_eh(const char *p, unsigned n, const char *nome)
{
    unsigned k = 0;
    while (nome[k]) {
        if (k >= n) return false;
        int c = p[k];
        if (c >= 'A' && c <= 'Z') c += 32;
        if (c != nome[k]) return false;
        k++;
    }
    /* El nombre debe acabar aquí: el siguiente carácter no puede seguir siendo letra. */
    if (k < n) {
        int c = p[k];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return false;
    }
    return true;
}

/* Etiquetas que pasan a salto de línea (bloques y filas). */
static bool es_bloque(const char *p, unsigned n)
{
    static const char *const bloques[] = {
        "p", "br", "div", "tr", "li", "h1", "h2", "h3", "h4", "h5", "h6",
        "table", "ul", "ol", "blockquote", "hr", "pre", "section", "article",
        "header", "footer", "dt", "dd", 0,
    };
    for (int i = 0; bloques[i]; i++)
        if (etiqueta_eh(p, n, bloques[i])) return true;
    return false;
}

/* Etiquetas cuyo contenido se descarta por completo. */
static const char *se_ignora(const char *p, unsigned n)
{
    if (etiqueta_eh(p, n, "script")) return "script";
    if (etiqueta_eh(p, n, "style")) return "style";
    if (etiqueta_eh(p, n, "head")) return "head";
    return NULL;
}

/* Entidades más comunes. Devuelve el carácter (Latin-1) y cuántos bytes ocupa, o 0. */
static int entidad(const char *s, unsigned n, char *c)
{
    static const struct { const char *nome; char v; } ents[] = {
        { "amp;", '&' }, { "lt;", '<' }, { "gt;", '>' }, { "quot;", '"' }, { "apos;", '\'' },
        { "#39;", '\'' }, { "nbsp;", ' ' }, { "ntilde;", (char)0xF1 }, { "Ntilde;", (char)0xD1 },
        { "aacute;", (char)0xE1 }, { "eacute;", (char)0xE9 }, { "iacute;", (char)0xED },
        { "oacute;", (char)0xF3 }, { "uacute;", (char)0xFA }, { "iexcl;", (char)0xA1 },
        { "iquest;", (char)0xBF }, { "copy;", (char)0xA9 }, { "reg;", (char)0xAE },
        { "hellip;", '.' }, { "mdash;", '-' }, { "ndash;", '-' },
        { 0, 0 },
    };
    for (int i = 0; ents[i].nome; i++) {
        size_t k = tam_texto(ents[i].nome);
        if (k <= n && memcmp(s, ents[i].nome, k) == 0) { *c = ents[i].v; return (int)k; }
    }
    return 0;
}

unsigned html_para_texto(const char *html, unsigned n, char *dst, unsigned max)
{
    unsigned o = 0;
    const char *ignorar = NULL;     /* dentro de <script>/<style>/<head>: no se escribe nada */
    unsigned i = 0;
    /* Escribe un carácter sin pasarse del búfer; no escribe dos saltos seguidos. */
    #define PONER(ch) do { if (o + 1 < max) dst[o] = (ch); o++; } while (0)
    while (i < n) {
        char c = html[i];
        if (c == '<') {
            unsigned fim = i + 1;
            while (fim < n && html[fim] != '>') fim++;
            if (fim >= n) { /* "<" sin cerrar: se trata como texto */ }
            else {
                const char *p = html + i + 1;
                unsigned pn = fim - (i + 1);
                bool fecha = pn && p[0] == '/';
                if (fecha) { p++; pn--; }
                /* Quita los atributos: el nombre acaba en espacio, "/" o ">". */
                unsigned nome = 0;
                while (nome < pn && p[nome] != ' ' && p[nome] != '\t' && p[nome] != '\r' &&
                       p[nome] != '\n' && p[nome] != '/') nome++;
                if (ignorar) {
                    if (fecha && etiqueta_eh(p, nome, ignorar)) ignorar = NULL;
                } else {
                    const char *ig = se_ignora(p, nome);
                    if (ig && !fecha) ignorar = ig;
                    else if (es_bloque(p, nome)) PONER('\n');
                }
                i = fim + 1;
                continue;
            }
        }
        if (ignorar) { i++; continue; }
        if (c == '&') {
            char ch;
            int k = entidad(html + i + 1, n - i - 1, &ch);
            if (k) { PONER(ch); i += 1 + (unsigned)k; continue; }
        }
        if (c == '\r') { i++; continue; }
        if (c == '\n' || c == '\t') c = ' ';   /* el HTML trata los saltos como espacio */
        PONER(c);
        i++;
    }
    #undef PONER
    if (o < max) dst[o] = 0;
    return o < max ? o : max - 1;
}

/* ============================================================ MIME: analizar */

/* Busca una cabecera (sin distinguir mayúsculas) dentro de un bloque de cabeceras.
 * Las cabeceras largas siguen en la línea siguiente si empieza por espacio o tab.
 * Copia el valor (sin espacios al principio) en dst. Devuelve false si no existe. */
static bool cabecera(const char *h, unsigned n, const char *nome, char *dst, unsigned max)
{
    unsigned k = tam_texto(nome);
    unsigned i = 0;
    while (i < n) {
        unsigned fim = i;
        while (fim < n && h[fim] != '\n') fim++;
        /* Línea de cabecera: "Nome: valor". Las líneas que empiezan por espacio continúan la anterior. */
        bool igual = fim - i > k && h[i + k] == ':';
        if (igual) {
            for (unsigned j = 0; j < k; j++) {
                int a = h[i + j], b = nome[j];
                if (a >= 'A' && a <= 'Z') a += 32;
                if (b >= 'A' && b <= 'Z') b += 32;
                if (a != b) { igual = false; break; }
            }
        }
        if (igual) {
            unsigned v = i + k + 1, vf = fim;
            while (v < vf && (h[v] == ' ' || h[v] == '\t')) v++;
            /* Continuaciones (folding): se suman con un espacio. */
            while (fim + 1 < n && (h[fim + 1] == ' ' || h[fim + 1] == '\t')) {
                unsigned f2 = fim + 1;
                while (f2 < n && h[f2] != '\n') f2++;
                fim = f2;
            }
            unsigned o = 0;
            for (unsigned j = v; j < fim && o + 1 < max; j++) {
                char c = h[j];
                if (c == '\r' || c == '\n') c = ' ';
                if (c == '\t') c = ' ';
                if (c == ' ' && o && dst[o - 1] == ' ') continue;
                dst[o++] = c;
            }
            while (o && dst[o - 1] == ' ') o--;
            dst[o] = 0;
            return true;
        }
        if (fim == i) break;  /* línea vacía: fin de las cabeceras */
        i = fim + 1;
    }
    dst[0] = 0;
    return false;
}

/* Valor de un parámetro (p. ej. name o boundary) dentro de una cabecera con ";".
 * Admite comillas. Devuelve false si no está. */
static bool parametro(const char *v, const char *nome, char *dst, unsigned max)
{
    unsigned k = tam_texto(nome);
    const char *p = v;
    while (*p) {
        while (*p == ' ' || *p == ';' || *p == '\t') p++;
        bool igual = compara_texto_n(p, nome, k) == 0 && p[k] == '=';
        if (igual) {
            p += k + 1;
            unsigned o = 0;
            if (*p == '"') {
                p++;
                while (*p && *p != '"' && o + 1 < max) dst[o++] = *p++;
            } else {
                while (*p && *p != ';' && *p != ' ' && o + 1 < max) dst[o++] = *p++;
            }
            dst[o] = 0;
            return true;
        }
        while (*p && *p != ';') p++;
    }
    dst[0] = 0;
    return false;
}

/* Tipo principal de una cabecera Content-Type ("text/plain" de "text/plain; charset=..."). */
static void tipo_de(const char *ct, char *dst, unsigned max)
{
    unsigned o = 0;
    while (ct[o] && ct[o] != ';' && ct[o] != ' ' && o + 1 < max) {
        char c = ct[o];
        if (c >= 'A' && c <= 'Z') c += 32;
        dst[o++] = c;
    }
    dst[o] = 0;
}

/* Descodifica un cuerpo según su Content-Transfer-Encoding. Devuelve un búfer aloca'd y su tamaño. */
static char *descodificar_corpo(const char *corpo, unsigned n, const char *cte, unsigned *out_n)
{
    char *dst = aloca(n + 4);
    if (!dst) { *out_n = 0; return NULL; }
    int r;
    if (compara_texto(cte, "base64") == 0) r = base64_descodificar(corpo, n, (uint8_t *)dst, n + 4);
    else if (compara_texto(cte, "quoted-printable") == 0) r = quoted_printable_descodificar(corpo, n, (uint8_t *)dst, n + 4);
    else {
        memcpy(dst, corpo, n);
        r = (int)n;
    }
    if (r < 0) r = 0;
    dst[r] = 0;
    *out_n = (unsigned)r;
    return dst;
}

/* Quita el relleno de la última línea del cuerpo (CRLF antes del siguiente límite). */
static unsigned recortar_crlf(const char *p, unsigned n)
{
    while (n && (p[n - 1] == '\r' || p[n - 1] == '\n')) n--;
    return n;
}

/* Une un texto nuevo al acumulado (las partes text/plain de un multipart se concatenan). */
static void acrescentar(char **acc, unsigned *n_acc, const char *t, unsigned n)
{
    if (!n) return;
    char *novo = realoca(*acc, *n_acc + n + 1);
    if (!novo) return;
    memcpy(novo + *n_acc, t, n);
    *n_acc += n;
    novo[*n_acc] = 0;
    *acc = novo;
}

/* Analiza una parte MIME (cabeceras + cuerpo) y la mete en m. "nivel" evita recursiones
 * absurdas (un correo con cientos de niveles de multipart). */
static void analisar_parte(const char *p, unsigned n, mensagem_t *m, int nivel)
{
    if (nivel > 8) return;
    /* Separa cabeceras del cuerpo por la primera línea vacía. */
    unsigned fim_h = 0;
    unsigned corpo_ini = n;
    while (fim_h < n) {
        if (p[fim_h] == '\n' && (fim_h + 1 < n && p[fim_h + 1] == '\n')) { corpo_ini = fim_h + 2; break; }
        if (p[fim_h] == '\n' && (fim_h + 2 < n && p[fim_h + 1] == '\r' && p[fim_h + 2] == '\n')) { corpo_ini = fim_h + 3; break; }
        fim_h++;
    }
    const char *h = p;
    unsigned hn = corpo_ini < n ? fim_h : n;
    const char *corpo = p + corpo_ini;
    unsigned cn = n - corpo_ini;

    char ct[256], cte[64], disp[256], tipo[64], boundary[128], nome[128];
    if (!cabecera(h, hn, "Content-Type", ct, sizeof ct)) copia_texto(ct, "text/plain", sizeof ct);
    if (!cabecera(h, hn, "Content-Transfer-Encoding", cte, sizeof cte)) copia_texto(cte, "7bit", sizeof cte);
    cabecera(h, hn, "Content-Disposition", disp, sizeof disp);
    tipo_de(ct, tipo, sizeof tipo);

    /* multipart: se recorre cada parte entre los límites. */
    if (comeca_com(tipo, "multipart/") && parametro(ct, "boundary", boundary, sizeof boundary)) {
        unsigned bl = tam_texto(boundary);
        /* Las partes empiezan con "--boundary" en línea propia y acaban con "--boundary--". */
        unsigned i = 0;
        unsigned parte_ini = 0;
        bool dentro = false;
        while (i < cn) {
            bool linha_ini = i == 0 || corpo[i - 1] == '\n';
            if (linha_ini && i + 2 + bl <= cn && corpo[i] == '-' && corpo[i + 1] == '-' &&
                memcmp(corpo + i + 2, boundary, bl) == 0) {
                if (dentro) {
                    /* fin de la parte anterior: [parte_ini, i) sin el CRLF previo */
                    analisar_parte(corpo + parte_ini, recortar_crlf(corpo + parte_ini, i - parte_ini), m, nivel + 1);
                }
                dentro = true;
                /* Salta la línea del límite. */
                while (i < cn && corpo[i] != '\n') i++;
                i++;
                parte_ini = i;
                continue;
            }
            i++;
        }
        return;
    }

    /* Partes con nombre de fichero o marcadas como adjuntos: no son cuerpo. */
    bool adjunto = compara_texto_n(disp, "attachment", 10) == 0 ||
                   (parametro(ct, "name", nome, sizeof nome) && !comeca_com(tipo, "text/"));
    if (!adjunto && parametro(disp, "filename", nome, sizeof nome)) adjunto = true;

    if (adjunto) {
        if (m->n_anexos < MIME_MAX_ADJUNTOS) {
            int k = m->n_anexos++;
            if (!parametro(disp, "filename", m->anexos[k].nome, sizeof m->anexos[k].nome) &&
                !parametro(ct, "name", m->anexos[k].nome, sizeof m->anexos[k].nome))
                copia_texto(m->anexos[k].nome, "adjunto", sizeof m->anexos[k].nome);
            copia_texto(m->anexos[k].tipo, tipo, sizeof m->anexos[k].tipo);
            unsigned dn = 0;
            char *d = descodificar_corpo(corpo, cn, cte, &dn);
            libera(d);   /* solo medimos el tamaño; el contenido no se guarda en memoria */
            m->anexos[k].tamanho = dn;
        }
        return;
    }

    if (compara_texto(tipo, "text/plain") == 0 || (tipo[0] == 0)) {
        unsigned dn = 0;
        char *d = descodificar_corpo(corpo, recortar_crlf(corpo, cn), cte, &dn);
        if (d) {
            char *acc = m->texto;
            unsigned na = m->texto ? tam_texto(m->texto) : 0;
            acrescentar(&acc, &na, d, dn);
            m->texto = acc;
            libera(d);
        }
    } else if (compara_texto(tipo, "text/html") == 0) {
        unsigned dn = 0;
        char *d = descodificar_corpo(corpo, recortar_crlf(corpo, cn), cte, &dn);
        if (d) {
            char *acc = m->html;
            unsigned na = m->html ? tam_texto(m->html) : 0;
            acrescentar(&acc, &na, d, dn);
            m->html = acc;
            libera(d);
        }
    }
}

/* Cabeceras del mensaje de cara al usuario. */
static void cabeceras_visiveis(const char *p, unsigned n, mensagem_t *m)
{
    unsigned fim_h = n;
    for (unsigned i = 0; i + 1 < n; i++)
        if (p[i] == '\n' && (p[i + 1] == '\n' || (i + 2 < n && p[i + 1] == '\r' && p[i + 2] == '\n'))) { fim_h = i; break; }
    cabecera(p, fim_h, "From", m->de, sizeof m->de);
    cabecera(p, fim_h, "To", m->para, sizeof m->para);
    cabecera(p, fim_h, "Subject", m->assunto, sizeof m->assunto);
    cabecera(p, fim_h, "Date", m->data, sizeof m->data);
}

int mensagem_analisar(const char *bruta, unsigned n, mensagem_t *m)
{
    memset(m, 0, sizeof *m);
    cabeceras_visiveis(bruta, n, m);
    analisar_parte(bruta, n, m, 0);
    return 0;
}

void mensagem_analisar_liberar(mensagem_t *m)
{
    libera(m->texto);
    libera(m->html);
    m->texto = m->html = NULL;
}

/* ========================================================= MIME: construir */

/* Anexa texto a un búfer de salida con control de tamaño. */
typedef struct { char *b; unsigned n, cap; bool erro; } saida_t;

static void sai_bytes(saida_t *s, const void *p, unsigned n)
{
    if (s->erro) return;
    if (s->n + n + 1 > s->cap) {
        unsigned novo = s->cap ? s->cap : 4096;
        while (novo < s->n + n + 1) novo *= 2;
        char *b = realoca(s->b, novo);
        if (!b) { s->erro = true; return; }
        s->b = b;
        s->cap = novo;
    }
    memcpy(s->b + s->n, p, n);
    s->n += n;
    s->b[s->n] = 0;
}

static void sai(saida_t *s, const char *t) { sai_bytes(s, t, (unsigned)tam_texto(t)); }

static void sai_linha(saida_t *s, const char *t) { sai(s, t); sai(s, "\r\n"); }

/* Cabecera de texto con el Subject en UTF-8: en MIME se codifica como RFC 2047 si no es ASCII
 * puro. Aquí se envía en UTF-8 tal cual con charset declarado; el servidor no lo toca. */
static bool solo_ascii(const char *t)
{
    for (; *t; t++) if ((unsigned char)*t >= 0x80) return false;
    return true;
}

/* Convierte el texto de la pantalla (Latin-1) a UTF-8 para mandarlo con charset=UTF-8. */
static unsigned latin1_a_utf8(const char *src, char *dst, unsigned max)
{
    unsigned o = 0;
    for (unsigned i = 0; src[i]; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x80) { if (o + 1 < max) dst[o] = (char)c; o++; }
        else {
            if (o + 2 < max) { dst[o] = (char)(0xC0 | c >> 6); dst[o + 1] = (char)(0x80 | (c & 63)); }
            o += 2;
        }
    }
    if (o < max) dst[o] = 0;
    return o < max ? o : max - 1;
}

/* Escribe una línea del cuerpo en base64, partida cada 76 caracteres (RFC 2045). */
static void sai_base64(saida_t *s, const uint8_t *d, unsigned n)
{
    char linha[80];   /* 76 caracteres de base64 + el cero, como mucho */
    unsigned i = 0;
    /* 57 bytes de entrada = 76 caracteres de base64. */
    while (i < n) {
        unsigned k = n - i > 57 ? 57 : n - i;
        unsigned c = base64_codificar(d + i, k, linha, sizeof linha);
        sai_bytes(s, linha, c);
        sai(s, "\r\n");
        i += k;
    }
}

/* Nombre de fichero limpio para la cabecera: se quitan comillas y barras. */
static void nome_limpo(const char *in, char *out, unsigned max)
{
    unsigned o = 0;
    for (unsigned i = 0; in[i] && o + 1 < max; i++) {
        char c = in[i];
        if (c == '"' || c == '\\' || c == '\r' || c == '\n') continue;
        out[o++] = c;
    }
    out[o] = 0;
}

int mensagem_construir(const mensagem_envio_t *e, char **out, unsigned *n)
{
    saida_t s;
    memset(&s, 0, sizeof s);
    /* El cuerpo en UTF-8 (aloca: puede ser grande). Latin-1 -> UTF-8 duplica como mucho. */
    unsigned lt = e->texto ? tam_texto(e->texto) : 0;
    char *tmp = aloca(2 * lt + 2);
    if (!tmp) return -1;
    unsigned tcorpo = e->texto ? latin1_a_utf8(e->texto, tmp, 2 * lt + 2) : 0;
    sai_linha(&s, "MIME-Version: 1.0");
    sai(&s, "From: "); sai_linha(&s, e->de);
    sai(&s, "To: "); sai_linha(&s, e->para);
    /* Cabecera Subject: si no es ASCII, se manda en UTF-8 con =?UTF-8?B?...?= (RFC 2047). */
    if (solo_ascii(e->assunto)) {
        sai(&s, "Subject: "); sai_linha(&s, e->assunto);
    } else {
        char *u = aloca(2 * tam_texto(e->assunto) + 2);
        if (u) {
            latin1_a_utf8(e->assunto, u, 2 * tam_texto(e->assunto) + 2);
            char b[700];
            unsigned bn = base64_codificar((const uint8_t *)u, tam_texto(u), b, sizeof b);
            sai(&s, "Subject: =?UTF-8?B?"); sai_bytes(&s, b, bn); sai_linha(&s, "?=");
            libera(u);
        }
    }
    sai_linha(&s, "X-Mailer: SaguiOS correo");

    static const char limite[] = "sagui-correo-0001";
    bool tem_anexos = e->n_anexos > 0;

    if (tem_anexos) {
        sai(&s, "Content-Type: multipart/mixed; boundary=\""); sai(&s, limite); sai_linha(&s, "\"");
        sai_linha(&s, "");
        sai(&s, "--"); sai_linha(&s, limite);
    }
    /* El cuerpo en texto plano, en base64 para no preocuparse por las líneas largas ni por
     * el punto final del DATA. */
    sai_linha(&s, "Content-Type: text/plain; charset=UTF-8");
    sai_linha(&s, "Content-Transfer-Encoding: base64");
    sai_linha(&s, "");
    sai_base64(&s, (const uint8_t *)tmp, tcorpo);

    for (int i = 0; tem_anexos && i < e->n_anexos; i++) {
        char nome[160];
        nome_limpo(e->anexos[i].nome, nome, sizeof nome);
        sai(&s, "--"); sai_linha(&s, limite);
        sai(&s, "Content-Type: application/octet-stream; name=\""); sai(&s, nome); sai_linha(&s, "\"");
        sai_linha(&s, "Content-Transfer-Encoding: base64");
        sai(&s, "Content-Disposition: attachment; filename=\""); sai(&s, nome); sai_linha(&s, "\"");
        sai_linha(&s, "");
        sai_base64(&s, e->anexos[i].dados, e->anexos[i].n);
    }
    if (tem_anexos) { sai(&s, "--"); sai(&s, limite); sai_linha(&s, "--"); }

    libera(tmp);
    if (s.erro) { libera(s.b); return -1; }
    *out = s.b;
    *n = s.n;
    return (int)s.n;
}
