// SPDX-License-Identifier: GPL-3.0-only
/* arte.h - dibujos en pixel art (letras = colores de la paleta). */
#ifndef SAGUI_ARTE_H
#define SAGUI_ARTE_H
#include "tipos.h"

typedef struct {
    const char *const *linhas;
    int largura, altura;
} desenho_t;

extern const desenho_t arte_sagui, arte_icone_arquivos, arte_icone_terminal, arte_icone_editor,
                       arte_icone_calculadora, arte_icone_banana, arte_icone_sagui3d, arte_icone_sobre, arte_icone_pendrive,
                       arte_icone_tarefas, arte_icone_visor, arte_icone_paint,
                       arte_icone_chip8, arte_icone_configuracion, arte_icone_discos;
/* 16x16 (D065): el botón Inicio y los programas sin icono de 32x32. */
extern const desenho_t arte_inicio, arte_mini_pantalla, arte_mini_cipo, arte_mini_markdown, arte_mini_infopc;
extern const desenho_t arte_mini_mico;   /* mejora 037 */
extern const desenho_t arte_mini_bluetooth;   /* D279 */
/* 16x16 dos programas que estavam sem icone no menu Inicio (D139). */
extern const desenho_t arte_mini_recortes, arte_mini_icones, arte_mini_musica, arte_mini_relogio,
                       arte_mini_visorpdf, arte_mini_video, arte_mini_ia;
extern const desenho_t arte_marca_ia;   /* D380: logo «IA Powered» (escritorio, abajo a la izquierda) */
extern const desenho_t arte_mini_vps;     /* mejora 123: VisualPythonS */
extern const desenho_t arte_mini_exploradorbd;   /* mejora 127: Explorador de bases de datos */
extern const desenho_t arte_mini_exploradorsqlite;   /* mejora 131: Explorador SQLite */
extern const desenho_t arte_mini_exploradorlmk;   /* mejora 131: Explorador lite-monkey */
extern const desenho_t arte_mini_pluma;   /* mejora 116: el procesador de textos */
extern const desenho_t arte_mini_arara;   /* D465: diagramas (Arara) */
extern const desenho_t arte_mini_tucan;   /* D369: el editor de presentaciones */
extern const desenho_t arte_mini_macaco;   /* melhoria 078: a folha de calculo */
extern const desenho_t arte_mini_gebramini;   /* mejora 128: geometría dinámica */
extern const desenho_t arte_mini_planomini;   /* mejora 129: mini CAD 2D */
extern const desenho_t arte_mini_jclic;   /* D185: actividades JClic */
extern const desenho_t arte_mini_jclicbajar;   /* D185: bajar actividades JClic */
extern const desenho_t arte_mini_ayuda;   /* D455: Ayuda */
extern const desenho_t arte_mini_jabuti;   /* D377: Jabuti */
extern const desenho_t arte_mini_visorlogs;   /* D463: Visor de registros */
extern const desenho_t arte_mini_snake;   /* D463: Snake */
extern const desenho_t arte_mini_aoen;     /* D382: AoEN */
extern const desenho_t arte_mini_brumacanto; /* D444: Brumacanto */
extern const desenho_t arte_mini_capibara;   /* D452: Capibara */
extern const desenho_t arte_mini_capibara_editor;   /* D460: el editor de Capibara */
#ifndef SAGUI_SEM_GPL
extern const desenho_t arte_mini_tyrian;   /* D180: o OpenTyrian */
extern const desenho_t arte_mini_openjazz;   /* D228: OpenJazz */
extern const desenho_t arte_mini_freeciv;    /* D374: Freeciv */
extern const desenho_t arte_mini_conquer;    /* D230: Command & Conquer (Vanilla Conquer) */
extern const desenho_t arte_mini_quake;      /* D229: Quake */
extern const desenho_t arte_mini_doom;       /* D383: DOOM (Chocolate Doom) */
extern const desenho_t arte_mini_tzar;       /* D417: Tzar */
extern const desenho_t arte_mini_dosbox;     /* D294: DOSBox */
extern const desenho_t arte_mini_recreativa; /* D294: Recreativa */
extern const desenho_t arte_mini_gnuboy;     /* D235: Game Boy (SDL2-GNUBoy) */
extern const desenho_t arte_mini_netsurf;  /* D184: el navegador NetSurf */
#endif
extern const desenho_t arte_mini_buscaminas;   /* D248 */
extern const desenho_t arte_mini_spider;   /* D173: o solitario Spider */
extern const desenho_t arte_mini_chatia;     /* o chat com IA (D143) */
extern const desenho_t arte_mini_correo;     /* o cliente de correio (082, D146) */
bool arte_cor(char letra, uint32_t *cor);

#endif
