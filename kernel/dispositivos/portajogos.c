// SPDX-License-Identifier: GPL-2.0-or-later
/* portajogos.c - el joystick analógico del puerto de juegos (0x201) (D307).
 *
 * El puerto de juegos de IBM (y el de las tarjetas de sonido, que es el mismo) no tiene
 * IRQ ni registros: al escribir cualquier cosa en 0x201, los cuatro monoestables suben los
 * bits 0-3 y cada uno baja cuando se carga su condensador a través del potenciómetro del eje
 * (portajogos_conv.h: de ~24 us a ~1,1 ms). Los bits 4-7 son los botones, a 0 al pulsarlos.
 *
 * Medida: con las interrupciones apagadas (si no, una IRQ por medio alarga el tiempo), se
 * escribe en el puerto y se lee en bucle mirando el TSC (medido al arrancar, hw_tsc_khz) hasta
 * que bajan todos los ejes presentes o pasa el plazo (PJ_PRAZO_US, 3 ms). Además del TSC hay un
 * tope de vueltas: si el TSC no avanzara, el bucle termina igual. Solo se mide cuando un
 * programa pregunta por el estado, como mucho cada 20 ms: con un joystick de 100 kohmios son
 * unos 1,1 ms sin interrupciones cada 20 ms en el peor caso.
 *
 * Detección (al arrancar y con MANDOS_DETECTAR): una medida con los cuatro ejes. Sin puerto
 * el bus devuelve 0xFF y nada baja; sin joystick, los bits de los ejes se quedan arriba (o
 * a 0 sin subir): en los dos casos no hay ningún eje que baje después de subir, y no hay
 * mando. Un eje que bajó cuenta como presente; después solo se esperan los presentes.
 *
 * Calibración: sin una puesta a mano (Configuración > Mandos, guardada en sagui.conf y
 * aplicada con «configuracion --aplicar»), la primera medida es el centro y los extremos se
 * amplían solos al mover el mando. */
#include "mandos.h"
#include "console.h"
#include "cpu.h"
#include "info_hardware.h"
#include "portajogos_conv.h"
#include "portas.h"
#include "prazo.h"
#include "relogio.h"
#include "texto.h"

#define PORTA_JOGOS   0x201
#define VOLTAS_MAX    20000u    /* tope de lecturas del puerto (~20 ms en un ISA real) */
#define CADA_TIQUES   2         /* 20 ms entre medidas */
#define FALHAS_RETIRAR 100      /* medidas seguidas sin ningún eje: se dio por desconectado */

static int      manejador = -1;
static uint8_t  presentes;       /* bits 0-3: ejes que responden */
static pj_cal_t cal[PJ_EIXOS];
static bool     manual;          /* calibración puesta a mano */
static bool     cal_iniciada[PJ_EIXOS];
static uint32_t ultima_medida;
static uint32_t medidas, falhas_seguidas;
static estado_mando_t ultimo;
static uint16_t ultimo_crudo[PJ_EIXOS];
static uint8_t  primeira_leitura;   /* diagnóstico: el puerto justo después de dispararlo */

/* Una medida: tiempos de los ejes de la máscara (décimas de us; PJ_SEM_EIXO si no bajó o no
 * llegó a subir) y los botones (bit a 1 = pulsado). false = sin TSC medido. */
static bool medir(uint8_t mascara, uint16_t t[PJ_EIXOS], uint8_t *botoes)
{
    uint32_t khz = hw_tsc_khz();
    for (int i = 0; i < PJ_EIXOS; i++) t[i] = PJ_SEM_EIXO;
    *botoes = 0;
    if (!khz) return false;
    uint32_t prazo = PJ_PRAZO_US * (khz / 1000u);   /* ciclos */
    uint32_t ciclos[PJ_EIXOS] = { 0, 0, 0, 0 };
    uint8_t pendentes = mascara, v = 0xFF;
    bool i0 = int_salva_desliga();
    porta_escreve8(PORTA_JOGOS, 0xFF);
    uint64_t t0 = prazo_tsc();
    for (uint32_t voltas = 0; pendentes && voltas < VOLTAS_MAX; voltas++) {
        v = porta_le8(PORTA_JOGOS);
        if (!voltas) primeira_leitura = v;
        uint32_t dt = (uint32_t)(prazo_tsc() - t0);
        for (int i = 0; i < PJ_EIXOS; i++)
            if ((pendentes & (1u << i)) && !(v & (1u << i))) { ciclos[i] = dt ? dt : 1; pendentes &= (uint8_t)~(1u << i); }
        if (dt > prazo) break;
    }
    int_restaura(i0);
    *botoes = (uint8_t)((~v >> 4) & 0x0F);
    for (int i = 0; i < PJ_EIXOS; i++) {
        if (!(mascara & (1u << i)) || !ciclos[i]) continue;
        uint32_t d = pj_decimas_us(ciclos[i], khz);
        /* Por debajo de 2 us el bit no llegó a subir (el mínimo real es ~24 us). */
        if (d >= 20 && d < PJ_SEM_EIXO) t[i] = (uint16_t)d;
    }
    return true;
}

int pj_manejador(void) { return manejador; }

bool pj_detectar(void)
{
    uint16_t t[PJ_EIXOS];
    uint8_t b;
    uint8_t antes = porta_le8(PORTA_JOGOS);
    if (!medir(0x0F, t, &b)) {
        klog("[MANDOS] puerto de juegos: sin TSC medido, no se mira\n");
        return false;
    }
    /* Segunda medida: los monoestables recién disparados pueden dar una primera lectura rara. */
    medir(0x0F, t, &b);
    uint8_t p = 0;
    for (int i = 0; i < PJ_EIXOS; i++) if (t[i] != PJ_SEM_EIXO) p |= (uint8_t)(1u << i);
    if (!(p & 0x03)) p = 0;        /* sin X ni Y del primer joystick no se toma como joystick */
    if (!p) {
        klog("[MANDOS] puerto de juegos: no hay joystick (0x201 = %02x; al dispararlo, %02x)\n", antes, primeira_leitura);
        if (manejador >= 0) { mando_retirar(manejador); manejador = -1; }
        presentes = 0;
        return false;
    }
    int n_eixos = (p & 0x0C) ? 4 : 2;
    if (manejador >= 0 && p == presentes) return true;   /* el mismo de antes */
    if (manejador >= 0) mando_retirar(manejador);
    presentes = p;
    for (int i = 0; i < PJ_EIXOS; i++) {
        if (!manual || !pj_cal_valida(&cal[i])) {
            if (t[i] != PJ_SEM_EIXO) pj_cal_inicial(t[i], &cal[i]);
            cal_iniciada[i] = t[i] != PJ_SEM_EIXO;
        }
    }
    char nome[48];
    snformatar(nome, sizeof nome, "Joystick analógico (%d ejes, 4 botones)", n_eixos);
    manejador = mando_registrar(MANDO_TIPO_PUERTO, nome, "puerto 0x201", 0, 0, (uint8_t)n_eixos, 4, 0, MANDO_MAPA_NENHUM);
    klog("[MANDOS] puerto de juegos: ejes %x; tiempos %u %u %u %u (decimas de us); calibracion %s\n", p,
         t[0], t[1], t[2], t[3], manual ? "guardada" : "automatica");
    falhas_seguidas = 0;
    ultima_medida = relogio_tiques() - CADA_TIQUES;
    return true;
}

void pj_ler(estado_mando_t *e, uint16_t crudo[4])
{
    uint32_t agora = relogio_tiques();
    if (manejador >= 0 && agora - ultima_medida >= CADA_TIQUES) {
        uint16_t t[PJ_EIXOS];
        uint8_t b;
        ultima_medida = agora;
        if (medir(presentes, t, &b)) {
            medidas++;
            bool algum = false;
            hid_estado_t h;
            memset(&h, 0, sizeof h);
            for (int i = 0; i < PJ_EIXOS; i++) {
                ultimo_crudo[i] = (presentes & (1u << i)) ? t[i] : PJ_SEM_EIXO;
                if (t[i] == PJ_SEM_EIXO) continue;
                algum = true;
                if (!cal_iniciada[i]) { pj_cal_inicial(t[i], &cal[i]); cal_iniciada[i] = true; }
                if (!manual) pj_cal_ampliar(t[i], &cal[i]);
                h.eixo[i] = pj_normalizar(t[i], &cal[i]);
            }
            h.botoes = b;
            if (algum) falhas_seguidas = 0;
            else if (++falhas_seguidas >= FALHAS_RETIRAR) {
                klog("[MANDOS] puerto de juegos: el joystick no responde, se retira\n");
                mando_retirar(manejador);
                manejador = -1;
                presentes = 0;
            }
            if (manejador >= 0) mando_atualizar(manejador, &h);
            ultimo.informes = medidas;
            for (int i = 0; i < MANDO_EIXOS_MAX; i++) ultimo.eixos[i] = i < PJ_EIXOS ? h.eixo[i] : 0;
            ultimo.botoes = h.botoes;
        }
    }
    *e = ultimo;
    e->id = 0;
    for (int i = 0; i < 4; i++) crudo[i] = ultimo_crudo[i];
}

void pj_calibracao(uint16_t c[4][3], bool *m)
{
    for (int i = 0; i < PJ_EIXOS; i++) { c[i][0] = cal[i].min; c[i][1] = cal[i].centro; c[i][2] = cal[i].max; }
    *m = manual;
}

void pj_calibrar(const uint16_t c[4][3])
{
    if (!c) {
        manual = false;
        for (int i = 0; i < PJ_EIXOS; i++) cal_iniciada[i] = false;   /* la próxima medida es el centro */
        klog("[MANDOS] puerto de juegos: calibracion automatica\n");
        return;
    }
    bool alguma = false;
    for (int i = 0; i < PJ_EIXOS; i++) {
        pj_cal_t n = { c[i][0], c[i][1], c[i][2] };
        if (!pj_cal_valida(&n)) continue;   /* un eje sin datos se queda como estaba */
        cal[i] = n;
        cal_iniciada[i] = true;
        alguma = true;
    }
    if (alguma) manual = true;
    klog("[MANDOS] puerto de juegos: calibracion %s (X %u/%u/%u, Y %u/%u/%u)\n", alguma ? "puesta" : "rechazada",
         cal[0].min, cal[0].centro, cal[0].max, cal[1].min, cal[1].centro, cal[1].max);
}
