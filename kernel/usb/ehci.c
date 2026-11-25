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
 *   - los datos pasan por un buffer de 1 MB (128 KB con poca memoria, D468) en memoria
 *     física contigua del kernel (el controlador necesita direcciones físicas; en SaguiOS
 *     el kernel ve la memoria física 1:1, D008). D468: si quien llama da un búfer del
 *     propio kernel (memoria baja, 1:1 y por tanto contigua), el DMA va directo a él, sin
 *     copia; los trozos pequeños (CBW, CSW) van por una página aparte;
 *   - D468: varias transferencias seguidas del mismo endpoint (CBW + datos, datos + CSW)
 *     se entregan en una sola cola de qTD, sin esperar a la IRQ entre una y otra.
 * Antes de todo, el sistema pide el controlador a la BIOS (que lo usaba para el
 * pendrive de arranque), por el procedimiento "BIOS/OS handoff" de la especificación. */
#include "usb.h"
#include "isoent.h"
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
#include "prazo.h"

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
/* D468: transferencias bulk largas. Buffer de 1 MB si hay al menos 64 MB de memoria baja (si no,
 * o si no hay 1 MB contiguo, el de 128 KB de siempre); 128 qTD por controlador en dos páginas
 * propias (20 KB por qTD con el buffer alineado a página, 16 KB si no: 1 MB son 52 o 64 qTD). El
 * último qTD de la cola es el de «parada»: inactivo, adonde salta una entrada que viene corta. */
#define BULK_MAX      (1024u * 1024u)
#define QTDS_BULK_PAGS 2u
#define QTDS_BULK_N   (QTDS_BULK_PAGS * PAGINA / 64u)
#define PEQ_MAX       512u        /* las partes de hasta 512 bytes van por la página "pequeno" */
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
static uint32_t buffer_bytes;      /* D468: 1 MB o 128 KB */
static uint8_t *pequeno;           /* D468: una página para las partes pequeñas (512 bytes por parte) */
static bool qtds_grandes = true;   /* D468: todos los controladores tienen su cola de 128 qTD */

static void usar(ehci_t *c)
{
    uint32_t p = c->estruturas;
    cabeca = (qh_t *)p;
    fila = (qh_t *)(p + 128);
    setup_pacote = (uint8_t *)(p + 256);
    qtds = (qtd_t *)(p + 512);
}

int ehci_quantidade(void) { return qtd_ctls; }

/* D450: al apagar o reiniciar (interrupciones cerradas): sin interrupciones, Run/Stop = 0 y el
 * HCHalted como mucho 20 ms por controlador (por el TSC). CONFIGFLAG no se toca: el firmware lo
 * pone a su gusto al arrancar. */
int ehci_parar_todos(void)
{
    int paradas = 0;
    for (int i = 0; i < qtd_ctls; i++) {
        ehci_t *c = &ctls[i];
        if (!c->op) continue;
        c->op[USBINTR] = 0;
        c->op[USBCMD] &= ~CMD_RS;
        prazo_t pz = prazo_ms(20);
        while (!(c->op[USBSTS] & STS_HALTED) && !prazo_vencido(&pz)) ;
        if (c->op[USBSTS] & STS_HALTED) paradas++;
        else klog("[EHCI] %d: no se ha parado en 20 ms (USBSTS %x): se sigue\n", i, c->op[USBSTS]);
        c->ok = false;
    }
    return paradas;
}
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
    /* La página pequeña antes: "buffer" solo queda puesto si está todo. */
    if (!pequeno && !(pequeno = (uint8_t *)memfisica_alocar())) return false;
    uint32_t b = 0;
    if (memfisica_geridas() >= 16384u) {             /* D468: 64 MB de memoria baja o más */
        b = memfisica_alocar_seguidas(BULK_MAX / PAGINA);
        if (b) buffer_bytes = BULK_MAX;
    }
    if (!b) {
        b = memfisica_alocar_seguidas(BUFFER_BYTES / PAGINA);
        if (!b) return false;
        buffer_bytes = BUFFER_BYTES;
    }
    buffer = (uint8_t *)b;
    klog("[EHCI] buffer de datos de %u KB\n", buffer_bytes / 1024);
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
    /* D468: la cola larga de qTD; sin ella, el controlador se queda con las 8 de siempre. */
    c->qtds_bulk = memfisica_alocar_seguidas(QTDS_BULK_PAGS);
    if (c->qtds_bulk) memset((void *)c->qtds_bulk, 0, QTDS_BULK_PAGS * PAGINA);
    else qtds_grandes = false;
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

/* Cuelga la QH en el anillo, espera el último qTD y saca la QH. Devuelve 0 o error.
 * D468: "q" es la cola (la de control o la larga de bulk); con "curto_termina", un qTD que
 * acaba con bytes por recibir (entrada corta: el controlador ha saltado al de parada) termina
 * la espera. */
static int executar(ehci_t *c, const usb_disp_t *d, uint8_t ep, uint16_t mps, qtd_t *q, int n_qtd, uint32_t tempo_ms,
                    bool curto_termina)
{
    for (int i = 0; i < n_qtd - 1; i++) q[i].proximo = (uint32_t)&q[i + 1];
    memset((void *)fila, 0, sizeof *fila);
    fila->caracteristicas = qh_caracteristicas(d, ep, mps, false);
    fila->capacidades = qh_capacidades(d, false);
    fila->prox_qtd = (uint32_t)&q[0];
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
        uint32_t f = q[n_qtd - 1].ficha;
        bool parou = false, curto = false;
        for (int i = 0; i < n_qtd; i++) {
            uint32_t fi = q[i].ficha;
            if (fi & F_PAROU) { parou = true; f = fi; break; }
            if (curto_termina && i < n_qtd - 1 && !(fi & F_ATIVO) && ((fi >> 16) & 0x7FFF)) { curto = true; break; }
        }
        if (parou) { r = (f & F_ERROS) ? USB_ERRO : USB_ERRO_PAROU; break; }
        if (curto || !(f & F_ATIVO)) break;
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
    int r = executar(c, d, 0, mps0 ? mps0 : 64, qtds, n, 1000, false);
    int feito = 0;
    if (r == 0 && tam) {
        feito = (int)tam - (int)((qtds[1].ficha >> 16) & 0x7FFF);
        if (entrada) memcpy(dados, buffer, (uint32_t)feito);
    }
    int_restaura(i0);
    trava_soltar(&trava_ehci);
    return r < 0 ? r : feito;
}

/* D468: ¿puede ir el DMA directo al búfer de quien llama? Solo si es memoria baja del kernel
 * (vista 1:1, D008: la dirección es la física y las páginas seguidas son contiguas), fuera del
 * primer MB y con al menos una página de datos (lo pequeño, mejor por la página "pequeno"). La
 * memoria de un programa (desde USUARIO_INICIO) puede estar en la alta o en la swap: esa, por
 * el buffer. */
static bool dma_direto(const void *p, uint32_t tam)
{
    uint32_t a = (uint32_t)p;
    return tam >= PAGINA && a >= 0x100000u && a + tam > a && a + tam <= MEMFIS_LIMITE;
}

uint32_t ehci_bulk_max(void)
{
    if (!buffer) return BUFFER_BYTES;
    /* Sin la cola larga quedan las 8 de control: 4 de 16 KB de datos + CBW o CSW + la parada. */
    if (!qtds_grandes) return 65536u;
    return buffer_bytes;
}

/* Bytes de un qTD que empieza en "endereco": lo que cabe en sus 5 páginas, en múltiplos de 4 KB
 * (así cada qTD menos el último lleva un número entero de paquetes, sea cual sea el mps). */
static uint32_t qtd_cabe(uint32_t endereco)
{
    return (20480u - (endereco & 0xFFFu)) & ~0xFFFu;
}

int ehci_bulk_cadeia(usb_disp_t *d, uint8_t ep, uint16_t mps, bool entrada, uint8_t *alterna, usb_parte_t *p, int n)
{
    ehci_t *c = d->ctl;
    if (n < 1 || n > USB_CADEIA_MAX) return USB_ERRO;
    for (int k = 0; k < n; k++) p[k].r = USB_NAO_FEITA;
    if (!mps) return USB_ERRO;   /* dispositivo a medio enumerar o ya retirado: sin paquete no hay división */
    int grandes = 0;
    uint32_t total = 0;
    for (int k = 0; k < n; k++) {
        if (p[k].tam > PEQ_MAX) grandes++;
        if (p[k].tam > ehci_bulk_max()) return USB_ERRO;
        total += p[k].tam;
    }
    if (grandes > 1) return USB_ERRO;   /* un solo buffer grande */
    trava_pegar(&trava_ehci);
    bool i0 = int_salva_desliga();
    usar(c);
    qtd_t *q = c->qtds_bulk ? (qtd_t *)c->qtds_bulk : qtds;
    int qmax = (c->qtds_bulk ? (int)QTDS_BULK_N : MAX_QTD) - 1;   /* el último, la parada */
    qtd_t *parada = &q[qmax];
    parada->proximo = T_TERMINA;
    parada->alternativo = T_TERMINA;
    parada->ficha = 0;                                  /* inactivo: la cola se queda quieta */
    /* Dónde va cada parte: la página pequeña, el búfer de quien llama o el buffer. */
    uint32_t fis[USB_CADEIA_MAX];
    int ini[USB_CADEIA_MAX], ult[USB_CADEIA_MAX];
    for (int k = 0; k < n; k++) {
        if (p[k].tam <= PEQ_MAX) fis[k] = (uint32_t)pequeno + (uint32_t)k * PEQ_MAX;
        else if (dma_direto(p[k].dados, p[k].tam)) fis[k] = (uint32_t)p[k].dados;
        else fis[k] = (uint32_t)buffer;
        if (!entrada && p[k].tam && fis[k] != (uint32_t)p[k].dados) memcpy((void *)fis[k], p[k].dados, p[k].tam);
    }
    /* Los qTD de todas las partes, seguidos. Cada qTD menos el último de una parte lleva un
     * número entero de paquetes; la alternancia de cada uno es la que deja el anterior. En la
     * entrada, un paquete corto salta a la parada (el resto no se hace: quien llama lo repite). */
    int nq = 0;
    uint32_t t = *alterna;
    bool cabe = true;
    for (int k = 0; k < n && cabe; k++) {
        uint32_t off = 0;
        ini[k] = nq;
        do {
            if (nq >= qmax) { cabe = false; break; }
            uint32_t end = fis[k] + off, parte = p[k].tam - off, max = qtd_cabe(end);
            if (parte > max) parte = max;
            preparar_qtd(&q[nq], entrada ? PID_ENTRADA : PID_SAIDA, t, end, parte, false);
            if (entrada) q[nq].alternativo = (uint32_t)parada;
            q[nq].preenchimento[0] = parte;             /* el controlador no lee aquí: lo pedido */
            t ^= (parte ? (parte + mps - 1) / mps : 1u) & 1;
            off += parte;
            nq++;
        } while (off < p[k].tam);
        ult[k] = nq - 1;
    }
    int r = USB_ERRO;
    if (cabe) {
        q[nq - 1].ficha |= 1u << 15;                    /* IOC en el último */
        r = executar(c, d, ep, mps, q, nq, 5000u + total / 256u, entrada);
        /* Resultado de cada parte, en orden, hasta la primera que no terminó entera. */
        uint32_t pacotes = 0;
        bool parar = false;
        for (int k = 0; k < n && !parar; k++) {
            uint32_t feito = 0;
            int rk = 0;
            bool inteira = true;
            for (int i = ini[k]; i <= ult[k]; i++) {
                uint32_t f = q[i].ficha, pedido = q[i].preenchimento[0], sobrou = (f >> 16) & 0x7FFF;
                if (f & F_PAROU) { rk = (f & F_ERROS) ? USB_ERRO : USB_ERRO_PAROU; feito += pedido - sobrou; break; }
                if (f & F_ATIVO) { rk = r < 0 ? r : USB_ERRO_TEMPO; inteira = false; break; }   /* no acabó: la espera falló */
                feito += pedido - sobrou;
                if (sobrou) { inteira = false; break; }   /* corto: terminó aquí */
            }
            /* La alternancia cambia con cada paquete hecho (como antes: también si el endpoint se paró). */
            if (rk == 0 || rk == USB_ERRO_PAROU) pacotes += feito ? (feito + mps - 1) / mps : (rk == 0 ? 1u : 0u);
            p[k].r = rk < 0 ? rk : (int)feito;
            if (rk < 0 || !inteira) parar = true;
            if (entrada && rk == 0 && feito && fis[k] != (uint32_t)p[k].dados) memcpy(p[k].dados, (void *)fis[k], feito);
        }
        *alterna ^= (uint8_t)(pacotes & 1);
        for (int k = 0; k < n; k++)
            if (p[k].r < 0 && p[k].r != USB_ERRO_PAROU && p[k].r != USB_NAO_FEITA) {
                klog("[EHCI] cadena de %d partes (%u bytes, %d qTD): la %d (%u bytes en %x) error %d; qTD %x ... %x\n", n,
                     total, nq, k, p[k].tam, fis[k], p[k].r, q[ini[k]].ficha, q[ult[k]].ficha);
                break;
            }
        r = 0;
    }
    int_restaura(i0);
    trava_soltar(&trava_ehci);
    return r;
}

int ehci_bulk(usb_disp_t *d, uint8_t ep, uint16_t mps, bool entrada, uint8_t *alterna, void *dados, uint32_t tam)
{
    usb_parte_t p = { dados, tam, USB_NAO_FEITA };
    int r = ehci_bulk_cadeia(d, ep, mps, entrada, alterna, &p, 1);
    return r < 0 ? r : p.r;
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


/* ------------------------------------------------------------ transferencias isócronas (D275) */

/* iTD (apartado 3.3 de la especificación EHCI): 8 transacciones, una por microcuadro,
 * y 7 punteros de página. Con el formato de 64 bits siguen 7 palabras con la parte alta
 * (en cero). Ocupa 128 bytes, alineada en 32. */
typedef struct {
    volatile uint32_t proximo;
    volatile uint32_t trans[8];
    volatile uint32_t pagina[7];
    volatile uint32_t pagina_alta[7];
    uint32_t preenchimento[9];
} __attribute__((aligned(32))) itd_t;
_Static_assert(sizeof(itd_t) == 128, "iTD: 128 bytes");

#define ITD_ATIVO   (1u << 31)
#define ITD_ERROS   (7u << 28)       /* búfer, "babble", transacción */
#define ISO_N       64               /* iTD del anillo: un milisegundo cada uno (potencia de 2) */
#define ISO_ESTRUTURAS 8192u         /* 64 iTD de 128 bytes */

typedef struct {
    bool     usado, ligado;
    ehci_t  *c;
    uint32_t itds;                 /* físico de los 64 iTD */
    uint8_t  dev, ep, ivl_uf;      /* dirección, endpoint, microcuadros entre transacciones (1, 2, 4 u 8) */
    uint16_t mps;
    uint32_t anel_fis, anel_tam, bytes_s;
    uint8_t  tam_quadro;
    uint32_t pos_fila, pos_feito, acc;
    uint32_t agora, ultimo_f;      /* cuenta absoluta de cuadros (1 ms) y el último FRINDEX>>3 visto */
    uint32_t prox, vel;            /* cuadro del próximo iTD a armar y del más antiguo sin completar */
    uint16_t bytes[ISO_N];         /* bytes planeados de cada iTD */
    uint32_t ancho;                /* bytes por microcuadro que reserva */
    bool     entrada;              /* D277: flujo de entrada (cámara) */
    bool     sitd;                 /* D277: plena velocidad tras un hub de alta velocidad: siTD en vez de iTD */
    uint8_t  mult;                 /* transacciones por microcuadro (iTD) */
    uint8_t  tt_hub, tt_porta;     /* siTD: hub con el traductor y su puerto */
    uint8_t  arm[ISO_N];           /* entrada: transacciones puestas en el iTD (0 = ninguna) */
    isoent_t re;
    uint32_t paquetes, completados, subdesb, erros;
} ehci_iso_t;

static ehci_iso_t isoc[USB_ISO_MAX * USB_MAX_CONTROLADORES];

static itd_t *itd_de(ehci_iso_t *e, uint32_t k) { return (itd_t *)(e->itds + 128u * k); }

/* siTD (apartado 3.4 de la especificación EHCI): una transacción isócrona de plena
 * velocidad que el EHCI reparte por el traductor (TT) de un hub de alta velocidad con
 * "start-split" (S) y "complete-split" (C) en microcuadros concretos del cuadro. Ocupa
 * 28 bytes (36 con 64 bits); se alinean en 64 y se guardan 64 en la página. */
typedef struct {
    volatile uint32_t proximo;
    volatile uint32_t carac;        /* dirección, endpoint, hub, puerto y sentido (bit 31 = entrada) */
    volatile uint32_t mascaras;     /* S-mask (7:0) y C-mask (15:8) */
    volatile uint32_t estado;       /* estado (7:0), C-prog (15:8), bytes (25:16), selector de página (30), IOC (31) */
    volatile uint32_t pagina0;      /* página 0 y desplazamiento actual */
    volatile uint32_t pagina1;      /* página 1, TP (4:3) y T-count (2:0) */
    volatile uint32_t atras;        /* puntero de retroceso */
    volatile uint32_t pagina0_alta, pagina1_alta;
    uint32_t preenchimento[7];
} __attribute__((aligned(64))) sitd_t;
_Static_assert(sizeof(sitd_t) == 64, "siTD: 64 bytes");
#define SITD_ATIVO   0x80u
#define SITD_ERROS   0x7Cu            /* ERR, búfer, babble, transacción y microcuadro perdido */
#define SITD_TIPO    4u               /* tipo de enlace 2 = siTD */
#define SITD_SPLIT_MAX 188u           /* bytes por start-split de salida */
#define SITD_ENTRADA_MAX 752u         /* con S en el microcuadro 0, los C (2 a hs+3) caben en el cuadro */

static sitd_t *sitd_de(ehci_iso_t *e, uint32_t k) { return (sitd_t *)(e->itds + 64u * k); }

/* Actualiza la cuenta absoluta de cuadros con FRINDEX (11 bits de cuadro). */
static void iso_relogio(ehci_iso_t *e)
{
    uint32_t f = (e->c->op[FRINDEX] >> 3) & 0x7FF;
    e->agora += (f - e->ultimo_f) & 0x7FF;
    e->ultimo_f = f;
}

/* Cuenta lo que ya pasó: un iTD está completo cuando su cuadro terminó. */
/* Entrada: pasa a la cola de recibidos lo que dejó el iTD o el siTD k (no lo que se saltó). */
static void iso_recibir(ehci_iso_t *e, uint32_t k, bool perdido)
{
    if (e->sitd) {
        sitd_t *t = sitd_de(e, k);
        if (!e->arm[k]) return;
        uint32_t st = t->estado;
        bool mal = perdido || (st & SITD_ATIVO) || (st & SITD_ERROS);
        uint32_t resto = (st >> 16) & 0x3FF;
        isoent_completar(&e->re, mal || resto > e->mps ? 0 : e->mps - resto, mal);
        t->estado = 0;
        e->arm[k] = 0;
        return;
    }
    itd_t *t = itd_de(e, k);
    uint32_t cnt = e->arm[k];
    for (uint32_t i = 0; i < 8 && cnt; i++) {
        if (i % e->ivl_uf) continue;
        uint32_t tr = t->trans[i];
        bool mal = perdido || (tr & ITD_ATIVO) || (tr & ITD_ERROS);
        isoent_completar(&e->re, mal ? 0 : (tr >> 16) & 0xFFFu, mal);
    }
    e->arm[k] = 0;
}

static void iso_completar(ehci_iso_t *e)
{
    while (e->vel != e->prox && (int32_t)(e->agora - e->vel) > 0) {
        uint32_t k = e->vel % ISO_N;
        if (e->entrada) {
            bool ativo = e->arm[k] && (e->sitd ? (sitd_de(e, k)->estado & SITD_ATIVO) != 0 : (itd_de(e, k)->trans[0] & ITD_ATIVO) != 0);
            if (ativo) e->subdesb++;
            iso_recibir(e, k, false);
            if (e->sitd) sitd_de(e, k)->estado = 0;
            else for (int i = 0; i < 8; i++) itd_de(e, k)->trans[i] = 0;
            e->completados++;
            e->vel++;
            continue;
        }
        if (e->sitd) {                       /* salida por siTD: un paquete por cuadro */
            sitd_t *t = sitd_de(e, k);
            if (t->estado & SITD_ATIVO) e->subdesb++;
            else if (t->estado & SITD_ERROS) e->erros++;
            t->estado = 0;
            e->pos_feito += e->bytes[k];
            e->bytes[k] = 0;
            e->completados++;
            e->vel++;
            continue;
        }
        itd_t *t = itd_de(e, k);
        bool ativo = false, erro = false;
        for (int i = 0; i < 8; i++) { ativo |= (t->trans[i] & ITD_ATIVO) != 0; erro |= (t->trans[i] & ITD_ERROS) != 0; }
        if (ativo) e->subdesb++;             /* el controlador no llegó a ejecutarlo */
        if (erro) e->erros++;
        for (int i = 0; i < 8; i++) t->trans[i] = 0;   /* inactivo: no se repite 64 ms después */
        e->pos_feito += e->bytes[k];
        e->bytes[k] = 0;
        e->completados++;
        e->vel++;
    }
}

/* Arma el siTD del cuadro f (D277). Salida: un paquete de bytes_s/1000 bytes, con tantos
 * start-split como trozos de 188 bytes. Entrada: una casilla de mps bytes, con un
 * start-split en el microcuadro 0 y los complete-split desde el 2. Sin probar en
 * hardware (el QEMU no reparte los siTD por un traductor). */
static void sitd_armar(ehci_iso_t *e, uint32_t f)
{
    uint32_t k = f % ISO_N;
    sitd_t *t = sitd_de(e, k);
    uint32_t n, a;
    if (e->entrada) {
        if (!isoent_livres(&e->re)) { t->estado = 0; e->arm[k] = 0; return; }
        n = e->mps;
        a = e->anel_fis + isoent_postar(&e->re);
        e->arm[k] = 1;
    } else {
        n = usb_iso_calcular(&e->acc, e->bytes_s, e->tam_quadro, 1000, e->mps);
        uint32_t lim = e->anel_tam - e->pos_fila;
        if (n > lim) n = lim - lim % e->tam_quadro;
        a = e->anel_fis + e->pos_fila;
        e->pos_fila += n;
        if (e->pos_fila >= e->anel_tam) e->pos_fila -= e->anel_tam;
    }
    uint32_t hs = (n + SITD_SPLIT_MAX - 1) / SITD_SPLIT_MAX;
    if (!hs) hs = 1;
    uint32_t smask, cmask;
    if (e->entrada) { smask = 1; cmask = ((1u << (hs + 2)) - 1) << 2; }
    else { smask = (1u << hs) - 1; cmask = 0; }
    t->estado = 0;
    t->carac = (uint32_t)e->dev | ((uint32_t)e->ep << 8) | ((uint32_t)e->tt_hub << 16) | ((uint32_t)e->tt_porta << 24) |
               (e->entrada ? 1u << 31 : 0);
    t->mascaras = smask | (cmask << 8);
    t->pagina0 = a;
    t->pagina1 = ((a + (n ? n - 1 : 0)) & ~0xFFFu) | (e->entrada ? 0u : (hs > 1 ? 1u << 3 : 0u)) | (e->entrada ? 0u : hs);
    t->atras = T_TERMINA;
    t->pagina0_alta = t->pagina1_alta = 0;
    __asm__ volatile("" ::: "memory");
    t->estado = SITD_ATIVO | (n << 16);
    e->bytes[k] = (uint16_t)n;
    e->paquetes++;
}

/* Arma el iTD del cuadro f (en su posición k = f mod 64) con los paquetes siguientes. */
static void iso_armar(ehci_iso_t *e, uint32_t f)
{
    if (e->sitd) { sitd_armar(e, f); return; }
    uint32_t k = f % ISO_N;
    itd_t *t = itd_de(e, k);
    uint32_t paginas[7], np = 0, total = 0;
    uint32_t tr[8];
    uint32_t cnt = 8 / e->ivl_uf;
    if (e->entrada && isoent_livres(&e->re) < cnt) {      /* el consumidor va atrasado: este milisegundo queda vacío */
        for (int i = 0; i < 8; i++) t->trans[i] = 0;
        e->arm[k] = 0;
        return;
    }
    for (int i = 0; i < 8; i++) {
        tr[i] = 0;
        if (i % e->ivl_uf) continue;
        uint32_t n, a;
        if (e->entrada) {
            n = e->mps * e->mult;
            a = e->anel_fis + isoent_postar(&e->re);
        } else {
            n = usb_iso_calcular(&e->acc, e->bytes_s, e->tam_quadro, e->ivl_uf * 125u, e->mps);
            uint32_t lim = e->anel_tam - e->pos_fila;
            if (n > lim) n = lim - lim % e->tam_quadro;
            a = e->anel_fis + e->pos_fila;
        }
        uint32_t p0 = a >> 12, p1 = n ? (a + n - 1) >> 12 : p0;
        /* Una transacción usa la última página anotada (o una nueva) y, si cruza, la
         * siguiente a continuación en la lista. */
        int pg = -1;
        if (np && paginas[np - 1] == p0) pg = (int)np - 1;
        else if (np < 7) { paginas[np++] = p0; pg = (int)np - 1; }
        if (pg >= 0 && p1 != p0) {
            if (np < 7) paginas[np++] = p1;
            else pg = -1;
        }
        if (pg < 0) n = 0;                  /* no cabe en las 7 páginas: este paquete se pierde */
        tr[i] = ITD_ATIVO | (n << 16) | ((uint32_t)(pg < 0 ? 0 : pg) << 12) | (a & 0xFFFu);
        if (!e->entrada) {
            e->pos_fila += n;
            if (e->pos_fila >= e->anel_tam) e->pos_fila -= e->anel_tam;
        }
        total += n;
    }
    for (uint32_t i = 0; i < np; i++) t->pagina[i] = paginas[i] << 12;
    for (uint32_t i = np; i < 7; i++) t->pagina[i] = 0;
    t->pagina[0] |= (uint32_t)e->dev | ((uint32_t)e->ep << 8);
    /* tamaño máximo de paquete y sentido (bit 11 = entrada), y las transacciones por microcuadro */
    t->pagina[1] |= (e->mps & 0x7FFu) | (e->entrada ? 1u << 11 : 0u);
    t->pagina[2] |= e->entrada ? e->mult : 1u;
    for (int i = 0; i < 7; i++) t->pagina_alta[i] = 0;
    __asm__ volatile("" ::: "memory");
    for (int i = 0; i < 8; i++) t->trans[i] = tr[i];   /* al final: ya activo */
    if (e->entrada) e->arm[k] = (uint8_t)cnt;
    e->bytes[k] = (uint16_t)(total > 0xFFFF ? 0xFFFF : total);
    e->paquetes++;
}

static void iso_encher(ehci_iso_t *e)
{
    iso_relogio(e);
    iso_completar(e);
    if (!e->ligado) return;
    if ((int32_t)(e->prox - e->agora) < 2) {
        /* Nos quedamos atrás (el kernel estuvo sin atender el flujo): se salta al futuro
         * y se pierde ese trozo; los iTD atrasados quedaron completados arriba. */
        if (e->prox != e->vel) { e->subdesb++; }
        while (e->vel != e->prox) {          /* lo armado para cuadros pasados cuenta como completado */
            uint32_t k = e->vel % ISO_N;
            if (e->entrada) iso_recibir(e, k, true);
            itd_t *t = itd_de(e, k);
            if (e->sitd) sitd_de(e, k)->estado = 0;
            else for (int i = 0; i < 8; i++) t->trans[i] = 0;
            e->pos_feito += e->bytes[k];
            e->bytes[k] = 0;
            e->vel++;
        }
        e->prox = e->vel = e->agora + 3;
    }
    while ((int32_t)(e->prox - e->agora) < (int32_t)USB_ISO_COLA_MS && e->prox - e->vel < ISO_N - 4) {
        iso_armar(e, e->prox);
        e->prox++;
    }
}

int ehci_iso_abrir(usb_disp_t *d, const usb_iso_cfg_t *cfg)
{
    ehci_t *c = d->ctl;
    uint32_t mult = cfg->mult ? cfg->mult : 1;
    if (!c || !cfg->mps || cfg->mps > 1024 || mult > 3) return USB_ERRO;
    bool sitd = d->velocidade != USB_VEL_ALTA;
    if (sitd && (d->velocidade != USB_VEL_PLENA || cfg->mps > 1023 || mult > 1 || !d->tt_endereco)) {
        klog("[EHCI] isocrono de baja velocidad o sin traductor: no se puede\n");
        return USB_ERRO;
    }
    if (sitd && cfg->entrada && cfg->mps > SITD_ENTRADA_MAX) {
        klog("[EHCI] isocrono de entrada por siTD de %u bytes: demasiado grande (maximo %u)\n", cfg->mps, SITD_ENTRADA_MAX);
        return USB_ERRO;
    }
    if (cfg->entrada ? (cfg->anel_tam % (cfg->mps * mult) != 0 || cfg->anel_tam / (cfg->mps * mult) > ISOENT_CASILLAS_MAX)
                     : (!cfg->tam_quadro || cfg->anel_tam % cfg->tam_quadro || mult > 1)) return USB_ERRO;
    bool i0 = int_salva_desliga();
    int h = -1;
    for (int i = 0; i < USB_ISO_MAX * USB_MAX_CONTROLADORES; i++) {
        if (isoc[i].usado && isoc[i].c == c) {     /* la lista periódica solo lleva el anillo de iTD de un flujo */
            klog("[EHCI] isocrono: ya hay un flujo en este controlador\n");
            int_restaura(i0);
            return USB_ERRO;
        }
        if (!isoc[i].usado && h < 0) h = i;
    }
    /* Un dispositivo "1.x" que el EHCI ve en alta velocidad (el QEMU): un paquete por cuadro. */
    uint32_t b = cfg->intervalo ? cfg->intervalo : 1, ivl = 1u << (b > 4 ? 3 : b - 1);
    if (d->bcd_usb < 0x0200) ivl = 8;
    if (ivl > 8) ivl = 8;
    if (sitd) ivl = 8;                 /* un paquete por cuadro */
    if (cfg->entrada && !sitd && (cfg->anel_tam / (cfg->mps * mult)) % (8 / ivl) != 0) { int_restaura(i0); return USB_ERRO; }
    uint32_t ancho = cfg->mps * mult;
    int r = USB_ERRO;
    if (h >= 0 && c->ancho_iso + ancho <= USB_ISO_ANCHO_MAX) {
        uint32_t it = memfisica_alocar_seguidas(ISO_ESTRUTURAS / PAGINA);
        if (it && periodica_ligar(c)) {
            ehci_iso_t *e = &isoc[h];
            memset(e, 0, sizeof *e);
            memset((void *)it, 0, ISO_ESTRUTURAS);
            e->entrada = cfg->entrada;
            e->sitd = sitd;
            e->mult = (uint8_t)mult;
            e->tt_hub = d->tt_endereco;
            e->tt_porta = d->tt_porta;
            if (e->entrada && !isoent_iniciar(&e->re, cfg->anel_tam, cfg->mps * mult)) {
                memfisica_liberar_seguidas(it, ISO_ESTRUTURAS / PAGINA);
                int_restaura(i0);
                return USB_ERRO;
            }
            e->usado = true;
            e->c = c;
            e->itds = it;
            e->dev = d->endereco;
            e->ep = cfg->ep;
            e->ivl_uf = (uint8_t)ivl;
            e->mps = cfg->mps;
            e->anel_fis = cfg->anel_fis;
            e->anel_tam = cfg->anel_tam;
            e->bytes_s = cfg->bytes_s;
            e->tam_quadro = cfg->tam_quadro;
            e->ancho = ancho;
            e->ultimo_f = (c->op[FRINDEX] >> 3) & 0x7FF;
            e->agora = e->prox = e->vel = e->ultimo_f;
            c->ancho_iso += ancho;
            /* Cada entrada de la lista periódica pasa primero por el iTD (o siTD) de su cuadro (mod 64) */
            qh_t *cab = periodica_cabeca(c);
            for (uint32_t k = 0; k < ISO_N; k++) {
                if (sitd) sitd_de(e, k)->proximo = (uint32_t)cab | T_QH;
                else itd_de(e, k)->proximo = (uint32_t)cab | T_QH;
            }
            volatile uint32_t *lista = (volatile uint32_t *)c->periodica;
            for (uint32_t f = 0; f < 1024; f++)
                lista[f] = sitd ? ((uint32_t)sitd_de(e, f % ISO_N) | SITD_TIPO) : (it + 128u * (f % ISO_N));   /* tipo 0 = iTD */
            r = h;
        } else if (it) memfisica_liberar_seguidas(it, ISO_ESTRUTURAS / PAGINA);
    } else if (h >= 0)
        klog("[EHCI] isocrono: sin ancho de banda (%u + %u > %u bytes por microcuadro)\n", c->ancho_iso, ancho, USB_ISO_ANCHO_MAX);
    int_restaura(i0);
    return r;
}

static ehci_iso_t *iso_h(usb_disp_t *d, int h)
{
    if (h < 0 || h >= USB_ISO_MAX * USB_MAX_CONTROLADORES || !isoc[h].usado || isoc[h].c != d->ctl) return NULL;
    return &isoc[h];
}

void ehci_iso_ligar(usb_disp_t *d, int h)
{
    bool i0 = int_salva_desliga();
    ehci_iso_t *e = iso_h(d, h);
    if (e) {
        iso_relogio(e);
        iso_completar(e);
        while (e->vel != e->prox) { e->bytes[e->vel % ISO_N] = 0; e->vel++; }
        for (uint32_t k = 0; k < ISO_N; k++) {
            if (e->sitd) sitd_de(e, k)->estado = 0;
            else for (int i = 0; i < 8; i++) itd_de(e, k)->trans[i] = 0;
            e->arm[k] = 0;
        }
        if (e->entrada) isoent_reiniciar(&e->re);
        e->pos_fila = e->pos_feito = e->acc = 0;
        e->prox = e->vel = e->agora + 3;
        e->ligado = true;
        iso_encher(e);
    }
    int_restaura(i0);
}

void ehci_iso_parar(usb_disp_t *d, int h)
{
    bool i0 = int_salva_desliga();
    ehci_iso_t *e = iso_h(d, h);
    if (e) e->ligado = false;
    int_restaura(i0);
}

uint32_t ehci_iso_posicao(usb_disp_t *d, int h)
{
    bool i0 = int_salva_desliga();
    ehci_iso_t *e = iso_h(d, h);
    uint32_t r = 0;
    if (e) { iso_encher(e); r = e->pos_feito; }
    int_restaura(i0);
    return r;
}

bool ehci_iso_ler(usb_disp_t *d, int h, uint32_t *off, uint32_t *len, bool *erro)
{
    bool i0 = int_salva_desliga();
    ehci_iso_t *e = iso_h(d, h);
    bool r = e && e->entrada && isoent_ler(&e->re, off, len, erro);
    int_restaura(i0);
    return r;
}

bool ehci_iso_info(usb_disp_t *d, int h, usb_iso_info_t *i)
{
    ehci_iso_t *e = iso_h(d, h);
    if (!e) return false;
    i->paquetes = e->paquetes;
    i->completados = e->completados;
    i->subdesbordes = e->subdesb;
    i->errores = e->erros;
    i->ancho = e->ancho;
    return true;
}

void ehci_iso_fechar(usb_disp_t *d, int h)
{
    bool i0 = int_salva_desliga();
    ehci_iso_t *e = h >= 0 && h < USB_ISO_MAX * USB_MAX_CONTROLADORES && isoc[h].usado ? &isoc[h] : NULL;
    if (e) {
        ehci_t *c = e->c;
        e->ligado = false;
        qh_t *cab = periodica_cabeca(c);
        volatile uint32_t *lista = (volatile uint32_t *)c->periodica;
        for (uint32_t f = 0; f < 1024; f++) lista[f] = (uint32_t)cab | T_QH;
        ehci_esperar_ms(c, 3);              /* el controlador ya no los está leyendo */
        memfisica_liberar_seguidas(e->itds, ISO_ESTRUTURAS / PAGINA);
        if (c->ancho_iso >= e->ancho) c->ancho_iso -= e->ancho;
        e->usado = false;
    }
    (void)d;
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
