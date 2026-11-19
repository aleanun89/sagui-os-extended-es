// SPDX-License-Identifier: GPL-3.0-only
/* git - la orden git de SaguiOS (D400), sobre libgit2 (GPL-2.0 con excepción de enlace).
 *
 *   git [-C carpeta] orden [opciones]
 *   órdenes: init clone fetch pull push status add commit log diff branch checkout remote config
 *            version help
 *
 * No es el git original (GPL-2.0-only, que no se puede enlazar con la libc/libsagui GPL-3.0): es
 * una orden propia sobre libgit2. Por HTTP(S) usa el TLS de SaguiOS (flujo_sagui.c) y por SSH
 * lanza /bin/ssh con tuberías (el transporte «exec» de libgit2 y proceso.c). Sin editor: los
 * mensajes de commit van con -m. El resultado de las órdenes es el de git (0 = bien).
 * Mensajes en español (Latin-1 en el terminal). Los textos de los commits se guardan en UTF-8: el -m
 * llega en Latin-1 del terminal y se convierte; log y diff los vuelven a Latin-1 si la salida es el terminal. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <git2.h>

int git_sagui_registrar_flujos(void);

static git_repository *repo;
static const char *prog = "git";

static int fallo(int e, const char *que)
{
    const git_error *g = git_error_last();
    fprintf(stderr, "git: %s: %s\n", que, g && g->message ? g->message : "error desconocido");
    return e < 0 ? 128 : e;
}
#define CHK(x, que) do { int e_ = (x); if (e_ < 0) { rc = fallo(e_, que); goto fin; } } while (0)

/* ---- codificación: el terminal de SaguiOS es Latin-1; git guarda UTF-8 ---- */
static int utf8_valido(const unsigned char *p)
{
    while (*p) {
        int n = *p < 0x80 ? 0 : (*p & 0xE0) == 0xC0 ? 1 : (*p & 0xF0) == 0xE0 ? 2 : (*p & 0xF8) == 0xF0 ? 3 : -1;
        if (n < 0) return 0;
        p++;
        while (n--) { if ((*p & 0xC0) != 0x80) return 0; p++; }
    }
    return 1;
}

/* Texto de la línea de órdenes (Latin-1) -> UTF-8, si ya no lo es. Devuelve memoria propia. */
static char *a_utf8(const char *t)
{
    size_t n = strlen(t), k = 0;
    char *o = malloc(n * 2 + 1);
    if (!o) return NULL;
    if (utf8_valido((const unsigned char *)t)) { memcpy(o, t, n + 1); return o; }
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)t[i];
        if (c < 0x80) o[k++] = (char)c;
        else { o[k++] = (char)(0xC0 | (c >> 6)); o[k++] = (char)(0x80 | (c & 0x3F)); }
    }
    o[k] = 0;
    return o;
}

static int a_terminal;   /* la salida es el terminal: UTF-8 -> Latin-1 */

/* Escribe n bytes de texto UTF-8 por la salida estándar; en el terminal, como Latin-1. */
static void sacar(const char *t, size_t n)
{
    if (!a_terminal) { fwrite(t, 1, n, stdout); return; }
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)t[i];
        if (c < 0x80) fputc(c, stdout);
        else if ((c & 0xE0) == 0xC0 && i + 1 < n) {
            unsigned cp = ((c & 0x1F) << 6) | ((unsigned char)t[i + 1] & 0x3F);
            fputc(cp < 0x100 ? (int)cp : '?', stdout);
            i++;
        } else {
            fputc('?', stdout);
            while (i + 1 < n && ((unsigned char)t[i + 1] & 0xC0) == 0x80) i++;
        }
    }
}

static int abrir_repo(void)
{
    if (repo) return 0;
    int e = git_repository_open_ext(&repo, ".", 0, NULL);
    if (e < 0) {
        fprintf(stderr, "git: no es un repositorio git (ni en las carpetas de arriba)\n");
        return 128;
    }
    return 0;
}

static int es_opc(const char *a, const char *corta, const char *larga)
{
    return (corta && !strcmp(a, corta)) || (larga && !strcmp(a, larga));
}

/* ---- credenciales (HTTPS): URL, GIT_USERNAME/GIT_PASSWORD o se pregunta ---- */
static void leer_linea(const char *pregunta, char *buf, size_t n)
{
    fprintf(stderr, "%s", pregunta);
    fflush(stderr);
    buf[0] = 0;
    if (fgets(buf, (int)n, stdin)) {
        size_t k = strlen(buf);
        while (k && (buf[k - 1] == '\n' || buf[k - 1] == '\r')) buf[--k] = 0;
    }
}

static int cb_credenciales(git_credential **out, const char *url, const char *usuario, unsigned tipos, void *p)
{
    static int intentos;
    (void)p;
    if (!(tipos & GIT_CREDENTIAL_USERPASS_PLAINTEXT) || ++intentos > 3) return GIT_PASSTHROUGH;
    char u[128], c[128];
    const char *eu = getenv("GIT_USERNAME"), *ec = getenv("GIT_PASSWORD");
    if (eu && ec) return git_credential_userpass_plaintext_new(out, eu, ec);
    fprintf(stderr, "Credenciales para %s\n", url);
    if (usuario && usuario[0]) snprintf(u, sizeof u, "%s", usuario);
    else leer_linea("Usuario: ", u, sizeof u);
    leer_linea("Contraseña: ", c, sizeof c);
    return git_credential_userpass_plaintext_new(out, u, c);
}

static int cb_progreso(const git_indexer_progress *s, void *p)
{
    static unsigned ultimo = ~0u;
    (void)p;
    unsigned pc = s->total_objects ? (s->received_objects * 100u) / s->total_objects : 0;
    if (pc / 25 != ultimo || s->received_objects == s->total_objects) {
        if (pc / 25 != ultimo)
            fprintf(stderr, "Recibiendo objetos: %u%% (%u/%u)\n", pc, s->received_objects, s->total_objects);
        ultimo = pc / 25;
    }
    return 0;
}

static int cb_remoto(const char *msg, int len, void *p)
{
    (void)p;
    fwrite(msg, 1, (size_t)len, stderr);
    return 0;
}

static void cbs_iniciar(git_remote_callbacks *cb)
{
    git_remote_init_callbacks(cb, GIT_REMOTE_CALLBACKS_VERSION);
    cb->credentials = cb_credenciales;
    cb->transfer_progress = cb_progreso;
    cb->sideband_progress = cb_remoto;
}

/* ---- firma (autor/confirmador) ---- */
static int firma(git_signature **s)
{
    int e = git_signature_default_from_env(s, NULL, repo);
    if (e < 0) e = git_signature_default(s, repo);
    if (e < 0)
        fprintf(stderr, "git: falta tu identidad: git config --global user.name \"Nombre\"; git config --global user.email correo\n");
    return e;
}

/* ================= init ================= */
static int c_init(int argc, char **argv)
{
    const char *dir = ".", *rama = "main";
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (es_opc(argv[i], "-b", "--initial-branch") && i + 1 < argc) rama = argv[++i];
        else if (argv[i][0] != '-') dir = argv[i];
    }
    git_repository_init_options o = GIT_REPOSITORY_INIT_OPTIONS_INIT;
    o.flags = GIT_REPOSITORY_INIT_MKPATH;
    o.initial_head = rama;
    CHK(git_repository_init_ext(&repo, dir, &o), "init");
    printf("Repositorio git vacío creado en %s\n", git_repository_path(repo));
fin:
    return rc;
}

/* ================= clone ================= */
static int c_clone(int argc, char **argv)
{
    const char *url = NULL, *dir = NULL, *rama = NULL;
    int rc = 0, bare = 0;
    for (int i = 1; i < argc; i++) {
        if (es_opc(argv[i], "-b", "--branch") && i + 1 < argc) rama = argv[++i];
        else if (!strcmp(argv[i], "--bare")) bare = 1;
        else if (argv[i][0] != '-') { if (!url) url = argv[i]; else dir = argv[i]; }
    }
    if (!url) { fprintf(stderr, "uso: git clone [-b rama] [--bare] url [carpeta]\n"); return 129; }
    char nombre[256];
    if (!dir) {
        const char *b = strrchr(url, '/');
        const char *c = strrchr(url, ':');
        if (c && (!b || c > b)) b = c;
        b = b ? b + 1 : url;
        snprintf(nombre, sizeof nombre, "%s", *b ? b : "repo");
        size_t k = strlen(nombre);
        if (k > 4 && !strcmp(nombre + k - 4, ".git")) nombre[k - 4] = 0;
        dir = nombre;
    }
    git_clone_options o = GIT_CLONE_OPTIONS_INIT;
    cbs_iniciar(&o.fetch_opts.callbacks);
    o.checkout_branch = rama;
    o.bare = bare;
    fprintf(stderr, "Clonando en '%s'...\n", dir);
    CHK(git_clone(&repo, url, dir, &o), "clone");
fin:
    return rc;
}

/* ================= remoto por defecto ================= */
static int remoto_de(git_remote **r, const char *nombre)
{
    if (nombre) return git_remote_lookup(r, repo, nombre);
    git_reference *h = NULL;
    git_buf rem = GIT_BUF_INIT;
    int e = -1;
    if (git_repository_head(&h, repo) == 0 && git_reference_is_branch(h)) {
        if (git_branch_upstream_remote(&rem, repo, git_reference_name(h)) == 0 && rem.size)
            e = git_remote_lookup(r, repo, rem.ptr);
    }
    git_buf_dispose(&rem);
    git_reference_free(h);
    return e == 0 ? 0 : git_remote_lookup(r, repo, "origin");
}

/* ================= fetch ================= */
static int hacer_fetch(const char *nombre)
{
    git_remote *r = NULL;
    int rc = 0;
    CHK(remoto_de(&r, nombre), "no hay remoto (git remote add origin URL)");
    git_fetch_options fo = GIT_FETCH_OPTIONS_INIT;
    cbs_iniciar(&fo.callbacks);
    fo.prune = GIT_FETCH_PRUNE_UNSPECIFIED;
    CHK(git_remote_fetch(r, NULL, &fo, NULL), "fetch");
    const git_indexer_progress *st = git_remote_stats(r);
    printf("Traídos %u objetos (%u bytes) de %s\n", st->received_objects, (unsigned)st->received_bytes, git_remote_name(r));
fin:
    git_remote_free(r);
    return rc;
}

static int c_fetch(int argc, char **argv)
{
    int rc = abrir_repo();
    if (rc) return rc;
    return hacer_fetch(argc > 1 && argv[1][0] != '-' ? argv[1] : NULL);
}

/* ================= pull ================= */
static int c_pull(int argc, char **argv)
{
    git_reference *cab = NULL, *up = NULL;
    git_annotated_commit *ac = NULL;
    git_object *obj = NULL;
    git_tree *tree = NULL;
    git_commit *padre = NULL, *otro = NULL;
    git_index *idx = NULL;
    int rc = abrir_repo();
    if (rc) return rc;
    rc = hacer_fetch(argc > 1 && argv[1][0] != '-' ? argv[1] : NULL);
    if (rc) return rc;
    CHK(git_repository_head(&cab, repo), "HEAD");
    if (!git_reference_is_branch(cab)) { fprintf(stderr, "git: pull: HEAD no está en una rama\n"); rc = 1; goto fin; }
    if (git_branch_upstream(&up, cab) < 0) {
        char t[300];
        git_buf rem = GIT_BUF_INIT;
        const char *rn = "origin";
        if (git_branch_upstream_remote(&rem, repo, git_reference_name(cab)) == 0 && rem.size) rn = rem.ptr;
        snprintf(t, sizeof t, "refs/remotes/%s/%s", rn, git_reference_name(cab) + strlen("refs/heads/"));
        git_buf_dispose(&rem);
        if (git_reference_lookup(&up, repo, t) < 0) {
            fprintf(stderr, "git: pull: la rama no tiene rama remota que seguir\n");
            rc = 1;
            goto fin;
        }
    }
    CHK(git_annotated_commit_from_ref(&ac, repo, up), "rama remota");
    git_merge_analysis_t an;
    git_merge_preference_t pref;
    const git_annotated_commit *cabezas[1] = { ac };
    CHK(git_merge_analysis(&an, &pref, repo, cabezas, 1), "análisis de la fusión");
    if (an & GIT_MERGE_ANALYSIS_UP_TO_DATE) { printf("Ya está actualizado.\n"); goto fin; }
    if (an & GIT_MERGE_ANALYSIS_UNBORN) {
        const git_oid *id = git_annotated_commit_id(ac);
        CHK(git_object_lookup(&obj, repo, id, GIT_OBJECT_COMMIT), "commit remoto");
        git_checkout_options co = GIT_CHECKOUT_OPTIONS_INIT;
        co.checkout_strategy = GIT_CHECKOUT_SAFE;
        CHK(git_checkout_tree(repo, obj, &co), "checkout");
        git_reference *nueva = NULL;
        CHK(git_reference_create(&nueva, repo, git_reference_name(cab), id, 1, "pull: primer commit"), "rama");
        git_reference_free(nueva);
        printf("Actualizado.\n");
        goto fin;
    }
    if (an & GIT_MERGE_ANALYSIS_FASTFORWARD) {
        const git_oid *id = git_annotated_commit_id(ac);
        CHK(git_object_lookup(&obj, repo, id, GIT_OBJECT_COMMIT), "commit remoto");
        git_checkout_options co = GIT_CHECKOUT_OPTIONS_INIT;
        co.checkout_strategy = GIT_CHECKOUT_SAFE;
        CHK(git_checkout_tree(repo, obj, &co), "checkout (¿cambios locales que se pisarían?)");
        git_reference *nueva = NULL;
        CHK(git_reference_set_target(&nueva, cab, id, "pull: avance rápido"), "avance rápido");
        git_reference_free(nueva);
        char h[GIT_OID_MAX_HEXSIZE + 1];
        git_oid_tostr(h, 9, id);
        printf("Avance rápido hasta %s\n", h);
        goto fin;
    }
    if (an & GIT_MERGE_ANALYSIS_NORMAL) {
        git_merge_options mo = GIT_MERGE_OPTIONS_INIT;
        git_checkout_options co = GIT_CHECKOUT_OPTIONS_INIT;
        co.checkout_strategy = GIT_CHECKOUT_SAFE | GIT_CHECKOUT_ALLOW_CONFLICTS;
        CHK(git_merge(repo, cabezas, 1, &mo, &co), "fusión");
        CHK(git_repository_index(&idx, repo), "índice");
        if (git_index_has_conflicts(idx)) {
            printf("CONFLICTO: arregla los archivos marcados, 'git add' y 'git commit'\n");
            rc = 1;
            goto fin;
        }
        git_oid tid, cid;
        git_signature *sig = NULL;
        CHK(firma(&sig), "identidad");
        CHK(git_index_write_tree(&tid, idx), "árbol");
        git_index_write(idx);
        CHK(git_tree_lookup(&tree, repo, &tid), "árbol");
        CHK(git_commit_lookup(&padre, repo, git_reference_target(cab)), "padre");
        CHK(git_commit_lookup(&otro, repo, git_annotated_commit_id(ac)), "padre remoto");
        char msg[300];
        snprintf(msg, sizeof msg, "Fusión de %s en %s\n", git_reference_shorthand(up), git_reference_shorthand(cab));
        const git_commit *padres[2] = { padre, otro };
        int e = git_commit_create(&cid, repo, "HEAD", sig, sig, NULL, msg, tree, 2, padres);
        git_signature_free(sig);
        CHK(e, "commit de la fusión");
        git_repository_state_cleanup(repo);
        printf("Fusión hecha.\n");
    }
fin:
    git_index_free(idx);
    git_tree_free(tree);
    git_commit_free(padre);
    git_commit_free(otro);
    git_object_free(obj);
    git_annotated_commit_free(ac);
    git_reference_free(up);
    git_reference_free(cab);
    return rc;
}

/* ================= push ================= */
static int cb_push_ref(const char *ref, const char *estado, void *p)
{
    (void)p;
    if (estado) { fprintf(stderr, "git: push: %s rechazado: %s\n", ref, estado); *(int *)p = 1; }
    return 0;
}

static int c_push(int argc, char **argv)
{
    const char *rem = NULL, *rama = NULL;
    int rc = 0, fuerza = 0, up = 0, malo = 0;
    git_remote *r = NULL;
    git_reference *cab = NULL;
    for (int i = 1; i < argc; i++) {
        if (es_opc(argv[i], "-f", "--force")) fuerza = 1;
        else if (es_opc(argv[i], "-u", "--set-upstream")) up = 1;
        else if (argv[i][0] != '-') { if (!rem) rem = argv[i]; else rama = argv[i]; }
    }
    rc = abrir_repo();
    if (rc) return rc;
    CHK(remoto_de(&r, rem), "no hay remoto (git remote add origin URL)");
    char spec[400], corto[200];
    if (!rama) {
        CHK(git_repository_head(&cab, repo), "HEAD");
        if (!git_reference_is_branch(cab)) { fprintf(stderr, "git: push: HEAD no está en una rama\n"); rc = 1; goto fin; }
        snprintf(corto, sizeof corto, "%s", git_reference_shorthand(cab));
        rama = corto;
    }
    if (strchr(rama, ':')) snprintf(spec, sizeof spec, "%s%s", fuerza ? "+" : "", rama);
    else snprintf(spec, sizeof spec, "%srefs/heads/%s:refs/heads/%s", fuerza ? "+" : "", rama, rama);
    char *specs[1] = { spec };
    git_strarray sa = { specs, 1 };
    git_push_options po = GIT_PUSH_OPTIONS_INIT;
    cbs_iniciar(&po.callbacks);
    po.callbacks.push_update_reference = cb_push_ref;
    po.callbacks.payload = &malo;
    CHK(git_remote_push(r, &sa, &po), "push");
    if (malo) { rc = 1; goto fin; }
    printf("Enviado %s a %s\n", rama, git_remote_name(r));
    if (up) {
        git_reference *lr = NULL;
        if (git_branch_lookup(&lr, repo, rama, GIT_BRANCH_LOCAL) == 0) {
            char t[300];
            snprintf(t, sizeof t, "%s/%s", git_remote_name(r), rama);
            git_branch_set_upstream(lr, t);
            git_reference_free(lr);
        }
    }
fin:
    git_reference_free(cab);
    git_remote_free(r);
    return rc;
}

/* ================= status ================= */
static int c_status(int argc, char **argv)
{
    (void)argc; (void)argv;
    git_status_list *sl = NULL;
    git_reference *cab = NULL;
    int rc = abrir_repo();
    if (rc) return rc;
    if (git_repository_head(&cab, repo) == 0) {
        if (git_reference_is_branch(cab)) printf("## %s\n", git_reference_shorthand(cab));
        else printf("## (HEAD desacoplado)\n");
    } else printf("## sin commits todavía\n");
    git_status_options o = GIT_STATUS_OPTIONS_INIT;
    o.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    o.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED | GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX | GIT_STATUS_OPT_SORT_CASE_SENSITIVELY;
    CHK(git_status_list_new(&sl, repo, &o), "status");
    size_t n = git_status_list_entrycount(sl);
    for (size_t i = 0; i < n; i++) {
        const git_status_entry *e = git_status_byindex(sl, i);
        unsigned s = e->status;
        if (s & GIT_STATUS_IGNORED) continue;
        char x = ' ', y = ' ';
        const char *ruta = NULL;
        if (s & GIT_STATUS_INDEX_NEW) x = 'A';
        else if (s & GIT_STATUS_INDEX_MODIFIED) x = 'M';
        else if (s & GIT_STATUS_INDEX_DELETED) x = 'D';
        else if (s & GIT_STATUS_INDEX_RENAMED) x = 'R';
        else if (s & GIT_STATUS_INDEX_TYPECHANGE) x = 'T';
        if (s & GIT_STATUS_WT_NEW) { x = '?'; y = '?'; }
        else if (s & GIT_STATUS_WT_MODIFIED) y = 'M';
        else if (s & GIT_STATUS_WT_DELETED) y = 'D';
        else if (s & GIT_STATUS_WT_TYPECHANGE) y = 'T';
        else if (s & GIT_STATUS_WT_RENAMED) y = 'R';
        if (s & GIT_STATUS_CONFLICTED) { x = 'U'; y = 'U'; }
        if (e->head_to_index && e->head_to_index->new_file.path) ruta = e->head_to_index->new_file.path;
        if (!ruta && e->index_to_workdir) ruta = e->index_to_workdir->new_file.path;
        if (!ruta && e->index_to_workdir) ruta = e->index_to_workdir->old_file.path;
        if (x == 'R' && e->head_to_index && e->head_to_index->old_file.path)
            printf("%c%c %s -> %s\n", x, y, e->head_to_index->old_file.path, ruta);
        else printf("%c%c %s\n", x, y, ruta ? ruta : "?");
    }
fin:
    git_status_list_free(sl);
    git_reference_free(cab);
    return rc;
}

/* ================= add ================= */
static int c_add(int argc, char **argv)
{
    git_index *idx = NULL;
    int rc = abrir_repo();
    if (rc) return rc;
    CHK(git_repository_index(&idx, repo), "índice");
    char **rutas = calloc((size_t)argc + 1, sizeof *rutas);
    size_t nr = 0;
    int todo = 0;
    for (int i = 1; i < argc; i++) {
        if (es_opc(argv[i], "-A", "--all") || es_opc(argv[i], "-u", NULL)) todo = 1;
        else if (argv[i][0] != '-' || !strcmp(argv[i], "-")) rutas[nr++] = argv[i];
    }
    if (!nr && todo) rutas[nr++] = (char *)".";
    if (!nr) { fprintf(stderr, "uso: git add archivo... | git add .\n"); free(rutas); git_index_free(idx); return 129; }
    git_strarray sa = { rutas, nr };
    int e = git_index_add_all(idx, &sa, GIT_INDEX_ADD_DEFAULT, NULL, NULL);
    if (e == 0) e = git_index_update_all(idx, &sa, NULL, NULL);   /* también registra los borrados */
    if (e == 0) e = git_index_write(idx);
    free(rutas);
    CHK(e, "add");
fin:
    git_index_free(idx);
    return rc;
}

/* ================= commit ================= */
typedef struct { git_oid id; int ok; } mhead_t;
static int cb_mergehead(const git_oid *o, void *d)
{
    mhead_t *m = d;
    if (!m->ok) { m->id = *o; m->ok = 1; }
    return 0;
}

static int c_commit(int argc, char **argv)
{
    git_index *idx = NULL;
    git_tree *tree = NULL, *tpadre = NULL;
    git_commit *padre = NULL, *mhead = NULL;
    git_signature *sig = NULL;
    git_reference *cab = NULL;
    char *msg = NULL;
    int rc = abrir_repo(), todo = 0, enmienda = 0;
    if (rc) return rc;
    for (int i = 1; i < argc; i++) {
        if (es_opc(argv[i], "-m", "--message") && i + 1 < argc) {
            size_t a = msg ? strlen(msg) : 0, b = strlen(argv[i + 1]);
            char *u = a_utf8(argv[i + 1]);
            b = strlen(u);
            msg = realloc(msg, a + b + 3);
            if (a) { memcpy(msg + a, "\n\n", 2); a += 2; }
            memcpy(msg + a, u, b + 1);
            free(u);
            i++;
        } else if (es_opc(argv[i], "-a", "--all")) todo = 1;
        else if (!strcmp(argv[i], "--amend")) enmienda = 1;
    }
    if (!msg && !enmienda) { fprintf(stderr, "git: commit: falta el mensaje (-m \"texto\"); no hay editor\n"); return 129; }
    CHK(git_repository_index(&idx, repo), "índice");
    if (todo) {
        char *dot[1] = { (char *)"*" };
        git_strarray sa = { dot, 1 };
        CHK(git_index_update_all(idx, &sa, NULL, NULL), "add -a");
        CHK(git_index_write(idx), "índice");
    }
    CHK(firma(&sig), "identidad");
    git_oid tid, cid;
    CHK(git_index_write_tree(&tid, idx), "árbol");
    CHK(git_tree_lookup(&tree, repo, &tid), "árbol");
    const git_commit *padres[2];
    int np = 0;
    if (git_repository_head_unborn(repo) == 0) {
        CHK(git_repository_head(&cab, repo), "HEAD");
        CHK(git_commit_lookup(&padre, repo, git_reference_target(cab)), "padre");
        if (enmienda) {
            const git_commit *ant = padre;
            if (!msg) msg = strdup(git_commit_message(ant));
            unsigned npd = git_commit_parentcount(ant);
            git_commit *gp = NULL;
            if (npd) { CHK(git_commit_parent(&gp, ant, 0), "padre"); padres[np++] = gp; }
            git_commit_amend(&cid, ant, "HEAD", NULL, sig, NULL, msg, tree);
            printf("[%.8s] enmendado\n", git_oid_tostr_s(&cid));
            git_commit_free(gp);
            goto fin;
        }
        CHK(git_commit_tree(&tpadre, padre), "árbol del padre");
        padres[np++] = padre;
        if (git_repository_state(repo) == GIT_REPOSITORY_STATE_MERGE) {
            mhead_t mh = { {{0}}, 0 };
            git_repository_mergehead_foreach(repo, cb_mergehead, &mh);
            if (mh.ok) { CHK(git_commit_lookup(&mhead, repo, &mh.id), "MERGE_HEAD"); padres[np++] = mhead; }
        } else if (!git_oid_cmp(git_tree_id(tpadre), &tid)) {
            printf("Nada que confirmar: el árbol no ha cambiado (¿git add?)\n");
            rc = 1;
            goto fin;
        }
    }
    CHK(git_commit_create(&cid, repo, "HEAD", sig, sig, NULL, msg, tree, (size_t)np, padres), "commit");
    {
        char h[16];
        git_oid_tostr(h, 9, &cid);
        git_reference *h2 = NULL;
        const char *rn = "(desacoplado)";
        if (git_repository_head(&h2, repo) == 0 && git_reference_is_branch(h2)) rn = git_reference_shorthand(h2);
        {
            size_t k = strcspn(msg, "\n");
            printf("[%s %s] ", rn, h);
            sacar(msg, k > 60 ? 60 : k);
            printf("\n");
        }
        git_reference_free(h2);
    }
    git_repository_state_cleanup(repo);
fin:
    free(msg);
    git_signature_free(sig);
    git_tree_free(tree);
    git_tree_free(tpadre);
    git_commit_free(padre);
    git_commit_free(mhead);
    git_reference_free(cab);
    git_index_free(idx);
    return rc;
}

/* ================= log ================= */
static int c_log(int argc, char **argv)
{
    git_revwalk *w = NULL;
    int rc = abrir_repo(), max = -1, una = 0;
    const char *desde = NULL;
    if (rc) return rc;
    for (int i = 1; i < argc; i++) {
        if (es_opc(argv[i], "-n", NULL) && i + 1 < argc) max = atoi(argv[++i]);
        else if (argv[i][0] == '-' && argv[i][1] >= '0' && argv[i][1] <= '9') max = atoi(argv[i] + 1);
        else if (!strcmp(argv[i], "--oneline")) una = 1;
        else if (argv[i][0] != '-') desde = argv[i];
    }
    CHK(git_revwalk_new(&w, repo), "log");
    git_revwalk_sorting(w, GIT_SORT_TIME);
    if (desde) {
        git_object *o = NULL;
        CHK(git_revparse_single(&o, repo, desde), "revisión");
        int e = git_revwalk_push(w, git_object_id(o));
        git_object_free(o);
        CHK(e, "log");
    } else if (git_revwalk_push_head(w) < 0) {
        fprintf(stderr, "git: log: no hay commits todavía\n");
        rc = 128;
        goto fin;
    }
    git_oid id;
    while (max != 0 && git_revwalk_next(&id, w) == 0) {
        git_commit *c = NULL;
        if (git_commit_lookup(&c, repo, &id) < 0) break;
        char h[GIT_OID_MAX_HEXSIZE + 1];
        const git_signature *a = git_commit_author(c);
        if (una) {
            git_oid_tostr(h, 9, &id);
            const char *m = git_commit_message(c);
            const char *nl = strchr(m, '\n');
            printf("%s ", h);
            sacar(m, nl ? (size_t)(nl - m) : strlen(m));
            printf("\n");
        } else {
            git_oid_tostr(h, sizeof h, &id);
            time_t t = (time_t)a->when.time + a->when.offset * 60;
            struct tm *tm = gmtime(&t);
            char f[64] = "";
            if (tm) strftime(f, sizeof f, "%Y-%m-%d %H:%M:%S", tm);
            printf("commit %s\nAutor: ", h);
            sacar(a->name, strlen(a->name));
            printf(" <%s>\nFecha: %s %c%02d%02d\n\n", a->email, f,
                   a->when.offset < 0 ? '-' : '+', abs(a->when.offset) / 60, abs(a->when.offset) % 60);
            const char *m = git_commit_message(c);
            while (*m) {
                const char *nl = strchr(m, '\n');
                size_t k = nl ? (size_t)(nl - m) : strlen(m);
                if (!nl && !k) break;
                printf("    ");
                sacar(m, k);
                printf("\n");
                m += k + (nl ? 1 : 0);
            }
            printf("\n");
        }
        git_commit_free(c);
        if (max > 0) max--;
    }
fin:
    git_revwalk_free(w);
    return rc;
}

/* ================= diff ================= */
static int cb_diff(const git_diff_delta *d, const git_diff_hunk *h, const git_diff_line *l, void *p)
{
    (void)d; (void)h; (void)p;
    if (l->origin == '+' || l->origin == '-' || l->origin == ' ') fputc(l->origin, stdout);
    sacar(l->content, l->content_len);
    return 0;
}

static int c_diff(int argc, char **argv)
{
    git_diff *df = NULL;
    git_tree *tree = NULL;
    git_object *o = NULL;
    git_index *idx = NULL;
    int rc = abrir_repo(), cached = 0, stat = 0, nombres = 0;
    if (rc) return rc;
    char *rutas[64];
    size_t nr = 0;
    const char *rev = NULL;
    for (int i = 1; i < argc; i++) {
        if (es_opc(argv[i], NULL, "--cached") || !strcmp(argv[i], "--staged")) cached = 1;
        else if (!strcmp(argv[i], "--stat")) stat = 1;
        else if (!strcmp(argv[i], "--name-only")) nombres = 1;
        else if (!strcmp(argv[i], "--")) { for (i++; i < argc && nr < 64; i++) rutas[nr++] = argv[i]; }
        else if (argv[i][0] != '-') rev = argv[i];
    }
    git_diff_options o2 = GIT_DIFF_OPTIONS_INIT;
    o2.pathspec.strings = rutas;
    o2.pathspec.count = nr;
    CHK(git_repository_index(&idx, repo), "índice");
    if (cached || rev) {
        git_object *ob = NULL;
        if (git_revparse_single(&ob, repo, rev ? rev : "HEAD^{tree}") == 0) {
            git_object *peeled = NULL;
            if (git_object_peel(&peeled, ob, GIT_OBJECT_TREE) == 0) tree = (git_tree *)peeled;
            git_object_free(ob);
        }
        if (cached) CHK(git_diff_tree_to_index(&df, repo, tree, idx, &o2), "diff");
        else CHK(git_diff_tree_to_workdir_with_index(&df, repo, tree, &o2), "diff");
    } else {
        CHK(git_diff_index_to_workdir(&df, repo, idx, &o2), "diff");
    }
    if (stat) {
        git_diff_stats *s = NULL;
        git_buf b = GIT_BUF_INIT;
        CHK(git_diff_get_stats(&s, df), "stats");
        git_diff_stats_to_buf(&b, s, GIT_DIFF_STATS_FULL | GIT_DIFF_STATS_SHORT, 80);
        fputs(b.ptr, stdout);
        git_buf_dispose(&b);
        git_diff_stats_free(s);
    } else {
        CHK(git_diff_print(df, nombres ? GIT_DIFF_FORMAT_NAME_ONLY : GIT_DIFF_FORMAT_PATCH, cb_diff, NULL), "diff");
    }
fin:
    git_diff_free(df);
    git_tree_free(tree);
    git_object_free(o);
    git_index_free(idx);
    return rc;
}

/* ================= branch ================= */
static int c_branch(int argc, char **argv)
{
    int rc = abrir_repo(), borrar = 0, todas = 0;
    const char *nombre = NULL, *inicio = NULL;
    git_branch_iterator *it = NULL;
    git_reference *r = NULL, *nueva = NULL;
    git_commit *c = NULL;
    git_object *o = NULL;
    if (rc) return rc;
    for (int i = 1; i < argc; i++) {
        if (es_opc(argv[i], "-d", "--delete") || !strcmp(argv[i], "-D")) borrar = 1;
        else if (es_opc(argv[i], "-a", "--all")) todas = 1;
        else if (es_opc(argv[i], "-r", NULL)) todas = 2;
        else if (argv[i][0] != '-') { if (!nombre) nombre = argv[i]; else inicio = argv[i]; }
    }
    if (borrar) {
        if (!nombre) { fprintf(stderr, "uso: git branch -d rama\n"); return 129; }
        CHK(git_branch_lookup(&r, repo, nombre, GIT_BRANCH_LOCAL), "rama");
        CHK(git_branch_delete(r), "borrar rama");
        printf("Rama %s borrada\n", nombre);
    } else if (nombre) {
        if (inicio) { CHK(git_revparse_single(&o, repo, inicio), "revisión"); CHK(git_commit_lookup(&c, repo, git_object_id(o)), "commit"); }
        else {
            CHK(git_repository_head(&r, repo), "HEAD (¿aún no hay commits?)");
            CHK(git_commit_lookup(&c, repo, git_reference_target(r)), "commit");
        }
        CHK(git_branch_create(&nueva, repo, nombre, c, 0), "crear rama");
    } else {
        git_reference *cab = NULL;
        const char *actual = NULL;
        if (git_repository_head(&cab, repo) == 0 && git_reference_is_branch(cab)) actual = git_reference_shorthand(cab);
        git_branch_t tipo = todas == 1 ? GIT_BRANCH_ALL : todas == 2 ? GIT_BRANCH_REMOTE : GIT_BRANCH_LOCAL;
        CHK(git_branch_iterator_new(&it, repo, tipo), "ramas");
        git_reference *ref;
        git_branch_t t;
        while (git_branch_next(&ref, &t, it) == 0) {
            const char *n = NULL;
            git_branch_name(&n, ref);
            printf("%c %s%s\n", actual && t == GIT_BRANCH_LOCAL && !strcmp(actual, n) ? '*' : ' ',
                   t == GIT_BRANCH_REMOTE ? "remotes/" : "", n);
            git_reference_free(ref);
        }
        git_reference_free(cab);
    }
fin:
    git_branch_iterator_free(it);
    git_reference_free(r);
    git_reference_free(nueva);
    git_commit_free(c);
    git_object_free(o);
    return rc;
}

/* ================= checkout ================= */
static int c_checkout(int argc, char **argv)
{
    int rc = abrir_repo(), nueva = 0, fuerza = 0;
    const char *spec = NULL, *inicio = NULL;
    git_object *o = NULL;
    git_reference *r = NULL, *rr = NULL;
    git_commit *c = NULL;
    git_index *idx = NULL;
    if (rc) return rc;
    char *rutas[64];
    size_t nr = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-b") && i + 1 < argc) { nueva = 1; spec = argv[++i]; }
        else if (es_opc(argv[i], "-f", "--force")) fuerza = 1;
        else if (!strcmp(argv[i], "--")) { for (i++; i < argc && nr < 64; i++) rutas[nr++] = argv[i]; }
        else if (argv[i][0] != '-') { if (!spec) spec = argv[i]; else if (nueva) inicio = argv[i]; else if (nr < 64) rutas[nr++] = argv[i]; }
    }
    if (!spec && !nr) { fprintf(stderr, "uso: git checkout rama | -b nueva [inicio] | -- archivos\n"); return 129; }
    git_checkout_options co = GIT_CHECKOUT_OPTIONS_INIT;
    co.checkout_strategy = fuerza ? GIT_CHECKOUT_FORCE : GIT_CHECKOUT_SAFE;
    if (!nueva && nr && (!spec || git_reference_lookup(&r, repo, spec) < 0)) {
        /* git checkout -- archivos (o «git checkout archivo»): restaura desde el índice */
        if (spec && !nr) rutas[nr++] = (char *)spec;
        else if (spec) { memmove(rutas + 1, rutas, nr * sizeof *rutas); rutas[0] = (char *)spec; nr++; }
        co.checkout_strategy = GIT_CHECKOUT_FORCE;
        co.paths.strings = rutas;
        co.paths.count = nr;
        CHK(git_checkout_index(repo, NULL, &co), "checkout de archivos");
        goto fin;
    }
    if (nr && !nueva) { /* «checkout rama archivos»: desde esa revisión */
        CHK(git_revparse_single(&o, repo, spec), "revisión");
        co.checkout_strategy = GIT_CHECKOUT_FORCE;
        co.paths.strings = rutas;
        co.paths.count = nr;
        CHK(git_checkout_tree(repo, o, &co), "checkout de archivos");
        goto fin;
    }
    if (nueva) {
        git_object *ob = NULL;
        CHK(git_revparse_single(&ob, repo, inicio ? inicio : "HEAD"), "revisión inicial");
        int e = git_commit_lookup(&c, repo, git_object_id(ob));
        git_object_free(ob);
        CHK(e, "commit");
        CHK(git_branch_create(&rr, repo, spec, c, 0), "crear rama");
        CHK(git_object_lookup(&o, repo, git_commit_id(c), GIT_OBJECT_COMMIT), "commit");
        CHK(git_checkout_tree(repo, o, &co), "checkout");
        CHK(git_repository_set_head(repo, git_reference_name(rr)), "HEAD");
        printf("Cambiado a la rama nueva '%s'\n", spec);
        goto fin;
    }
    git_reference_free(r);
    r = NULL;
    if (git_branch_lookup(&r, repo, spec, GIT_BRANCH_LOCAL) == 0) {
        CHK(git_revparse_single(&o, repo, git_reference_name(r)), "rama");
        CHK(git_checkout_tree(repo, o, &co), "checkout (¿cambios locales que se pisarían?)");
        CHK(git_repository_set_head(repo, git_reference_name(r)), "HEAD");
        printf("Cambiado a la rama '%s'\n", spec);
        goto fin;
    }
    /* «git checkout x» con x solo en el remoto: se crea la rama local que lo sigue */
    {
        git_strarray rems = { 0 };
        git_remote_list(&rems, repo);
        for (size_t i = 0; i < rems.count && !rr; i++) {
            char t[300];
            git_reference *rm = NULL;
            snprintf(t, sizeof t, "refs/remotes/%s/%s", rems.strings[i], spec);
            if (git_reference_lookup(&rm, repo, t) == 0) {
                git_commit *cm = NULL;
                if (git_commit_lookup(&cm, repo, git_reference_target(rm)) == 0 &&
                    git_branch_create(&rr, repo, spec, cm, 0) == 0) {
                    snprintf(t, sizeof t, "%s/%s", rems.strings[i], spec);
                    git_branch_set_upstream(rr, t);
                    git_object_lookup(&o, repo, git_commit_id(cm), GIT_OBJECT_COMMIT);
                }
                git_commit_free(cm);
                git_reference_free(rm);
            }
        }
        git_strarray_dispose(&rems);
        if (rr && o) {
            CHK(git_checkout_tree(repo, o, &co), "checkout");
            CHK(git_repository_set_head(repo, git_reference_name(rr)), "HEAD");
            printf("Cambiado a la rama '%s' (sigue a la remota)\n", spec);
            goto fin;
        }
    }
    /* un commit o etiqueta: HEAD desacoplado */
    CHK(git_revparse_single(&o, repo, spec), "no existe esa rama ni revisión");
    {
        git_object *cm = NULL;
        CHK(git_object_peel(&cm, o, GIT_OBJECT_COMMIT), "commit");
        int e = git_checkout_tree(repo, cm, &co);
        if (e == 0) e = git_repository_set_head_detached(repo, git_object_id(cm));
        git_object_free(cm);
        CHK(e, "checkout");
        printf("HEAD desacoplado en %.8s\n", git_oid_tostr_s(git_object_id(o)));
    }
fin:
    git_index_free(idx);
    git_object_free(o);
    git_reference_free(r);
    git_reference_free(rr);
    git_commit_free(c);
    return rc;
}

/* ================= remote ================= */
static int c_remote(int argc, char **argv)
{
    int rc = abrir_repo();
    git_remote *r = NULL;
    if (rc) return rc;
    int a = 1, verboso = 0;
    if (a < argc && (!strcmp(argv[a], "-v") || !strcmp(argv[a], "--verbose"))) { verboso = 1; a++; }
    if (a >= argc) {
        git_strarray l = { 0 };
        CHK(git_remote_list(&l, repo), "remoto");
        for (size_t i = 0; i < l.count; i++) {
            if (verboso) {
                git_remote *x = NULL;
                if (git_remote_lookup(&x, repo, l.strings[i]) == 0) {
                    const char *pu = git_remote_pushurl(x);
                    printf("%s\t%s (fetch)\n%s\t%s (push)\n", l.strings[i], git_remote_url(x), l.strings[i], pu ? pu : git_remote_url(x));
                    git_remote_free(x);
                }
            } else printf("%s\n", l.strings[i]);
        }
        git_strarray_dispose(&l);
    } else if (!strcmp(argv[a], "add") && a + 2 < argc) {
        CHK(git_remote_create(&r, repo, argv[a + 1], argv[a + 2]), "remote add");
    } else if ((!strcmp(argv[a], "remove") || !strcmp(argv[a], "rm")) && a + 1 < argc) {
        CHK(git_remote_delete(repo, argv[a + 1]), "remote remove");
    } else if (!strcmp(argv[a], "set-url") && a + 2 < argc) {
        CHK(git_remote_set_url(repo, argv[a + 1], argv[a + 2]), "remote set-url");
    } else if (!strcmp(argv[a], "get-url") && a + 1 < argc) {
        CHK(git_remote_lookup(&r, repo, argv[a + 1]), "remote");
        printf("%s\n", git_remote_url(r));
    } else if (!strcmp(argv[a], "rename") && a + 2 < argc) {
        git_strarray pr = { 0 };
        CHK(git_remote_rename(&pr, repo, argv[a + 1], argv[a + 2]), "remote rename");
        git_strarray_dispose(&pr);
    } else {
        fprintf(stderr, "uso: git remote [-v] | add nombre url | remove nombre | set-url nombre url | get-url nombre | rename a b\n");
        rc = 129;
    }
fin:
    git_remote_free(r);
    return rc;
}

/* ================= config ================= */
static int cb_cfg(const git_config_entry *e, void *p)
{
    (void)p;
    printf("%s=%s\n", e->name, e->value ? e->value : "");
    return 0;
}

static int c_config(int argc, char **argv)
{
    git_config *cfg = NULL, *nivel = NULL;
    int rc = 0, global = 0, lista = 0, quita = 0;
    const char *k = NULL, *v = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--global")) global = 1;
        else if (!strcmp(argv[i], "--local")) global = 0;
        else if (!strcmp(argv[i], "--list") || !strcmp(argv[i], "-l")) lista = 1;
        else if (!strcmp(argv[i], "--unset")) quita = 1;
        else if (!strcmp(argv[i], "--get")) ;
        else if (argv[i][0] != '-' || !k) { if (!k) k = argv[i]; else v = argv[i]; }
    }
    if (git_repository_open_ext(&repo, ".", 0, NULL) < 0) repo = NULL;
    if (repo) CHK(git_repository_config(&cfg, repo), "config");
    else CHK(git_config_open_default(&cfg), "config");
    if (global || !repo) {
        git_buf p = GIT_BUF_INIT;
        char ruta[300];
        int e;
        if (git_config_find_global(&p) == 0) snprintf(ruta, sizeof ruta, "%s", p.ptr);
        else {
            const char *h = getenv("HOME");
            snprintf(ruta, sizeof ruta, "%s/.gitconfig", h ? h : "/home");
        }
        git_buf_dispose(&p);
        e = git_config_open_ondisk(&nivel, ruta);   /* el archivo se crea al fijar la primera clave */
        CHK(e, "config global");
    } else nivel = cfg, cfg = NULL;
    if (lista) {
        git_config *todo = cfg ? cfg : nivel;
        CHK(git_config_foreach(todo, cb_cfg, NULL), "config");
    } else if (!k) {
        fprintf(stderr, "uso: git config [--global] clave [valor] | --list | --unset clave\n");
        rc = 129;
    } else if (quita) {
        CHK(git_config_delete_entry(nivel, k), "config --unset");
    } else if (v) {
        CHK(git_config_set_string(nivel, k, v), "config");
    } else {
        git_config *busca = repo ? ((git_repository_config(&cfg, repo) == 0) ? cfg : nivel) : nivel;
        git_buf out = GIT_BUF_INIT;
        if (git_config_get_string_buf(&out, busca, k) < 0) { rc = 1; git_buf_dispose(&out); goto fin; }
        printf("%s\n", out.ptr);
        git_buf_dispose(&out);
    }
fin:
    if (cfg && cfg != nivel) git_config_free(cfg);
    if (nivel) git_config_free(nivel);
    return rc;
}

static int ayuda(void)
{
    printf("git (SaguiOS, sobre libgit2) - uso: git [-C carpeta] orden [opciones]\n"
           "  init [carpeta]            crea un repositorio\n"
           "  clone URL [carpeta]       clona (https://, ssh://, usuario@host:ruta, file)\n"
           "  fetch [remoto]            trae lo nuevo del remoto\n"
           "  pull [remoto]             fetch + fusión (avance rápido o fusión sin conflictos)\n"
           "  push [-u] [-f] [remoto [rama]]\n"
           "  status                    estado en formato corto (XY ruta)\n"
           "  add archivo... | .        prepara cambios\n"
           "  commit -m texto [-a]      confirma (sin editor)\n"
           "  log [-n N] [--oneline]    historial\n"
           "  diff [--cached] [--stat]  diferencias\n"
           "  branch [-a] [-d] [nombre] ramas\n"
           "  checkout rama | -b nueva | -- archivos\n"
           "  remote [-v] | add | remove | set-url | get-url | rename\n"
           "  config [--global] clave [valor] | --list | --unset clave\n"
           "Variables: GIT_USERNAME/GIT_PASSWORD (HTTPS), GIT_SSL_NO_VERIFY, GIT_SSL_CAINFO,\n"
           "GIT_SSH (otra orden ssh), GIT_AUTHOR_NAME/EMAIL. Ayuda larga: ayuda git\n");
    return 0;
}

int principal(int argc, char **argv)
{
    int a = 1, rc;
    if (!getenv("HOME")) setenv("HOME", "/home", 1);
    a_terminal = isatty(1);
    while (a < argc && argv[a][0] == '-') {
        if (!strcmp(argv[a], "-C") && a + 1 < argc) {
            if (chdir(argv[a + 1]) < 0) { fprintf(stderr, "git: no se puede entrar en %s\n", argv[a + 1]); return 128; }
            a += 2;
        } else if (!strcmp(argv[a], "--version")) { printf("git versión SaguiOS (libgit2 %s)\n", LIBGIT2_VERSION); return 0; }
        else if (!strcmp(argv[a], "-h") || !strcmp(argv[a], "--help")) return ayuda();
        else { fprintf(stderr, "git: opción global desconocida: %s\n", argv[a]); return 129; }
    }
    if (a >= argc) return ayuda();
    git_libgit2_init();
    git_sagui_registrar_flujos();
    git_libgit2_opts(GIT_OPT_SET_OWNER_VALIDATION, 0);
    const char *o = argv[a];
    int n = argc - a;
    char **v = argv + a;
    if (!strcmp(o, "init")) rc = c_init(n, v);
    else if (!strcmp(o, "clone")) rc = c_clone(n, v);
    else if (!strcmp(o, "fetch")) rc = c_fetch(n, v);
    else if (!strcmp(o, "pull")) rc = c_pull(n, v);
    else if (!strcmp(o, "push")) rc = c_push(n, v);
    else if (!strcmp(o, "status")) rc = c_status(n, v);
    else if (!strcmp(o, "add")) rc = c_add(n, v);
    else if (!strcmp(o, "commit")) rc = c_commit(n, v);
    else if (!strcmp(o, "log")) rc = c_log(n, v);
    else if (!strcmp(o, "diff")) rc = c_diff(n, v);
    else if (!strcmp(o, "branch")) rc = c_branch(n, v);
    else if (!strcmp(o, "checkout") || !strcmp(o, "switch")) rc = c_checkout(n, v);
    else if (!strcmp(o, "remote")) rc = c_remote(n, v);
    else if (!strcmp(o, "config")) rc = c_config(n, v);
    else if (!strcmp(o, "version")) { printf("git versión SaguiOS (libgit2 %s)\n", LIBGIT2_VERSION); rc = 0; }
    else if (!strcmp(o, "help")) rc = ayuda();
    else { fprintf(stderr, "git: '%s' no es una orden de git de SaguiOS (git help)\n", o); rc = 1; }
    if (repo) git_repository_free(repo);
    git_libgit2_shutdown();
    (void)prog;
    fflush(stdout);
    return rc;
}
