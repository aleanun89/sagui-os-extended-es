// SPDX-License-Identifier: GPL-2.0-or-later
/* contas.c - las cuentas de usuario: /etc/usuarios y la sombra (D255). Ver contas.h. */
#include "sagui.h"
#include "contas.h"

#define ARQ_MAX 16384   /* las dos tablas son pequeñas: más que esto es un archivo dañado */

/* Las cuentas de serie: valen siempre que el archivo falte o no las tenga. */
static const conta_t serie[] = {
    { "root", UID_ROOT, GID_ROOT, "/home", "/bin/sagsh", "Administrador" },
    { "sagui", UID_SAGUI, GID_SAGUI, "/home", "/bin/sagsh", "Usuario del escritorio" },
    { "servicio", 100, 100, "/", "", "Servicios sin escritorio" },
};
#define N_SERIE ((int)(sizeof serie / sizeof serie[0]))

/* Lee un archivo entero (terminado en cero). NULL si no existe o no se puede. */
static char *ler_tudo(const char *arq, int *erro)
{
    int fd = abrir(arq, ABRIR_LER);
    if (fd < 0) { if (erro) *erro = fd; return NULL; }
    char *b = aloca(ARQ_MAX + 1);
    if (!b) { fechar(fd); if (erro) *erro = ERRO_SEM_MEMORIA; return NULL; }
    int n = 0, r;
    while (n < ARQ_MAX && (r = ler(fd, b + n, (unsigned)(ARQ_MAX - n))) > 0) n += r;
    fechar(fd);
    b[n] = 0;
    if (erro) *erro = 0;
    return b;
}

/* Corta "s" en campos separados por ':' (sin copiar). Devuelve cuántos. */
static int campos(char *s, char **c, int max)
{
    int n = 0;
    c[n++] = s;
    for (; *s && n < max; s++)
        if (*s == ':') { *s = 0; c[n++] = s + 1; }
    return n;
}

static bool numero(const char *s, unsigned *v)
{
    unsigned x = 0;
    if (!*s) return false;
    for (; *s; s++) {
        if (*s < '0' || *s > '9' || x > UID_MAX) return false;
        x = x * 10 + (unsigned)(*s - '0');
    }
    if (x > UID_MAX) return false;
    *v = x;
    return true;
}

static bool nome_valido(const char *n)
{
    if (!*n || tam_texto(n) >= CONTA_NOME_MAX) return false;
    for (; *n; n++)
        if (!((*n >= 'a' && *n <= 'z') || (*n >= '0' && *n <= '9') || *n == '_' || *n == '-')) return false;
    return true;
}

static int achar_uid(const conta_t *v, int n, unsigned uid)
{
    for (int i = 0; i < n; i++) if (v[i].uid == uid) return i;
    return -1;
}

static int achar_nome(const conta_t *v, int n, const char *nome)
{
    for (int i = 0; i < n; i++) if (compara_texto(v[i].nome, nome) == 0) return i;
    return -1;
}

int contas_ler(conta_t *v, int max, bool *danado)
{
    int n = 0;
    bool mal = false;
    char *b = ler_tudo(CONTAS_ARQ_USUARIOS, NULL);
    if (!b) mal = true;
    for (char *l = b; l && *l && n < max;) {
        char *fim = l;
        while (*fim && *fim != '\n') fim++;
        char *prox = *fim ? fim + 1 : fim;
        *fim = 0;
        if (fim > l && fim[-1] == '\r') fim[-1] = 0;
        while (*l == ' ' || *l == '\t') l++;
        if (*l && *l != '#') {
            char *c[6];
            int k = campos(l, c, 6);
            conta_t x;
            memset(&x, 0, sizeof x);
            if (k >= 3 && nome_valido(c[0]) && numero(c[1], &x.uid) && numero(c[2], &x.gid) &&
                achar_uid(v, n, x.uid) < 0 && achar_nome(v, n, c[0]) < 0 &&
                /* root es el 0 y el 0 es root: nada de otro nombre con uid 0 */
                ((x.uid == 0) == (compara_texto(c[0], "root") == 0))) {
                copia_texto(x.nome, c[0], sizeof x.nome);
                copia_texto(x.casa, k > 3 && c[3][0] ? c[3] : "/home", sizeof x.casa);
                copia_texto(x.shell, k > 4 ? c[4] : "", sizeof x.shell);
                copia_texto(x.descricao, k > 5 ? c[5] : "", sizeof x.descricao);
                v[n++] = x;
            } else {
                mal = true;
            }
        }
        l = prox;
    }
    if (b) libera(b);
    /* Las de serie que falten (por nombre y por uid). */
    for (int i = 0; i < N_SERIE && n < max; i++)
        if (achar_nome(v, n, serie[i].nome) < 0 && achar_uid(v, n, serie[i].uid) < 0) {
            v[n++] = serie[i];
            if (i < 2) mal = true;   /* root o sagui faltaban: el archivo no está bien */
        }
    if (danado) *danado = mal;
    return n;
}

bool conta_por_uid(unsigned uid, conta_t *c)
{
    conta_t v[CONTAS_MAX];
    int n = contas_ler(v, CONTAS_MAX, NULL);
    int i = achar_uid(v, n, uid);
    if (i < 0) return false;
    *c = v[i];
    return true;
}

bool conta_por_nome(const char *nome, conta_t *c)
{
    conta_t v[CONTAS_MAX];
    int n = contas_ler(v, CONTAS_MAX, NULL);
    int i = achar_nome(v, n, nome);
    if (i < 0) return false;
    *c = v[i];
    return true;
}

const char *conta_nome(unsigned uid, char *buf, int max)
{
    conta_t c;
    if (conta_por_uid(uid, &c)) copia_texto(buf, c.nome, (size_t)max);
    else formatar_texto(buf, (size_t)max, "%u", uid);
    return buf;
}

/* ---------------------------------------------------------------- sombra */

int sombra_ler(const char *nome, char *hash, int max)
{
    int erro;
    char *b = ler_tudo(CONTAS_ARQ_SOMBRA, &erro);
    if (!b) return erro == ERRO_NAO_EXISTE ? 0 : erro;
    int achou = 0;
    int nn = (int)tam_texto(nome);
    for (char *l = b; *l;) {
        char *fim = l;
        while (*fim && *fim != '\n') fim++;
        char *prox = *fim ? fim + 1 : fim;
        *fim = 0;
        if (fim > l && fim[-1] == '\r') fim[-1] = 0;
        if (compara_texto_n(l, nome, (size_t)nn) == 0 && l[nn] == ':') {
            copia_texto(hash, l + nn + 1, (size_t)max);
            achou = 1;
            break;
        }
        l = prox;
    }
    memset(b, 0, ARQ_MAX);   /* no dejar hashes por la memoria */
    libera(b);
    return achou;
}

/* escrever_tudo de la libsagui devuelve los bytes escritos: aquí, 0 o el error. */
static int escrever_todo(int fd, const char *s, int n)
{
    int r = escrever_tudo(fd, s, (unsigned)n);
    return r == n ? 0 : r < 0 ? r : ERRO_LIMITE;
}

int sombra_gravar(const char *nome, const char *hash)
{
    if (!nome_valido(nome) || acha_char(hash, '\n') || acha_char(hash, ':')) return ERRO_INVALIDO;
    int erro;
    char *b = ler_tudo(CONTAS_ARQ_SOMBRA, &erro);
    if (!b && erro != ERRO_NAO_EXISTE) return erro;
    char *novo = aloca(ARQ_MAX + SOMBRA_TEXTO_MAX + 64);
    if (!novo) { if (b) libera(b); return ERRO_SEM_MEMORIA; }
    int n = 0;
    int nn = (int)tam_texto(nome);
    bool posto = false;
    for (char *l = b; l && *l;) {
        char *fim = l;
        while (*fim && *fim != '\n') fim++;
        char *prox = *fim ? fim + 1 : fim;
        *fim = 0;
        if (compara_texto_n(l, nome, (size_t)nn) == 0 && l[nn] == ':') {
            if (!posto) n += formatar_texto(novo + n, (size_t)(ARQ_MAX + SOMBRA_TEXTO_MAX - n), "%s:%s\n", nome, hash);
            posto = true;
        } else if (*l && n + (int)tam_texto(l) + 2 < ARQ_MAX) {
            n += formatar_texto(novo + n, (size_t)(ARQ_MAX + SOMBRA_TEXTO_MAX - n), "%s\n", l);
        }
        l = prox;
    }
    if (!posto) n += formatar_texto(novo + n, (size_t)(ARQ_MAX + SOMBRA_TEXTO_MAX - n), "%s:%s\n", nome, hash);
    if (b) { memset(b, 0, ARQ_MAX); libera(b); }
    int r = conf_gravar("sombra", novo, n);
    memset(novo, 0, (size_t)n);
    libera(novo);
    return r;
}

/* ---------------------------------------------------------------- configuración del sistema */

bool conf_persistente(void)
{
    int e = usuario_op(USU_ESTADO, 0, 0);
    return e >= 0 && !(e & USU_EST_EN_VIVO);
}

/* Crea las carpetas que falten hasta la última "/" de "caminho". */
static void criar_pais(const char *caminho)
{
    char c[160];
    copia_texto(c, caminho, sizeof c);
    for (char *p = c + 1; *p; p++)
        if (*p == '/') { *p = 0; criar_pasta(c); *p = '/'; }
}

/* Escribe "arq" de una vez: temporal al lado y renombrar encima. */
static int gravar_atomico(const char *arq, const void *dados, int n)
{
    char tmp[176];
    formatar_texto(tmp, sizeof tmp, "%s.tmp", arq);
    criar_pais(arq);
    int fd = abrir(tmp, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int r = escrever_todo(fd, (const char *)dados, n);
    if (r == 0) sincronizar(fd);
    fechar(fd);
    if (r >= 0) {
        r = renomear(tmp, arq);
        if (r == ERRO_JA_EXISTE) { apagar(arq); r = renomear(tmp, arq); }   /* por si el sistema de archivos no reemplaza */
    }
    if (r < 0) apagar(tmp);
    return r < 0 ? r : 0;
}

int conf_gravar(const char *rel, const void *dados, int n)
{
    if (!rel[0] || rel[0] == '/' || acha_char(rel, '.') == rel) return ERRO_INVALIDO;
    char ram[160], disco[176];
    formatar_texto(ram, sizeof ram, "/etc/%s", rel);
    formatar_texto(disco, sizeof disco, "%s/etc/%s", CONF_SISTEMA_PONTO, rel);
    int rd = 0;
    if (conf_persistente()) rd = gravar_atomico(disco, dados, n);
    int rr = gravar_atomico(ram, dados, n);
    return rd < 0 ? rd : rr;
}

int conf_apagar(const char *rel)
{
    char ram[160], disco[176];
    formatar_texto(ram, sizeof ram, "/etc/%s", rel);
    formatar_texto(disco, sizeof disco, "%s/etc/%s", CONF_SISTEMA_PONTO, rel);
    if (conf_persistente()) apagar(disco);
    return apagar(ram);
}

/* ---------------------------------------------------------------- identidad */

unsigned meu_uid(void) { return (unsigned)usuario_op(USU_CONSULTAR, 0, 0) & 0xFFFF; }
unsigned meu_gid(void) { return ((unsigned)usuario_op(USU_CONSULTAR, 0, 0) >> 16) & 0xFFFF; }
bool     sou_root(void) { return meu_uid() == UID_ROOT; }

bool falta_admin(const char *programa)
{
    if (sou_root()) return false;
    imprimir_em(ERROS, "%s: hace falta ser administrador: usa «admin %s ...» (o «su»)\n", programa, programa);
    return true;
}

/* ---------------------------------------------------------------- pedir al gestor de servicios */

int ler_senha(const char *pergunta, char *buf, int max)
{
    imprimir_em(ERROS, "%s", pergunta);
    int n = 0;
    if (tipo_descritor(ENTRADA) != TIPO_TERMINAL) {
        /* Entrada redirigida (una tubería, ssh sin terminal): una línea. */
        n = ler_linha(ENTRADA, buf, (unsigned)max);
        imprimir_em(ERROS, "\n");
        if (n < 0) { buf[0] = 0; return -1; }
        buf[max - 1] = 0;
        int t = (int)tam_texto(buf);
        while (t > 0 && (buf[t - 1] == '\r' || buf[t - 1] == '\n')) buf[--t] = 0;
        return t;
    }
    modo_terminal(TERM_BRUTO);   /* sin eco */
    for (;;) {
        unsigned char c;
        int r = ler(ENTRADA, &c, 1);
        if (r <= 0 || c == '\n' || c == '\r') break;
        if (c == 3) { n = -1; break; }                       /* Ctrl+C */
        if (c == 8 || c == 127) { if (n > 0) n--; continue; }
        if (c == 27 || c < 32) continue;
        if (n < max - 1) buf[n++] = (char)c;
    }
    modo_terminal(TERM_LINHA);
    imprimir_em(ERROS, "\n");
    if (n < 0) { memset(buf, 0, (size_t)max); return -1; }
    buf[n] = 0;
    return n;
}

int pedir_conta(pedido_conta_t *p)
{
    int id = servicos_kernel(SERV_PEDIR, (int)p);
    if (id < 0) return id;
    unsigned fim = tempo_ms() + 90000;   /* la comprobación tarda; en una máquina emulada, más */
    for (;;) {
        int r = servicos_kernel(SERV_RESULTADO, id);
        if (r != 0) return r;
        if ((int)(tempo_ms() - fim) >= 0) return ERRO_TEMPO;
        dormir(50);
    }
}

/* Pide la contraseña nueva dos veces. 0 = bien. */
static int senha_nova_dupla(char *nova, int max)
{
    char otra[PED_SENHA_MAX];
    for (int i = 0; i < 3; i++) {
        if (ler_senha("Contraseña nueva: ", nova, max) < 0) return -1;
        if ((int)tam_texto(nova) < 4) { imprimir_em(ERROS, "Demasiado corta (al menos 4 caracteres).\n"); continue; }
        if (ler_senha("Repítela: ", otra, sizeof otra) < 0) return -1;
        bool igual = compara_texto(nova, otra) == 0;
        memset(otra, 0, sizeof otra);
        if (igual) return 0;
        imprimir_em(ERROS, "No coinciden.\n");
    }
    return -1;
}

int elevar_interativo(unsigned uid_alvo, const char *programa)
{
    if (meu_uid() == uid_alvo) return 0;
    pedido_conta_t p;
    memset(&p, 0, sizeof p);
    p.tipo = PED_ELEVAR;
    p.uid_alvo = uid_alvo;
    char alvo[CONTA_NOME_MAX];
    conta_nome(uid_alvo, alvo, sizeof alvo);
    /* Primero sin contraseña: el supervisor dice si la cuenta tiene (sin contar como fallo). */
    int r = pedir_conta(&p);
    for (int intento = 0; r != 1 && intento < 3; intento++) {
        if (r == ERRO_SEM_SENHA) {
            if (usuario_op(USU_ESTADO, 0, 0) & USU_EST_SERVICIO) break;
            imprimir_em(ERROS, "%s aún no tiene contraseña. Elige una (se pedirá a partir de ahora):\n", alvo);
            if (senha_nova_dupla(p.senha_nova, sizeof p.senha_nova) < 0) { r = ERRO_INVALIDO; break; }
        } else if (r == ERRO_ACESSO) {
            char q[64];
            formatar_texto(q, sizeof q, "Contraseña de %s: ", alvo);
            if (ler_senha(q, p.senha, sizeof p.senha) < 0) break;
        } else break;
        r = pedir_conta(&p);
        if (r == ERRO_ACESSO && p.senha[0]) imprimir_em(ERROS, "Contraseña incorrecta.\n");
    }
    memset(&p, 0, sizeof p);
    if (r == 1) return 0;
    if (r == ERRO_NAO_EXISTE)
        imprimir_em(ERROS, "%s: el gestor de servicios no está en marcha: no se puede comprobar la contraseña "
                    "(arranca en modo rescate: Ctrl al encender)\n", programa);
    else if (r != ERRO_INVALIDO && !(r == ERRO_ACESSO))
        imprimir_em(ERROS, "%s: %s\n", programa, nome_erro(r));
    else if (r == ERRO_ACESSO)
        imprimir_em(ERROS, "%s: no se concedió\n", programa);
    return r < 0 ? r : ERRO_ACESSO;
}
