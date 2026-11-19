// SPDX-License-Identifier: GPL-2.0-or-later
/* portatil.c - lo propio de los portátiles por ACPI: el brillo, los avisos del
 * firmware, SIS_PORTATIL y el volcado de las tablas (D403).
 *
 * Reparto:
 *   video_acpi.c  el ACPI Video estándar (casi todas las marcas): dispositivos de
 *                 vídeo con _DOD/_DOS, brillo por _BCL/_BCM/_BQC y los avisos de
 *                 las teclas de brillo (0x86/0x87) y de pantalla (0x80);
 *   fujitsu.c     FUJ02B1 (brillo de Fujitsu) y FUJ02E3 (botones y luces);
 *   toshiba.c     TOS1900/TOS620x (la interfaz HCI de Toshiba: teclas Fn, brillo,
 *                 ventilador, interruptor de la radio);
 *   portatil.c    esto: elige el control del brillo, atiende los pedidos que
 *                 llegan de fuera (la orden `brillo`, las teclas, las pruebas) y
 *                 publica el estado para `portatil`.
 *
 * Todo lo que llama al intérprete corre en la tarea "acpi-aml" (uacpi_sagui.c),
 * la única que puede: desde fuera solo se deja un pedido y se despierta la
 * tarea. Las teclas se convierten en acciones comunes (acoes.c), que son las
 * mismas para el teclado, para cada marca y para la orden.
 *
 * Seguridad: sin --uacpi no existe la tarea y aquí no se hace nada (las órdenes
 * dicen por qué). No se cambia nada del firmware al arrancar salvo lo que hace
 * Linux con estos mismos dispositivos (_DOS(0), ENAB y activar las teclas de
 * Toshiba); el brillo solo cambia cuando lo pide alguien. */
#include "uacpi_sagui.h"
#include "portatil.h"
#include "acoes.h"
#include "bateria.h"
#include "brilho_conv.h"
#include "console.h"
#include "cpu.h"
#include "gui.h"
#include "info_hardware.h"
#include "memoria.h"
#include "paginacao.h"
#include "processo.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"

#define NOTIF_FILA      8
#define ESPERA_MAX_MS   3000
#define PERIODO_INFO    (5 * TIQUES_POR_SEGUNDO)

static brilho_drv_t drv;
static bool tem_drv;
static int brilho_idx = -1;           /* el último nivel conocido (leído o puesto) */
static volatile bool iniciado;        /* portatil_iniciar() terminó: se aceptan pedidos */

/* Un pedido de brillo a la vez; los pasos seguidos se suman. */
static volatile bool ped_tem;
static volatile int ped_modo, ped_valor, ped_origem;
static volatile uint32_t ped_seq, feito_seq;
static volatile int32_t resultado;
static const char canal_feito = 0;

/* Avisos simulados (PORTATIL_NOTIFICAR) y la luz de la radio. */
static struct { uint8_t alvo; uint32_t valor; } notif[NOTIF_FILA];
static volatile uint32_t n_ini, n_fim;
static volatile int radio_ped = -1;

static uint32_t eventos, ultimo_evento;
static uint32_t ultima_info;
static info_portatil_t pub = { .versao = 1, .brilho_pct = -1, .toshiba_ventilador = -1, .toshiba_radio = -1 };

/* ------------------------------------------------------------ ayudas para los drivers */

bool portatil_tem_metodo(uacpi_namespace_node *no, const char *nome)
{
    uacpi_namespace_node *m = NULL;
    if (uacpi_namespace_node_find(no, nome, &m) != UACPI_STATUS_OK || !m) return false;
    uacpi_object_type t;
    return uacpi_namespace_node_type(m, &t) == UACPI_STATUS_OK && t == UACPI_OBJECT_METHOD;
}

uacpi_status portatil_eval_ints(uacpi_namespace_node *no, const char *nome, const uint64_t *args, int n,
                                uacpi_object **ret)
{
    uacpi_object *obj[6];
    if (n > 6) return UACPI_STATUS_INVALID_ARGUMENT;
    int k;
    for (k = 0; k < n; k++) {
        obj[k] = uacpi_object_create_integer(args[k]);
        if (!obj[k]) break;
    }
    uacpi_status r = UACPI_STATUS_OUT_OF_MEMORY;
    if (k == n) {
        uacpi_object_array a = { obj, (uacpi_size)n };
        r = uacpi_eval(no, nome, n ? &a : NULL, ret);
    }
    while (k > 0) uacpi_object_unref(obj[--k]);
    return r;
}

uacpi_status portatil_eval_int(uacpi_namespace_node *no, const char *nome, const uint64_t *args, int n, uint64_t *v)
{
    uacpi_object *o = NULL;
    uacpi_status r = portatil_eval_ints(no, nome, args, n, &o);
    if (r == UACPI_STATUS_OK) {
        uacpi_u64 x = 0;
        if (!o || uacpi_object_get_integer(o, &x) != UACPI_STATUS_OK) r = UACPI_STATUS_TYPE_MISMATCH;
        else *v = x;
    }
    if (o) uacpi_object_unref(o);
    return r;
}

void portatil_nome(uacpi_namespace_node *no, char *dst, uint32_t max)
{
    const char *n = uacpi_namespace_node_generate_absolute_path(no);
    txt_copia_n(dst, n ? n : "?", max);
    if (n) uacpi_free_absolute_path(n);
}

/* ------------------------------------------------------------ estado publicado */

static void publicar(void)
{
    info_portatil_t n = pub;
    info_bateria_t b;
    bateria_estado(&b);
    n.uacpi = b.uacpi;
    n.brilho_fonte = tem_drv ? (uint32_t)drv.fonte : PORTATIL_FONTE_NADA;
    n.brilho_niveis = tem_drv ? (uint32_t)drv.niveis : 0;
    n.brilho_nivel = brilho_idx >= 0 ? (uint32_t)brilho_idx : 0;
    n.brilho_pct = tem_drv && brilho_idx >= 0 ? drv.pct(brilho_idx) : -1;
    n.brilho_valor = tem_drv && brilho_idx >= 0 ? drv.valor(brilho_idx) : 0;
    n.brilho_min = tem_drv ? drv.valor(0) : 0;
    n.brilho_max = tem_drv ? drv.valor(drv.niveis - 1) : 0;
    video_acpi_info(&n);
    fujitsu_info(&n);
    toshiba_info(&n);
    n.radio_apagada = acoes_radio_apagada() ? 1 : 0;
    n.eventos = eventos;
    n.ultimo_evento = ultimo_evento;
    n.acoes = acoes_feitas();
    n.ultima_acao = acoes_ultima();
    if (tem_drv) n.motivo[0] = 0;
    else txt_copia_n(n.motivo, "el ACPI de este equipo no tiene control del brillo", sizeof n.motivo);
    bool i0 = int_salva_desliga();
    pub = n;
    int_restaura(i0);
    ultima_info = relogio_tiques();
}

void portatil_evento(int alvo, uint32_t valor)
{
    eventos++;
    ultimo_evento = (uint32_t)alvo << 16 | (valor & 0xFFFF);
}

/* ------------------------------------------------------------ brillo */

void portatil_registrar_brilho(const brilho_drv_t *d)
{
    if (tem_drv || !d || d->niveis < 2) return;
    drv = *d;
    tem_drv = true;
}

int portatil_brilho_fonte(void) { return tem_drv ? drv.fonte : PORTATIL_FONTE_NADA; }

static int indice_de_pct(int pct)
{
    int melhor = 0, dmin = 1000;
    for (int i = 0; i < drv.niveis; i++) {
        int p = drv.pct(i), d = p > pct ? p - pct : pct - p;
        if (d < dmin) { dmin = d; melhor = i; }
    }
    return melhor;
}

/* Hace el cambio y deja el aviso. Para las teclas del ACPI Video y de Fujitsu:
 * muchas BIOS (sobre todo las de la época de Windows XP) cambian el brillo
 * ellas mismas y además avisan (0x86/0x87, o el 0x80 del FUJ02B1); si el nivel
 * ya no es el que dejamos, lo hizo el firmware y aquí solo se avisa (si no,
 * subiría dos pasos). */
static int32_t aplicar(int modo, int valor, int origem)
{
    if (!tem_drv) return ERRO_NAO_EXISTE;
    int cur = drv.ler();
    int novo;
    if (modo == PORTATIL_BRILHO_PASSO) {
        if ((origem == ORIGEM_ACAO_VIDEO || origem == ORIGEM_ACAO_FUJITSU) && cur >= 0 && brilho_idx >= 0 && cur != brilho_idx) {
            klog("[PORTATIL] el firmware ya cambio el brillo (nivel %d -> %d)\n", brilho_idx, cur);
            brilho_idx = cur;
            acoes_osd(OSD_BRILHO, drv.pct(cur));
            return drv.pct(cur);
        }
        novo = brilho_passo(drv.niveis, cur >= 0 ? cur : brilho_idx, valor);
    } else if (modo == PORTATIL_BRILHO_FIXAR) {
        novo = indice_de_pct(valor);
    } else {
        return brilho_idx >= 0 ? drv.pct(brilho_idx) : ERRO_NAO_EXISTE;
    }
    if (novo < 0) return ERRO_INVALIDO;
    if (!drv.fixar(novo)) {
        klog("[PORTATIL] no se pudo poner el nivel %d de %s\n", novo, drv.nome);
        return ERRO_NAO_SUPORTADO;
    }
    int lido = drv.ler();
    brilho_idx = lido >= 0 ? lido : novo;
    int pct = drv.pct(brilho_idx);
    klog("[PORTATIL] brillo: nivel %d de %d (%d %%, valor %u)%s\n", brilho_idx + 1, drv.niveis, pct,
         drv.valor(brilho_idx), lido >= 0 && lido != novo ? " (el firmware dejo otro)" : "");
    acoes_osd(OSD_BRILHO, pct);
    return pct;
}

void portatil_brilho_sincronizar(void)
{
    if (!tem_drv) return;
    int cur = drv.ler();
    if (cur >= 0) brilho_idx = cur;
    if (brilho_idx >= 0) acoes_osd(OSD_BRILHO, drv.pct(brilho_idx));
    publicar();
}

int32_t portatil_brilho_pedir(int modo, int valor, int origem)
{
    if (!iniciado || !tem_drv) return ERRO_NAO_EXISTE;
    bool i0 = int_salva_desliga();
    if (ped_tem && ped_modo == PORTATIL_BRILHO_PASSO && modo == PORTATIL_BRILHO_PASSO) {
        ped_valor += valor;
    } else {
        ped_modo = modo;
        ped_valor = valor;
    }
    ped_origem = origem;
    ped_tem = true;
    uint32_t s = ++ped_seq;
    int_restaura(i0);
    uacpi_sagui_acordar();
    return (int32_t)(s & 0x7FFFFFFF);
}

void portatil_radio_mudou(bool apagada)
{
    radio_ped = apagada ? 1 : 0;
    uacpi_sagui_acordar();
}

/* ------------------------------------------------------------ la tarea */

void portatil_iniciar(void)
{
    video_acpi_iniciar();
    fujitsu_iniciar();
    toshiba_iniciar();
    if (tem_drv) {
        brilho_idx = drv.ler();
        klog("[PORTATIL] brillo por %s: %d niveles (%u-%u), ahora %d %%\n", drv.nome, drv.niveis, drv.valor(0),
             drv.valor(drv.niveis - 1), brilho_idx >= 0 ? drv.pct(brilho_idx) : -1);
    } else {
        klog("[PORTATIL] no hay control del brillo por ACPI\n");
    }
    publicar();
    iniciado = true;
}

static void despachar(int alvo, uint32_t valor)
{
    switch (alvo) {
    case PORTATIL_ALVO_VIDEO:
    case PORTATIL_ALVO_BUS:     video_acpi_notificar(alvo, valor); break;
    case PORTATIL_ALVO_FUJ_BL:
    case PORTATIL_ALVO_FUJ_EXT: fujitsu_notificar(alvo, valor); break;
    case PORTATIL_ALVO_TOSHIBA: toshiba_notificar(valor); break;
    }
}

bool portatil_pendente(void) { return iniciado && (ped_tem || n_ini != n_fim || radio_ped >= 0); }

void portatil_ciclo(void)
{
    if (!iniciado) return;
    bool mudou = false;
    for (int k = 0; k < NOTIF_FILA; k++) {
        bool i0 = int_salva_desliga();
        if (n_ini == n_fim) { int_restaura(i0); break; }
        int alvo = notif[n_ini % NOTIF_FILA].alvo;
        uint32_t valor = notif[n_ini % NOTIF_FILA].valor;
        n_ini++;
        int_restaura(i0);
        klog("[PORTATIL] aviso simulado: destino %d, valor %x\n", alvo, valor);
        despachar(alvo, valor);
        mudou = true;
    }
    if (ped_tem) {
        bool i0 = int_salva_desliga();
        int modo = ped_modo, valor = ped_valor, origem = ped_origem;
        uint32_t s = ped_seq;
        ped_tem = false;
        int_restaura(i0);
        int32_t r = aplicar(modo, valor, origem);
        i0 = int_salva_desliga();
        resultado = r;
        feito_seq = s;
        acordar(&canal_feito);
        int_restaura(i0);
        mudou = true;
    }
    int rp = radio_ped;
    if (rp >= 0) {
        radio_ped = -1;
        fujitsu_luz_radio(rp != 0);
        mudou = true;
    }
    toshiba_ciclo();
    if (mudou || relogio_tiques() - ultima_info >= PERIODO_INFO) publicar();
}

/* ------------------------------------------------------------ tablas ACPI (PORTATIL_TABELA) */

/* Copia los bytes de una tabla (de la lista de INFO_ACPI) por una ventana de
 * una página: solo lectura, y sirve sin --uacpi (para mandar el volcado de una
 * máquina real). Una copia a la vez. */
static volatile bool copiando;
static uint32_t janela;
static uint8_t pagina[PAGINA];

static int32_t copiar_tabela(uint32_t indice, uint32_t buf, uint32_t n)
{
    const info_acpi_t *a = hw_acpi();
    if (!a || !a->achou) return ERRO_NAO_EXISTE;
    if (indice >= a->qtd) return ERRO_INVALIDO;
    const info_acpi_tabela_t *t = &a->tabela[indice];
    if (t->tamanho < 36 || t->tamanho > (4u << 20)) return ERRO_FORMATO;
    uint32_t total = t->tamanho, copiar = n < total ? n : total;
    if (copiar && !usuario_ok((void *)buf, copiar, true)) return ERRO_ENDERECO;
    bool i0 = int_salva_desliga();
    if (copiando) { int_restaura(i0); return ERRO_OCUPADO; }
    copiando = true;
    if (!janela) janela = janela_reservar(PAGINA);
    int_restaura(i0);
    if (!janela) { copiando = false; return ERRO_SEM_MEMORIA; }
    uint32_t feito = 0;
    while (feito < copiar) {
        uint32_t fis = t->endereco + feito;
        uint32_t desl = fis & (PAGINA - 1);
        uint32_t pedaco = PAGINA - desl;
        if (pedaco > copiar - feito) pedaco = copiar - feito;
        i0 = int_salva_desliga();
        janela_pagina(janela, fis & ~(PAGINA - 1), 0);
        memcpy(pagina, (const void *)(uintptr_t)(janela + desl), pedaco);
        janela_soltar(janela);
        int_restaura(i0);
        memcpy((void *)(buf + feito), pagina, pedaco);
        feito += pedaco;
    }
    copiando = false;
    return (int32_t)total;
}

/* ------------------------------------------------------------ SIS_PORTATIL */

static int32_t esperar_brilho(int32_t seq)
{
    if (seq < 0) return seq;
    for (uint32_t ms = 0; ms < ESPERA_MAX_MS; ms += 10) {
        if ((int32_t)(feito_seq - (uint32_t)seq) >= 0) return resultado;
        uacpi_sagui_dormir_ms(10);
    }
    return ERRO_OCUPADO;
}

int32_t sis_portatil(uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    (void)d;
    switch (op) {
    case PORTATIL_ESTADO: {
        if (b < sizeof(info_portatil_t) || !usuario_ok((void *)a, sizeof(info_portatil_t), true)) return ERRO_ENDERECO;
        info_portatil_t v;
        bool i0 = int_salva_desliga();
        v = pub;
        int_restaura(i0);
        info_bateria_t bt;
        bateria_estado(&bt);
        v.uacpi = bt.uacpi;   /* el de ahora: se publica antes de que la tarea diga «activo» */
        if (!iniciado) {
            txt_copia_n(v.motivo, bt.uacpi == UACPI_ATIVO || bt.uacpi == UACPI_INICIANDO ? "iniciando" :
                        bt.motivo[0] ? bt.motivo : "sin el intérprete ACPI", sizeof v.motivo);
        }
        memcpy((void *)a, &v, sizeof v);
        return 0;
    }
    case PORTATIL_BRILHO:
        if (a == PORTATIL_BRILHO_LER) return iniciado && tem_drv && pub.brilho_pct >= 0 ? pub.brilho_pct : ERRO_NAO_EXISTE;
        if (a == PORTATIL_BRILHO_FIXAR && b > 100) return ERRO_INVALIDO;
        if (a != PORTATIL_BRILHO_FIXAR && a != PORTATIL_BRILHO_PASSO) return ERRO_INVALIDO;
        return esperar_brilho(portatil_brilho_pedir((int)a, (int)b, ORIGEM_ACAO_ORDEN));
    case PORTATIL_NOTIFICAR: {
        if (!iniciado) return ERRO_NAO_EXISTE;
        if (a < PORTATIL_ALVO_VIDEO || a > PORTATIL_ALVO_TOSHIBA || b > 0xFFFF) return ERRO_INVALIDO;
        bool i0 = int_salva_desliga();
        if (n_fim - n_ini >= NOTIF_FILA) { int_restaura(i0); return ERRO_OCUPADO; }
        notif[n_fim % NOTIF_FILA].alvo = (uint8_t)a;
        notif[n_fim % NOTIF_FILA].valor = b;
        n_fim++;
        int_restaura(i0);
        uacpi_sagui_acordar();
        return 0;
    }
    case PORTATIL_TABELA:
        return copiar_tabela(a, b, c);
    case PORTATIL_ACAO:
        if (a == 0 || a >= ACAO_MAX) return ERRO_INVALIDO;
        acao_sistema((int)a, ORIGEM_ACAO_ORDEN);
        if (!gui_ativa()) acoes_atender();   /* sin escritorio no hay quien la recoja enseguida */
        return 0;
    }
    return ERRO_INVALIDO;
}
