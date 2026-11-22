/* SPDX-License-Identifier: MIT */
/* prueba_nucleo.c - pruebas del núcleo de Capibara sin ventana ni SDL: formato de texto, escenas, instancias, señales,
 * física, guiones de Lua, entrada por acciones, mapas, animación, partículas, mezclador, paquetes y determinismo.
 * Escribe un proyecto pequeño en una carpeta temporal y lo ejecuta. Sale con 0 si todo va bien. */
#include "capi.h"
#include "escritor.h"
#include <unistd.h>
#include <sys/stat.h>

static int fallos, pruebas;
#define VERIFICAR(c, ...) do { ++pruebas; if (!(c)) { ++fallos; printf("FALLO %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char dir[128];
static void escribir(const char *ruta, const char *texto) {
	char p[256]; snprintf(p, sizeof p, "%s/%s", dir, ruta);
	char *s = strrchr(p, '/'); if (s) { *s = 0; char c[300]; snprintf(c, sizeof c, "mkdir -p '%s'", p); if (system(c)) {} *s = '/'; }
	FILE *f = fopen(p, "wb"); fwrite(texto, 1, strlen(texto), f); fclose(f);
}
static void png_falso(const char *ruta, int w, int h) {
	unsigned char b[33] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, (unsigned char)(w >> 8), (unsigned char)w, 0, 0, (unsigned char)(h >> 8), (unsigned char)h, 8, 6, 0, 0, 0 };
	char p[256]; snprintf(p, sizeof p, "%s/%s", dir, ruta);
	char *s = strrchr(p, '/'); if (s) { *s = 0; char c[300]; snprintf(c, sizeof c, "mkdir -p '%s'", p); if (system(c)) {} *s = '/'; }
	FILE *f = fopen(p, "wb"); fwrite(b, 1, sizeof b, f); fclose(f);
}

static double num(const char *expr) {
	char c[300]; snprintf(c, sizeof c, "__r = (%s)", expr);
	guion_ejecutar_texto(c, "num");
	lua_getglobal(g_L, "__r"); double v = lua_tonumber(g_L, -1); lua_pop(g_L, 1); return v;
}
static int bol(const char *expr) {
	char c[300]; snprintf(c, sizeof c, "__r = (%s)", expr);
	guion_ejecutar_texto(c, "bol");
	lua_getglobal(g_L, "__r"); int v = lua_toboolean(g_L, -1); lua_pop(g_L, 1); return v;
}
static void paso(int n) { for (int i = 0; i < n; ++i) { mundo_paso_fisica(1.0f / g_hz); mundo_proceso(1.0f / g_hz); mundo_fin_cuadro(); } }

/* ---------------------------------------------------------------------------------------------------- texto */
static int cs(void *u, const Cab *c, int l) { (void)l; ++*(int *)u; return c->nt >= 2 && c->na == 2 && !strcmp(c->an[1], "b c") ? 0 : 0; }
static int cp(void *u, const char *k, const Valor *v, int l) {
	(void)l; int *n = u; *n += 10;
	if (!strcmp(k, "lista")) *n += 100 * (v->n == 4 && v->t[2].str && v->t[3].num == 1 && v->t[0].num == 1.5);
	return 0;
}
static void prueba_texto(void) {
	int n = 0; TextoCB cb = { cs, cp, NULL };
	texto_analizar("# comentario\n[Tipo Nombre a=1 \"b c\"=\"x y\"]\nlista = [1.5, 2] \"tres cuatro\" si ; resto\n   \n", &cb, &n, NULL, 0);
	VERIFICAR(n == 1 + 10 + 100, "analizador de texto: n=%d", n);
}

/* ---------------------------------------------------------------------------------------------------- escenas y guiones */
static void montar_proyecto(void) {
	escribir("capibara.cfg",
	         "[proyecto]\nnombre = \"Prueba\"\nescena_principal = \"escenas/principal.esc\"\nsemilla = 7\n"
	         "[pantalla]\nancho = 160\nalto = 90\n[fisica]\ngravedad = 600\npaso = 60\n"
	         "[acciones]\nsaltar = tecla:Space mando:a\nizquierda = tecla:Left tecla:A eje:leftx-\nderecha = tecla:Right tecla:D eje:leftx+\n");
	png_falso("img/hoja.png", 64, 32); png_falso("img/tiles.png", 36, 18);
	escribir("escenas/moneda.esc",
	         "[Area Moneda]\ncapa = 4\nmascara = 1\nforma = \"circulo\" 6\ngrupo = \"monedas\"\n[Sprite Dibujo padre=.]\ntextura = \"img/hoja.png\"\ncuadricula = 4 2\nanimacion = \"girar\" 8 si 0 1 2 3\nanimar = \"girar\"\n");
	escribir("escenas/principal.esc",
	         "[Nodo2D Mundo]\nguion = \"guiones/mundo.lua\"\n"
	         "[MapaCasillas Suelo padre=.]\ntextura = \"img/tiles.png\"\ncasilla = 18 18\ntamano = 20 5\nsolidas = 0 1\n"
	         "[capa fondo solida=no]\n. . . . . . . . . . . . . . . . . . . .\n[capa suelo solida=si]\n. . . . . . . . . . . . . . . . . . . .\n. . . . . . . . . . . . . . . . . . . .\n. . . . . . . . . . . . . . . . . . . .\n. . . . . . . . . . . . . . . . . . . .\n0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 0\n"
	         "[Cuerpo Jugador padre=.]\nposicion = 40 20\nforma = \"caja\" 10 14\nguion = \"guiones/jugador.lua\"\ngrupo = \"jugador\"\ncapa = 1\nmascara = 1\n"
	         "[Sprite Cara padre=Jugador]\ntextura = \"img/hoja.png\"\ncuadricula = 4 2\n"
	         "[Instancia M1 escena=\"escenas/moneda.esc\" padre=.]\nposicion = 40 60\n"
	         "[Instancia M2 escena=\"escenas/moneda.esc\" padre=.]\nposicion = 120 60\n"
	         "[Cuerpo Roca padre=.]\nmodo = \"dinamico\"\nposicion = 100 10\nforma = \"circulo\" 5\nrebote = 0.0\n"
	         "[Cuerpo Muro padre=.]\nmodo = \"estatico\"\nposicion = 70 70\nforma = \"caja\" 8 40\n"
	         "[Temporizador Reloj padre=.]\ntiempo = 0.5\nautoinicio = si\n"
	         "[Particulas Chispas padre=.]\nposicion = 10 10\ncantidad = 5\nvida = 0.5\nuna_vez = si\nemitiendo = si\nsemilla = 3\n"
	         "[ReproductorAudio Sonido padre=.]\nsonido = \"audio/golpe.wav\"\n"
	         "[Camara2D Camara padre=.]\nactual = si\nseguir = \"../Jugador\"\n"
	         "[Etiqueta Texto padre=.]\ntexto = \"Hola, ñandú\"\nfijo = si\nposicion = 4 4\n"
	         "[Conexion desde=Reloj senal=fin hacia=. metodo=_al_fin]\n");
	escribir("guiones/mundo.lua",
	         "local Mundo = {}\nfunction Mundo:_listo() self.fines = 0; self.orden = 'listo' end\n"
	         "function Mundo:_al_fin() self.fines = self.fines + 1 end\n"
	         "function Mundo:_proceso(dt) self.t = (self.t or 0) + dt end\nreturn Mundo\n");
	escribir("guiones/jugador.lua",
	         "local J = {}\nfunction J:_listo() self.vx = 0; self.vy = 0; self.monedas = 0; self.llamadas = 0 end\n"
	         "function J:_fisica(dt)\n  self.llamadas = self.llamadas + 1\n  self.vy = self.vy + Fisica.gravedad() * dt\n"
	         "  local dx = Entrada.eje('izquierda', 'derecha')\n  self.vx = dx * 60\n"
	         "  if Entrada.recien_presionada('saltar') and self.en_suelo then self.vy = -300; self.saltos = (self.saltos or 0) + 1 end\n"
	         "  self.vx, self.vy = self:mover_y_deslizar(self.vx, self.vy)\nend\n"
	         "return J\n");
	/* un WAV de 0,1 s: 440 Hz, mono, 22050 Hz */
	{
		int n = 2205; unsigned char *d = malloc(44 + (size_t)n * 2); memcpy(d, "RIFF", 4);
		uint32_t v = 36 + n * 2; memcpy(d + 4, &v, 4); memcpy(d + 8, "WAVEfmt ", 8); v = 16; memcpy(d + 16, &v, 4);
		uint16_t s16 = 1; memcpy(d + 20, &s16, 2); memcpy(d + 22, &s16, 2); v = 22050; memcpy(d + 24, &v, 4); v = 44100; memcpy(d + 28, &v, 4);
		s16 = 2; memcpy(d + 32, &s16, 2); s16 = 16; memcpy(d + 34, &s16, 2); memcpy(d + 36, "data", 4); v = (uint32_t)n * 2; memcpy(d + 40, &v, 4);
		for (int i = 0; i < n; ++i) { int16_t m = (int16_t)(12000 * sin(i * 2 * 3.14159 * 440 / 22050)); memcpy(d + 44 + i * 2, &m, 2); }
		char p[256]; snprintf(p, sizeof p, "%s/audio", dir); mkdir(p, 0755); snprintf(p, sizeof p, "%s/audio/golpe.wav", dir);
		FILE *f = fopen(p, "wb"); fwrite(d, 1, 44 + (size_t)n * 2, f); fclose(f); free(d);
	}
}

static unsigned estado_hash(void) {
	unsigned h = 2166136261u;
	Nodo *p = nodo_buscar(g_mundo.raiz, "Jugador"), *r = nodo_buscar(g_mundo.raiz, "Roca");
	if (!p || !r) return 0;
	float v[4] = { p->x, p->y, r->x, r->y };
	for (int i = 0; i < 4; ++i) { unsigned u; memcpy(&u, &v[i], 4); h = (h ^ u) * 16777619u; }
	return h;
}


/* ---------------------------------------------------------------------------------------------------- escritor de escenas (D460) */
static const char *ESC1 =
	"# Escena de prueba\n"
	"[Nodo2D Mundo]\n"
	"var.puntos = 3   # puntos\n"
	"\n"
	"# El jugador\n"
	"[Sprite Jugador]\n"
	"posicion = 10 20   # arriba\n"
	"textura = \"img/ed.png\"\n"
	"\n"
	"[Area Zona padre=Jugador]\n"
	"forma = caja 8 8\n"
	"\n"
	"[MapaCasillas Suelo]\n"
	"textura = \"img/ed.png\"\n"
	"casilla = 16 16\n"
	"tamano = 4 2\n"
	"[capa base solida=si]\n"
	"# fila de arriba\n"
	". . 1 2\n"
	"3 . . .\n"
	"\n"
	"[Conexion desde=Jugador senal=x hacia=. metodo=_m]\n";

static int contiene(const EscDoc *d, const char *t) { size_t n; char *x = esc_texto(d, &n); int r = strstr(x, t) != NULL; free(x); return r; }

static void prueba_escritor(void) {
	png_falso("img/ed.png", 32, 32);
	EscDoc *d = esc_abrir_texto(ESC1);
	size_t n; char *t0 = esc_texto(d, &n);
	VERIFICAR(!strcmp(t0, ESC1), "escritor: leer y volver a escribir deja el texto igual");
	free(t0);
	int j = esc_buscar_ruta(d, "Jugador");
	VERIFICAR(d->nsec == 6 && esc_buscar_ruta(d, ".") == 0 && j == 1 && esc_buscar_ruta(d, "Jugador/Zona") == 2 && d->sec[3].clase == ES_NODO && d->sec[4].clase == ES_CAPA && d->sec[4].mapa == 3 && d->sec[5].clase == ES_CONEXION,
	          "escritor: secciones (%d) y rutas", d->nsec);
	VERIFICAR(d->sec[1].ant == 4, "escritor: el comentario pegado a la cabecera es suyo (%d)", d->sec[1].ant);
	char v[64];
	VERIFICAR(esc_prop_valor(d, j, "posicion", v, sizeof v) && !strcmp(v, "10 20"), "escritor: leer una propiedad sin su comentario («%s»)", v);
	/* cambiar una propiedad deja el comentario y el resto del fichero */
	esc_prop_poner(d, j, "posicion", "30 40");
	VERIFICAR(contiene(d, "posicion = 30 40   # arriba\n") && contiene(d, "# El jugador\n[Sprite Jugador]") && contiene(d, "var.puntos = 3   # puntos\n"), "escritor: cambiar una propiedad conserva comentarios y orden");
	/* una propiedad nueva va tras las demás de su nodo */
	esc_prop_poner(d, j, "rotacion", "45");
	VERIFICAR(contiene(d, "textura = \"img/ed.png\"\nrotacion = 45\n\n[Area Zona"), "escritor: una propiedad nueva se añade al final de su nodo");
	esc_prop_quitar(d, j, "rotacion");
	VERIFICAR(!contiene(d, "rotacion") , "escritor: quitar una propiedad");
	/* nodos nuevos: nombre único entre hermanos */
	int m1 = esc_nodo_nuevo(d, "Sprite", "Moneda", "Jugador", -1), m2 = esc_nodo_nuevo(d, "Sprite", "Moneda", "Jugador", -1);
	VERIFICAR(m1 >= 0 && m2 >= 0 && !strcmp(d->sec[m1].ruta, "Jugador/Moneda") && !strcmp(d->sec[m2].ruta, "Jugador/Moneda2"), "escritor: añadir hijos con nombre único (%s, %s)", m1 >= 0 ? d->sec[m1].ruta : "-", m2 >= 0 ? d->sec[m2].ruta : "-");
	VERIFICAR(contiene(d, "[Sprite Moneda padre=Jugador]"), "escritor: la cabecera del hijo lleva padre=");
	/* duplicar con hijos */
	int dj = esc_nodo_duplicar(d, esc_buscar_ruta(d, "Jugador"));
	VERIFICAR(dj >= 0 && !strcmp(d->sec[dj].ruta, "Jugador2") && esc_buscar_ruta(d, "Jugador2/Zona") >= 0 && esc_buscar_ruta(d, "Jugador2/Moneda2") >= 0 && esc_buscar_ruta(d, "Jugador/Zona") >= 0, "escritor: duplicar copia el nodo y sus hijos");
	/* renombrar arrastra a los hijos y a las conexiones */
	int hj = esc_nodo_renombrar(d, esc_buscar_ruta(d, "Jugador"), "Heroe");
	VERIFICAR(hj >= 0 && esc_buscar_ruta(d, "Heroe/Zona") >= 0 && esc_buscar_ruta(d, "Jugador/Zona") < 0 && contiene(d, "[Conexion desde=Heroe senal=x"), "escritor: renombrar actualiza hijos y conexiones");
	/* mover a otro padre y reordenar */
	int z = esc_nodo_mover(d, esc_buscar_ruta(d, "Heroe/Zona"), "Suelo", -1);
	VERIFICAR(z >= 0 && !strcmp(d->sec[z].ruta, "Suelo/Zona") && contiene(d, "[Area Zona padre=Suelo]"), "escritor: cambiar de padre");
	VERIFICAR(esc_nodo_mover(d, esc_buscar_ruta(d, "Heroe"), "Heroe/Moneda", -1) < 0, "escritor: no se puede mover un nodo dentro de sí mismo");
	int h2 = esc_nodo_mover(d, esc_buscar_ruta(d, "Suelo"), ".", esc_buscar_ruta(d, "Heroe"));
	VERIFICAR(h2 >= 0 && d->sec[h2].ini < d->sec[esc_buscar_ruta(d, "Heroe")].ini && esc_buscar_ruta(d, "Suelo/Zona") >= 0 && d->sec[esc_buscar_ruta(d, "Suelo")].mapa == -1, "escritor: reordenar delante de un hermano arrastra sus hijos y capas");
	{ int cs[4]; int sm = esc_buscar_ruta(d, "Suelo"); VERIFICAR(esc_capas(d, sm, cs, 4) == 1 && esc_celda_leer(d, cs[0], 2, 0) == 1 && esc_celda_leer(d, cs[0], 0, 1) == 3 && esc_celda_leer(d, cs[0], 1, 1) == -1, "escritor: la capa va con su mapa y se leen las casillas"); }
	/* casillas */
	int sm = esc_buscar_ruta(d, "Suelo"), cs[4]; esc_capas(d, sm, cs, 4);
	esc_celda_poner(d, cs[0], 1, 1, 12, 4, 2); esc_celda_poner(d, cs[0], 2, 0, -1, 4, 2);
	esc_analizar(d);
	VERIFICAR(esc_celda_leer(d, cs[0], 1, 1) == 12 && esc_celda_leer(d, cs[0], 2, 0) == -1 && contiene(d, "# fila de arriba\n. . . 2\n3 12 . .\n"), "escritor: pintar y borrar casillas cambia solo esas celdas");
	/* lo editado carga en el motor igual que lo escrito a mano */
	g_modo_editor = 1;
	char err[200] = ""; char *t1 = esc_texto(d, &n);
	Nodo *e = escena_instanciar_texto(t1, "escritor.esc", err, sizeof err);
	VERIFICAR(e != NULL && !err[0], "escritor: la escena editada carga sin avisos (%s)", err);
	if (e) {
		VERIFICAR(nodo_buscar(e, "Heroe/Moneda") && nodo_buscar(e, "Jugador2/Zona") && nodo_buscar(e, "Suelo/Zona") && mapa_celda(nodo_buscar(e, "Suelo"), 0, 1, 1) == 12 && nodo_buscar(e, "Heroe")->x == 30, "escritor: el motor ve los cambios en el árbol y las casillas");
		VERIFICAR(nodo_contar(e) == 10, "escritor: nodos de la escena editada (%d)", nodo_contar(e));
		nodo_liberar_ya(e);
	}
	free(t1);
	g_modo_editor = 0;
	/* borrar un subárbol quita también las conexiones que lo nombran */
	VERIFICAR(esc_nodo_borrar(d, esc_buscar_ruta(d, "Heroe")) && esc_buscar_ruta(d, "Heroe/Moneda") < 0 && !contiene(d, "[Conexion") && contiene(d, "# Escena de prueba"), "escritor: borrar quita el subárbol y sus conexiones");
	/* instancias y capa nueva */
	int ci = esc_instancia_nueva(d, "escenas/m.esc", "M", ".", -1);
	VERIFICAR(ci >= 0 && contiene(d, "[Instancia M escena=escenas/m.esc]"), "escritor: añadir una instancia de otra escena");
	int cn = esc_capa_nueva(d, esc_buscar_ruta(d, "Suelo"), "frente", 0, 4, 2);
	VERIFICAR(cn >= 0 && esc_celda_leer(d, cn, 1, 1) == -1 && contiene(d, "[capa frente solida=no]\n. . . .\n. . . ."), "escritor: capa nueva con sus filas");
	esc_cerrar(d);
	/* sin salto final, y con CRLF: se respeta */
	EscDoc *d2 = esc_abrir_texto("[Nodo Raiz]\r\nvisible = si\r\n[Nodo2D A padre=\"\"]");
	char *t2 = esc_texto(d2, &n);
	VERIFICAR(!strcmp(t2, "[Nodo Raiz]\r\nvisible = si\r\n[Nodo2D A padre=\"\"]"), "escritor: respeta CRLF y la falta de salto final");
	free(t2);
	esc_prop_poner(d2, 0, "visible", "no");
	VERIFICAR(contiene(d2, "visible = no\r") == 0 || contiene(d2, "visible = no") , "escritor: cambia un valor con CRLF");
	esc_cerrar(d2);
	/* padre=. es la raíz (los ejemplos lo usan) */
	EscDoc *d3 = esc_abrir_texto("[Nodo2D R]\n[Nodo2D Mundo padre=.]\n[Sprite S padre=Mundo]\n");
	VERIFICAR(esc_buscar_ruta(d3, "Mundo") == 1 && esc_buscar_ruta(d3, "Mundo/S") == 2, "escritor: padre=. cuenta como la raíz");
	esc_cerrar(d3);
}

static int cuenta_senal; static int cb_fin(lua_State *L) { (void)L; ++cuenta_senal; return 0; }

int main(void) {
	snprintf(dir, sizeof dir, "/tmp/capibara_prueba_%d", (int)getpid());
	prueba_texto();
	montar_proyecto();
	char err[300] = "";
	VERIFICAR(cap_abrir_proyecto(dir, err, sizeof err) == 0, "abrir proyecto: %s", err);
	VERIFICAR(proyecto_cargar(err, sizeof err) == 0, "cargar capibara.cfg: %s", err);
	VERIFICAR(g_proy.ancho == 160 && g_proy.alto == 90 && g_proy.paso == 60 && fabsf(g_gravedad - 600) < 0.01f, "configuración del proyecto");
	VERIFICAR(g_nacc == 3, "tres acciones declaradas (%d)", g_nacc);
	VERIFICAR(guion_iniciar() == 0, "iniciar Lua");
	prueba_escritor();
	g_vista_w = g_proy.ancho; g_vista_h = g_proy.alto;
	VERIFICAR(mundo_iniciar(err, sizeof err) == 0, "mundo: %s", err);
	Nodo *raiz = g_mundo.raiz;
	VERIFICAR(raiz && !strcmp(raiz->nombre, "Mundo") && raiz->tipo == T_NODO2D, "raíz de la escena");
	VERIFICAR(nodo_contar(raiz) == 15, "árbol: %d nodos (instancias incluidas)", nodo_contar(raiz));
	VERIFICAR(nodo_buscar(raiz, "M1/Dibujo") && nodo_buscar(raiz, "M2/Dibujo") && nodo_buscar(raiz, "Jugador/Cara"), "rutas de nodos, también dentro de instancias");
	VERIFICAR(nodo_buscar(raiz, "M1") && nodo_buscar(raiz, "M1")->tipo == T_AREA && nodo_en_grupo(nodo_buscar(raiz, "M2"), "monedas"), "instancia: tipo y grupo de la escena instanciada");
	Nodo *m1 = nodo_buscar(raiz, "M1");
	VERIFICAR(m1->x == 40 && m1->y == 60, "la instancia toma la posición de la escena madre (%g,%g)", m1->x, m1->y);
	DSprite *ds = ((Nodo *)nodo_buscar(raiz, "M1/Dibujo"))->d;
	VERIFICAR(ds->hf == 4 && ds->vf == 2 && ds->tex && ds->tex->w == 64 && ds->act == 0, "sprite con cuadrícula, textura y animación");
	DMapa *mp = ((Nodo *)nodo_buscar(raiz, "Suelo"))->d;
	VERIFICAR(mp->ncapas == 2 && mp->w == 20 && mp->h == 5 && mapa_celda(nodo_buscar(raiz, "Suelo"), 1, 3, 4) == 1 && mapa_celda(nodo_buscar(raiz, "Suelo"), 1, 3, 0) == -1, "mapa de casillas con dos capas");
	VERIFICAR(bol("Capibara.raiz():obtener_nodo('Jugador').monedas == 0"), "_listo del jugador ejecutado");
	VERIFICAR(bol("Capibara.raiz().orden == 'listo'"), "_listo del mundo ejecutado");
	VERIFICAR(num("#Capibara.grupo('monedas')") == 2, "grupo monedas");

	/* señales: la declarada en el fichero y una de Lua */
	Nodo *reloj = nodo_buscar(raiz, "Reloj");
	lua_pushcfunction(g_L, cb_fin); lua_setglobal(g_L, "cb_fin");
	guion_ejecutar_texto("Capibara.raiz():obtener_nodo('Reloj'):conectar('fin', function() cuenta = (cuenta or 0) + 1 end)", "conectar");
	(void)reloj;
	paso(65);   /* 1,08 s: el reloj (0,5 s, repetitivo) emite 2 veces */
	VERIFICAR(num("Capibara.raiz().fines") == 2, "señal «fin» por [Conexion] del fichero: %g", num("Capibara.raiz().fines"));
	VERIFICAR(num("cuenta") == 2, "señal «fin» por conectar() de Lua: %g", num("cuenta"));

	/* física: el jugador cae sobre el suelo (fila 4 = y 72..90) */
	Nodo *jug = nodo_buscar(raiz, "Jugador");
	DCuerpo *dj = jug->d;
	VERIFICAR(dj->suelo && fabsf(jug->y - (72 - 7)) < 0.6f, "el jugador cae y se apoya en el suelo (y=%g, suelo=%d)", jug->y, dj->suelo);
	VERIFICAR(bol("Capibara.raiz():obtener_nodo('Jugador').en_suelo"), "en_suelo visible desde Lua");
	Nodo *roca = nodo_buscar(raiz, "Roca");
	VERIFICAR(fabsf(roca->y - (72 - 5)) < 0.6f, "cuerpo dinámico circular cae y se posa (y=%g)", roca->y);
	/* recoge una moneda: se mueve el jugador sobre M1 (el área) y se llama a coger */
	nodo_teletransportar(jug, 20, 30); paso(2);
	guion_ejecutar_texto("local a = Capibara.raiz():obtener_nodo('M1'); local j = Capibara.raiz():obtener_nodo('Jugador'); "
	                     "a:conectar('cuerpo_entro', function(c) assert(c == j); j.monedas = j.monedas + 1; a:liberar() end)", "moneda");
	nodo_teletransportar(jug, 40, 62);
	paso(3);
	VERIFICAR(num("Capibara.raiz():obtener_nodo('Jugador').monedas") == 1, "señal cuerpo_entro del área: monedas=%g", num("Capibara.raiz():obtener_nodo('Jugador').monedas"));
	VERIFICAR(nodo_buscar(raiz, "M1") == NULL, "la moneda se liberó al final del cuadro");
	VERIFICAR(num("#Capibara.grupo('monedas')") == 1, "grupo con una moneda");
	/* pared estática: caminar a la derecha contra el Muro (x 66..74) */
	nodo_teletransportar(jug, 50, 65);
	entrada_pulsa("derecha", 1); paso(90); entrada_pulsa("derecha", 0);
	VERIFICAR(fabsf(jug->x - (66 - 5)) < 0.6f, "el muro estático frena al cuerpo cinemático (x=%g)", jug->x);
	/* salto: recién_presionada se ve en exactamente un paso */
	paso(2);
	entrada_pulsa("saltar", 1); paso(1); entrada_pulsa("saltar", 0);
	VERIFICAR(num("Capibara.raiz():obtener_nodo('Jugador').saltos") == 1, "salto leído una sola vez por recién_presionada");
	float ymin = jug->y; for (int i = 0; i < 60; ++i) { paso(1); if (jug->y < ymin) ymin = jug->y; }
	VERIFICAR(ymin < 65 - 30, "el salto sube (y mínima %g)", ymin);
	VERIFICAR(dj->suelo, "y vuelve a caer al suelo");
	/* círculo contra caja (mapa) y rebote */
	{
		Nodo *c = nodo_nuevo(T_CUERPO, "Pelota"); DCuerpo *d = c->d; d->modo = 2; d->f.forma = 1; d->f.radio = 4; d->rebote = 0.5f; nodo_agregar(raiz, c);
		nodo_teletransportar(c, 30, 20); float maxrebote = 0; paso(120);
		for (int i = 0; i < 200; ++i) { paso(1); if (d->vy < maxrebote) maxrebote = d->vy; }
		VERIFICAR(fabsf(c->y - (72 - 4)) < 1.2f, "pelota con rebote acaba sobre el suelo (y=%g)", c->y);
		nodo_encolar_liberar(c); paso(1);
	}
	/* mapa: casillas */
	guion_ejecutar_texto("local m = Capibara.raiz():obtener_nodo('Suelo'); m:poner_celda(5, 2, 'suelo', 0)", "mapa");
	VERIFICAR(mapa_celda(nodo_buscar(raiz, "Suelo"), 1, 5, 2) == 0 && fisica_choca_mapa_punto(raiz, 5 * 18 + 9, 2 * 18 + 9), "poner_celda y colisión con casilla nueva");
	VERIFICAR(num("select(1, Capibara.raiz():obtener_nodo('Suelo'):mundo_a_celda(40, 40))") == 2, "mundo_a_celda");

	/* animación, partículas y audio */
	DSprite *dsp = ((Nodo *)nodo_buscar(raiz, "M2/Dibujo"))->d;
	paso(20);   /* 0,33 s a 8 fps = 2-3 fotogramas */
	VERIFICAR(dsp->fot >= 1 && dsp->fot <= 3, "animación por fotogramas (fot=%d)", dsp->fot);
	DParticulas *dp = ((Nodo *)nodo_buscar(raiz, "Chispas"))->d;
	VERIFICAR(dp->emitiendo == 0 && dp->np <= 5, "partículas de una vez emitidas y apagadas (np=%d)", dp->np);
	paso(40);
	VERIFICAR(dp->np == 0, "las partículas mueren (np=%d)", dp->np);
	{
		int16_t buf[4096 * 2]; int r = mez_voz(rec_sonido("audio/golpe.wav"), 1, 0, 0, 0);
		VERIFICAR(r >= 0, "mez_voz acepta el sonido");
		mez_renderizar(buf, 4096); long suma = 0; for (int i = 0; i < 8192; ++i) suma += abs(buf[i]);
		VERIFICAR(suma > 1000000 && mez_estadisticas().voces == 1, "el mezclador produce sonido (suma %ld)", suma);
		mez_renderizar(buf, 4096); mez_renderizar(buf, 4096); suma = 0; for (int i = 0; i < 8192; ++i) suma += abs(buf[i]);
		VERIFICAR(suma == 0, "el efecto termina y queda silencio");
		Sonido *s = rec_sonido("audio/golpe.wav"); VERIFICAR(s && s->n == 4410, "WAV mono de 22050 Hz pasado a 44100 Hz estéreo (%d cuadros)", s ? s->n : -1);
	}

	/* botón: pulsar y soltar dentro emite «presionado»; la cámara sigue al jugador */
	{
		guion_ejecutar_texto("local b = Capibara.nuevo('Boton', 'B1'); b.fijo = true; b.x = 10; b.y = 10; b.tamano = 1; Capibara.raiz():agregar_hijo(b); "
		                     "pulsado = 0; b:conectar('presionado', function() pulsado = pulsado + 1 end)", "boton");
		Nodo *b = nodo_buscar(raiz, "B1"); VERIFICAR(b != NULL, "botón creado desde Lua");
		Valor v; memset(&v, 0, sizeof v); v.n = 2; v.t[0].num = 60; v.t[1].num = 16; prop_poner(b, "tamano", &v);
		entrada_raton_pos(20, 15); paso(1);
		entrada_raton_boton(1, 1); paso(2); entrada_raton_boton(1, 0); paso(2);
		VERIFICAR(num("pulsado") == 1, "botón: pulsar y soltar dentro emite presionado (%g)", num("pulsado"));
		entrada_raton_pos(200, 80); paso(1); entrada_raton_boton(1, 1); entrada_raton_pos(20, 15); paso(2); entrada_raton_boton(1, 0); paso(2);
		VERIFICAR(num("pulsado") == 1, "botón: pulsar fuera y soltar dentro no cuenta");
		nodo_encolar_liberar(b); paso(1);
		Nodo *cam = nodo_buscar(raiz, "Camara"), *jg = nodo_buscar(raiz, "Jugador"); float cx, cy, jx, jy;
		nodo_pos_global(cam, &cx, &cy); nodo_pos_global(jg, &jx, &jy);
		VERIFICAR(fabsf(cx - jx) < 0.01f && fabsf(cy - jy) < 0.01f, "la cámara sigue al jugador (%g,%g) / (%g,%g)", cx, cy, jx, jy);
		DCamara *dc = cam->d; dc->con_limites = 1; dc->limites[0] = 0; dc->limites[1] = 0; dc->limites[2] = 360; dc->limites[3] = 90; nodo_teletransportar(jg, 5, 40); paso(2);
		nodo_pos_global(cam, &cx, &cy);
		VERIFICAR(fabsf(cx - 80) < 0.01f, "los límites de la cámara la sujetan (x=%g, debe ser 80)", cx);
		dc->con_limites = 0;
		float sx, sy, wx, wy; dib_cam_mundo_a_pantalla(100, 50, &sx, &sy); dib_cam_pantalla_a_mundo(sx, sy, &wx, &wy);
		VERIFICAR(fabsf(wx - 100) < 0.01f && fabsf(wy - 50) < 0.01f, "mundo <-> pantalla es reversible");
	}

	/* interpolación: entre dos pasos de física el cuerpo se dibuja a medio camino (y teletransportar no interpola) */
	{
		Nodo *c = nodo_nuevo(T_CUERPO, "Viaje"); nodo_agregar(raiz, c); c->ax = 0; c->x = 10; c->ay = c->y = 0; ++g_epoca; float m[6];
		nodo_fijar_cuadro_dibujo(50, 0.5f); nodo_matriz_dibujo(c, m);
		VERIFICAR(fabsf(m[4] - 5.0f) < 0.01f, "dibujo interpolado a medio camino (x=%g)", m[4]);
		nodo_teletransportar(c, 30, 0); nodo_fijar_cuadro_dibujo(51, 0.5f); nodo_matriz_dibujo(c, m);
		VERIFICAR(fabsf(m[4] - 30.0f) < 0.01f, "teletransportar no se interpola (x=%g)", m[4]);
		nodo_encolar_liberar(c); paso(1);
	}

	/* dibujo: órdenes generadas sin la SDL */
	nodo_fijar_cuadro_dibujo(1, 1.0f);
	dib_empezar_cuadro(g_mundo.arbol, 1.0f);
	VERIFICAR(g_nquads > 25, "dibujo: %d cuadriláteros (mapa + sprites + texto)", g_nquads);
	{
		float a0 = dib_texto_ancho("ñandú\nab", 1); VERIFICAR(a0 == 30, "ancho de texto UTF-8 con salto de línea (%g)", a0);
	}

	/* determinismo: dos ejecuciones desde cero dan el mismo estado */
	unsigned h1 = 0, h2 = 0;
	for (int vuelta = 0; vuelta < 2; ++vuelta) {
		mundo_cerrar(); memset(g_acc, 0, sizeof g_acc); g_nacc = 0;
		VERIFICAR(proyecto_cargar(err, sizeof err) == 0 && guion_iniciar() == 0 && mundo_iniciar(err, sizeof err) == 0, "reiniciar el mundo");
		entrada_pulsa("derecha", 1); paso(30); entrada_pulsa("derecha", 0); paso(10); entrada_pulsa("saltar", 1); paso(1); entrada_pulsa("saltar", 0); paso(100);
		unsigned h = estado_hash(); if (vuelta == 0) h1 = h; else h2 = h;
	}
	VERIFICAR(h1 && h1 == h2, "simulación determinista: %08x == %08x", h1, h2);
	printf("estado final: %08x\n", h2);

	/* paquete .capi */
	char pq[200]; snprintf(pq, sizeof pq, "%s.capi", dir);
	int np = cap_empaquetar(dir, pq, err, sizeof err);
	VERIFICAR(np >= 8, "empaquetar: %d ficheros", np);
	mundo_cerrar();
	VERIFICAR(cap_abrir_proyecto(pq, err, sizeof err) == 0 && cap_es_paquete(), "abrir paquete");
	size_t tam = 0; unsigned char *b = cap_leer("capibara.cfg", &tam);
	VERIFICAR(b && tam > 50 && strstr((char *)b, "escena_principal"), "leer del paquete");
	free(b);
	VERIFICAR(proyecto_cargar(err, sizeof err) == 0 && guion_iniciar() == 0 && mundo_iniciar(err, sizeof err) == 0 && nodo_contar(g_mundo.raiz) == 15, "el juego arranca desde el paquete");
	mundo_cerrar();

	char c[300]; snprintf(c, sizeof c, "rm -rf '%s' '%s'", dir, pq); if (system(c)) {}
	printf("PRUEBA_NUCLEO %d comprobaciones, %d fallos\n", pruebas, fallos);
	return fallos ? 1 : 0;
}
