// SPDX-License-Identifier: GPL-2.0-or-later
/* acel2d.h - 2D por la GPU para el escritorio: copiar y rellenar rectángulos (D195).
 *
 * La capa común (kernel/gui/acel2d.c) elige al arrancar el acelerador que
 * corresponde a la tarjeta (tabla por PCI), lo pone a prueba contra la CPU y
 * solo entonces lo usa. Sin acelerador, o si falla en cualquier momento, todo
 * lo hace la CPU como siempre (video.c). Los aceleradores están en
 * kernel/dispositivos/acel_*.c. */
#ifndef SAGUI_ACEL2D_H
#define SAGUI_ACEL2D_H
#include "tipos.h"
#include "pci.h"

/* Lo que pide la cabecera de arranque (CAB_ACEL2D, byte 135; mkimagem.py). */
#define ACEL2D_MODO_MASCARA 0x0F  /* D308: 4 bits (antes 3); 0x10 y 0x20 siguen siendo las pruebas */
#define ACEL2D_AUTO         0     /* elección automática por el PCI (lo normal) */
#define ACEL2D_CPU          1     /* --sin-acel2d o --acel2d=cpu: siempre la CPU */
#define ACEL2D_VMWARE       2     /* --acel2d=vmware: solo ese acelerador */
#define ACEL2D_INTEL        3     /* --acel2d=intel */
#define ACEL2D_RADEON       4     /* --acel2d=radeon (R100-R500 y, D223, R6xx/R7xx; D286 HD 5000-6000; D360 GCN 1.0) */
#define ACEL2D_NVIDIA       6     /* --acel2d=nvidia (GeForce 6/7, D224) */
#define ACEL2D_TDFX         7     /* --acel2d=tdfx (3dfx Banshee, Voodoo3, Voodoo4/5; D305) */
#define ACEL2D_MACH64       8     /* --acel2d=mach64 (ATI Mach64 y Rage hasta la XL, D308) */
#define ACEL2D_S3           9     /* --acel2d=s3 (S3 Trio64 y ViRGE, D308) */
#define ACEL2D_PROVA_PIXEL  0x10  /* pruebas: la autoprueba del arranque ve un píxel distinto */
#define ACEL2D_PROVA_TEMPO  0x20  /* pruebas: la autoprueba del arranque no termina a tiempo */

/* Filas de la zona de prueba de la capa común, debajo de la pantalla visible
 * cuando el VBE anuncia memoria para ellas: un driver que necesite memoria de
 * vídeo propia (el anillo de la R6xx/R7xx, D223) la pone detrás. */
#define ACEL2D_ZONA_A       64

/* Pantalla y memoria de vídeo que ve el acelerador (la rellena acel2d.c). */
typedef struct {
    uint32_t fb_fisico;      /* framebuffer lineal del VBE (dirección física) */
    uint32_t pitch;          /* bytes por línea */
    int      larg, alt;      /* píxeles visibles */
    uint32_t memoria;        /* bytes de memoria de vídeo que anunció el VBE */
    int      bytes_px;       /* D305: 4 (32 bits 0x00RRGGBB); 3 (24 bits) o 2 (RGB565) solo con
                                los aceleradores que lo aceptan (outras_profundidades) */
} acel_tela_t;

/* D391: de dónde sale la vuelta (VRAM -> RAM): una dirección del MC de la VRAM (el destino del 3D) o,
 * con ACEL_ORIGEM_TELA, las filas de la pantalla desde «y» (la zona de prueba, para la autoprueba).
 * «cpu» es lo mismo visto por la CPU (la GPU simulada copia de ahí). gpu = 0: la GPU de verdad no lo
 * alcanza (el 3D simulado) y solo la simulada sirve. */
#define ACEL_ORIGEM_TELA 0xFFFFFFFFFFFFFFFFull
typedef struct {
    uint64_t        gpu;
    const uint32_t *cpu;
    uint32_t        passo;   /* bytes por fila */
    int             y;       /* con ACEL_ORIGEM_TELA: la fila de la pantalla donde empieza */
} acel_origem_t;

/* D387: volcado del búfer de atrás (RAM del sistema) a la pantalla por la GPU. El driver da a la
 * GPU acceso a ese búfer (GART en las Radeon R5xx; GTT en las Intel, pendiente) y lo copia con su
 * motor 2D. La capa común (acel2d.c) lo prueba al arrancar, cuenta y, si algo falla, vuelve a la
 * CPU. Regiones: 0 = el búfer de atrás; 1 = el búfer de la autoprueba. */
typedef struct {
    /* Comprueba la tarjeta y deja la GART encendida y vacía. false + motivo si no se puede. */
    bool (*preparar)(char *motivo, int n);
    /* Apunta en la GART las páginas de «buf» (bytes desde buf) como la región «reg»; «passo» son
     * los bytes por fila, larg x alt los píxeles. Vuelve a llamarse si el búfer cambia. */
    bool (*mapear)(int reg, const void *buf, uint32_t passo, uint32_t larg, uint32_t alt, char *motivo, int n);
    /* Copia (x, y, l, a) de la región a (dx, dy) de la pantalla (dy puede caer en la zona de
     * prueba, debajo de la visible). Pone las órdenes y vuelve: la GPU sigue sola. */
    bool (*volcar)(int reg, int x, int y, int l, int a, int dx, int dy);
    bool (*esperar)(uint32_t ms);     /* hasta que el motor termine; false: tiempo agotado */
    void (*soltar)(void);             /* apaga la GART y deja todo como estaba (sin esperar al motor) */
    void (*detalle)(const char *fase);   /* registros de la GART al log [ACEL2D] */
    /* D391 (opcionales; NULL si el driver no sabe): la vuelta del 3D a la RAM. mapear_escrita apunta
     * las «npag» páginas físicas (de RAM) del destino CON ESCRITURA en la región 2 de la GART; con
     * npag = 0 las vuelve a dejar en la página de relleno, solo de lectura (se llama siempre al
     * terminar cada copia: la GPU solo puede escribir en la RAM mientras dura). retornar copia l x a
     * píxeles del origen a la región 2 (desl: bytes desde su primera página; passo: bytes por fila),
     * sin el alfa si «mascara» (0x00RRGGBB, como la CPU); pone las órdenes y vuelve. */
    bool (*mapear_escrita)(const uint64_t *paginas, uint32_t npag, char *motivo, int n);
    bool (*retornar)(const acel_origem_t *o, int l, int a, uint32_t desl, uint32_t passo, bool mascara);
    /* Depuración en hardware real (opcional; NULL si no): «acel2d volcado paso N», la puesta en marcha de la
     * GART solo hasta el paso N (registros al log) y deshecha después. false + motivo si falla. */
    bool (*paso)(int n, char *motivo, int nm);
} acel_volcado_t;

/* Un acelerador. Las coordenadas son píxeles del framebuffer, con el mismo pitch
 * que la pantalla; «y» puede pasar de la altura visible (zona de prueba).
 * copiar() tiene que dar el resultado de memmove con solapes en cualquier
 * dirección. Todo devuelve false si no pudo (la capa vuelve a la CPU). */
typedef struct {
    const char *nome;                 /* "vmware", "intel", "radeon", "r600dma" */
    int         modo;                 /* ACEL2D_VMWARE... (para forzarlo) */
    bool (*iniciar)(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *motivo, int n);
    bool (*copiar)(int ox, int oy, int dx, int dy, int l, int a);
    bool (*preencher)(int x, int y, int l, int a, uint32_t cor);
    bool (*esperar)(void);            /* false: tiempo agotado */
    bool (*atualizar)(int x, int y, int l, int a);   /* NULL: la pantalla lee la memoria sola; false: no responde */
    void (*desligar)(void);           /* deja de usar el motor (y la pantalla como la dejó el VBE) */
    bool fora_da_tela;                /* la zona de prueba puede ir debajo de la pantalla visible
                                         (VMware en QEMU: no, solo acepta rectángulos visibles) */
    /* D246 (opcionales; a 0 / NULL en los demás aceleradores): */
    uint32_t    min_pixeis;           /* copias de menos píxeles las hace la CPU (sin apagar el acelerador) */
    bool        assincrono;           /* el motor sigue trabajando tras copiar(): hay que esperar antes de que
                                         la CPU toque la pantalla (acel_sincronizar) */
    void (*detalle)(const char *fase);   /* vuelca al log [ACEL2D] los registros clave («acel2d probar --detalle») */
    /* D305 (opcional; false en los demás): el acelerador también trabaja con pantallas de 24 bits
     * (0xRRGGBB empaquetado) y de 16 (RGB565). Los colores de preencher() le llegan ya en el formato
     * de la tarjeta (los bits bajos de un uint32_t) y las coordenadas siguen en píxeles. */
    bool        outras_profundidades;
    /* D387 (opcional; NULL en los demás): volcado del búfer de atrás por la GPU. */
    const acel_volcado_t *volcado;
} acel_driver_t;

/* Un plazo con el TSC (y, sin TSC medido, con un número de vueltas). */
typedef struct { uint64_t fim; uint32_t voltas, max_voltas; } acel_prazo_t;
void acel_prazo(acel_prazo_t *p, uint32_t ms);
void acel_prazo_longo(acel_prazo_t *p, uint32_t ms);   /* D285: sin el recorte de la sesión */
bool acel_prazo_esgotado(acel_prazo_t *p);

/* Capa común */
void        acel2d_iniciar(void);            /* tras video_iniciar: detecta, prueba y activa */
bool        acel2d_ativo(void);
/* D285: tarjeta de la pantalla y su acelerador, aunque el 2D esté en la CPU (false: ninguna de la tabla). */
bool        acel2d_placa(pci_disp_t *d, int *geracao, const acel_driver_t **driver, acel_tela_t *t, bool *suspeito);
const char *acel2d_nome(void);               /* "vmware", "intel", "radeon", "tdfx", "mach64", "s3"... o "cpu" */
const char *acel2d_motivo(void);             /* por qué la CPU, o qué tarjeta se usa */
bool        acel2d_forcado(void);            /* se pidió un acelerador concreto (--acel2d=...) */
void        acel2d_desligar(const char *motivo);   /* vuelve a la CPU para siempre */
/* D387: la memoria de vídeo real de la tarjeta, según su driver (bytes; «fonte», el registro). Se guarda la
 * primera que llega con un valor creíble (4 MB a 64 GB) y sale en INFO_ACEL2D (infopc, Configuración, Tareas). */
void        acel2d_vram_real(uint64_t bytes, const char *fonte);
int32_t     acel2d_apagar(void);             /* D244: «acel2d apagar» */
int32_t     acel2d_acender(void);            /* D244: «acel2d encender»: repite la autoprueba */

/* Sobre la pantalla visible. false = no se hizo: la CPU tiene que hacerlo
 * (quien llama marca la zona para video_mostrar). */
bool acel_copiar(int ox, int oy, int dx, int dy, int l, int a);
bool acel_rellenar(int x, int y, int l, int a, uint32_t cor);
bool acel_esperar(void);                     /* antes de que la CPU toque lo que la GPU escribe */
void acel_sincronizar(void);                 /* D246: espera solo si el motor es asíncrono y hay algo pendiente */
void acel_atualizar(int x, int y, int l, int a);   /* la CPU escribió en la pantalla */

/* D387: el volcado del búfer de atrás por la GPU (video_mostrar). acel_volcar devuelve false si lo
 * tiene que hacer la CPU (apagado, rectángulo pequeño, varias pantallas...); si devuelve true, la GPU
 * sigue copiando: antes de que la CPU escriba en la pantalla o mueva el búfer de atrás hay que
 * llamar a acel_sincronizar(). acel_volcado_buffer avisa de que el búfer de atrás va a cambiar
 * (antes = true: espera y lo suelta) o ya cambió (false: lo vuelve a apuntar). */
bool acel_volcar(int x, int y, int l, int a);
void acel_volcado_buffer(bool antes);
void acel2d_volcado_iniciar(void);           /* tras acel2d_iniciar (o sin acelerador, el simulado) */
/* Depuración del Amilo (dbg4): un cuadrado de 10x10 en la fila «fila» y la columna «col» de las marcas de la
 * esquina de arriba a la izquierda (acel2d.c): estado 0 amarillo, 1 blanco, 2 rojo. Para los drivers. */
void acel_marca_fina(int fila, int col, int estado);
void acel_marca_paso(int fila, int col, int estado, bool tras);     /* dbg5: solo con el motor parado */
bool acel_radeon_dbg_marcas(void);                                  /* dbg5: opción MARCAS (acel_radeon.c) */
int32_t acel2d_volcado_ctl(int op);          /* INFO_ACEL2D_CTL 2 apagar, 3 encender, 4 medir; 10-35 paso 0-25 (depuración) */
/* Estado del volcado (info_acel2d_t.volcado_estado) y de la vuelta (retorno_estado, D391). */
enum { VOLCADO_NO = 0, VOLCADO_ACTIVO, VOLCADO_KCONFIG, VOLCADO_A_MANO, VOLCADO_FALLO, VOLCADO_LENTO };

/* D391: la vuelta del cuadro del 3D (VRAM) a la RAM del programa por la GPU, sin que la CPU lea la
 * VRAM. La llama el driver 3D al «leer el cuadro», con el motor 3D ya parado (esperó su valla):
 * true = el cuadro ya está en «dest» (la GPU terminó); false = lo tiene que copiar la CPU (apagado,
 * destino que no sirve, o un fallo, que apaga lo que corresponda como el vigilante). «dest» es del
 * programa que llama (o del kernel) y «passo_px» sus píxeles por fila. */
bool acel_retornar(const acel_origem_t *o, int l, int a, uint32_t *dest, int passo_px);
int32_t acel2d_retorno_ctl(int op);          /* INFO_ACEL2D_CTL 5 apagar, 6 encender, 7 medir */

/* D346: contadores del reparto GPU / software (solo lectura por INFO_ACEL2D, sin ser administrador).
 * Lo que la CPU hace en el kernel y la GPU podría haber hecho: mover rectángulos y desplazar la consola
 * (copias), rellenos del gestor de ventanas, y lo que la CPU vuelca del búfer de atrás a la pantalla.
 * Cuentan siempre, con o sin acelerador (un incremento, sin cerrojo: el kernel corre en la CPU 0). */
enum { ACEL_SW_MOVER, ACEL_SW_ROLAR, ACEL_SW_RELLENO, ACEL_SW_MOSTRAR, ACEL_SW_N };
void acel2d_contar_sw(int tipo, uint32_t pixeis);

/* Medidas (INFO_ACEL2D) y la orden de prueba (acel2d probar, INFO_ACEL2D_PROVA). */
void    acel2d_contar_arrasto(uint32_t quadros, uint64_t ciclos);
int32_t acel2d_info(uint32_t ptr, uint32_t n);
int32_t acel2d_prova(uint32_t ptr, uint32_t n);

/* Los aceleradores (kernel/dispositivos/acel_*.c). */
extern const acel_driver_t acel_vmware;
extern const acel_driver_t acel_intel;
extern const acel_driver_t acel_radeon;
extern const acel_driver_t acel_r600;      /* R6xx/R7xx por el motor DMA (D223) */
extern const acel_driver_t acel_gcn;       /* Radeon GCN 1.0 por el motor DMA (D360) */
extern const acel_driver_t acel_nv;
extern const acel_driver_t acel_tdfx;      /* 3dfx Banshee/Voodoo3/Voodoo4/5 (D305) */
extern const acel_driver_t acel_mach64;   /* ATI Mach64/Rage (D308) */
extern const acel_driver_t acel_s3;       /* S3 Trio64 y ViRGE (D308) */

#endif
