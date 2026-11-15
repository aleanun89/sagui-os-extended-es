// SPDX-License-Identifier: GPL-2.0-or-later
/* bt_gestor.c - el gestor Bluetooth (D279): tabla de dispositivos, flujo de conexión y emparejamiento,
 * claves de enlace guardadas y operaciones que usan la llamada SIS_BLUETOOTH y el terminal.
 *
 * Flujo de una conexión que empezamos nosotros (teclado o ratón):
 *   Create_Connection -> Authentication_Requested (SSP: Just Works o confirmación numérica; el
 *   controlador pide la clave guardada con Link Key Request) -> Set_Connection_Encryption ->
 *   L2CAP PSM 1 (SDP): servicio HID -> L2CAP PSM de control -> L2CAP PSM de interrupción ->
 *   HIDP SET_PROTOCOL(boot) (o protocolo normal si el ratón se entiende por su descriptor).
 * Los teclados reconectan solos: llegan como Connection Request, y si tenemos su clave se aceptan
 * y esperan los dos canales L2CAP que abre el dispositivo. */
#include "bt.h"

#define T_LIGANDO     20000
#define T_AUTENTICAR  60000
#define T_CIFRAR      15000
#define T_SDP         15000
#define T_HID         15000
#define T_CONFIRMAR   40000

static bt_disp_t disp[BT_MAX_DISP];
static bool buscando;
static uint32_t busca_ini;
static uint8_t conf_modo = BT_CONF_NADA;
static uint32_t conf_num, conf_desde;
static int conf_idx = -1;
static char erro_ad[64];
static int est_ad = BT_SEM_ADAPTADOR;
static bool energia = true;
static bool nome_pedido;          /* un Remote_Name_Request en curso */
static bt_sdp_hid_t sdp_res;
static int sdp_dono = -1;
static char conf_texto[BT_CONF_MAX];

void bt_g_iniciar(void)
{
    memset(disp, 0, sizeof disp);
    buscando = false;
    conf_modo = BT_CONF_NADA;
    conf_idx = -1;
    erro_ad[0] = 0;
    nome_pedido = false;
    energia = true;
    sdp_dono = -1;
    bt_sdp_reiniciar();
}

int bt_n_disp(void)
{
    int n = 0;
    for (int i = 0; i < BT_MAX_DISP; i++) if (disp[i].usado) n++;
    return n;
}

/* El i-ésimo dispositivo usado (en orden de la tabla). */
const bt_disp_t *bt_disp(int i)
{
    for (int k = 0; k < BT_MAX_DISP; k++) {
        if (!disp[k].usado) continue;
        if (i-- == 0) return &disp[k];
    }
    return NULL;
}

int bt_g_indice(const bt_disp_t *d)
{
    int n = 0;
    for (int k = 0; k < BT_MAX_DISP; k++) {
        if (!disp[k].usado) continue;
        if (&disp[k] == d) return n;
        n++;
    }
    return -1;
}

static bt_disp_t *disp_n(int i) { return (bt_disp_t *)bt_disp(i); }

bt_disp_t *bt_g_por_addr(const uint8_t *addr, bool criar)
{
    for (int i = 0; i < BT_MAX_DISP; i++)
        if (disp[i].usado && !memcmp(disp[i].addr, addr, 6)) return &disp[i];
    if (!criar) return NULL;
    int k = -1;
    for (int i = 0; i < BT_MAX_DISP && k < 0; i++) if (!disp[i].usado) k = i;
    for (int i = 0; i < BT_MAX_DISP && k < 0; i++)   /* tabla llena: se sustituye uno visto hace tiempo y sin clave */
        if (!disp[i].tem_chave && !disp[i].visto && disp[i].estado == BT_D_LIVRE) k = i;
    for (int i = 0; i < BT_MAX_DISP && k < 0; i++)
        if (!disp[i].tem_chave && disp[i].estado == BT_D_LIVRE) k = i;
    if (k < 0) return NULL;
    memset(&disp[k], 0, sizeof disp[k]);
    disp[k].usado = true;
    memcpy(disp[k].addr, addr, 6);
    disp[k].manejador = 0xFFFF;
    disp[k].canal_sdp = disp[k].canal_ctl = disp[k].canal_int = -1;
    return &disp[k];
}

bt_disp_t *bt_g_por_manejador(uint16_t m)
{
    for (int i = 0; i < BT_MAX_DISP; i++)
        if (disp[i].usado && disp[i].manejador == m) return &disp[i];
    return NULL;
}

bt_disp_t *bt_g_por_canal(int canal)
{
    const bt_canal_t *c = bt_l2cap_canal(canal);
    return c ? bt_g_por_manejador(c->manejador) : NULL;
}

/* ------------------------------------------------------------------ configuración */

int bt_g_serializar(char *s, int max)
{
    int k = 0;
    const char *cab = "# Bluetooth de SaguiOS (D279): claves de enlace. No lo compartas: da acceso a tus dispositivos.\n";
    int l = (int)bt_strlen(cab);
    if (l >= max) return 0;
    memcpy(s, cab, (size_t)l);
    k = l;
    if (k + 12 < max) { memcpy(s + k, energia ? "energia 1\n" : "energia 0\n", 10); k += 10; }
    for (int i = 0; i < BT_MAX_DISP; i++) {
        if (!disp[i].usado || !disp[i].tem_chave) continue;
        if (k + 120 + (int)bt_strlen(disp[i].nome) >= max) break;
        memcpy(s + k, "dispositivo ", 12); k += 12;
        k += bt_addr_texto(s + k, disp[i].addr);
        s[k++] = ' ';
        k += bt_hex(s + k, disp[i].chave, 16);
        s[k++] = ' ';
        s[k++] = (char)('0' + (disp[i].tipo_chave / 10) % 10);
        s[k++] = (char)('0' + disp[i].tipo_chave % 10);
        s[k++] = ' ';
        s[k++] = (char)('0' + disp[i].tipo);
        s[k++] = ' ';
        int nl = (int)bt_strlen(disp[i].nome);
        memcpy(s + k, disp[i].nome, (size_t)nl); k += nl;
        s[k++] = '\n';
    }
    s[k] = 0;
    return k;
}

void bt_g_mudou(void)
{
    const bt_plat_t *p = bt_plat();
    if (!p || !p->guardar) return;
    int n = bt_g_serializar(conf_texto, sizeof conf_texto);
    p->guardar(conf_texto, n);
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void bt_g_carregar(const char *t, int n)
{
    int i = 0;
    while (i < n) {
        int f = i;
        while (f < n && t[f] != '\n') f++;
        char l[160];
        int ll = f - i < (int)sizeof l - 1 ? f - i : (int)sizeof l - 1;
        memcpy(l, t + i, (size_t)ll);
        l[ll] = 0;
        i = f + 1;
        if (ll >= 9 && !memcmp(l, "energia ", 8)) { energia = l[8] != '0'; continue; }
        if (ll < 12 + 17 + 1 + 32 + 1 + 2 + 1 + 1 || memcmp(l, "dispositivo ", 12)) continue;
        uint8_t a[6], key[16];
        if (!bt_addr_ler(l + 12, a)) continue;
        const char *p = l + 12 + 17;
        if (*p++ != ' ') continue;
        bool ok = true;
        for (int k = 0; k < 16 && ok; k++) {
            int h = hexval(p[2 * k]), lo = hexval(p[2 * k + 1]);
            if (h < 0 || lo < 0) ok = false; else key[k] = (uint8_t)(h << 4 | lo);
        }
        if (!ok || p[32] != ' ') continue;
        p += 33;
        if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9' || p[2] != ' ' || p[3] < '0' || p[3] > '3' || p[4] != ' ') continue;
        bt_disp_t *d = bt_g_por_addr(a, true);
        if (!d) continue;
        memcpy(d->chave, key, 16);
        d->tem_chave = true;
        d->tipo_chave = (uint8_t)((p[0] - '0') * 10 + (p[1] - '0'));
        d->tipo = (uint8_t)(p[3] - '0');
        bt_copia(d->nome, p + 5, BT_NOME_MAX);
    }
}

/* ------------------------------------------------------------------ adaptador */

void bt_g_set_estado_adaptador(int e)
{
    est_ad = e;
    if (e == BT_FALHOU && !erro_ad[0]) bt_copia(erro_ad, "el controlador no completo el inicio", sizeof erro_ad);
}

bool bt_g_energia(void) { return energia; }

/* El controlador USB se retiró: todos los dispositivos quedan sin enlace. */
void bt_g_adaptador_retirado(void)
{
    est_ad = BT_SEM_ADAPTADOR;
    buscando = false;
    nome_pedido = false;
    conf_modo = BT_CONF_NADA;
    for (int i = 0; i < BT_MAX_DISP; i++) {
        if (!disp[i].usado) continue;
        disp[i].estado = BT_D_LIVRE;
        disp[i].manejador = 0xFFFF;
        disp[i].canal_sdp = disp[i].canal_ctl = disp[i].canal_int = -1;
    }
    bt_l2cap_reiniciar();
}

void bt_g_adaptador_pronto(void) { erro_ad[0] = 0; }

void bt_g_erro(const char *m) { bt_copia(erro_ad, m, sizeof erro_ad); }

void bt_g_info(bt_info_t *i)
{
    bt_hci_info(i);
    i->estado = !energia && est_ad == BT_PRONTO ? BT_APAGADO : (uint32_t)est_ad;
    i->energia = energia;
    i->buscando = buscando;
    i->conf_modo = conf_modo;
    i->conf_num = conf_num;
    i->conf_disp = (int8_t)(conf_modo ? conf_idx : -1);
    bt_copia(i->erro, erro_ad, sizeof i->erro);
}

/* ------------------------------------------------------------------ fallos y cierres */

static void desconectar_acl(bt_disp_t *d, uint8_t motivo)
{
    if (d->manejador == 0xFFFF) return;
    uint8_t b[3];
    bt_poe16(b, d->manejador);
    b[2] = motivo;
    bt_hci_cmd(OP_DESCONECTAR, b, 3);
}

static void falla(bt_disp_t *d, const char *msg)
{
    d->estado = BT_D_ERRO;
    bt_copia(d->erro, msg, sizeof d->erro);
    bt_log("[BT] %s: %s", d->nome[0] ? d->nome : "dispositivo", msg);
    if (conf_modo && conf_idx == (int)(d - disp)) conf_modo = BT_CONF_NADA;
    desconectar_acl(d, 0x13);
}

/* "texto (0xNN)" */
static void falla_x(bt_disp_t *d, const char *pre, int v)
{
    static const char h[] = "0123456789abcdef";
    char t[48];
    bt_copia(t, pre, 38);
    int k = (int)bt_strlen(t);
    t[k++] = ' '; t[k++] = '('; t[k++] = '0'; t[k++] = 'x';
    t[k++] = h[(v >> 4) & 15];
    t[k++] = h[v & 15];
    t[k++] = ')';
    t[k] = 0;
    falla(d, t);
}

static void libera_hid(bt_disp_t *d)
{
    const bt_plat_t *p = bt_plat();
    if (d->estado == BT_D_CONECTADO && p->soltar_entrada) p->soltar_entrada(d->tipo);
}

/* ------------------------------------------------------------------ búsqueda */

int bt_buscar(void)
{
    if (est_ad != BT_PRONTO || !energia) return -1;
    if (buscando) return 0;
    for (int i = 0; i < BT_MAX_DISP; i++) disp[i].visto = false;
    static const uint8_t p[5] = { 0x33, 0x8B, 0x9E, 6, 0 };   /* GIAC, 6 x 1,28 s */
    if (bt_hci_cmd(OP_INQUIRY, p, 5) < 0) return -1;
    buscando = true;
    busca_ini = bt_agora();
    return 0;
}

static uint8_t tipo_de_clase(uint32_t c)
{
    if (((c >> 8) & 0x1F) != 5) return BT_T_OUTRO;   /* clase principal: periférico */
    switch ((c >> 6) & 3) {
    case 1: return BT_T_TECLADO;
    case 2: return BT_T_RATO;
    case 3: return BT_T_COMBO;
    }
    return BT_T_OUTRO;
}

void bt_g_inq_resultado(const uint8_t *addr, uint32_t clase, int8_t rssi, const char *nome)
{
    bt_disp_t *d = bt_g_por_addr(addr, true);
    if (!d) return;
    d->visto = true;
    d->clase = clase;
    d->rssi = rssi;
    if (!d->tipo) d->tipo = tipo_de_clase(clase);
    if (nome && nome[0] && !d->tem_chave) bt_copia(d->nome, nome, BT_NOME_MAX);
    else if (nome && nome[0] && !d->nome[0]) bt_copia(d->nome, nome, BT_NOME_MAX);
}

static void pedir_nombre(void)
{
    if (nome_pedido || buscando) return;
    for (int i = 0; i < BT_MAX_DISP; i++) {
        if (disp[i].usado && disp[i].visto && !disp[i].nome[0] && disp[i].estado == BT_D_LIVRE) {
            uint8_t b[10];
            memcpy(b, disp[i].addr, 6);
            b[6] = 1; b[7] = 0; b[8] = 0; b[9] = 0;
            if (bt_hci_cmd(OP_NOME_REMOTO, b, 10) == 0) { nome_pedido = true; disp[i].desde = bt_agora(); }
            return;
        }
    }
}

void bt_g_inq_fim(void)
{
    buscando = false;
    pedir_nombre();
}

void bt_g_nome_remoto(const uint8_t *addr, int estado, const char *nome)
{
    nome_pedido = false;
    bt_disp_t *d = bt_g_por_addr(addr, false);
    if (d) {
        if (!estado && nome[0]) bt_copia(d->nome, nome, BT_NOME_MAX);
        else bt_copia(d->nome, "(sin nombre)", BT_NOME_MAX);   /* para no preguntar otra vez */
    }
    pedir_nombre();
}

/* ------------------------------------------------------------------ conexión */

int bt_conectar(int idx, bool so_parear)
{
    bt_disp_t *d = disp_n(idx);
    if (!d || est_ad != BT_PRONTO || !energia) return -1;
    if (d->estado != BT_D_LIVRE && d->estado != BT_D_ERRO) return -2;
    for (int i = 0; i < BT_MAX_DISP; i++)   /* un flujo a la vez: el emparejamiento tiene una sola pantalla */
        if (disp[i].usado && disp[i].estado >= BT_D_LIGANDO && disp[i].estado <= BT_D_HID && &disp[i] != d) return -3;
    if (buscando) {
        bt_hci_cmd(OP_INQUIRY_CANC, NULL, 0);
        buscando = false;
    }
    d->estado = BT_D_LIGANDO;
    d->so_parear = so_parear;
    d->iniciado_aqui = true;
    d->cerrando = false;
    d->erro[0] = 0;
    d->manejador = 0xFFFF;
    d->canal_sdp = d->canal_ctl = d->canal_int = -1;
    d->hid_ok = false;
    d->modo_boot = false;
    d->desde = bt_agora();
    uint8_t p[13];
    memcpy(p, d->addr, 6);
    bt_poe16(p + 6, 0xCC18);   /* DM1/DH1/DM3/DH3/DM5/DH5 */
    p[8] = 1;                  /* modo de repetición de exploración R1 */
    p[9] = 0;
    bt_poe16(p + 10, 0);
    p[12] = 1;                 /* se permite el cambio de papel */
    if (bt_hci_cmd(OP_CRIAR_CONEX, p, 13) < 0) { d->estado = BT_D_LIVRE; return -4; }
    return 0;
}

bool bt_g_pedido_conexao(const uint8_t *addr)
{
    bt_disp_t *d = bt_g_por_addr(addr, false);
    if (!d || !d->tem_chave || !energia) return false;
    if (d->estado != BT_D_LIVRE && d->estado != BT_D_ERRO) return false;
    d->estado = BT_D_LIGANDO;
    d->iniciado_aqui = false;
    d->so_parear = false;
    d->cerrando = false;
    d->erro[0] = 0;
    d->desde = bt_agora();
    d->canal_sdp = d->canal_ctl = d->canal_int = -1;
    return true;
}

void bt_g_conexao(int estado, uint16_t m, const uint8_t *addr, bool entrante)
{
    (void)entrante;
    bt_disp_t *d = addr ? bt_g_por_addr(addr, false) : NULL;
    if (!d && !addr)   /* la orden Create_Connection fue rechazada: el único en LIGANDO que empezamos */
        for (int i = 0; i < BT_MAX_DISP; i++)
            if (disp[i].usado && disp[i].estado == BT_D_LIGANDO && disp[i].iniciado_aqui) { d = &disp[i]; break; }
    if (!d) {   /* conexión de alguien que no conocemos: fuera */
        if (!estado && m != 0xFFFF) { uint8_t b[3]; bt_poe16(b, m); b[2] = 0x13; bt_hci_cmd(OP_DESCONECTAR, b, 3); }
        return;
    }
    if (estado) {
        d->manejador = 0xFFFF;
        if (estado == 0x04) falla(d, "no responde: esta encendido y en modo de emparejamiento?");
        else falla_x(d, "la conexion fallo", estado);
        return;
    }
    if (d->estado != BT_D_LIGANDO) {   /* cancelada por el usuario mientras tanto */
        uint8_t b[3];
        bt_poe16(b, m); b[2] = 0x13;
        bt_hci_cmd(OP_DESCONECTAR, b, 3);
        return;
    }
    d->manejador = m;
    d->desde = bt_agora();
    if (d->iniciado_aqui) {
        d->estado = BT_D_AUTENTICANDO;
        uint8_t b[2];
        bt_poe16(b, m);
        bt_hci_cmd(OP_AUTENTICAR, b, 2);
    } else {
        d->estado = BT_D_HID;   /* reconexión del dispositivo: esperamos sus dos canales */
    }
}

void bt_g_desconexao(uint16_t m, uint8_t motivo)
{
    bt_disp_t *d = bt_g_por_manejador(m);
    if (!d) return;
    int idx = (int)(d - disp);
    d->manejador = 0xFFFF;
    d->canal_sdp = d->canal_ctl = d->canal_int = -1;
    if (conf_modo && conf_idx == idx) conf_modo = BT_CONF_NADA;
    if (d->estado == BT_D_CONECTADO) {
        libera_hid(d);
        d->estado = BT_D_LIVRE;
        if (!d->cerrando) {
            bt_log("[BT] %s se desconecto (motivo %02x)", d->nome, motivo);
        }
    } else if (d->estado == BT_D_ERRO || d->estado == BT_D_LIVRE) {
        d->estado = d->estado == BT_D_ERRO ? BT_D_ERRO : BT_D_LIVRE;
    } else if (d->cerrando || (d->so_parear && d->tem_chave)) {
        d->estado = BT_D_LIVRE;
    } else {
        falla_x(d, "se perdio la conexion", motivo);
        d->manejador = 0xFFFF;
    }
    d->cerrando = false;
}

int bt_desconectar(int idx)
{
    bt_disp_t *d = disp_n(idx);
    if (!d) return -1;
    d->cerrando = true;
    if (d->manejador != 0xFFFF) {
        desconectar_acl(d, 0x13);
        return 0;
    }
    if (d->estado == BT_D_LIGANDO) {
        uint8_t b[6];
        memcpy(b, d->addr, 6);
        bt_hci_cmd(OP_CANCELAR_CONEX, b, 6);
        d->estado = BT_D_LIVRE;
        d->cerrando = false;
        return 0;
    }
    d->cerrando = false;
    if (d->estado == BT_D_ERRO) { d->estado = BT_D_LIVRE; d->erro[0] = 0; }
    return 0;
}

int bt_esquecer(int idx)
{
    bt_disp_t *d = disp_n(idx);
    if (!d) return -1;
    if (d->manejador != 0xFFFF) { d->cerrando = true; desconectar_acl(d, 0x13); }
    if (d->estado == BT_D_CONECTADO) libera_hid(d);
    if (conf_modo && conf_idx == idx) conf_modo = BT_CONF_NADA;
    bool tenia = d->tem_chave;
    uint16_t m = d->manejador;
    memset(d, 0, sizeof *d);
    d->manejador = m;   /* el evento de desconexión no encontrará nada: bien */
    d->usado = false;
    if (tenia) bt_g_mudou();
    return 0;
}

int bt_energia(bool on)
{
    if (est_ad != BT_PRONTO) return -1;
    if (on == energia) return 0;
    energia = on;
    if (!on) {
        if (buscando) { bt_hci_cmd(OP_INQUIRY_CANC, NULL, 0); buscando = false; }
        for (int i = 0; i < BT_MAX_DISP; i++) {
            if (!disp[i].usado) continue;
            if (disp[i].estado == BT_D_CONECTADO) libera_hid(&disp[i]);
            disp[i].cerrando = true;
            if (disp[i].manejador != 0xFFFF) desconectar_acl(&disp[i], 0x15);   /* apagado */
            if (disp[i].estado != BT_D_LIGANDO) continue;
            bt_desconectar(i);
        }
        conf_modo = BT_CONF_NADA;
    }
    bt_hci_energia(on);
    bt_g_mudou();
    return 0;
}

/* ------------------------------------------------------------------ autenticación y cifrado */

bool bt_g_chave_pedida(const uint8_t *addr, uint8_t *chave)
{
    bt_disp_t *d = bt_g_por_addr(addr, false);
    if (!d || !d->tem_chave) return false;
    memcpy(chave, d->chave, 16);
    return true;
}

void bt_g_chave_nova(const uint8_t *addr, const uint8_t *chave, uint8_t tipo)
{
    bt_disp_t *d = bt_g_por_addr(addr, true);
    if (!d) return;
    memcpy(d->chave, chave, 16);
    d->tipo_chave = tipo;
    d->tem_chave = true;
    bt_g_mudou();
}

bool bt_g_io_pedido(const uint8_t *addr)
{
    bt_disp_t *d = bt_g_por_addr(addr, false);
    return d && (d->estado == BT_D_AUTENTICANDO || d->estado == BT_D_LIGANDO) && d->iniciado_aqui;
}

void bt_g_io_resposta(const uint8_t *addr, uint8_t io)
{
    bt_disp_t *d = bt_g_por_addr(addr, false);
    if (d) d->io_remoto = io;
}

void bt_g_confirmar_pedido(const uint8_t *addr, uint32_t num)
{
    bt_disp_t *d = bt_g_por_addr(addr, false);
    if (!d || d->estado != BT_D_AUTENTICANDO) { bt_hci_cmd(OP_CONF_NAO, addr, 6); return; }
    if (d->io_remoto == 3) { bt_hci_cmd(OP_CONF_SIM, addr, 6); return; }   /* Just Works: sin nada que comparar */
    conf_modo = BT_CONF_COMPARAR;
    conf_num = num;
    conf_idx = (int)(d - disp);
    conf_desde = bt_agora();
}

void bt_g_passkey_mostrar(const uint8_t *addr, uint32_t num)
{
    bt_disp_t *d = bt_g_por_addr(addr, false);
    if (!d || d->estado != BT_D_AUTENTICANDO) return;
    conf_modo = BT_CONF_MOSTRAR;
    conf_num = num;
    conf_idx = (int)(d - disp);
    conf_desde = bt_agora();
}

int bt_confirmar(bool sim)
{
    if (conf_modo != BT_CONF_COMPARAR || conf_idx < 0 || !disp[conf_idx].usado) return -1;
    bt_hci_cmd(sim ? OP_CONF_SIM : OP_CONF_NAO, disp[conf_idx].addr, 6);
    conf_modo = BT_CONF_NADA;
    return 0;
}

void bt_g_pin_pedido(const uint8_t *addr)
{
    bt_disp_t *d = bt_g_por_addr(addr, false);
    if (d && d->iniciado_aqui && d->estado == BT_D_AUTENTICANDO) {   /* dispositivos anteriores a SSP: PIN de fábrica */
        uint8_t b[23];
        memset(b, 0, sizeof b);
        memcpy(b, addr, 6);
        b[6] = 4;
        memcpy(b + 7, "0000", 4);
        bt_hci_cmd(OP_PIN_SIM, b, 23);
    } else {
        bt_hci_cmd(OP_PIN_NAO, addr, 6);
    }
}

void bt_g_ssp_fim(const uint8_t *addr, int estado)
{
    (void)addr;
    conf_modo = BT_CONF_NADA;
    if (estado) {
        bt_disp_t *d = bt_g_por_addr(addr, false);
        if (d && d->estado == BT_D_AUTENTICANDO) bt_copia(d->erro, "emparejamiento rechazado", sizeof d->erro);
    }
}

void bt_g_autenticacao(uint16_t m, int estado)
{
    bt_disp_t *d = m == 0xFFFF ? NULL : bt_g_por_manejador(m);
    if (!d && m == 0xFFFF)
        for (int i = 0; i < BT_MAX_DISP; i++)
            if (disp[i].usado && (disp[i].estado == BT_D_AUTENTICANDO || disp[i].estado == BT_D_CIFRANDO)) { d = &disp[i]; break; }
    if (!d) return;
    if (d->estado != BT_D_AUTENTICANDO && d->estado != BT_D_CIFRANDO) return;
    conf_modo = BT_CONF_NADA;
    if (estado) {
        if (d->erro[0]) { d->estado = BT_D_ERRO; desconectar_acl(d, 0x13); return; }
        if (estado == 0x05 || estado == 0x06) {   /* clave rechazada o perdida en el dispositivo */
            bool tenia = d->tem_chave;
            d->tem_chave = false;
            memset(d->chave, 0, 16);
            if (tenia) bt_g_mudou();
            falla(d, "clave rechazada: pon el dispositivo en modo de emparejamiento y vuelve a emparejar");
        } else {
            falla_x(d, "autenticacion fallida", estado);
        }
        return;
    }
    if (d->estado != BT_D_AUTENTICANDO) return;
    d->estado = BT_D_CIFRANDO;
    d->desde = bt_agora();
    uint8_t b[3];
    bt_poe16(b, d->manejador);
    b[2] = 1;
    bt_hci_cmd(OP_CIFRAR, b, 3);
}

void bt_g_cifrado(uint16_t m, int estado, bool ligado)
{
    bt_disp_t *d = bt_g_por_manejador(m);
    if (!d || d->estado != BT_D_CIFRANDO) return;
    if (estado || !ligado) { falla_x(d, "no se pudo cifrar el enlace", estado); return; }
    if (d->so_parear) {
        d->cerrando = true;
        d->estado = BT_D_LIVRE;
        bt_log("[BT] %s emparejado", d->nome);
        desconectar_acl(d, 0x13);
        return;
    }
    d->estado = BT_D_SDP;
    d->desde = bt_agora();
    int c = bt_l2cap_conectar(d->manejador, PSM_SDP);
    if (c < 0) { falla(d, "sin canales L2CAP libres"); return; }
    d->canal_sdp = (int8_t)c;
}

/* ------------------------------------------------------------------ L2CAP hacia arriba */

bool bt_g_aceitar_psm(uint16_t m, uint16_t psm)
{
    bt_disp_t *d = bt_g_por_manejador(m);
    return d && d->tem_chave && !d->iniciado_aqui && (psm == PSM_HID_CTL || psm == PSM_HID_INT);
}

static void hid_pronto(bt_disp_t *d)
{
    const bt_plat_t *p = bt_plat();
    d->estado = BT_D_CONECTADO;
    d->erro[0] = 0;
    bool normal = false;
    if (p->hid_conectar) {
        bool tengo = sdp_dono == (int)(d - disp) && sdp_res.desc_tam;
        normal = p->hid_conectar(d->tipo, tengo ? sdp_res.desc : NULL, tengo ? sdp_res.desc_tam : 0) == 1;
    }
    d->modo_boot = !normal;
    if (d->modo_boot) bt_hid_protocolo_boot(d->canal_ctl);
    bt_log("[BT] %s conectado (%s)", d->nome[0] ? d->nome : "HID",
           d->tipo == BT_T_TECLADO ? "teclado" : d->tipo == BT_T_RATO ? "raton" : "HID");
    bt_g_mudou();   /* por si cambiaron el nombre o el tipo */
}

static void abrir_hid(bt_disp_t *d)
{
    d->estado = BT_D_HID;
    d->desde = bt_agora();
    int c = bt_l2cap_conectar(d->manejador, d->psm_ctl ? d->psm_ctl : PSM_HID_CTL);
    if (c < 0) { falla(d, "sin canales L2CAP libres"); return; }
}

void bt_g_canal_aberto(int canal)
{
    const bt_canal_t *c = bt_l2cap_canal(canal);
    bt_disp_t *d = bt_g_por_canal(canal);
    if (!c || !d) return;
    d->desde = bt_agora();
    if (c->psm == PSM_SDP) {
        d->canal_sdp = (int8_t)canal;
        sdp_dono = (int)(d - disp);
        memset(&sdp_res, 0, sizeof sdp_res);
        bt_sdp_pedir(canal);
    } else if (c->psm == (d->psm_ctl ? d->psm_ctl : PSM_HID_CTL) && d->canal_ctl < 0) {
        d->canal_ctl = (int8_t)canal;
        if (c->iniciado_aqui) {
            if (bt_l2cap_conectar(d->manejador, d->psm_int ? d->psm_int : PSM_HID_INT) < 0) falla(d, "sin canales L2CAP libres");
        } else if (d->canal_int >= 0) {
            hid_pronto(d);
        }
    } else if (c->psm == (d->psm_int ? d->psm_int : PSM_HID_INT) && d->canal_int < 0) {
        d->canal_int = (int8_t)canal;
        if (d->canal_ctl >= 0) hid_pronto(d);
    }
}

void bt_g_canal_dados(int canal, const uint8_t *dados, int n)
{
    const bt_canal_t *c = bt_l2cap_canal(canal);
    bt_disp_t *d = bt_g_por_canal(canal);
    if (!c || !d) return;
    if (c->psm == PSM_SDP) {
        if (d->estado != BT_D_SDP) return;
        int r = bt_sdp_resposta(canal, dados, n, &sdp_res);
        if (r == 0) return;
        d->canal_sdp = -1;
        bt_l2cap_cerrar(canal);
        if (r == 1) {
            d->hid_ok = true;
            d->psm_ctl = sdp_res.psm_ctl;
            d->psm_int = sdp_res.psm_int;
            d->subclasse_hid = sdp_res.subclasse;
            if (sdp_res.subclasse & 0xC0) d->tipo = (sdp_res.subclasse & 0xC0) == 0xC0 ? BT_T_COMBO : (sdp_res.subclasse & 0x40) ? BT_T_TECLADO : BT_T_RATO;
            if (sdp_res.nome[0] && !d->nome[0]) bt_copia(d->nome, sdp_res.nome, BT_NOME_MAX);
        } else {
            bt_log("[BT] SDP sin servicio HID utilizable (%d): se prueban los PSM de siempre", r);
            sdp_dono = -1;
            d->psm_ctl = PSM_HID_CTL;
            d->psm_int = PSM_HID_INT;
        }
        abrir_hid(d);
        return;
    }
    if (d->estado != BT_D_CONECTADO && d->estado != BT_D_HID) return;
    if (canal == d->canal_ctl) bt_hid_dados_ctl(canal, dados, n);
    else if (canal == d->canal_int) bt_hid_dados_int(canal, dados, n, d->tipo, d->modo_boot);
}

void bt_g_canal_fechado(int canal, uint16_t m, uint16_t psm, int motivo)
{
    bt_disp_t *d = bt_g_por_manejador(m);
    if (!d) return;
    if (d->canal_sdp == canal) d->canal_sdp = -1;
    bool era_ctl = d->canal_ctl == canal, era_int = d->canal_int == canal;
    if (era_ctl) d->canal_ctl = -1;
    if (era_int) d->canal_int = -1;
    if (psm == PSM_SDP) {
        if (d->estado == BT_D_SDP && motivo) {   /* ni siquiera abrió: PSM de siempre */
            d->psm_ctl = PSM_HID_CTL;
            d->psm_int = PSM_HID_INT;
            abrir_hid(d);
        }
        return;
    }
    if (d->estado == BT_D_CONECTADO && (era_ctl || era_int)) {
        libera_hid(d);
        d->estado = BT_D_LIVRE;
        d->cerrando = true;
        desconectar_acl(d, 0x13);
        return;
    }
    if (d->estado == BT_D_HID && motivo && !d->cerrando) {
        falla_x(d, "el dispositivo rechazo el canal HID", motivo & 0xFF);
    }
}

void bt_g_hid_desenchufado(int canal)
{
    bt_disp_t *d = bt_g_por_canal(canal);
    if (!d) return;
    bt_log("[BT] %s pidio desenchufar el cable virtual: se olvida", d->nome);
    int i = (int)(d - disp);
    bt_esquecer(i);
}

/* ------------------------------------------------------------------ tiempos */

void bt_g_tic(void)
{
    uint32_t ahora = bt_agora();
    if (buscando && ahora - busca_ini > 15000) buscando = false;
    if (conf_modo == BT_CONF_COMPARAR && ahora - conf_desde > T_CONFIRMAR) bt_confirmar(false);
    for (int i = 0; i < BT_MAX_DISP; i++) {
        bt_disp_t *d = &disp[i];
        if (!d->usado) continue;
        uint32_t lim = 0;
        const char *msg = NULL;
        switch (d->estado) {
        case BT_D_LIGANDO: lim = T_LIGANDO; msg = "tiempo agotado al conectar"; break;
        case BT_D_AUTENTICANDO: lim = T_AUTENTICAR; msg = "tiempo agotado al emparejar"; break;
        case BT_D_CIFRANDO: lim = T_CIFRAR; msg = "tiempo agotado al cifrar"; break;
        case BT_D_SDP: lim = T_SDP; msg = "tiempo agotado consultando el servicio HID"; break;
        case BT_D_HID: lim = T_HID; msg = "tiempo agotado abriendo los canales HID"; break;
        default: break;
        }
        if (lim && ahora - d->desde > lim) {
            if (d->estado == BT_D_LIGANDO && d->manejador == 0xFFFF && d->iniciado_aqui) {
                uint8_t b[6];
                memcpy(b, d->addr, 6);
                bt_hci_cmd(OP_CANCELAR_CONEX, b, 6);
            }
            falla(d, msg);
        }
    }
    if (nome_pedido) {   /* el nombre remoto no llegó */
        for (int i = 0; i < BT_MAX_DISP; i++)
            if (disp[i].usado && !disp[i].nome[0] && ahora - disp[i].desde > 10000 && disp[i].visto) {
                bt_copia(disp[i].nome, "(sin nombre)", BT_NOME_MAX);
                nome_pedido = false;
                pedir_nombre();
                break;
            }
    }
}
