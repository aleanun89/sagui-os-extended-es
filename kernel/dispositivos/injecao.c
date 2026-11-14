// SPDX-License-Identifier: GPL-2.0-or-later
/* injecao.c - entrada inyectada para el escritorio remoto (D251, mejora 097).
 *
 * SIS_INYECTAR deja al servidor VNC (vncd) meter teclas y ratón como si
 * vinieran del hardware: las teclas van al búfer del teclado (teclado.c) y el
 * ratón a mover_para (mouse.c), así que el gestor de ventanas no distingue
 * una cosa de la otra (foco, atajos, arrastres, captura).
 *
 * Quién puede (lo delicado): manejar el teclado y el ratón es manejar todo el
 * sistema, así que solo un proceso a la vez tiene el permiso, y solo si
 *   1. se cargó de /bin/vncd o /apps/bin/vncd (lo anota el cargador:
 *      processo_t.exe_sistema = EXE_VNCD);
 *   2. su padre es el supervisor de servicios registrado (D216), y ese
 *      supervisor se cargó de /bin/servicios: es decir, lo lanzó el gestor
 *      de servicios como servicio, no alguien a mano desde el Terminal;
 *   3. lo pide (INY_ESTADO 1). Mientras viva lo conserva; otro vncd recibe
 *      ERRO_OCUPADO. Al soltarlo (INY_ESTADO 0) o al morir, lo remoto que
 *      quedara pulsado (modificadores, teclas, botones) se suelta.
 * El resto recibe ERRO_ACESSO. Los argumentos se validan: códigos de tecla y
 * botones fuera de rango y opciones desconocidas dan ERRO_INVALIDO; la
 * posición se recorta a la pantalla. */
#include "abi.h"
#include "console.h"
#include "cpu.h"
#include "mouse.h"
#include "processo.h"
#include "sistema.h"
#include "supervisor.h"
#include "teclado.h"

static int dono;              /* pid con el permiso (0 = nadie) */
static uint8_t botoes_dono;   /* botones que el remoto tiene pulsados (BOTAO_*) */

static bool vivo(int pid)
{
    processo_t *p = pid > 0 ? processo_por_pid(pid) : NULL;
    return p && p->estado != P_ZUMBI && p->estado != P_LIVRE;
}

/* Deja de haber dueño y suelta lo que tuviera pulsado. Con las interrupciones apagadas. */
static void soltar(const char *motivo)
{
    klog("[INYECTAR] permiso retirado al pid %d (%s)\n", dono, motivo);
    dono = 0;
    teclado_injetar_soltar();
    if (botoes_dono) {
        mouse_injetar_aqui((uint8_t)(mouse_botoes() & ~botoes_dono), 0);
        botoes_dono = 0;
    }
}

/* El gestor de ventanas, en cada vuelta: si el dueño murió con algo pulsado,
 * se suelta (un Alt remoto pegado dejaría el Alt+Tab abierto para siempre). */
void injecao_vigiar(void)
{
    if (!dono) return;
    bool i = int_salva_desliga();
    if (dono && !vivo(dono)) soltar("el proceso termino");
    int_restaura(i);
}

static uint8_t botoes_de(uint32_t b)
{
    return (uint8_t)(((b & INY_BOTON_IZQ) ? BOTAO_ESQUERDO : 0) | ((b & INY_BOTON_MEDIO) ? BOTAO_MEIO : 0) |
                     ((b & INY_BOTON_DER) ? BOTAO_DIREITO : 0));
}

static int32_t pedir(processo_t *p, uint32_t a)
{
    if (a == 0) {
        if (dono != p->pid) return ERRO_ACESSO;
        soltar("lo pidio");
        return 0;
    }
    if (a != 1) return ERRO_INVALIDO;
    if (dono == p->pid) return 0;
    const char *falta = NULL;
    int sup = supervisor_pid_confiavel();
    if (p->exe_sistema != EXE_VNCD) falta = "no es /bin/vncd ni /apps/bin/vncd";
    else if (!sup || p->pai != sup) falta = "no lo lanzo el gestor de servicios";
    if (falta) {
        klog("[INYECTAR] permiso denegado a %s (pid %d): %s\n", p->nome, p->pid, falta);
        return ERRO_ACESSO;
    }
    if (dono && vivo(dono)) {
        klog("[INYECTAR] permiso denegado a %s (pid %d): ya lo tiene el pid %d\n", p->nome, p->pid, dono);
        return ERRO_OCUPADO;
    }
    if (dono) soltar("el proceso termino");
    dono = p->pid;
    botoes_dono = 0;
    klog("[INYECTAR] permiso concedido a %s (pid %d)\n", p->nome, p->pid);
    return 0;
}

/* SIS_INYECTAR (INY_*, a, b): ver abi.h. */
int32_t sis_inyectar(uint32_t op, uint32_t a, uint32_t b, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    processo_t *p = processo_recursos();   /* un hilo del vncd cuenta como el vncd */
    bool i = int_salva_desliga();
    int32_t r = 0;
    if (op == INY_ESTADO) { r = pedir(p, a); goto fim; }
    if (!dono || dono != p->pid) { r = ERRO_ACESSO; goto fim; }
    switch (op) {
    case INY_TECLA: {
        int t = (int)a;
        bool valida = (t >= 0 && t <= 0xFF) || (t >= TECLA_CIMA && t <= TECLA_IMPRPANT);
        if (!valida || (b & ~(uint32_t)(INY_PULSADA | INY_MOD_SHIFT | INY_MOD_CTRL | INY_MOD_ALT))) {
            r = ERRO_INVALIDO;
            break;
        }
        uint32_t m = ((b & INY_MOD_SHIFT) ? MOD_SHIFT : 0) | ((b & INY_MOD_CTRL) ? MOD_CTRL : 0) |
                     ((b & INY_MOD_ALT) ? MOD_ALT : 0);
        teclado_injetar(t, m, (b & INY_PULSADA) != 0);
        break;
    }
    case INY_RATON: {
        if (b & ~(uint32_t)(INY_BOTON_IZQ | INY_BOTON_MEDIO | INY_BOTON_DER)) { r = ERRO_INVALIDO; break; }
        uint8_t novos = botoes_de(b);
        /* Los del teclado/ratón real que estén pulsados se respetan; solo cambian los remotos. */
        uint8_t todos = (uint8_t)((mouse_botoes() & ~botoes_dono) | novos);
        botoes_dono = novos;
        mouse_injetar((int)(a >> 16), (int)(a & 0xFFFF), todos);   /* recortada a la pantalla */
        break;
    }
    case INY_RUEDA: {
        int dz = (int)a;
        if (dz < -127 || dz > 127) { r = ERRO_INVALIDO; break; }
        if (b) { r = ERRO_INVALIDO; break; }
        if (dz) mouse_injetar_aqui(mouse_botoes(), dz);
        break;
    }
    default:
        r = ERRO_INVALIDO;
    }
fim:
    int_restaura(i);
    return r;
}
