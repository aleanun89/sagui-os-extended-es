// SPDX-License-Identifier: GPL-2.0-or-later
/* hda_ajustes.c - elección del ajuste de un códec HDA (D107).
 *
 * Solo lógica pura, sin hardware: compila igual en el kernel y en el
 * anfitrión (la prueba h79 la ejecuta con muchos identificadores). Los
 * datos de los códecs IDT 92HD71Bxx/92HD75Bxx salen de Linux y están
 * aislados en kernel/terceros/linux/hda/ (D073). */
#include "hda_ajustes.h"

/* Sin GPL (mejora 057, D109) no entran los datos de Linux: ningún códec
 * recibe ajuste y el driver usa la elección genérica de pines. */
#ifndef SAGUI_SEM_GPL
#include "../terceros/linux/hda/idt_92hd71bxx.h"

static void zerar(hda_ajuste_t *a)
{
    uint8_t *p = (uint8_t *)a;
    for (uint32_t i = 0; i < sizeof *a; i++) p[i] = 0;
}

static void pino(hda_ajuste_t *a, uint8_t no, uint32_t config)
{
    if (a->n_pinos >= HDA_AJUSTE_MAX_PINOS) return;
    a->pino_no[a->n_pinos] = no;
    a->pino_config[a->n_pinos] = config;
    a->n_pinos++;
}

static bool familia_idt71(uint32_t codec)
{
    for (int i = 0; idt71_codecs[i]; i++)
        if (idt71_codecs[i] == codec) return true;
    return false;
}

static const idt71_subsistema_t *procurar_subsistema(uint32_t ssid)
{
    uint16_t fab = (uint16_t)(ssid >> 16), id = (uint16_t)ssid;
    if (!ssid || ssid == 0xFFFFFFFFu) return NULL;
    for (int i = 0; idt71_subsistemas[i].fabricante; i++) {
        const idt71_subsistema_t *s = &idt71_subsistemas[i];
        if (s->fabricante == fab && (id & s->mascara) == s->id) return s;
    }
    return NULL;
}

/* IDT 92HD71Bxx / 92HD75Bxx: lo que hace patch_stac92hd71bxx() y el ajuste elegido. */
static void ajuste_idt71(uint32_t codec, uint32_t ssid_pci, uint32_t ssid_codec, uint32_t n_gpio, hda_ajuste_t *a)
{
    a->nome = "idt71";
    a->modelo = "IDT 92HD71B/75B";
    a->gpio_mascara = a->gpio_direcao = a->gpio_dados = IDT71_GPIO_EAPD;   /* «GPIO0 = EAPD» */
    a->verbo_gpio_cmos = IDT_VERBO_GPIO_CMOS;
    a->verbo_mapa_energia = IDT_VERBO_MAPA_ENERGIA;
    (void)idt71_nos_energia;   /* el mapa se escribe entero (0 = todos encendidos) */
    a->no_volume = IDT71_NO_VOLUME;
    a->volume_valor = IDT71_VOLUME_DIRETO;
    a->nos_desmutar = idt71_nos_desmutar;
    a->n_desmutar = 3;
    if (codec == 0x111d76b6 || codec == 0x111d76b7 || codec == 0x111d7608) {
        a->nos_desmutar++;
        a->n_desmutar--;
    }
    if (codec == 0x111d7608) {           /* «disable VSW» */
        pino(a, IDT7608_PINO_F, IDT7608_CONFIG_F);
        pino(a, IDT7608_PINO_VSW, IDT7608_CONFIG_VSW);
    }

    const idt71_subsistema_t *s = procurar_subsistema(ssid_pci);
    uint32_t ssid = ssid_pci;
    if (!s) { s = procurar_subsistema(ssid_codec); ssid = ssid_codec; }
    if (!s) return;
    a->ssid = ssid;
    a->modelo = s->nome;
    uint8_t led = 0;
    bool dv5 = false, hp = false;
    switch (s->ajuste) {
    case IDT71_REF:
        a->nome = "idt71-ref";
        a->gpio_mascara = a->gpio_direcao = a->gpio_dados = 0;
        break;
    case IDT71_HP_DV4:
        a->nome = "idt71-hp-dv4";
        led = 0x01;
        dv5 = hp = true;
        break;
    case IDT71_HP_DV5:
        a->nome = "idt71-hp-dv5";
        dv5 = hp = true;
        break;
    case IDT71_HP_HDX:
        a->nome = "idt71-hp-hdx";
        led = 0x08;
        hp = true;
        break;
    case IDT71_HP_M4:
        a->nome = "idt71-hp-m4";
        a->gpio_mascara |= 0x02;         /* detección de VREF por GPIO1 (solo la máscara) */
        hp = true;
        break;
    default:
        a->nome = "idt71-hp";
        hp = true;
        break;
    }
    if (dv5) {
        pino(a, IDT71_DV5_PINO, IDT71_DV5_CONFIG);
        if (n_gpio >= IDT71_GPIO_GRAVES_MIN_GPIOS) {   /* subwoofer del dv7 */
            a->gpio_mascara |= IDT71_GPIO_GRAVES;
            a->gpio_direcao |= IDT71_GPIO_GRAVES;
            a->gpio_dados |= IDT71_GPIO_GRAVES;
        }
    }
    if (led) {                           /* stac_setup_gpio(): el LED de silencio, encendido = con sonido */
        a->gpio_mascara |= led;
        a->gpio_direcao |= led;
        a->gpio_dados |= led;
    }
    if (hp) {
        a->hp_auriculares = true;
        a->hp_pino_a = IDT71_HP_PINO_A;
        a->hp_pino_f = IDT71_HP_PINO_F;
    }
}

bool hda_ajuste_escolher(uint32_t codec, uint32_t ssid_pci, uint32_t ssid_codec, uint32_t n_gpio, hda_ajuste_t *a)
{
    zerar(a);
    if (familia_idt71(codec)) {
        ajuste_idt71(codec, ssid_pci, ssid_codec, n_gpio, a);
        return true;
    }
    return false;
}
#else
bool hda_ajuste_escolher(uint32_t codec, uint32_t ssid_pci, uint32_t ssid_codec, uint32_t n_gpio, hda_ajuste_t *a)
{
    (void)codec; (void)ssid_pci; (void)ssid_codec; (void)n_gpio;
    uint8_t *p = (uint8_t *)a;
    for (uint32_t i = 0; i < sizeof *a; i++) p[i] = 0;
    return false;
}
#endif

bool hda_config_saida_fones(uint32_t config)
{
    uint32_t disp = (config >> 20) & 0xF;
    return (config >> 30) != 1 && (disp == 0 || disp == 2);
}

uint32_t hda_config_para_fones(uint32_t config)
{
    config = (config & ~0x00F00000u) | (2u << 20);
    return (config & ~0xFFu) | 0x1F;
}
