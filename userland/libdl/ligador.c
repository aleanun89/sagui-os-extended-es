// SPDX-License-Identifier: GPL-3.0-only
/* ligador.c - cargador de bibliotecas compartidas en espacio de usuario (D378;
 * diseño completo en docs/enlace-dinamico.md).
 *
 * El mismo código sirve de dos formas:
 *   - enlazado en un programa estático (libdl, fase 1): dlopen/dlsym para
 *     cargar .so desde un programa normal; la memoria sale del malloc
 *     (mapear_memoria) y el programa ofrece sus símbolos con dl_exportar();
 *   - compilado con LD_INTERPRETE, dentro de /lib/ld-sagui.so.1 (fase 2): el
 *     intérprete de los programas con PT_INTERP. El kernel lo carga a dirección
 *     fija (INTERP_BASE) y le pasa el vector auxiliar; él abre los DT_NEEDED,
 *     reloca el programa (también R_386_COPY) y salta a su entrada. No usa el
 *     malloc (el del programa todavía no existe): sus estructuras van en una
 *     reserva propia y las páginas se piden con MEM_RESERVAR/MEM_ASIGNAR.
 *
 * Qué hace abrir una biblioteca:
 *   1. buscarla (ruta con '/', o SAGUI_LIB_CAMINHO si no es un proceso
 *      «seguro» (root o programa del sistema), DT_RUNPATH con $ORIGIN, /lib y
 *      /apps/lib);
 *   2. leer la cabecera y las de programa; comprobar ELF32, little-endian,
 *      ET_DYN, i386, sin PT_INTERP ni TLS, y ningún segmento W+X (D351);
 *   3. reservar las direcciones de todos los PT_LOAD de una vez y leer cada
 *      segmento directamente en su sitio (el resto queda a cero: la BSS);
 *   4. abrir sus DT_NEEDED (recursivo, con cuenta de referencias);
 *   5. aplicar las relocaciones (DT_REL y DT_JMPREL) en el momento: enlace
 *      inmediato. Rechaza DT_TEXTREL: el código tiene que ser PIC (-fPIC);
 *   6. proteger las páginas: código RX, solo lectura R, datos RW y la zona
 *      PT_GNU_RELRO (GOT incluida) en solo lectura;
 *   7. al final de toda la operación (cuando el programa también está
 *      relocado), llamar a DT_INIT y DT_INIT_ARRAY, dependencias antes.
 *
 * Orden de búsqueda de los símbolos: el programa (su .dynsym, o lo de
 * dl_exportar) y las bibliotecas en el orden en que se abrieron.
 *
 * Los campos y los números son los de la especificación ELF (gABI y
 * suplemento i386 del System V); el código es de este proyecto. */
#include "sagui.h"
#include "contas.h"
#include "ligador.h"

/* ---------------------------------------------------------- formato ELF32 */

typedef struct {
    uint8_t  ident[16];
    uint16_t tipo, maquina;
    uint32_t versao, entrada, desloc_ph, desloc_sh, flags;
    uint16_t tam_cabecalho, tam_ph, qtd_ph, tam_sh, qtd_sh, indice_nomes;
} elf_cab_t;

typedef struct {
    uint32_t tipo, desloc, virt, fis, tam_arquivo, tam_memoria, flags, alinhamento;
} elf_ph_t;

typedef struct { int32_t tag; uint32_t val; } elf_dyn_t;
typedef struct { uint32_t nome, valor, tam; uint8_t info, outro; uint16_t secao; } elf_sym_t;
typedef struct { uint32_t desloc, info; } elf_rel_t;

#define ET_DYN        3
#define EM_386        3
#define PT_LOAD       1
#define PT_DYNAMIC    2
#define PT_INTERP     3
#define PT_TLS        7
#define PT_GNU_RELRO  0x6474e552u
#define PF_X          1
#define PF_W          2

#define DT_NULL        0
#define DT_NEEDED      1
#define DT_PLTRELSZ    2
#define DT_HASH        4
#define DT_STRTAB      5
#define DT_SYMTAB      6
#define DT_RELA        7
#define DT_STRSZ       10
#define DT_INIT        12
#define DT_FINI        13
#define DT_SONAME      14
#define DT_RPATH       15
#define DT_REL         17
#define DT_RELSZ       18
#define DT_PLTREL      20
#define DT_TEXTREL     22
#define DT_JMPREL      23
#define DT_INIT_ARRAY  25
#define DT_FINI_ARRAY  26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_RUNPATH     29
#define DT_FLAGS       30
#define DT_GNU_HASH    0x6ffffef5
#define DF_TEXTREL     4

#define R_386_NONE     0
#define R_386_32       1
#define R_386_PC32     2
#define R_386_COPY     5
#define R_386_GLOB_DAT 6
#define R_386_JMP_SLOT 7
#define R_386_RELATIVE 8

#define STB_LOCAL  0
#define STB_WEAK   2
#define STT_TLS    6
#define STT_GNU_IFUNC 10

#define PAG 4096u
#define MAX_NECESSARIAS 16
#define MAX_PH 16
#define MAX_PENDENTES 64

/* ------------------------------------------------------ biblioteca abierta */

typedef struct biblioteca {
    struct biblioteca *prox;          /* lista global, en orden de apertura */
    char      nome[96];
    uint8_t  *mapa;                   /* las páginas pedidas (NULL en el programa) */
    uint32_t  base;                   /* dirección de carga (mapa - menor p_vaddr; 0 en el programa) */
    uint32_t  tam;                    /* bytes de mapa */
    uint32_t  lim_ini, lim_fim;       /* direcciones válidas de la imagen (para comprobar punteros) */
    unsigned  refs, relocacoes;
    bool      pronta;                 /* constructores ya llamados */
    bool      programa;               /* el ejecutable (solo en el intérprete) */
    const elf_sym_t *simbolos;
    const char      *textos;
    uint32_t         tam_textos;
    const uint32_t  *hash;            /* DT_HASH */
    const uint32_t  *gnu_hash;        /* DT_GNU_HASH */
    const char      *soname;
    struct biblioteca *necessarias[MAX_NECESSARIAS];
    int        n_necessarias;
    uint32_t   init, init_array, init_array_tam;
    uint32_t   fini, fini_array, fini_array_tam;
} biblioteca_t;

static biblioteca_t *bibliotecas;
static const dl_simbolo_t *exportados;
static char erro[160];
static bool ha_erro;
static mutex_t trava;
static bool trava_pronta;
static biblioteca_t *pendentes[MAX_PENDENTES];   /* constructores por llamar, en orden */
static int n_pendentes;

static void falhar(const char *fmt, const char *a, int n)
{
    formatar_texto(erro, sizeof erro, fmt, a ? a : "", n);
    ha_erro = true;
}

char *dlerror(void)
{
    if (!ha_erro) return NULL;
    ha_erro = false;
    return erro;
}

void dl_exportar(const dl_simbolo_t *tabela) { exportados = tabela; }

/* ------------------------------------------------- memoria (los dos modos) */

#ifdef LD_INTERPRETE
/* El intérprete corre antes que el malloc del programa: sus estructuras van
 * en una reserva fija y las páginas de las bibliotecas se piden al kernel. Lo
 * devuelto se apunta para reutilizar las direcciones en otro dlopen. */
#define MAX_BIBLIOTECAS 64
static biblioteca_t reserva[MAX_BIBLIOTECAS];
static bool reserva_usada[MAX_BIBLIOTECAS];
static struct { uint32_t ini, tam; } livres[32];
static int n_livres;
bool ld_seguro = true;   /* AT_SECURE: sin variables de entorno */

static biblioteca_t *bib_nova(void)
{
    for (int i = 0; i < MAX_BIBLIOTECAS; i++)
        if (!reserva_usada[i]) {
            reserva_usada[i] = true;
            memset(&reserva[i], 0, sizeof reserva[i]);
            return &reserva[i];
        }
    return NULL;
}

static void bib_soltar(biblioteca_t *b) { reserva_usada[b - reserva] = false; }

static uint8_t *paginas_pedir(uint32_t tam)
{
    uint32_t r = 0;
    for (int i = 0; i < n_livres; i++)
        if (livres[i].tam >= tam) {
            r = livres[i].ini;
            livres[i].ini += tam;
            livres[i].tam -= tam;
            if (!livres[i].tam) livres[i] = livres[--n_livres];
            break;
        }
    if (!r) {
        r = (uint32_t)memoria_paginas(MEM_RESERVAR, 0, tam);
        if (r > 0xFFFF0000u || r < 0x80000000u) return NULL;   /* un código de error */
    }
    if (memoria_paginas(MEM_ASIGNAR, (void *)r, tam) < 0) {
        if (n_livres < 32) { livres[n_livres].ini = r; livres[n_livres++].tam = tam; }
        return NULL;
    }
    return (uint8_t *)r;
}

static void paginas_devolver(uint8_t *p, uint32_t tam)
{
    memoria_proteger(p, tam, MEM_PROT_ESCRITA);
    memoria_paginas(MEM_DEVOLVER, p, tam);
    if (n_livres < 32) { livres[n_livres].ini = (uint32_t)p; livres[n_livres++].tam = tam; }
}
#else
static biblioteca_t *bib_nova(void) { return aloca_zerado(sizeof(biblioteca_t)); }
static void bib_soltar(biblioteca_t *b) { libera(b); }
static uint8_t *paginas_pedir(uint32_t tam) { return mapear_memoria(tam); }
static void paginas_devolver(uint8_t *p, uint32_t tam)
{
    /* Las páginas vuelven al malloc: antes, otra vez RW y sin ejecución. */
    memoria_proteger(p, tam, MEM_PROT_ESCRITA);
    desmapear_memoria(p, tam);
}
#endif

/* Proceso «seguro» (como los setuid de Linux): no se fía del entorno ni de las carpetas que
 * pueda cambiar una cuenta normal. Lo es root (también si se elevó después de arrancar, D255:
 * el AT_SECURE se calculó al cargar), un programa del sistema (/bin, servicios: USU_EST_SISTEMA,
 * D251/D269) y, en el intérprete, lo que dijo el kernel con AT_SECURE. Ante la duda, seguro. */
static bool proceso_seguro(void)
{
#ifdef LD_INTERPRETE
    if (ld_seguro) return true;
#endif
    if (sou_root()) return true;
    int e = usuario_op(USU_ESTADO, 0, 0);
    return e < 0 || (e & USU_EST_SISTEMA);
}

/* En un proceso seguro solo se cargan bibliotecas de root que nadie más pueda cambiar: el
 * fichero y todas las carpetas hasta la raíz son de root y sin escritura para el grupo ni para
 * los demás (y de un sistema de archivos con dueños: en FAT todo es de la cuenta que lo montó).
 * Así ni una instalación con dueños mal puestos ni un $ORIGIN en /tmp o en /home dejan a una
 * cuenta normal meter código en un proceso de root. */
static bool de_confianza(const char *caminho)
{
    char c[128];
    if (tam_texto(caminho) >= sizeof c || caminho[0] != '/') return false;
    copia_texto(c, caminho, sizeof c);
    for (;;) {
        info_permisos_t p;
        if (permisos_arquivo(c[0] ? c : "/", &p) < 0 || !p.com_donos || p.uid != 0 || (p.modo & 022)) return false;
        if (!c[0]) return true;
        char *barra = acha_ultimo_char(c, '/');
        if (!barra) return false;
        *barra = 0;   /* "/apps/lib/x.so" -> "/apps/lib" -> "/apps" -> "" (la raíz) */
    }
}

/* ---------------------------------------------------------- tablas hash */

static uint32_t hash_sysv(const char *s)
{
    uint32_t h = 0;
    for (; *s; s++) {
        h = (h << 4) + (uint8_t)*s;
        uint32_t g = h & 0xF0000000u;
        if (g) h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

static uint32_t hash_gnu(const char *s)
{
    uint32_t h = 5381;
    for (; *s; s++) h = h * 33 + (uint8_t)*s;
    return h;
}

/* ¿[a, a+n) está dentro de la imagen de "b"? */
static bool dentro(const biblioteca_t *b, uint32_t a, uint32_t n)
{
    return a >= b->lim_ini && a <= b->lim_fim && n <= b->lim_fim - a;
}

/* ¿Sirve este símbolo de la tabla para resolver una referencia de fuera? En el
 * programa (no PIC), una función de una biblioteca cuya dirección se toma vale
 * su entrada de la PLT del programa (sin sección, con valor): esa es la
 * dirección «canónica» para todos, salvo para las JMP_SLOT (que llevan a la
 * función de verdad). */
static bool definido(const biblioteca_t *b, const elf_sym_t *s, bool plt_canonica)
{
    if ((s->info >> 4) == STB_LOCAL) return false;
    if (s->secao != 0) return true;
    return plt_canonica && b->programa && s->valor != 0;
}

static const char *nome_de(const biblioteca_t *b, const elf_sym_t *s)
{
    return s->nome < b->tam_textos ? b->textos + s->nome : "";
}

/* El símbolo definido "nome" de la biblioteca "b", o NULL. */
static const elf_sym_t *procurar_em(const biblioteca_t *b, const char *nome, uint32_t hs, uint32_t hg, bool plt)
{
    /* Las tablas vienen del fichero: cada lectura se comprueba dentro de la imagen (una tabla
     * mal formada no puede leer fuera ni dar vueltas sin fin). */
    if (b->gnu_hash) {
        const uint32_t *g = b->gnu_hash;
        uint32_t nbaldes = g[0], primeiro = g[1], nfiltro = g[2], desloc = g[3];
        if (!nbaldes || !nfiltro || nbaldes > 0x1000000u || nfiltro > 0x1000000u) return NULL;
        const uint32_t *filtro = g + 4, *baldes = filtro + nfiltro, *cadeia = baldes + nbaldes;
        if (!dentro(b, (uint32_t)filtro, (nfiltro + nbaldes) * 4)) return NULL;
        uint32_t palavra = filtro[(hg / 32) % nfiltro];
        uint32_t mascara = (1u << (hg % 32)) | (1u << ((hg >> desloc) % 32));
        if ((palavra & mascara) != mascara) return NULL;
        uint32_t i = baldes[hg % nbaldes];
        if (i < primeiro) return NULL;
        for (;; i++) {
            if (i - primeiro >= 0x1000000u || !dentro(b, (uint32_t)&cadeia[i - primeiro], 4) || !dentro(b, (uint32_t)&b->simbolos[i], sizeof(elf_sym_t)))
                return NULL;
            uint32_t h2 = cadeia[i - primeiro];
            const elf_sym_t *s = &b->simbolos[i];
            if ((hg | 1) == (h2 | 1) && definido(b, s, plt) && compara_texto(nome_de(b, s), nome) == 0) return s;
            if (h2 & 1) return NULL;
        }
    }
    if (b->hash) {
        uint32_t nbaldes = b->hash[0], ncadeia = b->hash[1];
        if (!nbaldes || nbaldes > 0x1000000u || ncadeia > 0x1000000u) return NULL;
        const uint32_t *baldes = b->hash + 2, *cadeia = baldes + nbaldes;
        if (!dentro(b, (uint32_t)baldes, (nbaldes + ncadeia) * 4)) return NULL;
        uint32_t passos = 0;
        for (uint32_t i = baldes[hs % nbaldes]; i && i < ncadeia && passos++ < ncadeia; i = cadeia[i]) {
            const elf_sym_t *s = &b->simbolos[i];
            if (!dentro(b, (uint32_t)s, sizeof *s)) return NULL;
            if (definido(b, s, plt) && compara_texto(nome_de(b, s), nome) == 0) return s;
        }
    }
    return NULL;
}

/* Busca en el ámbito global (sin mirar en "excluir": para R_386_COPY).
 * Devuelve la dirección y, en *sim, el símbolo encontrado (para su tamaño). */
static uint32_t procurar_global(const char *nome, bool *achou, const biblioteca_t *excluir, const elf_sym_t **sim, bool plt,
                                const biblioteca_t **dono)
{
    *achou = true;
    if (sim) *sim = NULL;
    if (dono) *dono = NULL;
    if (exportados)
        for (const dl_simbolo_t *e = exportados; e->nome; e++)
            if (compara_texto(e->nome, nome) == 0) return (uint32_t)e->endereco;
    uint32_t hs = hash_sysv(nome), hg = hash_gnu(nome);
    for (biblioteca_t *b = bibliotecas; b; b = b->prox) {
        if (!b->simbolos || b == excluir) continue;
        const elf_sym_t *s = procurar_em(b, nome, hs, hg, plt);
        if (s) { if (sim) *sim = s; if (dono) *dono = b; return b->base + s->valor; }
    }
    *achou = false;
    return 0;
}

/* ------------------------------------------------------------- relocación */

static bool relocar(biblioteca_t *b, uint32_t tabela, uint32_t tam)
{
    if (!tabela || !tam) return true;
    if (!dentro(b, b->base + tabela, tam)) { falhar("%s: tabla de relocaciones fuera de la imagen", b->nome, 0); return false; }
    const elf_rel_t *r = (const elf_rel_t *)(b->base + tabela);
    for (uint32_t i = 0; i < tam / sizeof *r; i++) {
        uint32_t tipo = r[i].info & 0xFF, isim = r[i].info >> 8;
        if (tipo == R_386_NONE) continue;
        uint32_t *p = (uint32_t *)(b->base + r[i].desloc);
        if (!dentro(b, (uint32_t)p, 4)) { falhar("%s: relocación fuera de la imagen", b->nome, 0); return false; }
        uint32_t s = 0;
        const elf_sym_t *def = NULL;
        const biblioteca_t *dono = NULL;
        const elf_sym_t *sim = &b->simbolos[isim];
        if (tipo != R_386_RELATIVE) {
            if (!dentro(b, (uint32_t)sim, sizeof *sim)) { falhar("%s: símbolo fuera de la tabla", b->nome, 0); return false; }
            uint8_t t = sim->info & 0xF;
            if (t == STT_TLS || t == STT_GNU_IFUNC) {
                formatar_texto(erro, sizeof erro, "%s: símbolo TLS o IFUNC no soportado: %s", b->nome, nome_de(b, sim));
                ha_erro = true;
                return false;
            }
            if ((sim->info >> 4) == STB_LOCAL) s = b->base + sim->valor;
            else {
                bool achou;
                s = procurar_global(nome_de(b, sim), &achou, tipo == R_386_COPY ? b : NULL, &def, tipo != R_386_JMP_SLOT, &dono);
                if (!achou && (sim->info >> 4) != STB_WEAK) {
                    formatar_texto(erro, sizeof erro, "%s: símbolo sin definir: %s", b->nome, nome_de(b, sim));
                    ha_erro = true;
                    return false;
                }
            }
        }
        switch (tipo) {
        case R_386_RELATIVE: *p += b->base; break;
        case R_386_32:       *p += s; break;
        case R_386_PC32:     *p += s - (uint32_t)p; break;
        case R_386_GLOB_DAT:
        case R_386_JMP_SLOT: *p = s; break;
        case R_386_COPY:
            /* Solo en el programa: el dato de la biblioteca pasa a su BSS. */
            if (!b->programa || !def || !dono || def->tam != sim->tam || !dentro(b, (uint32_t)p, sim->tam) ||
                !dentro(dono, s, sim->tam)) {
                formatar_texto(erro, sizeof erro, "%s: R_386_COPY no válida: %s", b->nome, nome_de(b, sim));
                ha_erro = true;
                return false;
            }
            memcpy(p, (const void *)s, sim->tam);
            break;
        default:
            falhar("%s: relocación de tipo %d no soportada", b->nome, (int)tipo);
            return false;
        }
        b->relocacoes++;
    }
    return true;
}

/* ------------------------------------------------------------ protección */

/* Permisos de una página: bit 0 = escritura, bit 1 = ejecución. Si la
 * comparten dos segmentos, la suma. Una página que toca la zona PT_GNU_RELRO
 * queda de solo lectura si todo lo escribible que tiene es de esa zona (el ld
 * la hace acabar en un límite de página; su principio puede caer a mitad). */
static unsigned permisos_pagina(const elf_ph_t *ph, int nph, uint32_t pag)
{
    unsigned perm = 0;
    uint32_t r0 = 0, r1 = 0;
    for (int i = 0; i < nph; i++)
        if (ph[i].tipo == PT_GNU_RELRO) { r0 = ph[i].virt; r1 = ph[i].virt + ph[i].tam_memoria; }
    bool relro = r1 > pag && r0 < pag + PAG, fora = false;
    for (int i = 0; i < nph; i++) {
        if (ph[i].tipo != PT_LOAD || !ph[i].tam_memoria) continue;
        uint32_t a = ph[i].virt > pag ? ph[i].virt : pag;
        uint32_t b = ph[i].virt + ph[i].tam_memoria < pag + PAG ? ph[i].virt + ph[i].tam_memoria : pag + PAG;
        if (a >= b) continue;
        perm |= ((ph[i].flags & PF_W) ? 1u : 0) | ((ph[i].flags & PF_X) ? 2u : 0);
        if ((ph[i].flags & PF_W) && (a < r0 || b > r1)) fora = true;
    }
    if (relro && !fora) perm = (perm & ~1u) | 4u;   /* bit 2: página de la RELRO */
    return perm;
}

/* Protege [menor, menor + tam) (direcciones del fichero) de "b". */
static bool proteger(biblioteca_t *b, const elf_ph_t *ph, int nph, uint32_t menor, uint32_t tam, bool so_relro)
{
    for (uint32_t p = 0; p < tam; ) {
        unsigned perm = permisos_pagina(ph, nph, menor + p);
        uint32_t q = p + PAG;
        while (!so_relro && q < tam && permisos_pagina(ph, nph, menor + q) == perm) q += PAG;
        if ((perm & 3) == 3) { falhar("%s: página con escritura y ejecución (W^X)", b->nome, 0); return false; }
        int prot = (perm & 1 ? MEM_PROT_ESCRITA : 0) | (perm & 2 ? MEM_PROT_EXEC : 0);
        /* En el programa (el kernel ya puso los permisos de cada segmento) solo falta la RELRO. */
        bool fazer = so_relro ? (perm & 4) != 0 : prot != MEM_PROT_ESCRITA;
        if (fazer && memoria_proteger((void *)(b->base + menor + p), q - p, prot) < 0) {
            falhar("%s: no se pudieron proteger las páginas", b->nome, 0);
            return false;
        }
        p = q;
    }
    return true;
}

/* ------------------------------------------------- la sección dinámica */

typedef struct {
    uint32_t rel, relsz, jmprel, pltrelsz, soname, runpath;
    uint32_t necessarias[MAX_NECESSARIAS];
    int      n_necessarias;
} dinamica_t;

static bool ler_dinamica(biblioteca_t *b, uint32_t endereco, dinamica_t *d)
{
    memset(d, 0, sizeof *d);
    d->runpath = 0xFFFFFFFFu;
    uint32_t soname = 0xFFFFFFFFu;
    for (const elf_dyn_t *x = (const elf_dyn_t *)endereco; ; x++) {
        if (!dentro(b, (uint32_t)x, sizeof *x)) { falhar("%s: PT_DYNAMIC fuera de la imagen", b->nome, 0); return false; }
        if (x->tag == DT_NULL) break;
        uint32_t v = x->val;
        switch (x->tag) {
        case DT_NEEDED:
            if (d->n_necessarias == MAX_NECESSARIAS) { falhar("%s: demasiadas dependencias", b->nome, 0); return false; }
            d->necessarias[d->n_necessarias++] = v;
            break;
        case DT_SYMTAB:   b->simbolos = (const elf_sym_t *)(b->base + v); break;
        case DT_STRTAB:   b->textos = (const char *)(b->base + v); break;
        case DT_STRSZ:    b->tam_textos = v; break;
        case DT_HASH:     b->hash = (const uint32_t *)(b->base + v); break;
        case DT_GNU_HASH: b->gnu_hash = (const uint32_t *)(b->base + v); break;
        case DT_SONAME:   soname = v; break;
        case DT_RPATH:
        case DT_RUNPATH:  d->runpath = v; break;
        case DT_REL:      d->rel = v; break;
        case DT_RELSZ:    d->relsz = v; break;
        case DT_JMPREL:   d->jmprel = v; break;
        case DT_PLTRELSZ: d->pltrelsz = v; break;
        case DT_PLTREL:   if (v != DT_REL) { falhar("%s: DT_PLTREL no es DT_REL", b->nome, 0); return false; } break;
        case DT_RELA:     falhar("%s: relocaciones RELA (no son de i386)", b->nome, 0); return false;
        case DT_TEXTREL:  falhar("%s: tiene relocaciones de texto: compílala con -fPIC", b->nome, 0); return false;
        case DT_FLAGS:
            if (v & DF_TEXTREL) { falhar("%s: tiene relocaciones de texto: compílala con -fPIC", b->nome, 0); return false; }
            break;
        case DT_INIT:     b->init = v; break;
        case DT_FINI:     b->fini = v; break;
        case DT_INIT_ARRAY:   b->init_array = v; break;
        case DT_INIT_ARRAYSZ: b->init_array_tam = v; break;
        case DT_FINI_ARRAY:   b->fini_array = v; break;
        case DT_FINI_ARRAYSZ: b->fini_array_tam = v; break;
        default: break;
        }
    }
    if (!b->simbolos || !b->textos || (!b->hash && !b->gnu_hash) ||
        !dentro(b, (uint32_t)b->textos, b->tam_textos) || (b->hash && !dentro(b, (uint32_t)b->hash, 8)) ||
        (b->gnu_hash && !dentro(b, (uint32_t)b->gnu_hash, 16))) {
        falhar("%s: sin tabla de símbolos dinámica válida", b->nome, 0);
        return false;
    }
    if (!b->tam_textos || b->textos[b->tam_textos - 1] != 0) {   /* así ningún nombre lee más allá */
        falhar("%s: tabla de nombres sin terminar", b->nome, 0);
        return false;
    }
    b->soname = soname < b->tam_textos ? b->textos + soname : "";
    if (d->runpath != 0xFFFFFFFFu && d->runpath >= b->tam_textos) d->runpath = 0xFFFFFFFFu;
    for (int i = 0; i < d->n_necessarias; i++)
        if (d->necessarias[i] >= b->tam_textos) { falhar("%s: DT_NEEDED mal formado", b->nome, 0); return false; }
    return true;
}

/* ----------------------------------------------------------- abrir, cerrar */

static void soltar(biblioteca_t *b);

static void tirar_da_lista(biblioteca_t *b)
{
    for (biblioteca_t **pp = &bibliotecas; *pp; pp = &(*pp)->prox)
        if (*pp == b) { *pp = b->prox; return; }
}

static void destruir(biblioteca_t *b)
{
    tirar_da_lista(b);
    for (int i = 0; i < n_pendentes; i++)
        if (pendentes[i] == b) pendentes[i] = NULL;
    for (int i = b->n_necessarias - 1; i >= 0; i--) soltar(b->necessarias[i]);
    if (b->mapa) paginas_devolver(b->mapa, b->tam);
    bib_soltar(b);
}

static void soltar(biblioteca_t *b)
{
    if (--b->refs) return;
    if (b->pronta) {
        if (b->fini_array && dentro(b, b->base + b->fini_array, b->fini_array_tam))
            for (uint32_t i = b->fini_array_tam / 4; i > 0; i--) {
                uint32_t f = ((uint32_t *)(b->base + b->fini_array))[i - 1];
                if (f && f != 0xFFFFFFFFu) ((void (*)(void))f)();
            }
        if (b->fini) ((void (*)(void))(b->base + b->fini))();
    }
    destruir(b);
}

/* Llama a los constructores de lo que se cargó en esta operación, en el
 * orden en que terminó de cargarse (las dependencias, antes). */
static void construir_pendentes(void)
{
    for (int i = 0; i < n_pendentes; i++) {
        biblioteca_t *b = pendentes[i];
        if (!b || b->pronta) continue;
        b->pronta = true;
        if (b->init) ((void (*)(void))(b->base + b->init))();
        if (b->init_array && dentro(b, b->base + b->init_array, b->init_array_tam))
            for (uint32_t k = 0; k < b->init_array_tam / 4; k++) {
                uint32_t f = ((uint32_t *)(b->base + b->init_array))[k];
                if (f && f != 0xFFFFFFFFu) ((void (*)(void))f)();
            }
    }
    n_pendentes = 0;
}

/* Carpetas que nunca entran en la búsqueda por nombre: /apps/vps es de la cuenta sagui
 * (VisualPythonS guarda ahí, D297) aunque esté dentro de /apps, que es de root; y una
 * carpeta con «..» podría salir de /lib o de /apps/lib hacia cualquier sitio. */
static bool carpeta_excluida(const char *p)
{
    const char *vps = "/apps/vps";
    size_t n = tam_texto(vps), i = 0;
    while (i < n && p[i] == vps[i]) i++;
    if (i == n && (p[n] == 0 || p[n] == '/')) return true;
    for (const char *q = p; *q; q++)
        if (q[0] == '.' && q[1] == '.' && (q == p || q[-1] == '/') && (q[2] == 0 || q[2] == '/')) return true;
    return false;
}

static bool e_arquivo(const char *caminho)
{
    info_arquivo_t inf;
    return info_arquivo(caminho, &inf) >= 0 && (inf.tipo == TIPO_ARQUIVO || inf.tipo == TIPO_PROGRAMA);
}

/* Dónde está "nome": una ruta con '/' se usa tal cual; si no, se busca en
 * SAGUI_LIB_CAMINHO (no en un proceso seguro: root o programa del sistema,
 * como hace Linux con los setuid), en el DT_RUNPATH de quien la pide, en /lib
 * y en /apps/lib. En un proceso seguro, del DT_RUNPATH solo vale $ORIGIN (la
 * carpeta del programa según el kernel, AT_EXECFN, o la de una biblioteca ya
 * cargada) o /lib y /apps/lib: nunca una carpeta que diga el entorno; y solo se
 * aceptan ficheros de root que nadie más pueda cambiar (de_confianza). Nunca en
 * /apps/vps ni en carpetas con «..» (carpeta_excluida).
 * Una ruta que no cabe entera es un error: cortada, sería otra.
 * Devuelve 1 si la encontró, 0 si no y -1 si la rechazó (con el error puesto). */
static int localizar(const char *nome, const char *runpath, bool de_origem, char *caminho, int max)
{
    bool seguro = proceso_seguro();
    if (acha_char(nome, '/')) {
        if (tam_texto(nome) >= (size_t)max) { falhar("%s: ruta demasiado larga", nome, 0); return -1; }
        copia_texto(caminho, nome, (size_t)max);
        if (!e_arquivo(caminho)) return 0;
        if (seguro && !de_confianza(caminho)) {
            falhar("%s: un proceso de root o del sistema solo carga bibliotecas de root", caminho, 0);
            return -1;
        }
        return 1;
    }
    const char *var = seguro ? NULL : obter_env("SAGUI_LIB_CAMINHO");
    if (runpath && seguro && !de_origem && compara_texto(runpath, "/lib") != 0 && compara_texto(runpath, "/apps/lib") != 0)
        runpath = NULL;
    char lista[384];
    if ((var ? tam_texto(var) + 1 : 0) + (runpath ? tam_texto(runpath) + 1 : 0) + 15 >= sizeof lista) var = NULL;
    formatar_texto(lista, sizeof lista, "%s%s%s%s/lib:/apps/lib", var ? var : "", var ? ":" : "",
                   runpath ? runpath : "", runpath ? ":" : "");
    bool rechazada = false;
    for (char *p = lista; *p; ) {
        char *fim = acha_char(p, ':');
        if (fim) *fim = 0;
        if (*p && !carpeta_excluida(p) && tam_texto(p) + 1 + tam_texto(nome) < (size_t)max) {
            formatar_texto(caminho, (size_t)max, "%s/%s", p, nome);
            if (e_arquivo(caminho)) {
                if (!seguro || de_confianza(caminho)) return 1;
                rechazada = true;   /* sigue buscando: la de /lib o /apps/lib vale */
            }
        }
        if (!fim) break;
        p = fim + 1;
    }
    if (rechazada) {
        falhar("%s: un proceso de root o del sistema solo carga bibliotecas de root", nome, 0);
        return -1;
    }
    return 0;
}

static int ler_tudo(int fd, void *d, uint32_t n)
{
    uint32_t lidos = 0;
    while (lidos < n) {
        int r = ler(fd, (uint8_t *)d + lidos, n - lidos);
        if (r <= 0) return -1;
        lidos += (uint32_t)r;
    }
    return 0;
}

static biblioteca_t *abrir_biblioteca(const char *nome, const char *runpath, bool de_origem, int profundidade);

/* Abre las dependencias de "b" (ya en la lista) con su DT_RUNPATH. */
static bool abrir_necessarias(biblioteca_t *b, const dinamica_t *d, int profundidade)
{
    char rp[128];
    rp[0] = 0;
    bool de_origem = false;
    if (d->runpath != 0xFFFFFFFFu) {
        const char *r = b->textos + d->runpath;
        char origem[sizeof b->nome];
        copia_texto(origem, b->nome, sizeof origem);
        char *barra = acha_ultimo_char(origem, '/');
        if (barra) *barra = 0;
        else origem[0] = 0;   /* sin carpeta conocida: $ORIGIN no lleva a ningún sitio */
        bool cabe;
        if (comeca_com(r, "$ORIGIN")) {
            cabe = origem[0] && tam_texto(origem) + tam_texto(r + 7) < sizeof rp;
            if (cabe) formatar_texto(rp, sizeof rp, "%s%s", origem, r + 7);
            de_origem = !acha_char(r + 7, ':');
        } else {
            cabe = tam_texto(r) < sizeof rp;
            if (cabe) copia_texto(rp, r, sizeof rp);
        }
        if (!cabe) rp[0] = 0;   /* cortada sería otra carpeta: se busca solo en /lib y /apps/lib */
    }
    for (int i = 0; i < d->n_necessarias; i++) {
        biblioteca_t *n = abrir_biblioteca(b->textos + d->necessarias[i], rp[0] ? rp : NULL, de_origem, profundidade + 1);
        if (!n) return false;
        b->necessarias[b->n_necessarias++] = n;
    }
    return true;
}

static bool pendente(biblioteca_t *b)
{
    if (n_pendentes == MAX_PENDENTES) { falhar("%s: demasiadas bibliotecas en una operación", b->nome, 0); return false; }
    pendentes[n_pendentes++] = b;
    return true;
}

static biblioteca_t *carregar(const char *caminho, int profundidade)
{
    biblioteca_t *b = NULL;
    elf_cab_t c;
    elf_ph_t ph[MAX_PH];
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) { falhar("%s: no se puede leer", caminho, 0); return NULL; }
    if (ler_tudo(fd, &c, sizeof c) < 0 || c.ident[0] != 0x7F || c.ident[1] != 'E' || c.ident[2] != 'L' || c.ident[3] != 'F' ||
        c.ident[4] != 1 || c.ident[5] != 1 || c.tipo != ET_DYN || c.maquina != EM_386 || c.tam_ph != sizeof(elf_ph_t) ||
        c.qtd_ph == 0 || c.qtd_ph > MAX_PH) {
        falhar("%s: no es una biblioteca ELF32 i386 (ET_DYN)", caminho, 0);
        goto mal;
    }
    if (posicionar(fd, (int)c.desloc_ph, POS_INICIO) < 0 || ler_tudo(fd, ph, c.qtd_ph * sizeof(elf_ph_t)) < 0) {
        falhar("%s: cabeceras de programa truncadas", caminho, 0);
        goto mal;
    }
    uint32_t menor = 0xFFFFFFFFu, maior = 0, dinamico = 0;
    for (int i = 0; i < c.qtd_ph; i++) {
        if (ph[i].tipo == PT_INTERP) { falhar("%s: es un ejecutable (PT_INTERP), no una biblioteca", caminho, 0); goto mal; }
        if (ph[i].tipo == PT_TLS) { falhar("%s: usa TLS (__thread), que SaguiOS no tiene", caminho, 0); goto mal; }
        if (ph[i].tipo == PT_DYNAMIC) dinamico = ph[i].virt;
        if (ph[i].tipo != PT_LOAD || !ph[i].tam_memoria) continue;
        if ((ph[i].flags & PF_W) && (ph[i].flags & PF_X)) { falhar("%s: segmento con escritura y ejecución (W^X)", caminho, 0); goto mal; }
        if (ph[i].tam_arquivo > ph[i].tam_memoria || ph[i].tam_memoria > 0x10000000u || ph[i].virt > 0x10000000u) {
            falhar("%s: segmento mal formado", caminho, 0);
            goto mal;
        }
        if (ph[i].virt < menor) menor = ph[i].virt;
        if (ph[i].virt + ph[i].tam_memoria > maior) maior = ph[i].virt + ph[i].tam_memoria;
    }
    if (menor > maior || !dinamico) { falhar("%s: sin segmentos o sin PT_DYNAMIC", caminho, 0); goto mal; }
    menor &= ~(PAG - 1);

    b = bib_nova();
    if (!b) { falhar("%s: demasiadas bibliotecas abiertas", caminho, 0); goto mal; }
    copia_texto(b->nome, caminho, sizeof b->nome);
    b->tam = (maior - menor + PAG - 1) & ~(PAG - 1);
    b->mapa = paginas_pedir(b->tam);
    if (!b->mapa) { falhar("%s: sin memoria para cargarla", caminho, 0); goto mal; }
    b->base = (uint32_t)b->mapa - menor;
    b->lim_ini = (uint32_t)b->mapa;
    b->lim_fim = (uint32_t)b->mapa + b->tam;
    for (int i = 0; i < c.qtd_ph; i++)
        if (ph[i].tipo == PT_LOAD && ph[i].tam_arquivo &&
            (posicionar(fd, (int)ph[i].desloc, POS_INICIO) < 0 ||
             ler_tudo(fd, (void *)(b->base + ph[i].virt), ph[i].tam_arquivo) < 0)) {
            falhar("%s: fichero truncado", caminho, 0);
            goto mal;
        }
    fechar(fd);
    fd = -1;

    dinamica_t d;
    if (!ler_dinamica(b, b->base + dinamico, &d)) goto mal;

    /* En la lista antes que sus dependencias: así sus símbolos van antes. */
    b->refs = 1;
    b->prox = NULL;
    biblioteca_t **pp = &bibliotecas;
    while (*pp) pp = &(*pp)->prox;
    *pp = b;
    if (!abrir_necessarias(b, &d, profundidade)) goto mal_lista;
    if (!relocar(b, d.rel, d.relsz) || !relocar(b, d.jmprel, d.pltrelsz)) goto mal_lista;
    if (!proteger(b, ph, c.qtd_ph, menor, b->tam, false) || !pendente(b)) goto mal_lista;
    return b;

mal_lista:
    destruir(b);
    return NULL;
mal:
    if (fd >= 0) fechar(fd);
    if (b) {
        if (b->mapa) paginas_devolver(b->mapa, b->tam);
        bib_soltar(b);
    }
    return NULL;
}

static biblioteca_t *abrir_biblioteca(const char *nome, const char *runpath, bool de_origem, int profundidade)
{
    if (profundidade > 8) { falhar("%s: dependencias demasiado anidadas", nome, 0); return NULL; }
    /* La misma biblioteca, por soname o por ruta: una sola copia. */
    for (biblioteca_t *b = bibliotecas; b; b = b->prox)
        if (b->soname && b->soname[0] && compara_texto(b->soname, nome) == 0) { b->refs++; return b; }
    char caminho[sizeof ((biblioteca_t *)0)->nome];
    int l = localizar(nome, runpath, de_origem, caminho, sizeof caminho);
    if (l < 0) return NULL;
    if (l == 0) { falhar("%s: no se encuentra la biblioteca", nome, 0); return NULL; }
    for (biblioteca_t *b = bibliotecas; b; b = b->prox)
        if (compara_texto(b->nome, caminho) == 0) { b->refs++; return b; }
    return carregar(caminho, profundidade);
}

static void tomar(void)
{
    if (!trava_pronta) { mutex_criar(&trava); trava_pronta = true; }
    mutex_tomar(&trava);
}

void *dlopen(const char *nome, int modo)
{
    (void)modo;   /* siempre inmediato y con ámbito global (D378) */
    if (!nome) { falhar("dlopen(NULL) todavía no se admite", NULL, 0); return NULL; }
    tomar();
    n_pendentes = 0;
    biblioteca_t *b = abrir_biblioteca(nome, NULL, false, 0);
    mutex_largar(&trava);
    construir_pendentes();   /* fuera del cerrojo: un constructor puede llamar a dlopen */
    return b;
}

static bool valida(biblioteca_t *x)
{
    for (biblioteca_t *b = bibliotecas; b; b = b->prox)
        if (b == x && !b->programa) return true;
    return false;
}

void *dlsym(void *biblioteca, const char *simbolo)
{
    tomar();
    void *r = NULL;
    if (!biblioteca) {
        bool achou;
        r = (void *)procurar_global(simbolo, &achou, NULL, NULL, true, NULL);
        if (!achou) falhar("símbolo no encontrado: %s", simbolo, 0);
    } else if (!valida(biblioteca)) {
        falhar("dlsym: biblioteca no válida", NULL, 0);
    } else {
        /* La biblioteca y después sus dependencias, en anchura. */
        biblioteca_t *fila[32];
        int n = 0, i = 0;
        fila[n++] = biblioteca;
        uint32_t hs = hash_sysv(simbolo), hg = hash_gnu(simbolo);
        while (i < n && !r) {
            biblioteca_t *b = fila[i++];
            const elf_sym_t *s = procurar_em(b, simbolo, hs, hg, true);
            if (s) { r = (void *)(b->base + s->valor); break; }
            for (int k = 0; k < b->n_necessarias && n < 32; k++) fila[n++] = b->necessarias[k];
        }
        if (!r) falhar("símbolo no encontrado: %s", simbolo, 0);
    }
    mutex_largar(&trava);
    return r;
}

int dlclose(void *biblioteca)
{
    tomar();
    int r = 0;
    if (!valida(biblioteca)) { falhar("dlclose: biblioteca no válida", NULL, 0); r = -1; }
    else soltar(biblioteca);
    mutex_largar(&trava);
    return r;
}

int dl_info(void *biblioteca, dl_info_t *info)
{
    tomar();
    int r = -1;
    if (valida(biblioteca)) {
        biblioteca_t *b = biblioteca;
        info->nome = b->nome;
        info->soname = b->soname;
        info->base = b->base;
        info->tamanho = b->tam;
        info->relocacoes = b->relocacoes;
        info->referencias = b->refs;
        r = 0;
    }
    mutex_largar(&trava);
    return r;
}

#ifdef LD_INTERPRETE
/* -------------------------------------------- el programa (solo intérprete) */

/* Registra el programa (ya cargado por el kernel), abre sus DT_NEEDED, lo
 * reloca (R_386_COPY incluida), deja su RELRO en solo lectura y llama a los
 * constructores de las bibliotecas. 0 = bien; si no, el error en dlerror(). */
int ld_preparar_programa(const void *cabeceras, uint32_t qtd, const char *nome)
{
    const elf_ph_t *ph = cabeceras;
    if (qtd == 0 || qtd > MAX_PH) { falhar("%s: cabeceras de programa no válidas", nome, 0); return -1; }
    biblioteca_t *b = bib_nova();
    if (!b) return -1;
    /* "nome" es la ruta que dio el kernel (AT_EXECFN): de ella sale el $ORIGIN. Si no cabe,
     * mejor sin $ORIGIN que con otra carpeta. */
    if (tam_texto(nome) < sizeof b->nome) copia_texto(b->nome, nome, sizeof b->nome);
    else copia_texto(b->nome, "programa", sizeof b->nome);
    b->programa = true;
    b->refs = 1;
    b->lim_ini = 0xFFFFFFFFu;
    uint32_t dinamico = 0;
    for (uint32_t i = 0; i < qtd; i++) {
        if (ph[i].tipo == PT_DYNAMIC) dinamico = ph[i].virt;
        if (ph[i].tipo != PT_LOAD || !ph[i].tam_memoria) continue;
        if (ph[i].virt < b->lim_ini) b->lim_ini = ph[i].virt;
        if (ph[i].virt + ph[i].tam_memoria > b->lim_fim) b->lim_fim = ph[i].virt + ph[i].tam_memoria;
    }
    if (!dinamico || b->lim_ini >= b->lim_fim) { falhar("%s: sin PT_DYNAMIC", nome, 0); return -1; }
    dinamica_t d;
    if (!ler_dinamica(b, dinamico, &d)) return -1;
    b->prox = bibliotecas;   /* el programa, el primero del ámbito global */
    bibliotecas = b;
    n_pendentes = 0;
    if (!abrir_necessarias(b, &d, 0)) return -1;
    if (!relocar(b, d.rel, d.relsz) || !relocar(b, d.jmprel, d.pltrelsz)) return -1;
    uint32_t menor = b->lim_ini & ~(PAG - 1);
    if (!proteger(b, ph, (int)qtd, menor, ((b->lim_fim + PAG - 1) & ~(PAG - 1)) - menor, true)) return -1;
    b->pronta = true;   /* los constructores del programa los llama su _inicio */
    construir_pendentes();
    return 0;
}
#endif
