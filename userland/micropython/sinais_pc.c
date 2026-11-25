// SPDX-License-Identifier: GPL-2.0-or-later
/* sinais_pc.c - el módulo signal de Python en el PC (VisualPythonS portable, D459), sobre las señales POSIX.
 *
 * Es la versión para Linux de sinais_sagui.c (D401, D419), con la misma interfaz y el mismo modo de llamar a los
 * tratadores: el de C (manipulador) solo cuenta la señal y levanta sagui_sinais_pend; los de Python corren en un
 * punto seguro (sagui_rodar_tratadores, desde el bucle del intérprete y las esperas) y solo en el hilo principal.
 * Los números de las señales de SaguiOS son los de Linux, así que las constantes coinciden. Sin restaurador propio:
 * lo pone la libc del PC. Diferencias con sinais_sagui.c: setitimer acepta intervalo (lo hace el PC). */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/objlist.h"
#include "sagui_mp.h"

#define NSIG_ 32

volatile int sagui_sinais_pend;
static volatile sig_atomic_t contagem[NSIG_];
static unsigned visto[NSIG_];
static pthread_t hilo_principal;

__attribute__((constructor)) static void anotar_principal(void) { hilo_principal = pthread_self(); }

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

static void manipulador(int sinal)
{
    if (sinal > 0 && sinal < NSIG_) {
        contagem[sinal]++;
        sagui_sinais_pend = 1;
    }
}

static bool no_principal(void) { return pthread_equal(pthread_self(), hilo_principal); }

void sagui_hilo_sin_sinais(void)
{
    sigset_t todas;
    sigfillset(&todas);
    pthread_sigmask(SIG_BLOCK, &todas, NULL);
}

void sagui_rodar_tratadores(void)
{
    if (!sagui_sinais_pend || !no_principal()) return;
    sagui_sinais_pend = 0;
    mp_obj_list_t *l = MP_OBJ_TO_PTR(lista_tratadores());
    for (int s = 1; s < NSIG_; s++) {
        while (visto[s] != (unsigned)contagem[s]) {
            visto[s]++;
            mp_obj_t h = l->items[s];
            if (h == mp_const_none || !mp_obj_is_callable(h)) continue;
            nlr_buf_t nlr;
            if (nlr_push(&nlr) == 0) {
                mp_call_function_2(h, MP_OBJ_NEW_SMALL_INT(s), mp_const_none);
                nlr_pop();
            } else {
                sagui_sinais_pend = 1;
                nlr_jump(nlr.ret_val);
            }
        }
    }
}

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
    if (s == SIGKILL || s == SIGSTOP) mp_raise_OSError(MP_EINVAL);
    mp_obj_list_t *l = MP_OBJ_TO_PTR(lista_tratadores());
    struct sigaction nova, velha;
    memset(&nova, 0, sizeof nova);
    if (mp_obj_is_small_int(tratador) && (MP_OBJ_SMALL_INT_VALUE(tratador) == 0 || MP_OBJ_SMALL_INT_VALUE(tratador) == 1)) {
        nova.sa_handler = MP_OBJ_SMALL_INT_VALUE(tratador) ? SIG_IGN : SIG_DFL;
    } else if (mp_obj_is_callable(tratador)) {
        nova.sa_handler = manipulador;
        sigfillset(&nova.sa_mask);   /* sin SA_RESTART: las esperas vuelven con EINTR, como en SaguiOS */
    } else {
        mp_raise_TypeError(MP_ERROR_TEXT("signal handler must be signal.SIG_IGN, signal.SIG_DFL, or a callable object"));
    }
    if (sigaction(s, &nova, &velha) < 0) mp_raise_OSError(errno);
    mp_obj_t antes = velha.sa_handler == manipulador ? l->items[s] : MP_OBJ_NEW_SMALL_INT(velha.sa_handler == SIG_IGN ? 1 : 0);
    l->items[s] = nova.sa_handler == manipulador ? tratador : mp_const_none;
    return antes;
}
static MP_DEFINE_CONST_FUN_OBJ_2(signal_signal_obj, signal_signal);

static mp_obj_t signal_getsignal(mp_obj_t sinal)
{
    int s = numero_sinal(sinal);
    struct sigaction velha;
    if (sigaction(s, NULL, &velha) < 0) mp_raise_OSError(errno);
    if (velha.sa_handler == manipulador) return ((mp_obj_list_t *)MP_OBJ_TO_PTR(lista_tratadores()))->items[s];
    return MP_OBJ_NEW_SMALL_INT(velha.sa_handler == SIG_IGN ? 1 : 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(signal_getsignal_obj, signal_getsignal);

static mp_obj_t signal_default_int_handler(mp_obj_t a, mp_obj_t b)
{
    (void)a; (void)b;
    nlr_raise(mp_obj_new_exception(&mp_type_KeyboardInterrupt));
}
static MP_DEFINE_CONST_FUN_OBJ_2(signal_default_int_handler_obj, signal_default_int_handler);

static mp_obj_t signal_alarm(mp_obj_t segundos)
{
    mp_int_t s = mp_obj_get_int(segundos);
    if (s < 0) mp_raise_ValueError(MP_ERROR_TEXT("alarm time must be >= 0"));
    return MP_OBJ_NEW_SMALL_INT(alarm((unsigned)s));
}
static MP_DEFINE_CONST_FUN_OBJ_1(signal_alarm_obj, signal_alarm);

static mp_float_t segundos_de(struct timeval t) { return (mp_float_t)t.tv_sec + (mp_float_t)t.tv_usec / 1000000; }
static struct timeval timeval_de(mp_float_t s)
{
    struct timeval t;
    t.tv_sec = (time_t)s;
    t.tv_usec = (suseconds_t)((s - (mp_float_t)t.tv_sec) * 1000000);
    if (s > 0 && t.tv_sec == 0 && t.tv_usec == 0) t.tv_usec = 1;
    return t;
}
static mp_obj_t par_itimer(const struct itimerval *v)
{
    mp_obj_t t[2] = { mp_obj_new_float(segundos_de(v->it_value)), mp_obj_new_float(segundos_de(v->it_interval)) };
    return mp_obj_new_tuple(2, t);
}

static mp_obj_t signal_setitimer(size_t n_args, const mp_obj_t *args)
{
    if (mp_obj_get_int(args[0]) != 0) mp_raise_OSError(MP_EINVAL);   /* solo ITIMER_REAL, como en SaguiOS */
    mp_float_t seg = mp_obj_get_float(args[1]);
    mp_float_t inter = n_args > 2 ? mp_obj_get_float(args[2]) : 0;
    if (seg < 0 || inter < 0) mp_raise_OSError(MP_EINVAL);
    struct itimerval nova, velha;
    nova.it_value = timeval_de(seg);
    nova.it_interval = timeval_de(inter);
    if (setitimer(ITIMER_REAL, &nova, &velha) < 0) mp_raise_OSError(errno);
    return par_itimer(&velha);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(signal_setitimer_obj, 2, 3, signal_setitimer);

static mp_obj_t signal_getitimer(mp_obj_t which)
{
    if (mp_obj_get_int(which) != 0) mp_raise_OSError(MP_EINVAL);
    struct itimerval v;
    if (getitimer(ITIMER_REAL, &v) < 0) mp_raise_OSError(errno);
    return par_itimer(&v);
}
static MP_DEFINE_CONST_FUN_OBJ_1(signal_getitimer_obj, signal_getitimer);

static mp_obj_t signal_pause(void)
{
    sigset_t m;
    pthread_sigmask(SIG_BLOCK, NULL, &m);
    sagui_rodar_tratadores();
    if (!sagui_sinais_pend) SEM_GIL(sigsuspend(&m));
    sagui_rodar_tratadores();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(signal_pause_obj, signal_pause);

static mp_obj_t signal_raise_signal(mp_obj_t sinal)
{
    int s = numero_sinal(sinal);
    if (raise(s) != 0) mp_raise_OSError(errno);
    sagui_rodar_tratadores();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(signal_raise_signal_obj, signal_raise_signal);

static void conjunto_posix(mp_obj_t iteravel, sigset_t *c)
{
    sigemptyset(c);
    mp_obj_t it = mp_getiter(iteravel, NULL), v;
    while ((v = mp_iternext(it)) != MP_OBJ_STOP_ITERATION) sigaddset(c, numero_sinal(v));
}

static mp_obj_t conjunto_de(const sigset_t *m)
{
    mp_obj_t c = mp_obj_new_set(0, NULL);
    for (int s = 1; s < NSIG_; s++)
        if (sigismember(m, s) == 1) mp_obj_set_store(c, MP_OBJ_NEW_SMALL_INT(s));
    return c;
}

static mp_obj_t signal_pthread_sigmask(mp_obj_t como, mp_obj_t conj)
{
    static const int modos[3] = { SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK };
    int c = mp_obj_get_int(como);
    if (c < 0 || c > 2) mp_raise_ValueError(MP_ERROR_TEXT("invalid how"));
    sigset_t nova, velha;
    conjunto_posix(conj, &nova);
    int r = pthread_sigmask(modos[c], &nova, &velha);
    if (r) mp_raise_OSError(r);
    return conjunto_de(&velha);
}
static MP_DEFINE_CONST_FUN_OBJ_2(signal_pthread_sigmask_obj, signal_pthread_sigmask);

static mp_obj_t signal_sigpending(void)
{
    sigset_t m;
    if (sigpending(&m) < 0) mp_raise_OSError(errno);
    return conjunto_de(&m);
}
static MP_DEFINE_CONST_FUN_OBJ_0(signal_sigpending_obj, signal_sigpending);

static mp_obj_t signal_valid_signals(void)
{
    sigset_t m;
    sigfillset(&m);
    return conjunto_de(&m);
}
static MP_DEFINE_CONST_FUN_OBJ_0(signal_valid_signals_obj, signal_valid_signals);

static mp_obj_t signal_strsignal(mp_obj_t sinal)
{
    int s = mp_obj_get_int(sinal);
    if (s < 1 || s >= NSIG_) mp_raise_ValueError(MP_ERROR_TEXT("signal number out of range"));
    const char *n = strsignal(s);
    if (!n) return mp_const_none;
    return mp_obj_new_str(n, strlen(n));
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
