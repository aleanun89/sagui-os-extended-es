// SPDX-License-Identifier: GPL-3.0-only
/* paginacao.c - paginación en dos modos: clásica de 2 niveles o PAE de 3 (D010, D049, D349).
 *
 * Sin PAE: directorio (1024 entradas de 4 MB) -> tabla (1024 entradas de 4 KB)
 * -> página, con entradas de 4 bytes. Con PAE (D349): PDPT (4 entradas de 1 GB)
 * -> 4 directorios (512 entradas de 2 MB cada uno) -> tabla (512 entradas de
 * 4 KB), con entradas de 8 bytes (dirección física de hasta 36 bits y NX en el
 * bit 63). Para no escribir todo dos veces, el código ve los dos modos igual:
 * un "directorio" de 1024 o 2048 entradas indexado por virt >> d_desl (22 o 21)
 * y tablas de n_tab entradas (1024 o 512); con PAE la entrada i del directorio
 * está en el directorio PDPT[i / 512], posición i % 512. Las entradas se leen y
 * escriben con ent_ler()/ent_escrever(), que saben el tamaño.
 *
 * La mitad baja del espacio (0 a 2 GB) es del kernel y es la misma en todos los
 * procesos: sin PAE se copian sus entradas del directorio al crear el espacio;
 * con PAE las dos primeras entradas del PDPT de cada proceso apuntan a los dos
 * directorios del kernel (compartidos). La mitad alta es del programa.
 *
 * El kernel ve la RAM por la propia dirección (1:1) sólo hasta el fin real de la RAM
 * (redondeado a 4 MB): lo que la BIOS pone justo encima (MMIO) queda sin mapa.
 * Ese mapa usa páginas grandes (4 MB con PSE; 2 MB con PAE) directo en el
 * directorio (bit PS), sin tablas; con PGE quedan globales (no salen del TLB al
 * cambiar de proceso). El primer bloque usa siempre una tabla de 4 KB, para que
 * la página 0 quede ausente (puntero nulo), y la franja de ventanas también,
 * porque janela_mapear() mapea página a página.
 *
 * NX (D349, solo con PAE y si la CPU lo tiene): el mapa 1:1 por encima del fin
 * del kernel, la franja de ventanas y el APIC no son ejecutables; en los
 * programas lo deciden los llamadores con PG_NX (pila, montón, datos del ELF). */
#include "imagens.h"
#include "paginacao.h"
#include "bootinfo.h"
#include "kconf.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "texto.h"
#include "cpu_dados.h"
#include "ioapic.h"
#include "memalta.h"
#include "msr.h"
#include "troca.h"
#include "copia_escrita.h"
#include "abi.h"

void espaco_sincronizar(espaco_t e);   /* processos.c (D165) */

#define PG_GRANDE     0x080   /* PDE: página grande (PS): 4 MB sin PAE, 2 MB con PAE */
#define PG_GLOBAL     0x100   /* queda en el TLB al cambiar el CR3 (con CR4.PGE) */
#define CR4_PSE       0x10
#define CR4_PAE       0x20
#define CR4_PGE       0x80
#define MSR_EFER      0xC0000080u
#define EFER_NXE      0x800u
#define END_32        0xFFFFF000ull            /* dirección en una entrada de 4 bytes */
#define END_PAE       0x000FFFFFFFFFF000ull    /* en una de 8 bytes (bits 12 a 51) */
#define APIC_VIRT     0xFFC00000u              /* APIC local e IOAPIC (D157, D166) */
/* D401: bit 10 de la entrada (libre para el sistema en los dos formatos): página compartida de
 * copia al escribir. Presente y sin W; «lógicamente escribible, copiar antes». El API usa 0x400
 * para PG_NX, que nunca llega al hardware como bit 10 (ent_valor y ent_flags lo quitan): por eso
 * esta marca no se ve en pte_flags() y solo la ponen y la quitan las funciones de aquí. */
#define HW_COW        0x400u

bool     paginacao_com_pae;
uint64_t paginacao_bit_nx;

static espaco_t dir_kernel;
static uint32_t janela_proxima = KERNEL_JANELAS;
static uint32_t fim_mapa;              /* fin del mapa 1:1 del kernel */
static bool     com_pse, com_pge, cpu_pae, cpu_nx, cpu_intel, xd_liberado;
static uint32_t max_ext;               /* la hoja extendida más alta de CPUID */
static uint32_t bits_fisicos = 32;     /* MAXPHYADDR de la CPU */
static uint64_t limite_fisico = 0x100000000ull;

/* La geometría del modo elegido (paginacao_escolher_modo). */
static uint32_t d_desl = 22;           /* bits de la dirección que cubre una entrada del directorio */
static uint32_t n_tab = 1024;          /* entradas por tabla */
static uint32_t t_ent = 4;             /* bytes por entrada */
static uint64_t mascara_end = END_32;

#define PAE           paginacao_com_pae
#define PDE_KERNEL    (USUARIO_INICIO >> d_desl)     /* 512 o 1024 */
#define PDE_USUARIO_FIM (USUARIO_FIM >> d_desl)      /* 1023 o 2046 */
#define PDE_APIC      (APIC_VIRT >> d_desl)
#define BLOCO         (1u << d_desl)                 /* lo que cubre una entrada del directorio */

/* ------------------------------------------------------------------ entradas */

static inline pte_t ent_ler(const void *p)
{
    const volatile uint32_t *m = (const volatile uint32_t *)p;
    if (!PAE) return m[0];
    uint32_t baixa = m[0], alta = m[1];
    return (uint64_t)alta << 32 | baixa;
}

/* Escribe el valor ya hecho de una entrada; con PAE, en el orden seguro de pte_escrever. */
static inline void ent_escrever(void *p, pte_t v)
{
    volatile uint32_t *m = (volatile uint32_t *)p;
    if (!PAE) { m[0] = (uint32_t)v; return; }
    m[0] = 0;
    __asm__ volatile("" ::: "memory");
    m[1] = (uint32_t)(v >> 32);
    __asm__ volatile("" ::: "memory");
    m[0] = (uint32_t)v;
}

/* Valor de una entrada a partir de la dirección y las banderas del API (PG_NX incluida). */
static inline pte_t ent_valor(fis_t fis, uint32_t flags)
{
    pte_t v = (fis & mascara_end) | (flags & 0xFFFu & ~(uint32_t)PG_NX);
    if (flags & PG_NX) v |= paginacao_bit_nx;
    return v;
}

/* Las banderas del API de una entrada (PG_NX si tiene el bit 63). */
static inline uint32_t ent_flags(pte_t v)
{
    uint32_t f = (uint32_t)v & 0xFFFu & ~(uint32_t)PG_NX;
    if (paginacao_bit_nx && (v & paginacao_bit_nx)) f |= PG_NX;
    return f;
}

static inline fis_t ent_end(pte_t v) { return v & mascara_end; }

/* Cambia bits de una entrada PRESENTE sin pasar nunca por P=0 y sin perder los
 * bits A/D que el hardware ponga a la vez: un cmpxchg atómico (cmpxchg8b con PAE;
 * toda CPU con PAE lo tiene). Para permisos (mprotect): otro hilo del mismo
 * proceso en otra CPU puede estar recorriendo la tabla en ese instante. */
static void ent_trocar(void *p, pte_t por, pte_t tirar)
{
    if (PAE) {
        /* lock cmpxchg8b a mano: el gcc de 32 bits lo dejaría en una llamada a la libatomic. */
        uint32_t lo = ((volatile uint32_t *)p)[0], hi = ((volatile uint32_t *)p)[1];
        bool igual;
        do {
            uint64_t novo = (((uint64_t)hi << 32 | lo) | por) & ~tirar;
            __asm__ volatile("lock cmpxchg8b %1"
                             : "=@ccz"(igual), "+m"(*(volatile uint64_t *)p), "+a"(lo), "+d"(hi)
                             : "b"((uint32_t)novo), "c"((uint32_t)(novo >> 32))
                             : "memory");
        } while (!igual);   /* si otro (el hardware, con A/D) cambió la entrada, lo y hi traen la nueva */
    } else {
        uint32_t *e = (uint32_t *)p;
        uint32_t velho = *(volatile uint32_t *)e;
        while (!__atomic_compare_exchange_n(e, &velho, (velho | (uint32_t)por) & ~(uint32_t)tirar, false,
                                            __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            ;
    }
}

/* Banderas del API (PG_ESCRITA, PG_NX) a bits de la entrada. */
static inline pte_t bits_hw(uint32_t f)
{
    return (f & PG_ESCRITA) | ((f & PG_NX) ? paginacao_bit_nx : 0);
}
static inline bool grande(pte_t pde) { return (pde & (PG_PRESENTE | PG_GRANDE)) == (PG_PRESENTE | PG_GRANDE); }

/* La entrada i del directorio de un espacio. Con PAE, el directorio PDPT[i / 512]
 * (los cuatro existen siempre, en la memoria baja). */
static inline void *pde_de(espaco_t e, uint32_t i)
{
    if (!PAE) return (uint32_t *)e + i;
    const uint64_t *pdpt = (const uint64_t *)e;
    return (uint64_t *)(uint32_t)(pdpt[i >> 9] & END_32) + (i & 511);
}

/* La entrada k de la tabla a la que apunta una entrada de directorio (nunca una grande). */
static inline void *pte_de(pte_t pde, uint32_t k)
{
    return (uint8_t *)(uint32_t)ent_end(pde) + k * t_ent;
}

static inline void invalidar(uint32_t virt) { pagina_invalidar(virt); }

/* Una página a cero de la zona baja: directorios, tablas y PDPT, que el
 * kernel lee y escribe por su dirección (mapeo 1:1). */
static uint32_t pagina_zerada(void)
{
    uint32_t p = memfisica_alocar();
    if (p) memset((void *)p, 0, PAGINA);
    return p;
}

/* D348: una página a cero para un programa: de la zona alta si queda, puesta
 * a cero por una ventana temporal (el kernel no la ve por su dirección). */
static fis_t pagina_usuario_zerada(void)
{
    fis_t p = memfisica_alocar_usuario();
    if (p) fisica_zerar(p);
    return p;
}

/* ------------------------------------------------------------- el modo */

static inline void cpuid(uint32_t f, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(f), "c"(0));
}

/* CPUID 1, EDX: bit 3 = PSE, bit 6 = PAE, bit 13 = PGE. CPUID 0x80000001, EDX:
 * bit 20 = NX. CPUID 0x80000008, EAX[7:0] = bits de dirección física. Sin
 * CPUID (486 antiguos), tablas de 4 KB y nada más. */
static void conferir_cpu(void)
{
    uint32_t antes, depois;
    __asm__ volatile("pushf; pop %0; mov %0, %1; xor $0x200000, %1; push %1; popf; pushf; pop %1; push %0; popf"
                     : "=&r"(antes), "=&r"(depois));
    if (!((antes ^ depois) & 0x200000)) return;
    uint32_t a, b, c, d;
    cpuid(0, &a, &b, &c, &d);
    cpu_intel = b == 0x756E6547u && d == 0x49656E69u && c == 0x6C65746Eu;   /* "GenuineIntel" */
    if (a < 1) return;
    cpuid(1, &a, &b, &c, &d);
    com_pse = (d >> 3) & 1;
    cpu_pae = (d >> 6) & 1;
    com_pge = (d >> 13) & 1;
    bits_fisicos = cpu_pae ? 36 : 32;
    cpuid(0x80000000u, &a, &b, &c, &d);
    max_ext = a;
    if (max_ext >= 0x80000001u && max_ext < 0x8000FFFFu) {
        cpuid(0x80000001u, &a, &b, &c, &d);
        cpu_nx = (d >> 20) & 1;
    }
    if (cpu_pae && max_ext >= 0x80000008u && max_ext < 0x8000FFFFu) {
        cpuid(0x80000008u, &a, &b, &c, &d);
        if ((a & 0xFF) >= 32 && (a & 0xFF) <= 52) bits_fisicos = a & 0xFF;
    }
}

/* D349: PAE si la CPU lo anuncia y la cabecera no lo prohíbe (mkimagem.py
 * --sem-pae); NX si además la CPU lo tiene y no se pidió --sem-nx. */
void paginacao_escolher_modo(void)
{
    conferir_cpu();
    uint8_t opc = kconf_cab(CAB_MEM_OPC);   /* D353: con lo que guardó kconfig */
    /* D349: en Intel, la BIOS puede apagar NX con XD_DISABLE (IA32_MISC_ENABLE bit 34)
     * y entonces CPUID no lo anuncia. Como Linux, se vuelve a encender (lectura y
     * escritura protegidas contra #GP: si el MSR no existe no pasa nada), salvo con
     * --sem-nx. Las CPU secundarias hacen lo mismo en el trampolín. */
    if (cpu_intel && cpu_pae && !cpu_nx && !(opc & (CAB_MEM_SEM_PAE | CAB_MEM_SEM_NX)) &&
        max_ext >= 0x80000001u && max_ext < 0x8000FFFFu) {
        uint64_t misc;
        if (msr_ler_seguro(0x1A0, &misc) && (misc & (1ull << 34)) &&
            msr_escrever_seguro(0x1A0, misc & ~(1ull << 34))) {
            uint32_t a, b, c, d;
            cpuid(0x80000001u, &a, &b, &c, &d);
            cpu_nx = (d >> 20) & 1;
            xd_liberado = cpu_nx;
            klog("[SAGUI] paginacao: XD_DISABLE estaba puesto por la BIOS: quitado, NX %s\n", cpu_nx ? "disponible" : "sigue sin estar");
        }
    }
    PAE = cpu_pae && !(opc & CAB_MEM_SEM_PAE);
    paginacao_bit_nx = (PAE && cpu_nx && !(opc & CAB_MEM_SEM_NX)) ? (1ull << 63) : 0;
    if (PAE) {
        d_desl = 21;
        n_tab = 512;
        t_ent = 8;
        mascara_end = END_PAE;
        /* Hasta 64 GB (36 bits), o menos si la CPU tiene menos bits. */
        uint32_t bits = bits_fisicos > 36 ? 36 : bits_fisicos;
        limite_fisico = 1ull << bits;
    }
    klog("[SAGUI] paginacao: modo %s, NX %s (CPU: PAE %s, NX %s, %u bits fisicos%s%s)\n",
         PAE ? "PAE" : "sem PAE", paginacao_bit_nx ? "sim" : "nao", cpu_pae ? "sim" : "nao", cpu_nx ? "sim" : "nao",
         bits_fisicos, (opc & CAB_MEM_SEM_PAE) ? ", --sem-pae" : "", (opc & CAB_MEM_SEM_NX) ? ", --sem-nx" : "");
}

bool     paginacao_pae_cpu(void)        { return cpu_pae; }
bool     paginacao_nx_cpu(void)         { return cpu_nx; }
bool     paginacao_nx(void)             { return paginacao_bit_nx != 0; }
bool     paginacao_xd_liberado(void)    { return xd_liberado; }
uint64_t paginacao_limite_fisico(void)  { return limite_fisico; }

/* --------------------------------------------------------- el mapa del kernel */

/* Fin de la RAM que el kernel necesita ver: el mayor fin de región libre del E820,
 * y como mínimo el kernel y el ramdisk; redondeado hacia arriba a 4 MB y limitado
 * a la franja de ventanas. */
static uint32_t calcular_fim_mapa(void)
{
    extern char _kernel_fim[];
    uint64_t fim = (uint32_t)_kernel_fim;
    if (info_boot.ramdisk_inicio + info_boot.ramdisk_bytes > fim)
        fim = info_boot.ramdisk_inicio + info_boot.ramdisk_bytes;
    for (uint32_t i = 0; i < info_boot.mapa_qtd; i++) {
        regiao_e820_t *r = &mapa_memoria[i];
        if (r->tipo == E820_LIVRE && (r->extra & 1) && r->base < KERNEL_JANELAS && r->base + r->tamanho > fim)
            fim = r->base + r->tamanho;
    }
    fim = (fim + 0x3FFFFF) & ~(uint64_t)0x3FFFFF;
    return fim > KERNEL_JANELAS ? KERNEL_JANELAS : (uint32_t)fim;
}

/* Una página grande sólo cubre RAM "libre" entera del E820: un bloque que
 * toca región reservada (o el primer MB, con las franjas de tipos de memoria
 * fijos del MTRR) puede tener tipos de memoria mezclados, y una página grande
 * en esas condiciones es indefinida en el manual de Intel (D010). Esos bloques
 * quedan con tabla de 4 KB. */
static bool bloco_so_ram(uint32_t base, uint32_t tam)
{
    uint64_t ini = base, fim = (uint64_t)base + tam;
    bool coberto = false;
    for (uint32_t i = 0; i < info_boot.mapa_qtd; i++) {
        regiao_e820_t *r = &mapa_memoria[i];
        if (!(r->extra & 1)) continue;
        uint64_t rf = r->base + r->tamanho;
        if (r->tipo == E820_LIVRE) {
            if (r->base <= ini && rf >= fim) coberto = true;
        } else if (r->base < fim && rf > ini) {
            return false;
        }
    }
    return coberto;
}

static uint32_t tabela_nova(void)
{
    uint32_t t = pagina_zerada();
    if (!t) { kprintf("paginacion: sin memoria para las tablas del kernel\n"); parar_maquina(); }
    return t;
}

static inline uint64_t ler_msr(uint32_t msr)
{
    uint32_t a, d;
    __asm__ volatile("rdmsr" : "=a"(a), "=d"(d) : "c"(msr));
    return ((uint64_t)d << 32) | a;
}

static inline void gravar_msr(uint32_t msr, uint64_t v)
{
    __asm__ volatile("wrmsr" : : "c"(msr), "a"((uint32_t)v), "d"((uint32_t)(v >> 32)) : "memory");
}

void paginacao_iniciar(void)
{
    extern char _kernel_fim[];
    fim_mapa = calcular_fim_mapa();
    bool grandes_ok = PAE || com_pse;   /* con PAE las páginas de 2 MB no necesitan PSE */
    uint32_t global = com_pge ? PG_GLOBAL : 0;
    /* D349: el mapa 1:1 deja de ser ejecutable a partir del fin del kernel. Lo
     * de debajo (el primer MB con el trampolín de las CPU secundarias y el
     * propio kernel) sigue ejecutable. ent_valor() ignora PG_NX sin NX. */
    uint32_t nx_desde = ((uint32_t)_kernel_fim + PAGINA - 1) & ~(PAGINA - 1);

    dir_kernel = tabela_nova();
    if (PAE) {
        /* El PDPT y sus cuatro directorios, todos en la baja. */
        uint64_t *pdpt = (uint64_t *)dir_kernel;
        for (uint32_t j = 0; j < 4; j++)
            pdpt[j] = tabela_nova() | PG_PRESENTE;   /* en el PDPT solo P (R/W y U/S son reservados) */
    }
    /* Debajo de fim_mapa: 1:1 (páginas grandes si se puede). En la franja de
     * ventanas: tablas vacías (pero existentes), para que todo proceso las
     * herede desde ya. Entre las dos: nada. */
    uint32_t grandes = 0, tabelas = 0;
    for (uint32_t i = 0; i < PDE_KERNEL; i++) {
        uint32_t base = i << d_desl;
        void *pd = pde_de(dir_kernel, i);
        if (base < fim_mapa && grandes_ok && i > 0 && bloco_so_ram(base, BLOCO)) {
            ent_escrever(pd, ent_valor(base, PG_GRANDE | global | PG_PRESENTE | PG_ESCRITA |
                                             (base >= nx_desde ? PG_NX : 0)));
            grandes++;
        } else if (base < fim_mapa) {
            tabelas++;
            uint32_t tab = tabela_nova();
            for (uint32_t k = 0; k < n_tab; k++) {
                uint32_t a = base + (k << 12);
                ent_escrever((uint8_t *)tab + k * t_ent,
                             ent_valor(a, global | PG_PRESENTE | PG_ESCRITA | (a >= nx_desde ? PG_NX : 0)));
            }
            ent_escrever(pd, ent_valor(tab, PG_PRESENTE | PG_ESCRITA));
        } else if (base >= KERNEL_JANELAS) {
            ent_escrever(pd, ent_valor(tabela_nova(), PG_PRESENTE | PG_ESCRITA));
        }
    }
    ent_escrever(pte_de(ent_ler(pde_de(dir_kernel, 0)), 0), 0);   /* página 0 ausente: puntero nulo genera falta */

    /* El APIC local (D157): está en la memoria del sistema, fuera de la
     * RAM, así que necesita un mapa propio. Se mapea al final del espacio
     * del programa (0xFFC00000), donde ningún programa llega, y el mapa se
     * copia para todos los espacios: el kernel toca el APIC incluso
     * cuando corre en el contexto de un programa (el planificador, por
     * ejemplo). */
    {
        uint32_t tab = tabela_nova();
        ent_escrever((void *)tab, ent_valor(cpu_lapic_fisico(), PG_PRESENTE | PG_ESCRITA | PG_NX));
        /* Los IOAPIC (D166), en las páginas siguientes y sin caché. Mapear
         * no toca el hardware: sin --ioapic nadie los lee. */
        for (uint32_t i = 0; i < ioapic_quantidade() && i < IOAPIC_MAX; i++)
            ent_escrever((uint8_t *)tab + (1 + i) * t_ent,
                         ent_valor(ioapic_fisico(i), PG_PCD | PG_PWT | PG_PRESENTE | PG_ESCRITA | PG_NX));
        ent_escrever(pde_de(dir_kernel, PDE_APIC), ent_valor(tab, PG_PRESENTE | PG_ESCRITA));
    }

    uint32_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    if (com_pse) cr4 |= CR4_PSE;
    if (PAE) cr4 |= CR4_PAE;
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4) : "memory");
    /* D349: EFER.NXE antes de la paginación: las tablas ya llevan el bit 63 y,
     * sin NXE, es un bit reservado (fallo de página). Las CPU secundarias lo
     * ponen en el trampolín. */
    if (paginacao_bit_nx) gravar_msr(MSR_EFER, ler_msr(MSR_EFER) | EFER_NXE);
    espaco_ativar(dir_kernel);
    /* PG enciende la paginación; WP hace que el kernel también respete páginas sólo de
     * lectura; EM hace que cualquier instrucción de punto flotante genere excepción
     * hasta que fpu_iniciar() compruebe la CPU y encienda FPU/MMX/SSE (D032). */
    uint32_t cr0 = le_cr0();
    cr0 |= 0x80000000u | 0x00010000u | 0x4u;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");
    if (com_pge)
        __asm__ volatile("mov %%cr4, %%eax; or %0, %%eax; mov %%eax, %%cr4" : : "i"(CR4_PGE) : "eax", "memory");
    klog("[SAGUI] paginacao: 1:1 ate %x (%u MB), %s, %u grandes e %u tabelas, PGE %s, %s%s\n",
         fim_mapa, fim_mapa >> 20, !grandes_ok ? "tabelas de 4 KB" : PAE ? "paginas de 2 MB" : "paginas de 4 MB",
         grandes, tabelas,
         com_pge ? "sim" : "nao", PAE ? "PAE (3 niveis)" : "2 niveis", paginacao_bit_nx ? ", NX ativo" : "");
}

uint32_t paginacao_fim_mapa(void) { return fim_mapa; }
bool     paginacao_pse(void)      { return PAE || com_pse; }

espaco_t espaco_kernel(void) { return dir_kernel; }

/* ------------------------------------------------------------ los espacios */

espaco_t espaco_criar(void)
{
    if (!PAE) {
        espaco_t e = pagina_zerada();
        if (!e) return 0;
        /* Se copia el directorio entero del kernel: las 512 primeras entradas son
         * el mapa 1:1 y las ventanas, y la última (1023) es el APIC local (D157).
         * Las entradas del programa (512 a 1022) están a cero. */
        memcpy((void *)e, (void *)dir_kernel, 1024 * sizeof(uint32_t));
        return e;
    }
    /* D349: PDPT propio; los directorios 0 y 1 (0 a 2 GB) son los del kernel,
     * compartidos; el 2 y el 3 son del programa, copiados de los del kernel
     * (vacíos salvo la entrada del APIC). */
    uint32_t pdpt = pagina_zerada(), d2 = pagina_zerada(), d3 = pagina_zerada();
    if (!pdpt || !d2 || !d3) {
        if (pdpt) memfisica_liberar(pdpt);
        if (d2) memfisica_liberar(d2);
        if (d3) memfisica_liberar(d3);
        return 0;
    }
    const uint64_t *k = (const uint64_t *)dir_kernel;
    memcpy((void *)d2, (void *)(uint32_t)(k[2] & END_32), PAGINA);
    memcpy((void *)d3, (void *)(uint32_t)(k[3] & END_32), PAGINA);
    uint64_t *n = (uint64_t *)pdpt;
    n[0] = k[0];
    n[1] = k[1];
    n[2] = d2 | PG_PRESENTE;
    n[3] = d3 | PG_PRESENTE;
    return pdpt;
}

void espaco_destruir(espaco_t e)
{
    if (!e || e == dir_kernel) return;
    if (espaco_atual() == e)
        espaco_ativar(dir_kernel);   /* nunca borrar el directorio en uso */
    for (uint32_t i = PDE_KERNEL; i < PDE_USUARIO_FIM; i++) {
        pte_t pde = ent_ler(pde_de(e, i));
        if (!(pde & PG_PRESENTE)) continue;
        for (uint32_t k = 0; k < n_tab; k++) {
            pte_t pte = ent_ler(pte_de(pde, k));
            if ((pte & PG_PRESENTE) && !(pte & PG_EMPRESTADA))   /* las prestadas tienen otro dueño */
                memfisica_liberar(ent_end(pte));
            else if (troca_e_entrada(pte))   /* D352: su trozo en el almacén de la swap */
                troca_soltar(pte);
        }
        memfisica_liberar(ent_end(pde));
    }
    if (PAE) {
        const uint64_t *pdpt = (const uint64_t *)e;
        memfisica_liberar(pdpt[2] & END_32);
        memfisica_liberar(pdpt[3] & END_32);
    }
    /* D378 (fase 3): las páginas compartidas (prestadas) no se liberaron arriba: bajan las
     * cuentas de sus objetos. Ninguna CPU usa ya "e" (lo garantiza quien destruye). Antes de
     * liberar el directorio: así su dirección no puede volver a ser otro espacio entretanto. */
    imagens_espaco_destruido(e);
    memfisica_liberar(e);
}

void espaco_ativar(espaco_t e)
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(e) : "memory");
}

espaco_t espaco_atual(void) { return le_cr3(); }

bool espaco_mapear(espaco_t e, uint32_t virt, fis_t fis, uint32_t flags)
{
    uint32_t i = virt >> d_desl, k = (virt >> 12) & (n_tab - 1);
    void *pd = pde_de(e, i);
    pte_t pde = ent_ler(pd);
    if (grande(pde)) return false;   /* mapa 1:1 del kernel en páginas grandes: no se toca */
    if (fis >= limite_fisico) return false;
    if (!(pde & PG_PRESENTE)) {
        if (i < PDE_KERNEL) return false;   /* las tablas del kernel ya existen todas */
        uint32_t t = pagina_zerada();
        if (!t) return false;
        pde = ent_valor(t, PG_PRESENTE | PG_ESCRITA | PG_USUARIO);
        ent_escrever(pd, pde);
    }
    pte_escrever(pte_de(pde, k), fis, flags | PG_PRESENTE);
    if (espaco_atual() == e || i < PDE_KERNEL)
        invalidar(virt);
    return true;
}

fis_t espaco_traduzir(espaco_t e, uint32_t virt, uint32_t *flags)
{
    pte_t pde = ent_ler(pde_de(e, virt >> d_desl));
    if (!(pde & PG_PRESENTE)) return 0;
    if (grande(pde)) {
        if (flags) *flags = ent_flags(pde) & ~(uint32_t)PG_GRANDE;
        return (ent_end(pde) & ~(fis_t)(BLOCO - 1)) | (virt & (BLOCO - 1));
    }
    void *pp = pte_de(pde, (virt >> 12) & (n_tab - 1));
    pte_t pte = ent_ler(pp);
    /* D352: una página en la swap se trae ahora: quien traduce va a usar la dirección física
     * (cargador, pila de un hilo, futex) o a tocarla (usuario_ok). Sin memoria para traerla,
     * es como si no estuviera. */
    if (troca_e_entrada(pte) && (virt >> d_desl) >= PDE_KERNEL) {
        troca_trazer(e, virt, true);
        pte = ent_ler(pp);
    }
    if (!(pte & PG_PRESENTE)) return 0;
    /* Permiso efectivo: las dos tienen que permitir (y NX en cualquiera de las dos prohíbe). */
    if (flags) {
        uint32_t fp = ent_flags(pte), fd = ent_flags(pde);
        *flags = (fp & fd & ~(uint32_t)PG_NX) | ((fp | fd) & PG_NX);
    }
    return ent_end(pte) | (virt & 0xFFF);
}

bool espaco_alocar(espaco_t e, uint32_t virt, uint32_t bytes, uint32_t flags)
{
    uint32_t ini = virt & ~(PAGINA - 1), fim = (virt + bytes + PAGINA - 1) & ~(PAGINA - 1);
    if (ini < USUARIO_INICIO || fim > USUARIO_FIM || fim < ini) return false;
    for (uint32_t v = ini; v < fim; v += PAGINA) {
        if (espaco_existe(e, v)) continue;   /* ya existe (también si está en la swap, D352): mantiene */
        fis_t p = pagina_usuario_zerada();
        /* D352: con A puesta, el reloj de la swap le da una vuelta antes de sacarla. */
        if (!p || !espaco_mapear(e, v, p, flags | PG_ACESSADA)) {
            if (p) memfisica_liberar(p);
            return false;
        }
    }
    return true;
}

void espaco_ajustar(espaco_t e, uint32_t virt, uint32_t bytes, uint32_t por, uint32_t tirar)
{
    uint32_t ini = virt & ~(PAGINA - 1), fim = (virt + bytes + PAGINA - 1) & ~(PAGINA - 1);
    if (ini < USUARIO_INICIO || fim > USUARIO_FIM || fim < ini) return;
    por &= PG_ESCRITA | PG_NX;
    tirar &= PG_ESCRITA | PG_NX;
    for (uint32_t v = ini; v < fim; v += PAGINA) {
        pte_t pde = ent_ler(pde_de(e, v >> d_desl));
        if (!(pde & PG_PRESENTE) || grande(pde)) continue;
        void *p = pte_de(pde, (v >> 12) & (n_tab - 1));
        pte_t pte = ent_ler(p);
        if (troca_e_entrada(pte)) {
            /* D352: en la swap, los permisos se guardan en la propia entrada (no presente: el
             * hardware no la mira, y el kernel la cambia con el cerrojo general). */
            uint32_t b = (uint32_t)pte;
            if (por & PG_ESCRITA) b |= TROCA_ESCRITA;
            if (por & PG_NX) b |= TROCA_NX;
            if (tirar & PG_ESCRITA) b &= ~TROCA_ESCRITA;
            if (tirar & PG_NX) b &= ~TROCA_NX;
            ent_escrever(p, b);
            continue;
        }
        if (!(pte & PG_PRESENTE) || (pte & PG_EMPRESTADA)) continue;
        if ((pte & HW_COW) || cow_refs(ent_end(pte)) > 1) {
            /* D401: una página compartida nunca recibe W: dar escritura la deja de copia al
             * escribir (el fallo hará la copia) y quitarla quita también la marca. */
            pte_t pv = bits_hw(por) & ~(pte_t)PG_ESCRITA, tv = bits_hw(tirar);
            if (por & PG_ESCRITA) pv |= HW_COW;
            if (tirar & PG_ESCRITA) tv |= HW_COW;
            ent_trocar(p, pv, tv);
        } else {
            ent_trocar(p, bits_hw(por), bits_hw(tirar));   /* el marco no cambia: nunca P=0 */
        }
        if (espaco_atual() == e) invalidar(v);
    }
}

void espaco_desmapear(espaco_t e, uint32_t virt)
{
    pte_t pde = ent_ler(pde_de(e, virt >> d_desl));
    if (!(pde & PG_PRESENTE) || grande(pde)) return;
    ent_escrever(pte_de(pde, (virt >> 12) & (n_tab - 1)), 0);
    if (espaco_atual() == e) invalidar(virt);
}

fis_t espaco_tirar(espaco_t e, uint32_t virt)
{
    if ((virt >> d_desl) < PDE_KERNEL) return 0;
    pte_t pde = ent_ler(pde_de(e, virt >> d_desl));
    if (!(pde & PG_PRESENTE) || grande(pde)) return 0;
    void *p = pte_de(pde, (virt >> 12) & (n_tab - 1));
    pte_t pte = ent_ler(p);
    /* La marca de prestada solo está en la PTE (espaco_traduzir la pierde al
     * combinarla con la PDE): por eso se mira aquí. */
    if (troca_e_entrada(pte)) {   /* D352: estaba en la swap: se suelta su trozo */
        ent_escrever(p, 0);
        troca_soltar(pte);
        return 0;
    }
    if (!(pte & PG_PRESENTE) || (pte & PG_EMPRESTADA)) return 0;
    ent_escrever(p, 0);
    if (espaco_atual() == e) invalidar(virt);
    return ent_end(pte);
}

uint32_t espaco_paginas_usuario(espaco_t e)
{
    uint32_t n = 0;
    for (uint32_t i = PDE_KERNEL; i < PDE_USUARIO_FIM; i++) {
        pte_t pde = ent_ler(pde_de(e, i));
        if (!(pde & PG_PRESENTE)) continue;
        for (uint32_t k = 0; k < n_tab; k++)
            if (ent_ler(pte_de(pde, k)) & PG_PRESENTE) n++;
    }
    return n;
}

/* ------------------------------------------------- copia al escribir (D401) */

/* Cambia una entrada presente por otra de golpe: con PAE, un cmpxchg8b (nunca pasa por P=0 ni
 * por una mitad vieja y otra nueva, que otra CPU podría ver al recorrer la tabla). */
static void ent_substituir(void *p, pte_t novo)
{
    if (PAE) {
        uint32_t lo = ((volatile uint32_t *)p)[0], hi = ((volatile uint32_t *)p)[1];
        bool igual;
        do {
            __asm__ volatile("lock cmpxchg8b %1"
                             : "=@ccz"(igual), "+m"(*(volatile uint64_t *)p), "+a"(lo), "+d"(hi)
                             : "b"((uint32_t)novo), "c"((uint32_t)(novo >> 32))
                             : "memory");
        } while (!igual);
    } else {
        __atomic_store_n((uint32_t *)p, (uint32_t)novo, __ATOMIC_SEQ_CST);
    }
}

bool pte_partilhada(pte_t v)
{
    return (v & PG_PRESENTE) && ((v & HW_COW) || cow_refs(ent_end(v)) > 1);
}

/* El fallo de escritura en "virt" (página presente sin W). 0: no es de copia al escribir (un
 * fallo de verdad); 1: resuelto (o ya lo resolvió otra CPU), repetir la instrucción; -1: sin
 * memoria para la copia. Con el cerrojo general; no duerme. */
int espaco_cow_falta(espaco_t e, uint32_t virt)
{
    virt &= ~(PAGINA - 1);
    void *p = espaco_pte(e, virt);
    if (!p) return 0;
    pte_t v = ent_ler(p);
    if (!(v & PG_PRESENTE) || !(v & PG_USUARIO)) return 0;   /* revisión D401: una del kernel nunca es COW */
    if (v & PG_ESCRITA) return 1;            /* otra CPU ya la rompió: solo repetir */
    if (!(v & HW_COW)) return 0;
    fis_t velha = ent_end(v);
    if (cow_refs(velha) <= 1) {
        /* Ya es solo nuestra (el otro escribió antes, hizo exec o terminó): sin copiar. Dar W
         * no necesita avisar a las demás CPU (con la traducción vieja solo vuelven a fallar). */
        ent_trocar(p, PG_ESCRITA | PG_SUJA, HW_COW);
        if (espaco_atual() == e) invalidar(virt);
        return 1;
    }
    bool irq = int_salva_desliga();
    fis_t nova = memfisica_alocar_usuario();   /* puede sacar otras páginas a la swap, nunca esta */
    if (!nova) { int_restaura(irq); return -1; }
    v = ent_ler(p);
    fisica_copiar_pagina(nova, velha);
    uint32_t fl = ent_flags(v) & (PG_USUARIO | PG_NX | PG_PWT | PG_PCD | PG_PAT);
    ent_substituir(p, ent_valor(nova, fl | PG_PRESENTE | PG_ESCRITA | PG_ACESSADA | PG_SUJA));
    if (espaco_atual() == e) invalidar(virt);
    int_restaura(irq);
    /* Otros hilos del proceso en otras CPU pueden tener aún el marco viejo en su TLB. */
    espaco_sincronizar(e);
    memfisica_liberar(velha);   /* baja su cuenta (copia_escrita.c); la libera si era la última */
    return 1;
}

bool espaco_romper_cow(espaco_t e, uint32_t virt)
{
    return espaco_cow_falta(e, virt) > 0;
}

fis_t espaco_traduzir_escrita(espaco_t e, uint32_t virt)
{
    void *p = espaco_pte(e, virt);
    if (p && (ent_ler(p) & (PG_PRESENTE | HW_COW)) == (PG_PRESENTE | HW_COW) && espaco_cow_falta(e, virt) < 0)
        return 0;
    return espaco_traduzir(e, virt, NULL);
}

/* Clona la mitad del programa de "de" en "para" (un espacio recién creado): cada página propia
 * presente pasa a compartirse (la cuenta sube; las escribibles quedan de copia al escribir en
 * los dos), las que están en la swap se copian para el hijo (la del padre se queda allí) y las
 * prestadas (ventanas, BAR, bibliotecas compartidas) no se tocan: las de las bibliotecas las
 * clona imagens_clonar(). "copiar_ja" (o NULL) dice qué páginas se copian ya en vez de
 * compartirse (las palabras de un futex con alguien esperando). Quien llama sincroniza después
 * las TLB de las demás CPU que corran "de" (espaco_sincronizar). Con el cerrojo general. Si
 * falla, "para" queda a medias y quien llama lo destruye (las cuentas bajan solas). */
int espaco_clonar(espaco_t de, espaco_t para, bool (*copiar_ja)(fis_t))
{
    uint32_t n = 0;
    for (uint32_t i = PDE_KERNEL; i < PDE_USUARIO_FIM; i++) {
        pte_t pde = ent_ler(pde_de(de, i));
        if (!(pde & PG_PRESENTE) || grande(pde)) continue;
        for (uint32_t k = 0; k < n_tab; k++) {
            pte_t pte = ent_ler(pte_de(pde, k));
            if ((pte & PG_PRESENTE) && !(pte & PG_EMPRESTADA)) n++;
        }
    }
    if (!cow_reservar(n)) return ERRO_SEM_MEMORIA;
    bool atual = espaco_atual() == de;
    for (uint32_t i = PDE_KERNEL; i < PDE_USUARIO_FIM; i++) {
        pte_t pde = ent_ler(pde_de(de, i));
        if (!(pde & PG_PRESENTE) || grande(pde)) continue;
        for (uint32_t k = 0; k < n_tab; k++) {
            void *pp = pte_de(pde, k);
            pte_t pte = ent_ler(pp);
            uint32_t v = (i << d_desl) | (k << 12);
            if (troca_e_entrada(pte)) {
                fis_t c = troca_copiar(pte);   /* puede sacar otras: las que faltan se ven al llegar */
                uint32_t b = (uint32_t)pte;
                uint32_t fl = PG_USUARIO | ((b & TROCA_ESCRITA) ? PG_ESCRITA : 0) | ((b & TROCA_NX) ? PG_NX : 0);
                if (!c) return ERRO_SEM_MEMORIA;
                if (!espaco_mapear(para, v, c, fl | PG_ACESSADA)) { memfisica_liberar(c); return ERRO_SEM_MEMORIA; }
                continue;
            }
            if (!(pte & PG_PRESENTE) || (pte & PG_EMPRESTADA)) continue;
            fis_t f = ent_end(pte);
            uint32_t fl = ent_flags(pte) & (PG_USUARIO | PG_NX | PG_PWT | PG_PCD | PG_PAT);
            bool escrevivel = (pte & PG_ESCRITA) || (pte & HW_COW);
            if (escrevivel && copiar_ja && copiar_ja(f)) {
                fis_t c = memfisica_alocar_usuario();
                if (!c) return ERRO_SEM_MEMORIA;
                fisica_copiar_pagina(c, f);
                if (!espaco_mapear(para, v, c, fl | PG_ESCRITA | PG_ACESSADA)) { memfisica_liberar(c); return ERRO_SEM_MEMORIA; }
                continue;
            }
            if (!espaco_mapear(para, v, f, fl | PG_ACESSADA)) return ERRO_SEM_MEMORIA;
            cow_somar(f);   /* desde aquí, destruir "para" baja la cuenta en vez de liberarla */
            if (escrevivel) {
                if (pte & PG_ESCRITA) {
                    ent_trocar(pp, HW_COW, PG_ESCRITA);
                    if (atual) invalidar(v);
                }
                void *ph = espaco_pte(para, v);
                if (ph) ent_trocar(ph, HW_COW, 0);
            }
        }
    }
    return 0;
}

/* Páginas de "e" que comparte con otros espacios y que no se le cuentan: de cada una paga su
 * parte (1/referencias), como imagens_paginas_alheias() con las bibliotecas (D378). */
uint32_t espaco_paginas_cow_alheias(espaco_t e)
{
    if (!cow_paginas()) return 0;
    uint32_t mil = 0;   /* en milésimas de página */
    for (uint32_t i = PDE_KERNEL; i < PDE_USUARIO_FIM; i++) {
        pte_t pde = ent_ler(pde_de(e, i));
        if (!(pde & PG_PRESENTE) || grande(pde)) continue;
        for (uint32_t k = 0; k < n_tab; k++) {
            pte_t pte = ent_ler(pte_de(pde, k));
            if (!(pte & PG_PRESENTE) || (pte & PG_EMPRESTADA)) continue;
            uint32_t r = cow_refs(ent_end(pte));
            if (r > 1) mil += 1000u - 1000u / r;
        }
    }
    return mil / 1000u;
}

/* ------------------------------------------------------ la swap (D352) */

void *espaco_pte(espaco_t e, uint32_t virt)
{
    if (virt < USUARIO_INICIO || virt >= USUARIO_FIM) return NULL;
    pte_t pde = ent_ler(pde_de(e, virt >> d_desl));
    if (!(pde & PG_PRESENTE) || grande(pde)) return NULL;
    return pte_de(pde, (virt >> 12) & (n_tab - 1));
}

bool espaco_existe(espaco_t e, uint32_t virt)
{
    if (virt < USUARIO_INICIO) return espaco_traduzir(e, virt, NULL) != 0;
    void *p = espaco_pte(e, virt);
    if (!p) return false;
    pte_t v = ent_ler(p);
    return (v & PG_PRESENTE) || troca_e_entrada(v);
}

uint32_t espaco_paginas_troca(espaco_t e)
{
    uint32_t n = 0;
    for (uint32_t i = PDE_KERNEL; i < PDE_USUARIO_FIM; i++) {
        pte_t pde = ent_ler(pde_de(e, i));
        if (!(pde & PG_PRESENTE)) continue;
        for (uint32_t k = 0; k < n_tab; k++)
            if (troca_e_entrada(ent_ler(pte_de(pde, k)))) n++;
    }
    return n;
}

uint32_t paginacao_bloco(void)          { return BLOCO; }
pte_t    pte_ler(const void *p)         { return ent_ler(p); }
void     pte_gravar(void *p, pte_t v)   { ent_escrever(p, v); }
pte_t    pte_valor(fis_t fis, uint32_t flags) { return ent_valor(fis, flags); }
fis_t    pte_endereco(pte_t v)          { return ent_end(v); }
uint32_t pte_flags(pte_t v)             { return ent_flags(v); }
void     pte_mudar(void *p, uint32_t por, uint32_t tirar) { ent_trocar(p, por & 0xFFFu, tirar & 0xFFFu); }

/* La mitad baja (la que lleva P) se cambia a 0 con un xchg: el hardware pone A/D con
 * operaciones atómicas y solo en entradas presentes, así que lo que devuelve es lo último
 * que tuvo. La alta (PAE) se borra después: con P=0 ya nadie la mira. */
pte_t pte_esvaziar(void *p)
{
    volatile uint32_t *m = (volatile uint32_t *)p;
    uint32_t baixa = __atomic_exchange_n((uint32_t *)p, 0u, __ATOMIC_SEQ_CST);
    if (!PAE) return baixa;
    uint32_t alta = m[1];
    m[1] = 0;
    return (uint64_t)alta << 32 | baixa;
}

/* --------------------------------------------------------- las ventanas */

uint32_t janela_mapear(uint32_t fis, uint32_t bytes)
{
    return janela_mapear_tipo(fis, bytes, 0);
}

/* "tipo" son los bits de tipo de memoria de la PTE (PG_PWT, PG_PCD, PG_PAT).
 * La franja de ventanas usa siempre tablas de 4 KB, así que cada página tiene su
 * propio tipo y nunca hay página grande con tipos mezclados (D010, D057).
 * Nada de la franja se ejecuta: con NX, no ejecutable (D349). */
uint32_t janela_mapear_tipo(uint32_t fis, uint32_t bytes, uint32_t tipo)
{
    uint32_t desloc = fis & 0xFFF;
    uint32_t paginas = (desloc + bytes + PAGINA - 1) / PAGINA;
    if (janela_proxima + paginas * PAGINA > USUARIO_INICIO) return 0;
    uint32_t virt = janela_proxima;
    for (uint32_t i = 0; i < paginas; i++)
        espaco_mapear(dir_kernel, virt + i * PAGINA, (fis & ~(PAGINA - 1)) + i * PAGINA, PG_ESCRITA | PG_NX | tipo);
    janela_proxima += paginas * PAGINA;
    return virt + desloc;
}

/* D192: una franja de la ventana del kernel reservada sin mapear, para quien
 * mapea y desmapea páginas sueltas a menudo (uACPI: las tablas ACPI y las
 * regiones de memoria del AML). janela_mapear() solo avanza; aquí el dueño de
 * la franja pone y quita las páginas con janela_pagina() y janela_soltar().
 * Las páginas no son globales: el TLB de otra CPU las olvida al cambiar de
 * proceso, y solo la CPU 0 las toca (las tareas del kernel corren allí). */
uint32_t janela_reservar(uint32_t bytes)
{
    uint32_t paginas = (bytes + PAGINA - 1) / PAGINA;
    if (!paginas || janela_proxima + paginas * PAGINA > USUARIO_INICIO) return 0;
    uint32_t virt = janela_proxima;
    janela_proxima += paginas * PAGINA;
    return virt;
}

void janela_pagina(uint32_t virt, fis_t fis, uint32_t tipo)
{
    if (virt < KERNEL_JANELAS || virt >= USUARIO_INICIO) return;
    espaco_mapear(dir_kernel, virt & ~(PAGINA - 1), fis & ~(fis_t)(PAGINA - 1), PG_ESCRITA | PG_NX | tipo);
}

void *janela_entrada(uint32_t virt)
{
    if (virt < KERNEL_JANELAS || virt >= USUARIO_INICIO) return NULL;
    pte_t pde = ent_ler(pde_de(dir_kernel, virt >> d_desl));
    if (!(pde & PG_PRESENTE) || grande(pde)) return NULL;
    return pte_de(pde, (virt >> 12) & (n_tab - 1));
}

void janela_soltar(uint32_t virt)
{
    if (virt < KERNEL_JANELAS || virt >= USUARIO_INICIO) return;
    pte_t pde = ent_ler(pde_de(dir_kernel, virt >> d_desl));
    if (!(pde & PG_PRESENTE) || grande(pde)) return;
    ent_escrever(pte_de(pde, (virt >> 12) & (n_tab - 1)), 0);
    invalidar(virt & ~(PAGINA - 1));   /* la tabla es de todos los espacios: siempre */
}
