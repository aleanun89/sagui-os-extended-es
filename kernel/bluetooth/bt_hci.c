// SPDX-License-Identifier: GPL-2.0-or-later
/* bt_hci.c - capa HCI de la pila Bluetooth (D279): órdenes, eventos, tráfico ACL con créditos,
 * inicio del controlador y el registro del adaptador. Solo especificación pública (Core,
 * volumen 4 parte E: HCI funcional). */
#include "bt.h"
#ifdef BT_ANFITRION
#include <stdarg.h>
#include <stdio.h>
#else
#include "console.h"
#endif

#define EVENTOS_MASCARA 0x00007FFFFFFFFFFFull   /* bits 0..46: incluye SSP y los resultados extendidos */
#define TIEMPO_CMD_MS   5000

static const bt_plat_t *plat;
static bt_info_t info;

static struct { uint16_t op; uint8_t n; uint8_t par[248]; } cfila[BT_CMD_FILA];
static int c_cab, c_cola, c_cnt;
static int cred_cmd;
static uint32_t cmd_envio;
static uint16_t cmd_op;           /* última orden enviada (para el tiempo límite) */

static struct { uint16_t manejador; uint8_t n, pb; uint8_t d[BT_ACL_FILA_TAM]; } afila[BT_ACL_FILA];
static int a_cab, a_cola, a_cnt;
static struct { uint16_t manejador; int n; } sinack[BT_MAX_LIGACOES + 4];   /* paquetes sin confirmar por enlace */
static int acl_libres, acl_num;
static uint16_t acl_mtu;

enum { P_RESET, P_VERSAO, P_FEATURES, P_ADDR, P_BUFFER, P_EVMASK, P_INQMODE, P_SSP, P_NOME, P_CLASE, P_SCAN, P_FIM };
static const uint16_t paso_op[] = { OP_RESET, OP_RD_VERSAO, OP_RD_FEATURES, OP_RD_ADDR, OP_RD_BUFFER, OP_SET_EVMASK, OP_WR_INQMODE,
                                    OP_WR_SSP, OP_WR_NOME, OP_WR_CLASE, OP_WR_SCAN };
static int passo;
static uint32_t paso_desde;       /* ms: cuándo se pidió el paso actual del inicio (vigilancia) */
static bool listo;
/* Características LMP (Read_Local_Supported_Features, Core vol. 2 parte C §3.3). Sin ellas (orden
 * rechazada) se supone todo y se prueba: lo que el controlador rechace se salta. */
static uint8_t feat[8];
static bool tem_feat;
static uint8_t inq_modo;          /* 2 = con EIR, 1 = con RSSI, 0 = clásico */
static int erros_logados;

#define FEAT_RSSI_INQ(f) ((f)[3] & 0x40)   /* byte 3 bit 6: RSSI with inquiry results */
#define FEAT_EIR(f)      ((f)[6] & 0x01)   /* byte 6 bit 0: Extended Inquiry Response */
#define FEAT_SSP(f)      ((f)[6] & 0x08)   /* byte 6 bit 3: Secure Simple Pairing */

const bt_plat_t *bt_plat(void) { return plat; }
uint32_t bt_agora(void) { return plat && plat->ms ? plat->ms() : 0; }

void bt_log(const char *fmt, ...)
{
    char t[160];
    va_list ap;
    va_start(ap, fmt);
#ifdef BT_ANFITRION
    vsnprintf(t, sizeof t, fmt, ap);
#else
    formatar(t, sizeof t, fmt, ap);
#endif
    va_end(ap);
    if (plat && plat->registro) plat->registro(t);
}

/* ------------------------------------------------------------------ utilidades */

int bt_hex(char *s, const uint8_t *d, int n)
{
    static const char h[] = "0123456789abcdef";
    for (int i = 0; i < n; i++) { s[2 * i] = h[d[i] >> 4]; s[2 * i + 1] = h[d[i] & 15]; }
    s[2 * n] = 0;
    return 2 * n;
}

int bt_addr_texto(char *s, const uint8_t *a)
{
    static const char h[] = "0123456789ABCDEF";
    int k = 0;
    for (int i = 5; i >= 0; i--) {
        s[k++] = h[a[i] >> 4];
        s[k++] = h[a[i] & 15];
        if (i) s[k++] = ':';
    }
    s[k] = 0;
    return k;
}

static int nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool bt_addr_ler(const char *s, uint8_t *a)
{
    for (int i = 5; i >= 0; i--) {
        int h = nibble(s[0]), l = nibble(s[1]);
        if (h < 0 || l < 0) return false;
        a[i] = (uint8_t)(h << 4 | l);
        s += 2;
        if (i) { if (*s != ':') return false; s++; }
    }
    return true;
}

/* ------------------------------------------------------------------ órdenes */

static void enviar_cmds(void)
{
    while (c_cnt && cred_cmd > 0) {
        uint8_t p[3 + 248];
        bt_poe16(p, cfila[c_cab].op);
        p[2] = cfila[c_cab].n;
        memcpy(p + 3, cfila[c_cab].par, cfila[c_cab].n);
        int n = 3 + cfila[c_cab].n;
        cmd_op = cfila[c_cab].op;
        c_cab = (c_cab + 1) % BT_CMD_FILA;
        c_cnt--;
        cred_cmd--;
        cmd_envio = bt_agora();
        if (plat->enviar_cmd(p, n) < 0) {
            info.erros_hci++;
            cred_cmd++;   /* no salió: el crédito sigue ahí */
        }
    }
}

int bt_hci_cmd(uint16_t op, const void *par, int n)
{
    if (n < 0 || n > 248 || c_cnt >= BT_CMD_FILA) return -1;
    cfila[c_cola].op = op;
    cfila[c_cola].n = (uint8_t)n;
    if (n) memcpy(cfila[c_cola].par, par, (size_t)n);
    c_cola = (c_cola + 1) % BT_CMD_FILA;
    c_cnt++;
    enviar_cmds();
    return 0;
}

/* Nombre corto de las órdenes de inicio, para el registro (qué orden contó como error). */
static const char *nome_op(uint16_t op)
{
    switch (op) {
    case OP_RESET: return "Reset";
    case OP_RD_VERSAO: return "Read_Local_Version";
    case OP_RD_FEATURES: return "Read_Local_Features";
    case OP_RD_ADDR: return "Read_BD_ADDR";
    case OP_RD_BUFFER: return "Read_Buffer_Size";
    case OP_SET_EVMASK: return "Set_Event_Mask";
    case OP_WR_INQMODE: return "Write_Inquiry_Mode";
    case OP_WR_SSP: return "Write_Simple_Pairing_Mode";
    case OP_WR_NOME: return "Write_Local_Name";
    case OP_WR_CLASE: return "Write_Class_of_Device";
    case OP_WR_SCAN: return "Write_Scan_Enable";
    case OP_INQUIRY: return "Inquiry";
    case OP_CRIAR_CONEX: return "Create_Connection";
    default: return "orden";
    }
}

/* Pasos que dependen de lo que el controlador anuncia: con las características leídas, se saltan los
 * que no soporta (un clon CSR BT 2.0, HCI/LMP 3, no tiene EIR ni SSP: Write_Simple_Pairing_Mode le
 * es desconocida y contesta con Command Status 0x01, no con Command Complete). */
static bool paso_soportado(int p)
{
    if (!tem_feat) return true;
    if (p == P_INQMODE) return inq_modo != 0;
    if (p == P_SSP) return FEAT_SSP(feat) != 0;
    return true;
}

static void init_paso(void)
{
    uint8_t b[248];
    while (passo < P_FIM && !paso_soportado(passo)) {
        bt_log("[BT] inicio: %s se salta (el controlador no la anuncia)", nome_op(paso_op[passo]));
        passo++;
    }
    paso_desde = bt_agora();
    switch (passo) {
    case P_RESET: bt_hci_cmd(OP_RESET, NULL, 0); break;
    case P_VERSAO: bt_hci_cmd(OP_RD_VERSAO, NULL, 0); break;
    case P_FEATURES: bt_hci_cmd(OP_RD_FEATURES, NULL, 0); break;
    case P_ADDR: bt_hci_cmd(OP_RD_ADDR, NULL, 0); break;
    case P_BUFFER: bt_hci_cmd(OP_RD_BUFFER, NULL, 0); break;
    case P_EVMASK:
        for (int i = 0; i < 8; i++) b[i] = (uint8_t)(EVENTOS_MASCARA >> (8 * i));
        bt_hci_cmd(OP_SET_EVMASK, b, 8);
        break;
    case P_INQMODE: b[0] = inq_modo; bt_hci_cmd(OP_WR_INQMODE, b, 1); break;   /* 2: RSSI y nombre (EIR); 1: RSSI */
    case P_SSP: b[0] = 1; bt_hci_cmd(OP_WR_SSP, b, 1); break;
    case P_NOME:
        memset(b, 0, sizeof b);
        memcpy(b, "SaguiOS", 7);
        bt_hci_cmd(OP_WR_NOME, b, 248);
        break;
    case P_CLASE: b[0] = 0x04; b[1] = 0x01; b[2] = 0x00; bt_hci_cmd(OP_WR_CLASE, b, 3); break;   /* ordenador de sobremesa */
    case P_SCAN:
        b[0] = bt_g_energia() ? 2 : 0;   /* conectable (los teclados reconectan solos), nunca visible */
        bt_hci_cmd(OP_WR_SCAN, b, 1);
        break;
    case P_FIM:
        listo = true;
        info.estado = BT_PRONTO;
        bt_g_set_estado_adaptador(BT_PRONTO);
        bt_g_adaptador_pronto();
        break;
    }
}

void bt_hci_iniciar(const bt_plat_t *p)
{
    plat = p;
    memset(&info, 0, sizeof info);
    info.energia = bt_g_energia();
    c_cab = c_cola = c_cnt = 0;
    a_cab = a_cola = a_cnt = 0;
    memset(sinack, 0, sizeof sinack);
    cred_cmd = 1;
    acl_libres = acl_num = 0;
    acl_mtu = 0;
    listo = false;
    memset(feat, 0, sizeof feat);
    tem_feat = false;
    inq_modo = 2;
    erros_logados = 0;
    passo = P_RESET;
    info.estado = BT_INICIANDO;
    bt_l2cap_reiniciar();
    bt_g_set_estado_adaptador(BT_INICIANDO);
    init_paso();
}

bool bt_hci_listo(void) { return listo; }

void bt_hci_energia(bool e)
{
    info.energia = e;
    if (listo) {
        uint8_t b = e ? 2 : 0;
        bt_hci_cmd(OP_WR_SCAN, &b, 1);
    }
}

/* Terminó la orden "op" con el estado "st". Si es parte del inicio, sigue con el siguiente paso. */
static bool paso_critico(int p) { return p == P_RESET || p == P_VERSAO || p == P_ADDR || p == P_BUFFER; }

/* Un error de orden fuera del inicio: se cuenta y se anota (las primeras, para no llenar el registro). */
static void erro_orden(uint16_t op, uint8_t st)
{
    info.erros_hci++;
    if (erros_logados < 16) {
        erros_logados++;
        bt_log("[BT] %s (%04x) rechazada (estado %02x)", nome_op(op), op, st);
    }
}

static void orden_acabada(uint16_t op, uint8_t st, const uint8_t *r, int n)
{
    bool en_inicio = info.estado == BT_INICIANDO && passo < P_FIM && op == paso_op[passo];
    if (!en_inicio) {
        if (st) erro_orden(op, st);
        return;
    }
    if (st) info.erros_hci++;
    bool critico = paso_critico(passo);
    if (st && critico) {
        info.estado = BT_FALHOU;
        bt_log("[BT] el controlador rechazo la orden %04x de inicio (estado %02x)", op, st);
        bt_g_set_estado_adaptador(BT_FALHOU);
        return;
    }
    if (!st) {
        if (passo == P_VERSAO && n >= 8) {
            info.hci_ver = r[0];
            info.hci_rev = bt_le16(r + 1);
            info.lmp_ver = r[3];
            info.fabricante = bt_le16(r + 4);
            info.lmp_sub = bt_le16(r + 6);
        } else if (passo == P_FEATURES && n >= 8) {
            memcpy(feat, r, 8);
            tem_feat = true;
            inq_modo = FEAT_EIR(feat) ? 2 : FEAT_RSSI_INQ(feat) ? 1 : 0;
            bt_log("[BT] caracteristicas LMP %02x%02x%02x%02x%02x%02x%02x%02x: RSSI %s, EIR %s, SSP %s", feat[0], feat[1], feat[2],
                   feat[3], feat[4], feat[5], feat[6], feat[7], FEAT_RSSI_INQ(feat) ? "si" : "no", FEAT_EIR(feat) ? "si" : "no",
                   FEAT_SSP(feat) ? "si" : "no");
        } else if (passo == P_ADDR && n >= 6) {
            memcpy(info.addr, r, 6);
        } else if (passo == P_BUFFER && n >= 7) {
            acl_mtu = bt_le16(r);
            acl_num = bt_le16(r + 3);
            acl_libres = acl_num;
            info.acl_mtu = acl_mtu;
            info.acl_num = (uint8_t)(acl_num > 255 ? 255 : acl_num);
            if (acl_mtu < 27 || acl_num < 1) {
                info.estado = BT_FALHOU;
                bt_log("[BT] tamano de buffer ACL imposible (%u x %d)", acl_mtu, acl_num);
                bt_g_set_estado_adaptador(BT_FALHOU);
                return;
            }
        }
    } else {
        bt_log("[BT] inicio: %s (%04x) rechazada (estado %02x): se sigue", nome_op(op), op, st);
        if (passo == P_INQMODE && inq_modo == 2) {   /* sin EIR: se prueba con RSSI solo */
            inq_modo = 1;
            init_paso();
            return;
        }
    }
    passo++;
    init_paso();
}

/* ------------------------------------------------------------------ ACL */

static int *sinack_de(uint16_t m, bool criar)
{
    int libre = -1;
    for (int i = 0; i < (int)(sizeof sinack / sizeof sinack[0]); i++) {
        if (sinack[i].n && sinack[i].manejador == m) return &sinack[i].n;
        if (!sinack[i].n && libre < 0) libre = i;
    }
    if (!criar || libre < 0) return NULL;
    sinack[libre].manejador = m;
    return &sinack[libre].n;
}

static void drenar_acl(void)
{
    while (a_cnt && acl_libres > 0) {
        uint8_t p[4 + BT_ACL_FILA_TAM];
        int n = afila[a_cab].n;
        bt_poe16(p, (uint16_t)(afila[a_cab].manejador | (afila[a_cab].pb ? 0x1000 : 0x2000)));   /* PB: continuación o inicio */
        bt_poe16(p + 2, (uint16_t)n);
        memcpy(p + 4, afila[a_cab].d, (size_t)n);
        uint16_t m = afila[a_cab].manejador;
        a_cab = (a_cab + 1) % BT_ACL_FILA;
        a_cnt--;
        if (plat->enviar_acl(p, 4 + n) < 0) { info.erros_hci++; continue; }
        acl_libres--;
        info.acl_tx++;
        int *s = sinack_de(m, true);
        if (s) (*s)++;
    }
}

int bt_hci_acl_enviar(uint16_t manejador, const uint8_t *dados, int n)
{
    if (!listo || n < 0 || n > BT_ACL_FILA_TAM) return -1;
    int frag = (n + acl_mtu - 1) / acl_mtu;   /* el controlador tiene buffers de acl_mtu bytes: se fragmenta */
    if (frag < 1) frag = 1;
    if (a_cnt + frag > BT_ACL_FILA) return -1;
    for (int i = 0, pos = 0; i < frag; i++) {
        int l = n - pos > acl_mtu ? acl_mtu : n - pos;
        afila[a_cola].manejador = manejador;
        afila[a_cola].n = (uint8_t)l;
        afila[a_cola].pb = i ? 1 : 0;
        memcpy(afila[a_cola].d, dados + pos, (size_t)l);
        pos += l;
        a_cola = (a_cola + 1) % BT_ACL_FILA;
        a_cnt++;
    }
    drenar_acl();
    return 0;
}

void bt_hci_acl_entrada(const uint8_t *p, int n)
{
    if (n < 4) return;
    uint16_t hf = bt_le16(p), len = bt_le16(p + 2);
    if (4 + len > n) return;   /* truncado: se descarta */
    info.acl_rx++;
    bt_l2cap_acl(hf & 0x0FFF, (uint8_t)((hf >> 12) & 3), p + 4, len);
}

static void quitar_acl_de(uint16_t m)
{
    int k = a_cnt;
    int cab = a_cab;
    a_cnt = 0;
    int cola = a_cab;
    for (int i = 0; i < k; i++) {
        int j = (cab + i) % BT_ACL_FILA;
        if (afila[j].manejador == m) continue;
        if (cola != j) afila[cola] = afila[j];
        cola = (cola + 1) % BT_ACL_FILA;
        a_cnt++;
    }
    a_cola = cola;
    int *s = sinack_de(m, false);
    if (s) {   /* lo que estaba en vuelo en ese enlace ya no se confirma */
        acl_libres += *s;
        if (acl_libres > acl_num) acl_libres = acl_num;
        *s = 0;
    }
}

/* ------------------------------------------------------------------ eventos */

static void eir_nome(const uint8_t *e, int n, char *nome, int max)
{
    for (int i = 0; i + 1 < n && e[i];) {
        int l = e[i];
        if (i + 1 + l > n) break;
        if ((e[i + 1] == 0x09 || (e[i + 1] == 0x08 && !nome[0])) && l > 1) {
            int k = l - 1 < max - 1 ? l - 1 : max - 1;
            memcpy(nome, e + i + 2, (size_t)k);
            nome[k] = 0;
        }
        i += l + 1;
    }
}

static void ev_inquiry(uint8_t code, const uint8_t *p, int n)
{
    if (n < 1) return;
    int num = p[0];
    if (num < 1) return;
    if (code == EV_INQ_EXT) {
        if (n < 15) return;
        char nome[BT_NOME_MAX];
        nome[0] = 0;
        eir_nome(p + 15, n - 15, nome, sizeof nome);
        uint32_t cod = (uint32_t)(p[9] | p[10] << 8 | p[11] << 16);
        bt_g_inq_resultado(p + 1, cod, (int8_t)p[14], nome);
        return;
    }
    int rec = (n - 1) / num;
    if (rec < 13) return;
    for (int i = 0; i < num; i++) {
        const uint8_t *r = p + 1 + i * rec;
        uint32_t cod;
        int8_t rssi = 0;
        if (code == EV_INQ_RES) cod = (uint32_t)(r[rec - 5] | r[rec - 4] << 8 | r[rec - 3] << 16);
        else { cod = (uint32_t)(r[rec - 6] | r[rec - 5] << 8 | r[rec - 4] << 16); rssi = (int8_t)r[rec - 1]; }
        bt_g_inq_resultado(r, cod, rssi, "");
    }
}

static void ev_completo(const uint8_t *p, int n)
{
    if (n < 3) return;
    cred_cmd = p[0];
    uint16_t op = bt_le16(p + 1);
    uint8_t st = n >= 4 ? p[3] : 0;
    orden_acabada(op, st, p + 4, n >= 4 ? n - 4 : 0);
    if (op == OP_INQUIRY_CANC) { /* el gestor ve el fin por el evento de fin de búsqueda o por su cuenta */ }
}

static void ev_estado(const uint8_t *p, int n)
{
    if (n < 4) return;
    cred_cmd = p[1];
    uint16_t op = bt_le16(p + 2);
    /* Una orden de inicio rechazada con Command Status (los clones CSR lo hacen con las órdenes que no
     * conocen) acaba ahí: sin esto el inicio se quedaba en «iniciando» para siempre. */
    if (p[0] && info.estado == BT_INICIANDO && passo < P_FIM && op == paso_op[passo]) {
        orden_acabada(op, p[0], NULL, 0);
        return;
    }
    if (p[0]) {
        erro_orden(op, p[0]);
        if (op == OP_CRIAR_CONEX) bt_g_conexao(p[0], 0xFFFF, NULL, false);   /* sin dirección: el gestor la tiene */
        else if (op == OP_INQUIRY) bt_g_inq_fim();
        else if (op == OP_AUTENTICAR || op == OP_CIFRAR) bt_g_autenticacao(0xFFFF, p[0]);
    }
}

void bt_hci_evento(const uint8_t *p, int n)
{
    if (n < 2 || 2 + p[1] > n) return;
    uint8_t code = p[0];
    int len = p[1];
    const uint8_t *q = p + 2;
    info.eventos++;
    switch (code) {
    case EV_INQ_FIM: bt_g_inq_fim(); break;
    case EV_INQ_RES: case EV_INQ_RSSI: case EV_INQ_EXT: ev_inquiry(code, q, len); break;
    case EV_CONEXAO:
        if (len >= 11 && q[9] == 1) bt_g_conexao(q[0], bt_le16(q + 1), q + 3, false);
        break;
    case EV_PEDIDO_CONEX:
        if (len >= 10) {
            if (q[9] != 1) { uint8_t r[7]; memcpy(r, q, 6); r[6] = 0x0F; bt_hci_cmd(OP_REJEITAR_CONEX, r, 7); break; }
            if (bt_g_pedido_conexao(q)) {
                uint8_t r[7];
                memcpy(r, q, 6);
                r[6] = 0x01;   /* seguimos siendo esclavos: sin cambio de papel */
                bt_hci_cmd(OP_ACEITAR_CONEX, r, 7);
            } else {
                uint8_t r[7];
                memcpy(r, q, 6);
                r[6] = 0x0F;   /* dirección inaceptable */
                bt_hci_cmd(OP_REJEITAR_CONEX, r, 7);
            }
        }
        break;
    case EV_DESCONEXAO:
        if (len >= 4 && q[0] == 0) {
            uint16_t m = bt_le16(q + 1);
            quitar_acl_de(m);
            bt_l2cap_ligacao_caiu(m);
            bt_g_desconexao(m, q[3]);
            drenar_acl();
        }
        break;
    case EV_AUTENTICACAO: if (len >= 3) bt_g_autenticacao(bt_le16(q + 1), q[0]); break;
    case EV_CIFRADO: if (len >= 4) bt_g_cifrado(bt_le16(q + 1), q[0], q[3] != 0); break;
    case EV_NOME_REMOTO:
        if (len >= 7) {
            char nome[BT_NOME_MAX];
            int k = 0;
            while (k < len - 7 && k < BT_NOME_MAX - 1 && q[7 + k]) { nome[k] = (char)q[7 + k]; k++; }
            nome[k] = 0;
            bt_g_nome_remoto(q + 1, q[0], nome);
        }
        break;
    case EV_CMD_COMPLETO: ev_completo(q, len); break;
    case EV_CMD_ESTADO: ev_estado(q, len); break;
    case EV_ERRO_HW:
        bt_log("[BT] error de hardware del controlador (%02x)", len ? q[0] : 0);
        info.erros_hci++;
        break;
    case EV_PACOTES_COMP:
        if (len >= 1) {
            int k = q[0];
            if (1 + 4 * k > len) break;
            for (int i = 0; i < k; i++) {
                uint16_t m = bt_le16(q + 1 + 2 * i) & 0x0FFF;
                uint16_t c = bt_le16(q + 1 + 2 * k + 2 * i);
                int *s = sinack_de(m, false);
                if (s) *s = *s > c ? *s - c : 0;
                acl_libres += c;
            }
            if (acl_libres > acl_num) acl_libres = acl_num;
            drenar_acl();
        }
        break;
    case EV_PEDIDO_CHAVE:
        if (len >= 6) {
            uint8_t r[22];
            memcpy(r, q, 6);
            if (bt_g_chave_pedida(q, r + 6)) bt_hci_cmd(OP_CHAVE_SIM, r, 22);
            else bt_hci_cmd(OP_CHAVE_NAO, r, 6);
        }
        break;
    case EV_NOVA_CHAVE:
        if (len >= 23) bt_g_chave_nova(q, q + 6, q[22]);
        break;
    case EV_PEDIDO_PIN: if (len >= 6) bt_g_pin_pedido(q); break;
    case EV_PEDIDO_IO:
        if (len >= 6) {
            uint8_t r[9];
            memcpy(r, q, 6);
            if (bt_g_io_pedido(q)) {
                r[6] = 0x01;   /* DisplayYesNo */
                r[7] = 0x00;   /* sin datos OOB */
                r[8] = 0x04;   /* vínculo general, sin MITM obligatorio */
                bt_hci_cmd(OP_IO_SIM, r, 9);
            } else {
                r[6] = 0x18;   /* emparejamiento no permitido */
                bt_hci_cmd(OP_IO_NAO, r, 7);
            }
        }
        break;
    case EV_RESPOSTA_IO: if (len >= 9) bt_g_io_resposta(q, q[6]); break;
    case EV_CONFIRMAR: if (len >= 10) bt_g_confirmar_pedido(q, (uint32_t)(q[6] | q[7] << 8 | q[8] << 16 | (uint32_t)q[9] << 24)); break;
    case EV_PASSKEY_MOSTRAR:
        if (len >= 10) bt_g_passkey_mostrar(q, (uint32_t)(q[6] | q[7] << 8 | q[8] << 16 | (uint32_t)q[9] << 24));
        break;
    case EV_SSP_FIM: if (len >= 7) bt_g_ssp_fim(q + 1, q[0]); break;
    default: break;   /* cambio de papel, ranuras máximas, modo... no nos interesan */
    }
}

/* Órdenes que no contestan: tras TIEMPO_CMD_MS se devuelve el crédito (o el inicio falla). */
void bt_hci_tic(void)
{
    if (!plat) return;
    uint32_t ahora = bt_agora();
    if (cred_cmd <= 0 && (ahora - cmd_envio) > TIEMPO_CMD_MS) {
        info.erros_hci++;
        if (info.estado == BT_INICIANDO) {
            info.estado = BT_FALHOU;
            bt_log("[BT] el controlador no contesta a la orden %04x", cmd_op);
            bt_g_set_estado_adaptador(BT_FALHOU);
            return;
        }
        bt_log("[BT] la orden %04x no obtuvo respuesta", cmd_op);
        cred_cmd = 1;
    }
    /* Vigilancia del inicio: el crédito volvió (Command Status) pero el paso nunca acabó. */
    if (info.estado == BT_INICIANDO && passo < P_FIM && cred_cmd > 0 && !c_cnt && (ahora - paso_desde) > TIEMPO_CMD_MS) {
        uint16_t op = paso_op[passo];
        info.erros_hci++;
        if (paso_critico(passo)) {
            info.estado = BT_FALHOU;
            bt_log("[BT] inicio: %s (%04x) no acabo nunca", nome_op(op), op);
            bt_g_set_estado_adaptador(BT_FALHOU);
            return;
        }
        bt_log("[BT] inicio: %s (%04x) no acabo: se sigue", nome_op(op), op);
        passo++;
        init_paso();
    }
    enviar_cmds();
    drenar_acl();
}

void bt_hci_info(bt_info_t *i)
{
    uint8_t es = info.estado;
    bool energia = info.energia;
    *i = info;
    i->estado = es;
    i->energia = energia;
    i->acl_libres = (uint8_t)(acl_libres > 255 ? 255 : acl_libres);
}
