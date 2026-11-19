// SPDX-License-Identifier: GPL-2.0-or-later
/* acoes.c - acciones comunes del sistema (volumen, silencio, brillo, radio) y su
 * aviso en pantalla (D403). Ver acoes.h.
 *
 * Por qué una cola y no hacerlo en el momento: la tecla llega en la IRQ del
 * teclado (o en la tarea "acpi-aml", con el intérprete a medias), y cambiar el
 * volumen puede tocar la tarjeta (el silencio de un audio USB es una
 * transferencia de control), y el brillo es AML. Así, quien avisa no espera, y
 * las acciones se hacen una detrás de otra en un sitio conocido.
 *
 * La cola es pequeña (16): si se llena, la tecla se pierde (y se anota), nunca
 * se pisa lo encolado. El aviso en pantalla es una sola casilla: el último
 * manda. */
#include "acoes.h"
#include "audio.h"
#include "console.h"
#include "cpu.h"
#include "gui.h"
#include "portatil.h"
#include "processo.h"
#include "texto.h"
#include "wifi.h"

#define FILA        16
#define PASSO_VOL   5

static struct { uint8_t acao, origem; } fila[FILA];
static volatile uint32_t f_ini, f_fim;
static uint32_t perdidas;

static volatile int osd_tipo, osd_valor;
static volatile bool osd_novo;

#define FILA_AVISOS 4
static char avisos[FILA_AVISOS][ACOES_AVISO_BYTES];
static volatile uint32_t v_ini, v_fim;

static bool radio_apagada;
static uint32_t feitas, ultima;

static const char *nome_acao(int a)
{
    static const char *const n[ACAO_MAX] = { "?", "volumen+", "volumen-", "silencio", "brillo+", "brillo-",
                                             "radio", "pantalla" };
    return a > 0 && a < ACAO_MAX ? n[a] : "?";
}

static const char *nome_origem(int o)
{
    static const char *const n[] = { "?", "teclado", "acpi-video", "fujitsu", "toshiba", "orden", "hp" };
    return o > 0 && o <= ORIGEM_ACAO_HP ? n[o] : "?";
}

void acao_sistema(int acao, int origem)
{
    if (acao <= 0 || acao >= ACAO_MAX) return;
    bool i0 = int_salva_desliga();
    if (f_fim - f_ini >= FILA) {
        perdidas++;
        int_restaura(i0);
        return;
    }
    fila[f_fim % FILA].acao = (uint8_t)acao;
    fila[f_fim % FILA].origem = (uint8_t)origem;
    f_fim++;
    int_restaura(i0);
    /* Sin escritorio, quien atiende es la tarea "acpi-aml" (si existe). El
     * escritorio mira la cola en cada tic, no hace falta despertarlo. */
    uacpi_sagui_acordar();
}

void acoes_osd(int tipo, int valor)
{
    bool i0 = int_salva_desliga();
    osd_tipo = tipo;
    osd_valor = valor;
    osd_novo = true;
    int_restaura(i0);
}

bool acoes_osd_proximo(int *tipo, int *valor)
{
    bool i0 = int_salva_desliga();
    bool ha = osd_novo;
    if (ha) {
        *tipo = osd_tipo;
        *valor = osd_valor;
        osd_novo = false;
    }
    int_restaura(i0);
    return ha;
}

void acoes_aviso(const char *texto)
{
    bool i0 = int_salva_desliga();
    if (v_fim - v_ini >= FILA_AVISOS) v_ini++;   /* llena: se pierde el más viejo */
    txt_copia_n(avisos[v_fim % FILA_AVISOS], texto, ACOES_AVISO_BYTES);
    v_fim++;
    int_restaura(i0);
}

bool acoes_aviso_proximo(char *dst, uint32_t max)
{
    bool i0 = int_salva_desliga();
    bool ha = v_ini != v_fim;
    if (ha) {
        txt_copia_n(dst, avisos[v_ini % FILA_AVISOS], max);
        v_ini++;
    }
    int_restaura(i0);
    return ha;
}

bool acoes_radio_apagada(void) { return radio_apagada; }
uint32_t acoes_feitas(void) { return feitas; }
uint32_t acoes_ultima(void) { return ultima; }

/* -1 en el volumen o en el silencio: no hay tarjeta de sonido funcionando. */
static int volume_atual(void) { return sis_audio_controle((uint32_t)-1, AUDIO_VOLUME, (uint32_t)-1, 0, 0); }

static void volume(int delta)
{
    int v = volume_atual();
    if (v < 0) { acoes_osd(OSD_SEM_SOM, 0); return; }
    v += delta;
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    sis_audio_controle((uint32_t)-1, AUDIO_VOLUME, (uint32_t)v, 0, 0);
    /* Como en casi todos los sistemas: tocar el volumen quita el silencio. */
    if (sis_audio_controle((uint32_t)-1, AUDIO_SILENCIO, (uint32_t)-1, 0, 0) == 1)
        sis_audio_controle((uint32_t)-1, AUDIO_SILENCIO, 0, 0, 0);
    acoes_osd(OSD_VOLUME, v);
}

static void silencio(void)
{
    int s = sis_audio_controle((uint32_t)-1, AUDIO_SILENCIO, (uint32_t)-1, 0, 0);
    if (s < 0) { acoes_osd(OSD_SEM_SOM, 0); return; }
    s = !s;
    sis_audio_controle((uint32_t)-1, AUDIO_SILENCIO, (uint32_t)s, 0, 0);
    acoes_osd(s ? OSD_SILENCIO : OSD_VOLUME, s ? 1 : volume_atual());
}

/* El modo avión: lo que SaguiOS sabe apagar es su Wi-Fi (se desconecta y no se
 * deja conectar ni buscar hasta volver a encenderla); la luz de los Fujitsu que
 * la tienen la pone portatil.c. La radio de las tarjetas que no maneja SaguiOS
 * sigue como la dejó la BIOS. */
static void radio(void)
{
    radio_apagada = !radio_apagada;
    if (radio_apagada) sis_wifi(WIFI_DESCONECTAR, 0, 0, 0, 0);
    portatil_radio_mudou(radio_apagada);
    klog("[ACCIONES] radio inalambrica %s\n", radio_apagada ? "apagada (modo avion)" : "encendida");
    acoes_osd(OSD_RADIO, radio_apagada ? 1 : 0);
}

static void fazer(int acao, int origem)
{
    feitas++;
    ultima = (uint32_t)acao << 8 | (uint32_t)origem;
    if (feitas <= 64) klog("[ACCIONES] %s (%s)\n", nome_acao(acao), nome_origem(origem));
    switch (acao) {
    case ACAO_VOLUME_MAIS:  volume(PASSO_VOL); break;
    case ACAO_VOLUME_MENOS: volume(-PASSO_VOL); break;
    case ACAO_SILENCIO:     silencio(); break;
    case ACAO_BRILHO_MAIS:
    case ACAO_BRILHO_MENOS:
        /* El aviso lo deja la tarea "acpi-aml" cuando lo haya cambiado. */
        if (portatil_brilho_pedir(PORTATIL_BRILHO_PASSO, acao == ACAO_BRILHO_MAIS ? 1 : -1, origem) < 0)
            acoes_osd(OSD_SEM_BRILHO, 0);
        break;
    case ACAO_RADIO:        radio(); break;
    case ACAO_TELA:         acoes_osd(OSD_TELA, 0); break;
    }
}

void acoes_atender(void)
{
    static uint32_t perdidas_vistas;
    for (int n = 0; n < FILA; n++) {
        bool i0 = int_salva_desliga();
        if (f_ini == f_fim) { int_restaura(i0); break; }
        int acao = fila[f_ini % FILA].acao, origem = fila[f_ini % FILA].origem;
        f_ini++;
        int_restaura(i0);
        fazer(acao, origem);
    }
    if (!gui_ativa()) {   /* sin escritorio, los avisos de texto van a la consola */
        char t[ACOES_AVISO_BYTES];
        while (acoes_aviso_proximo(t, sizeof t)) {
            console_cor(COR_AMARELO, COR_PRETO);
            kprintf("\n%s\n", t);
            console_cor(COR_CINZA, COR_PRETO);
        }
    }
    if (perdidas != perdidas_vistas) {
        perdidas_vistas = perdidas;
        klog("[ACCIONES] cola llena: %u acciones perdidas\n", perdidas);
    }
}
