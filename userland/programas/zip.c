// SPDX-License-Identifier: GPL-2.0-or-later
/* zip - crea archivos .zip (mejora 056, D103).
 *
 *   zip [-r] [-q] [-0..-9] archivo.zip ruta...
 * -r recorre las carpetas, -q no muestra lo que añade, -0 solo guarda (stored), -1..-9 nivel
 * de deflate (6 por defecto). Si archivo.zip ya existe se reemplaza (no se actualiza).
 * Sin cifrado ni ZIP64. Los nombres se guardan en UTF-8. */
#include "compressao.h"

static zip_escritor_t zip;
static bool silencio;
static int nivel = 6, erros, anadidos;
static const char *ignorar_real;

static int agregar(const char *real, const char *nome, unsigned tipo, unsigned tamanho, void *ctx)
{
    (void)ctx; (void)tamanho;
    if (ignorar_real && compara_texto(real, ignorar_real) == 0) return 0;
    const char *n = nome;
    for (;;) {
        if (n[0] == '/') n++;
        else if (n[0] == '.' && n[1] == '/') n += 2;
        else break;
    }
    if (!*n || compara_texto(n, ".") == 0) return 0;
    for (const char *p = n; *p; p++)
        if (p[0] == '.' && p[1] == '.' && (p == n || p[-1] == '/') && (p[2] == 0 || p[2] == '/')) {
            imprimir_em(ERROS, "zip: %s: nombre con '..', se omite\n", nome);
            erros++;
            return 0;
        }
    char u[1024];
    if (nome_para_utf8(n, u, sizeof u) < 0) { imprimir_em(ERROS, "zip: %s: nombre demasiado largo\n", nome); erros++; return 0; }
    uint32_t mt = comp_mtime(real);
    int r;
    if (tipo == TIPO_PASTA) r = zip_adicionar(&zip, u, NULL, 0, mt, 0, true, 0755);
    else if (tipo == TIPO_ARQUIVO || tipo == TIPO_PROGRAMA) {
        int len;
        uint8_t *d = carregar_arquivo(real, &len);
        if (!d) { imprimir_em(ERROS, "zip: %s: no se puede leer\n", real); erros++; return 0; }
        r = zip_adicionar(&zip, u, d, (uint32_t)len, mt, nivel, false, tipo == TIPO_PROGRAMA ? 0755 : 0644);
        libera(d);
    } else { imprimir_em(ERROS, "zip: %s: tipo no soportado, se omite\n", real); erros++; return 0; }
    if (r < 0) { imprimir_em(ERROS, "zip: %s: %s\n", nome, comp_nome_erro(r)); return r; }
    anadidos++;
    if (!silencio) {
        const zip_ent_t *e = &zip.e[zip.n - 1];
        if (tipo == TIPO_PASTA) imprimir("  adding: %s (stored 0%%)\n", e->nome);
        else {
            unsigned pc = !e->tu ? 0 : e->tu < 40000000u ? 100 - e->tc * 100 / e->tu : 100 - (e->tc >> 7) * 100 / (e->tu >> 7);
            imprimir("  adding: %s (%s %u%%)\n", e->nome, e->metodo ? "deflated" : "stored", pc);
        }
    }
    return 0;
}

int principal(int argc, char **argv)
{
    bool recursivo = false;
    const char *arq = NULL;
    int primeira = argc;
    for (int i = 1; i < argc; i++) {
        char *a = argv[i];
        if (compara_texto(a, "--help") == 0) goto uso;
        if (a[0] == '-' && a[1]) {
            for (char *c = a + 1; *c; c++) {
                if (*c == 'r') recursivo = true;
                else if (*c == 'q') silencio = true;
                else if (*c >= '0' && *c <= '9') nivel = *c - '0';
                else { imprimir_em(ERROS, "zip: opción desconocida -%c\n", *c); goto uso; }
            }
        } else if (!arq) arq = a;
        else { primeira = i; break; }
    }
    if (!arq || primeira >= argc) {
        imprimir_em(ERROS, "zip: faltan el archivo .zip o las rutas a añadir\n");
        goto uso;
    }
    char destino[300];
    copia_texto(destino, arq, sizeof destino);
    const char *base = acha_ultimo_char(arq, '/');
    if (!acha_char(base ? base + 1 : arq, '.')) junta_texto(destino, ".zip", sizeof destino);
    ignorar_real = destino;
    for (int i = primeira; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1]) continue;
        info_arquivo_t inf;
        if (info_arquivo(argv[i], &inf) < 0) { imprimir_em(ERROS, "zip: %s: no existe\n", argv[i]); erros++; continue; }
        if (percorrer(argv[i], argv[i], recursivo, agregar, NULL) < 0) { erros++; break; }
    }
    if (!anadidos) { imprimir_em(ERROS, "zip: no se añadió nada, no se crea %s\n", destino); return 1; }
    int r = zip_concluir(&zip);
    if (r >= 0) r = salvar_arquivo(destino, zip.b.d, zip.b.n);
    if (r < 0) { imprimir_em(ERROS, "zip: %s: %s\n", destino, r <= -100 ? comp_nome_erro(r) : nome_erro(r)); return 1; }
    return erros ? 1 : 0;
uso:
    imprimir_em(ERROS, "uso: zip [-r] [-q] [-0..-9] archivo.zip ruta...\n"
                       "  -r recorrer carpetas   -q silencioso   -0 sin compresión   -1..-9 nivel\n");
    return 2;
}
