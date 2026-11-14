// SPDX-License-Identifier: GPL-2.0-or-later
/* pantalla - resolución de la pantalla (D058, D059).
 *
 *   pantalla                      lista los modos, la resolución actual y el write-combining
 *   pantalla 1280x1024            cambia ahora (placa del Bochs/QEMU) o, sin cambio en caliente,
 *                                 graba para el próximo arranque
 *   pantalla 1280x1024 --guardar  cambia ahora y graba para los próximos arranques
 *   pantalla --guardar            graba la resolución actual
 *   pantalla auto                 borra la preferencia (vuelve a 1024x768 automático)
 *   pantalla --ventana            la ventana "Pantalla", con la lista y los botones
 *
 * La preferencia vive en el encabezado de boot del disco de arranque, que el
 * cargador lee antes del kernel (D058). Arrancando desde el CD (D077) no hay dónde
 * grabar: el kernel rechaza con ERRO_SO_LEITURA y aquí se explica. */
#include "sagui.h"
#include "gui.h"

#define MAX_MODOS 192

static modo_video_t modos[MAX_MODOS];
static int qtd;
static info_tela_t tela;

static void ler_estado(void)
{
    qtd = modos_video(modos, MAX_MODOS);
    if (qtd < 0) qtd = 0;
    memset(&tela, 0, sizeof tela);
    info_sistema(INFO_TELA, &tela, sizeof tela);
}

static void texto_preferida(char *s, int max)
{
    if (tela.pref_largura) formatar_texto(s, (size_t)max, "%ux%u", tela.pref_largura, tela.pref_altura);
    else copia_texto(s, "automática (1024x768 si la hay)", (size_t)max);
}

static void texto_wc(char *s, int max)
{
    if (tela.wc) formatar_texto(s, (size_t)max, "activo (%s)", tela.wc_via);
    else formatar_texto(s, (size_t)max, "no: %s", tela.wc_motivo);
}

/* "1280x1024" -> ancho y alto. */
static bool ler_resolucao(const char *s, int *l, int *a)
{
    const char *x = acha_char(s, 'x');
    if (!x) x = acha_char(s, 'X');
    if (!x || x == s || !x[1]) return false;
    for (const char *p = s; *p; p++)
        if (p != x && (*p < '0' || *p > '9')) return false;
    *l = texto_para_int(s);
    *a = texto_para_int(x + 1);
    return *l > 0 && *a > 0 && *l < 65536 && *a < 65536;
}

static void listar_modos(void)
{
    char pref[48], wc[112];
    texto_preferida(pref, sizeof pref);
    texto_wc(wc, sizeof wc);
    imprimir("Pantalla actual:      %ux%u, %u bits", tela.largura, tela.altura, tela.bpp);
    if (tela.modo) imprimir(" (modo VBE 0x%x)", tela.modo);
    imprimir("\n");
    if (tela.a_quente) imprimir("Cambio en caliente:   sí (tarjeta Bochs/QEMU, hasta %ux%u)\n", tela.max_largura, tela.max_altura);
    else imprimir("Cambio en caliente:   no (el cambio vale para el próximo arranque)\n");
    imprimir("Preferida al arrancar: %s%s\n", pref, tela.arranque_cd ? " (arranque desde CD: no se puede guardar)"
                                                 : tela.pref_gravavel ? "" : " (no se encontró el disco de arranque)");
    imprimir("Write-combining:      %s\n", wc);
    imprimir("\n  Resolución   Bits  VBE     Notas\n");
    for (int i = 0; i < qtd; i++) {
        modo_video_t *m = &modos[i];
        char res[16], vbe[8], notas[64];
        formatar_texto(res, sizeof res, "%ux%u", m->largura, m->altura);
        if (m->modo) formatar_texto(vbe, sizeof vbe, "0x%x", m->modo); else copia_texto(vbe, "-", sizeof vbe);
        notas[0] = 0;
        if (m->flags & MODO_ATUAL) junta_texto(notas, "actual ", sizeof notas);
        if (m->flags & MODO_PREFERIDO) junta_texto(notas, "preferida ", sizeof notas);
        if (m->flags & MODO_A_QUENTE) junta_texto(notas, "en caliente ", sizeof notas);
        if (m->flags & MODO_VBE) junta_texto(notas, "arranque", sizeof notas);
        int n = (int)tam_texto(notas);
        while (n > 0 && notas[n - 1] == ' ') notas[--n] = 0;
        imprimir("%c %-12s %4u  %-6s  %s\n", (m->flags & MODO_ATUAL) ? '*' : ' ', res, m->bpp, vbe, notas);
    }
    registrar("pantalla: %ux%u, %d modos, a quente %u, preferida %ux%u, wc %u %s", tela.largura, tela.altura, qtd,
              tela.a_quente, tela.pref_largura, tela.pref_altura, tela.wc, tela.wc_via);
}

/* Aplica (y graba, si se pidió). Devuelve 0 o un error, con el mensaje ya impreso. */
static int aplicar(int l, int a, bool guardar, bool falar)
{
    int op;
    bool igual = l == (int)tela.largura && a == (int)tela.altura;
    if (!l) op = MUDAR_GUARDAR;
    else if (igual) op = guardar ? MUDAR_GUARDAR : 0;
    else if (tela.a_quente) op = MUDAR_AGORA | (guardar ? MUDAR_GUARDAR : 0);
    else op = MUDAR_GUARDAR;
    if (!op) {
        if (falar) imprimir("La pantalla ya está en %dx%d.\n", l, a);
        return 0;
    }
    int r = trocar_modo_video(l, a, op);
    registrar("pantalla: mudar %dx%d op %d -> %d", l, a, op, r);
    if (!falar) return r < 0 ? r : 0;
    if (r < 0) {
        bool na_lista = false;
        for (int i = 0; i < qtd; i++) na_lista |= modos[i].largura == l && modos[i].altura == a;
        if (r == ERRO_INVALIDO && na_lista && op == MUDAR_GUARDAR)
            imprimir("pantalla: no se guardó: la BIOS no ofrece %dx%d al arrancar (mira la columna VBE).\n", l, a);
        else if (r == ERRO_INVALIDO) imprimir("pantalla: %dx%d no está en la lista de modos (escribe pantalla para verlos)\n", l, a);
        else if (r == ERRO_SO_LEITURA)
            imprimir("pantalla: no se puede guardar: SaguiOS ha arrancado desde un CD, que es de solo lectura.\n"
                     "          Para guardar la resolución, arranca desde la imagen en un pendrive o disco.\n");
        else if (r == ERRO_NAO_EXISTE) imprimir("pantalla: no se encontró el disco de arranque de SaguiOS para guardar\n");
        else if (r == ERRO_FORMATO) imprimir("pantalla: la cabecera del disco no es la de este arranque; no se ha tocado\n");
        else imprimir("pantalla: %s\n", nome_erro(r));
        return r;
    }
    if (r & MUDAR_AGORA) imprimir("Pantalla cambiada a %dx%d.\n", l, a);
    if (r & MUDAR_GUARDAR) {
        if (l) imprimir("Guardado: el próximo arranque usará %dx%d.\n", l, a);
        else imprimir("Guardado: el próximo arranque elegirá la resolución automáticamente.\n");
    }
    if ((op & MUDAR_GUARDAR) && !(r & MUDAR_GUARDAR) && tela.arranque_cd)
        imprimir("pantalla: no se puede guardar: SaguiOS ha arrancado desde un CD, que es de solo lectura.\n");
    else if ((op & MUDAR_GUARDAR) && !(r & MUDAR_GUARDAR))
        imprimir("pantalla: no se guardó: la BIOS no ofrece %dx%d al arrancar (mira la columna VBE).\n", l, a);
    if (l && !igual && !(op & MUDAR_AGORA) && (r & MUDAR_GUARDAR))
        imprimir("Esta tarjeta no cambia de modo en caliente: reinicia para usar %dx%d.\n", l, a);
    return 0;
}

/* ------------------------------------------------------------ ventana */

#define JL 440
#define JA 420

#define MOTIVO_COLUNAS ((JL - 40) / 8)

static char itens[MAX_MODOS][64];
static int indice_item[MAX_MODOS];   /* elemento de la lista -> modos[] */
static lista_t lista;

static void preencher_lista(void)
{
    int n = 0;
    lista.sel = -1;
    for (int i = 0; i < qtd; i++) {
        modo_video_t *m = &modos[i];
        if (m->bpp != 32) continue;   /* la ventana solo muestra 32 bits: los de 24 son los mismos tamaños */
        formatar_texto(itens[n], sizeof itens[n], "%ux%u%s%s%s", m->largura, m->altura,
                       (m->flags & MODO_ATUAL) ? "  (actual)" : "", (m->flags & MODO_PREFERIDO) ? "  (preferida)" : "",
                       (m->flags & MODO_A_QUENTE) ? "" : "  (al reiniciar)");
        if (m->flags & MODO_ATUAL) lista.sel = n;
        indice_item[n++] = i;
    }
    lista.qtd = n;
}

static void desenhar(jan_t *j, botao_t *b, int nb, const char *aviso)
{
    char t[112], pref[48];
    g_retangulo(j, 0, 0, j->larg, j->alt, COR_FUNDO);
    formatar_texto(t, sizeof t, "Actual: %ux%u, %u bits", tela.largura, tela.altura, tela.bpp);
    g_texto(j, 12, 10, t, COR_TEXTO);
    texto_preferida(pref, sizeof pref);
    formatar_texto(t, sizeof t, "Al arrancar: %s", pref);
    g_texto(j, 12, 28, t, COR_TEXTO);
    /* El motivo de no haber WC puede tener casi 80 letras: va abajo, en
     * hasta dos líneas cortadas en un espacio (la ventana tiene 52 de ancho). */
    formatar_texto(t, sizeof t, "Write-combining: %s", tela.wc ? "" : "no");
    if (tela.wc) texto_wc(t + tam_texto(t), (int)(sizeof t - tam_texto(t)));
    g_texto(j, 12, 46, t, COR_TEXTO_FRACO);
    const char *p = tela.wc ? "" : tela.wc_motivo;
    for (int y = 64; *p && y <= 82; y += 18) {
        int n = (int)tam_texto(p);
        if (n > MOTIVO_COLUNAS) {
            n = MOTIVO_COLUNAS;
            while (n > 0 && p[n] != ' ') n--;
            if (n == 0) n = MOTIVO_COLUNAS;
        }
        g_texto_n(j, 28, y, p, n, COR_TEXTO_FRACO);
        p += n;
        while (*p == ' ') p++;
    }
    lista_desenhar(j, &lista);
    for (int i = 0; i < nb; i++) botao_desenhar(j, &b[i]);
    g_texto(j, 12, JA - 22, aviso, COR_DESTAQUE);
    jan_mostrar(j);
}

static int janela(void)
{
    jan_t *j = jan_abrir("Pantalla", JL, JA);
    if (!j) { imprimir("pantalla: no se pudo abrir la ventana (¿hay escritorio?)\n"); return 1; }
    botao_t b[3] = {
        { 12, JA - 58, 120, 26, "Aplicar", false },
        { 142, JA - 58, 150, 26, "Aplicar y guardar", false },
        { JL - 102, JA - 58, 90, 26, "Cerrar", false },
    };
    char aviso[80] = "";
    ler_estado();
    /* Sin WC, el motivo ocupa una o dos líneas arriba de la lista. */
    int linhas = tela.wc ? 0 : (int)tam_texto(tela.wc_motivo) > MOTIVO_COLUNAS ? 2 : 1;
    int ly = 70 + linhas * 18;
    lista = (lista_t){ 12, ly, JL - 24, JA - ly - 70, itens, 0, -1, 0, NULL };
    preencher_lista();
    desenhar(j, b, 3, aviso);
    registrar("pantalla: janela aberta, %d modos de 32 bits", lista.qtd);
    evento_t e;
    for (;;) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_TELA) {   /* otro programa cambió la resolución */
            ler_estado();
            preencher_lista();
            desenhar(j, b, 3, aviso);
            continue;
        }
        int ativou = lista_tratar(j, &lista, &e);
        bool ap = botao_tratar(j, &b[0], &e) || ativou == LISTA_ATIVOU;
        bool gu = botao_tratar(j, &b[1], &e);
        if (botao_tratar(j, &b[2], &e)) break;
        if ((ap || gu) && lista.sel >= 0) {
            modo_video_t *m = &modos[indice_item[lista.sel]];
            int r = aplicar(m->largura, m->altura, gu, false);
            if (r == ERRO_SO_LEITURA || (r >= 0 && gu && tela.arranque_cd))
                copia_texto(aviso, "Arranque desde CD: no se puede guardar", sizeof aviso);
            else if (r < 0) formatar_texto(aviso, sizeof aviso, "No se pudo: %s", nome_erro(r));
            else if (tela.a_quente) formatar_texto(aviso, sizeof aviso, "%ux%u%s", m->largura, m->altura, gu ? ", guardada" : "");
            else formatar_texto(aviso, sizeof aviso, "%ux%u al reiniciar", m->largura, m->altura);
            ler_estado();
            preencher_lista();
            desenhar(j, b, 3, aviso);
        } else if (ativou) {
            lista_desenhar(j, &lista);
            jan_mostrar(j);
        }
    }
    jan_fechar(j);
    return 0;
}

/* ------------------------------------------------------------ principal */

int principal(int argc, char **argv)
{
    bool guardar = false, ventana = false, autom = false, tem_res = false;
    int l = 0, a = 0;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--guardar") == 0 || compara_texto(argv[i], "-g") == 0) guardar = true;
        else if (compara_texto(argv[i], "--ventana") == 0 || compara_texto(argv[i], "-v") == 0) ventana = true;
        else if (compara_texto(argv[i], "auto") == 0) autom = true;
        else if (ler_resolucao(argv[i], &l, &a)) tem_res = true;
        else {
            imprimir("uso: pantalla [ANCHOxALTO] [--guardar] | pantalla auto | pantalla --ventana\n");
            return 2;
        }
    }
    ler_estado();
    if (ventana) return janela();
    if (!tela.largura && !tela.a_quente && qtd == 0) {
        imprimir("pantalla: sin modo gráfico (VESA no encontrado)\n");
        return 1;
    }
    if (autom) return aplicar(0, 0, true, true) < 0 ? 1 : 0;
    if (tem_res) return aplicar(l, a, guardar, true) < 0 ? 1 : 0;
    if (guardar) return aplicar((int)tela.largura, (int)tela.altura, true, true) < 0 ? 1 : 0;
    listar_modos();
    return 0;
}
