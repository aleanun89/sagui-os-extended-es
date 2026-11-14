// SPDX-License-Identifier: GPL-3.0-only
/* cipo [dirección] (alias navegador) - el Cipó, navegador de SaguiOS (D047).
 *
 * Autor original: M4R4G0N (PR #3 del repositorio original); adaptado para la
 * pila de red de D046 (HTTP/1.1 con chunked, libsagui/http.c) y con la
 * interfaz traducida al español.
 *
 * El nombre: el sagui atraviesa la selva saltando de liana en liana; el navegador
 * atraviesa la web saltando de enlace en enlace.
 *
 * Descarga la página por HTTP/1.1 (libsagui, http.c), lee el HTML y diagrama el texto
 * en la ventana: títulos, párrafos, listas, negrita, enlaces clicables, líneas
 * y formularios simples (una caja de texto, como en una búsqueda). No ejecuta
 * JavaScript, no carga imágenes ni CSS: la página aparece como texto
 * organizado, al estilo de los primeros navegadores. HTTPS queda afuera (no
 * hay TLS en SaguiOS); los sitios que todavía hablan HTTP se abren.
 *
 * Cómo funciona:
 *   1. el cuerpo entero se descarga a memoria (hasta 1 MB) y se convierte de
 *      UTF-8 a Latin-1, la codificación de SaguiOS;
 *   2. el lector de HTML recorre el texto una vez, y cada palabra se vuelve un
 *      "segmento" ya con línea, columna y estilo (la diagramación se hace en
 *      el momento, para el ancho fijo de la ventana);
 *   3. dibujar es solo pintar los segmentos de las líneas visibles. */
#include "tls.h"
#include "gui.h"
#include "imagem.h"
#include "motor.h"
#include "js.h"
#include "qjs_bridge.h"   /* D149: o motor de JavaScript da pagina (QuickJS) */

static motor_t *motor;
static int motor_altura;
static int rol_px;
static int foco_campo = -1;

/* La ventana es redimensionable y maximizable (D110): el tamaño es el actual
 * (j->larg x j->alt) y todo lo que depende de él se calcula con estas macros. */
#define LARG_INICIO 860
#define ALT_INICIO  600
#define LARG_MIN    560
#define ALT_MIN     300
#define LARG      (j->larg)
#define ALT       (j->alt)
#define TOPO      40
#define ABA_A     26                /* altura da barra de abas (D148) */
#define ABA_Y     (TOPO + ABA_A)    /* onde começa a página */
#define RODAPE    28
#define AREA_A    (ALT - ABA_Y - RODAPE)
#define ROLAGEM   14
#define MARGEM    12
#define LARG_TEXTO (LARG - ROLAGEM - 2 * MARGEM)
#define LINHA_A   18
#define VISIVEIS  ((AREA_A - 8) / LINHA_A)
#define URL_MAX   600
#define PAGINA_MAX (1024 * 1024)
#define HIST_MAX  16
#define IMG_MAX_QTD   12            /* imágenes descargadas por página */
#define IMG_MAX_BYTES (4 * 1024 * 1024)  /* tamaño máximo de cada archivo de imagen */
#define IMG_MAX_PIXELS (800 * 600)       /* píxeles de la imagen ya escalada (a janela tem 860 px) */
#define IMG_ALTURA_MAX 700          /* altura máxima en pantalla (px) */
#define MOTOR_NOS_MAX 20000
#define MOTOR_CMDS_MAX 40000
#define MOTOR_ALTURA_MAX (1024 * 1024)
#define FAIXA_PINTURA 96
#define TECLA_F3_CIPO 0x10C         /* TECLA_F1 + 2 en teclado.h del kernel */
#define TECLA_F5_CIPO 0x10E

/* Estilos (bits) */
#define E_NEGRITO 1
#define E_LINK    2
#define E_TITULO  4
#define E_ITALICO 8
#define E_PRE     16
#define E_CAMPO   32
#define E_LINHA   64   /* línea horizontal (<hr>) */
#define E_IMAGEM  128  /* imagen descargada (seg_t.img) */

typedef struct {
    int     linha;
    int16_t x, n;
    uint8_t estilo;
    int16_t alvo;        /* enlace o campo de formulario; -1 = ninguno */
    int     texto;       /* desplazamiento en "textos" */
    int16_t img, w, h;   /* E_IMAGEM: índice en "imgs" y tamaño en pantalla */
    int16_t nlin;        /* líneas que ocupa (1 el texto, varias la imagen) */
} seg_t;

/* Imágenes descargadas (se conservan al cambiar el ancho: solo se vuelve a maquetar). */
typedef struct { char url[URL_MAX]; imagem_t img; bool ok; } img_t;
static img_t imgs[IMG_MAX_QTD];
static int   qtd_imgs;
static char  pendentes[IMG_MAX_QTD][URL_MAX];   /* imágenes vistas al maquetar y aún no descargadas */
static int   qtd_pendentes;

enum { A_LINK = 1, A_CAMPO };
typedef struct { int tipo; int url; int form; int nome; } alvo_t;
typedef struct { int acao; char fixa[300]; int campo; } form_t;   /* campo = nombre del 1º campo de texto */

static seg_t  *segs;   static int qtd_segs, cap_segs;
static alvo_t *alvos;  static int qtd_alvos, cap_alvos;
static char   *textos; static int qtd_textos, cap_textos;
static form_t  forms[8];
static int     qtd_forms;
static int     linhas, topo;
static char    titulo[96];
static char    url_atual[URL_MAX];
static char    historico[HIST_MAX][URL_MAX];
static int     historico_pos[HIST_MAX];       /* onde se estava a ler (D148) */
static int     qtd_historico;
static char    futuro[HIST_MAX][URL_MAX];     /* para "Adelante" */
static int     futuro_pos[HIST_MAX];
static int     qtd_futuro;
/* Documento tal como llegó (HTML o texto plano): al cambiar el ancho de la ventana
 * se vuelve a maquetar a partir de él, sin descargarlo otra vez. */
static char   *doc;
static int     doc_tam;
static bool    doc_html = true;
static int     foco_alvo = -1;                /* enlace con foco de teclado (Tab); -1 = ninguno */
static bool    rolando;                       /* arrastrando el pulgar de la barra */
static int     rolando_dy;
static bool    endereco_sel;                  /* la dirección está "seleccionada" (Ctrl+L): lo que se escribe la reemplaza */

/* Búsqueda en la página (Ctrl+F). */
static bool    buscando;
static caixa_t caixa_busca;
static char    consulta[64];
static char   *plano;                         /* todo el texto de la página, para buscar */
static int     plano_tam;
static int    *plano_ini;                     /* desplazamiento de cada segmento en "plano" */
static int     coincide_atual = -1;           /* posición de la coincidencia actual en "plano" */
static int     coincidencias;
static bool recursos_pendentes;
static bool a_carregar_registrado;   /* ja se registou que a carga comecou */
static bool pintura_pendente;
static int pintura_y;
static char salvar_imagem_url[URL_MAX];
static char salvar_link_url[URL_MAX];

/* O relógio das animações (D148): o motor web avança com o relógio do SO, mas
 * a contagem começa a zero em cada página (senão a animação estaria no fim
 * assim que a página aparecia). */
static unsigned ms_base;

/* A posição do rato (para saber que link está por baixo quando se copia). */
static int mouse_x, mouse_y;

/* Abas (D148): cada aba guarda a sua página (o documento, o motor e onde se
 * estava a ler). Trocar de aba não volta a descarregar nada: a página fica em
 * memória, que é também a cache. */
#define ABAS_MAX 6
typedef struct {
    char    url[URL_MAX];
    char    titulo[96];
    char   *doc;
    int     doc_tam;
    bool    doc_html;
    motor_t *motor;
    int     motor_altura;
    int     rol_px;                   /* posição de leitura (px da página) */
    int     topo;
    bool    recursos_pendentes;
    bool    a_carregar_registrado;
    bool    pintura_pendente;
    int     pintura_y;
} aba_t;
static aba_t abas[ABAS_MAX];
static int   qtd_abas;
static int   aba_atual;

/* Marcadores (D148): os sítios que o utilizador guardou, num ficheiro de texto
 * (uma linha por marcador: url, uma tabulação e o título). */
#define MARC_MAX 32
static char marcador_url[MARC_MAX][URL_MAX];
static char marcador_titulo[MARC_MAX][96];
static int  qtd_marcadores;
#define COD_MENU_MARCADORES 64
#define COD_MENU_CONTEXTUAL 65   /* el menú del botón derecho (D152) */

/* Posições de leitura por sítio (D152): onde se ficou a ler em cada página,
 * para que ao voltar a ela apareça tal como se deixou. */
#define POS_MAX 64
static char pos_url[POS_MAX][URL_MAX];
static int  pos_px[POS_MAX];
static int  qtd_pos;

/* El menú del botón derecho (D152): cada línea tiene su acción, para que el
 * EV_MENU sepa qué hacer con la línea elegida. */
enum { ACAO_NADA = 0, ACAO_GUARDAR_IMAGEM, ACAO_GUARDAR_LINK, ACAO_ABA_NOVA };
static int menu_acoes[4];
static int qtd_menu_acoes;

static jan_t  *j;
static caixa_t endereco;
static botao_t bt_voltar = { 8, 7, 56, 26, "Atrás", false };
static botao_t bt_avancar = { 68, 7, 76, 26, "Adelante", false };
static botao_t bt_recarregar = { 148, 7, 76, 26, "Recargar", false };
static botao_t bt_inicio = { 228, 7, 56, 26, "Inicio", false };
static botao_t bt_ir = { 0, 7, 42, 26, "Ir", false };   /* x: ajustar_barra() */
static char rodape_texto[128];

/* ------------------------------------------------------------ memoria */

static bool crescer(void **p, int *cap, int precisa, int tam_item)
{
    if (precisa <= *cap) return true;
    int nova = *cap ? *cap * 2 : 1024;
    while (nova < precisa) nova *= 2;
    void *q = realoca(*p, (size_t)nova * (size_t)tam_item);
    if (!q) return false;
    *p = q;
    *cap = nova;
    return true;
}

static int guardar_texto(const char *t, int n)
{
    if (!crescer((void **)&textos, &cap_textos, qtd_textos + n + 1, 1)) return 0;
    int k = qtd_textos;
    memcpy(textos + k, t, (size_t)n);
    textos[k + n] = 0;
    qtd_textos += n + 1;
    return k;
}

/* ------------------------------------------------------------ texto */

/* Un carácter Unicode -> Latin-1 (lo que no existe se vuelve algo parecido). */
static char para_latin1(uint32_t c)
{
    if (c < 0x80 || (c >= 0xA0 && c < 0x100)) return c == 0xA0 ? ' ' : (char)c;
    switch (c) {
    case 0x2018: case 0x2019: case 0x201A: case 0x2032: return '\'';
    case 0x201C: case 0x201D: case 0x201E: case 0x2033: return '"';
    case 0x2013: case 0x2014: case 0x2212: return '-';
    case 0x2022: case 0x2026: case 0x00B7: return (char)0xB7;
    case 0x20AC: return 'E';
    default: return '?';
    }
}

/* Convierte el cuerpo a Latin-1, si es UTF-8 válido; si no, ya es Latin-1. */
static int converter(char *s, int n)
{
    for (int i = 0; i < n;) {          /* verifica primero */
        uint8_t c = (uint8_t)s[i];
        int extra = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
        if (extra < 0 || (extra && i + extra >= n)) return n;   /* no es UTF-8: ya es Latin-1 */
        for (int k = 1; k <= extra; k++) if (((uint8_t)s[i + k] & 0xC0) != 0x80) return n;
        i += 1 + extra;
    }
    int k = 0;
    for (int i = 0; i < n;) {
        uint8_t c = (uint8_t)s[i];
        uint32_t cp;
        int extra = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : 3;
        cp = extra == 0 ? c : extra == 1 ? (c & 0x1Fu) : extra == 2 ? (c & 0x0Fu) : (c & 0x07u);
        for (int e = 1; e <= extra; e++) cp = cp << 6 | ((uint8_t)s[i + e] & 0x3Fu);
        s[k++] = para_latin1(cp);
        i += 1 + extra;
    }
    return k;
}

static bool letra(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static char minuscula(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

static bool igual_sem_caixa(const char *a, const char *b, int n)
{
    for (int i = 0; i < n; i++) if (minuscula(a[i]) != minuscula(b[i])) return false;
    return true;
}

/* Uma letra acentuada vira a letra simples (D148): "cancion" encontra "canción"
 * e "atreves" encontra "através". O texto da página é Latin-1, por isso os
 * valores são os da Latin-1 (0xE1 = á, 0xE9 = é...). */
static char sem_tilde(unsigned char c)
{
    switch (c) {
    case 0xE1: case 0xC1: return 'a';   /* á Á */
    case 0xE9: case 0xC9: return 'e';   /* é É */
    case 0xED: case 0xCD: return 'i';   /* í Í */
    case 0xF3: case 0xD3: return 'o';   /* ó Ó */
    case 0xFA: case 0xDA: return 'u';   /* ú Ú */
    case 0xFC: case 0xDC: return 'u';   /* ü Ü */
    case 0xF1: case 0xD1: return 'n';   /* ñ Ñ */
    case 0xE7: case 0xC7: return 'c';   /* ç Ç */
    case 0xE0: case 0xC0: return 'a';   /* à À */
    case 0xE2: case 0xC2: return 'a';   /* â Â */
    case 0xEA: case 0xCA: return 'e';   /* ê Ê */
    case 0xEE: case 0xCE: return 'i';   /* î Î */
    case 0xF4: case 0xD4: return 'o';   /* ô Ô */
    case 0xFB: case 0xDB: return 'u';   /* û Û */
    case 0xBF: return '?';              /* ¿ */
    default: return (char)c;
    }
}

/* Como igual_sem_caixa(), mas sem tildes: a busca ignora os acentos. */
static bool igual_sem_caixa_e_tilde(const char *a, const char *b, int n)
{
    for (int i = 0; i < n; i++) {
        unsigned char x = (unsigned char)sem_tilde((unsigned char)a[i]);
        unsigned char y = (unsigned char)sem_tilde((unsigned char)b[i]);
        if (x >= 'A' && x <= 'Z') x = (unsigned char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (unsigned char)(y + 32);
        if (x != y) return false;
    }
    return true;
}

/* "&amp;", "&eacute;", "&#233;", "&#xE9;" -> un carácter. Devuelve cuántos
 * bytes de la entrada usó (0 = no era una entidad). */
static int entidade(const char *s, int n, char *saida)
{
    int fim = 1;
    while (fim < n && fim < 12 && s[fim] != ';' && s[fim] != '<' && !eh_espaco(s[fim])) fim++;
    if (fim >= n || s[fim] != ';' || fim < 2) return 0;
    const char *nome = s + 1;
    int t = fim - 1;
    if (nome[0] == '#') {
        uint32_t v = 0;
        bool hexa = t > 1 && (nome[1] == 'x' || nome[1] == 'X');
        for (int i = hexa ? 2 : 1; i < t; i++) {
            char c = minuscula(nome[i]);
            int d = c >= '0' && c <= '9' ? c - '0' : hexa && c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
            if (d < 0) return 0;
            v = v * (hexa ? 16 : 10) + (uint32_t)d;
        }
        *saida = para_latin1(v);
        return fim + 1;
    }
    static const struct { const char *n; uint8_t c; } nomes[] = {
        { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' }, { "apos", '\'' }, { "nbsp", ' ' },
        { "copy", 0xA9 }, { "reg", 0xAE }, { "laquo", 0xAB }, { "raquo", 0xBB }, { "middot", 0xB7 },
        { "bull", 0xB7 }, { "hellip", 0xB7 }, { "mdash", '-' }, { "ndash", '-' }, { "lsquo", '\'' },
        { "rsquo", '\'' }, { "ldquo", '"' }, { "rdquo", '"' }, { "deg", 0xB0 }, { "ordm", 0xBA },
        { "ordf", 0xAA }, { "szlig", 0xDF }, { "iexcl", 0xA1 }, { "iquest", 0xBF }, { "euro", 'E' },
    };
    for (unsigned i = 0; i < sizeof nomes / sizeof nomes[0]; i++)
        if ((int)tam_texto(nomes[i].n) == t && memcmp(nomes[i].n, nome, (size_t)t) == 0) { *saida = (char)nomes[i].c; return fim + 1; }
    /* Letra acentuada: "a" + "acute", "C" + "cedil"... */
    static const struct { const char *suf; const char *letras; uint8_t base[6]; } acentos[] = {
        { "acute", "AEIOUY", { 0xC1, 0xC9, 0xCD, 0xD3, 0xDA, 0xDD } },
        { "grave", "AEIOU", { 0xC0, 0xC8, 0xCC, 0xD2, 0xD9 } },
        { "circ", "AEIOU", { 0xC2, 0xCA, 0xCE, 0xD4, 0xDB } },
        { "tilde", "ANO", { 0xC3, 0xD1, 0xD5 } },
        { "uml", "AEIOU", { 0xC4, 0xCB, 0xCF, 0xD6, 0xDC } },
        { "cedil", "C", { 0xC7 } },
    };
    for (unsigned i = 0; i < sizeof acentos / sizeof acentos[0]; i++) {
        int ts = (int)tam_texto(acentos[i].suf);
        if (t != ts + 1 || memcmp(nome + 1, acentos[i].suf, (size_t)ts) != 0) continue;
        char l = nome[0];
        bool minusc = l >= 'a' && l <= 'z';
        char L = minusc ? (char)(l - 32) : l;
        for (int k = 0; acentos[i].letras[k]; k++)
            if (acentos[i].letras[k] == L) { *saida = (char)(acentos[i].base[k] + (minusc ? 0x20 : 0)); return fim + 1; }
    }
    return 0;
}

/* Copia s[0..n) a saida, cambiando las entidades. */
static int decodificar(const char *s, int n, char *saida, int max)
{
    int k = 0;
    for (int i = 0; i < n && k < max - 1;) {
        int u = s[i] == '&' ? entidade(s + i, n - i, &saida[k]) : 0;
        if (u) { k++; i += u; }
        else saida[k++] = s[i++];
    }
    saida[k] = 0;
    return k;
}

/* ------------------------------------------------------------ direcciones */

/* Une una dirección relativa a la de la página. false = no se puede seguir
 * (javascript:, mailto:, solo "#trecho"). */
static bool juntar_url(const char *base, const char *rel, char *saida, int max)
{
    while (*rel == ' ' || *rel == '\n' || *rel == '\t') rel++;
    if (!*rel || *rel == '#' || comeca_com(rel, "javascript:") || comeca_com(rel, "mailto:")) return false;
    if (comeca_com(rel, "http://") || comeca_com(rel, "https://") || comeca_com(rel, "sobre:")) {
        copia_texto(saida, rel, (size_t)max);
        return true;
    }
    if (comeca_com(rel, "//")) { formatar_texto(saida, (size_t)max, "http:%s", rel); return true; }
    if (!comeca_com(base, "http://")) return false;
    /* Raíz del sitio: "http://host[:puerto]". */
    int raiz = 7;
    while (base[raiz] && base[raiz] != '/' && base[raiz] != '?' && base[raiz] != '#') raiz++;
    int fim = raiz, ultima = -1;   /* fin del camino (sin consulta) y su última barra */
    while (base[fim] && base[fim] != '?' && base[fim] != '#') { if (base[fim] == '/') ultima = fim; fim++; }
    char caminho[URL_MAX];
    int k = 0;
    if (*rel == '/') {
        caminho[0] = 0;
    } else if (*rel == '?') {                       /* misma página, otra consulta */
        if (fim == raiz) caminho[k++] = '/';
        for (int i = raiz; i < fim && k < (int)sizeof caminho - 1; i++) caminho[k++] = base[i];
        caminho[k] = 0;
    } else {                                         /* misma carpeta */
        if (ultima < 0) caminho[k++] = '/';
        for (int i = raiz; i <= ultima && k < (int)sizeof caminho - 1; i++) caminho[k++] = base[i];
        caminho[k] = 0;
    }
    junta_texto(caminho, rel, sizeof caminho);
    /* Quita "/./" y "carpeta/../" del camino (la consulta queda como está). */
    char limpo[URL_MAX];
    char *q = acha_char(caminho, '?');
    int fim_caminho = q ? (int)(q - caminho) : (int)tam_texto(caminho);
    k = 0;
    int i = 0;
    while (i < fim_caminho && k < (int)sizeof limpo - 1) {
        if (caminho[i] == '/' && caminho[i + 1] == '.' && (i + 2 == fim_caminho || caminho[i + 2] == '/')) { i += 2; continue; }
        if (caminho[i] == '/' && caminho[i + 1] == '.' && caminho[i + 2] == '.' && (i + 3 == fim_caminho || caminho[i + 3] == '/')) {
            while (k > 0 && limpo[k - 1] != '/') k--;
            if (k > 0) k--;
            i += 3;
            continue;
        }
        limpo[k++] = caminho[i++];
    }
    if (k == 0) limpo[k++] = '/';
    limpo[k] = 0;
    if (q) junta_texto(limpo, q, sizeof limpo);
    k = 0;
    for (int r = 0; r < raiz && k < max - 1; r++) saida[k++] = base[r];
    saida[k] = 0;
    junta_texto(saida, limpo, (size_t)max);
    return true;
}

/* ------------------------------------------------------------ diagramación */

static int x_atual, linha_atual, recuo, estilo, alvo_atual = -1;
static bool linha_vazia = true, espaco_pendente;
static int brancas;          /* líneas en blanco seguidas (evita huecos) */

static void quebrar(void)
{
    if (!linha_vazia) { linha_atual++; brancas = 0; }
    x_atual = recuo;
    linha_vazia = true;
    espaco_pendente = false;
}

static void paragrafo(void)
{
    quebrar();
    if (linha_atual > 0 && brancas == 0) { linha_atual++; brancas = 1; }
}

static void segmento(const char *t, int n, int est, int alvo)
{
    if (!crescer((void **)&segs, &cap_segs, qtd_segs + 1, (int)sizeof(seg_t))) return;
    seg_t *s = &segs[qtd_segs++];
    s->linha = linha_atual;
    s->x = (int16_t)x_atual;
    s->n = (int16_t)n;
    s->estilo = (uint8_t)est;
    s->alvo = (int16_t)alvo;
    s->texto = guardar_texto(t, n);
    s->img = -1;
    s->w = s->h = 0;
    s->nlin = 1;
    linha_vazia = false;
    brancas = 0;
    if (linha_atual + 1 > linhas) linhas = linha_atual + 1;
}

/* Una palabra (sin espacios), con salto de línea automático. */
static void palavra(const char *t, int n)
{
    while (n > 0) {
        int esp = espaco_pendente && !linha_vazia ? LETRA_L : 0;
        int cabe = (LARG_TEXTO - x_atual - esp) / LETRA_L;
        if (n > cabe && !linha_vazia) { quebrar(); continue; }
        if (cabe < 1) cabe = 1;
        int k = n < cabe ? n : cabe;
        x_atual += esp;
        segmento(t, k, estilo, alvo_atual);
        x_atual += k * LETRA_L;
        espaco_pendente = false;
        t += k;
        n -= k;
        if (n) quebrar();
    }
}

static void texto_solto(const char *t)
{
    while (*t) {
        while (*t == ' ') { espaco_pendente = true; t++; }
        int n = 0;
        while (t[n] && t[n] != ' ') n++;
        if (n) palavra(t, n);
        t += n;
    }
}

static int novo_alvo(int tipo, const char *url, int form, const char *nome)
{
    if (qtd_alvos >= 32000 || !crescer((void **)&alvos, &cap_alvos, qtd_alvos + 1, (int)sizeof(alvo_t))) return -1;
    alvo_t *a = &alvos[qtd_alvos];
    a->tipo = tipo;
    a->url = url ? guardar_texto(url, (int)tam_texto(url)) : -1;
    a->form = form;
    a->nome = nome ? guardar_texto(nome, (int)tam_texto(nome)) : -1;
    return qtd_alvos++;
}

/* Valor de un atributo de la etiqueta ("href", "alt"...), ya sin entidades. */
static bool atributo(const char *a, int n, const char *nome, char *valor, int max)
{
    int t = (int)tam_texto(nome);
    for (int i = 0; i + t <= n; i++) {
        if (!igual_sem_caixa(a + i, nome, t) || (i > 0 && !eh_espaco(a[i - 1]))) continue;
        int k = i + t;
        while (k < n && eh_espaco(a[k])) k++;
        if (k >= n || a[k] != '=') continue;
        k++;
        while (k < n && eh_espaco(a[k])) k++;
        char aspas = (k < n && (a[k] == '"' || a[k] == '\'')) ? a[k++] : 0;
        int ini = k;
        while (k < n && (aspas ? a[k] != aspas : !eh_espaco(a[k]) && a[k] != '>')) k++;
        decodificar(a + ini, k - ini, valor, max);
        return true;
    }
    return false;
}

/* Etiquetas que empiezan un bloque nuevo (cortan la línea). */
static bool bloco(const char *t)
{
    static const char *const nomes[] = { "div", "tr", "table", "form", "center", "dl", "dt", "dd", "section",
                                         "article", "header", "footer", "nav", "address", "figure", "main",
                                         "aside", "caption", "fieldset", "tbody", "thead", "option", NULL };
    for (int i = 0; nomes[i]; i++) if (compara_texto(t, nomes[i]) == 0) return true;
    return false;
}

static bool paragrafo_tag(const char *t)
{
    return compara_texto(t, "p") == 0 || compara_texto(t, "blockquote") == 0 || compara_texto(t, "pre") == 0 ||
           compara_texto(t, "ul") == 0 || compara_texto(t, "ol") == 0 ||
           (t[0] == 'h' && t[1] >= '1' && t[1] <= '6' && !t[2]);
}

static void liberar_plano(void);

static int achar_imagem(const char *url)
{
    for (int i = 0; i < qtd_imgs; i++) if (compara_texto(imgs[i].url, url) == 0) return i;
    return -1;
}

/* Pone una imagen ya descargada como un bloque: escalada al ancho disponible
 * (nunca se agranda) y con un límite de altura; ocupa varias líneas de texto. */
static void colocar_imagem(int k)
{
    int disp = LARG_TEXTO - recuo;
    if (disp < 40) disp = 40;
    int w = imgs[k].img.larg, h = imgs[k].img.alt;
    if (w > disp) { h = h * disp / w; w = disp; }
    if (h > IMG_ALTURA_MAX) { w = w * IMG_ALTURA_MAX / h; h = IMG_ALTURA_MAX; }
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    quebrar();
    x_atual = recuo;
    segmento("", 0, E_IMAGEM | (alvo_atual >= 0 ? E_LINK : 0), alvo_atual);
    seg_t *s = &segs[qtd_segs - 1];
    s->img = (int16_t)k;
    s->w = (int16_t)w;
    s->h = (int16_t)h;
    s->nlin = (int16_t)((h + LINHA_A - 1) / LINHA_A);
    linha_atual += s->nlin - 1;
    if (linha_atual + 1 > linhas) linhas = linha_atual + 1;
    quebrar();
}

static int pilha_lista[8], niveis;   /* contador de cada lista abierta (0 = con viñeta) */
static int negrito_n, italico_n, pre_n, form_atual = -1;
static bool em_titulo;

static void ler_html(const char *h, int n)
{
    int pular_ate = -1;   /* dentro de <script> o <style>: todo hasta el cierre */
    char palavra_buf[256];
    int pk = 0;
#define SOLTAR() do { if (pk) { palavra_buf[pk] = 0; if (em_titulo) { junta_texto(titulo, palavra_buf, sizeof titulo); } else palavra(palavra_buf, pk); pk = 0; } } while (0)
    for (int i = 0; i < n;) {
        char c = h[i];
        if (c == '<') {
            if (i + 3 < n && h[i + 1] == '!' && h[i + 2] == '-' && h[i + 3] == '-') {   /* comentario */
                int k = i + 4;
                while (k + 2 < n && !(h[k] == '-' && h[k + 1] == '-' && h[k + 2] == '>')) k++;
                i = k + 3;
                continue;
            }
            int k = i + 1;
            bool fecha = k < n && h[k] == '/';
            if (fecha) k++;
            if (k >= n || (!letra(h[k]) && h[k] != '!')) {   /* "<" suelto en el texto */
                if (pular_ate < 0 && pk < (int)sizeof palavra_buf - 1) palavra_buf[pk++] = '<';
                i++;
                continue;
            }
            char nome[16];
            int nn = 0;
            while (k < n && (letra(h[k]) || (h[k] >= '0' && h[k] <= '9') || h[k] == '!') && nn < 15) nome[nn++] = minuscula(h[k++]);
            nome[nn] = 0;
            int ini_attr = k;
            char aspas = 0;
            while (k < n && (aspas || h[k] != '>')) {
                if (aspas && h[k] == aspas) aspas = 0;
                else if (!aspas && (h[k] == '"' || h[k] == '\'')) aspas = h[k];
                k++;
            }
            const char *attrs = h + ini_attr;
            int na = k - ini_attr;
            i = k + 1;
            if (pular_ate >= 0) {
                if (fecha && pular_ate == 1 && (compara_texto(nome, "script") == 0 || compara_texto(nome, "style") == 0 ||
                                                compara_texto(nome, "svg") == 0 || compara_texto(nome, "template") == 0))
                    pular_ate = -1;
                continue;
            }
            if (!fecha && (compara_texto(nome, "script") == 0 || compara_texto(nome, "style") == 0 ||
                           compara_texto(nome, "svg") == 0 || compara_texto(nome, "template") == 0)) {
                if (!(na > 0 && attrs[na - 1] == '/')) pular_ate = 1;
                continue;
            }
            /* Una etiqueta siempre separa palabras (menos las de estilo del texto). */
            bool de_estilo = compara_texto(nome, "b") == 0 || compara_texto(nome, "strong") == 0 ||
                             compara_texto(nome, "i") == 0 || compara_texto(nome, "em") == 0 ||
                             compara_texto(nome, "a") == 0 || compara_texto(nome, "span") == 0 ||
                             compara_texto(nome, "font") == 0 || compara_texto(nome, "small") == 0 ||
                             compara_texto(nome, "code") == 0 || compara_texto(nome, "u") == 0;
            if (!de_estilo) { SOLTAR(); espaco_pendente = true; }
            else SOLTAR();
            char v[URL_MAX];
            if (compara_texto(nome, "title") == 0) { em_titulo = !fecha; continue; }
            if (compara_texto(nome, "br") == 0) {
                if (linha_vazia) linha_atual++;
                quebrar();
                continue;
            }
            if (compara_texto(nome, "hr") == 0) { quebrar(); segmento("", 0, E_LINHA, -1); quebrar(); continue; }
            if (nome[0] == 'h' && nome[1] >= '1' && nome[1] <= '6' && !nome[2]) {
                paragrafo();
                if (fecha) estilo &= ~E_TITULO; else estilo |= E_TITULO;
                continue;
            }
            if (compara_texto(nome, "ul") == 0 || compara_texto(nome, "ol") == 0) {
                if (!fecha && niveis < 8) { pilha_lista[niveis++] = nome[0] == 'o' ? 1 : 0; recuo += 24; }
                else if (fecha && niveis > 0) { niveis--; recuo -= 24; }
                if (niveis == 0 || !fecha) paragrafo(); else quebrar();
                continue;
            }
            if (compara_texto(nome, "li") == 0) {
                quebrar();
                if (!fecha) {
                    char marca[8];
                    if (niveis && pilha_lista[niveis - 1]) formatar_texto(marca, sizeof marca, "%d.", pilha_lista[niveis - 1]++);
                    else { marca[0] = (char)0xB7; marca[1] = 0; }
                    int e = estilo;
                    estilo &= ~E_LINK;
                    x_atual = recuo - 16 > 0 ? recuo - 16 : 0;
                    segmento(marca, (int)tam_texto(marca), estilo, -1);
                    x_atual = recuo;
                    estilo = e;
                }
                continue;
            }
            if (compara_texto(nome, "b") == 0 || compara_texto(nome, "strong") == 0 || compara_texto(nome, "th") == 0) {
                negrito_n += fecha ? -1 : 1;
                if (negrito_n < 0) negrito_n = 0;
                if (negrito_n) estilo |= E_NEGRITO; else estilo &= ~E_NEGRITO;
                if (compara_texto(nome, "th") == 0) x_atual += fecha ? 0 : 16;
                continue;
            }
            if (compara_texto(nome, "i") == 0 || compara_texto(nome, "em") == 0) {
                italico_n += fecha ? -1 : 1;
                if (italico_n < 0) italico_n = 0;
                if (italico_n) estilo |= E_ITALICO; else estilo &= ~E_ITALICO;
                continue;
            }
            if (compara_texto(nome, "pre") == 0) {
                paragrafo();
                pre_n += fecha ? -1 : 1;
                if (pre_n < 0) pre_n = 0;
                if (pre_n) estilo |= E_PRE; else estilo &= ~E_PRE;
                continue;
            }
            if (compara_texto(nome, "a") == 0) {
                if (fecha) { alvo_atual = -1; estilo &= ~E_LINK; continue; }
                char url[URL_MAX];
                if (atributo(attrs, na, "href", v, sizeof v) && juntar_url(url_atual, v, url, sizeof url)) {
                    alvo_atual = novo_alvo(A_LINK, url, -1, NULL);
                    if (alvo_atual >= 0) estilo |= E_LINK;
                }
                continue;
            }
            if (compara_texto(nome, "td") == 0) { if (!fecha && !linha_vazia) x_atual += 16; continue; }
            if (compara_texto(nome, "img") == 0) {
                char src[URL_MAX], abs[URL_MAX];
                if (atributo(attrs, na, "src", src, sizeof src) && juntar_url(url_atual, src, abs, sizeof abs) &&
                    (comeca_com(abs, "http://") || comeca_com(abs, "https://"))) {
                    int k = achar_imagem(abs);
                    if (k < 0 && qtd_imgs + qtd_pendentes < IMG_MAX_QTD) {
                        bool ja = false;
                        for (int q = 0; q < qtd_pendentes; q++) if (compara_texto(pendentes[q], abs) == 0) ja = true;
                        if (!ja) copia_texto(pendentes[qtd_pendentes++], abs, URL_MAX);
                    }
                    if (k >= 0 && imgs[k].ok) { colocar_imagem(k); continue; }
                }
                if (atributo(attrs, na, "alt", v, 80) && v[0]) {
                    char t[96];
                    formatar_texto(t, sizeof t, "[%s]", v);
                    int e = estilo;
                    estilo |= E_ITALICO;
                    texto_solto(t);
                    estilo = e;
                } else if (alvo_atual >= 0) {
                    palavra("[imagen]", 8);
                }
                continue;
            }
            if (compara_texto(nome, "form") == 0) {
                quebrar();
                if (fecha) { form_atual = -1; continue; }
                form_atual = -1;
                char metodo[8] = "get";
                atributo(attrs, na, "method", metodo, sizeof metodo);
                if (qtd_forms < 8 && minuscula(metodo[0]) == 'g') {
                    char acao[URL_MAX];
                    if (!atributo(attrs, na, "action", v, sizeof v) || !v[0] || !juntar_url(url_atual, v, acao, sizeof acao))
                        copia_texto(acao, url_atual, sizeof acao);
                    char *q = acha_char(acao, '?');
                    if (q) *q = 0;
                    form_t *f = &forms[qtd_forms];
                    f->acao = guardar_texto(acao, (int)tam_texto(acao));
                    f->fixa[0] = 0;
                    f->campo = -1;
                    form_atual = qtd_forms++;
                }
                continue;
            }
            if (compara_texto(nome, "input") == 0 && !fecha) {
                char tipo[16] = "text", nm[64] = "", valor[64] = "";
                atributo(attrs, na, "type", tipo, sizeof tipo);
                atributo(attrs, na, "name", nm, sizeof nm);
                atributo(attrs, na, "value", valor, sizeof valor);
                for (char *p = tipo; *p; p++) *p = minuscula(*p);
                form_t *f = form_atual >= 0 ? &forms[form_atual] : NULL;
                if (compara_texto(tipo, "hidden") == 0) {
                    if (f && nm[0]) {
                        char par[140];
                        formatar_texto(par, sizeof par, "%s%s=%s", f->fixa[0] ? "&" : "", nm, valor);
                        junta_texto(f->fixa, par, sizeof f->fixa);
                    }
                } else if (compara_texto(tipo, "text") == 0 || compara_texto(tipo, "search") == 0) {
                    if (f && nm[0] && f->campo < 0) f->campo = guardar_texto(nm, (int)tam_texto(nm));
                    int a = f && nm[0] ? novo_alvo(A_CAMPO, NULL, form_atual, nm) : -1;
                    int e = estilo;
                    estilo = E_CAMPO;
                    int al = alvo_atual;
                    alvo_atual = a;
                    palavra(valor[0] ? valor : "______________________", valor[0] ? (int)tam_texto(valor) : 22);
                    alvo_atual = al;
                    estilo = e;
                } else if (compara_texto(tipo, "submit") == 0) {
                    char t[80];
                    formatar_texto(t, sizeof t, "[%s]", valor[0] ? valor : "Enviar");
                    char campo[64] = "";
                    if (f && f->campo >= 0) copia_texto(campo, textos + f->campo, sizeof campo);   /* textos puede cambiar de lugar */
                    int a = campo[0] ? novo_alvo(A_CAMPO, NULL, form_atual, campo) : -1;
                    int e = estilo, al = alvo_atual;
                    estilo = E_CAMPO;
                    alvo_atual = a;
                    palavra(t, (int)tam_texto(t));
                    alvo_atual = al;
                    estilo = e;
                }
                continue;
            }
            if (paragrafo_tag(nome)) { paragrafo(); continue; }
            if (bloco(nome)) { quebrar(); continue; }
            continue;
        }
        if (pular_ate >= 0) { i++; continue; }
        /* Texto. */
        if (c == '&') {
            char ch;
            int u = entidade(h + i, n - i, &ch);
            if (u) { if (pk < (int)sizeof palavra_buf - 1) palavra_buf[pk++] = ch; i += u; continue; }
        }
        if (pre_n && !em_titulo) {
            if (c == '\n') { SOLTAR(); linha_vazia = false; quebrar(); i++; continue; }
            if (c == '\r') { i++; continue; }
            if (c == '\t') c = ' ';
            if (c == ' ') { SOLTAR(); x_atual += LETRA_L; i++; continue; }
        } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            SOLTAR();
            if (em_titulo) { if (titulo[0]) junta_texto(titulo, " ", sizeof titulo); }
            else espaco_pendente = true;
            i++;
            continue;
        }
        if (pk < (int)sizeof palavra_buf - 1) palavra_buf[pk++] = c;
        else { SOLTAR(); palavra_buf[pk++] = c; }
        i++;
    }
    SOLTAR();
#undef SOLTAR
}

static void limpar_pagina(void)
{
    qtd_segs = qtd_alvos = qtd_textos = qtd_forms = 0;
    linhas = topo = 0;
    x_atual = linha_atual = recuo = estilo = 0;
    alvo_atual = form_atual = -1;
    linha_vazia = true;
    espaco_pendente = false;
    brancas = 1;
    niveis = negrito_n = italico_n = pre_n = 0;
    em_titulo = false;
    titulo[0] = 0;
    liberar_plano();
}

/* ------------------------------------------------------------ dibujo */

static void desenhar_rodape(void)
{
    g_retangulo(j, 0, ALT - RODAPE, LARG, RODAPE, 0xE6E9EC);
    g_retangulo(j, 0, ALT - RODAPE, LARG, 1, COR_BORDA);
    if (buscando) {
        caixa_busca.x = 64;
        caixa_busca.y = ALT - RODAPE + 2;
        caixa_busca.l = 240 < LARG - 80 ? 240 : LARG - 80;
        g_texto(j, 8, ALT - RODAPE + 6, "Buscar:", COR_TEXTO);
        caixa_desenhar(j, &caixa_busca);
        char t[96];
        if (!consulta[0]) copia_texto(t, "Intro/F3: siguiente, Esc: cerrar", sizeof t);
        else if (!coincidencias) copia_texto(t, "sin coincidencias", sizeof t);
        else formatar_texto(t, sizeof t, "%d coincidencia%s", coincidencias, coincidencias == 1 ? "" : "s");
        int x = caixa_busca.x + caixa_busca.l + 10;
        if (x < LARG) g_texto_n(j, x, ALT - RODAPE + 6, t, (LARG - x - 4) / LETRA_L, COR_TEXTO_FRACO);
    } else {
        g_texto_n(j, 8, ALT - RODAPE + 6, rodape_texto, (LARG - 16) / LETRA_L, COR_TEXTO_FRACO);
    }
    jan_mostrar_area(j, 0, ALT - RODAPE, LARG, RODAPE);
}

static void rodape(const char *t)
{
    if (compara_texto(t, rodape_texto) == 0) return;
    copia_texto(rodape_texto, t, sizeof rodape_texto);
    if (!buscando) desenhar_rodape();
}

static uint32_t cor_de(int e)
{
    if (e & E_CAMPO) return 0x3A4450;
    if (e & E_LINK) return 0x1A4FBF;
    if (e & E_TITULO) return 0x0B7A3E;
    if (e & E_PRE) return 0x5A2A82;
    if (e & E_ITALICO) return 0x4A5560;
    return COR_TEXTO;
}

/* ---- búsqueda en la página ---- */

static void liberar_plano(void)
{
    libera(plano);
    libera(plano_ini);
    plano = NULL;
    plano_ini = NULL;
    plano_tam = 0;
}

/* Une el texto de todos los segmentos (separados por un espacio) para buscar frases. */
static void montar_plano(void)
{
    liberar_plano();
    int total = 1;
    for (int i = 0; i < qtd_segs; i++) total += segs[i].n + 1;
    plano = aloca((size_t)total);
    plano_ini = aloca((size_t)(qtd_segs + 1) * sizeof(int));
    if (!plano || !plano_ini) { liberar_plano(); return; }
    int k = 0;
    for (int i = 0; i < qtd_segs; i++) {
        plano_ini[i] = k;
        if (segs[i].n) { memcpy(plano + k, textos + segs[i].texto, (size_t)segs[i].n); k += segs[i].n; }
        plano[k++] = ' ';
    }
    plano[k] = 0;
    plano_tam = k;
}

static bool coincide_em(int p, int ql)
{
    return p >= 0 && p + ql <= plano_tam && igual_sem_caixa_e_tilde(plano + p, consulta, ql);
}

static void contar_coincidencias(void)
{
    coincidencias = 0;
    int ql = (int)tam_texto(consulta);
    if (!plano || ql == 0) return;
    for (int p = 0; p + ql <= plano_tam; p++)
        if (coincide_em(p, ql)) { coincidencias++; p += ql - 1; }
}

/* Pinta de fondo las coincidencias que tocan el segmento i. */
static void destacar(int i, int x, int y)
{
    int ql = (int)tam_texto(consulta);
    if (!buscando || !plano || ql == 0 || !plano_ini) return;
    int o = plano_ini[i], n = segs[i].n;
    for (int p = o - ql + 1 > 0 ? o - ql + 1 : 0; p < o + n; p++) {
        if (!coincide_em(p, ql)) continue;
        int a = (p > o ? p : o) - o, b = (p + ql < o + n ? p + ql : o + n) - o;
        if (b > a) g_retangulo(j, x + a * LETRA_L, y, (b - a) * LETRA_L, LETRA_A, p == coincide_atual ? 0xFF9A3C : 0xFFE680);
    }
}

static int max_topo(void)
{
    int m = motor_altura > AREA_A ? (motor_altura - AREA_A + LINHA_A - 1) / LINHA_A : 0;
    return m;
}

static void desenhar_pagina(void)
{
    if (motor) {
        pintura_y = rol_px;
        pintura_pendente = true;
        g_retangulo(j, 0, ABA_Y, LARG - ROLAGEM, AREA_A, motor_fundo(motor));
    } else {
        pintura_pendente = false;
        g_retangulo(j, 0, ABA_Y, LARG - ROLAGEM, AREA_A, COR_BRANCO);
    }
    int bx = LARG - ROLAGEM;
    g_retangulo(j, bx, ABA_Y, ROLAGEM, AREA_A, 0xE6E9EC);
    g_retangulo(j, bx, ABA_Y, 1, AREA_A, COR_BORDA);
    if (motor_altura > AREA_A) {
        int a = AREA_A * AREA_A / motor_altura;
        if (a < 20) a = 20;
        int max = motor_altura - AREA_A;
        int y = ABA_Y + (AREA_A - a) * rol_px / max;
        g_retangulo(j, bx + 3, y + 2, ROLAGEM - 5, a - 4, rolando ? 0x5A636C : 0x8A939C);
    }
    jan_mostrar_area(j, 0, ABA_Y, LARG, AREA_A);
}

static void rolar(int para)
{
    int max = max_topo();
    if (para > max) para = max;
    if (para < 0) para = 0;
    topo = para;
    int novo = para * LINHA_A;
    int limite = motor_altura > AREA_A ? motor_altura - AREA_A : 0;
    if (novo > limite) novo = limite;
    if (novo < 0) novo = 0;
    if (novo == rol_px) return;
    rol_px = novo;
    registrar("cipo: rolagem para %d px (linha %d)\n", rol_px, topo);
    desenhar_pagina();
}

/* Altura y posición del pulgar de la barra de desplazamiento. */
static void pulgar(int *y, int *a)
{
    *a = motor_altura > 0 ? AREA_A * AREA_A / motor_altura : AREA_A;
    if (*a < 20) *a = 20;
    if (*a > AREA_A) *a = AREA_A;
    int max = motor_altura > AREA_A ? motor_altura - AREA_A : 0;
    *y = ABA_Y + (max ? (AREA_A - *a) * rol_px / max : 0);
}

/* Lleva el pulgar a que su centro quede en la fila y de la ventana. */
static void rolar_para_pulgar(int y)
{
    int py, a;
    pulgar(&py, &a);
    int livre = AREA_A - a;
    if (livre <= 0) return;
    int pos = y - rolando_dy - ABA_Y;
    if (pos < 0) pos = 0;
    if (pos > livre) pos = livre;
    int max = motor_altura > AREA_A ? motor_altura - AREA_A : 0;
    rol_px = (pos * max + livre / 2) / livre;
    topo = (rol_px + LINHA_A / 2) / LINHA_A;
    desenhar_pagina();
}

static void ajustar_barra(void)
{
    bt_ir.x = LARG - 50;
    endereco.x = 292;
    endereco.y = 8;
    endereco.l = LARG - 292 - 58;
}

static void desenhar_barra(void)
{
    g_retangulo(j, 0, 0, LARG, TOPO, 0xE6E9EC);
    g_retangulo(j, 0, TOPO - 1, LARG, 1, COR_BORDA);
    botao_desenhar(j, &bt_voltar);
    botao_desenhar(j, &bt_avancar);
    botao_desenhar(j, &bt_recarregar);
    botao_desenhar(j, &bt_inicio);
    botao_desenhar(j, &bt_ir);
    caixa_desenhar(j, &endereco);
    if (endereco_sel && endereco.foco) {   /* dirección "seleccionada" (Ctrl+L) */
        int n = (int)tam_texto(endereco.texto), cabem = (endereco.l - 10) / LETRA_L;
        if (n > cabem) n = cabem;
        g_retangulo(j, endereco.x + 4, endereco.y + 3, n * LETRA_L + 2, LETRA_A + 2, COR_SELECAO);
        g_texto_n(j, endereco.x + 5, endereco.y + 4, endereco.texto, n, COR_BRANCO);
    }
    jan_mostrar_area(j, 0, 0, LARG, TOPO);
}

/* ---- abas (D148) ---- */

static void carregar(const char *pedido);   /* usado por nova_aba() */
static void nova_aba_com_url(const char *url);
static void rolar_px(int px);              /* usado por carregar() */

/* Guarda o estado da aba atual: a página em memória e onde se estava a ler. */
static void aba_guardar(void)
{
    aba_t *a = &abas[aba_atual];
    copia_texto(a->url, url_atual, URL_MAX);
    copia_texto(a->titulo, titulo, 96);
    a->doc = doc; a->doc_tam = doc_tam; a->doc_html = doc_html;
    a->motor = motor; a->motor_altura = motor_altura;
    a->rol_px = rol_px; a->topo = topo;
    a->recursos_pendentes = recursos_pendentes;
    a->a_carregar_registrado = a_carregar_registrado;
    a->pintura_pendente = pintura_pendente; a->pintura_y = pintura_y;
}

/* Carrega o estado de uma aba nas variáveis do programa. */
static void aba_trocar(int i)
{
    aba_t *a = &abas[i];
    copia_texto(url_atual, a->url, URL_MAX);
    copia_texto(titulo, a->titulo, 96);
    doc = a->doc; doc_tam = a->doc_tam; doc_html = a->doc_html;
    motor = a->motor; motor_altura = a->motor_altura;
    rol_px = a->rol_px; topo = a->topo;
    recursos_pendentes = a->recursos_pendentes;
    a_carregar_registrado = a->a_carregar_registrado;
    pintura_pendente = a->pintura_pendente; pintura_y = a->pintura_y;
    aba_atual = i;
}

/* A barra de abas: uma pastilha por aba (com o título) e o botão de nova aba. */
static void desenhar_abas(void)
{
    g_retangulo(j, 0, TOPO, LARG, ABA_A, 0xE6E9EC);
    g_retangulo(j, 0, TOPO + ABA_A - 1, LARG, 1, COR_BORDA);
    int x = 4;
    for (int i = 0; i < qtd_abas; i++) {
        int l = 140;
        if (x + l > LARG - 40) break;
        bool ativa = (i == aba_atual);
        g_retangulo(j, x, TOPO + 3, l, ABA_A - 6, ativa ? 0xFFFFFF : 0xD3D8DD);
        const char *t = abas[i].titulo[0] ? abas[i].titulo : abas[i].url;
        g_texto_n(j, x + 6, TOPO + 6, t, (l - 10) / LETRA_L, ativa ? 0x1E2328 : 0x5A636C);
        x += l + 4;
    }
    g_retangulo(j, LARG - 34, TOPO + 3, 28, ABA_A - 6, 0xD3D8DD);
    g_texto(j, LARG - 26, TOPO + 6, "+", 0x1E2328);
    jan_mostrar_area(j, 0, TOPO, LARG, ABA_A);
}

/* Que aba está no ponto x da barra de abas (-1 = nenhuma). */
static int aba_no_clic(int x)
{
    int ini = 4;
    for (int i = 0; i < qtd_abas; i++) {
        int l = 140;
        if (ini + l > LARG - 40) break;
        if (x >= ini && x < ini + l) return i;
        ini += l + 4;
    }
    return -1;
}

static void aba_titulo_janela(void)
{
    char t[128];
    formatar_texto(t, sizeof t, "Cipó - %s", titulo);
    titulo_janela(j->id, t);
}

/* Troca de aba: a página fica em memória (não se volta a descarregar). */
static void trocar_aba(int i)
{
    if (i < 0 || i >= qtd_abas || i == aba_atual) return;
    aba_guardar();
    aba_trocar(i);
    recursos_pendentes = false;
    pintura_pendente = false;
    aba_titulo_janela();
    desenhar_barra();
    desenhar_abas();
    desenhar_pagina();
    desenhar_rodape();
    registrar("cipo: aba %d (%s)\n", i, url_atual);
}

static void nova_aba_com_url(const char *url)
{
    if (qtd_abas >= ABAS_MAX) { rodape("Já há muitas abas"); return; }
    aba_guardar();
    motor = NULL;            /* a aba anterior ficou com o seu motor */
    doc = NULL; doc_tam = 0;
    memset(&abas[qtd_abas], 0, sizeof abas[0]);
    aba_atual = qtd_abas++;
    bool link = url && url[0];
    carregar(link ? url : "sobre:inicio");
    desenhar_abas();
    if (link) registrar("cipo: link noutra aba %d (%s)\n", aba_atual, url);
    else registrar("cipo: nova aba %d\n", aba_atual);
}

static void nova_aba(void)
{
    nova_aba_com_url(NULL);
}

/* Abre um link numa aba nova (D152): a aba atual fica como estava e a nova
 * fica ativa, com a página do link carregada. */
static void abrir_link_nova_aba(const char *url)
{
    if (!url || !url[0]) return;
    char u[URL_MAX];
    copia_texto(u, url, sizeof u);
    registrar("cipo: abrir link noutra aba %s\n", u);
    nova_aba_com_url(u);
}

static void fechar_aba(void)
{
    if (qtd_abas <= 1) return;
    aba_guardar();
    aba_t *a = &abas[aba_atual];
    if (a->motor) motor_fim(a->motor);
    libera(a->doc);
    for (int i = aba_atual; i < qtd_abas - 1; i++) abas[i] = abas[i + 1];
    qtd_abas--;
    if (aba_atual >= qtd_abas) aba_atual = qtd_abas - 1;
    memset(&abas[qtd_abas], 0, sizeof abas[0]);
    aba_trocar(aba_atual);
    recursos_pendentes = false;
    pintura_pendente = false;
    aba_titulo_janela();
    desenhar_barra();
    desenhar_abas();
    desenhar_pagina();
    desenhar_rodape();
    registrar("cipo: aba fechada, %d restantes\n", qtd_abas);
}

static void desenhar_tudo(void)
{
    g_retangulo(j, 0, 0, LARG, ALT, COR_FUNDO);
    ajustar_barra();
    desenhar_barra();
    desenhar_abas();
    desenhar_pagina();
    desenhar_rodape();
    jan_mostrar(j);
}

/* ---- posição de leitura de cada sítio (D152) ---- */

/* Onde ficam os ficheiros do utilizador: /home se existir (a partição de
 * dados, que é persistente); /tmp se não existir (o ramdisk). */
static const char *pasta_usuario(void)
{
    info_arquivo_t inf;
    if (info_arquivo("/home", &inf) == 0 && inf.tipo == TIPO_PASTA) return "/home";
    return "/tmp";
}

/* O ficheiro das posições: na mesma pasta dos marcadores (/home/.config/cipo),
 * para que as coisas do Cipó vivam todas juntas. */
static void caminho_posicoes(char *saida, int max)
{
    char base[64];
    copia_texto(base, pasta_usuario(), sizeof base);
    char d[96];
    formatar_texto(d, sizeof d, "%s/.config", base);
    criar_pasta(d);
    formatar_texto(d, sizeof d, "%s/.config/cipo", base);
    criar_pasta(d);
    formatar_texto(saida, max, "%s/.config/cipo/posicoes", base);
}

/* A posição de leitura de um sítio, em píxeles da página (-1 = não está
 * guardada). */
static int posicao_procurar(const char *url)
{
    for (int i = 0; i < qtd_pos; i++)
        if (compara_texto(pos_url[i], url) == 0) return pos_px[i];
    return -1;
}

/* Guarda a posição de leitura de um sítio. O sítio mais recente fica no fim da
 * lista; quando a lista enche, o mais antigo sai (a lista não cresce sem
 * limite). */
static void posicao_guardar(const char *url, int px)
{
    if (!url || !url[0] || px < 0) return;
    for (int i = 0; i < qtd_pos; i++) {
        if (compara_texto(pos_url[i], url) != 0) continue;
        /* Já se conhecia: sai da lista e volta ao fim (o mais recente). */
        if (i < qtd_pos - 1) {
            memmove(pos_url[i], pos_url[i + 1], sizeof pos_url[0] * (size_t)(qtd_pos - 1 - i));
            memmove(pos_px + i, pos_px + i + 1, sizeof pos_px[0] * (size_t)(qtd_pos - 1 - i));
        }
        qtd_pos--;
        break;
    }
    if (qtd_pos == POS_MAX) {   /* a lista está cheia: sai o mais antigo */
        memmove(pos_url[0], pos_url[1], sizeof pos_url[0] * (POS_MAX - 1));
        memmove(pos_px, pos_px + 1, sizeof pos_px[0] * (POS_MAX - 1));
        qtd_pos--;
    }
    copia_texto(pos_url[qtd_pos], url, URL_MAX);
    pos_px[qtd_pos] = px;
    qtd_pos++;
}

/* Grava as posições no ficheiro: uma linha por sítio, "url<TAB>pixels". A
 * tabulação é o mesmo separador dos marcadores: uma URL não a traz codificada
 * (o espaço e os caracteres especiais vão em %XX). */
static void gravar_posicoes(void)
{
    char caminho[128];
    caminho_posicoes(caminho, sizeof caminho);
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    for (int i = 0; i < qtd_pos; i++) {
        char linha[URL_MAX + 32];
        int n = formatar_texto(linha, sizeof linha, "%s\t%d\n", pos_url[i], pos_px[i]);
        escrever_tudo(fd, linha, (unsigned)n);
    }
    fechar(fd);
}

/* Lê as posições do ficheiro. Se o ficheiro não existir ou estiver estragado,
 * o Cipó arranca na mesma (sem posições guardadas): uma linha que não tenha o
 * formato "url<TAB>pixels" é ignorada. */
static void ler_posicoes(void)
{
    qtd_pos = 0;
    char caminho[128];
    caminho_posicoes(caminho, sizeof caminho);
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return;
    char linha[URL_MAX + 32];
    while (qtd_pos < POS_MAX) {
        int n = ler_linha(fd, linha, sizeof linha);
        if (n < 0) break;
        if (n && linha[n - 1] == '\r') linha[n - 1] = 0;
        if (!n || linha[0] == '#') continue;
        /* A posição é o que vem depois da última tabulação; o URL é o resto. */
        char *sep = NULL;
        for (char *p = linha; *p; p++) if (*p == '\t') sep = p;
        if (!sep) continue;
        *sep = 0;
        int v = 0;
        bool ok = sep[1] != 0;
        for (char *p = sep + 1; ok && *p; p++) {
            if (*p < '0' || *p > '9') { ok = false; break; }
            v = v * 10 + (*p - '0');
            if (v > 1000000) { ok = false; break; }
        }
        if (!ok || !linha[0]) continue;
        copia_texto(pos_url[qtd_pos], linha, URL_MAX);
        pos_px[qtd_pos] = v;
        qtd_pos++;
    }
    fechar(fd);
}

/* ------------------------------------------------------------ carregar */

static const char pagina_inicio[] =
    "<title>Inicio</title><h1>Cipó, el navegador de SaguiOS</h1>"
    "<p>Escribe una dirección arriba y pulsa Intro, o elige un sitio de la lista. SaguiOS habla "
    "<b>HTTP</b> y <b>HTTPS</b> (TLS 1.2 y 1.3, con las raíces de <b>certificados</b>).</p>"
    "<h3>Sitios sencillos para probar</h3><ul>"
    "<li><a href=\"https://lite.duckduckgo.com/lite/\">lite.duckduckgo.com</a>: buscador en HTML sencillo (HTTPS)</li>"
    "<li><a href=\"http://info.cern.ch/\">info.cern.ch</a>: el primer sitio web de la historia (1991)</li>"
    "<li><a href=\"http://68k.news/\">68k.news</a>: noticias del día en HTML sencillo</li>"
    "<li><a href=\"http://neverssl.com/\">neverssl.com</a>: un sitio hecho para no usar nunca https</li>"
    "<li><a href=\"http://example.com/\">example.com</a>: la página de ejemplo de internet</li></ul>"
    "<h3>Trucos</h3><ul><li>Flechas, Re Pág, Av Pág, Inicio, Fin y la rueda del ratón desplazan la página; Espacio baja una pantalla. "
    "Arrastra la barra de la derecha.</li>"
    "<li>Alt+Izquierda / Alt+Derecha: atrás y adelante. F5: recargar. Ctrl+L: ir a la barra de dirección. "
    "Alt+Inicio: esta página.</li>"
    "<li>Ctrl+F busca en la página (F3 y Mayús+F3 saltan entre coincidencias, Esc cierra). "
    "La búsqueda no distingue tildes: \"cancion\" encuentra \"canción\". "
    "Tab y Mayús+Tab pasan de enlace en enlace e Intro lo abre.</li>"
    "<li>Ctrl+D guarda a página como marcador e Ctrl+B abre a lista de marcadores. "
    "Ctrl+C copia: el enlace que está bajo el ratón, o el texto de la página.</li>"
    "<li>Ctrl+T abre una pestaña nueva, Ctrl+W la cierra y Ctrl+Tab pasa a la siguiente. "
    "Cada pestaña guarda su página: al volver atrás no se vuelve a descargar.</li>"
    "<li>La ventana se puede agrandar o maximizar: el texto se vuelve a acomodar al ancho. Se muestran "
    "imágenes PNG, JPEG, BMP y PPM; las grandes se reducen para que quepan.</li>"
    "<li>Para guardar un archivo, usa <b>curl -o</b> o <b>wget</b> en el Terminal.</li></ul>";

/* ---- o motor de JavaScript da pagina (D149) ----
 *
 * As paginas correm o QuickJS (bellard/quickjs, MIT): cada <script> da
 * pagina e executado com um DOM minimo - document.title, document.body,
 * console, alert e setTimeout. O ponte esta em userland/web/quickjs/
 * (qjs_bridge.c); aqui so ha o que o browser faz com o que o script diz.
 */

static qjs_ponte_t *js;          /* o motor da pagina atual */
static int   js_geracao;         /* cada documento novo e um motor novo */
static int   js_geracao_rodada;  /* a geracao que os scripts ja correram */
static char  js_mensagem[160];   /* o que o ultimo alert() mostrou */

/* Uma pagina pedida para o fim: o script (location.href) ou a pagina
 * (meta refresh). A navegacao fica para o bucle principal, para nao
 * destruir o motor a meio de um script (D159). */
static char  url_pendente[URL_MAX];
static bool  tem_url_pendente;
static int   redirecoes;         /* quantos reenvios seguidos */

static void diagramar(void);
static void definir_doc(char *corpo, int tam, bool html);

/* Pede outra pagina: guarda o endereco (resolvido contra a pagina actual)
 * para o bucle principal navegar quando for seguro. */
static void pedir_pagina(const char *url)
{
    if (!url || !url[0]) return;
    char u[URL_MAX];
    if (url_juntar(url_atual, url, u, sizeof u)) copia_texto(url_pendente, u, sizeof url_pendente);
    else copia_texto(url_pendente, url, sizeof url_pendente);
    tem_url_pendente = true;
    registrar("cipo: pedido para ir a %s\n", url_pendente);
}

static void js_titulo(const char *t)
{
    registrar("cipo: js: titulo = %s\n", t);
    copia_texto(titulo, t, sizeof titulo);
    aba_titulo_janela();
    desenhar_abas();
    desenhar_barra();
}

static void js_alerta(const char *t)
{
    copia_texto(js_mensagem, t, sizeof js_mensagem);
    registrar("cipo: js: alerta: %s\n", t);
    desenhar_rodape();
}

static void js_log(const char *t)
{
    registrar("cipo: js: %s\n", t);
}

/* O script pediu outra pagina (location.href = ... ou location.replace(...)).
 * A navegacao nao se faz aqui: o script ainda esta a correr e o motor nao
 * pode ser destruido a meio. Guarda-se o pedido e o bucle principal trata
 * dele, quando o script ja acabou (D159). */
static void js_navegar(const char *url)
{
    if (!url || !url[0]) return;
    pedir_pagina(url);
    registrar("cipo: js: pedido para ir a %s\n", url_pendente);
}

/* O valor de um atributo dentro de uma etiqueta (<...>): copia para `saida`
 * (sem as aspas). Devolve false se o atributo nao estiver la. */
static bool atributo_valor(const char *tag, int tam, const char *nome,
                           char *saida, int max)
{
    int ln = (int)tam_texto(nome);
    for (int i = 0; i + ln < tam; i++) {
        /* o nome do atributo tem de vir depois de um espaco (ou da barra do
         * fim da etiqueta) */
        if (tag[i] != ' ' && tag[i] != '\t' && tag[i] != '\n' && tag[i] != '\r' && tag[i] != '/') continue;
        int j = i;
        while (j < tam && (tag[j] == ' ' || tag[j] == '\t' || tag[j] == '\n' || tag[j] == '\r')) j++;
        if (j + ln > tam) return false;
        if (!igual_sem_caixa_e_tilde(tag + j, nome, ln)) continue;
        int k = j + ln;
        while (k < tam && (tag[k] == ' ' || tag[k] == '\t')) k++;
        if (k >= tam || tag[k] != '=') continue;
        k++;
        while (k < tam && (tag[k] == ' ' || tag[k] == '\t')) k++;
        if (k >= tam) return false;
        char q = tag[k];
        int ini;
        if (q == '"' || q == '\'') {
            k++;
            ini = k;
            while (k < tam && tag[k] != q) k++;
        } else {
            ini = k;
            while (k < tam && tag[k] != ' ' && tag[k] != '\t' && tag[k] != '>' && tag[k] != '/') k++;
            copia_texto(saida, tag + ini, max);
            return true;
        }
        int n = k - ini;
        if (n > max - 1) n = max - 1;
        memcpy(saida, tag + ini, (size_t)n);
        saida[n] = 0;
        return true;
    }
    return false;
}

/* Um <meta http-equiv="refresh" content="0;url=..."> manda para outra
 * pagina. Só o reenvio imediato (0 segundos) conta: os outros esperariam, e
 * o SO não tem relógio para esperar. Devolve o endereço (ou NULL). */
static bool meta_refresh(const char *doc, int tam, char *url, int max)
{
    if (!doc) return false;
    for (int i = 0; i + 5 < tam; i++) {
        if (doc[i] != '<' || !igual_sem_caixa_e_tilde(doc + i + 1, "meta", 4)) continue;
        int fim = i + 5;
        while (fim < tam && doc[fim] != '>') fim++;
        if (fim >= tam) return false;
        char equiv[32], conteudo[256];
        if (!atributo_valor(doc + i, fim - i, "http-equiv", equiv, sizeof equiv)) continue;
        if (!igual_sem_caixa_e_tilde(equiv, "refresh", 7)) continue;
        if (!atributo_valor(doc + i, fim - i, "content", conteudo, sizeof conteudo)) continue;
        const char *c = conteudo;
        while (*c == ' ') c++;
        if (*c >= '0' && *c <= '9') {
            int atraso = 0;
            while (*c >= '0' && *c <= '9') { atraso = atraso * 10 + (*c - '0'); c++; }
            if (atraso > 0) return false;      /* não é imediato: ignora */
            while (*c == ' ' || *c == ';') c++;
        }
        while (*c == ' ') c++;
        if (igual_sem_caixa_e_tilde(c, "url=", 4)) c += 4;
        while (*c == ' ') c++;
        if (!*c) return false;
        copia_texto(url, c, max);
        return true;
    }
    return false;
}

/* O script trocou o corpo da pagina: monta um documento novo (o mesmo, com o
 * conteudo do <body> trocado) e volta a maquetar. */
static void js_trocar_corpo(const char *html);
/* Latin-1 -> UTF-8: o documento esta em Latin-1 (como o motor web) e o
 * QuickJS trabalha com UTF-8. */
static int latin1_para_utf8(const char *s, int n, char *o, int max)
{
    int k = 0;
    for (int i = 0; i < n && k + 3 < max; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) o[k++] = (char)c;
        else {
            o[k++] = (char)(0xC0 | (c >> 6));
            o[k++] = (char)(0x80 | (c & 0x3F));
        }
    }
    o[k] = 0;
    return k;
}

/* ---- el DOM de la página para el JavaScript (D160) ----
 *
 * El motor de la web (motor.c) tiene el árbol de la página; el puente del
 * QuickJS necesita verlo como una lista de nodos. Aquí se convierte el árbol
 * en un texto (UTF-8, con los campos separados por tabulador) que es lo que
 * el puente entiende (el formato está en qjs_bridge.h).
 */
#define CIPO_DOM_MAX   256      /* nodos que se mandan al puente */
#define CIPO_DOM_TEXTO 49152    /* el tamaño del texto del DOM */

static motor_dom_no_t dom_nos[CIPO_DOM_MAX];   /* la lista de nodos */
static char dom_texto[CIPO_DOM_TEXTO];          /* el texto del DOM */
static int  dom_texto_tam;

/* Añade texto al final del DOM (si cabe). */
static int dom_add(char *dst, int max, int k, const char *s)
{
    while (*s && k < max - 1) dst[k++] = *s++;
    dst[k] = 0;
    return k;
}

/* Añade un número al final del DOM. */
static int dom_num(char *dst, int max, int k, int v)
{
    char b[16];
    formatar_texto(b, sizeof b, "%d", v);
    return dom_add(dst, max, k, b);
}

/* Escribe un campo de texto (UTF-8, con los escapes: \\, \t, \n, \r). */
static int dom_campo(char *dst, int max, int k, const char *s)
{
    if (!s) s = "";
    for (const char *q = s; *q; q++) {
        char b[4];
        int n;
        unsigned char c = (unsigned char)*q;
        if (c == '\\') { b[0] = '\\'; b[1] = '\\'; n = 2; }
        else if (c == '\t') { b[0] = '\\'; b[1] = 't'; n = 2; }
        else if (c == '\n') { b[0] = '\\'; b[1] = 'n'; n = 2; }
        else if (c == '\r') { b[0] = '\\'; b[1] = 'r'; n = 2; }
        else n = latin1_para_utf8(q, 1, b, 4);
        if (k + n >= max) break;
        memcpy(dst + k, b, (size_t)n);
        k += n;
    }
    dst[k] = 0;
    return k;
}

/* Construye el texto del DOM de la página (a partir del motor). */
static void dom_construir(void)
{
    dom_texto_tam = 0;
    dom_texto[0] = 0;
    if (!motor) return;
    int n = motor_dom_lista(motor, dom_nos, CIPO_DOM_MAX);
    int k = 0;
    /* la cabecera: el <body> y el <html> */
    k = dom_add(dom_texto, CIPO_DOM_TEXTO, k, "D\t");
    k = dom_num(dom_texto, CIPO_DOM_TEXTO, k, motor_dom_corpo(motor));
    k = dom_add(dom_texto, CIPO_DOM_TEXTO, k, "\t");
    k = dom_num(dom_texto, CIPO_DOM_TEXTO, k, motor_dom_documento(motor));
    k = dom_add(dom_texto, CIPO_DOM_TEXTO, k, "\n");
    for (int i = 0; i < n; i++) {
        motor_dom_no_t *e = &dom_nos[i];
        if (k + 64 >= CIPO_DOM_TEXTO) break;
        k = dom_add(dom_texto, CIPO_DOM_TEXTO, k, "N\t");
        k = dom_num(dom_texto, CIPO_DOM_TEXTO, k, e->id);
        k = dom_add(dom_texto, CIPO_DOM_TEXTO, k, "\t");
        k = dom_num(dom_texto, CIPO_DOM_TEXTO, k, e->pai);
        k = dom_add(dom_texto, CIPO_DOM_TEXTO, k, "\t");
        k = dom_campo(dom_texto, CIPO_DOM_TEXTO, k, e->tag);
        k = dom_add(dom_texto, CIPO_DOM_TEXTO, k, "\t");
        k = dom_campo(dom_texto, CIPO_DOM_TEXTO, k, e->id_attr);
        k = dom_add(dom_texto, CIPO_DOM_TEXTO, k, "\t");
        k = dom_campo(dom_texto, CIPO_DOM_TEXTO, k, e->classe);
        k = dom_add(dom_texto, CIPO_DOM_TEXTO, k, "\t");
        k = dom_num(dom_texto, CIPO_DOM_TEXTO, k, e->n_atr);
        k = dom_add(dom_texto, CIPO_DOM_TEXTO, k, "\t");
        k = dom_campo(dom_texto, CIPO_DOM_TEXTO, k, e->texto);
        k = dom_add(dom_texto, CIPO_DOM_TEXTO, k, "\n");
        for (int a = 0; a < e->n_atr; a++) {
            if (k + 32 >= CIPO_DOM_TEXTO) break;
            k = dom_add(dom_texto, CIPO_DOM_TEXTO, k, "A\t");
            k = dom_campo(dom_texto, CIPO_DOM_TEXTO, k, e->atr_nome[a]);
            k = dom_add(dom_texto, CIPO_DOM_TEXTO, k, "\t");
            k = dom_campo(dom_texto, CIPO_DOM_TEXTO, k, e->atr_valor[a]);
            k = dom_add(dom_texto, CIPO_DOM_TEXTO, k, "\n");
        }
    }
    dom_texto_tam = k;
    registrar("cipo: js: el DOM tiene %d nodo(s)\n", n);
}

/* Corre os <script> da pagina. Cada pagina nova tem o seu motor: o script
 * de uma pagina nao pode ver as variaveis de outra. */
static void correr_scripts(void)
{
    if (!doc || !doc_html) return;
    if (js_geracao != js_geracao_rodada) {
        js_geracao_rodada = js_geracao;
        if (js) { qjs_destruir(js); js = NULL; }
        qjs_ganchos_t g = { js_titulo, js_trocar_corpo, js_alerta, js_log, js_navegar };
        js = qjs_criar(&g);
        if (!js) { registrar("cipo: js: sem memoria para o motor\n"); return; }
    }
    if (!js) return;
    qjs_definir_url(js, url_atual);
    qjs_definir_titulo(js, titulo);
    qjs_definir_corpo(js, "");
    qjs_tique(js, tempo_ms());     /* o relogio dos temporizadores */
    /* o DOM da pagina: o script ve a arvore dos elementos (D160) */
    dom_construir();
    if (dom_texto_tam > 0) qjs_definir_dom(js, dom_texto, dom_texto_tam);
    int i = 0, quantos = 0;
    while (i < doc_tam) {
        if (i + 7 <= doc_tam && doc[i] == '<' && igual_sem_caixa_e_tilde(doc + i + 1, "script", 6)) {
            int j = i + 7;
            while (j < doc_tam && doc[j] != '>') j++;
            if (j >= doc_tam) break;
            int k = j + 1;
            while (k + 8 <= doc_tam &&
                   !(doc[k] == '<' && doc[k + 1] == '/' && igual_sem_caixa_e_tilde(doc + k + 2, "script", 6))) k++;
            int fim = (k + 8 <= doc_tam) ? k : doc_tam;
            int n = fim - (j + 1);
            char *codigo = aloca((size_t)n * 2 + 2);
            if (codigo) {
                int m = latin1_para_utf8(doc + j + 1, n, codigo, n * 2 + 2);
                quantos++;
                if (qjs_executar(js, codigo, m, "<script>") != 0)
                    registrar("cipo: js: o script %d deu erro\n", quantos);
                libera(codigo);
            }
            i = (k + 8 <= doc_tam) ? k + 8 : doc_tam;
            continue;
        }
        i++;
    }
    if (quantos) registrar("cipo: js: %d script(s) correram\n", quantos);
    /* Os elementos com um tratador de clique: o motor tem de os saber para
     * que sejam alvo do rato (o alvo se cria no layout). Marca-se e volta a
     * maquetar a pagina (D160). */
    if (motor) {
        int ids[64];
        int n = qjs_clique_ids(js, ids, 64);
        for (int i = 0; i < n; i++) motor_marcar_clique(motor, ids[i]);
        if (n > 0) {
            motor_altura = motor_layout(motor, LARG - ROLAGEM, AREA_A);
            registrar("cipo: js: %d elemento(s) com tratador de clique\n", n);
        }
    }
}

/* O script trocou o corpo: novo documento e nova maquetacao. */
static void js_trocar_corpo(const char *html)
{
    if (!doc || !html) return;
    int ini = 0, fim = doc_tam;
    for (int i = 0; i + 5 < doc_tam; i++) {
        if (doc[i] == '<' && igual_sem_caixa_e_tilde(doc + i + 1, "body", 4)) {
            int j = i + 5;
            while (j < doc_tam && doc[j] != '>') j++;
            if (j < doc_tam) { ini = j + 1; break; }
        }
    }
    for (int i = doc_tam - 7; i >= ini; i--) {
        if (doc[i] == '<' && doc[i + 1] == '/' && igual_sem_caixa_e_tilde(doc + i + 2, "body", 4)) { fim = i; break; }
    }
    int nl = (int)tam_texto(html);
    int novo = ini + nl + (doc_tam - fim);
    char *d = aloca((size_t)novo + 1);
    if (!d) return;
    memcpy(d, doc, (size_t)ini);
    memcpy(d + ini, html, (size_t)nl);
    memcpy(d + ini + nl, doc + fim, (size_t)(doc_tam - fim));
    d[novo] = 0;
    definir_doc(d, novo, true);
    registrar("cipo: js: o corpo foi trocado (%d bytes)\n", novo);
    js_geracao_rodada = js_geracao;   /* os scripts ja correram: nao correm outra vez */
    diagramar();
    desenhar_tudo();
}

static int codigo_pagina;   /* código HTTP de la página actual (>= 400 = título "Error N") */

static uint8_t *motor_baixar(const char *url, unsigned *n);
static bool motor_imagem(const uint8_t *d, unsigned n, uint32_t **px, int *l, int *a);

/* Se queda con el documento (toma la propiedad del búfer). */
static void definir_doc(char *corpo, int tam, bool html)
{
    libera(doc);
    doc = corpo;
    doc_tam = tam;
    doc_html = html;
    js_geracao++;          /* documento novo: o motor do JavaScript tambem e novo */
}

static void definir_doc_texto(const char *h)
{
    int n = (int)tam_texto(h);
    char *c = aloca((size_t)n + 1);
    if (c) memcpy(c, h, (size_t)n + 1);
    definir_doc(c, c ? n : 0, true);
}

/* Maqueta el documento con el ancho actual de la ventana. */
static void diagramar(void)
{
    limpar_pagina();
    qtd_pendentes = 0;
    if (doc && doc_html) ler_html(doc, doc_tam);
    else if (doc) {
        estilo = E_PRE;
        pre_n = 1;
        for (int i = 0, ini = 0; i <= doc_tam; i++)
            if (i == doc_tam || doc[i] == '\n') {
                int k = i - ini;
                if (k && doc[i - 1] == '\r') k--;
                if (k) palavra(doc + ini, k);
                linha_vazia = false;
                quebrar();
                ini = i + 1;
            }
    }
    if (motor) motor_fim(motor);
    motor_ganchos_t g = { motor_baixar, motor_imagem, NULL };
    motor = motor_novo(&g);
    ms_base = tempo_ms();   /* o relógio das animações começa a zero nesta página */
    if (motor && doc) {
        motor_ler(motor, doc, doc_tam, url_atual);
        int nos = 0, cmds = 0, im = 0;
        motor_numeros(motor, &nos, &cmds, &im);
        if (nos > MOTOR_NOS_MAX) {
            registrar("cipo: pagina excede limite de nos (%d)\n", nos);
            motor_fim(motor);
            motor = NULL;
            motor_altura = 0;
        } else {
            motor_altura = motor_layout(motor, LARG - ROLAGEM, AREA_A);
            recursos_pendentes = true;
            a_carregar_registrado = false;
            copia_texto(titulo, motor_titulo(motor), sizeof titulo);
            if (motor_altura < 0) motor_altura = 0;
            motor_numeros(motor, &nos, &cmds, &im);
            if (motor_altura > MOTOR_ALTURA_MAX || cmds > MOTOR_CMDS_MAX) {
                registrar("cipo: layout excede limite (%d px, %d comandos)\n", motor_altura, cmds);
                motor_fim(motor);
                motor = NULL;
                motor_altura = 0;
                recursos_pendentes = false;
            }
        }
    }
    topo = 0;
    rol_px = 0;
    linhas = (motor_altura + LINHA_A - 1) / LINHA_A + 1;
    if (codigo_pagina >= 400 && !titulo[0]) formatar_texto(titulo, sizeof titulo, "Error %d", codigo_pagina);
    if (!titulo[0]) copia_texto(titulo, url_atual, sizeof titulo);
    if (foco_alvo >= qtd_alvos) foco_alvo = -1;
    if (buscando) { montar_plano(); contar_coincidencias(); }
    int nos = 0, cmds = 0, im = 0;
    if (motor) motor_numeros(motor, &nos, &cmds, &im);
    registrar("cipo: motor nos=%d cmds=%d imgs=%d altura=%d links=%d\n", nos, cmds, im, motor_altura, motor ? motor_links(motor) : 0);
    correr_scripts();      /* os <script> da pagina (D149) */
}

/* Vuelve a maquetar conservando la posición de lectura: el primer segmento visible
 * sigue siendo el primero visible aunque cambien las líneas. */
static void refazer_conservando(void)
{
    int idx = 0;
    while (idx < qtd_segs && segs[idx].linha + segs[idx].nlin <= topo) idx++;
    diagramar();
    int t = idx < qtd_segs ? segs[idx].linha : max_topo();
    topo = t > max_topo() ? max_topo() : t;
}

static void pagina_de_erro(const char *titulo_erro, const char *texto)
{
    char h[900];
    formatar_texto(h, sizeof h, "<title>%s</title><h2>%s</h2><p>%s</p><p><a href=\"sobre:inicio\">Volver al inicio</a></p>",
                   titulo_erro, titulo_erro, texto);
    definir_doc_texto(h);
    diagramar();
}

typedef struct {
    char *corpo;
    int   tam;
    int   codigo;
    bool  html;
    bool  truncado;
    const char *erro;
    char  detalhe[200];
} baixa_t;

/* Descarga la dirección (siguiendo redirecciones). "pagina" = es la página que se
 * muestra (actualiza barra, pie y url_atual); si no, es una imagen. */
static void baixar(char *url, bool pagina, int maximo, baixa_t *b)
{
    memset(b, 0, sizeof *b);
    b->html = true;
    static http_resposta_t r;
    for (int redirecoes = 0; redirecoes < 5; redirecoes++) {
        url_t u;
        if (!url_separar(url, &u)) { b->erro = "Dirección no válida"; return; }
        char st[160];
        formatar_texto(st, sizeof st, "%s %s%s...", pagina ? "Conectando con" : "Descargando imagen de", u.host, u.https ? " (HTTPS)" : "");
        rodape(st);
        /* HTTPS (D062): la misma lectura, por encima del TLS. Certificado
         * malo = página de error con el motivo; no hay "continuar de todos modos". */
        static tls_config_t cfg;
        char e2[160];
        if (u.https && !cfg.raizes && !tls_config_padrao(&cfg, NULL, e2, sizeof e2)) {
            b->erro = "No se pudo preparar HTTPS";
            copia_texto(b->detalhe, e2, sizeof b->detalhe);
            return;
        }
        static conexao_t con;
        int rc = conexao_abrir(&con, &u, &cfg);
        if (rc < 0) {
            b->erro = rc == TLS_ERRO_CERT ? "Conexión no segura" : "No se pudo abrir la página";
            formatar_texto(b->detalhe, sizeof b->detalhe, "%s: %s.", u.host, con.erro);
            conexao_fechar(&con);
            return;
        }
        char pedido[1024];
        int np = http_montar_pedido(&u, "GET", NULL, pedido, sizeof pedido);
        if (conexao_escrever(&con, pedido, (unsigned)np) < 0 || http_ler_cabecalho_de(&r, conexao_ler, &con) < 0) {
            conexao_fechar(&con);
            b->erro = "Respuesta no válida del servidor";
            return;
        }
        b->codigo = r.codigo;
        char destino[URL_MAX];
        if (b->codigo >= 300 && b->codigo < 400 && http_campo(r.cabecalho, "Location", destino, sizeof destino)) {
            conexao_fechar(&con);
            char nova[URL_MAX];
            if (!juntar_url(url, destino, nova, sizeof nova)) { b->erro = "Redirección no válida"; return; }
            copia_texto(url, nova, URL_MAX);
            if (pagina) {
                copia_texto(url_atual, url, sizeof url_atual);
                caixa_definir(&endereco, url);
                desenhar_barra();
            }
            continue;
        }
        char tipo[80] = "text/html";
        http_campo(r.cabecalho, "Content-Type", tipo, sizeof tipo);
        for (char *p = tipo; *p; p++) *p = minuscula(*p);
        b->html = comeca_com(tipo, "text/html") || comeca_com(tipo, "application/xhtml");
        if (pagina && !b->html && !comeca_com(tipo, "text/")) {
            conexao_fechar(&con);
            b->erro = "Tipo de archivo que Cipó no muestra";
            formatar_texto(b->detalhe, sizeof b->detalhe, "El servidor envió \"%s\". Para guardar el archivo, usa wget o curl -o en el Terminal.", tipo);
            return;
        }
        if (!pagina && b->codigo != 200) {
            conexao_fechar(&con);
            b->erro = "Imagen no disponible";
            return;
        }
        int cap = 65536;
        b->corpo = aloca((size_t)cap);
        if (!b->corpo) { conexao_fechar(&con); b->erro = "Sin memoria"; return; }
        int n, mostrado = 0;
        while ((n = http_ler_corpo(&r, b->corpo + b->tam, cap - b->tam)) > 0) {
            b->tam += n;
            if (b->tam == cap) {
                if (cap >= maximo) { b->truncado = true; break; }
                int nova = cap * 2;
                if (nova > maximo) nova = maximo;
                char *q = realoca(b->corpo, (size_t)nova);
                if (!q) { b->erro = "Sin memoria"; b->truncado = true; break; }
                b->corpo = q;
                cap = nova;
            }
            if (pagina && b->tam / 16384 != mostrado) {
                mostrado = b->tam / 16384;
                formatar_texto(st, sizeof st, "Cargando %s... %d KB", u.host, b->tam / 1024);
                rodape(st);
            }
        }
        if (n < 0) b->erro = "Error al leer la respuesta";
        conexao_fechar(&con);
        return;
    }
    b->erro = "Demasiadas redirecciones";
}

static uint8_t *motor_baixar(const char *url, unsigned *n)
{
    *n = 0;
    char u[URL_MAX];
    copia_texto(u, url, sizeof u);
    static baixa_t b;
    baixar(u, false, IMG_MAX_BYTES, &b);
    if (b.erro || b.truncado || !b.corpo || b.tam <= 0) { libera(b.corpo); return NULL; }
    *n = (unsigned)b.tam;
    return (uint8_t *)b.corpo;
}

/* Uma imagem maior demais se reduz (o mais perto, sem suavizar: a
 * interpolação custa mais e a diferença quase não se nota). Antes, uma foto
 * de 3 MB simplesmente não aparecia. */
static void imagem_para_caber(uint32_t **px, int *l, int *a, int max_pixels)
{
    if (!px || !*px || *l <= 0 || *a <= 0) return;
    if ((long)*l * *a <= max_pixels) return;
    int nl = *l, na = *a;
    while ((long)nl * na > max_pixels) {
        nl = nl * 3 / 4;
        na = na * 3 / 4;
        if (nl < 1) nl = 1;
        if (na < 1) na = 1;
    }
    uint32_t *novo = (uint32_t *)aloca((size_t)nl * (size_t)na * sizeof(uint32_t));
    if (!novo) return;
    for (int y = 0; y < na; y++)
        for (int x = 0; x < nl; x++) {
            int ox = x * *l / nl, oy = y * *a / na;
            novo[y * nl + x] = (*px)[oy * *l + ox];
        }
    libera(*px);
    *px = novo;
    *l = nl;
    *a = na;
    registrar("cipo: imagem reduzida para %dx%d\n", nl, na);
}

static bool motor_imagem(const uint8_t *d, unsigned n, uint32_t **px, int *l, int *a)
{
    if (n > IMG_MAX_BYTES) return false;
    if (!web_imagem_decodificar(d, n, px, l, a)) return false;
    if (*l <= 0 || *a <= 0) { libera(*px); *px = NULL; return false; }
    registrar("cipo: imagem %dx%d, primeiro pixel %08x\n", *l, *a, (*px)[0]);
    imagem_para_caber(px, l, a, IMG_MAX_PIXELS);
    return true;
}

static void liberar_imagens(void)
{
    for (int i = 0; i < qtd_imgs; i++) imagem_liberar(&imgs[i].img);
    qtd_imgs = 0;
    qtd_pendentes = 0;
    if (motor) { motor_fim(motor); motor = NULL; }
    motor_altura = rol_px = foco_campo = 0;
    recursos_pendentes = pintura_pendente = false;
}

/* Descarga un recurso del motor por vuelta del bucle de eventos. */
static void processar_recurso(void)
{
    if (pintura_pendente && motor) {
        int fim = rol_px + AREA_A;
        int faixa = fim - pintura_y < FAIXA_PINTURA ? fim - pintura_y : FAIXA_PINTURA;
        if (faixa > 0) {
            motor_pintar(motor, j->px + (ABA_Y + pintura_y - rol_px) * j->larg,
                         LARG - ROLAGEM, j->larg, pintura_y, faixa);
            motor_rolagem(motor, rol_px);
            pintura_y += faixa;
            jan_mostrar_area(j, 0, ABA_Y + pintura_y - rol_px - faixa, LARG - ROLAGEM, faixa);
        }
        if (pintura_y >= fim) {
            pintura_pendente = false;
            registrar("cipo: pagina pintada\n");
        }
        return;
    }
    if (!recursos_pendentes || !motor) return;
    rodape("Cargando recursos de la página...");
    if (!a_carregar_registrado) {
        a_carregar_registrado = true;
        registrar("cipo: a carregar recursos\n");
    }
    if (motor_imagens(motor, 1) > 0) {
        motor_altura = motor_layout(motor, LARG - ROLAGEM, AREA_A);
        if (motor_altura < 0) motor_altura = 0;
        int nos = 0, cmds = 0, im = 0;
        if (motor) motor_numeros(motor, &nos, &cmds, &im);
        if (motor_altura > MOTOR_ALTURA_MAX || cmds > MOTOR_CMDS_MAX || nos > MOTOR_NOS_MAX) {
            registrar("cipo: layout excede limite (%d px, %d nos, %d comandos)\n", motor_altura, nos, cmds);
            motor_fim(motor);
            motor = NULL;
            motor_altura = 0;
            recursos_pendentes = false;
        }
        registrar("cipo: recurso en trozo nos=%d cmds=%d imgs=%d\n", nos, cmds, im);
        desenhar_pagina();
        return;
    }
    recursos_pendentes = false;
    registrar("cipo: recursos listos\n");
    rodape("Listo");
    desenhar_pagina();
}

/* Descarga imágenes del renderizador antiguo; no participa en el motor web. */
static void buscar_imagens(void)
{
    if (!qtd_pendentes) return;
    char lista[IMG_MAX_QTD][URL_MAX];
    int n = qtd_pendentes;
    memcpy(lista, pendentes, sizeof lista);
    for (int i = 0; i < n && qtd_imgs < IMG_MAX_QTD; i++) {
        char st[96];
        formatar_texto(st, sizeof st, "Descargando imagen %d de %d...", i + 1, n);
        rodape(st);
        img_t *im = &imgs[qtd_imgs];
        memset(im, 0, sizeof *im);
        copia_texto(im->url, lista[i], sizeof im->url);
        char url[URL_MAX];
        copia_texto(url, lista[i], sizeof url);
        static baixa_t b;
        baixar(url, false, IMG_MAX_BYTES, &b);
        if (!b.erro && !b.truncado && b.tam > 0 && imagem_decodificar((const uint8_t *)b.corpo, b.tam, &im->img) == 0) {
            if ((long)im->img.larg * im->img.alt > IMG_MAX_PIXELS) imagem_liberar(&im->img);
            else im->ok = true;
        }
        if (im->ok) registrar("cipo: imagen %s %dx%d\n", lista[i], im->img.larg, im->img.alt);
        else registrar("cipo: imagen %s no se muestra\n", lista[i]);
        libera(b.corpo);
        qtd_imgs++;
    }
    refazer_conservando();
    rodape("Listo");
    desenhar_pagina();
}

/* Descarga la dirección (siguiendo redirecciones) y arma la página. */
static void carregar(const char *pedido)
{
    char url[URL_MAX];
    copia_texto(url, pedido, sizeof url);
    if (!comeca_com(url, "http://") && !comeca_com(url, "https://") && !comeca_com(url, "sobre:")) {
        char t[URL_MAX];
        formatar_texto(t, sizeof t, "http://%s", url);
        copia_texto(url, t, sizeof url);
    }
    /* Guarda a posição de leitura do sítio que se está a deixar (D152): ao
     * voltar a ele, a página aparece onde se ficou. */
    if (url_atual[0]) {
        posicao_guardar(url_atual, rol_px);
        gravar_posicoes();
        registrar("cipo: posicao guardada (%d px) em %s\n", rol_px, url_atual);
    }
    copia_texto(url_atual, url, sizeof url_atual);
    caixa_definir(&endereco, url);
    endereco.foco = false;
    endereco_sel = false;
    foco_alvo = -1;
    codigo_pagina = 0;
    liberar_imagens();
    desenhar_barra();
    registrar("cipo: abrindo %s\n", url);

    if (comeca_com(url, "sobre:")) {
        definir_doc_texto(pagina_inicio);
        diagramar();
    } else {
        static baixa_t b;
        baixar(url, true, PAGINA_MAX, &b);
        codigo_pagina = b.codigo;
        if (b.erro) {
            registrar("cipo: erro: %s %s\n", b.erro, b.detalhe);
            libera(b.corpo);
            codigo_pagina = 0;
            pagina_de_erro(b.erro, b.detalhe[0] ? b.detalhe : url);
        } else if (b.corpo) {
            b.tam = converter(b.corpo, b.tam);
            definir_doc(b.corpo, b.tam, b.html);   /* el documento se queda con el búfer */
            diagramar();
            /* Um <meta http-equiv="refresh"> manda para outra pagina (D159).
             * Se o script ja pediu uma, o pedido do script manda. */
            if (!tem_url_pendente) {
                char destino[URL_MAX];
                if (meta_refresh(doc, doc_tam, destino, sizeof destino)) {
                    registrar("cipo: meta refresh para %s\n", destino);
                    pedir_pagina(destino);
                }
            }
        } else {
            definir_doc(NULL, 0, true);
            diagramar();
        }
    }
    char t[128];
    formatar_texto(t, sizeof t, "Cipó - %s", titulo);
    titulo_janela(j->id, t);
    if (qtd_abas) {   /* o título da aba atual (D148) */
        copia_texto(abas[aba_atual].titulo, titulo, 96);
        desenhar_abas();
    }
    registrar("cipo: pronto \"%s\" (%d linhas, %d links)\n", titulo, linhas, motor ? motor_links(motor) : qtd_alvos);
    rodape(recursos_pendentes ? "Cargando recursos de la página..." : "Listo");
    /* Se a pagina nao pediu outra, a cadeia de reenvios acaba aqui (D159). */
    if (!tem_url_pendente) redirecoes = 0;
    /* Repõe a posição de leitura do sítio, se estiver guardada (D152). */
    int onde = posicao_procurar(url_atual);
    if (onde > 0) {
        rolar_px(onde);
        registrar("cipo: posicao reposta (%d px) em %s\n", onde, url_atual);
    }
    desenhar_pagina();
}

static void navegar(const char *url)
{
    if (url_atual[0] && compara_texto(url, url_atual) != 0) {
        if (qtd_historico == HIST_MAX) {
            memmove(historico[0], historico[1], sizeof historico[0] * (HIST_MAX - 1));
            memmove(historico_pos, historico_pos + 1, sizeof historico_pos[0] * (HIST_MAX - 1));
            qtd_historico--;
        }
        copia_texto(historico[qtd_historico], url_atual, URL_MAX);
        historico_pos[qtd_historico] = rol_px;   /* onde se estava a ler */
        qtd_historico++;
    }
    qtd_futuro = 0;
    char u[URL_MAX];
    copia_texto(u, url, sizeof u);
    carregar(u);
}

/* Se a página já está aberta noutra aba, troca para ela em vez de a voltar a
 * descarregar (a cache das abas). */
static int aba_com_url(const char *url)
{
    for (int i = 0; i < qtd_abas; i++)
        if (i != aba_atual && abas[i].url[0] && compara_texto(abas[i].url, url) == 0) return i;
    return -1;
}

/* Volta a uma posição de leitura (em píxeles da página). */
static void rolar_px(int px)
{
    int max = motor_altura > AREA_A ? motor_altura - AREA_A : 0;
    if (px > max) px = max;
    if (px < 0) px = 0;
    rol_px = px;
    topo = (rol_px + LINHA_A / 2) / LINHA_A;
    desenhar_pagina();
}

static void voltar(void)
{
    if (!qtd_historico) return;
    if (qtd_futuro == HIST_MAX) {
        memmove(futuro[0], futuro[1], sizeof futuro[0] * (HIST_MAX - 1));
        memmove(futuro_pos, futuro_pos + 1, sizeof futuro_pos[0] * (HIST_MAX - 1));
        qtd_futuro--;
    }
    copia_texto(futuro[qtd_futuro], url_atual, URL_MAX);
    futuro_pos[qtd_futuro] = rol_px;
    qtd_futuro++;
    char u[URL_MAX];
    copia_texto(u, historico[--qtd_historico], sizeof u);
    int onde = historico_pos[qtd_historico];
    int outra = aba_com_url(u);
    if (outra >= 0) { trocar_aba(outra); return; }
    carregar(u);
    if (onde > 0) rolar_px(onde);
    registrar("cipo: voltar para %s (pos %d)\n", u, onde);
}

static void avancar(void)
{
    if (!qtd_futuro) return;
    if (qtd_historico == HIST_MAX) {
        memmove(historico[0], historico[1], sizeof historico[0] * (HIST_MAX - 1));
        memmove(historico_pos, historico_pos + 1, sizeof historico_pos[0] * (HIST_MAX - 1));
        qtd_historico--;
    }
    copia_texto(historico[qtd_historico], url_atual, URL_MAX);
    historico_pos[qtd_historico] = rol_px;
    qtd_historico++;
    char u[URL_MAX];
    copia_texto(u, futuro[--qtd_futuro], sizeof u);
    int onde = futuro_pos[qtd_futuro];
    int outra = aba_com_url(u);
    if (outra >= 0) { trocar_aba(outra); return; }
    carregar(u);
    if (onde > 0) rolar_px(onde);
}

static void recarregar(void)
{
    char u[URL_MAX];
    copia_texto(u, url_atual, sizeof u);
    registrar("cipo: recargar\n");
    carregar(u);
}

/* Formulario de búsqueda: pide el texto y arma "accion?fijos&campo=texto". */
static void enviar_formulario(alvo_t *a)
{
    form_t *f = &forms[a->form];
    char texto[120] = "";
    if (!pedir_texto("Cipó", "Escribe el texto y pulsa Intro:", texto, sizeof texto)) return;
    char url[URL_MAX];
    int k = formatar_texto(url, sizeof url, "%s?%s%s%s=", textos + f->acao, f->fixa, f->fixa[0] ? "&" : "", textos + a->nome);
    static const char hexa[] = "0123456789ABCDEF";
    for (const char *p = texto; *p && k < (int)sizeof url - 4; p++) {
        uint8_t c = (uint8_t)*p;
        if (c == ' ') url[k++] = '+';
        else if (letra((char)c) || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') url[k++] = (char)c;
        else if (c >= 0x80) {   /* Latin-1 -> UTF-8, que es lo que esperan los sitios */
            uint8_t b[2] = { (uint8_t)(0xC0 | (c >> 6)), (uint8_t)(0x80 | (c & 0x3F)) };
            for (int i = 0; i < 2; i++) { url[k++] = '%'; url[k++] = hexa[b[i] >> 4]; url[k++] = hexa[b[i] & 15]; }
        } else { url[k++] = '%'; url[k++] = hexa[c >> 4]; url[k++] = hexa[c & 15]; }
    }
    url[k] = 0;
    navegar(url);
}

static void abrir_alvo(int a)
{
    if (a < 0 || a >= qtd_alvos) return;
    if (alvos[a].tipo == A_LINK) {
        char url[URL_MAX];
        copia_texto(url, textos + alvos[a].url, sizeof url);
        navegar(url);
    } else if (alvos[a].tipo == A_CAMPO) {
        enviar_formulario(&alvos[a]);
    }
}

static int alvo_em(int x, int y)
{
    const motor_alvo_t *a;
    if (!motor || y < ABA_Y || y >= ALT - RODAPE || x >= LARG - ROLAGEM) return -1;
    a = motor_alvo(motor, x, y - ABA_Y + rol_px);
    if (!a) return -1;
    return a->tipo;
}

static const motor_alvo_t *alvo_motor_em(int x, int y)
{
    if (!motor || y < ABA_Y || y >= ALT - RODAPE || x >= LARG - ROLAGEM) return NULL;
    return motor_alvo(motor, x, y - ABA_Y + rol_px);
}

static void nome_arquivo_url(const char *url, char *saida, int max)
{
    const char *p = url;
    const char *fim;
    int n = 0;
    while (*p && *p != '?' && *p != '#') p++;
    fim = p;
    while (p > url && p[-1] != '/') p--;
    while (p < fim && n < max - 1) {
        char c = *p++;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_') saida[n++] = c;
        else saida[n++] = '_';
    }
    saida[n] = 0;
    if (!n || compara_texto(saida, ".") == 0 || compara_texto(saida, "..") == 0)
        copia_texto(saida, "descarga", (size_t)max);
}

/* Pasta onde se guardam as descargas: /home se existe (parti��o de dados,
 * persistente); se n�o, /tmp, que sempre existe no ramdisk. */
static const char *pasta_descarga(void)
{
    info_arquivo_t inf;
    if (info_arquivo("/home", &inf) == 0 && inf.tipo == TIPO_PASTA) return "/home";
    return "/tmp";
}

static void guardar_recurso(const char *url)
{
    char u[URL_MAX], caminho[128];
    copia_texto(u, url, sizeof u);
    static baixa_t b;
    baixar(u, false, IMG_MAX_BYTES, &b);
    if (b.erro || b.truncado || !b.corpo || b.tam <= 0) {
        rodape("No se pudo descargar el recurso");
        libera(b.corpo);
        registrar("cipo: guardar error de descarga\n");
        return;
    }
    char nome[96];
    nome_arquivo_url(url, nome, sizeof nome);
    formatar_texto(caminho, sizeof caminho, "%s/%s", pasta_descarga(), nome);
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    int r = fd < 0 ? fd : escrever_tudo(fd, b.corpo, (unsigned)b.tam);
    if (fd >= 0) fechar(fd);
    libera(b.corpo);
    if (r != b.tam) {
        rodape("No se pudo guardar el archivo");
        registrar("cipo: guardar error de escritura %s (%d)\n", caminho, r);
        return;
    }
    formatar_texto(rodape_texto, sizeof rodape_texto, "Guardado en %s", caminho);
    desenhar_rodape();
    registrar("cipo: guardado %s (%d bytes)\n", caminho, r);
}

static void abrir_alvo_motor(const motor_alvo_t *a)
{
    if (!a) return;
    /* Un manejador de JavaScript puede atender el clic (D160). Si el script
     * cancela el evento (preventDefault), no se navega. */
    if (a->dom >= 0 && js) {
        if (qjs_clique(js, a->dom)) {
            registrar("cipo: js: el clic lo cancelo el script\n");
            return;
        }
    }
    if (a->tipo == MA_LINK && a->href && a->href[0]) navegar(a->href);
    else if ((a->tipo == MA_CAMPO || a->tipo == MA_BOTAO) && a->envia) {
        char url[URL_MAX];
        if (motor_envio(motor, a, url, sizeof url)) navegar(url);
    } else if (a->tipo == MA_CAMPO) {
        motor_foco(motor, a);
        foco_campo = motor_foco_campo(motor);
        desenhar_pagina();
    }
}

static void tecla_campo_motor(unsigned t)
{
    if (!motor || !motor_tem_foco(motor)) return;
    if (t == TECLA_ESC || t == 27) { motor_foco(motor, NULL); foco_campo = -1; desenhar_pagina(); return; }
    if (t == '\n' || t == '\r') {
        char url[URL_MAX];
        if (motor_envio(motor, NULL, url, sizeof url)) navegar(url);
        return;
    }
    char v[1024];
    copia_texto(v, motor_foco_valor(motor), sizeof v);
    int n = (int)tam_texto(v);
    if (t == '\b') {
        if (!n) return;
        do n--; while (n > 0 && ((uint8_t)v[n] & 0xC0) == 0x80);
        v[n] = 0;
    } else if (t >= 32 && t < 256 && n < (int)sizeof v - 3) {
        if (t < 0x80) v[n++] = (char)t;
        else { v[n++] = (char)(0xC0 | (t >> 6)); v[n++] = (char)(0x80 | (t & 0x3F)); }
        v[n] = 0;
    } else return;
    motor_foco_definir(motor, v);
    motor_altura = motor_layout(motor, LARG - ROLAGEM, AREA_A);
    desenhar_pagina();
}

/* Avança o relógio do motor (para as animações). true = alguma animação mudou
 * e a página precisa de ser repintada. */
static bool animacoes_tick(void)
{
    if (!motor) return false;
    return motor_tick(motor, tempo_ms() - ms_base);
}

/* ---- marcadores e cópia (D148) ---- */

/* Onde ficam os marcadores: /home se existir (a partição de dados, que é
 * persistente); /tmp se não existir (o ramdisk). */
static void caminho_marcadores(char *saida, int max)
{
    char base[64];
    copia_texto(base, pasta_usuario(), sizeof base);
    char d[96];
    formatar_texto(d, sizeof d, "%s/.config", base);
    criar_pasta(d);
    formatar_texto(d, sizeof d, "%s/.config/cipo", base);
    criar_pasta(d);
    formatar_texto(saida, max, "%s/.config/cipo/marcadores", base);
}

/* Lê os marcadores: uma linha por marcador, "url<TAB>título". */
static void ler_marcadores(void)
{
    qtd_marcadores = 0;
    char caminho[128];
    caminho_marcadores(caminho, sizeof caminho);
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return;
    char linha[URL_MAX + 96];
    while (qtd_marcadores < MARC_MAX) {
        int n = ler_linha(fd, linha, sizeof linha);
        if (n < 0) break;
        if (!n || linha[0] == '#') continue;
        char *sep = NULL;
        for (char *p = linha; *p; p++) if (*p == '\t') { sep = p; break; }
        if (sep) *sep = 0;
        copia_texto(marcador_url[qtd_marcadores], linha, URL_MAX);
        if (sep) copia_texto(marcador_titulo[qtd_marcadores], sep + 1, 96);
        else marcador_titulo[qtd_marcadores][0] = 0;
        qtd_marcadores++;
    }
    fechar(fd);
}

static void gravar_marcadores(void)
{
    char caminho[128];
    caminho_marcadores(caminho, sizeof caminho);
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    for (int i = 0; i < qtd_marcadores; i++) {
        char linha[URL_MAX + 96];
        int n = formatar_texto(linha, sizeof linha, "%s\t%s\n", marcador_url[i], marcador_titulo[i]);
        escrever_tudo(fd, linha, (unsigned)n);
    }
    fechar(fd);
}

/* Ctrl+D: guarda a página atual. */
static void adicionar_marcador(void)
{
    if (!url_atual[0]) return;
    for (int i = 0; i < qtd_marcadores; i++)
        if (compara_texto(marcador_url[i], url_atual) == 0) {
            rodape("Já está nos marcadores");
            registrar("cipo: marcador ja existe %s\n", url_atual);
            return;
        }
    if (qtd_marcadores == MARC_MAX) {
        rodape("Já há muitos marcadores");
        return;
    }
    copia_texto(marcador_url[qtd_marcadores], url_atual, URL_MAX);
    copia_texto(marcador_titulo[qtd_marcadores], titulo, 96);
    qtd_marcadores++;
    gravar_marcadores();
    char t[128];
    formatar_texto(t, sizeof t, "Marcador guardado (%d)", qtd_marcadores);
    rodape(t);
    registrar("cipo: marcador %d %s (%s)\n", qtd_marcadores, url_atual, titulo);
}

/* Ctrl+B: a lista de marcadores, como menu (a escolha chega como EV_MENU). */
static void mostrar_marcadores(void)
{
    ler_marcadores();
    if (!qtd_marcadores) {
        rodape("Não há marcadores (Ctrl+D guarda um)");
        registrar("cipo: sem marcadores\n");
        return;
    }
    char itens[MARC_MAX * (URL_MAX + 96)];
    int k = 0;
    for (int i = 0; i < qtd_marcadores; i++) {
        const char *t = marcador_titulo[i][0] ? marcador_titulo[i] : marcador_url[i];
        k += formatar_texto(itens + k, (int)sizeof itens - k, "%s\t%s\n", t, marcador_url[i]);
        if (k >= (int)sizeof itens - 2) break;
    }
    itens[k > 0 ? k - 1 : 0] = 0;
    menu_contextual(j->id, mouse_x > 0 ? mouse_x : 8, ALT - RODAPE - 10, itens, COD_MENU_MARCADORES);
    registrar("cipo: menu de marcadores (%d)\n", qtd_marcadores);
}

/* Ctrl+C: copia para a área de transferência do sistema (D074). O link que
 * está por baixo do rato, o texto do campo que tem o foco, ou o texto todo
 * da página (neste caso, o que se vê). */
static void copiar_agora(void)
{
    if (endereco.foco) {
        int n = copiar_texto(endereco.texto, (unsigned)tam_texto(endereco.texto));
        registrar("cipo: copiado o endereco (%d bytes)\n", n);
        rodape("Endereço copiado");
        return;
    }
    const motor_alvo_t *a = alvo_motor_em(mouse_x, mouse_y);
    if (a && a->tipo == MA_LINK && a->href) {
        int n = copiar_texto(a->href, (unsigned)tam_texto(a->href));
        registrar("cipo: copiado o link %s (%d bytes)\n", a->href, n);
        rodape("Link copiado");
        return;
    }
    if (motor && motor_tem_foco(motor)) {
        const char *v = motor_foco_valor(motor);
        int n = copiar_texto(v, (unsigned)tam_texto(v));
        registrar("cipo: copiado o campo (%d bytes)\n", n);
        rodape("Texto copiado");
        return;
    }
    montar_plano();
    if (plano && plano_tam) {
        int n = copiar_texto(plano, (unsigned)plano_tam);
        registrar("cipo: copiado o texto da pagina (%d bytes)\n", n);
        rodape("Texto da página copiado");
    }
}

/* ---- teclado: foco entre enlaces ---- */

/* Desplaza la página para que el destino (enlace) quede a la vista. */
static void mostrar_segmento(int i)
{
    if (i < 0 || i >= qtd_segs) return;
    seg_t *s = &segs[i];
    if (s->linha >= topo && s->linha + s->nlin <= topo + VISIVEIS) return;
    int t = s->linha - VISIVEIS / 3;
    topo = t < 0 ? 0 : t > max_topo() ? max_topo() : t;
    rol_px = topo * LINHA_A;
}

static void mover_foco(int sentido)
{
    if (!qtd_alvos) return;
    int n;
    if (foco_alvo < 0) {
        /* primer enlace visible hacia adelante (o último hacia atrás) */
        n = -1;
        for (int i = 0; i < qtd_segs; i++) {
            if (segs[i].alvo < 0) continue;
            if (sentido > 0 && segs[i].linha >= topo) { n = segs[i].alvo; break; }
            if (sentido < 0 && segs[i].linha < topo + VISIVEIS) n = segs[i].alvo;
        }
        if (n < 0) n = sentido > 0 ? 0 : qtd_alvos - 1;
    } else {
        n = (foco_alvo + sentido + qtd_alvos) % qtd_alvos;
    }
    foco_alvo = n;
    for (int i = 0; i < qtd_segs; i++) if (segs[i].alvo == n) { mostrar_segmento(i); break; }
    desenhar_pagina();
    registrar("cipo: foco en el enlace %d\n", n);
    if (alvos[n].tipo == A_LINK) rodape(textos + alvos[n].url);
    else rodape("Formulario: Intro para escribir");
}

/* ---- buscar en la página ---- */

/* Salta a la coincidencia siguiente (sentido 1) o anterior (-1) y la deja a la vista. */
static void buscar_proxima(int sentido)
{
    int ql = (int)tam_texto(consulta);
    if (!plano || ql == 0 || !coincidencias) return;
    int p;
    if (coincide_atual < 0) {
        int i = 0;
        while (i < qtd_segs && segs[i].linha < topo) i++;
        p = i < qtd_segs ? plano_ini[i] : 0;
        if (sentido < 0) p--;
        else p--;   /* la primera búsqueda empieza en el primer segmento visible */
    } else p = coincide_atual;
    int achou = -1;
    for (int k = 1; k <= plano_tam; k++) {
        int q = ((p + sentido * k) % plano_tam + plano_tam) % plano_tam;
        if (coincide_em(q, ql)) { achou = q; break; }
    }
    if (achou < 0) return;
    coincide_atual = achou;
    int ord = 0;
    for (int q = 0; q <= achou; q++) if (coincide_em(q, ql)) { ord++; q += ql - 1; }
    int si = 0;
    for (int i = 0; i < qtd_segs; i++) if (plano_ini[i] <= achou) si = i; else break;
    mostrar_segmento(si);
    registrar("cipo: busca \"%s\": %d de %d (linha %d)\n", consulta, ord, coincidencias, segs[si].linha);
    desenhar_pagina();
}

static void abrir_busca(void)
{
    if (!buscando) {
        buscando = true;
        caixa_busca = (caixa_t){ .x = 64, .y = 0, .l = 240 };
        consulta[0] = 0;
        coincide_atual = -1;
        coincidencias = 0;
        montar_plano();
    }
    caixa_busca.foco = true;
    endereco.foco = false;
    endereco_sel = false;
    desenhar_barra();
    desenhar_tudo();
}

static void fechar_busca(void)
{
    if (!buscando) return;
    buscando = false;
    coincide_atual = -1;
    liberar_plano();
    desenhar_tudo();
}

static void atualizar_busca(void)
{
    if (compara_texto(caixa_busca.texto, consulta) == 0) return;
    copia_texto(consulta, caixa_busca.texto, sizeof consulta);
    coincide_atual = -1;
    contar_coincidencias();
    if (coincidencias) buscar_proxima(1);
    else desenhar_pagina();
    desenhar_rodape();
}

/* La ventana cambió de tamaño (arrastrar el borde, maximizar, restaurar). */
static void novo_tamanho(void)
{
    if (motor) {
        motor_altura = motor_layout(motor, LARG - ROLAGEM, AREA_A);
        if (motor_altura < 0) motor_altura = 0;
        linhas = (motor_altura + LINHA_A - 1) / LINHA_A + 1;
        int max = motor_altura > AREA_A ? motor_altura - AREA_A : 0;
        if (rol_px > max) rol_px = max;
        topo = (rol_px + LINHA_A / 2) / LINHA_A;
    }
    registrar("cipo: ventana %dx%d, texto de %d px de ancho, %d lineas\n", LARG, ALT, LARG_TEXTO, linhas);
    desenhar_tudo();
}

static void ir_para_endereco(void)
{
    if (buscando) { caixa_busca.foco = false; desenhar_rodape(); }
    endereco.foco = true;
    endereco.cursor = (int)tam_texto(endereco.texto);
    endereco_sel = true;
    desenhar_barra();
}

/* Atajos que valen siempre. true = tratado. */
static bool atalho(const evento_t *e)
{
    unsigned t = e->tecla, m = e->modificadores;
    bool a_escrever = (buscando && caixa_busca.foco) || endereco.foco;
    if ((m & 4) && t == TECLA_ESQUERDA) { voltar(); return true; }
    if ((m & 4) && t == TECLA_DIREITA) { avancar(); return true; }
    if ((m & 4) && t == TECLA_HOME) { navegar("sobre:inicio"); return true; }
    if (t == TECLA_F5_CIPO || t == 18) { recarregar(); return true; }          /* F5, Ctrl+R */
    if (t == 12) { ir_para_endereco(); return true; }                          /* Ctrl+L */
    if (t == 6) { abrir_busca(); return true; }                                /* Ctrl+F */
    if (t == 3) { copiar_agora(); return true; }                                /* Ctrl+C */
    if (t == 20) { nova_aba(); return true; }                                   /* Ctrl+T */
    if (t == 23) { fechar_aba(); return true; }                                 /* Ctrl+W */
    if (t == 9 && (m & 2)) {                                                    /* Ctrl+Tab */
        trocar_aba((m & 1) ? (aba_atual + qtd_abas - 1) % qtd_abas : (aba_atual + 1) % qtd_abas);
        return true;
    }
    if (!a_escrever) {
        if (t == 4) { adicionar_marcador(); return true; }                      /* Ctrl+D */
        if (t == 2) { mostrar_marcadores(); return true; }                      /* Ctrl+B */
    }
    if (t == TECLA_F3_CIPO) {
        if (!buscando) abrir_busca();
        else buscar_proxima((m & 1) ? -1 : 1);
        return true;
    }
    return false;
}

int principal(int argc, char **argv)
{
    j = jan_abrir("Cipó", LARG_INICIO, ALT_INICIO);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL, LARG_MIN, ALT_MIN);
    endereco = (caixa_t){ .x = 292, .y = 8, .l = LARG - 292 - 58 };
    copia_texto(rodape_texto, "?", sizeof rodape_texto);
    ajustar_barra();
    desenhar_barra();
    rodape("Listo");
    ler_marcadores();
    ler_posicoes();
    qtd_abas = 1;
    aba_atual = 0;
    carregar(argc > 1 ? argv[1] : "sobre:inicio");
    aba_guardar();          /* a primeira aba fica com a página */
    desenhar_abas();
    jan_mostrar(j);

    evento_t e;
    for (;;) {
        /* O script pediu outra pagina (D159): navega-se aqui, fora do script,
         * para o motor poder ser destruido com calma (e nao a meio de um
         * script). E o que faz a pagina de reenvio do DuckDuckGo funcionar. */
        if (tem_url_pendente) {
            char u[URL_MAX];
            copia_texto(u, url_pendente, sizeof u);
            url_pendente[0] = 0;
            tem_url_pendente = false;
            if (++redirecoes > 8) {
                registrar("cipo: demasiadas reenvios seguidos; para\n");
                redirecoes = 0;
            } else {
                registrar("cipo: navega para %s\n", u);
                navegar(u);
                continue;
            }
        }
        /* As animações da página andam com o relógio: se alguma mudou, repinta-se
         * a banda visível (sem recalcular o layout). */
        if (animacoes_tick()) { desenhar_pagina(); continue; }
        /* Os temporizadores do script (setTimeout): correm com o relogio do SO. */
        if (js && qjs_tique(js, tempo_ms()) > 0) desenhar_pagina();
        if (proximo_evento(j->id, &e, false) != 1) {
            if (pintura_pendente || recursos_pendentes) { processar_recurso(); continue; }
            if (esperar_evento(j->id, &e, 15) != 1) continue;
        }
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_MENU && e.tecla == COD_MENU_CONTEXTUAL) {
            /* O menu do botão direito (D152): cada linha tem a sua acção. */
            if (e.x >= 0 && e.x < qtd_menu_acoes) {
                int acao = menu_acoes[e.x];
                if (acao == ACAO_GUARDAR_IMAGEM) guardar_recurso(salvar_imagem_url);
                else if (acao == ACAO_GUARDAR_LINK) guardar_recurso(salvar_link_url);
                else if (acao == ACAO_ABA_NOVA) abrir_link_nova_aba(salvar_link_url);
            }
            qtd_menu_acoes = 0;
            salvar_imagem_url[0] = salvar_link_url[0] = 0;
            continue;
        }
        if (e.tipo == EV_MENU && e.tecla == COD_MENU_MARCADORES) {
            if (e.x >= 0 && e.x < qtd_marcadores) {
                char u[URL_MAX];
                copia_texto(u, marcador_url[e.x], sizeof u);
                registrar("cipo: marcador escolhido %s\n", u);
                navegar(u);
            }
            continue;
        }
        if (jan_redimensionar(j, &e)) { novo_tamanho(); continue; }
        if (e.tipo == EV_RODA) {
            int n = (int)e.cliques;
            if (e.y >= ABA_Y && e.y < ALT - RODAPE) {
                rolar(topo + n * RODA_LINHAS);
                registrar("cipo: rueda %d, linea %d de %d\n", n, topo, linhas);
            }
            continue;
        }
        if (e.tipo == EV_SOLTOU && rolando) { rolando = false; desenhar_pagina(); continue; }
        if (e.tipo == EV_TECLA) {
            unsigned t = e.tecla;
            if (atalho(&e)) continue;
            if (buscando && caixa_busca.foco) {
                if (t == TECLA_ESC || t == 27) { fechar_busca(); continue; }
                if (t == '\n') { buscar_proxima((e.modificadores & 1) ? -1 : 1); continue; }
                if (t == TECLA_CIMA || t == TECLA_BAIXO || t == TECLA_PGUP || t == TECLA_PGDN) { /* rueda de la página */ }
                else { caixa_tratar(j, &caixa_busca, &e); atualizar_busca(); continue; }
            }
            if (motor && motor_tem_foco(motor)) { tecla_campo_motor(t); continue; }
            if (endereco.foco) {
                if (t == '\t' || t == TECLA_ESC || t == 27) { endereco.foco = false; endereco_sel = false; desenhar_barra(); continue; }
                if (endereco_sel) {
                    bool substitui = (t >= 0x20 && t < 0x100 && t != 0x7F) || t == '\b' || t == TECLA_DELETE || t == 22;
                    endereco_sel = false;
                    if (substitui) caixa_definir(&endereco, "");
                    else desenhar_barra();
                }
                if (caixa_tratar(j, &endereco, &e) && endereco.texto[0]) navegar(endereco.texto);
                continue;
            }
            if (t == TECLA_BAIXO) rolar(topo + 1);
            else if (t == TECLA_CIMA) rolar(topo - 1);
            else if (t == TECLA_PGDN || t == ' ') rolar(topo + VISIVEIS - 1);
            else if (t == TECLA_PGUP) rolar(topo - (VISIVEIS - 1));
            else if (t == TECLA_HOME) rolar(0);
            else if (t == TECLA_END) rolar(linhas);
            else if (t == '\b') voltar();
            else if (t == '\t') mover_foco((e.modificadores & 1) ? -1 : 1);
            else if (t == '\n') { if (foco_alvo >= 0) abrir_alvo(foco_alvo); }
            else if (t == TECLA_ESC || t == 27) { if (buscando) fechar_busca(); else if (foco_alvo >= 0) { foco_alvo = -1; desenhar_pagina(); } }
            continue;
        }
        /* Ratón: las cajas de texto reciben clics y movimientos. */
        if (e.tipo == EV_MOVER) { mouse_x = e.x; mouse_y = e.y; }
        if (e.tipo == EV_CLIQUE) { mouse_x = e.x; mouse_y = e.y; }
        /* La barra de abas: un clic cambia de aba, el botón "+" abre una nueva. */
        if (e.tipo == EV_CLIQUE && e.y >= TOPO && e.y < ABA_Y) {
            if (e.x >= LARG - 34) { nova_aba(); continue; }
            int idx = aba_no_clic(e.x);
            if (idx >= 0) { trocar_aba(idx); continue; }
        }
        if (e.tipo == EV_CLIQUE && (e.botoes & 2) && motor) {
            const motor_alvo_t *a = alvo_motor_em(e.x, e.y);
            salvar_imagem_url[0] = salvar_link_url[0] = 0;
            qtd_menu_acoes = 0;
            if (a && a->tipo == MA_IMAGEM && a->href) copia_texto(salvar_imagem_url, a->href, sizeof salvar_imagem_url);
            if (a && a->tipo == MA_LINK && a->href) {
                copia_texto(salvar_link_url, a->href, sizeof salvar_link_url);
                if (a->imagem) copia_texto(salvar_imagem_url, a->imagem, sizeof salvar_imagem_url);
            }
            /* O menu (D152): guardar a imagem, guardar o enlace e abrir o
             * enlace noutra aba. Cada linha guarda a sua acção. */
            char itens[128];
            int k = 0;
            if (salvar_imagem_url[0]) {
                k += formatar_texto(itens + k, (int)sizeof itens - k, "Guardar imagen\n");
                menu_acoes[qtd_menu_acoes++] = ACAO_GUARDAR_IMAGEM;
            }
            if (salvar_link_url[0]) {
                k += formatar_texto(itens + k, (int)sizeof itens - k, "Guardar enlace\n");
                menu_acoes[qtd_menu_acoes++] = ACAO_GUARDAR_LINK;
            }
            if (salvar_link_url[0]) {
                k += formatar_texto(itens + k, (int)sizeof itens - k, "Abrir en pestaña nueva\n");
                menu_acoes[qtd_menu_acoes++] = ACAO_ABA_NOVA;
            }
            if (!k) continue;
            itens[k - 1] = 0;   /* sem o "\n" do fim */
            menu_contextual(j->id, e.x, e.y, itens, COD_MENU_CONTEXTUAL);
            continue;
        }
        if (e.tipo == EV_CLIQUE) {
            bool tinha = endereco.foco;
            endereco_sel = false;
            if (tinha) desenhar_barra();
            if (buscando && caixa_tratar(j, &caixa_busca, &e)) { /* Intro no llega con el ratón */ }
            if (caixa_tratar(j, &endereco, &e) && endereco.texto[0]) { navegar(endereco.texto); continue; }
            if (!tinha && endereco.foco) {   /* primer clic en la dirección: queda seleccionada, como en los navegadores */
                endereco.cursor = (int)tam_texto(endereco.texto);
                endereco_sel = true;
                desenhar_barra();
            }
        }
        if (botao_tratar(j, &bt_ir, &e) && endereco.texto[0]) { navegar(endereco.texto); continue; }
        if (botao_tratar(j, &bt_inicio, &e)) { navegar("sobre:inicio"); continue; }
        if (botao_tratar(j, &bt_recarregar, &e)) { recarregar(); continue; }
        if (botao_tratar(j, &bt_avancar, &e)) { avancar(); continue; }
        if (botao_tratar(j, &bt_voltar, &e)) { voltar(); continue; }
        if (e.tipo == EV_CLIQUE && (e.botoes & 1)) {
            if (e.x >= LARG - ROLAGEM && e.y >= ABA_Y && e.y < ALT - RODAPE && motor_altura > AREA_A) {
                /* Barra de desplazamiento: sobre el pulgar lo agarra; en otro punto, lo centra ahí y sigue arrastrando. */
                int py, a;
                pulgar(&py, &a);
                rolando_dy = (e.y >= py && e.y < py + a) ? e.y - py : a / 2;
                rolando = true;
                rolar_para_pulgar(e.y);
                desenhar_pagina();
                continue;
            }
            const motor_alvo_t *a = alvo_motor_em(e.x, e.y);
            if (a) {
                foco_alvo = -1;
                /* Ctrl+clic abre o link noutra aba (D152). */
                if ((e.modificadores & 2) && a->tipo == MA_LINK && a->href) abrir_link_nova_aba(a->href);
                else abrir_alvo_motor(a);
            }
            continue;
        }
        if (e.tipo == EV_MOVER) {
            if (rolando) {
                if (e.botoes & 1) rolar_para_pulgar(e.y);
                else { rolando = false; desenhar_pagina(); }
                continue;
            }
            const motor_alvo_t *a = alvo_motor_em(e.x, e.y);
            if (a && a->tipo == MA_LINK) rodape(a->href);
            else if (a) rodape("Formulario: haz clic para escribir");
            else rodape("Listo");
        }
    }
    /* Guarda a posição de leitura do sítio onde se ficou (D152), para que a
     * próxima vez a página apareça onde se leu. */
    if (url_atual[0]) {
        posicao_guardar(url_atual, rol_px);
        gravar_posicoes();
    }
    liberar_imagens();
    jan_fechar(j);
    return 0;
}
