/* kernel/rede/ure_ids.h - VID:PID de los Ethernet USB Realtek RTL8152/8153/8156 (D433).
 * GENERADO por tools/gerar_ure_ids.py desde OpenBSD sys/dev/usb/if_ure.c (ure_devs[])
 * y usbdevs.h: no editar a mano. Solo identificadores numericos. */
#ifndef SAGUI_URE_IDS_H
#define SAGUI_URE_IDS_H

struct ure_id { uint16_t vid, pid; };

static const struct ure_id ure_ids[] = {
    { 0x043E, 0x9819 },   /* LG_RTL8153 */
    { 0x043E, 0x9820 },   /* LG_RTL8153B */
    { 0x045E, 0x07AB },   /* MICROSOFT_DOCKETH */
    { 0x045E, 0x07C6 },   /* MICROSOFT_DOCKETH2 */
    { 0x045E, 0x0927 },   /* MICROSOFT_SURFETH */
    { 0x045E, 0x0C5E },   /* MICROSOFT_WINDEVETH */
    { 0x04E8, 0xA101 },   /* SAMSUNG2_RTL8153 */
    { 0x050D, 0x047A },   /* BELKIN_RTL8152B */
    { 0x050D, 0x048A },   /* BELKIN_RTL8153 */
    { 0x056E, 0x4013 },   /* ELECOM_RTL8153B */
    { 0x056E, 0x4017 },   /* ELECOM_RTL8156B */
    { 0x0930, 0x0416 },   /* TOSHIBA_RTL8153B */
    { 0x0955, 0x09FF },   /* NVIDIA_TEGRAETH */
    { 0x0B05, 0x18D1 },   /* ASUS_RTL8156 */
    { 0x0BDA, 0x8050 },   /* REALTEK_RTL8152B */
    { 0x0BDA, 0x8152 },   /* REALTEK_RTL8152 */
    { 0x0BDA, 0x8153 },   /* REALTEK_RTL8153 */
    { 0x0BDA, 0x8156 },   /* REALTEK_RTL8156 */
    { 0x0BDA, 0x8157 },   /* REALTEK_RTL8157 */
    { 0x0BDA, 0x815A },   /* REALTEK_RTL8159 */
    { 0x0F21, 0x8153 },   /* IOI_RTL8153 */
    { 0x1025, 0x104E },   /* TTL_RTL8153 */
    { 0x13B1, 0x0041 },   /* CISCOLINKSYS_USB3GIGV1 */
    { 0x14FF, 0x1400 },   /* TWINHEAD_RTL8153B */
    { 0x17EF, 0x304F },   /* LENOVO_ONELINKPRO */
    { 0x17EF, 0x3052 },   /* LENOVO_TABLETDOCK */
    { 0x17EF, 0x3054 },   /* LENOVO_ONELINKPLUS */
    { 0x17EF, 0x3057 },   /* LENOVO_WIGIGDOCK */
    { 0x17EF, 0x3062 },   /* LENOVO_DOCK_ETHERNET */
    { 0x17EF, 0x3065 },   /* LENOVO_TB3GFXDOCK */
    { 0x17EF, 0x3069 },   /* LENOVO_RTL8153B_1 */
    { 0x17EF, 0x3082 },   /* LENOVO_TB3DOCKGEN2 */
    { 0x17EF, 0x3098 },   /* LENOVO_RTL8153B_2 */
    { 0x17EF, 0x309B },   /* LENOVO_RTL8153B_3 */
    { 0x17EF, 0x309C },   /* LENOVO_RTL8153B_4 */
    { 0x17EF, 0x309D },   /* LENOVO_RTL8153B_5 */
    { 0x17EF, 0x7205 },   /* LENOVO_RTL8153_1 */
    { 0x17EF, 0x720A },   /* LENOVO_ONELINK */
    { 0x17EF, 0x720B },   /* LENOVO_RTL8153_2 */
    { 0x17EF, 0x720C },   /* LENOVO_RTL8153_3 */
    { 0x17EF, 0x7214 },   /* LENOVO_RTL8153B_6 */
    { 0x17EF, 0x721E },   /* LENOVO_RTL8153B_7 */
    { 0x17EF, 0x8153 },   /* LENOVO_RTL8153B_8 */
    { 0x17EF, 0xA359 },   /* LENOVO_RTL8153B_9 */
    { 0x17EF, 0xA387 },   /* LENOVO_USBCDOCKGEN2 */
    { 0x17EF, 0xA3C1 },   /* LENOVO_TB3DOCK */
    { 0x2001, 0x7E34 },   /* DLINK_RTL8153_1 */
    { 0x2001, 0xA710 },   /* DLINK_RTL8153_2 */
    { 0x208E, 0xD003 },   /* LUXSHARE_RTL8153 */
    { 0x20F4, 0xE02B },   /* TRENDNET_RTL8156 */
    { 0x2357, 0x0601 },   /* TPLINK_EU300 */
    { 0x2357, 0x0602 },   /* TPLINK_RTL8152B_1 */
    { 0x2357, 0x0603 },   /* TPLINK_RTL8152B_2 */
    { 0x2717, 0x0011 },   /* XIAOMI_RTL8152B */
    { 0x2B73, 0x0007 },   /* PIONEERDJ_RTL8152B */
    { 0x2B73, 0x0031 },   /* PIONEERDJ_RTL8153B */
    { 0x2D1C, 0x8153 },   /* CLUB3D_RTL8153 */
    { 0x30DA, 0x5101 },   /* CLEVO_RTL8153B */
    { 0x30F3, 0x0419 },   /* DYNABOOK_RTL8153B_1 */
    { 0x30F3, 0x0425 },   /* DYNABOOK_RTL8153B_2 */
};
#define URE_N_IDS ((int)(sizeof ure_ids / sizeof ure_ids[0]))

#endif
