// SPDX-License-Identifier: GPL-2.0-or-later
/* amd.h - el armazón del driver 3D de las GPU de AMD (mejora 054, parte B2; D158).
 *
 * Esto es la estructura, no el motor: la GPU todavía no dibuja nada por
 * hardware (eso es la parte B1 de la mejora 054, sin hacer). Lo que hay aquí
 * es lo que hace falta para que, cuando llegue el motor, las familias se
 * añadan con una entrada de tabla y no con código nuevo:
 *
 *   - la tabla de familias (userland/libamd/amd.c): los identificadores de
 *     dispositivo de cada familia, su generación, los registros de
 *     identificación que se leen, la ISA de sus shaders, qué partes de B1
 *     reutiliza y cuáles no, y si hace falta microcódigo;
 *   - la identificación: se busca la GPU de AMD en el bus PCI, se mapea su
 *     BAR (con la llamada del kernel, D158) y se leen sus registros;
 *   - el motor: el camino por defecto es el de SaguiGL por software; la
 *     aceleración por hardware solo se usa si la GPU está identificada, la
 *     opción de arranque no la ha quitado y el motor existe;
 *   - el vigilante: toda operación con la GPU tiene un plazo máximo; si se
 *     pasa, la GPU queda marcada como fallida y el motor vuelve al software
 *     (un fallo de la GPU nunca cuelga el sistema).
 *
 * La tabla de familias es la misma idea que la de userland/inc/gpuid.h (la
 * parte A1, D104), pero con la información que hace falta para programar la
 * GPU: los registros y las diferencias entre familias. */
#ifndef SAGUI_AMD_H
#define SAGUI_AMD_H

#include "sagui.h"

/* Las familias de la mejora 054: B1 es la primera (R6xx/R7xx) y B2 son las
 * siguientes (Evergreen, R5xx y R300/R420). */
typedef enum {
    AMD_FAMILIA_NENHUMA = 0,
    AMD_FAMILIA_R300,        /* X300-X850 (R300, R420) */
    AMD_FAMILIA_R5XX,        /* X1xxx (R520, RV5xx): el Amilo */
    AMD_FAMILIA_R6XX,        /* HD 2xxx-4xxx, HD 3200/4200: el dv7 */
    AMD_FAMILIA_EVERGREEN,   /* HD 5xxx */
    AMD_FAMILIA_POSTERIOR    /* Northern Islands, GCN, RDNA: fuera del plan */
} amd_familia_t;

/* Las partes de B1 (mejora 054) que una familia puede reutilizar. */
#define AMD_B1_LEITURA   0x01   /* B1.1: leer los registros de la GPU */
#define AMD_B1_COMANDOS  0x02   /* B1.2: el procesador de comandos con microcódigo */
#define AMD_B1_GART      0x04   /* B1.3: GART y VRAM */
#define AMD_B1_2D        0x08   /* B1.4: el 2D por la GPU */
#define AMD_B1_3D        0x10   /* B1.5: el 3D por la GPU */
#define AMD_B1_MOTOR     0x20   /* B1.6: el motor r600 de SaguiGL */

#define AMD_FAIXAS_MAX 16

/* Un rango de identificadores de dispositivo. */
typedef struct { unsigned short a, b; } amd_faixa_t;

/* Una entrada de la tabla de familias. */
typedef struct {
    amd_familia_t familia;
    const char *nome;          /* «AMD Evergreen» */
    const char *detalhe;       /* «Radeon HD 5000, Ontario/Llano» */
    unsigned    geracao;       /* 1 = R3xx, 2 = R5xx, 3 = R6xx, 4 = Evergreen */
    const char *chip;          /* «RV670», «R600», «Cedaro»... */
    /* Los registros de identificación:
     *   mm_index: el registro donde se escribe el índice de un registro
     *             indirecto; mm_data: el de los datos (mm_index + 4);
     *   id_registro y rev_registro: los registros directos del identificador
     *             y de la revisión (-1 = no hay, se leen del PCI);
     *   grbm_indice: el índice del estado del bus maestro de registros
     *             (-1 = distinto en esta familia, a confirmar). */
    unsigned    mm_index;
    unsigned    mm_data;
    int         id_registro;   /* -1 = el identificador no está directo */
    int         rev_registro;
    int         subsis_registro;
    int         grbm_indice;
    unsigned    bar_registros; /* el BAR donde están los registros (0) */
    amd_faixa_t faixas[AMD_FAIXAS_MAX];   /* los device id de la familia */
    unsigned    n_faixas;
    /* Lo que la familia necesita saber para programarla. */
    int         microcodigo;   /* 1 = hace falta microcódigo (PFP/ME) */
    const char *isa;           /* la ISA de los shaders */
    unsigned    partes_b1;     /* qué partes de B1 reutiliza (AMD_B1_*) */
    const char *reutiliza;     /* y cómo */
    const char *falta;         /* qué falta de B1 para esta familia */
} amd_familia_info_t;

/* El estado de la aceleración. */
typedef enum {
    AMD_ACEL_SIN_PROBAR = 0,  /* todavía no se ha mirado */
    AMD_ACEL_PERMITIDA,       /* se podría acelerar */
    AMD_ACEL_DESACTIVADA,     /* la opción de arranque lo quitó (SGL_MOTOR=software) */
    AMD_ACEL_CAIDA,           /* la GPU falló: se vuelve al motor por software */
    AMD_ACEL_NO_GPU           /* no hay ninguna GPU AMD */
} amd_acel_t;

/* El motor que está en uso. */
typedef enum {
    AMD_MOTOR_SOFTWARE = 0,   /* el motor por software de SaguiGL (el camino por defecto) */
    AMD_MOTOR_GPU = 1         /* el motor por la GPU (todavía no existe) */
} amd_motor_t;

/* Lo que se sabe de la GPU. */
typedef struct {
    int         tem_gpu;          /* 1 = hay una GPU AMD en el bus PCI */
    amd_familia_t familia;
    const amd_familia_info_t *info;   /* la entrada de la tabla (NULL si no se sabe) */
    unsigned    barramento, dispositivo, funcao;
    unsigned    modelo;           /* device id del PCI */
    unsigned    revisao;          /* revision id del PCI */
    unsigned    bar;              /* el BAR donde están los registros */
    uint32_t    mapa;             /* la dirección virtual del mapa */
    unsigned    tamanho;          /* los bytes mapeados */
    int         mapeada;          /* 1 = el BAR ya está mapeado */
    int         mapa_escritura;   /* 1 = el mapa se pidió con permiso de escritura */
    int         leu_registros;    /* 1 = se han leído los registros de identificación */
    unsigned    device_id;        /* el DEVICE_ID del MMIO (0 si no está) */
    unsigned    rev_mmio;         /* el REV_ID del MMIO */
    unsigned    subsis_id;        /* el subsistema */
    unsigned    subsis_vendor;
    unsigned    grbm_status;      /* el estado del bus maestro de registros */
    int         tem_grbm;         /* 1 = se pudo leer */
    char        erro[80];         /* por qué no se pudo leer (vacío si todo bien) */
} amd_gpu_t;

/* ---- el motor ---- */

/* Lee del kernel el estado de la aceleración (INFO_MOTOR_GPU) y decide. */
void        amd_iniciar(void);
amd_motor_t amd_motor(void);
const char *amd_motor_nome(void);
amd_acel_t  amd_acel(void);
const char *amd_acel_nome(void);
const char *amd_motivo(void);          /* por qué no se acelera ("" si sí) */
unsigned    amd_plazo_vigilante(void); /* el plazo máximo del vigilante, en ms */

/* La caída al motor por software: la GPU falló y no se vuelve a intentar. */
void amd_cair_al_software(const char *motivo);

/* ---- la identificación ---- */

/* La tabla de familias. Devuelve cuántas hay. */
const amd_familia_info_t *const *amd_tabela(int *n);
/* La familia de un device id (NULL si no se sabe). */
const amd_familia_info_t *amd_buscar_familia(unsigned modelo);

/* Busca la GPU de AMD, mapea su BAR y lee sus registros de identificación.
 * Devuelve 0 si todo fue bien; un número negativo si no. */
int amd_identificar(amd_gpu_t *g);

/* Lee los registros de identificación de la familia (DEVICE_ID, REV_ID,
 * SUBSYS_ID y el estado del bus maestro de registros). El BAR tiene que estar
 * mapeado ya (g->mapa). Sirve también para probar la lectura con un
 * dispositivo que no sea una GPU. */
int amd_leer_registros(amd_gpu_t *g);

/* La familia por su nombre («AMD Evergreen»); NULL si no hay ninguna con ese
 * nombre. */
const amd_familia_info_t *amd_familia_nome(const char *nome);

/* ---- los BAR (el kernel mapea; el programa no ve direcciones físicas) ---- */

int  amd_mapear_bar(unsigned barramento, unsigned dispositivo, unsigned funcao, unsigned bar,
                    unsigned desloc, unsigned tamanho, int escrever, uint32_t *endereco);
void amd_desejar_bar(uint32_t endereco);

/* ---- el vigilante ---- */

/* La función que lee el registro que se espera. */
typedef uint32_t (*amd_leitor_t)(amd_gpu_t *g, void *contexto);

/* Espera a que un registro llegue a un valor, con un plazo máximo (0 = el
 * plazo del vigilante). Devuelve 0 si llegó a tiempo; -1 si se pasó el plazo
 * (la GPU queda marcada como fallida y el motor vuelve al software). */
int amd_esperar(amd_gpu_t *g, uint32_t prazo_ms, amd_leitor_t ler, void *contexto, uint32_t valor);

/* El vigilante sobre una operación cualquiera: la ejecuta y, si se pasa del
 * plazo, da el mismo resultado que la caída al software. */
typedef int (*amd_operacao_t)(amd_gpu_t *g, void *contexto);
int amd_vigilar(amd_gpu_t *g, uint32_t prazo_ms, amd_operacao_t op, void *contexto);

/* Un lector de registro directo (para el vigilante). */
uint32_t amd_ler_registro(amd_gpu_t *g, void *contexto);
/* Un lector simulado, que nunca cambia: sirve para probar el vigilante sin
 * GPU (la opción --simular del programa `amd`). */
uint32_t amd_ler_simulado(amd_gpu_t *g, void *contexto);

#endif
