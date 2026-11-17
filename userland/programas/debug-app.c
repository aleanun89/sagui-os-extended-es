// SPDX-License-Identifier: GPL-2.0-or-later
/* debug-app - DEBUG-APP: deja entrar a otro equipo de la red local, por SSH y
 * durante un rato, para depurar este (D362).
 *
 *   debug-app                 la ventana (la del menú Inicio, Sistema)
 *   debug-app --texto [OPCIONES]   lo mismo en el terminal o por SSH (como root:
 *                             «admin debug-app --texto»); Intro corta
 *     --puerto N              puerto (de serie 2222 o el primero libre de los 9 siguientes)
 *     --minutos N             duración, de 15 a 480 (de serie 120)
 *     --segundos N            duración corta, para las pruebas (5 o más)
 *     --permitir LISTA        quién puede entrar: «subred» (de serie), redes a.b.c.d/N,
 *                             direcciones, y «publicas» para dejar también las no privadas
 *     --ip A.B.C.D            la dirección que va en el código (de serie, la de la tarjeta)
 *     --guardar               guarda esas opciones como las de serie de esta cuenta
 *
 * Al permitir la depuración se genera un secreto de un solo uso con el
 * generador aleatorio del kernel y se lanza «sshd --depuracion» como root, con
 * la clave PÚBLICA que sale del secreto (el secreto no sale de este programa
 * más que en el código que se enseña). El código (debug_app_logica.h) lleva la
 * dirección, el puerto, el secreto y una huella de la clave del servidor
 * atada al secreto (dapp_huella);
 * se le pasa al otro equipo (tools/depurar-remoto.py). La sesión se acaba con
 * la primera conexión autenticada (cuando termina), al caducar, con «Cortar»
 * o al cerrar esta ventana o este programa: sshd lo nota porque se cierra la
 * tubería de su entrada. Registro: /home/.config/debug-app/debug-app.log; en
 * dmesg, una línea al abrir y otra al cerrar. */
#include "sagui.h"
#include "gui.h"
#include "contas.h"
#include "cripto.h"
#include "servicos.h"
#include "debug_app_logica.h"

#define CONF_PASTA "/home/.config/debug-app"
#define CONF_ARQ   CONF_PASTA "/debug-app.conf"
#define REGISTRO   CONF_PASTA "/debug-app.log"

/* ------------------------------------------------------------ opciones */

typedef struct {
    int  porta;            /* 0 = DAPP_PUERTO o el primero libre */
    int  segundos;
    char permitir[120];
    char ip[16];           /* vacío = la de la tarjeta */
} dapp_opcoes_t;

static dapp_opcoes_t op = { 0, DAPP_DEF_MIN * 60, "subred", "" };

static void conf_ler(void)
{
    int fd = abrir(CONF_ARQ, ABRIR_LER);
    if (fd < 0) return;
    static leitor_linhas_t l;
    char linha[200];
    leitor_iniciar(&l, fd, false);
    while (leitor_linha(&l, linha, sizeof linha, NULL) >= 0) {
        char *v = acha_char(linha, '=');
        if (!v || linha[0] == '#') continue;
        *v++ = 0;
        int n = texto_para_int(v);
        if (compara_texto(linha, "puerto") == 0 && n >= 0 && n < 65536) op.porta = n;
        else if (compara_texto(linha, "minutos") == 0 && n >= DAPP_MIN_MIN && n <= DAPP_MAX_MIN) op.segundos = n * 60;
        else if (compara_texto(linha, "permitir") == 0 && v[0]) copia_texto(op.permitir, v, sizeof op.permitir);
        else if (compara_texto(linha, "ip") == 0) copia_texto(op.ip, v, sizeof op.ip);
    }
    fechar(fd);
}

static bool conf_gravar_usuario(void)
{
    servicos_criar_pastas(CONF_PASTA);
    int fd = abrir(CONF_ARQ, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return false;
    imprimir_em(fd, "# DEBUG-APP (D362): las opciones de serie de esta cuenta\npuerto=%d\nminutos=%d\npermitir=%s\nip=%s\n",
                op.porta, op.segundos / 60, op.permitir, op.ip);
    fechar(fd);
    return true;
}

/* ------------------------------------------------------------ la sesión */

typedef struct {
    int  pid;              /* el «sshd --depuracion» */
    int  fd_ctl;           /* nuestra punta de su entrada: cerrarla corta la sesión */
    int  fd_av;            /* sus avisos «@...» */
    char lin[240];
    int  nlin;
    bool activa;           /* el sshd está vivo */
    bool pronta;           /* ya hay código */
    bool usada;            /* alguien entró con el código */
    unsigned fim;          /* tempo_ms() de la caducidad */
    uint32_t ip;
    int  porta;
    char codigo[DAPP_CODIGO_MAX];
    char huella[64];
    char estado[160];
    char motivo_fim[160];
    uint8_t segredo[DAPP_SEGREDO];
} sessao_t;

static sessao_t ss;

/* Lo que pasa, para quien mira (la ventana o el terminal). */
static void (*ao_evento)(const char *texto);

/* La dirección que irá en el código y si la red sirve; si no, el motivo. */
static bool rede_ok(const dapp_permitir_t *perm, uint32_t *ip, char *erro, int max)
{
    config_rede_t cr;
    memset(&cr, 0, sizeof cr);
    config_rede(REDE_CONSULTAR, &cr);
    if (!cr.ip) {
        copia_texto(erro, _("No hay red: este equipo no tiene dirección IP. Conecta el cable (o la Wi-Fi) y vuelve a probar."), (size_t)max);
        return false;
    }
    *ip = cr.ip;
    if (op.ip[0] && !texto_para_ip(op.ip, ip)) {
        formatar_texto(erro, (size_t)max, _("La dirección «%s» no es válida."), op.ip);
        return false;
    }
    if (!dapp_ip_privada(*ip) && !perm->publicas) {
        char t[16];
        formatar_texto(erro, (size_t)max, _("La dirección de este equipo (%s) no es privada: DEBUG-APP solo se abre en una red local."),
                       ip_para_texto(*ip, t));
        return false;
    }
    return true;
}

/* Lanza el sshd de la sesión. El proceso ya tiene que ser root (el hijo lo hereda). */
static bool sessao_abrir(char *erro, int max)
{
    dapp_permitir_t perm;
    char e2[120];
    memset(&ss, 0, sizeof ss);
    ss.fd_ctl = ss.fd_av = -1;
    if (dapp_permitir_ler(op.permitir, &perm, e2, sizeof e2) < 0) {
        formatar_texto(erro, (size_t)max, _("Direcciones permitidas: %s"), e2);
        return false;
    }
    if (!rede_ok(&perm, &ss.ip, erro, max)) return false;
    if (!sou_root()) {
        copia_texto(erro, _("Hace falta ser administrador."), (size_t)max);
        return false;
    }
    uint8_t semente[32], pub[32];
    char hexpub[65], seg[16], porta[8];
    cripto_aleatorio(ss.segredo, DAPP_SEGREDO);
    dapp_semente(ss.segredo, semente);
    ed25519_publica(pub, semente);
    cripto_apagar(semente, sizeof semente);
    dapp_hex(pub, 32, hexpub);
    formatar_texto(seg, sizeof seg, "%d", op.segundos);
    formatar_texto(porta, sizeof porta, "%d", op.porta);
    int pin[2], pout[2];
    if (criar_pipe(pin) < 0) { copia_texto(erro, _("No se pudo crear la tubería."), (size_t)max); return false; }
    if (criar_pipe(pout) < 0) {
        fechar(pin[0]);
        fechar(pin[1]);
        copia_texto(erro, _("No se pudo crear la tubería."), (size_t)max);
        return false;
    }
    char *argv[] = { "sshd", "--depuracion", porta, hexpub, seg, op.permitir, NULL };
    int g0 = duplicar(ENTRADA, 28), g1 = duplicar(SAIDA, 29);
    duplicar(pin[0], ENTRADA);
    duplicar(pout[1], SAIDA);
    int pid = executar("/bin/sshd", argv);
    duplicar(g0, ENTRADA);
    duplicar(g1, SAIDA);
    fechar(g0);
    fechar(g1);
    fechar(pin[0]);
    fechar(pout[1]);
    if (pid < 0) {
        fechar(pin[1]);
        fechar(pout[0]);
        formatar_texto(erro, (size_t)max, _("No se pudo lanzar sshd: %s"), nome_erro(pid));
        return false;
    }
    ss.pid = pid;
    ss.fd_ctl = pin[1];
    ss.fd_av = pout[0];
    ss.activa = true;
    ss.fim = tempo_ms() + (unsigned)op.segundos * 1000u;
    copia_texto(ss.estado, _("Preparando..."), sizeof ss.estado);
    return true;
}

static void sessao_acabou(const char *motivo)
{
    if (!ss.activa) return;
    ss.activa = false;
    ss.pronta = false;
    cripto_apagar(ss.segredo, sizeof ss.segredo);
    cripto_apagar(ss.codigo, sizeof ss.codigo);
    if (ss.fd_ctl >= 0) fechar(ss.fd_ctl);
    if (ss.fd_av >= 0) fechar(ss.fd_av);
    ss.fd_ctl = ss.fd_av = -1;
    if (!ss.motivo_fim[0]) copia_texto(ss.motivo_fim, motivo, sizeof ss.motivo_fim);
    formatar_texto(ss.estado, sizeof ss.estado, _("Sesión cerrada: %s"), ss.motivo_fim);
    if (ao_evento) ao_evento(ss.estado);
}

/* Una línea «@...» del sshd. */
static void aviso(char *l)
{
    char t[200];
    if (comeca_com(l, "puerto ")) ss.porta = texto_para_int(l + 7);
    else if (comeca_com(l, "huella ")) copia_texto(ss.huella, l + 7, sizeof ss.huella);
    else if (comeca_com(l, "fp ")) {
        dapp_codigo_t c;
        memset(&c, 0, sizeof c);
        c.ip = ss.ip;
        c.porta = (unsigned)ss.porta;
        memcpy(c.segredo, ss.segredo, DAPP_SEGREDO);
        uint8_t hash_chave[32];
        if (dapp_de_hex(l + 3, hash_chave, 32) && ss.porta > 0) {
            dapp_huella(ss.segredo, hash_chave, c.huella);   /* atada al secreto (v2) */
            ss.pronta = dapp_codificar(&c, ss.codigo, sizeof ss.codigo);
        }
        cripto_apagar(&c, sizeof c);
    } else if (compara_texto(l, "escuchando") == 0) {
        copia_texto(ss.estado, _("Esperando la conexión del otro equipo."), sizeof ss.estado);
        if (ao_evento) ao_evento(ss.estado);
    } else if (comeca_com(l, "conexion ")) {
        formatar_texto(ss.estado, sizeof ss.estado, _("Conexión de %s: comprobando la clave..."), l + 9);
        if (ao_evento) ao_evento(ss.estado);
    } else if (comeca_com(l, "conectado ")) {
        ss.usada = true;
        cripto_apagar(ss.codigo, sizeof ss.codigo);   /* ya no vale: no se sigue enseñando */
        formatar_texto(ss.estado, sizeof ss.estado, _("CONECTADO desde %s (como root). El código ya no vale."), l + 10);
        if (ao_evento) ao_evento(ss.estado);
        formatar_texto(t, sizeof t, _("DEBUG-APP: %s ha entrado como administrador"), l + 10);
        aviso_tela(t, NULL);
    } else if (comeca_com(l, "rechazada ")) {
        formatar_texto(t, sizeof t, _("Conexión rechazada: %s"), l + 10);
        if (ao_evento) ao_evento(t);
    } else if (comeca_com(l, "fallo ")) {
        formatar_texto(t, sizeof t, _("Una conexión probó una clave que no vale (%s de %d)."), l + 6, DAPP_FALLOS_MAX);
        if (ao_evento) ao_evento(t);
    } else if (comeca_com(l, "fin ")) {
        copia_texto(ss.motivo_fim, l + 4, sizeof ss.motivo_fim);
    }
}

/* Lee lo que haya dicho el sshd y mira si sigue vivo. true = cambió algo. */
static bool sessao_atender(void)
{
    bool mudou = false;
    int cod, pid;
    while ((pid = recolher(&cod)) > 0)   /* primero: así lo que dejó en la tubería se lee abajo */
        if (pid == ss.pid) { ss.pid = 0; mudou = true; }
    while (ss.activa && ss.fd_av >= 0) {
        int d = disponivel(ss.fd_av);
        if (d == ERRO_PIPE_QUEBRADO) { sessao_acabou(_("el servidor de la sesión terminó")); return true; }
        if (d <= 0) break;
        char b[128];
        int n = ler(ss.fd_av, b, d < (int)sizeof b ? (unsigned)d : sizeof b);
        if (n <= 0) break;
        for (int i = 0; i < n; i++) {
            if (b[i] == '\n') {
                ss.lin[ss.nlin] = 0;
                if (ss.lin[0] == '@') aviso(ss.lin + 1);
                ss.nlin = 0;
                mudou = true;
            } else if (ss.nlin < (int)sizeof ss.lin - 1) ss.lin[ss.nlin++] = b[i];
        }
    }
    if (ss.activa && !ss.pid) { sessao_acabou(_("el servidor de la sesión terminó")); mudou = true; }
    return mudou;
}

/* Corta: cierra la entrada del sshd (que mata la sesión entera) y espera un poco a que lo diga. */
static void sessao_cortar(void)
{
    if (!ss.activa) return;
    if (ss.fd_ctl >= 0) { fechar(ss.fd_ctl); ss.fd_ctl = -1; }
    unsigned ate = tempo_ms() + 4000;
    while (ss.activa && (int)(ate - tempo_ms()) > 0) {
        sessao_atender();
        if (ss.activa) dormir(50);
    }
    sessao_acabou(_("cortada"));
}

static void registro_local(const char *fmt, const char *a)
{
    servicos_criar_pastas(CONF_PASTA);
    int fd = abrir(REGISTRO, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_ACRESCENTAR);
    if (fd < 0) return;
    char hora[32], t[200];
    fuso_data_t d;
    agora_local(&d);
    formatar_data(hora, sizeof hora, "%Y-%m-%d %H:%M:%S", &d);
    formatar_texto(t, sizeof t, fmt, a);
    imprimir_em(fd, "%s [%d] debug-app (uid %u): %s\n", hora, meu_pid(), meu_uid(), t);
    fechar(fd);
}

static void resta_texto(char *t, int max)
{
    int s = ss.activa ? (int)(ss.fim - tempo_ms()) / 1000 : 0;
    if (s < 0) s = 0;
    formatar_texto(t, (size_t)max, "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
}

/* ------------------------------------------------------------ modo texto */

static void ev_texto(const char *t) { imprimir("debug-app: %s\n", t); }

static int modo_texto(int argc, char **argv)
{
    bool guardar = false, perm_linha = false;
    conf_ler();
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (compara_texto(a, "--guardar") == 0) { guardar = true; continue; }
        if (!v) { imprimir_em(ERROS, _("debug-app: falta el valor de %s\n"), a); return 2; }
        int n = texto_para_int(v);
        i++;
        if (compara_texto(a, "--puerto") == 0 && n > 0 && n < 65536) op.porta = n;
        else if (compara_texto(a, "--minutos") == 0 && n >= DAPP_MIN_MIN && n <= DAPP_MAX_MIN) op.segundos = n * 60;
        else if (compara_texto(a, "--segundos") == 0 && n >= 5 && n <= DAPP_MAX_MIN * 60) op.segundos = n;
        else if (compara_texto(a, "--permitir") == 0) { copia_texto(op.permitir, v, sizeof op.permitir); perm_linha = true; }
        else if (compara_texto(a, "--ip") == 0) copia_texto(op.ip, v, sizeof op.ip);
        else { imprimir_em(ERROS, _("debug-app: opción o valor no válido: %s %s\n"), a, v); return 2; }
    }
    /* El fichero de opciones está en /home, donde escribe la cuenta normal: que
     * no pueda abrir la sesión de root a Internet sin que quien la abre lo diga
     * (en la ventana, la casilla pide su propia confirmación). */
    if (!perm_linha) {
        dapp_permitir_t p;
        char e[120];
        if (dapp_permitir_ler(op.permitir, &p, e, sizeof e) == 0 && p.publicas) {
            imprimir_em(ERROS, _("debug-app: las opciones guardadas permiten direcciones públicas; para eso hay que decirlo con --permitir\n"));
            return 1;
        }
    }
    if (!sou_root()) {
        imprimir_em(ERROS, _("debug-app: hace falta ser administrador: usa «admin debug-app --texto»\n"));
        return 1;
    }
    if (guardar && !conf_gravar_usuario()) imprimir_em(ERROS, _("debug-app: no se pudieron guardar las opciones\n"));
    char erro[200];
    ao_evento = ev_texto;
    if (!sessao_abrir(erro, sizeof erro)) {
        imprimir_em(ERROS, "debug-app: %s\n", erro);
        return 1;
    }
    registro_local("sesión pedida desde el terminal (%s)", op.permitir);
    unsigned ate = tempo_ms() + 15000;
    while (ss.activa && !ss.pronta && (int)(ate - tempo_ms()) > 0) {
        sessao_atender();
        if (!ss.pronta) dormir(50);
    }
    if (!ss.pronta) {
        sessao_cortar();
        imprimir_em(ERROS, _("debug-app: no se pudo abrir la sesión: %s\n"), ss.motivo_fim[0] ? ss.motivo_fim : _("sin respuesta de sshd"));
        return 1;
    }
    char ip[16], r[16];
    resta_texto(r, sizeof r);
    imprimir(_("ATENCIÓN: quien tenga este código entrará en este equipo como ADMINISTRADOR (root).\n"));
    imprimir(_("Código: %s\n"), ss.codigo);
    imprimir(_("Dirección: %s  puerto %d\n"), ip_para_texto(ss.ip, ip), ss.porta);
    imprimir(_("Huella del servidor: %s\n"), ss.huella);
    imprimir(_("Permitidas: %s\n"), op.permitir);
    imprimir(_("Caduca en %s. Vale para una sola conexión. Pulsa Intro para cortar.\n"), r);
    bool entrada = true;
    while (ss.activa) {
        sessao_atender();
        if (entrada) {
            int d = disponivel(ENTRADA);
            if (d < 0 && d != ERRO_PIPE_QUEBRADO) entrada = false;       /* no se puede mirar sin bloquear */
            else if (d == ERRO_PIPE_QUEBRADO) entrada = false;
            else if (d > 0) {
                char b[64];
                int n = ler(ENTRADA, b, sizeof b);
                for (int i = 0; i < n; i++)
                    if (b[i] == '\n' || b[i] == '\r' || b[i] == 'q' || b[i] == 3) { sessao_cortar(); break; }
            }
        }
        if (ss.activa) dormir(100);
    }
    registro_local("sesión cerrada: %s", ss.motivo_fim);
    return 0;
}

/* ------------------------------------------------------------ ventana */

#define VL 660
#define VA 470
#define COR_ALERTA 0xB02020

static jan_t *j;
static caixa_t c_ip, c_porta, c_min, c_perm;
static casilla_t k_pub;
static botao_t b_abrir = { 0, 0, 260, 30, NULL, false }, b_cortar = { 0, 0, 160, 30, NULL, false },
               b_copiar = { 0, 0, 150, 26, NULL, false };
static char msg[200], ultimo_ev[200];
static bool msg_erro;

static void ev_janela(const char *t) { copia_texto(ultimo_ev, t, sizeof ultimo_ev); }

static void desenhar(void)
{
    int L = j->larg, A = j->alt, y = 12;
    char t[240];
    g_retangulo(j, 0, 0, L, A, COR_FUNDO);
    if (!ss.activa) {
        rotulo(j, 12, y, _("Depuración remota por la red local"), COR_DESTAQUE);
        y += 24;
        rotulo(j, 12, y, _("Deja que otro equipo de tu red entre en este por SSH para hacer pruebas."), COR_TEXTO);
        y += 18;
        rotulo(j, 12, y, _("Quien tenga el código entrará como ADMINISTRADOR (root): todo el sistema."), COR_ALERTA);
        y += 18;
        rotulo(j, 12, y, _("Una sola conexión; caduca sola; «Cortar» o cerrar la ventana la termina."), COR_TEXTO_FRACO);
        y += 30;
        static const char *const et[] = { N_("Dirección del código:"), N_("Puerto (0 = 2222 o libre):"),
                                           N_("Duración (minutos, 15-480):"), N_("Direcciones permitidas:") };
        caixa_t *cx[] = { &c_ip, &c_porta, &c_min, &c_perm };
        for (int i = 0; i < 4; i++) {
            rotulo(j, 12, y + 6, _(et[i]), COR_TEXTO);
            cx[i]->x = 240;
            cx[i]->y = y;
            cx[i]->l = L - 252;
            caixa_desenhar(j, cx[i]);
            y += 34;
        }
        rotulo(j, 12, y, _("«subred», redes (192.168.5.0/24) o direcciones, separadas por comas"), COR_TEXTO_FRACO);
        y += 24;
        k_pub.x = 12;
        k_pub.y = y;
        casilla_desenhar(j, &k_pub);
        rotulo(j, 32, y, _("Permitir también direcciones públicas (peligroso: solo detrás de una VPN)"), k_pub.marcada ? COR_ALERTA : COR_TEXTO);
        y += 34;
        b_abrir.texto = _("Permitir depuración remota");
        b_abrir.x = 12;
        b_abrir.y = y;
        botao_desenhar(j, &b_abrir);
        y += 44;
        if (msg[0]) rotulo(j, 12, y, msg, msg_erro ? COR_ALERTA : COR_TEXTO);
        else rotulo(j, 12, y, _("Pide la contraseña de administrador. Más información: docs/debug-app.md"), COR_TEXTO_FRACO);
    } else {
        g_retangulo(j, 0, 0, L, 30, COR_ALERTA);
        rotulo(j, 12, 7, _("SESIÓN REMOTA ACTIVA: quien tenga el código entra como root"), COR_BRANCO);
        y = 44;
        if (ss.pronta && !ss.usada) {
            rotulo(j, 12, y, _("Código (díctalo o cópialo al otro equipo):"), COR_TEXTO);
            y += 22;
            g_retangulo(j, 12, y - 4, L - 24, 26, COR_BRANCO);
            g_moldura(j, 12, y - 4, L - 24, 26, COR_BORDA);
            g_texto(j, 20, y + 1, ss.codigo, COR_TEXTO);
            y += 30;
            b_copiar.texto = _("Copiar código");
            b_copiar.x = 12;
            b_copiar.y = y;
            botao_desenhar(j, &b_copiar);
            y += 36;
        } else if (ss.usada) {
            rotulo(j, 12, y, _("El código ya se ha usado: no vale para nadie más."), COR_TEXTO);
            y += 30;
        } else {
            rotulo(j, 12, y, _("Preparando el código..."), COR_TEXTO);
            y += 30;
        }
        char ip[16];
        formatar_texto(t, sizeof t, _("Dirección: %s, puerto %d"), ip_para_texto(ss.ip, ip), ss.porta);
        rotulo(j, 12, y, t, COR_TEXTO);
        y += 20;
        formatar_texto(t, sizeof t, _("Huella del servidor: %s"), ss.huella);
        rotulo(j, 12, y, t, COR_TEXTO);
        y += 20;
        char r[16];
        resta_texto(r, sizeof r);
        formatar_texto(t, sizeof t, _("Caduca en: %s"), r);
        rotulo(j, 12, y, t, COR_TEXTO);
        y += 20;
        formatar_texto(t, sizeof t, _("Permitidas: %s"), op.permitir);
        rotulo(j, 12, y, t, COR_TEXTO_FRACO);
        y += 28;
        rotulo(j, 12, y, ss.estado, COR_DESTAQUE);
        y += 20;
        if (ultimo_ev[0] && compara_texto(ultimo_ev, ss.estado) != 0) rotulo(j, 12, y, ultimo_ev, COR_TEXTO_FRACO);
        b_cortar.texto = _("Cortar");
        b_cortar.x = L - 12 - b_cortar.l;
        b_cortar.y = A - 12 - b_cortar.a;
        botao_desenhar(j, &b_cortar);
        if (msg[0]) rotulo(j, 12, A - 34, msg, msg_erro ? COR_ALERTA : COR_TEXTO);
    }
    jan_mostrar(j);
}

static void titulo(void)
{
    titulo_janela(j->id, ss.activa ? _("DEBUG-APP: SESIÓN REMOTA ACTIVA") : "DEBUG-APP");
}

static void permitir(void)
{
    msg[0] = 0;
    msg_erro = true;
    copia_texto(op.ip, c_ip.texto, sizeof op.ip);
    op.porta = texto_para_int(c_porta.texto);
    int min = texto_para_int(c_min.texto);
    if (op.porta < 0 || op.porta > 65535) { copia_texto(msg, _("El puerto no es válido."), sizeof msg); return; }
    if (min < DAPP_MIN_MIN || min > DAPP_MAX_MIN) { copia_texto(msg, _("La duración va de 15 a 480 minutos."), sizeof msg); return; }
    op.segundos = min * 60;
    /* «publicas» va en la lista: es lo que lee el sshd */
    char lista[120];
    copia_texto(lista, c_perm.texto[0] ? c_perm.texto : "subred", sizeof lista);
    bool tinha = false;
    {
        dapp_permitir_t p;
        char e[120];
        if (dapp_permitir_ler(lista, &p, e, sizeof e) == 0) tinha = p.publicas;
        if (k_pub.marcada && !tinha) junta_texto(lista, ",publicas", sizeof lista);
    }
    if (tinha && !k_pub.marcada) { copia_texto(msg, _("La lista dice «publicas»: marca la casilla para confirmarlo."), sizeof msg); return; }
    copia_texto(op.permitir, lista, sizeof op.permitir);
    dapp_permitir_t p;
    char e[120];
    uint32_t ip;
    if (dapp_permitir_ler(op.permitir, &p, e, sizeof e) < 0) { formatar_texto(msg, sizeof msg, _("Direcciones permitidas: %s"), e); return; }
    if (!rede_ok(&p, &ip, msg, sizeof msg)) return;
    if (p.publicas && !confirmar(_("DEBUG-APP"), _("Has permitido direcciones públicas: cualquiera de Internet que llegue a este puerto podría probar el código. ¿Seguro?")))
        return;
    if (!confirmar(_("DEBUG-APP"),
                   _("La sesión remota ejecutará órdenes como ADMINISTRADOR (root): podrá leer, cambiar y borrar cualquier cosa. "
                     "Dale el código solo a quien vaya a depurar este equipo. ¿Abrir la sesión?")))
        return;
    conf_gravar_usuario();
    bool era_root = sou_root();
    if (!era_root && elevar_grafico(_("abrir una sesión de depuración remota como administrador (root)")) < 0) {
        copia_texto(msg, _("Cancelado: hacen falta permisos de administrador."), sizeof msg);
        registro_local("sesión pedida desde la ventana: %s", "permiso denegado");
        return;
    }
    bool ok = sessao_abrir(msg, sizeof msg);
    if (!era_root) elevacao_soltar();   /* el sshd ya es root; esta ventana no necesita seguir siéndolo */
    if (!ok) return;
    registro_local("sesión pedida desde la ventana (%s)", op.permitir);
    msg[0] = 0;
    ultimo_ev[0] = 0;
    aviso_tela(_("DEBUG-APP: depuración remota permitida (root). Ciérrala con «Cortar»."), NULL);
    titulo();
}

static int janela(void)
{
    conf_ler();
    j = jan_abrir("DEBUG-APP", VL, VA);
    if (!j) { imprimir_em(ERROS, _("debug-app: no se pudo abrir la ventana (¿hay escritorio?); usa «debug-app --texto»\n")); return 1; }
    ao_evento = ev_janela;
    copia_texto(c_ip.texto, op.ip, sizeof c_ip.texto);
    if (!c_ip.texto[0]) {
        config_rede_t cr;
        memset(&cr, 0, sizeof cr);
        config_rede(REDE_CONSULTAR, &cr);
        if (cr.ip) ip_para_texto(cr.ip, c_ip.texto);
    }
    formatar_texto(c_porta.texto, sizeof c_porta.texto, "%d", op.porta);
    formatar_texto(c_min.texto, sizeof c_min.texto, "%d", op.segundos / 60);
    copia_texto(c_perm.texto, op.permitir, sizeof c_perm.texto);
    {
        dapp_permitir_t p;
        char e[80];
        k_pub.marcada = dapp_permitir_ler(op.permitir, &p, e, sizeof e) == 0 && p.publicas;
    }
    caixa_t *cx[] = { &c_ip, &c_porta, &c_min, &c_perm };
    for (int i = 0; i < 4; i++) cx[i]->cursor = (int)tam_texto(cx[i]->texto);
    desenhar();
    evento_t e;
    unsigned ultimo = 0;
    for (;;) {
        int r = esperar_evento(j->id, &e, ss.activa ? 250 : 1000);
        bool mudou = false;
        if (ss.activa) {
            bool estava = ss.activa;
            mudou = sessao_atender();
            if (estava && !ss.activa) {
                copia_texto(msg, ss.estado, sizeof msg);
                msg_erro = false;
                aviso_tela(_("DEBUG-APP: la sesión remota se ha cerrado."), NULL);
                registro_local("sesión cerrada: %s", ss.motivo_fim);
                titulo();
            }
        }
        if (r != 1) {
            if (mudou || (ss.activa && tempo_ms() - ultimo >= 1000)) { ultimo = tempo_ms(); desenhar(); }
            continue;
        }
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo != EV_CLIQUE && e.tipo != EV_SOLTOU && e.tipo != EV_TECLA) continue;
        if (!ss.activa) {
            for (int i = 0; i < 4; i++)
                if (caixa_tratar(j, cx[i], &e)) { permitir(); break; }
            if (casilla_tratar(&k_pub, &e)) mudou = true;
            if (botao_tratar(j, &b_abrir, &e)) permitir();
        } else {
            if (ss.pronta && !ss.usada && botao_tratar(j, &b_copiar, &e)) {
                copiar_texto(ss.codigo, (unsigned)tam_texto(ss.codigo));
                copia_texto(msg, _("Código copiado al portapapeles."), sizeof msg);
                msg_erro = false;
            }
            if (botao_tratar(j, &b_cortar, &e)) {
                sessao_cortar();
                copia_texto(msg, ss.estado, sizeof msg);
                msg_erro = false;
                registro_local("sesión cerrada: %s", ss.motivo_fim);
                aviso_tela(_("DEBUG-APP: la sesión remota se ha cerrado."), NULL);
                titulo();
            }
        }
        desenhar();
    }
    if (ss.activa) {
        sessao_cortar();
        registro_local("sesión cerrada al cerrar la ventana: %s", ss.motivo_fim);
        aviso_tela(_("DEBUG-APP: la sesión remota se ha cerrado."), NULL);
    }
    jan_fechar(j);
    return 0;
}

static const char *AYUDA =
    "uso: debug-app                    la ventana DEBUG-APP\n"
    "     debug-app --texto [--puerto N] [--minutos 15-480] [--permitir LISTA] [--ip A.B.C.D] [--guardar]\n"
    "Deja entrar a otro equipo de la red local por SSH, como administrador (root), con un\n"
    "código de un solo uso que caduca. En el otro equipo: tools/depurar-remoto.py CODIGO.\n"
    "LISTA: «subred» (de serie), redes a.b.c.d/N o direcciones, y «publicas». Ver docs/debug-app.md.\n";

int principal(int argc, char **argv)
{
    if (argc > 1 && (compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "--help") == 0 ||
                     compara_texto(argv[1], "-h") == 0)) {
        escrever_texto(SAIDA, AYUDA);
        return 0;
    }
    if (argc > 1 && compara_texto(argv[1], "--texto") == 0) return modo_texto(argc, argv);
    if (argc > 1) { escrever_texto(ERROS, AYUDA); return 2; }
    return janela();
}
