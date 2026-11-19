// SPDX-License-Identifier: GPL-2.0-or-later
/* fpu.c - FPU x87, MMX y SSE para los programas.
 *
 * Hasta la fase 5 el CR0.EM estaba activado y cualquier instrucción de punto
 * flotante, MMX o SSE terminaba el programa. Aquí la CPU se consulta por el
 * CPUID y, si tiene FPU, el kernel:
 *   - desactiva CR0.EM y activa CR0.MP y CR0.NE (los errores de la FPU pasan a la
 *     excepción 16, no a la IRQ 13 del PIC);
 *   - con FXSR, activa CR4.OSFXSR (SSE y FXSAVE/FXRSTOR) y, con SSE,
 *     CR4.OSXMMEXCPT (los errores SSE pasan a la excepción 19).
 *
 * Los registros x87/MMX/XMM son de cada proceso, pero guardarlos en cada
 * cambio costaría 512 bytes de ida y vuelta cada 20 ms, incluso para quien
 * nunca usa la FPU (casi todos). Por eso el cambio es perezoso: el
 * planificador solo activa CR0.TS cuando el siguiente proceso no es el "dueño" del
 * estado cargado. La primera instrucción de FPU/MMX/SSE suya genera la excepción
 * 7 (#NM); el manejador guarda el estado del dueño anterior, carga el del nuevo (o
 * un estado limpio, la primera vez) y desactiva TS. El kernel se compila con
 * -mgeneral-regs-only y nunca toca esos registros, así que no necesita
 * protegerse de ellos. Ver D032.
 *
 * Excepción (D042): las copias y rellenos grandes de la interfaz gráfica
 * usan SSE2 (kernel/lib/sse2.c), siempre entre fpu_kernel_inicio() y
 * fpu_kernel_fim(), como el kernel_fpu_begin/end de Linux: el estado del dueño
 * va a su área y nadie queda como dueño; al final el TS vuelve a activarse y
 * el siguiente #NM devuelve los registros a quien vaya a usarlos.
 *
 * AVX (D381): si la CPU tiene XSAVE y AVX, y no está apagado (kconfig avx=no, mkimagem.py
 * --sem-avx o el modo rescate), se enciende CR4.OSXSAVE y XCR0 = x87|SSE|AVX, y el estado se
 * guarda con XSAVE/XRSTOR (832 bytes: los YMM enteros) en lugar de FXSAVE/FXRSTOR. Todo lo
 * demás (cambio perezoso, #NM, fpu_kernel_inicio/fim, SMP) es igual: solo cambian salvar() y
 * carregar(). Sin AVX, nada cambia. Se usa XSAVE y no XSAVEOPT a propósito: XSAVEOPT puede
 * saltarse lo que «no cambió desde el último XRSTOR de esa dirección», una optimización que
 * depende de que nadie más escriba el área; con el cambio perezoso ya se guarda poco y no vale
 * el riesgo. El kernel nunca ejecuta instrucciones VEX: su SSE (sse2.c...) es el clásico, que
 * respeta las mitades altas de los YMM del proceso (y además va después de guardarlas). */
#include "fpu.h"
#include "console.h"
#include "cpu.h"
#include "processo.h"
#include "texto.h"
#include "smp.h"
#include "bootinfo.h"
#include "kconf.h"

#define CR0_MP 0x02
#define CR0_EM 0x04
#define CR0_TS 0x08
#define CR0_NE 0x20
#define CR4_OSFXSR     0x200
#define CR4_OSXMMEXCPT 0x400
#define CR4_OSXSAVE    0x40000
#define XCR0_X87 0x1
#define XCR0_SSE 0x2
#define XCR0_AVX 0x4
#define XCR0_SAGUI (XCR0_X87 | XCR0_SSE | XCR0_AVX)   /* lo único que se enciende (nada de AVX-512, MPX...) */

static uint32_t recursos;
static char fabricante[13];
/* Por CPU (D164): cada procesador tiene sus propios registros de FPU/SSE, así
 * que el dueño de esos registros y el uso del kernel son de cada uno. */
static processo_t *dono_cpu[CPU_MAX];   /* de quién es el estado que está en los registros de cada CPU */
static int kernel_nivel_cpu[CPU_MAX];   /* fpu_kernel_inicio() abiertos (pueden anidarse) */
static bool kernel_int_cpu[CPU_MAX];    /* las interrupciones estaban activadas en el primer inicio */
#define dono         dono_cpu[cpu_atual()]
#define kernel_nivel kernel_nivel_cpu[cpu_atual()]
#define kernel_int   kernel_int_cpu[cpu_atual()]
static uint8_t estado_limpo[FPU_ESTADO] __attribute__((aligned(FPU_ESTADO_ALINHA)));
static const char *avx_motivo = "la CPU no tiene AVX";
_Static_assert(FPU_ESTADO % FPU_ESTADO_ALINHA == 0, "el área del XSAVE tiene que medir un múltiplo de 64");
/* Las áreas de los procesos van en tablas estáticas de processo_t (processos.c): con la
 * alineación del tipo y el campo a 64, todas quedan a 64 (XSAVE da #GP si no). */
_Static_assert(__alignof__(processo_t) >= FPU_ESTADO_ALINHA, "processo_t tiene que ir alineado a 64");
_Static_assert(__builtin_offsetof(processo_t, fpu) % FPU_ESTADO_ALINHA == 0, "el área fpu tiene que ir alineada a 64");

static inline void cpuid(uint32_t folha, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(folha), "c"(0));
}

/* El CPUID existe si el bit ID (21) del EFLAGS se puede cambiar. */
static bool tem_cpuid(void)
{
    uint32_t antes, depois;
    __asm__ volatile("pushf; pop %0; mov %0, %1; xor $0x200000, %1; push %1; popf; pushf; pop %1; push %0; popf"
                     : "=&r"(antes), "=&r"(depois));
    return ((antes ^ depois) & 0x200000) != 0;
}

static inline uint32_t le_cr4(void) { uint32_t v; __asm__ volatile("mov %%cr4, %0" : "=r"(v)); return v; }
static inline void escreve_cr0(uint32_t v) { __asm__ volatile("mov %0, %%cr0" : : "r"(v) : "memory"); }
static inline void escreve_cr4(uint32_t v) { __asm__ volatile("mov %0, %%cr4" : : "r"(v) : "memory"); }

/* XSETBV/XGETBV del registro XCR0 (solo con CR4.OSXSAVE). */
static inline void escreve_xcr0(uint32_t v) { __asm__ volatile("xsetbv" : : "c"(0), "a"(v), "d"(0) : "memory"); }
static inline uint32_t le_xcr0(void) { uint32_t a, d; __asm__ volatile("xgetbv" : "=a"(a), "=d"(d) : "c"(0)); return a; }

/* XSAVE/XRSTOR con la máscara EDX:EAX = XCR0_SAGUI. El área: 64 bytes de alineación, y la
 * cabecera (512-575) con XCOMP_BV y lo reservado a cero, o XRSTOR da #GP: las áreas son
 * estáticas o se ponen a cero al crear el proceso, y solo las escriben XSAVE (que en la
 * cabecera solo toca XSTATE_BV) y nada más. */
static void salvar(uint8_t *area)
{
    if (recursos & CPU_AVX)
        __asm__ volatile("xsave (%0)" : : "r"(area), "a"(XCR0_SAGUI), "d"(0) : "memory");
    else if (recursos & CPU_FXSR) __asm__ volatile("fxsave (%0)" : : "r"(area) : "memory");
    else __asm__ volatile("fnsave (%0); fwait" : : "r"(area) : "memory");
}

static void carregar(const uint8_t *area)
{
    if (recursos & CPU_AVX)
        __asm__ volatile("xrstor (%0)" : : "r"(area), "a"(XCR0_SAGUI), "d"(0) : "memory");
    else if (recursos & CPU_FXSR) __asm__ volatile("fxrstor (%0)" : : "r"(area) : "memory");
    else __asm__ volatile("frstor (%0)" : : "r"(area) : "memory");
}

/* #NM: un programa usó la FPU/MMX/SSE con CR0.TS activado. */
static quadro_t *dispositivo_indisponivel(quadro_t *q)
{
    if (!(q->cs & 3))
        excecao_fatal(q);   /* el kernel solo usa SSE entre fpu_kernel_inicio/fim (TS desactivado): si llegó aquí, es un error suyo */
    __asm__ volatile("clts");
    if (dono != processo_atual()) {
        if (dono) salvar(dono->fpu);
        carregar(processo_atual()->fpu_usada ? processo_atual()->fpu : estado_limpo);
        processo_atual()->fpu_usada = true;
        dono = processo_atual();
    }
    return q;
}

void fpu_trocar(processo_t *prox)
{
    if (!(recursos & CPU_FPU)) return;
    /* Con varias CPU (D164) un proceso puede seguir en otra CPU: su estado
     * tiene que estar en memoria, no en los registros de esta. Se guarda al
     * cambiar a otro proceso; si vuelve el mismo, los registros siguen siendo suyos. */
    if (smp_arrancadas() && dono && dono != prox) {
        __asm__ volatile("clts" ::: "memory");
        salvar(dono->fpu);
        dono = NULL;
    }
    uint32_t cr0 = le_cr0();
    uint32_t novo = prox == dono ? cr0 & ~CR0_TS : cr0 | CR0_TS;
    if (novo != cr0) escreve_cr0(novo);
}

/* El kernel va a usar SSE/SSE2. Interrupciones desactivadas hasta el fin: nada puede
 * cambiar de proceso ni llamar a otra rutina con SSE en medio (el kernel no es
 * reentrante, D010, y ninguna interrupción usa SSE). Devuelve false si la CPU
 * no tiene SSE (D208: antes pedía SSE2); entonces quien llamó usa el camino sin SSE y no llama al fin. */
bool fpu_kernel_inicio(void)
{
    if ((recursos & (CPU_FPU | CPU_SSE)) != (CPU_FPU | CPU_SSE)) return false;   /* D208: basta SSE; cada variante pide lo suyo */
    bool estavam = int_salva_desliga();
    if (kernel_nivel++ == 0) {
        kernel_int = estavam;
        __asm__ volatile("clts" ::: "memory");
        if (dono) {               /* los registros son de un proceso: guarda */
            salvar(dono->fpu);
            dono = NULL;          /* el próximo #NM suyo recarga desde el área */
        }
    }
    return true;
}

void fpu_kernel_fim(void)
{
    if (kernel_nivel <= 0) return;
    if (--kernel_nivel == 0) {
        escreve_cr0(le_cr0() | CR0_TS);   /* nadie es dueño: quien use la FPU recibe #NM */
        int_restaura(kernel_int);
    }
}

bool fpu_kernel_ativo(void) { return kernel_nivel > 0; }

void fpu_esquecer(processo_t *p)
{
    /* el próximo #NM no guarda nada encima de un proceso muerto, en ninguna CPU */
    for (int i = 0; i < CPU_MAX; i++)
        if (dono_cpu[i] == p) dono_cpu[i] = NULL;
}

/* D381: ¿se puede encender AVX en esta CPU? Todo se mira antes de tocar CR4 o XCR0: CPUID.1
 * (XSAVE, AVX), la hoja 0xD (que XCR0 admita x87|SSE|AVX y que el área quepa en FPU_ESTADO).
 * Devuelve NULL si sí, o el motivo si no. "max" es la hoja más alta de CPUID. */
static const char *avx_possivel(uint32_t max)
{
    uint32_t a, b, c, d;
    if ((recursos & (CPU_FXSR | CPU_SSE | CPU_SSE2)) != (CPU_FXSR | CPU_SSE | CPU_SSE2)) return "la CPU no tiene AVX";
    cpuid(1, &a, &b, &c, &d);
    if (!(c & (1u << 28))) return "la CPU no tiene AVX";
    if (!(c & (1u << 26))) return "la CPU no tiene XSAVE";
    if (max < 0xD) return "la CPU no tiene la hoja 0xD de CPUID";
    cpuid(0xD, &a, &b, &c, &d);
    if ((a & XCR0_SAGUI) != XCR0_SAGUI) return "XCR0 no admite x87|SSE|AVX";
    /* Subhoja 2 (AVX): tamaño 256 y desplazamiento dentro del área (en el formato estándar, 576). */
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0xD), "c"(2));
    if (a != 256 || b < 576 || b + a > FPU_ESTADO) return "el area del XSAVE no cabe";
    if ((uint32_t)(uintptr_t)estado_limpo & (FPU_ESTADO_ALINHA - 1))
        return "el area del XSAVE no esta alineada";
    if (kconf_cab(CAB_CPU_OPC) & CAB_CPU_SEM_AVX) return "apagado (kconfig avx=no o --sem-avx)";
    if (info_boot.opcoes & OPC_BOOT_RESCATE) return "apagado en el modo rescate";
    return NULL;
}

/* Enciende CR4.OSXSAVE y XCR0 en esta CPU y comprueba que el tamaño del área que da CPUID
 * con ese XCR0 cabe. Si no, lo deja como estaba (sin OSXSAVE) y devuelve false. */
static bool avx_ligar_cpu(void)
{
    escreve_cr4(le_cr4() | CR4_OSXSAVE);
    escreve_xcr0(XCR0_SAGUI);
    uint32_t a, b, c, d;
    cpuid(0xD, &a, &b, &c, &d);
    if (le_xcr0() == XCR0_SAGUI && b >= 576 && b <= FPU_ESTADO) return true;
    escreve_xcr0(XCR0_X87 | XCR0_SSE);
    escreve_cr4(le_cr4() & ~(uint32_t)CR4_OSXSAVE);
    return false;
}

void fpu_iniciar(void)
{
    uint32_t max = 0;
    if (tem_cpuid()) {
        uint32_t a, b, c, d;
        cpuid(0, &a, &b, &c, &d);
        max = a;
        memcpy(fabricante, &b, 4); memcpy(fabricante + 4, &d, 4); memcpy(fabricante + 8, &c, 4);
        if (a >= 1) {
            cpuid(1, &a, &b, &c, &d);
            if (d & (1u << 0))  recursos |= CPU_FPU;
            if (d & (1u << 23)) recursos |= CPU_MMX;
            if (d & (1u << 24)) recursos |= CPU_FXSR;
            if (d & (1u << 25)) recursos |= CPU_SSE;
            if (d & (1u << 26)) recursos |= CPU_SSE2;
            if (c & (1u << 0))  recursos |= CPU_SSE3;
            if (c & (1u << 9))  recursos |= CPU_SSSE3;
            if (c & (1u << 19)) recursos |= CPU_SSE41;
            if (c & (1u << 20)) recursos |= CPU_SSE42;
        }
    } else {
        txt_copia_n(fabricante, "sem CPUID", sizeof fabricante);
    }
    /* SSE sin FXSR no existe en la práctica, y sin FXSR no hay forma de guardar los XMM. */
    if (!(recursos & CPU_FXSR)) recursos &= ~(uint32_t)(CPU_SSE | CPU_SSE2 | CPU_SSE3 | CPU_SSSE3 | CPU_SSE41 | CPU_SSE42);
    if (!(recursos & CPU_FPU)) {
        klog("[CPU] %s sem FPU: CR0.EM continua ligado\n", fabricante);
        return;
    }

    escreve_cr0((le_cr0() & ~(uint32_t)(CR0_EM | CR0_TS)) | CR0_MP | CR0_NE);
    if (recursos & CPU_FXSR) {
        uint32_t cr4 = le_cr4() | CR4_OSFXSR;
        if (recursos & CPU_SSE) cr4 |= CR4_OSXMMEXCPT;
        escreve_cr4(cr4);
    }
    /* D381: AVX, si se puede y no está apagado. Antes del estado limpio: así se guarda ya con XSAVE. */
    if (max) {
        avx_motivo = avx_possivel(max);
        if (!avx_motivo) {
            if (avx_ligar_cpu()) {
                uint32_t a, b, c, d;
                recursos |= CPU_AVX;
                cpuid(1, &a, &b, &c, &d);
                if (c & (1u << 12)) recursos |= CPU_FMA;
                if (max >= 7) {
                    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(7), "c"(0));
                    if (b & (1u << 5)) recursos |= CPU_AVX2;
                }
                avx_motivo = "encendido (XSAVE, x87|SSE|AVX)";
            } else {
                avx_motivo = "CPUID da un area del XSAVE que no cabe: apagado";
            }
        }
    }
    /* Estado limpio: FPU reiniciada y, con SSE, MXCSR por defecto (todas las
     * excepciones enmascaradas, redondeo al más cercano). La x87 empieza a 53
     * bits (palabra de control 0x027F en lugar del 0x037F del FNINIT, D236):
     * cada operación de double redondea como en el SSE2, que es lo que espera
     * la libm x87 (floor/ceil con el truco de 2^52). Va en este estado, que
     * todo proceso recibe en su primer #NM, y no en el inicio de cada
     * programa: así quien no usa la FPU no paga el #NM ni el FXSAVE, y en una
     * CPU sin FPU nada ejecuta una instrucción de x87. Quien quiera los 64
     * bits (el cc, el Pascal, el Math del Cipó) la cambia él. */
    __asm__ volatile("fninit");
    {
        uint16_t cw = 0x027F;
        __asm__ volatile("fldcw %0" : : "m"(cw));
    }
    if (recursos & CPU_SSE) {
        uint32_t mxcsr = 0x1F80;
        __asm__ volatile("ldmxcsr %0" : : "m"(mxcsr));
    }
    salvar(estado_limpo);
    if (!(recursos & CPU_FXSR)) carregar(estado_limpo);   /* el FNSAVE reinicia la FPU; deja igual al guardado */
    idt_registrar(7, dispositivo_indisponivel);
    escreve_cr0(le_cr0() | CR0_TS);   /* nadie es dueño todavía */

    char s[96];
    fpu_descrever(s, sizeof s);
    klog("[CPU] %s: %s\n", fabricante, s);
    klog("[CPU] AVX: %s\n", avx_motivo);
}

/* Un procesador secundario (D164): lo mismo que fpu_iniciar() en el principal,
 * con los recursos que este ya detectó (todos los núcleos son iguales). Sin
 * esto el secundario ejecutaba código SSE del kernel sin CR4.OSFXSR. */
bool fpu_iniciar_secundaria(void)
{
    { void rastro_mce_secundaria(void); rastro_mce_secundaria(); }   /* depuración (rastro.c): CR4.MCE también aquí */
    if (!(recursos & CPU_FPU)) return true;
    escreve_cr0((le_cr0() & ~(uint32_t)(CR0_EM | CR0_TS)) | CR0_MP | CR0_NE);
    if (recursos & CPU_FXSR) {
        uint32_t cr4 = le_cr4() | CR4_OSFXSR;
        if (recursos & CPU_SSE) cr4 |= CR4_OSXMMEXCPT;
        escreve_cr4(cr4);
    }
    /* D381: con AVX encendido en el principal, también aquí (el trampolín no copia OSXSAVE:
     * se pone después de comprobar CPUID en esta misma CPU). Un proceso puede pasar de una CPU a
     * otra con sus YMM, así que una CPU que no pueda encenderlo no se usa (false: no arranca). */
    if (recursos & CPU_AVX) {
        uint32_t a, b, c, d;
        cpuid(1, &a, &b, &c, &d);
        if ((c & ((1u << 26) | (1u << 28))) != ((1u << 26) | (1u << 28))) return false;
        cpuid(0xD, &a, &b, &c, &d);
        if ((a & XCR0_SAGUI) != XCR0_SAGUI) return false;
        if (!avx_ligar_cpu()) return false;
    }
    __asm__ volatile("fninit");
    if (recursos & CPU_SSE) {
        uint32_t mxcsr = 0x1F80;
        __asm__ volatile("ldmxcsr %0" : : "m"(mxcsr));
    }
    escreve_cr0(le_cr0() | CR0_TS);   /* como en el principal: nadie es dueño todavía */
    return true;
}

uint32_t fpu_recursos(void) { return recursos; }
const char *fpu_avx_motivo(void) { return avx_motivo; }
const char *cpu_fabricante(void) { return fabricante; }

int fpu_descrever(char *s, int max)
{
    static const struct { uint32_t bit; const char *nome; } nomes[] = {
        { CPU_FPU, "FPU" }, { CPU_MMX, "MMX" }, { CPU_SSE, "SSE" }, { CPU_SSE2, "SSE2" },
        { CPU_SSE3, "SSE3" }, { CPU_SSSE3, "SSSE3" }, { CPU_SSE41, "SSE4.1" }, { CPU_SSE42, "SSE4.2" },
        { CPU_AVX, "AVX" }, { CPU_AVX2, "AVX2" }, { CPU_FMA, "FMA" },
    };
    int n = 0;
    if (max > 0) s[0] = 0;
    for (unsigned i = 0; i < sizeof nomes / sizeof nomes[0]; i++)
        if (recursos & nomes[i].bit)
            n += snformatar(s + n, (uint32_t)(max - n), "%s%s", n ? " " : "", nomes[i].nome);
    return n;
}
