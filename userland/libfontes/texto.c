/* texto.c - la parte sencilla de la libfontes (D196): abrir una fuente de un
 * archivo, medir y dibujar texto, y el catálogo de fuentes del sistema. */
#include "sagui.h"
#include "fontes.h"

#define ARQ_CONF   "/home/.config/sagui.conf"
#define ARQ_SESION "/tmp/fuentes.conf"

fonte_t *fonte_abrir_archivo(const char *ruta)
{
    info_arquivo_t inf;
    if (info_arquivo(ruta, &inf) < 0 || inf.tamanho < 64 || inf.tamanho > (16u << 20)) return NULL;
    int fd = abrir(ruta, ABRIR_LER);
    if (fd < 0) return NULL;
    uint8_t *d = aloca(inf.tamanho);
    if (!d) { fechar(fd); return NULL; }
    unsigned n = 0;
    int r;
    while (n < inf.tamanho && (r = ler(fd, d + n, inf.tamanho - n)) > 0) n += (unsigned)r;
    fechar(fd);
    fonte_t *f = n == inf.tamanho ? fonte_abrir(d, n) : NULL;
    libera(d);
    return f;
}

/* Lee un carácter: UTF-8, o el byte suelto (Latin-1) si no es una secuencia válida. */
static int ler_car(const char *p, int resto, uint32_t *cp)
{
    uint8_t c = (uint8_t)p[0];
    int n = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
    if (!n || n > resto) { *cp = c; return 1; }
    uint32_t v = n == 1 ? c : n == 2 ? (c & 0x1Fu) : n == 3 ? (c & 0x0Fu) : (c & 0x07u);
    for (int k = 1; k < n; k++) {
        if (((uint8_t)p[k] & 0xC0) != 0x80) { *cp = c; return 1; }
        v = v << 6 | ((uint8_t)p[k] & 0x3Fu);
    }
    *cp = v;
    return n;
}

static unsigned car_visible(fonte_t *f, uint32_t cp)
{
    if (fonte_tem(f, cp)) return cp;
    return fonte_tem(f, '?') ? '?' : cp;
}

static int recorrer(fonte_t *f, uint32_t *dst, int larg, int alt, int x, int y,
                    const char *t, int n, int px, uint32_t cor)
{
    if (!f || !t) return 0;
    if (px < 4) px = 4;
    int asc = fonte_ascendente(f, px);
    int base = y + asc;
    int x64 = x * 64;
    for (int i = 0; i < n && t[i] && t[i] != '\n';) {
        uint32_t cp;
        i += ler_car(t + i, n - i, &cp);
        if (cp == '\r' || cp == 0xFEFF) continue;
        cp = car_visible(f, cp);
        int xe = (x64 + 32) >> 6;
        x64 += fonte_avanco(f, cp, px);
        if (!dst || cp == ' ' || cp == 0xA0 || cp < ' ') continue;
        int l, a, ox, oy;
        const uint8_t *g = fonte_glifo(f, cp, px, &l, &a, &ox, &oy);
        if (!g) continue;
        for (int yy = 0; yy < a; yy++) {
            int py = base + oy + yy;
            if (py < 0 || py >= alt) continue;
            const uint8_t *lin = g + yy * l;
            uint32_t *fila = dst + py * larg;
            for (int xx = 0; xx < l; xx++) {
                int pxx = xe + ox + xx, c = lin[xx];
                if (!c || pxx < 0 || pxx >= larg) continue;
                uint32_t d = fila[pxx];
                int dr = (d >> 16) & 255, dg = (d >> 8) & 255, db = d & 255;
                int sr = (cor >> 16) & 255, sg = (cor >> 8) & 255, sb = cor & 255;
                dr += ((sr - dr) * c + 127) / 255;
                dg += ((sg - dg) * c + 127) / 255;
                db += ((sb - db) * c + 127) / 255;
                fila[pxx] = (d & 0xFF000000u) | (uint32_t)dr << 16 | (uint32_t)dg << 8 | (uint32_t)db;
            }
        }
    }
    return (x64 + 32) / 64 - x;
}

int fonte_medir_n(fonte_t *f, const char *t, int n, int px) { return recorrer(f, NULL, 0, 0, 0, 0, t, n, px, 0); }
int fonte_medir(fonte_t *f, const char *t, int px) { return t ? recorrer(f, NULL, 0, 0, 0, 0, t, 1 << 20, px, 0) : 0; }

int fonte_texto_buf_n(fonte_t *f, uint32_t *dst, int larg, int alt, int x, int y,
                      const char *t, int n, int px, uint32_t cor)
{
    return recorrer(f, dst, larg, alt, x, y, t, n, px, cor);
}

int fonte_texto_buf(fonte_t *f, uint32_t *dst, int larg, int alt, int x, int y,
                    const char *t, int px, uint32_t cor)
{
    return t ? recorrer(f, dst, larg, alt, x, y, t, 1 << 20, px, cor) : 0;
}

/* ------------------------------------------------------------ el catálogo */

static bool es_fuente(const char *n)
{
    size_t l = tam_texto(n);
    if (l < 5) return false;
    const char *e = n + l - 4;
    return (e[0] == '.' && (e[1] | 32) == 'o' && (e[2] | 32) == 't' && (e[3] | 32) == 'f') ||
           (e[0] == '.' && (e[1] | 32) == 't' && (e[2] | 32) == 't' && (e[3] | 32) == 'f');
}

static int cmp_info(const fuente_info_t *a, const fuente_info_t *b)
{
    int r = compara_texto(a->familia, b->familia);
    return r ? r : compara_texto(a->estilo, b->estilo);
}

int fuentes_catalogo(fuente_info_t *v, int max)
{
    static const char *const dirs[2] = { FUENTES_DIR_APPS, FUENTES_DIR_HOME };
    static uint8_t tabla[4096];
    int n = 0;
    for (int d = 0; d < 2; d++) {
        entrada_pasta_t e;
        for (int i = 0; n < max && listar(dirs[d], i, &e) > 0; i++) {
            if (e.tipo == TIPO_PASTA || !es_fuente(e.nome)) continue;
            fuente_info_t *f = &v[n];
            memset(f, 0, sizeof *f);
            formatar_texto(f->ruta, sizeof f->ruta, "%s/%s", dirs[d], e.nome);
            int fd = abrir(f->ruta, ABRIR_LER);
            if (fd < 0) continue;
            int t = fonte_leer_tabla_name(fd, tabla, sizeof tabla);
            fechar(fd);
            if (t > 0) fonte_nombre_de_tabla(tabla, t, f->familia, sizeof f->familia, f->estilo, sizeof f->estilo);
            if (!f->familia[0]) copia_texto(f->familia, e.nome, sizeof f->familia);
            n++;
        }
    }
    for (int i = 1; i < n; i++) {
        fuente_info_t t = v[i];
        int j = i;
        while (j > 0 && cmp_info(&v[j - 1], &t) > 0) { v[j] = v[j - 1]; j--; }
        v[j] = t;
    }
    return n;
}

/* Busca «clave=valor» en un texto; true si la encontró. */
static bool buscar_clave(char *t, const char *clave, char *valor, int n)
{
    size_t lc = tam_texto(clave);
    for (char *p = t; *p;) {
        char *fin = acha_char(p, '\n');
        if (fin) *fin = 0;
        if (compara_texto_n(p, clave, lc) == 0 && p[lc] == '=') {
            copia_texto(valor, p + lc + 1, n);
            size_t l = tam_texto(valor);
            while (l && (valor[l - 1] == '\r' || valor[l - 1] == ' ')) valor[--l] = 0;
            return l > 0;
        }
        if (!fin) break;
        p = fin + 1;
    }
    return false;
}

bool fuentes_defecto(int uso, char *ruta, int n)
{
    static char t[2048];
    const char *clave = uso == FUENTE_TITULOS ? "fuente_titulos" : "fuente_documentos";
    static const char *const archivos[2] = { ARQ_SESION, ARQ_CONF };
    for (int a = 0; a < 2; a++) {
        int fd = abrir(archivos[a], ABRIR_LER);
        if (fd < 0) continue;
        int r = ler(fd, t, sizeof t - 1);
        fechar(fd);
        if (r <= 0) continue;
        t[r] = 0;
        if (buscar_clave(t, clave, ruta, n)) return true;
    }
    return false;
}

fonte_t *fuentes_abrir_defecto(int uso)
{
    char r[128];
    if (!fuentes_defecto(uso, r, sizeof r)) return NULL;
    return fonte_abrir_archivo(r);
}
