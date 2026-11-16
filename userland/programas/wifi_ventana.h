// SPDX-License-Identifier: GPL-2.0-or-later
/* wifi_ventana.h - la ventana de redes Wi-Fi (D354): la que abre el icono de la bandeja.
 *
 * Se incluye en wifi.c y usa lo suyo: ler_config/recordar/olvidar (wifi.conf del usuario), preparar
 * (PMK, SAE o WEP calculados aquí) y la llamada SIS_WIFI. Conectar y desconectar no piden ser root:
 * vale la sesión local (D354); desde un servicio o SSH el kernel da ERRO_ACESSO y se dice.
 *
 *   wifi --ventana [--simulado]
 *
 * --simulado: cinco redes de mentira y un «punto de acceso» que acepta la contraseña «sagui1234»,
 * para probar la ventana (y que se guardan las redes) en QEMU, que no tiene el pincho. */

#define VL 560
#define VA 440
#define COR_AVISO 0xB02020
#define MAX_ENT 40

/* ------------------------------------------------------------ simulación */

static info_wifi_t sim_inf;
static unsigned sim_t0, sim_busca_t0;
static bool sim_conectando, sim_pmk_ok;

static const struct { const char *ssid; unsigned char seg, qual; signed char sinal; } sim_redes[] = {
    { "SaguiNet", WIFI_WPA2, 82, -48 }, { "CafeLibre", WIFI_ABERTA, 55, -66 }, { "Fibra-5G", WIFI_WPA3, 40, -74 },
    { "Vieja", WIFI_WEP, 30, -80 }, { "Casa", WIFI_WPA2, 20, -88 },
};

static int wifi_sim(int op, void *p, unsigned n)
{
    (void)n;
    if (sim_inf.estado == 0) { sim_inf.estado = WIFI_PRONTO; copia_texto(sim_inf.chip, "Simulado", sizeof sim_inf.chip); }
    if (op == WIFI_ESTADO) {
        if (sim_busca_t0 && tempo_ms() - sim_busca_t0 > 900) sim_busca_t0 = 0;
        sim_inf.buscando = sim_busca_t0 != 0;
        if (sim_conectando && tempo_ms() - sim_t0 > 1200) {
            sim_conectando = false;
            if (sim_pmk_ok) { sim_inf.estado = WIFI_CONECTADO; sim_inf.erro[0] = 0; }
            else { sim_inf.estado = WIFI_PRONTO; copia_texto(sim_inf.erro, "contraseña incorrecta", sizeof sim_inf.erro); }
        }
        memcpy(p, &sim_inf, sizeof sim_inf);
        return 0;
    }
    if (op == WIFI_BUSCAR) { sim_busca_t0 = tempo_ms() | 1; return 0; }
    if (op == WIFI_REDES) {
        rede_wifi_t *r = p;
        int k = (int)(sizeof sim_redes / sizeof sim_redes[0]);
        for (int i = 0; i < k && i < (int)n; i++) {
            memset(&r[i], 0, sizeof r[i]);
            copia_texto(r[i].ssid, sim_redes[i].ssid, sizeof r[i].ssid);
            r[i].bssid[5] = (unsigned char)(i + 1);
            r[i].canal = (unsigned char)(1 + 5 * i);
            r[i].sinal = sim_redes[i].sinal;
            r[i].seguranca = sim_redes[i].seg;
            r[i].suportada = 1;
            r[i].qualidade = sim_redes[i].qual;
        }
        return k;
    }
    if (op == WIFI_DESCONECTAR) { sim_inf.estado = WIFI_PRONTO; sim_inf.ssid[0] = 0; return 0; }
    if (op == WIFI_CONECTAR) {
        const pedido_wifi_t *q = p;
        uint8_t esp[32];
        sim_pmk_ok = (!q->tem_pmk && !q->nwep && !q->tem_pt) || q->nwep || q->tem_pt ||
                     (wpa_pmk("sagui1234", (const uint8_t *)q->ssid, (uint32_t)tam_texto(q->ssid), esp) && !memcmp(esp, q->pmk, 32));
        copia_texto(sim_inf.ssid, q->ssid, sizeof sim_inf.ssid);
        sim_inf.estado = WIFI_CONECTANDO;
        sim_inf.erro[0] = 0;
        sim_inf.seguranca = WIFI_WPA2;
        sim_inf.qualidade = 80;
        sim_inf.canal = 6;
        sim_conectando = true;
        sim_t0 = tempo_ms();
        return 0;
    }
    return ERRO_INVALIDO;
}

/* ------------------------------------------------------------ la lista */

typedef struct { char ssid[33]; int k; bool rec, con; unsigned seg; int qual; } ent_t;

enum { F_NADA, F_BUSCANDO, F_CONECTANDO };

static jan_t *jw;
static info_wifi_t inf;
static bool tem;
static int nrede, fase, nent;
static unsigned t_fase;
static ent_t ent[MAX_ENT];
static char rec_ssid[MAX_LINHAS][33];
static int nrec;
static char itens[MAX_ENT][64];
static botao_t b_buscar = { 0, 8, 150, 26, N_("Buscar de nuevo"), false },
               b_conectar = { 12, 0, 110, 26, N_("Conectar"), false },
               b_desconectar = { 130, 0, 110, 26, N_("Desconectar"), false },
               b_olvidar = { 248, 0, 90, 26, N_("Olvidar"), false },
               b_ok = { 0, 0, 100, 26, N_("Conectar"), false }, b_cancel = { 0, 0, 100, 26, N_("Cancelar"), false };
static lista_t lista = { 12, 46, VL - 24, 200, itens, 0, -1, 0, 0, 0 };
static caixa_t c_clave;
static casilla_t c_mostrar;
static bool pidiendo;
static bool msg_info;   /* el mensaje es un aviso de progreso, no un error */
static char objetivo[33];
static pedido_wifi_t ped;

static void barras(jan_t *j, int x, int y, int i)
{
    ent_t *e = &ent[i];
    int n = e->k < 0 ? 0 : e->qual >= 75 ? 4 : e->qual >= 50 ? 3 : e->qual >= 25 ? 2 : 1;
    for (int b = 0; b < 4; b++) {
        int h = 3 + 3 * b;
        g_retangulo(j, x + 3 * b, y + 12 - h, 2, h, b < n ? (e->con ? COR_DESTAQUE : 0x3A4652) : 0xC4CBD2);
    }
    if (e->k >= 0 && e->seg != WIFI_ABERTA) {   /* el candado */
        g_retangulo(j, x + 12, y + 7, 6, 5, 0x6A5A1E);
        g_moldura(j, x + 13, y + 3, 4, 5, 0x6A5A1E);
    }
}

static void cargar_recordadas(void)
{
    ler_config();
    nrec = 0;
    for (int i = 0; i < nlinhas && nrec < MAX_LINHAS; i++) {
        int k = 0;
        for (const char *p = linhas[i] + 5; p[0] != ' ' && p[0] && p[1] && k < 32; p += 2) {
            int a = hexval(p[0]), b = hexval(p[1]);
            if (a < 0 || b < 0) break;
            rec_ssid[nrec][k++] = (char)(a << 4 | b);
        }
        rec_ssid[nrec][k] = 0;
        if (k) nrec++;
    }
    wpa_apagar(linhas, sizeof linhas);
}

static bool es_recordada(const char *s)
{
    for (int i = 0; i < nrec; i++) if (!compara_texto(rec_ssid[i], s)) return true;
    return false;
}

/* Reconstruye la lista: la conectada primero, luego de más a menos señal, y al final las recordadas fuera de alcance. */
static void cargar_redes(void)
{
    static rede_wifi_t buf[32];
    nrede = tem ? wifi(WIFI_REDES, buf, 32) : 0;
    if (nrede < 0) nrede = 0;
    memcpy(redes, buf, sizeof buf);
    cargar_recordadas();
    nent = 0;
    for (int i = 0; i < nrede && nent < MAX_ENT; i++) {
        if (!redes[i].ssid[0]) continue;
        int d = -1;
        for (int k = 0; k < nent; k++) if (!compara_texto(ent[k].ssid, redes[i].ssid)) d = k;
        if (d >= 0) {
            if (redes[i].sinal > redes[ent[d].k].sinal) { ent[d].k = i; ent[d].qual = redes[i].qualidade; ent[d].seg = redes[i].seguranca; }
            continue;
        }
        ent_t *e = &ent[nent++];
        copia_texto(e->ssid, redes[i].ssid, sizeof e->ssid);
        e->k = i; e->qual = redes[i].qualidade; e->seg = redes[i].seguranca;
        e->rec = es_recordada(e->ssid);
        e->con = tem && inf.estado == WIFI_CONECTADO && !compara_texto(inf.ssid, e->ssid);
    }
    for (int i = 0; i < nrec && nent < MAX_ENT; i++) {
        bool ja = false;
        for (int k = 0; k < nent; k++) if (!compara_texto(ent[k].ssid, rec_ssid[i])) ja = true;
        if (ja) continue;
        ent_t *e = &ent[nent++];
        memset(e, 0, sizeof *e);
        copia_texto(e->ssid, rec_ssid[i], sizeof e->ssid);
        e->k = -1; e->rec = true;
        e->con = tem && inf.estado == WIFI_CONECTADO && !compara_texto(inf.ssid, e->ssid);
    }
    for (int i = 1; i < nent; i++)   /* ordenar: conectada, a la vista por señal, fuera de alcance */
        for (int k = i; k > 0; k--) {
            ent_t *a = &ent[k - 1], *b = &ent[k];
            int pa = a->con ? 1000 : a->k < 0 ? -1 : a->qual, pb = b->con ? 1000 : b->k < 0 ? -1 : b->qual;
            if (pb <= pa) break;
            ent_t x = *a; *a = *b; *b = x;
        }
    for (int i = 0; i < nent; i++) {
        char nome[26];
        copia_texto(nome, ent[i].ssid, sizeof nome);
        formatar_texto(itens[i], sizeof itens[i], "%-25s %s", nome,
                       ent[i].con ? _("conectada") : ent[i].rec ? (ent[i].k < 0 ? _("recordada, fuera de alcance") : _("recordada")) : "");
    }
    lista.qtd = nent;
    registrar("wifi: lista %d redes (%d recordadas)", nent, nrec);
    if (lista.sel >= nent) lista.sel = nent - 1;
}

/* ------------------------------------------------------------ dibujo */

static const char *nome_est(void)
{
    if (!tem) return _("sin adaptador");
    switch (inf.estado) {
    case WIFI_INICIANDO: return _("iniciando el adaptador...");
    case WIFI_PRONTO: return _("sin conectar");
    case WIFI_CONECTANDO: case WIFI_SAUDACAO: return _("conectando...");
    case WIFI_CONECTADO: return _("conectado");
    case WIFI_SEM_FIRMWARE: return _("falta el firmware");
    }
    return _("error");
}

static void linha_msg(int y, const char *t, uint32_t cor)
{
    int cols = (jw->larg - 24) / LETRA_L;
    g_texto_n(jw, 12, y, t, cols, cor);
    if ((int)tam_texto(t) > cols) g_texto_n(jw, 12, y + 16, t + cols, cols, cor);
}

static void desenhar_tudo(void)
{
    int L = jw->larg, A = jw->alt;
    g_retangulo(jw, 0, 0, L, A, COR_FUNDO);
    char t[160];
    if (!tem) {
        rotulo(jw, 12, 14, _("Wi-Fi: no hay ningún adaptador (el pincho Realtek RTL8188EU)."), COR_AVISO);
    } else {
        if (inf.estado == WIFI_CONECTADO)
            formatar_texto(t, sizeof t, _("Wi-Fi: conectado a %s (%u %%)"), inf.ssid, inf.qualidade);
        else formatar_texto(t, sizeof t, _("Wi-Fi: %s"), nome_est());
        rotulo(jw, 12, 14, t, inf.estado == WIFI_FALHOU ? COR_AVISO : COR_TEXTO);
        b_buscar.x = L - 12 - b_buscar.l;
        b_buscar.texto = fase == F_BUSCANDO ? _("Buscando...") : _("Buscar de nuevo");
        botao_desenhar(jw, &b_buscar);
    }
    int y0 = A - 124;
    lista.l = L - 24;
    lista.a = y0 - 8 - lista.y;
    lista_desenhar(jw, &lista);
    if (!nent && tem) rotulo(jw, 24, lista.y + 12, fase == F_BUSCANDO ? _("Buscando redes...") : _("No se ve ninguna red. Pulsa «Buscar de nuevo»."), COR_TEXTO_FRACO);
    if (pidiendo) {
        formatar_texto(t, sizeof t, _("Contraseña de «%s»:"), objetivo);
        rotulo(jw, 12, y0, t, COR_TEXTO);
        c_clave.x = 12; c_clave.y = y0 + 20; c_clave.l = L - 24; c_clave.foco = true;
        caixa_desenhar(jw, &c_clave);
        c_mostrar.x = 12; c_mostrar.y = y0 + 58;
        casilla_desenhar(jw, &c_mostrar);
        rotulo(jw, 32, y0 + 57, _("Mostrar la contraseña"), COR_TEXTO);
        b_cancel.x = L - 12 - b_cancel.l; b_cancel.y = y0 + 52;
        b_ok.x = b_cancel.x - 8 - b_ok.l; b_ok.y = y0 + 52;
        botao_desenhar(jw, &b_ok);
        botao_desenhar(jw, &b_cancel);
        if (msg_gui[0]) linha_msg(y0 + 84, msg_gui, COR_AVISO);
    } else {
        b_conectar.y = b_desconectar.y = b_olvidar.y = y0;
        botao_desenhar(jw, &b_conectar);
        botao_desenhar(jw, &b_desconectar);
        botao_desenhar(jw, &b_olvidar);
        if (msg_gui[0]) linha_msg(y0 + 38, msg_gui, msg_info ? COR_TEXTO : COR_AVISO);
        else rotulo(jw, 12, y0 + 38, _("Elige una red y pulsa «Conectar» (o doble clic)."), COR_TEXTO_FRACO);
        rotulo(jw, 12, y0 + 84, _("Conectar y desconectar no necesita ser administrador."), COR_TEXTO_FRACO);
    }
    jan_mostrar(jw);
}

/* ------------------------------------------------------------ acciones */

static void mensaje(const char *m) { msg_info = false; copia_texto(msg_gui, m, sizeof msg_gui); }

static void fallo_llamada(int r)
{
    if (r == ERRO_ACESSO) mensaje(_("Solo la sesión local o root pueden conectar o desconectar el Wi-Fi."));
    else if (r == ERRO_NAO_EXISTE) mensaje(_("No hay adaptador Wi-Fi."));
    else formatar_texto(msg_gui, sizeof msg_gui, _("No se pudo: %s"), nome_erro(r));
}

/* senha NULL: la red recordada o abierta; si no, la contraseña recién escrita. */
static void conectar_ent(const ent_t *e, const char *senha)
{
    msg_gui[0] = 0;
    memset(&ped, 0, sizeof ped);
    copia_texto(ped.ssid, e->ssid, sizeof ped.ssid);
    if (senha) {
        if (e->k < 0) {
            if (!wpa_pmk(senha, (const uint8_t *)e->ssid, (uint32_t)tam_texto(e->ssid), ped.pmk)) {
                mensaje(_("La clave de WPA/WPA2 tiene de 8 a 63 caracteres."));
                wpa_apagar(&ped, sizeof ped);
                return;
            }
            ped.tem_pmk = 1;
        } else if (preparar(&ped, e->ssid, senha, e->k, nrede)) { wpa_apagar(&ped, sizeof ped); return; }
    } else if (e->rec) {
        ler_config();
        bool aberta;
        ler_recordada(e->ssid, &ped, &aberta);
        wpa_apagar(linhas, sizeof linhas);
    }
    int r = wifi(WIFI_CONECTAR, &ped, sizeof ped);
    if (r < 0) { wpa_apagar(&ped, sizeof ped); fallo_llamada(r); return; }
    copia_texto(objetivo, e->ssid, sizeof objetivo);
    fase = F_CONECTANDO;
    t_fase = tempo_ms();
    registrar("wifi: conectando a %s%s", e->ssid, senha ? " (con contrasena)" : "");
    msg_info = true;
    formatar_texto(msg_gui, sizeof msg_gui, _("Conectando a %s..."), e->ssid);
}

static void pulsar_conectar(void)
{
    msg_gui[0] = 0;
    msg_info = false;
    if (lista.sel < 0 || lista.sel >= nent) { mensaje(_("Elige primero una red de la lista.")); return; }
    ent_t *e = &ent[lista.sel];
    if (e->con) { mensaje(_("Ya estás conectado a esa red.")); return; }
    if (e->k >= 0 && !redes[e->k].suportada) { mensaje(_("Esa red no se puede usar (Enterprise u otra no admitida).")); return; }
    if (e->rec || (e->k >= 0 && e->seg == WIFI_ABERTA)) conectar_ent(e, NULL);
    else {   /* pide la contraseña */
        copia_texto(objetivo, e->ssid, sizeof objetivo);
        memset(&c_clave, 0, sizeof c_clave);
        c_clave.oculta = true; c_clave.foco = true;
        c_mostrar.marcada = false;
        pidiendo = true;
        registrar("wifi: pide la contrasena de %s", objetivo);
    }
}

static void enviar_clave(void)
{
    int i;
    for (i = 0; i < nent; i++) if (!compara_texto(ent[i].ssid, objetivo)) break;
    if (i == nent) { pidiendo = false; return; }
    if (!c_clave.texto[0]) { mensaje(_("Escribe la contraseña.")); return; }
    conectar_ent(&ent[i], c_clave.texto);
    if (fase == F_CONECTANDO) pidiendo = false;
    memset(&c_clave, 0, sizeof c_clave);
}

static void iniciar_busca(void)
{
    if (!tem || fase != F_NADA) return;
    if (wifi(WIFI_BUSCAR, 0, 0) < 0) return;
    fase = F_BUSCANDO;
    t_fase = tempo_ms();
}

/* Cada vuelta: estado del adaptador y fin de búsqueda o de conexión. */
static void refrescar(void)
{
    tem = wifi(WIFI_ESTADO, &inf, sizeof inf) >= 0;
    unsigned dt = tempo_ms() - t_fase;
    if (fase == F_BUSCANDO && tem && !inf.buscando && dt > 600) { fase = F_NADA; cargar_redes(); }
    if (fase == F_CONECTANDO) {
        if (tem && inf.estado == WIFI_CONECTADO) {
            recordar(objetivo, &ped);
            wpa_apagar(&ped, sizeof ped);
            fase = F_NADA;
            registrar("wifi: conectado a %s", objetivo);
            msg_info = true;
            formatar_texto(msg_gui, sizeof msg_gui, _("Conectado a %s."), objetivo);
            cargar_redes();
        } else if (tem && inf.estado == WIFI_PRONTO && inf.erro[0] && dt > 1500) {
            wpa_apagar(&ped, sizeof ped);
            fase = F_NADA;
            registrar("wifi: error al conectar: %s", inf.erro);
            msg_info = false;
            formatar_texto(msg_gui, sizeof msg_gui, _("No se pudo conectar: %s"), inf.erro);
        } else if (dt > 45000) {
            wpa_apagar(&ped, sizeof ped);
            fase = F_NADA;
            mensaje(_("No se pudo conectar: tiempo agotado."));
        }
    }
    if (fase == F_NADA) {   /* la conexión puede cambiar sola (se pierde la señal, reconecta) */
        bool con = tem && inf.estado == WIFI_CONECTADO;
        bool vista = false;
        for (int i = 0; i < nent; i++) if (ent[i].con) vista = true;
        if (con != vista) cargar_redes();
    }
}

static int ventana(void)
{
    modo_gui = true;
    jw = jan_abrir(_("Redes Wi-Fi"), VL, VA);
    if (!jw) { imprimir("wifi: no se pudo abrir la ventana (¿hay escritorio?)\n"); return 1; }
    jan_estilo(jw, JAN_REDIMENSIONAVEL, 480, 360);
    lista.icone = barras;
    refrescar();
    cargar_redes();
    if (tem && inf.estado == WIFI_PRONTO) iniciar_busca();
    desenhar_tudo();
    evento_t e;
    for (;;) {
        if (esperar_evento(jw->id, &e, 400) != 1) { refrescar(); desenhar_tudo(); continue; }
        if (e.tipo == EV_FECHAR) break;
        if (jan_redimensionar(jw, &e)) { desenhar_tudo(); continue; }
        if (e.tipo == EV_CLIQUE || e.tipo == EV_SOLTOU || e.tipo == EV_TECLA || e.tipo == EV_RODA) {
            if (pidiendo) {
                if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) { pidiendo = false; memset(&c_clave, 0, sizeof c_clave); }
                else if (casilla_tratar(&c_mostrar, &e)) c_clave.oculta = !c_mostrar.marcada;
                else if (botao_tratar(jw, &b_ok, &e)) enviar_clave();
                else if (botao_tratar(jw, &b_cancel, &e)) { pidiendo = false; memset(&c_clave, 0, sizeof c_clave); msg_gui[0] = 0; }
                else if (caixa_tratar(jw, &c_clave, &e)) enviar_clave();
            } else {
                int q = lista_tratar(jw, &lista, &e);
                if (q == LISTA_ATIVOU) pulsar_conectar();
                else if (q != LISTA_NADA) msg_gui[0] = 0;
                else if (botao_tratar(jw, &b_buscar, &e)) iniciar_busca();
                else if (botao_tratar(jw, &b_conectar, &e)) pulsar_conectar();
                else if (botao_tratar(jw, &b_desconectar, &e)) {
                    int r = wifi(WIFI_DESCONECTAR, 0, 0);
                    registrar("wifi: desconectar -> %d", r);
                    if (r < 0) fallo_llamada(r); else { mensaje(_("Desconectado.")); msg_info = true; }
                    refrescar();
                    cargar_redes();
                } else if (botao_tratar(jw, &b_olvidar, &e)) {
                    if (lista.sel >= 0 && lista.sel < nent && ent[lista.sel].rec) {
                        char s[33];
                        copia_texto(s, ent[lista.sel].ssid, sizeof s);
                        if (confirmar(_("Redes Wi-Fi"), _("¿Olvidar esta red? Habrá que escribir la contraseña otra vez."))) {
                            msg_gui[0] = 0;
                            olvidar(s);
                            registrar("wifi: olvidada %s", s);
                            cargar_redes();
                        }
                    } else mensaje(_("Esa red no está entre las recordadas."));
                }
            }
            refrescar();
            desenhar_tudo();
        }
    }
    wpa_apagar(&ped, sizeof ped);
    wpa_apagar(&c_clave, sizeof c_clave);
    jan_fechar(jw);
    return 0;
}
