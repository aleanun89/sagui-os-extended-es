// SPDX-License-Identifier: GPL-2.0-or-later
/* hda.c - Intel High Definition Audio (D089).
 *
 * El controlador HDA es estándar (la especificación "High Definition Audio" 1.0a
 * de Intel): todos los fabricantes (Intel, AMD, NVIDIA, VIA...) tienen los mismos
 * registros. Lo que cambia de un PC a otro es el códec, un chip aparte
 * en el "link" HDA, descrito por un árbol de "widgets": conversores (DAC),
 * mezcladores, selectores y pines (las tomas). Por eso el driver:
 *   1. reinicia el controlador y encuentra los códecs presentes (STATESTS);
 *   2. habla con cada códec por "verbos" de 32 bits: CORB (cola de
 *      comandos en memoria) y RIRB (cola de respuestas); si no
 *      responde, por la interfaz de comando inmediato;
 *   3. recorre los widgets del grupo de funciones de audio (AFG) y, para cada
 *      pin de salida analógica conectado a algo (altavoz, auriculares, salida de
 *      línea), busca un camino hasta un DAC por las listas de conexiones;
 *   4. activa el camino: selecciona la conexión en cada selector, quita el mudo de los
 *      amplificadores con ganancia de 0 dB, activa la salida del pin (y el EAPD, el
 *      amplificador externo de muchos portátiles) y pone cada DAC en el flujo 1,
 *      formato 48 kHz, 16 bits, estéreo;
 *   5. programa el primer descriptor de flujo de salida con una lista (BDL) de
 *      32 trozos que cubren un anillo de 32768 cuadros. El flujo es cíclico: la
 *      tarjeta vuelve al principio sola, y LPIB dice el byte que está leyendo.
 *
 * Todos los pines de salida encontrados suenan a la vez: es lo que garantiza sonido
 * en un portátil sin saber el modelo.
 *
 * D107: antes de encender los pines se aplica el ajuste del códec y del
 * subsistema (hda_ajustes.c; los datos de los IDT de los HP salen de Linux):
 * GPIO que encienden amplificadores y configuraciones de pines corregidas.
 * Al empezar a sonar, si hay auriculares enchufados (detección de
 * presencia), los altavoces se callan. La llamada SIS_AUDIO_HDA (102) deja
 * al programa «sonido» leer el códec y probar pines y GPIO a mano en el
 * portátil, sin recompilar. */
#include "audio.h"
#include "hda_ajustes.h"
#include "console.h"
#include "portas.h"
#include "paginacao.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"

/* Registros globales */
#define GCAP      0x00
#define GCTL      0x08
#define STATESTS  0x0E
#define INTCTL    0x20
#define INTSTS    0x24
#define CORBLBASE 0x40
#define CORBUBASE 0x44
#define CORBWP    0x48
#define CORBRP    0x4A
#define CORBCTL   0x4C
#define CORBSIZE  0x4E
#define RIRBLBASE 0x50
#define RIRBUBASE 0x54
#define RIRBWP    0x58
#define RINTCNT   0x5A
#define RIRBCTL   0x5C
#define RIRBSTS   0x5D
#define RIRBSIZE  0x5E
#define RINTCNT   0x5A
#define ICOI      0x60
#define ICII      0x64
#define ICIS      0x68
/* Descriptor de flujo (relativo a su inicio) */
#define SD_CTL    0x00
#define SD_STS    0x03
#define SD_LPIB   0x04
#define SD_CBL    0x08
#define SD_LVI    0x0C
#define SD_FMT    0x12
#define SD_BDPL   0x18
#define SD_BDPU   0x1C

/* Parámetros (verbo F00) */
#define P_FABRICANTE  0x00
#define P_SUBNOS      0x04
#define P_TIPO_GRUPO  0x05
#define P_WIDGET      0x09
#define P_PINO        0x0C
#define P_AMP_ENTRADA 0x0D
#define P_CONEXOES    0x0E
#define P_AMP_SAIDA   0x12
/* Tipos de widget (bits 23:20 de P_WIDGET) */
#define W_SAIDA       0x0
#define W_MISTURADOR  0x2
#define W_SELETOR     0x3
#define W_PINO        0x4

#define N_BDL     32
#define QUADROS   32768
#define TAG       1                     /* número del flujo en el link */
#define FORMATO   0x0011                /* 48 kHz, 16 bits, 2 canales */
#define MAX_NOS   128
#define MAX_CONEX 16

static volatile uint8_t *regs;
static uint32_t *corb, *rirb;
static uint32_t corb_n, rirb_n, rirb_lido;
static bool imediato;                   /* sin CORB/RIRB: comando inmediato */
static uint32_t sd;                     /* desplazamiento del descriptor de flujo de salida */
static uint32_t sd_indice;
static uint32_t bdl_fis, anel_fis;

static inline uint8_t  le8(uint32_t r)  { return *(volatile uint8_t *)(regs + r); }
static inline uint16_t le16(uint32_t r) { return *(volatile uint16_t *)(regs + r); }
static inline uint32_t le32(uint32_t r) { return *(volatile uint32_t *)(regs + r); }
static inline void es8(uint32_t r, uint8_t v)   { *(volatile uint8_t *)(regs + r) = v; }
static inline void es16(uint32_t r, uint16_t v) { *(volatile uint16_t *)(regs + r) = v; }
static inline void es32(uint32_t r, uint32_t v) { *(volatile uint32_t *)(regs + r) = v; }

static bool antes(uint32_t limite) { return (int32_t)(relogio_tiques() - limite) < 0; }

/* ------------------------------------------------------------ verbos */

/* Esperas de los verbos: en tics del reloj y, además, en vueltas de
 * ~1 µs, porque las llamadas al sistema (SIS_AUDIO_HDA, abrir un flujo)
 * corren con las interrupciones desactivadas y allí el reloj no avanza. */
#define VOLTAS_MAX 100000u

static bool comando_imediato(uint32_t verbo, uint32_t *resp)
{
    uint32_t lim = relogio_tiques() + 2, v = 0;
    while ((le16(ICIS) & 1) && antes(lim) && v++ < VOLTAS_MAX) porta_pausa();
    es16(ICIS, 2);                       /* limpia "respuesta válida" */
    es32(ICOI, verbo);
    es16(ICIS, 1);                       /* envía */
    lim = relogio_tiques() + 2;
    v = 0;
    while (!(le16(ICIS) & 2) && antes(lim) && v++ < VOLTAS_MAX) porta_pausa();
    if (!(le16(ICIS) & 2)) return false;
    *resp = le32(ICII);
    es16(ICIS, 2);
    return true;
}

/* D211: la autoprueba de la IRQ deja un verbo en la CORB cuya respuesta
 * puede no haberse leído aún: se descarta antes del siguiente comando. */
static volatile bool prova_pendente;

static void consumir_prova(void)
{
    uint32_t lim = relogio_tiques() + 3, voltas = 0;
    while (prova_pendente && antes(lim) && voltas++ < VOLTAS_MAX) {
        uint32_t rp = le16(RIRBWP) & 0xFF;
        while (rp != rirb_lido) {
            rirb_lido = (rirb_lido + 1) % rirb_n;
            if (!(rirb[rirb_lido * 2 + 1] & 0x10)) prova_pendente = false;   /* la nuestra (no espontánea) */
        }
        if (prova_pendente) porta_pausa();
    }
    prova_pendente = false;
}

static bool comando_corb(uint32_t verbo, uint32_t *resp)
{
    if (prova_pendente) consumir_prova();
    uint32_t wp = (le16(CORBWP) + 1) % corb_n;
    corb[wp] = verbo;
    es16(CORBWP, (uint16_t)wp);
    uint32_t lim = relogio_tiques() + 3, voltas = 0;
    while (antes(lim) && voltas++ < VOLTAS_MAX) {
        uint32_t rp = le16(RIRBWP) & 0xFF;
        if (rp != rirb_lido) {
            rirb_lido = (rirb_lido + 1) % rirb_n;
            uint32_t ex = rirb[rirb_lido * 2 + 1];
            if (ex & 0x10) continue;      /* respuesta espontánea (jack), no es la nuestra */
            *resp = rirb[rirb_lido * 2];
            es8(RIRBSTS, 5);
            return true;
        }
        porta_pausa();
    }
    return false;
}

static uint32_t verbo_cru(uint32_t cmd)
{
    uint32_t r = 0;
    if (!imediato && comando_corb(cmd, &r)) return r;
    if (!imediato) {
        klog("[HDA] CORB/RIRB sem resposta (CORBRP %x CORBWP %x RIRBWP %x RIRBSTS %x n %u/%u): comando imediato\n",
             le16(CORBRP), le16(CORBWP), le16(RIRBWP), le8(RIRBSTS), corb_n, rirb_n);
        imediato = true;
    }
    if (!comando_imediato(cmd, &r)) return 0xFFFFFFFFu;
    return r;
}

static uint32_t verbo(uint32_t cad, uint32_t no, uint32_t v, uint32_t carga)
{
    uint32_t cmd = cad << 28 | (no & 0x7F) << 20;
    if (v <= 0xF) cmd |= v << 16 | (carga & 0xFFFF);   /* verbos de 4 bits, carga de 16 */
    else cmd |= (v & 0xFFF) << 8 | (carga & 0xFF);
    return verbo_cru(cmd);
}

static uint32_t param(uint32_t cad, uint32_t no, uint32_t p) { return verbo(cad, no, 0xF00, p); }

/* ------------------------------------------------------------ el códec */

typedef struct {
    uint32_t caps, pino, config;
    uint8_t  tipo, nconex;
    uint8_t  conex[MAX_CONEX];
} widget_t;

static widget_t nos[MAX_NOS];
static uint32_t afg, primeiro_no, n_nos;
static uint32_t amp_saida_afg, amp_entrada_afg;

/* D107: el códec en uso, el ajuste y los pines encendidos (para el autosilencio y el diagnóstico). */
static uint32_t cad_uso, codec_id, codec_rev, ssid_pci, ssid_codec, n_gpio;
static hda_ajuste_t ajuste;
static bool com_ajuste;
static uint8_t gpio_m, gpio_d, gpio_v;          /* máscara, dirección y datos escritos */
static bool autosilencio = true, fones_presentes;
static bool hda_pronto;                         /* el driver hda es el que está en uso */
static uint32_t n_ligados;
static uint8_t ligado_no[HDA_MAX_PINOS], ligado_papel[HDA_MAX_PINOS], ligado_dac[HDA_MAX_PINOS];
static uint8_t ligado_ctl[HDA_MAX_PINOS];

static widget_t *widget(uint32_t n) { return n >= primeiro_no && n < primeiro_no + n_nos ? &nos[n - primeiro_no] : NULL; }

static void ler_conexoes(uint32_t cad, uint32_t n, widget_t *w)
{
    uint32_t c = param(cad, n, P_CONEXOES);
    uint32_t total = c & 0x7F;
    bool longa = c & 0x80;
    uint32_t por = longa ? 2 : 4, bits = longa ? 16 : 8;
    uint32_t anterior = 0;
    w->nconex = 0;
    for (uint32_t i = 0; i < total && w->nconex < MAX_CONEX; i += por) {
        uint32_t r = verbo(cad, n, 0xF02, i);
        for (uint32_t k = 0; k < por && i + k < total && w->nconex < MAX_CONEX; k++) {
            uint32_t e = (r >> (k * bits)) & ((1u << bits) - 1);
            uint32_t faixa = e & (1u << (bits - 1));
            e &= (1u << (bits - 1)) - 1;
            if (faixa && anterior)       /* "de anterior hasta e" */
                for (uint32_t x = anterior + 1; x <= e && w->nconex < MAX_CONEX; x++) w->conex[w->nconex++] = (uint8_t)x;
            else w->conex[w->nconex++] = (uint8_t)e;
            anterior = e;
        }
    }
}

static uint32_t amp_caps(uint32_t cad, uint32_t n, bool saida)
{
    widget_t *w = widget(n);
    if (w && (w->caps & (1u << 3)))      /* el widget tiene sus propios parámetros de amplificador */
        return param(cad, n, saida ? P_AMP_SAIDA : P_AMP_ENTRADA);
    return saida ? amp_saida_afg : amp_entrada_afg;
}

/* Quita el mudo y pone la ganancia de 0 dB (el "offset" del amplificador). */
static void amp_ligar(uint32_t cad, uint32_t n, bool saida, uint32_t indice)
{
    uint32_t caps = amp_caps(cad, n, saida);
    uint32_t ganho = caps & 0x7F;
    if (ganho > ((caps >> 8) & 0x7F)) ganho = (caps >> 8) & 0x7F;
    uint32_t carga = (saida ? 0x8000u : 0x4000u) | 0x3000u | (indice & 0xF) << 8 | ganho;
    verbo(cad, n, 0x3, carga);
}

static void amp_calar(uint32_t cad, uint32_t n, uint32_t indice)
{
    verbo(cad, n, 0x3, 0x4000u | 0x3000u | (indice & 0xF) << 8 | 0x80);
}

/* Camino de un pin hasta un DAC (búsqueda en profundidad, hasta 6 widgets). */
static int caminho[8], caminho_n;

static bool achar_dac(uint32_t n, int prof)
{
    widget_t *w = widget(n);
    if (!w || prof >= 6) return false;
    for (int i = 0; i < prof; i++) if (caminho[i] == (int)n) return false;
    caminho[prof] = (int)n;
    if (w->tipo == W_SAIDA && !(w->caps & (1u << 9))) {   /* DAC analógico */
        caminho_n = prof + 1;
        return true;
    }
    if (w->tipo != W_PINO && w->tipo != W_MISTURADOR && w->tipo != W_SELETOR) return false;
    for (int i = 0; i < w->nconex; i++)
        if (achar_dac(w->conex[i], prof + 1)) return true;
    return false;
}

static void ligar_caminho(uint32_t cad)
{
    for (int i = 0; i < caminho_n; i++) {
        uint32_t n = (uint32_t)caminho[i];
        widget_t *w = widget(n);
        if (w->caps & (1u << 10)) verbo(cad, n, 0x705, 0);   /* energía D0 */
        if (i + 1 < caminho_n) {
            int k = 0;
            while (k < w->nconex && w->conex[k] != caminho[i + 1]) k++;
            if (w->tipo == W_MISTURADOR) {
                /* mezclador: entrada nuestra abierta, las otras (bucle analógico, micrófono) calladas */
                for (int j = 0; j < w->nconex; j++)
                    if (j == k) amp_ligar(cad, n, false, (uint32_t)j); else amp_calar(cad, n, (uint32_t)j);
            } else {
                if (w->nconex > 1) verbo(cad, n, 0x701, (uint32_t)k);
                if (w->caps & (1u << 1)) amp_ligar(cad, n, false, (uint32_t)k);
            }
        }
        if (w->caps & (1u << 2)) amp_ligar(cad, n, true, 0);
        if (w->tipo == W_SAIDA) {
            verbo(cad, n, 0x2, FORMATO);
            verbo(cad, n, 0x706, TAG << 4);
        }
    }
}

static const char *nome_pino(uint32_t config)
{
    switch ((config >> 20) & 0xF) {
    case 0x0: return "salida de linea";
    case 0x1: return "altavoz";
    case 0x2: return "auriculares";
    default:  return "salida";
    }
}

/* Enciende un pin de salida: camino hasta un DAC, salida y EAPD. Devuelve false sin camino. */
static bool ligar_pino(uint32_t cad, uint32_t n)
{
    widget_t *w = widget(n);
    if (!w || w->tipo != W_PINO || !achar_dac(n, 0)) return false;
    ligar_caminho(cad);
    uint32_t ctl = 0x40;                                           /* salida activada */
    if (w->pino & (1u << 3)) ctl |= 0x80;                         /* puede alimentar auriculares */
    verbo(cad, n, 0x707, ctl);
    if (w->pino & (1u << 16)) verbo(cad, n, 0x70C, 2);            /* EAPD */
    uint32_t k = 0;
    while (k < n_ligados && ligado_no[k] != n) k++;
    if (k < HDA_MAX_PINOS) {
        uint32_t disp = (w->config >> 20) & 0xF;
        ligado_no[k] = (uint8_t)n;
        ligado_papel[k] = (uint8_t)(disp == 1 ? 1 : disp == 2 ? 2 : 0);
        ligado_dac[k] = (uint8_t)caminho[caminho_n - 1];
        ligado_ctl[k] = (uint8_t)ctl;
        if (k == n_ligados) n_ligados++;
    }
    return true;
}

/* GPIO del grupo de audio (como stac_gpio_set de Linux): suma máscara y
 * dirección a las que ya tiene el códec y cambia solo los datos de los bits de salida. */
static void gpio_escrever(uint32_t cad, uint8_t mascara, uint8_t direcao, uint8_t dados)
{
    uint32_t v = verbo(cad, afg, 0xF15, 0) & 0xFF;
    uint32_t m = verbo(cad, afg, 0xF16, 0) & 0xFF;
    uint32_t d = verbo(cad, afg, 0xF17, 0) & 0xFF;
    v = (v & ~(uint32_t)direcao) | (dados & direcao);
    m |= mascara;
    d |= direcao;
    if (com_ajuste && ajuste.verbo_gpio_cmos) verbo(cad, afg, ajuste.verbo_gpio_cmos, 0);
    verbo(cad, afg, 0x716, m);
    verbo(cad, afg, 0x717, d);
    audio_esperar_us(1000);
    verbo(cad, afg, 0x715, v);
    gpio_m = (uint8_t)m;
    gpio_d = (uint8_t)d;
    gpio_v = (uint8_t)v;
    klog("[HDA] GPIO: mascara %x direcao %x dados %x\n", m, d, v);
}

/* Antes de buscar los pines: configuraciones sustituidas, control de volumen,
 * amplificadores de entrada y el mapa de energía de los IDT. */
static void ajuste_antes(uint32_t cad)
{
    for (uint32_t i = 0; i < ajuste.n_pinos; i++) {
        widget_t *w = widget(ajuste.pino_no[i]);
        if (w && w->tipo == W_PINO) {
            klog("[HDA] ajuste: pino %u config %08x -> %08x\n", ajuste.pino_no[i], w->config, ajuste.pino_config[i]);
            w->config = ajuste.pino_config[i];
        }
    }
    if (ajuste.hp_auriculares) {
        widget_t *a = widget(ajuste.hp_pino_a), *f = widget(ajuste.hp_pino_f);
        if (a && f && a->tipo == W_PINO && f->tipo == W_PINO &&
            hda_config_saida_fones(a->config) && hda_config_saida_fones(f->config)) {
            a->config = hda_config_para_fones(a->config);
            f->config = hda_config_para_fones(f->config);
            klog("[HDA] ajuste: pinos %u e %u como fones\n", ajuste.hp_pino_a, ajuste.hp_pino_f);
        }
    }
    widget_t *vol = ajuste.no_volume ? widget(ajuste.no_volume) : NULL;
    if (vol && vol->tipo == 0x6) verbo(cad, ajuste.no_volume, 0x70F, ajuste.volume_valor);
    widget_t *w0 = ajuste.n_desmutar ? widget(ajuste.nos_desmutar[0]) : NULL;
    if (w0 && (w0->caps & (1u << 1)))
        for (uint32_t i = 0; i < ajuste.n_desmutar; i++)
            verbo(cad, ajuste.nos_desmutar[i], 0x3, 0x4000u | 0x3000u);   /* entrada 0, 0 dB, sin silencio */
    if (ajuste.verbo_mapa_energia) verbo(cad, afg, ajuste.verbo_mapa_energia, 0);
}

/* Configura un códec; devuelve cuántos pines quedaron activados. */
static int configurar_codec(uint32_t cad, char *descr, int max, uint32_t ssid_controlador)
{
    uint32_t id = param(cad, 0, P_FABRICANTE);
    if (id == 0xFFFFFFFFu || id == 0) return 0;
    uint32_t sub = param(cad, 0, P_SUBNOS);
    afg = 0;
    for (uint32_t g = (sub >> 16) & 0xFF, k = 0; k < (sub & 0xFF); k++)
        if ((param(cad, g + k, P_TIPO_GRUPO) & 0xFF) == 1) { afg = g + k; break; }
    if (!afg) return 0;
    verbo(cad, afg, 0x705, 0);           /* grupo de audio en D0 */
    audio_esperar_us(1000);
    amp_saida_afg = param(cad, afg, P_AMP_SAIDA);
    amp_entrada_afg = param(cad, afg, P_AMP_ENTRADA);
    uint32_t s = param(cad, afg, P_SUBNOS);
    primeiro_no = (s >> 16) & 0xFF;
    n_nos = s & 0xFF;
    if (n_nos > MAX_NOS) n_nos = MAX_NOS;
    for (uint32_t i = 0; i < n_nos; i++) {
        uint32_t n = primeiro_no + i;
        widget_t *w = &nos[i];
        memset(w, 0, sizeof *w);
        w->caps = param(cad, n, P_WIDGET);
        w->tipo = (uint8_t)((w->caps >> 20) & 0xF);
        if (w->caps & (1u << 8)) ler_conexoes(cad, n, w);
        if (w->tipo == W_PINO) {
            w->pino = param(cad, n, P_PINO);
            w->config = verbo(cad, n, 0xF1C, 0);
        }
    }
    cad_uso = cad;
    codec_id = id;
    codec_rev = param(cad, 0, 0x02);
    ssid_pci = ssid_controlador;
    ssid_codec = verbo(cad, afg, 0xF20, 0);
    n_gpio = param(cad, afg, 0x11) & 0xFF;
    n_ligados = 0;
    com_ajuste = hda_ajuste_escolher(id, ssid_pci, ssid_codec, n_gpio, &ajuste);
    klog("[HDA] codec %u: %08x rev %08x, subsistema PCI %08x, codec %08x, %u GPIO, ajuste %s (%s)\n", cad, id,
         codec_rev, ssid_pci, ssid_codec, n_gpio, com_ajuste ? ajuste.nome : "nenhum",
         com_ajuste ? ajuste.modelo : "-");
    if (com_ajuste) ajuste_antes(cad);
    int ligados = 0, usado = 0;
    usado += snformatar(descr, (size_t)max, "codec %04x:%04x", id >> 16, id & 0xFFFF);
    if (com_ajuste) usado += snformatar(descr + usado, (size_t)(max - usado), " [%s]", ajuste.nome);
    /* Primero los altavoces y auriculares; la salida de línea también. */
    for (int prioridade = 0; prioridade < 3; prioridade++)
        for (uint32_t i = 0; i < n_nos; i++) {
            widget_t *w = &nos[i];
            if (w->tipo != W_PINO || !(w->pino & (1u << 4))) continue;      /* necesita saber reproducir */
            if (w->caps & (1u << 9)) continue;                             /* digital (HDMI, S/PDIF) */
            if ((w->config >> 30) == 1) continue;                          /* sin conexión física */
            uint32_t disp = (w->config >> 20) & 0xF;
            if (!((prioridade == 0 && disp == 1) || (prioridade == 1 && disp == 2) || (prioridade == 2 && disp == 0)))
                continue;
            uint32_t n = primeiro_no + i;
            if (!ligar_pino(cad, n)) continue;
            if (usado < max - 1)
                usado += snformatar(descr + usado, (size_t)(max - usado), "%s %s %u", ligados ? "," : ";",
                                    nome_pino(w->config), n);
            klog("[HDA] codec %u: pino %u (%s, config %08x) -> DAC %d, %d widgets no caminho\n", cad, n,
                 nome_pino(w->config), w->config, caminho[caminho_n - 1], caminho_n);
            ligados++;
        }
    if (ligados && com_ajuste && (ajuste.gpio_mascara | ajuste.gpio_direcao))
        gpio_escrever(cad, ajuste.gpio_mascara, ajuste.gpio_direcao, ajuste.gpio_dados);
    return ligados;
}

/* ------------------------------------------------------------ auriculares y altavoces */

static bool fone_presente(uint32_t n)
{
    widget_t *w = widget(n);
    if (!w || !(w->pino & (1u << 2))) return false;              /* sin detección de presencia */
    if ((w->config >> 8) & 1) return false;                      /* «sin detección» en la configuración */
    if (w->pino & (1u << 1)) {                                   /* necesita disparo (verbo 709) */
        verbo(cad_uso, n, 0x709, 0);
        audio_esperar_us(100);
    }
    uint32_t r = verbo(cad_uso, n, 0xF09, 0);
    return r != 0xFFFFFFFFu && (r & 0x80000000u);
}

/* Al empezar a sonar: con auriculares enchufados, los altavoces se callan
 * (solo si hay altavoces y auriculares con detección; si no, nada cambia). */
static void autosilencio_aplicar(void)
{
    bool alto = false, fones = false;
    for (uint32_t k = 0; k < n_ligados; k++) {
        if (ligado_papel[k] == 1) alto = true;
        else if (ligado_papel[k] == 2 && fone_presente(ligado_no[k])) fones = true;
    }
    if (!alto) return;
    bool calar = autosilencio && fones;
    if (calar != fones_presentes) klog("[HDA] fones %s: alto-falantes %s\n", fones ? "enchufados" : "fora",
                                       calar ? "calados" : "ligados");
    fones_presentes = calar;
    for (uint32_t k = 0; k < n_ligados; k++)
        if (ligado_papel[k] == 1) verbo(cad_uso, ligado_no[k], 0x707, calar ? 0 : ligado_ctl[k]);
}

/* ------------------------------------------------------------ el flujo */

static void sd_reiniciar(uint32_t base)
{
    es8(base + SD_CTL, 0);
    uint32_t lim = relogio_tiques() + 3;
    while ((le8(base + SD_CTL) & 2) && antes(lim)) porta_pausa();
    es8(base + SD_CTL, 1);               /* SRST */
    lim = relogio_tiques() + 3;
    while (!(le8(base + SD_CTL) & 1) && antes(lim)) porta_pausa();
    es8(base + SD_CTL, 0);
    lim = relogio_tiques() + 3;
    while ((le8(base + SD_CTL) & 1) && antes(lim)) porta_pausa();
    es8(base + SD_STS, 0x1C);
}

static void fluxo_reiniciar(void) { sd_reiniciar(sd); }

static void ligar(void)
{
    autosilencio_aplicar();
    fluxo_reiniciar();
    es8(sd + SD_CTL + 2, TAG << 4);      /* número del flujo (bits 23:20) */
    es32(sd + SD_CBL, QUADROS * 4);
    es16(sd + SD_LVI, N_BDL - 1);
    es16(sd + SD_FMT, FORMATO);
    es32(sd + SD_BDPL, bdl_fis);
    es32(sd + SD_BDPU, 0);
    es32(INTCTL, le32(INTCTL) | 0x80000000u | (1u << sd_indice));
    es8(sd + SD_CTL, 0x02 | 0x04);       /* RUN + IOCE */
}

static void parar(void)
{
    fluxo_reiniciar();
}

static uint32_t posicao(void)
{
    return (le32(sd + SD_LPIB) % (QUADROS * 4)) / 4;
}

/* D211: con CIE (interrupción del controlador) en el INTCTL, la respuesta
 * a un verbo levanta la IRQ (RIRBSTS.RINTFL, con RINTCNT = 1); atender()
 * la reconoce y la limpia. El verbo es inocuo: leer el fabricante del nodo
 * raíz del códec. */
static bool provocar_irq(void)
{
    if (imediato || !corb || !corb_n || !rirb_n) return false;
    bool i0 = int_salva_desliga();
    if (prova_pendente) consumir_prova();
    es32(INTCTL, le32(INTCTL) | 0x80000000u | 0x40000000u);   /* GIE + CIE */
    uint32_t wp = (le16(CORBWP) + 1) % corb_n;
    corb[wp] = cad_uso << 28 | 0xF0000u;
    prova_pendente = true;
    es16(CORBWP, (uint16_t)wp);
    int_restaura(i0);
    return true;
}

static void fim_prova_irq(void)
{
    bool i0 = int_salva_desliga();
    uint32_t v = le32(INTCTL) & ~0x40000000u;
    if (!(v & 0x3FFFFFFFu)) v &= ~0x80000000u;   /* sin flujos con interrupción, sin GIE (como antes) */
    es32(INTCTL, v);
    if (prova_pendente) consumir_prova();
    es8(RIRBSTS, 5);
    int_restaura(i0);
}

static bool atender(void)
{
    uint32_t st = le32(INTSTS);
    if (!(st & (1u << sd_indice))) {
        if (le8(RIRBSTS) & 5) { es8(RIRBSTS, 5); return true; }
        return false;
    }
    es8(sd + SD_STS, 0x1C);
    return true;
}

/* ------------------------------------------------------------ captura (D292) */

/* La entrada de sonido: el primer descriptor de flujo de entrada (a 0x80), con su propio anillo de
 * CAP_QUADROS cuadros de 48 kHz, 16 bits, estéreo. Cada entrada (micrófono interno, externo, línea,
 * auxiliar) es un pin de entrada con un camino hasta un conversor de entrada (ADC) por selectores y
 * mezcladores; elegir una activa su camino y calla las demás entradas de los mezcladores del camino. */
#define CAP_QUADROS 16384
#define CAP_N_BDL   16
#define CAP_TAG     2
#define W_ENTRADA   0x1

typedef struct {
    uint8_t pino, adc, n;               /* n = nodos del camino (ADC primero, pin al final) */
    uint8_t cam[8], idx[8];             /* nodos y, en cada uno, la conexión hacia el siguiente */
} entrada_t;

static entrada_t ents[CAP_ENTRADAS_MAX];
static int n_ents, ent_atual = -1;
static uint32_t sd_in, bdl_in_fis, anel_in_fis;
static uint8_t *anel_in;
static captura_hw_t cap_hw;
static int cap_cam[8];
static int cap_cam_n;

static bool cap_achar(uint32_t n, uint32_t pino, int prof)
{
    widget_t *w = widget(n);
    if (!w || prof >= 6) return false;
    for (int i = 0; i < prof; i++) if (cap_cam[i] == (int)n) return false;
    cap_cam[prof] = (int)n;
    if (n == pino) { cap_cam_n = prof + 1; return true; }
    if (w->tipo != W_ENTRADA && w->tipo != W_MISTURADOR && w->tipo != W_SELETOR) return false;
    for (int i = 0; i < w->nconex; i++)
        if (cap_achar(w->conex[i], pino, prof + 1)) return true;
    return false;
}

/* Nombre de la entrada según la configuración por defecto del pin (Latin-1: es texto de la interfaz). */
static bool cap_nome(uint32_t config, char *nome, int prio_out[1])
{
    uint32_t disp = (config >> 20) & 0xF;
    bool interno = ((config >> 28) & 3) == 1;
    if (disp == 0xA) { txt_copia_n(nome, interno ? "Micr\xf3" "fono interno" : "Micr\xf3" "fono externo", 24); prio_out[0] = interno ? 0 : 1; }
    else if (disp == 0x8) { txt_copia_n(nome, "Entrada de l\xednea", 24); prio_out[0] = 2; }
    else if (disp == 0x9) { txt_copia_n(nome, "Auxiliar", 24); prio_out[0] = 3; }
    else return false;
    return true;
}

static void cap_descobrir(uint32_t cad)
{
    n_ents = 0;
    for (int prio = 0; prio < 4; prio++)
        for (uint32_t i = 0; i < n_nos && n_ents < CAP_ENTRADAS_MAX; i++) {
            widget_t *w = &nos[i];
            if (w->tipo != W_PINO || !(w->pino & (1u << 5))) continue;     /* sabe grabar */
            if (w->caps & (1u << 9)) continue;                             /* digital */
            if ((w->config >> 30) == 1) continue;                          /* sin conexión física */
            char nome[24];
            int pr[1];
            if (!cap_nome(w->config, nome, pr) || pr[0] != prio) continue;
            uint32_t pino = primeiro_no + i;
            for (uint32_t a = 0; a < n_nos; a++) {
                widget_t *adc = &nos[a];
                if (adc->tipo != W_ENTRADA || (adc->caps & (1u << 9))) continue;
                if (!cap_achar(primeiro_no + a, pino, 0)) continue;
                entrada_t *e = &ents[n_ents];
                e->pino = (uint8_t)pino;
                e->adc = (uint8_t)(primeiro_no + a);
                e->n = (uint8_t)cap_cam_n;
                for (int k = 0; k < cap_cam_n; k++) {
                    e->cam[k] = (uint8_t)cap_cam[k];
                    e->idx[k] = 0;
                    widget_t *wk = widget((uint32_t)cap_cam[k]);
                    if (k + 1 < cap_cam_n && wk)
                        for (int m = 0; m < wk->nconex; m++)
                            if (wk->conex[m] == cap_cam[k + 1]) { e->idx[k] = (uint8_t)m; break; }
                }
                /* nombre único: si ya hay otra con ese nombre, se añade el número del pin */
                for (int k = 0; k < n_ents; k++)
                    if (txt_compara(cap_hw.nombre[k], nome) == 0) {
                        int u = (int)txt_tamanho(nome);
                        if (u > 19) u = 19;
                        snformatar(nome + u, (size_t)(24 - u), " %u", pino);
                        break;
                    }
                txt_copia_n(cap_hw.nombre[n_ents], nome, 24);
                klog("[HDA] entrada %d: %s, pino %u (config %08x) -> ADC %u, %d widgets\n", n_ents, nome, pino, w->config,
                     e->adc, e->n);
                n_ents++;
                break;
            }
        }
    (void)cad;
}

/* El amplificador de entrada donde va la ganancia de grabación: el primero del camino (desde el ADC) con pasos. */
static bool cap_amp_alvo(const entrada_t *e, uint32_t *no, uint32_t *indice, uint32_t *passos)
{
    for (int j = 0; j + 1 < e->n; j++) {
        widget_t *w = widget(e->cam[j]);
        if (!w || !(w->caps & (1u << 1))) continue;
        uint32_t p = (amp_caps(cad_uso, e->cam[j], false) >> 8) & 0x7F;
        if (!p) continue;
        *no = e->cam[j]; *indice = e->idx[j]; *passos = p;
        return true;
    }
    return false;
}

static void cap_ganho(unsigned pct)
{
    if (ent_atual < 0) return;
    uint32_t no, idx, passos;
    if (!cap_amp_alvo(&ents[ent_atual], &no, &idx, &passos)) return;
    uint32_t g = passos * pct / 100;
    verbo(cad_uso, no, 0x3, 0x4000u | 0x3000u | (idx & 0xF) << 8 | g);
}

static bool cap_entrada(int i)
{
    if (i < 0 || i >= n_ents) return false;
    uint32_t cad = cad_uso;
    if (ent_atual >= 0 && ent_atual != i) verbo(cad, ents[ent_atual].pino, 0x707, 0);   /* apaga el pin anterior */
    const entrada_t *e = &ents[i];
    for (int j = 0; j < e->n; j++) {
        uint32_t n = e->cam[j];
        widget_t *w = widget(n);
        if (!w) continue;
        if (w->caps & (1u << 10)) verbo(cad, n, 0x705, 0);       /* energía D0 */
        if (j + 1 < e->n) {
            uint32_t k = e->idx[j];
            if (w->tipo == W_MISTURADOR) {
                for (int m = 0; m < w->nconex; m++)
                    if ((uint32_t)m == k) amp_ligar(cad, n, false, (uint32_t)m); else amp_calar(cad, n, (uint32_t)m);
            } else {
                if (w->nconex > 1) verbo(cad, n, 0x701, k);
                if (w->caps & (1u << 1)) amp_ligar(cad, n, false, k);
            }
        }
        if (w->tipo == W_ENTRADA) {
            verbo(cad, n, 0x2, FORMATO);
            verbo(cad, n, 0x706, CAP_TAG << 4);
        }
        if (w->tipo == W_PINO) {
            uint32_t vref = (w->pino >> 8) & 0xFF, ctl = 0x20;
            if ((w->config >> 20 & 0xF) == 0xA) ctl |= (vref & (1u << 3)) ? 4 : (vref & (1u << 1)) ? 1 : 0;   /* micrófono: VREF 80 % o 50 % */
            verbo(cad, n, 0x707, ctl);
            if (w->caps & (1u << 1)) amp_ligar(cad, n, false, 0);
        }
    }
    ent_atual = i;
    klog("[HDA] captura: entrada %d (%s)\n", i, cap_hw.nombre[i]);
    return true;
}

static bool cap_ligar(void)
{
    if (!anel_in || ent_atual < 0) return false;
    sd_reiniciar(sd_in);
    es8(sd_in + SD_CTL + 2, CAP_TAG << 4);
    es32(sd_in + SD_CBL, CAP_QUADROS * 4);
    es16(sd_in + SD_LVI, CAP_N_BDL - 1);
    es16(sd_in + SD_FMT, FORMATO);
    es32(sd_in + SD_BDPL, bdl_in_fis);
    es32(sd_in + SD_BDPU, 0);
    es8(sd_in + SD_CTL, 0x02);           /* RUN, sin interrupción: el tic del reloj vacía el anillo */
    return true;
}

static void cap_parar(void) { sd_reiniciar(sd_in); }

static uint32_t cap_posicao(void) { return (le32(sd_in + SD_LPIB) % (CAP_QUADROS * 4)) / 4; }

/* Prepara la captura del códec ya configurado: anillo, lista de descriptores y entradas. */
static void cap_preparar(uint32_t cad, uint32_t iss)
{
    memset(&cap_hw, 0, sizeof cap_hw);
    n_ents = 0;
    ent_atual = -1;
    if (!iss) { klog("[HDA] sin flujos de entrada: sin captura\n"); return; }
    cap_descobrir(cad);
    if (!n_ents) { klog("[HDA] el codec no tiene entradas analogicas: sin captura\n"); return; }
    void *mem;
    uint32_t fis = audio_dma_alocar(1 + CAP_QUADROS * 4 / 4096, &mem);
    if (!fis) { klog("[HDA] sin memoria para la captura\n"); n_ents = 0; return; }
    sd_in = 0x80;
    bdl_in_fis = fis;
    anel_in_fis = fis + 4096;
    anel_in = (uint8_t *)mem + 4096;
    uint32_t *bdl = (uint32_t *)mem;
    for (int i = 0; i < CAP_N_BDL; i++) {
        bdl[4 * i] = anel_in_fis + (uint32_t)i * (CAP_QUADROS * 4 / CAP_N_BDL);
        bdl[4 * i + 1] = 0;
        bdl[4 * i + 2] = CAP_QUADROS * 4 / CAP_N_BDL;
        bdl[4 * i + 3] = 0;              /* sin IOC: no interrumpe */
    }
    sd_reiniciar(sd_in);
    cap_hw.taxa = 48000;
    cap_hw.quadros = CAP_QUADROS;
    cap_hw.anel = (const volatile int16_t *)anel_in;
    cap_hw.n_entradas = n_ents;
    cap_hw.ligar = cap_ligar;
    cap_hw.parar = cap_parar;
    cap_hw.posicao = cap_posicao;
    cap_hw.entrada = cap_entrada;
    cap_hw.ganho = cap_ganho;
}

/* ------------------------------------------------------------ inicio */

static bool iniciar_corb(uint32_t mem_fis, uint8_t *mem)
{
    corb = (uint32_t *)mem;
    rirb = (uint32_t *)(mem + 1024);
    es8(CORBCTL, 0);
    es8(RIRBCTL, 0);
    uint32_t lim = relogio_tiques() + 3;
    while (((le8(CORBCTL) & 2) || (le8(RIRBCTL) & 2)) && antes(lim)) porta_pausa();
    /* Tamaño: el mayor que acepta el controlador (256, 16 o 2 entradas). */
    uint8_t cs = le8(CORBSIZE), rs = le8(RIRBSIZE);
    uint8_t ct = (cs & 0x40) ? 2 : (cs & 0x20) ? 1 : 0, rt = (rs & 0x40) ? 2 : (rs & 0x20) ? 1 : 0;
    es8(CORBSIZE, (cs & ~3u) | ct);
    es8(RIRBSIZE, (rs & ~3u) | rt);
    corb_n = ct == 2 ? 256 : ct == 1 ? 16 : 2;
    rirb_n = rt == 2 ? 256 : rt == 1 ? 16 : 2;
    es32(CORBLBASE, mem_fis);
    es32(CORBUBASE, 0);
    es32(RIRBLBASE, mem_fis + 1024);
    es32(RIRBUBASE, 0);
    /* Pone a cero el puntero de lectura de la CORB (algunos controladores no muestran el bit: solo un límite). */
    es16(CORBRP, 0x8000);
    lim = relogio_tiques() + 2;
    while (!(le16(CORBRP) & 0x8000) && antes(lim)) porta_pausa();
    es16(CORBRP, 0);
    lim = relogio_tiques() + 2;
    while ((le16(CORBRP) & 0x8000) && antes(lim)) porta_pausa();
    es16(CORBWP, 0);
    es16(RIRBWP, 0x8000);
    es16(RINTCNT, 1);
    rirb_lido = 0;
    es8(CORBCTL, 2);
    /* RINTCTL activado (el bit de estado en cada respuesta, sin interrupción: el
     * CIE del INTCTL queda desactivado). Sin él, el QEMU deja de leer la CORB
     * después de RINTCNT respuestas, esperando a que se borre el estado. */
    es16(RINTCNT, 1);                    /* D211: el aviso tras cada respuesta (como Linux) */
    es8(RIRBCTL, 3);
    lim = relogio_tiques() + 3;
    while ((!(le8(CORBCTL) & 2) || !(le8(RIRBCTL) & 2)) && antes(lim)) porta_pausa();
    return (le8(CORBCTL) & 2) && (le8(RIRBCTL) & 2);
}

static bool tentar(pci_disp_t *d, placa_som_t *p)
{
    uint32_t bar = pci_le32(d, 0x10);
    if (bar & 1) return false;
    if ((bar & 6) == 4 && pci_le32(d, 0x14)) { klog("[HDA] BAR acima de 4 GB\n"); return false; }
    pci_escreve32(d, 0x04, (pci_le32(d, 0x04) | 0x6) & ~(1u << 10));
    regs = (volatile uint8_t *)janela_mapear(bar & ~0xFu, 16384);
    if (!regs) return false;

    /* Reinicio del controlador: CRST a 0 y de vuelta a 1. */
    es32(INTCTL, 0);
    es8(CORBCTL, 0);
    es8(RIRBCTL, 0);
    es32(GCTL, le32(GCTL) & ~1u);
    uint32_t lim = relogio_tiques() + 5;
    while ((le32(GCTL) & 1) && antes(lim)) porta_pausa();
    audio_esperar_us(200);
    es32(GCTL, le32(GCTL) | 1);
    lim = relogio_tiques() + 5;
    while (!(le32(GCTL) & 1) && antes(lim)) porta_pausa();
    if (!(le32(GCTL) & 1)) { klog("[HDA] controlador nao saiu do reset\n"); return false; }
    relogio_dormir_ms(20);               /* los códecs se presentan en hasta 521 µs; margen */
    uint16_t presentes = le16(STATESTS);
    uint16_t gcap = le16(GCAP);
    uint32_t iss = (gcap >> 8) & 0xF, oss = (gcap >> 12) & 0xF;
    if (!oss) { klog("[HDA] sem fluxos de saida\n"); return false; }
    sd_indice = iss;
    sd = 0x80 + iss * 0x20;

    uint8_t *mem;
    uint32_t mem_fis = audio_dma_alocar(2 + QUADROS * 4 / 4096, (void **)&mem);
    if (!mem_fis) { klog("[HDA] sem memoria\n"); return false; }
    imediato = !iniciar_corb(mem_fis, mem);
    if (imediato) klog("[HDA] CORB/RIRB nao ligou: comando imediato\n");
    bdl_fis = mem_fis + 4096;            /* la BDL necesita 128 bytes de alineación */
    uint32_t *bdl = (uint32_t *)(mem + 4096);
    anel_fis = mem_fis + 8192;
    for (int i = 0; i < N_BDL; i++) {
        bdl[4 * i] = anel_fis + (uint32_t)i * (QUADROS * 4 / N_BDL);
        bdl[4 * i + 1] = 0;
        bdl[4 * i + 2] = QUADROS * 4 / N_BDL;
        bdl[4 * i + 3] = 1;              /* IOC */
    }

    char descr[64] = "";
    int ligados = 0;
    /* Subsistema PCI (registro 0x2C: fabricante en los bits 15:0, modelo en 31:16) como fabricante << 16 | modelo */
    uint32_t ssid = pci_le32(d, 0x2C);
    ssid = ssid << 16 | ssid >> 16;
    for (uint32_t cad = 0; cad < 15 && !ligados; cad++)
        if (presentes & (1u << cad)) ligados = configurar_codec(cad, descr, sizeof descr, ssid);
    if (!ligados) { klog("[HDA] nenhum codec com saida analogica (STATESTS %x)\n", presentes); return false; }

    hda_pronto = true;
    p->driver = "hda";
    snformatar(p->placa, sizeof p->placa, "Intel HDA %04x:%04x", d->fabricante, d->modelo);
    txt_copia_n(p->codec, descr, sizeof p->codec);
    p->pci = *d;
    p->irq = (int)(pci_le32(d, 0x3C) & 0xFF);
    p->taxa = 48000;
    p->quadros = QUADROS;
    p->anel = (int16_t *)(mem + 8192);
    p->ligar = ligar;
    p->parar = parar;
    p->posicao = posicao;
    p->atender = atender;
    p->provocar_irq = provocar_irq;
    p->fim_prova_irq = fim_prova_irq;
    p->cap = (captura_hw_t){ 0 };
    cap_preparar(cad_uso, iss);
    p->cap = cap_hw;
    fluxo_reiniciar();
    klog("[HDA] %02x:%02x.%x (%04x:%04x) em %x, irq %d, %u fluxos de saida, %s\n", d->barramento, d->dispositivo,
         d->funcao, d->fabricante, d->modelo, bar & ~0xFu, p->irq, oss, descr);
    return true;
}

/* ------------------------------------------------------------ SIS_AUDIO_HDA (D107) */

int32_t hda_chamada(uint32_t op, uint32_t a, uint32_t b)
{
    if (!hda_pronto) return ERRO_NAO_EXISTE;
    switch (op) {
    case HDA_OP_VERBO: {
        uint32_t *r = (uint32_t *)b;
        if (!usuario_ok(r, sizeof *r, true)) return ERRO_ENDERECO;
        *r = verbo_cru(cad_uso << 28 | (a & 0x07FFFFFFu));
        return 0;
    }
    case HDA_OP_ESTADO: {
        info_hda_t *v = (info_hda_t *)a;
        if (!usuario_ok(v, sizeof *v, true)) return ERRO_ENDERECO;
        memset(v, 0, sizeof *v);
        v->codec = (unsigned char)cad_uso;
        v->afg = (unsigned char)afg;
        v->primeiro_no = (unsigned char)primeiro_no;
        v->n_nos = (unsigned char)n_nos;
        v->id = codec_id;
        v->revisao = codec_rev;
        v->ssid_pci = ssid_pci;
        v->ssid_codec = ssid_codec;
        if (com_ajuste) {
            txt_copia_n(v->ajuste, ajuste.nome, sizeof v->ajuste);
            txt_copia_n(v->modelo, ajuste.modelo, sizeof v->modelo);
        }
        v->n_gpio = (unsigned char)n_gpio;
        v->gpio_mascara = gpio_m;
        v->gpio_direcao = gpio_d;
        v->gpio_dados = gpio_v;
        v->autosilencio = autosilencio;
        v->fones_presentes = fones_presentes;
        v->n_pinos = (unsigned char)n_ligados;
        for (uint32_t k = 0; k < n_ligados; k++) {
            widget_t *w = widget(ligado_no[k]);
            v->pino[k] = ligado_no[k];
            v->pino_papel[k] = ligado_papel[k];
            v->pino_dac[k] = ligado_dac[k];
            v->pino_config[k] = w ? w->config : 0;
        }
        return 0;
    }
    case HDA_OP_PINO: {
        widget_t *w = widget(a);
        if (!w || w->tipo != W_PINO) return ERRO_INVALIDO;
        if (b) {
            if (!ligar_pino(cad_uso, a)) return ERRO_NAO_EXISTE;
            klog("[HDA] pino %u ligado a mao (DAC %d)\n", a, caminho[caminho_n - 1]);
        } else {
            verbo(cad_uso, a, 0x707, 0);
            for (uint32_t k = 0; k < n_ligados; k++)
                if (ligado_no[k] == a) {     /* sale de la lista (el autosilencio no lo vuelve a encender) */
                    n_ligados--;
                    ligado_no[k] = ligado_no[n_ligados];
                    ligado_papel[k] = ligado_papel[n_ligados];
                    ligado_dac[k] = ligado_dac[n_ligados];
                    ligado_ctl[k] = ligado_ctl[n_ligados];
                    break;
                }
            klog("[HDA] pino %u desligado a mao\n", a);
        }
        return 0;
    }
    case HDA_OP_GPIO:
        if (b & 0xFF) gpio_escrever(cad_uso, (uint8_t)b, (uint8_t)b, (uint8_t)a);
        else if (com_ajuste) gpio_escrever(cad_uso, ajuste.gpio_mascara, ajuste.gpio_direcao, ajuste.gpio_dados);
        return 0;
    case HDA_OP_AUTOSILENCIO:
        if ((int32_t)a >= 0) {
            autosilencio = a != 0;
            autosilencio_aplicar();
        }
        return autosilencio;
    default:
        return ERRO_INVALIDO;
    }
}

bool hda_procurar(placa_som_t *p)
{
    pci_disp_t v[4];
    int n = pci_procurar_classe(0x04, 0x03, v, 4);   /* multimedia, HD Audio */
    for (int i = 0; i < n; i++)
        if (tentar(&v[i], p)) return true;
    return false;
}
