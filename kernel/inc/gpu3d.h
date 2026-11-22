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

/* D432: una presentación directa. Coordenadas ya recortadas: la fuente dentro de su búfer y el
 * destino dentro de la pantalla visible. */
#define G3D_APRES_BANDAS  0x100u   /* (interno) antes, pintar de negro la pantalla fuera del destino */
typedef struct {
    int      vaga;                 /* hueco de textura, o < 0: el destino principal */
    int      ox, oy, ol, oa;       /* en píxeles de la fuente */
    int      dx, dy, dl, da;       /* en píxeles de la pantalla */
    uint32_t flags;                /* G3D_APRES_* (abi.h) y G3D_APRES_BANDAS */
} g3d_apres_t;
/* Los rectángulos negros (hasta 4: arriba, abajo, izquierda, derecha) que rodean el destino en una
 * pantalla de larg x alt; devuelve cuántos (los vacíos no cuentan). r[i] = { x, y, l, a }. */
int g3d_apres_bandas(const g3d_apres_t *p, int larg, int alt, int r[4][4]);
/* Seis vértices v1 (dos triángulos) que llevan el rectángulo de la fuente (de un búfer de fl x fa
 * texels, coordenadas de textura de 0 a 1) al de destino; color blanco opaco, sin especular. */
void g3d_apres_quad(const g3d_apres_t *p, int fl, int fa, gpu3d_vert_t q[6]);
/* Seis vértices de color «cor» (0xAARRGGBB) sobre el rectángulo x, y, l, a. */
void g3d_quad_cor(int x, int y, int l, int a, uint32_t cor, gpu3d_vert_t q[6]);

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
    /* ---- gpu3d v2 (D425; docs/gpu3d.md «gpu3d v2»). Todas pueden ser NULL: un driver sin ellas
     * sigue como v1 y SaguiGL dibuja por software lo que no cabe en v1. */
    /* Qué sabe hacer (tras iniciar). La capa ya rellenó los valores de v1 (bits 0, 1 unidad,
     * GPU3D_MAX_LADO, solo ARGB, Z 16 y 24, sin esténcil): el driver cambia lo que tenga. */
    void (*capacidades)(gpu3d_capas_t *c);
    /* Como desenhar, con el estado v2 (ya validado contra las capacidades) y vértices v2. */
    int  (*desenhar2)(const gpu3d_estado2_t *e, const gpu3d_vert2_t *v, int n, g3d_vala_t *vala);
    /* Un nivel de una cara (t->dados: puntero del programa ya comprobado, como en v1); dados 0: soltar el hueco. */
    int  (*textura2)(const gpu3d_textura2_t *t);
    int  (*limpar2)(const gpu3d_limpar2_t *l);
    /* Dibujar en el hueco «vaga» (su nivel 0; cara 0) o, con vaga < 0, en el destino principal.
     * zbits 0/16/24 y estencil 0/8: el Z y el esténcil propios de ese destino. */
    int  (*destino)(int vaga, int zbits, int estencil);
    /* ---- D432: presentar sin volver a la RAM (docs/gpu3d.md). Puede ser NULL.
     * p == NULL: «¿puedes escribir en la pantalla de ahora?» (G3D_OK o G3D_NAO; sin tocar la GPU).
     * Si no, copia el rectángulo de la fuente al de la pantalla visible (escalando), espera a que
     * la GPU termine (con plazo) y deja el destino de dibujo como estaba. La capa ya comprobó los
     * rectángulos y que quien llama tiene la pantalla en exclusiva. */
    int  (*apresentar)(const g3d_apres_t *p);
} gpu3d_driver_t;

/* D390: los ajustes del fichero del driver, leídos por la capa antes de iniciar() y ya
 * filtrados por sus rangos (n = 0 sin fichero). */
extern g3d_ajustes_t gpu3d_ajustes;

extern const gpu3d_driver_t gpu3d_intel;   /* Gen3: 915, 945, G33, Pineview (gpu3d_intel.c) */
extern const gpu3d_driver_t gpu3d_r600;    /* Radeon R7xx por el CP (gpu3d_r600.c, D284): no se enciende solo */
extern const gpu3d_driver_t gpu3d_r500;    /* Radeon R5xx: X1300 a X1950 (gpu3d_r500.c, D287) */
extern const gpu3d_driver_t gpu3d_evergreen; /* Radeon Evergreen y NI sin Cayman: HD 5000-6000, HD 7500M/7600M (gpu3d_evergreen.c, D427) */
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
/* D432: el compositor retiró la pantalla exclusiva (para el contador y el log). */
void gpu3d_exclusiva_perdida(void);

/* D436 (audio): las esperas largas de un driver (vallas, cola llena, leer el cuadro por la CPU) llaman a esto en cada
 * vuelta. Si la espera es de una llamada de un programa (GPU3D_OP_*), cada GPU3D_RESPIRAR_MS deja pasar las
 * interrupciones pendientes (el reloj y la tarjeta de sonido: el mezclador del kernel) y cede la CPU a quien esté listo
 * (el hilo de audio). Fuera de eso (arranque, tareas del kernel, una IRQ) no hace nada: se sigue girando como antes.
 * Solo se puede llamar donde el estado del driver está entero (entre órdenes completas en la cola): mientras cede,
 * otro proceso puede entrar en el driver (el 2D por la cola del CP); otro hilo del dueño, no (espera en gpu3d_op). */
#define GPU3D_RESPIRAR_MS 2
void gpu3d_respirar(void);
/* Nunca cede dentro de un fpu_kernel_inicio/fim (copias con SSE): allí no hace nada. Pruebas: forzar ceder siempre. */
void gpu3d_respirar_forcar(bool sim);
/* Mientras haya un bloqueo, no cede (las entradas del driver para el 2D y la energía, la vuelta por la GART). */
void gpu3d_respirar_bloquear(bool sim);

/* Coma flotante sin coma flotante: bits IEEE-754 <-> coma fija con «frac» bits. */
uint32_t g3d_float_de_fixo(int32_t v, int frac);
int32_t  g3d_fixo_de_float(uint32_t bits, int frac);

#endif
