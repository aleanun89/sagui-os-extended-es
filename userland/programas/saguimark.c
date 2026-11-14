// SPDX-License-Identifier: GPL-2.0-or-later
/* saguimark - el benchmark de SaguiOS en modo texto (D093, mejora 051).
 *
 *   saguimark            modo preciso (9 repeticiones por prueba)
 *   saguimark --rapido   3 repeticiones (unos 15 s en un PC real)
 *   saguimark --solo g1,g2   sólo algunos grupos: enteros, fpu, simd, memoria, 2d, sistema
 *   saguimark --sin-2d       todo menos el grupo Gráficos 2D (no abre ventana)
 *   saguimark --no-guardar   no graba el resultado
 *
 * El motor es el mismo de la pestaña Benchmark de infopc (userland/libsagui/
 * saguimark.c). El resultado va a /home/saguimark-AAAAMMDD-HHMM.txt (o a
 * /tmp, sin /home grabable), con la línea lista para la lista de referencias.
 * Sale con 0 si todo se verificó, 2 si alguna prueba dio ERROR. */
#include "sagui.h"
#include "saguimark.h"

static void progresso(void *ctx, int feito, int total, const sm_prova_t *p)
{
    (void)ctx;
    if (p) imprimir("  [%d/%d] %s: %s...\n", feito + 1, total, sm_nome_grupo(p->grupo), p->nome);
}

static sm_resultado_t r;
static char texto[8192];

int principal(int argc, char **argv)
{
    int modo = SM_PRECISO;
    unsigned grupos = SM_TODOS;
    bool guardar = true, sem_2d = false;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--rapido") == 0 || compara_texto(argv[i], "--rápido") == 0) modo = SM_RAPIDO;
        else if (compara_texto(argv[i], "--preciso") == 0) modo = SM_PRECISO;
        else if (compara_texto(argv[i], "--no-guardar") == 0) guardar = false;
        else if (compara_texto(argv[i], "--sin-2d") == 0) sem_2d = true;
        else if (compara_texto(argv[i], "--solo") == 0 && i + 1 < argc) {
            grupos = 0;
            char lista[64];
            copia_texto(lista, argv[++i], sizeof lista);
            char *p = lista;
            for (;;) {
                char *c = acha_char(p, ',');
                if (c) *c = 0;
                int g = sm_grupo_por_nome(p);
                if (g < 0) { imprimir_em(ERROS, "saguimark: grupo desconocido: %s\n", p); return 1; }
                grupos |= 1u << g;
                if (!c) break;
                p = c + 1;
            }
        } else {
            imprimir("Uso: saguimark [--rapido | --preciso] [--solo grupos | --sin-2d] [--no-guardar]\n"
                     "  Mide la CPU, la FPU, SIMD (SSE2), la memoria y los gráficos 2D con pruebas verificadas.\n"
                     "  Grupos: enteros, fpu, simd, memoria, 2d, sistema (separados por comas).\n"
                     "  El grupo 2d abre una ventana de 128x128 en una esquina (sin escritorio: n/d).\n"
                     "  Cierra los demás programas antes de medir.\n");
            return compara_texto(argv[i], "-h") == 0 ? 0 : 1;
        }
    }
    if (sem_2d) grupos &= ~(1u << SM_2D);
    imprimir("SaguiMark %u, modo %s. No toques nada hasta que termine.\n", SM_VERSAO,
             modo == SM_PRECISO ? "preciso" : "rápido");
    registrar("saguimark: inicio modo %s grupos %x", modo == SM_PRECISO ? "preciso" : "rapido", grupos);
    if (!sm_executar(&r, modo, grupos, progresso, NULL)) {
        imprimir_em(ERROS, "saguimark: no se puede medir (sin TSC o sin memoria)\n");
        return 1;
    }
    imprimir("\n");
    int n = sm_texto(&r, texto, sizeof texto);
    escrever_tudo(SAIDA, texto, (unsigned)n);
    imprimir("\n");
    n = sm_comparar(&r, texto, sizeof texto);
    escrever_tudo(SAIDA, texto, (unsigned)n);
    for (int i = 0; i < r.qtd; i++)
        registrar("saguimark: prova %s/%s estado %d tempo %u us var %u pontos %u obtido %x esperado %x",
                  sm_nome_grupo(r.prova[i].grupo), r.prova[i].nome, r.prova[i].estado, r.prova[i].tempo_us,
                  r.prova[i].var_x10, r.prova[i].pontos, r.prova[i].obtido, r.prova[i].esperado);
    registrar("saguimark: total %u erro %d parcial %d grupos %u %u %u %u %u %u duracao %u ms", r.total, r.erro,
              r.total_parcial, r.pontos_grupo[0], r.pontos_grupo[1], r.pontos_grupo[2], r.pontos_grupo[3],
              r.pontos_grupo[4], r.pontos_grupo[5], r.duracao_ms);
    if (guardar) {
        char caminho[96];
        int e = sm_salvar(&r, caminho, sizeof caminho);
        if (e < 0) imprimir_em(ERROS, "saguimark: no se ha podido guardar %s: %s\n", caminho, nome_erro(e));
        else {
            imprimir("\nResultado guardado en %s\n", caminho);
            registrar("saguimark: guardado %s", caminho);
        }
    }
    return r.erro ? 2 : 0;
}
