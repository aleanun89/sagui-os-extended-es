// SPDX-License-Identifier: GPL-2.0-or-later
/* ssh - cliente SSH (D317): entrar desde SaguiOS en otra máquina.
 *
 *   ssh [-p PUERTO] [-i CLAVE] [-l USUARIO] [-t|-T] [-n] [-q] [-o OPCIÓN] [usuario@]máquina [orden...]
 *
 * Sin orden, una sesión interactiva con terminal remoto (pty-req con el TERM y
 * el tamaño del Terminal; window-change cuando cambia). Con orden, la ejecuta
 * y devuelve su código de salida; con la entrada y la salida por tuberías
 * sirve a otros programas (scp, executar()).
 *
 * Protocolo SSH-2 con los algoritmos del servidor de SaguiOS (D250), que son
 * los que prefiere un OpenSSH moderno: curve25519-sha256 (con el modo estricto
 * contra «Terrapin»), clave del servidor ssh-ed25519 y chacha20-poly1305@openssh.com.
 * El transporte (paquetes, H, claves) es el de ssh_transporte.h, el mismo del sshd.
 *
 * Seguridad (D317):
 *   - la clave del servidor se comprueba con /home/.ssh/known_hosts: la primera
 *     vez se enseña la huella y se pregunta; si cambió, se rechaza sin opción
 *     de saltárselo (hay que borrar la línea a mano);
 *   - autenticación con la clave ed25519 /home/.ssh/id_ed25519 (ssh-keygen)
 *     y, si el servidor la admite, con contraseña (sin eco);
 *   - las contraseñas y las claves no se escriben en ningún sitio y se borran
 *     de la memoria en cuanto se usan;
 *   - todo lo que llega del servidor se lee con límites, y los textos que se
 *     enseñan (motivos, avisos) se limpian de secuencias de control.
 *
 * Teclas en la sesión interactiva: Ctrl+C y Ctrl+D van al otro lado;
 * «~.» al principio de una línea corta la conexión; «~?» la ayuda. */
#include "sagui.h"
#include "contas.h"
#include "cripto.h"
#include "servicos.h"
#include "sshd_logica.h"
#include "ssh_transporte.h"
#include "ssh_cliente_logica.h"

#define VERSAO_SSH     "SSH-2.0-SaguiOS_ssh_1.0"
#define PKT_MAX        35000u
#define RX_TAM         (2 * PKT_MAX + 64)
#define MSG_MAX        (PKT_MAX - 64)
#define DADOS_MAX      32768u             /* lo más que aceptamos en un paquete de canal */
#define JANELA_LOCAL   (1024u * 1024u)    /* lo que el servidor puede mandar sin que se le confirme */
#define PACOTES_MAX    0x80000000u
#define CONECTAR_MS    15000u
#define SAIR_ERRO      255

#define DIR_SSH        "/home/.ssh"
#define KNOWN_HOSTS    DIR_SSH "/known_hosts"
#define CHAVE_PADRAO   DIR_SSH "/id_ed25519"

/* Mensajes */
#define M_DESCONECTAR   1
#define M_IGNORAR       2
#define M_NAO_IMPLEM    3
#define M_DEPURAR       4
#define M_PEDIR_SERVICO 5
#define M_SERVICO_OK    6
#define M_EXT_INFO      7
#define M_KEXINIT       20
#define M_NEWKEYS       21
#define M_ECDH_INIT     30
#define M_ECDH_RESP     31
#define M_AUT_PEDIDO    50
#define M_AUT_FALHA     51
#define M_AUT_OK        52
#define M_AUT_AVISO     53
#define M_AUT_60        60   /* PK_OK, PASSWD_CHANGEREQ o INFO_REQUEST, según el método */
#define M_AUT_INFO_RESP 61
#define M_GLOBAL        80
#define M_GLOBAL_OK     81
#define M_GLOBAL_FALHA  82
#define M_CANAL_ABRIR   90
#define M_CANAL_ABERTO  91
#define M_CANAL_RECUSA  92
#define M_CANAL_JANELA  93
#define M_CANAL_DADOS   94
#define M_CANAL_EXTRA   95
#define M_CANAL_EOF     96
#define M_CANAL_FECHAR  97
#define M_CANAL_PEDIDO  98
#define M_CANAL_OK      99
#define M_CANAL_FALHA   100

#define D_PROTOCOLO     2
#define D_KEX           3
#define D_HOST_CHAVE    9
#define D_PELO_APLIC    11

#define ALG_KEX_1  "curve25519-sha256"
#define ALG_KEX_2  "curve25519-sha256@libssh.org"
#define ALG_CHAVE  SSH_ED25519
#define ALG_CIFRA  "chacha20-poly1305@openssh.com"
#define ALG_MAC    "hmac-sha2-256"   /* con un cifrado AEAD no se usa; la lista no puede ir vacía */
#define ALG_COMP   "none"

static const char *AYUDA =
    "uso: ssh [-p PUERTO] [-i CLAVE] [-l USUARIO] [-t|-T] [-n] [-q] [-o OPCIÓN] [usuario@]máquina [orden...]\n"
    "Entra en otra máquina por SSH (D317). Sin orden, una sesión con terminal; con orden, la ejecuta\n"
    "y termina con su código de salida.\n"
    "  -p PUERTO   puerto del servidor (22)\n"
    "  -i CLAVE    clave privada ed25519 (" CHAVE_PADRAO ", la de ssh-keygen)\n"
    "  -l USUARIO  usuario remoto (o usuario@máquina; si no, el de esta cuenta)\n"
    "  -t / -T     pedir / no pedir terminal remoto\n"
    "  -n          no leer la entrada (para lanzarlo desde otro programa)\n"
    "  -q          sin avisos\n"
    "  -o StrictHostKeyChecking=ask|accept-new|yes   qué hacer con una máquina desconocida\n"
    "  -o BatchMode=yes   no preguntar nada (ni contraseñas ni huellas)\n"
    "Las máquinas conocidas están en " KNOWN_HOSTS ". Si la clave de una cambia, la conexión se\n"
    "rechaza: hay que borrar su línea a mano (o con ssh-keygen -R máquina) si el cambio es legítimo.\n"
    "En la sesión: «~.» al principio de una línea corta la conexión; «~?» la ayuda.\n";

/* ------------------------------------------------------------ estado */

static int      sk = -1;
static uint8_t *rx, *tx, *msg;
static unsigned rx_ini, rx_fim;
static bool     cif_rx, cif_tx;
static uint8_t  ch_rx[64], ch_tx[64];
static uint32_t seq_rx, seq_tx, pacotes_chave;
static bool     rx_tam_ok;
static uint32_t rx_tam;
static uint8_t  sessao_id[32];
static bool     tem_sessao, estrito;
static char     v_servidor[256];
static uint8_t  srv_pub[32];             /* la clave del servidor del primer intercambio */
static ssh_escritor_t m;
static uint8_t  enchimento[256];
static unsigned enchimento_pos = sizeof enchimento;

/* opciones */
static const char *host, *usuario, *arq_chave;
static int   porta = 22;
static int   quer_pty = -1;              /* -1 = según haya orden y terminal */
static bool  sem_entrada, quieto, lote;
static int   politica = 0;               /* 0 ask, 1 accept-new, 2 yes (solo las conocidas) */
static char  ordem[2048];

/* el terminal local; los datos del canal van por fd_ent y fd_sai (la entrada
 * y la salida, o los archivos de --entrada-de/--salida-a, que usa scp para que
 * el terminal siga libre para preguntar) */
static int   fd_ent = ENTRADA, fd_sai = SAIDA;
static bool  term_local;                 /* la entrada 0 es un terminal: se puede preguntar */
static bool  entrada_term, saida_term, erros_term, bruto;

static bool modo_xterm;   /* D320: el Terminal de SaguiOS está en modo xterm (CSI ? 7700 h) */

static void restaurar_terminal(void)
{
    if (modo_xterm) {   /* pantalla normal, atributos y juego de caracteres de siempre; teclas del sagsh */
        escrever_tudo(SAIDA, "\033(B\033[0m\033[?25h\033[?7700l", 27);
        modo_xterm = false;
    }
    if (bruto) { modo_terminal(TERM_LINHA); bruto = false; }
}

static void terminar(int codigo) __attribute__((noreturn));
static void terminar(int codigo)
{
    restaurar_terminal();
    if (sk >= 0) fechar(sk);
    cripto_apagar(ch_rx, sizeof ch_rx);
    cripto_apagar(ch_tx, sizeof ch_tx);
    sair(codigo);
}

/* Un texto que viene del otro lado, para enseñarlo: UTF-8 -> Latin-1 y sin
 * secuencias de control (un servidor hostil no debe poder mover el cursor,
 * borrar la pantalla ni escribir en el título). */
static void texto_limpo(const uint8_t *o, unsigned n, char *dst, unsigned max, bool saltos)
{
    ssh_utf8_t u = { 0 };
    unsigned k = 0;
    for (unsigned i = 0; i < n && k + 1 < max; i++) {
        uint8_t c1[4];
        unsigned m1 = ssh_utf8_latin1(&u, o + i, 1, c1);
        for (unsigned j = 0; j < m1 && k + 1 < max; j++) {
            uint8_t c = c1[j];
            if (c == '\n' && saltos) dst[k++] = '\n';
            else if (c == '\t') dst[k++] = ' ';
            else if (c < 0x20 || c == 0x7F || (c >= 0x80 && c < 0xA0)) { if (c != '\r') dst[k++] = '?'; }
            else dst[k++] = (char)c;
        }
    }
    dst[k] = 0;
}

static void aviso(const char *fmt, ...)
{
    char t[400];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(t, sizeof t, fmt, ap);
    va_end(ap);
    if (bruto) {   /* en modo bruto el «\n» del Terminal ya vuelve a la columna 0; la consola también */
        imprimir_em(ERROS, "\nssh: %s\n", t);
    } else imprimir_em(ERROS, "ssh: %s\n", t);
}

static void fatal(const char *fmt, ...) __attribute__((noreturn));
static void fatal(const char *fmt, ...)
{
    char t[400];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(t, sizeof t, fmt, ap);
    va_end(ap);
    restaurar_terminal();
    imprimir_em(ERROS, "ssh: %s\n", t);
    terminar(SAIR_ERRO);
}

/* ------------------------------------------------------------ transporte */

static void enviar(const uint8_t *pl, unsigned n)
{
    if (n > MSG_MAX) fatal("mensaje propio demasiado grande");
    unsigned pad = ssh_pacote_relleno(n, cif_tx);
    if (enchimento_pos + pad > sizeof enchimento) { cripto_aleatorio(enchimento, sizeof enchimento); enchimento_pos = 0; }
    unsigned total = ssh_pacote_montar(tx, PKT_MAX + 64, pl, n, enchimento + enchimento_pos, cif_tx ? ch_tx : NULL, seq_tx);
    enchimento_pos += pad;
    if (!total) fatal("mensaje propio demasiado grande");
    seq_tx++;
    if (++pacotes_chave >= PACOTES_MAX) fatal("demasiados paquetes con las mismas claves");
    if (escrever_tudo(sk, tx, total) != (int)total) fatal("la conexión se cerró");
}

static void m_iniciar(unsigned tipo) { se_iniciar(&m, msg, MSG_MAX); se_u8(&m, tipo); }
static void m_enviar(void)
{
    if (m.erro) fatal("mensaje propio demasiado grande");
    enviar(msg, m.n);
}

static void desconectar(unsigned codigo, const char *texto) __attribute__((noreturn));
static void desconectar(unsigned codigo, const char *texto)
{
    static bool ja;
    if (!ja && sk >= 0) {
        ja = true;
        m_iniciar(M_DESCONECTAR);
        se_u32(&m, codigo);
        se_cstr(&m, texto);
        se_cstr(&m, "es");
        m_enviar();
    }
    fatal("%s", texto);
}

/* Lee del socket lo que haya. ms = 0: sin esperar. -> 1 leyó, 0 nada, -1 cerrado o plazo vencido */
static int rx_encher(unsigned ms)
{
    if (rx_ini == rx_fim) rx_ini = rx_fim = 0;
    else if (rx_ini && rx_fim > RX_TAM - PKT_MAX / 2) {
        memmove(rx, rx + rx_ini, rx_fim - rx_ini);
        rx_fim -= rx_ini;
        rx_ini = 0;
    }
    if (rx_fim >= RX_TAM) return -1;
    if (ms == 0) {
        int d = disponivel(sk);
        if (d == 0) return 0;
        if (d < 0) return -1;
    }
    tempo_limite(sk, ms ? ms : 1000);
    int n = ler(sk, rx + rx_fim, RX_TAM - rx_fim);
    if (n <= 0) return -1;
    rx_fim += (unsigned)n;
    return 1;
}

/* Un paquete completo del búfer: 1 = hay (válido hasta el siguiente rx_encher), 0 = falta. */
static int pacote_tentar(const uint8_t **pl, unsigned *pn)
{
    unsigned total;
    int r = ssh_pacote_extrair(rx + rx_ini, rx_fim - rx_ini, cif_rx ? ch_rx : NULL, seq_rx, PKT_MAX,
                               &rx_tam_ok, &rx_tam, pl, pn, &total);
    if (r == SSH_PKT_FALTA) return 0;
    if (r == SSH_PKT_TAMANHO) {
        if (cif_rx) fatal("el servidor mandó un paquete de tamaño no válido (¿manipulado?)");
        desconectar(D_PROTOCOLO, "tamaño de paquete no válido");
    }
    if (r == SSH_PKT_MAC) fatal("la etiqueta (MAC) de un paquete del servidor no coincide: conexión cortada");
    if (r == SSH_PKT_RELLENO) desconectar(D_PROTOCOLO, "relleno de paquete no válido");
    rx_ini += total;
    seq_rx++;
    if (++pacotes_chave >= PACOTES_MAX) desconectar(D_KEX, "demasiados paquetes con las mismas claves");
    return 1;
}

static void pacote_esperar(const uint8_t **pl, unsigned *pn, unsigned ms)
{
    for (;;) {
        if (pacote_tentar(pl, pn)) return;
        if (rx_encher(ms) < 0) fatal("el servidor cerró la conexión o no respondió a tiempo");
    }
}

/* El motivo de un DISCONNECT del servidor, y se termina. */
static void desconectado(const uint8_t *pl, unsigned pn) __attribute__((noreturn));
static void desconectado(const uint8_t *pl, unsigned pn)
{
    ssh_leitor_t l;
    unsigned n;
    char t[200];
    sl_iniciar(&l, pl + 1, pn - 1);
    uint32_t cod = sl_u32(&l);
    const uint8_t *s = sl_texto(&l, &n);
    texto_limpo(s, l.erro ? 0 : n, t, sizeof t, false);
    fatal("el servidor cortó la conexión (%u): %s", (unsigned)cod, t[0] ? t : "sin motivo");
}

/* El siguiente mensaje "de verdad": se saltan los de relleno, depuración y
 * extensiones; un DISCONNECT termina. */
static unsigned proximo(const uint8_t **pl, unsigned *pn, unsigned ms)
{
    for (;;) {
        pacote_esperar(pl, pn, ms);
        unsigned t = (*pl)[0];
        if (t == M_IGNORAR || t == M_DEPURAR || t == M_NAO_IMPLEM || t == M_EXT_INFO) continue;
        if (t == M_DESCONECTAR) desconectado(*pl, *pn);
        return t;
    }
}

/* ------------------------------------------------------------ known_hosts */

static void pasta_ssh(void)
{
    criar_pasta("/home");
    criar_pasta(DIR_SSH);
}

/* Una línea de la entrada (en modo línea), sin el salto. -1 = fin. */
static int pergunta(const char *texto, char *r, unsigned max)
{
    imprimir_em(ERROS, "%s", texto);
    int n = ler_linha(ENTRADA, r, max);
    if (n < 0) { r[0] = 0; return -1; }
    r[max - 1] = 0;
    int t = (int)tam_texto(r);
    while (t > 0 && (r[t - 1] == '\r' || r[t - 1] == '\n' || r[t - 1] == ' ')) r[--t] = 0;
    return t;
}

static void verificar_maquina(const uint8_t pub[32])
{
    char nome[300], huella[64];
    if (!ssh_kh_nome(host, porta, nome, sizeof nome)) fatal("nombre de máquina no válido para known_hosts: %s", host);
    ssh_huella(pub, huella, sizeof huella);
    ssh_kh_t v;
    ssh_kh_iniciar(&v);
    int fd = abrir(KNOWN_HOSTS, ABRIR_LER);
    if (fd >= 0) {
        static leitor_linhas_t l;
        static char linha[2048];
        leitor_iniciar(&l, fd, false);
        while (leitor_linha(&l, linha, sizeof linha, NULL) >= 0) ssh_kh_somar(&v, linha, nome, pub);
        fechar(fd);
    }
    if (v.estado == SSH_KH_CONHECIDA) return;
    if (v.estado == SSH_KH_REVOCADA) {
        escrever_texto(ERROS, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                              "@  ATENCIÓN: LA CLAVE DE ESTA MÁQUINA ESTÁ REVOCADA         @\n"
                              "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
        imprimir_em(ERROS, "La clave ED25519 de %s (%s) está marcada como @revoked en %s.\n", nome, huella, KNOWN_HOSTS);
        desconectar(D_HOST_CHAVE, "clave del servidor revocada");
    }
    if (v.estado == SSH_KH_MUDOU) {
        /* en varias llamadas: imprimir_em tiene un búfer limitado */
        escrever_texto(ERROS,
            "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
            "@  ATENCIÓN: ¡LA CLAVE DE LA MÁQUINA REMOTA HA CAMBIADO!    @\n"
            "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
        imprimir_em(ERROS, "Alguien podría estar haciéndose pasar por %s (un ataque de intermediario),\n"
                           "o puede que la máquina haya cambiado de clave (reinstalada, otra máquina con su IP).\n", nome);
        imprimir_em(ERROS, "La huella de la clave ED25519 que presenta ahora es:\n  %s\n", huella);
        imprimir_em(ERROS, "La clave guardada está en %s, línea %d.\n", KNOWN_HOSTS, v.linha_mudou);
        imprimir_em(ERROS, "La conexión se rechaza. Si el cambio es legítimo, borra esa línea a mano\n"
                           "(o con «ssh-keygen -R %s») y vuelve a conectar.\n", nome);
        desconectar(D_HOST_CHAVE, "la clave del servidor no es la guardada");
    }
    /* desconocida */
    if (politica == 2) {
        imprimir_em(ERROS, "ssh: la máquina %s no está en %s (StrictHostKeyChecking=yes); su huella ED25519 es %s\n",
                    nome, KNOWN_HOSTS, huella);
        desconectar(D_HOST_CHAVE, "máquina desconocida");
    }
    if (politica == 0) {
        if (lote || !term_local) {
            imprimir_em(ERROS, "ssh: la máquina %s no está en %s y no se puede preguntar (sin terminal);\n"
                               "     su huella ED25519 es %s. Conecta una vez a mano para aceptarla.\n",
                        nome, KNOWN_HOSTS, huella);
            desconectar(D_HOST_CHAVE, "máquina desconocida");
        }
        imprimir_em(ERROS, "No se puede comprobar la identidad de la máquina «%s».\n"
                           "La huella de su clave ED25519 es %s.\n", nome, huella);
        if (v.outro_tipo)
            imprimir_em(ERROS, "Aviso: %s tiene guardada una clave de otro tipo (RSA, ECDSA...), que este cliente no usa.\n",
                        KNOWN_HOSTS);
        char r[16];
        for (;;) {
            int n = pergunta("¿Seguro que quieres conectar? (sí/no): ", r, sizeof r);
            if (n < 0) desconectar(D_HOST_CHAVE, "máquina no aceptada");
            if (compara_texto(r, "sí") == 0 || compara_texto(r, "si") == 0 || compara_texto(r, "s\xC3\xAD") == 0 ||
                compara_texto(r, "yes") == 0)
                break;
            if (compara_texto(r, "no") == 0 || compara_texto(r, "n") == 0) desconectar(D_HOST_CHAVE, "máquina no aceptada");
            imprimir_em(ERROS, "Escribe «sí» o «no».\n");
        }
    }
    /* se añade al final de known_hosts */
    pasta_ssh();
    char linha[400], pubtxt[120];
    ssh_linha_publica(pub, pubtxt, sizeof pubtxt);
    formatar_texto(linha, sizeof linha, "%s %s\n", nome, pubtxt);
    bool salto = false;
    info_arquivo_t ia;
    if (info_arquivo(KNOWN_HOSTS, &ia) == 0 && ia.tamanho > 0) {
        fd = abrir(KNOWN_HOSTS, ABRIR_LER);
        if (fd >= 0) {
            char c = '\n';
            posicionar(fd, (int)ia.tamanho - 1, POS_INICIO);
            ler(fd, &c, 1);
            fechar(fd);
            salto = c != '\n';
        }
    }
    fd = abrir(KNOWN_HOSTS, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_ACRESCENTAR);
    if (fd < 0 || (salto && escrever_tudo(fd, "\n", 1) != 1) || escrever_tudo(fd, linha, (unsigned)tam_texto(linha)) < 0) {
        if (fd >= 0) fechar(fd);
        aviso("no se pudo guardar la máquina en %s: se volverá a preguntar", KNOWN_HOSTS);
        return;
    }
    sincronizar(fd);
    fechar(fd);
    if (!quieto) imprimir_em(ERROS, "Aviso: «%s» (ED25519) añadida a la lista de máquinas conocidas.\n", nome);
}

/* ------------------------------------------------------------ intercambio de claves */

static void esperar_kex(unsigned tipo, const uint8_t **pl, unsigned *pn, bool tolerar)
{
    for (;;) {
        pacote_esperar(pl, pn, 30000);
        unsigned t = (*pl)[0];
        if (t == tipo) return;
        if (tolerar && (t == M_IGNORAR || t == M_DEPURAR || t == M_NAO_IMPLEM)) continue;
        if (t == M_DESCONECTAR) desconectado(*pl, *pn);
        desconectar(D_PROTOCOLO, "mensaje inesperado durante el intercambio de claves");
    }
}

/* El intercambio entero. "is_srv" = el KEXINIT del servidor si ya llegó (lo
 * pidió él para renovar las claves); NULL en el primero (se manda el nuestro
 * y se espera el suyo). */
static void trocar_chaves(const uint8_t *is_srv, unsigned nis_srv)
{
    static uint8_t ic[400], is[PKT_MAX];
    unsigned nis;
    bool inicial = !tem_sessao;
    ssh_escritor_t e;
    se_iniciar(&e, ic, sizeof ic);
    se_u8(&e, M_KEXINIT);
    uint8_t galleta[16];
    cripto_aleatorio(galleta, 16);
    se_bytes(&e, galleta, 16);
    se_cstr(&e, inicial ? ALG_KEX_1 "," ALG_KEX_2 ",ext-info-c,kex-strict-c-v00@openssh.com" : ALG_KEX_1 "," ALG_KEX_2);
    se_cstr(&e, ALG_CHAVE);
    se_cstr(&e, ALG_CIFRA);
    se_cstr(&e, ALG_CIFRA);
    se_cstr(&e, ALG_MAC);
    se_cstr(&e, ALG_MAC);
    se_cstr(&e, ALG_COMP);
    se_cstr(&e, ALG_COMP);
    se_cstr(&e, "");
    se_cstr(&e, "");
    se_u8(&e, 0);
    se_u32(&e, 0);
    enviar(ic, e.n);

    const uint8_t *pl;
    unsigned pn;
    if (!is_srv) {
        esperar_kex(M_KEXINIT, &pl, &pn, true);
        is_srv = pl;
        nis_srv = pn;
    }
    if (nis_srv > sizeof is) desconectar(D_PROTOCOLO, "KEXINIT demasiado grande");
    memcpy(is, is_srv, nis_srv);
    nis = nis_srv;
    const uint8_t *lst[10];
    unsigned nl[10];
    bool segue;
    if (!ssh_kexinit_ler(is, nis, lst, nl, &segue)) desconectar(D_PROTOCOLO, "KEXINIT del servidor mal formado");
    if (inicial) {
        estrito = ssh_lista_tem(lst[0], nl[0], "kex-strict-s-v00@openssh.com");
        /* en modo estricto, el KEXINIT tiene que haber sido el primer paquete del servidor */
        if (estrito && seq_rx != 1) desconectar(D_PROTOCOLO, "modo estricto: paquetes antes del KEXINIT");
    }
    const char *kex = ssh_lista_tem(lst[0], nl[0], ALG_KEX_1) ? ALG_KEX_1 : ssh_lista_tem(lst[0], nl[0], ALG_KEX_2) ? ALG_KEX_2 : NULL;
    const char *falta = NULL;
    if (!kex) falta = "intercambio de claves (este cliente usa curve25519-sha256)";
    else if (!ssh_lista_tem(lst[1], nl[1], ALG_CHAVE)) falta = "clave del servidor (este cliente usa ssh-ed25519)";
    else if (!ssh_lista_tem(lst[2], nl[2], ALG_CIFRA) || !ssh_lista_tem(lst[3], nl[3], ALG_CIFRA))
        falta = "cifrado (este cliente usa chacha20-poly1305@openssh.com)";
    else if (!ssh_lista_tem(lst[6], nl[6], ALG_COMP) || !ssh_lista_tem(lst[7], nl[7], ALG_COMP)) falta = "compresión (none)";
    if (falta) {
        char t[200];
        formatar_texto(t, sizeof t, "sin algoritmo común de %s", falta);
        desconectar(D_KEX, t);
    }
    bool tolerar = !(estrito && inicial);
    if (segue && !(ssh_primeiro_eh(lst[0], nl[0], kex) && ssh_primeiro_eh(lst[1], nl[1], ALG_CHAVE)))
        pacote_esperar(&pl, &pn, 30000);   /* el servidor adivinó mal: su siguiente paquete no vale (RFC 4253, 7) */

    uint8_t priv[32], qc[32], qs[32], k[32], k_mp[40], hh[32], assin[64], pub[32];
    cripto_aleatorio(priv, 32);
    x25519_base(qc, priv);
    m_iniciar(M_ECDH_INIT);
    se_texto(&m, qc, 32);
    m_enviar();

    esperar_kex(M_ECDH_RESP, &pl, &pn, tolerar);
    const uint8_t *ks;
    unsigned nks;
    if (!ssh_ecdh_resposta_ler(pl, pn, &ks, &nks, pub, qs, assin)) {
        cripto_apagar(priv, sizeof priv);
        desconectar(D_KEX, "respuesta del intercambio de claves no válida (¿clave que no es ssh-ed25519?)");
    }
    x25519(k, priv, qs);
    cripto_apagar(priv, sizeof priv);
    unsigned zero = 0;
    for (int i = 0; i < 32; i++) zero |= k[i];
    if (!zero) desconectar(D_KEX, "clave efímera del servidor no válida");
    ssh_escritor_t ek;
    se_iniciar(&ek, k_mp, sizeof k_mp);
    se_mpint(&ek, k, 32);
    cripto_apagar(k, sizeof k);
    ssh_hash_troca(hh, VERSAO_SSH, v_servidor, ic, e.n, is, nis, ks, nks, qc, qs, k_mp, ek.n);
    if (!ed25519_verificar(assin, hh, 32, pub)) {
        cripto_apagar(k_mp, sizeof k_mp);
        desconectar(D_KEX, "la firma del servidor no es válida");
    }
    if (inicial) {
        memcpy(sessao_id, hh, 32);
        tem_sessao = true;
        memcpy(srv_pub, pub, 32);
        verificar_maquina(pub);       /* antes de mandar nada cifrado: puede preguntar o rechazar */
    } else if (memcmp(pub, srv_pub, 32) != 0) {
        desconectar(D_HOST_CHAVE, "la clave del servidor cambió al renovar las claves");
    }

    m_iniciar(M_NEWKEYS);
    m_enviar();
    uint8_t nova_rx[64];
    ssh_derivar(ch_tx, k_mp, ek.n, hh, 'C', sessao_id);   /* cliente -> servidor */
    ssh_derivar(nova_rx, k_mp, ek.n, hh, 'D', sessao_id); /* servidor -> cliente */
    cripto_apagar(k_mp, sizeof k_mp);
    cif_tx = true;
    if (estrito) seq_tx = 0;

    esperar_kex(M_NEWKEYS, &pl, &pn, tolerar);
    memcpy(ch_rx, nova_rx, 64);
    cripto_apagar(nova_rx, sizeof nova_rx);
    cif_rx = true;
    rx_tam_ok = false;
    if (estrito) seq_rx = 0;
    pacotes_chave = 0;
}

/* La línea de identificación del servidor (RFC 4253, 4.2): antes puede
 * haber otras líneas (que no empiecen por «SSH-»); con límites. */
static void ler_versao(void)
{
    unsigned linhas = 0;
    for (;;) {
        for (unsigned i = rx_ini; i < rx_fim; i++) {
            if (rx[i] != '\n') continue;
            unsigned n = i - rx_ini;
            if (n && rx[i - 1] == '\r') n--;
            if (n >= sizeof v_servidor) fatal("identificación del servidor demasiado larga");
            bool ssh = n >= 4 && memcmp(rx + rx_ini, "SSH-", 4) == 0;
            if (ssh) {
                memcpy(v_servidor, rx + rx_ini, n);
                v_servidor[n] = 0;
            }
            rx_ini = i + 1;
            if (!ssh) {
                if (++linhas > 50) fatal("el servidor no se identifica como SSH");
                i = rx_ini - 1;
                continue;
            }
            for (char *p = v_servidor; *p; p++)
                if ((unsigned char)*p < 0x20 || (unsigned char)*p > 0x7E) fatal("identificación del servidor no válida");
            if (!comeca_com(v_servidor, "SSH-2.0-") && !comeca_com(v_servidor, "SSH-1.99-")) {
                char t[64];
                copia_texto(t, v_servidor, sizeof t);
                fatal("el servidor no habla SSH-2 («%s»)", t);
            }
            return;
        }
        if (rx_fim - rx_ini > 255) fatal("identificación del servidor demasiado larga");
        if (rx_fim >= RX_TAM) fatal("el servidor no se identifica como SSH");
        if (rx_encher(15000) < 0) fatal("el servidor no se identificó");
    }
}

/* ------------------------------------------------------------ autenticación (RFC 4252, 4256) */

static uint8_t ch_semente[32], ch_publica[32];
static bool    tem_chave;

static void carregar_chave(void)
{
    const char *arq = arq_chave ? arq_chave : CHAVE_PADRAO;
    int fd = abrir(arq, ABRIR_LER);
    if (fd < 0) {
        if (arq_chave) aviso("no se puede abrir la clave %s: %s", arq, nome_erro(fd));
        return;
    }
    static char t[4096];
    int n = ler(fd, t, sizeof t - 1);
    fechar(fd);
    if (n <= 0) { cripto_apagar(t, sizeof t); return; }
    int r = ssh_chave_privada_ler(t, (unsigned)n, ch_semente, ch_publica, NULL, 0);
    cripto_apagar(t, sizeof t);
    if (r == SSH_CH_OK) { tem_chave = true; return; }
    aviso("la clave %s no se puede usar: %s", arq,
          r == SSH_CH_CIFRADA ? "tiene frase de paso (este cliente aún no las admite)" :
          r == SSH_CH_TIPO ? "no es ed25519" : r == SSH_CH_ROTA ? "está dañada" : "no es una clave privada de OpenSSH");
}

static void aut_cabecalho(const char *metodo)
{
    m_iniciar(M_AUT_PEDIDO);
    se_cstr(&m, usuario);
    se_cstr(&m, "ssh-connection");
    se_cstr(&m, metodo);
}

/* La respuesta a una petición: 1 = dentro, 0 = rechazada (con la lista de
 * métodos en "metodos"), 60 = un SSH_MSG 60 (en pl/pn). Enseña los avisos. */
static int aut_resposta(char *metodos, unsigned max, const uint8_t **pl, unsigned *pn)
{
    for (;;) {
        unsigned t = proximo(pl, pn, 120000);
        ssh_leitor_t l;
        sl_iniciar(&l, *pl + 1, *pn - 1);
        if (t == M_AUT_OK) return 1;
        if (t == M_AUT_FALHA) {
            unsigned n;
            const uint8_t *s = sl_texto(&l, &n);
            if (l.erro) desconectar(D_PROTOCOLO, "respuesta de autenticación mal formada");
            if (n >= max) n = max - 1;
            memcpy(metodos, s, n);
            metodos[n] = 0;
            for (unsigned i = 0; i < n; i++)
                if ((unsigned char)metodos[i] < 0x20 || (unsigned char)metodos[i] > 0x7E) metodos[i] = '?';
            return 0;
        }
        if (t == M_AUT_AVISO) {
            unsigned n;
            const uint8_t *s = sl_texto(&l, &n);
            if (!l.erro && !quieto) {
                static char b[1024];
                texto_limpo(s, n, b, sizeof b, true);
                imprimir_em(ERROS, "%s%s", b, (b[0] && b[tam_texto(b) - 1] != '\n') ? "\n" : "");
            }
            continue;
        }
        if (t == M_AUT_60) return 60;
        desconectar(D_PROTOCOLO, "mensaje inesperado durante la autenticación");
    }
}

static bool lista_tem(const char *lista, const char *nome)
{
    return ssh_lista_tem((const uint8_t *)lista, (unsigned)tam_texto(lista), nome);
}

static bool pode_perguntar(void) { return term_local && !lote; }

static bool aut_chave(char *metodos, unsigned max)
{
    uint8_t blob[SSH_BLOB_ED25519], assin[64];
    static uint8_t dados[1024];
    ssh_blob_montar(blob, ch_publica);
    /* lo firmado: id de sesión y la petición hasta la clave (RFC 4252, 7) */
    ssh_escritor_t e;
    se_iniciar(&e, dados, sizeof dados);
    se_texto(&e, sessao_id, 32);
    se_u8(&e, M_AUT_PEDIDO);
    se_cstr(&e, usuario);
    se_cstr(&e, "ssh-connection");
    se_cstr(&e, "publickey");
    se_u8(&e, 1);
    se_cstr(&e, SSH_ED25519);
    se_texto(&e, blob, sizeof blob);
    if (e.erro) fatal("usuario demasiado largo");
    ed25519_assinar(assin, dados, e.n, ch_semente, ch_publica);
    aut_cabecalho("publickey");
    se_u8(&m, 1);
    se_cstr(&m, SSH_ED25519);
    se_texto(&m, blob, sizeof blob);
    se_u32(&m, 4 + (sizeof SSH_ED25519 - 1) + 4 + 64);
    se_cstr(&m, SSH_ED25519);
    se_texto(&m, assin, 64);
    m_enviar();
    const uint8_t *pl;
    unsigned pn;
    int r = aut_resposta(metodos, max, &pl, &pn);
    if (r == 60) desconectar(D_PROTOCOLO, "respuesta de autenticación inesperada");
    return r == 1;
}

static bool aut_senha(char *metodos, unsigned max)
{
    for (int i = 0; i < 3; i++) {
        char p[160], s[256];
        formatar_texto(p, sizeof p, "Contraseña de %s@%s: ", usuario, host);
        if (ler_senha(p, s, sizeof s) < 0) { cripto_apagar(s, sizeof s); fatal("cancelado"); }
        aut_cabecalho("password");
        se_u8(&m, 0);
        se_cstr(&m, s);
        cripto_apagar(s, sizeof s);
        bool erro = m.erro;
        if (!erro) enviar(msg, m.n);
        cripto_apagar(msg, m.n);
        if (erro) fatal("contraseña demasiado larga");
        const uint8_t *pl;
        unsigned pn;
        int r = aut_resposta(metodos, max, &pl, &pn);
        if (r == 1) return true;
        if (r == 60) { aviso("el servidor pide cambiar la contraseña: hazlo desde otro cliente"); return false; }
        if (!lista_tem(metodos, "password")) return false;
        imprimir_em(ERROS, "Permiso denegado; prueba otra vez.\n");
    }
    return false;
}

/* keyboard-interactive (RFC 4256): lo que usa OpenSSH con PAM para la contraseña. */
static bool aut_teclado(char *metodos, unsigned max)
{
    aut_cabecalho("keyboard-interactive");
    se_cstr(&m, "");
    se_cstr(&m, "");
    m_enviar();
    for (int rodadas = 0; rodadas < 8; rodadas++) {
        const uint8_t *pl;
        unsigned pn;
        int r = aut_resposta(metodos, max, &pl, &pn);
        if (r == 1) return true;
        if (r == 0) return false;
        ssh_leitor_t l;
        unsigned nn, ni, nlg;
        sl_iniciar(&l, pl + 1, pn - 1);
        const uint8_t *nome = sl_texto(&l, &nn);
        const uint8_t *instr = sl_texto(&l, &ni);
        sl_texto(&l, &nlg);
        uint32_t np = sl_u32(&l);
        if (l.erro || np > 8) desconectar(D_PROTOCOLO, "petición de contraseña mal formada");
        static char t[600];
        texto_limpo(nome, nn, t, sizeof t, true);
        if (t[0]) imprimir_em(ERROS, "%s\n", t);
        texto_limpo(instr, ni, t, sizeof t, true);
        if (t[0]) imprimir_em(ERROS, "%s\n", t);
        static char resp[8][128];
        for (uint32_t i = 0; i < np; i++) {
            unsigned npr;
            const uint8_t *pr = sl_texto(&l, &npr);
            bool eco = sl_u8(&l) != 0;
            if (l.erro) desconectar(D_PROTOCOLO, "petición de contraseña mal formada");
            texto_limpo(pr, npr, t, 200, false);
            int k = eco ? pergunta(t, resp[i], sizeof resp[i]) : ler_senha(t, resp[i], sizeof resp[i]);
            if (k < 0) { cripto_apagar(resp, sizeof resp); fatal("cancelado"); }
        }
        if (l.pos != l.n) { cripto_apagar(resp, sizeof resp); desconectar(D_PROTOCOLO, "petición de contraseña mal formada"); }
        m_iniciar(M_AUT_INFO_RESP);
        se_u32(&m, np);
        for (uint32_t i = 0; i < np; i++) se_cstr(&m, resp[i]);   /* Latin-1 -> el otro lado espera UTF-8: solo ASCII es seguro */
        cripto_apagar(resp, sizeof resp);
        bool erro = m.erro;
        if (!erro) enviar(msg, m.n);
        cripto_apagar(msg, m.n);
        if (erro) fatal("respuesta demasiado larga");
    }
    return false;
}

static void autenticar(void)
{
    m_iniciar(M_PEDIR_SERVICO);
    se_cstr(&m, "ssh-userauth");
    m_enviar();
    const uint8_t *pl;
    unsigned pn;
    for (;;) {
        unsigned t = proximo(&pl, &pn, 30000);
        if (t == M_SERVICO_OK) break;
        if (t == M_AUT_AVISO) continue;
        desconectar(D_PROTOCOLO, "el servidor no aceptó el servicio ssh-userauth");
    }
    char metodos[256];
    aut_cabecalho("none");
    m_enviar();
    int r = aut_resposta(metodos, sizeof metodos, &pl, &pn);
    if (r == 1) return;
    if (r == 60) desconectar(D_PROTOCOLO, "respuesta de autenticación inesperada");
    carregar_chave();
    bool probou = false;
    if (tem_chave && lista_tem(metodos, "publickey")) {
        probou = true;
        bool ok = aut_chave(metodos, sizeof metodos);
        cripto_apagar(ch_semente, sizeof ch_semente);
        if (ok) return;
    }
    cripto_apagar(ch_semente, sizeof ch_semente);
    if (pode_perguntar() && lista_tem(metodos, "password")) {
        probou = true;
        if (aut_senha(metodos, sizeof metodos)) return;
    }
    if (pode_perguntar() && lista_tem(metodos, "keyboard-interactive")) {
        probou = true;
        if (aut_teclado(metodos, sizeof metodos)) return;
    }
    if (!probou && !tem_chave && lista_tem(metodos, "publickey") && !quieto)
        aviso("no hay clave en %s (créala con «ssh-keygen -t ed25519» y pon el .pub en el authorized_keys del servidor)",
              arq_chave ? arq_chave : CHAVE_PADRAO);
    fatal("permiso denegado (%s)", metodos);
}

/* ------------------------------------------------------------ el canal de la sesión (RFC 4254) */

static uint32_t remoto, jan_remota, max_remoto;
static uint32_t jan_local = JANELA_LOCAL, consumido;
static bool     aberto, eof_enviado, eof_remoto, fechar_enviado, fechado;
static int      codigo_saida = -1;
static bool     com_pty;
static int      cols = 80, lins = 24;
static bool     tamanho_terminal;        /* el Terminal contesta a ESC [ 18 t */
static unsigned ultima_pergunta_tam;
static bool     pergunta_pendente;
static ssh_escape_t esc;
static ssh_teclado_t teclado;
static ssh_utf8_t utf_saida, utf_erros;

static void canal_pedido(const char *tipo, bool resposta)
{
    m_iniciar(M_CANAL_PEDIDO);
    se_u32(&m, remoto);
    se_cstr(&m, tipo);
    se_u8(&m, resposta);
}

/* Mientras se espera una respuesta: atiende lo que no es esa respuesta. */
static void despachar(const uint8_t *pl, unsigned pn);

static bool esperar_resposta_canal(void)
{
    for (;;) {
        const uint8_t *pl;
        unsigned pn;
        unsigned t = proximo(&pl, &pn, 60000);
        if (t == M_CANAL_OK || t == M_CANAL_FALHA) {
            ssh_leitor_t l;
            sl_iniciar(&l, pl + 1, pn - 1);
            if (sl_u32(&l) != 0 || l.erro) desconectar(D_PROTOCOLO, "respuesta para un canal desconocido");
            return t == M_CANAL_OK;
        }
        despachar(pl, pn);
        if (fechado) terminar(codigo_saida >= 0 ? codigo_saida : SAIR_ERRO);
    }
}

static void abrir_canal(void)
{
    m_iniciar(M_CANAL_ABRIR);
    se_cstr(&m, "session");
    se_u32(&m, 0);
    se_u32(&m, JANELA_LOCAL);
    se_u32(&m, DADOS_MAX);
    m_enviar();
    for (;;) {
        const uint8_t *pl;
        unsigned pn;
        unsigned t = proximo(&pl, &pn, 60000);
        ssh_leitor_t l;
        sl_iniciar(&l, pl + 1, pn - 1);
        if (t == M_CANAL_ABERTO) {
            uint32_t meu = sl_u32(&l);
            remoto = sl_u32(&l);
            jan_remota = sl_u32(&l);
            max_remoto = sl_u32(&l);
            if (l.erro || meu != 0) desconectar(D_PROTOCOLO, "confirmación de canal mal formada");
            if (max_remoto > DADOS_MAX) max_remoto = DADOS_MAX;
            if (max_remoto < 64) desconectar(D_PROTOCOLO, "el servidor admite paquetes demasiado pequeños");
            aberto = true;
            return;
        }
        if (t == M_CANAL_RECUSA) {
            sl_u32(&l);
            uint32_t cod = sl_u32(&l);
            unsigned n;
            const uint8_t *s = sl_texto(&l, &n);
            char b[200];
            texto_limpo(s, l.erro ? 0 : n, b, sizeof b, false);
            fatal("el servidor no abrió la sesión (%u): %s", (unsigned)cod, b);
        }
        if (t == M_GLOBAL) { despachar(pl, pn); continue; }
        desconectar(D_PROTOCOLO, "mensaje inesperado al abrir la sesión");
    }
}

static void pedir_tamanho(void)
{
    if (!tamanho_terminal) return;
    escrever_texto(SAIDA, "\033[18t");
    ultima_pergunta_tam = tempo_ms();
    pergunta_pendente = true;
}

static void mandar_tamanho(bool pedido)
{
    if (pedido) {
        /* pty-req: TERM, columnas, filas, píxeles (0) y los modos: VERASE = 127 */
        const char *term = obter_env("TERM");
        static const uint8_t modos[] = { 3, 0, 0, 0, 127, 0 };
        canal_pedido("pty-req", true);
        se_cstr(&m, term && term[0] && tam_texto(term) < 64 ? term : "xterm");
        se_u32(&m, (uint32_t)cols);
        se_u32(&m, (uint32_t)lins);
        se_u32(&m, 0);
        se_u32(&m, 0);
        se_texto(&m, modos, sizeof modos);
        m_enviar();
    } else {
        canal_pedido("window-change", false);
        se_u32(&m, (uint32_t)cols);
        se_u32(&m, (uint32_t)lins);
        se_u32(&m, 0);
        se_u32(&m, 0);
        m_enviar();
    }
}

/* Lo que llega del otro lado -> la pantalla (o la tubería, tal cual). */
static void mostrar(int fd, ssh_utf8_t *u, const uint8_t *d, unsigned n)
{
    bool term = fd == fd_sai ? saida_term : erros_term;
    if (!term) {
        if (escrever_tudo(fd, d, n) < 0 && fd == fd_sai) desconectar(D_PELO_APLIC, "la salida se cerró");
        return;
    }
    static uint8_t a[4096], b[3 * 4096];
    while (n) {
        unsigned p = n > 1024 ? 1024 : n;
        unsigned k = ssh_utf8_latin1(u, d, p, a);   /* p <= 1024: con las cajas DEC puede crecer hasta 3*p + 3 */
        if (com_pty) k = ssh_retroceso_cursor(a, k, b);
        else memcpy(b, a, k);
        escrever_tudo(fd, b, k);
        d += p;
        n -= p;
    }
}

static void despachar(const uint8_t *pl, unsigned pn)
{
    ssh_leitor_t l;
    unsigned tipo = pl[0];
    sl_iniciar(&l, pl + 1, pn - 1);
    switch (tipo) {
    case M_IGNORAR: case M_DEPURAR: case M_NAO_IMPLEM: case M_EXT_INFO:
    case M_GLOBAL_OK: case M_GLOBAL_FALHA:
        return;
    case M_DESCONECTAR:
        desconectado(pl, pn);
    case M_KEXINIT:
        trocar_chaves(pl, pn);   /* el servidor renueva las claves */
        return;
    case M_GLOBAL: {               /* hostkeys-00@openssh.com, keepalive...: no se atienden */
        unsigned n;
        sl_texto(&l, &n);
        bool resp = sl_u8(&l) != 0;
        if (l.erro) desconectar(D_PROTOCOLO, "petición global mal formada");
        if (resp) { m_iniciar(M_GLOBAL_FALHA); m_enviar(); }
        return;
    }
    case M_CANAL_JANELA: {
        uint32_t id = sl_u32(&l), mais = sl_u32(&l);
        if (l.erro || id != 0 || !aberto || mais > 0xFFFFFFFFu - jan_remota) desconectar(D_PROTOCOLO, "ajuste de ventana no válido");
        jan_remota += mais;
        return;
    }
    case M_CANAL_DADOS:
    case M_CANAL_EXTRA: {
        uint32_t id = sl_u32(&l);
        uint32_t qual = tipo == M_CANAL_EXTRA ? sl_u32(&l) : 0;
        unsigned n;
        const uint8_t *d = sl_texto(&l, &n);
        if (l.erro || id != 0 || !aberto) desconectar(D_PROTOCOLO, "datos para un canal desconocido");
        if (n > jan_local || n > DADOS_MAX) desconectar(D_PROTOCOLO, "datos de canal fuera de la ventana");
        jan_local -= n;
        consumido += n;
        if (tipo == M_CANAL_DADOS) mostrar(fd_sai, &utf_saida, d, n);
        else if (qual == 1) mostrar(ERROS, &utf_erros, d, n);
        if (consumido >= JANELA_LOCAL / 2) {
            m_iniciar(M_CANAL_JANELA);
            se_u32(&m, remoto);
            se_u32(&m, consumido);
            m_enviar();
            jan_local += consumido;
            consumido = 0;
        }
        return;
    }
    case M_CANAL_EOF:
        if (sl_u32(&l) != 0 || l.erro) desconectar(D_PROTOCOLO, "fin de datos de un canal desconocido");
        eof_remoto = true;
        return;
    case M_CANAL_FECHAR:
        if (sl_u32(&l) != 0 || l.erro) desconectar(D_PROTOCOLO, "cierre de un canal desconocido");
        if (!fechar_enviado) {
            m_iniciar(M_CANAL_FECHAR);
            se_u32(&m, remoto);
            m_enviar();
            fechar_enviado = true;
        }
        fechado = true;
        return;
    case M_CANAL_PEDIDO: {
        uint32_t id = sl_u32(&l);
        char nome[64];
        sl_cstr(&l, nome, sizeof nome);
        bool resp = sl_u8(&l) != 0;
        if (l.erro || id != 0) desconectar(D_PROTOCOLO, "petición de canal mal formada");
        if (compara_texto(nome, "exit-status") == 0) {
            uint32_t c = sl_u32(&l);
            if (!l.erro) codigo_saida = c > 255 ? 255 : (int)c;
        } else if (compara_texto(nome, "exit-signal") == 0) {
            char sinal[32];
            sl_cstr(&l, sinal, sizeof sinal);
            if (!l.erro && !quieto) {
                for (char *p = sinal; *p; p++) if ((unsigned char)*p < 0x21 || (unsigned char)*p > 0x7E) *p = '?';
                aviso("la orden remota terminó por la señal %s", sinal);
            }
            codigo_saida = SAIR_ERRO;
        } else if (resp) {        /* keepalive@openssh.com y demás: «no sé» */
            m_iniciar(M_CANAL_FALHA);
            se_u32(&m, remoto);
            m_enviar();
        }
        return;
    }
    case M_CANAL_OK:
    case M_CANAL_FALHA:
        return;
    default:
        m_iniciar(M_NAO_IMPLEM);
        se_u32(&m, seq_rx - 1);
        m_enviar();
        return;
    }
}

static void mandar_dados(const uint8_t *d, unsigned n)
{
    while (n) {
        unsigned p = n;
        if (p > max_remoto) p = max_remoto;
        if (p > jan_remota) p = jan_remota;
        if (!p) {
            /* sin ventana: se atiende al servidor hasta que la dé */
            const uint8_t *pl;
            unsigned pn;
            pacote_esperar(&pl, &pn, 120000);
            despachar(pl, pn);
            if (fechado) return;
            continue;
        }
        m_iniciar(M_CANAL_DADOS);
        se_u32(&m, remoto);
        se_texto(&m, d, p);
        m_enviar();
        jan_remota -= p;
        d += p;
        n -= p;
    }
}

static void mandar_eof(void)
{
    if (eof_enviado || fechar_enviado) return;
    m_iniciar(M_CANAL_EOF);
    se_u32(&m, remoto);
    m_enviar();
    eof_enviado = true;
}

static void ajuda_escape(void)
{
    imprimir_em(ERROS, "\nSecuencias de escape (al principio de una línea):\n"
                       "  ~.  cortar la conexión\n"
                       "  ~~  mandar un «~»\n"
                       "  ~?  esta ayuda\n");
}

/* Lo que se tecleó o llegó por la entrada -> el canal. */
static void entrada_dados(uint8_t *b, unsigned n)
{
    if (!com_pty || !entrada_term) {
        if (entrada_term) {   /* el Terminal escribe en Latin-1; el otro lado espera UTF-8 */
            static uint8_t u[2 * 4096];
            unsigned k = ssh_latin1_utf8(b, n, u);
            mandar_dados(u, k);
        } else mandar_dados(b, n);
        return;
    }
    if (modo_xterm) {   /* teclas ya en formato xterm (D320) */
        static uint8_t xc[2 * 4096 + 8];
        if (tamanho_terminal) {
            bool achou = false;
            int nl = lins, nc = cols;
            n = ssh_tamanho_resposta(b, n, &nl, &nc, &achou);
            if (achou) {
                pergunta_pendente = false;
                if (nl != lins || nc != cols) { lins = nl; cols = nc; mandar_tamanho(false); }
            }
        }
        static uint8_t sx[4096 + 8];
        unsigned k2 = 0;
        for (unsigned i = 0; i < n; i++) {
            uint8_t o[2];
            unsigned no;
            int a = ssh_escape_tecla(&esc, b[i], o, &no);
            if (a == SSH_ESC_SAIR) {
                if (k2) mandar_dados(xc, ssh_teclas_xterm_cru(sx, k2, xc));
                imprimir_em(ERROS, "\nConexión con %s cerrada.\n", host);
                m_iniciar(M_DESCONECTAR);
                se_u32(&m, D_PELO_APLIC);
                se_cstr(&m, "desconectado (~.)");
                se_cstr(&m, "es");
                m_enviar();
                terminar(SAIR_ERRO);
            }
            if (a == SSH_ESC_AJUDA) ajuda_escape();
            for (unsigned jj = 0; jj < no; jj++) sx[k2++] = o[jj];
        }
        if (k2) mandar_dados(xc, ssh_teclas_xterm_cru(sx, k2, xc));
        return;
    }
    if (tamanho_terminal) {
        bool achou = false;
        int nl = lins, nc = cols;
        n = ssh_tamanho_resposta(b, n, &nl, &nc, &achou);
        if (achou) {
            pergunta_pendente = false;
            if (nl != lins || nc != cols) {
                lins = nl;
                cols = nc;
                mandar_tamanho(false);
            }
        }
    }
    static uint8_t s[2 * 4096], x[4 * 2 * 4096 + 8];
    unsigned k = 0;
    for (unsigned i = 0; i < n; i++) {
        uint8_t o[2];
        unsigned no;
        int a = ssh_escape_tecla(&esc, b[i], o, &no);
        if (a == SSH_ESC_SAIR) {
            if (k) mandar_dados(x, ssh_teclas_xterm(&teclado, s, k, x));
            imprimir_em(ERROS, "\nConexión con %s cerrada.\n", host);
            m_iniciar(M_DESCONECTAR);
            se_u32(&m, D_PELO_APLIC);
            se_cstr(&m, "desconectado (~.)");
            se_cstr(&m, "es");
            m_enviar();
            terminar(SAIR_ERRO);   /* como OpenSSH: 255 */
        }
        if (a == SSH_ESC_AJUDA) ajuda_escape();
        for (unsigned j = 0; j < no; j++) s[k++] = o[j];
    }
    if (k) mandar_dados(x, ssh_teclas_xterm(&teclado, s, k, x));
}

/* Atiende a la vez la red y la entrada hasta que se cierre el canal. */
static void sessao(void)
{
    const uint8_t *pl;
    unsigned pn;
    bool eof_local = sem_entrada;
    int tipo_ent = tipo_descritor(fd_ent);
    if (eof_local) mandar_eof();
    unsigned visto = (unsigned)esperar_rede(0, 0);
    static uint8_t b[4096];
    while (!fechado) {
        bool fez = false;
        for (int k = 0; k < 16 && !fechado; k++) {
            if (pacote_tentar(&pl, &pn)) { despachar(pl, pn); fez = true; continue; }
            int e = rx_encher(0);
            if (e < 0) {
                if (codigo_saida >= 0) terminar(codigo_saida);
                fatal("el servidor cerró la conexión");
            }
            if (e == 0) break;
            fez = true;
        }
        if (fechado) break;
        if (!eof_local && jan_remota > 0) {
            unsigned cabe = sizeof b;
            if (com_pty && entrada_term) cabe = 1024;   /* las teclas pueden crecer al traducirlas */
            int d = tipo_ent == TIPO_ARQUIVO ? (int)cabe : disponivel(fd_ent);
            if (d == ERRO_PIPE_QUEBRADO) {
                eof_local = true;
                mandar_eof();
                fez = true;
            } else if (d > 0) {
                int n = ler(fd_ent, b, (unsigned)d < cabe ? (unsigned)d : cabe);
                if (n <= 0) {
                    eof_local = true;
                    mandar_eof();
                } else entrada_dados(b, (unsigned)n);
                fez = true;
            } else if (d < 0 && d != ERRO_PIPE_QUEBRADO && tipo_ent != TIPO_TERMINAL) {
                /* un descriptor sin «disponivel»: se lee bloqueando, como un archivo */
                tipo_ent = TIPO_ARQUIVO;
            }
        }
        if (com_pty && tamanho_terminal && (int)(tempo_ms() - ultima_pergunta_tam) >= 1000) pedir_tamanho();
        if (!fez) visto = (unsigned)esperar_rede(visto, (!eof_local) ? 20 : 500);
    }
    /* una pregunta de tamaño sin contestar: su respuesta no debe llegar al shell local */
    if (pergunta_pendente) {
        unsigned fim = tempo_ms() + 300;
        while ((int)(fim - tempo_ms()) > 0) {
            int d = disponivel(ENTRADA);
            if (d > 0) {
                int n = ler(ENTRADA, b, d < (int)sizeof b ? (unsigned)d : sizeof b);
                bool achou = false;
                if (n > 0) ssh_tamanho_resposta(b, (unsigned)n, &lins, &cols, &achou);
                if (achou) break;
            }
            dormir(10);
        }
    }
}

/* El tamaño del terminal local: la respuesta del Terminal a ESC [ 18 t, o
 * COLUMNS/LINES, o 80x24. Las teclas que lleguen mientras tanto no se pierden. */
static uint8_t adiantado[256];
static unsigned n_adiantado;

static void tamanho_inicial(void)
{
    const char *c = obter_env("COLUMNS"), *l = obter_env("LINES");
    if (c && texto_para_int(c) > 0 && texto_para_int(c) < 1000) cols = texto_para_int(c);
    if (l && texto_para_int(l) > 0 && texto_para_int(l) < 1000) lins = texto_para_int(l);
    const char *t = obter_env("TERM");
    if (!t || !comeca_com(t, "xterm")) return;   /* la consola del kernel no contesta */
    tamanho_terminal = true;
    pedir_tamanho();
    unsigned fim = tempo_ms() + 400;
    while ((int)(fim - tempo_ms()) > 0 && n_adiantado < sizeof adiantado) {
        int d = disponivel(ENTRADA);
        if (d > 0) {
            int n = ler(ENTRADA, adiantado + n_adiantado, (unsigned)(sizeof adiantado - n_adiantado));
            if (n > 0) n_adiantado += (unsigned)n;
            bool achou = false;
            n_adiantado = ssh_tamanho_resposta(adiantado, n_adiantado, &lins, &cols, &achou);
            if (achou) { pergunta_pendente = false; return; }
        }
        dormir(10);
    }
    tamanho_terminal = false;   /* este terminal no contesta: no se vuelve a preguntar */
    pergunta_pendente = false;
}

/* ------------------------------------------------------------ opciones */

static void uso(void)
{
    escrever_texto(ERROS, AYUDA);
    sair(SAIR_ERRO);
}

static void opcao_o(const char *o)
{
    char k[64];
    const char *v = acha_char(o, '=');
    if (!v) uso();
    unsigned n = (unsigned)(v - o);
    if (n >= sizeof k) uso();
    memcpy(k, o, n);
    k[n] = 0;
    v++;
    if (compara_texto(k, "StrictHostKeyChecking") == 0) {
        if (compara_texto(v, "ask") == 0) politica = 0;
        else if (compara_texto(v, "accept-new") == 0) politica = 1;
        else if (compara_texto(v, "yes") == 0) politica = 2;
        else {
            /* «no» u «off» no existen aquí: aceptar cualquier clave quita la protección (D317) */
            imprimir_em(ERROS, "ssh: StrictHostKeyChecking=%s no se admite (solo ask, accept-new o yes)\n", v);
            sair(SAIR_ERRO);
        }
    } else if (compara_texto(k, "BatchMode") == 0) lote = compara_texto(v, "yes") == 0;
    else if (compara_texto(k, "Port") == 0) porta = texto_para_int(v);
    else if (compara_texto(k, "User") == 0) usuario = v;
    else if (compara_texto(k, "IdentityFile") == 0) arq_chave = v;
    else {
        imprimir_em(ERROS, "ssh: opción -o %s no admitida\n", k);
        sair(SAIR_ERRO);
    }
}

int principal(int argc, char **argv)
{
    int i = 1;
    for (; i < argc; i++) {
        const char *a = argv[i];
        if (a[0] != '-' || !a[1]) break;
        if (compara_texto(a, "--") == 0) { i++; break; }
        if (compara_texto(a, "--ayuda") == 0 || compara_texto(a, "--help") == 0 || compara_texto(a, "-h") == 0) {
            escrever_texto(SAIDA, AYUDA);
            return 0;
        }
        if (compara_texto(a, "-V") == 0) { imprimir_em(ERROS, "%s\n", VERSAO_SSH); return 0; }
        /* para scp: los datos del canal desde/hacia un archivo, con el terminal libre para preguntar */
        if ((compara_texto(a, "--entrada-de") == 0 || compara_texto(a, "--salida-a") == 0) && i + 1 < argc) {
            bool ent = a[2] == 'e';
            const char *arq = argv[++i];
            int fd = ent ? abrir(arq, ABRIR_LER) : abrir(arq, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
            if (fd < 0) { imprimir_em(ERROS, "ssh: %s: %s\n", arq, nome_erro(fd)); return SAIR_ERRO; }
            if (ent) fd_ent = fd; else fd_sai = fd;
            continue;
        }
        /* opciones con valor, juntas («-p2222») o separadas */
        char op = a[1];
        if (op == 'p' || op == 'i' || op == 'l' || op == 'o') {
            const char *v = a[2] ? a + 2 : (i + 1 < argc ? argv[++i] : NULL);
            if (!v) uso();
            if (op == 'p') porta = texto_para_int(v);
            else if (op == 'i') arq_chave = v;
            else if (op == 'l') usuario = v;
            else opcao_o(v);
            continue;
        }
        for (const char *p = a + 1; *p; p++) {
            if (*p == 't') quer_pty = 1;
            else if (*p == 'T') quer_pty = 0;
            else if (*p == 'n') sem_entrada = true;
            else if (*p == 'q') quieto = true;
            else if (*p == 'v') { /* sin registro detallado: se acepta y se ignora */ }
            else uso();
        }
    }
    if (i >= argc) uso();
    static char destino[256];
    copia_texto(destino, argv[i++], sizeof destino);
    char *arroba = NULL;
    for (char *p = destino; *p; p++) if (*p == '@') arroba = p;
    if (arroba) { *arroba = 0; if (!usuario) usuario = destino; host = arroba + 1; }
    else host = destino;
    static char conta[CONTA_NOME_MAX];
    if (!usuario || !usuario[0]) {   /* como OpenSSH: el nombre de la cuenta local */
        conta_nome(meu_uid(), conta, sizeof conta);
        usuario = conta[0] ? conta : "sagui";
    }
    if (!host[0] || porta <= 0 || porta > 65535) uso();
    ordem[0] = 0;
    for (; i < argc; i++) {
        if (ordem[0]) junta_texto(ordem, " ", sizeof ordem);
        if (tam_texto(ordem) + tam_texto(argv[i]) + 2 >= sizeof ordem) { imprimir_em(ERROS, "ssh: orden demasiado larga\n"); return SAIR_ERRO; }
        junta_texto(ordem, argv[i], sizeof ordem);
    }
    term_local = tipo_descritor(ENTRADA) == TIPO_TERMINAL;
    entrada_term = !sem_entrada && tipo_descritor(fd_ent) == TIPO_TERMINAL;
    saida_term = tipo_descritor(fd_sai) == TIPO_TERMINAL;
    erros_term = tipo_descritor(ERROS) == TIPO_TERMINAL;
    com_pty = quer_pty == 1 || (quer_pty < 0 && !ordem[0] && entrada_term);
    if (com_pty && !entrada_term && quer_pty == 1 && !quieto) aviso("la entrada no es un terminal: se pide terminal remoto igualmente (-t)");

    rx = aloca(RX_TAM);
    tx = aloca(PKT_MAX + 64);
    msg = aloca(PKT_MAX);
    if (!rx || !tx || !msg) fatal("sin memoria");

    sk = conectar_tcp(host, porta, CONECTAR_MS);
    if (sk < 0) fatal("%s puerto %d: %s", host, porta, nome_erro(sk));
    escrever_texto(sk, VERSAO_SSH "\r\n");
    ler_versao();
    trocar_chaves(NULL, 0);
    autenticar();
    abrir_canal();

    if (com_pty) {
        if (entrada_term) {
            modo_terminal(TERM_BRUTO);
            bruto = true;
            tamanho_inicial();
            const char *tt = obter_env("TERM");
            if (saida_term && tt && comeca_com(tt, "xterm")) {   /* D320: el Terminal habla xterm: teclas, LF, cajas */
                escrever_tudo(SAIDA, "\033[?7700h", 8);
                modo_xterm = true;
                utf_saida.caixas = true;
                utf_erros.caixas = true;
            }
        }
        mandar_tamanho(true);
        if (!esperar_resposta_canal()) aviso("el servidor no dio un terminal remoto");
    }
    if (ordem[0]) {
        canal_pedido("exec", true);
        se_cstr(&m, ordem);
        m_enviar();
        if (!esperar_resposta_canal()) fatal("el servidor no aceptó la orden");
    } else {
        canal_pedido("shell", true);
        m_enviar();
        if (!esperar_resposta_canal()) fatal("el servidor no abrió el shell");
    }
    ssh_escape_iniciar(&esc);
    if (n_adiantado) { entrada_dados(adiantado, n_adiantado); n_adiantado = 0; }
    sessao();
    /* fin educado de la conexión */
    m_iniciar(M_DESCONECTAR);
    se_u32(&m, D_PELO_APLIC);
    se_cstr(&m, "hasta luego");
    se_cstr(&m, "es");
    m_enviar();
    if (com_pty && codigo_saida < 0 && !quieto) aviso("conexión con %s cerrada", host);
    if (fd_sai != SAIDA) { sincronizar(fd_sai); fechar(fd_sai); }
    terminar(codigo_saida >= 0 ? codigo_saida : (com_pty ? 0 : SAIR_ERRO));
}
