// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Extraído de sound/pci/hda/patch_sigmatel.c (Linux v6.6):
 * HD audio interface patch for SigmaTel STAC92xx
 *
 * Copyright (c) 2005 Embedded Alley Solutions, Inc.
 * Matt Porter <mporter@embeddedalley.com>
 *
 * Based on patch_cmedia.c and patch_realtek.c
 * Copyright (c) 2004 Takashi Iwai <tiwai@suse.de>
 *
 * SaguiOS (D073, D107): solo DATOS de la familia IDT/SigmaTel 92HD71Bxx y
 * 92HD75Bxx (los códecs de los HP Pavilion dv4/dv5/dv6/dv7):
 *   - los identificadores de códec que Linux trata con patch_stac92hd71bxx();
 *   - los GPIO por defecto («GPIO0 = EAPD») y el GPIO5 del subwoofer
 *     (stac_add_hp_bass_switch, «HP dv7 bass switch - GPIO5»);
 *   - los verbos propios de IDT (mapa de energía 0x7ec/0xfec, GPIO en CMOS 0x7e7);
 *   - los nodos del mapa de energía (stac92hd71bxx_pwr_nids), los que se
 *     desmutean (stac92hd71bxx_unmute_nids) y el control de volumen 0x28
 *     (stac92hd71bxx_core_init);
 *   - la tabla de subsistemas stac92hd71bxx_fixup_tbl (solo las filas de HP
 *     y la de la placa de referencia; las de Dell llevan tablas de pines que
 *     no usamos) y lo que hace cada ajuste (stac92hd71bxx_fixup_*).
 * La lógica que aplica esto es propia: kernel/dispositivos/hda_ajustes.c y
 * hda.c. Cambios: C freestanding, estructuras propias en lugar de
 * snd_pci_quirk/hda_fixup; las cadenas de ajustes (HP_DV4 -> HP_DV5 -> HP)
 * van ya resueltas como banderas. Ver docs/terceros.md.
 */
#ifndef TERCEIROS_LINUX_HDA_IDT_92HD71BXX_H
#define TERCEIROS_LINUX_HDA_IDT_92HD71BXX_H

/* Verbos propios de IDT (patch_sigmatel.c: AC_VERB_IDT_SET_POWER_MAP...) */
#define IDT_VERBO_MAPA_ENERGIA      0x7ec   /* bit = 1: el puerto queda apagado */
#define IDT_VERBO_LER_MAPA_ENERGIA  0xfec
#define IDT_VERBO_GPIO_CMOS         0x7e7   /* stac_gpio_set(): «Configure GPIOx as CMOS» con 0 */

/* stac92hd71bxx_pwr_nids: los puertos con bit en el mapa de energía (A, D, F) */
static const unsigned char idt71_nos_energia[3] = { 0x0a, 0x0d, 0x0f };

/* stac92hd71bxx_unmute_nids: «unmute right and left channels for nodes
 * 0x0f, 0xa, 0x0d» (amplificador de entrada, índice 0). En los 92HD71B5X
 * (76b6/76b7) y 92HD75B2X5 (7608) Linux empieza por el segundo. */
static const unsigned char idt71_nos_desmutar[3] = { 0x0f, 0x0a, 0x0d };

/* stac92hd71bxx_core_init: «set master volume and direct control» */
#define IDT71_NO_VOLUME     0x28
#define IDT71_VOLUME_DIRETO 0xff            /* AC_VERB_SET_VOLUME_KNOB_CONTROL (0x70f) */

/* patch_stac92hd71bxx(): «GPIO0 = EAPD» */
#define IDT71_GPIO_EAPD     0x01
/* stac_add_hp_bass_switch(): «HP dv7 bass switch - GPIO5»; Linux lo añade
 * en el ajuste HP_DV5 si el códec tiene 6 GPIO o más («enable bass on HP dv7») */
#define IDT71_GPIO_GRAVES   0x20
#define IDT71_GPIO_GRAVES_MIN_GPIOS 6

/* stac92hd71bxx_fixup_hp_dv5(): pin 0x0d (altavoz) con otra configuración */
#define IDT71_DV5_PINO      0x0d
#define IDT71_DV5_CONFIG    0x90170010u

/* stac92hd71bxx_fixup_hp(): si los puertos A (0x0a) y F (0x0f) son salidas
 * de auriculares o de línea, «these are supposedly dock and built-in
 * headphones»: los dos pasan a auriculares (fixup_hp_headphone). */
#define IDT71_HP_PINO_A     0x0a
#define IDT71_HP_PINO_F     0x0f

/* patch_stac92hd71bxx(), caso 0x111d7608: «disable VSW» con dos pines sin uso */
#define IDT7608_PINO_F      0x0f
#define IDT7608_CONFIG_F    0x40f000f0u
#define IDT7608_PINO_VSW    0x19
#define IDT7608_CONFIG_VSW  0x40f000f3u

/* Identificadores de códec de patch_stac92hd71bxx() (snd_hda_preset_sigmatel) */
static const unsigned int idt71_codecs[] = {
    0x111d7603,   /* 92HD75B3X5 «6 Port with Analog Mixer» */
    0x111d7608,   /* 92HD75B2X5 «5 Port with Analog Mixer» */
    0x111d76b0, 0x111d76b1,   /* 92HD71B8X */
    0x111d76b2, 0x111d76b3,   /* 92HD71B7X */
    0x111d76b4, 0x111d76b5,   /* 92HD71B6X */
    0x111d76b6, 0x111d76b7,   /* 92HD71B5X «4 Port without Analog Mixer» */
    0
};

/* Ajustes de stac92hd71bxx_fixups, con la cadena ya resuelta:
 *   REF            -> sin GPIO (gpio_mask = gpio_dir = gpio_data = 0)
 *   HP_DV4         -> gpio_led 0x01, y sigue a HP_DV5
 *   HP_DV5         -> pin 0x0d = 0x90170010, GPIO5 de graves, y sigue a HP
 *   HP_HDX         -> gpio_led 0x08, y sigue a HP
 *   HP_M4          -> GPIO1 en la máscara (detección de VREF), y sigue a HP
 *   HP             -> auriculares en A y F (ver arriba) */
#define IDT71_REF     1
#define IDT71_HP_DV4  2
#define IDT71_HP_DV5  3
#define IDT71_HP_HDX  4
#define IDT71_HP_M4   5
#define IDT71_HP      6

/* stac92hd71bxx_fixup_tbl (filas de HP y de la referencia). Se compara
 * (subsistema & mascara) == id; vale la primera fila. La última es
 * SND_PCI_QUIRK_VENDOR(PCI_VENDOR_ID_HP, ...): cualquier HP. */
typedef struct {
    unsigned short fabricante, mascara, id;
    unsigned char  ajuste;
    const char    *nome;
} idt71_subsistema_t;

static const idt71_subsistema_t idt71_subsistemas[] = {
    { 0x8086, 0xffff, 0x2668, IDT71_REF,    "DFI LanParty" },
    { 0x15bd, 0xffff, 0x3101, IDT71_REF,    "DFI LanParty" },
    { 0x103c, 0xfff0, 0x1720, IDT71_HP_DV5, "HP" },
    { 0x103c, 0xfff0, 0x3080, IDT71_HP_DV5, "HP" },
    { 0x103c, 0xfff0, 0x30f0, IDT71_HP_DV4, "HP dv4-7" },
    { 0x103c, 0xfff0, 0x3600, IDT71_HP_DV5, "HP dv4-7" },
    { 0x103c, 0xffff, 0x3610, IDT71_HP_HDX, "HP HDX" },
    { 0x103c, 0xffff, 0x361a, IDT71_HP_M4,  "HP mini 1000" },
    { 0x103c, 0xffff, 0x361b, IDT71_HP_HDX, "HP HDX" },
    { 0x103c, 0xfff0, 0x3620, IDT71_HP_DV5, "HP dv6" },
    { 0x103c, 0xffff, 0x3061, IDT71_HP_DV5, "HP dv6" },
    { 0x103c, 0xffff, 0x363e, IDT71_HP_DV5, "HP DV6" },
    { 0x103c, 0xfff0, 0x7010, IDT71_HP_DV5, "HP" },
    { 0x103c, 0x0000, 0x0000, IDT71_HP,     "HP" },
    { 0, 0, 0, 0, 0 }
};

#endif
