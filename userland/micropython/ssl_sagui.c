/* ssl_sagui.c - el módulo ssl sobre libtls de SaguiOS (D060-D063, D069).
 *
 *   ssl.wrap_socket(sock, server_hostname=None, cert_reqs=CERT_REQUIRED,
 *                   cafile=None, cadata=None)
 *   ctx = ssl.create_default_context()      # ou ssl.SSLContext()
 *   ctx.load_verify_locations(cafile=..., cadata=...)
 *   ctx.check_hostname = False; ctx.verify_mode = ssl.CERT_NONE
 *   s = ctx.wrap_socket(sock, server_hostname="exemplo.com")
 *
 * Solo cliente (libtls no tiene el lado servidor). Por defecto la cadena es
 * validada con el almacén del sistema (/etc/ssl/certs, lo que administra el comando
 * "certificados") y el nombre tiene que coincidir con server_hostname (o
 * con el IP, si no se dio). Diferencia con CPython: wrap_socket() sin más
 * también valida por defecto; para no validar hay que pedir
 * cert_reqs=ssl.CERT_NONE. Los errores de certificado salen como
 * ssl.SSLCertVerificationError con el texto de libtls, en español. */
#include <string.h>
#include "py/runtime.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "py/objexcept.h"
#include "sagui_mp.h"
#include "ponte_sagui.h"

#define CERT_NONE     0
#define CERT_OPTIONAL 1
#define CERT_REQUIRED 2
#define PROTOCOL_TLS_CLIENT 16

/* SSLError (subclase de OSError) y SSLCertVerificationError (de ella). */
static MP_DEFINE_CONST_OBJ_TYPE(tipo_sslerror, MP_QSTR_SSLError, MP_TYPE_FLAG_NONE,
    make_new, mp_obj_exception_make_new, print, mp_obj_exception_print,
    attr, mp_obj_exception_attr, parent, &mp_type_OSError);
static MP_DEFINE_CONST_OBJ_TYPE(tipo_certerror, MP_QSTR_SSLCertVerificationError, MP_TYPE_FLAG_NONE,
    make_new, mp_obj_exception_make_new, print, mp_obj_exception_print,
    attr, mp_obj_exception_attr, parent, &tipo_sslerror);

/* Los textos de libtls vienen en Latin-1; los de aquí, en UTF-8 (ver mp_sagui.c). */
static NORETURN void levantar_ssl(const mp_obj_type_t *tipo, const char *texto_latin1)
{
    mp_obj_t m = sagui_str_de_latin1(texto_latin1);
    nlr_raise(mp_obj_new_exception_args(tipo, 1, &m));
}

static NORETURN void levantar_ssl_aqui(const char *texto)
{
    mp_obj_t m = mp_obj_new_str(texto, strlen(texto));
    nlr_raise(mp_obj_new_exception_args(&tipo_sslerror, 1, &m));
}

/* ------------------------------------------------------------ SSLContext */

typedef struct {
    mp_obj_base_t base;
    int   verify_mode;
    bool  check_hostname;
    void *raizes;          /* NULL = las del sistema */
} contexto_t;

static const mp_obj_type_t tipo_contexto;

static contexto_t *novo_contexto(bool verificar)
{
    contexto_t *c = mp_obj_malloc_with_finaliser(contexto_t, &tipo_contexto);
    c->verify_mode = verificar ? CERT_REQUIRED : CERT_NONE;
    c->check_hostname = verificar;
    c->raizes = NULL;
    return c;
}

static mp_obj_t contexto_make_new(const mp_obj_type_t *tipo, size_t n_args, size_t n_kw, const mp_obj_t *args)
{
    (void)tipo;
    mp_arg_check_num(n_args, n_kw, 0, 1, false);
    /* PROTOCOL_TLS_CLIENT (el predeterminado aquí) valida; PROTOCOL_TLS (2), como en
     * CPython, empieza sin validar. */
    bool verificar = n_args == 0 || mp_obj_get_int(args[0]) == PROTOCOL_TLS_CLIENT;
    return MP_OBJ_FROM_PTR(novo_contexto(verificar));
}

static mp_obj_t contexto_del(mp_obj_t o)
{
    contexto_t *c = MP_OBJ_TO_PTR(o);
    if (c->raizes) ponte_raizes_liberar(c->raizes);
    c->raizes = NULL;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(contexto_del_obj, contexto_del);

/* load_verify_locations(cafile=None, capath=None, cadata=None): suma
 * certificados de confianza a las raíces del sistema. cadata puede ser texto PEM
 * o bytes DER. */
static void somar_raizes(contexto_t *c, mp_obj_t cafile, mp_obj_t cadata)
{
    if (cafile == mp_const_none && cadata == mp_const_none) return;
    if (!c->raizes) {
        c->raizes = ponte_raizes_novo(1);
        if (!c->raizes) mp_raise_OSError(MP_ENOMEM);
    }
    if (cafile != mp_const_none) {
        char caminho[256];
        sagui_str_para_latin1(cafile, caminho, sizeof caminho);
        int r = ponte_raizes_somar_arquivo(c->raizes, caminho);
        if (r < 0) sagui_levantar_obj(r, cafile);
        if (r == 0) levantar_ssl_aqui("el archivo no contiene ningún certificado PEM");
    }
    if (cadata != mp_const_none) {
        size_t n;
        const char *d = mp_obj_str_get_data(cadata, &n);
        int r = ponte_raizes_somar(c->raizes, d, n);
        if (r == 0) levantar_ssl_aqui("cadata no contiene ningún certificado");
    }
}

static mp_obj_t contexto_load_verify_locations(size_t n_args, const mp_obj_t *args, mp_map_t *kw)
{
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_cafile, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_capath, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_cadata, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
    };
    mp_arg_val_t a[3];
    mp_arg_parse_all(n_args - 1, args + 1, kw, 3, permitidos, a);
    if (a[1].u_obj != mp_const_none) mp_raise_NotImplementedError(MP_ERROR_TEXT("capath"));
    somar_raizes(MP_OBJ_TO_PTR(args[0]), a[0].u_obj, a[2].u_obj);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(contexto_load_verify_locations_obj, 1, contexto_load_verify_locations);

/* Las raíces del sistema ya son el predeterminado: nada que hacer. */
static mp_obj_t contexto_nada(size_t n_args, const mp_obj_t *args) { (void)n_args; (void)args; return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(contexto_nada_obj, 1, 2, contexto_nada);

/* ------------------------------------------------------------ SSLSocket */

typedef struct {
    mp_obj_base_t base;
    mp_obj_t sock;         /* el socket de debajo (queda vivo y se cierra junto) */
    mp_obj_t contexto;
    void    *tls;          /* NULL = cerrado */
    int      fd;
} tls_obj_t;

static const mp_obj_type_t tipo_tls;

static tls_obj_t *aberto(mp_obj_t o)
{
    tls_obj_t *t = MP_OBJ_TO_PTR(o);
    if (!t->tls) mp_raise_OSError(MP_EBADF);
    return t;
}

static mp_obj_t embrulhar(contexto_t *c, mp_obj_t sock, mp_obj_t host)
{
    int fd = sagui_socket_fd(sock);
    char nome[256] = "";
    if (host != mp_const_none) sagui_str_para_latin1(host, nome, sizeof nome);
    else if (c->check_hostname && c->verify_mode != CERT_NONE) {
        /* Sin server_hostname: comprueba el IP con que se conectó (el SAN de IP). */
        mp_obj_t par = mp_call_function_0(mp_load_attr(sock, MP_QSTR_getpeername));
        sagui_str_para_latin1(mp_obj_subscr(par, MP_OBJ_NEW_SMALL_INT(0), MP_OBJ_SENTINEL), nome, sizeof nome);
    }
    int opcoes = 0;
    if (c->verify_mode != CERT_NONE) opcoes |= PONTE_VERIFICAR;
    if (c->check_hostname) opcoes |= PONTE_CONFERIR_NOME;
    char erro[200];
    int cert = 0;
    void *t = ponte_tls_abrir(fd, nome, opcoes, c->raizes, erro, sizeof erro, &cert);
    if (!t) levantar_ssl(cert ? &tipo_certerror : &tipo_sslerror, erro);
    tls_obj_t *o = mp_obj_malloc_with_finaliser(tls_obj_t, &tipo_tls);
    o->sock = sock;
    o->contexto = MP_OBJ_FROM_PTR(c);
    o->tls = t;
    o->fd = fd;
    return MP_OBJ_FROM_PTR(o);
}

static mp_obj_t contexto_wrap_socket(size_t n_args, const mp_obj_t *args, mp_map_t *kw)
{
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_sock, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_server_side, MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_do_handshake_on_connect, MP_ARG_BOOL, {.u_bool = true} },
        { MP_QSTR_suppress_ragged_eofs, MP_ARG_BOOL, {.u_bool = true} },
        { MP_QSTR_server_hostname, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
    };
    mp_arg_val_t a[5];
    mp_arg_parse_all(n_args - 1, args + 1, kw, 5, permitidos, a);
    if (a[1].u_bool) mp_raise_NotImplementedError(MP_ERROR_TEXT("server_side"));
    return embrulhar(MP_OBJ_TO_PTR(args[0]), a[0].u_obj, a[4].u_obj);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(contexto_wrap_socket_obj, 2, contexto_wrap_socket);

static void contexto_atributo(mp_obj_t o, qstr nome, mp_obj_t *dest)
{
    contexto_t *c = MP_OBJ_TO_PTR(o);
    if (dest[0] == MP_OBJ_NULL) {   /* lectura */
        if (nome == MP_QSTR_verify_mode) dest[0] = MP_OBJ_NEW_SMALL_INT(c->verify_mode);
        else if (nome == MP_QSTR_check_hostname) dest[0] = mp_obj_new_bool(c->check_hostname);
        else dest[1] = MP_OBJ_SENTINEL;
        return;
    }
    if (dest[1] == MP_OBJ_NULL) return;   /* del: no */
    if (nome == MP_QSTR_verify_mode) {
        int v = mp_obj_get_int(dest[1]);
        if (v < CERT_NONE || v > CERT_REQUIRED) mp_raise_ValueError(NULL);
        if (v == CERT_NONE && c->check_hostname)
            mp_raise_ValueError(MP_ERROR_TEXT("Cannot set verify_mode to CERT_NONE when check_hostname is enabled."));
        c->verify_mode = v;
        dest[0] = MP_OBJ_NULL;
    } else if (nome == MP_QSTR_check_hostname) {
        c->check_hostname = mp_obj_is_true(dest[1]);
        if (c->check_hostname && c->verify_mode == CERT_NONE) c->verify_mode = CERT_REQUIRED;
        dest[0] = MP_OBJ_NULL;
    }
}

static const mp_rom_map_elem_t contexto_metodos_tab[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&contexto_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_wrap_socket), MP_ROM_PTR(&contexto_wrap_socket_obj) },
    { MP_ROM_QSTR(MP_QSTR_load_verify_locations), MP_ROM_PTR(&contexto_load_verify_locations_obj) },
    { MP_ROM_QSTR(MP_QSTR_load_default_certs), MP_ROM_PTR(&contexto_nada_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_default_verify_paths), MP_ROM_PTR(&contexto_nada_obj) },
};
static MP_DEFINE_CONST_DICT(contexto_metodos, contexto_metodos_tab);

static MP_DEFINE_CONST_OBJ_TYPE(
    tipo_contexto,
    MP_QSTR_SSLContext,
    MP_TYPE_FLAG_NONE,
    make_new, contexto_make_new,
    attr, contexto_atributo,
    locals_dict, &contexto_metodos
    );

/* ------------------------------------------------------------ lectura y escritura */

/* Con plazo (settimeout), espera a que lleguen los datos antes de llamar a
 * libtls: si el plazo venciera en medio de un registro, la conexión TLS quedaría
 * inutilizable. Así el timeout sale limpio y la conexión sigue valiendo. */
static int esperar_dados(tls_obj_t *t, int *erro)
{
    if (ponte_tls_pendente(t->tls) > 0) return 0;
    mp_obj_t tempo = mp_call_function_0(mp_load_attr(t->sock, MP_QSTR_gettimeout));
    if (tempo == mp_const_none) return 0;
    mp_float_t seg = mp_obj_get_float(tempo);
    unsigned prazo = (unsigned)(seg * 1000), inicio = tempo_ms();
    for (;;) {
        if (disponivel(t->fd) != 0) return 0;
        if (tempo_ms() - inicio >= prazo) { *erro = prazo ? ERRO_TEMPO : -1; return -1; }
        SEM_GIL(dormir(10));
    }
}

static mp_uint_t tls_ler_stream(mp_obj_t o, void *buf, mp_uint_t n, int *erro)
{
    tls_obj_t *t = MP_OBJ_TO_PTR(o);
    if (!t->tls) { *erro = MP_EBADF; return MP_STREAM_ERROR; }
    int e;
    if (esperar_dados(t, &e) < 0) { *erro = e == ERRO_TEMPO ? MP_ETIMEDOUT : MP_EAGAIN; return MP_STREAM_ERROR; }
    int r = ponte_tls_ler(t->tls, buf, n);
    if (r < 0) { *erro = MP_EIO; return MP_STREAM_ERROR; }
    return r;
}

static mp_uint_t tls_escrever_stream(mp_obj_t o, const void *buf, mp_uint_t n, int *erro)
{
    tls_obj_t *t = MP_OBJ_TO_PTR(o);
    if (!t->tls) { *erro = MP_EBADF; return MP_STREAM_ERROR; }
    int r = ponte_tls_escrever(t->tls, buf, n);
    if (r < 0) { *erro = MP_EIO; return MP_STREAM_ERROR; }
    return n;
}

static mp_obj_t tls_close(mp_obj_t o)
{
    tls_obj_t *t = MP_OBJ_TO_PTR(o);
    if (t->tls) {
        /* Sin asignar nada: esto también se ejecuta como finalizador, dentro del
         * recolector, donde asignar da MemoryError. */
        ponte_tls_fechar(t->tls);
        t->tls = NULL;
        sagui_socket_fechar(t->sock);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(tls_close_obj, tls_close);

static mp_uint_t tls_ioctl(mp_obj_t o, mp_uint_t pedido, uintptr_t arg, int *erro)
{
    tls_obj_t *t = MP_OBJ_TO_PTR(o);
    if (pedido == MP_STREAM_CLOSE) { tls_close(o); return 0; }
    if (!t->tls) { *erro = MP_EBADF; return MP_STREAM_ERROR; }
    if (pedido == MP_STREAM_POLL) {
        mp_uint_t pronto = arg & MP_STREAM_POLL_WR;
        if ((arg & MP_STREAM_POLL_RD) && (ponte_tls_pendente(t->tls) > 0 || disponivel(t->fd) != 0)) pronto |= MP_STREAM_POLL_RD;
        return pronto;
    }
    if (pedido == MP_STREAM_GET_FILENO) return t->fd;
    if (pedido == MP_STREAM_FLUSH) return 0;
    *erro = MP_EINVAL;
    return MP_STREAM_ERROR;
}

/* recv(n) / read(n): errores con el texto de libtls; plazo vencido =
 * socket.timeout; sin datos en un socket no bloqueante = EAGAIN. */
static mp_obj_t tls_recv(size_t n_args, const mp_obj_t *args)
{
    tls_obj_t *t = aberto(args[0]);
    mp_int_t n = n_args > 1 ? mp_obj_get_int(args[1]) : 4096;
    if (n < 0) n = 4096;
    vstr_t v;
    vstr_init_len(&v, n);
    int e;
    if (n && esperar_dados(t, &e) < 0) {
        if (e == ERRO_TEMPO) sagui_levantar_rede(ERRO_TEMPO);
        mp_raise_OSError(MP_EAGAIN);
    }
    int r = n ? ponte_tls_ler(t->tls, v.buf, n) : 0;
    if (r < 0) levantar_ssl(&tipo_sslerror, ponte_tls_erro(t->tls));
    v.len = r;
    return mp_obj_new_bytes_from_vstr(&v);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tls_recv_obj, 1, 3, tls_recv);

static mp_obj_t tls_sendall(mp_obj_t o, mp_obj_t dados)
{
    tls_obj_t *t = aberto(o);
    mp_buffer_info_t b;
    mp_get_buffer_raise(dados, &b, MP_BUFFER_READ);
    int r = ponte_tls_escrever(t->tls, b.buf, b.len);
    if (r < 0) levantar_ssl(&tipo_sslerror, ponte_tls_erro(t->tls));
    return MP_OBJ_NEW_SMALL_INT(b.len);
}
static MP_DEFINE_CONST_FUN_OBJ_2(tls_sendall_obj, tls_sendall);

static mp_obj_t tls_delegar(mp_obj_t o, qstr metodo, size_t n, const mp_obj_t *args)
{
    tls_obj_t *t = aberto(o);
    mp_obj_t m[4];
    mp_load_method(t->sock, metodo, m);
    for (size_t i = 0; i < n; i++) m[2 + i] = args[i];
    return mp_call_method_n_kw(n, 0, m);
}
static mp_obj_t tls_settimeout(mp_obj_t o, mp_obj_t v) { return tls_delegar(o, MP_QSTR_settimeout, 1, &v); }
static MP_DEFINE_CONST_FUN_OBJ_2(tls_settimeout_obj, tls_settimeout);
static mp_obj_t tls_setblocking(mp_obj_t o, mp_obj_t v) { return tls_delegar(o, MP_QSTR_setblocking, 1, &v); }
static MP_DEFINE_CONST_FUN_OBJ_2(tls_setblocking_obj, tls_setblocking);
static mp_obj_t tls_gettimeout(mp_obj_t o) { return tls_delegar(o, MP_QSTR_gettimeout, 0, NULL); }
static MP_DEFINE_CONST_FUN_OBJ_1(tls_gettimeout_obj, tls_gettimeout);
static mp_obj_t tls_getpeername(mp_obj_t o) { return tls_delegar(o, MP_QSTR_getpeername, 0, NULL); }
static MP_DEFINE_CONST_FUN_OBJ_1(tls_getpeername_obj, tls_getpeername);
static mp_obj_t tls_fileno(mp_obj_t o) { return MP_OBJ_NEW_SMALL_INT(aberto(o)->fd); }
static MP_DEFINE_CONST_FUN_OBJ_1(tls_fileno_obj, tls_fileno);

/* makefile(): el propio objeto ya es un stream (read, readline, write). */
static mp_obj_t tls_makefile(size_t n_args, const mp_obj_t *args, mp_map_t *kw)
{
    (void)n_args; (void)kw;
    return args[0];
}
static MP_DEFINE_CONST_FUN_OBJ_KW(tls_makefile_obj, 1, tls_makefile);

static mp_obj_t tls_version(mp_obj_t o) { return sagui_str_de_latin1(ponte_tls_versao(aberto(o)->tls)); }
static MP_DEFINE_CONST_FUN_OBJ_1(tls_version_obj, tls_version);

static mp_obj_t tls_cipher(mp_obj_t o)
{
    tls_obj_t *t = aberto(o);
    mp_obj_t c[3] = { sagui_str_de_latin1(ponte_tls_suite(t->tls)), sagui_str_de_latin1(ponte_tls_versao(t->tls)), mp_const_none };
    return mp_obj_new_tuple(3, c);
}
static MP_DEFINE_CONST_FUN_OBJ_1(tls_cipher_obj, tls_cipher);

/* getpeercert(binary_form=False): el DER, o {"subject": "CN=..., O=..."}. */
static mp_obj_t tls_getpeercert(size_t n_args, const mp_obj_t *args)
{
    tls_obj_t *t = aberto(args[0]);
    const unsigned char *der;
    char sujeito[256];
    int n = ponte_tls_certificado(t->tls, &der, sujeito, sizeof sujeito);
    if (n <= 0) return mp_const_none;
    if (n_args > 1 && mp_obj_is_true(args[1])) return mp_obj_new_bytes(der, n);
    mp_obj_t d = mp_obj_new_dict(1);
    mp_obj_dict_store(d, MP_OBJ_NEW_QSTR(MP_QSTR_subject), sagui_str_de_latin1(sujeito));
    return d;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tls_getpeercert_obj, 1, 2, tls_getpeercert);

static mp_obj_t tls_exit(size_t n_args, const mp_obj_t *args) { (void)n_args; return tls_close(args[0]); }
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tls_exit_obj, 4, 4, tls_exit);

static const mp_rom_map_elem_t tls_metodos_tab[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&tls_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&mp_identity_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&tls_exit_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&tls_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv), MP_ROM_PTR(&tls_recv_obj) },
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&tls_sendall_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendall), MP_ROM_PTR(&tls_sendall_obj) },
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_settimeout), MP_ROM_PTR(&tls_settimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_gettimeout), MP_ROM_PTR(&tls_gettimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_setblocking), MP_ROM_PTR(&tls_setblocking_obj) },
    { MP_ROM_QSTR(MP_QSTR_getpeername), MP_ROM_PTR(&tls_getpeername_obj) },
    { MP_ROM_QSTR(MP_QSTR_fileno), MP_ROM_PTR(&tls_fileno_obj) },
    { MP_ROM_QSTR(MP_QSTR_makefile), MP_ROM_PTR(&tls_makefile_obj) },
    { MP_ROM_QSTR(MP_QSTR_version), MP_ROM_PTR(&tls_version_obj) },
    { MP_ROM_QSTR(MP_QSTR_cipher), MP_ROM_PTR(&tls_cipher_obj) },
    { MP_ROM_QSTR(MP_QSTR_getpeercert), MP_ROM_PTR(&tls_getpeercert_obj) },
};
static MP_DEFINE_CONST_DICT(tls_metodos, tls_metodos_tab);

static const mp_stream_p_t tls_stream = {
    .read = tls_ler_stream,
    .write = tls_escrever_stream,
    .ioctl = tls_ioctl,
};

static MP_DEFINE_CONST_OBJ_TYPE(
    tipo_tls,
    MP_QSTR_SSLSocket,
    MP_TYPE_FLAG_NONE,
    protocol, &tls_stream,
    locals_dict, &tls_metodos
    );

/* ------------------------------------------------------------ módulo */

static mp_obj_t ssl_create_default_context(size_t n_args, const mp_obj_t *args, mp_map_t *kw)
{
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_purpose, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_cafile, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_capath, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_cadata, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
    };
    mp_arg_val_t a[4];
    mp_arg_parse_all(n_args, args, kw, 4, permitidos, a);
    contexto_t *c = novo_contexto(true);
    somar_raizes(c, a[1].u_obj, a[3].u_obj);
    return MP_OBJ_FROM_PTR(c);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(ssl_create_default_context_obj, 0, ssl_create_default_context);

/* wrap_socket(sock, server_side=False, keyfile=None, certfile=None,
 *             cert_reqs=CERT_REQUIRED, ca_certs=None, server_hostname=None,
 *             cadata=None, do_handshake=True) */
static mp_obj_t ssl_wrap_socket(size_t n_args, const mp_obj_t *args, mp_map_t *kw)
{
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_sock, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_server_side, MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_keyfile, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_certfile, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_cert_reqs, MP_ARG_INT, {.u_int = CERT_REQUIRED} },
        { MP_QSTR_ca_certs, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_server_hostname, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_cadata, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_cafile, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_do_handshake, MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_bool = true} },
    };
    mp_arg_val_t a[10];
    mp_arg_parse_all(n_args, args, kw, 10, permitidos, a);
    if (a[1].u_bool) mp_raise_NotImplementedError(MP_ERROR_TEXT("server_side"));
    bool verificar = a[4].u_int != CERT_NONE;
    contexto_t *c = novo_contexto(verificar);
    mp_obj_t cafile = a[8].u_obj != mp_const_none ? a[8].u_obj : a[5].u_obj;
    somar_raizes(c, cafile, a[7].u_obj);
    return embrulhar(c, a[0].u_obj, a[6].u_obj);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(ssl_wrap_socket_obj, 1, ssl_wrap_socket);

static const mp_rom_map_elem_t ssl_globais_tab[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ssl) },
    { MP_ROM_QSTR(MP_QSTR_wrap_socket), MP_ROM_PTR(&ssl_wrap_socket_obj) },
    { MP_ROM_QSTR(MP_QSTR_create_default_context), MP_ROM_PTR(&ssl_create_default_context_obj) },
    { MP_ROM_QSTR(MP_QSTR_SSLContext), MP_ROM_PTR(&tipo_contexto) },
    { MP_ROM_QSTR(MP_QSTR_SSLSocket), MP_ROM_PTR(&tipo_tls) },
    { MP_ROM_QSTR(MP_QSTR_SSLError), MP_ROM_PTR(&tipo_sslerror) },
    { MP_ROM_QSTR(MP_QSTR_SSLCertVerificationError), MP_ROM_PTR(&tipo_certerror) },
    { MP_ROM_QSTR(MP_QSTR_CertificateError), MP_ROM_PTR(&tipo_certerror) },
    { MP_ROM_QSTR(MP_QSTR_CERT_NONE), MP_ROM_INT(CERT_NONE) },
    { MP_ROM_QSTR(MP_QSTR_CERT_OPTIONAL), MP_ROM_INT(CERT_OPTIONAL) },
    { MP_ROM_QSTR(MP_QSTR_CERT_REQUIRED), MP_ROM_INT(CERT_REQUIRED) },
    { MP_ROM_QSTR(MP_QSTR_PROTOCOL_TLS), MP_ROM_INT(2) },
    { MP_ROM_QSTR(MP_QSTR_PROTOCOL_TLS_CLIENT), MP_ROM_INT(PROTOCOL_TLS_CLIENT) },
    { MP_ROM_QSTR(MP_QSTR_HAS_SNI), MP_ROM_TRUE },
};
static MP_DEFINE_CONST_DICT(ssl_globais, ssl_globais_tab);

const mp_obj_module_t modulo_ssl = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ssl_globais,
};
MP_REGISTER_MODULE(MP_QSTR_ssl, modulo_ssl);
