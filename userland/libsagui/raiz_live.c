// SPDX-License-Identifier: GPL-2.0-or-later
/* raiz_live.c - la raíz doble de la live (D388): qué es cada partición montada en «/».
 * Aparte de particoes_tamanho.c para que las pruebas del anfitrión (h318, h213) sigan compilando
 * las particiones sin las llamadas de montajes. */
#include "particoes.h"

/* D388: ¿«/» es una live (la SAGUI-RAIZ de solo lectura con una capa de cambios encima)? El kernel monta
 * entonces «/» con el tipo «live» (montagem.c, la sobreposición) sobre la base (kernel/fs/raiz.c, sobreposicao.c) y la capa
 * persistente queda también como montada en «/». */
bool pt_raiz_live(char *base, char *capa, int max)
{
    info_montagem_t m[16];
    int n = info_sistema(INFO_MONTAGENS, m, sizeof m / sizeof m[0]);
    bool live = false;
    for (int i = 0; i < n; i++)
        if (compara_texto(m[i].ponto, "/") == 0 && compara_texto(m[i].tipo, "live") == 0) {
            live = true;
            if (base && max > 0) copia_texto(base, m[i].dispositivo, (size_t)max);
        }
    if (!live) return false;
    if (capa && max > 0) {
        capa[0] = 0;
        info_disco_t d;
        for (int i = 0; listar_discos(i, &d) == 1; i++)
            if (d.particao && d.montado && compara_texto(d.ponto, "/") == 0 && compara_texto(d.rotulo, "SAGUI-PERSIST") == 0)
                copia_texto(capa, d.nome, (size_t)max);
    }
    return true;
}

const char *pt_papel_raiz(const info_disco_t *d)
{
    if (!d->particao || !d->montado || compara_texto(d->ponto, "/") != 0) return NULL;
    char base[16];
    if (pt_raiz_live(base, NULL, sizeof base)) {
        if (compara_texto(d->nome, base) == 0) return "sistema de serie (base de /, solo lectura)";
        return "tus cambios (capa de /)";
    }
    return "raíz del sistema";
}
