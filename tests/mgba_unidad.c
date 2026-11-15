// SPDX-License-Identifier: GPL-2.0-or-later
/* mgba_unidad.c - arnés de la prueba h322 en el PC (D322): el núcleo de mGBA sin SDL.
 * Uso: mgba_unidad ROM CUADROS [TECLAS_HEX_DESDE_CUADRO:...] [guardado]
 * Corre la ROM, y escribe el color (0xRRGGBB) del centro de los cuatro cuadrantes. Con «TECLAS»
 * (p. ej. 1@30 = tecla A a partir del cuadro 30, 0@60 = suelta) las manda al núcleo.
 * Con «estado» guarda un estado en el cuadro 20 y lo carga en el 60 (para ver que vuelve). */
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/serialize.h>
#include <mgba-util/vfs.h>
#include <mgba/internal/sm83/sm83.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned larg, alt;
static color_t *tela;

static unsigned rgb(unsigned x, unsigned y) {
	color_t p = tela[y * larg + x];
	return ((p & 0xFF) << 16) | (p & 0xFF00) | ((p >> 16) & 0xFF);
}

static void cuadrantes(const char *que) {
	printf("%s: %06x %06x %06x %06x\n", que, rgb(larg / 4, alt / 4), rgb(3 * larg / 4, alt / 4),
	       rgb(larg / 4, 3 * alt / 4), rgb(3 * larg / 4, 3 * alt / 4));
}

int main(int argc, char **argv) {
	if (argc < 3) return 2;
	struct VFile *vf = VFileOpen(argv[1], O_RDONLY);
	struct mCore *core = vf ? mCoreFindVF(vf) : NULL;
	if (!core) { printf("FALLO: no es una ROM\n"); return 1; }
	core->init(core);
	mCoreInitConfig(core, NULL);
	struct mCoreOptions o;
	memset(&o, 0, sizeof o);
	o.volume = 0x100; o.audioBuffers = 1024;
	mCoreConfigLoadDefaults(&core->config, &o);
	mCoreConfigSetDefaultIntValue(&core->config, "sgb.borders", 0);   /* sin el marco de la Super Game Boy: 160x144 */
	{	/* Paleta de la Game Boy en gris (mGBA parte de negro si no se le da ninguna). */
		static const unsigned gris[4] = { 0xF8F8F8, 0xA8A8A8, 0x505050, 0x000000 };
		for (int i = 0; i < 12; i++) {
			char k[16];
			snprintf(k, sizeof k, "gb.pal[%d]", i);
			mCoreConfigSetDefaultIntValue(&core->config, k, M_RGB8_TO_NATIVE(gris[i & 3]));
		}
	}
	mCoreLoadForeignConfig(core, &core->config);
	core->desiredVideoDimensions(core, &larg, &alt);
	tela = calloc(larg * alt, sizeof *tela);
	core->setVideoBuffer(core, tela, larg);
	if (!core->loadROM(core, vf)) { printf("FALLO: loadROM\n"); return 1; }
	char sav[300];
	snprintf(sav, sizeof sav, "%s.sav", argv[1]);
	struct VFile *sv = VFileOpen(sav, O_CREAT | O_RDWR);
	if (sv) core->loadSave(core, sv);
	core->reset(core);
	int cuadros = atoi(argv[2]);
	int estado = argc > 4 && !strcmp(argv[4], "estado");
	unsigned teclas = 0;
	char *prog = argc > 3 ? strdup(argv[3]) : NULL;
	if (getenv("MGBA_PC") && core->platform(core) != mPLATFORM_GBA) { struct SM83Core *c = core->cpu; for (int i = 0; i < 400; i++) { printf("pc=%04x sp=%04x a=%02x\n", c->pc, c->sp, c->a); core->step(core); } return 0; }
	for (int f = 0; f < cuadros; f++) {
		for (char *t = prog ? strtok(prog, ":") : NULL; t; t = strtok(NULL, ":")) {
			unsigned k; int c;
			if (sscanf(t, "%x@%d", &k, &c) == 2 && c == f) teclas = k;
		}
		if (prog) { free(prog); prog = strdup(argv[3]); }
		core->setKeys(core, teclas);
		core->runFrame(core);
		if (estado && f == 20) {
			cuadrantes("antes de guardar");
			struct VFile *e = VFileMemChunk(NULL, 0);
			if (!mCoreSaveStateNamed(core, e, SAVESTATE_ALL & ~SAVESTATE_SCREENSHOT)) { printf("FALLO: guardar estado\n"); return 1; }
			e->seek(e, 0, SEEK_SET);
			size_t n = e->size(e);
			printf("estado de %zu bytes\n", n);
			FILE *fo = fopen("/tmp/mgba_unidad.estado", "wb");
			void *b = malloc(n); e->read(e, b, n); fwrite(b, 1, n, fo); fclose(fo); free(b);
			e->close(e);
		}
		if (estado && f == 60) {
			cuadrantes("antes de cargar");
			struct VFile *e = VFileOpen("/tmp/mgba_unidad.estado", O_RDONLY);
			if (!mCoreLoadStateNamed(core, e, SAVESTATE_ALL & ~SAVESTATE_SCREENSHOT)) { printf("FALLO: cargar estado\n"); return 1; }
			e->close(e);
			core->runFrame(core);
			cuadrantes("despues de cargar");
		}
	}
	cuadrantes("final");
	if (getenv("MGBA_REJILLA")) for (unsigned y = 0; y < alt; y += 16) { for (unsigned x = 0; x < larg; x += 10) printf("%02x ", rgb(x, y) & 0xFF); printf("\n"); }
	if (getenv("MGBA_VOLCAR")) { unsigned h[8] = {0}, v[8]; int n = 0; for (unsigned i = 0; i < larg * alt; i++) { int j; for (j = 0; j < n; j++) if (v[j] == tela[i]) break; if (j == n && n < 8) v[n++] = tela[i]; if (j < 8) h[j]++; } for (int j = 0; j < n; j++) printf("color %08x: %u\n", v[j], h[j]); }
	if (getenv("MGBA_VOLCAR") && core->platform(core) != mPLATFORM_GBA) {
		printf("LCDC=%02x BGP=%02x SCX=%02x LY=%02x tile0=%02x tile1=%02x map0=%02x map10=%02x mapfila9=%02x\n", core->busRead8(core, 0xFF40), core->busRead8(core, 0xFF47), core->busRead8(core, 0xFF43), core->busRead8(core, 0xFF44), core->busRead8(core, 0x8000), core->busRead8(core, 0x8010), core->busRead8(core, 0x9800), core->busRead8(core, 0x980A), core->busRead8(core, 0x9800 + 9 * 32));
	}
	core->unloadROM(core);
	core->deinit(core);
	return 0;
}
