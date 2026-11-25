// SPDX-License-Identifier: GPL-3.0-only
/* gui_progreso.c - ventana de progreso de transferencias y diálogo de opciones (D467).
 * Ver el comentario de gui.h. Todo corre en el programa que llama: no hay hilos, la ventana se atiende entre trozo y trozo. */
#include "sagui.h"
#include "gui.h"
#include "sonidos.h"

#define PL 460            /* ancho de la ventana */
#define PM 16             /* margen */
#define Y_PCT 50
#define Y_BARRA 74
#define Y_FILAS 102
#define PASO_FILA 19
#define Y_GRAF 202
#define ALT_GRAF 92
#define Y_BOTONES_BASE 204
#define PULSO_MS 100      /* cada cuánto se atienden los eventos y se repinta */

static uint32_t kb_de(uint64_t b) { return (uint32_t)(b >> 10); }

/* ------------------------------------------------------------ textos */

void transf_texto_tamano(char *s, size_t max, uint32_t kb, uint32_t unidad)
{
    if (unidad >= 1048576u) {
        uint32_t d = (uint32_t)((uint64_t)kb * 10u >> 20);
        formatar_texto(s, max, "%u,%u GB", d / 10, d % 10);
    } else if (unidad >= 1024u) {
        uint32_t d = kb < 400000000u ? kb * 10u / 1024u : (kb / 1024u) * 10u;
        formatar_texto(s, max, "%u,%u MB", d / 10, d % 10);
    } else formatar_texto(s, max, "%u KB", kb);
}

void transf_texto_velocidad(char *s, size_t max, uint32_t kbs)
{
    if (kbs == TC_SIN_DATOS) { copia_texto(s, _("Calculando..."), max); return; }
    if (kbs >= 1024u) {
        uint32_t d = kbs * 10u / 1024u;
        formatar_texto(s, max, "%u,%u MB/s", d / 10, d % 10);
    } else formatar_texto(s, max, "%u KB/s", kbs);
}

void transf_texto_restante(char *s, size_t max, uint32_t seg)
{
    if (seg == TC_SIN_DATOS) { copia_texto(s, _("Calculando..."), max); return; }
    uint32_t a, b;
    switch (tc_clasificar(seg, &a, &b)) {
    case TC_T_SEG: formatar_texto(s, max, a == 1 ? _("Unos %u segundo") : _("Unos %u segundos"), a); break;
    case TC_T_MIN: formatar_texto(s, max, a == 1 ? _("Unos %u minuto") : _("Unos %u minutos"), a); break;
    default: formatar_texto(s, max, _("%u h %u min"), a, b); break;
    }
}

/* ------------------------------------------------------------ dibujo de la ventana de progreso */

static int ancho_pasos(int cols) { return cols * LETRA_L; }

static void fila(transf_t *t, int y, const char *etiqueta, const char *valor)
{
    jan_t *j = t->j;
    g_retangulo(j, 0, y, PL, 18, mod_tema.superficie);
    g_texto(j, PM, y, etiqueta, mod_tema.texto_fraco);
    g_texto_n(j, PM + 136, y, valor, (PL - PM - 136 - PM) / LETRA_L, mod_tema.texto);
    jan_mostrar_area(j, 0, y, PL, 18);
}

static void botones_colocar(transf_t *t)
{
    int by = Y_BOTONES_BASE + (t->detalles ? ALT_GRAF + 8 : 0);
    t->b_det.x = PM; t->b_det.y = by; t->b_det.variante = MB_PLANO;
    t->b_det.texto = t->detalles ? N_("Menos detalles") : N_("Más detalles"); t->b_det.l = 0;
    t->b_det.fundo = mod_tema.superficie;
    t->b_cancel.variante = MB_SECUNDARIO; t->b_cancel.texto = N_("Cancelar"); t->b_cancel.l = 96; t->b_cancel.fundo = mod_tema.superficie;
    t->b_pausa.variante = MB_SECUNDARIO; t->b_pausa.texto = t->pausado ? N_("Reanudar") : N_("Pausar"); t->b_pausa.l = 96; t->b_pausa.fundo = mod_tema.superficie;
    t->b_cancel.x = PL - PM - 96; t->b_cancel.y = by;
    t->b_pausa.x = t->b_cancel.x - 8 - 96; t->b_pausa.y = by;
    mbotao_ajustar(&t->b_det); mbotao_ajustar(&t->b_cancel); mbotao_ajustar(&t->b_pausa);
}

static int alto_ventana(const transf_t *t) { return Y_BOTONES_BASE + (t->detalles ? ALT_GRAF + 8 : 0) + 28 + 14; }

static uint32_t velocidad_actual(const transf_t *t) { return tc_kbs(&t->vel); }

static void dibujar_grafico(transf_t *t)
{
    if (!t->detalles) return;
    jan_t *j = t->j;
    int x = PM, y = Y_GRAF + 4, w = PL - 2 * PM, h = ALT_GRAF - 22;
    uint32_t max = 1;
    for (int i = 0; i < t->n_graf; i++) if (t->graf[i] > max) max = t->graf[i];
    g_retangulo(j, 0, Y_GRAF, PL, ALT_GRAF, mod_tema.superficie);
    char a[48], b[64], v[48];
    uint32_t kbs = velocidad_actual(t);
    transf_texto_velocidad(v, sizeof v, kbs);
    formatar_texto(b, sizeof b, _("Velocidad: %s"), v);
    g_texto(j, x, Y_GRAF - 2, b, mod_tema.texto_fraco);
    transf_texto_velocidad(a, sizeof a, max);
    g_texto(j, PL - PM - g_largura_texto(a), Y_GRAF - 2, a, mod_tema.texto_fraco);
    y += 14; h -= 0;
    g_retangulo(j, x, y, w, h, mod_tema.campo);
    for (int c = 0; c < w - 2 && t->n_graf > 0; c++) {
        int k = c * t->n_graf / (w - 2);
        int alt = (int)((uint64_t)t->graf[k] * (uint32_t)(h - 4) / max);
        if (alt > 0) g_retangulo(j, x + 1 + c, y + h - 2 - alt, 1, alt, mod_tema.acento_suave);
        if (alt > 0) g_retangulo(j, x + 1 + c, y + h - 2 - alt, 1, 1, mod_tema.acento);
    }
    g_moldura(j, x, y, w, h, mod_tema.borde);
    jan_mostrar_area(j, 0, Y_GRAF, PL, ALT_GRAF);
}

static void dibujar_fijo(transf_t *t)
{
    jan_t *j = t->j;
    g_retangulo(j, 0, 0, PL, j->alt, mod_tema.superficie);
    int cols = (PL - 2 * PM) / LETRA_L;
    int n = (int)tam_texto(t->cabecera);
    g_texto_n(j, PM, 12, t->cabecera, cols, mod_tema.texto);
    if (n > cols) g_texto_n(j, PM, 30, t->cabecera + cols, cols, mod_tema.texto);
    (void)ancho_pasos;
}

static void dibujar_dinamico(transf_t *t, bool todo)
{
    jan_t *j = t->j;
    uint32_t kb = kb_de(t->bytes), kt = kb_de(t->total);
    uint32_t pct = tc_porcentaje(kb, kt);
    char s[160], u[64], w[64];
    if (t->preparando) copia_texto(s, _("Preparando..."), sizeof s);
    else formatar_texto(s, sizeof s, t->pausado ? _("%u%% completado (en pausa)") : _("%u%% completado"), pct);
    g_retangulo(j, 0, Y_PCT, PL, 20, mod_tema.superficie);
    g_texto(j, PM, Y_PCT, s, mod_tema.texto);
    jan_mostrar_area(j, 0, Y_PCT, PL, 20);
    if (t->preparando) progresso_mod_indet(j, PM, Y_BARRA, PL - 2 * PM, 16, t->fase++ * 4, mod_tema.superficie);
    else progresso_mod_desenhar(j, PM, Y_BARRA, PL - 2 * PM, 16, (int)pct, 0, mod_tema.superficie);
    fila(t, Y_FILAS, _("Nombre:"), t->actual);
    formatar_texto(s, sizeof s, _("%u de %u"), t->hechos < t->elementos ? t->hechos + 1 : t->hechos, t->elementos);
    fila(t, Y_FILAS + PASO_FILA, _("Elementos:"), t->preparando ? "..." : s);
    transf_texto_tamano(u, sizeof u, kb, kt);
    transf_texto_tamano(w, sizeof w, kt, kt);
    formatar_texto(s, sizeof s, _("%s de %s"), u, w);
    fila(t, Y_FILAS + 2 * PASO_FILA, _("Transferido:"), t->preparando ? "..." : s);
    uint32_t kbs = velocidad_actual(t);
    transf_texto_velocidad(s, sizeof s, t->pausado ? 0 : kbs);
    fila(t, Y_FILAS + 3 * PASO_FILA, _("Velocidad:"), t->pausado ? _("En pausa") : s);
    uint32_t seg = kt > kb ? tc_segundos(kt - kb, kbs) : 0;
    if (kt <= kb && !t->preparando) seg = 0;
    transf_texto_restante(s, sizeof s, t->pausado || t->preparando ? TC_SIN_DATOS : seg);
    fila(t, Y_FILAS + 4 * PASO_FILA, _("Tiempo restante:"), t->pausado ? "-" : s);
    if (todo || t->detalles) dibujar_grafico(t);
    mbotao_desenhar(j, &t->b_pausa);
    mbotao_desenhar(j, &t->b_cancel);
    mbotao_desenhar(j, &t->b_det);
    /* el título de la ventana también lleva el porcentaje (barra de tareas) */
    formatar_texto(s, sizeof s, "%s - %u%%", _(t->verbo), pct);
    titulo_janela(j->id, s);
    uint32_t ahora = tempo_ms();
    if (todo || ahora - t->ult_texto >= 500) {   /* al registro, a lo sumo dos veces por segundo */
        t->ult_texto = ahora;
        char vs[48], rs[48];
        transf_texto_velocidad(vs, sizeof vs, kbs);
        transf_texto_restante(rs, sizeof rs, t->pausado || t->preparando ? TC_SIN_DATOS : seg);
        registrar("[archivos] progreso %u%% %u/%u KB velocidad=%s restante=%s %s", pct, kb, kt, vs, rs, t->pausado ? "pausa" : "marcha");
    }
}

/* ------------------------------------------------------------ ciclo de vida */

void transf_iniciar(transf_t *t, const char *verbo, const char *cabecera, int (*fuera)(void *), void *dato)
{
    memset(t, 0, sizeof *t);
    copia_texto(t->verbo, verbo, sizeof t->verbo);
    copia_texto(t->cabecera, cabecera, sizeof t->cabecera);
    t->fuera = fuera;
    t->fuera_dato = dato;
    t->retardo_ms = 700;
    t->t0 = t->ult_pulso = tempo_ms();
    t->preparando = true;
    t->graf_periodo = 500;
    tc_reiniciar(&t->vel, t->t0, 0);
}

void transf_cabecera(transf_t *t, const char *texto)
{
    copia_texto(t->cabecera, texto, sizeof t->cabecera);
    if (t->j) { dibujar_fijo(t); dibujar_dinamico(t, true); jan_mostrar(t->j); }
}

static void abrir_ventana(transf_t *t)
{
    t->j = jan_abrir(_(t->verbo), PL, alto_ventana(t));
    if (!t->j) { t->retardo_ms = 0xFFFFFFFFu; return; }
    jan_estilo(t->j, JAN_DIALOGO, 0, 0);
    botones_colocar(t);
    dibujar_fijo(t);
    dibujar_dinamico(t, true);
    jan_mostrar(t->j);
    registrar("[archivos] progreso ventana abierta %dx%d pausa=%d,%d,%d,%d cancelar=%d,%d,%d,%d", t->j->larg, t->j->alt,
              t->b_pausa.x, t->b_pausa.y, t->b_pausa.l, t->b_pausa.a, t->b_cancel.x, t->b_cancel.y, t->b_cancel.l, t->b_cancel.a);
}

static void conmutar_detalles(transf_t *t)
{
    t->detalles = !t->detalles;
    uint32_t *px;
    int alt = alto_ventana(t);
    if (redimensionar_janela(t->j->id, PL, alt, &px) == 0) { t->j->px = px; t->j->larg = PL; t->j->alt = alt; }
    botones_colocar(t);
    dibujar_fijo(t);
    dibujar_dinamico(t, true);
    jan_mostrar(t->j);
}

static void alternar_pausa(transf_t *t)
{
    t->pausado = !t->pausado;
    if (!t->pausado) transf_reloj(t);
    botones_colocar(t);
    dibujar_dinamico(t, true);
}

/* Atiende los eventos de la ventana de progreso (clics, cruz, Esc). */
static void eventos(transf_t *t)
{
    evento_t e;
    while (t->j && proximo_evento(t->j->id, &e, false) == 1) {
        if (e.tipo == EV_FECHAR || (e.tipo == EV_TECLA && e.tecla == TECLA_ESC)) { t->cancelado = true; continue; }
        if (e.tipo == EV_TEMA) { botones_colocar(t); dibujar_fijo(t); dibujar_dinamico(t, true); jan_mostrar(t->j); continue; }
        if (mbotao_tratar(t->j, &t->b_cancel, &e)) { t->cancelado = true; registrar("[archivos] progreso cancelar"); continue; }
        if (mbotao_tratar(t->j, &t->b_pausa, &e)) { alternar_pausa(t); registrar("[archivos] progreso %s", t->pausado ? "pausar" : "reanudar"); continue; }
        if (mbotao_tratar(t->j, &t->b_det, &e)) { conmutar_detalles(t); continue; }
    }
}

static void muestras_graf(transf_t *t, uint32_t ahora)
{
    if (ahora - t->ult_graf < t->graf_periodo) return;
    t->ult_graf = ahora;
    uint32_t v = velocidad_actual(t);
    if (v == TC_SIN_DATOS) v = 0;
    if (t->n_graf == TG_MAX) {   /* lleno: se junta de dos en dos y cada muestra dura el doble */
        for (int i = 0; i < TG_MAX / 2; i++) t->graf[i] = (t->graf[2 * i] + t->graf[2 * i + 1]) / 2;
        t->n_graf = TG_MAX / 2;
        t->graf_periodo *= 2;
    }
    t->graf[t->n_graf++] = v;
}

int transf_pulso(transf_t *t)
{
    uint32_t ahora = tempo_ms();
    if (ahora - t->ult_pulso < PULSO_MS && !t->pausado) return t->cancelado;
    t->ult_pulso = ahora;
    if (!t->preparando) { tc_muestra(&t->vel, ahora, kb_de(t->bytes)); muestras_graf(t, ahora); }
    if (!t->j && ahora - t->t0 >= t->retardo_ms) abrir_ventana(t);
    if (t->fuera && t->fuera(t->fuera_dato)) t->cancelado = true;
    if (t->j) {
        eventos(t);
        if (!t->cancelado) dibujar_dinamico(t, false);
    }
    while (t->pausado && !t->cancelado) {   /* en pausa: se espera aquí, atendiendo la ventana */
        dormir(30);
        if (t->fuera && t->fuera(t->fuera_dato)) t->cancelado = true;
        eventos(t);
    }
    return t->cancelado;
}

int transf_preparando(transf_t *t) { return transf_pulso(t); }

void transf_total(transf_t *t, uint32_t elementos, uint64_t bytes)
{
    t->elementos = elementos;
    t->total = bytes;
    t->preparando = false;
    t->hechos = 0;
    t->bytes = 0;
    tc_reiniciar(&t->vel, tempo_ms(), 0);
    t->ult_graf = tempo_ms();
    registrar("[archivos] progreso total %u elementos %u KB", elementos, kb_de(bytes));
}

void transf_archivo(transf_t *t, const char *nombre)
{
    copia_texto(t->actual, nombre, sizeof t->actual);
}

void transf_listo_archivo(transf_t *t) { t->hechos++; }

int transf_avanzar(transf_t *t, uint32_t n)
{
    t->bytes += n;
    return transf_pulso(t);
}

void transf_saltar(transf_t *t, uint64_t bytes) { t->bytes += bytes; t->hechos++; }

void transf_reloj(transf_t *t)
{
    uint32_t ahora = tempo_ms();
    tc_reiniciar(&t->vel, ahora, kb_de(t->bytes));
    t->ult_pulso = ahora;
}

void transf_cerrar(transf_t *t)
{
    if (t->j) {
        t->bytes = t->total;   /* que se vea el 100 % un instante no hace falta: se cierra */
        jan_fechar(t->j);
        t->j = NULL;
        registrar("[archivos] progreso ventana cerrada");
    }
}

/* ------------------------------------------------------------ diálogo con varios botones */

#define DO_MAX_BOTONES 6

static int cortar_linea(const char *t, int cols, int *salto)
{
    int n = 0, ult = -1;
    *salto = 0;
    while (t[n] && t[n] != '\n' && n < cols) { if (t[n] == ' ') ult = n; n++; }
    if (t[n] == '\n') { *salto = 1; return n; }
    if (!t[n]) return n;
    if (ult > 0) { n = ult; *salto = 1; }
    return n;
}

int dialogo_opciones(const char *titulo, const char *texto, const char *const *botones, int n, const char *casilla, bool *marcada)
{
    if (n < 1 || n > DO_MAX_BOTONES) return -1;
    titulo = idioma_traducir_sl(titulo);
    texto = idioma_traducir_sl(texto);
    mbotao_t bs[DO_MAX_BOTONES];
    int total_b = 0;
    for (int i = 0; i < n; i++) {
        memset(&bs[i], 0, sizeof bs[i]);
        bs[i].variante = i == 0 ? MB_PRIMARIO : MB_SECUNDARIO;
        bs[i].texto = botones[i];
        bs[i].fundo = mod_tema.superficie;
        mbotao_ajustar(&bs[i]);
        if (bs[i].l < 84) bs[i].l = 84;
        total_b += bs[i].l + 8;
    }
    int larg = total_b + 2 * PM;
    if (larg < 400) larg = 400;
    if (larg > 640) larg = 640;
    int cols = (larg - 2 * PM) / LETRA_L;
    int lineas = 0;
    for (const char *p = texto; *p;) {
        int salto, c = cortar_linea(p, cols, &salto);
        lineas++;
        p += c + salto;
        if (!c && !salto) break;
    }
    if (!lineas) lineas = 1;
    int alt = PM + lineas * (LETRA_A + 2) + 12 + (casilla ? 26 : 0) + 28 + PM + 4;
    jan_t *j = jan_abrir(titulo, larg, alt);
    if (!j) return -1;
    jan_estilo(j, JAN_DIALOGO, 0, 0);
    g_retangulo(j, 0, 0, larg, alt, mod_tema.superficie);
    int y = PM;
    for (const char *p = texto; *p;) {
        int salto, c = cortar_linea(p, cols, &salto);
        g_texto_n(j, PM, y, p, c, mod_tema.texto);
        y += LETRA_A + 2;
        p += c + salto;
        if (!c && !salto) break;
    }
    y += 12;
    casilla_t cs = { PM, y + 1, marcada ? *marcada : false };
    if (casilla) {
        casilla_desenhar(j, &cs);
        g_texto(j, PM + 22, y, idioma_traducir_sl(casilla), mod_tema.texto);
        y += 26;
    }
    int x = larg - PM;
    for (int i = n - 1; i >= 0; i--) { x -= bs[i].l; bs[i].x = x; bs[i].y = alt - PM - 4 - 28; x -= 8; mbotao_desenhar(j, &bs[i]); }
    jan_mostrar(j);
    for (int i = 0; i < n; i++) registrar("[archivos] dialogo boton %d: %d,%d,%d,%d", i, bs[i].x, bs[i].y, bs[i].l, bs[i].a);
    sonido_evento("pregunta");
    int res = -2;
    evento_t e;
    while (res == -2) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR || (e.tipo == EV_TECLA && e.tecla == TECLA_ESC)) { res = -1; break; }
        if (e.tipo == EV_TEMA) continue;
        if (casilla && casilla_tratar(&cs, &e)) {
            casilla_desenhar(j, &cs);
            jan_mostrar_area(j, cs.x, cs.y, 14, 14);
            continue;
        }
        if (e.tipo == EV_TECLA && e.tecla == '\n') { res = 0; break; }
        for (int i = 0; i < n; i++)
            if (mbotao_tratar(j, &bs[i], &e)) { res = i; break; }
    }
    if (marcada) *marcada = cs.marcada;
    jan_fechar(j);
    return res;
}
