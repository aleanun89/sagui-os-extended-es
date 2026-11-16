// SPDX-License-Identifier: GPL-2.0-or-later
/* tar - empaqueta y desempaqueta archivos (mejora 056, D103).
 *
 *   tar -cvzf salida.tar.gz [-C carpeta] ruta...    crear (-z comprime con gzip)
 *   tar -xvf entrada.tar [-C destino] [ruta...]     extraer (siempre dentro del destino)
 *   tar -tvf entrada.tar                            listar
 * El formato de escritura es ustar (POSIX, con la extensión @LongLink de GNU para
 * nombres largos); al leer acepta también pax. La compresión gzip se detecta
 * por el contenido, no por la extensión. Sin -f se usa la entrada o salida estándar. */
#include "compressao.h"

#define MAX_RUTAS 64

static buf_t salida;
static bool verboso;
static int erros;
static const char *ignorar_real;   /* el propio archivo de salida, para no meterlo dentro de sí mismo */

static void uso(void)
{
    imprimir_em(ERROS, "uso: tar -c|-x|-t [-v] [-z] [-f archivo] [-C carpeta] [ruta...]\n"
                       "  -c crear   -x extraer   -t listar   -v detallado   -z gzip (al crear)\n"
                       "  -f archivo (o - = entrada/salida estándar)   -C carpeta (destino al extraer, base al crear)\n");
}

static int agregar(const char *real, const char *nome, unsigned tipo, unsigned tamanho, void *ctx)
{
    (void)ctx; (void)tamanho;
    if (ignorar_real && compara_texto(real, ignorar_real) == 0) return 0;
    char u[1024];
    const char *n = nome;
    while (*n == '/') n++;
    if (!*n) return 0;
    if (nome_para_utf8(n, u, sizeof u) < 0) { imprimir_em(ERROS, "tar: %s: nombre demasiado largo\n", nome); erros++; return 0; }
    uint32_t mt = comp_mtime(real);
    int r;
    if (tipo == TIPO_PASTA) r = tar_adicionar(&salida, u, NULL, 0, mt, 0755, true);
    else if (tipo == TIPO_ARQUIVO || tipo == TIPO_PROGRAMA) {
        int len;
        uint8_t *d = carregar_arquivo(real, &len);
        if (!d) { imprimir_em(ERROS, "tar: %s: no se puede leer\n", real); erros++; return 0; }
        r = tar_adicionar(&salida, u, d, (uint32_t)len, mt, tipo == TIPO_PROGRAMA ? 0755 : 0644, false);
        libera(d);
    } else { imprimir_em(ERROS, "tar: %s: tipo no soportado, se omite\n", real); erros++; return 0; }
    if (r < 0) { imprimir_em(ERROS, "tar: %s: %s\n", nome, comp_nome_erro(r)); return r; }
    if (verboso) imprimir("%s%s\n", n, tipo == TIPO_PASTA ? "/" : "");
    return 0;
}

static bool seleccionado(const char *nome, char **rutas, int n)
{
    if (!n) return true;
    for (int i = 0; i < n; i++) {
        size_t l = tam_texto(rutas[i]);
        while (l > 1 && rutas[i][l - 1] == '/') l--;
        if (compara_texto_n(nome, rutas[i], l) == 0 && (nome[l] == 0 || nome[l] == '/')) return true;
    }
    return false;
}

static void modo_texto(char *s, const tar_entrada_t *e)
{
    uint32_t m = e->modo;
    s[0] = e->tipo == '5' ? 'd' : '-';
    for (int i = 0; i < 9; i++) s[1 + i] = m & (0400 >> i) ? "rwxrwxrwx"[i] : '-';
    s[10] = 0;
}

int principal(int argc, char **argv)
{
    char op = 0;
    const char *arq = NULL, *pasta_c = NULL;
    bool gz = false;
    char *rutas[MAX_RUTAS], *bases[MAX_RUTAS];
    int nrutas = 0;
    int nivel = 6;
    for (int i = 1; i < argc; i++) {
        char *a = argv[i];
        if (compara_texto(a, "--help") == 0) { uso(); return 0; }
        bool opcoes = a[0] == '-' && a[1];
        if (!opcoes && i == 1 && !op) { opcoes = true; a--; }   /* estilo antiguo: tar cvzf archivo ... */
        if (!opcoes) {
            if (nrutas >= MAX_RUTAS) { imprimir_em(ERROS, "tar: demasiadas rutas\n"); return 2; }
            bases[nrutas] = (char *)pasta_c;
            rutas[nrutas++] = a;
            continue;
        }
        if (a[1] == '-') { imprimir_em(ERROS, "tar: opción desconocida %s\n", a); uso(); return 2; }
        for (char *c = a + 1; *c; c++) {
            if (*c == 'c' || *c == 'x' || *c == 't') op = *c;
            else if (*c == 'v') verboso = true;
            else if (*c == 'z') gz = true;
            else if (*c >= '1' && *c <= '9') nivel = *c - '0';
            else if (*c == 'f' || *c == 'C') {
                const char *v = c[1] ? c + 1 : (i + 1 < argc ? argv[++i] : NULL);
                if (!v) { imprimir_em(ERROS, "tar: la opción -%c necesita un valor\n", *c); return 2; }
                if (*c == 'f') arq = v; else pasta_c = v;
                break;
            } else { imprimir_em(ERROS, "tar: opción desconocida -%c\n", *c); uso(); return 2; }
        }
    }
    if (!op) { imprimir_em(ERROS, "tar: hay que indicar -c, -x o -t\n"); uso(); return 2; }
    bool padrao = !arq || compara_texto(arq, "-") == 0;

    if (op == 'c') {
        if (!nrutas) { imprimir_em(ERROS, "tar: no hay nada que archivar\n"); return 2; }
        char real_arq[256];
        if (!padrao) { copia_texto(real_arq, arq, sizeof real_arq); ignorar_real = real_arq; }
        for (int i = 0; i < nrutas; i++) {
            char real[512];
            if (bases[i] && bases[i][0]) formatar_texto(real, sizeof real, "%s/%s", bases[i], rutas[i]);
            else copia_texto(real, rutas[i], sizeof real);
            info_arquivo_t inf;
            if (info_arquivo(real, &inf) < 0) { imprimir_em(ERROS, "tar: %s: no existe\n", real); erros++; continue; }
            if (percorrer(real, rutas[i], true, agregar, NULL) < 0) { erros++; break; }
        }
        if (tar_concluir(&salida) < 0) { imprimir_em(ERROS, "tar: sin memoria\n"); return 1; }
        uint8_t *dados = salida.d;
        int n = salida.n;
        if (gz) {
            int cap = deflar_limite(n) + 64;
            uint8_t *g = aloca((size_t)cap);
            const char *nm = NULL;
            if (!g) { imprimir_em(ERROS, "tar: sin memoria\n"); return 1; }
            int r = gzip_comprimir(dados, n, nm, hora_utc(NULL), nivel, g, cap);
            if (r < 0) { imprimir_em(ERROS, "tar: %s\n", comp_nome_erro(r)); return 1; }
            dados = g; n = r;
        }
        if (padrao) escrever_tudo(SAIDA, dados, (unsigned)n);
        else {
            int r = salvar_arquivo(arq, dados, n);
            if (r < 0) { imprimir_em(ERROS, "tar: %s: %s\n", arq, nome_erro(r)); return 1; }
        }
        return erros ? 1 : 0;
    }

    /* -x y -t: leer el archivo entero, descomprimir si es gzip */
    int n;
    info_arquivo_t ia;
    if (!padrao && info_arquivo(arq, &ia) < 0) { imprimir_em(ERROS, "tar: %s: no existe\n", arq); return 1; }
    uint8_t *d = padrao ? carregar_descritor(ENTRADA, &n) : carregar_arquivo(arq, &n);
    if (!d) { imprimir_em(ERROS, "tar: %s: no se puede leer\n", padrao ? "entrada estándar" : arq); return 1; }
    if (eh_gzip(d, n)) {
        uint8_t *t;
        int tn, r = gzip_descomprimir(d, n, &t, &tn, NULL, 0);
        if (r < 0) { imprimir_em(ERROS, "tar: %s: %s\n", padrao ? "-" : arq, comp_nome_erro(r)); return 1; }
        libera(d);
        d = t; n = tn;
    }
    const char *destino = pasta_c && pasta_c[0] ? pasta_c : ".";
    if (op == 'x' && compara_texto(destino, ".") != 0) {
        int r = criar_pasta_p(destino);
        if (r < 0) { imprimir_em(ERROS, "tar: %s: %s\n", destino, nome_erro(r)); return 1; }
    }
    static tar_entrada_t e;
    int pos = 0, r, total = 0;
    while ((r = tar_proximo(d, n, &pos, &e)) > 0) {
        char nome[1024], destino_real[600];
        if (nome_de_utf8(e.nome, nome, sizeof nome) < 0) copia_texto(nome, e.nome, sizeof nome);
        if (!seleccionado(nome, rutas, nrutas)) continue;
        total++;
        if (op == 't') {
            if (verboso) {
                char m[12], dt[24];
                modo_texto(m, &e);
                comp_data_curta(e.mtime, dt, sizeof dt);
                imprimir("%s %9u %s %s\n", m, e.tamanho, dt, nome);
            } else imprimir("%s\n", nome);
            continue;
        }
        if (e.tipo != '0' && e.tipo != '5') {
            imprimir_em(ERROS, "tar: %s: tipo '%c' no soportado (enlaces), se omite\n", nome, e.tipo);
            erros++;
            continue;
        }
        if (!caminho_seguro(nome, destino, destino_real, sizeof destino_real)) {
            imprimir_em(ERROS, "tar: %s: ruta peligrosa (sale del destino), se omite\n", nome);
            erros++;
            continue;
        }
        if (e.tipo == '5') {
            int q = criar_pasta_p(destino_real);
            if (q < 0) { imprimir_em(ERROS, "tar: %s: %s\n", destino_real, nome_erro(q)); erros++; }
            else if (verboso) imprimir("%s/\n", nome);
            continue;
        }
        char *barra = acha_ultimo_char(destino_real, '/');
        if (barra) { *barra = 0; criar_pasta_p(destino_real); *barra = '/'; }
        int q = salvar_arquivo(destino_real, e.dados, (int)e.tamanho);
        if (q < 0) { imprimir_em(ERROS, "tar: %s: %s\n", destino_real, nome_erro(q)); erros++; }
        else {
            if (e.mtime) fixar_datas(destino_real, e.mtime, PARQ_DATA_AGORA);   /* D341: la fecha del archivo */
            if (verboso) imprimir("%s\n", nome);
        }
    }
    if (r < 0) {
        imprimir_em(ERROS, "tar: %s: %s\n", padrao ? "-" : arq, r == COMP_ERRO_FORMATO ? "no es un archivo tar o está corrupto" : comp_nome_erro(r));
        return 1;
    }
    /* D341: las fechas de las carpetas, al final (crear lo de dentro las cambia) */
    if (op == 'x') {
        pos = 0;
        while (tar_proximo(d, n, &pos, &e) > 0) {
            char nome[1024], destino_real[600];
            if (e.tipo != '5' || !e.mtime) continue;
            if (nome_de_utf8(e.nome, nome, sizeof nome) < 0) copia_texto(nome, e.nome, sizeof nome);
            if (!seleccionado(nome, rutas, nrutas) || !caminho_seguro(nome, destino, destino_real, sizeof destino_real)) continue;
            fixar_datas(destino_real, e.mtime, PARQ_DATA_AGORA);
        }
    }

    return erros ? 1 : 0;
}
