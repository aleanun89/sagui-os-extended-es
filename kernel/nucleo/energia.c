// SPDX-License-Identifier: GPL-3.0-only
/* energia.c - reiniciar y apagar la máquina (D113, D216, D450).
 *
 * D450: un solo camino, siempre en la tarea "apagar" del kernel (CPU 0), con un orden fijo y un
 * plazo en cada paso. Si un paso no termina a tiempo se apunta y se sigue: nada puede dejar la
 * máquina esperando para siempre.
 *
 *   1. inicio;
 *   2. servicios: el gestor de servicios para los suyos (como mucho 20 s, supervisor.c);
 *   3. discos: grabar lo pendiente y desmontar (montagem_desmontar_tudo; cada orden de los drivers
 *      de disco tiene su plazo por el TSC);
 *   4. ACPI: _TTS(5) y, al apagar, _PTS(5) por uACPI si está activo (como mucho 3 s);
 *      --- desde aquí, las interrupciones cerradas para siempre y nada de planificar ---
 *      procesadores: IPI de parada a los secundarios (como mucho 100 ms);
 *   5. drivers, al revés que al arrancar: sonido y red sin DMA, USB parado (xHCI, EHCI), aviso
 *      de apagado al NVMe (5 s cada uno), USB y discos sin DMA, interrupciones del IOAPIC y del
 *      8259 enmascaradas. La GPU no se toca: es la que enseña las marcas en la pantalla;
 *   6. reiniciar: RESET_REG de la FADT (1 s), 8042 (0xFE, 0,5 s), 0xCF9 (2 y 0x0E, 0,5 s) y
 *      triple fallo; apagar: SLP_TYP|SLP_EN del \_S5_ (3 s) y, si sigue encendida, el aviso.
 *
 * Cada paso deja una marca en el registro (serie), en el rastro que sobrevive a un reinicio en
 * caliente (rastro.h, 0x00450000 | modo << 12 | paso) y, desde la parada de los procesadores, en
 * la pantalla. */
#include "idioma.h"
#include "acpi.h"
#include "cpu.h"
#include "console.h"
#include "gui.h"
#include "ioapic.h"
#include "montagem.h"
#include "nvme.h"
#include "pci.h"
#include "portas.h"
#include "prazo.h"
#include "rastro.h"
#include "relogio.h"
#include "sensores_conv.h"
#include "smp.h"
#include "supervisor.h"
#include "texto.h"
#include "uacpi_sagui.h"
#include "usb.h"
#include "xhci.h"

#define RASTRO_ENERGIA(modo, paso) (0x00450000u | ((uint32_t)(modo) << 12) | (uint32_t)(paso))

static uint64_t t0;
static int modo_atual;
static bool na_tela;   /* desde la parada de los procesadores: las marcas también en la pantalla */

static uint32_t ms_passados(void)
{
    uint32_t khz = hw_tsc_khz();
    if (!khz || !t0) return 0;
    return (uint32_t)sconv_div64(prazo_tsc() - t0, khz);
}

/* La marca de un paso: registro, rastro y (en la parte final) pantalla. kprintf ya escribe en el
 * serie y en el registro: con la pantalla no hace falta el klog. */
static void marca(uint32_t paso, const char *fmt, ...)
{
    char txt[160];
    va_list ap;
    va_start(ap, fmt);
    formatar(txt, sizeof txt, fmt, ap);
    va_end(ap);
    rastro_marca(RASTRO_ENERGIA(modo_atual, paso));
    if (na_tela) kprintf(" [ENERGIA] +%u ms, paso %u: %s\n", ms_passados(), paso, txt);
    else klog("[ENERGIA] +%u ms, paso %u: %s\n", ms_passados(), paso, txt);
}

static void esperar_ms(uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    while (!prazo_vencido(&p)) ;
}

static void esperar_us(uint32_t us)
{
    uint32_t khz = hw_tsc_khz();
    if (!khz) { for (uint32_t i = 0; i < us; i++) porta_pausa(); return; }
    uint64_t fim = prazo_tsc() + (uint64_t)(khz / 1000u) * us;
    while (prazo_tsc() < fim) ;
}

/* ------------------------------------------------------------ drivers (paso 5) */

/* El registro de comando PCI: sin bus master (bit 2) y sin INTx (bit 10); y el MSI y el MSI-X
 * apagados en la lista de capacidades. Solo con lo que ya no hace falta. */
static void pci_silenciar(const pci_disp_t *d)
{
    uint32_t cmd = pci_le32(d, 0x04);
    if ((cmd & 0xFFFF) == 0xFFFF) return;
    if (cmd & (1u << 20)) {   /* hay lista de capacidades */
        uint8_t p = (uint8_t)(pci_le32(d, 0x34) & 0xFC);
        for (int volta = 0; p >= 0x40 && volta < 48; volta++) {
            uint32_t v = pci_le32(d, p);
            if ((uint8_t)v == 0x05 && (v & (1u << 16))) pci_escreve32(d, p, v & ~(1u << 16));
            else if ((uint8_t)v == 0x11 && (v & (1u << 31))) pci_escreve32(d, p, v & ~(1u << 31));
            p = (uint8_t)((v >> 8) & 0xFC);
        }
    }
    /* Solo los 16 bits de comando: escribir 0 en los de estado no borra nada (se borran con 1). */
    pci_escreve32(d, 0x04, (cmd & 0xFFFFu & ~(1u << 2)) | (1u << 10));
}

/* Qué clases se silencian y en qué tanda: 1 = sonido, red, inalámbricos y comunicaciones
 * (antes que el USB); 2 = USB y discos (después de pararlos). Ni la GPU (03), ni los puentes
 * (06), ni el SMBus, ni los del sistema (08): por esos pasan la pantalla y el propio reinicio. */
static int tanda(const pci_disp_t *d)
{
    switch (d->classe) {
    case 0x02: case 0x04: case 0x07: case 0x0D: return 1;
    case 0x01: return 2;
    case 0x0C: return d->subclasse == 0x03 ? 2 : 0;
    default: return 0;
    }
}

static pci_disp_t lista_pci[96];
static int qtd_pci;

static int silenciar_tanda(int t)
{
    int n = 0;
    for (int i = 0; i < qtd_pci; i++)
        if (tanda(&lista_pci[i]) == t) { pci_silenciar(&lista_pci[i]); n++; }
    return n;
}

static void parar_drivers(void)
{
    qtd_pci = pci_listar(lista_pci, (int)(sizeof lista_pci / sizeof lista_pci[0]));
    marca(5, "drivers (%d funciones PCI): sonido, red e inalambricos sin DMA", qtd_pci);
    int n = silenciar_tanda(1);
    int x = xhci_parar_todos();
    int eh = ehci_parar_todos();
    marca(5, "  %d silenciados; USB parado: xHCI %d, EHCI %d", n, x, eh);
    marca(5, "  NVMe: aviso de apagado (hasta 5 s cada uno)");
    int nv = nvme_desligar_todos();
    n = silenciar_tanda(2);
    if (ioapic_ativo())
        for (uint8_t irq = 0; irq < 16; irq++) ioapic_mascarar(irq, true);
    porta_escreve8(0x21, 0xFF);   /* 8259: todo enmascarado (con o sin IOAPIC) */
    porta_escreve8(0xA1, 0xFF);
    marca(5, "  NVMe %d terminados; USB y discos sin DMA (%d); IOAPIC y 8259 enmascarados", nv, n);
}

/* ------------------------------------------------------------ los métodos de reinicio (paso 6) */

static void reset_acpi(void)
{
    char d[64];
    acpi_reset_descrever(d, sizeof d);
    if (!acpi_reset_disponivel()) { marca(6, "ACPI RESET_REG: %s", d); return; }
    marca(6, "ACPI RESET_REG: %s (espera 1 s)", d);
    acpi_reset_escrever();
    esperar_ms(1000);
}

static void reset_teclado(void)
{
    marca(6, "8042: 0xFE al puerto 0x64 (8042 %s en la FADT; espera 0,5 s)",
          acpi_8042_declarado() ? "declarado" : "no declarado");
    /* Como Linux: diez veces, esperando a que el búfer de entrada esté libre (10 ms como mucho:
     * sin 8042, el puerto lee 0xFF y el bit no se apaga nunca). */
    for (int i = 0; i < 10; i++) {
        prazo_t p = prazo_ms(10);
        while ((porta_le8(0x64) & 2) && !prazo_vencido(&p)) ;
        esperar_us(50);
        porta_escreve8(0x64, 0xFE);
        esperar_us(50);
    }
    esperar_ms(500);
}

static void reset_cf9(void)
{
    marca(6, "0xCF9: 0x02 y luego 0x0E (reinicio completo; espera 0,5 s)");
    uint8_t v = (uint8_t)(porta_le8(0xCF9) & ~0x0Eu);
    porta_escreve8(0xCF9, (uint8_t)(v | 0x02));
    esperar_us(50);
    porta_escreve8(0xCF9, (uint8_t)(v | 0x0E));
    esperar_us(50);
    esperar_ms(500);
}

static SEM_RETORNO void reset_triplo(void)
{
    marca(6, "triple fallo (IDT vacia + int3)");
    struct EMPACOTADO { uint16_t limite; uint32_t base; } vazia = { 0, 0 };
    __asm__ volatile("lidt %0; int3" : : "m"(vazia));
    parar_maquina();
}

/* El orden de Linux (ACPI, teclado, 0xCF9, triple fallo), empezando por el pedido: los otros
 * dos siguen después y el triple fallo siempre al final. EFI ResetSystem no se usa: el kernel
 * es de 32 bits y no guarda los servicios de ejecución del firmware (D450). */
static SEM_RETORNO void reiniciar_metodos(int primeiro)
{
    if (primeiro == ENERGIA_RESET_TRIPLE) reset_triplo();
    if (primeiro < ENERGIA_RESET_ACPI || primeiro > ENERGIA_RESET_CF9) primeiro = ENERGIA_RESET_ACPI;
    for (int k = 0; k < 3; k++) {
        int m = ENERGIA_RESET_ACPI + (primeiro - ENERGIA_RESET_ACPI + k) % 3;
        if (m == ENERGIA_RESET_ACPI) reset_acpi();
        else if (m == ENERGIA_RESET_TECLADO) reset_teclado();
        else reset_cf9();
    }
    reset_triplo();
}

/* Sin la tarea "apagar" (muy al principio del arranque): solo los métodos. */
SEM_RETORNO void maquina_reiniciar(void)
{
    int_desliga();
    klog("[SAGUI] reiniciando\n");
    t0 = prazo_tsc();
    modo_atual = ENERGIA_REINICIAR;
    reiniciar_metodos(ENERGIA_RESET_ACPI);
}

/* ------------------------------------------------------------ el orden */

/* El aviso de siempre (D113) cuando la máquina no se apaga sola. Con el escritorio y antes de la
 * parte final, en una notificación; si no, en la pantalla de texto. */
static SEM_RETORNO void aviso_pode_apagar(void)
{
    klog("[SAGUI] ya se puede apagar el PC\n");
    if (gui_ativa() && !na_tela) {
        gui_aviso(_("Ya puedes apagar el ordenador."));
        int_liga();
        relogio_dormir_ms(500);      /* el gestor de ventanas dibuja el aviso */
    } else {
        console_cor(COR_BRANCO, COR_AZUL);
        kprintf("\n Ya puedes apagar el ordenador. \n");
        console_cor(COR_CINZA, COR_PRETO);
        console_mostrar();
    }
    parar_maquina();
}

SEM_RETORNO void energia_executar(int modo, int metodo)
{
    t0 = prazo_tsc();
    modo_atual = modo;
    bool reiniciar = modo == ENERGIA_REINICIAR;
    klog(reiniciar ? "[SAGUI] reiniciando\n" : "[SAGUI] apagando\n");
    marca(1, "%s (metodo pedido %d, CPU %u)", reiniciar ? "reinicio" : "apagado", metodo, cpu_atual());

    marca(2, "servicios (hasta 20 s)");
    supervisor_parar_servicos(reiniciar ? SERV_MOTIVO_REINICIAR : SERV_MOTIVO_APAGAR);

    marca(3, "discos: grabar y desmontar");
    bool i0 = int_salva_desliga();   /* como siempre (D040): nadie escribe mientras tanto */
    montagem_desmontar_tudo();
    int_restaura(i0);

    marca(4, "ACPI: %s", uacpi_sagui_preparar_energia(!reiniciar, 3000));

    /* Sin apagado ACPI: como siempre (D113), el aviso y parada, sin tocar nada más. */
    if (!reiniciar && !acpi_disponivel()) {
        acpi_apagar();   /* solo apunta el motivo en el registro */
        aviso_pode_apagar();
    }

    /* Desde aquí no se vuelve a planificar: interrupciones cerradas y la pantalla para las marcas. */
    int_desliga();
    console_esconder(false);
    console_cor(COR_BRANCO, COR_AZUL);
    console_limpar();
    na_tela = true;
    kprintf("\n %s SaguiOS...\n\n", reiniciar ? "Reiniciando" : "Apagando");
    uint32_t paradas = smp_parar_secundarias(100);
    marca(4, "procesadores: %u de %u secundarios parados", paradas, smp_arrancadas());
    parar_drivers();

    if (reiniciar) reiniciar_metodos(metodo);

    marca(6, "S5 por ACPI (espera 3 s)");
    acpi_apagar();   /* solo vuelve si no se apagó */
    marca(6, "la maquina sigue encendida");
    aviso_pode_apagar();
}
