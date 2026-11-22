/* sinais_sagui.c - el módulo signal sobre las señales del kernel (D401, D419).
 *
 * Interfaz de CPython: signal, getsignal, alarm, setitimer (un disparo), getitimer,
 * pause, raise_signal, pthread_sigmask, sigpending, valid_signals, strsignal,
 * default_int_handler y las constantes SIGxxx, SIG_DFL, SIG_IGN, SIG_BLOCK...
 *
 * Cómo se llama a un tratador de Python de forma segura. El kernel interrumpe el
 * programa en cualquier instrucción: ahí no se puede tocar el montón ni el
 * intérprete. Por eso el tratador del kernel es una función de C mínima
 * (manipulador) que solo cuenta la señal y levanta una bandera
 * (sagui_sinais_pend). Quien la mira, en un punto seguro, llama a los
 * tratadores de Python con sagui_rodar_tratadores():
 *   - el bucle del intérprete, en cada salto hacia atrás (MICROPY_VM_HOOK_LOOP,
 *     mpconfigport.h: una lectura y una comparación);
 *   - las esperas: time.sleep, select, waitpid, os.read, socket.recv/accept
 *     (cuando una señal las interrumpe, el tratador corre y la espera sigue,
 *     como pide PEP 475). Si el tratador levanta una excepción, sale de la
 *     espera (así funciona el clásico alarm + excepción de plazo).
 * Los tratadores solo corren en el hilo principal (como en CPython): los demás
 * hilos bloquean todas las señales al nacer (sagui_hilo_sin_sinais).
 *
 * Diferencias con CPython: Ctrl+C sigue terminando el programa (D037) mientras
 * nadie instale un tratador de SIGINT (signal.default_int_handler da el
 * KeyboardInterrupt de siempre si se instala); el segundo argumento del
 * tratador (el frame) es None; no hay set_wakeup_fd, siginterrupt ni
 * temporizadores periódicos (setitimer con intervalo da OSError). */
#include <string.h>
#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/objlist.h"
#include "sagui_mp.h"
#include "mpthreadport.h"

#define NSIG_ 32

volatile int sagui_sinais_pend;
static volatile unsigned contagem[NSIG_];   /* las veces que llegó cada señal (solo las escribe el manipulador) */
static unsigned visto[NSIG_];               /* las que ya se entregaron a Python (solo el hilo principal) */
static unsigned alarme_fim;                 /* tempo_ms() en que dispara la alarma (0 = ninguna), para getitimer */

/* Los tratadores de Python: una lista de 32 (None = ninguno; el valor lo guarda el módulo). */
MP_REGISTER_ROOT_POINTER(mp_obj_t sagui_tratadores);

static mp_obj_t lista_tratadores(void)
{
    if (MP_STATE_VM(sagui_tratadores) == MP_OBJ_NULL)
        MP_STATE_VM(sagui_tratadores) = mp_obj_new_list(NSIG_, NULL);
    mp_obj_list_t *l = MP_OBJ_TO_PTR(MP_STATE_VM(sagui_tratadores));
    for (int i = 0; i < NSIG_; i++)
        if (l->items[i] == MP_OBJ_NULL) l->items[i] = mp_const_none;
    return MP_STATE_VM(sagui_tratadores);
}

/* La vuelta de un tratador: al hacer «ret», esp apunta a [señal][&info][&contexto]
 * (el mismo restaurador de la libc común, D401). */
void __sagui_sinal_restaurar(void);
__asm__(".text\n"
        ".globl __sagui_sinal_restaurar\n"
        ".type __sagui_sinal_restaurar, @function\n"
        "__sagui_sinal_restaurar:\n"
        "    movl 8(%esp), %ebx\n"
        "    movl $144, %eax\n"          /* SIS_SINAL_VOLTAR */
        "    int $0x80\n"
        "    hlt\n");

/* El tratador del kernel: nada de Python aquí. */
static void manipulador(int sinal)
{
    if (sinal > 0 && sinal < NSIG_) {
        contagem[sinal]++;
        sagui_sinais_pend = 1;
    }
}

static bool no_principal(void)
{
    uint32_t esp;
    __asm__("movl %%esp, %0" : "=r"(esp));
    return esp >= SAGUI_PILHA_PRINCIPAL_BASE;
}

void sagui_hilo_sin_sinais(void)
{
    unsigned todas = 0xFFFFFFFEu;
    sinal_op(SINOP_MASCARA, SINOP_FIXAR, (unsigned)&todas, 0);
}

void sagui_rodar_tratadores(void)
{
    if (!sagui_sinais_pend || !no_principal()) return;
    sagui_sinais_pend = 0;   /* antes de mirar los contadores: una señal nueva lo vuelve a levantar */
    mp_obj_t lista = lista_tratadores();
    mp_obj_list_t *l = MP_OBJ_TO_PTR(lista);
    for (int s = 1; s < NSIG_; s++) {
        while (visto[s] != contagem[s]) {
            visto[s]++;
            mp_obj_t h = l->items[s];
            if (h == mp_const_none || !mp_obj_is_callable(h)) continue;
            nlr_buf_t nlr;
            if (nlr_push(&nlr) == 0) {
                mp_call_function_2(h, MP_OBJ_NEW_SMALL_INT(s), mp_const_none);
                nlr_pop();
            } else {
                sagui_sinais_pend = 1;   /* las que queden se entregarán después */
                nlr_jump(nlr.ret_val);
            }
        }
    }
}

/* ------------------------------------------------------------ signal() */

static int numero_sinal(mp_obj_t o)
{
    int s = mp_obj_get_int(o);
    if (s < 1 || s >= NSIG_) mp_raise_ValueError(MP_ERROR_TEXT("signal number out of range"));
    return s;
}

static mp_obj_t signal_signal(mp_obj_t sinal, mp_obj_t tratador)
{
    int s = numero_sinal(sinal);
    if (!no_principal()) mp_raise_ValueError(MP_ERROR_TEXT("signal only works in main thread"));
    if (s == SINAL_KILL || s == SINAL_STOP) mp_raise_OSError(MP_EINVAL);
    mp_obj_list_t *l = MP_OBJ_TO_PTR(lista_tratadores());
    acao_sinal_t nova, velha;
    memset(&nova, 0, sizeof nova);
    if (mp_obj_is_small_int(tratador) && (MP_OBJ_SMALL_INT_VALUE(tratador) == 0 || MP_OBJ_SMALL_INT_VALUE(tratador) == 1)) {
        nova.tratador = (unsigned)MP_OBJ_SMALL_INT_VALUE(tratador);
    } else if (mp_obj_is_callable(tratador)) {
        nova.tratador = (unsigned)manipulador;
        nova.mascara = 0xFFFFFFFEu;   /* mientras corre el manipulador, ninguna otra */
        nova.flags = SINAL_SA_RESTORER;   /* sin SA_RESTART: las esperas vuelven con EINTR y Python las repite tras el tratador */
        nova.restaurador = (unsigned)__sagui_sinal_restaurar;
    } else {
        mp_raise_TypeError(MP_ERROR_TEXT("signal handler must be signal.SIG_IGN, signal.SIG_DFL, or a callable object"));
    }
    int r = sinal_op(SINOP_ACAO, (unsigned)s, (unsigned)&nova, (unsigned)&velha);
    if (r < 0) sagui_levantar(r, NULL);
    mp_obj_t antes = velha.tratador == (unsigned)manipulador ? l->items[s] : MP_OBJ_NEW_SMALL_INT(velha.tratador);
    l->items[s] = nova.tratador == (unsigned)manipulador ? tratador : mp_const_none;
    return antes;
}
static MP_DEFINE_CONST_FUN_OBJ_2(signal_signal_obj, signal_signal);

static mp_obj_t signal_getsignal(mp_obj_t sinal)
{
    int s = numero_sinal(sinal);
    acao_sinal_t velha;
    int r = sinal_op(SINOP_ACAO, (unsigned)s, 0, (unsigned)&velha);
    if (r < 0) sagui_levantar(r, NULL);
    if (velha.tratador == (unsigned)manipulador) return ((mp_obj_list_t *)MP_OBJ_TO_PTR(lista_tratadores()))->items[s];
    return MP_OBJ_NEW_SMALL_INT(velha.tratador);
}
static MP_DEFINE_CONST_FUN_OBJ_1(signal_getsignal_obj, signal_getsignal);

static mp_obj_t signal_default_int_handler(mp_obj_t a, mp_obj_t b)
{
    (void)a; (void)b;
    nlr_raise(mp_obj_new_exception(&mp_type_KeyboardInterrupt));
}
static MP_DEFINE_CONST_FUN_OBJ_2(signal_default_int_handler_obj, signal_default_int_handler);

/* ------------------------------------------------------------ alarmas */

static unsigned programar_alarma(unsigned ms)
{
    int r = sinal_op(SINOP_ALARME, ms, 0, 0);
    alarme_fim = ms ? (tempo_ms() + ms) | 1 : 0;
    return r <= 0 ? 0 : (unsigned)r;
}

static mp_obj_t signal_alarm(mp_obj_t segundos)
{
    mp_int_t s = mp_obj_get_int(segundos);
    if (s < 0) mp_raise_ValueError(MP_ERROR_TEXT("alarm time must be >= 0"));
    unsigned ms = s > 0x7FFFFFFF / 1000 ? 0x7FFFFFFFu : (unsigned)s * 1000u;
    unsigned resto = programar_alarma(ms);
    return MP_OBJ_NEW_SMALL_INT((resto + 999) / 1000);
}
static MP_DEFINE_CONST_FUN_OBJ_1(signal_alarm_obj, signal_alarm);

static mp_obj_t duas_floats(mp_float_t a, mp_float_t b)
{
    mp_obj_t t[2] = { mp_obj_new_float(a), mp_obj_new_float(b) };
    return mp_obj_new_tuple(2, t);
}

static mp_float_t resta_alarma(void)
{
    if (!alarme_fim) return 0;
    int d = (int)(alarme_fim - tempo_ms());
    return d > 0 ? (mp_float_t)d / 1000 : 0;
}

/* setitimer(ITIMER_REAL, segundos, intervalo=0): solo un disparo (el kernel no repite). */
static mp_obj_t signal_setitimer(size_t n_args, const mp_obj_t *args)
{
    if (mp_obj_get_int(args[0]) != 0) mp_raise_OSError(MP_EINVAL);   /* solo ITIMER_REAL */
    mp_float_t seg = mp_obj_get_float(args[1]);
    mp_float_t inter = n_args > 2 ? mp_obj_get_float(args[2]) : 0;
    if (seg < 0 || inter < 0) mp_raise_OSError(MP_EINVAL);
    if (inter != 0) mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("interval timers are not supported (one shot only)"));
    mp_float_t antes = resta_alarma();
    unsigned ms = seg > 0x7FFFFF ? 0x7FFFFFFFu : (unsigned)(seg * 1000);
    if (seg > 0 && ms == 0) ms = 1;
    programar_alarma(ms);
    return duas_floats(antes, 0);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(signal_setitimer_obj, 2, 3, signal_setitimer);

static mp_obj_t signal_getitimer(mp_obj_t which)
{
    if (mp_obj_get_int(which) != 0) mp_raise_OSError(MP_EINVAL);
    return duas_floats(resta_alarma(), 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(signal_getitimer_obj, signal_getitimer);

/* ------------------------------------------------------------ esperas y envío */

/* pause(): duerme hasta que llegue una señal (y su tratador corre). */
static mp_obj_t signal_pause(void)
{
    unsigned m = 0;
    sinal_op(SINOP_MASCARA, SINOP_BLOQUEAR, 0, (unsigned)&m);   /* la máscara actual */
    sagui_rodar_tratadores();
    if (!sagui_sinais_pend) SEM_GIL(sinal_op(SINOP_SUSPENDER, (unsigned)&m, 0, 0));
    sagui_rodar_tratadores();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(signal_pause_obj, signal_pause);

static mp_obj_t signal_raise_signal(mp_obj_t sinal)
{
    int s = numero_sinal(sinal);
    int r = sinal_op(SINOP_HILO, 0, (unsigned)s, 0);
    if (r < 0) sagui_levantar(r, NULL);
    sagui_rodar_tratadores();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(signal_raise_signal_obj, signal_raise_signal);

static unsigned mascara_de(mp_obj_t iteravel)
{
    unsigned m = 0;
    mp_obj_t it = mp_getiter(iteravel, NULL), v;
    while ((v = mp_iternext(it)) != MP_OBJ_STOP_ITERATION) m |= 1u << numero_sinal(v);
    return m;
}

static mp_obj_t conjunto_de(unsigned m)
{
    mp_obj_t c = mp_obj_new_set(0, NULL);
    for (int s = 1; s < NSIG_; s++)
        if (m & (1u << s)) mp_obj_set_store(c, MP_OBJ_NEW_SMALL_INT(s));
    return c;
}

/* pthread_sigmask(cómo, conjunto) -> el conjunto anterior. */
static mp_obj_t signal_pthread_sigmask(mp_obj_t como, mp_obj_t conj)
{
    int c = mp_obj_get_int(como);
    if (c < 0 || c > 2) mp_raise_ValueError(MP_ERROR_TEXT("invalid how"));
    unsigned nova = mascara_de(conj), velha = 0;
    int r = sinal_op(SINOP_MASCARA, (unsigned)c, (unsigned)&nova, (unsigned)&velha);
    if (r < 0) sagui_levantar(r, NULL);
    return conjunto_de(velha);
}
static MP_DEFINE_CONST_FUN_OBJ_2(signal_pthread_sigmask_obj, signal_pthread_sigmask);

static mp_obj_t signal_sigpending(void)
{
    unsigned m = 0;
    int r = sinal_op(SINOP_PENDENTES, (unsigned)&m, 0, 0);
    if (r < 0) sagui_levantar(r, NULL);
    return conjunto_de(m);
}
static MP_DEFINE_CONST_FUN_OBJ_0(signal_sigpending_obj, signal_sigpending);

static mp_obj_t signal_valid_signals(void)
{
    return conjunto_de(0xFFFFFFFEu);
}
static MP_DEFINE_CONST_FUN_OBJ_0(signal_valid_signals_obj, signal_valid_signals);

static mp_obj_t signal_strsignal(mp_obj_t sinal)
{
    static const char *const nomes[NSIG_] = {
        NULL, "Hangup", "Interrupt", "Quit", "Illegal instruction", "Trace/breakpoint trap", "Aborted",
        "Bus error", "Floating point exception", "Killed", "User defined signal 1", "Segmentation fault",
        "User defined signal 2", "Broken pipe", "Alarm clock", "Terminated", NULL, "Child exited",
        "Continued", "Stopped (signal)", "Stopped", "Stopped (tty input)", "Stopped (tty output)",
        "Urgent I/O condition", "CPU time limit exceeded", "File size limit exceeded", NULL, NULL,
        "Window changed", NULL, NULL, "Bad system call",
    };
    int s = mp_obj_get_int(sinal);
    if (s < 1 || s >= NSIG_) mp_raise_ValueError(MP_ERROR_TEXT("signal number out of range"));
    if (!nomes[s]) return mp_const_none;
    return mp_obj_new_str(nomes[s], strlen(nomes[s]));
}
static MP_DEFINE_CONST_FUN_OBJ_1(signal_strsignal_obj, signal_strsignal);

#define SIG(n, v) { MP_ROM_QSTR(MP_QSTR_##n), MP_ROM_INT(v) }
static const mp_rom_map_elem_t signal_globais_tab[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_signal) },
    { MP_ROM_QSTR(MP_QSTR_signal), MP_ROM_PTR(&signal_signal_obj) },
    { MP_ROM_QSTR(MP_QSTR_getsignal), MP_ROM_PTR(&signal_getsignal_obj) },
    { MP_ROM_QSTR(MP_QSTR_default_int_handler), MP_ROM_PTR(&signal_default_int_handler_obj) },
    { MP_ROM_QSTR(MP_QSTR_alarm), MP_ROM_PTR(&signal_alarm_obj) },
    { MP_ROM_QSTR(MP_QSTR_setitimer), MP_ROM_PTR(&signal_setitimer_obj) },
    { MP_ROM_QSTR(MP_QSTR_getitimer), MP_ROM_PTR(&signal_getitimer_obj) },
    { MP_ROM_QSTR(MP_QSTR_pause), MP_ROM_PTR(&signal_pause_obj) },
    { MP_ROM_QSTR(MP_QSTR_raise_signal), MP_ROM_PTR(&signal_raise_signal_obj) },
    { MP_ROM_QSTR(MP_QSTR_pthread_sigmask), MP_ROM_PTR(&signal_pthread_sigmask_obj) },
    { MP_ROM_QSTR(MP_QSTR_sigpending), MP_ROM_PTR(&signal_sigpending_obj) },
    { MP_ROM_QSTR(MP_QSTR_valid_signals), MP_ROM_PTR(&signal_valid_signals_obj) },
    { MP_ROM_QSTR(MP_QSTR_strsignal), MP_ROM_PTR(&signal_strsignal_obj) },
    SIG(SIG_DFL, 0), SIG(SIG_IGN, 1), SIG(SIG_BLOCK, 0), SIG(SIG_UNBLOCK, 1), SIG(SIG_SETMASK, 2),
    SIG(NSIG, NSIG_), SIG(ITIMER_REAL, 0),
    SIG(SIGHUP, 1), SIG(SIGINT, 2), SIG(SIGQUIT, 3), SIG(SIGILL, 4), SIG(SIGTRAP, 5), SIG(SIGABRT, 6),
    SIG(SIGBUS, 7), SIG(SIGFPE, 8), SIG(SIGKILL, 9), SIG(SIGUSR1, 10), SIG(SIGSEGV, 11), SIG(SIGUSR2, 12),
    SIG(SIGPIPE, 13), SIG(SIGALRM, 14), SIG(SIGTERM, 15), SIG(SIGCHLD, 17), SIG(SIGCONT, 18),
    SIG(SIGSTOP, 19), SIG(SIGTSTP, 20), SIG(SIGTTIN, 21), SIG(SIGTTOU, 22), SIG(SIGURG, 23),
    SIG(SIGXCPU, 24), SIG(SIGXFSZ, 25), SIG(SIGWINCH, 28), SIG(SIGSYS, 31),
};
static MP_DEFINE_CONST_DICT(signal_globais, signal_globais_tab);

const mp_obj_module_t modulo_signal = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&signal_globais,
};
MP_REGISTER_MODULE(MP_QSTR_signal, modulo_signal);
