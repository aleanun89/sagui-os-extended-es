// SPDX-License-Identifier: GPL-2.0-or-later
/* unzip - lista, comprueba y extrae archivos .zip (mejora 056, D103).
 *
 *   unzip [-l] [-t] [-o] [-q] [-d carpeta] archivo.zip [nombre...]
 * -l lista, -t comprueba los CRC, -o sobrescribe sin preguntar, -d carpeta de destino.
 * Siempre extrae dentro del destino: las rutas con ".." o absolutas se rechazan o
 * se recortan, nunca salen. Sin cifrado ni ZIP64: un archivo que los necesite da error. */
#include "compressao.h"

static void uso(void)
{
    imprimir_em(ERROS, "uso: unzip [-l] [-t] [-o] [-q] [-d carpeta] archivo.zip [nombre...]\n"
                       "  -l listar   -t comprobar   -o sobrescribir   -q silencioso   -d carpeta de destino\n");
}

static bool seleccionado(const char *nome, char **f, int n)
{
    if (!n) return true;
    for (int i = 0; i < n; i++) {
        size_t l = tam_texto(f[i]);
        while (l > 1 && f[i][l - 1] == '/') l--;
        if (compara_texto_n(nome, f[i], l) == 0 && (nome[l] == 0 || nome[l] == '/')) return true;
    }
    return false;
}

/* D341: la fecha de modificación de la entrada en UTC: la del campo "UT" si la hay; si no, la del
 * DOS, que es hora local (se toma la zona del sistema). 0 = sin fecha. */
static uint32_t data_zip(const zip_ent_t *e)
{
    if (e->mtime_ut) return e->mtime_ut;
    int ano = 1980 + (e->data >> 9), mes = (e->data >> 5) & 15, dia = e->data & 31;
    if (!e->data || mes < 1 || mes > 12 || dia < 1) return 0;
    return local_para_utc(fuso_compor(ano, mes, dia, e->hora >> 11, (e->hora >> 5) & 63, (e->hora & 31) * 2), NULL);
}

int principal(int argc, char **argv)

{
    bool listar_ = false, testar = false, sobrescrever = false, silencio = false;
    const char *arq = NULL, *destino = ".";
    char *filtro[64];
    int nfiltro = 0;
    for (int i = 1; i < argc; i++) {
        char *a = argv[i];
        if (compara_texto(a, "--help") == 0) { uso(); return 0; }
        if (a[0] == '-' && a[1]) {
            for (char *c = a + 1; *c; c++) {
                if (*c == 'l') listar_ = true;
                else if (*c == 't') testar = true;
                else if (*c == 'o') sobrescrever = true;
                else if (*c == 'q') silencio = true;
                else if (*c == 'n') sobrescrever = false;
                else if (*c == 'd') {
                    const char *v = c[1] ? c + 1 : (i + 1 < argc ? argv[++i] : NULL);
                    if (!v) { imprimir_em(ERROS, "unzip: -d necesita una carpeta\n"); return 2; }
                    destino = v;
                    break;
                } else { imprimir_em(ERROS, "unzip: opción desconocida -%c\n", *c); uso(); return 2; }
            }
        } else if (!arq) arq = a;
        else if (nfiltro < 64) filtro[nfiltro++] = a;
    }
    if (!arq) { uso(); return 2; }
    char nome_arq[300];
    copia_texto(nome_arq, arq, sizeof nome_arq);
    info_arquivo_t inf;
    if (info_arquivo(nome_arq, &inf) < 0) {
        junta_texto(nome_arq, ".zip", sizeof nome_arq);
        if (info_arquivo(nome_arq, &inf) < 0) { imprimir_em(ERROS, "unzip: %s: no existe\n", arq); return 1; }
    }
    int n;
    uint8_t *d = carregar_arquivo(nome_arq, &n);
    if (!d) { imprimir_em(ERROS, "unzip: %s: no se puede leer\n", nome_arq); return 1; }
    zip_leitor_t z;
    int r = zip_abrir(&z, d, n);
    if (r < 0) {
        imprimir_em(ERROS, "unzip: %s: %s\n", nome_arq, r == COMP_ERRO_FORMATO ? "no es un archivo zip válido" : comp_nome_erro(r));
        return 1;
    }
    int erros = 0, feitos = 0;
    uint32_t total = 0;
    if (listar_) {
        imprimir("Archive:  %s\n  Length      Date    Time    Name\n---------  ---------- -----   ----\n", nome_arq);
        for (int i = 0; i < z.cnt; i++) {
            char nome[600];
            nome_de_utf8(z.e[i].nome, nome, sizeof nome);
            if (!seleccionado(nome, filtro, nfiltro)) continue;
            const zip_ent_t *e = &z.e[i];
            imprimir("%9u  %04u-%02u-%02u %02u:%02u   %s\n", e->tu, 1980 + (e->data >> 9), (e->data >> 5) & 15, e->data & 31,
                     e->hora >> 11, (e->hora >> 5) & 63, nome);
            total += e->tu;
            feitos++;
        }
        imprimir("---------                     -------\n%9u                     %d archivo%s\n", total, feitos, feitos == 1 ? "" : "s");
        return 0;
    }
    if (!testar && compara_texto(destino, ".") != 0) {
        r = criar_pasta_p(destino);
        if (r < 0) { imprimir_em(ERROS, "unzip: %s: %s\n", destino, nome_erro(r)); return 1; }
    }
    if (!silencio && !testar) imprimir("Archive:  %s\n", nome_arq);
    for (int i = 0; i < z.cnt; i++) {
        const zip_ent_t *e = &z.e[i];
        char nome[600], real[700];
        nome_de_utf8(e->nome, nome, sizeof nome);
        if (!seleccionado(nome, filtro, nfiltro)) continue;
        feitos++;
        if (testar) {
            uint8_t *o = NULL;
            if (e->pasta) continue;
            r = zip_extrair(&z, i, &o);
            if (r < 0) { imprimir_em(ERROS, "unzip: %s: %s\n", nome, comp_nome_erro(r)); erros++; continue; }
            libera(o);
            if (!silencio) imprimir("    testing: %-30s OK\n", nome);
            continue;
        }
        if (!caminho_seguro(nome, destino, real, sizeof real)) {
            imprimir_em(ERROS, "unzip: %s: ruta peligrosa (sale del destino), se omite\n", nome);
            erros++;
            continue;
        }
        if (e->pasta) {
            r = criar_pasta_p(real);
            if (r < 0) { imprimir_em(ERROS, "unzip: %s: %s\n", real, nome_erro(r)); erros++; }
            else if (!silencio) imprimir("   creating: %s/\n", nome);
            continue;
        }
        info_arquivo_t ie;
        if (info_arquivo(real, &ie) == 0 && !sobrescrever) {
            imprimir_em(ERROS, "unzip: %s ya existe, se omite (usa -o para sobrescribir)\n", real);
            erros++;
            continue;
        }
        uint8_t *o = NULL;
        r = zip_extrair(&z, i, &o);
        if (r < 0) { imprimir_em(ERROS, "unzip: %s: %s\n", nome, comp_nome_erro(r)); erros++; continue; }
        char *barra = acha_ultimo_char(real, '/');
        if (barra) { *barra = 0; criar_pasta_p(real); *barra = '/'; }
        int q = salvar_arquivo(real, o, r);
        libera(o);
        if (q >= 0) { uint32_t t = data_zip(e); if (t) fixar_datas(real, t, PARQ_DATA_AGORA); }   /* D341 */
        if (q < 0) { imprimir_em(ERROS, "unzip: %s: %s\n", real, nome_erro(q)); erros++; }

        else if (!silencio) imprimir("  %s: %s\n", e->metodo ? "inflating" : "extracting", nome);
    }
    if (testar && !erros) imprimir("No errors detected in compressed data of %s.\n", nome_arq);
    if (nfiltro && !feitos) { imprimir_em(ERROS, "unzip: ningún archivo coincide con lo pedido\n"); return 1; }
    return erros ? 1 : 0;
}
