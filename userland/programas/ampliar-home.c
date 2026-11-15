// SPDX-License-Identifier: GPL-2.0-or-later
/* ampliar-home - «Ampliar /home para usar todo el espacio libre» (D318, v8.4.3).
 *
 *   ampliar-home                    dice cuánto se puede ampliar /home (no cambia nada)
 *   ampliar-home --ventana          el diálogo (si hay algo que ampliar)
 *   ampliar-home --primer-arranque  el diálogo del primer arranque: solo si el disco de arranque tiene
 *                                   al menos 512 MB libres que SaguiOS no usa, hay escritorio y no se
 *                                   ha contestado antes. Lo lanza el gestor de servicios
 *                                   (/etc/servicios/ampliar-home.conf, una vez por arranque).
 * El trabajo lo hace la libsagui (home_ampliar.c), igual que «particiones ampliar /home»: /home crece
 * hasta el final del disco (las particiones de detrás se mueven al final, copiadas y releídas antes de
 * cambiar la tabla) o, si /home está en la RAM, se le crea una partición SAGUI-DATOS.
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
#define A 250

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

enum { EST_PERGUNTA, EST_TRABALHANDO, EST_FIM };
static int est;
static bool ok_final;

static void desenhar(const home_plano_t *h)
{
    g_retangulo(j, 0, 0, L, A, W98_FACE);
    g_retangulo(j, 16, 16, 32, 32, 0x2E9E4F);   /* un disco verde: espacio para /home */
    g_bisel(j, 16, 16, 32, 32, false);
    g_texto(j, 24, 24, "+", W98_LUZ);
    char t[400], g[16], d[16];
    tamanho(h->ganho, g, sizeof g);
    info_bloco_t b;
    tamanho(blocos_info(h->disco, &b) == 0 ? b.setores : 0, d, sizeof d);
    if (est == EST_PERGUNTA) {
        g_texto(j, 64, 16, "Ampliar /home", W98_NEGRO);
        g_texto(j, 65, 16, "Ampliar /home", W98_NEGRO);
        if (h->criar)
            formatar_texto(t, sizeof t, "El disco de arranque (%s, %s) tiene %s libres que SaguiOS no está usando, y /home está en la RAM: "
                           "lo que guardes se pierde al apagar.\n¿Crear /home en ese espacio libre (%s) y conservar lo que hay ahora?", h->disco, d, g, g);
        else
            formatar_texto(t, sizeof t, "El disco de arranque (%s, %s) tiene %s libres que SaguiOS no está usando.\n"
                           "¿Ampliar /home para usar todo el espacio libre (%s)?%s", h->disco, d, g, g,
                           h->plano.nmover ? " Las particiones de detrás de /home se copian al final del disco y se comprueban antes de cambiar nada." : "");
        linhas(64, 40, t, 56, W98_NEGRO);
        botao(L - 400, A - 42, 110, h->criar ? "Crear /home" : "Ampliar");
        botao(L - 280, A - 42, 100, "Ahora no");
        botao(L - 170, A - 42, 160, "No volver a preguntar");
    } else {
        g_texto(j, 64, 16, est == EST_FIM ? (ok_final ? "Hecho" : "No se ha podido") : "Ampliando /home...", W98_NEGRO);
        linhas(64, 40, estado, 56, est == EST_FIM && !ok_final ? 0xC00000 : W98_NEGRO);
        if (est == EST_TRABALHANDO && pc_atual >= 0) {
            g_retangulo(j, 64, A - 80, L - 90, 20, W98_LUZ);
            g_bisel(j, 64, A - 80, L - 90, 20, true);
            g_retangulo(j, 67, A - 77, (L - 96) * pc_atual / 100, 14, W98_AZUL);
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
    if (pc / 5 == pc_atual / 5) return;
    pc_atual = pc;
    desenhar(hp);
}

static int janela(home_plano_t *h, bool primeiro)
{
    j = jan_abrir(primeiro ? "Primer arranque: espacio libre" : "Ampliar /home", L, A);
    if (!j) { registrar("ampliar-home: sin escritorio: no se pregunta"); return 0; }
    hp = h;
    est = EST_PERGUNTA;
    desenhar(h);
    registrar("ampliar-home: pregunta: ganar %u MB (%s) en %s; botones y=%d ampliar=%d ahora_no=%d nunca=%d", h->ganho / 2048,
              h->criar ? "crear" : "ampliar", h->disco, A - 42, L - 400 + 55, L - 280 + 50, L - 170 + 80);
    evento_t e;
    for (;;) {
        if (esperar_evento(j->id, &e, 1000) != 1) continue;
        if (e.tipo == EV_FECHAR) { if (est != EST_TRABALHANDO) break; continue; }
        bool linha = e.tipo == EV_CLIQUE && e.y >= A - 42 && e.y < A - 16;
        if (est == EST_FIM) {
            if ((linha && e.x >= L - 120 && e.x < L - 20) || (e.tipo == EV_TECLA && (e.tecla == '\n' || e.tecla == TECLA_ESC))) break;
            continue;
        }
        if (est != EST_PERGUNTA) continue;
        if ((linha && e.x >= L - 280 && e.x < L - 180) || (e.tipo == EV_TECLA && e.tecla == TECLA_ESC)) { registrar("ampliar-home: ahora no"); break; }
        if (linha && e.x >= L - 170 && e.x < L - 10) { lembrar("no volver a preguntar"); break; }
        /* Solo con el ratón: este diálogo sale solo al arrancar y un Intro escrito para otra ventana no
         * debe cambiar el disco. */
        if (linha && e.x >= L - 400 && e.x < L - 290) {
            est = EST_TRABALHANDO;
            copia_texto(estado, h->criar ? "Creando la partición de /home y copiando lo que hay..."
                                         : "Comprobando el sistema de archivos, moviendo lo que haga falta y ampliando /home. No apagues el ordenador.", sizeof estado);
            desenhar(h);
            registrar("ampliar-home: empieza");
            int r = home_ampliar(h, progresso, NULL, estado, sizeof estado);
            ok_final = r >= 0;
            if (ok_final) lembrar("hecho");
            registrar("ampliar-home: resultado %d: %s", r, estado);
            est = EST_FIM;
            desenhar(h);
        }
    }
    jan_fechar(j);
    return ok_final ? 0 : 1;
}

int principal(int argc, char **argv)
{
    bool primeiro = argc > 1 && compara_texto(argv[1], "--primer-arranque") == 0;
    bool ventana = argc > 1 && compara_texto(argv[1], "--ventana") == 0;
    if (argc > 1 && !primeiro && !ventana) {
        imprimir("uso: ampliar-home [--ventana | --primer-arranque]\n"
                 "Dice cuánto se puede ampliar /home en el disco de arranque; con --ventana lo pregunta y lo hace.\n"
                 "En el terminal: particiones ampliar /home\n");
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
    if (!primeiro && !ventana) { imprimir("%s\nPara hacerlo: particiones ampliar /home (o ampliar-home --ventana)\n", m); return 0; }
    if (!sou_root() && elevar_grafico("ampliar /home en el disco de arranque") != 0) return 1;
    return janela(&h, primeiro);
}
