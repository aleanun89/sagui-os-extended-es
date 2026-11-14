// SPDX-License-Identifier: GPL-3.0-only
/* paginacao.c - paginación de 2 niveles. Ver D010 y D049.
 *
 * Directorio (1024 entradas de 4 MB) -> tabla (1024 entradas de 4 KB) ->
 * página. Las 512 primeras entradas del directorio (0 a 2 GB) son del kernel y
 * son las mismas en todos los procesos. Las siguientes son del programa de cada
 * proceso.
 *
 * El kernel ve la RAM por la propia dirección (1:1) sólo hasta el fin real de la RAM
 * (redondeado a 4 MB): lo que la BIOS pone justo encima (MMIO) queda sin mapa.
 * Si la CPU tiene PSE, ese mapa usa páginas de 4 MB directo en el directorio (bit
 * PS), sin tablas; con PGE quedan globales (no salen del TLB al cambiar de
 * proceso). Los primeros 4 MB usan siempre una tabla de 4 KB, para que la página
 * 0 quede ausente (puntero nulo), y la franja de ventanas también, porque
 * janela_mapear() mapea página a página. */
#include "paginacao.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "texto.h"
#include "cpu_dados.h"
#include "ioapic.h"

#define ENTRADAS      1024
#define PDE_KERNEL    (USUARIO_INICIO >> 22)      /* 512 */
#define PDE_USUARIO_FIM (USUARIO_FIM >> 22)       /* 1023 */
#define MASCARA_END   0xFFFFF000u
#define MASCARA_4MB   0xFFC00000u
#define PG_4MB        0x080   /* PDE: página de 4 MB (PS) */
#define PG_GLOBAL     0x100   /* queda en el TLB al cambiar el CR3 (con CR4.PGE) */
#define CR4_PSE       0x10
#define CR4_PGE       0x80

static espaco_t dir_kernel;
static uint32_t janela_proxima = KERNEL_JANELAS;
static uint32_t fim_mapa;              /* fin del mapa 1:1 del kernel */
static bool     com_pse, com_pge;

static inline bool grande(uint32_t pde) { return (pde & (PG_PRESENTE | PG_4MB)) == (PG_PRESENTE | PG_4MB); }
/* Sólo para entradas que apuntan a tabla (nunca una de 4 MB). */
static inline uint32_t *tabela_de(uint32_t entrada) { return (uint32_t *)(entrada & MASCARA_END); }

static inline void invalidar(uint32_t virt)
{
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

static uint32_t pagina_zerada(void)
{
    uint32_t p = memfisica_alocar();
    if (p) memset((void *)p, 0, PAGINA);
    return p;
}

/* CPUID 1, EDX: bit 3 = PSE, bit 13 = PGE. Toda CPU con CPUID que corre
 * SaguiOS (i686) tiene los dos; sin CPUID, todo queda en tablas de 4 KB. */
static void conferir_cpu(void)
{
    uint32_t antes, depois;
    __asm__ volatile("pushf; pop %0; mov %0, %1; xor $0x200000, %1; push %1; popf; pushf; pop %1; push %0; popf"
                     : "=&r"(antes), "=&r"(depois));
    if (!((antes ^ depois) & 0x200000)) return;
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0), "c"(0));
    if (a < 1) return;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    com_pse = (d >> 3) & 1;
    com_pge = (d >> 13) & 1;
}

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

/* Una página de 4 MB sólo cubre RAM "libre" entera del E820: un bloque que
 * toca región reservada (o el primer MB, con las franjas de tipos de memoria
 * fijos del MTRR) puede tener tipos de memoria mezclados, y una página grande
 * en esas condiciones es indefinida en el manual de Intel (D010). Esos bloques
 * quedan con tabla de 4 KB. */
static bool bloco_so_ram(uint32_t base)
{
    uint64_t ini = base, fim = (uint64_t)base + 0x400000;
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

void paginacao_iniciar(void)
{
    conferir_cpu();
    fim_mapa = calcular_fim_mapa();
    uint32_t global = com_pge ? PG_GLOBAL : 0;
    dir_kernel = pagina_zerada();
    uint32_t *dir = (uint32_t *)dir_kernel;
    /* Debajo de fim_mapa: 1:1 (4 MB por entrada con PSE). En la franja de
     * ventanas: tablas vacías (pero existentes), para que todo proceso las
     * herede desde ya. Entre las dos: nada. */
    uint32_t grandes = 0, tabelas = 0;
    for (uint32_t i = 0; i < PDE_KERNEL; i++) {
        uint32_t base = i << 22;
        if (base < fim_mapa && com_pse && i > 0 && bloco_so_ram(base)) {
            dir[i] = base | PG_4MB | global | PG_PRESENTE | PG_ESCRITA;
            grandes++;
        } else if (base < fim_mapa) {
            tabelas++;
            uint32_t *tab = (uint32_t *)tabela_nova();
            for (uint32_t k = 0; k < ENTRADAS; k++)
                tab[k] = (base + (k << 12)) | global | PG_PRESENTE | PG_ESCRITA;
            dir[i] = (uint32_t)tab | PG_PRESENTE | PG_ESCRITA;
        } else if (base >= KERNEL_JANELAS) {
            dir[i] = tabela_nova() | PG_PRESENTE | PG_ESCRITA;
        }
    }
    tabela_de(dir[0])[0] = 0;   /* página 0 ausente: puntero nulo genera falta */

    /* El APIC local (D157): está en la memoria del sistema, fuera de la
     * RAM, así que necesita un mapa propio. Se mapea al final del espacio
     * del programa (0xFFC00000), donde ningún programa llega, y el mapa se
     * copia para todos los espacios: el kernel toca el APIC incluso
     * cuando corre en el contexto de un programa (el planificador, por
     * ejemplo). */
    {
        uint32_t *tab = (uint32_t *)tabela_nova();
        tab[0] = (cpu_lapic_fisico() & MASCARA_END) | PG_PRESENTE | PG_ESCRITA;
        /* Los IOAPIC (D166), en las páginas siguientes y sin caché. Mapear
         * no toca el hardware: sin --ioapic nadie los lee. */
        for (uint32_t i = 0; i < ioapic_quantidade() && i < IOAPIC_MAX; i++)
            tab[1 + i] = (ioapic_fisico(i) & MASCARA_END) | PG_PCD | PG_PWT | PG_PRESENTE | PG_ESCRITA;
        dir[1023] = (uint32_t)tab | PG_PRESENTE | PG_ESCRITA;
    }

    if (com_pse)
        __asm__ volatile("mov %%cr4, %%eax; or %0, %%eax; mov %%eax, %%cr4" : : "i"(CR4_PSE) : "eax", "memory");
    espaco_ativar(dir_kernel);
    /* PG enciende la paginación; WP hace que el kernel también respete páginas sólo de
     * lectura; EM hace que cualquier instrucción de punto flotante genere excepción
     * hasta que fpu_iniciar() compruebe la CPU y encienda FPU/MMX/SSE (D032). */
    uint32_t cr0 = le_cr0();
    cr0 |= 0x80000000u | 0x00010000u | 0x4u;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");
    if (com_pge)
        __asm__ volatile("mov %%cr4, %%eax; or %0, %%eax; mov %%eax, %%cr4" : : "i"(CR4_PGE) : "eax", "memory");
    klog("[SAGUI] paginacao: 1:1 ate %x (%u MB), %s, %u paginas de 4 MB e %u tabelas, PGE %s\n",
         fim_mapa, fim_mapa >> 20, com_pse ? "paginas de 4 MB" : "tabelas de 4 KB", grandes, tabelas,
         com_pge ? "sim" : "nao");
}

uint32_t paginacao_fim_mapa(void) { return fim_mapa; }
bool     paginacao_pse(void)      { return com_pse; }

espaco_t espaco_kernel(void) { return dir_kernel; }

espaco_t espaco_criar(void)
{
    espaco_t e = pagina_zerada();
    if (!e) return 0;
    /* Se copia el directorio entero del kernel: las 512 primeras entradas son
     * el mapa 1:1 y las ventanas, y la última (1023) es el APIC local (D157).
     * Las entradas del programa (512 a 1022) están a cero. */
    memcpy((void *)e, (void *)dir_kernel, 1024 * sizeof(uint32_t));
    return e;
}

void espaco_destruir(espaco_t e)
{
    if (!e || e == dir_kernel) return;
    if (espaco_atual() == e)
        espaco_ativar(dir_kernel);   /* nunca borrar el directorio en uso */
    uint32_t *dir = (uint32_t *)e;
    for (uint32_t i = PDE_KERNEL; i < PDE_USUARIO_FIM; i++) {
        if (!(dir[i] & PG_PRESENTE)) continue;
        uint32_t *tab = tabela_de(dir[i]);
        for (uint32_t k = 0; k < ENTRADAS; k++)
            if ((tab[k] & PG_PRESENTE) && !(tab[k] & PG_EMPRESTADA))   /* las prestadas tienen otro dueño */
                memfisica_liberar(tab[k] & MASCARA_END);
        memfisica_liberar((uint32_t)tab);
    }
    memfisica_liberar(e);
}

void espaco_ativar(espaco_t e)
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(e) : "memory");
}

espaco_t espaco_atual(void) { return le_cr3(); }

bool espaco_mapear(espaco_t e, uint32_t virt, uint32_t fis, uint32_t flags)
{
    uint32_t *dir = (uint32_t *)e;
    uint32_t i = virt >> 22, k = (virt >> 12) & 0x3FF;
    if (grande(dir[i])) return false;   /* mapa 1:1 del kernel en 4 MB: no se toca */
    if (!(dir[i] & PG_PRESENTE)) {
        if (i < PDE_KERNEL) return false;   /* las tablas del kernel ya existen todas */
        uint32_t t = pagina_zerada();
        if (!t) return false;
        dir[i] = t | PG_PRESENTE | PG_ESCRITA | PG_USUARIO;
    }
    tabela_de(dir[i])[k] = (fis & MASCARA_END) | flags | PG_PRESENTE;
    if (espaco_atual() == e || i < PDE_KERNEL)
        invalidar(virt);
    return true;
}

uint32_t espaco_traduzir(espaco_t e, uint32_t virt, uint32_t *flags)
{
    uint32_t *dir = (uint32_t *)e;
    uint32_t pde = dir[virt >> 22];
    if (!(pde & PG_PRESENTE)) return 0;
    if (grande(pde)) {
        if (flags) *flags = pde & 0xFFF & ~PG_4MB;
        return (pde & MASCARA_4MB) | (virt & ~MASCARA_4MB);
    }
    uint32_t pte = tabela_de(pde)[(virt >> 12) & 0x3FF];
    if (!(pte & PG_PRESENTE)) return 0;
    if (flags) *flags = pte & pde & 0xFFF;   /* permiso efectivo: las dos tienen que permitir */
    return (pte & MASCARA_END) | (virt & 0xFFF);
}

bool espaco_alocar(espaco_t e, uint32_t virt, uint32_t bytes, uint32_t flags)
{
    uint32_t ini = virt & MASCARA_END, fim = (virt + bytes + PAGINA - 1) & MASCARA_END;
    if (ini < USUARIO_INICIO || fim > USUARIO_FIM || fim < ini) return false;
    for (uint32_t v = ini; v < fim; v += PAGINA) {
        if (espaco_traduzir(e, v, NULL)) continue;   /* ya existe: mantiene */
        uint32_t p = pagina_zerada();
        if (!p || !espaco_mapear(e, v, p, flags)) {
            if (p) memfisica_liberar(p);
            return false;
        }
    }
    return true;
}

void espaco_desmapear(espaco_t e, uint32_t virt)
{
    uint32_t *dir = (uint32_t *)e;
    uint32_t pde = dir[virt >> 22];
    if (!(pde & PG_PRESENTE) || grande(pde)) return;
    tabela_de(pde)[(virt >> 12) & 0x3FF] = 0;
    if (espaco_atual() == e) invalidar(virt);
}

uint32_t espaco_tirar(espaco_t e, uint32_t virt)
{
    uint32_t *dir = (uint32_t *)e;
    uint32_t pde = dir[virt >> 22];
    if (!(pde & PG_PRESENTE) || grande(pde) || (virt >> 22) < PDE_KERNEL) return 0;
    uint32_t *pte = &tabela_de(pde)[(virt >> 12) & 0x3FF];
    /* La marca de prestada solo está en la PTE (espaco_traduzir la pierde al
     * combinarla con la PDE): por eso se mira aquí. */
    if (!(*pte & PG_PRESENTE) || (*pte & PG_EMPRESTADA)) return 0;
    uint32_t fis = *pte & MASCARA_END;
    *pte = 0;
    if (espaco_atual() == e) invalidar(virt);
    return fis;
}

uint32_t espaco_paginas_usuario(espaco_t e)
{
    uint32_t *dir = (uint32_t *)e, n = 0;
    for (uint32_t i = PDE_KERNEL; i < PDE_USUARIO_FIM; i++) {
        if (!(dir[i] & PG_PRESENTE)) continue;
        uint32_t *tab = tabela_de(dir[i]);
        for (uint32_t k = 0; k < ENTRADAS; k++)
            if (tab[k] & PG_PRESENTE) n++;
    }
    return n;
}

uint32_t janela_mapear(uint32_t fis, uint32_t bytes)
{
    return janela_mapear_tipo(fis, bytes, 0);
}

/* "tipo" son los bits de tipo de memoria de la PTE (PG_PWT, PG_PCD, PG_PAT).
 * La franja de ventanas usa siempre tablas de 4 KB, así que cada página tiene su
 * propio tipo y nunca hay página grande con tipos mezclados (D010, D057). */
uint32_t janela_mapear_tipo(uint32_t fis, uint32_t bytes, uint32_t tipo)
{
    uint32_t desloc = fis & 0xFFF;
    uint32_t paginas = (desloc + bytes + PAGINA - 1) / PAGINA;
    if (janela_proxima + paginas * PAGINA > USUARIO_INICIO) return 0;
    uint32_t virt = janela_proxima;
    for (uint32_t i = 0; i < paginas; i++)
        espaco_mapear(dir_kernel, virt + i * PAGINA, (fis & MASCARA_END) + i * PAGINA, PG_ESCRITA | tipo);
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

void janela_pagina(uint32_t virt, uint32_t fis, uint32_t tipo)
{
    if (virt < KERNEL_JANELAS || virt >= USUARIO_INICIO) return;
    espaco_mapear(dir_kernel, virt & MASCARA_END, fis & MASCARA_END, PG_ESCRITA | tipo);
}

void janela_soltar(uint32_t virt)
{
    if (virt < KERNEL_JANELAS || virt >= USUARIO_INICIO) return;
    uint32_t *dir = (uint32_t *)dir_kernel;
    uint32_t pde = dir[virt >> 22];
    if (!(pde & PG_PRESENTE) || grande(pde)) return;
    tabela_de(pde)[(virt >> 12) & 0x3FF] = 0;
    invalidar(virt & MASCARA_END);   /* la tabla es de todos los espacios: siempre */
}
