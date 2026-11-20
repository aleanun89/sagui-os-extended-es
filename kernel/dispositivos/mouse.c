// SPDX-License-Identifier: GPL-3.0-only
/* mouse.c - driver del mouse PS/2 (el touchpad del G470 habla el mismo protocolo).
 *
 * El mouse está en el segundo puerto ("auxiliar") del mismo controlador i8042 del
 * teclado. Para activarlo: habilitar el puerto auxiliar, activar la IRQ 12 en el byte
 * de configuración del controlador y enviar al mouse "valores por defecto" (0xF6) y
 * "empieza a enviar" (0xF4). Los comandos para el mouse pasan por el controlador
 * con el prefijo 0xD4.
 *
 * Después de eso, cada movimiento o clic llega como un paquete de 3 bytes:
 *   byte 0: botones (bits 0-2), bit 3 siempre 1, signos de X e Y (bits 4-5),
 *           desbordamiento de X e Y (bits 6-7);
 *   byte 1: desplazamiento X; byte 2: desplazamiento Y (hacia arriba es positivo).
 * El bit 3 siempre activado es lo que permite reencontrar el inicio de un paquete
 * si un byte se pierde.
 *
 * Rueda (D048): el i8042.c intenta el modo IntelliMouse (tasas 200, 100, 80 y
 * "¿cuál es su ID?"). Si el mouse responde ID 3, cada paquete gana un
 * cuarto byte: el giro de la rueda en muescas, con signo (+ = hacia abajo). Si
 * responde otra cosa (el touchpad del G470 suele decir 0), todo sigue
 * con 3 bytes. Para no quedar desincronizado para siempre si un byte se
 * pierde, un paquete a medias se descarta después de medio segundo parado.
 *
 * En una máquina virtual (QEMU, VMware) el mouse PS/2 solo da desplazamientos, y el
 * cursor de SaguiOS no coincide con el cursor del computador de fuera. Si la CPU
 * dice que corre bajo un hipervisor y el "puerto trasero" de VMware (vmmouse,
 * puerto 0x5658) responde, el driver pide posiciones absolutas: el PS/2 sigue
 * generando la IRQ 12 en cada evento, pero la posición y los botones pasan a venir de la
 * cola del vmmouse. En el G470 nada de eso se intenta. Ver D035. */
#include "mouse.h"
#include "console.h"
#include "cpu.h"
#include "portas.h"
#include "processo.h"
#include "video.h"
#include "telas.h"
#include "i8042.h"
#include "relogio.h"
#include "painel.h"

#define FILA   64

static uint8_t pacote[4];
static int pos_pacote, tam_pacote = 3;
static uint32_t tique_ultimo_byte;
static int mx, my, larg = 1024, alt = 768;   /* límites: el tamaño de la pantalla (mouse_iniciar, mouse_tela_mudou) */
static uint8_t botoes;
static evento_mouse_t fila[FILA];
static uint32_t ini, fim;
static const char canal = 0;
static bool absoluto;   /* vmmouse activado: la posición viene de la máquina virtual */
static bool captura;    /* D242: un programa pidió la captura: el cursor no se mueve y los deltas van crudos */
static bool vm_relativo;   /* durante la captura, el vmmouse está en modo relativo */
static int abs_ult_x, abs_ult_y;   /* tableta USB capturada: última posición, para sacar el delta */

#define VMWARE_MAGICO   0x564D5868u
#define VMWARE_PORTA    0x5658
#define VMW_VERSAO      10
#define VMW_MOUSE_DADOS 39
#define VMW_MOUSE_ESTADO 40
#define VMW_MOUSE_CMD   41
#define VMMOUSE_LER_ID   0x45414552u
#define VMMOUSE_ABSOLUTO 0x53424152u
#define VMMOUSE_RELATIVO 0x4C455252u   /* "RREL": D242, modo relativo mientras dura la captura */
#define VMMOUSE_DESLIGAR 0x000000F5u
#define VMMOUSE_ID       0x3442554Au

/* "in" en el puerto trasero: EAX = mágico, ECX = comando, EBX = argumento. */
static void vmware(uint32_t cmd, uint32_t arg, uint32_t r[4])
{
    uint32_t a = VMWARE_MAGICO, b = arg, c = cmd, d = VMWARE_PORTA;
    __asm__ volatile("inl %%dx, %%eax" : "+a"(a), "+b"(b), "+c"(c), "+d"(d));
    r[0] = a; r[1] = b; r[2] = c; r[3] = d;
}

static bool sob_hipervisor(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    return (c >> 31) & 1;   /* CPUID.1:ECX bit 31, activado por QEMU, VMware, VirtualBox... */
}

static bool vmmouse_ligar(void)
{
    if (!sob_hipervisor()) return false;
    uint32_t r[4];
    vmware(VMW_VERSAO, 0, r);
    if (r[1] != VMWARE_MAGICO) return false;
    vmware(VMW_MOUSE_CMD, VMMOUSE_LER_ID, r);
    vmware(VMW_MOUSE_ESTADO, 0, r);
    if ((r[0] & 0xFFFF) == 0 || (r[0] & 0xFFFF0000u) == 0xFFFF0000u) return false;
    vmware(VMW_MOUSE_DADOS, 1, r);
    if (r[0] != VMMOUSE_ID) return false;
    vmware(VMW_MOUSE_CMD, captura ? VMMOUSE_RELATIVO : VMMOUSE_ABSOLUTO, r);
    vm_relativo = captura;
    return true;
}

const void *mouse_canal(void) { return &canal; }

static int dw_pendente;   /* D407: rueda horizontal del paquete que se está tratando (panel táctil) */

static void guardar(int16_t dx, int16_t dy, uint8_t mudaram, int dz)
{
    uint32_t prox = (fim + 1) % FILA;
    if (prox == ini) ini = (ini + 1) % FILA;   /* cola llena: olvida el más antiguo */
    fila[fim] = (evento_mouse_t){ (int16_t)mx, (int16_t)my, dx, dy, botoes, mudaram, (int8_t)dz, (int8_t)dw_pendente };
    dw_pendente = 0;
    fim = prox;
    acordar(&canal);
}

static void mover_para(int x, int y, uint8_t novos, int dz);

/* Con el vmmouse activado, cada paquete PS/2 solo avisa de que hay eventos en la cola
 * suya: 4 palabras por evento (botones, x, y, rueda), con x e y de 0 a 0xFFFF
 * y la rueda como entero con signo (+ = hacia abajo, como en el IntelliMouse). */
static void tratar_vmmouse(void)
{
    uint32_t r[4];
    for (int n = 0; n < 16; n++) {
        vmware(VMW_MOUSE_ESTADO, 0, r);
        if ((r[0] & 0xFFFF0000u) == 0xFFFF0000u) {   /* error: reactiva el modo absoluto */
            vmware(VMW_MOUSE_CMD, VMMOUSE_DESLIGAR, r);
            absoluto = vmmouse_ligar();
            return;
        }
        if ((r[0] & 0xFFFF) < 4) return;
        vmware(VMW_MOUSE_DADOS, 4, r);
        uint8_t novos = (uint8_t)(((r[0] & 0x20) ? BOTAO_ESQUERDO : 0) |
                                  ((r[0] & 0x10) ? BOTAO_DIREITO : 0) |
                                  ((r[0] & 0x08) ? BOTAO_MEIO : 0));
        int dz = (int)r[3];
        if (dz > 127) dz = 127;
        if (dz < -127) dz = -127;
        if (vm_relativo) {   /* D242: x e y son desplazamientos con signo (y crece hacia abajo) */
            mover_para(mx + (int)r[1], my + (int)r[2], novos, dz);
            continue;
        }
        mover_para((int)(((r[1] & 0xFFFF) * (uint32_t)larg + 0x8000) >> 16),   /* redondeado */
                   (int)(((r[2] & 0xFFFF) * (uint32_t)alt + 0x8000) >> 16), novos, dz);
    }
}

static void tratar_pacote(void)
{
    if (absoluto) { tratar_vmmouse(); return; }
    uint8_t b0 = pacote[0];
    if (b0 & 0xC0) return;                         /* desbordamiento: el valor no es fiable */
    int dx = pacote[1] - ((b0 & 0x10) ? 256 : 0);
    int dy = pacote[2] - ((b0 & 0x20) ? 256 : 0);
    int dz = tam_pacote == 4 ? (int8_t)pacote[3] : 0;
    mover_para(mx + dx, my - dy, b0 & 7, dz);      /* en la pantalla, y crece hacia abajo */
}

static void mover_para(int x, int y, uint8_t novos, int dz)
{
    int dx = x - mx, dy = y - my;
    if (captura) {
        /* D242: los deltas van crudos (sin recortar al borde); el cursor ni se mueve ni se dibuja. */
        uint8_t mud = novos ^ botoes;
        botoes = novos;
        if (dx > 32767) dx = 32767;
        if (dx < -32768) dx = -32768;
        if (dy > 32767) dy = 32767;
        if (dy < -32768) dy = -32768;
        if (dx || dy || mud || dz || dw_pendente) guardar((int16_t)dx, (int16_t)dy, mud, dz);
        return;
    }
    mx = x;
    my = y;
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    if (mx >= larg) mx = larg - 1;
    if (my >= alt) my = alt - 1;
    telas_prender(&mx, &my);   /* D290: nunca en la zona sin pantalla del escritorio */
    uint8_t mudaram = novos ^ botoes;
    botoes = novos;
    if (mudaram & BOTAO_ESQUERDO)
        klog("[MOUSE] botao esquerdo %s em (%d,%d)\n", novos & BOTAO_ESQUERDO ? "apertado" : "solto", mx, my);
    if (mudaram & BOTAO_DIREITO)
        klog("[MOUSE] botao direito %s em (%d,%d)\n", novos & BOTAO_DIREITO ? "apertado" : "solto", mx, my);
    if (dz) klog("[MOUSE] roda %d em (%d,%d)\n", dz, mx, my);
    if (dw_pendente) klog("[MOUSE] roda horizontal %d em (%d,%d)\n", dw_pendente, mx, my);
    guardar((int16_t)dx, (int16_t)dy, mudaram, dz);
    if (video_ativo()) {
        video_cursor(mx, my, true);
        video_mostrar();
    }
}

/* Cada byte del mouse llega aquí por i8042.c, venga de la IRQ 12, de la IRQ 1 o
 * de la comprobación del reloj. */
void mouse_byte(uint8_t b)
{
    if (painel_ativo()) { painel_byte(b); return; }   /* D407: panel táctil en modo absoluto (paquetes de 6 bytes) */
    uint32_t agora = relogio_tiques();
    if (pos_pacote > 0 && agora - tique_ultimo_byte > 50) pos_pacote = 0;   /* paquete a medias, viejo */
    tique_ultimo_byte = agora;
    if (pos_pacote == 0 && !(b & 0x08)) return;   /* fuera de sincronía: espera un byte 0 válido */
    pacote[pos_pacote++] = b;
    if (pos_pacote >= tam_pacote) {
        pos_pacote = 0;
        tratar_pacote();
    }
}

/* El i8042.c avisa el ID del mouse antes de activar la IRQ 12. */
void mouse_formato(int id)
{
    tam_pacote = id == 3 ? 4 : 3;
    pos_pacote = 0;
}

static bool usb_com_roda;   /* D214: hay un ratón USB con rueda */

bool mouse_tem_roda(void) { return tam_pacote == 4 || absoluto || usb_com_roda || painel_ativo(); }

/* D407: un evento de los gestos del panel táctil. La rueda va recortada a lo que cabe en el evento. */
void mouse_painel(int dx, int dy, int rv, int rh, uint8_t novos)
{
    if (rv > 127) rv = 127;
    if (rv < -127) rv = -127;
    if (rh > 127) rh = 127;
    if (rh < -127) rh = -127;
    dw_pendente = rh;
    mover_para(mx + dx, my + dy, novos, rv);
    dw_pendente = 0;
}

void mouse_usb_presente(bool com_roda)
{
    if (com_roda) usb_com_roda = true;
}

void mouse_usb(int dx, int dy, int dz, uint8_t botoes)
{
    if (dz > 127) dz = 127;
    if (dz < -127) dz = -127;
    mover_para(mx + dx, my + dy, botoes, dz);
}

void mouse_usb_absoluto(uint32_t x, uint32_t xmax, uint32_t y, uint32_t ymax, int dz, uint8_t botoes)
{
    if (!xmax || !ymax) return;
    if (dz > 127) dz = 127;
    if (dz < -127) dz = -127;
    while (xmax > 0xFFFF) { xmax >>= 1; x >>= 1; }   /* las cuentas caben en 32 bits */
    while (ymax > 0xFFFF) { ymax >>= 1; y >>= 1; }
    if (x > xmax) x = xmax;
    if (y > ymax) y = ymax;
    int px = (int)(x * (uint32_t)larg / (xmax + 1u)), py = (int)(y * (uint32_t)alt / (ymax + 1u));
    if (captura) {   /* D242: una tableta no tiene deltas; se calculan contra la lectura anterior */
        mover_para(mx + (px - abs_ult_x), my + (py - abs_ult_y), botoes, dz);
        abs_ult_x = px; abs_ult_y = py;
        return;
    }
    abs_ult_x = px; abs_ult_y = py;
    mover_para(px, py, botoes, dz);
}

/* D242: activa o suelta la captura del ratón. Al capturar, el cursor se queda en (x, y) (el
 * centro de la ventana) y desaparece; con vmmouse se pasa al modo relativo del dispositivo para
 * que el cursor del anfitrión no choque con el borde. Al soltar vuelve al modo absoluto y el
 * cursor reaparece donde estaba. */
void mouse_captura(bool lig, int x, int y)
{
    bool i = int_salva_desliga();
    if (lig == captura) { int_restaura(i); return; }
    captura = lig;
    if (lig) { mx = x; my = y; }
    if (absoluto) {
        uint32_t r[4];
        vmware(VMW_MOUSE_CMD, lig ? VMMOUSE_RELATIVO : VMMOUSE_ABSOLUTO, r);
        vm_relativo = lig;
        for (int n = 0; n < 64; n++) {   /* descarta lo que quedara en la cola del modo anterior */
            vmware(VMW_MOUSE_ESTADO, 0, r);
            if ((r[0] & 0xFFFF0000u) == 0xFFFF0000u || (r[0] & 0xFFFF) < 4) break;
            vmware(VMW_MOUSE_DADOS, 4, r);
        }
    }
    if (video_ativo()) {
        if (lig) video_cursor(mx, my, false); else video_cursor(mx, my, true);
        video_mostrar();
    }
    klog("[MOUSE] captura %s, cursor em (%d,%d)%s\n", lig ? "ligada (deltas crudos, cursor fijo)" : "solta", mx, my,
         absoluto ? (lig ? " [vmmouse relativo]" : " [vmmouse absoluto]") : "");
    int_restaura(i);
}

bool mouse_capturado(void) { return captura; }

/* D251: el escritorio remoto (injecao.c) manda posiciones absolutas en la
 * pantalla, como el vmmouse o una tableta USB: el mismo camino (mover_para),
 * así que cursor, clics, arrastres, foco y captura se portan igual. */
static int iny_ult_x, iny_ult_y;

void mouse_injetar(int x, int y, uint8_t novos)
{
    bool i = int_salva_desliga();
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= larg) x = larg - 1;
    if (y >= alt) y = alt - 1;
    if (captura) mover_para(mx + (x - iny_ult_x), my + (y - iny_ult_y), novos, 0);   /* D242: deltas */
    else mover_para(x, y, novos, 0);
    iny_ult_x = x; iny_ult_y = y;
    int_restaura(i);
}

/* Rueda (o soltar los botones) donde esté el cursor. */
void mouse_injetar_aqui(uint8_t novos, int dz)
{
    bool i = int_salva_desliga();
    if (dz > 127) dz = 127;
    if (dz < -127) dz = -127;
    mover_para(mx, my, novos, dz);
    int_restaura(i);
}

uint8_t mouse_botoes(void) { return botoes; }

bool mouse_iniciar(void)
{
    if (video_ativo()) {
        larg = (int)video_largura();
        alt = (int)video_altura();
    }
    mx = larg / 2;
    my = alt / 2;
    pos_pacote = 0;
    bool ok = i8042_iniciar(true);
    if (ok && !painel_ativo()) absoluto = vmmouse_ligar();
    if (ok && video_ativo()) {
        video_cursor(mx, my, true);
        video_mostrar();
    }
    klog("[MOUSE] %s\n", ok ? "mouse PS/2 ligado" : "mouse PS/2 nao respondeu");
    if (ok) {
        int id = i8042_diagnostico()->id_mouse;
        if (painel_ativo()) klog("[MOUSE] painel tactil %s: modo absoluto, pacotes de 6 bytes (D407)\n", painel_nome());
        else if (tam_pacote == 4) klog("[MOUSE] IntelliMouse (id 3): roda ligada, pacotes de 4 bytes\n");
        else klog("[MOUSE] id %d: sem roda, pacotes de 3 bytes\n", id);
    }
    if (absoluto) klog("[MOUSE] vmmouse: posicao absoluta da maquina virtual (roda na quarta palavra)\n");
    return ok;
}

bool mouse_tentar(evento_mouse_t *e)
{
    bool i = int_salva_desliga();
    bool tem = ini != fim;
    if (tem) { *e = fila[ini]; ini = (ini + 1) % FILA; }
    int_restaura(i);
    return tem;
}

void mouse_posicao(int *x, int *y) { *x = mx; *y = my; }

/* El modo de vídeo cambió (D059): límites nuevos. El vmmouse sigue enviando
 * 0..0xFFFF y la cuenta de tratar_vmmouse ya usa larg/alt. Llamado con las
 * interrupciones desactivadas. */
void mouse_tela_mudou(int l, int a)
{
    larg = l;
    alt = a;
    if (mx >= larg) mx = larg - 1;
    if (my >= alt) my = alt - 1;
    if (video_ativo()) video_cursor(mx, my, true);
    klog("[MOUSE] limites %dx%d, cursor em (%d,%d)\n", larg, alt, mx, my);
}
