// SPDX-License-Identifier: GPL-3.0-only
/* jogo.h - Sagui 3D (bônus B4): tipos y constantes compartidos.
 *
 * El juego se divide en archivos pequeños, cada uno con un tema:
 *   contas.c   punto fijo y la tabla de seno (el programa no tiene números
 *              de punto flotante: -mgeneral-regs-only)
 *   arte.c     paleta, texturas y personajes (pixel art hecho aquí)
 *   mapa.c     lectura del archivo de fase y consultas geométricas
 *   tela3d.c   el dibujo en perspectiva
 *   mundo.c    reglas: movimiento, monos-prego, semillas, bananas
 *   sagui3d.c  ventana, bucle principal, pantallas, barra de estado y sonidos
 * Ver la sección "Sagui 3D" en docs/decisoes.md. */
#ifndef SAGUI3D_JOGO_H
#define SAGUI3D_JOGO_H
#include "sagui.h"
#include "gui.h"

/* ------------------------------------------------------------ punto fijo */
/* "fixo": número con 16 bits enteros y 16 de fracción. 1,0 = 65536.
 * Las coordenadas del mapa están en "palmos": el sagui tiene el ojo a 40 palmos
 * del suelo y una sala común tiene 128 de altura. */
typedef int32_t fixo;
#define UM        65536
#define FX(n)     ((fixo)((n) * UM))
#define INTEIRO(f) ((f) >> 16)

static inline fixo fmul(fixo a, fixo b) { return (fixo)(((int64_t)a * b) >> 16); }
fixo muldiv(fixo a, fixo b, fixo c);   /* a * b / c con 64 bits en el medio, saturado */
fixo fdiv(fixo a, fixo b);             /* a / b en punto fijo */
int32_t raiz(uint32_t n);              /* raíz cuadrada entera */

/* Ángulos: 4096 pasos por vuelta. 0 = este, 1024 = norte (y crece hacia el norte). */
#define VOLTA      4096
#define QUARTO     1024
#define ANG(a)     ((a) & (VOLTA - 1))
void  contas_iniciar(void);
fixo  seno(int a);
fixo  cosseno(int a);
int   angulo_de(fixo dx, fixo dy);     /* dirección de un vector */
uint32_t sorteio(void);                /* números pseudoaleatorios del juego */

/* ------------------------------------------------------------ pantalla */
#define VL 320           /* la vista 3D se dibuja en 320x200 ... */
#define VA 200
#define ESCALA 2         /* ... y se amplía 2x en la ventana */
#define BARRA 48         /* barra de estado abajo */
#define JAN_L (VL * ESCALA)
#define JAN_A (VA * ESCALA + BARRA)
#define FOCO 176         /* distancia focal en píxeles: ~85 grados de visión */
#define HORIZONTE (VA / 2)
extern uint32_t vista[VA][VL];

/* ------------------------------------------------------------ arte */
#define TEX 32                          /* texturas de 32x32 */
enum { T_PEDRA, T_CIPO, T_MADEIRA, T_GRAMA, T_TERRA, T_LAJOTA, T_FOLHAS, T_ROCHA, T_CEU, QTD_TEXTURAS };
extern uint8_t texturas[QTD_TEXTURAS][TEX][TEX];   /* índices de la paleta */
#define NIVEIS_LUZ 16
extern uint32_t sombra[NIVEIS_LUZ][96];            /* color de cada índice en cada nivel de luz */
int  textura_por_nome(const char *nome);           /* -1 si no existe */

typedef struct {
    int l, a;
    const uint8_t *px;     /* l*a índices; 0 = transparente */
} figura_t;
enum { F_MACACO, F_MACACO_TONTO, F_BANANA, F_CACHO, F_CASCA, F_SEMENTE, F_POTE,
       F_PORTEIRA, F_PORTEIRA_ABERTA, F_ESTRELA, F_CORACAO, F_ESTILINGUE, F_ESTILINGUE_PUXADO,
       F_SAGUI, QTD_FIGURAS };
extern figura_t figuras[QTD_FIGURAS];
extern const uint32_t paleta[96];
void arte_iniciar(void);
int      altura_arvores(int angulo);
uint32_t cor_arvore(int angulo);
uint32_t degrade_ceu(int y);

/* ------------------------------------------------------------ mapa */
#define MAX_PONTOS  256
#define MAX_SALAS   64
#define MAX_LADOS   12      /* lados por sala */
#define MAX_COISAS  96

typedef struct { fixo x, y; } ponto_t;

typedef struct {
    int  a, b;          /* índices de los puntos; la sala queda a la izquierda de a->b */
    int  vizinha;       /* -1 = pared */
    bool secreta;       /* parece pared, pero se puede atravesar */
    int  textura;
    fixo comprimento;
    fixo nx, ny;        /* normal de longitud 1 apuntando hacia dentro de la sala */
} lado_t;

typedef struct {
    int    n;
    lado_t lados[MAX_LADOS];
    fixo   chao, teto;
    int    luz;            /* 0 (oscuridad) a 15 (sol) */
    int    piso, forro;    /* texturas; techo T_CEU = cielo abierto */
} sala_t;

enum { C_BANANA, C_CACHO, C_POTE, C_PORTEIRA, C_CASCA, C_MACACO, C_SEMENTE_VOANDO };

typedef struct {
    int  tipo;
    bool ativa;
    fixo x, y;
    int  sala;
    /* monos */
    int  estado, tempo, acertos, ang, desvio;
    fixo alvo_x, alvo_y;
    bool tem_alvo;
    /* semillas volando */
    fixo vx, vy, z;
} coisa_t;

typedef struct {
    char    nome[48];
    int     minimo;                /* bananas para abrir la cerca */
    int     qtd_pontos, qtd_salas, qtd_coisas;
    ponto_t pontos[MAX_PONTOS];
    sala_t  salas[MAX_SALAS];
    coisa_t coisas[MAX_COISAS];
    fixo    inicio_x, inicio_y;
    int     inicio_ang, inicio_sala;
    int     total_bananas, passagens, paredes;
} mapa_t;

extern mapa_t mapa;
bool mapa_carregar(const char *caminho, char *erro, int tam_erro);
int  sala_do_ponto(fixo x, fixo y, int palpite);   /* -1 = fuera del mapa */
bool dentro_da_sala(int s, fixo x, fixo y);
/* Distancia (con signo: positiva dentro) del punto a la recta del lado. */
fixo distancia_ao_lado(const sala_t *s, int l, fixo x, fixo y);
bool linha_livre(int sala, fixo ax, fixo ay, fixo bx, fixo by, fixo altura);

/* ------------------------------------------------------------ dibujo 3D */
typedef struct {
    fixo x, y, z;      /* z = altura del ojo */
    int  ang;
    int  sala;
} camera_t;
void tela3d_desenhar(const camera_t *c, int relogio);

/* ------------------------------------------------------------ mundo */
#define TIQUE_MS 30                /* la lógica avanza en pasos de 30 ms */
#define OLHO     40                /* altura del ojo sobre el suelo */
#define ALTURA_SAGUI 56
#define DEGRAU   24                /* el mayor escalón que se puede subir */
#define RAIO_SAGUI  14
#define RAIO_MACACO 16

enum { M_QUIETO, M_CACANDO, M_PARADO, M_RECUANDO, M_TONTO, M_ESCORREGANDO, M_FUGINDO, M_FORA };

typedef struct {
    fixo x, y, z;
    int  ang, sala;
    int  vidas, bananas, comidas, cascas, sementes, pontos;
    int  invulneravel, recarga, puxado;
    bool cacho;
    int  espantados;
} sagui_t;

extern sagui_t sagui;
extern int  fase_atual;
extern char aviso[64];
extern int  aviso_tempo;

typedef struct {
    bool frente, tras, esquerda, direita, atirar, correr;
    int  giro_mouse;
} comandos_t;

enum { R_NADA, R_FASE_CONCLUIDA, R_FIM_DE_JOGO };
void mundo_iniciar_fase(void);
int  mundo_tique(const comandos_t *cmd);
void mundo_largar_casca(void);
bool porteira_aberta(void);
void mundo_mostrar_aviso(const char *t);

/* ------------------------------------------------------------ sonidos (bip) */
void som_tiro(void);
void som_banana(void);
void som_acerto(void);
void som_vida_perdida(void);
void som_fase_concluida(void);
void som_casca(void);
void som_escorregou(void);
void som_cacho(void);
void som_porteira_fechada(void);

#endif
