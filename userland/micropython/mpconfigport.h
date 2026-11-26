/* mpconfigport.h - configuração do MicroPython para o SaguiOS.
 *
 * O MicroPython (licença MIT, ver LICENCA-MICROPYTHON.txt) é gerado pelo
 * "embed port" do próprio projeto (ver docs/decisoes.md, D034). Aqui ficam só
 * as escolhas do SaguiOS: ponto flutuante em double pelo SSE2 (D038),
 * inteiros grandes por software (mpz), o REPL e a biblioteca padrão (D067):
 * o nível "extra" do MicroPython, com arquivos, import, unicode, os, socket
 * e ssl escritos para o SaguiOS nos *_sagui.c. */
#include <stddef.h>
#include <port/mpconfigport_common.h>

/* Nível "extra": json, re, random, binascii, heapq, select, collections
 * completo, set/frozenset, memoryview, help()... (tamanhos em D067). */
#define MICROPY_CONFIG_ROM_LEVEL                (MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES)

#define MICROPY_ENABLE_COMPILER                 (1)
#define MICROPY_ENABLE_GC                       (1)
#define MICROPY_ENABLE_FINALISER                (1)   /* arquivos e sockets esquecidos são fechados */
/* Heap que cresce sozinho (D067): começa com 4 MB e, quando uma coleta não
 * basta, pede mais memória à libsagui (dobrando), até 256 MB. Com
 * -X heap=N ou PYTHONHEAP=N o tamanho é fixo (ver mp_sagui.c). */
#define MICROPY_GC_SPLIT_HEAP                   (1)
#define MICROPY_GC_SPLIT_HEAP_AUTO              (1)
#define MP_PLAT_ALLOC_HEAP(n)                   sagui_aloca_heap(n)
#define MP_PLAT_FREE_HEAP(p)                    sagui_libera_heap(p)
#define MICROPY_PY_GC                           (1)
/* Hilos (D170): el módulo _thread sobre los hilos de usuario de la D167
 * (mpthreadport.c). Con GIL: un solo hilo ejecuta Python a la vez; se suelta
 * cada MICROPY_PY_THREAD_GIL_VM_DIVISOR saltos hacia atrás del intérprete y
 * en las esperas (time.sleep, Lock, red, entrada). */
#define MICROPY_PY_THREAD                       (1)
#define MICROPY_PY_THREAD_GIL                   (1)
#define MICROPY_PY_THREAD_GIL_VM_DIVISOR        (1024)
#define MICROPY_HELPER_REPL                     (1)
#define MICROPY_REPL_AUTO_INDENT                (0)
#define MICROPY_REPL_EMACS_KEYS                 (0)
#define MICROPY_KBD_EXCEPTION                   (0)   /* Ctrl+C: o kernel encerra o grupo (D037) */
#define MICROPY_ENABLE_SCHEDULER                (0)
/* Señales (D419): los tratadores de Python de signal.signal los llama el bucle del
 * intérprete en cada salto hacia atrás, si el manipulador de C levantó la bandera
 * (sinais_sagui.c). Una lectura y una comparación por salto. */
extern volatile int sagui_sinais_pend;
void sagui_rodar_tratadores(void);
#define MICROPY_VM_HOOK_LOOP                    if (sagui_sinais_pend) { MARK_EXC_IP_SELECTIVE(); sagui_rodar_tratadores(); }
/* Ponto flutuante (D038): double de 64 bits, contas pelo SSE2, funções do
 * módulo math da libm_dbl (musl/FreeBSD) e sqrt pela instrução sqrtsd. */
#define MICROPY_FLOAT_IMPL                      (MICROPY_FLOAT_IMPL_DOUBLE)
/* O GCC conhece _Float16 com SSE2, mas as conversões ficam na libgcc, que
 * não é ligada: o formato 'e' do struct/array usa o código do MicroPython. */
#define MICROPY_FLOAT_USE_NATIVE_FLT16          (0)
#define MICROPY_PY_MATH                         (1)
#define MICROPY_PY_MATH_CONSTANTS               (1)
#define MICROPY_PY_MATH_SPECIAL_FUNCTIONS       (1)
#define MICROPY_PY_MATH_FACTORIAL               (1)
#define MICROPY_PY_MATH_ISCLOSE                 (1)
#define MICROPY_PY_CMATH                        (1)
#define MICROPY_PY_BUILTINS_ROUND_INT           (1)
#define MICROPY_PY_FSTRINGS                     (1)
#define MICROPY_LONGINT_IMPL                    (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_ERROR_REPORTING                 (MICROPY_ERROR_REPORTING_NORMAL)
#define MICROPY_ENABLE_SOURCE_LINE              (1)
#ifdef SAGUI_PORTABLE
/* En el PC (repositorio portable, D459): x86-64 o ARM del anfitrion; el NLR lo elige el propio MicroPython. */
#define MICROPY_GCREGS_SETJMP                   (0)
#else
#define MICROPY_NLR_X86                         (1)
/* Compiladores de código máquina (D479): @micropython.native y @micropython.viper emiten x86 de 32 bits
 * (py/emitnx86.c). El código va al montón (MP_PLAT_ALLOC_EXEC por omisión): el SaguiOS no marca páginas NX. */
#define MICROPY_EMIT_X86                        (1)
#define MICROPY_GCREGS_SETJMP                   (0)
#endif
#define MICROPY_USE_INTERNAL_ERRNO              (1)
#define MICROPY_PY_SYS_PLATFORM                 "saguios"
#define MICROPY_ALLOC_PATH_MAX                  (256)

/* Texto (D068): str em UTF-8; o terminal e os arquivos em modo texto são
 * convertidos de/para Latin-1 em arquivos_sagui.c. */
#define MICROPY_PY_BUILTINS_STR_UNICODE         (1)

/* Arquivos e import (D067): open() e os objetos de arquivo são do
 * arquivos_sagui.c, sem o VFS do MicroPython. */
#define MICROPY_VFS                             (0)
#define MICROPY_PY_IO                           (1)
#define MICROPY_PY_IO_IOBASE                    (1)
#define MICROPY_PY_SYS_STDFILES                 (1)
#define MICROPY_PY_SYS_STDIO_BUFFER             (1)
#define MICROPY_ENABLE_EXTERNAL_IMPORT          (1)
#define MICROPY_MODULE_BUILTIN_SUBPACKAGES      (1)   /* import os.path */
#define MICROPY_PY_BUILTINS_INPUT               (1)
#define mp_hal_readline                         sagui_readline

/* Módulos do extmod que entram (copiados para micropython_embed/extmod):
 * json, re, random, heapq, binascii, select e time. */
#define MICROPY_PY_JSON                         (1)
#define MICROPY_PY_RE                           (1)
#define MICROPY_PY_RE_MATCH_GROUPS              (1)
#define MICROPY_PY_RE_MATCH_SPAN_START_END      (1)
#define MICROPY_PY_RE_SUB                       (1)
#define MICROPY_PY_RANDOM                       (1)
#define MICROPY_PY_RANDOM_EXTRA_FUNCS           (1)
#define MICROPY_PY_RANDOM_SEED_INIT_FUNC        (sagui_aleatorio32())
#define MICROPY_PY_HEAPQ                        (1)
#define MICROPY_PY_BINASCII                     (1)
#define MICROPY_PY_BINASCII_CRC32               (0)   /* precisaria do deflate (uzlib) */
#define MICROPY_PY_SELECT                       (1)
#define MICROPY_PY_SELECT_SELECT                (1)
/* O select pergunta a cada objeto (ioctl POLL) e, se nada está pronto,
 * dorme 10 ms (um tique do relógio do kernel) antes de perguntar de novo. */
#define MICROPY_EVENT_POLL_HOOK                 { extern int dormir(unsigned); MP_THREAD_GIL_EXIT(); dormir(10); MP_THREAD_GIL_ENTER(); sagui_rodar_tratadores(); }
#define MICROPY_PY_TIME                         (1)
#define MICROPY_PY_TIME_GMTIME_LOCALTIME_MKTIME (0)   /* os nossos, com zona horária, em tempo_sagui.h (D080) */
#define MICROPY_PY_TIME_TIME_TIME_NS            (1)
#define MICROPY_PY_TIME_INCLUDEFILE             "tempo_sagui.h"
#define MICROPY_EPOCH_IS_1970                   (1)

/* Módulos próprios do SaguiOS no lugar dos do extmod: os (os_sagui.c),
 * hashlib (hashlib_sagui.c), socket (rede_sagui.c), ssl (ssl_sagui.c) e
 * signal (sinais_sagui.c, D419). */
#define MICROPY_PY_OS                           (0)
#define MICROPY_PY_HASHLIB                      (0)
#define MICROPY_PY_SOCKET                       (0)
#define MICROPY_PY_SSL                          (0)
/* Do nível "extra", mas sem uso aqui (e sem os fontes copiados). */
#define MICROPY_PY_ASYNCIO                      (0)
#define MICROPY_PY_UCTYPES                      (0)
#define MICROPY_PY_DEFLATE                      (0)
#define MICROPY_PY_FRAMEBUF                     (0)
#define MICROPY_PY_PLATFORM                     (0)
#define MICROPY_PY_MACHINE                      (0)
#define MICROPY_PY_MICROPYTHON_RINGIO           (0)

/* Números de erro que o SaguiOS usa e que o MicroPython não define (são os
 * do Linux, como os outros MP_E*), e a lista do módulo errno. */
#define MP_ENOTEMPTY    (39)
#define MP_ENETUNREACH  (101)
#define MICROPY_PY_ERRNO_LIST \
    X(EPERM) X(ENOENT) X(ESRCH) X(EIO) X(EBADF) X(EAGAIN) X(ENOMEM) X(EACCES) X(EFAULT) \
    X(EBUSY) X(EEXIST) X(ENODEV) X(ENOTDIR) X(EISDIR) X(EINVAL) X(EMFILE) X(ENOSPC) \
    X(ESPIPE) X(EROFS) X(EPIPE) X(ENOTEMPTY) X(EOPNOTSUPP) X(EAFNOSUPPORT) X(EADDRINUSE) \
    X(ECONNABORTED) X(ECONNRESET) X(ENOBUFS) X(EISCONN) X(ENOTCONN) X(ENETUNREACH) \
    X(ETIMEDOUT) X(ECONNREFUSED) X(EHOSTUNREACH) X(EALREADY) X(EINPROGRESS) \
    X(EINTR) X(ECHILD) X(ENOEXEC) X(EDOM) X(ERANGE)

/* random: a semente vem do kernel (mp_sagui.c). */
#include <stdint.h>
uint32_t sagui_aleatorio32(void);
void *sagui_aloca_heap(size_t n);
void sagui_libera_heap(void *p);
