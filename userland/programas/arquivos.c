// SPDX-License-Identifier: GPL-3.0-only
/* arquivos [carpeta] - el explorador de archivos de SaguiOS (H23; mejora 058, D136; renovado en la v8.5.5.2, D420).
 *
 * Una ventana de verdad: barra de herramientas (atrás, adelante, arriba, nueva carpeta, borrar,
 * ocultos, vista y buscar), barra de órdenes con iconos (nuevo, cortar, copiar, pegar, renombrar,
 * comprimir, extraer, terminal aquí, propiedades, vistas, panel doble, vista previa...), pestañas
 * (Ctrl+T, Ctrl+W, Ctrl+Tab), panel doble (F3) con arrastrar y soltar entre paneles, pestañas y
 * la barra lateral, migas de pan con flecha de subcarpetas y ruta editable con autocompletar
 * (Ctrl+L, Tab), barra lateral (Inicio, Favoritos fijables, unidades con su barra de espacio,
 * Papelera y etiquetas de color), vistas (lista, detalles con columnas ordenables y ajustables,
 * iconos con miniaturas y mosaicos), agrupar por tipo o fecha, panel de vista previa (Alt+P),
 * búsqueda recursiva con filtros en segundo plano, página de Inicio (recientes, favoritos y
 * unidades) y estado de git en las carpetas de un repositorio.
 *
 * Las ideas de diseño vienen de «Files» (explorador de Windows, licencia MIT: solo ideas, ningún
 * código); el aspecto es el de SaguiOS. El estado de cada panel vive en painel_t: el resto del
 * programa usa nombres cortos (pasta, nomes, lista...) que son macros sobre el panel activo.
 *
 * Lo que no cambia: el menú contextual (D051) tiene las mismas líneas de siempre (las nuevas van
 * al final) y los mensajes del registro ("arquivos: pronto", "arquivos: pasta ... (N itens)",
 * "arquivos: pasta criada ...", "arquivos: apagado ...") son los que las pruebas de la H23, la H34,
 * la H43, la H45 y la H55 esperan. */
#include "sagui.h"
#include "gui.h"
#include "icones.h"
#include "imagem.h"
#include "contas.h"
#include "conf_maquina.h"
#include "sonidos.h"

#define LARG_INICIO 720
#define ALT_INICIO  440
#define LATERAL 140      /* barra lateral */
#define MAX_ITENS 256    /* elementos que se guardan de una carpeta */
#define MAX_FILAS (MAX_ITENS + 16)   /* y las cabeceras de grupo */
#define ALT_BARRA 40     /* barra de herramientas */
#define ALT_ORDENS 28    /* barra de órdenes (D420) */
#define ALT_ROTA  26     /* barra de ruta */
#define ALT_STATUS 24    /* barra de estado */
#define LARG_PREVIA 200  /* panel de vista previa */
#define LARG (j->larg)
#define ALT  (j->alt)
#define TIPO_GRUPO 99    /* fila que es una cabecera de grupo, no un archivo */
#define INICIO_RUTA "@inicio"   /* la página de Inicio es una «carpeta» virtual */

/* Vistas de la lista. */
enum { VISTA_LISTA, VISTA_DETALHES, VISTA_ICONES, VISTA_MOSAICOS };
/* Columnas por las que se ordena (las de la vista de detalles en este orden). */
enum { ORD_NOME, ORD_TAMANHO, ORD_TIPO, ORD_DATA };
/* Por qué se agrupa. */
enum { AGR_NADA, AGR_TIPO, AGR_DATA };
/* Qué hace el cuadro de la barra de ruta. */
enum { MODO_ROTA, MODO_BUSCA };
/* Códigos de los menús contextuales (el código vuelve en EV_MENU). */
#define MENU_ITEM  1
#define MENU_FUNDO 2
#define MENU_COM   3   /* "Abrir con..." */
#define MENU_GRUPO 4   /* accoes para varios elementos marcados (D139) */
#define MENU_NUEVO 5   /* «Nuevo» de la barra de órdenes */
#define MENU_VER   6   /* «Ver»: vistas, agrupar, ordenar */
#define MENU_MIGA  7   /* subcarpetas de una miga de pan */
#define MENU_ETIQ  8   /* elegir la etiqueta de color de lo seleccionado */
#define MENU_FILTRO 9  /* filtros de la búsqueda */
#define MENU_FAV   10
#define MENU_AUTOC 11  /* candidatos al completar la ruta */  /* quitar un favorito */
#define MENU_MAX   64  /* líneas de un menú (las del kernel) */

#define MAX_HIST 32     /* carpetas visitadas (atrás y adelante) */
#define MAX_AREA 64     /* archivos del portapapeles */
#define MAX_LADO 40     /* filas de la barra lateral */
#define LARG_CELULA 92  /* largura de uma celula da vista de icones */
#define ALT_CELULA  74  /* altura de uma celula da vista de icones */
#define LARG_MOSAICO 176 /* y la del mosaico */
#define ALT_MOSAICO  52
#define MAX_ARGS_GRUPO 64    /* argv do grupo: o tar aceita 64 rotas (D139) */
#define MAX_FAV 12
#define MAX_RECIENTES 12
#define MAX_ETQ 160     /* archivos con etiqueta de color */
#define N_CORES 6
#ifndef TECLA_F3
#define TECLA_F3 0x10C
#define TECLA_F5 0x10E
#endif
#define ALVOS_MAX 36

/* ------------------------------------------------------------ estado */

/* Una pestaña guardada (la activa de cada panel vive en el propio panel). */
typedef struct { char a_pasta[160]; int a_vista, a_ordem, a_agrupar; bool a_ordem_desc; } aba_t;

/* Todo lo de un panel de archivos. */
typedef struct {
    int x, l;                         /* franja horizontal que ocupa en la ventana */
    char s_pasta[160];
    char s_nomes[MAX_FILAS][64];
    uint8_t s_tipos[MAX_FILAS];
    uint32_t s_tamanhos[MAX_FILAS];
    uint32_t s_modificadas[MAX_FILAS];
    bool s_tem_data[MAX_FILAS];
    uint8_t grupo[MAX_FILAS];         /* a qué grupo pertenece (al agrupar) */
    char s_rotas[MAX_FILAS][200];       /* ruta completa (en la búsqueda) */
    bool s_marcados[MAX_FILAS];         /* selección múltiple */
    int s_n_marcados;
    int s_ancora;                       /* ancla del rango (Mayús) */
    lista_t s_lista;
    char s_filtro[64];                  /* filtro rápido mientras se escribe */
    char s_padrao_busca[64];            /* patrón de la búsqueda */
    int s_n_resultados;
    bool s_em_busca;
    char s_hist[MAX_HIST][160];
    int s_hist_n, s_hist_pos;
    int s_vista, s_ordem, s_agrupar;
    bool s_ordem_desc;
    aba_t abas[ABAS_MAX];
    int n_abas, aba_sel;
    abas_t barra_abas;
    migas_t migas;
    int y_rota;                       /* donde empieza la barra de ruta de este panel */
    int n_alvos;                      /* tarjetas pulsables de la página de Inicio */
    int alvo_h;                       /* la que tiene el ratón encima + 1 (0 = ninguna; D423) */
    struct { int x, y, l, a; char cam[160]; } alvos[ALVOS_MAX];
    uint8_t gitm[MAX_FILAS];          /* estado de git de cada elemento ('M', '?', 'A', 'D' o 0) */
    char rama[48];                    /* rama de git de la carpeta ("" = no es un repositorio) */
} painel_t;

static painel_t paineis[2];
static painel_t *P = &paineis[0];
static int n_paineis = 1, ativo;
static bool previa;                   /* panel de vista previa a la derecha */

#define pasta (P->s_pasta)
#define nomes (P->s_nomes)
#define tipos (P->s_tipos)
#define tamanhos (P->s_tamanhos)
#define modificadas (P->s_modificadas)
#define tem_data (P->s_tem_data)
#define rotas (P->s_rotas)
#define marcados (P->s_marcados)
#define n_marcados (P->s_n_marcados)
#define ancora (P->s_ancora)
#define lista (P->s_lista)
#define filtro (P->s_filtro)
#define padrao_busca (P->s_padrao_busca)
#define n_resultados (P->s_n_resultados)
#define em_busca (P->s_em_busca)
#define hist (P->s_hist)
#define hist_n (P->s_hist_n)
#define hist_pos (P->s_hist_pos)
#define vista (P->s_vista)
#define ordem (P->s_ordem)
#define ordem_desc (P->s_ordem_desc)
#define agrupar (P->s_agrupar)

static jan_t *j;
static bool ver_ocultos;
static info_disco_t pendrive;
static bool tem_pendrive;
static info_disco_t cdrom;           /* D233: el CD/DVD de datos que haya en un lector */
static bool tem_cd;
static char rodape[200];
static colunas_t colunas;            /* columnas de la vista de detalles (las comparten los paneles) */

/* Portapapeles de archivos. */
static char area_caminhos[MAX_AREA][200];
static int area_n;
static bool area_mover;
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
    case 'P': return esq_trazo(0x202428);   /* D428 */
    case 'Y': return 0xFFE070;
    case 'A': return 0xF2C230;
    case 'B': return 0xFFFFFF;
    case 'C': return 0x9AA3AC;
    case 'V': return 0x2A6FD8;
    case 'D': return esq_trazo(0x182030);
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
        formatar_texto(s, max, "%u%c%u GB", kb / (1024u * 1024u), regional_decimal(), (kb % (1024u * 1024u)) * 10u / (1024u * 1024u));   /* D313: coma o punto */
    else if (kb >= 1024u)
        formatar_texto(s, max, "%u%c%u MB", kb / 1024u, regional_decimal(), (kb % 1024u) * 10u / 1024u);
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
            formatar_texto(s, max, _("%s libres"), t);
            return;
        }
    info_memoria_t mem;
    if (info_sistema(INFO_MEMORIA, &mem, sizeof mem) == 0) {
        char t[24];
        tamanho_kb(mem.livre_kb, t, sizeof t);
        formatar_texto(s, max, _("%s libres (RAM)"), t);
        return;
    }
    copia_texto(s, _("espacio desconocido"), max);
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
    if (tipos[i] == TIPO_PASTA) return _("carpeta");
    if (tipos[i] == TIPO_PROGRAMA) return _("programa");
    return _("archivo");
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
static bool casa_filtro(const char *f, const char *nome)
{
    if (!f[0]) return true;
    if (acha_char(f, '*') || acha_char(f, '?')) return casar_glob(f, nome);
    return contem(nome, f);
}

/* ------------------------------------------------------------ preferencias */

static bool existe_pasta(const char *c)
{
    info_arquivo_t inf;
    return info_arquivo(c, &inf) == 0 && inf.tipo == TIPO_PASTA;
}

/* D420: lo que Archivos guarda (preferencias, portapapeles, favoritos, etiquetas, recientes) va en la
 * carpeta ~/.config de la cuenta (HOME, o la de la cuenta si no hay HOME; /home/.config en el caso
 * simple). Si no hay carpeta personal, en /etc (la capa de RAM del sistema). */
static bool conf_caminho(const char *nome, char *s, size_t max)
{
    static char base[96];
    if (!base[0]) {
        const char *h = obter_env("HOME");
        char c[64];
        if (h && h[0] && compara_texto(h, "/") != 0 && existe_pasta(h)) copia_texto(base, h, sizeof base);
        else if (casa_real_de_uid(meu_uid(), c, sizeof c) && existe_pasta(c)) copia_texto(base, c, sizeof base);
        else if (existe_pasta("/home")) copia_texto(base, "/home", sizeof base);
        else copia_texto(base, "-", sizeof base);
    }
    if (base[0] != '-') { formatar_texto(s, max, "%s/.config/%s", base, nome); return true; }
    if (compara_texto(nome, "arquivos") == 0) copia_texto(s, "/etc/arquivos.conf", max);
    else formatar_texto(s, max, "/etc/%s", nome);
    return false;
}

/* Crea ~/.config si hace falta antes de escribir un fichero de configuración. */
static void conf_garantir(const char *caminho)
{
    char d[120];
    copia_texto(d, caminho, sizeof d);
    char *b = acha_ultimo_char(d, '/');
    if (!b || b == d) return;
    *b = 0;
    if (!existe_pasta(d)) criar_pasta(d);
}

/* Escribe un fichero de texto pequeño (una línea por cosa). */
static bool gravar_linhas(const char *caminho, const char *const *linhas, int n)
{
    conf_garantir(caminho);
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
    char buf[1024], c[120];
    conf_caminho("arquivos", c, sizeof c);
    int n = ler_linhas(c, buf, sizeof buf, 16);
    int ini = 0;
    for (int i = 0; i < n; i++) {
        char *l = buf + ini;
        ini += (int)tam_texto(l) + 1;
        if (comeca_com(l, "ocultos=")) ver_ocultos = l[8] == '1';
        else if (comeca_com(l, "vista=")) {
            if (compara_texto(l + 6, "detalhes") == 0) vista = VISTA_DETALHES;
            else if (compara_texto(l + 6, "icones") == 0) vista = VISTA_ICONES;
            else if (compara_texto(l + 6, "mosaicos") == 0) vista = VISTA_MOSAICOS;
            else vista = VISTA_LISTA;
        } else if (comeca_com(l, "ordem=")) {
            if (compara_texto(l + 6, "tamanho") == 0 || compara_texto(l + 6, "tamanho-") == 0) ordem = ORD_TAMANHO;
            else if (comeca_com(l + 6, "tipo")) ordem = ORD_TIPO;
            else if (comeca_com(l + 6, "data")) ordem = ORD_DATA;
            else ordem = ORD_NOME;
            ordem_desc = l[tam_texto(l) - 1] == '-';
        } else if (comeca_com(l, "agrupar=")) {
            agrupar = compara_texto(l + 8, "tipo") == 0 ? AGR_TIPO : compara_texto(l + 8, "data") == 0 ? AGR_DATA : AGR_NADA;
        } else if (comeca_com(l, "previa=")) previa = l[7] == '1';
        else if (comeca_com(l, "col=")) {   /* anchos de las columnas: tamaño,tipo,fecha */
            const char *q = l + 4;
            int v[3] = { 0, 0, 0 };
            for (int k = 0; k < 3; k++) {
                v[k] = texto_para_int(q);
                while (*q && *q != ',') q++;
                if (*q == ',') q++;
            }
            if (v[0] >= 40 && v[1] >= 40 && v[2] >= 40 && v[0] <= 320 && v[1] <= 320 && v[2] <= 320) {
                colunas.larg[1] = v[0]; colunas.larg[2] = v[1]; colunas.larg[3] = v[2];
            }
        }
    }
}

static void guardar_preferencias(void)
{
    char c[120];
    conf_caminho("arquivos", c, sizeof c);
    char l1[32], l2[32], l3[32], l4[32], l5[32], l6[48];
    formatar_texto(l1, sizeof l1, "ocultos=%d", ver_ocultos ? 1 : 0);
    formatar_texto(l2, sizeof l2, "vista=%s", vista == VISTA_DETALHES ? "detalhes" : vista == VISTA_ICONES ? "icones" : vista == VISTA_MOSAICOS ? "mosaicos" : "lista");
    formatar_texto(l3, sizeof l3, "ordem=%s%s",
                    ordem == ORD_TAMANHO ? "tamanho" : ordem == ORD_TIPO ? "tipo" : ordem == ORD_DATA ? "data" : "nome",
                    ordem_desc ? "-" : "");
    formatar_texto(l4, sizeof l4, "agrupar=%s", agrupar == AGR_TIPO ? "tipo" : agrupar == AGR_DATA ? "data" : "nada");
    formatar_texto(l5, sizeof l5, "previa=%d", previa ? 1 : 0);
    formatar_texto(l6, sizeof l6, "col=%d,%d,%d", colunas.larg[1], colunas.larg[2], colunas.larg[3]);
    const char *linhas[6] = { l1, l2, l3, l4, l5, l6 };
    gravar_linhas(c, linhas, 6);
}

/* ------------------------------------------------------------ portapapeles */

/* El portapapeles de archivos: la primera línea dice si es copia o corte y las
 * demás son las rutas completas. */
static void ler_area(void)
{
    char buf[8192], c[120];
    conf_caminho("arquivos.area", c, sizeof c);
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
    char c[120];
    conf_caminho("arquivos.area", c, sizeof c);
    conf_garantir(c);
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

/* ------------------------------------------------------------ favoritos, recientes y etiquetas */

/* Favoritos: las carpetas fijadas en la barra lateral (arrastrando una carpeta o con el menú). Mientras no
 * se haya fijado nada, salen las de siempre (Sistema, home, tmp). Por cuenta, en ~/.config/arquivos.fav. */
static char fav[MAX_FAV][160];
static int n_fav;
static bool fav_propios;     /* ya hay un fichero de favoritos (lo del usuario) */

static void ler_fav(void)
{
    char buf[MAX_FAV * 164], c[120];
    conf_caminho("arquivos.fav", c, sizeof c);
    int n = ler_linhas(c, buf, sizeof buf, MAX_FAV);
    n_fav = 0;
    int ini = 0;
    fav_propios = n > 0;
    for (int i = 0; i < n; i++) {
        char *l = buf + ini;
        ini += (int)tam_texto(l) + 1;
        if (l[0] == '/') copia_texto(fav[n_fav++], l, sizeof fav[0]);
    }
    if (!fav_propios) {
        copia_texto(fav[n_fav++], "/", sizeof fav[0]);
        if (existe_pasta("/home")) copia_texto(fav[n_fav++], "/home", sizeof fav[0]);
        if (existe_pasta("/tmp")) copia_texto(fav[n_fav++], "/tmp", sizeof fav[0]);
    }
}

static void guardar_fav(void)
{
    char c[120];
    conf_caminho("arquivos.fav", c, sizeof c);
    const char *l[MAX_FAV];
    for (int i = 0; i < n_fav; i++) l[i] = fav[i];
    if (n_fav == 0) { conf_garantir(c); int fd = abrir(c, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR); if (fd >= 0) { escrever_tudo(fd, "\n", 1); fechar(fd); } }
    else gravar_linhas(c, l, n_fav);
    fav_propios = true;
}

static int fav_indice(const char *c)
{
    for (int i = 0; i < n_fav; i++) if (compara_texto(fav[i], c) == 0) return i;
    return -1;
}

static bool fav_anadir(const char *c)
{
    if (n_fav >= MAX_FAV || fav_indice(c) >= 0 || !existe_pasta(c)) return false;
    copia_texto(fav[n_fav++], c, sizeof fav[0]);
    guardar_fav();
    registrar("arquivos: favorito anadido %s", c);
    return true;
}

static void fav_quitar(int i)
{
    if (i < 0 || i >= n_fav) return;
    registrar("arquivos: favorito quitado %s", fav[i]);
    for (int k = i; k + 1 < n_fav; k++) copia_texto(fav[k], fav[k + 1], sizeof fav[0]);
    n_fav--;
    guardar_fav();
}

/* Recientes: lo último que se abrió (carpetas y archivos), lo más nuevo primero. */
static char reciente[MAX_RECIENTES][160];
static int n_reciente;

static void ler_reciente(void)
{
    char buf[MAX_RECIENTES * 164], c[120];
    conf_caminho("arquivos.recientes", c, sizeof c);
    int n = ler_linhas(c, buf, sizeof buf, MAX_RECIENTES);
    n_reciente = 0;
    int ini = 0;
    for (int i = 0; i < n; i++) {
        char *l = buf + ini;
        ini += (int)tam_texto(l) + 1;
        if (l[0] == '/') copia_texto(reciente[n_reciente++], l, sizeof reciente[0]);
    }
}

static void anotar_reciente(const char *c)
{
    if (!c[0] || compara_texto(c, "/") == 0) return;
    int k = -1;
    for (int i = 0; i < n_reciente; i++) if (compara_texto(reciente[i], c) == 0) { k = i; break; }
    if (k < 0) { k = n_reciente < MAX_RECIENTES ? n_reciente++ : MAX_RECIENTES - 1; }
    for (int i = k; i > 0; i--) copia_texto(reciente[i], reciente[i - 1], sizeof reciente[0]);
    copia_texto(reciente[0], c, sizeof reciente[0]);
    char ruta[120];
    conf_caminho("arquivos.recientes", ruta, sizeof ruta);
    const char *l[MAX_RECIENTES];
    for (int i = 0; i < n_reciente; i++) l[i] = reciente[i];
    gravar_linhas(ruta, l, n_reciente);
}

/* Etiquetas de color: cada archivo puede llevar una de seis; se guardan por cuenta en
 * ~/.config/arquivos.etiquetas, una línea «N|ruta». */
static const uint32_t COR_ETQ[N_CORES] = { 0xD83A3A, 0xE88A1C, 0xE8C81C, 0x2FA84F, 0x2A6FD8, 0x8A4FC8 };
static const char *const NOME_ETQ[N_CORES] = { N_("Rojo"), N_("Naranja"), N_("Amarillo"), N_("Verde"), N_("Azul"), N_("Violeta") };
static char etq_ruta[MAX_ETQ][160];
static uint8_t etq_cor[MAX_ETQ];
static int n_etq;

static void ler_etiquetas(void)
{
    static char buf[MAX_ETQ * 164];
    char c[120];
    conf_caminho("arquivos.etiquetas", c, sizeof c);
    int n = ler_linhas(c, buf, sizeof buf, MAX_ETQ);
    n_etq = 0;
    int ini = 0;
    for (int i = 0; i < n; i++) {
        char *l = buf + ini;
        ini += (int)tam_texto(l) + 1;
        if (l[0] >= '0' && l[0] < '0' + N_CORES && l[1] == '|' && l[2] == '/') {
            etq_cor[n_etq] = (uint8_t)(l[0] - '0');
            copia_texto(etq_ruta[n_etq++], l + 2, sizeof etq_ruta[0]);
        }
    }
}

static void guardar_etiquetas(void)
{
    static char linhas[MAX_ETQ][164];
    const char *l[MAX_ETQ];
    char c[120];
    conf_caminho("arquivos.etiquetas", c, sizeof c);
    for (int i = 0; i < n_etq; i++) { formatar_texto(linhas[i], sizeof linhas[0], "%d|%s", etq_cor[i], etq_ruta[i]); l[i] = linhas[i]; }
    if (n_etq == 0) { conf_garantir(c); int fd = abrir(c, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR); if (fd >= 0) fechar(fd); }
    else gravar_linhas(c, l, n_etq);
}

/* La etiqueta de una ruta (-1 = sin etiqueta). */
static int etiqueta_de(const char *c)
{
    for (int i = 0; i < n_etq; i++) if (compara_texto(etq_ruta[i], c) == 0) return etq_cor[i];
    return -1;
}

/* Pone (cor >= 0) o quita (cor < 0) la etiqueta de una ruta. */
static void etiqueta_poner(const char *c, int cor)
{
    int k = -1;
    for (int i = 0; i < n_etq; i++) if (compara_texto(etq_ruta[i], c) == 0) { k = i; break; }
    if (cor < 0) {
        if (k < 0) return;
        for (int i = k; i + 1 < n_etq; i++) { copia_texto(etq_ruta[i], etq_ruta[i + 1], sizeof etq_ruta[0]); etq_cor[i] = etq_cor[i + 1]; }
        n_etq--;
        return;
    }
    if (k < 0) { if (n_etq >= MAX_ETQ) return; k = n_etq++; copia_texto(etq_ruta[k], c, sizeof etq_ruta[0]); }
    etq_cor[k] = (uint8_t)cor;
}

/* ------------------------------------------------------------ lista */

/* ¿La extensión del nombre es una de la lista (en minúsculas, separadas por espacios)? */
static bool tem_ext(const char *nome, const char *exts)
{
    const char *pt = acha_ultimo_char(nome, '.');
    if (!pt || !pt[1]) return false;
    char e[12];
    int n = 0;
    for (const char *q = pt + 1; *q && n < 10; q++) e[n++] = (*q >= 'A' && *q <= 'Z') ? (char)(*q + 32) : *q;
    e[n] = 0;
    size_t t = tam_texto(e);
    for (const char *q = exts; *q;) {
        const char *f = q;
        while (*f && *f != ' ') f++;
        if ((size_t)(f - q) == t && compara_texto_n(q, e, t) == 0) return true;
        q = *f ? f + 1 : f;
    }
    return false;
}

static bool eh_imagem_arq(const char *n) { return tem_ext(n, "png bmp ppm pgm pbm pnm jpg jpeg gif"); }

/* Los grupos de «agrupar por tipo» y de «agrupar por fecha». */
enum { GT_CARPETAS, GT_IMAGENES, GT_DOCS, GT_AUDIOVIDEO, GT_PROGRAMAS, GT_COMPRIMIDOS, GT_OTROS, GT_N };
enum { GD_HOY, GD_AYER, GD_SEMANA, GD_MES, GD_ANO, GD_ANTIGUO, GD_SIN, GD_N };

static const char *grupo_nome(int agr, int g)
{
    if (agr == AGR_TIPO) {
        switch (g) {
        case GT_CARPETAS: return _("Carpetas");
        case GT_IMAGENES: return _("Imágenes");
        case GT_DOCS: return _("Documentos");
        case GT_AUDIOVIDEO: return _("Audio y vídeo");
        case GT_PROGRAMAS: return _("Programas");
        case GT_COMPRIMIDOS: return _("Comprimidos");
        default: return _("Otros");
        }
    }
    switch (g) {
    case GD_HOY: return _("Hoy");
    case GD_AYER: return _("Ayer");
    case GD_SEMANA: return _("Últimos 7 días");
    case GD_MES: return _("Últimos 30 días");
    case GD_ANO: return _("Último año");
    case GD_ANTIGUO: return _("Más antiguo");
    default: return _("Sin fecha");
    }
}

static uint32_t data_de(int i);

static int grupo_de(int i)
{
    if (agrupar == AGR_TIPO) {
        if (tipos[i] == TIPO_PASTA) return GT_CARPETAS;
        if (tipos[i] == TIPO_PROGRAMA) return GT_PROGRAMAS;
        if (eh_imagem_arq(nomes[i])) return GT_IMAGENES;
        if (tem_ext(nomes[i], "txt md pdf doc docx odt rtf csv xls xlsx pptx tcn c h py sh json xml html htm css js")) return GT_DOCS;
        if (tem_ext(nomes[i], "wav mp3 flac ogg mid midi kar avi mpg mpeg mp4 m4v mkv")) return GT_AUDIOVIDEO;
        if (tem_ext(nomes[i], "zip gz tgz tar bz2 xz 7z")) return GT_COMPRIMIDOS;
        return GT_OTROS;
    }
    uint32_t m = data_de(i), agora = hora_utc(NULL);
    if (!m) return GD_SIN;
    uint32_t dif = agora > m ? agora - m : 0, dias = dif / 86400u;
    return dias < 1 ? GD_HOY : dias < 2 ? GD_AYER : dias < 7 ? GD_SEMANA : dias < 30 ? GD_MES : dias < 365 ? GD_ANO : GD_ANTIGUO;
}

/* Compara dos elementos por la columna de orden (las carpetas siempre primero,
 * como las ponía el programa de siempre; al agrupar, primero el grupo). */
static int comparar(int a, int b)
{
    if (agrupar != AGR_NADA && P->grupo[a] != P->grupo[b]) return P->grupo[a] < P->grupo[b] ? -1 : 1;
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

/* Copia todo lo de la fila b en la a. */
static void copiar_fila(int a, int b)
{
    copia_texto(nomes[a], nomes[b], sizeof nomes[a]);
    tipos[a] = tipos[b]; tamanhos[a] = tamanhos[b]; modificadas[a] = modificadas[b];
    tem_data[a] = tem_data[b]; P->grupo[a] = P->grupo[b];
    copia_texto(rotas[a], rotas[b], sizeof rotas[a]);
}

static void trocar(int a, int b)
{
    static char n[64], r[200];
    uint8_t t = tipos[a], g = P->grupo[a];
    uint32_t z = tamanhos[a], d = modificadas[a];
    bool f = tem_data[a];
    copia_texto(n, nomes[a], sizeof n);
    copia_texto(r, rotas[a], sizeof r);
    copiar_fila(a, b);
    copia_texto(nomes[b], n, sizeof nomes[b]);
    tipos[b] = t; tamanhos[b] = z; modificadas[b] = d; tem_data[b] = f; P->grupo[b] = g;
    copia_texto(rotas[b], r, sizeof rotas[b]);
}

static void ordenar(int n)
{
    for (int i = 1; i < n; i++) {
        int k = i;
        while (k > 0 && comparar(k - 1, k) > 0) { trocar(k - 1, k); k--; }
    }
}

/* Con «agrupar» se calcula el grupo de cada elemento (y la fecha si hace falta) antes de ordenar. */
static void preparar_grupos(int n)
{
    for (int i = 0; i < n; i++) {
        if ((agrupar == AGR_DATA || ordem == ORD_DATA) && !tem_data[i]) data_de(i);
        P->grupo[i] = agrupar == AGR_NADA ? 0 : (uint8_t)grupo_de(i);
    }
}

/* Después de ordenar: mete una cabecera (TIPO_GRUPO) delante de cada grupo. Devuelve el nuevo total. */
static int inserir_cabecalhos(int n)
{
    if (agrupar == AGR_NADA || n == 0) return n;
    int extra = 0, ant = -1;
    for (int i = 0; i < n; i++) if (P->grupo[i] != ant) { extra++; ant = P->grupo[i]; }
    int dst = n + extra - 1;
    for (int i = n - 1; i >= 0; i--) {
        copiar_fila(dst--, i);
        bool primero = i == 0 || P->grupo[i - 1] != P->grupo[i];
        if (primero) {
            int g = P->grupo[i];
            copia_texto(nomes[dst], grupo_nome(agrupar, g), sizeof nomes[dst]);
            tipos[dst] = TIPO_GRUPO; tamanhos[dst] = 0; modificadas[dst] = 0; tem_data[dst] = true;
            P->grupo[dst] = (uint8_t)g; rotas[dst][0] = 0;
            dst--;
        }
    }
    return n + extra;
}

static void limpar_marcados(void)
{
    for (int i = 0; i < MAX_FILAS; i++) marcados[i] = false;
    n_marcados = 0;
    ancora = -1;
}

/* ------------------------------------------------------------ dibujo */

/* ---- la barra de órdenes moderna (D423): botones planos con icono, vistas en un control segmentado y la
 * búsqueda a la derecha. Todo viene de la libsagui (gui_moderno.c); aquí solo se colocan las piezas. ---- */
enum { BN_VOLTAR, BN_AVANCAR, BN_SUBIR, BN_NOVA, BN_BORRAR, BN_OCULTOS, BN_N };
#define BARRA_Y 6          /* y de los botones de la primera fila */
#define BARRA_A 28         /* alto de botones y campo */
static mbotao_t bt[BN_N];
static bool bt_vis[BN_N];
static segmentado_t seg_vistas;
static bool seg_vis;
static int sep_x[3], n_sep;      /* separadores entre grupos */
static int busca_x, busca_l;     /* el campo de búsqueda (busca_l = 0: la ventana es demasiado estrecha) */
static bool busca_hover, busca_hover_x;
static dica_t dica;

static void barra_iniciar(void)
{
    seg_vistas.hover = -1;
    dica_iniciar(&dica);
}

static void barra_montar(void)
{
    static const char *const ROT[BN_N] = { NULL, NULL, NULL, N_("Nueva carpeta"), N_("Borrar"), NULL };
    static const char *const IC[BN_N] = { "voltar", "avancar", "subir", "pasta", "excluir", "olho" };
    static const char *const DICA[BN_N] = { N_("Atrás"), N_("Adelante"), N_("Subir a la carpeta superior"), N_("Nueva carpeta"),
                                            N_("Borrar lo seleccionado"), N_("Mostrar u ocultar los archivos ocultos (Ctrl+H)") };
    static const int VAR[BN_N] = { MB_ICONO, MB_ICONO, MB_ICONO, MB_PLANO, MB_PELIGRO, MB_ICONO };
    int x = 8, lim = LARG - 8 - 120;   /* se reservan 120 px para la búsqueda */
    n_sep = 0;
    for (int i = 0; i < BN_N; i++) {
        mbotao_t *b = &bt[i];
        bool hov = b->hover, apr = b->apertado;
        memset(b, 0, sizeof *b);
        b->hover = hov; b->apertado = apr;
        b->variante = VAR[i]; b->icone = IC[i]; b->texto = ROT[i] ? _(ROT[i]) : NULL; b->dica = _(DICA[i]);
        b->y = BARRA_Y; b->a = BARRA_A;
        b->ativo = i == BN_OCULTOS && ver_ocultos;
        mbotao_ajustar(b);
        if (i == BN_NOVA && bt_vis[BN_SUBIR]) { sep_x[n_sep++] = x + 2; x += 8; }   /* navegación | acciones */
        bt_vis[i] = x + b->l <= (i < BN_NOVA ? LARG - 8 : lim);
        b->x = x;
        if (bt_vis[i]) x += b->l + 4;
    }
    static const char *const SI[4] = { "lista", "tabela", "quadro", "imagem" };
    static const char *const SD[4] = { N_("Vista de lista"), N_("Vista de detalles"), N_("Vista de iconos"), N_("Vista de mosaicos") };
    seg_vistas.n = 4; seg_vistas.sel = vista; seg_vistas.y = BARRA_Y; seg_vistas.a = BARRA_A;
    for (int k = 0; k < 4; k++) { seg_vistas.icones[k] = SI[k]; seg_vistas.dicas[k] = _(SD[k]); }
    segmentado_ajustar(&seg_vistas);
    seg_vis = x + 8 + seg_vistas.l <= lim;
    if (seg_vis) { sep_x[n_sep++] = x + 2; x += 8; seg_vistas.x = x; x += seg_vistas.l + 10; }
    busca_x = x;
    busca_l = LARG - 8 - x;
    if (busca_l > 190) { busca_x = LARG - 8 - 190; busca_l = 190; }
    if (busca_l < 90) busca_l = 0;
    static int ult_firma;
    int firma = LARG * 7 + x;
    if (firma != ult_firma) {   /* para las pruebas: dónde quedan los botones (x, ancho) */
        ult_firma = firma;
        registrar("arquivos: barra voltar=%d,%d avancar=%d,%d subir=%d,%d nova=%d,%d borrar=%d,%d ocultos=%d,%d vistas=%d,%d busca=%d,%d y=%d a=%d",
                  bt[0].x, bt[0].l, bt[1].x, bt[1].l, bt[2].x, bt[2].l, bt[3].x, bt[3].l, bt[4].x, bt[4].l, bt[5].x, bt[5].l,
                  seg_vistas.x, seg_vistas.l, busca_x, busca_l, BARRA_Y, BARRA_A);
    }
}

/* El campo de búsqueda: el de la ruta en modo búsqueda cuando se está editando; si no, lo que se busca o filtra. */
static void busca_caixa(caixa_t *v)
{
    bool edita = caixa.foco && modo_caixa == MODO_BUSCA;
    if (edita) *v = caixa;
    else { painel_t *pa = &paineis[ativo]; memset(v, 0, sizeof *v); copia_texto(v->texto, pa->s_em_busca ? pa->s_padrao_busca : pa->s_filtro, sizeof v->texto); }
    v->x = busca_x; v->y = BARRA_Y; v->l = busca_l;
    if (edita) { caixa.x = busca_x; caixa.y = BARRA_Y; caixa.l = busca_l; }
}

static void desenhar_busca_barra(void)
{
    if (busca_l <= 0) return;
    caixa_t v;
    busca_caixa(&v);
    campo_desenhar(j, &v, BARRA_A, _("Buscar"), CAMPO_LUPA | CAMPO_LIMPAR | (busca_hover ? CAMPO_HOVER : 0) | (busca_hover_x ? CAMPO_HOVER_X : 0), 0);
}

static void desenhar_barra(void)
{
    g_retangulo(j, 0, 0, LARG, ALT_BARRA, mod_tema.barra);
    barra_montar();
    for (int k = 0; k < n_sep; k++) mod_separador(j, sep_x[k], BARRA_Y + 4, BARRA_A - 8);
    for (int i = 0; i < BN_N; i++) if (bt_vis[i]) mbotao_desenhar(j, &bt[i]);
    if (seg_vis) segmentado_desenhar(j, &seg_vistas);
    desenhar_busca_barra();
    jan_mostrar_area(j, 0, 0, LARG, ALT_BARRA);
}

/* ---- la barra de órdenes (D420): botones solo con icono; el nombre sale en la barra de estado ---- */

enum { OR_NUEVO, OR_CORTAR, OR_COPIAR, OR_PEGAR, OR_RENOMBRAR, OR_COMPRIMIR, OR_EXTRAER, OR_TERMINAL, OR_PROPS,
       OR_VER, OR_DOBLE, OR_PREVIA, OR_PESTANA, OR_INICIO, OR_FAV, OR_ETIQUETA, OR_N };
static const struct { const char *icone; const char *texto; } ORDENS[OR_N] = {
    { "documento-novo", N_("Nuevo (carpeta o archivo)") }, { "editar", N_("Cortar (Ctrl+X)") }, { "copiar", N_("Copiar (Ctrl+C)") },
    { "colar", N_("Pegar (Ctrl+V)") }, { "documento-editar", N_("Renombrar (F2)") }, { "cofre", N_("Comprimir en .zip") },
    { "abrir", N_("Extraer aquí") }, { "terminal", N_("Abrir terminal aquí") }, { "informacao", N_("Propiedades") },
    { "tabela", N_("Ver: vistas, agrupar y ordenar") }, { "quadro", N_("Panel doble (F3)") }, { "olho", N_("Vista previa (Alt+P)") },
    { "janela", N_("Pestaña nueva (Ctrl+T)") }, { "inicio", N_("Inicio (Alt+Inicio)") }, { "estrela", N_("Anclar a Favoritos (Ctrl+D)") },
    { "marcador", N_("Etiqueta de color") },
};
static int ordens_x[OR_N];   /* x de cada botón (-1 si no cabe) */
#define ORDEN_L 26
static bool git_ha(void);
static const char *git_ramo(void);

static mbotao_t ob[OR_N];   /* los botones de la fila de órdenes (planos, solo icono) */

static bool orden_ativa(int k) { return (k == OR_DOBLE && n_paineis > 1) || (k == OR_PREVIA && previa); }

static void desenhar_orden(int k)
{
    if (ordens_x[k] < 0) return;
    mbotao_t *b = &ob[k];
    b->x = ordens_x[k]; b->y = ALT_BARRA + 2; b->l = ORDEN_L; b->a = ALT_ORDENS - 5;
    b->variante = MB_ICONO; b->icone = ORDENS[k].icone; b->texto = NULL;
    b->ativo = orden_ativa(k);
    mbotao_desenhar(j, b);
}

static void desenhar_ordens(void)
{
    g_retangulo(j, 0, ALT_BARRA, LARG, ALT_ORDENS, mod_tema.barra);
    g_retangulo(j, 0, ALT_BARRA + ALT_ORDENS - 1, LARG, 1, mod_tema.borde);
    int x = 6;
    for (int k = 0; k < OR_N; k++) {
        if (k == OR_VER || k == OR_PESTANA) {   /* un hueco entre grupos, con su separador fino */
            x += 8;
            if (x - 5 + 1 < LARG) mod_separador(j, x - 5, ALT_BARRA + 6, ALT_ORDENS - 13);
        }
        ordens_x[k] = x + ORDEN_L > LARG ? -1 : x;
        if (ordens_x[k] >= 0) desenhar_orden(k);
        x += ORDEN_L + 2;
    }
    if (git_ha()) {   /* la rama de git, a la derecha, como una etiqueta */
        char t[64];
        formatar_texto(t, sizeof t, "git: %s", git_ramo());
        int w = g_largura_texto(t);
        if (x + w + 24 < LARG) {
            g_retangulo_red(j, LARG - w - 22, ALT_BARRA + 3, w + 16, ALT_ORDENS - 8, 9, mod_tema.acento_suave);
            g_texto(j, LARG - w - 14, ALT_BARRA + (ALT_ORDENS - LETRA_A) / 2 - 1, t, mod_tema.acento);
        }
    }
    jan_mostrar_area(j, 0, ALT_BARRA, LARG, ALT_ORDENS);
}

/* El botón de la barra de órdenes bajo el punto, o -1. */
static int ordem_em(int x, int y)
{
    if (y < ALT_BARRA || y >= ALT_BARRA + ALT_ORDENS) return -1;
    for (int k = 0; k < OR_N; k++)
        if (ordens_x[k] >= 0 && x >= ordens_x[k] && x < ordens_x[k] + ORDEN_L) return k;
    return -1;
}

/* ---- la barra lateral: Inicio, Favoritos, Unidades y Etiquetas ---- */

enum { LT_SEC, LT_ITEM, LT_UNID, LT_ETQ };
enum { LUG_INICIO, LUG_FAV, LUG_UNID, LUG_PEN, LUG_CD, LUG_LIXEIRA, LUG_ETQ };
typedef struct {
    int tipo, acao;
    const char *icone;
    char texto[40];
    char cam[160];
    int cor, pct;
    int y, a;           /* posición dentro de la barra, sin desplazar */
} lado_t;
static lado_t lado[MAX_LADO];
static int qtd_lado;
static int lado_rolo;             /* desplazamiento de la barra lateral */
static int lado_alto_total;
static int lado_destaque = -1;    /* fila destino de un arrastre */
static int lado_hover = -1;       /* fila con el ratón encima (D423) */
static int lado_fav_menu = -1;    /* favorito sobre el que se abrió el menú contextual */

static lado_t *lado_novo(int tipo, int acao, const char *icone, const char *texto, const char *cam, int a)
{
    if (qtd_lado >= MAX_LADO) return NULL;
    lado_t *l = &lado[qtd_lado++];
    l->tipo = tipo; l->acao = acao; l->icone = icone; l->cor = 0; l->pct = -1;
    copia_texto(l->texto, texto, sizeof l->texto);
    copia_texto(l->cam, cam ? cam : "", sizeof l->cam);
    l->a = a;
    return l;
}

static void construir_lateral(void)
{
    qtd_lado = 0;
    lado_novo(LT_ITEM, LUG_INICIO, "inicio", _("Inicio"), INICIO_RUTA, 24);
    lado_novo(LT_SEC, 0, NULL, _("FAVORITOS"), NULL, 20);
    for (int i = 0; i < n_fav; i++) {
        const char *n = compara_texto(fav[i], "/") == 0 ? _("Sistema") : nome_base(fav[i]);
        const char *ic = compara_texto(fav[i], "/") == 0 ? "disco" : compara_texto(fav[i], "/home") == 0 ? "casa" : "pasta";
        lado_t *l = lado_novo(LT_ITEM, LUG_FAV, ic, n, fav[i], 24);
        if (l) l->cor = i;   /* el índice en fav[] */
    }
    lado_novo(LT_SEC, 0, NULL, _("ETIQUETAS"), NULL, 20);
    lado_novo(LT_ETQ, LUG_ETQ, NULL, "etq", NULL, 26);   /* una fila con los seis colores */
    lado_novo(LT_SEC, 0, NULL, _("UNIDADES"), NULL, 20);
    info_montagem_t mont[16];
    int nm = info_sistema(INFO_MONTAGENS, mont, 16);
    for (int i = 0; i < nm; i++) {
        if (!mont[i].ponto[0] || !mont[i].total_kb) continue;
        const char *n = compara_texto(mont[i].ponto, "/") == 0 ? _("Sistema") : nome_base(mont[i].ponto);
        lado_t *l = lado_novo(LT_UNID, LUG_UNID, "disco", n, mont[i].ponto, 34);
        if (!l) break;
        unsigned usados = mont[i].total_kb - (mont[i].livre_kb < mont[i].total_kb ? mont[i].livre_kb : mont[i].total_kb);
        l->pct = (int)((unsigned long long)usados * 100u / mont[i].total_kb);
        l->cor = (mont[i].opcoes & MONTAR_SO_LEITURA) != 0;   /* D446: candado de solo lectura (XFS, ext4 -o ro, CD) */
    }
    if (tem_pendrive)
        lado_novo(LT_ITEM, LUG_PEN, "disco", pendrive.montado ? _("Retirar") : pendrive.rotulo[0] ? pendrive.rotulo : _("Pendrive"), NULL, 24);
    if (tem_cd) {   /* D233: abre el CD/DVD; dentro de él, el mismo botón lo expulsa */
        bool dentro = cdrom.montado && cdrom.ponto[0] && comeca_com(pasta, cdrom.ponto);
        lado_novo(LT_ITEM, LUG_CD, dentro ? "excluir" : "disco", dentro ? _("Expulsar") : cdrom.rotulo[0] ? cdrom.rotulo : "CD/DVD", NULL, 24);
    }
    if (existe_pasta("/home/.papelera")) lado_novo(LT_ITEM, LUG_LIXEIRA, "excluir", _("Lixeira"), "/home/.papelera", 24);
    int y = 6;
    for (int i = 0; i < qtd_lado; i++) { lado[i].y = y; y += lado[i].a; }
    lado_alto_total = y + 4;
    static int ult_firma;
    int firma = lado_alto_total * 31 + qtd_lado;
    if (firma != ult_firma) {   /* para las pruebas: dónde quedan las secciones (y de la barra, sin desplazar) */
        ult_firma = firma;
        for (int i = 0; i < qtd_lado; i++)
            if (lado[i].tipo == LT_SEC || lado[i].tipo == LT_ETQ) registrar("arquivos: lateral %s y=%d", lado[i].texto, lado[i].y + ALT_BARRA + ALT_ORDENS);
    }
}

static bool lado_atual(const lado_t *l)
{
    if (l->acao == LUG_INICIO) return compara_texto(pasta, INICIO_RUTA) == 0;
    if (l->acao == LUG_FAV || l->acao == LUG_LIXEIRA || l->acao == LUG_UNID) return compara_texto(pasta, l->cam) == 0;
    if (l->acao == LUG_ETQ) return false;
    return false;
}

static void desenhar_lateral(void)
{
    int h = ALT - ALT_BARRA - ALT_ORDENS - ALT_STATUS, y0 = ALT_BARRA + ALT_ORDENS;
    const mod_tema_t *t = &mod_tema;
    uint32_t fundo = mod_mezclar(t->barra, t->pista, 90);
    g_retangulo(j, 0, y0, LATERAL, h, fundo);
    g_retangulo(j, LATERAL - 1, y0, 1, h, t->borde);
    construir_lateral();
    int max = lado_alto_total - h;
    if (max < 0) max = 0;
    if (lado_rolo > max) lado_rolo = max;
    if (lado_rolo < 0) lado_rolo = 0;
    for (int i = 0; i < qtd_lado; i++) {
        lado_t *l = &lado[i];
        int y = y0 + l->y - lado_rolo;
        if (y < y0 || y + l->a > y0 + h) continue;
        if (l->tipo == LT_SEC) { rotulo(j, 10, y + 2, l->texto, t->texto_fraco); continue; }
        bool at = lado_atual(l);
        int fx = 4, fl = LATERAL - 9;   /* la fila: un rectángulo redondeado */
        if (i == lado_destaque) { g_retangulo_red(j, fx, y, fl, l->a - 1, t->radio, t->acento_suave); g_aro_red(j, fx, y, fl, l->a - 1, t->radio, 2, t->acento); }
        else if (at) {
            g_retangulo_red(j, fx, y, fl, l->a - 1, t->radio, t->acento_suave);
            g_retangulo_red(j, fx, y + (l->a - 1 - 14) / 2, 3, 14, 1, t->acento);   /* la marca del elemento actual */
        } else if (i == lado_hover && l->tipo != LT_ETQ) g_retangulo_red(j, fx, y, fl, l->a - 1, t->radio, t->hover);
        if (l->tipo == LT_ETQ) {
            for (int c = 0; c < N_CORES; c++) {   /* un punto de color por etiqueta (el activo con aro) */
                bool act = em_busca && comeca_com(padrao_busca, "@etq") && padrao_busca[4] - '0' == c;
                g_retangulo_red(j, 10 + c * 20, y + 4, 16, 16, 8, COR_ETQ[c]);
                if (act) { g_aro_red(j, 8 + c * 20, y + 2, 20, 20, 10, 2, t->texto); }
                else g_aro_red(j, 10 + c * 20, y + 4, 16, 16, 8, 1, mod_mezclar(COR_ETQ[c], 0x000000, 70));
            }
        } else {
            icone_desenhar(j, 9, y + (l->tipo == LT_UNID ? 2 : 4), l->icone, 1);
            bool cadeado = l->tipo == LT_UNID && l->cor;   /* D446: unidad de solo lectura */
            g_texto_n(j, 29, y + (l->tipo == LT_UNID ? 1 : 4), l->texto, (LATERAL - (cadeado ? 56 : 36)) / LETRA_L, t->texto);
            if (cadeado) icone_desenhar(j, LATERAL - 28, y + 1, "cadeado", 1);
            if (l->tipo == LT_UNID && l->pct >= 0) barra_uso_desenhar(j, 29, y + 22, LATERAL - 42, 6, l->pct);
        }
    }
    if (max > 0) {   /* hay más de lo que cabe: una barra delgada */
        int pol = h * h / lado_alto_total;
        if (pol < 10) pol = 10;
        g_retangulo_red(j, LATERAL - 5, y0 + (h - pol) * lado_rolo / max, 3, pol, 1, t->borde_forte);
    }
    jan_mostrar_area(j, 0, y0, LATERAL, h);
}

/* La fila de la barra lateral bajo el punto (-1 = ninguna). */
static int lado_em(int x, int y)
{
    int y0 = ALT_BARRA + ALT_ORDENS, h = ALT - y0 - ALT_STATUS;
    if (x < 0 || x >= LATERAL - 1 || y < y0 || y >= y0 + h) return -1;
    for (int i = 0; i < qtd_lado; i++) {
        int yy = y0 + lado[i].y - lado_rolo;
        if (y >= yy && y < yy + lado[i].a) return i;
    }
    return -1;
}

/* ---- disposición: cada panel ocupa su franja; encima de la lista van las pestañas (si hay más de una),
 * la barra de ruta y, en la vista de detalles, el encabezado de columnas ---- */

#define GRELHA (vista == VISTA_ICONES || vista == VISTA_MOSAICOS)
static int celula_l(void) { return vista == VISTA_MOSAICOS ? LARG_MOSAICO : LARG_CELULA; }
static int celula_a(void) { return vista == VISTA_MOSAICOS ? ALT_MOSAICO : ALT_CELULA; }

static void layout(void)
{
    int area_l = LARG - LATERAL - (previa ? LARG_PREVIA : 0);
    if (area_l < 160) area_l = 160;
    for (int k = 0; k < n_paineis; k++) {
        painel_t *p = &paineis[k];
        p->l = n_paineis == 1 ? area_l : (area_l - 4) / 2;
        p->x = LATERAL + (k == 0 ? 0 : p->l + 4);
        int y = ALT_BARRA + ALT_ORDENS;
        p->barra_abas.x = p->x + 4; p->barra_abas.l = p->l - 8; p->barra_abas.y = y;
        if (p->n_abas > 1) y += ABAS_ALTURA;
        p->y_rota = y;
        p->migas.x = p->x + 6; p->migas.y = y + 4; p->migas.l = p->l - 14; p->migas.a = ALT_ROTA - 8;
        p->s_lista.x = p->x + 6;
        p->s_lista.l = p->l - 14;
        p->s_lista.y = y + ALT_ROTA + 4 + (p->s_vista == VISTA_DETALHES ? 18 : 0);
        p->s_lista.a = ALT - p->s_lista.y - 30;
    }
}

static void usar_painel(int k) { ativo = k; P = &paineis[k]; }

/* Texto del nombre de una pestaña. */
static void nome_da_aba(const char *caminho, char *s, size_t max)
{
    if (compara_texto(caminho, INICIO_RUTA) == 0) copia_texto(s, _("Inicio"), max);
    else if (compara_texto(caminho, "/") == 0) copia_texto(s, _("Sistema"), max);
    else copia_texto(s, nome_base(caminho), max);
}

static void desenhar_abas_painel(painel_t *p)
{
    if (p->n_abas <= 1) return;
    p->barra_abas.n = p->n_abas;
    p->barra_abas.sel = p->aba_sel;
    for (int i = 0; i < p->n_abas; i++)
        nome_da_aba(i == p->aba_sel ? p->s_pasta : p->abas[i].a_pasta, p->barra_abas.titulos[i], sizeof p->barra_abas.titulos[0]);
    abas_desenhar(j, &p->barra_abas);
}

/* La barra de ruta: migas de pan con flecha (o el cuadro de edición / búsqueda en el panel activo). */
static int filtro_bx;   /* x del botón «Filtros» de la búsqueda (0 = no se ve) */
static bool busca_em_curso(void);
static int filtros_ativos(void);

/* El dibujo del cuadro de texto compartido (caixa): la ruta en la banda del panel o la búsqueda en la barra. */
static void desenhar_rota_painel(painel_t *p);
static void caixa_gancho(jan_t *jj, caixa_t *c)
{
    (void)jj; (void)c;
    if (modo_caixa == MODO_BUSCA) { desenhar_rota_painel(P); desenhar_busca_barra(); }
    else desenhar_rota_painel(P);
}

static void rota_painel_corpo(painel_t *p)
{
    int x0 = p->x + 6, l = p->l - 14;
    if (l < 40) l = 40;
    g_retangulo(j, x0 - 2, p->y_rota, l + 4, ALT_ROTA + 2, mod_tema.barra);   /* la banda entera */
    bool ativo_p = p == &paineis[ativo];
    if (ativo_p && caixa.foco && modo_caixa == MODO_ROTA) {   /* se está editando la ruta */
        caixa.x = x0; caixa.y = p->y_rota + 1; caixa.l = l;
        campo_desenhar(j, &caixa, ALT_ROTA - 2, NULL, CAMPO_RECTO, mod_tema.barra);
        filtro_bx = 0;
        jan_mostrar_area(j, x0 - 2, p->y_rota, l + 4, ALT_ROTA + 2);
        return;
    }
    if (ativo_p) filtro_bx = 0;
    p->migas.foco = n_paineis > 1 && ativo_p;
    if (compara_texto(p->s_pasta, INICIO_RUTA) == 0) {
        g_caixa_red(j, x0, p->y_rota + 1, l, ALT_ROTA - 2, mod_tema.radio, mod_tema.campo, p->migas.foco ? mod_tema.acento : mod_tema.borde);
        icone_desenhar(j, x0 + 8, p->y_rota + 5, "inicio", 1);
        g_texto(j, x0 + 28, p->y_rota + 5, _("Inicio"), mod_tema.texto);
        p->migas.n = 0;
        jan_mostrar_area(j, x0 - 2, p->y_rota, l + 4, ALT_ROTA + 2);
        return;
    }
    migas_desenhar(j, &p->migas, p->s_pasta);
    if (ativo_p && caixa.foco && modo_caixa == MODO_BUSCA) {   /* «Filtros»: tipo, tamaño y fecha */
        char t[32];
        int nf = filtros_ativos();
        if (nf) formatar_texto(t, sizeof t, "%s (%d)", _("Filtros"), nf); else copia_texto(t, _("Filtros"), sizeof t);
        filtro_bx = x0 + l - 82;
        mbotao_t fb;
        memset(&fb, 0, sizeof fb);
        fb.x = filtro_bx; fb.y = p->y_rota + 3; fb.l = 78; fb.a = ALT_ROTA - 6; fb.variante = MB_SECUNDARIO; fb.texto = t; fb.fundo = mod_tema.campo;
        fb.apertado = false;
        mbotao_desenhar(j, &fb);
    } else if (p->s_em_busca || busca_em_curso()) {   /* aviso de búsqueda: se sale con Esc */
        char t[80];
        formatar_texto(t, sizeof t, _("Búsqueda: %s"), p->s_padrao_busca);
        int w = g_largura_texto(t);
        if (w + 20 < p->migas.l) {
            int bx = p->migas.x + p->migas.l - w - 16;
            g_retangulo_red(j, bx, p->migas.y - 1, w + 12, p->migas.a + 2, 8, mod_tema.acento_suave);
            g_texto(j, bx + 6, p->migas.y, t, mod_tema.acento);
        }
    }
}

static void desenhar_rota_painel(painel_t *p)
{
    rota_painel_corpo(p);
    if (p == &paineis[ativo]) desenhar_busca_barra();
}

/* La carpeta se comprueba con los migas de la ruta actual; sin estado propio. */

/* ---- columnas de la vista de detalles ---- */
static int col_tam, col_tipo, col_data;

static void preparar_colunas(void)
{
    if (!colunas.larg[1]) {
        colunas.larg[1] = 90; colunas.larg[2] = 80; colunas.larg[3] = 160;
        colunas.direita[1] = true;
    }
    /* en un panel estrecho (doble panel) solo caben las primeras columnas */
    colunas.n = lista.l >= 440 ? 4 : lista.l >= 330 ? 3 : 2;
    if (lista.l < 330 && colunas.larg[1] > 80) colunas.larg[1] = 80;
    colunas.titulos[0] = _("Nombre"); colunas.titulos[1] = _("Tamaño"); colunas.titulos[2] = _("Tipo"); colunas.titulos[3] = _("Fecha");
    colunas.x = lista.x; colunas.l = lista.l; colunas.y = lista.y - 18;
    colunas.ord = ordem; colunas.desc = ordem_desc;
    col_tam = (colunas.n > 2 ? colunas_x(&colunas, 2) : lista.x + lista.l) - 10;   /* borde derecho del texto de «Tamaño» */
    col_tipo = colunas.n > 2 ? colunas_x(&colunas, 2) : -1;
    col_data = colunas.n > 3 ? colunas_x(&colunas, 3) : -1;
}

static void desenhar_cabecalho(void)
{
    preparar_colunas();
    colunas_desenhar(j, &colunas);
}

/* ---- los pequeños adornos de cada elemento: etiqueta de color y estado de git ---- */
static char git_marca(int i);
static bool miniatura_desenhar(int i, int x, int y, int l, int a);

static void adornos(int i, int x, int y)
{
    if (tipos[i] == TIPO_GRUPO) return;
    char c[200];
    caminho_de(i, c, sizeof c);
    int e = etiqueta_de(c);
    if (e >= 0) { g_retangulo(j, x, y, 8, 8, COR_ETQ[e]); g_moldura(j, x, y, 8, 8, 0x404850); }
    char g = git_marca(i);
    if (g) {
        uint32_t cor = g == '?' ? 0x2FA84F : g == 'D' ? 0xC03030 : 0xC07010;
        g_texto_n(j, x + 11, y - 4, &g, 1, cor);
    }
}

/* Una fila de la vista de lista (icono, nombre, tamaño, tipo y fecha). */
static void desenhar_linha(int i, int y, bool sel, bool atual)
{
    int x0 = lista.x;
    if (tipos[i] == TIPO_GRUPO) {   /* cabecera de grupo */
        g_retangulo(j, x0 + 2, y, lista.l - 4, LISTA_ALTURA_ITEM, sagui_esq.ar_cab);
        g_retangulo(j, x0 + 2, y + LISTA_ALTURA_ITEM - 1, lista.l - 4, 1, sagui_esq.ar_linha);
        g_texto_n(j, x0 + 8, y + 2, nomes[i], (lista.l - 20) / LETRA_L, COR_DESTAQUE);
        return;
    }
    if (sel) g_retangulo(j, x0 + 2, y, lista.l - 4, LISTA_ALTURA_ITEM, atual ? sagui_esq.ar_atual_sel : COR_SELECAO);
    else if (atual) g_retangulo(j, x0 + 2, y, lista.l - 4, LISTA_ALTURA_ITEM, sagui_esq.ar_atual);
    uint32_t cor = sel ? COR_SEL_TEXTO : COR_TEXTO;
    icone_item(x0 + 6, y + 2, i, 1);
    int ancho_nome = (vista == VISTA_DETALHES ? colunas_x(&colunas, 1) - 24 - x0 - 4 : lista.l - 34);
    if (ancho_nome < 24) ancho_nome = 24;
    g_texto_n(j, x0 + 28, y + 2, nomes[i], ancho_nome / LETRA_L, cor);
    int nl = (int)tam_texto(nomes[i]) * 8;
    adornos(i, x0 + 28 + (nl > ancho_nome ? ancho_nome : nl) + 4, y + 7);
    char t[32];
    if (tipos[i] == TIPO_PASTA) copia_texto(t, "-", sizeof t);
    else formatar_texto(t, sizeof t, "%u", tamanhos[i]);
    int tl = (int)tam_texto(t);
    g_texto(j, col_tam - tl * 8, y + 2, t, cor);
    if (col_tipo >= 0) g_texto(j, col_tipo, y + 2, tipo_texto(i), cor);
    if (col_data >= 0) {
        data_texto(i, t, sizeof t);
        g_texto_n(j, col_data, y + 2, t, (lista.l - (col_data - x0) - 6) / LETRA_L, cor);
    }
}

/* ------------------------------------------------------------ grelha de icones */

/* Desenhos que a grelha usa (estao mais abaixo no ficheiro). */
static void desenhar_lista(void);
static const char *vista_nome(void);
static void desenhar_rodape(const char *msg);
static void info_item(int i, char *msg, size_t max);
static void previa_atualizar(void);

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
    int c = (lista.l - 12) / celula_l();
    return c < 1 ? 1 : c;
}

static int grade_linhas(void)
{
    int l = (lista.a - 6) / celula_a();
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
    previa_atualizar();
    char msg[200];
    info_item(sel, msg, sizeof msg);
    desenhar_rodape(msg);
    return true;
}

/* Un archivo en una celda de la vista de iconos o de mosaicos. */
static void desenhar_celula(int i, int cx, int cy)
{
    bool sel = marcados[i];
    uint32_t cor = sel ? COR_SEL_TEXTO : COR_TEXTO;
    int cl = celula_l() - 4, ca = celula_a() - 6;
    if (sel) g_retangulo(j, cx, cy, cl, ca, COR_SELECAO);
    else if (i == lista.sel) g_retangulo(j, cx, cy, cl, ca, sagui_esq.ar_atual);
    if (vista == VISTA_MOSAICOS) {
        if (!miniatura_desenhar(i, cx + 4, cy + 4, 40, 40)) icone_item(cx + 8, cy + 8, i, 2);
        g_texto_n(j, cx + 50, cy + 5, nomes[i], (cl - 54) / LETRA_L, cor);
        char t[40];
        if (tipos[i] == TIPO_PASTA) copia_texto(t, tipo_texto(i), sizeof t);
        else formatar_texto(t, sizeof t, "%s, %u B", tipo_texto(i), tamanhos[i]);
        g_texto_n(j, cx + 50, cy + 25, t, (cl - 54) / LETRA_L, sel ? COR_SEL_TEXTO : COR_TEXTO_FRACO);
        adornos(i, cx + cl - 14, cy + ca - 12);
    } else {
        if (!miniatura_desenhar(i, cx + 16, cy + 3, 56, 40)) icone_item(cx + 28, cy + 4, i, 2);
        g_texto_n(j, cx + 2, cy + 46, nomes[i], (cl - 4) / LETRA_L, cor);
        adornos(i, cx + cl - 12, cy + 4);
    }
}

/* La página de Inicio (carpeta virtual): favoritos, unidades y recientes como tarjetas pulsables. */
/* Una tarjeta pulsable de la página de Inicio: se anota para los clics y los resaltes. */
static int alvo_nova(int x, int y, int l, int a, const char *cam)
{
    if (P->n_alvos >= ALVOS_MAX) return -1;
    int k = P->n_alvos++;
    P->alvos[k].x = x; P->alvos[k].y = y; P->alvos[k].l = l; P->alvos[k].a = a;
    copia_texto(P->alvos[k].cam, cam, sizeof P->alvos[0].cam);
    return k;
}

/* La página de Inicio (D423): tres secciones sobre un fondo claro; favoritos y unidades son tarjetas con sombra,
 * y lo reciente es una tarjeta con sus filas. */
#define INICIO_FUNDO (sagui_esq.ar_inicio)
static void desenhar_inicio(void)
{
    const mod_tema_t *t = &mod_tema;
    int x0 = lista.x, y0 = lista.y, l = lista.l;
    g_retangulo(j, x0, y0, l, lista.a, INICIO_FUNDO);
    P->n_alvos = 0;
    int y = y0 + 8, fim = y0 + lista.a;
    int util = l - 24;   /* de x0 + 12 a x0 + l - 12 */
    rotulo(j, x0 + 12, y, _("Favoritos"), t->acento);
    y += 24;
    int cols = (util + 12) / (136 + 12);
    if (cols < 1) cols = 1;
    int w = (util - (cols - 1) * 12) / cols, col = 0;
    for (int i = 0; i < n_fav; i++) {
        if (y + 34 > fim) break;
        int x = x0 + 12 + col * (w + 12);
        bool raiz = compara_texto(fav[i], "/") == 0;
        int k = alvo_nova(x, y, w, 32, fav[i]);
        cartao_t c = { x, y, w, 32, raiz ? _("Sistema") : nome_base(fav[i]), raiz ? "disco" : "pasta", k >= 0 && P->alvo_h == k + 1, false, INICIO_FUNDO };
        cartao_desenhar(j, &c);
        if (++col >= cols) { col = 0; y += 42; }
    }
    if (col) y += 42;
    y += 6;
    info_montagem_t mont[16];
    int nm = info_sistema(INFO_MONTAGENS, mont, 16);
    int nu = 0;
    for (int i = 0; i < nm; i++) if (mont[i].ponto[0] && mont[i].total_kb) nu++;
    if (nu && y + 24 + 66 < fim) {
        rotulo(j, x0 + 12, y, _("Unidades"), t->acento);
        y += 24;
        int ucols = (util + 12) / (176 + 12);
        if (ucols < 1) ucols = 1;
        int uw = (util - (ucols - 1) * 12) / ucols;
        col = 0;
        for (int i = 0; i < nm; i++) {
            if (!mont[i].ponto[0] || !mont[i].total_kb) continue;
            if (y + 66 > fim) break;
            int x = x0 + 12 + col * (uw + 12);
            int k = alvo_nova(x, y, uw, 62, mont[i].ponto);
            cartao_t c = { x, y, uw, 62, compara_texto(mont[i].ponto, "/") == 0 ? _("Sistema") : nome_base(mont[i].ponto), "disco", k >= 0 && P->alvo_h == k + 1, false, INICIO_FUNDO };
            cartao_desenhar(j, &c);
            unsigned usados = mont[i].total_kb - (mont[i].livre_kb < mont[i].total_kb ? mont[i].livre_kb : mont[i].total_kb);
            int ix, iy, il, ia;
            cartao_interior(&c, &ix, &iy, &il, &ia);
            barra_uso_desenhar(j, ix, iy + 1, il, 8, (int)((unsigned long long)usados * 100u / mont[i].total_kb));
            char t1[24], t2[48];
            tamanho_kb(mont[i].livre_kb, t1, sizeof t1);
            formatar_texto(t2, sizeof t2, _("%s libres"), t1);
            g_texto_n(j, ix, iy + 12, t2, il / LETRA_L, t->texto_fraco);
            if (++col >= ucols) { col = 0; y += 72; }
        }
        if (col) y += 72;
        y += 6;
    }
    if (y + 52 < fim) {
        rotulo(j, x0 + 12, y, _("Recientes"), t->acento);
        y += 24;
        int filas = n_reciente;
        int caben = (fim - y - 20 - 8) / 20;
        if (filas > caben) filas = caben;
        int ca = 20 + (filas > 0 ? filas * 20 : 20);
        cartao_t c = { x0 + 12, y, util, ca, NULL, NULL, false, false, INICIO_FUNDO };
        cartao_desenhar(j, &c);
        int ix, iy, il, ia;
        cartao_interior(&c, &ix, &iy, &il, &ia);
        if (n_reciente == 0) g_texto(j, ix, iy + 2, _("(todavía nada: lo que abras aparecerá aquí)"), t->texto_fraco);
        for (int i = 0; i < filas; i++) {
            int fy = iy + i * 20;
            info_arquivo_t inf;
            bool pasta_r = info_arquivo(reciente[i], &inf) == 0 && inf.tipo == TIPO_PASTA;
            int k = alvo_nova(x0 + 14, fy, util - 4, 20, reciente[i]);
            if (k >= 0 && P->alvo_h == k + 1) g_retangulo_red(j, x0 + 15, fy, util - 6, 20, t->radio, t->acento_suave);
            icone_desenhar(j, ix, fy + 2, pasta_r ? "pasta" : "documento", 1);
            g_texto_n(j, ix + 22, fy + 2, nome_base(reciente[i]), 18, t->texto);
            int pl = (il - 22 - 150) / LETRA_L;
            if (pl > 4) g_texto_n(j, ix + 22 + 150, fy + 2, reciente[i], pl, t->texto_fraco);
        }
    }
}

static int dnd_painel_alvo = -1;   /* panel que recibe lo que se arrastra (-1 = ninguno) */

static void desenhar_lista(void)
{
    static int ult_topo[2] = { -1, -1 }, ult_vista[2] = { -1, -1 };
    if (P == &paineis[ativo] && seg_vistas.sel != vista) {   /* la vista cambió (menú, atajo): el selector la sigue */
        seg_vistas.sel = vista;
        if (seg_vis) segmentado_desenhar(j, &seg_vistas);
    }
    int pk = (int)(P - paineis);
    if (lista.qtd > 0) {   /* topo siempre dentro de los limites (redimensionar o filtrar lo pueden dejar fuera) */
        if (lista.topo >= lista.qtd) lista.topo = lista.qtd - 1;
        if (lista.topo < 0) lista.topo = 0;
    } else lista.topo = 0;
    /* En la rejilla, topo siempre al inicio de una fila: AvPag, redimensionar o la lista
     * comun lo pueden dejar a media fila y la rejilla se correria de lado (D139, D217). */
    if (GRELHA) {
        int c = grade_colunas();
        if (c > 0) lista.topo = (lista.topo / c) * c;
    }
    if (lista.topo != ult_topo[pk] || vista != ult_vista[pk]) {
        registrar("arquivos: lista desenhada vista=%s topo=%d", vista_nome(), lista.topo);
        ult_topo[pk] = lista.topo; ult_vista[pk] = vista;
    }
    int x0 = lista.x, l = lista.l, alt = lista.a;
    int topo = vista == VISTA_DETALHES ? lista.y - 18 : lista.y;   /* el encabezado va arriba */
    g_retangulo(j, x0, lista.y, l, alt, COR_CAMPO);
    g_moldura(j, x0, lista.y, l, alt, dnd_painel_alvo == pk ? COR_SELECAO : COR_BORDA);
    if (dnd_painel_alvo == pk) g_moldura(j, x0 + 1, lista.y + 1, l - 2, alt - 2, COR_SELECAO);
    if (compara_texto(pasta, INICIO_RUTA) == 0) {
        desenhar_inicio();
        jan_mostrar_area(j, x0, topo, l, lista.y + alt - topo);
        return;
    }
    if (vista == VISTA_DETALHES) desenhar_cabecalho();
    else preparar_colunas();
    int vis = (alt - 4) / LISTA_ALTURA_ITEM;
    int grade_col = grade_colunas(), grade_lin = grade_linhas();
    if (GRELHA) {
        for (int k = 0; k < grade_lin && lista.topo + k * grade_col < lista.qtd; k++)
            for (int c = 0; c < grade_col; c++) {
                int i = lista.topo + k * grade_col + c;
                if (i >= lista.qtd) break;
                desenhar_celula(i, x0 + 6 + c * celula_l(), lista.y + 4 + k * celula_a());
            }
    } else {
        for (int k = 0; k < vis && lista.topo + k < lista.qtd; k++) {
            int i = lista.topo + k;
            desenhar_linha(i, lista.y + 2 + k * LISTA_ALTURA_ITEM, marcados[i], i == lista.sel);
        }
    }
    if (lista.qtd == 0 && !busca_em_curso()) {
        const char *m = em_busca ? _("Sin resultados.") : _("Esta carpeta está vacía.");
        g_texto(j, x0 + (l - g_largura_texto(m)) / 2, lista.y + alt / 2 - 8, m, COR_TEXTO_FRACO);
    }
    /* A barra de deslocamento conta as unidades visiveis: itens nas vistas de
     * lista e de detalhes, linhas da grelha na vista de icones (D139). */
    int un_vis, un_total, un_topo;
    if (GRELHA) {
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
        g_retangulo(j, x0 + l - 6, lista.y + 2 + pol_y, 4, pol_a, sagui_esq.scroll_polegar);
    }
    jan_mostrar_area(j, x0, topo, l, lista.y + alt - topo);
}

static void desenhar_painel(int k)
{
    painel_t *ant = P;
    P = &paineis[k];
    desenhar_abas_painel(P);
    desenhar_rota_painel(P);
    desenhar_lista();
    P = ant;
}

/* La barra de estado: elementos, selección y espacio libre. */
static void desenhar_rodape(const char *msg)
{
    if (msg != rodape) copia_texto(rodape, msg, sizeof rodape);
    int y = ALT - ALT_STATUS;
    g_retangulo(j, 0, y, LARG, ALT_STATUS, mod_tema.barra);
    g_retangulo(j, 0, y, LARG, 1, mod_tema.borde);
    g_texto_n(j, 10, y + 4, msg, (LARG - 30) / LETRA_L, mod_tema.texto_fraco);
    char t[64];
    espaco_livre(t, sizeof t);
    int lw = g_largura_texto(t);
    if (lw < LARG - 20) g_texto(j, LARG - 10 - lw, y + 4, t, mod_tema.texto_fraco);
    jan_mostrar_area(j, 0, y, LARG, ALT_STATUS);
}

/* Lo que dice la barra de estado del elemento elegido. */
static void info_item(int i, char *msg, size_t max)
{
    if (tipos[i] == TIPO_GRUPO) { copia_texto(msg, nomes[i], max); return; }
    if (em_busca) { copia_texto(msg, rotas[i], max); return; }
    if (tipos[i] == TIPO_PASTA) formatar_texto(msg, max, _("%s/ (carpeta)"), nomes[i]);
    else formatar_texto(msg, max, _("%s: %u bytes%s"), nomes[i], tamanhos[i], tipos[i] == TIPO_PROGRAMA ? _(" (programa)") : "");
}

/* El texto del estado: cuántos elementos hay y qué está seleccionado. */
static void estado_texto(char *s, size_t max){
    int itens = 0;
    for (int i = 0; i < lista.qtd; i++) if (tipos[i] != TIPO_GRUPO) itens++;
    if (compara_texto(pasta, INICIO_RUTA) == 0) copia_texto(s, _("Inicio"), max);
    else if (em_busca) formatar_texto(s, max, _("%d resultados para %s"), n_resultados, padrao_busca);
    else if (filtro[0]) formatar_texto(s, max, _("%d elementos (filtro %s)"), itens, filtro);
    else if (itens == 1) formatar_texto(s, max, "%s", _("1 elemento"));
    else formatar_texto(s, max, _("%d elementos"), itens);
    if (n_marcados > 1) {
        char t[64];
        formatar_texto(t, sizeof t, _("%d seleccionados"), n_marcados);
        if (tam_texto(s) + 2 < max) { junta_texto(s, " - ", max); junta_texto(s, t, max); }
    }
}

static void desenhar_previa(void);

static void desenhar_tudo(void)
{
    layout();
    desenhar_barra();
    desenhar_ordens();
    desenhar_lateral();
    /* el área de los paneles se limpia: al aparecer las pestañas o la vista previa todo se desplaza */
    g_retangulo(j, LATERAL, ALT_BARRA + ALT_ORDENS, LARG - LATERAL, ALT - ALT_BARRA - ALT_ORDENS - ALT_STATUS, mod_tema.barra);
    jan_mostrar_area(j, LATERAL, ALT_BARRA + ALT_ORDENS, LARG - LATERAL, ALT - ALT_BARRA - ALT_ORDENS - ALT_STATUS);
    for (int k = 0; k < n_paineis; k++) desenhar_painel(k);
    desenhar_previa();
    char msg[200];
    estado_texto(msg, sizeof msg);
    desenhar_rodape(msg);
}

/* ------------------------------------------------------------ git (solo lectura, con la orden /apps/bin/git) */

static bool ruta_programa(char *bin, unsigned n, const char *prog);

/* Ejecuta una orden y recoge lo que escribe en la salida estándar (hasta max-1 bytes y prazo ms). */
static int capturar_saida(const char *bin, char *const argv[], char *buf, int max, unsigned prazo)
{
    int p[2];
    if (criar_pipe(p) < 0) return -1;
    int g = -1;
    if (tipo_descritor(SAIDA) >= 0) {
        for (int fd = 10; fd < 32; fd++) if (tipo_descritor(fd) < 0) { g = fd; break; }
        if (g >= 0) duplicar(SAIDA, g);
    }
    duplicar(p[1], SAIDA);
    int pid = executar(bin, argv);
    if (g >= 0) { duplicar(g, SAIDA); fechar(g); } else fechar(SAIDA);
    fechar(p[1]);
    if (pid < 0) { fechar(p[0]); return -1; }
    int n = 0;
    unsigned ini = tempo_ms();
    for (;;) {
        int d = disponivel(p[0]);
        if (d > 0) {
            int r = ler(p[0], buf + n, (unsigned)(max - 1 - n < d ? max - 1 - n : d));
            if (r > 0) n += r;
            if (n >= max - 1) break;
            continue;
        }
        if (d < 0) break;   /* terminó */
        if (tempo_ms() - ini > prazo) break;
        dormir(10);
    }
    fechar(p[0]);
    buf[n] = 0;
    int cod;
    if (esperar_pid(pid, &cod, 1) <= 0) { /* sigue corriendo: lo recoge el bucle principal (recolher) */ }
    return n;
}

static char git_raiz[160];          /* repositorio cuyo estado hay en git_saida */
static char git_saida[6000];
static unsigned git_ms;
static bool git_valido;

/* La raíz del repositorio que contiene la carpeta (el que tiene .git), o false. */
static bool git_achar_raiz(const char *dir, char *raiz, size_t max)
{
    char c[200];
    copia_texto(c, dir, sizeof c);
    for (int nivel = 0; nivel < 10; nivel++) {
        char g[220];
        caminho_juntar(g, sizeof g, c, ".git");
        info_arquivo_t inf;
        if (info_arquivo(g, &inf) == 0) { copia_texto(raiz, c, max); return true; }
        if (compara_texto(c, "/") == 0) return false;
        char *b = acha_ultimo_char(c, '/');
        if (!b) return false;
        if (b == c) copia_texto(c, "/", sizeof c); else *b = 0;
    }
    return false;
}

static bool git_ha(void) { return P->rama[0] != 0; }
static const char *git_ramo(void) { return P->rama; }
static char git_marca(int i) { return P->rama[0] ? (char)P->gitm[i] : 0; }

/* Lee el estado de git de la carpeta del panel (a lo sumo una vez cada 2 s por repositorio). */
static void git_atualizar(bool forzar)
{
    P->rama[0] = 0;
    for (int i = 0; i < MAX_FILAS; i++) P->gitm[i] = 0;
    if (P->s_em_busca || compara_texto(P->s_pasta, INICIO_RUTA) == 0) return;
    char raiz[160];
    if (!git_achar_raiz(P->s_pasta, raiz, sizeof raiz)) return;
    char bin[40];
    if (!ruta_programa(bin, sizeof bin, "git")) { copia_texto(P->rama, "?", sizeof P->rama); return; }
    if (forzar || !git_valido || compara_texto(git_raiz, raiz) != 0 || tempo_ms() - git_ms > 2000) {
        char *argv[4] = { "git", "status", "--short", NULL };
        char cwd[200];
        pasta_atual(cwd, sizeof cwd);
        mudar_pasta(raiz);
        int n = capturar_saida(bin, argv, git_saida, sizeof git_saida, 2500);
        mudar_pasta(cwd);
        git_valido = n >= 0;
        copia_texto(git_raiz, raiz, sizeof git_raiz);
        git_ms = tempo_ms();
        registrar("arquivos: git estado de %s (%d bytes)", raiz, n);
    }
    if (!git_valido) return;
    /* la ruta de la carpeta relativa a la raíz del repositorio */
    const char *rel = P->s_pasta + tam_texto(raiz);
    if (compara_texto(raiz, "/") == 0) rel = P->s_pasta + 1;
    else if (*rel == '/') rel++;
    char rama[48];
    copia_texto(rama, "HEAD", sizeof rama);
    /* los nombres de la salida corta: «## rama» y «XY ruta» */
    for (char *l = git_saida; *l;) {
        char *fim = l;
        while (*fim && *fim != '\n') fim++;
        char sv = *fim;
        *fim = 0;
        if (l[0] == '#' && l[1] == '#') {
            const char *r = l + 3;
            int k = 0;
            while (r[k] && r[k] != '.' && r[k] != ' ' && k < 46) { rama[k] = r[k]; k++; }
            rama[k] = 0;
            if (r[k] == ' ') {   /* «## No commits yet on main»: la rama es la última palabra */
                const char *u = acha_ultimo_char(r, ' ');
                if (u && u[1]) { copia_texto(rama, u + 1, sizeof rama); }
            }
        } else if (tam_texto(l) > 3) {
            char x = l[0], y = l[1];
            const char *cam = l + 3;
            char marca = (x == '?' && y == '?') ? '?' : (x == 'A' || y == 'A') ? 'A' : (x == 'D' || y == 'D') ? 'D' : 'M';
            /* ¿está dentro de esta carpeta? */
            size_t rl = tam_texto(rel);
            if (rl == 0 || (compara_texto_n(cam, rel, rl) == 0 && cam[rl] == '/')) {
                const char *resto = cam + (rl ? rl + 1 : 0);
                char nome[64];
                int k = 0;
                while (resto[k] && resto[k] != '/' && k < 63) { nome[k] = resto[k]; k++; }
                nome[k] = 0;
                for (int i = 0; i < P->s_lista.qtd; i++)
                    if (P->s_tipos[i] != TIPO_GRUPO && compara_texto(P->s_nomes[i], nome) == 0) {
                        if (!P->gitm[i] || marca == 'M') P->gitm[i] = (uint8_t)marca;
                        break;
                    }
            }
        }
        *fim = sv;
        l = sv ? fim + 1 : fim;
    }
    copia_texto(P->rama, rama, sizeof P->rama);
    registrar("arquivos: git rama %s en %s", rama, raiz);
}

/* ------------------------------------------------------------ cargar */

static void busca_cancelar(void);
static void previa_invalidar(void);

/* Lee la carpeta y deja la lista lista para dibujar. */
static void carregar(void)
{
    int n = 0, itens;
    if (compara_texto(pasta, INICIO_RUTA) == 0) {
        em_busca = false;
        n = 0;
        itens = 0;
    } else if (!em_busca) {
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
        itens = n;
        preparar_grupos(n);
        ordenar(n);
        if (!GRELHA) n = inserir_cabecalhos(n);
    } else {
        n = n_resultados;
        itens = n;
    }
    lista.qtd = n;
    lista.sel = -1;
    for (int i = 0; i < n; i++) if (tipos[i] != TIPO_GRUPO) { lista.sel = i; break; }
    lista.topo = 0;
    limpar_marcados();
    previa_invalidar();
    git_atualizar(false);
    desenhar_tudo();
    registrar("arquivos: pasta %s (%d itens)", pasta, itens);
}

/* ------------------------------------------------------------ navegación */

/* Deja la ruta limpia: sin barras repetidas ni barra final (salvo la raíz) y sin «.» ni «..». */
static void ruta_limpia(const char *r, char *s, size_t max)
{
    size_t n = 0;
    s[0] = 0;
    for (const char *q = r; *q && n + 2 < max;) {
        while (*q == '/') q++;
        const char *f = q;
        while (*f && *f != '/') f++;
        size_t t = (size_t)(f - q);
        if (t == 0) break;
        if (t == 1 && q[0] == '.') { q = f; continue; }
        if (t == 2 && q[0] == '.' && q[1] == '.') {
            while (n > 0 && s[n - 1] != '/') n--;
            if (n > 0) n--;
            s[n] = 0;
            q = f;
            continue;
        }
        s[n++] = '/';
        for (size_t k = 0; k < t && n + 1 < max; k++) s[n++] = q[k];
        s[n] = 0;
        q = f;
    }
    if (n == 0) copia_texto(s, "/", max);
}

static void entrar(const char *nova_, bool guardar_historia)
{
    info_arquivo_t inf;
    if (!nova_ || !nova_[0]) return;
    char nova_buf[200];
    const char *nova = nova_;
    if (nova_[0] == '/') { ruta_limpia(nova_, nova_buf, sizeof nova_buf); nova = nova_buf; }
    bool inicio = compara_texto(nova, INICIO_RUTA) == 0;
    if (!inicio && (info_arquivo(nova, &inf) < 0 || inf.tipo != TIPO_PASTA)) {
        registrar("arquivos: ruta no valida %s", nova);
        desenhar_rodape(_("Esa carpeta no existe"));
        return;
    }
    busca_cancelar();
    copia_texto(pasta, nova, sizeof pasta);
    em_busca = false;
    filtro[0] = 0;
    padrao_busca[0] = 0;
    if (!inicio) anotar_reciente(pasta);
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
    if (compara_texto(pasta, INICIO_RUTA) == 0) return;
    if (compara_texto(pasta, "/") == 0) return;
    char v[160];
    char *b = acha_ultimo_char(pasta, '/');
    if (b == pasta) copia_texto(v, "/", sizeof v);
    else { copia_texto(v, pasta, sizeof v); *acha_ultimo_char(v, '/') = 0; }
    entrar(v, true);
}

/* ------------------------------------------------------------ pestañas y paneles */

static void painel_iniciar(painel_t *p, const char *caminho)
{
    memset(p, 0, sizeof *p);
    copia_texto(p->s_pasta, caminho, sizeof p->s_pasta);
    p->s_lista = (lista_t){ 0, 0, 100, 100, p->s_nomes, 0, -1, 0, NULL, desenhar_lista_gancho };
    p->s_hist_pos = 0; p->s_hist_n = 1;
    copia_texto(p->s_hist[0], caminho, sizeof p->s_hist[0]);
    p->n_abas = 1; p->aba_sel = 0;
    p->barra_abas.destaque = -1; p->barra_abas.premida = -1;
}

static void aba_guardar(painel_t *p)
{
    aba_t *a = &p->abas[p->aba_sel];
    copia_texto(a->a_pasta, p->s_pasta, sizeof a->a_pasta);
    a->a_vista = p->s_vista; a->a_ordem = p->s_ordem; a->a_ordem_desc = p->s_ordem_desc; a->a_agrupar = p->s_agrupar;
}

/* Hace de la pestaña i la activa del panel P (la lista de la que sale se guarda sin recargar). */
static void aba_abrir(int i, bool nova)
{
    painel_t *p = P;
    if (i < 0 || i >= p->n_abas) return;
    if (!nova) aba_guardar(p);
    p->aba_sel = i;
    aba_t *a = &p->abas[i];
    p->s_vista = a->a_vista; p->s_ordem = a->a_ordem; p->s_ordem_desc = a->a_ordem_desc; p->s_agrupar = a->a_agrupar;
    p->s_hist_n = 0; p->s_hist_pos = -1;
    p->s_em_busca = false;
    char c[160];
    copia_texto(c, a->a_pasta, sizeof c);
    copia_texto(p->s_pasta, "/", sizeof p->s_pasta);   /* para que entrar() no crea que ya está */
    layout();
    entrar(c, true);
    registrar("arquivos: aba %d de %d (%s)", i + 1, p->n_abas, c);
}

static void aba_nova(const char *caminho)
{
    painel_t *p = P;
    if (p->n_abas >= ABAS_MAX) { desenhar_rodape(_("No caben más pestañas")); return; }
    aba_guardar(p);
    aba_t *n = &p->abas[p->n_abas];
    copia_texto(n->a_pasta, caminho, sizeof n->a_pasta);
    n->a_vista = p->s_vista; n->a_ordem = p->s_ordem; n->a_ordem_desc = p->s_ordem_desc; n->a_agrupar = p->s_agrupar;
    p->n_abas++;
    aba_abrir(p->n_abas - 1, true);
    registrar("arquivos: aba nueva (%d)", p->n_abas);
}

static void aba_fechar(int i)
{
    painel_t *p = P;
    if (p->n_abas <= 1 || i < 0 || i >= p->n_abas) { if (p->n_abas <= 1) desenhar_rodape(_("Es la única pestaña")); return; }
    aba_guardar(p);
    for (int k = i; k + 1 < p->n_abas; k++) p->abas[k] = p->abas[k + 1];
    p->n_abas--;
    int sel = p->aba_sel;
    if (i < sel) sel--;
    else if (i == sel && sel >= p->n_abas) sel = p->n_abas - 1;
    registrar("arquivos: aba cerrada (%d)", p->n_abas);
    aba_abrir(sel, i != p->aba_sel ? false : true);
    if (p->n_abas == 1) { layout(); desenhar_tudo(); }
}

static void aba_proxima(int dir)
{
    painel_t *p = P;
    if (p->n_abas < 2) return;
    aba_abrir((p->aba_sel + dir + p->n_abas) % p->n_abas, false);
}

/* Panel doble: el segundo panel empieza en la misma carpeta. */
static void painel_doble(void)
{
    if (n_paineis == 1) {
        painel_iniciar(&paineis[1], pasta);
        paineis[1].s_vista = vista; paineis[1].s_ordem = ordem; paineis[1].s_ordem_desc = ordem_desc; paineis[1].s_agrupar = agrupar;
        n_paineis = 2;
        painel_t *ant = P;
        usar_painel(1);
        layout();
        carregar();
        P = ant; ativo = (int)(P - paineis);
    } else {
        n_paineis = 1;
        usar_painel(0);
    }
    layout();
    desenhar_tudo();
    registrar("arquivos: paneles %d", n_paineis);
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

/* .pptx .tcn: presentaciones, para Tucán (D369). */
static bool tucan_extensao(const char *c)
{
    size_t n = tam_texto(c);
    if (n > 5) {
        char e[6];
        for (int k = 0; k < 5; k++) e[k] = (char)(c[n - 5 + k] | 0x20);
        e[5] = 0;
        if (compara_texto(e, ".pptx") == 0) return true;
    }
    if (n > 4) {
        char e[5];
        for (int k = 0; k < 4; k++) e[k] = (char)(c[n - 4 + k] | 0x20);
        e[4] = 0;
        if (compara_texto(e, ".tcn") == 0) return true;
    }
    return false;
}

/* .db .sqlite .sqlite3: bases de datos, para el Explorador de bases de datos (mejora 127, D304). */
static bool bd_extensao(const char *c)
{
    size_t n = tam_texto(c);
    char e[9];
    size_t k = 0;
    if (n < 4) return false;
    for (size_t i = n - (n > 8 ? 8 : n); i < n; i++) e[k++] = (char)(c[i] | 0x20);
    e[k] = 0;
    return (k >= 3 && compara_texto(e + k - 3, ".db") == 0) || (k >= 7 && compara_texto(e + k - 7, ".sqlite") == 0) ||
           (k >= 8 && compara_texto(e + k - 8, ".sqlite3") == 0);
}

/* .avi .mpg .mpeg .mp4 .m4v: el reproductor de video (mejora 048, D342). */
static bool video_extensao(const char *c)
{
    size_t n = tam_texto(c);
    if (n < 4) return false;
    char e[6];
    for (int k = 0; k < 4; k++) e[k] = (char)(c[n - 4 + k] | 0x20);
    e[4] = 0;
    if (compara_texto(e, ".avi") == 0) return true;
    if (compara_texto(e, ".mpg") == 0) return true;
    if (compara_texto(e, ".mp4") == 0) return true;   /* D342: H.264 + AAC (D276) */
    if (compara_texto(e, ".m4v") == 0) return true;
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

/* .jbt y .blq (el nombre de antes): proyectos de Jabuti, la aplicación para aprender a programar (D377). */
static bool jabuti_extensao(const char *c)
{
    size_t n = tam_texto(c);
    if (n < 5) return false;
    char e[5];
    for (int k = 0; k < 4; k++) e[k] = (char)(c[n - 4 + k] | 0x20);
    e[4] = 0;
    return compara_texto(e, ".jbt") == 0 || compara_texto(e, ".blq") == 0;
}

/* .ttf .otf: la vista previa de la fuente con el botón «Instalar» (programa fuentes, D356) */
static bool fuente_extensao(const char *c)
{
    unsigned n = tam_texto(c);
    if (n < 5 || c[n - 4] != '.') return false;
    char e[4] = { (char)(c[n - 3] | 0x20), (char)(c[n - 2] | 0x20), (char)(c[n - 1] | 0x20), 0 };
    return compara_texto(e, "ttf") == 0 || compara_texto(e, "otf") == 0;
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

/* .nes: ROM de NES, LaiNES de /apps (D321; BSD). .sms .sg .gg: Master System, SG-1000 y Game Gear, Gearsystem (D321; GPL-3.0). */
static bool ext_rom(const char *c, const char *e1, const char *e2)
{
    unsigned n = tam_texto(c);
    unsigned l1 = tam_texto(e1);
    if (n > l1 && c[n - l1 - 1] == '.') {
        bool ig = true;
        for (unsigned k = 0; k < l1; k++) if ((c[n - l1 + k] | 0x20) != e1[k]) ig = false;
        if (ig) return true;
    }
    if (!e2) return false;
    return ext_rom(c, e2, NULL);
}
static bool nes_extensao(const char *c) { return ext_rom(c, "nes", NULL); }
#ifndef SAGUI_SEM_GPL
/* .sav .sav.gz: partidas guardadas de Freeciv (D374; GPL); el cliente las carga con --file. */
static bool freeciv_extensao(const char *c)
{
    unsigned n = tam_texto(c);
    if (ext_rom(c, "sav", NULL)) return true;
    return n > 7 && c[n - 7] == '.' && (c[n - 6] | 0x20) == 's' && (c[n - 5] | 0x20) == 'a' && (c[n - 4] | 0x20) == 'v'
           && c[n - 3] == '.' && (c[n - 2] | 0x20) == 'g' && (c[n - 1] | 0x20) == 'z';
}
#endif
static bool sms_extensao(const char *c) { return ext_rom(c, "sms", "sg") || ext_rom(c, "gg", NULL); }

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

/* D324: la ruta de un programa por su nombre: /bin si esta en la base, /apps/bin (la particion
 * SAGUI-APPS) si es una aplicacion que no va en el ramdisk. */
static bool ruta_programa(char *bin, unsigned n, const char *prog)
{
    info_arquivo_t inf;
    formatar_texto(bin, n, "/bin/%s", prog);
    if (info_arquivo(bin, &inf) == 0) return true;
    formatar_texto(bin, n, "/apps/bin/%s", prog);
    if (info_arquivo(bin, &inf) == 0) return true;
    formatar_texto(bin, n, "/bin/%s", prog);   /* no esta en ninguno: el error sale al ejecutar */
    return false;
}

static void abrir_arquivo(const char *c, const char *nome, int tipo)
{
    anotar_reciente(c);
    char *argv[3];
    int pid;
    if (tipo == TIPO_PROGRAMA) {
        argv[0] = (char *)nome; argv[1] = NULL;
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
        if (tucan_extensao(c)) prog = "tucan";   /* .pptx .tcn: Tucán (D369) */
        if (jabuti_extensao(c)) prog = "jabuti";   /* .jbt .blq: Jabuti (D377) */
        if (video_extensao(c)) prog = "reproductor";   /* .avi .mpg .mp4 .m4v: el Reproductor de vídeo (D129) */
        char bin[40];
        ruta_programa(bin, sizeof bin, prog);
        char *argv_ft[4];
        bool ft = fuente_extensao(c);
        if (ft) { prog = "fuentes"; ruta_programa(bin, sizeof bin, prog); argv_ft[0] = "fuentes"; argv_ft[1] = "--ventana"; argv_ft[2] = (char *)c; argv_ft[3] = NULL; }
#ifndef SAGUI_SEM_GPL
        if (gb_extensao(c)) { prog = "gnuboy"; formatar_texto(bin, sizeof bin, "/apps/bin/gnuboy"); }   /* .gb .gbc: GNUBoy (D235) */
        if (dos_extensao(c)) { prog = "dosbox"; formatar_texto(bin, sizeof bin, "/apps/bin/dosbox"); }   /* .exe .com .bat: DOSBox (D294) */
        if (sms_extensao(c)) { prog = "gearsystem"; formatar_texto(bin, sizeof bin, "/apps/bin/gearsystem"); }   /* .sms .sg .gg: Gearsystem (D321) */
#endif
        if (nes_extensao(c)) { prog = "laines"; formatar_texto(bin, sizeof bin, "/apps/bin/laines"); }   /* .nes: LaiNES (D321) */
        char *argv_fc[4];
        bool fc = false;
#ifndef SAGUI_SEM_GPL
        fc = freeciv_extensao(c);
#endif
        if (fc) { prog = "freeciv-sdl2"; formatar_texto(bin, sizeof bin, "/apps/bin/freeciv-sdl2"); argv_fc[0] = "freeciv-sdl2"; argv_fc[1] = "--file"; argv_fc[2] = (char *)c; argv_fc[3] = NULL; }   /* .sav .sav.gz: Freeciv (D374) */
        argv[0] = (char *)prog; argv[1] = (char *)c; argv[2] = NULL;
        char *argv_bd[4];
        if (bd_extensao(c)) {   /* .db .sqlite .sqlite3: el Explorador SQLite (Python, D333); no ejecuta nada al abrir */
            prog = "exploradorsqlite";
            ruta_programa(bin, sizeof bin, "python");
            argv_bd[0] = "python"; argv_bd[1] = "/lib/python/basedatos/explorador_sqlite.py"; argv_bd[2] = (char *)c; argv_bd[3] = NULL;
            pid = executar(bin, argv_bd);
        } else if (fc) pid = executar(bin, argv_fc);
        else if (ft) pid = executar(bin, argv_ft);   /* .ttf .otf: vista previa e «Instalar» (D356) */
        else
        pid = executar(bin, argv);
        registrar("arquivos: abrindo no %s %s (pid %d)", prog, c, pid);
    }
    char msg[200];
    formatar_texto(msg, sizeof msg, pid > 0 ? _("Abriendo %s...") : _("No se ha podido abrir %s"), nome);
    desenhar_rodape(msg);
}

static void abrir_item(int i)
{
    if (i < 0 || i >= lista.qtd || tipos[i] == TIPO_GRUPO) return;
    char c[200];
    caminho_de(i, c, sizeof c);
    if (tipos[i] == TIPO_PASTA) { entrar(c, true); return; }
    abrir_arquivo(c, nomes[i], tipos[i]);
}

static void nova_pasta(void)
{
    char nome[64] = "";
    if (!pedir_texto(_("Nueva carpeta"), _("Nombre de la nueva carpeta:"), nome, sizeof nome) || !nome[0]) return;
    char c[200];
    caminho_juntar(c, sizeof c, pasta, nome);
    int r = criar_pasta(c);
    if (r < 0) { char m[200]; formatar_texto(m, sizeof m, _("No se ha podido crear la carpeta:\n%s"), nome_erro(r)); mensagem(_("Archivos"), m); return; }
    registrar("arquivos: pasta criada %s", c);
    carregar();
}

static void apagar_item(void)
{
    if (n_marcados <= 0) {
        desenhar_rodape(_("No hay nada seleccionado"));
        return;
    }
    char pergunta[200];
    if (n_marcados == 1) {
        int i = lista.sel;
        if (i < 0 || i >= lista.qtd) return;
        char c[200];
        caminho_de(i, c, sizeof c);
        formatar_texto(pergunta, sizeof pergunta, _("¿Seguro que quieres borrar\n%s%s?"), nomes[i], tipos[i] == TIPO_PASTA ? "/" : "");
        if (!confirmar(_("Borrar"), pergunta)) return;
        int r = tipos[i] == TIPO_PASTA ? remover_pasta(c) : apagar(c);
        if (r < 0) { char m[200]; formatar_texto(m, sizeof m, _("No se ha podido borrar:\n%s"), nome_erro(r)); mensagem(_("Archivos"), m); return; }
        registrar("arquivos: apagado %s", c);
    } else {
        formatar_texto(pergunta, sizeof pergunta, _("¿Seguro que quieres borrar\n%d elementos?"), n_marcados);
        if (!confirmar(_("Borrar"), pergunta)) return;
        for (int i = 0; i < lista.qtd; i++) {
            if (!marcados[i]) continue;
            char c[200];
            caminho_de(i, c, sizeof c);
            int r = tipos[i] == TIPO_PASTA ? remover_pasta(c) : apagar(c);
            if (r < 0) continue;
            registrar("arquivos: apagado %s", c);
        }
    }
    sonido_evento("papelera");   /* sonido del sistema al borrar (D421; no hace nada si está apagado) */
    em_busca = false;          /* al borrar se sale de la búsqueda */
    carregar();
}

static void renomear_item(void)
{
    int i = lista.sel;
    if (i < 0 || i >= lista.qtd) return;
    char nome[64], antigo[200], novo[200];
    copia_texto(nome, nomes[i], sizeof nome);
    if (!pedir_texto(_("Renombrar"), _("Nuevo nombre:"), nome, sizeof nome) || !nome[0]) return;
    if (compara_texto(nome, nomes[i]) == 0) return;
    if (acha_char(nome, '/')) { mensagem(_("Archivos"), _("El nombre no puede contener '/'.")); return; }
    caminho_de(i, antigo, sizeof antigo);
    caminho_juntar(novo, sizeof novo, pasta, nome);
    int r = renomear(antigo, novo);
    if (r < 0) { char m[200]; formatar_texto(m, sizeof m, _("No se ha podido renombrar:\n%s"), nome_erro(r)); mensagem(_("Archivos"), m); return; }
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
    char quando[64], criado[64];
    copia_texto(quando, _("desconocida"), sizeof quando);
    copia_texto(criado, quando, sizeof criado);
    datas_arquivo_t d;
    if (datas_arquivo(c, &d) == 0) {
        fuso_data_t l;
        if (d.modificado) { hora_local(d.modificado, &l); formatar_data(quando, sizeof quando, "%d/%m/%Y %H:%M:%S %Z", &l); }
        if (d.criado) { hora_local(d.criado, &l); formatar_data(criado, sizeof criado, "%d/%m/%Y %H:%M:%S %Z", &l); }
    }
    uint32_t tam = tamanhos[i];
    if (tipos[i] == TIPO_PASTA) tam = tamanho_pasta(c, 0);
    char tamtxt[24];
    formatar_texto(tamtxt, sizeof tamtxt, _("%u bytes"), tam);
    char livre[64];
    espaco_livre(livre, sizeof livre);
    /* D282: permisos, dueño y grupo (en FAT32, los del montaje). */
    char perm[120];
    formatar_texto(perm, sizeof perm, "%s", _("desconocidos"));
    info_permisos_t p;
    if (permisos_arquivo(c, &p) == 0) {
        char rwx[10], dono[CONTA_NOME_MAX], grupo[CONTA_NOME_MAX];
        texto_modo(p.modo, rwx);
        formatar_texto(perm, sizeof perm, _("%s (%04o)%s\nDueño: %s\nGrupo: %s"), rwx, p.modo,
                       p.com_donos ? "" : _(", los del montaje"), conta_nome(p.uid, dono, sizeof dono),
                       conta_nome(p.gid, grupo, sizeof grupo));
    }
    formatar_texto(m, sizeof m, _("Nombre: %s\nTipo: %s\nTamaño: %s\nCreado: %s\nModificado: %s\nPermisos: %s\nRuta: %s\nEspacio libre: %s"),
                    nomes[i], tipo, tamtxt, criado, quando, perm, c, livre);
    registrar("arquivos: propriedades de %s (modificado %s)", c, quando);
    mensagem(_("Propiedades"), m);
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
        formatar_texto(msg, sizeof msg, _("%s ya existe.\n¿Reemplazarlo?"), saida);
        if (!confirmar(_("Comprimir"), msg)) return;
    }
    desenhar_rodape(_("Comprimiendo..."));
    char *a_zip[] = { "zip", "-rq", saida, nomes[i], NULL };
    char *a_tgz[] = { "tar", "-czf", saida, nomes[i], NULL };
    int r = rodar(tgz ? a_tgz : a_zip);
    registrar("arquivos: comprimir %s -> %s (codigo %d)", nomes[i], saida, r);
    carregar();
    formatar_texto(msg, sizeof msg, r == 0 ? _("Creado %s") : _("No se ha podido crear %s"), saida);
    desenhar_rodape(msg);
}

/* ---- accoes de grupo (D139): com varios elementos marcados ---- */

/* Abre todos os elementos marcados: cada um no seu programa (as pastas, numa
 * janela nova do explorador). */
static void abrir_grupo(void)
{
    if (n_marcados <= 0) { desenhar_rodape(_("No hay nada seleccionado")); return; }
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
    formatar_texto(msg, sizeof msg, _("%d elementos abiertos"), abertos);
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
    formatar_texto(m, sizeof m, _("Elementos: %d\nTama\xf1o total: %u bytes\nEspacio libre: %s\n\n%s"), n, total, livre, lista_nomes);
    registrar("arquivos: propriedades do grupo (%d itens)", n);
    mensagem(_("Propiedades"), m);
}

/* Comprime todos os elementos marcados num unico .zip ou .tar.gz. */
static void comprimir_grupo(bool tgz)
{
    if (n_marcados <= 0) { desenhar_rodape(_("No hay nada seleccionado")); return; }
    char base[64];
    if (compara_texto(pasta, "/") == 0) copia_texto(base, "seleccion", sizeof base);
    else copia_texto(base, nome_base(pasta), sizeof base);
    char saida[100], msg[200];
    formatar_texto(saida, sizeof saida, "%s%s", base, tgz ? ".tar.gz" : ".zip");
    info_arquivo_t inf;
    char c[200];
    caminho_juntar(c, sizeof c, pasta, saida);
    if (info_arquivo(c, &inf) == 0) {
        formatar_texto(msg, sizeof msg, _("%s ya existe.\n\xbfReemplazarlo?"), saida);
        if (!confirmar(_("Comprimir"), msg)) return;
    }
    desenhar_rodape(_("Comprimiendo..."));
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
        formatar_texto(msg, sizeof msg, _("Creado %s (%d de %d elementos)"), saida, quantos, quantos + skipped);
    else
        formatar_texto(msg, sizeof msg, r == 0 ? _("Creado %s") : _("No se ha podido crear %s"), saida);
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
        if (!pedir_texto(_("Extraer en carpeta"), _("Carpeta de destino:"), destino, sizeof destino) || !destino[0]) return;
    }
    desenhar_rodape(_("Extrayendo..."));
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
    formatar_texto(msg, sizeof msg, r == 0 ? _("Extraído %s") : _("No se ha podido extraer %s (¿ya existía algo?)"), nomes[i]);
    desenhar_rodape(msg);
}

/* ------------------------------------------------------------ copiar y pegar */

/* D341: la copia queda con la fecha de modificacion del original, como cp -p (sin error si el
 * destino no la guarda). */
static void conservar_data(const char *origem, const char *destino)
{
    datas_arquivo_t d;
    if (datas_arquivo(origem, &d) == 0 && d.modificado) fixar_datas(destino, d.modificado, PARQ_DATA_MANTER);
}

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
        conservar_data(origem, destino);
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
    conservar_data(origem, destino);
    return true;
}

/* Copia (Ctrl+C) o corta (Ctrl+X) los elementos marcados. */
static void copiar_area(bool mover)
{
    if (n_marcados <= 0) {
        desenhar_rodape(_("No hay nada seleccionado"));
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
    formatar_texto(msg, sizeof msg, _("%d %s para el portapapeles"), area_n, mover ? _("cortados") : _("copiados"));
    desenhar_rodape(msg);
    registrar("arquivos: %s %d itens para a area de transferencia", mover ? "cortados" : "copiados", area_n);
}

/* Pega (Ctrl+V) el portapapeles en la carpeta actual. */
static void colar_area(void)
{
    if (area_n <= 0) {
        desenhar_rodape(_("El portapapeles de archivos está vacío"));
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
            formatar_texto(m, sizeof m, _("%s ya existe.\n¿Reemplazarlo?"), nome);
            if (!confirmar(_("Pegar"), m)) continue;
        }
        desenhar_rodape(_("Copiando..."));
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
    formatar_texto(msg, sizeof msg, _("%d %s pegados"), copiados, mover ? _("movidos") : _("copiados"));
    desenhar_rodape(msg);
    registrar("arquivos: colados %d itens", copiados);
    carregar();
}

/* ------------------------------------------------------------ búsqueda */

/* Búsqueda recursiva en segundo plano (D420): se recorre un puñado de entradas por vuelta del bucle de
 * eventos y la lista se va llenando, así la ventana no se congela en un árbol grande. Con filtros de tipo,
 * tamaño y fecha. Una búsqueda pequeña termina en la primera pasada. */
#define BUSCA_FILA 96
#define BUSCA_NIVEL_MAX 8
static int f_tipo, f_tam, f_data;     /* filtros: tipo (0-5), tamaño (0-2), fecha (0-3) */
static struct {
    bool ativa;
    int painel;
    char fila[BUSCA_FILA][200];
    uint8_t nivel[BUSCA_FILA];
    int n_fila;
    char atual[200];
    int atual_nivel, idx;
    bool abierta;
    unsigned ultimo_dibujo;
} busca;

static int filtros_ativos(void) { return (f_tipo != 0) + (f_tam != 0) + (f_data != 0); }
static bool busca_em_curso(void) { return busca.ativa; }
static void busca_cancelar(void) { busca.ativa = false; }

static bool filtro_tipo_ok(const char *nome, uint8_t tipo)
{
    switch (f_tipo) {
    case 1: return tipo == TIPO_PASTA;
    case 2: return tipo != TIPO_PASTA && eh_imagem_arq(nome);
    case 3: return tipo != TIPO_PASTA && tem_ext(nome, "txt md pdf doc docx odt rtf csv xls xlsx pptx tcn c h py sh json xml html htm css js");
    case 4: return tipo != TIPO_PASTA && tem_ext(nome, "wav mp3 flac ogg mid midi kar avi mpg mpeg mp4 m4v mkv");
    case 5: return tipo == TIPO_PROGRAMA;
    default: return true;
    }
}

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

static void busca_mostrar(bool final)
{
    painel_t *ant = P;
    P = &paineis[busca.painel];
    lista.qtd = n_resultados;
    if (lista.sel < 0 && n_resultados) lista.sel = 0;
    desenhar_rota_painel(P);
    desenhar_lista();
    char msg[160];
    formatar_texto(msg, sizeof msg, final ? _("%d resultados para %s") : _("Buscando... %d resultados para %s"), n_resultados, padrao_busca);
    P = ant;
    desenhar_rodape(msg);
}

/* Procesa entradas durante ms milisegundos; devuelve true si la búsqueda ha terminado. */
static bool busca_passo(unsigned ms)
{
    unsigned ini = tempo_ms();
    painel_t *ant = P;
    P = &paineis[busca.painel];
    bool fim = false;
    uint32_t agora = f_data ? hora_utc(NULL) : 0;
    while (!fim) {
        if (!busca.abierta) {
            if (busca.n_fila == 0) { fim = true; break; }
            busca.n_fila--;
            copia_texto(busca.atual, busca.fila[busca.n_fila], sizeof busca.atual);
            busca.atual_nivel = busca.nivel[busca.n_fila];
            busca.idx = 0;
            busca.abierta = true;
        }
        entrada_pasta_t e;
        int r = listar(busca.atual, busca.idx++, &e);
        if (r != 1) { busca.abierta = false; }
        else {
            bool oculto = eh_oculto(e.nome);
            if (!oculto || ver_ocultos) {
                if (e.tipo == TIPO_PASTA && busca.atual_nivel < BUSCA_NIVEL_MAX && busca.n_fila < BUSCA_FILA &&
                    !(compara_texto(busca.atual, "/") == 0 && (compara_texto(e.nome, "proc") == 0 || compara_texto(e.nome, "dev") == 0))) {
                    caminho_juntar(busca.fila[busca.n_fila], sizeof busca.fila[0], busca.atual, e.nome);
                    busca.nivel[busca.n_fila++] = (uint8_t)(busca.atual_nivel + 1);
                }
                if (casa_filtro(padrao_busca, e.nome) && filtro_tipo_ok(e.nome, (uint8_t)e.tipo) &&
                    (f_tam == 0 || (e.tipo != TIPO_PASTA && (f_tam == 1 ? e.tamanho > 1048576u : e.tamanho < 102400u)))) {
                    bool ok = true;
                    if (f_data) {
                        datas_arquivo_t d;
                        char c[200];
                        caminho_juntar(c, sizeof c, busca.atual, e.nome);
                        uint32_t lim = f_data == 1 ? 86400u : f_data == 2 ? 7u * 86400u : 30u * 86400u;
                        ok = datas_arquivo(c, &d) == 0 && d.modificado && agora >= d.modificado && agora - d.modificado <= lim;
                    }
                    if (ok && !acrescentar_resultado(busca.atual, e.nome, (uint8_t)e.tipo, e.tamanho)) fim = true;
                }
            }
        }
        if (tempo_ms() - ini >= ms) break;
    }
    P = ant;
    return fim;
}

static void busca_acabar(void)
{
    busca.ativa = false;
    painel_t *ant = P;
    P = &paineis[busca.painel];
    registrar("arquivos: busca %s (%d resultados)", padrao_busca, n_resultados);
    P = ant;
    busca_mostrar(true);
}

/* Un paso de la búsqueda entre dos eventos. */
static void busca_tic(void)
{
    if (!busca.ativa) return;
    bool fim = busca_passo(20);
    if (fim) { busca_acabar(); return; }
    if (tempo_ms() - busca.ultimo_dibujo > 250) { busca.ultimo_dibujo = tempo_ms(); busca_mostrar(false); }
}

static void buscar(const char *patrao)
{
    if (!patrao || !patrao[0]) return;
    busca.ativa = false;
    copia_texto(padrao_busca, patrao, sizeof padrao_busca);
    n_resultados = 0;
    em_busca = true;
    busca.painel = (int)(P - paineis);
    busca.n_fila = 1;
    copia_texto(busca.fila[0], compara_texto(pasta, INICIO_RUTA) == 0 ? "/" : pasta, sizeof busca.fila[0]);
    busca.nivel[0] = 0;
    busca.abierta = false;
    busca.ativa = true;
    busca.ultimo_dibujo = tempo_ms();
    bool fim = busca_passo(30);   /* lo pequeño termina aquí */
    lista.qtd = n_resultados;
    lista.sel = n_resultados ? 0 : -1;
    lista.topo = 0;
    limpar_marcados();
    previa_invalidar();
    if (fim) { busca.ativa = false; }
    desenhar_tudo();
    if (fim) busca_acabar();
}

/* Los archivos con una etiqueta de color, como una lista de resultados. */
static void mostrar_etiqueta(int cor)
{
    busca_cancelar();
    n_resultados = 0;
    for (int i = 0; i < n_etq && n_resultados < MAX_ITENS; i++) {
        if (etq_cor[i] != cor) continue;
        info_arquivo_t inf;
        if (info_arquivo(etq_ruta[i], &inf) < 0) continue;
        char dir[160];
        copia_texto(dir, etq_ruta[i], sizeof dir);
        char *b = acha_ultimo_char(dir, '/');
        if (b == dir) copia_texto(dir, "/", sizeof dir); else if (b) *b = 0;
        acrescentar_resultado(dir, nome_base(etq_ruta[i]), (uint8_t)inf.tipo, inf.tamanho);
    }
    formatar_texto(padrao_busca, sizeof padrao_busca, "@etq%d", cor);
    em_busca = true;
    lista.qtd = n_resultados;
    lista.sel = n_resultados ? 0 : -1;
    lista.topo = 0;
    limpar_marcados();
    previa_invalidar();
    desenhar_tudo();
    char msg[120];
    formatar_texto(msg, sizeof msg, _("%d archivos con etiqueta %s"), n_resultados, _(NOME_ETQ[cor]));
    desenhar_rodape(msg);
    registrar("arquivos: etiqueta %d (%d archivos)", cor, n_resultados);
}

/* Menú de filtros: marca los activos. */
static void menu_filtros(int x, int y)
{
    static const char *const T[] = { N_("Tipo: cualquiera"), N_("Tipo: carpetas"), N_("Tipo: imágenes"), N_("Tipo: documentos"), N_("Tipo: audio y vídeo"), N_("Tipo: programas") };
    static const char *const Z[] = { N_("Tamaño: cualquiera"), N_("Tamaño: más de 1 MB"), N_("Tamaño: menos de 100 KB") };
    static const char *const D[] = { N_("Fecha: cualquiera"), N_("Fecha: hoy"), N_("Fecha: últimos 7 días"), N_("Fecha: últimos 30 días") };
    char it[640];
    int k = 0;
    it[0] = 0;
#define LINEA(txt, on) do { if (k) it[k++] = '\n'; if (on) it[k++] = '*'; const char *q_ = (txt); while (*q_ && k < (int)sizeof it - 2) it[k++] = *q_++; it[k] = 0; } while (0)
    for (int i = 0; i < 6; i++) LINEA(_(T[i]), i == f_tipo);
    LINEA("-", false);
    for (int i = 0; i < 3; i++) LINEA(_(Z[i]), i == f_tam);
    LINEA("-", false);
    for (int i = 0; i < 4; i++) LINEA(_(D[i]), i == f_data);
#undef LINEA
    menu_contextual(j->id, x, y, it, MENU_FILTRO);
}

/* Cambia un filtro (índice del menú) y repite la búsqueda en curso. */
static void aplicar_filtro(int idx)
{
    if (idx >= 0 && idx <= 5) f_tipo = idx;
    else if (idx >= 7 && idx <= 9) f_tam = idx - 7;
    else if (idx >= 11 && idx <= 14) f_data = idx - 11;
    else return;
    registrar("arquivos: filtro tipo=%d tamano=%d fecha=%d", f_tipo, f_tam, f_data);
    if (padrao_busca[0] && padrao_busca[0] != '@') { char v[64]; copia_texto(v, padrao_busca, sizeof v); buscar(v); }
    else desenhar_rota_painel(P);
}

/* ------------------------------------------------------------ miniaturas y vista previa */

/* Miniaturas (D420): las imágenes de la vista de iconos y de mosaicos se muestran reducidas. Se decodifican de a
 * una por vuelta ociosa del bucle de eventos (una foto grande tarda en un equipo antiguo) y se guardan en una
 * caché pequeña con reemplazo del menos usado; hasta entonces sale el icono de siempre. */
#define MINI_N 24
#define MINI_L 56
#define MINI_A 40
#define MINI_MAX_BYTES (400 * 1024)
typedef struct { char cam[200]; uint32_t tam; imagem_t img; bool ok, falha, em_uso; unsigned uso; } mini_t;
static mini_t mini[MINI_N];
static unsigned mini_reloj;
static bool mini_pendientes;

static mini_t *mini_achar(const char *cam, uint32_t tam)
{
    for (int k = 0; k < MINI_N; k++)
        if (mini[k].em_uso && mini[k].tam == tam && compara_texto(mini[k].cam, cam) == 0) return &mini[k];
    return NULL;
}

/* Dibuja la imagen cabiendo en la caja l x a, centrada. */
static void imagem_encaixada(const imagem_t *img, int x, int y, int l, int a)
{
    if (img->larg <= 0 || img->alt <= 0) return;
    int w = l, h = img->alt * l / img->larg;
    if (h > a) { h = a; w = img->larg * a / img->alt; }
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    imagem_desenhar(j->px, j->larg, x, y, l, a, img, x + (l - w) / 2, y + (a - h) / 2, w, h, false);
}

static bool miniatura_desenhar(int i, int x, int y, int l, int a)
{
    if (tipos[i] != TIPO_ARQUIVO || !eh_imagem_arq(nomes[i]) || tamanhos[i] > MINI_MAX_BYTES) return false;
    char c[200];
    caminho_de(i, c, sizeof c);
    mini_t *m = mini_achar(c, tamanhos[i]);
    if (!m) { mini_pendientes = true; return false; }
    if (!m->ok) return false;
    m->uso = ++mini_reloj;
    imagem_encaixada(&m->img, x, y, l, a);
    return true;
}

/* Prepara una miniatura de las que se ven. Devuelve true si hizo algo (hay que redibujar). */
static bool miniatura_gerar(void)
{
    if (!mini_pendientes) return false;
    mini_pendientes = false;
    for (int k = 0; k < n_paineis; k++) {
        painel_t *p = &paineis[k];
        if (p->s_vista != VISTA_ICONES && p->s_vista != VISTA_MOSAICOS) continue;
        painel_t *ant = P;
        P = p;
        int col = grade_colunas(), lin = grade_linhas();
        for (int q = 0; q < col * lin && p->s_lista.topo + q < p->s_lista.qtd; q++) {
            int i = p->s_lista.topo + q;
            if (p->s_tipos[i] != TIPO_ARQUIVO || !eh_imagem_arq(p->s_nomes[i]) || p->s_tamanhos[i] > MINI_MAX_BYTES) continue;
            char c[200];
            caminho_de(i, c, sizeof c);
            if (mini_achar(c, p->s_tamanhos[i])) continue;
            /* hueco: uno libre o el menos usado */
            int s = 0;
            for (int z = 0; z < MINI_N; z++) {
                if (!mini[z].em_uso) { s = z; break; }
                if (mini[z].uso < mini[s].uso) s = z;
            }
            mini_t *m = &mini[s];
            if (m->em_uso && m->ok) imagem_liberar(&m->img);
            memset(m, 0, sizeof *m);
            copia_texto(m->cam, c, sizeof m->cam);
            m->tam = p->s_tamanhos[i];
            m->em_uso = true;
            m->uso = ++mini_reloj;
            imagem_t grande;
            if (imagem_ler(c, &grande) == 0) {
                int w = MINI_L, h = grande.alt * MINI_L / (grande.larg ? grande.larg : 1);
                if (h > MINI_A) { h = MINI_A; w = grande.larg * MINI_A / (grande.alt ? grande.alt : 1); }
                if (w < 1) w = 1;
                if (h < 1) h = 1;
                m->ok = imagem_escalar(&m->img, &grande, w, h, true);
                imagem_liberar(&grande);
            }
            m->falha = !m->ok;
            P = ant;
            registrar("arquivos: miniatura %s%s", c, m->ok ? "" : " (falla)");
            return true;
        }
        P = ant;
    }
    return false;
}

/* ---- el panel de vista previa (Alt+P) ---- */

static char previa_cam[200];           /* lo que muestra ahora */
static uint32_t previa_tam;
static bool previa_sucia = true;
static imagem_t previa_img;
static bool previa_tem_img;
static char previa_txt[1400];          /* texto (hasta 1400 bytes, con \n) */
static int previa_txt_n;
static bool previa_codigo;
static char previa_info[3][80];
static int previa_n_info;

static void previa_invalidar(void) { previa_sucia = true; }

static void previa_limpar(void)
{
    if (previa_tem_img) { imagem_liberar(&previa_img); previa_tem_img = false; }
    previa_txt_n = 0; previa_txt[0] = 0; previa_codigo = false; previa_n_info = 0;
}

static bool texto_legivel(const char *b, int n)
{
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)b[i];
        if (c == 0 || (c < 9) || (c > 13 && c < 32)) return false;
    }
    return true;
}

/* Carga lo que enseñará el panel para el elemento (ruta c). */
static void previa_carregar(const char *c, int tipo, uint32_t tam)
{
    previa_limpar();
    copia_texto(previa_cam, c, sizeof previa_cam);
    previa_tam = tam;
    if (tipo == TIPO_PASTA) {
        entrada_pasta_t e;
        int n = 0, sub = 0;
        for (int i = 0; i < 400 && listar(c, i, &e) == 1; i++) { n++; if (e.tipo == TIPO_PASTA) sub++; }
        formatar_texto(previa_info[0], sizeof previa_info[0], _("%d elementos"), n);
        formatar_texto(previa_info[1], sizeof previa_info[1], _("%d carpetas"), sub);
        previa_n_info = 2;
        return;
    }
    if (eh_imagem_arq(c) && tam <= 1536 * 1024) {
        imagem_t grande;
        if (imagem_ler(c, &grande) == 0) {
            formatar_texto(previa_info[0], sizeof previa_info[0], "%dx%d px", grande.larg, grande.alt);
            previa_n_info = 1;
            int bl = LARG_PREVIA - 16, ba = 150;
            int w = bl, h = grande.alt * bl / (grande.larg ? grande.larg : 1);
            if (h > ba) { h = ba; w = grande.larg * ba / (grande.alt ? grande.alt : 1); }
            if (w < 1) w = 1;
            if (h < 1) h = 1;
            previa_tem_img = imagem_escalar(&previa_img, &grande, w, h, true);
            imagem_liberar(&grande);
        }
        return;
    }
    if (tam > 0 && tam < 4 * 1024 * 1024) {
        int fd = abrir(c, ABRIR_LER);
        if (fd >= 0) {
            char b[sizeof previa_txt - 1];
            int n = ler(fd, b, (unsigned)sizeof b);
            fechar(fd);
            if (n > 0 && texto_legivel(b, n < 512 ? n : 512)) {
                memcpy(previa_txt, b, (size_t)n);
                previa_txt[n] = 0;
                previa_txt_n = n;
                previa_codigo = tem_ext(c, "c h py sh js json css html htm xml pas go rs asm mk cpp hpp java");
            }
        }
    }
}

/* Colorea un trozo de una línea de código: comentarios, cadenas, números y palabras clave. */
static bool palabra_clave(const char *p, int n)
{
    static const char *const K[] = { "if", "else", "for", "while", "return", "int", "char", "void", "static", "struct", "typedef", "const",
        "def", "class", "import", "from", "in", "not", "and", "or", "function", "var", "let", "unsigned", "break", "continue", "switch",
        "case", "enum", "include", "define", "true", "false", "NULL", "None", "True", "False", "do", "then", "fi", "esac", "elif" };
    for (unsigned k = 0; k < sizeof K / sizeof K[0]; k++)
        if ((int)tam_texto(K[k]) == n && compara_texto_n(K[k], p, (size_t)n) == 0) return true;
    return false;
}

static void linha_codigo(int x, int y, const char *l, int n, int max)
{
    bool coment = false;
    char q = 0;
    int col = 0;
    for (int i = 0; i < n && col < max; ) {
        char c = l[i];
        uint32_t cor = COR_TEXTO;
        int len = 1;
        if (coment) cor = 0x5A8A5A;
        else if (q) { cor = 0xA05010; if (c == q) q = 0; }
        else if (c == '"' || c == '\'') { q = c; cor = 0xA05010; }
        else if (c == '#' || (c == '/' && i + 1 < n && l[i + 1] == '/')) { coment = true; cor = 0x5A8A5A; }
        else if (c >= '0' && c <= '9') cor = 0x8030A0;
        else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            int f = i;
            while (f < n && ((l[f] >= 'a' && l[f] <= 'z') || (l[f] >= 'A' && l[f] <= 'Z') || l[f] == '_' || (l[f] >= '0' && l[f] <= '9'))) f++;
            len = f - i;
            if (palabra_clave(l + i, len)) cor = 0x2A4FA8;
            if (len > max - col) len = max - col;
            g_texto_n(j, x + col * LETRA_L, y, l + i, len, cor);
            col += len; i += len;
            continue;
        }
        g_texto_n(j, x + col * LETRA_L, y, &c, 1, cor);
        col++; i += len;
    }
}

static void desenhar_previa(void)
{
    if (!previa) return;
    int px = LARG - LARG_PREVIA, py = ALT_BARRA + ALT_ORDENS, ph = ALT - py - ALT_STATUS;
    g_retangulo(j, px, py, LARG_PREVIA, ph, sagui_esq.ar_previa);
    g_retangulo(j, px, py, 1, ph, COR_BORDA);
    int i = lista.sel;
    if (n_marcados > 1) {
        char t[64];
        formatar_texto(t, sizeof t, _("%d seleccionados"), n_marcados);
        g_texto_n(j, px + 8, py + 8, t, (LARG_PREVIA - 16) / LETRA_L, COR_TEXTO);
        jan_mostrar_area(j, px, py, LARG_PREVIA, ph);
        return;
    }
    if (i < 0 || i >= lista.qtd || tipos[i] == TIPO_GRUPO) {
        g_texto_n(j, px + 8, py + 8, _("Vista previa"), (LARG_PREVIA - 16) / LETRA_L, COR_TEXTO_FRACO);
        g_texto_n(j, px + 8, py + 28, _("Elige un archivo"), (LARG_PREVIA - 16) / LETRA_L, COR_TEXTO_FRACO);
        jan_mostrar_area(j, px, py, LARG_PREVIA, ph);
        return;
    }
    char c[200];
    caminho_de(i, c, sizeof c);
    if (previa_sucia || compara_texto(previa_cam, c) != 0 || previa_tam != tamanhos[i]) {
        previa_carregar(c, tipos[i], tamanhos[i]);
        previa_sucia = false;
    }
    int ancho_c = (LARG_PREVIA - 16) / LETRA_L;
    g_texto_n(j, px + 8, py + 6, nomes[i], ancho_c, COR_TEXTO);
    g_retangulo(j, px + 6, py + 24, LARG_PREVIA - 12, 1, sagui_esq.ar_linha);
    int y = py + 30, cuerpo_a = ph - 30 - 104;
    if (previa_tem_img) {
        imagem_encaixada(&previa_img, px + 8, y, LARG_PREVIA - 16, previa_img.alt);
        y += previa_img.alt + 4;
        for (int k = 0; k < previa_n_info; k++, y += 16) g_texto_n(j, px + 8, y, previa_info[k], ancho_c, COR_TEXTO_FRACO);
    } else if (previa_txt_n > 0) {
        int fila = 0, ini = 0, max_f = cuerpo_a / 16;
        for (int k = 0; k <= previa_txt_n && fila < max_f; k++) {
            if (previa_txt[k] == '\n' || previa_txt[k] == 0 || k - ini >= ancho_c * 3) {
                int len = k - ini;
                if (len > 0 && previa_txt[ini + len - 1] == '\r') len--;
                if (previa_codigo) linha_codigo(px + 8, y + fila * 16, previa_txt + ini, len, ancho_c);
                else g_texto_n(j, px + 8, y + fila * 16, previa_txt + ini, len < ancho_c ? len : ancho_c, COR_TEXTO);
                fila++;
                ini = (previa_txt[k] == '\n' || previa_txt[k] == 0) ? k + 1 : k;
                if (!previa_txt[k]) break;
            }
        }
    } else {
        icone_item(px + (LARG_PREVIA - 64) / 2, y + 6, i, 4);
        y += 80;
        for (int k = 0; k < previa_n_info; k++, y += 16) g_texto_n(j, px + 8, y, previa_info[k], ancho_c, COR_TEXTO_FRACO);
        if (tipos[i] != TIPO_PASTA && (pdf_extensao(c) || audio_extensao(c) || video_extensao(c))) {
            g_texto_n(j, px + 8, y + 4, pdf_extensao(c) ? _("Doble clic: Visor de PDF") : _("Doble clic: Reproductor"), ancho_c, COR_TEXTO_FRACO);
        }
    }
    /* las propiedades, abajo */
    int yp = py + ph - 100;
    g_retangulo(j, px + 6, yp - 4, LARG_PREVIA - 12, 1, sagui_esq.ar_linha);
    char t[96], quando[48], rwx[10];
    quando[0] = 0;
    datas_arquivo_t d;
    if (datas_arquivo(c, &d) == 0 && d.modificado) { fuso_data_t l; hora_local(d.modificado, &l); formatar_data(quando, sizeof quando, "%d/%m/%Y %H:%M", &l); }
    formatar_texto(t, sizeof t, "%s: %s", _("Tipo"), tipo_texto(i));
    g_texto_n(j, px + 8, yp, t, ancho_c, COR_TEXTO);
    if (tipos[i] == TIPO_PASTA) formatar_texto(t, sizeof t, "%s: -", _("Tamaño"));
    else formatar_texto(t, sizeof t, "%s: %u B", _("Tamaño"), tamanhos[i]);
    g_texto_n(j, px + 8, yp + 16, t, ancho_c, COR_TEXTO);
    formatar_texto(t, sizeof t, "%s: %s", _("Fecha"), quando[0] ? quando : "-");
    g_texto_n(j, px + 8, yp + 32, t, ancho_c, COR_TEXTO);
    info_permisos_t pm;
    if (permisos_arquivo(c, &pm) == 0) {
        char dono[CONTA_NOME_MAX];
        texto_modo(pm.modo, rwx);
        formatar_texto(t, sizeof t, "%s", rwx);
        g_texto_n(j, px + 8, yp + 48, t, ancho_c, COR_TEXTO);
        formatar_texto(t, sizeof t, "%s: %s", _("Dueño"), conta_nome(pm.uid, dono, sizeof dono));
        g_texto_n(j, px + 8, yp + 64, t, ancho_c, COR_TEXTO);
    }
    jan_mostrar_area(j, px, py, LARG_PREVIA, ph);
}

static void previa_atualizar(void)
{
    if (!previa) return;
    previa_sucia = false;   /* lo carga desenhar_previa si cambió el elemento */
    desenhar_previa();
}

/* ------------------------------------------------------------ vistas */

static const char *vista_nome(void)
{
    return vista == VISTA_DETALHES ? "detalhes" : vista == VISTA_ICONES ? "icones" : vista == VISTA_MOSAICOS ? "mosaicos" : "lista";
}

static void poner_vista(int v)
{
    vista = v;
    layout();
    lista.topo = 0;
    if (v != VISTA_DETALHES && v != VISTA_LISTA) { /* la rejilla no lleva cabeceras de grupo */ }
    guardar_preferencias();
    carregar();
    registrar("arquivos: vista %s", vista_nome());
}

static void mudar_ocultos(void)
{
    ver_ocultos = !ver_ocultos;
    em_busca = false;          /* cambiar la preferencia sale de la búsqueda */
    busca_cancelar();
    padrao_busca[0] = 0;
    guardar_preferencias();
    carregar();
    char msg[120];
    formatar_texto(msg, sizeof msg, _("Ocultos %s"), ver_ocultos ? _("visibles") : _("ocultos"));
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

static void mudar_agrupar(int g)
{
    agrupar = g;
    guardar_preferencias();
    carregar();
    registrar("arquivos: agrupar %s", g == AGR_TIPO ? "tipo" : g == AGR_DATA ? "data" : "nada");
}

/* El menú «Ver»: vistas, agrupar y ordenar (lo activo lleva un asterisco). */
static void menu_ver(int x, int y)
{
    char it[512];
    int k = 0;
    it[0] = 0;
#define LINEA(txt, on) do { if (k) it[k++] = '\n'; if (on) it[k++] = '*'; const char *q_ = (txt); while (*q_ && k < (int)sizeof it - 2) it[k++] = *q_++; it[k] = 0; } while (0)
    LINEA(_("Lista"), vista == VISTA_LISTA);
    LINEA(_("Detalles"), vista == VISTA_DETALHES);
    LINEA(_("Iconos"), vista == VISTA_ICONES);
    LINEA(_("Mosaicos"), vista == VISTA_MOSAICOS);
    LINEA("-", false);
    LINEA(_("Sin agrupar"), agrupar == AGR_NADA);
    LINEA(_("Agrupar por tipo"), agrupar == AGR_TIPO);
    LINEA(_("Agrupar por fecha"), agrupar == AGR_DATA);
    LINEA("-", false);
    LINEA(_("Ordenar por nombre"), ordem == ORD_NOME);
    LINEA(_("Ordenar por tamaño"), ordem == ORD_TAMANHO);
    LINEA(_("Ordenar por tipo"), ordem == ORD_TIPO);
    LINEA(_("Ordenar por fecha"), ordem == ORD_DATA);
#undef LINEA
    menu_contextual(j->id, x, y, it, MENU_VER);
}

static void escolha_ver(int idx)
{
    if (idx >= 0 && idx <= 3) poner_vista(idx == 0 ? VISTA_LISTA : idx == 1 ? VISTA_DETALHES : idx == 2 ? VISTA_ICONES : VISTA_MOSAICOS);
    else if (idx >= 5 && idx <= 7) mudar_agrupar(idx - 5);
    else if (idx >= 9 && idx <= 12) { ordem = idx - 9; ordem_desc = false; guardar_preferencias(); carregar(); }
}

/* ------------------------------------------------------------ "Abrir con" */

/* Los programas que pueden abrir un archivo. */
typedef struct { const char *nome; } app_t;
static const app_t apps[] = {
    { "editor" }, { "visor" }, { "paint" }, { "markdown" },
    { "mico" }, { "reproductor" }, { "musica" }, { "visorpdf" }, { "convertir-audio" }, { "tucan" }, { "jabuti" },
};

/* El programa que abriría este archivo (la asociación por defecto). */
/* La lista de «Abrir con...»: los de siempre y, si están instalados en /apps, los emuladores (D321). */
static const char *abrir_lista[16];
static int abrir_n;
static void preparar_lista_abrir(const char *arquivo)
{
    static const char *const EMUS[] = { "laines", "gearsystem" };
    abrir_n = 0;
    for (unsigned a = 0; a < sizeof apps / sizeof apps[0] && abrir_n < 14; a++) {
        char bin[40];
        if (compara_texto(apps[a].nome, "convertir-audio") == 0 && !audio_extensao(arquivo)) continue;   /* D339: solo para sonidos */
        if (ruta_programa(bin, sizeof bin, apps[a].nome)) abrir_lista[abrir_n++] = apps[a].nome;   /* D324: sin /apps no salen */
    }
    for (unsigned a = 0; a < sizeof EMUS / sizeof EMUS[0] && abrir_n < 16; a++) {
        char bin[40];
        info_arquivo_t inf;
        formatar_texto(bin, sizeof bin, "/apps/bin/%s", EMUS[a]);
        if (info_arquivo(bin, &inf) >= 0) abrir_lista[abrir_n++] = EMUS[a];
    }
}

static const char *programa_padrao(const char *c)
{
    bool md = tam_texto(c) > 3 && compara_texto(c + tam_texto(c) - 3, ".md") == 0;
    const char *p = imagem_extensao(c) ? "visor" : md ? "markdown" : "editor";
    if (rom_chip8(c)) p = "chip8";
    if (audio_extensao(c)) p = "musica";
    if (pdf_extensao(c)) p = "visorpdf";
    if (tucan_extensao(c)) p = "tucan";      /* D369 */
    if (video_extensao(c)) p = "reproductor";
    if (nes_extensao(c)) p = "laines";       /* D321 */
    if (sms_extensao(c)) p = "gearsystem";
    return p;
}

static void abrir_com(const char *prog, const char *arquivo)
{
    char bin[40];
    ruta_programa(bin, sizeof bin, prog);
    char *argv[3];
    argv[0] = (char *)prog; argv[1] = (char *)arquivo; argv[2] = NULL;
    int pid = executar(bin, argv);
    registrar("arquivos: abrir com %s %s (pid %d)", prog, arquivo, pid);
    char msg[200];
    formatar_texto(msg, sizeof msg, pid > 0 ? _("Abriendo %s con %s...") : _("No se ha podido abrir %s"), nome_base(arquivo), prog);
    desenhar_rodape(msg);
}

/* El menú "Abrir con...": el programa de siempre sale marcado con un asterisco. */
static void menu_abrir_com(void)
{
    int i = lista.sel;
    if (i < 0 || i >= lista.qtd) return;
    if (tipos[i] == TIPO_PASTA) { mensagem(_("Abrir con"), _("Una carpeta se abre, no se abre con un programa.")); return; }
    char c[200];
    caminho_de(i, c, sizeof c);
    const char *padrao = programa_padrao(c);
    char itens[MENU_MAX * 24];
    int k = 0;
    itens[0] = 0;
    preparar_lista_abrir(c);
    for (int a = 0; a < abrir_n; a++) {
        const char *p = abrir_lista[a];
        if (k) itens[k++] = '\n';
        if (compara_texto(p, padrao) == 0) itens[k++] = '*';
        while (*p && k < (int)sizeof itens - 2) itens[k++] = *p++;
        itens[k] = 0;
    }
    itens[k] = 0;
    menu_contextual(j->id, lista.x + 20, lista.y + 20, itens, MENU_COM);
}

/* ------------------------------------------------------------ menú contextual */

/* Las mismas líneas de siempre (D051 y la mejora 056); las nuevas (D420) van al final. */
enum { MI_ABRIR, MI_RENOMEAR, MI_APAGAR, MI_SEP, MI_PROPRIEDADES, MI_SEP2, MI_ZIP, MI_TGZ, MI_EXTRAIR, MI_EXTRAIR_EN,
       MI_SEP3, MI_ABRIR_COM, MI_COPIAR, MI_CORTAR, MI_FAV, MI_ETIQUETA, MI_TERMINAL };
/* As linhas do menu de grupo (D139). */
enum { MG_ABRIR, MG_COPIAR, MG_CORTAR, MG_SEP, MG_BORRAR, MG_PROPRIEDADES, MG_SEP2, MG_ZIP, MG_TGZ };
enum { MF_NOVA_PASTA, MF_ATUALIZAR, MF_SEP, MF_VOLTAR, MF_SEP2, MF_NOVO_ARQUIVO, MF_COLAR, MF_VER, MF_TERMINAL };

/* El elemento que hay bajo el ratón (según la vista); -1 si no hay ninguno o es una cabecera de grupo. */
static int item_de_ponto(int x, int y)
{
    if (GRELHA) {
        int por_linha = grade_colunas();
        int linha = (y - lista.y - 4) / celula_a(), coluna = (x - lista.x - 6) / celula_l();
        if (y - lista.y - 4 < 0 || x - lista.x - 6 < 0 || linha < 0 || coluna < 0 || coluna >= por_linha) return -1;
        int i = lista.topo + linha * por_linha + coluna;
        return i < lista.qtd && tipos[i] != TIPO_GRUPO ? i : -1;
    }
    int i = lista.topo + (y - lista.y - 2) / LISTA_ALTURA_ITEM;
    return i >= 0 && i < lista.qtd && y >= lista.y + 2 && tipos[i] != TIPO_GRUPO ? i : -1;
}

static bool dentro_lista(int x, int y)
{
    return x >= lista.x && x < lista.x + lista.l && y >= lista.y && y < lista.y + lista.a;
}

static void abrir_menu(const evento_t *e)
{
    if (!dentro_lista(e->x, e->y)) return;
    if (compara_texto(pasta, INICIO_RUTA) == 0) return;
    int i = item_de_ponto(e->x, e->y);
    if (i >= 0) {
        if (marcados[i] && n_marcados > 1) {   /* o menu do grupo (D139) */
            lista.sel = i;
            desenhar_lista();
            char itens[240];
            formatar_texto(itens, sizeof itens, "%s (%d)\n%s\n%s\n-\n%s (%d)\n%s (%d)\n-\n%s\n%s",
                           _("Abrir"), n_marcados, _("Copiar"), _("Cortar"), _("Borrar"), n_marcados, _("Propiedades"), n_marcados,
                           _("Comprimir en .zip"), _("Comprimir en .tar.gz"));
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
        previa_atualizar();
        registrar("arquivos: menu do item %s", nomes[i]);
        char itens[512];
        bool descomp = eh_comprimido(nomes[i]) && tipos[i] != TIPO_PASTA;
        formatar_texto(itens, sizeof itens, "%s\n%s\n%s\n-\n%s\n-\n%s\n%s\n%s%s\n%s%s\n-\n%s%s\n%s\n%s\n%s%s\n%s\n%s", _("Abrir"), _("Renombrar"), _("Borrar"),
                       _("Propiedades"), _("Comprimir en .zip"), _("Comprimir en .tar.gz"),
                       descomp ? "" : "~", _("Extraer aquí"), descomp ? "" : "~", _("Extraer en carpeta..."),
                       tipos[i] == TIPO_PASTA ? "~" : "", _("Abrir con..."), _("Copiar"), _("Cortar"),
                       tipos[i] == TIPO_PASTA ? "" : "~", _("Anclar a Favoritos"), _("Etiqueta de color..."), _("Abrir terminal aquí"));
        menu_contextual(j->id, e->x, e->y, itens, MENU_ITEM);
    } else {
        registrar("arquivos: menu do fundo");
        char itens[320];
        formatar_texto(itens, sizeof itens, "%s\n%s\n-\n%s%s\n-\n%s\n%s%s\n%s\n%s", _("Nueva carpeta"), _("Actualizar"),
                       compara_texto(pasta, "/") == 0 ? "~" : "", _("Atrás"),
                       _("Nuevo archivo de texto"), area_n ? "" : "~", _("Pegar"), _("Ver..."), _("Abrir terminal aquí"));
        menu_contextual(j->id, e->x, e->y, itens, MENU_FUNDO);
    }
}

/* ---- nuevo archivo, terminal, favoritos, etiquetas ---- */

static void novo_arquivo(void)
{
    if (compara_texto(pasta, INICIO_RUTA) == 0) return;
    char nome[64] = "nuevo.txt";
    if (!pedir_texto(_("Nuevo archivo"), _("Nombre del nuevo archivo:"), nome, sizeof nome) || !nome[0]) return;
    char c[200];
    caminho_juntar(c, sizeof c, pasta, nome);
    info_arquivo_t inf;
    if (info_arquivo(c, &inf) == 0) { mensagem(_("Archivos"), _("Ya existe un elemento con ese nombre.")); return; }
    int fd = abrir(c, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { mensagem(_("Archivos"), _("No se ha podido crear el archivo.")); return; }
    fechar(fd);
    registrar("arquivos: arquivo criado %s", c);
    carregar();
}

static void abrir_terminal(void)
{
    char bin[40];
    ruta_programa(bin, sizeof bin, "terminal");
    char *argv[2] = { "terminal", NULL };
    char cwd[200];
    pasta_atual(cwd, sizeof cwd);
    mudar_pasta(compara_texto(pasta, INICIO_RUTA) == 0 ? "/" : pasta);
    int pid = executar(bin, argv);
    mudar_pasta(cwd);
    registrar("arquivos: terminal em %s (pid %d)", pasta, pid);
    desenhar_rodape(pid > 0 ? _("Terminal abierto en esta carpeta") : _("No se ha podido abrir el terminal"));
}

static void anclar_favorito(void)
{
    char c[200];
    int i = lista.sel;
    if (i >= 0 && i < lista.qtd && tipos[i] == TIPO_PASTA) caminho_de(i, c, sizeof c);
    else copia_texto(c, pasta, sizeof c);
    if (compara_texto(c, INICIO_RUTA) == 0) return;
    bool ok = fav_anadir(c);
    desenhar_lateral();
    desenhar_rodape(ok ? _("Anclado a Favoritos") : _("Ya está en Favoritos (o no cabe más)"));
}

static void menu_etiquetas(int x, int y)
{
    char it[200];
    int k = 0;
    it[0] = 0;
    for (int i = 0; i < N_CORES; i++) {
        const char *q = _(NOME_ETQ[i]);
        if (k) it[k++] = '\n';
        while (*q && k < (int)sizeof it - 2) it[k++] = *q++;
    }
    it[k++] = '\n';
    const char *q = _("Quitar etiqueta");
    while (*q && k < (int)sizeof it - 2) it[k++] = *q++;
    it[k] = 0;
    menu_contextual(j->id, x, y, it, MENU_ETIQ);
}

static void aplicar_etiqueta(int idx)
{
    if (idx < 0 || idx > N_CORES) return;
    int n = 0;
    for (int i = 0; i < lista.qtd; i++) {
        if (!marcados[i] || tipos[i] == TIPO_GRUPO) continue;
        char c[200];
        caminho_de(i, c, sizeof c);
        etiqueta_poner(c, idx == N_CORES ? -1 : idx);
        n++;
    }
    guardar_etiquetas();
    registrar("arquivos: etiqueta %s en %d elementos", idx == N_CORES ? "ninguna" : "color", n);
    if (em_busca && comeca_com(padrao_busca, "@etq")) mostrar_etiqueta(padrao_busca[4] - '0');
    else desenhar_lista();
}

/* ---- las subcarpetas de una miga de pan (la flecha) ---- */
static char miga_base[160];
static char miga_sub[20][64];
static int miga_n;

static void menu_miga(const char *cam, int x, int y)
{
    char it[20 * 66];
    int k = 0;
    miga_n = 0;
    entrada_pasta_t e;
    for (int i = 0; miga_n < 20 && listar(cam, i, &e) == 1; i++) {
        if (e.tipo != TIPO_PASTA || (eh_oculto(e.nome) && !ver_ocultos)) continue;
        copia_texto(miga_sub[miga_n++], e.nome, sizeof miga_sub[0]);
    }
    if (!miga_n) { desenhar_rodape(_("Sin subcarpetas")); return; }
    copia_texto(miga_base, cam, sizeof miga_base);
    it[0] = 0;
    for (int i = 0; i < miga_n; i++) {
        if (i) it[k++] = '\n';
        for (const char *q = miga_sub[i]; *q && k < (int)sizeof it - 2; q++) it[k++] = *q;
    }
    it[k] = 0;
    menu_contextual(j->id, x, y, it, MENU_MIGA);
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
        else if (indice == MI_ABRIR_COM) menu_abrir_com();
        else if (indice == MI_COPIAR) copiar_area(false);
        else if (indice == MI_CORTAR) copiar_area(true);
        else if (indice == MI_FAV) anclar_favorito();
        else if (indice == MI_ETIQUETA) menu_etiquetas(lista.x + 40, lista.y + 30);
        else if (indice == MI_TERMINAL) abrir_terminal();
    } else if (codigo == MENU_FUNDO) {
        if (indice == MF_NOVA_PASTA) nova_pasta();
        else if (indice == MF_ATUALIZAR) { git_atualizar(true); carregar(); }
        else if (indice == MF_VOLTAR) voltar();
        else if (indice == MF_NOVO_ARQUIVO) novo_arquivo();
        else if (indice == MF_COLAR) colar_area();
        else if (indice == MF_VER) menu_ver(lista.x + 40, lista.y + 30);
        else if (indice == MF_TERMINAL) abrir_terminal();
    } else if (codigo == MENU_GRUPO) {
        if (indice == MG_ABRIR) abrir_grupo();
        else if (indice == MG_COPIAR) copiar_area(false);
        else if (indice == MG_CORTAR) copiar_area(true);
        else if (indice == MG_BORRAR) apagar_item();
        else if (indice == MG_PROPRIEDADES) propriedades_grupo();
        else if (indice == MG_ZIP) comprimir_grupo(false);
        else if (indice == MG_TGZ) comprimir_grupo(true);
    } else if (codigo == MENU_COM) {
        if (indice < 0 || indice >= abrir_n) return;
        int i = lista.sel;
        if (i < 0 || i >= lista.qtd) return;
        char c[200];
        caminho_de(i, c, sizeof c);
        abrir_com(abrir_lista[indice], c);
    } else if (codigo == MENU_NUEVO) {
        if (indice == 0) nova_pasta(); else if (indice == 1) novo_arquivo();
    } else if (codigo == MENU_VER) escolha_ver(indice);
    else if (codigo == MENU_ETIQ) aplicar_etiqueta(indice);
    else if (codigo == MENU_FILTRO) aplicar_filtro(indice);
    else if (codigo == MENU_MIGA) {
        if (indice < miga_n) { char c[200]; caminho_juntar(c, sizeof c, miga_base, miga_sub[indice]); entrar(c, true); }
    } else if (codigo == MENU_FAV) {
        if (indice == 0) { fav_quitar(lado_fav_menu); desenhar_lateral(); }
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
            mensagem(_("Archivos"), r == ERRO_OCUPADO ? _("El CD/DVD está en uso (¿el instalador?): no se expulsa.")
                                                   : _("No se ha podido expulsar el CD/DVD."));
            return;
        }
        copia_texto(pasta, "/", sizeof pasta);
        atualizar_cd();
        carregar();
        return;
    }
    if (!cdrom.montado) {
        int r = montar_em(cdrom.nome, "/cd", MONTAR_SO_LEITURA);
        if (r < 0) { char m[120]; formatar_texto(m, sizeof m, _("No se ha podido montar el CD/DVD:\n%s"), nome_erro(r)); mensagem(_("Archivos"), m); return; }
        atualizar_cd();
    }
    entrar(cdrom.ponto[0] ? cdrom.ponto : "/cd", true);
    registrar("arquivos: cd aberto");
}

/* ------------------------------------------------------------ barra lateral: acciones */

static void acao_lado(int acao)
{
    if (acao == LUG_CD) { acao_cd(); return; }
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
            mensagem(_("Archivos"), r < 0 ? _("No se ha podido desmontar el pendrive.") : _("Ya puedes retirar el pendrive."));
            carregar();
        } else {
            int r = montar(pendrive.nome);
            if (r < 0) { char m[120]; formatar_texto(m, sizeof m, _("No se ha podido montar el pendrive:\n%s"), nome_erro(r)); mensagem(_("Archivos"), m); }
            else {
                atualizar_pendrive();
                desenhar_lateral();
                entrar(pendrive.ponto[0] ? pendrive.ponto : "/usb", true);
                registrar("arquivos: pendrive aberto");
            }
        }
        return;
    }
}

/* Clic en una fila de la barra lateral. */
static void clic_lado(int idx, int x)
{
    lado_t *l = &lado[idx];
    if (l->tipo == LT_SEC) return;
    if (l->acao == LUG_INICIO) { entrar(INICIO_RUTA, true); return; }
    if (l->acao == LUG_FAV || l->acao == LUG_UNID || l->acao == LUG_LIXEIRA) {
        if (!existe_pasta(l->cam)) { desenhar_rodape(_("Esa carpeta ya no existe")); return; }
        entrar(l->cam, true);
        return;
    }
    if (l->acao == LUG_ETQ) { int c = (x - 8) / 20; if (c >= 0 && c < N_CORES) mostrar_etiqueta(c); return; }
    acao_lado(l->acao);
}

/* ------------------------------------------------------------ selección */

/* Marca o desmarca un elemento (Ctrl). */
static void alternar_marca(int i)
{
    if (i < 0 || i >= lista.qtd || tipos[i] == TIPO_GRUPO) return;
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
    for (int k = a; k <= b; k++) if (tipos[k] != TIPO_GRUPO) { marcados[k] = true; n_marcados++; }
}

/* El recuadro de selección con el ratón. */
static bool arrasto_ativo;
static int arrasto_x, arrasto_y;

static void marcar_rectangulo(int x, int y)
{
    int x0 = arrasto_x < x ? arrasto_x : x, x1 = arrasto_x < x ? x : arrasto_x;
    int y0 = arrasto_y < y ? arrasto_y : y, y1 = arrasto_y < y ? y : arrasto_y;
    limpar_marcados();
    if (GRELHA) {   /* a grelha (D139) */
        int col = grade_colunas(), lin = grade_linhas();
        for (int k = 0; k < lin; k++)
            for (int c = 0; c < col; c++) {
                int i = lista.topo + k * col + c;
                if (i >= lista.qtd) break;
                int cx = lista.x + 6 + c * celula_l(), cy = lista.y + 4 + k * celula_a();
                if (cy + celula_a() - 6 < y0 || cy > y1) continue;
                if (cx + celula_l() - 4 < x0 || cx > x1) continue;
                if (tipos[i] == TIPO_GRUPO) continue;
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
        if (tipos[i] == TIPO_GRUPO) continue;
        marcados[i] = true;
        n_marcados++;
    }
}

/* ------------------------------------------------------------ arrastrar y soltar */

/* Se arrastran los elementos marcados a otro panel, a una pestaña, a una carpeta de la lista o de la barra
 * lateral (copia; con Mayús, mueve), a «Favoritos» (una carpeta se ancla) o a una etiqueta de color. */
enum { DD_NADA, DD_PAINEL, DD_ABA, DD_LADO, DD_PASTA, DD_FAV, DD_ETQ };
static struct {
    bool premido, ativo;
    int x0, y0;
    int origem;            /* panel de donde sale */
    int tipo, painel, idx; /* destino actual */
    char dest[160];
    int soltar_item;       /* elemento que se queda solo al soltar sin haber arrastrado (-1 = ninguno) */
} dnd;
static int dnd_item = -1;  /* carpeta de la lista resaltada como destino */

static int painel_em(int x, int y)
{
    if (y < ALT_BARRA + ALT_ORDENS || y >= ALT - ALT_STATUS) return -1;
    for (int k = 0; k < n_paineis; k++) if (x >= paineis[k].x && x < paineis[k].x + paineis[k].l) return k;
    return -1;
}

static bool destino_valido(const char *d)
{
    return d[0] == '/' && existe_pasta(d);
}

static void dnd_calcular(int x, int y)
{
    int t = DD_NADA, pn = -1, idx = -1;
    char d[160];
    d[0] = 0;
    int li = lado_em(x, y);
    if (li >= 0) {
        lado_t *l = &lado[li];
        if (l->tipo == LT_SEC && l->texto[0] == 'F') t = DD_FAV;
        else if (l->acao == LUG_ETQ) { idx = (x - 8) / 20; if (idx >= 0 && idx < N_CORES) t = DD_ETQ; }
        else if (l->acao == LUG_FAV && destino_valido(l->cam)) {
            t = DD_LADO; idx = li; copia_texto(d, l->cam, sizeof d);
        } else if ((l->acao == LUG_UNID || l->acao == LUG_LIXEIRA) && destino_valido(l->cam)) { t = DD_LADO; idx = li; copia_texto(d, l->cam, sizeof d); }
    } else {
        for (int k = 0; k < n_paineis && t == DD_NADA; k++) {
            painel_t *p = &paineis[k];
            if (p->n_abas > 1) {
                int a = abas_em(&p->barra_abas, x, y);
                if (a >= 0) {
                    t = DD_ABA; pn = k; idx = a;
                    copia_texto(d, a == p->aba_sel ? p->s_pasta : p->abas[a].a_pasta, sizeof d);
                    if (!destino_valido(d)) t = DD_NADA;
                }
            }
        }
        if (t == DD_NADA) {
            int k = painel_em(x, y);
            if (k >= 0) {
                painel_t *ant = P;
                P = &paineis[k];
                if (compara_texto(pasta, INICIO_RUTA) != 0 && !em_busca && dentro_lista(x, y)) {
                    int i = item_de_ponto(x, y);
                    if (i >= 0 && tipos[i] == TIPO_PASTA && !(k == dnd.origem && marcados[i])) {
                        t = DD_PASTA; pn = k; idx = i;
                        caminho_de(i, d, sizeof d);
                    } else if (k != dnd.origem) { t = DD_PAINEL; pn = k; copia_texto(d, pasta, sizeof d); }
                }
                P = ant;
            }
        }
    }
    bool igual = t == dnd.tipo && pn == dnd.painel && idx == dnd.idx;
    dnd.tipo = t; dnd.painel = pn; dnd.idx = idx;
    copia_texto(dnd.dest, d, sizeof dnd.dest);
    if (igual) return;
    /* resaltado */
    lado_destaque = (t == DD_LADO || t == DD_FAV || t == DD_ETQ) ? li : -1;
    dnd_painel_alvo = (t == DD_PAINEL) ? pn : (t == DD_PASTA) ? pn : -1;
    dnd_item = t == DD_PASTA ? idx : -1;
    for (int k = 0; k < n_paineis; k++) paineis[k].barra_abas.destaque = (t == DD_ABA && pn == k) ? idx : -1;
    desenhar_lateral();
    for (int k = 0; k < n_paineis; k++) desenhar_painel(k);
    char msg[160];
    if (t == DD_NADA) formatar_texto(msg, sizeof msg, _("Arrastrando %d elementos"), P->s_n_marcados);
    else if (t == DD_FAV) copia_texto(msg, _("Suelta aquí para anclar la carpeta a Favoritos"), sizeof msg);
    else if (t == DD_ETQ) formatar_texto(msg, sizeof msg, _("Suelta aquí para ponerles la etiqueta %s"), _(NOME_ETQ[idx]));
    else formatar_texto(msg, sizeof msg, _("Soltar en %s (Mayús: mover)"), d);
    desenhar_rodape(msg);
}

static void dnd_limpiar(void)
{
    dnd.ativo = dnd.premido = false;
    dnd.tipo = DD_NADA; dnd.painel = -1; dnd.idx = -1;
    lado_destaque = -1; dnd_painel_alvo = -1; dnd_item = -1;
    for (int k = 0; k < n_paineis; k++) paineis[k].barra_abas.destaque = -1;
}

/* Copia (o mueve) los elementos marcados del panel activo a la carpeta dest. */
static void soltar_em(const char *dest, bool mover)
{
    int n = 0, falhas = 0;
    char origem_dir[160];
    copia_texto(origem_dir, pasta, sizeof origem_dir);
    for (int i = 0; i < lista.qtd; i++) {
        if (!marcados[i] || tipos[i] == TIPO_GRUPO) continue;
        char orig[200], destino[200];
        caminho_de(i, orig, sizeof orig);
        caminho_juntar(destino, sizeof destino, dest, nomes[i]);
        if (compara_texto(orig, destino) == 0) continue;
        size_t ol = tam_texto(orig);
        if (tipos[i] == TIPO_PASTA && compara_texto_n(dest, orig, ol) == 0 && (dest[ol] == '/' || dest[ol] == 0)) { falhas++; continue; }
        info_arquivo_t inf;
        if (info_arquivo(destino, &inf) == 0) {
            char m[200];
            formatar_texto(m, sizeof m, _("%s ya existe.\n¿Reemplazarlo?"), nomes[i]);
            if (!confirmar(mover ? _("Mover") : _("Copiar"), m)) continue;
        }
        desenhar_rodape(mover ? _("Moviendo...") : _("Copiando..."));
        if (!copiar_ficheiro(orig, destino)) { falhas++; continue; }
        n++;
        if (mover) {
            if (tipos[i] == TIPO_PASTA) remover_pasta(orig); else apagar(orig);
        }
    }
    registrar("arquivos: soltados %d itens em %s (%s)", n, dest, mover ? "mover" : "copiar");
    int act = (int)(P - paineis);
    for (int k = 0; k < n_paineis; k++) {
        usar_painel(k);
        if (compara_texto(pasta, dest) == 0 || (mover && compara_texto(pasta, origem_dir) == 0)) { git_atualizar(true); carregar(); }
    }
    usar_painel(act);
    char msg[160];
    if (falhas) formatar_texto(msg, sizeof msg, _("%d elementos %s (%d con error)"), n, mover ? _("movidos") : _("copiados"), falhas);
    else formatar_texto(msg, sizeof msg, _("%d elementos %s a %s"), n, mover ? _("movidos") : _("copiados"), dest);
    desenhar_rodape(msg);
}

static void dnd_soltar(bool mover)
{
    int t = dnd.tipo;
    char d[160];
    copia_texto(d, dnd.dest, sizeof d);
    int idx = dnd.idx;
    dnd_limpiar();
    desenhar_lateral();
    for (int k = 0; k < n_paineis; k++) desenhar_painel(k);
    if (t == DD_FAV) {
        int i = lista.sel;
        int n = 0;
        for (int k = 0; k < lista.qtd; k++) {
            if (!marcados[k] || tipos[k] != TIPO_PASTA) continue;
            char c[200];
            caminho_de(k, c, sizeof c);
            if (fav_anadir(c)) n++;
        }
        (void)i;
        desenhar_lateral();
        desenhar_rodape(n ? _("Anclado a Favoritos") : _("Solo se pueden anclar carpetas (y no repetidas)"));
    } else if (t == DD_ETQ) {
        for (int k = 0; k < lista.qtd; k++) {
            if (!marcados[k] || tipos[k] == TIPO_GRUPO) continue;
            char c[200];
            caminho_de(k, c, sizeof c);
            etiqueta_poner(c, idx);
        }
        guardar_etiquetas();
        registrar("arquivos: etiqueta %d puesta arrastrando", idx);
        desenhar_lista();
    } else if (t != DD_NADA && d[0]) soltar_em(d, mover);
}

/* ------------------------------------------------------------ autocompletar la ruta */

static char autoc_dir[160];
static char autoc_nome[16][64];
static int autoc_n;

static void autocompletar(void)
{
    char tx[160];
    copia_texto(tx, caixa.texto, sizeof tx);
    if (tx[0] != '/') {   /* relativa a la carpeta actual */
        char v[160];
        caminho_juntar(v, sizeof v, compara_texto(pasta, INICIO_RUTA) == 0 ? "/" : pasta, tx);
        copia_texto(tx, v, sizeof tx);
    }
    char *b = acha_ultimo_char(tx, '/');
    if (!b) return;
    char prefijo[64];
    copia_texto(prefijo, b + 1, sizeof prefijo);
    if (b == tx) copia_texto(autoc_dir, "/", sizeof autoc_dir); else { *b = 0; copia_texto(autoc_dir, tx, sizeof autoc_dir); }
    size_t pl = tam_texto(prefijo);
    autoc_n = 0;
    entrada_pasta_t e;
    for (int i = 0; autoc_n < 16 && listar(autoc_dir, i, &e) == 1; i++) {
        if (e.tipo != TIPO_PASTA) continue;
        if (pl && compara_texto_n(e.nome, prefijo, pl) != 0) continue;
        if (!pl && eh_oculto(e.nome) && !ver_ocultos) continue;
        copia_texto(autoc_nome[autoc_n++], e.nome, sizeof autoc_nome[0]);
    }
    if (!autoc_n) { desenhar_rodape(_("Sin coincidencias")); return; }
    char r[200];
    /* el prefijo común más largo */
    char comum[64];
    copia_texto(comum, autoc_nome[0], sizeof comum);
    for (int i = 1; i < autoc_n; i++) {
        size_t k = 0;
        while (comum[k] && autoc_nome[i][k] == comum[k]) k++;
        comum[k] = 0;
    }
    if (autoc_n == 1 || tam_texto(comum) > pl) {
        caminho_juntar(r, sizeof r, autoc_dir, comum);
        if (autoc_n == 1) junta_texto(r, "/", sizeof r);
        caixa_definir(&caixa, r);
        desenhar_rota_painel(P);
        return;
    }
    char it[16 * 66];
    int k = 0;
    for (int i = 0; i < autoc_n; i++) {
        if (i) it[k++] = '\n';
        for (const char *q = autoc_nome[i]; *q && k < (int)sizeof it - 2; q++) it[k++] = *q;
    }
    it[k] = 0;
    menu_contextual(j->id, caixa.x, caixa.y + 18, it, MENU_AUTOC);
}

/* ------------------------------------------------------------ principal */

/* D428: otro esquema de apariencia: se repinta todo con los colores nuevos. */
static void repintar_tema(void)
{
    desenhar_tudo();
    jan_mostrar(j);
}

static void novo_tamanho(void)
{
    layout();
    for (int k = 0; k < n_paineis; k++) {
        painel_t *p = &paineis[k];
        int vis = (p->s_lista.a - 4) / LISTA_ALTURA_ITEM;
        if (p->s_lista.sel >= p->s_lista.topo + vis) p->s_lista.topo = p->s_lista.sel - vis + 1;
        if (p->s_lista.topo > p->s_lista.qtd - vis) p->s_lista.topo = p->s_lista.qtd - vis;
        if (p->s_lista.topo < 0) p->s_lista.topo = 0;
    }
    desenhar_tudo();
    jan_mostrar(j);
    registrar("arquivos: janela %dx%d, %d itens visiveis", LARG, ALT, (lista.a - 4) / LISTA_ALTURA_ITEM);
}

/* Después de un clic o una tecla que mueve la selección: si cae en una cabecera de grupo, pasa al
 * siguiente elemento de verdad (o al anterior si no hay más). */
static void sel_sin_cabecalho(int dir)
{
    if (lista.sel < 0 || lista.sel >= lista.qtd || tipos[lista.sel] != TIPO_GRUPO) return;
    int k = lista.sel;
    while (k >= 0 && k < lista.qtd && tipos[k] == TIPO_GRUPO) k += dir;
    if (k < 0 || k >= lista.qtd) { k = lista.sel; while (k >= 0 && k < lista.qtd && tipos[k] == TIPO_GRUPO) k -= dir; }
    if (k >= 0 && k < lista.qtd) lista.sel = k;
}

static void editar_ruta(void)
{
    modo_caixa = MODO_ROTA;
    caixa_definir(&caixa, compara_texto(pasta, INICIO_RUTA) == 0 ? "/" : pasta);
    caixa.foco = true;
    desenhar_rota_painel(P);
    registrar("arquivos: editar ruta (%s) panel=%d pasta=%s busca=%d", caixa.texto, ativo + 1, pasta, em_busca);
}

static void iniciar_busca(void)
{
    modo_caixa = MODO_BUSCA;
    caixa_definir(&caixa, filtro);
    caixa.foco = true;
    desenhar_rota_painel(P);
}

/* La «x» del campo de búsqueda (o Esc dos veces): quita el filtro y la búsqueda. */
static void limpar_busca(void)
{
    caixa.foco = false;
    caixa_definir(&caixa, "");
    filtro[0] = 0;
    if (busca.ativa || em_busca) { em_busca = false; busca_cancelar(); padrao_busca[0] = 0; }
    carregar();
    desenhar_rota_painel(P);
}

/* El tooltip según lo que haya bajo el ratón: botones de la primera fila, vistas, órdenes y búsqueda. */
static void dica_atualizar(int mx, int my)
{
    int id = -1, x = 0, y = 0, l = 0, a = 0;
    const char *t = NULL;
    for (int i = 0; i < BN_N && id < 0; i++)
        if (bt_vis[i] && mx >= bt[i].x && mx < bt[i].x + bt[i].l && my >= bt[i].y && my < bt[i].y + bt[i].a) { id = i; t = bt[i].dica; x = bt[i].x; y = bt[i].y; l = bt[i].l; a = bt[i].a; }
    if (id < 0 && seg_vis) {
        int k = segmentado_em(&seg_vistas, mx, my);
        if (k >= 0) { id = 20 + k; t = seg_vistas.dicas[k]; segmentado_rect(&seg_vistas, k, &x, &y, &l, &a); }
    }
    if (id < 0) {
        int k = ordem_em(mx, my);
        if (k >= 0) { id = 40 + k; t = ORDENS[k].texto; x = ordens_x[k]; y = ALT_BARRA + 2; l = ORDEN_L; a = ALT_ORDENS - 5; }
    }
    if (id < 0 && busca_l > 0 && mx >= busca_x && mx < busca_x + busca_l && my >= BARRA_Y && my < BARRA_Y + BARRA_A && !caixa.foco) {
        id = 60; t = N_("Buscar archivos (Ctrl+F)"); x = busca_x; y = BARRA_Y; l = busca_l; a = BARRA_A;
    }
    dica_mover(j, &dica, id, t, x, y, l, a);
}

static void ordem_executar(int k)
{
    switch (k) {
    case OR_NUEVO: {
        char it[64];
        int k = 0;
        const char *a = _("Carpeta"), *b = _("Archivo de texto");
        while (*a && k < 30) it[k++] = *a++;
        it[k++] = '\n';
        while (*b && k < (int)sizeof it - 1) it[k++] = *b++;
        it[k] = 0;
        menu_contextual(j->id, ordens_x[OR_NUEVO], ALT_BARRA + ALT_ORDENS, it, MENU_NUEVO);
        break;
    }
    case OR_CORTAR: copiar_area(true); break;
    case OR_COPIAR: copiar_area(false); break;
    case OR_PEGAR: colar_area(); break;
    case OR_RENOMBRAR: renomear_item(); break;
    case OR_COMPRIMIR: if (n_marcados > 1) comprimir_grupo(false); else comprimir_item(false); break;
    case OR_EXTRAER: extrair_item(false); break;
    case OR_TERMINAL: abrir_terminal(); break;
    case OR_PROPS: if (n_marcados > 1) propriedades_grupo(); else propriedades_item(); break;
    case OR_VER: menu_ver(ordens_x[OR_VER], ALT_BARRA + ALT_ORDENS); break;
    case OR_DOBLE: painel_doble(); break;
    case OR_PREVIA: previa = !previa; guardar_preferencias(); layout(); desenhar_tudo(); registrar("arquivos: previa %s", previa ? "sim" : "nao"); break;
    case OR_PESTANA: aba_nova(INICIO_RUTA); break;
    case OR_INICIO: entrar(INICIO_RUTA, true); break;
    case OR_FAV: anclar_favorito(); break;
    case OR_ETIQUETA: if (n_marcados > 0) menu_etiquetas(ordens_x[OR_ETIQUETA], ALT_BARRA + ALT_ORDENS); else desenhar_rodape(_("No hay nada seleccionado")); break;
    default: break;
    }
}

int principal(int argc, char **argv)
{
    painel_iniciar(&paineis[0], "/");
    P = &paineis[0];
    if (argc > 1) copia_texto(pasta, argv[1], sizeof pasta);
    ler_preferencias();
    ler_area();
    ler_fav();
    ler_reciente();
    ler_etiquetas();
    j = jan_abrir(_("Archivos"), LARG_INICIO, ALT_INICIO);
    tema_ao_mudar(repintar_tema);   /* D428 */
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL, 420, 260);
    /* La carpeta de arranque es el primer paso del historial (el botón "Atrás"). */
    copia_texto(hist[0], pasta, sizeof hist[0]);
    atualizar_pendrive();
    atualizar_cd();
    caixa.desenhar = caixa_gancho;
    barra_iniciar();
    layout();
    carregar();
    jan_mostrar(j);
    registrar("arquivos: pronto");

    evento_t e;
    unsigned ultimo = tempo_ms();
    int hover_ordem = -1;
    dnd.painel = -1; dnd.idx = -1; dnd.soltar_item = -1;
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
            dica_tic(j, &dica);
            busca_tic();
            if (miniatura_gerar()) { for (int k = 0; k < n_paineis; k++) { painel_t *a = P; P = &paineis[k]; desenhar_lista(); P = a; } continue; }
            dormir(busca.ativa ? 2 : 50);
            continue;
        }
        int c;
        while (recolher(&c) > 0) ;   /* programas abertos desde acá que ya terminaron */
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_MOVER) dica_atualizar(e.x, e.y);
        else if (e.tipo == EV_CLIQUE || e.tipo == EV_TECLA || e.tipo == EV_RODA) dica_ocultar(j, &dica);
        if (jan_redimensionar(j, &e)) { dica_ocultar(j, &dica); novo_tamanho(); continue; }
        if (e.tipo == EV_MENU) {
            if ((int)e.tecla == MENU_AUTOC) {
                if (e.x >= 0 && e.x < autoc_n) {
                    char r[200];
                    caminho_juntar(r, sizeof r, autoc_dir, autoc_nome[e.x]);
                    junta_texto(r, "/", sizeof r);
                    caixa_definir(&caixa, r);
                    desenhar_rota_painel(P);
                }
            } else escolha_menu((int)e.tecla, e.x);
            continue;
        }

        /* Un clic en otro panel lo hace el activo. */
        if (e.tipo == EV_CLIQUE && n_paineis > 1 && !dnd.ativo) {
            int k = painel_em(e.x, e.y);
            if (k >= 0 && k != ativo) {
                caixa.foco = false;
                usar_painel(k);
                for (int q = 0; q < n_paineis; q++) desenhar_rota_painel(&paineis[q]);
                previa_atualizar();
                registrar("arquivos: panel activo %d", k + 1);
            }
        }
        if (e.tipo == EV_RODA) {
            int k = painel_em(e.x, e.y);
            if (k >= 0 && k != ativo) usar_painel(k);
        }

        /* La ruta: Tab completa. */
        if (caixa.foco && modo_caixa == MODO_ROTA && e.tipo == EV_TECLA && e.tecla == '\t') { autocompletar(); continue; }
        /* El cuadro de la ruta (Ctrl+L) y el de la búsqueda. */
        if (caixa_tratar(j, &caixa, &e)) {
            if (modo_caixa == MODO_ROTA) {
                char v[160];
                copia_texto(v, caixa.texto, sizeof v);
                caixa.foco = false;
                if (compara_texto(v, "inicio") == 0 || compara_texto(v, INICIO_RUTA) == 0) entrar(INICIO_RUTA, true);
                else entrar(v, true);
                desenhar_rota_painel(P);
            } else {
                char v[64];
                copia_texto(v, caixa.texto, sizeof v);
                caixa.foco = false;
                if (!v[0] && filtros_ativos()) copia_texto(v, "*", sizeof v);
                if (v[0]) buscar(v);
                else desenhar_rota_painel(P);
            }
            continue;
        }
        if (caixa.foco && e.tipo == EV_TECLA) {
            if (e.tecla == TECLA_ESC) { caixa.foco = false; desenhar_rota_painel(P); continue; }
            if (modo_caixa == MODO_BUSCA) {
                /* filtro rápido: la lista se va filtrando mientras se escribe */
                char antes[64];
                copia_texto(antes, filtro, sizeof antes);
                copia_texto(filtro, caixa.texto, sizeof filtro);
                if (compara_texto(antes, filtro) != 0) {
                    em_busca = false;
                    busca_cancelar();
                    carregar();
                    char msg[160];
                    formatar_texto(msg, sizeof msg, _("Filtro: %s"), filtro);
                    desenhar_rodape(msg);
                }
            }
            continue;
        }

        /* El botón «Filtros» de la búsqueda. */
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && caixa.foco && modo_caixa == MODO_BUSCA && filtro_bx &&
            e.x >= filtro_bx && e.x < filtro_bx + 78 && e.y >= P->y_rota && e.y < P->y_rota + ALT_ROTA) {
            menu_filtros(filtro_bx, P->y_rota + ALT_ROTA);
            continue;
        }

        /* El botón derecho: menú contextual (o quitar un favorito). */
        if (e.tipo == EV_CLIQUE && (e.botoes & 2)) {
            int li = lado_em(e.x, e.y);
            if (li >= 0) {
                if (lado[li].acao == LUG_FAV) { lado_fav_menu = lado[li].cor; menu_contextual(j->id, e.x, e.y, _("Quitar de Favoritos"), MENU_FAV); }
                continue;
            }
            int k = painel_em(e.x, e.y);
            if (k >= 0 && P->n_abas > 1 && abas_em(&P->barra_abas, e.x, e.y) >= 0) continue;
            abrir_menu(&e);
            continue;
        }

        /* La primera fila de la barra (D423): botones planos, vistas y búsqueda. */
        bool clic = false;
        for (int i = 0; i < BN_N; i++) {
            if (!bt_vis[i] || !mbotao_tratar(j, &bt[i], &e)) continue;
            clic = true;
            if (i == BN_VOLTAR) voltar();
            else if (i == BN_AVANCAR) avancar();
            else if (i == BN_SUBIR) subir();
            else if (i == BN_NOVA) nova_pasta();
            else if (i == BN_BORRAR) apagar_item();
            else if (i == BN_OCULTOS) mudar_ocultos();
        }
        if (clic) continue;
        if (seg_vis) {
            int sv = segmentado_tratar(j, &seg_vistas, &e);
            if (sv >= 0) { if (sv != vista) poner_vista(sv); continue; }
        }
        if (busca_l > 0) {
            caixa_t v;
            busca_caixa(&v);
            bool in = e.x >= busca_x && e.x < busca_x + busca_l && e.y >= BARRA_Y && e.y < BARRA_Y + BARRA_A;
            bool inx = in && campo_x_em(&v, BARRA_A, e.x, e.y);
            if (e.tipo == EV_MOVER && (in != busca_hover || inx != busca_hover_x)) { busca_hover = in; busca_hover_x = inx; desenhar_busca_barra(); }
            if (e.tipo == EV_CLIQUE && (e.botoes & 1) && in) {
                if (inx) limpar_busca();
                else if (caixa.foco && modo_caixa == MODO_BUSCA) {   /* ya se edita: el clic coloca el cursor */
                    int tx = busca_x + 10 + 16 + 6, n = (int)tam_texto(caixa.texto);
                    int cabem = (busca_x + busca_l - 10 - 18 - tx) / LETRA_L;
                    if (cabem < 1) cabem = 1;
                    int ini = caixa.cursor > cabem ? caixa.cursor - cabem : 0;
                    int col = ini + (e.x - tx + LETRA_L / 2) / LETRA_L;
                    caixa.cursor = col < 0 ? 0 : col > n ? n : col;
                    desenhar_busca_barra();
                } else iniciar_busca();
                continue;
            }
        }

        /* La barra de órdenes. */
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && e.y >= ALT_BARRA && e.y < ALT_BARRA + ALT_ORDENS) {
            int k = ordem_em(e.x, e.y);
            if (k >= 0) { registrar("arquivos: orden %d", k); ordem_executar(k); }
            continue;
        }
        if (e.tipo == EV_MOVER && !(e.botoes & 1)) {   /* el nombre del botón, en la barra de estado, y su resalte */
            int k = ordem_em(e.x, e.y);
            if (k != hover_ordem) {
                if (hover_ordem >= 0) { ob[hover_ordem].hover = false; desenhar_orden(hover_ordem); }
                hover_ordem = k;
                if (k >= 0) { ob[k].hover = true; desenhar_orden(k); desenhar_rodape(_(ORDENS[k].texto)); }
                else { char msg[200]; estado_texto(msg, sizeof msg); desenhar_rodape(msg); }
            }
        }
        if (e.tipo == EV_MOVER && !dnd.ativo && !(e.botoes & 1)) {   /* resaltes: barra lateral, migas de pan y tarjetas de Inicio */
            int li = lado_em(e.x, e.y);
            if (li >= 0 && lado[li].tipo == LT_SEC) li = -1;
            if (li != lado_hover) { lado_hover = li; desenhar_lateral(); }
            for (int k = 0; k < n_paineis; k++) {
                painel_t *p = &paineis[k];
                if (compara_texto(p->s_pasta, INICIO_RUTA) == 0 || (p == P && caixa.foco && modo_caixa == MODO_ROTA)) continue;
                bool seta;
                int h = migas_hover_em(&p->migas, e.x, e.y, &seta);
                if (h != p->migas.hover || (h && seta != p->migas.hover_seta)) { p->migas.hover = h; p->migas.hover_seta = seta; desenhar_rota_painel(p); }
            }
            int ia = -1;
            if (compara_texto(pasta, INICIO_RUTA) == 0 && dentro_lista(e.x, e.y))
                for (int i = 0; i < P->n_alvos; i++)
                    if (e.x >= P->alvos[i].x && e.x < P->alvos[i].x + P->alvos[i].l && e.y >= P->alvos[i].y && e.y < P->alvos[i].y + P->alvos[i].a) { ia = i; break; }
            if (ia + 1 != P->alvo_h) { P->alvo_h = ia + 1; if (compara_texto(pasta, INICIO_RUTA) == 0) desenhar_lista(); }
        }

        /* La barra lateral. */
        if (e.tipo == EV_RODA && e.x < LATERAL) {
            lado_rolo -= (int)e.cliques * 3 * 8;
            desenhar_lateral();
            continue;
        }
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && e.x < LATERAL - 1 && e.y >= ALT_BARRA + ALT_ORDENS && e.y < ALT - ALT_STATUS) {
            int li = lado_em(e.x, e.y);
            if (li >= 0) { clic_lado(li, e.x); }
            continue;
        }

        /* Las pestañas de cada panel (también el arrastre para moverlas). */
        {
            bool tratado = false;
            for (int k = 0; k < n_paineis && !tratado; k++) {
                painel_t *p = &paineis[k];
                if (p->n_abas <= 1 || dnd.ativo) continue;
                int i = -1, q = -1;
                int r = abas_tratar(j, &p->barra_abas, &e, &i, &q);
                if (r == ABAS_NADA) continue;
                tratado = true;
                if (k != ativo) usar_painel(k);
                if (r == ABAS_SELECIONOU) { p->barra_abas.sel = p->aba_sel; aba_abrir(i, false); }
                else if (r == ABAS_FECHOU) aba_fechar(i);
                else if (r == ABAS_NOVA) aba_nova(INICIO_RUTA);
                else if (r == ABAS_MOVEU) {
                    aba_guardar(p);
                    aba_t t = p->abas[i]; p->abas[i] = p->abas[q]; p->abas[q] = t;
                    p->aba_sel = p->barra_abas.sel;
                    desenhar_abas_painel(p);
                    registrar("arquivos: aba movida %d -> %d", i + 1, q + 1);
                }
            }
            if (tratado) continue;
        }

        /* La barra de ruta: migas de pan (clic en una parte, o en su flecha), o editar la ruta. */
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && e.y >= P->y_rota && e.y < P->y_rota + ALT_ROTA && e.x >= P->x && e.x < P->x + P->l) {
            char cam[160];
            int r = compara_texto(pasta, INICIO_RUTA) == 0 ? 0 : migas_em(&P->migas, e.x, e.y, cam, sizeof cam);
            if (r == 1) entrar(cam, true);
            else if (r == 2) menu_miga(cam, e.x, P->migas.y + P->migas.a);
            else editar_ruta();
            continue;
        }

        /* El encabezado de la vista de detalles: ordenar y ajustar columnas. */
        if (vista == VISTA_DETALHES && compara_texto(pasta, INICIO_RUTA) != 0) {
            preparar_colunas();
            int col = 0;
            int r = colunas_tratar(j, &colunas, &e, &col);
            if (r == COLUNAS_ORDENAR) { mudar_ordem(col); continue; }
            if (r == COLUNAS_REDIM) {
                for (int k = 0; k < n_paineis; k++) { painel_t *a = P; P = &paineis[k]; if (vista == VISTA_DETALHES) desenhar_lista(); P = a; }
                continue;
            }
            if (e.tipo == EV_SOLTOU && colunas.arrasto == 0 && colunas.arrasto_x0) { colunas.arrasto_x0 = 0; guardar_preferencias(); }
            if (e.tipo == EV_CLIQUE && colunas.arrasto > 0) continue;
        }

        /* Las tarjetas de la página de Inicio. */
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && compara_texto(pasta, INICIO_RUTA) == 0 && dentro_lista(e.x, e.y)) {
            for (int i = 0; i < P->n_alvos; i++) {
                if (e.x >= P->alvos[i].x && e.x < P->alvos[i].x + P->alvos[i].l && e.y >= P->alvos[i].y && e.y < P->alvos[i].y + P->alvos[i].a) {
                    char cam[160];
                    copia_texto(cam, P->alvos[i].cam, sizeof cam);
                    if (existe_pasta(cam)) entrar(cam, true); else abrir_arquivo(cam, nome_base(cam), TIPO_ARQUIVO);
                    break;
                }
            }
            continue;
        }

        /* Arrastrar y soltar: empezar, seguir y soltar. */
        if (e.tipo == EV_MOVER && dnd.premido && (e.botoes & 1)) {
            if (!dnd.ativo) {
                int dx = e.x - dnd.x0, dy = e.y - dnd.y0;
                if (dx * dx + dy * dy > 36 && n_marcados > 0) { dnd.ativo = true; dnd.origem = ativo; registrar("arquivos: arrastre empezado (%d itens)", n_marcados); }
            }
            if (dnd.ativo) dnd_calcular(e.x, e.y);
            continue;
        }
        if (e.tipo == EV_SOLTOU && dnd.premido) {
            if (dnd.ativo) {
                dnd_soltar((e.modificadores & 1) != 0);
            } else {
                int si = dnd.soltar_item;
                dnd_limpiar();
                if (si >= 0 && si < lista.qtd && marcados[si] && n_marcados > 1) {   /* un clic sin arrastrar elige solo ese */
                    limpar_marcados(); marcados[si] = true; n_marcados = 1; ancora = si; lista.sel = si;
                    desenhar_lista(); previa_atualizar();
                }
            }
            dnd.premido = false;
            continue;
        }

        /* El recuadro de selección con el ratón. */
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && dentro_lista(e.x, e.y) && compara_texto(pasta, INICIO_RUTA) != 0 && item_de_ponto(e.x, e.y) < 0) {
            int ih = lista.topo + (e.y - lista.y - 2) / LISTA_ALTURA_ITEM;
            if (!GRELHA && e.y >= lista.y + 2 && ih >= 0 && ih < lista.qtd && tipos[ih] == TIPO_GRUPO) continue;   /* cabecera de grupo */
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
            previa_atualizar();
            continue;
        }

        /* La lista: un clic elige (Ctrl y Mayús añaden o quitan del rango). */
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && dentro_lista(e.x, e.y)) {
            int i = item_de_ponto(e.x, e.y);
            if (i >= 0) {
                bool ctrl = (e.modificadores & 2) != 0, mayus = (e.modificadores & 1) != 0;
                /* doble clic de verdad: dos pulsaciones seguidas en el mismo sitio (el núcleo cuenta solo el tiempo) */
                static int ult_x = -100, ult_y = -100;
                bool duplo = e.cliques >= 2 && e.x - ult_x <= 6 && ult_x - e.x <= 6 && e.y - ult_y <= 6 && ult_y - e.y <= 6;
                ult_x = e.x; ult_y = e.y;
                if (!duplo) e.cliques = 1;
                dnd.soltar_item = -1;
                if (ctrl) alternar_marca(i);
                else if (mayus) marcar_rango(i);
                else if (marcados[i] && n_marcados > 1 && e.cliques < 2) dnd.soltar_item = i;   /* puede ser el principio de un arrastre */
                else { limpar_marcados(); marcados[i] = true; n_marcados = 1; ancora = i; }
                lista.sel = i;
                desenhar_lista();
                previa_atualizar();
                char msg[200];
                info_item(i, msg, sizeof msg);
                desenhar_rodape(msg);
                dnd.premido = !ctrl && !mayus && e.cliques < 2;
                dnd.x0 = e.x; dnd.y0 = e.y; dnd.ativo = false;
                if (e.cliques >= 2) { dnd_limpiar(); abrir_item(i); }
                continue;
            }
        }
        /* Na vista de icones a roda e as setas sao da grelha (D139): a lista
         * comum rolaria um item de cada vez e a grelha andaria de lado. */
        if (GRELHA && e.tipo == EV_RODA && dentro_lista(e.x, e.y)) {
            if (grade_rolar((int)e.cliques * RODA_LINHAS))
                registrar("arquivos: rolou a grelha para o item %d (linha %d de %d)",
                          lista.topo, lista.topo / grade_colunas(), lista.qtd);
            continue;
        }

        /* Atajos de teclado (como Files). */
        if (e.tipo == EV_TECLA) {
            unsigned t = e.tecla;
            bool ctrl = (e.modificadores & 2) != 0, alt = (e.modificadores & 4) != 0, mayus = (e.modificadores & 1) != 0;
            if (ctrl && t == 20) { aba_nova(INICIO_RUTA); continue; }                   /* Ctrl+T */
            if (ctrl && t == 23) { aba_fechar(P->aba_sel); continue; }                  /* Ctrl+W */
            if (ctrl && t == '\t') { aba_proxima(mayus ? -1 : 1); continue; }           /* Ctrl+Tab */
            if (ctrl && t == 14 && mayus) { nova_pasta(); continue; }                    /* Ctrl+Shift+N */
            if (ctrl && t == 14) { char *a[2] = { "arquivos", NULL }; executar("/bin/arquivos", a); continue; }   /* Ctrl+N: otra ventana */
            if (ctrl && t == 4) { anclar_favorito(); continue; }                         /* Ctrl+D */
            if (ctrl && (t == 5 || t == 6)) { iniciar_busca(); continue; }              /* Ctrl+E, Ctrl+F */
            if (t == TECLA_F3 || (ctrl && t == 19)) { painel_doble(); continue; }      /* F3 */
            if (t == TECLA_F5) { git_atualizar(true); carregar(); continue; }           /* F5 */
            if (alt && (t == 'p' || t == 'P')) { ordem_executar(OR_PREVIA); continue; }   /* Alt+P */
            if (alt && t == TECLA_ESQUERDA) { voltar(); continue; }
            if (alt && t == TECLA_DIREITA) { avancar(); continue; }
            if (alt && t == TECLA_CIMA) { subir(); continue; }
            if (alt && t == TECLA_HOME) { entrar(INICIO_RUTA, true); continue; }
            if (ctrl && t >= '1' && t <= '4') { poner_vista(t == '1' ? VISTA_LISTA : t == '2' ? VISTA_DETALHES : t == '3' ? VISTA_ICONES : VISTA_MOSAICOS); continue; }
            if (t == '\t' && n_paineis > 1) {                                            /* Tab: el otro panel */
                usar_painel(1 - ativo);
                for (int q = 0; q < n_paineis; q++) desenhar_rota_painel(&paineis[q]);
                previa_atualizar();
                continue;
            }
        }
        /* En la vista de iconos las setas son de la rejilla. */
        if (GRELHA && e.tipo == EV_TECLA) {
            unsigned tk = e.tecla;
            if (tk == TECLA_BAIXO || tk == TECLA_CIMA || tk == TECLA_DIREITA || tk == TECLA_ESQUERDA ||
                tk == TECLA_HOME || tk == TECLA_END || tk == TECLA_PGUP || tk == TECLA_PGDN) {
                grade_tecla(tk);
                continue;
            }
        }
        /* En la vista de iconos un clic en un hueco no es de la lista comun (D217): sus filas
         * son de otra altura y seleccionaria un elemento que no esta bajo el raton. */
        int r = (GRELHA && e.tipo == EV_CLIQUE) ? LISTA_NADA : (compara_texto(pasta, INICIO_RUTA) == 0 ? LISTA_NADA : lista_tratar(j, &lista, &e));
        if (r == LISTA_ATIVOU) { if (lista.sel >= 0 && tipos[lista.sel] != TIPO_GRUPO) abrir_item(lista.sel); }
        else if (r == LISTA_SELECIONOU && lista.sel >= 0) {
            sel_sin_cabecalho(e.tipo == EV_TECLA && (e.tecla == TECLA_CIMA || e.tecla == TECLA_PGUP || e.tecla == TECLA_HOME) ? -1 : 1);
            limpar_marcados();
            marcados[lista.sel] = true;
            n_marcados = 1;
            ancora = lista.sel;
            desenhar_lista();
            previa_atualizar();
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
                for (int i = 0; i < lista.qtd; i++) if (tipos[i] != TIPO_GRUPO) { marcados[i] = true; n_marcados++; }
                desenhar_lista();
                previa_atualizar();
                char msg[80];
                formatar_texto(msg, sizeof msg, _("%d elementos selecionados"), n_marcados);
                desenhar_rodape(msg);
                registrar("arquivos: selecionados %d itens", n_marcados);
                continue;
            }
            if (ctrl && t == 3) { copiar_area(false); continue; }        /* Ctrl+C */
            if (ctrl && t == 24) { copiar_area(true); continue; }         /* Ctrl+X */
            if (ctrl && t == 22) { colar_area(); continue; }              /* Ctrl+V */
            if (ctrl && t == 12) { editar_ruta(); continue; }             /* Ctrl+L */
            if (t == 0x10B) { renomear_item(); continue; }                /* F2 */
            if (t == TECLA_DELETE) { apagar_item(); continue; }
            if (t == '\b') { subir(); continue; }
            if (t == TECLA_ESC) {
                if (caixa.foco) { caixa.foco = false; desenhar_rota_painel(P); continue; }
                if (busca.ativa || em_busca) { em_busca = false; busca_cancelar(); padrao_busca[0] = 0; carregar(); continue; }
                limpar_marcados();
                desenhar_lista();
                previa_atualizar();
                continue;
            }
        }
    }
    jan_fechar(j);
    return 0;
}
