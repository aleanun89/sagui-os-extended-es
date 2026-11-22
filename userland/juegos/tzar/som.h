/* SPDX-License-Identifier: GPL-3.0-or-later */
/* som.h - sonido de Tzar: efectos WAV por evento, voces, ambiente, película y música MIDI (D417, fase 7).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): src/audio.rs (categorías, canales,
 * prioridades, atenuación por distancia, elección de hueco) y de frontend/gameplay.rs el reparto de eventos a sonidos.
 * El sonido SOLO lee eventos del mundo (TzEvento, con su propio cursor) y consulta el estado de los objetos; no escribe en TzMundo.
 *
 * Una mezcladora de 30 huecos como el original (3 de naturaleza, 19 de efectos, 4 de voces de unidades y 4 de edificios) más una voz
 * de habla, el sonido de la película y la música, a 44100 Hz estéreo de 16 bits. Sin dispositivo (tz_som_criar con dispositivo 0)
 * no abre nada: las pruebas piden el PCM con tz_som_misturar. */
#ifndef TZAR_SOM_H
#define TZAR_SOM_H

#include "mundo.h"

typedef struct TzSom TzSom;
typedef struct TzMusica TzMusica;

enum { TZ_CANAL_NATUREZA = 0, TZ_CANAL_EFEITO, TZ_CANAL_UNIDADE, TZ_CANAL_EDIFICIO, TZ_CANAIS };

/* categorías del original (audio.rs category::*) */
enum {
	TZ_CAT_NENHUMA = 0, TZ_CAT_SELECAO = 1, TZ_CAT_ORDEM = 2, TZ_CAT_CRIAR = 3, TZ_CAT_CORTAR = 4, TZ_CAT_CONSTRUIR = 5, TZ_CAT_CAVAR = 6,
	TZ_CAT_FOGO = 7, TZ_CAT_TIRO = 8, TZ_CAT_TIRO_PESADO = 9, TZ_CAT_DRAGAO = 10, TZ_CAT_GOLPE = 11, TZ_CAT_FEITICO = 13,
	TZ_CAT_EXPLOSAO = 15, TZ_CAT_SPLASH = 17, TZ_CAT_RAIO = 19, TZ_CAT_OBJ_MAPA = 20, TZ_CAT_MORTE = 22, TZ_CAT_DESABA = 23,
	TZ_CAT_UNIDADE_PRONTA = 24, TZ_CAT_EDIFICIO_PRONTO = 25, TZ_CAT_N = 27
};

/* ------------------------------------------------------------------ vida */
/* Crea el sonido: lee DATA/SOUNDS.INI (grupos y prioridades) del recurso y toma de b las definiciones de objeto (sound_*). Con
 * dispositivo != 0 abre la tarjeta (SDL) y empieza a sonar; si no hay tarjeta devuelve el sonido igualmente, mudo. b puede ser NULL. */
TzSom *tz_som_criar(TzRecursos *r, const TzBiblioteca *b, int dispositivo);
void tz_som_fechar(TzSom *s);
int tz_som_tem_dispositivo(const TzSom *s);
/* El mapa (o campaña) del que salen los «USER\x.wav» y «USER\x.mid» de la misión; vale mientras no se cambie ni se cierre. */
void tz_som_fonte_missao(TzSom *s, TzHpfs *mapa);
/* Volúmenes 0..100: efectos, voces (habla), música. */
void tz_som_volumes(TzSom *s, int efeitos, int fala, int musica);
/* Mezcla `quadros` cuadros estéreo (la llamada de la tarjeta; las pruebas la usan sin tarjeta). */
void tz_som_misturar(TzSom *s, int16_t *saida, int quadros);

/* ------------------------------------------------------------------ partida */
/* Dónde oye el jugador: la casilla del centro de la vista y su ancho en píxeles. Si el centro se mueve, lo que suena se recoloca. */
void tz_som_camara(TzSom *s, int cx, int cy, int largura_px);
/* Lee los eventos nuevos del mundo y los hace sonar; también el ruido del trabajo (talar, minar, construir) y el ambiente. */
void tz_som_eventos(TzSom *s, const TzMundo *m);
/* Un sonido suelto por nombre de recurso («SOUND/NOTIFY/CHAT.WAV») o de grupo («@infantry_cmd»), en el sitio (cx, cy) o sin sitio (-1). */
int tz_som_tocar(TzSom *s, const char *nome, int categoria, int canal, int cx, int cy);
void tz_som_parar_todos(TzSom *s);
/* el vigilante (vigia.c): 1 calla la tarjeta (sin cerrojo: se llama desde otro hilo), 0 la vuelve a poner */
void tz_som_pausar(TzSom *s, int pausar);
/* el cursor de eventos: una partida nueva o cargada empieza a leer desde su cola (seq = TzMundo.ev_seq) */
void tz_som_cursor(TzSom *s, uint32_t seq);
/* tira lo que quede del sonido de la película (al saltar la introducción) */
void tz_som_parar_filme(TzSom *s);
/* Los sonidos puestos en el mapa con el editor (<mapa>.sounds: registros de 112 bytes, world.rs SoundSource): se leen al empezar
 * la misión y suenan solos desde tz_som_eventos (una vez, cada tanto o en bucle) donde el jugador ve. Devuelve cuántos. */
int tz_som_fontes_mapa(TzSom *s, const uint8_t *dados, uint32_t n, const TzMapa *mapa);
int tz_som_nfontes(const TzSom *s);
/* La lluvia (audio/ambience.rs): con chove, el clip de empezar, los del medio uno tras otro y el de acabar cuando para. ms es el
 * reloj del juego (se para con la partida). */
void tz_som_chuva(TzSom *s, int chove, uint32_t ms);
int tz_som_chovendo(const TzSom *s);
/* Habla de una historia o conversación («USER\voz.wav» de la misión): para los efectos y suena; NULL o vacío la calla. */
int tz_som_fala(TzSom *s, const char *wav);
/* Sonido de una película (PCM estéreo de 44100 Hz): se encola; mientras haya, calla el resto. */
void tz_som_filme_empurrar(TzSom *s, const int16_t *pcm, int quadros);
int tz_som_filme_pendente(const TzSom *s);

/* ------------------------------------------------------------------ música (musica.c) */
TzMusica *tz_som_musica(TzSom *s);
int tz_som_ativas(const TzSom *s);   /* voces sonando (para las pruebas) */

/* ------------------------------------------------------------------ piezas que prueba el arnés */
/* atenuación y panorama en centésimas de decibelio (audio.rs attenuation): distancia Manhattan en casillas */
void tz_som_atenuacao(int dx, int dy, int largura_px, int efeito, int *volume_cb, int *pan_cb);
/* el clip de un grupo con pesos para una tirada de 0..1000 (audio.rs pick); -1 si ninguno */
int tz_som_escolher(const int *pesos, int n, int tirada);
/* decodifica un WAV a mono de 16 bits (taxa = la del fichero); 0 si no se admite. *pcm lo libera el que llama. */
int tz_som_wav(const uint8_t *d, uint32_t n, int16_t **pcm, int *quadros, int *taxa);

#endif
