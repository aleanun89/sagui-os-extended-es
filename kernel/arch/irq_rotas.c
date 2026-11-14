// SPDX-License-Identifier: GPL-2.0-or-later
/* irq_rotas.c - el camino de cada IRQ PCI: autoprueba en el arranque, _PRT
 * con uACPI y vuelta al 8259 por línea (D211). Ver irq_rotas.h.
 *
 * Reglas para no perder ningún dispositivo:
 *   - nada de esto corre sin el IOAPIC activo (--ioapic): sin él, el 8259 de
 *     siempre y solo la tabla para infopc;
 *   - una ruta que no entrega la interrupción provocada se enmascara (una
 *     entrada mal configurada podría disparar sin parar) y la línea pasa al
 *     8259; si tampoco llega por el 8259, todo vuelve a como estaba (los
 *     drivers sondean si su IRQ no llega: D089, D095);
 *   - con el _PRT, una línea sin sonda conserva la entrada que tenía y gana
 *     las GSI nuevas: nunca se quita algo que funcionaba sin haberlo probado;
 *   - las pruebas se hacen en la CPU 0 con el cerrojo general tomado (la
 *     línea se lleva a la CPU 0 mientras dura): una IRQ que llega ahí entra
 *     sin esperar el cerrojo, y ninguna otra CPU usa el dispositivo a la vez.
 *     Cada espera dura como mucho 50 ms (por el TSC), dos intentos. */
#include "irq_rotas.h"
#include "ioapic.h"
#include "cpu.h"
#include "cpu_dados.h"
#include "console.h"
#include "texto.h"
#include "relogio.h"
#include "info_hardware.h"
#include "bootinfo.h"
#include "sistema.h"
#include "uacpi_sagui.h"
#include "prt.h"
#include "abi.h"
#include <uacpi/uacpi.h>
#include <uacpi/utilities.h>

#define FUNCOES_MAX 64
#define SONDAS_MAX  8
#define PROVA_MS    50
#define SEM_LINHA   0xFF

typedef struct {
    uint8_t  bus, disp, fun;
    uint8_t  pino;         /* 1-4; 0 = puente sin interrupción */
    uint8_t  linha;        /* la de la BIOS (0x3C); SEM_LINHA si no es una IRQ ISA válida */
    uint8_t  secundario;   /* puente PCI-PCI: su bus secundario */
    uint32_t gsi;          /* la que usa ahora (IOAPIC_SEM_GSI = ninguna) */
    int      rota;         /* índice de la ruta, IOAPIC_ROTA_ISA o IOAPIC_ROTA_ERRO (sin entrada) */
    bool     prt;          /* la GSI salió del _PRT */
    bool     nivel, baixo;
    int      sonda;        /* índice de su sonda o -1 */
} funcao_t;

static funcao_t funcoes[FUNCOES_MAX];
static int n_funcoes;
static irq_sonda_t sondas[SONDAS_MAX];
static uint8_t resultado[SONDAS_MAX];   /* IRQ_PROVA_* */
static int n_sondas;
static volatile bool arranque_feito;

void irq_sonda_registrar(const irq_sonda_t *s)
{
    if (n_sondas >= SONDAS_MAX || !s->provocar || !s->contador || !s->terminar) return;
    sondas[n_sondas] = *s;
    resultado[n_sondas] = IRQ_PROVA_NADA;
    n_sondas++;
}

static uint8_t prova_byte(void) { return cabecalho_boot[CAB_IRQ_PRUEBA]; }

/* Las líneas que también usa algo que no es PCI (el PIT, el teclado, el
 * reloj, el ratón, el coprocesador, la SCI): su entrada ISA nunca se quita. */
static bool isa_exclusiva(uint8_t irq)
{
    return irq == 0 || irq == 1 || irq == 2 || irq == 8 || irq == 12 || irq == 13 || irq == ioapic_sci();
}

static funcao_t *funcao_da_sonda(int k)
{
    for (int i = 0; i < n_funcoes; i++)
        if (funcoes[i].sonda == k) return &funcoes[i];
    return NULL;
}

static const char *nome_funcao(const funcao_t *f)
{
    return f->sonda >= 0 ? sondas[f->sonda].nome : "PCI";
}

/* ------------------------------------------------------------ las funciones PCI */

static void listar_funcoes(void)
{
    static pci_disp_t v[FUNCOES_MAX];
    int n = pci_listar(v, FUNCOES_MAX);
    n_funcoes = 0;
    for (int i = 0; i < n && n_funcoes < FUNCOES_MAX; i++) {
        uint32_t r3c = pci_le32(&v[i], 0x3C);
        uint8_t tipo = (uint8_t)((pci_le32(&v[i], 0x0C) >> 16) & 0x7F);
        uint8_t pino = (uint8_t)((r3c >> 8) & 0xFF), linha = (uint8_t)(r3c & 0xFF);
        uint8_t sec = tipo == 1 ? (uint8_t)((pci_le32(&v[i], 0x18) >> 8) & 0xFF) : 0;
        if (pino > 4) pino = 0;
        if (!pino && !sec) continue;
        funcao_t *f = &funcoes[n_funcoes++];
        f->bus = v[i].barramento; f->disp = v[i].dispositivo; f->fun = v[i].funcao;
        f->pino = pino;
        f->linha = (linha >= 1 && linha < 16 && linha != 2) ? linha : SEM_LINHA;
        f->secundario = sec;
        f->gsi = IOAPIC_SEM_GSI;
        f->rota = IOAPIC_ROTA_ERRO;
        f->prt = f->nivel = f->baixo = false;
        f->sonda = -1;
    }
    for (int k = 0; k < n_sondas; k++)
        for (int i = 0; i < n_funcoes; i++)
            if (funcoes[i].bus == sondas[k].pci.barramento && funcoes[i].disp == sondas[k].pci.dispositivo &&
                funcoes[i].fun == sondas[k].pci.funcao)
                funcoes[i].sonda = k;
}

/* ------------------------------------------------------------ la prueba */

static inline uint64_t tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static bool interrupcoes_ligadas(void)
{
    uint32_t f;
    __asm__ volatile("pushf; pop %0" : "=r"(f));
    return (f & 0x200) != 0;
}

static bool esperar(irq_sonda_t *s, uint32_t antes)
{
    uint32_t khz = hw_tsc_khz();
    if (khz) {
        uint64_t limite = tsc() + (uint64_t)khz * PROVA_MS;
        while (tsc() < limite) {
            if (s->contador(s->ctx) != antes) return true;
            __asm__ volatile("pause");
        }
    } else {
        uint32_t t0 = relogio_tiques();
        while (relogio_tiques() - t0 < PROVA_MS * TIQUES_POR_SEGUNDO / 1000 + 2) {
            if (s->contador(s->ctx) != antes) return true;
            __asm__ volatile("pause");
        }
    }
    return s->contador(s->ctx) != antes;
}

/* Una sonda con la configuración que haya ahora: dos intentos. */
static uint8_t provar(int k)
{
    irq_sonda_t *s = &sondas[k];
    if (!interrupcoes_ligadas() || cpu_atual() != 0) return IRQ_PROVA_OCUPADO;
    uint8_t dest = ioapic_destino_atual(s->irq);
    bool mover = dest != ioapic_apic_cpu0();
    if (mover) ioapic_destino(s->irq, ioapic_apic_cpu0());
    uint8_t r = IRQ_PROVA_FALHA;
    for (int t = 0; t < 2; t++) {
        uint32_t antes = s->contador(s->ctx);
        if (!s->provocar(s->ctx)) { r = IRQ_PROVA_OCUPADO; break; }
        bool ok = esperar(s, antes);
        s->terminar(s->ctx, ok);
        if (ok) { r = IRQ_PROVA_OK; break; }
    }
    if (mover) ioapic_destino(s->irq, dest);
    return r;
}

/* En la tarea acpi-aml el dispositivo puede estar ocupado (una copia del
 * pendrive tiene el bloqueo del EHCI): se duerme un poco y se reintenta,
 * hasta un segundo. En el arranque no se duerme. */
static uint8_t provar_esperando(int k, bool dormir)
{
    uint8_t r = provar(k);
    for (int t = 0; dormir && r == IRQ_PROVA_OCUPADO && t < 20; t++) {
        uacpi_sagui_dormir_ms(50);
        r = provar(k);
    }
    return r;
}

/* ¿La ruta de la función entrega ahora por el IOAPIC? */
static bool rota_ativa(const funcao_t *f)
{
    if (f->rota == IOAPIC_ROTA_ISA) return !ioapic_isa_desviada(f->linha);
    bool ligada = false;
    return f->rota >= 0 && ioapic_rota_info(f->rota, NULL, NULL, &ligada) && ligada;
}

/* Prueba las sondas de la línea con lo que haya ahora y, a la que no le
 * llega, le quita su ruta y le da el 8259. Devuelve cuántas quedan sin
 * interrupción. */
static int provar_linha(uint8_t irq, const char *fase, bool dormir)
{
    for (int k = 0; k < n_sondas; k++)
        if (sondas[k].irq == irq) resultado[k] = provar_esperando(k, dormir);
    int perdidas = 0;
    for (int k = 0; k < n_sondas; k++) {
        if (sondas[k].irq != irq) continue;
        irq_sonda_t *s = &sondas[k];
        funcao_t *f = funcao_da_sonda(k);
        uint32_t gsi = f ? f->gsi : ioapic_gsi_isa(irq);
        if (resultado[k] == IRQ_PROVA_OK) {
            klog("[IOAPIC] autoprueba (%s): %s %02x:%02x.%x GSI %u -> IRQ %u: llega\n", fase, s->nome,
                 s->pci.barramento, s->pci.dispositivo, s->pci.funcao, gsi, irq);
            continue;
        }
        if (resultado[k] == IRQ_PROVA_OCUPADO) {
            klog("[IOAPIC] autoprueba (%s): %s %02x:%02x.%x ocupado, no se prueba\n", fase, s->nome,
                 s->pci.barramento, s->pci.dispositivo, s->pci.funcao);
            continue;
        }
        /* Si otro dispositivo de la misma ruta recibió la suya, la ruta
         * funciona: es cosa de este dispositivo. */
        bool mesma_ok = false;
        for (int j = 0; j < n_sondas; j++) {
            funcao_t *g = funcao_da_sonda(j);
            if (j != k && sondas[j].irq == irq && resultado[j] == IRQ_PROVA_OK && g && f && g->rota == f->rota)
                mesma_ok = true;
        }
        if (mesma_ok) {
            klog("[IOAPIC] autoprueba (%s): %s %02x:%02x.%x no interrumpe, pero la GSI %u si llega: se deja\n",
                 fase, s->nome, s->pci.barramento, s->pci.dispositivo, s->pci.funcao, gsi);
            perdidas++;
            continue;
        }
        ioapic_estado_t antes;
        ioapic_estado_salvar(&antes);
        if (!f || f->rota == IOAPIC_ROTA_ISA) {
            if (!isa_exclusiva(irq)) ioapic_isa_desviar(irq, true);
        } else if (f->rota >= 0) {
            ioapic_rota_ligar(f->rota, false);
        }
        ioapic_via_pic(irq, true);
        uint8_t r2 = provar_esperando(k, dormir);
        if (r2 == IRQ_PROVA_OK) {
            resultado[k] = IRQ_PROVA_PIC;
            klog("[IOAPIC] autoprueba (%s): %s %02x:%02x.%x GSI %u no llega -> PIC (IRQ %u)\n", fase, s->nome,
                 s->pci.barramento, s->pci.dispositivo, s->pci.funcao, gsi, irq);
        } else {
            ioapic_estado_restaurar(&antes);
            perdidas++;
            klog("[IOAPIC] autoprueba (%s): %s %02x:%02x.%x GSI %u no llega, ni por el PIC (IRQ %u): "
                 "se deja como estaba (el driver sondea)\n", fase, s->nome, s->pci.barramento,
                 s->pci.dispositivo, s->pci.funcao, gsi, irq);
        }
    }
    return perdidas;
}

/* La tabla para el registro: dispositivo -> pin -> GSI -> vector -> CPU. */
static void tabela(const char *fase)
{
    klog("[IOAPIC] tabla (%s): funcion PCI, pin, GSI, IRQ/vector, CPU (APIC), autoprueba\n", fase);
    for (int i = 0; i < n_funcoes; i++) {
        funcao_t *f = &funcoes[i];
        if (!f->pino) continue;
        if (f->linha == SEM_LINHA) {
            klog("[IOAPIC]   %02x:%02x.%x %-5s INT%c -> sin linea de la BIOS: sin IRQ\n", f->bus, f->disp, f->fun,
                 nome_funcao(f), 'A' + f->pino - 1);
            continue;
        }
        static const char *const prova[] = { "-", "ok", "8259", "falla", "ocupado" };
        const char *p = f->sonda >= 0 ? prova[resultado[f->sonda] % 5] : "-";
        bool pic = (ioapic_linhas_pic() >> f->linha) & 1;
        if (rota_ativa(f))
            klog("[IOAPIC]   %02x:%02x.%x %-5s INT%c -> GSI %u (%s, %s, activa %s) -> IRQ %u vector %x -> APIC %u%s; "
                 "autoprueba %s\n", f->bus, f->disp, f->fun, nome_funcao(f), 'A' + f->pino - 1, f->gsi,
                 f->prt ? "_PRT" : "linea BIOS", f->nivel ? "nivel" : "flanco", f->baixo ? "baja" : "alta", f->linha,
                 VETOR_IRQ0 + f->linha, ioapic_destino_atual(f->linha), pic ? " y 8259" : "", p);
        else
            klog("[IOAPIC]   %02x:%02x.%x %-5s INT%c -> %s IRQ %u vector %x -> CPU 0; autoprueba %s\n", f->bus,
                 f->disp, f->fun, nome_funcao(f), 'A' + f->pino - 1, pic ? "8259" : "sin entrada", f->linha,
                 VETOR_IRQ0 + f->linha, p);
    }
}

/* ------------------------------------------------------------ arranque */

void irq_rotas_arranque(void)
{
    listar_funcoes();
    if (!ioapic_ativo()) {
        arranque_feito = true;
        return;
    }
    /* La adivinanza de la D166: la entrada ISA de la línea de la BIOS, con la
     * forma de la override o del ELCR. */
    for (int i = 0; i < n_funcoes; i++) {
        funcao_t *f = &funcoes[i];
        if (!f->pino || f->linha == SEM_LINHA) continue;
        f->gsi = ioapic_gsi_isa(f->linha);
        f->rota = f->gsi == IOAPIC_SEM_GSI ? IOAPIC_ROTA_ERRO : IOAPIC_ROTA_ISA;
        f->nivel = true;
        f->baixo = false;
    }
    if (prova_byte() & IRQ_PRUEBA_ISA_ERRADA)
        for (int k = 0; k < n_sondas; k++)
            if (!isa_exclusiva(sondas[k].irq) && !ioapic_isa_desviada(sondas[k].irq)) {
                ioapic_isa_desviar(sondas[k].irq, true);
                klog("[IOAPIC] prueba: la entrada ISA de la IRQ %u no entrega (simulado, --irq-prueba isa-errada)\n",
                     sondas[k].irq);
            }
    uint16_t provadas = 0;
    uint64_t t0 = tsc();
    for (int k = 0; k < n_sondas; k++) {
        uint8_t irq = sondas[k].irq;
        if (irq >= 16 || (provadas >> irq) & 1) continue;
        provadas |= (uint16_t)(1u << irq);
        provar_linha(irq, "arranque", false);
    }
    uint32_t khz = hw_tsc_khz();
    klog("[IOAPIC] autoprueba (arranque): %d sondas en %u ms\n", n_sondas,
         khz ? (uint32_t)((tsc() - t0) / khz) : 0);
    tabela("arranque");
    arranque_feito = true;
}

/* ------------------------------------------------------------ _PRT */

void irq_rotas_prt(void)
{
    while (!arranque_feito) uacpi_sagui_dormir_ms(100);
    if (!ioapic_ativo()) {
        klog("[PRT] sin IOAPIC: no se lee el _PRT (el 8259 usa las lineas de la BIOS)\n");
        return;
    }
    uacpi_status st = uacpi_set_interrupt_model(UACPI_INTERRUPT_MODEL_IOAPIC);
    klog("[PRT] \\_PIC(1) (modo APIC): %s\n", uacpi_status_to_string(st));
    if (st != UACPI_STATUS_OK) return;

    static prt_funcao_t pf[FUNCOES_MAX];
    memset(pf, 0, sizeof pf);
    for (int i = 0; i < n_funcoes; i++) {
        pf[i].bus = funcoes[i].bus; pf[i].disp = funcoes[i].disp; pf[i].fun = funcoes[i].fun;
        pf[i].pino = funcoes[i].pino;
        pf[i].linha = funcoes[i].linha == SEM_LINHA ? 0 : funcoes[i].linha;
        pf[i].secundario = funcoes[i].secundario;
    }
    int achados = prt_resolver(pf, n_funcoes, true);
    if (achados < 0) {
        klog("[PRT] no hay puente raiz PCI (PNP0A03/PNP0A08) en el espacio de nombres: sigue lo de antes\n");
        return;
    }
    klog("[PRT] %d funciones PCI con GSI (%d _PRT leidos)\n", achados, prt_tabelas_lidas());
    for (int i = 0; i < n_funcoes; i++) {
        prt_funcao_t *p = &pf[i];
        if (!p->pino) continue;
        if (p->achado)
            klog("[PRT] %02x:%02x.%x INT%c -> %s%s%s -> GSI %u %s activa %s%s (la BIOS dejo la IRQ %u)\n", p->bus,
                 p->disp, p->fun, 'A' + p->pino - 1, p->tabela, p->ligacao[0] ? " " : " (directo)", p->ligacao,
                 p->gsi, p->nivel ? "nivel" : "flanco", p->baixo ? "baja" : "alta", p->srs ? ", enlace encendido" : "",
                 p->linha);
        else
            klog("[PRT] %02x:%02x.%x INT%c -> sin GSI: %s\n", p->bus, p->disp, p->fun, 'A' + p->pino - 1, p->erro);
    }
    if (prova_byte() & IRQ_PRUEBA_PRT_ERRADA) {
        /* Las GSI de verdad del _PRT no se pueden usar como "equivocadas". */
        static uint32_t reais[FUNCOES_MAX];
        int nr = 0;
        for (int i = 0; i < n_funcoes; i++) if (pf[i].achado) reais[nr++] = pf[i].gsi;
        for (int i = 0; i < n_funcoes; i++) {
            uint32_t g;
            if (funcoes[i].sonda < 0 || !pf[i].achado || !ioapic_gsi_livre(&g, reais, nr)) continue;
            klog("[PRT] prueba: %s %02x:%02x.%x recibe la GSI equivocada %u en vez de %u (--irq-prueba prt-errada)\n",
                 nome_funcao(&funcoes[i]), pf[i].bus, pf[i].disp, pf[i].fun, g, pf[i].gsi);
            pf[i].gsi = g;
            /* Se reserva ya: la siguiente función recibe otra. */
            ioapic_rota_criar(funcoes[i].linha, g, true, true);
        }
    }

    static funcao_t copia[FUNCOES_MAX];
    for (uint8_t irq = 1; irq < 16; irq++) {
        bool tem = false;
        for (int i = 0; i < n_funcoes; i++)
            if (funcoes[i].linha == irq && pf[i].achado) tem = true;
        if (!tem) continue;
        ioapic_estado_t antes;
        ioapic_estado_salvar(&antes);
        memcpy(copia, funcoes, sizeof copia);
        uint8_t res_antes[SONDAS_MAX];
        memcpy(res_antes, resultado, sizeof res_antes);

        bool todas_fora = true, mudou = false;
        for (int i = 0; i < n_funcoes; i++) {
            funcao_t *f = &funcoes[i];
            if (f->linha != irq || !f->pino) continue;
            if (!pf[i].achado) { todas_fora = false; continue; }
            int r = ioapic_rota_criar(irq, pf[i].gsi, pf[i].nivel, pf[i].baixo);
            if (r == IOAPIC_ROTA_ERRO) {
                klog("[PRT] %02x:%02x.%x: la GSI %u no sirve para la IRQ %u (fuera de los IOAPIC o de otra linea): "
                     "sigue como estaba\n", f->bus, f->disp, f->fun, pf[i].gsi, irq);
                todas_fora = false;
                continue;
            }
            if (r == IOAPIC_ROTA_ISA) todas_fora = false;
            if (f->rota != r) mudou = true;
            f->rota = r;
            f->gsi = r == IOAPIC_ROTA_ISA ? ioapic_gsi_isa(irq) : pf[i].gsi;
            f->prt = true;
            f->nivel = pf[i].nivel;
            f->baixo = pf[i].baixo;
            if (r >= 0) ioapic_rota_ligar(r, true);
        }
        bool tem_sonda = false;
        for (int k = 0; k < n_sondas; k++) if (sondas[k].irq == irq) tem_sonda = true;
        if (!tem_sonda) {
            if (mudou)
                klog("[PRT] IRQ %u: sin autoprueba; se anaden las GSI del _PRT y se conserva la entrada de antes\n", irq);
            continue;
        }
        /* Con sonda: lo que dice el firmware, solo. Fuera el 8259 y, si
         * todas las funciones de la línea tienen su GSI, la entrada ISA. */
        ioapic_via_pic(irq, false);
        if (todas_fora && !isa_exclusiva(irq)) ioapic_isa_desviar(irq, true);
        else ioapic_isa_desviar(irq, (antes.isa_desviada >> irq) & 1);
        provar_linha(irq, "_PRT", true);
        bool alguma = false, ocupado = false;
        for (int k = 0; k < n_sondas; k++) {
            if (sondas[k].irq != irq) continue;
            if (resultado[k] == IRQ_PROVA_OK || resultado[k] == IRQ_PROVA_PIC) alguma = true;
            if (resultado[k] == IRQ_PROVA_OCUPADO) ocupado = true;
        }
        if (!alguma) {
            ioapic_estado_restaurar(&antes);
            memcpy(funcoes, copia, sizeof copia);
            memcpy(resultado, res_antes, sizeof res_antes);
            klog("[PRT] IRQ %u: %s con el _PRT: vuelve lo de antes\n", irq,
                 ocupado ? "no se pudo probar (dispositivo ocupado)" : "ninguna interrupcion llega");
        }
    }
    tabela("_PRT");
}

/* ------------------------------------------------------------ INFO_IRQ_VIAS */

int32_t irq_rotas_info(uint32_t ptr, uint32_t n)
{
    info_irq_via_t *v = (info_irq_via_t *)ptr;
    if (!n || n > 256 || !usuario_ok(v, n * sizeof *v, true)) return ERRO_ENDERECO;
    uint32_t k = 0;
    uint16_t com_pci = 0;
    bool io = ioapic_ativo();
    for (int i = 0; i < n_funcoes && k < n; i++) {
        funcao_t *f = &funcoes[i];
        if (!f->pino) continue;
        info_irq_via_t *e = &v[k++];
        memset(e, 0, sizeof *e);
        e->bus = f->bus; e->disp = f->disp; e->fun = f->fun; e->pino = f->pino;
        e->irq = f->linha;
        e->gsi = IOAPIC_SEM_GSI;
        if (f->sonda >= 0) {
            txt_copia_n(e->nome, sondas[f->sonda].nome, sizeof e->nome);
            e->prova = resultado[f->sonda];
        }
        if (f->linha == SEM_LINHA) continue;
        com_pci |= (uint16_t)(1u << f->linha);
        e->atendidas = irq_contagem(f->linha);
        e->por_segundo = irq_por_segundo(f->linha); e->tormentas = irq_tormentas(f->linha);
        e->tormenta = irq_mascarada(f->linha);
        e->prt = f->prt;
        e->nivel = f->nivel;
        e->baixo = f->baixo;
        if (!io) { e->via = IRQ_VIA_PIC; continue; }
        e->apic = ioapic_destino_atual(f->linha);
        if (rota_ativa(f)) { e->via = IRQ_VIA_IOAPIC; e->gsi = f->gsi; }
        else if ((ioapic_linhas_pic() >> f->linha) & 1) { e->via = IRQ_VIA_MISTA; e->apic = ioapic_apic_cpu0(); }
        else e->via = IRQ_VIA_IOAPIC;
    }
    /* Las líneas ISA en uso sin funciones PCI (PIT, teclado, ratón, SCI...). */
    uint16_t uso = irq_linhas_em_uso();
    for (uint8_t irq = 0; irq < 16 && k < n; irq++) {
        if (!((uso >> irq) & 1) || ((com_pci >> irq) & 1)) continue;
        info_irq_via_t *e = &v[k++];
        memset(e, 0, sizeof *e);
        e->irq = irq;
        e->bus = 0xFF;
        e->atendidas = irq_contagem(irq);
        e->por_segundo = irq_por_segundo(irq); e->tormentas = irq_tormentas(irq);
        e->tormenta = irq_mascarada(irq);
        e->gsi = IOAPIC_SEM_GSI;
        if (!io) { e->via = IRQ_VIA_PIC; continue; }
        e->via = IRQ_VIA_IOAPIC;
        e->apic = ioapic_destino_atual(irq);
        e->gsi = ioapic_gsi_isa(irq);
    }
    return (int32_t)k;
}
