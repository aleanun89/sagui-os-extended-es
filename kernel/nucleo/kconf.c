// SPDX-License-Identifier: GPL-2.0-or-later
/* kconf.c - opciones del kernel que se guardan desde SaguiOS y valen en el siguiente arranque (D353).
 * Ver kernel/inc/kconf.h para el modelo y docs/decisoes.md (D353) para el porqué.
 *
 * Reglas:
 *  - Solo opciones que ya existían (las de mkimagem.py) y están probadas, más tres topes de la caché de
 *    disco que solo pueden BAJAR lo de siempre. Nada de lo que pueda dejar la máquina sin arrancar sin
 *    salida: el modo rescate (Ctrl al arrancar) no aplica nada de esto.
 *  - Los bits de la imagen no se tocan: lo guardado va aparte (bytes 192-255) y se aplica encima en
 *    una copia (cab_ef); volver a la imagen es borrar esa zona.
 *  - Un valor guardado fuera de rango, de una opción que este kernel no conoce o una zona con la suma
 *    mala se ignoran y se anota en el registro.
 *  - Lo calculado al arrancar no cambia hasta el siguiente arranque, aunque se guarde otra cosa; solo
 *    el gobernador de la frecuencia se aplica además al momento (es lo mismo que «frecuencia ahorro»).
 *
 * Añadir una opción: una línea en "ops" con un hueco nuevo (KC_* de kconf.h; nunca reutilizar uno), que
 * quien la lee use kconf_cab() o kconf_valor(), y su texto en userland/libsagui/kconf.c. */
#include "kconf.h"
#include "abi.h"
#include "bootinfo.h"
#include "console.h"
#include "frequencia.h"
#include "modos_video.h"
#include "sistema.h"
#include "texto.h"

typedef struct {
    const char *nome;
    uint8_t  hueco, tipo;
    uint8_t  desl, mascara;   /* SINO: byte y bit de la imagen; el bit significa «no» (--sem-pae...) */
    uint8_t  larg;            /* NUM/LISTA con valor de la imagen: 1 = u8, 2 = u16 en "desl"; 0 = sin él (0) */
    uint16_t min, max;
} kconf_op_t;

static const kconf_op_t ops[] = {
    { "pae",                KC_PAE,                KCONF_SINO,  CAB_MEM_OPC,   CAB_MEM_SEM_PAE,        0, 0, 1 },
    { "nx",                 KC_NX,                 KCONF_SINO,  CAB_MEM_OPC,   CAB_MEM_SEM_NX,         0, 0, 1 },
    { "cache_disco",        KC_CACHE_DISCO,        KCONF_SINO,  CAB_DISCO_OPC, CAB_DISCO_SEM_CACHE,    0, 0, 1 },
    { "cache_alta",         KC_CACHE_ALTA,         KCONF_SINO,  CAB_DISCO_OPC, CAB_DISCO_CACHE_BAIXA,  0, 0, 1 },
    { "escritura_diferida", KC_ESCRITURA_DIFERIDA, KCONF_SINO,  CAB_DISCO_OPC, CAB_DISCO_SEM_DIFERIDA, 0, 0, 1 },
    { "dma",                KC_DMA,                KCONF_SINO,  CAB_DISCO_OPC, CAB_DISCO_SEM_DMA,      0, 0, 1 },
    { "cache_max_mb",       KC_CACHE_MAX_MB,       KCONF_NUM,   0, 0,                              0, 4, 1024 },
    { "sucios_max_mb",      KC_SUCIOS_MAX_MB,      KCONF_NUM,   0, 0,                              0, 1, 16 },
    { "vaciado_s",          KC_VACIADO_S,          KCONF_NUM,   0, 0,                              0, 1, 30 },
    { "frecuencia",         KC_FRECUENCIA,         KCONF_SINO,  CAB_FREQ,      1 /* CAB_FREQ_DESATIVAR */, 0, 0, 1 },
    { "gobernador",         KC_GOBERNADOR,         KCONF_LISTA, 0, 0,                              0, 1, 3 },
    /* D352: la swap comprimida (troca.c lee con kconf_cab()). swap_mb desde 4: troca.c no baja de 1024 páginas
     * (con 1-3 el tope real sería 4 MB igualmente). */
    { "swap",               KC_SWAP,               KCONF_SINO,  CAB_MEM_OPC,   CAB_MEM_SEM_TROCA,      0, 0, 1 },
    { "swap_mb",            KC_SWAP_MB,            KCONF_NUM,   CAB_TROCA_MB,  0,                      2, 4, 1024 },
    /* v8.5.4.2 (D379): el tope de lo que se escribe en la RAM (capa de la live, /tmp), en % de la RAM (0 = 50);
     * y el modo de la raíz: 3 live desde el medio, 4 todo a la RAM (0 = el de la imagen). raiz.c los lee con
     * kconf_valor(); «instalado» y «solo el initramfs» no se guardan: solo con la tecla M al arrancar. */
    { "capa_ram_pct",       KC_CAPA_RAM_PCT,       KCONF_NUM,   0, 0,                              0, 10, 90 },
    { "raiz_modo",          KC_RAIZ_MODO,          KCONF_LISTA, 0, 0,                              0, 3, 4 },
    /* v8.5.4.10 (D385): la copia de la salida del Terminal al puerto serie. La lee el Terminal (INFO_KCONF, «efetivo»);
     * el kernel no hace nada con ella. Las imágenes de serie la llevan apagada (mkimagem.py --sin-serie-terminal). */
    { "serie_terminal",     KC_SERIE_TERMINAL,     KCONF_SINO,  CAB_TERM_OPC,  CAB_TERM_SIN_SERIE,     0, 0, 1 },
    /* v8.5.4.10r2 (D387): el volcado del búfer de atrás a la pantalla por la GPU (GART de las R5xx). acel2d.c lo lee
     * con kconf_cab(CAB_ACEL_OPC). Encendido de serie: solo se usa si su autoprueba pasa. */
    { "volcado_gpu",        KC_VOLCADO_GPU,        KCONF_SINO,  CAB_ACEL_OPC,  CAB_ACEL_SIN_VOLCADO,   0, 0, 1 },
    /* v8.5.4.10r2 (D391): la vuelta del cuadro del 3D a la RAM por la GPU (la misma GART). acel2d.c lo lee con
     * kconf_cab(CAB_ACEL_OPC). Encendido de serie: solo se usa si su autoprueba pasa. */
    { "vuelta_gpu",         KC_VUELTA_GPU,         KCONF_SINO,  CAB_ACEL_OPC,  CAB_ACEL_SIN_RETORNO,   0, 0, 1 },
    /* v8.5.5 (D381): AVX (estado YMM con XSAVE). Lo lee fpu_iniciar() con kconf_cab(CAB_CPU_OPC). */
    { "avx",                KC_AVX,                KCONF_SINO,  CAB_CPU_OPC,   CAB_CPU_SEM_AVX,        0, 0, 1 },
    /* v8.5.5 (D399): el modo de energía de la GPU al arrancar: 1 bios (no tocar), 2 bajo, 3 alto, 4 dinámico (el
     * «auto» del gobernador); 0 (automático) = dinámico si la tarjeta está confirmada, si no bios. gpu_energia.c lo
     * lee con kconf_valor(); el modo rescate y Mayús al arrancar no lo aplican. */
    { "gpu_energia",        KC_GPU_ENERGIA,        KCONF_LISTA, 0, 0,                              0, 1, 4 },
};
#define N_OPS ((int)(sizeof ops / sizeof ops[0]))

static bool     iniciado;
static uint8_t  cab_ef[KCONF_FIM];        /* los 256 primeros bytes de la cabecera, con lo guardado encima */
static uint16_t val_ef[KCONF_HUECOS];     /* el valor de cada opción en este arranque */
static uint16_t val_lido[KCONF_HUECOS];   /* lo que había guardado al arrancar (para el registro) */
static uint8_t  ign[KCONF_HUECOS];        /* KCONF_IGN_* */
static uint32_t pres_boot, desconhecidos, estado;

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static void po32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static uint32_t soma_zona(const uint8_t *c)
{
    uint32_t s = 0;
    for (uint32_t i = KCONF_ZONA; i < KCONF_SOMA; i++) s += c[i];
    return s;
}

/* 1 = zona buena, 0 = no hay nada guardado, -1 = la firma está pero la suma no. */
static int zona_estado(const uint8_t *c)
{
    if (le32(c + KCONF_ASSINATURA) != KCONF_ASSINATURA_VAL) return 0;
    return le32(c + KCONF_SOMA) == soma_zona(c) ? 1 : -1;
}

static const kconf_op_t *op_do_hueco(uint32_t h)
{
    for (int i = 0; i < N_OPS; i++)
        if (ops[i].hueco == h) return &ops[i];
    return NULL;
}

static uint16_t valor_imagem(const kconf_op_t *o, const uint8_t *c)
{
    if (o->tipo == KCONF_SINO) return (c[o->desl] & o->mascara) ? 0 : 1;
    if (o->larg == 1) return c[o->desl];
    if (o->larg == 2) return le16(c + o->desl);
    return 0;
}

static bool valido(const kconf_op_t *o, uint32_t v)
{
    if (o->tipo == KCONF_SINO) return v <= 1;
    return v == 0 || (v >= o->min && v <= o->max);
}

/* Lo guardado (si la zona es buena y el valor vale) o, si no, lo de la imagen. */
static uint16_t valor_em(const kconf_op_t *o, const uint8_t *c, bool *guardado)
{
    *guardado = false;
    if (zona_estado(c) == 1 && (le32(c + KCONF_PRESENCA) >> o->hueco & 1)) {
        uint16_t v = le16(c + KCONF_VALORES + 2 * o->hueco);
        *guardado = true;
        if (valido(o, v)) return v;
    }
    return valor_imagem(o, c);
}

void kconf_iniciar(void)
{
    memcpy(cab_ef, cabecalho_boot, KCONF_FIM);
    for (int i = 0; i < N_OPS; i++) val_ef[ops[i].hueco] = valor_imagem(&ops[i], cab_ef);
    int z = zona_estado(cabecalho_boot);
    bool rescate = (info_boot.opcoes & OPC_BOOT_RESCATE) != 0;
    if (rescate) estado |= KCONF_EST_RESCATE;
    if (z < 0) estado |= KCONF_EST_DANADA;
    if (z == 1) {
        pres_boot = le32(cabecalho_boot + KCONF_PRESENCA);
        for (uint32_t h = 0; h < 32; h++) {
            if (!(pres_boot >> h & 1)) continue;
            const kconf_op_t *o = h < KCONF_HUECOS ? op_do_hueco(h) : NULL;
            if (!o) { desconhecidos |= 1u << h; continue; }
            uint16_t v = le16(cabecalho_boot + KCONF_VALORES + 2 * h);
            val_lido[h] = v;
            if (rescate) { ign[h] = KCONF_IGN_RESCATE; continue; }
            if (!valido(o, v)) { ign[h] = KCONF_IGN_RANGO; continue; }
            val_ef[h] = v;
            if (o->tipo == KCONF_SINO) {
                if (v) cab_ef[o->desl] &= (uint8_t)~o->mascara;
                else cab_ef[o->desl] |= o->mascara;
            } else if (o->larg == 1) cab_ef[o->desl] = (uint8_t)v;
            else if (o->larg == 2) { cab_ef[o->desl] = (uint8_t)v; cab_ef[o->desl + 1] = (uint8_t)(v >> 8); }
        }
    }
    iniciado = true;
}

void kconf_registrar(void)
{
    if (estado & KCONF_EST_DANADA)
        klog("[KCONF] la configuracion guardada del kernel no pasa la suma: ignorada (valores de la imagen)\n");
    if (!pres_boot) return;
    if (estado & KCONF_EST_RESCATE)
        klog("[KCONF] modo rescate: la configuracion guardada del kernel NO se aplica (valores de la imagen); "
             "«kconfig --por-defecto» la borra\n");
    char lista[200];
    int k = 0;
    lista[0] = 0;
    for (int i = 0; i < N_OPS; i++) {
        const kconf_op_t *o = &ops[i];
        if (!(pres_boot >> o->hueco & 1)) continue;
        if (ign[o->hueco] == KCONF_IGN_RANGO)
            klog("[KCONF] AVISO: %s=%u fuera de rango (%u a %u%s): ignorada, vale el de la imagen (%u)\n", o->nome,
                 val_lido[o->hueco], o->tipo == KCONF_SINO ? 0 : o->min, o->max, o->tipo == KCONF_SINO ? "" : ", 0 = automatico",
                 val_ef[o->hueco]);
        else if (!ign[o->hueco] && k < (int)sizeof lista - 32)
            k += snformatar(lista + k, sizeof lista - (size_t)k, "%s%s=%u", k ? " " : "", o->nome, val_ef[o->hueco]);
    }
    if (desconhecidos) klog("[KCONF] AVISO: opciones guardadas que este kernel no conoce (huecos %x): ignoradas\n", desconhecidos);
    if (k) klog("[KCONF] aplicada la configuracion guardada: %s\n", lista);
}

uint8_t kconf_cab(uint32_t desl)
{
    if (!iniciado || desl >= KCONF_FIM) return cabecalho_boot[desl & 511];
    return cab_ef[desl];
}

uint32_t kconf_valor(int hueco)
{
    return hueco >= 0 && hueco < KCONF_HUECOS ? val_ef[hueco] : 0;
}

static void fechar_zona(uint8_t *s)
{
    po32(s + KCONF_SOMA, soma_zona(s));
}

bool kconf_esquecer_bits(uint8_t *s, uint32_t desl, uint32_t mascara)
{
    if (zona_estado(s) != 1) return false;
    uint32_t p = le32(s + KCONF_PRESENCA), antes = p;
    for (int i = 0; i < N_OPS; i++)
        if (ops[i].tipo == KCONF_SINO && ops[i].desl == desl && (ops[i].mascara & mascara)) p &= ~(1u << ops[i].hueco);
    if (p == antes) return false;
    po32(s + KCONF_PRESENCA, p);
    fechar_zona(s);
    klog("[KCONF] Configuracion cambio el byte %u de la imagen: se olvida lo guardado para esos bits\n", desl);
    return true;
}

int32_t kconf_info(uint32_t ptr, uint32_t n)
{
    if (n > (uint32_t)N_OPS) n = (uint32_t)N_OPS;
    if (n && !usuario_ok((void *)ptr, n * sizeof(info_kconf_t), true)) return ERRO_ENDERECO;
    info_kconf_t *v = (info_kconf_t *)ptr;
    info_frequencia_t f;
    bool freq = frequencia_info_kernel(&f) && f.estado == FREQ_EST_ATIVO;
    for (uint32_t i = 0; i < n; i++) {
        const kconf_op_t *o = &ops[i];
        info_kconf_t e;
        memset(&e, 0, sizeof e);
        txt_copia_n(e.nome, o->nome, sizeof e.nome);
        e.hueco = o->hueco;
        e.tipo = o->tipo;
        e.min = o->tipo == KCONF_SINO ? 0 : o->min;
        e.max = o->max;
        e.imagem = valor_imagem(o, cabecalho_boot);
        bool g;
        e.proximo = valor_em(o, cabecalho_boot, &g);
        e.guardado = g;
        if (g) e.valor_guardado = le16(cabecalho_boot + KCONF_VALORES + 2 * o->hueco);
        e.efetivo = val_ef[o->hueco];
        if (o->hueco == KC_GOBERNADOR && freq) e.efetivo = (uint16_t)(f.politica + 1);   /* el de ahora */
        e.ignorada = ign[o->hueco];
        v[i] = e;
    }
    return (int32_t)(n | (estado | ((info_boot.opcoes & OPC_BOOT_CD) ? KCONF_EST_CD : 0)) << 16);
}

typedef struct { uint32_t op, hueco, valor; } kconf_pedido_t;

static bool mudar_zona(uint8_t *s, void *ctx)
{
    kconf_pedido_t *p = ctx;
    uint8_t antes[KCONF_FIM - KCONF_ZONA];
    memcpy(antes, s + KCONF_ZONA, sizeof antes);
    if (p->op == KCONF_OP_QUITAR_TODO) {
        memset(s + KCONF_ZONA, 0, KCONF_FIM - KCONF_ZONA);
    } else {
        if (zona_estado(s) != 1) {   /* nada guardado (o dañado): se empieza de cero */
            memset(s + KCONF_ZONA, 0, KCONF_FIM - KCONF_ZONA);
            po32(s + KCONF_ASSINATURA, KCONF_ASSINATURA_VAL);
        }
        uint32_t pres = le32(s + KCONF_PRESENCA);
        uint8_t *val = s + KCONF_VALORES + 2 * p->hueco;
        if (p->op == KCONF_OP_FIJAR) {
            pres |= 1u << p->hueco;
            val[0] = (uint8_t)p->valor;
            val[1] = (uint8_t)(p->valor >> 8);
        } else {
            pres &= ~(1u << p->hueco);
            val[0] = val[1] = 0;
        }
        po32(s + KCONF_PRESENCA, pres);
        if (!pres) memset(s + KCONF_ZONA, 0, KCONF_FIM - KCONF_ZONA);   /* sin nada guardado, la zona en blanco */
        else fechar_zona(s);
    }
    return memcmp(antes, s + KCONF_ZONA, sizeof antes) != 0;
}

int32_t kconf_gravar(uint32_t pedido)
{
    kconf_pedido_t p = { pedido & 0xFF, (pedido >> 8) & 0xFF, pedido >> 16 };
    const kconf_op_t *o = NULL;
    if (p.op == KCONF_OP_FIJAR || p.op == KCONF_OP_QUITAR) {
        o = op_do_hueco(p.hueco);
        if (!o) return ERRO_INVALIDO;
        if (p.op == KCONF_OP_FIJAR && !valido(o, p.valor)) return ERRO_INVALIDO;
    } else if (p.op != KCONF_OP_QUITAR_TODO) return ERRO_INVALIDO;
    const char *nome;
    int r = cab_regravar(mudar_zona, &p, &nome);
    if (p.op == KCONF_OP_QUITAR_TODO) klog("[KCONF] configuracion guardada borrada en %s (valores de la imagen): %d\n", nome, r);
    else if (p.op == KCONF_OP_QUITAR) klog("[KCONF] %s vuelve al valor de la imagen en %s: %d\n", o->nome, nome, r);
    else klog("[KCONF] guardado %s=%u en %s: %d\n", o->nome, p.valor, nome, r);
    /* El gobernador cambia también ahora (como «frecuencia ahorro»: el límite de temperatura sigue mandando). */
    if (r == 0 && o && o->hueco == KC_GOBERNADOR && p.op == KCONF_OP_FIJAR && p.valor) {
        int32_t q = sis_frequencia(FREQ_OP_POLITICA, p.valor - 1, 0, 0, 0);
        klog("[KCONF] gobernador aplicado al momento: %d\n", q);
    }
    return r;
}
