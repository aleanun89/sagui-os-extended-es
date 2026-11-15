// SPDX-License-Identifier: GPL-2.0-or-later
/* memoria_mgba.c - la memoria «anonima» que pide mGBA (mgba-util/memory.h), sobre malloc (D322).
 * El original (src/platform/posix/memory.c) usa mmap; el SaguiOS no lo tiene. */
#include <mgba-util/memory.h>
#include <stdlib.h>
#include <string.h>

void* anonymousMemoryMap(size_t size) {
	return calloc(1, size);
}

void mappedMemoryFree(void* memory, size_t size) {
	(void) size;
	free(memory);
}

/* La libm del sistema no tiene exp2f (la usa la llamada al sistema SoundBiosGetKey... de la BIOS HLE). */
#include <math.h>
float exp2f(float x) {
	return powf(2.0f, x);
}

/* Directorios del VFS: mGBA los usa para sus carpetas de capturas, trucos y estados (core/directories.c).
 * Aquí los guardados van por rutas fijas del front-end y no se usan: no hay directorio que abrir. */
#include <mgba-util/vfs.h>
struct VDir* VDirOpen(const char* path) {
	(void) path;
	return NULL;
}
bool VDirCreate(const char* path) {
	(void) path;
	return false;
}
