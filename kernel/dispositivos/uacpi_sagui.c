// SPDX-License-Identifier: GPL-2.0-or-later
/* uacpi_sagui.c - la capa del kernel para uACPI y la tarea "acpi-aml" (D192).
 *
 * uACPI (kernel/terceros/uacpi/, MIT, sin cambios) es un intérprete de AML:
 * ejecuta los métodos de la DSDT y las SSDT. SaguiOS lo usa para la batería
 * (bateria.c) y el controlador embebido (ec.c); el apagado y el botón de
 * encendido siguen siendo de acpi.c (D113), que no cambia.
 *
 * Reglas para no estropear nada (D192):
 *   - todo pasa en la tarea del kernel "acpi-aml", después del arranque: un
 *     AML lento o colgado no retrasa el escritorio, y la tarea se puede
 *     expulsar como cualquier otra. Es la única que llama a uACPI;
 *   - es opcional y está desactivado por defecto: solo arranca si la imagen
 *     se creó con --uacpi (OPC_BOOT_UACPI), sin --sem-acpi, y si acpi.c dejó
 *     el sistema en modo ACPI (SCI_EN = 1). Sin --uacpi no se ejecuta nada de
 *     AML ni se crea la tarea: el arranque es el de siempre. uACPI nunca
 *     cambia el modo (UACPI_FLAG_NO_ACPI_MODE) ni apaga la máquina;
 *   - si uACPI no puede cargar, se dice en el registro y en INFO_BATERIA, el
 *     botón se vuelve a armar y el sistema sigue sin él;
 *   - la memoria: el AML no puede mapear la RAM libre del sistema (E820 tipo
 *     1 por encima de 1 MB), que es de los programas; lo de debajo del mapa
 *     1:1 se ve por su dirección y lo de arriba por una franja de ventanas
 *     propia (8 MB), sin caché si no es RAM ni tablas;
 *   - la pila: uACPI corre en una pila propia de 64 KB (la de una tarea del
 *     kernel es de 8 KB) con una página de guarda que se comprueba.
 *
 * Los cerrojos: el kernel tiene un cerrojo general (D165) y las tareas del
 * kernel corren solo en la CPU 0, así que dentro del kernel solo hay una CPU
 * a la vez. Los "spinlocks" de uACPI bastan con desactivar las
 * interrupciones; los semáforos (sus mutex y eventos) duermen la tarea con
 * bloquear()/acordar() y un tiempo máximo. */
#include "uacpi_sagui.h"
#include <uacpi/kernel_api.h>
#include <uacpi/internal/opregion.h>   /* D405: solo para encontrar el manejador de serie de SystemMemory */
#include <uacpi/internal/io.h>
#include <uacpi/sleep.h>
#include "acpi.h"
#include "acoes.h"
#include "bateria.h"
#include "gpu_energia.h"
#include "gui.h"
#include "portatil.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "gui.h"
#include "info_hardware.h"
#include "irq_rotas.h"
#include "memoria.h"
#include "paginacao.h"
#include "portas.h"
#include "processo.h"
#include "relogio.h"
#include "texto.h"

#define JANELA_PAGINAS   2048                 /* 8 MB de ventanas para las tablas y las regiones */
#define PILHA_BYTES      (64u * 1024u)
#define GUARDA_BYTES     PAGINA               /* debajo de la pila: se llena y se comprueba */
#define GUARDA_PADRAO    0x5A6B7C8Du
#define FILA_TRABALHO    32
#define PERIODO_MS       1000
#define PERIODO_EC_MS    200    /* D403: con EC, sus eventos (teclas Fn) se miran 5 veces por segundo */
#define ESPERA_ARRANQUE_MS 1000

static const char canal_tarefa = 0;

/* ------------------------------------------------------------ registro */

void uacpi_kernel_log(uacpi_log_level nivel, const uacpi_char *texto)
{
    static const char *const nomes[] = { "?", "error", "aviso", "info", "traza", "depura" };
    klog("[UACPI] %s: %s", nivel <= UACPI_LOG_DEBUG ? nomes[nivel] : "?", texto);
}

/* ------------------------------------------------------------ memoria */

void *uacpi_kernel_alloc(uacpi_size n) { return kmalloc(n ? n : 1); }
void uacpi_kernel_free(void *p) { if (p) kfree(p); }

/* ------------------------------------------------------------ mapas */

static uint32_t janela_base;
static uint8_t janela_usada[JANELA_PAGINAS / 8];
static uint32_t janela_em_uso, janela_maximo;
static bool avisou_ram;

static bool bit_usado(uint32_t i) { return janela_usada[i >> 3] & (1u << (i & 7)); }
static void bit_por(uint32_t i, bool v)
{
    if (v) janela_usada[i >> 3] |= (uint8_t)(1u << (i & 7));
    else janela_usada[i >> 3] &= (uint8_t)~(1u << (i & 7));
}

/* ¿Toca [ini, fim) la RAM libre del E820 por encima de 1 MB? Es la de los
 * programas y el kernel: el AML no tiene nada que hacer ahí. */
static bool toca_ram_livre(uint64_t ini, uint64_t fim)
{
    for (uint32_t i = 0; i < info_boot.mapa_qtd; i++) {
        const regiao_e820_t *r = &mapa_memoria[i];
        if (!(r->extra & 1) || r->tipo != E820_LIVRE) continue;
        uint64_t rini = r->base, rfim = r->base + r->tamanho;
        if (rfim <= 0x100000) continue;
        if (rini < 0x100000) rini = 0x100000;
        if (ini < rfim && fim > rini) return true;
    }
    return false;
}

/* ¿Está [ini, fim) entero en RAM, tablas ACPI o NVS (E820 1, 3 y 4)? Solo
 * eso se mapea con caché; el resto (registros de dispositivos) sin ella. */
static bool e_memoria(uint64_t ini, uint64_t fim)
{
    uint64_t c = ini;
    while (c < fim) {
        bool achou = false;
        for (uint32_t i = 0; i < info_boot.mapa_qtd; i++) {
            const regiao_e820_t *r = &mapa_memoria[i];
            if (!(r->extra & 1) || (r->tipo != 1 && r->tipo != 3 && r->tipo != 4)) continue;
            if (c >= r->base && c < r->base + r->tamanho) { c = r->base + r->tamanho; achou = true; break; }
        }
        if (!achou) return false;
    }
    return true;
}

void *uacpi_kernel_map(uacpi_phys_addr fis, uacpi_size n)
{
    if (!n) n = 1;
    uint64_t fim = (uint64_t)fis + n;
    if (fim > 0x100000000ull) return UACPI_MAP_FAILED;
    if (toca_ram_livre(fis, fim)) {
        if (!avisou_ram) {
            avisou_ram = true;
            klog("[UACPI] mapa rechazado: %x+%x es RAM libre del sistema\n", fis, n);
        }
        return UACPI_MAP_FAILED;
    }
    /* Debajo del fin del mapa 1:1 la dirección física ya es la virtual
     * (salvo la página 0, que queda sin mapa para atrapar los punteros nulos). */
    if (fis >= PAGINA && fim <= paginacao_fim_mapa()) return (void *)(uintptr_t)fis;

    uint32_t desloc = fis & (PAGINA - 1);
    uint32_t pags = (uint32_t)((desloc + n + PAGINA - 1) / PAGINA);
    if (!janela_base || pags > JANELA_PAGINAS) return UACPI_MAP_FAILED;
    uint32_t tipo = e_memoria(fis, fim) ? 0 : (PG_PCD | PG_PWT);
    bool i0 = int_salva_desliga();
    uint32_t ini = 0, seguidas = 0;
    bool achou = false;
    for (uint32_t i = 0; i < JANELA_PAGINAS; i++) {
        if (bit_usado(i)) { seguidas = 0; continue; }
        if (!seguidas) ini = i;
        if (++seguidas == pags) { achou = true; break; }
    }
    if (!achou) {
        int_restaura(i0);
        klog("[UACPI] sin ventanas para %x+%x (%u paginas en uso)\n", fis, n, janela_em_uso);
        return UACPI_MAP_FAILED;
    }
    for (uint32_t k = 0; k < pags; k++) {
        bit_por(ini + k, true);
        janela_pagina(janela_base + (ini + k) * PAGINA, (fis & ~(PAGINA - 1)) + k * PAGINA, tipo);
    }
    janela_em_uso += pags;
    if (janela_em_uso > janela_maximo) janela_maximo = janela_em_uso;
    int_restaura(i0);
    return (void *)(uintptr_t)(janela_base + ini * PAGINA + desloc);
}

void uacpi_kernel_unmap(void *p, uacpi_size n)
{
    uint32_t v = (uint32_t)(uintptr_t)p;
    if (!janela_base || v < janela_base || v >= janela_base + JANELA_PAGINAS * PAGINA) return;   /* era 1:1 */
    if (!n) n = 1;
    uint32_t desloc = v & (PAGINA - 1);
    uint32_t pags = (uint32_t)((desloc + n + PAGINA - 1) / PAGINA);
    uint32_t ini = (v - janela_base) / PAGINA;
    bool i0 = int_salva_desliga();
    for (uint32_t k = 0; k < pags && ini + k < JANELA_PAGINAS; k++) {
        if (!bit_usado(ini + k)) continue;
        janela_soltar(janela_base + (ini + k) * PAGINA);
        bit_por(ini + k, false);
        janela_em_uso--;
    }
    int_restaura(i0);
}

/* ------------------------------------------------------------ tablas */

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *rsdp)
{
    const info_acpi_t *a = hw_acpi();
    if (!a || !a->achou || !a->rsdp) return UACPI_STATUS_NOT_FOUND;
    *rsdp = a->rsdp;
    return UACPI_STATUS_OK;
}

/* ------------------------------------------------------------ PCI (mecanismo 1, solo el segmento 0) */

static uint32_t pci_endereco(uacpi_handle h, uacpi_size reg)
{
    return (uint32_t)(uintptr_t)h | (uint32_t)(reg & 0xFC);
}

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address a, uacpi_handle *h)
{
    if (a.segment != 0 || a.device > 31 || a.function > 7) return UACPI_STATUS_NOT_FOUND;
    *h = (uacpi_handle)(uintptr_t)(0x80000000u | (uint32_t)a.bus << 16 | (uint32_t)a.device << 11 |
                                   (uint32_t)a.function << 8);
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle h) { (void)h; }

static uacpi_status pci_ler(uacpi_handle h, uacpi_size reg, uint32_t largura, uint32_t *v)
{
    if (reg + largura > 256 || (reg & (largura - 1))) return UACPI_STATUS_INVALID_ARGUMENT;
    bool i0 = int_salva_desliga();
    porta_escreve32(0xCF8, pci_endereco(h, reg));
    uint16_t porta = (uint16_t)(0xCFC + (reg & 3));
    *v = largura == 1 ? porta_le8(porta) : largura == 2 ? porta_le16(porta) : porta_le32(0xCFC);
    int_restaura(i0);
    return UACPI_STATUS_OK;
}

static uacpi_status pci_escrever(uacpi_handle h, uacpi_size reg, uint32_t largura, uint32_t v)
{
    if (reg + largura > 256 || (reg & (largura - 1))) return UACPI_STATUS_INVALID_ARGUMENT;
    bool i0 = int_salva_desliga();
    porta_escreve32(0xCF8, pci_endereco(h, reg));
    uint16_t porta = (uint16_t)(0xCFC + (reg & 3));
    if (largura == 1) porta_escreve8(porta, (uint8_t)v);
    else if (largura == 2) porta_escreve16(porta, (uint16_t)v);
    else porta_escreve32(0xCFC, v);
    int_restaura(i0);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle h, uacpi_size r, uacpi_u8 *v)
{
    uint32_t x = 0xFF;
    uacpi_status s = pci_ler(h, r, 1, &x);
    *v = (uacpi_u8)x;
    return s;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle h, uacpi_size r, uacpi_u16 *v)
{
    uint32_t x = 0xFFFF;
    uacpi_status s = pci_ler(h, r, 2, &x);
    *v = (uacpi_u16)x;
    return s;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle h, uacpi_size r, uacpi_u32 *v)
{
    uint32_t x = 0xFFFFFFFFu;
    uacpi_status s = pci_ler(h, r, 4, &x);
    *v = x;
    return s;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle h, uacpi_size r, uacpi_u8 v) { return pci_escrever(h, r, 1, v); }
uacpi_status uacpi_kernel_pci_write16(uacpi_handle h, uacpi_size r, uacpi_u16 v) { return pci_escrever(h, r, 2, v); }
uacpi_status uacpi_kernel_pci_write32(uacpi_handle h, uacpi_size r, uacpi_u32 v) { return pci_escrever(h, r, 4, v); }

/* ------------------------------------------------------------ puertos de E/S */

/* El "handle" es el puerto base con el bit 16 puesto (el puerto 0 existe). */
uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size n, uacpi_handle *h)
{
    if ((uint64_t)base + n > 0x10000) return UACPI_STATUS_INVALID_ARGUMENT;
    *h = (uacpi_handle)(uintptr_t)(0x10000u | (uint32_t)base);
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle h) { (void)h; }

static uint16_t porta_de(uacpi_handle h, uacpi_size d) { return (uint16_t)(((uint32_t)(uintptr_t)h & 0xFFFF) + d); }

uacpi_status uacpi_kernel_io_read8(uacpi_handle h, uacpi_size d, uacpi_u8 *v) { *v = porta_le8(porta_de(h, d)); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_read16(uacpi_handle h, uacpi_size d, uacpi_u16 *v) { *v = porta_le16(porta_de(h, d)); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_read32(uacpi_handle h, uacpi_size d, uacpi_u32 *v) { *v = porta_le32(porta_de(h, d)); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_write8(uacpi_handle h, uacpi_size d, uacpi_u8 v) { porta_escreve8(porta_de(h, d), v); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_write16(uacpi_handle h, uacpi_size d, uacpi_u16 v) { porta_escreve16(porta_de(h, d), v); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_write32(uacpi_handle h, uacpi_size d, uacpi_u32 v) { porta_escreve32(porta_de(h, d), v); return UACPI_STATUS_OK; }

/* ------------------------------------------------------------ tiempo */

static inline uint64_t tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (uint64_t)hi << 32 | lo;
}

/* Nanosegundos desde el arranque, estrictamente crecientes: por el TSC si se
 * midió en el arranque; si no, por los tiques del reloj (10 ms). */
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void)
{
    static uint64_t tsc0, ultimo;
    uint32_t khz = hw_tsc_khz();
    uint64_t ns;
    bool i0 = int_salva_desliga();
    if (khz) {
        if (!tsc0) tsc0 = tsc() - (uint64_t)relogio_tiques() * (khz * (1000u / TIQUES_POR_SEGUNDO));
        uint64_t d = tsc() - tsc0;   /* en dos pasos: d * 10^6 no cabe en 64 bits tras horas a GHz */
        ns = d / khz * 1000000ull + d % khz * 1000000ull / khz;
    } else {
        ns = (uint64_t)relogio_tiques() * (1000000000ull / TIQUES_POR_SEGUNDO);
    }
    if (ns <= ultimo) ns = ultimo + 1;
    ultimo = ns;
    int_restaura(i0);
    return ns;
}

void uacpi_kernel_stall(uacpi_u8 us)
{
    uint32_t khz = hw_tsc_khz();
    if (khz) {
        uint64_t fim = tsc() + (uint64_t)us * khz / 1000 + 1;
        while ((int64_t)(tsc() - fim) < 0) porta_pausa();
    } else {
        for (uint32_t i = 0; i < us; i++) porta_pausa();   /* ~1 us por acceso al puerto 0x80 */
    }
}

/* ¿Puede dormir quien llama? Una tarea o un programa, fuera de una IRQ y con
 * las interrupciones puestas. */
static bool ints_ligadas(void)
{
    uint32_t f;
    __asm__ volatile("pushf; pop %0" : "=r"(f));
    return (f & 0x200) != 0;
}

static bool pode_dormir(void)
{
    processo_t *p = processo_atual();
    return p && p->pid != 0 && !contexto_irq() && ints_ligadas();
}

bool uacpi_sagui_pode_dormir(void) { return pode_dormir(); }

/* Duerme hasta "tiques" tiques o hasta acordar(canal). Con las interrupciones
 * desactivadas al entrar (quien llama ya miró su condición). */
static void dormir_em(const void *canal, uint32_t tiques)
{
    processo_t *eu = processo_atual();
    eu->acordar_em = relogio_tiques() + (tiques ? tiques : 1);
    bloquear(canal);
    ceder();
    eu->acordar_em = 0;
}

void uacpi_sagui_dormir_ms(uint32_t ms)
{
    if (!pode_dormir()) {
        for (uint32_t i = 0; i < ms * 1000; i++) porta_pausa();
        return;
    }
    static const char canal_dormir = 0;
    uint32_t fim = relogio_tiques() + (ms * TIQUES_POR_SEGUNDO + 999) / 1000;
    for (;;) {
        int_desliga();
        int32_t falta = (int32_t)(fim - relogio_tiques());
        if (falta <= 0) { int_liga(); return; }
        dormir_em(&canal_dormir, (uint32_t)falta);
        int_liga();
    }
}

void uacpi_kernel_sleep(uacpi_u64 ms) { uacpi_sagui_dormir_ms(ms > 60000 ? 60000 : (uint32_t)ms); }

/* ------------------------------------------------------------ interrupciones y cerrojos */

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) { return int_salva_desliga() ? 1 : 0; }
void uacpi_kernel_restore_interrupts(uacpi_interrupt_state s) { int_restaura(s != 0); }

/* Un cerrojo de espera activa = interrupciones desactivadas: dentro del
 * kernel solo corre una CPU a la vez (cerrojo general, D165). El objeto solo
 * existe porque uACPI quiere un puntero distinto de NULL. */
uacpi_handle uacpi_kernel_create_spinlock(void) { return kmalloc(4); }
void uacpi_kernel_free_spinlock(uacpi_handle h) { if (h) kfree(h); }
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle h) { (void)h; return int_salva_desliga() ? 1 : 0; }
void uacpi_kernel_unlock_spinlock(uacpi_handle h, uacpi_cpu_flags f) { (void)h; int_restaura(f != 0); }

typedef struct { volatile int32_t unidades; } semaforo_t;

uacpi_handle uacpi_kernel_create_semaphore(uacpi_u32 iniciais)
{
    semaforo_t *s = kmalloc(sizeof *s);
    if (s) s->unidades = (int32_t)(iniciais > 0x7FFFFFFF ? 0x7FFFFFFF : iniciais);
    return s;
}

void uacpi_kernel_free_semaphore(uacpi_handle h) { if (h) kfree(h); }

/* 0 = una sola vez; 0xFFFF = sin límite (despierta cada segundo para volver a
 * mirar y deja una línea en el registro cada 10 s: un cerrojo que nadie
 * suelta es un fallo, pero no debe colgar el sistema, solo esta tarea). */
uacpi_status uacpi_kernel_wait_for_semaphore(uacpi_handle h, uacpi_u16 ms)
{
    semaforo_t *s = h;
    uint32_t ini = relogio_tiques();
    uint32_t limite = ini + ((uint32_t)ms * TIQUES_POR_SEGUNDO + 999) / 1000;
    uint32_t aviso = ini + 10 * TIQUES_POR_SEGUNDO;
    for (;;) {
        bool i0 = int_salva_desliga();
        if (s->unidades > 0) {
            s->unidades--;
            int_restaura(i0);
            return UACPI_STATUS_OK;
        }
        uint32_t agora = relogio_tiques();
        if (ms == 0 || (ms != 0xFFFF && (int32_t)(agora - limite) >= 0)) {
            int_restaura(i0);
            return UACPI_STATUS_TIMEOUT;
        }
        if (ms == 0xFFFF && (int32_t)(agora - aviso) >= 0) {
            aviso = agora + 10 * TIQUES_POR_SEGUNDO;
            klog("[UACPI] esperando un semaforo desde hace %u s\n", (agora - ini) / TIQUES_POR_SEGUNDO);
        }
        if (i0 && pode_dormir()) {
            uint32_t passo = ms == 0xFFFF ? TIQUES_POR_SEGUNDO : (limite - agora);
            dormir_em(s, passo > TIQUES_POR_SEGUNDO ? TIQUES_POR_SEGUNDO : passo);
            int_restaura(i0);
        } else {
            int_restaura(i0);
            porta_pausa();   /* no puede dormir: gira (no debería pasar, uACPI corre en su tarea) */
        }
    }
}

void uacpi_kernel_signal_semaphore(uacpi_handle h)
{
    semaforo_t *s = h;
    bool i0 = int_salva_desliga();
    if (s->unidades < 0x7FFFFFFF) s->unidades++;
    acordar(s);
    int_restaura(i0);
}

uacpi_thread_id uacpi_kernel_get_thread_id(void)
{
    processo_t *p = processo_atual();
    return p ? (uacpi_thread_id)p : (uacpi_thread_id)1;
}

/* ------------------------------------------------------------ SCI */

/* Solo la IRQ del SCI: uACPI atiende ahí los eventos fijos que él activa (el
 * cerrojo global) y sus GPE (todos desactivados: no se llama a
 * uacpi_finalize_gpe_initialization). Se comparte con el manejador de acpi.c,
 * que se instaló antes y por eso mira primero el botón de encendido. */
static volatile uacpi_interrupt_handler sci_manejador;
static uacpi_handle sci_ctx;
static bool sci_compartilhado;
static uint32_t sci_irq;

static bool sci_trampolim(void)
{
    uacpi_interrupt_handler h = sci_manejador;
    return h && h(sci_ctx) == UACPI_INTERRUPT_HANDLED;
}

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq, uacpi_interrupt_handler h, uacpi_handle ctx,
                                                    uacpi_handle *saida)
{
    if (irq >= 16 || (sci_compartilhado && irq != sci_irq)) {
        klog("[UACPI] IRQ %u no soportada (solo la del SCI)\n", irq);
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    if (sci_manejador) return UACPI_STATUS_ALREADY_EXISTS;
    bool i0 = int_salva_desliga();
    sci_ctx = ctx;
    sci_manejador = h;
    if (!sci_compartilhado) {
        sci_compartilhado = true;
        sci_irq = irq;
        irq_compartilhar((uint8_t)irq, sci_trampolim);
    }
    int_restaura(i0);
    *saida = (uacpi_handle)&sci_manejador;
    klog("[UACPI] manejador del SCI en la IRQ %u\n", irq);
    return UACPI_STATUS_OK;
}

/* No hay cómo quitar un atendedor de la línea: se queda, pero sin hacer nada. */
uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler h, uacpi_handle irq)
{
    (void)h; (void)irq;
    bool i0 = int_salva_desliga();
    sci_manejador = NULL;
    sci_ctx = NULL;
    int_restaura(i0);
    return UACPI_STATUS_OK;
}

/* El botón de encendido como evento fijo de uACPI: llega aquí solo si uACPI
 * ve el bit antes que acpi.c (que es quien lo atiende normalmente). */
static uacpi_interrupt_ret botao_fixo(uacpi_handle ctx)
{
    (void)ctx;
    acpi_botao_evento();
    return UACPI_INTERRUPT_HANDLED;
}

/* ------------------------------------------------------------ trabajo diferido */

/* Las notificaciones (Notify) y los GPE se ejecutan después, en la tarea
 * "acpi-aml", nunca en la IRQ ni dentro del AML que las pidió. */
static struct { uacpi_work_handler f; uacpi_handle ctx; } fila[FILA_TRABALHO];
static uint32_t fila_ini, fila_fim, fila_perdidos;

uacpi_handle uacpi_kernel_create_work_item(void) { return (uacpi_handle)&fila; }   /* no hace falta guardar nada */
void uacpi_kernel_free_work_item(uacpi_handle h) { (void)h; }

void uacpi_kernel_schedule_work(uacpi_work_type tipo, uacpi_handle item, uacpi_work_handler f, uacpi_handle ctx)
{
    (void)tipo; (void)item;
    bool i0 = int_salva_desliga();
    if (fila_fim - fila_ini >= FILA_TRABALHO) {
        fila_perdidos++;
        int_restaura(i0);
        klog("[UACPI] cola de trabajo llena: se pierde un aviso (%u)\n", fila_perdidos);
        return;
    }
    fila[fila_fim % FILA_TRABALHO].f = f;
    fila[fila_fim % FILA_TRABALHO].ctx = ctx;
    fila_fim++;
    acordar(&canal_tarefa);
    int_restaura(i0);
}

/* D403/D405: alguien dejó un pedido para la tarea (brillo, teclas, radios del HP): que lo mire ya. */
static volatile bool pedido_novo;

void uacpi_sagui_acordar(void)
{
    bool i0 = int_salva_desliga();
    pedido_novo = true;
    acordar(&canal_tarefa);
    int_restaura(i0);
}

void uacpi_sagui_trabalho_pendente(void)
{
    for (int n = 0; n < 4 * FILA_TRABALHO; n++) {   /* un trabajo puede encolar otro, pero no sin fin */
        bool i0 = int_salva_desliga();
        if (fila_ini == fila_fim) { int_restaura(i0); return; }
        uacpi_work_handler f = fila[fila_ini % FILA_TRABALHO].f;
        uacpi_handle ctx = fila[fila_ini % FILA_TRABALHO].ctx;
        fila_ini++;
        int_restaura(i0);
        f(ctx);
    }
}

/* uACPI la llama desde la propia tarea (al desinstalar algo): lo pendiente
 * se ejecuta aquí mismo. Ninguna IRQ puede estar a medias: dentro del kernel
 * solo hay una CPU (D165). */
uacpi_status uacpi_kernel_wait_for_work_completion(void)
{
    uacpi_sagui_trabalho_pendente();
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *r)
{
    if (r->type == UACPI_FIRMWARE_REQUEST_TYPE_FATAL)
        klog("[UACPI] el firmware pide Fatal: tipo %x codigo %x arg %x\n", r->fatal.type, r->fatal.code,
             (uint32_t)r->fatal.arg);
    return UACPI_STATUS_OK;
}

/* ------------------------------------------------------------ SystemMemory por páginas (D405) */

/* El manejador de serie de uACPI mapea la OperationRegion ENTERA al adjuntarla (su propio
 * código dice «FIXME: this really shouldn't try to map everything at once»). El HP dv7
 * declara \PCFG = SystemMemory 0xE0000000, 256 MB (el MMCONFIG entero) en lo alto de la
 * DSDT: no cabe en la franja de 8 MB, el mapa falla y uACPI aborta la carga de la DSDT
 * entera. Aquí, sin tocar uACPI: las regiones que caben (hasta REGIAO_INTEIRA) se mapean
 * enteras como siempre; las más grandes se mapean página a página en cada acceso. Las
 * reglas de uacpi_kernel_map (nunca RAM libre; sin caché lo que no es RAM ni tablas)
 * valen igual en los dos casos. */
#define REGIAO_INTEIRA (1024u * 1024u)

typedef struct {
    uint64_t base, tam;
    uint8_t *virt;          /* NULL = por páginas */
} regiao_mem_t;

static uacpi_status regiao_memoria(uacpi_region_op op, uacpi_handle dados)
{
    if (op == UACPI_REGION_OP_ATTACH) {
        uacpi_region_attach_data *a = dados;
        regiao_mem_t *r = kmalloc(sizeof *r);
        if (!r) return UACPI_STATUS_OUT_OF_MEMORY;
        r->base = a->generic_info.base;
        r->tam = a->generic_info.length;
        r->virt = NULL;
        if (r->tam <= REGIAO_INTEIRA) {
            void *v = uacpi_kernel_map(r->base, (uacpi_size)r->tam);
            if (v == UACPI_MAP_FAILED) {
                kfree(r);
                uacpi_trace_region_error(a->region_node, "unable to map", UACPI_STATUS_MAPPING_FAILED);
                return UACPI_STATUS_MAPPING_FAILED;
            }
            r->virt = v;
        } else {
            const char *n = uacpi_namespace_node_generate_absolute_path(a->region_node);
            klog("[UACPI] region %s: %x+%x MB, se mapea pagina a pagina al usarla\n", n ? n : "?", (uint32_t)r->base,
                 (uint32_t)(r->tam >> 20));
            if (n) uacpi_free_absolute_path(n);
        }
        a->out_region_context = r;
        return UACPI_STATUS_OK;
    }
    if (op == UACPI_REGION_OP_DETACH) {
        uacpi_region_detach_data *d = dados;
        regiao_mem_t *r = d->region_context;
        if (r) {
            if (r->virt) uacpi_kernel_unmap(r->virt, (uacpi_size)r->tam);
            kfree(r);
        }
        return UACPI_STATUS_OK;
    }
    if (op != UACPI_REGION_OP_READ && op != UACPI_REGION_OP_WRITE) return UACPI_STATUS_INVALID_ARGUMENT;
    uacpi_region_rw_data *rw = dados;
    regiao_mem_t *r = rw->region_context;
    uint64_t desl = rw->address - r->base;
    if (r->virt)
        return op == UACPI_REGION_OP_READ ? uacpi_system_memory_read(r->virt, (uacpi_size)desl, rw->byte_width, &rw->value)
                                          : uacpi_system_memory_write(r->virt, (uacpi_size)desl, rw->byte_width, rw->value);
    void *v = uacpi_kernel_map(rw->address, rw->byte_width);
    if (v == UACPI_MAP_FAILED) return UACPI_STATUS_MAPPING_FAILED;
    uacpi_status s = op == UACPI_REGION_OP_READ ? uacpi_system_memory_read(v, 0, rw->byte_width, &rw->value)
                                                : uacpi_system_memory_write(v, 0, rw->byte_width, rw->value);
    uacpi_kernel_unmap(v, rw->byte_width);
    return s;
}

/* Cambia el manejador de serie de SystemMemory de la raíz por el de arriba. Antes de
 * cargar las tablas: aún no hay regiones adjuntadas. Si algo falla, sigue el de serie. */
static void trocar_manejador_memoria(void)
{
    uacpi_namespace_node *raiz = uacpi_namespace_root();
    uacpi_address_space_handlers *hs = uacpi_node_get_address_space_handlers(raiz);
    uacpi_region_handler serie = NULL;
    for (uacpi_address_space_handler *h = hs ? hs->head : NULL; h; h = h->next)
        if (h->space == UACPI_ADDRESS_SPACE_SYSTEM_MEMORY) { serie = h->callback; break; }
    if (!serie) { klog("[UACPI] sin manejador de SystemMemory de serie: se deja como esta\n"); return; }
    uacpi_status r = uacpi_uninstall_address_space_handler(raiz, UACPI_ADDRESS_SPACE_SYSTEM_MEMORY, serie);
    if (r != UACPI_STATUS_OK) {
        klog("[UACPI] no se pudo quitar el manejador de SystemMemory de serie: %s\n", uacpi_status_to_string(r));
        return;
    }
    r = uacpi_install_address_space_handler(raiz, UACPI_ADDRESS_SPACE_SYSTEM_MEMORY, regiao_memoria, NULL);
    if (r != UACPI_STATUS_OK) {
        klog("[UACPI] manejador de SystemMemory propio: %s; vuelve el de serie\n", uacpi_status_to_string(r));
        uacpi_install_address_space_handler(raiz, UACPI_ADDRESS_SPACE_SYSTEM_MEMORY, serie, NULL);
    }
}

/* ------------------------------------------------------------ apagar y reiniciar (D450)
 *
 * Antes de apagar o reiniciar, el AML que el firmware espera, como Linux: _TTS(5) («el sistema va
 * a pasar a S5», si existe) y, solo al apagar, _PTS(5) y _SST(0) (uacpi_prepare_for_sleep_state).
 * Lo ejecuta la tarea "acpi-aml", que es la única que llama a uACPI: la tarea "apagar" deja el
 * pedido y espera como mucho el plazo que diga. Si uACPI no está activo, no se hace nada. */
static volatile int energia_pedido;      /* 0 = nada; 1 = reiniciar; 2 = apagar */
static volatile bool energia_feito;
static volatile bool aml_ativo;          /* la tarea ya cargó el espacio de nombres y está en su bucle */
static const char canal_energia = 0;
static char energia_texto[96];

static uacpi_status avaliar_com_inteiro(const char *nome, uacpi_u64 valor)
{
    uacpi_object *arg = uacpi_object_create_integer(valor);
    if (!arg) return UACPI_STATUS_OUT_OF_MEMORY;
    uacpi_object_array args = { &arg, 1 };
    uacpi_status r = uacpi_eval(uacpi_namespace_root(), nome, &args, UACPI_NULL);
    uacpi_object_unref(arg);
    return r;
}

static void energia_atender(void)
{
    int p = energia_pedido;
    if (!p || energia_feito) return;
    uacpi_status tts = avaliar_com_inteiro("_TTS", 5);
    uacpi_status pts = UACPI_STATUS_OK;
    if (p == 2) pts = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    snformatar(energia_texto, sizeof energia_texto, "_TTS(5): %s; _PTS(5): %s",
               tts == UACPI_STATUS_NOT_FOUND ? "no existe" : uacpi_status_to_string(tts),
               p == 2 ? (pts == UACPI_STATUS_OK ? "hecho" : uacpi_status_to_string(pts)) : "no (reinicio)");
    bool i0 = int_salva_desliga();
    energia_feito = true;
    acordar(&canal_energia);
    int_restaura(i0);
}

/* Desde la tarea "apagar" (puede dormir). Devuelve el texto para el registro. */
const char *uacpi_sagui_preparar_energia(bool apagar, uint32_t ms)
{
    if (!aml_ativo) return "uACPI no esta activo: sin _TTS ni _PTS";
    energia_feito = false;
    energia_pedido = apagar ? 2 : 1;
    uacpi_sagui_acordar();
    uint32_t fim = relogio_tiques() + (ms * TIQUES_POR_SEGUNDO + 999) / 1000;
    for (;;) {
        int_desliga();
        int32_t falta = (int32_t)(fim - relogio_tiques());
        if (energia_feito || falta <= 0) { int_liga(); break; }
        dormir_em(&canal_energia, (uint32_t)falta);
        int_liga();
    }
    if (!energia_feito) {
        snformatar(energia_texto, sizeof energia_texto, "el AML no termino en %u ms: se sigue sin esperar", ms);
        aml_ativo = false;   /* que nadie más lo espere */
    }
    return energia_texto;
}

/* ------------------------------------------------------------ la tarea */

static uint32_t *guarda;    /* la página de guarda, debajo de la pila */

static bool guarda_intacta(void)
{
    for (uint32_t i = 0; i < GUARDA_BYTES / 4; i++)
        if (guarda[i] != GUARDA_PADRAO) return false;
    return true;
}

static void falhou(const char *etapa, uacpi_status r)
{
    char m[64];
    snformatar(m, sizeof m, "%s: %s", etapa, uacpi_status_to_string(r));
    klog("[UACPI] no se pudo cargar (%s): el sistema sigue sin uACPI\n", m);
    bateria_uacpi_estado(UACPI_FALHOU, m);
    acpi_botao_rearmar();   /* uacpi_initialize() apaga los eventos fijos */
}

/* Corre en la pila grande. Si vuelve, la tarea termina. */
static void executar(void)
{
    uacpi_sagui_dormir_ms(ESPERA_ARRANQUE_MS);   /* que el escritorio salga primero */
    bateria_uacpi_estado(UACPI_INICIANDO, "");
    uint64_t t0 = uacpi_kernel_get_nanoseconds_since_boot();

    uacpi_status r = uacpi_initialize(UACPI_FLAG_NO_ACPI_MODE);
    if (r != UACPI_STATUS_OK) { falhou("tablas", r); return; }
    if (acpi_botao_ativo()) {
        r = uacpi_install_fixed_event_handler(UACPI_FIXED_EVENT_POWER_BUTTON, botao_fixo, NULL);
        if (r != UACPI_STATUS_OK) {
            klog("[UACPI] boton: %s; queda solo el de acpi.c\n", uacpi_status_to_string(r));
            acpi_botao_rearmar();
        }
    }
    trocar_manejador_memoria();   /* D405: regiones grandes (el MMCONFIG del dv7) por páginas */
    r = uacpi_namespace_load();
    if (r != UACPI_STATUS_OK) { falhou("espacio de nombres", r); return; }
    /* El EC antes de los _INI: muchos leen el EC (o miran si ya se puede). */
    ec_iniciar();
    r = uacpi_namespace_initialize();
    if (r != UACPI_STATUS_OK) { falhou("_INI", r); return; }
    uacpi_sagui_trabalho_pendente();
    /* D211: con el IOAPIC, el enrutado PCI de verdad (\_PIC(1) y _PRT). */
    irq_rotas_prt();
    bateria_iniciar();
    zonas_termicas_iniciar();   /* D399 */
    wmi_iniciar();       /* D405: el puente WMI (PNP0C14) y, encima, el de HP */
    hp_wmi_iniciar();
    portatil_iniciar();   /* D403: brillo y teclas de los portátiles (ACPI Video, Fujitsu, Toshiba) */
    uacpi_sagui_trabalho_pendente();
    uint64_t ms = (uacpi_kernel_get_nanoseconds_since_boot() - t0) / 1000000ull;
    klog("[UACPI] listo en %u ms (ventanas: %u paginas en uso, maximo %u)\n", (uint32_t)ms, janela_em_uso,
         janela_maximo);
    bateria_uacpi_estado(UACPI_ATIVO, "");
    aml_ativo = true;   /* D450 */

    for (;;) {
        energia_atender();   /* D450: primero, si la tarea "apagar" lo espera */
        uacpi_sagui_trabalho_pendente();
        ec_atender_eventos();
        bateria_ciclo();
        zonas_termicas_ciclo();   /* D399: solo si alguien mira la GPU */
        hp_wmi_ciclo();           /* D405 */
        if (!gui_ativa()) acoes_atender();   /* D403: sin escritorio, las acciones y los avisos se hacen aquí */
        portatil_ciclo();
        if (!guarda_intacta()) {
            klog("[UACPI] la pila de uACPI se ha desbordado: se detiene (reinicia pronto)\n");
            bateria_uacpi_estado(UACPI_FALHOU, "pila desbordada");
            for (;;) { int_desliga(); dormir_em(&canal_tarefa, 0x7FFFFFFF); int_liga(); }
        }
        int_desliga();
        if (fila_ini == fila_fim && !pedido_novo && !portatil_pendente() && !(energia_pedido && !energia_feito))
            dormir_em(&canal_tarefa, (ec_presente() ? PERIODO_EC_MS : PERIODO_MS) * TIQUES_POR_SEGUNDO / 1000);
        pedido_novo = false;
        int_liga();
    }
}

/* Pasa a la pila grande, llama a "f" y vuelve a la de la tarea. EBX guarda
 * la pila vieja (el C la conserva entre llamadas). */
static void com_pilha(void (*f)(void), uint32_t topo)
{
    __asm__ volatile("mov %%esp, %%ebx\n\t"
                     "mov %1, %%esp\n\t"
                     "call *%0\n\t"
                     "mov %%ebx, %%esp\n\t"
                     : "+a"(f), "+c"(topo) : : "ebx", "edx", "memory", "cc");
}

static void tarefa_aml(void *arg)
{
    (void)arg;
    uint32_t paginas = (PILHA_BYTES + GUARDA_BYTES) / PAGINA;
    uint32_t base = memfisica_alocar_seguidas(paginas);
    janela_base = janela_reservar(JANELA_PAGINAS * PAGINA);
    if (!base || !janela_base) {
        klog("[UACPI] sin memoria para la pila o las ventanas: el sistema sigue sin uACPI\n");
        bateria_uacpi_estado(UACPI_FALHOU, "sin memoria");
        if (base) memfisica_liberar_seguidas(base, paginas);
        return;
    }
    guarda = (uint32_t *)base;
    for (uint32_t i = 0; i < GUARDA_BYTES / 4; i++) guarda[i] = GUARDA_PADRAO;
    com_pilha(executar, (base + paginas * PAGINA) & ~15u);
    /* executar() volvió: uACPI no cargó. La pila no se devuelve: algún
     * puntero de uACPI podría haber quedado en ella; son 68 KB. */
}

void uacpi_sagui_iniciar(void)
{
    /* Desactivado por defecto: solo con la opción de imagen --uacpi. */
    if (!(info_boot.opcoes & OPC_BOOT_UACPI)) {
        klog("[UACPI] desactivado (la imagen no lleva la opcion --uacpi)\n");
        bateria_uacpi_estado(UACPI_DESLIGADO, "desactivado (imagen sin --uacpi)");
        return;
    }
    if (info_boot.opcoes & OPC_BOOT_SEM_ACPI) {
        bateria_uacpi_estado(UACPI_DESLIGADO, "ACPI desactivado al arrancar (--sem-acpi)");
        return;
    }
    if (!acpi_modo_ativo()) {
        klog("[UACPI] el sistema no esta en modo ACPI: no se arranca el interprete\n");
        bateria_uacpi_estado(UACPI_DESLIGADO, "el sistema no está en modo ACPI");
        return;
    }
    bateria_uacpi_estado(UACPI_INICIANDO, "");   /* hasta que la tarea termine de cargar */
    tarefa_kernel("acpi-aml", tarefa_aml, NULL);
    klog("[UACPI] tarea \"acpi-aml\" creada\n");
}
