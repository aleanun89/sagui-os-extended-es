/* SPDX-License-Identifier: GPL-3.0-or-later */
/* som.c - mezcladora y efectos de sonido de Tzar (D417, fase 7).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): src/audio.rs (CATEGORIES, Channel, attenuation,
 * placement, pick, choose, effect_priority, play) y el reparto de eventos a sonidos de frontend/gameplay.rs. Cada efecto es un WAV de
 * sounds.wdt elegido de un grupo de SOUNDS.INI según los pesos (tirada de 0 a 1000); las clases dan el grupo con sus claves
 * sound_select, sound_command, sound_attack y sound_die. Lo que oye el jugador se atenúa por la distancia Manhattan en casillas
 * a la vista y se reparte entre los dos altavoces. Los huecos, las prioridades y quién desplaza a quién son los del original. */
#include "musica.h"
#include <SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define TAXA        44100
#define N_HUECOS    30
#define VISTA_MAX   1024      /* LAYOUT1024H: lo más ancho que oye el original */

static const struct { const char *chave; int max; } CATEGORIAS[TZ_CAT_N] = {
	{ "", 0 }, { "obj_select", 200 }, { "obj_order", 200 }, { "obj_create", 200 }, { "chop", 4 }, { "build", 4 }, { "dig", 2 },
	{ "fire", 3 }, { "attack_far", 6 }, { "attack_far", 6 }, { "attack_far", 6 }, { "attack_close", 5 }, { "attack_close", 5 },
	{ "attack_far", 5 }, { "explosion", 4 }, { "explosion", 4 }, { "explosion", 4 }, { "explosion", 4 }, { "explosion", 5 },
	{ "explosion", 4 }, { "mapedit_object", 5 }, { "mapedit_action", 5 }, { "death", 3 }, { "death", 3 }, { "obj_unit_ready", 200 },
	{ "obj_building_ready", 200 }, { "obj_taunt", 8 },
};
static const int HUECOS[TZ_CANAIS] = { 3, 19, 4, 4 };   /* naturaleza, efectos, unidades, edificios */
static const int HUECO_INI[TZ_CANAIS] = { 0, 3, 22, 26 };

typedef struct { char nome[100]; uint32_t hash; int16_t *pcm; int n, taxa; } Pcm;

typedef struct {
	const Pcm *pcm;
	uint32_t pos;            /* posición en 1/65536 de muestra */
	uint32_t passo;          /* avance por cuadro de salida */
	int32_t gl, gr;          /* ganancias Q12 */
	uint8_t ativa, repetir, categoria, prioridade;
	uint8_t dono_tipo;       /* 0 nadie, 1 sitio, 2 objeto */
	int dono, ax, ay;        /* donde estaba al empezar (casillas) */
	int volume, pan;         /* centésimas de dB */
} Voz;

/* un sonido del mapa puesto con el editor (<mapa>.sounds, world.rs SoundSource) */
typedef struct {
	char som[48];            /* grupo de SOUNDS.INI o WAV */
	uint8_t modo, atrasado;  /* 0 una vez (o cada periodo s), 1 en bucle, 2 cada R(min, max) s */
	uint16_t periodo, min, max;
	int cx, cy;
	uint32_t prox_ms;        /* cuándo suena la próxima vez (0 = aún no se decidió) */
	int voz;                 /* hueco del bucle que suena (-1 ninguno) */
	const void *pcm;         /* lo que suena en ese hueco (para saber si sigue siendo suyo) */
	uint8_t feito;           /* modo 0 sin periodo: ya sonó */
} FonteMapa;
#define FONTES_MAPA 64

struct TzSom {
	SDL_AudioDeviceID dev;
	TzRecursos *rec;
	const TzBiblioteca *bib;
	TzIni sons; int tem_sons;
	uint8_t prio[TZ_CAT_N];
	Pcm **cache; int ncache, capcache; size_t bytes_cache;   /* cada Pcm aparte: las voces guardan su puntero */
	Voz voz[N_HUECOS];
	Voz fala;
	int16_t *filme; int filme_cap, filme_ini, filme_n;   /* anillo estéreo */
	int cam_cx, cam_cy, cam_larg, cam_listo;
	int vol_efeitos, vol_fala, vol_musica;
	TzMusica *musica;
	TzHpfs *missao;
	uint32_t cursor;
	uint32_t semente;
	uint32_t ultimo_trabalho;
	float amp_tab[251];      /* amplitud para 0, -10, -20 ... -2500 cB */
	FonteMapa fontes[FONTES_MAPA]; int nfontes;
	/* lluvia (audio/ambience.rs Rain): nivel 0-2, terminando, cuándo empieza el clip siguiente (ms del juego) */
	int chuva_ativa, chuva_nivel, chuva_fim; uint32_t chuva_prox;
};

/* ------------------------------------------------------------------ cerrojo */
void tz_som_trancar(TzSom *s) { if (s && s->dev) SDL_LockAudioDevice(s->dev); }
void tz_som_destrancar(TzSom *s) { if (s && s->dev) SDL_UnlockAudioDevice(s->dev); }

static uint32_t sorteio(TzSom *s)
{
	s->semente = s->semente * 1664525u + 1013904223u;
	return s->semente >> 8;
}

/* ------------------------------------------------------------------ atenuación (audio.rs attenuation) */
void tz_som_atenuacao(int dx, int dy, int largura_px, int efeito, int *volume_cb, int *pan_cb)
{
	int64_t k = efeito ? 2500 : 400, m = efeito ? 3 : 2;
	int64_t larg = largura_px < 32 ? 32 : largura_px > VISTA_MAX ? VISTA_MAX : largura_px;
	int64_t cel = (int64_t)(dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
	if (cel > (1 << 20)) cel = 1 << 20;
	int64_t vol = -cel * k / (larg * m / 32);
	if (vol < -k) vol = -10000;
	int64_t ddx = dx;
	if (ddx < -(1 << 20)) ddx = -(1 << 20);
	if (ddx > (1 << 20)) ddx = 1 << 20;
	int64_t pan = ddx * 51200 / larg;
	if (pan < -10000) pan = -10000;
	if (pan > 10000) pan = 10000;
	*volume_cb = (int)vol;
	*pan_cb = (int)pan;
}

int tz_som_escolher(const int *pesos, int n, int tirada)
{
	int64_t baixo = 0;
	for (int i = 0; i < n; ++i) {
		int64_t alto = baixo + pesos[i];
		if (tirada >= baixo && tirada <= alto) return i;
		baixo = alto;
	}
	return -1;
}

/* ------------------------------------------------------------------ WAV */
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t le16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }

int tz_som_wav(const uint8_t *d, uint32_t n, int16_t **out, int *quadros, int *taxa_out)
{
	if (n < 44 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) return 0;
	uint32_t pos = 12;
	int canais = 0, bits = 0, taxa = 0, fmt = 0;
	const uint8_t *dados = NULL;
	uint32_t dlen = 0;
	while (pos + 8 <= n) {
		uint32_t len = le32(d + pos + 4);
		if (!memcmp(d + pos, "fmt ", 4) && pos + 8 + 16 <= n) {
			fmt = (int)le16(d + pos + 8); canais = (int)le16(d + pos + 10); taxa = (int)le32(d + pos + 12); bits = (int)le16(d + pos + 22);
		} else if (!memcmp(d + pos, "data", 4)) {
			dados = d + pos + 8;
			dlen = len;
			if (dlen > n - pos - 8) dlen = n - pos - 8;   /* un fichero cortado vale hasta donde llega */
			break;
		}
		uint64_t sig = (uint64_t)pos + 8 + len + (len & 1);
		if (sig > n) break;
		pos = (uint32_t)sig;
	}
	if (!dados || !dlen || (fmt != 1 && fmt != 0xfffe) || (bits != 8 && bits != 16) || canais < 1 || canais > 2 || taxa < 4000 || taxa > 192000) return 0;
	int bytes = bits / 8;
	uint32_t cuadros = dlen / (uint32_t)(bytes * canais);
	if (!cuadros) return 0;
	int16_t *pcm = malloc((size_t)cuadros * 2);
	if (!pcm) return 0;
	for (uint32_t i = 0; i < cuadros; ++i) {
		int soma = 0;
		for (int c = 0; c < canais; ++c) {
			const uint8_t *p = dados + ((size_t)i * (size_t)canais + (size_t)c) * (size_t)bytes;
			soma += bits == 8 ? ((int)p[0] - 128) * 256 : (int16_t)le16(p);
		}
		pcm[i] = (int16_t)(soma / canais);
	}
	*out = pcm; *quadros = (int)cuadros; *taxa_out = taxa;
	return 1;
}

/* ------------------------------------------------------------------ recursos */
uint8_t *tz_som_ler(TzSom *s, const char *nome, uint32_t *n)
{
	/* «USER\x.wav» o «USER/x.mid» es de la misión (el mapa o la campaña); lo demás, de los paquetes */
	if (s->missao && (!strncasecmp(nome, "USER\\", 5) || !strncasecmp(nome, "USER/", 5))) {
		char b[200];
		snprintf(b, sizeof b, "%s", nome);
		for (char *p = b; *p; ++p) if (*p == '\\') *p = '/';
		uint8_t *d = tz_hpfs_entrada(s->missao, b) ? tz_hpfs_ler(s->missao, b, n) : NULL;
		if (d) return d;
		/* los ficheros de la misión van con el nombre del mapa delante: «MAPS/x.USER/..» o sueltos en MAPS/ */
		for (int i = 0; i < s->missao->n; ++i) {
			const char *e = s->missao->e[i].nome;
			size_t le = strlen(e), lb = strlen(b + 5);
			if (le >= lb && !strcasecmp(e + le - lb, b + 5) && (le == lb || e[le - lb - 1] == '/' || e[le - lb - 1] == '\\')) return tz_hpfs_ler(s->missao, e, n);
		}
		return NULL;
	}
	if (!s->rec) return NULL;
	return tz_recursos_ler(s->rec, nome, n);
}

static const Pcm *carregar(TzSom *s, const char *nome)
{
	uint32_t h = tz_hash(nome);
	for (int i = 0; i < s->ncache; ++i) if (s->cache[i]->hash == h && !strcasecmp(s->cache[i]->nome, nome)) return s->cache[i]->pcm ? s->cache[i] : NULL;
	if (s->ncache == s->capcache) {
		int nc = s->capcache ? s->capcache * 2 : 64;
		Pcm **nv = realloc(s->cache, (size_t)nc * sizeof *nv);
		if (!nv) return NULL;
		s->cache = nv; s->capcache = nc;
	}
	Pcm *p = calloc(1, sizeof *p);
	if (!p) return NULL;
	s->cache[s->ncache++] = p;
	snprintf(p->nome, sizeof p->nome, "%s", nome);
	p->hash = h;
	uint32_t n;
	uint8_t *d = tz_som_ler(s, nome, &n);
	if (!d) return NULL;
	if (!tz_som_wav(d, n, &p->pcm, &p->n, &p->taxa)) p->pcm = NULL;
	free(d);
	if (p->pcm) s->bytes_cache += (size_t)p->n * 2;
	return p->pcm ? p : NULL;
}

/* la caché de sonidos no pasa de 48 MB: lo que no suena se suelta */
static void purgar(TzSom *s)
{
	if (s->bytes_cache < 48u << 20) return;
	tz_som_trancar(s);
	for (int i = 0; i < s->ncache; ++i) {
		Pcm *p = s->cache[i];
		if (!p->pcm) continue;
		int uso = s->fala.ativa && s->fala.pcm == p;
		for (int v = 0; v < N_HUECOS && !uso; ++v) if (s->voz[v].ativa && s->voz[v].pcm == p) uso = 1;
		if (uso) continue;
		s->bytes_cache -= (size_t)p->n * 2;
		free(p->pcm);
		p->pcm = NULL;
		p->hash = 0;
		p->nome[0] = 1;   /* marca de libre: ninguna búsqueda la encuentra */
	}
	tz_som_destrancar(s);
}

/* ------------------------------------------------------------------ grupos de SOUNDS.INI */
typedef struct { char nome[100]; int peso; } Cand;

static int grupo(const TzSom *s, const char *chave, Cand *out, int max)
{
	int n = 0;
	size_t l = strlen(chave);
	if (l > 4 && !strcasecmp(chave + l - 4, ".wav")) {
		snprintf(out[0].nome, sizeof out[0].nome, "SOUND/%s", chave);
		for (char *p = out[0].nome; *p; ++p) if (*p == '\\') *p = '/';
		out[0].peso = 1000;
		return 1;
	}
	if (!s->tem_sons) return 0;
	const TzIniSecao *sec = tz_ini_secao(&s->sons, chave);
	if (!sec) return 0;
	for (int i = 0; i < sec->n && n < max; ++i) {
		int peso = atoi(sec->p[i].valor);
		if (peso <= 0) continue;
		snprintf(out[n].nome, sizeof out[n].nome, "SOUND/%s", sec->p[i].chave);
		for (char *p = out[n].nome; *p; ++p) if (*p == '\\') *p = '/';
		out[n].peso = peso;
		n++;
	}
	return n;
}

/* la clave de un objeto: la sección de su civilización y, si no, «comm» (como las imágenes) */
static const char *propriedade(const TzSom *s, int classe, int civ, const char *chave)
{
	if (!s->bib || classe < 0 || classe >= 144) return NULL;
	for (int i = 0; i < s->bib->nobjdefs; ++i) {
		if (strcmp(s->bib->objclasse[i], tz_classes[classe])) continue;
		const TzIni *d = &s->bib->objdefs[i];
		const char *v = tz_ini(d, civ == 2 ? "arab" : civ == 3 ? "asia" : "euro", chave);
		if (!v || !*v) v = tz_ini(d, "comm", chave);
		return v && *v ? v : NULL;
	}
	return NULL;
}

/* ------------------------------------------------------------------ huecos (audio.rs choose) */
static int distancia(const TzSom *s, int x, int y)
{
	return abs(s->cam_cx - x) + abs(s->cam_cy - y);
}

static int escolher_buraco(TzSom *s, int canal, const Voz *nova, int no_limite)
{
	int ini = HUECO_INI[canal], n = HUECOS[canal];
	int perto = distancia(s, nova->ax, nova->ay);
	for (int passo = no_limite ? 2 : 0; passo < 3; ++passo)
		for (int k = 0; k < n; ++k) {
			const Voz *v = &s->voz[ini + k];
			int toma = 0;
			if (!v->ativa) toma = !no_limite;
			else if (passo == 1) toma = nova->prioridade > v->prioridade;
			else if (passo == 2 && no_limite) toma = nova->categoria != 0 && v->categoria == nova->categoria && perto <= distancia(s, v->ax, v->ay);
			else if (passo == 2) toma = v->prioridade == nova->prioridade && perto <= distancia(s, v->ax, v->ay);
			if (toma) return ini + k;
		}
	return -1;
}

static int32_t ganho(TzSom *s, int cb, int volume_pct)
{
	int idx = (-cb) / 10;
	float a = cb >= 0 ? 1.0f : idx > 250 ? 0.0f : s->amp_tab[idx];
	return (int32_t)(a * 4096.0f * (float)volume_pct / 100.0f);
}

static void gains_voz(TzSom *s, Voz *v, int canal)
{
	int pct = canal == TZ_CANAL_EFEITO || canal == TZ_CANAL_NATUREZA ? s->vol_efeitos : s->vol_fala;
	int pan = v->pan;
	int32_t g = ganho(s, v->volume, pct);
	/* pan > 0: se atenúa el izquierdo (DirectSound) */
	int32_t gl = pan > 0 ? (int32_t)((int64_t)g * ganho(s, -pan, 100) >> 12) : g;
	int32_t gr = pan < 0 ? (int32_t)((int64_t)g * ganho(s, pan, 100) >> 12) : g;
	v->gl = gl; v->gr = gr;
}

/* devuelve el hueco que tomó o -1 */
static int tocar_pcm_k(TzSom *s, const Pcm *pcm, int categoria, int canal, int tem_sitio, int cx, int cy, int dono, int repetir)
{
	int volume = 0, pan = 0;
	if (tem_sitio) {
		int dx = cx - s->cam_cx, dy = cy - s->cam_cy;
		if (canal == TZ_CANAL_EFEITO) {
			tz_som_atenuacao(dx, dy, s->cam_larg, 1, &volume, &pan);
			if (volume <= -2500) return -1;
		} else {
			tz_som_atenuacao(dx, dy, s->cam_larg, 0, &volume, &pan);
			if (volume < -400) volume = -400;   /* las voces de unidades y edificios nunca bajan de -400 */
		}
	}
	int base = categoria >= 0 && categoria < TZ_CAT_N ? s->prio[categoria] : 0;
	int prioridade = base, no_limite = 0;
	Voz nova;
	memset(&nova, 0, sizeof nova);
	if (canal == TZ_CANAL_EFEITO) {
		int tocando = 0;
		for (int k = 0; k < HUECOS[canal]; ++k) {
			const Voz *v = &s->voz[HUECO_INI[canal] + k];
			if (v->ativa && v->categoria == categoria) tocando++;
		}
		int max = categoria >= 0 && categoria < TZ_CAT_N ? CATEGORIAS[categoria].max : 0;
		prioridade = base == 255 ? base : base / (tocando + 1);
		no_limite = tocando >= max;
	}
	nova.categoria = (uint8_t)categoria; nova.prioridade = (uint8_t)prioridade;
	nova.ax = tem_sitio ? cx : s->cam_cx; nova.ay = tem_sitio ? cy : s->cam_cy;
	nova.dono_tipo = !tem_sitio ? 0 : dono >= 0 ? 2 : 1;
	nova.dono = dono; nova.volume = volume; nova.pan = pan;
	tz_som_trancar(s);
	int k = escolher_buraco(s, canal, &nova, no_limite);
	if (k >= 0) {
		nova.pcm = pcm;
		nova.pos = 0;
		nova.passo = (uint32_t)(((uint64_t)pcm->taxa << 16) / TAXA);
		nova.repetir = (uint8_t)repetir;
		nova.ativa = 1;
		gains_voz(s, &nova, canal);
		s->voz[k] = nova;
	}
	tz_som_destrancar(s);
	return k;
}

static int tocar_pcm(TzSom *s, const Pcm *pcm, int categoria, int canal, int tem_sitio, int cx, int cy, int dono, int repetir)
{
	return tocar_pcm_k(s, pcm, categoria, canal, tem_sitio, cx, cy, dono, repetir) >= 0;
}

/* el clip de un nombre de recurso o de un grupo («@grupo», o un nombre sin «/» que es grupo o WAV de SOUND/) */
static const Pcm *escolher_clip(TzSom *s, const char *nome)
{
	Cand c[64];
	int n;
	if (nome[0] == '@') n = grupo(s, nome + 1, c, 64);
	else if (!strchr(nome, '/') && !strchr(nome, '\\')) n = grupo(s, nome, c, 64);
	else {
		n = 1;
		snprintf(c[0].nome, sizeof c[0].nome, "%s", nome);
		c[0].peso = 1000;
		for (char *p = c[0].nome; *p; ++p) if (*p == '\\') *p = '/';
	}
	if (!n) return NULL;
	int pesos[64];
	for (int i = 0; i < n; ++i) pesos[i] = c[i].peso;
	int k = tz_som_escolher(pesos, n, (int)(sorteio(s) % 1001));
	return k < 0 ? NULL : carregar(s, c[k].nome);
}

int tz_som_tocar(TzSom *s, const char *nome, int categoria, int canal, int cx, int cy)
{
	Cand c[64];
	int n;
	char buf[100];
	if (nome[0] == '@') n = grupo(s, nome + 1, c, 64);
	else {
		snprintf(buf, sizeof buf, "%s", nome);
		n = 1;
		snprintf(c[0].nome, sizeof c[0].nome, "%s", buf);
		c[0].peso = 1000;
		for (char *p = c[0].nome; *p; ++p) if (*p == '\\') *p = '/';
	}
	if (!n) return 0;
	int pesos[64];
	for (int i = 0; i < n; ++i) pesos[i] = c[i].peso;
	int k = tz_som_escolher(pesos, n, (int)(sorteio(s) % 1001));
	if (k < 0) return 0;
	const Pcm *p = carregar(s, c[k].nome);
	if (!p) return 0;
	purgar(s);
	return tocar_pcm(s, p, categoria, canal, cx >= 0, cx, cy, -1, 0);
}

void tz_som_parar_todos(TzSom *s)
{
	tz_som_trancar(s);
	for (int i = 0; i < N_HUECOS; ++i) s->voz[i].ativa = 0;
	tz_som_destrancar(s);
}

void tz_som_pausar(TzSom *s, int pausar)
{
	if (s && s->dev) SDL_PauseAudioDevice(s->dev, pausar != 0);
}

void tz_som_cursor(TzSom *s, uint32_t seq)
{
	if (s) s->cursor = seq;
}

void tz_som_parar_filme(TzSom *s)
{
	tz_som_trancar(s);
	s->filme_n = 0; s->filme_ini = 0;
	tz_som_destrancar(s);
}

int tz_som_ativas(const TzSom *s)
{
	int n = 0;
	for (int i = 0; i < N_HUECOS; ++i) n += s->voz[i].ativa;
	return n + s->fala.ativa;
}

/* ------------------------------------------------------------------ habla y película */
int tz_som_fala(TzSom *s, const char *wav)
{
	tz_som_trancar(s);
	s->fala.ativa = 0;
	tz_som_destrancar(s);
	if (!wav || !*wav) return 0;
	const Pcm *p = carregar(s, wav);
	if (!p) return -1;
	tz_som_trancar(s);
	/* al hablar callan los efectos (0x4d0a00) */
	for (int i = 0; i < N_HUECOS; ++i) s->voz[i].ativa = 0;
	memset(&s->fala, 0, sizeof s->fala);
	s->fala.pcm = p;
	s->fala.passo = (uint32_t)(((uint64_t)p->taxa << 16) / TAXA);
	s->fala.gl = s->fala.gr = ganho(s, 0, s->vol_fala);
	s->fala.ativa = 1;
	tz_som_destrancar(s);
	return 0;
}

void tz_som_filme_empurrar(TzSom *s, const int16_t *pcm, int quadros)
{
	tz_som_trancar(s);
	if (s->filme_cap < s->filme_n + quadros) {
		int nc = (s->filme_n + quadros) * 2 + 4096;
		int16_t *nv = malloc((size_t)nc * 4);
		if (nv) {
			for (int i = 0; i < s->filme_n; ++i) { int q = (s->filme_ini + i) % (s->filme_cap ? s->filme_cap : 1); nv[i * 2] = s->filme[q * 2]; nv[i * 2 + 1] = s->filme[q * 2 + 1]; }
			free(s->filme);
			s->filme = nv; s->filme_cap = nc; s->filme_ini = 0;
		}
	}
	if (s->filme_cap >= s->filme_n + quadros)
		for (int i = 0; i < quadros; ++i) {
			int q = (s->filme_ini + s->filme_n + i) % s->filme_cap;
			s->filme[q * 2] = pcm[i * 2]; s->filme[q * 2 + 1] = pcm[i * 2 + 1];
		}
	if (s->filme_cap >= s->filme_n + quadros) s->filme_n += quadros;
	tz_som_destrancar(s);
}
int tz_som_filme_pendente(const TzSom *s) { return s->filme_n; }

/* ------------------------------------------------------------------ mezcla */
void tz_som_misturar(TzSom *s, int16_t *saida, int quadros)
{
	int32_t *mix = calloc((size_t)quadros * 2, sizeof *mix);
	if (!mix) { memset(saida, 0, (size_t)quadros * 4); return; }
	int filme = s->filme_n > 0;
	if (filme) {
		/* la película calla el resto (0x4d0a00) */
		int n = s->filme_n < quadros ? s->filme_n : quadros;
		for (int i = 0; i < n; ++i) {
			int q = (s->filme_ini + i) % s->filme_cap;
			mix[i * 2] = s->filme[q * 2]; mix[i * 2 + 1] = s->filme[q * 2 + 1];
		}
		s->filme_ini = (s->filme_ini + n) % s->filme_cap;
		s->filme_n -= n;
	} else {
		if (s->musica) tz_musica_gerar(s->musica, mix, quadros, s->vol_musica * 256 / 100);
		for (int vi = -1; vi < N_HUECOS; ++vi) {
			Voz *v = vi < 0 ? &s->fala : &s->voz[vi];
			if (!v->ativa) continue;
			const Pcm *p = v->pcm;
			for (int i = 0; i < quadros; ++i) {
				uint32_t idx = v->pos >> 16;
				if (idx >= (uint32_t)p->n) {
					if (v->repetir) { v->pos = 0; idx = 0; } else { v->ativa = 0; break; }
				}
				int32_t a = p->pcm[idx], b = idx + 1 < (uint32_t)p->n ? p->pcm[idx + 1] : a;
				int32_t fr = (int32_t)(v->pos & 0xffff);
				int32_t smp = a + (int32_t)(((int64_t)(b - a) * fr) >> 16);
				mix[i * 2] += (smp * v->gl) >> 12;
				mix[i * 2 + 1] += (smp * v->gr) >> 12;
				v->pos += v->passo;
			}
		}
	}
	for (int i = 0; i < quadros * 2; ++i) saida[i] = (int16_t)(mix[i] > 32767 ? 32767 : mix[i] < -32768 ? -32768 : mix[i]);
	free(mix);
}

static void chamada(void *ud, Uint8 *stream, int len) { tz_som_misturar((TzSom *)ud, (int16_t *)stream, len / 4); }

/* ------------------------------------------------------------------ cámara */
/* volumen y panorama de una voz con sitio desde el centro de la vista (con el cerrojo tomado) */
static void recolocar(TzSom *s, Voz *v, int canal)
{
	int vol, pan;
	tz_som_atenuacao(v->ax - s->cam_cx, v->ay - s->cam_cy, s->cam_larg, canal == TZ_CANAL_EFEITO, &vol, &pan);
	if (canal == TZ_CANAL_EFEITO && vol < -2500 && !v->repetir) { v->ativa = 0; return; }
	if (canal == TZ_CANAL_EFEITO && vol < -2500) vol = -2500;   /* un bucle del mapa lejos: casi mudo, pero sigue */
	if (canal != TZ_CANAL_EFEITO && vol < -400) vol = -400;
	v->volume = vol; v->pan = pan;
	gains_voz(s, v, canal);
}

void tz_som_camara(TzSom *s, int cx, int cy, int largura_px)
{
	int mexeu = !s->cam_listo || abs(cx - s->cam_cx) + abs(cy - s->cam_cy) > 3;
	tz_som_trancar(s);
	s->cam_cx = cx; s->cam_cy = cy; s->cam_larg = largura_px; s->cam_listo = 1;
	if (mexeu) {
		/* lo que suena en un sitio se oye ahora desde el centro nuevo; un efecto que baja de -2500 calla (audio.rs relocate) */
		for (int canal = TZ_CANAL_EFEITO; canal < TZ_CANAIS; ++canal)
			for (int k = 0; k < HUECOS[canal]; ++k) {
				Voz *v = &s->voz[HUECO_INI[canal] + k];
				if (v->ativa && v->dono_tipo != 0) recolocar(s, v, canal);
			}
	}
	tz_som_destrancar(s);
}

/* Las voces que siguen a un objeto (dueño de tipo 2) van a donde está ahora: audio.rs Listener.objects (0x4d01c0), que el
 * original pasa cada vez que la vista se mueve; aquí además cuando el objeto anda con la vista quieta. */
static void seguir_objetos(TzSom *s, const TzMundo *m)
{
	tz_som_trancar(s);
	for (int canal = TZ_CANAL_EFEITO; canal < TZ_CANAIS; ++canal)
		for (int k = 0; k < HUECOS[canal]; ++k) {
			Voz *v = &s->voz[HUECO_INI[canal] + k];
			if (!v->ativa || v->dono_tipo != 2 || v->dono < 0 || v->dono >= m->nobj) continue;
			const TzObj *o = &m->obj[v->dono];
			if (o->cx == v->ax && o->cy == v->ay) continue;
			v->ax = o->cx; v->ay = o->cy;
			recolocar(s, v, canal);
		}
	tz_som_destrancar(s);
}

static void fontes_tic(TzSom *s, const TzMundo *m, uint32_t ms);

/* ------------------------------------------------------------------ eventos del mundo */
static const char *const FEITICOS[21] = {
	NULL, "@spells/lightning.wav", "@spells/heal.wav", NULL, NULL, "@spells/bless.wav", "@spells/freeze.wav", "@spells/confusion.wav",
	NULL, "@spells/invisible.wav", NULL, "@spells/reveal.wav", "@spells/ressurect.wav", NULL, "@spells/turn to stone.wav",
	"@spells/whirlpool.wav", "@spells/crusade.wav", "@spells/flame ball.wav", "@spells/teach.wav", NULL, "@spells/curse.wav",
};

static int visivel(const TzMundo *m, int cx, int cy) { return tz_mundo_nevoa(m, m->jogador, cx, cy) == 2; }

static void voz_objeto(TzSom *s, const TzMundo *m, const TzObj *o, int id, const char *chave, int categoria, int escolhida)
{
	const char *g = propriedade(s, o->classe, o->civ, chave);
	if (!g) return;
	char nome[110];
	snprintf(nome, sizeof nome, "@%s", g);
	int canal = tz_classe_e_edificio(o->classe) ? TZ_CANAL_EDIFICIO : TZ_CANAL_UNIDADE;
	(void)escolhida;
	Cand c[64];
	int n = grupo(s, g, c, 64);
	if (!n) return;
	int pesos[64];
	for (int i = 0; i < n; ++i) pesos[i] = c[i].peso;
	int k = tz_som_escolher(pesos, n, (int)(sorteio(s) % 1001));
	if (k < 0) return;
	const Pcm *p = carregar(s, c[k].nome);
	if (p) tocar_pcm(s, p, categoria, canal, 1, o->cx, o->cy, id, 0);
}

static void efeito_objeto(TzSom *s, const TzMundo *m, const TzObj *o, int id, const char *chave, int categoria)
{
	if (!visivel(m, o->cx, o->cy)) return;
	const char *g = propriedade(s, o->classe, o->civ, chave);
	if (!g) return;
	Cand c[64];
	int n = grupo(s, g, c, 64);
	if (!n) return;
	int pesos[64];
	for (int i = 0; i < n; ++i) pesos[i] = c[i].peso;
	int k = tz_som_escolher(pesos, n, (int)(sorteio(s) % 1001));
	if (k < 0) return;
	const Pcm *p = carregar(s, c[k].nome);
	if (p) tocar_pcm(s, p, categoria, TZ_CANAL_EFEITO, 1, o->cx, o->cy, id, 0);
}

static int categoria_ataque(const TzSom *s, const TzMundo *m, const TzObj *o)
{
	if (o->classe == 32) return TZ_CAT_DRAGAO;
	const TzAtributos *at = m->cat ? tz_catalogo_at(m->cat, o->classe, o->civ) : NULL;
	switch (at ? at->categoria_arma : 0) {
	case 1: return TZ_CAT_TIRO;
	case 2: return TZ_CAT_TIRO_PESADO;
	case 3: return TZ_CAT_FEITICO;
	case 5: return TZ_CAT_EXPLOSAO;
	default: return TZ_CAT_GOLPE;
	}
}

static void aviso(TzSom *s, const char *wav, int canal, int cx, int cy)
{
	tz_som_tocar(s, wav, canal == TZ_CANAL_EDIFICIO ? TZ_CAT_EDIFICIO_PRONTO : TZ_CAT_UNIDADE_PRONTA, canal, cx, cy);
}

void tz_som_eventos(TzSom *s, const TzMundo *m)
{
	TzEvento ev[128];
	int n;
	seguir_objetos(s, m);
	fontes_tic(s, m, m->tic * TZ_TIC_MS);
	while ((n = tz_mundo_eventos(m, &s->cursor, ev, 128)) > 0) {
		for (int i = 0; i < n; ++i) {
			const TzEvento *e = &ev[i];
			const TzObj *o = e->obj >= 0 && e->obj < m->nobj ? &m->obj[e->obj] : NULL;
			int meu = e->reino == m->jogador;
			switch (e->tipo) {
			case TZ_EV_MORREU:
				if (o) efeito_objeto(s, m, o, e->obj, "sound_die", tz_classe_e_edificio(o->classe) ? TZ_CAT_DESABA : TZ_CAT_MORTE);
				break;
			case TZ_EV_ATACOU:
				if (o) efeito_objeto(s, m, o, e->obj, "sound_attack", categoria_ataque(s, m, o));
				break;
			case TZ_EV_ORDEM:
				if (o && meu) voz_objeto(s, m, o, e->obj, "sound_command", TZ_CAT_ORDEM, 0);
				break;
			case TZ_EV_SELECIONADO:
				if (o) voz_objeto(s, m, o, e->obj, "sound_select", TZ_CAT_SELECAO, 0);
				break;
			case TZ_EV_CRIADO:
				if (o && meu && e->valor == 1) aviso(s, "SOUND/NOTIFY/BLDGDONE.WAV", TZ_CANAL_EDIFICIO, o->cx, o->cy);
				else if (o && visivel(m, o->cx, o->cy)) efeito_objeto(s, m, o, e->obj, "sound_create", TZ_CAT_CRIAR);
				break;
			case TZ_EV_OBRA_FIM:
				if (o && meu) aviso(s, "SOUND/NOTIFY/BLDGDONE.WAV", TZ_CANAL_EDIFICIO, o->cx, o->cy);
				break;
			case TZ_EV_TREINADO:
				if (meu && o) aviso(s, "SOUND/NOTIFY/UNITDONE.WAV", TZ_CANAL_UNIDADE, o->cx, o->cy);
				break;
			case TZ_EV_PESQUISA:
				if (meu) tz_som_tocar(s, "SOUND/NOTIFY/TECHDONE.WAV", TZ_CAT_UNIDADE_PRONTA, TZ_CANAL_UNIDADE, -1, -1);
				break;
			case TZ_EV_ATAQUE_AVISO:
				if (meu) tz_som_tocar(s, "SOUND/NOTIFY/ATTACK.WAV", TZ_CAT_UNIDADE_PRONTA, TZ_CANAL_UNIDADE, -1, -1);
				break;
			case TZ_EV_PORTA:
				if (visivel(m, e->x, e->y)) tz_som_tocar(s, e->valor ? "SOUND/GATE/GATE OPEN.WAV" : "SOUND/GATE/GATE CLOSE.WAV", TZ_CAT_ORDEM, TZ_CANAL_EFEITO, e->x, e->y);
				break;
			case TZ_EV_HECHIZO:
				if (e->valor >= 0 && e->valor < 21 && FEITICOS[e->valor] && o && visivel(m, o->cx, o->cy))
					tz_som_tocar(s, FEITICOS[e->valor], TZ_CAT_FEITICO, TZ_CANAL_EFEITO, o->cx, o->cy);
				break;
			case TZ_EV_ESGOTADO:
				if (o && o->classe == TZC_RES_GOLD && visivel(m, o->cx, o->cy)) tz_som_tocar(s, "SOUND/EFFECTS/DESTROY MINE.WAV", TZ_CAT_DESABA, TZ_CANAL_EFEITO, o->cx, o->cy);
				break;
			case TZ_EV_MENSAGEM:
				tz_som_tocar(s, "SOUND/NOTIFY/CHAT.WAV", TZ_CAT_UNIDADE_PRONTA, TZ_CANAL_UNIDADE, -1, -1);
				break;
			case TZ_EV_SOM: {
				const char *nome = tz_mundo_texto(m, e->valor);
				if (nome && *nome) {
					if (!strncasecmp(nome, "USER", 4)) { const Pcm *p = carregar(s, nome); if (p) tocar_pcm(s, p, TZ_CAT_OBJ_MAPA, TZ_CANAL_EFEITO, e->x >= 0, e->x, e->y, -1, 0); }
					else {
						char b[110];
						snprintf(b, sizeof b, "SOUND/%s", nome);
						tz_som_tocar(s, b, TZ_CAT_OBJ_MAPA, TZ_CANAL_EFEITO, e->x, e->y);
					}
				}
				break;
			}
			default: break;
			}
		}
		if (n < 128) break;
	}
	/* el ruido del trabajo: cada 400 ms miramos a quien trabaja cerca y suena su golpe */
	if (m->tic - s->ultimo_trabalho >= 8) {
		s->ultimo_trabalho = m->tic;
		for (int i = 0; i < m->nobj; ++i) {
			const TzObj *o = &m->obj[i];
			if (!o->ator || o->estado != TZ_E_TRABALHANDO) continue;
			if (abs(o->cx - s->cam_cx) + abs(o->cy - s->cam_cy) > 60) continue;
			if (((uint32_t)i + m->tic / 8) % 3) continue;   /* no todos a la vez */
			const char *g = NULL;
			int cat = 0;
			switch (o->ator->trabalho) {
			case TZ_TRAB_CORTAR: g = "@chop"; cat = TZ_CAT_CORTAR; break;
			case TZ_TRAB_MINAR: g = "@mine"; cat = TZ_CAT_CAVAR; break;
			case TZ_TRAB_CONSTRUIR: case TZ_TRAB_REPARAR: g = "@build"; cat = TZ_CAT_CONSTRUIR; break;
			default: break;
			}
			if (g && visivel(m, o->cx, o->cy)) tz_som_tocar(s, g, cat, TZ_CANAL_EFEITO, o->cx, o->cy);
		}
	}
}

/* ------------------------------------------------------------------ sonidos del mapa (<mapa>.sounds) */
static uint16_t le16b(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

int tz_som_fontes_mapa(TzSom *s, const uint8_t *d, uint32_t n, const TzMapa *mapa)
{
	s->nfontes = 0;
	/* world.rs sound_sources (0x440dba): registros de 112 bytes; con +0x68 el sonido es del objeto de id +0x6e, si no de la
	 * casilla (+0x6a, +0x6c) */
	for (uint32_t off = 0; d && off + 112 <= n && s->nfontes < FONTES_MAPA; off += 112) {
		const uint8_t *r = d + off;
		FonteMapa *f = &s->fontes[s->nfontes];
		memset(f, 0, sizeof *f);
		int l = 0;
		while (l < 47 && l < 0x40 && r[0x20 + l]) { f->som[l] = (char)r[0x20 + l]; l++; }
		f->som[l] = 0;
		if (!l) continue;
		f->modo = r[0x60]; f->atrasado = r[0x61];
		f->periodo = le16b(r + 0x62); f->min = le16b(r + 0x64); f->max = le16b(r + 0x66);
		f->cx = le16b(r + 0x6a); f->cy = le16b(r + 0x6c);
		if (r[0x68]) {
			uint16_t id = le16b(r + 0x6e);
			int achou = 0;
			for (int i = 0; mapa && i < mapa->nobj; ++i) if (mapa->obj[i].id == id) { f->cx = mapa->obj[i].x; f->cy = mapa->obj[i].y; achou = 1; break; }
			if (!achou) continue;
		}
		f->voz = -1;
		s->nfontes++;
	}
	return s->nfontes;
}

int tz_som_nfontes(const TzSom *s) { return s->nfontes; }

static uint32_t espera_fonte(TzSom *s, const FonteMapa *f)
{
	if (f->modo == 2) { uint32_t a = f->min, b = f->max >= f->min ? f->max : f->min; return (a + sorteio(s) % (b - a + 1)) * 1000u; }
	return (uint32_t)f->periodo * 1000u;
}

/* gameplay.rs map_sounds: suenan donde la niebla está levantada; los bucles callan si se dejan de ver */
static void fontes_tic(TzSom *s, const TzMundo *m, uint32_t ms)
{
	for (int i = 0; i < s->nfontes; ++i) {
		FonteMapa *f = &s->fontes[i];
		int ve = f->cx >= 0 && f->cy >= 0 && f->cx < m->lado && f->cy < m->lado && tz_mundo_nevoa(m, m->jogador, f->cx, f->cy) == 2;
		if (f->modo == 1) {
			int suena = f->voz >= 0 && s->voz[f->voz].ativa && (const void *)s->voz[f->voz].pcm == f->pcm;
			if (ve && !suena) {
				const Pcm *p = escolher_clip(s, f->som);
				f->voz = p ? tocar_pcm_k(s, p, TZ_CAT_OBJ_MAPA, TZ_CANAL_EFEITO, 1, f->cx, f->cy, -1, 1) : -1;
				f->pcm = f->voz >= 0 ? p : NULL;
			} else if (!ve && suena) {
				tz_som_trancar(s);
				s->voz[f->voz].ativa = 0;
				tz_som_destrancar(s);
				f->voz = -1;
			}
			continue;
		}
		if (f->feito) continue;
		if (!f->prox_ms) f->prox_ms = ms + (f->atrasado ? espera_fonte(s, f) : 0) + 1;
		if (ms < f->prox_ms) continue;
		if (ve) {
			const Pcm *p = escolher_clip(s, f->som);
			if (p) tocar_pcm(s, p, TZ_CAT_OBJ_MAPA, TZ_CANAL_EFEITO, 1, f->cx, f->cy, -1, 0);
		}
		uint32_t e = espera_fonte(s, f);
		if (f->modo == 0 && !e) f->feito = 1;
		else f->prox_ms = ms + (e ? e : 1000);
	}
}

/* ------------------------------------------------------------------ lluvia (audio/ambience.rs, audio.rs rain_clip) */
static const char *clip_chuva(int nivel, const char *clip, char *b, size_t n)
{
	static const char *const niveis[3] = { "LIGHT", "MID", "HEAVY" };
	snprintf(b, n, "SOUND/RAIN/%s/RAIN %s.WAV", niveis[nivel < 0 ? 0 : nivel > 2 ? 2 : nivel], clip);
	return b;
}

/* el clip siguiente empieza 700 ms antes de que acabe este (0x4a81d0) */
static uint32_t duracao_menos(const Pcm *p) { uint32_t ms = p && p->taxa ? (uint32_t)((uint64_t)p->n * 1000 / (uint32_t)p->taxa) : 1000; return ms > 700 ? ms - 700 : ms; }

void tz_som_chuva(TzSom *s, int chove, uint32_t ms)
{
	char b[80];
	if (s->chuva_ativa && s->chuva_fim && chove) s->chuva_ativa = 0;   /* vuelve a llover mientras acaba: otra con nivel nuevo */
	if (!s->chuva_ativa && chove) {
		s->chuva_nivel = (int)(sorteio(s) % 3);
		const Pcm *p = carregar(s, clip_chuva(s->chuva_nivel, "START", b, sizeof b));
		if (p) tocar_pcm(s, p, 0, TZ_CANAL_NATUREZA, 0, -1, -1, -1, 0);
		s->chuva_ativa = 1; s->chuva_fim = 0;
		s->chuva_prox = ms + duracao_menos(p);
		return;
	}
	if (s->chuva_ativa && !chove) s->chuva_fim = 1;
	if (!s->chuva_ativa || ms < s->chuva_prox) return;
	if (s->chuva_fim) {
		const Pcm *p = carregar(s, clip_chuva(s->chuva_nivel, "END", b, sizeof b));
		if (p) tocar_pcm(s, p, 0, TZ_CANAL_NATUREZA, 0, -1, -1, -1, 0);
		s->chuva_ativa = 0;
		return;
	}
	char n[4];
	snprintf(n, sizeof n, "%d", 1 + (int)(sorteio(s) % 6));
	const Pcm *p = carregar(s, clip_chuva(s->chuva_nivel, n, b, sizeof b));
	if (p) tocar_pcm(s, p, 0, TZ_CANAL_NATUREZA, 0, -1, -1, -1, 0);
	s->chuva_prox = ms + duracao_menos(p);
}

int tz_som_chovendo(const TzSom *s) { return s->chuva_ativa && !s->chuva_fim; }

/* ------------------------------------------------------------------ vida */
void tz_som_fonte_missao(TzSom *s, TzHpfs *mapa) { s->missao = mapa; s->nfontes = 0; s->chuva_ativa = 0; }

void tz_som_volumes(TzSom *s, int efeitos, int fala, int musica)
{
	tz_som_trancar(s);
	s->vol_efeitos = efeitos < 0 ? 0 : efeitos > 100 ? 100 : efeitos;
	s->vol_fala = fala < 0 ? 0 : fala > 100 ? 100 : fala;
	s->vol_musica = musica < 0 ? 0 : musica > 100 ? 100 : musica;
	tz_som_destrancar(s);
}

TzMusica *tz_som_musica(TzSom *s) { return s->musica; }
int tz_som_tem_dispositivo(const TzSom *s) { return s->dev != 0; }

TzSom *tz_som_criar(TzRecursos *r, const TzBiblioteca *b, int dispositivo)
{
	TzSom *s = calloc(1, sizeof *s);
	if (!s) return NULL;
	s->rec = r; s->bib = b;
	s->vol_efeitos = 80; s->vol_fala = 90; s->vol_musica = 60;
	s->semente = 417;
	s->cam_larg = 1024;
	for (int i = 0; i <= 250; ++i) s->amp_tab[i] = powf(10.0f, -(float)(i * 10) / 2000.0f);
	if (r && tz_ini_recurso(r, &s->sons, "DATA/SOUNDS.INI") == 0) s->tem_sons = 1;
	for (int i = 0; i < TZ_CAT_N; ++i) {
		const char *v = s->tem_sons && CATEGORIAS[i].chave[0] ? tz_ini(&s->sons, "priority", CATEGORIAS[i].chave) : NULL;
		s->prio[i] = (uint8_t)(v ? atoi(v) : 0);
	}
	char ini[600];
	snprintf(ini, sizeof ini, "%s/Tzar.ini", r ? r->raiz : ".");
	s->musica = tz_musica_criar(s, r, ini);
	if (dispositivo) {
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
			SDL_AudioSpec quer, tem;
			SDL_zero(quer);
			quer.freq = TAXA; quer.format = AUDIO_S16SYS; quer.channels = 2; quer.samples = 1024; quer.callback = chamada; quer.userdata = s;
			s->dev = SDL_OpenAudioDevice(NULL, 0, &quer, &tem, 0);
			if (s->dev && (tem.freq != TAXA || tem.format != AUDIO_S16SYS || tem.channels != 2)) { SDL_CloseAudioDevice(s->dev); s->dev = 0; }
			if (s->dev) SDL_PauseAudioDevice(s->dev, 0);
		}
	}
	return s;
}

void tz_som_fechar(TzSom *s)
{
	if (!s) return;
	if (s->dev) SDL_CloseAudioDevice(s->dev);
	s->dev = 0;
	tz_musica_liberar(s->musica);
	for (int i = 0; i < s->ncache; ++i) { free(s->cache[i]->pcm); free(s->cache[i]); }
	free(s->cache);
	free(s->filme);
	if (s->tem_sons) tz_ini_liberar(&s->sons);
	free(s);
}
