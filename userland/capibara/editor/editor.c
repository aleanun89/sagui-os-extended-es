/* SPDX-License-Identifier: MIT */
/* editor.c - el editor de Capibara (D460), /apps/bin/capibara-editor.
 *
 * Disposición con paneles divisibles: árbol de la escena | lienzo (y código) | inspector, y abajo los recursos del proyecto,
 * la paleta de casillas y la consola. El editor no guarda un árbol propio: lo que edita es el TEXTO de la escena (EscDoc,
 * escritor.c) y cada orden cambia solo las líneas que le tocan, así los comentarios y el orden del fichero se conservan.
 * De ese texto se instancia la escena con el propio motor (modo editor: sin guiones ni sonidos) para dibujarla, elegir
 * nodos con el ratón y leer los valores por omisión del inspector. El lienzo dibuja con el motor (dibujo.c, cámara libre)
 * y un rasterizador por software (raster.c). ▶ Jugar lanza /apps/bin/capibara en otro proceso y recoge su salida.
 * Los textos de la interfaz salen en Latin-1; las escenas y guiones se guardan en UTF-8 (hay que convertir al mostrarlos). */
#include "capi.h"
#include "escritor.h"
#include "raster.h"
#include "sagui.h"
#include "gui.h"
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#define TECLA_F1_ 0x10A   /* F1; F2 = +1... */
#define VENT_L 1000
#define VENT_A 690
#define MENU_A 20
#define BARRA_A 34
#define CAB_A (MENU_A + BARRA_A)
#define TITULO_A 18
#define MAXN 600          /* nodos del árbol */
#define MAXFILAS 200      /* filas del inspector */
#define MAXSEL 64
#define MAXREC 500
#define RUTA 256

enum { H_ARBOL = 1, H_VISTA, H_CODIGO, H_INSPETOR, H_RECURSOS, H_PALETA, H_CONSOLA };
enum { FO_NADA, FO_ARBOL, FO_VISTA, FO_CODIGO, FO_INSPETOR, FO_RECURSOS, FO_PALETA, FO_CONSOLA };
enum { HM_MOVER, HM_GIRAR, HM_ESCALAR, HM_PINTAR, HM_BORRAR };
enum { B_NUEVO, B_ABRIR, B_GUARDAR, B_DESHACER, B_REHACER, B_MOVER, B_GIRAR, B_ESCALAR, B_PINTAR, B_BORRAR, B_REJILLA, B_IMAN, B_JUGAR, B_PARAR, B_EXPORTAR, NBTN };
enum { TIPO_NODO = 1, TIPO_RECURSO = 2 };

/* acciones del menú y del teclado */
enum {
	A_NADA, A_NUEVO_PROY, A_ABRIR_PROY, A_EJEMPLO_PLAT, A_EJEMPLO_ESC, A_NUEVA_ESCENA, A_GUARDAR, A_EXPORTAR, A_SALIR,
	A_DESHACER, A_REHACER, A_DUPLICAR, A_BORRAR, A_RENOMBRAR, A_VARIABLE, A_SENAL, A_INSTANCIA,
	A_JUGAR, A_JUGAR_ESCENA, A_PARAR, A_CREAR_GUION, A_EDITAR_GUION, A_COMPROBAR,
	A_REJILLA, A_IMAN, A_COLISIONES, A_RESTABLECER, A_CENTRAR, A_CAPA_NUEVA, A_CAPA_SIG,
	A_TIPO0 = 100     /* A_TIPO0 + tipo de nodo: añadir un nodo de ese tipo */
};

/* ---------------------------------------------------------------------------------------------------- estado */
static jan_t *jn;
static paineis_t pn;
static barra_menu_t menu;
static mbotao_t btn[NBTN];
static arvore_t arv;
static const char *arv_txt[MAXN]; static unsigned char arv_niv[MAXN], arv_abr[MAXN];
static char arv_nom[MAXN][72]; static int arv_sec[MAXN];
static arvore_t rec;
static const char *rec_txt[MAXREC]; static unsigned char rec_niv[MAXREC], rec_abr[MAXREC];
static char rec_nom[MAXREC][72], rec_ruta[MAXREC][96];
static edvista_t vs;
static inspetor_t ins;
static ins_prop_t props[MAXFILAS];
static edpaleta_t pal;
static imagem_t img_pal; static bool pal_valida; static char pal_ruta[RUTA];
static consola_t con;
static codigo_t *cod; static char cod_arch[128]; static char *cod_guardado; static int cod_guardado_n;   /* el texto del guion tal como está en disco */
static pilha_desfazer_t pila;
static int foco = FO_NADA;
static int modo = HM_MOVER, ver_colisiones = 1, iman_on = 1, cur_capa;
static bool sincronizando;

static char proy_dir[RUTA];                  /* carpeta del proyecto abierto */
static char esc_rel[128];                    /* escena abierta, relativa al proyecto */
static EscDoc *doc;
static bool esc_mod;
static Nodo *live;
static Nodo *lista[MAXN]; static int lista_z[MAXN]; static int nlista;
static char sel[MAXSEL][CAPI_RUTA_MAX]; static int nsel;
static uint32_t *buf_vista; static size_t buf_cap;
static bool avisar_carga;

/* ---------------------------------------------------------------------------------------------------- utilidades */
static void u8_l1(char *dst, const char *src, size_t n) {   /* UTF-8 -> Latin-1 */
	size_t o = 0;
	for (const unsigned char *s = (const unsigned char *)src; *s && o + 1 < n;) {
		if (*s < 0x80) dst[o++] = (char)*s++;
		else if ((*s & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) { int c = ((*s & 0x1F) << 6) | (s[1] & 0x3F); dst[o++] = c < 256 ? (char)c : '?'; s += 2; }
		else if ((*s & 0xF0) == 0xE0) { dst[o++] = '?'; s += s[1] && s[2] ? 3 : 1; }
		else if ((*s & 0xF8) == 0xF0) { dst[o++] = '?'; s += 4; }
		else dst[o++] = (char)*s++;
	}
	dst[o] = 0;
}
static void l1_u8(char *dst, const char *src, size_t n) {   /* Latin-1 -> UTF-8 */
	size_t o = 0;
	for (const unsigned char *s = (const unsigned char *)src; *s && o + 3 < n; ++s) {
		if (*s < 0x80) dst[o++] = (char)*s; else { dst[o++] = (char)(0xC0 | (*s >> 6)); dst[o++] = (char)(0x80 | (*s & 0x3F)); }
	}
	dst[o] = 0;
}

static void fmt_n(char *o, size_t no, double v) {
	char b[40]; snprintf(b, sizeof b, "%.4f", v);
	char *e = b + strlen(b);
	while (e > b && e[-1] == '0') *--e = 0;
	if (e > b && e[-1] == '.') *--e = 0;
	if (!strcmp(b, "-0")) strcpy(b, "0");
	snprintf(o, no, "%s", b);
}

static void ce_con(int nivel, const char *fmt, ...) {
	char t[CONSOLA_TXT * 2];
	va_list ap; va_start(ap, fmt); vsnprintf(t, sizeof t, fmt, ap); va_end(ap);
	t[CONSOLA_TXT - 1] = 0;
	consola_escrever(&con, nivel, t);
}
static void ce_con_linea(int nivel, int linea, const char *texto) {
	char t[CONSOLA_TXT]; snprintf(t, sizeof t, "%s", texto);
	consola_escrever_linha(&con, nivel, linea, t);
}

/* los avisos del cargador de escenas (propiedad desconocida, padre no encontrado...) van a la consola */
static void hook_log(const char *m) {
	if (!avisar_carga) return;
	char t[CONSOLA_TXT]; u8_l1(t, m, sizeof t);
	consola_escrever(&con, CON_AVISO, t);
}

static int existe_dir(const char *r) { struct stat st; return stat(r, &st) == 0 && S_ISDIR(st.st_mode); }
static int existe_fich(const char *r) { struct stat st; return stat(r, &st) == 0 && S_ISREG(st.st_mode); }
static void mkdirs(const char *ruta) {
	char t[RUTA * 2]; snprintf(t, sizeof t, "%s", ruta);
	for (char *p = t + 1; *p; ++p) if (*p == '/') { *p = 0; mkdir(t, 0777); *p = '/'; }
	mkdir(t, 0777);
}
static char *leer_todo(const char *ruta, size_t *n) {
	FILE *f = fopen(ruta, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
	if (l < 0) { fclose(f); return NULL; }
	char *b = malloc((size_t)l + 1);
	size_t r = fread(b, 1, (size_t)l, f); fclose(f);
	b[r] = 0; if (n) *n = r;
	return b;
}
static int escribir_todo(const char *ruta, const char *t, size_t n) {
	char dir[RUTA * 2]; snprintf(dir, sizeof dir, "%s", ruta);
	char *b = strrchr(dir, '/'); if (b) { *b = 0; mkdirs(dir); }
	FILE *f = fopen(ruta, "wb");
	if (!f) return -1;
	size_t w = fwrite(t, 1, n, f); int c = fclose(f);
	return w == n && c == 0 ? 0 : -1;
}
static void ruta_proy(char *out, size_t no, const char *rel) { snprintf(out, no, "%s/%s", proy_dir, rel); }

/* copia recursiva de una carpeta (los ejemplos de /apps van a la carpeta del usuario para poder editarlos) */
static int copiar_dir(const char *origen, const char *destino) {
	mkdirs(destino);
	DIR *d = opendir(origen);
	if (!d) return -1;
	struct dirent *e; int n = 0;
	while ((e = readdir(d)) != NULL) {
		if (e->d_name[0] == '.') continue;
		char o[RUTA * 2], t[RUTA * 2]; snprintf(o, sizeof o, "%s/%s", origen, e->d_name); snprintf(t, sizeof t, "%s/%s", destino, e->d_name);
		if (existe_dir(o)) n += copiar_dir(o, t) > 0 ? 1 : 0;
		else { size_t l; char *b = leer_todo(o, &l); if (b) { if (escribir_todo(t, b, l) == 0) ++n; free(b); } }
	}
	closedir(d);
	return n;
}

static bool termina_en(const char *s, const char *suf) { size_t a = strlen(s), b = strlen(suf); return a >= b && !strcasecmp(s + a - b, suf); }

/* ---------------------------------------------------------------------------------------------------- escena viva */
static void ruta_de_nodo(Nodo *n, char *out, size_t no) {
	if (!n->padre || n == live) { snprintf(out, no, "."); return; }
	char t[CAPI_RUTA_MAX] = "";
	for (Nodo *p = n; p && p != live; p = p->padre) {
		char u[CAPI_RUTA_MAX]; snprintf(u, sizeof u, "%s%s%s", p->nombre, t[0] ? "/" : "", t);
		snprintf(t, sizeof t, "%s", u);
	}
	snprintf(out, no, "%s", t);
}

/* la ruta del nodo en el fichero de la escena: lo que hay dentro de una instancia se elige como la instancia entera */
static void ruta_editable(Nodo *n, char *out, size_t no) {
	ruta_de_nodo(n, out, no);
	while (strcmp(out, ".") && esc_buscar_ruta(doc, out) < 0) { char *b = strrchr(out, '/'); if (b) *b = 0; else snprintf(out, no, "."); }
}

static void recolectar(Nodo *n, int z) {
	if (nlista >= MAXN) return;
	z += n->z;
	lista[nlista] = n; lista_z[nlista++] = z;
	for (int i = 0; i < n->nh; ++i) recolectar(n->hijos[i], z);
}

static void recargar_vivo(void) {
	if (live) nodo_liberar_ya(live);
	live = NULL; nlista = 0;
	size_t n; char *t = esc_texto(doc, &n);
	escena_vaciar_cache();
	char err[200] = "";
	live = escena_instanciar_texto(t, esc_rel, err, sizeof err);
	free(t);
	if (!live) ce_con(CON_ERRO, "%s", err[0] ? err : "escena vacía");
	else recolectar(live, 0);
}

static Nodo *nodo_vivo(const char *ruta) { return live ? nodo_buscar(live, ruta) : NULL; }

static int sel_indice(const char *ruta) { for (int i = 0; i < nsel; ++i) if (!strcmp(sel[i], ruta)) return i; return -1; }
static void sel_limpiar(void) { nsel = 0; }
static void sel_unica(const char *ruta) { nsel = 0; if (ruta && *ruta) { snprintf(sel[0], sizeof sel[0], "%s", ruta); nsel = 1; } }
static void sel_agregar(const char *ruta) { if (nsel < MAXSEL && sel_indice(ruta) < 0) snprintf(sel[nsel++], sizeof sel[0], "%s", ruta); }
static int sel_sec(void) { return nsel ? esc_buscar_ruta(doc, sel[0]) : -1; }
static Nodo *sel_nodo(void) { return nsel ? nodo_vivo(sel[0]) : NULL; }

/* ---------------------------------------------------------------------------------------------------- geometría de nodos */
static float cuadrado(float a) { return a * a; }

/* caja del nodo en coordenadas del mundo: centro, tamaño y giro (grados, sentido horario); 0 si el nodo no tiene caja */
static int caja_mundo(Nodo *n, float *cx, float *cy, float *w, float *h, float *rot) {
	float bx, by, bw, bh, m[6];
	if (!nodo_es2d(n)) return 0;
	nodo_matriz(n, m);
	if (!nodo_caja_local(n, &bx, &by, &bw, &bh)) return 0;
	float lx = bx + bw * 0.5f, ly = by + bh * 0.5f;
	*cx = m[0] * lx + m[2] * ly + m[4]; *cy = m[1] * lx + m[3] * ly + m[5];
	*w = bw * sqrtf(cuadrado(m[0]) + cuadrado(m[1])); *h = bh * sqrtf(cuadrado(m[2]) + cuadrado(m[3]));
	*rot = atan2f(m[1], m[0]) * 180.0f / 3.14159265f;
	return 1;
}

/* ¿el punto del mundo cae sobre el nodo? (los nodos sin caja se eligen por su origen, con un margen) */
static int nodo_en_punto(Nodo *n, float wx, float wy, float margen) {
	if (!nodo_es2d(n) || !(n->flags & F_VISIBLE)) return 0;
	float m[6], bx, by, bw, bh;
	nodo_matriz(n, m);
	float det = m[0] * m[3] - m[1] * m[2];
	int caja = nodo_caja_local(n, &bx, &by, &bw, &bh);
	if (!caja) return fabsf(wx - m[4]) <= margen && fabsf(wy - m[5]) <= margen;
	if (fabsf(det) < 1e-9f) return 0;
	float dx = wx - m[4], dy = wy - m[5];
	float lx = (m[3] * dx - m[2] * dy) / det, ly = (-m[1] * dx + m[0] * dy) / det;
	float mx = margen / (sqrtf(cuadrado(m[0]) + cuadrado(m[1])) + 1e-6f), my = margen / (sqrtf(cuadrado(m[2]) + cuadrado(m[3])) + 1e-6f);
	if (n->tipo == T_MAPA) {   /* un mapa se elige por donde tiene casillas: los huecos son del rectángulo de selección */
		DMapa *d = n->d; int cx = (int)floorf(lx / (float)d->cw), cy = (int)floorf(ly / (float)d->ch);
		if (cx < 0 || cy < 0 || cx >= d->w || cy >= d->h) return 0;
		for (int k = 0; k < d->ncapas; ++k) if (mapa_celda(n, k, cx, cy) >= 0) return 1;
		return 0;
	}
	int fuera = lx >= bx - mx && lx <= bx + bw + mx && ly >= by - my && ly <= by + bh + my;
	if (n->tipo == T_CAMARA) {   /* la cámara es un marco: se elige por su borde, no por lo que encierra */
		int dentro = lx >= bx + 2 * mx && lx <= bx + bw - 2 * mx && ly >= by + 2 * my && ly <= by + bh - 2 * my;
		return fuera && !dentro;
	}
	return fuera;
}

static void caja_aabb(Nodo *n, float *x0, float *y0, float *x1, float *y1) {
	float m[6], bx, by, bw, bh;
	nodo_matriz(n, m);
	if (!nodo_caja_local(n, &bx, &by, &bw, &bh)) { *x0 = m[4] - 6; *y0 = m[5] - 6; *x1 = m[4] + 6; *y1 = m[5] + 6; return; }
	float px[4] = { bx, bx + bw, bx + bw, bx }, py[4] = { by, by, by + bh, by + bh };
	*x0 = *y0 = 1e30f; *x1 = *y1 = -1e30f;
	for (int i = 0; i < 4; ++i) {
		float wx = m[0] * px[i] + m[2] * py[i] + m[4], wy = m[1] * px[i] + m[3] * py[i] + m[5];
		if (wx < *x0) *x0 = wx;
		if (wx > *x1) *x1 = wx;
		if (wy < *y0) *y0 = wy;
		if (wy > *y1) *y1 = wy;
	}
}

static void sel_a_vista(void) {   /* rectángulo con asas del lienzo */
	Nodo *n = sel_nodo();
	if (!n || nsel < 1) { edvista_selecionar(&vs, 0, 0, 0, 0, 0); return; }
	if (nsel == 1) {
		float cx, cy, w, h, r;
		if (caja_mundo(n, &cx, &cy, &w, &h, &r)) {
			int rr = (int)floorf(r + 0.5f) % 360; if (rr < 0) rr += 360;
			edvista_selecionar(&vs, (int)floorf(cx - w * 0.5f + 0.5f), (int)floorf(cy - h * 0.5f + 0.5f), (int)floorf(w + 0.5f) > 0 ? (int)floorf(w + 0.5f) : 1, (int)floorf(h + 0.5f) > 0 ? (int)floorf(h + 0.5f) : 1, rr);
			return;
		}
		float gx, gy; nodo_pos_global(n, &gx, &gy);
		edvista_selecionar(&vs, (int)floorf(gx - 6 + 0.5f), (int)floorf(gy - 6 + 0.5f), 12, 12, 0);
		return;
	}
	float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
	for (int i = 0; i < nsel; ++i) {
		Nodo *k = nodo_vivo(sel[i]); if (!k) continue;
		float a, b, c, d; caja_aabb(k, &a, &b, &c, &d);
		if (a < x0) x0 = a;
		if (b < y0) y0 = b;
		if (c > x1) x1 = c;
		if (d > y1) y1 = d;
	}
	edvista_selecionar(&vs, (int)x0, (int)y0, (int)(x1 - x0) > 0 ? (int)(x1 - x0) : 1, (int)(y1 - y0) > 0 ? (int)(y1 - y0) : 1, 0);
}

/* ---------------------------------------------------------------------------------------------------- dibujo del lienzo */
static uint32_t gris(uint32_t c, int pct) {
	unsigned r = ((c >> 16) & 255) * (unsigned)pct / 100, g = ((c >> 8) & 255) * (unsigned)pct / 100, b = (c & 255) * (unsigned)pct / 100;
	return (r << 16) | (g << 8) | b;
}

/* el cuadro de la cámara: el de la Camara2D de la escena, o el origen con el tamaño del juego */
static void marco_juego(float *x, float *y, float *w, float *h) {
	*x = 0; *y = 0; *w = (float)g_proy.ancho; *h = (float)g_proy.alto;
	for (int i = 0; i < nlista; ++i) if (lista[i]->tipo == T_CAMARA) {
		float cx, cy, bw, bh, r;
		if (caja_mundo(lista[i], &cx, &cy, &bw, &bh, &r)) { *x = cx - bw * 0.5f; *y = cy - bh * 0.5f; *w = bw; *h = bh; }
		return;
	}
}

static void vista_dibujar(void *ctx, jan_t *j, edvista_t *v) {
	(void)ctx;
	int w = v->l, h = v->a;
	if (w <= 0 || h <= 0) return;
	if ((size_t)w * (size_t)h > buf_cap) { buf_cap = (size_t)w * (size_t)h; buf_vista = realloc(buf_vista, buf_cap * 4); }
	uint32_t fondo = ((uint32_t)g_proy.fondo[0] << 16) | ((uint32_t)g_proy.fondo[1] << 8) | g_proy.fondo[2], fuera = gris(fondo, 55);
	float fx, fy, fw, fh; marco_juego(&fx, &fy, &fw, &fh);
	int sx0, sy0, sx1, sy1;
	edvista_para_tela(v, (int)floorf(fx), (int)floorf(fy), &sx0, &sy0); edvista_para_tela(v, (int)floorf(fx + fw), (int)floorf(fy + fh), &sx1, &sy1);
	sx0 -= v->x; sx1 -= v->x; sy0 -= v->y; sy1 -= v->y;
	for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) buf_vista[(size_t)y * (size_t)w + (size_t)x] = (x >= sx0 && x < sx1 && y >= sy0 && y < sy1) ? fondo : fuera;
	if (live) {
		float zf = (float)v->zoom / 1000.0f;
		g_vista_w = w; g_vista_h = h;
		dib_camara_libre(1, ((float)w * 0.5f - (float)v->ox) / zf, ((float)h * 0.5f - (float)v->oy) / zf, zf);
		nodo_fijar_cuadro_dibujo(0, 1.0f);
		g_dep_col = ver_colisiones; g_hud = 0;
		dib_empezar_cuadro(live, 1.0f);
		rast_dibujar(buf_vista, w, h);
	}
	for (int y = 0; y < h && v->y + y < j->alt; ++y) memcpy(&j->px[(size_t)(v->y + y) * (size_t)j->larg + (size_t)v->x], &buf_vista[(size_t)y * (size_t)w], (size_t)w * 4);
	/* rejilla */
	if (v->grade > 0 && v->grade * v->zoom / 1000 >= 4) {
		int g = v->grade, w0, w1, h0, h1;
		edvista_para_mundo(v, v->x, v->y, &w0, &h0); edvista_para_mundo(v, v->x + w, v->y + h, &w1, &h1);
		uint32_t col = mod_mezclar(mod_tema.borde, 0xFFFFFF, 40);
		for (int x = (w0 / g) * g; x <= w1; x += g) edvista_linha(j, v, x, h0, x, h1, col);
		for (int y = (h0 / g) * g; y <= h1; y += g) edvista_linha(j, v, w0, y, w1, y, col);
	}
	edvista_moldura(j, v, (int)floorf(fx), (int)floorf(fy), (int)floorf(fw), (int)floorf(fh), mod_tema.acento);
	/* marcadores de lo que no se ve (nodos sin dibujo propio) y contorno de lo elegido */
	for (int i = 0; i < nlista; ++i) {
		Nodo *n = lista[i]; float cx, cy, bw, bh, r;
		if (!nodo_es2d(n) || !(n->flags & F_VISIBLE)) continue;
		int con_caja = caja_mundo(n, &cx, &cy, &bw, &bh, &r);
		int elegido = 0; char ruta[CAPI_RUTA_MAX]; ruta_de_nodo(n, ruta, sizeof ruta); elegido = sel_indice(ruta) >= 0;
		float gx, gy; nodo_pos_global(n, &gx, &gy);
		if (!con_caja || n->tipo == T_PARTICULAS) {
			int x = (int)gx, y = (int)gy; uint32_t c = elegido ? mod_tema.acento : mod_tema.texto_fraco;
			edvista_linha(j, v, x - 4000 / v->zoom, y, x + 4000 / v->zoom, y, c); edvista_linha(j, v, x, y - 4000 / v->zoom, x, y + 4000 / v->zoom, c);
			edvista_texto(j, v, x + 5000 / v->zoom, y + 3000 / v->zoom, n->nombre, c);
		} else if (elegido && nsel > 1) {
			float a, b, c2, d; caja_aabb(n, &a, &b, &c2, &d);
			edvista_moldura(j, v, (int)a, (int)b, (int)(c2 - a), (int)(d - b), mod_tema.acento);
		}
	}
}

static int vista_acertar(void *ctx, int wx, int wy) {
	(void)ctx;
	float margen = 4000.0f / (float)vs.zoom;
	int mejor = -1, mz = -2000000000, mclase = -1; float marea = 1e30f;
	for (int i = 0; i < nlista; ++i) {
		if (lista[i] == live && lista[i]->tipo == T_NODO2D) continue;   /* la raíz es un agrupador: se elige desde el árbol */
		if (!nodo_en_punto(lista[i], (float)wx, (float)wy, margen)) continue;
		/* gana el de más z; a igual z, lo que no es cámara ni mapa y, entre iguales, la caja más pequeña (lo de dentro) */
		int clase = lista[i]->tipo == T_CAMARA ? 0 : lista[i]->tipo == T_MAPA ? 1 : 2;
		float cx, cy, w, h, r, area = caja_mundo(lista[i], &cx, &cy, &w, &h, &r) ? w * h : 0;
		if (lista_z[i] > mz || (lista_z[i] == mz && (clase > mclase || (clase == mclase && area <= marea)))) { mejor = i; mz = lista_z[i]; mclase = clase; marea = area; }
	}
	return mejor;
}

/* ---------------------------------------------------------------------------------------------------- documento, deshacer */
typedef struct { char *antes, *despues; char clave[200]; } Cmd;
static Cmd *ultimo_cmd;

static void reconstruir_arbol(void);
static void cargar_inspetor(void);
static void sincronizar_todo(void);
static void sincronizar_ligero(void);
static void log_nodos(void);
static void layout_y_log(void);

static void doc_reemplazar(const char *texto) {
	esc_cerrar(doc);
	doc = esc_abrir_texto(texto);
	esc_mod = true;
	/* la selección sigue por rutas; lo que ya no existe se quita */
	int k = 0;
	for (int i = 0; i < nsel; ++i) if (esc_buscar_ruta(doc, sel[i]) >= 0) { if (k != i) memcpy(sel[k], sel[i], sizeof sel[0]); ++k; }
	nsel = k;
	recargar_vivo();
}
static void ap_texto(void *d, bool refazer) { Cmd *c = d; doc_reemplazar(refazer ? c->despues : c->antes); ultimo_cmd = NULL; }
static void lib_cmd(void *d) { Cmd *c = d; if (c == ultimo_cmd) ultimo_cmd = NULL; free(c->antes); free(c->despues); free(c); }

static char *snap(void) { return esc_texto(doc, NULL); }

/* Registra el cambio ya hecho en doc (antes = texto previo; toma su propiedad). clave no vacía: los cambios seguidos de lo mismo se funden. */
static void commit(const char *nombre, char *antes, const char *clave) {
	char *des = snap();
	if (!strcmp(antes, des)) { free(antes); free(des); return; }
	esc_mod = true;
	{ unsigned h = 2166136261u; for (const char *q = des; *q; ++q) { h ^= (unsigned char)*q; h *= 16777619u; } registrar("capibara-editor: cambio %s hash %08x\n", nombre, h); }
	Cmd *top = ed_desfazer_topo(&pila);
	if (clave && clave[0] && top && top == ultimo_cmd && !strcmp(top->clave, clave)) { free(top->despues); top->despues = des; free(antes); return; }
	Cmd *c = calloc(1, sizeof *c);
	c->antes = antes; c->despues = des; snprintf(c->clave, sizeof c->clave, "%s", clave ? clave : "");
	ed_desfazer_registrar(&pila, nombre, ap_texto, lib_cmd, c);
	ultimo_cmd = c;
}

static void esc_guardar_ahora(void);

/* ---------------------------------------------------------------------------------------------------- árbol de la escena */
static int narv;

static void arbol_recorrer(int sec, int nivel) {
	if (narv >= MAXN) return;
	const EscSec *s = &doc->sec[sec];
	char t[CAPI_NOME_MAX + 40];
	if (s->clase == ES_INSTANCIA) snprintf(t, sizeof t, "%s [Instancia]", s->nombre); else snprintf(t, sizeof t, "%s (%s)", s->nombre, s->tipo);
	u8_l1(arv_nom[narv], t, sizeof arv_nom[0]);
	arv_sec[narv] = sec; arv_niv[narv] = (unsigned char)nivel; arv_abr[narv] = 1;
	int yo = narv++;
	(void)yo;
	const char *base = esc_base_hijos(doc, sec);
	for (int i = 0; i < doc->nsec; ++i) {
		const EscSec *h = &doc->sec[i];
		if ((h->clase != ES_NODO && h->clase != ES_INSTANCIA) || i == sec || !strcmp(h->ruta, ".")) continue;
		if (!strcmp(h->padre, base)) arbol_recorrer(i, nivel + 1);
	}
}

static void reconstruir_arbol(void) {
	narv = 0;
	int raiz = esc_buscar_ruta(doc, ".");
	if (raiz >= 0) arbol_recorrer(raiz, 0);
	for (int i = 0; i < narv; ++i) arv_txt[i] = arv_nom[i];
	arv.textos = arv_txt; arv.nivel = arv_niv; arv.aberto = arv_abr; arv.n = narv;
	arv.sel = -1;
	if (nsel) { int s = esc_buscar_ruta(doc, sel[0]); for (int i = 0; i < narv; ++i) if (arv_sec[i] == s) arv.sel = i; }
	if (arv.sel >= 0) arvore_mostrar(&arv, arv.sel);
	for (int i = 0; i < narv && i < 30; ++i) registrar("capibara-editor: arbolfila %d %s\n", i, doc->sec[arv_sec[i]].ruta);   /* para las pruebas */
}

/* fila (índice en la tabla) del árbol bajo la coordenada y de la ventana, o -1 */
static int arbol_fila_en(int y) {
	int fila = (y - arv.y - 2) / 20 + arv.topo, vis = 0;
	if (y < arv.y) return -1;
	for (int i = 0; i < arv.n; i++) if (arvore_visivel(&arv, i)) { if (vis == fila) return i; vis++; }
	return -1;
}

/* ---------------------------------------------------------------------------------------------------- recursos del proyecto */
static int nrec;
static void rec_agregar(const char *txt, int niv, const char *ruta) {
	if (nrec >= MAXREC) return;
	u8_l1(rec_nom[nrec], txt, sizeof rec_nom[0]);
	snprintf(rec_ruta[nrec], sizeof rec_ruta[0], "%s", ruta);
	rec_niv[nrec] = (unsigned char)niv; rec_abr[nrec] = 1; ++nrec;
}
static int cmp_cad(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
static int juntar_ficheros(const char *rel, char (*lista_f)[96], int max, int n) {
	char dir[RUTA * 2]; snprintf(dir, sizeof dir, "%s/%s", proy_dir, rel);
	DIR *d = opendir(dir);
	if (!d) return n;
	struct dirent *e;
	while ((e = readdir(d)) != NULL && n < max) {
		if (e->d_name[0] == '.') continue;
		char r[96], full[RUTA * 2 + 2];
		snprintf(r, sizeof r, "%s%s%s", rel, *rel ? "/" : "", e->d_name);
		snprintf(full, sizeof full, "%s/%s", proy_dir, r);
		if (existe_dir(full)) n = juntar_ficheros(r, lista_f, max, n); else snprintf(lista_f[n++], 96, "%s", r);
	}
	closedir(d);
	return n;
}
static void reconstruir_recursos(void) {
	static char fich[MAXREC][96];
	int n = proy_dir[0] ? juntar_ficheros("", fich, MAXREC, 0) : 0;
	qsort(fich, (size_t)n, 96, cmp_cad);
	nrec = 0;
	static const struct { const char *titulo; const char *ext[3]; } CAT[] = {
		{ N_("Escenas"), { ".esc", NULL } }, { N_("Guiones"), { ".lua", NULL } }, { N_("Texturas"), { ".png", ".bmp", ".jpg" } }, { N_("Sonidos"), { ".wav", NULL } } };
	for (int c = 0; c < 4; ++c) {
		rec_agregar(_(CAT[c].titulo), 0, "");
		for (int i = 0; i < n; ++i) for (int q = 0; q < 3 && CAT[c].ext[q]; ++q) if (termina_en(fich[i], CAT[c].ext[q])) { rec_agregar(fich[i], 1, fich[i]); break; }
	}
	for (int i = 0; i < nrec; ++i) rec_txt[i] = rec_nom[i];
	rec.textos = rec_txt; rec.nivel = rec_niv; rec.aberto = rec_abr; rec.n = nrec;
	if (rec.sel >= nrec) rec.sel = -1;
}

/* ---------------------------------------------------------------------------------------------------- paleta de casillas */
static Nodo *mapa_sel(void) { Nodo *n = sel_nodo(); return n && n->tipo == T_MAPA ? n : NULL; }

static void liberar_paleta(void) { if (pal_valida) { imagem_liberar(&img_pal); pal_valida = false; } }

/* construye la imagen de la paleta con la textura (RGBA sobre un damero gris) dividida en casillas de cw x ch */
static void paleta_de_textura(const char *ruta, int cw, int ch) {
	if (pal_valida && !strcmp(pal_ruta, ruta) && pal.cel_l == cw && pal.cel_a == ch) return;
	liberar_paleta();
	Textura *t = ruta[0] ? rec_textura(ruta) : NULL;
	if (!t || !t->px || cw < 1 || ch < 1) { pal.img = NULL; return; }
	if (!imagem_criar(&img_pal, t->w, t->h, 0)) return;
	for (int y = 0; y < t->h; ++y) for (int x = 0; x < t->w; ++x) {
		uint32_t p = t->px[(size_t)y * (size_t)t->w + (size_t)x], a = p >> 24;
		uint32_t fondo = (((x / 4) + (y / 4)) & 1) ? 0x707070 : 0x909090;
		unsigned r = (((p >> 16) & 255) * a + ((fondo >> 16) & 255) * (255 - a)) / 255, g = (((p >> 8) & 255) * a + ((fondo >> 8) & 255) * (255 - a)) / 255, b = ((p & 255) * a + (fondo & 255) * (255 - a)) / 255;
		img_pal.px[(size_t)y * (size_t)t->w + (size_t)x] = (r << 16) | (g << 8) | b;
	}
	pal_valida = true; snprintf(pal_ruta, sizeof pal_ruta, "%s", ruta);
	int x = pal.x, y = pal.y, l = pal.l, a = pal.a;
	edpaleta_iniciar(&pal, x, y, l, a, &img_pal, cw, ch);
	pal.escala = cw <= 8 ? 3 : cw <= 20 ? 2 : 1;
}

static void actualizar_paleta(void) {
	Nodo *m = mapa_sel();
	if (m) { DMapa *d = m->d; paleta_de_textura(d->ruta, d->cw, d->ch); }
	else if (!pal_valida) pal.img = NULL;
}

/* ---------------------------------------------------------------------------------------------------- inspector */
enum { RK_NADA, RK_SEC, RK_NOMBRE, RK_TIPO, RK_PROP, RK_VAR, RK_SENAL };
typedef struct { int clase; PropInfo pi; int comp, primera; char clave[48]; int con_sec; } FilaMeta;
static FilaMeta meta[MAXFILAS];
static int nfilas;
static const char *opc_pool[MAXFILAS][8];
static char opc_buf[MAXFILAS][96];

static int fila_nueva(int clase) {
	if (nfilas >= MAXFILAS) return -1;
	memset(&props[nfilas], 0, sizeof props[0]); memset(&meta[nfilas], 0, sizeof meta[0]);
	meta[nfilas].clase = clase; meta[nfilas].primera = nfilas;
	return nfilas++;
}
static void seccion_fila(const char *titulo) { int i = fila_nueva(RK_SEC); if (i >= 0) inspetor_secao(&props[i], titulo, true); }

static void comp_nombre(const PropInfo *pi, int k, char *out, size_t no) {
	char et[64] = ""; int q = 0;
	if (pi->etiquetas) { const char *s = pi->etiquetas; while (q < k && (s = strchr(s, '|'))) { ++s; ++q; } if (s) { const char *e = strchr(s, '|'); snprintf(et, sizeof et, "%.*s", e ? (int)(e - s) : (int)strlen(s), s); } }
	if (!et[0]) snprintf(et, sizeof et, "%d", k + 1);
	snprintf(out, no, "%s %s", pi->nombre, et);
}

static uint32_t color_de_valor(const Valor *v) {
	uint8_t c[4] = { 255, 255, 255, 255 };
	if (v->n >= 1 && v->t[0].str) {
		const char *s = v->t[0].s; if (*s == '#') ++s;
		size_t l = strlen(s);
		if (l == 6 || l == 8) for (size_t i = 0; i < l / 2; ++i) { char h[3] = { s[2 * i], s[2 * i + 1], 0 }; c[i] = (uint8_t)strtol(h, NULL, 16); }
	} else if (v->n >= 3) for (int i = 0; i < v->n && i < 4; ++i) c[i] = (uint8_t)(v->t[i].num < 0 ? 0 : v->t[i].num > 255 ? 255 : v->t[i].num);
	return ((uint32_t)c[3] << 24) | ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | c[2];
}

static int tok_i(const Valor *v, int i) { return i < v->n ? (int)floor(v->t[i].num + (v->t[i].num < 0 ? -0.5 : 0.5)) : 0; }
static double tok_f(const Valor *v, int i) { return i < v->n ? v->t[i].num : 0; }

static void quitar_comillas(const char *raw, char *out, size_t no) {
	snprintf(out, no, "%s", raw);
	size_t l = strlen(out);
	if (l >= 2 && out[0] == '"' && out[l - 1] == '"') { memmove(out, out + 1, l - 2); out[l - 2] = 0; }
	/* \" y \\ del fichero */
	char *w = out;
	for (char *r = out; *r; ++r) { if (*r == '\\' && r[1]) { ++r; *w++ = *r == 'n' ? '\n' : *r; } else *w++ = *r; }
	*w = 0;
}

static void fila_prop(const PropInfo *pi, int sec, Nodo *nv) {
	char raw[512] = "";
	if (!esc_prop_valor(doc, sec, pi->nombre, raw, sizeof raw)) { if (!nv || !prop_texto(nv, pi->nombre, raw, sizeof raw)) raw[0] = 0; }
	Valor v; texto_valor(raw, &v);
	int primera = nfilas;
	int n = pi->tipo == PK_V2 || pi->tipo == PK_V2I ? 2 : pi->tipo == PK_V4 || pi->tipo == PK_V4I ? 4 : 1;
	for (int k = 0; k < n; ++k) {
		int i = fila_nueva(RK_PROP); if (i < 0) return;
		meta[i].pi = *pi; meta[i].comp = k; meta[i].primera = primera;
		ins_prop_t *p = &props[i]; char nom[40];
		if (n > 1) comp_nombre(pi, k, nom, sizeof nom); else snprintf(nom, sizeof nom, "%s", pi->nombre);
		int paso = (int)(pi->paso * 100 + 0.5f); if (paso < 1) paso = 1;
		switch (pi->tipo) {
		case PK_FLOAT: inspetor_fixo(p, nom, (int)floor(tok_f(&v, 0) * 100 + 0.5), 2, (int)(pi->min * 100), (int)(pi->max * 100), paso); break;
		case PK_V2: case PK_V4: inspetor_fixo(p, nom, (int)floor(tok_f(&v, k) * 100 + 0.5), 2, (int)(pi->min * 100), (int)(pi->max * 100), paso); break;
		case PK_INT: inspetor_int(p, nom, tok_i(&v, 0), (int)pi->min, (int)pi->max, 1); break;
		case PK_V2I: case PK_V4I: inspetor_int(p, nom, tok_i(&v, k), (int)pi->min, (int)pi->max, 1); break;
		case PK_BOOL: inspetor_bool(p, nom, v.n && v.t[0].num != 0); break;
		case PK_COLOR: inspetor_cor(p, nom, color_de_valor(&v), true); break;
		case PK_ENUM: {
			snprintf(opc_buf[i], sizeof opc_buf[i], "%s", pi->opciones);
			int no = 0, sel_i = 0; char *s = opc_buf[i];
			while (s && no < 8) { char *e = strchr(s, '|'); if (e) *e = 0; opc_pool[i][no++] = s; s = e ? e + 1 : NULL; }
			for (int q = 0; q < no; ++q) if (v.n && !strcmp(v.t[0].s, opc_pool[i][q])) sel_i = q;
			inspetor_enum(p, nom, sel_i, opc_pool[i], no);
			break;
		}
		case PK_TEXT: {
			char t[512], l1[200]; quitar_comillas(raw, t, sizeof t); u8_l1(l1, t, sizeof l1);
			if (pi->ruta) inspetor_arquivo(p, nom, l1, pi->ruta == 1 ? "Imágenes (*.png)|*.png|Todos|*" : pi->ruta == 2 ? "Sonidos (*.wav)|*.wav|Todos|*" : "Lua (*.lua)|*.lua|Todos|*");
			else inspetor_texto(p, nom, l1);
			p->desab = strlen(l1) > 90;
			break;
		}
		default: {   /* PK_LISTA: el texto tal como está en el fichero */
			char l1[200]; u8_l1(l1, raw, sizeof l1);
			inspetor_texto(p, nom, l1);
			p->desab = strlen(l1) > 90;
		}
		}
	}
}

static void cargar_inspetor(void) {
	sincronizando = true;
	nfilas = 0;
	Nodo *nv = sel_nodo(); int sec = sel_sec();
	if (nv && sec >= 0 && nsel >= 1) {
		const EscSec *s = &doc->sec[sec];
		seccion_fila(_("Nodo"));
		int i = fila_nueva(RK_NOMBRE); char nm[CAPI_NOME_MAX]; u8_l1(nm, s->nombre, sizeof nm);
		if (i >= 0) { inspetor_texto(&props[i], N_("Nombre"), nm); props[i].desab = !strcmp(s->ruta, ".") && 0; }
		i = fila_nueva(RK_TIPO);
		if (i >= 0) { inspetor_texto(&props[i], N_("Tipo"), s->clase == ES_INSTANCIA ? "Instancia" : s->tipo); props[i].desab = true; }
		PropInfo pi;
		static const char *const TIT[] = { N_("Nodo"), N_("Transformación"), N_("Apariencia"), N_("Propiedades del tipo") };
		for (int g = 0; g < 4; ++g) {
			int hay = 0;
			for (int k = 0; prop_listar(nv->tipo, k, &pi); ++k) if (!pi.oculta && pi.grupo == g) hay = 1;
			if (!hay) continue;
			if (g > 0) seccion_fila(_(TIT[g] ));
			for (int k = 0; prop_listar(nv->tipo, k, &pi); ++k) if (!pi.oculta && pi.grupo == g) fila_prop(&pi, sec, nv);
		}
		/* variables propias del nodo (var.*) */
		int hay_var = 0;
		for (int l = s->ini + 1; l < s->fin; ++l) {
			const char *t = doc->lin[l]; while (*t == ' ' || *t == '\t') ++t;
			if (strncmp(t, "var.", 4)) continue;
			const char *eq = strchr(t, '='); if (!eq) continue;
			char clave[48]; size_t kl = (size_t)(eq - t); while (kl && t[kl - 1] == ' ') --kl; snprintf(clave, sizeof clave, "%.*s", (int)kl, t);
			if (!hay_var) { seccion_fila(_("Variables")); hay_var = 1; }
			char raw[300], l1[200]; esc_prop_valor(doc, sec, clave, raw, sizeof raw); u8_l1(l1, raw, sizeof l1);
			int f = fila_nueva(RK_VAR); if (f < 0) break;
			snprintf(meta[f].clave, sizeof meta[f].clave, "%s", clave);
			inspetor_texto(&props[f], clave, l1); props[f].desab = strlen(l1) > 90;
		}
		/* señales conectadas desde este nodo */
		int hay_sen = 0;
		for (int c = 0; c < doc->nsec; ++c) {
			if (doc->sec[c].clase != ES_CONEXION) continue;
			char desde[CAPI_RUTA_MAX], sen[40], hacia[CAPI_RUTA_MAX], met[40];
			if (!esc_attr_valor(doc, c, "desde", desde, sizeof desde) || strcmp(desde, s->ruta)) continue;
			esc_attr_valor(doc, c, "senal", sen, sizeof sen); esc_attr_valor(doc, c, "hacia", hacia, sizeof hacia); esc_attr_valor(doc, c, "metodo", met, sizeof met);
			if (!hay_sen) { seccion_fila(_("Señales")); hay_sen = 1; }
			int f = fila_nueva(RK_SENAL); if (f < 0) break;
			char t[200], l1[200]; snprintf(t, sizeof t, "%s:%s", hacia, met); u8_l1(l1, t, sizeof l1);
			meta[f].con_sec = c;
			inspetor_texto(&props[f], sen, l1);
		}
	} else if (nsel > 1) {
		seccion_fila(_("Selección"));
		int i = fila_nueva(RK_TIPO); char t[40]; snprintf(t, sizeof t, "%d nodos", nsel);
		if (i >= 0) { inspetor_texto(&props[i], N_("Elegidos"), t); props[i].desab = true; }
	}
	ins.p = props; ins.n = nfilas; ins.sel = -1; ins.editando = -1; ins.arrasto = -1; ins.menu_prop = -1; ins.ultimo = -1;
	if (ins.topo * INS_FILA >= nfilas * INS_FILA) ins.topo = 0;
	sincronizando = false;
	for (int i = 0; i < nfilas; ++i) registrar("capibara-editor: fila %d %s\n", i, props[i].nome);
}

/* El texto del fichero de una fila (o grupo de filas) del inspector */
static void texto_de_prop(int i, char *out, size_t no) {
	const FilaMeta *m = &meta[i]; const PropInfo *pi = &m->pi; int p0 = m->primera; char a[64];
	out[0] = 0;
	switch (pi->tipo) {
	case PK_FLOAT: fmt_n(out, no, props[p0].valor / 100.0); break;
	case PK_INT: snprintf(out, no, "%d", props[p0].valor); break;
	case PK_BOOL: snprintf(out, no, "%s", props[p0].valor ? "si" : "no"); break;
	case PK_V2: case PK_V4: for (int k = 0; k < pi->n; ++k) { fmt_n(a, sizeof a, props[p0 + k].valor / 100.0); strncat(out, k ? " " : "", no - strlen(out) - 1); strncat(out, a, no - strlen(out) - 1); } break;
	case PK_V2I: case PK_V4I: for (int k = 0; k < pi->n; ++k) { snprintf(a, sizeof a, "%s%d", k ? " " : "", props[p0 + k].valor); strncat(out, a, no - strlen(out) - 1); } break;
	case PK_COLOR: {
		uint32_t c = (uint32_t)props[p0].valor;
		if ((c >> 24) == 255) snprintf(out, no, "\"#%02x%02x%02x\"", (c >> 16) & 255, (c >> 8) & 255, c & 255);
		else snprintf(out, no, "\"#%02x%02x%02x%02x\"", (c >> 16) & 255, (c >> 8) & 255, c & 255, c >> 24);
		break;
	}
	case PK_ENUM: snprintf(out, no, "%s", props[p0].opcoes[props[p0].valor]); break;
	case PK_TEXT: {
		char u[300]; l1_u8(u, props[p0].texto, sizeof u);
		size_t o = 0; out[o++] = '"';
		for (const char *s = u; *s && o + 3 < no; ++s) { if (*s == '"' || *s == '\\') out[o++] = '\\'; if (*s == '\n') { out[o++] = '\\'; out[o++] = 'n'; continue; } out[o++] = *s; }
		out[o++] = '"'; out[o] = 0;
		break;
	}
	default: l1_u8(out, props[p0].texto, no);
	}
}

static void aplicar_posicion_sin_xy(int sec) { esc_prop_quitar(doc, sec, "x"); esc_prop_quitar(doc, sec, "y"); }

static void ins_mudou(void *ctx, inspetor_t *s, int i) {
	(void)ctx; (void)s;
	if (sincronizando || i < 0 || i >= nfilas) return;
	FilaMeta *m = &meta[i]; int sec = sel_sec();
	if (sec < 0) return;
	char *antes = snap(), clave[200];
	if (m->clase == RK_NOMBRE) {
		char nuevo[CAPI_NOME_MAX]; l1_u8(nuevo, props[i].texto, sizeof nuevo);
		int ok = nuevo[0] && !strpbrk(nuevo, " \t[]=\"/");
		if (!ok) { ce_con(CON_AVISO, "%s", _("El nombre no puede estar vacío ni llevar espacios, corchetes, comillas o barras.")); free(antes); cargar_inspetor(); return; }
		int r = esc_nodo_renombrar(doc, sec, nuevo);
		if (r < 0) { ce_con(CON_AVISO, "%s", _("Ya hay un nodo con ese nombre.")); free(antes); cargar_inspetor(); return; }
		sel_unica(doc->sec[r].ruta);
		snprintf(clave, sizeof clave, "nombre:%s", doc->sec[r].ruta);
		commit(N_("Renombrar nodo"), antes, clave);
		registrar("capibara-editor: renombrado %s\n", doc->sec[r].ruta);
		sincronizar_todo();
		return;
	}
	if (m->clase == RK_VAR) {
		char t[300]; l1_u8(t, props[i].texto, sizeof t);
		esc_prop_poner(doc, sec, m->clave, t);
		snprintf(clave, sizeof clave, "%s:%s", sel[0], m->clave);
		commit(N_("Cambiar variable"), antes, clave);
		registrar("capibara-editor: var %s %s = %s\n", sel[0], m->clave, t);
		recargar_vivo(); sincronizar_ligero();
		return;
	}
	if (m->clase == RK_SENAL) {
		char t[300]; l1_u8(t, props[i].texto, sizeof t);
		if (!t[0]) { /* vacío: se quita la conexión */
			esc_conexion_borrar(doc, m->con_sec);
			commit(N_("Quitar conexión"), antes, "");
			sincronizar_todo();
			return;
		}
		char *dp = strrchr(t, ':');
		if (dp) { *dp = 0; esc_attr_poner(doc, m->con_sec, "hacia", t); esc_attr_poner(doc, m->con_sec, "metodo", dp + 1); }
		else esc_attr_poner(doc, m->con_sec, "metodo", t);
		commit(N_("Cambiar conexión"), antes, "");
		recargar_vivo(); sincronizar_ligero();
		return;
	}
	if (m->clase != RK_PROP) { free(antes); return; }
	char t[600]; texto_de_prop(i, t, sizeof t);
	if (!strcmp(m->pi.nombre, "posicion") || !strcmp(m->pi.nombre, "escala")) { if (!strcmp(m->pi.nombre, "posicion")) aplicar_posicion_sin_xy(sec); }
	if (m->pi.tipo == PK_LISTA && !t[0]) esc_prop_quitar(doc, sec, m->pi.nombre); else esc_prop_poner(doc, sec, m->pi.nombre, t);
	snprintf(clave, sizeof clave, "%s:%s", sel[0], m->pi.nombre);
	commit(N_("Cambiar propiedad"), antes, clave);
	registrar("capibara-editor: prop %s %s = %s\n", sel[0], m->pi.nombre, t);
	recargar_vivo();
	sincronizar_ligero();
}

static void sincronizar_ligero(void) { sel_a_vista(); actualizar_paleta(); }
static void sincronizar_todo(void) { reconstruir_arbol(); cargar_inspetor(); sel_a_vista(); actualizar_paleta(); }

/* ---------------------------------------------------------------------------------------------------- código (guiones Lua) */
static bool cod_conv;       /* el texto del editor está en Latin-1 y el fichero en UTF-8 */
static void cod_recordar(void) {   /* el texto actual pasa a ser el «guardado» (el componente no sabe volver a «sin modificar» sin recargar) */
	int n; const char *t = codigo_texto(cod, &n);
	free(cod_guardado); cod_guardado = malloc((size_t)n + 1); memcpy(cod_guardado, t, (size_t)n); cod_guardado[n] = 0; cod_guardado_n = n;
}
static bool cod_cambiado(void) {
	if (!cod_arch[0]) return false;
	int n; const char *t = codigo_texto(cod, &n);
	return !cod_guardado || n != cod_guardado_n || memcmp(t, cod_guardado, (size_t)n) != 0;
}
static bool texto_cabe_l1(const char *t) {
	for (const unsigned char *s = (const unsigned char *)t; *s; ++s) if (*s >= 0xF0 || (*s >= 0xE0)) return false; else if (*s >= 0xC0) { if (*s > 0xC3) return false; ++s; if (!*s) break; }
	return true;
}

static void mostrar_codigo(bool si) {
	for (int i = 0; i < pn.n; ++i) if (pn.no[i].tipo == PD_FOLHA && pn.no[i].id == H_CODIGO) {
		int hermano = -1;
		for (int k = 0; k < pn.n; ++k) if (pn.no[k].tipo == PD_PILHA && (pn.no[k].a == i || pn.no[k].b == i)) hermano = k;
		if (hermano >= 0) paineis_plegar(&pn, hermano, !si);
	}
}

static void guardar_guion(void);

static int abrir_guion(const char *rel) {
	if (cod_arch[0] && strcmp(cod_arch, rel) && cod_cambiado()) guardar_guion();   /* no se pierde lo escrito en el otro guion */
	char r[RUTA * 2]; ruta_proy(r, sizeof r, rel);
	size_t n; char *t = leer_todo(r, &n);
	if (!t) { ce_con(CON_ERRO, "%s: %s", _("No existe el guion"), rel); return -1; }
	cod_conv = texto_cabe_l1(t);
	if (cod_conv) { char *l = malloc(n + 1); u8_l1(l, t, n + 1); codigo_definir(cod, l, -1); free(l); } else codigo_definir(cod, t, (int)n);
	free(t);
	snprintf(cod_arch, sizeof cod_arch, "%s", rel);
	cod_recordar();
	mostrar_codigo(true);
	layout_y_log();
	registrar("capibara-editor: guion abierto %s\n", rel);
	return 0;
}

static bool comprobar_lua(const char *rel, const char *txt, size_t n, bool marcar) {
	if (!g_L) return true;
	char nom[160]; snprintf(nom, sizeof nom, "@%s", rel);
	if (luaL_loadbuffer(g_L, txt, n, nom) == 0) { lua_pop(g_L, 1); if (marcar) codigo_marcar_erro(cod, 0, ""); return true; }
	const char *m = lua_tostring(g_L, -1); char msg[300]; snprintf(msg, sizeof msg, "%s", m ? m : "?"); lua_pop(g_L, 1);
	int linea = 0; const char *p = strstr(msg, ".lua:");
	if (p) linea = atoi(p + 5);
	char l1[300]; u8_l1(l1, msg, sizeof l1);
	if (marcar && linea > 0) { const char *mp = strstr(l1, ".lua:"); mp = mp ? strchr(mp + 5, ':') : NULL; mp = mp ? mp + 1 : l1; while (*mp == ' ') ++mp; codigo_marcar_erro(cod, linea, mp); }
	ce_con_linea(CON_ERRO, linea, l1);
	registrar("capibara-editor: error de Lua %s linea %d\n", rel, linea);
	return false;
}

static void guardar_guion(void) {
	if (!cod_arch[0] || !cod_cambiado()) return;
	int n; const char *t = codigo_texto(cod, &n);
	char *u = malloc((size_t)n * 2 + 2);
	if (cod_conv) l1_u8(u, t, (size_t)n * 2 + 2); else { memcpy(u, t, (size_t)n); u[n] = 0; }
	char r[RUTA * 2]; ruta_proy(r, sizeof r, cod_arch);
	if (escribir_todo(r, u, strlen(u)) == 0) {
		ce_con(CON_MSG, "%s: %s", _("Guion guardado"), cod_arch);
		registrar("capibara-editor: guion guardado %s\n", cod_arch);
		comprobar_lua(cod_arch, u, strlen(u), true);
		cod_recordar();
	} else ce_con(CON_ERRO, "%s: %s", _("No se pudo guardar"), cod_arch);
	free(u);
}

static void esc_guardar_ahora(void) {
	if (!doc || !esc_rel[0]) return;
	char r[RUTA * 2]; ruta_proy(r, sizeof r, esc_rel);
	if (esc_guardar(doc, r) == 0) { esc_mod = false; ce_con(CON_MSG, "%s: %s", _("Escena guardada"), esc_rel); registrar("capibara-editor: escena guardada %s\n", esc_rel); }
	else ce_con(CON_ERRO, "%s: %s", _("No se pudo guardar"), esc_rel);
}
static void guardar_todo(void) { esc_guardar_ahora(); guardar_guion(); }

/* ---------------------------------------------------------------------------------------------------- abrir escenas y proyectos */
static void encuadrar_vista(void) {
	float fx, fy, fw, fh; marco_juego(&fx, &fy, &fw, &fh);
	if (vs.l <= 0 || vs.a <= 0) return;
	int zx = (int)((float)vs.l * 1000.0f / (fw + 24)), zy = (int)((float)vs.a * 1000.0f / (fh + 24));
	int z = zx < zy ? zx : zy;
	if (z > 4000) z = 4000;
	if (z < EDVISTA_ZOOM_MIN) z = EDVISTA_ZOOM_MIN;
	vs.zoom = z;
	vs.ox = vs.l / 2 - (int)((fx + fw * 0.5f) * (float)z / 1000.0f); vs.oy = vs.a / 2 - (int)((fy + fh * 0.5f) * (float)z / 1000.0f);
}

static int abrir_escena(const char *rel) {
	char r[RUTA * 2]; ruta_proy(r, sizeof r, rel);
	size_t n; char *t = leer_todo(r, &n);
	if (!t) { ce_con(CON_ERRO, "%s: %s", _("No existe la escena"), rel); return -1; }
	if (doc) esc_cerrar(doc);
	doc = esc_abrir_texto(t); free(t);
	snprintf(esc_rel, sizeof esc_rel, "%s", rel);
	esc_mod = false; ed_desfazer_limpar(&pila); ultimo_cmd = NULL;
	sel_limpiar();
	avisar_carga = true; recargar_vivo(); avisar_carga = false;
	encuadrar_vista();
	sincronizar_todo();
	ce_con(CON_MSG, "%s: %s (%d %s)", _("Escena abierta"), rel, live ? nodo_contar(live) : 0, _("nodos"));
	registrar("capibara-editor: escena %s nodos %d\n", rel, live ? nodo_contar(live) : 0);
	return 0;
}

static int abrir_proyecto(const char *dir) {
	char cfg[RUTA * 2]; snprintf(cfg, sizeof cfg, "%s/capibara.cfg", dir);
	if (termina_en(dir, ".capi")) { mensagem(_("Capibara"), _("Los paquetes .capi no se editan: abre la carpeta del proyecto.")); return -1; }
	if (!existe_fich(cfg)) { mensagem(_("Capibara"), _("Esa carpeta no es un proyecto de Capibara (falta capibara.cfg).")); return -1; }
	if (live) { nodo_liberar_ya(live); live = NULL; nlista = 0; }
	liberar_paleta(); pal.img = NULL;
	rec_liberar_proyecto();
	snprintf(proy_dir, sizeof proy_dir, "%s", dir);
	size_t l = strlen(proy_dir); while (l > 1 && proy_dir[l - 1] == '/') proy_dir[--l] = 0;
	Proyecto defecto = { "Capibara", "", 320, 180, 1, { 32, 32, 48, 255 }, 900, 60, 1234, 0 };
	g_proy = defecto;
	char err[200] = "";
	cap_abrir_proyecto(proy_dir, err, sizeof err);
	if (proyecto_cargar(err, sizeof err) != 0) ce_con(CON_AVISO, "capibara.cfg: %s", err);
	cod_arch[0] = 0; codigo_definir(cod, "", 0); free(cod_guardado); cod_guardado = NULL; mostrar_codigo(false);
	reconstruir_recursos();
	esc_rel[0] = 0;
	if (doc) { esc_cerrar(doc); doc = NULL; }
	const char *prin = g_proy.principal[0] ? g_proy.principal : "escenas/principal.esc";
	if (abrir_escena(prin) != 0) { doc = esc_abrir_texto(""); sincronizar_todo(); }
	ce_con(CON_MSG, "%s: %s (%s)", _("Proyecto"), g_proy.nombre, proy_dir);
	registrar("capibara-editor: proyecto %s principal %s pantalla %dx%d\n", proy_dir, prin, g_proy.ancho, g_proy.alto);
	return 0;
}

static void abrir_ejemplo(const char *nombre) {
	const char *home = getenv("HOME"); if (!home || !*home) home = "/home";
	char o[RUTA], d[RUTA];
	snprintf(o, sizeof o, "/apps/capibara/%s", nombre); snprintf(d, sizeof d, "%s/capibara/%s", home, nombre);
	if (!existe_fich("/apps/capibara") && !existe_dir(o)) { ce_con(CON_ERRO, "%s: %s", _("No hay ejemplo"), o); return; }
	if (!existe_dir(d)) { int n = copiar_dir(o, d); ce_con(CON_MSG, "%s %s (%d %s)", _("Ejemplo copiado a"), d, n, _("ficheros")); registrar("capibara-editor: ejemplo copiado %s %d\n", d, n); }
	abrir_proyecto(d);
}

static void proyecto_nuevo(void) {
	const char *home = getenv("HOME"); if (!home || !*home) home = "/home";
	char dir[RUTA]; snprintf(dir, sizeof dir, "%s/capibara/proyecto", home);
	for (int k = 2; existe_dir(dir) && k < 100; ++k) snprintf(dir, sizeof dir, "%s/capibara/proyecto%d", home, k);
	if (!pedir_texto(_("Proyecto nuevo"), _("Carpeta del proyecto:"), dir, sizeof dir) || !dir[0]) return;
	if (existe_fich(dir) || (existe_dir(dir) && existe_fich(proy_dir))) { /* nada */ }
	char cfg[RUTA * 2]; snprintf(cfg, sizeof cfg, "%s/capibara.cfg", dir);
	if (existe_fich(cfg)) { mensagem(_("Capibara"), _("Ya hay un proyecto en esa carpeta.")); return; }
	const char *nombre = strrchr(dir, '/'); nombre = nombre ? nombre + 1 : dir;
	char t[1500];
	snprintf(t, sizeof t,
		"[proyecto]\nnombre = \"%s\"\nescena_principal = \"escenas/principal.esc\"\nsemilla = 1\n\n[pantalla]\nancho = 320\nalto = 180\nescala_entera = no\nfondo = \"#2a3f5f\"\n\n"
		"[fisica]\ngravedad = 900\npaso = 60\n\n[acciones]\nsalir = tecla:Escape\n", nombre);
	escribir_todo(cfg, t, strlen(t));
	char p[RUTA * 2];
	snprintf(p, sizeof p, "%s/escenas/principal.esc", dir);
	const char *esc = "# Escena principal del proyecto\n[Nodo2D Principal]\nguion = \"guiones/principal.lua\"\n\n[Etiqueta Saludo]\ntexto = \"\xc2\xa1" "Hola, Capibara!\"\nposicion = 60 80\nescala_texto = 2\n";
	escribir_todo(p, esc, strlen(esc));
	snprintf(p, sizeof p, "%s/guiones/principal.lua", dir);
	const char *lua = "-- Guion de la escena principal\nlocal Principal = {}\n\nfunction Principal:_listo()\n  print(\"Hola desde Capibara\")\nend\n\nreturn Principal\n";
	escribir_todo(p, lua, strlen(lua));
	registrar("capibara-editor: proyecto nuevo %s\n", dir);
	abrir_proyecto(dir);
}

static void escena_nueva(void) {
	if (!proy_dir[0]) return;
	char rel[128] = "escenas/nueva.esc";
	for (int k = 2;; ++k) { char r[RUTA * 2]; ruta_proy(r, sizeof r, rel); if (!existe_fich(r) || k > 99) break; snprintf(rel, sizeof rel, "escenas/nueva%d.esc", k); }
	if (!pedir_texto(_("Escena nueva"), _("Fichero de la escena:"), rel, sizeof rel) || !rel[0]) return;
	if (!termina_en(rel, ".esc")) strncat(rel, ".esc", sizeof rel - strlen(rel) - 1);
	char r[RUTA * 2]; ruta_proy(r, sizeof r, rel);
	if (existe_fich(r)) { mensagem(_("Capibara"), _("Esa escena ya existe.")); return; }
	const char *t = "[Nodo2D Raiz]\n";
	escribir_todo(r, t, strlen(t));
	reconstruir_recursos();
	abrir_escena(rel);
}

/* ---------------------------------------------------------------------------------------------------- órdenes sobre los nodos */
static void tras_estructura(void) { recargar_vivo(); sincronizar_todo(); }

static void centro_vista(int *wx, int *wy) { edvista_para_mundo(&vs, vs.x + vs.l / 2, vs.y + vs.a / 2, wx, wy); }

static int snap_rejilla(int v) { return iman_on && vs.grade > 0 ? (v + (v < 0 ? -vs.grade / 2 : vs.grade / 2)) / vs.grade * vs.grade : v; }

static void anadir_nodo(int tipo) {
	if (!doc) return;
	char *antes = snap();
	const char *padre = nsel ? sel[0] : ".";
	if (esc_buscar_ruta(doc, ".") < 0) padre = ".";
	int primero = esc_buscar_ruta(doc, ".") < 0;
	int r = esc_nodo_nuevo(doc, CAPI_TIPOS[tipo], CAPI_TIPOS[tipo], padre, -1);
	if (r < 0) { free(antes); return; }
	if (tipo != T_NODO && tipo != T_TEMP && tipo != T_AUDIO) {
		int wx, wy; centro_vista(&wx, &wy);
		Nodo *pn_ = primero ? NULL : nodo_vivo(padre);
		float gx = 0, gy = 0; if (pn_) nodo_pos_global(pn_, &gx, &gy);
		char t[80]; snprintf(t, sizeof t, "%d %d", snap_rejilla(wx - (int)gx), snap_rejilla(wy - (int)gy));
		esc_prop_poner(doc, r, "posicion", t);
	}
	if (tipo == T_ETIQUETA) esc_prop_poner(doc, r, "texto", "\"Texto\"");
	if (tipo == T_BOTON) esc_prop_poner(doc, r, "texto", "\"Bot\xc3\xb3" "n\"");
	if (tipo == T_AREA || tipo == T_CUERPO) esc_prop_poner(doc, r, "forma", "caja 16 16");
	char ruta[CAPI_RUTA_MAX]; snprintf(ruta, sizeof ruta, "%s", doc->sec[r].ruta);
	commit(N_("Añadir nodo"), antes, "");
	sel_unica(ruta);
	tras_estructura();
	registrar("capibara-editor: nodo anadido %s tipo %s\n", ruta, CAPI_TIPOS[tipo]);
	ce_con(CON_MSG, "%s: %s", _("Nodo añadido"), ruta);
}

static void borrar_seleccion(void) {
	if (!doc || !nsel) return;
	char *antes = snap(); int n = 0;
	char copia[MAXSEL][CAPI_RUTA_MAX]; int nc = nsel; memcpy(copia, sel, sizeof copia);
	for (int i = 0; i < nc; ++i) { int s = esc_buscar_ruta(doc, copia[i]); if (s >= 0 && esc_nodo_borrar(doc, s)) ++n; }
	if (!n) { free(antes); ce_con(CON_AVISO, "%s", _("La raíz de la escena no se puede borrar.")); return; }
	commit(N_("Borrar nodos"), antes, "");
	sel_limpiar();
	tras_estructura();
	registrar("capibara-editor: borrados %d\n", n);
}

static void duplicar_seleccion(void) {
	if (!doc || !nsel) return;
	int s = sel_sec(); if (s < 0) return;
	char *antes = snap();
	int r = esc_nodo_duplicar(doc, s);
	if (r < 0) { free(antes); return; }
	char ruta[CAPI_RUTA_MAX]; snprintf(ruta, sizeof ruta, "%s", doc->sec[r].ruta);
	commit(N_("Duplicar nodo"), antes, "");
	sel_unica(ruta); tras_estructura();
	registrar("capibara-editor: duplicado %s\n", ruta);
}

static void renombrar_seleccion(void) {
	if (!doc || !nsel) return;
	int s = sel_sec(); if (s < 0) return;
	char nm[CAPI_NOME_MAX], l1[CAPI_NOME_MAX]; u8_l1(l1, doc->sec[s].nombre, sizeof l1);
	snprintf(nm, sizeof nm, "%s", l1);
	if (!pedir_texto(_("Renombrar"), _("Nombre nuevo:"), nm, sizeof nm) || !nm[0]) return;
	char u[CAPI_NOME_MAX]; l1_u8(u, nm, sizeof u);
	if (strpbrk(u, " \t[]=\"/")) { ce_con(CON_AVISO, "%s", _("El nombre no puede llevar espacios, corchetes, comillas o barras.")); return; }
	char *antes = snap();
	int r = esc_nodo_renombrar(doc, s, u);
	if (r < 0) { free(antes); ce_con(CON_AVISO, "%s", _("Ya hay un nodo con ese nombre.")); return; }
	sel_unica(doc->sec[r].ruta);
	commit(N_("Renombrar nodo"), antes, "");
	tras_estructura();
}

static void anadir_variable(void) {
	int s = sel_sec(); if (s < 0) return;
	char nom[48] = "vidas", val[96] = "3";
	if (!pedir_texto(_("Variable nueva"), _("Nombre (se usará como self.nombre en Lua):"), nom, sizeof nom) || !nom[0]) return;
	if (!pedir_texto(_("Variable nueva"), _("Valor (número, \"texto\", si o no):"), val, sizeof val)) return;
	char clave[64], u[200]; snprintf(clave, sizeof clave, "var.%s", nom); l1_u8(u, val, sizeof u);
	char *antes = snap();
	esc_prop_poner(doc, s, clave, u);
	commit(N_("Añadir variable"), antes, "");
	recargar_vivo(); cargar_inspetor();
	registrar("capibara-editor: variable anadida %s %s = %s\n", sel[0], clave, u);
}

static void conectar_senal(void) {
	int s = sel_sec(); if (s < 0) return;
	char sen[48] = "presionado", hacia[96] = ".", met[48] = "_pulsado";
	if (!pedir_texto(_("Conectar señal"), _("Señal de este nodo:"), sen, sizeof sen) || !sen[0]) return;
	if (!pedir_texto(_("Conectar señal"), _("Nodo destino (ruta desde la raíz, «.» = la raíz):"), hacia, sizeof hacia) || !hacia[0]) return;
	if (!pedir_texto(_("Conectar señal"), _("Método del guion del destino:"), met, sizeof met) || !met[0]) return;
	char *antes = snap();
	esc_conexion_nueva(doc, doc->sec[s].ruta, sen, hacia, met);
	commit(N_("Conectar señal"), antes, "");
	recargar_vivo(); cargar_inspetor();
	registrar("capibara-editor: conexion %s.%s -> %s.%s\n", sel[0], sen, hacia, met);
}

static void instancia_nueva(void) {
	if (!doc) return;
	char esc[128] = "";
	if (rec.sel >= 0 && rec_niv[rec.sel] == 1 && termina_en(rec_ruta[rec.sel], ".esc")) snprintf(esc, sizeof esc, "%s", rec_ruta[rec.sel]);
	if (!pedir_texto(_("Instanciar escena"), _("Escena (ruta dentro del proyecto):"), esc, sizeof esc) || !esc[0]) return;
	char r[RUTA * 2]; ruta_proy(r, sizeof r, esc);
	if (!existe_fich(r)) { ce_con(CON_ERRO, "%s: %s", _("No existe la escena"), esc); return; }
	if (!strcmp(esc, esc_rel)) { ce_con(CON_AVISO, "%s", _("Una escena no se puede instanciar a sí misma.")); return; }
	char *antes = snap();
	const char *padre = nsel ? sel[0] : ".";
	int s = esc_instancia_nueva(doc, esc, "Instancia", padre, -1);
	if (s < 0) { free(antes); return; }
	int wx, wy; centro_vista(&wx, &wy); char t[64]; snprintf(t, sizeof t, "%d %d", snap_rejilla(wx), snap_rejilla(wy));
	esc_prop_poner(doc, s, "posicion", t);
	char ruta[CAPI_RUTA_MAX]; snprintf(ruta, sizeof ruta, "%s", doc->sec[s].ruta);
	commit(N_("Instanciar escena"), antes, "");
	sel_unica(ruta); tras_estructura();
	registrar("capibara-editor: instancia %s de %s\n", ruta, esc);
}

static void crear_guion(void) {
	int s = sel_sec(); if (s < 0 || !proy_dir[0]) return;
	char nom[CAPI_NOME_MAX], clase[CAPI_NOME_MAX], rel[160];
	snprintf(nom, sizeof nom, "%s", doc->sec[s].nombre);
	/* nombre de fichero en minúsculas y de clase en CamelCase, solo con letras y cifras */
	size_t o = 0;
	for (const char *c = nom; *c && o + 1 < sizeof clase; ++c) if (isalnum((unsigned char)*c)) { clase[o] = (char)(o == 0 ? toupper((unsigned char)*c) : *c); ++o; }
	clase[o] = 0;
	if (!clase[0]) snprintf(clase, sizeof clase, "Guion");
	char f[CAPI_NOME_MAX]; for (size_t i = 0; clase[i]; ++i) f[i] = (char)tolower((unsigned char)clase[i]); f[strlen(clase)] = 0;
	snprintf(rel, sizeof rel, "guiones/%s.lua", f);
	for (int k = 2;; ++k) { char r[RUTA * 2]; ruta_proy(r, sizeof r, rel); if (!existe_fich(r) || k > 99) break; snprintf(rel, sizeof rel, "guiones/%s%d.lua", f, k); }
	char t[900];
	snprintf(t, sizeof t,
		"-- Guion del nodo %s\n"
		"-- Los m\xc3\xa9" "todos que quieras: _listo(), _proceso(dt), _fisica(dt) y _entrada(ev). Dentro de ellos, self es el nodo.\n"
		"local %s = {}\n\n"
		"function %s:_listo()\n  -- el nodo y sus hijos ya est\xc3\xa1" "n en el \xc3\xa1" "rbol\nend\n\n"
		"function %s:_proceso(dt)\n  -- una vez por cuadro dibujado\nend\n\n"
		"return %s\n", nom, clase, clase, clase, clase);
	char r[RUTA * 2]; ruta_proy(r, sizeof r, rel);
	if (escribir_todo(r, t, strlen(t)) != 0) { ce_con(CON_ERRO, "%s: %s", _("No se pudo crear"), rel); return; }
	char *antes = snap();
	char val[200]; snprintf(val, sizeof val, "\"%s\"", rel);
	esc_prop_poner(doc, s, "guion", val);
	commit(N_("Crear guion"), antes, "");
	reconstruir_recursos();
	recargar_vivo(); sincronizar_todo();
	abrir_guion(rel);
	ce_con(CON_MSG, "%s: %s", _("Guion creado"), rel);
	registrar("capibara-editor: guion creado %s para %s\n", rel, sel[0]);
}

static void editar_guion(void) {
	int s = sel_sec(); if (s < 0) return;
	char g[200] = "";
	if (!esc_prop_valor(doc, s, "guion", g, sizeof g)) { crear_guion(); return; }
	char u[200]; quitar_comillas(g, u, sizeof u);
	if (!u[0]) { crear_guion(); return; }
	abrir_guion(u);
}

static void comprobar_guion(void) {
	if (!cod_arch[0]) return;
	int n; const char *t = codigo_texto(cod, &n);
	char *u = malloc((size_t)n * 2 + 2);
	if (cod_conv) l1_u8(u, t, (size_t)n * 2 + 2); else { memcpy(u, t, (size_t)n); u[n] = 0; }
	if (comprobar_lua(cod_arch, u, strlen(u), true)) { ce_con(CON_MSG, "%s", _("Guion correcto")); registrar("capibara-editor: guion correcto %s\n", cod_arch); }
	free(u);
}

static void exportar_capi(void) {
	if (!proy_dir[0]) return;
	guardar_todo();
	char out[RUTA]; snprintf(out, sizeof out, "%s.capi", proy_dir);
	if (!pedir_texto(_("Exportar a .capi"), _("Fichero de salida:"), out, sizeof out) || !out[0]) return;
	if (!termina_en(out, ".capi")) strncat(out, ".capi", sizeof out - strlen(out) - 1);
	char err[200] = "";
	int n = cap_empaquetar(proy_dir, out, err, sizeof err);
	if (n < 0) { ce_con(CON_ERRO, "%s", err); return; }
	ce_con(CON_MSG, "%s: %s (%d %s)", _("Exportado"), out, n, _("ficheros"));
	registrar("capibara-editor: exportado %s %d\n", out, n);
}

/* ---------------------------------------------------------------------------------------------------- jugar */
static int juego_pid = -1, juego_fd = -1;
static char juego_lin[1024]; static int juego_n;

/* «ruta.lua:12:» dentro de un texto: devuelve la línea (0 si no hay) y la ruta */
static int ruta_y_linea(const char *t, char *arch, size_t na, const char **resto) {
	const char *p = t;
	while ((p = strstr(p, ".lua:")) != NULL) {
		const char *q = p + 5; int linea = 0;
		while (*q >= '0' && *q <= '9') linea = linea * 10 + (*q++ - '0');
		if (linea > 0) {
			const char *ini = p; while (ini > t && ini[-1] != ' ' && ini[-1] != '\t' && ini[-1] != '"' && ini[-1] != '@' && ini[-1] != '(') --ini;
			size_t l = (size_t)(p + 4 - ini); if (l >= na) l = na - 1;
			memcpy(arch, ini, l); arch[l] = 0;
			if (resto) { const char *r = q; if (*r == ':') ++r; while (*r == ' ') ++r; *resto = r; }
			return linea;
		}
		p += 5;
	}
	return 0;
}

static void linea_de_juego(const char *l) {
	char t[CONSOLA_TXT * 2], l1[CONSOLA_TXT * 2];
	const char *x = l; if (!strncmp(x, "[CAPI] ", 7)) x += 7;
	u8_l1(l1, x, sizeof l1);
	snprintf(t, sizeof t, "%s", l1);
	char arch[96]; int linea = ruta_y_linea(l1, arch, sizeof arch, NULL);
	int nivel = CON_MSG;
	if (!strncmp(x, "error en", 8) || (linea && (strstr(l1, "attempt") || strstr(l1, "error") || strstr(l1, "unexpected") || strstr(l1, "expected")))) nivel = CON_ERRO;
	else if (l[0] == '\t' || !strncmp(x, "stack traceback", 15) || strstr(l1, "no se pudo") || strstr(l1, "no existe") || strstr(l1, "desconocid")) nivel = CON_AVISO;
	if (linea) ce_con_linea(nivel, linea, t); else consola_escrever(&con, nivel, t);
	registrar("capibara-editor: juego> %s\n", l1);
}

static void juego_datos(const char *b, int n) {
	for (int i = 0; i < n; ++i) {
		if (b[i] == '\n' || juego_n >= (int)sizeof juego_lin - 1) { juego_lin[juego_n] = 0; if (juego_n && juego_lin[juego_n - 1] == '\r') juego_lin[--juego_n] = 0; if (juego_n) linea_de_juego(juego_lin); juego_n = 0; }
		else juego_lin[juego_n++] = b[i];
	}
}

static void parar_juego(void) {
	if (juego_pid > 0) { matar(juego_pid); ce_con(CON_MSG, "%s", _("Juego detenido")); registrar("capibara-editor: juego parado\n"); }
}

static void jugar(bool escena_actual) {
	if (!proy_dir[0]) return;
	if (juego_pid > 0) { parar_juego(); return; }
	guardar_todo();
	int p[2];
	if (criar_pipe(p) < 0) { ce_con(CON_ERRO, "%s", _("No se pudo crear la tubería.")); return; }
	char *argv[8]; int na = 0;
	argv[na++] = "capibara";
	char esc[160]; snprintf(esc, sizeof esc, "%s", esc_rel);
	if (escena_actual && esc_rel[0]) { argv[na++] = "--escena"; argv[na++] = esc; }
	argv[na++] = proy_dir; argv[na] = NULL;
	int g0 = duplicar(SAIDA, 28), g1 = duplicar(ERROS, 29);
	duplicar(p[1], SAIDA); duplicar(p[1], ERROS);
	int pid = executar("/apps/bin/capibara", argv);
	duplicar(g0, SAIDA); duplicar(g1, ERROS);
	fechar(g0); fechar(g1); fechar(p[1]);
	if (pid < 0) { fechar(p[0]); ce_con(CON_ERRO, "%s", _("No se pudo lanzar /apps/bin/capibara.")); return; }
	juego_pid = pid; juego_fd = p[0]; juego_n = 0;
	ce_con(CON_MSG, "%s %s", _("Jugando"), escena_actual ? esc_rel : g_proy.principal);
	registrar("capibara-editor: jugando pid %d %s\n", pid, escena_actual ? esc_rel : "principal");
}

/* recoge la salida del juego; true = hubo algo que repintar */
static bool vigilar_juego(void) {
	if (juego_pid <= 0) return false;
	bool algo = false; char b[512];
	for (;;) {
		int d = disponivel(juego_fd);
		if (d > 0) { int r = ler(juego_fd, b, (unsigned)(d < (int)sizeof b ? d : (int)sizeof b)); if (r > 0) { juego_datos(b, r); algo = true; } else break; continue; }
		if (d < 0) {   /* terminó */
			int cod_s = 0;
			fechar(juego_fd); juego_fd = -1;
			if (juego_n) { juego_lin[juego_n] = 0; linea_de_juego(juego_lin); juego_n = 0; }
			esperar(juego_pid, &cod_s);
			ce_con(CON_MSG, "%s (%s %d)", _("El juego terminó"), _("código"), cod_s);
			registrar("capibara-editor: juego terminado codigo %d\n", cod_s);
			juego_pid = -1; algo = true;
		}
		break;
	}
	return algo;
}

/* ---------------------------------------------------------------------------------------------------- gestos del lienzo */
typedef struct { char ruta[CAPI_RUTA_MAX]; float x, y, rot, sx, sy, gx, gy, cx0, cy0, m0[6], cLx, cLy; int con_caja; } Orig;
static Orig orig[MAXSEL]; static int norig;
static bool gesto; static char *gesto_antes;

static void gesto_iniciar(void) {
	gesto = true; gesto_antes = snap(); norig = 0;
	for (int i = 0; i < nsel && norig < MAXSEL; ++i) {
		Nodo *n = nodo_vivo(sel[i]); if (!n || !nodo_es2d(n)) continue;
		Orig *o = &orig[norig++];
		snprintf(o->ruta, sizeof o->ruta, "%s", sel[i]);
		o->x = n->x; o->y = n->y; o->rot = n->rot; o->sx = n->sx; o->sy = n->sy;
		nodo_pos_global(n, &o->gx, &o->gy); nodo_matriz(n, o->m0);
		float bx, by, bw, bh; o->con_caja = nodo_caja_local(n, &bx, &by, &bw, &bh);
		o->cLx = o->con_caja ? bx + bw * 0.5f : 0; o->cLy = o->con_caja ? by + bh * 0.5f : 0;
		o->cx0 = o->m0[0] * o->cLx + o->m0[2] * o->cLy + o->m0[4]; o->cy0 = o->m0[1] * o->cLx + o->m0[3] * o->cLy + o->m0[5];
	}
}

static float rej_f(float v) { return iman_on && vs.grade > 0 ? floorf(v / (float)vs.grade + 0.5f) * (float)vs.grade : v; }

static void gesto_aplicar(int ev, int a, int b, int c, int d) {
	if (norig < 1) return;
	if (ev == EDVISTA_E_MOVER) {
		float dx = (float)(a - vs.s0x), dy = (float)(b - vs.s0y);
		float ngx = rej_f(orig[0].gx + dx), ngy = rej_f(orig[0].gy + dy);
		dx = ngx - orig[0].gx; dy = ngy - orig[0].gy;
		for (int i = 0; i < norig; ++i) { Nodo *n = nodo_vivo(orig[i].ruta); if (n) { nodo_poner_pos_global(n, orig[i].gx + dx, orig[i].gy + dy); nodo_moverse(n); } }
	} else if (ev == EDVISTA_E_ESCALAR && norig == 1 && orig[0].con_caja) {
		Orig *o = &orig[0]; Nodo *n = nodo_vivo(o->ruta); if (!n) return;
		float kx = vs.s0l > 0 ? (float)c / (float)vs.s0l : 1, ky = vs.s0a > 0 ? (float)d / (float)vs.s0a : 1;
		float cxn = (float)a + (float)c * 0.5f, cyn = (float)b + (float)d * 0.5f;
		n->sx = o->sx * kx; n->sy = o->sy * ky; nodo_moverse(n);
		nodo_poner_pos_global(n, cxn - (o->m0[0] * kx * o->cLx + o->m0[2] * ky * o->cLy), cyn - (o->m0[1] * kx * o->cLx + o->m0[3] * ky * o->cLy));
		nodo_moverse(n);
	} else if (ev == EDVISTA_E_GIRAR && norig == 1) {
		Orig *o = &orig[0]; Nodo *n = nodo_vivo(o->ruta); if (!n) return;
		int dg = a - vs.s0rot; while (dg > 180) dg -= 360; while (dg < -180) dg += 360;
		float r = (float)dg * 3.14159265f / 180.0f, cs = cosf(r), sn = sinf(r);
		n->rot = o->rot + (float)dg; nodo_moverse(n);
		float a1 = cs * o->m0[0] - sn * o->m0[1], b1 = sn * o->m0[0] + cs * o->m0[1], c1 = cs * o->m0[2] - sn * o->m0[3], d1 = sn * o->m0[2] + cs * o->m0[3];
		nodo_poner_pos_global(n, o->cx0 - (a1 * o->cLx + c1 * o->cLy), o->cy0 - (b1 * o->cLx + d1 * o->cLy));
		nodo_moverse(n);
	}
	(void)d;
}

static void gesto_terminar(int ev) {
	gesto = false;
	char *antes = gesto_antes; gesto_antes = NULL;
	if (!antes) return;
	int cambios = 0;
	for (int i = 0; i < norig; ++i) {
		Nodo *n = nodo_vivo(orig[i].ruta); int s = esc_buscar_ruta(doc, orig[i].ruta);
		if (!n || s < 0) continue;
		char t[96], u[48], v[48];
		if (fabsf(n->x - orig[i].x) > 1e-4f || fabsf(n->y - orig[i].y) > 1e-4f) {
			fmt_n(u, sizeof u, n->x); fmt_n(v, sizeof v, n->y); snprintf(t, sizeof t, "%s %s", u, v);
			aplicar_posicion_sin_xy(s); esc_prop_poner(doc, s, "posicion", t); ++cambios;
		}
		if (fabsf(n->rot - orig[i].rot) > 1e-4f) { fmt_n(u, sizeof u, n->rot); esc_prop_poner(doc, s, "rotacion", u); ++cambios; }
		if (fabsf(n->sx - orig[i].sx) > 1e-4f || fabsf(n->sy - orig[i].sy) > 1e-4f) {
			fmt_n(u, sizeof u, n->sx); fmt_n(v, sizeof v, n->sy); snprintf(t, sizeof t, "%s %s", u, v);
			esc_prop_poner(doc, s, "escala", t); ++cambios;
		}
	}
	const char *nom = ev == EDVISTA_E_MOVER ? N_("Mover nodos") : ev == EDVISTA_E_ESCALAR ? N_("Escalar nodo") : N_("Girar nodo");
	if (cambios) {
		commit(nom, antes, "");
		recargar_vivo();
		Nodo *n = sel_nodo();
		if (n) {
			char a1[32], a2[32], a3[32], a4[32], a5[32];
			fmt_n(a1, sizeof a1, n->x); fmt_n(a2, sizeof a2, n->y); fmt_n(a3, sizeof a3, n->rot); fmt_n(a4, sizeof a4, n->sx); fmt_n(a5, sizeof a5, n->sy);
			registrar("capibara-editor: %s %s posicion %s %s rot %s escala %s %s\n", ev == EDVISTA_E_MOVER ? "movido" : ev == EDVISTA_E_ESCALAR ? "escalado" : "girado", sel[0], a1, a2, a3, a4, a5);
		}
	} else free(antes);
	sincronizar_todo();
}

static void vista_aviso(void *ctx, int ev, int a, int b, int c, int d, bool fim) {
	(void)ctx;
	switch (ev) {
	case EDVISTA_E_CLIQUE: {
		(void)b; (void)c;
		if (a >= 0 && a < nlista) {
			char r[CAPI_RUTA_MAX]; ruta_editable(lista[a], r, sizeof r);
			if (d & 3) { int k = sel_indice(r); if (k >= 0) { memmove(sel[k], sel[k + 1], sizeof sel[0] * (size_t)(nsel - k - 1)); --nsel; } else sel_agregar(r); }
			else if (sel_indice(r) < 0) sel_unica(r);
		} else if (!(d & 3)) sel_limpiar();
		sincronizar_todo();
		registrar("capibara-editor: sel %s\n", nsel ? sel[0] : "-");
		break;
	}
	case EDVISTA_E_RET: {
		sel_limpiar();
		for (int i = 0; i < nlista; ++i) {
			Nodo *n = lista[i];
			if (n == live || !nodo_es2d(n) || !(n->flags & F_VISIBLE)) continue;
			float x0, y0, x1, y1; caja_aabb(n, &x0, &y0, &x1, &y1);
			if (x0 < (float)c && x1 > (float)a && y0 < (float)d && y1 > (float)b) { char r[CAPI_RUTA_MAX]; ruta_editable(n, r, sizeof r); sel_agregar(r); }
		}
		sincronizar_todo();
		registrar("capibara-editor: rectangulo %d %d %d %d toca %d\n", a, b, c, d, nsel);
		break;
	}
	case EDVISTA_E_MOVER: case EDVISTA_E_ESCALAR: case EDVISTA_E_GIRAR: {
		int esperado = ev == EDVISTA_E_MOVER ? HM_MOVER : ev == EDVISTA_E_ESCALAR ? HM_ESCALAR : HM_GIRAR;
		if (modo != esperado || !nsel) { if (fim) sel_a_vista(); break; }
		if (!gesto && !fim) gesto_iniciar();
		if (gesto) gesto_aplicar(ev, a, b, c, d);
		if (fim && gesto) gesto_terminar(ev);
		break;
	}
	case EDVISTA_E_ZOOM: registrar("capibara-editor: zoom %d ox %d oy %d\n", vs.zoom, vs.ox, vs.oy); log_nodos(); break;
	case EDVISTA_E_DESLOCAR: if (fim) { registrar("capibara-editor: desplazado ox %d oy %d zoom %d\n", vs.ox, vs.oy, vs.zoom); log_nodos(); } break;
	}
}

/* ---------------------------------------------------------------------------------------------------- pintar casillas */
static bool pintando; static char *pint_antes; static int pint_borrar, pint_celdas;

static int pint_lcx = -1000, pint_lcy = -1000;   /* última casilla pintada del trazo (se unen con una línea) */

static void pintar_casilla(Nodo *m, int capa, int cs, int cx, int cy) {
	DMapa *d = m->d;
	int c0 = 0, f0 = 0, c1 = 0, f1 = 0;
	if (!pint_borrar) edpaleta_selecao(&pal, &c0, &f0, &c1, &f1);
	for (int f = f0; f <= f1; ++f) for (int c = c0; c <= c1; ++c) {
		int x = cx + c - c0, y = cy + f - f0;
		if (x < 0 || y < 0 || x >= d->w || y >= d->h) continue;
		int v = pint_borrar ? -1 : edpaleta_indice(&pal, c, f);
		if (mapa_celda(m, capa, x, y) == v) continue;
		esc_celda_poner(doc, cs, x, y, v, d->w, d->h);
		mapa_poner(m, capa, x, y, v);
		++pint_celdas;
	}
}

static void pintar_en(int sx, int sy) {
	Nodo *m = mapa_sel();
	if (!m) return;
	DMapa *d = m->d;
	int wx, wy; edvista_para_mundo(&vs, sx, sy, &wx, &wy);
	float mm[6]; nodo_matriz(m, mm);
	float det = mm[0] * mm[3] - mm[1] * mm[2]; if (fabsf(det) < 1e-9f) return;
	float dx = (float)wx - mm[4], dy = (float)wy - mm[5];
	float lx = (mm[3] * dx - mm[2] * dy) / det, ly = (-mm[1] * dx + mm[0] * dy) / det;
	int cx = (int)floorf(lx / (float)d->cw), cy = (int)floorf(ly / (float)d->ch);
	int capa = cur_capa < d->ncapas ? cur_capa : 0;
	int ms = sel_sec(), cs[6];
	if (ms < 0 || esc_capas(doc, ms, cs, 6) <= capa) return;
	/* del último punto al actual, casilla a casilla (el ratón puede saltar varias entre dos eventos) */
	int x0 = pint_lcx > -1000 ? pint_lcx : cx, y0 = pint_lcy > -1000 ? pint_lcy : cy;
	int n = abs(cx - x0) > abs(cy - y0) ? abs(cx - x0) : abs(cy - y0);
	for (int i = 0; i <= n; ++i) pintar_casilla(m, capa, cs[capa], n ? x0 + (cx - x0) * i / n : cx, n ? y0 + (cy - y0) * i / n : cy);
	pint_lcx = cx; pint_lcy = cy;
	nodo_moverse(m);
}

static bool pintar_empezar(const evento_t *e) {
	Nodo *m = mapa_sel();
	if (!m) { ce_con(CON_AVISO, "%s", _("Elige un MapaCasillas en el árbol para pintar.")); return true; }
	DMapa *d = m->d;
	if (!pal.img && modo == HM_PINTAR && !(e->botoes & 2)) { ce_con(CON_AVISO, "%s", _("El mapa no tiene textura: pon una en el inspector.")); return true; }
	pint_antes = snap(); pint_celdas = 0;
	if (d->ncapas == 0) {   /* el mapa aún no tiene capas: se crea la primera */
		esc_capa_nueva(doc, sel_sec(), "base", 1, d->w, d->h);
		recargar_vivo(); cur_capa = 0;
	}
	pintando = true; pint_borrar = modo == HM_BORRAR || (e->botoes & 2) != 0; pint_lcx = pint_lcy = -1000;
	pintar_en(e->x, e->y);
	return true;
}

static void pintar_terminar(void) {
	pintando = false;
	char *antes = pint_antes; pint_antes = NULL;
	if (!antes) return;
	commit(pint_borrar ? N_("Borrar casillas") : N_("Pintar casillas"), antes, "");
	if (pint_celdas) {
		registrar("capibara-editor: %s %d casillas capa %d\n", pint_borrar ? "borradas" : "pintadas", pint_celdas, cur_capa);
		ce_con(CON_MSG, "%s: %d", pint_borrar ? _("Casillas borradas") : _("Casillas pintadas"), pint_celdas);
	}
}

static bool dentro_vista(int x, int y) { return x >= vs.x && y >= vs.y && x < vs.x + vs.l && y < vs.y + vs.a; }

/* true = el evento era del pincel */
static bool pincel_evento(const evento_t *e) {
	if (modo != HM_PINTAR && modo != HM_BORRAR) return false;
	if (e->tipo == EV_CLIQUE && (e->botoes & 3) && !(e->botoes & 4) && dentro_vista(e->x, e->y) && !vs.mao) return pintar_empezar(e);
	if (e->tipo == EV_MOVER && pintando) { pintar_en(e->x, e->y); return true; }
	if (e->tipo == EV_SOLTOU && pintando) { pintar_terminar(); return true; }
	return false;
}

/* ---------------------------------------------------------------------------------------------------- arrastrar y soltar */
static bool dnd_vista;

static bool alvo_arbol(void *ctx, int fase, const arrsol_item_t *it, int x, int y) {
	(void)ctx;
	if (it->tipo != TIPO_NODO) return false;
	if (fase != ARRSOL_SOLTA) return true;
	int from = it->dados[0] | (it->dados[1] << 8);
	int fila = arbol_fila_en(y);
	if (fila < 0 || from < 0 || from >= doc->nsec) return false;
	int destino = arv_sec[fila];
	if (destino == from) return false;
	int vis = 0; for (int i = 0; i < fila; ++i) if (arvore_visivel(&arv, i)) ++vis;
	int top = arv.y + 2 + (vis - arv.topo) * 20, frac = (y - top) * 100 / 20;
	const EscSec *t = &doc->sec[destino];
	char padre[CAPI_RUTA_MAX]; int antes = -1;
	int es_raiz = !strcmp(t->ruta, ".");
	if (es_raiz || (frac >= 25 && frac < 75)) snprintf(padre, sizeof padre, "%s", t->ruta);                  /* como último hijo */
	else if (frac < 25) { snprintf(padre, sizeof padre, "%s", t->padre[0] ? t->padre : "."); antes = destino; }     /* delante */
	else {                                                                                                     /* detrás: delante del siguiente hermano */
		snprintf(padre, sizeof padre, "%s", t->padre[0] ? t->padre : ".");
		int niv = arv_niv[fila];
		for (int i = fila + 1; i < arv.n; ++i) { if (arv_niv[i] < niv) break; if (arv_niv[i] == niv) { antes = arv_sec[i]; break; } }
	}
	char ruta_origen[CAPI_RUTA_MAX]; snprintf(ruta_origen, sizeof ruta_origen, "%s", doc->sec[from].ruta);
	char *snap0 = snap();
	int r = esc_nodo_mover(doc, from, padre, antes);
	if (r < 0) { free(snap0); ce_con(CON_AVISO, "%s", _("No se puede mover el nodo ahí.")); return false; }
	char nueva[CAPI_RUTA_MAX]; snprintf(nueva, sizeof nueva, "%s", doc->sec[r].ruta);
	commit(N_("Mover en el árbol"), snap0, "");
	sel_unica(nueva); tras_estructura();
	registrar("capibara-editor: arbol movido %s -> %s\n", ruta_origen, nueva);
	return true;
}

static bool alvo_vista(void *ctx, int fase, const arrsol_item_t *it, int x, int y) {
	(void)ctx;
	if (it->tipo != TIPO_RECURSO) return false;
	int k = it->dados[0] | (it->dados[1] << 8);
	if (k < 0 || k >= nrec) return false;
	const char *ruta = rec_ruta[k];
	int png = termina_en(ruta, ".png") || termina_en(ruta, ".bmp") || termina_en(ruta, ".jpg"), esc = termina_en(ruta, ".esc");
	if (!png && !esc) return false;
	if (fase == ARRSOL_SAI) { dnd_vista = false; return false; }
	if (fase == ARRSOL_ENTRA) { dnd_vista = true; return true; }
	if (fase == ARRSOL_MOVE) return true;
	dnd_vista = false;
	int wx, wy; edvista_para_mundo(&vs, x, y, &wx, &wy);
	char *antes = snap(); char t[64]; snprintf(t, sizeof t, "%d %d", snap_rejilla(wx), snap_rejilla(wy));
	const char *base = strrchr(ruta, '/'); base = base ? base + 1 : ruta;
	char nom[CAPI_NOME_MAX]; snprintf(nom, sizeof nom, "%s", base); char *pt = strrchr(nom, '.'); if (pt) *pt = 0;
	for (char *c = nom; *c; ++c) if (!isalnum((unsigned char)*c) && *c != '_') *c = '_';
	if (!nom[0]) snprintf(nom, sizeof nom, "Nodo");
	if (nom[0] >= 'a' && nom[0] <= 'z') nom[0] = (char)(nom[0] - 32);
	int s; const char *padre = nsel ? sel[0] : ".";
	if (png) { s = esc_nodo_nuevo(doc, "Sprite", nom, padre, -1); if (s >= 0) { char q[200]; snprintf(q, sizeof q, "\"%s\"", ruta); esc_prop_poner(doc, s, "textura", q); } }
	else { if (!strcmp(ruta, esc_rel)) { free(antes); return false; } s = esc_instancia_nueva(doc, ruta, nom, padre, -1); }
	if (s < 0) { free(antes); return false; }
	esc_prop_poner(doc, s, "posicion", t);
	char nueva[CAPI_RUTA_MAX]; snprintf(nueva, sizeof nueva, "%s", doc->sec[s].ruta);
	commit(png ? N_("Añadir sprite") : N_("Instanciar escena"), antes, "");
	sel_unica(nueva); tras_estructura();
	registrar("capibara-editor: soltado %s en %d %d como %s\n", ruta, wx, wy, nueva);
	return true;
}

static void registrar_alvos(void) {
	arrsol_alvos_limpar();
	arrsol_alvo(arv.x, arv.y, arv.l, arv.a, alvo_arbol, NULL);
	arrsol_alvo(vs.x, vs.y, vs.l, vs.a, alvo_vista, NULL);
}

/* ---------------------------------------------------------------------------------------------------- menús */
static const char *men_nom[7]; static const char *men_txt[7]; static int men_acc[7][40]; static char men_anad[600];

static void menu_construir(void) {
	static const int A0[] = { A_NUEVO_PROY, A_ABRIR_PROY, A_EJEMPLO_PLAT, A_EJEMPLO_ESC, A_NUEVA_ESCENA, 0, A_GUARDAR, A_EXPORTAR, 0, A_SALIR };
	static const int A1[] = { A_DESHACER, A_REHACER, 0, A_DUPLICAR, A_BORRAR, A_RENOMBRAR, 0, A_VARIABLE, A_SENAL };
	static const int A3[] = { A_JUGAR, A_JUGAR_ESCENA, A_PARAR, 0, A_CREAR_GUION, A_EDITAR_GUION, A_COMPROBAR };
	static const int A4[] = { A_REJILLA, A_IMAN, A_COLISIONES, 0, A_RESTABLECER, A_CAPA_SIG, A_CAPA_NUEVA };
	men_nom[0] = _("Archivo"); men_nom[1] = _("Editar"); men_nom[2] = _("Añadir"); men_nom[3] = _("Proyecto"); men_nom[4] = _("Ver");
	men_txt[0] = MENU_("Nuevo proyecto...\tCtrl+N\nAbrir proyecto...\tCtrl+O\nAbrir ejemplo: plataformas\nAbrir ejemplo: escuadrón\nNueva escena...\n-\nGuardar\tCtrl+S\nExportar a .capi...\tCtrl+E\n-\nSalir");
	men_txt[1] = MENU_("Deshacer\tCtrl+Z\nRehacer\tCtrl+Y\n-\nDuplicar\tCtrl+D\nBorrar\tSupr\nRenombrar\tF2\n-\nAñadir variable...\tF3\nConectar señal...\tF4");
	men_txt[3] = MENU_("Jugar\tF5\nJugar la escena abierta\tF6\nParar\tF7\n-\nCrear guion del nodo\nEditar guion del nodo\nComprobar guion");
	men_txt[4] = MENU_("Rejilla\nImán\nColisiones\n-\nEncuadrar la escena\tInicio\nCapa de casillas siguiente\nCapa de casillas nueva");
	memcpy(men_acc[0], A0, sizeof A0); memcpy(men_acc[1], A1, sizeof A1); memcpy(men_acc[3], A3, sizeof A3); memcpy(men_acc[4], A4, sizeof A4);
	/* Añadir: un tipo de nodo por línea y la instancia de otra escena */
	men_anad[0] = 0;
	for (int t = 0; t < T_N; ++t) { strncat(men_anad, CAPI_TIPOS[t], sizeof men_anad - strlen(men_anad) - 1); strncat(men_anad, "\n", sizeof men_anad - strlen(men_anad) - 1); men_acc[2][t] = A_TIPO0 + t; }
	strncat(men_anad, "-\n", sizeof men_anad - strlen(men_anad) - 1); men_acc[2][T_N] = 0;
	strncat(men_anad, idioma_traducir_sl("Instancia de escena..."), sizeof men_anad - strlen(men_anad) - 1); men_acc[2][T_N + 1] = A_INSTANCIA;
	men_txt[2] = men_anad;
	for (int i = 0; i < 5; ++i) { menu.nomes[i] = men_nom[i]; menu.itens[i] = men_txt[i]; }
	menu.n = 5;
}

/* ---------------------------------------------------------------------------------------------------- disposición y dibujo */
static void rect_h(int h, int *x, int *y, int *l, int *a) { if (!paineis_rect(&pn, h, x, y, l, a)) { *x = *y = 0; *l = *a = 0; } }

static void layout(void) {
	barra_menu_iniciar(&menu, 0, 0, jn->larg);
	paineis_mover(&pn, 0, CAB_A, jn->larg, jn->alt - CAB_A);
	int x, y, l, a;
	rect_h(H_ARBOL, &x, &y, &l, &a);
	arv.x = x; arv.y = y + TITULO_A; arv.l = l; arv.a = a > TITULO_A ? a - TITULO_A : 1;
	rect_h(H_VISTA, &x, &y, &l, &a);
	edvista_mover(&vs, x, y, l, a);
	rect_h(H_CODIGO, &x, &y, &l, &a);
	codigo_mover(cod, x, y + TITULO_A, l, a > TITULO_A ? a - TITULO_A : 1);
	rect_h(H_INSPETOR, &x, &y, &l, &a);
	ins.x = x; ins.y = y + TITULO_A; ins.l = l; ins.a = a > TITULO_A ? a - TITULO_A : 1;
	ins.col_nome = l * 45 / 100 < 70 ? 70 : l * 45 / 100;
	rect_h(H_RECURSOS, &x, &y, &l, &a);
	rec.x = x; rec.y = y + TITULO_A; rec.l = l; rec.a = a > TITULO_A ? a - TITULO_A : 1;
	rect_h(H_PALETA, &x, &y, &l, &a);
	pal.x = x; pal.y = y + TITULO_A; pal.l = l; pal.a = a > TITULO_A ? a - TITULO_A : 1;
	rect_h(H_CONSOLA, &x, &y, &l, &a);
	con.x = x; con.y = y; con.l = l; con.a = a;
}

static void log_posiciones(void) {
	int x, y, l, a;
	static const struct { const char *n; int h; } T[] = { { "arbol", H_ARBOL }, { "vista", H_VISTA }, { "codigo", H_CODIGO }, { "inspetor", H_INSPETOR }, { "recursos", H_RECURSOS }, { "paleta", H_PALETA }, { "consola", H_CONSOLA } };
	for (int i = 0; i < 7; ++i) { rect_h(T[i].h, &x, &y, &l, &a); registrar("capibara-editor: pos %s %d %d %d %d\n", T[i].n, x, y, l, a); }
	registrar("capibara-editor: pos arbolfilas %d %d\n", arv.y, arv.l);
	for (int i = 0; i < NBTN; ++i) registrar("capibara-editor: pos boton%d %d %d %d %d\n", i, btn[i].x, btn[i].y, btn[i].l, btn[i].a);
	registrar("capibara-editor: pos vista_zoom %d ox %d oy %d\n", vs.zoom, vs.ox, vs.oy);
	for (int i = 0; i < pn.n; i++) if (pn.no[i].tipo != PD_FOLHA) registrar("capibara-editor: pos sep%d %d %d %d %d plegado %d\n", i, pn.no[i].sx, pn.no[i].sy, pn.no[i].sl, pn.no[i].sh, pn.no[i].plegado);
	for (int i = 0; i < menu.n; ++i) registrar("capibara-editor: pos menu%d %d %d %d %d\n", i, menu.x_item[i], menu.y, menu.l_item[i], menu.a);
	registrar("capibara-editor: pos inspector_filas %d\n", nfilas);
	log_nodos();
	for (int k = 0; k < 5; k++) { consola_boton(&con, k, &x, &y, &l, &a); registrar("capibara-editor: pos cbtn%d %d %d %d %d\n", k, x, y, l, a); }
}

/* para las pruebas: dónde cae cada nodo del fichero en la ventana */
static void log_nodos(void) {
	if (!doc || !live) return;
	int n = 0;
	for (int i = 0; i < doc->nsec && n < 40; ++i) {
		const EscSec *s = &doc->sec[i];
		if (s->clase != ES_NODO && s->clase != ES_INSTANCIA) continue;
		Nodo *nv = nodo_vivo(s->ruta); float cx, cy, w, h, r;
		if (!nv || !caja_mundo(nv, &cx, &cy, &w, &h, &r)) continue;
		int sx, sy; edvista_para_tela(&vs, (int)cx, (int)cy, &sx, &sy);
		registrar("capibara-editor: nodopos %s %d %d %d %d\n", s->ruta, sx, sy, (int)w, (int)h);
		++n;
	}
}

static void layout_y_log(void) { layout(); log_posiciones(); }

static void titulo_panel(int h, const char *t) {
	int x, y, l, a; rect_h(h, &x, &y, &l, &a);
	if (l <= 0 || a <= 0) return;
	g_retangulo(jn, x, y, l, TITULO_A, mod_tema.barra);
	g_retangulo(jn, x, y + TITULO_A - 1, l, 1, mod_tema.borde);
	g_texto(jn, x + 6, y + 1, t, mod_tema.texto_fraco);
}

static void actualizar_botones(void) {
	btn[B_MOVER].ativo = modo == HM_MOVER; btn[B_GIRAR].ativo = modo == HM_GIRAR; btn[B_ESCALAR].ativo = modo == HM_ESCALAR;
	btn[B_PINTAR].ativo = modo == HM_PINTAR; btn[B_BORRAR].ativo = modo == HM_BORRAR;
	btn[B_REJILLA].ativo = vs.grade > 0; btn[B_IMAN].ativo = iman_on;
	btn[B_DESHACER].desabilitado = !ed_desfazer_pode(&pila); btn[B_REHACER].desabilitado = !ed_refazer_pode(&pila);
	btn[B_JUGAR].desabilitado = juego_pid > 0 || !proy_dir[0]; btn[B_PARAR].desabilitado = juego_pid <= 0;
	btn[B_GUARDAR].desabilitado = !proy_dir[0]; btn[B_EXPORTAR].desabilitado = !proy_dir[0];
}

static void redibujar(void) {
	g_retangulo(jn, 0, 0, jn->larg, jn->alt, COR_FUNDO);
	g_retangulo(jn, 0, MENU_A, jn->larg, BARRA_A, mod_tema.barra);
	g_retangulo(jn, 0, CAB_A - 1, jn->larg, 1, mod_tema.borde);
	actualizar_botones();
	for (int i = 0; i < NBTN; ++i) mbotao_desenhar(jn, &btn[i]);
	paineis_desenhar(jn, &pn);
	char t[160];
	snprintf(t, sizeof t, "%s%s", _("Escena"), esc_mod ? " *" : "");
	if (esc_rel[0]) { snprintf(t, sizeof t, "%s: %s%s", _("Escena"), esc_rel, esc_mod ? " *" : ""); }
	titulo_panel(H_ARBOL, t);
	titulo_panel(H_INSPETOR, _("Inspector"));
	titulo_panel(H_RECURSOS, _("Recursos"));
	titulo_panel(H_PALETA, pal_valida ? _("Paleta de casillas") : _("Paleta"));
	snprintf(t, sizeof t, "%s%s%s", _("Guion"), cod_arch[0] ? ": " : "", cod_arch);
	if (cod_cambiado()) strncat(t, " *", sizeof t - strlen(t) - 1);
	titulo_panel(H_CODIGO, t);
	int x, y, l, a;
	if (paineis_rect(&pn, H_ARBOL, &x, &y, &l, &a)) arvore_desenhar(jn, &arv);
	if (paineis_rect(&pn, H_VISTA, &x, &y, &l, &a)) edvista_desenhar(jn, &vs);
	if (paineis_rect(&pn, H_CODIGO, &x, &y, &l, &a) && l > 0 && a > TITULO_A) codigo_desenhar(jn, cod);
	if (paineis_rect(&pn, H_INSPETOR, &x, &y, &l, &a)) inspetor_desenhar(jn, &ins);
	if (paineis_rect(&pn, H_RECURSOS, &x, &y, &l, &a)) arvore_desenhar(jn, &rec);
	if (paineis_rect(&pn, H_PALETA, &x, &y, &l, &a)) { if (pal.img) edpaleta_desenhar(jn, &pal); else g_texto(jn, pal.x + 6, pal.y + 6, _("Elige un MapaCasillas con textura."), mod_tema.texto_fraco); }
	if (paineis_rect(&pn, H_CONSOLA, &x, &y, &l, &a)) consola_desenhar(jn, &con);
	if (dnd_vista) g_moldura(jn, vs.x + 1, vs.y + 1, vs.l - 2, vs.a - 2, mod_tema.acento);
	barra_menu_desenhar(jn, &menu);
	jan_mostrar(jn);
}

static void usar_foco(int f) { foco = f; codigo_foco(cod, f == FO_CODIGO); }

static int foco_en(int x, int y) {
	static const struct { int h, f; } T[] = { { H_ARBOL, FO_ARBOL }, { H_VISTA, FO_VISTA }, { H_CODIGO, FO_CODIGO }, { H_INSPETOR, FO_INSPETOR }, { H_RECURSOS, FO_RECURSOS }, { H_PALETA, FO_PALETA }, { H_CONSOLA, FO_CONSOLA } };
	for (int i = 0; i < 7; i++) { int rx, ry, rl, ra; if (paineis_rect(&pn, T[i].h, &rx, &ry, &rl, &ra) && x >= rx && y >= ry && x < rx + rl && y < ry + ra) return T[i].f; }
	return FO_NADA;
}

/* ---------------------------------------------------------------------------------------------------- órdenes y bucle principal */
static bool salir;

static unsigned hash_doc(void) {
	size_t n; char *t = esc_texto(doc, &n); unsigned h = 2166136261u;
	for (size_t i = 0; i < n; ++i) { h ^= (unsigned char)t[i]; h *= 16777619u; }
	free(t);
	return h;
}
static void log_estado(const char *que) {
	registrar("capibara-editor: %s deshacer %d rehacer %d hash %08x nodos %d sin_guardar %d\n", que, ed_desfazer_pode(&pila), ed_refazer_pode(&pila), hash_doc(), live ? nodo_contar(live) : 0, esc_mod);
}

static void abrir_recurso(int i) {
	if (i < 0 || i >= nrec || rec_niv[i] != 1) return;
	const char *r = rec_ruta[i];
	if (termina_en(r, ".esc")) {
		if (esc_mod && !confirmar(_("Capibara"), _("La escena tiene cambios sin guardar. ¿Descartarlos?"))) return;
		abrir_escena(r);
	} else if (termina_en(r, ".lua")) abrir_guion(r);
	else if (termina_en(r, ".png") || termina_en(r, ".bmp") || termina_en(r, ".jpg")) { Nodo *m = mapa_sel(); DMapa *d = m ? m->d : NULL; paleta_de_textura(r, d ? d->cw : 16, d ? d->ch : 16); ce_con(CON_MSG, "%s: %s", _("Paleta"), r); }
}

static void ejecutar_accion(int a) {
	switch (a) {
	case A_NUEVO_PROY: proyecto_nuevo(); break;
	case A_ABRIR_PROY: {
		char d[RUTA]; const char *home = getenv("HOME"); snprintf(d, sizeof d, "%s", proy_dir[0] ? proy_dir : (home && *home ? home : "/home"));
		if (dialogo_carpeta(_("Abrir proyecto de Capibara"), d, d, sizeof d)) abrir_proyecto(d);
		break;
	}
	case A_EJEMPLO_PLAT: abrir_ejemplo("plataformas"); break;
	case A_EJEMPLO_ESC: abrir_ejemplo("disparos"); break;
	case A_NUEVA_ESCENA: escena_nueva(); break;
	case A_GUARDAR: guardar_todo(); break;
	case A_EXPORTAR: exportar_capi(); break;
	case A_SALIR: salir = true; break;
	case A_DESHACER: if (ed_desfazer(&pila)) { sincronizar_todo(); ce_con(CON_MSG, "%s", _("Deshecho")); log_estado("deshacer"); } break;
	case A_REHACER: if (ed_refazer(&pila)) { sincronizar_todo(); ce_con(CON_MSG, "%s", _("Rehecho")); log_estado("rehacer"); } break;
	case A_DUPLICAR: duplicar_seleccion(); break;
	case A_BORRAR: borrar_seleccion(); break;
	case A_RENOMBRAR: renombrar_seleccion(); break;
	case A_VARIABLE: anadir_variable(); break;
	case A_SENAL: conectar_senal(); break;
	case A_INSTANCIA: instancia_nueva(); break;
	case A_JUGAR: jugar(false); break;
	case A_JUGAR_ESCENA: jugar(true); break;
	case A_PARAR: parar_juego(); break;
	case A_CREAR_GUION: crear_guion(); break;
	case A_EDITAR_GUION: editar_guion(); break;
	case A_COMPROBAR: comprobar_guion(); break;
	case A_REJILLA: vs.grade = vs.grade ? 0 : 16; registrar("capibara-editor: rejilla %d\n", vs.grade); break;
	case A_IMAN: iman_on = !iman_on; registrar("capibara-editor: iman %d\n", iman_on); break;
	case A_COLISIONES: ver_colisiones = !ver_colisiones; registrar("capibara-editor: colisiones %d\n", ver_colisiones); break;
	case A_RESTABLECER: encuadrar_vista(); break;
	case A_CAPA_SIG: { Nodo *m = mapa_sel(); if (m) { DMapa *d = m->d; cur_capa = d->ncapas ? (cur_capa + 1) % d->ncapas : 0; ce_con(CON_MSG, "%s: %d", _("Capa de casillas"), cur_capa + 1); registrar("capibara-editor: capa %d\n", cur_capa); } break; }
	case A_CAPA_NUEVA: {
		Nodo *m = mapa_sel(); int ms = sel_sec();
		if (m && ms >= 0) {
			DMapa *d = m->d; if (d->ncapas >= 6) { ce_con(CON_AVISO, "%s", _("Un mapa admite hasta 6 capas.")); break; }
			char nom[40]; snprintf(nom, sizeof nom, "capa%d", d->ncapas + 1);
			char *antes = snap(); esc_capa_nueva(doc, ms, nom, 0, d->w, d->h); commit(N_("Capa nueva"), antes, "");
			recargar_vivo(); cur_capa = d->ncapas; Nodo *m2 = mapa_sel(); cur_capa = m2 ? ((DMapa *)m2->d)->ncapas - 1 : 0; sincronizar_todo();
		}
		break;
	}
	default:
		if (a >= A_TIPO0 && a < A_TIPO0 + T_N) anadir_nodo(a - A_TIPO0);
	}
}

static int accion_de_boton(int i) {
	switch (i) {
	case B_NUEVO: return A_NUEVO_PROY; case B_GUARDAR: return A_GUARDAR; case B_ABRIR: return A_ABRIR_PROY; case B_DESHACER: return A_DESHACER; case B_REHACER: return A_REHACER;
	case B_REJILLA: return A_REJILLA; case B_IMAN: return A_IMAN; case B_JUGAR: return A_JUGAR; case B_PARAR: return A_PARAR; case B_EXPORTAR: return A_EXPORTAR;
	}
	return A_NADA;
}

static bool hay_cambios(void) { return esc_mod || cod_cambiado(); }

int editor_principal(int argc, char **argv) {
	jn = jan_abrir(_("Capibara: editor"), VENT_L, VENT_A);
	if (!jn) return 1;
	jan_estilo(jn, JAN_REDIMENSIONAVEL, 760, 520);
	g_capi_log_hook = hook_log; g_modo_editor = 1;
	if (guion_iniciar() != 0) registrar("capibara-editor: no se pudo iniciar Lua\n");
	rec_poner_decodificador(ed_decodificar);
	ed_desfazer_iniciar(&pila, 300);
	cod = codigo_criar(0, 0, 100, 100);
	if (!cod) return 1;
	codigo_regras(cod, &edrealce_lua);
	consola_iniciar(&con, 0, 0, 100, 100);
	doc = esc_abrir_texto("");

	paineis_iniciar(&pn, 0, CAB_A, VENT_L, VENT_A - CAB_A);
	int n_arbol = paineis_folha(&pn, H_ARBOL), n_vista = paineis_folha(&pn, H_VISTA), n_cod = paineis_folha(&pn, H_CODIGO);
	int n_ins = paineis_folha(&pn, H_INSPETOR), n_rec = paineis_folha(&pn, H_RECURSOS), n_pal = paineis_folha(&pn, H_PALETA), n_con = paineis_folha(&pn, H_CONSOLA);
	int centro = paineis_dividir(&pn, PD_PILHA, n_vista, n_cod, 640, 120, 60, 2);
	int medio = paineis_dividir(&pn, PD_LADO, centro, n_ins, 720, 200, 220, 0);
	int alto = paineis_dividir(&pn, PD_LADO, n_arbol, medio, 190, 100, 300, 1);
	int pc = paineis_dividir(&pn, PD_LADO, n_pal, n_con, 400, 120, 120, 0);
	int bajo = paineis_dividir(&pn, PD_LADO, n_rec, pc, 230, 120, 250, 0);
	paineis_dividir(&pn, PD_PILHA, alto, bajo, 720, 200, 90, 2);

	memset(&arv, 0, sizeof arv); arv.sel = -1;
	memset(&rec, 0, sizeof rec); rec.sel = -1;
	edvista_iniciar(&vs, 0, 0, 100, 100);
	vs.ox = 90; vs.oy = 60; vs.zoom = 2000;
	vs.ctx = NULL; vs.desenhar = vista_dibujar; vs.acertar = vista_acertar; vs.aviso = vista_aviso;
	vs.iman = false; vs.grade = 16;
	inspetor_iniciar(&ins, 0, 0, 100, 100, props, 0);
	ins.mudou = ins_mudou;
	edpaleta_iniciar(&pal, 0, 0, 100, 100, NULL, 16, 16);

	static const char *const rot[NBTN] = { N_("Nuevo"), N_("Abrir"), N_("Guardar"), N_("Deshacer"), N_("Rehacer"), N_("Mover"), N_("Girar"), N_("Escalar"), N_("Pintar"), N_("Goma"),
		N_("Rejilla"), N_("Imán"), N_("Jugar"), N_("Parar"), N_("Exportar") };
	int bx = 6;
	for (int i = 0; i < NBTN; ++i) {
		memset(&btn[i], 0, sizeof btn[i]);
		btn[i].texto = rot[i];
		btn[i].variante = i == B_JUGAR ? MB_PRIMARIO : i == B_PARAR ? MB_PELIGRO : MB_SECUNDARIO;
		btn[i].fundo = mod_tema.barra;
		btn[i].y = MENU_A + 3; btn[i].a = 28;
		mbotao_ajustar(&btn[i]);
		btn[i].l -= 8;   /* un poco más ceñidos para que quepan en 1000 píxeles */
		if (i == B_DESHACER || i == B_MOVER || i == B_REJILLA || i == B_JUGAR) bx += 8;
		btn[i].x = bx;
		bx += btn[i].l + 2;
	}
	menu_construir();
	layout();
	paineis_plegar(&pn, centro, true);   /* el código se despliega al abrir un guion */
	layout();

	const char *proyecto = NULL, *ejemplo = NULL;
	for (int i = 1; i < argc; ++i) {
		if (!strncmp(argv[i], "--ejemplo=", 10)) ejemplo = argv[i] + 10;
		else if (argv[i][0] != '-') proyecto = argv[i];
	}
	sincronizar_todo();
	reconstruir_recursos();
	redibujar();
	if (proyecto) abrir_proyecto(proyecto); else if (ejemplo) abrir_ejemplo(ejemplo);
	layout(); sincronizar_todo(); redibujar();
	log_posiciones(); log_estado("inicio");
	registrar("capibara-editor: pronto\n");

	evento_t e;
	while (!salir) {
		bool bloquear = juego_pid <= 0;
		if (juego_pid > 0 && vigilar_juego()) redibujar();
		if (proximo_evento(jn->id, &e, bloquear) != 1) { if (!bloquear) dormir(10); continue; }
		if (e.tipo == EV_FECHAR) {
			if (hay_cambios() && !confirmar(_("Capibara"), _("Hay cambios sin guardar. ¿Salir sin guardar?"))) continue;
			break;
		}
		if (jan_redimensionar(jn, &e)) { layout(); redibujar(); log_posiciones(); continue; }
		if (e.tipo == EV_TEMA) { redibujar(); continue; }

		registrar_alvos();
		int dd = arrsol_tratar(jn, &e);
		if (dd != ARRSOL_NADA) {
			if (dd == ARRSOL_FIM || dd == ARRSOL_CANCELOU) { dnd_vista = false; redibujar(); }
			continue;
		}

		bool cambio = false, tecla = e.tipo == EV_TECLA;
		/* menús */
		if ((e.tipo == EV_CLIQUE && (e.botoes & 1)) || e.tipo == EV_MENU) {
			int m = barra_menu_tratar(jn, &menu, &e);
			if (m >= 0) { int a = men_acc[m / 100][m % 100]; if (a) ejecutar_accion(a); redibujar(); continue; }
		}
		/* atajos */
		if (tecla) {
			bool ctrl = (e.modificadores & 2) != 0; unsigned k = e.tecla; int a = A_NADA;
			if (ctrl && (k == 19 || k == 's')) a = A_GUARDAR;
			else if (ctrl && (k == 26 || k == 'z') && foco != FO_CODIGO && foco != FO_INSPETOR) a = A_DESHACER;
			else if (ctrl && (k == 25 || k == 'y') && foco != FO_CODIGO && foco != FO_INSPETOR) a = A_REHACER;
			else if (ctrl && (k == 4 || k == 'd') && foco != FO_CODIGO) a = A_DUPLICAR;
			else if (ctrl && (k == 14 || k == 'n')) a = A_NUEVO_PROY;
			else if (ctrl && (k == 15 || k == 'o')) a = A_ABRIR_PROY;
			else if (ctrl && (k == 5 || k == 'e')) a = A_EXPORTAR;
			else if (ctrl && (k == 7 || k == 'g')) a = A_EDITAR_GUION;   /* Ctrl+G: edita el guion del nodo (o lo crea) */
			else if (k == TECLA_F1_ + 4) a = A_JUGAR;
			else if (k == TECLA_F1_ + 5) a = A_JUGAR_ESCENA;
			else if (k == TECLA_F1_ + 6) a = A_PARAR;
			else if (k == TECLA_F1_ + 1 && (foco == FO_ARBOL || foco == FO_VISTA)) a = A_RENOMBRAR;
			else if (k == TECLA_F1_ + 2) a = A_VARIABLE;
			else if (k == TECLA_F1_ + 3) a = A_SENAL;
			else if (k == TECLA_DELETE && (foco == FO_ARBOL || foco == FO_VISTA)) a = A_BORRAR;
			else if (k == TECLA_HOME && foco == FO_VISTA) a = A_RESTABLECER;
			if (a != A_NADA) { ejecutar_accion(a); redibujar(); continue; }
		}
		if (e.tipo == EV_CLIQUE && (e.botoes & 1)) {
			usar_foco(foco_en(e.x, e.y));
			if (foco == FO_ARBOL) {
				int fila = arbol_fila_en(e.y);
				if (fila >= 0 && e.x > arv.x + 22 && arv_sec[fila] != esc_buscar_ruta(doc, ".")) {
					arrsol_item_t it; memset(&it, 0, sizeof it);
					it.tipo = TIPO_NODO; it.dados[0] = (unsigned char)(arv_sec[fila] & 255); it.dados[1] = (unsigned char)(arv_sec[fila] >> 8); it.n = 2;
					snprintf(it.rotulo, sizeof it.rotulo, "%s", arv_nom[fila]);
					arrsol_preparar(&it, e.x, e.y);
				}
			} else if (foco == FO_RECURSOS) {
				int fila = (e.y - rec.y - 2) / 20 + rec.topo, vis = 0, item = -1;
				for (int i = 0; i < rec.n; i++) if (arvore_visivel(&rec, i)) { if (vis == fila) { item = i; break; } vis++; }
				if (item >= 0 && rec_niv[item] == 1 && e.x > rec.x + 22) {
					arrsol_item_t it; memset(&it, 0, sizeof it);
					it.tipo = TIPO_RECURSO; it.dados[0] = (unsigned char)(item & 255); it.dados[1] = (unsigned char)(item >> 8); it.n = 2;
					snprintf(it.rotulo, sizeof it.rotulo, "%s", rec_nom[item]);
					arrsol_preparar(&it, e.x, e.y);
				}
			}
		}
		/* paneles */
		int pr = paineis_tratar(jn, &pn, &e);
		if (pr == PAINEIS_ARRASTA || pr == PAINEIS_PLEGOU) { layout(); redibujar(); if (pr == PAINEIS_PLEGOU) log_posiciones(); continue; }
		if (pr == PAINEIS_SOLTOU) { layout(); redibujar(); log_posiciones(); continue; }
		if (e.tipo == EV_MOVER && pn.arrasto >= 0) continue;
		/* barra de botones */
		for (int i = 0; i < NBTN; ++i) {
			if (!mbotao_tratar(jn, &btn[i], &e) || btn[i].desabilitado) continue;
			switch (i) {
			case B_MOVER: modo = HM_MOVER; break; case B_GIRAR: modo = HM_GIRAR; break; case B_ESCALAR: modo = HM_ESCALAR; break;
			case B_PINTAR: modo = HM_PINTAR; break; case B_BORRAR: modo = HM_BORRAR; break;
			default: ejecutar_accion(accion_de_boton(i));
			}
			registrar("capibara-editor: boton %d modo %d\n", i, modo);
			cambio = true;
		}
		/* componentes (el teclado, solo al que tiene el foco) */
		if (!tecla || foco == FO_ARBOL) {
			int r = arvore_tratar(jn, &arv, &e);
			if (r == ARVORE_ESCOLHEU && arv.sel >= 0) {
				int s = arv_sec[arv.sel];
				sel_unica(doc->sec[s].ruta);
				cargar_inspetor(); sel_a_vista(); actualizar_paleta();
				registrar("capibara-editor: arbol elegido %s\n", doc->sec[s].ruta);
				cambio = true;
			}
		}
		if (!tecla || foco == FO_RECURSOS) {
			int r = arvore_tratar(jn, &rec, &e);
			if (e.tipo == EV_CLIQUE && e.cliques == 2 && (e.botoes & 1) && foco == FO_RECURSOS && rec.sel >= 0) { abrir_recurso(rec.sel); cambio = true; }
			else if (r == ARVORE_ESCOLHEU) cambio = true;
		}
		if (!tecla || foco == FO_VISTA) {
			if (pincel_evento(&e)) { if (pintando) edvista_desenhar(jn, &vs); else cambio = true; continue; }
			edvista_tratar(jn, &vs, &e);
		}
		if (!tecla || foco == FO_INSPETOR) {
			int r = inspetor_tratar(jn, &ins, &e);
			if (r == INSPETOR_MUDOU) cambio = true;
		}
		if (!tecla || foco == FO_PALETA) {
			int r = pal.img ? edpaleta_tratar(jn, &pal, &e) : EDPALETA_NADA;
			if (r == EDPALETA_ESCOLHEU) {
				int c0, f0, c1, f1; edpaleta_selecao(&pal, &c0, &f0, &c1, &f1);
				registrar("capibara-editor: casillas %d %d %d %d\n", c0, f0, c1, f1);
				cambio = true;
			}
		}
		if (!tecla || foco == FO_CONSOLA) {
			int r = consola_tratar(jn, &con, &e);
			if (r == CONSOLA_IR) {
				char arch[96] = "", msg[CONSOLA_TXT] = ""; int linea = con.ir_linha;
				for (int i = CONSOLA_MAX - 1; i >= 0; --i) {
					if (con.lin[i].linha != linea || !con.lin[i].texto[0]) continue;
					const char *resto = NULL;
					if (ruta_y_linea(con.lin[i].texto, arch, sizeof arch, &resto) == linea) { if (resto) snprintf(msg, sizeof msg, "%s", resto); break; }
					arch[0] = 0;
				}
				if (!arch[0] && cod_arch[0]) snprintf(arch, sizeof arch, "%s", cod_arch);
				if (arch[0]) { if (strcmp(arch, cod_arch)) abrir_guion(arch); codigo_ir_linha(cod, linea); if (msg[0]) codigo_marcar_erro(cod, linea, msg); usar_foco(FO_CODIGO); }
				registrar("capibara-editor: ir a %s linea %d\n", arch, linea);
				cambio = true;
			}
		}
		if (!tecla || foco == FO_CODIGO) {
			int r = codigo_tratar(jn, cod, &e);
			if (r == CODIGO_MUDOU) cambio = true;
		}
		if (cambio || (e.tipo != EV_MOVER && e.tipo != EV_SOLTOU && e.tipo != EV_RODA)) { layout(); redibujar(); }
	}
	parar_juego();
	codigo_liberar(cod);
	ed_desfazer_liberar(&pila);
	jan_fechar(jn);
	return 0;
}
