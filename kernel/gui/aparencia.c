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

static uint32_t acento = ACENTO_PADRAO;

uint32_t aparencia_acento(void) { return acento; }
void aparencia_padrao(void) { acento = ACENTO_PADRAO; }

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
    if (rgb != 0xFFFFFFFFu && (rgb & 0xFF000000u) == 0 && rgb != acento) {
        acento = rgb;
        if (gui_ativa()) {
            gui_redesenhar(0, 0, (int)video_largura(), (int)video_altura());
            gui_mostrar();
        }
    }
    return (int32_t)acento;
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
    i.acento = acento;
    i.fundo_imagem = area_fundo_personalizado() ? 1 : 0;
    i.largura = video_largura();
    i.altura = video_altura();
    if (n > sizeof i) n = sizeof i;
    if (!usuario_ok((void *)ptr, n, true)) return ERRO_ENDERECO;
    memcpy((void *)ptr, &i, n);
    return 0;
}
