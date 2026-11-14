/* rede_sagui.c - el módulo socket sobre los sockets de SaguiOS (D046, D069).
 *
 * La interfaz es la de CPython (que MicroPython también sigue): socket(),
 * connect, bind, listen, accept, send, sendall, recv, recv_into, sendto,
 * recvfrom, settimeout, setblocking, makefile, shutdown, getaddrinfo... y el
 * el objeto también es un stream (read, readline, write; y select lo entiende).
 *
 * Diferencias que vienen del kernel:
 *   - bind() solo guarda el puerto: en SaguiOS escutar(fd, porta, fila) hace el
 *     bind y el listen del TCP de una vez, y por eso listen() es el que llama
 *     al kernel. En UDP bind() ya llama a escutar().
 *   - No hay loopback: 127.0.0.1 da "destino inalcanzable"; el anfitrión del
 *     QEMU es 10.0.2.2.
 *   - Modo no bloqueante (setblocking(False) o settimeout(0)): recv y
 *     accept preguntan al kernel si hay algo (disponivel) y, si no hay,
 *     lanzan OSError(EAGAIN). El connect sigue esperando la conexión.
 *   - setsockopt acepta las opciones comunes (SO_REUSEADDR, TCP_NODELAY...) y
 *     no hace nada: el kernel ya reutiliza puertos y envía al momento. */
#include <stdio.h>
#include <string.h>
#include "py/runtime.h"
#include "py/stream.h"
#include "py/objstr.h"
#include "py/mperrno.h"
#include "py/objexcept.h"
#include "sagui_mp.h"

#define AF_INET     2
#define SOCK_STREAM 1
#define SOCK_DGRAM  2

typedef struct {
    mp_obj_base_t base;
    int     fd;            /* -1 = cerrado */
    uint8_t tcp;
    uint8_t escutando;
    int     tempo_ms;      /* -1 = bloquea sin límite; 0 = no bloqueante; >0 = plazo */
    int     porta_local;   /* de bind() */
} sock_t;

static const mp_obj_type_t tipo_socket;
static int tempo_padrao = -1;   /* setdefaulttimeout */

/* socket.timeout e socket.gaierror: subclases de OSError, como en CPython
 * (e.errno sigue valiendo, como en MicroPython). */
MP_DEFINE_CONST_OBJ_TYPE(sagui_tipo_timeout, MP_QSTR_timeout, MP_TYPE_FLAG_NONE,
    make_new, mp_obj_exception_make_new, print, mp_obj_exception_print,
    attr, mp_obj_exception_attr, parent, &mp_type_OSError);
static MP_DEFINE_CONST_OBJ_TYPE(tipo_gaierror, MP_QSTR_gaierror, MP_TYPE_FLAG_NONE,
    make_new, mp_obj_exception_make_new, print, mp_obj_exception_print,
    attr, mp_obj_exception_attr, parent, &mp_type_OSError);

static MP_DEFINE_STR_OBJ(str_tempo, "timed out");

NORETURN void sagui_levantar_rede(int r)
{
    if (r == ERRO_TEMPO) {
        mp_obj_t a[2] = { MP_OBJ_NEW_SMALL_INT(MP_ETIMEDOUT), MP_OBJ_FROM_PTR(&str_tempo) };
        nlr_raise(mp_obj_new_exception_args(&sagui_tipo_timeout, 2, a));
    }
    sagui_levantar(r, NULL);
}

static NORETURN void levantar_eagain(void)
{
    mp_raise_OSError(MP_EAGAIN);
}

static sock_t *aberto(mp_obj_t o)
{
    sock_t *s = MP_OBJ_TO_PTR(o);
    if (s->fd < 0) mp_raise_OSError(MP_EBADF);
    return s;
}

/* ------------------------------------------------------------ direcciones */

static mp_obj_t texto_ip(uint32_t ip)
{
    char b[16];
    snprintf(b, sizeof b, "%u.%u.%u.%u", (unsigned)(ip >> 24), (unsigned)(ip >> 16 & 255),
             (unsigned)(ip >> 8 & 255), (unsigned)(ip & 255));
    return mp_obj_new_str(b, strlen(b));
}

static mp_obj_t tupla_endereco(uint32_t ip, int porta)
{
    mp_obj_t t[2] = { texto_ip(ip), MP_OBJ_NEW_SMALL_INT(porta) };
    return mp_obj_new_tuple(2, t);
}

/* Nombre o "a.b.c.d" -> IP. "" y "0.0.0.0" = cualquiera; "<broadcast>" = todos. */
static uint32_t resolver_nome(mp_obj_t host)
{
    char h[256];
    sagui_str_para_latin1(host, h, sizeof h);
    if (!h[0] || !strcmp(h, "0.0.0.0")) return 0;
    if (!strcmp(h, "<broadcast>")) return 0xFFFFFFFF;
    uint32_t ip = 0;
    int r = resolver(h, &ip);
    if (r < 0) {
        vstr_t v;
        vstr_init(&v, 64);
        vstr_add_str(&v, "no se pudo resolver ");
        sagui_latin1_para_vstr(&v, h, strlen(h));
        vstr_add_str(&v, ": ");
        const char *m = nome_erro(r);
        sagui_latin1_para_vstr(&v, m, strlen(m));
        mp_obj_t a[2] = { MP_OBJ_NEW_SMALL_INT(sagui_errno(r)), mp_obj_new_str_from_vstr(&v) };
        nlr_raise(mp_obj_new_exception_args(&tipo_gaierror, 2, a));
    }
    return ip;
}

static uint32_t endereco(mp_obj_t addr, int *porta)
{
    size_t n;
    mp_obj_t *e;
    mp_obj_get_array(addr, &n, &e);
    if (n != 2) mp_raise_TypeError(MP_ERROR_TEXT("address must be (host, port)"));
    *porta = mp_obj_get_int(e[1]);
    if (*porta < 0 || *porta > 65535) mp_raise_ValueError(MP_ERROR_TEXT("port must be 0-65535"));
    return resolver_nome(e[0]);
}

/* ------------------------------------------------------------ creación y opciones */

static void aplicar_tempo(sock_t *s, int ms)
{
    s->tempo_ms = ms;
    tempo_limite(s->fd, ms > 0 ? (unsigned)ms : 0);   /* 0 en el kernel = sin límite */
}

static sock_t *novo_socket(int fd, bool tcp)
{
    sock_t *s = mp_obj_malloc_with_finaliser(sock_t, &tipo_socket);
    s->fd = fd;
    s->tcp = tcp;
    s->escutando = 0;
    s->porta_local = 0;
    aplicar_tempo(s, tempo_padrao);
    return s;
}

/* socket(family=AF_INET, type=SOCK_STREAM, proto=0) */
static mp_obj_t socket_make_new(const mp_obj_type_t *tipo, size_t n_args, size_t n_kw, const mp_obj_t *args)
{
    (void)tipo;
    mp_arg_check_num(n_args, n_kw, 0, 4, false);
    int af = n_args > 0 ? mp_obj_get_int(args[0]) : AF_INET;
    int t = n_args > 1 ? mp_obj_get_int(args[1]) : SOCK_STREAM;
    if (af != AF_INET) mp_raise_OSError(MP_EAFNOSUPPORT);
    t &= 0xF;   /* SOCK_NONBLOCK/SOCK_CLOEXEC de Linux: ignorados */
    if (t != SOCK_STREAM && t != SOCK_DGRAM) mp_raise_OSError(MP_EINVAL);
    int fd = criar_socket(t == SOCK_STREAM ? SOCK_TCP : SOCK_UDP);
    if (fd < 0) sagui_levantar(fd, NULL);
    return MP_OBJ_FROM_PTR(novo_socket(fd, t == SOCK_STREAM));
}

static mp_obj_t socket_close(mp_obj_t o)
{
    sock_t *s = MP_OBJ_TO_PTR(o);
    if (s->fd >= 0) fechar(s->fd);
    s->fd = -1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_close_obj, socket_close);

static mp_obj_t socket_exit(size_t n_args, const mp_obj_t *args)
{
    (void)n_args;
    return socket_close(args[0]);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_exit_obj, 4, 4, socket_exit);

static mp_obj_t socket_fileno(mp_obj_t o) { return MP_OBJ_NEW_SMALL_INT(((sock_t *)MP_OBJ_TO_PTR(o))->fd); }
static MP_DEFINE_CONST_FUN_OBJ_1(socket_fileno_obj, socket_fileno);

static int tempo_de(mp_obj_t t)
{
    if (t == mp_const_none) return -1;
    mp_float_t seg = mp_obj_get_float(t);
    if (seg < 0) mp_raise_ValueError(MP_ERROR_TEXT("Timeout value out of range"));
    if (seg > 3600) seg = 3600;   /* el kernel acepta hasta 1 hora */
    int ms = (int)(seg * 1000);
    return seg > 0 && ms == 0 ? 1 : ms;
}

static mp_obj_t socket_settimeout(mp_obj_t o, mp_obj_t t)
{
    aplicar_tempo(aberto(o), tempo_de(t));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_settimeout_obj, socket_settimeout);

static mp_obj_t socket_gettimeout(mp_obj_t o)
{
    sock_t *s = MP_OBJ_TO_PTR(o);
    if (s->tempo_ms < 0) return mp_const_none;
    return mp_obj_new_float((mp_float_t)s->tempo_ms / 1000);
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_gettimeout_obj, socket_gettimeout);

static mp_obj_t socket_setblocking(mp_obj_t o, mp_obj_t sim)
{
    aplicar_tempo(aberto(o), mp_obj_is_true(sim) ? -1 : 0);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_setblocking_obj, socket_setblocking);

static mp_obj_t socket_setsockopt(size_t n_args, const mp_obj_t *args)
{
    (void)n_args;
    aberto(args[0]);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_setsockopt_obj, 4, 4, socket_setsockopt);

/* ------------------------------------------------------------ conexión */

static mp_obj_t socket_connect(mp_obj_t o, mp_obj_t addr)
{
    sock_t *s = aberto(o);
    int porta;
    uint32_t ip = endereco(addr, &porta);
    int r = SEM_GIL(conectar(s->fd, ip, porta));
    if (r < 0) sagui_levantar_rede(r);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_connect_obj, socket_connect);

static mp_obj_t socket_bind(mp_obj_t o, mp_obj_t addr)
{
    sock_t *s = aberto(o);
    int porta;
    endereco(addr, &porta);   /* SaguiOS tiene una sola dirección: el IP se ignora */
    s->porta_local = porta;
    if (!s->tcp && porta) {
        int r = escutar(s->fd, porta, 0);
        if (r < 0) sagui_levantar(r == ERRO_OCUPADO ? ERRO_OCUPADO : r, NULL);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_bind_obj, socket_bind);

static mp_obj_t socket_listen(size_t n_args, const mp_obj_t *args)
{
    sock_t *s = aberto(args[0]);
    int fila = n_args > 1 ? mp_obj_get_int(args[1]) : 5;
    if (!s->tcp) mp_raise_OSError(MP_EOPNOTSUPP);
    if (!s->porta_local) mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("listen() necesita bind() con un puerto"));
    if (fila < 1) fila = 1;
    int r = escutar(s->fd, s->porta_local, fila);
    if (r == ERRO_OCUPADO || r == ERRO_JA_EXISTE) mp_raise_OSError(MP_EADDRINUSE);
    if (r < 0) sagui_levantar(r, NULL);
    s->escutando = 1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_listen_obj, 1, 2, socket_listen);

static mp_obj_t socket_accept(mp_obj_t o)
{
    sock_t *s = aberto(o);
    if (s->tempo_ms == 0 && disponivel(s->fd) <= 0) levantar_eagain();
    endereco_rede_t de;
    int fd = SEM_GIL(aceitar_conexao(s->fd, &de));
    if (fd < 0) sagui_levantar_rede(fd);
    mp_obj_t t[2] = { MP_OBJ_FROM_PTR(novo_socket(fd, true)), tupla_endereco(de.ip, de.porta) };
    return mp_obj_new_tuple(2, t);
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_accept_obj, socket_accept);

static mp_obj_t socket_shutdown(mp_obj_t o, mp_obj_t como)
{
    sock_t *s = aberto(o);
    if (s->tcp && mp_obj_get_int(como) != 0) opcao_socket(s->fd, OPC_FECHAR_ENVIO, 0);   /* SHUT_WR o SHUT_RDWR */
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_shutdown_obj, socket_shutdown);

static mp_obj_t enderecos(mp_obj_t o, bool remoto)
{
    sock_t *s = aberto(o);
    enderecos_socket_t e;
    memset(&e, 0, sizeof e);
    opcao_socket(s->fd, OPC_ENDERECOS, (unsigned)&e);
    if (remoto && !e.ip_remoto) mp_raise_OSError(MP_ENOTCONN);
    return remoto ? tupla_endereco(e.ip_remoto, e.porta_remota) : tupla_endereco(e.ip_local, e.porta_local ? e.porta_local : s->porta_local);
}
static mp_obj_t socket_getsockname(mp_obj_t o) { return enderecos(o, false); }
static MP_DEFINE_CONST_FUN_OBJ_1(socket_getsockname_obj, socket_getsockname);
static mp_obj_t socket_getpeername(mp_obj_t o) { return enderecos(o, true); }
static MP_DEFINE_CONST_FUN_OBJ_1(socket_getpeername_obj, socket_getpeername);

/* ------------------------------------------------------------ datos */

/* Lee hasta n bytes; 0 = el otro lado cerró. En modo no bloqueante sin nada
 * para leer devuelve -1 con *erro = EAGAIN. */
static int receber(sock_t *s, void *buf, size_t n, int *erro)
{
    if (s->tempo_ms == 0) {
        int d = disponivel(s->fd);
        if (d == 0) { *erro = MP_EAGAIN; return -1; }
    }
    int r = SEM_GIL(ler(s->fd, buf, n));
    if (r == ERRO_PIPE_QUEBRADO) return 0;
    if (r < 0) { *erro = r; return -2; }
    return r;
}

static mp_obj_t socket_recv(size_t n_args, const mp_obj_t *args)
{
    sock_t *s = aberto(args[0]);
    mp_int_t n = mp_obj_get_int(args[1]);
    (void)n_args;
    if (n < 0) mp_raise_ValueError(NULL);
    vstr_t v;
    vstr_init_len(&v, n);
    int erro, r = n ? receber(s, v.buf, n, &erro) : 0;
    if (r == -1) levantar_eagain();
    if (r < 0) sagui_levantar_rede(erro);
    v.len = r;
    return mp_obj_new_bytes_from_vstr(&v);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_recv_obj, 2, 3, socket_recv);

static mp_obj_t socket_recv_into(size_t n_args, const mp_obj_t *args)
{
    sock_t *s = aberto(args[0]);
    mp_buffer_info_t b;
    mp_get_buffer_raise(args[1], &b, MP_BUFFER_WRITE);
    size_t n = b.len;
    if (n_args > 2 && mp_obj_get_int(args[2]) > 0 && (size_t)mp_obj_get_int(args[2]) < n) n = mp_obj_get_int(args[2]);
    int erro, r = n ? receber(s, b.buf, n, &erro) : 0;
    if (r == -1) levantar_eagain();
    if (r < 0) sagui_levantar_rede(erro);
    return MP_OBJ_NEW_SMALL_INT(r);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_recv_into_obj, 2, 4, socket_recv_into);

static mp_obj_t socket_send(size_t n_args, const mp_obj_t *args)
{
    sock_t *s = aberto(args[0]);
    (void)n_args;
    mp_buffer_info_t b;
    mp_get_buffer_raise(args[1], &b, MP_BUFFER_READ);
    int r = SEM_GIL(escrever(s->fd, b.buf, b.len));
    if (r < 0) sagui_levantar_rede(r);
    return MP_OBJ_NEW_SMALL_INT(r);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_send_obj, 2, 3, socket_send);

static mp_obj_t socket_sendall(size_t n_args, const mp_obj_t *args)
{
    sock_t *s = aberto(args[0]);
    (void)n_args;
    mp_buffer_info_t b;
    mp_get_buffer_raise(args[1], &b, MP_BUFFER_READ);
    int r = SEM_GIL(escrever_tudo(s->fd, b.buf, b.len));
    if (r < 0) sagui_levantar_rede(r);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_sendall_obj, 2, 3, socket_sendall);

static mp_obj_t socket_sendto(size_t n_args, const mp_obj_t *args)
{
    sock_t *s = aberto(args[0]);
    mp_buffer_info_t b;
    mp_get_buffer_raise(args[1], &b, MP_BUFFER_READ);
    int porta;
    uint32_t ip = endereco(args[n_args - 1], &porta);   /* sendto(datos[, flags], dirección) */
    int r = SEM_GIL(enviar_para(s->fd, b.buf, b.len, ip, porta));
    if (r < 0) sagui_levantar_rede(r);
    return MP_OBJ_NEW_SMALL_INT(r);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_sendto_obj, 3, 4, socket_sendto);

static mp_obj_t socket_recvfrom(size_t n_args, const mp_obj_t *args)
{
    sock_t *s = aberto(args[0]);
    (void)n_args;
    mp_int_t n = mp_obj_get_int(args[1]);
    if (n < 0) mp_raise_ValueError(NULL);
    if (s->tempo_ms == 0 && disponivel(s->fd) <= 0) levantar_eagain();
    vstr_t v;
    vstr_init_len(&v, n);
    endereco_rede_t de;
    memset(&de, 0, sizeof de);
    int r = SEM_GIL(receber_de(s->fd, v.buf, n, &de));
    if (r < 0) sagui_levantar_rede(r);
    v.len = r;
    mp_obj_t t[2] = { mp_obj_new_bytes_from_vstr(&v), tupla_endereco(de.ip, de.porta) };
    return mp_obj_new_tuple(2, t);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_recvfrom_obj, 2, 3, socket_recvfrom);

/* makefile(mode='r'): un objeto de archivo sobre el mismo descriptor (con
 * buffer de lectura, readline rápido y, en modo texto, la conversión de
 * Latin-1). Cerrar el archivo no cierra el socket, como en CPython. */
static mp_obj_t socket_makefile(size_t n_args, const mp_obj_t *args, mp_map_t *kw)
{
    sock_t *s = aberto(args[0]);
    (void)kw;
    const char *m = n_args > 1 ? mp_obj_str_get_str(args[1]) : "r";
    return sagui_arquivo_de_fd(s->fd, strchr(m, 'b') == NULL);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(socket_makefile_obj, 1, socket_makefile);

/* ------------------------------------------------------------ stream */

static mp_uint_t socket_ler(mp_obj_t o, void *buf, mp_uint_t n, int *erro)
{
    sock_t *s = MP_OBJ_TO_PTR(o);
    if (s->fd < 0) { *erro = MP_EBADF; return MP_STREAM_ERROR; }
    int e, r = receber(s, buf, n, &e);
    if (r == -1) { *erro = MP_EAGAIN; return MP_STREAM_ERROR; }
    if (r < 0) { *erro = sagui_errno(e); return MP_STREAM_ERROR; }
    return r;
}

static mp_uint_t socket_escrever(mp_obj_t o, const void *buf, mp_uint_t n, int *erro)
{
    sock_t *s = MP_OBJ_TO_PTR(o);
    if (s->fd < 0) { *erro = MP_EBADF; return MP_STREAM_ERROR; }
    int r = SEM_GIL(escrever(s->fd, buf, n));
    if (r < 0) { *erro = sagui_errno(r); return MP_STREAM_ERROR; }
    return r;
}

static mp_uint_t socket_ioctl(mp_obj_t o, mp_uint_t pedido, uintptr_t arg, int *erro)
{
    sock_t *s = MP_OBJ_TO_PTR(o);
    if (pedido == MP_STREAM_CLOSE) { socket_close(o); return 0; }
    if (s->fd < 0) { *erro = MP_EBADF; return MP_STREAM_ERROR; }
    if (pedido == MP_STREAM_GET_FILENO) return s->fd;
    if (pedido == MP_STREAM_POLL) {
        /* Listo para leer: hay bytes, una conexión en la cola (escucha) o el
         * otro lado cerró (recv dará b""). Escribir no bloquea mucho
         * en SaguiOS: siempre listo. */
        mp_uint_t pronto = 0;
        int d = disponivel(s->fd);
        if ((arg & MP_STREAM_POLL_RD) && d != 0) pronto |= MP_STREAM_POLL_RD;
        if (arg & MP_STREAM_POLL_WR) pronto |= MP_STREAM_POLL_WR;
        if (d == ERRO_PIPE_QUEBRADO) pronto |= MP_STREAM_POLL_HUP;
        return pronto;
    }
    if (pedido == MP_STREAM_FLUSH) return 0;
    *erro = MP_EINVAL;
    return MP_STREAM_ERROR;
}

static const mp_stream_p_t socket_stream = {
    .read = socket_ler,
    .write = socket_escrever,
    .ioctl = socket_ioctl,
};

static void socket_imprimir(const mp_print_t *print, mp_obj_t o, mp_print_kind_t tipo)
{
    (void)tipo;
    sock_t *s = MP_OBJ_TO_PTR(o);
    mp_printf(print, "<socket fd=%d type=%s>", s->fd, s->tcp ? "SOCK_STREAM" : "SOCK_DGRAM");
}

static const mp_rom_map_elem_t socket_metodos_tab[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&socket_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&mp_identity_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&socket_exit_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&socket_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_fileno), MP_ROM_PTR(&socket_fileno_obj) },
    { MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&socket_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_bind), MP_ROM_PTR(&socket_bind_obj) },
    { MP_ROM_QSTR(MP_QSTR_listen), MP_ROM_PTR(&socket_listen_obj) },
    { MP_ROM_QSTR(MP_QSTR_accept), MP_ROM_PTR(&socket_accept_obj) },
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&socket_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendall), MP_ROM_PTR(&socket_sendall_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv), MP_ROM_PTR(&socket_recv_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv_into), MP_ROM_PTR(&socket_recv_into_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendto), MP_ROM_PTR(&socket_sendto_obj) },
    { MP_ROM_QSTR(MP_QSTR_recvfrom), MP_ROM_PTR(&socket_recvfrom_obj) },
    { MP_ROM_QSTR(MP_QSTR_settimeout), MP_ROM_PTR(&socket_settimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_gettimeout), MP_ROM_PTR(&socket_gettimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_setblocking), MP_ROM_PTR(&socket_setblocking_obj) },
    { MP_ROM_QSTR(MP_QSTR_setsockopt), MP_ROM_PTR(&socket_setsockopt_obj) },
    { MP_ROM_QSTR(MP_QSTR_shutdown), MP_ROM_PTR(&socket_shutdown_obj) },
    { MP_ROM_QSTR(MP_QSTR_getsockname), MP_ROM_PTR(&socket_getsockname_obj) },
    { MP_ROM_QSTR(MP_QSTR_getpeername), MP_ROM_PTR(&socket_getpeername_obj) },
    { MP_ROM_QSTR(MP_QSTR_makefile), MP_ROM_PTR(&socket_makefile_obj) },
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
};
static MP_DEFINE_CONST_DICT(socket_metodos, socket_metodos_tab);

static MP_DEFINE_CONST_OBJ_TYPE(
    tipo_socket,
    MP_QSTR_socket,
    MP_TYPE_FLAG_NONE,
    make_new, socket_make_new,
    print, socket_imprimir,
    protocol, &socket_stream,
    locals_dict, &socket_metodos
    );

/* Para ssl: el descriptor de un socket, si es bloqueante, y cerrarlo. */
void sagui_socket_fechar(mp_obj_t o)
{
    if (mp_obj_is_type(o, &tipo_socket)) socket_close(o);
}

int sagui_socket_fd(mp_obj_t o)
{
    if (!mp_obj_is_type(o, &tipo_socket)) mp_raise_TypeError(MP_ERROR_TEXT("expected a socket"));
    return aberto(o)->fd;
}

bool sagui_socket_bloqueante(mp_obj_t o)
{
    return ((sock_t *)MP_OBJ_TO_PTR(o))->tempo_ms != 0;
}

/* ------------------------------------------------------------ módulo */

/* getaddrinfo(host, port, family=0, type=0, proto=0, flags=0): el DNS del
 * sistema (SIS_RESOLVER); solo IPv4, una respuesta. */
static mp_obj_t socket_getaddrinfo(size_t n_args, const mp_obj_t *args, mp_map_t *kw)
{
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_host, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_port, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_family, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_type, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_proto, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_flags, MP_ARG_INT, {.u_int = 0} },
    };
    mp_arg_val_t a[6];
    mp_arg_parse_all(n_args, args, kw, 6, permitidos, a);
    if (a[2].u_int && a[2].u_int != AF_INET) mp_raise_OSError(MP_EAFNOSUPPORT);
    uint32_t ip = a[0].u_obj == mp_const_none ? 0 : resolver_nome(a[0].u_obj);
    int porta = a[1].u_obj == mp_const_none ? 0 : mp_obj_get_int(a[1].u_obj);
    int tipo = a[3].u_int ? a[3].u_int : SOCK_STREAM;
    mp_obj_t t[5] = {
        MP_OBJ_NEW_SMALL_INT(AF_INET), MP_OBJ_NEW_SMALL_INT(tipo),
        MP_OBJ_NEW_SMALL_INT(a[4].u_int ? a[4].u_int : (tipo == SOCK_STREAM ? 6 : 17)),
        MP_OBJ_NEW_QSTR(MP_QSTR_), tupla_endereco(ip, porta),
    };
    mp_obj_t r = mp_obj_new_tuple(5, t);
    return mp_obj_new_list(1, &r);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(socket_getaddrinfo_obj, 2, socket_getaddrinfo);

static mp_obj_t socket_gethostbyname(mp_obj_t host) { return texto_ip(resolver_nome(host)); }
static MP_DEFINE_CONST_FUN_OBJ_1(socket_gethostbyname_obj, socket_gethostbyname);

static mp_obj_t socket_gethostname(void) { return MP_OBJ_NEW_QSTR(MP_QSTR_sagui); }
static MP_DEFINE_CONST_FUN_OBJ_0(socket_gethostname_obj, socket_gethostname);

static mp_obj_t socket_getdefaulttimeout(void)
{
    return tempo_padrao < 0 ? mp_const_none : mp_obj_new_float((mp_float_t)tempo_padrao / 1000);
}
static MP_DEFINE_CONST_FUN_OBJ_0(socket_getdefaulttimeout_obj, socket_getdefaulttimeout);

static mp_obj_t socket_setdefaulttimeout(mp_obj_t t)
{
    tempo_padrao = tempo_de(t);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_setdefaulttimeout_obj, socket_setdefaulttimeout);

static mp_obj_t socket_inet_aton(mp_obj_t s)
{
    uint32_t ip = resolver_nome(s);
    uint8_t b[4] = { ip >> 24, ip >> 16, ip >> 8, ip };
    return mp_obj_new_bytes(b, 4);
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_inet_aton_obj, socket_inet_aton);

static mp_obj_t socket_inet_ntoa(mp_obj_t b)
{
    mp_buffer_info_t i;
    mp_get_buffer_raise(b, &i, MP_BUFFER_READ);
    if (i.len != 4) mp_raise_ValueError(NULL);
    const uint8_t *p = i.buf;
    return texto_ip((uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]);
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_inet_ntoa_obj, socket_inet_ntoa);

static const mp_rom_map_elem_t socket_globais_tab[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_socket) },
    { MP_ROM_QSTR(MP_QSTR_socket), MP_ROM_PTR(&tipo_socket) },
    { MP_ROM_QSTR(MP_QSTR_error), MP_ROM_PTR(&mp_type_OSError) },
    { MP_ROM_QSTR(MP_QSTR_timeout), MP_ROM_PTR(&sagui_tipo_timeout) },
    { MP_ROM_QSTR(MP_QSTR_gaierror), MP_ROM_PTR(&tipo_gaierror) },
    { MP_ROM_QSTR(MP_QSTR_getaddrinfo), MP_ROM_PTR(&socket_getaddrinfo_obj) },
    { MP_ROM_QSTR(MP_QSTR_gethostbyname), MP_ROM_PTR(&socket_gethostbyname_obj) },
    { MP_ROM_QSTR(MP_QSTR_gethostname), MP_ROM_PTR(&socket_gethostname_obj) },
    { MP_ROM_QSTR(MP_QSTR_getdefaulttimeout), MP_ROM_PTR(&socket_getdefaulttimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_setdefaulttimeout), MP_ROM_PTR(&socket_setdefaulttimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_inet_aton), MP_ROM_PTR(&socket_inet_aton_obj) },
    { MP_ROM_QSTR(MP_QSTR_inet_ntoa), MP_ROM_PTR(&socket_inet_ntoa_obj) },
    { MP_ROM_QSTR(MP_QSTR_AF_INET), MP_ROM_INT(AF_INET) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_STREAM), MP_ROM_INT(SOCK_STREAM) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_DGRAM), MP_ROM_INT(SOCK_DGRAM) },
    { MP_ROM_QSTR(MP_QSTR_IPPROTO_TCP), MP_ROM_INT(6) },
    { MP_ROM_QSTR(MP_QSTR_IPPROTO_UDP), MP_ROM_INT(17) },
    { MP_ROM_QSTR(MP_QSTR_SOL_SOCKET), MP_ROM_INT(1) },
    { MP_ROM_QSTR(MP_QSTR_SO_REUSEADDR), MP_ROM_INT(2) },
    { MP_ROM_QSTR(MP_QSTR_SO_BROADCAST), MP_ROM_INT(6) },
    { MP_ROM_QSTR(MP_QSTR_SO_KEEPALIVE), MP_ROM_INT(9) },
    { MP_ROM_QSTR(MP_QSTR_TCP_NODELAY), MP_ROM_INT(1) },
    { MP_ROM_QSTR(MP_QSTR_SHUT_RD), MP_ROM_INT(0) },
    { MP_ROM_QSTR(MP_QSTR_SHUT_WR), MP_ROM_INT(1) },
    { MP_ROM_QSTR(MP_QSTR_SHUT_RDWR), MP_ROM_INT(2) },
};
static MP_DEFINE_CONST_DICT(socket_globais, socket_globais_tab);

const mp_obj_module_t modulo_socket = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&socket_globais,
};
MP_REGISTER_MODULE(MP_QSTR_socket, modulo_socket);
