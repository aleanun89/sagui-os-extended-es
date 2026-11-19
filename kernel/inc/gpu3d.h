// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d.h - 3D por la GPU: la capa común y lo que comparten sus drivers (D285).
 *
 * Reparto:
 *   - SaguiGL (userland/libsgl) transforma, ilumina y recorta con la CPU, y
 *     manda triángulos en coordenadas del destino (gpu3d_vert_t de abi.h);
 *   - la capa común (kernel/gui/gpu3d.c) elige el driver por el PCI, lo pone a
 *     prueba la primera vez que se usa (autoprueba), lleva el dueño del motor,
 *     el vigilante y las medidas, y vuelve al software si algo falla;
 *   - el driver de la tarjeta (kernel/dispositivos/gpu3d_*.c) traduce a las
 *     órdenes de su GPU.
 *
 * Piezas que comparten los drivers, pensadas para que duren (64 bits en la
 * v10.0 y después amdgpu/LinuxKPI/Vulkan: ver docs/gpu3d.md):
 *   - memoria de GPU (g3d_arena_t): una zona con direcciones de GPU propias,
 *     separada de la dirección de la CPU, repartida en bloques;
 *   - búferes (g3d_buffer_t): un bloque con su uso (destino, Z, textura, lote
 *     de órdenes), su dirección de GPU y, si la CPU lo ve, su ventana;
 *   - colas de órdenes (g3d_fila_t): palabras que se preparan en RAM y se
 *     entregan de una vez al motor;
 *   - vallas (g3d_vala_t): un número de secuencia por entrega; se espera a una
 *     valla, no «a la GPU».
 * Las direcciones de GPU y los tamaños van en uint64_t aunque hoy quepan en 32
 * bits. Nada de esto usa coma flotante (el kernel no la tiene). */
#ifndef SAGUI_GPU3D_H
#define SAGUI_GPU3D_H
#include "tipos.h"
#include "abi.h"
#include "acel2d.h"
#include "pci.h"
#include "gpu3d_diag.h"

/* Lo que pide la cabecera de arranque (CAB_GPU3D; mkimagem.py --gpu3d). */
#define GPU3D_CAB_MODO      0x07
#define GPU3D_CAB_AUTO      0     /* por el PCI */
#define GPU3D_CAB_NAO       1     /* --gpu3d=no: SaguiGL siempre por software */
#define GPU3D_CAB_SIMULADO  2     /* --gpu3d=simulado: un driver de mentira por la CPU (pruebas en QEMU) */
#define GPU3D_CAB_INTEL     3     /* --gpu3d=intel: aunque el 2D haya fallado */
#define GPU3D_CAB_RADEON    4     /* --gpu3d=radeon (D284, D287): el 3D de las R7xx y R5xx se prueba con el primer uso (sin esto, solo a mano) */
#define GPU3D_CAB_ANEL      0x08  /* órdenes en el anillo en vez de lotes */
#define GPU3D_CAB_P_PIXEL   0x10  /* pruebas: la autoprueba ve un píxel distinto */
#define GPU3D_CAB_P_TEMPO   0x20  /* pruebas: la GPU deja de responder tras la autoprueba */
#define GPU3D_CAB_GTT       0x40  /* D358: RAM del sistema en el GTT (Intel Gen3; --gpu3d-gtt, «gpu3d encender gtt») */
#define GPU3D_CAB_SEM_GTT   0x80  /* D406: no encender sola la GART de las R7xx confirmadas («gpu3d encender sin-gtt») */

#define GPU3D_PRAZO_MS      2000  /* vigilante: lo más que se espera a una valla (D158) */

/* Resultado de las funciones de un driver. */
#define G3D_OK        0
#define G3D_FALHA   (-1)          /* la GPU no respondió o rechazó la orden: la capa apaga el 3D */
#define G3D_NAO       1           /* no soportado o no cabe: sigue el software, el 3D no se apaga */

/* ------------------------------------------------------------ memoria de GPU */

#define G3D_BLOCOS 48
typedef struct { uint64_t off, bytes; bool usado; } g3d_bloco_t;
typedef struct {
    uint64_t    gpu;             /* dirección de GPU del principio de la zona */
    uint64_t    bytes;
    uint8_t    *cpu;             /* ventana de la CPU sobre la zona (NULL: no se ve) */
    g3d_bloco_t b[G3D_BLOCOS];   /* ordenados por desplazamiento, sin huecos entre ellos */
    int         n;
} g3d_arena_t;

void     g3d_arena_iniciar(g3d_arena_t *a, uint64_t gpu, uint8_t *cpu, uint64_t bytes);
uint64_t g3d_arena_livre(const g3d_arena_t *a);          /* bytes sin usar (suma) */

/* ------------------------------------------------------------ búferes */

#define G3D_BUF_ALVO     1       /* destino de color */
#define G3D_BUF_Z        2
#define G3D_BUF_TEXTURA  3
#define G3D_BUF_LOTE     4       /* órdenes */
typedef struct {
    bool      usado;
    uint8_t   tipo;              /* G3D_BUF_* */
    uint64_t  gpu;               /* dirección de GPU */
    uint64_t  bytes;
    uint8_t  *cpu;               /* dirección en el kernel (NULL: no se ve) */
    uint32_t  passo;             /* bytes por fila (destino, Z, textura) */
    int       larg, alt, bpp;
} g3d_buffer_t;

/* alinhamento: potencia de 2. false: no cabe. */
bool g3d_buffer_criar(g3d_arena_t *a, g3d_buffer_t *b, uint8_t tipo, uint64_t bytes, uint32_t alinhamento);
void g3d_buffer_soltar(g3d_arena_t *a, g3d_buffer_t *b);

/* ------------------------------------------------------------ colas de órdenes */

typedef struct { uint32_t *p; uint32_t n, max; bool transbordou; } g3d_fila_t;
static inline void g3d_fila_iniciar(g3d_fila_t *f, uint32_t *p, uint32_t max) { f->p = p; f->n = 0; f->max = max; f->transbordou = false; }
static inline void g3d_por(g3d_fila_t *f, uint32_t v) { if (f->n < f->max) f->p[f->n++] = v; else f->transbordou = true; }

/* ------------------------------------------------------------ vallas */

typedef uint32_t g3d_vala_t;     /* número de secuencia de una entrega; 0 = ninguna */

/* ------------------------------------------------------------ un driver */

typedef struct {
    const char *nome;
    /* Pone el motor en marcha (variante: GPU3D_CAB_ANEL...). No dibuja nada todavía. */
    int  (*iniciar)(const pci_disp_t *d, int geracao, const acel_tela_t *t, uint32_t variante, char *motivo, int n);
    /* Crea (o cambia) el destino de color y su Z. zbits es lo deseado: el driver elige lo que tiene. */
    int  (*alvo)(int larg, int alt, int zbits);
    int  (*limpar)(uint32_t mascara, uint32_t cor);
    /* argb NULL: suelta el hueco. */
    int  (*textura)(int vaga, int larg, int alt, const uint32_t *argb);
    /* Estado y triángulos juntos: una entrega a la cola, con su valla. */
    int  (*desenhar)(const gpu3d_estado_t *e, const gpu3d_vert_t *v, int n, g3d_vala_t *vala);
    int  (*esperar)(g3d_vala_t vala, uint32_t ms);
    /* Espera a lo pendiente y copia el destino (0x00RRGGBB) a la RAM. */
    int  (*ler)(uint32_t *dest, int passo_px);
    void (*desligar)(void);
    void (*detalle)(const char *fase);     /* registros clave al log [GPU3D] */
    void (*memoria)(uint32_t *total_kb, uint32_t *livre_kb);
    /* D390: diagnóstico común (gpu3d_diag.h). «conf»: nombre corto del fichero de ajustes
     * (/sistema/conf/gpu3d-<conf>.conf); «faixas»: los registros que ese fichero puede tocar
     * (n_faixas 0: ninguno; nunca el MC, los relojes, la energía ni la pantalla). El driver
     * escribe gpu3d_ajustes en su cola: los G3D_AJ_INICIO tras el estado fijo y los
     * G3D_AJ_DIBUJO con el estado de cada dibujo. */
    const char        *conf;
    const g3d_faixa_t *faixas;
    int                n_faixas;
    /* Lo que la CPU ve del destino actual (cpu NULL: nada). Sin destino, las dos vacías. */
    void (*vistas)(g3d_vista_t *cor, g3d_vista_t *z);
} gpu3d_driver_t;

/* D390: los ajustes del fichero del driver, leídos por la capa antes de iniciar() y ya
 * filtrados por sus rangos (n = 0 sin fichero). */
extern g3d_ajustes_t gpu3d_ajustes;

extern const gpu3d_driver_t gpu3d_intel;   /* Gen3: 915, 945, G33, Pineview (gpu3d_intel.c) */
extern const gpu3d_driver_t gpu3d_r600;    /* Radeon R7xx por el CP (gpu3d_r600.c, D284): no se enciende solo */
extern const gpu3d_driver_t gpu3d_r500;    /* Radeon R5xx: X1300 a X1950 (gpu3d_r500.c, D287) */
extern const gpu3d_driver_t gpu3d_sim;     /* de mentira, por la CPU (gpu3d_sim.c): solo pruebas */

/* D358: lo que quedó en marcha en la Intel (GPU3D_VAR_*) y los KB de RAM en el GTT. */
void gpu3d_intel_estado(uint32_t *variante, uint32_t *ram_kb);
/* D406: lo mismo para las R7xx (GPU3D_VAR_GTT si la vuelta del cuadro va por la GART). */
void gpu3d_r600_estado(uint32_t *variante, uint32_t *ram_kb);

/* ------------------------------------------------------------ la capa */

void    gpu3d_iniciar(void);                 /* al arrancar: solo mira el PCI y la cabecera */
int32_t gpu3d_info(uint32_t ptr, uint32_t n);
int32_t gpu3d_op(uint32_t ptr, uint32_t n);
int32_t gpu3d_prova(uint32_t ptr, uint32_t n);
int32_t gpu3d_ctl(uint32_t op);
const char *gpu3d_resumo(void);              /* una línea para el arranque */

/* Coma flotante sin coma flotante: bits IEEE-754 <-> coma fija con «frac» bits. */
uint32_t g3d_float_de_fixo(int32_t v, int frac);
int32_t  g3d_fixo_de_float(uint32_t bits, int frac);

#endif
