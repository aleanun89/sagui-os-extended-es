/* wifi_usb_logica.h - qué familia de pincho Wi-Fi USB es un VID:PID (D409).
 * Pura (para probarla en el anfitrión). El RTL8188EU (0bda:8179, D215) es el único
 * probado en hardware; el resto son experimentales. */
#ifndef SAGUI_WIFI_USB_LOGICA_H
#define SAGUI_WIFI_USB_LOGICA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdbool.h>
#include <stdint.h>
#endif
#include "wifi_usb_ids.h"

enum {
    WUSB_NINGUNO = 0,
    WUSB_RTL8188EU,    /* Realtek RTL8188EU/ETV (D215): probado */
    WUSB_RTL8192CU,    /* Realtek RTL8188CU/RTL8192CU/RTL8188RU (urtwn): experimental */
    WUSB_RT2870,       /* Ralink RT2870/RT3070/RT5370... (run): experimental */
    WUSB_AR9271,       /* Atheros AR9271 (ath9k_htc): reconocido, sin driver todavía */
};

static inline bool wusb_en(const struct wusb_id *t, int n, uint16_t vid, uint16_t pid)
{
    for (int i = 0; i < n; i++) if (t[i].vid == vid && t[i].pid == pid) return true;
    return false;
}

#define WUSB_N(t) ((int)(sizeof(t) / sizeof((t)[0])))

static inline int wusb_familia(uint16_t vid, uint16_t pid)
{
    if (vid == 0x0BDA && pid == 0x8179) return WUSB_RTL8188EU;
    if (wusb_en(wusb_ids_rtl8192cu, WUSB_N(wusb_ids_rtl8192cu), vid, pid)) return WUSB_RTL8192CU;
    if (wusb_en(wusb_ids_rt2870, WUSB_N(wusb_ids_rt2870), vid, pid)) return WUSB_RT2870;
    if (wusb_en(wusb_ids_ar9271, WUSB_N(wusb_ids_ar9271), vid, pid)) return WUSB_AR9271;
    return WUSB_NINGUNO;
}

static inline const char *wusb_nome_familia(int f)
{
    switch (f) {
    case WUSB_RTL8188EU: return "Realtek RTL8188EU";
    case WUSB_RTL8192CU: return "Realtek RTL8188CU/8192CU";
    case WUSB_RT2870:    return "Ralink RT2870/RT3070/RT5370";
    case WUSB_AR9271:    return "Atheros AR9271";
    }
    return "";
}

/* Los que no se probaron en hardware (D409): el aviso del sistema los marca. */
static inline bool wusb_experimental(int f) { return f == WUSB_RTL8192CU || f == WUSB_RT2870 || f == WUSB_AR9271; }
#endif
