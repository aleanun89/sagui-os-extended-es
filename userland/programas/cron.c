// SPDX-License-Identifier: GPL-2.0-or-later
/* cron - ejecuta tareas a horas fijas (D216, mejora 074).
 *
 * Es un servicio más del supervisor (/etc/servicios/cron.conf). Lee dos tablas:
 * /etc/crontab (la del sistema) y /home/.config/crontab (la del usuario, la que
 * cambia "crontab"), y las vuelve a leer cuando cambian. El formato está en
 * cron.h: 5 campos o atajos (@reboot, @daily...), con la hora local de la zona
 * del sistema (D080).
 *
 * Cada tarea se ejecuta con "sagsh -c orden", con la entrada vacía y la salida
 * y los errores en <registros>/cron-salida.log; cada ejecución se anota en
 * <registros>/cron.log (inicio y fin con el código y la duración). Los
 * registros van en /logs o, si no hay, en /tmp/.servicios (oculta).
 *
 * Tareas perdidas (como anacron): con /home persistente, la hora de la última
 * ejecución de cada atajo periódico se guarda en /home/.config/cron/estado; al
 * arrancar, el que lleva más de su periodo sin ejecutarse (o nunca se ejecutó)
 * se ejecuta una vez. Sin /home persistente no se recupera nada.
 *
 * El supervisor le pide que pare cerrando su entrada: aquí se mira una vez por
 * segundo. Ejecutado a mano desde un terminal, termina con Ctrl+C.
 *
 *   cron                       el servicio
 *   cron proximas              lo que se va a ejecutar y cuándo
 *   cron --proxima "CAMPOS" [DESDE [N]]   las N próximas horas (UTC) de una
 *                              expresión, desde DESDE (segundos UTC); para las pruebas */
#include "sagui.h"
#include "cron.h"
#include "servicos.h"

#define MAX_TAREFAS 64
#define MAX_EXEC     8
#define MAX_VARS     8
#define FD_AVISO    29          /* la entrada que puso el supervisor (su cierre = parar) */
#define FD_SAIDA    28          /* la salida que puso el supervisor (el registro del servicio) */

static const char *const tabelas[2] = { "/etc/crontab", "/home/.config/crontab" };

typedef struct {
    cron_tarefa_t t;
    int tabela;
    unsigned chave;
} tarefa_t;

typedef struct {
    int pid;
    unsigned inicio;
    char comando[CRON_COMANDO_MAX];
} exec_t;

static tarefa_t tar[MAX_TAREFAS];
static int qtd;
static char vars[2][MAX_VARS][96];
static int nvars[2];
static unsigned tam_visto[2], mod_visto[2];
static bool existe_visto[2];
static exec_t execs[MAX_EXEC];
static char arq_log[96], arq_saida[96], arq_estado[96];
static bool recuperar_ok;

/* ------------------------------------------------------------ registro */

static void anotar(const char *fmt, ...)
{
    char t[300];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(t, sizeof t, fmt, ap);
    va_end(ap);
    registrar("cron: %s", t);
    int fd = abrir(arq_log, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_ACRESCENTAR);
    if (fd < 0) return;
    fuso_data_t d;
    agora_local(&d);
    char h[32];
    formatar_data(h, sizeof h, "%Y-%m-%d %H:%M:%S", &d);
    imprimir_em(fd, "%s %s\n", h, t);
    fechar(fd);
}

/* ------------------------------------------------------------ estado (tareas perdidas) */

static unsigned chave_de(const tarefa_t *t)
{
    unsigned h = 2166136261u;   /* FNV-1a de la tabla, el atajo y la orden */
    h = (h ^ (unsigned)t->tabela) * 16777619u;
    h = (h ^ (unsigned)t->t.q.especial) * 16777619u;
    for (const char *p = t->t.comando; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    return h;
}

#define MAX_ESTADO 64
static unsigned est_chave[MAX_ESTADO], est_hora[MAX_ESTADO];
static int qtd_est;

static unsigned hexa(const char *s)
{
    unsigned v = 0;
    for (; *s; s++) {
        int d = *s >= '0' && *s <= '9' ? *s - '0' : *s >= 'a' && *s <= 'f' ? *s - 'a' + 10 : -1;
        if (d < 0) break;
        v = v * 16 + (unsigned)d;
    }
    return v;
}

static void estado_ler(void)
{
    qtd_est = 0;
    int fd = abrir(arq_estado, ABRIR_LER);
    if (fd < 0) return;
    unsigned tam;
    char *b = ler_todo_fd(fd, &tam);
    fechar(fd);
    if (!b) return;
    for (char *l = b; l && *l && qtd_est < MAX_ESTADO;) {
        char *fim = acha_char(l, '\n');
        if (fim) *fim = 0;
        char *esp = acha_char(l, ' ');
        if (esp) {
            *esp = 0;
            est_chave[qtd_est] = hexa(l);
            est_hora[qtd_est++] = (unsigned)texto_para_int(esp + 1);
        }
        l = fim ? fim + 1 : NULL;
    }
    libera(b);
}

static unsigned estado_obter(unsigned chave)
{
    for (int i = 0; i < qtd_est; i++)
        if (est_chave[i] == chave) return est_hora[i];
    return 0;
}

static void estado_por(unsigned chave, unsigned hora)
{
    int i = 0;
    while (i < qtd_est && est_chave[i] != chave) i++;
    if (i == qtd_est) {
        if (qtd_est == MAX_ESTADO) return;
        qtd_est++;
    }
    est_chave[i] = chave;
    est_hora[i] = hora;
    int fd = abrir(arq_estado, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    for (int k = 0; k < qtd_est; k++) imprimir_em(fd, "%x %u\n", est_chave[k], est_hora[k]);
    fechar(fd);
}

/* ------------------------------------------------------------ tablas */

static void ler_tabela(int k)
{
    nvars[k] = 0;
    int fd = abrir(tabelas[k], ABRIR_LER);
    if (fd < 0) return;
    unsigned tam;
    char *b = ler_todo_fd(fd, &tam);
    fechar(fd);
    if (!b) return;
    b[utf8_para_latin1(b, tam_texto(b), NULL)] = 0;
    int num = 0, erros = 0, n = 0;
    for (char *l = b; l && *l;) {
        char *fim = acha_char(l, '\n');
        if (fim) *fim = 0;
        num++;
        tarefa_t t;
        char erro[160];
        int r = cron_analisar_linha(l, &t.t, erro, sizeof erro);
        if (r < 0) { anotar("%s, línea %d: %s (se ignora)", tabelas[k], num, erro); erros++; }
        else if (r == 2) {
            const char *p = l;
            while (*p == ' ' || *p == '\t') p++;
            if (nvars[k] < MAX_VARS) copia_texto(vars[k][nvars[k]++], p, sizeof vars[k][0]);
        } else if (r == 1) {
            if (qtd >= MAX_TAREFAS) { anotar("%s: demasiadas tareas (máximo %d)", tabelas[k], MAX_TAREFAS); break; }
            t.t.linha = num;
            t.tabela = k;
            t.chave = chave_de(&t);
            tar[qtd++] = t;
            n++;
        }
        l = fim ? fim + 1 : NULL;
    }
    libera(b);
    anotar("%s: %d tarea(s)%s", tabelas[k], n, erros ? " (con errores)" : "");
}

/* Lo que se mira para saber si una tabla cambió: si existe, su tamaño y su fecha. */
static bool tabela_mudou(int k, bool guardar)
{
    info_arquivo_t i;
    datas_arquivo_t d;
    bool existe = info_arquivo(tabelas[k], &i) == 0;
    unsigned tam = existe ? i.tamanho : 0, mod = 0;
    if (existe && datas_arquivo(tabelas[k], &d) == 0) mod = d.modificado;
    bool mudou = existe != existe_visto[k] || tam != tam_visto[k] || mod != mod_visto[k];
    if (guardar) { existe_visto[k] = existe; tam_visto[k] = tam; mod_visto[k] = mod; }
    return mudou;
}

static void ler_tabelas(void)
{
    qtd = 0;
    for (int k = 0; k < 2; k++) {
        tabela_mudou(k, true);
        ler_tabela(k);
    }
}

/* ------------------------------------------------------------ ejecutar */

static void hora_texto(char *h, int max)
{
    fuso_data_t d;
    agora_local(&d);
    formatar_data(h, max, "%Y-%m-%d %H:%M:%S", &d);
}

static void executar_tarefa(const tarefa_t *t, const char *motivo)
{
    int k = 0;
    while (k < MAX_EXEC && execs[k].pid) k++;
    if (k == MAX_EXEC) {
        anotar("demasiadas tareas a la vez (%d): se salta «%s»", MAX_EXEC, t->t.comando);
        return;
    }
    char *envp[48];
    int ne = 0;
    for (int i = 0; environ && environ[i] && ne < 38; i++) envp[ne++] = environ[i];
    for (int i = 0; i < nvars[t->tabela]; i++) envp[ne++] = vars[t->tabela][i];
    envp[ne] = NULL;
    int saida = abrir(arq_saida, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_ACRESCENTAR);
    if (saida >= 0) {
        char h[32];
        hora_texto(h, sizeof h);
        imprimir_em(saida, "== %s %s (%s, línea %d): %s ==\n", h, motivo, tabelas[t->tabela], t->t.linha, t->t.comando);
    }
    int p[2];
    bool tem_pipe = criar_pipe(p) == 0;
    if (tem_pipe) {
        fechar(p[1]);              /* entrada vacía: leer da fin de archivo */
        duplicar(p[0], ENTRADA);
    }
    if (saida >= 0) { duplicar(saida, SAIDA); duplicar(saida, ERROS); }
    char cmd[CRON_COMANDO_MAX];
    copia_texto(cmd, t->t.comando, sizeof cmd);
    char *argv[] = { "sagsh", "-c", cmd, NULL };
    /* D255: las tareas de la tabla del usuario (/home/.config/crontab) corren
     * como sagui aunque el cron sea root; las de /etc/crontab, como el cron. */
    if (t->tabela == 1 && (usuario_op(USU_CONSULTAR, 0, 0) & 0xFFFF) == UID_ROOT)
        usuario_op(USU_COMO, UID_SAGUI, GID_SAGUI);
    int pid = executar_env("/bin/sagsh", argv, envp);
    usuario_op(USU_COMO, -1, 0);
    /* Los de cron otra vez. */
    duplicar(FD_AVISO, ENTRADA);
    duplicar(FD_SAIDA, SAIDA);
    duplicar(FD_SAIDA, ERROS);
    if (tem_pipe) fechar(p[0]);
    if (saida >= 0) fechar(saida);
    if (pid < 0) {
        anotar("no se pudo ejecutar «%s»: %s", t->t.comando, nome_erro(pid));
        return;
    }
    definir_grupo(pid, 0);
    execs[k].pid = pid;
    execs[k].inicio = tempo_ms();
    copia_texto(execs[k].comando, t->t.comando, sizeof execs[k].comando);
    anotar("inicio pid %d (%s): %s", pid, motivo, t->t.comando);
    if (t->t.q.recuperar && recuperar_ok) estado_por(t->chave, hora_utc(NULL));
}

static void terminou(int pid, int codigo)
{
    for (int k = 0; k < MAX_EXEC; k++)
        if (execs[k].pid == pid) {
            unsigned ms = tempo_ms() - execs[k].inicio;
            anotar("fin pid %d, código %d, %u,%u s: %s", pid, codigo, ms / 1000, ms % 1000 / 100, execs[k].comando);
            execs[k].pid = 0;
            return;
        }
}

/* Las @reboot van una vez por arranque del sistema, no cada vez que el
 * servicio cron se reinicia: se apunta en /tmp/.servicios/cron.arranque cuándo arrancó
 * el sistema (UTC; un /tmp persistente guarda el de la sesión anterior, que no coincide). */
static bool primeira_vez_neste_arranque(void)
{
    const char *marca = SERV_DIR_COMUM "/cron.arranque";
    servicos_criar_pastas(SERV_DIR_COMUM);
    unsigned arranque = hora_utc(NULL) - tempo_ms() / 1000u, visto = 0;
    int fd = abrir(marca, ABRIR_LER);
    if (fd >= 0) {
        char b[24];
        int n = ler(fd, b, sizeof b - 1);
        fechar(fd);
        b[n > 0 ? n : 0] = 0;
        visto = (unsigned)texto_para_int(b);
    }
    if (visto && (visto > arranque ? visto - arranque : arranque - visto) <= 3) return false;
    fd = abrir(marca, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd >= 0) { imprimir_em(fd, "%u\n", arranque); fechar(fd); }
    return true;
}

/* ------------------------------------------------------------ cron proximas y --proxima */

static void mostrar_hora(uint32_t utc, char *t, int max)
{
    fuso_data_t d;
    hora_local(utc, &d);
    formatar_data(t, max, "%Y-%m-%d %H:%M %Z", &d);
}

static int proximas(void)
{
    uint32_t agora = hora_utc(NULL);
    if (!qtd) { imprimir("No hay tareas en %s ni en %s.\n", tabelas[0], tabelas[1]); return 0; }
    for (int i = 0; i < qtd; i++) {
        char t[48];
        if (tar[i].t.q.especial == CRON_ARRANQUE) copia_texto(t, "al arrancar", sizeof t);
        else {
            uint32_t p = cron_proxima(&tar[i].t.q, agora);
            if (p) mostrar_hora(p, t, sizeof t);
            else copia_texto(t, "nunca (en 5 años)", sizeof t);
        }
        imprimir("%-24s %s  (%s:%d)\n", t, tar[i].t.comando, tabelas[tar[i].tabela], tar[i].t.linha);
    }
    return 0;
}

static int proxima_expr(int argc, char **argv)
{
    if (argc < 3) { imprimir_em(ERROS, "uso: cron --proxima \"m h d M s\" [DESDE [N]]\n"); return 2; }
    char linha[200];
    formatar_texto(linha, sizeof linha, "%s x", argv[2]);
    cron_tarefa_t t;
    char erro[160];
    if (cron_analisar_linha(linha, &t, erro, sizeof erro) != 1) { imprimir_em(ERROS, "cron: %s\n", erro); return 1; }
    uint32_t desde = argc > 3 ? (uint32_t)texto_para_int(argv[3]) : hora_utc(NULL);
    int n = argc > 4 ? texto_para_int(argv[4]) : 1;
    if (n < 1) n = 1;
    for (int i = 0; i < n; i++) {
        uint32_t p = cron_proxima(&t.q, desde);
        if (!p) { imprimir("proxima: nunca\n"); return 0; }
        char h[48];
        mostrar_hora(p, h, sizeof h);
        imprimir("proxima: %u %s\n", p, h);
        desde = p;
    }
    return 0;
}

static const char *AYUDA =
    "uso: cron | cron proximas | cron --proxima \"m h d M s\" [DESDE [N]]\n"
    "Ejecuta las tareas de /etc/crontab y /home/.config/crontab a su hora (zona del\n"
    "sistema). Normalmente lo arranca el gestor de servicios (servicio estado cron).\n"
    "  proximas   lista cuándo se va a ejecutar cada tarea\n"
    "  --proxima  las N próximas horas de una expresión desde DESDE (segundos UTC)\n"
    "Formato de las tablas: minuto hora día mes día_semana orden, o @reboot,\n"
    "@hourly, @daily, @weekly, @monthly, @yearly (con ! delante: sin recuperar las\n"
    "tareas perdidas). Para cambiar la tabla del usuario: crontab -e.\n"
    "Registros: /logs/cron.log y /logs/cron-salida.log (o en /tmp/.servicios).\n";

int principal(int argc, char **argv)
{
    const char *pasta = servicos_pasta_registros();
    formatar_texto(arq_log, sizeof arq_log, "%s/cron.log", pasta);
    formatar_texto(arq_saida, sizeof arq_saida, "%s/cron-salida.log", pasta);
    if (argc > 1) {
        if (compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "--help") == 0 ||
            compara_texto(argv[1], "-h") == 0) { escrever_texto(SAIDA, AYUDA); return 0; }
        if (compara_texto(argv[1], "--proxima") == 0) return proxima_expr(argc, argv);
        if (compara_texto(argv[1], "proximas") == 0 || compara_texto(argv[1], "próximas") == 0) {
            for (int k = 0; k < 2; k++) {   /* sin anotar nada: solo leer */
                int fd = abrir(tabelas[k], ABRIR_LER);
                if (fd < 0) continue;
                unsigned tam;
                char *b = ler_todo_fd(fd, &tam);
                fechar(fd);
                if (!b) continue;
                int num = 0;
                for (char *l = b; l && *l && qtd < MAX_TAREFAS;) {
                    char *fim = acha_char(l, '\n');
                    if (fim) *fim = 0;
                    num++;
                    char erro[160];
                    if (cron_analisar_linha(l, &tar[qtd].t, erro, sizeof erro) == 1) {
                        tar[qtd].t.linha = num;
                        tar[qtd++].tabela = k;
                    }
                    l = fim ? fim + 1 : NULL;
                }
                libera(b);
            }
            return proximas();
        }
        imprimir_em(ERROS, "cron: opción «%s» desconocida (mira cron --ayuda)\n", argv[1]);
        return 2;
    }

    /* El servicio. */
    duplicar(ENTRADA, FD_AVISO);
    duplicar(SAIDA, FD_SAIDA);
    servicos_criar_pastas(pasta);
    recuperar_ok = servicos_montado("/home");
    if (recuperar_ok) {
        servicos_criar_pastas("/home/.config/cron");
        copia_texto(arq_estado, "/home/.config/cron/estado", sizeof arq_estado);
    } else copia_texto(arq_estado, SERV_DIR_COMUM "/cron.estado", sizeof arq_estado);
    mudar_pasta(recuperar_ok ? "/home" : "/");
    anotar("en marcha (pid %d); tareas perdidas: %s", meu_pid(),
           recuperar_ok ? "se recuperan (/home persistente)" : "no se recuperan (/home no es persistente)");
    ler_tabelas();
    estado_ler();
    uint32_t agora = hora_utc(NULL);
    bool primeira = primeira_vez_neste_arranque();
    if (!primeira) anotar("ya se había ejecutado en este arranque: las @reboot no se repiten");
    for (int i = 0; i < qtd; i++) {
        const tarefa_t *t = &tar[i];
        if (t->t.q.especial == CRON_ARRANQUE) { if (primeira) executar_tarefa(t, "@reboot"); }
        else if (t->t.q.recuperar && recuperar_ok) {
            uint32_t ultima = estado_obter(t->chave), periodo = cron_periodo(t->t.q.especial);
            if (!ultima || agora - ultima >= periodo) {
                anotar("tarea perdida (última vez: %s): %s", ultima ? "hace más de un periodo" : "nunca", t->t.comando);
                executar_tarefa(t, "tarea perdida");
            }
        }
    }
    uint32_t ultimo_min = agora / 60;
    unsigned ultima_vista = tempo_ms();
    for (;;) {
        /* El supervisor pide parar cerrando la entrada. */
        int d = disponivel(FD_AVISO);
        if (d == ERRO_PIPE_QUEBRADO) {
            anotar("parado (lo pidió el supervisor)");
            return 0;
        }
        if (d > 0) { char lixo[64]; ler(FD_AVISO, lixo, sizeof lixo); }
        int codigo, pid;
        while ((pid = recolher(&codigo)) > 0) terminou(pid, codigo);
        agora = hora_utc(NULL);
        uint32_t m = agora / 60;
        if (m > ultimo_min) {
            /* Cada minuto nuevo; si el reloj saltó más de 5 minutos, solo el actual. */
            uint32_t desde = m - ultimo_min > 5 ? m : ultimo_min + 1;
            for (uint32_t k = desde; k <= m; k++) {
                fuso_data_t dl;
                hora_local(k * 60, &dl);
                for (int i = 0; i < qtd; i++)
                    if (cron_casa(&tar[i].t.q, &dl)) executar_tarefa(&tar[i], "a su hora");
            }
            ultimo_min = m;
        } else if (m < ultimo_min) ultimo_min = m;   /* el reloj fue hacia atrás: nada se repite */
        if (tempo_ms() - ultima_vista >= 2000) {
            ultima_vista = tempo_ms();
            if (tabela_mudou(0, false) || tabela_mudou(1, false)) {
                anotar("las tablas han cambiado: se vuelven a leer");
                ler_tabelas();
            }
        }
        dormir(500);
    }
}
