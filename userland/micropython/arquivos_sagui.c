/* arquivos_sagui.c - open(), los objetos de archivo, sys.stdin/stdout/stderr,
 * la conversión Latin-1 <-> UTF-8 y los errores de SaguiOS como OSError.
 *
 * Por qué no el VFS de MicroPython (D067): el VFS es una tabla de montaje
 * para placas sin sistema operativo; en SaguiOS quien monta (/, /usb,
 * /home) es el kernel, y las llamadas de archivo ya aceptan cualquier ruta.
 * VfsPosix pediría una capa POSIX (open, stat, opendir...) solo para
 * traducir de vuelta a las mismas llamadas, y no sabría si el archivo se
 * abrió en modo texto o binario, que es donde está la conversión de Latin-1.
 * Aquí los objetos de archivo hablan directamente con libsagui e implementan el
 * protocolo de stream de MicroPython: read/readline/iteración/with/seek vienen
 * listos de py/stream.c.
 *
 * Texto (D068): el str de MicroPython es UTF-8; el terminal, el Editor y los
 * archivos de SaguiOS son Latin-1. En modo texto (el predeterminado de open(), y
 * sys.stdin/stdout/stderr) la lectura convierte Latin-1 -> UTF-8 y la escritura
 * UTF-8 -> Latin-1 (lo que no cabe en Latin-1 pasa a '?'). Quien quiera los bytes
 * tal cual usa el modo binario ('b') o encoding='utf-8'. */
#include <string.h>
#include "py/runtime.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "py/objstr.h"
#include "py/unicode.h"
#include "py/builtin.h"
#include "sagui_mp.h"

#define BUF_LEITURA 1024

/* ------------------------------------------------------------ texto */

void sagui_latin1_para_vstr(vstr_t *v, const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) vstr_add_byte(v, c);
        else { vstr_add_byte(v, 0xC0 | (c >> 6)); vstr_add_byte(v, 0x80 | (c & 0x3F)); }
    }
}

mp_obj_t sagui_str_de_latin1(const char *s)
{
    vstr_t v;
    vstr_init(&v, strlen(s) + 8);
    sagui_latin1_para_vstr(&v, s, strlen(s));
    return mp_obj_new_str_from_vstr(&v);
}

/* Tamaño de un carácter UTF-8 por el primer byte (0 = byte no válido). */
static int tam_utf8(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 0;
}

static unsigned decodificar(const unsigned char *p, int n)
{
    unsigned cp = n == 1 ? p[0] : p[0] & (0x7F >> n);
    for (int i = 1; i < n; i++) cp = cp << 6 | (p[i] & 0x3F);
    return cp;
}

size_t sagui_str_para_latin1(mp_obj_t s, char *buf, size_t max)
{
    size_t n;
    const unsigned char *p = (const unsigned char *)mp_obj_str_get_data(s, &n);
    size_t k = 0;
    for (size_t i = 0; i < n;) {
        int t = tam_utf8(p[i]);
        if (t == 0 || i + t > n) t = 1;
        unsigned cp = t == 1 ? p[i] : decodificar(p + i, t);
        if (k + 1 >= max) mp_raise_ValueError(MP_ERROR_TEXT("path too long"));
        buf[k++] = cp <= 0xFF ? (char)cp : '?';
        i += t;
    }
    buf[k] = 0;
    return k;
}

char *sagui_para_utf8(const char *s, size_t n, size_t *tam, size_t *cap)
{
    if (utf8_check((const byte *)s, n)) {
        char *b = m_new(char, n + 1);
        memcpy(b, s, n);
        b[n] = 0;
        *tam = n;
        *cap = n + 1;
        return b;
    }
    vstr_t v;
    vstr_init(&v, n + n / 4 + 8);
    sagui_latin1_para_vstr(&v, s, n);
    *tam = v.len;
    *cap = v.alloc;
    return v.buf;
}

/* ------------------------------------------------------------ errores */

int sagui_errno(int e)
{
    switch (e) {
    case ERRO_INVALIDO:      return MP_EINVAL;
    case ERRO_NAO_EXISTE:    return MP_ENOENT;
    case ERRO_JA_EXISTE:     return MP_EEXIST;
    case ERRO_SEM_MEMORIA:   return MP_ENOMEM;
    case ERRO_ENDERECO:      return MP_EFAULT;
    case ERRO_NAO_E_PASTA:   return MP_ENOTDIR;
    case ERRO_E_PASTA:       return MP_EISDIR;
    case ERRO_NAO_VAZIA:     return MP_ENOTEMPTY;
    case ERRO_LIMITE:        return MP_EMFILE;
    case ERRO_DESCRITOR:     return MP_EBADF;
    case ERRO_PIPE_QUEBRADO: return MP_EPIPE;
    case ERRO_SEM_PROCESSO:  return MP_ESRCH;
    case ERRO_FORMATO:       return MP_ENOEXEC;
    case ERRO_ACESSO:        return MP_EPERM;
    case ERRO_OCUPADO:       return MP_EBUSY;
    case ERRO_SO_LEITURA:    return MP_EROFS;
    case ERRO_TEMPO:         return MP_ETIMEDOUT;
    case ERRO_SEM_REDE:      return MP_ENETUNREACH;
    case ERRO_RECUSADA:      return MP_ECONNREFUSED;
    case ERRO_INALCANCAVEL:  return MP_EHOSTUNREACH;
    case ERRO_CONEXAO_CAIU:  return MP_ECONNRESET;
    default:                 return MP_EIO;
    }
}

/* OSError(errno, "texto de SaguiOS: contexto"). El número sirve para quien
 * compara e.errno con errno.ENOENT; el texto, en español como el resto del
 * sistema, es lo que la persona lee en el traceback. */
NORETURN void sagui_levantar_obj(int erro, mp_obj_t contexto)
{
    vstr_t v;
    vstr_init(&v, 64);
    const char *m = nome_erro(erro);
    sagui_latin1_para_vstr(&v, m, strlen(m));
    if (contexto != MP_OBJ_NULL) {
        vstr_add_str(&v, ": ");
        size_t n;
        const char *c = mp_obj_str_get_data(contexto, &n);
        vstr_add_strn(&v, c, n);
    }
    mp_obj_t args[2] = { MP_OBJ_NEW_SMALL_INT(sagui_errno(erro)), mp_obj_new_str_from_vstr(&v) };
    nlr_raise(mp_obj_new_exception_args(&mp_type_OSError, 2, args));
}

NORETURN void sagui_levantar(int erro, const char *contexto)
{
    sagui_levantar_obj(erro, contexto ? sagui_str_de_latin1(contexto) : MP_OBJ_NULL);
}

/* ------------------------------------------------------------ objetos de archivo */

typedef struct {
    mp_obj_base_t base;
    int      fd;           /* -1 = cerrado */
    uint8_t  latin1;       /* modo texto con conversión (el predeterminado) */
    uint8_t  meu;          /* cerrar el fd junto con el objeto (no vale para 0, 1 y 2) */
    uint8_t  pendente;     /* lectura: 2.º byte de un carácter ya entregado a medias */
    uint8_t  nparcial;     /* escritura: bytes de un carácter UTF-8 todavía incompleto */
    uint8_t  parcial[4];
    uint8_t  um_por_vez;   /* leer del kernel un byte por vez (entrada estándar; ver abajo) */
    uint16_t ini, fim;     /* bytes crudos leídos y todavía no entregados, en buf */
    uint8_t *buf;
} arquivo_t;

/* Lo que ya llegó del kernel pero no se entregó: el fd va adelantado. */
static void descartar_leitura(arquivo_t *a)
{
    if (a->fim > a->ini) posicionar(a->fd, -(int)(a->fim - a->ini), POS_ATUAL);
    a->ini = a->fim = 0;
    a->pendente = 0;
}

static int encher(arquivo_t *a)
{
    if (!a->buf) a->buf = m_new(uint8_t, BUF_LEITURA);
    /* La entrada estándar (terminal o pipe) puede esperar mucho: sin el GIL
     * (D170), para que otro hilo siga mientras uno espera una línea. */
    int r = a->um_por_vez ? SEM_GIL(ler(a->fd, a->buf, 1)) : ler(a->fd, a->buf, BUF_LEITURA);
    a->ini = 0;
    a->fim = r > 0 ? r : 0;
    return r;
}

static mp_uint_t arquivo_ler(mp_obj_t o, void *destino, mp_uint_t n, int *erro)
{
    arquivo_t *a = MP_OBJ_TO_PTR(o);
    uint8_t *d = destino;
    if (a->fd < 0) { *erro = MP_EBADF; return MP_STREAM_ERROR; }
    mp_uint_t k = 0;
    if (a->pendente && n) { d[k++] = a->pendente; a->pendente = 0; }
    while (k < n) {
        if (a->ini == a->fim) {
            /* Ya hay algo: devuelve sin esperar más (terminal y pipe bloquearían). */
            if (k) break;
            /* Lectura grande en binario: directo al destino, sin copiar. */
            if (!a->latin1 && !a->um_por_vez && n - k >= BUF_LEITURA) {
                int r = ler(a->fd, d + k, n - k);
                if (r < 0 && r != ERRO_PIPE_QUEBRADO) { *erro = sagui_errno(r); return MP_STREAM_ERROR; }
                return k + (r > 0 ? r : 0);
            }
            int r = encher(a);
            if (r == 0 || r == ERRO_PIPE_QUEBRADO) break;   /* fin */
            if (r < 0) { *erro = sagui_errno(r); return MP_STREAM_ERROR; }
        }
        if (!a->latin1) {
            mp_uint_t t = a->fim - a->ini;
            if (t > n - k) t = n - k;
            memcpy(d + k, a->buf + a->ini, t);
            a->ini += t;
            k += t;
            continue;
        }
        unsigned char c = a->buf[a->ini++];
        if (c < 0x80) { d[k++] = c; continue; }
        d[k++] = 0xC0 | (c >> 6);
        if (k < n) d[k++] = 0x80 | (c & 0x3F);
        else a->pendente = 0x80 | (c & 0x3F);
    }
    return k;
}

/* Escribe convirtiendo UTF-8 -> Latin-1. Un carácter puede llegar partido
 * entre dos llamadas (el print escribe a trozos): el principio queda en
 * a->parcial hasta completarse. */
static int escrever_latin1(arquivo_t *a, const uint8_t *s, mp_uint_t n)
{
    char saida[256];
    unsigned k = 0;
    for (mp_uint_t i = 0; i < n; i++) {
        uint8_t b = s[i];
        if (a->nparcial) {
            if ((b & 0xC0) == 0x80) {
                a->parcial[a->nparcial++] = b;
                int t = tam_utf8(a->parcial[0]);
                if (a->nparcial < t) continue;
                unsigned cp = decodificar(a->parcial, t);
                saida[k++] = cp <= 0xFF ? (char)cp : '?';
                a->nparcial = 0;
                goto cheio;
            }
            saida[k++] = '?';   /* secuencia cortada: el byte actual reinicia */
            a->nparcial = 0;
            if (k == sizeof saida) { int r = escrever_tudo(a->fd, saida, k); if (r < 0) return r; k = 0; }
        }
        if (b < 0x80) saida[k++] = (char)b;
        else if (tam_utf8(b) >= 2) { a->parcial[0] = b; a->nparcial = 1; }
        else saida[k++] = '?';
    cheio:
        if (k == sizeof saida) {
            int r = escrever_tudo(a->fd, saida, k);
            if (r < 0) return r;
            k = 0;
        }
    }
    if (k) {
        int r = escrever_tudo(a->fd, saida, k);
        if (r < 0) return r;
    }
    return 0;
}

static mp_uint_t arquivo_escrever(mp_obj_t o, const void *s, mp_uint_t n, int *erro)
{
    arquivo_t *a = MP_OBJ_TO_PTR(o);
    if (a->fd < 0) { *erro = MP_EBADF; return MP_STREAM_ERROR; }
    if (a->fim > a->ini || a->pendente) descartar_leitura(a);
    int r = a->latin1 ? escrever_latin1(a, s, n) : escrever_tudo(a->fd, s, n);
    if (r < 0) { *erro = sagui_errno(r); return MP_STREAM_ERROR; }
    return n;
}

static mp_uint_t arquivo_ioctl(mp_obj_t o, mp_uint_t pedido, uintptr_t arg, int *erro)
{
    arquivo_t *a = MP_OBJ_TO_PTR(o);
    if (a->fd < 0 && pedido != MP_STREAM_CLOSE) { *erro = MP_EBADF; return MP_STREAM_ERROR; }
    switch (pedido) {
    case MP_STREAM_FLUSH:
        return 0;   /* no hay buffer de escritura: todo ya fue al kernel */
    case MP_STREAM_SEEK: {
        struct mp_stream_seek_t *s = (struct mp_stream_seek_t *)arg;
        mp_off_t desloc = s->offset;
        if (s->whence == MP_SEEK_CUR) desloc -= a->fim - a->ini;
        a->ini = a->fim = 0;
        a->pendente = 0;
        int r = posicionar(a->fd, desloc, s->whence);
        if (r < 0) { *erro = r == ERRO_INVALIDO ? MP_ESPIPE : sagui_errno(r); return MP_STREAM_ERROR; }
        s->offset = r;
        return 0;
    }
    case MP_STREAM_CLOSE:
        if (a->fd >= 0 && a->meu) fechar(a->fd);
        a->fd = -1;
        return 0;
    case MP_STREAM_GET_FILENO:
        return a->fd;
    case MP_STREAM_POLL: {
        /* Los archivos están siempre listos; pipes y el terminal virtual dicen
         * por disponivel() si hay algo que leer. */
        mp_uint_t pronto = arg & MP_STREAM_POLL_WR;
        if (arg & MP_STREAM_POLL_RD) {
            int d = a->fim > a->ini ? 1 : disponivel(a->fd);
            if (d != 0) pronto |= MP_STREAM_POLL_RD;   /* > 0, fin o "no sé" (terminal) */
        }
        return pronto;
    }
    default:
        *erro = MP_EINVAL;
        return MP_STREAM_ERROR;
    }
}

static void arquivo_imprimir(const mp_print_t *print, mp_obj_t o, mp_print_kind_t tipo)
{
    (void)tipo;
    arquivo_t *a = MP_OBJ_TO_PTR(o);
    mp_printf(print, "<io.%s %d>", mp_obj_get_type_str(o), a->fd);
}

static mp_obj_t arquivo_fileno(mp_obj_t o)
{
    arquivo_t *a = MP_OBJ_TO_PTR(o);
    return MP_OBJ_NEW_SMALL_INT(a->fd);
}
static MP_DEFINE_CONST_FUN_OBJ_1(arquivo_fileno_obj, arquivo_fileno);

static mp_obj_t arquivo_isatty(mp_obj_t o)
{
    arquivo_t *a = MP_OBJ_TO_PTR(o);
    return mp_obj_new_bool(a->fd >= 0 && tipo_descritor(a->fd) == TIPO_TERMINAL);
}
static MP_DEFINE_CONST_FUN_OBJ_1(arquivo_isatty_obj, arquivo_isatty);

static const mp_rom_map_elem_t arquivo_metodos_tab[] = {
    { MP_ROM_QSTR(MP_QSTR_fileno), MP_ROM_PTR(&arquivo_fileno_obj) },
    { MP_ROM_QSTR(MP_QSTR_isatty), MP_ROM_PTR(&arquivo_isatty_obj) },
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_readlines), MP_ROM_PTR(&mp_stream_unbuffered_readlines_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_seek), MP_ROM_PTR(&mp_stream_seek_obj) },
    { MP_ROM_QSTR(MP_QSTR_tell), MP_ROM_PTR(&mp_stream_tell_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush), MP_ROM_PTR(&mp_stream_flush_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&mp_identity_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&mp_stream___exit___obj) },
};
static MP_DEFINE_CONST_DICT(arquivo_metodos, arquivo_metodos_tab);

static const mp_stream_p_t stream_binario = {
    .read = arquivo_ler,
    .write = arquivo_escrever,
    .ioctl = arquivo_ioctl,
};

static const mp_stream_p_t stream_texto = {
    .read = arquivo_ler,
    .write = arquivo_escrever,
    .ioctl = arquivo_ioctl,
    .is_text = true,
};

/* sys.stdin/stdout/stderr y sus .buffer (bytes sin conversión). Son
 * objetos estáticos, fuera del recolector: el buffer de lectura también debe ser
 * (el recolector no mira dentro de ellos y liberaría un buffer asignado con m_new). */
static uint8_t buf_padrao[6][BUF_LEITURA];
#define PADRAO(tipo, fd, latin1, i) { { &tipo }, fd, latin1, 0, 0, 0, {0}, 0, 0, 0, buf_padrao[i] }
arquivo_t mp_sys_stdin_obj = PADRAO(sagui_tipo_texto, 0, 1, 0);
arquivo_t mp_sys_stdout_obj = PADRAO(sagui_tipo_texto, 1, 1, 1);
arquivo_t mp_sys_stderr_obj = PADRAO(sagui_tipo_texto, 2, 1, 2);
static arquivo_t stdin_buffer = PADRAO(sagui_tipo_binario, 0, 0, 3);
static arquivo_t stdout_buffer = PADRAO(sagui_tipo_binario, 1, 0, 4);
static arquivo_t stderr_buffer = PADRAO(sagui_tipo_binario, 2, 0, 5);

/* La entrada estándar, cuando es el terminal o un pipe, se lee un byte por vez
 * (como ler_linha de libsagui): así python nunca consume lo que viene
 * después de la línea que pidió, y el resto queda para quien lea el mismo pipe
 * después (otro programa en un script de sagsh, o un os.system). */
void sagui_iniciar_padrao(void)
{
    bool arquivo = tipo_descritor(0) == TIPO_ARQUIVO;
    mp_sys_stdin_obj.um_por_vez = stdin_buffer.um_por_vez = !arquivo;
}

/* Atributos: "buffer" (solo en los tres estándar), "name", "mode" y "closed";
 * lo demás pasa a los métodos (dest[1] = MP_OBJ_SENTINEL). */
static void arquivo_atributo(mp_obj_t o, qstr nome, mp_obj_t *dest)
{
    if (dest[0] != MP_OBJ_NULL) return;   /* solo lectura */
    arquivo_t *a = MP_OBJ_TO_PTR(o);
    if (nome == MP_QSTR_buffer) {
        if (a == &mp_sys_stdin_obj) { dest[0] = MP_OBJ_FROM_PTR(&stdin_buffer); return; }
        if (a == &mp_sys_stdout_obj) { dest[0] = MP_OBJ_FROM_PTR(&stdout_buffer); return; }
        if (a == &mp_sys_stderr_obj) { dest[0] = MP_OBJ_FROM_PTR(&stderr_buffer); return; }
    } else if (nome == MP_QSTR_closed) {
        dest[0] = mp_obj_new_bool(a->fd < 0);
        return;
    }
    dest[1] = MP_OBJ_SENTINEL;
}

MP_DEFINE_CONST_OBJ_TYPE(
    sagui_tipo_binario,
    MP_QSTR_FileIO,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    print, arquivo_imprimir,
    attr, arquivo_atributo,
    protocol, &stream_binario,
    locals_dict, &arquivo_metodos
    );

MP_DEFINE_CONST_OBJ_TYPE(
    sagui_tipo_texto,
    MP_QSTR_TextIOWrapper,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    print, arquivo_imprimir,
    attr, arquivo_atributo,
    protocol, &stream_texto,
    locals_dict, &arquivo_metodos
    );

/* ------------------------------------------------------------ open() */

/* open(arquivo, mode='r', buffering=-1, encoding=None, errors=None,
 *      newline=None, closefd=True)
 * Modos: r, w, a, x, con + (leer y escribir) y b (binario) o t (texto). */
static mp_obj_t sagui_open(size_t n_args, const mp_obj_t *pos, mp_map_t *kw)
{
    enum { A_ARQ, A_MODO, A_BUF, A_COD, A_ERROS, A_LINHA, A_FECHAR };
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_file, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_mode, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_QSTR(MP_QSTR_r)} },
        { MP_QSTR_buffering, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_encoding, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_errors, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_newline, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_closefd, MP_ARG_BOOL, {.u_bool = true} },
    };
    mp_arg_val_t arg[MP_ARRAY_SIZE(permitidos)];
    mp_arg_parse_all(n_args, pos, kw, MP_ARRAY_SIZE(permitidos), permitidos, arg);

    const char *m = mp_obj_str_get_str(arg[A_MODO].u_obj);
    int modo = 0;
    bool binario = false, mais = false, exclusivo = false;
    char base = 0;
    for (; *m; m++) {
        switch (*m) {
        case 'r': case 'w': case 'a': case 'x':
            if (base) goto invalido;
            base = *m;
            break;
        case '+': mais = true; break;
        case 'b': binario = true; break;
        case 't': break;
        default: goto invalido;
        }
    }
    switch (base) {
    case 'r': modo = ABRIR_LER; break;
    case 'w': modo = ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR; break;
    case 'a': modo = ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_ACRESCENTAR; break;
    case 'x': modo = ABRIR_ESCREVER | ABRIR_CRIAR; exclusivo = true; break;
    default: goto invalido;
    }
    if (mais) modo |= ABRIR_LER | ABRIR_ESCREVER;

    bool latin1 = !binario;
    if (arg[A_COD].u_obj != mp_const_none) {
        if (binario) mp_raise_ValueError(MP_ERROR_TEXT("binary mode doesn't take an encoding argument"));
        char cod[16];
        sagui_str_para_latin1(arg[A_COD].u_obj, cod, sizeof cod);
        for (char *p = cod; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
        if (!strcmp(cod, "utf-8") || !strcmp(cod, "utf8")) latin1 = false;
        else if (strcmp(cod, "latin-1") && strcmp(cod, "latin1") && strcmp(cod, "iso-8859-1") && strcmp(cod, "iso8859-1"))
            mp_raise_ValueError(MP_ERROR_TEXT("unknown encoding"));
    }

    int fd;
    bool meu = true;
    if (mp_obj_is_small_int(arg[A_ARQ].u_obj)) {
        fd = MP_OBJ_SMALL_INT_VALUE(arg[A_ARQ].u_obj);
        meu = arg[A_FECHAR].u_bool;
    } else {
        char caminho[256];
        sagui_str_para_latin1(arg[A_ARQ].u_obj, caminho, sizeof caminho);
        if (exclusivo) {
            info_arquivo_t i;
            if (info_arquivo(caminho, &i) == 0) sagui_levantar_obj(ERRO_JA_EXISTE, arg[A_ARQ].u_obj);
        }
        fd = abrir(caminho, modo);
        if (fd < 0) sagui_levantar_obj(fd, arg[A_ARQ].u_obj);
    }
    arquivo_t *a = mp_obj_malloc_with_finaliser(arquivo_t, binario ? &sagui_tipo_binario : &sagui_tipo_texto);
    a->fd = fd;
    a->latin1 = latin1;
    a->meu = meu;
    a->pendente = a->nparcial = a->um_por_vez = 0;
    a->ini = a->fim = 0;
    a->buf = NULL;
    return MP_OBJ_FROM_PTR(a);

invalido:
    mp_raise_ValueError(MP_ERROR_TEXT("invalid mode"));
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_open_obj, 1, sagui_open);

mp_obj_t sagui_arquivo_de_fd(int fd, bool texto)
{
    arquivo_t *a = mp_obj_malloc_with_finaliser(arquivo_t, texto ? &sagui_tipo_texto : &sagui_tipo_binario);
    a->fd = fd;
    a->latin1 = texto;
    a->meu = 0;
    a->pendente = a->nparcial = a->um_por_vez = 0;
    a->ini = a->fim = 0;
    a->buf = NULL;
    return MP_OBJ_FROM_PTR(a);
}

/* ------------------------------------------------------------ terminal */

void sagui_saida_texto(int fd, const char *s, size_t n)
{
    arquivo_t *a = fd == 2 ? &mp_sys_stderr_obj : &mp_sys_stdout_obj;
    escrever_latin1(a, (const uint8_t *)s, n);
}

int sagui_entrada_byte(void)
{
    uint8_t c;
    int erro;
    mp_uint_t r = arquivo_ler(MP_OBJ_FROM_PTR(&mp_sys_stdin_obj), &c, 1, &erro);
    return r == 1 ? c : -1;
}

static const mp_print_t print_erros = { &mp_sys_stderr_obj, mp_stream_write_adaptor };
const mp_print_t *sagui_print_erros(void) { return &print_erros; }
