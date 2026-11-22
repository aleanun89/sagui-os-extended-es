/* SPDX-License-Identifier: MIT */
/* texto.c - analizador del formato de texto de Capibara (escenas .esc y el proyecto capibara.cfg).
 *
 *   # comentario  (o ; comentario, solo al principio de línea o tras un espacio, fuera de comillas)
 *   [Tipo Nombre clave=valor "otra clave"=...]     cabecera de sección (nodo, capa, conexión...)
 *   propiedad = valor valor "texto" si no 3.5      propiedad de la sección actual
 *   . . 3 4 . 7                                    fila de datos (línea sin «=», dentro de una sección)
 *
 * Los valores se separan por espacios o comas; las comillas dobles agrupan; si/verdadero/true valen 1 y no/falso/false
 * valen 0; corchetes [ ] dentro de un valor se ignoran (sirven para agrupar listas a la vista). */
#include "capi.h"
#include <ctype.h>

/* separa el trozo siguiente de una línea: devuelve 0 si no hay más; q=1 si iba entre comillas */
static int trozo(const char **pp, char *out, size_t no, int *q) {
	const char *p = *pp;
	while (*p == ' ' || *p == '\t' || *p == ',') ++p;
	if (!*p || *p == '\r' || *p == '\n') { *pp = p; return 0; }
	if ((*p == '#' || *p == ';') ) { while (*p) ++p; *pp = p; return 0; }
	size_t n = 0;
	*q = 0;
	if (*p == '"') {
		*q = 1; ++p;
		while (*p && *p != '"' && *p != '\n' && *p != '\r') {
			if (*p == '\\' && p[1]) { ++p; out[n < no - 1 ? n++ : n] = *p == 'n' ? '\n' : *p; ++p; continue; }
			if (n < no - 1) out[n++] = *p;
			++p;
		}
		if (*p == '"') ++p;
	} else {
		while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != '\r' && *p != '\n' && !(*p == ';' ) ) { if (n < no - 1) out[n++] = *p; ++p; }
	}
	out[n] = 0; *pp = p;
	return 1;
}

static int es_numero(const char *s) {
	char *f; if (!*s) return 0;
	strtod(s, &f);
	return *f == 0;
}

static void cargar_valor(const char *p, Valor *v) {
	char t[96]; int q;
	v->n = 0;
	while (v->n < VAL_MAX && trozo(&p, t, sizeof t, &q)) {
		/* corchetes sueltos de las listas */
		if (!q) { char *a = t; while (*a == '[') ++a; size_t l = strlen(a); while (l && a[l - 1] == ']') a[--l] = 0; if (a != t) memmove(t, a, strlen(a) + 1); if (!*t) continue; }
		Tok *k = &v->t[v->n++];
		k->str = 0; k->num = 0; k->s[0] = 0;
		if (!q && es_numero(t)) { k->num = strtod(t, NULL); snprintf(k->s, sizeof k->s, "%s", t); continue; }
		if (!q && (!strcmp(t, "si") || !strcmp(t, "sí") || !strcmp(t, "verdadero") || !strcmp(t, "true"))) { k->num = 1; snprintf(k->s, sizeof k->s, "%s", t); continue; }
		if (!q && (!strcmp(t, "no") || !strcmp(t, "falso") || !strcmp(t, "false"))) { k->num = 0; snprintf(k->s, sizeof k->s, "%s", t); continue; }
		k->str = 1; snprintf(k->s, sizeof k->s, "%s", t);
	}
}

void texto_valor(const char *p, Valor *v) { cargar_valor(p, v); }   /* D460: el escritor y el editor separan valores como el cargador */

/* ¿hay un «=» fuera de comillas (y la línea no es una fila de datos)? devuelve su posición o NULL */
static const char *buscar_igual(const char *p) {
	int q = 0;
	for (; *p; ++p) {
		if (*p == '"') q = !q;
		else if (!q && *p == '=') return p;
		else if (!q && (*p == '#' || (*p == ';' && (p[-1] == ' ' || p[-1] == '\t')))) return NULL;
	}
	return NULL;
}

int texto_analizar(const char *buf, TextoCB *cb, void *u, char *err, size_t nerr) {
	static Valor val;
	int linea = 0;
	const char *p = buf;
	if (err && nerr) err[0] = 0;
	while (*p) {
		++linea;
		const char *fin = p;
		while (*fin && *fin != '\n') ++fin;
		char l[2048]; size_t n = (size_t)(fin - p);
		if (n >= sizeof l) n = sizeof l - 1;
		memcpy(l, p, n); l[n] = 0;
		p = *fin ? fin + 1 : fin;
		const char *s = l;
		while (*s == ' ' || *s == '\t') ++s;
		size_t sl = strlen(s);
		while (sl && (s[sl - 1] == '\r' || s[sl - 1] == ' ')) ((char *)s)[--sl] = 0;
		if (!sl || *s == '#' || *s == ';') continue;
		if (*s == '[') {
			const char *c = s + 1;
			const char *cierre = strrchr(s, ']');
			if (!cierre) { if (err) snprintf(err, nerr, "línea %d: falta ] en la cabecera", linea); return -1; }
			char dentro[1024]; size_t dl = (size_t)(cierre - c); if (dl >= sizeof dentro) dl = sizeof dentro - 1;
			memcpy(dentro, c, dl); dentro[dl] = 0;
			Cab cab; memset(&cab, 0, sizeof cab);
			const char *q = dentro;
			while (*q) {
				while (*q == ' ' || *q == '\t') ++q;
				if (!*q) break;
				char w[96]; size_t k = 0; int comilla = 0;
				if (*q == '"') { comilla = 1; ++q; while (*q && *q != '"') { if (k < sizeof w - 1) w[k++] = *q; ++q; } if (*q) ++q; w[k] = 0; }
				else { while (*q && *q != ' ' && *q != '\t' && *q != '=') { if (k < sizeof w - 1) w[k++] = *q; ++q; } w[k] = 0; }
				if (!comilla && *q == '=') {
					++q;
					char v[96]; size_t j = 0;
					if (*q == '"') { ++q; while (*q && *q != '"') { if (j < sizeof v - 1) v[j++] = *q; ++q; } if (*q) ++q; }
					else while (*q && *q != ' ' && *q != '\t') { if (j < sizeof v - 1) v[j++] = *q; ++q; }
					v[j] = 0;
					if (cab.na < 10) { snprintf(cab.an[cab.na], sizeof cab.an[0], "%s", w); snprintf(cab.av[cab.na], sizeof cab.av[0], "%s", v); ++cab.na; }
				} else if (cab.nt < 6) snprintf(cab.t[cab.nt++], sizeof cab.t[0], "%s", w);
			}
			if (cb->seccion && cb->seccion(u, &cab, linea) < 0) { if (err) snprintf(err, nerr, "línea %d: sección no válida", linea); return -1; }
			continue;
		}
		const char *eq = buscar_igual(s);
		if (eq) {
			char clave[64]; size_t kl = (size_t)(eq - s);
			while (kl && (s[kl - 1] == ' ' || s[kl - 1] == '\t')) --kl;
			if (kl >= sizeof clave) kl = sizeof clave - 1;
			memcpy(clave, s, kl); clave[kl] = 0;
			cargar_valor(eq + 1, &val);
			if (cb->propiedad && cb->propiedad(u, clave, &val, linea) < 0) { if (err) snprintf(err, nerr, "línea %d: propiedad «%s» no válida", linea, clave); return -1; }
		} else if (cb->fila && cb->fila(u, s, linea) < 0) { if (err) snprintf(err, nerr, "línea %d: fila no válida", linea); return -1; }
	}
	return 0;
}
