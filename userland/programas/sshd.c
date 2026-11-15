// SPDX-License-Identifier: GPL-2.0-or-later
/* sshd - servidor SSH de administración (D250, mejora 052).
 *
 * Para entrar desde el PC del mantenedor (OpenSSH) en una máquina con SaguiOS
 * por la red local y depurar: ejecutar órdenes, abrir un sagsh interactivo y
 * traer o llevar archivos.
 *
 *   sshd [-p PUERTO]     escucha (lo normal es como servicio: servicio activar sshd)
 *   sshd --huella        la huella de la clave del servidor (la crea si no existe)
 *   sshd --claves        las claves autorizadas que hay
 *   sshd --ayuda
 *
 * Protocolo SSH-2 (RFC 4253, 4252, 4254) con un solo algoritmo de cada clase,
 * los que un OpenSSH moderno prefiere:
 *   - intercambio de claves curve25519-sha256 (y curve25519-sha256@libssh.org),
 *     con el modo estricto de OpenSSH 9.6 (kex-strict, contra «Terrapin»);
 *   - clave del servidor ssh-ed25519;
 *   - cifrado chacha20-poly1305@openssh.com (la etiqueta se comprueba antes de
 *     descifrar el cuerpo);
 *   - sin compresión.
 * Quien no los tenga recibe una desconexión con el motivo.
 *
 * Autenticación: SOLO por clave pública ssh-ed25519, contra
 * /home/.ssh/authorized_keys y /etc/ssh/authorized_keys. No hay contraseñas.
 * El nombre de usuario da igual (SaguiOS tiene un solo usuario).
 *
 * Canales "session": exec (sagsh -c ORDEN, con la salida, los errores y el
 * código de salida), shell (sagsh interactivo sobre una PTY, D036) y el
 * subsistema sftp (sshd_sftp.h), que es lo que usan sftp y scp.
 *
 * Canales "direct-tcpip" (D253): la redirección local de puertos de
 * «ssh -L 5900:localhost:5900 root@IP», para entrar cifrado a un servicio que
 * solo escucha en 127.0.0.1 (el VNC). De serie solo hacia esta máquina
 * (redirigir=local); «ssh -R» no se admite.
 *
 * Seguro por defecto: el servicio viene desactivado, solo atiende a la red
 * local, limita conexiones e intentos y lo anota todo en /logs/sshd.log (o en
 * /tmp/.servicios/sshd.log si no hay /logs) y en el registro del sistema.
 *
 * Procesos: el que escucha lanza un «sshd --sesion» por conexión, con el
 * socket como entrada (los hijos solo heredan los descriptores 0, 1 y 2): un
 * fallo en una sesión no tumba el servicio ni las otras sesiones.
 *
 * Configuración (clave=valor) en /etc/ssh/sshd.conf y /home/.ssh/sshd.conf
 * (gana la segunda): ver docs/ssh.md. */
#include "sagui.h"
#include "contas.h"
#include "cripto.h"
#include "servicos.h"
#include "sshd_logica.h"
#include "ssh_transporte.h"   /* D317: paquetes, H y claves, comunes con el cliente ssh */

#define VERSAO_SSH     "SSH-2.0-SaguiOS_sshd_1.0"
#define PKT_MAX        35000u            /* tamaño máximo de un paquete (RFC 4253, 6.1) */
#define RX_TAM         (2 * PKT_MAX + 64)
#define MSG_MAX        (PKT_MAX - 64)
#define DADOS_MAX      32768u            /* datos por paquete de canal */
#define JANELA_LOCAL   (256u * 1024u)    /* lo que el cliente puede mandar sin que se le confirme */
#define MAX_CANAIS     12                /* MAX_SESSOES + MAX_TUNEIS */
#define MAX_SESSOES    4                 /* canales «session» por conexión */
#define MAX_TUNEIS     8                 /* canales «direct-tcpip» (ssh -L) por conexión */
#define JANELA_TUNEL   (64u * 1024u)     /* ventana (y memoria) de cada túnel en el sentido cliente -> destino */
#define TX_KERNEL      16384u            /* TX_TAM del kernel (tcp.c): lo que cabe en la cola de envío de un socket */
#define CONECTAR_MS    3000u             /* plazo para conectar con el destino de un túnel */
#define SEM_CANAL_MS   60000u            /* sesión sin ningún canal: se corta... */
#define SEM_CANAL_TUNEL_MS (30u * 60000u)   /* ...o media hora si se pueden abrir túneles (ssh -N -L espera a que alguien conecte) */
#define MAX_CHAVES     32
#define FILA_MAX       (192u * 1024u)    /* respuestas del SFTP esperando ventana: por encima, no se atienden más pedidos */
#define PIPE_KERNEL    512u              /* PIPE_TAM del kernel (pipe.h) */
#define REGISTRO_MAX   (256u * 1024u)
#define PACOTES_MAX    0x80000000u       /* paquetes con las mismas claves antes de exigir otro intercambio */
#define VIVO_MS        60000u            /* sin recibir nada tanto tiempo: se pregunta al cliente si sigue ahí */
#define VIVO_MAX       3                 /* preguntas seguidas sin respuesta antes de cortar */

#define DIR_CASA       "/home/.ssh"
#define DIR_ETC        "/etc/ssh"
#define NOME_CHAVE     "ssh_host_ed25519_key"
#define MARCA_CHAVE    "SAGUIOS-ED25519-SEMILLA"

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
#define M_AUT_PK_OK     60
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

/* Motivos de desconexión (RFC 4253, 11.1) */
#define D_PROTOCOLO     2
#define D_KEX           3
#define D_MAC           5
#define D_SERVICO       7
#define D_PELO_APLIC    11
#define D_CONEXOES      12
#define D_SEM_METODOS   14

#define ALG_KEX    "curve25519-sha256,curve25519-sha256@libssh.org"
#define ALG_CHAVE  SSH_ED25519
#define ALG_CIFRA  "chacha20-poly1305@openssh.com"
#define ALG_MAC    "hmac-sha2-256"   /* solo para que la lista no vaya vacía: con un cifrado AEAD no se usa */
#define ALG_COMP   "none"

/* Códigos de salida de una sesión (los lee el proceso que escucha). */
#define SAIR_BEM        0
#define SAIR_SEM_AUT    10   /* se fue sin autenticarse o falló: cuenta para el bloqueo de su dirección */
#define SAIR_ERRO       11

/* ------------------------------------------------------------ configuración */

static struct {
    int  porta;
    bool so_local;        /* solo_red_local */
    int  max_conexoes;
    int  max_tentativas;
    int  graca_s;         /* segundos para autenticarse */
    int  bloqueio_s;      /* cuánto se ignora a una dirección que falla demasiado */
    int  falhas_bloqueio; /* conexiones fallidas seguidas para bloquearla */
    bool exec_utf8;       /* convertir a UTF-8 la salida de las órdenes sin terminal */
    int  redirigir;       /* SSH_RED_*: a dónde se dejan abrir túneles (ssh -L) */
} conf = { 22, true, 4, 6, 60, 120, 5, false, SSH_RED_LOCAL };

static bool valor_sim(const char *v)
{
    return compara_texto(v, "si") == 0 || compara_texto(v, "1") == 0 || compara_texto(v, "yes") == 0;
}

static void conf_ler(const char *arq)
{
    int fd = abrir(arq, ABRIR_LER);
    if (fd < 0) return;
    static leitor_linhas_t l;
    char linha[200];
    leitor_iniciar(&l, fd, false);
    while (leitor_linha(&l, linha, sizeof linha, NULL) >= 0) {
        char *p = linha;
        while (eh_espaco(*p)) p++;
        if (!*p || *p == '#') continue;
        char *v = acha_char(p, '=');
        if (!v) continue;
        *v++ = 0;
        char *f = p + tam_texto(p);
        while (f > p && eh_espaco(f[-1])) *--f = 0;
        while (eh_espaco(*v)) v++;
        f = v + tam_texto(v);
        while (f > v && eh_espaco(f[-1])) *--f = 0;
        int n = texto_para_int(v);
        if (compara_texto(p, "puerto") == 0 && n > 0 && n < 65536) conf.porta = n;
        else if (compara_texto(p, "solo_red_local") == 0) conf.so_local = valor_sim(v);
        else if (compara_texto(p, "max_conexiones") == 0 && n > 0 && n <= 8) conf.max_conexoes = n;
        else if (compara_texto(p, "max_intentos") == 0 && n > 0 && n <= 20) conf.max_tentativas = n;
        else if (compara_texto(p, "tiempo_entrada") == 0 && n >= 5 && n <= 600) conf.graca_s = n;
        else if (compara_texto(p, "bloqueo_segundos") == 0 && n >= 0 && n <= 86400) conf.bloqueio_s = n;
        else if (compara_texto(p, "bloqueo_fallos") == 0 && n > 0 && n <= 100) conf.falhas_bloqueio = n;
        else if (compara_texto(p, "exec_utf8") == 0) conf.exec_utf8 = valor_sim(v);
        else if (compara_texto(p, "redirigir") == 0) {
            /* un valor que no se entiende cierra la puerta en vez de dejarla como estaba */
            int modo = ssh_modo_redirigir(v);
            conf.redirigir = modo < 0 ? SSH_RED_NO : modo;
        }
    }
    fechar(fd);
}

static void conf_carregar(void)
{
    conf_ler(DIR_ETC "/sshd.conf");
    conf_ler(DIR_CASA "/sshd.conf");
}

/* ------------------------------------------------------------ registro */

static char quem[40];   /* "10.0.2.2:51234" en una sesión; vacío en el que escucha */

/* Una línea en /logs/sshd.log (o /tmp/.servicios/sshd.log) y en el registro
 * del sistema. Nunca se le pasan claves privadas ni secretos. */
static void anotar(const char *fmt, ...)
{
    char texto[400], linha[480], hora[32], arq[96];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(texto, sizeof texto, fmt, ap);
    va_end(ap);
    for (char *p = texto; *p; p++)
        if ((unsigned char)*p < 0x20) *p = '?';   /* nada de saltos de línea metidos por el cliente */
    fuso_data_t d;
    agora_local(&d);
    formatar_data(hora, sizeof hora, "%Y-%m-%d %H:%M:%S", &d);
    formatar_texto(linha, sizeof linha, "%s [%d]%s%s %s\n", hora, meu_pid(), quem[0] ? " " : "", quem, texto);
    const char *pasta = servicos_pasta_registros();
    servicos_criar_pastas(pasta);
    formatar_texto(arq, sizeof arq, "%s/sshd.log", pasta);
    info_arquivo_t i;
    if (info_arquivo(arq, &i) == 0 && i.tamanho > REGISTRO_MAX) {
        char velho[112];
        formatar_texto(velho, sizeof velho, "%s.1", arq);
        apagar(velho);
        renomear(arq, velho);
    }
    int fd = abrir(arq, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_ACRESCENTAR);
    if (fd >= 0) {
        escrever_tudo(fd, linha, (unsigned)tam_texto(linha));
        fechar(fd);
    }
    registrar("sshd:%s%s %s", quem[0] ? " " : "", quem, texto);
}

/* ------------------------------------------------------------ claves */

static uint8_t srv_semente[32], srv_publica[32];
static char    srv_arquivo[96];

/* "SHA256:..." como la muestran ssh y ssh-keygen -l. */
static void impressao(const uint8_t pub[32], char *dst, unsigned max)
{
    uint8_t blob[SSH_BLOB_ED25519], h[32];
    char b[48];
    ssh_blob_montar(blob, pub);
    sha256(blob, sizeof blob, h);
    b64_codificar(h, 32, b, sizeof b);
    char *ig = acha_char(b, '=');
    if (ig) *ig = 0;
    formatar_texto(dst, max, "SHA256:%s", b);
}

static void linha_publica(const uint8_t pub[32], char *dst, unsigned max)
{
    uint8_t blob[SSH_BLOB_ED25519];
    char b[80];
    ssh_blob_montar(blob, pub);
    b64_codificar(blob, sizeof blob, b, sizeof b);
    formatar_texto(dst, max, SSH_ED25519 " %s root@saguios", b);
}

static bool chave_ler(const char *arq)
{
    char t[160];
    int fd = abrir(arq, ABRIR_LER);
    if (fd < 0) return false;
    int n = ler(fd, t, sizeof t - 1);
    fechar(fd);
    if (n <= 0) return false;
    t[n] = 0;
    unsigned m = sizeof MARCA_CHAVE - 1;
    if (!ssh_comeca(t, MARCA_CHAVE " ")) return false;
    char *b = t + m + 1, *f = b;
    while (*f && !eh_espaco(*f)) f++;
    uint8_t sem[33];
    bool ok = b64_decodificar(b, (int)(f - b), sem, sizeof sem, false) == 32;
    if (ok) {
        memcpy(srv_semente, sem, 32);
        copia_texto(srv_arquivo, arq, sizeof srv_arquivo);
    }
    cripto_apagar(sem, sizeof sem);
    cripto_apagar(t, sizeof t);
    return ok;
}

static bool chave_gravar(const char *pasta)
{
    char arq[96], t[160], b[48], pub[160];
    servicos_criar_pastas(pasta);
    formatar_texto(arq, sizeof arq, "%s/" NOME_CHAVE, pasta);
    int fd = abrir(arq, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return false;
    b64_codificar(srv_semente, 32, b, sizeof b);
    formatar_texto(t, sizeof t, MARCA_CHAVE " %s\n", b);
    bool ok = escrever_tudo(fd, t, (unsigned)tam_texto(t)) == (int)tam_texto(t);
    sincronizar(fd);
    fechar(fd);
    cripto_apagar(t, sizeof t);
    cripto_apagar(b, sizeof b);
    if (!ok) { apagar(arq); return false; }
    copia_texto(srv_arquivo, arq, sizeof srv_arquivo);
    /* la pública, en el formato de OpenSSH, para quien quiera copiarla a su known_hosts */
    formatar_texto(arq, sizeof arq, "%s/" NOME_CHAVE ".pub", pasta);
    fd = abrir(arq, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd >= 0) {
        linha_publica(srv_publica, pub, sizeof pub);
        imprimir_em(fd, "%s\n", pub);
        fechar(fd);
    }
    return true;
}

/* D255: con la partición SAGUI-CONF, la clave va en /etc/ssh (que se guarda
 * allí con conf_gravar y vuelve a /etc al arrancar), solo para root. */
static bool chave_gravar_conf(void)
{
    char t[160], b[48], pub[160];
    b64_codificar(srv_semente, 32, b, sizeof b);
    formatar_texto(t, sizeof t, MARCA_CHAVE " %s\n", b);
    int r = conf_gravar("ssh/" NOME_CHAVE, t, (int)tam_texto(t));
    cripto_apagar(t, sizeof t);
    cripto_apagar(b, sizeof b);
    if (r < 0) return false;
    linha_publica(srv_publica, pub, sizeof pub);
    junta_texto(pub, "\n", sizeof pub);
    conf_gravar("ssh/" NOME_CHAVE ".pub", pub, (int)tam_texto(pub));
    copia_texto(srv_arquivo, DIR_ETC "/" NOME_CHAVE, sizeof srv_arquivo);
    return true;
}

/* Carga la clave del servidor; si no hay (primer arranque) y "criar", la
 * genera. D255: primero /etc/ssh (de SAGUI-CONF); la de /home/.ssh (donde iba
 * antes) se pasa a SAGUI-CONF la primera vez, para que la huella no cambie.
 * Sin SAGUI-CONF, como antes: en /home/.ssh o, si no se puede, en /etc/ssh. */
static bool chave_servidor(bool criar)
{
    if (chave_ler(DIR_ETC "/" NOME_CHAVE)) {
        ed25519_publica(srv_publica, srv_semente);
        return true;
    }
    if (chave_ler(DIR_CASA "/" NOME_CHAVE)) {
        ed25519_publica(srv_publica, srv_semente);
        if (criar && conf_persistente() && chave_gravar_conf())
            anotar("clave del servidor pasada de " DIR_CASA " a SAGUI-CONF (" DIR_ETC "); la huella no cambia");
        return true;
    }
    if (!criar) return false;
    cripto_aleatorio(srv_semente, 32);
    ed25519_publica(srv_publica, srv_semente);
    if (!(conf_persistente() && chave_gravar_conf()) && !chave_gravar(DIR_CASA) && !chave_gravar(DIR_ETC)) {
        anotar("no se puede guardar la clave del servidor (ni en " DIR_CASA " ni en " DIR_ETC "): vale solo para esta ejecución");
        srv_arquivo[0] = 0;
    }
    char h[64];
    info_aleatorio_t ia;
    memset(&ia, 0, sizeof ia);
    info_aleatorio(&ia);
    impressao(srv_publica, h, sizeof h);
    anotar("clave del servidor nueva (ed25519) en %s: %s%s", srv_arquivo[0] ? srv_arquivo : "(memoria)", h,
           (ia.fontes & (ALEA_RDRAND | ALEA_RDSEED)) ? "" : " (CPU sin RDRAND: aleatoriedad de las interrupciones, D061)");
    return true;
}

/* ¿La clave del servidor sobrevive a un reinicio? */
static bool chave_persistente(void)
{
    if (comeca_com(srv_arquivo, "/home/")) return servicos_montado("/home");
    if (comeca_com(srv_arquivo, DIR_ETC "/")) return conf_persistente();   /* D255: SAGUI-CONF */
    return false;   /* /etc está en el ramdisk */
}

/* D255: cada clave entra con una cuenta: las de /home/.ssh/authorized_keys
 * como sagui y las de CONTAS_CLAVES_ROOT (/etc/ssh/authorized_keys) como root. */
typedef struct { uint8_t pub[32]; char comentario[40]; unsigned uid; } autorizada_t;
static autorizada_t autorizadas[MAX_CHAVES];
static int n_autorizadas, n_recusadas;

static void autorizadas_ler(const char *arq, unsigned uid)
{
    int fd = abrir(arq, ABRIR_LER);
    if (fd < 0) return;
    static leitor_linhas_t l;
    static char linha[1024];
    leitor_iniciar(&l, fd, false);
    while (leitor_linha(&l, linha, sizeof linha, NULL) >= 0) {
        if (n_autorizadas >= MAX_CHAVES) { n_recusadas++; continue; }
        autorizada_t *a = &autorizadas[n_autorizadas];
        int r = ssh_linha_autorizada(linha, a->pub, a->comentario, sizeof a->comentario);
        if (r == 1) { a->uid = uid; n_autorizadas++; }
        else if (r < 0) n_recusadas++;
        else {
            const char *p = linha;
            while (eh_espaco(*p)) p++;
            if (*p && *p != '#') n_recusadas++;   /* otro tipo de clave (RSA, ECDSA...) */
        }
    }
    fechar(fd);
}

static void autorizadas_carregar(void)
{
    n_autorizadas = n_recusadas = 0;
    autorizadas_ler(DIR_CASA "/authorized_keys", UID_SAGUI);
    autorizadas_ler(CONTAS_CLAVES_ROOT, UID_ROOT);
}

/* La entrada de esa clave: si el cliente pide root, la de root si la hay; si
 * no, la de sagui. Una clave solo de sagui con «root@» entra como sagui (y se
 * anota): no da más de lo que da su archivo. */
static const autorizada_t *autorizada(const uint8_t pub[32], bool quer_root)
{
    const autorizada_t *outra = NULL;
    for (int i = 0; i < n_autorizadas; i++)
        if (memcmp(autorizadas[i].pub, pub, 32) == 0) {
            if ((autorizadas[i].uid == UID_ROOT) == quer_root) return &autorizadas[i];
            if (!outra) outra = &autorizadas[i];
        }
    return outra;
}

static unsigned uid_sessao = UID_SAGUI;   /* la cuenta con la que se entró */

/* Tras autenticar, la sesión deja de ser root si la cuenta no lo es: el shell,
 * las órdenes, el SFTP y los túneles corren ya con esa cuenta. La clave del
 * servidor ya está en memoria (la renovación de claves la sigue teniendo). */
static void tomar_conta(const autorizada_t *a, const char *pedido)
{
    uid_sessao = a->uid;
    char n[CONTA_NOME_MAX];
    conta_nome(a->uid, n, sizeof n);
    if (compara_texto(pedido, n) != 0)
        anotar("el cliente pidió «%s» y entra como %s (la clave está en %s)", pedido, n,
               a->uid == UID_ROOT ? CONTAS_CLAVES_ROOT : DIR_CASA "/authorized_keys");
    if (meu_uid() != a->uid) {
        int r = usuario_op(USU_FIJAR, (int)a->uid, (int)(a->uid == UID_ROOT ? GID_ROOT : GID_SAGUI));
        if (r < 0) anotar("no se pudo pasar a la cuenta %s (%s): la sesión sigue con uid %u", n, nome_erro(r), meu_uid());
    }
}

/* ------------------------------------------------------------ transporte (una sesión) */

static int      sk = -1;                 /* el socket */
static uint8_t *rx, *tx, *msg;
static unsigned rx_ini, rx_fim;
static bool     cif_rx, cif_tx;          /* ya se cifra en ese sentido */
static uint8_t  ch_rx[64], ch_tx[64];
static uint32_t seq_rx, seq_tx;
static uint32_t pacotes_chave;           /* paquetes desde el último intercambio de claves */
static bool     rx_tam_ok;
static uint32_t rx_tam;
static uint8_t  sessao_id[32];
static bool     tem_sessao, estrito, autenticado;
static char     v_cliente[256];
static ssh_escritor_t m;                 /* el mensaje que se está montando (sobre msg) */
static uint8_t  enchimento[256];
static unsigned enchimento_pos = sizeof enchimento;

static void sessao_limpar(void);

static void terminar(int codigo) __attribute__((noreturn));
static void terminar(int codigo)
{
    sessao_limpar();
    if (sk >= 0) fechar(sk);
    cripto_apagar(ch_rx, sizeof ch_rx);
    cripto_apagar(ch_tx, sizeof ch_tx);
    cripto_apagar(srv_semente, sizeof srv_semente);
    sair(autenticado ? SAIR_BEM : codigo);
}

static void fatal(const char *motivo) __attribute__((noreturn));
static void fatal(const char *motivo)
{
    anotar("conexión terminada: %s", motivo);
    terminar(SAIR_ERRO);
}

static void enviar(const uint8_t *pl, unsigned n)
{
    if (n > MSG_MAX) fatal("mensaje propio demasiado grande");
    unsigned pad = ssh_pacote_relleno(n, cif_tx);
    if (enchimento_pos + pad > sizeof enchimento) { cripto_aleatorio(enchimento, sizeof enchimento); enchimento_pos = 0; }
    unsigned total = ssh_pacote_montar(tx, PKT_MAX + 64, pl, n, enchimento + enchimento_pos, cif_tx ? ch_tx : NULL, seq_tx);
    enchimento_pos += pad;
    if (!total) fatal("mensaje propio demasiado grande");
    seq_tx++;
    pacotes_chave++;
    if (escrever_tudo(sk, tx, total) != (int)total) {
        anotar("conexión terminada: el cliente cerró");
        terminar(SAIR_ERRO);
    }
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
    if (!ja) {
        ja = true;
        m_iniciar(M_DESCONECTAR);
        se_u32(&m, codigo);
        se_cstr(&m, texto);
        se_cstr(&m, "es");
        m_enviar();
    }
    fatal(texto);
}

/* Lee del socket lo que haya. ms = 0: sin esperar. -> 1 leyó, 0 nada, -1 se cerró o venció el plazo */
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

/* Saca un paquete completo del búfer: 1 = hay (su contenido en *pl, válido
 * hasta el siguiente rx_encher), 0 = falta. La etiqueta se comprueba antes de
 * mirar nada más que el tamaño. */
static int pacote_tentar(const uint8_t **pl, unsigned *pn)
{
    unsigned total;
    int r = ssh_pacote_extrair(rx + rx_ini, rx_fim - rx_ini, cif_rx ? ch_rx : NULL, seq_rx, PKT_MAX,
                               &rx_tam_ok, &rx_tam, pl, pn, &total);
    if (r == SSH_PKT_FALTA) return 0;
    if (r == SSH_PKT_TAMANHO) {
        /* con cifrado, un tamaño absurdo es un paquete manipulado: se corta sin más */
        if (cif_rx) fatal("tamaño de paquete no válido");
        desconectar(D_PROTOCOLO, "tamaño de paquete no válido");
    }
    if (r == SSH_PKT_MAC) fatal("la etiqueta (MAC) de un paquete no coincide");
    if (r == SSH_PKT_RELLENO) desconectar(D_PROTOCOLO, "relleno de paquete no válido");
    rx_ini += total;
    seq_rx++;
    pacotes_chave++;
    if (pacotes_chave >= PACOTES_MAX) desconectar(D_KEX, "demasiados paquetes con las mismas claves: hace falta otro intercambio");
    return 1;
}

static void pacote_esperar(const uint8_t **pl, unsigned *pn, unsigned ms)
{
    for (;;) {
        if (pacote_tentar(pl, pn)) return;
        if (rx_encher(ms) < 0) fatal(autenticado ? "el cliente cerró o dejó de responder" : "el cliente cerró o no respondió a tiempo");
    }
}

/* Claves de 64 bytes (RFC 4253, 7.2), con el id de esta sesión. */
static void derivar(uint8_t saida[64], const uint8_t *k_mp, unsigned nk, const uint8_t hh[32], char letra)
{
    ssh_derivar(saida, k_mp, nk, hh, letra, sessao_id);
}

/* Durante un intercambio solo pueden llegar sus mensajes; fuera del modo
 * estricto se toleran los de relleno y depuración. */
static void esperar_kex(unsigned tipo, const uint8_t **pl, unsigned *pn, bool tolerar)
{
    for (;;) {
        pacote_esperar(pl, pn, 30000);
        unsigned t = (*pl)[0];
        if (t == tipo) return;
        if (tolerar && (t == M_IGNORAR || t == M_DEPURAR || t == M_NAO_IMPLEM)) continue;
        if (t == M_DESCONECTAR) fatal("el cliente se desconectó durante el intercambio de claves");
        desconectar(D_PROTOCOLO, "mensaje inesperado durante el intercambio de claves");
    }
}

/* El intercambio de claves entero, a partir del KEXINIT del cliente (copiado
 * en ic). Sirve para el primero y para los que pida el cliente después. */
static void trocar_chaves(const uint8_t *ic, unsigned nic)
{
    static uint8_t is[400];
    bool inicial = !tem_sessao;
    ssh_leitor_t l;
    const uint8_t *lst[10];
    unsigned nl[10];
    if (nic < 17) desconectar(D_PROTOCOLO, "KEXINIT mal formado");
    sl_iniciar(&l, ic + 17, nic - 17);
    for (int i = 0; i < 10; i++) lst[i] = sl_texto(&l, &nl[i]);
    bool segue = sl_u8(&l) != 0;
    sl_u32(&l);
    if (l.erro) desconectar(D_PROTOCOLO, "KEXINIT mal formado");

    /* el nuestro */
    ssh_escritor_t e;
    se_iniciar(&e, is, sizeof is);
    se_u8(&e, M_KEXINIT);
    uint8_t galleta[16];
    cripto_aleatorio(galleta, 16);
    se_bytes(&e, galleta, 16);
    se_cstr(&e, inicial ? ALG_KEX ",kex-strict-s-v00@openssh.com" : ALG_KEX);
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
    enviar(is, e.n);

    if (inicial) {
        estrito = ssh_lista_tem(lst[0], nl[0], "kex-strict-c-v00@openssh.com");
        /* en modo estricto, el KEXINIT tiene que ser el primer paquete del cliente */
        if (estrito && seq_rx != 1) desconectar(D_PROTOCOLO, "modo estricto: paquetes antes del KEXINIT");
    }
    bool ext_info = inicial && ssh_lista_tem(lst[0], nl[0], "ext-info-c");
    char kex[48], alg[48];
    const char *falta = NULL;
    if (!ssh_escolher(lst[0], nl[0], ALG_KEX, kex, sizeof kex)) falta = "intercambio de claves (hace falta curve25519-sha256)";
    else if (!ssh_escolher(lst[1], nl[1], ALG_CHAVE, alg, sizeof alg)) falta = "clave del servidor (hace falta ssh-ed25519)";
    else if (!ssh_escolher(lst[2], nl[2], ALG_CIFRA, alg, sizeof alg) || !ssh_escolher(lst[3], nl[3], ALG_CIFRA, alg, sizeof alg))
        falta = "cifrado (hace falta chacha20-poly1305@openssh.com)";
    else if (!ssh_escolher(lst[6], nl[6], ALG_COMP, alg, sizeof alg) || !ssh_escolher(lst[7], nl[7], ALG_COMP, alg, sizeof alg))
        falta = "compresión (hace falta none)";
    if (falta) {
        char t[160];
        formatar_texto(t, sizeof t, "sin algoritmo común de %s", falta);
        desconectar(D_KEX, t);
    }
    bool tolerar = !(estrito && inicial);
    const uint8_t *pl;
    unsigned pn;
    if (segue && !(ssh_primeiro_eh(lst[0], nl[0], kex) && ssh_primeiro_eh(lst[1], nl[1], ALG_CHAVE)))
        pacote_esperar(&pl, &pn, 30000);   /* el cliente adivinó mal: su siguiente paquete no vale (RFC 4253, 7) */

    esperar_kex(M_ECDH_INIT, &pl, &pn, tolerar);
    unsigned nq;
    sl_iniciar(&l, pl + 1, pn - 1);
    const uint8_t *qc = sl_texto(&l, &nq);
    if (l.erro || nq != 32) desconectar(D_KEX, "clave efímera del cliente no válida");

    uint8_t priv[32], qs[32], k[32], k_mp[40], hh[32], assin[64], blob[SSH_BLOB_ED25519];
    cripto_aleatorio(priv, 32);
    x25519_base(qs, priv);
    x25519(k, priv, qc);
    cripto_apagar(priv, sizeof priv);
    unsigned zero = 0;
    for (int i = 0; i < 32; i++) zero |= k[i];
    if (!zero) desconectar(D_KEX, "clave efímera del cliente no válida");
    ssh_escritor_t ek;
    se_iniciar(&ek, k_mp, sizeof k_mp);
    se_mpint(&ek, k, 32);
    ssh_blob_montar(blob, srv_publica);

    /* H = SHA-256(V_C, V_S, I_C, I_S, K_S, Q_C, Q_S, K)   (RFC 5656, 4 / RFC 8731) */
    ssh_hash_troca(hh, v_cliente, VERSAO_SSH, ic, nic, is, e.n, blob, sizeof blob, qc, qs, k_mp, ek.n);
    if (!tem_sessao) { memcpy(sessao_id, hh, 32); tem_sessao = true; }
    ed25519_assinar(assin, hh, 32, srv_semente, srv_publica);

    m_iniciar(M_ECDH_RESP);
    se_texto(&m, blob, sizeof blob);
    se_texto(&m, qs, 32);
    se_u32(&m, 4 + (sizeof SSH_ED25519 - 1) + 4 + 64);
    se_cstr(&m, SSH_ED25519);
    se_texto(&m, assin, 64);
    m_enviar();
    m_iniciar(M_NEWKEYS);
    m_enviar();

    uint8_t nova_rx[64];
    derivar(nova_rx, k_mp, ek.n, hh, 'C');   /* cliente -> servidor */
    derivar(ch_tx, k_mp, ek.n, hh, 'D');     /* servidor -> cliente */
    cif_tx = true;
    if (estrito) seq_tx = 0;
    cripto_apagar(k, sizeof k);
    cripto_apagar(k_mp, sizeof k_mp);

    esperar_kex(M_NEWKEYS, &pl, &pn, tolerar);
    memcpy(ch_rx, nova_rx, 64);
    cripto_apagar(nova_rx, sizeof nova_rx);
    cif_rx = true;
    rx_tam_ok = false;
    if (estrito) seq_rx = 0;
    pacotes_chave = 0;

    if (ext_info) {   /* RFC 8308: qué firmas de usuario se aceptan */
        m_iniciar(M_EXT_INFO);
        se_u32(&m, 1);
        se_cstr(&m, "server-sig-algs");
        se_cstr(&m, SSH_ED25519);
        m_enviar();
    }
    if (inicial) anotar("intercambio de claves: %s, " ALG_CHAVE ", " ALG_CIFRA "%s", kex, estrito ? ", modo estricto" : "");
    else anotar("claves renovadas a petición del cliente");
}

/* La línea de identificación del cliente (RFC 4253, 4.2). */
static void ler_versao(void)
{
    for (;;) {
        for (unsigned i = rx_ini; i < rx_fim; i++) {
            if (rx[i] != '\n') continue;
            unsigned n = i - rx_ini;
            if (n && rx[i - 1] == '\r') n--;
            if (n >= sizeof v_cliente) fatal("identificación del cliente demasiado larga");
            memcpy(v_cliente, rx + rx_ini, n);
            v_cliente[n] = 0;
            rx_ini = i + 1;
            if (!comeca_com(v_cliente, "SSH-2.0-") && !comeca_com(v_cliente, "SSH-1.99-")) {
                for (char *p = v_cliente; *p; p++)
                    if ((unsigned char)*p < 0x20 || (unsigned char)*p > 0x7e) *p = '?';
                v_cliente[40] = 0;
                anotar("no es un cliente SSH-2 («%s»)", v_cliente);
                escrever_texto(sk, "Protocol mismatch.\r\n");
                terminar(SAIR_ERRO);
            }
            return;
        }
        if (rx_fim - rx_ini > 255) fatal("identificación del cliente demasiado larga");
        if (rx_encher(15000) < 0) fatal("el cliente no se identificó");
    }
}

/* ------------------------------------------------------------ autenticación (RFC 4252) */

static void autenticar(void)
{
    const uint8_t *pl;
    unsigned pn;
    ssh_leitor_t l;
    unsigned limite = tempo_ms() + (unsigned)conf.graca_s * 1000;
    int tentativas = 0;
    bool servico = false;
    for (;;) {
        int resta = (int)(limite - tempo_ms());
        if (resta <= 0) desconectar(D_PELO_APLIC, "tiempo de entrada agotado");
        if (!pacote_tentar(&pl, &pn)) {
            if (rx_encher((unsigned)resta) < 0) fatal(tempo_ms() >= limite ? "tiempo de entrada agotado" : "el cliente cerró sin autenticarse");
            continue;
        }
        unsigned tipo = pl[0];
        sl_iniciar(&l, pl + 1, pn - 1);
        if (tipo == M_IGNORAR || tipo == M_DEPURAR || tipo == M_NAO_IMPLEM) continue;
        if (tipo == M_DESCONECTAR) fatal("el cliente se desconectó sin autenticarse");
        if (tipo == M_PEDIR_SERVICO && !servico) {
            unsigned n;
            const uint8_t *s = sl_texto(&l, &n);
            if (l.erro || !sl_igual(s, n, "ssh-userauth")) desconectar(D_SERVICO, "servicio no disponible");
            m_iniciar(M_SERVICO_OK);
            se_cstr(&m, "ssh-userauth");
            m_enviar();
            servico = true;
            continue;
        }
        if (tipo != M_AUT_PEDIDO || !servico) desconectar(D_PROTOCOLO, "mensaje inesperado antes de autenticarse");

        char usuario[65], metodo[32];
        unsigned ns, nu;
        const uint8_t *u = l.p + l.pos;          /* el "string" del usuario tal cual, para la firma */
        sl_cstr(&l, usuario, sizeof usuario);
        nu = (unsigned)(l.p + l.pos - u);
        const uint8_t *serv = sl_texto(&l, &ns);
        sl_cstr(&l, metodo, sizeof metodo);
        if (l.erro) desconectar(D_PROTOCOLO, "petición de autenticación mal formada");
        if (!sl_igual(serv, ns, "ssh-connection")) desconectar(D_SERVICO, "servicio no disponible");
        for (char *p = usuario; *p; p++)
            if ((unsigned char)*p < 0x21 || (unsigned char)*p > 0x7e) *p = '?';
        bool conta = true;
        if (compara_texto(metodo, "none") == 0) {
            conta = false;   /* es la pregunta «¿qué métodos hay?» */
        } else if (compara_texto(metodo, "publickey") == 0) {
            unsigned na, nb;
            bool com_assin = sl_u8(&l) != 0;
            const uint8_t *alg = sl_texto(&l, &na);
            const uint8_t *blob = sl_texto(&l, &nb);
            uint8_t pub[32];
            char h[64];
            if (l.erro) desconectar(D_PROTOCOLO, "petición de autenticación mal formada");
            if (!sl_igual(alg, na, SSH_ED25519) || !ssh_blob_ler(blob, nb, pub)) {
                anotar("usuario %s: clave de un tipo no admitido (solo ssh-ed25519)", usuario);
            } else {
                const autorizada_t *a = autorizada(pub, compara_texto(usuario, "root") == 0);
                impressao(pub, h, sizeof h);
                if (!a) {
                    anotar("usuario %s: clave NO autorizada %s", usuario, h);
                } else if (!com_assin) {
                    m_iniciar(M_AUT_PK_OK);   /* «esa clave vale: fírmame la petición» */
                    se_texto(&m, alg, na);
                    se_texto(&m, blob, nb);
                    m_enviar();
                    continue;
                } else {
                    unsigned nf, nt, nas;
                    const uint8_t *f = sl_texto(&l, &nf);
                    ssh_leitor_t lf;
                    sl_iniciar(&lf, f, nf);
                    const uint8_t *tf = sl_texto(&lf, &nt);
                    const uint8_t *as = sl_texto(&lf, &nas);
                    /* lo firmado: id de sesión, y la petición hasta la clave (RFC 4252, 7) */
                    static uint8_t dados[512];
                    ssh_escritor_t e;
                    se_iniciar(&e, dados, sizeof dados);
                    se_texto(&e, sessao_id, 32);
                    se_u8(&e, M_AUT_PEDIDO);
                    se_bytes(&e, u, nu);
                    se_cstr(&e, "ssh-connection");
                    se_cstr(&e, "publickey");
                    se_u8(&e, 1);
                    se_cstr(&e, SSH_ED25519);
                    se_texto(&e, blob, nb);
                    if (!l.erro && !lf.erro && !e.erro && lf.pos == nf && sl_igual(tf, nt, SSH_ED25519) && nas == 64 &&
                        ed25519_verificar(as, dados, e.n, pub)) {
                        m_iniciar(M_AUT_OK);
                        m_enviar();
                        autenticado = true;
                        anotar("usuario %s: autenticado con la clave %s%s%s%s", usuario, h, a->comentario[0] ? " (" : "",
                               a->comentario, a->comentario[0] ? ")" : "");
                        tomar_conta(a, usuario);   /* D255 */
                        return;
                    }
                    anotar("usuario %s: firma NO válida con la clave %s", usuario, h);
                }
            }
        } else if (compara_texto(metodo, "password") == 0 || compara_texto(metodo, "keyboard-interactive") == 0) {
            anotar("usuario %s: intento con contraseña rechazado (no se admiten)", usuario);
        } else {
            for (char *p = metodo; *p; p++)
                if ((unsigned char)*p < 0x21 || (unsigned char)*p > 0x7e) *p = '?';
            anotar("usuario %s: método «%s» rechazado", usuario, metodo);
        }
        if (conta && ++tentativas >= conf.max_tentativas) desconectar(D_PROTOCOLO, "demasiados intentos de autenticación");
        m_iniciar(M_AUT_FALHA);
        se_cstr(&m, "publickey");
        se_u8(&m, 0);
        m_enviar();
    }
}

/* ------------------------------------------------------------ canales (RFC 4254) */

enum { C_NOVO, C_PROC, C_SFTP, C_TUNEL };

typedef struct canal {
    bool     usado;
    int      tipo;
    uint32_t remoto, jan_remota, max_remoto;
    uint32_t jan_local, consumido;
    uint32_t jan_max;          /* la ventana que se le ofrece al cliente (y lo que cabe en "ent") */
    /* terminal pedido (pty-req) */
    bool     pty;
    int      cols, lins;
    char     term[32];
    ssh_tecla_t tecla;
    /* proceso (exec o shell) */
    int      pid;
    bool     terminou, out_fim, err_fim, utf8;
    int      codigo;
    unsigned t_fim;
    int      fd_in, fd_in_l, fd_out, fd_err, fd_tv;
    /* lo que mandó el cliente y aún no se ha entregado (al proceso o al SFTP) */
    uint8_t *ent;
    unsigned ent_ini, ent_n;
    bool     sem_janela;       /* hay salida esperando a que el cliente dé ventana */
    bool     eof_cliente, eof_entregue, eof_enviado, fechar_enviado;
    /* salida en cola (SFTP) */
    uint8_t *fila;
    unsigned fila_ini, fila_n, fila_cap;
    void    *sftp_;
    /* túnel (direct-tcpip, D253) */
    int      fd_sk;            /* el socket con el destino */
    bool     sk_morto;         /* el destino cortó: ya no se le puede escribir */
    uint32_t t_ip;
    unsigned t_porta, t_porta_local;
    unsigned t_ida, t_volta;   /* bytes cliente -> destino y destino -> cliente */
} canal_t;

#include "sshd_sftp.h"

static canal_t canais[MAX_CANAIS];
static unsigned sem_canais_desde;
static unsigned ultimo_rx;      /* tempo_ms() del último paquete del cliente */
static unsigned ultima_pergunta;
static int      perguntas;      /* «¿sigues ahí?» sin respuesta */
static char cliente_ip[16];
static int  cliente_porta;

static canal_t *canal_de(uint32_t id)
{
    if (id >= MAX_CANAIS || !canais[id].usado) desconectar(D_PROTOCOLO, "canal desconocido");
    return &canais[id];
}

static int n_canais(void)
{
    int n = 0;
    for (int i = 0; i < MAX_CANAIS; i++) n += canais[i].usado;
    return n;
}

/* Termina un proceso y todos los que lanzó (matar solo termina uno). */
static void matar_arvore(int raiz)
{
    static info_processo_t v[64];
    int marca[64], nm = 0;
    int n = info_sistema(INFO_PROCESSOS, v, 64);
    marca[nm++] = raiz;
    for (bool mais = true; mais;) {
        mais = false;
        for (int i = 0; i < n && nm < 64; i++) {
            if (!v[i].usuario || v[i].pid == meu_pid()) continue;
            bool pai = false, ja = false;
            for (int k = 0; k < nm; k++) {
                if (marca[k] == v[i].pai) pai = true;
                if (marca[k] == v[i].pid) ja = true;
            }
            if (pai && !ja) { marca[nm++] = v[i].pid; mais = true; }
        }
    }
    for (int k = nm - 1; k >= 0; k--) matar(marca[k]);
}

static void fechar_fd(int *fd)
{
    if (*fd >= 0) fechar(*fd);
    *fd = -1;
}

static void canal_liberar(canal_t *c)
{
    if (c->tipo == C_PROC && c->pid > 0 && !c->terminou) {
        matar_arvore(c->pid);   /* el zombi lo recoge el bucle principal (o el kernel al salir) */
        anotar("canal %d: proceso %d terminado al cerrarse el canal", (int)(c - canais), c->pid);
    }
    fechar_fd(&c->fd_in);
    fechar_fd(&c->fd_in_l);
    fechar_fd(&c->fd_out);
    fechar_fd(&c->fd_err);
    fechar_fd(&c->fd_tv);
    if (c->tipo == C_TUNEL) {
        char t[16];
        fechar_fd(&c->fd_sk);
        anotar("canal %d: túnel a %s:%u cerrado (%u bytes de ida, %u de vuelta)", (int)(c - canais),
               ip_para_texto(c->t_ip, t), c->t_porta, c->t_ida, c->t_volta);
    }
    sftp_destruir((sftp_t *)c->sftp_);
    libera(c->ent);
    libera(c->fila);
    memset(c, 0, sizeof *c);
    if (!n_canais()) sem_canais_desde = tempo_ms();
}

static void sessao_limpar(void)
{
    for (int i = 0; i < MAX_CANAIS; i++)
        if (canais[i].usado) canal_liberar(&canais[i]);
}

static void canal_resposta(canal_t *c, bool ok)
{
    m_iniciar(ok ? M_CANAL_OK : M_CANAL_FALHA);
    se_u32(&m, c->remoto);
    m_enviar();
}

/* Datos al cliente (ya cabe en su ventana: quien llama lo ha mirado). */
static void canal_dados(canal_t *c, const uint8_t *p, unsigned n, bool erros)
{
    m_iniciar(erros ? M_CANAL_EXTRA : M_CANAL_DADOS);
    se_u32(&m, c->remoto);
    if (erros) se_u32(&m, 1);   /* SSH_EXTENDED_DATA_STDERR */
    se_texto(&m, p, n);
    m_enviar();
    c->jan_remota -= n;
}

static unsigned canal_cabe(const canal_t *c)
{
    unsigned n = c->jan_remota;
    if (n > c->max_remoto) n = c->max_remoto;
    if (n > DADOS_MAX) n = DADOS_MAX;
    return n;
}

static bool canal_fila_por(canal_t *c, const uint8_t *p, unsigned n)
{
    if (c->fila_ini + c->fila_n + n > c->fila_cap) {
        if (c->fila_ini) {
            memmove(c->fila, c->fila + c->fila_ini, c->fila_n);
            c->fila_ini = 0;
        }
        if (c->fila_n + n > c->fila_cap) {
            unsigned nova = c->fila_cap ? c->fila_cap * 2 : 65536;
            while (nova < c->fila_n + n) nova *= 2;
            uint8_t *b = realoca(c->fila, nova);
            if (!b) fatal("sin memoria");
            c->fila = b;
            c->fila_cap = nova;
        }
    }
    memcpy(c->fila + c->fila_ini + c->fila_n, p, n);
    c->fila_n += n;
    return true;
}

/* Lo que el cliente mandó y ya se ha usado: cuando junta bastante, se le
 * devuelve ventana. */
static void canal_consumiu(canal_t *c, unsigned n)
{
    c->ent_ini += n;
    c->ent_n -= n;
    if (!c->ent_n) c->ent_ini = 0;
    c->consumido += n;
    if (c->consumido >= c->jan_max / 4) {
        m_iniciar(M_CANAL_JANELA);
        se_u32(&m, c->remoto);
        se_u32(&m, c->consumido);
        m_enviar();
        c->jan_local += c->consumido;
        c->consumido = 0;
    }
}

static void canal_eof(canal_t *c)
{
    if (c->eof_enviado) return;
    m_iniciar(M_CANAL_EOF);
    se_u32(&m, c->remoto);
    m_enviar();
    c->eof_enviado = true;
}

static void canal_fim(canal_t *c, bool com_codigo)
{
    canal_eof(c);
    if (com_codigo) {
        int cod = c->codigo;
        if (cod == SAIDA_ENCERRADO_PELO_KERNEL) cod = 137;
        else if (cod < 0 || cod > 255) cod = 255;
        m_iniciar(M_CANAL_PEDIDO);
        se_u32(&m, c->remoto);
        se_cstr(&m, "exit-status");
        se_u8(&m, 0);
        se_u32(&m, (uint32_t)cod);
        m_enviar();
    }
    if (!c->fechar_enviado) {
        m_iniciar(M_CANAL_FECHAR);
        se_u32(&m, c->remoto);
        m_enviar();
        c->fechar_enviado = true;
    }
}

/* Lanza sagsh (con -c ORDEN, o interactivo si comando es NULL) con su entrada
 * y sus salidas en tuberías, o en una PTY si el cliente la pidió. */
static bool canal_lancar(canal_t *c, const char *comando)
{
    static char e_cliente[64], e_term[48], e_lin[24], e_col[24];
    static char e_path[] = "PATH=" SAGUI_PATH_PADRAO, e_home[] = "HOME=/home";
    char *envp[56];
    int ne = 0;
    bool tem_path = false, tem_home = false;
    for (int i = 0; environ && environ[i] && ne < 46; i++) {
        if (comeca_com(environ[i], "TERM=") || comeca_com(environ[i], "LINES=") || comeca_com(environ[i], "COLUMNS=") ||
            comeca_com(environ[i], "SSH_CLIENT=")) continue;
        if (comeca_com(environ[i], "PATH=")) tem_path = true;
        if (comeca_com(environ[i], "HOME=")) tem_home = true;
        envp[ne++] = environ[i];
    }
    if (!tem_path) envp[ne++] = e_path;
    if (!tem_home) envp[ne++] = e_home;
    formatar_texto(e_cliente, sizeof e_cliente, "SSH_CLIENT=%s %d %d", cliente_ip, cliente_porta, conf.porta);
    envp[ne++] = e_cliente;
    if (c->pty) {
        formatar_texto(e_term, sizeof e_term, "TERM=%s", c->term[0] ? c->term : "xterm");
        formatar_texto(e_lin, sizeof e_lin, "LINES=%d", c->lins);
        formatar_texto(e_col, sizeof e_col, "COLUMNS=%d", c->cols);
        envp[ne++] = e_term;
        envp[ne++] = e_lin;
        envp[ne++] = e_col;
    }
    envp[ne] = NULL;
    char *argv[4] = { "sagsh", NULL, NULL, NULL };
    if (comando) { argv[1] = "-c"; argv[2] = (char *)comando; }

    int pin[2] = { -1, -1 }, pout[2] = { -1, -1 }, perr[2] = { -1, -1 }, tv[2] = { -1, -1 };
    bool ok = c->pty ? abrir_tv(tv) == 0 : criar_pipe(pin) == 0 && criar_pipe(pout) == 0 && criar_pipe(perr) == 0;
    c->ent = aloca(JANELA_LOCAL);
    int pid = ERRO_SEM_MEMORIA;
    if (ok && c->ent) {
        int g0 = duplicar(ENTRADA, 28), g1 = duplicar(SAIDA, 29), g2 = duplicar(ERROS, 30);
        if (c->pty) { duplicar(tv[1], ENTRADA); duplicar(tv[1], SAIDA); duplicar(tv[1], ERROS); }
        else { duplicar(pin[0], ENTRADA); duplicar(pout[1], SAIDA); duplicar(perr[1], ERROS); }
        pid = executar_env("/bin/sagsh", argv, envp);
        duplicar(g0, ENTRADA); duplicar(g1, SAIDA); duplicar(g2, ERROS);
        fechar(g0); fechar(g1); fechar(g2);
    }
    if (pid < 0) {
        int todos[8] = { pin[0], pin[1], pout[0], pout[1], perr[0], perr[1], tv[0], tv[1] };
        for (int i = 0; i < 8; i++) if (todos[i] >= 0) fechar(todos[i]);
        libera(c->ent);
        c->ent = NULL;
        anotar("canal %d: no se pudo lanzar sagsh (%s)", (int)(c - canais), nome_erro(pid));
        return false;
    }
    c->tipo = C_PROC;
    c->pid = pid;
    if (c->pty) {
        fechar(tv[1]);
        c->fd_tv = tv[0];
        c->err_fim = true;
    } else {
        fechar(pout[1]);
        fechar(perr[1]);
        c->fd_in = pin[1];
        c->fd_in_l = pin[0];   /* nuestra copia del lado de lectura: para saber cuánto cabe sin bloquear */
        c->fd_out = pout[0];
        c->fd_err = perr[0];
    }
    return true;
}

/* Mueve los datos de un canal con proceso. true = hizo algo. */
static bool bombear_proc(canal_t *c)
{
    static uint8_t bruto[4096], conv[3 * 4096 + 8];
    bool fez = false;
    if (c->fechar_enviado) {   /* canal ya cerrado por nuestro lado: ni un dato más (RFC 4254, 5.3) */
        c->ent_n = c->ent_ini = 0;
        return false;
    }
    /* del cliente al proceso */
    if (c->ent_n) {
        if (c->pty) {
            unsigned k = c->ent_n > 64 ? 64 : c->ent_n;   /* poco a poco: la PTY tira lo que no le cabe */
            unsigned n = ssh_teclas(&c->tecla, c->ent + c->ent_ini, k, conv);
            if (n) escrever(c->fd_tv, conv, n);
            canal_consumiu(c, k);
            fez = true;
        } else if (c->fd_in >= 0) {
            int d = disponivel(c->fd_in_l);
            unsigned livre = d >= 0 && (unsigned)d < PIPE_KERNEL ? PIPE_KERNEL - (unsigned)d : 0;
            if (c->terminou) livre = c->ent_n;   /* ya no hay quien lo lea: se tira */
            if (livre) {
                unsigned k = c->ent_n < livre ? c->ent_n : livre;
                if (!c->terminou) escrever(c->fd_in, c->ent + c->ent_ini, k);
                canal_consumiu(c, k);
                fez = true;
            }
        } else canal_consumiu(c, c->ent_n);
    }
    if (c->eof_cliente && !c->ent_n && !c->eof_entregue) {
        c->eof_entregue = true;
        if (c->pty) escrever(c->fd_tv, "\x04", 1);   /* Ctrl+D */
        else fechar_fd(&c->fd_in);
        fez = true;
    }
    /* del proceso al cliente, solo lo que quepa en su ventana */
    c->sem_janela = false;
    for (int qual = 0; qual < 2; qual++) {
        int fd = qual ? c->fd_err : (c->pty ? c->fd_tv : c->fd_out);
        bool *fim = qual ? &c->err_fim : &c->out_fim;
        if (*fim || fd < 0) continue;
        unsigned cabe = canal_cabe(c);
        if (c->pty || c->utf8) cabe /= 3;   /* la conversión puede triplicar */
        if (cabe > sizeof bruto) cabe = sizeof bruto;
        if (!cabe) { c->sem_janela = true; continue; }
        int d = disponivel(fd);
        if (d == 0) continue;
        if (d < 0) { *fim = true; fez = true; continue; }
        int n = ler(fd, bruto, (unsigned)d < cabe ? (unsigned)d : cabe);
        if (n <= 0) { *fim = true; fez = true; continue; }
        if (c->pty) canal_dados(c, conv, ssh_tela(bruto, (unsigned)n, conv), false);
        else if (c->utf8) canal_dados(c, conv, ssh_para_utf8(bruto, (unsigned)n, conv), qual == 1);
        else canal_dados(c, bruto, (unsigned)n, qual == 1);
        fez = true;
    }
    /* El proceso terminó: se vacían sus tuberías y se cierra el canal con su
     * código. Un nieto en segundo plano puede tenerlas abiertas: con 300 ms
     * sin que salga nada, se da por terminado. */
    if (c->terminou && !c->fechar_enviado) {
        if (fez || c->sem_janela) c->t_fim = tempo_ms();
        if ((c->out_fim && c->err_fim) || tempo_ms() - c->t_fim > 300) {
            canal_fim(c, true);
            fez = true;
        }
    }
    return fez;
}

static bool bombear_sftp(canal_t *c)
{
    bool fez = false;
    sftp_t *s = (sftp_t *)c->sftp_;
    if (c->fechar_enviado) return false;   /* canal ya cerrado por nuestro lado */
    /* pedidos completos, mientras las respuestas en cola no sean demasiadas */
    while (c->ent_n >= 4 && c->fila_n < FILA_MAX && !c->fechar_enviado) {
        const uint8_t *q = c->ent + c->ent_ini;
        uint32_t t = (uint32_t)q[0] << 24 | (uint32_t)q[1] << 16 | (uint32_t)q[2] << 8 | q[3];
        if (t < 1 || t > SFTP_PKT_MAX) {
            anotar("sftp: paquete de tamaño no válido (%u): canal cerrado", t);
            canal_fim(c, false);
            return true;
        }
        if (c->ent_n < 4 + t) break;
        sftp_pedido(c, s, q + 4, t);
        canal_consumiu(c, 4 + t);
        fez = true;
    }
    while (c->fila_n) {
        unsigned k = canal_cabe(c);
        if (!k) break;
        if (k > c->fila_n) k = c->fila_n;
        canal_dados(c, c->fila + c->fila_ini, k, false);
        c->fila_ini += k;
        c->fila_n -= k;
        if (!c->fila_n) c->fila_ini = 0;
        fez = true;
    }
    if (c->eof_cliente && !c->fila_n && c->ent_n < 4 && !c->fechar_enviado) {
        c->codigo = 0;
        canal_fim(c, true);
        fez = true;
    }
    return fez;
}

/* ------------------------------------------------------------ túneles: ssh -L (RFC 4254, 7.2; D253) */

/* Abre el socket con el destino de un canal direct-tcpip. 0 = abierto; si no,
 * el código de SSH_MSG_CHANNEL_OPEN_FAILURE (1 = prohibido, 2 = no se pudo
 * conectar) y el motivo. La conexión espera (como mucho CONECTAR_MS): hacia
 * esta máquina la respuesta es inmediata. */
static unsigned tunel_abrir(canal_t *c, ssh_leitor_t *l, const char **motivo)
{
    static char texto[96];
    char host[64], orig[48], t[16];
    unsigned nh, no;
    int id = (int)(c - canais);
    const uint8_t *h = sl_texto(l, &nh);
    uint32_t porta = sl_u32(l);
    const uint8_t *o = sl_texto(l, &no);
    uint32_t porta_o = sl_u32(l);
    if (l->erro) desconectar(D_PROTOCOLO, "apertura de canal mal formada");
    if (nh >= sizeof host) nh = sizeof host - 1;   /* un nombre así de largo no es ni localhost ni una dirección */
    if (no >= sizeof orig) no = sizeof orig - 1;
    for (unsigned i = 0; i < nh; i++) host[i] = h[i] > 0x20 && h[i] < 0x7f ? (char)h[i] : '?';
    for (unsigned i = 0; i < no; i++) orig[i] = o[i] > 0x20 && o[i] < 0x7f ? (char)o[i] : '?';
    host[nh] = orig[no] = 0;

    config_rede_t cr;
    memset(&cr, 0, sizeof cr);
    config_rede(REDE_CONSULTAR, &cr);
    uint32_t ip = 0;
    int r = ssh_destino_tunel(host, conf.redirigir, cr.ip, cr.mascara, &ip);
    *motivo = NULL;
    if (conf.redirigir == SSH_RED_NO) *motivo = "redirección de puertos desactivada (redirigir=no)";
    else if (r == -2) *motivo = "el destino tiene que ser localhost o una dirección IPv4 (los nombres no se resuelven)";
    else if (r < 0) *motivo = conf.redirigir == SSH_RED_REDE ? "destino fuera de la red local (redirigir=red-local)"
                                                             : "solo se redirige a esta máquina: localhost (redirigir=local)";
    else if (porta == 0 || porta > 65535) *motivo = "puerto de destino no válido";
    if (*motivo) {
        anotar("túnel a %s:%u (pedido desde %s:%u) RECHAZADO: %s", host, porta, orig, porta_o, *motivo);
        return 1;   /* SSH_OPEN_ADMINISTRATIVELY_PROHIBITED */
    }
    int fd = criar_socket(SOCK_TCP);
    int e = fd;
    if (fd >= 0) {
        tempo_limite(fd, CONECTAR_MS);
        e = conectar(fd, ip, (int)porta);
    }
    c->ent = e < 0 ? NULL : aloca(JANELA_TUNEL);
    if (e >= 0 && !c->ent) e = ERRO_SEM_MEMORIA;
    if (e < 0) {
        if (fd >= 0) fechar(fd);
        formatar_texto(texto, sizeof texto, "no se pudo conectar con %s:%u: %s", ip_para_texto(ip, t), porta, nome_erro(e));
        *motivo = texto;
        anotar("túnel a %s:%u (pedido desde %s:%u) rechazado: %s", host, porta, orig, porta_o, nome_erro(e));
        return 2;   /* SSH_OPEN_CONNECT_FAILED */
    }
    /* Las lecturas se hacen solo con datos esperando y las escrituras solo con
     * sitio en la cola (tunel_livre); por si acaso, ninguna espera más de esto. */
    tempo_limite(fd, 20);
    enderecos_socket_t en;
    memset(&en, 0, sizeof en);
    opcao_socket(fd, OPC_ENDERECOS, (unsigned)&en);
    c->tipo = C_TUNEL;
    c->fd_sk = fd;
    c->t_ip = ip;
    c->t_porta = porta;
    c->t_porta_local = en.porta_local;
    anotar("canal %d: túnel a %s:%u abierto (pedido desde %s:%u)", id, ip_para_texto(ip, t), porta, orig, porta_o);
    return 0;
}

/* Cuánto se le puede escribir al destino sin que la escritura espere: lo que
 * queda libre en la cola de envío de su socket (INFO_SOCKETS, D079). */
static unsigned tunel_livre(const canal_t *c)
{
    static info_socket_t v[48];
    int n = info_sistema(INFO_SOCKETS, v, 48);
    int eu = meu_pid();
    for (int i = 0; i < n; i++) {
        const info_conexao_t *k = &v[i].c;
        if (k->tipo != SOCK_TCP || k->pid != eu || k->porta_local != c->t_porta_local || k->porta_remota != c->t_porta ||
            k->ip_remoto != c->t_ip) continue;
        return k->na_fila_tx < TX_KERNEL ? TX_KERNEL - k->na_fila_tx : 0;
    }
    return 512;   /* no aparece (no debería): poco a poco, con el plazo del socket */
}

/* Lo que mandó el cliente y aún espera, al destino. true = hizo algo. */
static bool tunel_escrever(canal_t *c)
{
    if (!c->ent_n) return false;
    if (c->sk_morto) { canal_consumiu(c, c->ent_n); return true; }   /* ya no hay a quién dárselo */
    unsigned k = tunel_livre(c);
    if (!k) return false;
    if (k > c->ent_n) k = c->ent_n;
    int r = escrever(c->fd_sk, c->ent + c->ent_ini, k);
    if (r > 0) {
        c->t_ida += (unsigned)r;
        canal_consumiu(c, (unsigned)r);
        return true;
    }
    if (r == ERRO_TEMPO || r == 0) return false;
    c->sk_morto = true;
    canal_consumiu(c, c->ent_n);
    return true;
}

/* Mueve los datos de un túnel en los dos sentidos y lo cierra cuando acaban
 * los dos (o cuando el destino corta). true = hizo algo. */
static bool bombear_tunel(canal_t *c)
{
    static uint8_t bruto[DADOS_MAX];
    bool fez = false;
    if (c->fechar_enviado) {   /* cerrado por nuestro lado: se espera el CLOSE del cliente */
        c->ent_n = c->ent_ini = 0;
        return false;
    }
    /* del cliente al destino; su EOF es el fin de envío (FIN) del socket */
    fez |= tunel_escrever(c);
    if (c->eof_cliente && !c->ent_n && !c->eof_entregue) {
        c->eof_entregue = true;
        if (!c->sk_morto) opcao_socket(c->fd_sk, OPC_FECHAR_ENVIO, 0);
        fez = true;
    }
    /* del destino al cliente, solo lo que quepa en su ventana */
    c->sem_janela = false;
    if (!c->out_fim) {
        int d = disponivel(c->fd_sk);
        if (d > 0) {
            unsigned cabe = canal_cabe(c);
            if (cabe > sizeof bruto) cabe = sizeof bruto;
            if (!cabe) c->sem_janela = true;
            else {
                int n = ler(c->fd_sk, bruto, (unsigned)d < cabe ? (unsigned)d : cabe);
                if (n > 0) {
                    canal_dados(c, bruto, (unsigned)n, false);
                    c->t_volta += (unsigned)n;
                    fez = true;
                } else if (n != ERRO_TEMPO) d = -1;
            }
        }
        if (d < 0) {   /* el destino cerró su envío (FIN) o cortó */
            c->out_fim = true;
            if (opcao_socket(c->fd_sk, OPC_ESTADO, 0) == CON_FECHADA) c->sk_morto = true;
            canal_eof(c);
            fez = true;
        }
    }
    if (c->sk_morto && !c->out_fim) {   /* cortó mientras se le escribía */
        c->out_fim = true;
        fez = true;
    }
    if (c->out_fim && (c->eof_entregue || c->sk_morto)) {
        canal_fim(c, false);
        fechar_fd(&c->fd_sk);   /* el socket se suelta ya, sin esperar el CLOSE del cliente */
        fez = true;
    }
    return fez;
}

/* Una petición sobre un canal: pty-req, shell, exec, subsystem... */
static void canal_pedido(canal_t *c, ssh_leitor_t *l)
{
    static char cmd_utf[2048], cmd[2048];
    char tipo[40];
    int id = (int)(c - canais);
    sl_cstr(l, tipo, sizeof tipo);
    bool quer = sl_u8(l) != 0;
    if (l->erro) desconectar(D_PROTOCOLO, "petición de canal mal formada");
    bool ok = false;
    if (compara_texto(tipo, "pty-req") == 0 && c->tipo == C_NOVO) {
        sl_cstr(l, c->term, sizeof c->term);
        uint32_t cols = sl_u32(l), lins = sl_u32(l);
        if (!l->erro) {
            for (char *p = c->term; *p; p++)
                if (*p < 0x21 || *p > 0x7e || *p == '=') *p = '_';
            c->pty = true;
            c->cols = cols > 0 && cols < 1000 ? (int)cols : 80;
            c->lins = lins > 0 && lins < 1000 ? (int)lins : 24;
            ok = true;
        }
    } else if (compara_texto(tipo, "window-change") == 0) {
        /* la PTY de SaguiOS no tiene tamaño: se anota y nada más */
        uint32_t cols = sl_u32(l), lins = sl_u32(l);
        if (!l->erro && cols < 1000 && lins < 1000) { c->cols = (int)cols; c->lins = (int)lins; ok = true; }
    } else if (compara_texto(tipo, "shell") == 0 && c->tipo == C_NOVO) {
        ok = canal_lancar(c, NULL);
        anotar("canal %d: shell%s%s", id, c->pty ? " con terminal" : " sin terminal", ok ? "" : " (falló)");
    } else if (compara_texto(tipo, "exec") == 0 && c->tipo == C_NOVO) {
        unsigned n;
        const uint8_t *p = sl_texto(l, &n);
        bool cabe = !l->erro && n < sizeof cmd_utf;
        for (unsigned i = 0; cabe && i < n; i++)
            if (!p[i]) cabe = false;
        if (cabe) {
            memcpy(cmd_utf, p, n);
            cmd_utf[n] = 0;
            n = ssh_de_utf8(p, n, (uint8_t *)cmd);   /* el cliente escribe en UTF-8; SaguiOS, en Latin-1 */
            cmd[n] = 0;
            c->utf8 = conf.exec_utf8;
            ok = canal_lancar(c, cmd);
            anotar("canal %d: orden%s: %s%s", id, c->pty ? " (con terminal)" : "", cmd_utf, ok ? "" : " (falló)");
        } else anotar("canal %d: orden demasiado larga o mal formada: rechazada", id);
    } else if (compara_texto(tipo, "subsystem") == 0 && c->tipo == C_NOVO) {
        char nome[32];
        sl_cstr(l, nome, sizeof nome);
        if (!l->erro && compara_texto(nome, "sftp") == 0) {
            c->ent = aloca(JANELA_LOCAL);
            c->sftp_ = sftp_criar();
            if (c->ent && c->sftp_) { c->tipo = C_SFTP; ok = true; }
            anotar("canal %d: subsistema sftp%s", id, ok ? "" : " (sin memoria)");
        } else anotar("canal %d: subsistema desconocido rechazado", id);
    }
    /* env, signal, x11-req, auth-agent-req...: no se atienden */
    if (quer) canal_resposta(c, ok);
}

static void despachar(const uint8_t *pl, unsigned pn)
{
    ssh_leitor_t l;
    unsigned tipo = pl[0];
    sl_iniciar(&l, pl + 1, pn - 1);
    switch (tipo) {
    case M_DESCONECTAR:
        anotar("desconexión del cliente");
        terminar(SAIR_BEM);
    case M_IGNORAR:
    case M_DEPURAR:
    case M_NAO_IMPLEM:
        return;
    case M_KEXINIT: {
        static uint8_t ic[PKT_MAX];
        memcpy(ic, pl, pn);
        trocar_chaves(ic, pn);
        return;
    }
    case M_GLOBAL: {
        unsigned n;
        const uint8_t *nome = sl_texto(&l, &n);
        bool quer = sl_u8(&l) != 0;
        if (l.erro) desconectar(D_PROTOCOLO, "petición global mal formada");
        if (sl_igual(nome, n, "tcpip-forward")) anotar("redirección remota (ssh -R) RECHAZADA: no se admite");
        if (quer) { m_iniciar(M_GLOBAL_FALHA); m_enviar(); }   /* sin «ssh -R»; así también responde a los «keepalive» */
        return;
    }
    case M_CANAL_ABRIR: {
        unsigned nt;
        const uint8_t *t = sl_texto(&l, &nt);
        uint32_t remoto = sl_u32(&l), janela = sl_u32(&l), max = sl_u32(&l);
        if (l.erro) desconectar(D_PROTOCOLO, "apertura de canal mal formada");
        int livre = -1, n_sessoes = 0, n_tuneis = 0;
        for (int i = MAX_CANAIS - 1; i >= 0; i--) {
            if (!canais[i].usado) livre = i;
            else if (canais[i].tipo == C_TUNEL) n_tuneis++;
            else n_sessoes++;
        }
        bool sessao = sl_igual(t, nt, "session"), tunel = sl_igual(t, nt, "direct-tcpip");
        unsigned recusa = 0;
        const char *motivo = NULL;
        canal_t *c = livre >= 0 ? &canais[livre] : NULL;
        if (!sessao && !tunel) {
            recusa = 3;   /* SSH_OPEN_UNKNOWN_CHANNEL_TYPE */
            motivo = "solo hay canales de sesión y direct-tcpip (ssh -L)";
            anotar("canal de un tipo no admitido rechazado (solo «session» y «direct-tcpip»)");
        } else if (!c || max < 256 || (sessao ? n_sessoes >= MAX_SESSOES : n_tuneis >= MAX_TUNEIS)) {
            recusa = 4;   /* SSH_OPEN_RESOURCE_SHORTAGE */
            motivo = "demasiados canales";
            if (tunel) anotar("túnel RECHAZADO: ya hay %d en esta conexión (el máximo)", n_tuneis);
        }
        if (!recusa) {
            memset(c, 0, sizeof *c);
            c->tipo = C_NOVO;
            c->remoto = remoto;
            c->jan_remota = janela;
            c->max_remoto = max;
            c->jan_local = c->jan_max = tunel ? JANELA_TUNEL : JANELA_LOCAL;
            c->fd_in = c->fd_in_l = c->fd_out = c->fd_err = c->fd_tv = c->fd_sk = -1;
            c->cols = 80;
            c->lins = 24;
            if (tunel) recusa = tunel_abrir(c, &l, &motivo);
        }
        if (recusa) {
            m_iniciar(M_CANAL_RECUSA);
            se_u32(&m, remoto);
            se_u32(&m, recusa);
            se_cstr(&m, motivo);
            se_cstr(&m, "es");
            m_enviar();
            return;
        }
        c->usado = true;
        m_iniciar(M_CANAL_ABERTO);
        se_u32(&m, remoto);
        se_u32(&m, (uint32_t)livre);
        se_u32(&m, c->jan_max);
        se_u32(&m, DADOS_MAX);
        m_enviar();
        return;
    }
    case M_CANAL_JANELA: {
        canal_t *c = canal_de(sl_u32(&l));
        uint32_t mais = sl_u32(&l);
        if (l.erro || mais > 0xFFFFFFFFu - c->jan_remota) desconectar(D_PROTOCOLO, "ajuste de ventana no válido");
        c->jan_remota += mais;
        return;
    }
    case M_CANAL_DADOS:
    case M_CANAL_EXTRA: {
        canal_t *c = canal_de(sl_u32(&l));
        unsigned n;
        if (tipo == M_CANAL_EXTRA) sl_u32(&l);
        const uint8_t *d = sl_texto(&l, &n);
        if (l.erro || n > c->jan_local || n > DADOS_MAX) desconectar(D_PROTOCOLO, "datos de canal fuera de la ventana");
        c->jan_local -= n;
        if (tipo == M_CANAL_EXTRA || !c->ent || c->eof_cliente || c->fechar_enviado) {
            c->consumido += n;   /* no hay a quién dárselo: se descarta */
            return;
        }
        if (c->ent_n + n > c->jan_max) desconectar(D_PROTOCOLO, "datos de canal fuera de la ventana");
        if (c->ent_ini + c->ent_n + n > c->jan_max) {
            memmove(c->ent, c->ent + c->ent_ini, c->ent_n);
            c->ent_ini = 0;
        }
        memcpy(c->ent + c->ent_ini + c->ent_n, d, n);
        c->ent_n += n;
        return;
    }
    case M_CANAL_EOF: {
        canal_t *c = canal_de(sl_u32(&l));
        c->eof_cliente = true;
        return;
    }
    case M_CANAL_FECHAR: {
        canal_t *c = canal_de(sl_u32(&l));
        if (c->tipo == C_TUNEL && !c->fechar_enviado) tunel_escrever(c);   /* lo último que mandó, si cabe */
        if (!c->fechar_enviado) {
            m_iniciar(M_CANAL_FECHAR);
            se_u32(&m, c->remoto);
            m_enviar();
        }
        canal_liberar(c);
        return;
    }
    case M_CANAL_PEDIDO: {
        canal_t *c = canal_de(sl_u32(&l));
        canal_pedido(c, &l);
        return;
    }
    case M_CANAL_OK:
    case M_CANAL_FALHA:
    case M_GLOBAL_OK:
    case M_GLOBAL_FALHA:   /* la respuesta a nuestro «¿sigues ahí?» */
        return;
    default:
        m_iniciar(M_NAO_IMPLEM);
        se_u32(&m, seq_rx - 1);
        m_enviar();
        return;
    }
}

/* Una conexión entera; el socket llega como descriptor 0. */
static int sessao(const char *ip, const char *porta)
{
    copia_texto(cliente_ip, ip, sizeof cliente_ip);
    cliente_porta = texto_para_int(porta);
    formatar_texto(quem, sizeof quem, "%s:%d", cliente_ip, cliente_porta);
    sk = duplicar(ENTRADA, 10);
    if (sk < 0) return SAIR_ERRO;
    /* la entrada pasa a ser una tubería sin escritor: nadie lee del socket por error */
    int p[2];
    if (criar_pipe(p) == 0) {
        duplicar(p[0], ENTRADA);
        fechar(p[0]);
        fechar(p[1]);
    }
    conf_carregar();
    autorizadas_carregar();
    rx = aloca(RX_TAM);
    tx = aloca(PKT_MAX + 64);
    msg = aloca(PKT_MAX);
    if (!rx || !tx || !msg || !chave_servidor(false)) {
        anotar("sesión sin memoria o sin la clave del servidor");
        return SAIR_ERRO;
    }
    escrever_texto(sk, VERSAO_SSH "\r\n");
    ler_versao();
    anotar("conexión; cliente «%s»", v_cliente);

    const uint8_t *pl;
    unsigned pn;
    pacote_esperar(&pl, &pn, 15000);
    if (pl[0] != M_KEXINIT) desconectar(D_PROTOCOLO, "se esperaba el KEXINIT");
    static uint8_t ic[PKT_MAX];
    memcpy(ic, pl, pn);
    trocar_chaves(ic, pn);
    autenticar();

    sem_canais_desde = ultimo_rx = ultima_pergunta = tempo_ms();
    unsigned visto = (unsigned)esperar_rede(0, 0);
    for (;;) {
        bool fez = false;
        for (int k = 0; k < 16; k++) {
            int r = pacote_tentar(&pl, &pn);
            if (r) {
                ultimo_rx = tempo_ms();
                perguntas = 0;
                despachar(pl, pn);
                fez = true;
                continue;
            }
            int e = rx_encher(0);
            if (e < 0) {
                anotar("el cliente cerró la conexión");
                terminar(SAIR_BEM);
            }
            if (e == 0) break;
            fez = true;
        }
        int pid, cod;
        while ((pid = recolher(&cod)) > 0) {
            for (int i = 0; i < MAX_CANAIS; i++) {
                canal_t *c = &canais[i];
                if (!c->usado || c->tipo != C_PROC || c->pid != pid || c->terminou) continue;
                c->terminou = true;
                c->codigo = cod;
                c->t_fim = tempo_ms();
                anotar("canal %d: el proceso %d terminó con código %d", i, pid, cod);
            }
            fez = true;
        }
        bool procs = false;
        for (int i = 0; i < MAX_CANAIS; i++) {
            canal_t *c = &canais[i];
            if (!c->usado) continue;
            if (c->tipo == C_PROC) { procs = true; fez |= bombear_proc(c); }
            else if (c->tipo == C_SFTP) fez |= bombear_sftp(c);
            else if (c->tipo == C_TUNEL) {
                fez |= bombear_tunel(c);
                if (c->ent_n) procs = true;   /* espera sitio en la cola del destino: eso no despierta a esperar_rede */
            }
        }
        if (!n_canais() && tempo_ms() - sem_canais_desde > (conf.redirigir == SSH_RED_NO ? SEM_CANAL_MS : SEM_CANAL_TUNEL_MS))
            desconectar(D_PELO_APLIC, "demasiado tiempo sin ningún canal abierto");
        /* Un cliente que desaparece sin cerrar (el PC se suspende, se cae la
         * red) dejaría la sesión ocupando sitio para siempre: tras un minuto
         * de silencio se le pregunta, y a la tercera sin respuesta se corta. */
        unsigned agora = tempo_ms();
        if (agora - ultimo_rx > VIVO_MS && agora - ultima_pergunta > VIVO_MS) {
            if (perguntas >= VIVO_MAX) fatal("el cliente no responde");
            m_iniciar(M_GLOBAL);
            se_cstr(&m, "keepalive@openssh.com");
            se_u8(&m, 1);
            m_enviar();
            perguntas++;
            ultima_pergunta = agora;
        }
        if (fez) hilo_ceder();
        else if (procs) dormir(5);        /* las tuberías no despiertan a esperar_rede */
        else visto = (unsigned)esperar_rede(visto, 500);
    }
}

/* ------------------------------------------------------------ el proceso que escucha */

#define MAX_FILHOS  8
#define MAX_FALHAS  16

static struct { int pid; uint32_t ip; } filhos[MAX_FILHOS];
static struct { uint32_t ip; int falhas; unsigned ate; bool bloqueado; } falhas[MAX_FALHAS];

static int n_filhos(void)
{
    int n = 0;
    for (int i = 0; i < MAX_FILHOS; i++) n += filhos[i].pid != 0;
    return n;
}

static void anotar_falha(uint32_t ip, bool falhou)
{
    int livre = -1, velho = 0;
    char t[16];
    for (int i = 0; i < MAX_FALHAS; i++) {
        if (falhas[i].ip == ip) {
            if (!falhou) { falhas[i].ip = 0; return; }
            if (++falhas[i].falhas >= conf.falhas_bloqueio && conf.bloqueio_s) {
                falhas[i].bloqueado = true;
                falhas[i].ate = tempo_ms() + (unsigned)conf.bloqueio_s * 1000;
                anotar("%s bloqueada %d s: %d conexiones seguidas sin autenticarse", ip_para_texto(ip, t), conf.bloqueio_s, falhas[i].falhas);
            }
            return;
        }
        if (!falhas[i].ip && livre < 0) livre = i;
        if (falhas[i].ate < falhas[velho].ate) velho = i;
    }
    if (!falhou) return;
    if (livre < 0) livre = velho;
    falhas[livre].ip = ip;
    falhas[livre].falhas = 1;
    falhas[livre].bloqueado = false;
    falhas[livre].ate = tempo_ms();
    if (conf.falhas_bloqueio <= 1 && conf.bloqueio_s) {
        falhas[livre].bloqueado = true;
        falhas[livre].ate = tempo_ms() + (unsigned)conf.bloqueio_s * 1000;
    }
}

static bool bloqueada(uint32_t ip)
{
    for (int i = 0; i < MAX_FALHAS; i++) {
        if (falhas[i].ip != ip || !falhas[i].bloqueado) continue;
        if ((int)(falhas[i].ate - tempo_ms()) > 0) return true;
        falhas[i].ip = 0;   /* cumplió */
        falhas[i].bloqueado = false;
    }
    return false;
}

static void recolher_filhos(void)
{
    int pid, cod;
    while ((pid = recolher(&cod)) > 0)
        for (int i = 0; i < MAX_FILHOS; i++)
            if (filhos[i].pid == pid) {
                anotar_falha(filhos[i].ip, cod != SAIR_BEM);
                filhos[i].pid = 0;
            }
}

static int escutar_ssh(int porta_pedida)
{
    conf_carregar();
    if (porta_pedida) conf.porta = porta_pedida;
    if (!chave_servidor(true)) return 1;
    autorizadas_carregar();
    char h[64];
    impressao(srv_publica, h, sizeof h);
    int e = criar_socket(SOCK_TCP);
    int r = e < 0 ? e : escutar(e, conf.porta, 4);
    if (r < 0) {
        anotar("no se puede escuchar en el puerto %d: %s", conf.porta, nome_erro(r));
        imprimir_em(ERROS, "sshd: puerto %d: %s\n", conf.porta, nome_erro(r));
        return 1;
    }
    anotar("escuchando en el puerto %d (%s); clave del servidor %s; %d clave(s) autorizada(s)", conf.porta,
           conf.so_local ? "solo red local" : "CUALQUIER dirección", h, n_autorizadas);
    if (!chave_persistente())
        anotar("aviso: /home no es persistente: la clave del servidor cambiará en cada arranque (el cliente avisará de que ha cambiado)");
    if (!n_autorizadas)
        anotar("aviso: no hay ninguna clave en " DIR_CASA "/authorized_keys ni en " DIR_ETC "/authorized_keys: nadie puede entrar");
    if (n_recusadas)
        anotar("aviso: %d línea(s) de authorized_keys no valen (solo ssh-ed25519, sin opciones delante)", n_recusadas);
    imprimir("sshd: escuchando en el puerto %d; huella %s\n", conf.porta, h);
    cripto_apagar(srv_semente, sizeof srv_semente);   /* quien escucha no firma: la relee cada sesión */

    bool com_tubo = tipo_descritor(ENTRADA) == TIPO_PIPE;
    tempo_limite(e, 300);
    for (;;) {
        /* el gestor de servicios pide parar cerrando nuestra entrada (D216) */
        if (com_tubo && disponivel(ENTRADA) == ERRO_PIPE_QUEBRADO) {
            anotar("parada pedida por el gestor de servicios (%d sesión(es) abiertas siguen hasta que terminen)", n_filhos());
            fechar(e);
            return 0;
        }
        recolher_filhos();
        endereco_rede_t de;
        int s = aceitar_conexao(e, &de);
        if (s == ERRO_TEMPO) continue;
        if (s < 0) { dormir(300); continue; }
        char ip[16], porta[8];
        ip_para_texto(de.ip, ip);
        formatar_texto(porta, sizeof porta, "%u", de.porta);
        config_rede_t cr;
        memset(&cr, 0, sizeof cr);
        config_rede(REDE_CONSULTAR, &cr);
        const char *motivo = NULL;
        if (conf.so_local && !ssh_ip_local(de.ip, cr.ip, cr.mascara)) motivo = "no es de la red local (solo_red_local=si)";
        else if (bloqueada(de.ip)) motivo = "dirección bloqueada por fallos de autenticación";
        else if (n_filhos() >= conf.max_conexoes) motivo = "demasiadas conexiones a la vez";
        int pid = 0;
        if (!motivo) {
            char *argv[] = { "sshd", "--sesion", ip, porta, NULL };
            int g0 = duplicar(ENTRADA, 28);
            duplicar(s, ENTRADA);
            pid = executar("/bin/sshd", argv);
            duplicar(g0, ENTRADA);
            fechar(g0);
            if (pid < 0) motivo = nome_erro(pid);
        }
        fechar(s);
        if (motivo) { anotar("conexión de %s:%s rechazada: %s", ip, porta, motivo); continue; }
        for (int i = 0; i < MAX_FILHOS; i++)
            if (!filhos[i].pid) { filhos[i].pid = pid; filhos[i].ip = de.ip; break; }
    }
}

/* ------------------------------------------------------------ principal */

static const char AYUDA[] =
    "uso: sshd [-p PUERTO]    servidor SSH de administración (D250)\n"
    "     sshd --huella       huella de la clave del servidor (la crea si no existe)\n"
    "     sshd --claves       claves autorizadas\n"
    "Como servicio (desactivado de serie):  servicio activar sshd ; servicio iniciar sshd\n"
    "Solo clave pública ssh-ed25519: la del PC (~/.ssh/id_ed25519.pub) va en una línea de\n"
    DIR_CASA "/authorized_keys. Desde el PC: ssh root@IP, ssh root@IP 'dmesg', scp, sftp.\n"
    "Opciones en " DIR_ETC "/sshd.conf y " DIR_CASA "/sshd.conf: puerto, solo_red_local,\n"
    "max_conexiones, max_intentos, exec_utf8, redirigir. Registro: /logs/sshd.log.\n"
    "Túnel a un servicio de esta máquina (VNC): ssh -L 5900:localhost:5900 root@IP. Ver docs/ssh.md.\n";

int principal(int argc, char **argv)
{
    int porta = 0;
    if (argc >= 4 && compara_texto(argv[1], "--sesion") == 0) return sessao(argv[2], argv[3]);
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--ayuda") == 0 || compara_texto(argv[i], "--help") == 0) {
            escrever_texto(SAIDA, AYUDA);
            return 0;
        } else if (compara_texto(argv[i], "--huella") == 0) {
            char h[64], l[160];
            if (!chave_servidor(true)) return 1;
            impressao(srv_publica, h, sizeof h);
            linha_publica(srv_publica, l, sizeof l);
            imprimir("%s (ED25519)\n%s\narchivo: %s%s\n", h, l, srv_arquivo[0] ? srv_arquivo : "(no se pudo guardar)",
                     chave_persistente() ? "" : "\naviso: /home no es persistente; la clave cambiará en el próximo arranque");
            return 0;
        } else if (compara_texto(argv[i], "--claves") == 0) {
            autorizadas_carregar();
            for (int k = 0; k < n_autorizadas; k++) {
                char h[64];
                impressao(autorizadas[k].pub, h, sizeof h);
                imprimir("%s %s\n", h, autorizadas[k].comentario);
            }
            imprimir("%d clave(s) autorizada(s)", n_autorizadas);
            if (n_recusadas) imprimir("; %d línea(s) no valen (solo ssh-ed25519, sin opciones delante)", n_recusadas);
            imprimir("\n");
            return 0;
        } else if (compara_texto(argv[i], "-p") == 0 && i + 1 < argc) {
            porta = texto_para_int(argv[++i]);
            if (porta <= 0 || porta > 65535) { imprimir_em(ERROS, "sshd: puerto no válido\n"); return 2; }
        } else {
            escrever_texto(ERROS, AYUDA);
            return 2;
        }
    }
    return escutar_ssh(porta);
}
