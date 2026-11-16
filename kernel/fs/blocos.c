// SPDX-License-Identifier: GPL-2.0-or-later
/* blocos.c - acceso en bruto a los sectores de un disco (D179).
 *
 * Lo usan la gestión de particiones (programa "particiones" y la ventana de
 * "discos"), el formateo y el instalador, todos en el espacio de usuario: el
 * kernel solo da sectores, pero pone las reglas que impiden perder datos por
 * error, porque un fallo del programa no debe poder saltárselas:
 *   - leer vale siempre (no cambia nada);
 *   - escribir, descartar (TRIM) y volver a leer la tabla exigen que el proceso
 *     haya "reservado" el disco entero; la reserva solo se da si no hay NADA
 *     montado de ese disco, y mientras dura no se puede montar nada de él;
 *   - el disco del que arrancó la máquina (o uno que podría serlo) pide la
 *     confirmación doble (DBLOCO_CONF_ARRANQUE);
 *   - en una partición no se escribe fuera de sus límites (disco_escrever);
 *   - un disco GPT no se toca (ni la cabecera ni las entradas, al principio y al
 *     final) salvo que el instalador vaya a borrarlo entero (DBLOCO_CONF_GPT);
 *   - un sector 0 con la firma 0x55AA tiene que traer una tabla coherente: cada
 *     partición dentro del disco y sin solapes entre las primarias. Un sector 0
 *     sin firma (el instalador lo deja así mientras copia) se acepta.
 *   - D213: con DBLOCO_CONF_MONTADO se reserva aunque haya particiones montadas (el
 *     disco de arranque, con /home y /apps). Entonces lo montado y la partición de
 *     SaguiOS del disco de arranque son intocables: nada se escribe ni se descarta
 *     dentro de ellas, y cada escritura del disco entero (MBR, EBR) se simula antes y
 *     solo se hace si la tabla que queda las sigue describiendo igual.
 * La reserva caduca sola si el proceso termina sin soltarla. */
#include "disco.h"
#include "abi.h"
#include "console.h"
#include "memoria.h"
#include "montagem.h"
#include "processo.h"
#include "sistema.h"
#include "texto.h"
#include "trava.h"
#include "cpu.h"
#include "cpu_dados.h"
#include "info_hardware.h"
#include "prazo.h"
#include "relogio.h"

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static int meu_pid(void) { return processo_recursos()->pid; }

/* ¿El que llama tiene reservado el disco (entero) de este volumen? */
static bool reserva_minha(disco_t *d)
{
    disco_t *r = disco_raiz(d);
    return disco_reservado(r) && r->reservado_pid == meu_pid();
}

/* La tabla de un sector 0 que se va a escribir: si lleva la firma, cada entrada
 * usada tiene que caber en el disco y las primarias no pueden solaparse. */
static bool mbr_coerente(disco_t *d, const uint8_t *s)
{
    const uint64_t esc = d->bloco_fisico == 4096 ? 8 : 1;   /* D283: la tabla cuenta en bloques reales */
    if (s[510] != 0x55 || s[511] != 0xAA) return true;   /* sin firma: no es una tabla */
    uint32_t ini[4], fim[4];
    for (int i = 0; i < 4; i++) {
        const uint8_t *e = s + 446 + i * 16;
        ini[i] = fim[i] = 0;
        if (!e[4]) continue;
        if (e[4] == 0xEE) return (d->reservado_flags & DBLOCO_CONF_GPT) != 0;   /* no se crean GPT */
        if (e[0] != 0 && e[0] != 0x80) return false;
        uint64_t a = (uint64_t)le32(e + 8) * esc, n = (uint64_t)le32(e + 12) * esc;
        if (!a || !n || a >= d->setores || n > d->setores - a) return false;
        ini[i] = (uint32_t)a; fim[i] = (uint32_t)(a + n);
    }
    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 4; j++)
            if (fim[i] && fim[j] && ini[i] < fim[j] && ini[j] < fim[i]) return false;
    return true;
}

/* Zonas de la GPT: los 34 primeros sectores y los 33 últimos (en 4096: 6 y 5 bloques de 4096). */
static bool toca_gpt(disco_t *r, uint32_t lba, uint32_t n)
{
    if (!r->gpt || (r->reservado_flags & DBLOCO_CONF_GPT)) return false;
    uint32_t esc = r->bloco_fisico == 4096 ? 8 : 1;   /* D283: 34 y 33 bloques reales (en 4096: la cabecera y 4 bloques de entradas) */
    uint32_t cab = esc == 8 ? 6 * 8 : 34, cola = esc == 8 ? 5 * 8 : 33;
    return lba < cab || lba + n > r->setores - cola;
}

/* D338: cifras de una reserva (lo que hace un formateo, un cambio de tamaño o el instalador),
 * anotadas al soltarla: cuánto duró, cuánto se leyó y grabó y en cuánto tiempo dentro del
 * driver, las interrupciones de cada línea y el uso de la CPU 0. Sirven para ver dónde se va
 * el tiempo en una máquina real (y en las pruebas) sin herramientas aparte. */
static struct {
    disco_t *d;
    uint32_t tic0, ocioso0, irq0[16];
    uint64_t tsc0, tsc_ler, tsc_esc;
    uint32_t ops_ler, ops_esc, set_ler, set_esc;
} med;

static void medida_iniciar(disco_t *d)
{
    memset(&med, 0, sizeof med);
    med.d = d;
    med.tic0 = relogio_tiques();
    med.ocioso0 = cpu_dados(0)->tiques_ociosos;
    for (int i = 0; i < 16; i++) med.irq0[i] = irq_contagem((uint8_t)i);
    med.tsc0 = prazo_tsc();
}

static uint32_t tsc_ms(uint64_t t)
{
    uint32_t khz = hw_tsc_khz();
    return khz ? (uint32_t)(t / khz) : 0;
}

static void medida_anotar(disco_t *d)
{
    if (med.d != d) return;
    uint32_t tics = relogio_tiques() - med.tic0, ocio = cpu_dados(0)->tiques_ociosos - med.ocioso0;
    uint32_t ms = tsc_ms(prazo_tsc() - med.tsc0), ml = tsc_ms(med.tsc_ler), me = tsc_ms(med.tsc_esc);
    char irqs[160];
    int k = 0;
    irqs[0] = 0;
    for (int i = 0; i < 16 && k < (int)sizeof irqs - 24; i++) {
        uint32_t n = irq_contagem((uint8_t)i) - med.irq0[i];
        if (n) k += snformatar(irqs + k, sizeof irqs - (size_t)k, " %d:%u", i, n);
    }
    klog("[BLOCOS] %s: reserva de %u ms; leidos %u sectores en %u ordenes (%u ms en el driver, %u KB/s), "
         "grabados %u sectores en %u ordenes (%u ms, %u KB/s); CPU0 ociosa %u de %u tics; IRQ%s\n",
         d->nome, ms, med.set_ler, med.ops_ler, ml, ml ? (uint32_t)((uint64_t)med.set_ler * 500 / ml) : 0,
         med.set_esc, med.ops_esc, me, me ? (uint32_t)((uint64_t)med.set_esc * 500 / me) : 0, ocio, tics, k ? irqs : " -");
    med.d = NULL;
}

static int reservar(disco_t *d, uint32_t flags)
{
    if (d->pai) return ERRO_INVALIDO;                 /* se reserva el disco entero */
    if (disco_reservado(d)) return d->reservado_pid == meu_pid() ? 0 : ERRO_OCUPADO;
    /* Desmontar primero; o, con DBLOCO_CONF_MONTADO (D213), lo montado queda protegido
     * (nunca en un disco GPT, que aquí no se cambia). */
    if (disco_montado(d) && (!(flags & DBLOCO_CONF_MONTADO) || d->gpt)) return ERRO_OCUPADO;
    if (disco_pode_ser_arranque(d) && !(flags & DBLOCO_CONF_ARRANQUE)) return ERRO_DISCO_ARRANQUE;
    if (!d->escrever) return ERRO_SO_LEITURA;
    d->reservado_pid = meu_pid();
    d->reservado_flags = flags;
    disco_tabela_esquecer(d, 0, 0);   /* D338 */
    medida_iniciar(d);
    klog("[BLOCOS] %s reservado por el pid %d (flags %x)%s%s\n", d->nome, d->reservado_pid, flags,
         d->arranque_real ? ", ES EL DISCO DE ARRANQUE" : "",
         (flags & DBLOCO_CONF_MONTADO) && disco_montado(d) ? ", con particiones montadas (protegidas)" : "");
    return 0;
}

static int escrever(disco_t *d, uint32_t lba, uint32_t n, const uint8_t *k)
{
    disco_t *r = disco_raiz(d);
    uint32_t abs = d->pai ? d->inicio + lba : lba;
    bool comprobada = false;   /* D338: la cadena de la tabla que recuerda la caché ya es la de después */
    if (lba >= d->setores || n > d->setores - lba) return ERRO_INVALIDO;
    if (toca_gpt(r, abs, n)) { klog("[BLOCOS] %s: escritura en la GPT rechazada\n", r->nome); return ERRO_ACESSO; }
    if (abs == 0 && !mbr_coerente(r, k)) {
        klog("[BLOCOS] %s: tabla de particiones incoherente, no se escribe\n", r->nome);
        return ERRO_INVALIDO;
    }
    if (r->reservado_flags & DBLOCO_CONF_MONTADO) {   /* D213 */
        disco_t *p = disco_toca_protegida(r, abs, n);
        if (p) {
            klog("[BLOCOS] %s: escritura en %s (%s) rechazada\n", d->nome, p->nome, p->montagem ? "montada" : "SaguiOS");
            return ERRO_OCUPADO;
        }
        /* Una escritura del disco entero (o de la extendida) puede ser la tabla: se simula. */
        if (!d->pai || d->tipo_mbr == 0x05 || d->tipo_mbr == 0x0F || d->tipo_mbr == 0x85) {
            if (!disco_tabela_conserva(r, abs, n, k)) return ERRO_OCUPADO;
            comprobada = true;
        }
    }
    /* El cerrojo de D040 (disco interno sin montar = solo lectura) se abre solo
     * para esta escritura del dueño de la reserva. */
    bool antes = r->pai_gravando;
    r->pai_gravando = true;
    int x = disco_escrever(r, abs, n, k);
    r->pai_gravando = antes;
    /* D338: la cadena de la tabla comprobada sigue valiendo si esta escritura no tocó ninguno de sus
     * sectores; si falló, no se sabe qué quedó en el disco. */
    if (x < 0) disco_tabela_esquecer(r, 0, 0);
    else if (!comprobada) disco_tabela_esquecer(r, abs, n);
    return x;
}

static int descartar(disco_t *d, uint32_t lba, uint32_t n)
{
    disco_t *r = disco_raiz(d);
    uint32_t abs = d->pai ? d->inicio + lba : lba;
    if (lba >= d->setores || n > d->setores - lba) return ERRO_INVALIDO;
    if (toca_gpt(r, abs, n) || abs == 0) return ERRO_ACESSO;   /* la tabla nunca se descarta */
    if ((r->reservado_flags & DBLOCO_CONF_MONTADO) && disco_toca_protegida(r, abs, n)) return ERRO_OCUPADO;   /* D213 */
    if (!r->descartar || r->trim != TRIM_SIM) return ERRO_NAO_SUPORTADO;
    int x = r->descartar(r, abs, n);
    disco_tabela_esquecer(r, abs, n);   /* D338 */
    klog("[BLOCOS] %s: TRIM de %u sectores desde %u: %d\n", d->nome, n, abs, x);
    return x < 0 ? ERRO_ACESSO : 0;
}

int32_t sis_disco_blocos(uint32_t op, uint32_t pnome, uint32_t a, uint32_t b, uint32_t c)
{
    char nome[40];
    int r = usuario_texto((const char *)pnome, nome, sizeof nome);
    if (r < 0) return r;
    disco_t *d = disco_por_nome(nome);
    if (!d) return ERRO_NAO_EXISTE;

    switch (op) {
    case DBLOCO_INFO: {
        info_bloco_t *inf = (info_bloco_t *)a;
        if (!usuario_ok(inf, sizeof *inf, true)) return ERRO_ENDERECO;
        disco_t *raiz = disco_raiz(d);
        memset(inf, 0, sizeof *inf);
        inf->setores = d->setores;
        inf->inicio = d->pai ? d->inicio : 0;
        inf->trim = raiz->trim;
        inf->arranque_real = raiz->arranque_real;
        inf->reservado_pid = disco_reservado(raiz) ? (unsigned)raiz->reservado_pid : 0;
        inf->gpt = raiz->gpt;
        inf->tipo_mbr = d->pai ? d->tipo_mbr : 0;
        inf->cab_lba = raiz->arranque_real ? raiz->cab_lba : 0;   /* D270 */
        inf->bloco_fisico = raiz->bloco_fisico == 4096 ? 4096 : 512;   /* D283 */
        return 0;
    }
    case DBLOCO_RESERVAR:
        return reservar(d, a);
    case DBLOCO_EXPULSAR: {   /* D233: CD/DVD (o su imagen El Torito): desmontar y abrir la bandeja */
        disco_t *raiz = disco_raiz(d);
        if (!raiz->expulsar) return ERRO_NAO_SUPORTADO;
        trava_pegar(&trava_fs);
        int x = raiz->expulsar(raiz, a != 0);
        trava_soltar(&trava_fs);
        return x;
    }
    case DBLOCO_LIBERAR:
    case DBLOCO_RELER:
        if (d->pai || !reserva_minha(d)) return ERRO_ACESSO;
        trava_pegar(&trava_fs);
        disco_reler(d);
        trava_soltar(&trava_fs);
        if (op == DBLOCO_LIBERAR) {
            d->reservado_pid = 0;
            d->reservado_flags = 0;
            medida_anotar(d);
            disco_tabela_esquecer(d, 0, 0);
            klog("[BLOCOS] %s liberado\n", d->nome);
        }
        return 0;
    case DBLOCO_LER:
    case DBLOCO_ESCREVER: {
        uint32_t lba = a, n = b;
        void *u = (void *)c;
        if (!n || n > DBLOCO_MAX_SETORES) return ERRO_INVALIDO;
        if (!usuario_ok(u, n * 512, op == DBLOCO_LER)) return ERRO_ENDERECO;
        if (op == DBLOCO_ESCREVER && !reserva_minha(d)) return ERRO_ACESSO;
        /* Los drivers con DMA no ven la memoria del programa: se pasa por un búfer del kernel. D338:
         * hasta 128 KB de una vez (una sola orden al driver); sin memoria contigua para tanto, en
         * trozos de 64 KB como antes. */
        uint32_t passo = n;
        uint8_t *k = kmalloc(n * 512);
        if (!k && n > 128) { passo = 128; k = kmalloc(passo * 512); }
        if (!k) return ERRO_SEM_MEMORIA;
        int x = 0;
        for (uint32_t f = 0; f < n && x == 0; f += passo) {
            uint32_t q = n - f < passo ? n - f : passo;
            uint8_t *uu = (uint8_t *)u + f * 512;
            if (op == DBLOCO_ESCREVER) memcpy(k, uu, q * 512);
            trava_pegar(&trava_fs);
            uint64_t t0 = prazo_tsc();
            x = op == DBLOCO_LER ? disco_ler(d, lba + f, q, k) : escrever(d, lba + f, q, k);
            if (med.d && med.d == disco_raiz(d)) {   /* D338: las cifras de la reserva */
                if (op == DBLOCO_LER) { med.tsc_ler += prazo_tsc() - t0; med.ops_ler++; med.set_ler += q; }
                else { med.tsc_esc += prazo_tsc() - t0; med.ops_esc++; med.set_esc += q; }
            }
            trava_soltar(&trava_fs);
            if (x == 0 && op == DBLOCO_LER) memcpy(uu, k, q * 512);
        }
        kfree(k);
        return x;
    }
    case DBLOCO_DESCARTAR: {
        if (!reserva_minha(d)) return ERRO_ACESSO;
        trava_pegar(&trava_fs);
        int x = descartar(d, a, b);
        trava_soltar(&trava_fs);
        return x;
    }
    }
    return ERRO_INVALIDO;
}
