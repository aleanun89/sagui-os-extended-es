/* wifi.c - Wi-Fi cliente (802.11 b/g) con el pincho Realtek RTL8188EU (D215).
 *
 * Una tarea del kernel, «wifi», hace todo lo que habla con el pincho (cada
 * acceso es una transferencia USB que duerme); la pila IP, que corre con las
 * interrupciones desactivadas y no puede dormir, solo ve dos colas en
 * memoria: lo que hay que mandar y lo que llegó (como un anillo de una
 * tarjeta PCI). Lo que hace la tarea:
 *   1. cambio de modo: el pincho puede aparecer como CD virtual (1111:1111,
 *      con el driver de Windows); se le manda la orden de expulsión SCSI de
 *      usb_modeswitch (StandardEject) y vuelve como 0bda:8179;
 *   2. eFuse (MAC), firmware de /lib/firmware y tablas del chip (rtl8188eu.c);
 *   3. búsqueda: por los canales 1 a 13, sondeo activo y balizas;
 *   4. conexión: autenticación abierta (o de clave compartida con WEP, o SAE
 *      con WPA3), asociación y el saludo de cuatro pasos (kernel/lib/wpa.c)
 *      con las claves CCMP en la CAM del chip (cifrado por hardware) y la
 *      renovación de la GTK;
 *   5. datos: Ethernet <-> 802.11 (LLC/SNAP) para la pila IP, como wlan0;
 *   6. pérdida de balizas, reconexión y redes recordadas (/home/.config/wifi.conf,
 *      que escribe el programa «wifi»: el SSID y la PMK, nunca la contraseña).
 *
 * D220 (v7.3.2): WEP (40 y 104 bits, abierta y de clave compartida), WPA y
 * WPA/WPA2 mixto con TKIP, y WPA3-Personal (SAE, kernel/lib/sae.c) con la
 * 802.11w. Por hardware solo va el CCMP de las tramas de datos (como el urtwn
 * de OpenBSD): TKIP y WEP se cifran y descifran aquí (kernel/lib/wpa_cifra.c),
 * igual que el CCMP de las tramas de gestión protegidas y el BIP.
 *
 * D238 (v7.4.2): rendimiento. La recepción con una cola de lecturas USB en
 * vuelo (la IRQ del xHCI despierta a la tarea), el envío sin esperar a cada
 * trama (xHCI), la 802.11n (HT, MCS 0-7 a 20 MHz, GI corto) cuando la red la
 * tiene, con WMM y CCMP, y los acuerdos de Block Ack rechazados (no hay búfer de
 * reordenación). Cada cosa vuelve sola a lo de antes si falla, y se puede
 * apagar en /home/.config/wifi-desempenho.conf («ht 0», «filas 0»).
 *
 * D280 (v8.2): el mismo programa y la misma pila 802.11 sirven también a la
 * Wi-Fi integrada Atheros AR9485 (PCIe 168c:0032, kernel/rede/ar9485.c): sin
 * firmware ni eFuse, el chip se enciende desde la propia tarea y los anillos DMA
 * se sondean. Las llamadas al chip pasan por las funciones «drv_*», que van al
 * RTL8188EU o al AR9485 según cuál esté (por ahora solo uno a la vez). El AR9485
 * se asocia como b/g (sin HT) y su CCMP va por software (ar9485.c lo explica).
 *
 * No se admiten: Enterprise, A-MPDU, 40 MHz, ahorro de energía ni 5 GHz (el
 * RTL8188EU no lo tiene; el AR9485 es de 2,4 GHz). */
#include "wifi.h"
#include "ar9485.h"
#include "wpa.h"
#include "wpa_cifra.h"
#include "sae.h"
#include "abi.h"
#include "aleatorio.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "permisos.h"
#include "processo.h"
#include "rede.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"
#include "vfs.h"

#define FIRMWARE "/lib/firmware/rtl8188eufw.bin"
#define CONFIG   "/home/.config/wifi.conf"
#define AJUSTES  "/home/.config/wifi-desempenho.conf"   /* D238: «ht 0» y «filas 0» apagan lo nuevo */

#define MAX_REDES  32
#define FILA       32
#define FILA_RX    64                  /* D238: una transferencia agregada trae varias tramas de golpe */

/* ------------------------------------------------------------ estado */

static r88e_t chip = { .rx_h = -1 };
static usb_disp_t *disp_novo;          /* lo apuntó usb.c: la tarea lo arranca */
static bool tarefa_criada;
static bool via_pci;                   /* D280: el chip es el AR9485 (PCIe), no el pincho USB */
static bool pci_novo;                  /* la tarjeta PCI está por encender: la tarea la arranca */
static bool ajuste_cifra_hw;           /* «cifrado hw» en wifi-desempenho.conf: CCMP del AR9485 por hardware */
static const char canal_tarefa = 0;

static uint32_t estado = WIFI_SEM_PINCHO;
static char erro[80];
static char nome_chip[40];

typedef struct {
    uint8_t   bssid[6];
    wpa_bss_t i;
    int8_t    rssi;
    uint16_t  intervalo, capacidade;
    uint8_t   canal_rx;      /* canal en el que se oyó */
    uint32_t  visto;
    wn_ap_t   n;             /* D238: HT y WMM de la baliza */
} bss_t;

static bss_t redes[MAX_REDES];
static int nredes;
static bool buscando, pedido_busca;
static uint32_t buscas;

static pedido_wifi_t pedido;           /* la conexión pedida (o la recordada) */
static bool pedido_conectar, pedido_desconectar, auto_conectar = true;
static bss_t alvo;                     /* la red a la que estamos conectados (o conectando) */
static uint16_t aid;
static bool chaves_ok;
static wpa_sup_t sup;
static uint64_t pn_tx;                 /* contador del CCMP o TSC del TKIP de lo que mandamos */
static uint64_t pn_rx_par[17], pn_rx_grupo; /* contra la repetición (por TID; 16 = sin QoS) */
static uint32_t ultima_baliza, conectado_em, saudacao_desde;
static int sinal = -100;
static uint32_t rx_quadros, tx_quadros, balizas, respostas_sondeo, swdec;

/* D220: cómo es la conexión en curso. */
#define MODO_ABERTA 0
#define MODO_WEP    1
#define MODO_WPA    2                  /* saludo de cuatro pasos (WPA, WPA2, PSK-SHA256) */
#define MODO_SAE    3                  /* SAE y luego el saludo (WPA3) */
static int modo;
static wpa_par_t par;                  /* lo negociado (cifrados, AKM, 802.11w) */
static sae_t sae;
static uint32_t iv_wep;
static uint8_t chave_wep[13], nchave_wep;
static uint64_t pn_rx_gestao, ipn_rx;  /* 802.11w: gestión protegida (CCMP) y de grupo (BIP) */
static bool saq_ativa;                 /* consulta SA en curso (desautenticación sin proteger) */
static uint16_t saq_id;
static uint32_t saq_ate, saq_reenvio;
static bool houve_falha_mic;           /* contramedidas de TKIP (12.8.2) */
static uint32_t falha_mic_em, tkip_bloqueado_ate;
static uint32_t gestao_hw;             /* tramas de gestión protegidas que el chip dice haber descifrado */

/* D238: rendimiento. */
static bool ajuste_ht = true, ajuste_filas = true;
static bool usar_ht;                   /* la asociación en curso anuncia HT y WMM */
static volatile bool resp_assoc_ht;    /* la respuesta de asociación trae HT Capabilities */
static uint8_t ht_vetados[4][6];       /* puntos de acceso con los que la HT falló (hasta reiniciar) */
static int n_vetados;
static uint32_t rx_bytes, tx_bytes, addba_recusados;
static uint32_t marca_rx_bytes, marca_tx_bytes, marca_em;   /* para el registro periódico */
/* Respuestas de gestión que espera la conexión. La autenticación guarda el
 * cuerpo entero (SAE y clave compartida lo necesitan). */
static volatile int resp_auth = -1, resp_assoc = -1;
static uint8_t auth_corpo[600];
static uint32_t nauth_corpo;
static uint16_t auth_alg, auth_seq;

/* ------------------------------------------------------------ colas con la pila IP */

typedef struct { uint16_t n; uint8_t q[ETH_MAX]; } quadro_eth_t;
static quadro_eth_t fila_tx[FILA], fila_rx[FILA_RX];
static uint32_t tx_ini, tx_qtd, rx_ini, rx_qtd;
static placa_t placa_wifi;

/* ------------------------------------------------------------ el chip (D280)
 * Una sola capa entre la pila 802.11 y los dos drivers. «chip» guarda lo que los dos
 * comparten (MAC, canal, tasa); lo propio de cada uno está en rtl8188eu.c y ar9485.c. */

static void anotar_erro(const char *s);

static void chip_sincronizar(void)
{
    if (!via_pci) return;
    const ar9485_t *a = ar9485_info();
    memcpy(chip.mac, a->mac, 6);
    chip.canal = a->canal;
    chip.taxa = a->taxa;
    chip.erros_usb = 0;
}

static int drv_enviar(r88e_t *c, const uint8_t *q, uint32_t n, int tipo, bool cifrar)
{
    if (!via_pci) return r88e_enviar(c, q, n, tipo, cifrar);
    int r = ar9485_enviar(q, n, tipo == R88E_TX_GESTAO ? AR9485_TX_GESTAO : tipo == R88E_TX_EAPOL ? AR9485_TX_EAPOL : AR9485_TX_DADOS,
                          cifrar);
    chip.taxa = ar9485_info()->taxa;
    return r;
}

static int drv_receber(r88e_t *c, r88e_rx_t cb)
{
    if (!via_pci) return r88e_receber(c, cb);
    int r = ar9485_receber(cb);
    ar9485_periodico();
    chip_sincronizar();
    return r;
}

static void drv_canal(r88e_t *c, uint8_t canal)
{
    if (!via_pci) { r88e_canal(c, canal); return; }
    ar9485_canal(canal);
    chip_sincronizar();
    if (!ar9485_info()->pronto && estado != WIFI_FALHOU) {   /* el reinicio falló: el registro dice por qué */
        estado = WIFI_FALHOU;
        anotar_erro("el chip dejó de responder al cambiar de canal (ver el registro [AR9485])");
    }
}

static void drv_modo_busca(r88e_t *c, bool b) { if (via_pci) ar9485_modo_busca(b); else r88e_modo_busca(c, b); }
static void drv_desassociado(r88e_t *c) { if (via_pci) ar9485_desassociado(); else r88e_desassociado(c); }
static void drv_limpar_chaves(r88e_t *c) { if (via_pci) ar9485_limpar_chaves(); else r88e_limpar_chaves(c); }
static void drv_rx_simples(r88e_t *c) { if (!via_pci) r88e_rx_simples(c); }
static void drv_em_voo(r88e_t *c, uint32_t *rx, uint32_t *tx) { if (via_pci) *rx = *tx = 0; else r88e_em_voo(c, rx, tx); }
static void drv_chave(r88e_t *c, int entrada, const uint8_t k[16], const uint8_t *mac, uint8_t keyid)
{
    if (via_pci) ar9485_chave(entrada, k, mac);
    else r88e_chave(c, entrada, k, mac, keyid);
}
static void drv_ht(r88e_t *c, bool ht, bool sgi, uint8_t mcs)
{
    if (via_pci) { c->ht = false; c->sgi = false; c->taxa = ar9485_info()->taxa; }
    else r88e_ht(c, ht, sgi, mcs);
}
/* El CCMP de los datos lo hace el chip (RTL8188EU siempre; AR9485 solo si se pide). */
static bool ccmp_por_hw(void) { return !via_pci || ar9485_info()->cifra_hw; }

static bool wl_enviar(const void *q, uint32_t n)
{
    if (estado != WIFI_CONECTADO || n < 14 || n > ETH_MAX) return false;
    bool i0 = int_salva_desliga();
    bool ok = tx_qtd < FILA;
    if (ok) {
        quadro_eth_t *f = &fila_tx[(tx_ini + tx_qtd) % FILA];
        memcpy(f->q, q, n);
        f->n = (uint16_t)n;
        tx_qtd++;
        acordar(&canal_tarefa);
    }
    int_restaura(i0);
    return ok;
}

static int wl_receber(void *destino)
{
    bool i0 = int_salva_desliga();
    int n = 0;
    if (rx_qtd) {
        quadro_eth_t *f = &fila_rx[rx_ini];
        memcpy(destino, f->q, f->n);
        n = f->n;
        rx_ini = (rx_ini + 1) % FILA_RX;
        rx_qtd--;
    }
    int_restaura(i0);
    return n;
}

static bool wl_cabo(void) { return estado == WIFI_CONECTADO; }
static bool wl_atender(void) { return false; }

static void entregar_eth(const uint8_t *q, uint32_t n)
{
    bool i0 = int_salva_desliga();
    if (rx_qtd < FILA_RX) {
        quadro_eth_t *f = &fila_rx[(rx_ini + rx_qtd) % FILA_RX];
        memcpy(f->q, q, n);
        f->n = (uint16_t)n;
        rx_qtd++;
        rx_bytes += n;
    }
    int_restaura(i0);
    rede_acordar();   /* D238: la pila lo procesa ya, no en su próximo tic */
}

/* ------------------------------------------------------------ utilidades */

static uint32_t agora(void) { return relogio_tiques(); }
static bool passou(uint32_t t) { return (int32_t)(agora() - t) >= 0; }

static void dormir_tiques(uint32_t t)
{
    processo_atual()->acordar_em = agora() + (t ? t : 1);
    bool i0 = int_salva_desliga();
    bloquear(&canal_tarefa);
    ceder();
    int_restaura(i0);
}

static void anotar_erro(const char *s)
{
    txt_copia_n(erro, s, sizeof erro);
    klog("[WIFI] %s\n", s);
}

static int qualidade(int dbm)
{
    if (dbm <= -95) return 0;
    if (dbm >= -45) return 100;
    return (dbm + 95) * 2;
}

/* Las MAC ajenas no van al registro tal cual (D215): solo los tres últimos bytes. */
static void ofuscar(char *s, int max, const uint8_t *m)
{
    snformatar(s, (size_t)max, "xx:xx:xx:%02x:%02x:%02x", m[3], m[4], m[5]);
}

/* ------------------------------------------------------------ tramas de gestión */

static uint8_t tq[2400];   /* la trama en montaje (solo la tarea la usa) */

static uint32_t cab_gestao(uint8_t subtipo, const uint8_t *destino, const uint8_t *bssid)
{
    memset(tq, 0, 24);
    tq[0] = (uint8_t)(subtipo << 4);
    memcpy(tq + 4, destino, 6);
    memcpy(tq + 10, chip.mac, 6);
    memcpy(tq + 16, bssid, 6);
    return 24;
}

static uint32_t ies_taxas(uint32_t k)
{
    static const uint8_t t[] = { 1, 8, 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24,
                                 50, 4, 0x30, 0x48, 0x60, 0x6c };
    memcpy(tq + k, t, sizeof t);
    return k + sizeof t;
}

static void enviar_sondeo(const uint8_t *ssid, uint8_t nssid)
{
    static const uint8_t todos[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    uint32_t k = cab_gestao(4, todos, todos);
    tq[k++] = 0;
    tq[k++] = nssid;
    memcpy(tq + k, ssid, nssid);
    k += nssid;
    k = ies_taxas(k);
    tq[k++] = 3; tq[k++] = 1; tq[k++] = chip.canal;
    if (drv_enviar(&chip, tq, k, R88E_TX_GESTAO, false) == 0) tx_quadros++;
}

/* Autenticación: algoritmo (0 abierta, 1 clave compartida, 3 SAE), paso y
 * estado, y el resto del cuerpo (lo de SAE o el reto de la clave compartida). */
static void enviar_auth(uint16_t alg, uint16_t seq, uint16_t st, const uint8_t *resto, uint32_t nresto)
{
    uint32_t k = cab_gestao(11, alvo.bssid, alvo.bssid);
    tq[k++] = (uint8_t)alg; tq[k++] = (uint8_t)(alg >> 8);
    tq[k++] = (uint8_t)seq; tq[k++] = (uint8_t)(seq >> 8);
    tq[k++] = (uint8_t)st; tq[k++] = (uint8_t)(st >> 8);
    if (nresto) { memcpy(tq + k, resto, nresto); k += nresto; }
    if (drv_enviar(&chip, tq, k, R88E_TX_GESTAO, false) == 0) tx_quadros++;
}

static void enviar_assoc(void)
{
    uint32_t k = cab_gestao(0, alvo.bssid, alvo.bssid);
    uint16_t cap = 0x0001;                         /* ESS */
    if (alvo.capacidade & 0x0020) cap |= 0x0020;   /* preámbulo corto */
    if (alvo.capacidade & 0x0400) cap |= 0x0400;   /* ranura corta */
    if (modo != MODO_ABERTA) cap |= 0x0010;        /* privacidad */
    tq[k++] = (uint8_t)cap; tq[k++] = (uint8_t)(cap >> 8);
    tq[k++] = 10; tq[k++] = 0;                     /* intervalo de escucha */
    tq[k++] = 0;
    tq[k++] = alvo.i.nssid;
    memcpy(tq + k, alvo.i.ssid, alvo.i.nssid);
    k += alvo.i.nssid;
    k = ies_taxas(k);
    if (modo >= MODO_WPA) k += wpa_montar_ie(&par, tq + k);   /* RSN (o WPA) y, con H2E, el RSNXE */
    if (usar_ht) {
        /* D238: 802.11n. Las HT Capabilities y el WMM (una estación HT es de QoS:
         * sin WMM, hostapd y la mayoría de los routers la asocian como b/g). */
        k += wn_ie_ht_cap(tq + k);
        k += wn_ie_wmm(tq + k);
    }
    if (drv_enviar(&chip, tq, k, R88E_TX_GESTAO, false) == 0) tx_quadros++;
}

/* Con la 802.11w activa, las tramas de gestión de unidifusión (desautenticación,
 * desasociación, acción) van protegidas con CCMP y la clave de par (IEEE 802.11-2020, 12.2.8):
 * se cifran aquí, el chip las manda tal cual. */
static bool pmf_ativa(void) { return chaves_ok && par.pmf && modo >= MODO_WPA; }

static void enviar_gestao(uint32_t k)
{
    if (pmf_ativa()) k = ccmp_cifrar(tq, 24, k, sup.ptk + 32, ++pn_tx, 0);
    if (drv_enviar(&chip, tq, k, R88E_TX_GESTAO, false) == 0) tx_quadros++;
}

static void enviar_deauth(uint16_t motivo)
{
    uint32_t k = cab_gestao(12, alvo.bssid, alvo.bssid);
    tq[k++] = (uint8_t)motivo; tq[k++] = (uint8_t)(motivo >> 8);
    enviar_gestao(k);
}

/* Consulta SA (802.11w, 11.13): acción de la categoría 8. */
static void enviar_sa_query(uint8_t acao, uint16_t id)
{
    uint32_t k = cab_gestao(13, alvo.bssid, alvo.bssid);
    tq[k++] = 8;
    tq[k++] = acao;                                /* 0 = petición, 1 = respuesta */
    tq[k++] = (uint8_t)id; tq[k++] = (uint8_t)(id >> 8);
    enviar_gestao(k);
}

/* ------------------------------------------------------------ 802.11n (D238) */

static bool ht_vetado(const uint8_t *bssid)
{
    for (int i = 0; i < n_vetados; i++) if (!memcmp(ht_vetados[i], bssid, 6)) return true;
    return false;
}

static void ht_vetar(const uint8_t *bssid, const char *motivo)
{
    if (ht_vetado(bssid)) return;
    memcpy(ht_vetados[n_vetados < 4 ? n_vetados++ : 3], bssid, 6);
    klog("[WIFI] 802.11n: %s; con esta red, b/g hasta reiniciar\n", motivo);
}

/* La HT solo con CCMP o sin cifrado: la 802.11n prohíbe las tasas HT con TKIP
 * y WEP (IEEE 802.11-2020, 12.5.2.1 y 12.5.3.1) y hostapd rechaza la asociación
 * HT de una estación TKIP. */
static bool ht_possivel(const bss_t *b)
{
    if (via_pci) return false;                              /* D280: el AR9485 se asocia como b/g */
    if (!ajuste_ht || !b->n.ht || !b->n.wmm || ht_vetado(b->bssid)) return false;
    if (modo == MODO_ABERTA) return true;
    return modo >= MODO_WPA && par.cifra_par == WPA_CIFRA_CCMP;
}

/* Lo lee la tarea antes de arrancar el chip y de cada conexión: «ht 0» y
 * «filas 0» (una por línea) apagan la 802.11n y las colas USB. Sin el archivo,
 * todo encendido. */
static void ler_ajustes(void)
{
    uint8_t *txt;
    uint32_t tam;
    ajuste_ht = ajuste_filas = true;
    ajuste_cifra_hw = false;
    if (!vfs_existe_arquivo(AJUSTES) || vfs_ler_inteiro(AJUSTES, &txt, &tam) < 0) return;
    for (uint32_t i = 0; i < tam;) {
        uint32_t f = i;
        while (f < tam && txt[f] != '\n') f++;
        const char *l = (const char *)txt + i;
        uint32_t nl = f - i;
        while (nl && (l[nl - 1] == '\r' || l[nl - 1] == ' ')) nl--;
        if (nl == 4 && !memcmp(l, "ht 0", 4)) ajuste_ht = false;
        else if (nl == 7 && !memcmp(l, "filas 0", 7)) ajuste_filas = false;
        else if (nl == 10 && !memcmp(l, "cifrado hw", 10)) ajuste_cifra_hw = true;   /* D280: CCMP del AR9485 por hardware */
        i = f + 1;
    }
    kfree(txt);
}

/* El punto de acceso pide un acuerdo de Block Ack para mandarnos A-MPDU: se
 * rechaza con el estado 37 (como hace Linux cuando no puede), repitiendo el
 * token y los parámetros, para que no se quede esperando ni reintentando.
 * Aceptarlo pediría un búfer de reordenación por TID antes de la comprobación
 * de repetición del CCMP (las MPDU reenviadas llegan desordenadas). */
static void recusar_addba(const uint8_t *b, uint32_t nb)
{
    uint8_t r[WN_ADDBA_RESP_TAM];
    if (estado < WIFI_CONECTANDO || estado > WIFI_CONECTADO) return;
    if (!wn_addba_resposta(b, nb, WN_ESTADO_RECUSADO, r)) return;
    uint32_t k = cab_gestao(13, alvo.bssid, alvo.bssid);
    memcpy(tq + k, r, sizeof r);
    k += sizeof r;
    enviar_gestao(k);
    if (addba_recusados++ == 0)
        klog("[WIFI] el punto de acceso pidió Block Ack (TID %u): se rechaza; sin A-MPDU\n", (unsigned)((b[3] >> 2) & 0x0F));
}

/* ------------------------------------------------------------ datos */

/* Ethernet (destino, origen, tipo, carga) -> 802.11 hacia el punto de acceso. */
static void enviar_dados(const uint8_t *eth, uint32_t n, bool eapol)
{
    if (n < 14 || n > ETH_MAX) return;
    /* El CCMP lo hace el chip (el RTL8188EU siempre; el AR9485 solo con «cifrado hw»,
     * D280); si no, y TKIP y WEP, el software (D220). */
    uint8_t cifra = modo == MODO_WEP ? WPA_CIFRA_WEP40 : par.cifra_par;
    bool ccmp = chaves_ok && cifra == WPA_CIFRA_CCMP;
    bool cifrar = ccmp && ccmp_por_hw();
    bool ccmp_soft = ccmp && !cifrar;
    bool soft = chaves_ok && cifra != WPA_CIFRA_CCMP;
    memset(tq, 0, 24);
    tq[0] = 0x08;                       /* datos */
    tq[1] = (uint8_t)(0x01 | (cifrar ? 0x40 : 0));   /* hacia el sistema de distribución (ccmp_cifrar pone el bit de protegida) */
    memcpy(tq + 4, alvo.bssid, 6);
    memcpy(tq + 10, chip.mac, 6);
    memcpy(tq + 16, eth, 6);
    uint32_t k = 24;
    if (cifrar) {
        pn_tx++;
        tq[k++] = (uint8_t)pn_tx;
        tq[k++] = (uint8_t)(pn_tx >> 8);
        tq[k++] = 0;
        tq[k++] = 0x20;                 /* ExtIV, clave 0 (la de par) */
        for (int i = 2; i < 6; i++) tq[k++] = (uint8_t)(pn_tx >> (8 * i));
    }
    static const uint8_t llc[6] = { 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00 };
    memcpy(tq + k, llc, 6);
    k += 6;
    memcpy(tq + k, eth + 12, n - 12);   /* tipo y carga */
    k += n - 12;
    if (ccmp_soft) k = ccmp_cifrar(tq, 24, k, sup.ptk + 32, ++pn_tx, 0);
    else if (soft && modo == MODO_WEP) k = wep_cifrar(tq, 24, k, chave_wep, nchave_wep, iv_wep++ & 0xFFFFFF, 0);
    else if (soft) k = tkip_cifrar(tq, 24, k, sup.ptk + 32, sup.ptk + 56, ++pn_tx, 0);   /* Michael de la estación */
    if (drv_enviar(&chip, tq, k, eapol ? R88E_TX_EAPOL : R88E_TX_DADOS, cifrar) == 0) tx_quadros++;
}

static void enviar_eapol(const uint8_t *e, uint32_t n)
{
    static uint8_t eth[ETH_MAX];
    if (n + 14 > sizeof eth) return;
    memcpy(eth, alvo.bssid, 6);
    memcpy(eth + 6, chip.mac, 6);
    eth[12] = 0x88; eth[13] = 0x8E;
    memcpy(eth + 14, e, n);
    enviar_dados(eth, n + 14, true);
}

static void ficou_conectado(void);
static void desconectar(const char *motivo, bool avisar_ap);

/* Solo el CCMP va a la CAM del chip; las claves de TKIP se quedan en «sup» y
 * las usa el software (D220). La IGTK (802.11w) también es del software. */
static void instalar_chaves(bool de_par)
{
    if (de_par && par.cifra_par == WPA_CIFRA_CCMP) drv_chave(&chip, 4, sup.ptk + 32, alvo.bssid, 0);
    if (sup.ngtk == 16 && par.cifra_grupo == WPA_CIFRA_CCMP)
        drv_chave(&chip, sup.gtk_id, sup.gtk, (const uint8_t *)0, sup.gtk_id);
    if (sup.ngtk) {
        pn_rx_grupo = (uint64_t)sup.rsc[0] | (uint64_t)sup.rsc[1] << 8 | (uint64_t)sup.rsc[2] << 16 |
                      (uint64_t)sup.rsc[3] << 24 | (uint64_t)sup.rsc[4] << 32 | (uint64_t)sup.rsc[5] << 40;
        if (pn_rx_grupo) pn_rx_grupo--;
    }
    if (sup.tem_igtk) {
        ipn_rx = 0;
        for (int i = 0; i < 6; i++) ipn_rx |= (uint64_t)sup.ipn[i] << (8 * i);
        if (ipn_rx) ipn_rx--;
    }
}

static void eapol_chegou(const uint8_t *e, uint32_t n)
{
    static uint8_t resp[WPA_EAPOL_MAX];
    uint32_t nr = 0;
    if (modo < MODO_WPA) return;
    int r = wpa_processar(&sup, e, n, resp, &nr);
    switch (r) {
    case WPA_RESPONDER:
        if (!saudacao_desde) saudacao_desde = agora();
        if (sup.tem_pmkid && sup.pmkid_visto && !sup.pmkid_ok)
            klog("[WIFI] el PMKID del mensaje 1 no es el del SAE (se sigue: el MIC dirá si la PMK es la misma)\n");
        enviar_eapol(resp, nr);
        break;
    case WPA_CHAVES:
        enviar_eapol(resp, nr);
        instalar_chaves(true);
        chaves_ok = true;
        if (par.pmf && !sup.tem_igtk) klog("[WIFI] 802.11w sin IGTK: las desautenticaciones de difusión se ignoran\n");
        /* WPA1: la GTK llega en un saludo de grupo aparte; hasta entonces no hay difusión */
        if (sup.ngtk) ficou_conectado();
        break;
    case WPA_GTK_NOVA:
        enviar_eapol(resp, nr);
        instalar_chaves(false);
        klog("[WIFI] clave de grupo renovada (id %u%s)\n", sup.gtk_id, sup.tem_igtk ? ", con IGTK" : "");
        if (estado == WIFI_SAUDACAO && chaves_ok) ficou_conectado();
        break;
    case WPA_ERRO_MIC:
        klog("[WIFI] EAPOL con MIC que no cuadra: descartado\n");
        break;
    case WPA_ERRO:
        klog("[WIFI] EAPOL no valido o repetido: descartado\n");
        break;
    }
}

/* Un fallo de Michael (TKIP) con el ICV bien: se avisa al punto de acceso y,
 * si es el segundo en 60 s, contramedidas (12.8.2): desautenticación con el
 * motivo 14 y 60 s sin conectarse a redes con TKIP. */
static void michael_falhou(bool de_par)
{
    static uint8_t resp[WPA_EAPOL_MAX];
    klog("[WIFI] fallo de Michael (TKIP, clave %s): se avisa al punto de acceso\n", de_par ? "de par" : "de grupo");
    uint32_t nr = wpa_falha_mic(&sup, de_par, resp);
    if (nr) enviar_eapol(resp, nr);
    bool segundo = houve_falha_mic && !passou(falha_mic_em + 60 * TIQUES_POR_SEGUNDO);
    houve_falha_mic = true;
    falha_mic_em = agora();
    if (segundo) {
        tkip_bloqueado_ate = agora() + 60 * TIQUES_POR_SEGUNDO;
        if (!tkip_bloqueado_ate) tkip_bloqueado_ate = 1;
        enviar_deauth(14);   /* motivo 14: fallo de MIC */
        desconectar("contramedidas de TKIP: dos fallos de Michael en 60 s; 60 s sin conectarse con TKIP", false);
    }
}

/* Un MSDU (LLC/SNAP, tipo y carga) -> Ethernet, con su destino y su origen. */
static void entregar_msdu(const uint8_t *destino, const uint8_t *origem, const uint8_t *c, uint32_t nc, bool protegida,
                          bool eapol_ok)
{
    if (nc < 8 || c[0] != 0xAA || c[1] != 0xAA || c[2] != 0x03) return;
    uint16_t tipo = (uint16_t)(c[6] << 8 | c[7]);
    if (tipo == 0x888E) { if (eapol_ok) eapol_chegou(c + 8, nc - 8); return; }
    if (modo != MODO_ABERTA && (!chaves_ok || !protegida)) return;   /* el puerto no está abierto */
    static uint8_t eth[ETH_MAX];
    uint32_t carga = nc - 8;
    if (carga + 14 > ETH_MAX) return;
    memcpy(eth, destino, 6);
    memcpy(eth + 6, origem, 6);
    eth[12] = (uint8_t)(tipo >> 8); eth[13] = (uint8_t)tipo;
    memcpy(eth + 14, c + 8, carga);
    entregar_eth(eth, carga + 14);
}

/* D238: A-MSDU (IEEE 802.11-2020, 9.3.2.2.2). Una estación HT tiene que saber
 * recibirlo (anunciamos 3839 bytes) aunque no haya Block Ack: varios MSDU en
 * una trama, cada uno con destino, origen y longitud (2 bytes, big endian)
 * delante y rellenado a 4 bytes. Como Linux contra el ataque de agregación
 * (FragAttacks, CVE-2020-24588), se tira el A-MSDU cuyo primer destino es una
 * cabecera LLC/SNAP: es una trama normal con el bit de A-MSDU cambiado. El
 * EAPOL dentro de un A-MSDU no se acepta. */
static void amsdu_chegou(const uint8_t *c, uint32_t nc, bool protegida)
{
    static const uint8_t snap[6] = { 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00 };
    if (nc >= 6 && !memcmp(c, snap, 6)) return;
    uint32_t i = 0;
    while (i + 14 <= nc) {
        uint32_t l = (uint32_t)(c[i + 12] << 8 | c[i + 13]);
        if (i + 14 + l > nc) return;
        entregar_msdu(c + i, c + i + 6, c + i + 14, l, protegida, false);
        i += (14 + l + 3) & ~3u;
    }
}

/* Trama de datos del punto de acceso: 802.11 -> Ethernet. */
static void dados_chegou(const uint8_t *q, uint32_t n, bool decifrada)
{
    if ((q[1] & 0x03) != 0x02) return;                      /* solo desde el sistema de distribución */
    if (memcmp(q + 10, alvo.bssid, 6) != 0) return;
    if (q[0] & 0x40) return;                                /* sin datos (nulos) */
    if (!memcmp(q + 16, chip.mac, 6)) return;               /* nuestra propia difusión, devuelta por el AP */
    uint32_t cab = 24;
    bool qos = (q[0] & 0x80) != 0;
    int tid = 16;
    if (qos) { tid = q[24] & 0x0F; cab += 2; if (q[1] & 0x80) cab += 4; }
    if (n < cab + 8) return;
    const uint8_t *c = q + cab;
    uint32_t nc = n - cab;
    bool protegida = (q[1] & 0x40) != 0;
    bool grupo = (q[4] & 1) != 0;
    uint8_t cifra = modo == MODO_WEP ? WPA_CIFRA_WEP40 : grupo ? par.cifra_grupo : par.cifra_par;
    if (protegida && cifra == WPA_CIFRA_CCMP) {
        /* Como el urtwn de OpenBSD, se confía en el descifrado del chip (con
         * las claves en la CAM): una trama con MIC malo llega con ICVERR y
         * rtl8188eu.c ya la tiró. El bit 27 del descriptor (SWDEC en el
         * fabricante: «hay que descifrar por software») solo se cuenta. */
        if (!decifrada && via_pci) {
            /* D280: el AR9485 trae la trama cifrada (el descifrado por hardware va apagado): CCMP
             * por software, con la clave de par o la de grupo, en una copia. */
            static uint8_t cq[2400];
            if (!chaves_ok || n > sizeof cq || nc < 16 || !(c[3] & 0x20)) return;
            if (grupo && (sup.ngtk != 16 || (c[3] >> 6) != sup.gtk_id)) return;
            memcpy(cq, q, n);
            uint64_t pn;
            uint32_t claro;
            if (ccmp_decifrar(cq, cab, n, grupo ? sup.gtk : sup.ptk + 32, &pn, &claro) != CIFRA_OK) return;
            uint64_t *ult = grupo ? &pn_rx_grupo : &pn_rx_par[tid];
            if (pn <= *ult) return;                         /* repetida */
            *ult = pn;
            c = cq + cab + 8;
            nc = claro;
        } else {
            if (!decifrada) swdec++;
            if (!chaves_ok || nc < 16 || !(c[3] & 0x20)) return;
            uint64_t pn = ccmp_pn(c);
            uint64_t *ult = grupo ? &pn_rx_grupo : &pn_rx_par[tid];
            if (pn <= *ult) return;                         /* repetida */
            *ult = pn;
            c += 8;
            nc -= 16;                                       /* cabecera CCMP y MIC */
        }
    } else if (protegida) {
        /* TKIP y WEP: el chip no tiene la clave (no va a la CAM) y la trama
         * llega cifrada; se descifra aquí (D220), en una copia. */
        static uint8_t rq[2400];
        if (!chaves_ok || n > sizeof rq) return;
        memcpy(rq, q, n);
        uint32_t claro = 0;
        if (cifra == WPA_CIFRA_WEP40) {
            if (wep_decifrar(rq, cab, n, chave_wep, nchave_wep, &claro) != CIFRA_OK) return;
            c = rq + cab + 4;
        } else if (cifra == WPA_CIFRA_TKIP) {
            if (nc < 8 + 12 || !(c[3] & 0x20)) return;
            if (grupo && (c[3] >> 6) != sup.gtk_id) return;   /* otra clave de grupo (la vieja) */
            if (grupo && sup.ngtk != 32) return;
            uint64_t tsc = tkip_tsc(c);
            uint64_t *ult = grupo ? &pn_rx_grupo : &pn_rx_par[tid];
            if (tsc <= *ult) return;                         /* repetida: antes de Michael */
            const uint8_t *tk = grupo ? sup.gtk : sup.ptk + 32;
            const uint8_t *mic = grupo ? sup.gtk + 16 : sup.ptk + 48;   /* Michael del autenticador */
            int r = tkip_decifrar(rq, cab, n, tk, mic, &claro);
            if (r == CIFRA_ERRO_MIC) { michael_falhou(!grupo); return; }
            if (r != CIFRA_OK) return;
            *ult = tsc;
            c = rq + cab + 8;
        } else return;
        nc = claro;
    }
    if (qos && (q[24] & 0x80)) { amsdu_chegou(c, nc, protegida); return; }   /* D238 */
    entregar_msdu(q + 4, q + 16, c, nc, protegida, true);
}

/* ------------------------------------------------------------ recepción */

static void guardar_bss(const uint8_t *q, uint32_t n, int rssi)
{
    if (n < 36) return;
    const uint8_t *b = q + 24;
    bss_t novo;
    memset(&novo, 0, sizeof novo);
    memcpy(novo.bssid, q + 16, 6);
    novo.intervalo = (uint16_t)(b[8] | b[9] << 8);
    novo.capacidade = (uint16_t)(b[10] | b[11] << 8);
    if (!(novo.capacidade & 1)) return;                     /* no es un punto de acceso (ad hoc) */
    wpa_analisar_ies(b + 12, n - 36, (novo.capacidade & 0x10) != 0, &novo.i);
    wn_analisar_ies(b + 12, n - 36, &novo.n);
    novo.rssi = (int8_t)rssi;
    novo.canal_rx = chip.canal;
    if (!novo.i.canal) novo.i.canal = chip.canal;
    if (novo.i.canal != chip.canal) return;                 /* eco de un canal vecino */
    novo.visto = agora();
    int k = -1;
    for (int i = 0; i < nredes; i++) if (!memcmp(redes[i].bssid, novo.bssid, 6)) { k = i; break; }
    if (k < 0) {
        if (nredes < MAX_REDES) k = nredes++;
        else {
            k = 0;
            for (int i = 1; i < nredes; i++) if (redes[i].rssi < redes[k].rssi) k = i;
            if (redes[k].rssi >= novo.rssi) return;
        }
    } else if (!novo.i.nssid && redes[k].i.nssid) {         /* la baliza oculta no borra el nombre del sondeo */
        novo.i.nssid = redes[k].i.nssid;
        memcpy(novo.i.ssid, redes[k].i.ssid, novo.i.nssid);
    }
    redes[k] = novo;
}

/* Trama de gestión protegida (802.11w): CCMP por software con la clave de
 * par y el contador de gestión aparte. Si el chip dice que ya la descifró y
 * el software no puede, solo se cuenta (para verlo con el pincho real). */
static bool abrir_gestao(const uint8_t *q, uint32_t n, bool decifrada, const uint8_t **corpo, uint32_t *nc)
{
    static uint8_t g[2400];
    if (n > sizeof g || n < 24 + 16) return false;
    memcpy(g, q, n);
    uint64_t pn;
    uint32_t claro;
    if (ccmp_decifrar(g, 24, n, sup.ptk + 32, &pn, &claro) != CIFRA_OK) {
        if (decifrada) gestao_hw++;
        return false;
    }
    if (pn <= pn_rx_gestao) return false;                  /* repetida */
    pn_rx_gestao = pn;
    *corpo = g + 24 + 8;
    *nc = claro;
    return true;
}

static void iniciar_sa_query(void)
{
    if (saq_ativa || estado != WIFI_CONECTADO) return;
    aleatorio_bytes(&saq_id, sizeof saq_id);
    saq_ativa = true;
    saq_ate = agora() + TIQUES_POR_SEGUNDO;
    saq_reenvio = agora() + TIQUES_POR_SEGUNDO / 3;
    klog("[WIFI] desautenticación sin proteger con 802.11w: consulta SA\n");
    enviar_sa_query(0, saq_id);
}

static void chegou(const uint8_t *q, uint32_t n, int rssi, bool decifrada)
{
    rx_quadros++;
    if (n < 24) return;
    uint8_t tipo = q[0] & 0x0C, sub = q[0] >> 4;
    if (tipo == 0x08) {
        bool associado = estado == WIFI_SAUDACAO || estado == WIFI_CONECTADO || (estado == WIFI_CONECTANDO && resp_assoc == 0);
        if (associado && (!memcmp(q + 4, chip.mac, 6) || (q[4] & 1))) dados_chegou(q, n, decifrada);
        return;
    }
    if (tipo != 0x00) return;
    if (sub == 8 || sub == 5) {                             /* baliza o respuesta de sondeo */
        if (sub == 5 && !memcmp(q + 4, chip.mac, 6)) respostas_sondeo++;
        if (buscando) guardar_bss(q, n, rssi);
        if (estado >= WIFI_CONECTANDO && estado <= WIFI_CONECTADO && !memcmp(q + 16, alvo.bssid, 6)) {
            ultima_baliza = agora();
            balizas++;
            sinal = (sinal * 7 + rssi) / 8;
        }
        return;
    }
    bool robusta = sub == 12 || sub == 10 || sub == 13;     /* desautenticación, desasociación, acción */
    bool ligado = estado >= WIFI_CONECTANDO && estado <= WIFI_CONECTADO;
    if (robusta && (q[4] & 1)) {
        /* De difusión: solo cuenta con la 802.11w y el BIP correcto (D220);
         * sin ella se ignora, como siempre (la pérdida de balizas lo nota). */
        uint64_t ipn;
        if (sub != 13 && ligado && pmf_ativa() && sup.tem_igtk && !memcmp(q + 16, alvo.bssid, 6) && n >= 26 &&
            bip_verificar(q, n, sup.igtk, sup.igtk_id, &ipn) && ipn > ipn_rx) {
            ipn_rx = ipn;
            char m[80];
            snformatar(m, sizeof m, "el punto de acceso cortó la conexión (motivo %u, difusión con BIP)", (unsigned)(q[24] | q[25] << 8));
            desconectar(m, false);
        }
        return;
    }
    if (memcmp(q + 4, chip.mac, 6) != 0 || memcmp(q + 16, alvo.bssid, 6) != 0) return;
    const uint8_t *b = q + 24;
    uint32_t nb = n - 24;
    if (robusta && pmf_ativa()) {
        if (!(q[1] & 0x40)) {
            /* Sin proteger con la 802.11w activa: puede ser falsa. Una
             * desautenticación abre la consulta SA (11.13): si el punto de
             * acceso no contesta protegido en 1 s, se da por buena. */
            if (sub != 13) iniciar_sa_query();
            return;
        }
        if (!abrir_gestao(q, n, decifrada, &b, &nb)) return;
    } else if (q[1] & 0x40) return;                       /* protegida sin 802.11w: no es para nosotros */
    if (sub == 11 && nb >= 6) {                             /* autenticación (abierta, clave compartida o SAE) */
        uint16_t st = (uint16_t)(b[4] | b[5] << 8);
        nauth_corpo = nb < sizeof auth_corpo ? nb : sizeof auth_corpo;
        memcpy(auth_corpo, b, nauth_corpo);
        auth_alg = (uint16_t)(b[0] | b[1] << 8);
        auth_seq = (uint16_t)(b[2] | b[3] << 8);
        resp_auth = st;
    } else if (sub == 13 && nb >= 4 && b[0] == 8) {        /* consulta SA (solo protegida) */
        uint16_t id = (uint16_t)(b[2] | b[3] << 8);
        if (b[1] == 0) enviar_sa_query(1, id);
        else if (b[1] == 1 && saq_ativa && id == saq_id) {
            saq_ativa = false;
            klog("[WIFI] consulta SA respondida: la desautenticación sin proteger era falsa\n");
        }
    } else if (sub == 13 && nb >= 2 && b[0] == WN_CAT_BLOCK_ACK) {   /* D238: Block Ack */
        if (b[1] == WN_ADDBA_PEDIDO) recusar_addba(b, nb);
        /* DELBA y lo demás: no hay acuerdos que deshacer */
    } else if ((sub == 1 || sub == 3) && nb >= 6) {        /* respuesta de asociación */
        uint16_t st = (uint16_t)(b[2] | b[3] << 8);
        aid = (uint16_t)((b[4] | b[5] << 8) & 0x3FFF);
        wn_ap_t ra;
        wn_analisar_ies(b + 6, nb - 6, &ra);
        resp_assoc_ht = ra.ht;                              /* D238: el punto de acceso nos acepta como HT */
        resp_assoc = st;
    } else if (sub == 12 || sub == 10) {                    /* desautenticación o desasociación */
        if (estado >= WIFI_CONECTANDO && estado <= WIFI_CONECTADO) {
            char m[80];
            snformatar(m, sizeof m, "el punto de acceso cortó la conexión (motivo %u)", nb >= 2 ? (unsigned)(b[0] | b[1] << 8) : 0u);
            desconectar(m, false);
        }
    }
}

/* Recibe durante unos tiques (o hasta que «parar» se cumpla). */
static void escutar(uint32_t tiques, volatile int *parar)
{
    uint32_t fim = agora() + tiques;
    while (!passou(fim) && chip.presente) {
        int r = drv_receber(&chip, chegou);
        if (parar && *parar >= 0) return;
        if (r <= 0) dormir_tiques(1);
    }
}

/* ------------------------------------------------------------ búsqueda */

static void buscar(const pedido_wifi_t *oculta)
{
    bool conectado = estado == WIFI_CONECTADO;
    if (conectado) return;   /* conectado: la lista se queda como estaba (no se deja la red) */
    uint32_t transf0 = chip.rx_transf;
    buscando = true;
    nredes = 0;
    drv_modo_busca(&chip, true);
    for (uint8_t c = 1; c <= 13 && chip.presente; c++) {
        drv_canal(&chip, c);
        enviar_sondeo((const uint8_t *)"", 0);
        if (oculta && oculta->ssid[0]) enviar_sondeo((const uint8_t *)oculta->ssid, (uint8_t)txt_tamanho(oculta->ssid));
        escutar(TIQUES_POR_SEGUNDO / 5, (volatile int *)0);
    }
    drv_modo_busca(&chip, false);
    buscando = false;
    buscas++;
    klog("[WIFI] busqueda: %d redes\n", nredes);
    if (chip.rx_fila && chip.rx_transf == transf0 && chip.presente) {
        /* D238: 13 canales sin una sola transferencia de recepción: la cola no
         * funciona con este controlador. Se vuelve a una lectura cada vez y se
         * repite la búsqueda. */
        klog("[WIFI] la cola de recepción no trajo nada en la búsqueda: una lectura cada vez (D215)\n");
        drv_rx_simples(&chip);
        buscar(oculta);
    }
}

/* ------------------------------------------------------------ conexión */

static bool suportada(const bss_t *b)
{
    wpa_par_t p;
    return b->i.seguranca == WPA_SEG_ABERTA || b->i.seguranca == WPA_SEG_WEP || wpa_escolher(&b->i, true, true, &p);
}

/* El PWE de SAE (cazar y picotear) de este punto de acceso, si se trajo. */
static const uint8_t *pwe_de(const pedido_wifi_t *p, const uint8_t bssid[6])
{
    for (int i = 0; i < p->npwe && i < WIFI_MAX_PWE; i++)
        if (!memcmp(p->pwe[i].bssid, bssid, 6)) return p->pwe[i].pwe;
    return (const uint8_t *)0;
}

/* ¿Hay con qué hacer SAE con este punto de acceso? *h2e = con hash-to-element. */
static bool sae_possivel(const bss_t *b, const pedido_wifi_t *p, bool *h2e)
{
    *h2e = false;
    if (b->i.h2e && p->tem_pt) { *h2e = true; return true; }
    return !b->i.so_h2e && pwe_de(p, b->bssid) != (const uint8_t *)0;
}

/* Las credenciales sirven para la red (también para las recordadas). */
static bool credenciais_servem(const bss_t *b, const pedido_wifi_t *p)
{
    if (b->i.seguranca == WPA_SEG_ABERTA) return true;
    if (b->i.seguranca == WPA_SEG_WEP) return p->nwep == 5 || p->nwep == 13;
    bool h2e;
    wpa_par_t x;
    return wpa_escolher(&b->i, sae_possivel(b, p, &h2e), p->tem_pmk != 0, &x);
}

static int melhor(const char *ssid)
{
    int k = -1, n = (int)txt_tamanho(ssid);
    for (int i = 0; i < nredes; i++)
        if (redes[i].i.nssid == n && !memcmp(redes[i].i.ssid, ssid, (uint32_t)n) && (k < 0 || redes[i].rssi > redes[k].rssi))
            k = i;
    return k;
}

static const char *nome_modo(void)
{
    if (modo == MODO_ABERTA) return "abierta";
    if (modo == MODO_WEP) return nchave_wep == 13 ? "WEP de 104 bits: INSEGURA" : "WEP de 40 bits: INSEGURA";
    if (modo == MODO_SAE) return par.h2e ? "WPA3-SAE (H2E) CCMP, 802.11w" : "WPA3-SAE CCMP, 802.11w";
    if (par.wpa1) return par.cifra_par == WPA_CIFRA_TKIP ? "WPA-PSK TKIP" : "WPA-PSK CCMP";
    if (par.cifra_par == WPA_CIFRA_TKIP) return "WPA2-PSK TKIP";
    if (par.cifra_grupo == WPA_CIFRA_TKIP) return "WPA2-PSK CCMP, grupo TKIP";
    if (par.akm == 6) return "WPA2-PSK-SHA256 CCMP, 802.11w";
    return par.pmf ? "WPA2-PSK CCMP, 802.11w" : "WPA2-PSK CCMP";
}

static void ficou_conectado(void)
{
    estado = WIFI_CONECTADO;
    conectado_em = agora();
    ultima_baliza = agora();
    erro[0] = 0;
    char m[24];
    ofuscar(m, sizeof m, alvo.bssid);
    klog("[WIFI] conectado (%s, canal %u, %s)\n", m, alvo.i.canal, nome_modo());
    if (modo == MODO_WEP) klog("[WIFI] AVISO: WEP no protege: cualquiera cerca puede leer y falsificar el tráfico\n");
    if (modo >= MODO_WPA && (par.cifra_par == WPA_CIFRA_TKIP || par.cifra_grupo == WPA_CIFRA_TKIP))
        klog("[WIFI] AVISO: TKIP está obsoleto; mejor WPA2-AES o WPA3 en el router\n");
    swdec = 0;
    gestao_hw = 0;
    rede_wifi_conectada(true);
}

static void desconectar(const char *motivo, bool avisar_ap)
{
    bool estava = estado == WIFI_CONECTADO;
    if (avisar_ap && estado >= WIFI_CONECTANDO && estado <= WIFI_CONECTADO) enviar_deauth(3);   /* la estación se va */
    if (estado >= WIFI_CONECTANDO && estado <= WIFI_CONECTADO) drv_desassociado(&chip);
    chaves_ok = false;
    saq_ativa = false;
    wpa_apagar(&sup, sizeof sup);
    wpa_apagar(&sae, sizeof sae);
    wpa_apagar(chave_wep, sizeof chave_wep);
    nchave_wep = 0;
    if (chip.presente && estado != WIFI_FALHOU && estado != WIFI_SEM_FIRMWARE) estado = WIFI_PRONTO;
    if (motivo) anotar_erro(motivo);
    if (estava) rede_wifi_conectada(false);
    if (estava && swdec) klog("[WIFI] %u tramas cifradas llegaron con SWDEC\n", swdec);
    if (estava && gestao_hw) klog("[WIFI] %u tramas de gestión protegidas que el chip dijo descifrar y no se pudieron leer\n", gestao_hw);
    bool i0 = int_salva_desliga();
    tx_qtd = rx_qtd = 0;
    int_restaura(i0);
}

/* Espera una autenticación del punto de acceso con ese algoritmo (y, si seq
 * no es 0, ese paso), mandando antes la trama que ya está en «tq». */
static bool esperar_auth(uint16_t alg, uint16_t seq, uint32_t tiques)
{
    uint32_t fim = agora() + tiques;
    while (!passou(fim) && chip.presente) {
        resp_auth = -1;
        escutar(fim - agora(), &resp_auth);
        if (resp_auth >= 0 && auth_alg == alg && (!seq || auth_seq == seq)) return true;
    }
    return false;
}

/* Autenticación de clave compartida (WEP, 12.3.3.3): el reto del paso 2 vuelve
 * en el paso 3 cifrado con WEP. */
static bool auth_compartida(void)
{
    for (int t = 0; t < 3 && chip.presente; t++) {
        enviar_auth(1, 1, 0, (const uint8_t *)0, 0);
        if (!esperar_auth(1, 2, TIQUES_POR_SEGUNDO / 3)) continue;
        if (resp_auth != 0) { anotar_erro("el punto de acceso rechazó la clave compartida (WEP)"); return false; }
        const uint8_t *e = auth_corpo + 6;
        if (nauth_corpo < 6 + 2 || e[0] != 16 || e[1] == 0 || nauth_corpo < 6 + 2u + e[1]) {
            anotar_erro("el reto de la clave compartida no llegó bien");
            return false;
        }
        uint32_t k = cab_gestao(11, alvo.bssid, alvo.bssid);
        tq[k++] = 1; tq[k++] = 0; tq[k++] = 3; tq[k++] = 0; tq[k++] = 0; tq[k++] = 0;
        memcpy(tq + k, e, 2u + e[1]);
        k += 2u + e[1];
        k = wep_cifrar(tq, 24, k, chave_wep, nchave_wep, iv_wep++ & 0xFFFFFF, 0);
        if (drv_enviar(&chip, tq, k, R88E_TX_GESTAO, false) == 0) tx_quadros++;
        if (!esperar_auth(1, 4, TIQUES_POR_SEGUNDO / 2)) continue;
        if (resp_auth == 0) return true;
        anotar_erro("la clave WEP no es correcta (el punto de acceso rechazó el reto)");
        return false;
    }
    anotar_erro("el punto de acceso no responde a la clave compartida (WEP)");
    return false;
}

/* SAE (12.4): Commit y Confirm con el grupo 19. El PWE sale del PT (H2E) o
 * viene ya calculado por el programa «wifi» (cazar y picotear). */
static bool auth_sae(void)
{
    uint8_t pwe[SAE_PONTO_TAM];
    if (par.h2e) {
        if (!sae_pwe_de_pt(pedido.pt, chip.mac, alvo.bssid, pwe)) { anotar_erro("el PT de SAE recordado no vale"); return false; }
    } else {
        const uint8_t *x = pwe_de(&pedido, alvo.bssid);
        if (!x) { anotar_erro("WPA3: falta el PWE de este punto de acceso (vuelve a dar la contraseña)"); return false; }
        memcpy(pwe, x, sizeof pwe);
    }
    bool ok = false;
    for (int t = 0; t < 8 && !ok; t++) {
        uint8_t r[32], m[32];
        aleatorio_bytes(r, sizeof r);
        aleatorio_bytes(m, sizeof m);
        ok = sae_commit(&sae, pwe, r, m);
        wpa_apagar(r, sizeof r);
        wpa_apagar(m, sizeof m);
    }
    wpa_apagar(pwe, sizeof pwe);
    if (!ok) { anotar_erro("no se pudo preparar el Commit de SAE"); return false; }
    static uint8_t corpo[2 + 255 + 32 + 64 + 3];
    static uint8_t ficha[255];               /* anti-clogging token (12.4.6) */
    uint32_t nficha = 0;
    uint16_t estado_commit = par.h2e ? 126 : 0;   /* 126: SAE con hash-to-element */
    bool commit_ok = false;
    for (int t = 0; t < 6 && !commit_ok && chip.presente; t++) {
        uint32_t k = 0;
        corpo[k++] = SAE_GRUPO; corpo[k++] = 0;
        if (nficha && !par.h2e) { memcpy(corpo + k, ficha, nficha); k += nficha; }
        memcpy(corpo + k, sae.escalar, 32); k += 32;
        memcpy(corpo + k, sae.elemento, 64); k += 64;
        if (nficha && par.h2e) {             /* con H2E, dentro de su elemento (255, extensión 93) */
            corpo[k++] = 255; corpo[k++] = (uint8_t)(1 + nficha); corpo[k++] = 93;
            memcpy(corpo + k, ficha, nficha); k += nficha;
        }
        enviar_auth(3, 1, estado_commit, corpo, k);
        if (!esperar_auth(3, 1, TIQUES_POR_SEGUNDO / 2)) continue;
        const uint8_t *b = auth_corpo + 6;
        uint32_t nb = nauth_corpo - 6;
        if (resp_auth == 76) {               /* el punto de acceso pide la ficha */
            if (nb < 2) continue;
            b += 2; nb -= 2;
            if (par.h2e && nb >= 3 && b[0] == 255 && b[2] == 93 && b[1] >= 1 && 2u + b[1] <= nb) { nb = b[1] - 1u; b += 3; }
            nficha = nb < sizeof ficha ? nb : sizeof ficha;
            memcpy(ficha, b, nficha);
            klog("[WIFI] SAE: el punto de acceso pide un anti-clogging token (%u bytes)\n", nficha);
            continue;
        }
        if (resp_auth == 77) { anotar_erro("el punto de acceso no admite el grupo 19 (P-256) de SAE"); return false; }
        if (resp_auth != estado_commit) {
            char e[80];
            snformatar(e, sizeof e, "el punto de acceso rechazó el Commit de SAE (estado %d)", resp_auth);
            anotar_erro(e);
            return false;
        }
        if (nb < 2 + 32 + 64 || b[0] != SAE_GRUPO || b[1] != 0) { anotar_erro("Commit de SAE mal formado"); return false; }
        int r = sae_processar_commit(&sae, b + 2, b + 34);
        if (r < 0) {
            klog("[WIFI] SAE: Commit del punto de acceso rechazado (%d)\n", r);
            anotar_erro("el Commit de SAE del punto de acceso no es válido");
            return false;
        }
        commit_ok = true;
    }
    if (!commit_ok) { anotar_erro("el punto de acceso no responde al SAE"); return false; }
    uint8_t c[2 + 32];
    c[0] = 1; c[1] = 0;                      /* send-confirm = 1 */
    sae_confirmar(&sae, 1, c + 2);
    for (int t = 0; t < 4 && chip.presente; t++) {
        enviar_auth(3, 2, 0, c, sizeof c);
        if (!esperar_auth(3, 2, TIQUES_POR_SEGUNDO / 2)) continue;
        if (resp_auth != 0 || nauth_corpo < 6 + 2 + 32) break;
        uint16_t sc = (uint16_t)(auth_corpo[6] | auth_corpo[7] << 8);
        if (sae_verificar(&sae, sc, auth_corpo + 8)) return true;
        break;
    }
    anotar_erro("la contraseña no es correcta (el SAE de WPA3 no terminó)");
    return false;
}

static void conectar(void)
{
    erro[0] = 0;
    ler_ajustes();
    if (!ajuste_filas && (chip.rx_fila || chip.tx_fila)) drv_rx_simples(&chip);
    int k = melhor(pedido.ssid);
    if (k < 0) { buscar(&pedido); k = melhor(pedido.ssid); }
    if (k < 0) { anotar_erro("no se encuentra la red"); return; }
    alvo = redes[k];
    if (!suportada(&alvo)) {
        anotar_erro(alvo.i.seguranca == WPA_SEG_OUTRA ? "la red no es WPA/WPA2/WPA3-Personal (Enterprise no se admite)" :
                    "la red usa un cifrado que no se admite");
        return;
    }
    memset(&par, 0, sizeof par);
    if (alvo.i.seguranca == WPA_SEG_ABERTA) modo = MODO_ABERTA;
    else if (alvo.i.seguranca == WPA_SEG_WEP) {
        if (pedido.nwep != 5 && pedido.nwep != 13) {
            anotar_erro("la red usa WEP: hace falta su clave (5 o 13 caracteres, o 10 o 26 hexadecimales)");
            return;
        }
        modo = MODO_WEP;
        memcpy(chave_wep, pedido.wep, pedido.nwep);
        nchave_wep = pedido.nwep;
    } else {
        bool h2e;
        bool com_sae = sae_possivel(&alvo, &pedido, &h2e);
        if (!wpa_escolher(&alvo.i, com_sae, pedido.tem_pmk != 0, &par)) {
            anotar_erro(alvo.i.seguranca == WPA_SEG_WPA3 ? "la red es WPA3: hace falta su contraseña" : "la red pide contraseña");
            return;
        }
        modo = par.akm == 8 ? MODO_SAE : MODO_WPA;
        par.h2e = modo == MODO_SAE && h2e;
        bool tkip = par.cifra_par == WPA_CIFRA_TKIP || par.cifra_grupo == WPA_CIFRA_TKIP;
        if (tkip && tkip_bloqueado_ate && !passou(tkip_bloqueado_ate)) {
            anotar_erro("contramedidas de TKIP en curso: espera un minuto");
            return;
        }
    }
    estado = WIFI_CONECTANDO;
    chaves_ok = false;
    saq_ativa = false;
    pn_tx = 0;
    memset(pn_rx_par, 0, sizeof pn_rx_par);
    pn_rx_grupo = pn_rx_gestao = ipn_rx = 0;
    aleatorio_bytes(&iv_wep, sizeof iv_wep);
    saudacao_desde = 0;
    sinal = alvo.rssi;
    drv_limpar_chaves(&chip);
    drv_canal(&chip, alvo.i.canal);
    drv_modo_busca(&chip, false);
    int t;
    if (modo == MODO_SAE) {
        if (!auth_sae()) {
            /* Transición WPA2/WPA3: si el SAE no sale y hay PMK, WPA2-PSK (un aviso en el registro). */
            wpa_par_t p2;
            if (pedido.tem_pmk && wpa_escolher(&alvo.i, false, true, &p2)) {
                klog("[WIFI] SAE no terminó (%s); se prueba WPA2-PSK\n", erro);
                par = p2;
                modo = MODO_WPA;
                erro[0] = 0;
            } else { estado = WIFI_PRONTO; return; }
        }
    }
    if (modo != MODO_SAE) {
        bool ok = false;
        for (t = 0; t < 4 && !ok && chip.presente; t++) {
            enviar_auth(0, 1, 0, (const uint8_t *)0, 0);
            ok = esperar_auth(0, 2, TIQUES_POR_SEGUNDO / 3);
        }
        if (ok && resp_auth == 13 && modo == MODO_WEP) {   /* sistema abierto no admitido: clave compartida */
            if (!auth_compartida()) { estado = WIFI_PRONTO; return; }
        } else if (!ok || resp_auth != 0) {
            estado = WIFI_PRONTO;
            anotar_erro(!ok ? "el punto de acceso no responde a la autenticación" : "el punto de acceso rechazó la autenticación");
            return;
        }
    }
    /* El suplicante, listo antes de asociarse: el mensaje 1 puede llegar en la
     * misma transferencia USB que la respuesta de asociación. */
    if (modo >= MODO_WPA) {
        uint8_t snonce[WPA_NONCE_TAM];
        aleatorio_bytes(snonce, sizeof snonce);
        const uint8_t *ie = par.wpa1 ? alvo.i.wpa1_ie : alvo.i.rsn;
        uint32_t nie = par.wpa1 ? alvo.i.nwpa1_ie : alvo.i.nrsn;
        wpa_sup_preparar(&sup, &par, modo == MODO_SAE ? sae.pmk : pedido.pmk, alvo.bssid, chip.mac, ie, nie,
                         par.wpa1 ? (const uint8_t *)0 : alvo.i.rsnxe, par.wpa1 ? 0 : alvo.i.nrsnxe,
                         modo == MODO_SAE ? sae.pmkid : (const uint8_t *)0, snonce);
        wpa_apagar(snonce, sizeof snonce);
        wpa_apagar(&sae, sizeof sae);
    }
    resp_assoc = -1;
    resp_assoc_ht = false;
    usar_ht = ht_possivel(&alvo);
    for (t = 0; t < 4 && resp_assoc < 0 && chip.presente; t++) { enviar_assoc(); escutar(TIQUES_POR_SEGUNDO / 3, &resp_assoc); }
    if (usar_ht && resp_assoc != 0 && chip.presente) {
        /* D238: la asociación con HT no salió: otra vez como b/g (como antes). La
         * estación sigue autenticada (12.3.3.3): basta con repetir la asociación. */
        char m[64];
        if (resp_assoc < 0) snformatar(m, sizeof m, "sin respuesta a la asociación HT");
        else snformatar(m, sizeof m, "la asociación HT se rechazó (estado %d)", resp_assoc);
        ht_vetar(alvo.bssid, m);
        usar_ht = false;
        resp_assoc = -1;
        for (t = 0; t < 4 && resp_assoc < 0 && chip.presente; t++) { enviar_assoc(); escutar(TIQUES_POR_SEGUNDO / 3, &resp_assoc); }
    }
    if (resp_assoc != 0) {
        estado = WIFI_PRONTO;
        char m[80];
        if (resp_assoc < 0) snformatar(m, sizeof m, "el punto de acceso no responde a la asociación");
        else snformatar(m, sizeof m, "el punto de acceso rechazó la asociación (estado %d)", resp_assoc);
        anotar_erro(m);
        return;
    }
    if (via_pci) ar9485_associado(alvo.bssid, aid, (alvo.capacidade & 0x0400) != 0, (alvo.capacidade & 0x0020) != 0);
    else r88e_associado(&chip, alvo.bssid, alvo.intervalo, (alvo.capacidade & 0x0400) != 0, (alvo.capacidade & 0x0020) != 0);
    /* D238: tasas HT solo si las anunciamos y el punto de acceso las aceptó
     * (su respuesta trae HT Capabilities); si no, la escalera b/g de siempre. */
    usar_ht = usar_ht && resp_assoc_ht;
    drv_ht(&chip, usar_ht, alvo.n.sgi20, alvo.n.mcs);
    if (usar_ht)
        klog("[WIFI] 802.11n: MCS 0-%d a 20 MHz, GI corto %s, WMM; empieza en MCS %u\n",
             alvo.n.mcs & 0x80 ? 7 : alvo.n.mcs & 0x40 ? 6 : alvo.n.mcs & 0x20 ? 5 : 4, chip.sgi ? "sí" : "no",
             (unsigned)(chip.taxa - WN_TAXA_MCS0));
    else if (alvo.n.ht)
        klog("[WIFI] la red es 802.11n pero se conecta como b/g (%s)\n",
             !ajuste_ht ? "ht 0 en " AJUSTES : ht_vetado(alvo.bssid) ? "falló antes con HT" :
             !alvo.n.wmm ? "sin WMM" : (modo == MODO_WEP || par.cifra_par == WPA_CIFRA_TKIP) ? "WEP o TKIP" :
             "el punto de acceso no la aceptó");
    ultima_baliza = agora();
    if (modo == MODO_ABERTA) { ficou_conectado(); return; }
    if (modo == MODO_WEP) { chaves_ok = true; ficou_conectado(); return; }
    if (estado == WIFI_CONECTANDO) estado = WIFI_SAUDACAO;
    if (!saudacao_desde) saudacao_desde = agora();
}

/* ------------------------------------------------------------ redes recordadas */

static int hexval(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

static int hex_ler(const char *s, int ns, uint8_t *o, int max)
{
    int n = 0;
    for (int i = 0; i + 1 < ns && n < max; i += 2) {
        int a = hexval(s[i]), b = hexval(s[i + 1]);
        if (a < 0 || b < 0) return -1;
        o[n++] = (uint8_t)(a << 4 | b);
    }
    return n;
}

/* Formato (lo escribe el programa «wifi»): una línea por red,
 *   rede <SSID en hexadecimal> <PMK en hexadecimal | aberta | -> [wep=<hex>] [pt=<hex>] [pwe=<BSSID>:<hex>]...
 * (lo de detrás de la PMK es de la D220: la clave WEP, el PT de SAE y los PWE
 * de SAE por punto de acceso). Elige la recordada que se oiga más fuerte y
 * con la que se pueda conectar. */
static void ler_extras(const char *l, int nl, pedido_wifi_t *p)
{
    for (int i = 0; i < nl;) {
        while (i < nl && l[i] == ' ') i++;
        int f = i;
        while (f < nl && l[f] != ' ') f++;
        const char *t = l + i;
        int nt = f - i;
        if (nt > 4 && !memcmp(t, "wep=", 4)) {
            int n = hex_ler(t + 4, nt - 4, p->wep, 13);
            if (n == 5 || n == 13) p->nwep = (unsigned char)n;
        } else if (nt == 3 + 128 && !memcmp(t, "pt=", 3)) {
            if (hex_ler(t + 3, 128, p->pt, 64) == 64) p->tem_pt = 1;
        } else if (nt == 4 + 12 + 1 + 128 && !memcmp(t, "pwe=", 4) && t[16] == ':' && p->npwe < WIFI_MAX_PWE) {
            pwe_wifi_t *w = &p->pwe[p->npwe];
            if (hex_ler(t + 4, 12, w->bssid, 6) == 6 && hex_ler(t + 17, 128, w->pwe, 64) == 64) p->npwe++;
        }
        i = f;
    }
}

/* D354: las redes recordadas son de cada usuario (su /home/.config/wifi.conf, que con varias cuentas
 * está en /home/<nombre>): se lee la de la sesión local y, si no tiene, la de /home (compatibilidad). */
static const char *ruta_config(char *buf, size_t n)
{
    permisos_sessao_caminho(buf, n, "/.config/wifi.conf");
    return vfs_existe_arquivo(buf) ? buf : CONFIG;
}

static bool escolher_recordada(void)
{
    uint8_t *txt;
    uint32_t tam;
    char rc[96];
    if (vfs_ler_inteiro(ruta_config(rc, sizeof rc), &txt, &tam) < 0) return false;
    bool achou = false;
    int melhor_rssi = -200;
    for (uint32_t i = 0; i < tam;) {
        uint32_t f = i;
        while (f < tam && txt[f] != '\n') f++;
        const char *l = (const char *)txt + i;
        int nl = (int)(f - i);
        if (nl > 6 && !memcmp(l, "rede ", 5)) {
            int a = 5, b = a;
            while (b < nl && l[b] != ' ') b++;
            pedido_wifi_t p;
            memset(&p, 0, sizeof p);
            int ns = hex_ler(l + a, b - a, (uint8_t *)p.ssid, 32);
            if (ns > 0) {
                p.ssid[ns] = 0;
                const char *pm = l + b + 1;
                int npm = nl - b - 1;
                if (npm >= 64 && hex_ler(pm, 64, p.pmk, 32) == 32) p.tem_pmk = 1;
                int c = b + 1;
                while (c < nl && l[c] != ' ') c++;
                ler_extras(l + c, nl - c, &p);
                int k = melhor(p.ssid);
                if (k >= 0 && redes[k].rssi > melhor_rssi && credenciais_servem(&redes[k], &p)) {
                    melhor_rssi = redes[k].rssi;
                    pedido = p;
                    achou = true;
                }
                wpa_apagar(&p, sizeof p);
            }
        }
        i = f + 1;
    }
    wpa_apagar(txt, tam);
    kfree(txt);
    return achou;
}

/* ------------------------------------------------------------ cambio de modo (CD virtual) */

/* Las órdenes de StandardEject de usb_modeswitch: PREVENT ALLOW MEDIUM
 * REMOVAL (permitir) y START STOP UNIT con expulsión, en los LUN 0 y 1, por
 * el protocolo Bulk-Only (CBW de 31 bytes y CSW de 13). */
static void trocar_modo(usb_disp_t *d)
{
    static const uint8_t cdb[4][6] = { { 0x1E, 0, 0, 0, 0, 0 }, { 0x1B, 0, 0, 0, 0x02, 0 },
                                       { 0x1E, 0, 0, 0, 0, 0 }, { 0x1B, 0, 0, 0, 0x02, 0 } };
    klog("[WIFI] %04x:%04x es el CD virtual del pincho: orden de expulsion (como usb_modeswitch)\n",
         d->id_fabricante, d->id_produto);
    if (!d->ep_entrada || !d->ep_saida) return;
    for (int i = 0; i < 4; i++) {
        uint8_t cbw[31];
        memset(cbw, 0, sizeof cbw);
        cbw[0] = 'U'; cbw[1] = 'S'; cbw[2] = 'B'; cbw[3] = 'C';
        cbw[4] = (uint8_t)(0x21 + i); cbw[5] = 0x43; cbw[6] = 0x65; cbw[7] = 0x87;
        cbw[13] = (uint8_t)(i / 2);   /* LUN */
        cbw[14] = 6;
        memcpy(cbw + 15, cdb[i], 6);
        if (usb_bulk(d, false, cbw, sizeof cbw) < 0) break;
        uint8_t csw[13];
        if (usb_bulk(d, true, csw, sizeof csw) < 0) break;
    }
}

/* ------------------------------------------------------------ la tarea */

/* Da de alta wlan0 en la pila IP (la usan los dos chips). */
static void registrar_placa(const char *nome, const char *descricao, uint16_t vid, uint16_t pid)
{
    memset(&placa_wifi, 0, sizeof placa_wifi);
    placa_wifi.nome = nome;
    placa_wifi.descricao = descricao;
    memcpy(placa_wifi.mac, chip.mac, 6);
    placa_wifi.pci.fabricante = vid;
    placa_wifi.pci.modelo = pid;
    placa_wifi.irq = -1;
    placa_wifi.enviar = wl_enviar;
    placa_wifi.receber = wl_receber;
    placa_wifi.cabo = wl_cabo;
    placa_wifi.atender = wl_atender;
    rede_wifi_registrar(&placa_wifi);
}

/* D280: la Atheros AR9485 integrada (PCIe). Sin firmware ni eFuse: todo el inicio es ar9485_iniciar. */
static void arrancar_ar9485(void)
{
    estado = WIFI_INICIANDO;
    erro[0] = 0;
    nome_chip[0] = 0;
    chip.fw_versao[0] = 0;
    ler_ajustes();
    ar9485_cifra_hw(ajuste_cifra_hw);
    const char *motivo = "";
    if (ar9485_iniciar(&motivo) < 0) {
        estado = WIFI_FALHOU;
        anotar_erro(motivo);
        klog("[WIFI] AR9485: no arranca: %s\n", motivo);
        return;
    }
    const ar9485_t *a = ar9485_info();
    snformatar(nome_chip, sizeof nome_chip, "Atheros AR9485 (revisión %u)", (unsigned)a->mac_rev);
    chip_sincronizar();
    klog("[WIFI] %s, MAC %02x:%02x:%02x:%02x:%02x:%02x (ROM), CCMP por %s\n", nome_chip, chip.mac[0], chip.mac[1], chip.mac[2],
         chip.mac[3], chip.mac[4], chip.mac[5], a->cifra_hw ? "hardware" : "software");
    estado = WIFI_PRONTO;
    registrar_placa("ar9485", "Atheros AR9485 (Wi-Fi PCIe)", 0x168C, 0x0032);
}

static void arrancar_chip(void)
{
    if (via_pci) { arrancar_ar9485(); return; }
    estado = WIFI_INICIANDO;
    erro[0] = 0;
    const char *motivo;
    chip.erros_usb = 0;
    if (r88e_ler_efuse(&chip, &motivo) < 0) { estado = WIFI_FALHOU; anotar_erro(motivo); return; }
    snformatar(nome_chip, sizeof nome_chip, "Realtek RTL8188%s (versión %x)", chip.etv ? "ETV" : "EUS",
               (chip.sys_cfg >> 12) & 0xF);
    klog("[WIFI] %s, MAC %02x:%02x:%02x:%02x:%02x:%02x (eFuse)\n", nome_chip, chip.mac[0], chip.mac[1], chip.mac[2],
         chip.mac[3], chip.mac[4], chip.mac[5]);
#ifdef SAGUI_SEM_FIRMWARE
    estado = WIFI_SEM_FIRMWARE;
    anotar_erro("esta compilación no lleva firmware (SEM_FIRMWARE=1): el Wi-Fi no se puede usar");
    return;
#else
    uint8_t *fw;
    uint32_t nfw;
    if (vfs_ler_inteiro(FIRMWARE, &fw, &nfw) < 0) {
        estado = WIFI_SEM_FIRMWARE;
        anotar_erro("falta el firmware " FIRMWARE);
        return;
    }
    ler_ajustes();
    chip.usar_filas = ajuste_filas;
    int r = r88e_iniciar(&chip, fw, nfw, &motivo);
    kfree(fw);
    if (r < 0) { estado = WIFI_FALHOU; anotar_erro(motivo); return; }
    klog("[WIFI] firmware %s cargado; chip listo (%u errores USB)\n", chip.fw_versao, chip.erros_usb);
    estado = WIFI_PRONTO;
    registrar_placa("rtl8188eu", "Realtek RTL8188EU (Wi-Fi USB)", 0x0BDA, 0x8179);
#endif
}

static void tarefa_wifi(void *arg)
{
    (void)arg;
    uint32_t proxima_auto = 0;
    uint32_t transf_vistas = 0, transf_em = 0;
    usb_avisar_fila(&canal_tarefa);   /* D238: la IRQ del xHCI despierta a la tarea */
    for (;;) {
        if (disp_novo || pci_novo) {
            bool i0 = int_salva_desliga();
            if (pci_novo) {
                via_pci = true;
                chip.d = (usb_disp_t *)0;
                pci_novo = false;
            } else {
                via_pci = false;
                chip.d = disp_novo;
                disp_novo = (usb_disp_t *)0;
            }
            chip.presente = true;
            int_restaura(i0);
            chip.rx_h = -1;
            nredes = 0;
            arrancar_chip();
            proxima_auto = agora() + TIQUES_POR_SEGUNDO;
        }
        if (!chip.presente && !disp_novo && !pci_novo) estado = WIFI_SEM_PINCHO;   /* se retiró a mitad del inicio */
        if (!chip.presente || estado == WIFI_FALHOU || estado == WIFI_SEM_FIRMWARE || estado == WIFI_SEM_PINCHO) {
            dormir_tiques(TIQUES_POR_SEGUNDO / 2);
            continue;
        }
        if (pedido_desconectar) {
            pedido_desconectar = false;
            auto_conectar = false;
            desconectar((const char *)0, true);
        }
        if (pedido_busca) { pedido_busca = false; buscar((const pedido_wifi_t *)0); }
        if (pedido_conectar) {
            pedido_conectar = false;
            if (estado != WIFI_PRONTO) desconectar((const char *)0, true);
            conectar();
        }
        /* Lo que la pila IP dejó para mandar. */
        while (tx_qtd && estado == WIFI_CONECTADO) {
            static uint8_t q[ETH_MAX];
            bool i0 = int_salva_desliga();
            quadro_eth_t *f = &fila_tx[tx_ini];
            uint32_t n = f->n;
            memcpy(q, f->q, n);
            tx_ini = (tx_ini + 1) % FILA;
            tx_qtd--;
            int_restaura(i0);
            enviar_dados(q, n, false);
            tx_bytes += n;
        }
        /* D238: todo lo que haya llegado (con la cola, varias transferencias). */
        int r = 0;
        for (int k = 0; k < 2 * USB_FILA_MAX; k++) {
            int x = drv_receber(&chip, chegou);
            if (x <= 0) break;
            r += x;
        }
        if (chip.rx_transf != transf_vistas) { transf_vistas = chip.rx_transf; transf_em = agora(); }
        else if (chip.rx_fila && estado == WIFI_CONECTADO && passou(transf_em + 5 * TIQUES_POR_SEGUNDO)) {
            /* Conectado y 5 s sin nada del USB (las balizas llegan 10 veces por
             * segundo): la cola se ha quedado parada. Una lectura cada vez. */
            klog("[WIFI] la cola de recepción no trae nada desde hace 5 s: una lectura cada vez (D215)\n");
            drv_rx_simples(&chip);
            transf_em = agora();
        }
        if (estado == WIFI_CONECTADO && passou(marca_em + 10 * TIQUES_POR_SEGUNDO)) {
            /* D238: cada 10 s con tráfico, una línea para medir desde el registro serie. */
            uint32_t drx = rx_bytes - marca_rx_bytes, dtx = tx_bytes - marca_tx_bytes;
            if (drx + dtx >= 64 * 1024) {
                uint32_t vrx, vtx;
                drv_em_voo(&chip, &vrx, &vtx);
                uint16_t td = wn_taxa_dec(chip.taxa, chip.sgi && chip.taxa >= WN_TAXA_MCS0);
                klog("[WIFI] rx %u KB/s, tx %u KB/s; tasa %u,%u Mb/s%s; USB rx %u (cola %u, en vuelo %u, hasta %u tramas), "
                     "tx %u (en vuelo %u, llena %u)\n", drx / 10240, dtx / 10240, td / 10, td % 10,
                     chip.ht ? " (HT)" : "", chip.rx_transf, chip.rx_fila, vrx, chip.rx.max_por_transf, chip.tx_transf,
                     vtx, chip.tx_cheia);
            }
            marca_rx_bytes = rx_bytes;
            marca_tx_bytes = tx_bytes;
            marca_em = agora();
        }
        if (estado == WIFI_SAUDACAO && passou(saudacao_desde + 8 * TIQUES_POR_SEGUNDO)) {
            if (chip.ht) {
                /* D238: con HT puede ser cosa de la HT (QoS, tasas): se veta para
                 * esta red y se prueba otra vez como b/g antes de culpar a la contraseña. */
                ht_vetar(alvo.bssid, "el saludo de WPA no terminó con HT");
                desconectar("el saludo de WPA no terminó con 802.11n: se prueba como b/g", true);
                proxima_auto = agora() + TIQUES_POR_SEGUNDO;
            } else {
                desconectar("la contraseña no es correcta (el saludo de WPA no terminó)", true);
                auto_conectar = false;
            }
        }
        if (saq_ativa && estado == WIFI_CONECTADO) {   /* consulta SA de la 802.11w */
            if (passou(saq_ate)) {
                saq_ativa = false;
                desconectar("el punto de acceso no confirmó la conexión (consulta SA de la 802.11w)", false);
                proxima_auto = agora() + 2 * TIQUES_POR_SEGUNDO;
            } else if (passou(saq_reenvio)) {
                saq_reenvio = saq_ate + TIQUES_POR_SEGUNDO;
                enviar_sa_query(0, saq_id);
            }
        }
        if (estado == WIFI_CONECTADO && passou(ultima_baliza + 10 * TIQUES_POR_SEGUNDO)) {
            desconectar("se perdió la señal del punto de acceso", false);
            proxima_auto = agora() + 2 * TIQUES_POR_SEGUNDO;
        }
        if (estado != WIFI_CONECTADO) { marca_em = agora(); marca_rx_bytes = rx_bytes; marca_tx_bytes = tx_bytes; }
        if (estado == WIFI_PRONTO && auto_conectar && passou(proxima_auto)) {
            proxima_auto = agora() + 30 * TIQUES_POR_SEGUNDO;
            bool i0 = int_salva_desliga();
            bool havia = pedido.ssid[0] != 0;
            int_restaura(i0);
            if (!havia) {   /* sin una red pedida en esta sesión: las recordadas */
                char rc[96];
                if (!vfs_existe_arquivo(ruta_config(rc, sizeof rc))) continue;
                buscar((const pedido_wifi_t *)0);
                if (escolher_recordada()) conectar();
            } else conectar();
        }
        if (r <= 0 && !tx_qtd) dormir_tiques(1);
    }
}

/* ------------------------------------------------------------ USB */

bool wifi_usb_reconhecer(usb_disp_t *d)
{
    if (d->id_fabricante == 0x1111 && d->id_produto == 0x1111 && d->classe == 0x08) {
        trocar_modo(d);
        return true;
    }
    if (d->id_fabricante != 0x0BDA || d->id_produto != 0x8179) return false;
    klog("[WIFI] pincho Realtek RTL8188EU (0bda:8179), %s\n",
         chip.presente ? "ya hay otro: este no se usa" : "se enciende en la tarea wifi");
    if (chip.presente || disp_novo || pci_novo) return true;
    if (!d->ep_entrada || !d->ep_saida) { klog("[WIFI] sin endpoints bulk\n"); return true; }
    d->wifi = true;
    disp_novo = d;
    estado = WIFI_INICIANDO;
    if (!tarefa_criada) { tarefa_criada = true; tarefa_kernel("wifi", tarefa_wifi, (void *)0); }
    acordar(&canal_tarefa);
    return true;
}

/* D280: rede_iniciar() llama aquí tras buscar la tarjeta de red: si hay una AR9485 en el PCI, la tarea
 * «wifi» la enciende (lo lento, y lo que puede fallar, va en la tarea y no en el arranque). */
void wifi_pci_iniciar(void)
{
    if (chip.presente || disp_novo || pci_novo) return;
    if (!ar9485_procurar()) return;
    pci_novo = true;
    estado = WIFI_INICIANDO;
    if (!tarefa_criada) { tarefa_criada = true; tarefa_kernel("wifi", tarefa_wifi, (void *)0); }
    acordar(&canal_tarefa);
}

void wifi_usb_retirado(usb_disp_t *d)
{
    if (d != chip.d) { if (d == disp_novo) disp_novo = (usb_disp_t *)0; return; }
    klog("[WIFI] pincho retirado\n");
    bool estava = estado == WIFI_CONECTADO;
    chip.presente = false;
    if (chip.rx_h >= 0 && d->tipo_ctl == USB_EHCI) usb_rx_fechar(d, chip.rx_h);
    chip.rx_h = -1;
    estado = WIFI_SEM_PINCHO;
    chaves_ok = false;
    tx_qtd = rx_qtd = 0;
    if (estava) rede_wifi_conectada(false);
    rede_wifi_registrar((placa_t *)0);
}

/* ------------------------------------------------------------ llamada al sistema */

void wifi_info(info_wifi_t *v)
{
    memset(v, 0, sizeof *v);
    v->estado = estado;
    txt_copia_n(v->chip, nome_chip, sizeof v->chip);
    txt_copia_n(v->firmware, chip.fw_versao, sizeof v->firmware);
    if (estado != WIFI_SEM_PINCHO) memcpy(v->mac, chip.mac, 6);
    v->buscando = buscando || pedido_busca;
    v->canal = chip.canal;
    if (estado >= WIFI_CONECTANDO && estado <= WIFI_CONECTADO) {
        memcpy(v->ssid, alvo.i.ssid, alvo.i.nssid);
        memcpy(v->bssid, alvo.bssid, 6);
        v->sinal = (signed char)sinal;
        v->qualidade = (unsigned char)qualidade(sinal);
        /* la seguridad de la conexión (no la de la red): WPA3 si se hizo SAE */
        v->seguranca = modo == MODO_ABERTA ? WIFI_ABERTA : modo == MODO_WEP ? WIFI_WEP : modo == MODO_SAE ? WIFI_WPA3 :
                       par.wpa1 ? WIFI_WPA1 : WIFI_WPA2;
        if (modo >= MODO_WPA) {
            if (par.cifra_par == WPA_CIFRA_TKIP || par.cifra_grupo == WPA_CIFRA_TKIP) v->detalhe |= WIFI_DET_TKIP;
            if (par.pmf) v->detalhe |= WIFI_DET_PMF;
            if (par.h2e) v->detalhe |= WIFI_DET_H2E;
        }
        /* D238: con HT, la tasa MCS (6,5 a 72,2 Mb/s); taxa_mbps sigue siendo entera (5 = 5,5). */
        bool sgi = chip.sgi && chip.taxa >= WN_TAXA_MCS0;
        v->taxa_dec = wn_taxa_dec(chip.taxa, sgi);
        v->taxa_mbps = (unsigned char)(v->taxa_dec / 10);
        v->mcs = chip.taxa >= WN_TAXA_MCS0 && chip.taxa <= WN_TAXA_MCS7 ? (unsigned char)(chip.taxa - WN_TAXA_MCS0) : 255;
        v->sgi = sgi;
        v->ht = chip.ht ? 1 : alvo.n.ht ? 2 : 0;
        uint8_t tr = chip.taxa_rx;
        v->taxa_rx_dec = wn_taxa_dec(tr, false);
        v->mcs_rx = tr >= WN_TAXA_MCS0 && tr <= WN_TAXA_MCS7 ? (unsigned char)(tr - WN_TAXA_MCS0) : 255;
    } else {
        v->mcs = v->mcs_rx = 255;
    }
    txt_copia_n(v->erro, erro, sizeof v->erro);
    v->redes = (unsigned)nredes;
    v->rx_quadros = rx_quadros;
    v->tx_quadros = tx_quadros;
    v->balizas = balizas;
    v->erros_usb = chip.erros_usb;
    v->respostas_sondeo = respostas_sondeo;
    if (estado == WIFI_CONECTADO) v->conectado_s = (agora() - conectado_em) / TIQUES_POR_SEGUNDO;
    /* D238: para medir. Las transferencias en vuelo, sin tocar el USB (el
     * estado de las colas está en memoria). */
    v->rx_bytes = rx_bytes;
    v->tx_bytes = tx_bytes;
    v->rx_usb = chip.rx_transf;
    v->tx_usb = chip.tx_transf;
    v->rx_agregadas_max = chip.rx.max_por_transf;
    v->tx_cola_llena = chip.tx_cheia;
    v->tx_perdidas = chip.tx_perdidas;
    v->addba_rechazados = addba_recusados;
    v->rx_cola = chip.rx_fila;
    v->tx_cola = chip.tx_fila;
    if (estado != WIFI_SEM_PINCHO && chip.presente) {
        uint32_t vrx, vtx;
        drv_em_voo(&chip, &vrx, &vtx);
        v->rx_en_vuelo = (unsigned char)vrx;
        v->tx_en_vuelo = (unsigned char)vtx;
    }
}

int32_t sis_wifi(uint32_t op, uint32_t ptr, uint32_t n, uint32_t d_, uint32_t e_)
{
    (void)d_; (void)e_;
    switch (op) {
    case WIFI_ESTADO: {
        uint32_t tam = (n && n < sizeof(info_wifi_t)) ? n : sizeof(info_wifi_t);
        if (!usuario_ok((void *)ptr, tam, true)) return ERRO_ENDERECO;
        static info_wifi_t v;
        wifi_info(&v);
        memcpy((void *)ptr, &v, tam);
        return estado == WIFI_SEM_PINCHO ? ERRO_NAO_EXISTE : 0;
    }
    case WIFI_BUSCAR:
        if (estado == WIFI_SEM_PINCHO || estado == WIFI_INICIANDO) return ERRO_NAO_EXISTE;
        if (estado == WIFI_FALHOU || estado == WIFI_SEM_FIRMWARE) return ERRO_OCUPADO;
        pedido_busca = true;
        acordar(&canal_tarefa);
        return 0;
    case WIFI_REDES: {
        if (n > MAX_REDES) n = MAX_REDES;
        if (!usuario_ok((void *)ptr, n * sizeof(rede_wifi_t), true)) return ERRO_ENDERECO;
        rede_wifi_t *o = (rede_wifi_t *)ptr;
        uint32_t k = 0;
        for (int i = 0; i < nredes && k < n; i++, k++) {
            memset(&o[k], 0, sizeof o[k]);
            memcpy(o[k].ssid, redes[i].i.ssid, redes[i].i.nssid);
            memcpy(o[k].bssid, redes[i].bssid, 6);
            o[k].canal = redes[i].i.canal;
            o[k].sinal = redes[i].rssi;
            o[k].seguranca = redes[i].i.seguranca;
            o[k].suportada = suportada(&redes[i]);
            o[k].qualidade = (unsigned char)qualidade(redes[i].rssi);
            o[k].oculta = redes[i].i.nssid == 0;
            const wpa_bss_t *b = &redes[i].i;
            bool tkip = b->nrsn ? (b->cifra_grupo == WPA_CIFRA_TKIP || b->cifras_par == (1u << WPA_CIFRA_TKIP)) :
                        (b->wpa1 && (b->wpa1_grupo == WPA_CIFRA_TKIP || b->wpa1_pares == (1u << WPA_CIFRA_TKIP)));
            o[k].detalhe = (unsigned char)((tkip ? WIFI_DET_TKIP : 0) | (b->mfpr ? WIFI_DET_PMF : 0) |
                                           (b->h2e ? WIFI_DET_H2E : 0) | (b->so_h2e ? WIFI_DET_SO_H2E : 0));
        }
        return (int32_t)k;
    }
    case WIFI_CONECTAR: {
        if (!usuario_ok((void *)ptr, sizeof(pedido_wifi_t), false)) return ERRO_ENDERECO;
        if (estado == WIFI_SEM_PINCHO || estado == WIFI_INICIANDO) return ERRO_NAO_EXISTE;
        if (estado == WIFI_FALHOU || estado == WIFI_SEM_FIRMWARE) return ERRO_OCUPADO;
        pedido_wifi_t p = *(const pedido_wifi_t *)ptr;
        p.ssid[32] = 0;
        if (!p.ssid[0]) return ERRO_INVALIDO;
        pedido = p;
        wpa_apagar(&p, sizeof p);
        erro[0] = 0;
        auto_conectar = true;
        pedido_conectar = true;
        acordar(&canal_tarefa);
        return 0;
    }
    case WIFI_DESCONECTAR:
        if (estado == WIFI_SEM_PINCHO) return ERRO_NAO_EXISTE;
        pedido_desconectar = true;
        bool i0 = int_salva_desliga();
        wpa_apagar(&pedido, sizeof pedido);
        int_restaura(i0);
        acordar(&canal_tarefa);
        return 0;
    }
    return ERRO_INVALIDO;
}
