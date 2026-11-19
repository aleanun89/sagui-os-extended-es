// SPDX-License-Identifier: GPL-2.0-or-later
/* uacpi_volcado.c - el intérprete de AML (uACPI) en el PC, sobre el volcado
 * ACPI de una máquina real (D192).
 *
 * Sirve para comprobar, sin el portátil, que las tablas de una BIOS concreta
 * se cargan y que la batería se lee sin colgar: hace lo mismo que la tarea
 * "acpi-aml" del kernel (uacpi_sagui.c, ec.c, bateria.c) en el mismo orden:
 *   uacpi_initialize(UACPI_FLAG_NO_ACPI_MODE), uacpi_namespace_load(), el EC
 *   (PNP0C09) con su manejador, uacpi_namespace_initialize() (los _INI),
 *   busca PNP0C0A y ACPI0003 y evalúa _STA, _BIX o _BIF, _BST y _PSR varias
 *   veces; al final calcula el porcentaje con kernel/lib/bateria_conv.c.
 *
 * La "máquina" es de mentira:
 *   - la memoria física son 4 GB reservados sin ocupar (mmap): las tablas van
 *     a direcciones inventadas, con un RSDP, una XSDT y una RSDT nuevas y la
 *     FADT apuntando a la DSDT y la FACS (los volcados de uACPI/ACPIDumps no
 *     guardan las direcciones). El resto de la memoria lee ceros y recuerda
 *     lo que se escribe;
 *   - los puertos de E/S empiezan a 0 (o a 0xFF con --io-ff) y recuerdan lo
 *     último que se escribió (así uACPI ve los bits de habilitar que pone);
 *   - el espacio PCI recuerda lo que se escribe (empieza a ceros);
 *   - el controlador embebido es una RAM de 256 bytes a ceros (o 0xFF);
 *   - el reloj es virtual: avanza en cada consulta y con Sleep/Stall, así un
 *     bucle de espera del AML acaba por el límite de uACPI y no cuelga.
 * Si aun así algo se queda colgado, un temporizador (--limite, 60 s) lo dice
 * y sale con el código 3.
 *
 * Las SSDT "dinámicas" del esquema de Intel (OEM "PmRef", menos "CpuPm") no
 * van en la XSDT: en la máquina las carga el _PDC del procesador, que el
 * SaguiOS no llama.
 *
 * Con --prt 945|hm55 (D211) hace además lo de irq_rotas.c en la tarea: llama
 * a \_PIC(0) y a \_PIC(1), escribe todos los _PRT del espacio de nombres
 * en los dos modos y resuelve con kernel/dispositivos/prt.c (el mismo código
 * que el kernel) las funciones PCI de un chipset de ejemplo: el 945/ICH7-M
 * (Inspiron 6400, el del Amilo Pi 1536) o el HM55 (Satellite L655). Como el
 * PCI es de mentira, el registro de enrutado PIRQ del puente LPC tiene los
 * valores de una BIOS típica (--pirq cambia los ocho bytes).
 *
 * Con --portatil (D403) mira lo que buscan video_acpi.c, fujitsu.c y toshiba.c:
 * los dispositivos de vídeo (_DOD/_DOS) y sus salidas con _BCL/_BCM/_BQC (con
 * los niveles ya ordenados por kernel/lib/brilho_conv.c), FUJ02B1/FUJ02E3 y
 * TOS1900/TOS620x (métodos, _STA, FUNC(0x1000, 0), y la HCI: brillo,
 * ventilador, radio). Llama a _DOS(0) y a ENAB como el kernel; nunca a _BCM.
 *
 * Uso: uacpi_volcado [--io-ff] [--ec-ff] [--limite S] [-v] [--prt 945|hm55] [--portatil] volcado.txt
 * El volcado es la salida de `acpidump` (texto: "DSDT @ 0x..." y las líneas
 * "    0000: 44 53 44 54 ...").
 * Se compila en el anfitrión (tests/h158_bateria.py, parte 0b):
 *   cc -O1 -g -Ikernel/terceros/uacpi/include -Ikernel/inc -DSAGUI_HOST
 *      tools/uacpi_volcado.c kernel/lib/bateria_conv.c kernel/lib/brilho_conv.c
 *      kernel/dispositivos/prt.c kernel/terceros/uacpi/source/(todos).c -o uacpi_volcado */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>

#include <uacpi/uacpi.h>
#include <uacpi/kernel_api.h>
#include <uacpi/namespace.h>
#include <uacpi/utilities.h>
#include <uacpi/opregion.h>
#include <uacpi/notify.h>
#include <uacpi/context.h>
#include <uacpi/event.h>

#include "bateria_conv.h"
#include "brilho_conv.h"
#include "prt.h"

#define MEM_BYTES     (1ull << 32)
#define TABELAS_BASE  0x10000000u   /* donde se ponen las tablas del volcado */
#define RSDP_END      0x000E0000u
#define MAX_TABELAS   64

static uint8_t *mem;                /* la memoria física de mentira */
static int verboso, io_ff, ec_ff;
static unsigned limite_s = 60;
static uint64_t relogio_ns;         /* reloj virtual */
static unsigned erros_log, avisos_log;
static uint8_t ec_ram[256];
static uint8_t portas[65536 + 4];
static unsigned ec_leituras, ec_escritas, io_acessos, pci_acessos, notificacoes;

/* ------------------------------------------------------------ volcado */

typedef struct { char assin[5]; uint8_t *dados; uint32_t n; uint32_t end; } tabela_t;
static tabela_t tab[MAX_TABELAS];
static int ntab;

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Lee el volcado de acpidump: una cabecera "XXXX @ 0x..." por tabla y
 * líneas "    0000: 44 53 ...  texto". Solo se toman los 16 grupos de dos
 * cifras hexadecimales tras los dos puntos (el texto del final se ignora). */
static int ler_volcado(const char *nome)
{
    FILE *f = fopen(nome, "r");
    if (!f) { perror(nome); return -1; }
    char linha[512];
    tabela_t *t = NULL;
    uint32_t cap = 0;
    while (fgets(linha, sizeof linha, f)) {
        char *at = strstr(linha, " @ 0x");
        if (at && at - linha == 4) {
            if (ntab == MAX_TABELAS) { fprintf(stderr, "demasiadas tablas\n"); break; }
            t = &tab[ntab++];
            memcpy(t->assin, linha, 4);
            t->assin[4] = 0;
            t->dados = NULL; t->n = 0; cap = 0;
            continue;
        }
        if (!t) continue;
        char *p = linha;
        while (*p == ' ') p++;
        char *dp = strchr(p, ':');
        if (!dp || dp - p < 4 || dp - p > 8) continue;
        p = dp + 1;
        for (int g = 0; g < 16; g++) {
            if (p[0] != ' ' || hexval(p[1]) < 0 || hexval(p[2]) < 0) break;
            if (t->n == cap) {
                cap = cap ? cap * 2 : 4096;
                t->dados = realloc(t->dados, cap);
                if (!t->dados) { fclose(f); return -1; }
            }
            t->dados[t->n++] = (uint8_t)(hexval(p[1]) * 16 + hexval(p[2]));
            p += 3;
        }
    }
    fclose(f);
    return ntab;
}

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static void es32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void es64(uint8_t *p, uint64_t v) { es32(p, (uint32_t)v); es32(p + 4, (uint32_t)(v >> 32)); }

static void soma(uint8_t *t, uint32_t n, uint32_t pos)
{
    uint8_t s = 0;
    t[pos] = 0;
    for (uint32_t i = 0; i < n; i++) s += t[i];
    t[pos] = (uint8_t)-s;
}

static int dinamica(const tabela_t *t)
{
    if (strcmp(t->assin, "SSDT") || t->n < 36) return 0;
    return !memcmp(t->dados + 10, "PmRef ", 6) && memcmp(t->dados + 16, "CpuPm", 5);
}

/* Pone las tablas en la memoria de mentira y construye RSDP, XSDT y RSDT. */
static int montar(void)
{
    uint32_t pos = TABELAS_BASE;
    int fadt = -1, dsdt = -1, facs = -1;
    for (int i = 0; i < ntab; i++) {
        tabela_t *t = &tab[i];
        if (!strcmp(t->assin, "RSD ") || !strcmp(t->assin, "RSDP") || !strcmp(t->assin, "XSDT") ||
            !strcmp(t->assin, "RSDT")) { t->end = 0; continue; }
        if (t->n < 36 && strcmp(t->assin, "FACS")) { t->end = 0; continue; }
        if (strcmp(t->assin, "FACS") && le32(t->dados + 4) != t->n) {
            fprintf(stderr, "aviso: %s mide %u y el volcado trae %u bytes\n", t->assin, le32(t->dados + 4), t->n);
            if (le32(t->dados + 4) > t->n) { t->end = 0; continue; }
        }
        pos = (pos + 63) & ~63u;
        t->end = pos;
        memcpy(mem + pos, t->dados, t->n);
        pos += t->n;
        if (!strcmp(t->assin, "FACP") && fadt < 0) fadt = i;
        else if (!strcmp(t->assin, "DSDT") && dsdt < 0) dsdt = i;
        else if (!strcmp(t->assin, "FACS") && facs < 0) facs = i;
    }
    if (fadt < 0 || dsdt < 0) { fprintf(stderr, "el volcado no tiene FACP o DSDT\n"); return -1; }
    /* La FADT apunta a la DSDT y a la FACS (32 y 64 bits). */
    uint8_t *f = mem + tab[fadt].end;
    uint32_t fn = tab[fadt].n;
    es32(f + 40, tab[dsdt].end);
    es32(f + 36, facs >= 0 ? tab[facs].end : 0);
    if (fn >= 140) es64(f + 132, facs >= 0 ? tab[facs].end : 0);
    if (fn >= 148) es64(f + 140, tab[dsdt].end);
    soma(f, fn, 9);
    /* XSDT y RSDT con todo menos DSDT, FACS y las SSDT dinámicas. */
    uint32_t ent[MAX_TABELAS];
    int n = 0;
    for (int i = 0; i < ntab; i++) {
        if (!tab[i].end || i == dsdt || !strcmp(tab[i].assin, "FACS") || !strcmp(tab[i].assin, "DSDT")) continue;
        if (dinamica(&tab[i])) {
            if (verboso) printf("  (SSDT dinámica fuera de la XSDT: %.8s)\n", tab[i].dados + 16);
            continue;
        }
        ent[n++] = tab[i].end;
    }
    pos = (pos + 63) & ~63u;
    uint32_t xsdt = pos, xn = 36 + 8 * n;
    uint8_t *x = mem + xsdt;
    memcpy(x, "XSDT", 4); es32(x + 4, xn); x[8] = 1;
    memcpy(x + 10, "SAGUI ", 6); memcpy(x + 16, "VOLCADO ", 8);
    for (int i = 0; i < n; i++) es64(x + 36 + 8 * i, ent[i]);
    soma(x, xn, 9);
    pos = (xsdt + xn + 63) & ~63u;
    uint32_t rsdt = pos, rn = 36 + 4 * n;
    uint8_t *r = mem + rsdt;
    memcpy(r, "RSDT", 4); es32(r + 4, rn); r[8] = 1;
    memcpy(r + 10, "SAGUI ", 6); memcpy(r + 16, "VOLCADO ", 8);
    for (int i = 0; i < n; i++) es32(r + 36 + 4 * i, ent[i]);
    soma(r, rn, 9);
    uint8_t *p = mem + RSDP_END;
    memcpy(p, "RSD PTR ", 8);
    memcpy(p + 9, "SAGUI ", 6);
    p[15] = 2;
    es32(p + 16, rsdt);
    es32(p + 20, 36);
    es64(p + 24, xsdt);
    soma(p, 20, 8);
    soma(p, 36, 32);
    return n;
}

/* ------------------------------------------------------------ uacpi_kernel_* */

void uacpi_kernel_log(uacpi_log_level nivel, const uacpi_char *texto)
{
    if (nivel == UACPI_LOG_ERROR) erros_log++;
    if (nivel == UACPI_LOG_WARN) avisos_log++;
    if (nivel <= UACPI_LOG_WARN || verboso)
        printf("  [uACPI %s] %s", nivel == UACPI_LOG_ERROR ? "error" : nivel == UACPI_LOG_WARN ? "aviso" : "info",
               texto);
}

void *uacpi_kernel_alloc(uacpi_size n) { return malloc(n ? n : 1); }
void uacpi_kernel_free(void *p) { free(p); }

void *uacpi_kernel_map(uacpi_phys_addr fis, uacpi_size n)
{
    if (fis >= MEM_BYTES || n > MEM_BYTES - fis) return NULL;
    return mem + fis;
}
void uacpi_kernel_unmap(void *p, uacpi_size n) { (void)p; (void)n; }

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *rsdp) { *rsdp = RSDP_END; return UACPI_STATUS_OK; }

/* PCI: 256 bytes por función, creados al usarse. */
typedef struct pci_fn { uacpi_pci_address a; uint8_t cfg[256]; struct pci_fn *prox; } pci_fn_t;
static pci_fn_t *pcis;

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address a, uacpi_handle *h)
{
    for (pci_fn_t *p = pcis; p; p = p->prox)
        /* Campo a campo: la estructura tiene un byte de relleno sin valor fijo. */
        if (p->a.segment == a.segment && p->a.bus == a.bus && p->a.device == a.device &&
            p->a.function == a.function) { *h = p; return UACPI_STATUS_OK; }
    pci_fn_t *p = calloc(1, sizeof *p);
    if (!p) return UACPI_STATUS_OUT_OF_MEMORY;
    p->a = a; p->prox = pcis; pcis = p;
    *h = p;
    return UACPI_STATUS_OK;
}
void uacpi_kernel_pci_device_close(uacpi_handle h) { (void)h; }

static uacpi_status pci_rw(uacpi_handle h, uacpi_size r, int n, uint32_t *v, int escrever)
{
    pci_fn_t *p = h;
    pci_acessos++;
    if (r + n > 256) { if (!escrever) *v = 0xFFFFFFFFu >> (32 - 8 * n); return UACPI_STATUS_OK; }
    if (escrever) { for (int i = 0; i < n; i++) p->cfg[r + i] = (uint8_t)(*v >> (8 * i)); }
    else { *v = 0; for (int i = 0; i < n; i++) *v |= (uint32_t)p->cfg[r + i] << (8 * i); }
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_read8(uacpi_handle h, uacpi_size r, uacpi_u8 *v)
{ uint32_t x; pci_rw(h, r, 1, &x, 0); *v = (uint8_t)x; return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_pci_read16(uacpi_handle h, uacpi_size r, uacpi_u16 *v)
{ uint32_t x; pci_rw(h, r, 2, &x, 0); *v = (uint16_t)x; return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_pci_read32(uacpi_handle h, uacpi_size r, uacpi_u32 *v)
{ uint32_t x; pci_rw(h, r, 4, &x, 0); *v = x; return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_pci_write8(uacpi_handle h, uacpi_size r, uacpi_u8 v)
{ uint32_t x = v; return pci_rw(h, r, 1, &x, 1); }
uacpi_status uacpi_kernel_pci_write16(uacpi_handle h, uacpi_size r, uacpi_u16 v)
{ uint32_t x = v; return pci_rw(h, r, 2, &x, 1); }
uacpi_status uacpi_kernel_pci_write32(uacpi_handle h, uacpi_size r, uacpi_u32 v)
{ uint32_t x = v; return pci_rw(h, r, 4, &x, 1); }

/* E/S: el asa es la base del rango. */
uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size n, uacpi_handle *h)
{ (void)n; *h = (uacpi_handle)(uintptr_t)base; return UACPI_STATUS_OK; }
void uacpi_kernel_io_unmap(uacpi_handle h) { (void)h; }
static uint32_t porta_rw(uacpi_handle h, uacpi_size d, int n, uint32_t v, int escrever)
{
    uint32_t p = ((uint32_t)(uintptr_t)h + (uint32_t)d) & 0xFFFF;
    io_acessos++;
    if (escrever) { for (int i = 0; i < n; i++) portas[p + i] = (uint8_t)(v >> (8 * i)); return 0; }
    v = 0;
    for (int i = 0; i < n; i++) v |= (uint32_t)portas[p + i] << (8 * i);
    return v;
}
uacpi_status uacpi_kernel_io_read8(uacpi_handle h, uacpi_size d, uacpi_u8 *v)
{ *v = (uint8_t)porta_rw(h, d, 1, 0, 0); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_read16(uacpi_handle h, uacpi_size d, uacpi_u16 *v)
{ *v = (uint16_t)porta_rw(h, d, 2, 0, 0); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_read32(uacpi_handle h, uacpi_size d, uacpi_u32 *v)
{ *v = porta_rw(h, d, 4, 0, 0); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_write8(uacpi_handle h, uacpi_size d, uacpi_u8 v)
{ porta_rw(h, d, 1, v, 1); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_write16(uacpi_handle h, uacpi_size d, uacpi_u16 v)
{ porta_rw(h, d, 2, v, 1); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_write32(uacpi_handle h, uacpi_size d, uacpi_u32 v)
{ porta_rw(h, d, 4, v, 1); return UACPI_STATUS_OK; }

/* El reloj virtual: 10 us por consulta. */
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) { return relogio_ns += 10000; }
void uacpi_kernel_stall(uacpi_u8 us) { relogio_ns += us * 1000ull; }
void uacpi_kernel_sleep(uacpi_u64 ms) { relogio_ns += ms * 1000000ull; }

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) { return 1; }
void uacpi_kernel_restore_interrupts(uacpi_interrupt_state s) { (void)s; }

uacpi_handle uacpi_kernel_create_spinlock(void) { return malloc(4); }
void uacpi_kernel_free_spinlock(uacpi_handle h) { free(h); }
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle h) { (void)h; return 0; }
void uacpi_kernel_unlock_spinlock(uacpi_handle h, uacpi_cpu_flags f) { (void)h; (void)f; }

/* Semáforos: con un solo hilo, esperar a uno vacío es esperar a que pase el
 * tiempo (el AML que espera un evento que nadie señala acaba por tiempo). */
uacpi_handle uacpi_kernel_create_semaphore(uacpi_u32 iniciais)
{
    uint32_t *s = malloc(sizeof *s);
    if (s) *s = iniciais;
    return s;
}
void uacpi_kernel_free_semaphore(uacpi_handle h) { free(h); }
uacpi_status uacpi_kernel_wait_for_semaphore(uacpi_handle h, uacpi_u16 ms)
{
    uint32_t *s = h;
    if (*s) { (*s)--; return UACPI_STATUS_OK; }
    if (ms == 0xFFFF) {
        printf("  [arnés] espera sin fin a un semáforo vacío: se da por agotada\n");
        erros_log++;
    }
    relogio_ns += (ms == 0xFFFF ? 1000 : ms) * 1000000ull;
    return UACPI_STATUS_TIMEOUT;
}
void uacpi_kernel_signal_semaphore(uacpi_handle h) { (*(uint32_t *)h)++; }

uacpi_thread_id uacpi_kernel_get_thread_id(void) { return (uacpi_thread_id)1; }

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq, uacpi_interrupt_handler f, uacpi_handle ctx,
                                                    uacpi_handle *h)
{
    (void)f; (void)ctx;
    if (verboso) printf("  (SCI en la IRQ %u: no llegará nunca)\n", irq);
    *h = (uacpi_handle)(uintptr_t)(irq + 1);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler f, uacpi_handle h)
{ (void)f; (void)h; return UACPI_STATUS_OK; }

/* Trabajo diferido (Notify y GPE): una cola que se vacía tras cada método. */
typedef struct { uacpi_work_handler f; uacpi_handle ctx; } trabalho_t;
static trabalho_t fila[64];
static int nfila;

uacpi_handle uacpi_kernel_create_work_item(void) { return (uacpi_handle)fila; }
void uacpi_kernel_free_work_item(uacpi_handle h) { (void)h; }
void uacpi_kernel_schedule_work(uacpi_work_type tipo, uacpi_handle item, uacpi_work_handler f, uacpi_handle ctx)
{
    (void)tipo; (void)item;
    if (nfila == 64) { printf("  [arnés] cola de trabajo llena\n"); erros_log++; return; }
    fila[nfila].f = f; fila[nfila].ctx = ctx; nfila++;
}
static void correr_fila(void)
{
    for (int vueltas = 0; nfila && vueltas < 16; vueltas++) {
        trabalho_t t[64];
        int n = nfila;
        memcpy(t, fila, n * sizeof *t);
        nfila = 0;
        for (int i = 0; i < n; i++) t[i].f(t[i].ctx);
    }
}
uacpi_status uacpi_kernel_wait_for_work_completion(void) { correr_fila(); return UACPI_STATUS_OK; }

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *r)
{
    printf("  [arnés] petición del firmware %d\n", (int)r->type);
    return UACPI_STATUS_OK;
}

/* ------------------------------------------------------------ EC y batería */

static uacpi_status regiao_ec(uacpi_region_op op, uacpi_handle dados)
{
    if (op == UACPI_REGION_OP_ATTACH || op == UACPI_REGION_OP_DETACH) return UACPI_STATUS_OK;
    if (op != UACPI_REGION_OP_READ && op != UACPI_REGION_OP_WRITE) return UACPI_STATUS_INVALID_ARGUMENT;
    uacpi_region_rw_data *rw = dados;
    if (rw->byte_width < 1 || rw->byte_width > 8 || rw->offset + rw->byte_width > 256)
        return UACPI_STATUS_INVALID_ARGUMENT;
    if (op == UACPI_REGION_OP_READ) {
        uint64_t v = 0;
        for (unsigned i = 0; i < rw->byte_width; i++) v |= (uint64_t)ec_ram[rw->offset + i] << (8 * i);
        rw->value = v;
        ec_leituras++;
    } else {
        for (unsigned i = 0; i < rw->byte_width; i++) ec_ram[rw->offset + i] = (uint8_t)(rw->value >> (8 * i));
        ec_escritas++;
    }
    return UACPI_STATUS_OK;
}

static uacpi_namespace_node *ec_no, *bats[BATERIA_MAX], *adapt[2];
static int nbats, nadapt;

static uacpi_iteration_decision achar(void *u, uacpi_namespace_node *no, uacpi_u32 prof)
{
    (void)u; (void)prof;
    static const char *const ec_ids[] = { "PNP0C09", NULL };
    if (!ec_no && uacpi_device_matches_pnp_id(no, ec_ids)) ec_no = no;
    return UACPI_ITERATION_DECISION_CONTINUE;
}

static uacpi_status notificacao(uacpi_handle ctx, uacpi_namespace_node *no, uacpi_u64 v)
{
    (void)ctx; (void)no;
    notificacoes++;
    if (verboso) printf("  (Notify %llx)\n", (unsigned long long)v);
    return UACPI_STATUS_OK;
}

static uacpi_iteration_decision achar_bat(void *u, uacpi_namespace_node *no, uacpi_u32 prof)
{
    (void)u; (void)prof;
    static const char *const bat_ids[] = { "PNP0C0A", NULL };
    static const char *const ac_ids[] = { "ACPI0003", NULL };
    const char *tipo = NULL;
    if (nbats < BATERIA_MAX && uacpi_device_matches_pnp_id(no, bat_ids)) { bats[nbats++] = no; tipo = "batería (PNP0C0A)"; }
    else if (nadapt < 2 && uacpi_device_matches_pnp_id(no, ac_ids)) { adapt[nadapt++] = no; tipo = "cargador (ACPI0003)"; }
    if (tipo) {
        uacpi_install_notify_handler(no, notificacao, NULL);
        const char *nome = uacpi_namespace_node_generate_absolute_path(no);
        printf("  %s: %s\n", tipo, nome ? nome : "?");
        if (nome) uacpi_free_absolute_path(nome);
    }
    return UACPI_ITERATION_DECISION_CONTINUE;
}

static uint32_t pkg_int(const uacpi_object_array *a, uint32_t i)
{
    uacpi_u64 v;
    if (i >= a->count || uacpi_object_get_integer(a->objects[i], &v) != UACPI_STATUS_OK) return BATERIA_DESCONHECIDO;
    return v > 0xFFFFFFFFull ? BATERIA_DESCONHECIDO : (uint32_t)v;
}

static const char *estado_txt(uacpi_status r) { return uacpi_status_to_string(r); }

static void ler_bateria(int i, bateria_bruta_t *b)
{
    memset(b, 0, sizeof *b);
    b->existe = true;
    uacpi_u32 sta = 0;
    uacpi_status r = uacpi_eval_sta(bats[i], &sta);
    printf("  BAT%d _STA: %s %x\n", i, estado_txt(r), sta);
    correr_fila();
    b->presente = r == UACPI_STATUS_OK && (sta & 0x10);
    uacpi_object *o = NULL;
    uacpi_object_array a;
    r = uacpi_eval_simple_package(bats[i], "_BIX", &o);
    if (r == UACPI_STATUS_OK && uacpi_object_get_package(o, &a) == UACPI_STATUS_OK && a.count >= 20) {
        b->info_ok = true;
        b->unidade = pkg_int(&a, 1); b->projeto = pkg_int(&a, 2); b->total = pkg_int(&a, 3);
        b->projeto_mv = pkg_int(&a, 5); b->aviso = pkg_int(&a, 6); b->baixo = pkg_int(&a, 7);
        b->ciclos = pkg_int(&a, 8);
        printf("  BAT%d _BIX: ok (%u elementos)\n", i, (unsigned)a.count);
    } else {
        if (r != UACPI_STATUS_NOT_FOUND) printf("  BAT%d _BIX: %s\n", i, estado_txt(r));
        if (o) uacpi_object_unref(o);
        o = NULL;
        r = uacpi_eval_simple_package(bats[i], "_BIF", &o);
        if (r == UACPI_STATUS_OK && uacpi_object_get_package(o, &a) == UACPI_STATUS_OK && a.count >= 13) {
            b->info_ok = true;
            b->unidade = pkg_int(&a, 0); b->projeto = pkg_int(&a, 1); b->total = pkg_int(&a, 2);
            b->projeto_mv = pkg_int(&a, 4); b->aviso = pkg_int(&a, 5); b->baixo = pkg_int(&a, 6);
            b->ciclos = BATERIA_DESCONHECIDO;
            printf("  BAT%d _BIF: ok (%u elementos) unidad %u diseño %u completa %u\n", i, (unsigned)a.count,
                   b->unidade, b->projeto, b->total);
        } else {
            printf("  BAT%d _BIF: %s\n", i, estado_txt(r));
        }
    }
    if (o) uacpi_object_unref(o);
    correr_fila();
    o = NULL;
    r = uacpi_eval_simple_package(bats[i], "_BST", &o);
    if (r == UACPI_STATUS_OK && uacpi_object_get_package(o, &a) == UACPI_STATUS_OK && a.count >= 4) {
        b->estado_ok = true;
        b->estado = pkg_int(&a, 0); b->taxa = pkg_int(&a, 1); b->restante = pkg_int(&a, 2); b->tensao_mv = pkg_int(&a, 3);
        if (b->estado == BATERIA_DESCONHECIDO) b->estado = 0;
        printf("  BAT%d _BST: ok estado %x tasa %x restante %x tension %x\n", i, b->estado, b->taxa, b->restante,
               b->tensao_mv);
    } else {
        printf("  BAT%d _BST: %s\n", i, estado_txt(r));
    }
    if (o) uacpi_object_unref(o);
    correr_fila();
}


/* ------------------------------------------------------------ _PRT (D211) */

/* Las funciones PCI de un chipset de ejemplo: bus, ranura, función, pin
 * (1-4), línea de la BIOS y bus secundario si es un puente. */
typedef struct { uint8_t b, d, f, pino, linha, sec; const char *nome; } fn_ex_t;

static const fn_ex_t ex_945[] = {
    { 0, 0x02, 0, 1, 11, 0, "VGA GMA950" },
    { 0, 0x1B, 0, 1, 11, 0, "HDA" },
    { 0, 0x1C, 0, 1, 11, 2, "puerto PCIe 1" },
    { 0, 0x1C, 1, 2, 10, 3, "puerto PCIe 2" },
    { 0, 0x1C, 2, 3, 11, 4, "puerto PCIe 3" },
    { 0, 0x1C, 3, 4, 11, 5, "puerto PCIe 4" },
    { 2, 0x00, 0, 1, 11, 0, "tras PCIe 1" },
    { 3, 0x00, 0, 1, 10, 0, "tras PCIe 2 (wifi)" },
    { 0, 0x1D, 0, 1, 11, 0, "UHCI 1" },
    { 0, 0x1D, 1, 2, 10, 0, "UHCI 2" },
    { 0, 0x1D, 2, 3, 11, 0, "UHCI 3" },
    { 0, 0x1D, 3, 4, 5, 0, "UHCI 4" },
    { 0, 0x1D, 7, 1, 11, 0, "EHCI" },
    { 0, 0x1E, 0, 0, 0, 6, "puente PCI" },
    { 6, 0x00, 0, 1, 10, 0, "tras 1e: red" },
    { 6, 0x01, 0, 1, 11, 0, "tras 1e: CardBus" },
    { 6, 0x01, 1, 2, 10, 0, "tras 1e: lector SD" },
    { 0, 0x1F, 2, 2, 10, 0, "SATA/IDE" },
    { 0, 0x1F, 3, 2, 10, 0, "SMBus" },
};

static const fn_ex_t ex_hm55[] = {
    { 0, 0x02, 0, 1, 11, 0, "VGA HD" },
    { 0, 0x16, 0, 1, 11, 0, "MEI" },
    { 0, 0x1A, 0, 1, 11, 0, "EHCI 2" },
    { 0, 0x1B, 0, 1, 10, 0, "HDA" },
    { 0, 0x1C, 0, 1, 11, 1, "puerto PCIe 1" },
    { 0, 0x1C, 1, 2, 10, 2, "puerto PCIe 2" },
    { 0, 0x1C, 2, 3, 11, 3, "puerto PCIe 3" },
    { 0, 0x1C, 3, 4, 11, 4, "puerto PCIe 4" },
    { 0, 0x1C, 4, 1, 11, 5, "puerto PCIe 5" },
    { 0, 0x1C, 5, 2, 10, 6, "puerto PCIe 6" },
    { 1, 0x00, 0, 1, 11, 0, "tras PCIe 1" },
    { 2, 0x00, 0, 1, 10, 0, "tras PCIe 2 (wifi)" },
    { 5, 0x00, 0, 1, 11, 0, "tras PCIe 5 (red)" },
    { 0, 0x1D, 0, 1, 11, 0, "EHCI 1" },
    { 0, 0x1E, 0, 0, 0, 7, "puente PCI" },
    { 0, 0x1F, 2, 2, 10, 0, "SATA" },
    { 0, 0x1F, 3, 3, 5, 0, "SMBus" },
    { 0, 0x1F, 6, 3, 5, 0, "termico" },
};

/* Valores típicos del enrutado PIRQ A-H (0x60-0x63 y 0x68-0x6B del LPC):
 * la IRQ ISA de cada PIRQ en el modo PIC (bit 7 = apagado). */
static uint8_t pirq[8] = { 0x0B, 0x0A, 0x0B, 0x05, 0x05, 0x0A, 0x0B, 0x0B };   /* E = 5: el EHCI del Amilo */

static void preparar_lpc(void)
{
    uacpi_pci_address a = { 0, 0, 0x1F, 0 };
    uacpi_handle h;
    if (uacpi_kernel_pci_device_open(a, &h) != UACPI_STATUS_OK) return;
    for (int i = 0; i < 4; i++) {
        uacpi_kernel_pci_write8(h, 0x60 + i, pirq[i]);
        uacpi_kernel_pci_write8(h, 0x68 + i, pirq[4 + i]);
    }
}

static uacpi_iteration_decision escrever_prt(void *u, uacpi_namespace_node *no, uacpi_u32 prof)
{
    (void)u; (void)prof;
    uacpi_namespace_node *m = NULL;
    if (uacpi_namespace_node_find(no, "_PRT", &m) != UACPI_STATUS_OK || !m) return UACPI_ITERATION_DECISION_CONTINUE;
    uacpi_pci_routing_table *t = NULL;
    const char *p = uacpi_namespace_node_generate_absolute_path(no);
    uacpi_status r = uacpi_get_pci_routing_table(no, &t);
    if (r != UACPI_STATUS_OK) {
        printf("  %s._PRT: %s\n", p ? p : "?", estado_txt(r));
    } else {
        printf("  %s._PRT: %zu filas\n", p ? p : "?", (size_t)t->num_entries);
        for (size_t i = 0; i < t->num_entries; i++) {
            uacpi_pci_routing_table_entry *e = &t->entries[i];
            const char *f = e->source ? uacpi_namespace_node_generate_absolute_path(e->source) : NULL;
            printf("    ranura %02x INT%c -> %s %u\n", (unsigned)(e->address >> 16), 'A' + e->pin,
                   e->source ? (f ? f : "?") : "GSI", (unsigned)e->index);
            if (f) uacpi_free_absolute_path(f);
        }
        uacpi_free_pci_routing_table(t);
    }
    if (p) uacpi_free_absolute_path(p);
    return UACPI_ITERATION_DECISION_CONTINUE;
}

static int resolver_exemplo(const fn_ex_t *ex, int n, const char *modo, prt_funcao_t *f)
{
    memset(f, 0, 32 * sizeof *f);
    for (int i = 0; i < n; i++) {
        f[i].bus = ex[i].b; f[i].disp = ex[i].d; f[i].fun = ex[i].f;
        f[i].pino = ex[i].pino; f[i].linha = ex[i].linha; f[i].secundario = ex[i].sec;
    }
    int r = prt_resolver(f, n, false);
    printf("[PRT] modo %s: %d de %d funciones con IRQ, %d _PRT leidos\n", modo, r, n, prt_tabelas_lidas());
    for (int i = 0; i < n; i++) {
        if (!f[i].pino) continue;
        if (f[i].achado)
            printf("[PRT] %02x:%02x.%x INT%c %-20s -> %s%s%s -> %s %u %s activa %s%s\n", f[i].bus, f[i].disp, f[i].fun,
                   'A' + f[i].pino - 1, ex[i].nome, f[i].tabela, f[i].ligacao[0] ? " " : "", f[i].ligacao,
                   strcmp(modo, "PIC") ? "GSI" : "IRQ", f[i].gsi,
                   f[i].nivel ? "nivel" : "flanco", f[i].baixo ? "baja" : "alta",
                   f[i].disp_tabela != f[i].disp || f[i].pino_tabela != f[i].pino ? " (pin girado)" : "");
        else
            printf("[PRT] %02x:%02x.%x INT%c %-20s -> sin IRQ: %s\n", f[i].bus, f[i].disp, f[i].fun,
                   'A' + f[i].pino - 1, ex[i].nome, f[i].erro);
    }
    return r;
}

static int prova_prt(const char *qual)
{
    const fn_ex_t *ex = ex_945;
    int n = (int)(sizeof ex_945 / sizeof ex_945[0]);
    if (!strcmp(qual, "hm55")) { ex = ex_hm55; n = (int)(sizeof ex_hm55 / sizeof ex_hm55[0]); }
    preparar_lpc();
    static prt_funcao_t f[2][32];
    int total = 0;
    for (int modo = 0; modo < 2; modo++) {
        uacpi_status r = uacpi_set_interrupt_model(modo ? UACPI_INTERRUPT_MODEL_IOAPIC : UACPI_INTERRUPT_MODEL_PIC);
        printf("\\_PIC(%d): %s\n", modo, estado_txt(r));
        uacpi_namespace_for_each_child(uacpi_namespace_root(), escrever_prt, NULL, UACPI_OBJECT_DEVICE_BIT,
                                       UACPI_MAX_DEPTH_ANY, NULL);
        int k = resolver_exemplo(ex, n, modo ? "APIC" : "PIC", f[modo]);
        if (modo) total = k;
    }
    /* Coherencia en los chipsets de Intel: el enlace LNKx (o PIRQx) del modo
     * PIC es el PIRQ, y en el modo APIC el PIRQ A-H llega al pin 16-23. */
    int comparadas = 0, iguais = 0;
    for (int i = 0; i < n; i++) {
        const char *l = f[0][i].ligacao;
        size_t t = strlen(l);
        if (!f[1][i].achado || t < 4) continue;
        char letra = l[t - 1];
        if (strncmp(l + t - 4, "LNK", 3) && strncmp(l + t - 4, "PIR", 3)) continue;
        if (letra < 'A' || letra > 'H') continue;
        comparadas++;
        if (f[1][i].gsi == 16u + (uint32_t)(letra - 'A')) iguais++;
        else printf("[PRT] incoherente: %02x:%02x.%x %s en el modo PIC y GSI %u en el APIC\n", f[1][i].bus,
                    f[1][i].disp, f[1][i].fun, l, f[1][i].gsi);
    }
    printf("[PRT] coherencia PIRQ -> GSI 16-23: %d de %d funciones\n", iguais, comparadas);
    return total;
}

static void colgado(int s)
{
    (void)s;
    static const char m[] = "COLGADO: el intérprete no terminó a tiempo\n";
    if (write(1, m, sizeof m - 1) < 0) {}
    _exit(3);
}

/* ------------------------------------------------------------ portátiles (D403) */

static const char *caminho(uacpi_namespace_node *no)
{
    static char b[4][96];
    static int k;
    const char *p = uacpi_namespace_node_generate_absolute_path(no);
    char *d = b[k++ & 3];
    snprintf(d, 96, "%s", p ? p : "?");
    if (p) uacpi_free_absolute_path(p);
    return d;
}

static int tem(uacpi_namespace_node *no, const char *m)
{
    uacpi_namespace_node *x = NULL;
    uacpi_object_type t;
    return uacpi_namespace_node_find(no, m, &x) == UACPI_STATUS_OK && x &&
           uacpi_namespace_node_type(x, &t) == UACPI_STATUS_OK && t == UACPI_OBJECT_METHOD;
}

static uacpi_status eval_ints(uacpi_namespace_node *no, const char *m, const uint64_t *a, int n, uacpi_object **ret)
{
    uacpi_object *o[6];
    for (int i = 0; i < n; i++) o[i] = uacpi_object_create_integer(a[i]);
    uacpi_object_array arr = { o, (uacpi_size)n };
    uacpi_status r = uacpi_eval(no, m, n ? &arr : NULL, ret);
    for (int i = 0; i < n; i++) uacpi_object_unref(o[i]);
    return r;
}

static int saidas_brilho, buses_video, fuj, tos;

static uacpi_iteration_decision p_saida(void *u, uacpi_namespace_node *no, uacpi_u32 prof)
{
    (void)u; (void)prof;
    if (!tem(no, "_BCL") || !tem(no, "_BCM")) return UACPI_ITERATION_DECISION_CONTINUE;
    saidas_brilho++;
    uacpi_object *o = NULL;
    uacpi_object_array a;
    uacpi_status r = uacpi_eval_simple_package(no, "_BCL", &o);
    uint64_t bruto[BRILHO_MAX_NIVEIS + 2];
    int k = 0;
    if (r == UACPI_STATUS_OK && uacpi_object_get_package(o, &a) == UACPI_STATUS_OK)
        for (uacpi_size i = 0; i < a.count && k < BRILHO_MAX_NIVEIS + 2; i++) {
            uacpi_u64 v;
            if (uacpi_object_get_integer(a.objects[i], &v) == UACPI_STATUS_OK) bruto[k++] = v;
        }
    if (o) uacpi_object_unref(o);
    uint32_t niv[BRILHO_MAX_NIVEIS];
    int n = brilho_niveis(bruto, k, niv, BRILHO_MAX_NIVEIS);
    printf("  salida %s: _BCL %s, %d valores, %d niveles", caminho(no), estado_txt(r), k, n);
    if (n >= 2) printf(" (%u-%u)", niv[0], niv[n - 1]);
    if (tem(no, "_BQC")) {
        uacpi_u64 q = 0;
        uacpi_status rq = uacpi_eval_simple_integer(no, "_BQC", &q);
        bool ei = false;
        int i = n >= 2 && rq == UACPI_STATUS_OK ? brilho_bqc(niv, n, q, &ei) : -1;
        printf(", _BQC %s %llu -> nivel %d%s", estado_txt(rq), (unsigned long long)q, i, ei ? " (indice)" : "");
    } else {
        printf(", sin _BQC");
    }
    printf("\n");
    correr_fila();
    return UACPI_ITERATION_DECISION_CONTINUE;
}

static uacpi_iteration_decision p_dispositivo(void *u, uacpi_namespace_node *no, uacpi_u32 prof)
{
    (void)u; (void)prof;
    static const char *const id_fuj[] = { "FUJ02B1", "FUJ02E3", NULL };
    static const char *const id_tos[] = { "TOS1900", "TOS6200", "TOS6207", "TOS6208", NULL };
    uacpi_u32 sta = 0xF;
    uacpi_status rs = uacpi_eval_sta(no, &sta);
    if (tem(no, "_DOD") || tem(no, "_DOS")) {
        buses_video++;
        printf("video %s: _STA %s %x%s%s\n", caminho(no), estado_txt(rs), sta, tem(no, "_DOS") ? ", _DOS" : "",
               tem(no, "_DOD") ? ", _DOD" : "");
        if (tem(no, "_DOS")) {
            uint64_t z = 0;
            uacpi_object *o = NULL;
            uacpi_status r = eval_ints(no, "_DOS", &z, 1, &o);
            if (o) uacpi_object_unref(o);
            printf("  _DOS(0): %s\n", estado_txt(r));
            correr_fila();
        }
        uacpi_namespace_for_each_child(no, p_saida, NULL, UACPI_OBJECT_DEVICE_BIT, 1, NULL);
    }
    if (uacpi_device_matches_pnp_id(no, id_fuj)) {
        fuj++;
        printf("fujitsu %s: _STA %s %x, GBLL %d RBLL %d SBLL %d SBL2 %d FUNC %d\n", caminho(no), estado_txt(rs), sta,
               tem(no, "GBLL"), tem(no, "RBLL"), tem(no, "SBLL"), tem(no, "SBL2"), tem(no, "FUNC"));
        uacpi_u64 v;
        if (tem(no, "RBLL") && uacpi_eval_simple_integer(no, "RBLL", &v) == UACPI_STATUS_OK) printf("  RBLL %llu\n", (unsigned long long)v);
        if (tem(no, "GBLL") && uacpi_eval_simple_integer(no, "GBLL", &v) == UACPI_STATUS_OK) printf("  GBLL %llx\n", (unsigned long long)v);
        if (tem(no, "FUNC")) {
            uint64_t a[4] = { 0x1000, 0, 0, 0 }, b[4] = { 0x1002, 0, 0, 0 };
            uacpi_object *o = NULL;
            uacpi_status r = eval_ints(no, "FUNC", a, 4, &o);
            uacpi_u64 f = 0;
            if (o) { uacpi_object_get_integer(o, &f); uacpi_object_unref(o); o = NULL; }
            uacpi_status r2 = eval_ints(no, "FUNC", b, 4, &o);
            uacpi_u64 bt = 0;
            if (o) { uacpi_object_get_integer(o, &bt); uacpi_object_unref(o); }
            printf("  FUNC(0x1000,0) %s %llx; FUNC(0x1002,0) %s %llx\n", estado_txt(r), (unsigned long long)f,
                   estado_txt(r2), (unsigned long long)bt);
        }
        correr_fila();
    }
    if (uacpi_device_matches_pnp_id(no, id_tos)) {
        tos++;
        const char *m = tem(no, "GHCI") ? "GHCI" : tem(no, "SPFC") ? "SPFC" : NULL;
        printf("toshiba %s: _STA %s %x, HCI %s, INFO %d, ENAB %d\n", caminho(no), estado_txt(rs), sta, m ? m : "no",
               tem(no, "INFO"), tem(no, "ENAB"));
        if (m && (sta & 1)) {
            static const struct { uint32_t reg, extra; const char *n; } regs[] = {
                { 0x002A, 0, "brillo" }, { 0x0004, 0, "ventilador" }, { 0x0056, 1, "radio" }, { 0x0016, 0, "cola" } };
            for (unsigned i = 0; i < 4; i++) {
                uint64_t in[6] = { 0xF300, regs[i].reg, 0, regs[i].extra, 0, 0 };
                uacpi_object *o = NULL;
                uacpi_object_array a;
                uacpi_status r = eval_ints(no, m, in, 6, &o);
                uacpi_u64 r0 = 0xFFFF, r2 = 0;
                if (o && uacpi_object_get_package(o, &a) == UACPI_STATUS_OK && a.count >= 3) {
                    uacpi_object_get_integer(a.objects[0], &r0);
                    uacpi_object_get_integer(a.objects[2], &r2);
                }
                if (o) uacpi_object_unref(o);
                printf("  HCI %s (%04x): %s, resultado %llx valor %llx\n", regs[i].n, regs[i].reg, estado_txt(r),
                       (unsigned long long)r0, (unsigned long long)r2);
            }
            if (tem(no, "ENAB")) {
                uacpi_object *o = NULL;
                uacpi_status r = uacpi_eval(no, "ENAB", NULL, &o);
                if (o) uacpi_object_unref(o);
                printf("  ENAB: %s\n", estado_txt(r));
            }
            if (tem(no, "INFO")) {
                uacpi_u64 v = 0;
                uacpi_status r = uacpi_eval_simple_integer(no, "INFO", &v);
                printf("  INFO: %s %llx\n", estado_txt(r), (unsigned long long)v);
            }
            correr_fila();
        }
    }
    return UACPI_ITERATION_DECISION_CONTINUE;
}

static void prova_portatil(void)
{
    uacpi_namespace_for_each_child(uacpi_namespace_root(), p_dispositivo, NULL, UACPI_OBJECT_DEVICE_BIT,
                                   UACPI_MAX_DEPTH_ANY, NULL);
    printf("portatil: %d dispositivos de video, %d salidas con brillo, %d de Fujitsu, %d de Toshiba\n", buses_video,
           saidas_brilho, fuj, tos);
}

int main(int argc, char **argv)
{
    const char *nome = NULL, *prt = NULL;
    int portatil = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) verboso = 1;
        else if (!strcmp(argv[i], "--portatil")) portatil = 1;
        else if (!strcmp(argv[i], "--io-ff")) io_ff = 1;
        else if (!strcmp(argv[i], "--ec-ff")) ec_ff = 1;
        else if (!strcmp(argv[i], "--limite") && i + 1 < argc) limite_s = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--prt") && i + 1 < argc) prt = argv[++i];
        else if (!strcmp(argv[i], "--pirq") && i + 1 < argc) {
            char *c = argv[++i];
            for (int k = 0; k < 8 && *c; k++) { pirq[k] = (uint8_t)strtoul(c, &c, 16); if (*c == ',') c++; }
        }
        else nome = argv[i];
    }
    if (!nome) { fprintf(stderr, "uso: %s [--io-ff] [--ec-ff] [--limite S] [-v] [--prt 945|hm55] [--pirq a,b,...] [--portatil] volcado.txt\n", argv[0]); return 2; }
    setvbuf(stdout, NULL, _IOLBF, 0);
    mem = mmap(NULL, MEM_BYTES, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (mem == MAP_FAILED) { perror("mmap"); return 2; }
    if (ec_ff) memset(ec_ram, 0xFF, sizeof ec_ram);
    if (io_ff) memset(portas, 0xFF, sizeof portas);
    if (ler_volcado(nome) <= 0) { fprintf(stderr, "%s: sin tablas\n", nome); return 2; }
    int n = montar();
    if (n < 0) return 2;
    printf("%s: %d tablas en el volcado, %d en la XSDT\n", nome, ntab, n);
    signal(SIGALRM, colgado);
    alarm(limite_s);

    uacpi_context_set_log_level(verboso ? UACPI_LOG_INFO : UACPI_LOG_WARN);
    uacpi_status r = uacpi_initialize(UACPI_FLAG_NO_ACPI_MODE);
    printf("uacpi_initialize: %s\n", estado_txt(r));
    if (r != UACPI_STATUS_OK) return 1;
    r = uacpi_namespace_load();
    printf("uacpi_namespace_load: %s\n", estado_txt(r));
    if (r != UACPI_STATUS_OK) return 1;
    uacpi_namespace_for_each_child(uacpi_namespace_root(), achar, NULL, UACPI_OBJECT_DEVICE_BIT,
                                   UACPI_MAX_DEPTH_ANY, NULL);
    if (ec_no) {
        const char *p = uacpi_namespace_node_generate_absolute_path(ec_no);
        r = uacpi_install_address_space_handler(ec_no, UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER, regiao_ec, NULL);
        printf("EC %s: manejador %s\n", p ? p : "?", estado_txt(r));
        if (p) uacpi_free_absolute_path(p);
    } else {
        printf("EC: no hay PNP0C09\n");
    }
    r = uacpi_namespace_initialize();
    printf("uacpi_namespace_initialize: %s\n", estado_txt(r));
    correr_fila();
    if (r != UACPI_STATUS_OK) return 1;
    uacpi_namespace_for_each_child(uacpi_namespace_root(), achar_bat, NULL, UACPI_OBJECT_DEVICE_BIT,
                                   UACPI_MAX_DEPTH_ANY, NULL);
    if (portatil) {
        prova_portatil();
        alarm(0);
        printf("fin: %u errores y %u avisos de uACPI; Notify %u\n", erros_log, avisos_log, notificacoes);
        printf("SIN COLGARSE\n");
        return 0;
    }
    if (prt) {
        int k = prova_prt(prt);
        alarm(0);
        printf("fin: %u errores y %u avisos de uACPI; %d funciones con GSI en el modo APIC\n", erros_log, avisos_log, k);
        printf("SIN COLGARSE\n");
        return 0;
    }
    if (!nbats && !nadapt) printf("no hay batería ni cargador ACPI\n");
    /* Tres vueltas, como tres lecturas de la tarea (cada 5 s). */
    for (int vuelta = 0; vuelta < 3; vuelta++) {
        uint32_t adaptador = nadapt ? 1 : 0;
        for (int i = 0; i < nadapt; i++) {
            uacpi_u64 v = 0;
            r = uacpi_eval_simple_integer(adapt[i], "_PSR", &v);
            if (vuelta == 0) printf("  ADP%d _PSR: %s %llu\n", i, estado_txt(r), (unsigned long long)v);
            if (r == UACPI_STATUS_OK) { adaptador = v ? 2 : 1; break; }
        }
        correr_fila();
        bateria_bruta_t b[BATERIA_MAX];
        for (int i = 0; i < nbats; i++) {
            if (vuelta == 0) ler_bateria(i, &b[i]);
            else {
                /* Solo _STA y _BST, como la tarea después de la primera. */
                uacpi_u32 sta = 0;
                uacpi_eval_sta(bats[i], &sta);
                uacpi_object *o = NULL;
                uacpi_eval_simple_package(bats[i], "_BST", &o);
                if (o) uacpi_object_unref(o);
                correr_fila();
            }
        }
        relogio_ns += 5000000000ull;
        if (vuelta == 0 && nbats) {
            info_bateria_t s;
            memset(&s, 0, sizeof s);
            bateria_calcular(b, nbats, adaptador, &s);
            char t[32];
            bateria_texto_tempo(s.minutos, t, sizeof t);
            printf("resultado: presente %u porcentaje %d tiempo '%s' estado %x cargador %u\n", s.presente,
                   s.porcentagem, t, s.estado, adaptador);
        }
    }
    alarm(0);
    printf("fin: %u errores y %u avisos de uACPI; EC %u lecturas %u escrituras; E/S %u; PCI %u; Notify %u; "
           "reloj virtual %llu ms\n", erros_log, avisos_log, ec_leituras, ec_escritas, io_acessos, pci_acessos,
           notificacoes, (unsigned long long)(relogio_ns / 1000000ull));
    printf("SIN COLGARSE\n");
    return 0;
}
