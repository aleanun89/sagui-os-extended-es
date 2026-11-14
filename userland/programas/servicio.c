// SPDX-License-Identifier: GPL-2.0-or-later
/* servicio - maneja los servicios en segundo plano desde el terminal (D216).
 *
 *   servicio listar                  todos, con su estado
 *   servicio estado [nombre]         uno en detalle (sin nombre, como listar)
 *   servicio iniciar|parar|reiniciar nombre
 *   servicio activar|desactivar nombre    arranque automático al iniciar
 *   servicio registro nombre [-n N]  las últimas líneas de su salida
 *   servicio recargar                vuelve a leer las definiciones
 *
 * Habla con el supervisor (/bin/servicios) por /tmp/.servicios (servicos.h). */
#include "sagui.h"
#include "servicos.h"
#include "contas.h"

#define MAXS 24

static const char *AYUDA =
    "uso: servicio listar | estado [NOMBRE] | iniciar NOMBRE | parar NOMBRE | reiniciar NOMBRE\n"
    "       servicio activar NOMBRE | desactivar NOMBRE | registro NOMBRE [-n N] | recargar\n"
    "Maneja los programas en segundo plano (servicios) que vigila el supervisor.\n"
    "  listar       todos los servicios: estado, PID, arranque automático, reinicios\n"
    "  estado       uno en detalle: definición, registro y por qué está así\n"
    "  iniciar      lo arranca ahora (cuando estén listas sus dependencias)\n"
    "  parar        le pide que termine (cierra su entrada) y, si no lo hace, lo mata\n"
    "  reiniciar    parar e iniciar\n"
    "  activar      arrancará solo al iniciar el sistema (desactivar: ya no)\n"
    "  registro     las últimas líneas de su salida (10 por defecto)\n"
    "  recargar     vuelve a leer las definiciones\n"
    "Definiciones: /etc/servicios y /home/.config/servicios (archivos .conf con\n"
    "nombre=, comando=, arrancar=si, reiniciar=siempre|fallo|no, espera=red,/home,...).\n"
    "Registro del supervisor: /logs/servicios.log (o /tmp/.servicios/servicios.log).\n";

static servico_info_t v[MAXS];

static void tempo_texto(unsigned ms, char *t, int max)
{
    unsigned s = ms / 1000;
    if (s >= 86400) formatar_texto(t, (size_t)max, "%ud %u:%02u", s / 86400, s / 3600 % 24, s / 60 % 60);
    else formatar_texto(t, (size_t)max, "%u:%02u:%02u", s / 3600, s / 60 % 60, s % 60);
}

static int ler_estado(void)
{
    int n = servicos_ler_estado(v, MAXS);
    if (n == ERRO_NAO_EXISTE)
        imprimir_em(ERROS, "servicio: el supervisor de servicios no está en marcha\n");
    return n;
}

static int achar(int n, const char *nome)
{
    for (int i = 0; i < n; i++)
        if (compara_texto(v[i].nome, nome) == 0) return i;
    return -1;
}

static int listar_todos(void)
{
    int n = ler_estado();
    if (n < 0) return 1;
    if (n == 0) { imprimir("No hay servicios definidos (mira /etc/servicios).\n"); return 0; }
    imprimir("NOMBRE           ESTADO        PID  ARRANQUE  REINIC.  TIEMPO      DETALLE\n");
    unsigned agora = tempo_ms();
    for (int i = 0; i < n; i++) {
        char t[24], pid[12];
        tempo_texto(agora - v[i].desde_ms, t, sizeof t);
        formatar_texto(pid, sizeof pid, v[i].pid ? "%d" : "-", v[i].pid);
        imprimir("%-16s %-12s %5s  %-8s  %7u  %-10s  %s\n", v[i].nome, servicos_texto_estado(v[i].estado), pid,
                 v[i].arrancar ? "sí" : "no", v[i].reinicios, t, v[i].detalhe);
    }
    return 0;
}

static int estado_de(const char *nome)
{
    int n = ler_estado();
    if (n < 0) return 1;
    int i = achar(n, nome);
    if (i < 0) { imprimir_em(ERROS, "servicio: no existe el servicio «%s»\n", nome); return 1; }
    servico_info_t *s = &v[i];
    char t[24];
    tempo_texto(tempo_ms() - s->desde_ms, t, sizeof t);
    imprimir("%s - %s\n", s->nome, s->descricao[0] ? s->descricao : "(sin descripción)");
    imprimir("  Estado:     %s desde hace %s\n", servicos_texto_estado(s->estado), t);
    if (s->pid) imprimir("  PID:        %d\n", s->pid);
    if (s->detalhe[0]) imprimir("  Detalle:    %s\n", s->detalhe);
    imprimir("  Arranque:   %s\n", s->arrancar ? "automático al iniciar el sistema" : "a mano");
    imprimir("  Reinicios:  %u\n", s->reinicios);
    imprimir("  Definido:   %s\n", s->usuario ? "/home/.config/servicios" : "/etc/servicios");
    imprimir("  Registro:   %s\n", s->registro);
    return 0;
}

/* Espera (como mucho "ms") a que el servicio llegue a un estado final para la orden. */
static int esperar_estado(const char *nome, bool quer_marcha, int pid_antes, unsigned ms)
{
    unsigned fim = tempo_ms() + ms;
    for (;;) {
        int n = servicos_ler_estado(v, MAXS);
        int i = n > 0 ? achar(n, nome) : -1;
        if (i < 0) return 1;
        servico_info_t *s = &v[i];
        bool marcha = compara_texto(s->estado, SERV_E_MARCHA) == 0;
        if (quer_marcha && marcha && s->pid != pid_antes) {
            imprimir("%s: en marcha (pid %d)\n", s->nome, s->pid);
            return 0;
        }
        if (!quer_marcha && compara_texto(s->estado, SERV_E_PARADO) == 0) {
            imprimir("%s: parado\n", s->nome);
            return 0;
        }
        if (compara_texto(s->estado, SERV_E_FALLIDO) == 0 ||
            (quer_marcha && compara_texto(s->estado, SERV_E_PARADO) == 0 && tempo_ms() - s->desde_ms < 60000 &&
             s->detalhe[0])) {
            imprimir("%s: %s%s%s\n", s->nome, servicos_texto_estado(s->estado), s->detalhe[0] ? " - " : "", s->detalhe);
            return 1;
        }
        if ((int)(tempo_ms() - fim) >= 0) {
            imprimir("%s: %s%s%s (sigue en ello)\n", s->nome, servicos_texto_estado(s->estado),
                     s->detalhe[0] ? " - " : "", s->detalhe);
            return compara_texto(s->estado, SERV_E_ESPERANDO) == 0 ? 0 : 1;
        }
        dormir(100);
    }
}

static int pedir(const char *ordem, const char *nome)
{
    int pid_antes = 0;
    if (nome) {
        int n = ler_estado();
        if (n < 0) return 1;
        int i = achar(n, nome);
        if (i < 0) { imprimir_em(ERROS, "servicio: no existe el servicio «%s» (mira «servicio listar»)\n", nome); return 1; }
        pid_antes = v[i].pid;
    }
    if (falta_admin("servicio")) return 1;   /* D255: las órdenes al gestor son de root */
    char msg[200];
    int r = servicos_pedir(ordem, nome, msg, sizeof msg);
    if (r < 0) {
        imprimir_em(ERROS, "servicio: %s\n", r == ERRO_NAO_EXISTE ? "el supervisor de servicios no está en marcha"
                                                                  : r == ERRO_ACESSO ? nome_erro(r) : "el supervisor no contesta");
        return 1;
    }
    if (r != 0) { imprimir_em(ERROS, "servicio: %s\n", msg); return 1; }
    if (compara_texto(ordem, "iniciar") == 0) {
        if (pid_antes) { imprimir("%s\n", msg); return 0; }
        return esperar_estado(nome, true, 0, 5000);
    }
    if (compara_texto(ordem, "reiniciar") == 0) return esperar_estado(nome, true, pid_antes, 15000);
    if (compara_texto(ordem, "parar") == 0) return esperar_estado(nome, false, 0, 15000);
    imprimir("%s\n", msg);
    return 0;
}

/* Las últimas "linhas" líneas del registro del servicio. */
static int registro(const char *nome, int linhas)
{
    int n = ler_estado();
    if (n < 0) return 1;
    int i = achar(n, nome);
    if (i < 0) { imprimir_em(ERROS, "servicio: no existe el servicio «%s»\n", nome); return 1; }
    int fd = abrir(v[i].registro, ABRIR_LER);
    if (fd < 0) { imprimir("%s: todavía no hay registro (%s)\n", nome, v[i].registro); return 0; }
    unsigned tam;
    char *b = ler_todo_fd(fd, &tam);
    fechar(fd);
    if (!b) return 1;
    unsigned ini = tam;
    int vistas = 0;
    if (ini > 0 && b[ini - 1] == '\n') ini--;
    while (ini > 0) {
        if (b[ini - 1] == '\n' && ++vistas == linhas) break;
        ini--;
    }
    imprimir("== %s ==\n", v[i].registro);
    escrever_tudo(SAIDA, b + ini, tam - ini);
    libera(b);
    return 0;
}

int principal(int argc, char **argv)
{
    int linhas = 10;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "n:", &o)) >= 0) {
        if (r == 'n') { if (!texto_para_numero(o.valor, &linhas) || linhas < 1) linhas = 10; }
        else if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        else return opcao_invalida("servicio", r, &o);
    }
    int na = argc - o.indice;
    char **a = argv + o.indice;
    if (na == 0 || compara_texto(a[0], "listar") == 0 || compara_texto(a[0], "lista") == 0) return listar_todos();
    if (compara_texto(a[0], "estado") == 0) return na > 1 ? estado_de(a[1]) : listar_todos();
    if (compara_texto(a[0], "recargar") == 0) return pedir("recargar", NULL);
    static const char *const com_nome[] = { "iniciar", "parar", "reiniciar", "activar", "desactivar", NULL };
    for (int k = 0; com_nome[k]; k++)
        if (compara_texto(a[0], com_nome[k]) == 0) {
            if (na < 2) { imprimir_em(ERROS, "servicio: falta el nombre del servicio (servicio %s NOMBRE)\n", a[0]); return 2; }
            return pedir(com_nome[k], a[1]);
        }
    if (compara_texto(a[0], "registro") == 0) {
        if (na < 2) { imprimir_em(ERROS, "servicio: falta el nombre del servicio (servicio registro NOMBRE)\n"); return 2; }
        return registro(a[1], linhas);
    }
    imprimir_em(ERROS, "servicio: orden «%s» desconocida (mira servicio --ayuda)\n", a[0]);
    return 2;
}
