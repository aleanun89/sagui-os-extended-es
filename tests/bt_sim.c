// SPDX-License-Identifier: GPL-2.0-or-later
/* bt_sim.c - ver bt_sim.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bt_sim.h"

sim_cfg_t sim_cfg;
sim_remoto_t sim_remotos[SIM_MAX_REMOTOS];
int sim_n_remotos;
uint32_t sim_ms;
int sim_acl_creditos_vistos, sim_cmds, sim_acl_tx;

char sim_conf[4096];
int sim_conf_n, sim_guardados;
uint8_t sim_rep_teclado[64][8];
int sim_n_teclado;
uint8_t sim_rep_raton[64][8];
int sim_rep_raton_n[64];
bool sim_rep_raton_boot[64];
int sim_n_raton;
int sim_soltar, sim_hid_conectar_n, sim_hid_devuelve, sim_hid_desc_n;
char sim_log[8192];

static const uint8_t ADDR_CTL[6] = { 0x01, 0x00, 0x00, 0xA0, 0x12, 0x0A };   /* ya en orden HCI */

/* ------------------------------------------------------------------ cola hacia la pila */
typedef struct { bool acl; int n; uint8_t d[1100]; } paq_t;
#define COLA 512
static paq_t cola[COLA];
static int q_cab, q_cnt;
static int en_vuelo;         /* ACL enviados por la pila sin completar */
static int cred_cmd = 1;
static uint16_t ret[64];
static int n_ret;
static uint8_t rbuf[SIM_MAX_REMOTOS][700];
static int rn[SIM_MAX_REMOTOS];

static void q_poner(bool acl, const uint8_t *d, int n)
{
    if (q_cnt >= COLA) { fprintf(stderr, "sim: cola llena\n"); exit(2); }
    paq_t *p = &cola[(q_cab + q_cnt++) % COLA];
    p->acl = acl;
    p->n = n;
    memcpy(p->d, d, (size_t)n);
}

static void q_evento(uint8_t code, const void *par, int n)
{
    uint8_t b[300];
    b[0] = code;
    b[1] = (uint8_t)n;
    if (n) memcpy(b + 2, par, (size_t)n);
    q_poner(false, b, n + 2);
}

static void q_cc(uint16_t op, const void *ret, int n)   /* Command Complete */
{
    uint8_t b[260];
    b[0] = (uint8_t)cred_cmd;
    b[1] = (uint8_t)op; b[2] = (uint8_t)(op >> 8);
    if (n) memcpy(b + 3, ret, (size_t)n);
    q_evento(EV_CMD_COMPLETO, b, 3 + n);
}

static void q_cs(uint16_t op, uint8_t st)   /* Command Status */
{
    uint8_t b[4] = { st, (uint8_t)cred_cmd, (uint8_t)op, (uint8_t)(op >> 8) };
    q_evento(EV_CMD_ESTADO, b, 4);
}

void sim_evento_crudo(const uint8_t *p, int n) { q_poner(false, p, n); }
void sim_acl_crudo(const uint8_t *p, int n) { q_poner(true, p, n); }

void sim_bombear(void)
{
    while (q_cnt) {
        paq_t p = cola[q_cab];
        q_cab = (q_cab + 1) % COLA;
        q_cnt--;
        if (p.acl) bt_hci_acl_entrada(p.d, p.n);
        else bt_hci_evento(p.d, p.n);
    }
}

void sim_avanzar(uint32_t ms)
{
    while (ms) {
        uint32_t paso = ms > 100 ? 100 : ms;
        sim_ms += paso;
        ms -= paso;
        bt_hci_tic();
        bt_l2cap_tic();
        bt_g_tic();
        sim_bombear();
    }
    sim_bombear();
}

/* ------------------------------------------------------------------ remotos */

static sim_remoto_t *por_addr(const uint8_t *a)
{
    for (int i = 0; i < sim_n_remotos; i++) if (!memcmp(sim_remotos[i].addr, a, 6)) return &sim_remotos[i];
    return NULL;
}

static sim_remoto_t *por_manejador(uint16_t m)
{
    for (int i = 0; i < sim_n_remotos; i++) if (sim_remotos[i].ligado && sim_remotos[i].manejador == m) return &sim_remotos[i];
    return NULL;
}

static sim_remoto_t *nuevo(const char *nome, uint8_t b5, uint32_t cod, int io)
{
    sim_remoto_t *r = &sim_remotos[sim_n_remotos++];
    memset(r, 0, sizeof *r);
    uint8_t a[6] = { b5, 0x55, 0x44, 0x33, 0x22, 0x11 };
    memcpy(r->addr, a, 6);
    strncpy(r->nome, nome, sizeof r->nome - 1);
    r->cod = cod;
    r->io = io;
    r->presente = true;
    r->inquiry_ext = false;
    return r;
}

sim_remoto_t *sim_teclado(void) { return &sim_remotos[0]; }
sim_remoto_t *sim_raton(void) { return &sim_remotos[1]; }
sim_remoto_t *sim_combo(void) { return &sim_remotos[2]; }

void sim_reiniciar(void)
{
    memset(&sim_cfg, 0, sizeof sim_cfg);
    sim_cfg.acl_num = 8;
    sim_cfg.acl_mtu = 310;
    sim_cfg.inq_formato = 0;
    q_cab = q_cnt = 0;
    en_vuelo = 0;
    n_ret = 0;
    memset(rn, 0, sizeof rn);
    cred_cmd = 1;
    sim_n_remotos = 0;
    sim_acl_creditos_vistos = sim_cmds = sim_acl_tx = 0;
    sim_conf_n = sim_guardados = 0;
    sim_n_teclado = sim_n_raton = sim_soltar = sim_hid_conectar_n = 0;
    sim_hid_devuelve = 0;
    sim_hid_desc_n = -1;
    sim_log[0] = 0;
    sim_ms = 100000;
    sim_remoto_t *k = nuevo("Teclado BT", 0x01, 0x000540, 2);
    k->inquiry_ext = true;
    k->subclasse = 0x40;
    sim_remoto_t *m = nuevo("Raton BT", 0x02, 0x000580, 3);
    m->subclasse = 0x80;
    m->descritor_raton = true;
    sim_remoto_t *c = nuevo("Combo BT", 0x03, 0x0005C0, 1);
    c->subclasse = 0xC0;
    c->sdp_fragmentado = true;
    c->inquiry_ext = true;
}

/* L2CAP del remoto -> host por ACL, con fragmentación según el búfer del controlador. */
static void r_acl(sim_remoto_t *r, uint16_t cid, const uint8_t *d, int n)
{
    uint8_t f[2000];
    f[0] = (uint8_t)n; f[1] = (uint8_t)(n >> 8);
    f[2] = (uint8_t)cid; f[3] = (uint8_t)(cid >> 8);
    memcpy(f + 4, d, (size_t)n);
    int total = n + 4, pos = 0;
    bool primero = true;
    while (pos < total) {
        int l = total - pos > sim_cfg.acl_mtu ? sim_cfg.acl_mtu : total - pos;
        uint8_t p[1100];
        uint16_t hf = (uint16_t)(r->manejador | (primero ? 0x2000 : 0x1000));
        p[0] = (uint8_t)hf; p[1] = (uint8_t)(hf >> 8);
        p[2] = (uint8_t)l; p[3] = (uint8_t)(l >> 8);
        memcpy(p + 4, f + pos, (size_t)l);
        q_poner(true, p, 4 + l);
        pos += l;
        primero = false;
    }
}

static void r_sinal(sim_remoto_t *r, uint8_t code, uint8_t id, const uint8_t *b, int n)
{
    uint8_t p[300];
    p[0] = code; p[1] = id; p[2] = (uint8_t)n; p[3] = (uint8_t)(n >> 8);
    if (n) memcpy(p + 4, b, (size_t)n);
    r_acl(r, 1, p, n + 4);
}

static int r_canal_por_cid(sim_remoto_t *r, uint16_t cid)
{
    for (int i = 0; i < r->ncid; i++) if (r->cid[i] == cid) return i;
    return -1;
}

static int r_canal_psm(sim_remoto_t *r, uint16_t psm)
{
    for (int i = 0; i < r->ncid; i++) if (r->psm[i] == psm && r->cid[i]) return i;
    return -1;
}

static uint16_t r_nuevo_cid(sim_remoto_t *r, uint16_t hcid, uint16_t psm)
{
    int i = r->ncid++;
    r->cid[i] = (uint16_t)(0x0050 + i + (r - sim_remotos) * 8);
    r->hcid[i] = hcid;
    r->psm[i] = psm;
    r->config_ok[i] = false;
    return r->cid[i];
}

/* ------------------------------------------------------------------ SDP del remoto */

static int de_uuid16(uint8_t *p, uint16_t u) { p[0] = 0x19; p[1] = (uint8_t)(u >> 8); p[2] = (uint8_t)u; return 3; }
static int de_u16(uint8_t *p, uint16_t u) { p[0] = 0x09; p[1] = (uint8_t)(u >> 8); p[2] = (uint8_t)u; return 3; }
static int de_u8(uint8_t *p, uint8_t u) { p[0] = 0x08; p[1] = u; return 2; }
/* Secuencia con longitud de 2 bytes que se cierra con seq_fin. */
static int seq_ini(uint8_t *p, int k) { p[k] = 0x36; p[k + 1] = 0; p[k + 2] = 0; return k + 3; }
static void seq_fin(uint8_t *p, int ini, int k) { int l = k - ini - 3; p[ini + 1] = (uint8_t)(l >> 8); p[ini + 2] = (uint8_t)l; }

static const uint8_t DESC_RATON[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x03, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38,
    0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06, 0xC0, 0xC0 };

/* La lista de listas de atributos de la respuesta: un registro HID. */
static int sdp_registro(sim_remoto_t *r, uint8_t *o)
{
    int k = 0, a, b, c;
    int ext = seq_ini(o, k);
    int reg = ext;
    ext = seq_ini(o, reg);
    k = ext;
    if (r->sdp_sem_hid) { seq_fin(o, reg, k); seq_fin(o, 0, k); return k; }
    k += de_u16(o + k, 0x0004);
    a = k; k = seq_ini(o, k);
    b = k; k = seq_ini(o, k); k += de_uuid16(o + k, 0x0100); k += de_u16(o + k, 0x0011); seq_fin(o, b, k);
    b = k; k = seq_ini(o, k); k += de_uuid16(o + k, 0x0011); seq_fin(o, b, k);
    seq_fin(o, a, k);
    k += de_u16(o + k, 0x000D);
    a = k; k = seq_ini(o, k);
    b = k; k = seq_ini(o, k);
    c = k; k = seq_ini(o, k); k += de_uuid16(o + k, 0x0100); k += de_u16(o + k, 0x0013); seq_fin(o, c, k);
    c = k; k = seq_ini(o, k); k += de_uuid16(o + k, 0x0011); seq_fin(o, c, k);
    seq_fin(o, b, k);
    seq_fin(o, a, k);
    k += de_u16(o + k, 0x0100);
    int nl = (int)strlen(r->nome);
    o[k++] = 0x25; o[k++] = (uint8_t)nl; memcpy(o + k, r->nome, (size_t)nl); k += nl;
    k += de_u16(o + k, 0x0202); k += de_u8(o + k, r->subclasse);
    k += de_u16(o + k, 0x020E); o[k++] = 0x28; o[k++] = 1;
    if (r->descritor_raton) {
        k += de_u16(o + k, 0x0206);
        a = k; k = seq_ini(o, k);
        b = k; k = seq_ini(o, k);
        k += de_u8(o + k, 0x22);
        o[k++] = 0x25; o[k++] = (uint8_t)sizeof DESC_RATON;
        memcpy(o + k, DESC_RATON, sizeof DESC_RATON); k += (int)sizeof DESC_RATON;
        seq_fin(o, b, k);
        seq_fin(o, a, k);
    }
    seq_fin(o, reg, k);
    seq_fin(o, 0, k);
    return k;
}

static void sdp_responder(sim_remoto_t *r, int canal, const uint8_t *d, int n)
{
    (void)canal;
    if (r->ignora_sdp || n < 5) return;
    uint8_t resp[700];
    uint16_t tid = (uint16_t)(d[1] << 8 | d[2]);
    if (r->sdp_erro) {
        resp[0] = 0x01; resp[1] = (uint8_t)(tid >> 8); resp[2] = (uint8_t)tid; resp[3] = 0; resp[4] = 2; resp[5] = 0; resp[6] = 3;
        r_acl(r, r->hcid[canal], resp, 7);
        return;
    }
    uint8_t attr[600];
    int tam = sdp_registro(r, attr);
    int pl = (d[3] << 8) | d[4];
    int cl = pl - 28;   /* bytes del estado de continuación: el cuerpo fijo son 27 + 1 */
    int inicio = 0;
    if (cl == 2) inicio = 150;
    int trozo = r->sdp_fragmentado ? 150 : 1000;
    int resto = tam - inicio;
    int n_env = resto > trozo ? trozo : resto;
    int p = 0;
    resp[p++] = 0x07;
    resp[p++] = (uint8_t)(tid >> 8); resp[p++] = (uint8_t)tid;
    bool mas = resto > n_env;
    int plen = 2 + n_env + 1 + (mas ? 2 : 0);
    resp[p++] = (uint8_t)(plen >> 8); resp[p++] = (uint8_t)plen;
    resp[p++] = (uint8_t)(n_env >> 8); resp[p++] = (uint8_t)n_env;
    memcpy(resp + p, attr + inicio, (size_t)n_env); p += n_env;
    if (mas) { resp[p++] = 2; resp[p++] = 0xAB; resp[p++] = 0xCD; }
    else resp[p++] = 0;
    r_acl(r, r->hcid[canal], resp, p);
}

/* ------------------------------------------------------------------ L2CAP del remoto */

static bool r_reconectando[SIM_MAX_REMOTOS];
static void r_conn_req(sim_remoto_t *r, uint16_t psm);

static void r_config_req(sim_remoto_t *r, int c)
{
    uint8_t b[8];
    b[0] = (uint8_t)r->hcid[c]; b[1] = (uint8_t)(r->hcid[c] >> 8);
    b[2] = b[3] = 0;
    b[4] = 1; b[5] = 2; b[6] = (uint8_t)(672 & 255); b[7] = (uint8_t)(672 >> 8);
    r_sinal(r, 4, ++r->id_sinal, b, 8);
}

static void r_sinal_recibida(sim_remoto_t *r, uint8_t code, uint8_t id, const uint8_t *d, int n)
{
    uint8_t b[16];
    switch (code) {
    case 2: {   /* Connection Request del host */
        uint16_t psm = (uint16_t)(d[0] | d[1] << 8), hcid = (uint16_t)(d[2] | d[3] << 8);
        bool hid = psm == 0x11 || psm == 0x13;
        if (hid && r->recusa_hid) {
            b[0] = b[1] = 0; b[2] = d[2]; b[3] = d[3]; b[4] = 3; b[5] = 0; b[6] = b[7] = 0;   /* seguridad */
            r_sinal(r, 3, id, b, 8);
            return;
        }
        uint16_t cid = r_nuevo_cid(r, hcid, psm);
        b[0] = (uint8_t)cid; b[1] = (uint8_t)(cid >> 8); b[2] = d[2]; b[3] = d[3]; b[4] = 0; b[5] = 0; b[6] = b[7] = 0;
        r_sinal(r, 3, id, b, 8);
        r_config_req(r, r->ncid - 1);
        break;
    }
    case 3: {   /* Connection Response del host a una petición nuestra */
        int c = r_canal_por_cid(r, (uint16_t)(d[2] | d[3] << 8));
        if (c < 0 || (d[4] | d[5] << 8) != 0) return;
        r->hcid[c] = (uint16_t)(d[0] | d[1] << 8);
        r_config_req(r, c);
        break;
    }
    case 4: {   /* Config Request del host */
        uint16_t dcid = (uint16_t)(d[0] | d[1] << 8);
        int c = r_canal_por_cid(r, dcid);
        if (c < 0) return;
        b[0] = (uint8_t)r->hcid[c]; b[1] = (uint8_t)(r->hcid[c] >> 8); b[2] = b[3] = 0; b[4] = b[5] = 0;
        r_sinal(r, 5, id, b, 6);
        break;
    }
    case 5: {   /* Config Response: ya está configurado por nuestro lado */
        int c = r_canal_por_cid(r, (uint16_t)(d[0] | d[1] << 8));
        if (c >= 0) {
            r->config_ok[c] = true;
            if (r_reconectando[r - sim_remotos] && r->psm[c] == 0x11) {
                r_reconectando[r - sim_remotos] = false;
                r_conn_req(r, 0x13);
            }
        }
        break;
    }
    case 6: {   /* Disconnect Request */
        int c = r_canal_por_cid(r, (uint16_t)(d[0] | d[1] << 8));
        r_sinal(r, 7, id, d, 4);
        if (c >= 0) r->cid[c] = 0;
        break;
    }
    case 7: break;
    case 8: r_sinal(r, 9, id, d, n); break;
    default: break;
    }
}

static void r_datos(sim_remoto_t *r, uint16_t cid, const uint8_t *d, int n)
{
    int c = r_canal_por_cid(r, cid);
    if (c < 0 || !n) return;
    if (r->psm[c] == 1) { sdp_responder(r, c, d, n); return; }
    if (r->psm[c] == 0x11) {
        uint8_t t = d[0] >> 4;
        if (t == 7) {   /* SET_PROTOCOL */
            r->protocolo_boot = (d[0] & 1) == 0;
            r->n_set_protocol++;
            uint8_t h = 0x00;   /* HANDSHAKE: éxito */
            r_acl(r, r->hcid[c], &h, 1);
        }
    }
}

static void r_acl_recibido(sim_remoto_t *r, const uint8_t *p, int n)
{
    if (n < 4) return;
    uint16_t len = (uint16_t)(p[0] | p[1] << 8), cid = (uint16_t)(p[2] | p[3] << 8);
    if (len + 4 != n) return;
    if (cid == 1) {
        for (int k = 0; k + 4 <= len;) {
            uint16_t l = (uint16_t)(p[4 + k + 2] | p[4 + k + 3] << 8);
            r_sinal_recibida(r, p[4 + k], p[4 + k + 1], p + 4 + k + 4, l);
            k += 4 + l;
        }
    } else r_datos(r, cid, p + 4, len);
}

void sim_teclear(sim_remoto_t *r, const uint8_t *inf, int n)
{
    int c = r_canal_psm(r, 0x13);
    if (c < 0) return;
    uint8_t b[40];
    b[0] = 0xA1;
    memcpy(b + 1, inf, (size_t)n);
    r_acl(r, r->hcid[c], b, n + 1);
    sim_bombear();
}

void sim_unplug(sim_remoto_t *r)
{
    int c = r_canal_psm(r, 0x11);
    if (c < 0) return;
    uint8_t m = 0x15;
    r_acl(r, r->hcid[c], &m, 1);
    sim_bombear();
}

void sim_desconectar_remoto(sim_remoto_t *r, uint8_t motivo)
{
    if (!r->ligado) return;
    uint8_t b[4] = { 0, (uint8_t)r->manejador, (uint8_t)(r->manejador >> 8), motivo };
    r->ligado = false;
    r->ncid = 0;
    q_evento(EV_DESCONEXAO, b, 4);
    sim_bombear();
}

static void r_conectado(sim_remoto_t *r, uint16_t m)
{
    r->ligado = true;
    r->manejador = m;
    r->ncid = 0;
    r->protocolo_boot = false;
    memset(r->cid, 0, sizeof r->cid);
    uint8_t b[11];
    b[0] = 0;
    b[1] = (uint8_t)m; b[2] = (uint8_t)(m >> 8);
    memcpy(b + 3, r->addr, 6);
    b[9] = 1; b[10] = 0;
    q_evento(EV_CONEXAO, b, 11);
}

static void r_conn_req(sim_remoto_t *r, uint16_t psm)
{
    uint16_t cid = r_nuevo_cid(r, 0, psm);
    uint8_t b[4] = { (uint8_t)psm, (uint8_t)(psm >> 8), (uint8_t)cid, (uint8_t)(cid >> 8) };
    r_sinal(r, 2, ++r->id_sinal, b, 4);
}

/* El dispositivo reconecta solo: Connection Request; si se acepta se autentica y abre sus dos canales. */

void sim_remoto_conecta(sim_remoto_t *r)
{
    uint8_t b[10];
    memcpy(b, r->addr, 6);
    b[6] = (uint8_t)r->cod; b[7] = (uint8_t)(r->cod >> 8); b[8] = (uint8_t)(r->cod >> 16);
    b[9] = 1;
    r_reconectando[r - sim_remotos] = true;
    q_evento(EV_PEDIDO_CONEX, b, 10);
    sim_bombear();
    if (!r->ligado) { r_reconectando[r - sim_remotos] = false; return; }
    q_evento(EV_PEDIDO_CHAVE, r->addr, 6);
    sim_bombear();
    r_conn_req(r, 0x11);
    sim_bombear();
}

void sim_passkey_tecleada(sim_remoto_t *r)
{
    if (!r->passkey_pendiente) return;
    r->passkey_pendiente = 0;
    uint8_t b[7];
    b[0] = 0; memcpy(b + 1, r->addr, 6);
    q_evento(EV_SSP_FIM, b, 7);
    uint8_t k[23];
    memcpy(k, r->addr, 6);
    for (int i = 0; i < 16; i++) k[6 + i] = (uint8_t)(0x40 + i + r->addr[0]);
    k[22] = 5;   /* clave combinada autenticada (P-192) */
    memcpy(r->chave, k + 6, 16);
    r->tem_chave = true;
    q_evento(EV_NOVA_CHAVE, k, 23);
    uint8_t a[3] = { 0, (uint8_t)r->manejador, (uint8_t)(r->manejador >> 8) };
    q_evento(EV_AUTENTICACAO, a, 3);
    sim_bombear();
}

/* ------------------------------------------------------------------ el controlador */

static void auth_ok(sim_remoto_t *r, uint8_t st)
{
    uint8_t a[3] = { st, (uint8_t)r->manejador, (uint8_t)(r->manejador >> 8) };
    q_evento(EV_AUTENTICACAO, a, 3);
}

static void emparejar_resultado(sim_remoto_t *r, bool ok)
{
    uint8_t b[7];
    if (!ok) {
        b[0] = 0x0B; memcpy(b + 1, r->addr, 6);
        q_evento(EV_SSP_FIM, b, 7);
        auth_ok(r, 0x05);
        return;
    }
    b[0] = 0; memcpy(b + 1, r->addr, 6);
    q_evento(EV_SSP_FIM, b, 7);
    uint8_t k[23];
    memcpy(k, r->addr, 6);
    for (int i = 0; i < 16; i++) k[6 + i] = (uint8_t)(0xA0 + i + r->addr[0]);
    k[22] = r->io == 3 ? 4 : 5;
    memcpy(r->chave, k + 6, 16);
    r->tem_chave = true;
    q_evento(EV_NOVA_CHAVE, k, 23);
    auth_ok(r, 0);
}

static void cmd(uint16_t op, const uint8_t *p, int n)
{
    sim_remoto_t *r;
    uint8_t b[300];
    if (sim_cfg.falla_op && sim_cfg.falla_op == op) {
        b[0] = (uint8_t)sim_cfg.falla_estado;
        q_cc(op, b, 1);
        return;
    }
    switch (op) {
    case OP_RESET: b[0] = 0; q_cc(op, b, 1); break;
    case OP_RD_VERSAO: b[0] = 0; b[1] = 6; b[2] = 0x34; b[3] = 0x12; b[4] = 6; b[5] = 0x0A; b[6] = 0; b[7] = 0x34; b[8] = 0x12; q_cc(op, b, 9); break;
    case OP_RD_ADDR: b[0] = 0; memcpy(b + 1, ADDR_CTL, 6); q_cc(op, b, 7); break;
    case OP_RD_BUFFER:
        b[0] = 0; b[1] = (uint8_t)sim_cfg.acl_mtu; b[2] = (uint8_t)(sim_cfg.acl_mtu >> 8); b[3] = 64;
        b[4] = (uint8_t)sim_cfg.acl_num; b[5] = (uint8_t)(sim_cfg.acl_num >> 8); b[6] = 1; b[7] = 0;
        q_cc(op, b, 8);
        break;
    case OP_WR_SSP: b[0] = sim_cfg.sin_ssp ? 0x11 : 0; q_cc(op, b, 1); break;
    case OP_SET_EVMASK: case OP_WR_INQMODE: case OP_WR_NOME: case OP_WR_CLASE: case OP_WR_SCAN:
        b[0] = 0; q_cc(op, b, 1);
        break;
    case OP_INQUIRY:
        q_cs(op, 0);
        for (int i = 0; i < sim_n_remotos; i++) {
            r = &sim_remotos[i];
            if (!r->presente) continue;
            if (r->inquiry_ext) {
                uint8_t e[1 + 6 + 1 + 1 + 3 + 2 + 1 + 240];
                memset(e, 0, sizeof e);
                e[0] = 1; memcpy(e + 1, r->addr, 6); e[7] = 1;
                e[9] = (uint8_t)r->cod; e[10] = (uint8_t)(r->cod >> 8); e[11] = (uint8_t)(r->cod >> 16);
                e[14] = (uint8_t)-52;
                int nl = (int)strlen(r->nome);
                e[15] = (uint8_t)(nl + 1); e[16] = 0x09; memcpy(e + 17, r->nome, (size_t)nl);
                q_evento(EV_INQ_EXT, e, 15 + 240);
            } else if (sim_cfg.inq_formato == 1) {
                uint8_t e[1 + 14];
                memset(e, 0, sizeof e);
                e[0] = 1; memcpy(e + 1, r->addr, 6); e[7] = 1;
                e[10] = (uint8_t)r->cod; e[11] = (uint8_t)(r->cod >> 8); e[12] = (uint8_t)(r->cod >> 16);
                q_evento(EV_INQ_RES, e, 15);
            } else {
                uint8_t e[1 + 14];
                memset(e, 0, sizeof e);
                e[0] = 1; memcpy(e + 1, r->addr, 6); e[7] = 1;
                e[9] = (uint8_t)r->cod; e[10] = (uint8_t)(r->cod >> 8); e[11] = (uint8_t)(r->cod >> 16);
                e[14] = (uint8_t)-60;
                q_evento(EV_INQ_RSSI, e, 15);
            }
        }
        b[0] = 0;
        q_evento(EV_INQ_FIM, b, 1);
        break;
    case OP_INQUIRY_CANC: b[0] = 0; q_cc(op, b, 1); break;
    case OP_NOME_REMOTO:
        q_cs(op, 0);
        r = por_addr(p);
        b[0] = r && r->presente ? 0 : 0x04;
        memcpy(b + 1, p, 6);
        memset(b + 7, 0, 248);
        if (r) strncpy((char *)b + 7, r->nome, 247);
        q_evento(EV_NOME_REMOTO, b, 255);
        break;
    case OP_CRIAR_CONEX:
        q_cs(op, 0);
        r = por_addr(p);
        if (!r || !r->presente || r->falha_conexao) {
            b[0] = r && r->falha_conexao ? (uint8_t)r->falha_conexao : 0x04;
            b[1] = b[2] = 0; memcpy(b + 3, p, 6); b[9] = 1; b[10] = 0;
            q_evento(EV_CONEXAO, b, 11);
        } else r_conectado(r, (uint16_t)(0x0100 + (r - sim_remotos)));
        break;
    case OP_CANCELAR_CONEX: b[0] = 0; memcpy(b + 1, p, 6); q_cc(op, b, 7); break;
    case OP_ACEITAR_CONEX:
        q_cs(op, 0);
        r = por_addr(p);
        if (r) r_conectado(r, (uint16_t)(0x0100 + (r - sim_remotos)));
        break;
    case OP_REJEITAR_CONEX: q_cs(op, 0); break;
    case OP_AUTENTICAR:
        q_cs(op, 0);
        r = por_manejador((uint16_t)(p[0] | p[1] << 8));
        if (r) q_evento(EV_PEDIDO_CHAVE, r->addr, 6);
        break;
    case OP_CHAVE_SIM:
        r = por_addr(p);
        b[0] = 0; memcpy(b + 1, p, 6); q_cc(op, b, 7);
        if (r) auth_ok(r, r->tem_chave && !memcmp(r->chave, p + 6, 16) ? 0 : 0x06);
        break;
    case OP_CHAVE_NAO:
        r = por_addr(p);
        b[0] = 0; memcpy(b + 1, p, 6); q_cc(op, b, 7);
        if (r) {
            if (sim_cfg.sin_ssp) { q_evento(EV_PEDIDO_PIN, r->addr, 6); break; }
            q_evento(EV_PEDIDO_IO, r->addr, 6);
        }
        break;
    case OP_IO_SIM:
        r = por_addr(p);
        b[0] = 0; memcpy(b + 1, p, 6); q_cc(op, b, 7);
        if (r) {
            uint8_t io[9];
            memcpy(io, r->addr, 6); io[6] = (uint8_t)r->io; io[7] = 0; io[8] = 4;
            q_evento(EV_RESPOSTA_IO, io, 9);
            uint8_t cf[10];
            memcpy(cf, r->addr, 6);
            uint32_t num = 123456;
            cf[6] = (uint8_t)num; cf[7] = (uint8_t)(num >> 8); cf[8] = (uint8_t)(num >> 16); cf[9] = (uint8_t)(num >> 24);
            if (r->io == 2) { r->passkey_pendiente = 1; q_evento(EV_PASSKEY_MOSTRAR, cf, 10); }
            else q_evento(EV_CONFIRMAR, cf, 10);
        }
        break;
    case OP_IO_NAO:
        r = por_addr(p);
        b[0] = 0; memcpy(b + 1, p, 6); q_cc(op, b, 7);
        if (r) { b[0] = 0x18; memcpy(b + 1, r->addr, 6); q_evento(EV_SSP_FIM, b, 7); auth_ok(r, 0x18); }
        break;
    case OP_CONF_SIM:
        r = por_addr(p);
        b[0] = 0; memcpy(b + 1, p, 6); q_cc(op, b, 7);
        if (r) emparejar_resultado(r, true);
        break;
    case OP_CONF_NAO:
        r = por_addr(p);
        b[0] = 0; memcpy(b + 1, p, 6); q_cc(op, b, 7);
        if (r) emparejar_resultado(r, false);
        break;
    case OP_PIN_SIM:
        r = por_addr(p);
        b[0] = 0; memcpy(b + 1, p, 6); q_cc(op, b, 7);
        if (r) {
            uint8_t k[23];
            memcpy(k, r->addr, 6);
            for (int i = 0; i < 16; i++) k[6 + i] = (uint8_t)(0x10 + i);
            k[22] = 0;
            memcpy(r->chave, k + 6, 16);
            r->tem_chave = true;
            q_evento(EV_NOVA_CHAVE, k, 23);
            auth_ok(r, 0);
        }
        break;
    case OP_PIN_NAO: b[0] = 0; memcpy(b + 1, p, 6); q_cc(op, b, 7); r = por_addr(p); if (r) auth_ok(r, 0x06); break;
    case OP_CIFRAR:
        q_cs(op, 0);
        b[0] = 0; b[1] = p[0]; b[2] = p[1]; b[3] = 1;
        q_evento(EV_CIFRADO, b, 4);
        break;
    case OP_DESCONECTAR:
        q_cs(op, 0);
        r = por_manejador((uint16_t)(p[0] | p[1] << 8));
        if (r) {
            b[0] = 0; b[1] = p[0]; b[2] = p[1]; b[3] = 0x16;   /* terminada por el anfitrión local */
            r->ligado = false;
            r->ncid = 0;
            q_evento(EV_DESCONEXAO, b, 4);
        }
        break;
    default:
        b[0] = 0x01;   /* orden desconocida */
        q_cc(op, b, 1);
        break;
    }
    (void)n;
}

void sim_soltar_acl(void)
{
    for (int i = 0; i < n_ret; i++) {
        uint8_t b[5] = { 1, (uint8_t)ret[i], (uint8_t)(ret[i] >> 8), 1, 0 };
        q_evento(EV_PACOTES_COMP, b, 5);
        en_vuelo--;
    }
    n_ret = 0;
    sim_bombear();
}

static int enviar_cmd(const uint8_t *p, int n)
{
    sim_cmds++;
    if (n < 3 || p[2] != n - 3) { fprintf(stderr, "sim: orden mal formada\n"); exit(2); }
    if (cred_cmd < 1) { fprintf(stderr, "sim: la pila mando una orden sin credito\n"); exit(2); }
    if (sim_cfg.mudo) return 0;
    cmd((uint16_t)(p[0] | p[1] << 8), p + 3, n - 3);
    return 0;
}

static int enviar_acl(const uint8_t *p, int n)
{
    sim_acl_tx++;
    if (n < 4) { fprintf(stderr, "sim: ACL corto\n"); exit(2); }
    int len = p[2] | p[3] << 8;
    if (len + 4 != n || len > sim_cfg.acl_mtu) { fprintf(stderr, "sim: ACL mal formado (%d, mtu %d)\n", len, sim_cfg.acl_mtu); exit(2); }
    if (++en_vuelo > sim_cfg.acl_num) { fprintf(stderr, "sim: la pila excedio los buferes ACL del controlador\n"); exit(2); }
    if (en_vuelo > sim_acl_creditos_vistos) sim_acl_creditos_vistos = en_vuelo;
    uint16_t m = (uint16_t)((p[0] | p[1] << 8) & 0x0FFF);
    sim_remoto_t *r = por_manejador(m);
    if (sim_cfg.retener_acl) {   /* el controlador no confirma hasta sim_soltar_acl */
        if (n_ret < 64) ret[n_ret++] = m;
    } else {
        uint8_t b[5] = { 1, (uint8_t)m, (uint8_t)(m >> 8), 1, 0 };   /* Number of Completed Packets */
        en_vuelo--;
        q_evento(EV_PACOTES_COMP, b, 5);
    }
    if (r) {
        int k = (int)(r - sim_remotos);
        uint8_t pb = (uint8_t)(((p[0] | p[1] << 8) >> 12) & 3);
        if (pb == 2) {
            if (len > (int)sizeof rbuf[k]) { fprintf(stderr, "sim: trama demasiado grande\n"); exit(2); }
            memcpy(rbuf[k], p + 4, (size_t)len);
            rn[k] = len;
        } else if (pb == 1) {
            if (rn[k] + len > (int)sizeof rbuf[k]) { fprintf(stderr, "sim: trama demasiado grande\n"); exit(2); }
            memcpy(rbuf[k] + rn[k], p + 4, (size_t)len);
            rn[k] += len;
        } else { fprintf(stderr, "sim: PB %d inesperado\n", pb); exit(2); }
        if (rn[k] >= 4) {
            int total = (rbuf[k][0] | rbuf[k][1] << 8) + 4;
            if (rn[k] == total) { int nn = rn[k]; rn[k] = 0; r_acl_recibido(r, rbuf[k], nn); }
            else if (rn[k] > total) { fprintf(stderr, "sim: trama con datos de mas\n"); exit(2); }
        }
    }
    return 0;
}

static void p_log(const char *t)
{
    size_t l = strlen(sim_log);
    if (l + strlen(t) + 2 < sizeof sim_log) { strcat(sim_log, t); strcat(sim_log, "\n"); }
}

static uint32_t p_ms(void) { return sim_ms; }

static void p_guardar(const char *t, int n)
{
    if (n > (int)sizeof sim_conf - 1) n = (int)sizeof sim_conf - 1;
    memcpy(sim_conf, t, (size_t)n);
    sim_conf[n] = 0;
    sim_conf_n = n;
    sim_guardados++;
}

static int p_hid_conectar(int tipo, const uint8_t *d, int n)
{
    (void)tipo;
    sim_hid_conectar_n++;
    sim_hid_desc_n = d ? n : -1;
    return tipo == BT_T_RATO ? sim_hid_devuelve : 0;
}

static void p_teclado(const uint8_t *r, int n)
{
    (void)n;
    if (sim_n_teclado < 64) memcpy(sim_rep_teclado[sim_n_teclado++], r, 8);
}

static void p_raton(const uint8_t *r, int n, bool boot)
{
    if (sim_n_raton < 64) {
        memcpy(sim_rep_raton[sim_n_raton], r, (size_t)(n > 8 ? 8 : n));
        sim_rep_raton_n[sim_n_raton] = n;
        sim_rep_raton_boot[sim_n_raton++] = boot;
    }
}

static void p_soltar(int tipo) { (void)tipo; sim_soltar++; }

bt_plat_t sim_plat = {
    .enviar_cmd = enviar_cmd, .enviar_acl = enviar_acl, .ms = p_ms, .guardar = p_guardar,
    .hid_conectar = p_hid_conectar, .teclado = p_teclado, .raton = p_raton, .soltar_entrada = p_soltar, .registro = p_log,
};
