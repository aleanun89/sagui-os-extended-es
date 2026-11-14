// SPDX-License-Identifier: GPL-2.0-or-later
/* prt.c - el enrutado de las interrupciones PCI del firmware (_PRT) con
 * uACPI (D211). Ver prt.h.
 *
 * Pasos:
 *   1. los puentes raíz (PNP0A03/PNP0A08) del espacio de nombres, con su
 *      número de bus (_BBN, 0 si no lo tienen);
 *   2. el nodo ACPI de cada puente PCI-PCI: el hijo del nodo de su bus cuyo
 *      _ADR es (ranura << 16 | función). Se repite hasta que no aparece
 *      ninguno nuevo (un puente detrás de otro necesita antes el de arriba);
 *   3. cada función con pin: la fila (ranura, pin) del _PRT de su bus; si su
 *      bus no tiene _PRT o no tiene la fila, el pin girado en el bus de
 *      arriba, como hace el hardware de un puente PCI-PCI;
 *   4. la fila da la GSI directamente o por un Link Device (su _CRS).
 *
 * No toca el hardware salvo el _SRS de un enlace apagado (si se permite). Se
 * compila igual en el kernel y en el PC (tools/uacpi_volcado.c). */
#include "prt.h"
#include <uacpi/uacpi.h>
#include <uacpi/namespace.h>
#include <uacpi/utilities.h>
#include <uacpi/resources.h>

#define BUSES 256

static uacpi_namespace_node *no_bus[BUSES];          /* el nodo ACPI de cada bus */
static uacpi_pci_routing_table *tabela[BUSES];        /* su _PRT, si lo tiene */
static bool tabela_tentada[BUSES];
static int ponte_de[BUSES];                           /* índice de la función puente de cada bus (-1 = raíz o nada) */
static int tabelas_lidas;

int prt_tabelas_lidas(void) { return tabelas_lidas; }

static void copiar(char *d, const char *s, int max)
{
    int i = 0;
    if (s) for (; s[i] && i < max - 1; i++) d[i] = s[i];
    d[i] = 0;
}

static void nome_no(char *d, int max, uacpi_namespace_node *no)
{
    const uacpi_char *c = uacpi_namespace_node_generate_absolute_path(no);
    copiar(d, c ? c : "?", max);
    if (c) uacpi_free_absolute_path(c);
}

/* ------------------------------------------------------------ puentes raíz */

static uacpi_iteration_decision raiz_achada(void *u, uacpi_namespace_node *no, uacpi_u32 prof)
{
    (void)prof;
    int *n = u;
    uacpi_u64 bbn = 0;
    if (uacpi_eval_simple_integer(no, "_BBN", &bbn) != UACPI_STATUS_OK) bbn = 0;
    if (bbn < BUSES && !no_bus[bbn]) {
        no_bus[bbn] = no;
        (*n)++;
    }
    /* Los hijos de un puente raíz no son otros puentes raíz. */
    return UACPI_ITERATION_DECISION_NEXT_PEER;
}

static int achar_raizes(void)
{
    static const uacpi_char *const ids[] = { "PNP0A08", "PNP0A03", UACPI_NULL };
    int n = 0;
    uacpi_find_devices_at(uacpi_namespace_root(), ids, raiz_achada, &n);
    if (!n) {
        /* uacpi_find_devices_at mira el _STA: si el del puente dice "no
         * está" (un _STA que lee algo que aún no existe), el nombre de
         * siempre. */
        uacpi_namespace_node *no = UACPI_NULL;
        if (uacpi_namespace_node_find(UACPI_NULL, "\\_SB.PCI0", &no) == UACPI_STATUS_OK && no) {
            no_bus[0] = no;
            n = 1;
        }
    }
    return n;
}

/* El hijo de "pai" que es el dispositivo (ranura, función). */
static uacpi_namespace_node *filho_com_adr(uacpi_namespace_node *pai, uint32_t adr)
{
    uacpi_namespace_node *it = UACPI_NULL;
    while (uacpi_namespace_node_next_typed(pai, &it, UACPI_OBJECT_DEVICE_BIT) == UACPI_STATUS_OK) {
        uacpi_u64 v;
        if (uacpi_eval_simple_integer(it, "_ADR", &v) == UACPI_STATUS_OK && (uint32_t)v == adr)
            return it;
    }
    return UACPI_NULL;
}

static uacpi_pci_routing_table *prt_do_bus(uint32_t bus)
{
    if (bus >= BUSES || !no_bus[bus]) return UACPI_NULL;
    if (!tabela_tentada[bus]) {
        tabela_tentada[bus] = true;
        if (uacpi_get_pci_routing_table(no_bus[bus], &tabela[bus]) == UACPI_STATUS_OK) tabelas_lidas++;
        else tabela[bus] = UACPI_NULL;
    }
    return tabela[bus];
}

/* ------------------------------------------------------------ enlaces */

typedef struct {
    bool achou;
    uint32_t irq;
    bool nivel, baixo;
    uacpi_resource *recurso;   /* para el _SRS: el recurso de _PRS a reescribir */
} irq_lida_t;

static uacpi_iteration_decision ver_recurso(void *u, uacpi_resource *r)
{
    irq_lida_t *l = u;
    if (r->type == UACPI_RESOURCE_TYPE_IRQ && r->irq.num_irqs) {
        l->irq = r->irq.irqs[0];
        l->nivel = r->irq.triggering == UACPI_TRIGGERING_LEVEL;
        l->baixo = r->irq.polarity == UACPI_POLARITY_ACTIVE_LOW;
    } else if (r->type == UACPI_RESOURCE_TYPE_EXTENDED_IRQ && r->extended_irq.num_irqs) {
        l->irq = r->extended_irq.irqs[0];
        l->nivel = r->extended_irq.triggering == UACPI_TRIGGERING_LEVEL;
        l->baixo = r->extended_irq.polarity == UACPI_POLARITY_ACTIVE_LOW;
    } else {
        return UACPI_ITERATION_DECISION_CONTINUE;
    }
    l->achou = true;
    l->recurso = r;
    return UACPI_ITERATION_DECISION_BREAK;
}

/* Un enlace apagado: se elige una IRQ de su _PRS (la que la BIOS dejó en la
 * función si está en la lista; si no, la primera) y se enciende con _SRS. */
static bool ligar_enlace(uacpi_namespace_node *no, uint8_t preferida)
{
    uacpi_resources *rs = UACPI_NULL;
    if (uacpi_get_possible_resources(no, &rs) != UACPI_STATUS_OK || !rs) return false;
    irq_lida_t l = { 0 };
    uacpi_for_each_resource(rs, ver_recurso, &l);
    bool ok = false;
    if (l.achou) {
        uacpi_resource *r = l.recurso;
        uint32_t escolha = 0;
        if (r->type == UACPI_RESOURCE_TYPE_IRQ) {
            for (uint32_t i = 0; i < r->irq.num_irqs; i++)
                if (r->irq.irqs[i] == preferida) escolha = preferida;
            if (!escolha) escolha = r->irq.irqs[0];
            r->irq.irqs[0] = (uacpi_u8)escolha;
            r->irq.num_irqs = 1;
        } else {
            for (uint32_t i = 0; i < r->extended_irq.num_irqs; i++)
                if (r->extended_irq.irqs[i] == preferida) escolha = preferida;
            if (!escolha) escolha = r->extended_irq.irqs[0];
            r->extended_irq.irqs[0] = escolha;
            r->extended_irq.num_irqs = 1;
        }
        ok = escolha != 0 && uacpi_set_resources(no, rs) == UACPI_STATUS_OK;
    }
    uacpi_free_resources(rs);
    return ok;
}

static bool irq_do_enlace(uacpi_namespace_node *no, uint8_t preferida, bool permitir_srs, prt_funcao_t *f)
{
    irq_lida_t l = { 0 };
    uacpi_resources *rs = UACPI_NULL;
    if (uacpi_get_current_resources(no, &rs) == UACPI_STATUS_OK && rs) {
        uacpi_for_each_resource(rs, ver_recurso, &l);
        uacpi_free_resources(rs);
    }
    if ((!l.achou || l.irq == 0) && permitir_srs && ligar_enlace(no, preferida)) {
        f->srs = true;
        l.achou = false;
        rs = UACPI_NULL;
        if (uacpi_get_current_resources(no, &rs) == UACPI_STATUS_OK && rs) {
            uacpi_for_each_resource(rs, ver_recurso, &l);
            uacpi_free_resources(rs);
        }
    }
    if (!l.achou || l.irq == 0) {
        f->erro = "enlace apagado (_CRS sin IRQ)";
        return false;
    }
    f->gsi = l.irq;
    f->nivel = l.nivel;
    f->baixo = l.baixo;
    return true;
}

/* ------------------------------------------------------------ resolver */

static bool resolver_uma(prt_funcao_t *v, int idx, bool permitir_srs)
{
    prt_funcao_t *f = &v[idx];
    uint32_t bus = f->bus, disp = f->disp, pino = f->pino;   /* pino 1-4 */
    for (int salto = 0; salto < 16; salto++) {
        uacpi_pci_routing_table *t = prt_do_bus(bus);
        if (t) {
            for (uacpi_size i = 0; i < t->num_entries; i++) {
                uacpi_pci_routing_table_entry *e = &t->entries[i];
                if ((e->address >> 16) != disp || e->pin != pino - 1) continue;
                f->pino_tabela = (uint8_t)pino;
                f->disp_tabela = (uint8_t)disp;
                nome_no(f->tabela, sizeof f->tabela, no_bus[bus]);
                if (!e->source) {
                    /* Directo: la GSI, por nivel y activa baja (PCI). */
                    f->gsi = e->index;
                    f->nivel = true;
                    f->baixo = true;
                    f->ligacao[0] = 0;
                    return true;
                }
                nome_no(f->ligacao, sizeof f->ligacao, e->source);
                return irq_do_enlace(e->source, f->linha, permitir_srs, f);
            }
        }
        /* Sin tabla o sin fila: el pin girado en el bus de arriba. */
        int p = bus < BUSES ? ponte_de[bus] : -1;
        if (p < 0) {
            f->erro = t ? "el _PRT no tiene la fila" : "el bus no tiene _PRT";
            return false;
        }
        pino = ((pino - 1 + disp) % 4) + 1;
        bus = v[p].bus;
        disp = v[p].disp;
    }
    f->erro = "demasiados puentes";
    return false;
}

int prt_resolver(prt_funcao_t *v, int n, bool permitir_srs)
{
    for (int b = 0; b < BUSES; b++) {
        if (tabela[b]) uacpi_free_pci_routing_table(tabela[b]);
        tabela[b] = UACPI_NULL;
        tabela_tentada[b] = false;
        no_bus[b] = UACPI_NULL;
        ponte_de[b] = -1;
    }
    tabelas_lidas = 0;
    for (int i = 0; i < n; i++) {
        v[i].achado = v[i].nivel = v[i].baixo = v[i].srs = false;
        v[i].gsi = 0;
        v[i].tabela[0] = v[i].ligacao[0] = 0;
        v[i].pino_tabela = v[i].disp_tabela = 0;
        v[i].erro = "";
    }
    if (!achar_raizes()) return -1;

    /* Los puentes PCI-PCI: su bus secundario cuelga del nodo con su _ADR. */
    for (int i = 0; i < n; i++)
        if (v[i].secundario && ponte_de[v[i].secundario] < 0)
            ponte_de[v[i].secundario] = i;
    for (int volta = 0; volta < 8; volta++) {
        bool novo = false;
        for (int i = 0; i < n; i++) {
            uint32_t s = v[i].secundario;
            if (!s || no_bus[s] || !no_bus[v[i].bus]) continue;
            uacpi_namespace_node *no = filho_com_adr(no_bus[v[i].bus], (uint32_t)v[i].disp << 16 | v[i].fun);
            if (no) { no_bus[s] = no; novo = true; }
        }
        if (!novo) break;
    }

    int achados = 0;
    for (int i = 0; i < n; i++) {
        if (v[i].pino < 1 || v[i].pino > 4) { v[i].erro = "sin pin de interrupcion"; continue; }
        if (resolver_uma(v, i, permitir_srs)) {
            v[i].achado = true;
            achados++;
        }
    }
    for (int b = 0; b < BUSES; b++) {
        if (tabela[b]) uacpi_free_pci_routing_table(tabela[b]);
        tabela[b] = UACPI_NULL;
    }
    return achados;
}
