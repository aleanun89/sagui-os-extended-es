/* os_processos_sagui.h - lo POSIX del módulo os: procesos, descriptores y tuberías (D419).
 *
 * Se incluye desde os_sagui.c (una sola unidad de compilación, así que la
 * extracción de qstr de MicroPython ve los nombres). Va sobre las llamadas de
 * la D401: fork (con copia al escribir), execve, waitpid, señales, pipe, dup2 y
 * fcntl. Interfaz de CPython:
 *   fork, _exit, execv, execve, execvp, execvpe, waitpid, wait, kill, killpg,
 *   getppid, getpgid, getuid/geteuid/getgid/getegid, pipe, dup, dup2, close,
 *   read, write, open (con O_*), isatty, set_blocking/get_blocking,
 *   set_inheritable/get_inheritable, WNOHANG, WIFEXITED/WEXITSTATUS/
 *   WIFSIGNALED/WTERMSIG/WIFSTOPPED/WSTOPSIG, waitstatus_to_exitcode, devnull.
 * Lo que no hay en SaguiOS (y por tanto no se ofrece): os.chown, os.setuid, os.nice,
 * os.getgroups, os.fork con hilos vivos en el hijo (el hijo queda con un solo hilo).
 *
 * Las esperas largas (waitpid, read) sueltan el GIL y, si una señal las
 * interrumpe, llaman a los tratadores de Python (sinais_sagui.c) y siguen
 * (PEP 475). Los nombres de archivo y los argumentos van en Latin-1 hacia el
 * kernel, como el resto (D068). */

#define O_RDONLY_    0
#define O_WRONLY_    1
#define O_RDWR_      2
#define O_CREAT_     0x40
#define O_EXCL_      0x80
#define O_TRUNC_     0x200
#define O_APPEND_    0x400
#define O_NONBLOCK_  0x800
#define O_CLOEXEC_   0x80000

#define MAX_ARGS 1024

/* Lista o tupla de str o bytes -> vector de char* (Latin-1) terminado en NULL. */
static char **vetor_texto(mp_obj_t lista)
{
    size_t n;
    mp_obj_t *e;
    mp_obj_get_array(lista, &n, &e);
    if (n > MAX_ARGS) mp_raise_ValueError(MP_ERROR_TEXT("too many arguments"));
    char **v = m_new(char *, n + 1);
    for (size_t i = 0; i < n; i++) {
        size_t k;
        mp_obj_str_get_data(e[i], &k);
        v[i] = m_new(char, k + 1);
        sagui_str_para_latin1(e[i], v[i], k + 1);
    }
    v[n] = NULL;
    return v;
}

/* Un diccionario (nombre -> valor) -> "nombre=valor" para el kernel. */
static char **ambiente_de(mp_obj_t dic)
{
    mp_map_t *m = mp_obj_dict_get_map(dic);
    char **v = m_new(char *, m->used + 1);
    size_t k = 0;
    for (size_t i = 0; i < m->alloc; i++) {
        if (!mp_map_slot_is_filled(m, i)) continue;
        size_t a, b;
        mp_obj_str_get_data(m->table[i].key, &a);
        mp_obj_str_get_data(m->table[i].value, &b);
        char *s = m_new(char, a + b + 2);
        sagui_str_para_latin1(m->table[i].key, s, a + 1);
        size_t la = strlen(s);
        s[la] = '=';
        sagui_str_para_latin1(m->table[i].value, s + la + 1, b + 1);
        v[k++] = s;
    }
    v[k] = NULL;
    return v;
}

/* ------------------------------------------------------------ procesos */

static mp_obj_t os_fork(void)
{
    int r = bifurcar();
    if (r < 0) sagui_levantar(r, NULL);
    if (r == 0) sagui_hilos_pos_fork();   /* el hijo se queda con un solo hilo */
    return MP_OBJ_NEW_SMALL_INT(r);
}
static MP_DEFINE_CONST_FUN_OBJ_0(os_fork_obj, os_fork);

static mp_obj_t os__exit(mp_obj_t codigo)
{
    sair(mp_obj_get_int(codigo));
}
static MP_DEFINE_CONST_FUN_OBJ_1(os__exit_obj, os__exit);

/* execve(ruta, argumentos, entorno): solo vuelve si falla. */
static NORETURN void ejecutar(mp_obj_t ruta, mp_obj_t args, char **env)
{
    char c[CAMINHO_MAX];
    caminho_de(ruta, c);
    char **v = vetor_texto(args);
    if (!v[0]) mp_raise_ValueError(MP_ERROR_TEXT("execv() arg 2 must not be empty"));
    int r = substituir(c, v, env);
    sagui_levantar_obj(r, ruta);
}

static mp_obj_t os_execv(mp_obj_t ruta, mp_obj_t args)
{
    ejecutar(ruta, args, sagui_ambiente());
}
static MP_DEFINE_CONST_FUN_OBJ_2(os_execv_obj, os_execv);

static mp_obj_t os_execve(mp_obj_t ruta, mp_obj_t args, mp_obj_t env)
{
    ejecutar(ruta, args, ambiente_de(env));
}
static MP_DEFINE_CONST_FUN_OBJ_3(os_execve_obj, os_execve);

/* execvp: si el nombre no lleva «/», se busca en PATH (o /bin y /apps/bin). */
static NORETURN void ejecutar_path(mp_obj_t nome, mp_obj_t args, char **env)
{
    char c[CAMINHO_MAX];
    caminho_de(nome, c);
    if (strchr(c, '/')) ejecutar(nome, args, env);
    const char *path = NULL;
    for (char **e = env; *e; e++)
        if (!strncmp(*e, "PATH=", 5)) path = *e + 5;
    if (!path || !*path) path = "/bin:/apps/bin";
    int ultimo = ERRO_NAO_EXISTE;
    char **v = vetor_texto(args);
    if (!v[0]) mp_raise_ValueError(MP_ERROR_TEXT("execv() arg 2 must not be empty"));
    while (*path) {
        const char *fim = strchr(path, ':');
        size_t n = fim ? (size_t)(fim - path) : strlen(path);
        char ruta[CAMINHO_MAX];
        if (n + strlen(c) + 2 < sizeof ruta) {
            memcpy(ruta, path, n);
            ruta[n] = n ? '/' : 0;
            strcpy(ruta + (n ? n + 1 : 0), c);
            int r = substituir(ruta, v, env);
            if (r != ERRO_NAO_EXISTE) ultimo = r;
        }
        path = fim ? fim + 1 : path + n;
    }
    sagui_levantar_obj(ultimo, nome);
}

static mp_obj_t os_execvp(mp_obj_t nome, mp_obj_t args)
{
    ejecutar_path(nome, args, sagui_ambiente());
}
static MP_DEFINE_CONST_FUN_OBJ_2(os_execvp_obj, os_execvp);

static mp_obj_t os_execvpe(mp_obj_t nome, mp_obj_t args, mp_obj_t env)
{
    ejecutar_path(nome, args, ambiente_de(env));
}
static MP_DEFINE_CONST_FUN_OBJ_3(os_execvpe_obj, os_execvpe);

/* waitpid(pid, opciones) -> (pid, estado); con WNOHANG y nadie, (0, 0). */
static mp_obj_t os_waitpid(mp_obj_t pid, mp_obj_t opc)
{
    int p = mp_obj_get_int(pid), o = mp_obj_get_int(opc), estado = 0, r;
    for (;;) {
        estado = 0;
        r = SEM_GIL(esperar_pid(p, &estado, o & ESPERA_SEM_BLOQUEAR));
        if (r != ERRO_INTERROMPIDO) break;
        sagui_rodar_tratadores();   /* PEP 475: tratadores de Python y a esperar otra vez */
    }
    if (r == ERRO_SEM_PROCESSO) mp_raise_OSError(MP_ECHILD);
    if (r < 0) sagui_levantar(r, NULL);
    mp_obj_t t[2] = { MP_OBJ_NEW_SMALL_INT(r), MP_OBJ_NEW_SMALL_INT(estado) };
    return mp_obj_new_tuple(2, t);
}
static MP_DEFINE_CONST_FUN_OBJ_2(os_waitpid_obj, os_waitpid);

static mp_obj_t os_wait(void)
{
    return os_waitpid(MP_OBJ_NEW_SMALL_INT(-1), MP_OBJ_NEW_SMALL_INT(0));
}
static MP_DEFINE_CONST_FUN_OBJ_0(os_wait_obj, os_wait);

/* Estado de waitpid como en Linux: (código << 8) si salió; el número de la señal si lo mató una. */
static mp_obj_t os_WIFEXITED(mp_obj_t s) { return mp_obj_new_bool((mp_obj_get_int(s) & 0x7f) == 0); }
static MP_DEFINE_CONST_FUN_OBJ_1(os_WIFEXITED_obj, os_WIFEXITED);
static mp_obj_t os_WEXITSTATUS(mp_obj_t s) { return MP_OBJ_NEW_SMALL_INT((mp_obj_get_int(s) >> 8) & 0xff); }
static MP_DEFINE_CONST_FUN_OBJ_1(os_WEXITSTATUS_obj, os_WEXITSTATUS);
static mp_obj_t os_WIFSIGNALED(mp_obj_t s)
{
    int x = mp_obj_get_int(s) & 0x7f;
    return mp_obj_new_bool(x != 0 && x != 0x7f);
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_WIFSIGNALED_obj, os_WIFSIGNALED);
static mp_obj_t os_WTERMSIG(mp_obj_t s) { return MP_OBJ_NEW_SMALL_INT(mp_obj_get_int(s) & 0x7f); }
static MP_DEFINE_CONST_FUN_OBJ_1(os_WTERMSIG_obj, os_WTERMSIG);
static mp_obj_t os_WIFSTOPPED(mp_obj_t s) { return mp_obj_new_bool((mp_obj_get_int(s) & 0xff) == 0x7f); }
static MP_DEFINE_CONST_FUN_OBJ_1(os_WIFSTOPPED_obj, os_WIFSTOPPED);
static mp_obj_t os_WSTOPSIG(mp_obj_t s) { return MP_OBJ_NEW_SMALL_INT((mp_obj_get_int(s) >> 8) & 0xff); }
static MP_DEFINE_CONST_FUN_OBJ_1(os_WSTOPSIG_obj, os_WSTOPSIG);

/* waitstatus_to_exitcode: el código de salida, o -(señal) si lo mató una. */
static mp_obj_t os_waitstatus_to_exitcode(mp_obj_t s)
{
    int x = mp_obj_get_int(s);
    if ((x & 0x7f) == 0) return MP_OBJ_NEW_SMALL_INT((x >> 8) & 0xff);
    if ((x & 0xff) != 0x7f) return MP_OBJ_NEW_SMALL_INT(-(x & 0x7f));
    mp_raise_ValueError(MP_ERROR_TEXT("process stopped"));
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_waitstatus_to_exitcode_obj, os_waitstatus_to_exitcode);

static mp_obj_t os_kill(mp_obj_t pid, mp_obj_t sinal)
{
    int r = sinal_op(SINOP_ENVIAR, (unsigned)mp_obj_get_int(pid), (unsigned)mp_obj_get_int(sinal), 0);
    if (r < 0) sagui_levantar(r, NULL);
    sagui_rodar_tratadores();   /* kill(getpid(), sig): el tratador corre ya, como en CPython */
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(os_kill_obj, os_kill);

static mp_obj_t os_killpg(mp_obj_t grupo, mp_obj_t sinal)
{
    int g = mp_obj_get_int(grupo);
    int r = sinal_op(SINOP_ENVIAR, (unsigned)(g == 0 ? 0 : -g), (unsigned)mp_obj_get_int(sinal), 0);
    if (r < 0) sagui_levantar(r, NULL);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(os_killpg_obj, os_killpg);

static mp_obj_t os_getppid(void) { return MP_OBJ_NEW_SMALL_INT(sinal_op(SINOP_PAI, 0, 0, 0)); }
static MP_DEFINE_CONST_FUN_OBJ_0(os_getppid_obj, os_getppid);

static mp_obj_t os_getpgid(mp_obj_t pid)
{
    int r = sinal_op(SINOP_GRUPO, (unsigned)mp_obj_get_int(pid), 0, 0);
    if (r < 0) sagui_levantar(r, NULL);
    return MP_OBJ_NEW_SMALL_INT(r);
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_getpgid_obj, os_getpgid);

static mp_obj_t os_getpgrp(void) { return MP_OBJ_NEW_SMALL_INT(sinal_op(SINOP_GRUPO, 0, 0, 0)); }
static MP_DEFINE_CONST_FUN_OBJ_0(os_getpgrp_obj, os_getpgrp);

/* La identidad de la D255: uid en los 16 bits bajos, gid en los altos. SaguiOS no
 * distingue real de efectiva. */
static mp_obj_t os_getuid(void) { return MP_OBJ_NEW_SMALL_INT(usuario_op(USU_CONSULTAR, 0, 0) & 0xFFFF); }
static MP_DEFINE_CONST_FUN_OBJ_0(os_getuid_obj, os_getuid);
static mp_obj_t os_getgid(void) { return MP_OBJ_NEW_SMALL_INT((usuario_op(USU_CONSULTAR, 0, 0) >> 16) & 0xFFFF); }
static MP_DEFINE_CONST_FUN_OBJ_0(os_getgid_obj, os_getgid);

/* (tiempo de CPU en ms, memoria en KB, hilos) del proceso: lo usa resource.getrusage. */
static mp_obj_t os__infoproc(void)
{
    info_processo_t v[64];
    int n = info_sistema(INFO_PROCESSOS, v, 64), yo = meu_pid();
    for (int i = 0; i < n && i < 64; i++) {
        if (v[i].pid != yo) continue;
        mp_obj_t t[3] = { mp_obj_new_int_from_uint(v[i].cpu_ms), mp_obj_new_int_from_uint(v[i].memoria_kb),
                          MP_OBJ_NEW_SMALL_INT(v[i].n_hilos) };
        return mp_obj_new_tuple(3, t);
    }
    mp_raise_OSError(MP_ESRCH);
}
static MP_DEFINE_CONST_FUN_OBJ_0(os__infoproc_obj, os__infoproc);

/* ------------------------------------------------------------ descriptores */

static mp_obj_t os_pipe(void)
{
    int fds[2];
    int r = criar_pipe(fds);
    if (r < 0) sagui_levantar(r, NULL);
    mp_obj_t t[2] = { MP_OBJ_NEW_SMALL_INT(fds[0]), MP_OBJ_NEW_SMALL_INT(fds[1]) };
    return mp_obj_new_tuple(2, t);
}
static MP_DEFINE_CONST_FUN_OBJ_0(os_pipe_obj, os_pipe);

static mp_obj_t os_dup(mp_obj_t fd)
{
    int r = descritor_op(mp_obj_get_int(fd), DESC_DUPLICAR, 0);
    if (r < 0) sagui_levantar(r, NULL);
    return MP_OBJ_NEW_SMALL_INT(r);
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_dup_obj, os_dup);

static mp_obj_t os_dup2(size_t n_args, const mp_obj_t *args)
{
    (void)n_args;   /* inheritable=True: aceptado, siempre heredable salvo FD_CLOEXEC */
    int a = mp_obj_get_int(args[0]), b = mp_obj_get_int(args[1]);
    if (a == b) {
        if (tipo_descritor(a) < 0) mp_raise_OSError(MP_EBADF);
        return args[1];
    }
    int r = duplicar(a, b);
    if (r < 0) sagui_levantar(r, NULL);
    return MP_OBJ_NEW_SMALL_INT(r);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(os_dup2_obj, 2, 3, os_dup2);

static mp_obj_t os_close(mp_obj_t fd)
{
    int r = fechar(mp_obj_get_int(fd));
    if (r < 0) sagui_levantar(r, NULL);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_close_obj, os_close);

static mp_obj_t os_read(mp_obj_t fd, mp_obj_t n_in)
{
    int f = mp_obj_get_int(fd);
    mp_int_t n = mp_obj_get_int(n_in);
    if (n < 0) mp_raise_ValueError(NULL);
    vstr_t v;
    vstr_init_len(&v, n);
    int r;
    for (;;) {
        r = n ? SEM_GIL(ler(f, v.buf, n)) : 0;
        if (r != ERRO_INTERROMPIDO) break;
        sagui_rodar_tratadores();
    }
    if (r == ERRO_PIPE_QUEBRADO) r = 0;   /* el otro lado cerró: fin de archivo */
    if (r < 0) sagui_levantar(r, NULL);
    v.len = r;
    return mp_obj_new_bytes_from_vstr(&v);
}
static MP_DEFINE_CONST_FUN_OBJ_2(os_read_obj, os_read);

static mp_obj_t os_write(mp_obj_t fd, mp_obj_t dados)
{
    mp_buffer_info_t b;
    mp_get_buffer_raise(dados, &b, MP_BUFFER_READ);
    int r;
    for (;;) {
        r = SEM_GIL(escrever(mp_obj_get_int(fd), b.buf, b.len));
        if (r != ERRO_INTERROMPIDO) break;
        sagui_rodar_tratadores();
    }
    if (r < 0) sagui_levantar(r, NULL);
    return MP_OBJ_NEW_SMALL_INT(r);
}
static MP_DEFINE_CONST_FUN_OBJ_2(os_write_obj, os_write);

/* /dev/null no existe en el SGFS (lo mismo hace sagsh): leer = un pipe sin escritor
 * (fin de archivo al momento); escribir = un archivo temporal ya borrado. */
int sagui_abrir_nulo(bool escrita)
{
    if (!escrita) {
        int fds[2];
        int r = criar_pipe(fds);
        if (r < 0) return r;
        fechar(fds[1]);
        return fds[0];
    }
    static int seq;
    char nome[64];
    snprintf(nome, sizeof nome, "/tmp/.mp-nulo-%d-%d", meu_pid(), seq++);
    int fd = abrir(nome, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd >= 0) apagar(nome);
    return fd;
}

/* open(ruta, banderas, modo=0o777): O_RDONLY/WRONLY/RDWR, O_CREAT, O_EXCL, O_TRUNC,
 * O_APPEND, O_NONBLOCK y O_CLOEXEC. El modo (permisos) se ignora, como en mkdir. */
static mp_obj_t os_open(size_t n_args, const mp_obj_t *args)
{
    (void)n_args;
    char c[CAMINHO_MAX];
    caminho_de(args[0], c);
    int fl = mp_obj_get_int(args[1]), modo = 0;
    switch (fl & 3) {
    case O_RDONLY_: modo = ABRIR_LER; break;
    case O_WRONLY_: modo = ABRIR_ESCREVER; break;
    default: modo = ABRIR_LER | ABRIR_ESCREVER; break;
    }
    if (fl & O_CREAT_) modo |= ABRIR_CRIAR;
    if (fl & O_TRUNC_) modo |= ABRIR_TRUNCAR | ABRIR_ESCREVER;
    if (fl & O_APPEND_) modo |= ABRIR_ACRESCENTAR | ABRIR_ESCREVER;
    int fd;
    if (!strcmp(c, "/dev/null")) {
        fd = sagui_abrir_nulo(modo & ABRIR_ESCREVER);
    } else {
        if ((fl & O_CREAT_) && (fl & O_EXCL_)) {
            info_arquivo_t i;
            if (info_arquivo(c, &i) == 0) sagui_levantar_obj(ERRO_JA_EXISTE, args[0]);
        }
        fd = abrir(c, modo);
    }
    if (fd < 0) sagui_levantar_obj(fd, args[0]);
    if (fl & O_NONBLOCK_) descritor_op(fd, DESC_FIXAR_FL, DESC_FL_NAO_BLOQUEAR);
    if (fl & O_CLOEXEC_) descritor_op(fd, DESC_FIXAR_FD, DESC_FD_FECHAR_EXEC);
    return MP_OBJ_NEW_SMALL_INT(fd);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(os_open_obj, 2, 3, os_open);

static mp_obj_t os_isatty(mp_obj_t fd)
{
    return mp_obj_new_bool(tipo_descritor(mp_obj_get_int(fd)) == TIPO_TERMINAL);
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_isatty_obj, os_isatty);

static mp_obj_t os_set_blocking(mp_obj_t fd, mp_obj_t sim)
{
    int f = mp_obj_get_int(fd);
    int fl = descritor_op(f, DESC_LER_FL, 0);
    if (fl < 0) sagui_levantar(fl, NULL);
    fl = mp_obj_is_true(sim) ? fl & ~DESC_FL_NAO_BLOQUEAR : fl | DESC_FL_NAO_BLOQUEAR;
    int r = descritor_op(f, DESC_FIXAR_FL, (unsigned)fl);
    if (r < 0) sagui_levantar(r, NULL);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(os_set_blocking_obj, os_set_blocking);

static mp_obj_t os_get_blocking(mp_obj_t fd)
{
    int fl = descritor_op(mp_obj_get_int(fd), DESC_LER_FL, 0);
    if (fl < 0) sagui_levantar(fl, NULL);
    return mp_obj_new_bool(!(fl & DESC_FL_NAO_BLOQUEAR));
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_get_blocking_obj, os_get_blocking);

static mp_obj_t os_set_inheritable(mp_obj_t fd, mp_obj_t sim)
{
    int r = descritor_op(mp_obj_get_int(fd), DESC_FIXAR_FD, mp_obj_is_true(sim) ? 0 : DESC_FD_FECHAR_EXEC);
    if (r < 0) sagui_levantar(r, NULL);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(os_set_inheritable_obj, os_set_inheritable);

static mp_obj_t os_get_inheritable(mp_obj_t fd)
{
    int r = descritor_op(mp_obj_get_int(fd), DESC_LER_FD, 0);
    if (r < 0) sagui_levantar(r, NULL);
    return mp_obj_new_bool(!(r & DESC_FD_FECHAR_EXEC));
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_get_inheritable_obj, os_get_inheritable);

static MP_DEFINE_STR_OBJ(str_devnull, "/dev/null");

/* Las entradas del diccionario del módulo os. */
#define OS_PROCESOS_ENTRADAS \
    { MP_ROM_QSTR(MP_QSTR_fork), MP_ROM_PTR(&os_fork_obj) }, \
    { MP_ROM_QSTR(MP_QSTR__exit), MP_ROM_PTR(&os__exit_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_execv), MP_ROM_PTR(&os_execv_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_execve), MP_ROM_PTR(&os_execve_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_execvp), MP_ROM_PTR(&os_execvp_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_execvpe), MP_ROM_PTR(&os_execvpe_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_waitpid), MP_ROM_PTR(&os_waitpid_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_wait), MP_ROM_PTR(&os_wait_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_WIFEXITED), MP_ROM_PTR(&os_WIFEXITED_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_WEXITSTATUS), MP_ROM_PTR(&os_WEXITSTATUS_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_WIFSIGNALED), MP_ROM_PTR(&os_WIFSIGNALED_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_WTERMSIG), MP_ROM_PTR(&os_WTERMSIG_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_WIFSTOPPED), MP_ROM_PTR(&os_WIFSTOPPED_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_WSTOPSIG), MP_ROM_PTR(&os_WSTOPSIG_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_waitstatus_to_exitcode), MP_ROM_PTR(&os_waitstatus_to_exitcode_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_WNOHANG), MP_ROM_INT(ESPERA_SEM_BLOQUEAR) }, \
    { MP_ROM_QSTR(MP_QSTR_kill), MP_ROM_PTR(&os_kill_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_killpg), MP_ROM_PTR(&os_killpg_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_getppid), MP_ROM_PTR(&os_getppid_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_getpgid), MP_ROM_PTR(&os_getpgid_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_getpgrp), MP_ROM_PTR(&os_getpgrp_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_getuid), MP_ROM_PTR(&os_getuid_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_geteuid), MP_ROM_PTR(&os_getuid_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_getgid), MP_ROM_PTR(&os_getgid_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_getegid), MP_ROM_PTR(&os_getgid_obj) }, \
    { MP_ROM_QSTR(MP_QSTR__infoproc), MP_ROM_PTR(&os__infoproc_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_pipe), MP_ROM_PTR(&os_pipe_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_dup), MP_ROM_PTR(&os_dup_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_dup2), MP_ROM_PTR(&os_dup2_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&os_close_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&os_read_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&os_write_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&os_open_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_isatty), MP_ROM_PTR(&os_isatty_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_set_blocking), MP_ROM_PTR(&os_set_blocking_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_get_blocking), MP_ROM_PTR(&os_get_blocking_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_set_inheritable), MP_ROM_PTR(&os_set_inheritable_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_get_inheritable), MP_ROM_PTR(&os_get_inheritable_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_devnull), MP_ROM_PTR(&str_devnull) }, \
    { MP_ROM_QSTR(MP_QSTR_O_RDONLY), MP_ROM_INT(O_RDONLY_) }, \
    { MP_ROM_QSTR(MP_QSTR_O_WRONLY), MP_ROM_INT(O_WRONLY_) }, \
    { MP_ROM_QSTR(MP_QSTR_O_RDWR), MP_ROM_INT(O_RDWR_) }, \
    { MP_ROM_QSTR(MP_QSTR_O_CREAT), MP_ROM_INT(O_CREAT_) }, \
    { MP_ROM_QSTR(MP_QSTR_O_EXCL), MP_ROM_INT(O_EXCL_) }, \
    { MP_ROM_QSTR(MP_QSTR_O_TRUNC), MP_ROM_INT(O_TRUNC_) }, \
    { MP_ROM_QSTR(MP_QSTR_O_APPEND), MP_ROM_INT(O_APPEND_) }, \
    { MP_ROM_QSTR(MP_QSTR_O_NONBLOCK), MP_ROM_INT(O_NONBLOCK_) }, \
    { MP_ROM_QSTR(MP_QSTR_O_CLOEXEC), MP_ROM_INT(O_CLOEXEC_) },
