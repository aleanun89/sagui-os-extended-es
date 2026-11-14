// SPDX-License-Identifier: GPL-3.0-only
/* ls (listar) - lista archivos y carpetas (D097).
 *
 * En el terminal, el formato de siempre (nombre, tamaño y «programa»); con -l,
 * fecha, tamaño y nombre. Si la salida va a un pipe o a un archivo (o con -1),
 * solo los nombres, uno por línea: así "ls | grep x" y "ls | wc -l" funcionan
 * como en Unix. Las carpetas se ordenan por nombre salvo -t o -S. */
#include "sagui.h"

typedef struct { entrada_pasta_t e; uint32_t mt; } item_t;

static bool longo, ocultos, humano, recursivo, uno, por_tempo, por_tamanho, inverso, so_pasta, tty, cor;
static int cuantos_args;

static const char *AYUDA =
    "uso: ls [-1ahlRrSt] [-d] [archivo|carpeta...]\n"
    "  -l  formato largo: fecha de modificación, tamaño y nombre\n"
    "  -a  incluye los nombres que empiezan por punto\n"
    "  -h  tamaños legibles (1.5K, 3M) con -l\n"
    "  -R  recursivo: entra en las subcarpetas\n"
    "  -1  un nombre por línea (por defecto si la salida no es el terminal)\n"
    "  -t  ordena por fecha de modificación (la más nueva primero)\n"
    "  -S  ordena por tamaño (el mayor primero)\n"
    "  -r  invierte el orden\n"
    "  -d  muestra la carpeta en sí y no su contenido\n"
    "  --color[=cuándo]  colores (always, never, auto): por defecto, solo en el terminal con colores\n";

/* Colores (D263): carpetas en azul, programas en verde, imágenes y sonido en magenta,
 * comprimidos en rojo. Solo con --color o en un terminal con TERM; nunca en tuberías. */
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
    static const char *const img[] = { ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".qoi", ".ppm", ".wav", ".mp3", ".ogg", ".flac", ".mid", ".mpg", ".mp4", NULL };
    static const char *const zip[] = { ".zip", ".tar", ".gz", ".tgz", ".bz2", ".xz", ".7z", ".rar", ".iso", NULL };
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

static int cmp_nome(const void *a, const void *b)
{
    int r = compara_texto(((const item_t *)a)->e.nome, ((const item_t *)b)->e.nome);
    return inverso ? -r : r;
}
static int cmp_tempo(const void *a, const void *b)
{
    const item_t *x = a, *y = b;
    if (x->mt != y->mt) return inverso ? (x->mt > y->mt ? 1 : -1) : (x->mt < y->mt ? 1 : -1);
    return compara_texto(x->e.nome, y->e.nome);
}
static int cmp_tamanho(const void *a, const void *b)
{
    const item_t *x = a, *y = b;
    if (x->e.tamanho != y->e.tamanho) return inverso ? (x->e.tamanho > y->e.tamanho ? 1 : -1) : (x->e.tamanho < y->e.tamanho ? 1 : -1);
    return compara_texto(x->e.nome, y->e.nome);
}

static void mostrar(const char *caminho, const char *nome, unsigned tipo, unsigned tamanho, uint32_t mt)
{
    char linha[420], tam[16], np[200];
    if (!tty || uno) {
        if (!longo) {
            if (cor && *cor_de(nome, tipo)) { nome_pintado(np, sizeof np, nome, tipo, "", 0); saida_linha(np, -1); }
            else saida_linha(nome, -1);
            return;
        }
    } else if (!longo) {
        if (tipo == TIPO_PASTA) { nome_pintado(np, sizeof np, nome, tipo, "/", 0); saida_linha(np, -1); return; }
        nome_pintado(np, sizeof np, nome, tipo, "", 28);
        if (tipo == TIPO_PROGRAMA) formatar_texto(linha, sizeof linha, "%s %7u bytes  programa", np, tamanho);
        else formatar_texto(linha, sizeof linha, "%s %7u bytes", np, tamanho);
        saida_linha(linha, -1);
        return;
    }
    (void)caminho;
    char data[32];
    if (mt) {
        fuso_data_t l;
        hora_local(mt, &l);
        formatar_data(data, sizeof data, "%d/%m/%Y %H:%M", &l);
    } else copia_texto(data, "                ", sizeof data);
    if (humano) formatar_tamanho(tam, sizeof tam, tamanho);
    else formatar_texto(tam, sizeof tam, "%u", tamanho);
    nome_pintado(np, sizeof np, nome, tipo, tipo == TIPO_PASTA ? "/" : "", 0);
    if (tipo == TIPO_PASTA) formatar_texto(linha, sizeof linha, "%s  %9s  %s", data, "<carpeta>", np);
    else formatar_texto(linha, sizeof linha, "%s  %9s  %s%s", data, tam, np, tipo == TIPO_PROGRAMA ? "  (programa)" : "");
    saida_linha(linha, -1);
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
    void **pt = aloca((size_t)(n ? n : 1) * sizeof *pt);
    if (!it || !pt) { libera(v); libera(it); libera(pt); imprimir_em(ERROS, "ls: sin memoria\n"); return 1; }
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (!ocultos && v[i].nome[0] == '.') continue;
        it[k].e = v[i];
        char c[256];
        caminho_juntar(c, sizeof c, pasta, v[i].nome);
        it[k].mt = (longo || por_tempo) ? mtime_de(c) : 0;
        pt[k] = &it[k];
        k++;
    }
    libera(v);
    ordenar_ponteiros(pt, k, por_tempo ? cmp_tempo : por_tamanho ? cmp_tamanho : cmp_nome);
    if (cabecalho) {
        if (!*primeiro) saida_linha("", 0);
        char t[300];
        formatar_texto(t, sizeof t, "%s:", pasta);
        saida_linha(t, -1);
    }
    *primeiro = false;
    int erros = 0;
    for (int i = 0; i < k; i++) {
        item_t *x = pt[i];
        mostrar(pasta, x->e.nome, x->e.tipo, x->e.tamanho, x->mt);
    }
    if (recursivo)
        for (int i = 0; i < k; i++) {
            item_t *x = pt[i];
            if (x->e.tipo != TIPO_PASTA) continue;
            char c[256];
            caminho_juntar(c, sizeof c, pasta, x->e.nome);
            erros += listar_pasta(c, true, primeiro);
        }
    libera(it);
    libera(pt);
    return erros;
}

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    int r;
    int modo_cor = cor_opcao(&argc, argv);
    while ((r = obter_opcao(argc, argv, "1ahlRrStd", &o)) >= 0) {
        switch (r) {
        case 'l': longo = true; break;
        case 'a': ocultos = true; break;
        case 'h': humano = true; break;
        case 'R': recursivo = true; break;
        case '1': uno = true; break;
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
    cuantos_args = argc - o.indice;
    bool primeiro = true;
    int erros = 0;
    if (cuantos_args == 0) return listar_pasta(".", recursivo, &primeiro) ? 1 : 0;
    /* primero los archivos sueltos, después las carpetas (como ls de Unix) */
    for (int pasada = 0; pasada < 2; pasada++)
        for (int i = o.indice; i < argc; i++) {
            info_arquivo_t inf;
            int s = info_arquivo(argv[i], &inf);
            if (s < 0) { if (pasada == 0) { imprimir_em(ERROS, "ls: %s: %s\n", argv[i], nome_erro(s)); erros++; } continue; }
            bool eh_pasta = inf.tipo == TIPO_PASTA;
            if (pasada == 0 && (!eh_pasta || so_pasta)) {
                mostrar(NULL, argv[i], so_pasta && eh_pasta ? TIPO_PASTA : inf.tipo, inf.tamanho,
                        (longo || por_tempo) ? mtime_de(argv[i]) : 0);
                primeiro = false;
            } else if (pasada == 1 && eh_pasta && !so_pasta)
                erros += listar_pasta(argv[i], cuantos_args > 1 || recursivo, &primeiro);
        }
    return erros ? 1 : 0;
}
