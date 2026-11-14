// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf_lex.c - léxico, objetos y estructura del PDF (mejora 059, D138).
 *
 * Un PDF es una lista de objetos numerados, cada uno con su sitio en el
 * fichero. La "tabla de referencias cruzadas" (xref) dice donde esta cada
 * uno. Este modulo:
 *
 *   - lee el xref (el clasico, con "xref", y el nuevo, que es un stream);
 *   - lee los objetos bajo demanda (con su cache);
 *   - descomprime los streams (Flate, LZW, ASCIIHex, ASCII85, RunLength);
 *   - recorre el arbol de paginas.
 *
 * Los PDF cifrados se abren con la clave que da el modulo de cripto: las
 * cadenas y los streams se descifran al leerlos (el /ID y el /Encrypt
 * nunca se descifran).
 *
 * Cuando el xref esta roto (pasa: un PDF guardado a medias), se busca el
 * patron "N G obj" por todo el fichero y se reconstruye. Es lo que hacen
 * los lectores que no se rinden. */
#include "pdf_lex.h"
#include "compressao.h"
#include "pdf_ccitt.h"
#include "pdf_cripto.h"
#include "pdf.h"

/* ------------------------------------------------------------ utiles */

/* Compara dos textos. No hay strcmp en SaguiOS (no hay libc). */
static bool igual(const char *a, const char *b)
{
    if (!a || !b) return a == b;
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static char *copiar_n(const char *s, int n)
{
    char *d = aloca((size_t)n + 1);
    if (!d) return NULL;
    memcpy(d, s, (size_t)n);
    d[n] = 0;
    return d;
}

/* El buffer de todo el fichero: se lee entero, porque el xref y los streams
 * necesitan acceso aleatorio. Los PDF de verdad caben de sobra. */
typedef struct {
    uint8_t *dados;
    uint32_t tam;
} pdf_buf_t;

/* ------------------------------------------------------------ lexer */

/* El cursor de lectura sobre el buffer. */
typedef struct {
    const uint8_t *d;
    uint32_t tam;
    uint32_t pos;
} pdf_lex_t;

static bool eh_whitespace(int c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == 0;
}

static bool eh_delimitador(int c)
{
    return eh_whitespace(c) || c == '(' || c == ')' || c == '<' || c == '>' ||
           c == '[' || c == ']' || c == '{' || c == '}' || c == '/' || c == '%';
}

/* Salta los espacios y los comentarios (de % hasta el fin de linea). */
static void lex_pular(pdf_lex_t *l)
{
    while (l->pos < l->tam) {
        uint8_t c = l->d[l->pos];
        if (eh_whitespace(c)) { l->pos++; continue; }
        if (c == '%') {
            while (l->pos < l->tam && l->d[l->pos] != '\r' && l->d[l->pos] != '\n') l->pos++;
            continue;
        }
        break;
    }
}

/* Mira el siguiente byte sin comerlo. -1 si se acabo. */
static int lex_peek(pdf_lex_t *l)
{
    lex_pular(l);
    if (l->pos >= l->tam) return -1;
    return l->d[l->pos];
}

/* Lee una palabra clave (true, false, null, obj, endobj, stream, R...). */
static bool lex_palavra(pdf_lex_t *l, char *buf, int max)
{
    lex_pular(l);
    int n = 0;
    while (l->pos < l->tam) {
        uint8_t c = l->d[l->pos];
        if (eh_delimitador(c)) break;
        if (n + 1 >= max) return false;
        buf[n++] = (char)c;
        l->pos++;
    }
    buf[n] = 0;
    return n > 0;
}

/* El numero: puede ser entero o real. */
static bool lex_numero(pdf_lex_t *l, double *valor, bool *entero)
{
    lex_pular(l);
    if (l->pos >= l->tam) return false;
    uint32_t ini = l->pos;
    bool real = false;
    if (l->d[l->pos] == '+' || l->d[l->pos] == '-') l->pos++;
    while (l->pos < l->tam) {
        uint8_t c = l->d[l->pos];
        if (c >= '0' && c <= '9') { l->pos++; continue; }
        if (c == '.') { real = true; l->pos++; continue; }
        if (c == '-' || c == '+') { l->pos++; continue; }   /* exponente */
        break;
    }
    if (l->pos == ini) return false;
    char buf[64];
    uint32_t n = l->pos - ini;
    if (n >= sizeof buf) n = sizeof buf - 1;
    memcpy(buf, l->d + ini, n);
    buf[n] = 0;
    double v = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (buf[i] == '.') { real = true; break; }
    }
    /* Conversion manual: sin libc no hay strtod. */
    uint32_t i = 0;
    bool neg = false;
    if (i < n && (buf[i] == '+' || buf[i] == '-')) { neg = buf[i] == '-'; i++; }
    double parte = 0;
    for (; i < n && buf[i] >= '0' && buf[i] <= '9'; i++)
        parte = parte * 10 + (buf[i] - '0');
    if (i < n && buf[i] == '.') {
        i++;
        double f = 0.1;
        for (; i < n && buf[i] >= '0' && buf[i] <= '9'; i++, f *= 0.1)
            parte += (buf[i] - '0') * f;
        real = true;
    }
    if (i < n && (buf[i] == 'e' || buf[i] == 'E')) {
        i++;
        bool eneg = false;
        if (i < n && (buf[i] == '+' || buf[i] == '-')) { eneg = buf[i] == '-'; i++; }
        double e = 0;
        for (; i < n && buf[i] >= '0' && buf[i] <= '9'; i++)
            e = e * 10 + (buf[i] - '0');
        double mult = 1;
        while (e-- > 0) mult *= 10;
        if (eneg) mult = 1 / mult;
        parte *= mult;
        real = true;
    }
    v = neg ? -parte : parte;
    *valor = v;
    *entero = !real;
    return true;
}

/* Un nombre: /algo. Las barras #xx se convierten en su byte. */
static char *lex_nome(pdf_lex_t *l)
{
    lex_pular(l);
    if (l->pos >= l->tam || l->d[l->pos] != '/') return NULL;
    l->pos++;
    char buf[256];
    int n = 0;
    while (l->pos < l->tam) {
        uint8_t c = l->d[l->pos];
        if (eh_whitespace(c) || eh_delimitador(c)) break;
        if (c == '#' && l->pos + 2 < l->tam) {
            int h1 = l->d[l->pos + 1], h2 = l->d[l->pos + 2];
            int v1 = (h1 >= '0' && h1 <= '9') ? h1 - '0' : (h1 >= 'a' && h1 <= 'f') ? h1 - 'a' + 10 :
                    (h1 >= 'A' && h1 <= 'F') ? h1 - 'A' + 10 : -1;
            int v2 = (h2 >= '0' && h2 <= '9') ? h2 - '0' : (h2 >= 'a' && h2 <= 'f') ? h2 - 'a' + 10 :
                    (h2 >= 'A' && h2 <= 'F') ? h2 - 'A' + 10 : -1;
            if (v1 >= 0 && v2 >= 0) {
                if (n < (int)sizeof buf - 1) buf[n++] = (char)(v1 * 16 + v2);
                l->pos += 3;
                continue;
            }
        }
        if (n < (int)sizeof buf - 1) buf[n++] = (char)c;
        l->pos++;
    }
    buf[n] = 0;
    return copiar_n(buf, n);
}

/* Una cadena literal ( ... ) con los parentesis anidados y las escapes. */
static uint8_t *lex_cadeia(pdf_lex_t *l, int *tam)
{
    lex_pular(l);
    if (l->pos >= l->tam || l->d[l->pos] != '(') return NULL;
    l->pos++;
    uint8_t *buf = aloca(4096);
    if (!buf) return NULL;
    int cap = 4096, n = 0;
    int profundidade = 1;
    while (l->pos < l->tam) {
        uint8_t c = l->d[l->pos++];
        if (c == '\\') {
            if (l->pos >= l->tam) break;
            uint8_t e = l->d[l->pos++];
            switch (e) {
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case '(': c = '('; break;
            case ')': c = ')'; break;
            case '\\': c = '\\'; break;
            case '\r':
                if (l->pos < l->tam && l->d[l->pos] == '\n') l->pos++;
                continue;   /* la continuacion de linea no da ningun byte */
            case '\n': continue;
            default:
                if (e >= '0' && e <= '7') {
                    int v = e - '0';
                    for (int k = 0; k < 2 && l->pos < l->tam; k++) {
                        uint8_t o = l->d[l->pos];
                        if (o < '0' || o > '7') break;
                        v = v * 8 + (o - '0');
                        l->pos++;
                    }
                    c = (uint8_t)v;
                }
                break;
            }
        } else if (c == '(') {
            profundidade++;
        } else if (c == ')') {
            profundidade--;
            if (profundidade == 0) break;
        }
        if (n + 1 > cap) {
            int nova = cap * 2;
            uint8_t *p = realoca(buf, (size_t)nova);
            if (!p) { libera(buf); return NULL; }
            buf = p;
            cap = nova;
        }
        buf[n++] = c;
    }
    *tam = n;
    return buf;
}

/* Una cadena hexadecimal < ... >. */
static uint8_t *lex_hex(pdf_lex_t *l, int *tam)
{
    lex_pular(l);
    if (l->pos >= l->tam || l->d[l->pos] != '<') return NULL;
    l->pos++;
    uint8_t *buf = aloca(4096);
    if (!buf) return NULL;
    int cap = 4096, n = 0;
    int nibble = -1;
    while (l->pos < l->tam) {
        uint8_t c = l->d[l->pos++];
        if (c == '>') break;
        int v = -1;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else continue;
        if (nibble < 0) { nibble = v; continue; }
        if (n + 1 > cap) {
            int nova = cap * 2;
            uint8_t *p = realoca(buf, (size_t)nova);
            if (!p) { libera(buf); return NULL; }
            buf = p;
            cap = nova;
        }
        buf[n++] = (uint8_t)(nibble * 16 + v);
        nibble = -1;
    }
    if (nibble >= 0) {
        if (n + 1 > cap) {
            int nova = cap + 1;
            uint8_t *p = realoca(buf, (size_t)nova);
            if (!p) { libera(buf); return NULL; }
            buf = p;
            cap = nova;
        }
        buf[n++] = (uint8_t)(nibble * 16);
    }
    *tam = n;
    return buf;
}

/* ------------------------------------------------------------ objetos */

typedef struct pdf_doc pdf_doc_t;

/* Un objeto que vive dentro de un "object stream": en los PDF de la version
 * 1.5 en adelante, los objetos no van sueltos por el fichero, sino todos
 * juntos dentro de un stream comprimido. */
typedef struct {
    int num;      /* numero del objeto */
    int stream;   /* numero del object stream donde vive */
    int offset;   /* donde empieza dentro del stream */
} pdf_objstm_t;

/* Una pagina, con lo que hereda de las paginas padre. */
typedef struct {
    pdf_obj_t *obj;      /* el objeto /Type /Page */
    double    box[4];    /* el /MediaBox (x0 y0 x1 y1) */
    bool      tem_box;
    int       rotate;    /* el /Rotate (0, 90, 180 o 270) */
} pdf_pagina_info_t;

struct pdf_doc {
    int      fd;
    pdf_buf_t buf;

    /* La xref: por numero de objeto, donde esta y de que tipo es. */
    uint8_t  *tipo_xref;     /* 0 = libre, 1 = normal, 2 = dentro de un stream */
    uint32_t *pos_xref;      /* la posicion (normal) o el numero del stream */
    uint16_t *gen_xref;
    uint16_t *idx_xref;      /* el indice dentro del stream (tipo 2) */
    uint32_t *off_objstm;    /* tipo 2: donde empieza dentro del stream (o ~0) */

    int       n_objetos;
    int       cap_objetos;   /* el tamano reservado de las tablas */
    pdf_obj_t **cache;       /* los objetos ya leidos */
    uint8_t  *cache_ok;

    /* Los objetos que viven dentro de un object stream. */
    pdf_objstm_t *objstm;
    int           n_objstm;

    pdf_obj_t *trailer;      /* el diccionario del trailer */
    pdf_obj_t *catalogo;
    pdf_obj_t *info;
    int       n_paginas;
    pdf_pagina_info_t *paginas;

    /* Los metadatos, ya en UTF-8. */
    char     *titulo, *autor, *produtor;

    pdf_cripto_t cripto;
    bool     tem_cripto;
    bool     precisa_contrasena;
    bool     aberto;
    bool     escaneando;     /* evita bucles al reconstruir la xref */
    bool     reconstruida;   /* ya se reconstruyo la xref (solo una vez) */
    int      prof_objeto;    /* lecturas de objetos anidadas */
};

/* forward */
static pdf_obj_t *obj_novo(pdf_obj_tipo_t tipo);
static void       obj_liberar(pdf_obj_t *o);
static pdf_obj_t *ler_objeto(pdf_doc_t *d, pdf_lex_t *l, int num, int gen, bool en_stream);

/* Crea un objeto vacio del tipo dado. */
static pdf_obj_t *obj_novo(pdf_obj_tipo_t tipo)
{
    pdf_obj_t *o = aloca_zerado(sizeof(pdf_obj_t));
    if (!o) return NULL;
    o->tipo = tipo;
    return o;
}

/* Libera un objeto y todo lo que tiene dentro. */
static void obj_liberar(pdf_obj_t *o)
{
    if (!o) return;
    if (o->tipo == PDF_OBJ_CADENA) { if (o->cadena) libera(o->cadena); }
    else if (o->tipo == PDF_OBJ_NOME) { if (o->nome) libera(o->nome); }
    else if (o->tipo == PDF_OBJ_ARREGLO) {
        for (int i = 0; i < o->n_itens; i++) obj_liberar(o->itens[i]);
        if (o->itens) libera(o->itens);
    } else if (o->tipo == PDF_OBJ_DICCIONARIO || o->tipo == PDF_OBJ_STREAM) {
        for (int i = 0; i < o->n_entradas; i++) {
            if (o->entradas[i].chave) libera(o->entradas[i].chave);
            obj_liberar(o->entradas[i].valor);
        }
        if (o->entradas) libera(o->entradas);
        if (o->tipo == PDF_OBJ_STREAM && o->datos) libera(o->datos);
    }
    libera(o);
}

/* Anade una entrada a un diccionario. */
/* Para crecer al doble sin guardar la capacidad: con n elementos se
 * reserva otra vez solo cuando n es 0 o una potencia de 2 (>= 4), y entonces
 * para el doble. Crecer de uno en uno copiaba el arreglo entero cada vez. */
static bool hay_que_crecer(int n)
{
    return n == 0 || (n >= 4 && (n & (n - 1)) == 0);
}

static size_t capacidade(int n)
{
    return n == 0 ? 4 : (size_t)n * 2;
}

static bool dict_add(pdf_obj_t *d, const char *chave, pdf_obj_t *valor)
{
    if (hay_que_crecer(d->n_entradas)) {
        pdf_entrada_t *p = realoca(d->entradas, capacidade(d->n_entradas) * sizeof(pdf_entrada_t));
        if (!p) return false;
        d->entradas = p;
    }
    d->entradas[d->n_entradas].chave = copiar_n(chave, (int)tam_texto(chave));
    if (!d->entradas[d->n_entradas].chave) return false;
    d->entradas[d->n_entradas].valor = valor;
    d->n_entradas++;
    return true;
}

pdf_obj_t *pdf_dict_buscar(const pdf_obj_t *dict, const char *chave)
{
    if (!dict) return NULL;
    if (dict->tipo != PDF_OBJ_DICCIONARIO && dict->tipo != PDF_OBJ_STREAM) return NULL;
    for (int i = 0; i < dict->n_entradas; i++)
        if (igual(dict->entradas[i].chave, chave)) return dict->entradas[i].valor;
    return NULL;
}

pdf_obj_t *pdf_dict_buscar_tipo(const pdf_obj_t *dict, const char *chave, pdf_obj_tipo_t tipo)
{
    pdf_obj_t *o = pdf_dict_buscar(dict, chave);
    if (o && o->tipo == tipo) return o;
    return NULL;
}

int pdf_obj_numero(const pdf_obj_t *o)
{
    if (!o || o->tipo != PDF_OBJ_REFERENCIA) return -1;
    return o->num;
}

/* ------------------------------------------------------------ filtros */

/* El predictor PNG (el que usan los streams de imagen y los xref nuevos). */
static bool aplicar_predictor(uint8_t **dados, int *n, int pred, int colunas, int cores, int bpc)
{
    /* Predictor 2 es el del TIFF; 10 a 15, los del PNG: cada fila lleva
     * delante un byte con su tipo (que puede variar de fila en fila) y ese
     * byte NO forma parte de los datos, asi que la salida es mas corta que
     * la entrada y hay que compactarla. */
    if (colunas <= 0) colunas = 1;
    if (cores <= 0) cores = 1;
    if (bpc <= 0) bpc = 8;
    int bpp = (cores * bpc + 7) / 8;          /* bytes por pixel */
    if (bpp < 1) bpp = 1;
    int fila = (colunas * cores * bpc + 7) / 8;
    if (fila <= 0 || fila > (1 << 24)) return true;
    uint8_t *d = *dados;
    int total = *n;

    if (pred == 2) {
        /* TIFF: solo se sabe hacer con 8 bits por componente (lo normal). */
        if (bpc != 8) return true;
        for (int pos = 0; pos + fila <= total; pos += fila)
            for (int i = cores; i < fila; i++) d[pos + i] = (uint8_t)(d[pos + i] + d[pos + i - cores]);
        return true;
    }
    if (pred < 10) return true;

    int nfilas = total / (fila + 1);
    if (total % (fila + 1)) nfilas++;          /* una ultima fila incompleta */
    uint8_t *anterior = aloca_zerado((size_t)fila);
    if (!anterior) return false;

    int ent = 0, sai = 0;
    for (int f = 0; f < nfilas && ent < total; f++) {
        uint8_t tipo = d[ent++];
        int len = total - ent;
        if (len > fila) len = fila;
        /* Se escribe en su sitio: la salida va siempre por detras de la
         * entrada (sai <= ent - 1), asi que no se pisa nada sin leer. */
        uint8_t *linha = d + sai;
        memmove(linha, d + ent, (size_t)len);
        ent += len;
        switch (tipo) {
        case 1:   /* Sub */
            for (int i = bpp; i < len; i++) linha[i] = (uint8_t)(linha[i] + linha[i - bpp]);
            break;
        case 2:   /* Up */
            for (int i = 0; i < len; i++) linha[i] = (uint8_t)(linha[i] + anterior[i]);
            break;
        case 3:   /* Average */
            for (int i = 0; i < len; i++) {
                int esq = i >= bpp ? linha[i - bpp] : 0;
                linha[i] = (uint8_t)(linha[i] + ((esq + anterior[i]) >> 1));
            }
            break;
        case 4:   /* Paeth */
            for (int i = 0; i < len; i++) {
                int a = i >= bpp ? linha[i - bpp] : 0;
                int b = anterior[i];
                int c = i >= bpp ? anterior[i - bpp] : 0;
                int p = a + b - c;
                int pa = p > a ? p - a : a - p;
                int pb = p > b ? p - b : b - p;
                int pc = p > c ? p - c : c - p;
                int pr = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                linha[i] = (uint8_t)(linha[i] + pr);
            }
            break;
        default:  /* 0 = ninguna (y los tipos desconocidos) */
            break;
        }
        memcpy(anterior, linha, (size_t)len);
        sai += len;
    }
    libera(anterior);
    *n = sai;
    return true;
}

/* Inflate de un FlateDecode (zlib o DEFLATE crudo). El tamano de salida no
 * se sabe de antemano: se empieza por algo razonable y se agranda si no
 * cabe. Si el flujo esta roto o cortado se devuelve lo que salio (es mejor
 * media pagina que ninguna, como hacen los demas lectores). */
bool pdf_inflar(const uint8_t *ent, int n_ent, uint8_t **saida, int *tam)
{
    if (!ent || n_ent <= 0) return false;
    if (n_ent >= 2) {
        uint8_t cmf = ent[0], flg = ent[1];
        /* CM = 8 (deflate), la cabecera divisible por 31 y sin diccionario. */
        if ((cmf & 0x0f) == 8 && (((unsigned)cmf << 8 | flg) % 31u) == 0 && !(flg & 0x20)) {
            ent += 2;
            n_ent -= 2;
        }
    }
    int max = n_ent < (1 << 20) ? n_ent * 8 + 4096 : n_ent * 4;
    while (true) {
        uint8_t *out = aloca((size_t)max + 1);
        if (!out) return false;
        int feitos = 0;
        int n = inflar_parcial(ent, n_ent, out, max, &feitos);
        if (n == COMP_ERRO_CHEIO && max < (256 << 20)) {
            libera(out);
            max = max < (64 << 20) ? max * 4 : (256 << 20);
            continue;
        }
        if (n < 0) n = feitos;      /* roto o cortado: lo que haya */
        if (n <= 0) { libera(out); return false; }
        out[n] = 0;
        *saida = out;
        *tam = n;
        return true;
    }
}

/* El LZW del PDF: como el de los GIF pero con el orden de codigos del TIFF
 * (el codigo se anade al final de la tabla y la tabla se agranda antes de
 * anadir el codigo). */
static bool lzw_descomprimir(const uint8_t *ent, int n, uint8_t **saida, int *tam_saida)
{
    uint8_t *out = aloca((size_t)n * 4 + 1024);
    if (!out) return false;
    int cap = n * 4 + 1024, total = 0;

    /* La tabla: cada entrada es un indice de prefijo y un sufijo. */
    int16_t pre[4096];
    uint8_t suf[4096];
    for (int i = 0; i < 256; i++) { pre[i] = -1; suf[i] = (uint8_t)i; }
    int prox = 258, tam_cod = 9;

    int cod_anterior = -1;
    uint8_t anterior[4096];
    int n_anterior = 0;

    uint32_t acum = 0;
    int nbits = 0;
    uint32_t pos = 0;

    while (true) {
        while (nbits < tam_cod && pos < (uint32_t)n) {
            acum = (acum << 8) | ent[pos++];
            nbits += 8;
        }
        if (nbits < tam_cod) break;
        int cod = (int)((acum >> (nbits - tam_cod)) & ((1u << tam_cod) - 1));
        nbits -= tam_cod;

        if (cod == 256) {          /* Clear: vuelve a empezar */
            prox = 258;
            tam_cod = 9;
            cod_anterior = -1;
            continue;
        }
        if (cod == 257) break;     /* EOD */

        int ini;
        if (cod < prox && (cod < 256 || pre[cod] >= 0)) {
            ini = cod;
        } else if (cod_anterior >= 0) {
            ini = cod_anterior;    /* el codigo KwKwK */
        } else {
            break;
        }

        /* Se copia la cadena. */
        int tmp[4096];
        int nt = 0;
        int c = ini;
        while (c >= 256 && nt < 4096) { tmp[nt++] = suf[c]; c = pre[c]; }
        tmp[nt++] = (c < 256) ? (uint8_t)c : 0;

        uint8_t cadena[4096];
        for (int i = 0; i < nt; i++) cadena[i] = tmp[nt - 1 - i];

        if (total + nt > cap) {
            int nova = cap * 2;
            uint8_t *p = realoca(out, (size_t)nova);
            if (!p) { libera(out); return false; }
            out = p;
            cap = nova;
        }
        memcpy(out + total, cadena, (size_t)nt);
        total += nt;

        if (cod_anterior >= 0 && prox < 4096) {
            pre[prox] = (int16_t)cod_anterior;
            suf[prox] = cadena[0];
            prox++;
            if (prox + 1 >= (1 << tam_cod) && tam_cod < 12) tam_cod++;
        }
        cod_anterior = cod;

        /* Se guarda para el caso KwKwK. */
        if (nt < (int)sizeof anterior) {
            memcpy(anterior, cadena, (size_t)nt);
            n_anterior = nt;
        }
    }
    (void)anterior; (void)n_anterior;
    *saida = out;
    *tam_saida = total;
    return true;
}

/* ASCII85: cinco caracteres por cuatro bytes, con 'z' para los ceros. */
static bool a85_descomprimir(const uint8_t *ent, int n, uint8_t **saida, int *tam_saida)
{
    uint8_t *out = aloca((size_t)n + 64);
    if (!out) return false;
    int cap = n + 64, total = 0;
    uint32_t acum = 0;
    int nbits = 0;
    int marcas = 0;
    for (int i = 0; i < n; i++) {
        uint8_t c = ent[i];
        if (eh_whitespace(c)) continue;
        if (c == '~') break;
        if (c == 'z' && nbits == 0) {
            if (total + 4 > cap) { int nova = cap * 2; uint8_t *p = realoca(out, (size_t)nova); if (!p) { libera(out); return false; } out = p; cap = nova; }
            memset(out + total, 0, 4);
            total += 4;
            continue;
        }
        if (c < '!' || c > 'u') continue;
        acum = acum * 85 + (c - '!');
        nbits += 5;
        if (nbits >= 8) {
            nbits -= 8;
            if (total + 1 > cap) { int nova = cap * 2; uint8_t *p = realoca(out, (size_t)nova); if (!p) { libera(out); return false; } out = p; cap = nova; }
            out[total++] = (uint8_t)((acum >> nbits) & 0xFF);
        }
        marcas++;
    }
    /* Los ultimos caracteres sueltos. */
    if (nbits >= 5) {
        acum = acum * 85 + 84;
        nbits += 5;
        if (nbits >= 8) {
            nbits -= 8;
            if (total + 1 > cap) { int nova = cap + 1; uint8_t *p = realoca(out, (size_t)nova); if (!p) { libera(out); return false; } out = p; cap = nova; }
            out[total++] = (uint8_t)((acum >> nbits) & 0xFF);
        }
    }
    *saida = out;
    *tam_saida = total;
    return true;
}

/* El filtro de imagen: solo se necesita saber cuanto mide (lo usa el
 * renderizador para las imagenes). El DCTDecode lo deja como esta (el
 * renderizador usa el JPEG que ya hay en la libsagui). */
static bool aplicar_filtros(pdf_doc_t *d, pdf_obj_t *stream, uint8_t **dados, int *tam)
{
    pdf_obj_t *filtros = pdf_dict_buscar(stream, "Filter");
    if (!filtros) {
        /* Sin filtro, los bytes del stream se quedan como estan. */
        *dados = stream->datos;
        *tam = stream->tam_datos;
        return true;
    }
    if (filtros->tipo == PDF_OBJ_NOME) {
        pdf_obj_t *f = obj_novo(PDF_OBJ_ARREGLO);
        if (!f) return false;
        pdf_obj_t *nome = obj_novo(PDF_OBJ_NOME);
        if (!nome) { obj_liberar(f); return false; }
        nome->nome = copiar_n(filtros->nome, (int)tam_texto(filtros->nome));
        if (!f->itens) {
            f->itens = aloca(sizeof(pdf_obj_t *));
            if (!f->itens) { obj_liberar(nome); obj_liberar(f); return false; }
        }
        f->itens[0] = nome;
        f->n_itens = 1;
        filtros = f;
    }
    if (filtros->tipo != PDF_OBJ_ARREGLO) {
        /* Un filtro que no es una lista: los bytes se quedan como estan. */
        *dados = stream->datos;
        *tam = stream->tam_datos;
        return true;
    }

    uint8_t *actual = stream->datos;
    int tam_actual = stream->tam_datos;

    for (int i = 0; i < filtros->n_itens; i++) {
        pdf_obj_t *f = filtros->itens[i];
        const char *nome = (f->tipo == PDF_OBJ_NOME) ? f->nome : (f->tipo == PDF_OBJ_CADENA ? f->cadena : NULL);
        if (!nome) continue;

        if (igual(nome, "FlateDecode") || igual(nome, "Fl")) {
            /* El inflate de la libsagui es DEFLATE crudo (RFC 1951). El
             * FlateDecode del PDF es un flujo zlib (RFC 1950): dos bytes de
             * cabecera (CMF y FLG), el DEFLATE y cuatro bytes de Adler-32 al
             * final. Algunos productores usan DEFLATE crudo, asi que se
             * prueban las dos formas. */
            uint8_t *out = NULL;
            int n = 0;
            if (!pdf_inflar(actual, tam_actual, &out, &n)) return false;
            if (actual != stream->datos) libera(actual);
            actual = out;
            tam_actual = n;
        } else if (igual(nome, "LZWDecode") || igual(nome, "LZW")) {
            uint8_t *out = NULL;
            int n = 0;
            if (!lzw_descomprimir(actual, tam_actual, &out, &n)) return false;
            if (actual != stream->datos) libera(actual);
            actual = out;
            tam_actual = n;
        } else if (igual(nome, "ASCIIHexDecode") || igual(nome, "AHx")) {
            uint8_t *out = aloca((size_t)tam_actual / 2 + 2);
            if (!out) return false;
            int n = 0, nibble = -1;
            for (int k = 0; k < tam_actual; k++) {
                uint8_t c = actual[k];
                if (c == '>') break;
                int v = -1;
                if (c >= '0' && c <= '9') v = c - '0';
                else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
                else continue;
                if (nibble < 0) { nibble = v; continue; }
                out[n++] = (uint8_t)(nibble * 16 + v);
                nibble = -1;
            }
            if (nibble >= 0) out[n++] = (uint8_t)(nibble * 16);
            if (actual != stream->datos) libera(actual);
            actual = out;
            tam_actual = n;
        } else if (igual(nome, "ASCII85Decode") || igual(nome, "A85")) {
            uint8_t *out = NULL;
            int n = 0;
            if (!a85_descomprimir(actual, tam_actual, &out, &n)) return false;
            if (actual != stream->datos) libera(actual);
            actual = out;
            tam_actual = n;
        } else if (igual(nome, "RunLengthDecode") || igual(nome, "RL")) {
            uint8_t *out = aloca((size_t)tam_actual * 2 + 64);
            if (!out) return false;
            int n = 0, i = 0;
            while (i < tam_actual) {
                int8_t l = (int8_t)actual[i++];
                if (l >= 0) {
                    int cnt = l + 1;
                    for (int k = 0; k < cnt && i < tam_actual; k++) out[n++] = actual[i++];
                } else if (l != -128) {
                    int cnt = 1 - l;
                    if (i >= tam_actual) break;
                    uint8_t b = actual[i++];
                    for (int k = 0; k < cnt; k++) out[n++] = b;
                } else {
                    break;
                }
            }
            if (actual != stream->datos) libera(actual);
            actual = out;
            tam_actual = n;
        } else if (igual(nome, "CCITTFaxDecode") || igual(nome, "CCF")) {
            /* Los escaneados en blanco y negro (fax grupo 3 o 4). */
            pdf_ccitt_par_t cp;
            cp.k = 0; cp.colunas = 1728; cp.filas = 0; cp.preto_1 = false; cp.alinhar = false;
            pdf_obj_t *pc = pdf_dict_buscar(stream, "DecodeParms");
            if (!pc) pc = pdf_dict_buscar(stream, "DP");
            if (pc && pc->tipo == PDF_OBJ_REFERENCIA) pc = pdf_doc_objeto(d, pc->num);
            if (pc && pc->tipo == PDF_OBJ_ARREGLO) pc = (i < pc->n_itens) ? pc->itens[i] : NULL;
            if (pc && pc->tipo == PDF_OBJ_REFERENCIA) pc = pdf_doc_objeto(d, pc->num);
            if (pc && pc->tipo == PDF_OBJ_DICCIONARIO) {
                pdf_obj_t *v;
                if ((v = pdf_dict_buscar_tipo(pc, "K", PDF_OBJ_NUMERO))) cp.k = (int)v->numero;
                if ((v = pdf_dict_buscar_tipo(pc, "Columns", PDF_OBJ_NUMERO))) cp.colunas = (int)v->numero;
                if ((v = pdf_dict_buscar_tipo(pc, "Rows", PDF_OBJ_NUMERO))) cp.filas = (int)v->numero;
                if ((v = pdf_dict_buscar_tipo(pc, "BlackIs1", PDF_OBJ_BOOL))) cp.preto_1 = v->booleano;
                if ((v = pdf_dict_buscar_tipo(pc, "EncodedByteAlign", PDF_OBJ_BOOL))) cp.alinhar = v->booleano;
            }
            /* Sin /Rows, el /Height de la imagen. */
            if (cp.filas <= 0) {
                pdf_obj_t *h = pdf_dict_buscar(stream, "Height");
                if (!h) h = pdf_dict_buscar(stream, "H");
                if (h && h->tipo == PDF_OBJ_NUMERO) cp.filas = (int)h->numero;
            }
            uint8_t *out = NULL;
            int n = 0;
            if (!pdf_ccitt_decodificar(actual, tam_actual, &cp, &out, &n)) return false;
            if (actual != stream->datos) libera(actual);
            actual = out;
            tam_actual = n;
            continue;           /* los DecodeParms ya son del CCITT, no un predictor */
        } else if (igual(nome, "DCTDecode") || igual(nome, "DCT") ||
                   igual(nome, "JPXDecode")) {
            /* Se deja como esta: el renderizador lo sabe tratar. */
            continue;
        } else {
            /* Filtro desconocido: se deja como esta. */
            continue;
        }

        /* Los parametros del filtro (el predictor): un diccionario si hay un
         * solo filtro, o una lista con uno por filtro (o null). */
        pdf_obj_t *par = pdf_dict_buscar(stream, "DecodeParms");
        if (!par) par = pdf_dict_buscar(stream, "DP");
        if (par && par->tipo == PDF_OBJ_REFERENCIA) par = pdf_doc_objeto(d, par->num);
        if (par && par->tipo == PDF_OBJ_ARREGLO)
            par = (i < par->n_itens) ? par->itens[i] : NULL;
        if (par && par->tipo == PDF_OBJ_REFERENCIA) par = pdf_doc_objeto(d, par->num);
        if (par && par->tipo == PDF_OBJ_DICCIONARIO) {
            pdf_obj_t *pred = pdf_dict_buscar_tipo(par, "Predictor", PDF_OBJ_NUMERO);
            if (pred && pred->numero >= 2) {
                pdf_obj_t *cols = pdf_dict_buscar_tipo(par, "Columns", PDF_OBJ_NUMERO);
                pdf_obj_t *cores = pdf_dict_buscar_tipo(par, "Colors", PDF_OBJ_NUMERO);
                pdf_obj_t *bpc = pdf_dict_buscar_tipo(par, "BitsPerComponent", PDF_OBJ_NUMERO);
                aplicar_predictor(&actual, &tam_actual, (int)pred->numero,
                                  cols ? (int)cols->numero : 1,
                                  cores ? (int)cores->numero : 1,
                                  bpc ? (int)bpc->numero : 8);
            }
        }
    }

    *dados = actual;
    *tam = tam_actual;
    return true;
}

/* ------------------------------------------------------------ lectura de objetos */

/* Lee un objeto del buffer. Si encuentra "N G R" devuelve una referencia. */
static pdf_obj_t *ler_objeto(pdf_doc_t *d, pdf_lex_t *l, int num, int gen, bool en_stream)
{
    (void)d;
    int c = lex_peek(l);
    if (c < 0) return NULL;

    if (c == '/') {
        char *nome = lex_nome(l);
        if (!nome) return NULL;
        pdf_obj_t *o = obj_novo(PDF_OBJ_NOME);
        if (!o) { libera(nome); return NULL; }
        o->nome = nome;
        return o;
    }
    if (c == '(') {
        int tam = 0;
        uint8_t *s = lex_cadeia(l, &tam);
        if (!s) return NULL;
        pdf_obj_t *o = obj_novo(PDF_OBJ_CADENA);
        if (!o) { libera(s); return NULL; }
        o->cadena = (char *)s;
        o->tam_cadena = tam;
        /* Las cadenas se descifran (si el documento esta cifrado). */
        if (d->tem_cripto && !en_stream) {
            int n = pdf_cripto_descifrar(&d->cripto, s, tam, num, gen);
            if (n < 0) { obj_liberar(o); return NULL; }
            o->tam_cadena = n;
        }
        return o;
    }
    if (c == '<' && l->pos + 1 < l->tam && l->d[l->pos + 1] == '<') {
        l->pos += 2;
        pdf_obj_t *dic = obj_novo(PDF_OBJ_DICCIONARIO);
        if (!dic) return NULL;
        while (true) {
            int p = lex_peek(l);
            if (p < 0) { obj_liberar(dic); return NULL; }
            if (p == '>') { l->pos++; if (l->pos < l->tam && l->d[l->pos] == '>') l->pos++; break; }
            if (p != '/') { obj_liberar(dic); return NULL; }
            char *chave = lex_nome(l);
            if (!chave) { obj_liberar(dic); return NULL; }
            pdf_obj_t *valor = ler_objeto(d, l, num, gen, en_stream);
            if (!valor) { libera(chave); obj_liberar(dic); return NULL; }
            if (!dict_add(dic, chave, valor)) { libera(chave); obj_liberar(valor); obj_liberar(dic); return NULL; }
            libera(chave);
        }
        /* Tras el diccionario puede venir un stream. */
        uint32_t guarda = l->pos;
        char palavra[16];
        if (lex_palavra(l, palavra, sizeof palavra) && igual(palavra, "stream")) {
            /* Despues de "stream" viene un fin de linea. */
            if (l->pos < l->tam && l->d[l->pos] == '\r') l->pos++;
            if (l->pos < l->tam && l->d[l->pos] == '\n') l->pos++;
            uint32_t inicio = l->pos;
            /* La longitud: /Length, que puede ser una referencia. */
            int64_t tam = -1;
            pdf_obj_t *len = pdf_dict_buscar(dic, "Length");
            if (len && len->tipo == PDF_OBJ_NUMERO) tam = (int64_t)len->numero;
            else if (len && len->tipo == PDF_OBJ_REFERENCIA) {
                pdf_obj_t *rl = pdf_doc_objeto(d, len->num);
                if (rl && rl->tipo == PDF_OBJ_NUMERO) tam = (int64_t)rl->numero;
            }
            if (tam < 0 || (uint64_t)tam > (uint64_t)(l->tam - inicio)) {
                /* La longitud no vale: se busca el "endstream". */
                tam = -1;
                for (uint32_t k = inicio; k + 9 <= l->tam; k++) {
                    if (memcmp(l->d + k, "endstream", 9) == 0) {
                        uint32_t fim = k;
                        if (fim > inicio && l->d[fim - 1] == '\n') fim--;
                        if (fim > inicio && l->d[fim - 1] == '\r') fim--;
                        tam = (int64_t)(fim - inicio);
                        break;
                    }
                }
            }
            if (tam < 0) { obj_liberar(dic); return NULL; }
            uint8_t *bytes = aloca((size_t)tam + 1);
            if (!bytes) { obj_liberar(dic); return NULL; }
            memcpy(bytes, l->d + inicio, (size_t)tam);
            bytes[tam] = 0;
            l->pos = inicio + (uint32_t)tam;
            /* Se pasa de "stream" para quitar el "endstream". */
            lex_palavra(l, palavra, sizeof palavra);

            dic->tipo = PDF_OBJ_STREAM;
            dic->datos = bytes;
            dic->tam_datos = (int)tam;

            /* Los streams se descifran (menos los que son imagenes DCT). */
            if (d->tem_cripto) {
                bool e_imagem = false;
                pdf_obj_t *f = pdf_dict_buscar(dic, "Filter");
                if (f) {
                    const char *fn = (f->tipo == PDF_OBJ_NOME) ? f->nome : NULL;
                    if (fn && (igual(fn, "DCTDecode") || igual(fn, "DCT"))) e_imagem = true;
                }
                if (!e_imagem) {
                    int n = pdf_cripto_descifrar(&d->cripto, bytes, (int)tam, num, gen);
                    if (n < 0) { obj_liberar(dic); return NULL; }
                    dic->tam_datos = n;
                }
            }
            /* Los filtros. */
            uint8_t *saida = NULL;
            int tam_saida = 0;
            if (!aplicar_filtros(d, dic, &saida, &tam_saida)) { obj_liberar(dic); return NULL; }
            dic->datos = saida;
            dic->tam_datos = tam_saida;
        } else {
            l->pos = guarda;   /* no era un stream */
        }
        return dic;
    }
    if (c == '<') {
        /* Una cadena en hexadecimal: <48656C6C6F> */
        int tam = 0;
        uint8_t *s = lex_hex(l, &tam);
        if (!s) return NULL;
        pdf_obj_t *o = obj_novo(PDF_OBJ_CADENA);
        if (!o) { libera(s); return NULL; }
        o->cadena = (char *)s;
        o->tam_cadena = tam;
        if (d->tem_cripto && !en_stream) {
            int n = pdf_cripto_descifrar(&d->cripto, s, tam, num, gen);
            if (n < 0) { obj_liberar(o); return NULL; }
            o->tam_cadena = n;
        }
        return o;
    }
    if (c == '[') {
        l->pos++;
        pdf_obj_t *arr = obj_novo(PDF_OBJ_ARREGLO);
        if (!arr) return NULL;
        while (true) {
            int p = lex_peek(l);
            if (p < 0) { obj_liberar(arr); return NULL; }
            if (p == ']') { l->pos++; break; }
            pdf_obj_t *v = ler_objeto(d, l, num, gen, en_stream);
            if (!v) { obj_liberar(arr); return NULL; }
            if (hay_que_crecer(arr->n_itens)) {
                pdf_obj_t **p2 = realoca(arr->itens, capacidade(arr->n_itens) * sizeof(pdf_obj_t *));
                if (!p2) { obj_liberar(v); obj_liberar(arr); return NULL; }
                arr->itens = p2;
            }
            arr->itens[arr->n_itens++] = v;
        }
        return arr;
    }
    if (c == ']' || c == '>' || c == '}' || c == ')') {
        l->pos++;
        return NULL;
    }
    if (c == '{') {
        /* Un PDF con PostScript dentro: se salta el bloque. */
        l->pos++;
        int nivel = 1;
        while (l->pos < l->tam && nivel > 0) {
            if (l->d[l->pos] == '{') nivel++;
            else if (l->d[l->pos] == '}') nivel--;
            l->pos++;
        }
        return NULL;
    }

    /* Numero, palabra clave o referencia. */
    double v = 0;
    bool ent = false;
    uint32_t guarda = l->pos;
    if (lex_numero(l, &v, &ent)) {
        /* A ver si es una referencia "n g R". */
        uint32_t depois = l->pos;
        double v2 = 0;
        bool ent2 = false;
        if (lex_numero(l, &v2, &ent2)) {
            char palavra[8];
            uint32_t guarda2 = l->pos;
            if (lex_palavra(l, palavra, sizeof palavra) && igual(palavra, "R")) {
                pdf_obj_t *o = obj_novo(PDF_OBJ_REFERENCIA);
                if (!o) return NULL;
                o->num = (int)v;
                o->gen = (int)v2;
                return o;
            }
            l->pos = guarda2;
        }
        l->pos = depois;
        pdf_obj_t *o = obj_novo(PDF_OBJ_NUMERO);
        if (!o) return NULL;
        o->numero = v;
        o->inteiro = ent;
        return o;
    }
    l->pos = guarda;

    char palavra[64];
    if (lex_palavra(l, palavra, sizeof palavra)) {
        if (igual(palavra, "true")) {
            pdf_obj_t *o = obj_novo(PDF_OBJ_BOOL);
            if (o) o->booleano = true;
            return o;
        }
        if (igual(palavra, "false")) {
            pdf_obj_t *o = obj_novo(PDF_OBJ_BOOL);
            if (o) o->booleano = false;
            return o;
        }
        if (igual(palavra, "null")) return obj_novo(PDF_OBJ_NULO);
        return NULL;
    }
    return NULL;
}

/* ------------------------------------------------------------ xref */

/* Agranda la tabla de la xref (y la cache de objetos) para que quepan 'n'
 * objetos. Los objetos nuevos salen vacios. */
static bool xref_crescer(pdf_doc_t *d, int n)
{
    if (n <= d->n_objetos) return true;
    if (n > 4000000) return false;          /* un numero de objeto absurdo */
    if (n <= d->cap_objetos) {
        for (int i = d->n_objetos; i < n; i++) {
            d->tipo_xref[i] = 0;
            d->pos_xref[i] = 0;
            d->gen_xref[i] = 0;
            d->cache[i] = NULL;
            d->cache_ok[i] = 0;
            d->off_objstm[i] = 0xFFFFFFFFu;
        }
        d->n_objetos = n;
        return true;
    }
    /* Se crece al doble: la xref se lee entrada a entrada y crecer de uno
     * en uno copiaba las tablas enteras cada vez (cuadratico: un PDF de 8000
     * objetos tardaba un segundo en abrirse). */
    int novo = d->cap_objetos ? d->cap_objetos * 2 : 256;
    while (novo < n) novo *= 2;
    int pedido = n;
    n = novo;
    uint8_t *t = realoca(d->tipo_xref, (size_t)n);
    if (!t) return false;
    d->tipo_xref = t;
    uint32_t *p = realoca(d->pos_xref, (size_t)n * sizeof(uint32_t));
    if (!p) return false;
    d->pos_xref = p;
    uint16_t *g = realoca(d->gen_xref, (size_t)n * sizeof(uint16_t));
    if (!g) return false;
    d->gen_xref = g;
    pdf_obj_t **c = realoca(d->cache, (size_t)n * sizeof(pdf_obj_t *));
    if (!c) return false;
    d->cache = c;
    uint8_t *k = realoca(d->cache_ok, (size_t)n);
    if (!k) return false;
    d->cache_ok = k;
    uint32_t *o = realoca(d->off_objstm, (size_t)n * sizeof(uint32_t));
    if (!o) return false;
    d->off_objstm = o;
    d->cap_objetos = n;
    for (int i = d->n_objetos; i < pedido; i++) {
        d->tipo_xref[i] = 0;
        d->pos_xref[i] = 0;
        d->gen_xref[i] = 0;
        d->cache[i] = NULL;
        d->cache_ok[i] = 0;
        d->off_objstm[i] = 0xFFFFFFFFu;
    }
    d->n_objetos = pedido;
    return true;
}

/* Apunta un objeto en la xref. Gana el primero que se anota: como el xref se
 * lee del mas nuevo al mas viejo (el /Prev), el mas nuevo manda. */
static void xref_poe(pdf_doc_t *d, int num, uint8_t tipo, uint32_t pos, uint16_t gen)
{
    if (num < 0) return;
    if (!xref_crescer(d, num + 1)) return;
    if (d->tipo_xref[num] != 0) return;
    d->tipo_xref[num] = tipo;
    d->pos_xref[num] = pos;
    d->gen_xref[num] = gen;
}

/* Lee un entero decimal en la posicion *pos (se salta los espacios antes). */
static bool ler_uint(const uint8_t *b, uint32_t tam, uint32_t *pos, uint32_t *valor)
{
    uint32_t p = *pos;
    while (p < tam && eh_whitespace(b[p])) p++;
    if (p >= tam || b[p] < '0' || b[p] > '9') return false;
    uint32_t v = 0;
    while (p < tam && b[p] >= '0' && b[p] <= '9') {
        if (v < 0x10000000u) v = v * 10 + (uint32_t)(b[p] - '0');
        p++;
    }
    *pos = p;
    *valor = v;
    return true;
}

/* El desplazamiento del "startxref" (la ultima vez que sale en el fichero). */
static bool achar_startxref(pdf_doc_t *d, uint32_t *desl)
{
    const uint8_t *b = d->buf.dados;
    uint32_t tam = d->buf.tam;
    if (tam < 9) return false;
    for (int64_t i = (int64_t)tam - 9; i >= 0; i--) {
        if (memcmp(b + i, "startxref", 9) == 0) {
            uint32_t p = (uint32_t)i + 9;
            return ler_uint(b, tam, &p, desl);
        }
    }
    return false;
}

static bool ler_xref_stream(pdf_doc_t *d, uint32_t pos, uint32_t *prev);

/* La tabla de referencias clasica: "xref", las subsecciones y el "trailer". */
static bool ler_xref_tabela(pdf_doc_t *d, uint32_t pos, uint32_t *prev)
{
    const uint8_t *b = d->buf.dados;
    uint32_t tam = d->buf.tam;
    uint32_t p = pos;
    while (p < tam && eh_whitespace(b[p])) p++;
    if (p + 4 > tam || memcmp(b + p, "xref", 4) != 0) return false;
    p += 4;
    *prev = 0;
    while (true) {
        uint32_t q = p;
        while (q < tam && eh_whitespace(b[q])) q++;
        if (q + 7 <= tam && memcmp(b + q, "trailer", 7) == 0) {
            pdf_lex_t l = { b, tam, q + 7 };
            lex_pular(&l);
            pdf_obj_t *tr = ler_objeto(d, &l, 0, 0, true);
            if (tr) {
                /* El /Prev es el de ESTE trailer (antes se leia siempre el
                 * del primero y las actualizaciones viejas no se leian). */
                pdf_obj_t *pr = pdf_dict_buscar_tipo(tr, "Prev", PDF_OBJ_NUMERO);
                if (pr && pr->numero >= 0) *prev = (uint32_t)pr->numero;
                /* Los PDF "hibridos" (Word, Acrobat) guardan en /XRefStm los
                 * objetos que van dentro de object streams. */
                pdf_obj_t *xs = pdf_dict_buscar_tipo(tr, "XRefStm", PDF_OBJ_NUMERO);
                uint32_t pos_xs = (xs && xs->numero > 0 && xs->numero < tam) ? (uint32_t)xs->numero : 0;
                if (!d->trailer) d->trailer = tr;
                else obj_liberar(tr);
                if (pos_xs) {
                    uint32_t lixo = 0;
                    ler_xref_stream(d, pos_xs, &lixo);
                }
            }
            return true;
        }
        /* Una subseccion: "primer numero" y "cuantos". */
        uint32_t inicio = 0, cont = 0;
        if (!ler_uint(b, tam, &p, &inicio)) return false;
        if (!ler_uint(b, tam, &p, &cont)) return false;
        for (uint32_t k = 0; k < cont; k++) {
            uint32_t off = 0, gen = 0;
            if (!ler_uint(b, tam, &p, &off)) return false;
            if (!ler_uint(b, tam, &p, &gen)) return false;
            while (p < tam && eh_whitespace(b[p])) p++;
            bool livre = (p < tam && b[p] == 'f');
            if (p < tam) p++;
            if (!livre) xref_poe(d, (int)(inicio + k), 1, off, (uint16_t)gen);
        }
    }
}

/* El xref de los PDF modernos: un stream con los datos en binario. */
static bool ler_xref_stream(pdf_doc_t *d, uint32_t pos, uint32_t *prev)
{
    const uint8_t *b = d->buf.dados;
    uint32_t tam = d->buf.tam;
    pdf_lex_t l = { b, tam, pos };
    lex_pular(&l);
    double v = 0;
    bool ent = false;
    if (!lex_numero(&l, &v, &ent)) return false;
    if (!lex_numero(&l, &v, &ent)) return false;
    char palavra[16];
    if (!lex_palavra(&l, palavra, sizeof palavra) || !igual(palavra, "obj")) return false;

    pdf_obj_t *o = ler_objeto(d, &l, 0, 0, true);
    if (!o) return false;
    if (o->tipo != PDF_OBJ_STREAM) { obj_liberar(o); return false; }

    *prev = 0;
    pdf_obj_t *pr = pdf_dict_buscar_tipo(o, "Prev", PDF_OBJ_NUMERO);
    if (pr && pr->numero >= 0) *prev = (uint32_t)pr->numero;

    /* El /W: quantos bytes lleva cada campo de cada entrada. */
    pdf_obj_t *w = pdf_dict_buscar_tipo(o, "W", PDF_OBJ_ARREGLO);
    if (!w || w->n_itens < 3) { obj_liberar(o); return false; }
    int lw[3];
    for (int i = 0; i < 3; i++) {
        pdf_obj_t *e = w->itens[i];
        lw[i] = (e && e->tipo == PDF_OBJ_NUMERO) ? (int)e->numero : 0;
    }
    int tam_ent = lw[0] + lw[1] + lw[2];
    if (tam_ent <= 0) { obj_liberar(o); return false; }

    /* Los rangos: el /Index, o [0 /Size] si no esta. */
    int rangos = 0;
    int *ini = NULL, *con = NULL;
    pdf_obj_t *ix = pdf_dict_buscar_tipo(o, "Index", PDF_OBJ_ARREGLO);
    if (ix && ix->n_itens >= 2) {
        rangos = ix->n_itens / 2;
        ini = aloca(sizeof(int) * (size_t)rangos);
        con = aloca(sizeof(int) * (size_t)rangos);
        if (!ini || !con) { libera(ini); libera(con); obj_liberar(o); return false; }
        for (int i = 0; i < rangos; i++) {
            ini[i] = (int)ix->itens[2 * i]->numero;
            con[i] = (int)ix->itens[2 * i + 1]->numero;
        }
    } else {
        pdf_obj_t *sz = pdf_dict_buscar_tipo(o, "Size", PDF_OBJ_NUMERO);
        rangos = 1;
        ini = aloca(sizeof(int));
        con = aloca(sizeof(int));
        if (!ini || !con) { libera(ini); libera(con); obj_liberar(o); return false; }
        ini[0] = 0;
        con[0] = sz ? (int)sz->numero : 0;
    }

    /* Las entradas: tipo 1 = objeto suelto, tipo 2 = dentro de un stream. */
    const uint8_t *dados = o->datos;
    int tam_dados = o->tam_datos;
    int p = 0;
    for (int r = 0; r < rangos; r++) {
        for (int k = 0; k < con[r]; k++) {
            if (p + tam_ent > tam_dados) break;
            uint64_t tipo = 1, f2 = 0, f3 = 0;
            int q = p;
            if (lw[0] > 0) { tipo = 0; for (int i = 0; i < lw[0]; i++) tipo = (tipo << 8) | dados[q++]; }
            if (lw[1] > 0) { f2 = 0; for (int i = 0; i < lw[1]; i++) f2 = (f2 << 8) | dados[q++]; }
            if (lw[2] > 0) { f3 = 0; for (int i = 0; i < lw[2]; i++) f3 = (f3 << 8) | dados[q++]; }
            p = q;
            int num = ini[r] + k;
            if (tipo == 1) xref_poe(d, num, 1, (uint32_t)f2, (uint16_t)f3);
            else if (tipo == 2) xref_poe(d, num, 2, (uint32_t)f2, (uint16_t)f3);
        }
    }

    libera(ini);
    libera(con);
    /* El propio stream del xref hace de trailer. */
    if (!d->trailer) d->trailer = o;
    else obj_liberar(o);
    return true;
}

/* Lee la xref y va hacia atras por el /Prev (las actualizaciones). */
static bool ler_xref(pdf_doc_t *d, uint32_t pos, int prof)
{
    if (prof > 32) return false;
    if (pos >= d->buf.tam) return false;
    uint32_t prev = 0;
    bool ok = ler_xref_tabela(d, pos, &prev);
    if (!ok) ok = ler_xref_stream(d, pos, &prev);
    if (prev > 0) ler_xref(d, prev, prof + 1);
    return ok;
}

/* El trailer "trailer" de los PDF clasicos (si el xref no dio ninguno). */
static bool achar_trailer(pdf_doc_t *d)
{
    const uint8_t *b = d->buf.dados;
    uint32_t tam = d->buf.tam;
    int64_t ult = -1;
    for (int64_t i = (int64_t)tam - 7; i >= 0; i--) {
        if (memcmp(b + i, "trailer", 7) == 0) { ult = i; break; }
    }
    if (ult < 0) return false;
    pdf_lex_t l = { b, tam, (uint32_t)(ult + 7) };
    lex_pular(&l);
    pdf_obj_t *tr = ler_objeto(d, &l, 0, 0, true);
    if (!tr) return false;
    if (!d->trailer) d->trailer = tr;
    else obj_liberar(tr);
    return true;
}

/* Reconstruye la xref a ojo: busca las cabeceras "N G obj" por todo el
 * fichero. Es el plan B para los PDF con la xref rota o ausente. */
static bool reconstruir_xref(pdf_doc_t *d)
{
    const uint8_t *b = d->buf.dados;
    uint32_t tam = d->buf.tam;
    bool algum = false;
    uint32_t p = 0;
    d->escaneando = true;
    while (p + 3 < tam) {
        if (b[p] == 'o' && b[p + 1] == 'b' && b[p + 2] == 'j' &&
            (p + 3 >= tam || eh_delimitador(b[p + 3]))) {
            /* Hacia atras: el numero de generacion y el del objeto. */
            int64_t q = (int64_t)p - 1;
            while (q >= 0 && eh_whitespace(b[q])) q--;
            int64_t fim_gen = q;
            while (q >= 0 && b[q] >= '0' && b[q] <= '9') q--;
            if (q < fim_gen) {
                int64_t meio = q;
                while (meio >= 0 && eh_whitespace(b[meio])) meio--;
                int64_t fim_num = meio;
                while (meio >= 0 && b[meio] >= '0' && b[meio] <= '9') meio--;
                if (meio < fim_num) {
                    uint32_t num = 0;
                    for (int64_t k = meio + 1; k <= fim_num; k++)
                        num = num * 10 + (uint32_t)(b[k] - '0');
                    uint32_t inicio = (uint32_t)(meio + 1);
                    pdf_lex_t l = { b, tam, p + 3 };
                    lex_pular(&l);
                    pdf_obj_t *o = ler_objeto(d, &l, (int)num, 0, false);
                    if (o) {
                        obj_liberar(o);
                        /* La ultima version del objeto es la que vale. */
                        if (!xref_crescer(d, (int)num + 1)) break;
                        d->tipo_xref[(int)num] = 1;
                        d->pos_xref[(int)num] = inicio;
                        d->gen_xref[(int)num] = 0;
                        algum = true;
                        p = l.pos;
                        continue;
                    }
                }
            }
        }
        p++;
    }
    d->escaneando = false;
    return algum;
}

/* ------------------------------------------------------------ objetos */

/* forward */
static pdf_obj_t *objeto_do_stream(pdf_doc_t *d, int num);

/* El objeto numero 'num'. Se lee la primera vez y se guarda en la cache. */
/* Lee el objeto 'num' donde dice la xref (sin cache). */
static pdf_obj_t *ler_da_xref(pdf_doc_t *d, int num, bool *pos_errada)
{
    pdf_obj_t *o = NULL;
    *pos_errada = false;
    if (d->tipo_xref[num] == 1) {
        uint32_t pos = d->pos_xref[num];
        *pos_errada = true;
        if (pos < d->buf.tam) {
            pdf_lex_t l = { d->buf.dados, d->buf.tam, pos };
            lex_pular(&l);
            double v1 = 0, v2 = 0;
            bool e1 = false, e2 = false;
            if (lex_numero(&l, &v1, &e1) && lex_numero(&l, &v2, &e2) && (int)v1 == num) {
                char palavra[8];
                if (lex_palavra(&l, palavra, sizeof palavra) && igual(palavra, "obj")) {
                    *pos_errada = false;
                    o = ler_objeto(d, &l, num, d->gen_xref[num], false);
                }
            }
        }
    } else if (d->tipo_xref[num] == 2) {
        o = objeto_do_stream(d, num);
    }
    return o;
}

static bool reconstruir_xref(pdf_doc_t *d);
static void expandir_objstm(pdf_doc_t *d);

pdf_obj_t *pdf_doc_objeto(pdf_doc_t *d, int num)
{
    if (!d || num < 0 || num >= d->n_objetos) return NULL;
    if (d->cache_ok[num]) return d->cache[num];
    if (d->escaneando) return NULL;
    /* Contra los object streams que se citan en circulo. */
    if (d->prof_objeto > 16) return NULL;
    d->prof_objeto++;
    bool errada = false;
    pdf_obj_t *o = ler_da_xref(d, num, &errada);
    if (!o && errada && !d->reconstruida) {
        /* La xref apunta a otro sitio (un PDF editado a mano, cortado o con
         * basura delante): se reconstruye buscando "N G obj" una vez. */
        d->reconstruida = true;
        reconstruir_xref(d);
        expandir_objstm(d);
        if (num < d->n_objetos) o = ler_da_xref(d, num, &errada);
    }
    d->prof_objeto--;
    if (o) {
        d->cache[num] = o;
        d->cache_ok[num] = 1;
    }
    return o;
}

/* Un objeto que vive dentro de un object stream (los PDF modernos). */
static pdf_obj_t *objeto_do_stream(pdf_doc_t *d, int num)
{
    int stream = (int)d->pos_xref[num];
    uint32_t offset = d->off_objstm[num];
    if (offset == 0xFFFFFFFFu || stream == num) return NULL;
    pdf_obj_t *st = pdf_doc_objeto(d, stream);
    if (!st || st->tipo != PDF_OBJ_STREAM) return NULL;
    if (offset >= (uint32_t)st->tam_datos) return NULL;
    pdf_lex_t l = { st->datos, (uint32_t)st->tam_datos, offset };
    lex_pular(&l);
    /* Lo que hay dentro ya viene descifrado: no hay que volver a hacerlo. */
    return ler_objeto(d, &l, num, 0, true);
}

/* Lee los object streams y apunta que objetos hay dentro de cada uno. */
static void expandir_objstm(pdf_doc_t *d)
{
    for (int num = 0; num < d->n_objetos; num++) {
        if (d->tipo_xref[num] != 1) continue;
        uint32_t pos = d->pos_xref[num];
        if (pos + 12 > d->buf.tam) continue;
        /* El diccionario dice /Type /ObjStm? */
        bool e_objstm = false;
        uint32_t fim = pos + 1024;
        if (fim > d->buf.tam) fim = d->buf.tam;
        for (uint32_t i = pos; i + 7 <= fim; i++)
            if (memcmp(d->buf.dados + i, "/ObjStm", 7) == 0) { e_objstm = true; break; }
        if (!e_objstm) continue;

        pdf_obj_t *o = pdf_doc_objeto(d, num);
        if (!o || o->tipo != PDF_OBJ_STREAM) continue;
        pdf_obj_t *pn = pdf_dict_buscar_tipo(o, "N", PDF_OBJ_NUMERO);
        pdf_obj_t *pf = pdf_dict_buscar_tipo(o, "First", PDF_OBJ_NUMERO);
        if (!pn || !pf) continue;
        int n = (int)pn->numero;
        int first = (int)pf->numero;
        if (n <= 0 || n > 1000000 || first < 0 || first > o->tam_datos) continue;

        pdf_lex_t l = { o->datos, (uint32_t)o->tam_datos, 0 };
        for (int i = 0; i < n; i++) {
            double v1 = 0, v2 = 0;
            bool e1 = false, e2 = false;
            if (!lex_numero(&l, &v1, &e1)) break;
            if (!lex_numero(&l, &v2, &e2)) break;
            int onum = (int)v1;
            int off = first + (int)v2;
            if (onum < 0 || onum > 10000000 || off < 0) continue;
            if (!xref_crescer(d, onum + 1)) break;
            /* Solo si la xref dice que el objeto vive en ESTE stream (o no
             * dice nada): una actualizacion posterior puede haberlo
             * reescrito fuera, y entonces la version del stream es vieja. */
            if (d->tipo_xref[onum] == 0) {
                d->tipo_xref[onum] = 2;
                d->pos_xref[onum] = (uint32_t)num;
                d->gen_xref[onum] = 0;
            }
            if (d->tipo_xref[onum] == 2 && d->pos_xref[onum] == (uint32_t)num)
                d->off_objstm[onum] = (uint32_t)off;
        }
    }
}

/* ------------------------------------------------------------ cifrado */

/* Prepara el descifrado si el documento esta cifrado. */
static void preparar_cripto(pdf_doc_t *d, const char *contrasena)
{
    if (!d->trailer) return;
    pdf_obj_t *enc = pdf_dict_buscar(d->trailer, "Encrypt");
    if (!enc) return;
    if (enc->tipo == PDF_OBJ_REFERENCIA) {
        enc = pdf_doc_objeto(d, enc->num);
        if (!enc) return;
    }
    if (enc->tipo != PDF_OBJ_DICCIONARIO) return;
    pdf_obj_t *filtro = pdf_dict_buscar_tipo(enc, "Filter", PDF_OBJ_NOME);
    if (filtro && !igual(filtro->nome, "Standard")) return;   /* cifrado raro: no se sabe */

    pdf_obj_t *o = pdf_dict_buscar(enc, "O");
    pdf_obj_t *u = pdf_dict_buscar(enc, "U");
    if (o && o->tipo == PDF_OBJ_REFERENCIA) o = pdf_doc_objeto(d, o->num);
    if (u && u->tipo == PDF_OBJ_REFERENCIA) u = pdf_doc_objeto(d, u->num);
    pdf_obj_t *p = pdf_dict_buscar_tipo(enc, "P", PDF_OBJ_NUMERO);
    pdf_obj_t *v = pdf_dict_buscar_tipo(enc, "V", PDF_OBJ_NUMERO);
    pdf_obj_t *r = pdf_dict_buscar_tipo(enc, "R", PDF_OBJ_NUMERO);
    pdf_obj_t *comp = pdf_dict_buscar_tipo(enc, "Length", PDF_OBJ_NUMERO);
    pdf_obj_t *em = pdf_dict_buscar_tipo(enc, "EncryptMetadata", PDF_OBJ_BOOL);

    /* El identificador del fichero: la primera cadena del /ID del trailer. */
    const uint8_t *id = NULL;
    int n_id = 0;
    pdf_obj_t *ids = pdf_dict_buscar(d->trailer, "ID");
    if (ids && ids->tipo == PDF_OBJ_ARREGLO && ids->n_itens > 0) {
        pdf_obj_t *s = ids->itens[0];
        if (s && s->tipo == PDF_OBJ_CADENA) { id = (const uint8_t *)s->cadena; n_id = s->tam_cadena; }
    }

    bool meta = em ? em->booleano : true;
    const uint8_t *o_d = (o && o->tipo == PDF_OBJ_CADENA) ? (const uint8_t *)o->cadena : NULL;
    int n_o = (o && o->tipo == PDF_OBJ_CADENA) ? o->tam_cadena : 0;
    const uint8_t *u_d = (u && u->tipo == PDF_OBJ_CADENA) ? (const uint8_t *)u->cadena : NULL;
    int n_u = (u && u->tipo == PDF_OBJ_CADENA) ? u->tam_cadena : 0;

    int res;
    if (v && (int)v->numero == 5) {
        /* V=5 (R5 y R6, AES-256): la clave de archivo va cifrada en el /OE y
         * en el /UE, asi que hacen falta esos dos. */
        pdf_obj_t *oe = pdf_dict_buscar(enc, "OE");
        pdf_obj_t *ue = pdf_dict_buscar(enc, "UE");
        if (oe && oe->tipo == PDF_OBJ_REFERENCIA) oe = pdf_doc_objeto(d, oe->num);
        if (ue && ue->tipo == PDF_OBJ_REFERENCIA) ue = pdf_doc_objeto(d, ue->num);
        const uint8_t *oe_d = (oe && oe->tipo == PDF_OBJ_CADENA) ? (const uint8_t *)oe->cadena : NULL;
        int n_oe = (oe && oe->tipo == PDF_OBJ_CADENA) ? oe->tam_cadena : 0;
        const uint8_t *ue_d = (ue && ue->tipo == PDF_OBJ_CADENA) ? (const uint8_t *)ue->cadena : NULL;
        int n_ue = (ue && ue->tipo == PDF_OBJ_CADENA) ? ue->tam_cadena : 0;
        res = pdf_cripto_preparar_v5(&d->cripto, contrasena ? contrasena : "",
                                     o_d, n_o, u_d, n_u, oe_d, n_oe, ue_d, n_ue,
                                     p ? (int)p->numero : -1,
                                     v ? (int)v->numero : 0,
                                     r ? (int)r->numero : 0, meta);
    } else {
        res = pdf_cripto_preparar(&d->cripto, contrasena ? contrasena : "",
                                   o_d, n_o, u_d, n_u,
                                   p ? (int)p->numero : -1,
                                   v ? (int)v->numero : 0,
                                   r ? (int)r->numero : 0,
                                   comp ? (int)comp->numero : 128,
                                   id, n_id, meta);
    }
    if (res == PDF_CRIPTO_OK) {
        d->tem_cripto = true;
        d->cripto.cifrar_meta = meta;
    } else {
        d->tem_cripto = false;
        d->precisa_contrasena = true;
    }
}

/* ------------------------------------------------------------ paginas */

/* Anade una pagina a la lista. */
static bool paginas_add(pdf_doc_t *d, const pdf_pagina_info_t *p)
{
    if (hay_que_crecer(d->n_paginas)) {
        pdf_pagina_info_t *v = realoca(d->paginas, capacidade(d->n_paginas) * sizeof(pdf_pagina_info_t));
        if (!v) return false;
        d->paginas = v;
    }
    d->paginas[d->n_paginas++] = *p;
    return true;
}

/* Los cuatro numeros del MediaBox (pueden ser referencias). */
static void ler_caixa(pdf_doc_t *d, pdf_obj_t *mb, double *box)
{
    for (int i = 0; i < 4 && i < mb->n_itens; i++) {
        pdf_obj_t *v = mb->itens[i];
        if (v && v->tipo == PDF_OBJ_REFERENCIA) v = pdf_doc_objeto(d, v->num);
        if (v && v->tipo == PDF_OBJ_NUMERO) box[i] = v->numero;
    }
}

/* Recorre el arbol de paginas, heredando el MediaBox y el Rotate. */
static void arvore_paginas(pdf_doc_t *d, pdf_obj_t *no, int prof, double *box, bool tem_box, int rotate)
{
    if (!no || prof > 64) return;
    if (no->tipo == PDF_OBJ_REFERENCIA) {
        no = pdf_doc_objeto(d, no->num);
        if (!no) return;
    }
    if (no->tipo != PDF_OBJ_DICCIONARIO && no->tipo != PDF_OBJ_STREAM) return;

    pdf_obj_t *mb = pdf_dict_buscar(no, "MediaBox");
    if (mb && mb->tipo == PDF_OBJ_ARREGLO && mb->n_itens >= 4) {
        ler_caixa(d, mb, box);
        tem_box = true;
    }
    pdf_obj_t *ro = pdf_dict_buscar_tipo(no, "Rotate", PDF_OBJ_NUMERO);
    if (ro) {
        int r = (int)ro->numero % 360;
        if (r < 0) r += 360;
        rotate = r;
    }

    pdf_obj_t *kids = pdf_dict_buscar(no, "Kids");
    if (kids && kids->tipo == PDF_OBJ_ARREGLO) {
        for (int i = 0; i < kids->n_itens; i++)
            arvore_paginas(d, kids->itens[i], prof + 1, box, tem_box, rotate);
        return;
    }

    pdf_obj_t *tipo = pdf_dict_buscar_tipo(no, "Type", PDF_OBJ_NOME);
    if (tipo && !igual(tipo->nome, "Page")) return;   /* un nodo sin hijos que no es pagina */

    pdf_pagina_info_t info;
    info.obj = no;
    for (int i = 0; i < 4; i++) info.box[i] = box[i];
    info.tem_box = tem_box;
    info.rotate = rotate;
    if (!tem_box) { info.box[0] = 0; info.box[1] = 0; info.box[2] = 612; info.box[3] = 792; }
    paginas_add(d, &info);
}

/* El catalogo: el /Root del trailer, o el objeto que sea /Type /Catalog. */
static void achar_catalogo(pdf_doc_t *d)
{
    pdf_obj_t *r = d->trailer ? pdf_dict_buscar(d->trailer, "Root") : NULL;
    if (r) {
        if (r->tipo == PDF_OBJ_REFERENCIA) {
            pdf_obj_t *c = pdf_doc_objeto(d, r->num);
            if (c) { d->catalogo = c; return; }
        } else if (r->tipo == PDF_OBJ_DICCIONARIO) {
            d->catalogo = r;
            return;
        }
    }
    for (int num = 0; num < d->n_objetos; num++) {
        pdf_obj_t *o = pdf_doc_objeto(d, num);
        if (!o) continue;
        pdf_obj_t *t = pdf_dict_buscar_tipo(o, "Type", PDF_OBJ_NOME);
        if (t && igual(t->nome, "Catalog")) { d->catalogo = o; return; }
    }
}

/* Un texto de PDF a UTF-8 (los metadatos vienen en UTF-16 con BOM). */
static char *texto_utf8(const uint8_t *s, int n)
{
    if (!s || n <= 0) return NULL;
    char *o = aloca((size_t)n * 4 + 8);
    if (!o) return NULL;
    int k = 0, i = 0;
    int endian = 0;   /* 0 = un byte, 1 = UTF-16BE, 2 = UTF-16LE */
    if (n >= 2 && s[0] == 0xFE && s[1] == 0xFF) { endian = 1; i = 2; }
    else if (n >= 2 && s[0] == 0xFF && s[1] == 0xFE) { endian = 2; i = 2; }
    while (i < n) {
        uint32_t c;
        if (endian == 0) c = s[i++];
        else {
            if (i + 1 >= n) break;
            if (endian == 1) c = ((uint32_t)s[i] << 8) | s[i + 1];
            else c = ((uint32_t)s[i + 1] << 8) | s[i];
            i += 2;
        }
        if (c >= 0xD800 && c <= 0xDBFF && endian && i + 1 < n) {
            uint32_t c2 = (endian == 1) ? (((uint32_t)s[i] << 8) | s[i + 1])
                                       : (((uint32_t)s[i + 1] << 8) | s[i]);
            if (c2 >= 0xDC00 && c2 <= 0xDFFF) {
                c = 0x10000 + ((c - 0xD800) << 10) + (c2 - 0xDC00);
                i += 2;
            }
        }
        if (c < 0x80) o[k++] = (char)c;
        else if (c < 0x800) {
            o[k++] = (char)(0xC0 | (c >> 6));
            o[k++] = (char)(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            o[k++] = (char)(0xE0 | (c >> 12));
            o[k++] = (char)(0x80 | ((c >> 6) & 0x3F));
            o[k++] = (char)(0x80 | (c & 0x3F));
        } else {
            o[k++] = (char)(0xF0 | (c >> 18));
            o[k++] = (char)(0x80 | ((c >> 12) & 0x3F));
            o[k++] = (char)(0x80 | ((c >> 6) & 0x3F));
            o[k++] = (char)(0x80 | (c & 0x3F));
        }
    }
    o[k] = 0;
    return o;
}

/* Un metadato del /Info, ya en UTF-8. */
static char *meta_texto(pdf_doc_t *d, const char *chave)
{
    if (!d->info) return NULL;
    pdf_obj_t *o = pdf_dict_buscar(d->info, chave);
    if (!o) return NULL;
    if (o->tipo == PDF_OBJ_REFERENCIA) {
        o = pdf_doc_objeto(d, o->num);
        if (!o) return NULL;
    }
    if (o->tipo == PDF_OBJ_CADENA) return texto_utf8((const uint8_t *)o->cadena, o->tam_cadena);
    if (o->tipo == PDF_OBJ_NOME) return texto_utf8((const uint8_t *)o->nome, (int)tam_texto(o->nome));
    return NULL;
}

/* ------------------------------------------------------------ abrir y cerrar */

/* Abre el documento. */
static pdf_doc_t *abrir_doc(const char *caminho, const char *contrasena, int *erro)
{
    pdf_doc_t *d = aloca_zerado(sizeof(pdf_doc_t));
    if (!d) { if (erro) *erro = PDF_ERRO_MEMORIA; return NULL; }
    d->fd = -1;

    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) { libera(d); if (erro) *erro = ERRO_NAO_EXISTE; return NULL; }
    info_arquivo_t info;
    /* O info_arquivo devolve 0 quando tudo corre bem (como os syscalls). */
    if (info_arquivo(caminho, &info) != 0 || info.tamanho == 0) {
        fechar(fd); libera(d);
        if (erro) *erro = PDF_ERRO_FORMATO;
        return NULL;
    }
    uint32_t tam = info.tamanho;
    d->buf.dados = aloca((size_t)tam + 1);
    if (!d->buf.dados) { fechar(fd); libera(d); if (erro) *erro = PDF_ERRO_MEMORIA; return NULL; }
    uint32_t lidos = 0;
    while (lidos < tam) {
        int n = ler(fd, d->buf.dados + lidos, (unsigned)(tam - lidos));
        if (n <= 0) break;
        lidos += (uint32_t)n;
    }
    fechar(fd);
    d->buf.tam = lidos;
    d->buf.dados[lidos] = 0;

    /* Algunos PDF llevan algo delante del %PDF- (una cabecera de correo, un
     * BOM...); los desplazamientos de la xref cuentan desde el %PDF-, asi
     * que se quita lo de delante. */
    if (lidos >= 8 && memcmp(d->buf.dados, "%PDF-", 5) != 0) {
        uint32_t lim = lidos < 1024 ? lidos - 5 : 1019;
        for (uint32_t k = 1; k < lim; k++) {
            if (memcmp(d->buf.dados + k, "%PDF-", 5) == 0) {
                memmove(d->buf.dados, d->buf.dados + k, lidos - k);
                lidos -= k;
                d->buf.tam = lidos;
                d->buf.dados[lidos] = 0;
                break;
            }
        }
    }
    if (lidos < 8 || memcmp(d->buf.dados, "%PDF-", 5) != 0) {
        libera(d->buf.dados);
        libera(d);
        if (erro) *erro = PDF_ERRO_FORMATO;
        return NULL;
    }

    /* El indice de referencias y el trailer. */
    uint32_t desl = 0;
    if (achar_startxref(d, &desl)) ler_xref(d, desl, 0);
    if (!d->trailer) achar_trailer(d);
    if (d->n_objetos == 0 && reconstruir_xref(d)) achar_trailer(d);

    /* El cifrado (antes de leer los objetos: las cadenas y los streams van
     * cifrados, pero el /Encrypt y el trailer no). */
    preparar_cripto(d, contrasena);

    /* Los objetos que viven dentro de un object stream. */
    expandir_objstm(d);

    /* El catalogo y las paginas. */
    achar_catalogo(d);
    if (d->catalogo) {
        pdf_obj_t *p = pdf_dict_buscar(d->catalogo, "Pages");
        if (p) {
            double box[4] = { 0, 0, 612, 792 };
            arvore_paginas(d, p, 0, box, false, 0);
        }
    }
    if (d->n_paginas == 0 && !d->reconstruida) {
        /* Sin paginas: la xref (o el trailer) no sirve. Plan B: buscar los
         * objetos a ojo y el catalogo por su /Type. */
        d->reconstruida = true;
        if (reconstruir_xref(d)) {
            expandir_objstm(d);
            if (!d->trailer) achar_trailer(d);
            d->catalogo = NULL;
            achar_catalogo(d);
            if (d->catalogo) {
                pdf_obj_t *p = pdf_dict_buscar(d->catalogo, "Pages");
                if (p) {
                    double box[4] = { 0, 0, 612, 792 };
                    arvore_paginas(d, p, 0, box, false, 0);
                }
            }
        }
    }

    /* Los metadatos. */
    pdf_obj_t *inf = d->trailer ? pdf_dict_buscar(d->trailer, "Info") : NULL;
    if (inf) {
        if (inf->tipo == PDF_OBJ_REFERENCIA) d->info = pdf_doc_objeto(d, inf->num);
        else if (inf->tipo == PDF_OBJ_DICCIONARIO) d->info = inf;
    }
    d->titulo = meta_texto(d, "Title");
    d->autor = meta_texto(d, "Author");
    d->produtor = meta_texto(d, "Producer");

    d->aberto = true;
    if (erro) *erro = 0;
    return d;
}

pdf_doc_t *pdf_doc_abrir(const char *caminho, int *erro)
{
    return abrir_doc(caminho, "", erro);
}

pdf_doc_t *pdf_doc_abrir_com_contrasena(const char *caminho, const char *contrasena, int *erro)
{
    return abrir_doc(caminho, contrasena ? contrasena : "", erro);
}

void pdf_doc_fechar(pdf_doc_t *d)
{
    if (!d) return;
    if (d->fd >= 0) fechar(d->fd);
    for (int i = 0; i < d->n_objetos; i++)
        if (d->cache_ok[i]) obj_liberar(d->cache[i]);
    libera(d->cache);
    libera(d->cache_ok);
    libera(d->tipo_xref);
    libera(d->pos_xref);
    libera(d->gen_xref);
    libera(d->idx_xref);
    libera(d->off_objstm);
    libera(d->objstm);
    libera(d->paginas);
    libera(d->titulo);
    libera(d->autor);
    libera(d->produtor);
    if (d->trailer) obj_liberar(d->trailer);
    libera(d->buf.dados);
    libera(d);
}

int pdf_doc_num_objetos(const pdf_doc_t *d) { return d ? d->n_objetos : 0; }

pdf_obj_t *pdf_doc_catalogo(pdf_doc_t *d) { return d ? d->catalogo : NULL; }

pdf_obj_t *pdf_doc_info(pdf_doc_t *d) { return d ? d->info : NULL; }

const char *pdf_doc_titulo(const pdf_doc_t *d) { return (d && d->titulo) ? d->titulo : ""; }
const char *pdf_doc_autor(const pdf_doc_t *d) { return (d && d->autor) ? d->autor : ""; }
const char *pdf_doc_produtor(const pdf_doc_t *d) { return (d && d->produtor) ? d->produtor : ""; }

int pdf_doc_paginas(const pdf_doc_t *d) { return d ? d->n_paginas : 0; }

pdf_obj_t *pdf_doc_pagina(pdf_doc_t *d, int i)
{
    if (!d || i < 0 || i >= d->n_paginas) return NULL;
    return d->paginas[i].obj;
}

const char *pdf_doc_texto(pdf_doc_t *d, pdf_obj_t *obj)
{
    if (!obj) return "";
    if (obj->tipo == PDF_OBJ_REFERENCIA) {
        obj = pdf_doc_objeto(d, obj->num);
        if (!obj) return "";
    }
    if (obj->tipo == PDF_OBJ_CADENA) return obj->cadena ? obj->cadena : "";
    if (obj->tipo == PDF_OBJ_NOME) return obj->nome ? obj->nome : "";
    return "";
}

int pdf_doc_pagina_largura(pdf_doc_t *d, int i)
{
    if (!d || i < 0 || i >= d->n_paginas) return 0;
    pdf_pagina_info_t *p = &d->paginas[i];
    double w = p->box[2] - p->box[0];
    double h = p->box[3] - p->box[1];
    if (p->rotate == 90 || p->rotate == 270) { double t = w; w = h; h = t; }
    if (w < 0) w = -w;
    return (int)(w + 0.5);
}

int pdf_doc_pagina_altura(pdf_doc_t *d, int i)
{
    if (!d || i < 0 || i >= d->n_paginas) return 0;
    pdf_pagina_info_t *p = &d->paginas[i];
    double w = p->box[2] - p->box[0];
    double h = p->box[3] - p->box[1];
    if (p->rotate == 90 || p->rotate == 270) { double t = w; w = h; h = t; }
    if (h < 0) h = -h;
    return (int)(h + 0.5);
}

bool pdf_doc_contrasena_necesaria(pdf_doc_t *d) { return d ? d->precisa_contrasena : false; }
