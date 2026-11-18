// SPDX-License-Identifier: GPL-3.0-only
/* ligador.h - carga de bibliotecas compartidas (.so) desde el programa (D378,
 * prototipo de la fase 1 del enlace dinámico; docs/enlace-dinamico.md).
 *
 * Es un cargador en espacio de usuario: lee el fichero ELF ET_DYN de i386,
 * lo copia en páginas propias del programa (mapear_memoria), aplica las
 * relocaciones, protege las páginas (código RX, solo lectura R, datos RW y
 * RELRO en solo lectura) y llama a sus constructores. Las páginas NO se
 * comparten entre procesos: eso es la fase 3 (kernel).
 *
 * Las funciones siguen el dlfcn.h de POSIX. Enlace siempre inmediato
 * (RTLD_LAZY se trata como RTLD_NOW). Los símbolos del propio programa (que es
 * estático y no tiene tabla dinámica) se ofrecen a las bibliotecas con
 * dl_exportar(). */
#ifndef SAGUI_LIGADOR_H
#define SAGUI_LIGADOR_H

#define RTLD_LAZY   1
#define RTLD_NOW    2
#define RTLD_GLOBAL 0x100
#define RTLD_LOCAL  0
#define RTLD_DEFAULT ((void *)0)

void *dlopen(const char *nome, int modo);
void *dlsym(void *biblioteca, const char *simbolo);
int   dlclose(void *biblioteca);
char *dlerror(void);

/* Símbolos del programa que las bibliotecas pueden usar (por ejemplo, las
 * funciones de la libsagui mientras no exista libsagui.so). La tabla termina
 * con nome == NULL y tiene que seguir viva mientras haya bibliotecas abiertas. */
typedef struct {
    const char *nome;
    void       *endereco;
} dl_simbolo_t;
void dl_exportar(const dl_simbolo_t *tabela);

/* Datos de una biblioteca abierta, para las pruebas y para «ldd». */
typedef struct {
    const char *nome;       /* la ruta con la que se abrió */
    const char *soname;     /* DT_SONAME, o "" */
    unsigned    base;       /* dirección de carga */
    unsigned    tamanho;    /* bytes de direcciones que ocupa */
    unsigned    relocacoes; /* relocaciones aplicadas */
    unsigned    referencias;
} dl_info_t;
int dl_info(void *biblioteca, dl_info_t *info);

#endif
