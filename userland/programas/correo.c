// SPDX-License-Identifier: GPL-2.0-or-later
/* correo.c - cliente de correo con ventana (082).
 *
 * Recibe por POP3 o IMAP y envía por SMTP. Los mensajes recibidos se guardan en
 * /home/correo/ (un fichero por mensaje) para verlos otra vez. Los cuerpos pueden venir
 * en texto plano o en HTML básico (se convierten a texto). Se pueden mandar adjuntos.
 *
 *   correo [--proto pop3|imap] [--host H] [--porta P] [--porta-envio P]
 *          [--utilizador U] [--clave C] [--tls]
 *
 * "--porta" es el de recepción (POP3 o IMAP) y "--porta-envio" el de envío (SMTP):
 * no son el mismo (110 y 143 para recibir, 587 o 25 para enviar).
 *
 * Sin argumentos lee la cuenta de /home/.config/correo (clave=valor). Si no hay cuenta,
 * abre la ventana de cuenta. La clave va en ese fichero: no se guarda en el repositorio.
 *
 * Sin libc, sin hilos, sin coma flotante: la red bloquea la ventana durante la petición,
 * como en el resto de programas con red. Todo el estado grande va en static (la pila es
 * de 64 KB). Los textos de la interfaz van en castellano; el texto de la red llega en
 * UTF-8 y se convierte a Latin-1 para la pantalla. */
#include "correo.h"
#include "gui.h"

#define ARQ_CONTA   "/home/.config/correo"
#define DIR_CONF    "/home/.config"
#define DIR_CORREO  "/home/correo"
#define MAX_MENS    200            /* mensajes que se muestran en la lista */
#define MARGEM      10
#define ALT_BARRA   40
#define LARG_LISTA  300
#define MAX_TEXTO   (256 * 1024)   /* tamaño máximo de un cuerpo que se muestra */

/* ------------------------------------------------------------- la cuenta */

typedef struct {
    char host[128];
    int  porta;          /* puerto de recepción (POP3 o IMAP) */
    int  porta_envio;    /* puerto de envío (SMTP): no es el mismo */
    char utilizador[128];
    char clave[128];
    char proto[8];    /* "pop3" o "imap" */
    bool tls;
} conta_t;

static conta_t conta;
static bool conta_valida;   /* hay host y usuario */

static bool home_persistente(void)
{
    info_montagem_t m[10];
    int n = info_sistema(INFO_MONTAGENS, m, 10);
    for (int i = 0; i < n && i < 10; i++)
        if (compara_texto(m[i].ponto, "/home") == 0 && !(m[i].opcoes & MONTAR_SO_LEITURA)) return true;
    return false;
}

static void conta_defeito(void)
{
    memset(&conta, 0, sizeof conta);
    copia_texto(conta.proto, "pop3", sizeof conta.proto);
    conta.porta = 110;
    conta.porta_envio = 587;
}

/* Lee clave=valor del fichero de cuenta. Devuelve false si no existe. */
static bool conta_ler(void)
{
    int fd = abrir(ARQ_CONTA, ABRIR_LER);
    if (fd < 0) return false;
    static char t[1024];
    int n = ler(fd, t, sizeof t - 1);
    fechar(fd);
    if (n <= 0) return false;
    t[n] = 0;
    for (char *p = t; *p;) {
        char *fim = acha_char(p, '\n');
        if (fim) *fim = 0;
        char *eq = acha_char(p, '=');
        if (p[0] != '#' && eq) {
            *eq = 0;
            const char *k = p, *v = eq + 1;
            if (compara_texto(k, "host") == 0) copia_texto(conta.host, v, sizeof conta.host);
            else if (compara_texto(k, "porta") == 0) conta.porta = texto_para_int(v);
            else if (compara_texto(k, "porta_envio") == 0) conta.porta_envio = texto_para_int(v);
            else if (compara_texto(k, "utilizador") == 0) copia_texto(conta.utilizador, v, sizeof conta.utilizador);
            else if (compara_texto(k, "clave") == 0) copia_texto(conta.clave, v, sizeof conta.clave);
            else if (compara_texto(k, "proto") == 0) copia_texto(conta.proto, v, sizeof conta.proto);
            else if (compara_texto(k, "tls") == 0) conta.tls = texto_para_int(v) != 0;
        }
        if (!fim) break;
        p = fim + 1;
    }
    return true;
}

/* Crea una carpeta y las de más arriba que falten (como "mkdir -p"). El sistema de
 * archivos no las crea solo: sin disco persistente, /home no existe y hay que
 * hacerlo. Devuelve true si la carpeta queda creada. */
static bool garantir_pasta(const char *caminho)
{
    char t[160];
    copia_texto(t, caminho, sizeof t);
    for (size_t i = 1; i < sizeof t && t[i]; i++) {
        if (t[i] != '/') continue;
        t[i] = 0;
        int r = criar_pasta(t);
        t[i] = '/';
        if (r < 0 && r != ERRO_JA_EXISTE) return false;
    }
    int r = criar_pasta(t);
    return r == 0 || r == ERRO_JA_EXISTE;
}

/* Graba la cuenta. 0 = grabada; 1 = /home no persiste (no se recordará); <0 = error. */
static int conta_gravar(void)
{
    char t[640];
    formatar_texto(t, sizeof t, "host=%s\nporta=%d\nporta_envio=%d\nutilizador=%s\nclave=%s\nproto=%s\ntls=%d\n",
                   conta.host, conta.porta, conta.porta_envio, conta.utilizador, conta.clave,
                   conta.proto, conta.tls ? 1 : 0);
    if (!home_persistente()) return 1;
    if (!garantir_pasta(DIR_CONF)) return -1;
    int fd = abrir(ARQ_CONTA, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return -1;
    int r = escrever_tudo(fd, t, (unsigned)tam_texto(t));
    fechar(fd);
    return r < 0 ? -1 : 0;
}

/* ----------------------------------------------------- los mensajes */

/* Una entrada de la lista: el fichero guardado y el resumen para la lista. */
typedef struct {
    char arquivo[64];     /* nombre dentro de DIR_CORREO */
    char de[64];          /* remitente, en Latin-1 */
    char assunto[96];     /* asunto, en Latin-1 */
    bool anexos;          /* tiene adjuntos (se marca con "*") */
} resumo_t;

static resumo_t resumos[MAX_MENS];
static int n_resumos;
static char  lista_itens[MAX_MENS][64];   /* lo que dibuja lista_t */
static int   sel_mensagem = -1;           /* índice en resumos; -1 = ninguna */
static char *corpo_texto;                 /* texto mostrado (aloca) */

/* Convierte un texto de la red (UTF-8) a Latin-1 para la pantalla, sanea los bytes que
 * no se pueden dibujar y lo corta en max. */
static void para_latin1(char *dst, const char *src, unsigned max)
{
    unsigned n = tam_texto(src);
    if (n >= max) n = max - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
    size_t sobra = 0;
    size_t m = utf8_para_latin1(dst, n, &sobra);
    dst[m] = 0;
    /* Los bytes de control (salvo tab y salto) se cambian por espacio. */
    for (size_t i = 0; i < m; i++) {
        unsigned char c = (unsigned char)dst[i];
        if ((c < 32 && c != '\t' && c != '\n') || c == 127) dst[i] = ' ';
    }
}

/* Nombre sin ruta del fichero de un mensaje: el índice y el momento, para que sea único. */
static void nome_arquivo(char *dst, unsigned max, int i)
{
    formatar_texto(dst, max, "%s/m%04d.eml", DIR_CORREO, i);
}

/* Devuelve el número libre más bajo para un mensaje nuevo guardado. */
static int proximo_numero(void)
{
    entrada_pasta_t *v = NULL;
    int n = listar_tudo(DIR_CORREO, &v);
    int max = 0;
    if (n > 0 && v) {
        for (int i = 0; i < n; i++) {
            /* nombres "mNNNN.eml" */
            if (v[i].nome[0] == 'm') {
                int k = texto_para_int(v[i].nome + 1);
                if (k > max) max = k;
            }
        }
    }
    if (v) libera(v);
    return max + 1;
}

/* Lee un mensaje guardado, lo analiza y rellena el resumen. */
static bool resumo_de(const char *caminho, resumo_t *r)
{
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return false;
    info_arquivo_t info;
    if (info_arquivo(caminho, &info) < 0) { fechar(fd); return false; }
    char *t = aloca(info.tamanho + 1);
    if (!t) { fechar(fd); return false; }
    int n = ler(fd, t, info.tamanho);
    fechar(fd);
    if (n <= 0) { libera(t); return false; }
    t[n] = 0;
    mensagem_t m;
    mensagem_analisar(t, (unsigned)n, &m);
    char de[256], assunto[256];
    para_latin1(de, m.de, sizeof de);
    para_latin1(assunto, m.assunto, sizeof assunto);
    copia_texto(r->de, de, sizeof r->de);
    copia_texto(r->assunto, assunto[0] ? assunto : "(sin asunto)", sizeof r->assunto);
    r->anexos = m.n_anexos > 0;
    mensagem_analisar_liberar(&m);
    libera(t);
    return true;
}

/* Vuelve a leer /home/correo y rellena resumos y la lista. */
static void recarregar_lista(void)
{
    n_resumos = 0;
    entrada_pasta_t *v = NULL;
    int n = listar_tudo(DIR_CORREO, &v);
    if (n > 0 && v) {
        for (int i = 0; i < n && n_resumos < MAX_MENS; i++) {
            if (v[i].tipo == TIPO_ARQUIVO && v[i].nome[0] == 'm') {
                char caminho[160];
                formatar_texto(caminho, sizeof caminho, "%s/%s", DIR_CORREO, v[i].nome);
                copia_texto(resumos[n_resumos].arquivo, v[i].nome, sizeof resumos[n_resumos].arquivo);
                if (resumo_de(caminho, &resumos[n_resumos])) n_resumos++;
            }
        }
    }
    if (v) libera(v);
}

/* Construye los textos de la lista (remitente y asunto, con "*" si hay adjuntos). */
static void montar_itens(void)
{
    for (int i = 0; i < n_resumos && i < MAX_MENS; i++) {
        formatar_texto(lista_itens[i], sizeof lista_itens[i], "%s%s", resumos[i].anexos ? "* " : "  ", resumos[i].assunto);
    }
}

/* Carga el cuerpo del mensaje seleccionado: texto plano, o HTML convertido a texto. */
static void carregar_corpo(int i, char **saida)
{
    libera(*saida);
    *saida = NULL;
    if (i < 0 || i >= n_resumos) return;
    char caminho[160];
    formatar_texto(caminho, sizeof caminho, "%s/%s", DIR_CORREO, resumos[i].arquivo);
    info_arquivo_t info;
    if (info_arquivo(caminho, &info) < 0 || info.tamanho == 0) return;
    unsigned lim = info.tamanho > MAX_TEXTO ? MAX_TEXTO : info.tamanho;
    char *t = aloca(lim + 1);
    if (!t) return;
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) { libera(t); return; }
    int n = ler(fd, t, lim);
    fechar(fd);
    if (n <= 0) { libera(t); return; }
    t[n] = 0;
    mensagem_t m;
    mensagem_analisar(t, (unsigned)n, &m);
    libera(t);
    char *out = NULL;
    unsigned nout = 0;
    if (m.texto) {
        nout = tam_texto(m.texto);
        out = aloca(nout + 1);
        if (out) memcpy(out, m.texto, nout + 1);
    } else if (m.html) {
        unsigned nh = tam_texto(m.html);
        out = aloca(nh + 1);
        if (out) nout = html_para_texto(m.html, nh, out, nh + 1);
    }
    /* Cabeceras visibles delante del cuerpo. */
    char cab[600];
    char de[256], para[256], assunto[256];
    para_latin1(de, m.de, sizeof de);
    para_latin1(para, m.para, sizeof para);
    para_latin1(assunto, m.assunto, sizeof assunto);
    formatar_texto(cab, sizeof cab, "De: %s\nPara: %s\nAsunto: %s\n", de, para, assunto);
    unsigned ncab = tam_texto(cab);
    char *final = aloca(ncab + nout + m.n_anexos * 160 + 64);
    if (final) {
        unsigned o = 0;
        memcpy(final, cab, ncab); o += ncab;
        for (int k = 0; k < m.n_anexos; k++) {
            char a[200];
            char nomeconv[160];
            para_latin1(nomeconv, m.anexos[k].nome, sizeof nomeconv);
            formatar_texto(a, sizeof a, "Adjunto: %s (%u bytes)\n", nomeconv, m.anexos[k].tamanho);
            unsigned na = tam_texto(a);
            memcpy(final + o, a, na); o += na;
        }
        final[o++] = '\n';
        if (out) {
            /* El texto de la red es UTF-8: se pasa a Latin-1 (la pantalla) en el mismo búfer.
             * La conversión solo acorta, así que cabe en "out". */
            size_t sob = 0;
            size_t mm = utf8_para_latin1(out, nout, &sob);
            out[mm] = 0;
            if (o + mm + 1 <= ncab + nout + m.n_anexos * 160 + 64) {
                memcpy(final + o, out, mm);
                o += (unsigned)mm;
            }
        }
        final[o] = 0;
        *saida = final;
    }
    libera(out);
    mensagem_analisar_liberar(&m);
}

/* ------------------------------------------------- la ventana principal */

static jan_t *jan;
static area_t corpo_area;
static lista_t lista;
static botao_t bt_receber, bt_redigir, bt_conta;
static char estado[160] = "Listo";

#define BARRA_Y     0
#define LISTA_X     MARGEM
#define LISTA_Y     (ALT_BARRA + 30)

static void montar_layout(void)
{
    int larg = jan->larg, alt = jan->alt;
    bt_receber = (botao_t){ MARGEM, 8, 110, 26, "Recibir", false };
    bt_redigir = (botao_t){ MARGEM + 120, 8, 110, 26, "Redactar", false };
    bt_conta   = (botao_t){ MARGEM + 240, 8, 110, 26, "Cuenta", false };
    lista.x = LISTA_X;
    lista.y = LISTA_Y;
    lista.l = LARG_LISTA;
    lista.a = alt - LISTA_Y - 30;
    lista.itens = lista_itens;
    lista.qtd = n_resumos;
    lista.icone = NULL;
    area_mudar_tamanho(&corpo_area, larg - LARG_LISTA - 3 * MARGEM, alt - LISTA_Y - 30);
    corpo_area.x = LISTA_X + LARG_LISTA + MARGEM;
    corpo_area.y = LISTA_Y;
}

static void desenhar_janela(void)
{
    g_retangulo(jan, 0, 0, jan->larg, jan->alt, COR_FUNDO);
    botao_desenhar(jan, &bt_receber);
    botao_desenhar(jan, &bt_redigir);
    botao_desenhar(jan, &bt_conta);
    if (!conta_valida) rotulo(jan, 360, 14, "Sin cuenta: pulsa Cuenta para configurarla", COR_TEXTO_FRACO);
    lista.qtd = n_resumos;
    lista_desenhar(jan, &lista);
    area_desenhar(jan, &corpo_area);
    /* Línea de estado al pie. */
    g_retangulo(jan, 0, jan->alt - 24, jan->larg, 24, COR_FUNDO);
    rotulo(jan, MARGEM, jan->alt - 18, estado, COR_TEXTO);
    jan_mostrar(jan);
}

static void definir_estado(const char *t)
{
    copia_texto(estado, t, sizeof estado);
}

/* Muestra el mensaje i en el cuerpo. */
static void mostrar_mensagem(int i)
{
    sel_mensagem = i;
    carregar_corpo(i, &corpo_texto);
    area_definir(&corpo_area, corpo_texto ? corpo_texto : "", corpo_texto ? (int)tam_texto(corpo_texto) : 0);
    corpo_area.somente_leitura = true;
    /* Se anota en el registro del sistema (la serie), que es donde se puede
     * mirar qué se está viendo: la ventana no tiene una barra de estado con
     * el cuerpo entero. */
    if (i >= 0 && i < n_resumos) {
        /* Del texto sale lo que va detrás de las cabeceras ("De:", "Para:", "Asunto:"). */
        const char *c = corpo_texto ? corpo_texto : "";
        for (int k = 0; k < 3; k++) {
            const char *nl = acha_char(c, '\n');
            if (!nl) { c = ""; break; }
            c = nl + 1;
        }
        char pedaco[120];
        size_t n = tam_texto(c);
        if (n > sizeof pedaco - 1) n = sizeof pedaco - 1;
        memcpy(pedaco, c, n);
        pedaco[n] = 0;
        for (size_t i2 = 0; i2 < n; i2++)
            if (pedaco[i2] == '\n' || pedaco[i2] == '\r') pedaco[i2] = ' ';
        registrar("correo: mensaje %d (%s): %s", i + 1, resumos[i].assunto, pedaco);
    }
}

/* --------------------------------------- recibir (POP3 o IMAP) */

/* Guarda cada mensaje recibido en /home/correo y cuenta cuántos llegan. */
static int guardar_mensagem(void *ctx, int i, const char *dados, unsigned n)
{
    int *cont = ctx;
    (void)i;
    int k = proximo_numero();
    char caminho[160];
    nome_arquivo(caminho, sizeof caminho, k);
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return -1;
    int r = escrever_tudo(fd, dados, n);
    fechar(fd);
    if (r < 0) return -1;
    (*cont)++;
    return 0;
}

static void recibir(void)
{
    if (!conta_valida) { definir_estado("Configura la cuenta primero (botón Cuenta)"); return; }
    if (!garantir_pasta(DIR_CORREO)) { definir_estado("No se puede crear la carpeta de los mensajes"); return; }
    buzon_cfg_t b;
    memset(&b, 0, sizeof b);
    b.host = conta.host;
    b.porta = conta.porta;
    b.tls = conta.tls;
    b.usuario = conta.utilizador;
    b.clave = conta.clave;
    static tls_config_t cfg;
    char e[200];
    if (!tls_config_padrao(&cfg, NULL, e, sizeof e)) { definir_estado(e); return; }
    b.cfg = &cfg;
    definir_estado("Conectando...");
    desenhar_janela();
    static char erro[200];
    int guardados = 0;
    int r;
    if (compara_texto(conta.proto, "imap") == 0) r = imap_receber(&b, guardar_mensagem, &guardados, erro, sizeof erro);
    else r = pop3_receber(&b, guardar_mensagem, &guardados, erro, sizeof erro);
    if (r < 0) {
        char t[220];
        formatar_texto(t, sizeof t, "Error al recibir: %s", erro);
        definir_estado(t);
        registrar("correo: error al recibir: %s", erro);
    } else {
        char t[120];
        formatar_texto(t, sizeof t, "Recibidos %d mensajes nuevos", guardados);
        definir_estado(t);
        registrar("correo: recibidos %d mensajes por %s", guardados, conta.proto);
    }
    recarregar_lista();
    montar_itens();
    sel_mensagem = n_resumos ? n_resumos - 1 : -1;
    lista.sel = sel_mensagem;
    if (sel_mensagem >= 0) mostrar_mensagem(sel_mensagem);
    else area_definir(&corpo_area, "", 0);
}

/* --------------------------------------------------- redactar y enviar */

#define MAX_ANEXOS 16
static caixa_t  caixa_para, caixa_assunto;
static area_t   area_corpo_novo;
static botao_t  bt_adjuntar, bt_enviar, bt_cancelar_novo;
static char    *anexos_dados[MAX_ANEXOS];
static unsigned anexos_n[MAX_ANEXOS];
static char     anexos_nome[MAX_ANEXOS][128];
static int      n_anexos;

static void anexos_limpar(void)
{
    for (int i = 0; i < n_anexos; i++) { libera(anexos_dados[i]); anexos_dados[i] = NULL; }
    n_anexos = 0;
}

/* Lee un fichero de la carpeta y lo añade como adjunto. */
static bool anexar(const char *caminho)
{
    if (n_anexos >= MAX_ANEXOS) { mensagem("Adjuntos", "Demasiados adjuntos (máximo 16)."); return false; }
    info_arquivo_t info;
    if (info_arquivo(caminho, &info) < 0) { mensagem("Adjuntos", "No se puede leer el fichero."); return false; }
    if (info.tamanho > 4 * 1024 * 1024) { mensagem("Adjuntos", "El fichero es demasiado grande (máximo 4 MB)."); return false; }
    char *d = aloca(info.tamanho + 1);
    if (!d) { mensagem("Adjuntos", "Sin memoria."); return false; }
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) { libera(d); mensagem("Adjuntos", "No se puede abrir el fichero."); return false; }
    int n = ler(fd, d, info.tamanho);
    fechar(fd);
    if (n < 0) { libera(d); return false; }
    anexos_dados[n_anexos] = d;
    anexos_n[n_anexos] = (unsigned)n;
    /* Nombre: lo último de la ruta. */
    const char *nome = acha_ultimo_char(caminho, '/');
    anexos_nome[n_anexos][0] = 0;
    copia_texto(anexos_nome[n_anexos], nome ? nome + 1 : caminho, sizeof anexos_nome[n_anexos]);
    n_anexos++;
    registrar("correo: adjunto %s (%u bytes)", anexos_nome[n_anexos - 1], anexos_n[n_anexos - 1]);
    return true;
}

static void desenhar_redigir(jan_t *w)
{
    g_retangulo(w, 0, 0, w->larg, w->alt, COR_FUNDO);
    rotulo(w, MARGEM, 12, "Para:", COR_TEXTO);
    caixa_desenhar(w, &caixa_para);
    rotulo(w, MARGEM, 44, "Asunto:", COR_TEXTO);
    caixa_desenhar(w, &caixa_assunto);
    area_desenhar(w, &area_corpo_novo);
    rotulo(w, MARGEM, area_corpo_novo.y + area_corpo_novo.a + 8, "Adjuntos:", COR_TEXTO);
    for (int i = 0; i < n_anexos; i++) {
        char t[160];
        formatar_texto(t, sizeof t, "%s (%u bytes)", anexos_nome[i], anexos_n[i]);
        rotulo(w, MARGEM + 10, area_corpo_novo.y + area_corpo_novo.a + 30 + i * 18, t, COR_TEXTO_FRACO);
    }
    botao_desenhar(w, &bt_adjuntar);
    botao_desenhar(w, &bt_enviar);
    botao_desenhar(w, &bt_cancelar_novo);
    jan_mostrar(w);
}

/* La dirección de remitente: si el usuario ya trae una dirección ("ana@exemplo.org"),
 * se usa esa; si no, se le pega el dominio del servidor ("ana@exemplo.org"), que es lo
 * que aceptan los servidores de verdad (un "MAIL FROM:<ana>" lo rechazan). */
static void conta_remitente(char *dst, unsigned max)
{
    if (acha_char(conta.utilizador, '@')) { copia_texto(dst, conta.utilizador, max); return; }
    /* El dominio: lo que hay detrás de la primera "/" del servidor, o el servidor mismo. */
    const char *p = conta.host;
    const char *barra = acha_char(p, '/');
    if (barra) {
        static char dom[128];
        size_t n = (size_t)(barra - p);
        if (n >= sizeof dom) n = sizeof dom - 1;
        memcpy(dom, p, n);
        dom[n] = 0;
        formatar_texto(dst, max, "%s@%s", conta.utilizador, dom);
        return;
    }
    formatar_texto(dst, max, "%s@%s", conta.utilizador, p);
}

/* Construye el mensaje y lo envía. Devuelve true si el servidor lo aceptó. */
static bool enviar(const char *para, const char *assunto, const char *texto, char *erro, unsigned max)
{
    mensagem_envio_t e;
    memset(&e, 0, sizeof e);
    static char remitente[256];
    conta_remitente(remitente, sizeof remitente);
    e.de = remitente;
    e.para = para;
    e.assunto = assunto;
    e.texto = texto;
    e.n_anexos = n_anexos;
    for (int i = 0; i < n_anexos; i++) {
        e.anexos[i].nome = anexos_nome[i];
        e.anexos[i].dados = (const uint8_t *)anexos_dados[i];
        e.anexos[i].n = anexos_n[i];
    }
    char *msg = NULL;
    unsigned nmsg = 0;
    if (mensagem_construir(&e, &msg, &nmsg) < 0) { copia_texto(erro, "no hay memoria para el mensaje", max); return false; }
    static tls_config_t cfg;
    if (!tls_config_padrao(&cfg, NULL, erro, max)) { libera(msg); return false; }
    smtp_envio_t s;
    memset(&s, 0, sizeof s);
    s.host = conta.host;
    s.porta = conta.porta_envio;
    s.tls = conta.tls;
    s.starttls = !conta.tls;
    s.usuario = conta.utilizador;
    s.clave = conta.clave;
    s.de = remitente;
    s.para = para;
    s.mensaje = msg;
    s.nmensaje = nmsg;
    s.cfg = &cfg;
    int r = smtp_enviar(&s, erro, max);
    libera(msg);
    return r == 0;
}

/* Ventana de redacción: modal (espera a que se cierre). */
static void redigir(void)
{
    if (!conta_valida) { definir_estado("Configura la cuenta primero (botón Cuenta)"); return; }
    jan_t *w = jan_abrir("Redactar", 560, 440);
    if (!w) return;
    jan_estilo(w, JAN_DIALOGO, 0, 0);
    memset(&caixa_para, 0, sizeof caixa_para);
    caixa_para.x = 80; caixa_para.y = 6; caixa_para.l = 460;
    caixa_definir(&caixa_para, "");
    memset(&caixa_assunto, 0, sizeof caixa_assunto);
    caixa_assunto.x = 80; caixa_assunto.y = 38; caixa_assunto.l = 460;
    caixa_definir(&caixa_assunto, "");
    area_iniciar(&area_corpo_novo, MARGEM, 70, 540, 200);
    n_anexos = 0;
    bt_adjuntar = (botao_t){ MARGEM, 380, 110, 26, "Adjuntar", false };
    bt_enviar = (botao_t){ 380, 380, 80, 26, "Enviar", false };
    bt_cancelar_novo = (botao_t){ 470, 380, 80, 26, "Cancelar", false };
    bool enviado = false;
    desenhar_redigir(w);
    evento_t e;
    for (;;) {
        if (proximo_evento(w->id, &e, false) != 1) {
            if (esperar_evento(w->id, &e, 60) != 1) continue;
        }
        if (e.tipo == EV_FECHAR) break;
        if (jan_redimensionar(w, &e)) { desenhar_redigir(w); continue; }
        bool mudou = false;
        mudou |= caixa_tratar(w, &caixa_para, &e);
        mudou |= caixa_tratar(w, &caixa_assunto, &e);
        mudou |= area_tratar(w, &area_corpo_novo, &e);
        if (botao_tratar(w, &bt_adjuntar, &e)) {
            char caminho[256] = "";
            if (pedir_texto("Adjuntar", "Ruta del fichero (ej. /home/doc.txt):", caminho, sizeof caminho) && caminho[0]) {
                anexar(caminho);
            }
            mudou = true;
        }
        if (botao_tratar(w, &bt_cancelar_novo, &e)) break;
        if (botao_tratar(w, &bt_enviar, &e)) {
            if (!caixa_para.texto[0]) {
                mensagem("Enviar", "Falta el destinatario.");
            } else {
                definir_estado("Enviando...");
                desenhar_redigir(w);
                char erro[200];
                bool ok = enviar(caixa_para.texto, caixa_assunto.texto,
                                 area_corpo_novo.buf ? area_corpo_novo.buf : "", erro, sizeof erro);
                if (ok) {
                    definir_estado("Mensaje enviado");
                    registrar("correo: mensaje enviado a %s (asunto %s, %d anexos)",
                              caixa_para.texto, caixa_assunto.texto[0] ? caixa_assunto.texto : "-", n_anexos);
                    enviado = true;
                    break;
                }
                char t[260];
                formatar_texto(t, sizeof t, "Error al enviar: %s", erro);
                definir_estado(t);
                registrar("correo: error al enviar: %s", erro);
                mensagem("Enviar", erro);
            }
            mudou = true;
        }
        if (mudou) desenhar_redigir(w);
    }
    (void)enviado;
    anexos_limpar();
    if (area_corpo_novo.buf) libera(area_corpo_novo.buf);
    area_corpo_novo.buf = NULL;
    fechar_janela(w->id);
    libera(w);
}

/* ---------------------------------------------------------- la cuenta */

static caixa_t  caixa_host, caixa_porta, caixa_porta_envio, caixa_usuario, caixa_clave;
static botao_t  bt_proto, bt_tls, bt_guardar, bt_cancelar_conta;

static void desenhar_conta(jan_t *w)
{
    g_retangulo(w, 0, 0, w->larg, w->alt, COR_FUNDO);
    rotulo(w, MARGEM, 16, "Servidor:", COR_TEXTO);    caixa_desenhar(w, &caixa_host);
    rotulo(w, MARGEM, 48, "Puerto:", COR_TEXTO);      caixa_desenhar(w, &caixa_porta);
    rotulo(w, MARGEM, 80, "Envío:", COR_TEXTO);       caixa_desenhar(w, &caixa_porta_envio);
    rotulo(w, MARGEM, 112, "Usuario:", COR_TEXTO);    caixa_desenhar(w, &caixa_usuario);
    rotulo(w, MARGEM, 144, "Clave:", COR_TEXTO);      caixa_desenhar(w, &caixa_clave);
    rotulo(w, MARGEM, 182, "Protocolo:", COR_TEXTO);  botao_desenhar(w, &bt_proto);
    botao_desenhar(w, &bt_tls);
    botao_desenhar(w, &bt_guardar);
    botao_desenhar(w, &bt_cancelar_conta);
    jan_mostrar(w);
}

static void editar_conta(void)
{
    jan_t *w = jan_abrir("Cuenta de correo", 460, 280);
    if (!w) return;
    jan_estilo(w, JAN_DIALOGO, 0, 0);
    memset(&caixa_host, 0, sizeof caixa_host);
    caixa_host.x = 110; caixa_host.y = 10; caixa_host.l = 330;
    caixa_definir(&caixa_host, conta.host);
    memset(&caixa_porta, 0, sizeof caixa_porta);
    caixa_porta.x = 110; caixa_porta.y = 42; caixa_porta.l = 90;
    char pt[12];
    formatar_texto(pt, sizeof pt, "%d", conta.porta);
    caixa_definir(&caixa_porta, pt);
    memset(&caixa_porta_envio, 0, sizeof caixa_porta_envio);
    caixa_porta_envio.x = 110; caixa_porta_envio.y = 74; caixa_porta_envio.l = 90;
    char pe[12];
    formatar_texto(pe, sizeof pe, "%d", conta.porta_envio);
    caixa_definir(&caixa_porta_envio, pe);
    memset(&caixa_usuario, 0, sizeof caixa_usuario);
    caixa_usuario.x = 110; caixa_usuario.y = 106; caixa_usuario.l = 330;
    caixa_definir(&caixa_usuario, conta.utilizador);
    memset(&caixa_clave, 0, sizeof caixa_clave);
    caixa_clave.x = 110; caixa_clave.y = 138; caixa_clave.l = 330;
    caixa_clave.oculta = true;   /* la clave se ve como asteriscos */
    caixa_definir(&caixa_clave, conta.clave);
    bool imap = compara_texto(conta.proto, "imap") == 0;
    bt_proto = (botao_t){ 110, 170, 110, 24, imap ? "IMAP" : "POP3", false };
    bt_tls = (botao_t){ 230, 170, 90, 24, conta.tls ? "TLS: si" : "TLS: no", false };
    bt_guardar = (botao_t){ 230, 216, 80, 26, "Guardar", false };
    bt_cancelar_conta = (botao_t){ 320, 216, 80, 26, "Cancelar", false };
    desenhar_conta(w);
    evento_t e;
    for (;;) {
        if (proximo_evento(w->id, &e, false) != 1) {
            if (esperar_evento(w->id, &e, 60) != 1) continue;
        }
        if (e.tipo == EV_FECHAR) break;
        bool mudou = false;
        mudou |= caixa_tratar(w, &caixa_host, &e);
        mudou |= caixa_tratar(w, &caixa_porta, &e);
        mudou |= caixa_tratar(w, &caixa_usuario, &e);
        mudou |= caixa_tratar(w, &caixa_clave, &e);
        if (botao_tratar(w, &bt_proto, &e)) {
            bool era_imap = compara_texto(conta.proto, "imap") == 0;
            copia_texto(conta.proto, era_imap ? "pop3" : "imap", sizeof conta.proto);
            bt_proto.texto = era_imap ? "POP3" : "IMAP";
            /* Puerto por defecto del protocolo, si no se ha tocado. */
            char pt2[12];
            formatar_texto(pt2, sizeof pt2, "%d", era_imap ? 143 : 110);
            if (compara_texto(caixa_porta.texto, "110") == 0 || compara_texto(caixa_porta.texto, "143") == 0)
                caixa_definir(&caixa_porta, pt2);
            mudou = true;
        }
        if (botao_tratar(w, &bt_tls, &e)) {
            conta.tls = !conta.tls;
            bt_tls.texto = conta.tls ? "TLS: si" : "TLS: no";
            mudou = true;
        }
        if (botao_tratar(w, &bt_cancelar_conta, &e)) break;
        if (botao_tratar(w, &bt_guardar, &e)) {
            copia_texto(conta.host, caixa_host.texto, sizeof conta.host);
            conta.porta = texto_para_int(caixa_porta.texto);
            conta.porta_envio = texto_para_int(caixa_porta_envio.texto);
            copia_texto(conta.utilizador, caixa_usuario.texto, sizeof conta.utilizador);
            copia_texto(conta.clave, caixa_clave.texto, sizeof conta.clave);
            conta_valida = conta.host[0] && conta.utilizador[0] && conta.porta > 0 && conta.porta_envio > 0;
            int g = conta_gravar();
            if (g == 1) definir_estado("Cuenta guardada, pero /home no es persistente: no se recordará");
            else if (g < 0) definir_estado("No se pudo guardar la cuenta");
            else definir_estado("Cuenta guardada");
            break;
        }
        if (mudou) desenhar_conta(w);
    }
    fechar_janela(w->id);
    libera(w);
}

/* ------------------------------------------------------- argumentos */

static void uso(void)
{
    imprimir("uso: correo [--proto pop3|imap] [--host H] [--porta P] [--porta-envio P]\n"
             "            [--utilizador U] [--clave C] [--tls]\n"
             "  Abre el cliente de correo. Sin argumentos usa la cuenta de /home/.config/correo.\n");
}

/* Lee los argumentos de la línea de comandos sobre la cuenta. Devuelve false si pide ayuda. */
static bool argumentos(int argc, char **argv, bool *tem_argumentos)
{
    *tem_argumentos = false;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "--ajuda") == 0 || compara_texto(a, "-h") == 0) { uso(); return false; }
        if (compara_texto(a, "--tls") == 0) { conta.tls = true; *tem_argumentos = true; continue; }
        if (i + 1 >= argc) continue;
        const char *v = argv[++i];
        *tem_argumentos = true;
        if (compara_texto(a, "--proto") == 0) copia_texto(conta.proto, v, sizeof conta.proto);
        else if (compara_texto(a, "--host") == 0) copia_texto(conta.host, v, sizeof conta.host);
        else if (compara_texto(a, "--porta") == 0) conta.porta = texto_para_int(v);
        else if (compara_texto(a, "--porta-envio") == 0) conta.porta_envio = texto_para_int(v);
        else if (compara_texto(a, "--utilizador") == 0) copia_texto(conta.utilizador, v, sizeof conta.utilizador);
        else if (compara_texto(a, "--clave") == 0) copia_texto(conta.clave, v, sizeof conta.clave);
    }
    return true;
}

/* ------------------------------------------------------------ principal */

int principal(int argc, char **argv)
{
    conta_defeito();
    bool linha_comandos = false;
    if (!argumentos(argc, argv, &linha_comandos)) return 0;

    if (!linha_comandos) conta_ler();
    /* El puerto por defecto depende del protocolo si no se dio uno. */
    if (conta.porta == 0) conta.porta = compara_texto(conta.proto, "imap") == 0 ? 143 : 110;
    if (conta.porta_envio == 0) conta.porta_envio = 587;
    conta_valida = conta.host[0] && conta.utilizador[0];

    jan = jan_abrir("Correo", 820, 540);
    if (!jan) return 1;
    jan_estilo(jan, JAN_REDIMENSIONAVEL, 560, 360);
    if (!area_iniciar(&corpo_area, 0, 0, 100, 100)) return 1;
    corpo_area.somente_leitura = true;
    lista.sel = -1;
    lista.topo = 0;

    recarregar_lista();
    montar_itens();
    montar_layout();
    if (n_resumos) {
        sel_mensagem = n_resumos - 1;
        lista.sel = sel_mensagem;
        mostrar_mensagem(sel_mensagem);
    }
    if (!conta_valida) {
        definir_estado("Sin cuenta: pulsa Cuenta para configurarla");
    }
    desenhar_janela();

    /* Si no hay cuenta, se abre la ventana de cuenta de entrada. */
    if (!conta_valida) editar_conta();

    evento_t e;
    for (;;) {
        if (proximo_evento(jan->id, &e, false) != 1) {
            if (esperar_evento(jan->id, &e, 200) != 1) continue;
        }
        if (e.tipo == EV_FECHAR) break;
        if (jan_redimensionar(jan, &e)) {
            montar_layout();
            desenhar_janela();
            continue;
        }
        bool mudou = false;
        if (botao_tratar(jan, &bt_receber, &e)) { recibir(); mudou = true; }
        if (botao_tratar(jan, &bt_redigir, &e)) { redigir(); mudou = true; }
        if (botao_tratar(jan, &bt_conta, &e)) { editar_conta(); mudou = true; }
        int sel = lista_tratar(jan, &lista, &e);
        if (sel == LISTA_SELECIONOU || sel == LISTA_ATIVOU) {
            if (lista.sel >= 0 && lista.sel < n_resumos) mostrar_mensagem(lista.sel);
            mudou = true;
        }
        if (area_tratar(jan, &corpo_area, &e)) mudou = true;
        if (mudou) desenhar_janela();
    }
    if (corpo_texto) libera(corpo_texto);
    anexos_limpar();
    fechar_janela(jan->id);
    libera(jan);
    return 0;
}
