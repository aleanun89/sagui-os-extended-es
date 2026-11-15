// SPDX-License-Identifier: GPL-2.0-or-later
/* isa.c - el bus ISA (D302): el controlador de DMA 8237, la memoria baja para él y la lista
 * de dispositivos ISA detectados (para infopc y lspci).
 *
 * El PC tiene dos 8237 en cascada. El primero lleva los canales 0-3, de 8 bits; el segundo, los
 * 4-7, de 16 bits (el 4 une los dos y no se usa). Cada canal tiene una dirección de 16 bits, un
 * contador de 16 bits y un registro de página aparte con los bits 16-23: por eso el búfer tiene que
 * estar por debajo de 16 MB y no puede cruzar un límite de 64 KB (en los de 16 bits la dirección y
 * el contador van en palabras: 128 KB). Para no depender de que quede memoria baja libre cuando
 * un driver la pida, isa_dma_reservar aparta 128 KB alineados en el arranque, antes que nadie
 * (entre 1 y 16 MB si queda, si no en la memoria convencional, por debajo de 640 KB).
 *
 * Los registros se escriben de 8 en 8 bits con un biestable ("flip-flop") que alterna el byte bajo
 * y el alto: antes de cada par se pone a cero.
 *
 * Referencia: la hoja de datos del Intel 8237A y la del 82C206/PIIX4 (el mapa de puertos del PC). */
#include "isa.h"
#include "abi.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "paginacao.h"
#include "portas.h"
#include "sistema.h"
#include "texto.h"

/* Puertos de cada canal: dirección, contador y página. */
static const uint8_t porta_end[8] = { 0x00, 0x02, 0x04, 0x06, 0xC0, 0xC4, 0xC8, 0xCC };
static const uint8_t porta_cont[8] = { 0x01, 0x03, 0x05, 0x07, 0xC2, 0xC6, 0xCA, 0xCE };
static const uint8_t porta_pag[8] = { 0x87, 0x83, 0x81, 0x82, 0x8F, 0x8B, 0x89, 0x8A };
/* Del controlador: estado, máscara de un canal, modo y el biestable. */
#define C8_ESTADO  0x08
#define C8_MASCARA 0x0A
#define C8_MODO    0x0B
#define C8_FF      0x0C
#define C16_ESTADO  0xD0
#define C16_MASCARA 0xD4
#define C16_MODO    0xD6
#define C16_FF      0xD8

static uint32_t reserva_fis, reserva_usada;
static uint8_t *reserva_cpu;
static uint32_t total[8];            /* bytes programados en cada canal */
static uint8_t tc_visto[2];          /* bits TC leídos del estado y aún no preguntados */

bool isa_dma_canal_valido(uint8_t canal) { return canal < 8 && canal != 4; }

/* ¿[ini, fim) está entero en una región libre del E820? */
static bool e820_livre(uint32_t ini, uint32_t fim)
{
    for (uint32_t i = 0; i < info_boot.mapa_qtd; i++) {
        regiao_e820_t *r = &mapa_memoria[i];
        if (r->tipo != E820_LIVRE) continue;
        if (r->base <= ini && r->base + r->tamanho >= fim) {
            /* y ninguna reservada encima (BIOS mal hechas: lo reservado gana, como en fisica.c) */
            for (uint32_t k = 0; k < info_boot.mapa_qtd; k++) {
                regiao_e820_t *q = &mapa_memoria[k];
                if (q->tipo != E820_LIVRE && q->base < fim && q->base + q->tamanho > ini) return false;
            }
            return true;
        }
    }
    return false;
}

void isa_dma_reservar(void)
{
    /* Primero entre 1 y 16 MB. Casi nunca queda: el ramdisk va justo detrás del kernel y pasa de
     * 16 MB. Entonces, en la memoria convencional (por debajo de 640 KB, donde el DOS ponía sus
     * búferes de DMA): el mapa de bits la da toda por ocupada (fisica.c) y después del arranque solo
     * se usan el trampolín de los AP (7000h) y lo que hay por debajo de 20000h; el EBDA va por
     * encima de 80000h. Se usa un bloque de 60000h o 40000h que el E820 diga libre entero. */
    reserva_fis = memfisica_alocar_baixa(ISA_DMA_RESERVA / PAGINA, ISA_DMA_RESERVA, 16u * 1024 * 1024);
    static const uint32_t convencional[] = { 0x60000, 0x40000 };
    for (unsigned i = 0; !reserva_fis && i < sizeof convencional / sizeof convencional[0]; i++)
        if (e820_livre(convencional[i], convencional[i] + ISA_DMA_RESERVA)) reserva_fis = convencional[i];
    if (reserva_fis) klog("[ISA] DMA: %u KB reservados en %x para el 8237\n", ISA_DMA_RESERVA / 1024, reserva_fis);
    else klog("[ISA] DMA: no queda memoria por debajo de 16 MB; sin DMA ISA\n");
}

uint32_t isa_dma_bufer(uint32_t bytes, bool de16, void **cpu)
{
    if (!reserva_fis || !bytes) return 0;
    uint32_t limite = de16 ? 128u * 1024 : 64u * 1024;
    if (bytes > limite) return 0;
    bytes = (bytes + PAGINA - 1) & ~(PAGINA - 1);
    bool i0 = int_salva_desliga();
    uint32_t ini = reserva_usada;
    /* si cruzaría el límite del canal, empieza en el siguiente */
    if ((reserva_fis + ini) / limite != (reserva_fis + ini + bytes - 1) / limite)
        ini = ((reserva_fis + ini + limite - 1) & ~(limite - 1)) - reserva_fis;
    if (ini + bytes > ISA_DMA_RESERVA) { int_restaura(i0); return 0; }
    reserva_usada = ini + bytes;
    int_restaura(i0);
    if (!reserva_cpu) {
        __asm__ volatile("wbinvd" ::: "memory");
        uint32_t v = janela_mapear_tipo(reserva_fis, ISA_DMA_RESERVA, PG_PCD | PG_PWT);
        if (!v) return 0;
        reserva_cpu = (uint8_t *)v;
    }
    memset(reserva_cpu + ini, 0, bytes);
    *cpu = reserva_cpu + ini;
    return reserva_fis + ini;
}

static void limpar_ff(uint8_t canal) { porta_escreve8(canal < 4 ? C8_FF : C16_FF, 0); }

void isa_dma_ativar(uint8_t canal, bool sim)
{
    if (!isa_dma_canal_valido(canal)) return;
    porta_escreve8(canal < 4 ? C8_MASCARA : C16_MASCARA, (uint8_t)((canal & 3) | (sim ? 0 : 4)));
}

bool isa_dma_programar(uint8_t canal, uint32_t fis, uint32_t bytes, uint8_t modo)
{
    if (!isa_dma_canal_valido(canal) || !bytes || fis + bytes > 16u * 1024 * 1024) return false;
    bool de16 = canal >= 4;
    uint32_t limite = de16 ? 128u * 1024 : 64u * 1024;
    if (bytes > limite || fis / limite != (fis + bytes - 1) / limite) return false;
    if (de16 && ((fis | bytes) & 1)) return false;
    uint32_t end = de16 ? (fis >> 1) & 0xFFFF : fis & 0xFFFF;
    uint32_t cont = (de16 ? bytes / 2 : bytes) - 1;
    uint8_t pag = (uint8_t)((fis >> 16) & (de16 ? 0xFE : 0xFF));
    bool i0 = int_salva_desliga();
    isa_dma_ativar(canal, false);
    limpar_ff(canal);
    porta_escreve8(canal < 4 ? C8_MODO : C16_MODO, (uint8_t)((canal & 3) | (modo & 0x1C) | 0x40));   /* modo simple */
    porta_escreve8(porta_end[canal], (uint8_t)end);
    porta_escreve8(porta_end[canal], (uint8_t)(end >> 8));
    porta_escreve8(porta_pag[canal], pag);
    limpar_ff(canal);
    porta_escreve8(porta_cont[canal], (uint8_t)cont);
    porta_escreve8(porta_cont[canal], (uint8_t)(cont >> 8));
    total[canal] = bytes;
    tc_visto[de16] &= (uint8_t)~(1u << (canal & 3));
    int_restaura(i0);
    return true;
}

static uint16_t ler_contador(uint8_t canal)
{
    limpar_ff(canal);
    uint16_t lo = porta_le8(porta_cont[canal]);
    return (uint16_t)(lo | porta_le8(porta_cont[canal]) << 8);
}

uint32_t isa_dma_feito(uint8_t canal)
{
    if (!isa_dma_canal_valido(canal) || !total[canal]) return 0;
    bool i0 = int_salva_desliga();
    /* El contador avanza mientras se lee byte a byte: dos lecturas iguales (o casi) seguidas. */
    uint16_t a = ler_contador(canal), b = a;
    for (int t = 0; t < 4; t++) {
        b = ler_contador(canal);
        uint16_t d = (uint16_t)(a - b);
        if (d < 8) break;
        a = b;
    }
    int_restaura(i0);
    uint32_t unidades = canal >= 4 ? total[canal] / 2 : total[canal];
    uint32_t resta = (uint32_t)b + 1;        /* 0xFFFF tras el final = 0 que faltan (vuelve a empezar) */
    if (resta > unidades) resta = unidades;
    uint32_t feito = unidades - resta;
    return canal >= 4 ? feito * 2 : feito;
}

bool isa_dma_terminou(uint8_t canal)
{
    if (!isa_dma_canal_valido(canal)) return false;
    bool de16 = canal >= 4;
    bool i0 = int_salva_desliga();
    tc_visto[de16] |= porta_le8(de16 ? C16_ESTADO : C8_ESTADO) & 0x0F;
    bool r = tc_visto[de16] & (1u << (canal & 3));
    tc_visto[de16] &= (uint8_t)~(1u << (canal & 3));
    int_restaura(i0);
    return r;
}

/* ------------------------------------------------------------ dispositivos */

static info_isa_t disp[ISA_DISPOSITIVOS];
static int ndisp;

void isa_registrar(const char *nome, uint16_t porta, uint8_t irq, uint8_t dma8, uint8_t dma16, const char *detalhe)
{
    if (ndisp >= ISA_DISPOSITIVOS) return;
    info_isa_t *d = &disp[ndisp++];
    memset(d, 0, sizeof *d);
    txt_copia_n(d->nome, nome, sizeof d->nome);
    txt_copia_n(d->detalhe, detalhe ? detalhe : "", sizeof d->detalhe);
    d->porta = porta;
    d->irq = irq;
    d->dma8 = dma8;
    d->dma16 = dma16;
}

int32_t isa_info(uint32_t ptr, uint32_t n)
{
    info_isa_t *v = (info_isa_t *)ptr;
    uint32_t k = (uint32_t)ndisp < n ? (uint32_t)ndisp : n;
    if (k && !usuario_ok(v, k * sizeof *v, true)) return ERRO_ENDERECO;
    for (uint32_t i = 0; i < k; i++) v[i] = disp[i];
    return (int32_t)ndisp;
}
