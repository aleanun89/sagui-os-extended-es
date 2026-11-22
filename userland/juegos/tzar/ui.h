/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ui.h - interfaz original de Tzar: fuentes .FNT, barra y HUD de la partida, selección, órdenes, colocación
 * de edificios y menús principales (D417, fase 4 de docs/tzar.md).
 *
 * Traducción a C de la interfaz de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): font.rs,
 * hud.rs (geometría LAYOUT800L, vista de selección, rejilla de órdenes), menu.rs (pantallas y rectángulos) y
 * frontend/gameplay.rs (selección y órdenes). Cada .c dice de qué parte sale.
 *
 * Solo habla con el mundo por las funciones de mundo.h (consultas, comandos y eventos con cursor propio): no
 * escribe nunca en TzMundo. No usa la SDL: dibuja en un lienzo ARGB (TzTela) y recibe la entrada como
 * TzUiEntrada, para probarla sin pantalla; el visor o el programa del juego traduce la SDL.
 */
#ifndef TZAR_UI_H
#define TZAR_UI_H

#include "tzar.h"
#include "mundo.h"
#include "opcoes.h"

/* ------------------------------------------------------------------ lienzo (ui_lienzo.c) */
typedef struct { int w, h; uint32_t *px; } TzTela;          /* 0xAARRGGBB, un píxel por entrada (sin pitch) */
typedef struct { int x, y, w, h; } TzRect;

int tz_tela_criar(TzTela *t, int w, int h);
void tz_tela_liberar(TzTela *t);
int tz_rect_dentro(TzRect r, int x, int y);
/* relleno con alfa (0xAARRGGBB; alfa 255 opaco) */
void tz_tela_preencher(TzTela *t, TzRect r, uint32_t argb);
void tz_tela_moldura(TzTela *t, TzRect r, uint32_t argb);
/* copia el recorte (sx, sy, sw, sh) de la imagen a (dx, dy): alfa 0 nada, 0x80 sombra, el resto opaco; escurecer
 * 0-100 % la oscurece (botones sin disponer) */
void tz_tela_imagem(TzTela *t, const TzImg *im, TzRect src, int dx, int dy, int escurecer);
/* igual pero escalada (vecino más próximo) al rectángulo destino */
void tz_tela_imagem_escala(TzTela *t, const TzImg *im, TzRect src, TzRect dst);
int tz_tela_gravar_bmp(const TzTela *t, const char *ruta);

/* ------------------------------------------------------------------ fuentes .FNT (ui_fonte.c)
 * Una sola lectura de .FNT para todo Tzar: la de dibujo.c (TzFonte de tzar.h, de font.rs). La interfaz dibuja el
 * texto en su lienzo TzTela (con alfa) con sus propias funciones. */
typedef TzFonte TzUiFonte;
#define tz_ui_fonte_ler tz_fonte_ler
#define tz_ui_fonte_liberar tz_fonte_liberar
int tz_ui_fonte_medir(const TzUiFonte *f, const char *txt);                       /* ancho en píxeles (0 sin fuente) */
/* texto Latin-1 (los bytes de los datos) en (x, y) */
void tz_ui_fonte_texto(TzTela *t, const TzUiFonte *f, int x, int y, const char *txt, uint32_t rgb);
/* texto centrado (o a la izquierda) en un rectángulo, recortado con «...» si no cabe */
void tz_ui_fonte_caixa(TzTela *t, const TzUiFonte *f, TzRect r, const char *txt, uint32_t rgb, int centrado);
/* parte el texto en líneas de ancho máximo; escribe hasta max y devuelve cuántas (cada una en 'linha') */
int tz_ui_fonte_quebrar(const TzUiFonte *f, const char *txt, int largura, char linha[][160], int max);

/* ------------------------------------------------------------------ recursos de la interfaz (ui_skin.c) */
#define TZ_UI_CACHE 96
typedef struct {
	TzRecursos *rec;
	TzIni textos;                /* LOCAL/STRINGS, 2 y 3 fundidos */
	TzIni icones;                /* DATA/ICONS.INI */
	TzUiFonte f10, f12b, f14b, f22, fpeq;
	uint32_t cores[8];           /* color de cada reino (COLOR BARS) */
	uint32_t cores_mini[8];
	struct { char chave[96]; int slot; TzImg img; int ok; } cache[TZ_UI_CACHE]; int ncache;
	int ok;
} TzUiSkin;

int tz_ui_skin_carregar(TzUiSkin *s, TzRecursos *r);
/* skin sin datos (pruebas sintéticas y arranque sin juego): fuentes y textos mínimos propios, sin arte */
int tz_ui_skin_vazia(TzUiSkin *s);
void tz_ui_skin_liberar(TzUiSkin *s);
/* IMAGES/<caminho>.BMP (verde transparente, slot = color del reino o -1), en caché; NULL si falta */
const TzImg *tz_ui_img(TzUiSkin *s, const char *caminho, int slot);
/* texto de STRINGS: [secao] chave; si falta, el valor de respaldo (nunca NULL) */
const char *tz_ui_txt(const TzUiSkin *s, const char *secao, const char *chave, const char *respaldo);
/* icono de ICONS.INI (clave) -> imagen y recorte cuadrado; 0 si no está */
int tz_ui_icone(TzUiSkin *s, const char *chave, int slot, const TzImg **img, TzRect *src);
/* icono de la clase de objeto (DATA/OBJECTS/<clase>.INI, «icon» de la civilización) */
int tz_ui_icone_classe(TzUiSkin *s, const TzBiblioteca *b, int classe, int civ, int slot, const TzImg **img, TzRect *src);

/* ------------------------------------------------------------------ entrada (ui_selecao.c) */
enum { TZ_IN_MOVER = 1, TZ_IN_BAIXO, TZ_IN_CIMA, TZ_IN_TECLA, TZ_IN_RODA };
enum { TZ_MOD_SHIFT = 1, TZ_MOD_CTRL = 2, TZ_MOD_ALT = 4, TZ_MOD_BOTAO = 0x100 /* botón izquierdo pulsado (lo pone la interfaz) */ };
enum {   /* teclas que no son un carácter */
	TZ_TC_ESC = 256, TZ_TC_ENTER, TZ_TC_BORRAR, TZ_TC_ESQ, TZ_TC_DIR, TZ_TC_CIMA, TZ_TC_BAIXO, TZ_TC_INICIO, TZ_TC_PGUP,
	TZ_TC_PGDN, TZ_TC_F1 = 300,
};
typedef struct {
	int tipo;                    /* TZ_IN_* */
	int x, y;                    /* píxeles de la ventana */
	int botao;                   /* 1 izquierdo, 2 medio, 3 derecho */
	int tecla;                   /* carácter en minúsculas o TZ_TC_* */
	int mods;                    /* TZ_MOD_* */
	int delta;                   /* rueda */
} TzUiEntrada;

#define TZ_SEL_MAX 128
typedef struct {
	TzId id[TZ_SEL_MAX]; int n;
	TzId grupo[10][TZ_SEL_MAX]; int ngrupo[10];
	uint32_t clique_ms; TzId clique_obj; int clique_classe;
	uint32_t grupo_ms; int grupo_ult;
} TzUiSel;

/* ------------------------------------------------------------------ partida (ui_jogo.c, ui_hud.c, ui_selecao.c) */
enum { TZ_UI_NORMAL = 0, TZ_UI_COLOCAR, TZ_UI_ALVO };
#define TZ_UI_BOTOES 8                   /* rejilla de órdenes: 2 x 4 por página */
#define TZ_UI_PILHA 4

typedef struct {
	const TzOrdemDef *def;               /* la orden */
	char hotkey;
	char rotulo[64];                     /* «Construir cuartel» */
	char custo[96];                      /* «oro 50 madera 20» o vacío */
	char motivo[128];                    /* por qué no se puede (vacío si se puede) */
	int ok;                              /* 1 = disponible */
	int classe;                          /* clase que construye o entrena, -1 */
	int tec;                             /* tecnología, -1 */
	char submenu[32];
} TzUiBotao;

typedef struct {
	/* lo que usa (no es suyo) */
	TzUiSkin *skin;
	TzMundo *m;
	const TzBiblioteca *bib;             /* iconos de las clases; puede ser NULL */
	/* vista */
	int w, h;
	TzRect mapa_rect, minimapa_rect, selecao_rect;   /* ventana del mapa, minimapa, vista de selección */
	int camx, camy;                      /* esquina de arriba a la izquierda del mundo, en píxeles */
	TzImg fundo;                         /* el marco de piedra a tamaño de ventana */
	uint32_t *minimapa;                  /* lado*lado del terreno (puede ser NULL; es del llamador) */
	/* entrada */
	TzUiSel sel;
	int modo;                            /* TZ_UI_* */
	int classe_colocar, colocar_cx, colocar_cy, colocar_ok;
	int ordem_idx;                       /* orden pendiente de objetivo (índice en lista) */
	char acao_alvo[48]; int tipo_alvo;
	int mx, my, mods;
	int arrasta, ax, ay;                 /* recuadro de selección */
	int arrasta_mini;
	int pagina;
	char pilha[TZ_UI_PILHA][32]; int npilha;   /* submenús abiertos */
	TzUiBotao botoes[TZ_UI_BOTOES]; int nbotoes, ntotal_botoes;
	TzUiBotao *lista; int nlista, caplista;
	int botao_sobre;                     /* botón bajo el ratón o -1 */
	TzId sel_menu_obj;                   /* objeto cuyo menú se calculó */
	/* avisos y eventos */
	uint32_t ev_cursor;
	char aviso[160]; uint32_t aviso_ate;
	uint32_t agora_ms;
	int botao_press;                     /* botón de órdenes pulsado (se actúa al soltar) */
	int scroll_bordes;                   /* 1 = el ratón en el borde de la ventana mueve la cámara */
	uint32_t ultimo_ms;
	int pedir_menu, pedir_sair;          /* el jugador pidió el menú de partida / salir */
	/* ventanas de la partida (ui_janelas.c): 0 ninguna, TZ_UJ_DIPLOMACIA o TZ_UJ_CHAT */
	int janela;
	char chat[72]; int chat_dest;        /* la línea que se escribe y a quién va (0 todos, 1 aliados, 2 enemigos) */
	char chat_hist[6][128]; uint32_t chat_cor[6]; int nchat;   /* las últimas líneas */
	uint32_t chat_ate;                   /* las líneas se ven sobre el mapa hasta entonces (ms) */
	char chat_saida[80];                 /* lo que se mandó: el programa lo lee (IA) y lo vacía */
	int pausa;
	int direita;                         /* el panel a la derecha (LAYOUT800R); se cambia con tz_ui_lado */
	/* estadística para la prueba */
	uint32_t n_comandos;
} TzUi;

int tz_ui_iniciar(TzUi *u, TzUiSkin *s, TzMundo *m, const TzBiblioteca *bib, int w, int h);
void tz_ui_liberar(TzUi *u);
void tz_ui_redimensionar(TzUi *u, int w, int h);
void tz_ui_lado(TzUi *u, int direita);                               /* panel a la izquierda (0) o a la derecha (1) */
/* rectángulos del marco según el lado del panel (hud.rs Layout) */
enum { TZ_UR_MENU = 0, TZ_UR_DIPLOMACIA, TZ_UR_CHAT, TZ_UR_PAINEL, TZ_UR_RELOGIO, TZ_UR_SETA0, TZ_UR_RECURSO0 = TZ_UR_SETA0 + 3 };
TzRect tz_ui_rect(const TzUi *u, int qual);
void tz_ui_mover_camara(TzUi *u, int dx, int dy);                    /* en píxeles, con límites */
void tz_ui_centrar(TzUi *u, int cx, int cy);                         /* cámara sobre una casilla */
void tz_ui_entrada(TzUi *u, const TzUiEntrada *e, uint32_t ms);
void tz_ui_tic(TzUi *u, uint32_t ms);                                /* cada cuadro: avisos, selección viva */
/* dibujo: primero el mundo (el llamador), luego esto: marcas de selección sobre el mapa y el marco con el HUD */
void tz_ui_desenhar_marcas(TzUi *u, TzTela *t);
void tz_ui_desenhar_hud(TzUi *u, TzTela *t);

/* selección (ui_selecao.c) */
void tz_ui_sel_limpar(TzUiSel *s);
void tz_ui_sel_vivos(const TzMundo *m, TzUiSel *s);                  /* quita los muertos */
int tz_ui_sel_clique(TzMundo *m, TzUiSel *s, int jogador, int px, int py, int somar, uint32_t ms, TzRect vista);
int tz_ui_sel_caixa(TzMundo *m, TzUiSel *s, int jogador, int x0, int y0, int x1, int y1, int somar);
void tz_ui_sel_grupo_guardar(TzUiSel *s, int g);
int tz_ui_sel_grupo_chamar(TzMundo *m, TzUiSel *s, int g, uint32_t ms, int *duplo);
TzId tz_ui_pick(const TzMundo *m, int px, int py);                   /* objeto bajo un punto del mundo (píxeles) */
/* órdenes: manda los comandos de la orden de clic derecho; devuelve cuántos mandó */
int tz_ui_ordem_direita(TzUi *u, int px, int py, int enfileirar);
int tz_ui_ordem_alvo(TzUi *u, int px, int py, int enfileirar);       /* orden pendiente con objetivo */

/* menú de órdenes del objeto seleccionado (ui_hud.c) */
void tz_ui_menu_atualizar(TzUi *u);
void tz_ui_botao_premir(TzUi *u, int i, int mods);
int tz_ui_botao_em(const TzUi *u, int x, int y);
const char *tz_ui_dica(TzUi *u, char *buf, int n);                   /* texto de la línea de ayuda */
void tz_ui_aviso(TzUi *u, const char *fmt, ...);

/* mundo visible (ui_mapa.c): terreno, escena estática y niebla; para el visor y las pruebas */
typedef struct {
	TzTerreno *ter; const TzMapa *mapa; TzEscena *esc;
	uint64_t ms;                         /* reloj del juego (animaciones; el agua cambia cada 250 ms) */
} TzUiMapa;
void tz_ui_mapa_desenhar(TzUi *u, TzTela *t, const TzUiMapa *mp);

/* diplomacia y chat (ui_janelas.c) */
enum { TZ_UJ_DIPLOMACIA = 1, TZ_UJ_CHAT = 2 };
void tz_ui_janela_desenhar(TzUi *u, TzTela *t);
int tz_ui_janela_entrada(TzUi *u, const TzUiEntrada *e);            /* 1 si la ventana se quedó con la entrada */
TzRect tz_ui_janela_rect(const TzUi *u);
/* la línea escrita pasa al historial («Tú: ...») y a chat_saida; extra (o NULL) es una respuesta del sistema */
void tz_ui_chat_linha(TzUi *u, int reino, const char *extra);
void tz_ui_chat_recebido(TzUi *u, const char *texto);              /* una frase de otro reino (la IA con «talk on») */

/* cursores del juego (ui_cursor.c): la forma que toca en la partida («» = la flecha) y su dibujo en el lienzo */
const char *tz_ui_cursor_forma(TzUi *u);
void tz_ui_cursor_desenhar(TzUiSkin *s, TzTela *t, const char *forma, int x, int y);
void tz_ui_cursor_liberar(void);

/* ------------------------------------------------------------------ menús principales (ui_menu.c) */
enum {
	TZ_MN_PRINCIPAL = 0, TZ_MN_UM, TZ_MN_CAMPANHAS, TZ_MN_MAPAS, TZ_MN_CONFIG, TZ_MN_CARREGAR, TZ_MN_GUARDAR, TZ_MN_OPCOES,
	TZ_MN_CREDITOS, TZ_MN_SAIR, TZ_MN_PARTIDA /* el de dentro de una partida */,
	TZ_MN_FIM /* fin de la partida (D_ENDGAME) */, TZ_MN_AJUDA /* instrucciones (HELP/HK.TXT) */,
	TZ_MN_ALEAT /* mapa aleatorio (S_SINGLERND, v8.5.5.2) */, TZ_MN_NTELAS
};
enum {
	TZ_MR_NADA = 0, TZ_MR_JOGAR, TZ_MR_CAMPANHA, TZ_MR_CARREGAR, TZ_MR_GUARDAR, TZ_MR_SAIR, TZ_MR_CONTINUAR, TZ_MR_REINICIAR,
	TZ_MR_SEGUINTE /* fin: la misión siguiente */, TZ_MR_REPETIR /* fin: otra vez la misma */, TZ_MR_ABANDONAR /* fin: al menú principal */,
};

typedef struct {
	char titulo[64];
	char fich[200];                      /* fichero del mapa o de la campaña (relativo a la instalación) */
	char dentro[100];
	int jogadores_min, jogadores_max;
	const char *descricao;               /* texto de la campaña o de la misión (lo guarda el llamador) o NULL */
	int campanha, indice;                /* misiones: campaña y número de la misión en ella */
	int disponivel;                      /* misiones: alcanzada o inicial */
} TzMnItem;

typedef struct {                          /* opciones de la escaramuza (sp_setupmap) */
	int jogadores, populacao, dificuldade, recursos, vitoria, tipo;
	int sem_nevoa, explorado;
	int raca[8];                          /* civilización del reino i (0 al azar, 1 europeo, 2 árabe, 3 asiático) */
	int equipe[8];                        /* 0 solo, 1..4 equipo (en el mapa aleatorio) */
} TzMnConfig;

/* pantalla del mapa aleatorio (S_SINGLERND de menu.rs, D435): lo que elige la persona; jogo.c lo pasa a TzAleatorio.
 * c lleva las reglas y los reinos (jugadores, civilización y equipo de cada uno, victoria, población, dificultad, recursos
 * iniciales 0..2, niebla); lado es un índice en tz_mn_lados. */
typedef struct {
	int lado;                             /* 0..6: 64, 96, 128, 192, 256, 384, 512 */
	int terreno;                          /* 0 al azar, 1 hierba, 2 verano, 3 nieve, 4 desierto */
	int tipo;                             /* 0 al azar, 1 archipiélagos, 2 islas, 3 continente, 4 lagos, 5 tierra */
	int recursos_mapa;                    /* minas del mapa: 0 pocas, 1 normales, 2 muchas */
	int guardado, animais, itens, portas, penhascos;   /* penhascos (montañas): sin generador, siempre 0 */
	unsigned long long semente;
	TzMnConfig c;
} TzMnAleat;
extern const int tz_mn_lados[7];
extern const char *const tz_mn_biomas[4];
int tz_mn_aleat_limite(int lado_idx);                  /* reinos que admite el tamaño (ReTzar random_player_limit: 4, 6 u 8) */
int tz_mn_aleat_tipo_ok(int lado_idx, int tipo);       /* tipos de geografía que admite el tamaño (random_kind_allowed) */
void tz_mn_aleat_padrao(TzMnAleat *a);
void tz_mn_aleat_normalizar(TzMnAleat *a);             /* mete todo en rango (jugadores según el tamaño, tipo permitido...) */
int tz_mn_aleat_texto(const TzMnAleat *a, char *out, size_t n);       /* una línea para tzar.cfg */
int tz_mn_aleat_de_texto(const char *t, TzMnAleat *a);                /* 0 si se leyó; si no, a queda como estaba */

typedef struct {
	char nome[64], data[40];
} TzMnGuardado;

typedef struct {
	TzUiSkin *skin;
	int w, h;
	int tela;                             /* TZ_MN_* */
	int pilha[8]; int npilha;
	int sobre, prem;                      /* widget bajo el ratón y el pulsado (-1 ninguno) */
	int mx, my;
	TzMnItem *campanhas; int ncampanhas;  /* las seis campañas */
	TzMnItem *mapas; int nmapas;          /* mapas sueltos y de la instalación */
	TzMnGuardado *guardados; int nguardados;
	int campanha_sel, mapa_sel, guardado_sel, rolo;
	TzMnConfig cfg;
	TzMnAleat aleat;                      /* pantalla del mapa aleatorio (se recuerda en tzar.cfg) */
	int ampliada;                         /* mapa aleatorio: la vista previa ampliada sobre la pantalla (un clic la cierra) */
	int aleat_mudou;                      /* 1 = la persona cambió algo: el llamador lo guarda y lo pone a 0 */
	TzMnAleat res_aleat;                  /* TZ_MR_JOGAR en el mapa aleatorio (res_indice -1): lo elegido */
	char nome_guardar[32]; int editando;
	int volume_som, volume_musica;        /* 0-100, tela de opciones */
	/* resultado: cuando acao != 0 el llamador lo atiende y lo pone a 0 */
	int acao, res_indice;
	TzMnConfig res_cfg;
	char res_nome[32];
	/* lista de misiones de las campañas (todas; TzMnItem.campanha dice de cuál) */
	TzMnItem *missoes; int nmissoes;
	int missao_sel;                       /* índice en missoes (-1 ninguna) */
	int res_missao;                       /* TZ_MR_CAMPANHA: número de la misión en la campaña */
	/* opciones de pantalla (pantalla Opciones): op_mudou = 1 cuando el jugador cambió algo; el llamador las aplica */
	TzOpcoes op; int op_mudou;
	const TzImg *previa;                  /* minimapa del mapa elegido (escaramuza; lo pone el llamador, puede ser NULL) */
	/* fin de la partida (TZ_MN_FIM) */
	int fim_ganhou, fim_seguinte;         /* ganó; hay misión siguiente */
	char fim_resumo[8][160]; uint32_t fim_cor[8]; int nfim_resumo;   /* tiempo y estadísticas de cada reino */
	/* instrucciones y créditos (se leen de los datos la primera vez) */
	char **ajuda; int najuda, ajuda_lido;
	char **creditos; uint8_t *creditos_estilo; int ncreditos, creditos_lido;
	uint32_t agora_ms, creditos_ms0;
} TzMenu;

void tz_menu_iniciar(TzMenu *m, TzUiSkin *s, int w, int h);
void tz_menu_liberar(TzMenu *m);
void tz_menu_ir(TzMenu *m, int tela);
void tz_menu_entrada(TzMenu *m, const TzUiEntrada *e);
void tz_menu_desenhar(TzMenu *m, TzTela *t);
void tz_menu_tic(TzMenu *m, uint32_t ms);                           /* reloj (créditos animados) */
void tz_menu_redimensionar(TzMenu *m, int w, int h);
/* rectángulos de los botones de la pantalla actual (para las pruebas); devuelve cuántos */
int tz_menu_botoes(const TzMenu *m, TzRect *out, char rotulos[][64], int max);

#endif
