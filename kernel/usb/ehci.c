// SPDX-License-Identifier: GPL-3.0-only
/* ehci.c - driver del controlador USB 2.0 (EHCI) (H29). Ver D025.
 *
 * El EHCI lee de la memoria dos listas: la "periódica" (no usada aquí) y la
 * "asíncrona", un anillo de cabezas de cola (QH), una por endpoint, cada
 * una apuntando a una lista de descriptores de transferencia (qTD). Los
 * formatos de QH, qTD y de los registros son los de la especificación EHCI.
 *
 * Diseño de este driver, pensado para ser simple y previsible:
 *   - una transferencia por vez (un bloqueo protege el buffer y los qTD):
 *     arma los qTD, cuelga una QH en el anillo, espera a que el controlador termine,
 *     saca la QH;
 *   - la espera tiene dos modos (D095). Síncrono: mira el bit "activo" de los qTD
 *     en un bucle, sin ceder la CPU; es el del comienzo del arranque y del pánico, cuando
 *     todavía no hay procesos ni interrupciones confiables. Asíncrono: el
 *     último qTD pide interrupción al terminar (IOC) y el proceso duerme hasta
 *     que llega, dejando la CPU para los demás; si la IRQ del controlador
 *     no llega (línea mal enrutada), la espera cae a "ceder el turno y
 *     comprobar de nuevo", que no congela nada, sólo gasta CPU;
 *   - el tiempo se mide con el contador de microcuadros del propio controlador
 *     (FRINDEX, 8 por milisegundo), que anda incluso con las interrupciones de
 *     la CPU desactivadas;
 *   - los datos pasan por un buffer de 128 KB en memoria física contigua del
 *     kernel (el controlador necesita direcciones físicas; en SaguiOS el
 *     kernel ve la memoria física 1:1, D008).
 * Antes de todo, el sistema pide el controlador a la BIOS (que lo usaba para el
 * pendrive de arranque), por el procedimiento "BIOS/OS handoff" de la especificación. */
#include "usb.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "paginacao.h"
#include "portas.h"
#include "processo.h"
#include "relogio.h"
#include "texto.h"
#include "trava.h"
#include "irq_rotas.h"

/* Registros de operación (desplazamiento / 4) */
#define USBCMD     0
#define USBSTS     1
#define USBINTR    2
#define FRINDEX    3
#define CTRLDSSEG  4
#define PERIODIC   5
#define ASYNCLIST  6
#define CONFIGFLAG 16
#define PORTSC     17

#define CMD_RS     (1u << 0)
#define CMD_RESET  (1u << 1)
#define CMD_ASE    (1u << 5)
#define CMD_PSE    (1u << 4)
#define CMD_IAAD   (1u << 6)
#define STS_INT    (1u << 0)    /* una transferencia con IOC terminó */
#define STS_ERRINT (1u << 1)    /* una transferencia terminó con error */
#define STS_IAA    (1u << 5)
#define STS_HALTED (1u << 12)
#define STS_PSS    (1u << 14)
#define STS_ASS    (1u << 15)

#define P_CCS  (1u << 0)
#define P_CSC  (1u << 1)
#define P_PE   (1u << 2)
#define P_PEC  (1u << 3)
#define P_OCC  (1u << 5)
#define P_PR   (1u << 8)
#define P_LS   (3u << 10)
#define P_PP   (1u << 12)
#define P_PO   (1u << 13)
#define P_MUDANCAS (P_CSC | P_PEC | P_OCC)   /* bits "escribir 1 para limpiar" */

/* Formatos de 64 bits (apéndice B de la especificación EHCI). Un controlador
 * que se declara capaz de direcciones de 64 bits (HCCPARAMS bit 0, caso de los
 * EHCI de Intel del G470) lee SIEMPRE la versión larga de las estructuras: después
 * de los 5 punteros de página vienen 5 palabras más con la parte alta de cada
 * uno. La primera versión de este driver usaba el formato corto, pegado: en el
 * G470 el controlador leía como "parte alta" los bytes de la estructura siguiente y
 * mandaba los datos a direcciones absurdas (error 17). El QEMU se declara sólo
 * de 32 bits y no mostraba el problema. Ahora las estructuras tienen siempre el
 * tamaño largo, con la parte alta en cero; un controlador de 32 bits
 * simplemente no lee el final. */

/* qTD: 13 palabras (52 bytes), ocupa 64, alineado en 32 */
typedef struct {
    volatile uint32_t proximo, alternativo, ficha;
    volatile uint32_t buffer[5];
    volatile uint32_t buffer_alto[5];
    uint32_t preenchimento[3];
} __attribute__((aligned(32))) qtd_t;

/* QH: 17 palabras (68 bytes), ocupa 96, alineado en 32 */
typedef struct {
    volatile uint32_t ligacao, caracteristicas, capacidades, atual;
    volatile uint32_t prox_qtd, alt_qtd, ficha;
    volatile uint32_t buffer[5];
    volatile uint32_t buffer_alto[5];
    uint32_t preenchimento[7];
} __attribute__((aligned(32))) qh_t;

_Static_assert(sizeof(qtd_t) == 64, "qTD no formato de 64 bits ocupa 64 bytes");
_Static_assert(sizeof(qh_t) == 96, "QH no formato de 64 bits ocupa 96 bytes");

#define T_TERMINA    1u
#define T_QH         (1u << 1)
#define F_ATIVO      (1u << 7)
#define F_PAROU      (1u << 6)
#define F_ERROS      0x3Cu        /* buffer, babble, transacción, microcuadro perdido */
#define PID_SAIDA    0u
#define PID_ENTRADA  1u
#define PID_SETUP    2u

#define MAX_QTD      8
#define QTD_MAX_BYTES 16384
#define BUFFER_BYTES  131072      /* 8 qTD de 16 KB (cada uno aceptaría hasta 20 KB con 5 páginas) */
#define ESPERAS_SEM_IRQ 6         /* esperas seguidas despertadas sólo por el reloj: la IRQ no llega */

static ehci_t ctls[USB_MAX_CONTROLADORES];
static int qtd_ctls;
static bool assincrono;            /* D095: ya hay planificador e IRQ: la espera puede dormir */
static void ehci_recuperar(void);
/* Una transferencia por vez en todo el sistema: el buffer, la QH de trabajo y
 * los qTD son únicos. Quien duerme esperando el DMA deja que otros procesos
 * entren aquí; el bloqueo los pone en cola. */
static trava_t trava_ehci = { .ao_roubar = ehci_recuperar };

/* Memoria de trabajo. Cada controlador tiene SU página de estructuras
 * (cabeza del anillo, cola de trabajo, setup y qTD): si dos controladores
 * apuntaran al mismo anillo, los dos ejecutarían la misma transferencia,
 * cada uno en su bus, y una estropearía a la otra (error 19, G470).
 * El buffer de datos es uno solo: sólo lo toca el controlador de la transferencia en curso,
 * y las transferencias son una por vez (interrupciones desactivadas).
 * Los punteros abajo apuntan a la página del controlador en uso. */
static qh_t *cabeca, *fila;
static qtd_t *qtds;
static uint8_t *setup_pacote;
static uint8_t *buffer;

static void usar(ehci_t *c)
{
    uint32_t p = c->estruturas;
    cabeca = (qh_t *)p;
    fila = (qh_t *)(p + 128);
    setup_pacote = (uint8_t *)(p + 256);
    qtds = (qtd_t *)(p + 512);
}

int ehci_quantidade(void) { return qtd_ctls; }
ehci_t *ehci_controlador(int i) { return i < qtd_ctls ? &ctls[i] : NULL; }

/* ------------------------------------------------------------ tiempo */

static void esperar_bruto_us(uint32_t us)
{
    for (uint32_t i = 0; i < us; i++) porta_pausa();   /* ~1 µs cada uno, sin depender del controlador */
}

void ehci_esperar_ms(ehci_t *c, uint32_t ms)
{
    if (!c || (c->op[USBSTS] & STS_HALTED)) { esperar_bruto_us(ms * 1000); return; }
    /* FRINDEX cuenta microcuadros (125 µs) en 14 bits. */
    uint32_t alvo = ms * 8, passou = 0, antes = c->op[FRINDEX] & 0x3FFF;
    for (uint32_t guarda = 0; passou < alvo && guarda < ms * 20000 + 1000; guarda++) {
        uint32_t agora = c->op[FRINDEX] & 0x3FFF;
        passou += (agora - antes) & 0x3FFF;
        antes = agora;
    }
}

/* ------------------------------------------------------------ inicialización */

/* Pide el controlador a la BIOS: enciende el bit "el sistema lo quiere" en la capacidad
 * USBLEGSUP y espera a que la BIOS lo suelte; después apaga las interrupciones SMI. */
static void tomar_da_bios(ehci_t *c)
{
    uint32_t hcc = *(volatile uint32_t *)(c->cap + 8);
    uint8_t eecp = (uint8_t)((hcc >> 8) & 0xFF);
    for (int volta = 0; eecp >= 0x40 && volta < 8; volta++) {
        uint32_t v = pci_le32(&c->pci, eecp);
        if ((v & 0xFF) == 1) {   /* USBLEGSUP */
            if (v & (1u << 16)) {
                pci_escreve32(&c->pci, eecp, v | (1u << 24));
                for (int i = 0; i < 1000 && (pci_le32(&c->pci, eecp) & (1u << 16)); i++)
                    esperar_bruto_us(1000);
                c->bios = (pci_le32(&c->pci, eecp) & (1u << 16)) ? -1 : 1;
                klog("[EHCI] BIOS %s o controlador\n", c->bios < 0 ? "NAO soltou" : "soltou");
            }
            pci_escreve32(&c->pci, (uint8_t)(eecp + 4), 0);   /* USBLEGCTLSTS: sin SMI */
            return;
        }
        eecp = (uint8_t)((v >> 8) & 0xFF);
    }
}

static bool alocar_buffer(void)
{
    if (buffer) return true;
    uint32_t b = memfisica_alocar_seguidas(BUFFER_BYTES / PAGINA);
    if (!b) return false;
    buffer = (uint8_t *)b;
    return true;
}

/* Una página por controlador: cabeza (96), cola de trabajo (96), paquete de
 * setup y 8 qTD de 64 bytes, sin ninguna estructura pegada a otra. */
static bool alocar_estruturas(ehci_t *c)
{
    uint32_t p = memfisica_alocar();
    if (!p) return false;
    memset((void *)p, 0, PAGINA);
    c->estruturas = p;
    usar(c);
    /* La cabeza del anillo asíncrono: apunta a sí misma, marcada como "H". */
    cabeca->ligacao = (uint32_t)cabeca | T_QH;
    cabeca->caracteristicas = (1u << 15) | (2u << 12);
    cabeca->capacidades = 1u << 30;
    cabeca->prox_qtd = T_TERMINA;
    cabeca->alt_qtd = T_TERMINA;
    cabeca->ficha = F_PAROU;   /* nunca ejecuta nada */
    return true;
}

static bool iniciar(ehci_t *c)
{
    uint32_t bar = pci_le32(&c->pci, 0x10) & ~0xFu;
    if (!bar) return false;
    /* Comando PCI: espacio de memoria + maestro del bus (DMA). */
    pci_escreve32(&c->pci, 0x04, pci_le32(&c->pci, 0x04) | 0x6);
    c->cap = (volatile uint8_t *)janela_mapear(bar, 4096);
    if (!c->cap) return false;
    c->op = (volatile uint32_t *)(c->cap + c->cap[0]);
    uint32_t hcs = *(volatile uint32_t *)(c->cap + 4);
    c->portas = (int)(hcs & 0xF);
    c->companheiros = (int)((hcs >> 12) & 0xF);
    for (int p = 0; p < USB_MAX_PORTAS; p++) c->resultado[p] = -1;   /* todavía no intentado */
    c->enderecos64 = *(volatile uint32_t *)(c->cap + 8) & 1;
    if (c->portas > USB_MAX_PORTAS) c->portas = USB_MAX_PORTAS;
    if (!alocar_estruturas(c)) return false;
    tomar_da_bios(c);

    /* Detiene y reinicia el controlador. */
    c->op[USBCMD] &= ~CMD_RS;
    for (int i = 0; i < 100 && !(c->op[USBSTS] & STS_HALTED); i++) esperar_bruto_us(1000);
    c->op[USBCMD] |= CMD_RESET;
    for (int i = 0; i < 250 && (c->op[USBCMD] & CMD_RESET); i++) esperar_bruto_us(1000);
    if (c->op[USBCMD] & CMD_RESET) { klog("[EHCI] reset nao terminou\n"); return false; }

    c->op[USBINTR] = 0;                       /* sin interrupciones: el driver consulta */
    if (*(volatile uint32_t *)(c->cap + 8) & 1) c->op[CTRLDSSEG] = 0;   /* 64 bits: parte alta = 0 */
    c->op[PERIODIC] = 0;
    c->op[ASYNCLIST] = (uint32_t)cabeca;
    /* Umbral de interrupción de 1 microcuadro (125 µs): con el valor de 8, cada
     * aviso de fin de transferencia atrasaba 1 ms, y un comando de pendrive
     * tiene tres transferencias (D095). */
    c->op[USBCMD] = (1u << 16) | CMD_RS | CMD_ASE;   /* en marcha, lista asíncrona */
    for (int i = 0; i < 100 && (c->op[USBSTS] & STS_HALTED); i++) esperar_bruto_us(1000);
    c->op[CONFIGFLAG] = 1;                     /* todos los puertos para este controlador */
    esperar_bruto_us(5000);
    if (hcs & (1u << 4))                       /* control de energía por puerto: enciende todos */
        for (int p = 0; p < c->portas; p++)
            c->op[PORTSC + p] = (c->op[PORTSC + p] & ~P_MUDANCAS) | P_PP;
    ehci_esperar_ms(c, 100);                   /* encendido y conexión estables (USB 2.0, 7.1.7.3) */
    c->proximo_endereco = 1;
    c->ok = !(c->op[USBSTS] & STS_HALTED);
    klog("[EHCI] controlador %02x:%02x.%x (%04x:%04x) em %x: %d portas, %s\n",
         c->pci.barramento, c->pci.dispositivo, c->pci.funcao, c->pci.fabricante, c->pci.modelo,
         bar, c->portas, c->ok ? "rodando" : "PARADO");
    return c->ok;
}

int ehci_iniciar_todos(void)
{
    pci_disp_t achados[USB_MAX_CONTROLADORES];
    int n = pci_procurar(0x0C, 0x03, 0x20, achados, USB_MAX_CONTROLADORES);   /* serial bus, USB, EHCI */
    if (n == 0 || !alocar_buffer()) return 0;
    bool i0 = int_salva_desliga();
    for (int i = 0; i < n; i++) {
        ehci_t *c = &ctls[qtd_ctls];
        memset(c, 0, sizeof *c);
        c->pci = achados[i];
        if (iniciar(c)) qtd_ctls++;
    }
    int_restaura(i0);
    return qtd_ctls;
}

/* ------------------------------------------------------------ puertos */

uint32_t ehci_porta_estado(ehci_t *c, int p) { return c->op[PORTSC + p]; }

bool ehci_porta_mudou(ehci_t *c, int p)
{
    uint32_t v = c->op[PORTSC + p];
    if (!(v & P_CSC)) return false;
    c->op[PORTSC + p] = (v & ~P_MUDANCAS) | P_CSC;   /* escribe 1 para limpiar */
    return true;
}

/* D267: el puerto solo se cede si hay un driver UHCI u OHCI que lo recoja; si el
 * chipset declara compañeros y no tenemos controlador para ellos, ceder el
 * puerto lo dejaría inútil. */
static bool companheiro_usavel(const ehci_t *c)
{
    return c->companheiros && (uhci_quantidade() || ohci_quantidade());
}

/* D227: las esperas largas (los 50 ms del reset y los 10 de recuperación) van por
 * usb_pausa_ms: en la tarea "usb" duermen con las interrupciones encendidas, en vez
 * de congelar el sistema cada vez que se enchufa algo. Los milisegundos de comprobar
 * que el controlador ha terminado el reset siguen girando (son pocos). */
int ehci_porta_resetar(ehci_t *c, int p)
{
    uint32_t v = c->op[PORTSC + p];
    if (!(v & P_CCS)) return USB_ERRO;
    if (v & P_PO) return USB_PORTA_CEDIDA;         /* ya es del compañero */
    if ((v & P_LS) == (1u << 10)) {                /* estado K: baja velocidad -> compañero */
        klog("[EHCI] porta %d: dispositivo USB 1.x de baixa velocidade -> %s\n", p,
             companheiro_usavel(c) ? "companheiro UHCI/OHCI" : "sem companheiro, ignorado");
        if (!companheiro_usavel(c)) return USB_PORTA_USB1;
        c->op[PORTSC + p] = (v & ~P_MUDANCAS) | P_PO;
        return USB_PORTA_CEDIDA;
    }
    c->op[PORTSC + p] = ((v & ~P_MUDANCAS) & ~P_PE) | P_PR;
    usb_pausa_ms(c, 50);                           /* la especificación USB pide 50 ms de reset en la raíz */
    c->op[PORTSC + p] = (c->op[PORTSC + p] & ~P_MUDANCAS) & ~P_PR;
    for (int i = 0; i < 20 && (c->op[PORTSC + p] & P_PR); i++) ehci_esperar_ms(c, 1);
    usb_pausa_ms(c, 10);
    v = c->op[PORTSC + p];
    if (!(v & P_PE)) {                             /* no habilitó: velocidad completa -> compañero */
        /* Sin compañeros (G470: los chipsets serie 6 sólo tienen el hub interno),
         * entregar el puerto a nadie lo dejaría inútil: se desiste de este intento
         * (D227: puede ser un contacto que aún no estaba firme, y se vuelve a probar). */
        klog("[EHCI] porta %d: dispositivo USB 1.x (velocidade completa) -> %s\n", p,
             companheiro_usavel(c) ? "companheiro UHCI/OHCI" : "sem companheiro, ignorado");
        if (!(v & P_CCS)) return USB_ERRO;         /* se ha ido durante el reset */
        if (!companheiro_usavel(c)) return USB_ERRO;
        c->op[PORTSC + p] = (v & ~P_MUDANCAS) | P_PO;
        return USB_PORTA_CEDIDA;
    }
    return 0;
}

/* ------------------------------------------------------------ transferencias */

static void preparar_qtd(qtd_t *t, uint32_t pid, uint32_t alterna, uint32_t endereco, uint32_t tam, bool ultimo)
{
    t->proximo = T_TERMINA;
    t->alternativo = T_TERMINA;
    t->ficha = (alterna << 31) | (tam << 16) | (ultimo ? (1u << 15) : 0) | (3u << 10) | (pid << 8) | F_ATIVO;
    t->buffer[0] = endereco;
    uint32_t pagina = endereco & ~0xFFFu;
    for (int i = 1; i < 5; i++) t->buffer[i] = pagina + (uint32_t)i * PAGINA;
    for (int i = 0; i < 5; i++) t->buffer_alto[i] = 0;   /* todo por debajo de 4 GB */
}

/* Saca la QH de trabajo del anillo y espera a que el controlador confirme (el
 * "doorbell" IAA): sólo después de eso deja de leerse su memoria. */
/* D215: lo que va detrás de la cabeza (o de la cola de trabajo) en el anillo:
 * la QH de recepción continua, si la hay, o la propia cabeza. */
static uint32_t cauda(ehci_t *c)
{
    return (c->continua ? c->continua : c->estruturas) | T_QH;
}

static void tirar_fila(ehci_t *c)
{
    cabeca->ligacao = cauda(c);
    c->op[USBSTS] = STS_IAA;
    c->op[USBCMD] |= CMD_IAAD;
    for (int i = 0; i < 20000 && !(c->op[USBSTS] & STS_IAA); i++) porta_pausa();
    c->op[USBSTS] = STS_IAA;
}

/* El bloqueo fue tomado por un proceso que murió en medio de una transferencia
 * (Ctrl+C durante una copia): su QH puede seguir en el anillo. */
static void ehci_recuperar(void)
{
    for (int i = 0; i < qtd_ctls; i++) {
        usar(&ctls[i]);
        tirar_fila(&ctls[i]);
        ctls[i].esperando = false;
        ctls[i].op[USBINTR] = 0;
    }
}

/* IRQ del controlador (línea PCI, posiblemente compartida con su dueño):
 * confirma el aviso, apaga la petición de interrupción hasta la próxima espera y
 * despierta a quien duerme. Si quien estaba corriendo era el proceso ocioso, cede
 * el turno ya, en vez de esperar el próximo tic del reloj (10 ms por transferencia
 * serían 100 transferencias por segundo, como mucho). */
static quadro_t *ao_irq_ehci(quadro_t *q)
{
    bool acordou = false;
    for (int i = 0; i < qtd_ctls; i++) {
        ehci_t *c = &ctls[i];
        if (c->prova && (c->op[USBSTS] & STS_IAA)) {   /* D211: la autoprueba de la IRQ */
            c->op[USBSTS] = STS_IAA;
            c->op[USBINTR] = 0;
            c->provas++;
            continue;
        }
        uint32_t sts = c->op[USBSTS] & (STS_INT | STS_ERRINT);
        if (!sts || !c->esperando) continue;
        c->op[USBSTS] = sts;
        c->op[USBINTR] = 0;
        c->irq_chegou = true;
        c->irqs++;
        acordar(c);
        acordou = true;
    }
    if (acordou && processo_atual() && processo_atual()->pid == 0) return escalonar(q);
    return q;
}

/* D211: la autoprueba de la IRQ. El controlador avisa por interrupción
 * cuando ha avanzado la fila asíncrona tras tocar la "campainha" (IAAD):
 * no hace falta ninguna transferencia ni ningún dispositivo. Con el bloqueo
 * del EHCI tomado: nadie más usa el controlador mientras tanto. */
static bool sonda_provocar(void *ctx)
{
    ehci_t *c = ctx;
    if (!c->ok || !trava_tentar(&trava_ehci)) return false;
    bool i0 = int_salva_desliga();
    c->prova = true;
    c->op[USBSTS] = STS_IAA;
    c->op[USBINTR] = STS_IAA;
    c->op[USBCMD] |= CMD_IAAD;
    int_restaura(i0);
    return true;
}

static uint32_t sonda_contador(void *ctx) { return ((ehci_t *)ctx)->provas; }

static void sonda_terminar(void *ctx, bool chegou)
{
    ehci_t *c = ctx;
    bool i0 = int_salva_desliga();
    c->op[USBINTR] = 0;
    for (int i = 0; i < 20000 && (c->op[USBCMD] & CMD_IAAD); i++) porta_pausa();
    c->op[USBSTS] = STS_IAA;
    c->prova = false;
    if (chegou && c->irq) {
        /* La IRQ llega (quizá ahora, por otro camino): las esperas vuelven a dormir. */
        c->irq_usavel = true;
        c->esperas_sem_irq = 0;
    }
    int_restaura(i0);
    trava_soltar(&trava_ehci);
}

void ehci_modo_assincrono(bool ligado)
{
    if (ligado) {
        for (int i = 0; i < qtd_ctls; i++) {
            ehci_t *c = &ctls[i];
            uint32_t l = pci_le32(&c->pci, 0x3C) & 0xFF;   /* "interrupt line" que la BIOS dejó */
            c->irq = (l >= 1 && l < 16 && l != 2) ? (uint8_t)l : 0;
            c->irq_usavel = c->irq != 0;
            c->esperas_sem_irq = 0;
            if (c->irq) {
                irq_registrar_extra(c->irq, ao_irq_ehci);
                irq_sonda_t s = { "EHCI", c->pci, c->irq, sonda_provocar, sonda_contador, sonda_terminar, c };
                irq_sonda_registrar(&s);
            }
            klog("[EHCI] %d: espera %s\n", i, c->irq ? "por interrupcao (IRQ)" : "cedendo a CPU (sem IRQ)");
        }
    }
    assincrono = ligado;
}

/* ¿Puede esperar durmiendo? Sólo con planificador, fuera de una IRQ (ningún otro
 * proceso correría mientras ella no terminara) y después del arranque. */
static bool pode_dormir(void)
{
    return assincrono && processo_atual() && !contexto_irq();
}

bool ehci_pode_dormir(void) { return pode_dormir(); }

/* Una vuelta de espera: duerme hasta la IRQ del controlador (con el reloj como
 * red de seguridad, como mucho un tic) o, sin IRQ, sólo cede el turno. */
static void esperar_evento(ehci_t *c, bool por_irq)
{
    if (por_irq) {
        bloquear(c);
        processo_atual()->acordar_em = relogio_tiques() + 1;
    }
    ceder();
    processo_atual()->acordar_em = 0;
}

/* Palabras 1 y 2 de una QH (D214): dirección, endpoint, velocidad y tamaño de
 * paquete; los dispositivos de baja o plena velocidad (detrás de un hub de alta)
 * van por "transacciones partidas" (split): la QH lleva la dirección del hub con
 * el "transaction translator" y su puerto, el endpoint 0 se marca como de
 * control y no se usa el contador de NAK. */
static uint32_t qh_caracteristicas(const usb_disp_t *d, uint8_t ep, uint16_t mps, bool periodica)
{
    uint32_t eps = d->velocidade == USB_VEL_BAIXA ? 1u : d->velocidade == USB_VEL_PLENA ? 0u : 2u;
    uint32_t v = (uint32_t)d->endereco | ((uint32_t)ep << 8) | (eps << 12) |
                 (1u << 14) /* la alternancia viene del qTD */ | ((uint32_t)mps << 16);
    if (eps != 2u) { if (ep == 0) v |= 1u << 27; }
    else if (!periodica) v |= 4u << 28;
    return v;
}

static uint32_t qh_capacidades(const usb_disp_t *d, bool periodica)
{
    uint32_t v = 1u << 30;
    bool partida = d->velocidade == USB_VEL_BAIXA || d->velocidade == USB_VEL_PLENA;
    if (partida) v |= ((uint32_t)d->tt_endereco << 16) | ((uint32_t)d->tt_porta << 23);
    if (periodica) v |= partida ? (0x01u | (0x1Cu << 8)) : 0x01u;   /* S-mask (y C-mask si es partida) */
    return v;
}

/* Cuelga la QH en el anillo, espera el último qTD y saca la QH. Devuelve 0 o error. */
static int executar(ehci_t *c, const usb_disp_t *d, uint8_t ep, uint16_t mps, int n_qtd, uint32_t tempo_ms)
{
    for (int i = 0; i < n_qtd - 1; i++) qtds[i].proximo = (uint32_t)&qtds[i + 1];
    memset((void *)fila, 0, sizeof *fila);
    fila->caracteristicas = qh_caracteristicas(d, ep, mps, false);
    fila->capacidades = qh_capacidades(d, false);
    fila->prox_qtd = (uint32_t)&qtds[0];
    fila->alt_qtd = T_TERMINA;
    fila->ligacao = cauda(c);

    /* El pedido de interrupción se arma ANTES de publicar la QH: la transferencia
     * puede terminar enseguida, y el aviso no puede perderse. */
    bool dormir = pode_dormir();
    bool por_irq = dormir && c->irq_usavel;
    bool dormiu = false;
    c->irq_chegou = false;
    if (por_irq) {
        c->op[USBSTS] = STS_INT | STS_ERRINT;      /* limpia lo que sobró de antes */
        c->esperando = true;
        c->op[USBINTR] = STS_INT | STS_ERRINT;
    }
    /* La "campainha" (IAAD) es un aviso: mientras el bit IAA esté ligado, el
     * controlador NO avanza la fila asíncrona y ninguna transferencia empieza
     * (el QEMU, como la especificación, espera a que el programa acuse el
     * recibo). El aviso se confirma un tic después de tocar la campainha, así
     * que puede quedar ligado al empezar la transferencia siguiente: por eso
     * se limpia aquí, antes de publicar la fila, y también mientras se espera
     * (un aviso confirmado en medio congelaría la fila hasta el final). */
    c->op[USBSTS] = STS_IAA;
    cabeca->ligacao = (uint32_t)fila | T_QH;

    int r = 0;
    uint32_t passou = 0, antes = c->op[FRINDEX] & 0x3FFF;
    for (;;) {
        uint32_t f = qtds[n_qtd - 1].ficha;
        bool parou = false;
        for (int i = 0; i < n_qtd; i++)
            if (qtds[i].ficha & F_PAROU) { parou = true; f = qtds[i].ficha; break; }
        if (parou) { r = (f & F_ERROS) ? USB_ERRO : USB_ERRO_PAROU; break; }
        if (!(f & F_ATIVO)) break;
        uint32_t agora = c->op[FRINDEX] & 0x3FFF;
        passou += (agora - antes) & 0x3FFF;
        antes = agora;
        if (passou > tempo_ms * 8) { r = USB_ERRO_TEMPO; break; }
        if (c->op[USBSTS] & STS_HALTED) { r = USB_ERRO; break; }
        c->op[USBSTS] = STS_IAA;    /* la fila asíncrona no puede ficar congelada */
        if (dormir) { esperar_evento(c, por_irq); dormiu = true; }
    }
    if (por_irq) {
        c->op[USBINTR] = 0;
        c->esperando = false;
        /* La IRQ tiene que llegar siempre que la espera durmió y la transferencia
         * terminó: si no llega varias veces seguidas, la línea no está
         * enrutada a este controlador y pasamos a comprobar cediendo el turno. */
        if (c->irq_chegou || !dormiu) c->esperas_sem_irq = 0;
        else if (++c->esperas_sem_irq >= ESPERAS_SEM_IRQ) {
            c->irq_usavel = false;
            klog("[EHCI] a IRQ %u nao chega: esperando cedendo a CPU\n", c->irq);
        }
    }

    tirar_fila(c);
    return r;
}

int ehci_controle(usb_disp_t *d, const void *setup, void *dados, uint16_t tam, bool entrada)
{
    ehci_t *c = d->ctl;
    uint16_t mps0 = d->mps0;
    if (tam > QTD_MAX_BYTES) return USB_ERRO;
    trava_pegar(&trava_ehci);
    bool i0 = int_salva_desliga();
    usar(c);
    memcpy(setup_pacote, setup, 8);
    if (!entrada && tam) memcpy(buffer, dados, tam);
    int n = 0;
    preparar_qtd(&qtds[n++], PID_SETUP, 0, (uint32_t)setup_pacote, 8, false);
    if (tam) preparar_qtd(&qtds[n++], entrada ? PID_ENTRADA : PID_SAIDA, 1, (uint32_t)buffer, tam, false);
    /* Estado: siempre alternancia 1, en la dirección contraria a los datos (o IN sin datos). */
    preparar_qtd(&qtds[n++], (tam && entrada) ? PID_SAIDA : PID_ENTRADA, 1, (uint32_t)buffer, 0, true);
    int r = executar(c, d, 0, mps0 ? mps0 : 64, n, 1000);
    int feito = 0;
    if (r == 0 && tam) {
        feito = (int)tam - (int)((qtds[1].ficha >> 16) & 0x7FFF);
        if (entrada) memcpy(dados, buffer, (uint32_t)feito);
    }
    int_restaura(i0);
    trava_soltar(&trava_ehci);
    return r < 0 ? r : feito;
}

int ehci_bulk(usb_disp_t *d, uint8_t ep, uint16_t mps, bool entrada, uint8_t *alterna, void *dados, uint32_t tam)
{
    ehci_t *c = d->ctl;
    if (tam > BUFFER_BYTES || tam > (uint32_t)MAX_QTD * QTD_MAX_BYTES) return USB_ERRO;
    if (!mps) return USB_ERRO;   /* dispositivo a medio enumerar o ya retirado: sin paquete no hay división */
    trava_pegar(&trava_ehci);
    bool i0 = int_salva_desliga();
    usar(c);
    if (!entrada) memcpy(buffer, dados, tam);
    /* Cada qTD (menos el último) lleva un múltiplo del tamaño del paquete, así que
     * la alternancia de cada uno es la inicial cambiada una vez por paquete. */
    int n = 0;
    uint32_t feito_antes = 0, t = *alterna;
    do {
        uint32_t parte = tam - feito_antes > QTD_MAX_BYTES ? QTD_MAX_BYTES : tam - feito_antes;
        preparar_qtd(&qtds[n], entrada ? PID_ENTRADA : PID_SAIDA, t, (uint32_t)buffer + feito_antes, parte, false);
        t ^= (parte / mps) & 1;
        feito_antes += parte;
        n++;
    } while (feito_antes < tam);
    qtds[n - 1].ficha |= 1u << 15;
    int r = executar(c, d, ep, mps, n, 5000);
    uint32_t feito = 0;
    for (int i = 0; i < n; i++) {
        uint32_t pedido = i < n - 1 ? QTD_MAX_BYTES : tam - (uint32_t)(n - 1) * QTD_MAX_BYTES;
        uint32_t sobrou = (qtds[i].ficha >> 16) & 0x7FFF;
        if (qtds[i].ficha & F_ATIVO) break;
        feito += pedido - sobrou;
        if (sobrou) break;   /* paquete corto: terminó aquí */
    }
    /* Alternancia para la próxima transferencia: cambia con cada paquete enviado. */
    uint32_t pacotes = (feito + mps - 1) / mps;
    if (feito == 0 && r == 0) pacotes = 1;   /* paquete de tamaño cero */
    if (r == 0 || r == USB_ERRO_PAROU) *alterna ^= (uint8_t)(pacotes & 1);
    if (entrada && feito) memcpy(dados, buffer, feito);
    int_restaura(i0);
    trava_soltar(&trava_ehci);
    return r < 0 ? r : (int)feito;
}

/* ------------------------------------------------------------ interrupción (HID, D214) */

/* La lista periódica: 1024 entradas (una por milisegundo) que apuntan todas a
 * una QH vacía y parada, la cabeza; detrás de ella, una QH por endpoint de
 * interrupción abierto, que el controlador visita en cada cuadro (cada 1 ms,
 * en el microcuadro 0; las partidas terminan en los microcuadros 2 a 4). Cada
 * QH tiene un qTD de entrada armado: cuando el dispositivo manda su informe,
 * el qTD queda inactivo y la tarea de HID lo lee y arma el otro. */
#define EHCI_MAX_INT 4

typedef struct {
    bool     usado;
    ehci_t  *c;
    uint32_t pagina;      /* QH (0), dos qTD (128 y 192) y el buffer (256) */
    uint16_t tam;
    uint8_t  alterna, atual;
} ehci_int_t;

static ehci_int_t ints[EHCI_MAX_INT];

static qh_t *periodica_cabeca(ehci_t *c) { return (qh_t *)(c->estruturas + 1024); }

static bool periodica_ligar(ehci_t *c)
{
    if (c->periodica) return true;
    uint32_t f = memfisica_alocar();
    if (!f) return false;
    qh_t *cab = periodica_cabeca(c);
    memset((void *)cab, 0, sizeof *cab);
    cab->ligacao = T_TERMINA;
    cab->prox_qtd = T_TERMINA;
    cab->alt_qtd = T_TERMINA;
    cab->ficha = F_PAROU;
    volatile uint32_t *lista = (volatile uint32_t *)f;
    for (int i = 0; i < 1024; i++) lista[i] = (uint32_t)cab | T_QH;
    c->op[PERIODIC] = f;
    c->op[USBCMD] |= CMD_PSE;
    for (int i = 0; i < 100 && !(c->op[USBSTS] & STS_PSS); i++) esperar_bruto_us(100);
    c->periodica = f;
    klog("[EHCI] lista periodica encendida (%s)\n", (c->op[USBSTS] & STS_PSS) ? "ok" : "NO se ha encendido");
    return true;
}

static void int_armar(ehci_int_t *e)
{
    qh_t *q = (qh_t *)e->pagina;
    e->atual ^= 1;
    qtd_t *t = (qtd_t *)(e->pagina + 128 + 64u * e->atual);
    preparar_qtd(t, PID_ENTRADA, e->alterna, e->pagina + 256, e->tam, false);
    q->ficha = 0;                  /* ni activa ni parada: el controlador sigue al próximo qTD */
    q->alt_qtd = T_TERMINA;
    q->prox_qtd = (uint32_t)t;
}

int ehci_int_abrir(usb_disp_t *d, uint8_t ep, uint16_t mps)
{
    ehci_t *c = d->ctl;
    if (!c || !mps) return USB_ERRO;
    bool i0 = int_salva_desliga();
    int h = -1;
    for (int i = 0; i < EHCI_MAX_INT; i++) if (!ints[i].usado) { h = i; break; }
    uint32_t pg = h >= 0 ? memfisica_alocar() : 0;
    if (h < 0 || !pg || !periodica_ligar(c)) {
        if (pg) memfisica_liberar(pg);
        int_restaura(i0);
        return USB_ERRO;
    }
    memset((void *)pg, 0, PAGINA);
    ehci_int_t *e = &ints[h];
    e->usado = true;
    e->c = c;
    e->pagina = pg;
    e->tam = mps > 1024 ? 1024 : mps;
    e->alterna = 0;
    e->atual = 1;
    qh_t *q = (qh_t *)pg;
    q->caracteristicas = qh_caracteristicas(d, ep, mps, true);
    q->capacidades = qh_capacidades(d, true);
    q->prox_qtd = T_TERMINA;
    int_armar(e);
    qh_t *cab = periodica_cabeca(c);
    q->ligacao = cab->ligacao;
    cab->ligacao = (uint32_t)q | T_QH;
    int_restaura(i0);
    return h;
}

int ehci_int_ler(int h, uint8_t *dados, int max)
{
    if (h < 0 || h >= EHCI_MAX_INT || !ints[h].usado) return USB_ERRO;
    ehci_int_t *e = &ints[h];
    bool i0 = int_salva_desliga();
    qtd_t *t = (qtd_t *)(e->pagina + 128 + 64u * e->atual);
    uint32_t f = t->ficha;
    int r = 0;
    if (f & F_ATIVO) r = 0;
    else if (f & F_PAROU) {
        r = (f & F_ERROS) ? USB_ERRO : USB_ERRO_PAROU;
        e->alterna = 0;
        int_armar(e);
    } else {
        int n = (int)e->tam - (int)((f >> 16) & 0x7FFF);
        if (n < 0) n = 0;
        if (n > max) n = max;
        memcpy(dados, (const void *)(e->pagina + 256), (uint32_t)n);
        e->alterna ^= 1;            /* un paquete por transferencia (tam <= paquete) */
        int_armar(e);
        r = n;
    }
    int_restaura(i0);
    return r;
}

void ehci_int_fechar(int h)
{
    if (h < 0 || h >= EHCI_MAX_INT || !ints[h].usado) return;
    ehci_int_t *e = &ints[h];
    bool i0 = int_salva_desliga();
    qh_t *q = (qh_t *)e->pagina;
    qh_t *p = periodica_cabeca(e->c);
    while ((p->ligacao & ~0x1Fu) && (p->ligacao & ~0x1Fu) != (uint32_t)q) p = (qh_t *)(p->ligacao & ~0x1Fu);
    if ((p->ligacao & ~0x1Fu) == (uint32_t)q) p->ligacao = q->ligacao;
    ehci_esperar_ms(e->c, 3);       /* el controlador ya no la está leyendo */
    memfisica_liberar(e->pagina);
    e->usado = false;
    int_restaura(i0);
}

/* ------------------------------------------------------------ recepción bulk continua (Wi-Fi, D215) */

/* Un endpoint bulk de entrada que tiene que estar SIEMPRE escuchando (el
 * pincho Wi-Fi manda las tramas cuando llegan, no cuando se le piden). Con
 * ehci_bulk habría que tener el bloqueo de las transferencias mientras se
 * espera, y los pendrives se quedarían parados. Aquí la QH de ese endpoint
 * vive fija en el anillo asíncrono, detrás de la cabeza (y de la cola de
 * trabajo cuando hay una transferencia en curso): cabeza -> [trabajo ->]
 * continua -> cabeza. Lleva un qTD de entrada armado sobre un buffer propio;
 * cuando el dispositivo manda algo, el qTD queda inactivo y ehci_rx_ler lo
 * copia y arma el otro (dos qTD que se alternan, como en la interrupción).
 * Una por controlador. */
typedef struct {
    bool     usado;
    ehci_t  *c;
    uint32_t pagina;      /* QH (0) y dos qTD (128 y 192); con cola, n qTD desde 128 */
    uint32_t buffer;      /* físico, alineado a página, hasta 16 KB (con cola, n seguidos) */
    uint32_t tam;
    uint8_t  alterna, atual;
    /* D238: cola de n qTD encadenados en círculo (0 = la manera de siempre) */
    uint8_t  n, prox;
    uint32_t completadas, erros;
} ehci_rx_t;

static ehci_rx_t rxs[USB_MAX_CONTROLADORES];

static void rx_armar(ehci_rx_t *e)
{
    qh_t *q = (qh_t *)e->pagina;
    e->atual ^= 1;
    qtd_t *t = (qtd_t *)(e->pagina + 128 + 64u * e->atual);
    preparar_qtd(t, PID_ENTRADA, e->alterna, e->buffer, e->tam, false);
    q->alt_qtd = T_TERMINA;
    q->prox_qtd = (uint32_t)t;
    q->ficha = 0;                  /* ni activa ni parada: el controlador sigue al próximo qTD */
}

int ehci_rx_abrir(usb_disp_t *d, uint8_t ep, uint16_t mps, uint32_t buffer_fis, uint32_t tam)
{
    ehci_t *c = d->ctl;
    if (!c || !mps || !buffer_fis || (buffer_fis & 0xFFFu) || tam > QTD_MAX_BYTES) return USB_ERRO;
    int h = -1;
    for (int i = 0; i < USB_MAX_CONTROLADORES; i++) {
        if (rxs[i].usado && rxs[i].c == c) return USB_ERRO;   /* una por controlador */
        if (!rxs[i].usado && h < 0) h = i;
    }
    uint32_t pg = h >= 0 ? memfisica_alocar() : 0;
    if (!pg) return USB_ERRO;
    memset((void *)pg, 0, PAGINA);
    trava_pegar(&trava_ehci);
    bool i0 = int_salva_desliga();
    usar(c);
    ehci_rx_t *e = &rxs[h];
    e->usado = true;
    e->c = c;
    e->pagina = pg;
    e->buffer = buffer_fis;
    e->tam = tam;
    e->alterna = 0;
    e->atual = 1;
    qh_t *q = (qh_t *)pg;
    q->caracteristicas = qh_caracteristicas(d, ep, mps, false);
    q->capacidades = qh_capacidades(d, false);
    q->prox_qtd = T_TERMINA;
    rx_armar(e);
    q->ligacao = c->estruturas | T_QH;
    c->continua = pg;
    cabeca->ligacao = pg | T_QH;   /* con el bloqueo no hay cola de trabajo colgada */
    int_restaura(i0);
    trava_soltar(&trava_ehci);
    return h;
}

/* D238: la recepción continua con una COLA de n qTD (hasta EHCI_RX_FILA_MAX),
 * cada uno con su buffer, encadenados en círculo: mientras el sistema copia lo
 * que llegó en uno, el controlador ya está llenando el siguiente (con un solo
 * qTD, el pincho esperaba a la próxima consulta). Cuando el controlador
 * llega a un qTD que todavía no se ha leído (inactivo), se queda en él y lo
 * vuelve a mirar en cada pasada por el anillo (EHCI 1.0, 4.10.2): en cuanto se
 * rearma, sigue. Un paquete corto termina el qTD y, con el puntero alternativo
 * vacío, el controlador pasa al siguiente.
 *
 * La alternancia de datos: con varios qTD armados de antemano no se puede saber
 * la de cada uno, así que esta QH la guarda ella misma (DTC = 0 en la palabra 1:
 * el controlador ignora la del qTD y conserva la suya). */
#define EHCI_RX_FILA_MAX 8

static qtd_t *rx_fila_qtd(ehci_rx_t *e, uint32_t i) { return (qtd_t *)(e->pagina + 128 + 64u * i); }

/* Arma el qTD i: todos los campos y, al final, la ficha con el bit de activo. */
static void rx_fila_armar(ehci_rx_t *e, uint32_t i)
{
    qtd_t *t = rx_fila_qtd(e, i);
    uint32_t b = e->buffer + i * e->tam;
    t->proximo = (uint32_t)rx_fila_qtd(e, (i + 1) % e->n);
    t->alternativo = T_TERMINA;
    t->buffer[0] = b;
    for (int k = 1; k < 5; k++) t->buffer[k] = (b & ~0xFFFu) + (uint32_t)k * PAGINA;
    for (int k = 0; k < 5; k++) t->buffer_alto[k] = 0;
    __asm__ volatile("" ::: "memory");
    t->ficha = (e->tam << 16) | (3u << 10) | (PID_ENTRADA << 8) | F_ATIVO;
}

int ehci_rx_abrir_fila(usb_disp_t *d, uint8_t ep, uint16_t mps, uint32_t buffer_fis, uint32_t tam, int n)
{
    ehci_t *c = d->ctl;
    if (!c || !mps || !buffer_fis || (buffer_fis & 0xFFFu) || (tam & 0xFFFu) || !tam || tam > QTD_MAX_BYTES ||
        n < 1 || n > EHCI_RX_FILA_MAX) return USB_ERRO;
    int h = -1;
    for (int i = 0; i < USB_MAX_CONTROLADORES; i++) {
        if (rxs[i].usado && rxs[i].c == c) return USB_ERRO;   /* una por controlador */
        if (!rxs[i].usado && h < 0) h = i;
    }
    uint32_t pg = h >= 0 ? memfisica_alocar() : 0;
    if (!pg) return USB_ERRO;
    memset((void *)pg, 0, PAGINA);
    trava_pegar(&trava_ehci);
    bool i0 = int_salva_desliga();
    usar(c);
    ehci_rx_t *e = &rxs[h];
    memset(e, 0, sizeof *e);
    e->usado = true;
    e->c = c;
    e->pagina = pg;
    e->buffer = buffer_fis;
    e->tam = tam;
    e->n = (uint8_t)n;
    for (int i = 0; i < n; i++) rx_fila_armar(e, (uint32_t)i);
    qh_t *q = (qh_t *)pg;
    q->caracteristicas = qh_caracteristicas(d, ep, mps, false) & ~(1u << 14);   /* DTC = 0: la alternancia, en la QH */
    q->capacidades = qh_capacidades(d, false);
    q->alt_qtd = T_TERMINA;
    q->prox_qtd = (uint32_t)rx_fila_qtd(e, 0);
    q->ficha = 0;                  /* ni activa ni parada, alternancia 0 */
    q->ligacao = c->estruturas | T_QH;
    c->continua = pg;
    cabeca->ligacao = pg | T_QH;   /* con el bloqueo no hay cola de trabajo colgada */
    int_restaura(i0);
    trava_soltar(&trava_ehci);
    return h;
}

static int rx_fila_ler(ehci_rx_t *e, uint8_t *dados, int max)
{
    qtd_t *t = rx_fila_qtd(e, e->prox);
    uint32_t f = t->ficha;
    if (f & F_ATIVO) return 0;
    if (f & F_PAROU) {
        /* La QH se paró en este qTD: se rearma la cola entera desde aquí y se
         * suelta la QH (con la alternancia a 0, como la lectura de siempre). */
        e->erros++;
        for (uint32_t i = 0; i < e->n; i++) rx_fila_armar(e, i);
        qh_t *q = (qh_t *)e->pagina;
        q->alt_qtd = T_TERMINA;
        q->prox_qtd = (uint32_t)t;
        q->ficha = 0;
        return (f & F_ERROS) ? USB_ERRO : USB_ERRO_PAROU;
    }
    int n = (int)e->tam - (int)((f >> 16) & 0x7FFF);
    if (n < 0) n = 0;
    if (n > max) n = max;
    if (n > 0) memcpy(dados, (const void *)(e->buffer + e->prox * e->tam), (uint32_t)n);
    e->completadas++;
    rx_fila_armar(e, e->prox);
    e->prox = (uint8_t)((e->prox + 1) % e->n);
    return n;
}

/* D238: en vuelo (armados) y terminados bien. */
bool ehci_rx_info(int h, uint32_t *emvoo, uint32_t *completadas, uint32_t *erros)
{
    if (h < 0 || h >= USB_MAX_CONTROLADORES || !rxs[h].usado || !rxs[h].n) return false;
    ehci_rx_t *e = &rxs[h];
    uint32_t v = 0;
    for (uint32_t i = 0; i < e->n; i++) if (rx_fila_qtd(e, i)->ficha & F_ATIVO) v++;
    *emvoo = v;
    *completadas = e->completadas;
    *erros = e->erros;
    return true;
}

/* >0 bytes recibidos, 0 nada todavía, <0 error (STALL o de transmisión: se rearma). */
int ehci_rx_ler(int h, uint8_t *dados, int max)
{
    if (h < 0 || h >= USB_MAX_CONTROLADORES || !rxs[h].usado) return USB_ERRO;
    ehci_rx_t *e = &rxs[h];
    bool i0 = int_salva_desliga();
    if (e->n) {
        int r = rx_fila_ler(e, dados, max);
        int_restaura(i0);
        return r;
    }
    qtd_t *t = (qtd_t *)(e->pagina + 128 + 64u * e->atual);
    uint32_t f = t->ficha;
    int r;
    if (f & F_ATIVO) r = 0;
    else if (f & F_PAROU) {
        r = (f & F_ERROS) ? USB_ERRO : USB_ERRO_PAROU;
        e->alterna = 0;
        rx_armar(e);
    } else {
        int n = (int)e->tam - (int)((f >> 16) & 0x7FFF);
        if (n < 0) n = 0;
        if (n > max) n = max;
        memcpy(dados, (const void *)e->buffer, (uint32_t)n);
        /* Con la alternancia en el qTD, el controlador la devuelve ya cambiada
         * por cada paquete recibido: es la del próximo. */
        e->alterna = (uint8_t)(f >> 31);
        rx_armar(e);
        r = n;
    }
    int_restaura(i0);
    return r;
}

void ehci_rx_fechar(int h)
{
    if (h < 0 || h >= USB_MAX_CONTROLADORES || !rxs[h].usado) return;
    ehci_rx_t *e = &rxs[h];
    ehci_t *c = e->c;
    trava_pegar(&trava_ehci);
    bool i0 = int_salva_desliga();
    usar(c);
    c->continua = 0;
    tirar_fila(c);                 /* la cabeza vuelve a apuntarse a sí misma, con el aviso IAA */
    memfisica_liberar(e->pagina);
    e->usado = false;
    int_restaura(i0);
    trava_soltar(&trava_ehci);
}
