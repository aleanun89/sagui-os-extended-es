// SPDX-License-Identifier: GPL-3.0-only
/* wg_ventana.h - la ventana «VPN (WireGuard)» (D363). Incluida por wg.c; la abre
 * Configuración > Red («VPN (WireGuard)...») y el icono de la VPN en la bandeja.
 *
 *   wg --ventana
 *
 * Lista las configuraciones de /home/.config/wireguard (los .conf) y deja:
 *   - Conectar / Desconectar (lo mismo que wg-quick up/down, con quick_up y quick_down);
 *   - Importar .conf...: copia el archivo elegido a la carpeta con permisos 600 (solo el
 *     dueño); si el archivo no trae PrivateKey, se le pone la generada aquí;
 *   - Borrar (pregunta);
 *   - Generar claves: una privada nueva, guardada en /home/.config/wireguard/.clave (600),
 *     y su pública a la vista con «Copiar» para dársela al servidor.
 * Arriba, el estado: encendida o no, el último saludo y los bytes (INFO_WG, cada segundo).
 * Conectar pide permiso de administrador cada vez (el diálogo del sistema, D372); Desconectar,
 * no. */

#define WL 600
#define WA 460
#define MAX_CONF 24
#define ARQ_CLAVE DIR_WG "/.clave"
#define COR_AVISO_WG 0xB02020

static jan_t *jg;
static char conf_itens[MAX_CONF][64];
static int nconf;
static lista_t lista_conf = { 12, 86, WL - 24, 160, conf_itens, 0, -1, 0, 0, 0 };
static botao_t b_wg_con = { 12, 0, 110, 26, N_("Conectar"), false },
               b_wg_des = { 128, 0, 110, 26, N_("Desconectar"), false },
               b_wg_imp = { 244, 0, 150, 26, N_("Importar .conf..."), false },
               b_wg_bor = { 400, 0, 90, 26, N_("Borrar"), false },
               b_wg_gen = { 12, 0, 150, 26, N_("Generar claves"), false },
               b_wg_cop = { 168, 0, 90, 26, N_("Copiar"), false };
static info_wg_t wg_inf;
static bool wg_ativa;
static char wg_publica[48];
static bool msg_ok;

static void wg_msg(bool ok, const char *t) { msg_ok = ok; copia_texto(msg_gui, t, sizeof msg_gui); }

/* La carpeta, solo del dueño (0700) aunque ya existiera con otros permisos. */
static void criar_dir_wg(void)
{
    criar_pasta("/home/.config");
    criar_pasta(DIR_WG);
    mudar_permisos(DIR_WG, 0700);
}

static void carregar_confs(void)
{
    nconf = 0;
    entrada_pasta_t e;
    for (int i = 0; nconf < MAX_CONF && listar(DIR_WG, i, &e) > 0; i++) {
        size_t n = tam_texto(e.nome);
        if (e.tipo == TIPO_PASTA || n <= 5 || compara_texto(e.nome + n - 5, ".conf") != 0) continue;
        copia_texto(conf_itens[nconf], e.nome, 64);
        conf_itens[nconf][n - 5] = 0;   /* sin «.conf» */
        nconf++;
    }
    lista_conf.qtd = nconf;
    if (lista_conf.sel >= nconf) lista_conf.sel = nconf - 1;
    if (lista_conf.sel < 0 && nconf) lista_conf.sel = 0;
}

/* La clave generada (si hay): se lee de .clave y se enseña su pública. */
static bool ler_clave(uint8_t k[32])
{
    char t[64];
    int fd = abrir(ARQ_CLAVE, ABRIR_LER);
    if (fd < 0) return false;
    int n = ler(fd, t, sizeof t - 1);
    fechar(fd);
    if (n <= 0) return false;
    t[n] = 0;
    bool ok = wg_base64_ler(recortar(t), k);
    cripto_apagar(t, sizeof t);
    return ok;
}

static void mostrar_publica(void)
{
    uint8_t k[32], p[32];
    wg_publica[0] = 0;
    if (!ler_clave(k)) return;
    wg_publica_de(k, p);
    wg_base64_escrever(p, wg_publica);
    cripto_apagar(k, sizeof k);
}

static bool gravar_privado(const char *caminho, const char *dados, unsigned n)
{
    criar_dir_wg();
    /* Se crea ya con 0600 (máscara 077): si se crease con 0644 y luego se cambiase, otro
     * usuario podría abrirlo en medio y leer la clave después con ese descriptor. */
    int mascara = mascara_criacao(077);
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (mascara >= 0) mascara_criacao(mascara);
    if (fd < 0) return false;
    mudar_permisos(caminho, 0600);   /* también si ya existía con otros permisos, antes de escribir */
    bool ok = escrever_tudo(fd, dados, n) == (int)n;
    fechar(fd);
    return ok;
}

static void gerar_claves(void)
{
    uint8_t k[32];
    if (ler_clave(k) && !confirmar(_("VPN (WireGuard)"), _("Ya hay un par de claves generado. ¿Cambiarlo por otro? El servidor tendrá que conocer la clave pública nueva."))) {
        cripto_apagar(k, sizeof k);
        return;
    }
    char t[48];
    wg_gerar_privada(k);
    wg_base64_escrever(k, t);
    junta_texto(t, "\n", sizeof t);
    bool ok = gravar_privado(ARQ_CLAVE, t, (unsigned)tam_texto(t));
    cripto_apagar(k, sizeof k);
    cripto_apagar(t, sizeof t);
    if (!ok) { wg_msg(false, _("No se pudo guardar la clave en /home/.config/wireguard.")); return; }
    mostrar_publica();
    registrar("wg: claves generadas");
    copiar_texto(wg_publica, (unsigned)tam_texto(wg_publica));
    wg_msg(true, _("Claves generadas. La pública está copiada: dásela al servidor (su [Peer] PublicKey)."));
}

/* Copia el .conf elegido a la carpeta (600). Sin PrivateKey, se le añade la generada. */
static void importar_de(const char *origem)
{
    char nome[16];
    if (!nome_de_caminho(origem, nome)) { wg_msg(false, _("El nombre del archivo no vale (letras, cifras, «_-.+=», hasta 15).")); return; }
    static char texto[CONF_MAX + 128];
    int fd = abrir(origem, ABRIR_LER);
    if (fd < 0) { wg_msg(false, _("No se pudo leer el archivo.")); return; }
    int n = 0, r;
    while (n < CONF_MAX && (r = ler(fd, texto + n, (unsigned)(CONF_MAX - n))) > 0) n += r;
    fechar(fd);
    texto[n] = 0;
    /* ¿Trae PrivateKey? Si no, va la generada justo después de [Interface]. */
    bool tem_priv = false;
    for (const char *p = texto; *p; p++)
        if ((p == texto || p[-1] == '\n') && compara_texto_n(p, "PrivateKey", 10) == 0) tem_priv = true;
    static char saida[CONF_MAX + 128];
    if (!tem_priv) {
        uint8_t k[32];
        if (!ler_clave(k)) { wg_msg(false, _("El archivo no trae PrivateKey: genera antes las claves aquí.")); return; }
        char b[48];
        wg_base64_escrever(k, b);
        cripto_apagar(k, sizeof k);
        char *i = NULL;
        for (char *p = texto; *p; p++)
            if ((p == texto || p[-1] == '\n') && compara_texto_n(p, "[Interface]", 11) == 0) { i = p + 11; break; }
        if (!i) { cripto_apagar(b, sizeof b); wg_msg(false, _("El archivo no tiene [Interface].")); return; }
        int k0 = (int)(i - texto);
        memcpy(saida, texto, (size_t)k0);
        saida[k0] = 0;
        formatar_texto(saida + k0, sizeof saida - (size_t)k0, "\nPrivateKey = %s%s", b, i);
        cripto_apagar(b, sizeof b);
    } else copia_texto(saida, texto, sizeof saida);
    cripto_apagar(texto, sizeof texto);
    char destino[160];
    formatar_texto(destino, sizeof destino, "%s/%s.conf", DIR_WG, nome);
    bool ok = gravar_privado(destino, saida, (unsigned)tam_texto(saida));
    cripto_apagar(saida, sizeof saida);
    if (!ok) { wg_msg(false, _("No se pudo guardar en /home/.config/wireguard.")); return; }
    static conf_wg_t c;
    char erro[200];
    if (!conf_ler(destino, &c, erro, sizeof erro)) {
        char m[240];
        formatar_texto(m, sizeof m, _("Importada, pero tiene un error: %s"), erro);
        wg_msg(false, m);
        registrar("wg: importada %s con un error", nome);
    } else {
        char m[160];
        formatar_texto(m, sizeof m, _("Importada como «%s»."), nome);
        wg_msg(true, m);
        registrar("wg: importada %s", nome);
    }
    conf_wg_apagar(&c);
    carregar_confs();
    for (int k = 0; k < nconf; k++) if (compara_texto(conf_itens[k], nome) == 0) lista_conf.sel = k;
}

static void importar(void)
{
    char origem[160];
    origem[0] = 0;
    if (!dialogo_abrir(_("Importar una configuración WireGuard"), _("WireGuard (*.conf)|*.conf|Todos los archivos (*.*)|*"),
                       "/home", origem, sizeof origem) || !origem[0])
        return;
    importar_de(origem);
}

static void refrescar_wg(void) { wg_ativa = info_wg(&wg_inf) > 0; }

static unsigned bits_masc(unsigned m) { unsigned b = 0; while (b < 32 && (m & (0x80000000u >> b))) b++; return b; }

static void desenhar_wg(void)
{
    int L = jg->larg, A = jg->alt;
    g_retangulo(jg, 0, 0, L, A, COR_FUNDO);
    char t[200], a[16], b[24], c[24];
    if (wg_ativa) {
        info_wg_par_t *p = &wg_inf.wg.pares[0];
        texto_desde(b, sizeof b, wg_inf.wg.npares ? p->ultimo_hs : 0);
        formatar_texto(t, sizeof t, _("VPN encendida: %s (%s/%u). Último saludo: %s."), wg_inf.config.nome,
                       ip_para_texto(wg_inf.config.ip, a), bits_masc(wg_inf.config.mascara), b);
        rotulo(jg, 12, 12, t, wg_inf.wg.npares && p->ultimo_hs ? COR_TEXTO : COR_AVISO_WG);
        texto_bytes(b, sizeof b, wg_inf.wg.npares ? p->rx_bytes : 0);
        texto_bytes(c, sizeof c, wg_inf.wg.npares ? p->tx_bytes : 0);
        if (wg_inf.wg.npares && p->endpoint_ip)
            formatar_texto(t, sizeof t, _("Recibido %s, enviado %s. Servidor %s:%u."), b, c, ip_para_texto(p->endpoint_ip, a), p->endpoint_porta);
        else formatar_texto(t, sizeof t, _("Recibido %s, enviado %s."), b, c);
        rotulo(jg, 12, 32, t, COR_TEXTO_FRACO);
    } else {
        rotulo(jg, 12, 12, _("VPN apagada."), COR_TEXTO);
        rotulo(jg, 12, 32, _("Elige una configuración y pulsa «Conectar»."), COR_TEXTO_FRACO);
    }
    rotulo(jg, 12, 64, _("Configuraciones (/home/.config/wireguard):"), COR_TEXTO);
    lista_conf.l = L - 24;
    int y = A - 150;
    lista_conf.a = y - 8 - lista_conf.y;
    lista_desenhar(jg, &lista_conf);
    if (!nconf) rotulo(jg, 24, lista_conf.y + 12, _("No hay ninguna. Pulsa «Importar .conf...»."), COR_TEXTO_FRACO);
    b_wg_con.y = b_wg_des.y = b_wg_imp.y = b_wg_bor.y = y;
    botao_desenhar(jg, &b_wg_con);
    botao_desenhar(jg, &b_wg_des);
    botao_desenhar(jg, &b_wg_imp);
    botao_desenhar(jg, &b_wg_bor);
    rotulo(jg, 12, y + 40, _("Tu clave pública (para el servidor):"), COR_TEXTO);
    rotulo(jg, 12, y + 60, wg_publica[0] ? wg_publica : _("(ninguna: pulsa «Generar claves»)"), wg_publica[0] ? COR_TEXTO : COR_TEXTO_FRACO);
    b_wg_gen.y = b_wg_cop.y = y + 82;
    botao_desenhar(jg, &b_wg_gen);
    botao_desenhar(jg, &b_wg_cop);
    if (msg_gui[0]) {
        int cols = (L - 24) / LETRA_L;
        uint32_t cor = msg_ok ? COR_TEXTO : COR_AVISO_WG;
        g_texto_n(jg, 12, y + 116, msg_gui, cols, cor);
        if ((int)tam_texto(msg_gui) > cols) g_texto_n(jg, 12, y + 132, msg_gui + cols, cols, cor);
    } else rotulo(jg, 12, y + 116, _("Conectar pide permiso de administrador; desconectar, no."), COR_TEXTO_FRACO);
    jan_mostrar(jg);
}

static void conectar_sel(void)
{
    if (lista_conf.sel < 0 || lista_conf.sel >= nconf) { wg_msg(false, _("Elige una configuración.")); return; }
    if (wg_ativa) { wg_msg(false, _("Ya hay una VPN encendida: desconéctala antes.")); return; }
    wg_msg(true, _("Conectando..."));
    desenhar_wg();
    msg_gui[0] = 0;
    registrar("wg: conectar %s", conf_itens[lista_conf.sel]);
    int r = quick_up(conf_itens[lista_conf.sel], false, true);
    registrar("wg: conectar -> %d", r);
    if (r == 0) wg_msg(true, _("VPN encendida."));
    else if (!msg_gui[0]) wg_msg(false, _("No se pudo encender la VPN."));
    else msg_ok = false;
}

static int ventana_wg(int argc, char **argv)
{
    modo_gui = true;
    jg = jan_abrir(_("VPN (WireGuard)"), WL, WA);
    if (!jg) { imprimir(_("wg: no se pudo abrir la ventana (¿hay escritorio?)\n")); return 1; }
    jan_estilo(jg, JAN_REDIMENSIONAVEL, 520, 400);
    carregar_confs();
    /* Para las pruebas (h365): --importar RUTA hace lo del botón sin el diálogo de archivos. */
    if (argc >= 4 && compara_texto(argv[2], "--importar") == 0) importar_de(argv[3]);
    mostrar_publica();
    refrescar_wg();
    desenhar_wg();
    registrar("wg: ventana, %d configuraciones%s", nconf, wg_publica[0] ? ", con claves" : "");
    evento_t e;
    for (;;) {
        if (esperar_evento(jg->id, &e, 1000) != 1) { refrescar_wg(); desenhar_wg(); continue; }
        if (e.tipo == EV_FECHAR) break;
        if (jan_redimensionar(jg, &e)) { desenhar_wg(); continue; }
        if (e.tipo != EV_CLIQUE && e.tipo != EV_SOLTOU && e.tipo != EV_TECLA && e.tipo != EV_RODA) continue;
        int q = lista_tratar(jg, &lista_conf, &e);
        if (q == LISTA_ATIVOU) conectar_sel();
        else if (q != LISTA_NADA) msg_gui[0] = 0;
        else if (botao_tratar(jg, &b_wg_con, &e)) conectar_sel();
        else if (botao_tratar(jg, &b_wg_des, &e)) {
            msg_gui[0] = 0;
            int r = quick_down(NULL, true);
            registrar("wg: desconectar -> %d", r);
            if (r == 0) wg_msg(true, _("VPN apagada."));
            else msg_ok = false;
        } else if (botao_tratar(jg, &b_wg_imp, &e)) importar();
        else if (botao_tratar(jg, &b_wg_bor, &e)) {
            if (lista_conf.sel >= 0 && lista_conf.sel < nconf &&
                confirmar(_("VPN (WireGuard)"), _("¿Borrar esta configuración? Se pierde su clave privada si no la tienes en otro sitio."))) {
                char c[160];
                formatar_texto(c, sizeof c, "%s/%s.conf", DIR_WG, conf_itens[lista_conf.sel]);
                if (apagar(c) == 0) wg_msg(true, _("Configuración borrada.")); else wg_msg(false, _("No se pudo borrar."));
                carregar_confs();
            }
        } else if (botao_tratar(jg, &b_wg_gen, &e)) gerar_claves();
        else if (botao_tratar(jg, &b_wg_cop, &e)) {
            if (wg_publica[0]) { copiar_texto(wg_publica, (unsigned)tam_texto(wg_publica)); wg_msg(true, _("Clave pública copiada.")); }
        }
        refrescar_wg();
        desenhar_wg();
    }
    jan_fechar(jg);
    return 0;
}
