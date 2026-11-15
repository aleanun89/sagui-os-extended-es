// SPDX-License-Identifier: GPL-2.0-or-later
/* chatia - el chat con IA con ventana (D143, D239).
 *
 *   chatia                    abre la ventana (usa la configuración de la IA, D145)
 *   chatia --url URL          elige la dirección base (http://host:puerto/v1)
 *   chatia --modelo MODELO    elige el modelo
 *   chatia --ayuda            muestra la ayuda
 *
 * La ventana tiene tres partes:
 *   - a la izquierda, la lista de conversaciones guardadas (la más nueva arriba),
 *     con «Nueva conversación», «Renombrar» y «Borrar»;
 *   - arriba a la derecha, la conversación, con las respuestas en Markdown
 *     (negrita, cursiva, títulos, listas, código y bloques de código con su
 *     botón «Copiar»), que se ven formateadas mientras llegan en streaming;
 *   - abajo, la caja de la pregunta con «Enviar», «Detener», «Adjuntar» (un
 *     fichero de texto que va como bloque en el mensaje) y «Regenerar» (vuelve
 *     a pedir la última respuesta).
 * «Preferencias» cambia, dentro de la misma ventana, el servidor, la clave y el
 * modelo del chat (la configuración compartida de D145, que también leen el
 * terminal y el Mico) y el mensaje de sistema de la conversación abierta.
 *
 * Cada conversación es un fichero de texto legible en
 * /home/.config/ia/conversas/conversa-NNNN.txt (o en /tmp/ia/conversas si /home
 * no es persistente, y entonces se avisa): el título, el mensaje de sistema y
 * los mensajes, cada uno detrás de una línea "--- papel".
 *
 * El contexto es la conversación: cada pregunta va con los últimos mensajes
 * (hasta HIST_MAX y CONTEXTO_MAX bytes). Todo está en castellano; la respuesta
 * llega en UTF-8 y se pasa a Latin-1, que es lo que dibuja la pantalla. */
#include "agentes.h"
#include "ia.h"
#include "gui.h"

#define LATERAL     200      /* ancho de la columna de conversaciones */
#define TOPO        56       /* dos filas: modelo y estado */
#define BASE        96       /* la fila de botones y la caja de la pregunta */
#define MARGEM      12
#define HIST_MAX    12       /* mensajes de contexto en cada petición */
#define CONTEXTO_MAX (48 * 1024)   /* bytes de texto de contexto */
#define ADJUNTO_MAX (16 * 1024)    /* tamaño máximo de un fichero adjunto */
#define CONV_MAX    256      /* conversaciones en la lista */
#define MODELOS_MAX 32

#define SISTEMA_PADRAO "Eres un asistente de SaguiOS-Extended. Responde siempre en castellano, " \
                       "de forma clara y concisa. Si no sabes algo, dilo con honestidad."

/* Los menús contextuales (el código que vuelve en EV_MENU). */
#define MENU_LISTA  1
#define MENU_VISOR  2
#define MENU_PROV   3

/* ------------------------------------------------------------- estado */

typedef struct {
    char  papel[12];         /* "user", "assistant" o "nota" (solo en pantalla) */
    char *texto;             /* Latin-1, terminado en cero */
    int   n;
} msg_t;

static jan_t *j;
static ia_config_t cfg;      /* la configuración de la sesión (con la línea de órdenes) */
static rico_t *visor;        /* la conversación, formateada */
static area_t entrada;       /* la pregunta */
static botao_t bt_enviar, bt_deter, bt_adjuntar, bt_quitar, bt_regenerar, bt_prefs;
static botao_t bt_nova, bt_renomear, bt_apagar, bt_modo;
static ag_panel_t *panel;      /* D295: el panel de agentes (modo agente) */
static bool modo_agente;
static char estado[200];
static bool estado_erro;
static bool ocupada;         /* la respuesta está llegando */
static bool parar;
static bool fechar_pendente;

/* La conversación abierta. */
static msg_t *msgs;
static int n_msgs, cap_msgs;
static char conv_arquivo[32];            /* "conversa-0001.txt"; vacío = aún sin guardar */
static char conv_titulo[64];
static char conv_sistema[IA_SISTEMA_MAX];

/* La lista lateral. */
static char pasta_conv[64];
static bool pasta_temporal;
static char itens[CONV_MAX][64];
static char arquivos[CONV_MAX][32];
static int n_conv;
static lista_t lista;

/* El fichero adjunto que va con la próxima pregunta. */
static char *adjunto;
static int adjunto_n;
static char adjunto_nome[64];

/* Las preferencias (un panel en el sitio de la conversación). */
static bool prefs_abertas;
static caixa_t cx_url, cx_chave, cx_modelo;
static area_t ar_sistema;
static lista_t lista_modelos;
static char modelos[MODELOS_MAX][64];
static botao_t bt_prov, bt_cargar, bt_padrao, bt_guardar, bt_cancelar;
static ia_provedor_t provedores[16];
static int n_provedores;

/* Quién recibe el teclado. */
enum { FOCO_ENTRADA, FOCO_URL, FOCO_CHAVE, FOCO_MODELO, FOCO_SISTEMA };
static int foco = FOCO_ENTRADA;

static int menu_secao = -1;   /* la sección del visor del último clic derecho */

/* ------------------------------------------------------------- mensajes */

static int msg_add(const char *papel, const char *t, int n)
{
    if (n_msgs >= cap_msgs) {
        int nova = cap_msgs ? cap_msgs * 2 : 16;
        msg_t *m = realoca(msgs, (size_t)nova * sizeof(msg_t));
        if (!m) return -1;
        msgs = m;
        cap_msgs = nova;
    }
    msg_t *m = &msgs[n_msgs];
    memset(m, 0, sizeof *m);
    copia_texto(m->papel, papel, sizeof m->papel);
    m->texto = aloca((size_t)n + 1);
    if (!m->texto) return -1;
    memcpy(m->texto, t, (size_t)n);
    m->texto[n] = 0;
    m->n = n;
    return n_msgs++;
}

static void msg_juntar(int i, const char *t, int n)
{
    msg_t *m = &msgs[i];
    char *novo = realoca(m->texto, (size_t)(m->n + n + 1));
    if (!novo) return;
    m->texto = novo;
    memcpy(m->texto + m->n, t, (size_t)n);
    m->n += n;
    m->texto[m->n] = 0;
}

static void msg_remover_ultima(void)
{
    if (n_msgs == 0) return;
    libera(msgs[--n_msgs].texto);
}

static void msgs_limpar(void)
{
    while (n_msgs) msg_remover_ultima();
}

static bool eh(const msg_t *m, const char *papel) { return compara_texto(m->papel, papel) == 0; }

/* La sección del visor de un mensaje (una por mensaje, en el mismo orden). */
static void secao_de(const msg_t *m)
{
    if (eh(m, "user")) rico_secao(visor, "Tú", COR_SELECAO, true);
    else if (eh(m, "assistant")) rico_secao(visor, "Asistente", COR_DESTAQUE, true);
    else rico_secao(visor, "Sistema", COR_TEXTO_FRACO, false);
    rico_juntar(visor, m->texto, m->n);
}

static void refazer_visor(void)
{
    rico_limpar(visor);
    for (int i = 0; i < n_msgs; i++) secao_de(&msgs[i]);
    rico_ir_fim(visor);
}

/* Un mensaje nuevo, en la conversación y en la pantalla. */
static int mostrar(const char *papel, const char *t, int n)
{
    int i = msg_add(papel, t, n);
    if (i >= 0) secao_de(&msgs[i]);
    rico_ir_fim(visor);
    return i;
}

static void nota(const char *t) { mostrar("nota", t, (int)tam_texto(t)); }

/* Mensajes de verdad (sin las notas de la pantalla). */
static int mensagens_reais(void)
{
    int k = 0;
    for (int i = 0; i < n_msgs; i++) if (!eh(&msgs[i], "nota")) k++;
    return k;
}

/* ------------------------------------------------------------- UTF-8 */

/* Latin-1 -> UTF-8 para los ficheros (devuelve un búfer nuevo). */
static char *para_utf8(const char *s, int n, int *saida)
{
    char *d = aloca((size_t)n * 2 + 1);
    if (!d) return NULL;
    int k = 0;
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) d[k++] = (char)c;
        else { d[k++] = (char)(0xC0 | (c >> 6)); d[k++] = (char)(0x80 | (c & 0x3F)); }
    }
    d[k] = 0;
    *saida = k;
    return d;
}

/* UTF-8 -> Latin-1 que sabe de los caracteres tipográficos que los modelos
 * usan mucho (comillas curvas, rayas, puntos suspensivos, viñetas) y que no
 * existen en Latin-1. Lo que no se conoce pasa a '?'. "pend" guarda la
 * secuencia que un pedazo de la respuesta dejó a medias. */
static char pend[4];
static int n_pend;

static unsigned byte_em(const char *t, int q)
{
    return q < n_pend ? (unsigned char)pend[q] : (unsigned char)t[q - n_pend];
}

static int utf8_latin1(const char *t, int n, char *d, int max)
{
    int k = 0, total = n_pend + n;
    char resto[4];
    int nr = 0;
    for (int i = 0; i < total && k < max - 4; ) {
        unsigned c = byte_em(t, i);
        int largo = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
        if (i + largo > total) {
            /* La secuencia sigue en el próximo pedazo. */
            for (int q = i; q < total && nr < 4; q++) resto[nr++] = (char)byte_em(t, q);
            break;
        }
        unsigned cp = c;
        if (largo == 2) cp = ((c & 0x1Fu) << 6) | (byte_em(t, i + 1) & 0x3Fu);
        else if (largo == 3) cp = ((c & 0x0Fu) << 12) | ((byte_em(t, i + 1) & 0x3Fu) << 6) | (byte_em(t, i + 2) & 0x3Fu);
        else if (largo == 4) cp = 0x10000;
        i += largo;
        if (cp < 0x100) { d[k++] = (char)cp; continue; }
        const char *sub = "?";
        switch (cp) {
        case 0x2018: case 0x2019: case 0x201A: case 0x2032: sub = "'"; break;
        case 0x201C: case 0x201D: case 0x201E: case 0x2033: sub = "\""; break;
        case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2015: case 0x2212: sub = "-"; break;
        case 0x2026: sub = "..."; break;
        case 0x2022: case 0x25CF: case 0x2023: sub = "·"; break;
        case 0x2192: sub = "->"; break;
        case 0x2190: sub = "<-"; break;
        case 0x2264: sub = "<="; break;
        case 0x2265: sub = ">="; break;
        case 0x2260: sub = "!="; break;
        case 0x2009: case 0x200A: case 0x202F: sub = " "; break;
        case 0x200B: case 0xFEFF: sub = ""; break;
        case 0x2713: case 0x2714: case 0x2705: sub = "[x]"; break;
        case 0x20AC: sub = "EUR"; break;
        }
        for (const char *s = sub; *s && k < max - 1; s++) d[k++] = *s;
    }
    n_pend = nr;
    memcpy(pend, resto, (size_t)nr);
    d[k] = 0;
    return k;
}

/* ------------------------------------------------------------- ficheros */

static void caminho_conv(char *dst, int max, const char *nome)
{
    formatar_texto(dst, (size_t)max, "%s/%s", pasta_conv, nome);
}

static int numero_conv(const char *nome)
{
    if (!comeca_com(nome, "conversa-")) return -1;
    const char *p = nome + 9;
    int v = 0, d = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; d++; }
    return d && compara_texto(p, ".txt") == 0 ? v : -1;
}

/* Escribe la conversación abierta: primero a un fichero temporal y después
 * lo pone en su sitio, así un corte a mitad no deja la conversación rota. */
static bool guardar_conversa(void)
{
    if (!conv_arquivo[0] || mensagens_reais() == 0) return false;
    int cap = 256 + (int)tam_texto(conv_titulo) + 2 * (int)tam_texto(conv_sistema);
    for (int i = 0; i < n_msgs; i++) cap += msgs[i].n * 2 + 32;
    char *t = aloca((size_t)cap);
    if (!t) return false;
    int n = formatar_texto(t, (size_t)cap, "# Conversación del chat con IA de SaguiOS (D239)\ntitulo: %s\n", conv_titulo);
    /* Una línea del contenido que empieza por "--- " o por "\" lleva una "\"
     * delante, para no confundirla con el comienzo de otro mensaje. */
    for (int i = -1; i < n_msgs; i++) {
        const char *papel = i < 0 ? "system" : msgs[i].papel;
        const char *s = i < 0 ? conv_sistema : msgs[i].texto;
        if (i >= 0 && eh(&msgs[i], "nota")) continue;
        n += formatar_texto(t + n, (size_t)(cap - n), "--- %s\n", papel);
        const char *p = s;
        while (*p) {
            const char *f = acha_char(p, '\n');
            int ln = f ? (int)(f - p) : (int)tam_texto(p);
            if (comeca_com(p, "--- ") || *p == '\\') t[n++] = '\\';
            memcpy(t + n, p, (size_t)ln);
            n += ln;
            t[n++] = '\n';
            p += ln;
            if (*p == '\n') p++;
        }
    }
    int nu;
    char *u = para_utf8(t, n, &nu);
    libera(t);
    if (!u) return false;
    char dest[128], tmp[136];
    caminho_conv(dest, sizeof dest, conv_arquivo);
    formatar_texto(tmp, sizeof tmp, "%s.tmp", dest);
    int fd = abrir(tmp, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    bool ok = fd >= 0 && escrever_tudo(fd, u, (unsigned)nu) >= 0;
    if (fd >= 0) { sincronizar(fd); fechar(fd); }
    libera(u);
    if (!ok) { apagar(tmp); return false; }
    apagar(dest);
    if (renomear(tmp, dest) < 0) return false;
    registrar("chatia: conversacion guardada en %s (%d mensajes)\n", dest, mensagens_reais());
    return true;
}

/* Lee un fichero de conversación (ya en Latin-1). NULL si no existe. */
static char *ler_conv(const char *nome, unsigned *tam)
{
    char c[128];
    caminho_conv(c, sizeof c, nome);
    int fd = abrir(c, ABRIR_LER);
    if (fd < 0) return NULL;
    char *t = ler_todo_fd(fd, tam);
    fechar(fd);
    if (!t) return NULL;
    *tam = (unsigned)utf8_para_latin1(t, *tam, NULL);
    t[*tam] = 0;
    return t;
}

/* El título de un fichero (la línea "titulo: "). */
static void titulo_de(const char *nome, char *dst, int max)
{
    unsigned n;
    char *t = ler_conv(nome, &n);
    copia_texto(dst, nome, (size_t)max);
    if (!t) return;
    char *p = t;
    while (p && *p) {
        char *f = acha_char(p, '\n');
        if (f) *f = 0;
        if (comeca_com(p, "titulo: ")) { copia_texto(dst, p + 8, (size_t)max); break; }
        if (comeca_com(p, "--- ")) break;
        p = f ? f + 1 : NULL;
    }
    libera(t);
}

/* Las conversaciones de la carpeta, la más nueva arriba. */
static void carregar_lista(void)
{
    entrada_pasta_t *v = NULL;
    int n = listar_tudo(pasta_conv, &v);
    n_conv = 0;
    /* Un .tmp sin su fichero es un guardado que se cortó a mitad: se recupera. */
    for (int i = 0; i < n; i++) {
        int k = (int)tam_texto(v[i].nome);
        if (k > 4 && compara_texto(v[i].nome + k - 4, ".tmp") == 0) {
            char de[128], para[128];
            caminho_conv(de, sizeof de, v[i].nome);
            copia_texto(para, de, sizeof para);
            para[tam_texto(para) - 4] = 0;
            info_arquivo_t inf;
            if (info_arquivo(para, &inf) < 0) renomear(de, para);
            else apagar(de);
        }
    }
    if (v) libera(v);
    v = NULL;
    n = listar_tudo(pasta_conv, &v);
    /* Por número, de la más nueva a la más antigua. */
    static int usados[CONV_MAX];
    int nu = 0;
    for (int i = 0; i < n && nu < CONV_MAX; i++) if (numero_conv(v[i].nome) >= 0) usados[nu++] = i;
    for (int a = 0; a < nu; a++)
        for (int b = a + 1; b < nu; b++)
            if (numero_conv(v[usados[b]].nome) > numero_conv(v[usados[a]].nome)) {
                int t = usados[a]; usados[a] = usados[b]; usados[b] = t;
            }
    for (int i = 0; i < nu; i++) {
        copia_texto(arquivos[n_conv], v[usados[i]].nome, sizeof arquivos[0]);
        titulo_de(arquivos[n_conv], itens[n_conv], sizeof itens[0]);
        n_conv++;
    }
    if (v) libera(v);
    lista.qtd = n_conv;
    registrar("chatia: %d conversaciones en %s\n", n_conv, pasta_conv);
}

static int indice_atual(void)
{
    if (!conv_arquivo[0]) return -1;
    for (int i = 0; i < n_conv; i++) if (compara_texto(arquivos[i], conv_arquivo) == 0) return i;
    return -1;
}

/* El título automático: las primeras palabras de la pregunta. */
static void titulo_automatico(const char *pergunta)
{
    char t[64];
    int k = 0;
    bool espaco = false, cortado = false;
    for (const char *p = pergunta; *p; p++) {
        if (k >= 40) { cortado = true; break; }
        char c = *p;
        if (c == '\n' || c == '\t' || c == '\r') c = ' ';
        if (c == ' ') { if (k && !espaco) { t[k++] = ' '; espaco = true; } continue; }
        t[k++] = c;
        espaco = false;
    }
    while (k && t[k - 1] == ' ') k--;
    if (cortado) {
        /* Corta en la última palabra entera. */
        int q = k;
        while (q > 20 && t[q - 1] != ' ') q--;
        if (q > 20) k = q - 1;
        t[k++] = '.'; t[k++] = '.'; t[k++] = '.';
    }
    t[k] = 0;
    if (!k) {
        if (adjunto_nome[0]) formatar_texto(t, sizeof t, "Adjunto: %s", adjunto_nome);
        else copia_texto(t, "Conversación", sizeof t);
    }
    copia_texto(conv_titulo, t, sizeof conv_titulo);
}

/* La conversación abierta pasa a tener fichero y sube a lo alto de la lista. */
static void registrar_conversa_nova(void)
{
    int maior = 0;
    for (int i = 0; i < n_conv; i++) { int v = numero_conv(arquivos[i]); if (v > maior) maior = v; }
    formatar_texto(conv_arquivo, sizeof conv_arquivo, "conversa-%04d.txt", maior + 1);
    if (n_conv >= CONV_MAX) n_conv = CONV_MAX - 1;   /* la más antigua deja de verse (no se borra) */
    for (int i = n_conv; i > 0; i--) {
        memcpy(itens[i], itens[i - 1], sizeof itens[0]);
        memcpy(arquivos[i], arquivos[i - 1], sizeof arquivos[0]);
    }
    copia_texto(itens[0], conv_titulo, sizeof itens[0]);
    copia_texto(arquivos[0], conv_arquivo, sizeof arquivos[0]);
    n_conv++;
    lista.qtd = n_conv;
    lista.sel = 0;
    lista.topo = 0;
    registrar("chatia: conversacion nueva %s: %s\n", conv_arquivo, conv_titulo);
}

/* ------------------------------------------------------------- dibujo */

static void texto_limitado(int x, int y, const char *texto, int ancho, uint32_t cor)
{
    int n = (int)tam_texto(texto), max = ancho / LETRA_L;
    if (max <= 0) return;
    if (n <= max) g_texto_n(j, x, y, texto, n, cor);
    else if (max > 3) {
        g_texto_n(j, x, y, texto, max - 3, cor);
        g_texto(j, x + (max - 3) * LETRA_L, y, "...", cor);
    }
}

static void desenhar_botao(botao_t *b, bool ativo)
{
    if (ativo) botao_desenhar(j, b);
    else {
        g_retangulo(j, b->x, b->y, b->l, b->a, COR_FUNDO);
        g_moldura(j, b->x, b->y, b->l, b->a, COR_BORDA);
        g_texto(j, b->x + (b->l - g_largura_texto(b->texto)) / 2,
                b->y + (b->a - LETRA_A) / 2, b->texto, COR_TEXTO_FRACO);
    }
}

static void pos_botao(botao_t *b, int x, int y, int l, int a, const char *t)
{
    b->x = x; b->y = y; b->l = l; b->a = a; b->texto = t;
}

static int x0(void) { return LATERAL + MARGEM; }
static int largura_principal(void) { return j->larg - x0() - MARGEM; }
static int altura_visor(void) { return j->alt - TOPO - BASE - 6; }

static bool ultima_e(const char *papel)
{
    for (int i = n_msgs - 1; i >= 0; i--) {
        if (eh(&msgs[i], "nota")) continue;
        return eh(&msgs[i], papel);
    }
    return false;
}

static bool pode_regenerar(void) { return !ocupada && (ultima_e("assistant") || ultima_e("user")); }

/* Las posiciones de todo (dependen del tamaño de la ventana). */
static void maquetar(void)
{
    int larg = j->larg, alt = j->alt, x = x0(), w = largura_principal();
    pos_botao(&bt_nova, 8, 8, LATERAL - 16, 26, "Nueva conversación");
    lista.x = 8; lista.y = 40; lista.l = LATERAL - 16; lista.a = alt - 40 - 44;
    pos_botao(&bt_renomear, 8, alt - 36, 100, 28, "Renombrar");
    pos_botao(&bt_apagar, 112, alt - 36, LATERAL - 120, 28, "Borrar");
    pos_botao(&bt_prefs, larg - MARGEM - 120, 4, 120, 24, "Preferencias");
    pos_botao(&bt_modo, larg - MARGEM - 250, 4, 124, 24, modo_agente ? "Chat normal" : "Agentes");
    if (panel) ag_panel_mudar_tamanho(panel, x, TOPO, w, alt - TOPO - 6);
    rico_mover(visor, x, TOPO, w, altura_visor());
    pos_botao(&bt_adjuntar, x, alt - BASE + 2, 96, 24, "Adjuntar");
    pos_botao(&bt_regenerar, x + 102, alt - BASE + 2, 100, 24, "Regenerar");
    pos_botao(&bt_quitar, larg - MARGEM - 70, alt - BASE + 2, 70, 24, "Quitar");
    entrada.x = x;
    entrada.y = alt - BASE + 32;
    area_mudar_tamanho(&entrada, w - 230, BASE - 44);
    pos_botao(&bt_enviar, larg - MARGEM - 220, alt - BASE + 34, 100, 28, "Enviar");
    pos_botao(&bt_deter, larg - MARGEM - 110, alt - BASE + 34, 110, 28, "Detener");

    /* El panel de preferencias ocupa el sitio de la conversación. */
    int px = x + 12, py = TOPO + 8, cw = w - 24 - 150 - 136;
    if (cw < 120) cw = 120;
    cx_url.x = px + 150; cx_url.y = py + 26; cx_url.l = cw;
    cx_chave.x = px + 150; cx_chave.y = py + 56; cx_chave.l = cw;
    cx_modelo.x = px + 150; cx_modelo.y = py + 86; cx_modelo.l = cw;
    pos_botao(&bt_prov, px + 150 + cw + 8, py + 26, 128, 24, "Proveedor...");
    pos_botao(&bt_cargar, px + 150 + cw + 8, py + 86, 128, 24, "Cargar modelos");
    lista_modelos.x = px + 150; lista_modelos.y = py + 116; lista_modelos.l = cw;
    lista_modelos.a = 4 * LISTA_ALTURA_ITEM + 4;
    pos_botao(&bt_padrao, px + w - 24 - 190, py + 204, 190, 22, "Castellano (omisión)");
    ar_sistema.x = px;
    ar_sistema.y = py + 230;
    area_mudar_tamanho(&ar_sistema, w - 24, 72);
    pos_botao(&bt_guardar, px, py + 310, 100, 28, "Guardar");
    pos_botao(&bt_cancelar, px + 108, py + 310, 100, 28, "Cancelar");
}

static void desenhar_prefs(void)
{
    int x = x0(), w = largura_principal(), h = altura_visor();
    int px = x + 12, py = TOPO + 8;
    g_retangulo(j, x, TOPO, w, h, COR_BRANCO);
    g_moldura(j, x, TOPO, w, h, COR_BORDA);
    g_texto(j, px, py, "Preferencias del chat", COR_TEXTO);
    g_texto(j, px + 1, py, "Preferencias del chat", COR_TEXTO);
    g_texto(j, px, py + 30, "Servidor (URL):", COR_TEXTO);
    g_texto(j, px, py + 60, "Clave de API:", COR_TEXTO);
    g_texto(j, px, py + 90, "Modelo del chat:", COR_TEXTO);
    cx_url.foco = foco == FOCO_URL;
    cx_chave.foco = foco == FOCO_CHAVE;
    cx_modelo.foco = foco == FOCO_MODELO;
    ar_sistema.foco = foco == FOCO_SISTEMA;
    caixa_desenhar(j, &cx_url);
    caixa_desenhar(j, &cx_chave);
    caixa_desenhar(j, &cx_modelo);
    botao_desenhar(j, &bt_prov);
    botao_desenhar(j, &bt_cargar);
    texto_limitado(px, py + 118, cfg.chave[0] ? "Con clave;" : "Sin clave;", 148, COR_TEXTO_FRACO);
    texto_limitado(px, py + 136, "vacía = no cambia", 148, COR_TEXTO_FRACO);
    if (lista_modelos.qtd) lista_desenhar(j, &lista_modelos);
    else {
        g_moldura(j, lista_modelos.x, lista_modelos.y, lista_modelos.l, lista_modelos.a, COR_BORDA);
        texto_limitado(lista_modelos.x + 6, lista_modelos.y + 6, "«Cargar modelos» pide la lista al servidor",
                       lista_modelos.l - 12, COR_TEXTO_FRACO);
    }
    texto_limitado(px, py + 208, "Mensaje de sistema de esta conversación:", w - 24 - 200, COR_TEXTO);
    botao_desenhar(j, &bt_padrao);
    area_desenhar(j, &ar_sistema);
    botao_desenhar(j, &bt_guardar);
    botao_desenhar(j, &bt_cancelar);
    char t[200];
    formatar_texto(t, sizeof t, "Se guarda en %s (la clave, en claro)%s",
                   ia_pasta(), pasta_temporal ? "; sin /home se pierde al apagar" : "");
    texto_limitado(px + 220, py + 316, t, w - 24 - 220, COR_TEXTO_FRACO);
}

static void desenhar(void)
{
    int larg = j->larg, alt = j->alt, x = x0(), w = largura_principal();
    g_retangulo(j, 0, 0, larg, alt, COR_FUNDO);

    /* La columna de las conversaciones. */
    desenhar_botao(&bt_nova, !ocupada);
    lista_desenhar(j, &lista);
    if (n_conv == 0)
        texto_limitado(lista.x + 6, lista.y + 6, "(sin conversaciones)", lista.l - 12, COR_TEXTO_FRACO);
    desenhar_botao(&bt_renomear, !ocupada && mensagens_reais() > 0);
    desenhar_botao(&bt_apagar, !ocupada && conv_arquivo[0]);
    g_retangulo(j, LATERAL, 0, 1, alt, COR_BORDA);

    /* Modelo, servidor y estado. */
    char linha[320];
    formatar_texto(linha, sizeof linha, "Modelo: %s", cfg.modelo[0] ? cfg.modelo : "(sin modelo)");
    texto_limitado(x, 8, linha, 220, COR_TEXTO);
    formatar_texto(linha, sizeof linha, "Servidor: %s", cfg.url[0] ? cfg.url : "(sin configurar)");
    texto_limitado(x + 230, 8, linha, w - 230 - 130, COR_TEXTO_FRACO);
    desenhar_botao(&bt_prefs, !ocupada);
    uint32_t cor = estado_erro || !cfg.url[0] ? 0xB3261E : ocupada ? COR_SELECAO : COR_DESTAQUE;
    g_retangulo(j, x, 34, 8, 8, cor);
    texto_limitado(x + 14, 30, estado, w - 14 - 130, cor);
    formatar_texto(linha, sizeof linha, "Mensajes: %d", mensagens_reais());
    texto_limitado(larg - MARGEM - 120, 30, linha, 120, COR_TEXTO_FRACO);

    desenhar_botao(&bt_modo, !ocupada);
    if (modo_agente && panel) { ag_panel_desenhar(panel); jan_mostrar(j); return; }
    if (prefs_abertas) desenhar_prefs();
    else rico_desenhar(j, visor);

    /* Abajo: adjuntar, regenerar, la pregunta y enviar/detener. */
    desenhar_botao(&bt_adjuntar, !ocupada);
    desenhar_botao(&bt_regenerar, pode_regenerar());
    if (adjunto) {
        formatar_texto(linha, sizeof linha, "Adjunto: %s (%d bytes)", adjunto_nome, adjunto_n);
        texto_limitado(x + 210, alt - BASE + 6, linha, bt_quitar.x - x - 220, COR_SELECAO);
        desenhar_botao(&bt_quitar, !ocupada);
    } else
        texto_limitado(x + 210, alt - BASE + 6, "Intro: enviar | Mayús+Intro: otra línea | Esc: detener",
                       w - 210, COR_TEXTO_FRACO);
    entrada.foco = !ocupada && foco == FOCO_ENTRADA;
    area_desenhar(j, &entrada);
    desenhar_botao(&bt_enviar, !ocupada);
    desenhar_botao(&bt_deter, ocupada);
    jan_mostrar(j);
}

static void definir_estado(bool erro, const char *t)
{
    estado_erro = erro;
    copia_texto(estado, t, sizeof estado);
}

/* ------------------------------------------------------------- petición */

static int resposta;          /* el mensaje de la respuesta que está llegando (-1 = aún no) */
static int resposta_n;

static void ao_texto(const char *t, int n, void *ctx)
{
    (void)ctx;
    static char tmp[3200];
    while (n > 0) {
        int parte = n > 1000 ? 1000 : n;
        int m = utf8_latin1(t, parte, tmp, sizeof tmp);
        t += parte;
        n -= parte;
        if (m <= 0) continue;
        bool seguia = rico_no_fim(visor);
        if (resposta < 0) {
            resposta = msg_add("assistant", "", 0);
            if (resposta < 0) return;
            rico_secao(visor, "Asistente", COR_DESTAQUE, true);
        }
        msg_juntar(resposta, tmp, m);
        rico_juntar(visor, tmp, m);
        resposta_n += m;
        if (seguia) rico_ir_fim(visor);
        if (!prefs_abertas) rico_desenhar(j, visor);
    }
}

/* Mientras llega la respuesta: «Detener», Esc, la rueda y «Copiar» siguen
 * funcionando (el programa no tiene hilos: los eventos se miran aquí). */
static int deve_parar(void *ctx)
{
    (void)ctx;
    evento_t e;
    while (proximo_evento(j->id, &e, false) == 1) {
        if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) parar = true;
        else if (e.tipo == EV_CLIQUE && (e.botoes & 1) &&
                 e.x >= bt_deter.x && e.x < bt_deter.x + bt_deter.l &&
                 e.y >= bt_deter.y && e.y < bt_deter.y + bt_deter.a) parar = true;
        else if (e.tipo == EV_FECHAR) { parar = true; fechar_pendente = true; }
        else if (!prefs_abertas && (e.tipo == EV_RODA || e.tipo == EV_CLIQUE ||
                 (e.tipo == EV_TECLA && (e.tecla == TECLA_PGUP || e.tecla == TECLA_PGDN)))) {
            if (rico_tratar(j, visor, &e) == RICO_COPIOU)
                registrar("chatia: copiado %d bytes al portapapeles\n", rico_copiado(visor));
        }
    }
    return parar ? 1 : 0;
}

/* Lo que se sabe de la respuesta, para las pruebas (y para quien mire el registro). */
static void registrar_resposta(void)
{
    if (resposta < 0) return;
    int s = rico_secoes(visor) - 1;
    rico_contagem_t c;
    rico_contar(visor, s, &c);
    registrar("chatia: markdown negrita=%d cursiva=%d titulos=%d codigo=%d listas=%d bloques=%d\n",
              c.negrita, c.cursiva, c.titulos, c.codigo_linha, c.itens_lista, c.blocos_codigo);
    for (int k = 0; k < c.blocos_codigo; k++) {
        retangulo_t r;
        if (rico_botao_copiar(visor, s, k, &r))
            registrar("chatia: bloque %d copiar en (%d,%d)\n", k, r.x + r.largura / 2, r.y + r.altura / 2);
    }
}

/* Pide la respuesta a la conversación tal como está (la última es del usuario). */
static void pedir(void)
{
    /* El contexto: los últimos mensajes, hasta HIST_MAX y CONTEXTO_MAX bytes,
     * empezando por una pregunta (algunos servidores no aceptan otra cosa). */
    static ia_mensagem_ref_t refs[HIST_MAX];
    int idx[HIST_MAX], q = 0, bytes = 0;
    for (int i = n_msgs - 1; i >= 0 && q < HIST_MAX; i--) {
        if (eh(&msgs[i], "nota")) continue;
        if (q > 0 && bytes + msgs[i].n > CONTEXTO_MAX) break;
        idx[q++] = i;
        bytes += msgs[i].n;
    }
    while (q > 1 && !eh(&msgs[idx[q - 1]], "user")) q--;
    for (int k = 0; k < q; k++) {
        refs[k].papel = msgs[idx[q - 1 - k]].papel;
        refs[k].texto = msgs[idx[q - 1 - k]].texto;
    }
    static ia_config_t c;
    c = cfg;
    copia_texto(c.sistema, conv_sistema, sizeof c.sistema);

    char erro[256];
    resposta = -1;
    resposta_n = 0;
    n_pend = 0;
    ocupada = true;
    parar = false;
    definir_estado(false, "Escribiendo... Esc: detener");
    desenhar();
    registrar("chatia: pergunta enviada (%d mensagens de contexto)\n", q);
    int r = ia_chat_conversa_ref(&c, refs, q, ao_texto, NULL, NULL, NULL, deve_parar, NULL,
                                 erro, sizeof erro);
    ocupada = false;
    if (r < 0 && !parar) {
        char t[300];
        formatar_texto(t, sizeof t, "Error: %s", erro);
        definir_estado(true, t);
        nota(t);
    } else if (parar) definir_estado(false, "Respuesta detenida");
    else if (resposta_n == 0) definir_estado(true, "La respuesta está vacía");
    else definir_estado(false, "Listo");
    registrar("chatia: resposta completa (%d bytes, %d mensagens na conversa)\n", resposta_n, mensagens_reais());
    registrar_resposta();
    guardar_conversa();
    desenhar();
}

/* ------------------------------------------------------------- acciones */

static void quitar_adjunto(void)
{
    libera(adjunto);
    adjunto = NULL;
    adjunto_n = 0;
    adjunto_nome[0] = 0;
}

static void enviar(void)
{
    if (ocupada) return;
    if ((!entrada.buf || !entrada.buf[0]) && !adjunto) return;
    if (!cfg.url[0]) {
        definir_estado(true, "Falta el servidor: «Preferencias» o la ventana Configuración > IA");
        desenhar();
        return;
    }
    /* La pregunta y, si hay, el fichero adjunto como bloque de código. */
    int np = entrada.buf ? entrada.tam : 0;
    int cap = np + adjunto_n + 200;
    char *t = aloca((size_t)cap);
    if (!t) return;
    memcpy(t, entrada.buf ? entrada.buf : "", (size_t)np);
    int n = np;
    t[n] = 0;
    if (!conv_arquivo[0] && mensagens_reais() == 0) titulo_automatico(t);
    if (adjunto) {
        /* La cerca no puede aparecer dentro del fichero. */
        const char *cerca = "```";
        for (int i = 0; i + 2 < adjunto_n; i++)
            if (adjunto[i] == '`' && adjunto[i + 1] == '`' && adjunto[i + 2] == '`') { cerca = "~~~~"; break; }
        const char *ext = acha_ultimo_char(adjunto_nome, '.');
        n += formatar_texto(t + n, (size_t)(cap - n), "%sArchivo adjunto: %s (%d bytes)\n%s%s\n",
                            n ? "\n\n" : "", adjunto_nome, adjunto_n, cerca, ext ? ext + 1 : "");
        memcpy(t + n, adjunto, (size_t)adjunto_n);
        n += adjunto_n;
        if (n && t[n - 1] != '\n') t[n++] = '\n';
        n += formatar_texto(t + n, (size_t)(cap - n), "%s", cerca);
        registrar("chatia: pregunta con adjunto %s (%d bytes)\n", adjunto_nome, adjunto_n);
    }
    mostrar("user", t, n);
    libera(t);
    area_definir(&entrada, "", 0);
    entrada.cursor = 0;
    quitar_adjunto();
    if (!conv_arquivo[0]) registrar_conversa_nova();
    guardar_conversa();   /* la pregunta queda guardada aunque la respuesta falle */
    pedir();
}

static void regenerar(void)
{
    if (!pode_regenerar()) return;
    while (n_msgs && eh(&msgs[n_msgs - 1], "nota")) { msg_remover_ultima(); rico_remover_ultima(visor); }
    if (n_msgs && eh(&msgs[n_msgs - 1], "assistant")) { msg_remover_ultima(); rico_remover_ultima(visor); }
    if (!n_msgs || !eh(&msgs[n_msgs - 1], "user")) return;
    registrar("chatia: regenerar la ultima respuesta\n");
    rico_ir_fim(visor);
    pedir();
}

static void nova_conversa(void)
{
    if (ocupada) return;
    msgs_limpar();
    conv_arquivo[0] = 0;
    conv_titulo[0] = 0;
    copia_texto(conv_sistema, SISTEMA_PADRAO, sizeof conv_sistema);
    lista.sel = -1;
    rico_limpar(visor);
    nota("Conversación nueva. Escribe una pregunta abajo; la respuesta llega en castellano.");
    if (pasta_temporal) nota("Sin /home persistente: las conversaciones se guardan en /tmp y se pierden al apagar.");
    area_definir(&entrada, "", 0);
    definir_estado(false, "Listo");
    desenhar();
}

/* Abre la conversación i de la lista. */
static void abrir_conversa(int i)
{
    if (ocupada || i < 0 || i >= n_conv) return;
    unsigned tam;
    char *t = ler_conv(arquivos[i], &tam);
    if (!t) { definir_estado(true, "No se pudo leer la conversación"); desenhar(); return; }
    msgs_limpar();
    copia_texto(conv_arquivo, arquivos[i], sizeof conv_arquivo);
    copia_texto(conv_titulo, itens[i], sizeof conv_titulo);
    conv_sistema[0] = 0;
    /* Línea a línea: "--- papel" abre un mensaje; la "\" de delante se quita.
     * El contenido se compacta dentro del propio búfer (siempre hacia atrás). */
    char papel[12] = "";
    int ini = 0, n = 0;
    char *p = t;
    for (;;) {
        char *f = acha_char(p, '\n');
        if (f) *f = 0;
        bool cab = comeca_com(p, "--- ");
        if ((cab || !f) && papel[0]) {
            if (!cab && *p) {        /* la última línea, sin salto */
                const char *s = *p == '\\' ? p + 1 : p;
                int ln = (int)tam_texto(s);
                memmove(t + ini + n, s, (size_t)ln);
                n += ln;
            }
            while (n > 0 && (t[ini + n - 1] == '\n' || t[ini + n - 1] == '\r')) n--;
            if (compara_texto(papel, "system") == 0) {
                int k = n < IA_SISTEMA_MAX - 1 ? n : IA_SISTEMA_MAX - 1;
                memcpy(conv_sistema, t + ini, (size_t)k);
                conv_sistema[k] = 0;
            } else if (n > 0) {
                char c = t[ini + n];
                t[ini + n] = 0;
                msg_add(compara_texto(papel, "assistant") == 0 ? "assistant" : "user", t + ini, n);
                t[ini + n] = c;
            }
            papel[0] = 0;
        }
        if (!f) break;
        if (cab) {
            copia_texto(papel, p + 4, sizeof papel);
            ini = (int)(f + 1 - t);
            n = 0;
        } else if (papel[0]) {
            const char *s = *p == '\\' ? p + 1 : p;
            int ln = (int)tam_texto(s);
            memmove(t + ini + n, s, (size_t)ln);
            n += ln;
            t[ini + n++] = '\n';
        }
        p = f + 1;
    }
    libera(t);
    lista.sel = i;
    refazer_visor();
    definir_estado(false, "Conversación abierta");
    registrar("chatia: conversacion abierta %s (%d mensajes)\n", conv_arquivo, mensagens_reais());
    desenhar();
}

static void renomear_conversa(void)
{
    if (ocupada || mensagens_reais() == 0) return;
    char t[64];
    copia_texto(t, conv_titulo, sizeof t);
    if (!pedir_texto("Renombrar conversación", "Título nuevo:", t, sizeof t) || !t[0]) { desenhar(); return; }
    copia_texto(conv_titulo, t, sizeof conv_titulo);
    int i = indice_atual();
    if (i >= 0) copia_texto(itens[i], t, sizeof itens[0]);
    guardar_conversa();
    registrar("chatia: conversacion renombrada %s: %s\n", conv_arquivo, conv_titulo);
    definir_estado(false, "Conversación renombrada");
    desenhar();
}

static void apagar_conversa(void)
{
    if (ocupada || !conv_arquivo[0]) return;
    char p[160];
    formatar_texto(p, sizeof p, "¿Borrar la conversación «%s»?\nNo se puede deshacer.", conv_titulo);
    if (!confirmar("Borrar conversación", p)) { desenhar(); return; }
    char c[128];
    caminho_conv(c, sizeof c, conv_arquivo);
    int r = apagar(c);
    registrar("chatia: conversacion borrada %s (%d)\n", conv_arquivo, r);
    int i = indice_atual();
    if (i >= 0) {
        for (int k = i; k < n_conv - 1; k++) {
            memcpy(itens[k], itens[k + 1], sizeof itens[0]);
            memcpy(arquivos[k], arquivos[k + 1], sizeof arquivos[0]);
        }
        n_conv--;
        lista.qtd = n_conv;
    }
    nova_conversa();
    definir_estado(r < 0, r < 0 ? "No se pudo borrar el fichero" : "Conversación borrada");
    desenhar();
}

/* ¿Es UTF-8 válido? (si no, el fichero se toma como Latin-1). */
static bool eh_utf8(const char *d, unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        unsigned char b = (unsigned char)d[i];
        if (b < 0x80) continue;
        int k = (b & 0xE0) == 0xC0 ? 1 : (b & 0xF0) == 0xE0 ? 2 : (b & 0xF8) == 0xF0 ? 3 : -1;
        if (k < 0 || i + (unsigned)k >= n) return false;
        for (int q = 1; q <= k; q++) if (((unsigned char)d[i + q] & 0xC0) != 0x80) return false;
        i += (unsigned)k;
    }
    return true;
}

static void erro_adjunto(const char *t)
{
    definir_estado(true, t);
    registrar("chatia: adjunto rechazado: %s\n", t);
    desenhar();
}

/* Lee un fichero de texto para adjuntarlo (con su límite de tamaño). */
static void adjuntar(void)
{
    if (ocupada) return;
    char c[128] = "/home/";
    if (!dialogo_abrir("Adjuntar archivo", "Texto (*.txt;*.md;*.c;*.h;*.py;*.sh)|*.txt;*.md;*.c;*.h;*.py;*.sh|Todos los archivos (*.*)|*", c, c, sizeof c) || !c[0]) {
        desenhar();
        return;
    }
    info_arquivo_t inf;
    char t[200];
    if (info_arquivo(c, &inf) < 0 || inf.tipo != TIPO_ARQUIVO) {
        formatar_texto(t, sizeof t, "No existe el archivo %s", c);
        erro_adjunto(t);
        return;
    }
    if (inf.tamanho > ADJUNTO_MAX * 2) {
        formatar_texto(t, sizeof t, "El archivo es demasiado grande (%u bytes; máximo %d)", inf.tamanho, ADJUNTO_MAX);
        erro_adjunto(t);
        return;
    }
    int fd = abrir(c, ABRIR_LER);
    unsigned n = 0;
    char *d = fd >= 0 ? ler_todo_fd(fd, &n) : NULL;
    if (fd >= 0) fechar(fd);
    if (!d) { erro_adjunto("No se pudo leer el archivo"); return; }
    for (unsigned i = 0; i < n; i++)
        if (d[i] == 0) {
            libera(d);
            erro_adjunto("El archivo no parece de texto (tiene bytes nulos)");
            return;
        }
    if (eh_utf8(d, n)) {
        char *l = aloca(n + 8);
        if (!l) { libera(d); return; }
        n_pend = 0;
        n = (unsigned)utf8_latin1(d, (int)n, l, (int)n + 8);
        n_pend = 0;
        libera(d);
        d = l;
    }
    if (n > ADJUNTO_MAX) {
        libera(d);
        formatar_texto(t, sizeof t, "El archivo es demasiado grande (%u bytes; máximo %d)", n, ADJUNTO_MAX);
        erro_adjunto(t);
        return;
    }
    quitar_adjunto();
    adjunto = d;
    adjunto_n = (int)n;
    copia_texto(adjunto_nome, nome_base(c), sizeof adjunto_nome);
    registrar("chatia: adjunto %s (%d bytes)\n", c, adjunto_n);
    formatar_texto(t, sizeof t, "Adjunto listo: %s; va con la próxima pregunta", adjunto_nome);
    definir_estado(false, t);
    desenhar();
}

/* ------------------------------------------------------------- preferencias */

static void abrir_prefs(void)
{
    if (ocupada) return;
    prefs_abertas = true;
    caixa_definir(&cx_url, cfg.url);
    caixa_definir(&cx_chave, "");
    caixa_definir(&cx_modelo, cfg.modelo);
    area_definir(&ar_sistema, conv_sistema, (int)tam_texto(conv_sistema));
    lista_modelos.qtd = 0;
    lista_modelos.sel = -1;
    foco = FOCO_URL;
    registrar("chatia: preferencias abiertas\n");
    desenhar();
}

static void fechar_prefs(void)
{
    prefs_abertas = false;
    foco = FOCO_ENTRADA;
    desenhar();
}

static void guardar_prefs(void)
{
    /* Lo compartido (D145): se lee el fichero tal cual, se cambian el
     * servidor, la clave (si se escribió una) y el modelo del chat, y se
     * vuelve a escribir. Los modelos del terminal y del Mico no se tocan. */
    ia_config_t g;
    ia_config_carregar_arquivo(&g);
    copia_texto(g.url, cx_url.texto, sizeof g.url);
    copia_texto(g.modelo_uso[IA_USO_CHATIA], cx_modelo.texto, IA_MODELO_MAX);
    if (!g.modelo[0]) copia_texto(g.modelo, cx_modelo.texto, sizeof g.modelo);
    bool chave = cx_chave.texto[0] != 0;
    if (chave) copia_texto(g.chave, cx_chave.texto, sizeof g.chave);
    int r = ia_config_gravar(&g, chave);
    /* Y la sesión. */
    copia_texto(cfg.url, cx_url.texto, sizeof cfg.url);
    copia_texto(cfg.modelo, cx_modelo.texto, sizeof cfg.modelo);
    if (chave) copia_texto(cfg.chave, cx_chave.texto, sizeof cfg.chave);
    /* El mensaje de sistema es de la conversación. */
    int n = ar_sistema.tam < IA_SISTEMA_MAX - 1 ? ar_sistema.tam : IA_SISTEMA_MAX - 1;
    memcpy(conv_sistema, ar_sistema.buf ? ar_sistema.buf : "", (size_t)n);
    conv_sistema[n] = 0;
    guardar_conversa();
    registrar("chatia: preferencias guardadas (modelo %s, %d)\n", cfg.modelo, r);
    char t[200];
    if (r < 0) formatar_texto(t, sizeof t, "No se pudo guardar la configuración (%s)", nome_erro(r));
    else formatar_texto(t, sizeof t, "Preferencias guardadas en %s%s", ia_pasta(),
                        pasta_temporal ? " (sin /home: solo hasta apagar)" : "");
    definir_estado(r < 0, t);
    fechar_prefs();
}

static void cargar_modelos(void)
{
    static ia_config_t c;
    c = cfg;
    copia_texto(c.url, cx_url.texto, sizeof c.url);
    if (cx_chave.texto[0]) copia_texto(c.chave, cx_chave.texto, sizeof c.chave);
    char erro[200];
    definir_estado(false, "Pidiendo la lista de modelos...");
    desenhar();
    int n = ia_modelos(&c, modelos, MODELOS_MAX, erro, sizeof erro);
    if (n < 0) {
        char t[260];
        formatar_texto(t, sizeof t, "Error al pedir los modelos: %s", erro);
        definir_estado(true, t);
        n = 0;
    } else {
        char t[80];
        formatar_texto(t, sizeof t, "%d modelos en el servidor", n);
        definir_estado(false, t);
    }
    lista_modelos.qtd = n;
    lista_modelos.sel = -1;
    lista_modelos.topo = 0;
    registrar("chatia: %d modelos cargados\n", n);
    desenhar();
}

static void menu_provedores(int x, int y)
{
    static char t[512];
    int k = 0;
    for (int i = 0; i < n_provedores; i++)
        k += formatar_texto(t + k, sizeof t - (size_t)k, "%s%s", i ? "\n" : "", provedores[i].nome);
    menu_contextual(j->id, x, y, t, MENU_PROV);
}

/* ------------------------------------------------------------- eventos */

static bool dentro(int x, int y, botao_t *b)
{
    return x >= b->x && x < b->x + b->l && y >= b->y && y < b->y + b->a;
}

static bool dentro_caixa(int x, int y, caixa_t *c)
{
    return x >= c->x && x < c->x + c->l && y >= c->y && y < c->y + LETRA_A + 8;
}

static bool dentro_area(int x, int y, area_t *a)
{
    return x >= a->x && x < a->x + a->l && y >= a->y && y < a->y + a->a;
}

static bool dentro_lista(int x, int y, lista_t *l)
{
    return x >= l->x && x < l->x + l->l && y >= l->y && y < l->y + l->a;
}

static void copiar_conversa(void)
{
    int cap = 64;
    for (int i = 0; i < n_msgs; i++) cap += msgs[i].n + 16;
    char *t = aloca((size_t)cap);
    if (!t) return;
    int n = 0;
    for (int i = 0; i < n_msgs; i++) {
        if (eh(&msgs[i], "nota")) continue;
        n += formatar_texto(t + n, (size_t)(cap - n), "%s%s:\n", n ? "\n\n" : "",
                            eh(&msgs[i], "user") ? "Tú" : "Asistente");
        memcpy(t + n, msgs[i].texto, (size_t)msgs[i].n);
        n += msgs[i].n;
    }
    int r = copiar_texto(t, (unsigned)n);
    libera(t);
    definir_estado(false, "Conversación copiada al portapapeles");
    registrar("chatia: conversacion copiada (%d bytes)\n", r);
}

static void tratar_menu(const evento_t *e)
{
    int i = (int)e->x;
    if (i < 0) { desenhar(); return; }
    if (e->tecla == MENU_LISTA) {
        if (i == 0) abrir_conversa(lista.sel);
        else if (i == 1) renomear_conversa();
        else if (i == 2) apagar_conversa();
    } else if (e->tecla == MENU_VISOR) {
        if (i == 0 && menu_secao >= 0 && menu_secao < n_msgs) {
            copiar_texto(msgs[menu_secao].texto, (unsigned)msgs[menu_secao].n);
            definir_estado(false, "Mensaje copiado al portapapeles");
        } else if (i == 1) copiar_conversa();
    } else if (e->tecla == MENU_PROV && i < n_provedores) {
        caixa_definir(&cx_url, provedores[i].url);
        caixa_definir(&cx_modelo, provedores[i].modelo);
        char t[120];
        formatar_texto(t, sizeof t, "Proveedor %s%s", provedores[i].nome,
                       provedores[i].precisa_chave ? ": hace falta una clave de API" : "");
        definir_estado(false, t);
    }
    desenhar();
}

static void clique_prefs(const evento_t *e)
{
    if (dentro(e->x, e->y, &bt_guardar)) { guardar_prefs(); return; }
    if (dentro(e->x, e->y, &bt_cancelar)) { fechar_prefs(); return; }
    if (dentro(e->x, e->y, &bt_prov)) { menu_provedores(bt_prov.x, bt_prov.y + bt_prov.a); return; }
    if (dentro(e->x, e->y, &bt_cargar)) { cargar_modelos(); return; }
    if (dentro(e->x, e->y, &bt_padrao)) {
        area_definir(&ar_sistema, SISTEMA_PADRAO, (int)tam_texto(SISTEMA_PADRAO));
        desenhar();
        return;
    }
    if (lista_modelos.qtd && dentro_lista(e->x, e->y, &lista_modelos)) {
        if (lista_tratar(j, &lista_modelos, e) != LISTA_NADA && lista_modelos.sel >= 0)
            caixa_definir(&cx_modelo, modelos[lista_modelos.sel]);
        foco = FOCO_MODELO;
        desenhar();
        return;
    }
    if (dentro_caixa(e->x, e->y, &cx_url)) { foco = FOCO_URL; caixa_tratar(j, &cx_url, e); }
    else if (dentro_caixa(e->x, e->y, &cx_chave)) { foco = FOCO_CHAVE; caixa_tratar(j, &cx_chave, e); }
    else if (dentro_caixa(e->x, e->y, &cx_modelo)) { foco = FOCO_MODELO; caixa_tratar(j, &cx_modelo, e); }
    else if (dentro_area(e->x, e->y, &ar_sistema)) {
        foco = FOCO_SISTEMA;
        ar_sistema.foco = true;
        area_tratar(j, &ar_sistema, e);
    }
    desenhar();
}

static void tecla(const evento_t *e)
{
    if (e->tecla == TECLA_ESC) {
        if (prefs_abertas) fechar_prefs();
        return;
    }
    if (e->tecla == 14) { nova_conversa(); return; }          /* Ctrl+N */
    if (!prefs_abertas && (e->tecla == TECLA_PGUP || e->tecla == TECLA_PGDN ||
                           ((e->tecla == TECLA_HOME || e->tecla == TECLA_END) && (e->modificadores & 2)))) {
        rico_tratar(j, visor, e);
        return;
    }
    switch (foco) {
    case FOCO_URL: caixa_tratar(j, &cx_url, e); break;
    case FOCO_CHAVE: caixa_tratar(j, &cx_chave, e); break;
    case FOCO_MODELO: caixa_tratar(j, &cx_modelo, e); break;
    case FOCO_SISTEMA: area_tratar(j, &ar_sistema, e); break;
    default:
        /* Intro envía; Mayús+Intro escribe otra línea. */
        if (e->tecla == '\n' && !(e->modificadores & 1)) { enviar(); return; }
        area_tratar(j, &entrada, e);
        break;
    }
    desenhar();
}

static void clique(const evento_t *e)
{
    if (e->botoes & 2) {
        /* Clic derecho: el menú de la lista o el del mensaje. */
        if (dentro_lista(e->x, e->y, &lista)) {
            int antes = lista.sel;
            lista_tratar(j, &lista, e);
            if (lista.sel >= 0 && (lista.sel != antes || lista.sel != indice_atual())) abrir_conversa(lista.sel);
            if (lista.sel >= 0) menu_contextual(j->id, e->x, e->y, "Abrir\nRenombrar...\nBorrar", MENU_LISTA);
            return;
        }
        if (!prefs_abertas && (menu_secao = rico_secao_em(visor, e->x, e->y)) >= 0) {
            menu_contextual(j->id, e->x, e->y, "Copiar mensaje\nCopiar toda la conversación", MENU_VISOR);
            return;
        }
        if (dentro_area(e->x, e->y, &entrada)) area_tratar(j, &entrada, e);
        return;
    }
    if (dentro(e->x, e->y, &bt_nova)) { nova_conversa(); return; }
    if (dentro(e->x, e->y, &bt_renomear)) { renomear_conversa(); return; }
    if (dentro(e->x, e->y, &bt_apagar)) { apagar_conversa(); return; }
    if (dentro(e->x, e->y, &bt_prefs)) { if (prefs_abertas) fechar_prefs(); else abrir_prefs(); return; }
    if (dentro(e->x, e->y, &bt_enviar)) { enviar(); return; }
    if (dentro(e->x, e->y, &bt_adjuntar)) { adjuntar(); return; }
    if (adjunto && dentro(e->x, e->y, &bt_quitar)) {
        quitar_adjunto();
        definir_estado(false, "Adjunto quitado");
        desenhar();
        return;
    }
    if (dentro(e->x, e->y, &bt_regenerar)) { regenerar(); return; }
    if (dentro_lista(e->x, e->y, &lista)) {
        if (lista_tratar(j, &lista, e) != LISTA_NADA && lista.sel != indice_atual()) abrir_conversa(lista.sel);
        return;
    }
    if (prefs_abertas && e->x >= x0() && e->y >= TOPO && e->y < TOPO + altura_visor()) { clique_prefs(e); return; }
    if (!prefs_abertas) {
        int r = rico_tratar(j, visor, e);
        if (r == RICO_COPIOU) {
            char t[80];
            formatar_texto(t, sizeof t, "Código copiado al portapapeles (%d bytes)", rico_copiado(visor));
            definir_estado(false, t);
            registrar("chatia: copiado %d bytes al portapapeles\n", rico_copiado(visor));
            desenhar();
            return;
        }
        if (r != RICO_NADA) return;
    }
    if (dentro_area(e->x, e->y, &entrada)) {
        foco = FOCO_ENTRADA;
        entrada.foco = true;
        area_tratar(j, &entrada, e);
        desenhar();
    }
}

int principal(int argc, char **argv)
{
    ia_config_carregar(&cfg);
    /* El modelo que le toca a este programa (D145). */
    ia_config_escolher(&cfg, IA_USO_CHATIA);
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "--ajuda") == 0 || compara_texto(a, "--ayuda") == 0 || compara_texto(a, "-h") == 0) {
            imprimir("uso: chatia [--url URL] [--modelo MODELO]\n"
                     "  Abre la ventana del chat con IA: las conversaciones a la izquierda,\n"
                     "  la conversación (con Markdown) arriba y la pregunta abajo.\n"
                     "  Las conversaciones se guardan en /home/.config/ia/conversas.\n");
            return 0;
        }
        if ((compara_texto(a, "--url") == 0 || compara_texto(a, "--modelo") == 0) && i + 1 < argc) {
            const char *v = argv[++i];
            if (compara_texto(a, "--url") == 0) copia_texto(cfg.url, v, sizeof cfg.url);
            else copia_texto(cfg.modelo, v, sizeof cfg.modelo);
        }
    }
    n_provedores = ia_provedores(provedores, 16);
    formatar_texto(pasta_conv, sizeof pasta_conv, "%s/conversas", ia_pasta());
    criar_pasta(pasta_conv);
    pasta_temporal = !ia_pasta_persistente();

    j = jan_abrir("Chat IA", 860, 600);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL, 640, 500);
    visor = rico_criar(x0(), TOPO, largura_principal(), altura_visor());
    if (!visor) return 1;
    if (!area_iniciar(&entrada, x0(), j->alt - BASE + 32, largura_principal() - 230, BASE - 44)) return 1;
    if (!area_iniciar(&ar_sistema, 0, 0, 200, 72)) return 1;
    lista.itens = itens;
    lista.sel = -1;
    lista_modelos.itens = modelos;
    lista_modelos.sel = -1;
    cx_chave.oculta = true;
    panel = ag_panel_criar(j, x0(), TOPO, largura_principal(), j->alt - TOPO - 6, NULL);
    if (panel) ag_panel_config(panel, &cfg);
    maquetar();
    carregar_lista();
    nova_conversa();
    if (pasta_temporal) {
        definir_estado(false, "Sin /home persistente: las conversaciones se guardan en /tmp");
        registrar("chatia: sin /home, conversaciones en %s\n", pasta_conv);
    }
    desenhar();
    registrar("chatia: janela %dx%d pronta\n", j->larg, j->alt);

    evento_t e;
    for (;;) {
        if (fechar_pendente) break;
        if (proximo_evento(j->id, &e, false) != 1) {
            if (esperar_evento(j->id, &e, 60) != 1) continue;
        }
        if (e.tipo == EV_FECHAR) break;
        if (jan_redimensionar(j, &e)) {
            bool seguia = rico_no_fim(visor);
            maquetar();
            if (seguia) rico_ir_fim(visor);
            desenhar();
            continue;
        }
        /* D295: el botón de modo y, en modo agente, todo lo demás va al panel. */
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && dentro(e.x, e.y, &bt_modo) && panel) {
            modo_agente = !modo_agente;
            registrar("chatia: modo %s\n", modo_agente ? "agente" : "chat");
            maquetar();
            desenhar();
            continue;
        }
        if (modo_agente && panel && !(e.tipo == EV_CLIQUE && dentro(e.x, e.y, &bt_prefs)) && e.tipo != EV_MENU) {
            if (ag_panel_tratar(panel, &e)) continue;
        }
        if (e.tipo == EV_MENU) { tratar_menu(&e); continue; }
        if (e.tipo == EV_TECLA) { tecla(&e); continue; }
        if (e.tipo == EV_RODA) {
            if (!prefs_abertas && rico_tratar(j, visor, &e) != RICO_NADA) continue;
            if (lista_tratar(j, &lista, &e) != LISTA_NADA) continue;
            if (prefs_abertas && lista_modelos.qtd) lista_tratar(j, &lista_modelos, &e);
            continue;
        }
        if (e.tipo == EV_CLIQUE) { clique(&e); continue; }
        if (e.tipo == EV_MOVER || e.tipo == EV_SOLTOU) {
            if (foco == FOCO_SISTEMA && prefs_abertas) area_tratar(j, &ar_sistema, &e);
            else if (foco == FOCO_ENTRADA) area_tratar(j, &entrada, &e);
        }
    }
    return 0;
}
