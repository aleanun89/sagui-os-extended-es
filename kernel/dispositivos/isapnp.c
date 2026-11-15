// SPDX-License-Identifier: GPL-2.0-or-later
/* isapnp.c - ISA Plug and Play (D302): encontrar las tarjetas ISA PnP y configurar una Sound
 * Blaster de Creative (Vibra16, SB16 PnP, AWE32/64 PnP) que la BIOS dejó sin activar.
 *
 * El protocolo (especificación "Plug and Play ISA" 1.0a de Intel y Microsoft):
 *   - tres puertos: ADDRESS (279h) y WRITE_DATA (A79h), fijos, y READ_DATA, que se elige
 *     entre 203h y 3FFh con los dos bits bajos a 1 (aquí 20Bh; si no aparece nada, 273h);
 *   - las tarjetas duermen ("Wait for Key") hasta oír la clave de iniciación: 32 bytes de un LFSR
 *     escritos en ADDRESS;
 *   - aislamiento: con WAKE[0] todas las tarjetas sin número (CSN 0) compiten. Cada una tiene
 *     un identificador de 72 bits (vendedor, serie y suma); se leen 72 pares del registro de
 *     aislamiento y en cada bit las que tienen un 1 contestan 55h y AAh. La que gana (la suma
 *     cuadra) recibe un CSN; se repite hasta que no contesta nadie;
 *   - configuración: WAKE[csn], se elige el dispositivo lógico (07h) y se escriben el puerto
 *     (60h/61h), la IRQ (70h/71h) y los DMA (74h/75h); 30h = 1 lo activa;
 *   - al acabar, todas vuelven a "Wait for Key" (02h = 2).
 *
 * Solo se llama desde la búsqueda de la SB16 y con un puente ISA en el PCI: para configurar, si no
 * contestó ninguna en los puertos fijos; para solo listar (y saber el identificador PnP de la que ya
 * configuró la BIOS), si contestó. No reinicia la configuración de las tarjetas que la BIOS ya activó
 * (solo sus CSN). Las tarjetas que no son de Creative se anotan en el registro y en la lista de
 * dispositivos ISA, sin configurar. */
#include "sb16.h"
#include "console.h"
#include "cpu.h"
#include "isa.h"
#include "pci.h"
#include "portas.h"
#include "texto.h"

#define PNP_ENDERECO 0x279
#define PNP_ESCRITA  0xA79
#define R_PORTA_LER  0x00
#define R_ISOLAR     0x01
#define R_CONTROLE   0x02     /* bit 1: volver a "Wait for Key"; bit 2: CSN a 0 */
#define R_ACORDAR    0x03
#define R_CSN        0x06
#define R_LOGICO     0x07
#define R_ATIVAR     0x30
#define R_FAIXA      0x31
#define R_ES0        0x60
#define R_IRQ0       0x70
#define R_DMA0       0x74
#define MAX_CARTOES  8

static uint16_t porta_ler;

static void esperar_us(uint32_t us) { while (us--) porta_pausa(); }

static void escrever(uint8_t r, uint8_t v)
{
    porta_escreve8(PNP_ENDERECO, r);
    porta_escreve8(PNP_ESCRITA, v);
}

static void chave(void)
{
    porta_escreve8(PNP_ENDERECO, 0);
    porta_escreve8(PNP_ENDERECO, 0);
    uint8_t c = 0x6A;
    for (int i = 0; i < 32; i++) {
        porta_escreve8(PNP_ENDERECO, c);
        c = (uint8_t)((c >> 1) | (((c ^ (c >> 1)) & 1) << 7));
    }
}

/* Lee un identificador de 72 bits. -> true si la suma cuadra (una tarjeta ganó). */
static bool isolar(uint8_t id[9])
{
    memset(id, 0, 9);
    porta_escreve8(PNP_ENDERECO, R_ISOLAR);
    esperar_us(1000);
    uint8_t soma = 0x6A, alguem = 0;
    for (int b = 0; b < 72; b++) {
        uint8_t x = porta_le8(porta_ler);
        esperar_us(250);
        uint8_t y = porta_le8(porta_ler);
        esperar_us(250);
        uint8_t bit = x == 0x55 && y == 0xAA;
        alguem |= bit;
        if (b < 64) soma = (uint8_t)((((soma ^ (soma >> 1)) & 1) ^ bit) << 7 | (soma >> 1));
        if (bit) id[b / 8] |= (uint8_t)(1u << (b % 8));
        /* el primer byte lleva la primera letra del vendedor (nunca 0): si nadie contestó en él,
         * no hay ninguna tarjeta compitiendo y no hace falta leer los 72 bits (36 ms) */
        if (b == 7 && !alguem) return false;
    }
    return alguem && id[8] == soma;
}

/* "CTL0070": el vendedor va en EISA comprimido (3 letras de 5 bits) y el producto en hex. */
static void nome_id(const uint8_t id[9], char *s)
{
    s[0] = (char)('@' + ((id[0] >> 2) & 0x1F));
    s[1] = (char)('@' + (((id[0] & 3) << 3) | (id[1] >> 5)));
    s[2] = (char)('@' + (id[1] & 0x1F));
    snformatar(s + 3, 8, "%02X%02X", id[2], id[3]);
}

/* Una IRQ que nadie usa: ni un driver, ni un dispositivo PCI, ni marcada por nivel en el ELCR. */
static uint8_t irq_livre(void)
{
    static const uint8_t candidatas[] = { 5, 7, 10, 9 };
    uint16_t usadas = (uint16_t)(porta_le8(0x4D0) | porta_le8(0x4D1) << 8);
    if (usadas & ((1u << 0) | (1u << 1) | (1u << 2) | (1u << 8) | (1u << 13))) usadas = 0;   /* sin ELCR */
    pci_disp_t v[48];
    int n = pci_listar(v, 48);
    for (int i = 0; i < n; i++) {
        uint8_t l = (uint8_t)(pci_le32(&v[i], 0x3C) & 0xFF);
        if (((pci_le32(&v[i], 0x3C) >> 8) & 0xFF) && l && l < 16) usadas |= (uint16_t)(1u << l);
    }
    for (unsigned i = 0; i < sizeof candidatas; i++) {
        uint8_t q = candidatas[i];
        if (!(usadas & (1u << q)) && !irq_ocupada(q) && !(irq_linhas_em_uso() & (1u << q))) return q;
    }
    return 0;
}

static bool porta_livre(uint16_t b)
{
    for (int i = 0; i < 16; i++) if (porta_le8((uint16_t)(b + i)) != 0xFF) return false;
    return true;
}

static int procurar_com(uint16_t rd, uint8_t ids[][9])
{
    porta_ler = rd;
    escrever(R_CONTROLE, 0x02);
    chave();
    escrever(R_CONTROLE, 0x04);      /* todos los CSN a 0 (la configuración activa no se toca) */
    esperar_us(2000);
    escrever(R_CONTROLE, 0x02);
    chave();
    escrever(R_ACORDAR, 0);
    escrever(R_PORTA_LER, (uint8_t)(rd >> 2));
    esperar_us(1000);
    int n = 0;
    while (n < MAX_CARTOES && isolar(ids[n])) {
        escrever(R_CSN, (uint8_t)(n + 1));
        n++;
        escrever(R_ACORDAR, 0);       /* las que quedan sin CSN vuelven a competir */
        escrever(R_PORTA_LER, (uint8_t)(rd >> 2));
    }
    return n;
}

bool isapnp_configurar_sb(isapnp_sb_t *r, bool configurar)
{
    pci_disp_t ponte;
    if (pci_procurar_classe(0x06, 0x01, &ponte, 1) < 1) return false;   /* sin bus ISA ni LPC: nada que buscar */
    static const uint16_t rds[] = { 0x20B, 0x273 };   /* los dos bits bajos siempre a 1 (el registro 00h guarda los 8 de arriba) */
    uint8_t ids[MAX_CARTOES][9];
    int n = 0;
    for (unsigned k = 0; k < sizeof rds / sizeof rds[0] && !n; k++) n = procurar_com(rds[k], ids);
    if (!n) {
        escrever(R_CONTROLE, 0x02);
        klog("[ISAPNP] ninguna tarjeta ISA PnP\n");
        return false;
    }
    bool feito = false;
    for (int i = 0; i < n; i++) {
        char nome[12];
        nome_id(ids[i], nome);
        uint32_t serie = (uint32_t)ids[i][4] | (uint32_t)ids[i][5] << 8 | (uint32_t)ids[i][6] << 16 | (uint32_t)ids[i][7] << 24;
        klog("[ISAPNP] CSN %d: %s, serie %08x\n", i + 1, nome, serie);
        bool creative = nome[0] == 'C' && nome[1] == 'T' && nome[2] == 'L';
        if (creative && !configurar) {          /* ya activa (la BIOS): solo el nombre */
            if (!r->id[0]) txt_copia_n(r->id, nome, sizeof r->id);
            continue;
        }
        if (!creative || feito) {
            isa_registrar(nome, 0, 0xFF, 0xFF, 0xFF, "ISA PnP, sin driver");
            continue;
        }
        static const uint16_t portas[] = { 0x220, 0x240, 0x260, 0x280 };
        uint16_t pb = 0;
        for (unsigned k = 0; k < 4 && !pb; k++) if (porta_livre(portas[k])) pb = portas[k];
        uint8_t q = irq_livre();
        if (!pb || !q) {
            klog("[ISAPNP] %s: sin %s libre; no se configura\n", nome, pb ? "IRQ" : "puerto");
            isa_registrar(nome, 0, 0xFF, 0xFF, 0xFF, "ISA PnP, sin recursos libres");
            continue;
        }
        escrever(R_ACORDAR, (uint8_t)(i + 1));   /* esta pasa a configuración */
        escrever(R_LOGICO, 0);                    /* el dispositivo lógico 0 es el audio */
        escrever(R_ATIVAR, 0);
        escrever(R_FAIXA, 0);
        escrever(R_ES0, (uint8_t)(pb >> 8));
        escrever(R_ES0 + 1, (uint8_t)pb);
        escrever(R_ES0 + 2, 0x03);                /* MPU-401 en 330h */
        escrever(R_ES0 + 3, 0x30);
        escrever(R_ES0 + 4, 0x03);                /* OPL en 388h */
        escrever(R_ES0 + 5, 0x88);
        escrever(R_IRQ0, q);
        escrever(R_IRQ0 + 1, 0x02);               /* flanco, activa alta (ISA) */
        escrever(R_DMA0, 1);
        escrever(R_DMA0 + 1, 5);
        escrever(R_ATIVAR, 1);
        r->porta = pb;
        r->irq = q;
        r->dma8 = 1;
        r->dma16 = 5;
        txt_copia_n(r->id, nome, sizeof r->id);
        klog("[ISAPNP] %s configurada: puerto %xh, IRQ %u, DMA 1 y 5\n", nome, pb, q);
        feito = true;
    }
    escrever(R_CONTROLE, 0x02);                   /* todas a "Wait for Key" */
    return feito;
}
