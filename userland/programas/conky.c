// SPDX-License-Identifier: GPL-3.0-only
/* conky - clon propio (no un porte) de Conky para el escritorio de SaguiOS (D365). Es un widget: lee
 * /home/.config/conky.conf con la sintaxis del .conkyrc clásico (opciones, una línea TEXT y debajo el texto
 * con ${variables}) y pinta una ventana de escritorio con los datos reales del sistema. Conky (GPL-3) no se
 * usa: depende de X11 y Lua; aquí solo se imita el formato del fichero. La tabla de variables soportadas
 * está en docs/conky.md. Lo no soportado se muestra tal cual (las variables) o se ignora (las opciones) y se
 * anota una vez en el registro («[CONKY] ...»).
 *
 * Consumo: una lectura del sistema por ciclo (update_interval, 1 s por defecto); si el texto resultante no
 * cambió no se repinta; el fichero se mira cada 5 s para recargarlo. */
#include "widget.h"

#define CONF_ARQ "/home/.config/conky.conf"
#define MAX_SEG   384
#define MAX_LIN   48
#define ARENA_MAX 8192
#define HIST_MAX  200
#define PAD 10

enum { S_TEXTO, S_VAR, S_COR, S_ALIGNR, S_ALIGNC, S_HR, S_VOFFSET, S_OFFSET, S_GOTO };
enum { V_CPU, V_CPUBAR, V_CPUGRAPH, V_MEM, V_MEMMAX, V_MEMPERC, V_MEMEASYFREE, V_MEMBAR, V_MEMGRAPH,
       V_SWAP, V_SWAPMAX, V_SWAPPERC, V_SWAPBAR, V_FSUSED, V_FSSIZE, V_FSFREE, V_FSUSEDPERC, V_FSFREEPERC, V_FSBAR,
       V_DOWN, V_UP, V_DOWNGRAPH, V_UPGRAPH, V_UPTIME, V_UPTIME_SHORT, V_TIME, V_NODENAME, V_KERNEL, V_SYSNAME,
       V_MACHINE, V_TOP, V_TEMP, V_BATPERC, V_BATBAR, V_PROCESSES, V_RUNNING, V_CPUCOUNT, V_NADA };

typedef struct { const char *nome; int tipo, var, por_mem; } nome_var_t;
static const nome_var_t tabla[] = {
    { "cpu", S_VAR, V_CPU, 0 }, { "cpubar", S_VAR, V_CPUBAR, 0 }, { "cpugraph", S_VAR, V_CPUGRAPH, 0 },
    { "mem", S_VAR, V_MEM, 0 }, { "memmax", S_VAR, V_MEMMAX, 0 }, { "memperc", S_VAR, V_MEMPERC, 0 },
    { "memeasyfree", S_VAR, V_MEMEASYFREE, 0 }, { "memfree", S_VAR, V_MEMEASYFREE, 0 },
    { "membar", S_VAR, V_MEMBAR, 0 }, { "memgraph", S_VAR, V_MEMGRAPH, 0 },
    { "swap", S_VAR, V_SWAP, 0 }, { "swapmax", S_VAR, V_SWAPMAX, 0 }, { "swapperc", S_VAR, V_SWAPPERC, 0 },
    { "swapbar", S_VAR, V_SWAPBAR, 0 },
    { "fs_used", S_VAR, V_FSUSED, 0 }, { "fs_size", S_VAR, V_FSSIZE, 0 }, { "fs_free", S_VAR, V_FSFREE, 0 },
    { "fs_used_perc", S_VAR, V_FSUSEDPERC, 0 }, { "fs_free_perc", S_VAR, V_FSFREEPERC, 0 }, { "fs_bar", S_VAR, V_FSBAR, 0 },
    { "downspeed", S_VAR, V_DOWN, 0 }, { "downspeedf", S_VAR, V_DOWN, 0 }, { "upspeed", S_VAR, V_UP, 0 }, { "upspeedf", S_VAR, V_UP, 0 },
    { "downspeedgraph", S_VAR, V_DOWNGRAPH, 0 }, { "upspeedgraph", S_VAR, V_UPGRAPH, 0 },
    { "uptime", S_VAR, V_UPTIME, 0 }, { "uptime_short", S_VAR, V_UPTIME_SHORT, 0 }, { "time", S_VAR, V_TIME, 0 },
    { "nodename", S_VAR, V_NODENAME, 0 }, { "hostname", S_VAR, V_NODENAME, 0 }, { "kernel", S_VAR, V_KERNEL, 0 },
    { "sysname", S_VAR, V_SYSNAME, 0 }, { "machine", S_VAR, V_MACHINE, 0 },
    { "top", S_VAR, V_TOP, 0 }, { "top_mem", S_VAR, V_TOP, 1 },
    { "hwmon", S_VAR, V_TEMP, 0 }, { "acpitemp", S_VAR, V_TEMP, 0 }, { "battery_percent", S_VAR, V_BATPERC, 0 },
    { "battery_bar", S_VAR, V_BATBAR, 0 }, { "processes", S_VAR, V_PROCESSES, 0 }, { "running_processes", S_VAR, V_RUNNING, 0 },
    { "cpu_count", S_VAR, V_CPUCOUNT, 0 }, { "color", S_COR, 0, 0 }, { "alignr", S_ALIGNR, 0, 0 }, { "alignc", S_ALIGNC, 0, 0 },
    { "hr", S_HR, 0, 0 }, { "stippled_hr", S_HR, 0, 0 }, { "voffset", S_VOFFSET, 0, 0 }, { "offset", S_OFFSET, 0, 0 },
    { "goto", S_GOTO, 0, 0 }, { "font", S_TEXTO, V_NADA, 0 }, { "tab", S_TEXTO, V_NADA, 0 },
};
#define N_TABLA (int)(sizeof tabla / sizeof tabla[0])

typedef struct {
    uint8_t tipo, var, por_mem;
    int     h, w;           /* barras, gráficas, hr: alto y ancho (0 = el ancho que quede) */
    int     n;              /* top: posición; voffset/offset/goto/alignr: píxeles; color: índice (-2 = el de por defecto) */
    uint32_t cor;           /* color propio de la gráfica o del color pedido */
    bool    tem_cor;
    char   *txt, *arg, *arg2;   /* texto fijo; interfaz/ruta/formato; tipo de top */
    int    *hist; int nh;       /* gráficas: muestras */
    int     val;            /* barras y gráficas: valor actual (0-100 en barras; décimas de % o bytes/s en gráficas) */
    char    ev[48];         /* texto evaluado */
} seg_t;

static seg_t segs[MAX_SEG];
static int nseg;
typedef struct { int ini, qtd; } lin_t;
static lin_t lin[MAX_LIN];
static int nlin;
static char arena[ARENA_MAX];
static int arena_n;

/* opciones de la cabecera */
static int  intervalo_ms = 1000, gap_x = 20, gap_y = 40, min_l = 280, min_a = 0, max_l;
static int  alin_h = 0, alin_v = 0;      /* 0 izquierda/arriba, 1 centro, 2 derecha/abajo */
static bool transparente, borde;
static int  opacidade = 100;
static uint32_t cor_padrao = WCOR_TEXTO, cores[10];
static int  bar_alt = 8, graf_alt = 25;
static unsigned ult_hash;
static bool hash_ok;

static char *arena_copia(const char *s, int n)
{
    if (arena_n + n + 1 > ARENA_MAX) return NULL;
    char *p = arena + arena_n;
    for (int i = 0; i < n; i++) p[i] = s[i];
    p[n] = 0;
    arena_n += n + 1;
    return p;
}

/* ---------------------------------------------------------------- colores y números */

static bool cor_texto(const char *s, uint32_t *c)
{
    static const struct { const char *n; uint32_t c; } nomes[] = {
        { "white", 0xFFFFFF }, { "black", 0x000000 }, { "red", 0xFF4040 }, { "green", 0x5CD08A }, { "blue", 0x5AA6E8 },
        { "yellow", 0xE8C65A }, { "orange", 0xFF9A3C }, { "grey", 0x909090 }, { "gray", 0x909090 }, { "cyan", 0x40E0E0 },
        { "magenta", 0xE060E0 }, { "lightgrey", 0xC8C8C8 }, { "lightgray", 0xC8C8C8 }, { "darkgrey", 0x505050 },
        { "darkgray", 0x505050 }, { "purple", 0xA060E0 }, { "pink", 0xFF90B0 }, { "brown", 0xA0703C },
    };
    if (*s == '#') s++;
    int n = 0;
    uint32_t v = 0;
    while ((s[n] >= '0' && s[n] <= '9') || ((s[n] | 32) >= 'a' && (s[n] | 32) <= 'f')) {
        v = v * 16 + (uint32_t)(s[n] <= '9' ? s[n] - '0' : (s[n] | 32) - 'a' + 10);
        n++;
    }
    if (n == 6 && !s[n]) { *c = v; return true; }
    for (int i = 0; i < (int)(sizeof nomes / sizeof nomes[0]); i++)
        if (compara_texto(s, nomes[i].n) == 0) { *c = nomes[i].c; return true; }
    return false;
}

/* Satura en ±100000: un número enorme del fichero no desborda las sumas de posiciones y tamaños. */
#define NUM_MAX 100000
static int numero(const char *s)
{
    bool neg = false;
    if (*s == '-') { neg = true; s++; }
    int v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s++ - '0'); if (v > NUM_MAX) v = NUM_MAX; }
    return neg ? -v : v;
}

static int em_ms(const char *s)   /* "1.5" -> 1500 */
{
    int e = numero(s), d = 0;
    while (*s && *s != '.') s++;
    if (*s == '.' && s[1] >= '0' && s[1] <= '9') { d = (s[1] - '0') * 100; if (s[2] >= '0' && s[2] <= '9') d += (s[2] - '0') * 10; }
    return e * 1000 + d;
}

static bool prefijo(const char *s, const char *p) { return compara_texto_n(s, p, tam_texto(p)) == 0; }

/* ---------------------------------------------------------------- análisis del fichero */

static void avisar_uma_vez(const char *tipo, const char *nome)
{
    static char vistos[16][24];
    static int nv;
    for (int i = 0; i < nv; i++) if (compara_texto(vistos[i], nome) == 0) return;
    if (nv < 16) copia_texto(vistos[nv++], nome, 24);
    registrar("[CONKY] %s no soportada: %s\n", tipo, nome);
}

static bool eh_sim(const char *v) { return v[0] == 'y' || v[0] == 'Y' || v[0] == 't' || v[0] == 'T' || v[0] == '1'; }

static const char *segundo_numero(const char *v)
{
    while (*v && *v != ' ') v++;
    while (*v == ' ') v++;
    return v;
}

static void opcao(const char *k, const char *v)
{
    static const char *const ignoradas[] = {
        "use_xft", "font", "xftfont", "xftalpha", "own_window", "own_window_type", "own_window_hints", "own_window_argb_visual",
        "double_buffer", "background", "no_buffers", "cpu_avg_samples", "net_avg_samples", "text_buffer_size",
        "override_utf8_locale", "draw_shades", "draw_outline", "draw_graph_borders", "border_width", "border_inner_margin",
        "border_outer_margin", "short_units", "pad_percents", "uppercase", "use_spacer", "format_human_readable",
        "total_run_times", "update_interval_on_battery", "xinerama_head", "stippled_borders", "out_to_console", "out_to_x",
        "extra_newline", "imlib_cache_size", "own_window_colour", "own_window_class", "own_window_title",
    };
    if (!compara_texto(k, "update_interval")) {
        intervalo_ms = em_ms(v);
        if (intervalo_ms < 250) intervalo_ms = 250;
        if (intervalo_ms > 3600000) intervalo_ms = 3600000;
    }
    else if (!compara_texto(k, "alignment")) {
        /* top_left, top_right, top_middle, bottom_*, middle_left, middle_right, middle_middle (y las abreviaturas tl, tr, bl...) */
        alin_h = alin_v = 0;
        if (prefijo(v, "bottom") || v[0] == 'b') alin_v = 2;
        else if (prefijo(v, "middle") || (v[0] == 'm' && v[1] == 'l') || (v[0] == 'm' && v[1] == 'r')) alin_v = 1;
        int n = (int)tam_texto(v);
        if ((n >= 5 && compara_texto(v + n - 5, "right") == 0) || (n == 2 && v[1] == 'r')) alin_h = 2;
        else if ((n >= 6 && compara_texto(v + n - 6, "middle") == 0 && !prefijo(v, "middle_left")) || (n == 2 && v[1] == 'm')) alin_h = 1;
    }
    else if (!compara_texto(k, "gap_x")) gap_x = numero(v);
    else if (!compara_texto(k, "gap_y")) gap_y = numero(v);
    else if (!compara_texto(k, "minimum_size")) { min_l = numero(v); const char *s = segundo_numero(v); if (*s) min_a = numero(s); }
    else if (!compara_texto(k, "maximum_width")) max_l = numero(v);
    else if (!compara_texto(k, "own_window_transparent")) transparente = eh_sim(v);
    else if (!compara_texto(k, "own_window_argb_value")) {
        opacidade = numero(v) * 100 / 255;
        if (opacidade < 10) opacidade = 10;
        if (opacidade > 100) opacidade = 100;
    }
    else if (!compara_texto(k, "default_color")) { uint32_t c; if (cor_texto(v, &c)) cor_padrao = c; }
    else if (prefijo(k, "color") && k[5] >= '0' && k[5] <= '9' && !k[6]) { uint32_t c; if (cor_texto(v, &c)) cores[k[5] - '0'] = c; }
    else if (!compara_texto(k, "draw_borders")) borde = eh_sim(v);
    else if (!compara_texto(k, "default_bar_size")) { const char *s = segundo_numero(v); if (*s) bar_alt = numero(s); }
    else if (!compara_texto(k, "default_graph_size")) { const char *s = segundo_numero(v); if (*s) graf_alt = numero(s); }
    else {
        for (int i = 0; i < (int)(sizeof ignoradas / sizeof ignoradas[0]); i++)
            if (!compara_texto(k, ignoradas[i])) return;   /* se acepta y se ignora: la letra es la del sistema (8x16) */
        avisar_uma_vez("opción", k);
    }
}

static void media(char *s)   /* quita comillas, comas y espacios de los bordes (sintaxis nueva con conky.config = { ... }) */
{
    int n = (int)tam_texto(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == ',' || s[n - 1] == ';' || s[n - 1] == '\r' || s[n - 1] == '\t')) s[--n] = 0;
    if (n >= 2 && (s[0] == '\'' || s[0] == '"') && s[n - 1] == s[0]) { for (int i = 0; i < n - 2; i++) s[i] = s[i + 1]; s[n - 2] = 0; }
}

static seg_t *nova_seg(int tipo)
{
    if (nseg >= MAX_SEG) return NULL;
    seg_t *s = &segs[nseg++];
    memset(s, 0, sizeof *s);
    s->tipo = (uint8_t)tipo;
    s->n = -1;
    return s;
}

static void texto_fixo(const char *t, int n)
{
    if (n <= 0) return;
    seg_t *s = nova_seg(S_TEXTO);
    if (s) s->txt = arena_copia(t, n);
    if (s && !s->txt) nseg--;
}

/* ${nome args}: separa los argumentos y rellena el segmento. false = no se conoce. */
static bool analisar_var(const char *nome, int nl, const char *args)
{
    char nm[32];
    if (nl <= 0 || nl >= (int)sizeof nm) return false;
    for (int i = 0; i < nl; i++) nm[i] = nome[i];
    nm[nl] = 0;
    int ti = -1;
    for (int i = 0; i < N_TABLA; i++) if (compara_texto(tabla[i].nome, nm) == 0) { ti = i; break; }
    if (ti < 0 && prefijo(nm, "color") && nm[5] >= '0' && nm[5] <= '9' && !nm[6]) {
        seg_t *s = nova_seg(S_COR);
        if (s) s->n = nm[5] - '0';
        return true;
    }
    if (ti < 0) return false;
    if (tabla[ti].tipo == S_TEXTO) return true;   /* ${font ...}, ${tab}: se ignoran */
    seg_t *s = nova_seg(tabla[ti].tipo);
    if (!s) return true;
    s->var = (uint8_t)tabla[ti].var;
    s->por_mem = (uint8_t)tabla[ti].por_mem;
    char tok[8][40];
    int nt = 0;
    for (const char *p = args; *p && nt < 8;) {
        while (*p == ' ') p++;
        if (!*p) break;
        int k = 0;
        if (s->tipo == S_VAR && s->var == V_TIME && nt == 0) { while (*p && k < 39) tok[nt][k++] = *p++; }   /* el formato lleva espacios */
        else while (*p && *p != ' ' && k < 39) tok[nt][k++] = *p++;
        tok[nt++][k] = 0;
    }
    switch (s->tipo) {
    case S_COR:
        if (nt > 0) {
            uint32_t c;
            if (cor_texto(tok[0], &c)) { s->cor = c; s->tem_cor = true; }
            else if (tok[0][0] >= '0' && tok[0][0] <= '9' && !tok[0][1]) s->n = tok[0][0] - '0';
        } else s->n = -2;   /* ${color}: el de por defecto */
        break;
    case S_ALIGNR: case S_ALIGNC: case S_VOFFSET: case S_OFFSET: case S_GOTO:
        s->n = nt > 0 ? numero(tok[0]) : 0;
        break;
    case S_HR:
        s->h = nt > 0 ? numero(tok[0]) : 1;
        if (s->h < 1) s->h = 1;
        break;
    case S_VAR:
        if (s->var == V_TIME) { const char *f = nt > 0 ? tok[0] : "%H:%M"; s->arg = arena_copia(f, (int)tam_texto(f)); break; }
        if (s->var == V_TOP) {
            if (nt > 0) s->arg2 = arena_copia(tok[0], (int)tam_texto(tok[0]));
            s->n = nt > 1 ? numero(tok[1]) - 1 : 0;
            if (s->n < 0) s->n = 0;
            break;
        }
        for (int i = 0; i < nt; i++) {
            const char *t = tok[i];
            bool tam = *t >= '0' && *t <= '9';
            for (const char *q = t; *q && tam; q++) if (!((*q >= '0' && *q <= '9') || *q == ',')) tam = false;
            uint32_t c;
            if (tam) {
                s->h = numero(t);
                const char *cm = t;
                while (*cm && *cm != ',') cm++;
                if (*cm == ',') s->w = numero(cm + 1);
            } else if (cor_texto(t, &c)) {
                s->cor = c; s->tem_cor = true;   /* #RRGGBB, RRGGBB o un nombre de color */
            } else if (!s->arg) s->arg = arena_copia(t, (int)tam_texto(t));
        }
        if (s->var == V_CPUBAR || s->var == V_MEMBAR || s->var == V_SWAPBAR || s->var == V_FSBAR || s->var == V_BATBAR) {
            if (!s->h) s->h = bar_alt;
        }
        if (s->var == V_CPUGRAPH || s->var == V_MEMGRAPH || s->var == V_DOWNGRAPH || s->var == V_UPGRAPH) {
            if (!s->h) s->h = graf_alt;
            s->hist = aloca(HIST_MAX * sizeof(int));
            if (s->hist) memset(s->hist, 0, HIST_MAX * sizeof(int));
            else s->tipo = S_TEXTO, s->txt = "";
        }
        break;
    }
    return true;
}

static void analisar_linha(const char *t, int n)
{
    int ini_seg = nseg;
    int i = 0, ini_txt = 0;
    while (i < n) {
        if (t[i] != '$') { i++; continue; }
        if (i + 1 < n && t[i + 1] == '$') { texto_fixo(t + ini_txt, i - ini_txt + 1); i += 2; ini_txt = i; continue; }
        texto_fixo(t + ini_txt, i - ini_txt);
        int j = i + 1;
        char args_b[96];
        if (j < n && t[j] == '{') {
            int fim = j + 1;
            while (fim < n && t[fim] != '}') fim++;
            int ns = j + 1;
            while (ns < fim && t[ns] != ' ') ns++;
            int nl = ns - (j + 1);
            int al = 0;
            for (int q = ns + 1; q < fim && al < (int)sizeof args_b - 1; q++) args_b[al++] = t[q];
            args_b[al] = 0;
            if (!analisar_var(t + j + 1, nl, args_b)) {
                char nm[32];
                int k = 0;
                for (; k < nl && k < 31; k++) nm[k] = t[j + 1 + k];
                nm[k] = 0;
                avisar_uma_vez("variable", nm);
                texto_fixo(t + i, (fim < n ? fim + 1 : n) - i);   /* se muestra tal cual */
            }
            i = fim < n ? fim + 1 : n;
        } else {
            int fim = j;
            while (fim < n && ((t[fim] >= 'a' && t[fim] <= 'z') || (t[fim] >= 'A' && t[fim] <= 'Z') || t[fim] == '_' || (t[fim] >= '0' && t[fim] <= '9'))) fim++;
            int nl = fim - j;
            if (nl == 0 || !analisar_var(t + j, nl, "")) { texto_fixo(t + i, fim - i); if (nl == 0) fim = i + 1; }   /* $ suelto o variable desconocida */
            i = fim;
        }
        ini_txt = i;
    }
    texto_fixo(t + ini_txt, n - ini_txt);
    if (nlin < MAX_LIN) { lin[nlin].ini = ini_seg; lin[nlin].qtd = nseg - ini_seg; nlin++; }
}

static const char *conf_padrao(void)
{
    static char b[2600];
    formatar_texto(b, sizeof b,
        "# conky.conf - Conky de SaguiOS (D365). Sintaxis del .conkyrc clásico: opciones, una línea TEXT y el texto.\n"
        "# Al guardar este fichero el widget se recarga solo (en unos segundos). Ver docs/conky.md.\n"
        "update_interval 1.0\n"
        "alignment top_left\n"
        "gap_x 20\n"
        "gap_y 40\n"
        "minimum_size 300 0\n"
        "own_window_transparent no\n"
        "default_color E8F2EA\n"
        "color1 5CD08A\n"
        "color2 E8C65A\n"
        "color3 5AA6E8\n"
        "default_bar_size 0 8\n"
        "default_graph_size 0 26\n"
        "\n"
        "TEXT\n"
        "${color1}${nodename}${color}${alignr}SaguiOS ${kernel}\n"
        "${color3}${hr 1}${color}\n"
        "${color2}%s${color}${alignr}${uptime}\n"
        "${color2}CPU${color}  ${cpu}%%${alignr}${cpugraph 18,150 5CD08A}\n"
        "${color2}%s${color}  ${mem} / ${memmax}${alignr}${memperc}%%\n"
        "${membar 8}\n"
        "${color2}%s${color}  ${fs_used /} / ${fs_size /}\n"
        "${fs_bar 8 /}\n"
        "${color2}%s${color}  ${downspeed eth0}/s${alignr}${downspeedgraph eth0 18,110 5AA6E8}\n"
        "${color2}%s${color}  ${upspeed eth0}/s${alignr}${upspeedgraph eth0 18,110 E8C65A}\n"
        "${color3}${hr 1}${color}\n"
        "${color2}%s${color}\n"
        "${top name 1}${alignr}${top cpu 1}%%\n"
        "${top name 2}${alignr}${top cpu 2}%%\n"
        "${top name 3}${alignr}${top cpu 3}%%\n"
        "${alignc}${time %%H:%%M:%%S}\n",
        _("Tiempo en marcha"), _("Memoria"), _("Disco"), _("Bajada"), _("Subida"), _("Procesos"));
    return b;
}

static void ler_conf(void)
{
    static char b[8192];
    int fd = abrir(CONF_ARQ, ABRIR_LER);
    int n = 0;
    if (fd >= 0) { n = ler(fd, b, sizeof b - 1); fechar(fd); }
    if (n <= 0) {   /* la primera vez: un conky.conf de ejemplo */
        const char *d = conf_padrao();
        n = (int)tam_texto(d);
        if (n > (int)sizeof b - 1) n = (int)sizeof b - 1;
        for (int i = 0; i < n; i++) b[i] = d[i];
        criar_pasta("/home/.config");
        fd = abrir(CONF_ARQ, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fd >= 0) { escrever(fd, d, (unsigned)n); fechar(fd); registrar("[CONKY] creado %s de ejemplo\n", CONF_ARQ); }
    }
    b[n] = 0;
    bool em_texto = false;
    for (char *p = b; *p;) {
        char *fim = p;
        while (*fim && *fim != '\n') fim++;
        char salvo = *fim;
        *fim = 0;
        int l = (int)(fim - p);
        if (l > 0 && p[l - 1] == '\r') p[--l] = 0;
        if (em_texto) {
            if (compara_texto(p, "]]") == 0) em_texto = false;
            else analisar_linha(p, l);
        } else {
            char *q = p;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == 0 || *q == '#' || (q[0] == '-' && q[1] == '-')) { /* comentario o línea vacía */ }
            else if (compara_texto(q, "TEXT") == 0) em_texto = true;
            else if (prefijo(q, "conky.text")) em_texto = true;
            else if (prefijo(q, "conky.config") || compara_texto(q, "}") == 0) { /* marco de la sintaxis nueva */ }
            else {
                char *k = q, *v = q;
                while (*v && *v != ' ' && *v != '\t' && *v != '=') v++;
                char *fk = v;
                while (*v == ' ' || *v == '\t' || *v == '=') v++;
                *fk = 0;
                media(v);
                if (*k) opcao(k, v);
            }
        }
        *fim = salvo;
        p = salvo ? fim + 1 : fim;
    }
    if (nlin == 0) analisar_linha("conky.conf sin TEXT", 19);
}

/* ---------------------------------------------------------------- valores */

static void dos_digitos(char *d, int n, unsigned x10)   /* 123 -> "12.30" */
{
    formatar_texto(d, (size_t)n, "%u.%02u", x10 / 10, x10 % 10 * 10);
}

static unsigned perc(unsigned a, unsigned b) { return b ? (unsigned)((unsigned long long)a * 100 / b) : 0; }

static void avaliar(seg_t *s)
{
    char *o = s->ev;
    const int n = (int)sizeof s->ev;
    unsigned a = 0, b = 0;
    o[0] = 0;
    s->val = 0;
    switch (s->var) {
    case V_CPU: formatar_texto(o, (size_t)n, "%u", widget_cpu_x10() / 10); break;
    case V_CPUBAR: s->val = (int)(widget_cpu_x10() / 10); break;
    case V_CPUGRAPH: s->val = (int)widget_cpu_x10(); break;
    case V_MEM: widget_mem(&a, &b); widget_kb(o, n, a); break;
    case V_MEMMAX: widget_mem(&a, &b); widget_kb(o, n, b); break;
    case V_MEMEASYFREE: widget_mem(&a, &b); widget_kb(o, n, b > a ? b - a : 0); break;
    case V_MEMPERC: widget_mem(&a, &b); formatar_texto(o, (size_t)n, "%u", perc(a, b)); break;
    case V_MEMBAR: widget_mem(&a, &b); s->val = (int)perc(a, b); break;
    case V_MEMGRAPH: widget_mem(&a, &b); s->val = (int)perc(a, b) * 10; break;
    case V_SWAP: widget_swap(&a, &b); widget_kb(o, n, a); break;
    case V_SWAPMAX: widget_swap(&a, &b); widget_kb(o, n, b); break;
    case V_SWAPPERC: widget_swap(&a, &b); formatar_texto(o, (size_t)n, "%u", perc(a, b)); break;
    case V_SWAPBAR: widget_swap(&a, &b); s->val = (int)perc(a, b); break;
    case V_FSUSED: case V_FSSIZE: case V_FSFREE: case V_FSUSEDPERC: case V_FSFREEPERC: case V_FSBAR:
        if (widget_disco(s->arg ? s->arg : "/", &a, &b) < 0) { copia_texto(o, "n/d", (size_t)n); break; }
        if (s->var == V_FSUSED) widget_kb(o, n, a);
        else if (s->var == V_FSSIZE) widget_kb(o, n, b);
        else if (s->var == V_FSFREE) widget_kb(o, n, b - a);
        else if (s->var == V_FSBAR) s->val = (int)perc(a, b);
        else formatar_texto(o, (size_t)n, "%u", s->var == V_FSUSEDPERC ? perc(a, b) : 100 - perc(a, b));
        break;
    case V_DOWN: case V_UP: case V_DOWNGRAPH: case V_UPGRAPH: {
        unsigned rx, tx;
        widget_rede(&rx, &tx, NULL, NULL, NULL);
        bool lo = s->arg && compara_texto(s->arg, "lo") == 0;   /* solo hay datos de la eth0 */
        unsigned v = lo ? 0 : (s->var == V_DOWN || s->var == V_DOWNGRAPH ? rx : tx);
        if (s->var == V_DOWN || s->var == V_UP) widget_bytes(o, n, v);
        else s->val = (int)v;
        break;
    }
    case V_UPTIME: case V_UPTIME_SHORT: {
        unsigned t = widget_segundos_ligado(), d = t / 86400, h = t / 3600 % 24, m = t / 60 % 60, sg = t % 60;
        if (s->var == V_UPTIME_SHORT) {
            if (d) formatar_texto(o, (size_t)n, "%ud %uh %um", d, h, m);
            else formatar_texto(o, (size_t)n, "%uh %um", h, m);
        }
        else if (d) formatar_texto(o, (size_t)n, "%ud %uh %um %us", d, h, m, sg);
        else if (h) formatar_texto(o, (size_t)n, "%uh %um %us", h, m, sg);
        else if (m) formatar_texto(o, (size_t)n, "%um %us", m, sg);
        else formatar_texto(o, (size_t)n, "%us", sg);
        break;
    }
    case V_TIME: { fuso_data_t d; agora_local(&d); formatar_data(o, n, s->arg ? s->arg : "%H:%M", &d); break; }
    case V_NODENAME: copia_texto(o, widget_nome_host(), (size_t)n); break;
    case V_KERNEL: copia_texto(o, widget_versao(), (size_t)n); break;
    case V_SYSNAME: copia_texto(o, "SaguiOS", (size_t)n); break;
    case V_MACHINE: copia_texto(o, "i686", (size_t)n); break;
    case V_TOP: {
        char nome[32];
        unsigned c = 0, m = 0;
        int pid = 0;
        const char *tipo = s->arg2 ? s->arg2 : "name";
        if (widget_top(s->n, s->por_mem != 0, nome, sizeof nome, &c, &m, &pid) < 0) { copia_texto(o, "-", (size_t)n); break; }
        if (!compara_texto(tipo, "name")) copia_texto(o, nome, (size_t)n);
        else if (!compara_texto(tipo, "pid")) formatar_texto(o, (size_t)n, "%d", pid);
        else if (!compara_texto(tipo, "cpu")) dos_digitos(o, n, c);
        else if (!compara_texto(tipo, "mem")) { unsigned ta, tb; widget_mem(&ta, &tb); dos_digitos(o, n, tb ? (unsigned)((unsigned long long)m * 1000 / tb) : 0); }
        else if (!compara_texto(tipo, "mem_res") || !compara_texto(tipo, "mem_vsize")) widget_kb(o, n, m);
        else copia_texto(o, "?", (size_t)n);
        break;
    }
    case V_TEMP: { int t = widget_temperatura(); if (t < 0) copia_texto(o, "n/d", (size_t)n); else formatar_texto(o, (size_t)n, "%d", t / 10); break; }
    case V_BATPERC: { int p = widget_bateria(); if (p < 0) copia_texto(o, "n/d", (size_t)n); else formatar_texto(o, (size_t)n, "%d", p); break; }
    case V_BATBAR: { int p = widget_bateria(); s->val = p < 0 ? 0 : p; break; }
    case V_PROCESSES: case V_RUNNING: {
        static info_processo_t v[64];
        int np = info_sistema(INFO_PROCESSOS, v, 64), c = 0;
        for (int i = 0; i < np; i++) if (s->var == V_PROCESSES || v[i].estado == 2) c++;
        formatar_texto(o, (size_t)n, "%d", c);
        break;
    }
    case V_CPUCOUNT: formatar_texto(o, (size_t)n, "%d", widget_ncpu()); break;
    }
}

static bool eh_barra(const seg_t *s) { return s->var == V_CPUBAR || s->var == V_MEMBAR || s->var == V_SWAPBAR || s->var == V_FSBAR || s->var == V_BATBAR; }
static bool eh_grafica(const seg_t *s) { return s->var == V_CPUGRAPH || s->var == V_MEMGRAPH || s->var == V_DOWNGRAPH || s->var == V_UPGRAPH; }

/* Evalúa todo (una lectura del sistema por ciclo) y empuja las muestras de las gráficas. */
static void evaluar_todo(bool empurrar)
{
    widget_datos_tique();
    for (int i = 0; i < nseg; i++) {
        seg_t *s = &segs[i];
        if (s->tipo != S_VAR) continue;
        avaliar(s);
        if (empurrar && eh_grafica(s) && s->hist) {
            if (s->nh < HIST_MAX) s->hist[s->nh++] = s->val;
            else { for (int k = 1; k < HIST_MAX; k++) s->hist[k - 1] = s->hist[k]; s->hist[HIST_MAX - 1] = s->val; }
        }
    }
}

/* ---------------------------------------------------------------- diseño y dibujo */

static int largo_seg(const seg_t *s)
{
    if (s->tipo == S_TEXTO) return (int)tam_texto(s->txt) * LETRA_L;
    if (s->tipo == S_VAR) {
        if (eh_barra(s) || eh_grafica(s)) return s->w;   /* 0 = el ancho que quede */
        return (int)tam_texto(s->ev) * LETRA_L;
    }
    return 0;
}

static int alto_linha(const lin_t *l)
{
    int h = 16;
    for (int i = 0; i < l->qtd; i++) {
        const seg_t *s = &segs[l->ini + i];
        if (s->tipo != S_VAR) continue;
        if (eh_grafica(s) && s->h + 2 > h) h = s->h + 2;
        if (eh_barra(s) && s->h + 4 > h) h = s->h + 4;
    }
    return h;
}

/* Calcula el tamaño (j == NULL) o dibuja. wtot: ancho de la ventana. Devuelve el alto usado. */
static int diseno(jan_t *j, int wtot, int *ancho_max)
{
    int y = PAD, anchomax = 0;
    uint32_t cor = cor_padrao;
    for (int li = 0; li < nlin; li++) {
        const lin_t *l = &lin[li];
        int lh = alto_linha(l);
        for (int i = 0; i < l->qtd; i++) if (segs[l->ini + i].tipo == S_VOFFSET) y += segs[l->ini + i].n;   /* baja (o sube) esta línea y las siguientes */
        int x = PAD, total = 0;
        for (int i = 0; i < l->qtd; i++) {
            seg_t *s = &segs[l->ini + i];
            total += s->tipo == S_OFFSET ? s->n : largo_seg(s);
        }
        if (total + 2 * PAD > anchomax) anchomax = total + 2 * PAD;
        for (int i = 0; i < l->qtd; i++) {
            seg_t *s = &segs[l->ini + i];
            switch (s->tipo) {
            case S_COR:
                if (s->n == -2) cor = cor_padrao;
                else if (s->tem_cor) cor = s->cor;
                else if (s->n >= 0 && s->n <= 9) cor = cores[s->n] ? cores[s->n] : cor_padrao;
                break;
            case S_OFFSET: x += s->n; break;
            case S_GOTO: x = PAD + s->n; break;
            case S_ALIGNR: case S_ALIGNC: {
                int resto = 0;
                for (int k = i + 1; k < l->qtd; k++) {
                    seg_t *t = &segs[l->ini + k];
                    resto += t->tipo == S_OFFSET ? t->n : largo_seg(t);
                }
                x = s->tipo == S_ALIGNR ? wtot - PAD - resto - s->n : (wtot - resto) / 2;
                if (x < PAD) x = PAD;
                break;
            }
            case S_TEXTO:
            case S_VAR: {
                int w = largo_seg(s);
                if (s->tipo == S_VAR && (eh_barra(s) || eh_grafica(s))) {
                    if (!w) w = wtot - PAD - x;
                    if (w < 4) w = 4;
                    if (j) {
                        uint32_t c = s->tem_cor ? s->cor : cor;
                        if (eh_barra(s)) widget_barra(j, x, y + (lh - s->h) / 2, w, s->h, s->val, c);
                        else {
                            int max = (s->var == V_CPUGRAPH || s->var == V_MEMGRAPH) ? 1000 : 1024;
                            for (int k = 0; k < s->nh; k++) if (s->hist[k] > max && s->var != V_CPUGRAPH && s->var != V_MEMGRAPH) max = s->hist[k];
                            widget_grafica(j, x, y + (lh - s->h) / 2, w, s->h, s->hist, s->nh, max, c);
                        }
                    }
                    x += w;
                    break;
                }
                if (j) g_texto(j, x, y + (lh - 16) / 2, s->tipo == S_TEXTO ? s->txt : s->ev, cor);
                x += w;
                break;
            }
            case S_HR: {
                int w = wtot - PAD - x;
                if (j && w > 0) g_retangulo(j, x, y + lh / 2, w, s->h, cor);
                x += w > 0 ? w : 0;
                break;
            }
            }
        }
        y += lh;
    }
    if (ancho_max) *ancho_max = anchomax;
    return y + PAD;
}

static unsigned hash_estado(void)
{
    unsigned h = 2166136261u;
    for (int i = 0; i < nseg; i++) {
        const seg_t *s = &segs[i];
        if (s->tipo != S_VAR) continue;
        for (const char *p = s->ev; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
        h = (h ^ (unsigned)s->val) * 16777619u;
        h = (h ^ (unsigned)s->nh) * 16777619u;
    }
    return h;
}

static int larg_j, alt_j;

/* El texto de cada línea tal como se pinta (una vez, al empezar): la prueba h367 lo lee del registro. */
static void registrar_linhas(void)
{
    for (int li = 0; li < nlin; li++) {
        char t[160];
        int n = 0;
        for (int i = 0; i < lin[li].qtd && n < (int)sizeof t - 50; i++) {
            const seg_t *s = &segs[lin[li].ini + i];
            if (s->tipo == S_TEXTO) n += formatar_texto(t + n, sizeof t - (size_t)n, "%s", s->txt);
            else if (s->tipo == S_VAR && (eh_barra(s) || eh_grafica(s))) n += formatar_texto(t + n, sizeof t - (size_t)n, "[%s %d]", eh_barra(s) ? "barra" : "grafica", s->val);
            else if (s->tipo == S_VAR) n += formatar_texto(t + n, sizeof t - (size_t)n, "%s", s->ev);
            if (n > (int)sizeof t - 1) n = (int)sizeof t - 1;   /* formatar_texto devuelve lo que habría escrito */
        }
        t[n] = 0;
        registrar("[CONKY] linea %d: %s\n", li, t);
    }
}

static bool tique(jan_t *j)
{
    evaluar_todo(true);
    unsigned h = hash_estado();
    if (hash_ok && h == ult_hash) return false;   /* nada cambió: no se repinta */
    if (!hash_ok) registrar_linhas();
    hash_ok = true;
    ult_hash = h;
    g_retangulo(j, 0, 0, larg_j, alt_j, WCOR_CLAVE);
    if (!transparente) {
        widget_painel(j, 0, 0, larg_j, alt_j, 9);
        if (borde) g_moldura(j, 2, 2, larg_j - 4, alt_j - 4, WCOR_ACENTO);
    }
    diseno(j, larg_j, NULL);
    return true;
}

/* Cada 5 s: si el fichero cambió, el widget se relanza (más sencillo y seguro que reordenarlo en caliente). */
static unsigned conf_visto, recarga_t;

static unsigned conf_fecha(void)
{
    datas_arquivo_t d;
    info_arquivo_t i;
    if (datas_arquivo(CONF_ARQ, &d) != 0 || info_arquivo(CONF_ARQ, &i) != 0) return 0;
    return d.modificado * 31u + i.tamanho;
}

static bool tique_recarga(jan_t *j)
{
    if (tempo_ms() - recarga_t >= 5000) {
        recarga_t = tempo_ms();
        unsigned f = conf_fecha();
        if (f != conf_visto && conf_visto != 0) {
            registrar("[CONKY] %s cambió: se recarga\n", CONF_ARQ);
            char *argv[] = { "conky", NULL };
            executar("/bin/conky", argv);
            fechar_janela(j->id);
            sair(0);
        }
    }
    return tique(j);
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    ler_conf();
    conf_visto = conf_fecha();
    recarga_t = tempo_ms();
    evaluar_todo(false);
    int anchomax;
    int alto = diseno(NULL, 100000, &anchomax);
    larg_j = anchomax > min_l ? anchomax : min_l;
    if (max_l > 0 && larg_j > max_l) larg_j = max_l;
    alt_j = alto > min_a ? alto : min_a;
    if (larg_j > 900) larg_j = 900;
    if (alt_j > 640) alt_j = 640;
    info_versao_t iv;
    int sw = 1024, sh = 768;
    if (info_sistema(INFO_VERSAO, &iv, sizeof iv) == 0 && iv.largura > 80) { sw = (int)iv.largura; sh = (int)iv.altura; }
    /* El kernel no acepta un widget más grande que la pantalla (menos la barra): se recorta. */
    if (larg_j > sw) larg_j = sw;
    if (alt_j > sh - 64) alt_j = sh - 64;
    int x = alin_h == 2 ? sw - larg_j - gap_x : alin_h == 1 ? (sw - larg_j) / 2 : gap_x;
    int y = alin_v == 2 ? sh - alt_j - gap_y : alin_v == 1 ? (sh - alt_j) / 2 : gap_y;
    if (x < 0) x = 0;
    if (y < 24) y = 24;
    jan_t *j = widget_abrir("conky", larg_j, alt_j, x, y, true, transparente ? 100 : opacidade);
    if (!j) return 1;
    registrar("[CONKY] %dx%d, %d líneas, %d elementos, ciclo %d ms\n", larg_j, alt_j, nlin, nseg, intervalo_ms);
    widget_ejecutar(j, "conky", intervalo_ms, tique_recarga, NULL);
    return 0;
}
