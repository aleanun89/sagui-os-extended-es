/* dlfcn.h - dlopen y compañía (D477): el prototipo y las constantes están en ligador.h (libsagui). */
#ifndef SAGUI_DLFCN_H
#define SAGUI_DLFCN_H
#include <ligador.h>
/* dladdr no resuelve nada (solo la usa el volcado de caída de Valkey). */
typedef struct { const char *dli_fname; void *dli_fbase; const char *dli_sname; void *dli_saddr; } Dl_info;
int dladdr(const void *dir, Dl_info *info);
#endif
