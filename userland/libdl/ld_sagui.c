// SPDX-License-Identifier: GPL-3.0-only
/* ld_sagui.c - /lib/ld-sagui.so.1, el intérprete de los programas con
 * bibliotecas compartidas (D378, fase 2a; docs/enlace-dinamico.md).
 *
 * Es un ELF estático a dirección fija (INTERP_BASE = 0xD8000000, guion
 * ld-sagui.ld): el kernel lo carga junto al programa cuando este trae
 * PT_INTERP y empieza por _ld_inicio con la pila de siempre más un cuarto
 * valor: [esp] = argc, [esp+4] = argv, [esp+8] = envp, [esp+12] = vector
 * auxiliar (AT_PHDR, AT_PHNUM, AT_ENTRY, AT_SECURE...). El intérprete abre las
 * bibliotecas, reloca el programa y salta a su entrada con la pila intacta,
 * así que el _inicio del programa la encuentra como en un programa estático.
 *
 * Después, el dlopen de los programas dinámicos es el de aquí: se lo ofrece
 * escribiendo su tabla de funciones en __ld_sagui_tabela (la define
 * dl_ponte.c dentro de libsagui.so). */
#include "sagui.h"
#include "ligador.h"

#define AT_NULL   0
#define AT_PHDR   3
#define AT_PHNUM  5
#define AT_ENTRY  9
#define AT_SECURE 23
#define AT_EXECFN 31

extern char **environ;
extern bool ld_seguro;
int ld_preparar_programa(const void *cabeceras, uint32_t qtd, const char *nome);

/* La tabla que ve libsagui.so (dl_ponte.c): el mismo orden allí. */
typedef struct {
    uint32_t versao;
    void *(*abrir)(const char *, int);
    void *(*simbolo)(void *, const char *);
    int   (*fechar)(void *);
    char *(*erro)(void);
    int   (*info)(void *, dl_info_t *);
} ld_tabela_t;

static const ld_tabela_t tabela = { 1, dlopen, dlsym, dlclose, dlerror, dl_info };

/* Devuelve la entrada del programa. Si algo falla, termina el proceso. */
uint32_t ld_principal(uint32_t *pilha)
{
    char **argv = (char **)pilha[1];
    environ = (char **)pilha[2];
    const uint32_t *aux = (const uint32_t *)pilha[3];
    const void *phdr = NULL;
    uint32_t phnum = 0, entrada = 0;
    const char *caminho = NULL;
    for (; aux && aux[0] != AT_NULL; aux += 2) {
        if (aux[0] == AT_PHDR) phdr = (const void *)aux[1];
        else if (aux[0] == AT_PHNUM) phnum = aux[1];
        else if (aux[0] == AT_ENTRY) entrada = aux[1];
        else if (aux[0] == AT_SECURE) ld_seguro = aux[1] != 0;
        else if (aux[0] == AT_EXECFN) caminho = (const char *)aux[1];
    }
    const char *nome = argv && argv[0] ? argv[0] : "?";
    /* El $ORIGIN del programa sale de la ruta que da el kernel (AT_EXECFN), nunca de argv[0]:
     * argv[0] lo elige quien lanza el programa, y en un programa del sistema o de root
     * apuntaría las bibliotecas a una carpeta suya. Sin AT_EXECFN, sin $ORIGIN. */
    if (!phdr || !entrada || ld_preparar_programa(phdr, phnum, caminho ? caminho : "programa") < 0) {
        char *e = dlerror();
        imprimir_em(ERROS, "ld-sagui: %s: %s\n", nome, e ? e : "sin vector auxiliar");
        sair(127);
    }
    const ld_tabela_t **t = (const ld_tabela_t **)dlsym(NULL, "__ld_sagui_tabela");
    if (t) *t = &tabela;
    else dlerror();
    return entrada;
}

/* La entrada: guarda ESP, llama a ld_principal con la pila alineada y salta
 * a la entrada del programa con ESP como lo dejó el kernel. */
__asm__(".section .text.inicio,\"ax\",@progbits\n"
        ".global _ld_inicio\n"
        "_ld_inicio:\n"
        "    xor %ebp, %ebp\n"
        "    mov %esp, %ebx\n"
        "    and $-16, %esp\n"
        "    sub $12, %esp\n"
        "    push %ebx\n"
        "    call ld_principal\n"
        "    mov %ebx, %esp\n"
        "    jmp *%eax\n"
        ".previous\n");
