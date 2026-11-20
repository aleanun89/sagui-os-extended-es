/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar.h - lectores de los datos de Tzar y escena del visor (D417).
 *
 * Port a SaguiOS de Tzar en C99 + SDL2. Los lectores son una traducción a C de ReTzar
 * (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later; cada .c dice de qué fichero sale).
 * Ningún dato del juego original va con SaguiOS: se leen de la instalación del usuario.
 *
 * Convenciones:
 *  - las funciones devuelven 0 si van bien y -1 si no; el motivo queda en tz_erro();
 *  - los nombres de recurso no distinguen mayúsculas ni la barra (DATA\IMAGES.INI = data/images.ini);
 *  - las imágenes son ARGB de 32 bits: alfa 0 transparente, alfa 0x80 sombra (oscurece al 50 %),
 *    alfa 0xFF opaco;
 *  - coordenadas del mundo en píxeles con y hacia abajo: la casilla (x, y) empieza en (32x, 32y).
 */
#ifndef TZAR_H
#define TZAR_H

#include <stdint.h>
#include <stdio.h>

#define TZ_CASILLA 32
#define TZ_MAX_RECURSO (512u * 1024u * 1024u)   /* nada de los datos puede pasar de aquí */

/* ------------------------------------------------------------------ errores y utilidades */
const char *tz_erro(void);
int tz_falha(const char *fmt, ...);                /* guarda el motivo y devuelve -1 */
uint32_t tz_u32(const uint8_t *p);
uint16_t tz_u16(const uint8_t *p);
void *tz_malloc(size_t n);                        /* malloc que no devuelve NULL sin avisar (tz_falha) */
char *tz_strdup(const char *s);
void tz_chave(char *dst, const char *src, size_t n);   /* nombre normalizado: mayúsculas y '/' */
uint32_t tz_hash(const char *s);

/* ------------------------------------------------------------------ paquetes *.wdt (wdt.c) */
typedef struct {
	char *nome;          /* tal como lo guarda el paquete ('\\' pasado a '/') */
	uint32_t pos, tam;   /* desplazamiento y tamaño dentro del paquete descomprimido */
} TzEntrada;

typedef struct {
	FILE *f;
	uint8_t *mem;        /* paquete ya descomprimido en memoria (pruebas), o NULL */
	char *pasta;         /* carpeta suelta (DATA, IMAGES...) en lugar de paquete, o NULL */
	uint32_t tam;        /* tamaño lógico (descomprimido) */
	uint32_t tam_fisico;
	int lzss;
	uint32_t trozo;      /* tamaño de cada trozo LZSS */
	uint8_t alg;
	uint32_t ntrozos;
	uint32_t *offs;      /* ntrozos + 1 desplazamientos (el último, el final del fichero) */
	uint8_t *cache;      /* un trozo descomprimido */
	uint8_t *cache_comp; /* búfer para el trozo comprimido */
	int64_t cache_i;
	TzEntrada *e;
	int n;
} TzPacote;

int tz_lzss_trozo(const uint8_t *src, uint32_t n, uint8_t alg, uint8_t *dst, uint32_t esperado);
int tz_lzss_desempacotar(const uint8_t *src, uint32_t n, uint8_t **out, uint32_t *tam);
int tz_pacote_abrir(TzPacote *p, const char *ruta);
int tz_pacote_memoria(TzPacote *p, const uint8_t *dados, uint32_t n);   /* copia y desempaqueta */
int tz_pacote_pasta(TzPacote *p, const char *raiz, const char *const *topos, int ntopos);
int tz_pacote_intervalo(TzPacote *p, uint32_t pos, uint32_t tam, uint8_t *dst);
uint8_t *tz_pacote_ler(TzPacote *p, const TzEntrada *e, uint32_t *tam);   /* malloc; NULL si falla */
void tz_pacote_fechar(TzPacote *p);

/* ------------------------------------------------------------------ contenedores HPFS (hpfs.c) */
typedef struct TzHpfs TzHpfs;
struct TzHpfs {
	FILE *f;             /* contenedor en un fichero (solo el de arriba lo cierra) */
	uint8_t *mem;        /* o en memoria */
	TzHpfs *pai;         /* o anidado: tramos dentro del padre */
	uint32_t (*tramos)[3];   /* (inicio lógico, posición en el padre, longitud) */
	int ntramos;
	uint32_t tam;
	uint32_t nblocos;
	TzEntrada *e;        /* pos = inodo */
	int n;
};

int tz_hpfs_abrir(TzHpfs *c, const char *ruta);
int tz_hpfs_memoria(TzHpfs *c, uint8_t *dados, uint32_t n);   /* toma los datos (free al cerrar) */
int tz_hpfs_anidado(TzHpfs *pai, const char *nome, TzHpfs *c);
const TzEntrada *tz_hpfs_entrada(const TzHpfs *c, const char *nome);
uint8_t *tz_hpfs_ler(TzHpfs *c, const char *nome, uint32_t *tam);   /* malloc; NULL si falta o falla */
void tz_hpfs_fechar(TzHpfs *c);

/* ------------------------------------------------------------------ INI (recursos.c) */
typedef struct { char *chave, *valor; } TzIniPar;
typedef struct { char *nome; TzIniPar *p; int n, cap; } TzIniSecao;
typedef struct {
	TzIniSecao *s;
	int ns, caps;
	/* tabla hash de «sección\001clave» -> (sección << 16 | par) y de secciones */
	uint32_t *tab; uint32_t tabcap, tabn;
	uint32_t *tsec; uint32_t tseccap;
} TzIni;

void tz_ini_ler(TzIni *ini, const uint8_t *dados, uint32_t n);   /* se puede llamar varias veces (fusiona) */
const char *tz_ini(const TzIni *ini, const char *secao, const char *chave);
const TzIniSecao *tz_ini_secao(const TzIni *ini, const char *secao);
int tz_ini_numeros(const TzIni *ini, const char *secao, const char *chave, int *v, int max);
void tz_ini_liberar(TzIni *ini);

/* ------------------------------------------------------------------ recursos de una instalación (recursos.c) */
typedef struct {
	TzPacote pac[10];
	int npac;
	char **nomes;        /* claves normalizadas de todo lo que hay, ordenadas */
	uint32_t *idx;       /* tabla hash: (pacote << 24 | entrada) + 1, 0 = vacío */
	uint32_t idxcap;
	int nnomes;
	char raiz[512];
} TzRecursos;

int tz_recursos_abrir(TzRecursos *r, const char *raiz);
int tz_recursos_existe(const TzRecursos *r, const char *nome);
uint8_t *tz_recursos_ler(TzRecursos *r, const char *nome, uint32_t *tam);
void tz_recursos_fechar(TzRecursos *r);
int tz_ini_recurso(TzRecursos *r, TzIni *ini, const char *nome);   /* añade un INI de los paquetes */

/* ------------------------------------------------------------------ imágenes (imagem.c) */
typedef struct { int w, h; uint32_t *px; } TzImg;

#define TZ_SOMBRA 0x80000000u
int tz_bmp_info(const uint8_t *d, uint32_t n, int *w, int *h, int *prof);
/* recorte (x, y, w, h) de un BMP; verde puro transparente si verde_transp; slot 0-7 color del reino o -1 */
int tz_bmp_recorte(const uint8_t *d, uint32_t n, int x, int y, int w, int h, int verde_transp, int slot, TzImg *out);
int tz_bmp(const uint8_t *d, uint32_t n, int verde_transp, int slot, TzImg *out);
int tz_bmp_indexado(const uint8_t *d, uint32_t n);

typedef struct { int cols, rows, w, h, fh, formato, cores; uint32_t base; } TzRle;
int tz_rle_cab(const uint8_t *d, uint32_t n, TzRle *r);
int tz_rle_indexado(const uint8_t *d, uint32_t n);
/* un cuadro (col = columna/dirección, row = fila/tiempo), como ReTzar lo deja tras reordenar */
int tz_rle_quadro(const uint8_t *d, uint32_t n, int col, int row, int slot, TzImg *out);
int tz_rle_validar(const uint8_t *d, uint32_t n);   /* recorre todas las filas */
void tz_img_liberar(TzImg *i);

/* ------------------------------------------------------------------ mapas (mapa.c) */
typedef struct {
	uint16_t id;
	uint16_t classe;
	uint32_t flags;
	uint16_t x, y;
	uint8_t dono, civ;
	uint16_t variante, vida, vida_max;
	char imagem[33];     /* sección de dibujo propia del registro */
	uint8_t *dados;      /* el registro entero (para lo que se lea más adelante) */
	uint32_t ndados;
} TzObjeto;

typedef struct {
	char nome[33], lider[33];
	int ativo, cor, equipe, civ;
	int ia;              /* byte 122: lo juega el ordenador */
	uint32_t recursos[4];
	int ia_modo;         /* byte 121: bits de la IA (1 economía, 2 guerra); 0 si el byte 122 es 2 (world.rs ai_mode) */
	char ia_guiao[33];   /* bytes 89-120: guion de IA del reino (vacío = random.ais) */
	int inicio[2];       /* bytes 84 y 86: casilla de inicio de la partida libre, -1 si no tiene (world.rs) */
} TzReino;

typedef struct {
	int lado;
	uint16_t *casillas;  /* lado*lado */
	char bioma[65];
	int jogadores[2];
	TzObjeto *obj;
	int nobj;
	TzReino reinos[8];
	int nreinos;
	uint32_t diplomacia[64];   /* .diplomacy: actitud de cada reino hacia cada otro (bits 0x10 guerra, 0x20 neutral, 0x40 aliado) */
	int tem_diplomacia;
} TzMapa;

int tz_mapa_ler(TzHpfs *c, TzMapa *m);   /* el primer *.header del contenedor */
int tz_mapa_cor(const TzMapa *m, int dono);   /* color 0-7 de un dueño, -1 si no tiene */
void tz_mapa_liberar(TzMapa *m);
extern const char *const tz_classes[144];
int tz_classe_e_unidade(int c);
int tz_classe_e_edificio(int c);
int tz_classe_e_marcador(int c);

/* ------------------------------------------------------------------ terreno (terreno.c) */
typedef struct {
	char nome[160];
	TzImg img;           /* el atlas entero */
	int cols, rows, quadros_costa, paginas;
} TzFolha;

typedef struct {
	char bioma[65], dir[65];
	TzFolha *folhas;
	int nfolhas;
	int16_t pares[8][8];         /* (a, b) -> folha, -1 si falta */
	int periodo[2];
	/* losetas de transición ya compuestas, por (código, x % periodo, y % periodo) */
	uint32_t *ccod; uint32_t **cpx; uint32_t ccap, cn;
	uint8_t mini[8][3]; int tem_mini[8];
	uint8_t cor_selecao[3];      /* color de las marcas de selección ([colors] selection del bioma) */
	uint8_t mini_arvore[3], mini_penhasco[3];
	/* árboles */
	TzImg arvores, arvores_s;
	int arv_anc[2], arv_s_anc[2];
	/* acantilados: atlas por tipo de terreno */
	TzImg penhascos[8];
	int tem_penhasco[8];
} TzTerreno;

int tz_terreno_carregar(TzTerreno *t, TzRecursos *r, const char *bioma);
/* los 32x32 píxeles ARGB de la casilla (x, y); NULL y tz_erro si no se puede */
const uint32_t *tz_terreno_loseta(TzTerreno *t, uint16_t cel, int x, int y, int *paso);
/* igual, en el cuadro de agua `tic` (el original corre el agua a WATERFPS = 4: tic = ms / 250) */
const uint32_t *tz_terreno_loseta_t(TzTerreno *t, uint16_t cel, int x, int y, uint32_t tic, int *paso);
int tz_terreno_minimapa(TzTerreno *t, const TzMapa *m, uint32_t *px);   /* lado*lado */
void tz_terreno_liberar(TzTerreno *t);

/* ------------------------------------------------------------------ escena: qué se dibuja y en qué orden (escena.c) */
enum { TZ_CAPA_SOMBRA = 0, TZ_CAPA_CORPO = 1, TZ_CAPA_ESTANDARTE = 2 };

typedef struct {
	int img;             /* índice en TzEscena.imgs (el cuadro que toca; tz_animar lo cambia) */
	int x, y;            /* esquina de arriba a la izquierda, en el mundo */
	int prioridade;
	int capa;
	int chao;            /* 1 = parche de suelo (debajo de todo), 2 = acantilado */
	float profundidade;
	int ordem;
	int obj;             /* índice en TzMapa.obj, -1 para árboles y acantilados */
	int anim;            /* índice en TzEscena.anims, -1 si no se anima */
	int voo;             /* 2 * índice en TzEscena.voos (+1 la sombra), -1 si no vuela */
	int sel[4];          /* caja de selección en el mundo (x, y, w, h) del cuerpo; w = 0 si no tiene */
} TzVisual;

/* ------------------------------------------------------------------ tiempos de animación (animacao.c) */
/* Traducido de ReTzar, src/animation.rs: línea de tiempo de una hoja, independiente de la simulación y de los
 * cuadros por segundo del dibujo. */
typedef struct { uint64_t *fins; int n; uint64_t duracao; } TzTempo;   /* fins[k] = dónde acaba el cuadro k, en ms */
int tz_tempo_novo(TzTempo *t, const uint64_t *atrasos, int n);
int tz_tempo_uniforme(TzTempo *t, int rows, uint64_t periodo);
/* hojas con direcciones: `anim_time` (1000 ms), `delay` por fila, rellenados de 1 ms en 1 ms hasta anim_time */
int tz_tempo_direcional(TzTempo *t, const TzIni *ini, const char *secao, int rows);
/* hojas de una columna con `fps` (o `fps_padrao`); -1 si la imagen se queda quieta (no es un error) */
int tz_tempo_passos(TzTempo *t, const TzIni *ini, const char *secao, int rows, int fps_padrao);
int tz_tempo_quadro(const TzTempo *t, uint64_t decorrido, uint64_t fase);
uint64_t tz_tempo_inicio(const TzTempo *t, int k);
uint64_t tz_tempo_atraso(const TzTempo *t, int k);
void tz_tempo_liberar(TzTempo *t);

typedef struct {
	TzIni imagens;       /* IMAGES.INI con el común y el del bioma */
	TzIni *objdefs;      /* DATA/OBJECTS/<clase>.INI */
	char **objclasse;
	int nobjdefs;
} TzBiblioteca;

/* Una animación de la escena: los cuadros (índices en imgs) y su línea de tiempo. */
typedef struct { TzTempo t; int nq; int *q; uint64_t fase; } TzAnim;

/* Un pájaro en vuelo (ReTzar, src/flight.rs): la ruta es al azar dentro del mapa, de casilla en casilla, con las
 * vueltas del original; sin buscador de caminos de vuelo (el original da rodeos por la niebla). */
typedef struct {
	int corpo, sombra;   /* índices de visuales (sombra -1 si no tiene) */
	int ncol, nfil;
	int *fq, *fs;        /* ncol * nfil índices de imgs (cuerpo y sombra) */
	int passo_ms, passos_quadro;
	int inicio[2], desvio[2], faixa[2];
	int coluna;
	uint32_t semente, aleatorio;
	int pos[2], alvo[2], tem_alvo, direcao, col, fil, passos, batidas, volta[2], tem_volta, passos_casa;
	int rota[22][2], nrota;
	int elevacao[2];
	uint64_t pendente_ms, ultimo_ms;
} TzVoo;

typedef struct {
	TzImg *imgs; int nimgs, capimgs;
	char **chaves;       /* clave de cada imagen («sección|color|col|fila») para no repetir */
	TzVisual *v; int nv, capv;
	int sem_imagem;      /* objetos sin dibujo definido */
	int falhas;          /* dibujos que no se pudieron leer */
	char ultima_falha[200];
	/* animación */
	TzAnim *anims; int nanims, capanims;
	TzVoo *voos; int nvoos;
	int *animv; int nanimv;   /* visuales que cambian de cuadro o de sitio */
	void *mt;            /* el montador (hojas leídas), para dibujar objetos con pose: tz_dibujar_objeto */
	int lado;
} TzEscena;

int tz_biblioteca_carregar(TzBiblioteca *b, TzRecursos *r, const char *bioma);
const char *tz_imagem_objeto(const TzBiblioteca *b, const TzObjeto *o);
void tz_biblioteca_liberar(TzBiblioteca *b);
int tz_escena_montar(TzEscena *e, TzBiblioteca *b, TzRecursos *r, TzTerreno *t, const TzMapa *m);
void tz_escena_liberar(TzEscena *e);
/* Pone en cada visual el cuadro del instante `ms` (desde que se abrió el mapa) y mueve los pájaros. */
void tz_animar(TzEscena *e, uint64_t ms);
float tz_profundidade(int x, int y, int lado);   /* orden del pintor de algo en el píxel (x, y) */

/* ------------------------------------------------------------------ lienzo, texto y fuentes (dibujo.c) */
typedef struct { uint32_t *px; int w, h, passo; } TzLienzo;   /* ARGB/RGB de 32 bits, passo en píxeles */
void tz_ret(TzLienzo *l, int x, int y, int w, int h, uint32_t cor);
void tz_ret_escuro(TzLienzo *l, int x, int y, int w, int h);   /* oscurece al 50 % */
void tz_pintar(TzLienzo *l, const TzImg *im, int x, int y);    /* alfa 0 nada, 0x80 sombra, resto opaco */

typedef struct {
	int alt, larg, nglifos, niveis;
	uint8_t avanco[256], glifo[256];
	uint8_t *px;         /* nglifos * alt filas de larg píxeles de cobertura */
} TzFonte;
int tz_fonte_ler(TzFonte *f, const uint8_t *d, uint32_t n);   /* *.FNT de Haemimont (ReTzar, src/font.rs) */
int tz_fonte_medir(const TzFonte *f, const char *latin1);
void tz_fonte_texto(const TzFonte *f, TzLienzo *l, int x, int y, const char *latin1, uint32_t cor);
void tz_fonte_liberar(TzFonte *f);

/* ------------------------------------------------------------------ textos y fichas (textos.c) */
typedef struct { TzIni ini; int carregado; } TzTextos;
int tz_textos_carregar(TzTextos *t, TzRecursos *r);   /* LOCAL/STRINGS.INI, STRINGS2 y STRINGS3 fusionados */
const char *tz_texto(const TzTextos *t, const char *secao, const char *chave);   /* bytes Windows-1252 o NULL */
void tz_cp1252_a_latin1(char *s);                      /* 0x80-0x9f -> lo más parecido de Latin-1 (en el sitio) */
int tz_cp1252_a_utf8(const char *s, char *dst, size_t n);
/* nombre de la clase de un objeto en el idioma de la instalación («Infante», «Puerta de piedra») */
int tz_nome_objeto(const TzTextos *t, const TzObjeto *o, char *dst, size_t n);   /* Latin-1 */
int tz_ficha_objeto(const TzTextos *t, const TzObjeto *o, char *dst, size_t n);  /* nombre + vida, Latin-1 */

/* ------------------------------------------------------------------ objetos con estado (animacao.c) */
enum { TZ_POSE_PARADO, TZ_POSE_ANDAR, TZ_POSE_ATACAR, TZ_POSE_CONSTRUIR, TZ_POSE_TRABALHAR, TZ_POSE_FEITICO, TZ_POSE_MORRER };
enum { TZ_TAREFA_MINAR, TZ_TAREFA_CORTAR, TZ_TAREFA_CAPTURAR, TZ_TAREFA_ORDENHAR, TZ_TAREFA_CAVAR };
typedef struct {
	int pose;            /* TZ_POSE_* */
	int direcao;         /* 0-7: sur, sureste, este, noreste, norte, noroeste, oeste, suroeste (columna = dir * cols / 8) */
	int ataque;          /* 0, 1 o 2: image_attack, image_attack2, image_attack3 */
	int tarefa;          /* TZ_TAREFA_* con TZ_POSE_TRABALHAR */
	int linha_ponte;     /* fila del puente levadizo de una puerta, 0-6 (6 = bajado) */
	int em_obra;         /* 1 = obra en construcción: progresa con vida / vida_max */
	int vida, vida_max;  /* para la obra y los muros dañados; 0 = los del registro del mapa */
	uint32_t desde_ms;   /* cuándo empezó esta pose, en el reloj del juego */
} TzEstado;

/* Sección de IMAGES.INI que corresponde a la pose (con las mismas reservas que ReTzar: runtime_image); NULL si no hay. */
const char *tz_seccao_pose(const TzBiblioteca *b, const TzObjeto *o, const TzEstado *s, char *buf, size_t n);
/* Dibuja sombra, cuerpo y estandarte de un objeto en el píxel (px, py) de su casilla (x * 32 + el avance de su paso),
 * en la pose y el instante `ms`. cam = esquina de la vista. Devuelve 0, o -1 si no hay dibujo (tz_erro). */
int tz_dibujar_objeto(TzEscena *e, TzLienzo *l, const TzObjeto *o, const TzEstado *s, int px, int py, int camx, int camy, uint64_t ms);
/* Dibujos sueltos animados: fuego, humo, explosiones, flechas y hechizos. `col` es la dirección de las flechas
 * (hojas de una columna con las 24 direcciones en las filas: fila = dir24). desde_ms = cuándo empezó. -1 si no existe. */
int tz_dibujar_efecto(TzEscena *e, TzLienzo *l, const char *secao, int x, int y, int camx, int camy, uint64_t desde_ms, uint64_t ms, int fila_fija);
/* Escena vacía (sin montar el mapa) lista para tz_dibujar_objeto y tz_dibujar_efecto: la simulación la usa así. */
int tz_escena_preparar(TzEscena *e, TzBiblioteca *b, TzRecursos *r, const TzMapa *m);
/* Lee todos los cuadros de todas las hojas animadas de IMAGES.INI (comprobación de los datos); 0 si no hay fallos. */
int tz_anim_verificar(TzEscena *e, int *hojas, int *cuadros, int *falhas, int *ausentes, char *ultimo, size_t n);
void tz_voo_iniciar(TzVoo *v, int ax, int ay, int coluna);   /* interno */

/* ------------------------------------------------------------------ selección (selecao.c) */
void tz_marco_selecao(TzLienzo *l, int x, int y, int w, int h, uint32_t cor);    /* esquinas de 6 píxeles */
void tz_barra_vida(TzLienzo *l, int x, int y, int casillas, int vida, int vida_max, int mana, int mana_max);
/* El objeto bajo el punto (wx, wy) del mundo: la unidad cuyo centro está más cerca (a menos de 30 píxeles), o
 * el objeto cuya caja contiene el punto y se pinta delante; -1 si no hay. */
int tz_selecao_apontar(const TzEscena *e, const TzMapa *m, int wx, int wy);
/* La caja de selección de un objeto de la escena (primer visual del cuerpo); 0 si no tiene. */
int tz_selecao_caixa(const TzEscena *e, int obj, int *x, int *y, int *w, int *h);


#endif
