// SPDX-License-Identifier: GPL-2.0-or-later
/* avisos.c - notificaciones emergentes con el texto completo (issue #16, D269).
 *
 * Antes: 96 bytes, dos líneas de 49 letras y 5 s fijos. Ahora: hasta AV_TEXTO bytes, el corte de
 * línea se calcula midiendo el texto con la fuente del sistema (texto_largura) y respetando las
 * palabras y los caracteres de varios bytes de UTF-8; la caja crece en alto hasta AV_LINEAS_MAX
 * líneas (si hay más, la última acaba en «...»); cada aviso dura según su longitud (mínimo 5 s) y
 * los que llegan mientras hay uno visible esperan en una cola. */
#include "avisos.h"
#include "acoes.h"
#include "gui.h"
#include "idioma.h"
#include "console.h"
#include "processo.h"
#include "relogio.h"
#include "texto.h"
#include "video.h"
#include "telas.h"
#include "versao.h"

#define AVISO_L        420
#define AV_MARGEN      16
#define AV_TEXTO       512
#define AV_LINEAS_MAX  14
#define AV_FILA        6
#define AV_LINEA_A     16
#define AV_CABECERA    24
#define AV_ALTO_MIN    62

typedef struct { char texto[AV_TEXTO]; char caminho[160]; bool audio; uint32_t seg; bool osd; } av_t;

#define OSD_SEGUNDOS 2   /* D403: lo que dura el aviso de una tecla (volumen, brillo...) */

static av_t fila[AV_FILA];
static int f_ini, f_qtd;
static bool ativo;
static av_t atual;
static uint32_t ate;
static int linha_ini[AV_LINEAS_MAX], linha_tam[AV_LINEAS_MAX], qtd_linhas, alto;
static bool linha_reticencias;

/* Anchura en píxeles de los n primeros bytes de t con la fuente del sistema. */
static int medir(const char *t, int n)
{
    char b[128];
    if (n > (int)sizeof b - 1) n = (int)sizeof b - 1;
    for (int i = 0; i < n; i++) b[i] = t[i];
    b[n] = 0;
    return texto_largura(b);
}

/* Parte el texto en líneas que caben en el ancho de la caja. */
static void quebrar(const char *t)
{
    const int maxw = AVISO_L - 2 * AV_MARGEN;
    qtd_linhas = 0;
    linha_reticencias = false;
    while (*t) {
        if (qtd_linhas == AV_LINEAS_MAX) { linha_reticencias = true; break; }
        int n = 0;
        while (t[n] && t[n] != '\n' && medir(t, n + 1) <= maxw) n++;
        int salto = 0;
        if (!t[n]) { /* fin */ }
        else if (t[n] == '\n') salto = 1;
        else {   /* la línea se llenó: último espacio, o corte en seco sin partir un carácter UTF-8 */
            int k = n;
            while (k > 0 && t[k] != ' ') k--;
            if (k > 0) {
                n = k;
                while (n > 0 && t[n - 1] == ' ') n--;
                salto = k - n;
                while (t[n + salto] == ' ') salto++;
            } else {
                if (n == 0) n = 1;
                while (n > 1 && ((unsigned char)t[n] & 0xC0) == 0x80) n--;
            }
        }
        linha_ini[qtd_linhas] = (int)(t - atual.texto);
        linha_tam[qtd_linhas] = n;
        qtd_linhas++;
        t += n + salto;
    }
    if (qtd_linhas == 0) { linha_ini[0] = 0; linha_tam[0] = 0; qtd_linhas = 1; }
    if (linha_reticencias) {   /* la última línea cede sitio a «...» */
        int i = qtd_linhas - 1;
        while (linha_tam[i] > 0 && medir(atual.texto + linha_ini[i], linha_tam[i]) + 24 > maxw) linha_tam[i]--;
    }
    alto = AV_CABECERA + qtd_linhas * AV_LINEA_A + 8;
    if (alto < AV_ALTO_MIN) alto = AV_ALTO_MIN;
}

void avisos_retangulo(int *x, int *y, int *l, int *a)
{
    int pl, pa;
    telas_principal_rect(&pl, &pa);   /* D290: en la pantalla principal, donde está la barra */
    *x = pl - AVISO_L - 12;
    *y = BARRA_SUPERIOR + 10;
    *l = AVISO_L;
    *a = alto;
}

static void redibujar(void)
{
    int x, y, l, a;
    avisos_retangulo(&x, &y, &l, &a);
    gui_redesenhar(x - 2, y - 2, l + 4, a + 4);
}

static void ativar(void)
{
    if (!f_qtd) return;
    atual = fila[f_ini];
    f_ini = (f_ini + 1) % AV_FILA;
    f_qtd--;
    quebrar(atual.texto);
    uint32_t s = atual.seg;
    if (!s) {   /* según la longitud: 5 s y 1 más por cada 25 letras, hasta 25 s */
        s = 5 + txt_tamanho(atual.texto) / 25;
        if (s > 25) s = 25;
    }
    if (atual.seg == 0 && f_qtd >= 3 && s > 5) s = 5;   /* con cola larga, no se hace esperar */
    ate = relogio_tiques() + s * TIQUES_POR_SEGUNDO;
    ativo = true;
    redibujar();
}

static void encolar(const char *texto, const char *caminho, bool audio, uint32_t segundos, bool osd)
{
    if (f_qtd == AV_FILA) { f_ini = (f_ini + 1) % AV_FILA; f_qtd--; }   /* cola llena: se pierde el más viejo */
    av_t *n = &fila[(f_ini + f_qtd) % AV_FILA];
    txt_copia_n(n->texto, texto, sizeof n->texto);
    txt_copia_n(n->caminho, caminho ? caminho : "", sizeof n->caminho);
    n->audio = audio && n->caminho[0];
    n->seg = segundos;
    n->osd = osd;
    f_qtd++;
    klog("[GUI] notificacao: %s\n", texto);
    if (!ativo) ativar();
}

void gui_aviso_ex(const char *texto, const char *caminho, bool audio, uint32_t segundos)
{
    encolar(texto, caminho, audio, segundos, false);
}

/* D403: el aviso de una tecla de función (acoes.h). Si ya se ve uno de estos, se
 * cambia su texto y vuelve a durar OSD_SEGUNDOS; si hay uno en la cola, se
 * cambia ese; si no, se encola como cualquier aviso, pero corto. */
void gui_aviso_osd(int tipo, int valor)
{
    char t[96];
    switch (tipo) {
    case OSD_VOLUME:     snformatar(t, sizeof t, _("Volumen: %d %%"), valor); break;
    case OSD_SILENCIO:   snformatar(t, sizeof t, _("Volumen: silencio")); break;
    case OSD_SEM_SOM:    snformatar(t, sizeof t, _("No hay tarjeta de sonido.")); break;
    case OSD_BRILHO:     snformatar(t, sizeof t, _("Brillo: %d %%"), valor); break;
    case OSD_SEM_BRILHO: snformatar(t, sizeof t, _("Este equipo no deja cambiar el brillo.")); break;
    case OSD_RADIO:
        snformatar(t, sizeof t, "%s", valor ? _("Radio inalámbrica: apagada (modo avión)") : _("Radio inalámbrica: encendida"));
        break;
    case OSD_TELA:       snformatar(t, sizeof t, _("Cambio de pantalla: usa la ventana Pantalla.")); break;
    default: return;
    }
    klog("[GUI] osd %d %d: %s\n", tipo, valor, t);
    if (ativo && atual.osd) {
        redibujar();   /* lo viejo (la caja puede cambiar de alto) */
        txt_copia_n(atual.texto, t, sizeof atual.texto);
        quebrar(atual.texto);
        ate = relogio_tiques() + OSD_SEGUNDOS * TIQUES_POR_SEGUNDO;
        redibujar();
        return;
    }
    for (int k = 0; k < f_qtd; k++) {
        av_t *a = &fila[(f_ini + k) % AV_FILA];
        if (a->osd) { txt_copia_n(a->texto, t, sizeof a->texto); return; }
    }
    encolar(t, NULL, false, OSD_SEGUNDOS, true);
}

void gui_aviso(const char *texto) { gui_aviso_ex(texto, NULL, false, 0); }

bool avisos_visivel(void) { return ativo; }

void avisos_desenhar(superficie_t *t)
{
    if (!ativo) return;
    int x, y, l, a;
    avisos_retangulo(&x, &y, &l, &a);
    sup_retangulo(t, x, y, l, a, 0x18202A);
    sup_moldura(t, x, y, l, a, 0xFFDF4A);
    sup_retangulo(t, x, y, 6, a, 0xFFDF4A);
    sup_texto(t, x + AV_MARGEN, y + 6, SAGUI_NOME, 0xFFDF4A);
    for (int i = 0; i < qtd_linhas; i++) {
        char b[128];
        int n = linha_tam[i];
        if (n > (int)sizeof b - 4) n = (int)sizeof b - 4;
        for (int k = 0; k < n; k++) b[k] = atual.texto[linha_ini[i] + k];
        b[n] = 0;
        if (linha_reticencias && i == qtd_linhas - 1) { b[n++] = '.'; b[n++] = '.'; b[n++] = '.'; b[n] = 0; }
        sup_texto(t, x + AV_MARGEN, y + AV_CABECERA + i * AV_LINEA_A, b, 0xFFFFFF);
    }
}

bool avisos_clique(int mx, int my)
{
    if (!ativo || !atual.caminho[0]) return false;
    int x, y, l, a;
    avisos_retangulo(&x, &y, &l, &a);
    if (mx < x || mx >= x + l || my < y || my >= y + a) return false;
    int pid;
    if (atual.audio) {   /* D233: Sintonía con la lista del CD; sin /apps, el reproductor del sistema con la carpeta */
        char *av[] = { "sintonia", atual.caminho, NULL };
        pid = processo_executar("/apps/bin/sintonia", 2, av, NULL);
        if (pid < 0) {
            char pasta[160];
            txt_copia_n(pasta, atual.caminho, sizeof pasta);
            for (int k = (int)txt_tamanho(pasta) - 1; k > 0; k--) if (pasta[k] == '/') { pasta[k] = 0; break; }
            char *am[] = { "musica", pasta, NULL };
            pid = processo_executar("/apps/bin/musica", 2, am, NULL);
        }
        klog("[GUI] aviso pulsado: reproducir %s (pid %d)\n", atual.caminho, pid);
    } else {   /* D125 */
        char *av[] = { "visor", atual.caminho, NULL };
        pid = processo_executar("/bin/visor", 2, av, NULL);
        klog("[GUI] aviso pulsado: visor %s (pid %d)\n", atual.caminho, pid);
    }
    redibujar();
    ativo = false;
    ativar();   /* el siguiente de la cola, si lo hay */
    return true;
}

void avisos_tique(void)
{
    if (ativo && (int32_t)(relogio_tiques() - ate) >= 0) {
        redibujar();
        ativo = false;
    }
    if (!ativo && f_qtd) ativar();
}
