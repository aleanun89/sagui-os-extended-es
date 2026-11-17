// SPDX-License-Identifier: GPL-3.0-only
/* htop - monitor de procesos interactivo para el terminal (D366). Clon propio: ni una línea de htop (GPL-2.0+).
 *
 * Cabecera con barras (CPU por núcleo, memoria, swap), lista de procesos con árbol, hilos, orden por columna,
 * búsqueda, filtro, nice, matar, ratón y barra de teclas F1-F10. Solo redibuja las líneas que cambian.
 * Teclas: docs/htop.md. Configuración propia: ~/.config/htoprc. */
#include "sagui.h"
#include "contas.h"

static const char *AYUDA =
    "uso: htop [-d DECIMAS] [-u USUARIO] [-t] [-C] [-s COLUMNA]\n"
    "  Monitor de procesos interactivo (necesita un terminal). F1 muestra las teclas; F10 o q sale.\n"
    "  -d D  décimas de segundo entre actualizaciones (15 = 1,5 s)   -u USU  solo los procesos de ese usuario\n"
    "  -t  vista de árbol   -C  sin colores   -s COL  ordena por pid, usuario, ni, res, cpu, mem, tiempo u orden\n";

#define MAXP   64
#define MAXF   (2 * MAXP)
#define MAXC   256
#define MAXL   100

/* ------------------------------------------------------------ colores y rejilla */

enum { A_N, A_CAB, A_SEL, A_BARRA, A_VERDE, A_ROJO, A_AZUL, A_AMAR, A_CIAN, A_FNUM, A_FETI, A_MARCA, A_HILO, A_KERN,
       A_CABSEL, A_TIT, A_CAJA, A_CAJASEL, A_GRIS, A_N_ATTR };

typedef struct { signed char fg, bg; unsigned char rev, neg; } paleta_t;   /* fg 0-15, bg 0-7, -1 = el del terminal */
static const paleta_t PAL_COLOR[A_N_ATTR] = {
    [A_N] = { -1, -1, 0, 0 },      [A_CAB] = { 0, 2, 0, 0 },     [A_SEL] = { 0, 6, 0, 0 },    [A_BARRA] = { 8, -1, 0, 0 },
    [A_VERDE] = { 10, -1, 0, 0 },  [A_ROJO] = { 9, -1, 0, 0 },   [A_AZUL] = { 12, -1, 0, 0 }, [A_AMAR] = { 11, -1, 0, 0 },
    [A_CIAN] = { 14, -1, 0, 0 },   [A_FNUM] = { 15, 0, 0, 0 },   [A_FETI] = { 0, 6, 0, 0 },   [A_MARCA] = { 11, -1, 0, 1 },
    [A_HILO] = { 10, -1, 0, 0 },   [A_KERN] = { 12, -1, 0, 0 },  [A_CABSEL] = { 0, 6, 0, 0 }, [A_TIT] = { 14, -1, 0, 1 },
    [A_CAJA] = { 0, 7, 0, 0 },     [A_CAJASEL] = { 15, 4, 0, 0 }, [A_GRIS] = { 8, -1, 0, 0 },
};
static const paleta_t PAL_MONO[A_N_ATTR] = {
    [A_N] = { -1, -1, 0, 0 },      [A_CAB] = { -1, -1, 1, 0 },   [A_SEL] = { -1, -1, 1, 0 },  [A_BARRA] = { -1, -1, 0, 0 },
    [A_VERDE] = { -1, -1, 0, 0 },  [A_ROJO] = { -1, -1, 0, 1 },  [A_AZUL] = { -1, -1, 0, 0 }, [A_AMAR] = { -1, -1, 0, 0 },
    [A_CIAN] = { -1, -1, 0, 0 },   [A_FNUM] = { -1, -1, 0, 0 },  [A_FETI] = { -1, -1, 1, 0 }, [A_MARCA] = { -1, -1, 0, 1 },
    [A_HILO] = { -1, -1, 0, 0 },   [A_KERN] = { -1, -1, 0, 0 },  [A_CABSEL] = { -1, -1, 1, 1 }, [A_TIT] = { -1, -1, 0, 1 },
    [A_CAJA] = { -1, -1, 1, 0 },   [A_CAJASEL] = { -1, -1, 0, 1 }, [A_GRIS] = { -1, -1, 0, 0 },
};

typedef unsigned short celda_t;   /* byte Latin-1 | atributo << 8 */
static celda_t tela[MAXL][MAXC], previa[MAXL][MAXC];
static int alto = 24, ancho = 80;
static bool mono;

static void limpiar_tela(void)
{
    for (int l = 0; l < MAXL; l++) for (int c = 0; c < MAXC; c++) tela[l][c] = ' ';
}

static void poner(int l, int c, int at, const char *s)
{
    if (l < 0 || l >= alto) return;
    for (; *s && c < ancho; s++, c++) if (c >= 0) tela[l][c] = (celda_t)((unsigned char)*s | at << 8);
}

static void ponerf(int l, int c, int at, const char *fmt, ...)
{
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(b, sizeof b, fmt, ap);
    va_end(ap);
    poner(l, c, at, b);
}

static void rellenar(int l, int c0, int c1, int at)
{
    if (l < 0 || l >= alto) return;
    for (int c = c0; c < c1 && c < ancho; c++) tela[l][c] = (celda_t)(' ' | at << 8);
}

/* Escribe en el terminal solo las líneas que cambiaron desde la última vez. */
static void volcar(void)
{
    static char sal[16384];
    int n = 0;
    const paleta_t *pal = mono ? PAL_MONO : PAL_COLOR;
    for (int l = 0; l < alto; l++) {
        if (memcmp(tela[l], previa[l], (size_t)ancho * sizeof(celda_t)) == 0) continue;
        int ult = ancho - 1;
        while (ult >= 0 && tela[l][ult] == ' ') ult--;
        if (n > (int)sizeof sal - 2200) { escrever_tudo(SAIDA, sal, (unsigned)n); n = 0; }
        n += formatar_texto(sal + n, sizeof sal - (unsigned)n, "\033[%d;1H", l + 1);
        int ant = -1;
        for (int c = 0; c <= ult; c++) {
            int at = tela[l][c] >> 8;
            if (at != ant) {
                const paleta_t *p = &pal[at];
                n += formatar_texto(sal + n, sizeof sal - (unsigned)n, "\033[0%s%s", p->neg ? ";1" : "", p->rev ? ";7" : "");
                if (p->fg >= 0) n += formatar_texto(sal + n, sizeof sal - (unsigned)n, ";%d", p->fg < 8 ? 30 + p->fg : 90 + p->fg - 8);
                if (p->bg >= 0) n += formatar_texto(sal + n, sizeof sal - (unsigned)n, ";%d", 40 + p->bg);
                sal[n++] = 'm';
                ant = at;
            }
            sal[n++] = (char)(tela[l][c] & 0xFF);
        }
        n += formatar_texto(sal + n, sizeof sal - (unsigned)n, "\033[0m\033[K");
        memcpy(previa[l], tela[l], (size_t)ancho * sizeof(celda_t));
    }
    if (n) escrever_tudo(SAIDA, sal, (unsigned)n);
}

static void invalidar(void)
{
    for (int l = 0; l < MAXL; l++) for (int c = 0; c < MAXC; c++) previa[l][c] = 0xFFFF;
}

/* ------------------------------------------------------------ teclado y ratón */

enum { K_NADA = -1, K_ARRIBA = 0x1000, K_ABAJO, K_IZQ, K_DER, K_INICIO, K_FIN, K_REPAG, K_AVPAG, K_SUPR, K_ESC,
       K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_RATON, K_TAM, K_RUEDA_ARR, K_RUEDA_ABA };

static unsigned char ebuf[256];
static int en;
static int rx, ry, rb, rl, rc;   /* ratón: columna, fila (base 0), botón; tamaño: líneas y columnas */

static void ebuf_quitar(int k)
{
    memmove(ebuf, ebuf + k, (size_t)(en - k));
    en -= k;
}

static void ebuf_llenar(void)
{
    int d = disponivel(ENTRADA);
    if (d <= 0 || en >= (int)sizeof ebuf - 1) return;
    if (d > (int)sizeof ebuf - en) d = (int)sizeof ebuf - en;
    int r = ler(ENTRADA, ebuf + en, (unsigned)d);
    if (r > 0) en += r;
}

/* Una tecla del búfer (o K_NADA si no hay una completa). Entiende el formato xterm (CSI/SS3, F1-F10, ratón SGR, la
 * respuesta a CSI 18 t) y el del modo bruto de SaguiOS (ESC + letra). */
static int analizar(bool final)
{
    if (!en) return K_NADA;
    int c = ebuf[0];
    if (c != 27) { ebuf_quitar(1); return c; }
    if (en == 1) { if (final) { ebuf_quitar(1); return K_ESC; } return K_NADA; }
    int d = ebuf[1];
    if (d == 27) { ebuf_quitar(2); return K_ESC; }
    if (d == 'O') {
        if (en < 3) return final ? (ebuf_quitar(en), K_ESC) : K_NADA;
        int e = ebuf[2];
        ebuf_quitar(3);
        switch (e) {
        case 'P': return K_F1; case 'Q': return K_F2; case 'R': return K_F3; case 'S': return K_F4;
        case 'A': return K_ARRIBA; case 'B': return K_ABAJO; case 'C': return K_DER; case 'D': return K_IZQ;
        case 'H': return K_INICIO; case 'F': return K_FIN;
        default: return 0;
        }
    }
    if (d != '[') {   /* modo bruto de SaguiOS */
        ebuf_quitar(2);
        switch (d) {
        case TECLA_B_CIMA: return K_ARRIBA; case TECLA_B_BAIXO: return K_ABAJO;
        case TECLA_B_ESQUERDA: return K_IZQ; case TECLA_B_DIREITA: return K_DER;
        case TECLA_B_HOME: return K_INICIO; case TECLA_B_END: return K_FIN;
        case TECLA_B_DELETE: return K_SUPR; case TECLA_B_PGUP: return K_REPAG; case TECLA_B_PGDN: return K_AVPAG;
        default: return 0;
        }
    }
    int i = 2, v[6] = { 0 }, nv = 0;
    bool menor = false;
    if (i < en && ebuf[i] == '<') { menor = true; i++; }
    bool vacio = true;
    while (i < en && ((ebuf[i] >= '0' && ebuf[i] <= '9') || ebuf[i] == ';')) {
        if (ebuf[i] == ';') { if (nv < 5) nv++; vacio = true; }
        else { v[nv] = v[nv] * 10 + (ebuf[i] - '0'); vacio = false; }
        i++;
    }
    (void)vacio;
    if (i >= en) { if (final || en > 32) { en = 0; } return K_NADA; }
    int fin = ebuf[i];
    nv++;
    ebuf_quitar(i + 1);
    if (menor && (fin == 'M' || fin == 'm') && nv >= 3) {
        rb = v[0]; rx = v[1] - 1; ry = v[2] - 1;
        if (fin == 'm') return 0;   /* soltar: no se usa */
        if (rb == 64) return K_RUEDA_ARR;
        if (rb == 65) return K_RUEDA_ABA;
        if (rb & 32) return 0;      /* arrastre */
        return (rb & 3) == 0 ? K_RATON : 0;
    }
    switch (fin) {
    case 'A': return K_ARRIBA; case 'B': return K_ABAJO; case 'C': return K_DER; case 'D': return K_IZQ;
    case 'H': return K_INICIO; case 'F': return K_FIN;
    case 't': if (v[0] == 8 && nv >= 3 && v[1] > 0 && v[2] > 0) { rl = v[1]; rc = v[2]; return K_TAM; } return 0;
    case '~':
        switch (v[0]) {
        case 1: case 7: return K_INICIO; case 4: case 8: return K_FIN; case 3: return K_SUPR;
        case 5: return K_REPAG; case 6: return K_AVPAG;
        case 11: return K_F1; case 12: return K_F2; case 13: return K_F3; case 14: return K_F4;
        case 15: return K_F5; case 17: return K_F6; case 18: return K_F7; case 19: return K_F8;
        case 20: return K_F9; case 21: return K_F10;
        default: return 0;
        }
    default: return 0;
    }
}

/* Espera una tecla hasta ms milisegundos (K_NADA si no llegó ninguna). */
static int esperar_tecla(unsigned ms)
{
    unsigned fin = tempo_ms() + ms;
    for (;;) {
        ebuf_llenar();
        int k = analizar(false);
        if (k != K_NADA) { if (k == 0) continue; return k; }
        if (en) {   /* secuencia a medias: un momento más y se da por buena */
            dormir(15);
            ebuf_llenar();
            k = analizar(true);
            if (k != K_NADA && k != 0) return k;
            continue;
        }
        if ((int)(fin - tempo_ms()) <= 0) return K_NADA;
        dormir(25);
    }
}

/* ------------------------------------------------------------ configuración */

enum { C_PID, C_USER, C_PRI, C_NI, C_RES, C_S, C_CPU, C_MEM, C_TIME, C_CMD, C_N };
static const struct { const char *tit; int ancho; bool izq; bool asc; const char *clave; } COL[C_N] = {
    [C_PID] = { N_("PID"), 5, false, true, "pid" },       [C_USER] = { N_("USUARIO"), 8, true, true, "usuario" },
    [C_PRI] = { N_("PRI"), 3, false, true, "pri" },       [C_NI] = { N_("NI"), 3, false, true, "ni" },
    [C_RES] = { N_("RES"), 7, false, false, "res" },      [C_S] = { N_("S"), 1, true, true, "estado" },
    [C_CPU] = { N_("CPU%"), 5, false, false, "cpu" },     [C_MEM] = { N_("MEM%"), 5, false, false, "mem" },
    [C_TIME] = { N_("TIEMPO+"), 9, false, false, "tiempo" }, [C_CMD] = { N_("ORDEN"), 0, true, true, "orden" },
};

typedef struct {
    unsigned columnas;     /* bit por columna visible (C_CMD siempre) */
    int      intervalo;    /* décimas de segundo */
    int      orden;        /* columna */
    bool     inverso, arbol, hilos, sin_kernel;
    bool     mono;
} conf_t;
static conf_t cf = { .columnas = 0x3FF, .intervalo = 15, .orden = C_CPU, .inverso = false };
static bool cf_sucia;
static char ruta_conf[96];

/* La carpeta personal de quien llama: la de su cuenta (HOME del sagsh vale «/» si nadie lo fijó). */
static void carpeta_casa(char *d, size_t n)
{
    conta_t c;
    int u = usuario_op(USU_CONSULTAR, 0, 0);
    if (u >= 0 && conta_por_uid((unsigned)u & 0xFFFF, &c) && c.casa[0]) copia_texto(d, c.casa, n);
    else copia_texto(d, "/home", n);
}

static void conf_ruta(void)
{
    char casa[64];
    carpeta_casa(casa, sizeof casa);
    formatar_texto(ruta_conf, sizeof ruta_conf, "%s/.config/htoprc", casa);
}

static void conf_leer(void)
{
    conf_ruta();
    int fd = abrir(ruta_conf, ABRIR_LER);
    if (fd < 0) return;
    unsigned tam = 0;
    char *t = ler_todo_fd(fd, &tam);
    fechar(fd);
    if (!t) return;
    for (char *l = t; *l;) {
        char *fin = l;
        while (*fin && *fin != '\n') fin++;
        char sv = *fin;
        *fin = 0;
        char *ig = l;
        while (*ig && *ig != '=') ig++;
        if (*ig == '=') {
            *ig = 0;
            int v = 0;
            const char *s = ig + 1;
            bool neg = false;
            if (*s == '-') { neg = true; s++; }
            while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
            if (neg) v = -v;
            if (compara_texto(l, "columnas") == 0) cf.columnas = (unsigned)v | (1u << C_CMD);
            else if (compara_texto(l, "intervalo") == 0 && v >= 2 && v <= 600) cf.intervalo = v;
            else if (compara_texto(l, "orden") == 0 && v >= 0 && v < C_N) cf.orden = v;
            else if (compara_texto(l, "inverso") == 0) cf.inverso = v != 0;
            else if (compara_texto(l, "arbol") == 0) cf.arbol = v != 0;
            else if (compara_texto(l, "hilos") == 0) cf.hilos = v != 0;
            else if (compara_texto(l, "sin_kernel") == 0) cf.sin_kernel = v != 0;
            else if (compara_texto(l, "mono") == 0) cf.mono = v != 0;
        }
        if (!sv) break;
        l = fin + 1;
    }
    libera(t);
}

static void conf_guardar(void)
{
    if (!cf_sucia) return;
    char b[256], dir[96];
    carpeta_casa(dir, sizeof dir);
    criar_pasta(dir);   /* en un sistema sin partición de configuración la carpeta personal puede no existir aún */
    int ld = (int)tam_texto(dir);
    formatar_texto(dir + ld, sizeof dir - (unsigned)ld, "/.config");
    criar_pasta(dir);
    int n = formatar_texto(b, sizeof b, "columnas=%u\nintervalo=%d\norden=%d\ninverso=%d\narbol=%d\nhilos=%d\nsin_kernel=%d\nmono=%d\n",
                           cf.columnas, cf.intervalo, cf.orden, cf.inverso, cf.arbol, cf.hilos, cf.sin_kernel, cf.mono);
    int fd = abrir(ruta_conf, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    escrever_tudo(fd, b, (unsigned)n);
    fechar(fd);
    cf_sucia = false;
}

/* ------------------------------------------------------------ datos */

typedef struct {
    int      id;           /* pid, o tid en un hilo */
    int      pid;          /* el proceso (el del líder en un hilo) */
    int      ppid;
    bool     hilo, kernel;
    unsigned uid, estado, mem_kb, cpu_ms, nhilos;
    int      nice;
    unsigned pct;          /* décimas de % de una CPU */
    char     nome[32];
} fila_t;

static fila_t F[MAXF];
static int nF;
static int prev_id[MAXF];
static unsigned prev_ms[MAXF];
static int nprev;
static info_nucleo_t nuc[64], nuc_ant[64];
static int ncpu = 1, nuc_n_ant;
static unsigned cpu_pm[64][2];       /* por CPU: usuario y sistema en milésimas */
static info_memoria_t mem;
static unsigned t_ultimo, n_hilos_total, n_ejec;

static char nomes_uid[16][CONTA_NOME_MAX];
static unsigned uids_conocidos[16];
static int n_uids;

static const char *nombre_uid(unsigned uid)
{
    for (int i = 0; i < n_uids; i++) if (uids_conocidos[i] == uid) return nomes_uid[i];
    if (n_uids >= 16) { static char t[12]; formatar_texto(t, sizeof t, "%u", uid); return t; }
    conta_nome(uid, nomes_uid[n_uids], CONTA_NOME_MAX);
    uids_conocidos[n_uids] = uid;
    return nomes_uid[n_uids++];
}

static unsigned ms_anterior(int id, unsigned actual)
{
    for (int i = 0; i < nprev; i++) if (prev_id[i] == id) return prev_ms[i];
    return actual;
}

static void leer_datos(void)
{
    static info_processo_t pv[MAXP];
    static info_hilo_t hv[MAXP];
    int n = info_sistema(INFO_PROCESSOS, pv, MAXP);
    if (n < 0) n = 0;
    int nh = info_sistema(INFO_HILOS, hv, MAXP);
    if (nh < 0) nh = 0;
    unsigned t = tempo_ms(), dt = t - t_ultimo;
    if (!t_ultimo || !dt) dt = 0;
    t_ultimo = t;
    nF = 0;
    n_hilos_total = 0;
    n_ejec = 0;
    unsigned k_ms = 0, u_ms = 0;
    for (int i = 0; i < n && nF < MAXF; i++) {
        fila_t *f = &F[nF++];
        memset(f, 0, sizeof *f);
        f->id = f->pid = pv[i].pid;
        f->ppid = pv[i].pai;
        f->kernel = !pv[i].usuario;
        f->estado = pv[i].estado;
        f->mem_kb = pv[i].memoria_kb;
        f->cpu_ms = pv[i].cpu_ms;
        f->nhilos = pv[i].n_hilos;
        copia_texto(f->nome, pv[i].nome, sizeof f->nome);
        int u = usuario_op(USU_DE_PID, f->pid, 0);
        f->uid = u < 0 || f->kernel ? 0 : (unsigned)u & 0xFFFF;
        info_planificacion_t pi;
        f->nice = planificacion_info(f->pid, &pi) == 0 ? pi.nice : 0;
        unsigned d = f->cpu_ms - ms_anterior(f->id, f->cpu_ms);
        f->pct = dt ? d * 1000 / dt : 0;
        if (f->pid != 0) { if (f->kernel) k_ms += d; else u_ms += d; }
        n_hilos_total += f->nhilos;
        if (f->estado == 2 && f->pid != 0) n_ejec++;
    }
    for (int i = 0; i < nh && nF < MAXF; i++) {
        int j = -1;
        for (int k = 0; k < n; k++) if (F[k].pid == hv[i].pid) { j = k; break; }
        if (j < 0) continue;
        fila_t *f = &F[nF++];
        memset(f, 0, sizeof *f);
        f->id = hv[i].tid;
        f->pid = hv[i].pid;
        f->ppid = hv[i].pid;
        f->hilo = true;
        f->kernel = F[j].kernel;
        f->estado = hv[i].estado;
        f->cpu_ms = hv[i].cpu_ms;
        f->uid = F[j].uid;
        copia_texto(f->nome, F[j].nome, sizeof f->nome);
        info_planificacion_t pi;
        f->nice = planificacion_info(f->id, &pi) == 0 ? pi.nice : F[j].nice;
        unsigned d = f->cpu_ms - ms_anterior(f->id, f->cpu_ms);
        f->pct = dt ? d * 1000 / dt : 0;
    }
    nprev = nF;
    for (int i = 0; i < nF; i++) { prev_id[i] = F[i].id; prev_ms[i] = F[i].cpu_ms; }

    /* CPU por núcleo: tiques en uso / tiques; «sistema» = la parte de los procesos del kernel */
    int nc = info_sistema(INFO_NUCLEOS, nuc, 64);
    if (nc < 1) nc = 0;
    unsigned ks = k_ms + u_ms ? k_ms * 1000 / (k_ms + u_ms) : 0;
    ncpu = nc ? nc : 1;
    for (int i = 0; i < ncpu; i++) {
        unsigned uso = 0;
        if (nc) {
            unsigned dtq = nuc[i].tiques - (i < nuc_n_ant ? nuc_ant[i].tiques : nuc[i].tiques);
            unsigned dio = nuc[i].tiques_ociosos - (i < nuc_n_ant ? nuc_ant[i].tiques_ociosos : nuc[i].tiques_ociosos);
            uso = dtq ? (dtq > dio ? (dtq - dio) * 1000 / dtq : 0) : 0;
        } else {
            for (int k = 0; k < nF; k++) if (!F[k].hilo && F[k].pid) uso += F[k].pct;
            if (uso > 1000) uso = 1000;
        }
        cpu_pm[i][1] = uso * ks / 1000;
        cpu_pm[i][0] = uso - cpu_pm[i][1];
    }
    for (int i = 0; i < nc; i++) nuc_ant[i] = nuc[i];
    nuc_n_ant = nc;
    memset(&mem, 0, sizeof mem);
    info_sistema(INFO_MEMORIA, &mem, sizeof mem);
}

/* ------------------------------------------------------------ lista visible */

static int V[MAXF];        /* índices en F de las líneas visibles */
static char PR[MAXF][28];  /* prefijo de árbol de cada una */
static int nV;
static char filtro[40], busca[40];
static int filtro_uid = -1;
static int sel, tope, sel_id = -1;
static bool seguir;
static int marcados[MAXP], nmarc;

static char minus(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

static bool contiene(const char *t, const char *p)
{
    if (!*p) return true;
    for (; *t; t++) {
        const char *a = t, *b = p;
        while (*a && *b && minus(*a) == minus(*b)) { a++; b++; }
        if (!*b) return true;
    }
    return false;
}

static int cmp_fila(const fila_t *a, const fila_t *b)
{
    long r = 0;
    switch (cf.orden) {
    case C_PID: r = a->id - b->id; break;
    case C_USER: r = compara_texto(nombre_uid(a->uid), nombre_uid(b->uid)); break;
    case C_PRI: case C_NI: r = a->nice - b->nice; break;
    case C_RES: case C_MEM: r = (long)a->mem_kb - (long)b->mem_kb; break;
    case C_S: r = (long)a->estado - (long)b->estado; break;
    case C_CPU: r = (long)a->pct - (long)b->pct; break;
    case C_TIME: r = a->cpu_ms < b->cpu_ms ? -1 : a->cpu_ms > b->cpu_ms ? 1 : 0; break;
    default: r = compara_texto(a->nome, b->nome); break;
    }
    if (!COL[cf.orden].asc) r = -r;
    if (cf.inverso) r = -r;
    if (r == 0) r = a->id - b->id;
    return r < 0 ? -1 : r > 0;
}

static bool pasa_filtros(const fila_t *f)
{
    if (cf.sin_kernel && f->kernel) return false;
    if (filtro_uid >= 0 && f->uid != (unsigned)filtro_uid) return false;
    if (filtro[0] && !contiene(f->nome, filtro)) return false;
    return true;
}

static bool oks[MAXF], visto[MAXF];

static void emitir(int i, const char *pref)
{
    if (nV >= MAXF) return;
    V[nV] = i;
    copia_texto(PR[nV], pref, sizeof PR[0]);
    nV++;
}

static void ordenar(int *v, int n)
{
    for (int i = 1; i < n; i++) {
        int x = v[i], j = i;
        while (j > 0 && cmp_fila(&F[v[j - 1]], &F[x]) > 0) { v[j] = v[j - 1]; j--; }
        v[j] = x;
    }
}

/* Los hilos de un proceso, ordenados. */
static int hilos_de(int i, int *v)
{
    int n = 0;
    for (int k = 0; k < nF; k++) if (F[k].hilo && oks[k] && F[k].pid == F[i].pid) v[n++] = k;
    ordenar(v, n);
    return n;
}

static void arbol_rama(int i, const char *cont, bool ultimo, bool raiz)
{
    char pref[28], cont2[28];
    if (raiz) { pref[0] = 0; cont2[0] = 0; }
    else {
        formatar_texto(pref, sizeof pref, "%s%s", cont, ultimo ? "`- " : "|- ");
        formatar_texto(cont2, sizeof cont2, "%s%s", cont, ultimo ? "   " : "|  ");
        if (tam_texto(cont2) > 18) cont2[18] = 0;
        if (tam_texto(pref) > 21) pref[21] = 0;
    }
    visto[i] = true;
    emitir(i, pref);
    if (F[i].hilo) return;
    int hijos[MAXF], nh = hilos_de(i, hijos), np = 0, proc[MAXF];
    for (int k = 0; k < nF; k++) if (!F[k].hilo && oks[k] && !visto[k] && k != i && F[k].ppid == F[i].pid) proc[np++] = k;
    ordenar(proc, np);
    for (int k = 0; k < np; k++) hijos[nh++] = proc[k];
    for (int k = 0; k < nh; k++) if (!visto[hijos[k]]) arbol_rama(hijos[k], cont2, k == nh - 1, false);
}

static void reconstruir(void)
{
    nV = 0;
    for (int i = 0; i < nF; i++) { visto[i] = false; oks[i] = false; }
    for (int i = 0; i < nF; i++) if (!F[i].hilo) oks[i] = pasa_filtros(&F[i]);
    for (int i = 0; i < nF; i++) if (F[i].hilo) {
        oks[i] = false;
        if (!cf.hilos) continue;
        for (int k = 0; k < nF; k++) if (!F[k].hilo && F[k].pid == F[i].pid) { oks[i] = oks[k]; break; }
    }
    int proc[MAXF], np = 0;
    for (int i = 0; i < nF; i++) if (!F[i].hilo && oks[i]) proc[np++] = i;
    ordenar(proc, np);
    if (cf.arbol) {
        for (int k = 0; k < np; k++) {
            int i = proc[k];
            bool raiz = F[i].ppid == F[i].pid;
            if (!raiz) { raiz = true; for (int j = 0; j < np; j++) if (proc[j] != i && F[proc[j]].pid == F[i].ppid) { raiz = false; break; } }
            if (raiz && !visto[i]) arbol_rama(i, "", true, true);
        }
        for (int k = 0; k < np; k++) if (!visto[proc[k]]) arbol_rama(proc[k], "", true, true);   /* por si hubiera un ciclo */
    } else {
        for (int k = 0; k < np; k++) {
            int hv[MAXF];
            emitir(proc[k], "");
            int nh = hilos_de(proc[k], hv);
            for (int h = 0; h < nh; h++) emitir(hv[h], "  ");
        }
    }
    if (nV == 0) { sel = 0; tope = 0; return; }
    if (seguir && sel_id >= 0) {
        for (int k = 0; k < nV; k++) if (F[V[k]].id == sel_id) { sel = k; break; }
    }
    if (sel >= nV) sel = nV - 1;
    if (sel < 0) sel = 0;
    sel_id = F[V[sel]].id;
}

static bool esta_marcado(int pid)
{
    for (int i = 0; i < nmarc; i++) if (marcados[i] == pid) return true;
    return false;
}

static void alternar_marca(int pid)
{
    for (int i = 0; i < nmarc; i++) if (marcados[i] == pid) { marcados[i] = marcados[--nmarc]; return; }
    if (nmarc < MAXP) marcados[nmarc++] = pid;
}

/* ------------------------------------------------------------ dibujo */

static char msg[96];
static unsigned msg_hasta;
static int col_x[C_N], col_w[C_N];
static int ini_lista, filas_lista;
static int fx[10], fw[10];   /* barra de teclas: posición y ancho de F1..F10 */
static int cab_filas;

static void aviso(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(msg, sizeof msg, fmt, ap);
    va_end(ap);
    msg_hasta = tempo_ms() + 3500;
}

static void texto_kb(char *d, size_t n, unsigned kb)
{
    if (kb < 1024) formatar_texto(d, n, "%uK", kb);
    else if (kb < 100 * 1024) formatar_texto(d, n, "%u.%uM", kb / 1024, kb % 1024 * 10 / 1024);
    else if (kb < 1024 * 1024) formatar_texto(d, n, "%uM", kb / 1024);
    else formatar_texto(d, n, "%u.%02uG", kb / 1048576, kb % 1048576 * 100 / 1048576);
}

/* Una barra: etiqueta, corchetes, hasta tres tramos de colores (en milésimas) y un texto a la derecha. */
static void barra(int l, int c, int w, const char *eti, const unsigned seg[3], const int at[3], const char *der)
{
    int ne = (int)tam_texto(eti);
    poner(l, c, A_CIAN, eti);
    poner(l, c + ne, A_N, "[");
    int bw = w - ne - 2;
    if (bw < 4) return;
    poner(l, c + ne + 1 + bw, A_N, "]");
    int dl = (int)tam_texto(der);
    int pos = 0;
    for (int s = 0; s < 3; s++) {
        int k = (int)((unsigned long)seg[s] * (unsigned)bw / 1000);
        if (seg[s] && !k && pos < bw && s == 0) k = 1;
        for (int i = 0; i < k && pos < bw; i++, pos++) tela[l][c + ne + 1 + pos] = (celda_t)('|' | at[s] << 8);
    }
    for (int i = 0; i < dl && dl <= bw; i++) {
        int x = c + ne + 1 + bw - dl + i;
        int a = (tela[l][x] >> 8) == A_N ? A_BARRA : (tela[l][x] >> 8);
        tela[l][x] = (celda_t)((unsigned char)der[i] | a << 8);
    }
}

static void dibujar_cabecera(void)
{
    int cols = ncpu <= 1 ? 1 : ncpu > 16 && ancho >= 100 ? 4 : 2;
    int filas = (ncpu + cols - 1) / cols;
    int maxf = alto / 3;
    if (maxf < 1) maxf = 1;
    if (filas > maxf) filas = maxf;
    int cw = (ancho - 1) / cols;
    for (int i = 0; i < ncpu; i++) {
        int col = i / filas;
        if (col >= cols) break;
        int l = i % filas;
        char eti[8], der[12];
        formatar_texto(eti, sizeof eti, "%3d", i);
        unsigned tot = cpu_pm[i][0] + cpu_pm[i][1];
        formatar_texto(der, sizeof der, "%u.%u%%", tot / 10, tot % 10);
        unsigned seg[3] = { cpu_pm[i][0], cpu_pm[i][1], 0 };
        int at[3] = { A_VERDE, A_ROJO, A_AZUL };
        barra(l, col * cw, cw - 1, eti, seg, at, der);
    }
    int l = filas, mitad = (ancho - 1) / 2;
    /* memoria */
    {
        unsigned total = mem.gerida_kb ? mem.gerida_kb : 1;
        unsigned cache = mem.cache_kb > mem.usada_kb ? mem.usada_kb : mem.cache_kb;
        unsigned usada = mem.usada_kb - cache;
        char der[32], a[12], b[12];
        texto_kb(a, sizeof a, mem.usada_kb);
        texto_kb(b, sizeof b, mem.gerida_kb);
        formatar_texto(der, sizeof der, "%s/%s", a, b);
        unsigned seg[3] = { usada * 1000 / total, cache * 1000 / total, 0 };
        int at[3] = { A_VERDE, A_AMAR, A_AZUL };
        barra(l, 0, mitad - 1, _("Mem"), seg, at, der);
    }
    /* swap (D352: comprimida en RAM) */
    {
        unsigned teto = mem.troca_teto_kb;
        char der[32], a[12], b[12];
        texto_kb(a, sizeof a, mem.troca_kb);
        texto_kb(b, sizeof b, teto);
        if (teto) formatar_texto(der, sizeof der, "%s/%s", a, b);
        else formatar_texto(der, sizeof der, "%s", _("sin swap"));
        unsigned seg[3] = { teto ? mem.troca_kb * 1000 / teto : 0, 0, 0 };
        int at[3] = { A_ROJO, A_ROJO, A_ROJO };
        barra(l + 1, 0, mitad - 1, _("Swp"), seg, at, der);
    }
    /* tareas y tiempo encendido */
    unsigned s = tempo_ms() / 1000, np = 0;
    for (int i = 0; i < nF; i++) if (!F[i].hilo) np++;
    {
        int x = mitad + 1;
        ponerf(l, x, A_N, "%s: ", _("Tareas"));
        x += (int)tam_texto(_("Tareas")) + 2;
        char t[16];
        formatar_texto(t, sizeof t, "%u", np);
        poner(l, x, A_VERDE, t);
        x += (int)tam_texto(t);
        ponerf(l, x, A_N, _(", %u hilos; %u en ejecución"), n_hilos_total, n_ejec);
    }
    ponerf(l + 1, mitad + 1, A_N, "%s: %u:%02u:%02u", _("Tiempo encendido"), s / 3600, s / 60 % 60, s % 60);
    ponerf(l + 1, mitad + 1 + 32, A_GRIS, "%s %u,%u s", _("Intervalo"), (unsigned)cf.intervalo / 10, (unsigned)cf.intervalo % 10);
    cab_filas = l + 2;
}

static void texto_tiempo(char *d, size_t n, unsigned ms)
{
    unsigned cs = ms / 10, seg = cs / 100, m = seg / 60;
    if (m >= 6000) formatar_texto(d, n, "%uh", m / 60);
    else if (m >= 100) formatar_texto(d, n, "%uh%02um", m / 60, m % 60);
    else formatar_texto(d, n, "%u:%02u.%02u", m, seg % 60, cs % 100);
}

static void dibujar_lista(void)
{
    /* posiciones de columnas */
    int x = 0;
    for (int c = 0; c < C_N; c++) {
        col_w[c] = 0;
        if (c != C_CMD && !(cf.columnas & (1u << c))) continue;
        col_x[c] = x;
        col_w[c] = c == C_CMD ? ancho - x : COL[c].ancho;
        x += COL[c].ancho + 1;
    }
    int lc = cab_filas;
    rellenar(lc, 0, ancho, A_CAB);
    for (int c = 0; c < C_N; c++) {
        if (!col_w[c]) continue;
        const char *t = _(COL[c].tit);
        int tl = (int)tam_texto(t);
        int at = c == cf.orden ? A_CABSEL : A_CAB;
        if (c == cf.orden) rellenar(lc, col_x[c], col_x[c] + (c == C_CMD ? col_w[c] : col_w[c] + 1), at);
        int off = COL[c].izq ? 0 : col_w[c] - tl;
        if (off < 0) off = 0;
        poner(lc, col_x[c] + off, at, t);
        if (c == cf.orden) poner(lc, col_x[c] + off + tl, at, (!COL[c].asc) != cf.inverso ? "v" : "^");
    }
    ini_lista = lc + 1;
    filas_lista = alto - 1 - ini_lista;
    if (filas_lista < 1) filas_lista = 1;
    if (sel < tope) tope = sel;
    if (sel >= tope + filas_lista) tope = sel - filas_lista + 1;
    if (tope > nV - filas_lista) tope = nV - filas_lista;
    if (tope < 0) tope = 0;
    unsigned total_kb = mem.gerida_kb ? mem.gerida_kb : 1;
    for (int r = 0; r < filas_lista && tope + r < nV; r++) {
        int k = tope + r, l = ini_lista + r;
        const fila_t *f = &F[V[k]];
        bool es = k == sel;
        int base = es ? A_SEL : esta_marcado(f->id) ? A_MARCA : f->hilo ? A_HILO : f->kernel ? A_KERN : A_N;
        if (es) rellenar(l, 0, ancho, A_SEL);
        for (int c = 0; c < C_N; c++) {
            if (!col_w[c]) continue;
            char t[48];
            t[0] = 0;
            switch (c) {
            case C_PID: formatar_texto(t, sizeof t, "%d", f->id); break;
            case C_USER: formatar_texto(t, sizeof t, "%s", nombre_uid(f->uid)); break;
            case C_PRI: formatar_texto(t, sizeof t, "%d", 20 + f->nice); break;
            case C_NI: formatar_texto(t, sizeof t, "%d", f->nice); break;
            case C_RES: if (!f->hilo) texto_kb(t, sizeof t, f->mem_kb); break;
            case C_S: t[0] = f->estado == 4 ? 'Z' : f->estado == 3 ? 'S' : 'R'; t[1] = 0; break;
            case C_CPU: formatar_texto(t, sizeof t, "%u.%u", f->pct / 10, f->pct % 10); break;
            case C_MEM: if (!f->hilo) { unsigned m = (unsigned)((unsigned long long)f->mem_kb * 1000 / total_kb); formatar_texto(t, sizeof t, "%u.%u", m / 10, m % 10); } break;
            case C_TIME: texto_tiempo(t, sizeof t, f->cpu_ms); break;
            default: break;
            }
            int a = base;
            if (!es && c == C_CPU && f->pct >= 500) a = A_ROJO;
            if (c == C_CMD) {
                poner(l, col_x[c], es ? A_SEL : A_GRIS, PR[k]);
                poner(l, col_x[c] + (int)tam_texto(PR[k]), a, f->nome);
                continue;
            }
            int tl = (int)tam_texto(t);
            int off = COL[c].izq ? 0 : col_w[c] - tl;
            if (off < 0) off = 0;
            poner(l, col_x[c] + off, a, t);
        }
    }
}

static const char *const F_ETI[10] = { N_("Ayuda"), N_("Config"), N_("Buscar"), N_("Filtro"), N_("Árbol"),
                                        N_("Orden"), N_("Nice -"), N_("Nice +"), N_("Matar"), N_("Salir") };

static void dibujar_pie(void)
{
    int l = alto - 1;
    rellenar(l, 0, ancho, A_FETI);
    if (msg[0] && (int)(msg_hasta - tempo_ms()) > 0) { poner(l, 0, A_FETI, msg); return; }
    msg[0] = 0;
    int total = 0;
    for (int i = 0; i < 10; i++) total += 2 + (i > 8) + (int)tam_texto(_(F_ETI[i]));
    int hueco = total + 10 <= ancho ? 1 : 0, x = 0;
    for (int i = 0; i < 10; i++) {
        const char *e = _(F_ETI[i]);
        int el = (int)tam_texto(e);
        char n[4];
        formatar_texto(n, sizeof n, "F%d", i + 1);
        int nl = (int)tam_texto(n);
        if (x + nl + el > ancho) { fx[i] = fw[i] = 0; continue; }
        poner(l, x, A_FNUM, n);
        poner(l, x + nl, A_FETI, e);
        fx[i] = x;
        fw[i] = nl + el;
        x += nl + el + hueco;
    }
}

/* Avisos de búsqueda y filtro en el pie. */
static char pie_prompt[48];
static const char *pie_texto;

static void dibujar_todo(void)
{
    limpiar_tela();
    dibujar_cabecera();
    dibujar_lista();
    dibujar_pie();
    if (pie_texto) {
        rellenar(alto - 1, 0, ancho, A_FETI);
        ponerf(alto - 1, 0, A_FETI, "%s%s_", pie_prompt, pie_texto);
    }
}

static void fijar_tamano(int lins, int cols)
{
    if (lins < 6) lins = 6;
    if (cols < 20) cols = 20;
    if (lins > MAXL) lins = MAXL;
    if (cols > MAXC) cols = MAXC;
    if (lins != alto || cols != ancho) { alto = lins; ancho = cols; invalidar(); escrever_texto(SAIDA, "\033[2J"); }
}

/* ------------------------------------------------------------ ventanas emergentes */

static void caja(int x0, int y0, int w, int h, const char *tit)
{
    for (int y = 0; y < h; y++) rellenar(y0 + y, x0, x0 + w, A_CAJA);
    for (int x = 1; x < w - 1; x++) { poner(y0, x0 + x, A_CAJA, "-"); poner(y0 + h - 1, x0 + x, A_CAJA, "-"); }
    for (int y = 1; y < h - 1; y++) { poner(y0 + y, x0, A_CAJA, "|"); poner(y0 + y, x0 + w - 1, A_CAJA, "|"); }
    poner(y0, x0, A_CAJA, "+"); poner(y0, x0 + w - 1, A_CAJA, "+");
    poner(y0 + h - 1, x0, A_CAJA, "+"); poner(y0 + h - 1, x0 + w - 1, A_CAJA, "+");
    if (tit) ponerf(y0, x0 + 2, A_CAJA, " %s ", tit);
}

/* Lista de opciones; Intro elige, Esc cancela. Devuelve el índice o -1. "alterna": cada Intro llama a cb y sigue abierta. */
typedef void (*cb_item_t)(int i);
typedef int (*cb_textos_t)(int i, char *d, size_t n);   /* texto de la línea i */

static int emergente(const char *tit, int n, cb_textos_t texto, int sel0, cb_item_t alterna)
{
    int s = sel0 < 0 ? 0 : sel0 >= n ? n - 1 : sel0, off = 0;
    for (;;) {
        int w = (int)tam_texto(tit) + 6;
        char t[96];
        for (int i = 0; i < n; i++) { texto(i, t, sizeof t); int l = (int)tam_texto(t) + 4; if (l > w) w = l; }
        if (w > ancho - 2) w = ancho - 2;
        int h = n + 2;
        if (h > alto - 2) h = alto - 2;
        int vis = h - 2;
        if (s < off) off = s;
        if (s >= off + vis) off = s - vis + 1;
        int x0 = (ancho - w) / 2, y0 = (alto - h) / 2;
        dibujar_todo();
        caja(x0, y0, w, h, tit);
        for (int i = 0; i < vis && off + i < n; i++) {
            texto(off + i, t, sizeof t);
            int at = off + i == s ? A_CAJASEL : A_CAJA;
            rellenar(y0 + 1 + i, x0 + 1, x0 + w - 1, at);
            poner(y0 + 1 + i, x0 + 2, at, t);
        }
        volcar();
        int k = esperar_tecla(500);
        if (k == K_NADA) continue;
        if (k == K_ARRIBA) s = s ? s - 1 : n - 1;
        else if (k == K_ABAJO) s = s < n - 1 ? s + 1 : 0;
        else if (k == K_INICIO) s = 0;
        else if (k == K_FIN) s = n - 1;
        else if (k == K_RATON) {
            if (rx >= x0 && rx < x0 + w && ry > y0 && ry < y0 + h - 1) {
                s = off + ry - y0 - 1;
                if (alterna) alterna(s); else return s;
            } else return -1;
        }
        else if (k == K_RUEDA_ARR) s = s ? s - 1 : 0;
        else if (k == K_RUEDA_ABA) s = s < n - 1 ? s + 1 : s;
        else if (k == K_TAM) fijar_tamano(rl, rc);
        else if (k == '\r' || k == '\n' || (k == ' ' && alterna)) { if (alterna) alterna(s); else return s; }
        else if (k == K_ESC || k == K_F10 || k == 'q' || k == K_F2 || k == K_F1 || k == K_F9 || k == K_F6) return -1;
    }
}

static const char *emerg_lineas[40];
static int emerg_n;
static int texto_lineas(int i, char *d, size_t n) { copia_texto(d, emerg_lineas[i], n); return 0; }

static void ayuda(void)
{
    emerg_lineas[0] = _("Flechas, Re/Av Pág, Inicio, Fin: moverse por la lista");
    emerg_lineas[1] = _("F3 /: buscar      F4 \\: filtrar      Esc: quitar el filtro");
    emerg_lineas[2] = _("F5 t: árbol       H: hilos           K: ocultar los del kernel");
    emerg_lineas[3] = _("F6 > .: ordenar por una columna       I: invertir");
    emerg_lineas[4] = _("P: por CPU   M: por memoria   T: por tiempo   N: por PID");
    emerg_lineas[5] = _("F7 ]: más prioridad (nice -; solo root)  F8 [: menos (nice +)");
    emerg_lineas[6] = _("F9 k: matar (a los marcados, o al seleccionado)");
    emerg_lineas[7] = _("Espacio: marcar    U: desmarcar todos   F: seguir el proceso");
    emerg_lineas[8] = _("u: solo los procesos de un usuario");
    emerg_lineas[9] = _("F2 S: configuración (columnas, intervalo, colores)");
    emerg_lineas[10] = _("Ctrl+L: redibujar    F10 q: salir");
    emerg_lineas[11] = _("Ratón: clic en una línea, en un título de columna o en la barra de F");
    emerg_n = 12;
    emergente(_("Ayuda de htop"), emerg_n, texto_lineas, 0, NULL);
}

static int elegido_uid[16], n_elegidos;
static int texto_usuarios(int i, char *d, size_t n)
{
    if (i == 0) copia_texto(d, _("Todos los usuarios"), n);
    else formatar_texto(d, n, "%s", nombre_uid((unsigned)elegido_uid[i - 1]));
    return 0;
}

static void menu_usuario(void)
{
    n_elegidos = 0;
    for (int i = 0; i < nF; i++) {
        bool ya = false;
        for (int k = 0; k < n_elegidos; k++) if (elegido_uid[k] == (int)F[i].uid) ya = true;
        if (!ya && n_elegidos < 16) elegido_uid[n_elegidos++] = (int)F[i].uid;
    }
    int r = emergente(_("Usuario"), n_elegidos + 1, texto_usuarios, 0, NULL);
    if (r < 0) return;
    filtro_uid = r == 0 ? -1 : elegido_uid[r - 1];
    reconstruir();
}

static int texto_columnas(int i, char *d, size_t n) { copia_texto(d, _(COL[i].tit), n); return 0; }

static void menu_orden(void)
{
    int r = emergente(_("Ordenar por"), C_N, texto_columnas, cf.orden, NULL);
    if (r < 0) return;
    if (r == cf.orden) cf.inverso = !cf.inverso; else { cf.orden = r; cf.inverso = false; }
    cf_sucia = true;
    reconstruir();
}

static const char *const SENALES[] = { N_("Matar (terminar el proceso)"), N_("Cancelar") };
static int texto_senales(int i, char *d, size_t n) { copia_texto(d, _(SENALES[i]), n); return 0; }

static void alternar_config(int i)
{
    if (i < C_CMD) { cf.columnas ^= 1u << i; }
    else if (i == C_CMD) { cf.intervalo = cf.intervalo >= 50 ? 5 : cf.intervalo >= 30 ? 50 : cf.intervalo >= 20 ? 30 : cf.intervalo >= 15 ? 20 : cf.intervalo >= 10 ? 15 : cf.intervalo >= 5 ? 10 : 5; }
    else if (i == C_CMD + 1) { cf.mono = !cf.mono; mono = cf.mono; invalidar(); }
    else if (i == C_CMD + 2) cf.sin_kernel = !cf.sin_kernel;
    else if (i == C_CMD + 3) cf.arbol = !cf.arbol;
    else if (i == C_CMD + 4) cf.hilos = !cf.hilos;
    cf_sucia = true;
    reconstruir();
}

static int texto_config(int i, char *d, size_t n)
{
    if (i < C_CMD) formatar_texto(d, n, "[%c] %s %s", cf.columnas & (1u << i) ? 'x' : ' ', _("Columna"), _(COL[i].tit));
    else if (i == C_CMD) formatar_texto(d, n, "%s: %u,%u s", _("Intervalo"), (unsigned)cf.intervalo / 10, (unsigned)cf.intervalo % 10);
    else if (i == C_CMD + 1) formatar_texto(d, n, "[%c] %s", cf.mono ? 'x' : ' ', _("Sin colores"));
    else if (i == C_CMD + 2) formatar_texto(d, n, "[%c] %s", cf.sin_kernel ? 'x' : ' ', _("Ocultar los procesos del kernel"));
    else if (i == C_CMD + 3) formatar_texto(d, n, "[%c] %s", cf.arbol ? 'x' : ' ', _("Vista de árbol"));
    else formatar_texto(d, n, "[%c] %s", cf.hilos ? 'x' : ' ', _("Mostrar los hilos"));
    return 0;
}

static void menu_config(void)
{
    emergente(_("Configuración (Intro cambia, Esc cierra)"), C_CMD + 5, texto_config, 0, alternar_config);
    conf_guardar();
}

/* ------------------------------------------------------------ acciones */

static void objetivos(int *v, int *n)
{
    *n = 0;
    if (nmarc) {
        for (int i = 0; i < nF; i++) if (esta_marcado(F[i].id) && *n < MAXF) v[(*n)++] = i;
        return;
    }
    if (nV) v[(*n)++] = V[sel];
}

static void cambiar_nice(int delta)
{
    int v[MAXF], n, hechos = 0, err = 0;
    objetivos(v, &n);
    for (int i = 0; i < n; i++) {
        const fila_t *f = &F[v[i]];
        int nn = f->nice + delta;
        if (nn < -20 || nn > 19) { err = ERRO_INVALIDO; continue; }
        int r = prioridade_processo(f->id, nn);
        if (r < 0) err = r; else hechos++;
    }
    if (err && !hechos) aviso(_("Nice: %s"), nome_erro(err));
    else aviso(_("Nice cambiado en %d proceso(s)"), hechos);
}

static void matar_objetivos(void)
{
    int v[MAXF], n;
    objetivos(v, &n);
    if (!n) return;
    char t[64];
    if (n == 1) formatar_texto(t, sizeof t, "%s %d (%s)", _("Matar"), F[v[0]].pid, F[v[0]].nome);
    else formatar_texto(t, sizeof t, _("Matar %d procesos"), n);
    if (emergente(t, 2, texto_senales, 0, NULL) != 0) return;
    int hechos = 0, err = 0;
    for (int i = 0; i < n; i++) {
        int pid = F[v[i]].pid;
        if (pid == 0 || pid == meu_pid()) { err = ERRO_ACESSO; continue; }
        int r = matar(pid);
        if (r < 0) err = r; else hechos++;
        for (int k = 0; k < nmarc; k++) if (marcados[k] == F[v[i]].id) { marcados[k] = marcados[--nmarc]; break; }
    }
    if (err) aviso(_("Matar: %s"), nome_erro(err)); else aviso(_("Terminados: %d"), hechos);
    leer_datos();
    reconstruir();
}

/* Pide una línea en el pie. En el filtro y en la búsqueda se actúa al teclear. Devuelve false si se cancela con Esc. */
static bool pedir(const char *eti, char *dst, size_t max, void (*vivo)(void))
{
    char viejo[48];
    copia_texto(viejo, dst, sizeof viejo);
    copia_texto(pie_prompt, eti, sizeof pie_prompt);
    pie_texto = dst;
    bool ok = false;
    for (;;) {
        dibujar_todo();
        volcar();
        int k = esperar_tecla(500);
        size_t l = tam_texto(dst);
        if (k == K_NADA) continue;
        if (k == '\r' || k == '\n') { ok = true; break; }
        if (k == K_ESC || k == K_F10 || k == 3) { copia_texto(dst, viejo, max); if (vivo) vivo(); break; }
        if ((k == 8 || k == 127) && l) { dst[l - 1] = 0; if (vivo) vivo(); }
        else if (k >= 32 && k < 256 && l + 1 < max) { dst[l] = (char)k; dst[l + 1] = 0; if (vivo) vivo(); }
        else if (k == K_RATON) { ok = true; break; }
    }
    pie_texto = NULL;
    return ok;
}

static void ir_a_siguiente(bool desde_actual)
{
    if (!busca[0] || !nV) return;
    for (int i = desde_actual ? 0 : 1; i <= nV; i++) {
        int k = (sel + i) % nV;
        if (contiene(F[V[k]].nome, busca)) { sel = k; sel_id = F[V[k]].id; return; }
    }
    aviso(_("No se encontró «%s»"), busca);
}

static void buscar_vivo(void) { ir_a_siguiente(true); }
static void filtrar_vivo(void) { reconstruir(); }

static void ordenar_por(int c)
{
    if (cf.orden == c) cf.inverso = !cf.inverso; else { cf.orden = c; cf.inverso = false; }
    cf_sucia = true;
    reconstruir();
}

static void alternar_arbol(void) { cf.arbol = !cf.arbol; cf_sucia = true; reconstruir(); }

static void mover(int d)
{
    if (!nV) return;
    sel += d;
    if (sel < 0) sel = 0;
    if (sel >= nV) sel = nV - 1;
    sel_id = F[V[sel]].id;
}

/* Devuelve false al salir. */
static bool tecla(int k)
{
    switch (k) {
    case K_ARRIBA: mover(-1); break;
    case K_ABAJO: mover(1); break;
    case K_REPAG: mover(-(filas_lista > 1 ? filas_lista - 1 : 1)); break;
    case K_AVPAG: mover(filas_lista > 1 ? filas_lista - 1 : 1); break;
    case K_INICIO: sel = 0; mover(0); break;
    case K_FIN: sel = nV - 1; mover(0); break;
    case K_RUEDA_ARR: mover(-3); break;
    case K_RUEDA_ABA: mover(3); break;
    case K_F10: case 'q': case 3: return false;
    case K_F1: case 'h': case '?': ayuda(); break;
    case K_F2: case 'S': menu_config(); break;
    case K_F3: case '/':
        if (k == K_F3 && busca[0]) { ir_a_siguiente(false); break; }
        if (pedir(_("Buscar: "), busca, sizeof busca, buscar_vivo)) {}
        break;
    case K_F4: case '\\': pedir(_("Filtro: "), filtro, sizeof filtro, filtrar_vivo); break;
    case K_ESC: if (filtro[0] || filtro_uid >= 0) { filtro[0] = 0; filtro_uid = -1; reconstruir(); } break;
    case K_F5: case 't': alternar_arbol(); break;
    case K_F6: case '>': case '.': menu_orden(); break;
    case 'P': ordenar_por(C_CPU); break;
    case 'M': ordenar_por(C_MEM); break;
    case 'T': ordenar_por(C_TIME); break;
    case 'N': ordenar_por(C_PID); break;
    case 'I': cf.inverso = !cf.inverso; cf_sucia = true; reconstruir(); break;
    case 'H': cf.hilos = !cf.hilos; cf_sucia = true; reconstruir(); break;
    case 'K': cf.sin_kernel = !cf.sin_kernel; cf_sucia = true; reconstruir(); break;
    case 'F': seguir = !seguir; aviso(seguir ? _("Siguiendo el proceso") : _("Ya no se sigue el proceso")); break;
    case 'u': menu_usuario(); break;
    case ' ': if (nV) { alternar_marca(F[V[sel]].id); mover(1); } break;
    case 'U': nmarc = 0; break;
    case K_F7: case ']': cambiar_nice(-1); leer_datos(); reconstruir(); break;
    case K_F8: case '[': cambiar_nice(1); leer_datos(); reconstruir(); break;
    case K_F9: case 'k': matar_objetivos(); break;
    case 12: invalidar(); break;
    case K_RATON: {
        if (ry == alto - 1) {
            for (int i = 0; i < 10; i++) if (fw[i] && rx >= fx[i] && rx < fx[i] + fw[i]) return tecla(K_F1 + i);
        } else if (ry == cab_filas) {
            for (int c = 0; c < C_N; c++)
                if (col_w[c] && rx >= col_x[c] && rx < col_x[c] + (c == C_CMD ? col_w[c] : col_w[c] + 1)) { ordenar_por(c); break; }
        } else if (ry >= ini_lista && ry < ini_lista + filas_lista && tope + ry - ini_lista < nV) {
            sel = tope + ry - ini_lista;
            sel_id = F[V[sel]].id;
        }
        break;
    }
    default: break;
    }
    return true;
}

/* ------------------------------------------------------------ principal */

static bool bajo_ssh;
static bool xterm_ok;

static void tamano_inicial(void)
{
    const char *c = obter_env("COLUMNS"), *l = obter_env("LINES");
    int v;
    if (c && texto_para_numero(c, &v) && v >= 20 && v < MAXC) ancho = v;
    if (l && texto_para_numero(l, &v) && v >= 6 && v < MAXL) alto = v;
}

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    int r;
    conf_leer();
    mono = cf.mono;
    const char *usu = NULL;
    bool arbol_cl = false;
    int orden_cl = -1;
    while ((r = obter_opcao(argc, argv, "d:u:tCs:", &o)) >= 0) {
        switch (r) {
        case 'd': { int d; if (!texto_para_numero(o.valor, &d) || d < 2 || d > 600) { imprimir_em(ERROS, "htop: -d: valor no válido (2-600)\n"); return 2; } cf.intervalo = d; break; }
        case 'u': usu = o.valor; break;
        case 't': arbol_cl = true; break;
        case 'C': mono = true; break;
        case 's':
            for (int c = 0; c < C_N; c++) if (compara_texto(o.valor, COL[c].clave) == 0) orden_cl = c;
            if (orden_cl < 0) { imprimir_em(ERROS, "htop: -s: columna desconocida: %s\n", o.valor); return 2; }
            break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("htop", r, &o);
        }
    }
    if (!eh_terminal(SAIDA) || !eh_terminal(ENTRADA)) { imprimir_em(ERROS, _("htop: necesita un terminal (para listas en texto: ps o top -b)\n")); return 1; }
    if (arbol_cl) cf.arbol = true;
    if (orden_cl >= 0) cf.orden = orden_cl;
    if (usu) {
        conta_t c;
        int uid;
        if (conta_por_nome(usu, &c)) filtro_uid = (int)c.uid;
        else if (texto_para_numero(usu, &uid)) filtro_uid = uid;
        else { imprimir_em(ERROS, "htop: usuario desconocido: %s\n", usu); return 2; }
    }
    const char *tt = obter_env("TERM");
    xterm_ok = tt && comeca_com(tt, "xterm");
    bajo_ssh = obter_env("SSH_CLIENT") != NULL;
    tamano_inicial();
    modo_terminal(TERM_BRUTO);
    if (xterm_ok) {
        /* teclas F1-F10 y ratón: el Terminal pasa a modo xterm (CSI ? 7700); bajo ssh ya lo está */
        if (!bajo_ssh) escrever_texto(SAIDA, "\033[?7700h");
        escrever_texto(SAIDA, "\033[?1049h\033[?25l\033[?1000h\033[?1006h");
    } else escrever_texto(SAIDA, "\033[?25l");
    escrever_texto(SAIDA, "\033[2J");
    if (xterm_ok && !bajo_ssh) {   /* el tamaño real del Terminal, antes de la primera imagen */
        escrever_texto(SAIDA, "\033[18t");
        for (unsigned fin = tempo_ms() + 400; (int)(fin - tempo_ms()) > 0;) {
            int k = esperar_tecla(50);
            if (k == K_TAM) { fijar_tamano(rl, rc); break; }
        }
    }
    limpiar_tela();
    invalidar();
    leer_datos();
    dormir(300);
    leer_datos();
    reconstruir();
    unsigned prox = tempo_ms() + (unsigned)cf.intervalo * 100;
    bool seguir_ = true;
    unsigned pregunta = 0;
    while (seguir_) {
        if (xterm_ok && (int)(tempo_ms() - pregunta) > 1500) { escrever_texto(SAIDA, "\033[18t"); pregunta = tempo_ms(); }
        dibujar_todo();
        volcar();
        int k = esperar_tecla(100);
        if (k == K_TAM) fijar_tamano(rl, rc);
        else if (k != K_NADA) {
            if (!tecla(k)) seguir_ = false;
            if (nV == 0 && !cf.arbol) { /* nada que mostrar */ }
        }
        if ((int)(tempo_ms() - prox) >= 0) {
            leer_datos();
            reconstruir();
            prox = tempo_ms() + (unsigned)cf.intervalo * 100;
        }
    }
    conf_guardar();
    if (xterm_ok) {
        escrever_texto(SAIDA, "\033[?1006l\033[?1000l\033[?25h\033[?1049l");
        if (!bajo_ssh) escrever_texto(SAIDA, "\033[?7700l");
    } else escrever_texto(SAIDA, "\033[0m\033[2J\033[H\033[?25h");
    modo_terminal(TERM_LINHA);
    return 0;
}
