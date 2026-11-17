/* sdl2_rwops.c - SDL_RWops de ficheros (stdio) y de memoria (D374). */
#include "SDL_rwops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { Uint8 *base, *aqui, *fin; } mem_t;

static Sint64 f_size(SDL_RWops *c) { FILE *f = c->hidden1; long p = ftell(f); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, p, SEEK_SET); return n; }
static Sint64 f_seek(SDL_RWops *c, Sint64 o, int w) { FILE *f = c->hidden1; if (fseek(f, (long)o, w == RW_SEEK_SET ? SEEK_SET : w == RW_SEEK_CUR ? SEEK_CUR : SEEK_END)) return -1; return ftell(f); }
static size_t f_read(SDL_RWops *c, void *p, size_t s, size_t n) { return fread(p, s, n, c->hidden1); }
static size_t f_write(SDL_RWops *c, const void *p, size_t s, size_t n) { return fwrite(p, s, n, c->hidden1); }
static int f_close(SDL_RWops *c) { int r = fclose(c->hidden1); SDL_FreeRW(c); return r; }

static Sint64 m_size(SDL_RWops *c) { mem_t *m = c->hidden1; return m->fin - m->base; }
static Sint64 m_seek(SDL_RWops *c, Sint64 o, int w)
{
    mem_t *m = c->hidden1;
    Uint8 *p = w == RW_SEEK_SET ? m->base + o : w == RW_SEEK_CUR ? m->aqui + o : m->fin + o;
    if (p < m->base || p > m->fin) return -1;
    m->aqui = p;
    return m->aqui - m->base;
}
static size_t m_read(SDL_RWops *c, void *p, size_t s, size_t n)
{
    mem_t *m = c->hidden1;
    if (!s) return 0;
    size_t disp = (size_t)(m->fin - m->aqui) / s;
    if (n > disp) n = disp;
    memcpy(p, m->aqui, n * s);
    m->aqui += n * s;
    return n;
}
static size_t m_write(SDL_RWops *c, const void *p, size_t s, size_t n)
{
    mem_t *m = c->hidden1;
    if (!s) return 0;
    size_t disp = (size_t)(m->fin - m->aqui) / s;
    if (n > disp) n = disp;
    memcpy(m->aqui, p, n * s);
    m->aqui += n * s;
    return n;
}
static int m_close(SDL_RWops *c) { free(c->hidden1); SDL_FreeRW(c); return 0; }

SDL_RWops *SDL_AllocRW(void) { return calloc(1, sizeof(SDL_RWops)); }
void SDL_FreeRW(SDL_RWops *a) { free(a); }

SDL_RWops *SDL_RWFromFile(const char *file, const char *mode)
{
    FILE *f = fopen(file, mode);
    if (!f) { SDL_SetError("no se puede abrir %s", file); return NULL; }
    SDL_RWops *c = SDL_AllocRW();
    if (!c) { fclose(f); return NULL; }
    c->size = f_size; c->seek = f_seek; c->read = f_read; c->write = f_write; c->close = f_close;
    c->hidden1 = f;
    return c;
}

SDL_RWops *SDL_RWFromMem(void *mem, int size)
{
    SDL_RWops *c = SDL_AllocRW();
    mem_t *m = malloc(sizeof *m);
    if (!c || !m) { free(c); free(m); return NULL; }
    m->base = m->aqui = mem; m->fin = m->base + size;
    c->size = m_size; c->seek = m_seek; c->read = m_read; c->write = m_write; c->close = m_close;
    c->hidden1 = m;
    return c;
}
SDL_RWops *SDL_RWFromConstMem(const void *mem, int size) { return SDL_RWFromMem((void *)mem, size); }

Sint64 SDL_RWsize(SDL_RWops *c) { return c->size(c); }
Sint64 SDL_RWseek(SDL_RWops *c, Sint64 o, int w) { return c->seek(c, o, w); }
Sint64 SDL_RWtell(SDL_RWops *c) { return c->seek(c, 0, RW_SEEK_CUR); }
size_t SDL_RWread(SDL_RWops *c, void *p, size_t s, size_t n) { return c->read(c, p, s, n); }
size_t SDL_RWwrite(SDL_RWops *c, const void *p, size_t s, size_t n) { return c->write(c, p, s, n); }
int SDL_RWclose(SDL_RWops *c) { return c->close(c); }

void *SDL_LoadFile_RW(SDL_RWops *src, size_t *n, int cerrar)
{
    size_t cap = 65536, len = 0;
    unsigned char *b = malloc(cap);
    while (b) {
        size_t r = src->read(src, b + len, 1, cap - len);
        len += r;
        if (len < cap) break;
        cap *= 2;
        unsigned char *nb = realloc(b, cap);
        if (!nb) { free(b); b = NULL; }
        else b = nb;
    }
    if (cerrar) src->close(src);
    if (n) *n = len;
    return b;
}
