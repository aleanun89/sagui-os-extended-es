// SPDX-License-Identifier: GPL-2.0-or-later
/* zona - zona horaria, reloj de la placa y la ventana "Fecha y hora" (D080).
 *
 *   zona                      muestra la zona, la hora local y UTC, el RTC y el próximo
 *                             cambio de horario
 *   zona lista [filtro]       las zonas de la tabla (filtro en el nombre o en el país)
 *   zona Europe/Madrid        cambia la zona (y graba)
 *   zona --tz 'CET-1CEST,...' una regla POSIX TZ a mano
 *   zona --rtc utc|local      cómo guarda la hora el reloj de la placa
 *   zona --formato 24|12      reloj de la barra en 24 o 12 horas
 *   zona --ajustar 'AAAA-MM-DD HH:MM[:SS]'   ajusta la hora (local) y graba el RTC
 *   zona --ventana            la ventana "Fecha y hora"
 *
 * El kernel aplica (SIS_ZONA) y este programa graba la configuración en
 * /etc/zona y, si /home es un disco persistente (D040), en /home/.zona, que
 * es lo que el kernel lee primero en el arranque. Sin disco persistente, avisa que la
 * elección se pierde al reiniciar. */
#include "sagui.h"
#include "gui.h"
#include "contas.h"

#define ARQ_RAM         "/etc/zona"
#define ARQ_PERSISTENTE "/home/.zona"

static config_zona_t cfg;

static bool home_persistente(void)
{
    info_montagem_t m[10];
    int n = info_sistema(INFO_MONTAGENS, m, 10);
    for (int i = 0; i < n && i < 10; i++)
        if (compara_texto(m[i].ponto, "/home") == 0 && !(m[i].opcoes & MONTAR_SO_LEITURA)) return true;
    return false;
}

static int gravar_arquivo(const char *caminho)
{
    char t[400];
    formatar_texto(t, sizeof t,
                   "# Zona horaria de SaguiOS (la escribe el programa \"zona\"; ver D080).\n"
                   "# zona: nombre de la tabla; tz: regla POSIX TZ; rtc: utc o local\n"
                   "# (como guarda la hora el reloj de la placa); formato: 24 o 12.\n"
                   "zona=%s\ntz=%s\n%s%s%sformato=%s\n",
                   cfg.nome, cfg.tz, cfg.rtc_auto ? "" : "rtc=", cfg.rtc_auto ? "" : (cfg.rtc_local ? "local" : "utc"),
                   cfg.rtc_auto ? "" : "\n", cfg.formato_24h ? "24" : "12");
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int r = escrever_tudo(fd, t, (unsigned)tam_texto(t));
    fechar(fd);
    return r < 0 ? r : 0;
}

/* Aplica en el kernel y graba. Devuelve 1 si quedó en un disco persistente, 0 si
 * solo en la RAM, <0 si el kernel rechazó. */
static int aplicar_e_gravar(void)
{
    /* D269: la zona y /etc/zona son del sistema: se pide permiso con el diálogo y se suelta al acabar. */
    if (!sou_root() && elevar_grafico("cambiar la zona horaria del sistema") != 0) return ERRO_ACESSO;
    int r = fixar_zona(&cfg);
    if (r < 0) { elevacao_soltar(); return r; }
    ler_zona(&cfg);
    gravar_arquivo(ARQ_RAM);
    bool persistente = home_persistente() && gravar_arquivo(ARQ_PERSISTENTE) == 0;
    registrar("zona: aplicada %s (%s), rtc %s, %s h, %s", cfg.nome, cfg.tz, cfg.rtc_local ? "local" : "utc",
              cfg.formato_24h ? "24" : "12", persistente ? "gravada em " ARQ_PERSISTENTE : "so na RAM");
    elevacao_soltar();
    return persistente ? 1 : 0;
}

static const char *aviso_ram =
    "Aviso: /home no es un disco persistente; la configuración se guarda solo en RAM\n"
    "(/etc/zona) y se perderá al reiniciar (ver \"make imagem-persistente\").\n";

static void texto_desvio(char *s, int max, int desvio)
{
    int a = desvio < 0 ? -desvio : desvio;
    if (a % 3600) formatar_texto(s, (size_t)max, "UTC%c%02d:%02d", desvio < 0 ? '-' : '+', a / 3600, a / 60 % 60);
    else formatar_texto(s, (size_t)max, "UTC%c%02d:00", desvio < 0 ? '-' : '+', a / 3600);
}

static const char *nome_para_mostrar(const char *nome)
{
    const zona_info_t *z = zona_achar(nome);
    return z ? z->mostrar : "regla propia";
}

/* El próximo instante en que el desvío cambia (hasta un año y medio); 0 = ninguno. */
static uint32_t proxima_mudanca(const fuso_t *f, uint32_t t)
{
    if (!f->tem_verao) return 0;
    int32_t d = fuso_desvio(f, t, NULL);
    for (uint32_t a = t, b; a < t + 550u * 86400; a = b) {
        b = a + 3600;
        if (fuso_desvio(f, b, NULL) == d) continue;
        while (b - a > 1) {
            uint32_t m = a + (b - a) / 2;
            if (fuso_desvio(f, m, NULL) == d) a = m; else b = m;
        }
        return b;
    }
    return 0;
}

static const char *nome_origem(unsigned o)
{
    switch (o) {
    case ZONA_PERSISTENTE: return ARQ_PERSISTENTE " (disco persistente)";
    case ZONA_ARQUIVO:     return ARQ_RAM " (solo en RAM)";
    case ZONA_PROGRAMA:    return "cambiada en esta sesión";
    default:               return "sin configurar (UTC)";
    }
}

static int mostrar(void)
{
    ler_zona(&cfg);
    hora_t h;
    uint32_t t = hora_utc(&h);
    const char *nome_proc;
    const fuso_t *f = zona_do_processo(&nome_proc);
    fuso_data_t l, u;
    hora_local(t, &l);
    fuso_decompor(t, &u);
    copia_texto(u.abrev, "UTC", sizeof u.abrev);
    char a[80], b[80], dv[16];
    formatar_data(a, sizeof a, "%A, %d/%m/%Y %H:%M:%S %Z", &l);
    formatar_data(b, sizeof b, "%A, %d/%m/%Y %H:%M:%S %Z", &u);
    texto_desvio(dv, sizeof dv, l.desvio);
    imprimir("Zona:            %s - %s\n", cfg.nome, nome_para_mostrar(cfg.nome));
    imprimir("Regla (POSIX TZ): %s\n", cfg.tz);
    const char *tz = obter_env("TZ");
    if (tz && *tz) imprimir("Variable TZ:     %s (manda en este proceso: %s)\n", tz, nome_proc);
    imprimir("Hora local:      %s (%s%s)\n", a, dv, l.verao ? ", horario de verano" : "");
    imprimir("Hora UTC:        %s\n", b);
    uint32_t m = proxima_mudanca(f, t);
    if (m) {
        fuso_data_t antes, depois, parede;
        hora_local(m - 1, &antes);
        hora_local(m, &depois);
        fuso_decompor(m + (uint32_t)antes.desvio, &parede);   /* el reloj de antes, en el instante del cambio */
        char x[64];
        formatar_data(x, sizeof x, "%d/%m/%Y", &depois);
        imprimir("Próximo cambio:  %s, %02u:%02u %s -> %02u:%02u %s\n", x, parede.hora, parede.minuto, antes.abrev,
                 depois.hora, depois.minuto, depois.abrev);
    } else imprimir("Próximo cambio:  ninguno (sin horario de verano)\n");
    data_t r;
    ler_rtc(&r);
    imprimir("Reloj del PC:    guarda la hora %s%s; ahora tiene %02u/%02u/%u %02u:%02u:%02u\n",
             cfg.rtc_local ? "LOCAL" : "UTC",
             cfg.rtc_auto ? (cfg.rtc_local ? " (automático: PC real, como Windows)" : " (automático: máquina virtual)") : "",
             r.dia, r.mes, r.ano, r.hora, r.minuto, r.segundo);
    imprimir("Formato:         %s\n", cfg.formato_24h ? "24 horas" : "12 horas (a. m./p. m.)");
    imprimir("Configuración:   %s\n", nome_origem(cfg.origem));
    return 0;
}

static int listar_zonas(const char *filtro)
{
    int n;
    const zona_info_t *z = zona_tabela(&n);
    uint32_t t = hora_utc(NULL);
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (filtro) {
            /* sin distinguir mayúsculas, en el nombre o en el nombre para mostrar */
            char a[96], f[64];
            formatar_texto(a, sizeof a, "%s %s", z[i].nome, z[i].mostrar);
            copia_texto(f, filtro, sizeof f);
            for (char *p = a; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
            for (char *p = f; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
            bool achou = false;
            size_t nf = tam_texto(f);
            for (char *p = a; *p && !achou; p++) achou = compara_texto_n(p, f, nf) == 0;
            if (!achou) continue;
        }
        fuso_t fz;
        fuso_ler(z[i].tz, &fz);
        fuso_data_t l;
        fuso_local(&fz, t, &l);
        char dv[16];
        texto_desvio(dv, sizeof dv, l.desvio);
        imprimir("%-31s %-9s %02u:%02u %-6s %s\n", z[i].nome, dv, l.hora, l.minuto, l.abrev, z[i].mostrar);
        k++;
    }
    if (!k) { imprimir_em(ERROS, "zona: ninguna zona con \"%s\"\n", filtro); return 1; }
    return 0;
}

static int trocar(const char *nome, const char *tz)
{
    ler_zona(&cfg);
    if (tz) {
        fuso_t f;
        if (!fuso_ler(tz, &f)) { imprimir_em(ERROS, "zona: regla POSIX TZ no válida: %s\n", tz); return 1; }
        copia_texto(cfg.nome, tz, sizeof cfg.nome);
        copia_texto(cfg.tz, tz, sizeof cfg.tz);
    } else {
        const zona_info_t *z = zona_achar(nome);
        if (!z) {
            imprimir_em(ERROS, "zona: no conozco \"%s\" (mira \"zona lista\"; o usa --tz con una regla POSIX)\n", nome);
            return 1;
        }
        copia_texto(cfg.nome, z->nome, sizeof cfg.nome);
        copia_texto(cfg.tz, z->tz, sizeof cfg.tz);
    }
    int r = aplicar_e_gravar();
    if (r < 0) { imprimir_em(ERROS, "zona: %s\n", nome_erro(r)); return 1; }
    if (r == 0) imprimir_em(ERROS, "%s", aviso_ram);
    return mostrar();
}

/* "AAAA-MM-DD HH:MM[:SS]" (o con / en lugar de -) -> segundos locales. */
static bool ler_data_hora(const char *s, uint32_t *local)
{
    int v[6] = { 0, 0, 0, 0, 0, 0 }, k = 0;
    for (const char *p = s; *p && k < 6; ) {
        if (*p < '0' || *p > '9') { p++; continue; }
        int n = 0;
        while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
        v[k++] = n;
    }
    if (k < 5 || v[0] < 2000 || v[0] > 2099 || v[1] < 1 || v[1] > 12 || v[2] < 1 || v[2] > fuso_dias_mes(v[0], v[1]) ||
        v[3] > 23 || v[4] > 59 || v[5] > 59)
        return false;
    *local = fuso_compor(v[0], v[1], v[2], v[3], v[4], v[5]);
    return true;
}

static int ajustar(const char *texto, char *msg, int max)
{
    uint32_t local;
    if (!ler_data_hora(texto, &local)) {
        formatar_texto(msg, (size_t)max, "Fecha no válida: \"%s\" (formato AAAA-MM-DD HH:MM:SS)", texto);
        return 1;
    }
    int situacao;
    uint32_t utc = local_para_utc(local, &situacao);
    int r = COM_ADMIN("cambiar la fecha y la hora del sistema", ajustar_hora(utc, HORA_GRAVAR_RTC));
    if (r < 0) { formatar_texto(msg, (size_t)max, "No se pudo ajustar la hora: %s", nome_erro(r)); return 1; }
    fuso_data_t l;
    hora_local(utc, &l);
    char t[64];
    formatar_data(t, sizeof t, "%d/%m/%Y %H:%M:%S %Z", &l);
    formatar_texto(msg, (size_t)max, "Hora ajustada: %s%s", t,
                   situacao == FUSO_INEXISTENTE ? " (esa hora no existe ese día: se adelantó)" :
                   situacao == FUSO_REPETIDA ? " (hora repetida: se tomó la primera)" : "");
    registrar("zona: hora ajustada para %u (%s)", utc, t);
    return 0;
}

/* ------------------------------------------------------------ ventana */

#define LARG 600
#define ALT  440
#define MAXZ 128

static jan_t *j;
static caixa_t busca;
static char itens[MAXZ][64];
static int indices[MAXZ];
static lista_t lista;
static botao_t b_rtc_utc   = { 16, 0, 96, 26, "UTC", false };
static botao_t b_rtc_local = { 118, 0, 132, 26, "Hora local", false };
static botao_t b_formato   = { 252, 0, 150, 26, "", false };
static botao_t b_ajustar   = { 16, 0, 200, 28, "Ajustar fecha y hora...", false };
static botao_t b_aplicar   = { 0, 0, 110, 28, "Aplicar", false };
static botao_t b_cerrar    = { 0, 0, 110, 28, "Cerrar", false };
static char estado[160];
static int sel_tabela = -1;       /* zona elegida (índice en la tabla) */

#define Y_RELOGIO 10
#define Y_BUSCA   72
#define Y_LISTA   104
#define ALT_LISTA (LISTA_ALTURA_ITEM * 9 + 4)
#define Y_RTC     (Y_LISTA + ALT_LISTA + 26)
#define Y_BOTOES  (ALT - 40)

static void filtrar(void)
{
    int n;
    const zona_info_t *z = zona_tabela(&n);
    uint32_t t = hora_utc(NULL);
    char f[64];
    copia_texto(f, busca.texto, sizeof f);
    for (char *p = f; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    size_t nf = tam_texto(f);
    lista.qtd = 0;
    lista.sel = -1;
    for (int i = 0; i < n && lista.qtd < MAXZ; i++) {
        char a[96];
        formatar_texto(a, sizeof a, "%s %s", z[i].nome, z[i].mostrar);
        for (char *p = a; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
        bool achou = nf == 0;
        for (char *p = a; *p && !achou; p++) achou = compara_texto_n(p, f, nf) == 0;
        if (!achou) continue;
        fuso_t fz;
        fuso_ler(z[i].tz, &fz);
        char dv[16];
        texto_desvio(dv, sizeof dv, fuso_desvio(&fz, t, NULL));
        formatar_texto(itens[lista.qtd], sizeof itens[0], "%-10s %s", dv, z[i].mostrar);
        indices[lista.qtd] = i;
        if (i == sel_tabela) lista.sel = lista.qtd;
        lista.qtd++;
    }
    if (lista.sel >= 0 && lista.sel < lista.topo) lista.topo = lista.sel;
    int vis = (lista.a - 4) / LISTA_ALTURA_ITEM;
    if (lista.sel >= lista.topo + vis) lista.topo = lista.sel - vis + 1;
    if (lista.topo > lista.qtd - vis) lista.topo = lista.qtd - vis;
    if (lista.topo < 0) lista.topo = 0;
}

static void desenhar_relogio(void)
{
    g_retangulo(j, 0, Y_RELOGIO, LARG, 52, COR_FUNDO);
    uint32_t t = hora_utc(NULL);
    fuso_data_t l, u;
    hora_local(t, &l);
    fuso_decompor(t, &u);
    char a[96], b[96], c[160];
    formatar_data(a, sizeof a, "%A, %d de %B de %Y", &l);
    formatar_data(b, sizeof b, cfg.formato_24h ? "%H:%M:%S %Z" : "%I:%M:%S %p %Z", &l);
    formatar_texto(c, sizeof c, "Hora local: %s, %s", a, b);
    g_texto(j, 16, Y_RELOGIO, c, COR_TEXTO);
    g_texto(j, 17, Y_RELOGIO, c, COR_TEXTO);
    formatar_data(a, sizeof a, "%d/%m/%Y %H:%M:%S", &u);
    formatar_texto(c, sizeof c, "UTC:        %s      Zona: %s", a, cfg.nome);
    g_texto(j, 16, Y_RELOGIO + 22, c, COR_TEXTO_FRACO);
    jan_mostrar_area(j, 0, Y_RELOGIO, LARG, 52);
}

static void desenhar_tudo(void)
{
    g_retangulo(j, 0, 0, LARG, ALT, COR_FUNDO);
    desenhar_relogio();
    g_texto(j, 16, Y_BUSCA + 4, "Buscar:", COR_TEXTO);
    caixa_desenhar(j, &busca);
    lista_desenhar(j, &lista);
    g_texto(j, 16, Y_RTC - 20, "El reloj del PC (RTC) guarda la hora en:", COR_TEXTO);
    b_rtc_utc.texto = cfg.rtc_local ? "UTC" : "(*) UTC";
    b_rtc_local.texto = cfg.rtc_local ? "(*) Hora local" : "Hora local";
    b_formato.texto = cfg.formato_24h ? "Formato: 24 horas" : "Formato: 12 horas";
    botao_desenhar(j, &b_rtc_utc);
    botao_desenhar(j, &b_rtc_local);
    botao_desenhar(j, &b_formato);
    g_texto(j, 16, Y_RTC + 34, cfg.rtc_auto ? "(elegido automáticamente: UTC en máquinas virtuales, hora local en un PC)"
                                            : "(Windows guarda la hora local; Linux y las máquinas virtuales, UTC)",
            COR_TEXTO_FRACO);
    botao_desenhar(j, &b_ajustar);
    botao_desenhar(j, &b_aplicar);
    botao_desenhar(j, &b_cerrar);
    g_texto_n(j, 16, Y_BOTOES - 22, estado, (LARG - 32) / LETRA_L, COR_TEXTO_FRACO);
    jan_mostrar(j);
}

static void status_persistencia(int r)
{
    if (r < 0) formatar_texto(estado, sizeof estado, "Error: %s", nome_erro(r));
    else if (r == 0) copia_texto(estado, "Guardado solo en RAM (/etc/zona): se perderá al reiniciar.", sizeof estado);
    else copia_texto(estado, "Guardado en /home/.zona: se mantiene al reiniciar.", sizeof estado);
}

static void aplicar_selecao(void)
{
    if (lista.sel < 0) { copia_texto(estado, "Elige una zona de la lista.", sizeof estado); return; }
    int n;
    const zona_info_t *z = zona_tabela(&n);
    sel_tabela = indices[lista.sel];
    copia_texto(cfg.nome, z[sel_tabela].nome, sizeof cfg.nome);
    copia_texto(cfg.tz, z[sel_tabela].tz, sizeof cfg.tz);
    status_persistencia(aplicar_e_gravar());
}

static int janela(void)
{
    ler_zona(&cfg);
    int n;
    zona_tabela(&n);
    const zona_info_t *atual = zona_achar(cfg.nome);
    sel_tabela = -1;
    if (atual) { int k; const zona_info_t *z = zona_tabela(&k); sel_tabela = (int)(atual - z); }
    j = jan_abrir("Fecha y hora", LARG, ALT);
    if (!j) { imprimir_em(ERROS, "zona: no se pudo abrir la ventana\n"); return 1; }
    busca = (caixa_t){ 80, Y_BUSCA, LARG - 96, "", 0, false };
    lista = (lista_t){ 16, Y_LISTA, LARG - 32, ALT_LISTA, itens, 0, -1, 0, NULL };
    b_rtc_utc.y = b_rtc_local.y = b_formato.y = Y_RTC;
    b_formato.x = LARG - 16 - b_formato.l;
    b_ajustar.y = b_aplicar.y = b_cerrar.y = Y_BOTOES;
    b_cerrar.x = LARG - 16 - b_cerrar.l;
    b_aplicar.x = b_cerrar.x - 10 - b_aplicar.l;
    filtrar();
    estado[0] = 0;
    if (!home_persistente()) copia_texto(estado, "Sin disco persistente: los cambios se guardan solo en RAM.", sizeof estado);
    desenhar_tudo();
    registrar("zona: ventana pronta (zona %s, rtc %s)", cfg.nome, cfg.rtc_local ? "local" : "utc");
    unsigned ultimo = tempo_ms();
    evento_t e;
    for (;;) {
        if (proximo_evento(j->id, &e, false) == 1) {
            if (e.tipo == EV_FECHAR) break;
            if (botao_tratar(j, &b_cerrar, &e)) break;
            bool redesenhar = false;
            if (botao_tratar(j, &b_aplicar, &e)) { aplicar_selecao(); filtrar(); redesenhar = true; }
            else if (botao_tratar(j, &b_rtc_utc, &e) || botao_tratar(j, &b_rtc_local, &e)) {
                cfg.rtc_local = e.x >= b_rtc_local.x;
                cfg.rtc_auto = 0;
                status_persistencia(aplicar_e_gravar());
                redesenhar = true;
            } else if (botao_tratar(j, &b_formato, &e)) {
                cfg.formato_24h = !cfg.formato_24h;
                status_persistencia(aplicar_e_gravar());
                redesenhar = true;
            } else if (botao_tratar(j, &b_ajustar, &e)) {
                fuso_data_t l;
                agora_local(&l);
                char t[64];
                formatar_data(t, sizeof t, "%Y-%m-%d %H:%M:%S", &l);
                if (pedir_texto("Ajustar fecha y hora", "Fecha y hora local (AAAA-MM-DD HH:MM:SS):", t, sizeof t)) {
                    ajustar(t, estado, sizeof estado);
                    filtrar();
                }
                redesenhar = true;
            } else if (e.tipo == EV_TECLA || (e.tipo == EV_CLIQUE && e.y >= busca.y && e.y < busca.y + LETRA_A + 8)) {
                if (e.tipo == EV_CLIQUE) busca.foco = e.x >= busca.x && e.x < busca.x + busca.l;
                char antes[128];
                copia_texto(antes, busca.texto, sizeof antes);
                bool enter = busca.foco && caixa_tratar(j, &busca, &e);
                if (compara_texto(antes, busca.texto) != 0) {
                    filtrar();
                    if (lista.qtd == 1) lista.sel = 0;
                    registrar("zona: busca \"%s\": %d zonas", busca.texto, lista.qtd);
                    redesenhar = true;
                } else if (!busca.foco || e.tecla == TECLA_CIMA || e.tecla == TECLA_BAIXO) {
                    if (lista_tratar(j, &lista, &e) == LISTA_ATIVOU) { aplicar_selecao(); filtrar(); redesenhar = true; }
                }
                if (enter) { aplicar_selecao(); filtrar(); redesenhar = true; }
                if (!redesenhar) caixa_desenhar(j, &busca), jan_mostrar(j);
            } else {
                if (e.tipo == EV_CLIQUE) busca.foco = false;
                int r = lista_tratar(j, &lista, &e);
                if (r == LISTA_ATIVOU) { aplicar_selecao(); filtrar(); redesenhar = true; }
                else if (r == LISTA_SELECIONOU && lista.sel >= 0) {
                    int k;
                    const zona_info_t *z = zona_tabela(&k);
                    registrar("zona: seleccionada %s", z[indices[lista.sel]].nome);
                }
            }
            if (redesenhar) desenhar_tudo();
            continue;
        }
        if (tempo_ms() - ultimo >= 1000) {
            ultimo = tempo_ms();
            ler_zona(&cfg);
            desenhar_relogio();
        }
        dormir(50);
    }
    jan_fechar(j);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc < 2) return mostrar();
    const char *a = argv[1];
    if (compara_texto(a, "--ventana") == 0) return janela();
    if (compara_texto(a, "lista") == 0) return listar_zonas(argc > 2 ? argv[2] : NULL);
    if (compara_texto(a, "--tz") == 0 && argc > 2) return trocar(NULL, argv[2]);
    if (compara_texto(a, "--rtc") == 0 && argc > 2) {
        ler_zona(&cfg);
        if (compara_texto(argv[2], "utc") == 0) cfg.rtc_local = 0;
        else if (compara_texto(argv[2], "local") == 0) cfg.rtc_local = 1;
        else if (compara_texto(argv[2], "auto") == 0) cfg.rtc_auto = 1;
        else { imprimir_em(ERROS, "uso: zona --rtc utc|local|auto\n"); return 2; }
        if (compara_texto(argv[2], "auto") != 0) cfg.rtc_auto = 0;
        int r = aplicar_e_gravar();
        if (r < 0) { imprimir_em(ERROS, "zona: %s\n", nome_erro(r)); return 1; }
        if (r == 0) imprimir_em(ERROS, "%s", aviso_ram);
        return mostrar();
    }
    if (compara_texto(a, "--formato") == 0 && argc > 2) {
        ler_zona(&cfg);
        cfg.formato_24h = compara_texto(argv[2], "12") != 0;
        int r = aplicar_e_gravar();
        if (r == 0) imprimir_em(ERROS, "%s", aviso_ram);
        return r < 0;
    }
    if (compara_texto(a, "--ajustar") == 0 && argc > 2) {
        char msg[160], texto[64];
        copia_texto(texto, argv[2], sizeof texto);
        if (argc > 3) { junta_texto(texto, " ", sizeof texto); junta_texto(texto, argv[3], sizeof texto); }
        ler_zona(&cfg);
        int r = ajustar(texto, msg, sizeof msg);
        imprimir_em(r ? ERROS : SAIDA, "%s\n", msg);
        return r;
    }
    if (a[0] == '-') {
        imprimir_em(ERROS, "uso: zona [lista [filtro] | ZONA | --tz REGLA | --rtc utc|local|auto | --formato 24|12 |\n"
                           "            --ajustar 'AAAA-MM-DD HH:MM:SS' | --ventana]\n");
        return 2;
    }
    return trocar(a, NULL);
}
