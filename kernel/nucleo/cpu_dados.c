// SPDX-License-Identifier: GPL-2.0-or-later
/* cpu_dados.c - la zona de datos por CPU (D150, D157).
 *
 * Tres cosas:
 *   1. la tabla de datos por CPU (una entrada por procesador, cada una en su
 *      línea de caché) y la pila del kernel de cada uno;
 *   2. saber qué CPU ejecuta este código: con el segmento GS de cada CPU,
 *      que apunta para a su propia entrada de la tabla;
 *   3. leer la MADT del ACPI (la tabla "APIC"), donde el firmware enumera
 *      los procesadores y sus identificadores de APIC local.
 *
 * La tabla se escribe una vez (por el processador principal) y cada
 * processor escribe só o seu próprio campo: por eso los campos que mudam
 * (o estado, o processo actual) son atómicos ou estão protegidos por
 * cerrojo. */
#include "cpu_dados.h"
#include "processo.h"
#include "abi.h"
#include "info_hardware.h"
#include "lapic.h"
#include "texto.h"
#include <stddef.h>

/* La pila del kernel y la zona de datos tienen que ser igual que las que
 * usa el planificador; si no, el tamaño de la pila cambiaría por el
 * camino y nadie se enteraría hasta que arrancara un núcleo nuevo. */
_Static_assert(sizeof(cpu_dados_t) == CPU_LINHA_CACHE, "los datos de cada CPU deben ocupar una línea de caché");
_Static_assert(CPU_PILHA_KERNEL == PILHA_KERNEL, "la pila por CPU debe ser la misma que la de un proceso");
_Static_assert(offsetof(cpu_dados_t, indice) == CPU_OFF_INDICE, "el índice se lee por GS en este desplazamiento");
_Static_assert(offsetof(cpu_dados_t, atual) == CPU_OFF_ATUAL, "el proceso actual se lee por GS en este desplazamiento");

/* La tabla: alineada a línea de caché y con una entrada por CPU. */
static cpu_dados_t dados[CPU_MAX] __attribute__((aligned(CPU_LINHA_CACHE)));

/* Una pila de kernel por CPU. */
static uint8_t pilhas[CPU_MAX][CPU_PILHA_KERNEL] __attribute__((aligned(CPU_LINHA_CACHE)));

/* Cuántos procesadores hay, según la MADT. 0 = todavía no se ha mirado. */
static uint32_t numero_cpus;

/* Los identificadores de los APIC locales que declaró el firmware, leídos
 * de la MADT. Se leen antes de la paginación (las tablas del ACPI pueden
 * estar fora do mapa 1:1 do kernel) e ficam guardado aqui. */
static uint32_t ids_capturados[CPU_MAX];
static uint32_t n_ids;
/* A base física do APIC local, que vem no cabeçalho da MADT. */
static uint32_t base_lapic = 0xFEE00000u;

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

/* Os deslocamentos onde podem estar as entradas da MADT: o cabeçalho tem 52
 * bytes na revisão 2 ou maior (é o que tem o endereço do APIC local e as
 * flags) e 44 bytes na revisão 1. Como nem toda a BIOS acerta a conta,
 * provam-se todos e fica o primeiro que dá processadores. */
static const uint32_t deslocamentos_madt[] = { 52, 44, 48, 36 };

/* Percorre a MADT (a tabela "APIC" do ACPI) e escreve os identificadores
 * dos APIC locais que o firmware declara, por ordem. Cada entrada de tipo 0
 * (Local APIC) ou de tipo 9 (x2APIC) é um processador; o bit 0 das suas
 * bandeiras diz se está ligado. Tem de ser chamada antes da paginação: a
 * tabela está na memória física e o kernel vê-a pelo seu próprio
 * endereço (mapa 1:1), mas só até ao fim da RAM. */
void cpu_lapic_capturar(void)
{
    n_ids = 0;
    const info_acpi_t *a = hw_acpi();
    if (!a || !a->achou) return;
    for (uint32_t i = 0; i < a->qtd; i++) {
        if (memcmp(a->tabela[i].assinatura, "APIC", 4) != 0) continue;
        const uint8_t *t = (const uint8_t *)a->tabela[i].endereco;
        uint32_t tam = a->tabela[i].tamanho;
        if (!t || tam < 44 || tam > (1u << 20)) return;
        /* O endereço do APIC local está no cabeçalho (offset 36, 8 bytes),
         * mas só nas revisões 2 ou maiores. */
        if (tam >= 44) {
            uint32_t base = le32(t + 36);
            if (base) base_lapic = base;
        }
        for (uint32_t c = 0; c < sizeof deslocamentos_madt / sizeof *deslocamentos_madt; c++) {
            uint32_t off = deslocamentos_madt[c];
            if (off + 2 > tam) continue;
            uint32_t n = 0;
            while (off + 2 <= tam) {
                uint8_t tipo = t[off];
                uint8_t len = t[off + 1];
                if (len < 2 || len > 32 || off + len > tam) break;
                if (tipo == 0 && len >= 8) {
                    if ((t[off + 4] & 1) && n < CPU_MAX)   /* Local APIC ligado */
                        ids_capturados[n++] = t[off + 3];
                } else if (tipo == 9 && len >= 12) {
                    if ((le32(t + off + 8) & 1) && n < CPU_MAX)   /* x2APIC ligado */
                        ids_capturados[n++] = le32(t + off + 4);
                }
                off += len;
            }
            if (n >= 1) { n_ids = n; return; }
        }
    }
}

/* O GS só está pronto depois de a GDT estar montada (gdt_iniciar). Antes
 * disso o kernel ainda não tem processos: usa-se o valor global. */
static volatile uint32_t gs_pronto;

void cpu_gs_ligar(void) { gs_pronto = 1; }
bool cpu_gs_lista(void) { return gs_pronto != 0; }

/* A base física do APIC local (a da MADT, ou o padrão se não houver). */
uint32_t cpu_lapic_fisico(void) { return base_lapic; }

/* Os identificadores dos APIC locais, dos que o firmware declarou. */
uint32_t cpu_lapic_ids(uint32_t *destino, uint32_t maximo)
{
    if (!destino || maximo == 0) return 0;
    uint32_t n = n_ids < maximo ? n_ids : maximo;
    for (uint32_t i = 0; i < n; i++)
        destino[i] = ids_capturados[i];
    return n;
}

/* Cuántos procesadores hay, según la MADT. 0 = todavía no se ha mirado. */
static uint32_t contar_madt(void)
{
    uint32_t ids[CPU_MAX];
    uint32_t n = cpu_lapic_ids(ids, CPU_MAX);
    if (n < 1) return 1;   /* sin ACPI o sin MADT: un solo procesador */
    /* A MADT pode nao listar o processador principal (no QEMU so lista os
     * secundarios): entao ele conta tambem (D157). */
    /* El del principal: con el cerrojo general (D165) esto puede correr en
     * un secundario, y entonces el APIC local que se lee es el suyo. */
    uint32_t meu = cpu_atual() == 0 ? lapic_id() : cpu_dados(0)->apic_id;
    bool estou = false;
    for (uint32_t i = 0; i < n; i++)
        if (ids[i] == meu) estou = true;
    if (!estou && n < CPU_MAX) n++;
    return (n > CPU_MAX) ? CPU_MAX : n;
}

uint32_t cpu_atual(void)
{
    /* Antes de que la GDT este montada no hay GS: es el principal. */
    if (!gs_pronto) return 0;
    return cpu_indice_gs();
}

void cpu_definir_indice(uint32_t indice)
{
    if (indice >= CPU_MAX) return;
    dados[indice].indice = indice;
}

cpu_dados_t *cpu_dados(uint32_t cpu)
{
    if (cpu >= CPU_MAX) return NULL;
    return &dados[cpu];
}

uint8_t *cpu_pilha(uint32_t cpu)
{
    if (cpu >= CPU_MAX) return NULL;
    return pilhas[cpu];
}

uint32_t cpu_pilha_topo(uint32_t cpu)
{
    if (cpu >= CPU_MAX) return 0;
    return (uint32_t)&pilhas[cpu][CPU_PILHA_KERNEL];
}

uint32_t cpu_numero(void)
{
    if (numero_cpus == 0) numero_cpus = contar_madt();
    return numero_cpus;
}

/* ------------------------------------------------------------ topología (D174)
 *
 * El identificador de APIC de cada CPU lógica se parte en campos: los bits
 * bajos dicen qué hilo del núcleo es (SMT), los siguientes qué núcleo del
 * paquete y los altos qué paquete. CPUID dice cuántos bits ocupa cada campo:
 *   - la hoja 0xB (Intel desde Nehalem, AMD desde Zen 2, y el QEMU si su
 *     nivel llega): por niveles, con el x2APIC de la propia CPU;
 *   - en AMD, la 0x8000001E (topoext): hilos por núcleo;
 *   - en Intel antiguo, la hoja 1 (lógicas por paquete) y la 4 (núcleos);
 *   - sin nada de eso, cada CPU lógica es un núcleo (lo que ya se suponía).
 * Lo lee cada CPU sobre sí misma: CPUID responde por la CPU que lo ejecuta. */

static inline void cpuid_ler(uint32_t f, uint32_t s, uint32_t r[4])
{
    __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(f), "c"(s));
}

static struct {
    uint32_t apic;        /* el identificador de APIC según CPUID */
    uint32_t chave;       /* apic sin los bits de hilo: igual en los hilos hermanos */
    uint32_t pacote;      /* apic sin los bits de hilo ni de núcleo */
    uint32_t hilo;        /* los bits de hilo */
    uint32_t lida;
} topo[CPU_MAX];
static const char *metodo_topo = "sin leer";

static uint32_t log2_teto(uint32_t n)
{
    uint32_t s = 0;
    while (s < 31 && (1u << s) < n) s++;
    return s;
}

void cpu_topologia_ler(uint32_t cpu)
{
    if (cpu >= CPU_MAX) return;
    uint32_t r[4];
    cpuid_ler(0, 0, r);
    uint32_t max = r[0];
    bool intel = r[1] == 0x756E6547u && r[3] == 0x49656E69u && r[2] == 0x6C65746Eu;   /* GenuineIntel */
    bool amd = r[1] == 0x68747541u && r[3] == 0x69746E65u && r[2] == 0x444D4163u;     /* AuthenticAMD */
    uint32_t maxx = 0;
    cpuid_ler(0x80000000u, 0, r);
    if (r[0] >= 0x80000000u && r[0] < 0x80000100u) maxx = r[0];
    cpuid_ler(1, 0, r);
    uint32_t apic = r[1] >> 24;
    bool htt = (r[3] & (1u << 28)) != 0;
    uint32_t logicos = htt ? ((r[1] >> 16) & 0xFF) : 1;
    if (logicos == 0) logicos = 1;
    uint32_t smt = 0, pac = log2_teto(logicos);
    const char *met = NULL;

    if (max >= 0xB) {
        cpuid_ler(0xB, 0, r);
        if ((r[1] & 0xFFFF) != 0) {
            apic = r[3];   /* el x2APIC de esta CPU */
            uint32_t nivel_pac = 0;
            for (uint32_t sub = 0; sub < 8; sub++) {
                cpuid_ler(0xB, sub, r);
                uint32_t tipo = (r[2] >> 8) & 0xFF;
                if (tipo == 0) break;
                if (tipo == 1) smt = r[0] & 0x1F;
                nivel_pac = r[0] & 0x1F;   /* el último nivel abarca el paquete */
            }
            pac = nivel_pac > smt ? nivel_pac : smt;
            met = "CPUID 0xB";
        }
    }
    if (!met && amd && maxx >= 0x8000001Eu) {
        cpuid_ler(0x80000001u, 0, r);
        if (r[2] & (1u << 22)) {   /* topoext */
            cpuid_ler(0x8000001Eu, 0, r);
            uint32_t hilos = ((r[1] >> 8) & 0xFF) + 1;
            smt = log2_teto(hilos);
            if (pac < smt) pac = smt;
            met = "CPUID 0x8000001E";
        }
    }
    if (!met && intel && htt && max >= 4) {
        cpuid_ler(4, 0, r);
        if (r[0] & 0x1F) {   /* hay descriptor de caché: el campo de núcleos vale */
            uint32_t nucleos = ((r[0] >> 26) & 0x3F) + 1;
            uint32_t hilos = logicos / nucleos;
            smt = log2_teto(hilos ? hilos : 1);
            met = "CPUID 1 y 4";
        }
    }
    if (!met) { smt = 0; met = "sin datos de hilos: cada CPU es un nucleo"; }
    if (pac < smt) pac = smt;
    topo[cpu].apic = apic;
    topo[cpu].chave = apic >> smt;
    topo[cpu].pacote = pac >= 32 ? 0 : apic >> pac;
    topo[cpu].hilo = apic & ((1u << smt) - 1);
    topo[cpu].lida = 1;
    dados[cpu].nucleo = topo[cpu].chave;
    if (cpu == 0) metodo_topo = met;
}

const char *cpu_topologia_metodo(void) { return metodo_topo; }

bool cpu_irmas(uint32_t a, uint32_t b)
{
    if (a == b || a >= CPU_MAX || b >= CPU_MAX || !topo[a].lida || !topo[b].lida) return false;
    return topo[a].chave == topo[b].chave;
}

/* ¿Es "cpu" la primera CPU (por número) de su núcleo físico? */
static bool primeira_do_nucleo(uint32_t cpu)
{
    for (uint32_t c = 0; c < cpu; c++)
        if (cpu_irmas(c, cpu)) return false;
    return true;
}

uint32_t cpu_nucleo_fisico(uint32_t cpu)
{
    if (cpu >= CPU_MAX) return 0;
    if (!topo[cpu].lida) return cpu;
    /* Número sin huecos: cuántos núcleos distintos aparecen antes que el suyo. */
    uint32_t primeira = cpu;
    for (uint32_t c = 0; c < cpu; c++)
        if (cpu_irmas(c, cpu)) { primeira = c; break; }
    uint32_t n = 0;
    for (uint32_t c = 0; c < primeira; c++)
        if (topo[c].lida && primeira_do_nucleo(c)) n++;
    return n;
}

uint32_t cpu_pacote(uint32_t cpu)
{
    if (cpu >= CPU_MAX || !topo[cpu].lida) return 0;
    /* Igual que el núcleo: numerado desde 0 por orden de aparición. */
    uint32_t n = 0;
    for (uint32_t c = 0; c < cpu; c++) {
        if (!topo[c].lida) continue;
        if (topo[c].pacote == topo[cpu].pacote) return cpu_pacote(c);
        bool primeiro = true;
        for (uint32_t k = 0; k < c; k++)
            if (topo[k].lida && topo[k].pacote == topo[c].pacote) { primeiro = false; break; }
        if (primeiro) n++;
    }
    return n;
}

uint32_t cpu_hilo_do_nucleo(uint32_t cpu)
{
    if (cpu >= CPU_MAX || !topo[cpu].lida) return 0;
    uint32_t n = 0;
    for (uint32_t c = 0; c < cpu; c++)
        if (cpu_irmas(c, cpu)) n++;
    return n;
}
