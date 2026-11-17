// SPDX-License-Identifier: GPL-3.0-only
/* widget.c - lo común de los widgets de escritorio (D365): configuración, ventana, bucle y datos del
 * sistema. Ver userland/inc/widget.h. Sin coma flotante (la libsagui va sin SSE/x87). */
#include "widget.h"

static inline int chamar4(unsigned n, unsigned a, unsigned b, unsigned c, unsigned d)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d) : "memory");
    return r;
}

const widget_def_t widget_lista[] = {
    { "relogio",    N_("Reloj"),               N_("Reloj analógico o digital"),                        "/bin/widget-relogio" },
    { "calendario", N_("Calendario"),          N_("El mes en curso, con el día de hoy marcado"),       "/bin/widget-calendario" },
    { "notas",      N_("Notas adhesivas"),     N_("Una nota de texto que se queda en el escritorio"),  "/bin/widget-notas" },
    { "sistema",    N_("Monitor del sistema"), N_("CPU, memoria y red, con gráficas"),                 "/bin/widget-sistema" },
    { "conky",      N_("Conky"),               N_("Texto con datos del sistema (~/.config/conky.conf)"), "/bin/conky" },
};
const int widget_qtd = (int)(sizeof widget_lista / sizeof widget_lista[0]);

/* ------------------------------------------------------------ configuración */

#define CONF_MAX 4096

static int conf_ler_tudo(char *b)
{
    int fd = abrir(WIDGET_CONF, ABRIR_LER);
    if (fd < 0) return 0;
    int n = ler(fd, b, CONF_MAX - 1);
    fechar(fd);
    if (n < 0) n = 0;
    b[n] = 0;
    return n;
}

/* Busca la línea «chave=»: devuelve su valor (sin el salto de línea) o NULL; l recibe la longitud del valor. */
static const char *conf_achar(const char *b, const char *chave, int *l, const char **inicio_linha)
{
    size_t kn = tam_texto(chave);
    for (const char *p = b; *p;) {
        const char *fim = p;
        while (*fim && *fim != '\n') fim++;
        if ((size_t)(fim - p) > kn && compara_texto_n(p, chave, kn) == 0 && p[kn] == '=') {
            if (l) *l = (int)(fim - (p + kn + 1));
            if (inicio_linha) *inicio_linha = p;
            return p + kn + 1;
        }
        p = *fim ? fim + 1 : fim;
    }
    return NULL;
}

int widget_ler(const char *chave, char *dst, int n)
{
    static char b[CONF_MAX];
    conf_ler_tudo(b);
    int l;
    const char *v = conf_achar(b, chave, &l, NULL);
    if (!v) { if (n > 0) dst[0] = 0; return -1; }
    if (l > n - 1) l = n - 1;
    for (int i = 0; i < l; i++) dst[i] = v[i];
    dst[l] = 0;
    return 0;
}

int widget_gravar(const char *chave, const char *valor)
{
    static char b[CONF_MAX], novo[CONF_MAX];
    int n = conf_ler_tudo(b);
    int l;
    const char *ini;
    const char *v = conf_achar(b, chave, &l, &ini);
    int k = 0;
    if (n == 0) k += formatar_texto(novo + k, CONF_MAX - (size_t)k, "# Widgets de escritorio de SaguiOS (D365)\n");
    if (v) {
        const char *depois = v + l;
        if (*depois == '\n') depois++;
        int antes = (int)(ini - b);
        for (int i = 0; i < antes && k < CONF_MAX - 1; i++) novo[k++] = b[i];
        if (valor) k += formatar_texto(novo + k, CONF_MAX - (size_t)k, "%s=%s\n", chave, valor);
        while (*depois && k < CONF_MAX - 1) novo[k++] = *depois++;
    } else {
        if (!valor) return 0;
        for (int i = 0; i < n && k < CONF_MAX - 1; i++) novo[k++] = b[i];
        if (k > 0 && novo[k - 1] != '\n') novo[k++] = '\n';
        k += formatar_texto(novo + k, CONF_MAX - (size_t)k, "%s=%s\n", chave, valor);
    }
    if (k >= CONF_MAX) k = CONF_MAX - 1;
    criar_pasta("/home/.config");
    int fd = abrir(WIDGET_CONF, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int r = escrever(fd, novo, (unsigned)k);
    fechar(fd);
    return r < 0 ? r : 0;
}

int widget_ler_int(const char *chave, int padrao)
{
    char t[24];
    if (widget_ler(chave, t, sizeof t) < 0) return padrao;
    const char *p = t;
    bool neg = false;
    if (*p == '-') { neg = true; p++; }
    if (*p < '0' || *p > '9') return padrao;
    int v = 0;
    while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
    return neg ? -v : v;
}

int widget_gravar_int(const char *chave, int valor)
{
    char t[24];
    formatar_texto(t, sizeof t, "%d", valor);
    return widget_gravar(chave, t);
}

bool widget_ativo(const char *widget)
{
    return widget_ler_int(widget, 0) == 1;
}

void widget_ativar(const char *widget, bool ligar)
{
    widget_gravar(widget, ligar ? "1" : "0");
}

/* ------------------------------------------------------------ ventana y bucle */

static void chave_de(char *dst, int n, const char *widget, const char *sufixo)
{
    formatar_texto(dst, (size_t)n, "%s.%s", widget, sufixo);
}

jan_t *widget_abrir(const char *widget, int l, int a, int x_def, int y_def, bool clave, int opacidade_def)
{
    char k[48];
    chave_de(k, sizeof k, widget, "opacidad");
    int op = widget_ler_int(k, opacidade_def);
    if (op < 10) op = 10;
    if (op > 100) op = 100;
    jan_t *j = aloca(sizeof(jan_t));
    if (!j) return NULL;
    j->id = chamar4(SIS_CRIAR_WIDGET, (unsigned)widget, (unsigned)(l | (a << 16)), (unsigned)&j->px,
                    (unsigned)(op | (clave ? WIDGET_CLAVE : 0)));
    if (j->id < 0) { libera(j); return NULL; }
    j->larg = l; j->alt = a; j->estilo = 0; j->min_l = j->min_a = 0;
    g_retangulo(j, 0, 0, l, a, clave ? WCOR_CLAVE : WCOR_PAINEL);
    int x = x_def, y = y_def;
    chave_de(k, sizeof k, widget, "x");
    int sx = widget_ler_int(k, -99999);
    chave_de(k, sizeof k, widget, "y");
    int sy = widget_ler_int(k, -99999);
    if (sx != -99999 && sy != -99999 && sx >= 0 && sy >= 0) { x = sx; y = sy; }
    mover_janela(j->id, x, y);
    return j;
}

void widget_ejecutar(jan_t *j, const char *widget, int ms, bool (*tique)(jan_t *), bool (*evento)(jan_t *, const evento_t *))
{
    if (ms < 100) ms = 100;
    if (tique) tique(j);
    jan_mostrar(j);
    unsigned prox = tempo_ms() + (unsigned)ms;
    for (;;) {
        int espera = (int)(prox - tempo_ms());
        if (espera < 0) espera = 0;
        evento_t e;
        bool redesenhar = false;
        if (esperar_evento(j->id, &e, espera) == 1) {
            if (e.tipo == EV_FECHAR) break;
            if (e.tipo == EV_WIDGET) {
                if (e.tecla == EVW_FIN_SESION) break;
                if (e.tecla == EVW_QUITAR) { widget_ativar(widget, false); break; }
                if (e.tecla == EVW_MOVIDO) {
                    char k[48];
                    chave_de(k, sizeof k, widget, "x");
                    widget_gravar_int(k, e.x);
                    chave_de(k, sizeof k, widget, "y");
                    widget_gravar_int(k, e.y);
                }
            } else if (evento && evento(j, &e)) redesenhar = true;
        }
        if ((int)(tempo_ms() - prox) >= 0) {
            prox = tempo_ms() + (unsigned)ms;
            if (!tique || tique(j)) redesenhar = true;
        }
        if (redesenhar) jan_mostrar(j);
    }
    fechar_janela(j->id);
}

/* ------------------------------------------------------------ dibujo */

static int raiz(int n)
{
    int r = 0;
    while ((r + 1) * (r + 1) <= n) r++;
    return r;
}

static uint32_t mezcla(uint32_t a, uint32_t b, int num, int den)
{
    uint32_t r = 0;
    for (int s = 0; s <= 16; s += 8) {
        int ca = (int)((a >> s) & 255), cb = (int)((b >> s) & 255);
        r |= (uint32_t)(ca + (cb - ca) * num / den) << s;
    }
    return r;
}

/* Rectángulo de esquinas redondeadas con degradado vertical c0 -> c1; fuera de las esquinas no pinta. */
static void forma(jan_t *j, int x, int y, int l, int a, int r, uint32_t c0, uint32_t c1)
{
    for (int i = 0; i < a; i++) {
        int corte = 0;
        int d = i < r ? r - i : (i >= a - r ? i - (a - r - 1) : 0);   /* distancia vertical a la curva */
        if (d > 0) {
            int w = raiz(4 * r * r - (2 * d - 1) * (2 * d - 1));
            corte = r - (w + 1) / 2;
        }
        g_retangulo(j, x + corte, y + i, l - 2 * corte, 1, a > 1 ? mezcla(c0, c1, i, a - 1) : c0);
    }
}

void widget_painel(jan_t *j, int x, int y, int l, int a, int r)
{
    forma(j, x, y, l, a, r, WCOR_BORDA, WCOR_BORDA);
    forma(j, x + 1, y + 1, l - 2, a - 2, r > 1 ? r - 1 : 0, WCOR_PAINEL2, WCOR_PAINEL);
}

void widget_texto(jan_t *j, int x, int y, const char *t, uint32_t cor, int escala)
{
    extern const unsigned char fonte_sagui[256][16];
    if (escala <= 1) { g_texto(j, x, y, t, cor); return; }
    for (int k = 0; t[k]; k++, x += LETRA_L * escala) {
        const unsigned char *g = fonte_sagui[(uint8_t)t[k]];
        for (int lin = 0; lin < 16; lin++)
            for (int col = 0; col < 8; col++)
                if (g[lin] & (0x80 >> col)) g_retangulo(j, x + col * escala, y + lin * escala, escala, escala, cor);
    }
}

void widget_barra(jan_t *j, int x, int y, int l, int a, int pct, uint32_t cor)
{
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    g_retangulo(j, x, y, l, a, 0x0C150F);
    g_moldura(j, x, y, l, a, WCOR_BORDA);
    int w = (l - 2) * pct / 100;
    if (w > 0) g_retangulo(j, x + 1, y + 1, w, a - 2, cor);
}

void widget_grafica(jan_t *j, int x, int y, int l, int a, const int *v, int n, int max, uint32_t cor)
{
    g_retangulo(j, x, y, l, a, 0x0C150F);
    g_moldura(j, x, y, l, a, WCOR_BORDA);
    if (max < 1) max = 1;
    int cols = l - 2;
    for (int c = 0; c < cols; c++) {
        int idx = n - cols + c;           /* una columna por muestra, la más nueva a la derecha */
        if (idx < 0 || idx >= n) continue;
        int h = (int)((long long)v[idx] * (a - 2) / max);
        if (h > a - 2) h = a - 2;
        if (h > 0) g_retangulo(j, x + 1 + c, y + a - 1 - h, 1, h, cor);
    }
}

void widget_numero(char *dst, int n, unsigned x10)
{
    formatar_texto(dst, (size_t)n, "%u,%u", x10 / 10, x10 % 10);
}

void widget_bytes(char *dst, int n, unsigned long long b)
{
    static const char *const u[] = { "B", "KiB", "MiB", "GiB", "TiB" };
    unsigned long long v = b, resto = 0;
    int i = 0;
    while (v >= 1024 && i < 4) { resto = v % 1024; v /= 1024; i++; }
    if (i == 0) formatar_texto(dst, (size_t)n, "%u%s", (unsigned)v, u[0]);
    else if (v >= 100) formatar_texto(dst, (size_t)n, "%u%s", (unsigned)v, u[i]);
    else formatar_texto(dst, (size_t)n, "%u.%u%s", (unsigned)v, (unsigned)(resto * 10 / 1024), u[i]);
}

void widget_kb(char *dst, int n, unsigned kb) { widget_bytes(dst, n, (unsigned long long)kb * 1024); }

static const short tabla_seno[91] = { 0, 17, 35, 52, 70, 87, 105, 122, 139, 156, 174, 191, 208, 225, 242, 259, 276, 292, 309, 326, 342, 358, 375, 391, 407, 423, 438, 454, 469, 485, 500, 515, 530, 545, 559, 574, 588, 602, 616, 629, 643, 656, 669, 682, 695, 707, 719, 731, 743, 755, 766, 777, 788, 799, 809, 819, 829, 839, 848, 857, 866, 875, 883, 891, 899, 906, 914, 921, 927, 934, 940, 946, 951, 956, 961, 966, 970, 974, 978, 982, 985, 988, 990, 993, 995, 996, 998, 999, 999, 1000, 1000 };

int widget_seno(int g)
{
    g %= 360;
    if (g < 0) g += 360;
    if (g <= 90) return tabla_seno[g];
    if (g <= 180) return tabla_seno[180 - g];
    if (g <= 270) return -tabla_seno[g - 180];
    return -tabla_seno[360 - g];
}

/* ------------------------------------------------------------ datos del sistema */

static unsigned geracao = 1;
void widget_datos_tique(void) { geracao++; }

static unsigned cpu_ger, cpu_x10_v;
static unsigned long long cpu_tot_ant, cpu_oc_ant;
static int ncpu_v = 1;

static void cpu_ler(void)
{
    if (cpu_ger == geracao) return;
    cpu_ger = geracao;
    static info_nucleo_t nuc[32];
    int n = info_sistema(INFO_NUCLEOS, nuc, 32);
    if (n < 1) return;
    if (n > 32) n = 32;
    ncpu_v = n;
    unsigned long long tot = 0, oc = 0;
    for (int i = 0; i < n; i++) { tot += nuc[i].tiques; oc += nuc[i].tiques_ociosos; }
    if (cpu_tot_ant && tot > cpu_tot_ant) {
        unsigned long long dt = tot - cpu_tot_ant, doc = oc - cpu_oc_ant;
        if (doc > dt) doc = dt;
        cpu_x10_v = (unsigned)((dt - doc) * 1000 / dt);
    }
    cpu_tot_ant = tot; cpu_oc_ant = oc;
}
unsigned widget_cpu_x10(void) { cpu_ler(); return cpu_x10_v; }
int widget_ncpu(void) { cpu_ler(); return ncpu_v; }

static unsigned mem_ger;
static info_memoria_t mem_v;
static void mem_ler(void)
{
    if (mem_ger == geracao) return;
    mem_ger = geracao;
    memset(&mem_v, 0, sizeof mem_v);
    info_sistema(INFO_MEMORIA, &mem_v, sizeof mem_v);
}
void widget_mem(unsigned *usada, unsigned *total)
{
    mem_ler();
    unsigned u = mem_v.usada_kb;
    if (mem_v.cache_kb < u) u = u - mem_v.cache_kb;   /* la caché de disco se devuelve cuando hace falta */
    if (usada) *usada = u;
    if (total) *total = mem_v.gerida_kb;
}
void widget_swap(unsigned *usada, unsigned *total)
{
    mem_ler();
    if (usada) *usada = mem_v.troca_kb;
    if (total) *total = mem_v.troca_teto_kb;
}

static unsigned rede_ger;
static int rede_rx[TRAFEGO_AMOSTRAS], rede_tx[TRAFEGO_AMOSTRAS], rede_n;
void widget_rede(unsigned *rx, unsigned *tx, const int **hrx, const int **htx, int *n)
{
    if (rede_ger != geracao) {
        rede_ger = geracao;
        static info_trafego_t t;
        memset(&t, 0, sizeof t);
        rede_n = 0;
        if (info_sistema(INFO_TRAFEGO, &t, sizeof t) == 0) {
            rede_n = (int)(t.amostras > TRAFEGO_AMOSTRAS ? TRAFEGO_AMOSTRAS : t.amostras);
            for (int i = 0; i < rede_n; i++) {
                rede_rx[i] = (int)t.rx[TRAFEGO_AMOSTRAS - rede_n + i];
                rede_tx[i] = (int)t.tx[TRAFEGO_AMOSTRAS - rede_n + i];
            }
        }
    }
    if (rx) *rx = rede_n ? (unsigned)rede_rx[rede_n - 1] : 0;
    if (tx) *tx = rede_n ? (unsigned)rede_tx[rede_n - 1] : 0;
    if (hrx) *hrx = rede_rx;
    if (htx) *htx = rede_tx;
    if (n) *n = rede_n;
}

int widget_disco(const char *ponto, unsigned *usado, unsigned *total)
{
    static info_montagem_t m[16];
    static unsigned ger, lido;
    static int qtd;
    if (ger != geracao) {
        ger = geracao;
        qtd = info_sistema(INFO_MONTAGENS, m, 16);
        if (qtd > 16) qtd = 16;
        lido = 1;
    }
    (void)lido;
    for (int i = 0; i < qtd; i++)
        if (compara_texto(m[i].ponto, ponto) == 0) {
            if (total) *total = m[i].total_kb;
            if (usado) *usado = m[i].total_kb - m[i].livre_kb;
            return 0;
        }
    if (compara_texto(ponto, "/") == 0) {   /* la raíz es el ramfs: no figura en las montagens (lo mismo hace df) */
        mem_ler();
        if (total) *total = mem_v.gerida_kb;
        if (usado) *usado = mem_v.gerida_kb - mem_v.livre_kb;
        return 0;
    }
    return -1;
}

int widget_temperatura(void)
{
    static info_sensores_t s;
    static unsigned ger;
    static int v = -1;
    if (ger != geracao) {
        ger = geracao;
        v = -1;
        memset(&s, 0, sizeof s);
        if (info_sistema(INFO_SENSORES, &s, sizeof s) == 0 && s.qtd_temp > 0) v = s.temp[0].mili_c / 100;
    }
    return v;
}

int widget_bateria(void)
{
    static info_bateria_t b;
    static unsigned ger;
    static int v = -1;
    if (ger != geracao) {
        ger = geracao;
        v = -1;
        memset(&b, 0, sizeof b);
        if (info_sistema(INFO_BATERIA, &b, sizeof b) == 0 && b.porcentagem >= 0) v = b.porcentagem;
    }
    return v;
}

unsigned widget_segundos_ligado(void)
{
    info_versao_t iv;
    if (info_sistema(INFO_VERSAO, &iv, sizeof iv) == 0) return iv.segundos_ligado;
    return tempo_ms() / 1000;
}

const char *widget_nome_host(void)
{
    static char h[64];
    if (h[0]) return h;
    const char *e = obter_env("HOSTNAME");
    if (e && *e) { copia_texto(h, e, sizeof h); return h; }
    int fd = abrir("/etc/hostname", ABRIR_LER);
    if (fd >= 0) {
        int n = ler(fd, h, sizeof h - 1);
        fechar(fd);
        h[n > 0 ? n : 0] = 0;
        for (int i = 0; h[i]; i++) if (h[i] == '\n' || h[i] == '\r' || h[i] == ' ') { h[i] = 0; break; }
    }
    if (!h[0]) copia_texto(h, "sagui", sizeof h);
    return h;
}

const char *widget_versao(void)
{
    static char v[40];
    if (v[0]) return v;
    info_versao_t iv;
    if (info_sistema(INFO_VERSAO, &iv, sizeof iv) == 0) copia_texto(v, iv.versao, sizeof v);
    else copia_texto(v, "?", sizeof v);
    return v;
}

/* Los procesos que más gastan: dos muestras de INFO_PROCESSOS (la de este ciclo y la anterior). */
#define TOP_MAX 64
typedef struct { int pid; unsigned cpu_ms; } top_ant_t;
static unsigned top_ger, top_t_ant;
static info_processo_t top_v[TOP_MAX];
static top_ant_t top_ant[TOP_MAX];
static int top_n, top_nant, top_ord_cpu[TOP_MAX], top_ord_mem[TOP_MAX];
static unsigned top_pct[TOP_MAX];

static void top_ler(void)
{
    if (top_ger == geracao) return;
    top_ger = geracao;
    int n = info_sistema(INFO_PROCESSOS, top_v, TOP_MAX);
    if (n < 0) n = 0;
    unsigned t = tempo_ms(), dt = top_t_ant && t > top_t_ant ? t - top_t_ant : 0;
    top_n = n;
    for (int i = 0; i < n; i++) {
        unsigned ant = top_v[i].cpu_ms;
        for (int k = 0; k < top_nant; k++) if (top_ant[k].pid == top_v[i].pid) { ant = top_ant[k].cpu_ms; break; }
        top_pct[i] = dt ? (top_v[i].cpu_ms - ant) * 1000 / dt : 0;
        top_ord_cpu[i] = top_ord_mem[i] = i;
    }
    for (int i = 1; i < n; i++) {   /* inserción: pocos procesos */
        int x = top_ord_cpu[i], k = i;
        while (k > 0 && top_pct[top_ord_cpu[k - 1]] < top_pct[x]) { top_ord_cpu[k] = top_ord_cpu[k - 1]; k--; }
        top_ord_cpu[k] = x;
        x = top_ord_mem[i]; k = i;
        while (k > 0 && top_v[top_ord_mem[k - 1]].memoria_kb < top_v[x].memoria_kb) { top_ord_mem[k] = top_ord_mem[k - 1]; k--; }
        top_ord_mem[k] = x;
    }
    for (int i = 0; i < n; i++) { top_ant[i].pid = top_v[i].pid; top_ant[i].cpu_ms = top_v[i].cpu_ms; }
    top_nant = n;
    top_t_ant = t;
}

int widget_top(int i, bool por_mem, char *nome, int n, unsigned *cpu, unsigned *mem, int *pid)
{
    top_ler();
    /* el proceso 0 (ocioso) no cuenta */
    int visto = -1;
    for (int k = 0; k < top_n; k++) {
        int idx = por_mem ? top_ord_mem[k] : top_ord_cpu[k];
        if (top_v[idx].pid == 0) continue;
        if (++visto == i) {
            if (nome) copia_texto(nome, top_v[idx].nome, (size_t)n);
            if (cpu) *cpu = top_pct[idx];
            if (mem) *mem = top_v[idx].memoria_kb;
            if (pid) *pid = top_v[idx].pid;
            return 0;
        }
    }
    if (nome && n > 0) nome[0] = 0;
    return -1;
}
