// SPDX-License-Identifier: GPL-3.0-only
/* imagens.c - páginas de solo lectura de las bibliotecas compartidas entre procesos
 * (D378, fase 3; diseño en docs/enlace-dinamico.md §15 y en D378).
 *
 * Un «objeto imagen» es un trozo de un fichero (páginas [desloc, desloc+n)) cargado una vez
 * en páginas físicas propias, con una cuenta de referencias (los mapas que lo usan). El
 * intérprete (ld-sagui) pide con MEM_COMPARTIR las páginas de una biblioteca que solo tienen
 * código o datos de solo lectura; lo escribible (.data, GOT, RELRO, .bss) sigue siendo
 * privado de cada proceso (MEM_ASIGNAR), así que no hace falta copia al escribir.
 *
 * Cómo se carga (sin dejar nada a medias si el proceso muere dormido):
 *   1. comprobaciones sin tocar nada: rango del área de datos sin asignar; el descriptor es
 *      un fichero abierto para leer y de confianza (de root sin escritura de grupo ni de otros,
 *      él y sus carpetas hasta la raíz, en un sistema de archivos con dueños); es un ELF
 *      ET_DYN de i386 y cada página pedida cae en un segmento PT_LOAD sin escritura (revisión
 *      I1: no se puede fijar en memoria cualquier fichero de root); queda sitio bajo el tope
 *      global (una cuarta parte de la RAM);
 *   2. se asignan páginas PRIVADAS del proceso en el rango (de solo lectura para él) y se leen
 *      del fichero (puede dormir). Si el proceso muere aquí, son suyas y se liberan con su
 *      espacio: ni fugas ni objetos a medias en la tabla. Si algo falla después de dormir, las
 *      páginas se quedan como propias normales (no se pueden quitar: deixar_privadas) y quien
 *      llama las devuelve con MEM_DEVOLVER;
 *   3. ya sin dormir (cerrojo general, interrupciones apagadas), cada entrada tiene que ser
 *      EXACTAMENTE la que se puso: presente, propia, sin escritura y con la NX pedida. Otro hilo
 *      del proceso puede haber dado escritura a esas páginas con MEM_PROTEGER mientras esta
 *      llamada dormía (dar permisos no espera a la barrera D351): entonces se rechaza, para
 *      que nunca quede un alias escribible de un objeto compartido (revisión B1). Después se
 *      busca un objeto con la misma clave (ruta, tamaño y fecha del fichero, desplazamiento,
 *      páginas, ejecución) y se COMPARA BYTE A BYTE con lo recién leído. Igual: se mapean sus
 *      páginas y las privadas se liberan. Distinto (el fichero cambió sin cambiar la clave):
 *      ese objeto queda obsoleto (nadie más lo encuentra) y las páginas privadas pasan a ser un
 *      objeto nuevo, con sus entradas reescritas con los permisos del objeto y la TLB de las
 *      demás CPU sincronizada antes de que nadie más pueda encontrarlo. Así una página
 *      compartida nunca tiene otro contenido que el que el proceso habría leído él mismo.
 *
 * Las entradas llevan PG_EMPRESTADA: espaco_destruir, MEM_DEVOLVER (espaco_tirar), la swap
 * (D352) y MEM_PROTEGER (espaco_ajustar) ya no las tocan; nunca llevan PG_ESCRITA y las de
 * datos llevan NX. Cada mapa (espacio, dirección, páginas, objeto) va en la lista de su
 * espacio (tabla por espacio); al destruirse el espacio (o con MEM_DEVOLVER) la cuenta baja,
 * y el objeto se libera cuando llega a cero (el espacio ya no está en ninguna CPU, o se
 * sincronizó la TLB antes).
 *
 * Concurrencia: todo esto corre con el cerrojo general (D165), así que entre dos esperas no
 * hay nadie más en el kernel. Solo duerme la lectura del fichero (a un búfer de una página
 * pedido en cada llamada). */
#include "imagens.h"
#include "abi.h"
#include "console.h"
#include "memalta.h"
#include "memoria.h"
#include "processo.h"
#include "texto.h"
#include "troca.h"
#include "vfs.h"

#define IMAGENS_MAX      256u      /* objetos vivos como mucho */
#define IMAGEM_PAGINAS   4096u     /* 16 MB por objeto como mucho */
#define MAPAS_MAX        4096u     /* mapas vivos como mucho (todos los procesos) */
#define BALDES           64u       /* listas de mapas, por espacio */
#define ELF_PH_MAX       16u

typedef struct imagem {
    struct imagem *prox;
    uint32_t chave;               /* hash de la ruta */
    uint32_t tamanho, modificado; /* del fichero cuando se creó */
    uint32_t desloc, paginas;
    bool     exec, obsoleta;
    uint32_t refs;                /* mapas que la usan */
    fis_t    fis[];               /* sus páginas físicas */
} imagem_t;

typedef struct mapa {
    struct mapa *prox;
    espaco_t  e;
    uint32_t  virt, paginas;
    uint32_t  restantes;          /* páginas aún mapeadas (MEM_DEVOLVER puede quitar unas) */
    imagem_t *im;
} mapa_t;

static imagem_t *imagens;
static mapa_t   *mapas[BALDES];   /* revisión M3: cada espacio mira solo su lista */
static uint32_t  n_imagens, n_mapas, paginas_total, mapeadas_total, reutilizadas, criadas;

static inline mapa_t **balde(espaco_t e) { return &mapas[(e >> 12) % BALDES]; }

static uint32_t hash_texto(const char *s)
{
    uint32_t h = 2166136261u;   /* FNV-1a */
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

/* Como interp_confiavel (carregador.c) y de_confianza (ligador.c): el fichero y todas sus
 * carpetas hasta la raíz, de root, sin escritura de grupo ni de otros, con dueños. */
static bool caminho_confiavel(const char *caminho)
{
    char c[CAMINHO_MAX];
    if (caminho[0] != '/' || txt_tamanho(caminho) >= sizeof c) return false;
    txt_copia_n(c, caminho, sizeof c);
    for (;;) {
        meta_t m;
        if (vfs_meta(c[0] ? c : "/", &m) < 0 || !m.com_donos || m.uid != 0 || (m.modo & 022)) return false;
        if (!c[0]) return true;
        char *barra = NULL;
        for (char *q = c; *q; q++)
            if (*q == '/') barra = q;
        if (!barra) return false;
        *barra = 0;   /* "/apps/lib/x.so" -> "/apps/lib" -> "/apps" -> "" (la raíz) */
    }
}

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* Revisión I1: el fichero es un ELF32 ET_DYN de i386 y cada página de [desloc, desloc+n) cae
 * dentro de lo que el fichero guarda de un PT_LOAD sin escritura (redondeado a páginas: la
 * última página de un segmento lleva también bytes de lo que le sigue en el fichero). "buf"
 * es un búfer de una página del kernel. */
static int32_t elf_compartivel(uint32_t fd, uint32_t tamanho, uint32_t desloc, uint32_t n, uint8_t *buf)
{
    int32_t r = arquivos_ler_em(fd, 0, buf, tamanho < PAGINA ? tamanho : PAGINA);
    if (r < 52) return r < 0 ? r : ERRO_FORMATO;
    uint32_t ph = le32(buf + 28);
    uint16_t tipo = le16(buf + 16), maquina = le16(buf + 18), tam_ph = le16(buf + 42), qtd = le16(buf + 44);
    if (buf[0] != 0x7F || buf[1] != 'E' || buf[2] != 'L' || buf[3] != 'F' || buf[4] != 1 || buf[5] != 1 || tipo != 3 ||
        maquina != 3 || tam_ph != 32 || !qtd || qtd > ELF_PH_MAX || ph > (uint32_t)r || qtd * 32u > (uint32_t)r - ph)
        return ERRO_FORMATO;   /* las cabeceras de programa, dentro de la primera página */
    for (uint32_t p = desloc; p < desloc + n; p += PAGINA) {
        bool coberta = false;
        for (uint32_t i = 0; i < qtd && !coberta; i++) {
            const uint8_t *x = buf + ph + i * 32;   /* tipo, desloc, virt, fis, tam_arq, tam_mem, flags */
            uint32_t d = le32(x + 4), t = le32(x + 16);
            if (le32(x) != 1 || (le32(x + 24) & 2) || !t || d + t < d) continue;   /* PT_LOAD sin PF_W */
            coberta = p >= (d & ~(PAGINA - 1)) && p < ((d + t + PAGINA - 1) & ~(PAGINA - 1));
        }
        if (!coberta) return ERRO_FORMATO;
    }
    return 0;
}

/* Quita y libera páginas privadas del rango (lo que espaco_alocar o la lectura dejaron). */
static void soltar_privadas(espaco_t e, uint32_t ini, uint32_t paginas)
{
    fis_t lote[32];
    uint32_t k = 0;
    for (uint32_t i = 0; i < paginas; i++) {
        fis_t f = espaco_tirar(e, ini + i * PAGINA);
        if (f) lote[k++] = f;
        if (k == 32 || (i + 1 == paginas && k)) {
            espaco_sincronizar(e);
            for (uint32_t j = 0; j < k; j++) memfisica_liberar(lote[j]);
            k = 0;
        }
    }
}

/* Tras haber dormido con las páginas ya puestas, NO se pueden quitar: otro hilo del proceso
 * puede estar dormido en una llamada con un puntero ya comprobado dentro del tramo (D351; la
 * barrera solo se miró al entrar) y, al despertar, el kernel fallaría en modo kernel. Se dejan
 * como páginas propias normales (lectura y escritura, sin ejecución: dar permisos no le hace
 * daño a nadie) y quien llama las devuelve con MEM_DEVOLVER, que sí espera a la barrera. */
static void deixar_privadas(espaco_t e, uint32_t ini, uint32_t paginas)
{
    espaco_ajustar(e, ini, paginas * PAGINA, PG_ESCRITA | PG_NX, 0);
    espaco_sincronizar(e);
}

static bool iguais(fis_t a, fis_t b)
{
    const uint32_t *x = fisica_mapear(a), *y = fisica_mapear(b);
    bool r = true;
    for (uint32_t i = 0; i < PAGINA / 4 && r; i++) r = x[i] == y[i];
    fisica_soltar((void *)y);
    fisica_soltar((void *)x);
    return r;
}

static void imagem_soltar(imagem_t *im)
{
    if (--im->refs) return;
    for (imagem_t **pp = &imagens; *pp; pp = &(*pp)->prox)
        if (*pp == im) { *pp = im->prox; break; }
    for (uint32_t i = 0; i < im->paginas; i++) memfisica_liberar(im->fis[i]);
    paginas_total -= im->paginas;
    n_imagens--;
    kfree(im);
}

static void mapa_tirar(mapa_t *m)
{
    for (mapa_t **pp = balde(m->e); *pp; pp = &(*pp)->prox)
        if (*pp == m) { *pp = m->prox; break; }
    mapeadas_total -= m->restantes;
    n_mapas--;
    imagem_soltar(m->im);
    kfree(m);
}

/* Revisión B1: la entrada de una página privada recién cargada tiene que ser exactamente la
 * que se puso: presente (no en la swap), propia, sin escritura y, con NX activo, con la NX
 * pedida. */
static bool entrada_esperada(pte_t v, bool exec)
{
    if (!(v & PG_PRESENTE) || troca_e_entrada(v)) return false;
    uint32_t f = pte_flags(v);
    if (f & (PG_EMPRESTADA | PG_ESCRITA)) return false;
    if (paginacao_nx() && (f & PG_NX) != (exec ? 0u : (uint32_t)PG_NX)) return false;
    return true;
}

int32_t imagens_mapear(processo_t *l, uint32_t ini, uint32_t n, uint32_t fd, bool exec, uint32_t desloc)
{
    if ((ini | n | desloc) & (PAGINA - 1) || !n || n / PAGINA > IMAGEM_PAGINAS) return ERRO_INVALIDO;
    espaco_t e = l->espaco;
    uint32_t paginas = n / PAGINA;

    /* 1. Sin tocar nada: el fichero (puede leer el disco: dormir). */
    char caminho[CAMINHO_MAX];
    uint32_t tamanho = 0, modificado = 0;
    int32_t r = arquivos_imagem_info(fd, caminho, sizeof caminho, &tamanho, &modificado);
    if (r < 0) return r;
    if (desloc >= tamanho || desloc + n < desloc) return ERRO_INVALIDO;
    if (!caminho_confiavel(caminho)) return ERRO_ACESSO;
    uint8_t *ponte = kmalloc(PAGINA);   /* revisión M2: uno por llamada, sin cerrojo */
    if (!ponte) return ERRO_SEM_MEMORIA;
    r = elf_compartivel(fd, tamanho, desloc, n, ponte);
    if (r < 0) { kfree(ponte); return r == ERRO_FORMATO ? ERRO_INVALIDO : r; }
    /* Revisión I1: tope global (una cuarta parte de la RAM): estas páginas no van a la swap. */
    estat_memoria_t est;
    memoria_estatisticas(&est);
    uint32_t teto = est.paginas_geridas / 4;
    if (paginas_total + paginas > teto) { kfree(ponte); return ERRO_LIMITE; }
    /* Ya sin dormir hasta asignar: el rango sigue sin asignar y sin mapas viejos encima (un
     * mapa a medias, tras un MEM_DEVOLVER parcial, todavía cuenta: no se mezclan dos). */
    for (uint32_t i = 0; i < paginas; i++)
        if (espaco_existe(e, ini + i * PAGINA)) { kfree(ponte); return ERRO_INVALIDO; }
    for (mapa_t *m = *balde(e); m; m = m->prox)
        if (m->e == e && m->virt < ini + n && ini < m->virt + m->paginas * PAGINA) { kfree(ponte); return ERRO_INVALIDO; }

    /* 2. Páginas privadas, de solo lectura para el programa (con los permisos finales de
     * ejecución), y el fichero dentro. */
    uint32_t flags = PG_USUARIO | (exec ? 0 : PG_NX);
    if (!espaco_alocar(e, ini, n, flags)) {
        kfree(ponte);
        soltar_privadas(e, ini, paginas);
        return ERRO_SEM_MEMORIA;
    }
    for (uint32_t i = 0; i < paginas && r == 0; i++) {
        uint32_t pos = desloc + i * PAGINA;
        if (pos >= tamanho) break;   /* lo que pasa del final, a cero (ya lo está) */
        uint32_t q = tamanho - pos < PAGINA ? tamanho - pos : PAGINA;
        int32_t lidos = arquivos_ler_em(fd, pos, ponte, q);
        if (lidos != (int32_t)q) { r = lidos < 0 ? lidos : ERRO_INVALIDO; break; }   /* fichero más corto */
        /* Después de dormir se vuelve a mirar dónde está (la swap pudo sacarla) y cómo está
         * (otro hilo pudo darle escritura: se sale ya, sin escribir). */
        fis_t f = espaco_traduzir(e, ini + i * PAGINA, NULL);
        void *p = espaco_pte(e, ini + i * PAGINA);
        if (!p || !f) { r = ERRO_SEM_MEMORIA; break; }
        if (!entrada_esperada(pte_ler(p), exec)) { r = ERRO_INVALIDO; break; }
        fisica_escrever(f & ~(fis_t)(PAGINA - 1), ponte, q);
    }
    kfree(ponte);

    /* 3. Sin dormir a partir de aquí. */
    fis_t *priv = r == 0 ? kmalloc(paginas * sizeof(fis_t)) : NULL;
    mapa_t *m = r == 0 ? kmalloc(sizeof *m) : NULL;
    if (r == 0 && (!priv || !m)) r = ERRO_SEM_MEMORIA;
    if (r == 0 && n_mapas >= MAPAS_MAX) r = ERRO_LIMITE;
    for (uint32_t i = 0; i < paginas && r == 0; i++) {
        /* Tiene que seguir presente y ser exactamente la privada que se puso (revisión B1:
         * sin escritura que otro hilo le haya dado con MEM_PROTEGER mientras se dormía). */
        if (!espaco_traduzir(e, ini + i * PAGINA, NULL)) { r = ERRO_SEM_MEMORIA; break; }
        void *p = espaco_pte(e, ini + i * PAGINA);
        pte_t v = p ? pte_ler(p) : 0;
        if (!entrada_esperada(v, exec)) { r = ERRO_INVALIDO; break; }
        priv[i] = pte_endereco(v);
    }
    if (r < 0) {
        kfree(priv);
        kfree(m);
        deixar_privadas(e, ini, paginas);
        return r;
    }

    uint32_t chave = hash_texto(caminho);
    imagem_t *im = NULL;
    for (imagem_t *x = imagens; x; x = x->prox) {
        if (x->obsoleta || x->chave != chave || x->tamanho != tamanho || x->modificado != modificado ||
            x->desloc != desloc || x->paginas != paginas || x->exec != exec)
            continue;
        uint32_t i = 0;
        while (i < paginas && iguais(x->fis[i], priv[i])) i++;
        if (i == paginas) { im = x; break; }
        x->obsoleta = true;   /* el fichero cambió sin cambiar su fecha ni su tamaño */
        klog("[SAGUI] imagens: %s (desde %u) cambio sin cambiar la fecha: objeto viejo apartado\n", caminho, desloc);
    }
    uint32_t pflags = PG_USUARIO | PG_EMPRESTADA | PG_ACESSADA | (exec ? 0 : PG_NX);
    int32_t resultado;
    if (im) {
        /* Las suyas en lugar de las privadas; las privadas se liberan cuando ninguna
         * CPU las tenga ya en la TLB. */
        for (uint32_t i = 0; i < paginas; i++) espaco_mapear(e, ini + i * PAGINA, im->fis[i], pflags);
        espaco_sincronizar(e);
        for (uint32_t i = 0; i < paginas; i++) memfisica_liberar(priv[i]);
        im->refs++;
        reutilizadas++;
        resultado = 1;
    } else {
        /* El tope se vuelve a mirar aquí: otros procesos pudieron crear objetos mientras esta
         * llamada dormía leyendo. */
        bool cabe = n_imagens < IMAGENS_MAX && paginas_total + paginas <= teto;
        im = cabe ? kmalloc(sizeof *im + paginas * sizeof(fis_t)) : NULL;
        if (!im) {
            kfree(priv);
            kfree(m);
            deixar_privadas(e, ini, paginas);
            return cabe ? ERRO_SEM_MEMORIA : ERRO_LIMITE;
        }
        im->chave = chave;
        im->tamanho = tamanho;
        im->modificado = modificado;
        im->desloc = desloc;
        im->paginas = paginas;
        im->exec = exec;
        im->obsoleta = false;
        im->refs = 1;
        /* Las privadas pasan a ser del objeto: mismo marco, entrada reescrita entera con los
         * permisos del objeto (revisión B1: nada de lo que hubiera antes) y la TLB de todas
         * las CPU al día antes de que otro proceso pueda encontrar el objeto. */
        for (uint32_t i = 0; i < paginas; i++) {
            im->fis[i] = priv[i];
            espaco_mapear(e, ini + i * PAGINA, priv[i], pflags);
        }
        espaco_sincronizar(e);
        im->prox = imagens;
        imagens = im;
        n_imagens++;
        paginas_total += paginas;
        criadas++;
        resultado = 0;
    }
    kfree(priv);
    m->e = e;
    m->virt = ini;
    m->paginas = m->restantes = paginas;
    m->im = im;
    m->prox = *balde(e);
    *balde(e) = m;
    n_mapas++;
    mapeadas_total += paginas;
    return resultado;
}

uint32_t imagens_desmapear(espaco_t e, uint32_t ini, uint32_t fim)
{
    uint32_t tiradas = 0;
    for (mapa_t *m = *balde(e); m; m = m->prox) {
        if (m->e != e || m->virt >= fim || ini >= m->virt + m->paginas * PAGINA) continue;
        uint32_t a = m->virt > ini ? m->virt : ini, b = m->virt + m->paginas * PAGINA;
        if (b > fim) b = fim;
        for (uint32_t v = a & ~(PAGINA - 1); v < b; v += PAGINA) {
            void *p = espaco_pte(e, v);
            if (!p) continue;
            pte_t x = pte_ler(p);
            /* Solo la entrada que pusimos: presente, prestada y con el marco del objeto. */
            if (!(x & PG_PRESENTE) || !(x & PG_EMPRESTADA) || pte_endereco(x) != m->im->fis[(v - m->virt) / PAGINA])
                continue;
            pte_esvaziar(p);
            if (espaco_atual() == e) pagina_invalidar(v);
            m->restantes--;
            mapeadas_total--;
            tiradas++;
        }
    }
    if (!tiradas) return 0;
    espaco_sincronizar(e);   /* ninguna CPU sigue usando las entradas viejas */
    for (mapa_t *m = *balde(e), *seg; m; m = seg) {
        seg = m->prox;
        if (m->e == e && !m->restantes) mapa_tirar(m);
    }
    return tiradas;
}

bool imagens_clonar(espaco_t de, espaco_t para)
{
    for (mapa_t *m = *balde(de); m; m = m->prox) {
        if (m->e != de) continue;
        mapa_t *n = kmalloc(sizeof *n);
        if (!n) return false;
        n->e = para;
        n->virt = m->virt;
        n->paginas = m->paginas;
        n->restantes = 0;
        n->im = m->im;
        m->im->refs++;
        bool ok = true;
        for (uint32_t i = 0; i < m->paginas && ok; i++) {
            uint32_t v = m->virt + i * PAGINA;
            void *p = espaco_pte(de, v);
            pte_t x = p ? pte_ler(p) : 0;
            /* Solo la entrada que puso imagens_mapear (MEM_DEVOLVER pudo quitar unas). */
            if (!(x & PG_PRESENTE) || !(x & PG_EMPRESTADA) || pte_endereco(x) != m->im->fis[i]) continue;
            ok = espaco_mapear(para, v, pte_endereco(x), pte_flags(x) & ~(uint32_t)(PG_ACESSADA | PG_SUJA));
            if (ok) n->restantes++;
        }
        /* En la lista aunque falle: destruir "para" lo suelta. Se inserta en la cabeza: si es la
         * misma lista que se recorre, queda detrás de "m" y no se vuelve a ver. */
        n->prox = *balde(para);
        *balde(para) = n;
        n_mapas++;
        mapeadas_total += n->restantes;
        if (!ok) return false;
    }
    return true;
}

void imagens_espaco_destruido(espaco_t e)
{
    for (mapa_t *m = *balde(e), *seg; m; m = seg) {
        seg = m->prox;
        if (m->e == e) mapa_tirar(m);
    }
}

uint32_t imagens_paginas_alheias(espaco_t e)
{
    /* Revisión I1: cada proceso paga su parte de cada objeto (restantes / refs); lo que no se
     * le cuenta es el resto. Un objeto que solo usa él se le cuenta entero. */
    uint32_t n = 0;
    for (mapa_t *m = *balde(e); m; m = m->prox)
        if (m->e == e) n += m->restantes - m->restantes / m->im->refs;
    return n;
}

void imagens_estatisticas(imagens_est_t *est)
{
    est->objetos = n_imagens;
    est->paginas = paginas_total;
    est->mapeadas = mapeadas_total;
    est->reutilizadas = reutilizadas;
    est->criadas = criadas;
}
