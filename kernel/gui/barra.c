// SPDX-License-Identifier: GPL-2.0-or-later
/* barra.c - la barra superior: botón Inicio, botones de las tareas y reloj (D065).
 *
 * La barra sigue arriba y con 24 píxeles (BARRA_SUPERIOR): de izquierda a
 * derecha, el botón Inicio con el sagui, un botón por ventana abierta y,
 * pegado a la derecha, el reloj. El lema aparece en el espacio que sobra entre
 * las tareas y el reloj (entero, corto o nada).
 *
 * Los botones de las tareas siguen el orden de creación de las ventanas (no el de
 * apilamiento), para no cambiar de lugar en cada clic. Cuando no caben,
 * se estrechan hasta TAREFA_MIN; por debajo de eso las que sobran van al menú del
 * botón "»". La ventana con el foco tiene el botón resaltado; las minimizadas, un
 * botón "apagado". Los diálogos (JAN_DIALOGO) no tienen botón.
 *
 * Nada aquí guarda punteros a ventanas: la barra pide la lista a janelas.c
 * (janelas_da_barra) y actúa por el id. En cada tic, una "firma" de la lista
 * dice si algo cambió; sólo entonces se ensucia la franja de la barra. El reloj
 * ensucia sólo su propio rectángulo, una vez por minuto (o cuando cambian la zona o la hora).
 * Muestra la hora local de la zona del sistema (D080): el cambio al horario
 * de verano aparece solo, sin reiniciar. */
#include "aparencia.h"
#include "gui.h"
#include "arte.h"
#include "audio.h"
#include "bateria.h"
#include "bateria_conv.h"
#include "console.h"
#include "mouse.h"
#include "processo.h"
#include "relogio.h"
#include "rede.h"
#include "wifi.h"
#include "sensores.h"
#include "sistema.h"
#include "teclado.h"
#include "texto.h"
#include "versao.h"
#include "montagem.h"
#include "video.h"
#include "telas.h"
#include "vfs.h"

#define BOTAO_Y     3
#define BOTAO_A     18
#define INICIO_X    2
#define INICIO_L    84
#define TAREFAS_X0  (INICIO_X + INICIO_L + 6)
#define TAREFA_MAX  170
#define TAREFA_MIN  72     /* ícono y unas 5 letras del título */
#define ESPACO      4      /* entre un botón y otro */
#define MAIS_L      24     /* el botón "»" con las tareas que no caben */
#define MAX_TAREFAS 32

#define COR_BARRA        0x18202A
#define COR_LINHA        aparencia_acento()   /* D112: color de acento elegido */
#define COR_INICIO       aparencia_acento()
#define COR_INICIO_ABERTO aparencia_escurecer(aparencia_acento(), 25)
#define COR_TAREFA       0x2C3A46
#define COR_TAREFA_BORDA 0x3E4E5C
#define COR_FOCO         0x4A5E70
#define COR_FOCO_MARCA   0xFFDF4A
#define COR_MINIMIZADA   0x1E2730
#define COR_MIN_BORDA    0x34404C
#define COR_MIN_TEXTO    0x8A96A2

/* ------------------------------------------------------------ disposición */

static janela_barra_t jb[MAX_TAREFAS];
static int njb;

typedef struct { int j, x, l; } botao_t;   /* j = índice en jb[] */

static struct {
    int L;
    int relogio_x;               /* donde empieza el texto del reloj */
    botao_t bt[MAX_TAREFAS];
    int nbt;
    int mais_x;                  /* -1 = sin botón "»" */
    int ocultas[MAX_TAREFAS], nocultas;
    int fim_tarefas;             /* primer x libre después de las tareas */
    int band_x, band_l;          /* la bandeja (D114), a la izquierda del reloj */
    int nic, ic_id[4], ic_x[4];  /* iconos visibles, de izquierda a derecha */
} lay;

/* La bandeja del sistema (D114): volumen, red, temperatura y (D192) batería. */
enum { IC_TEMP, IC_REDE, IC_VOL, IC_BAT };
#define IC_L      16      /* los iconos se dibujan en 16x16 */
#define IC_ESP    4
#define IC_Y      ((BARRA_SUPERIOR - 1 - IC_L) / 2)
static bool bandeja_temp_visivel(void);
static bool bandeja_bat_visivel(void);

static bool inicio_aberto;
static int minuto_mostrado = -1;
static uint32_t ajustes_vistos;
static uint32_t assinatura_vista;

static const char *const semana_curta[] = { "dom", "lun", "mar", "mié", "jue", "vie", "sáb" };

/* La hora en 24 h o en 12 h ("3:05 p. m."), según /etc/zona (D080). */
static void texto_hora(char *s, int max, const fuso_data_t *d)
{
    if (hora_24h()) snformatar(s, (uint32_t)max, "%02u:%02u", d->hora, d->minuto);
    else snformatar(s, (uint32_t)max, "%2u:%02u %s", d->hora % 12 ? d->hora % 12 : 12, d->minuto,
                    d->hora < 12 ? "a. m." : "p. m.");
}

/* El texto del reloj: completo de 800 píxeles para arriba; en una pantalla de 640,
 * sólo el día de la semana y la hora (la fecha entera aparece al hacer clic). */
static void texto_relogio(char *s, int max, const fuso_data_t *d, bool completo)
{
    const char *dia = semana_curta[d->dia_semana % 7];
    char h[16];
    texto_hora(h, sizeof h, d);
    if (completo)
        snformatar(s, (uint32_t)max, "%s %02u/%02u/%u  %s", dia, d->dia, d->mes, d->ano, h);
    else
        snformatar(s, (uint32_t)max, "%s %s", dia, h);
}

static int largura_barra(void) { int l, a; telas_principal_rect(&l, &a); return l; }   /* D290: la barra vive en la pantalla principal */

static bool relogio_completo(void) { return largura_barra() >= 800; }

static int largura_relogio(void)
{
    /* El formato tiene ancho fijo: el reloj no empuja los botones cada minuto. */
    const char *h = hora_24h() ? "00:00" : "00:00 p. m.";
    return texto_largura(relogio_completo() ? "dom 00/00/0000  " : "dom ") + texto_largura(h);
}

static void calcular(void)
{
    njb = janelas_da_barra(jb, MAX_TAREFAS);
    lay.L = largura_barra();
    lay.relogio_x = lay.L - largura_relogio() - 10;
    lay.nic = 0;
    if (bandeja_temp_visivel()) lay.ic_id[lay.nic++] = IC_TEMP;   /* temperatura, batería, red y, pegado al reloj, el volumen */
    if (bandeja_bat_visivel()) lay.ic_id[lay.nic++] = IC_BAT;     /* D192: solo con una batería puesta */
    lay.ic_id[lay.nic++] = IC_REDE;
    lay.ic_id[lay.nic++] = IC_VOL;
    lay.band_l = lay.nic * (IC_L + IC_ESP) - IC_ESP;
    lay.band_x = lay.relogio_x - 12 - lay.band_l;
    for (int i = 0; i < lay.nic; i++) lay.ic_x[i] = lay.band_x + i * (IC_L + IC_ESP);
    lay.nbt = lay.nocultas = 0;
    lay.mais_x = -1;
    int x0 = TAREFAS_X0, x1 = lay.band_x - 8, area = x1 - x0;
    lay.fim_tarefas = x0;
    if (njb == 0 || area < TAREFA_MIN) {
        for (int i = 0; i < njb; i++) lay.ocultas[lay.nocultas++] = i;
        if (njb && area >= MAIS_L) lay.mais_x = x0;
        return;
    }
    int n = njb, l = (area - (n - 1) * ESPACO) / n;
    if (l < TAREFA_MIN) {
        /* No caben todas: las k primeras y el "»". La que tiene el foco queda
         * siempre visible, en el lugar de la última. */
        n = (area - MAIS_L) / (TAREFA_MIN + ESPACO);
        if (n < 1) n = 1;
        l = (area - MAIS_L - n * ESPACO) / n;
    }
    if (l > TAREFA_MAX) l = TAREFA_MAX;
    int foco = -1;
    for (int i = 0; i < njb; i++) if (jb[i].foco) foco = i;
    bool troca = foco >= n;
    for (int i = 0, x = x0; i < n; i++, x += l + ESPACO) {
        int j = (troca && i == n - 1) ? foco : i;
        lay.bt[lay.nbt++] = (botao_t){ j, x, l };
        lay.fim_tarefas = x + l;
    }
    for (int i = n; i < njb; i++) lay.ocultas[lay.nocultas++] = (troca && i == foco) ? n - 1 : i;
    if (lay.nocultas) {
        lay.mais_x = lay.fim_tarefas + ESPACO;
        lay.fim_tarefas = lay.mais_x + MAIS_L;
    }
}

/* Una suma de todo lo que aparece en los botones: cambia cuando abre, cierra,
 * minimiza, cambia el foco o el título de alguna ventana, o cuando la pantalla
 * cambia de ancho. */
static uint32_t assinatura(void)
{
    uint32_t h = 2166136261u;
#define MISTURA(v) (h = (h ^ (uint32_t)(v)) * 16777619u)
    MISTURA(largura_barra());
    MISTURA(inicio_aberto);
    MISTURA(lay.nic);
    MISTURA(njb);
    for (int i = 0; i < njb; i++) {
        MISTURA(jb[i].id);
        MISTURA(jb[i].minimizada | jb[i].foco << 1);
        for (const char *c = jb[i].titulo; *c; c++) MISTURA((uint8_t)*c);
    }
#undef MISTURA
    return h;
}

static void registrar(void)
{
    char s[400];
    int k = snformatar(s, sizeof s, "[GUI] barra %d: inicio %d+%d, %d tarefas:", lay.L, INICIO_X, INICIO_L, njb);
    for (int i = 0; i < lay.nbt && k < (int)sizeof s - 40; i++) {
        janela_barra_t *b = &jb[lay.bt[i].j];
        k += snformatar(s + k, sizeof s - (uint32_t)k, " %d@%d+%d%s", b->id, lay.bt[i].x, lay.bt[i].l,
                        b->foco ? "f" : b->minimizada ? "m" : "");
    }
    if (lay.mais_x >= 0 && k < (int)sizeof s - 40)
        k += snformatar(s + k, sizeof s - (uint32_t)k, ", mais %d (%d ocultas)", lay.mais_x, lay.nocultas);
    if (k < (int)sizeof s - 20) snformatar(s + k, sizeof s - (uint32_t)k, ", relogio %d", lay.relogio_x);
    klog("%s\n", s);
    /* Los iconos de la bandeja, de izquierda a derecha (D114). */
    k = snformatar(s, sizeof s, "[GUI] bandeja %d+%d:", lay.band_x, lay.band_l);
    for (int i = 0; i < lay.nic; i++)
        k += snformatar(s + k, sizeof s - (uint32_t)k, " %s@%d", lay.ic_id[i] == IC_VOL ? "vol" : lay.ic_id[i] == IC_REDE ? "red"
                        : lay.ic_id[i] == IC_BAT ? "bat" : "temp", lay.ic_x[i]);
    klog("%s\n", s);
}

/* ------------------------------------------------------------ íconos */

/* El ícono de un programa por el nombre del proceso (el archivo en /bin). */
static const desenho_t *icone_do_processo(const char *nome)
{
    static const struct { const char *nome; const desenho_t *d; } t[] = {
        { "arquivos", &arte_icone_arquivos }, { "terminal", &arte_icone_terminal }, { "editor", &arte_icone_editor },
        { "calculadora", &arte_icone_calculadora }, { "banana", &arte_icone_banana },
        { "sagui3d", &arte_icone_sagui3d }, { "sobre-o-sistema", &arte_icone_sobre },
        { "tareas", &arte_icone_tarefas }, { "visor", &arte_icone_visor }, { "paint", &arte_icone_paint }, { "chip8", &arte_icone_chip8 }, { "configuracion", &arte_icone_configuracion }, { "discos", &arte_icone_discos },
        { "pantalla", &arte_mini_pantalla }, { "cipo", &arte_mini_cipo }, { "markdown", &arte_mini_markdown },
        { "infopc", &arte_mini_infopc }, { "ejecutar", &arte_inicio }, { "mico", &arte_mini_mico },
        /* D139: os programas que estavam sem icone. */
        { "recortes", &arte_mini_recortes }, { "icones", &arte_mini_icones },
        { "musica", &arte_mini_musica }, { "midi", &arte_mini_musica }, { "sintonia", &arte_mini_musica }, { "zona", &arte_mini_relogio },
        { "visorpdf", &arte_mini_visorpdf }, { "reproductor", &arte_mini_video }, { "camara", &arte_mini_video }, { "grabadora", &arte_mini_musica },
        { "ia", &arte_mini_ia }, { "vps", &arte_mini_vps }, { "exploradorbd", &arte_mini_exploradorbd }, { "macaco", &arte_mini_macaco }, { "pluma", &arte_mini_pluma }, { "gebramini", &arte_mini_gebramini }, { "spider", &arte_mini_spider },
        { "jclic", &arte_mini_jclic }, { "jclicbajar", &arte_mini_jclicbajar },   /* D185 */
        { "vnc", &arte_mini_pantalla },   /* D254 */
#ifndef SAGUI_SEM_GPL
        { "dosbox", &arte_mini_dosbox }, { "recreativa", &arte_mini_recreativa },   /* D294 */
#endif
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++)
        if (txt_compara(nome, t[i].nome) == 0) return t[i].d;
    return NULL;
}

const desenho_t *barra_icone_processo(const char *nome) { return icone_do_processo(nome); }   /* Alt+Tab (D119) */

/* ------------------------------------------------------------ dibujo */

static void desenhar_inicio(superficie_t *s)
{
    uint32_t fundo = inicio_aberto ? COR_INICIO_ABERTO : COR_INICIO;
    sup_retangulo(s, INICIO_X, BOTAO_Y, INICIO_L, BOTAO_A, fundo);
    sup_moldura(s, INICIO_X, BOTAO_Y, INICIO_L, BOTAO_A, inicio_aberto ? aparencia_escurecer(aparencia_acento(), 55) : aparencia_clarear(aparencia_acento(), 20));
    sup_desenho(s, INICIO_X + 6, BOTAO_Y + 1, &arte_inicio, 1);
    sup_texto(s, INICIO_X + 28, BOTAO_Y + 1, "Inicio", 0xFFFFFF);
    sup_texto(s, INICIO_X + 29, BOTAO_Y + 1, "Inicio", 0xFFFFFF);   /* negrita: la misma letra a un píxel al lado */
}

static void desenhar_tarefa(superficie_t *s, botao_t *b)
{
    janela_barra_t *j = &jb[b->j];
    uint32_t fundo = j->foco ? COR_FOCO : j->minimizada ? COR_MINIMIZADA : COR_TAREFA;
    uint32_t borda = j->minimizada ? COR_MIN_BORDA : COR_TAREFA_BORDA;
    uint32_t cor = j->minimizada ? COR_MIN_TEXTO : j->foco ? 0xFFFFFF : 0xD8DEE4;
    sup_retangulo(s, b->x, BOTAO_Y, b->l, BOTAO_A, fundo);
    sup_moldura(s, b->x, BOTAO_Y, b->l, BOTAO_A, borda);
    if (j->foco) sup_retangulo(s, b->x + 1, BOTAO_Y + BOTAO_A - 3, b->l - 2, 2, COR_FOCO_MARCA);
    int tx = b->x + 6;
    const desenho_t *ic = icone_do_processo(j->processo);
    if (ic) { sup_desenho_metade(s, b->x + 4, BOTAO_Y + 1, ic); tx = b->x + 24; }
    /* El título cortado en lo que cabe; si se cortó, termina en "..". */
    int cabe = (b->x + b->l - 4 - tx) / 8;
    char t[48];
    txt_copia_n(t, j->titulo, sizeof t);
    if (cabe < 1) return;
    if ((int)txt_tamanho(t) > cabe) {
        t[cabe] = 0;
        if (cabe >= 4) t[cabe - 1] = t[cabe - 2] = '.';
    }
    sup_texto(s, tx, BOTAO_Y + 1, t, cor);
}

static void desenhar_mais(superficie_t *s)
{
    sup_retangulo(s, lay.mais_x, BOTAO_Y, MAIS_L, BOTAO_A, COR_TAREFA);
    sup_moldura(s, lay.mais_x, BOTAO_Y, MAIS_L, BOTAO_A, COR_TAREFA_BORDA);
    sup_texto(s, lay.mais_x + (MAIS_L - 8) / 2, BOTAO_Y + 1, "»", 0xFFFFFF);
}

static void desenhar_lema(superficie_t *s)
{
    static const char *const lemas[] = { SAGUI_NOME_VERSAO ", basado en SaguiOS de Eli Rigobeli", SAGUI_NOME_VERSAO };
    int x1 = lay.band_x - 16;
    for (unsigned i = 0; i < sizeof lemas / sizeof lemas[0]; i++) {
        int l = texto_largura(lemas[i]);
        if (x1 - l >= lay.fim_tarefas + 16) {
            sup_texto(s, x1 - l, 4, lemas[i], 0x9AA8B8);
            return;
        }
    }
}

/* ------------------------------------------------------------ bandeja (D114) */

/* Tres iconos a la izquierda del reloj: temperatura de la CPU (sólo si se puede
 * leer), red y volumen. Una vez por segundo (como el reloj) se leen el audio,
 * la red y los sensores; sólo si algo cambió se ensucia el rectángulo de la
 * bandeja. El clic abre un panel emergente propio (no es un menú contextual:
 * lleva un control deslizante y un botón) y, con el ratón encima, aparece una
 * ayuda ("sin sonido", la IP...). Sin syscalls nuevas: el kernel lee
 * directo audio.c, rede.c y sensores.c. */

enum { P_NADA, P_VOL, P_RED, P_TEMP, P_BAT };
#define COR_ICO       0xD8DEE4
#define COR_ICO_CINZA 0x6A7682
#define COR_ICO_APAG  0x4A5662
#define COR_ICO_OK    0x6FD98F
#define COR_ICO_ERRO  0xE0524A
#define COR_TEMP_QUENTE 0xFFB02E     /* >= 70 °C */
#define COR_TEMP_ALTA   0xFF4A3A     /* >= 79 °C */
#define TEMP_QUENTE_MC  70000
#define TEMP_ALTA_MC    79000
#define COR_PAINEL      0x1C2630
#define COR_PAINEL_BORDA 0x3E4E5C
#define COR_TRILHO      0x0E141A
#define COR_BOTAO_BAND  0x2C3A46
#define COR_BAT_BAIXA   0xFF4A3A     /* D192: menos del 15 % y sin cargar */
#define COR_BAT_RAIO    0xFFD23F     /* el rayo de "cargando" */
#define BAT_COR_AVISO   15           /* % : el relleno pasa a rojo */
#define BAT_AVISO       10           /* % : una notificación (una vez por bajada) */
#define BAT_REARMAR     12

static struct {
    bool lido;                   /* ya hubo una lectura */
    int  som, vol, mudo;         /* AUDIO_SEM_PLACA/FUNCIONANDO/..., 0-100, 0/1 */
    int  rede;                   /* 0 sin tarjeta, 1 sin conexión, 2 conectado */
    bool temp_ok;
    int  temp_mc;
    unsigned thr;                /* SENS_THR_* */
    bool bat_ok;                 /* D192: hay batería puesta (bateria.c, uACPI) */
    bool bat_avisou;             /* ya se avisó de batería baja en esta bajada */
    info_bateria_t bat;
    uint32_t tique, hash;
} tb;
static info_rede_t rd;
static bool rd_tem;
static info_wifi_t wf;           /* D215: el pincho Wi-Fi */

static struct { int tipo, x, y, l, a; bool arrasta; } pop;
static struct { int ic, x, y, l; char t[72]; } dica = { .ic = -1 };

static bool bandeja_temp_visivel(void) { return tb.temp_ok; }
static bool bandeja_bat_visivel(void) { return tb.bat_ok; }

/* Punto de enganche de la mejora 061 (frecuencia de la CPU): si la CPU está
 * reduciendo la frecuencia por temperatura, el icono lo marca. Hoy sale del
 * indicador de throttling de INFO_SENSORES; la 061 puede cambiar sólo esto. */
static bool bandeja_limitada_termica(void) { return (tb.thr & SENS_THR_TERMICO) != 0; }

static void ip_texto(char *s, int max, uint32_t ip)
{
    snformatar(s, (uint32_t)max, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
}

static uint32_t bandeja_ler(void)
{
    tb.som = audio_mestre(&tb.vol, &tb.mudo);
    rd_tem = rede_bandeja(&rd);
    tb.rede = (!rd_tem || !rd.ativa) ? 0 : (rd.cabo && rd.ip && !rd.desativada) ? 2 : 1;
    wifi_info(&wf);
    if (!tb.rede && wf.estado != WIFI_SEM_PINCHO) tb.rede = 1;   /* solo el Wi-Fi, sin conectar */
    unsigned thr = 0;
    int mc = 0;
    tb.temp_ok = sensores_temp_bandeja(&mc, &thr);
    tb.temp_mc = tb.temp_ok ? mc : 0;
    tb.thr = thr;
    tb.bat_ok = bateria_estado(&tb.bat);
    tb.lido = true;
    uint32_t h = 2166136261u;
#define MIST(v) (h = (h ^ (uint32_t)(v)) * 16777619u)
    MIST(tb.som); MIST(tb.vol); MIST(tb.mudo); MIST(tb.rede); MIST(tb.temp_ok); MIST(tb.temp_mc / 1000); MIST(tb.thr);
    MIST(rd.ip); MIST(rd.roteador); MIST(rd.dns); MIST(rd.dhcp_estado); MIST(rd.cabo); MIST(rd.desativada);
    MIST(wf.estado); MIST(wf.qualidade / 25); MIST(wf.seguranca);
    MIST(tb.bat_ok); MIST(tb.bat.porcentagem); MIST(tb.bat.estado); MIST(tb.bat.adaptador); MIST(tb.bat.minutos);
#undef MIST
    return h;
}

static uint32_t cor_temp(void)
{
    return tb.temp_mc >= TEMP_ALTA_MC ? COR_TEMP_ALTA : tb.temp_mc >= TEMP_QUENTE_MC ? COR_TEMP_QUENTE : COR_ICO_OK;
}

/* ---- los iconos (16x16) */

static void ico_volume(superficie_t *s, int x, int y)
{
#define R(a, b, c, d, cor) sup_retangulo(s, x + (a), y + (b), (c), (d), (cor))
    bool sem = tb.som != AUDIO_FUNCIONANDO;
    uint32_t c = sem ? COR_ICO_APAG : COR_ICO;
    R(1, 5, 3, 6, c);
    for (int i = 0; i < 4; i++) R(4 + i, 4 - i, 1, 8 + 2 * i, c);
    if (sem) return;
    if (tb.mudo || tb.vol == 0) {   /* tachado */
        for (int i = 0; i < 6; i++) { R(9 + i, 5 + i, 2, 1, COR_ICO_ERRO); R(9 + i, 10 - i, 2, 1, COR_ICO_ERRO); }
        return;
    }
    R(9, 5, 1, 6, c);
    if (tb.vol > 33) R(11, 3, 1, 10, c);
    if (tb.vol > 66) R(13, 1, 1, 14, c);
}

/* D215: con el Wi-Fi, abanico de cuatro arcos (de abajo arriba), encendidos
 * según la calidad de la señal; sin conectar, en gris con la cruz. */
static bool bandeja_wifi(void)
{
    return wf.estado != WIFI_SEM_PINCHO && (txt_compara(rd.nome, "wlan0") == 0 || !rd_tem);
}

static void ico_wifi(superficie_t *s, int x, int y)
{
    bool ok = tb.rede == 2 && wf.estado == WIFI_CONECTADO;
    int acesos = ok ? 1 + (wf.qualidade >= 25) + (wf.qualidade >= 50) + (wf.qualidade >= 75) : 0;
    /* el punto; en rojo con WEP (D220: la conexión no protege nada) */
    R(7, 13, 2, 2, ok ? (wf.seguranca == WIFI_WEP ? COR_ICO_ERRO : COR_ICO_OK) : COR_ICO_CINZA);
    for (int a = 1; a <= 3; a++) {                            /* arcos: dos tramos inclinados y el techo */
        uint32_t c = a < acesos ? COR_ICO_OK : ok ? COR_ICO_APAG : COR_ICO_CINZA;
        int r = 3 * a, cy = 13 - r;
        R(8 - r, cy + 2, 1, 2, c); R(7 + r, cy + 2, 1, 2, c);
        R(8 - r + 1, cy + 1, 1, 1, c); R(6 + r, cy + 1, 1, 1, c);
        R(8 - r + 2, cy, 2 * r - 4, 1, c);
    }
    if (!ok)
        for (int i = 0; i < 5; i++) { R(i, i, 2, 1, COR_ICO_ERRO); R(i, 4 - i, 2, 1, COR_ICO_ERRO); }
}

static void ico_rede(superficie_t *s, int x, int y)
{
    if (bandeja_wifi()) { ico_wifi(s, x, y); return; }
    uint32_t c = tb.rede == 2 ? COR_ICO_OK : tb.rede == 1 ? COR_ICO_CINZA : COR_ICO_APAG;
    for (int i = 0; i < 4; i++) R(1 + 4 * i, 13 - (3 + 3 * i), 3, 3 + 3 * i + 1, c);
    if (tb.rede == 1)   /* sin conexión: una cruz roja arriba a la izquierda */
        for (int i = 0; i < 5; i++) { R(i, i, 2, 1, COR_ICO_ERRO); R(i, 4 - i, 2, 1, COR_ICO_ERRO); }
}

static void ico_temp(superficie_t *s, int x, int y)
{
    uint32_t c = cor_temp();
    int t = tb.temp_mc / 1000;
    int h = 1 + (t < 30 ? 0 : t > 90 ? 60 : t - 30) * 7 / 60;
    R(6, 1, 4, 10, 0x8A96A2);          /* tubo */
    R(7, 2, 2, 8, COR_BARRA);
    R(4, 10, 8, 5, 0x8A96A2);          /* bulbo */
    R(5, 11, 6, 3, c);
    R(7, 10 - h, 2, h, c);             /* el mercurio */
    if (bandeja_limitada_termica()) R(11, 1, 4, 4, COR_TEMP_ALTA);   /* limitada por temperatura (061) */
}

/* D192: la batería tumbada (cuerpo de 13x8 y el borne a la derecha), con el
 * relleno según el porcentaje: verde, o rojo por debajo del 15 % si no está
 * cargando; con el rayo amarillo encima si está cargando. Nivel desconocido:
 * sin relleno. */
static void ico_bateria(superficie_t *s, int x, int y)
{
    int pct = tb.bat.porcentagem;
    bool carga = (tb.bat.estado & BATERIA_CARREGANDO) != 0;
    uint32_t enche = pct < BAT_COR_AVISO && !carga ? COR_BAT_BAIXA : COR_ICO_OK;
    R(1, 4, 13, 1, COR_ICO);  R(1, 11, 13, 1, COR_ICO);   /* el cuerpo */
    R(1, 4, 1, 8, COR_ICO);   R(13, 4, 1, 8, COR_ICO);
    R(14, 6, 2, 4, COR_ICO);                              /* el borne */
    int w = pct < 0 ? 0 : (pct * 9 + 50) / 100;
    if (pct > 0 && w == 0) w = 1;
    if (w) R(3, 6, w, 4, enche);
    if (carga) {                                          /* el rayo, de arriba a la derecha hacia abajo a la izquierda */
        R(8, 2, 2, 1, COR_BAT_RAIO); R(7, 3, 2, 1, COR_BAT_RAIO); R(6, 4, 2, 1, COR_BAT_RAIO);
        R(5, 5, 2, 1, COR_BAT_RAIO); R(5, 6, 5, 1, COR_BAT_RAIO); R(8, 7, 2, 1, COR_BAT_RAIO);
        R(7, 8, 2, 1, COR_BAT_RAIO); R(6, 9, 2, 1, COR_BAT_RAIO); R(5, 10, 2, 1, COR_BAT_RAIO);
        R(5, 11, 1, 2, COR_BAT_RAIO);
    }
}
#undef R

static void bandeja_desenhar(superficie_t *s)
{
    for (int i = 0; i < lay.nic; i++) {
        int x = lay.ic_x[i];
        if (lay.ic_id[i] == IC_VOL) ico_volume(s, x, IC_Y);
        else if (lay.ic_id[i] == IC_REDE) ico_rede(s, x, IC_Y);
        else if (lay.ic_id[i] == IC_BAT) ico_bateria(s, x, IC_Y);
        else ico_temp(s, x, IC_Y);
    }
}

/* ---- textos */

static void dica_texto(int id, char *t, int max)
{
    char ip[20];
    if (id == IC_VOL) {
        if (tb.som != AUDIO_FUNCIONANDO) snformatar(t, (uint32_t)max, "Sin sonido");
        else if (tb.mudo) snformatar(t, (uint32_t)max, "Volumen: silencio");
        else snformatar(t, (uint32_t)max, "Volumen: %d %%", tb.vol);
    } else if (id == IC_REDE) {
        ip_texto(ip, sizeof ip, rd.ip);
        if (tb.rede == 2) snformatar(t, (uint32_t)max, "Red: %s (%s, %s)", ip, rd.nome, rd.driver[0] ? rd.driver : "?");
        else if (tb.rede == 1) snformatar(t, (uint32_t)max, "Sin conexión (%s)", rd.nome);
        else snformatar(t, (uint32_t)max, "Sin tarjeta de red");
    } else if (id == IC_BAT) {
        char tm[24];
        int pct = tb.bat.porcentagem;
        bateria_texto_tempo(tb.bat.minutos, tm, sizeof tm);
        if (pct < 0) snformatar(t, (uint32_t)max, "Batería: nivel desconocido");
        else if (tb.bat.estado & BATERIA_CARREGANDO)
            snformatar(t, (uint32_t)max, tm[0] ? "Batería: %d %% (cargando, %s para llenarse)" : "Batería: %d %% (cargando)", pct, tm);
        else if (tb.bat.estado & BATERIA_DESCARREGANDO)
            snformatar(t, (uint32_t)max, tm[0] ? "Batería: %d %% (quedan %s)" : "Batería: %d %%", pct, tm);
        else snformatar(t, (uint32_t)max, tb.bat.adaptador == 2 ? "Batería: %d %% (enchufado)" : "Batería: %d %%", pct);
    } else {
        snformatar(t, (uint32_t)max, "CPU: %d °C%s", tb.temp_mc / 1000, bandeja_limitada_termica() ? " (limitada)" : "");
    }
}

static int icone_em(int x, int y)
{
    if (y < 0 || y >= BARRA_SUPERIOR) return -1;
    for (int i = 0; i < lay.nic; i++)
        if (x >= lay.ic_x[i] - IC_ESP / 2 && x < lay.ic_x[i] + IC_L + IC_ESP / 2) return lay.ic_id[i];
    return -1;
}

static int icone_x(int id)
{
    for (int i = 0; i < lay.nic; i++) if (lay.ic_id[i] == id) return lay.ic_x[i];
    return lay.band_x;
}

/* ---- el panel emergente */

static void sujar_painel(void) { if (pop.tipo) gui_redesenhar(pop.x, pop.y, pop.l + 5, pop.a + 5); }
static void sujar_dica(void) { if (dica.ic >= 0) gui_redesenhar(dica.x, dica.y, dica.l + 4, 22); }
static void sujar_bandeja(void) { gui_redesenhar(lay.band_x - 3, 0, lay.band_l + 6, BARRA_SUPERIOR); }

#define VOL_SX   12
#define VOL_SY   36
#define VOL_SA   6
#define BTN_Y    54            /* "Silenciar" en el panel del volumen */
#define BTN_A    18
#define RED_BTN_L 132

static void painel_fechar(void)
{
    if (!pop.tipo) return;
    sujar_painel();
    pop.tipo = P_NADA;
    pop.arrasta = false;
}

static void painel_abrir(int id)
{
    painel_fechar();
    if (dica.ic >= 0) { sujar_dica(); dica.ic = -1; }
    pop.tipo = id == IC_VOL ? P_VOL : id == IC_REDE ? P_RED : id == IC_BAT ? P_BAT : P_TEMP;
    pop.l = id == IC_VOL ? 190 : id == IC_REDE ? (rd_tem && tb.rede ? 376 : 190) : id == IC_BAT ? 300 : 220;
    pop.a = id == IC_VOL ? 80 : id == IC_REDE ? (tb.rede ? 146 : 44) : id == IC_BAT ? 132 : (bandeja_limitada_termica() ? 62 : 44);
    int L = largura_barra();
    pop.x = icone_x(id) + IC_L / 2 - pop.l / 2;
    if (pop.x > L - pop.l - 6) pop.x = L - pop.l - 6;
    if (pop.x < 2) pop.x = 2;
    pop.y = BARRA_SUPERIOR + 1;
    pop.arrasta = false;
    sujar_painel();
    klog("[GUI] bandeja: painel %s aberto em (%d,%d) %dx%d\n", id == IC_VOL ? "volume" : id == IC_REDE ? "rede"
         : id == IC_BAT ? "bateria" : "temperatura",
         pop.x, pop.y, pop.l, pop.a);
}

/* Recorta un texto para que quepa en "px" píxeles, terminando en ".." si lo cortó. */
static void texto_cabe(char *t, int px)
{
    int cabe = px / 8, n = (int)txt_tamanho(t);
    if (n <= cabe) return;
    t[cabe] = 0;
    if (cabe >= 4) t[cabe - 1] = t[cabe - 2] = '.';
}

static void linha_painel(superficie_t *s, int y, const char *rot, const char *val)
{
    char t[96];
    snformatar(t, sizeof t, "%s%s", rot, val);
    texto_cabe(t, pop.l - 24);
    sup_texto(s, pop.x + 12, pop.y + y, t, 0xD8DEE4);
}

static void botao_painel(superficie_t *s, int x, int y, int l, int a, const char *t, bool marcado)
{
    sup_retangulo(s, x, y, l, a, marcado ? COR_FOCO : COR_BOTAO_BAND);
    sup_moldura(s, x, y, l, a, COR_PAINEL_BORDA);
    sup_texto(s, x + 6, y + (a - 16) / 2, t, 0xFFFFFF);
}

static void painel_desenhar(superficie_t *s)
{
    sup_retangulo(s, pop.x + 3, pop.y + 3, pop.l, pop.a, 0x0A0E12);   /* sombra */
    sup_retangulo(s, pop.x, pop.y, pop.l, pop.a, COR_PAINEL);
    sup_moldura(s, pop.x, pop.y, pop.l, pop.a, COR_PAINEL_BORDA);
    sup_retangulo(s, pop.x + 1, pop.y + 1, pop.l - 2, 2, COR_LINHA);
    char t[96], ip[20];
    if (pop.tipo == P_VOL) {
        snformatar(t, sizeof t, "%d %%", tb.mudo ? 0 : tb.vol);
        sup_texto(s, pop.x + 12, pop.y + 10, "Volumen", 0xFFFFFF);
        sup_texto(s, pop.x + pop.l - 12 - texto_largura(t), pop.y + 10, t, 0xD8DEE4);
        int sl = pop.l - 2 * VOL_SX;
        sup_retangulo(s, pop.x + VOL_SX, pop.y + VOL_SY, sl, VOL_SA, COR_TRILHO);
        sup_moldura(s, pop.x + VOL_SX, pop.y + VOL_SY, sl, VOL_SA, COR_PAINEL_BORDA);
        int fill = tb.vol * sl / 100;
        if (fill > 0) sup_retangulo(s, pop.x + VOL_SX, pop.y + VOL_SY, fill, VOL_SA, tb.mudo ? COR_ICO_CINZA : 0x14A055);
        sup_retangulo(s, pop.x + VOL_SX + fill - 3, pop.y + VOL_SY - 5, 7, 16, 0xE8EEF4);
        sup_moldura(s, pop.x + VOL_SX + fill - 3, pop.y + VOL_SY - 5, 7, 16, 0x4A5E70);
        /* "Silenciar" con su casilla */
        sup_retangulo(s, pop.x + VOL_SX, pop.y + BTN_Y, pop.l - 2 * VOL_SX, BTN_A, COR_BOTAO_BAND);
        sup_moldura(s, pop.x + VOL_SX, pop.y + BTN_Y, pop.l - 2 * VOL_SX, BTN_A, COR_PAINEL_BORDA);
        sup_retangulo(s, pop.x + VOL_SX + 5, pop.y + BTN_Y + 3, 12, 12, COR_TRILHO);
        sup_moldura(s, pop.x + VOL_SX + 5, pop.y + BTN_Y + 3, 12, 12, 0x8A96A2);
        if (tb.mudo) sup_retangulo(s, pop.x + VOL_SX + 8, pop.y + BTN_Y + 6, 6, 6, COR_ICO_ERRO);
        sup_texto(s, pop.x + VOL_SX + 24, pop.y + BTN_Y + 1, "Silenciar", 0xFFFFFF);
    } else if (pop.tipo == P_RED) {
        sup_texto(s, pop.x + 12, pop.y + 10, "Red", 0xFFFFFF);
        if (!tb.rede) {
            sup_texto(s, pop.x + 12, pop.y + 26, "Sin tarjeta de red", COR_MIN_TEXTO);
            return;
        }
        const char *est = rd.desativada ? "Desactivada" : !rd.cabo ? (bandeja_wifi() ? "Wi-Fi sin conectar" : "Cable desconectado")
                        : rd.ip ? (rd.metodo == REDE_POR_DHCP ? "Conectado (DHCP)" : "Conectado (fija)")
                        : (rd.dhcp_estado == 1 || rd.dhcp_estado == 2) ? "Buscando dirección (DHCP)..." : "Sin dirección IP";
        if (bandeja_wifi()) {
            /* D220: WEP y TKIP, con el aviso de que no protegen (o protegen poco) */
            if (wf.estado == WIFI_CONECTADO)
                snformatar(t, sizeof t, "%s (%u %%)%s", wf.ssid, wf.qualidade,
                           wf.seguranca == WIFI_WEP ? " WEP: INSEGURA" : (wf.detalhe & WIFI_DET_TKIP) ? " TKIP: débil" :
                           wf.seguranca == WIFI_WPA3 ? " WPA3" : "");
            else snformatar(t, sizeof t, "sin conectar (wifi buscar)");
            linha_painel(s, 28, "Wi-Fi: ", t);
        } else {
            snformatar(t, sizeof t, "%s (%s)", rd.descricao[0] ? rd.descricao : "eth0", rd.driver[0] ? rd.driver : "sin driver");
            linha_painel(s, 28, "Tarjeta: ", t);
        }
        linha_painel(s, 44, "Estado: ", est);
        ip_texto(ip, sizeof ip, rd.ip);
        linha_painel(s, 60, "IP: ", rd.ip ? ip : "-");
        ip_texto(ip, sizeof ip, rd.roteador);
        linha_painel(s, 76, "Puerta de enlace: ", rd.roteador ? ip : "-");
        ip_texto(ip, sizeof ip, rd.dns);
        linha_painel(s, 92, "DNS: ", rd.dns ? ip : "-");
        botao_painel(s, pop.x + 12, pop.y + pop.a - 30, RED_BTN_L, 20, "Renovar DHCP", false);
    } else if (pop.tipo == P_BAT) {
        const info_bateria_t *b = &tb.bat;
        bool carga = (b->estado & BATERIA_CARREGANDO) != 0;
        sup_texto(s, pop.x + 12, pop.y + 10, "Batería", 0xFFFFFF);
        if (b->porcentagem >= 0) snformatar(t, sizeof t, "%d %%", b->porcentagem);
        else snformatar(t, sizeof t, "?");
        sup_texto(s, pop.x + pop.l - 12 - texto_largura(t), pop.y + 10, t, 0xD8DEE4);
        int sl = pop.l - 2 * VOL_SX;   /* la barra del nivel, como la del volumen pero sin tirador */
        sup_retangulo(s, pop.x + VOL_SX, pop.y + 32, sl, 8, COR_TRILHO);
        sup_moldura(s, pop.x + VOL_SX, pop.y + 32, sl, 8, COR_PAINEL_BORDA);
        if (b->porcentagem > 0)
            sup_retangulo(s, pop.x + VOL_SX + 1, pop.y + 33, (sl - 2) * b->porcentagem / 100, 6,
                          b->porcentagem < BAT_COR_AVISO && !carga ? COR_BAT_BAIXA : 0x14A055);
        const char *est = carga ? "Cargando" : (b->estado & BATERIA_DESCARREGANDO) ? "Descargando"
                        : b->adaptador == 2 ? "Cargada (enchufado)" : "En reposo";
        linha_painel(s, 48, "Estado: ", (b->estado & BATERIA_CRITICA) ? "Crítica" : est);
        char tm[24];
        bateria_texto_tempo(b->minutos, tm, sizeof tm);
        linha_painel(s, 64, carga ? "Para llenarse: " : "Tiempo restante: ", tm[0] ? tm : "-");
        linha_painel(s, 80, "Cargador: ", b->adaptador == 2 ? "enchufado" : b->adaptador == 1 ? "desenchufado" : "no se sabe");
        const char *u = b->unidade_ma ? "mAh" : "mWh";
        if (b->restante != BATERIA_DESCONHECIDO && b->total != BATERIA_DESCONHECIDO)
            snformatar(t, sizeof t, "%u de %u %s", b->restante, b->total, u);
        else snformatar(t, sizeof t, "-");
        linha_painel(s, 96, "Capacidad: ", t);
        snformatar(t, sizeof t, "%s%s%s", b->modelo[0] ? b->modelo : "-", b->fabricante[0] ? ", " : "", b->fabricante);
        linha_painel(s, 112, "Modelo: ", t);
    } else {
        sup_texto(s, pop.x + 12, pop.y + 10, "Temperatura de la CPU", 0xFFFFFF);
        snformatar(t, sizeof t, "%d °C", tb.temp_mc / 1000);
        sup_texto(s, pop.x + 12, pop.y + 26, t, cor_temp());
        if (bandeja_limitada_termica()) sup_texto(s, pop.x + 12, pop.y + 42, "Limitada por temperatura", COR_TEMP_ALTA);
    }
}

bool barra_popup_visivel(void) { return pop.tipo || dica.ic >= 0; }   /* D195 */

void barra_popup_desenhar(superficie_t *s)
{
    if (pop.tipo) painel_desenhar(s);
    else if (dica.ic >= 0) {
        sup_retangulo(s, dica.x, dica.y, dica.l, 18, 0x10161C);
        sup_moldura(s, dica.x, dica.y, dica.l, 18, COR_PAINEL_BORDA);
        sup_texto(s, dica.x + 5, dica.y + 1, dica.t, 0xE8EEF4);
    }
}

/* ---- volumen */

static void volume_mudou(void)   /* tras tocar el volumen: vuelve a leer y ensucia lo que se ve */
{
    tb.hash = bandeja_ler();
    sujar_bandeja();
    sujar_painel();
    if (dica.ic == IC_VOL) { dica_texto(IC_VOL, dica.t, sizeof dica.t); sujar_dica(); }
}

static void volume_ajustar(int delta)
{
    if (tb.som != AUDIO_FUNCIONANDO) return;
    int v = tb.vol + delta;
    audio_mestre_definir(v < 0 ? 0 : v > 100 ? 100 : v, delta > 0 ? 0 : -1);
    volume_mudou();
}

static void volume_do_x(int mx)
{
    int sl = pop.l - 2 * VOL_SX;
    int v = (mx - (pop.x + VOL_SX)) * 100 / sl;
    v = v < 0 ? 0 : v > 100 ? 100 : v;
    if (v != tb.vol || tb.mudo) {
        audio_mestre_definir(v, v > 0 ? 0 : -1);
        volume_mudou();
    }
}

bool barra_roda(int x, int y, int dz)
{
    bool no_painel = pop.tipo == P_VOL && x >= pop.x && x < pop.x + pop.l && y >= pop.y && y < pop.y + pop.a;
    if (!no_painel && icone_em(x, y) != IC_VOL) return false;
    volume_ajustar(dz > 0 ? -5 : 5);   /* dz > 0: roda hacia abajo */
    return true;
}

/* ---- ratón y teclado con el panel abierto */

bool barra_popup_mouse(int x, int y, unsigned botoes, unsigned mudaram)
{
    if (!pop.tipo) return false;
    bool dentro = x >= pop.x && x < pop.x + pop.l && y >= pop.y && y < pop.y + pop.a;
    bool esq = (botoes & BOTAO_ESQUERDO) != 0;
    if (pop.arrasta) {
        if (!esq) { pop.arrasta = false; klog("[GUI] bandeja: volume solto em %d%%\n", tb.vol); }
        else volume_do_x(x);
        return true;
    }
    if (!(mudaram & botoes)) return dentro && mudaram != 0;   /* sólo se movió o soltó: no molesta fuera */
    if (!dentro) { painel_fechar(); klog("[GUI] bandeja: painel fechado\n"); return true; }
    if (!(mudaram & BOTAO_ESQUERDO)) return true;
    int rx = x - pop.x, ry = y - pop.y;
    if (pop.tipo == P_VOL) {
        if (ry >= VOL_SY - 6 && ry < VOL_SY + VOL_SA + 6 && rx >= VOL_SX - 4 && rx < pop.l - VOL_SX + 4) {
            pop.arrasta = true;
            volume_do_x(x);
        } else if (ry >= BTN_Y && ry < BTN_Y + BTN_A) {
            audio_mestre_definir(-1, !tb.mudo);
            klog("[GUI] bandeja: silencio %s\n", !tb.mudo ? "ligado" : "desligado");
            volume_mudou();
        }
    } else if (pop.tipo == P_RED && tb.rede && rx >= 12 && rx < 12 + RED_BTN_L && ry >= pop.a - 30 && ry < pop.a - 10) {
        klog("[GUI] bandeja: renovar DHCP\n");
        rede_renovar_dhcp();
        gui_aviso("Renovando el DHCP...");
    }
    return true;
}

bool barra_popup_tecla(int t)
{
    if (!pop.tipo) return false;
    if (t == TECLA_ESC) { painel_fechar(); return true; }
    if (pop.tipo == P_VOL) {
        if (t == TECLA_ESQUERDA || t == TECLA_BAIXO) { volume_ajustar(-5); return true; }
        if (t == TECLA_DIREITA || t == TECLA_CIMA) { volume_ajustar(5); return true; }
    }
    painel_fechar();   /* otra tecla: el panel se va y la tecla sigue su camino */
    return false;
}

static void bandeja_clicar(int id)
{
    if (id == IC_VOL && tb.som != AUDIO_FUNCIONANDO) { gui_aviso("No hay tarjeta de sonido."); return; }
    painel_abrir(id);
}

/* ---- una vez por segundo y al pasar el ratón */

static void bandeja_tique(void)
{
    uint32_t agora = relogio_tiques();
    if (!tb.lido || agora - tb.tique >= TIQUES_POR_SEGUNDO) {
        tb.tique = agora;
        bool tinha_temp = tb.temp_ok, tinha_bat = tb.bat_ok;
        uint32_t h = bandeja_ler();
        if (h != tb.hash) {
            tb.hash = h;
            char ip[20];
            ip_texto(ip, sizeof ip, rd.ip);
            klog("[GUI] bandeja: som %d volume %d%s, rede %d ip %s, temp %d, bat %d e%u a%u\n", tb.som, tb.vol,
                 tb.mudo ? " (silencio)" : "", tb.rede, ip, tb.temp_ok ? tb.temp_mc / 1000 : -1,
                 tb.bat_ok ? tb.bat.porcentagem : -1, tb.bat.estado, tb.bat.adaptador);
            if (tinha_temp == tb.temp_ok && tinha_bat == tb.bat_ok) { sujar_bandeja(); sujar_painel(); }
            else if (pop.tipo == P_BAT && !tb.bat_ok) painel_fechar();   /* se quitó la batería */
            if (dica.ic >= 0) { dica_texto(dica.ic, dica.t, sizeof dica.t); sujar_dica(); }
        }
        /* D192: batería baja, una notificación por bajada (sin ventanas). */
        bool baja = tb.bat_ok && (tb.bat.estado & BATERIA_DESCARREGANDO) && tb.bat.porcentagem >= 0 &&
                    tb.bat.porcentagem < BAT_AVISO;
        if (baja && !tb.bat_avisou) {
            char m[64];
            tb.bat_avisou = true;
            snformatar(m, sizeof m, "Batería baja: %d %%. Conecta el cargador.", tb.bat.porcentagem);
            klog("[GUI] bandeja: aviso de bateria baja (%d %%)\n", tb.bat.porcentagem);
            gui_aviso(m);
        } else if (!tb.bat_ok || !(tb.bat.estado & BATERIA_DESCARREGANDO) || tb.bat.porcentagem >= BAT_REARMAR) {
            tb.bat_avisou = false;
        }
    }
    int mx, my;
    mouse_posicao(&mx, &my);
    int ic = (pop.tipo || menu_aberto()) ? -1 : icone_em(mx, my);
    if (ic != dica.ic) {
        sujar_dica();
        dica.ic = ic;
        if (ic >= 0) {
            dica_texto(ic, dica.t, sizeof dica.t);
            dica.l = texto_largura(dica.t) + 10;
            int L = largura_barra();
            dica.x = icone_x(ic) + IC_L / 2 - dica.l / 2;
            if (dica.x > L - dica.l - 2) dica.x = L - dica.l - 2;
            if (dica.x < 2) dica.x = 2;
            dica.y = BARRA_SUPERIOR + 2;
            sujar_dica();
            klog("[GUI] bandeja: dica \"%s\"\n", dica.t);
        }
    } else if (ic >= 0) {
        int l = texto_largura(dica.t) + 10;
        if (l != dica.l) { sujar_dica(); dica.l = l; sujar_dica(); }
    }
}

/* INFO_BANDEJA (D114): lo que la bandeja muestra ahora, para pruebas y programas. */
int32_t barra_info_bandeja(uint32_t ptr, uint32_t n)
{
    if (n < sizeof(info_bandeja_t) || !usuario_ok((void *)ptr, sizeof(info_bandeja_t), true)) return ERRO_ENDERECO;
    info_bandeja_t *v = (info_bandeja_t *)ptr;
    memset(v, 0, sizeof *v);
    if (!tb.lido) tb.hash = bandeja_ler();
    v->versao = 2;
    v->som = (unsigned)tb.som;
    v->volume = (unsigned)tb.vol;
    v->silencio = (unsigned)tb.mudo;
    v->rede = (unsigned)tb.rede;
    v->ip = rd.ip;
    v->temp_ok = tb.temp_ok;
    v->temp_mili_c = tb.temp_mc;
    v->limitada = bandeja_limitada_termica();
    v->painel = (unsigned)pop.tipo;
    v->icones = (unsigned)lay.nic;
    v->bateria = tb.bat_ok;
    v->bateria_pct = tb.bat_ok ? tb.bat.porcentagem : -1;
    v->bateria_estado = tb.bat.estado;
    return 0;
}

void barra_desenhar(superficie_t *s)
{
    calcular();
    sup_retangulo(s, 0, 0, lay.L, BARRA_SUPERIOR, COR_BARRA);
    sup_retangulo(s, 0, BARRA_SUPERIOR - 1, lay.L, 1, COR_LINHA);
    desenhar_inicio(s);
    for (int i = 0; i < lay.nbt; i++) desenhar_tarefa(s, &lay.bt[i]);
    if (lay.mais_x >= 0) desenhar_mais(s);
    desenhar_lema(s);
    bandeja_desenhar(s);
    fuso_data_t d;
    hora_local(&d);
    char hora[48];
    texto_relogio(hora, sizeof hora, &d, relogio_completo());
    sup_texto(s, lay.relogio_x, 4, hora, 0xFFFFFF);
    if (d.minuto != minuto_mostrado || hora_ajustes() != ajustes_vistos) {
        minuto_mostrado = d.minuto;
        ajustes_vistos = hora_ajustes();
        char log[48];
        texto_relogio(log, sizeof log, &d, true);
        klog("[GUI] relogio: %s %s\n", log, d.abrev);
    }
}

/* A cada tic de la tarea de las ventanas. */
void barra_tique(void)
{
    bandeja_tique();   /* antes de calcular: el icono de temperatura cambia el ancho */
    calcular();
    uint32_t a = assinatura();
    if (a != assinatura_vista) {
        assinatura_vista = a;
        gui_redesenhar(0, 0, lay.L, BARRA_SUPERIOR);
        registrar();
    }
    fuso_data_t d;
    hora_local(&d);
    if (d.minuto != minuto_mostrado || hora_ajustes() != ajustes_vistos)
        gui_redesenhar(lay.relogio_x - 4, 0, lay.L - lay.relogio_x + 4, BARRA_SUPERIOR);
}

/* ------------------------------------------------------------ menú Inicio */

enum { G_ACESSORIOS, G_GRAFICOS, G_INTERNET, G_MULTIMEDIA, G_EDUCACION, G_JUEGOS, G_SISTEMA, QTD_GRUPOS };
static const char *const nome_grupo[QTD_GRUPOS] = { "Accesorios", "Gráficos", "Internet", "Multimedia", "Educación", "Juegos", "Sistema" };

typedef struct {
    const char *nome, *caminho, *argumento, *processo;
    int grupo;
    const desenho_t *icone;
    bool unico;                  /* una sola ventana: si ya está abierta, sólo sube */
} programa_t;

static const programa_t programas[] = {
    { "Archivos",                "/bin/arquivos",    NULL, "arquivos",    G_ACESSORIOS, &arte_icone_arquivos, false },
    { "Calculadora",             "/bin/calculadora", NULL, "calculadora", G_ACESSORIOS, &arte_icone_calculadora, false },
    { "Editor de texto",         "/bin/editor",      NULL, "editor",      G_ACESSORIOS, &arte_icone_editor, false },
    { "Recortes",                "/bin/recortes",    NULL, "recortes",    G_ACESSORIOS, &arte_mini_recortes, true },   /* D125, D139 */
    { "Iconos",                  "/bin/icones",      NULL, "icones",      G_ACESSORIOS, &arte_mini_icones, false },  /* D135, D139 */
    { "Música",                  "/bin/musica",      NULL, "musica",      G_ACESSORIOS, &arte_mini_musica, false },
    { "MIDI",                    "/bin/midi",        NULL, "midi",        G_ACESSORIOS, &arte_mini_musica, false },   /* D190 */
    { "Sintonía (reproductor)",  "/apps/bin/sintonia", NULL, "sintonia",  G_MULTIMEDIA, &arte_mini_musica, false },   /* D225: en /apps (D194); sólo si está */
    { "Markdown",                "/bin/markdown",    NULL, "markdown",    G_ACESSORIOS, &arte_mini_markdown, false },
    { "Mico (editor de código)", "/bin/mico",        NULL, "mico",        G_ACESSORIOS, &arte_mini_mico, false },
    { "VisualPythonS (diseñador)", "/bin/python", "/lib/python/vps/disenador.py", "vps", G_ACESSORIOS, &arte_mini_vps, false },   /* D297: melhoria 123 */
    { "Explorador de bases de datos", "/bin/python", "/lib/python/basedatos/explorador.py", "exploradorbd", G_ACESSORIOS, &arte_mini_exploradorbd, false },   /* D304: melhoria 127 */
    { "Pluma (procesador de textos)", "/bin/pluma", NULL, "pluma",      G_ACESSORIOS, &arte_mini_pluma, false },   /* D291: melhoria 116 */
    { "Macaco (hoja de cálculo)", "/bin/macaco",    NULL, "macaco",      G_ACESSORIOS, &arte_mini_macaco, false },   /* D142: melhoria 078 */
    { "Paint",                   "/bin/paint",       NULL, "paint",       G_GRAFICOS,   &arte_icone_paint, false },
    { "Visor de imágenes",       "/bin/visor",       NULL, "visor",       G_GRAFICOS,   &arte_icone_visor, false },
    { "Visor de PDF",            "/bin/visorpdf",    NULL, "visorpdf",    G_ACESSORIOS, &arte_mini_visorpdf, false },  /* D139: melhoria 059 */
    { "Reproductor de vídeo",    "/bin/reproductor", NULL, "reproductor", G_GRAFICOS,   &arte_mini_video, false },      /* D139: melhoria 048 */
    { "Grabadora de sonido",     "/bin/grabadora",   NULL, "grabadora",   G_MULTIMEDIA, &arte_mini_musica, false },   /* D292: micrófono */
    { "Cámara",                  "/bin/camara",      NULL, "camara",      G_GRAFICOS,   &arte_mini_video, false },      /* D277: cámaras UVC y fuente de prueba */
    { "Cipó (navegador)",        "/bin/cipo",        NULL, "cipo",        G_INTERNET,   &arte_mini_cipo, false },
    { "GebraMini (geometría dinámica)", "/bin/gebramini", NULL, "gebramini", G_EDUCACION, &arte_mini_gebramini, false },   /* D309: mejora 128 */
    { "Actividades JClic",       "/bin/jclic",       NULL, "jclic",       G_EDUCACION,  &arte_mini_jclic, false },        /* D185: visor de actividades educativas */
    { "Actividades JClic (bajar)", "/bin/jclicbajar", NULL, "jclicbajar", G_EDUCACION,  &arte_mini_jclicbajar, true },   /* D185: descargador del repositorio del XTEC */
#ifndef SAGUI_SEM_GPL   /* D184: ni el nombre en la variante sin GPL (la auditoría h81 busca las marcas) */
    { "NetSurf (navegador)",     "/apps/bin/netsurf", NULL, "netsurf",    G_INTERNET,   &arte_mini_netsurf, false },  /* D184, D194: en /apps; sólo si está */
#endif
    { "Sagui Atrapa Bananas",    "/bin/banana",      NULL, "banana",      G_JUEGOS,     &arte_icone_banana, false },
    { "Solitario Spider",        "/bin/spider",      NULL, "spider",      G_JUEGOS,     &arte_mini_spider, false },   /* D173 */
    { "Buscaminas",              "/bin/buscaminas",  NULL, "buscaminas",  G_JUEGOS,     &arte_mini_buscaminas, false },   /* D248 */
#ifndef SAGUI_SEM_GPL   /* D184: la h81 encontraba «OpenTyrian» en el kernel sin GPL */
    { "OpenTyrian",              "/bin/opentyrian",  NULL, "opentyrian",  G_JUEGOS,     &arte_mini_tyrian, false },   /* D180: sólo si está en la imagen (no en la variante sin GPL) */
    { "OpenTyrian (pantalla completa)", "/bin/opentyrian", "--pantalla-completa", "opentyrian-cheia", G_JUEGOS, &arte_mini_tyrian, false },   /* D204 */
    { "OpenJazz",                "/apps/bin/openjazz", NULL, "openjazz",  G_JUEGOS,     &arte_mini_openjazz, false },   /* D228: en /apps; sólo si está */
    { "OpenJazz (pantalla completa)", "/apps/bin/openjazz", "--pantalla-completa", "openjazz-cheia", G_JUEGOS, &arte_mini_openjazz, false },   /* D228 */
    { "Command & Conquer",       "/apps/bin/conquer", NULL, "conquer",    G_JUEGOS,     &arte_mini_conquer, false },   /* D230: Vanilla Conquer, en /apps; sólo si está */
    { "Quake",                   "/apps/bin/quake",  NULL, "quake",       G_JUEGOS,     &arte_mini_quake, false },   /* D229: en /apps; sólo si está */
    { "Quake (pantalla completa)", "/apps/bin/quake", "--pantalla-completa", "quake-cheia", G_JUEGOS, &arte_mini_quake, false },   /* D229 */
    { "Game Boy (GNUBoy)",       "/apps/bin/gnuboy", NULL, "gnuboy",      G_JUEGOS,     &arte_mini_gnuboy, false },    /* D235: SDL2-GNUBoy, en /apps; sólo si está */
    { "Recreativa (juegos de DOS)", "/apps/bin/recreativa", NULL, "recreativa", G_JUEGOS, &arte_mini_recreativa, false },   /* D294: el lanzador de juegos de DOS, en /apps; sólo si está */
    { "DOSBox",                  "/apps/bin/dosbox", NULL, "dosbox",      G_JUEGOS,     &arte_mini_dosbox, false },   /* D294: DOSBox 0.74, en /apps; sólo si está */
    { "DOSBox (pantalla completa)", "/apps/bin/dosbox", "--pantalla-completa", "dosbox-cheia", G_JUEGOS, &arte_mini_dosbox, false },   /* D294 */
#endif
    { "Sagui 3D",                "/bin/sagui3d",     NULL, "sagui3d",     G_JUEGOS,     &arte_icone_sagui3d, false },
    { "CHIP-8",                  "/bin/chip8",       NULL, "chip8",       G_JUEGOS,     &arte_icone_chip8, false },
    { "ROM de CHIP-8 (bajar)",   "/bin/python", "/bin/chip8-roms-gui", "python", G_JUEGOS, &arte_icone_chip8, false },   /* D123 */
    { "Terminal",                "/bin/terminal",    NULL, "terminal",    G_SISTEMA,    &arte_icone_terminal, false },
    { "Administrador de tareas", "/bin/tareas",      NULL, "tareas",      G_SISTEMA,    &arte_icone_tarefas, true },
    { "Información del PC",      "/bin/infopc",      NULL, "infopc",      G_SISTEMA,    &arte_mini_infopc, false },
    { "Pantalla",                "/bin/pantalla",    "--ventana", "pantalla", G_SISTEMA, &arte_mini_pantalla, true },
    { "Fecha y hora",            "/bin/zona",        "--ventana", "zona",     G_SISTEMA, &arte_mini_relogio, true },
    { "Configuración",           "/bin/configuracion", NULL, "configuracion", G_SISTEMA, &arte_icone_configuracion, true },
    { "Discos",                  "/bin/discos",      "--ventana", "discos", G_SISTEMA, &arte_icone_discos, true },   /* D121 */
    { "Bluetooth",               "/bin/bluetooth",   "--ventana", "bluetooth", G_SISTEMA, &arte_mini_bluetooth, true },   /* D279 */
    { "Instalar SaguiOS",        "/bin/instalar",    "--ventana", "instalar", G_SISTEMA, &arte_icone_discos, true },   /* D179 */
    { "IA (chat)",               "/bin/ia",          NULL, "ia",          G_ACESSORIOS, &arte_mini_ia, false },        /* D139: D134 */
    { "Chat IA",                 "/bin/chatia",      NULL, "chatia",      G_ACESSORIOS, &arte_mini_chatia, false },    /* D143: janela e contexto */
    { "Correo",                  "/bin/correo",      NULL, "correo",      G_INTERNET,   &arte_mini_correo, false },    /* D146: cliente de correio (082) */
    { "Escritorio remoto (VNC)", "/bin/vnc",         NULL, "vnc",         G_INTERNET,   &arte_mini_pantalla, false },  /* D254: cliente VNC (097); sin icono propio todavía */
};
#define QTD_PROGRAMAS (int)(sizeof programas / sizeof programas[0])
#define MAIS_USADOS 4

/* D180: el OpenTyrian es GPL y no va en la variante sin GPL (D109): sin
 * /bin/opentyrian, su entrada no aparece en el menú. D184: lo mismo con
 * NetSurf (GPL, y además solo se compila si se bajaron sus fuentes).
 * D194: las apps de /apps (la partición SAGUI-APPS) sólo salen si su
 * ejecutable está: sin la partición, el menú no las muestra. */
static bool programa_disponivel(const programa_t *p)
{
    if (txt_comeca_com(p->caminho, "/apps/")) return vfs_existe_arquivo(p->caminho);
#ifdef SAGUI_SEM_GPL
    return true;   /* en esta variante la entrada del OpenTyrian ni siquiera existe */
#else
    if (!txt_comeca_com(p->processo, "opentyrian")) return true;   /* D204: también «opentyrian-cheia» */
    return vfs_achar(p->caminho) != NULL;
#endif
}

/* El nombre del programa (argv[0]): lo que va tras la última barra de la ruta. */
static const char *nome_programa(const char *caminho)
{
    const char *n = caminho;
    for (const char *c = caminho; *c; c++)
        if (*c == '/') n = c + 1;
    return n;
}

/* Cuántas veces abrió cada programa la primera ventana desde el arranque (D065).
 * Queda sólo en la memoria: el SaguiOS no tiene dónde guardar preferencias de la
 * sesión gráfica todavía. Sin uso, valen los de la orden de abajo. */
static uint16_t usos[QTD_PROGRAMAS];
/* Por el nombre del proceso, no por el índice: la tabla crece en el medio (cada
 * programa nuevo entra en su grupo) y un índice fijo apuntaría a otro. */
static const char *const preferidos[MAIS_USADOS] = { "terminal", "arquivos", "editor", "cipo" };

void barra_janela_criada(const char *processo)
{
    for (int i = 0; i < QTD_PROGRAMAS; i++)
        if (txt_compara(processo, programas[i].processo) == 0 && usos[i] < 60000) usos[i]++;
}

/* Los MAIS_USADOS programas con más usos; empate (y cero) por el orden de los preferidos. */
static int mais_usados(int *v)
{
    int n = 0;
    while (n < MAIS_USADOS) {
        int melhor = -1, melhor_pref = 99;
        for (int i = 0; i < QTD_PROGRAMAS; i++) {
            bool ja = false;
            for (int k = 0; k < n; k++) ja |= v[k] == i;
            if (ja) continue;
            int pref = 50;
            for (int k = 0; k < MAIS_USADOS; k++) if (txt_compara(preferidos[k], programas[i].processo) == 0) pref = k;
            if (melhor < 0 || usos[i] > usos[melhor] || (usos[i] == usos[melhor] && pref < melhor_pref)) {
                melhor = i;
                melhor_pref = pref;
            }
        }
        if (melhor < 0) break;
        v[n++] = melhor;
    }
    return n;
}

enum { A_PROGRAMA = 0, A_EJECUTAR = 100, A_PANTALLA, A_ACERCA, A_REINICIAR, A_APAGAR, A_TAREFAS, A_FECHA, A_VPS = 200, A_JANELA = 1000 };
#define VPS_MAX 24   /* aplicaciones generadas por VisualPythonS que caben en el menú (D297) */

static menu_lista_t menu_inicio;

/* D297: las aplicaciones que genera VisualPythonS (mejora 123) viven en /apps/vps/<Nombre>/ (o en
 * /home/vps/<Nombre>/ si no hay partición de apps) con un lanzador.py: cada carpeta que lo tiene
 * es una entrada de «Mis aplicaciones». El menú las lee cada vez que se abre. */
static char vps_ruta[VPS_MAX][NOME_MAX + 16];
static char vps_nombre[VPS_MAX][NOME_MAX];
static int  vps_cuantas;

static bool vps_listar(const char *caminho, int i, entrada_pasta_t *e)
{
    const char *rel;
    montagem_t *m = montagem_de(caminho, &rel);   /* /apps es una partición montada, no el árbol del ramdisk */
    if (m) return m->fs->listar(m->priv, rel, i, e) == 1;
    return vfs_listar(caminho, i, e) == 1;
}

static void vps_buscar(const char *base)
{
    entrada_pasta_t e;
    for (int i = 0; vps_cuantas < VPS_MAX && vps_listar(base, i, &e); i++) {
        if (e.tipo != TIPO_PASTA) continue;
        char l[2 * NOME_MAX + 32];
        snformatar(l, sizeof l, "%s/%s/lanzador.py", base, e.nome);
        if (!vfs_existe_arquivo(l)) continue;
        snformatar(vps_ruta[vps_cuantas], sizeof vps_ruta[0], "%s/%s", base, e.nome);
        txt_copia_n(vps_nombre[vps_cuantas], e.nome, NOME_MAX);
        vps_cuantas++;
    }
}

static void executar_vps(int n)
{
    char l[2 * NOME_MAX + 32];
    snformatar(l, sizeof l, "%s/lanzador.py", vps_ruta[n]);
    char *argv[3] = { "python", l, NULL };
    int pid = processo_executar("/bin/python", 2, argv, NULL);
    klog("[GUI] inicio -> vps %s (pid %d)\n", l, pid);
    if (pid < 0) gui_aviso("No se pudo abrir la aplicación.");
}

static void executar_programa(const programa_t *p)
{
    char *argv[3] = { (char *)nome_programa(p->caminho), (char *)p->argumento, NULL };
    int argc = p->argumento ? 2 : 1;
    if (p->unico) { gui_abrir_unico(p->processo, p->caminho, argc, argv); return; }
    int pid = processo_executar(p->caminho, argc, argv, NULL);
    klog("[GUI] inicio -> %s (pid %d)\n", p->caminho, pid);
    if (pid < 0) gui_aviso("No se pudo abrir el programa.");
}

static void executar_ajudante(const char *argumento)
{
    char *argv[3] = { "ejecutar", (char *)argumento, NULL };
    int pid = processo_executar("/bin/ejecutar", argumento ? 2 : 1, argv, NULL);
    klog("[GUI] inicio -> /bin/ejecutar%s%s (pid %d)\n", argumento ? " " : "", argumento ? argumento : "", pid);
}

static void resposta_inicio(int indice, int janela, int dado)
{
    (void)janela; (void)dado;
    inicio_aberto = false;
    int a = menu_lista_acao(&menu_inicio, indice);
    if (a >= A_PROGRAMA && a < A_PROGRAMA + QTD_PROGRAMAS) executar_programa(&programas[a - A_PROGRAMA]);
    else if (a >= A_VPS && a < A_VPS + VPS_MAX) executar_vps(a - A_VPS);
    else if (a == A_EJECUTAR) executar_ajudante(NULL);
    else if (a == A_PANTALLA) area_abrir_pantalla();
    else if (a == A_ACERCA) {
        char *argv[] = { "sobre-o-sistema", NULL };
        gui_abrir_unico("sobre-o-sistema", "/bin/sobre-o-sistema", 1, argv);
    }
    else if (a == A_REINICIAR) executar_ajudante("--reiniciar");   /* pregunta antes: puede haber algo sin guardar */
    else if (a == A_APAGAR) executar_ajudante("--apagar");         /* D113: también pregunta */
}

void barra_abrir_inicio(bool teclado)
{
    if (inicio_aberto) { menu_cancelar(); return; }   /* el mismo botón (o tecla) cierra */
    menu_lista_t *ml = &menu_inicio;
    menu_lista_iniciar(ml);
    int top[MAIS_USADOS], n = mais_usados(top);
    for (int i = 0; i < n; i++) menu_lista_linha(ml, programas[top[i]].nome, A_PROGRAMA + top[i], programas[top[i]].icone);
    menu_lista_linha(ml, "-", MENU_NADA, NULL);
    for (int g = 0; g < QTD_GRUPOS; g++) {
        char t[MENU_MAX_TEXTO + 1];
        snformatar(t, sizeof t, ">%s", nome_grupo[g]);
        menu_lista_linha(ml, t, MENU_NADA, NULL);
        for (int i = 0; i < QTD_PROGRAMAS; i++)
            if (programas[i].grupo == g && programa_disponivel(&programas[i])) menu_lista_linha(ml, programas[i].nome, A_PROGRAMA + i, programas[i].icone);
        menu_lista_linha(ml, "<", MENU_NADA, NULL);
    }
    vps_cuantas = 0;
    vps_buscar("/apps/vps");
    vps_buscar("/home/vps");
    if (vps_cuantas) {
        menu_lista_linha(ml, ">Mis aplicaciones", MENU_NADA, NULL);
        for (int i = 0; i < vps_cuantas; i++) menu_lista_linha(ml, vps_nombre[i], A_VPS + i, &arte_mini_vps);
        menu_lista_linha(ml, "<", MENU_NADA, NULL);
    }
    menu_lista_linha(ml, "-", MENU_NADA, NULL);
    menu_lista_linha(ml, "Ejecutar...", A_EJECUTAR, &arte_inicio);
    menu_lista_linha(ml, "Configuración de pantalla", A_PANTALLA, &arte_mini_pantalla);
    menu_lista_linha(ml, "Acerca de " SAGUI_NOME, A_ACERCA, &arte_icone_sobre);
    menu_lista_linha(ml, "-", MENU_NADA, NULL);
    /* D113: "Apagar..." siempre se puede elegir. Sin ACPI utilizable, el
     * apagado graba los discos y avisa de que ya se puede apagar con el botón. */
    menu_lista_linha(ml, "Apagar...", A_APAGAR, NULL);
    menu_lista_linha(ml, "Reiniciar...", A_REINICIAR, NULL);
    if (!menu_lista_abrir(INICIO_X, BARRA_SUPERIOR, ml, resposta_inicio, 0, 0)) return;
    inicio_aberto = true;
    gui_redesenhar(0, 0, (int)video_largura(), BARRA_SUPERIOR);
    klog("[GUI] menu Inicio aberto (%s)\n", teclado ? "teclado" : "mouse");
    if (teclado) menu_destacar_primeira();
}

/* ------------------------------------------------------------ clics */

static menu_lista_t menu_extra;

static void resposta_extra(int indice, int janela, int dado)
{
    (void)janela; (void)dado;
    int a = menu_lista_acao(&menu_extra, indice);
    if (a >= A_JANELA) gui_janela_ativar(a - A_JANELA);
    else if (a == A_TAREFAS) gui_abrir_tarefas();
    else if (a == A_FECHA) {   /* D080: la ventana "Fecha y hora" */
        char *argv[] = { "zona", "--ventana", NULL };
        gui_abrir_unico("zona", "/bin/zona", 2, argv);
    }
}

/* "»": las ventanas que no cupieron en la barra. */
static void menu_ocultas(void)
{
    menu_lista_t *ml = &menu_extra;
    menu_lista_iniciar(ml);
    for (int i = 0; i < lay.nocultas; i++) {
        janela_barra_t *j = &jb[lay.ocultas[i]];
        char t[MENU_MAX_TEXTO + 1];
        snformatar(t, sizeof t, "%s%s", j->foco ? "*" : "", j->titulo);
        menu_lista_linha(ml, t, A_JANELA + j->id, icone_do_processo(j->processo));
    }
    menu_lista_abrir(lay.mais_x, BARRA_SUPERIOR, ml, resposta_extra, 0, 0);
}

static void mostrar_data(void)
{
    static const char *const dias[] = { "domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado" };
    static const char *const meses[] = { "enero", "febrero", "marzo", "abril", "mayo", "junio", "julio",
                                         "agosto", "septiembre", "octubre", "noviembre", "diciembre" };
    fuso_data_t d;
    hora_local(&d);
    char t[96], h[16];
    texto_hora(h, sizeof h, &d);
    snformatar(t, sizeof t, "%s, %u de %s de %u, %s %s", dias[d.dia_semana % 7],
               d.dia, meses[(d.mes >= 1 && d.mes <= 12) ? d.mes - 1 : 0], d.ano, h, d.abrev);
    gui_aviso(t);
}

bool barra_tratar_clique(int x, int y, unsigned botoes)
{
    calcular();
    bool esq = (botoes & BOTAO_ESQUERDO) != 0, dir = (botoes & BOTAO_DIREITO) != 0;
    if (x >= INICIO_X && x < INICIO_X + INICIO_L) { barra_abrir_inicio(false); return true; }
    for (int i = 0; i < lay.nbt; i++) {
        botao_t *b = &lay.bt[i];
        if (x < b->x || x >= b->x + b->l) continue;
        janela_barra_t *j = &jb[b->j];
        if (dir) { gui_janela_menu(j->id, x, BARRA_SUPERIOR); return true; }
        if (!esq) return true;
        /* Como en Windows: con el foco, el botón minimiza; sin él, trae hacia arriba. */
        klog("[GUI] barra: clique na tarefa %d (%s)\n", j->id, j->foco ? "minimizar" : "ativar");
        if (j->foco) gui_janela_minimizar(j->id);
        else gui_janela_ativar(j->id);
        return true;
    }
    if (lay.mais_x >= 0 && x >= lay.mais_x && x < lay.mais_x + MAIS_L) { menu_ocultas(); return true; }
    int ic = icone_em(x, y);
    if (ic >= 0) { if (esq) bandeja_clicar(ic); return true; }
    if (x >= lay.relogio_x - 6) {
        if (esq) mostrar_data();
        else if (dir) {   /* D080: botón derecho en el reloj */
            menu_lista_iniciar(&menu_extra);
            menu_lista_linha(&menu_extra, "Ajustar fecha y hora...", A_FECHA, NULL);
            menu_lista_abrir(x - 150 > 0 ? x - 150 : 0, BARRA_SUPERIOR, &menu_extra, resposta_extra, 0, 0);
            klog("[GUI] relogio: menu de contexto\n");
        }
        return true;
    }
    if (dir) {   /* la barra vacía: el atajo del Administrador de tareas */
        menu_lista_iniciar(&menu_extra);
        menu_lista_linha(&menu_extra, "Administrador de tareas\tCtrl+Mayús+Esc", A_TAREFAS, &arte_icone_tarefas);
        menu_lista_abrir(x, BARRA_SUPERIOR, &menu_extra, resposta_extra, 0, 0);
    }
    return true;
}
