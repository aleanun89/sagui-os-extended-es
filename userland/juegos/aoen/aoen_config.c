/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* aoen-config: herramienta de consola para fijar y comprobar la carpeta con los datos originales del juego. */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim(char *s) {
	size_t l = strlen(s);
	while (l > 0 && (s[l - 1] == '\n' || s[l - 1] == '\r' || s[l - 1] == ' ' || s[l - 1] == '"')) s[--l] = 0;
	char *p = s;
	while (*p == ' ' || *p == '"') ++p;
	memmove(s, p, strlen(p) + 1);
}

static int apply(const char *dir, int portable) {
	char resolved[1100], missing[256];
	if (!cfg_check_dir(dir, resolved, sizeof resolved, missing, sizeof missing)) {
		printf("\nNo es una carpeta de datos valida.\nFaltan: %s\n\n%s", missing, cfg_instructions());
		return 1;
	}
	if (!cfg_save_data_dir(resolved, portable)) {
		printf("\nNo se pudo guardar la configuracion.\n");
		return 1;
	}
	char p[1100];
	cfg_path(p, sizeof p);
	printf("\nListo. Carpeta de datos:\n  %s\nGuardada en:\n  %s\n", resolved, p);
	return 0;
}

static void show(void) {
	char p[1100], d[1100], resolved[1100], missing[256];
	cfg_path(p, sizeof p);
	printf("Archivo de configuracion: %s\n", p);
	if (!cfg_load_data_dir(d, sizeof d)) {
		printf("Carpeta de datos: (sin configurar)\n");
		return;
	}
	printf("Carpeta de datos: %s\n", d);
	if (cfg_check_dir(d, resolved, sizeof resolved, missing, sizeof missing)) printf("Estado: correcta (%s)\n", resolved);
	else printf("Estado: INCORRECTA, faltan: %s\n", missing);
}

int main(int argc, char **argv) {
	int portable = 0;
	const char *set = NULL;

	for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			printf("Uso:\n  aoen-config                 asistente interactivo\n  aoen-config --set RUTA      guardar la carpeta de datos\n  aoen-config --show          mostrar la configuracion actual\n  aoen-config --check RUTA    solo comprobar una carpeta\n  --textures RUTA            carpeta con texturas HD opcionales (PNG 512x512)\n  --portable                  guardar junto al ejecutable (aoen.cfg) en vez de en el perfil de usuario\n\n%s", cfg_instructions());
			return 0;
		} else if (!strcmp(argv[i], "--portable")) portable = 1;
		else if (!strcmp(argv[i], "--show")) { show(); return 0; }
		else if (!strcmp(argv[i], "--textures") && i + 1 < argc) {
			if (!cfg_save_textures_dir(argv[i + 1], portable)) { printf("No se pudo guardar la configuracion.\n"); return 1; }
			printf("Carpeta de texturas HD: %s\n(debe contener g_gr2_00_color.png, g_des_00_color.png, g_wt5_00_color.png y g_wt3_00_color.png, de 512x512)\n", argv[i + 1]);
			return 0;
		}
		else if (!strcmp(argv[i], "--set") && i + 1 < argc) set = argv[++i];
		else if (!strcmp(argv[i], "--check") && i + 1 < argc) {
			char r[1100], m[256];
			if (cfg_check_dir(argv[i + 1], r, sizeof r, m, sizeof m)) { printf("Correcta: %s\n", r); return 0; }
			printf("Incorrecta, faltan: %s\n", m);
			return 1;
		}
	}

	if (set) return apply(set, portable);

	printf("AoEN - configuracion\n====================\n\n%s\n", cfg_instructions());
	show();
	printf("\nIntroduzca la carpeta con los datos del juego (vacio para salir): ");
	fflush(stdout);

	char line[1100];
	if (!fgets(line, sizeof line, stdin)) return 0;
	trim(line);
	if (!line[0]) return 0;
	return apply(line, portable);
}
