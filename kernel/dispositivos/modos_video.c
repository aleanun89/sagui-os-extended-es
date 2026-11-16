// SPDX-License-Identifier: GPL-2.0-or-later
/* modos_video.c - lista de modos, cambio en caliente y resolución preferida. Ver D058 y D059.
 *
 * Tres fuentes de modos:
 *   - la lista del VBE que la etapa 2 guardó (boot_info, VBE_MODOS): es lo que
 *     la BIOS sabe activar y, por tanto, lo que vale para el próximo arranque;
 *   - con la tarjeta de Bochs/QEMU (bga.c), cualquier anchura múltiplo de 8 hasta
 *     su máximo: la lista añade las resoluciones comunes que caben;
 *   - el modo actual, si no está en ninguna de las dos.
 *
 * El cambio en caliente (SIS_MUDAR_MODO con MUDAR_AGORA) rehace, en este orden, el
 * vídeo (búfer de atrás), el terminal del kernel, el mouse, el fondo del área de
 * trabajo y las ventanas, y mide de nuevo la copia a la tarjeta (rapido.c).
 * Todo con las interrupciones desactivadas: la IRQ del mouse dibuja el cursor y la
 * tarea de las ventanas recompone la pantalla.
 *
 * La resolución preferida vive en la cabecera de arranque (LBA 1, CAB_RES_LARG y
 * CAB_RES_ALT), que la etapa 2 lee antes de elegir el modo: es el único
 * lugar que el cargador alcanza sin sistema de archivos. MUDAR_GUARDAR
 * regraba ese sector en el disco de arranque, y solo él (D058). */
#include "modos_video.h"
#include "abi.h"
#include "bga.h"
#include "bootinfo.h"
#include "console.h"
#include "acel2d.h"
#include "cpu.h"
#include "disco.h"
#include "gui.h"
#include "kconf.h"
#include "info_hardware.h"
#include "memoria.h"
#include "mouse.h"
#include "rapido.h"
#include "sistema.h"
#include "texto.h"
#include "trava.h"
#include "video.h"
#include "wc.h"

#define LISTA_MAX 192   /* los 128 del VBE (15 a 32 bits, D078) + los de la tarjeta */
#define MIN_L 640
#define MIN_A 480

static modo_video_t lista[LISTA_MAX];
static int qtd;

static uint32_t pref_l(void) { return info_boot.res_pref & 0xFFFF; }
static uint32_t pref_a(void) { return info_boot.res_pref >> 16; }

static bool cabe_a_quente(uint32_t l, uint32_t a)
{
    return video_troca_a_quente() && l >= MIN_L && a >= MIN_A && !(l & 7) &&
           l <= bga_max_largura() && a <= bga_max_altura() && l * a * 4 <= video_memoria_mapeada();
}

static int achar(uint32_t l, uint32_t a, uint32_t bpp)
{
    for (int i = 0; i < qtd; i++)
        if (lista[i].largura == l && lista[i].altura == a && (!bpp || lista[i].bpp == bpp)) return i;
    return -1;
}

static void acrescentar(uint32_t l, uint32_t a, uint32_t bpp, uint32_t modo, uint32_t flags)
{
    int i = achar(l, a, bpp);
    if (i >= 0) { lista[i].flags |= (uint8_t)flags; return; }
    if (qtd == LISTA_MAX) return;
    lista[qtd++] = (modo_video_t){ (unsigned short)l, (unsigned short)a, (unsigned short)modo, (unsigned char)bpp,
                                   (unsigned char)flags };
}

/* Rehace la lista (cambian las marcas de actual y preferida). Orden: anchura,
 * después altura, después bits. */
static void montar_lista(void)
{
    static const uint16_t comuns[][2] = {
        { 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1152, 864 }, { 1280, 720 }, { 1280, 768 }, { 1280, 800 },
        { 1280, 960 }, { 1280, 1024 }, { 1360, 768 }, { 1440, 900 }, { 1600, 900 }, { 1600, 1200 },
        { 1680, 1050 }, { 1920, 1080 }, { 1920, 1200 },
    };
    qtd = 0;
    for (uint32_t i = 0; i < info_boot.modos_qtd; i++) {
        modo_vbe_t *m = &modos_vbe[i];
        uint32_t f = MODO_VBE;
        if (m->bpp == video_bpp_a_quente() && cabe_a_quente(m->largura, m->altura)) f |= MODO_A_QUENTE;
        acrescentar(m->largura, m->altura, m->bpp, m->modo, f);
    }
    if (video_troca_a_quente())
        for (uint32_t i = 0; i < sizeof comuns / sizeof comuns[0]; i++)
            if (cabe_a_quente(comuns[i][0], comuns[i][1]))
                acrescentar(comuns[i][0], comuns[i][1], video_bpp_a_quente(), 0, MODO_A_QUENTE);
    if (video_ativo())
        acrescentar(video_prim_largura(), video_prim_altura(), info_boot.vesa_bpp, 0, 0);
    for (int i = 0; i < qtd; i++) {
        modo_video_t *m = &lista[i];
        if (video_ativo() && m->largura == video_prim_largura() && m->altura == video_prim_altura() && m->bpp == info_boot.vesa_bpp)
            m->flags |= MODO_ATUAL;
        if (m->largura == pref_l() && m->altura == pref_a()) m->flags |= MODO_PREFERIDO;
    }
    for (int i = 1; i < qtd; i++)   /* inserción: la lista es pequeña */
        for (int k = i; k > 0; k--) {
            modo_video_t *x = &lista[k - 1], *y = &lista[k];
            uint32_t cx = (uint32_t)x->largura << 20 | (uint32_t)x->altura << 8 | x->bpp;
            uint32_t cy = (uint32_t)y->largura << 20 | (uint32_t)y->altura << 8 | y->bpp;
            if (cx <= cy) break;
            modo_video_t t = *x; *x = *y; *y = t;
        }
}

void modos_iniciar(void)
{
    montar_lista();
    char s[400];
    int n = snformatar(s, sizeof s, "[VIDEO] %u modos VBE:", info_boot.modos_qtd);
    for (uint32_t i = 0; i < info_boot.modos_qtd && n < (int)sizeof s - 20; i++)
        n += snformatar(s + n, sizeof s - (uint32_t)n, " %ux%ux%u", modos_vbe[i].largura, modos_vbe[i].altura, modos_vbe[i].bpp);
    klog("%s\n", s);
    if (info_boot.res_pref)
        klog("[VIDEO] resolucao preferida no cabecalho: %ux%u\n", pref_l(), pref_a());
    if (video_troca_a_quente())
        klog("[VIDEO] troca de modo a quente disponivel (ate %ux%u)\n", bga_max_largura(), bga_max_altura());
}

/* ------------------------------------------------------------ disco de arranque */

/* La partición de SaguiOS (0xDA) del disco de arranque empieza en el LBA 1: el
 * primer sector de ella es la cabecera de arranque. */
static disco_t *particao_de_boot(uint32_t *lba)
{
    /* D179: si se sabe cuál es el disco de arranque de verdad, su LBA 1 (en un disco
     * instalado en un hueco, la partición del SaguiOS no empieza en el LBA 1). D270: en un
     * disco instalado con GPT, el primer sector de la partición de SaguiOS (cab_lba). */
    disco_t *r = disco_arranque_real();
    *lba = 1;
    if (r) { if (r->cab_lba) *lba = r->cab_lba; return r; }
    *lba = 0;
    for (int k = 0; k < MAX_DISCOS; k++) {
        disco_t *d = disco_indice(k);
        if (d && d->presente && d->pai && d->tipo_mbr == 0xDA && d->inicio == 1 && disco_raiz(d)->de_boot)
            return d;
    }
    return NULL;
}

/* D353: una sola escritura de la cabecera a la vez (leer, cambiar y grabar duerme en el driver: dos
 * a la vez, p. ej. «pantalla --guardar» y kconfig, perderían una de las dos). */
static trava_t trava_cab;

/* Regraba solo los 4 bytes de la resolución en la cabecera. Antes comprueba que el
 * sector es la misma cabecera con la que arrancó la máquina (los primeros 128
 * bytes iguales a la copia de 0x7E00, incluidas las sumas del kernel y del
 * ramdisk): ningún otro disco, ni una imagen distinta de SaguiOS, se
 * toca. Es una escritura pedida por la persona (pantalla --guardar), nunca en el
 * arranque; el cerrojo de D040 se abre solo para este sector de la partición del
 * propio SaguiOS, que ningún sistema de archivos usa. D353: por cab_regravar (el
 * cerrojo y la escritura sin diferir). */
static bool pref_mudar(uint8_t *s, void *ctx)
{
    const uint32_t *la = ctx;
    uint8_t antes[4];
    memcpy(antes, s + CAB_RES_LARG, 4);
    s[CAB_RES_LARG] = (uint8_t)la[0]; s[CAB_RES_LARG + 1] = (uint8_t)(la[0] >> 8);
    s[CAB_RES_ALT] = (uint8_t)la[1];  s[CAB_RES_ALT + 1] = (uint8_t)(la[1] >> 8);
    return memcmp(antes, s + CAB_RES_LARG, 4) != 0;
}

static int gravar_preferencia(uint32_t l, uint32_t a)
{
    uint32_t la[2] = { l, a };
    const char *nome;
    int r = cab_regravar(pref_mudar, la, &nome);
    if (r == 0) info_boot.res_pref = l | a << 16;
    klog("[VIDEO] preferencia %ux%u gravada em %s: %d\n", l, a, nome, r);
    return r;
}

/* D353: lee la cabecera del disco de arranque, comprueba que es la misma con la que arrancó la
 * máquina (los 128 primeros bytes, como gravar_preferencia), deja que "mudar" la cambie y, si
 * cambió algo, la graba sin pasar por la escritura diferida de la caché (disco_escrever_ja: solo ese
 * sector, no el bloque de 4 KB con el MBR) y copia el sector en cabecalho_boot (discos.c reconoce el
 * disco de arranque comparando 256 bytes). Nunca desde el CD (D077). Es la escritura que ya hacían «pantalla --guardar» y Configuración (D323). */
int cab_regravar(bool (*mudar)(uint8_t *s, void *ctx), void *ctx, const char **nome)
{
    if (nome) *nome = "?";
    if (info_boot.opcoes & OPC_BOOT_CD) return ERRO_SO_LEITURA;
    uint32_t lba;
    disco_t *p = particao_de_boot(&lba);
    if (!p) return ERRO_NAO_EXISTE;
    if (nome) *nome = p->nome;
    uint8_t *s = kmalloc(512);
    if (!s) return ERRO_SEM_MEMORIA;
    trava_pegar(&trava_cab);
    int r = disco_ler(p, lba, 1, s);
    if (r == 0 && memcmp(s, cabecalho_boot, CAB_RES_LARG) != 0) r = ERRO_FORMATO;
    if (r == 0 && mudar(s, ctx)) {
        bool antes = p->rw_autorizado;
        p->rw_autorizado = true;
        r = disco_escrever_ja(p, lba, 1, s);   /* sin diferir: solo este sector, ya */
        p->rw_autorizado = antes;
        if (r == 0) memcpy(cabecalho_boot, s, 512);
    }
    trava_soltar(&trava_cab);
    kfree(s);
    if (r < 0) klog("[KCONF] no se pudo grabar la cabecera de arranque en %s: %d\n", p->nome, r);
    return r;
}

/* D323: cambia bits de la cabecera de arranque del disco de arranque (lo que Configuración guarda de
 * la aceleración y del disco: solo se puede decidir antes de arrancar el kernel). Solo los bytes y
 * bits de la lista; igual que la resolución, solo en el disco de arranque (se comprueba que su
 * cabecera es la que arrancó) y nunca desde el CD. pedido = CAB_OPC_PEDIDO(desplazamiento, máscara, valor).
 * D353: si el byte cambia, se olvida lo que kconfig guardó para esos bits (manda lo último pedido). */
typedef struct { uint32_t desl, mascara, valor; } cab_opc_t;

static bool cab_opc_mudar(uint8_t *s, void *ctx)
{
    cab_opc_t *c = ctx;
    uint8_t nuevo = (uint8_t)((s[c->desl] & ~c->mascara) | c->valor);
    if (nuevo == s[c->desl]) return false;
    s[c->desl] = nuevo;
    kconf_esquecer_bits(s, c->desl, c->mascara);
    return true;
}

int32_t cab_opc_gravar(uint32_t pedido)
{
    cab_opc_t c = { pedido & 0xFF, (pedido >> 8) & 0xFF, (pedido >> 16) & 0xFF };
    uint32_t permitida = c.desl == CAB_ACEL2D ? ACEL2D_MODO_MASCARA
                       : c.desl == CAB_GPU3D ? 0x07
                       : c.desl == CAB_DISCO_OPC ? (CAB_DISCO_SEM_DMA | CAB_DISCO_SEM_CACHE | CAB_DISCO_SEM_DIFERIDA) : 0;
    if (!permitida || !c.mascara || (c.mascara & ~permitida) || (c.valor & ~c.mascara)) return ERRO_INVALIDO;
    const char *nome;
    int r = cab_regravar(cab_opc_mudar, &c, &nome);
    klog("[VIDEO] cabecera %u &%02x=%02x grabada en %s: %d\n", c.desl, c.mascara, c.valor, nome, r);
    return r;
}

/* ------------------------------------------------------------ cambio en caliente */

static int trocar(uint32_t l, uint32_t a)
{
    bool i = int_salva_desliga();
    if (!video_trocar_modo(l, a)) { int_restaura(i); return ERRO_SEM_MEMORIA; }
    console_tela_mudou();
    mouse_tela_mudou((int)video_largura(), (int)video_altura());   /* D290: el escritorio, no solo esta pantalla */
    if (gui_ativa()) {
        area_tela_mudou();
        gui_tela_mudou();
    } else {
        video_mostrar_tudo();
    }
    uint32_t modo = 0;
    for (uint32_t k = 0; k < info_boot.modos_qtd; k++)
        if (modos_vbe[k].largura == l && modos_vbe[k].altura == a && modos_vbe[k].bpp == video_bpp()) modo = modos_vbe[k].modo;
    info_boot.vesa_modo = modo;
    hw_video_atualizar(modo);
    rapido_medir_tela();
    int_restaura(i);
    return 0;
}

/* La misma lista, para el kernel (el submenú "Resolución" del área de
 * trabajo, D066). */
int modos_lista(modo_video_t *v, int n)
{
    montar_lista();
    if (n > qtd) n = qtd;
    memcpy(v, lista, (uint32_t)n * sizeof(modo_video_t));
    return n;
}

/* SIS_MODOS_VIDEO (modo_video_t[], n) -> cuántos copió. */
int32_t sis_modos_video(uint32_t pv, uint32_t n, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    montar_lista();
    if (n > (uint32_t)qtd) n = (uint32_t)qtd;
    if (!usuario_ok((void *)pv, n * sizeof(modo_video_t), true)) return ERRO_ENDERECO;
    memcpy((void *)pv, lista, n * sizeof(modo_video_t));
    return (int32_t)n;
}

/* SIS_MUDAR_MODO (largura | altura << 16, MUDAR_AGORA | MUDAR_GUARDAR).
 * Guardar 0x0 borra la preferencia (vuelve a la elección automática). Devuelve lo
 * que se hizo (MUDAR_*). */
int32_t sis_mudar_modo(uint32_t dims, uint32_t op, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    uint32_t l = dims & 0xFFFF, a = dims >> 16;
    if (!op || (op & ~(uint32_t)(MUDAR_AGORA | MUDAR_GUARDAR))) return ERRO_INVALIDO;
    montar_lista();
    bool zero = !l && !a;
    if (zero && (op & MUDAR_AGORA)) return ERRO_INVALIDO;
    if (!zero && achar(l, a, 0) < 0 && !cabe_a_quente(l, a)) return ERRO_INVALIDO;
    int32_t feito = 0;
    if (op & MUDAR_AGORA) {
        if (!video_troca_a_quente()) return ERRO_ACESSO;
        if (!cabe_a_quente(l, a)) return ERRO_INVALIDO;
        int r = trocar(l, a);
        if (r < 0) return r;
        feito |= MUDAR_AGORA;
    }
    if (op & MUDAR_GUARDAR) {
        /* Solo lo que ofrece el VBE: el cargador no tiene otra forma de activar. */
        bool do_vbe = zero;
        for (uint32_t k = 0; k < info_boot.modos_qtd; k++)
            if (modos_vbe[k].largura == l && modos_vbe[k].altura == a) do_vbe = true;
        int r = do_vbe ? gravar_preferencia(l, a) : ERRO_INVALIDO;
        if (r < 0) return feito ? feito : r;   /* cambió, pero no grabó: el programa comprueba el bit */
        feito |= MUDAR_GUARDAR;
    }
    return feito;
}

/* INFO_TELA */
int32_t modos_info_tela(uint32_t ptr)
{
    info_tela_t *t = (info_tela_t *)ptr;
    if (!usuario_ok(t, sizeof *t, true)) return ERRO_ENDERECO;
    memset(t, 0, sizeof *t);
    if (video_ativo()) {
        t->largura = video_prim_largura();   /* D290: el modo de la pantalla del arranque */
        t->altura = video_prim_altura();
        t->bpp = info_boot.vesa_bpp;
        t->pitch = video_pitch();
        t->modo = info_boot.vesa_modo;
        t->memoria_kb = video_memoria_mapeada() >> 10;
    }
    t->a_quente = video_troca_a_quente();
    if (t->a_quente) { t->max_largura = bga_max_largura(); t->max_altura = bga_max_altura(); }
    t->pref_largura = pref_l();
    t->pref_altura = pref_a();
    t->arranque_cd = (info_boot.opcoes & OPC_BOOT_CD) != 0;
    uint32_t lba;
    t->pref_gravavel = !t->arranque_cd && particao_de_boot(&lba) != NULL;
    t->wc = wc_ativo();
    t->wc_mbs = wc_mbs_medido();
    t->fps_tela_cheia = gui_tc_fps();
    txt_copia_n(t->wc_via, wc_via(), sizeof t->wc_via);
    txt_copia_n(t->wc_motivo, wc_motivo(), sizeof t->wc_motivo);
    return 0;
}
