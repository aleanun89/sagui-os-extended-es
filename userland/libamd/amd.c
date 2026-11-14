// SPDX-License-Identifier: GPL-2.0-or-later
/* amd.c - el armazón del driver 3D de las GPU de AMD (mejora 054, B2; D158).
 *
 * Aquí está la tabla de familias y la identificación. La GPU todavía no
 * dibuja nada por hardware: esto es la estructura para que las familias se
 * añadan con una entrada de tabla y no con código nuevo.
 *
 * Los identificadores de dispositivo salen de la tabla de la parte A1
 * (userland/inc/gpuid.h, D104), que ya está revisada; el reparto fino entre
 * R300, R420 y las R500 de escritorio se tendrá que ajustar con el hardware
 * real y con la base de datos pci.ids. Los registros (MM_INDEX y los
 * directos) salen de la documentación pública de AMD para cada familia.
 */
#include "amd.h"

/* ------------------------------------------------------------------ tabla */

/* Evergreen (HD 5xxx): la familia más nueva del plan (B2). */
static const amd_familia_info_t familia_evergreen = {
    AMD_FAMILIA_EVERGREEN, "AMD Evergreen", "Radeon HD 5000, Ontario/Llano, Cedar/Hemlock", 4, "RV670, RV770, RV730, RV740",
    0xE000, 0xE004,        /* el índice y los datos (indirectos) */
    0x18, 0x1C, 0x20,      /* DEVICE_ID, REV_ID y SUBSYS_ID (directos) */
    0x0B12,                /* el estado del bus maestro de registros */
    0,                     /* el BAR 0 es el de los registros */
    { { 0x6880, 0x68FF }, { 0x9640, 0x964F }, { 0x9800, 0x9808 } }, 3,
    1,                     /* hace falta microcódigo (PFP/ME), como en R6xx */
    "VLIW5 con 4 o 5 vías, y datos de 128 bits",
    AMD_B1_LEITURA | AMD_B1_COMANDOS | AMD_B1_GART | AMD_B1_2D,
    "la lectura de registros, el procesador de comandos con su microcódigo, el GART y el 2D",
    "el 3D y el motor r600: la ISA de los shaders es otra (VLIW5 de Evergreen), así que los shaders precompilados y el motor son propios"
};

/* R5xx (X1xxx): el Amilo. Sin microcódigo: se programa con registros. */
static const amd_familia_info_t familia_r5xx = {
    AMD_FAMILIA_R5XX, "AMD R5xx", "Radeon X1000 (R520/RV5xx), Xpress 1200", 2, "RV520, RV530, RV550, RV560, RV570",
    0x4000, 0x4004,        /* el índice y los datos (indirectos) */
    -1, -1, -1,            /* el identificador y la revisión no están directos: van por el PCI */
    -1,                    /* el estado del bus maestro: otro índice, a confirmar */
    0,
    { { 0x7100, 0x72FF }, { 0x791E, 0x791F }, { 0x7941, 0x7942 } }, 3,
    0,                     /* no hace falta microcódigo */
    "Turing: 4 vías de vector y 1 de escalar",
    AMD_B1_LEITURA | AMD_B1_GART,
    "la idea de leer registros (con otro MM_INDEX) y el gestor de memoria (GART y VRAM)",
    "el procesador de comandos con microcódigo no hace falta aquí (se programa con registros), el 2D por la GPU es otro motor y el 3D necesita shaders de otra ISA"
};

/* R300/R420 (X300-X850): la más antigua del plan. Reutiliza parte de R5xx. */
static const amd_familia_info_t familia_r300 = {
    AMD_FAMILIA_R300, "AMD R300/R420", "Radeon 9500-X850, Xpress 200, familia de gama baja", 1, "RV300, R300, R310, R420",
    0x4000, 0x4004,        /* el índice y los datos (indirectos) */
    -1, -1, -1,            /* el identificador y la revisión no están directos: van por el PCI */
    -1,                    /* el estado del bus maestro: otro índice, a confirmar */
    0,
    { { 0x4144, 0x4177 }, { 0x4A48, 0x4A6F }, { 0x4E44, 0x4E57 }, { 0x5460, 0x5465 },
      { 0x5548, 0x555F }, { 0x564A, 0x5657 }, { 0x5834, 0x5835 }, { 0x5954, 0x5955 },
      { 0x5A41, 0x5A42 }, { 0x5A61, 0x5A62 }, { 0x5B60, 0x5B65 }, { 0x5D48, 0x5D57 },
      { 0x5E48, 0x5E4F } }, 13,
    0,                     /* no hace falta microcódigo */
    "Turing: 4 vías de vector y 1 de escalar (en R300, sin escalar propio)",
    AMD_B1_LEITURA | AMD_B1_GART,
    "reutiliza de R5xx la lectura de registros y el gestor de memoria",
    "no hay procesador de comandos con microcódigo, ni 2D por la GPU como en B1; el 3D necesita shaders de otra ISA"
};

/* R6xx/R7xx (HD 2xxx-4xxx): la primera familia (B1), la del dv7. */
static const amd_familia_info_t familia_r6xx = {
    AMD_FAMILIA_R6XX, "AMD R6xx/R7xx", "Radeon HD 2000-4000, HD 3200/4200 integradas", 3, "R600, RV670, RV690, RV770",
    0xD00, 0xD04,          /* el índice y los datos (indirectos) */
    0x18, 0x1C, 0x20,      /* DEVICE_ID, REV_ID y SUBSYS_ID (directos) */
    0x0B12,                /* el estado del bus maestro de registros */
    0,
    { { 0x9400, 0x95FF }, { 0x9610, 0x9616 }, { 0x9710, 0x9715 } }, 3,
    1,                     /* hace falta microcódigo (PFP/ME) */
    "VLIW5: 5 vías de vector y 8 de escalar",
    AMD_B1_LEITURA | AMD_B1_COMANDOS | AMD_B1_GART | AMD_B1_2D | AMD_B1_3D | AMD_B1_MOTOR,
    "todo: es la base de B1, aquí se hace todo",
    "nada: es la familia de referencia"
};

/* Las de después: fuera del plan (el amdgpu necesita el PSP y el SMU). */
static const amd_familia_info_t familia_posterior = {
    AMD_FAMILIA_POSTERIOR, "AMD posterior", "Radeon HD 6000 y posteriores, GCN, RDNA", 5, "Turks, Bonaire, Navi",
    0, 0, -1, -1, -1, -1, 0,
    { { 0x6600, 0x687F }, { 0x6980, 0x69FF }, { 0x7300, 0x75FF }, { 0x9900, 0x99FF },
      { 0x1300, 0x13FF }, { 0x15D0, 0x15FF } }, 6,
    1,
    "GCN: 4 o 5 vías con su propia unidad de excursion; RDNA: otra cosa",
    0,
    "nada",
    "fuera del plan: su driver necesita cargar firmware en el PSP y programar el SMU (mejora 054)"
};

/* La familia R100/R200: tampoco está en el plan (A1 las tiene como «AMD R100/R200»). */
static const amd_familia_info_t familia_r100 = {
    AMD_FAMILIA_NENHUMA, "AMD R100/R200", "Radeon 7000-9250, Rage", 0, "R100, R200",
    0x4000, 0x4004, -1, -1, -1, -1, 0,
    { { 0x4136, 0x4137 }, { 0x4966, 0x496F }, { 0x4C57, 0x4C66 }, { 0x5144, 0x515E }, { 0x5960, 0x5969 } }, 5,
    0, "R100: 4 vías de vector sin escalar",
    0, "nada", "fuera del plan"
};

static const amd_familia_info_t *const tabela[] = {
    &familia_r6xx,        /* B1: la primera */
    &familia_evergreen,   /* B2 */
    &familia_r5xx,        /* B2 */
    &familia_r300,        /* B2 */
    &familia_posterior,
    &familia_r100,
};
#define TABELA_N (int)(sizeof tabela / sizeof tabela[0])

const amd_familia_info_t *const *amd_tabela(int *n)
{
    if (n) *n = TABELA_N;
    return tabela;
}

const amd_familia_info_t *amd_buscar_familia(unsigned modelo)
{
    for (int i = 0; i < TABELA_N; i++) {
        const amd_familia_info_t *f = tabela[i];
        for (unsigned k = 0; k < f->n_faixas; k++)
            if (modelo >= f->faixas[k].a && modelo <= f->faixas[k].b) return f;
    }
    return NULL;
}

/* ------------------------------------------------------- el estado del motor */

static amd_motor_t motor = AMD_MOTOR_SOFTWARE;
static amd_acel_t acel = AMD_ACEL_SIN_PROBAR;
static char motivo[128];
static unsigned plazo = MOTOR_GPU_VIGILANTE_MS;

static void copiar_motivo(const char *t)
{
    if (!t) { motivo[0] = 0; return; }
    size_t n = tam_texto(t);
    if (n >= sizeof motivo) n = sizeof motivo - 1;
    memcpy(motivo, t, n);
    motivo[n] = 0;
}

void amd_iniciar(void)
{
    info_motor_gpu_t m;
    memset(&m, 0, sizeof m);
    if (info_sistema(INFO_MOTOR_GPU, &m, sizeof m) == 0) {
        if (m.watchdog_ms) plazo = m.watchdog_ms;
        if (m.desativado) {
            acel = AMD_ACEL_DESACTIVADA;
            copiar_motivo(m.motivo[0] ? m.motivo : "la opcion de arranque pidio solo el motor por software");
        } else if (m.gpu_amd) {
            acel = AMD_ACEL_PERMITIDA;
            copiar_motivo("");
        } else {
            acel = AMD_ACEL_NO_GPU;
            copiar_motivo(m.motivo[0] ? m.motivo : "no hay ninguna GPU AMD en el bus PCI");
        }
    } else {
        acel = AMD_ACEL_NO_GPU;
        copiar_motivo("el kernel no pudo decir el estado del motor 3D");
    }
    /* El camino por defecto es el motor por software de SaguiGL: el motor por
     * la GPU todavía no existe (mejora 054, B1). */
    motor = AMD_MOTOR_SOFTWARE;
}

amd_motor_t amd_motor(void) { return motor; }
amd_acel_t  amd_acel(void)  { return acel; }
const char *amd_motivo(void) { return motivo; }
unsigned amd_plazo_vigilante(void) { return plazo; }

const char *amd_motor_nome(void)
{
    return motor == AMD_MOTOR_GPU ? "la GPU" : "software (SaguiGL por software)";
}

const char *amd_acel_nome(void)
{
    switch (acel) {
        case AMD_ACEL_SIN_PROBAR: return "sin probar";
        case AMD_ACEL_PERMITIDA:  return "permitida";
        case AMD_ACEL_DESACTIVADA: return "desactivada por la opcion de arranque";
        case AMD_ACEL_CAIDA:      return "caida al motor por software";
        case AMD_ACEL_NO_GPU:     return "sin GPU AMD";
    }
    return "?";
}

void amd_cair_al_software(const char *porque)
{
    motor = AMD_MOTOR_SOFTWARE;
    acel = AMD_ACEL_CAIDA;
    copiar_motivo(porque ? porque : "la GPU fallo");
}

/* --------------------------------------------------------- la identificación */

static inline uint32_t mm_leer(uint32_t base, unsigned desloc)
{
    return *(volatile uint32_t *)(base + desloc);
}

static inline void mm_escrever(uint32_t base, unsigned desloc, uint32_t v)
{
    *(volatile uint32_t *)(base + desloc) = v;
}

/* Un registro indirecto: se escribe su índice y se lee el de datos. */
static uint32_t mm_indireto(amd_gpu_t *g, unsigned indice)
{
    mm_escrever(g->mapa, g->info->mm_index, indice);
    return mm_leer(g->mapa, g->info->mm_data);
}

int amd_identificar(amd_gpu_t *g)
{
    static info_pci_t lista[64];
    memset(g, 0, sizeof *g);
    g->familia = AMD_FAMILIA_NENHUMA;
    int n = info_sistema(INFO_PCI, lista, 64);
    if (n < 0) {
        copiar_motivo("el kernel no pudo dar la lista del PCI");
        return n;
    }
    if (n > 64) n = 64;
    for (int i = 0; i < n; i++) {
        if (lista[i].classe != 0x03) continue;          /* solo lo que dibuja */
        if (lista[i].fabricante != 0x1002) continue;    /* solo AMD/ATI */
        g->tem_gpu = 1;
        g->barramento = lista[i].barramento;
        g->dispositivo = lista[i].dispositivo;
        g->funcao = lista[i].funcao;
        g->modelo = lista[i].modelo;
        g->revisao = lista[i].revisao;
        g->info = amd_buscar_familia(g->modelo);
        if (g->info) {
            g->familia = g->info->familia;
            g->bar = g->info->bar_registros;
        } else {
            g->bar = 0;
        }
        /* El mapa: los primeros 64 KB del BAR de los registros, que es donde
         * están los registros directos y el par MM_INDEX/MM_DATA de todas las
         * familias del plan.
         * Leer un registro indirecto exige escribir su índice, así que el mapa
         * va con permiso de escritura cuando la familia los usa (el kernel no
         * lo da por omisión: lo pide el programa). */
        uint32_t mapa = 0;
        int escrever = (g->info && g->info->grbm_indice >= 0) ? 1 : 0;
        int r = amd_mapear_bar(g->barramento, g->dispositivo, g->funcao, g->bar, 0, 64 * 1024,
                               escrever, &mapa);
        g->mapa_escritura = escrever;
        if (r < 0) {
            char t[80];
            formatar_texto(t, sizeof t, "no se pudo mapear el BAR %u (%s)", g->bar, nome_erro(r));
            memcpy(g->erro, t, sizeof g->erro);
            g->erro[sizeof g->erro - 1] = 0;
            return r;
        }
        g->mapa = mapa;
        g->tamanho = 64 * 1024;
        g->mapeada = 1;
        if (!g->info) {
            copiar_motivo("la GPU no es de ninguna familia conocida");
            return 0;
        }
        amd_leer_registros(g);
        return 0;
    }
    copiar_motivo("no hay ninguna GPU AMD en el bus PCI");
    return 0;
}

int amd_leer_registros(amd_gpu_t *g)
{
    if (!g->info || !g->mapeada) return -1;
    /* Los registros de identificación de la familia. */
    if (g->info->id_registro >= 0) {
        g->device_id = mm_leer(g->mapa, (unsigned)g->info->id_registro);
        g->rev_mmio = mm_leer(g->mapa, (unsigned)g->info->rev_registro);
        if (g->info->subsis_registro >= 0) {
            g->subsis_id = mm_leer(g->mapa, (unsigned)g->info->subsis_registro);
            g->subsis_vendor = mm_leer(g->mapa, (unsigned)g->info->subsis_registro + 4);
        }
    }
    if (g->info->grbm_indice >= 0) {
        g->grbm_status = mm_indireto(g, (unsigned)g->info->grbm_indice);
        g->tem_grbm = 1;
    }
    g->leu_registros = 1;
    return 0;
}

const amd_familia_info_t *amd_familia_nome(const char *nome)
{
    if (!nome) return NULL;
    for (int i = 0; i < TABELA_N; i++)
        if (compara_texto(tabela[i]->nome, nome) == 0) return tabela[i];
    return NULL;
}

/* ------------------------------------------------------------ el vigilante */

int amd_esperar(amd_gpu_t *g, uint32_t prazo_ms, amd_leitor_t ler, void *contexto, uint32_t valor)
{
    uint32_t inicio = tempo_ms();
    uint32_t limite = prazo_ms ? prazo_ms : amd_plazo_vigilante();
    for (;;) {
        if (ler(g, contexto) == valor) return 0;
        if (tempo_ms() - inicio >= limite) {
            amd_cair_al_software("el vigilante de la GPU se paso de plazo");
            return -1;
        }
        dormir(1);
    }
}

int amd_vigilar(amd_gpu_t *g, uint32_t prazo_ms, amd_operacao_t op, void *contexto)
{
    uint32_t inicio = tempo_ms();
    uint32_t limite = prazo_ms ? prazo_ms : amd_plazo_vigilante();
    int r = op(g, contexto);
    if (tempo_ms() - inicio > limite) {
        amd_cair_al_software("la operacion de la GPU se paso de plazo");
        return -1;
    }
    return r;
}

uint32_t amd_ler_registro(amd_gpu_t *g, void *contexto)
{
    if (!g->mapeada) return 0xFFFFFFFFu;
    return mm_leer(g->mapa, (unsigned)contexto);
}

uint32_t amd_ler_simulado(amd_gpu_t *g, void *contexto)
{
    (void)g; (void)contexto;
    return 0x5A5A5A5Au;   /* nunca llega al valor que se espera: el vigilante vence */
}
