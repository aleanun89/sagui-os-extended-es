// SPDX-License-Identifier: GPL-3.0-only
/* sonidos.c - esquemas y eventos de los sonidos del sistema (D421). Ver userland/inc/sonidos.h. */
#include "sagui.h"
#include "sonidos.h"

static const char *const ev_id[SON_EV_N] = {
    "arranque", "apagar", "reiniciar", "aviso", "error", "pregunta", "usb_conectar", "usb_desconectar", "bateria", "papelera",
    /* D484: las ventanas las detecta el vigilante (/bin/sonidos --vigilar); correo y captura los avisan sus programas. */
    "ventana_abrir", "ventana_cerrar", "minimizar", "maximizar", "correo", "captura",
};
/* Con los sonidos activados suenan todos menos el arranque, el apagado y el reinicio (esos retrasan o molestan: se piden). */
static const unsigned char ev_defecto[SON_EV_N] = { 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1 };   /* las de ventana, apagadas: se piden */

const char *sonidos_evento_id(int i) { return i >= 0 && i < SON_EV_N ? ev_id[i] : NULL; }
int sonidos_evento_defecto(int i) { return i >= 0 && i < SON_EV_N ? ev_defecto[i] : 0; }

int sonidos_evento_indice(const char *id)
{
    for (int i = 0; i < SON_EV_N; i++)
        if (compara_texto(id, ev_id[i]) == 0) return i;
    return -1;
}

void sonidos_conf_defecto(sonidos_conf_t *c)
{
    memset(c, 0, sizeof *c);
    c->volumen = 70;
    copia_texto(c->esquema, "saguios", sizeof c->esquema);
    for (int i = 0; i < SON_EV_N; i++) { c->ev[i] = ev_defecto[i]; c->vol[i] = 100; }
}

/* Lee un archivo pequeño entero; < 0 si no existe. */
static int ler_pequeno(const char *arq, char *t, int max)
{
    int fd = abrir(arq, ABRIR_LER);
    if (fd < 0) return -1;
    int n = ler(fd, t, (unsigned)(max - 1));
    fechar(fd);
    if (n < 0) return -1;
    t[n] = 0;
    return n;
}

/* Recorre las líneas clave=valor de t (se modifica); llama a cb(clave, valor, dato). */
static void por_linhas(char *t, void (*cb)(const char *, const char *, void *), void *dato)
{
    for (char *p = t; *p;) {
        char *fim = acha_char(p, '\n');
        if (fim) *fim = 0;
        char *eq = acha_char(p, '=');
        if (p[0] != '#' && eq) {
            *eq = 0;
            size_t tv = tam_texto(eq + 1);
            while (tv && (eq[tv] == '\r' || eq[tv] == ' ')) eq[tv--] = 0;
            cb(p, eq + 1, dato);
        }
        if (!fim) break;
        p = fim + 1;
    }
}

/* Contexto del lector: «base» es la carpeta contra la que se resuelven las rutas relativas (la del archivo leído). */
typedef struct { sonidos_conf_t *c; const char *base; int version; char nombre[32]; } lec_t;

static void cb_conf(const char *k, const char *v, void *d)
{
    lec_t *l = d;
    sonidos_conf_t *c = l->c;
    if (compara_texto(k, "activo") == 0) c->activo = texto_para_int(v) != 0;
    else if (compara_texto(k, "version") == 0) l->version = texto_para_int(v);
    else if (compara_texto(k, "volumen") == 0) { int x = texto_para_int(v); c->volumen = x < 0 ? 0 : x > 100 ? 100 : x; }
    else if (compara_texto(k, "esquema") == 0) copia_texto(c->esquema, v, sizeof c->esquema);
    else if (compara_texto(k, "preset") == 0) copia_texto(c->preset, v, sizeof c->preset);
    else if (compara_texto(k, "nombre") == 0) copia_texto(l->nombre, v, sizeof l->nombre);
    else if (comeca_com(k, "ev_")) {
        int i = sonidos_evento_indice(k + 3);
        if (i >= 0) c->ev[i] = texto_para_int(v) != 0;
    } else if (comeca_com(k, "vol_")) {
        int i = sonidos_evento_indice(k + 4);
        if (i >= 0) { int x = texto_para_int(v); c->vol[i] = (unsigned char)(x < 0 ? 0 : x > 100 ? 100 : x); }
    } else if (comeca_com(k, "asig_")) {
        int i = sonidos_evento_indice(k + 5);
        if (i < 0) return;
        if (v[0] == '/' || !v[0] || compara_texto(v, "-") == 0 || !l->base) copia_texto(c->asig[i], v, sizeof c->asig[i]);
        else formatar_texto(c->asig[i], sizeof c->asig[i], "%s/%s", l->base, v);   /* relativa al archivo */
    }
}

/* Lee el texto de un archivo de configuración o de preset en c (que ya trae los valores de serie). */
static int leer_conf_arq(const char *arq, const char *base, sonidos_conf_t *c, lec_t *l)
{
    static char t[6144];
    if (ler_pequeno(arq, t, sizeof t) < 0) return -1;
    lec_t loc = { c, base, 0, "" };
    por_linhas(t, cb_conf, &loc);
    if (l) *l = loc;
    return 0;
}

int sonidos_conf_ler(sonidos_conf_t *c)
{
    lec_t l;
    sonidos_conf_defecto(c);
    if (leer_conf_arq(SON_ARQ_CONF, NULL, c, &l) < 0) return 1;
    if (l.version < 2) {   /* D484: configuración de la v8.5.5.x: se guarda como preset «anterior» y se pasa al formato nuevo */
        if (!c->preset[0]) copia_texto(c->preset, "anterior", sizeof c->preset);
        sonidos_preset_guardar("anterior", c);
        sonidos_conf_gravar(c);
    }
    return 0;
}

/* Escribe la configuración (o un preset) como texto. rel = carpeta contra la que se hacen relativas las rutas (NULL = absolutas). */
static int conf_texto(const sonidos_conf_t *c, char *o, int max, bool es_conf, const char *nombre, const char *rel)
{
    int n = 0;
    if (es_conf) n += formatar_texto(o + n, (size_t)(max - n), "version=2\nactivo=%d\npreset=%s\n", c->activo ? 1 : 0, c->preset);
    else n += formatar_texto(o + n, (size_t)(max - n), "# Preset de sonidos de SaguiOS (D484)\nnombre=%s\n", nombre);
    n += formatar_texto(o + n, (size_t)(max - n), "volumen=%d\nesquema=%s\n", c->volumen, c->esquema);
    for (int i = 0; i < SON_EV_N; i++) {
        n += formatar_texto(o + n, (size_t)(max - n), "ev_%s=%d\nvol_%s=%d\n", ev_id[i], c->ev[i] ? 1 : 0, ev_id[i], c->vol[i]);
        if (c->asig[i][0]) {
            const char *a = c->asig[i];
            size_t lr = rel ? tam_texto(rel) : 0;
            if (lr && comeca_com(a, rel) && a[lr] == '/') a += lr + 1;
            n += formatar_texto(o + n, (size_t)(max - n), "asig_%s=%s\n", ev_id[i], a);
        }
    }
    return n;
}

static int gravar_texto(const char *arq, const char *o, int n)
{
    int fd = abrir(arq, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int r = escrever_tudo(fd, o, (unsigned)n);
    fechar(fd);
    return r < 0 ? r : 0;
}

int sonidos_conf_gravar(const sonidos_conf_t *c)
{
    static char o[6144];
    int n = conf_texto(c, o, sizeof o, true, NULL, NULL);
    criar_pasta("/home/.config");
    return gravar_texto(SON_ARQ_CONF, o, n);
}

/* El id de esquema solo lleva letras, cifras, «-» y «_» (va dentro de una ruta). */
static bool id_valido(const char *s)
{
    if (!s[0] || tam_texto(s) > 31) return false;
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '-' || *s == '_')) return false;
    return true;
}

typedef struct { const char *evento; char *dst; int max; int hallado; sonidos_esquema_t *e; } busca_t;

static void cb_esquema(const char *k, const char *v, void *d)
{
    busca_t *b = d;
    if (b->e) {
        if (compara_texto(k, "nombre") == 0) copia_texto(b->e->nombre, v, sizeof b->e->nombre);
        else if (compara_texto(k, "creditos") == 0) copia_texto(b->e->creditos, v, sizeof b->e->creditos);
    }
    if (b->evento && compara_texto(k, b->evento) == 0 && v[0] && acha_char(v, '/') == NULL) {
        copia_texto(b->dst, v, (size_t)b->max);
        b->hallado = 1;
    }
}

static bool carpeta_esquema(const char *id, char *dir, int max)
{
    info_arquivo_t inf;
    formatar_texto(dir, (size_t)max, "%s/%s", SON_DIR_SIS, id);
    if (info_arquivo(dir, &inf) == 0) return true;
    formatar_texto(dir, (size_t)max, "%s/%s", SON_DIR_PRIV, id);
    return info_arquivo(dir, &inf) == 0;
}

int sonidos_ruta(const char *esquema, const char *evento, char *dst, int max)
{
    static char t[1024];
    char dir[96], arq[96], nombre[64];
    if (!id_valido(esquema) || compara_texto(esquema, "ninguno") == 0) return ERRO_NAO_EXISTE;
    if (!carpeta_esquema(esquema, dir, sizeof dir)) return ERRO_NAO_EXISTE;
    formatar_texto(arq, sizeof arq, "%s/esquema.conf", dir);
    if (ler_pequeno(arq, t, sizeof t) < 0) return ERRO_NAO_EXISTE;
    busca_t b = { evento, nombre, sizeof nombre, 0, NULL };
    por_linhas(t, cb_esquema, &b);
    if (!b.hallado) return ERRO_NAO_EXISTE;
    formatar_texto(dst, (size_t)max, "%s/%s", dir, nombre);
    return 0;
}

static int esquemas_de(const char *base, bool privado, sonidos_esquema_t *v, int n, int max)
{
    static char t[1024];
    entrada_pasta_t e;
    for (int i = 0; n < max && listar(base, i, &e) > 0; i++) {
        char arq[128];
        if (e.tipo != TIPO_PASTA || !id_valido(e.nome) || compara_texto(e.nome, "ninguno") == 0) continue;
        bool dup = false;
        for (int k = 0; k < n; k++) if (compara_texto(v[k].id, e.nome) == 0) dup = true;
        if (dup) continue;
        formatar_texto(arq, sizeof arq, "%s/%s/esquema.conf", base, e.nome);
        if (ler_pequeno(arq, t, sizeof t) < 0) continue;
        sonidos_esquema_t *s = &v[n];
        memset(s, 0, sizeof *s);
        copia_texto(s->id, e.nome, sizeof s->id);
        copia_texto(s->nombre, e.nome, sizeof s->nombre);
        s->privado = privado;
        busca_t b = { NULL, NULL, 0, 0, s };
        por_linhas(t, cb_esquema, &b);
        n++;
    }
    return n;
}

int sonidos_esquemas(sonidos_esquema_t *v, int max)
{
    int n = esquemas_de(SON_DIR_SIS, false, v, 0, max);
    return esquemas_de(SON_DIR_PRIV, true, v, n, max);
}

/* ---- un sonido por acción (D484) ---- */

int sonidos_ruta_accion(const sonidos_conf_t *c, int i, char *dst, int max)
{
    if (i < 0 || i >= SON_EV_N) return ERRO_INVALIDO;
    const char *a = c->asig[i];
    if (compara_texto(a, "-") == 0) return ERRO_NAO_EXISTE;
    if (a[0]) {
        info_arquivo_t inf;
        if (info_arquivo(a, &inf) < 0) return ERRO_NAO_EXISTE;   /* el archivo propio ya no está */
        copia_texto(dst, a, (size_t)max);
        return 0;
    }
    if (!c->esquema[0] || compara_texto(c->esquema, "ninguno") == 0) return ERRO_NAO_EXISTE;
    return sonidos_ruta(c->esquema, ev_id[i], dst, max);
}

int sonidos_volumen_accion(const sonidos_conf_t *c, int i)
{
    int v = c->volumen * (i >= 0 && i < SON_EV_N ? c->vol[i] : 100) / 100;
    return v < 0 ? 0 : v > 100 ? 100 : v;
}

/* ---- presets (D484) ---- */

int sonidos_nombre_valido(const char *n) { return id_valido(n) ? 1 : 0; }

static void ruta_preset(const char *nombre, char *dst, int max) { formatar_texto(dst, (size_t)max, "%s/%s.sonidos", SON_DIR_PRESETS, nombre); }

static int copiar_arq(const char *orig, const char *dest)
{
    static unsigned char buf[4096];
    if (compara_texto(orig, dest) == 0) return 0;
    int a = abrir(orig, ABRIR_LER);
    if (a < 0) return a;
    int b = abrir(dest, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (b < 0) { fechar(a); return b; }
    int r = 0, n;
    while ((n = ler(a, buf, sizeof buf)) > 0)
        if (escrever_tudo(b, buf, (unsigned)n) < 0) { r = -1; break; }
    fechar(b);
    fechar(a);
    return r;
}

static bool es_de_sistema(const char *r) { return comeca_com(r, SON_DIR_SIS "/") || comeca_com(r, SON_DIR_PRIV "/"); }

/* Escribe un preset en arq: los audios propios (los que no son de un esquema instalado) se copian a dir_audio como
 * «<base>.<evento>.<ext>» y se citan con ruta relativa a rel. */
static int escribir_preset(const sonidos_conf_t *c, const char *nombre, const char *arq, const char *dir_audio, const char *base, const char *rel)
{
    static sonidos_conf_t t;
    static char o[6144];
    t = *c;
    criar_pasta(dir_audio);
    for (int i = 0; i < SON_EV_N; i++) {
        const char *a = c->asig[i];
        if (!a[0] || compara_texto(a, "-") == 0 || es_de_sistema(a)) continue;
        const char *pt = acha_ultimo_char(a, '.');
        char ext[8] = "";
        if (pt && tam_texto(pt) <= 6 && !acha_char(pt, '/')) copia_texto(ext, pt, sizeof ext);
        char dst[SON_RUTA_MAX];
        formatar_texto(dst, sizeof dst, "%s/%s.%s%s", dir_audio, base, ev_id[i], ext);
        if (copiar_arq(a, dst) < 0) return -3;
        copia_texto(t.asig[i], dst, sizeof t.asig[i]);
    }
    int n = conf_texto(&t, o, sizeof o, false, nombre, rel);
    return gravar_texto(arq, o, n);
}

static bool es_esquema(const char *n)
{
    static sonidos_esquema_t v[16];
    int k = sonidos_esquemas(v, 16);
    for (int i = 0; i < k; i++) if (compara_texto(v[i].id, n) == 0) return true;
    return compara_texto(n, "ninguno") == 0;
}

int sonidos_preset_guardar(const char *nombre, const sonidos_conf_t *c)
{
    char arq[96], dir_a[96];
    if (!id_valido(nombre)) return -1;
    if (es_esquema(nombre)) return -2;
    criar_pasta("/home/.config");
    criar_pasta(SON_DIR_PRESETS);
    ruta_preset(nombre, arq, sizeof arq);
    formatar_texto(dir_a, sizeof dir_a, "%s/audio", SON_DIR_PRESETS);
    return escribir_preset(c, nombre, arq, dir_a, nombre, SON_DIR_PRESETS);
}

int sonidos_preset_cargar(const char *nombre, sonidos_conf_t *c)
{
    static sonidos_conf_t t;
    char arq[96];
    sonidos_conf_defecto(&t);
    if (es_esquema(nombre)) {
        copia_texto(t.esquema, nombre, sizeof t.esquema);
        t.volumen = c->volumen;
    } else {
        if (!id_valido(nombre)) return -1;
        ruta_preset(nombre, arq, sizeof arq);
        if (leer_conf_arq(arq, SON_DIR_PRESETS, &t, NULL) < 0) return ERRO_NAO_EXISTE;
    }
    t.activo = c->activo;
    copia_texto(t.preset, nombre, sizeof t.preset);
    *c = t;
    return 0;
}

int sonidos_preset_borrar(const char *nombre)
{
    char arq[96], dir_a[96];
    entrada_pasta_t e;
    if (!id_valido(nombre) || es_esquema(nombre)) return -1;
    ruta_preset(nombre, arq, sizeof arq);
    if (apagar(arq) < 0) return ERRO_NAO_EXISTE;
    formatar_texto(dir_a, sizeof dir_a, "%s/audio", SON_DIR_PRESETS);
    char pre[40];
    formatar_texto(pre, sizeof pre, "%s.", nombre);
    for (int i = 0; listar(dir_a, i, &e) > 0;) {
        if (e.tipo != TIPO_PASTA && comeca_com(e.nome, pre)) {
            char f[128];
            formatar_texto(f, sizeof f, "%s/%s", dir_a, e.nome);
            apagar(f);
            i = 0;   /* la lista cambió: se vuelve a empezar */
        } else i++;
    }
    return 0;
}

int sonidos_preset_renombrar(const char *viejo, const char *nuevo)
{
    static sonidos_conf_t t;
    if (!id_valido(nuevo)) return -1;
    if (es_esquema(nuevo) || es_esquema(viejo)) return -2;
    if (compara_texto(viejo, nuevo) == 0) return 0;
    char arq[96];
    ruta_preset(nuevo, arq, sizeof arq);
    info_arquivo_t inf;
    if (info_arquivo(arq, &inf) == 0) return -4;   /* ya existe */
    t.volumen = 70;
    int r = sonidos_preset_cargar(viejo, &t);
    if (r < 0) return r;
    r = sonidos_preset_guardar(nuevo, &t);
    if (r < 0) return r;
    return sonidos_preset_borrar(viejo);
}

int sonidos_preset_exportar(const char *nombre, const char *destino)
{
    static sonidos_conf_t t;
    char dir[96], base[48];
    t.volumen = 70;
    int r = sonidos_preset_cargar(nombre, &t);
    if (r < 0) return r;
    copia_texto(dir, destino, sizeof dir);
    char *b = acha_ultimo_char(dir, '/');
    if (!b) return -1;
    copia_texto(base, b + 1, sizeof base);
    if (b == dir) dir[1] = 0; else *b = 0;
    char *pt = acha_ultimo_char(base, '.');
    if (pt) *pt = 0;
    if (!base[0]) return -1;
    return escribir_preset(&t, nombre, destino, dir, base, dir);
}

int sonidos_preset_importar(const char *origen, char *nombre_out, int max)
{
    static sonidos_conf_t t;
    static lec_t l;
    char dir[96], nombre[48];
    copia_texto(dir, origen, sizeof dir);
    char *b = acha_ultimo_char(dir, '/');
    if (!b) return -1;
    copia_texto(nombre, b + 1, sizeof nombre);
    if (b == dir) dir[1] = 0; else *b = 0;
    sonidos_conf_defecto(&t);
    if (leer_conf_arq(origen, dir, &t, &l) < 0) return ERRO_NAO_EXISTE;
    if (l.nombre[0]) copia_texto(nombre, l.nombre, sizeof nombre);
    else { char *pt = acha_ultimo_char(nombre, '.'); if (pt) *pt = 0; }
    if (!id_valido(nombre)) return -1;
    if (es_esquema(nombre)) formatar_texto(nombre, sizeof nombre, "%s_2", nombre);
    char arq[96];
    info_arquivo_t inf;
    for (int k = 2; k < 10; k++) {   /* si ya hay uno con ese nombre, no se pisa */
        ruta_preset(nombre, arq, sizeof arq);
        if (info_arquivo(arq, &inf) < 0) break;
        char *u = acha_ultimo_char(nombre, '_');
        if (u && u[1] >= '2' && u[1] <= '9' && !u[2]) *u = 0;
        formatar_texto(nombre + tam_texto(nombre), sizeof nombre - tam_texto(nombre), "_%d", k);
    }
    t.activo = 0;
    int r = sonidos_preset_guardar(nombre, &t);
    if (r < 0) return r;
    if (nombre_out) copia_texto(nombre_out, nombre, (size_t)max);
    return 0;
}

int sonidos_presets(sonidos_preset_t *v, int max)
{
    static sonidos_esquema_t es[16];
    entrada_pasta_t e;
    int n = 0, k = sonidos_esquemas(es, 16);
    for (int i = 0; i < k && n < max; i++) {
        copia_texto(v[n].nombre, es[i].id, sizeof v[n].nombre);
        v[n++].esquema = true;
    }
    for (int i = 0; n < max && listar(SON_DIR_PRESETS, i, &e) > 0; i++) {
        size_t l = tam_texto(e.nome);
        if (e.tipo == TIPO_PASTA || l < 9 || compara_texto(e.nome + l - 8, ".sonidos") != 0) continue;
        char nm[40];
        copia_texto(nm, e.nome, l - 7 < sizeof nm ? l - 7 : sizeof nm);
        if (!id_valido(nm)) continue;
        copia_texto(v[n].nombre, nm, sizeof v[n].nombre);
        v[n++].esquema = false;
    }
    return n;
}

/* Las acciones de ventana pueden llegar a ráfagas: entre dos sonidos de la misma clase, un mínimo de milisegundos. */
static unsigned min_ms_evento(int i) { return i >= 10 && i <= 13 ? 350 : 0; }

static int lanzar(const char *evento, unsigned max_ms)
{
    static sonidos_conf_t c;
    static unsigned ultimo[SON_EV_N];
    char ruta[SON_RUTA_MAX], ms[16];
    int i = sonidos_evento_indice(evento);
    if (i < 0) return 0;
    sonidos_conf_ler(&c);
    if (!c.activo || !c.ev[i] || !sonidos_volumen_accion(&c, i)) return 0;
    if (sonidos_ruta_accion(&c, i, ruta, sizeof ruta) < 0) return 0;   /* sin sonido para esta acción */
    unsigned ahora = tempo_ms();
    if (min_ms_evento(i) && ultimo[i] && ahora - ultimo[i] < min_ms_evento(i)) return 0;
    ultimo[i] = ahora ? ahora : 1;
    int codigo;
    while (recolher(&codigo) > 0) { }   /* los sonidos anteriores ya terminados */
    formatar_texto(ms, sizeof ms, "%u", max_ms);
    char *av[] = { "sonidos", "--tocar", (char *)evento, "--max", ms, NULL };
    int pid = executar("/bin/sonidos", av);
    return pid > 0 ? pid : 0;
}

int sonido_evento(const char *evento) { return lanzar(evento, SON_ARRANQUE_MAX_MS); }

int sonido_evento_esperar(const char *evento, unsigned max_ms)
{
    int pid = lanzar(evento, max_ms);
    if (pid <= 0) return 0;
    unsigned t0 = tempo_ms();
    int estado;
    while (esperar_pid(pid, &estado, ESPERA_SEM_BLOQUEAR) == 0) {
        if (tempo_ms() - t0 > max_ms + 500) { matar(pid); esperar_pid(pid, &estado, 0); break; }
        dormir(20);
    }
    return pid;
}
