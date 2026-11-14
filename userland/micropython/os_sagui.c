/* os_sagui.c - los módulos os y os.path sobre las llamadas de SaguiOS.
 *
 * No es el extmod/modos.c de MicroPython: aquel habla con su VFS (ver
 * D067 em arquivos_sagui.c) y no tiene environ, system ni os.path. Aquí:
 *   listdir, ilistdir, mkdir, makedirs, rmdir, remove/unlink, rename,
 *   getcwd, chdir, stat, urandom, getenv, putenv, unsetenv, environ,
 *   system, getpid, uname, sep, linesep, curdir, pardir, extsep e o
 *   submódulo path (join, exists, isdir, isfile, basename, dirname, split,
 *   splitext, abspath, normpath, isabs, getsize, expanduser).
 *
 * Los nombres de archivo llegan de Python en UTF-8 y van al kernel en Latin-1
 * (y vuelven convertidos), como el resto del sistema (D068). */
#include <string.h>
#include "py/runtime.h"
#include "py/objstr.h"
#include "py/objtuple.h"
#include "py/mperrno.h"
#include "sagui_mp.h"

#define CAMINHO_MAX 256
#define S_PASTA   0x4000   /* st_mode, como en Unix */
#define S_ARQUIVO 0x8000

int meu_pid(void);
int info_sistema(int tipo, void *buf, unsigned n);

/* Constantes de texto con caracteres que no caben en un nombre de qstr. */
static MP_DEFINE_STR_OBJ(str_ponto, ".");
static MP_DEFINE_STR_OBJ(str_ponto_ponto, "..");
static MP_DEFINE_STR_OBJ(str_os_path, "os.path");
static MP_DEFINE_STR_OBJ(str_casa, "/home");

static void caminho_de(mp_obj_t o, char *c)
{
    sagui_str_para_latin1(o, c, CAMINHO_MAX);
}

/* Una llamada que devuelve <0 en error se convierte en OSError con la ruta en el texto. */
static mp_obj_t conferir(int r, mp_obj_t caminho)
{
    if (r < 0) sagui_levantar_obj(r, caminho);
    return mp_const_none;
}

/* ------------------------------------------------------------ carpetas */

static mp_obj_t os_getcwd(void)
{
    char c[CAMINHO_MAX];
    int r = pasta_atual(c, sizeof c);
    if (r < 0) sagui_levantar(r, NULL);
    return sagui_str_de_latin1(c);
}
static MP_DEFINE_CONST_FUN_OBJ_0(os_getcwd_obj, os_getcwd);

static mp_obj_t os_chdir(mp_obj_t p)
{
    char c[CAMINHO_MAX];
    caminho_de(p, c);
    return conferir(mudar_pasta(c), p);
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_chdir_obj, os_chdir);

static mp_obj_t os_mkdir(size_t n_args, const mp_obj_t *args)
{
    char c[CAMINHO_MAX];
    caminho_de(args[0], c);   /* el modo (permisos) no existe en SaguiOS */
    return conferir(criar_pasta(c), args[0]);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(os_mkdir_obj, 1, 2, os_mkdir);

/* makedirs(caminho, exist_ok=False): crea las carpetas que faltan en la ruta. */
static mp_obj_t os_makedirs(size_t n_args, const mp_obj_t *pos, mp_map_t *kw)
{
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_name, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_mode, MP_ARG_INT, {.u_int = 0777} },
        { MP_QSTR_exist_ok, MP_ARG_BOOL, {.u_bool = false} },
    };
    mp_arg_val_t a[3];
    mp_arg_parse_all(n_args, pos, kw, 3, permitidos, a);
    char c[CAMINHO_MAX];
    size_t n = sagui_str_para_latin1(a[0].u_obj, c, sizeof c);
    while (n > 1 && c[n - 1] == '/') c[--n] = 0;
    for (size_t i = 1; i <= n; i++) {
        if (c[i] != '/' && c[i] != 0) continue;
        char guarda = c[i];
        c[i] = 0;
        int r = criar_pasta(c);
        c[i] = guarda;
        if (r == ERRO_JA_EXISTE) {
            info_arquivo_t inf;
            if (info_arquivo(c, &inf) == 0 && inf.tipo != TIPO_PASTA) r = ERRO_NAO_E_PASTA;
            else if (i < n || a[2].u_bool) r = 0;
        }
        if (r < 0) sagui_levantar_obj(r, a[0].u_obj);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(os_makedirs_obj, 1, os_makedirs);

static mp_obj_t os_rmdir(mp_obj_t p)
{
    char c[CAMINHO_MAX];
    caminho_de(p, c);
    return conferir(remover_pasta(c), p);
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_rmdir_obj, os_rmdir);

static mp_obj_t os_remove(mp_obj_t p)
{
    char c[CAMINHO_MAX];
    caminho_de(p, c);
    info_arquivo_t i;
    /* apagar() también quita carpetas vacías; el remove de Python no. */
    if (info_arquivo(c, &i) == 0 && i.tipo == TIPO_PASTA) sagui_levantar_obj(ERRO_E_PASTA, p);
    return conferir(apagar(c), p);
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_remove_obj, os_remove);

static mp_obj_t os_rename(mp_obj_t de, mp_obj_t para)
{
    char a[CAMINHO_MAX], b[CAMINHO_MAX];
    caminho_de(de, a);
    caminho_de(para, b);
    return conferir(renomear(a, b), de);
}
static MP_DEFINE_CONST_FUN_OBJ_2(os_rename_obj, os_rename);

/* Las entradas de una carpeta; f(nome, tipo, tamanho) para cada una. */
static mp_obj_t lista_pasta(size_t n_args, const mp_obj_t *args, bool detalhes)
{
    char c[CAMINHO_MAX];
    mp_obj_t p = n_args ? args[0] : MP_OBJ_FROM_PTR(&str_ponto);
    caminho_de(p, c);
    info_arquivo_t inf;
    int r = info_arquivo(c, &inf);
    if (r < 0) sagui_levantar_obj(r, p);
    if (inf.tipo != TIPO_PASTA) sagui_levantar_obj(ERRO_NAO_E_PASTA, p);
    bool bytes = mp_obj_is_type(p, &mp_type_bytes);
    mp_obj_t lista = mp_obj_new_list(0, NULL);
    entrada_pasta_t e;
    for (int i = 0; (r = listar(c, i, &e)) == 1; i++) {
        mp_obj_t nome = bytes ? mp_obj_new_bytes((const byte *)e.nome, strlen(e.nome)) : sagui_str_de_latin1(e.nome);
        if (detalhes) {
            mp_obj_t t[4] = {
                nome,
                MP_OBJ_NEW_SMALL_INT(e.tipo == TIPO_PASTA ? S_PASTA : S_ARQUIVO),
                MP_OBJ_NEW_SMALL_INT(0),
                mp_obj_new_int_from_uint(e.tamanho),
            };
            nome = mp_obj_new_tuple(4, t);
        }
        mp_obj_list_append(lista, nome);
    }
    if (r < 0) sagui_levantar_obj(r, p);
    return lista;
}

static mp_obj_t os_listdir(size_t n_args, const mp_obj_t *args)
{
    return lista_pasta(n_args, args, false);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(os_listdir_obj, 0, 1, os_listdir);

/* ilistdir (de MicroPython): (nome, tipo, inode, tamanho), tipo 0x4000 =
 * carpeta y 0x8000 = archivo. La carpeta se lee de una vez (es barato). */
static mp_obj_t os_ilistdir(size_t n_args, const mp_obj_t *args)
{
    return mp_getiter(lista_pasta(n_args, args, true), NULL);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(os_ilistdir_obj, 0, 1, os_ilistdir);

/* stat: SaguiOS solo guarda el tipo y el tamaño; lo demás vale 0. El
 * resultado se lee por índice (st[6]) o por nombre (st.st_size). */
static const qstr campos_stat[] = {
    MP_QSTR_st_mode, MP_QSTR_st_ino, MP_QSTR_st_dev, MP_QSTR_st_nlink, MP_QSTR_st_uid,
    MP_QSTR_st_gid, MP_QSTR_st_size, MP_QSTR_st_atime, MP_QSTR_st_mtime, MP_QSTR_st_ctime,
};

static mp_obj_t os_stat(mp_obj_t p)
{
    char c[CAMINHO_MAX];
    caminho_de(p, c);
    info_arquivo_t i;
    int r = info_arquivo(c, &i);
    if (r < 0) sagui_levantar_obj(r, p);
    mp_obj_t t[10];
    for (int k = 0; k < 10; k++) t[k] = MP_OBJ_NEW_SMALL_INT(0);
    t[0] = MP_OBJ_NEW_SMALL_INT(i.tipo == TIPO_PASTA ? (S_PASTA | 0755) : (S_ARQUIVO | 0644));
    t[3] = MP_OBJ_NEW_SMALL_INT(1);
    t[6] = mp_obj_new_int_from_uint(i.tamanho);
    return mp_obj_new_attrtuple(campos_stat, 10, t);
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_stat_obj, os_stat);

/* ------------------------------------------------------------ varios */

static mp_obj_t os_urandom(mp_obj_t n_in)
{
    mp_int_t n = mp_obj_get_int(n_in);
    if (n < 0) mp_raise_ValueError(NULL);
    vstr_t v;
    vstr_init_len(&v, n);
    for (mp_int_t k = 0; k < n;) {
        int t = n - k > 4096 ? 4096 : n - k;   /* el kernel da hasta 4096 por llamada */
        int r = bytes_aleatorios(v.buf + k, t);
        if (r <= 0) sagui_levantar(r < 0 ? r : ERRO_INVALIDO, NULL);
        k += r;
    }
    return mp_obj_new_bytes_from_vstr(&v);
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_urandom_obj, os_urandom);

static mp_obj_t os_getpid(void) { return MP_OBJ_NEW_SMALL_INT(meu_pid()); }
static MP_DEFINE_CONST_FUN_OBJ_0(os_getpid_obj, os_getpid);

static const qstr campos_uname[] = { MP_QSTR_sysname, MP_QSTR_nodename, MP_QSTR_release, MP_QSTR_version, MP_QSTR_machine };

static mp_obj_t os_uname(void)
{
    info_versao_t v;
    memset(&v, 0, sizeof v);
    info_sistema(INFO_VERSAO, &v, sizeof v);
    v.versao[sizeof v.versao - 1] = 0;
    mp_obj_t t[5] = {
        MP_OBJ_NEW_QSTR(MP_QSTR_SaguiOS), MP_OBJ_NEW_QSTR(MP_QSTR_sagui), sagui_str_de_latin1(v.versao),
        MP_OBJ_NEW_QSTR(MP_QSTR_MicroPython), MP_OBJ_NEW_QSTR(MP_QSTR_i686),
    };
    return mp_obj_new_attrtuple(campos_uname, 5, t);
}
static MP_DEFINE_CONST_FUN_OBJ_0(os_uname_obj, os_uname);

/* ------------------------------------------------------------ entorno */

/* os.environ es un dict común, creado la primera vez a partir del environ
 * que entregó el kernel (D037). getenv/putenv/unsetenv lo modifican, y
 * system() lo pasa adelante: así os.environ["X"] = "1" vale para los
 * programas que llame el script, como en CPython. */
MP_REGISTER_ROOT_POINTER(mp_obj_t sagui_environ);

static mp_obj_t ambiente(void)
{
    if (MP_STATE_VM(sagui_environ) == MP_OBJ_NULL) {
        mp_obj_t d = mp_obj_new_dict(0);
        for (char **e = environ; e && *e; e++) {
            const char *igual = strchr(*e, '=');
            if (!igual) continue;
            vstr_t k;
            vstr_init(&k, igual - *e + 2);
            sagui_latin1_para_vstr(&k, *e, igual - *e);
            mp_obj_dict_store(d, mp_obj_new_str_from_vstr(&k), sagui_str_de_latin1(igual + 1));
        }
        MP_STATE_VM(sagui_environ) = d;
    }
    return MP_STATE_VM(sagui_environ);
}

char **sagui_ambiente(void)
{
    mp_map_t *m = mp_obj_dict_get_map(ambiente());
    char **v = m_new(char *, m->used + 1);
    size_t k = 0;
    for (size_t i = 0; i < m->alloc; i++) {
        if (!mp_map_slot_is_filled(m, i)) continue;
        char nome[128], valor[512];
        size_t a = sagui_str_para_latin1(mp_obj_str_make_new(&mp_type_str, 1, 0, &m->table[i].key), nome, sizeof nome);
        size_t b = sagui_str_para_latin1(mp_obj_str_make_new(&mp_type_str, 1, 0, &m->table[i].value), valor, sizeof valor);
        char *s = m_new(char, a + b + 2);
        memcpy(s, nome, a);
        s[a] = '=';
        memcpy(s + a + 1, valor, b + 1);
        v[k++] = s;
    }
    v[k] = NULL;
    return v;
}

static mp_obj_t os_getenv(size_t n_args, const mp_obj_t *args)
{
    mp_map_elem_t *e = mp_map_lookup(mp_obj_dict_get_map(ambiente()), args[0], MP_MAP_LOOKUP);
    if (e) return e->value;
    return n_args > 1 ? args[1] : mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(os_getenv_obj, 1, 2, os_getenv);

static mp_obj_t os_putenv(mp_obj_t k, mp_obj_t v)
{
    mp_obj_dict_store(ambiente(), k, v);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(os_putenv_obj, os_putenv);

static mp_obj_t os_unsetenv(mp_obj_t k)
{
    mp_map_lookup(mp_obj_dict_get_map(ambiente()), k, MP_MAP_LOOKUP_REMOVE_IF_FOUND);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_unsetenv_obj, os_unsetenv);

/* system(comando): ejecuta en sagsh ("sagsh -c comando") con el entorno de
 * os.environ y espera; devuelve el código de salida. El hijo escribe directo en
 * mismo terminal. */
static mp_obj_t os_system(mp_obj_t cmd)
{
    size_t n;
    mp_obj_str_get_data(cmd, &n);
    char *c = m_new(char, n + 1);
    sagui_str_para_latin1(cmd, c, n + 1);
    char *argv[] = { "sagsh", "-c", c, NULL };
    int pid = executar_env("/bin/sagsh", argv, sagui_ambiente());
    if (pid < 0) sagui_levantar(pid, "/bin/sagsh");
    int codigo = 0;
    int r = SEM_GIL(esperar(pid, &codigo));
    if (r < 0) sagui_levantar(r, NULL);
    return MP_OBJ_NEW_SMALL_INT(codigo);
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_system_obj, os_system);

/* Atributos calculados del módulo (su diccionario queda en ROM). Para un
 * nombre desconocido devuelve MP_OBJ_NULL, que el module_attr de MicroPython
 * entiende como "no existe" (el import busca __init__ así, sin error). */
static mp_obj_t os_getattr(mp_obj_t nome)
{
    if (MP_OBJ_QSTR_VALUE(nome) == MP_QSTR_environ) return ambiente();
    return MP_OBJ_NULL;
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_getattr_obj, os_getattr);

/* ------------------------------------------------------------ os.path */

static const char *dados(mp_obj_t s, size_t *n) { return mp_obj_str_get_data(s, n); }
static mp_obj_t novo(mp_obj_t modelo, const char *s, size_t n)
{
    return mp_obj_new_str_of_type(mp_obj_get_type(modelo), (const byte *)s, n);
}

static mp_obj_t path_join(size_t n_args, const mp_obj_t *args)
{
    vstr_t v;
    vstr_init(&v, 32);
    for (size_t i = 0; i < n_args; i++) {
        size_t n;
        const char *s = dados(args[i], &n);
        if (n && s[0] == '/') vstr_reset(&v);   /* absoluto: vuelve a empezar */
        else if (v.len && v.buf[v.len - 1] != '/') vstr_add_byte(&v, '/');
        vstr_add_strn(&v, s, n);
    }
    return novo(args[0], v.buf, v.len);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR(path_join_obj, 1, path_join);

/* Posición justo después de la última '/' (0 si no hay). */
static size_t pos_nome(const char *s, size_t n)
{
    while (n && s[n - 1] != '/') n--;
    return n;
}

static mp_obj_t path_split(mp_obj_t p)
{
    size_t n;
    const char *s = dados(p, &n);
    size_t k = pos_nome(s, n);
    size_t d = k;   /* quita las barras del final de la carpeta, salvo si es solo "/" */
    while (d > 1 && s[d - 1] == '/') d--;
    mp_obj_t t[2] = { novo(p, s, d), novo(p, s + k, n - k) };
    return mp_obj_new_tuple(2, t);
}
static MP_DEFINE_CONST_FUN_OBJ_1(path_split_obj, path_split);

static mp_obj_t path_basename(mp_obj_t p)
{
    size_t n;
    const char *s = dados(p, &n);
    size_t k = pos_nome(s, n);
    return novo(p, s + k, n - k);
}
static MP_DEFINE_CONST_FUN_OBJ_1(path_basename_obj, path_basename);

static mp_obj_t path_dirname(mp_obj_t p)
{
    return mp_obj_subscr(path_split(p), MP_OBJ_NEW_SMALL_INT(0), MP_OBJ_SENTINEL);
}
static MP_DEFINE_CONST_FUN_OBJ_1(path_dirname_obj, path_dirname);

/* splitext("a/b.tar.gz") = ("a/b.tar", ".gz"); un punto al principio del nombre
 * (".bashrc") no es extensión. */
static mp_obj_t path_splitext(mp_obj_t p)
{
    size_t n;
    const char *s = dados(p, &n);
    size_t ini = pos_nome(s, n), k = n;
    while (k > ini && s[k - 1] != '.') k--;
    size_t primeiro = ini;
    while (primeiro < n && s[primeiro] == '.') primeiro++;
    if (k <= ini || k - 1 < primeiro) k = n + 1;   /* sin extensión */
    mp_obj_t t[2];
    if (k > n) { t[0] = p; t[1] = novo(p, "", 0); }
    else { t[0] = novo(p, s, k - 1); t[1] = novo(p, s + k - 1, n - k + 1); }
    return mp_obj_new_tuple(2, t);
}
static MP_DEFINE_CONST_FUN_OBJ_1(path_splitext_obj, path_splitext);

static int tipo_de(mp_obj_t p)
{
    char c[CAMINHO_MAX];
    size_t n;
    mp_obj_str_get_data(p, &n);
    if (n >= CAMINHO_MAX) return 0;
    caminho_de(p, c);
    info_arquivo_t i;
    return info_arquivo(c, &i) < 0 ? 0 : (int)i.tipo;
}

static mp_obj_t path_exists(mp_obj_t p) { return mp_obj_new_bool(tipo_de(p) != 0); }
static MP_DEFINE_CONST_FUN_OBJ_1(path_exists_obj, path_exists);
static mp_obj_t path_isdir(mp_obj_t p) { return mp_obj_new_bool(tipo_de(p) == TIPO_PASTA); }
static MP_DEFINE_CONST_FUN_OBJ_1(path_isdir_obj, path_isdir);
static mp_obj_t path_isfile(mp_obj_t p) { int t = tipo_de(p); return mp_obj_new_bool(t && t != TIPO_PASTA); }
static MP_DEFINE_CONST_FUN_OBJ_1(path_isfile_obj, path_isfile);

static mp_obj_t path_isabs(mp_obj_t p)
{
    size_t n;
    const char *s = dados(p, &n);
    return mp_obj_new_bool(n && s[0] == '/');
}
static MP_DEFINE_CONST_FUN_OBJ_1(path_isabs_obj, path_isabs);

static mp_obj_t path_getsize(mp_obj_t p)
{
    return mp_obj_subscr(os_stat(p), MP_OBJ_NEW_SMALL_INT(6), MP_OBJ_SENTINEL);
}
static MP_DEFINE_CONST_FUN_OBJ_1(path_getsize_obj, path_getsize);

/* normpath: quita "//", "." y resuelve ".." sin mirar el disco. */
static mp_obj_t path_normpath(mp_obj_t p)
{
    size_t n;
    const char *s = dados(p, &n);
    bool abs = n && s[0] == '/';
    size_t *ini = m_new(size_t, n / 2 + 2);   /* donde empieza cada parte ya aceptada, en v */
    size_t partes = 0;
    vstr_t v;
    vstr_init(&v, n + 1);
    if (abs) vstr_add_byte(&v, '/');
    for (size_t i = 0; i < n;) {
        while (i < n && s[i] == '/') i++;
        size_t j = i;
        while (j < n && s[j] != '/') j++;
        size_t t = j - i;
        if (t == 0 || (t == 1 && s[i] == '.')) { i = j; continue; }
        if (t == 2 && s[i] == '.' && s[i + 1] == '.') {
            if (partes && !(v.len - ini[partes - 1] == 2 && v.buf[ini[partes - 1]] == '.' && v.buf[ini[partes - 1] + 1] == '.')) {
                v.len = ini[--partes];
                if (v.len > (abs ? 1u : 0u)) v.len--;   /* la '/' antes de la parte */
                i = j;
                continue;
            }
            if (abs) { i = j; continue; }   /* "/.." = "/" */
        }
        if (v.len > (abs ? 1u : 0u)) vstr_add_byte(&v, '/');
        ini[partes++] = v.len;
        vstr_add_strn(&v, s + i, t);
        i = j;
    }
    if (v.len == 0) vstr_add_byte(&v, '.');
    return novo(p, v.buf, v.len);
}
static MP_DEFINE_CONST_FUN_OBJ_1(path_normpath_obj, path_normpath);

static mp_obj_t path_abspath(mp_obj_t p)
{
    size_t n;
    const char *s = dados(p, &n);
    if (n && s[0] == '/') return path_normpath(p);
    mp_obj_t a[2] = { os_getcwd(), p };
    return path_normpath(path_join(2, a));
}
static MP_DEFINE_CONST_FUN_OBJ_1(path_abspath_obj, path_abspath);

/* expanduser("~/x"): SaguiOS tiene un solo usuario, con la casa en /home. */
static mp_obj_t path_expanduser(mp_obj_t p)
{
    size_t n;
    const char *s = dados(p, &n);
    if (!n || s[0] != '~' || (n > 1 && s[1] != '/')) return p;
    mp_obj_t casa = os_getenv(1, (mp_obj_t[]){ MP_OBJ_NEW_QSTR(MP_QSTR_HOME) });
    if (casa == mp_const_none) casa = MP_OBJ_FROM_PTR(&str_casa);
    size_t k;
    const char *c = dados(casa, &k);
    while (k && c[k - 1] == '/') k--;   /* HOME=/ (el predeterminado de sagsh): "~/x" = "/x" */
    vstr_t v;
    vstr_init(&v, k + n);
    vstr_add_strn(&v, c, k);
    vstr_add_strn(&v, s + 1, n - 1);
    if (v.len == 0) vstr_add_byte(&v, '/');
    return mp_obj_new_str_from_vstr(&v);
}
static MP_DEFINE_CONST_FUN_OBJ_1(path_expanduser_obj, path_expanduser);

static const mp_rom_map_elem_t path_globais_tab[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_PTR(&str_os_path) },
    { MP_ROM_QSTR(MP_QSTR_sep), MP_ROM_QSTR(MP_QSTR__slash_) },
    { MP_ROM_QSTR(MP_QSTR_join), MP_ROM_PTR(&path_join_obj) },
    { MP_ROM_QSTR(MP_QSTR_split), MP_ROM_PTR(&path_split_obj) },
    { MP_ROM_QSTR(MP_QSTR_splitext), MP_ROM_PTR(&path_splitext_obj) },
    { MP_ROM_QSTR(MP_QSTR_basename), MP_ROM_PTR(&path_basename_obj) },
    { MP_ROM_QSTR(MP_QSTR_dirname), MP_ROM_PTR(&path_dirname_obj) },
    { MP_ROM_QSTR(MP_QSTR_exists), MP_ROM_PTR(&path_exists_obj) },
    { MP_ROM_QSTR(MP_QSTR_isdir), MP_ROM_PTR(&path_isdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_isfile), MP_ROM_PTR(&path_isfile_obj) },
    { MP_ROM_QSTR(MP_QSTR_isabs), MP_ROM_PTR(&path_isabs_obj) },
    { MP_ROM_QSTR(MP_QSTR_getsize), MP_ROM_PTR(&path_getsize_obj) },
    { MP_ROM_QSTR(MP_QSTR_normpath), MP_ROM_PTR(&path_normpath_obj) },
    { MP_ROM_QSTR(MP_QSTR_abspath), MP_ROM_PTR(&path_abspath_obj) },
    { MP_ROM_QSTR(MP_QSTR_expanduser), MP_ROM_PTR(&path_expanduser_obj) },
};
static MP_DEFINE_CONST_DICT(path_globais, path_globais_tab);

static const mp_obj_module_t modulo_path = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&path_globais,
};

/* ------------------------------------------------------------ os */

static MP_DEFINE_STR_OBJ(fim_de_linha, "\n");

static const mp_rom_map_elem_t os_globais_tab[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_os) },
    { MP_ROM_QSTR(MP_QSTR___getattr__), MP_ROM_PTR(&os_getattr_obj) },
    { MP_ROM_QSTR(MP_QSTR_path), MP_ROM_PTR(&modulo_path) },
    { MP_ROM_QSTR(MP_QSTR_name), MP_ROM_QSTR(MP_QSTR_posix) },
    { MP_ROM_QSTR(MP_QSTR_sep), MP_ROM_QSTR(MP_QSTR__slash_) },
    { MP_ROM_QSTR(MP_QSTR_linesep), MP_ROM_PTR(&fim_de_linha) },
    { MP_ROM_QSTR(MP_QSTR_curdir), MP_ROM_PTR(&str_ponto) },
    { MP_ROM_QSTR(MP_QSTR_pardir), MP_ROM_PTR(&str_ponto_ponto) },
    { MP_ROM_QSTR(MP_QSTR_extsep), MP_ROM_PTR(&str_ponto) },
    { MP_ROM_QSTR(MP_QSTR_getcwd), MP_ROM_PTR(&os_getcwd_obj) },
    { MP_ROM_QSTR(MP_QSTR_chdir), MP_ROM_PTR(&os_chdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_listdir), MP_ROM_PTR(&os_listdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_ilistdir), MP_ROM_PTR(&os_ilistdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkdir), MP_ROM_PTR(&os_mkdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_makedirs), MP_ROM_PTR(&os_makedirs_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmdir), MP_ROM_PTR(&os_rmdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove), MP_ROM_PTR(&os_remove_obj) },
    { MP_ROM_QSTR(MP_QSTR_unlink), MP_ROM_PTR(&os_remove_obj) },
    { MP_ROM_QSTR(MP_QSTR_rename), MP_ROM_PTR(&os_rename_obj) },
    { MP_ROM_QSTR(MP_QSTR_stat), MP_ROM_PTR(&os_stat_obj) },
    { MP_ROM_QSTR(MP_QSTR_urandom), MP_ROM_PTR(&os_urandom_obj) },
    { MP_ROM_QSTR(MP_QSTR_getpid), MP_ROM_PTR(&os_getpid_obj) },
    { MP_ROM_QSTR(MP_QSTR_uname), MP_ROM_PTR(&os_uname_obj) },
    { MP_ROM_QSTR(MP_QSTR_getenv), MP_ROM_PTR(&os_getenv_obj) },
    { MP_ROM_QSTR(MP_QSTR_putenv), MP_ROM_PTR(&os_putenv_obj) },
    { MP_ROM_QSTR(MP_QSTR_unsetenv), MP_ROM_PTR(&os_unsetenv_obj) },
    { MP_ROM_QSTR(MP_QSTR_system), MP_ROM_PTR(&os_system_obj) },
};
static MP_DEFINE_CONST_DICT(os_globais, os_globais_tab);

const mp_obj_module_t modulo_os = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&os_globais,
};
MP_REGISTER_MODULE(MP_QSTR_os, modulo_os);
