// SPDX-License-Identifier: GPL-3.0-only
/* i8042.c - el controlador del teclado y del touchpad (H19-fix).
 *
 * El teclado y el mouse/touchpad PS/2 están detrás del mismo chip, que tiene UNA
 * cola de salida de un byte. El bit 5 del estado dice de quién es el byte que está
 * esperando. Si un byte no se lee, ningún otro pasa: un byte del mouse
 * olvidado en la cola bloquea el teclado. Fue lo que pasó en el G470 en la fase 4.
 *
 * Por eso este es el único lugar que lee la cola. Cada byte se entrega al
 * teclado o al mouse según el bit 5, y la cola se vacía:
 *   - en la interrupción del teclado (IRQ 1);
 *   - en la interrupción del mouse (IRQ 12);
 *   - en cada tic del reloj, como red de seguridad: si una de las
 *     interrupciones no llega (configuración distinta en el hardware real),
 *     nada queda parado más de 10 ms.
 *
 * La configuración del chip se lee con la cola vacía y se comprueba; si la lectura
 * falla o sale extraña, usamos valores seguros, manteniendo siempre la
 * interrupción y la traducción del teclado activadas. */
#include "i8042.h"
#include "console.h"
#include "cpu.h"
#include "portas.h"
#include "painel.h"

#define DADOS  0x60
#define ESTADO 0x64

void teclado_byte(uint8_t b);
void mouse_byte(uint8_t b);
void mouse_formato(int id);

static bool mouse_ligado;
static i8042_diag_t diag;

const i8042_diag_t *i8042_diagnostico(void) { return &diag; }

static bool esperar_escrita(void)
{
    for (int i = 0; i < 200000; i++)
        if (!(porta_le8(ESTADO) & 2)) return true;
    return false;
}

/* Espera un byte durante ~0,5 s (cada lectura de puerto tarda ~1 µs). */
static int ler_resposta(void)
{
    for (int i = 0; i < 500000; i++)
        if (porta_le8(ESTADO) & 1) return porta_le8(DADOS);
    return -1;
}

static void descartar_pendentes(void)
{
    for (int i = 0; i < 32 && (porta_le8(ESTADO) & 1); i++) {
        porta_le8(DADOS);
        porta_pausa();
    }
}

static void comando(uint8_t c)
{
    esperar_escrita();
    porta_escreve8(ESTADO, c);
}

/* Envía un byte al mouse (prefijo 0xD4) y devuelve la respuesta. 0xFE = "repita". */
static int para_mouse(uint8_t c)
{
    for (int tentativa = 0; tentativa < 3; tentativa++) {
        comando(0xD4);
        esperar_escrita();
        porta_escreve8(DADOS, c);
        int r = ler_resposta();
        if (r != 0xFE) return r;
    }
    return 0xFE;
}

/* Como ler_resposta, pero solo acepta un byte que venga del mouse (bit 5 del
 * estado): una tecla pulsada durante el arranque no se convierte en "ID del mouse". */
static int ler_resposta_mouse(void)
{
    for (int i = 0; i < 500000; i++) {
        uint8_t estado = porta_le8(ESTADO);
        if (!(estado & 1)) continue;
        uint8_t b = porta_le8(DADOS);
        if (estado & 0x20) return b;
    }
    return -1;
}

static bool taxa_amostragem(uint8_t t)
{
    return para_mouse(0xF3) == 0xFA && para_mouse(t) == 0xFA;
}

/* Rueda del mouse (D048): el "golpe secreto" del IntelliMouse. Cambiar la tasa
 * de muestreo a 200, 100 y 80 y pedir el ID (0xF2): un mouse con rueda
 * responde 3 y pasa a enviar paquetes de 4 bytes; los demás (el touchpad del
 * G470, por ejemplo) responden 0 y siguen con 3 bytes. Cualquier
 * respuesta extraña vale como "sin rueda". Al final la tasa vuelve a 100. */
static int detectar_roda(void)
{
    int id = -1;
    if (taxa_amostragem(200) && taxa_amostragem(100) && taxa_amostragem(80) && para_mouse(0xF2) == 0xFA)
        id = ler_resposta_mouse();
    taxa_amostragem(100);
    return id;
}

/* D407: las órdenes al ratón para painel.c (reconocer el panel táctil y pasarlo al modo absoluto),
 * con las interrupciones desactivadas. */
int i8042_rato_cmd(uint8_t c) { return para_mouse(c); }
int i8042_rato_ler(void) { return ler_resposta_mouse(); }

/* La parte del ratón del arranque: valores por defecto, panel táctil (D407) o rueda (D048) y «empieza
 * a enviar». painel: se puede probar el modo absoluto del panel táctil. true = el ratón respondió. */
static bool iniciar_rato(bool painel)
{
    diag.resposta_padrao = para_mouse(0xF6);   /* valores por defecto */
    if (diag.resposta_padrao != 0xFA) {
        /* Algunos touchpads solo responden después de un reset (0xFF -> FA, AA, 00). */
        diag.resposta_reset = para_mouse(0xFF);
        if (diag.resposta_reset == 0xFA) { ler_resposta(); ler_resposta(); }
        diag.resposta_padrao = para_mouse(0xF6);
    }
    bool abs = false;
    if (diag.resposta_padrao == 0xFA) {
        /* D407: primero el panel táctil (deja el ratón como estaba si no lo es o algo falla) */
        abs = painel_detectar(painel);
        diag.id_mouse = abs ? 0 : detectar_roda();
    }
    mouse_formato(diag.id_mouse);              /* antes de la IRQ 12: el tamaño del paquete ya correcto */
    diag.resposta_ligar = para_mouse(0xF4);    /* empezar a enviar */
    bool ok = diag.resposta_padrao == 0xFA && diag.resposta_ligar == 0xFA;
    if (!ok && abs) painel_desligar();         /* el panel no empezó: el ratón (si responde) como siempre */
    return ok;
}

/* D407: vuelve a empezar el ratón (desde una llamada al sistema: pasar el panel táctil al modo PS/2 de
 * siempre o volver al absoluto). Para el ratón, lo reinicia (0xFF) y repite lo del arranque. Tarda un
 * momento con las interrupciones desactivadas: solo cuando la persona cambia la opción. */
bool i8042_rato_reiniciar(bool painel)
{
    bool ints = int_salva_desliga();
    bool antes = mouse_ligado;
    mouse_ligado = false;                      /* lo que llegue mientras tanto se descarta */
    for (int i = 0; i < 3; i++) {              /* parar el envío; entre medias pueden llegar datos */
        comando(0xD4);
        esperar_escrita();
        porta_escreve8(DADOS, 0xF5);
        int r = -1;
        for (int k = 0; k < 64 && r != 0xFA; k++) {
            r = ler_resposta_mouse();
            if (r < 0) break;                  /* nada en medio segundo: no insistir */
        }
        if (r == 0xFA) break;
    }
    descartar_pendentes();
    painel_desligar();
    diag.resposta_reset = para_mouse(0xFF);    /* el reinicio deja el panel en relativo */
    if (diag.resposta_reset == 0xFA) { ler_resposta(); ler_resposta(); }
    bool ok = iniciar_rato(painel);
    descartar_pendentes();
    mouse_ligado = ok || antes;
    int_restaura(ints);
    klog("[I8042] raton reiniciado (%s): padrao=%x id=%x ligar=%x -> %s\n", painel ? "panel permitido" : "solo PS/2",
         (uint32_t)diag.resposta_padrao, (uint32_t)diag.id_mouse, (uint32_t)diag.resposta_ligar, ok ? "ligado" : "nao respondeu");
    return ok;
}

/* Entrega todo lo que haya en la cola. */
void i8042_esvaziar(void)
{
    for (int i = 0; i < 32; i++) {
        uint8_t estado = porta_le8(ESTADO);
        if (!(estado & 1)) return;
        uint8_t b = porta_le8(DADOS);
        if (estado & 0x20) {
            if (mouse_ligado) mouse_byte(b);   /* sin mouse activado, el byte se descarta */
        } else {
            teclado_byte(b);
        }
    }
}

static quadro_t *ao_interromper(quadro_t *q)
{
    i8042_esvaziar();
    return q;
}

void i8042_tique(void)
{
    if (porta_le8(ESTADO) & 1)
        i8042_esvaziar();
    if (mouse_ligado) painel_tique();   /* D407: los clics de los toques que se sueltan solos */
}

static void gravar_config(uint8_t cfg)
{
    comando(0x60);
    esperar_escrita();
    porta_escreve8(DADOS, cfg);
}

bool i8042_iniciar(bool com_mouse)
{
    bool ints = int_salva_desliga();
    diag = (i8042_diag_t){ -1, -1, -1, -1, 0, -1 };
    comando(0xAD);                       /* teclado y mouse quietos durante la configuración */
    comando(0xA7);
    descartar_pendentes();               /* la cola TIENE que estar vacía antes de pedir la configuración */

    comando(0x20);
    int lida = ler_resposta();
    diag.config_lida = lida;
    /* Los bits 3 y 7 están reservados (cero). Si la lectura falló o llegó
     * extraña, parte de un valor seguro: interrupción del teclado, "sistema
     * pasó el POST" y traducción al código de teclas set 1. */
    uint8_t cfg = (lida < 0 || (lida & 0x88)) ? 0x45 : (uint8_t)lida;
    cfg |= 0x01 | 0x40;                  /* siempre: IRQ 1 y traducción (el mapa ABNT2 usa el set 1) */
    cfg &= (uint8_t)~0x10;               /* reloj del teclado activado */
    cfg &= (uint8_t)~0x02;               /* IRQ 12 solo después de que responda el mouse */
    cfg |= 0x20;                         /* reloj del mouse desactivado por ahora */
    gravar_config(cfg);
    comando(0xAE);                       /* teclado de vuelta */

    bool ok = false;
    if (com_mouse) {
        comando(0xA8);                   /* puerto del mouse */
        cfg &= (uint8_t)~0x20;
        gravar_config(cfg);
        descartar_pendentes();
        ok = iniciar_rato(true);
        if (ok) {
            cfg |= 0x02;                 /* ahora sí: IRQ 12 */
        } else {
            comando(0xA7);               /* mouse mudo: puerto desactivado, para no ensuciar la cola */
            cfg |= 0x20;
        }
        gravar_config(cfg);
        descartar_pendentes();
    }
    diag.config_final = cfg;
    mouse_ligado = ok;
    irq_registrar(1, ao_interromper);
    if (ok) irq_registrar(12, ao_interromper);
    int_restaura(ints);
    klog("[I8042] config lida %x, gravada %x; mouse: padrao=%x reset=%x id=%x ligar=%x -> %s\n",
         (uint32_t)diag.config_lida, cfg, (uint32_t)diag.resposta_padrao, (uint32_t)diag.resposta_reset,
         (uint32_t)diag.id_mouse, (uint32_t)diag.resposta_ligar, ok ? "ligado" : "nao respondeu");
    return ok;
}
