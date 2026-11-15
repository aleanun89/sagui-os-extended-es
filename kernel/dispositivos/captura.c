// SPDX-License-Identifier: GPL-2.0-or-later
/* captura.c - entrada de sonido (micrófono) del kernel (D292).
 *
 * La tarjeta (HDA o AC'97, ver hda.c y ac97.c) graba sin parar en un anillo de cuadros PCM de
 * 16 bits estéreo a 48 kHz. En cada tic del reloj (10 ms), mientras haya una captura abierta,
 * este módulo lee la posición de la tarjeta y copia lo nuevo a una cola propia del kernel
 * (CAP_COLA cuadros, 340 ms): de ahí lee el programa, con la tasa y los canales que pidió.
 * Así un programa lento no hace perder cuadros a la tarjeta y el medidor de nivel sigue
 * vivo aunque nadie lea.
 *
 * Conversión de tasa: interpolación lineal con paso en punto fijo 16.16 (como el mezclador de
 * salida; el kernel no usa la FPU). A 48 kHz el sonido sale igual que entró, bit a bit.
 * Mono: media de los dos canales.
 *
 * Hay una sola captura abierta, de un proceso (si muere, el tic la cierra). Los servicios
 * (de_servico) no pueden grabar, como no pueden usar la cámara (D277).
 *
 * Fuente de prueba (CAP_FUENTE_PRUEBA): un tono de 1 kHz generado aquí con el reloj, que pasa por
 * la misma cola y la misma conversión. Sirve para probar sin micrófono (QEMU no inyecta audio
 * de entrada: con -audiodev none la tarjeta emulada graba silencio). */
#include "audio.h"
#include "cpu.h"
#include "console.h"
#include "memoria.h"
#include "processo.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"

#define CAP_COLA 16384                 /* potencia de 2 */
#define TAXA_MIN 8000
#define TAXA_MAX 48000

static captura_hw_t hw;
static char hw_nome[48];
static bool hw_ok;
static int entrada_atual;
static unsigned ganho_pct = 70;
static bool mudo;

static struct {
    bool      aberta, prova;
    int       pid;
    uint32_t  taxa, canais;
    int16_t  *cola;                    /* CAP_COLA cuadros estéreo */
    uint32_t  ini, fim;                /* cuentas absolutas de cuadros nativos */
    uint32_t  fase;                    /* 16.16 entre cola[ini] y cola[ini + 1] */
    uint32_t  pos_hw;                  /* posición del anillo de la tarjeta ya copiada */
    uint32_t  total, perdidos;
    uint32_t  pico, rms;
    uint32_t  ult_decay;               /* último tic en que bajó el pico */
    uint32_t  ult_tique;               /* fuente de prueba */
    uint32_t  resto_prova, fase_tono;
} c;

static const int16_t tono[48] = {
    0, 1044, 2071, 3061, 4000, 4870, 5657, 6347, 6928, 7391, 7727, 7932, 8000, 7932, 7727, 7391, 6928, 6347,
    5657, 4870, 4000, 3061, 2071, 1044, 0, -1044, -2071, -3061, -4000, -4870, -5657, -6347, -6928, -7391,
    -7727, -7932, -8000, -7932, -7727, -7391, -6928, -6347, -5657, -4870, -4000, -3061, -2071, -1044 };

static uint32_t raiz(uint32_t x)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x) b >>= 2;
    while (b) {
        if (x >= r + b) { x -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return r;
}

void captura_registrar(const captura_hw_t *h, const char *nombre)
{
    if (!h->ligar || !h->posicao || h->n_entradas <= 0 || hw_ok) return;
    hw = *h;
    txt_copia_n(hw_nome, nombre, sizeof hw_nome);
    hw_ok = true;
    entrada_atual = 0;
    if (hw.entrada) hw.entrada(0);
    if (hw.ganho) hw.ganho(ganho_pct);
    klog("[CAPTURA] %s: %d entrada(s); primera: %s\n", hw_nome, hw.n_entradas, hw.nombre[0]);
}

int captura_hay(void) { return hw_ok; }

/* Mete cuadros en la cola (aplica el silencio y mide el nivel). Interrupciones desactivadas. */
static void empujar(const int16_t *lr, uint32_t n)
{
    if (!n) return;
    uint32_t pico = 0, soma = 0;
    for (uint32_t i = 0; i < n; i++) {
        int32_t l = mudo ? 0 : lr[2 * i], r = mudo ? 0 : lr[2 * i + 1];
        uint32_t al = (uint32_t)(l < 0 ? -l : l), ar = (uint32_t)(r < 0 ? -r : r);
        if (al > pico) pico = al;
        if (ar > pico) pico = ar;
        uint32_t m = al > ar ? al : ar;
        soma += (m * m) >> 10;
        int16_t *d = &c.cola[(c.fim & (CAP_COLA - 1)) * 2];
        d[0] = (int16_t)l;
        d[1] = (int16_t)r;
        c.fim++;
        if ((uint32_t)(c.fim - c.ini) > CAP_COLA) { c.ini = c.fim - CAP_COLA; c.perdidos++; }
    }
    if (pico > 32767) pico = 32767;
    c.total += n;
    /* El pico sube al momento y baja despacio (atualizar, una vez por tic); el RMS es la media del trozo, suavizada. */
    if (pico > c.pico) c.pico = pico;
    uint32_t rms = raiz((soma / n) << 10);
    c.rms = (c.rms * 3 + rms) / 4;
}

static void vaciar_hw(void)
{
    uint32_t q = hw.quadros, p = hw.posicao() % q;
    uint32_t d = (p + q - c.pos_hw) % q;
    int16_t trozo[256 * 2];
    while (d) {
        uint32_t n = d < 256 ? d : 256;
        for (uint32_t i = 0; i < n; i++) {
            const volatile int16_t *s = &hw.anel[((c.pos_hw + i) % q) * 2];
            trozo[2 * i] = s[0];
            trozo[2 * i + 1] = s[1];
        }
        empujar(trozo, n);
        c.pos_hw = (c.pos_hw + n) % q;
        d -= n;
    }
}

static void gerar_prova(void)
{
    uint32_t agora = relogio_tiques();
    uint32_t t = agora - c.ult_tique;
    c.ult_tique = agora;
    uint32_t falta = t * (TAXA_MAX / TIQUES_POR_SEGUNDO);
    int16_t trozo[256 * 2];
    while (falta) {
        uint32_t n = falta < 256 ? falta : 256;
        for (uint32_t i = 0; i < n; i++) {
            trozo[2 * i] = trozo[2 * i + 1] = tono[c.fase_tono];
            c.fase_tono = (c.fase_tono + 1) % 48;
        }
        empujar(trozo, n);
        falta -= n;
    }
}

static void atualizar(void)
{
    if (!c.aberta) return;
    uint32_t agora = relogio_tiques();
    if (agora != c.ult_decay) { c.ult_decay = agora; c.pico -= c.pico / 8; }   /* ~100 ms para bajar a la mitad */
    if (c.prova) gerar_prova();
    else vaciar_hw();
}

static void fechar(void)
{
    if (!c.aberta) return;
    bool i0 = int_salva_desliga();
    c.aberta = false;
    if (!c.prova && hw_ok) hw.parar();
    kfree(c.cola);
    c.cola = NULL;
    int_restaura(i0);
    klog("[CAPTURA] fechada; %u cuadros, %u perdidos\n", c.total, c.perdidos);
}

void captura_tique(void)
{
    if (!c.aberta) return;
    processo_t *p = processo_por_pid(c.pid);
    if (!p || p->estado == P_LIVRE || p->estado == P_ZUMBI) { fechar(); return; }
    atualizar();
}

/* Copia a "dst" cuadros convertidos a (taxa, canais); devuelve cuántos cuadros salieron. */
static uint32_t ler(int16_t *dst, uint32_t maximo)
{
    uint32_t passo = (TAXA_MAX << 16) / c.taxa, pos = c.fase, i;
    uint32_t qtd = c.fim - c.ini;
    for (i = 0; i < maximo; i++) {
        uint32_t k = pos >> 16, fr = (pos & 0xFFFF) >> 1;
        if (k >= qtd || (fr && k + 1 >= qtd)) break;
        const int16_t *a = &c.cola[((c.ini + k) & (CAP_COLA - 1)) * 2];
        int32_t l = a[0], r = a[1];
        if (fr) {
            const int16_t *b = &c.cola[((c.ini + k + 1) & (CAP_COLA - 1)) * 2];
            l += ((b[0] - l) * (int32_t)fr) >> 15;
            r += ((b[1] - r) * (int32_t)fr) >> 15;
        }
        if (c.canais == 1) dst[i] = (int16_t)((l + r) / 2);
        else { dst[2 * i] = (int16_t)l; dst[2 * i + 1] = (int16_t)r; }
        pos += passo;
    }
    uint32_t usados = pos >> 16;
    if (usados > qtd) usados = qtd;
    c.ini += usados;
    c.fase = pos & 0xFFFF;
    return i;
}

static bool dono_vivo(void)
{
    processo_t *p = processo_por_pid(c.pid);
    return p && p->estado != P_LIVRE && p->estado != P_ZUMBI;
}

int32_t captura_info(uint32_t ptr, uint32_t n)
{
    info_captura_t *s = (info_captura_t *)ptr;
    if (n < sizeof *s || !usuario_ok(s, sizeof *s, true)) return ERRO_ENDERECO;
    processo_t *eu = processo_recursos();
    if (eu->de_servico) return s->resultado = ERRO_ACESSO;
    int pid = eu->pid;
    int32_t r = ERRO_INVALIDO;
    bool prova = s->fuente == CAP_FUENTE_PRUEBA;
    switch (s->orden) {
    case CAP_ESTADO: {
        s->presente = prova ? 1 : hw_ok;
        s->abierta = c.aberta && c.prova == prova;
        s->taxa = TAXA_MAX;
        txt_copia_n(s->nome, prova ? "Fuente de prueba (tono 1 kHz)" : hw_ok ? hw_nome : "", sizeof s->nome);
        if (prova) {
            s->n_entradas = 1;
            txt_copia_n(s->entradas[0], "Tono de prueba", sizeof s->entradas[0]);
        } else if (hw_ok) {
            s->n_entradas = (unsigned)hw.n_entradas;
            for (int i = 0; i < hw.n_entradas && i < CAP_ENTRADAS_MAX; i++)
                txt_copia_n(s->entradas[i], hw.nombre[i], sizeof s->entradas[i]);
            s->entrada = (unsigned)entrada_atual;
        }
        s->ganancia = ganho_pct;
        s->mudo = mudo;
        if (c.aberta && c.prova == prova) {
            bool i0 = int_salva_desliga();
            atualizar();                   /* medidor al día aunque aún no haya pasado un tic */
            s->pico = c.pico; s->rms = c.rms; s->cuadros = c.total; s->perdidos = c.perdidos;
            int_restaura(i0);
        }
        r = s->presente ? 0 : ERRO_NAO_EXISTE;
        break;
    }
    case CAP_ABRIR:
        if (!prova && !hw_ok) { r = ERRO_NAO_EXISTE; break; }
        if (s->taxa < TAXA_MIN || s->taxa > TAXA_MAX || (s->canais != 1 && s->canais != 2)) { r = ERRO_INVALIDO; break; }
        if (c.aberta) {
            if (c.pid != pid && dono_vivo()) { r = ERRO_OCUPADO; break; }
            fechar();                      /* la misma aplicación cambia de formato, o el dueño murió */
        }
        {
            int16_t *cola = kmalloc(CAP_COLA * 4);
            if (!cola) { r = ERRO_SEM_MEMORIA; break; }
            bool i0 = int_salva_desliga();
            memset(&c, 0, sizeof c);
            c.cola = cola;
            c.pid = pid;
            c.taxa = s->taxa;
            c.canais = s->canais;
            c.prova = prova;
            if (prova) c.ult_tique = relogio_tiques();
            else if (!hw.ligar()) {
                kfree(cola);
                c.cola = NULL;
                int_restaura(i0);
                r = ERRO_NAO_EXISTE;
                break;
            }
            c.aberta = true;
            int_restaura(i0);
            klog("[CAPTURA] abierta por el proceso %d: %u Hz, %u canal(es)%s\n", pid, c.taxa, c.canais, prova ? " (prueba)" : "");
            r = 0;
        }
        break;
    case CAP_LER: {
        if (!c.aberta || c.pid != pid) { r = ERRO_NAO_EXISTE; break; }
        if (!s->buf || !usuario_ok(s->buf, s->buf_tam, true)) { r = ERRO_ENDERECO; break; }
        uint32_t tq = c.canais * 2u, maximo = s->buf_tam / tq;
        bool i0 = int_salva_desliga();
        atualizar();
        uint32_t feitos = ler((int16_t *)s->buf, maximo);
        int_restaura(i0);
        r = (int32_t)(feitos * tq);
        break;
    }
    case CAP_FECHAR:
        if (c.aberta && (c.pid == pid || !dono_vivo())) fechar();
        r = 0;
        break;
    case CAP_ENTRADA:
        if (!hw_ok) { r = ERRO_NAO_EXISTE; break; }
        if (s->escribir) {
            if ((int)s->entrada >= hw.n_entradas) { r = ERRO_INVALIDO; break; }
            if (hw.entrada && !hw.entrada((int)s->entrada)) { r = ERRO_NAO_SUPORTADO; break; }
            entrada_atual = (int)s->entrada;
            if (hw.ganho) hw.ganho(ganho_pct);
            klog("[CAPTURA] entrada: %s\n", hw.nombre[entrada_atual]);
        }
        s->entrada = (unsigned)entrada_atual;
        r = 0;
        break;
    case CAP_GANANCIA:
        if (s->escribir) {
            ganho_pct = s->ganancia > 100 ? 100 : s->ganancia;
            mudo = s->mudo != 0;
            if (hw_ok && hw.ganho) hw.ganho(ganho_pct);
        }
        s->ganancia = ganho_pct;
        s->mudo = mudo;
        r = 0;
        break;
    }
    return s->resultado = r;
}
