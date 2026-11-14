/* tempo_sagui.h - la parte de SaguiOS en el módulo time.
 *
 * El módulo time es el de MicroPython (extmod/modtime.c: sleep, sleep_ms,
 * ticks_*, localtime, gmtime, mktime); este archivo lo incluye él
 * (MICROPY_PY_TIME_INCLUDEFILE) y da el reloj del sistema y lo que falta para
 * programas escritos para CPython: time() con fracción de segundo,
 * monotonic(), perf_counter() y strftime().
 *
 * La hora viene del kernel en UTC, con milisegundos (SIS_HORA, D080), y la
 * hora local es la de la zona del proceso (la variable TZ o la del sistema): las
 * dos cuentas quedan en libsagui, al otro lado de la ponte (ponte_sagui.h),
 * porque este archivo no puede incluir sagui.h. Por eso gmtime,
 * localtime y mktime son estos, y no los de extmod (que no saben de zona:
 * MICROPY_PY_TIME_GMTIME_LOCALTIME_MKTIME desactivado). */
#include <stdio.h>
#include <string.h>
#include "py/obj.h"
#include "py/runtime.h"
#include "sagui_mp.h"
#include "ponte_sagui.h"

static mp_obj_t mp_time_time_get(void)
{
    unsigned ms;
    unsigned s = ponte_hora_utc(&ms);
    return mp_obj_new_float((mp_float_t)s + (mp_float_t)ms / 1000);
}

uint64_t mp_hal_time_ns(void)
{
    unsigned ms;
    unsigned s = ponte_hora_utc(&ms);
    return (uint64_t)s * 1000000000ULL + (uint64_t)ms * 1000000ULL;
}

/* La tupla de 8 de MicroPython (año, mes, día, hora, minuto, segundo, día
 * de la semana, día del año): sin el isdst de CPython, como el time de
 * MicroPython en todas partes, para los programas escritos para él. */
static mp_obj_t tempo_tupla(const int c[9])
{
    mp_obj_t t[8];
    for (int i = 0; i < 8; i++) t[i] = mp_obj_new_int(c[i]);
    return mp_obj_new_tuple(8, t);
}

static unsigned tempo_segundos(size_t n_args, const mp_obj_t *args)
{
    if (n_args == 0 || args[0] == mp_const_none) return ponte_hora_utc(NULL);
    return (unsigned)mp_obj_get_int(args[0]);
}

static mp_obj_t mp_time_localtime_get(void)
{
    int c[9];
    ponte_hora_local(ponte_hora_utc(NULL), c);
    return tempo_tupla(c);
}

/* localtime([segundos]): hora local de la zona del proceso. */
static mp_obj_t tempo_localtime(size_t n_args, const mp_obj_t *args)
{
    int c[9];
    ponte_hora_local(tempo_segundos(n_args, args), c);
    return tempo_tupla(c);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tempo_localtime_obj, 0, 1, tempo_localtime);

/* gmtime([segundos]): UTC. */
static mp_obj_t tempo_gmtime(size_t n_args, const mp_obj_t *args)
{
    int c[9];
    ponte_hora_utc_campos(tempo_segundos(n_args, args), c);
    c[8] = 0;
    return tempo_tupla(c);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tempo_gmtime_obj, 0, 1, tempo_gmtime);

/* mktime(tupla): la tupla es hora local; devuelve segundos UTC. En la hora
 * repetida queda la primera (D080). */
static mp_obj_t tempo_mktime(mp_obj_t tupla)
{
    size_t n;
    mp_obj_t *e;
    mp_obj_get_array(tupla, &n, &e);
    if (n < 6) mp_raise_TypeError(MP_ERROR_TEXT("mktime needs a tuple of length 8 or 9"));
    return mp_obj_new_int_from_uint(ponte_local_para_utc(mp_obj_get_int(e[0]), mp_obj_get_int(e[1]),
                                                         mp_obj_get_int(e[2]), mp_obj_get_int(e[3]),
                                                         mp_obj_get_int(e[4]), mp_obj_get_int(e[5])));
}
static MP_DEFINE_CONST_FUN_OBJ_1(tempo_mktime_obj, tempo_mktime);

/* monotonic() e perf_counter(): segundos (float) desde el arranque. */
static mp_obj_t tempo_monotonic(void)
{
    return mp_obj_new_float((mp_float_t)tempo_ms() / 1000);
}
static MP_DEFINE_CONST_FUN_OBJ_0(tempo_monotonic_obj, tempo_monotonic);

/* strftime(formato[, tupla]): los códigos de C en la localidad "C" (nombres en
 * inglés, como en CPython sin setlocale): %Y %m %d %H %M %S %y %j %a %A %b
 * %B %p %I %w %% y las combinaciones %c %x %X %D %T %F. */
static const char *const tempo_dias[] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
static const char *const tempo_meses[] = { "January", "February", "March", "April", "May", "June", "July",
                                           "August", "September", "October", "November", "December" };

static void tempo_formatar(vstr_t *v, const char *f, const mp_int_t *c);

static void tempo_codigo(vstr_t *v, char k, const mp_int_t *c)
{
    /* c: año, mes, día, hora, minuto, segundo, día de la semana (0 = lunes), día del año */
    char b[16];
    const char *s = b;
    switch (k) {
    case 'Y': snprintf(b, sizeof b, "%d", (int)c[0]); break;
    case 'y': snprintf(b, sizeof b, "%02d", (int)(c[0] % 100)); break;
    case 'm': snprintf(b, sizeof b, "%02d", (int)c[1]); break;
    case 'd': snprintf(b, sizeof b, "%02d", (int)c[2]); break;
    case 'H': snprintf(b, sizeof b, "%02d", (int)c[3]); break;
    case 'I': snprintf(b, sizeof b, "%02d", (int)(c[3] % 12 ? c[3] % 12 : 12)); break;
    case 'M': snprintf(b, sizeof b, "%02d", (int)c[4]); break;
    case 'S': snprintf(b, sizeof b, "%02d", (int)c[5]); break;
    case 'j': snprintf(b, sizeof b, "%03d", (int)c[7]); break;
    case 'w': snprintf(b, sizeof b, "%d", (int)((c[6] + 1) % 7)); break;
    case 'p': s = c[3] < 12 ? "AM" : "PM"; break;
    case 'A': s = tempo_dias[c[6] % 7]; break;
    case 'a': memcpy(b, tempo_dias[c[6] % 7], 3); b[3] = 0; break;
    case 'B': s = tempo_meses[(c[1] + 11) % 12]; break;
    case 'b': case 'h': memcpy(b, tempo_meses[(c[1] + 11) % 12], 3); b[3] = 0; break;
    case 'c': tempo_formatar(v, "%a %b %d %H:%M:%S %Y", c); return;
    case 'x': case 'D': tempo_formatar(v, "%m/%d/%y", c); return;
    case 'X': case 'T': tempo_formatar(v, "%H:%M:%S", c); return;
    case 'F': tempo_formatar(v, "%Y-%m-%d", c); return;
    case '%': s = "%"; break;
    default: b[0] = '%'; b[1] = k; b[2] = 0; break;
    }
    vstr_add_str(v, s);
}

static void tempo_formatar(vstr_t *v, const char *f, const mp_int_t *c)
{
    for (; *f; f++) {
        if (*f == '%' && f[1]) tempo_codigo(v, *++f, c);
        else vstr_add_byte(v, *f);
    }
}

static mp_obj_t tempo_strftime(size_t n_args, const mp_obj_t *args)
{
    mp_obj_t t = n_args > 1 ? args[1] : mp_time_localtime_get();
    size_t n;
    mp_obj_t *e;
    mp_obj_get_array(t, &n, &e);
    if (n < 8) mp_raise_TypeError(MP_ERROR_TEXT("strftime needs a tuple of length 8 or 9"));
    mp_int_t c[8];
    for (int i = 0; i < 8; i++) c[i] = mp_obj_get_int(e[i]);
    vstr_t v;
    vstr_init(&v, 32);
    tempo_formatar(&v, mp_obj_str_get_str(args[0]), c);
    return mp_obj_new_str_from_vstr(&v);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tempo_strftime_obj, 1, 2, tempo_strftime);

#define MICROPY_PY_TIME_EXTRA_GLOBALS \
    { MP_ROM_QSTR(MP_QSTR_gmtime), MP_ROM_PTR(&tempo_gmtime_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_localtime), MP_ROM_PTR(&tempo_localtime_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_mktime), MP_ROM_PTR(&tempo_mktime_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_monotonic), MP_ROM_PTR(&tempo_monotonic_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_perf_counter), MP_ROM_PTR(&tempo_monotonic_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_strftime), MP_ROM_PTR(&tempo_strftime_obj) },
