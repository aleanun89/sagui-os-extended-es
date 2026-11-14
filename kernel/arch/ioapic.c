// SPDX-License-Identifier: GPL-2.0-or-later
/* ioapic.c - el IOAPIC: las IRQ de los dispositivos sin el 8259 (D166).
 *
 * Tres partes:
 *   1. leer del firmware lo que hace falta (antes de la paginación): los
 *      IOAPIC y las "Interrupt Source Override" de la MADT, la IRQ de la SCI
 *      de la FADT y, de la tabla MP, si la placa tiene IMCR;
 *   2. el acceso a los registros del IOAPIC (IOREGSEL/IOWIN) y a su tabla de
 *      redirección;
 *   3. el cambio del PIC al IOAPIC, con vuelta atrás si el tic del PIT no
 *      llega por el camino nuevo.
 *
 * Los vectores no cambian (IRQ n -> 0x20 + n): la IDT, irq_registrar() y los
 * drivers no saben si la IRQ llegó por el PIC o por el IOAPIC. Lo único que
 * cambia en el despachador es el fin de interrupción (EOI al APIC local en
 * vez de al 8259) y que no hay que mirar las espurias del 8259. */
#include "ioapic.h"
#include "lapic.h"
#include "cpu.h"
#include "cpu_dados.h"
#include "portas.h"
#include "console.h"
#include "texto.h"
#include "info_hardware.h"
#include "relogio.h"
#include "msr.h"

/* Registros del IOAPIC: se escribe el índice en IOREGSEL (+0x00) y se lee o
 * escribe el valor en IOWIN (+0x10). */
#define IOREGSEL 0x00u
#define IOWIN    0x10u
#define REG_ID   0x00u
#define REG_VER  0x01u
#define REG_RED  0x10u   /* entrada n: 0x10 + 2n (baja) y 0x11 + 2n (alta) */

/* La parte baja de una entrada de la tabla de redirección. */
#define RED_MASCARA 0x10000u   /* bit 16: enmascarada */
#define RED_NIVEL   0x08000u   /* bit 15: disparo por nivel (0 = flanco) */
#define RED_BAIXO   0x02000u   /* bit 13: activa a nivel bajo (0 = alto) */
/* Modo de entrega fijo (bits 8-10 a 0) y destino físico (bit 11 a 0). */

/* Las banderas MPS INTI de una override de la MADT. */
#define INTI_POL_MASC  0x3u
#define INTI_POL_ALTO  0x1u
#define INTI_POL_BAIXO 0x3u
#define INTI_DISP_MASC 0xCu
#define INTI_DISP_FLANCO 0x4u
#define INTI_DISP_NIVEL  0xCu

#define SEM_GSI 0xFFFFFFFFu

typedef struct {
    uint8_t  id;          /* el identificador del IOAPIC (el de la MADT) */
    uint32_t fisico;      /* dirección física de sus registros */
    uint32_t gsi_base;    /* primer GSI que atiende */
    uint32_t entradas;    /* cuántas líneas tiene (se lee del registro VER) */
    uint32_t versao;
} ioapic_t;

static ioapic_t ioapics[IOAPIC_MAX];
static uint32_t n_ioapic;

/* Cada IRQ ISA: su GSI y su forma (polaridad y disparo). */
static struct {
    uint32_t gsi;
    bool override;      /* lo dijo la MADT */
    bool nivel;         /* disparo por nivel */
    bool baixo;         /* activa a nivel bajo */
} isa[16];

/* Lo que se leyó de las tablas, para el log (que no existe todavía cuando se
 * leen: es antes de la paginación, pero la serial ya está). */
static uint32_t n_overrides, n_nmi, sci_irq = SEM_GSI;
static bool pcat_compat, tem_imcr;
static bool ativo;

/* La entrada que tiene cada IRQ ISA en el IOAPIC, una vez programada (para
 * enmascarar y cambiar el destino sin volver a calcular nada). */
static int8_t isa_ioapic[16];
static uint8_t isa_pino[16];
static uint8_t destino_cpu0;

/* D211: cada línea (IRQ 0-15, vector 0x20 + n) puede llegar por más de una
 * entrada: la ISA de siempre y las GSI que el _PRT da a sus funciones PCI
 * (16-23 en los Intel). Todas llevan el mismo vector, así que los drivers
 * no se enteran. Y una línea puede entrar además por el 8259, en "modo
 * mixto": LINT0 de la CPU 0 en ExtINT y la línea desenmascarada en el PIC,
 * como vuelta atrás de la autoprueba (irq_rotas.c). */
typedef struct {
    uint32_t gsi;
    uint8_t  ioapic, pino, linha;
    bool     nivel, baixo;
    bool     ligada;
} rota_t;

static rota_t rotas[IOAPIC_ROTAS_MAX];
static uint32_t n_rotas;
static uint16_t quer;           /* las líneas que los drivers tienen habilitadas */
static uint16_t isa_desviada;   /* la entrada ISA de la línea, siempre enmascarada */
static uint16_t via_pic;        /* las líneas que entran también por el 8259 */
static bool pic_usado;          /* el 8259 se tocó alguna vez (si no, ni se lee) */
static uint8_t destino[16];

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

uint32_t ioapic_quantidade(void) { return n_ioapic; }
uint32_t ioapic_fisico(uint32_t i) { return i < n_ioapic ? ioapics[i].fisico : 0; }
bool ioapic_ativo(void) { return ativo; }

/* ------------------------------------------------------------ las tablas */

/* Una entrada de la MADT: se guarda lo que interesa. */
static void madt_entrada(const uint8_t *e, uint8_t tipo, uint8_t len)
{
    if (tipo == 1 && len >= 12) {                 /* IOAPIC */
        if (n_ioapic < IOAPIC_MAX) {
            ioapic_t *a = &ioapics[n_ioapic++];
            a->id = e[2];
            a->fisico = le32(e + 4);
            a->gsi_base = le32(e + 8);
        }
    } else if (tipo == 2 && len >= 10) {          /* Interrupt Source Override */
        uint8_t barramento = e[2], origem = e[3];
        uint32_t gsi = le32(e + 4);
        uint16_t f = le16(e + 8);
        if (barramento != 0 || origem >= 16) return;   /* solo ISA */
        n_overrides++;
        isa[origem].gsi = gsi;
        isa[origem].override = true;
        /* "Conforme al bus" (0) es, en ISA, activa alta y por flanco. */
        isa[origem].baixo = (f & INTI_POL_MASC) == INTI_POL_BAIXO;
        isa[origem].nivel = (f & INTI_DISP_MASC) == INTI_DISP_NIVEL;
    } else if (tipo == 3 && len >= 8) {           /* NMI source del IOAPIC */
        n_nmi++;
    } else if (tipo == 4 && len >= 6) {           /* NMI de un APIC local (LINTn) */
        n_nmi++;
    }
}

/* Recorre la MADT. Como en cpu_dados.c, no toda BIOS pone las entradas en
 * el desplazamiento 44: se prueban los mismos y vale el primero que da IOAPIC. */
static void ler_madt(const uint8_t *t, uint32_t tam)
{
    static const uint32_t desl[] = { 44, 52, 48, 36 };
    if (tam >= 44) pcat_compat = (le32(t + 40) & 1) != 0;
    for (uint32_t c = 0; c < sizeof desl / sizeof *desl; c++) {
        n_ioapic = 0; n_overrides = 0; n_nmi = 0;
        for (int i = 0; i < 16; i++) {
            isa[i].gsi = (uint32_t)i;
            isa[i].override = isa[i].nivel = isa[i].baixo = false;
        }
        uint32_t off = desl[c];
        while (off + 2 <= tam) {
            uint8_t tipo = t[off], len = t[off + 1];
            if (len < 2 || off + len > tam) break;
            madt_entrada(t + off, tipo, len);
            off += len;
        }
        if (n_ioapic) return;
    }
}

/* La FADT ("FACP") dice en qué IRQ está la SCI (desplazamiento 46, 16 bits).
 * Si la MADT no tiene override para ella, la SCI es por nivel y activa baja
 * (lo dice la especificación del ACPI), no como una ISA normal. */
static void ler_fadt(const uint8_t *t, uint32_t tam)
{
    if (tam >= 48) sci_irq = le16(t + 46);
}

/* La tabla MP de Intel (la de antes del ACPI): solo para un bit, el IMCRP
 * (bit 7 del segundo byte de funciones). Si está, la placa arranca en "modo
 * PIC" y hay que mover el IMCR para que las IRQ lleguen al IOAPIC. */
static bool mp_em(uint32_t ini, uint32_t tam)
{
    for (uint32_t p = ini; p + 16 <= ini + tam; p += 16) {
        const uint8_t *m = (const uint8_t *)p;
        if (m[0] != '_' || m[1] != 'M' || m[2] != 'P' || m[3] != '_') continue;
        uint8_t soma = 0;
        for (int i = 0; i < 16; i++) soma = (uint8_t)(soma + m[i]);
        if (soma) continue;
        tem_imcr = (m[12] & 0x80) != 0;
        return true;
    }
    return false;
}

static void ler_mp(void)
{
    /* El segmento de la EBDA está en 0x40E (zona de datos de la BIOS). La
     * dirección pasa por un registro para que el GCC no la tome por un
     * puntero nulo con desplazamiento (antes de la paginación sí se puede leer). */
    uint32_t endereco = 0x40E;
    __asm__("" : "+r"(endereco));
    uint32_t ebda = (uint32_t)(*(const volatile uint16_t *)endereco) << 4;
    if (ebda >= 0x80000 && ebda < 0xA0000 && mp_em(ebda, 1024)) return;
    if (mp_em(0x9FC00, 1024)) return;
    mp_em(0xF0000, 0x10000);
}

void ioapic_capturar(void)
{
    n_ioapic = 0;
    for (int i = 0; i < 16; i++) { isa[i].gsi = (uint32_t)i; isa_ioapic[i] = -1; }
    const info_acpi_t *a = hw_acpi();
    if (!a || !a->achou) return;
    for (uint32_t i = 0; i < a->qtd; i++) {
        const uint8_t *t = (const uint8_t *)a->tabela[i].endereco;
        uint32_t tam = a->tabela[i].tamanho;
        if (!t || tam < 36 || tam > (1u << 20)) continue;
        if (memcmp(a->tabela[i].assinatura, "APIC", 4) == 0)
            ler_madt(t, tam);
        else if (memcmp(a->tabela[i].assinatura, "FACP", 4) == 0)
            ler_fadt(t, tam);
    }
    if (n_ioapic) ler_mp();
}

/* ------------------------------------------------------------ registros */

static volatile uint32_t *regs(uint32_t i)
{
    return (volatile uint32_t *)(IOAPIC_VIRT + i * 0x1000u + (ioapics[i].fisico & 0xFFFu));
}

/* El par índice/valor no es atómico: siempre con las interrupciones
 * desactivadas (y solo la CPU 0 lo toca, D164). */
static uint32_t ler(uint32_t i, uint32_t reg)
{
    volatile uint32_t *r = regs(i);
    r[IOREGSEL / 4] = reg;
    return r[IOWIN / 4];
}

static void escrever(uint32_t i, uint32_t reg, uint32_t v)
{
    volatile uint32_t *r = regs(i);
    r[IOREGSEL / 4] = reg;
    r[IOWIN / 4] = v;
}

/* Qué IOAPIC y qué pin atienden un GSI (-1 si ninguno). */
static int achar_gsi(uint32_t gsi, uint8_t *pino)
{
    for (uint32_t i = 0; i < n_ioapic; i++)
        if (gsi >= ioapics[i].gsi_base && gsi < ioapics[i].gsi_base + ioapics[i].entradas) {
            *pino = (uint8_t)(gsi - ioapics[i].gsi_base);
            return (int)i;
        }
    return -1;
}

static void entrada_escrever(uint32_t i, uint8_t pino, uint32_t baixa, uint8_t destino)
{
    /* Primero la parte baja enmascarada, después el destino y por último la
     * parte baja de verdad: nunca hay un instante con la entrada a medias y
     * sin máscara. */
    escrever(i, REG_RED + 2u * pino, baixa | RED_MASCARA);
    escrever(i, REG_RED + 2u * pino + 1, (uint32_t)destino << 24);
    escrever(i, REG_RED + 2u * pino, baixa);
}

static bool tem_rota(uint8_t irq)
{
    for (uint32_t r = 0; r < n_rotas; r++)
        if (rotas[r].linha == irq) return true;
    return false;
}

/* Escribe en el hardware lo que toca a la línea "irq": su entrada ISA, sus
 * rutas del _PRT y su bit en el 8259. Con las interrupciones desactivadas. */
static void aplicar_linha(uint8_t irq)
{
    uint16_t b = (uint16_t)(1u << irq);
    bool q = (quer & b) != 0;
    if (isa_ioapic[irq] >= 0) {
        uint32_t i = (uint32_t)isa_ioapic[irq];
        uint32_t baixa = ler(i, REG_RED + 2u * isa_pino[irq]) & ~RED_MASCARA;
        if (!q || (isa_desviada & b) || irq == 2) baixa |= RED_MASCARA;
        entrada_escrever(i, isa_pino[irq], baixa, destino[irq]);
    }
    for (uint32_t r = 0; r < n_rotas; r++) {
        rota_t *t = &rotas[r];
        if (t->linha != irq) continue;
        uint32_t baixa = (VETOR_IRQ0 + (uint32_t)irq) | (t->nivel ? RED_NIVEL : 0) | (t->baixo ? RED_BAIXO : 0);
        if (!q || !t->ligada) baixa |= RED_MASCARA;
        entrada_escrever(t->ioapic, t->pino, baixa, destino[irq]);
    }
    if (pic_usado) {
        uint16_t abertas = quer & via_pic;
        uint8_t m1 = (uint8_t)~(abertas & 0xFF), m2 = (uint8_t)~(abertas >> 8);
        if (abertas & 0xFF00) m1 &= (uint8_t)~(1u << 2);   /* la cascada del esclavo */
        porta_escreve8(0x21, m1);
        porta_escreve8(0xA1, m2);
    }
}

void ioapic_mascarar(uint8_t irq, bool mascarada)
{
    if (!ativo || irq >= 16) return;
    bool f = int_salva_desliga();
    if (mascarada) quer &= (uint16_t)~(1u << irq);
    else quer |= (uint16_t)(1u << irq);
    aplicar_linha(irq);
    int_restaura(f);
}

bool ioapic_destino(uint8_t irq, uint8_t apic_id)
{
    if (!ativo || irq >= 16 || (isa_ioapic[irq] < 0 && !tem_rota(irq))) return false;
    bool f = int_salva_desliga();
    destino[irq] = apic_id;
    aplicar_linha(irq);
    int_restaura(f);
    return true;
}

uint8_t ioapic_destino_atual(uint8_t irq) { return irq < 16 ? destino[irq] : 0; }
uint8_t ioapic_apic_cpu0(void) { return destino_cpu0; }
uint32_t ioapic_sci(void) { return sci_irq; }

uint32_t ioapic_gsi_isa(uint8_t irq)
{
    if (!ativo || irq >= 16 || isa_ioapic[irq] < 0) return IOAPIC_SEM_GSI;
    return isa[irq].gsi;
}

/* ------------------------------------------------------------ rutas (D211) */

int ioapic_rota_criar(uint8_t irq, uint32_t gsi, bool nivel, bool baixo)
{
    if (!ativo || irq >= 16 || irq == 2) return IOAPIC_ROTA_ERRO;
    if (isa_ioapic[irq] >= 0 && isa[irq].gsi == gsi) return IOAPIC_ROTA_ISA;
    uint8_t pino;
    int i = achar_gsi(gsi, &pino);
    if (i < 0) return IOAPIC_ROTA_ERRO;
    /* La GSI no puede ser la entrada ISA de otra línea... */
    for (int x = 0; x < 16; x++)
        if (x != irq && isa_ioapic[x] >= 0 && isa[x].gsi == gsi) return IOAPIC_ROTA_ERRO;
    /* ... ni de una ruta de otra línea (dos líneas PIC en el mismo PIRQ es
     * una BIOS que se contradice). */
    for (uint32_t r = 0; r < n_rotas; r++)
        if (rotas[r].gsi == gsi) return rotas[r].linha == irq ? (int)r : IOAPIC_ROTA_ERRO;
    if (n_rotas >= IOAPIC_ROTAS_MAX) return IOAPIC_ROTA_ERRO;
    bool f = int_salva_desliga();
    rota_t *t = &rotas[n_rotas];
    t->gsi = gsi;
    t->ioapic = (uint8_t)i;
    t->pino = pino;
    t->linha = irq;
    t->nivel = nivel;
    t->baixo = baixo;
    t->ligada = false;
    n_rotas++;
    aplicar_linha(irq);
    int_restaura(f);
    return (int)(n_rotas - 1);
}

void ioapic_rota_ligar(int r, bool ligada)
{
    if (r < 0 || (uint32_t)r >= n_rotas) return;
    bool f = int_salva_desliga();
    rotas[r].ligada = ligada;
    aplicar_linha(rotas[r].linha);
    int_restaura(f);
}

bool ioapic_rota_info(int r, uint32_t *gsi, uint8_t *linha, bool *ligada)
{
    if (r < 0 || (uint32_t)r >= n_rotas) return false;
    if (gsi) *gsi = rotas[r].gsi;
    if (linha) *linha = rotas[r].linha;
    if (ligada) *ligada = rotas[r].ligada;
    return true;
}

void ioapic_isa_desviar(uint8_t irq, bool desviada)
{
    if (!ativo || irq >= 16) return;
    bool f = int_salva_desliga();
    if (desviada) isa_desviada |= (uint16_t)(1u << irq);
    else isa_desviada &= (uint16_t)~(1u << irq);
    aplicar_linha(irq);
    int_restaura(f);
}

bool ioapic_isa_desviada(uint8_t irq) { return irq < 16 && (isa_desviada & (1u << irq)); }

/* LINT0 de la CPU 0: ExtINT (el "cable virtual" del 8259) si alguna línea
 * entra por el PIC; enmascarada si no. Solo desde la CPU 0: la dirección
 * del APIC local es la de la CPU que ejecuta. */
static void atualizar_lint0(void)
{
    lapic_escrever(LAPIC_LINT0, via_pic ? 0x700u : (0x10000u | 0x700u));
}

bool ioapic_via_pic(uint8_t irq, bool pic)
{
    if (!ativo || irq >= 16 || irq == 2 || cpu_atual() != 0) return false;
    bool f = int_salva_desliga();
    if (pic) via_pic |= (uint16_t)(1u << irq);
    else via_pic &= (uint16_t)~(1u << irq);
    pic_usado = true;
    aplicar_linha(irq);
    atualizar_lint0();
    int_restaura(f);
    return true;
}

uint16_t ioapic_linhas_pic(void) { return ativo ? via_pic : 0; }

void ioapic_estado_salvar(ioapic_estado_t *e)
{
    e->isa_desviada = isa_desviada;
    e->via_pic = via_pic;
    e->rotas_ligadas = 0;
    for (uint32_t r = 0; r < n_rotas; r++)
        if (rotas[r].ligada) e->rotas_ligadas |= 1u << r;
}

void ioapic_estado_restaurar(const ioapic_estado_t *e)
{
    if (!ativo || cpu_atual() != 0) return;
    bool f = int_salva_desliga();
    isa_desviada = e->isa_desviada;
    if (e->via_pic) pic_usado = true;
    via_pic = e->via_pic;
    for (uint32_t r = 0; r < n_rotas; r++) rotas[r].ligada = (e->rotas_ligadas >> r) & 1;
    for (uint8_t irq = 0; irq < 16; irq++) aplicar_linha(irq);
    atualizar_lint0();
    int_restaura(f);
}

/* Un pin del IOAPIC de la GSI 16 en adelante que no use nadie: la prueba de
 * fallo (CAB_IRQ_PRUEBA) da ahí una GSI equivocada. */
bool ioapic_gsi_livre(uint32_t *gsi, const uint32_t *evitar, int n_evitar)
{
    for (uint32_t i = 0; i < n_ioapic; i++)
        for (uint32_t p = ioapics[i].entradas; p-- > 0;) {
            uint32_t g = ioapics[i].gsi_base + p;
            if (g < 16) break;
            bool usada = false;
            for (uint32_t r = 0; r < n_rotas; r++) if (rotas[r].gsi == g) usada = true;
            for (int e = 0; e < n_evitar; e++) if (evitar[e] == g) usada = true;
            if (!usada) { *gsi = g; return true; }
        }
    return false;
}

/* D168: con el cerrojo general (D165) cualquier CPU puede atender una IRQ,
 * así que las de los dispositivos se reparten por turnos entre los
 * secundarios. El PIT (IRQ 0) se queda en la CPU 0: es su tic y el de las
 * tareas del kernel. La IRQ 2 es la cascada del 8259 y no tiene entrada. */
uint32_t ioapic_repartir(void)
{
    if (!ativo) return 0;
    uint8_t ids[CPU_MAX];
    uint32_t n = 0;
    for (uint32_t c = 1; c < CPU_MAX; c++) {
        cpu_dados_t *d = cpu_dados(c);
        if (d && d->estado != CPU_LIVRE) ids[n++] = (uint8_t)d->apic_id;
    }
    if (!n) return 0;
    uint32_t movidas = 0, turno = 0;
    for (uint8_t irq = 1; irq < 16; irq++) {
        if (irq == 2 || (isa_ioapic[irq] < 0 && !tem_rota(irq))) continue;
        uint8_t destino = ids[turno++ % n];
        if (ioapic_destino(irq, destino)) {
            klog("[IOAPIC] IRQ %u -> APIC %u\n", irq, destino);
            movidas++;
        }
    }
    klog("[IOAPIC] reparto: %u IRQ entre %u CPU secundarias (la IRQ 0 sigue en la CPU 0)\n",
         movidas, n);
    return movidas;
}

/* ------------------------------------------------------------ el cambio */

/* El ELCR (0x4D0/0x4D1) del chipset: la BIOS marca ahí qué IRQ ISA son por
 * nivel (las de PCI). Las 0, 1, 2, 8 y 13 nunca pueden serlo: si lo
 * parecen, el registro no existe (se lee 0xFF) y no se usa. */
static uint16_t ler_elcr(void)
{
    uint16_t v = (uint16_t)(porta_le8(0x4D0) | (porta_le8(0x4D1) << 8));
    if (v & ((1u << 0) | (1u << 1) | (1u << 2) | (1u << 8) | (1u << 13)))
        return 0;
    return v;
}

/* El IMCR (puertos 0x22/0x23, registro 0x70): 1 = las IRQ van al APIC,
 * 0 = al PIC por el procesador (modo PIC de la tabla MP). */
static void imcr(uint8_t v)
{
    porta_escreve8(0x22, 0x70);
    porta_escreve8(0x23, v);
}

static inline uint64_t ler_tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* El APIC local tiene que estar presente (CPUID) y no apagado en el MSR
 * IA32_APIC_BASE (bit 11): si la BIOS lo apagó ahí, ni su MMIO responde. */
static bool lapic_utilizavel(void)
{
    uint32_t a = 1, b, c, d;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "=c"(c), "=d"(d));
    if (!(d & (1u << 9))) return false;
    uint64_t base;
    if (msr_ler_seguro(0x1B, &base) && !(base & (1u << 11))) return false;
    return true;
}

/* Vuelve al PIC: todas las entradas del IOAPIC enmascaradas, la máscara del
 * 8259 como estaba, LINT0 otra vez en ExtINT y el IMCR al PIC. */
static void voltar_ao_pic(uint8_t m1, uint8_t m2)
{
    bool f = int_salva_desliga();
    ativo = false;
    for (uint32_t i = 0; i < n_ioapic; i++)
        for (uint32_t p = 0; p < ioapics[i].entradas; p++)
            escrever(i, REG_RED + 2u * p, RED_MASCARA);
    if (tem_imcr) imcr(0x00);
    lapic_escrever(LAPIC_LINT0, 0x700);
    porta_escreve8(0x21, m1);
    porta_escreve8(0xA1, m2);
    int_restaura(f);
}

bool ioapic_ativar(void)
{
    if (!n_ioapic) {
        klog("[IOAPIC] la MADT no declara ningun IOAPIC: se queda el PIC 8259\n");
        return false;
    }
    if (!lapic_utilizavel()) {
        klog("[IOAPIC] sin APIC local utilizable (CPUID o MSR): se queda el PIC 8259\n");
        return false;
    }

    bool f = int_salva_desliga();

    /* Lo que hay en cada IOAPIC. */
    for (uint32_t i = 0; i < n_ioapic; i++) {
        uint32_t ver = ler(i, REG_VER);
        ioapics[i].versao = ver & 0xFF;
        ioapics[i].entradas = ((ver >> 16) & 0xFF) + 1;
        if (ioapics[i].entradas > 240) ioapics[i].entradas = 24;   /* MMIO que no responde (0xFFFFFFFF) */
        klog("[IOAPIC] IOAPIC %u: id %u, fisico %x, version %x, %u entradas, GSI %u-%u\n",
             i, ioapics[i].id, ioapics[i].fisico, ioapics[i].versao, ioapics[i].entradas,
             ioapics[i].gsi_base, ioapics[i].gsi_base + ioapics[i].entradas - 1);
        if (ver == 0xFFFFFFFFu) {
            int_restaura(f);
            klog("[IOAPIC] el IOAPIC %u no responde: se queda el PIC 8259\n", i);
            return false;
        }
        for (uint32_t p = 0; p < ioapics[i].entradas; p++)
            escrever(i, REG_RED + 2u * p, RED_MASCARA);
    }
    klog("[IOAPIC] MADT: %u overrides, %u fuentes NMI, PCAT_COMPAT %u, SCI en la IRQ %d, IMCR %s\n",
         n_overrides, n_nmi, pcat_compat ? 1 : 0, sci_irq == SEM_GSI ? -1 : (int)sci_irq,
         tem_imcr ? "si" : "no");

    /* Lo que el PIC tenía habilitado pasa al IOAPIC tal cual. */
    uint8_t m1 = porta_le8(0x21), m2 = porta_le8(0xA1);
    uint16_t habilitadas = (uint16_t)~(m1 | (m2 << 8));
    uint16_t elcr = ler_elcr();
    destino_cpu0 = (uint8_t)lapic_id();
    quer = habilitadas;
    for (int irq = 0; irq < 16; irq++) destino[irq] = destino_cpu0;

    /* Un GSI que ocupa la override de otra IRQ deja de ser de la IRQ con su
     * número (la IRQ 0 en el GSI 2 se lleva la línea de la cascada). */
    bool gsi_tomado[16] = { false };
    for (int irq = 0; irq < 16; irq++)
        if (isa[irq].override && isa[irq].gsi < 16 && isa[irq].gsi != (uint32_t)irq)
            gsi_tomado[isa[irq].gsi] = true;

    for (int irq = 0; irq < 16; irq++) {
        isa_ioapic[irq] = -1;
        if (irq == 2 && !isa[2].override) continue;          /* la cascada del 8259 */
        if (!isa[irq].override && gsi_tomado[irq]) continue;
        bool nivel = isa[irq].nivel, baixo = isa[irq].baixo;
        if (!isa[irq].override) {
            if ((uint32_t)irq == sci_irq) {
                nivel = true; baixo = true;          /* la SCI sin override (ACPI) */
            } else if (elcr & (1u << irq)) {
                nivel = true; baixo = false;         /* PCI en una IRQ ISA (ver la D166) */
            }
        }
        uint8_t pino;
        int i = achar_gsi(isa[irq].gsi, &pino);
        if (i < 0) continue;
        isa_ioapic[irq] = (int8_t)i;
        isa_pino[irq] = pino;
        uint32_t baixa = (VETOR_IRQ0 + (uint32_t)irq) | (nivel ? RED_NIVEL : 0) | (baixo ? RED_BAIXO : 0);
        if (!(habilitadas & (1u << irq)) || irq == 2) baixa |= RED_MASCARA;
        entrada_escrever((uint32_t)i, pino, baixa, destino_cpu0);
        if (isa[irq].override || nivel)
            klog("[IOAPIC] IRQ %d -> GSI %u (pin %u), %s, activa %s%s\n", irq, isa[irq].gsi, pino,
                 nivel ? "nivel" : "flanco", baixo ? "baja" : "alta", isa[irq].override ? " (override)" : "");
    }

    /* El APIC local de la CPU 0: activo, con su vector espurio, la prioridad
     * a 0 (lo acepta todo), LINT0 enmascarada (el 8259 ya no entrega nada) y
     * LINT1 en NMI. */
    lapic_escrever(LAPIC_TPR, 0);
    lapic_escrever(LAPIC_SVR, LAPIC_ESPURIO | 0x100);
    lapic_escrever(LAPIC_LINT0, 0x10000 | 0x700);
    lapic_escrever(LAPIC_LINT1, 0x400);
    if (tem_imcr) imcr(0x01);
    porta_escreve8(0x21, 0xFF);   /* el 8259, todo enmascarado */
    porta_escreve8(0xA1, 0xFF);
    ativo = true;
    int_restaura(f);

    /* ¿Llega el tic? 100 ms son 10 tics; con 3 basta. Si no, vuelta al PIC:
     * mejor sin IOAPIC que sin reloj. */
    uint32_t khz = hw_tsc_khz();
    uint32_t t0 = relogio_tiques();
    bool chegou = false;
    if (khz) {
        uint64_t alvo = ler_tsc() + (uint64_t)khz * 100;
        while (ler_tsc() < alvo)
            if (relogio_tiques() - t0 >= 3) { chegou = true; break; }
    } else {
        for (volatile uint32_t k = 0; k < 200000000u && !chegou; k++)
            if (relogio_tiques() - t0 >= 3) chegou = true;
    }
    if (!chegou) {
        voltar_ao_pic(m1, m2);
        klog("[IOAPIC] el tic del PIT no llega por el IOAPIC: vuelta al PIC 8259\n");
        return false;
    }
    klog("[IOAPIC] activo: las IRQ 0-15 van por el IOAPIC a la CPU 0 (APIC %u), el 8259 enmascarado\n",
         destino_cpu0);
    return true;
}
