/* SPDX-License-Identifier: GPL-3.0-only */
/* tcc_sagui_run.c - «tcc -run» en SaguiOS (D384). Lo incluye tccrun.c (parche 0001) dentro de
 * CONFIG_TCC_STATIC: tiene acceso a las funciones internas de TCC.
 *
 * En Linux, el programa que se compila en memoria usa la libc del propio tcc por dlsym(). En SaguiOS
 * el tcc es estático (no hay dlopen(NULL), D378): se genera al compilarlo una tabla con los símbolos
 * globales de la libc común, su libm y la base de la libsagui que lleva dentro (tcc_simbolos.c,
 * tools/tcc_simbolos.py), y este fichero los ofrece:
 *   - antes de enlazar, como símbolos débiles con dirección absoluta: así el enlazador no saca de
 *     /apps/tcc/lib/libc.a una segunda copia de printf, malloc o stdout (con su propio búfer, que nadie
 *     vaciaría al salir); lo que el programa defina él mismo manda sobre ellos;
 *   - y por dlsym(), para lo que siga sin definir al final.
 * Lo demás (SDL2 o la parte «medios» de la libsagui) sale de los archivos .a como siempre, y lo que
 * esos objetos piden de la libc también se resuelve con la del tcc.
 * Además, un programa de SaguiOS que empieza en principal() en vez de main() también se puede
 * ejecutar con -run: si no hay main, main es principal. */

typedef struct {
    const char *nome;
    void *endereco;
} tcc_simbolo_sagui_t;

/* Ordenada por nombre (strcmp), la genera tools/tcc_simbolos.py */
extern const tcc_simbolo_sagui_t tcc_simbolos_sagui[];
extern const int tcc_simbolos_sagui_n;

static void *sagui_buscar_simbolo(const char *nome)
{
    int ini = 0, fim = tcc_simbolos_sagui_n - 1;
    while (ini <= fim) {
        int meio = ini + (fim - ini) / 2;
        int c = strcmp(nome, tcc_simbolos_sagui[meio].nome);
        if (c == 0)
            return tcc_simbolos_sagui[meio].endereco;
        if (c < 0)
            fim = meio - 1;
        else
            ini = meio + 1;
    }
    return NULL;
}

ST_FUNC void *dlsym(void *handle, const char *symbol)
{
    (void)handle;
    return sagui_buscar_simbolo(symbol);
}

static int sagui_simbolo_definido(TCCState *s1, const char *nome)
{
    int i = find_elf_sym(symtab_section, nome);
    return i && ((ElfSym *)symtab_section->data)[i].st_shndx != SHN_UNDEF;
}

static void tcc_sagui_antes_de_run(TCCState *s1)
{
    int i;
    /* principal() como main() si el programa no tiene main */
    if (!sagui_simbolo_definido(s1, "main")) {
        i = find_elf_sym(symtab_section, "principal");
        if (i) {
            ElfSym *p = &((ElfSym *)symtab_section->data)[i];
            if (p->st_shndx != SHN_UNDEF)
                set_elf_sym(symtab_section, p->st_value, p->st_size,
                            ELFW(ST_INFO)(STB_GLOBAL, STT_FUNC), 0, p->st_shndx, "main");
        }
    }
    /* la libc y la libsagui del tcc, débiles (lo que el programa ya definió no se toca) */
    for (i = 0; i < tcc_simbolos_sagui_n; i++) {
        const char *nome = tcc_simbolos_sagui[i].nome;
        if (sagui_simbolo_definido(s1, nome))
            continue;
        set_elf_sym(symtab_section, (addr_t)(uintptr_t)tcc_simbolos_sagui[i].endereco, 0,
                    ELFW(ST_INFO)(STB_WEAK, STT_NOTYPE), 0, SHN_ABS, nome);
    }
}
