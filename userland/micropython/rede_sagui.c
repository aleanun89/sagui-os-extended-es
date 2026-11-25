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
 *   - Modo no bloqueante (setblocking(False) o settimeout(0)): el descriptor
 *     pasa a O_NONBLOCK (D401). recv, recvfrom y accept sin nada lanzan
 *     OSError(EAGAIN); connect devuelve EINPROGRESS (connect_ex lo da como número)
 *     y select/poll avisan de escritura al terminar; getsockopt(SO_ERROR) da el
 *     resultado, como en CPython.
 *   - setsockopt guarda las opciones comunes (SO_REUSEADDR, TCP_NODELAY, SO_RCVBUF...) y
 *     getsockopt las devuelve, pero el kernel no las usa: ya reutiliza puertos
 *     y envía al momento. Las desconocidas se aceptan sin efecto.
 *   - Solo IPv4 (AF_INET); AF_INET6 y AF_UNIX dan OSError(EAFNOSUPPORT); sin
 *     socketpair, sendmsg/recvmsg, MSG_PEEK ni gethostbyaddr (no hay DNS inverso).
 *   - Una señal que interrumpe recv/accept llama a los tratadores de signal y
 *     la espera sigue (PEP 475, D419). */
#include <stdio.h>
#include <string.h>
#include "py/runtime.h"
#include "py/stream.h"
#include "py/objstr.h"
#include "py/mperrno.h"
#include "py/objexcept.h"
#include "sagui_mp.h"

#define AF_UNSPEC   0
#define AF_INET     2
#define AF_INET6    10
#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_NONBLOCK 0x800
#define MSG_PEEK     0x02
#define MSG_DONTWAIT 0x40
#define MSG_WAITALL  0x100
#define MSG_NOSIGNAL 0x4000
#define SOL_SOCKET_  1
#define SO_TYPE_     3
#define SO_ERROR_    4
#define N_OPCOES     8

typedef struct {
    mp_obj_base_t base;
    int     fd;            /* -1 = cerrado */
    uint8_t tcp;
    uint8_t escutando;
    int     tempo_ms;      /* -1 = bloquea sin límite; 0 = no bloqueante; >0 = plazo */
    int     porta_local;   /* de bind() */
    int     opcoes[N_OPCOES];   /* lo que se guardó con setsockopt (sin efecto en el kernel) */
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
    /* El modo no bloqueante es del descriptor (O_NONBLOCK, D401): los plazos los pone el kernel. */
    int fl = descritor_op(s->fd, DESC_LER_FL, 0);
    if (fl >= 0) {
        fl = ms == 0 ? fl | DESC_FL_NAO_BLOQUEAR : fl & ~DESC_FL_NAO_BLOQUEAR;
        descritor_op(s->fd, DESC_FIXAR_FL, (unsigned)fl);
    }
}

static sock_t *novo_socket(int fd, bool tcp)
{
    sock_t *s = mp_obj_malloc_with_finaliser(sock_t, &tipo_socket);
    s->fd = fd;
    s->tcp = tcp;
    s->escutando = 0;
    s->porta_local = 0;
    memset(s->opcoes, 0, sizeof s->opcoes);
    s->opcoes[3] = s->opcoes[4] = 65536;   /* SO_RCVBUF y SO_SNDBUF que dirá getsockopt */
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
    bool sem_bloquear = t & SOCK_NONBLOCK;
    t &= 0xF;   /* SOCK_CLOEXEC de Linux: ignorado */
    if (t != SOCK_STREAM && t != SOCK_DGRAM) mp_raise_OSError(MP_EINVAL);
    int fd = criar_socket(t == SOCK_STREAM ? SOCK_TCP : SOCK_UDP);
    if (fd < 0) sagui_levantar(fd, NULL);
    sock_t *s = novo_socket(fd, t == SOCK_STREAM);
    if (sem_bloquear) aplicar_tempo(s, 0);
    return MP_OBJ_FROM_PTR(s);
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

/* dup(): otro descriptor al mismo socket (comparten la conexión). */
static mp_obj_t socket_dup(mp_obj_t o)
{
    sock_t *s = aberto(o);
    int fd = descritor_op(s->fd, DESC_DUPLICAR, 0);
    if (fd < 0) sagui_levantar(fd, NULL);
    sock_t *n = novo_socket(fd, s->tcp);
    memcpy(n->opcoes, s->opcoes, sizeof n->opcoes);
    aplicar_tempo(n, s->tempo_ms);   /* novo_socket aplicó el plazo por defecto: el flag O_NONBLOCK es el del fichero compartido */
    n->porta_local = s->porta_local;
    return MP_OBJ_FROM_PTR(n);
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_dup_obj, socket_dup);

/* detach(): suelta el descriptor sin cerrarlo y lo devuelve. */
static mp_obj_t socket_detach(mp_obj_t o)
{
    sock_t *s = aberto(o);
    int fd = s->fd;
    s->fd = -1;
    return MP_OBJ_NEW_SMALL_INT(fd);
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_detach_obj, socket_detach);

static mp_obj_t socket_getblocking(mp_obj_t o) { return mp_obj_new_bool(aberto(o)->tempo_ms != 0); }
static MP_DEFINE_CONST_FUN_OBJ_1(socket_getblocking_obj, socket_getblocking);

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

/* Qué casilla de opcoes guarda cada opción (-1 = ninguna: se acepta sin efecto). */
static int casilla_opcao(int nivel, int opc)
{
    if (nivel == SOL_SOCKET_) {
        switch (opc) {
        case 2: return 0;    /* SO_REUSEADDR */
        case 6: return 1;    /* SO_BROADCAST */
        case 9: return 2;    /* SO_KEEPALIVE */
        case 8: return 3;    /* SO_RCVBUF */
        case 7: return 4;    /* SO_SNDBUF */
        case 15: return 5;   /* SO_REUSEPORT */
        }
    } else if (nivel == 6 && opc == 1) {
        return 6;            /* TCP_NODELAY */
    }
    return -1;
}

static mp_obj_t socket_setsockopt(size_t n_args, const mp_obj_t *args)
{
    sock_t *s = aberto(args[0]);
    int c = casilla_opcao(mp_obj_get_int(args[1]), mp_obj_get_int(args[2]));
    if (c >= 0) {
        mp_int_t v;
        if (mp_obj_is_int(args[3])) v = mp_obj_get_int(args[3]);
        else {
            mp_buffer_info_t b;
            mp_get_buffer_raise(args[3], &b, MP_BUFFER_READ);
            v = 0;
            for (size_t i = 0; i < b.len && i < 4; i++) v |= (mp_int_t)((const uint8_t *)b.buf)[i] << (8 * i);
        }
        s->opcoes[c] = (int)v;
    }
    (void)n_args;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_setsockopt_obj, 4, 5, socket_setsockopt);

/* El resultado de un connect no bloqueante (SO_ERROR): 0 conectado, o el errno. */
static int erro_conexao(sock_t *s)
{
    int r = opcao_socket(s->fd, OPC_ERRO_CONEXAO, 0);
    return r == 1 ? MP_EINPROGRESS : r == ERRO_RECUSADA ? MP_ECONNREFUSED :
           (r == ERRO_INALCANCAVEL || r == ERRO_SEM_REDE) ? MP_ENETUNREACH : r < 0 ? sagui_errno(r) : 0;
}

/* getsockopt(nivel, opción[, tamaño]): un entero, o bytes si se da el tamaño. */
static mp_obj_t socket_getsockopt(size_t n_args, const mp_obj_t *args)
{
    sock_t *s = aberto(args[0]);
    int nivel = mp_obj_get_int(args[1]), opc = mp_obj_get_int(args[2]), v = 0;
    if (nivel == SOL_SOCKET_ && opc == SO_ERROR_) v = s->tcp ? erro_conexao(s) : 0;
    else if (nivel == SOL_SOCKET_ && opc == SO_TYPE_) v = s->tcp ? SOCK_STREAM : SOCK_DGRAM;
    else {
        int c = casilla_opcao(nivel, opc);
        if (c >= 0) v = s->opcoes[c];
    }
    if (n_args > 3) {
        uint8_t b[4] = { v, v >> 8, v >> 16, v >> 24 };
        return mp_obj_new_bytes(b, 4);
    }
    return mp_obj_new_int(v);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_getsockopt_obj, 3, 4, socket_getsockopt);

/* ------------------------------------------------------------ conexión */

static mp_obj_t socket_connect(mp_obj_t o, mp_obj_t addr)
{
    sock_t *s = aberto(o);
    int porta;
    uint32_t ip = endereco(addr, &porta);
    int r = SEM_GIL(conectar(s->fd, ip, porta));
    if (r < 0) sagui_levantar_rede(r);   /* no bloqueante: EINPROGRESS (ERRO_EM_CURSO) */
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_connect_obj, socket_connect);

/* connect_ex: lo mismo, pero devuelve el errno en vez de levantar (0 = bien). Un
 * nombre que no se resuelve sí levanta gaierror, como en CPython. */
static mp_obj_t socket_connect_ex(mp_obj_t o, mp_obj_t addr)
{
    sock_t *s = aberto(o);
    int porta;
    uint32_t ip = endereco(addr, &porta);
    int r = SEM_GIL(conectar(s->fd, ip, porta));
    return MP_OBJ_NEW_SMALL_INT(r < 0 ? sagui_errno(r) : 0);
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_connect_ex_obj, socket_connect_ex);

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
    int fd;
    for (;;) {
        fd = SEM_GIL(aceitar_conexao(s->fd, &de));
        if (fd != ERRO_INTERROMPIDO) break;
        sagui_rodar_tratadores();
    }
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

#ifdef SAGUI_PORTABLE
int portavel_enderecos_socket(int fd, enderecos_socket_t *e);   /* red_sdl.c */
#endif
static mp_obj_t enderecos(mp_obj_t o, bool remoto)
{
    sock_t *s = aberto(o);
    enderecos_socket_t e;
    memset(&e, 0, sizeof e);
#ifdef SAGUI_PORTABLE
    portavel_enderecos_socket(s->fd, &e);   /* en 64 bits el puntero no cabe en el `unsigned` de la llamada (D459) */
#else
    opcao_socket(s->fd, OPC_ENDERECOS, (unsigned)&e);
#endif
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
static int receber_flags(sock_t *s, void *buf, size_t n, int *erro, int flags)
{
    if (flags & MSG_PEEK) mp_raise_OSError(MP_EOPNOTSUPP);   /* el kernel no deja mirar sin consumir */
    if (s->tempo_ms == 0 || (flags & MSG_DONTWAIT)) {
        int d = disponivel(s->fd);
        if (d == 0) { *erro = MP_EAGAIN; return -1; }
    }
    size_t k = 0;
    for (;;) {
        int r = SEM_GIL(ler(s->fd, (char *)buf + k, n - k));
        if (r == ERRO_INTERROMPIDO) { sagui_rodar_tratadores(); continue; }   /* PEP 475 */
        if (r == ERRO_TENTAR) { if (k) return k; *erro = MP_EAGAIN; return -1; }
        if (r == ERRO_PIPE_QUEBRADO) return k;
        if (r < 0) { if (k) return k; *erro = r; return -2; }
        k += r;
        if (!(flags & MSG_WAITALL) || k >= n || r == 0) return k;
    }
}

static int receber(sock_t *s, void *buf, size_t n, int *erro)
{
    return receber_flags(s, buf, n, erro, 0);
}

static mp_obj_t socket_recv(size_t n_args, const mp_obj_t *args)
{
    sock_t *s = aberto(args[0]);
    mp_int_t n = mp_obj_get_int(args[1]);
    if (n < 0) mp_raise_ValueError(NULL);
    vstr_t v;
    vstr_init_len(&v, n);
    int erro, r = n ? receber_flags(s, v.buf, n, &erro, n_args > 2 ? mp_obj_get_int(args[2]) : 0) : 0;
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
    int erro, r = n ? receber_flags(s, b.buf, n, &erro, n_args > 3 ? mp_obj_get_int(args[3]) : 0) : 0;
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
    int r;
    for (;;) {
        r = SEM_GIL(escrever(s->fd, b.buf, b.len));
        if (r != ERRO_INTERROMPIDO) break;
        sagui_rodar_tratadores();
    }
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
    int flags = n_args > 2 ? mp_obj_get_int(args[2]) : 0;
    if ((s->tempo_ms == 0 || (flags & MSG_DONTWAIT)) && disponivel(s->fd) <= 0) levantar_eagain();
    vstr_t v;
    vstr_init_len(&v, n);
    endereco_rede_t de;
    memset(&de, 0, sizeof de);
    int r;
    for (;;) {
        r = SEM_GIL(receber_de(s->fd, v.buf, n, &de));
        if (r != ERRO_INTERROMPIDO) break;
        sagui_rodar_tratadores();
    }
    if (r < 0) sagui_levantar_rede(r);
    v.len = r;
    mp_obj_t t[2] = { mp_obj_new_bytes_from_vstr(&v), tupla_endereco(de.ip, de.porta) };
    return mp_obj_new_tuple(2, t);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_recvfrom_obj, 2, 3, socket_recvfrom);

/* recvfrom_into(búfer, nbytes=0, flags=0) -> (nbytes, dirección) */
static mp_obj_t socket_recvfrom_into(size_t n_args, const mp_obj_t *args)
{
    sock_t *s = aberto(args[0]);
    mp_buffer_info_t b;
    mp_get_buffer_raise(args[1], &b, MP_BUFFER_WRITE);
    size_t n = b.len;
    if (n_args > 2 && mp_obj_get_int(args[2]) > 0 && (size_t)mp_obj_get_int(args[2]) < n) n = mp_obj_get_int(args[2]);
    int flags = n_args > 3 ? mp_obj_get_int(args[3]) : 0;
    if ((s->tempo_ms == 0 || (flags & MSG_DONTWAIT)) && disponivel(s->fd) <= 0) levantar_eagain();
    endereco_rede_t de;
    memset(&de, 0, sizeof de);
    int r;
    for (;;) {
        r = SEM_GIL(receber_de(s->fd, b.buf, n, &de));
        if (r != ERRO_INTERROMPIDO) break;
        sagui_rodar_tratadores();
    }
    if (r < 0) sagui_levantar_rede(r);
    mp_obj_t t[2] = { MP_OBJ_NEW_SMALL_INT(r), tupla_endereco(de.ip, de.porta) };
    return mp_obj_new_tuple(2, t);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_recvfrom_into_obj, 2, 4, socket_recvfrom_into);

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
        /* Lo que dice el kernel (poll de la D401): el connect no bloqueante dispara POLLOUT al
         * acabar, la escucha POLLIN con una conexión en cola... */
        sondagem_t p = { s->fd, (short)(((arg & MP_STREAM_POLL_RD) ? SONDA_LER : 0) | ((arg & MP_STREAM_POLL_WR) ? SONDA_ESCREVER : 0)), 0 };
        if (sondar(&p, 1, 0) >= 0) {
            if (p.prontos & SONDA_LER) pronto |= MP_STREAM_POLL_RD;
            if (p.prontos & SONDA_ESCREVER) pronto |= MP_STREAM_POLL_WR;
            if (p.prontos & SONDA_ERRO) pronto |= MP_STREAM_POLL_ERR;
            if (p.prontos & SONDA_FECHADO) pronto |= MP_STREAM_POLL_HUP;
            return pronto & (arg | MP_STREAM_POLL_ERR | MP_STREAM_POLL_HUP);
        }
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

/* socket.family, .type y .proto como en CPython; lo demás son los métodos. */
static void socket_atributo(mp_obj_t o, qstr nome, mp_obj_t *dest)
{
    if (dest[0] != MP_OBJ_NULL) return;   /* solo lectura */
    sock_t *s = MP_OBJ_TO_PTR(o);
    if (nome == MP_QSTR_family) dest[0] = MP_OBJ_NEW_SMALL_INT(AF_INET);
    else if (nome == MP_QSTR_type) dest[0] = MP_OBJ_NEW_SMALL_INT(s->tcp ? SOCK_STREAM : SOCK_DGRAM);
    else if (nome == MP_QSTR_proto) dest[0] = MP_OBJ_NEW_SMALL_INT(0);
    else dest[1] = MP_OBJ_SENTINEL;
}

static const mp_rom_map_elem_t socket_metodos_tab[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&socket_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&mp_identity_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&socket_exit_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&socket_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_fileno), MP_ROM_PTR(&socket_fileno_obj) },
    { MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&socket_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_connect_ex), MP_ROM_PTR(&socket_connect_ex_obj) },
    { MP_ROM_QSTR(MP_QSTR_dup), MP_ROM_PTR(&socket_dup_obj) },
    { MP_ROM_QSTR(MP_QSTR_detach), MP_ROM_PTR(&socket_detach_obj) },
    { MP_ROM_QSTR(MP_QSTR_getsockopt), MP_ROM_PTR(&socket_getsockopt_obj) },
    { MP_ROM_QSTR(MP_QSTR_recvfrom_into), MP_ROM_PTR(&socket_recvfrom_into_obj) },
    { MP_ROM_QSTR(MP_QSTR_getblocking), MP_ROM_PTR(&socket_getblocking_obj) },
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
    attr, socket_atributo,
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

/* Servicios de /etc/services que más se usan (SaguiOS no tiene el archivo). */
static const struct { const char *nome; int porta; } servicos[] = {
    { "echo", 7 }, { "ftp-data", 20 }, { "ftp", 21 }, { "ssh", 22 }, { "telnet", 23 }, { "smtp", 25 },
    { "domain", 53 }, { "tftp", 69 }, { "http", 80 }, { "pop3", 110 }, { "ntp", 123 }, { "imap", 143 },
    { "snmp", 161 }, { "ldap", 389 }, { "https", 443 }, { "smtps", 465 }, { "submission", 587 },
    { "imaps", 993 }, { "pop3s", 995 }, { "mysql", 3306 }, { "postgresql", 5432 }, { "vnc", 5900 },
};

static int porta_de_servico(const char *nome)
{
    for (unsigned i = 0; i < sizeof servicos / sizeof servicos[0]; i++)
        if (!strcmp(servicos[i].nome, nome)) return servicos[i].porta;
    return -1;
}

/* Un puerto como número, o como nombre de servicio (str). */
static int porta_de(mp_obj_t o)
{
    if (o == mp_const_none) return 0;
    if (mp_obj_is_int(o)) return mp_obj_get_int(o);
    char n[32];
    sagui_str_para_latin1(o, n, sizeof n);
    char *fim;
    long v = 0;
    bool digitos = n[0] != 0;
    for (fim = n; *fim; fim++) {
        if (*fim < '0' || *fim > '9') { digitos = false; break; }
        v = v * 10 + (*fim - '0');
    }
    if (digitos) return (int)v;
    int p = porta_de_servico(n);
    if (p < 0) {
        mp_obj_t a[2] = { MP_OBJ_NEW_SMALL_INT(MP_EINVAL), MP_OBJ_NEW_QSTR(MP_QSTR_Servname_not_supported) };
        nlr_raise(mp_obj_new_exception_args(&tipo_gaierror, 2, a));
    }
    return p;
}

/* getaddrinfo(host, port, family=0, type=0, proto=0, flags=0): el DNS del
 * sistema (SIS_RESOLVER); solo IPv4. Sin tipo, una entrada TCP y otra UDP, como CPython. */
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
    int flags = a[5].u_int;
    uint32_t ip;
    if (a[0].u_obj == mp_const_none) ip = (flags & 1) ? 0 : 0x7F000001;   /* AI_PASSIVE: cualquiera; si no, loopback */
    else ip = resolver_nome(a[0].u_obj);
    int porta = porta_de(a[1].u_obj);
    int tipos[2] = { SOCK_STREAM, SOCK_DGRAM };
    int n = 2, ini = 0;
    if (a[3].u_int) { tipos[0] = a[3].u_int & 0xF; n = 1; }
    if (tipos[0] != SOCK_STREAM && tipos[0] != SOCK_DGRAM) mp_raise_OSError(MP_EINVAL);
    mp_obj_t lista = mp_obj_new_list(0, NULL);
    for (int i = ini; i < n; i++) {
        mp_obj_t canon = MP_OBJ_NEW_QSTR(MP_QSTR_);
        if ((flags & 2) && a[0].u_obj != mp_const_none) canon = a[0].u_obj;   /* AI_CANONNAME */
        mp_obj_t t[5] = {
            MP_OBJ_NEW_SMALL_INT(AF_INET), MP_OBJ_NEW_SMALL_INT(tipos[i]),
            MP_OBJ_NEW_SMALL_INT(a[4].u_int ? a[4].u_int : (tipos[i] == SOCK_STREAM ? 6 : 17)),
            canon, tupla_endereco(ip, porta),
        };
        mp_obj_list_append(lista, mp_obj_new_tuple(5, t));
    }
    return lista;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(socket_getaddrinfo_obj, 2, socket_getaddrinfo);

/* getservbyname(nombre, protocolo=None) -> puerto */
static mp_obj_t socket_getservbyname(size_t n_args, const mp_obj_t *args)
{
    (void)n_args;
    char n[32];
    sagui_str_para_latin1(args[0], n, sizeof n);
    int p = porta_de_servico(n);
    if (p < 0) mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("service/proto not found"));
    return MP_OBJ_NEW_SMALL_INT(p);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_getservbyname_obj, 1, 2, socket_getservbyname);

static mp_obj_t socket_getservbyport(size_t n_args, const mp_obj_t *args)
{
    (void)n_args;
    int p = mp_obj_get_int(args[0]);
    for (unsigned i = 0; i < sizeof servicos / sizeof servicos[0]; i++)
        if (servicos[i].porta == p) return mp_obj_new_str(servicos[i].nome, strlen(servicos[i].nome));
    mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("port/proto not found"));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_getservbyport_obj, 1, 2, socket_getservbyport);

static mp_obj_t socket_getprotobyname(mp_obj_t n_in)
{
    const char *n = mp_obj_str_get_str(n_in);
    if (!strcmp(n, "tcp")) return MP_OBJ_NEW_SMALL_INT(6);
    if (!strcmp(n, "udp")) return MP_OBJ_NEW_SMALL_INT(17);
    if (!strcmp(n, "icmp")) return MP_OBJ_NEW_SMALL_INT(1);
    if (!strcmp(n, "ip")) return MP_OBJ_NEW_SMALL_INT(0);
    mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("protocol not found"));
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_getprotobyname_obj, socket_getprotobyname);

static mp_obj_t socket_htons(mp_obj_t v) { uint32_t x = mp_obj_get_int(v) & 0xFFFF; return MP_OBJ_NEW_SMALL_INT((x >> 8) | ((x & 255) << 8)); }
static MP_DEFINE_CONST_FUN_OBJ_1(socket_htons_obj, socket_htons);
static mp_obj_t socket_htonl(mp_obj_t v)
{
    uint32_t x = (uint32_t)mp_obj_get_int_truncated(v);
    return mp_obj_new_int_from_uint((x >> 24) | ((x >> 8) & 0xFF00) | ((x << 8) & 0xFF0000) | (x << 24));
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_htonl_obj, socket_htonl);

/* inet_pton(AF_INET, "a.b.c.d") y inet_ntop(AF_INET, bytes): solo IPv4 y sin resolver nombres. */
static mp_obj_t socket_inet_pton(mp_obj_t af, mp_obj_t texto)
{
    if (mp_obj_get_int(af) != AF_INET) mp_raise_OSError(MP_EAFNOSUPPORT);
    const char *t = mp_obj_str_get_str(texto);
    unsigned v[4], k = 0, cifras = 0;
    memset(v, 0, sizeof v);
    for (const char *p = t;; p++) {
        if (*p >= '0' && *p <= '9') { v[k] = v[k] * 10 + (*p - '0'); if (++cifras > 3 || v[k] > 255) goto mal; }
        else if ((*p == '.' && cifras && k < 3) || (*p == 0 && cifras && k == 3)) { if (*p == 0) break; k++; cifras = 0; }
        else goto mal;
    }
    uint8_t b[4] = { v[0], v[1], v[2], v[3] };
    return mp_obj_new_bytes(b, 4);
mal:
    mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("illegal IP address string passed to inet_pton"));
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_inet_pton_obj, socket_inet_pton);

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

static mp_obj_t socket_inet_ntop(mp_obj_t af, mp_obj_t b)
{
    if (mp_obj_get_int(af) != AF_INET) mp_raise_OSError(MP_EAFNOSUPPORT);
    return socket_inet_ntoa(b);
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_inet_ntop_obj, socket_inet_ntop);

static mp_obj_t socket_gethostbyname_ex(mp_obj_t host)
{
    mp_obj_t ip = texto_ip(resolver_nome(host));
    mp_obj_t t[3] = { host, mp_obj_new_list(0, NULL), mp_obj_new_list(1, &ip) };
    return mp_obj_new_tuple(3, t);
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_gethostbyname_ex_obj, socket_gethostbyname_ex);

static mp_obj_t socket_getfqdn(size_t n_args, const mp_obj_t *args)
{
    return n_args && mp_obj_is_true(args[0]) ? args[0] : MP_OBJ_NEW_QSTR(MP_QSTR_sagui);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_getfqdn_obj, 0, 1, socket_getfqdn);

/* getnameinfo(dirección, banderas): solo numérico (no hay DNS inverso). */
static mp_obj_t socket_getnameinfo(mp_obj_t addr, mp_obj_t flags)
{
    (void)flags;
    size_t n;
    mp_obj_t *e;
    mp_obj_get_array(addr, &n, &e);
    if (n < 2) mp_raise_TypeError(MP_ERROR_TEXT("address must be (host, port)"));
    char p[12];
    snprintf(p, sizeof p, "%d", (int)mp_obj_get_int(e[1]));
    mp_obj_t t[2] = { texto_ip(resolver_nome(e[0])), mp_obj_new_str(p, strlen(p)) };
    return mp_obj_new_tuple(2, t);
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_getnameinfo_obj, socket_getnameinfo);

/* create_connection(dirección, timeout=<por defecto>, source_address=None) */
static mp_obj_t socket_create_connection(size_t n_args, const mp_obj_t *pos, mp_map_t *kw)
{
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_address, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_timeout, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_source_address, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
    };
    mp_arg_val_t a[3];
    mp_arg_parse_all(n_args, pos, kw, 3, permitidos, a);
    int porta;
    uint32_t ip = endereco(a[0].u_obj, &porta);
    int fd = criar_socket(SOCK_TCP);
    if (fd < 0) sagui_levantar(fd, NULL);
    sock_t *s = novo_socket(fd, true);
    mp_obj_t o = MP_OBJ_FROM_PTR(s);
    if (a[1].u_obj != MP_OBJ_NULL) aplicar_tempo(s, tempo_de(a[1].u_obj));
    int r = SEM_GIL(conectar(s->fd, ip, porta));
    if (r < 0) {
        socket_close(o);
        sagui_levantar_rede(r);
    }
    return o;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(socket_create_connection_obj, 1, socket_create_connection);

/* create_server(dirección, backlog=None, reuse_port=False): socket TCP ya en escucha. */
static mp_obj_t socket_create_server(size_t n_args, const mp_obj_t *pos, mp_map_t *kw)
{
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_address, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_family, MP_ARG_INT, {.u_int = AF_INET} },
        { MP_QSTR_backlog, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_reuse_port, MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_dualstack_ipv6, MP_ARG_BOOL, {.u_bool = false} },
    };
    mp_arg_val_t a[5];
    mp_arg_parse_all(n_args, pos, kw, 5, permitidos, a);
    if (a[1].u_int != AF_INET) mp_raise_OSError(MP_EAFNOSUPPORT);
    int porta;
    endereco(a[0].u_obj, &porta);
    int fd = criar_socket(SOCK_TCP);
    if (fd < 0) sagui_levantar(fd, NULL);
    sock_t *s = novo_socket(fd, true);
    mp_obj_t o = MP_OBJ_FROM_PTR(s);
    int fila = a[2].u_obj == mp_const_none ? 5 : mp_obj_get_int(a[2].u_obj);
    s->porta_local = porta;
    int r = escutar(s->fd, porta, fila < 1 ? 1 : fila);
    if (r < 0) {
        socket_close(o);
        if (r == ERRO_OCUPADO || r == ERRO_JA_EXISTE) mp_raise_OSError(MP_EADDRINUSE);
        sagui_levantar(r, NULL);
    }
    s->escutando = 1;
    return o;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(socket_create_server_obj, 1, socket_create_server);

static const mp_rom_map_elem_t socket_globais_tab[] = {
    { MP_ROM_QSTR(MP_QSTR_getservbyname), MP_ROM_PTR(&socket_getservbyname_obj) },
    { MP_ROM_QSTR(MP_QSTR_getservbyport), MP_ROM_PTR(&socket_getservbyport_obj) },
    { MP_ROM_QSTR(MP_QSTR_getprotobyname), MP_ROM_PTR(&socket_getprotobyname_obj) },
    { MP_ROM_QSTR(MP_QSTR_htons), MP_ROM_PTR(&socket_htons_obj) },
    { MP_ROM_QSTR(MP_QSTR_ntohs), MP_ROM_PTR(&socket_htons_obj) },
    { MP_ROM_QSTR(MP_QSTR_htonl), MP_ROM_PTR(&socket_htonl_obj) },
    { MP_ROM_QSTR(MP_QSTR_ntohl), MP_ROM_PTR(&socket_htonl_obj) },
    { MP_ROM_QSTR(MP_QSTR_inet_pton), MP_ROM_PTR(&socket_inet_pton_obj) },
    { MP_ROM_QSTR(MP_QSTR_inet_ntop), MP_ROM_PTR(&socket_inet_ntop_obj) },
    { MP_ROM_QSTR(MP_QSTR_gethostbyname_ex), MP_ROM_PTR(&socket_gethostbyname_ex_obj) },
    { MP_ROM_QSTR(MP_QSTR_getfqdn), MP_ROM_PTR(&socket_getfqdn_obj) },
    { MP_ROM_QSTR(MP_QSTR_getnameinfo), MP_ROM_PTR(&socket_getnameinfo_obj) },
    { MP_ROM_QSTR(MP_QSTR_create_connection), MP_ROM_PTR(&socket_create_connection_obj) },
    { MP_ROM_QSTR(MP_QSTR_create_server), MP_ROM_PTR(&socket_create_server_obj) },
    { MP_ROM_QSTR(MP_QSTR_AF_UNSPEC), MP_ROM_INT(AF_UNSPEC) },
    { MP_ROM_QSTR(MP_QSTR_AF_INET6), MP_ROM_INT(AF_INET6) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_NONBLOCK), MP_ROM_INT(SOCK_NONBLOCK) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_CLOEXEC), MP_ROM_INT(0x80000) },
    { MP_ROM_QSTR(MP_QSTR_SOMAXCONN), MP_ROM_INT(128) },
    { MP_ROM_QSTR(MP_QSTR_INADDR_ANY), MP_ROM_INT(0) },
    { MP_ROM_QSTR(MP_QSTR_SO_ERROR), MP_ROM_INT(SO_ERROR_) },
    { MP_ROM_QSTR(MP_QSTR_SO_TYPE), MP_ROM_INT(SO_TYPE_) },
    { MP_ROM_QSTR(MP_QSTR_SO_SNDBUF), MP_ROM_INT(7) },
    { MP_ROM_QSTR(MP_QSTR_SO_RCVBUF), MP_ROM_INT(8) },
    { MP_ROM_QSTR(MP_QSTR_SO_LINGER), MP_ROM_INT(13) },
    { MP_ROM_QSTR(MP_QSTR_SO_REUSEPORT), MP_ROM_INT(15) },
    { MP_ROM_QSTR(MP_QSTR_SOL_TCP), MP_ROM_INT(6) },
    { MP_ROM_QSTR(MP_QSTR_SOL_UDP), MP_ROM_INT(17) },
    { MP_ROM_QSTR(MP_QSTR_IPPROTO_IP), MP_ROM_INT(0) },
    { MP_ROM_QSTR(MP_QSTR_MSG_PEEK), MP_ROM_INT(MSG_PEEK) },
    { MP_ROM_QSTR(MP_QSTR_MSG_DONTWAIT), MP_ROM_INT(MSG_DONTWAIT) },
    { MP_ROM_QSTR(MP_QSTR_MSG_WAITALL), MP_ROM_INT(MSG_WAITALL) },
    { MP_ROM_QSTR(MP_QSTR_MSG_NOSIGNAL), MP_ROM_INT(MSG_NOSIGNAL) },
    { MP_ROM_QSTR(MP_QSTR_AI_PASSIVE), MP_ROM_INT(1) },
    { MP_ROM_QSTR(MP_QSTR_AI_CANONNAME), MP_ROM_INT(2) },
    { MP_ROM_QSTR(MP_QSTR_AI_NUMERICHOST), MP_ROM_INT(4) },
    { MP_ROM_QSTR(MP_QSTR_has_ipv6), MP_ROM_FALSE },
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
