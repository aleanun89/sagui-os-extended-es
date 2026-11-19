// SPDX-License-Identifier: GPL-2.0-or-later
/* fujitsu.c - los dispositivos ACPI propios de Fujitsu (y Fujitsu Siemens) (D403).
 *
 * Escrito para SaguiOS a partir de lo que se ve en el AML de estos portátiles
 * (FUJ02B1 y FUJ02E3 de los Lifebook/Amilo); los números de las funciones y de
 * los bits son los mismos que usa el driver fujitsu-laptop de Linux
 * (GPL-2.0-or-later), que sirvió de referencia del protocolo; no hay código
 * copiado de él.
 *
 * FUJ02B1 («brillo de Fujitsu»), métodos sin argumentos salvo SBLL:
 *   RBLL     cuántos niveles hay (0..RBLL-1);
 *   GBLL     el nivel actual (bits 0-27; el resto son marcas);
 *   SBLL(n)  pone el nivel (SBL2 en algunos modelos);
 *   Notify(0x80) cuando la tecla de brillo cambió el nivel en el firmware: el
 *   nuevo se lee con GBLL y se confirma con SBLL (Linux hace lo mismo).
 * FUJ02E3 («extras»): un único método FUNC(función, operación, campo, valor):
 *   0x1000 banderas: op 0 lo que hay, op 1 las «teclas blandas» pendientes
 *          (bit 5 radio, 26 panel táctil, 29 micrófono; se borran al leerlas),
 *          op 4 el estado (bit 5 el interruptor de la radio), op 5 escribe
 *          (bit 5: la luz de la radio, en los que tienen el bit 17 de op 0);
 *   0x1002 botones: op 0 qué botones hay, op 1 saca el siguiente del anillo
 *          (0 = vacío; 0x410-0x413 botones de aplicación, 0x420 la radio);
 *   Notify(0x80) cuando hay algo en el anillo.
 * Respuesta 0x80000000 = no está.
 *
 * Qué se hace: el brillo (si no hay ACPI Video, que manda), las teclas de la
 * radio (modo avión, acoes.c) y su luz; los botones de aplicación solo al
 * registro. Al arrancar se vacía el anillo (botones viejos), como Linux. */
#include "uacpi_sagui.h"
#include "acoes.h"
#include "brilho_conv.h"
#include "console.h"
#include "texto.h"

#define F_FLAGS        0x1000
#define F_BOTOES       0x1002
#define NAO_EXISTE     0x80000000u
#define FLAG_RADIO     (1u << 5)
#define FLAG_TACTIL    (1u << 26)
#define FLAG_MICRO     (1u << 29)
#define FLAG_BLANDAS   (FLAG_RADIO | FLAG_TACTIL | FLAG_MICRO)
#define TEM_LUZ_RADIO  (1u << 17)
#define LUZ_RADIO      (1u << 5)
#define ANEL_MAX       16

static uacpi_namespace_node *bl, *ext;
static int bl_total;                 /* niveles del FUJ02B1 */
static int bl_nivel = -1;            /* el último que se leyó o se puso */
static bool usa_sbl2;
static uint32_t funcoes, estado;
static char nome_bl[40], nome_ext[40];

/* ------------------------------------------------------------ FUJ02B1 */

static int bl_ler(void)
{
    uint64_t v;
    if (!bl || portatil_eval_int(bl, "GBLL", NULL, 0, &v) != UACPI_STATUS_OK) return -1;
    int n = (int)(v & 0x0FFFFFFF);
    return n < bl_total ? n : -1;
}

static bool bl_fixar(int n)
{
    if (!bl || n < 0 || n >= bl_total) return false;
    uint64_t a = (uint64_t)n;
    uacpi_object *o = NULL;
    uacpi_status r = portatil_eval_ints(bl, usa_sbl2 ? "SBL2" : "SBLL", &a, 1, &o);
    if (o) uacpi_object_unref(o);
    if (r == UACPI_STATUS_OK) bl_nivel = n;
    return r == UACPI_STATUS_OK;
}

static int bl_pct(int n) { return brilho_pct_linear(n, bl_total); }
static uint32_t bl_valor(int n) { return (uint32_t)n; }

static void bl_aviso(uint32_t v)
{
    if (v != 0x80) { klog("[FUJITSU] FUJ02B1: aviso %x desconocido\n", v); return; }
    int antes = bl_nivel, agora = bl_ler();
    if (agora < 0 || agora == antes) return;
    bl_fixar(agora);   /* confirmar lo que puso la tecla */
    klog("[FUJITSU] tecla de brillo: nivel %d -> %d de %d\n", antes, agora, bl_total);
    if (portatil_brilho_fonte() == PORTATIL_FONTE_FUJITSU)
        portatil_brilho_sincronizar();   /* ya está puesto: solo el aviso */
    else
        acao_sistema(antes >= 0 && agora < antes ? ACAO_BRILHO_MENOS : ACAO_BRILHO_MAIS, ORIGEM_ACAO_FUJITSU);
}

/* ------------------------------------------------------------ FUJ02E3 */

static uint32_t func(uint32_t f, uint32_t op, uint32_t campo, uint32_t valor)
{
    uint64_t a[4] = { f, op, campo, valor };
    uint64_t r;
    if (!ext || portatil_eval_int(ext, "FUNC", a, 4, &r) != UACPI_STATUS_OK) return NAO_EXISTE;
    return (uint32_t)r;
}

static void botao(uint32_t irb)
{
    uint32_t c = irb & 0x4FF;
    if (c == 0x420) acao_sistema(ACAO_RADIO, ORIGEM_ACAO_FUJITSU);
    else if (c >= 0x410 && c <= 0x413) klog("[FUJITSU] boton de aplicacion %u\n", c - 0x410 + 1);
    else if (c) klog("[FUJITSU] boton desconocido %x\n", irb);
}

static void ext_aviso(uint32_t v)
{
    if (v != 0x80) { klog("[FUJITSU] FUJ02E3: aviso %x desconocido\n", v); return; }
    if (funcoes) estado = func(F_FLAGS, 4, 0, 0);
    for (int i = 0; i < ANEL_MAX; i++) {
        uint32_t irb = func(F_BOTOES, 1, 0, 0);
        if (irb == 0 || irb == NAO_EXISTE) break;
        botao(irb);
    }
    if (funcoes & FLAG_BLANDAS) {
        uint32_t b = func(F_FLAGS, 1, 0, 0);
        if (b != NAO_EXISTE && (b & funcoes & FLAG_RADIO)) acao_sistema(ACAO_RADIO, ORIGEM_ACAO_FUJITSU);
        if (b != NAO_EXISTE && (b & (FLAG_TACTIL | FLAG_MICRO))) klog("[FUJITSU] tecla blanda %x (sin accion)\n", b);
    }
}

void fujitsu_notificar(int alvo, uint32_t v)
{
    portatil_evento(alvo, v);
    if (alvo == PORTATIL_ALVO_FUJ_BL) { if (bl) bl_aviso(v); }
    else if (ext) ext_aviso(v);
}

static uacpi_status notificacao(uacpi_handle ctx, uacpi_namespace_node *no, uacpi_u64 v)
{
    (void)no;
    fujitsu_notificar((int)(uintptr_t)ctx, (uint32_t)v);
    return UACPI_STATUS_OK;
}

void fujitsu_luz_radio(bool apagada)
{
    if (!ext || !(funcoes & TEM_LUZ_RADIO)) return;
    uint32_t r = func(F_FLAGS, 5, LUZ_RADIO, apagada ? 0 : LUZ_RADIO);
    klog("[FUJITSU] luz de la radio: %s (%x)\n", apagada ? "apagada" : "encendida", r);
}

/* ------------------------------------------------------------ búsqueda */

static uacpi_iteration_decision achar(void *u, uacpi_namespace_node *no, uacpi_u32 prof)
{
    (void)u; (void)prof;
    static const char *const id_bl[] = { "FUJ02B1", NULL };
    static const char *const id_ext[] = { "FUJ02E3", NULL };
    bool e_bl = !bl && uacpi_device_matches_pnp_id(no, id_bl);
    bool e_ext = !ext && uacpi_device_matches_pnp_id(no, id_ext);
    if (!e_bl && !e_ext) return UACPI_ITERATION_DECISION_CONTINUE;
    uint32_t sta = 0xF;
    if (uacpi_eval_sta(no, &sta) == UACPI_STATUS_OK && !(sta & 1)) return UACPI_ITERATION_DECISION_CONTINUE;
    if (e_bl) bl = no;
    else ext = no;
    return UACPI_ITERATION_DECISION_CONTINUE;
}

void fujitsu_iniciar(void)
{
    uacpi_namespace_for_each_child(uacpi_namespace_root(), achar, NULL, UACPI_OBJECT_DEVICE_BIT,
                                   UACPI_MAX_DEPTH_ANY, NULL);
    if (bl) {
        portatil_nome(bl, nome_bl, sizeof nome_bl);
        uint64_t t = 0;
        if (portatil_eval_int(bl, "RBLL", NULL, 0, &t) == UACPI_STATUS_OK && t >= 2 && t <= BRILHO_MAX_NIVEIS)
            bl_total = (int)t;
        usa_sbl2 = portatil_tem_metodo(bl, "SBL2");
        bl_nivel = bl_total ? bl_ler() : -1;
        uacpi_install_notify_handler(bl, notificacao, (uacpi_handle)(uintptr_t)PORTATIL_ALVO_FUJ_BL);
        klog("[FUJITSU] FUJ02B1 %s: %d niveles, nivel %d%s\n", nome_bl, bl_total, bl_nivel, usa_sbl2 ? " (SBL2)" : "");
        if (bl_total && portatil_tem_metodo(bl, "GBLL") && (portatil_tem_metodo(bl, "SBLL") || usa_sbl2)) {
            brilho_drv_t d = { PORTATIL_FONTE_FUJITSU, bl_total, bl_ler, bl_fixar, bl_pct, bl_valor, nome_bl };
            portatil_registrar_brilho(&d);
        }
    }
    if (ext) {
        portatil_nome(ext, nome_ext, sizeof nome_ext);
        if (!portatil_tem_metodo(ext, "FUNC")) {
            klog("[FUJITSU] FUJ02E3 %s sin FUNC: no se usa\n", nome_ext);
            ext = NULL;
        } else {
            funcoes = func(F_FLAGS, 0, 0, 0);
            if (funcoes == NAO_EXISTE) funcoes = 0;
            if (funcoes) estado = func(F_FLAGS, 4, 0, 0);
            uint32_t botoes = func(F_BOTOES, 0, 0, 0);
            int viejos = 0;
            while (viejos < ANEL_MAX) {
                uint32_t r = func(F_BOTOES, 1, 0, 0);
                if (r == 0 || r == NAO_EXISTE) break;
                viejos++;
            }
            uacpi_install_notify_handler(ext, notificacao, (uacpi_handle)(uintptr_t)PORTATIL_ALVO_FUJ_EXT);
            klog("[FUJITSU] FUJ02E3 %s: funciones %x, estado %x, botones %x%s (%d viejos en el anillo)\n", nome_ext,
                 funcoes, estado, botoes, funcoes & TEM_LUZ_RADIO ? ", luz de la radio" : "", viejos);
        }
    }
    if (!bl && !ext) klog("[FUJITSU] no hay dispositivos de Fujitsu (FUJ02B1/FUJ02E3)\n");
}

void fujitsu_info(info_portatil_t *p)
{
    p->fujitsu = (bl ? 1u : 0u) | (ext ? 2u : 0u);
    p->fujitsu_funcoes = funcoes;
    char t[40];
    t[0] = 0;
    if (bl) txt_copia_n(t, nome_bl, sizeof t);
    else if (ext) txt_copia_n(t, nome_ext, sizeof t);
    txt_copia_n(p->fujitsu_no, t, sizeof p->fujitsu_no);
}
