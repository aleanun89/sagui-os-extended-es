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
#include "video.h"   /* <video> con H.264/AAC (mejora 100, D276) */
#include "audio_formato.h"   /* <audio>: WAV, MP3 y FLAC (D489) */
#include "motor.h"
#include "js.h"
#include "rede.h"        /* D487: cookies, caché, POST, descompresión */
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
#define ABA_A     28                /* altura da barra de abas, arriba del todo (D148, D373) */
#define BARRA_A   34                /* la barra de herramientas, debajo de las pestañas */
#define MARCB_A   22                /* la barra de marcadores (opcional) */
#define ABA_Y     (ABA_A + BARRA_A + (conf.barra_marc ? MARCB_A : 0))   /* onde começa a página */
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
static bool relayout_pendente;      /* hay trozos nuevos sin maquetar (D441) */
static unsigned ult_relayout;
static unsigned est_repintados, est_apresentacoes, est_layouts, est_ms_pintura, est_trozos;   /* medidas del log */
static video_t *vid;                  /* declaración anticipada (la definición está en la sección de <video>) */
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
#define ABAS_MAX 16
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
    bool    carregando;               /* indicador de carga de la pestaña (D373) */
    bool    tem_fav, fav_tentado;     /* favicon de 16x16 (o el icono genérico) */
    uint32_t fav[16 * 16];
} aba_t;
static aba_t abas[ABAS_MAX];
static int   qtd_abas;
static int   aba_atual;

/* Marcadores (D148): os sítios que o utilizador guardou, num ficheiro de texto
 * (uma linha por marcador: url, uma tabulação e o título). */
#define MARC_MAX 128
static char marcador_url[MARC_MAX][URL_MAX];
static char marcador_titulo[MARC_MAX][96];
static char marcador_pasta[MARC_MAX][32];   /* carpeta del marcador ("" = sin carpeta, D373) */
static int  qtd_marcadores;
#define COD_MENU_MARCADORES 64
#define COD_MENU_CONTEXTUAL 65   /* el menú del botón derecho (D152) */
#define COD_MENU_ABA 66          /* el menú de una pestaña (D373) */
#define COD_MENU_ESTRELA 67
#define COD_MENU_PRINCIPAL 68    /* el menú «≡» */
#define COD_MENU_PASTA 69        /* una carpeta de la barra de marcadores */
#define COD_COMBO 70             /* de 70 en adelante: los desplegables de los ajustes */

/* Posições de leitura por sítio (D152): onde se ficou a ler em cada página,
 * para que ao voltar a ela apareça tal como se deixou. */
#define POS_MAX 64
static char pos_url[POS_MAX][URL_MAX];
static int  pos_px[POS_MAX];
static int  qtd_pos;

/* El menú del botón derecho (D152): cada línea tiene su acción, para que el
 * EV_MENU sepa qué hacer con la línea elegida. */
enum { ACAO_NADA = 0, ACAO_GUARDAR_IMAGEM, ACAO_GUARDAR_LINK, ACAO_ABA_NOVA, ACAO_DESCARGAR };
static int menu_acoes[4];
static int qtd_menu_acoes;

static jan_t  *j;
static caixa_t endereco;
static char rodape_texto[128];

/* ---- configuración, historial y gestores (D373) ---- */

/* Los ajustes del Cipó (D373): se guardan en /home/.config/cipo/cipo.conf, una línea «clave=valor». */
#define HILOS_MAX 12                /* descargas simultáneas: de 1 a 12 */
typedef struct {
    int  inicio_modo;               /* 0 = página propia de Cipó, 1 = en blanco, 2 = una dirección */
    char inicio_url[URL_MAX];
    int  buscador;                  /* índice en buscadores[] */
    int  hilos;                     /* descargas simultáneas pedidas (1-12) */
    char pasta_desc[96];            /* carpeta de descargas ("" = la de siempre) */
    bool js, imagens, barra_marc;
} conf_t;
static conf_t conf = { 0, "", 0, 4, "", true, true, false };   /* D373: 4 descargas simultáneas por defecto (mantenedor, 2026-11-17) */
typedef struct { const char *nome, *acao, *campo; } buscador_t;
static const buscador_t buscadores[] = {
    { "DuckDuckGo", "https://lite.duckduckgo.com/lite/", "q" },
    { "Wikipedia (es)", "https://es.wikipedia.org/w/index.php", "search" },
    { "Google", "https://www.google.com/search", "q" },
    { "Bing", "https://www.bing.com/search", "q" },
};
#define N_BUSC 4
static const char *const buscadores_nomes[N_BUSC] = { "DuckDuckGo", "Wikipedia (es)", "Google", "Bing" };

/* El historial que se recuerda entre sesiones (para sugerencias, «más visitados» y Ctrl+H). */
#define HST_MAX 400
#define HST_URL 160
typedef struct { char url[HST_URL]; char tit[64]; int visitas; unsigned ordem; } hst_t;
static hst_t hst[HST_MAX];
static int   qtd_hst;
static unsigned hst_contador;

/* Los paneles de gestión (marcadores, historial, descargas y ajustes) ocupan el sitio de la página. */
enum { P_NADA = 0, P_MARC, P_HIST, P_DESC, P_GERAL, P_PRIV, P_REND, P_N };
static int painel;

/* El gestor de descargas: cada una es un hilo que escribe su archivo (pausa y cancelar por banderas). */
#define DESC_MAX 16
enum { D_LIVRE = 0, D_BAIXANDO, D_PAUSADA, D_PRONTA, D_ERRO, D_CANCELADA };
typedef struct {
    char url[URL_MAX];
    char nome[64];
    char caminho[160];
    char erro[64];
    volatile int estado, bytes, total, pausa, cancela;
} desc_t;
static desc_t desc[DESC_MAX];
static int    qtd_desc, desc_sel = -1;

static char cert_texto[640];     /* el certificado de la página HTTPS actual (candado, D373) */
static bool cert_ok;
static int hilos_efectivos(void);
static void mostrar_painel(int p);
static void fechar_painel(void);
static void desenhar_barra(void);
static void desenhar_abas(void);
static void desenhar_marcb(void);
static void desc_iniciar(const char *url);
static void pf_limpar(void);
static void pf_lote(void);
static void sugestoes_fechar(void);
static void hst_registar(const char *url, const char *tit);
static uint8_t *pf_pegar(const char *url, unsigned *n, bool *achou);
static char *inicio_gerar(void);
static void painel_desenhar(void);
static void novo_tamanho(void);
static const char *url_inicio(void);
static void interpretar_entrada(const char *t, char *url, int max);
static void favicon_buscar(void);
static bool carregando_agora(void);
static void fechar_aba_idx(int i);

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

/* ---- pintura de la página sin parpadeo (D441) ----
 * La página se pinta en un búfer aparte (pag_fb) y se copia a la ventana de una vez: la
 * ventana nunca muestra el fondo vacío ni una página a medias. Al desplazarse se mueven
 * las filas ya pintadas del búfer y solo se pinta la franja que entra. */
static uint32_t *pag_fb;          /* la página visible: (LARG - ROLAGEM) x AREA_A */
static int pag_fb_l, pag_fb_a;
static bool pag_fb_ok;            /* pag_fb tiene la página entera para rol_px */

static bool pag_fb_garantir(void)
{
    int l = LARG - ROLAGEM, a = AREA_A;
    if (l <= 0 || a <= 0) return false;
    if (pag_fb && pag_fb_l == l && pag_fb_a == a) return true;
    libera(pag_fb);
    pag_fb = aloca((size_t)l * (size_t)a * 4);
    pag_fb_l = l; pag_fb_a = a;
    pag_fb_ok = false;
    return pag_fb != NULL;
}

/* La barra de desplazamiento (solo ella se dibuja directo en la ventana: es una columna). */
static void desenhar_barra_rolagem(void)
{
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
}

static void media_controles_pintar(void);   /* controles de <video> y <audio> (D489) */

/* Copia el búfer entero a la ventana y la presenta una sola vez. */
static void pag_fb_apresentar(void)
{
    for (int y = 0; y < pag_fb_a; y++)
        memcpy(j->px + (ABA_Y + y) * j->larg, pag_fb + y * pag_fb_l, (size_t)pag_fb_l * 4);
    media_controles_pintar();
    desenhar_barra_rolagem();
    jan_mostrar_area(j, 0, ABA_Y, LARG, AREA_A);
    est_apresentacoes++;
}

/* Pinta en el búfer las filas [f0, f0 + n) (filas de la ventana; y de la página = rol_px + f). */
static void pag_fb_pintar(int f0, int n)
{
    if (n <= 0) return;
    motor_rolagem(motor, rol_px);      /* fixed y sticky siguen el desplazamiento, no la franja (D271) */
    motor_pintar(motor, pag_fb + f0 * pag_fb_l, pag_fb_l, pag_fb_l, rol_px + f0, n);
}

/* Repinta la página entera de golpe (sin ceder el control). */
static void pagina_pintar_agora(void)
{
    if (!motor || !pag_fb_garantir()) return;
    unsigned t0 = tempo_ms();
    for (int f = 0; f < pag_fb_a; f += FAIXA_PINTURA)
        pag_fb_pintar(f, pag_fb_a - f < FAIXA_PINTURA ? pag_fb_a - f : FAIXA_PINTURA);
    pag_fb_ok = true;
    pintura_pendente = false;
    pag_fb_apresentar();
    est_ms_pintura += tempo_ms() - t0;
}

static void desenhar_pagina(void)
{
    if (painel) { painel_desenhar(); return; }
    if (motor && pag_fb_garantir()) {
        /* se pinta en franjas dentro del búfer (cediendo el control entre ellas);
         * la ventana sigue mostrando lo anterior hasta que la página nueva está entera */
        est_repintados++;
        pintura_y = rol_px;
        pintura_pendente = true;
        pag_fb_ok = false;
        return;
    }
    pintura_pendente = false;
    g_retangulo(j, 0, ABA_Y, LARG - ROLAGEM, AREA_A, motor ? motor_fundo(motor) : COR_BRANCO);
    desenhar_barra_rolagem();
    jan_mostrar_area(j, 0, ABA_Y, LARG, AREA_A);
}

/* Desplaza la página hasta rol_px (desde velho): mueve lo ya pintado y pinta solo la franja nueva. */
static void rolar_pintar(int velho)
{
    int d = rol_px - velho;
    bool copiar = motor && !painel && pag_fb_ok && pag_fb_garantir() && !pintura_pendente && !vid &&
                  d != 0 && (d < 0 ? -d : d) < pag_fb_a && !motor_tem_camadas(motor);
    if (!copiar) {
        if (motor && !painel) { est_repintados++; pagina_pintar_agora(); }
        else desenhar_pagina();
        return;
    }
    unsigned t0 = tempo_ms();
    int H = pag_fb_a, L = pag_fb_l;
    if (d > 0) {
        memmove(pag_fb, pag_fb + d * L, (size_t)(H - d) * (size_t)L * 4);
        pag_fb_pintar(H - d, d);
    } else {
        memmove(pag_fb + (-d) * L, pag_fb, (size_t)(H + d) * (size_t)L * 4);
        pag_fb_pintar(0, -d);
    }
    pag_fb_apresentar();
    est_repintados++;
    est_ms_pintura += tempo_ms() - t0;
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
    int velho = rol_px;
    registrar("cipo: rolagem para %d px (linha %d)\n", novo, topo);
    unsigned t0 = tempo_ms();
    /* un salto grande se reparte en pasos (desplazamiento suave) */
    int dist = novo - velho, ad = dist < 0 ? -dist : dist;
    int pasos = ad >= 120 && !painel ? (ad >= 360 ? 4 : 3) : 1;
    for (int k = 1; k <= pasos; k++) {
        int alvo = pasos == 1 || k == pasos ? novo : velho + dist * k / pasos;
        int ant = rol_px;
        rol_px = alvo;
        rolar_pintar(ant);
        if (k < pasos) dormir(6);
    }
    registrar("cipo: rolagem pintada en %u ms\n", tempo_ms() - t0);
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
    int velho = rol_px;
    rol_px = (pos * max + livre / 2) / livre;
    topo = (rol_px + LINHA_A / 2) / LINHA_A;
    if (rol_px != velho) rolar_pintar(velho);
}



/* ---- barra de herramientas y pestañas modernas (D373) ---- */

#define COR_BARRA   0xF4F6F8     /* barra de herramientas y pestaña activa (van unidas) */
#define COR_TIRA    0xD3D9E0     /* la tira de pestañas */
#define COR_ABA_OFF 0xC3CAD3
#define COR_ICONE   0x3A4550

enum { B_VOLTAR = 0, B_AVANCAR, B_RECARGAR, B_INICIO, B_CANDADO, B_ESTRELA, B_MENU, B_N };
typedef struct { int x, y, l, a; } cr_t;
static cr_t br[B_N];
static int  hover_b = -1;          /* botón bajo el ratón */
static int  pilula_x, pilula_l;    /* el óvalo de la dirección */
static int  aba_ini;               /* primera pestaña visible (desplazamiento) */
static int  aba_l = 160;           /* ancho de cada pestaña */
static int  aba_cabem = 1;
static int  arrasto = -1;          /* pestaña que se arrastra */
#define ABA_L_MAX 200
#define ABA_L_MIN 96
#define ABA_MAIS  26               /* el botón «+» */

/* Las sugerencias que salen bajo la dirección al escribir (marcadores primero, luego el historial). */
#define SUG_MAX 6
static int  sug_n, sug_sel = -1;
static const char *sug_url[SUG_MAX];
static char sug_rot[SUG_MAX][72];

static bool marcado_atual(void)
{
    for (int i = 0; i < qtd_marcadores; i++)
        if (compara_texto(marcador_url[i], url_atual) == 0) return true;
    return false;
}

static void lg(jan_t *jj, int x0, int y0, int x1, int y1, uint32_t c)
{
    g_linha(jj, x0, y0, x1, y1, c);
    g_linha(jj, x0 + 1, y0, x1 + 1, y1, c);
}

/* Los iconos, dibujados con líneas (diseño propio): (x, y) es la esquina de un cuadro de 24x24. */
static void icone(int k, int x, int y, uint32_t c, bool parar)
{
    int cx = x + 12, cy = y + 12;
    switch (k) {
    case B_VOLTAR:
        lg(j, cx + 6, cy, cx - 6, cy, c); lg(j, cx - 6, cy, cx, cy - 6, c); lg(j, cx - 6, cy, cx, cy + 6, c);
        break;
    case B_AVANCAR:
        lg(j, cx - 6, cy, cx + 6, cy, c); lg(j, cx + 6, cy, cx, cy - 6, c); lg(j, cx + 6, cy, cx, cy + 6, c);
        break;
    case B_RECARGAR:
        if (parar) { lg(j, cx - 5, cy - 5, cx + 5, cy + 5, c); lg(j, cx + 5, cy - 5, cx - 5, cy + 5, c); break; }
        {
            static const int8_t an[11][2] = { {7,0},{6,3},{3,6},{0,7},{-3,6},{-6,3},{-7,0},{-6,-3},{-3,-6},{0,-7},{3,-6} };
            for (int i = 0; i < 10; i++) lg(j, cx + an[i][0], cy + an[i][1], cx + an[i + 1][0], cy + an[i + 1][1], c);
            lg(j, cx + 3, cy - 6, cx + 8, cy - 6, c); lg(j, cx + 3, cy - 6, cx + 3, cy - 1, c);
        }
        break;
    case B_INICIO:
        lg(j, cx - 8, cy, cx, cy - 7, c); lg(j, cx, cy - 7, cx + 8, cy, c);
        g_retangulo(j, cx - 5, cy, 2, 7, c); g_retangulo(j, cx + 4, cy, 2, 7, c); g_retangulo(j, cx - 5, cy + 6, 11, 2, c);
        break;
    case B_CANDADO:   /* c = color del cuerpo; parar = abierto (sin cifrar) */
        g_retangulo(j, cx - 5, cy - 1, 11, 8, c);
        g_retangulo(j, cx - 3, cy - 6, 2, 5, c); g_retangulo(j, cx + 2, cy - 6, 2, 5, c); g_retangulo(j, cx - 3, cy - 7, 8, 2, c);
        if (parar) g_retangulo(j, cx + 2, cy - 6, 2, 3, 0xF4F6F8);
        break;
    case B_ESTRELA: {
        static const int8_t p[10][2] = { {0,-8},{2,-3},{8,-2},{3,1},{5,7},{0,4},{-5,7},{-3,1},{-8,-2},{-2,-3} };
        for (int i = 0; i < 10; i++) {
            int a = (i + 1) % 10;
            lg(j, cx + p[i][0], cy + p[i][1], cx + p[a][0], cy + p[a][1], c);
            if (parar) g_linha(j, cx, cy, cx + p[i][0], cy + p[i][1], c);   /* parar = rellena (ya es marcador) */
        }
        break;
    }
    case B_MENU:
        g_retangulo(j, cx - 7, cy - 6, 14, 2, c); g_retangulo(j, cx - 7, cy - 1, 14, 2, c); g_retangulo(j, cx - 7, cy + 4, 14, 2, c);
        break;
    }
}

static void ajustar_barra(void)
{
    int y = ABA_A + 5;
    br[B_VOLTAR]   = (cr_t){ 6, y, 24, 24 };
    br[B_AVANCAR]  = (cr_t){ 32, y, 24, 24 };
    br[B_RECARGAR] = (cr_t){ 58, y, 24, 24 };
    br[B_INICIO]   = (cr_t){ 84, y, 24, 24 };
    pilula_x = 114;
    pilula_l = LARG - 34 - pilula_x;
    br[B_CANDADO]  = (cr_t){ pilula_x + 2, y, 22, 24 };
    br[B_ESTRELA]  = (cr_t){ pilula_x + pilula_l - 26, y, 24, 24 };
    br[B_MENU]     = (cr_t){ LARG - 30, y, 24, 24 };
    endereco.x = pilula_x + 26;
    endereco.y = ABA_A + 5;
    endereco.l = pilula_l - 26 - 28;
}

/* ¿Hay algo cargándose en la pestaña activa? */
static bool carregando_agora(void) { return recursos_pendentes || pintura_pendente || abas[aba_atual].carregando; }

static void desenhar_barra(void)
{
    g_retangulo(j, 0, ABA_A, LARG, BARRA_A, COR_BARRA);
    g_retangulo(j, 0, ABA_A + BARRA_A - 1, LARG, 1, COR_BORDA);
    bool car = carregando_agora();
    for (int k = 0; k < B_N; k++) {
        if (k == B_CANDADO || k == B_ESTRELA) continue;
        bool ativo = true;
        if (k == B_VOLTAR) ativo = qtd_historico > 0;
        if (k == B_AVANCAR) ativo = qtd_futuro > 0;
        uint32_t c = ativo ? COR_ICONE : 0xA9B1BA;
        if (k == hover_b && ativo) g_retangulo(j, br[k].x, br[k].y, br[k].l, br[k].a, 0xDDE3EA);
        else g_retangulo(j, br[k].x, br[k].y, br[k].l, br[k].a, COR_BARRA);
        icone(k, br[k].x, br[k].y, c, k == B_RECARGAR ? car : false);
    }
    /* el óvalo de la dirección, con el candado (verde = HTTPS válido, ámbar = sin verificar, rojo = sin cifrar) */
    g_retangulo(j, pilula_x, ABA_A + 4, pilula_l, 26, COR_BRANCO);
    g_moldura(j, pilula_x, ABA_A + 4, pilula_l, 26, endereco.foco ? COR_DESTAQUE : COR_BORDA);
    if (hover_b == B_ESTRELA) g_retangulo(j, br[B_ESTRELA].x, br[B_ESTRELA].y, br[B_ESTRELA].l, br[B_ESTRELA].a, 0xE9EEF3);
    icone(B_ESTRELA, br[B_ESTRELA].x, br[B_ESTRELA].y, marcado_atual() ? 0xE0A020 : 0x8A939C, marcado_atual());
    if (comeca_com(url_atual, "https://")) icone(B_CANDADO, br[B_CANDADO].x, br[B_CANDADO].y, cert_ok ? COR_DESTAQUE : 0xC98A10, false);
    else if (comeca_com(url_atual, "http://")) icone(B_CANDADO, br[B_CANDADO].x, br[B_CANDADO].y, 0xB03A2E, true);
    else icone(B_CANDADO, br[B_CANDADO].x, br[B_CANDADO].y, 0x8A939C, false);
    caixa_desenhar(j, &endereco);
    if (endereco_sel && endereco.foco) {   /* dirección "seleccionada" (Ctrl+L) */
        int n = (int)tam_texto(endereco.texto), cabem = (endereco.l - 10) / LETRA_L;
        if (n > cabem) n = cabem;
        g_retangulo(j, endereco.x + 4, endereco.y + 3, n * LETRA_L + 2, LETRA_A + 2, COR_SELECAO);
        g_texto_n(j, endereco.x + 5, endereco.y + 4, endereco.texto, n, COR_BRANCO);
    }
    desenhar_marcb();
    jan_mostrar_area(j, 0, ABA_A, LARG, BARRA_A + (conf.barra_marc ? MARCB_A : 0));
}

/* La barra de marcadores: las carpetas primero como botones con menú, luego los sueltos. */
#define MARCB_ITENS 24
static int marcb_x[MARCB_ITENS + 1], marcb_idx[MARCB_ITENS];   /* x de cada chip; índice del marcador (o -1-n = carpeta n) */
static int marcb_n;
static char pastas[16][32];
static int  qtd_pastas;

static void juntar_pastas(void)
{
    qtd_pastas = 0;
    for (int i = 0; i < qtd_marcadores; i++) {
        if (!marcador_pasta[i][0]) continue;
        bool ja = false;
        for (int p = 0; p < qtd_pastas; p++) if (compara_texto(pastas[p], marcador_pasta[i]) == 0) ja = true;
        if (!ja && qtd_pastas < 16) copia_texto(pastas[qtd_pastas++], marcador_pasta[i], 32);
    }
}

static void desenhar_marcb(void)
{
    if (!conf.barra_marc) return;
    int y = ABA_A + BARRA_A;
    g_retangulo(j, 0, y, LARG, MARCB_A, 0xEAEEF2);
    g_retangulo(j, 0, y + MARCB_A - 1, LARG, 1, COR_BORDA);
    juntar_pastas();
    marcb_n = 0;
    int x = 6;
    for (int p = 0; p < qtd_pastas && marcb_n < MARCB_ITENS; p++) {
        char t[40];
        { char pn[20]; copia_texto(pn, pastas[p], 15); formatar_texto(t, sizeof t, "[%s]", pn); }
        int l = (int)tam_texto(t) * LETRA_L + 12;
        if (x + l > LARG - 8) break;
        g_retangulo(j, x, y + 2, l, MARCB_A - 5, 0xD9DFE6);
        g_texto(j, x + 6, y + 3, t, COR_TEXTO);
        marcb_x[marcb_n] = x; marcb_idx[marcb_n++] = -1 - p;
        x += l + 4;
    }
    for (int i = 0; i < qtd_marcadores && marcb_n < MARCB_ITENS; i++) {
        if (marcador_pasta[i][0]) continue;
        const char *t = marcador_titulo[i][0] ? marcador_titulo[i] : marcador_url[i];
        int n = (int)tam_texto(t); if (n > 16) n = 16;
        int l = n * LETRA_L + 12;
        if (x + l > LARG - 8) break;
        g_retangulo(j, x, y + 2, l, MARCB_A - 5, 0xDDE3EA);
        g_texto_n(j, x + 6, y + 3, t, n, COR_TEXTO);
        marcb_x[marcb_n] = x; marcb_idx[marcb_n++] = i;
        x += l + 4;
    }
    marcb_x[marcb_n] = x;
}

/* Qué chip de la barra de marcadores hay en (x, y); -1 = ninguno. */
static int marcb_em(int x, int y)
{
    if (!conf.barra_marc || y < ABA_A + BARRA_A || y >= ABA_A + BARRA_A + MARCB_A) return -1;
    for (int i = 0; i < marcb_n; i++) if (x >= marcb_x[i] && x < marcb_x[i + 1] - 4) return i;
    return -1;
}

/* ---- las pestañas ---- */

static void abas_layout(void)
{
    int avail = LARG - 8 - ABA_MAIS - 6;
    int n = qtd_abas > 0 ? qtd_abas : 1;
    int l = avail / n;
    if (l > ABA_L_MAX) l = ABA_L_MAX;
    if (l < ABA_L_MIN) l = ABA_L_MIN;
    aba_l = l;
    aba_cabem = avail / l;
    if (aba_cabem < 1) aba_cabem = 1;
    if (aba_cabem > n) aba_cabem = n;
    if (aba_ini > n - aba_cabem) aba_ini = n - aba_cabem;
    if (aba_ini < 0) aba_ini = 0;
    if (aba_atual < aba_ini) aba_ini = aba_atual;
    if (aba_atual >= aba_ini + aba_cabem) aba_ini = aba_atual - aba_cabem + 1;
}

static int aba_x(int i) { return 4 + (i - aba_ini) * aba_l; }
static int mais_x(void) { int x = 4 + aba_cabem * aba_l + 2; return x > LARG - ABA_MAIS - 4 ? LARG - ABA_MAIS - 4 : x; }

/* El icono genérico (un globo) cuando la página no da favicon. */
static void favicon_generico(int x, int y, uint32_t cor)
{
    for (int dy = -7; dy <= 7; dy++)
        for (int dx = -7; dx <= 7; dx++) {
            int d = dx * dx + dy * dy;
            if (d <= 49 && d >= 36) g_retangulo(j, x + 8 + dx, y + 8 + dy, 1, 1, cor);
        }
    g_retangulo(j, x + 1, y + 8, 15, 1, cor);
    g_retangulo(j, x + 8, y + 1, 1, 15, cor);
    for (int dy = -5; dy <= 5; dy += 1) { int dx = 3 - (dy * dy) / 8; g_retangulo(j, x + 8 + dx, y + 8 + dy, 1, 1, cor); g_retangulo(j, x + 8 - dx, y + 8 + dy, 1, 1, cor); }
}

static void desenhar_spinner(int x, int y)
{
    static const int8_t d[8][2] = { {0,-6},{4,-4},{6,0},{4,4},{0,6},{-4,4},{-6,0},{-4,-4} };
    int h = (int)((tempo_ms() / 90) % 8);
    for (int i = 0; i < 8; i++) {
        int k = (h - i + 8) % 8;   /* 0 = cabeza */
        uint32_t c = k == 0 ? COR_DESTAQUE : k < 3 ? 0x5FA880 : k < 5 ? 0x9CC6AE : 0xC9DDD1;
        g_retangulo(j, x + 8 + d[i][0] - 1, y + 8 + d[i][1] - 1, 3, 3, c);
    }
}

/* El título cortado con «...» para que quepa en n columnas. */
static void titulo_corto(const char *t, int n, char *s, int max)
{
    int l = (int)tam_texto(t);
    if (n < 4) n = 4;
    if (n > max - 1) n = max - 1;
    if (l <= n) { copia_texto(s, t, (size_t)max); return; }
    memcpy(s, t, (size_t)n - 3);
    s[n - 3] = s[n - 2] = s[n - 1] = '.';
    s[n] = 0;
}

static void desenhar_abas(void)
{
    abas_layout();
    g_retangulo(j, 0, 0, LARG, ABA_A, COR_TIRA);
    g_retangulo(j, 0, ABA_A - 1, LARG, 1, COR_BORDA);
    for (int i = aba_ini; i < aba_ini + aba_cabem; i++) {
        int x = aba_x(i);
        bool ativa = (i == aba_atual);
        int l = aba_l - 2;
        g_retangulo(j, x, 3, l, ABA_A - 3, ativa ? COR_BARRA : COR_ABA_OFF);
        if (ativa) {
            g_retangulo(j, x, 3, l, 2, COR_DESTAQUE);
            g_retangulo(j, x, ABA_A - 1, l, 1, COR_BARRA);   /* unida a la barra de herramientas */
        }
        aba_t *a = &abas[i];
        bool car = ativa ? carregando_agora() : a->carregando;
        if (car) desenhar_spinner(x + 5, 8);
        else if (a->tem_fav) {
            for (int fy = 0; fy < 16; fy++)
                for (int fx = 0; fx < 16; fx++) {
                    uint32_t p = a->fav[fy * 16 + fx];
                    if (p >> 24 >= 0x80 || (p >> 24) == 0) g_retangulo(j, x + 5 + fx, 8 + fy, 1, 1, p & 0xFFFFFF);
                }
        } else favicon_generico(x + 5, 8, ativa ? COR_ICONE : 0x6A737C);
        bool tem_x = aba_l >= 60;
        int cols = (l - 26 - (tem_x ? 18 : 0)) / LETRA_L;
        char t[96];
        titulo_corto(a->titulo[0] ? a->titulo : a->url, cols, t, sizeof t);
        g_texto_n(j, x + 25, 8, t, cols, ativa ? 0x1E2328 : 0x4A535C);
        if (tem_x) {
            int bx = x + l - 17;
            lg(j, bx + 3, 11, bx + 9, 17, 0x6A737C); lg(j, bx + 9, 11, bx + 3, 17, 0x6A737C);
        }
        if (!ativa && i + 1 < aba_ini + aba_cabem && i + 1 != aba_atual) g_retangulo(j, x + l, 8, 1, 14, 0xA9B1BA);
    }
    int mx = mais_x();
    lg(j, mx + 8, 14, mx + 18, 14, COR_ICONE); g_retangulo(j, mx + 12, 9, 2, 11, COR_ICONE);
    jan_mostrar_area(j, 0, 0, LARG, ABA_A);
}

/* Qué hay en el punto x de la tira: >= 0 una pestaña, -1 nada, -2 el «+»; *no_x = sobre el ×. */
static int aba_no_clic(int x, bool *no_x)
{
    if (no_x) *no_x = false;
    int mx = mais_x();
    if (x >= mx && x < mx + ABA_MAIS) return -2;
    for (int i = aba_ini; i < aba_ini + aba_cabem && i < qtd_abas; i++) {
        int ini = aba_x(i);
        if (x >= ini && x < ini + aba_l - 2) {
            if (no_x && aba_l >= 60 && x >= ini + aba_l - 2 - 19) *no_x = true;
            return i;
        }
    }
    return -1;
}

/* Las sugerencias bajo la dirección (D373): lo que se escribe, en marcadores y historial. */
static bool contem_sem_caixa(const char *h, const char *a)
{
    int n = (int)tam_texto(a);
    if (!n) return false;
    for (const char *p = h; *p; p++) if (igual_sem_caixa_e_tilde(p, a, n)) return true;
    return false;
}

static void sugestoes_calcular(void)
{
    sug_n = 0; sug_sel = -1;
    if (!endereco.foco || endereco_sel || !endereco.texto[0] || comeca_com(endereco.texto, "sobre:")) return;
    for (int i = 0; i < qtd_marcadores && sug_n < SUG_MAX; i++)
        if (contem_sem_caixa(marcador_url[i], endereco.texto) || contem_sem_caixa(marcador_titulo[i], endereco.texto)) {
            sug_url[sug_n] = marcador_url[i];
            formatar_texto(sug_rot[sug_n], sizeof sug_rot[0], "* %s", marcador_titulo[i][0] ? marcador_titulo[i] : marcador_url[i]);
            sug_n++;
        }
    for (int i = qtd_hst - 1; i >= 0 && sug_n < SUG_MAX; i--)
        if (contem_sem_caixa(hst[i].url, endereco.texto) || contem_sem_caixa(hst[i].tit, endereco.texto)) {
            bool dup = false;
            for (int k = 0; k < sug_n; k++) if (compara_texto(sug_url[k], hst[i].url) == 0) dup = true;
            if (dup) continue;
            sug_url[sug_n] = hst[i].url;
            formatar_texto(sug_rot[sug_n], sizeof sug_rot[0], "  %s", hst[i].url);
            sug_n++;
        }
}

static void sugestoes_desenhar(void)
{
    if (!sug_n) return;
    int y0 = ABA_A + BARRA_A + (conf.barra_marc ? MARCB_A : 0), x = pilula_x, l = pilula_l;
    int a = sug_n * 20 + 4;
    g_retangulo(j, x, y0, l, a, COR_BRANCO);
    g_moldura(j, x, y0, l, a, COR_BORDA);
    for (int i = 0; i < sug_n; i++) {
        if (i == sug_sel) g_retangulo(j, x + 1, y0 + 2 + i * 20, l - 2, 20, COR_SELECAO);
        g_texto_n(j, x + 8, y0 + 4 + i * 20, sug_rot[i], (l - 16) / LETRA_L, i == sug_sel ? COR_BRANCO : COR_TEXTO);
    }
    jan_mostrar_area(j, x, y0, l, a);
}

static void sugestoes_fechar(void)
{
    if (!sug_n) return;
    sug_n = 0; sug_sel = -1;
    desenhar_pagina();
}

/* ---- abas (D148) ---- */

static void carregar(const char *pedido);   /* usado por nova_aba() */
static void video_parar(void);
static void nova_aba_com_url(const char *url);
static void rolar_px(int px);              /* usado por carregar() */
static int zoom_do_sitio(const char *url);   /* usado al cargar la página (D489) */
static void zoom_paso(int dir);

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
    caixa_definir(&endereco, url_atual);          /* la dirección de la pestaña (D373) */
    endereco.foco = false;
    endereco_sel = false;
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
    video_parar();
    pf_limpar();
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
    carregar(link ? url : url_inicio());
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

/* Cierra la pestaña i (la activa o cualquier otra, con el clic central o el menú). */
static void fechar_aba_idx(int i)
{
    if (qtd_abas <= 1 || i < 0 || i >= qtd_abas) return;
    pf_limpar();
    aba_guardar();
    aba_t *a = &abas[i];
    if (a->motor) motor_fim(a->motor);
    libera(a->doc);
    int cur = aba_atual;
    for (int k = i; k < qtd_abas - 1; k++) abas[k] = abas[k + 1];
    qtd_abas--;
    memset(&abas[qtd_abas], 0, sizeof abas[0]);
    if (i == cur) {
        if (cur >= qtd_abas) cur = qtd_abas - 1;
        aba_atual = cur;
        aba_trocar(cur);
        recursos_pendentes = false;
        pintura_pendente = false;
    } else {
        if (i < cur) cur--;
        aba_atual = cur;
    }
    aba_titulo_janela();
    desenhar_barra();
    desenhar_abas();
    desenhar_pagina();
    desenhar_rodape();
    registrar("cipo: aba fechada, %d restantes\n", qtd_abas);
}

static void fechar_aba(void)
{
    fechar_aba_idx(aba_atual);
}

/* Arrastrar para reordenar: la pestaña i pasa a la posición k (la activa sigue siendo la misma). */
static void mover_aba(int i, int k)
{
    if (i == k || i < 0 || k < 0 || i >= qtd_abas || k >= qtd_abas) return;
    aba_guardar();
    aba_t t = abas[i];
    if (i < k) for (int m = i; m < k; m++) abas[m] = abas[m + 1];
    else for (int m = i; m > k; m--) abas[m] = abas[m - 1];
    abas[k] = t;
    if (aba_atual == i) aba_atual = k;
    else if (i < aba_atual && k >= aba_atual) aba_atual--;
    else if (i > aba_atual && k <= aba_atual) aba_atual++;
    registrar("cipo: aba %d movida a %d\n", i, k);
    desenhar_abas();
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
static bool pedido_por_js;       /* la última navegación la pidió un script (D487) */
static void definir_doc(char *corpo, int tam, bool html);

/* Pede outra pagina: guarda o endereco (resolvido contra a pagina actual)
 * para o bucle principal navegar quando for seguro. */
static void pedir_pagina(const char *url)
{
    if (!url || !url[0]) return;
    pedido_por_js = true;   /* viene de una página (script o meta refresh), no de la barra de direcciones */
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

/* ---- el script cambia la página (D271, D488): cipo_js.h ---- */
#include "cipo_js.h"

/* fetch y XMLHttpRequest (solo GET): la dirección puede ser relativa a la página. */
static char *js_baixar(const char *url, int *n)
{
    char abs[URL_MAX];
    *n = 0;
    if (!url || !juntar_url(url_atual, url, abs, sizeof abs)) return NULL;
    unsigned tam = 0;
    uint8_t *d = motor_baixar(abs, &tam);
    registrar("cipo: js: fetch %s (%u bytes)\n", abs, d ? tam : 0);
    if (!d) return NULL;
    *n = (int)tam;
    return (char *)d;
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
/* D488: la lista la escribe el motor (motor_dom_exportar), con todos los nodos:
 * antes eran los 256 primeros y 8 atributos por elemento. */
static char *dom_texto;
static int  dom_texto_tam;

static void dom_construir(void)
{
    if (dom_texto) { libera(dom_texto); dom_texto = NULL; }
    dom_texto_tam = 0;
    if (!motor) return;
    dom_texto = motor_dom_exportar(motor, -1, &dom_texto_tam);
    int n = 0;
    for (int i = 0; dom_texto && i < dom_texto_tam; i++) if (dom_texto[i] == 'N' && (i == 0 || dom_texto[i - 1] == '\n')) n++;
    registrar("cipo: js: el DOM tiene %d nodo(s)\n", n);
}

/* Corre os <script> da pagina. Cada pagina nova tem o seu motor: o script
 * de uma pagina nao pode ver as variaveis de outra. */
static void correr_scripts(void)
{
    if (!doc || !doc_html || !conf.js) return;
    if (js_geracao != js_geracao_rodada) {
        js_geracao_rodada = js_geracao;
        if (js) { qjs_destruir(js); js = NULL; }
        /* con designadores (D488): los ganchos nuevos de otras decisiones no se cruzan */
        qjs_ganchos_t g = { .titulo = js_titulo, .corpo = js_trocar_corpo, .alerta = js_alerta, .log = js_log,
                            .navegar = js_navegar, .atributo = js_atributo, .texto = js_texto, .baixar = js_baixar,
                            .dom_criar = js_dom_criar, .dom_inserir = js_dom_inserir, .dom_tirar = js_dom_tirar,
                            .dom_dado = js_dom_dado, .dom_html = js_dom_html, .caixa = js_caixa, .estilo = js_estilo,
                            .elemento_em = js_elemento_em, .janela = js_g_janela, .rolar = js_g_rolar,
                            .historia = js_historia, .ir = js_ir,
                            .firma = js_firma, .incremental = js_incremental,
                            .rolagem = js_rolagem, .rolar_caixa = js_rolar_caixa };
        js = qjs_criar(&g);
        if (!js) { registrar("cipo: js: sem memoria para o motor\n"); return; }
    }
    if (!js) return;
    qjs_definir_url(js, url_atual);
    qjs_definir_titulo(js, titulo);
    qjs_definir_corpo(js, "");
    qjs_tique(js, tempo_ms());     /* o relogio dos temporizadores */
    qjs_definir_janela(js, LARG - ROLAGEM, AREA_A);     /* innerWidth, innerHeight y matchMedia (D271) */
    js_dom_sujo = false;
    js_faixa_y0 = 1 << 30; js_faixa_y1 = -(1 << 30);
    js_layouts = js_layouts_inc = 0; js_ms_layout = js_ms_pintura = 0;
    js_rol_visto = -1; js_hist_pendente = 0;
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
    if (quantos) qjs_carga(js);     /* DOMContentLoaded y load (D271) */
    /* D488: el navegador ya no marca los elementos con manejador (el clic busca
     * el elemento bajo el puntero); lo que el script cambió se maqueta una vez. */
    if (motor && js_dom_sujo) {
        registrar("cipo: js: el script cambio la pagina\n");
        js_layout_sync();
        js_faixa_y0 = 1 << 30; js_faixa_y1 = -(1 << 30);   /* la página aún no está pintada */
    }
    js_rol_visto = rol_px;
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
static const uint32_t *motor_video(const char *url, int flags, int x, int y, int l, int a, int *fl, int *fa);

/* Se queda con el documento (toma la propiedad del búfer). */
static void definir_doc(char *corpo, int tam, bool html)
{
    video_parar();   /* documento nuevo: el vídeo del anterior se acaba */
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
    motor_ganchos_t g = { motor_baixar, motor_imagem, NULL, motor_video };
    motor = motor_novo(&g);
    ms_base = tempo_ms();   /* o relógio das animações começa a zero nesta página */
    if (motor && doc) {
        motor_zoom(motor, zoom_do_sitio(url_atual));   /* el zoom del sitio (D489) */
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
            recursos_pendentes = true; relayout_pendente = false; ult_relayout = tempo_ms(); est_repintados = est_apresentacoes = est_layouts = est_ms_pintura = est_trozos = 0;
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
    bool  arquivo;      /* el servidor manda un archivo que no se muestra: va al gestor de descargas */
    const char *erro;
    char  detalhe[200];
} baixa_t;

/* Descarga la dirección (siguiendo redirecciones). "pagina" = es la página que se
 * muestra (actualiza barra, pie y url_atual); si no, es una imagen. */
static mutex_t tls_trava = MUTEX_INICIAL;
static tls_config_t cfg;   /* las raíces de certificados, una sola vez para todos los hilos */
static void cert_resumo(tls_t *t, const char *host)
{
    const cert_t *c = NULL;
    cert_ok = false;
    cert_texto[0] = 0;
    if (tls_cadeia(t, &c) < 1 || !c) return;
    char suj[96], emi[96], d1[24], d2[24], hu[100];
    x509_nome_curto(c[0].sujeito, suj, sizeof suj);
    x509_nome_curto(c[0].emissor, emi, sizeof emi);
    x509_data_texto(c[0].antes, d1, sizeof d1);
    x509_data_texto(c[0].depois, d2, sizeof d2);
    x509_huella(&c[0], hu, sizeof hu);
    cert_ok = tls_validacao(t) == X509_OK;
    formatar_texto(cert_texto, sizeof cert_texto, "Sitio: %s\nEmitido para: %s\nEmitido por: %s\nVálido: %s a %s\nConexión: %s\nSHA-256: %s",
                   host, suj, emi, d1, d2, tls_versao_texto(t), hu);
}

/* D487: el pedido de red completo (método, cuerpo, cookies, caché, descompresión, redirecciones).
 * baixar() es el GET de siempre; los formularios POST y el JavaScript entran por baixar_ex(). */
#define PEDIDO_MAX 16384
static char nav_ref[URL_MAX];      /* página desde la que se hizo clic (Referer/SameSite de la navegación) */
static bool nav_ref_valido;

static void baixar_int(char *url, const rede_pedido_t *q, bool pagina, baixa_t *b, rede_resposta_t *extra,
                       http_resposta_t *rr, conexao_t *cn, bool *tem_trava, char *pedido);
static void baixar_ex(char *url, const rede_pedido_t *q, bool pagina, baixa_t *b, rede_resposta_t *extra)
{
    http_resposta_t *rr = aloca_zerado(sizeof *rr);
    conexao_t *cn = aloca_zerado(sizeof *cn);
    char *pedido = aloca(PEDIDO_MAX);
    bool trava = false;
    if (!rr || !cn || !pedido) { memset(b, 0, sizeof *b); b->erro = "Sin memoria"; libera(rr); libera(cn); libera(pedido); return; }
    baixar_int(url, q, pagina, b, extra, rr, cn, &trava, pedido);
    if (trava) mutex_largar(&tls_trava);
    libera(rr);
    libera(cn);
    libera(pedido);
}

static void baixar(char *url, bool pagina, int maximo, baixa_t *b)
{
    rede_pedido_t q;
    memset(&q, 0, sizeof q);
    q.metodo = "GET";
    q.url = url;
    q.maximo = maximo;
    q.flags = pagina ? (RP_PAGINA | RP_NAVEGACAO) : 0;
    q.iniciador = pagina ? (nav_ref_valido ? nav_ref : NULL) : url_atual;
    baixar_ex(url, &q, pagina, b, NULL);
}

/* D487: la página que se pide puede ser un POST (formulario); lo prepara navegar_envio(). */
static char *post_corpo;
static int post_n;
static char post_tipo[100];
static bool nav_clique;     /* la navegación viene de un clic o de un envío: lleva Referer */

static void baixar_pagina(char *url, baixa_t *b)
{
    rede_pedido_t q;
    memset(&q, 0, sizeof q);
    q.metodo = post_corpo ? "POST" : "GET";
    q.url = url;
    q.maximo = PAGINA_MAX;
    q.flags = RP_PAGINA | RP_NAVEGACAO | (post_corpo ? RP_SEM_CACHE : 0);
    q.corpo = (const uint8_t *)post_corpo;
    q.ncorpo = post_n;
    q.tipo_corpo = post_tipo[0] ? post_tipo : NULL;
    nav_ref_valido = nav_clique && nav_ref[0];
    q.iniciador = nav_ref_valido ? nav_ref : NULL;
    baixar_ex(url, &q, true, b, NULL);
    libera(post_corpo);
    post_corpo = NULL; post_n = 0; post_tipo[0] = 0;
    nav_clique = false; nav_ref_valido = false;
}

/* Ejecuta el pedido de un script (fetch, XMLHttpRequest): cualquier método, cabeceras y cuerpo, con
 * CORS sencillo (D487). Devuelve 0 con la respuesta, o -1 si no se pudo (red caída, CORS, memoria). */
static int rede_js_executor(const rede_pedido_t *q, rede_resposta_t *r)
{
    char url[URL_MAX];
    copia_texto(url, q->url, sizeof url);
    baixa_t *b = aloca_zerado(sizeof *b);
    if (!b) { r->erro = "Sin memoria"; return -1; }
    bool entre = q->iniciador && q->iniciador[0] && !rede_mesma_origem(url, q->iniciador) && !(q->flags & RP_NO_CORS);
    bool cred = (q->flags & RP_CREDENCIAIS) != 0;
    rede_pedido_t qq = *q;
    qq.flags |= RP_BRUTO;
    qq.maximo = q->maximo > 0 ? q->maximo : IMG_MAX_BYTES;
    if (entre) {
        qq.flags |= RP_CORS;
        const char *m = q->metodo ? q->metodo : "GET";
        if (rede_cors_precisa_preparacao(m, q->cabs)) {   /* petición previa OPTIONS */
            char cab[512], nomes[300] = "";
            for (const char *l = q->cabs ? q->cabs : ""; *l;) {
                const char *fin = acha_char(l, '\n');
                const char *dp = l;
                while (*dp && *dp != ':' && *dp != '\n') dp++;
                int nn = (int)(dp - l);
                if (*dp == ':' && nn > 0 && nn < 60 && !rede_igual_ci(l, "accept") && !rede_comeca_ci(l, "accept:")) {
                    if (nomes[0]) junta_texto(nomes, ",", sizeof nomes);
                    char t[64]; memcpy(t, l, (size_t)nn); t[nn] = 0;
                    for (char *c = t; *c; c++) if (*c >= 'A' && *c <= 'Z') *c = (char)(*c + 32);
                    junta_texto(nomes, t, sizeof nomes);
                }
                if (!fin) break;
                l = fin + 1;
            }
            formatar_texto(cab, sizeof cab, "Access-Control-Request-Method: %s\r\n%s%s%s", m, nomes[0] ? "Access-Control-Request-Headers: " : "", nomes, nomes[0] ? "\r\n" : "");
            rede_pedido_t po;
            memset(&po, 0, sizeof po);
            po.metodo = "OPTIONS"; po.url = url; po.cabs = cab; po.iniciador = q->iniciador; po.maximo = 65536;
            po.flags = RP_BRUTO | RP_CORS | RP_SEM_CACHE | RP_SEM_CODIF;
            rede_resposta_t pr;
            memset(&pr, 0, sizeof pr);
            char u2[URL_MAX];
            copia_texto(u2, url, sizeof u2);
            baixar_ex(u2, &po, false, b, &pr);
            bool ok = !b->erro && b->codigo >= 200 && b->codigo < 300 && pr.cab && rede_cors_permite(pr.cab, q->iniciador, cred);
            libera(b->corpo); libera(pr.cab);
            if (!ok) { r->cors_bloqueado = true; r->erro = "CORS"; libera(b); return -1; }
            memset(b, 0, sizeof *b);
        }
    }
    baixar_ex(url, &qq, false, b, r);
    r->codigo = b->codigo;
    r->corpo = (uint8_t *)b->corpo;
    r->tam = b->tam;
    r->truncado = b->truncado;
    r->erro = b->erro;
    copia_texto(r->detalhe, b->detalhe, sizeof r->detalhe);
    if (!r->url[0]) copia_texto(r->url, url, sizeof r->url);
    libera(b);
    if (!r->erro && entre && r->cab && !rede_cors_permite(r->cab, q->iniciador, cred)) {
        r->cors_bloqueado = true;
        r->erro = "CORS";
        libera(r->corpo); r->corpo = NULL; r->tam = 0;
    }
    return r->erro ? -1 : 0;
}

/* Entrega lo que había en la caché como si viniera de la red. */
static bool baixa_da_cache(const char *url, baixa_t *b, rede_resposta_t *extra, bool pagina)
{
    int n = 0;
    char *cab = NULL;
    uint8_t *c = rede_cache_ler(url, &n, &cab);
    if (!c) return false;
    b->corpo = (char *)c;
    b->tam = n;
    b->codigo = 200;
    char tipo[80] = "text/html";
    rede_cab_valor(cab, "Content-Type", tipo, sizeof tipo);
    for (char *p = tipo; *p; p++) *p = minuscula(*p);
    b->html = comeca_com(tipo, "text/html") || comeca_com(tipo, "application/xhtml");
    (void)pagina;
    if (extra) { extra->cab = cab; extra->de_cache = true; extra->codigo = 200; }
    else libera(cab);
    registrar("cipo: cache: %s (%d bytes)\n", url, n);
    return true;
}

static void baixar_int(char *url, const rede_pedido_t *q, bool pagina, baixa_t *b, rede_resposta_t *extra,
                       http_resposta_t *rr, conexao_t *cn, bool *tem_trava, char *pedido)
{
    memset(b, 0, sizeof *b);
    b->html = true;
    const bool principal = thread_id() == meu_pid();   /* solo el hilo principal toca la ventana (D373) */
    const bool bruto = (q->flags & RP_BRUTO) != 0;
    const int maximo = q->maximo;
    char metodo[12];
    copia_texto(metodo, q->metodo && q->metodo[0] ? q->metodo : "GET", sizeof metodo);
    for (char *p = metodo; *p; p++) if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);
    const uint8_t *corpo_env = q->corpo;
    int ncorpo = q->ncorpo;
    const char *tipo_corpo = q->tipo_corpo;
    rede_pedido_t qq = *q;
    qq.metodo = metodo;
    for (int redirecoes = 0; redirecoes < 15; redirecoes++) {
        url_t u;
        if (!url_separar(url, &u)) { b->erro = "Dirección no válida"; return; }
        if (!pagina) registrar("cipo: baixando %s\n", url);
        const bool get = compara_texto(metodo, "GET") == 0;
        char etag[100] = "", lm[48] = "";
        int est = CACHE_NADA;
        if (get && !(q->flags & RP_SEM_CACHE)) est = rede_cache_consultar(url, etag, sizeof etag, lm, sizeof lm);
        if (est == CACHE_FRESCA && baixa_da_cache(url, b, extra, pagina)) {
            if (extra) copia_texto(extra->url, url, sizeof extra->url);
            if (pagina && !b->html && !bruto) {   /* un archivo que no se muestra: lo resuelve la ruta normal, sin caché */
                libera(b->corpo); b->corpo = NULL; b->tam = 0;
            } else return;
        }
        char st[160];
        formatar_texto(st, sizeof st, "%s %s%s...", pagina ? "Conectando con" : "Descargando imagen de", u.host, u.https ? " (HTTPS)" : "");
        if (principal) rodape(st);
        /* HTTPS (D062): la misma lectura, por encima del TLS. Certificado
         * malo = página de error con el motivo; no hay "continuar de todos modos". */
        if (u.https && !*tem_trava) { mutex_tomar(&tls_trava); *tem_trava = true; }   /* la TLS no es reentrante: una conexión HTTPS a la vez */
        char e2[160];
        if (u.https && !cfg.raizes && !tls_config_padrao(&cfg, NULL, e2, sizeof e2)) {
            b->erro = "No se pudo preparar HTTPS";
            copia_texto(b->detalhe, e2, sizeof b->detalhe);
            return;
        }
        int rc = conexao_abrir(cn, &u, &cfg);
        if (rc < 0) {
            b->erro = rc == TLS_ERRO_CERT ? "Conexión no segura" : "No se pudo abrir la página";
            formatar_texto(b->detalhe, sizeof b->detalhe, "%s: %s.", u.host, cn->erro);
            conexao_fechar(cn);
            return;
        }
        if (pagina && u.https && cn->tls) cert_resumo(cn->tls, u.host);
        int np = rede_montar_pedido(&qq, metodo, &u, url, tipo_corpo, ncorpo, est == CACHE_VELHA ? etag : NULL,
                                    est == CACHE_VELHA ? lm : NULL, pedido, PEDIDO_MAX);
        if (np < 0) { conexao_fechar(cn); b->erro = "Petición demasiado larga"; return; }
        if (conexao_escrever(cn, pedido, (unsigned)np) < 0 ||
            (ncorpo > 0 && conexao_escrever(cn, corpo_env, (unsigned)ncorpo) < 0) ||
            http_ler_cabecalho_de(rr, conexao_ler, cn) < 0) {
            conexao_fechar(cn);
            b->erro = "Respuesta no válida del servidor";
            return;
        }
        if (compara_texto(metodo, "HEAD") == 0) rr->acabou = true;
        if (!(q->flags & RP_SEM_COOKIES)) rede_cookies_da_resposta(url, rr->cabecalho);   /* todos los Set-Cookie, también los de las redirecciones */
        b->codigo = rr->codigo;
        if (b->codigo == 304 && est == CACHE_VELHA) {   /* no ha cambiado: sirve la copia */
            conexao_fechar(cn);
            rede_cache_renovar(url, rr->cabecalho);
            if (baixa_da_cache(url, b, extra, pagina)) { if (extra) copia_texto(extra->url, url, sizeof extra->url); return; }
        }
        char destino[URL_MAX];
        if (b->codigo >= 300 && b->codigo < 400 && b->codigo != 304 && !(q->flags & RP_MANUAL) &&
            http_campo(rr->cabecalho, "Location", destino, sizeof destino)) {
            conexao_fechar(cn);
            char nova[URL_MAX];
            if (!juntar_url(url, destino, nova, sizeof nova)) { b->erro = "Redirección no válida"; return; }
            if (!rede_redirecao(rr->codigo, metodo, sizeof metodo)) { ncorpo = 0; corpo_env = NULL; tipo_corpo = NULL; }
            if (extra) extra->redirigido = true;
            copia_texto(url, nova, URL_MAX);
            if (pagina) {
                copia_texto(url_atual, url, sizeof url_atual);
                caixa_definir(&endereco, url);
                desenhar_barra();
            }
            continue;
        }
        char tipo[80] = "text/html";
        http_campo(rr->cabecalho, "Content-Type", tipo, sizeof tipo);
        for (char *p = tipo; *p; p++) *p = minuscula(*p);
        b->html = comeca_com(tipo, "text/html") || comeca_com(tipo, "application/xhtml");
        if (pagina && !bruto && !b->html && !comeca_com(tipo, "text/")) {
            conexao_fechar(cn);
            b->arquivo = true;   /* no es una página: lo recoge el gestor de descargas (D373) */
            copia_texto(b->detalhe, tipo, sizeof b->detalhe);
            return;
        }
        if (!pagina && !bruto && b->codigo != 200) {
            conexao_fechar(cn);
            b->erro = "Imagen no disponible";
            return;
        }
        int cap = 65536;
        b->corpo = aloca((size_t)cap);
        if (!b->corpo) { conexao_fechar(cn); b->erro = "Sin memoria"; return; }
        int n, mostrado = 0;
        while ((n = http_ler_corpo(rr, b->corpo + b->tam, cap - b->tam)) > 0) {
            b->tam += n;
            if (b->tam == cap) {
                if (cap >= maximo) { b->truncado = true; break; }
                int nova = cap * 2;
                if (nova > maximo) nova = maximo;
                char *qn = realoca(b->corpo, (size_t)nova);
                if (!qn) { b->erro = "Sin memoria"; b->truncado = true; break; }
                b->corpo = qn;
                cap = nova;
            }
            if (pagina && b->tam / 16384 != mostrado) {
                mostrado = b->tam / 16384;
                formatar_texto(st, sizeof st, "Cargando %s... %d KB", u.host, b->tam / 1024);
                if (principal) rodape(st);
            }
        }
        if (n < 0) b->erro = "Error al leer la respuesta";
        conexao_fechar(cn);
        /* gzip, deflate y brotli (D487) */
        if (b->corpo && b->tam > 0 && !b->truncado) {
            const char *erro_dec = NULL;
            int tn = b->tam;
            uint8_t *nb = rede_decodificar(rr->cabecalho, (uint8_t *)b->corpo, &tn, maximo, &erro_dec);
            b->corpo = (char *)nb;
            b->tam = tn;
            if (erro_dec && !nb) b->erro = erro_dec;
        } else if (b->corpo && b->truncado && b->tam > 0) {
            char ce[40];
            if (http_campo(rr->cabecalho, "Content-Encoding", ce, sizeof ce) && compara_texto(ce, "identity") != 0) {
                const char *erro_dec = NULL;   /* flujo comprimido cortado: se descomprime lo que haya */
                int tn = b->tam;
                uint8_t *nb = rede_decodificar(rr->cabecalho, (uint8_t *)b->corpo, &tn, maximo, &erro_dec);
                b->corpo = (char *)nb; b->tam = tn;
            }
        }
        if (get && !b->erro && !b->truncado && b->corpo && b->codigo == 200 && !(q->flags & RP_SEM_CACHE) && !q->corpo)
            rede_cache_guardar(url, b->codigo, rr->cabecalho, (uint8_t *)b->corpo, b->tam);
        if (extra) {
            extra->codigo = b->codigo;
            extra->cab = aloca((size_t)tam_texto(rr->cabecalho) + 1);
            if (extra->cab) memcpy(extra->cab, rr->cabecalho, tam_texto(rr->cabecalho) + 1);
            copia_texto(extra->url, url, sizeof extra->url);
        }
        return;
    }
    b->erro = "Demasiadas redirecciones";
}

static uint8_t *motor_baixar(const char *url, unsigned *n)
{
    *n = 0;
    bool achou;
    uint8_t *previo = pf_pegar(url, n, &achou);   /* ya lo bajó un hilo (D373) */
    if (achou) return previo;
    char u[URL_MAX];
    copia_texto(u, url, sizeof u);
    static baixa_t b;
    baixar(u, false, IMG_MAX_BYTES, &b);
    if (b.erro || b.truncado || !b.corpo || b.tam <= 0) { libera(b.corpo); return NULL; }
    *n = (unsigned)b.tam;
    return (uint8_t *)b.corpo;
}

/* ------------------------------------------------------------ <video> (mejora 100, D276)
 *
 * Un solo vídeo activo a la vez (el decodificador de la libsagui lleva estado
 * estático): el primero que pide la pintura. El MP4 se baja entero (hasta
 * VIDEO_MAX_BYTES; mientras baja, la ventana no responde) y se decodifica en el
 * bucle de eventos con el mismo decodificador que usa el reproductor del
 * sistema (H.264 Baseline/Main y AAC-LC, video.h). Los cuadros nuevos se
 * escriben directo en la caja del vídeo, sin repintar la página; si la página se
 * repinta (scroll, tamaño), el motor pide el cuadro actual por motor_video().
 * Si el equipo no llega a tiempo real se saltan cuadros (sin convertir color) y el
 * sonido manda, como en el reproductor. Un clic en el vídeo lo pausa o lo reanuda. */
#define VIDEO_MAX_BYTES (48 * 1024 * 1024)
/* vid: el vídeo activo (NULL = ninguno), declarado arriba */
static uint8_t *vid_dados;            /* el MP4 entero en memoria */
static char vid_url[URL_MAX];         /* de dónde se bajó el activo */
static char vid_quer[URL_MAX];        /* el que pidió la última pintura */
static char vid_falha[URL_MAX];       /* el que no se pudo abrir (no se reintenta) */
static int vid_flags, vid_px, vid_py, vid_pl, vid_pa;   /* banderas y caja en la página */
static imagem_t vid_img;              /* el último cuadro */
static bool vid_tocando, vid_fim;
static int vid_fps = 25, vid_prox;    /* cuadro que toca decodificar */
static int vid_audio = -1;
static uint32_t vid_taxa, vid_ms_escritos;
static unsigned vid_t0;
static int16_t vid_pcm[1024 * 2];
static int vid_pcm_n, vid_pcm_pos;    /* audio decodificado pendiente de escribir (cuadros estéreo) */
static unsigned vid_mostrados, vid_saltados, vid_ms_decod;

static bool med_audio;                /* el medio activo es un <audio> (D489) */
static const uint32_t *motor_video(const char *url, int flags, int x, int y, int l, int a, int *fl, int *fa)
{
    if (!vid_quer[0]) copia_texto(vid_quer, url, sizeof vid_quer);
    if (compara_texto(vid_quer, url) != 0) return NULL;   /* solo el primero */
    vid_flags = flags;
    vid_px = x; vid_py = y; vid_pl = l; vid_pa = a;
    med_audio = (flags & VIDEO_AUDIO) != 0;
    if (!med_audio && vid && vid_img.px && compara_texto(vid_url, url) == 0) { *fl = vid_img.larg; *fa = vid_img.alt; return vid_img.px; }
    return NULL;
}

static unsigned video_relogio(void)
{
    if (vid_audio < 0) return tempo_ms() - vid_t0;
    int pend = audio_pendente(vid_audio);
    if (pend < 0) pend = 0;
    return vid_ms_escritos > (uint32_t)pend ? vid_ms_escritos - (uint32_t)pend : 0;
}

static void video_abrir_som(void)
{
    vid_audio = -1;
    vid_pcm_n = vid_pcm_pos = 0;
    vid_ms_escritos = 0;
    const video_info_t *inf = video_info(vid);
    if (!inf->tem_audio || (vid_flags & VIDEO_MUTED)) return;
    vid_taxa = inf->taxa_audio;
    vid_audio = audio_abrir((int)vid_taxa, 2, 16);
    if (vid_audio < 0) vid_audio = -1;
}

/* Escribe el audio que toca, sin bloquear la ventana. */
static void video_escrever_som(void)
{
    if (vid_audio < 0) return;
    for (int vueltas = 0; vueltas < 8; vueltas++) {
        if (vid_pcm_pos >= vid_pcm_n) {
            vid_pcm_n = video_audio(vid, vid_pcm, 1024);
            vid_pcm_pos = 0;
            if (vid_pcm_n <= 0) { vid_pcm_n = 0; return; }
        }
        int r = audio_escrever_sem_bloquear(vid_audio, vid_pcm + vid_pcm_pos * 2, (unsigned)(vid_pcm_n - vid_pcm_pos) * 4);
        if (r <= 0) return;
        int cuadros = r / 4;
        vid_pcm_pos += cuadros;
        vid_ms_escritos += (uint32_t)cuadros * 1000u / vid_taxa;
    }
}

static void audio_solo_parar(void);
static void video_parar(void)
{
    audio_solo_parar();
    if (vid_audio >= 0) { audio_fechar(vid_audio, 0); vid_audio = -1; }
    if (vid) {
        registrar("cipo: video fin: %u cuadros mostrados, %u saltados, %u ms decodificando\n", vid_mostrados, vid_saltados, vid_ms_decod);
        video_fechar(vid);
        vid = NULL;
    }
    if (vid_img.px) imagem_liberar(&vid_img);
    libera(vid_dados);
    vid_dados = NULL;
    vid_url[0] = vid_quer[0] = vid_falha[0] = 0;
    vid_tocando = vid_fim = false;
    vid_mostrados = vid_saltados = vid_ms_decod = 0;
}

/* Pinta el cuadro actual en la ventana, en la caja del vídeo (la misma cuenta de ajuste que el motor). */
static void video_blit(void)
{
    if (!vid || !vid_img.px || pintura_pendente || vid_pl <= 0 || vid_pa <= 0) return;
    int tl = vid_img.larg, ta = vid_img.alt, bl, ba;
    if ((long)tl * vid_pa > (long)ta * vid_pl) { bl = vid_pl; ba = (int)((long)ta * vid_pl / tl); }
    else { ba = vid_pa; bl = (int)((long)tl * vid_pa / ta); }
    if (bl < 1 || ba < 1) return;
    int bx = vid_px + (vid_pl - bl) / 2;
    int by = ABA_Y + vid_py + (vid_pa - ba) / 2 - rol_px;
    int x0 = bx < 0 ? 0 : bx, x1 = bx + bl > LARG - ROLAGEM ? LARG - ROLAGEM : bx + bl;
    int y0 = by < ABA_Y ? ABA_Y : by, y1 = by + ba > ABA_Y + AREA_A ? ABA_Y + AREA_A : by + ba;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++) {
        const uint32_t *src = vid_img.px + ((yy - by) * ta / ba) * tl;
        uint32_t *d = j->px + yy * j->larg;
        for (int xx = x0; xx < x1; xx++) d[xx] = src[(xx - bx) * tl / bl];
    }
    if (!vid_tocando) {   /* en pausa: un triangulo de "play" blanco en el centro */
        int cx = bx + bl / 2, cy = by + ba / 2;
        for (int k = -14; k <= 14; k++) {
            int w = 14 - (k < 0 ? -k : k);
            int yy = cy + k;
            if (yy >= y0 && yy < y1 && cx - 6 >= x0 && cx - 6 + w < x1) g_retangulo(j, cx - 6, yy, w + 1, 1, 0xFFFFFF);
        }
    }
    jan_mostrar_area(j, x0, y0, x1 - x0, y1 - y0);
    media_controles_pintar();
}

/* ---- <audio> y controles de <video>/<audio> (D489) ----
 * El audio sin imagen se baja entero, se guarda en /tmp (audio_arquivo_abrir lee de un archivo) y lo
 * decodifica la libsagui (WAV, MP3, FLAC) a la tarjeta de sonido, sin bloquear la ventana. Si el
 * elemento lleva `controls`, el navegador dibuja encima de la caja una barra con reproducir/pausa, el
 * tiempo, la barra de progreso (un clic salta) y el volumen. Un solo medio activo a la vez. */
#define CTL_A 28
static audio_arquivo_t *aud;
static char aud_ruta[48];
static uint32_t aud_dur_ms;
static int med_vol = 100;
/* áreas de la barra, en coordenadas de la ventana (lo calcula media_controles_pintar) */
static int ctl_vis, ctl_x, ctl_y, ctl_l, ctl_prog0, ctl_prog1, ctl_vol0, ctl_vol1;

static void audio_solo_parar(void)
{
    if (aud) { audio_arquivo_fechar(aud); aud = NULL; }
    if (aud_ruta[0]) { apagar(aud_ruta); aud_ruta[0] = 0; }
    ctl_vis = 0;
}

static unsigned med_pos_ms(void)
{
    if (med_audio) return aud ? video_relogio() : 0;
    return vid ? (unsigned)((long long)vid_prox * 1000 / (vid_fps > 0 ? vid_fps : 25)) : 0;
}
static unsigned med_dur_ms(void)
{
    if (med_audio) return aud_dur_ms;
    if (!vid) return 0;
    const video_info_t *inf = video_info(vid);
    return inf->duracao_ms ? inf->duracao_ms : (unsigned)((long long)inf->quadros * 1000 / (vid_fps > 0 ? vid_fps : 25));
}

static void media_controles_pintar(void)
{
    ctl_vis = 0;
    if (!(vid_flags & VIDEO_CONTROLS) || !vid_quer[0] || pintura_pendente || painel) return;
    if (med_audio ? aud == NULL : vid == NULL) return;
    int wx = vid_px, wy = ABA_Y + vid_py - rol_px;
    int y = med_audio ? wy + (vid_pa - CTL_A) / 2 : wy + vid_pa - CTL_A;
    if (vid_pl < 120 || vid_pa < CTL_A || y < ABA_Y || y + CTL_A > ABA_Y + AREA_A || wx < 0 || wx + vid_pl > LARG - ROLAGEM) return;
    uint32_t fondo = med_audio ? 0xF1F3F4 : 0x202428, fg = med_audio ? 0x202428 : 0xFFFFFF, pista = med_audio ? 0xC4CBD2 : 0x59626B, resalta = 0x1A73E8;
    ctl_x = wx; ctl_y = y; ctl_l = vid_pl; ctl_vis = 1;
    g_retangulo(j, wx, y, vid_pl, CTL_A, fondo);
    /* reproducir / pausa */
    if (vid_tocando && !vid_fim) { g_retangulo(j, wx + 9, y + 7, 4, 14, fg); g_retangulo(j, wx + 16, y + 7, 4, 14, fg); }
    else for (int k = -7; k <= 7; k++) { int w = 8 - (k < 0 ? -k : k) * 8 / 7; if (w > 0) g_retangulo(j, wx + 10, y + 14 + k, w + 1, 1, fg); }
    int x = wx + 30, fin = wx + vid_pl - 8;
    unsigned pos = med_pos_ms(), dur = med_dur_ms();
    if (dur && pos > dur) pos = dur;
    if (vid_pl >= 260) {
        char t[32];
        formatar_texto(t, sizeof t, "%u:%02u / %u:%02u", pos / 60000, (pos / 1000) % 60, dur / 60000, (dur / 1000) % 60);
        g_texto(j, x, y + 6, t, fg);
        x += (int)tam_texto(t) * LETRA_L + 8;
    }
    /* volumen a la derecha */
    ctl_vol0 = ctl_vol1 = 0;
    if (vid_pl >= 200) {
        int vx = fin - 48;
        g_retangulo(j, vx - 18, y + 11, 5, 6, fg); g_retangulo(j, vx - 13, y + 8, 4, 12, fg);   /* altavoz */
        g_retangulo(j, vx, y + 13, 48, 2, pista);
        g_retangulo(j, vx, y + 13, 48 * med_vol / 100, 2, resalta);
        g_retangulo(j, vx + 48 * med_vol / 100 - 2, y + 9, 5, 10, fg);
        ctl_vol0 = vx; ctl_vol1 = vx + 48;
        fin = vx - 26;
    }
    /* progreso */
    ctl_prog0 = x; ctl_prog1 = fin;
    if (fin - x > 20) {
        g_retangulo(j, x, y + 13, fin - x, 3, pista);
        int w = dur ? (int)((long long)(fin - x) * pos / dur) : 0;
        g_retangulo(j, x, y + 13, w, 3, resalta);
        g_retangulo(j, x + w - 3, y + 9, 7, 11, fg);
    }
    jan_mostrar_area(j, wx, y, vid_pl, CTL_A);
}

/* Salta a ms dentro del medio. */
static void media_buscar(unsigned ms)
{
    unsigned dur = med_dur_ms();
    if (dur && ms > dur) ms = dur;
    if (med_audio) {
        if (!aud) return;
        audio_arquivo_buscar(aud, ms);
    } else {
        if (!vid) return;
        int fps = vid_fps > 0 ? vid_fps : 25;
        int cuadro = (int)((long long)ms * fps / 1000);
        video_buscar(vid, cuadro);
        vid_prox = cuadro;
        if (video_quadro(vid, &vid_img) == 0) { vid_prox = cuadro + 1; }
    }
    vid_t0 = tempo_ms() - ms;
    vid_fim = false;
    if (vid_audio >= 0) { audio_fechar(vid_audio, 0); vid_audio = -1; }
    if (vid_tocando) {
        if (med_audio) { vid_audio = audio_abrir((int)vid_taxa, 2, 16); if (vid_audio >= 0) audio_volume(vid_audio, med_vol); vid_pcm_n = vid_pcm_pos = 0; vid_ms_escritos = 0; }
        else video_abrir_som();
        if (vid_audio >= 0) { vid_ms_escritos = ms; audio_volume(vid_audio, med_vol); }
    }
    registrar("cipo: medio: salto a %u ms\n", ms);
}

/* Un clic en la barra de controles; true = era de la barra. */
static bool media_clique(int x, int y)
{
    if (!ctl_vis || x < ctl_x || x >= ctl_x + ctl_l || y < ctl_y || y >= ctl_y + CTL_A) return false;
    if (x < ctl_x + 30) {                 /* reproducir / pausa */
        if (vid_fim) { media_buscar(0); vid_fim = false; }
        vid_tocando = !vid_tocando;
        if (vid_tocando) {
            vid_t0 = tempo_ms() - med_pos_ms();
            if (vid_audio < 0) {
                if (med_audio) { vid_audio = audio_abrir((int)vid_taxa, 2, 16); if (vid_audio >= 0) { audio_volume(vid_audio, med_vol); vid_ms_escritos = med_pos_ms(); } }
                else video_abrir_som();
            }
        }
        registrar("cipo: medio %s\n", vid_tocando ? "reanudado" : "en pausa");
    } else if (ctl_vol1 && x >= ctl_vol0 - 4 && x <= ctl_vol1 + 4) {     /* volumen */
        med_vol = (x - ctl_vol0) * 100 / (ctl_vol1 - ctl_vol0);
        if (med_vol < 0) med_vol = 0;
        if (med_vol > 100) med_vol = 100;
        if (vid_audio >= 0) audio_volume(vid_audio, med_vol);
        registrar("cipo: medio: volumen %d\n", med_vol);
    } else if (x >= ctl_prog0 && x <= ctl_prog1 && ctl_prog1 > ctl_prog0) {      /* barra de progreso */
        unsigned dur = med_dur_ms();
        media_buscar((unsigned)((long long)(x - ctl_prog0) * dur / (ctl_prog1 - ctl_prog0)));
    }
    media_controles_pintar();
    return true;
}

/* Audio sin imagen: baja el archivo, lo abre y deja el sonido listo (suena si lleva autoplay). */
static void audio_iniciar(const char *url)
{
    static baixa_t ab;
    char u[URL_MAX];
    copia_texto(u, url, sizeof u);
    rodape("Descargando audio...");
    baixar(u, false, 64 * 1024 * 1024, &ab);
    if (ab.erro || ab.truncado || !ab.corpo || ab.tam < 32) {
        registrar("cipo: audio: no se pudo bajar (%s%s)\n", ab.erro ? ab.erro : "", ab.truncado ? ", demasiado grande" : "");
        libera(ab.corpo);
        ab.corpo = NULL;
        copia_texto(vid_falha, url, sizeof vid_falha);
        rodape("No se pudo descargar el audio");
        return;
    }
    formatar_texto(aud_ruta, sizeof aud_ruta, "/tmp/cipo-audio-%u.dat", tempo_ms() & 0xFFFF);
    int fd = abrir(aud_ruta, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd >= 0) { escrever_tudo(fd, ab.corpo, (unsigned)ab.tam); fechar(fd); }
    libera(ab.corpo);
    ab.corpo = NULL;
    int erro = 0;
    aud = fd >= 0 ? audio_arquivo_abrir(aud_ruta, &erro) : NULL;
    if (!aud) {
        registrar("cipo: audio: %s\n", fd >= 0 ? audio_erro_texto(erro) : "no se pudo guardar en /tmp");
        if (aud_ruta[0]) { apagar(aud_ruta); aud_ruta[0] = 0; }
        copia_texto(vid_falha, url, sizeof vid_falha);
        rodape("Formato de audio no reconocido");
        return;
    }
    const audio_info_t *inf = audio_arquivo_info(aud);
    copia_texto(vid_url, url, sizeof vid_url);
    aud_dur_ms = inf->duracao_ms;
    vid_taxa = inf->taxa;
    vid_fim = false;
    vid_pcm_n = vid_pcm_pos = 0;
    vid_ms_escritos = 0;
    vid_audio = -1;
    registrar("cipo: audio %s %u Hz %u canales %u ms\n", inf->nome_formato, inf->taxa, inf->canais, inf->duracao_ms);
    vid_tocando = (vid_flags & VIDEO_AUTOPLAY) != 0;
    vid_t0 = tempo_ms();
    if (vid_tocando && !(vid_flags & VIDEO_MUTED)) {
        vid_audio = audio_abrir((int)vid_taxa, 2, 16);
        if (vid_audio >= 0) audio_volume(vid_audio, med_vol);
    }
    rodape("");
    desenhar_pagina();
}

/* Un paso del audio sin imagen: escribe lo que cabe en la tarjeta. */
static void audio_tick(void)
{
    static unsigned ult_pintura;
    if (!aud) return;
    if (vid_tocando && !vid_fim) {
        for (int vueltas = 0; vueltas < 8; vueltas++) {
            if (vid_pcm_pos >= vid_pcm_n) {
                int16_t tmp[1024 * 2];
                int canais = (int)audio_arquivo_info(aud)->canais;
                int n = audio_arquivo_ler(aud, tmp, 1024);
                vid_pcm_pos = 0; vid_pcm_n = 0;
                if (n <= 0) {      /* fin del archivo: cuando se vacía la tarjeta, termina o vuelve a empezar */
                    if (vid_audio < 0 || audio_pendente(vid_audio) <= 0) {
                        if (vid_flags & VIDEO_LOOP) { media_buscar(0); }
                        else {
                            vid_tocando = false; vid_fim = true;
                            if (vid_audio >= 0) { audio_fechar(vid_audio, 0); vid_audio = -1; }
                            registrar("cipo: audio terminado\n");
                        }
                    }
                    break;
                }
                for (int i = 0; i < n; i++) {      /* siempre estéreo hacia la tarjeta */
                    vid_pcm[i * 2] = tmp[i * canais];
                    vid_pcm[i * 2 + 1] = tmp[i * canais + (canais > 1 ? 1 : 0)];
                }
                vid_pcm_n = n;
            }
            if (vid_audio < 0) {      /* sin tarjeta (o muted): el reloj corre solo */
                vid_pcm_pos = vid_pcm_n;
                if ((int)tempo_ms() - (int)vid_t0 < (int)(vid_ms_escritos + 1)) break;
                vid_ms_escritos += (uint32_t)vid_pcm_n * 1000u / (vid_taxa ? vid_taxa : 44100);
                continue;
            }
            int r = audio_escrever_sem_bloquear(vid_audio, vid_pcm + vid_pcm_pos * 2, (unsigned)(vid_pcm_n - vid_pcm_pos) * 4);
            if (r <= 0) break;
            int cuadros = r / 4;
            vid_pcm_pos += cuadros;
            vid_ms_escritos += (uint32_t)cuadros * 1000u / (vid_taxa ? vid_taxa : 44100);
        }
    }
    if (tempo_ms() - ult_pintura >= 250 && ctl_vis) { ult_pintura = tempo_ms(); media_controles_pintar(); }
}

static void video_iniciar(const char *url)
{
    static baixa_t vb;
    char u[URL_MAX];
    copia_texto(u, url, sizeof u);
    rodape("Descargando vídeo...");
    baixar(u, false, VIDEO_MAX_BYTES, &vb);
    if (vb.erro || vb.truncado || !vb.corpo || vb.tam < 32) {
        registrar("cipo: video: no se pudo bajar (%s%s)\n", vb.erro ? vb.erro : "", vb.truncado ? ", demasiado grande" : "");
        libera(vb.corpo);
        vb.corpo = NULL;
        copia_texto(vid_falha, url, sizeof vid_falha);
        rodape("No se pudo descargar el vídeo");
        return;
    }
    vid_dados = (uint8_t *)vb.corpo;
    vb.corpo = NULL;
    int erro = 0;
    vid = video_abrir_memoria(vid_dados, (uint32_t)vb.tam, &erro);
    if (!vid) {
        registrar("cipo: video: %s\n", video_erro_texto(erro));
        libera(vid_dados);
        vid_dados = NULL;
        copia_texto(vid_falha, url, sizeof vid_falha);
        rodape(video_erro_texto(erro));
        return;
    }
    const video_info_t *inf = video_info(vid);
    copia_texto(vid_url, url, sizeof vid_url);
    vid_fps = inf->fps > 0 ? inf->fps : 25;
    if (vid_pl > 0 && inf->larg >= 2 * vid_pl) video_reducao(vid, 2);   /* la caja es pequeña: mitad de tamaño, más rápido */
    vid_prox = 0;
    vid_fim = false;
    registrar("cipo: video %s %dx%d %d fps, %d cuadros, audio %s\n", inf->nome_formato, inf->larg, inf->alt, vid_fps, inf->quadros,
              inf->tem_audio ? "si" : "no");
    if (video_quadro(vid, &vid_img) == 0) { vid_prox = 1; vid_mostrados++; }
    vid_tocando = (vid_flags & VIDEO_AUTOPLAY) != 0;
    vid_t0 = tempo_ms();
    if (vid_tocando) video_abrir_som();
    rodape("");
    desenhar_pagina();   /* el motor pide el cuadro por motor_video() */
}

/* Un paso del bucle de eventos: arranca el vídeo que pidió la pintura y saca el cuadro que toca. */
static void video_tick(void)
{
    if (aud) { audio_tick(); return; }
    if (!vid) {
        if (vid_quer[0] && compara_texto(vid_quer, vid_falha) != 0 && !pintura_pendente) { if (med_audio) audio_iniciar(vid_quer); else video_iniciar(vid_quer); }
        return;
    }
    if (!vid_tocando || vid_fim) return;
    video_escrever_som();
    unsigned agora = video_relogio();
    int debe = (int)(agora * (unsigned)vid_fps / 1000u);
    if (vid_prox > debe) return;
    unsigned t0 = tempo_ms();
    int saltos = 0;
    while (debe - vid_prox >= 2 && saltos < 4) {   /* atrasado: se decodifican sin convertir color */
        if (video_quadro(vid, NULL) != 0) break;
        vid_prox++;
        saltos++;
        vid_saltados++;
        video_escrever_som();
    }
    int r = video_quadro(vid, &vid_img);
    vid_ms_decod += tempo_ms() - t0;
    if (r == 0) {
        vid_prox++;
        vid_mostrados++;
        video_escrever_som();
        video_blit();
    } else if (vid_flags & VIDEO_LOOP) {
        video_buscar(vid, 0);
        vid_prox = 0;
        vid_t0 = tempo_ms();
        if (vid_audio >= 0) { audio_fechar(vid_audio, 0); vid_audio = -1; }
        video_abrir_som();
    } else {
        vid_fim = true;
        vid_tocando = false;
        if (vid_audio >= 0) { audio_fechar(vid_audio, 1); vid_audio = -1; }
        registrar("cipo: video terminado\n");
    }
}

/* Clic sobre el vídeo: pausa o reanuda. true = el clic era del vídeo. */
static bool video_clique(int x, int y)
{
    if (media_clique(x, y)) return true;      /* la barra de controles (D489) */
    if (!vid || !vid_img.px) return false;
    int wy = ABA_Y + vid_py - rol_px;
    if (x < vid_px || x >= vid_px + vid_pl || y < wy || y >= wy + vid_pa || y < ABA_Y || y >= ABA_Y + AREA_A) return false;
    if (vid_fim) {   /* terminado: otra vez desde el principio */
        video_buscar(vid, 0);
        vid_prox = 0;
        vid_fim = false;
    }
    vid_tocando = !vid_tocando;
    if (vid_tocando) {
        vid_t0 = tempo_ms() - (unsigned)(vid_prox * 1000 / vid_fps);
        if (vid_audio < 0) video_abrir_som();
    }
    registrar("cipo: video %s\n", vid_tocando ? "reanudado" : "en pausa");
    video_blit();
    return true;
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

static unsigned rec_t0;   /* cuándo empezó la descarga de recursos de la página (para medirla) */

#define REPINTA_MS 80            /* durante la carga, como mucho ~12 repintados por segundo (D441) */

/* Rehace la maquetación con lo descargado hasta ahora y repinta. */
static void relayout_agora(void)
{
    relayout_pendente = false;
    ult_relayout = tempo_ms();
    est_layouts++;
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
}

/* Descarga un recurso del motor por vuelta del bucle de eventos. */
static void processar_recurso(void)
{
    if (painel) pintura_pendente = false;   /* el panel ocupa el sitio de la página */
    if (pintura_pendente && motor && pag_fb_garantir()) {
        /* franjas en el búfer: la ventana solo cambia cuando la página entera está lista (D441) */
        int fim = AREA_A;
        int f = pintura_y - rol_px;
        int faixa = fim - f < FAIXA_PINTURA ? fim - f : FAIXA_PINTURA;
        unsigned t0 = tempo_ms();
        if (faixa > 0) {
            pag_fb_pintar(f, faixa);
            pintura_y += faixa;
        }
        est_ms_pintura += tempo_ms() - t0;
        if (pintura_y - rol_px >= fim) {
            pintura_pendente = false;
            pag_fb_ok = true;
            pag_fb_apresentar();
            registrar("cipo: pagina pintada\n");
            if (sug_n) sugestoes_desenhar();
        }
        return;
    }
    if (!recursos_pendentes || !motor) return;
    if (!conf.imagens) {      /* ajuste: sin imágenes (ni fuentes web) */
        recursos_pendentes = false;
        registrar("cipo: recursos omitidos (imágenes desactivadas)\n");
        rodape("Listo");
        desenhar_barra(); desenhar_abas();
        return;
    }
    pf_lote();                /* varios hilos bajan por adelantado lo que el motor pedirá (D373) */
    rodape("Cargando recursos de la página...");
    if (!a_carregar_registrado) {
        a_carregar_registrado = true;
        rec_t0 = tempo_ms();
        registrar("cipo: a carregar recursos\n");
    }
    if (motor_imagens(motor, 1) > 0) {
        /* cada trozo que llega rehacía la maquetación y repintaba: ahora, como mucho cada REPINTA_MS (D441) */
        relayout_pendente = true;
        est_trozos++;
        if (tempo_ms() - ult_relayout >= REPINTA_MS) relayout_agora();
        return;
    }
    if (relayout_pendente) relayout_agora();
    recursos_pendentes = false;
    registrar("cipo: recursos listos en %u ms\n", tempo_ms() - rec_t0);
    registrar("cipo: pintura: %u repintados, %u apresentacoes, %u layouts, %u ms pintando, %u trozos\n", est_repintados, est_apresentacoes, est_layouts, est_ms_pintura, est_trozos);
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
    copia_texto(nav_ref, url_atual, sizeof nav_ref);   /* de dónde viene (Referer y SameSite) */
    copia_texto(url_atual, url, sizeof url_atual);
    caixa_definir(&endereco, url);
    endereco.foco = false;
    endereco_sel = false;
    sugestoes_fechar();
    if (painel) { painel = P_NADA; }
    foco_alvo = -1;
    codigo_pagina = 0;
    cert_texto[0] = 0;
    cert_ok = false;
    pf_limpar();
    liberar_imagens();
    if (qtd_abas) { abas[aba_atual].carregando = true; abas[aba_atual].tem_fav = false; abas[aba_atual].fav_tentado = comeca_com(url, "sobre:"); }
    desenhar_barra();
    desenhar_abas();
    registrar("cipo: abrindo %s\n", url);

    if (comeca_com(url, "sobre:")) {
        if (comeca_com(url, "sobre:red")) {
            /* Diagnóstico de la red (D487). "?recargar" descarta la memoria y vuelve a leer cookies y
             * almacenamiento del disco: solo lo acepta una dirección escrita, no una página. */
            if (compara_texto(url, "sobre:red?recargar") == 0 && !pedido_por_js) { rede_cookie_gravar(true); rede_cookie_recarregar(); rede_alm_recarregar(); }
            int ses = 0, ck = rede_cookie_contar(&ses), cb = 0, cn = rede_cache_contar(&cb);
            char h[900];
            formatar_texto(h, sizeof h, "<title>Red</title><h1>Red del Cipó</h1><p>Cookies: %d (%d de sesión), %d en disco.</p>"
                           "<p>Almacenamiento local: %d origen(es) en disco.</p><p>Caché HTTP: %d recursos, %d KB.</p>"
                           "<p>User-Agent: %s</p>", ck, ses, rede_cookie_em_disco(), rede_alm_em_disco(), cn, cb / 1024, REDE_UA);
            registrar("cipo: red: cookies=%d sesion=%d disco=%d storage_disco=%d cache=%d\n", ck, ses, rede_cookie_em_disco(), rede_alm_em_disco(), cn);
            definir_doc_texto(h);
        } else if (compara_texto(url, "sobre:blanco") == 0) definir_doc_texto("<title>En blanco</title>");
        else {
            char *h = inicio_gerar();
            if (h) { definir_doc(h, (int)tam_texto(h), true); }
            else definir_doc_texto(pagina_inicio);
        }
        diagramar();
    } else {
        static baixa_t b;
        baixar_pagina(url, &b);
        codigo_pagina = b.codigo;
        if (b.arquivo) {
            /* No es una página (un zip, un ejecutable...): lo baja el gestor de descargas (D373). */
            registrar("cipo: archivo %s (%s): descarga\n", url, b.detalhe);
            desc_iniciar(url);
            codigo_pagina = 0;
            definir_doc_texto("<title>Descarga</title><h1>Descarga iniciada</h1><p>Este enlace es un archivo, no una página. "
                              "Se está bajando en segundo plano: míralo en Descargas (Ctrl+J).</p>");
            diagramar();
        } else if (b.erro) {
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
    if (qtd_abas) abas[aba_atual].carregando = false;
    if (codigo_pagina > 0 && codigo_pagina < 400) hst_registar(url_atual, titulo);
    rede_cookie_gravar(false);
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
    if (qtd_desc && desc_sel >= 0 && desc[desc_sel].estado == D_BAIXANDO && compara_texto(titulo, "Descarga") == 0) mostrar_painel(P_DESC);
}

static void navegar(const char *url)
{
    video_parar();
    if (url_atual[0] && compara_texto(url, url_atual) != 0) {
        if (qtd_historico == HIST_MAX) {
            memmove(historico[0], historico[1], sizeof historico[0] * (HIST_MAX - 1));
            memmove(historico_pos, historico_pos + 1, sizeof historico_pos[0] * (HIST_MAX - 1));
            memmove(historico_doc, historico_doc + 1, sizeof historico_doc[0] * (HIST_MAX - 1));
            qtd_historico--;
        }
        copia_texto(historico[qtd_historico], url_atual, URL_MAX);
        historico_pos[qtd_historico] = rol_px;   /* onde se estava a ler */
        historico_doc[qtd_historico] = js_geracao;   /* D488: el documento de la entrada */
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
        memmove(futuro_doc, futuro_doc + 1, sizeof futuro_doc[0] * (HIST_MAX - 1));
        qtd_futuro--;
    }
    copia_texto(futuro[qtd_futuro], url_atual, URL_MAX);
    futuro_pos[qtd_futuro] = rol_px;
    futuro_doc[qtd_futuro] = js_geracao;
    qtd_futuro++;
    char u[URL_MAX];
    copia_texto(u, historico[--qtd_historico], sizeof u);
    int onde = historico_pos[qtd_historico];
    if (js && historico_doc[qtd_historico] == js_geracao) {     /* el mismo documento (pushState): popstate, sin descargar (D488) */
        char velha[URL_MAX];
        copia_texto(velha, url_atual, sizeof velha);
        copia_texto(url_atual, u, sizeof url_atual);
        js_popstate(-1, velha);
        return;
    }
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
        memmove(historico_doc, historico_doc + 1, sizeof historico_doc[0] * (HIST_MAX - 1));
        qtd_historico--;
    }
    copia_texto(historico[qtd_historico], url_atual, URL_MAX);
    historico_pos[qtd_historico] = rol_px;
    historico_doc[qtd_historico] = js_geracao;
    qtd_historico++;
    char u[URL_MAX];
    copia_texto(u, futuro[--qtd_futuro], sizeof u);
    int onde = futuro_pos[qtd_futuro];
    if (js && futuro_doc[qtd_futuro] == js_geracao) {     /* D488 */
        char velha[URL_MAX];
        copia_texto(velha, url_atual, sizeof velha);
        copia_texto(url_atual, u, sizeof url_atual);
        js_popstate(1, velha);
        return;
    }
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
    /* Mejora 103: se elige dónde guardar con el diálogo común (Intro acepta la propuesta). */
    if (!dialogo_guardar("Guardar como", NULL, caminho, caminho, sizeof caminho)) {
        libera(b.corpo);
        rodape("Guardado cancelado");
        return;
    }
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

/* Decodifica "a=b+c%41" en el sitio. */
static void form_decodificar(char *s)
{
    char *d = s;
    for (; *s; s++) {
        if (*s == '+') *d++ = ' ';
        else if (*s == '%' && s[1] && s[2]) {
            int h = s[1] <= '9' ? s[1] - '0' : (minuscula(s[1]) - 'a' + 10), l = s[2] <= '9' ? s[2] - '0' : (minuscula(s[2]) - 'a' + 10);
            *d++ = (char)(h * 16 + l);
            s += 2;
        } else *d++ = *s;
    }
    *d = 0;
}

/* Envía el formulario de quem (NULL = el campo con foco): GET como siempre; POST con el cuerpo en
 * urlencoded o multipart (D487). */
static void navegar_envio(const motor_alvo_t *quem)
{
    char url[URL_MAX];
    if (!motor_envio(motor, quem, url, sizeof url)) return;
    char metodo[12], enc[80], acao[URL_MAX];
    nav_clique = true;
    if (!motor_envio_modo(motor, quem, metodo, sizeof metodo, enc, sizeof enc, acao, sizeof acao) || compara_texto(metodo, "POST") != 0) {
        navegar(url);
        return;
    }
    char *q = acha_char(url, '?');
    const char *campos = q ? q + 1 : "";
    int nc = (int)tam_texto(campos);
    if (rede_comeca_ci(enc, "multipart/form-data")) {
        char borde[48];
        uint32_t r1 = 0, r2 = 0;
        bytes_aleatorios(&r1, 4); bytes_aleatorios(&r2, 4);
        formatar_texto(borde, sizeof borde, "----CipoFormBoundary%x%x", r1, r2);
        int cap = nc * 3 + 256 + (int)tam_texto(borde) * 2, k = 0;
        char *cuerpo = aloca((size_t)cap);
        char *cp = aloca((size_t)nc + 1);
        if (!cuerpo || !cp) { libera(cuerpo); libera(cp); return; }
        memcpy(cp, campos, (size_t)nc + 1);
        for (char *p = cp; *p;) {
            char *fin = acha_char(p, '&');
            if (fin) *fin = 0;
            char *eq = acha_char(p, '=');
            const char *val = "";
            if (eq) { *eq = 0; val = eq + 1; }
            form_decodificar(p);
            char *vv = aloca(tam_texto(val) + 1);
            if (vv) {
                copia_texto(vv, val, tam_texto(val) + 1);
                form_decodificar(vv);
                for (char *c = p; *c; c++) if (*c == '"') *c = '\'';
                int need = (int)tam_texto(borde) + (int)tam_texto(p) + (int)tam_texto(vv) + 96;
                if (k + need < cap) k += formatar_texto(cuerpo + k, (size_t)(cap - k), "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n", borde, p, vv);
                libera(vv);
            }
            if (!fin) break;
            p = fin + 1;
        }
        k += formatar_texto(cuerpo + k, (size_t)(cap - k), "--%s--\r\n", borde);
        libera(cp);
        post_corpo = cuerpo; post_n = k;
        formatar_texto(post_tipo, sizeof post_tipo, "multipart/form-data; boundary=%s", borde);
    } else if (rede_comeca_ci(enc, "text/plain")) {
        post_corpo = aloca((size_t)nc + 1);
        if (!post_corpo) return;
        memcpy(post_corpo, campos, (size_t)nc + 1);
        for (char *p = post_corpo; *p; p++) if (*p == '&') *p = '\n';
        post_n = nc;
        copia_texto(post_tipo, "text/plain", sizeof post_tipo);
    } else {
        post_corpo = aloca((size_t)nc + 1);
        if (!post_corpo) return;
        memcpy(post_corpo, campos, (size_t)nc + 1);
        post_n = nc;
        copia_texto(post_tipo, "application/x-www-form-urlencoded", sizeof post_tipo);
    }
    registrar("cipo: POST %s (%d bytes, %s)\n", acao, post_n, post_tipo);
    navegar(acao);
}

static void abrir_alvo_motor(const motor_alvo_t *a)
{
    if (!a) return;
    /* El click ya lo vio el JavaScript (js_clique_pagina, D488): si lo canceló, no se llega aquí. */
    if (a->tipo == MA_LINK && a->href && a->href[0]) {
        /* solo cambia el #: la misma página, con hashchange (D488) */
        if (js && js_so_ancora(url_atual, a->href)) {
            char json[URL_MAX + 32], *u = json;
            int k = formatar_texto(json, sizeof json, "{\"url\":\"");
            char t[URL_MAX * 2];
            int n = latin1_para_utf8(a->href, (int)tam_texto(a->href), t, (int)sizeof t);
            for (int i = 0; i < n && k < (int)sizeof json - 4; i++) { if (t[i] == '"' || t[i] == '\\') json[k++] = '\\'; json[k++] = t[i]; }
            json[k++] = '"'; json[k++] = '}'; json[k] = 0;
            js_historia(t, 0);
            qjs_despachar(js, -1, "hashchange", u);
            js_refrescar();
            return;
        }
        nav_clique = true;
        navegar(a->href);
    }
    else if ((a->tipo == MA_CAMPO || a->tipo == MA_BOTAO) && a->envia) {
        if (js && js_formulario(a->dom)) return;      /* submit cancelado por el script (D488) */
        navegar_envio(a);                             /* GET o POST (D487) */
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
        if (js && js_formulario(motor_dom_foco(motor))) return;    /* D488 */
        navegar_envio(NULL);
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
    js_campo_input();      /* el evento input (D488) */
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
    char linha[URL_MAX + 140];
    while (qtd_marcadores < MARC_MAX) {
        int n = ler_linha(fd, linha, sizeof linha);
        if (n < 0) break;
        if (!n || linha[0] == '#') continue;
        char *sep = NULL, *sep2 = NULL;   /* url<TAB>título[<TAB>carpeta] (la carpeta es de la D373) */
        for (char *p = linha; *p; p++) if (*p == '\t') { if (!sep) sep = p; else { sep2 = p; break; } }
        if (sep) *sep = 0;
        if (sep2) *sep2 = 0;
        copia_texto(marcador_url[qtd_marcadores], linha, URL_MAX);
        if (sep) copia_texto(marcador_titulo[qtd_marcadores], sep + 1, 96);
        else marcador_titulo[qtd_marcadores][0] = 0;
        if (sep2) copia_texto(marcador_pasta[qtd_marcadores], sep2 + 1, 32);
        else marcador_pasta[qtd_marcadores][0] = 0;
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
        char linha[URL_MAX + 140];
        int n = formatar_texto(linha, sizeof linha, "%s\t%s%s%s\n", marcador_url[i], marcador_titulo[i], marcador_pasta[i][0] ? "\t" : "", marcador_pasta[i]);
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
    marcador_pasta[qtd_marcadores][0] = 0;
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
    static char itens[32 * (URL_MAX + 96)];   /* el menú muestra los 32 primeros; el gestor (Ctrl+Mayús+B), todos */
    int k = 0;
    for (int i = 0; i < qtd_marcadores && i < 32; i++) {
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
/* ------------------------------------------------------------ ajustes, historial y marcadores (D373) */

static void caminho_cfg(const char *nome, char *saida, int max)
{
    char base[64], d[96];
    copia_texto(base, pasta_usuario(), sizeof base);
    formatar_texto(d, sizeof d, "%s/.config", base);
    criar_pasta(d);
    formatar_texto(d, sizeof d, "%s/.config/cipo", base);
    criar_pasta(d);
    formatar_texto(saida, max, "%s/.config/cipo/%s", base, nome);
}

static int num_de(const char *s, int min, int max, int pad)
{
    if (!s || !*s) return pad;
    int v = 0;
    bool neg = false;
    if (*s == '-') { neg = true; s++; }
    if (*s < '0' || *s > '9') return pad;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    if (neg) v = -v;
    return v < min ? min : v > max ? max : v;
}

/* cipo.conf: «clave=valor». Un valor que no vale se recorta (los hilos, a 1-12). */
static void ler_conf(void)
{
    char caminho[128];
    caminho_cfg("cipo.conf", caminho, sizeof caminho);
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return;
    char linha[URL_MAX + 40];
    while (ler_linha(fd, linha, sizeof linha) >= 0) {
        char *v = NULL;
        for (char *p = linha; *p; p++) if (*p == '=') { *p = 0; v = p + 1; break; }
        if (!v) continue;
        if (!compara_texto(linha, "inicio_modo")) conf.inicio_modo = num_de(v, 0, 2, 0);
        else if (!compara_texto(linha, "inicio_url")) copia_texto(conf.inicio_url, v, sizeof conf.inicio_url);
        else if (!compara_texto(linha, "buscador")) conf.buscador = num_de(v, 0, N_BUSC - 1, 0);
        else if (!compara_texto(linha, "hilos")) conf.hilos = num_de(v, 1, HILOS_MAX, 4);
        else if (!compara_texto(linha, "descargas")) copia_texto(conf.pasta_desc, v, sizeof conf.pasta_desc);
        else if (!compara_texto(linha, "js")) conf.js = num_de(v, 0, 1, 1) != 0;
        else if (!compara_texto(linha, "imagenes")) conf.imagens = num_de(v, 0, 1, 1) != 0;
        else if (!compara_texto(linha, "barra_marcadores")) conf.barra_marc = num_de(v, 0, 1, 0) != 0;
    }
    fechar(fd);
}

static void gravar_conf(void)
{
    char caminho[128], t[URL_MAX + 400];
    caminho_cfg("cipo.conf", caminho, sizeof caminho);
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    int n = formatar_texto(t, sizeof t,
        "# Ajustes del Cipó (D373)\ninicio_modo=%d\ninicio_url=%s\nbuscador=%d\nhilos=%d\ndescargas=%s\njs=%d\nimagenes=%d\nbarra_marcadores=%d\n",
        conf.inicio_modo, conf.inicio_url, conf.buscador, conf.hilos, conf.pasta_desc, conf.js, conf.imagens, conf.barra_marc);
    escrever_tudo(fd, t, (unsigned)n);
    fechar(fd);
}

/* Cuántas descargas a la vez caben de verdad: el valor pedido, recortado según la memoria libre
 * (cada hilo trae su pila y sus búferes; en un equipo de 128-256 MB, 12 serían demasiados). */
static int hilos_efectivos(void)
{
    int n = conf.hilos < 1 ? 1 : conf.hilos > HILOS_MAX ? HILOS_MAX : conf.hilos;
    info_memoria_t m;
    memset(&m, 0, sizeof m);
    if (info_sistema(INFO_MEMORIA, &m, sizeof m) >= 0 && m.livre_kb) {
        unsigned mb = m.livre_kb / 1024;
        int tope = mb < 24 ? 1 : mb < 48 ? 2 : mb < 80 ? 4 : mb < 120 ? 6 : mb < 160 ? 8 : HILOS_MAX;
        if (n > tope) n = tope;
    }
    return n;
}

static const char *pasta_descargas_conf(void)
{
    if (conf.pasta_desc[0]) {
        info_arquivo_t inf;
        if (info_arquivo(conf.pasta_desc, &inf) == 0 && inf.tipo == TIPO_PASTA) return conf.pasta_desc;
    }
    return pasta_descarga();
}

/* ---- historial ---- */

static void gravar_hst(void)
{
    char caminho[128];
    caminho_cfg("historial", caminho, sizeof caminho);
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    for (int i = 0; i < qtd_hst; i++) {
        char l[HST_URL + 96];
        int n = formatar_texto(l, sizeof l, "%s\t%s\t%d\n", hst[i].url, hst[i].tit, hst[i].visitas);
        escrever_tudo(fd, l, (unsigned)n);
    }
    fechar(fd);
}

static void ler_hst(void)
{
    qtd_hst = 0;
    char caminho[128];
    caminho_cfg("historial", caminho, sizeof caminho);
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return;
    char l[HST_URL + 96];
    while (qtd_hst < HST_MAX && ler_linha(fd, l, sizeof l) >= 0) {
        char *t1 = NULL, *t2 = NULL;
        for (char *p = l; *p; p++) if (*p == '\t') { if (!t1) { t1 = p; *p = 0; } else { t2 = p; *p = 0; break; } }
        if (!l[0] || !t1) continue;
        hst_t *h = &hst[qtd_hst++];
        copia_texto(h->url, l, sizeof h->url);
        copia_texto(h->tit, t1 + 1, sizeof h->tit);
        h->visitas = t2 ? num_de(t2 + 1, 1, 1000000, 1) : 1;
        h->ordem = ++hst_contador;
    }
    fechar(fd);
}

/* Anota una página vista (las de «sobre:» no entran). */
static void hst_registar(const char *url, const char *tit)
{
    if (!comeca_com(url, "http://") && !comeca_com(url, "https://")) return;
    if ((int)tam_texto(url) >= HST_URL) return;
    for (int i = 0; i < qtd_hst; i++)
        if (compara_texto(hst[i].url, url) == 0) {
            hst[i].visitas++;
            if (tit[0]) copia_texto(hst[i].tit, tit, sizeof hst[i].tit);
            hst_t t = hst[i];                 /* pasa al final: es lo más reciente */
            for (int k = i; k < qtd_hst - 1; k++) hst[k] = hst[k + 1];
            hst[qtd_hst - 1] = t;
            gravar_hst();
            return;
        }
    if (qtd_hst == HST_MAX) { memmove(&hst[0], &hst[1], sizeof hst[0] * (HST_MAX - 1)); qtd_hst--; }
    hst_t *h = &hst[qtd_hst++];
    copia_texto(h->url, url, sizeof h->url);
    copia_texto(h->tit, tit, sizeof h->tit);
    h->visitas = 1;
    gravar_hst();
}

/* ---- marcadores con carpetas, importar y exportar (formato HTML «Netscape bookmark file») ---- */

static void html_escapar(const char *s, char *o, int max)
{
    int k = 0;
    for (; *s && k < max - 7; s++) {
        const char *r = *s == '&' ? "&amp;" : *s == '<' ? "&lt;" : *s == '>' ? "&gt;" : *s == '"' ? "&quot;" : NULL;
        if (r) { while (*r && k < max - 2) o[k++] = *r++; }
        else o[k++] = *s;
    }
    o[k] = 0;
}

/* Exporta a un archivo HTML de marcadores (UTF-8, el que leen todos los navegadores). */
static bool marcadores_exportar(const char *caminho)
{
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return false;
    static const char cab[] = "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n<TITLE>Bookmarks</TITLE>\n<H1>Bookmarks</H1>\n<DL><p>\n";
    escrever_tudo(fd, cab, sizeof cab - 1);
    juntar_pastas();
    for (int pass = -1; pass < qtd_pastas; pass++) {
        if (pass >= 0) {
            char p8[160], pe[96];
            html_escapar(pastas[pass], pe, sizeof pe);
            latin1_para_utf8(pe, (int)tam_texto(pe), p8, sizeof p8);
            char l[300];
            int k = formatar_texto(l, sizeof l, "    <DT><H3>%s</H3>\n    <DL><p>\n", p8);
            escrever_tudo(fd, l, (unsigned)k);
        }
        for (int i = 0; i < qtd_marcadores; i++) {
            if (pass < 0 ? marcador_pasta[i][0] != 0 : compara_texto(marcador_pasta[i], pastas[pass]) != 0) continue;
            char ue[URL_MAX * 2], te[200], t8[400], l[URL_MAX * 2 + 520];
            html_escapar(marcador_url[i], ue, sizeof ue);
            html_escapar(marcador_titulo[i][0] ? marcador_titulo[i] : marcador_url[i], te, sizeof te);
            latin1_para_utf8(te, (int)tam_texto(te), t8, sizeof t8);
            int k = formatar_texto(l, sizeof l, "%s<DT><A HREF=\"%s\" ADD_DATE=\"0\">%s</A>\n", pass < 0 ? "    " : "        ", ue, t8);
            escrever_tudo(fd, l, (unsigned)k);
        }
        if (pass >= 0) escrever_tudo(fd, "    </DL><p>\n", 14);
    }
    escrever_tudo(fd, "</DL><p>\n", 9);
    fechar(fd);
    return true;
}

/* Importa un archivo HTML de marcadores; devuelve cuántos añadió (-1 si no se pudo leer). */
static int marcadores_importar(const char *caminho)
{
    info_arquivo_t inf;
    if (info_arquivo(caminho, &inf) != 0 || inf.tipo != TIPO_ARQUIVO || inf.tamanho > 2 * 1024 * 1024) return -1;
    char *h = aloca(inf.tamanho + 1);
    if (!h) return -1;
    int fd = abrir(caminho, ABRIR_LER), n = 0;
    if (fd >= 0) { int r; while (n < (int)inf.tamanho && (r = ler(fd, h + n, inf.tamanho - (unsigned)n)) > 0) n += r; fechar(fd); }
    h[n] = 0;
    n = converter(h, n);
    int novos = 0, dl = 0, dl_pasta = -1;
    bool pend = false;
    char pasta[32] = "";
    for (int i = 0; i < n; i++) {
        if (h[i] != '<') continue;
        const char *t = h + i + 1;
        int resto = n - i - 1;
        bool fecha = resto > 0 && t[0] == '/';
        const char *nm = fecha ? t + 1 : t;
        if (igual_sem_caixa_e_tilde(nm, "dl", 2) && (nm[2] == '>' || nm[2] == ' ' || nm[2] == 'p')) {
            if (fecha) { if (dl == dl_pasta) { pasta[0] = 0; dl_pasta = -1; } dl--; }
            else { dl++; if (pend) { dl_pasta = dl; pend = false; } }
        } else if (!fecha && igual_sem_caixa_e_tilde(nm, "h3", 2)) {
            int a = i; while (a < n && h[a] != '>') a++;
            int b = a + 1; while (b < n && !(h[b] == '<' && h[b + 1] == '/')) b++;
            char tmp[64];
            decodificar(h + a + 1, b - a - 1 > 60 ? 60 : b - a - 1, tmp, sizeof tmp);
            copia_texto(pasta, tmp, sizeof pasta);
            pend = true;
            i = b;
        } else if (!fecha && (nm[0] == 'a' || nm[0] == 'A') && (nm[1] == ' ' || nm[1] == '\t' || nm[1] == '\n')) {
            int a = i; while (a < n && h[a] != '>') a++;
            char href[URL_MAX];
            if (a < n && atributo_valor(h + i + 1, a - i - 1, "href", href, sizeof href)) {
                int b = a + 1; while (b < n && !(h[b] == '<' && h[b + 1] == '/')) b++;
                char tit[96];
                decodificar(h + a + 1, b - a - 1 > 90 ? 90 : b - a - 1, tit, sizeof tit);
                char u[URL_MAX];
                decodificar(href, (int)tam_texto(href), u, sizeof u);
                bool ja = false;
                for (int m = 0; m < qtd_marcadores; m++) if (!compara_texto(marcador_url[m], u)) ja = true;
                if ((comeca_com(u, "http://") || comeca_com(u, "https://")) && !ja && qtd_marcadores < MARC_MAX) {
                    copia_texto(marcador_url[qtd_marcadores], u, URL_MAX);
                    copia_texto(marcador_titulo[qtd_marcadores], tit, 96);
                    copia_texto(marcador_pasta[qtd_marcadores], pasta, 32);
                    registrar("cipo: importado marcador \"%s\" [%s]\n", tit, pasta);
                    qtd_marcadores++;
                    novos++;
                }
                i = b;
            }
        }
    }
    libera(h);
    return novos;
}

/* ---- la página de inicio propia, la entrada de la barra de dirección y los buscadores ---- */

static const char *url_inicio(void)
{
    if (conf.inicio_modo == 1) return "sobre:blanco";
    if (conf.inicio_modo == 2 && conf.inicio_url[0]) return conf.inicio_url;
    return "sobre:inicio";
}

static void url_codificar(const char *t, char *o, int max)
{
    static const char hexa[] = "0123456789ABCDEF";
    int k = 0;
    for (const unsigned char *p = (const unsigned char *)t; *p && k < max - 7; p++) {
        unsigned char c = *p;
        if (c == ' ') o[k++] = '+';
        else if (letra((char)c) || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') o[k++] = (char)c;
        else if (c >= 0x80) {   /* Latin-1 -> UTF-8, que es lo que esperan los sitios */
            unsigned char b[2] = { (unsigned char)(0xC0 | (c >> 6)), (unsigned char)(0x80 | (c & 0x3F)) };
            for (int i = 0; i < 2; i++) { o[k++] = '%'; o[k++] = hexa[b[i] >> 4]; o[k++] = hexa[b[i] & 15]; }
        } else { o[k++] = '%'; o[k++] = hexa[c >> 4]; o[k++] = hexa[c & 15]; }
    }
    o[k] = 0;
}

/* Lo escrito en la barra: una dirección, o una búsqueda con el motor elegido si parece texto suelto. */
static void interpretar_entrada(const char *t, char *url, int max)
{
    bool esq = comeca_com(t, "http://") || comeca_com(t, "https://") || comeca_com(t, "sobre:");
    bool espaco = false, ponto = false;
    for (const char *p = t; *p; p++) { if (*p == ' ') espaco = true; if (*p == '.' || *p == ':' || *p == '/') ponto = true; }
    if (!compara_texto(t, "localhost")) ponto = true;
    if (!esq && (espaco || !ponto)) {
        char q[URL_MAX];
        url_codificar(t, q, sizeof q);
        const buscador_t *b = &buscadores[conf.buscador];
        formatar_texto(url, max, "%s?%s=%s", b->acao, b->campo, q);
        registrar("cipo: búsqueda con %s\n", b->nome);
    } else copia_texto(url, t, (size_t)max);
}

/* La página de inicio: buscador con el motor elegido, accesos rápidos (marcadores y los más
 * visitados) y los consejos de siempre. Devuelve un bloque de aloca (Latin-1). */
static char *inicio_gerar(void)
{
    enum { CAP = 12288 };
    char *h = aloca(CAP);
    if (!h) return NULL;
    int k = 0;
    const buscador_t *b = &buscadores[conf.buscador];
    k += formatar_texto(h + k, CAP - k,
        "<title>Inicio</title><h1>Cipó, el navegador de SaguiOS</h1>"
        "<form action=\"%s\" method=\"get\"><p><input type=\"text\" name=\"%s\" size=\"40\"> "
        "<input type=\"submit\" value=\"Buscar con %s\"></p></form>", b->acao, b->campo, b->nome);
    k += formatar_texto(h + k, CAP - k, "<h3>Accesos rápidos</h3><ul>");
    int n = 0;
    for (int i = 0; i < qtd_marcadores && n < 8 && k < CAP - 1500; i++) {
        char u[URL_MAX * 2], t[200];
        html_escapar(marcador_url[i], u, sizeof u);
        html_escapar(marcador_titulo[i][0] ? marcador_titulo[i] : marcador_url[i], t, sizeof t);
        k += formatar_texto(h + k, CAP - k, "<li><a href=\"%s\">%s</a> (marcador)</li>", u, t);
        n++;
    }
    bool usado[HST_MAX];
    memset(usado, 0, sizeof usado);
    for (int r = 0; r < 8 && n < 14 && k < CAP - 1500; r++) {   /* los más visitados */
        int mejor = -1;
        for (int i = 0; i < qtd_hst; i++)
            if (!usado[i] && (mejor < 0 || hst[i].visitas > hst[mejor].visitas)) mejor = i;
        if (mejor < 0) break;
        usado[mejor] = true;
        char u[HST_URL * 2], t[160];
        html_escapar(hst[mejor].url, u, sizeof u);
        html_escapar(hst[mejor].tit[0] ? hst[mejor].tit : hst[mejor].url, t, sizeof t);
        k += formatar_texto(h + k, CAP - k, "<li><a href=\"%s\">%s</a> (%d visita%s)</li>", u, t, hst[mejor].visitas, hst[mejor].visitas == 1 ? "" : "s");
        n++;
    }
    if (!n) k += formatar_texto(h + k, CAP - k, "<li>Aquí aparecerán tus marcadores y los sitios que más visites.</li>");
    k += formatar_texto(h + k, CAP - k, "</ul>");
    k += formatar_texto(h + k, CAP - k, "%s", pagina_inicio + (sizeof("<title>Inicio</title><h1>Cipó, el navegador de SaguiOS</h1>") - 1));
    return h;
}

/* ---- favicon (D373): el <link rel="icon"> de la página, o /favicon.ico; si no, el icono genérico ---- */

static unsigned le32(const uint8_t *p) { return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24; }
static unsigned le16(const uint8_t *p) { return (unsigned)p[0] | (unsigned)p[1] << 8; }

static void favicon_escalar(aba_t *a, const uint32_t *px, int l, int h)
{
    if (l <= 0 || h <= 0) return;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) a->fav[y * 16 + x] = px[(y * h / 16) * l + (x * l / 16)] & 0xFFFFFF;
    a->tem_fav = true;
}

static void favicon_buscar(void)
{
    aba_t *a = &abas[aba_atual];
    a->fav_tentado = true;
    if (!doc || !doc_html || (!comeca_com(url_atual, "http://") && !comeca_com(url_atual, "https://"))) return;
    char href[URL_MAX] = "", abs[URL_MAX];
    for (int i = 0; i + 8 < doc_tam && i < 60000; i++) {
        if (doc[i] != '<') continue;
        if (igual_sem_caixa_e_tilde(doc + i + 1, "/head", 5) || igual_sem_caixa_e_tilde(doc + i + 1, "body", 4)) break;
        if (!igual_sem_caixa_e_tilde(doc + i + 1, "link", 4)) continue;
        int e = i;
        while (e < doc_tam && doc[e] != '>') e++;
        char rel[64];
        if (atributo_valor(doc + i + 5, e - i - 5, "rel", rel, sizeof rel) && contem_sem_caixa(rel, "icon")
            && atributo_valor(doc + i + 5, e - i - 5, "href", href, sizeof href)) break;
        href[0] = 0;
        i = e;
    }
    if (comeca_com(href, "data:")) return;
    if (!juntar_url(url_atual, href[0] ? href : "/favicon.ico", abs, sizeof abs)) return;
    static baixa_t fb;
    baixar(abs, false, 65536, &fb);
    if (fb.erro || !fb.corpo || fb.tam < 24) { libera(fb.corpo); registrar("cipo: favicon no disponible (%s)\n", abs); return; }
    const uint8_t *d = (const uint8_t *)fb.corpo;
    int n = fb.tam;
    if (d[0] == 0 && d[1] == 0 && d[2] == 1 && d[3] == 0) {         /* .ico: se busca la entrada más cercana a 16x16 */
        int cnt = (int)le16(d + 4), mejor = -1, ms = 1000;
        for (int k = 0; k < cnt && 6 + 16 * (k + 1) <= n; k++) {
            const uint8_t *e = d + 6 + 16 * k;
            int w = e[0] ? e[0] : 256, off = (int)le32(e + 12), sz = (int)le32(e + 8);
            int score = w > 16 ? w - 16 : 16 - w;
            if (off < 0 || sz <= 0 || off + sz > n) continue;
            if (score < ms) { ms = score; mejor = k; }
        }
        if (mejor >= 0) {
            const uint8_t *e = d + 6 + 16 * mejor;
            int off = (int)le32(e + 12), sz = (int)le32(e + 8);
            const uint8_t *dib = d + off;
            if (sz > 8 && dib[0] == 0x89 && dib[1] == 'P') { d = dib; n = sz; }
            else if (sz > 44 && le32(dib) == 40) {                 /* DIB de 24 o 32 bits */
                int w = (int)le32(dib + 4), h = (int)le32(dib + 8) / 2, bpp = (int)le16(dib + 14);
                if (w > 0 && w <= 256 && h > 0 && h <= 256 && (bpp == 32 || bpp == 24)) {
                    uint32_t *px = aloca((size_t)w * (size_t)h * 4);
                    if (px) {
                        const uint8_t *pd = dib + 40;
                        int stride = bpp == 32 ? w * 4 : (w * 3 + 3) & ~3;
                        for (int y = 0; y < h; y++)
                            for (int x = 0; x < w; x++) {
                                const uint8_t *q = pd + (h - 1 - y) * stride + x * (bpp / 8);
                                if (q + bpp / 8 > d + n) { px[y * w + x] = 0xE8ECF0; continue; }
                                unsigned al = bpp == 32 ? q[3] : 255;
                                unsigned r = (q[2] * al + 0xE8 * (255 - al)) / 255, g = (q[1] * al + 0xEC * (255 - al)) / 255, b = (q[0] * al + 0xF0 * (255 - al)) / 255;
                                px[y * w + x] = r << 16 | g << 8 | b;
                            }
                        favicon_escalar(a, px, w, h);
                        libera(px);
                    }
                }
                libera(fb.corpo);
                if (a->tem_fav) registrar("cipo: favicon %s\n", abs);
                return;
            } else { libera(fb.corpo); return; }
        } else { libera(fb.corpo); return; }
    }
    imagem_t im;
    memset(&im, 0, sizeof im);
    if (imagem_decodificar(d, n, &im) == 0) {
        favicon_escalar(a, im.px, im.larg, im.alt);
        imagem_liberar(&im);
    }
    libera(fb.corpo);
    if (a->tem_fav) registrar("cipo: favicon %s\n", abs);
    else registrar("cipo: favicon sin imagen (%s)\n", abs);
}

/* ---- descargas simultáneas (D373): hilos de trabajo que bajan los recursos por adelantado ----
 *
 * El motor de páginas no es reentrante y la ventana tampoco: los hilos SOLO bajan bytes
 * (baixar() no toca la ventana si no es el hilo principal). El análisis, la decodificación y el
 * pintado siguen en el hilo principal, que recoge cada recurso de la caché cuando el motor lo pide
 * (motor_baixar). Con 1 hilo no hay nada de esto y se baja en serie, como siempre. */
#define PF_MAX 24
#define PF_BYTES (1024 * 1024)       /* tope de cada recurso por adelantado (los mayores se bajan en serie) */
enum { PF_LIVRE = 0, PF_PEDIDO, PF_BAIXANDO, PF_LISTO, PF_ERRO, PF_FALHA };
typedef struct { char url[URL_MAX]; volatile int estado; uint8_t *dados; unsigned n; } pf_t;
static pf_t pf[PF_MAX];
static mutex_t pf_trava = MUTEX_INICIAL;
static volatile uint32_t pf_vivos;
static thread_t *pf_th[HILOS_MAX];
static int pf_nth;
static volatile int pf_cancela;
static int pf_hilos_usados;          /* hilos del último lote (para la ventana de ajustes) */

static void *pf_trabalhador(void *arg)
{
    (void)arg;
    for (;;) {
        int k = -1;
        mutex_tomar(&pf_trava);
        if (!pf_cancela)
            for (int i = 0; i < PF_MAX; i++) if (pf[i].estado == PF_PEDIDO) { pf[i].estado = PF_BAIXANDO; k = i; break; }
        mutex_largar(&pf_trava);
        if (k < 0) break;
        baixa_t *b = aloca_zerado(sizeof *b);
        char u[URL_MAX];
        copia_texto(u, pf[k].url, sizeof u);
        registrar("cipo: descarga hilo %d %s\n", thread_id(), u);
        if (b) baixar(u, false, PF_BYTES, b);
        mutex_tomar(&pf_trava);
        if (b && !b->erro && !b->truncado && b->corpo && b->tam > 0) {
            pf[k].dados = (uint8_t *)b->corpo; pf[k].n = (unsigned)b->tam; pf[k].estado = PF_LISTO;
        } else {
            if (b) libera(b->corpo);
            pf[k].estado = (b && b->erro) ? PF_ERRO : PF_FALHA;
        }
        mutex_largar(&pf_trava);
        libera(b);
    }
    atomico_dec32(&pf_vivos);
    return NULL;
}

static void pf_juntar(void)
{
    for (int i = 0; i < pf_nth; i++) if (pf_th[i]) thread_join(pf_th[i]);
    pf_nth = 0;
}

/* Deja la caché vacía (al cambiar de página o de pestaña): los hilos acaban lo que traen entre manos. */
static void pf_limpar(void)
{
    pf_cancela = 1;
    pf_juntar();
    pf_cancela = 0;
    mutex_tomar(&pf_trava);
    for (int i = 0; i < PF_MAX; i++) { if (pf[i].dados) libera(pf[i].dados); pf[i].dados = NULL; pf[i].estado = PF_LIVRE; }
    mutex_largar(&pf_trava);
}

/* Si la página tiene recursos por bajar y se piden varios hilos, lanza un lote. */
static void pf_lote(void)
{
    int h = hilos_efectivos();
    if (h < 2 || !motor || pf_vivos) return;
    pf_juntar();
    for (int i = 0; i < PF_MAX; i++) if (pf[i].estado == PF_PEDIDO || pf[i].estado == PF_BAIXANDO) return;
    const char *urls[PF_MAX];
    int n = motor_urls_pendentes(motor, urls, PF_MAX), pedidos = 0;
    for (int i = 0; i < n; i++) {
        bool ja = false;
        for (int k = 0; k < PF_MAX; k++) if (pf[k].estado != PF_LIVRE && !compara_texto(pf[k].url, urls[i])) ja = true;
        if (ja) continue;
        for (int k = 0; k < PF_MAX; k++)
            if (pf[k].estado == PF_LIVRE) { copia_texto(pf[k].url, urls[i], sizeof pf[k].url); pf[k].estado = PF_PEDIDO; pedidos++; break; }
    }
    if (pedidos < 2) {   /* uno solo no compensa: se baja en serie */
        for (int k = 0; k < PF_MAX; k++) if (pf[k].estado == PF_PEDIDO) pf[k].estado = PF_LIVRE;
        return;
    }
    int nth = h < pedidos ? h : pedidos;
    pf_vivos = (uint32_t)nth;
    pf_nth = 0;
    for (int i = 0; i < nth; i++) {
        thread_t *t = thread_create(pf_trabalhador, NULL, 64 * 1024);
        if (!t) { atomico_dec32(&pf_vivos); continue; }
        pf_th[pf_nth++] = t;
    }
    pf_hilos_usados = pf_nth;
    registrar("cipo: descargas en paralelo: %d recursos, %d hilos (pedidos %d)\n", pedidos, pf_nth, conf.hilos);
    if (!pf_nth) for (int k = 0; k < PF_MAX; k++) if (pf[k].estado == PF_PEDIDO) pf[k].estado = PF_LIVRE;
}

/* El recurso de la caché, esperando si lo está bajando un hilo. NULL + *achou = ya se intentó y falló. */
static uint8_t *pf_pegar(const char *url, unsigned *n, bool *achou)
{
    *achou = false;
    for (int i = 0; i < PF_MAX; i++) {
        if (pf[i].estado == PF_LIVRE || compara_texto(pf[i].url, url) != 0) continue;
        while (pf[i].estado == PF_BAIXANDO || (pf[i].estado == PF_PEDIDO && pf_vivos)) dormir(1);
        mutex_tomar(&pf_trava);
        int est = pf[i].estado;
        uint8_t *d = pf[i].dados;
        *n = pf[i].n;
        pf[i].dados = NULL;
        pf[i].estado = PF_LIVRE;
        mutex_largar(&pf_trava);
        if (est == PF_LISTO) { *achou = true; return d; }
        if (est == PF_ERRO) { *achou = true; return NULL; }
        return NULL;
    }
    return NULL;
}

/* ---- el gestor de descargas (D373) ---- */

static int desc_hilo_ler(void *c, void *buf, unsigned n)
{
    conexao_t *cn = c;
    if (!cn->tls) return conexao_ler(cn, buf, n);
    mutex_tomar(&tls_trava);
    int r = conexao_ler(cn, buf, n);
    mutex_largar(&tls_trava);
    return r;
}

static void desc_falha(desc_t *d, const char *t) { copia_texto(d->erro, t, sizeof d->erro); d->estado = D_ERRO; registrar("cipo: descarga error %s (%s)\n", d->nome, t); }

static void *desc_hilo(void *arg)
{
    desc_t *d = arg;
    http_resposta_t *rr = aloca_zerado(sizeof *rr);
    conexao_t *cn = aloca_zerado(sizeof *cn);
    char url[URL_MAX];
    copia_texto(url, d->url, sizeof url);
    int fd = -1;
    uint8_t *buf = aloca(16384);
    if (!rr || !cn || !buf) { desc_falha(d, "Sin memoria"); goto fim; }
    bool aberta = false;
    for (int red = 0; red < 5 && !aberta; red++) {
        url_t u;
        if (!url_separar(url, &u)) { desc_falha(d, "Dirección no válida"); goto fim; }
        char e2[160];
        if (u.https) {
            mutex_tomar(&tls_trava);
            if (!cfg.raizes && !tls_config_padrao(&cfg, NULL, e2, sizeof e2)) { mutex_largar(&tls_trava); desc_falha(d, "No se pudo preparar HTTPS"); goto fim; }
        }
        int rc = conexao_abrir(cn, &u, &cfg);
        if (u.https) mutex_largar(&tls_trava);
        if (rc < 0) { conexao_fechar(cn); desc_falha(d, "No se pudo conectar"); goto fim; }
        char ped[1024];
        int np = http_montar_pedido(&u, "GET", NULL, ped, sizeof ped);
        if (u.https) mutex_tomar(&tls_trava);
        int w = conexao_escrever(cn, ped, (unsigned)np);
        if (u.https) mutex_largar(&tls_trava);
        if (w < 0 || http_ler_cabecalho_de(rr, desc_hilo_ler, cn) < 0) { conexao_fechar(cn); desc_falha(d, "Respuesta no válida"); goto fim; }
        char dest[URL_MAX];
        if (rr->codigo >= 300 && rr->codigo < 400 && http_campo(rr->cabecalho, "Location", dest, sizeof dest)) {
            conexao_fechar(cn);
            char nova[URL_MAX];
            if (!juntar_url(url, dest, nova, sizeof nova)) { desc_falha(d, "Redirección no válida"); goto fim; }
            copia_texto(url, nova, sizeof url);
            continue;
        }
        aberta = true;
    }
    if (!aberta) { desc_falha(d, "Demasiadas redirecciones"); goto fim; }
    if (rr->codigo != 200) { conexao_fechar(cn); char t[32]; formatar_texto(t, sizeof t, "El servidor dice %d", rr->codigo); desc_falha(d, t); goto fim; }
    char cl[24];
    d->total = http_campo(rr->cabecalho, "Content-Length", cl, sizeof cl) ? num_de(cl, 0, 0x7FFFFFFF, 0) : 0;
    fd = abrir(d->caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { conexao_fechar(cn); desc_falha(d, "No se pudo crear el archivo"); goto fim; }
    int n = 0;
    for (;;) {
        while (d->pausa && !d->cancela) { d->estado = D_PAUSADA; dormir(100); }
        if (d->cancela) break;
        d->estado = D_BAIXANDO;
        n = http_ler_corpo(rr, buf, 16384);
        if (n <= 0) break;
        if (escrever_tudo(fd, buf, (unsigned)n) != n) { n = -1; break; }
        d->bytes += n;
    }
    conexao_fechar(cn);
    fechar(fd);
    fd = -1;
    if (d->cancela) { apagar(d->caminho); d->estado = D_CANCELADA; registrar("cipo: descarga cancelada %s\n", d->nome); }
    else if (n < 0) desc_falha(d, "Error al leer o escribir");
    else { d->estado = D_PRONTA; if (!d->total) d->total = d->bytes; registrar("cipo: descarga lista %s (%d bytes)\n", d->caminho, d->bytes); }
fim:
    if (fd >= 0) fechar(fd);
    libera(buf); libera(rr); libera(cn);
    return NULL;
}

static void desc_iniciar(const char *url)
{
    int k = -1;
    for (int i = 0; i < DESC_MAX; i++) if (desc[i].estado == D_LIVRE || desc[i].estado >= D_PRONTA) { k = i; break; }
    if (k < 0) { rodape("Hay demasiadas descargas; quita alguna de la lista"); return; }
    desc_t *d = &desc[k];
    memset(d, 0, sizeof *d);
    copia_texto(d->url, url, sizeof d->url);
    nome_arquivo_url(url, d->nome, sizeof d->nome);
    for (int v = 0; v < 100; v++) {
        if (v) { char b[64]; formatar_texto(b, sizeof b, "%d_%s", v, d->nome); formatar_texto(d->caminho, sizeof d->caminho, "%s/%s", pasta_descargas_conf(), b); }
        else formatar_texto(d->caminho, sizeof d->caminho, "%s/%s", pasta_descargas_conf(), d->nome);
        info_arquivo_t inf;
        bool ocupado = info_arquivo(d->caminho, &inf) == 0;
        for (int o = 0; o < DESC_MAX; o++) if (o != k && desc[o].estado && desc[o].estado < D_PRONTA && !compara_texto(desc[o].caminho, d->caminho)) ocupado = true;
        if (!ocupado) break;
    }
    d->estado = D_BAIXANDO;
    if (k >= qtd_desc) qtd_desc = k + 1;
    desc_sel = k;
    if (!thread_create(desc_hilo, d, 64 * 1024)) { desc_falha(d, "No se pudo crear el hilo"); return; }
    registrar("cipo: descarga iniciada %s -> %s\n", url, d->caminho);
}

static int desc_activas(void)
{
    int n = 0;
    for (int i = 0; i < qtd_desc; i++) if (desc[i].estado == D_BAIXANDO || desc[i].estado == D_PAUSADA) n++;
    return n;
}

/* ------------------------------------------------------------ paneles de gestión (D373)
 *
 * Marcadores, Historial, Descargas y los Ajustes ocupan el sitio de la página, con una lista de
 * secciones a la izquierda (como la Configuración del sistema). Se abren desde el menú «≡» o con
 * Ctrl+H (historial), Ctrl+J (descargas) y Ctrl+Mayús+B (marcadores). Esc los cierra. */

#define PAN_LISTA 150
static lista_t l_marc, l_hist;
static char it_marc[MARC_MAX][64];
static char it_hist[HST_MAX][64];
static int  hist_idx[HST_MAX], qtd_hist_f;
static caixa_t cx_busca, cx_inicio, cx_desc;
static casilla_t cs_js, cs_img, cs_barra;
static combo_t cb_inicio, cb_busc, cb_hilos;
static botao_t pb[8];
static unsigned ultimo_desc_ms;
static const char *const nomes_painel[P_N] = { "", N_("Marcadores"), N_("Historial"), N_("Descargas"), N_("General"), N_("Privacidad"), N_("Rendimiento") };
static const char *const inicio_txt[3] = { N_("Página propia de Cipó"), N_("Página en blanco"), N_("Esta dirección:") };
static const char *const hilos_txt[HILOS_MAX] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12" };

static int pan_y(void) { return ABA_Y; }
static int pan_x0(void) { return PAN_LISTA + 14; }
static int pan_l(void) { return LARG - PAN_LISTA - 28; }

static void marc_lista_reconstruir(void)
{
    for (int i = 0; i < qtd_marcadores; i++) {
        char t[96];
        const char *n = marcador_titulo[i][0] ? marcador_titulo[i] : marcador_url[i];
        if (marcador_pasta[i][0]) formatar_texto(t, sizeof t, "[%s] %s", marcador_pasta[i], n);
        else copia_texto(t, n, sizeof t);
        copia_texto(it_marc[i], t, 64);
    }
    l_marc.itens = it_marc;
    l_marc.qtd = qtd_marcadores;
    if (l_marc.sel >= qtd_marcadores) l_marc.sel = qtd_marcadores - 1;
}

static void hist_filtrar(void)
{
    qtd_hist_f = 0;
    for (int i = qtd_hst - 1; i >= 0; i--) {
        if (cx_busca.texto[0] && !contem_sem_caixa(hst[i].url, cx_busca.texto) && !contem_sem_caixa(hst[i].tit, cx_busca.texto)) continue;
        char t[96];
        formatar_texto(t, sizeof t, "%s%s%s", hst[i].tit, hst[i].tit[0] ? " - " : "", hst[i].url);
        copia_texto(it_hist[qtd_hist_f], t, 64);
        hist_idx[qtd_hist_f++] = i;
    }
    l_hist.itens = it_hist;
    l_hist.qtd = qtd_hist_f;
    if (l_hist.sel >= qtd_hist_f) l_hist.sel = qtd_hist_f - 1;
    if (l_hist.topo > l_hist.sel) l_hist.topo = l_hist.sel > 0 ? l_hist.sel : 0;
}

static void botoes_fila(int n, const char *const *txt, int y)
{
    int x = pan_x0();
    for (int i = 0; i < n; i++) {
        int l = (int)tam_texto(txt[i]) * LETRA_L + 20;
        pb[i] = (botao_t){ x, y, l, 26, txt[i], false };
        x += l + 8;
    }
}

static int pan_nbot;
static char pan_txt[8][40];

static void pan_botoes(int n, const char *a, const char *b, const char *c, const char *d, int y)
{
    const char *t[4] = { a, b, c, d };
    const char *tt[4];
    for (int i = 0; i < n; i++) { copia_texto(pan_txt[i], _(t[i]), sizeof pan_txt[i]); tt[i] = pan_txt[i]; }
    botoes_fila(n, tt, y);
    pan_nbot = n;
}

/* Coloca los controles de la sección actual (al abrirla y al redimensionar). */
static void painel_layout(void)
{
    int y0 = pan_y(), x0 = pan_x0(), cl = pan_l(), yb = y0 + AREA_A;
    switch (painel) {
    case P_MARC:
        l_marc = (lista_t){ .x = x0, .y = y0 + 34, .l = cl, .a = AREA_A - 34 - 76, .itens = it_marc, .qtd = 0, .sel = l_marc.sel, .topo = 0 };
        marc_lista_reconstruir();
        pan_botoes(4, N_("Abrir"), N_("Editar"), N_("Borrar"), N_("Carpeta..."), yb - 68);
        {
            const char *t[2] = { _("Importar HTML..."), _("Exportar HTML...") };
            int x = x0;
            for (int i = 0; i < 2; i++) { int l = (int)tam_texto(t[i]) * LETRA_L + 20; pb[4 + i] = (botao_t){ x, yb - 36, l, 26, t[i], false }; x += l + 8; }
            pan_nbot = 6;
            cs_barra = (casilla_t){ x + 10, yb - 32, conf.barra_marc };
        }
        break;
    case P_HIST:
        cx_busca.x = x0 + 80; cx_busca.y = y0 + 6; cx_busca.l = cl > 360 ? 300 : cl - 80;
        l_hist = (lista_t){ .x = x0, .y = y0 + 42, .l = cl, .a = AREA_A - 42 - 40, .itens = it_hist, .sel = l_hist.sel, .topo = 0 };
        hist_filtrar();
        pan_botoes(3, N_("Abrir"), N_("Quitar"), N_("Borrar historial"), 0, yb - 34);
        break;
    case P_DESC:
        pan_botoes(4, N_("Pausar/Reanudar"), N_("Cancelar"), N_("Abrir carpeta"), N_("Quitar de la lista"), yb - 34);
        break;
    case P_GERAL:
        combo_iniciar(&cb_inicio, x0, y0 + 40, 240, inicio_txt, 3, false, COD_COMBO);
        combo_definir(&cb_inicio, _(inicio_txt[conf.inicio_modo]));
        cb_inicio.sel = conf.inicio_modo;
        cx_inicio.x = x0 + 250; cx_inicio.y = y0 + 38; cx_inicio.l = cl - 250 > 300 ? 300 : cl - 250;
        caixa_definir(&cx_inicio, conf.inicio_url);
        combo_iniciar(&cb_busc, x0, y0 + 100, 240, buscadores_nomes, N_BUSC, false, COD_COMBO + 1);
        combo_definir(&cb_busc, buscadores_nomes[conf.buscador]);
        cb_busc.sel = conf.buscador;
        cx_desc.x = x0; cx_desc.y = y0 + 160; cx_desc.l = cl - 130 > 400 ? 400 : cl - 130;
        caixa_definir(&cx_desc, conf.pasta_desc[0] ? conf.pasta_desc : pasta_descarga());
        { const char *t = _("Elegir..."); pb[0] = (botao_t){ cx_desc.x + cx_desc.l + 8, y0 + 158, (int)tam_texto(t) * LETRA_L + 20, 26, t, false }; pan_nbot = 1; }
        cs_barra = (casilla_t){ x0, y0 + 204, conf.barra_marc };
        break;
    case P_PRIV:
        pan_botoes(2, N_("Activar o desactivar"), N_("Borrar historial"), 0, 0, y0 + 74);
        {
            const char *t = _("Olvidar posiciones de lectura");
            pb[2] = (botao_t){ x0, y0 + 112, (int)tam_texto(t) * LETRA_L + 20, 26, t, false };
            const char *t2 = _("Borrar cookies y caché");
            pb[3] = (botao_t){ x0, y0 + 144, (int)tam_texto(t2) * LETRA_L + 20, 26, t2, false };
            pan_nbot = 4;
        }
        break;
    case P_REND:
        combo_iniciar(&cb_hilos, x0, y0 + 56, 80, hilos_txt, HILOS_MAX, false, COD_COMBO + 2);
        combo_definir(&cb_hilos, hilos_txt[conf.hilos - 1]);
        cb_hilos.sel = conf.hilos - 1;
        cs_js = (casilla_t){ x0, y0 + 170, conf.js };
        cs_img = (casilla_t){ x0, y0 + 200, conf.imagens };
        pan_nbot = 0;
        break;
    }
}

static void rot_p(int x, int y, const char *t, uint32_t cor) { g_texto_n(j, x, y, t, (LARG - x - 6) / LETRA_L, cor); }

static void painel_desc_desenhar(void)
{
    int x0 = pan_x0(), y0 = pan_y() + 34, cl = pan_l();
    g_retangulo(j, x0, y0, cl, AREA_A - 34 - 40, COR_BRANCO);
    g_moldura(j, x0, y0, cl, AREA_A - 34 - 40, COR_BORDA);
    int fila = 0;
    for (int i = 0; i < qtd_desc; i++) {
        desc_t *d = &desc[i];
        if (d->estado == D_LIVRE) continue;
        int y = y0 + 4 + fila * 46;
        if (y + 44 > y0 + AREA_A - 34 - 40) break;
        if (i == desc_sel) g_retangulo(j, x0 + 1, y - 2, cl - 2, 44, 0xDCE8F8);
        char t[160];
        rot_p(x0 + 8, y, d->nome, COR_TEXTO);
        int pct = d->total > 0 ? (int)((long long)d->bytes * 100 / d->total) : 0;
        const char *est = d->estado == D_BAIXANDO ? _("Descargando") : d->estado == D_PAUSADA ? _("En pausa") : d->estado == D_PRONTA ? _("Completa")
                        : d->estado == D_CANCELADA ? _("Cancelada") : d->erro;
        formatar_texto(t, sizeof t, "%s  %d KB%s", est, d->bytes / 1024, d->total ? "" : "");
        if (d->total) { char u[48]; formatar_texto(u, sizeof u, " de %d KB (%d%%)", d->total / 1024, pct); junta_texto(t, u, sizeof t); }
        rot_p(x0 + 8, y + 18, t, COR_TEXTO_FRACO);
        int bl = cl - 24;
        g_retangulo(j, x0 + 8, y + 36, bl, 4, 0xD0D6DC);
        int ll = d->total > 0 ? (int)((long long)bl * d->bytes / d->total) : (d->estado == D_PRONTA ? bl : 0);
        if (ll > bl) ll = bl;
        g_retangulo(j, x0 + 8, y + 36, ll, 4, d->estado == D_ERRO ? 0xB03A2E : COR_DESTAQUE);
        fila++;
    }
    if (!fila) rot_p(x0 + 10, y0 + 10, _("No hay descargas. Los archivos que no son páginas se bajan aquí."), COR_TEXTO_FRACO);
    jan_mostrar_area(j, x0, y0, cl, AREA_A - 34 - 40);
}

static void painel_desenhar(void)
{
    int y0 = pan_y();
    pintura_pendente = false;
    g_retangulo(j, 0, y0, LARG, AREA_A, COR_FUNDO);
    g_retangulo(j, 0, y0, PAN_LISTA, AREA_A, 0xE6E9EC);
    g_retangulo(j, PAN_LISTA, y0, 1, AREA_A, COR_BORDA);
    for (int i = 1; i < P_N; i++) {
        int y = y0 + 8 + (i - 1) * 28;
        if (i == painel) g_retangulo(j, 4, y - 2, PAN_LISTA - 8, 24, COR_SELECAO);
        rot_p(14, y + 2, _(nomes_painel[i]), i == painel ? COR_BRANCO : COR_TEXTO);
        if (i == P_DESC || i == P_GERAL) g_retangulo(j, 8, y + 25, PAN_LISTA - 16, 1, 0xC4CBD2);
    }
    int x0 = pan_x0();
    rot_p(x0, y0 + 10, _(nomes_painel[painel]), COR_DESTAQUE);
    switch (painel) {
    case P_MARC:
        lista_desenhar(j, &l_marc);
        casilla_desenhar(j, &cs_barra);
        rot_p(cs_barra.x + 22, cs_barra.y + 2, _("Mostrar la barra de marcadores"), COR_TEXTO);
        break;
    case P_HIST:
        rot_p(x0, y0 + 12, _("Buscar:"), COR_TEXTO);
        caixa_desenhar(j, &cx_busca);
        lista_desenhar(j, &l_hist);
        break;
    case P_DESC:
        painel_desc_desenhar();
        break;
    case P_GERAL:
        rot_p(x0, y0 + 28 - 12, _("Página de inicio y pestaña nueva"), COR_TEXTO_FRACO);
        rot_p(x0, y0 + 88 - 12, _("Motor de búsqueda (barra de dirección y página de inicio)"), COR_TEXTO_FRACO);
        rot_p(x0, y0 + 148 - 12, _("Carpeta de descargas"), COR_TEXTO_FRACO);
        caixa_desenhar(j, &cx_inicio);
        combo_desenhar(j, &cb_busc);
        caixa_desenhar(j, &cx_desc);
        casilla_desenhar(j, &cs_barra);
        rot_p(x0 + 22, cs_barra.y + 2, _("Mostrar la barra de marcadores"), COR_TEXTO);
        combo_desenhar(j, &cb_inicio);
        break;
    case P_PRIV: {
        info_bloqueo_t bi;
        memset(&bi, 0, sizeof bi);
        char t[160];
        if (bloqueo_dns(BLOQ_ESTADO, &bi, sizeof bi) < 0) copia_texto(t, _("Bloqueo de publicidad y rastreo (DNS): no disponible"), sizeof t);
        else formatar_texto(t, sizeof t, _("Bloqueo de publicidad y rastreo (DNS): %s, %u dominios"), bi.activo ? _("activo") : _("desactivado"), bi.bloqueados);
        rot_p(x0, y0 + 44, t, COR_TEXTO);
        {
            int ses = 0, ck = rede_cookie_contar(&ses), cb = 0, cn = rede_cache_contar(&cb);
            formatar_texto(t, sizeof t, _("Cookies: %d (%d de sesión). Caché: %d recursos, %d KB."), ck, ses, cn, cb / 1024);
            rot_p(x0, y0 + 184, t, COR_TEXTO_FRACO);
        }
        break;
    }
    case P_REND: {
        rot_p(x0, y0 + 32, _("Descargas simultáneas (hilos que bajan imágenes y recursos a la vez):"), COR_TEXTO);
        combo_desenhar(j, &cb_hilos);
        char t[160];
        int ef = hilos_efectivos();
        if (ef < conf.hilos) formatar_texto(t, sizeof t, _("Ahora se usan %d: la memoria libre no alcanza para %d."), ef, conf.hilos);
        else formatar_texto(t, sizeof t, _("Ahora se usan %d (de 1 a 12; 1 = en serie, como siempre)."), ef);
        rot_p(x0, y0 + 90, t, ef < conf.hilos ? 0xB03A2E : COR_TEXTO_FRACO);
        rot_p(x0, y0 + 110, _("En equipos con poca memoria conviene un valor bajo."), COR_TEXTO_FRACO);
        rot_p(x0, y0 + 146, _("Contenido de las páginas"), COR_TEXTO_FRACO);
        casilla_desenhar(j, &cs_js); rot_p(x0 + 22, cs_js.y + 2, _("Ejecutar JavaScript"), COR_TEXTO);
        casilla_desenhar(j, &cs_img); rot_p(x0 + 22, cs_img.y + 2, _("Mostrar imágenes"), COR_TEXTO);
        break;
    }
    }
    for (int i = 0; i < pan_nbot; i++) botao_desenhar(j, &pb[i]);
    jan_mostrar_area(j, 0, y0, LARG, AREA_A);
}

static void mostrar_painel(int p)
{
    if (p <= P_NADA || p >= P_N) return;
    sugestoes_fechar();
    if (painel == P_NADA) ler_marcadores();
    painel = p;
    if (p == P_HIST) { cx_busca.texto[0] = 0; cx_busca.cursor = 0; cx_busca.foco = true; }
    painel_layout();
    painel_desenhar();
    registrar("cipo: panel %s\n", nomes_painel[p]);
}

static void fechar_painel(void)
{
    if (!painel) return;
    painel = P_NADA;
    registrar("cipo: panel cerrado\n");
    desenhar_barra();
    desenhar_pagina();
}

static void barra_marc_mudou(void)
{
    gravar_conf();
    novo_tamanho();
    ajustar_barra();
    if (painel) { painel_layout(); painel_desenhar(); }
}

static void editar_marcador(int i)
{
    char t[96], u[URL_MAX];
    copia_texto(t, marcador_titulo[i], sizeof t);
    if (!pedir_texto(_("Marcador"), _("Nombre:"), t, sizeof t)) return;
    copia_texto(u, marcador_url[i], sizeof u);
    if (!pedir_texto(_("Marcador"), _("Dirección:"), u, 120) || !u[0]) return;
    copia_texto(marcador_titulo[i], t, 96);
    copia_texto(marcador_url[i], u, URL_MAX);
    gravar_marcadores();
    registrar("cipo: marcador editado %s\n", u);
}

static void painel_abrir_url(const char *u)
{
    char c[URL_MAX];
    copia_texto(c, u, sizeof c);
    fechar_painel();
    navegar(c);
}

static void painel_pergunta_hilos(void)
{
    gravar_conf();
    registrar("cipo: descargas simultáneas %d (efectivas %d)\n", conf.hilos, hilos_efectivos());
}

/* Trata un evento mientras hay un panel abierto. true = consumido. */
static bool painel_evento(const evento_t *e)
{
    if (e->tipo == EV_MENU && e->tecla >= COD_COMBO && e->tecla < COD_COMBO + 3) {
        if (painel == P_GERAL && e->tecla == COD_COMBO && combo_tratar(j, &cb_inicio, e) == COMBO_SELECIONOU) {
            conf.inicio_modo = cb_inicio.sel; gravar_conf(); painel_desenhar();
        } else if (painel == P_GERAL && e->tecla == COD_COMBO + 1 && combo_tratar(j, &cb_busc, e) == COMBO_SELECIONOU) {
            conf.buscador = cb_busc.sel; gravar_conf(); registrar("cipo: buscador %s\n", buscadores[conf.buscador].nome);
        } else if (painel == P_REND && e->tecla == COD_COMBO + 2 && combo_tratar(j, &cb_hilos, e) == COMBO_SELECIONOU) {
            conf.hilos = cb_hilos.sel + 1; painel_pergunta_hilos(); painel_desenhar();
        }
        return true;
    }
    bool mouse = e->tipo == EV_CLIQUE || e->tipo == EV_SOLTOU || e->tipo == EV_MOVER || e->tipo == EV_RODA;
    if (mouse && e->y < pan_y()) return false;       /* pestañas y barra: siguen funcionando */
    if (e->tipo == EV_TECLA && endereco.foco) return false;
    if (e->tipo == EV_TECLA) {
        unsigned t = e->tecla;
        if ((e->modificadores & 6) || t == TECLA_F5_CIPO || t == TECLA_F3_CIPO) return false;   /* atajos globales */
        if (t == TECLA_ESC || t == 27) {
            if (painel == P_HIST && cx_busca.foco && cx_busca.texto[0]) { caixa_definir(&cx_busca, ""); hist_filtrar(); painel_desenhar(); return true; }
            fechar_painel(); return true;
        }
    }
    if (!mouse && e->tipo != EV_TECLA) return false;
    /* la lista de secciones */
    if (e->tipo == EV_CLIQUE && (e->botoes & 1) && e->x < PAN_LISTA) {
        int i = (e->y - pan_y() - 6) / 28 + 1;
        if (i >= 1 && i < P_N && i != painel) { painel = i; if (i == P_HIST) { cx_busca.texto[0] = 0; cx_busca.cursor = 0; cx_busca.foco = true; } painel_layout(); painel_desenhar(); registrar("cipo: panel %s\n", nomes_painel[i]); }
        return true;
    }
    bool mudou = false;
    switch (painel) {
    case P_MARC: {
        int sel0 = l_marc.sel;
        int r = lista_tratar(j, &l_marc, e);
        if (r == LISTA_ATIVOU && e->tipo == EV_CLIQUE && sel0 != l_marc.sel) r = LISTA_SELECIONOU;   /* un doble clic solo abre lo ya elegido */
        if (r != LISTA_NADA) registrar("cipo: panel marcadores, lista %d, elegido %d, clic (%d,%d) x%u, lista y %d a %d\n", r, l_marc.sel, e->x, e->y, e->cliques, l_marc.y, l_marc.a);
        if (r == LISTA_ATIVOU && l_marc.sel >= 0) { painel_abrir_url(marcador_url[l_marc.sel]); return true; }
        if (casilla_tratar(&cs_barra, e)) { conf.barra_marc = cs_barra.marcada; barra_marc_mudou(); return true; }
        for (int b = 0; b < pan_nbot; b++) if (botao_tratar(j, &pb[b], e)) {
            int s = l_marc.sel;
            registrar("cipo: panel marcadores, botón %d, elegido %d\n", b, s);
            if (b == 0 && s >= 0) { painel_abrir_url(marcador_url[s]); return true; }
            if (b == 1 && s >= 0) { editar_marcador(s); }
            if (b == 2 && s >= 0) {
                for (int k = s; k < qtd_marcadores - 1; k++) {
                    copia_texto(marcador_url[k], marcador_url[k + 1], URL_MAX); copia_texto(marcador_titulo[k], marcador_titulo[k + 1], 96); copia_texto(marcador_pasta[k], marcador_pasta[k + 1], 32);
                }
                qtd_marcadores--;
                gravar_marcadores();
                registrar("cipo: marcador borrado, quedan %d\n", qtd_marcadores);
            }
            if (b == 3 && s >= 0) {
                char p[32];
                copia_texto(p, marcador_pasta[s], sizeof p);
                if (pedir_texto(_("Carpeta"), _("Nombre de la carpeta (vacío = ninguna):"), p, sizeof p)) {
                    copia_texto(marcador_pasta[s], p, 32); gravar_marcadores(); registrar("cipo: marcador %d en la carpeta \"%s\"\n", s, p);
                }
            }
            if (b == 4) {
                char c[160];
                formatar_texto(c, sizeof c, "%s/marcadores.html", pasta_descargas_conf());
                if (dialogo_abrir(_("Importar marcadores"), "*.html", c, c, sizeof c)) {
                    int n = marcadores_importar(c);
                    if (n >= 0) { gravar_marcadores(); registrar("cipo: importados %d marcadores de %s\n", n, c); }
                    else mensagem("Cipó", _("No se pudo leer el archivo de marcadores."));
                }
            }
            if (b == 5) {
                char c[160];
                formatar_texto(c, sizeof c, "%s/marcadores.html", pasta_descargas_conf());
                if (dialogo_guardar(_("Exportar marcadores"), "*.html", c, c, sizeof c)) {
                    if (marcadores_exportar(c)) registrar("cipo: marcadores exportados a %s (%d)\n", c, qtd_marcadores);
                    else mensagem("Cipó", _("No se pudo escribir el archivo."));
                }
            }
            mudou = true;
        }
        if (mudou) { painel_layout(); painel_desenhar(); desenhar_barra(); }
        return true;
    }
    case P_HIST: {
        char antes[sizeof cx_busca.texto];
        copia_texto(antes, cx_busca.texto, sizeof antes);
        int r = LISTA_NADA;
        if (e->tipo == EV_TECLA && cx_busca.foco && e->tecla != TECLA_CIMA && e->tecla != TECLA_BAIXO && e->tecla != '\n') {
            caixa_tratar(j, &cx_busca, e);
        } else {
            if (e->tipo == EV_CLIQUE) { bool f = cx_busca.foco; caixa_tratar(j, &cx_busca, e); if (cx_busca.foco && !f) { painel_desenhar(); return true; } }
            int sel0 = l_hist.sel;
            r = lista_tratar(j, &l_hist, e);
            if (r == LISTA_ATIVOU && e->tipo == EV_CLIQUE && sel0 != l_hist.sel) r = LISTA_SELECIONOU;
        }
        if (compara_texto(antes, cx_busca.texto) != 0) { hist_filtrar(); painel_desenhar(); registrar("cipo: historial filtrado (%d)\n", qtd_hist_f); return true; }
        if ((r == LISTA_ATIVOU || (e->tipo == EV_TECLA && e->tecla == '\n' && cx_busca.foco)) && l_hist.sel < 0 && qtd_hist_f > 0) l_hist.sel = 0;
        if ((r == LISTA_ATIVOU || (e->tipo == EV_TECLA && e->tecla == '\n')) && l_hist.sel >= 0 && l_hist.sel < qtd_hist_f) { painel_abrir_url(hst[hist_idx[l_hist.sel]].url); return true; }
        for (int b = 0; b < pan_nbot; b++) if (botao_tratar(j, &pb[b], e)) {
            int s = l_hist.sel >= 0 && l_hist.sel < qtd_hist_f ? hist_idx[l_hist.sel] : -1;
            if (b == 0 && s >= 0) { painel_abrir_url(hst[s].url); return true; }
            if (b == 1 && s >= 0) { for (int k = s; k < qtd_hst - 1; k++) hst[k] = hst[k + 1]; qtd_hst--; gravar_hst(); }
            if (b == 2 && confirmar("Cipó", _("¿Borrar todo el historial?"))) { qtd_hst = 0; gravar_hst(); registrar("cipo: historial borrado\n"); }
            hist_filtrar(); painel_desenhar();
        }
        return true;
    }
    case P_DESC: {
        if (e->tipo == EV_CLIQUE && (e->botoes & 1)) {
            int y0 = pan_y() + 34 + 4, fila = 0, elegido = -1;
            for (int i = 0; i < qtd_desc; i++) {
                if (desc[i].estado == D_LIVRE) continue;
                int y = y0 + fila * 46;
                if (e->x >= pan_x0() && e->y >= y - 2 && e->y < y + 42) elegido = i;
                fila++;
            }
            if (elegido >= 0) { desc_sel = elegido; painel_desenhar(); return true; }
        }
        for (int b = 0; b < pan_nbot; b++) if (botao_tratar(j, &pb[b], e)) {
            if (desc_sel < 0 || desc_sel >= qtd_desc) return true;
            desc_t *d = &desc[desc_sel];
            if (b == 0 && (d->estado == D_BAIXANDO || d->estado == D_PAUSADA)) { d->pausa = !d->pausa; registrar("cipo: descarga %s %s\n", d->nome, d->pausa ? "en pausa" : "reanudada"); }
            if (b == 1 && (d->estado == D_BAIXANDO || d->estado == D_PAUSADA)) { d->cancela = 1; registrar("cipo: descarga cancelada por el usuario %s\n", d->nome); }
            if (b == 2) {
                char dir[160];
                copia_texto(dir, d->caminho, sizeof dir);
                for (int k = (int)tam_texto(dir) - 1; k > 0; k--) if (dir[k] == '/') { dir[k] = 0; break; }
                char *args[3] = { "arquivos", dir, NULL };
                int pid = executar("/bin/arquivos", args);
                registrar("cipo: abrir carpeta %s (pid %d)\n", dir, pid);
            }
            if (b == 3 && d->estado >= D_PRONTA) d->estado = D_LIVRE;
            painel_desenhar();
        }
        return true;
    }
    case P_GERAL: {
        if (combo_tratar(j, &cb_inicio, e) == COMBO_SELECIONOU) { conf.inicio_modo = cb_inicio.sel; gravar_conf(); }
        combo_tratar(j, &cb_busc, e);
        char a1[sizeof cx_inicio.texto], a2[sizeof cx_desc.texto];
        copia_texto(a1, cx_inicio.texto, sizeof a1); copia_texto(a2, cx_desc.texto, sizeof a2);
        caixa_tratar(j, &cx_inicio, e);
        caixa_tratar(j, &cx_desc, e);
        if (compara_texto(a1, cx_inicio.texto) != 0) { copia_texto(conf.inicio_url, cx_inicio.texto, sizeof conf.inicio_url); gravar_conf(); }
        if (compara_texto(a2, cx_desc.texto) != 0) { copia_texto(conf.pasta_desc, cx_desc.texto, sizeof conf.pasta_desc); gravar_conf(); }
        if (casilla_tratar(&cs_barra, e)) { conf.barra_marc = cs_barra.marcada; barra_marc_mudou(); return true; }
        if (botao_tratar(j, &pb[0], e)) {
            char c[96];
            copia_texto(c, cx_desc.texto, sizeof c);
            if (dialogo_carpeta(_("Carpeta de descargas"), c, c, sizeof c)) { copia_texto(conf.pasta_desc, c, sizeof conf.pasta_desc); gravar_conf(); caixa_definir(&cx_desc, c); }
        }
        painel_desenhar();
        return true;
    }
    case P_PRIV: {
        for (int b = 0; b < pan_nbot; b++) if (botao_tratar(j, &pb[b], e)) {
            if (b == 0) {
                info_bloqueo_t bi;
                memset(&bi, 0, sizeof bi);
                bloqueo_dns(BLOQ_ESTADO, &bi, sizeof bi);
                char *args[3] = { "bloqueo", bi.activo ? "desactivar" : "activar", NULL };
                int pid = executar("/bin/bloqueo", args);
                if (pid > 0) esperar(pid, NULL);
                registrar("cipo: bloqueo de publicidad %s\n", bi.activo ? "desactivado" : "activado");
            }
            if (b == 1 && confirmar("Cipó", _("¿Borrar todo el historial?"))) { qtd_hst = 0; gravar_hst(); registrar("cipo: historial borrado\n"); }
            if (b == 2) { qtd_pos = 0; gravar_posicoes(); registrar("cipo: posiciones de lectura olvidadas\n"); }
            if (b == 3 && confirmar("Cipó", _("¿Borrar las cookies y la caché?"))) { cookie_limpar(); rede_cache_limpar(); registrar("cipo: cookies y cache borrados\n"); }
            painel_desenhar();
        }
        return true;
    }
    case P_REND: {
        combo_tratar(j, &cb_hilos, e);
        if (casilla_tratar(&cs_js, e)) { conf.js = cs_js.marcada; gravar_conf(); registrar("cipo: JavaScript %s\n", conf.js ? "si" : "no"); painel_desenhar(); }
        if (casilla_tratar(&cs_img, e)) { conf.imagens = cs_img.marcada; gravar_conf(); registrar("cipo: imagenes %s\n", conf.imagens ? "si" : "no"); painel_desenhar(); }
        return true;
    }
    }
    return false;
}

/* ---- menús: «≡», pestañas, estrella y carpetas de la barra de marcadores (D373) ---- */

static int aba_menu_i = -1;
static int pasta_menu_idx[MARC_MAX];

static void quitar_marcador(int i)
{
    if (i < 0 || i >= qtd_marcadores) return;
    for (int k = i; k < qtd_marcadores - 1; k++) {
        copia_texto(marcador_url[k], marcador_url[k + 1], URL_MAX); copia_texto(marcador_titulo[k], marcador_titulo[k + 1], 96); copia_texto(marcador_pasta[k], marcador_pasta[k + 1], 32);
    }
    qtd_marcadores--;
    gravar_marcadores();
    registrar("cipo: marcador quitado, quedan %d\n", qtd_marcadores);
}

static void menu_principal(void)
{
    char t[640];
    int k = 0;
    k += formatar_texto(t + k, (int)sizeof t - k, "%s\n", _("Nueva pestaña\tCtrl+T"));
    k += formatar_texto(t + k, (int)sizeof t - k, "%s\n", _("Marcadores\tCtrl+B"));
    k += formatar_texto(t + k, (int)sizeof t - k, "%s\n", _("Historial\tCtrl+H"));
    k += formatar_texto(t + k, (int)sizeof t - k, "%s\n", _("Descargas\tCtrl+J"));
    k += formatar_texto(t + k, (int)sizeof t - k, "%s\n", _("Ajustes"));
    k += formatar_texto(t + k, (int)sizeof t - k, "%s\n", conf.barra_marc ? _("Ocultar la barra de marcadores") : _("Mostrar la barra de marcadores"));
    k += formatar_texto(t + k, (int)sizeof t - k, "%s\n", _("Buscar en la página\tCtrl+F"));
    k += formatar_texto(t + k, (int)sizeof t - k, "%s\n", _("Acercar\tCtrl++"));
    k += formatar_texto(t + k, (int)sizeof t - k, "%s\n", _("Alejar\tCtrl+-"));
    k += formatar_texto(t + k, (int)sizeof t - k, "%s", _("Tamaño normal\tCtrl+0"));
    menu_contextual(j->id, br[B_MENU].x - 150, ABA_A + BARRA_A, t, COD_MENU_PRINCIPAL);
}

static void abrir_gestor_marcadores(void) { mostrar_painel(P_MARC); }

static void acao_menu_principal(int k)
{
    if (k == 0) nova_aba();
    else if (k == 1) abrir_gestor_marcadores();
    else if (k == 2) mostrar_painel(P_HIST);
    else if (k == 3) mostrar_painel(P_DESC);
    else if (k == 4) mostrar_painel(P_GERAL);
    else if (k == 5) { conf.barra_marc = !conf.barra_marc; barra_marc_mudou(); }
    else if (k == 6) abrir_busca();
    else if (k == 7) zoom_paso(1);
    else if (k == 8) zoom_paso(-1);
    else if (k == 9) zoom_paso(0);
}

static void menu_aba(int i, int x, int y)
{
    char t[300];
    aba_menu_i = i;
    formatar_texto(t, sizeof t, "%s\n%s\n%s\n%s", _("Recargar"), _("Duplicar"), _("Cerrar"), _("Cerrar las demás"));
    menu_contextual(j->id, x, y, t, COD_MENU_ABA);
}

static void acao_menu_aba(int k)
{
    int i = aba_menu_i;
    aba_menu_i = -1;
    if (i < 0 || i >= qtd_abas) return;
    if (k == 0) { trocar_aba(i); recarregar(); }
    else if (k == 1) { char u[URL_MAX]; copia_texto(u, abas[i].url, sizeof u); if (i == aba_atual) copia_texto(u, url_atual, sizeof u); nova_aba_com_url(u); }
    else if (k == 2) fechar_aba_idx(i);
    else if (k == 3) {
        for (int m = qtd_abas - 1; m >= 0; m--) {
            if (m == i) continue;
            fechar_aba_idx(m);
            if (m < i) i--;
        }
    }
}

static const char *rotulo60(int i)
{
    static char r[64];
    copia_texto(r, marcador_titulo[i][0] ? marcador_titulo[i] : marcador_url[i], sizeof r);
    return r;
}

static void menu_pasta(int p, int x, int y)
{
    char t[1200];
    int k = 0, n = 0;
    for (int i = 0; i < qtd_marcadores && n < 20 && k < (int)sizeof t - 140; i++) {
        if (compara_texto(marcador_pasta[i], pastas[p]) != 0) continue;
        k += formatar_texto(t + k, (int)sizeof t - k, "%s%s", n ? "\n" : "", rotulo60(i));
        pasta_menu_idx[n++] = i;
    }
    if (!n) return;
    menu_contextual(j->id, x, y, t, COD_MENU_PASTA);
}

/* Interpreta un clic sobre la barra de herramientas; true = lo tratamos. */
static bool clique_barra(const evento_t *e)
{
    if (e->y < ABA_A || e->y >= ABA_A + BARRA_A) return false;
    for (int k = 0; k < B_N; k++) {
        if (e->x < br[k].x || e->x >= br[k].x + br[k].l || e->y < br[k].y || e->y >= br[k].y + br[k].a) continue;
        sugestoes_fechar();
        switch (k) {
        case B_VOLTAR: voltar(); break;
        case B_AVANCAR: avancar(); break;
        case B_RECARGAR:
            if (carregando_agora()) {
                recursos_pendentes = pintura_pendente = false;
                abas[aba_atual].carregando = false;
                pf_limpar();
                rodape("Carga detenida");
                registrar("cipo: carga detenida\n");
                desenhar_barra(); desenhar_abas();
            } else recarregar();
            break;
        case B_INICIO: navegar(url_inicio()); break;
        case B_CANDADO:
            if (comeca_com(url_atual, "https://") && cert_texto[0]) mensagem(_("Certificado del sitio"), cert_texto);
            else if (comeca_com(url_atual, "http://")) mensagem(_("Conexión no segura"), _("Esta página se cargó sin cifrar (HTTP): lo que envíes o recibas puede leerlo otra persona de la red."));
            else mensagem("Cipó", _("Página interna de Cipó."));
            break;
        case B_ESTRELA:
            if (!comeca_com(url_atual, "http")) { rodape("Solo se marcan páginas de la web"); break; }
            if (marcado_atual()) {
                char t[160];
                formatar_texto(t, sizeof t, "%s\n%s", _("Editar marcador..."), _("Quitar marcador"));
                menu_contextual(j->id, br[B_ESTRELA].x - 100, ABA_A + BARRA_A, t, COD_MENU_ESTRELA);
            } else { adicionar_marcador(); desenhar_barra(); }
            break;
        case B_MENU: menu_principal(); break;
        }
        return true;
    }
    return false;
}

/* ---- Zoom de la página (D489) ----
 * Ctrl++ / Ctrl+- / Ctrl+0 (y Ctrl+rueda): el motor maqueta otra vez con las medidas absolutas
 * multiplicadas, así que el texto se vuelve a repartir en líneas y no se estira un mapa de bits.
 * El zoom es por sitio (el servidor, sin el puerto) y se guarda en ~/.config/cipo/zoom. */
#define ZOOM_MAX_SITIOS 64
static char zoom_sitio[ZOOM_MAX_SITIOS][64];
static int zoom_sitio_pct[ZOOM_MAX_SITIOS];
static int zoom_qtd;
static const int zoom_pasos[] = { 25, 33, 50, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300, 400, 500 };
#define ZOOM_PASOS ((int)(sizeof zoom_pasos / sizeof zoom_pasos[0]))

/* El servidor de una dirección ("http://host:8080/x" -> "host"); vacío si no es de la web. */
static void sitio_de(const char *url, char *o, int max)
{
    o[0] = 0;
    const char *p = url;
    if (comeca_com(p, "http://")) p += 7;
    else if (comeca_com(p, "https://")) p += 8;
    else return;
    int n = 0;
    while (p[n] && p[n] != '/' && p[n] != ':' && p[n] != '?' && p[n] != '#' && n < max - 1) { o[n] = p[n]; n++; }
    o[n] = 0;
}

static int zoom_do_sitio(const char *url)
{
    char h[64];
    sitio_de(url, h, sizeof h);
    for (int i = 0; h[0] && i < zoom_qtd; i++) if (compara_texto(zoom_sitio[i], h) == 0) return zoom_sitio_pct[i];
    return 100;
}

static void caminho_zoom(char *saida, int max)
{
    char c[128];
    caminho_posicoes(c, sizeof c);            /* crea ~/.config/cipo */
    int n = (int)tam_texto(c);
    while (n && c[n - 1] != '/') n--;
    c[n] = 0;
    formatar_texto(saida, max, "%szoom", c);
}

static void zoom_gravar(void)
{
    char caminho[128];
    caminho_zoom(caminho, sizeof caminho);
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    for (int i = 0; i < zoom_qtd; i++) {
        char linha[96];
        int n = formatar_texto(linha, sizeof linha, "%s\t%d\n", zoom_sitio[i], zoom_sitio_pct[i]);
        escrever_tudo(fd, linha, (unsigned)n);
    }
    fechar(fd);
}

static void zoom_ler(void)
{
    zoom_qtd = 0;
    char caminho[128];
    caminho_zoom(caminho, sizeof caminho);
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return;
    char linha[96];
    while (zoom_qtd < ZOOM_MAX_SITIOS) {
        int n = ler_linha(fd, linha, sizeof linha);
        if (n < 0) break;
        char *sep = NULL;
        for (char *p = linha; *p; p++) if (*p == '\t') sep = p;
        if (!sep) continue;
        *sep = 0;
        int v = num_de(sep + 1, 25, 500, 100);
        if (!linha[0] || tam_texto(linha) >= 64) continue;
        copia_texto(zoom_sitio[zoom_qtd], linha, 64);
        zoom_sitio_pct[zoom_qtd++] = v;
    }
    fechar(fd);
}

/* Fija el zoom del sitio de la página actual (100 = lo quita de la lista). */
static void zoom_fijar(int pct)
{
    char h[64];
    sitio_de(url_atual, h, sizeof h);
    if (!h[0] || !motor) { rodape("El zoom solo vale en páginas de la web"); return; }
    if (pct < 25) pct = 25;
    if (pct > 500) pct = 500;
    int antes = motor_zoom_atual(motor);
    int i = 0;
    while (i < zoom_qtd && compara_texto(zoom_sitio[i], h) != 0) i++;
    if (pct == 100) {
        if (i < zoom_qtd) {
            for (int k = i; k < zoom_qtd - 1; k++) { copia_texto(zoom_sitio[k], zoom_sitio[k + 1], 64); zoom_sitio_pct[k] = zoom_sitio_pct[k + 1]; }
            zoom_qtd--;
        }
    } else {
        if (i == zoom_qtd) {
            if (zoom_qtd == ZOOM_MAX_SITIOS) {   /* llena: sale el más antiguo */
                for (int k = 0; k < zoom_qtd - 1; k++) { copia_texto(zoom_sitio[k], zoom_sitio[k + 1], 64); zoom_sitio_pct[k] = zoom_sitio_pct[k + 1]; }
                zoom_qtd--;
            }
            i = zoom_qtd++;
        }
        copia_texto(zoom_sitio[i], h, 64);
        zoom_sitio_pct[i] = pct;
    }
    zoom_gravar();
    if (pct == antes) return;
    /* la posición de lectura se conserva en proporción a la altura de la página */
    int altura_antes = motor_altura > 0 ? motor_altura : 1;
    int rol_antes = rol_px;
    motor_zoom(motor, pct);
    motor_altura = motor_layout(motor, LARG - ROLAGEM, AREA_A);
    if (motor_altura < 0) motor_altura = 0;
    linhas = (motor_altura + LINHA_A - 1) / LINHA_A + 1;
    int max = motor_altura > AREA_A ? motor_altura - AREA_A : 0;
    rol_px = (int)((int64_t)rol_antes * motor_altura / altura_antes);
    if (rol_px > max) rol_px = max;
    topo = (rol_px + LINHA_A / 2) / LINHA_A;
    char t[48];
    formatar_texto(t, sizeof t, "Zoom: %d%%", pct);
    registrar("cipo: zoom %d%% en %s (la página mide %d px)\n", pct, h, motor_altura);
    desenhar_pagina();
    rodape(t);
}

/* dir > 0 acerca un paso, dir < 0 aleja, 0 vuelve al 100 %. */
static void zoom_paso(int dir)
{
    int pct = motor ? motor_zoom_atual(motor) : 100;
    if (dir == 0) { zoom_fijar(100); return; }
    int i = 0;
    while (i < ZOOM_PASOS - 1 && zoom_pasos[i] < pct) i++;
    if (dir > 0) { while (i < ZOOM_PASOS - 1 && zoom_pasos[i] <= pct) i++; }
    else { while (i > 0 && zoom_pasos[i] >= pct) i--; }
    zoom_fijar(zoom_pasos[i]);
}

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
    if (js) { qjs_definir_janela(js, LARG - ROLAGEM, AREA_A); qjs_despachar(js, -1, "resize", "{}"); js_refrescar(); }   /* D488 */
    if (painel) painel_layout();
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
    if ((m & 4) && t == TECLA_HOME) { navegar(url_inicio()); return true; }
    if (t == TECLA_F5_CIPO || t == 18) { recarregar(); return true; }          /* F5, Ctrl+R */
    if (t == 12) { ir_para_endereco(); return true; }                          /* Ctrl+L */
    if (t == 6) { abrir_busca(); return true; }                                /* Ctrl+F */
    if (t == 3) { copiar_agora(); return true; }                                /* Ctrl+C */
    if (t == 20) { nova_aba(); return true; }                                   /* Ctrl+T */
    if ((m & 2) && (t == '+' || t == '=')) { zoom_paso(1); return true; }        /* Ctrl++ (D489) */
    if ((m & 2) && (t == '-' || t == '_')) { zoom_paso(-1); return true; }       /* Ctrl+- */
    if ((m & 2) && t == '0') { zoom_paso(0); return true; }                      /* Ctrl+0 */
    if ((m & 2) && (t == 8 || t == 'h')) { mostrar_painel(P_HIST); return true; }      /* Ctrl+H: historial con búsqueda */
    if ((m & 2) && (t == 10 || t == 'j')) { mostrar_painel(P_DESC); return true; }     /* Ctrl+J: descargas */
    if (t == 23) { fechar_aba(); return true; }                                 /* Ctrl+W */
    if (t == 9 && (m & 2)) {                                                    /* Ctrl+Tab */
        trocar_aba((m & 1) ? (aba_atual + qtd_abas - 1) % qtd_abas : (aba_atual + 1) % qtd_abas);
        return true;
    }
    if (!a_escrever) {
        if (t == 4) { adicionar_marcador(); return true; }                      /* Ctrl+D */
        if (t == 2 && (m & 1)) { abrir_gestor_marcadores(); return true; }       /* Ctrl+Mayús+B: el gestor */
        if (t == 2) { mostrar_marcadores(); return true; }                      /* Ctrl+B */
    }
    if (t == TECLA_F3_CIPO) {
        if (!buscando) abrir_busca();
        else buscar_proxima((m & 1) ? -1 : 1);
        return true;
    }
    return false;
}

/* El indicador de carga (D373): el giro de las pestañas y el icono de recargar/parar. */
static void indicadores_tick(void)
{
    static unsigned ult;
    static bool car_ant;
    bool car = carregando_agora();
    if (car != car_ant) { car_ant = car; desenhar_barra(); desenhar_abas(); ult = tempo_ms(); return; }
    if (car && tempo_ms() - ult >= 100) { ult = tempo_ms(); desenhar_abas(); }
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
    ler_conf();
    const char *url_arg = NULL;           /* cipo [--hilos N] [dirección]: --hilos fija las descargas simultáneas solo esta vez */
    for (int i = 1; i < argc; i++) {
        if (!compara_texto(argv[i], "--hilos") && i + 1 < argc) { conf.hilos = num_de(argv[i + 1], 1, HILOS_MAX, 1); i++; }
        else url_arg = argv[i];
    }
    ler_hst();
    ler_marcadores();
    ler_posicoes();
    zoom_ler();
    rede_cookie_carregar();                 /* las cookies persistentes del usuario (D487) */
    rede_definir_executor(rede_js_executor);
    ws_definir_trava(&tls_trava);
    registrar("cipo: ajustes: inicio %d, buscador %s, descargas simultáneas %d (efectivas %d), js %d, imágenes %d\n",
              conf.inicio_modo, buscadores[conf.buscador].nome, conf.hilos, hilos_efectivos(), conf.js, conf.imagens);
    ajustar_barra();
    qtd_abas = 1;
    aba_atual = 0;
    carregar(url_arg ? url_arg : url_inicio());
    aba_guardar();          /* a primeira aba fica com a página */
    desenhar_abas();
    jan_mostrar(j);

    evento_t e;
    unsigned ultimo_ws_ms = 0;
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
                pedido_por_js = false;
                continue;
            }
        }
        /* As animações da página andam com o relógio: se alguma mudou, repinta-se
         * a banda visível (sem recalcular o layout). */
        if (!painel && animacoes_tick()) { desenhar_pagina(); continue; }
        video_tick();
        /* El script pidió history.back/forward (D488): fuera del script. */
        if (js_hist_pendente) { int d = js_hist_pendente; js_hist_pendente = 0; if (d < 0) voltar(); else avancar(); continue; }
        /* Los WebSocket de la página (D487): el sondeo es una tarea más del bucle (qjs_chamar: con sus
         * microtareas detrás); los mensajes y las promesas entran así en la cola del script (D488). */
        if (js && ws_ativos() > 0 && tempo_ms() - ultimo_ws_ms >= 15) {
            ultimo_ws_ms = tempo_ms();
            qjs_chamar(js, "__sagui_ws_poll", NULL);
            js_refrescar();
        }
        /* El bucle de eventos del script (D488): temporizadores, quadros y el evento scroll. */
        js_volta();
        if (proximo_evento(j->id, &e, false) != 1) {
            indicadores_tick();
            if (pintura_pendente || recursos_pendentes) { processar_recurso(); continue; }
            if (!painel && qtd_abas && !abas[aba_atual].fav_tentado && doc && !tem_url_pendente) { favicon_buscar(); desenhar_abas(); continue; }
            if (painel == P_DESC && desc_activas() && tempo_ms() - ultimo_desc_ms > 300) { ultimo_desc_ms = tempo_ms(); painel_desc_desenhar(); }
            if (esperar_evento(j->id, &e, js_espera_ms(vid_tocando ? 4 : 15)) != 1) continue;
        }
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_MENU && e.tecla == COD_MENU_CONTEXTUAL) {
            /* O menu do botão direito (D152): cada linha tem a sua acção. */
            if (e.x >= 0 && e.x < qtd_menu_acoes) {
                int acao = menu_acoes[e.x];
                if (acao == ACAO_GUARDAR_IMAGEM) guardar_recurso(salvar_imagem_url);
                else if (acao == ACAO_GUARDAR_LINK) guardar_recurso(salvar_link_url);
                else if (acao == ACAO_ABA_NOVA) abrir_link_nova_aba(salvar_link_url);
                else if (acao == ACAO_DESCARGAR) { desc_iniciar(salvar_link_url); mostrar_painel(P_DESC); }
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
        if (e.tipo == EV_MENU && e.tecla == COD_MENU_ABA) { if (e.x >= 0) acao_menu_aba(e.x); continue; }
        if (e.tipo == EV_MENU && e.tecla == COD_MENU_PRINCIPAL) { if (e.x >= 0) acao_menu_principal(e.x); continue; }
        if (e.tipo == EV_MENU && e.tecla == COD_MENU_ESTRELA) {
            for (int m = 0; m < qtd_marcadores && e.x >= 0; m++)
                if (compara_texto(marcador_url[m], url_atual) == 0) { if (e.x == 0) editar_marcador(m); else quitar_marcador(m); break; }
            desenhar_barra();
            continue;
        }
        if (e.tipo == EV_MENU && e.tecla == COD_MENU_PASTA) {
            if (e.x >= 0 && e.x < MARC_MAX && pasta_menu_idx[e.x] < qtd_marcadores) { char u[URL_MAX]; copia_texto(u, marcador_url[pasta_menu_idx[e.x]], sizeof u); navegar(u); }
            continue;
        }
        if (jan_redimensionar(j, &e)) { novo_tamanho(); continue; }
        if (painel && painel_evento(&e)) continue;
        if (e.tipo == EV_RODA && e.y < ABA_A) {   /* la rueda sobre las pestañas pasa de una a otra */
            int n = (int)e.cliques;
            if (n && qtd_abas > 1) trocar_aba(((aba_atual + (n < 0 ? 1 : -1)) % qtd_abas + qtd_abas) % qtd_abas);
            continue;
        }
        if (e.tipo == EV_RODA && (e.modificadores & 2) && e.y >= ABA_Y && e.y < ALT - RODAPE) {   /* Ctrl+rueda: zoom (D489) */
            zoom_paso((int)e.cliques < 0 ? 1 : -1);
            continue;
        }
        if (e.tipo == EV_RODA) {
            int n = (int)e.cliques;
            if (e.y >= ABA_Y && e.y < ALT - RODAPE && motor && e.x < LARG - ROLAGEM &&
                motor_rolar_caixa(motor, e.x, e.y - ABA_Y + rol_px, 0, n * RODA_LINHAS * LINHA_A)) {   /* un bloque con overflow se queda la rueda (D489) */
                desenhar_pagina();
                registrar("cipo: rueda %d sobre un bloque con overflow\n", n);
            } else if (e.y >= ABA_Y && e.y < ALT - RODAPE) {
                rolar(topo + n * RODA_LINHAS);
                registrar("cipo: rueda %d, linea %d de %d\n", n, topo, linhas);
            }
            continue;
        }
        if (e.tipo == EV_SOLTOU && motor && motor_barra_arrastando(motor)) { motor_barra_soltar(motor); continue; }
        if (e.tipo == EV_SOLTOU && rolando) { rolando = false; desenhar_barra_rolagem(); jan_mostrar_area(j, LARG - ROLAGEM, ABA_Y, ROLAGEM, AREA_A); continue; }
        if (e.tipo == EV_TECLA) {
            unsigned t = e.tecla;
            if (atalho(&e)) continue;
            if (buscando && caixa_busca.foco) {
                if (t == TECLA_ESC || t == 27) { fechar_busca(); continue; }
                if (t == '\n') { buscar_proxima((e.modificadores & 1) ? -1 : 1); continue; }
                if (t == TECLA_CIMA || t == TECLA_BAIXO || t == TECLA_PGUP || t == TECLA_PGDN) { /* rueda de la página */ }
                else { caixa_tratar(j, &caixa_busca, &e); atualizar_busca(); continue; }
            }
            /* la página tiene oyentes de teclado (D488): la tecla es suya primero */
            if (js && motor && !endereco.foco && (motor_tem_foco(motor) || (qjs_quer(js) & QJS_QUER_TECLA)) && js_tecla(t, e.modificadores)) continue;
            if (motor && motor_tem_foco(motor)) { tecla_campo_motor(t); continue; }
            if (endereco.foco) {
                if (t == '\t' || t == TECLA_ESC || t == 27) { endereco.foco = false; endereco_sel = false; sugestoes_fechar(); desenhar_barra(); continue; }
                if (sug_n && (t == TECLA_BAIXO || t == TECLA_CIMA)) {
                    sug_sel += t == TECLA_BAIXO ? 1 : -1;
                    if (sug_sel < -1) sug_sel = sug_n - 1;
                    if (sug_sel >= sug_n) sug_sel = -1;
                    sugestoes_desenhar();
                    continue;
                }
                if (t == '\n' && sug_n && sug_sel >= 0) { char u[URL_MAX]; copia_texto(u, sug_url[sug_sel], sizeof u); sugestoes_fechar(); navegar(u); continue; }
                if (endereco_sel) {
                    bool substitui = (t >= 0x20 && t < 0x100 && t != 0x7F) || t == '\b' || t == TECLA_DELETE || t == 22;
                    endereco_sel = false;
                    if (substitui) caixa_definir(&endereco, "");
                    else desenhar_barra();
                }
                if (caixa_tratar(j, &endereco, &e) && endereco.texto[0]) {
                    char u[URL_MAX];
                    interpretar_entrada(endereco.texto, u, sizeof u);
                    sugestoes_fechar();
                    navegar(u);
                    continue;
                }
                int antes = sug_n;
                sugestoes_calcular();
                if (sug_n < antes) { desenhar_pagina(); }
                if (sug_n) sugestoes_desenhar();
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
        if (e.tipo == EV_CLIQUE) { mouse_x = e.x; mouse_y = e.y; if (e.botoes != 1) registrar("cipo: clic botones %u en (%d,%d)\n", e.botoes, e.x, e.y); }
        if (e.tipo == EV_MOVER) {
            int hb = -1;
            for (int k = 0; k < B_N; k++) if (e.x >= br[k].x && e.x < br[k].x + br[k].l && e.y >= br[k].y && e.y < br[k].y + br[k].a) hb = k;
            if (hb != hover_b) { hover_b = hb; desenhar_barra(); }
            if (arrasto >= 0 && (e.botoes & 1)) {     /* arrastrar una pestaña para reordenarla */
                int slot = aba_ini + (e.x - 4) / aba_l;
                if (slot < 0) slot = 0;
                if (slot >= qtd_abas) slot = qtd_abas - 1;
                if (slot != arrasto) { mover_aba(arrasto, slot); arrasto = slot; }
                continue;
            }
        }
        if (e.tipo == EV_SOLTOU) arrasto = -1;
        /* Las sugerencias de la dirección: un clic en una de ellas la abre. */
        if (e.tipo == EV_CLIQUE && sug_n) {
            int y0 = ABA_A + BARRA_A + (conf.barra_marc ? MARCB_A : 0);
            if (e.x >= pilula_x && e.x < pilula_x + pilula_l && e.y >= y0 && e.y < y0 + sug_n * 20 + 4) {
                int k = (e.y - y0 - 2) / 20;
                if (k >= 0 && k < sug_n) { char u[URL_MAX]; copia_texto(u, sug_url[k], sizeof u); sugestoes_fechar(); navegar(u); }
                continue;
            }
        }
        /* La tira de pestañas: clic = cambiar (y empezar a arrastrar), × o clic central = cerrar,
         * botón derecho = menú, «+» = nueva. */
        if (e.tipo == EV_CLIQUE && e.y < ABA_A) {
            sugestoes_fechar();
            bool no_x;
            int idx = aba_no_clic(e.x, &no_x);
            if (e.botoes & 4) { if (idx >= 0) fechar_aba_idx(idx); continue; }
            if (e.botoes & 2) { if (idx >= 0) menu_aba(idx, e.x, ABA_A); continue; }
            if (idx == -2) { nova_aba(); continue; }
            if (idx >= 0) {
                if (no_x) { fechar_aba_idx(idx); continue; }
                trocar_aba(idx);
                arrasto = idx;
            }
            continue;
        }
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && clique_barra(&e)) continue;
        if (e.tipo == EV_CLIQUE && (e.botoes & 1)) {      /* la barra de marcadores */
            int ci = marcb_em(e.x, e.y);
            if (ci >= 0) {
                if (marcb_idx[ci] >= 0) { char u[URL_MAX]; copia_texto(u, marcador_url[marcb_idx[ci]], sizeof u); navegar(u); }
                else menu_pasta(-1 - marcb_idx[ci], marcb_x[ci], ABA_A + BARRA_A + MARCB_A);
                continue;
            }
        }
        if (e.tipo == EV_CLIQUE && (e.botoes & 4) && motor) {   /* clic central en un enlace: pestaña nueva */
            const motor_alvo_t *a = alvo_motor_em(e.x, e.y);
            if (a && a->tipo == MA_LINK && a->href) abrir_link_nova_aba(a->href);
            continue;
        }
        if (e.tipo == EV_CLIQUE && (e.botoes & 2) && motor) {
            if (js_menu_pagina(e.x, e.y, e.modificadores)) continue;     /* contextmenu cancelado (D488) */
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
                k += formatar_texto(itens + k, (int)sizeof itens - k, "Descargar enlace\n");
                menu_acoes[qtd_menu_acoes++] = ACAO_DESCARGAR;
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
            if (caixa_tratar(j, &endereco, &e) && endereco.texto[0]) { char u[URL_MAX]; interpretar_entrada(endereco.texto, u, sizeof u); navegar(u); continue; }
            if (!endereco.foco && sug_n) sugestoes_fechar();
            if (!tinha && endereco.foco) {   /* primer clic en la dirección: queda seleccionada, como en los navegadores */
                endereco.cursor = (int)tam_texto(endereco.texto);
                endereco_sel = true;
                desenhar_barra();
            }
        }
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
            if (motor && e.y >= ABA_Y && e.y < ALT - RODAPE && e.x < LARG - ROLAGEM && motor_barra_pulsar(motor, e.x, e.y - ABA_Y + rol_px)) {   /* barra de un bloque con overflow (D489) */
                desenhar_pagina();
                continue;
            }
            if (video_clique(e.x, e.y)) continue;
            /* el JavaScript ve el clic primero (mousedown, mouseup, click; D488) */
            if (js_clique_pagina(e.x, e.y, (int)e.cliques, e.modificadores)) {
                if (alvo_motor_em(e.x, e.y)) registrar("cipo: js: el clic lo cancelo el script\n");
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
            if (motor && motor_barra_arrastando(motor)) {
                if (e.botoes & 1) motor_barra_mover(motor, e.x, e.y - ABA_Y + rol_px);
                else motor_barra_soltar(motor);
                desenhar_pagina();
                continue;
            }
            if (rolando) {
                if (e.botoes & 1) rolar_para_pulgar(e.y);
                else { rolando = false; desenhar_pagina(); }
                continue;
            }
            js_mover_rato(e.x, e.y, (int)e.botoes);     /* mousemove, over, out... (D488) */
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
    rede_cookie_gravar(true);
    video_parar();
    liberar_imagens();
    jan_fechar(j);
    return 0;
}
