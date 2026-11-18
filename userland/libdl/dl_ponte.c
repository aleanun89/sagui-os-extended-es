// SPDX-License-Identifier: GPL-3.0-only
/* dl_ponte.c - dlopen/dlsym/dlclose/dlerror dentro de libsagui.so (D378).
 *
 * En un programa dinámico quien carga bibliotecas es el intérprete
 * (/lib/ld-sagui.so.1), que ya tiene la lista de las abiertas: al arrancar
 * escribe aquí su tabla de funciones y estas llamadas pasan a él. Los
 * programas estáticos no enlazan este fichero: usan userland/libdl/ligador.c. */
#include "sagui.h"
#include "ligador.h"

typedef struct {
    uint32_t versao;
    void *(*abrir)(const char *, int);
    void *(*simbolo)(void *, const char *);
    int   (*fechar)(void *);
    char *(*erro)(void);
    int   (*info)(void *, dl_info_t *);
} ld_tabela_t;

const ld_tabela_t *__ld_sagui_tabela;

static char sem_interprete[] = "no hay intérprete de bibliotecas (programa estático)";

void *dlopen(const char *nome, int modo) { return __ld_sagui_tabela ? __ld_sagui_tabela->abrir(nome, modo) : NULL; }
void *dlsym(void *b, const char *s) { return __ld_sagui_tabela ? __ld_sagui_tabela->simbolo(b, s) : NULL; }
int   dlclose(void *b) { return __ld_sagui_tabela ? __ld_sagui_tabela->fechar(b) : -1; }
char *dlerror(void) { return __ld_sagui_tabela ? __ld_sagui_tabela->erro() : sem_interprete; }
int   dl_info(void *b, dl_info_t *i) { return __ld_sagui_tabela ? __ld_sagui_tabela->info(b, i) : -1; }
void  dl_exportar(const dl_simbolo_t *t) { (void)t; }   /* el programa dinámico ya tiene su .dynsym */
