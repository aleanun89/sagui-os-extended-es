/* SPDX-License-Identifier: MIT */
/* mundo.c - el estado del juego y lo que pasa en cada paso: física a paso fijo, _proceso, animaciones, partículas,
 * temporizadores, botones, cámara y la cola de nodos por liberar. No usa la SDL. */
#include "capi.h"

Mundo g_mundo; int g_hz = 60;
static char pend_escena[CAPI_RUTA_MAX];

int escena_cambiar(const char *ruta) { snprintf(pend_escena, sizeof pend_escena, "%s", ruta); return 0; }

/* ---------------------------------------------------------------------------------------------------- animación de sprites */
void sprite_animar(Nodo *n, const char *nome) {
	DSprite *d = n->d;
	for (int i = 0; i < d->nanims; ++i) if (!strcmp(d->anims[i].nome, nome)) {
		if (d->act == i && d->reproduciendo) return;
		d->act = i; d->t = 0; d->reproduciendo = 1; d->fin_enviado = 0; snprintf(d->act_nome, sizeof d->act_nome, "%s", nome);
		if (d->anims[i].n) d->fot = d->anims[i].fr[0];
		return;
	}
}
void sprite_actualizar(Nodo *n, float dt) {
	DSprite *d = n->d;
	if (d->act < 0 || !d->reproduciendo) return;
	Anim *a = &d->anims[d->act]; if (!a->n || a->fps <= 0) return;
	d->t += dt * d->vel;
	int i = (int)(d->t * a->fps);
	if (i >= a->n) {
		if (a->bucle) i %= a->n;
		else {
			i = a->n - 1; d->reproduciendo = 0;
			if (!d->fin_enviado) { d->fin_enviado = 1; lua_pushstring(g_L, a->nome); sen_emitir(n, "animacion_terminada", 1); }
		}
	}
	d->fot = a->fr[i];
}

/* ---------------------------------------------------------------------------------------------------- partículas */
static float azar(DParticulas *d) { return (capi_rnd(&d->sem) & 0xFFFFFF) / 16777216.0f; }
void particulas_emitir(Nodo *n, int cant) {
	DParticulas *d = n->d; if (cant <= 0) cant = d->cantidad;
	float gx, gy; nodo_pos_global(n, &gx, &gy);
	for (int k = 0; k < cant && d->np < d->max; ++k) {
		Part *p = &d->p[d->np++];
		float ang = (d->angulo + (azar(d) - 0.5f) * d->dispersion) * 3.14159265f / 180.f, v = d->v_min + (d->v_max - d->v_min) * azar(d);
		p->x = gx; p->y = gy; p->vx = cosf(ang) * v; p->vy = sinf(ang) * v;
		p->vida = p->vida0 = d->vida * (1.f - d->vida_az * azar(d)); if (p->vida < 0.01f) p->vida = p->vida0 = 0.01f;
	}
}
void particulas_actualizar(Nodo *n, float dt) {
	DParticulas *d = n->d;
	if (d->emitiendo) {
		if (d->una_vez) { particulas_emitir(n, 0); d->emitiendo = 0; }
		else { d->tasa_acc += dt * d->cantidad / (d->vida > 0.01f ? d->vida : 0.01f); int k = (int)d->tasa_acc; d->tasa_acc -= (float)k; if (k > 0) particulas_emitir(n, k); }
	}
	for (int i = 0; i < d->np;) {
		Part *p = &d->p[i];
		p->vida -= dt;
		if (p->vida <= 0) { *p = d->p[--d->np]; continue; }
		p->vy += d->grav * dt; p->x += p->vx * dt; p->y += p->vy * dt; ++i;
	}
}

/* ---------------------------------------------------------------------------------------------------- botones */
static void boton_actualizar(Nodo *n) {
	DBoton *d = n->d; float m[6]; nodo_matriz(n, m);
	float x = m[4], y = m[5], mx = (float)g_raton_x, my = (float)g_raton_y;
	if (!d->fijo) dib_cam_pantalla_a_mundo(mx, my, &mx, &my);
	int dentro = mx >= x && my >= y && mx < x + d->w && my < y + d->h && !d->deshabilitado;
	int abajo = entrada_boton_raton(1);
	if (d->estado == 2 && !abajo) { d->estado = dentro ? 1 : 0; if (dentro) sen_emitir(n, "presionado", 0); }   /* soltar dentro de un botón pulsado */
	else if (d->estado != 2 && abajo && !d->previo) {                                                           /* ¿la pulsación empezó dentro? */
		float px = (float)g_raton_pulso_x, py = (float)g_raton_pulso_y;
		if (!d->fijo) dib_cam_pantalla_a_mundo(px, py, &px, &py);
		if (px >= x && py >= y && px < x + d->w && py < y + d->h && !d->deshabilitado) d->estado = 2; else d->estado = dentro ? 1 : 0;
	}
	else if (d->estado != 2) d->estado = dentro ? 1 : 0;
	d->previo = abajo;
}

/* ---------------------------------------------------------------------------------------------------- recorrido de proceso */
static void proceso_nodo(Nodo *n, float dt) {
	if ((n->flags & F_BORRAR) || !(n->flags & F_PROCESAR)) return;
	switch (n->tipo) {
	case T_SPRITE: sprite_actualizar(n, dt); break;
	case T_PARTICULAS: particulas_actualizar(n, dt); break;
	case T_BOTON: boton_actualizar(n); break;
	case T_TEMP: {
		DTemp *t = n->d;
		if (t->activo) { t->resto -= dt; if (t->resto <= 0) { if (t->una_vez) t->activo = 0; else t->resto += t->tiempo > 0.001f ? t->tiempo : 0.001f; sen_emitir(n, "fin", 0); } }
		break;
	}
	default: break;
	}
	for (int i = 0; i < n->nh; ++i) proceso_nodo(n->hijos[i], dt);
}

void mundo_proceso(float dt) {
	entrada_enviar_scripts(g_mundo.arbol);
	guion_proceso(g_mundo.arbol, dt);
	proceso_nodo(g_mundo.arbol, dt);
	g_mundo.tiempo += dt;
}

/* ---------------------------------------------------------------------------------------------------- cámara */
void camara_actualizar(Nodo *raiz, float dt) {
	(void)raiz;
	Nodo *cn = dib_camara_actual(); if (!cn) return;
	DCamara *c = cn->d;
	if (!c->seguir_id && c->seguir_ruta[0]) { Nodo *t = nodo_buscar(cn, c->seguir_ruta); if (t) c->seguir_id = t->id; }
	Nodo *t = c->seguir_id ? nodo_de_id(c->seguir_id) : NULL;
	float gx, gy; nodo_pos_global(cn, &gx, &gy);
	if (t) {
		float tx, ty; nodo_pos_global(t, &tx, &ty);
		if (c->suav > 0) { float f = 1.f - (float)exp(-c->suav * dt); gx += (tx - gx) * f; gy += (ty - gy) * f; } else { gx = tx; gy = ty; }
	}
	float vz = c->zoom > 0.05f ? c->zoom : 1, hw = g_vista_w * 0.5f / vz, hh = g_vista_h * 0.5f / vz;
	if (c->con_limites) {
		float l = c->limites[0], u = c->limites[1], r = c->limites[2], b = c->limites[3];
		gx = (r - l < 2 * hw) ? (l + r) * 0.5f : capi_clamp(gx, l + hw, r - hw);
		gy = (b - u < 2 * hh) ? (u + b) * 0.5f : capi_clamp(gy, u + hh, b - hh);
	}
	nodo_poner_pos_global(cn, gx, gy);
	/* temblor */
	static uint32_t sem = 777;
	if (c->sac_t > 0) {
		c->sac_t -= dt; float k = c->sac_dur > 0 ? c->sac_t / c->sac_dur : 0; if (k < 0) k = 0;
		c->cx = ((capi_rnd(&sem) & 255) / 127.5f - 1) * c->sac_amp * k; c->cy = ((capi_rnd(&sem) & 255) / 127.5f - 1) * c->sac_amp * k;
	} else c->cx = c->cy = 0;
}

/* ---------------------------------------------------------------------------------------------------- pasos */
void mundo_paso_fisica(float dt) {
	g_en_fisica = 1;
	fisica_preparar(g_mundo.arbol);
	guion_fisica(g_mundo.arbol, dt);
	fisica_paso(g_mundo.arbol, dt);
	camara_actualizar(g_mundo.arbol, dt);
	g_en_fisica = 0;
	entrada_fin_paso_fisica();
}

static void montar_escena(const char *ruta) {
	char err[200] = "";
	Nodo *e = escena_instanciar(ruta, err, sizeof err);
	if (!e) { capi_log("no se pudo cargar la escena %s: %s", ruta, err); return; }
	if (err[0]) capi_log("%s", err);
	if (g_mundo.raiz) { nodo_encolar_liberar(g_mundo.raiz); }
	g_mundo.raiz = e;
	nodo_agregar(g_mundo.arbol, e);
}

void mundo_fin_cuadro(void) {
	nodo_vaciar_cola();
	++g_mundo.cuadro;
	entrada_nuevo_cuadro();
	if (pend_escena[0]) {
		char r[CAPI_RUTA_MAX]; snprintf(r, sizeof r, "%s", pend_escena); pend_escena[0] = 0;
		Nodo *vieja = g_mundo.raiz;
		g_mundo.raiz = NULL;
		if (vieja) nodo_liberar_ya(vieja);
		montar_escena(r);
	}
}

int mundo_iniciar(char *err, size_t nerr) {
	memset(&g_mundo, 0, sizeof g_mundo);
	g_hz = g_proy.paso;
	g_mundo.arbol = nodo_nuevo(T_NODO, "Arbol");
	g_mundo.arbol->flags |= F_ARBOL | F_LISTO;
	if (!g_proy.principal[0]) { if (err) snprintf(err, nerr, "capibara.cfg no dice cuál es la escena_principal"); return -1; }
	char e2[200] = "";
	Nodo *e = escena_instanciar(g_proy.principal, e2, sizeof e2);
	if (!e) { if (err) snprintf(err, nerr, "escena principal %s: %s", g_proy.principal, e2); return -1; }
	if (e2[0]) capi_log("%s", e2);
	g_mundo.raiz = e;
	nodo_agregar(g_mundo.arbol, e);
	return 0;
}

void mundo_cerrar(void) {
	if (g_mundo.arbol) nodo_liberar_ya(g_mundo.arbol);
	g_mundo.arbol = g_mundo.raiz = NULL;
	guion_cerrar();
	escena_vaciar_cache();
	rec_liberar_todo();
}
void mundo_cambiar_escena(const char *ruta) { escena_cambiar(ruta); }
