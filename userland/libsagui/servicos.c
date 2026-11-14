// SPDX-License-Identifier: GPL-2.0-or-later
/* servicos.c - el lado del cliente del gestor de servicios (D216): leer el
 * estado que publica el supervisor y mandarle órdenes. Lo usan "servicio",
 * el Administrador de tareas y el propio supervisor (las rutas y las carpetas).
 * El formato está descrito en servicos.h. */
#include "sagui.h"
#include "servicos.h"

/* ------------------------------------------------------------ utilidades */

bool servicos_montado(const char *ponto)
{
    info_montagem_t m[12];
    int n = info_sistema(INFO_MONTAGENS, m, 12);
    for (int i = 0; i < n && i < 12; i++)
        if (compara_texto(m[i].ponto, ponto) == 0) return true;
    return false;
}

const char *servicos_pasta_registros(void)
{
    info_arquivo_t i;
    if (info_arquivo("/logs", &i) == 0 && i.tipo == TIPO_PASTA) return "/logs";
    return SERV_DIR_COMUM;   /* sin /logs, dentro de la carpeta oculta: no ensucia /tmp en el Archivos */
}

void servicos_criar_pastas(const char *caminho)
{
    char c[128];
    copia_texto(c, caminho, sizeof c);
    for (char *p = c + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        criar_pasta(c);   /* si ya existe, ERRO_JA_EXISTE: da igual */
        *p = '/';
    }
    criar_pasta(c);
}

const char *servicos_texto_estado(const char *e)
{
    if (compara_texto(e, SERV_E_MARCHA) == 0) return "en marcha";
    if (compara_texto(e, SERV_E_PARADO) == 0) return "parado";
    if (compara_texto(e, SERV_E_ESPERANDO) == 0) return "esperando";
    if (compara_texto(e, SERV_E_REINICIANDO) == 0) return "reiniciando";
    if (compara_texto(e, SERV_E_PARANDO) == 0) return "parando";
    if (compara_texto(e, SERV_E_FALLIDO) == 0) return "fallido";
    return e;
}

/* ------------------------------------------------------------ estado */

/* Separa la línea por tabuladores (en el lugar); devuelve cuántos campos. */
static int campos(char *l, char **c, int max)
{
    int n = 0;
    c[n++] = l;
    for (char *p = l; *p && n < max; p++)
        if (*p == '\t') { *p = 0; c[n++] = p + 1; }
    return n;
}

static int ler_uma_vez(servico_info_t *v, int max, bool *completo)
{
    *completo = false;
    int fd = abrir(SERV_ARQ_ESTADO, ABRIR_LER);
    if (fd < 0) return 0;
    unsigned tam;
    char *b = ler_todo_fd(fd, &tam);
    fechar(fd);
    if (!b) return 0;
    int n = 0;
    char *l = b;
    while (*l) {
        char *fim = acha_char(l, '\n');
        if (!fim) break;   /* línea sin terminar: el supervisor aún escribe */
        *fim = 0;
        if (compara_texto(l, "fin") == 0) { *completo = true; break; }
        char *c[11];
        if (!comeca_com(l, "servicios ") && campos(l, c, 11) >= 10 && n < max) {
            servico_info_t *s = &v[n++];
            memset(s, 0, sizeof *s);
            copia_texto(s->nome, c[0], sizeof s->nome);
            copia_texto(s->estado, c[1], sizeof s->estado);
            s->pid = texto_para_int(c[2]);
            s->reinicios = (unsigned)texto_para_int(c[3]);
            s->desde_ms = (unsigned)texto_para_int(c[4]);
            s->arrancar = c[5][0] == '1';
            s->usuario = c[6][0] == '1';
            copia_texto(s->descricao, c[7], sizeof s->descricao);
            copia_texto(s->detalhe, c[8], sizeof s->detalhe);
            copia_texto(s->registro, c[9], sizeof s->registro);
        }
        l = fim + 1;
    }
    libera(b);
    return n;
}

int servicos_ler_estado(servico_info_t *v, int max)
{
    if (servicos_kernel(SERV_SUPERVISOR, 0) <= 0) return ERRO_NAO_EXISTE;
    /* El supervisor reescribe el archivo entero: quien lo pilla a medias
     * (sin la línea "fin") vuelve a mirar un poco después. */
    for (int intento = 0; intento < 20; intento++) {
        bool completo;
        int n = ler_uma_vez(v, max, &completo);
        if (completo) return n;
        dormir(50);
    }
    return 0;
}

/* ------------------------------------------------------------ órdenes */

int servicos_pedir(const char *ordem, const char *nome, char *resposta, int max)
{
    if (resposta && max > 0) resposta[0] = 0;
    if (servicos_kernel(SERV_SUPERVISOR, 0) <= 0) return ERRO_NAO_EXISTE;
    int eu = meu_pid();
    char tmp[64], ped[64], res[64];
    formatar_texto(tmp, sizeof tmp, "%s/%d.tmp", SERV_DIR_PEDIDOS, eu);
    formatar_texto(ped, sizeof ped, "%s/%d.ped", SERV_DIR_PEDIDOS, eu);
    formatar_texto(res, sizeof res, "%s/%d.res", SERV_DIR_PEDIDOS, eu);
    apagar(res);
    apagar(ped);
    int fd = abrir(tmp, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    char linha[96];
    int n = formatar_texto(linha, sizeof linha, "%s %s\n", ordem, nome ? nome : "-");
    escrever_tudo(fd, linha, (unsigned)n);
    fechar(fd);
    int r = renomear(tmp, ped);
    if (r < 0) { apagar(tmp); return r; }
    /* El supervisor mira los pedidos cada 0,2 s; parar y reiniciar contestan
     * al momento (el que llama espera después a que cambie el estado). */
    for (int i = 0; i < 100; i++) {
        dormir(50);
        fd = abrir(res, ABRIR_LER);
        if (fd < 0) continue;
        unsigned tam;
        char *b = ler_todo_fd(fd, &tam);
        fechar(fd);
        apagar(res);
        if (!b) return ERRO_SEM_MEMORIA;
        int codigo = texto_para_int(b);   /* primera línea: el código */
        char *msg = acha_char(b, '\n');
        if (resposta && max > 0) {
            copia_texto(resposta, msg ? msg + 1 : "", (size_t)max);
            int k = (int)tam_texto(resposta);
            while (k > 0 && resposta[k - 1] == '\n') resposta[--k] = 0;
        }
        libera(b);
        return codigo;
    }
    apagar(ped);
    return ERRO_OCUPADO;   /* no contestó a tiempo */
}
