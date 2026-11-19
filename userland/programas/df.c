// SPDX-License-Identifier: GPL-2.0-or-later
/* df - espacio libre en los sistemas de archivos montados (D097). */
#include "sagui.h"
#include "particoes.h"

static const char *AYUDA =
    "uso: df [-h] [-k] [carpeta...]\n"
    "  -h  tamaños legibles (1.5G)     -k  en bloques de 1 KB (por defecto)\n"
    "Sin argumentos muestra todos los montajes; con una carpeta, el montaje que la contiene.\n";

static void celda(char *s, size_t max, uint32_t kb, bool humano)
{
    if (humano) formatar_tamanho(s, max, kb >= 4194304u ? 0xFFFFFFFFu : kb * 1024u);
    else formatar_texto(s, max, "%u", kb);
}

int principal(int argc, char **argv)
{
    bool humano = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "hkT", &o)) >= 0) {
        switch (r) {
        case 'h': humano = true; break;
        case 'k': case 'T': break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("df", r, &o);
        }
    }
    static info_montagem_t m[16];
    int n = info_sistema(INFO_MONTAGENS, m, sizeof m / sizeof m[0]);
    if (n < 0) { imprimir_em(ERROS, "df: %s\n", nome_erro(n)); return 1; }
    /* La raíz (la RAM) no aparece como montaje: se añade a mano con los datos de la memoria. */
    bool hay_raiz = false;
    for (int i = 0; i < n; i++) if (compara_texto(m[i].ponto, "/") == 0) hay_raiz = true;
    if (!hay_raiz && n < 16) {
        info_memoria_t mem;
        if (info_sistema(INFO_MEMORIA, &mem, sizeof mem) == 0) {
            for (int i = n; i > 0; i--) m[i] = m[i - 1];
            copia_texto(m[0].dispositivo, "ramfs", sizeof m[0].dispositivo);
            copia_texto(m[0].ponto, "/", sizeof m[0].ponto);
            copia_texto(m[0].tipo, "ramfs", sizeof m[0].tipo);
            m[0].opcoes = 0;
            m[0].total_kb = mem.gerida_kb;
            m[0].livre_kb = mem.livre_kb;
            n++;
        }
    }
    /* D388: en la live, «/» es el sistema de serie (SAGUI-RAIZ, solo lectura) con la capa de cambios encima
     * (SAGUI-PERSIST o la RAM): una sola línea «/» con el espacio de la capa, y /sistema/serie (la misma base
     * vista sola) no se repite salvo que se pida. */
    char base[16] = "", capa[16] = "";
    bool live = pt_raiz_live(base, capa, sizeof base);
    for (int i = 0; live && i < n; i++) {
        if (compara_texto(m[i].ponto, "/") == 0) {
            if (capa[0]) formatar_texto(m[i].dispositivo, sizeof m[i].dispositivo, "%s+%s", base, capa);
            copia_texto(m[i].tipo, "live", sizeof m[i].tipo);
        }
    }
    imprimir("%-12s %-7s %9s %9s %9s %5s  %s\n", "Sistema", "Tipo", humano ? "Tamaño" : "Bloques-1K", "Usado", "Libre", "Uso%", "Montado en");
    int erros = 0;
    int nargs = argc - o.indice;
    for (int a = 0; a < (nargs ? nargs : 1); a++) {
        int mejor = -1;
        size_t mejor_n = 0;
        if (nargs) {
            info_arquivo_t inf;
            if (info_arquivo(argv[o.indice + a], &inf) < 0) { imprimir_em(ERROS, "df: %s: no existe\n", argv[o.indice + a]); erros++; continue; }
            char abs[256];
            if (argv[o.indice + a][0] == '/') copia_texto(abs, argv[o.indice + a], sizeof abs);
            else { pasta_atual(abs, sizeof abs); if (compara_texto(argv[o.indice + a], ".") != 0) caminho_juntar(abs, sizeof abs, abs, argv[o.indice + a]); }
            for (int i = 0; i < n; i++) {
                size_t l = tam_texto(m[i].ponto);
                bool prefijo = compara_texto(m[i].ponto, "/") == 0 || (compara_texto_n(abs, m[i].ponto, l) == 0 && (abs[l] == 0 || abs[l] == '/'));
                if (prefijo && l >= mejor_n) { mejor = i; mejor_n = l; }
            }
            if (mejor < 0) continue;
        }
        for (int i = nargs ? mejor : 0; i < (nargs ? mejor + 1 : n); i++) {
            if (live && !nargs && compara_texto(m[i].tipo, "serie") == 0) continue;
            uint32_t usado = m[i].total_kb >= m[i].livre_kb ? m[i].total_kb - m[i].livre_kb : 0;
            char t[16], u[16], l[16], pct[8];
            celda(t, sizeof t, m[i].total_kb, humano);
            celda(u, sizeof u, usado, humano);
            celda(l, sizeof l, m[i].livre_kb, humano);
            if (m[i].total_kb) formatar_texto(pct, sizeof pct, "%u%%", (usado * 100 + m[i].total_kb - 1) / m[i].total_kb);
            else copia_texto(pct, "-", sizeof pct);
            imprimir("%-12s %-7s %9s %9s %9s %5s  %s\n", m[i].dispositivo, m[i].tipo, t, u, l, pct, m[i].ponto);
        }
    }
    if (live) {
        if (capa[0])
            imprimir("/ es una live: el sistema de serie (%s, SAGUI-RAIZ, solo lectura) con tus cambios encima (%s, SAGUI-PERSIST);\n"
                     "el espacio de / es el que queda en la capa de cambios.\n", base, capa);
        else
            imprimir("/ es una live: el sistema de serie (%s, SAGUI-RAIZ, solo lectura) con tus cambios en la RAM (se pierden al apagar).\n", base);
    }
    return erros ? 1 : 0;
}
