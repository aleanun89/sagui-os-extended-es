/* desenrolar.h - la interfaz del desenrollador de pila (D228).
 *
 * Es la interfaz «_Unwind_*» del ABI de C++ de Itanium (la que usan GCC y
 * Clang en x86): el código que genera g++ para las excepciones llama a
 * _Unwind_Resume, y la «personalidad» de C++ (excecoes.cpp) habla con el
 * desenrollador por estas funciones. Los nombres y los valores son los del
 * ABI para que el compilador los encuentre; la implementación (desenrolar.c)
 * es nuestra y solo conoce el i386.
 */
#ifndef SAGUI_DESENROLAR_H
#define SAGUI_DESENROLAR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    _URC_NO_REASON = 0,
    _URC_FOREIGN_EXCEPTION_CAUGHT = 1,
    _URC_FATAL_PHASE2_ERROR = 2,
    _URC_FATAL_PHASE1_ERROR = 3,
    _URC_NORMAL_STOP = 4,
    _URC_END_OF_STACK = 5,
    _URC_HANDLER_FOUND = 6,
    _URC_INSTALL_CONTEXT = 7,
    _URC_CONTINUE_UNWIND = 8
} _Unwind_Reason_Code;

typedef int _Unwind_Action;
#define _UA_SEARCH_PHASE   1
#define _UA_CLEANUP_PHASE  2
#define _UA_HANDLER_FRAME  4
#define _UA_FORCE_UNWIND   8
#define _UA_END_OF_STACK   16

struct _Unwind_Exception;
typedef void (*_Unwind_Exception_Cleanup_Fn)(_Unwind_Reason_Code, struct _Unwind_Exception *);

/* La cabecera que va delante de cada excepción. El ABI pide la alineación
 * máxima (16 en i386), que es la del objeto lanzado que va detrás. */
struct _Unwind_Exception {
    uint64_t exception_class;
    _Unwind_Exception_Cleanup_Fn exception_cleanup;
    uintptr_t private_1;   /* aquí: 0 (no hay desenrollado forzado) */
    uintptr_t private_2;   /* aquí: el CFA del cuadro que tiene el manejador (fase 1) */
} __attribute__((__aligned__(16)));

struct _Unwind_Context;

typedef _Unwind_Reason_Code (*_Unwind_Personality_Fn)(int version, _Unwind_Action acoes,
    uint64_t classe, struct _Unwind_Exception *exc, struct _Unwind_Context *ctx);

_Unwind_Reason_Code _Unwind_RaiseException(struct _Unwind_Exception *exc);
_Unwind_Reason_Code _Unwind_Resume_or_Rethrow(struct _Unwind_Exception *exc);
void      _Unwind_Resume(struct _Unwind_Exception *exc) __attribute__((noreturn));
void      _Unwind_DeleteException(struct _Unwind_Exception *exc);
uintptr_t _Unwind_GetGR(struct _Unwind_Context *ctx, int reg);
void      _Unwind_SetGR(struct _Unwind_Context *ctx, int reg, uintptr_t valor);
uintptr_t _Unwind_GetIP(struct _Unwind_Context *ctx);
uintptr_t _Unwind_GetIPInfo(struct _Unwind_Context *ctx, int *antes_da_instrucao);
void      _Unwind_SetIP(struct _Unwind_Context *ctx, uintptr_t valor);
uintptr_t _Unwind_GetLanguageSpecificData(struct _Unwind_Context *ctx);
uintptr_t _Unwind_GetRegionStart(struct _Unwind_Context *ctx);
uintptr_t _Unwind_GetCFA(struct _Unwind_Context *ctx);

/* Lo que el desenrollador necesita del sistema (cxxrt_sistema.c, o el de la
 * prueba en el PC): un aviso de una línea antes de parar el programa. */
void cxxrt_aviso(const char *texto);

/* Las codificaciones de punteros de DWARF (.eh_frame y las tablas de C++). */
#define DW_EH_PE_absptr   0x00
#define DW_EH_PE_uleb128  0x01
#define DW_EH_PE_udata2   0x02
#define DW_EH_PE_udata4   0x03
#define DW_EH_PE_udata8   0x04
#define DW_EH_PE_sleb128  0x09
#define DW_EH_PE_sdata2   0x0A
#define DW_EH_PE_sdata4   0x0B
#define DW_EH_PE_sdata8   0x0C
#define DW_EH_PE_pcrel    0x10
#define DW_EH_PE_textrel  0x20
#define DW_EH_PE_datarel  0x30
#define DW_EH_PE_funcrel  0x40
#define DW_EH_PE_aligned  0x50
#define DW_EH_PE_indirect 0x80
#define DW_EH_PE_omit     0xFF

/* Lectura de los números y punteros de DWARF; las comparten el desenrollador
 * y la personalidad. ler_ponteiro devuelve 0 si la codificación no se conoce. */
uintptr_t dw_ler_uleb(const uint8_t **p);
intptr_t  dw_ler_sleb(const uint8_t **p);
int       dw_ler_ponteiro(const uint8_t **p, uint8_t cod, uintptr_t base_func, uintptr_t *saida);
unsigned  dw_tamanho_codificacao(uint8_t cod);

#ifdef __cplusplus
}
#endif

#endif
