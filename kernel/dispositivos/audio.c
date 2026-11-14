// SPDX-License-Identifier: GPL-2.0-or-later
/* audio.c - el mezclador de sonido del kernel y las llamadas de audio (D088-D090).
 *
 * La tarjeta (AC'97 o Intel HDA, ver ac97.c y hda.c) reproduce sin parar un anillo
 * de cuadros PCM de 16 bits estéreo a su tasa (48 kHz). El mezclador va
 * siempre LATENCIA_MS por delante del cuadro que la tarjeta está leyendo: en cada tic
 * del reloj (10 ms) y en cada interrupción de la tarjeta lee la posición, suma el
 * trozo siguiente de todos los flujos abiertos y escribe en el anillo. Ejecutar en los
 * dos lugares es lo que da la "sondeo como reserva": sin IRQ (línea ocupada
 * o que nunca llega) el tic por sí solo mantiene el anillo lleno.
 *
 * Cada flujo es de un programa: tasa entre 4 y 96 kHz, mono o estéreo, 8 o
 * 16 bits. Lo que escribe el programa pasa a 16 bits estéreo al momento (en un anillo
 * de FLUXO_QUADROS a su tasa); la conversión de tasa se hace en la mezcla,
 * con interpolación lineal y paso en punto fijo 16.16 (el kernel no usa la
 * FPU). Quien escribe con el anillo del flujo lleno duerme hasta que el mezclador
 * consuma.
 *
 * Volumen: uno por flujo y el maestro, de 0 a 100, con curva cuadrática (el
 * oído percibe el volumen de forma logarítmica; con la curva, 50 suena como
 * "mitad" y no casi igual a 100). 100 x 100 = ganancia 1: a 48 kHz el sonido sale
 * igual a lo que escribió el programa, bit a bit. El volumen de los amplificadores
 * de la tarjeta queda fijo en 0 dB.
 *
 * Sin flujos durante 2 s, el DMA para (ahorra el bus); el primer flujo
 * abierto lo reactiva desde el inicio del anillo. El anillo tiene 32768 cuadros (680 ms):
 * margen para que la tarjeta avance mucho entre dos miradas sin dar la vuelta. */
#include "irq_rotas.h"
#include "audio.h"
#include "console.h"
#include "eq_fixo.h"
#include "cpu.h"
#include "memoria.h"
#include "paginacao.h"
#include "portas.h"
#include "processo.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"

#define LATENCIA_MS    100
#define LATENCIA_MAX_MS 300
#define FLUXO_QUADROS  8192          /* potencia de 2 */
#define BLOCO          1024          /* cuadros mezclados a la vez */
#define OCIOSO_TIQUES  (2 * TIQUES_POR_SEGUNDO)

struct fluxo {
    bool      usado;
    int       pid;
    uint32_t  taxa;
    uint8_t   canais, bits, volume;
    uint32_t  passo;                 /* tasa del flujo / tasa de la tarjeta, 16.16 */
    uint32_t  fase;                  /* fracción entre buf[ini] y buf[ini + 1], 16.16 */
    int16_t  *buf;                   /* FLUXO_QUADROS cuadros estéreo */
    uint32_t  ini, qtd;
    uint32_t  fim_saida;             /* cuadro de la salida (cuenta absoluta) donde cayó la última muestra */
    uint32_t  fome;                  /* cuadros de la salida en que el flujo abierto no tenía qué reproducir */
};
typedef struct fluxo fluxo_t;

static placa_som_t placa;
static int estado = AUDIO_SEM_PLACA;
static pci_disp_t sem_driver;        /* AUDIO_SEM_DRIVER: la tarjeta que quedó sin driver */
static bool pronta, tocando, com_irq;
static uint32_t escrito, tocado;     /* cuadros de la salida desde que el DMA se activó */
static uint32_t pos_ant, latencia;
static uint32_t total_tocado, irqs, falhas, salto_max;
static uint32_t ocioso_desde;
static uint8_t volume_mestre = 100;
static bool calado;
static fluxo_t fluxos[AUDIO_FLUXOS];
static int32_t mistura[BLOCO * 2];

/* Ecualizador (D201): ver eq_fixo.h. Sin etapas no se toca nada: la salida es idéntica bit a bit. */
static eq_audio_t eq;
static eq_estado_t eq_est[2][EQ_ETAPAS];

static uint32_t ganho(uint32_t v) { return v * v * 65536u / 10000u; }   /* 100 -> 65536 = 1,0 */

/* ------------------------------------------------------------ ayudas para los drivers */

/* Memoria para DMA: el kernel ve la física 1:1 (D008), toda por debajo de
 * 4 GB. La CPU usa un alias sin caché (PCD|PWT): así lo que escribe
 * llega a la memoria incluso en controladores que leen sin "snoop" de la caché
 * (hay chipsets HDA así) y lo que graba la tarjeta (la RIRB del HDA) se lee de la
 * memoria. El WBINVD vacía antes lo que la caché tenía de esas páginas. */
uint32_t audio_dma_alocar(uint32_t paginas, void **cpu)
{
    uint32_t f = paginas == 1 ? memfisica_alocar() : memfisica_alocar_seguidas(paginas);
    if (!f) return 0;
    memset((void *)f, 0, paginas * PAGINA);
    __asm__ volatile("wbinvd" ::: "memory");
    uint32_t v = janela_mapear_tipo(f, paginas * PAGINA, PG_PCD | PG_PWT);
    if (!v) return 0;
    *cpu = (void *)v;
    return f;
}

/* El puerto 0x80 tarda ~1 µs en un PC (en un emulador, menos: los drivers siempre
 * esperan una condición, con un límite en tics del reloj). */
void audio_esperar_us(uint32_t us)
{
    while (us--) porta_pausa();
}

/* ------------------------------------------------------------ mezclador */

static uint32_t mistura_fluxo(fluxo_t *f, uint32_t n, uint32_t g)
{
    uint32_t pos = f->fase, i;
    for (i = 0; i < n; i++) {
        uint32_t k = pos >> 16, fr = (pos & 0xFFFF) >> 1;   /* 15 bits: (b - a) * fr cabe en 32 bits */
        if (k >= f->qtd || (fr && k + 1 >= f->qtd)) break;   /* el programa todavía no escribió */
        const int16_t *a = &f->buf[((f->ini + k) & (FLUXO_QUADROS - 1)) * 2];
        int32_t l = a[0], r = a[1];
        if (fr) {
            const int16_t *b = &f->buf[((f->ini + k + 1) & (FLUXO_QUADROS - 1)) * 2];
            l += ((b[0] - l) * (int32_t)fr) >> 15;
            r += ((b[1] - r) * (int32_t)fr) >> 15;
        }
        mistura[2 * i] += (l * (int32_t)g) >> 16;
        mistura[2 * i + 1] += (r * (int32_t)g) >> 16;
        pos += f->passo;
    }
    uint32_t usados = pos >> 16;
    if (usados > f->qtd) usados = f->qtd;
    f->ini = (f->ini + usados) & (FLUXO_QUADROS - 1);
    f->qtd -= usados;
    f->fase = pos & 0xFFFF;
    return i;
}

static void misturar(uint32_t n)
{
    memset(mistura, 0, n * 2 * sizeof mistura[0]);
    uint32_t gm = calado ? 0 : ganho(volume_mestre);
    for (int k = 0; k < AUDIO_FLUXOS; k++) {
        fluxo_t *f = &fluxos[k];
        if (!f->usado) continue;
        if (!f->qtd) { f->fome += n; continue; }
        uint32_t g = (uint32_t)(((uint64_t)ganho(f->volume) * gm) >> 16);
        uint32_t feitos = mistura_fluxo(f, n, g);
        f->fome += n - feitos;
        if (feitos) f->fim_saida = escrito + feitos;
        acordar(f);   /* quien esperaba espacio para escribir */
    }
}

static void atualizar_posicao(void)
{
    uint32_t p = placa.posicao() % placa.quadros;
    uint32_t d = (p + placa.quadros - pos_ant) % placa.quadros;
    pos_ant = p;
    if (d > salto_max) salto_max = d;
    /* Lo que la tarjeta ya reprodujo pasa a silencio: si un día ella pasa delante
     * del mezclador, reproduce silencio en vez de repetir sonido viejo del anillo. */
    for (uint32_t i = 0, off = tocado & (placa.quadros - 1); i < d; i++, off = (off + 1) & (placa.quadros - 1))
        ((volatile uint32_t *)placa.anel)[off] = 0;
    tocado += d;
    total_tocado += d;
}

static void bombear(void)
{
    if (!tocando) return;
    atualizar_posicao();
    if ((int32_t)(escrito - tocado) < 0) {
        /* La tarjeta pasó delante: hubo un hueco en el sonido. Casi siempre es
         * el kernel que estuvo mucho tiempo con las interrupciones desactivadas
         * (en QEMU, una lectura del pendrive USB puede tardar cientos de ms).
         * La distancia por delante crece 20 ms por hueco, hasta 300 ms. */
        falhas++;
        escrito = tocado;
        if (latencia < placa.taxa * LATENCIA_MAX_MS / 1000) latencia += placa.taxa / 50;
    }
    uint32_t alvo = tocado + latencia;
    while ((int32_t)(alvo - escrito) > 0) {
        uint32_t n = alvo - escrito, off = escrito & (placa.quadros - 1);
        if (n > placa.quadros - off) n = placa.quadros - off;
        if (n > BLOCO) n = BLOCO;
        misturar(n);
        volatile int16_t *d = placa.anel + off * 2;
        for (uint32_t i = 0; i < n * 2; i++) {
            int32_t v = mistura[i];
            v = v > 32767 ? 32767 : v < -32768 ? -32768 : v;
            d[i] = eq.etapas ? (int16_t)eq_filtrar(&eq, eq_est[i & 1], v) : (int16_t)v;
        }
        escrito += n;
    }
    bool algum = false;
    for (int k = 0; k < AUDIO_FLUXOS; k++) algum |= fluxos[k].usado;
    if (algum) ocioso_desde = 0;
    else if (!ocioso_desde) ocioso_desde = relogio_tiques() | 1;
    else if (relogio_tiques() - ocioso_desde > OCIOSO_TIQUES) {
        placa.parar();
        tocando = false;
        klog("[AUDIO] parado (sem fluxos); %u quadros tocados, %u falhas, maior salto da placa %u quadros\n",
             total_tocado, falhas, salto_max);
    }
}

static void ligar(void)
{
    if (tocando) return;
    for (uint32_t i = 0; i < placa.quadros * 2; i++) placa.anel[i] = 0;
    escrito = tocado = pos_ant = 0;
    ocioso_desde = 0;
    latencia = placa.taxa * LATENCIA_MS / 1000;
    placa.ligar();
    tocando = true;
    klog("[AUDIO] tocando\n");
    bombear();
}

static bool ao_irq(void)
{
    if (!pronta || !placa.atender()) return false;
    irqs++;
    bombear();
    return true;
}

void audio_tique(void)
{
    if (pronta) bombear();
}

/* ------------------------------------------------------------ inicio */

/* D211: la autoprueba de la IRQ (irq_rotas.c). */
static bool sonda_provocar(void *ctx) { (void)ctx; return pronta && placa.provocar_irq && placa.provocar_irq(); }
static uint32_t sonda_contador(void *ctx) { (void)ctx; return irqs; }
static void sonda_terminar(void *ctx, bool chegou) { (void)ctx; (void)chegou; if (placa.fim_prova_irq) placa.fim_prova_irq(); }

void audio_iniciar(void)
{
    memset(&placa, 0, sizeof placa);
    bool ok = hda_procurar(&placa);
    if (!ok) { memset(&placa, 0, sizeof placa); ok = ac97_procurar(&placa); }
    if (!ok) {
        pci_disp_t v[4];
        int n = pci_procurar_classe(0x04, 0x03, v, 4);
        if (!n) n = pci_procurar_classe(0x04, 0x01, v, 4);
        if (n) {
            estado = AUDIO_SEM_DRIVER;
            sem_driver = v[0];
            klog("[AUDIO] placa de som %04x:%04x sem driver\n", v[0].fabricante, v[0].modelo);
        }
        return;
    }
    estado = AUDIO_FUNCIONANDO;
    latencia = placa.taxa * LATENCIA_MS / 1000;
    com_irq = false;
    if (placa.irq > 0 && placa.irq < 16 && placa.irq != 2) {
        irq_compartilhar((uint8_t)placa.irq, ao_irq);
        com_irq = true;
    }
    bool i0 = int_salva_desliga();
    pronta = true;
    int_restaura(i0);
    if (com_irq && placa.provocar_irq) {
        irq_sonda_t s = { "HDA", placa.pci, (uint8_t)placa.irq, sonda_provocar, sonda_contador, sonda_terminar, NULL };
        irq_sonda_registrar(&s);
    }
    klog("[AUDIO] %s: %s; %s; %u Hz, %s\n", placa.driver, placa.placa, placa.codec, placa.taxa,
         com_irq ? "com interrupcao" : "por sondagem a cada tique");
}

void audio_linha_boot(char *s, int max, bool *aviso)
{
    *aviso = false;
    s[0] = 0;
    if (estado == AUDIO_SEM_DRIVER) {
        *aviso = true;
        snformatar(s, (size_t)max, "Tarjeta de sonido %04x:%04x sin driver (solo el altavoz del PC)",
                   sem_driver.fabricante, sem_driver.modelo);
    } else if (estado == AUDIO_FUNCIONANDO) {
        snformatar(s, (size_t)max, "Sonido: %s, %u kHz (comandos reproducir, volumen)",
                   placa.placa, placa.taxa / 1000);
    }
}

/* ------------------------------------------------------------ flujos */

static uint32_t pendente_ms(const fluxo_t *f)
{
    uint32_t ms = f->qtd * 1000u / f->taxa;
    if (tocando && (int32_t)(f->fim_saida - tocado) > 0) ms += (f->fim_saida - tocado) * 1000u / placa.taxa;
    return ms;
}

int32_t sis_audio_abrir(uint32_t taxa, uint32_t formato, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    if (estado != AUDIO_FUNCIONANDO) return ERRO_NAO_EXISTE;
    uint32_t canais = formato & 0xFF, bits = (formato >> 8) & 0xFF;
    if (taxa < AUDIO_TAXA_MIN || taxa > AUDIO_TAXA_MAX || (canais != 1 && canais != 2) || (bits != 8 && bits != 16))
        return ERRO_INVALIDO;
    fluxo_t *f = NULL;
    for (int k = 0; k < AUDIO_FLUXOS && !f; k++)
        if (!fluxos[k].usado) f = &fluxos[k];
    if (!f) return ERRO_LIMITE;
    int16_t *buf = kmalloc(FLUXO_QUADROS * 4);
    if (!buf) return ERRO_SEM_MEMORIA;
    memset(f, 0, sizeof *f);
    f->buf = buf;
    f->pid = processo_atual()->pid;
    f->taxa = taxa;
    f->canais = (uint8_t)canais;
    f->bits = (uint8_t)bits;
    f->volume = 100;
    /* (taxa << 16) / tasa de la tarjeta sin pasar de 32 bits (el kernel no tiene la división de 64) */
    f->passo = (taxa / placa.taxa << 16) + ((taxa % placa.taxa) << 16) / placa.taxa;
    int fd = arquivos_novo_audio(f);
    if (fd < 0) { kfree(buf); return fd; }
    f->usado = true;
    klog("[AUDIO] fluxo %d do processo %d: %u Hz, %u canal(is), %u bits\n", (int)(f - fluxos), f->pid, taxa, canais, bits);
    ligar();
    return fd;
}

/* Convierte lo que llegó del programa a 16 bits estéreo en el anillo del flujo.
 * Acepta solo cuadros enteros; menos de un cuadro se descarta. */
int32_t audio_fluxo_escrever(fluxo_t *f, const uint8_t *buf, uint32_t n, bool sem_bloquear)
{
    uint32_t tq = (uint32_t)f->canais * (f->bits / 8u);
    uint32_t quadros = n / tq;
    if (!quadros) return (int32_t)n;
    uint32_t livres = FLUXO_QUADROS - f->qtd;
    if (!livres) {
        if (sem_bloquear) return 0;
        if (!tocando) ligar();
        bloquear(f);
        return SIS_REPETIR;
    }
    if (quadros > livres) quadros = livres;
    for (uint32_t i = 0; i < quadros; i++) {
        int16_t l, r;
        if (f->bits == 8) {
            l = (int16_t)((buf[0] - 128) << 8);
            r = f->canais == 2 ? (int16_t)((buf[1] - 128) << 8) : l;
        } else {
            l = (int16_t)(buf[0] | buf[1] << 8);
            r = f->canais == 2 ? (int16_t)(buf[2] | buf[3] << 8) : l;
        }
        buf += tq;
        int16_t *d = &f->buf[((f->ini + f->qtd) & (FLUXO_QUADROS - 1)) * 2];
        d[0] = l;
        d[1] = r;
        f->qtd++;
    }
    if (!tocando) ligar();
    return (int32_t)(quadros * tq);
}

void audio_fluxo_fechar(fluxo_t *f)
{
    bool i0 = int_salva_desliga();
    f->usado = false;
    kfree(f->buf);
    f->buf = NULL;
    acordar(f);
    int_restaura(i0);
    klog("[AUDIO] fluxo %d fechado; %u ms sem dados do programa; %u falhas, maior salto %u, latencia %u\n",
         (int)(f - fluxos), f->fome / (placa.taxa / 1000), falhas, salto_max, latencia);
}

int32_t sis_audio_escrever(uint32_t fd, uint32_t buf, uint32_t n, uint32_t sem_bloquear, uint32_t e)
{
    (void)e;
    fluxo_t *f = arquivos_fluxo(fd);
    if (!f) return ERRO_DESCRITOR;
    if (!usuario_ok((void *)buf, n, false)) return ERRO_ENDERECO;
    return audio_fluxo_escrever(f, (const uint8_t *)buf, n, sem_bloquear != 0);
}

int32_t sis_audio_controle(uint32_t fd, uint32_t op, uint32_t valor, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    int32_t v = (int32_t)valor;
    if (estado != AUDIO_FUNCIONANDO) return ERRO_NAO_EXISTE;
    if (op == AUDIO_SILENCIO) {
        if (v >= 0) {
            calado = v != 0;
            klog("[AUDIO] %s\n", calado ? "silencio" : "som ligado");
        }
        return calado;
    }
    if (op == AUDIO_VOLUME && (int32_t)fd == -1) {
        if (v >= 0) {
            volume_mestre = (uint8_t)(v > 100 ? 100 : v);
            klog("[AUDIO] volume mestre %u\n", volume_mestre);
        }
        return volume_mestre;
    }
    if (op == AUDIO_EQ && (int32_t)fd == -1) {
        if (valor) {
            const eq_audio_t *u = (const eq_audio_t *)valor;
            if (!usuario_ok((void *)u, sizeof *u, false)) return ERRO_ENDERECO;
            eq_audio_t n;
            memcpy(&n, u, sizeof n);
            if (!eq_valido(&n)) return ERRO_INVALIDO;
            bool i0 = int_salva_desliga();
            eq = n;
            memset(eq_est, 0, sizeof eq_est);   /* estado limpio con los coeficientes nuevos */
            int_restaura(i0);
            klog("[AUDIO] ecualizador %s: %d %d %d %d %d dB, %u etapas\n", n.etapas ? "activo" : "plano",
                 n.db[0], n.db[1], n.db[2], n.db[3], n.db[4], n.etapas);
        }
        return (int32_t)eq.etapas;
    }
    fluxo_t *f = arquivos_fluxo(fd);
    if (!f) return ERRO_DESCRITOR;
    switch (op) {
    case AUDIO_VOLUME:
        if (v >= 0) f->volume = (uint8_t)(v > 100 ? 100 : v);
        return f->volume;
    case AUDIO_PENDENTE:
        return (int32_t)pendente_ms(f);
    case AUDIO_LIVRE:
        return (int32_t)((FLUXO_QUADROS - f->qtd) * f->canais * (f->bits / 8u));
    case AUDIO_DESCARTAR:
        f->qtd = 0;
        f->fase = 0;
        acordar(f);
        return 0;
    default:
        return ERRO_INVALIDO;
    }
}

/* fechar(fd, esperar): con esperar, duerme (de tic en tic) hasta la última
 * muestra salga de la tarjeta y solo entonces cierra. */
int32_t sis_audio_fechar(uint32_t fd, uint32_t esperar, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    fluxo_t *f = arquivos_fluxo(fd);
    if (!f) return ERRO_DESCRITOR;
    if (esperar && tocando && pendente_ms(f) > 0) {
        processo_atual()->acordar_em = relogio_tiques() + 1;
        bloquear(f);
        return SIS_REPETIR;
    }
    return sis_fechar(fd, 0, 0, 0, 0);
}

/* SIS_AUDIO_HDA (D107): diagnóstico del códec; solo con el driver hda. */
int32_t sis_audio_hda(uint32_t op, uint32_t a, uint32_t b, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    if (estado != AUDIO_FUNCIONANDO || txt_compara(placa.driver, "hda") != 0) return ERRO_NAO_EXISTE;
    return hda_chamada(op, a, b);
}

/* ------------------------------------------------------------ INFO_AUDIO */

int32_t audio_info(uint32_t ptr, uint32_t n)
{
    (void)n;
    info_audio_t *v = (info_audio_t *)ptr;
    if (!usuario_ok(v, sizeof *v, true)) return ERRO_ENDERECO;
    memset(v, 0, sizeof *v);
    v->estado = (unsigned char)estado;
    v->irq = 0xFF;
    v->volume = volume_mestre;
    v->silencio = calado;
    if (estado == AUDIO_SEM_DRIVER) {
        v->fabricante = sem_driver.fabricante;
        v->modelo = sem_driver.modelo;
        return 0;
    }
    if (estado != AUDIO_FUNCIONANDO) return 0;
    txt_copia_n(v->driver, placa.driver, sizeof v->driver);
    txt_copia_n(v->placa, placa.placa, sizeof v->placa);
    txt_copia_n(v->codec, placa.codec, sizeof v->codec);
    v->fabricante = placa.pci.fabricante;
    v->modelo = placa.pci.modelo;
    if (com_irq) v->irq = (unsigned char)placa.irq;
    v->taxa = placa.taxa;
    v->tocando = tocando;
    v->latencia_ms = latencia * 1000 / placa.taxa;
    bool i0 = int_salva_desliga();
    v->quadros = total_tocado;
    v->interrupcoes = irqs;
    v->falhas = falhas;
    for (int k = 0; k < AUDIO_FLUXOS; k++) {
        fluxo_t *f = &fluxos[k];
        if (!f->usado) continue;
        info_fluxo_audio_t *s = &v->fluxo[v->nfluxos++];
        s->pid = f->pid;
        s->taxa = f->taxa;
        s->canais = f->canais;
        s->bits = f->bits;
        s->volume = f->volume;
        s->pendente_ms = pendente_ms(f);
    }
    v->eq_etapas = (unsigned char)eq.etapas;
    for (int j = 0; j < EQ_BANDAS; j++) v->eq_db[j] = eq.db[j];
    int_restaura(i0);
    return 0;
}

/* ------------------------------------------------------------ bandeja (D114) */

/* Para la bandeja de la barra: estado y volumen maestro sin pasar por la syscall. */
int audio_mestre(int *vol, int *mudo)
{
    *vol = volume_mestre;
    *mudo = calado;
    return estado;
}

/* vol < 0 o mudo < 0: no cambia ese valor. Subir el volumen quita el silencio, como el comando volumen. */
void audio_mestre_definir(int vol, int mudo)
{
    if (estado != AUDIO_FUNCIONANDO) return;
    if (vol >= 0) volume_mestre = (uint8_t)(vol > 100 ? 100 : vol);
    if (mudo >= 0) calado = mudo != 0;
    klog("[AUDIO] bandeja: volume %u%s\n", volume_mestre, calado ? " (silencio)" : "");
}
