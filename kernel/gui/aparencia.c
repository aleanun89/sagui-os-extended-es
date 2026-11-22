// SPDX-License-Identifier: GPL-2.0-or-later
/* aparencia.c - cor de destaque e fundo escolhidos na Configuración (D112).
 *
 * O destaque (barra de título ativa, linha e botão Inicio da barra de tarefas) é uma só cor; as
 * variações (botão aberto, borda) saem dela. O fundo em imagem chega pronto do programa
 * "configuracion" (SIS_FUNDO): o kernel não decodifica imagens. */
#include "aparencia.h"
#include "gui.h"
#include "sistema.h"
#include "video.h"
#include "texto.h"
#include "esquemas.h"
#include "fontes_sistema.h"
#include "processo.h"

/* D428: el esquema vigente (todo lo que el kernel pinta sale de él) y el de la pantalla de inicio de sesión. El acento
 * de D112 es un campo más (esq.acento): en «SaguiOS» manda y da el color de los títulos activos; en los demás esquemas
 * el esquema define el suyo y elegir otro lo cambia junto con los títulos. */
static esquema_t esq, login;
static bool esq_ok;

/* D428: la fuente del sistema. fonte_sagui (la de siempre, id 0) se guarda al primer cambio y las demás vienen de
 * fontes_sistema.h (Terminus, Terminus negrita y Fixed, todas 8x16 con el Latin-1 entero). */
extern unsigned char fonte_sagui[256][16];
static unsigned char fonte_orig[256][16];
static bool fonte_orig_ok;
static uint32_t fonte_atual;

static void fonte_aplicar(uint32_t f)
{
    if (f > 3) f = 0;
    if (f == fonte_atual) return;
    if (!fonte_orig_ok) { memcpy(fonte_orig, fonte_sagui, sizeof fonte_orig); fonte_orig_ok = true; }
    const unsigned char (*t)[16] = f == 1 ? fonte_sis_terminus : f == 2 ? fonte_sis_terminusb : f == 3 ? fonte_sis_fixed : fonte_orig;
    memcpy(fonte_sagui, t, sizeof fonte_orig);
    fonte_atual = f;
}

const esquema_t *esquema(void)
{
    if (__builtin_expect(!esq_ok, 0)) {
        esq_saguios(&esq, ACENTO_PADRAO);
        esq.versao = 1;
        login = esq;
        esq_ok = true;
    }
    return &esq;
}

uint32_t aparencia_acento(void) { return esquema()->acento; }

/* Lo de la sesión que acaba: vuelve el esquema de la pantalla de inicio de sesión (el de /etc, D428). */
void aparencia_padrao(void)
{
    esquema();
    uint32_t v = esq.versao;
    esq = login;
    esq.versao = v + 1;
    fonte_aplicar(esq.fonte);
}

static void tema_mudou(void)
{
    if (!gui_ativa()) return;
    barra_tema_mudou();
    area_tema_mudou();
    janelas_tema_mudou();   /* avisa a los programas: EV_TEMA */
    gui_redesenhar(0, 0, (int)video_largura(), (int)video_altura());
    gui_mostrar();
}

/* Sanea lo que llega de un programa: colores de 24 bits, banderas 0/1, nombre terminado. */
static void sanear(esquema_t *e)
{
    e->nome[ESQ_NOME_MAX - 1] = 0;
    for (int i = 0; i < ESQ_NC; i++) e->v[i] &= 0xFFFFFF;
    e->escuro = e->escuro != 0; e->barra_propia = e->barra_propia != 0; e->escritorio_liso = e->escritorio_liso != 0;
    if (e->base >= ESQ_QTD) e->base = ESQ_SAGUIOS;
    if (e->fonte > 3) e->fonte = 0;
    if (e->id >= ESQ_QTD && e->id != ESQ_PERSONAL) e->id = ESQ_PERSONAL;
}

/* SIS_ESQUEMA (op, ptr, n) */
int32_t sis_esquema(uint32_t op, uint32_t ptr, uint32_t n, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    esquema();
    if (op == ESQ_OP_VERSAO) return (int32_t)esq.versao;
    if (op == ESQ_OP_LER) {
        if (n > sizeof esq) n = sizeof esq;
        if (!usuario_ok((void *)ptr, n, true)) return ERRO_ENDERECO;
        memcpy((void *)ptr, &esq, n);
        return (int32_t)esq.versao;
    }
    if (op != ESQ_OP_FIJAR && op != ESQ_OP_FIJAR_LOGIN) return ERRO_INVALIDO;
    if (n != sizeof esq) return ERRO_INVALIDO;
    if (!usuario_ok((void *)ptr, n, false)) return ERRO_ENDERECO;
    if (op == ESQ_OP_FIJAR_LOGIN && processo_atual()->uid != 0) return ERRO_ACESSO;
    esquema_t nuevo;
    memcpy(&nuevo, (void *)ptr, sizeof nuevo);
    sanear(&nuevo);
    nuevo.versao = esq.versao + 1;
    bool igual = memcmp(nuevo.nome, esq.nome, sizeof esq.nome) == 0 && memcmp(nuevo.v, esq.v, sizeof esq.v) == 0 &&
                 nuevo.id == esq.id && nuevo.fonte == esq.fonte && nuevo.barra_propia == esq.barra_propia && nuevo.escritorio_liso == esq.escritorio_liso;
    esq = nuevo;
    fonte_aplicar(esq.fonte);
    if (op == ESQ_OP_FIJAR_LOGIN) login = nuevo;
    if (!igual || op == ESQ_OP_FIJAR_LOGIN) tema_mudou();
    return (int32_t)esq.versao;
}

static uint32_t mistura(uint32_t c, uint32_t alvo, int pct)
{
    uint32_t r = 0;
    for (int s = 0; s <= 16; s += 8) {
        int a = (int)((c >> s) & 255), b = (int)((alvo >> s) & 255);
        r |= (uint32_t)(a + (b - a) * pct / 100) << s;
    }
    return r;
}

uint32_t aparencia_escurecer(uint32_t cor, int pct) { return mistura(cor, 0x000000, pct); }
uint32_t aparencia_clarear(uint32_t cor, int pct) { return mistura(cor, 0xFFFFFF, pct); }

/* SIS_ACENTO (rgb ou -1) -> cor atual. Muda e redesenha a tela toda (as barras usam a cor). */
int32_t sis_acento(uint32_t rgb, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    esquema();
    if (rgb != 0xFFFFFFFFu && (rgb & 0xFF000000u) == 0 && rgb != esq.acento) {
        esq.acento = rgb;
        if (esq.id == ESQ_SAGUIOS) { esq.titulo_a1 = esq.titulo_a2 = esq.marco_a = rgb; esq.boton_a = esq_mix(esq_mix(rgb, 0, 25), 0xFFFFFF, 25); }
        else {   /* en los demás esquemas, elegir otro acento recolorea también los títulos: pasa a ser personalizado */
            esq.destaque = rgb;
            esq.titulo_a1 = esq.marco_a = esq.inicio_b = esq_mix(rgb, 0, 15);
            esq.titulo_a2 = esq.inicio_a = rgb;
            esq.boton_a = esq_mix(esq_mix(rgb, 0, 25), 0xFFFFFF, 25);
            esq.id = ESQ_PERSONAL;
        }
        esq.versao++;
        tema_mudou();
    }
    return (int32_t)esq.acento;
}

/* SIS_FUNDO (op, ptr, y, n) */
int32_t sis_fundo(uint32_t op, uint32_t ptr, uint32_t y, uint32_t n, uint32_t e)
{
    (void)e;
    if (!gui_ativa()) return ERRO_ACESSO;
    if (op == FUNDO_PADRAO) { area_fundo_padrao(); area_fundo_fim(); return 0; }
    if (op == FUNDO_FIM) { area_fundo_fim(); return 0; }
    if (op == FUNDO_MARCA_IA) { area_marca_ia(y != 0); return 0; }   /* D380 */
    if (op != FUNDO_LINHAS) return ERRO_INVALIDO;
    uint32_t l = video_largura();
    if (n == 0 || n > 4096 || y > 8192 || !usuario_ok((void *)ptr, n * l * 4, false)) return ERRO_ENDERECO;
    return area_fundo_linhas((const uint32_t *)ptr, (int)y, (int)n);
}

/* INFO_APARENCIA */
int32_t aparencia_info(uint32_t ptr, uint32_t n)
{
    info_aparencia_t i;
    i.acento = esquema()->acento;
    i.fundo_imagem = area_fundo_personalizado() ? 1 : 0;
    i.largura = video_largura();
    i.altura = video_altura();
    if (n > sizeof i) n = sizeof i;
    if (!usuario_ok((void *)ptr, n, true)) return ERRO_ENDERECO;
    memcpy((void *)ptr, &i, n);
    return 0;
}
