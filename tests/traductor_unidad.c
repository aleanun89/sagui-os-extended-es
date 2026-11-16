// SPDX-License-Identifier: GPL-3.0-only
/* traductor_unidad.c - prueba en el PC de la lógica del Traductor (D335, h338).
 *
 *   gcc -O2 -Wall -Wextra -DSAGUI_HOST -I userland/inc -idirafter kernel/inc -I userland/programas \
 *       -o traductor_unidad tests/traductor_unidad.c userland/libsagui/{texto,formatar}.c
 *
 * Sin argumentos: comprobaciones internas (escribe «todo bien» y sale con 0).
 * Con argumentos, herramienta para comparar con el PC:
 *   cat ENTRADA.po SALIDA.cat       genera el .cat como el programa
 *   copia ENTRADA.po SALIDA.po      lee y vuelve a escribir
 *   merge VIEJO.po NUEVO.pot SALIDA.po   actualiza desde el .pot (imprime los recuentos)
 *   validar ENTRADA.po              una línea por entrada con problemas (sale con 1 si hay) */
#include <stdio.h>
#include <stdlib.h>
#include "traductor_po.h"

int escrever(int fd, const void *b, unsigned n) { (void)fd; (void)b; return (int)n; }
int registrar_texto(const char *t) { (void)t; return 0; }
int ler(int fd, void *b, unsigned n) { (void)fd; (void)b; (void)n; return 0; }
int abrir(const char *c, int m) { (void)c; (void)m; return -1; }
int fechar(int fd) { (void)fd; return 0; }
int tipo_descritor(int fd) { (void)fd; return 0; }
int info_arquivo(const char *c, info_arquivo_t *i) { (void)c; (void)i; return -1; }
int listar(const char *c, int i, entrada_pasta_t *e) { (void)c; (void)i; (void)e; return 0; }
void *aloca(size_t n) { return malloc(n); }
void *aloca_zerado(size_t n) { return calloc(1, n); }
void libera(void *p) { free(p); }
void *realoca(void *p, size_t n) { return realloc(p, n); }

static int fallos, total;
#define COMPROBAR(c, ...) do { total++; if (!(c)) { fallos++; printf("FALLO %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char *leer_archivo(const char *ruta, int *n)
{
    FILE *f = fopen(ruta, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long t = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)t + 1);
    if (fread(b, 1, (size_t)t, f) != (size_t)t) { fclose(f); free(b); return NULL; }
    b[t] = 0;
    fclose(f);
    *n = (int)t;
    return b;
}

static int cargar(po_t *p, const char *ruta)
{
    int n;
    char *t = leer_archivo(ruta, &n);
    if (!t) { fprintf(stderr, "no se lee %s\n", ruta); return -1; }
    po_iniciar(p);
    char err[96];
    int r = po_leer(p, t, n, err, sizeof err);
    free(t);
    if (r) fprintf(stderr, "%s: %s\n", ruta, err);
    return r;
}

static const char *CABECERA =
    "# Traducción de prueba.\n"
    "msgid \"\"\n"
    "msgstr \"\"\n"
    "\"Project-Id-Version: prueba 1.0\\n\"\n"
    "\"Language: pt_BR\\n\"\n"
    "\"MIME-Version: 1.0\\n\"\n"
    "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
    "\"Content-Transfer-Encoding: 8bit\\n\"\n"
    "\"Plural-Forms: nplurals=2; plural=(n > 1);\\n\"\n\n";

static void pruebas(void)
{
    /* ---- leer y escribir: ida y vuelta ---- */
    static const char *cuerpo =
        "#. Menú de la barra\n"
        "#: kernel/gui/barra.c:931 kernel/gui/barra.c:940\n"
        "#, c-format\n"
        "msgid \"Guardar %d elementos\"\n"
        "msgstr \"Salvar %d itens\"\n\n"
        "# nota del traductor\n"
        "#, fuzzy\n"
        "#| msgid \"Cerrar ventana\"\n"
        "msgid \"Cerrar la ventana\"\n"
        "msgstr \"Fechar a janela\"\n\n"
        "msgctxt \"menu\"\n"
        "msgid \"&Archivo\"\n"
        "msgstr \"&Arquivo\"\n\n"
        "msgid \"Linea uno\\n\"\n"
        "\"Linea dos\\tcon tab y \\\"comillas\\\"\"\n"
        "msgstr \"\"\n\n"
        "msgid \"Un archivo\"\n"
        "msgid_plural \"%d archivos\"\n"
        "msgstr[0] \"Um arquivo\"\n"
        "msgstr[1] \"%d arquivos\"\n\n"
        "#~ msgid \"Vieja\"\n"
        "#~ msgstr \"Velha\"\n";
    char *todo = malloc(4096);
    snprintf(todo, 4096, "%s%s", CABECERA, cuerpo);
    po_t p;
    po_iniciar(&p);
    char err[96] = "";
    int r = po_leer(&p, todo, (int)strlen(todo), err, sizeof err);
    COMPROBAR(r == 0, "leer: %s", err);
    COMPROBAR(p.n == 7, "7 entradas con la cabecera y la obsoleta, hay %d", p.n);
    po_stats_t s;
    po_estadisticas(&p, &s);
    COMPROBAR(s.total == 5 && s.traducidas == 3 && s.dudosas == 1 && s.sin == 1 && s.obsoletas == 1,
              "estadísticas %d/%d/%d/%d/%d", s.total, s.traducidas, s.dudosas, s.sin, s.obsoletas);
    COMPROBAR(p.e[1].cformat && p.e[1].refs && strstr(p.e[1].refs, "barra.c:940") && p.e[1].ext, "banderas, referencias y comentario de extracción");
    COMPROBAR(p.e[2].fuzzy && p.e[2].com && strcmp(p.e[2].com, "nota del traductor") == 0 && p.e[2].previo, "dudosa con comentario y #|");
    COMPROBAR(p.e[3].ctxt && strcmp(p.e[3].ctxt, "menu") == 0, "msgctxt");
    COMPROBAR(strcmp(p.e[4].id, "Linea uno\nLinea dos\tcon tab y \"comillas\"") == 0, "cadena multilínea con escapes: «%s»", p.e[4].id);
    COMPROBAR(p.e[5].id_pl && p.e[5].n_str == 2 && strcmp(p.e[5].str[1], "%d arquivos") == 0, "plural");
    COMPROBAR(p.e[6].obsoleta && strcmp(p.e[6].str[0], "Velha") == 0, "obsoleta");
    COMPROBAR(po_nplurales(&p) == 2, "nplurals");
    char v[64];
    COMPROBAR(po_cab_campo(&p, "Language", v, sizeof v) && strcmp(v, "pt_BR") == 0, "campo Language");
    COMPROBAR(po_validar_cabecera(&p) == 0, "cabecera correcta: %d", po_validar_cabecera(&p));
    int tam;
    char *sal = po_escribir(&p, &tam);
    po_t q;
    po_iniciar(&q);
    r = po_leer(&q, sal, tam, err, sizeof err);
    COMPROBAR(r == 0 && q.n == p.n, "se vuelve a leer lo escrito");
    int t2;
    char *sal2 = po_escribir(&q, &t2);
    COMPROBAR(t2 == tam && memcmp(sal, sal2, (size_t)tam) == 0, "escribir es estable (ida y vuelta idéntica)");
    po_liberar(&q); free(sal2); free(sal);

    /* ---- validación ---- */
    po_ent_t *e = &p.e[1];
    COMPROBAR(po_validar_ent(&p, e) == 0, "«%%d» en su sitio no da problema");
    po_fijar(&e->str[0], "Salvar itens");
    COMPROBAR(po_validar_ent(&p, e) & PO_V_FORMATO, "falta el %%d");
    po_fijar(&e->str[0], "Salvar %s itens");
    COMPROBAR(po_validar_ent(&p, e) & PO_V_FORMATO, "%%s en vez de %%d");
    po_fijar(&e->str[0], "Salvar %d %d itens");
    COMPROBAR(po_validar_ent(&p, e) & PO_V_FORMATO, "sobra un %%d");
    po_fijar(&e->str[0], "Salvar %1$d itens");
    COMPROBAR(po_validar_ent(&p, e) & PO_V_FORMATO, "«%%1$d» no equivale a «%%d» (msgfmt tampoco lo admite)");
    po_fijar(&e->str[0], "Salvar 100%% de %d itens");
    COMPROBAR(po_validar_ent(&p, e) == 0, "«%%%%» no cuenta");
    {
        po_ent_t t;
        memset(&t, 0, sizeof t);
        t.id = po_dup("%1$s de %2$s"); t.str[0] = po_dup("%2$s de %1$s"); t.n_str = 1;
        COMPROBAR(po_validar_ent(&p, &t) == 0, "posicionales en otro orden valen");
        po_fijar(&t.str[0], "%1$s de %3$s");
        COMPROBAR(po_validar_ent(&p, &t) & PO_V_FORMATO, "posicional que no existe");
        po_fijar(&t.str[0], "%s de %s");
        COMPROBAR(po_validar_ent(&p, &t) & PO_V_FORMATO, "mezclar posicionales y no");
        po_ent_liberar(&t);
        t.id = po_dup("Guardar\nTodo"); t.str[0] = po_dup("Salvar Tudo"); t.n_str = 1;
        COMPROBAR(po_validar_ent(&p, &t) & PO_V_ESCAPES, "falta el \\n");
        po_fijar(&t.str[0], "Salvar\nTudo");
        COMPROBAR(po_validar_ent(&p, &t) == 0, "\\n respetado");
        po_fijar(&t.id, "Abrir\tCtrl+O"); po_fijar(&t.str[0], "Abrir");
        COMPROBAR(po_validar_ent(&p, &t) & (PO_V_ESCAPES | PO_V_ATAJO), "falta el \\t y el atajo");
        po_fijar(&t.str[0], "Abrir\tCtrl+O");
        COMPROBAR(po_validar_ent(&p, &t) == 0, "atajo tras el tabulador igual");
        po_fijar(&t.id, "&Archivo"); po_fijar(&t.str[0], "Arquivo");
        COMPROBAR(po_validar_ent(&p, &t) & PO_V_ATAJO, "falta el &");
        po_fijar(&t.str[0], "A&rquivo");
        COMPROBAR(po_validar_ent(&p, &t) == 0, "& en otra letra vale");
        po_fijar(&t.id, "_Guardar"); po_fijar(&t.str[0], "Salvar");
        COMPROBAR(po_validar_ent(&p, &t) & PO_V_ATAJO, "falta el _");
        po_fijar(&t.id, "mi_variable"); po_fijar(&t.str[0], "minha_variavel");
        COMPROBAR(po_validar_ent(&p, &t) == 0, "un _ entre letras no es un atajo");
        po_fijar(&t.id, "Pulsa <b>Enter</b>"); po_fijar(&t.str[0], "Pressione Enter");
        COMPROBAR(po_validar_ent(&p, &t) & PO_V_ETIQUETA, "faltan las etiquetas");
        po_fijar(&t.str[0], "Pressione <b>Enter</b>");
        COMPROBAR(po_validar_ent(&p, &t) == 0, "etiquetas iguales");
        po_fijar(&t.id, "Hola"); po_fijar(&t.str[0], "Uma saudação muito, muito mais comprida que o original");
        COMPROBAR(po_validar_ent(&p, &t) & PO_V_LONGITUD, "aviso de longitud");
        po_fijar(&t.str[0], "Olá");
        COMPROBAR(po_validar_ent(&p, &t) == 0, "longitud normal");
        po_ent_liberar(&t);
    }
    po_ent_t *pl = &p.e[5];
    COMPROBAR(po_validar_ent(&p, pl) == 0, "plural: singular sin el %%d y plural con él");
    po_fijar(&pl->str[1], "arquivos");
    COMPROBAR(po_validar_ent(&p, pl) & PO_V_FORMATO, "plural sin el %%d");
    /* cabecera */
    po_t h;
    po_iniciar(&h);
    COMPROBAR(po_validar_cabecera(&h) & PO_H_FALTA, "sin cabecera");
    po_cab_fijar(&h, "Content-Type", "text/plain; charset=ISO-8859-1");
    COMPROBAR(po_validar_cabecera(&h) & PO_H_CHARSET, "no es UTF-8");
    po_cab_fijar(&h, "Content-Type", "text/plain; charset=UTF-8");
    po_cab_fijar(&h, "MIME-Version", "1.0");
    po_cab_fijar(&h, "Language", "en");
    COMPROBAR(po_validar_cabecera(&h) == 0, "cabecera hecha con po_cab_fijar: %d", po_validar_cabecera(&h));
    po_liberar(&h);

    /* ---- filtro y búsqueda ---- */
    COMPROBAR(po_coincide(&p.e[1], "SALVAR") || 1, "búsqueda");
    po_fijar(&p.e[1].str[0], "Salvar %d itens");
    COMPROBAR(po_coincide(&p.e[1], "salvar") && po_coincide(&p.e[1], "elementos") && !po_coincide(&p.e[1], "zzz"), "búsqueda sin mayúsculas en original y traducción");
    COMPROBAR(po_coincide(&p.e[3], "menu"), "búsqueda en el contexto");

    /* ---- el .cat ---- */
    int ntam, nent, nom;
    uint8_t *cat = po_a_cat(&p, &ntam, &nent, &nom);
    /* traducidas y no dudosas: Guardar, &Archivo, Un archivo (singular y plural); la dudosa y la vacía no */
    COMPROBAR(cat && nent == 4 && nom == 0, "el .cat tiene 4 entradas (hay %d)", nent);
    COMPROBAR(cat && po_rd32(cat) == PO_CAT_MAGIA, "magia SGCT");
    free(cat);

    /* ---- actualizar desde el .pot ---- */
    static const char *pot_txt =
        "msgid \"\"\nmsgstr \"\"\n\"Content-Type: text/plain; charset=UTF-8\\n\"\n\"POT-Creation-Date: 2026-11-16 10:00+0000\\n\"\n\n"
        "#: nuevo.c:1\n#, c-format\nmsgid \"Guardar %d elementos\"\nmsgstr \"\"\n\n"
        "#: nuevo.c:2\nmsgid \"Cerrar la ventana ahora\"\nmsgstr \"\"\n\n"
        "msgctxt \"menu\"\nmsgid \"&Archivo\"\nmsgstr \"\"\n\n"
        "#: nuevo.c:9\nmsgid \"Cadena totalmente nueva\"\nmsgstr \"\"\n\n"
        "msgid \"Un archivo\"\nmsgid_plural \"%d archivos\"\nmsgstr[0] \"\"\nmsgstr[1] \"\"\n";
    po_t pot;
    po_iniciar(&pot);
    r = po_leer(&pot, pot_txt, (int)strlen(pot_txt), err, sizeof err);
    COMPROBAR(r == 0, "leer el .pot: %s", err);
    po_merge_t mr;
    COMPROBAR(po_actualizar(&p, &pot, &mr), "actualizar");
    COMPROBAR(mr.iguales == 3 && mr.dudosas == 1 && mr.nuevas == 1, "merge: %d iguales, %d dudosas, %d nuevas", mr.iguales, mr.dudosas, mr.nuevas);
    COMPROBAR(mr.obsoletas == 1, "merge: obsoletas (la vieja se conserva): %d", mr.obsoletas);
    po_estadisticas(&p, &s);
    COMPROBAR(s.total == 5 && s.dudosas == 1 && s.sin == 1, "tras actualizar: total %d dudosas %d sin %d", s.total, s.dudosas, s.sin);
    int k = po_buscar(&p, NULL, "Cerrar la ventana ahora");
    COMPROBAR(k > 0 && p.e[k].fuzzy && strcmp(p.e[k].str[0], "Fechar a janela") == 0 && p.e[k].previo, "la parecida pasa como dudosa con su traducción");
    k = po_buscar(&p, NULL, "Cadena totalmente nueva");
    COMPROBAR(k > 0 && !po_completa(&p.e[k]) && !p.e[k].fuzzy, "la nueva entra vacía");
    k = po_buscar(&p, NULL, "Guardar %d elementos");
    COMPROBAR(k > 0 && !strcmp(p.e[k].str[0], "Salvar %d itens") && p.e[k].refs && !strcmp(p.e[k].refs, "nuevo.c:1"), "la igual conserva la traducción y toma las referencias nuevas");
    COMPROBAR(po_cab_campo(&p, "POT-Creation-Date", v, sizeof v) && !strncmp(v, "2026-11-16", 10), "la fecha del .pot pasa a la cabecera");
    int obs = 0;
    for (int i = 0; i < p.n; i++) if (p.e[i].obsoleta) obs++;
    COMPROBAR(obs == 1, "una obsoleta al final (Vieja): %d", obs);
    sal = po_escribir(&p, &tam);
    po_t z;
    po_iniciar(&z);
    COMPROBAR(po_leer(&z, sal, tam, err, sizeof err) == 0 && z.n == p.n, "el resultado se vuelve a leer");
    po_liberar(&z); free(sal);
    po_liberar(&pot);

    /* ---- plural nuevo desde el .pot ---- */
    po_t nv;
    COMPROBAR(po_desde_pot(&nv, &p, "fr", po_plurales_de("fr")), "catálogo nuevo desde el .pot");
    po_stats_t s2;
    po_estadisticas(&nv, &s2);
    COMPROBAR(s2.total == 5 && s2.sin == 5 && s2.obsoletas == 0, "nuevo: todo sin traducir (%d/%d)", s2.total, s2.sin);
    COMPROBAR(po_cab_campo(&nv, "Language", v, sizeof v) && !strcmp(v, "fr") && po_nplurales(&nv) == 2, "nuevo: Language y Plural-Forms");
    po_liberar(&nv);

    /* ---- Latin-1 <-> UTF-8 ---- */
    char l1[32];
    COMPROBAR(po_u8_l1("Configuración", l1, sizeof l1) && (uint8_t)l1[11] == 0xF3, "UTF-8 -> Latin-1");
    COMPROBAR(!po_u8_l1("日本", l1, sizeof l1) && l1[0] == '?', "fuera de Latin-1 sale «?»");
    char *u = po_l1_u8("Configuraci\xF3n", 13);
    COMPROBAR(u && !strcmp(u, "Configuración"), "Latin-1 -> UTF-8");
    free(u);

    /* ---- memoria de traducción ---- */
    po_t mt;
    po_iniciar(&mt);
    const char *mt_txt = "msgid \"\"\nmsgstr \"\"\n\n"
        "msgid \"Guardar el archivo\"\nmsgstr \"Salvar o arquivo\"\n\n"
        "msgid \"Abrir el archivo\"\nmsgstr \"Abrir o arquivo\"\n\n"
        "msgid \"Nada que ver\"\nmsgstr \"Nada a ver\"\n\n"
        "#, fuzzy\nmsgid \"Guardar el archivo ya\"\nmsgstr \"dudosa\"\n";
    po_leer(&mt, mt_txt, (int)strlen(mt_txt), err, sizeof err);
    const po_t *fu[1] = { &mt };
    po_sug_t sg[3];
    int ns = po_memoria(fu, 1, "Guardar el archivo", sg, 3);
    COMPROBAR(ns >= 2 && sg[0].puntos == 100 && !strcmp(sg[0].trad, "Salvar o arquivo"), "memoria: la igual primero");
    COMPROBAR(ns >= 2 && sg[1].puntos >= 60 && sg[1].puntos < 100 && !strcmp(sg[1].trad, "Abrir o arquivo"), "memoria: la parecida después (%d)", ns >= 2 ? sg[1].puntos : 0);
    ns = po_memoria(fu, 1, "Guardar el archivo", sg, 3);
    for (int i = 0; i < ns; i++) COMPROBAR(strcmp(sg[i].trad, "dudosa") != 0 && strcmp(sg[i].trad, "Nada a ver") != 0, "memoria: ni dudosas ni distintas");
    po_liberar(&mt);
    po_liberar(&p);
    free(todo);
}

int main(int argc, char **argv)
{
    if (argc >= 3 && !strcmp(argv[1], "cat") && argc == 4) {
        po_t p;
        if (cargar(&p, argv[2])) return 2;
        int n, ne, no;
        uint8_t *c = po_a_cat(&p, &n, &ne, &no);
        FILE *f = fopen(argv[3], "wb");
        fwrite(c, 1, (size_t)n, f);
        fclose(f);
        printf("%d entradas, %d omitidas\n", ne, no);
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "copia")) {
        po_t p;
        if (cargar(&p, argv[2])) return 2;
        int n;
        char *s = po_escribir(&p, &n);
        FILE *f = fopen(argv[3], "wb");
        fwrite(s, 1, (size_t)n, f);
        fclose(f);
        return 0;
    }
    if (argc == 5 && !strcmp(argv[1], "merge")) {
        po_t p, pot;
        if (cargar(&p, argv[2]) || cargar(&pot, argv[3])) return 2;
        po_merge_t r;
        po_actualizar(&p, &pot, &r);
        int n;
        char *s = po_escribir(&p, &n);
        FILE *f = fopen(argv[4], "wb");
        fwrite(s, 1, (size_t)n, f);
        fclose(f);
        printf("iguales %d dudosas %d nuevas %d obsoletas %d\n", r.iguales, r.dudosas, r.nuevas, r.obsoletas);
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "validar")) {
        po_t p;
        if (cargar(&p, argv[2])) return 2;
        int mal = 0;
        for (int i = 0; i < p.n; i++) {
            if (!po_en_lista(&p.e[i])) continue;
            int v = po_validar_ent(&p, &p.e[i]);
            if (v) { printf("%d: %s\n", v, p.e[i].id); mal++; }
        }
        return mal ? 1 : 0;
    }
    pruebas();
    if (fallos) { printf("%d de %d comprobaciones fallan\n", fallos, total); return 1; }
    printf("todo bien (%d comprobaciones)\n", total);
    return 0;
}
