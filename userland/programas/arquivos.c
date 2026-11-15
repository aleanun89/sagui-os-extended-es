// SPDX-License-Identifier: GPL-3.0-only
/* arquivos [carpeta] - el explorador de archivos de SaguiOS (H23; mejora 058, D136).
 *
 * Una ventana de verdad: barra de herramientas (atrás, adelante, arriba, nueva
 * carpeta, borrar, ocultos, vista y buscar), barra de ruta con las migas de pan
 * pulsables y editable (Ctrl+L), panel lateral con los lugares y las acciones,
 * lista de iconos, vista de detalles (ordenable por columna) y barra de estado
 * con el número de elementos y el espacio libre.
 *
 * Ver u ocultar los ocultos (Ctrl+H), selección múltiple (Ctrl y Mayús, Ctrl+A y
 * recuadro con el ratón), copiar, cortar y pegar (portapapeles de archivos propio,
 * con copia de carpetas), renombrar en el sitio (F2), nueva carpeta, borrar con
 * confirmación, propiedades, "Abrir con" y búsqueda por nombre con comodines.
 *
 * Lo que no cambia: el menú contextual (D051) tiene las mismas líneas de siempre y
 * los mensajes del registro ("arquivos: pronto", "arquivos: pasta ... (N itens)",
 * "arquivos: pasta criada ...", "arquivos: apagado ...") son los que las pruebas
 * de la H23, la H34, la H43, la H45 y la H55 esperan. */
#include "sagui.h"
#include "gui.h"
#include "icones.h"
#include "imagem.h"

#define LARG_INICIO 720
#define ALT_INICIO  440
#define LATERAL 140      /* panel lateral de lugares y acciones */
#define MAX_ITENS 256    /* elementos que se guardan de una carpeta */
#define ALT_BARRA 40     /* barra de herramientas */
#define ALT_ROTA  26     /* barra de ruta */
#define ALT_STATUS 24    /* barra de estado */
#define Y_LISTA  70      /* donde empieza la lista (igual que antes) */
#define LARG (j->larg)
#define ALT  (j->alt)

/* Vistas de la lista. */
enum { VISTA_LISTA, VISTA_DETALHES, VISTA_ICONES };
/* Columnas por las que se ordena. */
enum { ORD_NOME, ORD_TAMANHO, ORD_TIPO, ORD_DATA };
/* Qué hace el cuadro de la barra de ruta. */
enum { MODO_ROTA, MODO_BUSCA };
/* Códigos de los menús contextuales (el código vuelve en EV_MENU). */
#define MENU_ITEM  1
#define MENU_FUNDO 2
#define MENU_COM   3   /* "Abrir con..." */
#define MENU_GRUPO 4   /* accoes para varios elementos marcados (D139) */
#define MENU_MAX   64  /* líneas de un menú (las del kernel) */

#define MAX_HIST 32     /* carpetas visitadas (atrás y adelante) */
#define MAX_AREA 64     /* archivos del portapapeles */
#define MAX_MIGAS 16    /* trozos de la barra de ruta */
#define MAX_LADO 14
#define LARG_CELULA 92  /* largura de uma celula da vista de icones */
#define ALT_CELULA  74  /* altura de uma celula da vista de icones */
#define MAX_ARGS_GRUPO 64    /* argv do grupo: o tar aceita 64 rotas (D139) */     /* botones del panel lateral */

/* ------------------------------------------------------------ estado */

static jan_t *j;
static char pasta[160] = "/";
static char nomes[MAX_ITENS][64];
static uint8_t tipos[MAX_ITENS];
static uint32_t tamanhos[MAX_ITENS];
static uint32_t modificadas[MAX_ITENS];
static bool tem_data[MAX_ITENS];
static char rotas[MAX_ITENS][200];   /* ruta completa (en la búsqueda) */
static bool marcados[MAX_ITENS];     /* selección múltiple */
static int n_marcados;
static int ancora = -1;              /* ancla del rango (Mayús) */
static lista_t lista;
static bool ver_ocultos;
static int vista = VISTA_LISTA;
static int ordem = ORD_NOME;
static bool ordem_desc;
static char filtro[64];              /* filtro rápido mientras se escribe */
static char padrao_busca[64];        /* patrón de la búsqueda */
static int n_resultados;
static bool em_busca;
static info_disco_t pendrive;
static bool tem_pendrive;
static info_disco_t cdrom;           /* D233: el CD/DVD de datos que haya en un lector */
static bool tem_cd;
static char rodape[200];

/* Historial de carpetas: "atrás" y "adelante". */
static char hist[MAX_HIST][160];
static int hist_n, hist_pos = -1;
/* Portapapeles de archivos. */
static char area_caminhos[MAX_AREA][200];
static int area_n;
static bool area_mover;
/* Migas de pan de la barra de ruta. */
static struct { int x, l; char caminho[160]; } migas[MAX_MIGAS];
static int n_migas;
/* Cuadro de la barra de ruta (la ruta o la búsqueda). */
static caixa_t caixa;
static int modo_caixa = MODO_ROTA;

/* ------------------------------------------------------------ iconos */

/* Íconos 16x16 en pixel art (los que ya había; la biblioteca de D135 es para
 * los programas nuevos). */
static const char *const ic_pasta[] = {
    "................", "................", ".PPPPP..........", "PYYYYYP.........",
    "PYYYYYYPPPPPPPP.", "PYYYYYYYYYYYYYYP", "PPPPPPPPPPPPPPPP", "PAAAAAAAAAAAAAAP",
    "PAAAAAAAAAAAAAAP", "PAAAAAAAAAAAAAAP", "PAAAAAAAAAAAAAAP", "PAAAAAAAAAAAAAAP",
    "PAAAAAAAAAAAAAAP", "PPPPPPPPPPPPPPPP", "................", "................",
};
static const char *const ic_arquivo[] = {
    "..PPPPPPPPP.....", "..PBBBBBBBPP....", "..PBBBBBBBPBP...", "..PBBBBBBBPPPP..",
    "..PBCCCCCBBBBP..", "..PBBBBBBBBBBP..", "..PBCCCCCCCCBP..", "..PBBBBBBBBBBP..",
    "..PBCCCCCCCBBP..", "..PBBBBBBBBBBP..", "..PBCCCCCCCCBP..", "..PBCCCCCCCCBP..",
    "..PBCCCCCBBBBP..", "..PBBBBBBBBBBP..", "..PPPPPPPPPPPP..", "................",
};
static const char *const ic_programa[] = {
    "................", ".PPPPPPPPPPPPPP.", ".PVVVVVVVVVVVVP.", ".PPPPPPPPPPPPPP.",
    ".PDDDDDDDDDDDDP.", ".PDGDDDDDDDDDDP.", ".PDDGDDDDDDDDDP.", ".PDDDGDDDDDDDDP.",
    ".PDDGDDDDDDDDDP.", ".PDGDDGGGGDDDDP.", ".PDDDDDDDDDDDDP.", ".PDDDDDDDDDDDDP.",
    ".PPPPPPPPPPPPPP.", "................", "................", "................",
};

static uint32_t paleta(char c)
{
    switch (c) {
    case 'P': return 0x202428;
    case 'Y': return 0xFFE070;
    case 'A': return 0xF2C230;
    case 'B': return 0xFFFFFF;
    case 'C': return 0x9AA3AC;
    case 'V': return 0x2A6FD8;
    case 'D': return 0x182030;
    case 'G': return 0x3CDC6E;
    default:  return 0xFF000000;
    }
}

/* Dibuja el icono de un elemento (16x16, o a doble en la vista de iconos). */
static void icone_item(int x, int y, int i, int escala)
{
    const char *const *d = tipos[i] == TIPO_PASTA ? ic_pasta : tipos[i] == TIPO_PROGRAMA ? ic_programa : ic_arquivo;
    g_desenho(j, x, y, d, 16, 16, paleta, escala);
}

/* ------------------------------------------------------------ utilidades */

static bool eh_oculto(const char *nome)
{
    return nome[0] == '.';
}

/* 123456 -> "123,4 KB" (como el "df" del sistema). */
static void tamanho_kb(unsigned kb, char *s, size_t max)
{
    if (kb >= 1024u * 1024u)
        formatar_texto(s, max, "%u,%u GB", kb / (1024u * 1024u), (kb % (1024u * 1024u)) * 10u / (1024u * 1024u));
    else if (kb >= 1024u)
        formatar_texto(s, max, "%u,%u MB", kb / 1024u, (kb % 1024u) * 10u / 1024u);
    else formatar_texto(s, max, "%u KB", kb);
}

/* El espacio libre del volumen donde está la carpeta actual. */
static void espaco_livre(char *s, size_t max)
{
    info_montagem_t montagens[16];
    int n = info_sistema(INFO_MONTAGENS, montagens, 16);
    if (n > 0)
        for (int i = 0; i < n; i++) {
            if (!montagens[i].ponto[0] || !montagens[i].total_kb) continue;
            if (!comeca_com(pasta, montagens[i].ponto)) continue;
            char t[24];
            tamanho_kb(montagens[i].livre_kb, t, sizeof t);
            formatar_texto(s, max, "%s libres", t);
            return;
        }
    info_memoria_t mem;
    if (info_sistema(INFO_MEMORIA, &mem, sizeof mem) == 0) {
        char t[24];
        tamanho_kb(mem.livre_kb, t, sizeof t);
        formatar_texto(s, max, "%s libres (RAM)", t);
        return;
    }
    copia_texto(s, "espacio desconocido", max);
}

/* La ruta completa de un elemento (en la búsqueda ya lo está). */
static void caminho_de(int i, char *saida, size_t max)
{
    if (em_busca) { copia_texto(saida, rotas[i], max); return; }
    copia_texto(saida, pasta, max);
    if (compara_texto(pasta, "/") != 0) junta_texto(saida, "/", max);
    junta_texto(saida, nomes[i], max);
}

static const char *tipo_texto(int i)
{
    if (tipos[i] == TIPO_PASTA) return "carpeta";
    if (tipos[i] == TIPO_PROGRAMA) return "programa";
    return "archivo";
}

/* La fecha de modificación (UTC) de un elemento; se pide sólo cuando hace falta. */
static uint32_t data_de(int i)
{
    if (!tem_data[i]) {
        tem_data[i] = true;
        modificadas[i] = 0;
        datas_arquivo_t d;
        char c[200];
        caminho_de(i, c, sizeof c);
        if (datas_arquivo(c, &d) == 0) modificadas[i] = d.modificado;
    }
    return modificadas[i];
}

/* "10/11/2026 22:04" (hora local, D080). */
static void data_texto(int i, char *s, size_t max)
{
    uint32_t utc = data_de(i);
    if (!utc) { copia_texto(s, "-", max); return; }
    fuso_data_t l;
    hora_local(utc, &l);
    formatar_data(s, (int)max, "%d/%m/%Y %H:%M", &l);
}

/* ¿Está la aguja dentro del texto? (el filtro rápido no tiene comodines) */
static bool contem(const char *texto, const char *aguja)
{
    if (!aguja[0]) return true;
    size_t n = tam_texto(aguja);
    for (const char *p = texto; *p; p++) {
        if (!compara_texto_n(p, aguja, n)) return true;
    }
    return false;
}

/* El filtro rápido: si hay comodines, casan; si no, es un trozo del nombre. */
static bool casa_filtro(const char *filtro, const char *nome)
{
    if (!filtro[0]) return true;
    if (acha_char(filtro, '*') || acha_char(filtro, '?')) return casar_glob(filtro, nome);
    return contem(nome, filtro);
}

/* ------------------------------------------------------------ preferencias */

/* La preferencia se guarda en /home/.config/arquivos (disco persistente) y, si
 * /home no existe, en /etc/arquivos.conf (la capa de RAM del sistema). */
static const char *CAMINHO_PREF = "/home/.config/arquivos";
static const char *CAMINHO_PREF_ALT = "/etc/arquivos.conf";
static const char *CAMINHO_AREA = "/home/.config/arquivos.area";
static const char *CAMINHO_AREA_ALT = "/etc/arquivos.area";

static bool existe_pasta(const char *c)
{
    info_arquivo_t inf;
    return info_arquivo(c, &inf) == 0 && inf.tipo == TIPO_PASTA;
}

static const char *caminho_preferencia(bool escrever)
{
    if (existe_pasta("/home")) return CAMINHO_PREF;
    if (escrever && !existe_pasta("/tmp")) return 0;
    return CAMINHO_PREF_ALT;
}

/* Escribe un fichero de texto pequeño (una línea por cosa). */
static bool gravar_linhas(const char *caminho, const char *const *linhas, int n)
{
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return false;
    for (int i = 0; i < n; i++) {
        char buf[256];
        int t = formatar_texto(buf, sizeof buf, "%s\n", linhas[i]);
        if (escrever_tudo(fd, buf, (unsigned)t) < 0) { fechar(fd); return false; }
    }
    fechar(fd);
    return true;
}

/* Lee las líneas de un fichero de texto (devuelve cuántas; van separadas por un
 * cero, como las de /proc). */
static int ler_linhas(const char *caminho, char *buf, int max, int linhas_max)
{
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return 0;
    int n = 0, pos = 0, ini = 0;
    char c;
    while (n < linhas_max && pos < max - 1 && ler(fd, &c, 1) == 1) {
        if (c == '\n') { buf[pos++] = 0; n++; ini = pos; continue; }
        if (c == '\r') continue;
        buf[pos++] = c;
    }
    if (pos > ini && n < linhas_max) { buf[pos++] = 0; n++; }
    fechar(fd);
    return n;
}

static void ler_preferencias(void)
{
    char buf[1024];
    int n = ler_linhas(caminho_preferencia(false), buf, sizeof buf, 16);
    int ini = 0;
    for (int i = 0; i < n; i++) {
        char *l = buf + ini;
        ini += (int)tam_texto(l) + 1;
        if (comeca_com(l, "ocultos=")) ver_ocultos = l[8] == '1';
        else if (comeca_com(l, "vista=")) {
            if (compara_texto(l + 6, "detalhes") == 0) vista = VISTA_DETALHES;
            else if (compara_texto(l + 6, "icones") == 0) vista = VISTA_ICONES;
            else vista = VISTA_LISTA;
        } else if (comeca_com(l, "ordem=")) {
            if (compara_texto(l + 6, "tamanho") == 0) ordem = ORD_TAMANHO;
            else if (compara_texto(l + 6, "tipo") == 0) ordem = ORD_TIPO;
            else if (compara_texto(l + 6, "data") == 0) ordem = ORD_DATA;
            else ordem = ORD_NOME;
            ordem_desc = l[11] == '-';
        }
    }
}

static void guardar_preferencias(void)
{
    const char *c = caminho_preferencia(true);
    if (!c) return;
    char l1[32], l2[32], l3[32];
    formatar_texto(l1, sizeof l1, "ocultos=%d", ver_ocultos ? 1 : 0);
    formatar_texto(l2, sizeof l2, "vista=%s", vista == VISTA_DETALHES ? "detalhes" : vista == VISTA_ICONES ? "icones" : "lista");
    formatar_texto(l3, sizeof l3, "ordem=%s%s",
                    ordem == ORD_TAMANHO ? "tamanho" : ordem == ORD_TIPO ? "tipo" : ordem == ORD_DATA ? "data" : "nome",
                    ordem_desc ? "-" : "");
    const char *linhas[3] = { l1, l2, l3 };
    if (!gravar_linhas(c, linhas, 3)) return;
    /* en /home la carpeta .config puede no existir: se crea */
    if (compara_texto(c, CAMINHO_PREF) == 0 && !existe_pasta("/home/.config")) criar_pasta("/home/.config");
}

/* ------------------------------------------------------------ portapapeles */

/* El portapapeles de archivos: la primera línea dice si es copia o corte y las
 * demás son las rutas completas. */
static void ler_area(void)
{
    char buf[8192];
    const char *c = existe_pasta("/home") ? CAMINHO_AREA : CAMINHO_AREA_ALT;
    int n = ler_linhas(c, buf, sizeof buf, MAX_AREA + 1);
    area_n = 0;
    area_mover = false;
    int ini = 0;
    for (int i = 0; i < n && area_n < MAX_AREA; i++) {
        char *l = buf + ini;
        ini += (int)tam_texto(l) + 1;
        if (i == 0) { area_mover = compara_texto(l, "cortar") == 0; continue; }
        copia_texto(area_caminhos[area_n++], l, sizeof area_caminhos[0]);
    }
}

static void guardar_area(void)
{
    const char *c = existe_pasta("/home") ? CAMINHO_AREA : CAMINHO_AREA_ALT;
    int fd = abrir(c, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    char buf[256];
    int t = formatar_texto(buf, sizeof buf, "%s\n", area_mover ? "cortar" : "copiar");
    escrever_tudo(fd, buf, (unsigned)t);
    for (int i = 0; i < area_n; i++) {
        t = formatar_texto(buf, sizeof buf, "%s\n", area_caminhos[i]);
        escrever_tudo(fd, buf, (unsigned)t);
    }
    fechar(fd);
}

/* ------------------------------------------------------------ lista */

/* Compara dos elementos por la columna de orden (las carpetas siempre primero,
 * como las ponía el programa de siempre). */
static int comparar(int a, int b)
{
    bool pa = tipos[a] == TIPO_PASTA, pb = tipos[b] == TIPO_PASTA;
    if (pa != pb) return pa ? -1 : 1;
    int c = 0;
    switch (ordem) {
    case ORD_NOME:    c = compara_texto(nomes[a], nomes[b]); break;
    case ORD_TAMANHO: c = tamanhos[a] < tamanhos[b] ? -1 : tamanhos[a] > tamanhos[b] ? 1 : 0; break;
    case ORD_TIPO:    c = tipos[a] < tipos[b] ? -1 : tipos[a] > tipos[b] ? 1 : 0; break;
    default:          c = modificadas[a] < modificadas[b] ? -1 : modificadas[a] > modificadas[b] ? 1 : 0; break;
    }
    return ordem_desc ? -c : c;
}

static void trocar(int a, int b)
{
    char n[64];
    copia_texto(n, nomes[a], sizeof n);
    copia_texto(nomes[a], nomes[b], sizeof nomes[a]);
    copia_texto(nomes[b], n, sizeof nomes[b]);
    uint8_t t = tipos[a]; tipos[a] = tipos[b]; tipos[b] = t;
    uint32_t z = tamanhos[a]; tamanhos[a] = tamanhos[b]; tamanhos[b] = z;
    uint32_t d = modificadas[a]; modificadas[a] = modificadas[b]; modificadas[b] = d;
    bool f = tem_data[a]; tem_data[a] = tem_data[b]; tem_data[b] = f;
    char r[200];
    copia_texto(r, rotas[a], sizeof r);
    copia_texto(rotas[a], rotas[b], sizeof rotas[a]);
    copia_texto(rotas[b], r, sizeof rotas[b]);
}

static void ordenar(int n)
{
    for (int i = 1; i < n; i++) {
        int k = i;
        while (k > 0 && comparar(k - 1, k) > 0) { trocar(k - 1, k); k--; }
    }
}

static void limpar_marcados(void)
{
    for (int i = 0; i < MAX_ITENS; i++) marcados[i] = false;
    n_marcados = 0;
    ancora = -1;
}

/* ------------------------------------------------------------ dibujo */

static botao_t bt[8];
static const char *const ROT_BT[8] = { "Atrás", "Nueva carpeta", "Borrar", "Adelante", "Arriba", "Ocultos", "Vista", "Buscar" };
static const char *const IC_BT[8] = { "voltar", "pasta", "excluir", "avancar", "subir", "olho", "lista", "procurar" };

/* El ancho de un botón sale de su texto, no de un número fijo (D136): así
 * "Nueva carpeta" y "Adelante" ya no se salen y todos los botones tienen el
 * mismo margen. La letra no se puede hacer más pequeña: la fuente del
 * sistema es de 8x16 y es la única que hay. */
#define BT_MARGEM_ICON 4    /* del borde al icono */
#define BT_GAP_ICON    3    /* del icono al texto */
#define BT_MARGEM_FIM  6    /* del fin del texto al borde */
#define BT_ICONE_L     16   /* los icones de la biblioteca (D135) son de 16x16 */

static int largura_botao(const char *texto)
{
    return BT_MARGEM_ICON + BT_ICONE_L + BT_GAP_ICON + g_largura_texto(texto) + BT_MARGEM_FIM;
}

/* El ancho de un bot�n del lateral: ocupa el panel entero, pero nunca menos
 * que lo que necesita su texto (el "Abrir con..." es largo). */
static int largura_lado(const char *texto)
{
    int l = largura_botao(texto);
    int max = LATERAL - 18;
    return l > max ? l : max;
}

/* Botón con icono de la biblioteca (D135) y su texto. */
static void desenhar_botao(botao_t *b, const char *icone, const char *texto)
{
    g_retangulo(j, b->x, b->y, b->l, b->a, b->apertado ? COR_BOTAO_APERTADO : COR_BOTAO);
    g_relevo(j, b->x, b->y, b->l, b->a, b->apertado);
    g_moldura(j, b->x - 1, b->y - 1, b->l + 2, b->a + 2, 0x5A6470);
    int tx = b->x + BT_MARGEM_ICON;
    if (icone) { icone_desenhar(j, tx, b->y + (b->a - BT_ICONE_L) / 2, icone, 1); tx += BT_ICONE_L + BT_GAP_ICON; }
    g_texto(j, tx, b->y + (b->a - LETRA_A) / 2, texto, COR_TEXTO);
    jan_mostrar_area(j, b->x - 1, b->y - 1, b->l + 2, b->a + 2);
}

/* Como botao_tratar, pero dibujando el icono. */
static bool botao_tratar_icone(botao_t *b, const char *icone, const char *texto, const evento_t *e)
{
    if (e->tipo == EV_CLIQUE && (e->botoes & 1) && e->x >= b->x && e->x < b->x + b->l &&
        e->y >= b->y && e->y < b->y + b->a) {
        b->apertado = true;
        desenhar_botao(b, icone, texto);
        return false;
    }
    if (e->tipo == EV_SOLTOU && b->apertado) {
        b->apertado = false;
        desenhar_botao(b, icone, texto);
        return e->x >= b->x && e->x < b->x + b->l && e->y >= b->y && e->y < b->y + b->a;
    }
    return false;
}

static void desenhar_barra(void)
{
    g_retangulo(j, 0, 0, LARG, ALT_BARRA, 0xE6E9EC);
    g_retangulo(j, 0, ALT_BARRA - 1, LARG, 1, COR_BORDA);
    int x = 8;
    for (int i = 0; i < 8; i++) {
        bt[i].x = x; bt[i].y = 7; bt[i].l = largura_botao(ROT_BT[i]); bt[i].a = 26; bt[i].texto = ROT_BT[i]; bt[i].apertado = false;
        if (x + bt[i].l > LARG) break;   /* ventana estrecha: no caben todos */
        desenhar_botao(&bt[i], IC_BT[i], ROT_BT[i]);
        x += bt[i].l + 4;
    }
}

/* Los botones del panel lateral: primero los lugares, luego las acciones. */
typedef struct {
    botao_t b;
    int acao;
    const char *icone;
    char texto[24];
} lado_t;

enum { LUG_SISTEMA, LUG_HOME, LUG_TMP, LUG_PEN, LUG_LIXEIRA, LUG_CD,
       ACT_ABRIR_COM, ACT_COPIAR, ACT_CORTAR, ACT_COLAR, ACT_RENOMEAR, ACT_PROPS };
static lado_t lado[MAX_LADO];
static int qtd_lado;

static void desenhar_lateral(void)
{
    g_retangulo(j, 0, ALT_BARRA, LATERAL, ALT - ALT_BARRA - ALT_STATUS, 0xEAEDF0);
    g_retangulo(j, LATERAL - 1, ALT_BARRA, 1, ALT - ALT_BARRA - ALT_STATUS, COR_BORDA);
    qtd_lado = 0;
    int y = ALT_BARRA + 8;
    rotulo(j, 10, y, "LUGARES", COR_TEXTO_FRACO);
    y += 20;
    struct { int acao; const char *icone; const char *texto; } itens[] = {
        { LUG_SISTEMA, "disco", "Sistema" },
        { LUG_HOME, "casa", "home" },
        { LUG_TMP, "pasta", "tmp" },
    };
    for (unsigned k = 0; k < sizeof itens / sizeof itens[0]; k++) {
        if (itens[k].acao == LUG_HOME && !existe_pasta("/home")) continue;
        if (qtd_lado >= MAX_LADO) break;
        lado_t *l = &lado[qtd_lado++];
        l->acao = itens[k].acao;
        l->icone = itens[k].icone;
        copia_texto(l->texto, itens[k].texto, sizeof l->texto);
        l->b.x = 8; l->b.y = y; l->b.l = largura_lado(l->texto); l->b.a = 24; l->b.texto = l->texto; l->b.apertado = false;
        desenhar_botao(&l->b, l->icone, l->texto);
        y += 28;
    }
    /* El pendrive tiene su sitio siempre (y es el mismo botón para montar y para
     * desmontar: así el dedo no tiene que buscarlo). */
    y = ALT_BARRA + 108;
    if (tem_pendrive) {
        if (qtd_lado < MAX_LADO) {
            lado_t *l = &lado[qtd_lado++];
            l->acao = LUG_PEN;
            l->icone = "disco";
            copia_texto(l->texto, pendrive.montado ? "Retirar" : pendrive.rotulo[0] ? pendrive.rotulo : "Pendrive", sizeof l->texto);
            l->b.x = 8; l->b.y = y; l->b.l = largura_lado(l->texto); l->b.a = 24; l->b.texto = l->texto; l->b.apertado = false;
            desenhar_botao(&l->b, l->icone, l->texto);
        }
        y += 28;
    }
    /* D233: el CD/DVD de datos del lector: abre la carpeta donde está montado (lo monta
     * en /cd si no lo está); dentro de ella, el mismo botón lo expulsa. */
    if (tem_cd) {
        if (qtd_lado < MAX_LADO) {
            lado_t *l = &lado[qtd_lado++];
            bool dentro = cdrom.montado && cdrom.ponto[0] && comeca_com(pasta, cdrom.ponto);
            l->acao = LUG_CD;
            l->icone = dentro ? "excluir" : "disco";
            copia_texto(l->texto, dentro ? "Expulsar" : cdrom.rotulo[0] ? cdrom.rotulo : "CD/DVD", sizeof l->texto);
            l->b.x = 8; l->b.y = y; l->b.l = largura_lado(l->texto); l->b.a = 24; l->b.texto = l->texto; l->b.apertado = false;
            desenhar_botao(&l->b, l->icone, l->texto);
        }
        y += 28;
    }
    if (existe_pasta("/home/.papelera") && qtd_lado < MAX_LADO) {
        lado_t *l = &lado[qtd_lado++];
        l->acao = LUG_LIXEIRA;
        l->icone = "excluir";
        copia_texto(l->texto, "Lixeira", sizeof l->texto);
        l->b.x = 8; l->b.y = y; l->b.l = largura_lado(l->texto); l->b.a = 24; l->b.texto = l->texto; l->b.apertado = false;
        desenhar_botao(&l->b, l->icone, l->texto);
        y += 28;
    }
    y += 6;
    rotulo(j, 10, y, "ACCIONES", COR_TEXTO_FRACO);
    y += 20;
    struct { int acao; const char *icone; const char *texto; } acoes[] = {
        { ACT_ABRIR_COM, "abrir", "Abrir con..." },
        { ACT_COPIAR, "copiar", "Copiar" },
        { ACT_CORTAR, "editar", "Cortar" },
        { ACT_COLAR, "colar", "Pegar" },
        { ACT_RENOMEAR, "documento-editar", "Renombrar" },
        { ACT_PROPS, "informacao", "Propiedades" },
    };
    for (unsigned k = 0; k < sizeof acoes / sizeof acoes[0]; k++) {
        if (qtd_lado >= MAX_LADO) break;
        lado_t *l = &lado[qtd_lado++];
        l->acao = acoes[k].acao;
        l->icone = acoes[k].icone;
        copia_texto(l->texto, acoes[k].texto, sizeof l->texto);
        l->b.x = 8; l->b.y = y; l->b.l = largura_lado(l->texto); l->b.a = 24; l->b.texto = l->texto; l->b.apertado = false;
        desenhar_botao(&l->b, l->icone, l->texto);
        y += 26;
    }
    jan_mostrar_area(j, 0, ALT_BARRA, LATERAL, ALT - ALT_BARRA - ALT_STATUS);
}

/* La barra de ruta: las migas de pan, pulsables. */
static void desenhar_rota(void)
{
    int x0 = LATERAL + 6, l = LARG - x0 - 8;
    if (l < 40) l = 40;
    g_retangulo(j, x0, ALT_BARRA + 4, l, ALT_ROTA - 8, COR_BRANCO);
    g_moldura(j, x0, ALT_BARRA + 4, l, ALT_ROTA - 8, COR_BORDA);
    n_migas = 0;
    if (caixa.foco) {   /* se está editando la ruta o buscando */
        caixa.x = x0 + 4; caixa.y = ALT_BARRA + 6; caixa.l = l - 8;
        caixa_desenhar(j, &caixa);
        return;
    }
    int x = x0 + 8;
    if (n_migas < MAX_MIGAS) {
        migas[n_migas].x = x; migas[n_migas].l = 8;
        copia_texto(migas[n_migas].caminho, "/", sizeof migas[0].caminho);
        n_migas++;
    }
    g_texto(j, x, ALT_BARRA + 8, "/", COR_TEXTO);
    x += 14;
    char acc[160];
    acc[0] = 0;
    int ini = 1;
    bool primeiro = true;
    while (ini < (int)tam_texto(pasta)) {
        int fim = ini;
        while (pasta[fim] && pasta[fim] != '/') fim++;
        char parte[64];
        int t = fim - ini;
        if (t > 63) t = 63;
        memcpy(parte, pasta + ini, (size_t)t);
        parte[t] = 0;
        junta_texto(acc, "/", sizeof acc);
        junta_texto(acc, parte, sizeof acc);
        if (!primeiro) g_texto(j, x - 5, ALT_BARRA + 8, ">", COR_TEXTO_FRACO);
        g_texto(j, x, ALT_BARRA + 8, parte, COR_TEXTO);
        primeiro = false;
        if (n_migas < MAX_MIGAS) {
            migas[n_migas].x = x; migas[n_migas].l = t * 8;
            copia_texto(migas[n_migas].caminho, acc, sizeof migas[0].caminho);
            n_migas++;
        }
        x += t * 8 + 6;
        ini = fim + 1;
    }
    jan_mostrar_area(j, x0, ALT_BARRA + 4, l, ALT_ROTA - 8);
}

/* Las columnas de la vista de detalles (encabezado pulsable). */
static int col_tam, col_tipo, col_data;

static void desenhar_cabecalho(void)
{
    int x0 = lista.x, l = lista.l;
    int y = lista.y - 18;
    g_retangulo(j, x0, y, l, 18, 0xE6E9EC);
    g_retangulo(j, x0, y + 17, l, 1, COR_BORDA);
    col_tam = x0 + l - 250;
    col_tipo = x0 + l - 240;
    col_data = x0 + l - 160;
    uint32_t cor = ordem == ORD_NOME ? COR_DESTAQUE : COR_TEXTO;
    g_texto(j, x0 + 6, y + 2, "Nombre", cor);
    const char *seta = ordem_desc ? "v" : "^";
    g_texto(j, x0 + 62, y + 2, seta, COR_DESTAQUE);
    char t[16];
    copia_texto(t, "Tamaño", sizeof t);
    int tl = (int)tam_texto(t);
    cor = ordem == ORD_TAMANHO ? COR_DESTAQUE : COR_TEXTO;
    g_texto(j, col_tam - tl * 8, y + 2, t, cor);
    if (ordem == ORD_TAMANHO) g_texto(j, col_tam + 4, y + 2, seta, COR_DESTAQUE);
    copia_texto(t, "Tipo", sizeof t);
    cor = ordem == ORD_TIPO ? COR_DESTAQUE : COR_TEXTO;
    g_texto(j, col_tipo, y + 2, t, cor);
    if (ordem == ORD_TIPO) g_texto(j, col_tipo + 34, y + 2, seta, COR_DESTAQUE);
    copia_texto(t, "Fecha", sizeof t);
    cor = ordem == ORD_DATA ? COR_DESTAQUE : COR_TEXTO;
    g_texto(j, col_data, y + 2, t, cor);
    if (ordem == ORD_DATA) g_texto(j, col_data + 40, y + 2, seta, COR_DESTAQUE);
}

/* Una fila de la vista de lista (icono, nombre, tamaño, tipo y fecha). */
static void desenhar_linha(int i, int y, bool sel, bool atual)
{
    int x0 = lista.x;
    if (sel) g_retangulo(j, x0 + 2, y, lista.l - 4, LISTA_ALTURA_ITEM, atual ? 0x1E5AA8 : COR_SELECAO);
    else if (atual) g_retangulo(j, x0 + 2, y, lista.l - 4, LISTA_ALTURA_ITEM, 0xE7EEF8);
    uint32_t cor = sel ? COR_BRANCO : COR_TEXTO;
    icone_item(x0 + 6, y + 2, i, 1);
    g_texto_n(j, x0 + 28, y + 2, nomes[i], (lista.l - 34) / LETRA_L, cor);
    char t[32];
    if (tipos[i] == TIPO_PASTA) copia_texto(t, "-", sizeof t);
    else formatar_texto(t, sizeof t, "%u", tamanhos[i]);
    int tl = (int)tam_texto(t);
    g_texto(j, col_tam - tl * 8, y + 2, t, cor);
    g_texto(j, col_tipo, y + 2, tipo_texto(i), cor);
    data_texto(i, t, sizeof t);
    g_texto_n(j, col_data, y + 2, t, (lista.l - (col_data - x0) - 6) / LETRA_L, cor);
}

/* La lista según la vista. */
/* ------------------------------------------------------------ grelha de icones */

/* Desenhos que a grelha usa (estao mais abaixo no ficheiro). */
static void desenhar_lista(void);
static const char *vista_nome(void);
static void desenhar_rodape(const char *msg);
static void info_item(int i, char *msg, size_t max);

/* Dibujo de la lista para lista_tratar (D217): la rueda, el teclado y el clic de la lista
 * comun redibujan con la vista actual (iconos, detalles) y no con el dibujo generico. */
static void desenhar_lista_gancho(jan_t *jj, lista_t *l)
{
    (void)jj; (void)l;
    desenhar_lista();
}

/* A vista de icones e uma grelha: as celulas sao colunas de largura fixa e as
 * linhas de altura fixa. O topo da lista e sempre o primeiro item de uma linha
 * (um multiplo do numero de colunas), e assim a grelha nunca se desloca de lado
 * ao rolar (D139). */
static int grade_colunas(void)
{
    int c = (lista.l - 12) / LARG_CELULA;
    return c < 1 ? 1 : c;
}

static int grade_linhas(void)
{
    int l = (lista.a - 6) / ALT_CELULA;
    return l < 1 ? 1 : l;
}

/* Rola a grelha um numero de linhas inteiras; devolve true se mudou. */
static bool grade_rolar(int linhas)
{
    int col = grade_colunas(), vis = grade_linhas();
    int antes = lista.topo;
    int maximo = lista.qtd - vis * col;
    if (maximo < 0) maximo = 0;
    maximo = (maximo / col) * col;         /* so linhas completas */
    lista.topo += linhas * col;
    if (lista.topo > maximo) lista.topo = maximo;
    if (lista.topo < 0) lista.topo = 0;
    lista.topo = (lista.topo / col) * col;  /* sempre no inicio de uma linha */
    if (lista.topo == antes) return false;
    desenhar_lista();
    return true;
}

/* Mantem a celula selecionada dentro da janela da grelha. */
static void grade_garantir_visivel(int col, int vis)
{
    int sel = lista.sel;
    if (sel < lista.topo) { lista.topo = (sel / col) * col; return; }
    if (sel >= lista.topo + vis * col) lista.topo = ((sel - (vis - 1) * col) / col) * col;
}

/* As setas na grelha: a esquerda e a direita mudam de celula, a de cima e a de
 * baixo saltam uma linha inteira. */
static bool grade_tecla(unsigned t)
{
    int col = grade_colunas(), vis = grade_linhas();
    int sel = lista.sel, antes = sel;
    if (t == TECLA_BAIXO) sel += col;
    else if (t == TECLA_CIMA) sel -= col;
    else if (t == TECLA_DIREITA) sel++;
    else if (t == TECLA_ESQUERDA) sel--;
    else if (t == TECLA_HOME) sel = 0;
    else if (t == TECLA_END) sel = lista.qtd - 1;
    else if (t == TECLA_PGDN) sel += col * vis;
    else if (t == TECLA_PGUP) sel -= col * vis;
    else return false;
    if (sel < 0) sel = 0;
    if (sel >= lista.qtd) sel = lista.qtd - 1;
    if (sel == antes) return false;
    lista.sel = sel;
    grade_garantir_visivel(col, vis);
    limpar_marcados();
    marcados[sel] = true;
    n_marcados = 1;
    ancora = sel;
    desenhar_lista();
    char msg[200];
    info_item(sel, msg, sizeof msg);
    desenhar_rodape(msg);
    return true;
}

static void desenhar_lista(void)
{
    static int ult_topo = -1, ult_vista = -1;
    if (lista.qtd > 0) {   /* topo siempre dentro de los limites (redimensionar o filtrar lo pueden dejar fuera) */
        if (lista.topo >= lista.qtd) lista.topo = lista.qtd - 1;
        if (lista.topo < 0) lista.topo = 0;
    } else lista.topo = 0;
    /* En la rejilla, topo siempre al inicio de una fila: AvPag, redimensionar o la lista
     * comun lo pueden dejar a media fila y la rejilla se correria de lado (D139, D217). */
    if (vista == VISTA_ICONES) {
        int c = grade_colunas();
        if (c > 0) lista.topo = (lista.topo / c) * c;
    }
    if (lista.topo != ult_topo || vista != ult_vista) {
        registrar("arquivos: lista desenhada vista=%s topo=%d", vista_nome(), lista.topo);
        ult_topo = lista.topo; ult_vista = vista;
    }
    int x0 = lista.x, l = lista.l, alt = lista.a;
    int topo = vista == VISTA_DETALHES ? lista.y - 18 : lista.y;   /* el encabezado va arriba */
    g_retangulo(j, x0, lista.y, l, alt, COR_BRANCO);
    g_moldura(j, x0, lista.y, l, alt, COR_BORDA);
    if (vista == VISTA_DETALHES) desenhar_cabecalho();
    else { col_tam = x0 + l - 250; col_tipo = x0 + l - 240; col_data = x0 + l - 160; }
    int vis = (alt - 4) / LISTA_ALTURA_ITEM;
    int grade_col = grade_colunas(), grade_lin = grade_linhas();
    if (vista == VISTA_ICONES) {
        for (int k = 0; k < grade_lin && lista.topo + k * grade_col < lista.qtd; k++)
            for (int c = 0; c < grade_col; c++) {
                int i = lista.topo + k * grade_col + c;
                if (i >= lista.qtd) break;
                int cx = x0 + 6 + c * LARG_CELULA, cy = lista.y + 4 + k * ALT_CELULA;
                bool sel = marcados[i];
                if (sel) g_retangulo(j, cx, cy, 88, 68, COR_SELECAO);
                else if (i == lista.sel) g_retangulo(j, cx, cy, 88, 68, 0xE7EEF8);
                icone_item(cx + 28, cy + 4, i, 2);
                g_texto_n(j, cx + 2, cy + 46, nomes[i], (88 - 4) / LETRA_L, sel ? COR_BRANCO : COR_TEXTO);
            }
    } else {
        for (int k = 0; k < vis && lista.topo + k < lista.qtd; k++) {
            int i = lista.topo + k;
            desenhar_linha(i, lista.y + 2 + k * LISTA_ALTURA_ITEM, marcados[i], i == lista.sel);
        }
    }
    /* A barra de deslocamento conta as unidades visiveis: itens nas vistas de
     * lista e de detalhes, linhas da grelha na vista de icones (D139). */
    int un_vis, un_total, un_topo;
    if (vista == VISTA_ICONES) {
        un_vis = grade_lin;
        un_total = (lista.qtd + grade_col - 1) / grade_col;
        un_topo = lista.topo / grade_col;
    } else {
        un_vis = vis;
        un_total = lista.qtd;
        un_topo = lista.topo;
    }
    int un_max = un_total - un_vis;
    if (un_max > 0) {
        int trilho = alt - 4;
        int pol_a = trilho * un_vis / un_total;
        if (pol_a < 8) pol_a = 8;
        int pol_y = (trilho - pol_a) * un_topo / un_max;
        g_retangulo(j, x0 + l - 6, lista.y + 2 + pol_y, 4, pol_a, 0x9AA3AC);
    }
    jan_mostrar_area(j, x0, topo, l, lista.y + alt - topo);
}

/* La barra de estado: elementos, selección y espacio libre. */
static void desenhar_rodape(const char *msg)
{
    if (msg != rodape) copia_texto(rodape, msg, sizeof rodape);
    int y = ALT - ALT_STATUS;
    g_retangulo(j, 0, y, LARG, ALT_STATUS, 0xE6E9EC);
    g_retangulo(j, 0, y, LARG, 1, COR_BORDA);
    g_texto_n(j, 10, y + 4, msg, (LARG - 30) / LETRA_L, COR_TEXTO_FRACO);
    char t[64];
    espaco_livre(t, sizeof t);
    int lw = g_largura_texto(t);
    if (lw < LARG - 20) g_texto(j, LARG - 10 - lw, y + 4, t, COR_TEXTO_FRACO);
    jan_mostrar_area(j, 0, y, LARG, ALT_STATUS);
}

/* Lo que dice la barra de estado del elemento elegido. */
static void info_item(int i, char *msg, size_t max)
{
    if (em_busca) { copia_texto(msg, rotas[i], max); return; }
    if (tipos[i] == TIPO_PASTA) formatar_texto(msg, max, "%s/ (carpeta)", nomes[i]);
    else formatar_texto(msg, max, "%s: %u bytes%s", nomes[i], tamanhos[i], tipos[i] == TIPO_PROGRAMA ? " (programa)" : "");
}

/* El texto del estado: cuántos elementos hay y qué está seleccionado. */
static void estado_texto(char *s, size_t max){
    if (em_busca) formatar_texto(s, max, "%d resultados para %s", n_resultados, padrao_busca);
    else if (filtro[0]) formatar_texto(s, max, "%d elementos (filtro %s)", lista.qtd, filtro);
    else if (lista.qtd == 1) formatar_texto(s, max, "1 elemento");
    else formatar_texto(s, max, "%d elementos", lista.qtd);
    if (n_marcados > 1) {
        char t[64];
        formatar_texto(t, sizeof t, "%d seleccionados", n_marcados);
        if (tam_texto(s) + 2 < max) { junta_texto(s, " - ", max); junta_texto(s, t, max); }
    }
}

static void desenhar_tudo(void)
{
    desenhar_barra();
    desenhar_lateral();
    desenhar_rota();
    desenhar_lista();
    char msg[200];
    estado_texto(msg, sizeof msg);
    desenhar_rodape(msg);
}

/* ------------------------------------------------------------ cargar */

/* Lee la carpeta y deja la lista lista para dibujar. */
static void carregar(void)
{
    int n = 0;
    if (!em_busca) {
        entrada_pasta_t e;
        for (int passo = 0; passo < 2; passo++)
            for (int i = 0; n < MAX_ITENS && listar(pasta, i, &e) == 1; i++) {
                bool eh_pasta = e.tipo == TIPO_PASTA;
                if ((passo == 0) != eh_pasta) continue;      /* las carpetas primero */
                if (eh_oculto(e.nome) && !ver_ocultos) continue;
                if (!casa_filtro(filtro, e.nome)) continue;
                copia_texto(nomes[n], e.nome, sizeof nomes[0]);
                tipos[n] = (uint8_t)e.tipo;
                tamanhos[n] = e.tamanho;
                modificadas[n] = 0;
                tem_data[n] = false;
                rotas[n][0] = 0;
                n++;
            }
        ordenar(n);
    } else {
        n = n_resultados;
    }
    lista.qtd = n;
    lista.sel = n ? 0 : -1;
    lista.topo = 0;
    limpar_marcados();
    desenhar_tudo();
    registrar("arquivos: pasta %s (%d itens)", pasta, n);
}

/* ------------------------------------------------------------ navegación */

static void entrar(const char *nova, bool guardar_historia)
{
    info_arquivo_t inf;
    if (!nova || !nova[0]) return;
    if (info_arquivo(nova, &inf) < 0 || inf.tipo != TIPO_PASTA) return;
    copia_texto(pasta, nova, sizeof pasta);
    em_busca = false;
    filtro[0] = 0;
    padrao_busca[0] = 0;
    if (guardar_historia) {
        if (hist_pos < 0 || compara_texto(hist[hist_pos], pasta) != 0) {
            hist_pos++;
            if (hist_pos >= MAX_HIST) {
                memmove(hist[0], hist[1], sizeof hist[0] * (MAX_HIST - 1));
                hist_pos = MAX_HIST - 1;
            }
            copia_texto(hist[hist_pos], pasta, sizeof hist[0]);
            hist_n = hist_pos + 1;
        }
    }
    carregar();
}

static void subir(void);

static void voltar(void)
{
    if (hist_pos <= 0) { subir(); return; }   /* sin historial: sube, como antes */
    hist_pos--;
    char v[160];
    copia_texto(v, hist[hist_pos], sizeof v);
    entrar(v, false);
}

static void avancar(void)
{
    if (hist_pos < 0 || hist_pos + 1 >= hist_n) return;
    hist_pos++;
    char v[160];
    copia_texto(v, hist[hist_pos], sizeof v);
    entrar(v, false);
}

static void subir(void)
{
    if (compara_texto(pasta, "/") == 0) return;
    char v[160];
    char *b = acha_ultimo_char(pasta, '/');
    if (b == pasta) copia_texto(v, "/", sizeof v);
    else { copia_texto(v, pasta, sizeof v); *acha_ultimo_char(v, '/') = 0; }
    entrar(v, true);
}

/* ------------------------------------------------------------ acciones */

static bool rom_chip8(const char *c)
{
    size_t n = tam_texto(c);
    if (n < 4 || c[n - 4] != '.') return false;
    char e[4] = { (char)(c[n - 3] | 0x20), (char)(c[n - 2] | 0x20), (char)(c[n - 1] | 0x20), 0 };
    return compara_texto(e, "ch8") == 0 || compara_texto(e, "sc8") == 0 || compara_texto(e, "xo8") == 0;
}

/* .pdf: el visor de PDF propio (mejora 059, D138). */
static bool pdf_extensao(const char *c)
{
    size_t n = tam_texto(c);
    if (n < 4) return false;
    char e[5];
    for (int k = 0; k < 4; k++) e[k] = (char)(c[n - 4 + k] | 0x20);
    e[4] = 0;
    return compara_texto(e, ".pdf") == 0;
}

/* .avi .mpg .mpeg: el reproductor de video (mejora 048). */
static bool video_extensao(const char *c)
{
    size_t n = tam_texto(c);
    if (n < 4) return false;
    char e[6];
    for (int k = 0; k < 4; k++) e[k] = (char)(c[n - 4 + k] | 0x20);
    e[4] = 0;
    if (compara_texto(e, ".avi") == 0) return true;
    if (compara_texto(e, ".mpg") == 0) return true;
    /* .mpeg: a extensao tem cinco letras */
    if (n >= 5) {
        for (int k = 0; k < 5; k++) e[k] = (char)(c[n - 5 + k] | 0x20);
        e[5] = 0;
        if (compara_texto(e, ".mpeg") == 0) return true;
    }
    return false;
}

/* .jclic y .jclic.zip: el visor de actividades JClic (D183) */
static bool jclic_extensao(const char *c)
{
    unsigned n = tam_texto(c);
    if (n >= 6 && compara_texto_n(c + n - 6, ".jclic", 6) == 0) return true;
    if (n >= 10) {
        char e[11];
        for (unsigned k = 0; k < 10; k++) e[k] = (char)(c[n - 10 + k] | 0x20);
        e[10] = 0;
        return compara_texto(e, ".jclic.zip") == 0;
    }
    return false;
}

/* .wav .mp3 .flac .ogg: se abren con el reproductor de música (mejora 053, D115) */
static bool audio_extensao(const char *c)
{
    unsigned n = tam_texto(c), p = n;
    while (p && c[p - 1] != '.' && c[p - 1] != '/') p--;
    if (!p || c[p - 1] != '.') return false;
    char e[5] = { 0 };
    for (unsigned k = 0; k < 4 && p + k < n; k++) e[k] = (char)(c[p + k] | 0x20);
    return compara_texto(e, "wav") == 0 || compara_texto(e, "mp3") == 0 || compara_texto(e, "flac") == 0 ||
           compara_texto(e, "ogg") == 0;
}

/* .mid .midi .kar .rmi: el reproductor de MIDI (D190) */
static bool midi_extensao(const char *c)
{
    unsigned n = tam_texto(c), p = n;
    while (p && c[p - 1] != '.' && c[p - 1] != '/') p--;
    if (!p || c[p - 1] != '.') return false;
    char e[5] = { 0 };
    for (unsigned k = 0; k < 4 && p + k < n; k++) e[k] = (char)(c[p + k] | 0x20);
    return compara_texto(e, "mid") == 0 || compara_texto(e, "midi") == 0 || compara_texto(e, "kar") == 0 || compara_texto(e, "rmi") == 0;
}

#ifndef SAGUI_SEM_GPL
/* .gb .gbc: ROM de Game Boy, el emulador GNUBoy de /apps (D235; GPL: fuera de la variante sin GPL) */
static bool gb_extensao(const char *c)
{
    unsigned n = tam_texto(c);
    if (n >= 3 && c[n - 3] == '.' && (c[n - 2] | 0x20) == 'g' && (c[n - 1] | 0x20) == 'b') return true;
    return n >= 4 && c[n - 4] == '.' && (c[n - 3] | 0x20) == 'g' && (c[n - 2] | 0x20) == 'b' && (c[n - 1] | 0x20) == 'c';
}
#endif

#ifndef SAGUI_SEM_GPL
/* .exe .com .bat: programas de DOS, con DOSBox de /apps (D294; GPL: fuera de la variante sin GPL) */
static bool dos_extensao(const char *c)
{
    unsigned n = tam_texto(c);
    if (n < 5 || c[n - 4] != '.') return false;
    char e[4] = { (char)(c[n - 3] | 0x20), (char)(c[n - 2] | 0x20), (char)(c[n - 1] | 0x20), 0 };
    return compara_texto(e, "exe") == 0 || compara_texto(e, "com") == 0 || compara_texto(e, "bat") == 0;
}
#endif

static void abrir_item(int i)
{
    if (i < 0 || i >= lista.qtd) return;
    char c[200];
    caminho_de(i, c, sizeof c);
    if (tipos[i] == TIPO_PASTA) { entrar(c, true); return; }
    char *argv[3];
    int pid;
    if (tipos[i] == TIPO_PROGRAMA) {
        argv[0] = nomes[i]; argv[1] = NULL;
        pid = executar(c, argv);
        registrar("arquivos: executando %s (pid %d)", c, pid);
    } else {
        bool md = tam_texto(c) > 3 && compara_texto(c + tam_texto(c) - 3, ".md") == 0;   /* .md: visor de Markdown (D045) */
        const char *prog = imagem_extensao(c) ? "visor" : md ? "markdown" : "editor";   /* imágenes: Visor (D055) */
        if (rom_chip8(c)) prog = "chip8";   /* .ch8 .sc8 .xo8: el emulador (D070) */
        if (audio_extensao(c)) prog = "musica";   /* .wav .mp3 .flac .ogg: el Reproductor de música (D115) */
        if (midi_extensao(c)) prog = "midi";   /* .mid .midi .kar .rmi: el reproductor de MIDI (D190) */
        if (jclic_extensao(c)) prog = "jclic";   /* .jclic .jclic.zip: Actividades JClic (D183) */
        if (pdf_extensao(c)) prog = "visorpdf";   /* .pdf: el Visor de PDF (D138) */
        if (video_extensao(c)) prog = "reproductor";   /* .avi .mpg: el Reproductor de vídeo (D129) */
        char bin[24];
        formatar_texto(bin, sizeof bin, "/bin/%s", prog);
#ifndef SAGUI_SEM_GPL
        if (gb_extensao(c)) { prog = "gnuboy"; formatar_texto(bin, sizeof bin, "/apps/bin/gnuboy"); }   /* .gb .gbc: GNUBoy (D235) */
        if (dos_extensao(c)) { prog = "dosbox"; formatar_texto(bin, sizeof bin, "/apps/bin/dosbox"); }   /* .exe .com .bat: DOSBox (D294) */
#endif
        argv[0] = (char *)prog; argv[1] = c; argv[2] = NULL;
        pid = executar(bin, argv);
        registrar("arquivos: abrindo no %s %s (pid %d)", prog, c, pid);
    }
    char msg[200];
    formatar_texto(msg, sizeof msg, pid > 0 ? "Abriendo %s..." : "No se ha podido abrir %s", nomes[i]);
    desenhar_rodape(msg);
}

static void nova_pasta(void)
{
    char nome[64] = "";
    if (!pedir_texto("Nueva carpeta", "Nombre de la nueva carpeta:", nome, sizeof nome) || !nome[0]) return;
    char c[200];
    caminho_juntar(c, sizeof c, pasta, nome);
    int r = criar_pasta(c);
    if (r < 0) { char m[200]; formatar_texto(m, sizeof m, "No se ha podido crear la carpeta:\n%s", nome_erro(r)); mensagem("Archivos", m); return; }
    registrar("arquivos: pasta criada %s", c);
    carregar();
}

static void apagar_item(void)
{
    if (n_marcados <= 0) {
        desenhar_rodape("No hay nada seleccionado");
        return;
    }
    char pergunta[200];
    if (n_marcados == 1) {
        int i = lista.sel;
        if (i < 0 || i >= lista.qtd) return;
        char c[200];
        caminho_de(i, c, sizeof c);
        formatar_texto(pergunta, sizeof pergunta, "¿Seguro que quieres borrar\n%s%s?", nomes[i], tipos[i] == TIPO_PASTA ? "/" : "");
        if (!confirmar("Borrar", pergunta)) return;
        int r = tipos[i] == TIPO_PASTA ? remover_pasta(c) : apagar(c);
        if (r < 0) { char m[200]; formatar_texto(m, sizeof m, "No se ha podido borrar:\n%s", nome_erro(r)); mensagem("Archivos", m); return; }
        registrar("arquivos: apagado %s", c);
    } else {
        formatar_texto(pergunta, sizeof pergunta, "¿Seguro que quieres borrar\n%d elementos?", n_marcados);
        if (!confirmar("Borrar", pergunta)) return;
        for (int i = 0; i < lista.qtd; i++) {
            if (!marcados[i]) continue;
            char c[200];
            caminho_de(i, c, sizeof c);
            int r = tipos[i] == TIPO_PASTA ? remover_pasta(c) : apagar(c);
            if (r < 0) continue;
            registrar("arquivos: apagado %s", c);
        }
    }
    em_busca = false;          /* al borrar se sale de la búsqueda */
    carregar();
}

static void renomear_item(void)
{
    int i = lista.sel;
    if (i < 0 || i >= lista.qtd) return;
    char nome[64], antigo[200], novo[200];
    copia_texto(nome, nomes[i], sizeof nome);
    if (!pedir_texto("Renombrar", "Nuevo nombre:", nome, sizeof nome) || !nome[0]) return;
    if (compara_texto(nome, nomes[i]) == 0) return;
    if (acha_char(nome, '/')) { mensagem("Archivos", "El nombre no puede contener '/'."); return; }
    caminho_de(i, antigo, sizeof antigo);
    caminho_juntar(novo, sizeof novo, pasta, nome);
    int r = renomear(antigo, novo);
    if (r < 0) { char m[200]; formatar_texto(m, sizeof m, "No se ha podido renombrar:\n%s", nome_erro(r)); mensagem("Archivos", m); return; }
    registrar("arquivos: renomeado %s -> %s", antigo, novo);
    em_busca = false;          /* al renombrar se sale de la búsqueda */
    carregar();
    for (int k = 0; k < lista.qtd; k++)   /* el elemento renombrado sigue seleccionado */
        if (compara_texto(nomes[k], nome) == 0) { lista.sel = k; break; }
}

/* El tamaño de una carpeta (suma de lo que hay dentro, hasta tres niveles). */
static uint32_t tamanho_pasta(const char *dir, int nivel)
{
    if (nivel > 3) return 0;
    entrada_pasta_t e;
    uint32_t total = 0;
    for (int i = 0; listar(dir, i, &e) == 1; i++) {
        if (e.tipo == TIPO_PASTA) {
            char sub[200];
            caminho_juntar(sub, sizeof sub, dir, e.nome);
            total += tamanho_pasta(sub, nivel + 1);
        } else total += e.tamanho;
    }
    return total;
}

static void propriedades_item(void)
{
    int i = lista.sel;
    if (i < 0 || i >= lista.qtd) return;
    char c[200], m[600];
    caminho_de(i, c, sizeof c);
    const char *tipo = tipo_texto(i);
    char quando[64] = "desconocida", criado[64] = "desconocida";
    datas_arquivo_t d;
    if (datas_arquivo(c, &d) == 0) {
        fuso_data_t l;
        if (d.modificado) { hora_local(d.modificado, &l); formatar_data(quando, sizeof quando, "%d/%m/%Y %H:%M:%S %Z", &l); }
        if (d.criado) { hora_local(d.criado, &l); formatar_data(criado, sizeof criado, "%d/%m/%Y %H:%M:%S %Z", &l); }
    }
    uint32_t tam = tamanhos[i];
    if (tipos[i] == TIPO_PASTA) tam = tamanho_pasta(c, 0);
    char tamtxt[24];
    formatar_texto(tamtxt, sizeof tamtxt, "%u bytes", tam);
    char livre[64];
    espaco_livre(livre, sizeof livre);
    formatar_texto(m, sizeof m, "Nombre: %s\nTipo: %s\nTamaño: %s\nCreado: %s\nModificado: %s\nAtributos: -\nRuta: %s\nEspacio libre: %s",
                    nomes[i], tipo, tamtxt, criado, quando, c, livre);
    registrar("arquivos: propriedades de %s (modificado %s)", c, quando);
    mensagem("Propiedades", m);
}

/* ---- comprimir e extrair (mejora 056, D103): chama os comandos zip, tar e unzip na pasta atual */
static bool termina_em(const char *n, const char *suf)
{
    size_t a = tam_texto(n), b = tam_texto(suf);
    return a > b && compara_texto(n + a - b, suf) == 0;
}

static bool eh_tar_gz(const char *n) { return termina_em(n, ".tar.gz") || termina_em(n, ".tgz") || termina_em(n, ".tar"); }
static bool eh_comprimido(const char *n) { return termina_em(n, ".zip") || termina_em(n, ".gz") || eh_tar_gz(n); }

/* Roda o comando com a pasta atual do Archivos como pasta de trabalho; 0 = deu certo. */
static int rodar(char *const argv[])
{
    char bin[40];
    formatar_texto(bin, sizeof bin, "/bin/%s", argv[0]);
    mudar_pasta(pasta);
    int pid = executar(bin, argv), codigo = 1;
    if (pid <= 0) return 1;
    esperar(pid, &codigo);
    return codigo;
}

static void comprimir_item(bool tgz)
{
    int i = lista.sel;
    if (i < 0 || i >= lista.qtd) return;
    char saida[100], msg[200];
    formatar_texto(saida, sizeof saida, "%s%s", nomes[i], tgz ? ".tar.gz" : ".zip");
    info_arquivo_t inf;
    char c[200];
    caminho_juntar(c, sizeof c, pasta, saida);
    if (info_arquivo(c, &inf) == 0) {
        formatar_texto(msg, sizeof msg, "%s ya existe.\n¿Reemplazarlo?", saida);
        if (!confirmar("Comprimir", msg)) return;
    }
    desenhar_rodape("Comprimiendo...");
    char *a_zip[] = { "zip", "-rq", saida, nomes[i], NULL };
    char *a_tgz[] = { "tar", "-czf", saida, nomes[i], NULL };
    int r = rodar(tgz ? a_tgz : a_zip);
    registrar("arquivos: comprimir %s -> %s (codigo %d)", nomes[i], saida, r);
    carregar();
    formatar_texto(msg, sizeof msg, r == 0 ? "Creado %s" : "No se ha podido crear %s", saida);
    desenhar_rodape(msg);
}

/* ---- accoes de grupo (D139): com varios elementos marcados ---- */

/* Abre todos os elementos marcados: cada um no seu programa (as pastas, numa
 * janela nova do explorador). */
static void abrir_grupo(void)
{
    if (n_marcados <= 0) { desenhar_rodape("No hay nada seleccionado"); return; }
    int abertos = 0;
    for (int i = 0; i < lista.qtd; i++) {
        if (!marcados[i]) continue;
        char c[200];
        caminho_de(i, c, sizeof c);
        if (tipos[i] == TIPO_PASTA) {
            char *argv[3];
            argv[0] = (char *)"arquivos"; argv[1] = c; argv[2] = NULL;
            if (executar("/bin/arquivos", argv) > 0) abertos++;
        } else {
            abrir_item(i);
            abertos++;
        }
    }
    char msg[120];
    formatar_texto(msg, sizeof msg, "%d elementos abiertos", abertos);
    desenhar_rodape(msg);
    registrar("arquivos: abertos %d itens do grupo", abertos);
}

/* As propriedades de todo o grupo: quantos sao, o tamanho total e os nomes. */
static void propriedades_grupo(void)
{
    int n = 0;
    uint32_t total = 0;
    char lista_nomes[400];
    lista_nomes[0] = 0;
    for (int i = 0; i < lista.qtd; i++) {
        if (!marcados[i]) continue;
        char c[200];
        caminho_de(i, c, sizeof c);
        uint32_t tam = tamanhos[i];
        if (tipos[i] == TIPO_PASTA) tam = tamanho_pasta(c, 0);
        total += tam;
        char t[64];
        formatar_texto(t, sizeof t, "%s%s\n", nomes[i], tipos[i] == TIPO_PASTA ? "/" : "");
        if (tam_texto(lista_nomes) + tam_texto(t) + 2 < sizeof lista_nomes)
            junta_texto(lista_nomes, t, sizeof lista_nomes);
        n++;
    }
    char livre[64];
    espaco_livre(livre, sizeof livre);
    char m[700];
    formatar_texto(m, sizeof m, "Elementos: %d\nTama\xf1o total: %u bytes\nEspacio libre: %s\n\n%s", n, total, livre, lista_nomes);
    registrar("arquivos: propriedades do grupo (%d itens)", n);
    mensagem("Propiedades", m);
}

/* Comprime todos os elementos marcados num unico .zip ou .tar.gz. */
static void comprimir_grupo(bool tgz)
{
    if (n_marcados <= 0) { desenhar_rodape("No hay nada seleccionado"); return; }
    char base[64];
    if (compara_texto(pasta, "/") == 0) copia_texto(base, "seleccion", sizeof base);
    else copia_texto(base, nome_base(pasta), sizeof base);
    char saida[100], msg[200];
    formatar_texto(saida, sizeof saida, "%s%s", base, tgz ? ".tar.gz" : ".zip");
    info_arquivo_t inf;
    char c[200];
    caminho_juntar(c, sizeof c, pasta, saida);
    if (info_arquivo(c, &inf) == 0) {
        formatar_texto(msg, sizeof msg, "%s ya existe.\n\xbfReemplazarlo?", saida);
        if (!confirmar("Comprimir", msg)) return;
    }
    desenhar_rodape("Comprimiendo...");
    /* O argv: o programa, a opcao, o ficheiro de saida e os nomes dos
     * elementos marcados. O tar so aceita 64 rotas, por isso o grupo se
     * recorta (e avisa). */
    static char *argv[MAX_ARGS_GRUPO + 4];
    int n = 0, quantos = 0, skipped = 0;
    argv[n++] = tgz ? (char *)"tar" : (char *)"zip";
    argv[n++] = tgz ? (char *)"-czf" : (char *)"-rq";
    argv[n++] = saida;
    for (int i = 0; i < lista.qtd; i++) {
        if (!marcados[i]) continue;
        if (n >= MAX_ARGS_GRUPO) { skipped++; continue; }
        argv[n++] = nomes[i];
        quantos++;
    }
    argv[n] = NULL;
    int r = rodar(argv);
    registrar("arquivos: comprimir %d itens -> %s (codigo %d)", quantos, saida, r);
    if (skipped) registrar("arquivos: %d elementos fora do arquivo (limite de %d)", skipped, MAX_ARGS_GRUPO);
    carregar();
    if (r == 0 && skipped)
        formatar_texto(msg, sizeof msg, "Creado %s (%d de %d elementos)", saida, quantos, quantos + skipped);
    else
        formatar_texto(msg, sizeof msg, r == 0 ? "Creado %s" : "No se ha podido crear %s", saida);
    desenhar_rodape(msg);
}

static void extrair_item(bool em_pasta)
{
    int i = lista.sel;
    if (i < 0 || i >= lista.qtd || !eh_comprimido(nomes[i])) return;
    char destino[64] = ".", msg[200];
    if (em_pasta) {
        copia_texto(destino, nomes[i], sizeof destino);
        char *p = acha_char(destino, '.');
        if (p && p != destino) *p = 0;   /* "datos.tar.gz" -> "datos" */
        if (!pedir_texto("Extraer en carpeta", "Carpeta de destino:", destino, sizeof destino) || !destino[0]) return;
    }
    desenhar_rodape("Extrayendo...");
    int r;
    if (termina_em(nomes[i], ".zip")) {
        char *a[] = { "unzip", "-qo", "-d", destino, nomes[i], NULL };
        r = rodar(a);
    } else if (eh_tar_gz(nomes[i])) {
        char *a[] = { "tar", "-xf", nomes[i], "-C", destino, NULL };
        r = rodar(a);
    } else {
        char *a[] = { "gunzip", "-k", nomes[i], NULL };
        r = rodar(a);
    }
    registrar("arquivos: extrair %s em %s (codigo %d)", nomes[i], destino, r);
    carregar();
    formatar_texto(msg, sizeof msg, r == 0 ? "Extraído %s" : "No se ha podido extraer %s (¿ya existía algo?)", nomes[i]);
    desenhar_rodape(msg);
}

/* ------------------------------------------------------------ copiar y pegar */

/* Copia un archivo o una carpeta entera (recursiva). */
static bool copiar_ficheiro(const char *origem, const char *destino)
{
    info_arquivo_t inf;
    if (info_arquivo(origem, &inf) < 0) return false;
    if (inf.tipo == TIPO_PASTA) {
        if (criar_pasta(destino) < 0 && !existe_pasta(destino)) return false;
        entrada_pasta_t e;
        for (int i = 0; listar(origem, i, &e) == 1; i++) {
            char sub[200], dst[200];
            caminho_juntar(sub, sizeof sub, origem, e.nome);
            caminho_juntar(dst, sizeof dst, destino, e.nome);
            if (!copiar_ficheiro(sub, dst)) return false;
        }
        return true;
    }
    int fo = abrir(origem, ABRIR_LER);
    if (fo < 0) return false;
    int fd = abrir(destino, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { fechar(fo); return false; }
    char buf[2048];
    int n;
    while ((n = ler(fo, buf, sizeof buf)) > 0)
        if (escrever_tudo(fd, buf, (unsigned)n) < 0) { fechar(fo); fechar(fd); return false; }
    fechar(fo);
    fechar(fd);
    return true;
}

/* Copia (Ctrl+C) o corta (Ctrl+X) los elementos marcados. */
static void copiar_area(bool mover)
{
    if (n_marcados <= 0) {
        desenhar_rodape("No hay nada seleccionado");
        return;
    }
    area_n = 0;
    for (int i = 0; i < lista.qtd && area_n < MAX_AREA; i++) {
        if (!marcados[i]) continue;
        char c[200];
        caminho_de(i, c, sizeof c);
        copia_texto(area_caminhos[area_n++], c, sizeof area_caminhos[0]);
    }
    area_mover = mover;
    guardar_area();
    char msg[120];
    formatar_texto(msg, sizeof msg, "%d %s para el portapapeles", area_n, mover ? "cortados" : "copiados");
    desenhar_rodape(msg);
    registrar("arquivos: %s %d itens para a area de transferencia", mover ? "cortados" : "copiados", area_n);
}

/* Pega (Ctrl+V) el portapapeles en la carpeta actual. */
static void colar_area(void)
{
    if (area_n <= 0) {
        desenhar_rodape("El portapapeles de archivos está vacío");
        return;
    }
    bool mover = area_mover;
    int copiados = 0;
    for (int i = 0; i < area_n; i++) {
        char nome[80], destino[200];
        copia_texto(nome, nome_base(area_caminhos[i]), sizeof nome);
        caminho_juntar(destino, sizeof destino, pasta, nome);
        info_arquivo_t inf;
        if (info_arquivo(destino, &inf) == 0) {
            char m[200];
            formatar_texto(m, sizeof m, "%s ya existe.\n¿Reemplazarlo?", nome);
            if (!confirmar("Pegar", m)) continue;
        }
        desenhar_rodape("Copiando...");
        if (!copiar_ficheiro(area_caminhos[i], destino)) continue;
        copiados++;
        if (mover) {   /* el corte borra el original */
            if (info_arquivo(area_caminhos[i], &inf) == 0) {
                if (inf.tipo == TIPO_PASTA) remover_pasta(area_caminhos[i]);
                else apagar(area_caminhos[i]);
            }
        }
    }
    if (mover) { area_n = 0; area_mover = false; guardar_area(); }
    char msg[120];
    formatar_texto(msg, sizeof msg, "%d %s pegados", copiados, mover ? "movidos" : "copiados");
    desenhar_rodape(msg);
    registrar("arquivos: colados %d itens", copiados);
    carregar();
}

/* ------------------------------------------------------------ búsqueda */

/* Busca con comodines en la carpeta y sus subcarpetas. */
static int acrescentar_resultado(const char *dir, const char *nome, uint8_t tipo, uint32_t tam)
{
    if (n_resultados >= MAX_ITENS) return 0;
    int i = n_resultados++;
    copia_texto(nomes[i], nome, sizeof nomes[0]);
    tipos[i] = tipo;
    tamanhos[i] = tam;
    modificadas[i] = 0;
    tem_data[i] = false;
    caminho_juntar(rotas[i], sizeof rotas[0], dir, nome);
    return 1;
}

static void buscar(const char *patrao)
{
    if (!patrao || !patrao[0]) return;
    copia_texto(padrao_busca, patrao, sizeof padrao_busca);
    n_resultados = 0;
    entrada_pasta_t e;
    char sub[200];
    copia_texto(sub, pasta, sizeof sub);
    for (int i = 0; n_resultados < MAX_ITENS && listar(sub, i, &e) == 1; i++) {
        if (casar_glob(padrao_busca, e.nome)) acrescentar_resultado(sub, e.nome, (uint8_t)e.tipo, e.tamanho);
    }
    /* y en las subcarpetas (hasta dos niveles) */
    for (int i = 0; n_resultados < MAX_ITENS && listar(sub, i, &e) == 1; i++) {
        if (e.tipo != TIPO_PASTA) continue;
        char pasta2[200];
        caminho_juntar(pasta2, sizeof pasta2, sub, e.nome);
        for (int k = 0; n_resultados < MAX_ITENS && listar(pasta2, k, &e) == 1; k++) {
            if (casar_glob(padrao_busca, e.nome)) acrescentar_resultado(pasta2, e.nome, (uint8_t)e.tipo, e.tamanho);
        }
    }
    em_busca = true;
    carregar();
    char msg[160];
    formatar_texto(msg, sizeof msg, "%d resultados para %s", n_resultados, padrao_busca);
    desenhar_rodape(msg);
    registrar("arquivos: busca %s (%d resultados)", padrao_busca, n_resultados);
}

/* ------------------------------------------------------------ vistas */

static const char *vista_nome(void)
{
    return vista == VISTA_DETALHES ? "detalhes" : vista == VISTA_ICONES ? "icones" : "lista";
}

static void mudar_vista(void)
{
    vista = vista == VISTA_LISTA ? VISTA_DETALHES : vista == VISTA_DETALHES ? VISTA_ICONES : VISTA_LISTA;
    lista.y = vista == VISTA_DETALHES ? Y_LISTA + 18 : Y_LISTA;
    lista.a = ALT - lista.y - 30;
    lista.topo = 0;
    guardar_preferencias();
    desenhar_tudo();
    registrar("arquivos: vista %s", vista_nome());
}

static void mudar_ocultos(void)
{
    ver_ocultos = !ver_ocultos;
    em_busca = false;          /* cambiar la preferencia sale de la búsqueda */
    padrao_busca[0] = 0;
    guardar_preferencias();
    carregar();
    char msg[120];
    formatar_texto(msg, sizeof msg, "Ocultos %s", ver_ocultos ? "visibles" : "ocultos");
    desenhar_rodape(msg);
    registrar("arquivos: ocultos %s", ver_ocultos ? "sim" : "nao");
}

static void mudar_ordem(int coluna)
{
    if (ordem == coluna) ordem_desc = !ordem_desc;
    else { ordem = coluna; ordem_desc = false; }
    guardar_preferencias();
    carregar();
    const char *n = ordem == ORD_NOME ? "nome" : ordem == ORD_TAMANHO ? "tamanho" : ordem == ORD_TIPO ? "tipo" : "data";
    registrar("arquivos: ordenada por %s%s", n, ordem_desc ? " (desc)" : "");
}

/* La columna del encabezado de la vista de detalles bajo el ratón (-1 = ninguna). */
static int coluna_de_ponto(int x, int y)
{
    if (vista != VISTA_DETALHES) return -1;
    if (y < lista.y - 18 || y >= lista.y) return -1;
    int x0 = lista.x;
    if (x >= x0 + 6 && x < x0 + 70) return ORD_NOME;
    if (x >= col_tam - 48 && x <= col_tam) return ORD_TAMANHO;
    if (x >= col_tipo && x < col_tipo + 34) return ORD_TIPO;
    if (x >= col_data && x < col_data + 42) return ORD_DATA;
    return -1;
}

/* ------------------------------------------------------------ "Abrir con" */

/* Los programas que pueden abrir un archivo. */
typedef struct { const char *nome; } app_t;
static const app_t apps[] = {
    { "editor" }, { "visor" }, { "paint" }, { "markdown" },
    { "mico" }, { "reproductor" }, { "musica" }, { "visorpdf" },
};

/* El programa que abriría este archivo (la asociación por defecto). */
static const char *programa_padrao(const char *c)
{
    bool md = tam_texto(c) > 3 && compara_texto(c + tam_texto(c) - 3, ".md") == 0;
    const char *p = imagem_extensao(c) ? "visor" : md ? "markdown" : "editor";
    if (rom_chip8(c)) p = "chip8";
    if (audio_extensao(c)) p = "musica";
    if (pdf_extensao(c)) p = "visorpdf";
    if (video_extensao(c)) p = "reproductor";
    return p;
}

static void abrir_com(const char *prog, const char *arquivo)
{
    char bin[40];
    formatar_texto(bin, sizeof bin, "/bin/%s", prog);
    char *argv[3];
    argv[0] = (char *)prog; argv[1] = (char *)arquivo; argv[2] = NULL;
    int pid = executar(bin, argv);
    registrar("arquivos: abrir com %s %s (pid %d)", prog, arquivo, pid);
    char msg[200];
    formatar_texto(msg, sizeof msg, pid > 0 ? "Abriendo %s con %s..." : "No se ha podido abrir %s", nome_base(arquivo), prog);
    desenhar_rodape(msg);
}

/* El menú "Abrir con...": el programa de siempre sale marcado con un asterisco. */
static void menu_abrir_com(void)
{
    int i = lista.sel;
    if (i < 0 || i >= lista.qtd) return;
    if (tipos[i] == TIPO_PASTA) { mensagem("Abrir con", "Una carpeta se abre, no se abre con un programa."); return; }
    char c[200];
    caminho_de(i, c, sizeof c);
    const char *padrao = programa_padrao(c);
    char itens[MENU_MAX * 24];
    int k = 0;
    itens[0] = 0;
    for (unsigned a = 0; a < sizeof apps / sizeof apps[0]; a++) {
        const char *p = apps[a].nome;
        if (a) itens[k++] = '\n';
        if (compara_texto(p, padrao) == 0) itens[k++] = '*';
        while (*p && k < (int)sizeof itens - 2) itens[k++] = *p++;
        itens[k] = 0;
    }
    itens[k] = 0;
    menu_contextual(j->id, lista.x + 20, lista.y + 20, itens, MENU_COM);
}

/* ------------------------------------------------------------ menú contextual */

/* Las mismas líneas de siempre (D051 y la mejora 056). */
enum { MI_ABRIR, MI_RENOMEAR, MI_APAGAR, MI_SEP, MI_PROPRIEDADES, MI_SEP2, MI_ZIP, MI_TGZ, MI_EXTRAIR, MI_EXTRAIR_EN };
/* As linhas do menu de grupo (D139). */
enum { MG_ABRIR, MG_COPIAR, MG_CORTAR, MG_SEP, MG_BORRAR, MG_PROPRIEDADES, MG_SEP2, MG_ZIP, MG_TGZ };
enum { MF_NOVA_PASTA, MF_ATUALIZAR, MF_SEP, MF_VOLTAR };

/* El elemento que hay bajo el ratón (según la vista). */
static int item_de_ponto(int x, int y)
{
    if (vista == VISTA_ICONES) {
        int por_linha = grade_colunas();
        int linha = (y - lista.y - 4) / ALT_CELULA, coluna = (x - lista.x - 6) / LARG_CELULA;
        if (linha < 0 || coluna < 0 || coluna >= por_linha) return -1;
        int i = lista.topo + linha * por_linha + coluna;
        return i < lista.qtd ? i : -1;
    }
    int i = lista.topo + (y - lista.y - 2) / LISTA_ALTURA_ITEM;
    return i >= 0 && i < lista.qtd ? i : -1;
}

static bool dentro_lista(int x, int y)
{
    return x >= lista.x && x < lista.x + lista.l && y >= lista.y && y < lista.y + lista.a;
}

static void abrir_menu(const evento_t *e)
{
    if (!dentro_lista(e->x, e->y)) return;
    int i = item_de_ponto(e->x, e->y);
    if (i >= 0) {
        if (marcados[i] && n_marcados > 1) {   /* o menu do grupo (D139) */
            lista.sel = i;
            desenhar_lista();
            char itens[160];
            formatar_texto(itens, sizeof itens,
                           "Abrir (%d)\nCopiar\nCortar\n-\nBorrar (%d)\nPropiedades (%d)\n-\nComprimir en .zip\nComprimir en .tar.gz",
                           n_marcados, n_marcados, n_marcados);
            registrar("arquivos: menu do grupo (%d itens)", n_marcados);
            menu_contextual(j->id, e->x, e->y, itens, MENU_GRUPO);
            return;
        }
        lista.sel = i;   /* como en los otros sistemas: el botón derecho también selecciona */
        limpar_marcados();
        marcados[i] = true;
        n_marcados = 1;
        ancora = i;
        desenhar_lista();
        registrar("arquivos: menu do item %s", nomes[i]);
        menu_contextual(j->id, e->x, e->y, eh_comprimido(nomes[i]) && tipos[i] != TIPO_PASTA
                        ? "Abrir\nRenombrar\nBorrar\n-\nPropiedades\n-\nComprimir en .zip\nComprimir en .tar.gz\nExtraer aquí\nExtraer en carpeta..."
                        : "Abrir\nRenombrar\nBorrar\n-\nPropiedades\n-\nComprimir en .zip\nComprimir en .tar.gz\n~Extraer aquí\n~Extraer en carpeta...", MENU_ITEM);
    } else {
        registrar("arquivos: menu do fundo");
        menu_contextual(j->id, e->x, e->y, compara_texto(pasta, "/") == 0 ? "Nueva carpeta\nActualizar\n-\n~Atrás"
                                                                         : "Nueva carpeta\nActualizar\n-\nAtrás", MENU_FUNDO);
    }
}

static void escolha_menu(int codigo, int indice)
{
    if (indice < 0) { registrar("arquivos: menu cancelado"); return; }
    if (codigo == MENU_ITEM) {
        if (indice == MI_ABRIR) abrir_item(lista.sel);
        else if (indice == MI_RENOMEAR) renomear_item();
        else if (indice == MI_APAGAR) apagar_item();
        else if (indice == MI_PROPRIEDADES) propriedades_item();
        else if (indice == MI_ZIP) comprimir_item(false);
        else if (indice == MI_TGZ) comprimir_item(true);
        else if (indice == MI_EXTRAIR) extrair_item(false);
        else if (indice == MI_EXTRAIR_EN) extrair_item(true);
    } else if (codigo == MENU_FUNDO) {
        if (indice == MF_NOVA_PASTA) nova_pasta();
        else if (indice == MF_ATUALIZAR) carregar();
        else if (indice == MF_VOLTAR) voltar();
    } else if (codigo == MENU_GRUPO) {
        if (indice == MG_ABRIR) abrir_grupo();
        else if (indice == MG_COPIAR) copiar_area(false);
        else if (indice == MG_CORTAR) copiar_area(true);
        else if (indice == MG_BORRAR) apagar_item();
        else if (indice == MG_PROPRIEDADES) propriedades_grupo();
        else if (indice == MG_ZIP) comprimir_grupo(false);
        else if (indice == MG_TGZ) comprimir_grupo(true);
    } else if (codigo == MENU_COM) {
        if (indice < 0 || indice >= (int)(sizeof apps / sizeof apps[0])) return;
        int i = lista.sel;
        if (i < 0 || i >= lista.qtd) return;
        char c[200];
        caminho_de(i, c, sizeof c);
        abrir_com(apps[indice].nome, c);
    }
}

/* ------------------------------------------------------------ pendrive */

/* Verifica os pendrives; devolve true se algo mudou. */
static bool atualizar_pendrive(void)
{
    info_disco_t d;
    bool achou = false;
    for (int i = 0; listar_discos(i, &d) == 1; i++)
        if (d.suportado && !d.particao && (d.flags & DISCO_REMOVIVEL) && !(d.flags & DISCO_BOOT)) { achou = true; break; }
    bool mudou = achou != tem_pendrive || (achou && (d.montado != pendrive.montado || compara_texto(d.nome, pendrive.nome) != 0));
    tem_pendrive = achou;
    if (achou) pendrive = d;
    return mudou;
}

/* D233: el primer lector con un disco de datos (no la imagen de arranque de dentro). */
static bool atualizar_cd(void)
{
    info_disco_t d;
    bool achou = false;
    for (int i = 0; listar_discos(i, &d) == 1; i++)
        if ((d.flags & DISCO_CD) && !d.particao && !(d.flags & DISCO_BOOT) && d.suportado) { achou = true; break; }
    bool mudou = achou != tem_cd || (achou && (d.montado != cdrom.montado || compara_texto(d.rotulo, cdrom.rotulo) != 0 ||
                                              compara_texto(d.ponto, cdrom.ponto) != 0));
    tem_cd = achou;
    if (achou) cdrom = d;
    return mudou;
}

static void acao_cd(void)
{
    if (!tem_cd) return;
    if (cdrom.montado && cdrom.ponto[0] && comeca_com(pasta, cdrom.ponto)) {
        int r = expulsar_disco(cdrom.nome, 0);
        registrar("arquivos: expulsar %s -> %d", cdrom.nome, r);
        if (r < 0) {
            mensagem("Archivos", r == ERRO_OCUPADO ? "El CD/DVD está en uso (¿el instalador?): no se expulsa."
                                                   : "No se ha podido expulsar el CD/DVD.");
            return;
        }
        copia_texto(pasta, "/", sizeof pasta);
        atualizar_cd();
        carregar();
        return;
    }
    if (!cdrom.montado) {
        int r = montar_em(cdrom.nome, "/cd", MONTAR_SO_LEITURA);
        if (r < 0) { char m[120]; formatar_texto(m, sizeof m, "No se ha podido montar el CD/DVD:\n%s", nome_erro(r)); mensagem("Archivos", m); return; }
        atualizar_cd();
    }
    entrar(cdrom.ponto[0] ? cdrom.ponto : "/cd", true);
    registrar("arquivos: cd aberto");
}

static void acao_lado(int acao)
{
    if (acao == LUG_CD) { acao_cd(); return; }
    if (acao == LUG_SISTEMA) { entrar("/", true); return; }
    if (acao == LUG_HOME) { entrar("/home", true); return; }
    if (acao == LUG_TMP) { entrar("/tmp", true); return; }
    if (acao == LUG_LIXEIRA) { entrar("/home/.papelera", true); return; }
    if (acao == LUG_PEN) {
        if (!tem_pendrive) return;
        if (pendrive.montado) {
            char onde[64];
            copia_texto(onde, pendrive.ponto[0] ? pendrive.ponto : "/usb", sizeof onde);
            int r = desmontar(pendrive.nome);
            registrar("arquivos: remover com seguranca -> %d", r);
            if (comeca_com(pasta, onde)) copia_texto(pasta, "/", sizeof pasta);
            atualizar_pendrive();
            desenhar_lateral();
            mensagem("Archivos", r < 0 ? "No se ha podido desmontar el pendrive." : "Ya puedes retirar el pendrive.");
            carregar();
        } else {
            int r = montar(pendrive.nome);
            if (r < 0) { char m[120]; formatar_texto(m, sizeof m, "No se ha podido montar el pendrive:\n%s", nome_erro(r)); mensagem("Archivos", m); }
            else {
                atualizar_pendrive();
                desenhar_lateral();
                entrar(pendrive.ponto[0] ? pendrive.ponto : "/usb", true);
                registrar("arquivos: pendrive aberto");
            }
        }
        return;
    }
    if (acao == ACT_ABRIR_COM) { menu_abrir_com(); return; }
    if (acao == ACT_COPIAR) { copiar_area(false); return; }
    if (acao == ACT_CORTAR) { copiar_area(true); return; }
    if (acao == ACT_COLAR) { colar_area(); return; }
    if (acao == ACT_RENOMEAR) { renomear_item(); return; }
    if (acao == ACT_PROPS) { propriedades_item(); return; }
}

/* ------------------------------------------------------------ selección */

/* Marca o desmarca un elemento (Ctrl). */
static void alternar_marca(int i)
{
    if (i < 0 || i >= lista.qtd) return;
    if (marcados[i]) { marcados[i] = false; n_marcados--; }
    else { marcados[i] = true; n_marcados++; }
    ancora = i;
}

/* Selecciona un rango (Mayús). */
static void marcar_rango(int i)
{
    if (i < 0 || i >= lista.qtd) return;
    if (ancora < 0) ancora = i;
    int a = ancora < i ? ancora : i, b = ancora < i ? i : ancora;
    limpar_marcados();
    for (int k = a; k <= b; k++) { marcados[k] = true; n_marcados++; }
}

/* El recuadro de selección con el ratón. */
static bool arrasto_ativo;
static int arrasto_x, arrasto_y;

static void marcar_rectangulo(int x, int y)
{
    int x0 = arrasto_x < x ? arrasto_x : x, x1 = arrasto_x < x ? x : arrasto_x;
    int y0 = arrasto_y < y ? arrasto_y : y, y1 = arrasto_y < y ? y : arrasto_y;
    limpar_marcados();
    if (vista == VISTA_ICONES) {   /* a grelha (D139) */
        int col = grade_colunas(), lin = grade_linhas();
        for (int k = 0; k < lin; k++)
            for (int c = 0; c < col; c++) {
                int i = lista.topo + k * col + c;
                if (i >= lista.qtd) break;
                int cx = lista.x + 6 + c * LARG_CELULA, cy = lista.y + 4 + k * ALT_CELULA;
                if (cy + 68 < y0 || cy > y1) continue;
                if (cx + 88 < x0 || cx > x1) continue;
                marcados[i] = true;
                n_marcados++;
            }
        return;
    }
    int vis = (lista.a - 4) / LISTA_ALTURA_ITEM;
    for (int k = 0; k < vis && lista.topo + k < lista.qtd; k++) {
        int i = lista.topo + k, fy = lista.y + 2 + k * LISTA_ALTURA_ITEM;
        if (fy + LISTA_ALTURA_ITEM < y0 || fy > y1) continue;
        if (x1 < lista.x + 2 || x0 > lista.x + lista.l - 2) continue;
        marcados[i] = true;
        n_marcados++;
    }
}

/* ------------------------------------------------------------ principal */

static void novo_tamanho(void)
{
    lista.l = LARG - LATERAL - 14;
    lista.a = ALT - lista.y - 30;
    int vis = (lista.a - 4) / LISTA_ALTURA_ITEM;
    if (lista.sel >= lista.topo + vis) lista.topo = lista.sel - vis + 1;
    if (lista.topo > lista.qtd - vis) lista.topo = lista.qtd - vis;
    if (lista.topo < 0) lista.topo = 0;
    desenhar_tudo();
    jan_mostrar(j);
    registrar("arquivos: janela %dx%d, %d itens visiveis", LARG, ALT, vis);
}

int principal(int argc, char **argv)
{
    if (argc > 1) copia_texto(pasta, argv[1], sizeof pasta);
    ler_preferencias();
    ler_area();
    j = jan_abrir("Archivos", LARG_INICIO, ALT_INICIO);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL, 420, 260);
    lista = (lista_t){ LATERAL + 6, Y_LISTA, LARG - LATERAL - 14, ALT - Y_LISTA - 30, nomes, 0, -1, 0, NULL, desenhar_lista_gancho };
    /* La carpeta de arranque es el primer paso del historial (el botón "Atrás"). */
    hist_pos = 0;
    hist_n = 1;
    copia_texto(hist[0], pasta, sizeof hist[0]);
    atualizar_pendrive();
    atualizar_cd();
    carregar();
    jan_mostrar(j);
    registrar("arquivos: pronto");

    evento_t e;
    unsigned ultimo = tempo_ms();
    for (;;) {
        if (proximo_evento(j->id, &e, false) != 1) {
            /* Sem evento: de vez en quando verifica se entrou ou saiu um pendrive. */
            if (tempo_ms() - ultimo > 700) {
                ultimo = tempo_ms();
                if (atualizar_pendrive()) {
                    desenhar_lateral();
                    info_arquivo_t inf;
                    if (comeca_com(pasta, "/usb") && info_arquivo(pasta, &inf) < 0) { copia_texto(pasta, "/", sizeof pasta); carregar(); }
                }
                /* D233: el CD que se mete o se saca (si se sacó estando dentro, a la raíz). */
                if (atualizar_cd()) {
                    desenhar_lateral();
                    info_arquivo_t inf;
                    if (info_arquivo(pasta, &inf) < 0) { copia_texto(pasta, "/", sizeof pasta); carregar(); }
                }
            }
            dormir(50);
            continue;
        }
        int c;
        while (recolher(&c) > 0) ;   /* programas abertos desde acá que ya terminaron */
        if (e.tipo == EV_FECHAR) break;
        if (jan_redimensionar(j, &e)) { novo_tamanho(); continue; }
        if (e.tipo == EV_MENU) { escolha_menu((int)e.tecla, e.x); continue; }

        /* El cuadro de la ruta (Ctrl+L) y el de la búsqueda. */
        if (caixa_tratar(j, &caixa, &e)) {
            if (modo_caixa == MODO_ROTA) {
                char v[160];
                copia_texto(v, caixa.texto, sizeof v);
                caixa.foco = false;
                entrar(v, true);
            } else {
                char v[64];
                copia_texto(v, caixa.texto, sizeof v);
                caixa.foco = false;
                if (v[0]) buscar(v);
            }
            continue;
        }
        if (caixa.foco && e.tipo == EV_TECLA) {
            if (e.tecla == TECLA_ESC) { caixa.foco = false; desenhar_rota(); continue; }
            if (modo_caixa == MODO_BUSCA) {
                /* filtro rápido: la lista se va filtrando mientras se escribe */
                char antes[64];
                copia_texto(antes, filtro, sizeof antes);
                copia_texto(filtro, caixa.texto, sizeof filtro);
                if (compara_texto(antes, filtro) != 0) {
                    em_busca = false;
                    carregar();
                    char msg[160];
                    formatar_texto(msg, sizeof msg, "Filtro: %s", filtro);
                    desenhar_rodape(msg);
                }
            }
            continue;
        }

        /* El botón derecho abre el menú contextual. */
        if (e.tipo == EV_CLIQUE && (e.botoes & 2)) { abrir_menu(&e); continue; }

        /* La barra de herramientas. */
        bool clic = false;
        for (int i = 0; i < 8; i++) {
            if (bt[i].x + bt[i].l > LARG) break;
            if (!botao_tratar_icone(&bt[i], IC_BT[i], ROT_BT[i], &e)) continue;
            clic = true;
            if (i == 0) voltar();
            else if (i == 1) nova_pasta();
            else if (i == 2) apagar_item();
            else if (i == 3) avancar();
            else if (i == 4) subir();
            else if (i == 5) mudar_ocultos();
            else if (i == 6) mudar_vista();
            else if (i == 7) {
                modo_caixa = MODO_BUSCA;
                caixa_definir(&caixa, filtro);
                caixa.foco = true;
                desenhar_rota();
            }
        }
        if (clic) continue;

        /* El panel lateral. */
        for (int i = 0; i < qtd_lado; i++) {
            if (!botao_tratar_icone(&lado[i].b, lado[i].icone, lado[i].texto, &e)) continue;
            acao_lado(lado[i].acao);
            clic = true;
            break;
        }
        if (clic) continue;

        /* El encabezado de la vista de_details: al pulsarlo se ordena por esa columna. */
        if (e.tipo == EV_CLIQUE && (e.botoes & 1)) {
            int coluna = coluna_de_ponto(e.x, e.y);
            if (coluna >= 0) { mudar_ordem(coluna); continue; }
        }

        /* Las migas de pan de la barra de ruta (y, si se pincha fuera de ellas,
         * el cuadro de la ruta se abre para editarla). */
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && e.y >= ALT_BARRA && e.y < ALT_BARRA + ALT_ROTA) {
            bool numa_miga = false;
            for (int i = 0; i < n_migas; i++)
                if (e.x >= migas[i].x && e.x < migas[i].x + migas[i].l) { entrar(migas[i].caminho, true); numa_miga = true; break; }
            if (!numa_miga) {
                modo_caixa = MODO_ROTA;
                caixa_definir(&caixa, pasta);
                caixa.foco = true;
                desenhar_rota();
            }
        }

        /* El recuadro de selección con el ratón. */
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && dentro_lista(e.x, e.y) && item_de_ponto(e.x, e.y) < 0) {
            arrasto_ativo = true;
            arrasto_x = e.x;
            arrasto_y = e.y;
            limpar_marcados();
            desenhar_lista();
            continue;
        }
        if (e.tipo == EV_MOVER && arrasto_ativo && (e.botoes & 1)) {
            marcar_rectangulo(e.x, e.y);
            desenhar_lista();
            continue;
        }
        if (e.tipo == EV_SOLTOU && arrasto_ativo) {
            arrasto_ativo = false;
            marcar_rectangulo(e.x, e.y);
            desenhar_lista();
            continue;
        }

        /* La lista: un clic elige (Ctrl y Mayús añaden o quitan del rango). */
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && dentro_lista(e.x, e.y)) {
            int i = item_de_ponto(e.x, e.y);
            if (i >= 0) {
                bool ctrl = (e.modificadores & 2) != 0, mayus = (e.modificadores & 1) != 0;
                if (ctrl) alternar_marca(i);
                else if (mayus) marcar_rango(i);
                else { limpar_marcados(); marcados[i] = true; n_marcados = 1; ancora = i; }
                lista.sel = i;
                desenhar_lista();
                char msg[200];
                info_item(i, msg, sizeof msg);
                desenhar_rodape(msg);
                if (e.cliques >= 2) abrir_item(i);
                continue;
            }
        }
        /* Na vista de icones a roda e as setas sao da grelha (D139): a lista
         * comum rolaria um item de cada vez e a grelha andaria de lado. */
        if (vista == VISTA_ICONES && e.tipo == EV_RODA && dentro_lista(e.x, e.y)) {
            if (grade_rolar((int)e.cliques * RODA_LINHAS))
                registrar("arquivos: rolou a grelha para o item %d (linha %d de %d)",
                          lista.topo, lista.topo / grade_colunas(), lista.qtd);
            continue;
        }
        if (vista == VISTA_ICONES && e.tipo == EV_TECLA) {
            unsigned tk = e.tecla;
            if (tk == TECLA_BAIXO || tk == TECLA_CIMA || tk == TECLA_DIREITA || tk == TECLA_ESQUERDA ||
                tk == TECLA_HOME || tk == TECLA_END || tk == TECLA_PGUP || tk == TECLA_PGDN) {
                grade_tecla(tk);
                continue;
            }
        }
        /* En la vista de iconos un clic en un hueco no es de la lista comun (D217): sus filas
         * son de otra altura y seleccionaria un elemento que no esta bajo el raton. */
        int r = (vista == VISTA_ICONES && e.tipo == EV_CLIQUE) ? LISTA_NADA : lista_tratar(j, &lista, &e);
        if (r == LISTA_ATIVOU) abrir_item(lista.sel);
        else if (r == LISTA_SELECIONOU && lista.sel >= 0) {
            limpar_marcados();
            marcados[lista.sel] = true;
            n_marcados = 1;
            ancora = lista.sel;
            desenhar_lista();
            char msg[200];
            info_item(lista.sel, msg, sizeof msg);
            desenhar_rodape(msg);
        }

        /* El teclado. */
        if (e.tipo == EV_TECLA) {
            unsigned t = e.tecla;
            bool ctrl = (e.modificadores & 2) != 0;
            if (ctrl && t == 8) { mudar_ocultos(); continue; }          /* Ctrl+H */
            if (ctrl && t == 1) {                                        /* Ctrl+A */
                limpar_marcados();
                for (int i = 0; i < lista.qtd; i++) { marcados[i] = true; n_marcados++; }
                desenhar_lista();
                char msg[80];
                formatar_texto(msg, sizeof msg, "%d elementos selecionados", n_marcados);
                desenhar_rodape(msg);
                registrar("arquivos: selecionados %d itens", n_marcados);
                continue;
            }
            if (ctrl && t == 3) { copiar_area(false); continue; }        /* Ctrl+C */
            if (ctrl && t == 24) { copiar_area(true); continue; }         /* Ctrl+X */
            if (ctrl && t == 22) { colar_area(); continue; }              /* Ctrl+V */
            if (ctrl && t == 12) {                                        /* Ctrl+L */
                modo_caixa = MODO_ROTA;
                caixa_definir(&caixa, pasta);
                caixa.foco = true;
                desenhar_rota();
                continue;
            }
            if (t == 0x10B) { renomear_item(); continue; }                /* F2 */
            if (t == TECLA_DELETE) { apagar_item(); continue; }
            if (t == '\b') { subir(); continue; }
            if (t == TECLA_ESC) {
                if (caixa.foco) { caixa.foco = false; desenhar_rota(); continue; }
                limpar_marcados();
                desenhar_lista();
                continue;
            }
        }
    }
    jan_fechar(j);
    return 0;
}
