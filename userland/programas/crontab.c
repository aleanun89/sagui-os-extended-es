// SPDX-License-Identifier: GPL-2.0-or-later
/* crontab - la tabla de cron del usuario, /home/.config/crontab (D216).
 *
 *   crontab -l               la muestra
 *   crontab -e               la edita (con el Editor, o $EDITOR; sin escritorio,
 *                            con un editor de líneas aquí mismo) y la valida
 *   crontab -r               la borra
 *   crontab ARCHIVO | -      instala ARCHIVO (o la entrada) como tabla
 *   crontab --comprobar ARCHIVO   solo dice si es correcta
 *
 * Nunca se instala una tabla con errores: se dice la línea y el motivo. El
 * servicio cron ve el cambio en un par de segundos. */
#include "sagui.h"
#include "cron.h"
#include "servicos.h"

#define TABELA "/home/.config/crontab"

static const char *AYUDA =
    "uso: crontab -l | -e | -r | ARCHIVO | - | --comprobar ARCHIVO\n"
    "La tabla de tareas programadas del usuario (" TABELA ").\n"
    "  -l           la muestra\n"
    "  -e           la edita (Editor o $EDITOR; sin escritorio, editor de líneas) y la comprueba\n"
    "  -r           la borra\n"
    "  ARCHIVO      la reemplaza por ARCHIVO (\"-\": lo que llegue por la entrada)\n"
    "  --comprobar  dice si un archivo es una tabla correcta, sin instalarla\n"
    "Formato de cada línea: minuto hora día mes día_semana orden\n"
    "  */15 * * * * ntp          cada 15 minutos\n"
    "  30 8 * * 1-5 orden        a las 8:30 de lunes a viernes\n"
    "  @reboot orden             al arrancar; también @hourly, @daily, @weekly, @monthly, @yearly\n"
    "  NOMBRE=valor              variable para las tareas\n"
    "Si el PC estaba apagado, un @daily (o @hourly, @weekly...) se ejecuta al arrancar;\n"
    "con \"!@daily\" no. Las horas son las de la zona del sistema (zona).\n";

static const char *MODELO =
    "# Tabla de cron del usuario (crontab -e). Una tarea por línea:\n"
    "#   minuto hora día mes día_semana orden\n"
    "#   */15 * * * * echo hola >> /tmp/hola.txt\n"
    "#   @daily ntp\n"
    "# Más: crontab --ayuda\n";

static char *ler_arquivo(const char *arq)
{
    int fd = compara_texto(arq, "-") == 0 ? ENTRADA : abrir(arq, ABRIR_LER);
    if (fd < 0) return NULL;
    unsigned tam;
    char *b = ler_todo_fd(fd, &tam);
    if (fd != ENTRADA) fechar(fd);
    if (b) b[utf8_para_latin1(b, tam, NULL)] = 0;
    return b;
}

static int contar_tarefas(const char *texto)
{
    int n = 0;
    const char *p = texto;
    while (*p) {
        const char *fim = acha_char(p, '\n');
        int k = fim ? (int)(fim - p) : (int)tam_texto(p);
        char l[CRON_COMANDO_MAX + 128];
        if (k >= (int)sizeof l) k = (int)sizeof l - 1;
        memcpy(l, p, (size_t)k);
        l[k] = 0;
        cron_tarefa_t t;
        char e[8];
        if (cron_analisar_linha(l, &t, e, sizeof e) == 1) n++;
        if (!fim) break;
        p = fim + 1;
    }
    return n;
}

static int gravar(const char *texto)
{
    servicos_criar_pastas("/home/.config");
    int fd = abrir(TABELA, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { imprimir_em(ERROS, "crontab: no se puede escribir %s: %s\n", TABELA, nome_erro(fd)); return 1; }
    unsigned n = (unsigned)tam_texto(texto);
    escrever_tudo(fd, texto, n);
    if (n && texto[n - 1] != '\n') escrever_tudo(fd, "\n", 1);
    fechar(fd);
    imprimir("crontab: tabla instalada (%d tarea(s)); cron la leerá en unos segundos\n", contar_tarefas(texto));
    if (!servicos_montado("/home")) imprimir("crontab: ojo, /home no es persistente: la tabla se perderá al apagar\n");
    return 0;
}

static int instalar(const char *arq)
{
    char *b = ler_arquivo(arq);
    if (!b) { imprimir_em(ERROS, "crontab: no se puede leer %s\n", arq); return 1; }
    int erros = cron_comprobar_tabla(b, ERROS);
    if (erros) {
        imprimir_em(ERROS, "crontab: %d error(es): la tabla NO se ha instalado\n", erros);
        libera(b);
        return 1;
    }
    int r = gravar(b);
    libera(b);
    return r;
}

/* ------------------------------------------------------------ editor de líneas */

#define MAX_LINHAS 64
static char linhas[MAX_LINHAS][CRON_COMANDO_MAX + 64];
static int qtd_linhas;

static void mostrar_linhas(void)
{
    if (!qtd_linhas) imprimir("  (tabla vacía)\n");
    for (int i = 0; i < qtd_linhas; i++) imprimir("%3d  %s\n", i + 1, linhas[i]);
}

static int editor_linhas(const char *arq)
{
    char *b = ler_arquivo(arq);
    qtd_linhas = 0;
    for (char *l = b; l && *l && qtd_linhas < MAX_LINHAS;) {
        char *fim = acha_char(l, '\n');
        if (fim) *fim = 0;
        copia_texto(linhas[qtd_linhas++], l, sizeof linhas[0]);
        l = fim ? fim + 1 : NULL;
    }
    if (b) libera(b);
    imprimir("Editor de líneas de crontab. Órdenes: l (ver), a TEXTO (añadir), b N (borrar),\n"
             "c N TEXTO (cambiar), g (guardar y salir), q (salir sin guardar).\n");
    mostrar_linhas();
    char cmd[CRON_COMANDO_MAX + 80];
    for (;;) {
        imprimir("crontab> ");
        int n = ler_linha(ENTRADA, cmd, sizeof cmd);
        if (n < 0) return 1;   /* fin de la entrada: como q */
        char *p = cmd;
        while (*p == ' ') p++;
        char o = *p ? *p++ : 0;
        while (*p == ' ') p++;
        if (o == 'l') mostrar_linhas();
        else if (o == 'q') return 1;
        else if (o == 'g') break;
        else if (o == 'a') {
            if (qtd_linhas == MAX_LINHAS) { imprimir("la tabla está llena (%d líneas)\n", MAX_LINHAS); continue; }
            copia_texto(linhas[qtd_linhas++], p, sizeof linhas[0]);
        } else if (o == 'b' || o == 'c') {
            int k = texto_para_int(p) - 1;
            if (k < 0 || k >= qtd_linhas) { imprimir("no hay línea %d\n", k + 1); continue; }
            if (o == 'b') {
                for (int i = k; i < qtd_linhas - 1; i++) copia_texto(linhas[i], linhas[i + 1], sizeof linhas[0]);
                qtd_linhas--;
            } else {
                while (*p >= '0' && *p <= '9') p++;
                while (*p == ' ') p++;
                copia_texto(linhas[k], p, sizeof linhas[0]);
            }
        } else if (o) imprimir("orden desconocida: l, a, b, c, g o q\n");
    }
    int fd = abrir(arq, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return 1;
    for (int i = 0; i < qtd_linhas; i++) imprimir_em(fd, "%s\n", linhas[i]);
    fechar(fd);
    return 0;
}

/* ------------------------------------------------------------ crontab -e */

static bool achar_programa(const char *nome, char *caminho, int max)
{
    info_arquivo_t i;
    if (acha_char(nome, '/')) { copia_texto(caminho, nome, (size_t)max); return info_arquivo(caminho, &i) == 0; }
    static const char *const pastas[] = { "/bin", "/apps/bin" };
    for (int k = 0; k < 2; k++) {
        formatar_texto(caminho, (size_t)max, "%s/%s", pastas[k], nome);
        if (info_arquivo(caminho, &i) == 0) return true;
    }
    return false;
}

/* El editor de verdad (el Editor del escritorio o $EDITOR). -1 = no se pudo usar. */
static int editor_externo(const char *arq)
{
    const char *ed = obter_env("EDITOR");
    if (!ed || !*ed) ed = "editor";
    char caminho[128];
    if (!achar_programa(ed, caminho, sizeof caminho)) return -1;
    char *argv[] = { (char *)ed, (char *)arq, NULL };
    int pid = executar(caminho, argv);
    if (pid < 0) return -1;
    int codigo = 0;
    esperar(pid, &codigo);
    return codigo == 0 ? 0 : -1;   /* el Editor sin escritorio sale con 1 */
}

static bool perguntar(const char *pergunta)
{
    char r[16];
    imprimir("%s (s/n) ", pergunta);
    if (ler_linha(ENTRADA, r, sizeof r) < 0) return false;
    return r[0] == 's' || r[0] == 'S' || r[0] == 'y' || r[0] == 'Y';
}

static int editar(void)
{
    char tmp[64];
    formatar_texto(tmp, sizeof tmp, "/tmp/crontab.%d", meu_pid());
    char *atual = ler_arquivo(TABELA);
    int fd = abrir(tmp, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { imprimir_em(ERROS, "crontab: no se puede crear %s\n", tmp); if (atual) libera(atual); return 1; }
    const char *ini = atual ? atual : MODELO;
    escrever_tudo(fd, ini, (unsigned)tam_texto(ini));
    fechar(fd);
    if (atual) libera(atual);
    bool linhas_modo = false;
    for (;;) {
        int r = linhas_modo ? editor_linhas(tmp) : editor_externo(tmp);
        if (r < 0 && !linhas_modo) { linhas_modo = true; r = editor_linhas(tmp); }
        if (r != 0) { imprimir("crontab: no se ha cambiado nada\n"); apagar(tmp); return 1; }
        char *b = ler_arquivo(tmp);
        if (!b) { apagar(tmp); return 1; }
        int erros = cron_comprobar_tabla(b, SAIDA);
        if (!erros) {
            int g = gravar(b);
            libera(b);
            apagar(tmp);
            return g;
        }
        libera(b);
        imprimir("crontab: %d error(es) en la tabla.\n", erros);
        if (!perguntar("¿Volver a editarla?")) {
            imprimir("crontab: no se ha cambiado nada\n");
            apagar(tmp);
            return 1;
        }
    }
}

int principal(int argc, char **argv)
{
    static const opcao_longa_t longas[] = { { "comprobar", 'k' }, { NULL, 0 } };
    opcoes_t o = { 0 };
    o.longas = longas;
    int r, modo = 0;
    const char *comprobar = NULL;
    while ((r = obter_opcao(argc, argv, "lerk:", &o)) >= 0) {
        if (r == 'l' || r == 'e' || r == 'r') modo = r;
        else if (r == 'k') { modo = 'k'; comprobar = o.valor; }
        else if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        else return opcao_invalida("crontab", r, &o);
    }
    if (modo == 'l') {
        char *b = ler_arquivo(TABELA);
        if (!b) { imprimir_em(ERROS, "crontab: no hay tabla (%s)\n", TABELA); return 1; }
        escrever_texto(SAIDA, b);
        libera(b);
        return 0;
    }
    if (modo == 'r') {
        int e = apagar(TABELA);
        if (e < 0) { imprimir_em(ERROS, "crontab: no hay tabla que borrar\n"); return 1; }
        imprimir("crontab: tabla borrada\n");
        return 0;
    }
    if (modo == 'e') return editar();
    if (modo == 'k') {
        char *b = ler_arquivo(comprobar);
        if (!b) { imprimir_em(ERROS, "crontab: no se puede leer %s\n", comprobar); return 1; }
        int erros = cron_comprobar_tabla(b, SAIDA);
        imprimir(erros ? "crontab: %d error(es)\n" : "crontab: tabla correcta (%d tareas)\n",
                 erros ? erros : contar_tarefas(b));
        libera(b);
        return erros ? 1 : 0;
    }
    if (o.indice < argc) return instalar(argv[o.indice]);
    escrever_texto(ERROS, AYUDA);
    return 2;
}
