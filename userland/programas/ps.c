// SPDX-License-Identifier: GPL-3.0-only
/* ps (processos) - lista los procesos (D097). */
#include "sagui.h"

static const char *AYUDA =
    "uso: ps [-e] [-u] [-n] [-p PID] [-s | -c] [nombre]\n"
    "  -e, -A, -a, -f, -l  (se aceptan; siempre se muestran todos los procesos y todas las columnas)\n"
    "  -u  solo procesos de usuario (oculta los del kernel)   -n  sin cabecera ni resumen\n"
    "  -p PID  solo ese proceso                               nombre  solo los procesos con ese nombre\n"
    "  -s  ordena por memoria (el mayor primero)              -c  ordena por CPU\n"
    "Columnas: PID, PPID, estado, memoria, tiempo de CPU (ms) y nombre.\n";

static info_processo_t v[64];
static int orden;

static int cmp(const void *a, const void *b)
{
    const info_processo_t *x = a, *y = b;
    if (orden == 's') return x->memoria_kb < y->memoria_kb ? 1 : x->memoria_kb > y->memoria_kb ? -1 : 0;
    return x->cpu_ms < y->cpu_ms ? 1 : x->cpu_ms > y->cpu_ms ? -1 : 0;
}

int principal(int argc, char **argv)
{
    static const char *estados[] = { "libre", "listo", "ejecutando", "bloqueado", "zombi" };
    bool solo_usuario = false, sin_cab = false;
    int solo_pid = -1;
    opcoes_t o = { 0 };
    int r, n;
    while ((r = obter_opcao(argc, argv, "eAafluncsp:", &o)) >= 0) {
        switch (r) {
        case 'e': case 'A': case 'a': case 'f': case 'l': break;
        case 'u': solo_usuario = true; break;
        case 'n': sin_cab = true; break;
        case 's': case 'c': orden = r; break;
        case 'p':
            if (!texto_para_numero(o.valor, &solo_pid)) { imprimir_em(ERROS, "ps: %s: PID no válido\n", o.valor); return 2; }
            break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("ps", r, &o);
        }
    }
    const char *nombre = o.indice < argc ? argv[o.indice] : NULL;
    n = info_sistema(INFO_PROCESSOS, v, 64);
    if (n < 0) { imprimir_em(ERROS, "ps: %s\n", nome_erro(n)); return 1; }
    void *ptr[64];
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (solo_usuario && !v[i].usuario) continue;
        if (solo_pid >= 0 && v[i].pid != solo_pid) continue;
        if (nombre && compara_texto(v[i].nome, nombre) != 0) continue;
        ptr[k++] = &v[i];
    }
    if (orden) {
        /* cmp trabaja sobre los propios elementos: ordena punteros con un adaptador */
        for (int i = 1; i < k; i++) {
            void *t = ptr[i];
            int j = i;
            while (j > 0 && cmp(ptr[j - 1], t) > 0) { ptr[j] = ptr[j - 1]; j--; }
            ptr[j] = t;
        }
    }
    if (!sin_cab) imprimir("  PID PPID  ESTADO      MEMORIA   CPU (ms)  NOMBRE\n");
    for (int i = 0; i < k; i++) {
        const info_processo_t *p = ptr[i];
        imprimir("%5d %4d  %-10s %5u KB  %8u  %s%s\n", p->pid, p->pai,
                 p->estado <= 4 ? estados[p->estado] : "?", p->memoria_kb, p->cpu_ms,
                 p->nome, p->usuario ? "" : " (kernel)");
    }
    if (!sin_cab && eh_terminal(SAIDA)) imprimir("%d procesos\n", k);
    if ((solo_pid >= 0 || nombre) && k == 0) return 1;
    return 0;
}
