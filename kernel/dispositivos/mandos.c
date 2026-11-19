// SPDX-License-Identifier: GPL-2.0-or-later
/* mandos.c - la capa común de mandos y joysticks (D307).
 *
 * Una tabla de MANDOS_MAX mandos. Cada driver (kernel/usb/hid.c para los USB HID y el Xbox
 * 360 con cable, portajogos.c para el puerto de juegos) registra el suyo al conectarlo, le
 * pasa el estado ya normalizado cada vez que llega un informe y lo retira al desconectarlo.
 * Los programas lo leen por sondeo con INFO_MANDOS: SDL_JoystickUpdate, DOSBox y los juegos
 * miran el estado en cada cuadro, así que no hace falta una cola de eventos. La "geracao"
 * cambia con cada conexión o desconexión, para que quien enumere sepa cuándo repetir.
 *
 * Los mandos de prueba (mkimagem.py --mandos-prueba): un Logitech Dual Action, cuyo
 * descriptor pasa por el mismo analizador que el de un USB de verdad, y un Xbox 360 con su
 * decodificador. MANDOS_INYECTAR les entrega informes crudos: así se prueba en el QEMU, que
 * no emula ningún joystick USB, todo menos el transporte. */
#include "mandos.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "processo.h"
#include "sistema.h"
#include "texto.h"

#define MANDO_DESC_MAX 512   /* descriptor de informe que se guarda para «mandos --crudo» */

typedef struct {
    bool         usado;
    info_mando_t info;
    hid_estado_t est;
    uint32_t     informes;
    uint8_t      crudo[64];          /* el último informe tal como llegó (diagnóstico) */
    uint8_t      crudo_n;
    uint16_t     desc_n;
    uint8_t      desc[MANDO_DESC_MAX];
} mando_t;

static mando_t mandos[MANDOS_MAX];
static uint32_t geracao = 1, proximo_id = 1;

/* ------------------------------------------------------------ registro */

int mando_registrar(uint8_t tipo, const char *nome, const char *via, uint16_t fab, uint16_t prod,
                    uint8_t eixos, uint8_t botoes, uint8_t hats, uint8_t mapa)
{
    bool i0 = int_salva_desliga();
    int h = -1;
    for (int i = 0; i < MANDOS_MAX; i++) if (!mandos[i].usado) { h = i; break; }
    if (h >= 0) {
        mando_t *m = &mandos[h];
        memset(m, 0, sizeof *m);
        m->usado = true;
        m->info.id = proximo_id++;
        m->info.tipo = tipo;
        m->info.n_eixos = eixos > MANDO_EIXOS_MAX ? MANDO_EIXOS_MAX : eixos;
        m->info.n_botoes = botoes > MANDO_BOTOES_MAX ? MANDO_BOTOES_MAX : botoes;
        m->info.n_hats = hats > MANDO_HATS_MAX ? MANDO_HATS_MAX : hats;
        m->info.fabricante = fab;
        m->info.produto = prod;
        m->info.mapa = mapa;
        txt_copia_n(m->info.nome, nome, sizeof m->info.nome);
        txt_copia_n(m->info.via, via, sizeof m->info.via);
        geracao++;
    }
    int_restaura(i0);
    if (h < 0) klog("[MANDOS] demasiados mandos: %s no se registra\n", nome);
    else klog("[MANDOS] mando %u: %s (%s): %u ejes, %u botones, %u hats\n", mandos[h].info.id, nome, via,
              mandos[h].info.n_eixos, mandos[h].info.n_botoes, mandos[h].info.n_hats);
    return h;
}

void mando_atualizar(int h, const hid_estado_t *e)
{
    if (h < 0 || h >= MANDOS_MAX) return;
    bool i0 = int_salva_desliga();
    if (mandos[h].usado) {
        mandos[h].est = *e;
        mandos[h].informes++;
    }
    int_restaura(i0);
}

/* El último informe crudo, para «mandos --crudo» y --estado: con él y el descriptor se ve en
 * una máquina real si un eje mal leído es culpa del analizador o del mando. */
void mando_informe_crudo(int h, const uint8_t *b, int n)
{
    if (h < 0 || h >= MANDOS_MAX || n < 0) return;
    if (n > (int)sizeof mandos[h].crudo) n = (int)sizeof mandos[h].crudo;
    bool i0 = int_salva_desliga();
    if (mandos[h].usado) {
        memcpy(mandos[h].crudo, b, (size_t)n);
        mandos[h].crudo_n = (uint8_t)n;
    }
    int_restaura(i0);
}

void mando_descriptor(int h, const uint8_t *d, int n)
{
    if (h < 0 || h >= MANDOS_MAX || n < 0) return;
    if (n > MANDO_DESC_MAX) n = MANDO_DESC_MAX;
    bool i0 = int_salva_desliga();
    if (mandos[h].usado) {
        memcpy(mandos[h].desc, d, (size_t)n);
        mandos[h].desc_n = (uint16_t)n;
    }
    int_restaura(i0);
}

void mando_retirar(int h)
{
    if (h < 0 || h >= MANDOS_MAX) return;
    bool i0 = int_salva_desliga();
    bool estava = mandos[h].usado;
    uint32_t id = mandos[h].info.id;
    mandos[h].usado = false;
    if (estava) geracao++;
    int_restaura(i0);
    if (estava) klog("[MANDOS] mando %u retirado\n", id);
}

/* ------------------------------------------------------------ mandos de prueba */

/* El descriptor de informe del Logitech Dual Action (046d:c216), el mismo que da el F310 en
 * modo DirectInput: 4 ejes de 8 bits, un hat, 12 botones (tests/mandos_unidad.c). */
static const uint8_t desc_dual_action[] = {
    0x05, 0x01, 0x09, 0x04, 0xA1, 0x01, 0xA1, 0x02, 0x75, 0x08, 0x95, 0x04, 0x15, 0x00, 0x26, 0xFF,
    0x00, 0x35, 0x00, 0x46, 0xFF, 0x00, 0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x81, 0x02,
    0x75, 0x04, 0x95, 0x01, 0x25, 0x07, 0x46, 0x3B, 0x01, 0x65, 0x14, 0x09, 0x39, 0x81, 0x42, 0x65,
    0x00, 0x75, 0x01, 0x95, 0x0C, 0x25, 0x01, 0x45, 0x01, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0C, 0x81,
    0x02, 0x06, 0x00, 0xFF, 0x75, 0x01, 0x95, 0x10, 0x25, 0x01, 0x45, 0x01, 0x09, 0x01, 0x81, 0x02,
    0xC0, 0xA1, 0x02, 0x75, 0x08, 0x95, 0x07, 0x46, 0xFF, 0x00, 0x26, 0xFF, 0x00, 0x09, 0x02, 0x91,
    0x02, 0xC0, 0xC0,
};

static bool prova_ativa;
static hid_mando_t prova_hid;
static hid_estado_t prova_est[2];
static int prova_h[2] = { -1, -1 };   /* 0 = el Dual Action (HID), 1 = el Xbox 360 */

static void registrar_prova(void)
{
    static hid_desc_t d;
    if (hid_analisar(desc_dual_action, sizeof desc_dual_action, &d) <= 0 || !hid_mando_extrair(&d, &prova_hid)) {
        klog("[MANDOS] el descriptor de prueba no se entiende\n");
        return;
    }
    prova_ativa = true;
    prova_h[0] = mando_registrar(MANDO_TIPO_PRUEBA, "Mando de prueba (Dual Action)", "prueba HID", 0x046D, 0xC216,
                                 prova_hid.n_eixos, prova_hid.n_botoes, prova_hid.n_hats, MANDO_MAPA_DUALACTION);
    mando_descriptor(prova_h[0], desc_dual_action, (int)sizeof desc_dual_action);
    prova_h[1] = mando_registrar(MANDO_TIPO_PRUEBA, "Mando de prueba (Xbox 360)", "prueba Xbox", 0x045E, 0x028E,
                                 XBOX360_EIXOS, XBOX360_BOTOES, 1, MANDO_MAPA_XBOX360);
}

/* ------------------------------------------------------------ arranque */

void mandos_iniciar(void)
{
    if (cabecalho_boot[CAB_MANDOS_PRUEBA] & 1) registrar_prova();
    pj_detectar();
}

/* ------------------------------------------------------------ INFO_MANDOS */

/* El mando que ocupa la posición "indice" de la lista (la tabla sin huecos). */
static int por_indice(uint32_t indice)
{
    uint32_t k = 0;
    for (int i = 0; i < MANDOS_MAX; i++) {
        if (!mandos[i].usado) continue;
        if (k++ == indice) return i;
    }
    return -1;
}

int32_t mandos_info(uint32_t ptr, uint32_t n)
{
    info_mandos_t *s = (info_mandos_t *)ptr;
    if (n < sizeof *s || !usuario_ok(s, sizeof *s, true)) return ERRO_ENDERECO;
    int32_t r = ERRO_INVALIDO;
    switch (s->orden) {
    case MANDOS_LISTA: {
        bool i0 = int_salva_desliga();
        uint32_t k = 0;
        for (int i = 0; i < MANDOS_MAX; i++)
            if (mandos[i].usado) s->lista[k++] = mandos[i].info;
        s->n = k;
        s->geracao = geracao;
        int_restaura(i0);
        r = 0;
        break;
    }
    case MANDOS_ESTADO: {
        int h = por_indice(s->indice);
        if (h < 0) { r = ERRO_NAO_EXISTE; break; }
        if (h == pj_manejador()) {
            pj_ler(&s->estado, s->crudo);   /* mide el puerto ahora (mira el plazo él) */
            s->estado.id = mandos[h].info.id;
            r = 0;
            break;
        }
        bool i0 = int_salva_desliga();
        mando_t *m = &mandos[h];
        s->estado.id = m->info.id;
        s->estado.informes = m->informes;
        for (int i = 0; i < MANDO_EIXOS_MAX; i++) s->estado.eixos[i] = m->est.eixo[i];
        s->estado.botoes = m->est.botoes;
        for (int i = 0; i < MANDO_HATS_MAX; i++) s->estado.hats[i] = m->est.hat[i];
        s->informe_n = m->crudo_n;
        memcpy(s->informe, m->crudo, m->crudo_n);
        int_restaura(i0);
        for (int i = 0; i < 4; i++) s->crudo[i] = 0xFFFF;
        r = 0;
        break;
    }
    case MANDOS_DETECTAR:
        r = pj_detectar() ? 1 : 0;
        break;
    case MANDOS_CALIBRAR: {
        if (s->escribir == 1) {
            uint16_t c[4][3];
            for (int i = 0; i < 4; i++) for (int k = 0; k < 3; k++) c[i][k] = s->cal[i][k];
            pj_calibrar(c);
        } else if (s->escribir == 2) pj_calibrar(NULL);
        bool manual = false;
        pj_calibracao(s->cal, &manual);
        s->calibrado = manual;
        r = 0;
        break;
    }
    case MANDOS_DESCRIPTOR: {
        int h = por_indice(s->indice);
        if (h < 0) { r = ERRO_NAO_EXISTE; break; }
        bool i0 = int_salva_desliga();
        mando_t *m = &mandos[h];
        uint32_t de = s->escribir, k = 0;
        s->n = m->desc_n;
        for (; de + k < m->desc_n && k < sizeof s->informe; k++) s->informe[k] = m->desc[de + k];
        s->informe_n = k;
        int_restaura(i0);
        r = 0;
        break;
    }
    case MANDOS_INYECTAR: {
        if (!prova_ativa) { r = ERRO_ACESSO; break; }
        int h = por_indice(s->indice);
        int k = h < 0 ? -1 : h == prova_h[0] ? 0 : h == prova_h[1] ? 1 : -1;
        if (k < 0 || s->informe_n > sizeof s->informe) { r = ERRO_INVALIDO; break; }
        hid_estado_t *e = &prova_est[k];
        bool ok = k == 0 ? hid_mando_decodificar(&prova_hid, s->informe, (int)s->informe_n, e)
                         : xbox360_decodificar(s->informe, (int)s->informe_n, e);
        mando_informe_crudo(h, s->informe, (int)s->informe_n);
        if (ok) mando_atualizar(h, e);
        r = ok ? 0 : ERRO_INVALIDO;
        break;
    }
    }
    s->resultado = r;
    return r;
}
