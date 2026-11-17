// SPDX-License-Identifier: GPL-2.0-or-later
/* sessao.c - inicio de sesión gráfico, cerrar sesión y cambiar de usuario (D340, mejora 098, fase 3).
 *
 * Forma parte del gestor de ventanas (el componente de confianza, como el diálogo de permisos de
 * D269): la pantalla cubre todo el escritorio, se queda con el teclado y el ratón, y la contraseña
 * no sale de aquí más que hacia el gestor de servicios (supervisor.c la borra al entregarla). Nunca
 * se escribe en el registro ni en la serie.
 *
 * Cuentas: las de /etc/usuarios con uid de persona (1000 a 59999) y un shell. Si la cuenta no tiene
 * contraseña (la sombra no tiene un hash válido), entra sin preguntar al supervisor, como antes; si
 * la línea es «!», no aparece. Si /etc/usuarios falta o está dañado, queda al menos sagui (D255).
 *
 * Arranque: si solo hay una cuenta y no tiene contraseña, se entra directo (ni pantalla ni espera).
 * Cerrar sesión: una tarea del kernel cierra todos los programas de la sesión local (no los servicios
 * ni el supervisor) y la pantalla vuelve a aparecer. Lo que lee o escribe discos va en esa tarea, no
 * en el gestor de ventanas. */
#include "sessao.h"
#include "abi.h"
#include "acpi.h"
#include "aparencia.h"
#include "arte.h"
#include "cpu.h"
#include "console.h"
#include "gui.h"
#include "idioma.h"
#include "montagem.h"
#include "mouse.h"
#include "permisos.h"
#include "processo.h"
#include "relogio.h"
#include "supervisor.h"
#include "teclado.h"
#include "telas.h"
#include "texto.h"
#include "vfs.h"
#include "video.h"
#include "memoria.h"
#include "versao.h"

#define CONTAS_TELA 8
#define PW 460
#define COR_AZUL  0x1F5FAF
#define COR_PAINEL 0xF2F4F7

typedef struct {
    char     nome[32];
    char     descr[48];
    uint32_t uid, gid;
    char     casa[40];   /* "" = /home tal cual (la cuenta principal) */
    bool     senha;
} conta_tela_t;

static conta_tela_t contas[CONTAS_TELA];
static int n_contas, sel;
enum { S_INATIVA, S_TELA, S_FECHANDO, S_ENTRANDO };
static volatile int estado = S_INATIVA;
static char senha[PED_SENHA_MAX];
static int  slen;
static char msg[96];
static uint32_t pedido;            /* pedido al supervisor en curso (0 = ninguno) */
static int  pedido_conta;
static uint32_t pedido_desde;
static conta_tela_t entrando;      /* la cuenta que está entrando (la usa la tarea) */
static volatile bool fim_pronto, inicio_pronto, inicio_pendente, restaurar_pendente;
static volatile bool inicio_negado;   /* D371: la carpeta personal no era de la cuenta */
static int  modo_saida;
static char anterior[32];          /* la cuenta que cerró la sesión: la pantalla empieza en ella (o en otra) */

/* ---------------------------------------------------------------- cuentas */

static bool nome_ok(const char *n, size_t k)
{
    if (!k || k >= 32) return false;
    for (size_t i = 0; i < k; i++)
        if (!((n[i] >= 'a' && n[i] <= 'z') || (n[i] >= '0' && n[i] <= '9') || n[i] == '_' || n[i] == '-')) return false;
    return true;
}

static uint32_t numero(const char *s, size_t k, bool *ok)
{
    uint32_t v = 0;
    *ok = k > 0 && k <= 6;
    for (size_t i = 0; i < k && *ok; i++) {
        if (s[i] < '0' || s[i] > '9') *ok = false;
        else v = v * 10 + (uint32_t)(s[i] - '0');
    }
    return v;
}

/* Corta la línea [l, fim) en campos separados por ':'. */
static int campos(const char *l, const char *fim, const char **c, size_t *k, int max)
{
    int n = 0;
    const char *ini = l;
    for (const char *p = l; p <= fim && n < max; p++)
        if (p == fim || *p == ':') { c[n] = ini; k[n] = (size_t)(p - ini); n++; ini = p + 1; }
    return n;
}

/* ¿Tiene contraseña la cuenta? Igual que el supervisor: solo un hash pbkdf2 cuenta; «!» = no entra. */
static int senha_de(const uint8_t *s, uint32_t tam, const char *nome)
{
    if (!s) return 0;
    size_t kn = txt_tamanho(nome);
    const char *p = (const char *)s, *fim = p + tam;
    while (p < fim) {
        const char *l = p, *le = p;
        while (le < fim && *le != '\n') le++;
        p = le < fim ? le + 1 : le;
        if (le > l && le[-1] == '\r') le--;
        if ((size_t)(le - l) <= kn || l[kn] != ':' || txt_compara_n(l, nome, kn) != 0) continue;
        const char *h = l + kn + 1;
        size_t kh = (size_t)(le - h);
        if (kh == 1 && *h == '!') return -1;
        return kh > 14 && txt_compara_n(h, "pbkdf2-sha256$", 14) == 0;
    }
    return 0;
}

static void carregar_contas(void)
{
    n_contas = 0;
    uint8_t *d = NULL, *s = NULL;
    uint32_t tam = 0, ts = 0;
    if (vfs_ler_inteiro(CONTAS_ARQ_USUARIOS, &d, &tam) < 0) d = NULL;
    if (vfs_ler_inteiro(CONTAS_ARQ_SOMBRA, &s, &ts) < 0) s = NULL;
    bool tem_sagui = false;
    const char *p = d ? (const char *)d : "", *fim = p + (d ? tam : 0);
    while (p < fim && n_contas < CONTAS_TELA) {
        const char *l = p, *le = p;
        while (le < fim && *le != '\n') le++;
        p = le < fim ? le + 1 : le;
        if (le > l && le[-1] == '\r') le--;
        if (l >= le || *l == '#') continue;
        const char *c[6];
        size_t k[6] = { 0 };
        int nc = campos(l, le, c, k, 6);
        if (nc < 5) continue;
        bool ok1, ok2;
        uint32_t uid = numero(c[1], k[1], &ok1), gid = numero(c[2], k[2], &ok2);
        if (!ok1 || !ok2 || !nome_ok(c[0], k[0]) || uid < UID_SAGUI || uid >= 60000 || gid > UID_MAX || !k[4]) continue;
        bool rep = false;
        for (int i = 0; i < n_contas; i++) rep |= contas[i].uid == uid;
        if (rep) continue;
        conta_tela_t *x = &contas[n_contas];
        memset(x, 0, sizeof *x);
        memcpy(x->nome, c[0], k[0]);
        if (nc == 6) memcpy(x->descr, c[5], k[5] < sizeof x->descr - 1 ? k[5] : sizeof x->descr - 1);
        x->uid = uid;
        x->gid = gid;
        /* La carpeta: /home solo para la cuenta principal (uid 1000); las demás, /home/<nombre>. */
        if (uid == UID_SAGUI && k[3] == 5 && txt_compara_n(c[3], "/home", 5) == 0) x->casa[0] = 0;
        else {
            char casa[40];
            size_t kc = k[3] < sizeof casa - 1 ? k[3] : sizeof casa - 1;
            memcpy(casa, c[3], kc);
            casa[kc] = 0;
            if (kc && kc == k[3] && permisos_casa_valida(casa) && casa[0]) txt_copia_n(x->casa, casa, sizeof x->casa);
            else if (uid == UID_SAGUI) x->casa[0] = 0;
            else snformatar(x->casa, sizeof x->casa, "/home/%s", x->nome);
        }
        int sh = senha_de(s, ts, x->nome);
        if (sh < 0) continue;   /* «!»: no entra con contraseña */
        x->senha = sh > 0;
        if (uid == UID_SAGUI) tem_sagui = true;
        n_contas++;
    }
    if (!tem_sagui && n_contas < CONTAS_TELA) {   /* D255: sagui existe siempre */
        int sh = senha_de(s, ts, "sagui");
        if (sh >= 0) {
            conta_tela_t *x = &contas[n_contas++];
            memset(x, 0, sizeof *x);
            txt_copia_n(x->nome, "sagui", sizeof x->nome);
            x->uid = UID_SAGUI;
            x->gid = GID_SAGUI;
            x->senha = sh > 0;
        }
    }
    if (d) kfree(d);
    if (s) kfree(s);
    sel = 0;
    for (int i = 0; i < n_contas; i++)
        if (anterior[0] && txt_compara(contas[i].nome, anterior) == 0)
            sel = modo_saida == USU_SAIR_CAMBIAR && n_contas > 1 ? (i + 1) % n_contas : i;
}

/* ---------------------------------------------------------------- cerrar los programas de la sesión */

static bool da_sessao(const processo_t *p)
{
    if (p->estado == P_LIVRE || p->estado == P_ZUMBI || p->lider || !p->usuario || p->pid == 0) return false;
    if (p->de_servico || p->exe_sistema == EXE_SERVICIOS) return false;   /* servicios y el supervisor */
    return p->pid != supervisor_pid_confiavel();
}

static int matar_sessao(void)
{
    int n = 0;
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *p = processo_indice(i);
        if (!p || !da_sessao(p)) continue;
        bool i0 = int_salva_desliga();
        if (da_sessao(p)) { processo_encerrar(p, SAIDA_ENCERRADO_PELO_KERNEL, "fin de la sesion"); n++; }
        int_restaura(i0);
    }
    return n;
}

static void matar_tudo(void)
{
    int total = 0;
    for (int k = 0; k < 30; k++) {   /* uno que lanzaba otro mientras tanto: otra vuelta (3 s como mucho) */
        int n = matar_sessao();
        total += n;
        if (!n) break;
        relogio_dormir_ms(100);
    }
    klog("[SESION] %d programa(s) de la sesion cerrados\n", total);
}

/* ---------------------------------------------------------------- la carpeta personal */

static int criar_pasta_k(const char *c)
{
    const char *rel;
    montagem_t *m = montagem_de(c, &rel);
    if (m) {
        if (!*rel) return ERRO_JA_EXISTE;
        if (m->opcoes & MONTAR_SO_LEITURA) return ERRO_SO_LEITURA;
        return m->fs->criar_pasta(m->priv, rel);
    }
    return vfs_criar(c, TIPO_PASTA, NULL);
}

/* La carpeta de una cuenta que no es la principal: si falta (un /home en la RAM, o se borró), se crea
 * de la cuenta y 0700 (D282), con su .config. Si existe, no se toca.
 * D371: false si la carpeta no es de la cuenta o la pueden cambiar otros (grupo o demás con «w»), o si el
 * sistema de archivos no guarda dueños: /home es de sagui y sagui pudo dejar ahí una carpeta suya con ese
 * nombre (un /home en la RAM pierde las carpetas al reiniciar) con su configuración dentro (órdenes de
 * widgets, conky, servicios...). Entrar ahí sería correr como la cuenta lo que otro preparó. */
static bool preparar_casa(const conta_tela_t *c)
{
    if (!c->casa[0]) return true;
    meta_t m;
    if (vfs_meta(c->casa, &m) != 0) {
        int r = criar_pasta_k(c->casa);
        if (r == ERRO_NAO_EXISTE && criar_pasta_k("/home") >= 0)   /* sin partición /home, en la RAM aún no hay /home */
            r = criar_pasta_k(c->casa);
        if (r >= 0) {
            vfs_fixar_meta(c->casa, (int32_t)c->uid, (int32_t)c->gid, 0700);
            char cfg[CAMINHO_MAX];
            snformatar(cfg, sizeof cfg, "%s/.config", c->casa);
            if (criar_pasta_k(cfg) >= 0) vfs_fixar_meta(cfg, (int32_t)c->uid, (int32_t)c->gid, 0700);
        }
        klog("[SESION] carpeta %s creada para %s: %d\n", c->casa, c->nome, r);
        if (vfs_meta(c->casa, &m) != 0) return false;
    }
    if (m.tipo != TIPO_PASTA || !m.com_donos || m.uid != c->uid || (m.modo & 022)) {
        klog("[SESION] %s no es una carpeta de %s (uid %u, modo 0x%x): no se entra\n", c->casa, c->nome, m.uid,
             (unsigned)m.modo);
        return false;
    }
    return true;
}

/* ---------------------------------------------------------------- la tarea del kernel */

#define T_FIM    1
#define T_INICIO 2

static void tarefa(void *arg)
{
    int modo = (int)(uintptr_t)arg;
    matar_tudo();
    if (modo == T_FIM) {
        permisos_fixar_sessao(UID_SAGUI, GID_SAGUI, "");   /* detrás de la pantalla, la configuración de la máquina */
        fim_pronto = true;
        return;
    }
    if (!preparar_casa(&entrando)) {   /* D371: la sesión sigue siendo la de detrás de la pantalla */
        inicio_negado = true;
        return;
    }
    permisos_fixar_sessao(entrando.uid, entrando.gid, entrando.casa);
    klog("[SESION] entra %s (uid %u)\n", entrando.nome, entrando.uid);
    inicio_pronto = true;
}

/* ---------------------------------------------------------------- API */

bool sessao_ativa(void) { return estado != S_INATIVA; }

static void tela_toda(void) { gui_redesenhar(0, 0, (int)video_largura(), (int)video_altura()); }

static void limpar_senha(void)
{
    memset(senha, 0, sizeof senha);
    slen = 0;
}

static void abrir_tela(void)
{
    carregar_contas();
    limpar_senha();
    msg[0] = 0;
    pedido = 0;
    estado = S_TELA;
    klog("[SESION] pantalla de inicio de sesion: %d cuenta(s)\n", n_contas);
    tela_toda();
}

bool sessao_arrancar(void)
{
    if (permisos_modo_rescate() || permisos_sesion_raiz()) return true;   /* rescate y pruebas: como siempre */
    carregar_contas();
    if (n_contas == 1 && !contas[0].senha) {
        if (contas[0].uid != UID_SAGUI || contas[0].casa[0]) {
            if (!preparar_casa(&contas[0])) {   /* D371: no se entra en una carpeta que no es suya */
                abrir_tela();
                txt_copia_n(msg, _("Tu carpeta personal no es tuya o otros la pueden cambiar: avisa al administrador."), sizeof msg);
                return false;
            }
            permisos_fixar_sessao(contas[0].uid, contas[0].gid, contas[0].casa);
        }
        klog("[SESION] una sola cuenta sin contrasena (%s): entrada directa\n", contas[0].nome);
        return true;
    }
    abrir_tela();
    return false;
}

bool sessao_tomar_inicio(void)
{
    if (!inicio_pendente) return false;
    inicio_pendente = false;
    return true;
}

bool sessao_tomar_restaurar(void)
{
    if (!restaurar_pendente) return false;
    restaurar_pendente = false;
    return true;
}

int sessao_pedir_fim(int modo, int pid)
{
    if (!gui_ativa()) return ERRO_NAO_SUPORTADO;
    if (estado != S_INATIVA) return ERRO_OCUPADO;
    modo_saida = modo;
    conta_tela_t *c = NULL;
    for (int i = 0; i < n_contas; i++) if (contas[i].uid == permisos_uid_inicial()) c = &contas[i];
    txt_copia_n(anterior, c ? c->nome : "", sizeof anterior);
    klog("[SESION] %s pedido por pid %d\n", modo == USU_SAIR_CAMBIAR ? "cambiar de usuario" : "cerrar sesion", pid);
    estado = S_FECHANDO;
    tela_toda();
    if (!tarefa_kernel("sesion", tarefa, (void *)(uintptr_t)T_FIM)) { estado = S_INATIVA; tela_toda(); return ERRO_SEM_MEMORIA; }
    return 0;
}

static void entrar(int i)
{
    conta_tela_t *c = &contas[i];
    entrando = *c;
    limpar_senha();
    msg[0] = 0;
    estado = S_ENTRANDO;
    tela_toda();
    if (!tarefa_kernel("sesion", tarefa, (void *)(uintptr_t)T_INICIO)) {
        estado = S_TELA;
        txt_copia_n(msg, _("No se pudo iniciar la sesión."), sizeof msg);
        tela_toda();
    }
}

static void tentar(void)
{
    if (pedido || estado != S_TELA || !n_contas) return;
    conta_tela_t *c = &contas[sel];
    if (!c->senha) { entrar(sel); return; }   /* sin contraseña: como antes, sin preguntar a nadie */
    if (!slen) { txt_copia_n(msg, _("Escribe la contraseña."), sizeof msg); tela_toda(); return; }
    int r = supervisor_entrar_pedir(c->uid, senha);
    limpar_senha();
    if (r <= 0) {
        txt_copia_n(msg, r == ERRO_NAO_EXISTE ? _("No se puede comprobar la contraseña: el gestor de servicios no responde.")
                                              : _("No se pudo comprobar la contraseña."), sizeof msg);
        tela_toda();
        return;
    }
    pedido = (uint32_t)r;
    pedido_conta = sel;
    pedido_desde = relogio_tiques();
    msg[0] = 0;
    tela_toda();
}

/* D361: el nombre de la cuenta de la sesión (para la cabecera del menú Inicio). */
const char *sessao_utilizador(void) { return anterior[0] ? anterior : "sagui"; }

void sessao_tique(void)
{
    if (fim_pronto) {
        fim_pronto = false;
        /* Lo de la sesión que vive en el gestor de ventanas vuelve a lo de serie. */
        aparencia_padrao();
        barra_conf_padrao();   /* D361 */
        area_fundo_padrao();
        barra_sessao_nova();
        area_sessao_nova();
        restaurar_pendente = true;
        abrir_tela();
        return;
    }
    if (inicio_negado) {
        inicio_negado = false;
        estado = S_TELA;
        txt_copia_n(msg, _("Tu carpeta personal no es tuya o otros la pueden cambiar: avisa al administrador."), sizeof msg);
        tela_toda();
        return;
    }
    if (inicio_pronto) {
        inicio_pronto = false;
        txt_copia_n(anterior, entrando.nome, sizeof anterior);
        barra_sessao_nova();
        area_sessao_nova();
        estado = S_INATIVA;
        inicio_pendente = true;
        tela_toda();
        return;
    }
    if (estado != S_TELA || !pedido) return;
    int r = supervisor_entrar_resultado(pedido);
    if (r == 0) {
        if (relogio_tiques() - pedido_desde > 30 * TIQUES_POR_SEGUNDO) {   /* el supervisor no contesta */
            supervisor_entrar_cancelar(pedido);
            pedido = 0;
            txt_copia_n(msg, _("No se puede comprobar la contraseña: el gestor de servicios no responde."), sizeof msg);
            tela_toda();
        }
        return;
    }
    pedido = 0;
    if (r == 1) { entrar(pedido_conta); return; }
    if (r == ERRO_ACESSO) txt_copia_n(msg, _("Contraseña incorrecta. Inténtalo otra vez."), sizeof msg);
    else if (r == ERRO_ESPERA) txt_copia_n(msg, _("Demasiados fallos: espera un minuto para volver a intentarlo."), sizeof msg);
    else txt_copia_n(msg, _("No se pudo comprobar la contraseña."), sizeof msg);
    tela_toda();
}

/* ---------------------------------------------------------------- dibujo y eventos */

typedef struct { int x, y, l, a; } ret_t;
typedef struct { ret_t caixa, linha[CONTAS_TELA], campo, entrar, apagar, reiniciar; int y_rotulo, y_msg; } lay_t;

static void layout(lay_t *L)
{
    int sw, sh;
    telas_principal_rect(&sw, &sh);
    int h = 30 + 40 + n_contas * 40 + 12 + 46 + 28 + 56;
    L->caixa = (ret_t){ (sw - PW) / 2, (sh - h) / 2, PW, h };
    int y = L->caixa.y + 30 + 40;
    for (int i = 0; i < n_contas; i++) { L->linha[i] = (ret_t){ L->caixa.x + 20, y, PW - 40, 36 }; y += 40; }
    y += 12;
    L->y_rotulo = y;
    L->campo = (ret_t){ L->caixa.x + 20, y + 20, PW - 40, 24 };
    L->y_msg = y + 52;
    int yb = L->caixa.y + h - 44;
    L->entrar = (ret_t){ L->caixa.x + PW - 20 - 120, yb, 120, 30 };
    L->apagar = (ret_t){ L->caixa.x + 20, yb, 96, 30 };
    L->reiniciar = (ret_t){ L->caixa.x + 20 + 104, yb, 104, 30 };
}

static bool dentro(ret_t r, int x, int y) { return x >= r.x && x < r.x + r.l && y >= r.y && y < r.y + r.a; }

static void botao(superficie_t *t, ret_t r, const char *txt, uint32_t cor)
{
    sup_retangulo(t, r.x, r.y, r.l, r.a, cor);
    sup_moldura(t, r.x, r.y, r.l, r.a, 0x101820);
    sup_texto(t, r.x + (r.l - texto_largura(txt)) / 2, r.y + (r.a - 16) / 2, txt, 0xFFFFFF);
}

void sessao_desenhar(superficie_t *t)
{
    if (estado == S_INATIVA) return;
    /* Cubre todo (también la barra superior): detrás no se ve nada de la sesión. */
    uint32_t fundo = aparencia_escurecer(COR_AZUL, 55);
    sup_retangulo(t, 0, 0, t->larg, t->alt, fundo);
    int sw, sh;
    telas_principal_rect(&sw, &sh);
    sup_texto(t, 24, 20, SAGUI_NOME, 0xFFFFFF);
    sup_texto(t, 25, 20, SAGUI_NOME, 0xFFFFFF);
    if (estado == S_FECHANDO || estado == S_ENTRANDO) {
        const char *m = estado == S_FECHANDO ? _("Cerrando la sesión...") : _("Iniciando la sesión...");
        sup_texto(t, (sw - texto_largura(m)) / 2, sh / 2 - 8, m, 0xFFFFFF);
        return;
    }
    lay_t L;
    layout(&L);
    ret_t c = L.caixa;
    sup_retangulo(t, c.x, c.y, c.l, c.a, COR_PAINEL);
    sup_moldura(t, c.x - 1, c.y - 1, c.l + 2, c.a + 2, 0x000000);
    sup_retangulo(t, c.x, c.y, c.l, 30, COR_AZUL);
    sup_texto(t, c.x + 14, c.y + 7, _("Iniciar sesión"), 0xFFFFFF);
    sup_texto(t, c.x + 20, c.y + 30 + 14, _("Elige tu cuenta:"), 0x101820);
    for (int i = 0; i < n_contas; i++) {
        ret_t r = L.linha[i];
        bool s = i == sel;
        sup_retangulo(t, r.x, r.y, r.l, r.a, s ? 0xD6E4F5 : 0xFFFFFF);
        sup_moldura(t, r.x, r.y, r.l, r.a, s ? COR_AZUL : 0xC0C6CE);
        sup_desenho(t, r.x + 6, r.y + 2, &arte_sagui, 1);
        sup_texto(t, r.x + 48, r.y + 2, contas[i].nome, 0x101820);
        sup_texto(t, r.x + 49, r.y + 2, contas[i].nome, 0x101820);
        const char *d = contas[i].descr[0] ? contas[i].descr : "";
        sup_texto(t, r.x + 48, r.y + 18, d, 0x5A6470);
        if (!contas[i].senha) {
            const char *sc = _("sin contraseña");
            sup_texto(t, r.x + r.l - 8 - texto_largura(sc), r.y + 2, sc, 0x5A6470);
        }
    }
    if (n_contas && contas[sel].senha) {
        sup_texto(t, c.x + 20, L.y_rotulo, _("Contraseña:"), 0x101820);
        ret_t f = L.campo;
        sup_retangulo(t, f.x, f.y, f.l, f.a, 0xFFFFFF);
        sup_moldura(t, f.x, f.y, f.l, f.a, pedido ? 0x808890 : COR_AZUL);
        char pts[PED_SENHA_MAX];
        int n = slen, max = (f.l - 12) / 8;
        if (n > max) n = max;
        for (int i = 0; i < n; i++) pts[i] = '*';
        pts[n] = 0;
        sup_texto(t, f.x + 6, f.y + 4, pts, 0x101820);
        if (!pedido) sup_retangulo(t, f.x + 6 + n * 8, f.y + 4, 2, 16, 0x101820);
    } else if (n_contas) {
        sup_texto(t, c.x + 20, L.y_rotulo + 24, _("Esta cuenta no tiene contraseña: pulsa Entrar."), 0x303840);
    }
    if (pedido) sup_texto(t, c.x + 20, L.y_msg, _("Comprobando..."), COR_AZUL);
    else if (msg[0]) sup_texto(t, c.x + 20, L.y_msg, msg, 0xC0261B);
    botao(t, L.entrar, _("Entrar"), pedido ? 0x9AA3AE : 0x2E7D32);
    botao(t, L.apagar, _("Apagar"), 0x5A6470);
    botao(t, L.reiniciar, _("Reiniciar"), 0x5A6470);
}

static void escolher(int i)
{
    if (pedido || i < 0 || i >= n_contas || i == sel) return;
    sel = i;
    limpar_senha();
    msg[0] = 0;
    tela_toda();
}

bool sessao_tecla(int t)
{
    if (estado == S_INATIVA) return false;
    if (estado != S_TELA || pedido) return true;   /* todo el teclado es suyo */
    if (t == '\n' || t == '\r') { tentar(); return true; }
    if (t == TECLA_ESC) { limpar_senha(); msg[0] = 0; tela_toda(); return true; }
    if (t == TECLA_CIMA) { escolher(sel > 0 ? sel - 1 : n_contas - 1); return true; }
    if (t == TECLA_BAIXO || t == '\t') { escolher(n_contas ? (sel + 1) % n_contas : 0); return true; }
    if (t == 8 || t == 127) { if (slen > 0) senha[--slen] = 0; tela_toda(); return true; }
    if (t >= 32 && t < 256 && t != 127 && slen < PED_SENHA_MAX - 1 && n_contas && contas[sel].senha) {
        senha[slen++] = (char)t;
        tela_toda();
    }
    return true;
}

bool sessao_mouse(int x, int y, int botoes, int mudaram)
{
    if (estado == S_INATIVA) return false;
    if (estado != S_TELA || !(mudaram & botoes & BOTAO_ESQUERDO)) return true;
    lay_t L;
    layout(&L);
    if (dentro(L.entrar, x, y)) tentar();
    else if (dentro(L.apagar, x, y) && !pedido) { klog("[SESION] apagar desde la pantalla de inicio de sesion\n"); acpi_pedir_apagado(); }
    else if (dentro(L.reiniciar, x, y) && !pedido) { klog("[SESION] reiniciar desde la pantalla de inicio de sesion\n"); acpi_pedir_reinicio(); }
    else for (int i = 0; i < n_contas; i++) if (dentro(L.linha[i], x, y)) escolher(i);
    return true;
}
