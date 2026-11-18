/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
#define _POSIX_C_SOURCE 200809L
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define PATH_SEP '\\'
#else
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#define PATH_SEP '/'
#endif

const char *cfg_required_files[3] = { "GRAPHICS.DRS", "INTERFAC.DRS", "TERRAIN.DRS" };

static int file_exists(const char *p) {
	FILE *f = fopen(p, "rb");
	if (!f) return 0;
	fclose(f);
	return 1;
}

static void exe_dir(char *out, size_t n) {
	out[0] = 0;
#ifdef _WIN32
	char buf[1024];
	DWORD len = GetModuleFileNameA(NULL, buf, sizeof buf);
	if (len == 0 || len >= sizeof buf) return;
	for (char *c = buf + len; c > buf; --c) if (*c == '\\' || *c == '/') { *c = 0; break; }
	snprintf(out, n, "%s", buf);
#else
	char buf[1024];
	ssize_t len = readlink("/proc/self/exe", buf, sizeof buf - 1);
	if (len <= 0) return;
	buf[len] = 0;
	for (char *c = buf + len; c > buf; --c) if (*c == '/') { *c = 0; break; }
	snprintf(out, n, "%s", buf);
#endif
}

void cfg_user_path(char *out, size_t n) {
#ifdef _WIN32
	const char *base = getenv("APPDATA");
	snprintf(out, n, "%s\\aoen\\aoen.cfg", base ? base : ".");
#else
	const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
	if (xdg && *xdg) snprintf(out, n, "%s/aoen/aoen.cfg", xdg);
	else snprintf(out, n, "%s/.config/aoen/aoen.cfg", home ? home : ".");
#endif
}

static void cfg_portable_path(char *out, size_t n) {
	char d[1024];
	exe_dir(d, sizeof d);
	if (d[0]) snprintf(out, n, "%s%caoen.cfg", d, PATH_SEP);
	else snprintf(out, n, "aoen.cfg");
}

void cfg_path(char *out, size_t n) {
	char p[1100];
	cfg_portable_path(p, sizeof p);
	if (file_exists(p)) { snprintf(out, n, "%s", p); return; }
	cfg_user_path(out, n);
}

static int read_cfg(const char *path, char *out, size_t n) {
	FILE *f = fopen(path, "r");
	if (!f) return 0;
	char line[1100];
	int found = 0;
	while (fgets(line, sizeof line, f)) {
		if (strncmp(line, "data_dir=", 9) == 0) {
			size_t l = strlen(line + 9);
			while (l > 0 && (line[9 + l - 1] == '\n' || line[9 + l - 1] == '\r')) line[9 + --l] = 0;
			snprintf(out, n, "%s", line + 9);
			found = out[0] != 0;
		}
	}
	fclose(f);
	return found;
}

int cfg_load_data_dir(char *out, size_t n) {
	char p[1100];
	cfg_portable_path(p, sizeof p);
	if (read_cfg(p, out, n)) return 1;
	cfg_user_path(p, sizeof p);
	return read_cfg(p, out, n);
}

static void make_dirs(const char *path) {
	char tmp[1100];
	snprintf(tmp, sizeof tmp, "%s", path);
	for (char *c = tmp + 1; *c; ++c) {
		if (*c == '/' || *c == '\\') {
			char save = *c;
			*c = 0;
#ifdef _WIN32
			_mkdir(tmp);
#else
			mkdir(tmp, 0755);
#endif
			*c = save;
		}
	}
}

static int file_in(const char *dir, const char *name, char *found, size_t n);

void cfg_saves_dir(char *out, size_t n) {
	char p[1100];
	cfg_user_path(p, sizeof p);
	/* quita el nombre del fichero y conserva la carpeta */
	size_t l = strlen(p);
	while (l > 0 && p[l - 1] != '/' && p[l - 1] != '\\') --l;
	p[l] = 0;
	snprintf(out, n, "%ssaves%c", p, PATH_SEP);
	char probe[1200];
	snprintf(probe, sizeof probe, "%s.keep", out);
	make_dirs(probe);
}

static int read_key(const char *path, const char *key, char *out, size_t n) {
	FILE *f = fopen(path, "r");
	if (!f) return 0;
	char line[1100];
	size_t kl = strlen(key);
	int found = 0;
	while (fgets(line, sizeof line, f)) {
		if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
			size_t l = strlen(line + kl + 1);
			while (l > 0 && (line[kl + l] == '\n' || line[kl + l] == '\r')) line[kl + l--] = 0;
			snprintf(out, n, "%s", line + kl + 1);
			found = out[0] != 0;
		}
	}
	fclose(f);
	return found;
}

/* reescribe la configuración conservando el resto de claves */
static int write_key(const char *path, const char *key, const char *value) {
	static char lines[64][1200];
	int n = 0, found = 0;
	size_t kl = strlen(key);

	FILE *f = fopen(path, "r");
	if (f) {
		char line[1200];
		while (n < 63 && fgets(line, sizeof line, f)) {
			size_t l = strlen(line);
			while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
			if (!line[0] || line[0] == '#') continue;
			if (strncmp(line, key, kl) == 0 && line[kl] == '=') { snprintf(lines[n++], sizeof lines[0], "%s=%s", key, value); found = 1; }
			else snprintf(lines[n++], sizeof lines[0], "%s", line);
		}
		fclose(f);
	}
	if (!found && n < 64) snprintf(lines[n++], sizeof lines[0], "%s=%s", key, value);

	make_dirs(path);
	f = fopen(path, "w");
	if (!f) return 0;
	fprintf(f, "# AoEN configuration\n");
	for (int i = 0; i < n; ++i) fprintf(f, "%s\n", lines[i]);
	fclose(f);
	return 1;
}

int cfg_get_int(const char *key, int def) {
	char p[1100], v[64];
	cfg_portable_path(p, sizeof p);
	if (read_key(p, key, v, sizeof v)) return atoi(v);
	cfg_user_path(p, sizeof p);
	if (read_key(p, key, v, sizeof v)) return atoi(v);
	return def;
}

int cfg_set_int(const char *key, int value) {
	char p[1100], v[32];
	cfg_portable_path(p, sizeof p);
	FILE *f = fopen(p, "r");
	if (f) fclose(f); else cfg_user_path(p, sizeof p); /* fichero portable si existe; si no, el perfil de usuario */
	snprintf(v, sizeof v, "%d", value);
	return write_key(p, key, v);
}

int cfg_save_textures_dir(const char *dir, int portable) {
	char p[1100];
	if (portable) cfg_portable_path(p, sizeof p);
	else cfg_user_path(p, sizeof p);
	return write_key(p, "textures_dir", dir);
}

static int textures_ok(const char *dir) {
	char found[1200];
	return file_in(dir, "g_gr2_00_color.png", found, sizeof found) && file_in(dir, "g_des_00_color.png", found, sizeof found) &&
		file_in(dir, "g_wt5_00_color.png", found, sizeof found) && file_in(dir, "g_wt3_00_color.png", found, sizeof found);
}

int cfg_find_textures(char *out, size_t n) {
	char d[1100];
	const char *env = getenv("AOEN_TEX");
	if (env && *env && textures_ok(env)) { snprintf(out, n, "%s", env); return 1; }

	char p[1100];
	cfg_portable_path(p, sizeof p);
	if (read_key(p, "textures_dir", d, sizeof d) && textures_ok(d)) { snprintf(out, n, "%s", d); return 1; }
	cfg_user_path(p, sizeof p);
	if (read_key(p, "textures_dir", d, sizeof d) && textures_ok(d)) { snprintf(out, n, "%s", d); return 1; }

	char ex[1024];
	exe_dir(ex, sizeof ex);
	static const char *rel[] = { "texturas", "textures", "../texturas", "../textures", "/apps/aoen/texturas" };
	for (int i = 0; i < 5; ++i) {
		if (ex[0]) {
			snprintf(d, sizeof d, "%s%c%s", ex, PATH_SEP, rel[i]);
			if (textures_ok(d)) { snprintf(out, n, "%s", d); return 1; }
		}
		if (textures_ok(rel[i])) { snprintf(out, n, "%s", rel[i]); return 1; }
	}
	return 0;
}

int cfg_save_data_dir(const char *dir, int portable) {
	char p[1100];
	if (portable) cfg_portable_path(p, sizeof p);
	else cfg_user_path(p, sizeof p);

	return write_key(p, "data_dir", dir);
}

/* variantes de mayúsculas de un nombre de fichero: tal cual, en minúsculas, con mayúscula inicial */
static int file_in(const char *dir, const char *name, char *found, size_t n) {
	char low[64], cap[64], p[1200];
	size_t i = 0;
	for (; name[i] && i < 63; ++i) low[i] = (char)tolower((unsigned char)name[i]);
	low[i] = 0;
	snprintf(cap, sizeof cap, "%s", low);
	cap[0] = (char)toupper((unsigned char)cap[0]);

	const char *names[3] = { name, low, cap };
	for (int k = 0; k < 3; ++k) {
		snprintf(p, sizeof p, "%s%c%s", dir, PATH_SEP, names[k]);
		if (file_exists(p)) {
			if (found) snprintf(found, n, "%s", p);
			return 1;
		}
	}
	return 0;
}

static int dir_has_all(const char *dir) {
	for (int i = 0; i < 3; ++i)
		if (!file_in(dir, cfg_required_files[i], NULL, 0)) return 0;
	return 1;
}

int cfg_check_dir(const char *dir, char *resolved, size_t n, char *missing, size_t mn) {
	static const char *sub[] = { "", "DATA", "Data", "data", "GAME/DATA", "Game/Data", "game/data", "GAME/Data", "Game/data" };
	char cand[1200];

	if (missing && mn) missing[0] = 0;
	if (!dir || !*dir) {
		if (missing) snprintf(missing, mn, "no se indicó ninguna carpeta");
		return 0;
	}

	size_t len = strlen(dir);
	char base[1100];
	snprintf(base, sizeof base, "%s", dir);
	while (len > 1 && (base[len - 1] == '/' || base[len - 1] == '\\')) base[--len] = 0;

	for (int i = 0; i < (int)(sizeof sub / sizeof sub[0]); ++i) {
		if (sub[i][0]) snprintf(cand, sizeof cand, "%s%c%s", base, PATH_SEP, sub[i]);
		else snprintf(cand, sizeof cand, "%s", base);
#ifdef _WIN32
		for (char *c = cand; *c; ++c) if (*c == '/') *c = '\\';
#endif
		if (dir_has_all(cand)) {
			if (resolved) snprintf(resolved, n, "%s", cand);
			return 1;
		}
	}

	if (missing && mn) {
		missing[0] = 0;
		for (int i = 0; i < 3; ++i) {
			if (!file_in(base, cfg_required_files[i], NULL, 0)) {
				size_t l = strlen(missing);
				snprintf(missing + l, mn - l, "%s%s", l ? ", " : "", cfg_required_files[i]);
			}
		}
	}
	return 0;
}

int cfg_find_data(const char *cmdline, char *resolved, size_t n) {
	char d[1100];
	if (cmdline && *cmdline && cfg_check_dir(cmdline, resolved, n, NULL, 0)) return 1;

	const char *env = getenv("AOEN_DATA");
	if (env && *env && cfg_check_dir(env, resolved, n, NULL, 0)) return 1;

	if (cfg_load_data_dir(d, sizeof d) && cfg_check_dir(d, resolved, n, NULL, 0)) return 1;

	/* ubicaciones habituales */
	char ex[1024];
	exe_dir(ex, sizeof ex);
	char cand[1200];
	if (ex[0]) {
		snprintf(cand, sizeof cand, "%s%cdata", ex, PATH_SEP);
		if (cfg_check_dir(cand, resolved, n, NULL, 0)) return 1;
		snprintf(cand, sizeof cand, "%s", ex);
		if (cfg_check_dir(cand, resolved, n, NULL, 0)) return 1;
	}
	static const char *places[] = { "/jogos/AOE/DATA", "/jogos/AOE", "/jogos/aoe", "/jogos/aoe/data", "data", "../data", "GAME/DATA", "../GAME/DATA", "C:\\Program Files\\Microsoft Games\\Age of Empires\\Data", "C:\\Program Files (x86)\\Microsoft Games\\Age of Empires\\Data", "D:\\GAME\\DATA", "/media/cdrom/GAME/DATA", "/mnt/cdrom/GAME/DATA" };
	for (int i = 0; i < (int)(sizeof places / sizeof places[0]); ++i)
		if (cfg_check_dir(places[i], resolved, n, NULL, 0)) return 1;
	return 0;
}

const char *cfg_instructions(void) {
	return
		"AoEN necesita los datos de su copia original de Age of Empires.\n"
		"\n"
		"La carpeta de datos tiene que contener estos archivos:\n"
		"  GRAPHICS.DRS   (graficos de unidades y edificios)\n"
		"  INTERFAC.DRS   (menus, fondos, paleta de colores)\n"
		"  TERRAIN.DRS    (terreno)\n"
		"\n"
		"Donde encontrarlos:\n"
		"  - En el CD original:  D:\\GAME\\DATA   (o la imagen ISO montada)\n"
		"  - En el juego instalado:  carpeta Data dentro de la carpeta de Age of Empires\n"
		"  - Tambien sirve la carpeta del CD o la carpeta GAME: AoEN busca la subcarpeta correcta.\n"
		"\n"
#ifdef SAGUI_AOEN
		"En SaguiOS: copie la carpeta del juego a /jogos/AOE (con GAME/DATA o DATA dentro): se encuentra sola.\n"
		"Las imagenes privadas la llevan si estaba en input_share/games/AOE al compilar.\n"
		"\n"
#endif
		"Las mayusculas y minusculas no importan (GRAPHICS.DRS o graphics.drs).\n"
		"AoEN no incluye estos archivos: hace falta una copia legitima del juego.\n"
		"\n"
		"Como indicar la carpeta:\n"
		"  1. Ejecute aoen-config e introduzca la ruta, o\n"
		"  2. aoen-config --set RUTA   o   aoen RUTA   o   la variable de entorno AOEN_DATA.\n";
}

int cfg_list_files(const char *dir, const char *ext, char names[][260], int max) {
	int n = 0;
	size_t el = strlen(ext);
#ifdef _WIN32
	char pat[1200];
	WIN32_FIND_DATAA fd;
	snprintf(pat, sizeof pat, "%s\\*", dir);
	HANDLE h = FindFirstFileA(pat, &fd);
	if (h == INVALID_HANDLE_VALUE) return 0;
	do {
		const char *nm = fd.cFileName;
		size_t l = strlen(nm);
		if (l > el && l < 260 && n < max) {
			int ok = 1;
			for (size_t i = 0; i < el; ++i) if (tolower((unsigned char)nm[l - el + i]) != tolower((unsigned char)ext[i])) ok = 0;
			if (ok) snprintf(names[n++], 260, "%s", nm);
		}
	} while (FindNextFileA(h, &fd));
	FindClose(h);
#else
	DIR *d = opendir(dir);
	if (!d) return 0;
	struct dirent *de;
	while ((de = readdir(d)) != NULL && n < max) {
		const char *nm = de->d_name;
		size_t l = strlen(nm);
		if (l > el && l < 260) {
			int ok = 1;
			for (size_t i = 0; i < el; ++i) if (tolower((unsigned char)nm[l - el + i]) != tolower((unsigned char)ext[i])) ok = 0;
			if (ok) snprintf(names[n++], 260, "%s", nm);
		}
	}
	closedir(d);
#endif
	/* orden estable */
	for (int i = 0; i < n; ++i)
		for (int j = i + 1; j < n; ++j)
			if (strcmp(names[i], names[j]) > 0) { char t[260]; memcpy(t, names[i], 260); memcpy(names[i], names[j], 260); memcpy(names[j], t, 260); }
	return n;
}

static int has_midi(const char *dir) {
	static char names[4][260];
	return cfg_list_files(dir, ".mid", names, 4) > 0;
}

int cfg_find_music(const char *data_dir, char *out, size_t n) {
	char d[1100], p[1100];
	const char *env = getenv("AOEN_MUSIC");
	if (env && *env && has_midi(env)) { snprintf(out, n, "%s", env); return 1; }

	cfg_portable_path(p, sizeof p);
	if (read_key(p, "music_dir", d, sizeof d) && has_midi(d)) { snprintf(out, n, "%s", d); return 1; }
	cfg_user_path(p, sizeof p);
	if (read_key(p, "music_dir", d, sizeof d) && has_midi(d)) { snprintf(out, n, "%s", d); return 1; }

	char ex[1024];
	exe_dir(ex, sizeof ex);
	static const char *rel[] = { "musica", "music", "../musica", "../music", "/apps/aoen/musica" };
	for (int i = 0; i < 5; ++i) {
		if (ex[0]) {
			snprintf(d, sizeof d, "%s%c%s", ex, PATH_SEP, rel[i]);
			if (has_midi(d)) { snprintf(out, n, "%s", d); return 1; }
		}
		if (has_midi(rel[i])) { snprintf(out, n, "%s", rel[i]); return 1; }
	}

	/* música original del CD junto a la carpeta de datos */
	snprintf(d, sizeof d, "%s%c..%cSOUND", data_dir, PATH_SEP, PATH_SEP);
	if (has_midi(d)) { snprintf(out, n, "%s", d); return 1; }
	return 0;
}
