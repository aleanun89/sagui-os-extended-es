// SPDX-License-Identifier: GPL-3.0-only
/* ls (listar) - lista archivos y carpetas (D097; D385 lo pone en columnas).
 *
 * En el terminal: los nombres en columnas al ancho de la ventana (COLUMNS, que sagsh pone al día
 * en cada prompt), de arriba abajo como el ls de Unix, con las carpetas primero y en color por tipo.
 * -l: permisos, dueño, grupo, fecha, tamaño y nombre, en columnas alineadas. -s: el formato de antes
 * de SaguiOS (nombre, tamaño en bytes y «programa»), uno por línea. Si la salida va a un pipe o a un
 * archivo (o con -1), solo los nombres, uno por línea y sin agrupar: así "ls | grep x" y
 * "ls | wc -l" funcionan como en Unix. El orden es alfabético sin distinguir mayúsculas ni acentos
 * (salvo -t o -S). */
#include "sagui.h"
#include "contas.h"

typedef struct {
    entrada_pasta_t e;
    uint32_t mt;
    char perm[11], dono[CONTA_NOME_MAX], grupo[CONTA_NOME_MAX], tam[16];
} item_t;

static bool longo, ocultos, humano, recursivo, uno, por_tempo, por_tamanho, inverso, so_pasta, tty, cor, classico;
static bool agrupar = true;
static int cuantos_args, largura = 80;

static const char *AYUDA =
    "uso: ls [-1ahlRrSst] [-d] [archivo|carpeta...]\n"
    "  -l  formato largo: permisos, dueño y grupo, fecha de modificación, tamaño y nombre\n"
    "  -a  incluye los nombres que empiezan por punto\n"
    "  -h  tamaños legibles (1.5K, 3M) con -l\n"
    "  -R  recursivo: entra en las subcarpetas\n"
    "  -1  un nombre por línea (por defecto si la salida no es el terminal)\n"
    "  -s  el formato clásico de SaguiOS: nombre, tamaño en bytes y «programa», uno por línea\n"
    "  -t  ordena por fecha de modificación (la más nueva primero)\n"
    "  -S  ordena por tamaño (el mayor primero)\n"
    "  -r  invierte el orden\n"
    "  -d  muestra la carpeta en sí y no su contenido\n"
    "  --sin-agrupar  en el terminal, no pone las carpetas primero\n"
    "  --color[=cuándo]  colores (always, never, auto): por defecto, solo en el terminal con colores\n"
    "En el terminal, los nombres van en columnas al ancho de la ventana (variable COLUMNS).\n";

/* Colores (D263): carpetas en azul, programas en verde, imágenes, sonido y vídeo en magenta,
 * comprimidos en rojo. Solo con --color o en un terminal con TERM; nunca en tuberías. (SaguiOS no
 * tiene enlaces simbólicos; si llegan, irán en cian.) */
static bool termina_en(const char *n, const char *ext)
{
    size_t a = tam_texto(n), b = tam_texto(ext);
    if (a <= b) return false;
    for (size_t i = 0; i < b; i++) {
        char c = n[a - b + i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (c != ext[i]) return false;
    }
    return true;
}

static const char *cor_de(const char *nome, unsigned tipo)
{
    if (!cor) return "";
    if (tipo == TIPO_PASTA) return "\x1b[1;34m";
    if (tipo == TIPO_PROGRAMA) return "\x1b[1;32m";
    static const char *const img[] = { ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".qoi", ".ppm", ".pgm", ".tga", ".webp",
                                       ".svg", ".ico", ".wav", ".mp3", ".ogg", ".flac", ".opus", ".m4a", ".aac", ".mid",
                                       ".kar", ".mod", ".mpg", ".mpeg", ".mp4", ".avi", ".mkv", ".webm", NULL };
    static const char *const zip[] = { ".zip", ".tar", ".gz", ".tgz", ".bz2", ".xz", ".7z", ".rar", ".iso", ".zst", ".lz",
                                       ".lzma", ".cab", ".deb", ".rpm", ".jar", NULL };
    for (int i = 0; img[i]; i++) if (termina_en(nome, img[i])) return "\x1b[35m";
    for (int i = 0; zip[i]; i++) if (termina_en(nome, zip[i])) return "\x1b[31m";
    return "";
}

/* El nombre con su color y el relleno hasta "ancho" columnas (el relleno va fuera del color). */
static void nome_pintado(char *dst, size_t max, const char *nome, unsigned tipo, const char *sufixo, int ancho)
{
    const char *c = cor_de(nome, tipo);
    int n = (int)tam_texto(nome) + (int)tam_texto(sufixo);
    formatar_texto(dst, max, "%s%s%s%s%*s", c, nome, sufixo, *c ? "\x1b[0m" : "", n < ancho ? ancho - n : 0, "");
}

static uint32_t mtime_de(const char *caminho)
{
    datas_arquivo_t d;
    return datas_arquivo(caminho, &d) == 0 ? d.modificado : 0;
}

/* Letra para ordenar: minúscula y sin acento (Latin-1), para que «Árbol» vaya con la «a». */
static int letra_ordem(unsigned char c)
{
    static const char base[64] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
    if (c >= 0xC0) c = (unsigned char)base[c - 0xC0];
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
    return c;
}

static int cmp_texto(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = letra_ordem((unsigned char)*a), y = letra_ordem((unsigned char)*b);
        if (x != y) return x - y;
        if (!*a) break;
    }
    return compara_texto(a, b);   /* iguales sin mayúsculas: el orden de siempre desempata */
}

static int cmp_nome(const void *a, const void *b)
{
    int r = cmp_texto(((const item_t *)a)->e.nome, ((const item_t *)b)->e.nome);
    return inverso ? -r : r;
}
static int cmp_tempo(const void *a, const void *b)
{
    const item_t *x = a, *y = b;
    if (x->mt != y->mt) return inverso ? (x->mt > y->mt ? 1 : -1) : (x->mt < y->mt ? 1 : -1);
    return cmp_texto(x->e.nome, y->e.nome);
}
static int cmp_tamanho(const void *a, const void *b)
{
    const item_t *x = a, *y = b;
    if (x->e.tamanho != y->e.tamanho) return inverso ? (x->e.tamanho > y->e.tamanho ? 1 : -1) : (x->e.tamanho < y->e.tamanho ? 1 : -1);
    return cmp_texto(x->e.nome, y->e.nome);
}
/* Carpetas primero (solo en el terminal; las tuberías ven el orden puro). Es estable: dentro de cada
 * grupo queda el orden de antes. */
static int cmp_grupo(const void *a, const void *b)
{
    bool x = ((const item_t *)a)->e.tipo == TIPO_PASTA, y = ((const item_t *)b)->e.tipo == TIPO_PASTA;
    return x == y ? 0 : x ? -1 : 1;
}

/* D282: el nombre de un uid o gid, recordando los últimos (leer /etc/usuarios por
 * cada archivo sería lento). Un gid se nombra por la cuenta con ese número. */
static const char *nome_de(unsigned id)
{
    static struct { unsigned id; char nome[CONTA_NOME_MAX]; } cache[4];
    static int prox, usados;
    for (int i = 0; i < usados; i++) if (cache[i].id == id) return cache[i].nome;
    int i = prox;
    prox = (prox + 1) % 4;
    if (usados < 4) usados++;
    cache[i].id = id;
    conta_nome(id, cache[i].nome, sizeof cache[i].nome);
    return cache[i].nome;
}

/* Permisos, dueño, grupo y tamaño de un elemento (para -l). */
static void completar(item_t *x, const char *caminho)
{
    char c[256], m[10];
    if (caminho) caminho_juntar(c, sizeof c, caminho, x->e.nome);
    else copia_texto(c, x->e.nome, sizeof c);
    info_permisos_t p;
    char t = x->e.tipo == TIPO_PASTA ? 'd' : '-';
    if (permisos_arquivo(c, &p) < 0) {
        formatar_texto(x->perm, sizeof x->perm, "%c?????????", t);
        copia_texto(x->dono, "?", sizeof x->dono);
        copia_texto(x->grupo, "?", sizeof x->grupo);
    } else {
        texto_modo(p.modo, m);
        formatar_texto(x->perm, sizeof x->perm, "%c%s", t, m);
        copia_texto(x->dono, nome_de(p.uid), sizeof x->dono);
        copia_texto(x->grupo, nome_de(p.gid), sizeof x->grupo);
    }
    if (x->e.tipo == TIPO_PASTA) copia_texto(x->tam, "<carpeta>", sizeof x->tam);
    else if (humano) formatar_tamanho(x->tam, sizeof x->tam, x->e.tamanho);
    else formatar_texto(x->tam, sizeof x->tam, "%u", x->e.tamanho);
}

static int maximo(int a, int b) { return a > b ? a : b; }

/* -l: todo alineado. Las anchuras salen del grupo entero (dos pasadas). */
static void mostrar_longo(item_t **v, int n)
{
    int wd = 1, wg = 1, wt = 1;
    for (int i = 0; i < n; i++) {
        wd = maximo(wd, (int)tam_texto(v[i]->dono));
        wg = maximo(wg, (int)tam_texto(v[i]->grupo));
        wt = maximo(wt, (int)tam_texto(v[i]->tam));
    }
    for (int i = 0; i < n; i++) {
        item_t *x = v[i];
        char linha[480], data[32], np[300];
        if (x->mt) {
            fuso_data_t l;
            hora_local(x->mt, &l);
            formatar_data(data, sizeof data, "%d/%m/%Y %H:%M", &l);
        } else copia_texto(data, "                ", sizeof data);
        nome_pintado(np, sizeof np, x->e.nome, x->e.tipo, x->e.tipo == TIPO_PASTA ? "/" : "", 0);
        formatar_texto(linha, sizeof linha, "%s %-*s %-*s %s  %*s  %s%s", x->perm, wd, x->dono, wg, x->grupo, data, wt, x->tam,
                       np, x->e.tipo == TIPO_PROGRAMA ? "  (programa)" : "");
        saida_linha(linha, -1);
    }
}

/* -s: el formato de siempre de SaguiOS. */
static void mostrar_classico(item_t **v, int n)
{
    for (int i = 0; i < n; i++) {
        item_t *x = v[i];
        char linha[420], np[300];
        if (x->e.tipo == TIPO_PASTA) { nome_pintado(np, sizeof np, x->e.nome, x->e.tipo, "/", 0); saida_linha(np, -1); continue; }
        nome_pintado(np, sizeof np, x->e.nome, x->e.tipo, "", 28);
        if (x->e.tipo == TIPO_PROGRAMA) formatar_texto(linha, sizeof linha, "%s %7u bytes  programa", np, x->e.tamanho);
        else formatar_texto(linha, sizeof linha, "%s %7u bytes", np, x->e.tamanho);
        saida_linha(linha, -1);
    }
}

/* En columnas, de arriba abajo (como ls de Unix): las más columnas que quepan en «largura», cada una
 * tan ancha como su nombre más largo, separadas por dos espacios. */
static void mostrar_colunas(item_t **v, int n)
{
    if (n == 0) return;
    int *w = aloca((size_t)n * sizeof *w), *wc = aloca((size_t)n * sizeof *wc);
    if (!w || !wc) { libera(w); libera(wc); mostrar_classico(v, n); return; }
    for (int i = 0; i < n; i++) w[i] = (int)tam_texto(v[i]->e.nome) + (v[i]->e.tipo == TIPO_PASTA ? 1 : 0);
    int ncol = 1, nlin = n;
    for (int c = n < largura / 3 ? n : largura / 3; c > 1; c--) {
        int l = (n + c - 1) / c;
        if ((c - 1) * l >= n) continue;   /* columnas vacías: no vale */
        int tot = 0;
        for (int k = 0; k < c; k++) {
            int m = 0;
            for (int i = k * l; i < n && i < (k + 1) * l; i++) m = maximo(m, w[i]);
            wc[k] = m;
            tot += m + (k ? 2 : 0);
        }
        if (tot < largura) { ncol = c; nlin = l; break; }
    }
    if (ncol == 1) wc[0] = 0;
    else for (int k = 0; k < ncol; k++) {   /* vuelve a medir (el bucle pudo dejar otras) */
        int m = 0;
        for (int i = k * nlin; i < n && i < (k + 1) * nlin; i++) m = maximo(m, w[i]);
        wc[k] = m;
    }
    char *linha = aloca((size_t)largura * 2 + 64 * (size_t)ncol + 300);
    if (!linha) { libera(w); libera(wc); mostrar_classico(v, n); return; }
    for (int l = 0; l < nlin; l++) {
        int pos = 0;
        for (int k = 0; k < ncol; k++) {
            int i = k * nlin + l;
            if (i >= n) break;
            bool ultima = k == ncol - 1 || i + nlin >= n;
            char np[300];
            nome_pintado(np, sizeof np, v[i]->e.nome, v[i]->e.tipo, v[i]->e.tipo == TIPO_PASTA ? "/" : "",
                         ultima ? 0 : wc[k] + 2);
            int t = (int)tam_texto(np);
            memcpy(linha + pos, np, (size_t)t);
            pos += t;
        }
        linha[pos] = 0;
        if (saida_linha(linha, pos) < 0) break;
    }
    libera(linha);
    libera(w);
    libera(wc);
}

static void mostrar_nomes(item_t **v, int n)
{
    for (int i = 0; i < n; i++) {
        char np[300];
        if (cor && *cor_de(v[i]->e.nome, v[i]->e.tipo)) { nome_pintado(np, sizeof np, v[i]->e.nome, v[i]->e.tipo, "", 0); saida_linha(np, -1); }
        else saida_linha(v[i]->e.nome, -1);
    }
}

/* Ordena y muestra un grupo (el contenido de una carpeta, o los archivos sueltos de la línea de órdenes). */
static void mostrar_grupo(item_t **v, int n, const char *caminho)
{
    ordenar_ponteiros((void **)v, n, por_tempo ? cmp_tempo : por_tamanho ? cmp_tamanho : cmp_nome);
    bool vista = tty && !uno;
    if (vista && agrupar && !classico) ordenar_ponteiros((void **)v, n, cmp_grupo);
    if (longo) {
        for (int i = 0; i < n; i++) completar(v[i], caminho);
        mostrar_longo(v, n);
    } else if (classico) mostrar_classico(v, n);
    else if (vista) mostrar_colunas(v, n);
    else mostrar_nomes(v, n);
}

static int listar_pasta(const char *pasta, bool cabecalho, bool *primeiro)
{
    entrada_pasta_t *v;
    int n = listar_tudo(pasta, &v);
    if (n < 0) {
        imprimir_em(ERROS, "ls: %s: %s\n", pasta, nome_erro(n));
        return 1;
    }
    item_t *it = aloca((size_t)(n ? n : 1) * sizeof *it);
    item_t **pt = aloca((size_t)(n ? n : 1) * sizeof *pt);
    if (!it || !pt) { libera(v); libera(it); libera(pt); imprimir_em(ERROS, "ls: sin memoria\n"); return 1; }
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (!ocultos && v[i].nome[0] == '.') continue;
        memset(&it[k], 0, sizeof it[k]);
        it[k].e = v[i];
        char c[256];
        caminho_juntar(c, sizeof c, pasta, v[i].nome);
        it[k].mt = (longo || por_tempo) ? mtime_de(c) : 0;
        pt[k] = &it[k];
        k++;
    }
    libera(v);
    if (cabecalho) {
        if (!*primeiro) saida_linha("", 0);
        char t[300];
        formatar_texto(t, sizeof t, "%s:", pasta);
        saida_linha(t, -1);
    }
    *primeiro = false;
    mostrar_grupo(pt, k, pasta);
    int erros = 0;
    if (recursivo) {
        /* las subcarpetas en orden de nombre (mostrar_grupo pudo agruparlas) */
        ordenar_ponteiros((void **)pt, k, cmp_nome);
        for (int i = 0; i < k; i++) {
            item_t *x = pt[i];
            if (x->e.tipo != TIPO_PASTA) continue;
            char c[256];
            caminho_juntar(c, sizeof c, pasta, x->e.nome);
            erros += listar_pasta(c, true, primeiro);
        }
    }
    libera(it);
    libera(pt);
    return erros;
}

/* El ancho del terminal: COLUMNS (sagsh la pone al día en cada prompt, D385), o 80. */
static int ler_largura(void)
{
    for (char **e = environ; e && *e; e++)
        if (compara_texto_n(*e, "COLUMNS=", 8) == 0) {
            int n = 0;
            for (const char *p = *e + 8; *p >= '0' && *p <= '9'; p++) n = n * 10 + (*p - '0');
            if (n >= 20 && n <= 1000) return n;
        }
    return 80;
}

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    int r;
    int modo_cor = cor_opcao(&argc, argv);
    for (int i = 1; i < argc; i++)   /* --sin-agrupar (se quita antes de mirar las letras) */
        if (compara_texto(argv[i], "--sin-agrupar") == 0) {
            agrupar = false;
            for (int k = i; k < argc - 1; k++) argv[k] = argv[k + 1];
            argc--; i--;
        }
    while ((r = obter_opcao(argc, argv, "1ahlRrSstd", &o)) >= 0) {
        switch (r) {
        case 'l': longo = true; break;
        case 'a': ocultos = true; break;
        case 'h': humano = true; break;
        case 'R': recursivo = true; break;
        case '1': uno = true; break;
        case 's': classico = true; break;
        case 't': por_tempo = true; break;
        case 'S': por_tamanho = true; break;
        case 'r': inverso = true; break;
        case 'd': so_pasta = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("ls", r, &o);
        }
    }
    tty = eh_terminal(SAIDA);
    cor = cor_para(SAIDA, modo_cor);
    largura = ler_largura();
    cuantos_args = argc - o.indice;
    bool primeiro = true;
    int erros = 0;
    if (cuantos_args == 0) return listar_pasta(".", recursivo, &primeiro) ? 1 : 0;
    /* primero los archivos sueltos (juntos, como un grupo), después las carpetas (como ls de Unix) */
    item_t *it = aloca((size_t)cuantos_args * sizeof *it);
    item_t **pt = aloca((size_t)cuantos_args * sizeof *pt);
    if (!it || !pt) { imprimir_em(ERROS, "ls: sin memoria\n"); return 1; }
    int k = 0;
    for (int i = o.indice; i < argc; i++) {
        info_arquivo_t inf;
        int s = info_arquivo(argv[i], &inf);
        if (s < 0) { imprimir_em(ERROS, "ls: %s: %s\n", argv[i], nome_erro(s)); erros++; continue; }
        if (inf.tipo == TIPO_PASTA && !so_pasta) continue;
        memset(&it[k], 0, sizeof it[k]);
        copia_texto(it[k].e.nome, argv[i], sizeof it[k].e.nome);
        it[k].e.tipo = inf.tipo;
        it[k].e.tamanho = inf.tamanho;
        it[k].mt = (longo || por_tempo) ? mtime_de(argv[i]) : 0;
        pt[k] = &it[k];
        k++;
    }
    if (k > 0) { mostrar_grupo(pt, k, NULL); primeiro = false; }
    libera(it);
    libera(pt);
    for (int i = o.indice; i < argc && !so_pasta; i++) {
        info_arquivo_t inf;
        if (info_arquivo(argv[i], &inf) < 0 || inf.tipo != TIPO_PASTA) continue;
        erros += listar_pasta(argv[i], cuantos_args > 1 || recursivo, &primeiro);
    }
    return erros ? 1 : 0;
}
