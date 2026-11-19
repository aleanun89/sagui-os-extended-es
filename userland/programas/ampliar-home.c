// SPDX-License-Identifier: GPL-2.0-or-later
/* ampliar-home - «Ampliar /home para usar todo el espacio libre» (D318, v8.4.3).
 *
 *   ampliar-home                    dice cuánto se puede ampliar /home (no cambia nada)
 *   ampliar-home --ventana          el diálogo (si hay algo que ampliar)
 *   ampliar-home --texto [todo|8G|16G|...] [-s]
 *                                   lo mismo en el terminal (D388): estimación del tiempo, elección del
 *                                   tamaño (todo, uno menor o ahora no) y barra de avance con el tiempo restante
 *   ampliar-home --primer-arranque  el diálogo del primer arranque: solo si el disco de arranque tiene
 *                                   al menos 512 MB libres que SaguiOS no usa, hay escritorio y no se
 *                                   ha contestado antes. Lo lanza el gestor de servicios
 *                                   (/etc/servicios/ampliar-home.conf, una vez por arranque).
 * El trabajo lo hace la libsagui (home_ampliar.c), igual que «particiones ampliar /home»: /home crece
 * hasta el final del disco (D388: es la última lógica y crece sin mover nada; con la disposición de antes,
 * las particiones de detrás se mueven al final, copiadas y releídas antes de cambiar la tabla) o, si /home
 * está en la RAM, se le crea una partición SAGUI-DATOS. Si para crecer habría que mover la raíz en uso
 * (la disposición de antes en la live), el diálogo lo explica y dice que hay que regrabar o reinstalar.
 * «No volver a preguntar» (y una ampliación hecha) se recuerda en SAGUI-CONF (o en /home/.config si no
 * hay SAGUI-CONF): /sistema/conf/ampliar-home.no. */
#include "sagui.h"
#include "contas.h"
#include "gui.h"
#include "particoes.h"

#define MARCA_CONF CONF_SISTEMA_PONTO "/ampliar-home.no"
#define MARCA_HOME "/home/.config/ampliar-home.no"
#define MINIMO (512u * 2048u)   /* lo que tiene que ganar /home para preguntar (sectores) */
#define L 540
#define A 340

static void tamanho(uint32_t setores, char *s, int max)
{
    uint32_t mb = setores / 2048;
    if (mb >= 1024) formatar_texto(s, (size_t)max, "%u,%u GB", mb / 1024, (mb % 1024) * 10 / 1024);
    else formatar_texto(s, (size_t)max, "%u MB", mb);
}

static const char *marca(void) { return conf_persistente() ? MARCA_CONF : MARCA_HOME; }

static bool ja_respondido(void)
{
    info_arquivo_t i;
    return info_arquivo(MARCA_CONF, &i) == 0 || info_arquivo(MARCA_HOME, &i) == 0;
}

static void lembrar(const char *porque)
{
    criar_pasta("/home/.config");
    int fd = abrir(marca(), ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd >= 0) { escrever_tudo(fd, porque, (unsigned)tam_texto(porque)); fechar(fd); }
    registrar("ampliar-home: no se volverá a preguntar (%s, %s)", marca(), porque);
}

/* ------------------------------------------------------------ ventana */

static jan_t *j;
static char estado[300];
static int pc_atual = -1;

/* D388: la estimación de lo que se va a escribir y el avance (porcentaje, MB y tiempo restante). */
static home_estim_t est_h;
static uint32_t t_ini;
static int pc_ult = -1;
static uint32_t mbs_med;                 /* velocidad medida (0 = aún no) */
static home_plano_t completo;            /* el plan de «todo»: las opciones de tamaño menor parten de él */
static uint32_t tam_op[4];               /* 0 = todo; 1.. = total de /home en sectores */
static int n_op, sel;
static char txt_est[260];

static void texto_estimacion(void)
{
    char t[24];
    home_texto_tempo(est_h.segundos, t, sizeof t);
    formatar_texto(txt_est, sizeof txt_est, "Hay que poner a cero unos %u MB (%u grupos nuevos). A unos %u MB/s tardará, como mucho, %s. "
                   "No apagues el equipo mientras tanto.", est_h.mb, est_h.grupos, est_h.mbs, t);
}

/* "NN %  X de Y MB  quedan unos N min" para el porcentaje pc. */
static void texto_avance(int pc, char *s, int max)
{
    uint32_t seg = est_h.segundos, ms = tempo_ms() - t_ini;
    if (pc >= 3 && ms > 3000) seg = (uint32_t)((uint64_t)ms * (uint32_t)(100 - pc) / (uint32_t)pc / 1000);
    else if (pc > 0) seg = seg * (uint32_t)(100 - pc) / 100;
    char t[24];
    home_texto_tempo(seg, t, sizeof t);
    formatar_texto(s, (size_t)max, "%d %%  -  %u de %u MB escritos  -  quedan %s", pc, (uint32_t)((uint64_t)est_h.mb * (uint32_t)pc / 100), est_h.mb, t);
}

/* Anota el avance (y reinicia el reloj si empieza una fase nueva: el porcentaje vuelve a bajar). Devuelve true
 * si hay que repintar (cambió el entero). */
static bool avance_nota(int pc)
{
    if (pc > 100) pc = 100;
    if (pc_ult < 0 || pc < pc_ult) t_ini = tempo_ms();
    if (pc == pc_ult) return false;
    bool nuevo10 = pc / 10 != pc_ult / 10 || pc == 100;
    pc_ult = pc;
    if (nuevo10) {
        char s[160];
        texto_avance(pc, s, sizeof s);
        registrar("ampliar-home: avance %s", s);
    }
    return true;
}

static void botao(int x, int y, int l, const char *t)
{
    g_retangulo(j, x, y, l, 26, W98_FACE);
    g_bisel(j, x, y, l, 26, false);
    g_texto(j, x + (l - g_largura_texto(t)) / 2, y + 5, t, W98_NEGRO);
}

static int linhas(int x, int y, const char *t, int n, uint32_t cor)
{
    char l[128];
    while (*t) {
        int k = 0, ult = -1;
        while (t[k] && t[k] != '\n' && k < n) { if (t[k] == ' ') ult = k; k++; }
        if (t[k] && t[k] != '\n' && ult > 0) k = ult;
        copia_texto(l, t, (size_t)(k + 1 < (int)sizeof l ? k + 1 : (int)sizeof l));
        g_texto(j, x, y, l, cor);
        y += 18;
        t += k;
        while (*t == ' ' || *t == '\n') t++;
    }
    return y;
}

enum { EST_PERGUNTA, EST_TRABALHANDO, EST_FIM, EST_BLOQUEADO };
static int est;
static bool ok_final;

static void desenhar(const home_plano_t *h)
{
    g_retangulo(j, 0, 0, L, A, W98_FACE);
    g_retangulo(j, 16, 16, 32, 32, 0x2E9E4F);   /* un disco verde: espacio para /home */
    g_bisel(j, 16, 16, 32, 32, false);
    g_texto(j, 24, 24, "+", W98_LUZ);
    char t[640], g[16], d[16];
    tamanho(h->ganho, g, sizeof g);
    info_bloco_t b;
    tamanho(blocos_info(h->disco, &b) == 0 ? b.setores : 0, d, sizeof d);
    if (est == EST_BLOQUEADO) {   /* D388: la disposición de antes en la live: se explica qué hacer */
        g_texto(j, 64, 16, "No se puede ampliar /home", W98_NEGRO);
        g_texto(j, 65, 16, "No se puede ampliar /home", W98_NEGRO);
        formatar_texto(t, sizeof t, "El disco de arranque (%s, %s) tiene %s libres, pero esta imagen tiene la disposición de antes: "
                       "detrás de /home están la raíz del sistema y su capa de cambios, que no se pueden mover con el sistema en marcha.\n"
                       "Graba la imagen nueva (deja /home al final) o instala SaguiOS en este disco con «instalar» desde un pendrive.", h->disco, d, g);
        linhas(64, 40, t, 56, W98_NEGRO);
        botao(L - 280, A - 42, 100, "Ahora no");
        botao(L - 170, A - 42, 160, "No volver a preguntar");
    } else if (est == EST_PERGUNTA) {
        g_texto(j, 64, 16, "Ampliar /home", W98_NEGRO);
        g_texto(j, 65, 16, "Ampliar /home", W98_NEGRO);
        char tam_atual[16];
        tamanho(h->criar ? 0 : h->plano.antes, tam_atual, sizeof tam_atual);
        if (h->criar)
            formatar_texto(t, sizeof t, "El disco de arranque (%s, %s) tiene %s libres que SaguiOS no está usando, y /home está en la RAM: "
                           "lo que guardes se pierde al apagar.\n¿Crear /home en ese espacio libre y conservar lo que hay ahora? Elige su tamaño.", h->disco, d, g);
        else
            formatar_texto(t, sizeof t, "El disco de arranque (%s, %s) tiene %s libres que SaguiOS no está usando.\n"
                           "¿Ampliar /home (ahora %s)? Elige hasta qué tamaño.%s%s", h->disco, d, g, tam_atual,
                           h->plano.nmover ? " Las particiones de detrás de /home se copian al final del disco y se comprueban antes de cambiar nada." : "",
                           h->limitado ? " Su tabla de grupos no admite más: es lo máximo posible (para usar todo el disco, graba la imagen nueva)." : "");
        int yy = linhas(64, 40, t, 56, W98_NEGRO);
        linhas(64, yy + 4, txt_est, 56, 0x802020);
        g_texto(j, 64, A - 100, "Tamaño de /home:", W98_NEGRO);
        for (int i = 0; i < n_op; i++) {
            char r[24];
            if (i) tamanho(tam_op[i], r, sizeof r); else { char g2[16]; tamanho(completo.total_novo, g2, sizeof g2); formatar_texto(r, sizeof r, "Todo (%s)", g2); }
            int x = 64 + i * 112, l = i ? 104 : 140;
            if (i) x += 36;
            g_retangulo(j, x, A - 80, l, 26, i == sel ? W98_LUZ : W98_FACE);
            g_bisel(j, x, A - 80, l, 26, i != sel);
            g_texto(j, x + (l - g_largura_texto(r)) / 2, A - 80 + 5, r, W98_NEGRO);
        }
        botao(L - 400, A - 42, 110, h->criar ? "Crear /home" : "Ampliar");
        botao(L - 280, A - 42, 100, "Ahora no");
        botao(L - 170, A - 42, 160, "No volver a preguntar");
    } else {
        g_texto(j, 64, 16, est == EST_FIM ? (ok_final ? "Hecho" : "No se ha podido") : "Ampliando /home...", W98_NEGRO);
        linhas(64, 40, estado, 56, est == EST_FIM && !ok_final ? 0xC00000 : W98_NEGRO);
        if (est == EST_TRABALHANDO) {
            char s[160];
            g_texto(j, 64, A - 140, "NO APAGUES EL EQUIPO hasta que termine.", 0xC00000);
            g_texto(j, 65, A - 140, "NO APAGUES EL EQUIPO hasta que termine.", 0xC00000);
            g_retangulo(j, 64, A - 100, L - 90, 20, W98_LUZ);
            g_bisel(j, 64, A - 100, L - 90, 20, true);
            g_retangulo(j, 67, A - 97, (L - 96) * (pc_atual < 0 ? 0 : pc_atual) / 100, 14, W98_AZUL);
            texto_avance(pc_atual < 0 ? 0 : pc_atual, s, sizeof s);
            g_texto(j, 64, A - 74, s, W98_NEGRO);
        }
        if (est == EST_FIM) botao(L - 120, A - 42, 100, "Aceptar");
    }
    jan_mostrar(j);
}

static home_plano_t *hp;
static void progresso(int pc, void *ctx)
{
    (void)ctx;
    if (pc > 100) pc = 100;
    if (!avance_nota(pc)) return;
    pc_atual = pc;
    desenhar(hp);
}

/* Opciones de tamaño: «todo» y hasta tres menores (8, 16, 32, 64 GB...) que ganen algo de verdad. */
static void opciones(const home_plano_t *h)
{
    static const uint32_t gb[] = { 8, 16, 32, 64, 128, 256 };
    uint32_t atual = h->criar ? 0 : h->plano.antes, todo = h->total_novo;
    n_op = 1;
    tam_op[0] = 0;
    for (unsigned i = 0; i < sizeof gb / sizeof gb[0] && n_op < 4; i++) {
        uint32_t set = gb[i] * 1024u * 2048u;
        if (set > atual + 512u * 2048u && set + 1024u * 2048u < todo) tam_op[n_op++] = set;
    }
}

/* Elige la opción k: rehace el plan y la estimación. */
static void escolher(home_plano_t *h, int k)
{
    home_plano_t novo;
    char m[200];
    if (k == 0) novo = completo;
    else if (home_planejar_ate(&novo, tam_op[k], m, sizeof m) < 0 || !novo.ganho || novo.bloqueado) return;
    *h = novo;
    sel = k;
    home_estimar(h, mbs_med, &est_h);
    mbs_med = est_h.mbs;
    texto_estimacion();
    registrar("ampliar-home: estimación (%s): %u grupos nuevos, %u MB a escribir, %u MB/s, %u s", k ? "tamaño menor" : "todo", est_h.grupos,
              est_h.mb, est_h.mbs, est_h.segundos);
}

static int janela(home_plano_t *h, bool primeiro)
{
    j = jan_abrir(primeiro ? "Primer arranque: espacio libre" : "Ampliar /home", L, A);
    if (!j) { registrar("ampliar-home: sin escritorio: no se pregunta"); return 0; }
    hp = h;
    est = h->bloqueado ? EST_BLOQUEADO : EST_PERGUNTA;
    completo = *h;
    if (!h->bloqueado) { opciones(h); escolher(h, 0); }
    desenhar(h);
    if (h->bloqueado)
        registrar("ampliar-home: no se puede: %u MB libres en %s con la raíz detrás de /home; botones y=%d ahora_no=%d nunca=%d",
                  h->ganho / 2048, h->disco, A - 42, L - 280 + 50, L - 170 + 80);
    else
        registrar("ampliar-home: pregunta: ganar %u MB (%s) en %s; botones y=%d ampliar=%d ahora_no=%d nunca=%d tamanos=%d y_tam=%d", h->ganho / 2048,
                  h->criar ? "crear" : "ampliar", h->disco, A - 42, L - 400 + 55, L - 280 + 50, L - 170 + 80, n_op, A - 80 + 13);
    evento_t e;
    for (;;) {
        if (esperar_evento(j->id, &e, 1000) != 1) continue;
        if (e.tipo == EV_FECHAR) { if (est != EST_TRABALHANDO) break; continue; }
        bool linha = e.tipo == EV_CLIQUE && e.y >= A - 42 && e.y < A - 16;
        if (est == EST_FIM) {
            if ((linha && e.x >= L - 120 && e.x < L - 20) || (e.tipo == EV_TECLA && (e.tecla == '\n' || e.tecla == TECLA_ESC))) break;
            continue;
        }
        if (est != EST_PERGUNTA && est != EST_BLOQUEADO) continue;
        if ((linha && e.x >= L - 280 && e.x < L - 180) || (e.tipo == EV_TECLA && e.tecla == TECLA_ESC)) { registrar("ampliar-home: ahora no"); break; }
        if (linha && e.x >= L - 170 && e.x < L - 10) { lembrar("no volver a preguntar"); break; }
        /* Solo con el ratón: este diálogo sale solo al arrancar y un Intro escrito para otra ventana no
         * debe cambiar el disco. */
        if (est == EST_PERGUNTA && e.tipo == EV_CLIQUE && e.y >= A - 80 && e.y < A - 54) {   /* D388: el tamaño */
            for (int i = 0; i < n_op; i++) {
                int x = 64 + i * 112 + (i ? 36 : 0), l = i ? 104 : 140;
                if (e.x >= x && e.x < x + l && i != sel) { escolher(h, i); desenhar(h); break; }
            }
            continue;
        }
        if (est == EST_PERGUNTA && linha && e.x >= L - 400 && e.x < L - 290) {
            est = EST_TRABALHANDO;
            copia_texto(estado, h->criar ? "Creando la partición de /home y copiando lo que hay..."
                                         : "Comprobando el sistema de archivos y ampliando /home...", sizeof estado);
            pc_ult = -1;
            pc_atual = 0;
            t_ini = tempo_ms();
            desenhar(h);
            registrar("ampliar-home: empieza (%u MB a escribir, estimado %u s)", est_h.mb, est_h.segundos);
            int r = home_ampliar(h, progresso, NULL, estado, sizeof estado);
            ok_final = r >= 0;
            if (ok_final) lembrar("hecho");
            registrar("ampliar-home: resultado %d (%u s): %s", r, (tempo_ms() - t_ini) / 1000, estado);
            est = EST_FIM;
            desenhar(h);
        }
    }
    jan_fechar(j);
    return ok_final ? 0 : 1;
}

/* ------------------------------------------------------------ terminal (D388) */

static void avance_terminal(int pc, void *ctx)
{
    (void)ctx;
    if (!avance_nota(pc)) return;
    char s[160];
    texto_avance(pc > 100 ? 100 : pc, s, sizeof s);
    int cheio = (pc > 100 ? 100 : pc) * 30 / 100;
    char barra[32];
    for (int i = 0; i < 30; i++) barra[i] = i < cheio ? '#' : '.';
    barra[30] = 0;
    imprimir("\r  [%s] %s   ", barra, s);
    if (pc >= 100) imprimir("\n");
}

/* "8G", "512M", "todo" -> sectores (0 = todo); -1 si no se entiende. */
static int64_t tamano_arg(const char *a)
{
    if (compara_texto(a, "todo") == 0 || compara_texto(a, "max") == 0) return 0;
    char num[16];
    int k = 0;
    while (a[k] >= '0' && a[k] <= '9' && k < 15) { num[k] = a[k]; k++; }
    num[k] = 0;
    int v;
    if (!k || !texto_para_numero(num, &v) || v <= 0) return -1;
    if (a[k] == 'G' || a[k] == 'g') return (int64_t)v * 1024 * 2048;
    if (a[k] == 'M' || a[k] == 'm' || !a[k]) return (int64_t)v * 2048;
    return -1;
}

static int modo_texto(home_plano_t *h, const char *tam, bool sim)
{
    completo = *h;
    opciones(h);
    escolher(h, 0);
    char t[24], d[16];
    tamanho(h->criar ? 0 : h->plano.antes, d, sizeof d);
    int escolha = 0;
    if (tam) {   /* un tamaño dado: se prueba tal cual (no solo los de la lista) */
        int64_t set = tamano_arg(tam);
        if (set < 0) { imprimir("ampliar-home: no entiendo el tamaño «%s» (ejemplos: 8G, 512M, todo)\n", tam); return 2; }
        if (set) {
            if (n_op < 4) n_op++;
            tam_op[n_op - 1] = (uint32_t)set;
            escolha = n_op - 1;
            escolher(h, escolha);
            if (sel != escolha) { imprimir("ampliar-home: no se puede ampliar /home a %s (hay %u MB como mucho)\n", tam, completo.total_novo / 2048); return 1; }
        }
    } else {
        imprimir("/home mide ahora %s. ¿Hasta qué tamaño se amplía?\n", d);
        for (int i = 0; i < n_op; i++) {
            char r[24];
            tamanho(i ? tam_op[i] : completo.total_novo, r, sizeof r);
            escolher(h, i);
            home_texto_tempo(est_h.segundos, t, sizeof t);
            imprimir("  %d) %s%s: hay que escribir unos %u MB, tardará como mucho %s\n", i + 1, i ? "" : "todo, ", r, est_h.mb, t);
        }
        imprimir("  0) ahora no\nElige [0-%d]: ", n_op);
        char r[16];
        if (ler_linha(ENTRADA, r, sizeof r) < 0) return 1;
        int v;
        if (!texto_para_numero(r, &v) || v < 1 || v > n_op) { imprimir("No se ha cambiado nada.\n"); return 0; }
        escolha = v - 1;
        escolher(h, escolha);
    }
    home_texto_tempo(est_h.segundos, t, sizeof t);
    imprimir("Se va a %s /home hasta %u MB (%u grupos nuevos, unos %u MB que escribir a unos %u MB/s: como mucho %s).\n"
             "NO APAGUES EL EQUIPO hasta que termine.\n", h->criar ? "crear" : "ampliar", h->total_novo / 2048, est_h.grupos, est_h.mb, est_h.mbs, t);
    if (!sim) {
        char r[16];
        imprimir("¿Seguir? [escribe sí para seguir]: ");
        if (ler_linha(ENTRADA, r, sizeof r) < 0 || !(compara_texto(r, "sí") == 0 || compara_texto(r, "si") == 0 || compara_texto(r, "s") == 0)) {
            imprimir("No se ha cambiado nada.\n");
            return 0;
        }
    }
    char m[300];
    pc_ult = -1;
    t_ini = tempo_ms();
    registrar("ampliar-home: empieza en el terminal (%u MB a escribir, estimado %u s)", est_h.mb, est_h.segundos);
    int r = home_ampliar(h, avance_terminal, NULL, m, sizeof m);
    registrar("ampliar-home: resultado %d (%u s): %s", r, (tempo_ms() - t_ini) / 1000, m);
    imprimir("%s\n", m);
    return r < 0 ? 1 : 0;
}

int principal(int argc, char **argv)
{
    bool primeiro = argc > 1 && compara_texto(argv[1], "--primer-arranque") == 0;
    bool ventana = argc > 1 && compara_texto(argv[1], "--ventana") == 0;
    bool texto = argc > 1 && compara_texto(argv[1], "--texto") == 0;
    if (argc > 1 && !primeiro && !ventana && !texto) {
        imprimir("uso: ampliar-home [--ventana | --primer-arranque | --texto [todo|8G|512M] [-s]]\n"
                 "Dice cuánto se puede ampliar /home en el disco de arranque; con --ventana lo pregunta y lo hace.\n"
                 "Con --texto lo hace en el terminal (hace falta ser root): enseña antes una estimación del tiempo (MB que hay que\n"
                 "poner a cero y velocidad calculada del disco), deja elegir «todo», un tamaño menor (8G, 16G...) o ahora no, y\n"
                 "durante el trabajo muestra una barra con el porcentaje, los MB escritos y el tiempo que queda. Con -s no pregunta.\n"
                 "Ampliar /home a un SSD grande son varios GB de escrituras: en un portátil antiguo pueden ser muchos minutos.\n"
                 "NO apagues el equipo mientras trabaja.\n"
                 "En el terminal: particiones ampliar /home\n"
                 "Desde la v8.5.4.10r2 /home es la última partición: crece hasta el final del disco sin mover nada.\n"
                 "En una imagen grabada antes (/home la primera), en la live no se puede (habría que mover la raíz en uso):\n"
                 "graba la imagen nueva o instala SaguiOS en el disco con «instalar» desde un pendrive.\n");
        return 2;
    }
    if (primeiro && ja_respondido()) return 0;
    home_plano_t h;
    char m[300];
    int r = home_planejar(&h, m, sizeof m);
    if (r < 0 || h.ganho < (primeiro ? MINIMO : 2048u)) {
        if (!primeiro) imprimir("%s\n", r < 0 || m[0] ? m : "no hay espacio libre que añadir a /home");
        else registrar("ampliar-home: nada que preguntar (%s)", r < 0 ? m : "menos de 512 MB libres");
        return 0;
    }
    if (h.bloqueado && !primeiro && !ventana) { imprimir("%s\n", m); return 1; }   /* D388 */
    if (texto) {
        if (!sou_root()) { imprimir("ampliar-home: --texto cambia el disco: hace falta ser root (sudo ampliar-home --texto)\n"); return 1; }
        const char *tam = NULL;
        bool sim = false;
        for (int i = 2; i < argc; i++) { if (compara_texto(argv[i], "-s") == 0 || compara_texto(argv[i], "--si") == 0) sim = true; else tam = argv[i]; }
        return modo_texto(&h, tam, sim);
    }
    if (!primeiro && !ventana) { imprimir("%s\nPara hacerlo: particiones ampliar /home (o ampliar-home --ventana)\n", m); return 0; }
    if (h.bloqueado) { registrar("ampliar-home: %s", m); return janela(&h, primeiro); }   /* solo informa: no hace falta ser root */
    if (!sou_root() && elevar_grafico("ampliar /home en el disco de arranque") != 0) return 1;
    return janela(&h, primeiro);
}
