// SPDX-License-Identifier: GPL-2.0-or-later
/* cache_disco.h - caché de bloques común a los discos internos (D281).
 *
 * Una sola caché, por debajo de los sistemas de archivos (FAT32, ext2/3/4) y por encima del
 * driver de cada disco (ATA, AHCI, NVMe): bloques de 4 KB (8 sectores), LRU con un límite que
 * depende de la RAM, lectura anticipada en las lecturas secuenciales y escritura diferida con
 * vaciado en orden de grabación (cada bloque sucio se graba en el orden en que se escribió, de
 * modo que un corte de luz deja siempre un prefijo de lo escrito, como con la escritura
 * directa de antes). Se vacía: cada pocos segundos (tarea "sync-disco"), al desmontar, al
 * apagar o reiniciar, con "sync" y cuando hay demasiados bloques sucios. */
#ifndef SAGUI_CACHE_DISCO_H
#define SAGUI_CACHE_DISCO_H
#include "tipos.h"
#include "disco.h"

/* ¿Pasa este disco entero (no partición) por la caché? Solo los internos (ATA, SATA, NVMe): un
 * pendrive se retira sin avisar y un CD cambia de medio. */
bool cache_disco_aplica(disco_t *raiz);

/* "lba" y "n" son sectores del disco entero. Devuelven 0 o un código ERRO_*. */
int  cache_disco_ler(disco_t *raiz, uint32_t lba, uint32_t n, void *buf);
/* diferir = false: se graba ya (acceso en bruto, discos reservados); true: queda sucio en la caché. */
int  cache_disco_escrever(disco_t *raiz, uint32_t lba, uint32_t n, const void *buf, bool diferir);

int  cache_disco_sincronizar(disco_t *raiz);   /* graba lo sucio de ese disco; NULL = todos */
void cache_disco_descartar(disco_t *raiz);     /* sin grabar: el disco ya no está */
void cache_disco_tarefa_iniciar(void);         /* la tarea que vacía cada pocos segundos */

typedef struct {
    uint32_t blocos, max_blocos, sujos, max_sujos;
    uint32_t acertos, falhas, anticipados, gravados, vaciados, erros, diretos;
    /* D350: bloques con los datos en la memoria alta, bloques devueltos a los programas (reclamo
     * de memfisica_alocar_usuario y tarea), reclamos atendidos y reclamos con la caché ocupada,
     * páginas altas que la caché deja siempre libres. */
    uint32_t altos, cedidos, reclamos, reclamos_ocupada, reserva;
} cache_disco_est_t;
void cache_disco_estatisticas(cache_disco_est_t *e);
void cache_disco_registar(void);                /* una línea [CACHE] en el log con las cifras */

#endif
