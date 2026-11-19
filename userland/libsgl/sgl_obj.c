// SPDX-License-Identifier: GPL-2.0-or-later
/* sgl_obj.c - cargador de modelos OBJ (+ MTL) para SaguiGL (D404). Ver sgl_obj.h.
 *
 * Dos pasadas sobre el texto: la primera cuenta posiciones, coordenadas,
 * normales y triángulos; la segunda los copia ya desplegados (tres vértices
 * por triángulo, sin índices), agrupados por material en el orden del
 * fichero. Los números se leen con un analizador propio (sin libc): el
 * resultado es el mismo en el anfitrión y en el SaguiOS. */
#include "sgl_obj.h"

#ifdef SGL_ANFITRIAO
#include <stdio.h>
#include <stdlib.h>
#define OBJ_ALOCAR(n)  calloc(1, (n))
#define OBJ_LIBERAR(p) free(p)
#else
#include "sagui.h"
#include "imagem.h"
#define OBJ_ALOCAR(n)  aloca_zerado(n)
#define OBJ_LIBERAR(p) libera(p)
#endif

/* ------------------------------------------------------------ texto */

typedef struct { const char *p, *fim; } leitor_t;

static int fim_linha(const leitor_t *l) { return l->p >= l->fim || *l->p == '\n' || *l->p == '\r'; }
static void espacos(leitor_t *l) { while (l->p < l->fim && (*l->p == ' ' || *l->p == '\t')) l->p++; }
static void prox_linha(leitor_t *l)
{
    while (l->p < l->fim && *l->p != '\n') l->p++;
    if (l->p < l->fim) l->p++;
}

/* ¿La palabra de ahora es `w` (seguida de espacio o fin de línea)? Si sí, la salta. */
static int palavra(leitor_t *l, const char *w)
{
    const char *q = l->p;
    while (*w) { if (q >= l->fim || *q != *w) return 0; q++; w++; }
    if (q < l->fim && *q != ' ' && *q != '\t' && *q != '\n' && *q != '\r') return 0;
    l->p = q;
    return 1;
}

static float numero(leitor_t *l)
{
    espacos(l);
    int neg = 0;
    if (l->p < l->fim && (*l->p == '-' || *l->p == '+')) { neg = *l->p == '-'; l->p++; }
    double v = 0, esc = 1;
    while (l->p < l->fim && *l->p >= '0' && *l->p <= '9') v = v * 10 + (*l->p++ - '0');
    if (l->p < l->fim && *l->p == '.') {
        l->p++;
        while (l->p < l->fim && *l->p >= '0' && *l->p <= '9') { esc *= 0.1; v += (*l->p++ - '0') * esc; }
    }
    if (l->p < l->fim && (*l->p == 'e' || *l->p == 'E')) {
        l->p++;
        int en = 0, e = 0;
        if (l->p < l->fim && (*l->p == '-' || *l->p == '+')) { en = *l->p == '-'; l->p++; }
        while (l->p < l->fim && *l->p >= '0' && *l->p <= '9') { if (e < 300) e = e * 10 + (*l->p - '0'); l->p++; }
        while (e-- > 0) v = en ? v * 0.1 : v * 10;
    }
    return (float)(neg ? -v : v);
}

static int inteiro(leitor_t *l, int *ok)
{
    int neg = 0, v = 0, dig = 0;
    if (l->p < l->fim && *l->p == '-') { neg = 1; l->p++; }
    while (l->p < l->fim && *l->p >= '0' && *l->p <= '9') { v = v * 10 + (*l->p++ - '0'); dig = 1; }
    *ok = dig;
    return neg ? -v : v;
}

/* Copia la palabra siguiente (hasta el fin de línea, sin espacios al final). */
static void resto(leitor_t *l, char *d, int max)
{
    espacos(l);
    int n = 0;
    while (!fim_linha(l)) { if (n < max - 1) d[n++] = *l->p; l->p++; }
    while (n > 0 && (d[n - 1] == ' ' || d[n - 1] == '\t')) n--;
    d[n] = 0;
}

static int igual(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void copiar(char *d, const char *o, int max)
{
    int i = 0;
    while (o[i] && i < max - 1) { d[i] = o[i]; i++; }
    d[i] = 0;
}

/* ------------------------------------------------------------ materiales */

static int material(sgl_obj_t *m, const char *nome, int criar)
{
    for (int i = 0; i < m->n_mats; i++) if (igual(m->mat[i].nome, nome)) return i;
    if (!criar || m->n_mats >= SGL_OBJ_MAX_MAT) return -1;
    sgl_obj_mat_t *t = &m->mat[m->n_mats];
    copiar(t->nome, nome, sizeof t->nome);
    /* Sin MTL: un gris claro con un matiz sacado del nombre, para que las piezas se distingan. */
    unsigned h = 2166136261u;
    for (const char *c = nome; *c; c++) { h ^= (unsigned char)*c; h *= 16777619u; }
    t->kd[0] = 0.55f + (float)(h & 63) / 200.0f;
    t->kd[1] = 0.55f + (float)((h >> 6) & 63) / 200.0f;
    t->kd[2] = 0.55f + (float)((h >> 12) & 63) / 200.0f;
    t->ks[0] = t->ks[1] = t->ks[2] = 0.2f;
    t->ns = 16; t->d = 1;
    t->mapa[0] = 0; t->tex = 0;
    return m->n_mats++;
}

int sgl_obj_ler_mtl(const char *texto, int n, sgl_obj_t *m)
{
    leitor_t l = { texto, texto + n };
    int atual = -1;
    while (l.p < l.fim) {
        espacos(&l);
        char nome[96];
        if (palavra(&l, "newmtl")) { resto(&l, nome, 32); atual = material(m, nome, 1); }
        else if (atual >= 0 && palavra(&l, "Kd")) for (int k = 0; k < 3; k++) m->mat[atual].kd[k] = numero(&l);
        else if (atual >= 0 && palavra(&l, "Ks")) for (int k = 0; k < 3; k++) m->mat[atual].ks[k] = numero(&l);
        else if (atual >= 0 && palavra(&l, "Ns")) {
            /* El Ns de los MTL va de 0 a 1000; el brillo de GL, de 0 a 128. */
            float v = numero(&l) * 0.128f;
            m->mat[atual].ns = v < 0 ? 0 : v > 128 ? 128 : v;
        } else if (atual >= 0 && palavra(&l, "d")) m->mat[atual].d = numero(&l);
        else if (atual >= 0 && palavra(&l, "map_Kd")) { resto(&l, nome, 96); copiar(m->mat[atual].mapa, nome, 96); }
        prox_linha(&l);
    }
    return 0;
}

/* ------------------------------------------------------------ OBJ */

/* Un vértice de cara: índices ya resueltos (0..n-1, -1 si falta). */
typedef struct { int v, t, n; } ivert_t;

static int ler_vertice_face(leitor_t *l, int nv, int nt, int nn, ivert_t *o)
{
    int ok;
    o->t = o->n = -1;
    int v = inteiro(l, &ok);
    if (!ok) return 0;
    o->v = v < 0 ? nv + v : v - 1;
    if (l->p < l->fim && *l->p == '/') {
        l->p++;
        if (l->p < l->fim && *l->p != '/') { int t = inteiro(l, &ok); if (ok) o->t = t < 0 ? nt + t : t - 1; }
        if (l->p < l->fim && *l->p == '/') {
            l->p++;
            int nn_ = inteiro(l, &ok);
            if (ok) o->n = nn_ < 0 ? nn + nn_ : nn_ - 1;
        }
    }
    return 1;
}

int sgl_obj_ler(const char *texto, int n, sgl_obj_t *m)
{
    leitor_t l;
    int nv = 0, nt = 0, nn = 0, ntri = 0;
    /* Primera pasada: contar. */
    for (l.p = texto, l.fim = texto + n; l.p < l.fim; prox_linha(&l)) {
        espacos(&l);
        if (palavra(&l, "v")) nv++;
        else if (palavra(&l, "vt")) nt++;
        else if (palavra(&l, "vn")) nn++;
        else if (palavra(&l, "f")) {
            int k = 0;
            for (;;) {
                espacos(&l);
                if (fim_linha(&l)) break;
                while (!fim_linha(&l) && *l.p != ' ' && *l.p != '\t') l.p++;
                k++;
            }
            if (k >= 3) ntri += k - 2;
        }
    }
    if (!ntri || !nv) return -2;
    float *P = OBJ_ALOCAR((size_t)nv * 12), *T = OBJ_ALOCAR((size_t)(nt ? nt : 1) * 8), *N = OBJ_ALOCAR((size_t)(nn ? nn : 1) * 12);
    m->n_vert = ntri * 3;
    m->pos = OBJ_ALOCAR((size_t)m->n_vert * 12);
    m->nor = OBJ_ALOCAR((size_t)m->n_vert * 12);
    m->uv = OBJ_ALOCAR((size_t)m->n_vert * 8);
    if (!P || !T || !N || !m->pos || !m->nor || !m->uv) {
        if (P) OBJ_LIBERAR(P);
        if (T) OBJ_LIBERAR(T);
        if (N) OBJ_LIBERAR(N);
        sgl_obj_liberar(m);
        return -1;
    }
    m->tem_uv = nt > 0;
    int iv = 0, it = 0, in = 0, out = 0, mat = -1, erro = 0;
    m->n_grupos = 0;
    for (l.p = texto, l.fim = texto + n; l.p < l.fim; prox_linha(&l)) {
        espacos(&l);
        char nome[96];
        if (palavra(&l, "v")) { for (int k = 0; k < 3; k++) P[iv * 3 + k] = numero(&l); iv++; }
        else if (palavra(&l, "vt")) { T[it * 2] = numero(&l); T[it * 2 + 1] = numero(&l); it++; }
        else if (palavra(&l, "vn")) { for (int k = 0; k < 3; k++) N[in * 3 + k] = numero(&l); in++; }
        else if (palavra(&l, "mtllib")) { resto(&l, nome, 96); copiar(m->mtllib, nome, 96); }
        else if (palavra(&l, "usemtl")) { resto(&l, nome, 32); mat = material(m, nome, 1); }
        else if (palavra(&l, "f")) {
            ivert_t pol[64];
            int k = 0;
            for (;;) {
                espacos(&l);
                if (fim_linha(&l)) break;
                ivert_t iv_;
                if (!ler_vertice_face(&l, iv, it, in, &iv_)) { while (!fim_linha(&l) && *l.p != ' ') l.p++; continue; }
                if (iv_.v < 0 || iv_.v >= iv || iv_.t < -1 || iv_.t >= it || iv_.n < -1 || iv_.n >= in) { erro = 1; continue; }
                if (k < 64) pol[k++] = iv_;
            }
            /* El grupo del material de ahora (nuevo si cambió). */
            if (k >= 3 && (m->n_grupos == 0 || m->grupo[m->n_grupos - 1].mat != mat)) {
                if (m->n_grupos < SGL_OBJ_MAX_MAT * 2) {
                    m->grupo[m->n_grupos].mat = mat; m->grupo[m->n_grupos].ini = out; m->grupo[m->n_grupos].n = 0;
                    m->n_grupos++;
                }
            }
            for (int i = 1; i + 1 < k && out + 3 <= m->n_vert; i++) {
                ivert_t tri[3];
                tri[0] = pol[0]; tri[1] = pol[i]; tri[2] = pol[i + 1];
                /* normal de la cara, para los vértices sin la suya */
                const float *a = &P[tri[0].v * 3], *b = &P[tri[1].v * 3], *c = &P[tri[2].v * 3];
                float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
                float vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
                float fx = uy * vz - uz * vy, fy = uz * vx - ux * vz, fz = ux * vy - uy * vx;
                float fl = (float)__builtin_sqrt((double)(fx * fx + fy * fy + fz * fz));
                if (fl > 0) { fx /= fl; fy /= fl; fz /= fl; }
                for (int j = 0; j < 3; j++, out++) {
                    for (int q = 0; q < 3; q++) m->pos[out * 3 + q] = P[tri[j].v * 3 + q];
                    if (tri[j].n >= 0) for (int q = 0; q < 3; q++) m->nor[out * 3 + q] = N[tri[j].n * 3 + q];
                    else { m->nor[out * 3] = fx; m->nor[out * 3 + 1] = fy; m->nor[out * 3 + 2] = fz; }
                    if (tri[j].t >= 0) {
                        /* El OBJ tiene el origen de v abajo; las texturas de SaguiGL, arriba. */
                        m->uv[out * 2] = T[tri[j].t * 2];
                        m->uv[out * 2 + 1] = 1.0f - T[tri[j].t * 2 + 1];
                    }
                }
                if (m->n_grupos) m->grupo[m->n_grupos - 1].n += 3;
            }
        }
    }
    OBJ_LIBERAR(P); OBJ_LIBERAR(T); OBJ_LIBERAR(N);
    m->n_vert = out;
    if (erro && !out) { sgl_obj_liberar(m); return -3; }
    if (!out) { sgl_obj_liberar(m); return -2; }
    for (int k = 0; k < 3; k++) { m->min[k] = m->pos[k]; m->max[k] = m->pos[k]; }
    for (int i = 0; i < out; i++)
        for (int k = 0; k < 3; k++) {
            float v = m->pos[i * 3 + k];
            if (v < m->min[k]) m->min[k] = v;
            if (v > m->max[k]) m->max[k] = v;
        }
    return erro ? -3 : 0;
}

void sgl_obj_liberar(sgl_obj_t *m)
{
    if (m->pos) OBJ_LIBERAR(m->pos);
    if (m->nor) OBJ_LIBERAR(m->nor);
    if (m->uv) OBJ_LIBERAR(m->uv);
    m->pos = m->nor = m->uv = 0;
    for (int i = 0; i < m->n_mats; i++)
        if (m->mat[i].tex) { glDeleteTextures(1, &m->mat[i].tex); m->mat[i].tex = 0; }
    m->n_vert = 0; m->n_grupos = 0;
}

void sgl_obj_desenhar(const sgl_obj_t *m, float tamanho)
{
    if (!m->n_vert) return;
    float cx = (m->min[0] + m->max[0]) * 0.5f, cy = (m->min[1] + m->max[1]) * 0.5f, cz = (m->min[2] + m->max[2]) * 0.5f;
    float lado = m->max[0] - m->min[0];
    if (m->max[1] - m->min[1] > lado) lado = m->max[1] - m->min[1];
    if (m->max[2] - m->min[2] > lado) lado = m->max[2] - m->min[2];
    glPushMatrix();
    if (tamanho > 0 && lado > 0) { float e = tamanho / lado; glScalef(e, e, e); }
    glTranslatef(-cx, -cy, -cz);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    for (int g = 0; g < m->n_grupos; g++) {
        const sgl_obj_mat_t *t = m->grupo[g].mat >= 0 ? &m->mat[m->grupo[g].mat] : 0;
        float kd[4] = { 0.7f, 0.7f, 0.72f, 1 }, ks[4] = { 0.2f, 0.2f, 0.2f, 1 }, ns = 16;
        if (t) { for (int k = 0; k < 3; k++) { kd[k] = t->kd[k]; ks[k] = t->ks[k]; } kd[3] = t->d; ns = t->ns; }
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, kd);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, ks);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, ns);
        int tex = t && t->tex && m->tem_uv;
        if (tex) {
            glBindTexture(GL_TEXTURE_2D, t->tex);
            glEnable(GL_TEXTURE_2D);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, 0, m->uv + (size_t)m->grupo[g].ini * 2);
        }
        glVertexPointer(3, GL_FLOAT, 0, m->pos + (size_t)m->grupo[g].ini * 3);
        glNormalPointer(GL_FLOAT, 0, m->nor + (size_t)m->grupo[g].ini * 3);
        glDrawArrays(GL_TRIANGLES, 0, m->grupo[g].n);
        if (tex) { glDisable(GL_TEXTURE_2D); glDisableClientState(GL_TEXTURE_COORD_ARRAY); }
    }
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
}

/* ------------------------------------------------------------ ficheros */

/* El texto entero de un fichero (con un 0 al final); NULL si no se puede. */
static char *ler_tudo(const char *caminho, int *n)
{
#ifdef SGL_ANFITRIAO
    FILE *f = fopen(caminho, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long t = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = OBJ_ALOCAR((size_t)t + 1);
    if (b && fread(b, 1, (size_t)t, f) != (size_t)t) { OBJ_LIBERAR(b); b = 0; }
    fclose(f);
    *n = (int)t;
    return b;
#else
    info_arquivo_t inf;
    if (info_arquivo(caminho, &inf) != 0 || inf.tamanho > 64u * 1024 * 1024) return 0;
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return 0;
    char *b = OBJ_ALOCAR(inf.tamanho + 1);
    int lidos = 0;
    while (b && lidos < (int)inf.tamanho) {
        int r = ler(fd, b + lidos, inf.tamanho - (unsigned)lidos);
        if (r <= 0) break;
        lidos += r;
    }
    fechar(fd);
    if (b && lidos != (int)inf.tamanho) { OBJ_LIBERAR(b); b = 0; }
    *n = lidos;
    return b;
#endif
}

/* pasta de `caminho` + nome */
static void ao_lado(char *d, int max, const char *caminho, const char *nome)
{
    int u = -1, i = 0;
    for (; caminho[i]; i++) if (caminho[i] == '/') u = i;
    int k = 0;
    for (i = 0; i <= u && k < max - 1; i++) d[k++] = caminho[i];
    for (i = 0; nome[i] && k < max - 1; i++) d[k++] = nome[i];
    d[k] = 0;
}

#ifndef SGL_ANFITRIAO
/* Una imagen de la libsagui a una textura de GL (RGB; mipmaps y bilineal). */
static GLuint textura_de(const char *caminho)
{
    imagem_t img;
    if (imagem_ler(caminho, &img) != 0) return 0;
    GLuint id = 0;
    if (img.larg <= SGL_MAX_LADO && img.alt <= SGL_MAX_LADO) {
        unsigned char *rgb = OBJ_ALOCAR((size_t)img.larg * img.alt * 3);
        if (rgb) {
            for (int i = 0; i < img.larg * img.alt; i++) {
                rgb[i * 3] = (unsigned char)(img.px[i] >> 16); rgb[i * 3 + 1] = (unsigned char)(img.px[i] >> 8);
                rgb[i * 3 + 2] = (unsigned char)img.px[i];
            }
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);
            glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.larg, img.alt, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
            glBindTexture(GL_TEXTURE_2D, 0);
            OBJ_LIBERAR(rgb);
        }
    }
    imagem_liberar(&img);
    return id;
}
#endif

int sgl_obj_cargar(const char *caminho, sgl_obj_t *m)
{
    int n;
    char *t = ler_tudo(caminho, &n);
    if (!t) return -4;
    int r = sgl_obj_ler(t, n, m);
    OBJ_LIBERAR(t);
    if (r < 0 && !m->n_vert) return r;
    char c[192];
    if (m->mtllib[0]) {
        ao_lado(c, sizeof c, caminho, m->mtllib);
        char *mt = ler_tudo(c, &n);
        if (mt) { sgl_obj_ler_mtl(mt, n, m); OBJ_LIBERAR(mt); }
    }
#ifndef SGL_ANFITRIAO
    for (int i = 0; i < m->n_mats; i++)
        if (m->mat[i].mapa[0]) { ao_lado(c, sizeof c, caminho, m->mat[i].mapa); m->mat[i].tex = textura_de(c); }
    if (!m->mtllib[0] && m->tem_uv) {
        /* Sin MTL: <nombre>.png (o .bmp, .jpg, .ppm) al lado, para todo el modelo. */
        static const char *const ext[4] = { ".png", ".bmp", ".jpg", ".ppm" };
        int k = 0;
        while (caminho[k] && k < (int)sizeof c - 6) { c[k] = caminho[k]; k++; }
        if (k > 4 && c[k - 4] == '.') k -= 4;
        GLuint id = 0;
        for (int e = 0; e < 4 && !id; e++) {
            for (int j = 0; j < 5; j++) c[k + j] = ext[e][j];
            id = textura_de(c);
        }
        if (id) {
            int g = material(m, "(textura)", 1);
            if (g >= 0) {
                m->mat[g].tex = id;
                m->mat[g].kd[0] = m->mat[g].kd[1] = m->mat[g].kd[2] = 1.0f;
                for (int i = 0; i < m->n_grupos; i++) m->grupo[i].mat = g;
            }
        }
    }
#endif
    return r;
}
