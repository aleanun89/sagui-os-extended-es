// SPDX-License-Identifier: GPL-3.0-only
/* endurecer - el kernel no puede fallar al usar la memoria de un programa que
 * otro hilo suyo quita o protege mientras la llamada duerme (D351, H353).
 *
 * Un read() comprueba el buffer (usuario_ok) al entrar y escribe en él
 * después; entre medias puede dormir dentro del kernel (el disco: ata.c cede
 * el turno). Antes de la D351, otro hilo del mismo proceso podía quitarle en
 * ese hueco la escritura (memoria_proteger, mprotect) o las páginas
 * (MEM_DEVOLVER, free) y el kernel fallaba al copiar: pantalla de error.
 *
 *   - dos hilos leen sin parar trozos de 256 KB (lectura directa del disco,
 *     sin la caché) de un fichero grande (/apps/bin, /bin o /lib): uno en un buffer del área
 *     de datos y otro en un buffer de su propia pila (que crece al copiar);
 *   - el principal, a la vez, quita y devuelve la escritura del buffer y lo
 *     devuelve al kernel y lo vuelve a pedir, durante unos segundos;
 *   - al final, una lectura normal trae los mismos datos que al principio.
 *
 * Acaba con "RESULTADO endurecer: ...". Con --smp los hilos van en otras CPU. */
#include "sagui.h"
#include "verifica.h"

#define KB     1024u
#define TROZO  (256u * KB)   /* desde 128 KB el pedido va directo al disco (cache_disco.c) */
#define SEGUNDOS 6u

static char arquivo[96];
static uint8_t *buf;                 /* en el área de datos, alineado a página */
static volatile int parar;
static volatile unsigned lidas_dados, lidas_pilha, erros_dados, erros_pilha;
static volatile unsigned protecoes, devolucoes, falhas_principal;

/* El fichero más grande de las carpetas de programas del disco (lo que hay depende de la compilación). Desde
 * D379 las aplicaciones van dentro de SAGUI-RAIZ, y con la raíz en disco /bin y /lib también son del disco: se
 * mira en las tres y se dice cuántas entradas vio en cada una. Los ejecutables los da listar como TIPO_PROGRAMA,
 * no como TIPO_ARQUIVO (por eso antes no encontraba ninguno: solo quedaban scripts pequeños). */
static unsigned escolher_arquivo(void)
{
    static const char *const pastas[] = { "/apps/bin", "/bin", "/lib" };
    entrada_pasta_t e;
    unsigned maior = 0;
    for (unsigned p = 0; p < sizeof pastas / sizeof pastas[0]; p++) {
        int n = 0;
        for (int i = 0; listar(pastas[p], i, &e) > 0; i++, n++) {
            char c[96];
            formatar_texto(c, sizeof c, "%s/%s", pastas[p], e.nome);
            unsigned tam = e.tamanho;
            bool arq = e.tipo == TIPO_ARQUIVO || e.tipo == TIPO_PROGRAMA;   /* los ejecutables salen como programa */
            if (arq && tam > maior) {
                maior = tam;
                formatar_texto(arquivo, sizeof arquivo, "%s", c);
            }
        }
        imprimir("ENDURECER %s: %d entradas\n", pastas[p], n);
    }
    return maior;
}

static void *leitor_dados(void *x)
{
    (void)x;
    int fd = abrir(arquivo, ABRIR_LER);
    if (fd < 0) return (void *)1;
    while (!parar) {
        int n = ler(fd, buf, TROZO);
        if (n < 0) erros_dados++;           /* el buffer estaba protegido o devuelto al entrar: ERRO_ENDERECO */
        else if (n < (int)TROZO) posicionar(fd, 0, POS_INICIO);
        else lidas_dados++;
    }
    fechar(fd);
    return 0;
}

static __attribute__((noinline)) int ler_na_pilha(int fd)
{
    uint8_t local[TROZO];                    /* sin tocar: usuario_ok la hace crecer */
    int n = ler(fd, local, TROZO);
    return n == (int)TROZO && local[0] == 0x7F && local[1] == 'E' ? n : (n < 0 ? n : 0);
}

static void *leitor_pilha(void *x)
{
    (void)x;
    int fd = abrir(arquivo, ABRIR_LER);
    if (fd < 0) return (void *)1;
    while (!parar) {
        posicionar(fd, 0, POS_INICIO);
        int n = ler_na_pilha(fd);
        if (n > 0) lidas_pilha++;
        else erros_pilha++;
    }
    fechar(fd);
    return 0;
}

static uint32_t soma(const uint8_t *p, unsigned n)
{
    uint32_t h = 2166136261u;   /* FNV-1a */
    for (unsigned i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

static uint32_t ler_referencia(void)
{
    int fd = abrir(arquivo, ABRIR_LER);
    if (fd < 0) return 0;
    int n = ler(fd, buf, TROZO);
    fechar(fd);
    return n == (int)TROZO ? soma(buf, TROZO) : 0;
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    unsigned tam = escolher_arquivo();
    imprimir("ENDURECER fichero %s (%u bytes)\n", arquivo[0] ? arquivo : "-", tam);
    verifica(tam >= 2 * TROZO, "hay un fichero grande en /apps/bin, /bin o /lib (lectura del disco)");
    if (tam < 2 * TROZO) return resultado("endurecer");

    int base = memoria_paginas(MEM_RESERVAR, 0, TROZO);
    if ((unsigned)base >= 0xFFFFF000u || memoria_paginas(MEM_ASIGNAR, (void *)base, TROZO) < 0) {
        verifica(false, "reservar el buffer");
        return resultado("endurecer");
    }
    buf = (uint8_t *)base;
    uint32_t ref = ler_referencia();
    verifica(ref != 0, "lectura de referencia");

    thread_t *a = thread_create(leitor_dados, 0, 0);
    thread_t *b = thread_create(leitor_pilha, 0, 0);
    verifica(a && b, "dos hilos lectores");

    unsigned fim = tempo_ms() + SEGUNDOS * 1000u;
    for (unsigned vez = 0; tempo_ms() < fim; vez++) {
        if (vez & 1) {
            /* Quitar la escritura mientras el lector duerme en el disco. */
            if (memoria_proteger(buf, TROZO, 0) < 0) falhas_principal++;
            dormir(10);
            if (memoria_proteger(buf, TROZO, MEM_PROT_ESCRITA) < 0) falhas_principal++;
            protecoes++;
        } else {
            /* Devolver las páginas (como un free grande) y volver a pedirlas. */
            if (memoria_paginas(MEM_DEVOLVER, buf, TROZO) < 0) falhas_principal++;
            dormir(10);
            if (memoria_paginas(MEM_ASIGNAR, buf, TROZO) < 0) falhas_principal++;
            devolucoes++;
        }
        dormir(vez % 3);
    }
    parar = 1;
    int ra = a ? thread_join(a) : 1, rb = b ? thread_join(b) : 1;
    imprimir("ENDURECER lecturas %u (datos) %u (pila); rechazadas %u (datos) %u (pila); "
             "protecciones %u, devoluciones %u\n",
             lidas_dados, lidas_pilha, erros_dados, erros_pilha, protecoes, devolucoes);
    verifica(ra == 0 && rb == 0, "los dos lectores abrieron el fichero y acabaron");
    verifica(falhas_principal == 0, "proteger, devolver y asignar no fallan");
    verifica(protecoes > 20 && devolucoes > 20, "muchas protecciones y devoluciones durante las lecturas");
    verifica(lidas_dados > 5 && lidas_pilha > 5, "los dos lectores leyeron del disco");
    verifica(erros_pilha == 0, "las lecturas en la pila nunca fallan");
    verifica(ler_referencia() == ref, "una lectura al final trae los mismos datos");
    return resultado("endurecer");
}
