// SPDX-License-Identifier: GPL-3.0-only
/* depuracion_sagui.c - sin depurador JDWP ni JIT en SaguiOS (D376): las funciones que el intérprete
 * de miniJVM llama a esos módulos, vacías. El JIT de miniJVM no existe en 32 bits. */
#include "jvm.h"
#include "jvm_util.h"
#include "jit.h"
#include "jdwp.h"

void jit_init(CodeAttribute *ca) { (void)ca; }
void jit_destroy(Jit *jit) { (void)jit; }
s32 jit_invoke_from_jit(MethodInfo *m, Runtime *r) { (void)m; (void)r; return 0; }
s32 jit_osr_execute(Runtime *r, s32 pos) { (void)r; (void)pos; return 0; }
void jit_set_exception_jump_addr(Runtime *r, CodeAttribute *ca, s32 i) { (void)r; (void)ca; (void)i; }

s32 jdwp_start_server(MiniJVM *jvm) { (void)jvm; return 0; }
s32 jdwp_stop_server(MiniJVM *jvm) { (void)jvm; return 0; }
Runtime *jdwp_get_runtime(JdwpServer *s) { (void)s; return NULL; }
s32 jdwp_is_ignore_sync(JdwpServer *s, Runtime *r) { (void)s; (void)r; return 0; }
s32 jdwp_is_invoking(JdwpServer *s) { (void)s; return 0; }
void event_on_class_prepare(JdwpServer *s, Runtime *r, JClass *c) { (void)s; (void)r; (void)c; }
void event_on_class_unload(JdwpServer *s, JClass *c) { (void)s; (void)c; }
void event_on_thread_death(JdwpServer *s, Instance *t) { (void)s; (void)t; }
void event_on_thread_start(JdwpServer *s, Instance *t) { (void)s; (void)t; }
void jdwp_check_breakpoint(Runtime *r) { (void)r; }
void jdwp_check_debug_step(Runtime *r) { (void)r; }
void jdwp_check_debug_step_on_return(Runtime *r) { (void)r; }
s32 jdwp_client_count(JdwpServer *s) { (void)s; return 0; }
s32 jdwp_has_active_stepping(JdwpServer *s) { (void)s; return 0; }
