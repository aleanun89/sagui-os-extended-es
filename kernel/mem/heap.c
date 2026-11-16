// SPDX-License-Identifier: GPL-3.0-only
/* heap.c - kmalloc/kfree: clases de tamaño para bloques pequeños y páginas
 * enteras para los grandes. Ver D008. */
#include "memoria.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "texto.h"

#define CLASSES        8          /* 16, 32, ..., 2048 bytes */
#define MENOR_CLASSE   16
#define MAIOR_PEQUENO  2048
#define ASSIN_USADO    0x5A6C      /* marca de bloque en uso */
#define ASSIN_LIVRE    0x0F1E      /* marca de bloque liberado */
#define CLASSE_GRANDE  0xFF

/* Cabecera de 8 bytes antes de cada bloque entregado. Mantiene el puntero
 * devuelto alineado en 8. En un bloque pequeño libre, el campo "elo" apunta
 * al siguiente bloque libre de la misma clase; así la firma y la clase
 * siguen intactas mientras el bloque está en la lista, y una segunda
 * liberación se reconoce. En un bloque grande, "elo" guarda el número de páginas. */
typedef struct cabeca {
    uint16_t assinatura;
    uint8_t  classe;
    uint8_t  reservado;
    uint32_t elo;
} cabeca_t;

static cabeca_t *listas[CLASSES];
static uint32_t paginas_classes, blocos_usados, grandes_usados;

static int classe_de(size_t total)
{
    int c = 0;
    size_t tam = MENOR_CLASSE;
    while (tam < total) { tam <<= 1; c++; }
    return c;
}

/* Corta una página nueva en bloques de la clase c y los pone todos en la lista. */
static bool abastecer(int c)
{
    uint32_t pag = memfisica_alocar();
    if (!pag) return false;
    paginas_classes++;
    size_t tam = (size_t)MENOR_CLASSE << c;
    for (size_t off = 0; off + tam <= PAGINA; off += tam) {
        cabeca_t *b = (cabeca_t *)(pag + off);
        b->assinatura = ASSIN_LIVRE;
        b->classe = (uint8_t)c;
        b->elo = (uint32_t)listas[c];
        listas[c] = b;
    }
    return true;
}

void *kmalloc(size_t n)
{
    if (n == 0) return NULL;
    size_t total = n + sizeof(cabeca_t);
    bool i = int_salva_desliga();
    cabeca_t *h;
    if (total <= MAIOR_PEQUENO) {
        int c = classe_de(total);
        if (!listas[c] && !abastecer(c)) { int_restaura(i); return NULL; }
        h = listas[c];
        listas[c] = (cabeca_t *)h->elo;
        h->elo = 0;
        blocos_usados++;
    } else {
        uint32_t pags = (uint32_t)((total + PAGINA - 1) / PAGINA);
        uint32_t end = memfisica_alocar_seguidas(pags);
        if (!end) { int_restaura(i); return NULL; }
        h = (cabeca_t *)end;
        h->classe = CLASSE_GRANDE;
        h->elo = pags;
        grandes_usados++;
    }
    h->assinatura = ASSIN_USADO;
    int_restaura(i);
    return h + 1;
}

void *kcalloc(size_t n)
{
    void *p = kmalloc(n);
    if (p) memset(p, 0, n);
    return p;
}

void kfree(void *p)
{
    if (!p) return;
    cabeca_t *h = (cabeca_t *)p - 1;
    bool i = int_salva_desliga();
    if (h->assinatura != ASSIN_USADO) {
        klog("[SAGUI] AVISO: kfree(%p) %s\n", p,
             h->assinatura == ASSIN_LIVRE ? "liberado duas vezes" : "nao veio do kmalloc");
        int_restaura(i);
        return;
    }
    h->assinatura = ASSIN_LIVRE;
    if (h->classe == CLASSE_GRANDE) {
        grandes_usados--;
        memfisica_liberar_seguidas((uint32_t)h, h->elo);
    } else if (h->classe < CLASSES) {
        h->elo = (uint32_t)listas[h->classe];
        listas[h->classe] = h;
        blocos_usados--;
    }
    int_restaura(i);
}

void memoria_estatisticas(estat_memoria_t *e)
{
    e->ram_total = 0;
    for (uint32_t i = 0; i < info_boot.mapa_qtd; i++)
        if (mapa_memoria[i].tipo == E820_LIVRE && (mapa_memoria[i].extra & 1))
            e->ram_total += mapa_memoria[i].tamanho;
    e->altas_geridas = memfisica_geridas_alta();   /* D348 */
    e->altas_livres = memfisica_livres_alta();
    e->superiores_geridas = memfisica_geridas_superior();   /* D349 */
    e->superiores_livres = memfisica_livres_superior();
    e->paginas_geridas = memfisica_geridas() + e->altas_geridas;
    e->paginas_livres = memfisica_livres() + e->altas_livres;
    e->heap_paginas = paginas_classes;
    e->heap_blocos_usados = blocos_usados;
    e->heap_grandes = grandes_usados;
}
