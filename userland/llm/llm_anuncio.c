// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_anuncio.c - `llm --anunciar` (D380): si la IA local puede funcionar en este equipo
 * (más de 1,8 GB de RAM y un modelo que el motor sabe ejecutar en /home/compartido/modelos
 * o ~/modelos), pone el logo «IA Powered»
 * abajo a la izquierda del escritorio (SIS_FUNDO, FUNDO_MARCA_IA); si no, lo quita y dice
 * por qué. Lo lanza el servicio «llm» al arrancar (/etc/servicios/llm.conf): si el
 * escritorio aún no está, espera hasta dos minutos a que aparezca. */
#include "llm_programa.h"

#ifdef LLM_ANFITRIAO
int llm_anunciar(const char *modelo)
{
    (void)modelo;
    llm_eprintf("llm: --anunciar solo tiene sentido en SaguiOS\n");
    return 1;
}
#else

static int marca(int ativa)
{
    for (int i = 0; i < 120; i++) {
        int r = fundo_enviar(FUNDO_MARCA_IA, NULL, ativa, 0);
        if (r != ERRO_ACESSO) return r;
        dormir(1000);   /* sin escritorio todavía (o en el modo texto) */
    }
    return ERRO_ACESSO;
}

int llm_anunciar(const char *modelo)
{
    char msg[256], caminho[256];
    if (!llm_ram_suficiente(msg, sizeof msg)) {
        llm_printf("%s\n", msg);
        fundo_enviar(FUNDO_MARCA_IA, NULL, 0, 0);
        return 1;
    }
    if (!modelo) {
        if (llm_modelo_padrao(caminho, sizeof caminho) == 0) {
            llm_printf("La IA local no tiene modelo: bájelo en Configuración > IA o copie un .gguf a /home/compartido/modelos.\n");
            fundo_enviar(FUNDO_MARCA_IA, NULL, 0, 0);
            return 1;
        }
        modelo = caminho;
    }
    llm_modelo_t m;
    if (llm_modelo_abrir(&m, modelo, msg, sizeof msg)) {
        llm_modelo_fechar(&m);
        llm_printf("El modelo no sirve para la IA local: %s\n", msg);
        fundo_enviar(FUNDO_MARCA_IA, NULL, 0, 0);
        return 1;
    }
    llm_modelo_fechar(&m);
    int r = marca(1);
    if (r < 0) { llm_printf("No hay escritorio donde anunciar la IA local.\n"); return 1; }
    llm_printf("IA local activa: %s\n", modelo);
    return 0;
}
#endif
