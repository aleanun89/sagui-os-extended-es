// SPDX-License-Identifier: GPL-2.0-or-later
/* servicios - el supervisor del gestor de servicios (D216, mejora 074).
 *
 * El kernel lo lanza al arrancar (después del Terminal) y lo relanza si muere.
 * Lee las definiciones (archivos .conf) de /etc/servicios (las del sistema) y
 * de /home/.config/servicios (las del usuario; con el mismo nombre ganan
 * estas), arranca los que tienen "arrancar=si" en cuanto están listas sus
 * dependencias, los vigila y los vuelve a lanzar si mueren, con espera
 * creciente (1, 2, 4, 8 s... hasta 1 min); tras "max_fallos" fallos seguidos
 * (5 por defecto) el servicio queda «fallido» y se avisa en pantalla.
 *
 * Un servicio:
 *   nombre=litemonkey
 *   descripcion=Base de datos lite-monkey (puerto 7979)
 *   comando=/bin/litemonkey --datos /home/litemonkey
 *   arrancar=si              # al iniciar el sistema
 *   reiniciar=fallo          # siempre | fallo | no
 *   espera=red,/home         # la red con dirección, algo montado en /home, otros servicios
 *   entorno=LMK_MEMORIA=64   # (puede repetirse)
 *   registro=/logs/servicios/litemonkey.log   # por defecto /logs/servicios/<nombre>.log (o /tmp)
 *   parar_espera=5           # segundos que tiene para terminar antes de matarlo
 *   max_fallos=5
 *   pasta=/home              # carpeta de trabajo (por defecto /)
 *   usuario=root             # con qué cuenta corre (D255): por defecto root en /etc/servicios;
 *                            # los de /home/.config/servicios siempre como sagui
 *
 * Sin terminal: la entrada del servicio es un pipe que el supervisor cierra
 * para pedirle que pare (leer da fin de archivo; "while read l; do ...; done"
 * en un script basta), y la salida y los errores van a su registro. Si en
 * "parar_espera" segundos no ha terminado, se mata. SaguiOS no tiene señales:
 * ese fin de archivo es el aviso de parada (D216).
 *
 * Al apagar o reiniciar, el kernel avisa (SERV_APAGANDO) y espera como mucho
 * 20 s: aquí se paran primero los que dependen de otros y al final aquellos
 * de los que dependen, y después se dice SERV_PRONTO.
 *
 * Su propio registro: /logs/servicios.log (o /tmp/.servicios/servicios.log), y cada línea
 * también en el log del sistema (serie) como "servicios: ...".
 * El estado y las órdenes de "servicio" y del Administrador de tareas van por
 * /tmp/.servicios (servicos.h).
 *
 * D255: también es quien comprueba las contraseñas. El kernel le pasa los
 * pedidos de su, admin y passwd (SERV_PROXIMO): aquí se mira la sombra, se
 * anota y se responde (SERV_RESPONDER); si se concede, el kernel cambia la
 * identidad de quien lo pidió. Ver atender_contas. */
#include "sagui.h"
#include "servicos.h"
#include "contas.h"
#include "pbkdf2.h"

#define MAX_SERV       20          /* cada uno en marcha gasta un descriptor (su pipe) */
#define CICLO_MS       200
#define REGISTRO_MAX   (256u * 1024u)   /* al pasar de aquí, el registro pasa a .1 */
#define APAGADO_MS     15000       /* lo que se reparte al apagar (el kernel espera 20 s) */
#define VIDA_ESTAVEL_MS 60000      /* vivo tanto tiempo: los fallos vuelven a contar desde cero */
#define ESPERA_MAX_MS  60000

enum { E_PARADO, E_ESPERANDO, E_MARCHA, E_REINICIANDO, E_PARANDO, E_FALLIDO };
static const char *const nomes_estado[] = { SERV_E_PARADO, SERV_E_ESPERANDO, SERV_E_MARCHA,
                                            SERV_E_REINICIANDO, SERV_E_PARANDO, SERV_E_FALLIDO };
enum { R_NAO, R_FALHA, R_SEMPRE };

typedef struct {
    /* la definición */
    char nome[32], descricao[64], comando[256], registro[96], entorno[256], espera[128], pasta[96];
    char programa[32];   /* nombre del ejecutable en marcha (el que ve ps) */
    unsigned uid, gid;   /* D255: con qué cuenta corre */
    char origem[96];
    int  politica;
    bool arrancar, usuario;
    unsigned parar_ms;
    int  max_falhas;
    /* en marcha */
    int  estado, pid, pipe_esc;
    unsigned desde, inicio, proximo, limite_parar;
    unsigned reinicios;
    int  falhas;
    bool querer;      /* tiene que estar en marcha */
    bool relancar;    /* reiniciar: al pararse, volver a lanzarlo */
    bool remover;     /* ya no está definido: se quita al pararse */
    bool matado;
    char detalhe[64];
} servico_t;

static servico_t serv[MAX_SERV];
static int qtd;
static bool mudou = true;
static int fd_log = -1;      /* el registro del supervisor (también en 1 y 2) */
static int fd_nulo = -1;     /* el extremo de lectura de un pipe sin escritor: entrada vacía */
static char arq_log[96];
static int ativos_ditos = -1;

/* ------------------------------------------------------------ registro */

static void anotar(const char *fmt, ...)
{
    char t[240];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(t, sizeof t, fmt, ap);
    va_end(ap);
    registrar("servicios: %s", t);
    if (fd_log >= 0) {
        fuso_data_t d;
        agora_local(&d);
        char h[32];
        formatar_data(h, sizeof h, "%Y-%m-%d %H:%M:%S", &d);
        imprimir_em(fd_log, "%s %s\n", h, t);
    }
}

/* Al pasar de REGISTRO_MAX, el registro pasa a "<nombre>.1" (se pierde el .1 anterior). */
static void rodar_registro(const char *arq)
{
    info_arquivo_t i;
    if (info_arquivo(arq, &i) == 0 && i.tamanho > REGISTRO_MAX) {
        char velho[112];
        formatar_texto(velho, sizeof velho, "%s.1", arq);
        apagar(velho);
        renomear(arq, velho);
    }
}

static void pasta_de(const char *arq, char *dst, int max)
{
    copia_texto(dst, arq, (size_t)max);
    char *b = acha_ultimo_char(dst, '/');
    if (b && b != dst) *b = 0;
    else if (b) b[1] = 0;
}

static int abrir_registro(const char *arq)
{
    char pasta[96];
    pasta_de(arq, pasta, sizeof pasta);
    servicos_criar_pastas(pasta);
    rodar_registro(arq);
    return abrir(arq, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_ACRESCENTAR);
}

/* Los descriptores 0, 1 y 2 del supervisor: 0 una entrada vacía, 1 y 2 su
 * registro. Están siempre ocupados, así que el pipe de cada servicio nunca
 * cae en ellos; para lanzar uno se ponen los suyos y después se restauran. */
static void preparar_descritores(void)
{
    formatar_texto(arq_log, sizeof arq_log, "%s/servicios.log", servicos_pasta_registros());
    int fd = abrir_registro(arq_log);
    if (fd < 0) {
        copia_texto(arq_log, "/tmp/.servicios/servicios.log", sizeof arq_log);
        fd = abrir_registro(arq_log);
    }
    int p[2];
    if (criar_pipe(p) == 0) {
        fechar(p[1]);
        fd_nulo = duplicar(p[0], 30);
        if (fd_nulo != p[0]) fechar(p[0]);
        duplicar(fd_nulo, ENTRADA);
    }
    if (fd >= 0) {
        fd_log = duplicar(fd, 31);
        if (fd_log != fd) fechar(fd);
        duplicar(fd_log, SAIDA);
        duplicar(fd_log, ERROS);
    }
}

/* ------------------------------------------------------------ definiciones */

static void aparar(char *s)
{
    int n = (int)tam_texto(s), i = 0;
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) s[--n] = 0;
    while (s[i] == ' ' || s[i] == '\t') i++;
    if (i) memmove(s, s + i, (size_t)(n - i + 1));
}

static bool nome_valido(const char *n)
{
    if (!*n) return false;
    for (const char *p = n; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
              *p == '-' || *p == '_' || *p == '.'))
            return false;
    return true;
}

static bool sim(const char *v)
{
    return compara_texto(v, "si") == 0 || compara_texto(v, "sí") == 0 || compara_texto(v, "yes") == 0 ||
           compara_texto(v, "1") == 0 || compara_texto(v, "true") == 0;
}

static char *ler_arquivo(const char *arq)
{
    int fd = abrir(arq, ABRIR_LER);
    if (fd < 0) return NULL;
    unsigned tam;
    char *b = ler_todo_fd(fd, &tam);
    fechar(fd);
    return b;
}

static bool ler_definicao(const char *arq, const char *nome_arq, bool usuario, servico_t *s)
{
    memset(s, 0, sizeof *s);
    s->politica = R_FALHA;
    s->parar_ms = 5000;
    s->max_falhas = 5;
    s->usuario = usuario;
    s->pipe_esc = -1;
    s->uid = usuario ? UID_SAGUI : UID_ROOT;   /* D255: los del usuario, nunca como root */
    s->gid = usuario ? GID_SAGUI : GID_ROOT;
    char conta[CONTA_NOME_MAX] = "";
    copia_texto(s->origem, arq, sizeof s->origem);
    copia_texto(s->nome, nome_arq, sizeof s->nome);
    char *ponto = acha_ultimo_char(s->nome, '.');
    if (ponto) *ponto = 0;   /* "cron.conf" -> "cron" */
    char *b = ler_arquivo(arq);
    if (!b) { anotar("%s: no se puede leer", arq); return false; }
    b[utf8_para_latin1(b, tam_texto(b), NULL)] = 0;   /* las definiciones suelen estar en UTF-8 */
    int num = 0;
    for (char *l = b; l && *l;) {
        char *fim = acha_char(l, '\n');
        if (fim) *fim = 0;
        num++;
        /* Comentarios: la línea que empieza por # y lo que va tras " #". */
        for (char *p = l; *p; p++)
            if (*p == '#' && (p == l || p[-1] == ' ' || p[-1] == '\t')) { *p = 0; break; }
        aparar(l);
        if (*l) {
            char *ig = acha_char(l, '=');
            if (!ig) anotar("%s:%d: falta el «=» (clave=valor)", arq, num);
            else {
                *ig = 0;
                char *c = l, *v = ig + 1;
                aparar(c);
                aparar(v);
                if (compara_texto(c, "nombre") == 0) copia_texto(s->nome, v, sizeof s->nome);
                else if (compara_texto(c, "descripcion") == 0 || compara_texto(c, "descripción") == 0)
                    copia_texto(s->descricao, v, sizeof s->descricao);
                else if (compara_texto(c, "comando") == 0) copia_texto(s->comando, v, sizeof s->comando);
                else if (compara_texto(c, "arrancar") == 0) s->arrancar = sim(v);
                else if (compara_texto(c, "reiniciar") == 0) {
                    if (compara_texto(v, "siempre") == 0) s->politica = R_SEMPRE;
                    else if (compara_texto(v, "no") == 0) s->politica = R_NAO;
                    else if (compara_texto(v, "fallo") == 0) s->politica = R_FALHA;
                    else anotar("%s:%d: reiniciar=%s no vale (siempre, fallo o no)", arq, num, v);
                } else if (compara_texto(c, "espera") == 0) copia_texto(s->espera, v, sizeof s->espera);
                else if (compara_texto(c, "entorno") == 0) {
                    if (s->entorno[0]) junta_texto(s->entorno, "\n", sizeof s->entorno);
                    junta_texto(s->entorno, v, sizeof s->entorno);
                } else if (compara_texto(c, "registro") == 0) copia_texto(s->registro, v, sizeof s->registro);
                else if (compara_texto(c, "pasta") == 0 || compara_texto(c, "carpeta") == 0)
                    copia_texto(s->pasta, v, sizeof s->pasta);
                else if (compara_texto(c, "parar_espera") == 0) {
                    int n = texto_para_int(v);
                    s->parar_ms = (unsigned)(n < 1 ? 1 : n > 10 ? 10 : n) * 1000u;
                } else if (compara_texto(c, "max_fallos") == 0) {
                    int n = texto_para_int(v);
                    s->max_falhas = n < 1 ? 1 : n > 50 ? 50 : n;
                } else if (compara_texto(c, "usuario") == 0) copia_texto(conta, v, sizeof conta);
                else anotar("%s:%d: clave «%s» desconocida (se ignora)", arq, num, c);
            }
        }
        l = fim ? fim + 1 : NULL;
    }
    libera(b);
    if (!nome_valido(s->nome)) { anotar("%s: nombre «%s» no válido (letras, números, - _ .)", arq, s->nome); return false; }
    if (!s->comando[0]) { anotar("%s: falta comando=", arq); return false; }
    if (conta[0]) {
        conta_t ct;
        if (!conta_por_nome(conta, &ct)) { anotar("%s: el usuario «%s» no existe", arq, conta); return false; }
        if (usuario && ct.uid != UID_SAGUI)
            anotar("%s: los servicios de /home/.config/servicios corren como sagui (usuario=%s no vale)", arq, conta);
        else { s->uid = ct.uid; s->gid = ct.gid; }
    }
    if (!s->registro[0])
        formatar_texto(s->registro, sizeof s->registro, "%s/servicios/%s.log", servicos_pasta_registros(), s->nome);
    return true;
}

/* "nombre=si|no" de servicio activar/desactivar: cambia el arranque automático.
 * D255: el de los servicios del sistema está en /etc/servicios/activados (se
 * guarda en SAGUI-CONF, solo root); el de /home/.config/servicios/activados solo
 * vale para los servicios del usuario. Compatibilidad: mientras no exista el del
 * sistema, el de /home sigue valiendo para todos (como antes de D255: así una
 * máquina que ya arrancaba sshd sola no pierde el acceso al actualizar); con
 * SAGUI-CONF, sus líneas de servicios del sistema se pasan una vez al nuevo. */
#define ACTIVADOS_SIS_REL "servicios/activados"
#define ACTIVADOS_SIS     "/etc/" ACTIVADOS_SIS_REL

static void ler_activados(const char *arq, servico_t *v, int n, int so_usuario, char *migrar, unsigned cap)
{
    char *b = ler_arquivo(arq);
    if (!b) return;
    for (char *l = b; l && *l;) {
        char *fim = acha_char(l, '\n');
        if (fim) *fim = 0;
        char *ig = acha_char(l, '=');
        if (ig) {
            *ig = 0;
            aparar(l);
            for (int i = 0; i < n; i++) {
                if (compara_texto(v[i].nome, l) != 0) continue;
                if (so_usuario == 1 && !v[i].usuario) {   /* del sistema en el archivo del usuario */
                    if (migrar) { junta_texto(migrar, l, cap); junta_texto(migrar, "=", cap);
                                  junta_texto(migrar, ig + 1, cap); junta_texto(migrar, "\n", cap); }
                    continue;
                }
                if (so_usuario == 2 && v[i].usuario) continue;
                v[i].arrancar = sim(ig + 1);
            }
        }
        l = fim ? fim + 1 : NULL;
    }
    libera(b);
}

static void aplicar_activados(servico_t *v, int n)
{
    info_arquivo_t inf;
    bool tem_sis = info_arquivo(ACTIVADOS_SIS, &inf) == 0;
    if (!tem_sis) {
        char migrar[512] = "";
        ler_activados(SERV_ARQ_ACTIVADOS, v, n, 0, NULL, 0);   /* como antes: vale para todos */
        if (conf_persistente()) {
            ler_activados(SERV_ARQ_ACTIVADOS, v, n, 1, migrar, sizeof migrar);
            if (migrar[0] && conf_gravar(ACTIVADOS_SIS_REL, migrar, (int)tam_texto(migrar)) == 0)
                anotar("arranque automático de los servicios del sistema pasado a " ACTIVADOS_SIS " (SAGUI-CONF)");
        } else {
            anotar("aviso: sin SAGUI-CONF, el arranque automático de los servicios del sistema sale de %s (lo puede "
                   "cambiar el usuario)", SERV_ARQ_ACTIVADOS);
        }
        return;
    }
    ler_activados(SERV_ARQ_ACTIVADOS, v, n, 1, NULL, 0);
    ler_activados(ACTIVADOS_SIS, v, n, 2, NULL, 0);
}

static servico_t novos[MAX_SERV];

static int ler_pasta(const char *pasta, bool usuario, int n)
{
    entrada_pasta_t e;
    for (int i = 0; listar(pasta, i, &e) == 1; i++) {
        int t = (int)tam_texto(e.nome);
        if (e.tipo == TIPO_PASTA || t < 6 || compara_texto(e.nome + t - 5, ".conf") != 0) continue;
        char arq[128];
        formatar_texto(arq, sizeof arq, "%s/%s", pasta, e.nome);
        servico_t s;
        if (!ler_definicao(arq, e.nome, usuario, &s)) continue;
        int k = 0;
        while (k < n && compara_texto(novos[k].nome, s.nome) != 0) k++;
        if (k < n) {
            anotar("%s: sustituye a la definición de %s", arq, novos[k].origem);
            novos[k] = s;
        } else if (n < MAX_SERV) novos[n++] = s;
        else anotar("%s: demasiados servicios (máximo %d): se ignora", arq, MAX_SERV);
    }
    return n;
}

static void mudar_estado(servico_t *s, int e)
{
    if (s->estado != e) {
        s->estado = e;
        s->desde = tempo_ms();
    }
    mudou = true;
}

/* Las dependencias que son otros servicios, una a una. */
static int dependencias(const servico_t *s, char nomes[][32], int max)
{
    int n = 0;
    const char *p = s->espera;
    while (*p && n < max) {
        while (*p == ',' || *p == ' ') p++;
        int k = 0;
        char d[32];
        while (*p && *p != ',') { if (k < 31) d[k++] = *p; p++; }
        while (k > 0 && d[k - 1] == ' ') k--;
        d[k] = 0;
        if (k) copia_texto(nomes[n++], d, 32);
    }
    return n;
}

static int achar(const char *nome)
{
    for (int i = 0; i < qtd; i++)
        if (compara_texto(serv[i].nome, nome) == 0) return i;
    return -1;
}

/* Nivel de dependencias (0 = no depende de ningún servicio); -1 = ciclo. */
static int nivel(int i, int prof)
{
    if (prof > MAX_SERV) return -1;
    char d[8][32];
    int nd = dependencias(&serv[i], d, 8), n = 0;
    for (int k = 0; k < nd; k++) {
        int j = achar(d[k]);
        if (j < 0) continue;
        int v = nivel(j, prof + 1);
        if (v < 0) return -1;
        if (v + 1 > n) n = v + 1;
    }
    return n;
}

static void comecar_parar(servico_t *s, unsigned prazo_ms)
{
    if (s->estado != E_MARCHA) return;
    if (s->pipe_esc >= 0) { fechar(s->pipe_esc); s->pipe_esc = -1; }   /* el aviso: fin de su entrada */
    s->limite_parar = tempo_ms() + prazo_ms;
    s->matado = false;
    copia_texto(s->detalhe, "", sizeof s->detalhe);
    mudar_estado(s, E_PARANDO);
    anotar("%s: parando (pid %d; %u s para terminar)", s->nome, s->pid, prazo_ms / 1000);
}

static void quitar(int i)
{
    anotar("%s: ya no está definido: se quita", serv[i].nome);
    for (int k = i; k < qtd - 1; k++) serv[k] = serv[k + 1];
    qtd--;
    mudou = true;
}

/* Lee (o relee, con "servicio recargar") todas las definiciones. Lo que ya
 * estaba en marcha sigue en marcha con la definición nueva (se aplica al
 * reiniciarlo); lo que ya no existe se para y se quita; lo nuevo con
 * arrancar=si se arranca. */
static int carregar(bool recarga)
{
    int n = ler_pasta(SERV_DIR_SISTEMA, false, 0);
    n = ler_pasta(SERV_DIR_USUARIO, true, n);
    aplicar_activados(novos, n);
    for (int i = 0; i < qtd; i++) {
        int k = 0;
        while (k < n && compara_texto(novos[k].nome, serv[i].nome) != 0) k++;
        servico_t *s = &serv[i];
        if (k < n) {
            servico_t *d = &novos[k];
            copia_texto(s->descricao, d->descricao, sizeof s->descricao);
            copia_texto(s->comando, d->comando, sizeof s->comando);
            copia_texto(s->registro, d->registro, sizeof s->registro);
            copia_texto(s->entorno, d->entorno, sizeof s->entorno);
            copia_texto(s->espera, d->espera, sizeof s->espera);
            copia_texto(s->pasta, d->pasta, sizeof s->pasta);
            copia_texto(s->origem, d->origem, sizeof s->origem);
            s->politica = d->politica;
            s->arrancar = d->arrancar;
            s->usuario = d->usuario;
            s->parar_ms = d->parar_ms;
            s->max_falhas = d->max_falhas;
            s->remover = false;
            d->nome[0] = 0;   /* ya está */
        } else {
            s->remover = true;
            s->querer = false;
            s->relancar = false;
        }
    }
    for (int i = qtd - 1; i >= 0; i--)
        if (serv[i].remover) {
            if (serv[i].estado == E_MARCHA) comecar_parar(&serv[i], serv[i].parar_ms);
            else if (serv[i].estado != E_PARANDO) quitar(i);
        }
    for (int k = 0; k < n; k++) {
        if (!novos[k].nome[0]) continue;
        if (qtd >= MAX_SERV) { anotar("%s: demasiados servicios", novos[k].nome); continue; }
        servico_t *s = &serv[qtd++];
        *s = novos[k];
        s->estado = E_PARADO;
        s->desde = tempo_ms();
        s->querer = s->arrancar;
        anotar("%s: definido en %s (arrancar=%s)%s", s->nome, s->origem, s->arrancar ? "si" : "no",
               recarga ? " (nuevo)" : "");
    }
    for (int i = 0; i < qtd; i++)
        if (nivel(i, 0) < 0 && serv[i].estado != E_FALLIDO) {
            serv[i].querer = false;
            copia_texto(serv[i].detalhe, "dependencia circular", sizeof serv[i].detalhe);
            mudar_estado(&serv[i], E_FALLIDO);
            anotar("%s: dependencia circular en espera=%s: no se arranca", serv[i].nome, serv[i].espera);
        }
    mudou = true;
    return qtd;
}

/* ------------------------------------------------------------ dependencias */

static bool rede_pronta(void)
{
    config_rede_t c;
    return config_rede(REDE_CONSULTAR, &c) == 0 && c.ip != 0;
}

/* true = todo listo; si no, lo que falta en "falta". */
static bool dependencias_prontas(const servico_t *s, char *falta, int max)
{
    char d[8][32];
    int nd = dependencias(s, d, 8);
    for (int k = 0; k < nd; k++) {
        bool ok;
        if (compara_texto(d[k], "red") == 0) ok = rede_pronta();
        else if (d[k][0] == '/') ok = servicos_montado(d[k]);
        else {
            int j = achar(d[k]);
            if (j < 0) { formatar_texto(falta, (size_t)max, "espera: %s (no existe)", d[k]); return false; }
            ok = serv[j].estado == E_MARCHA;
        }
        if (!ok) { formatar_texto(falta, (size_t)max, "espera: %s", d[k]); return false; }
    }
    return true;
}

/* ------------------------------------------------------------ lanzar */

/* Divide la orden en palabras (con "comillas" y 'comillas'). */
static int dividir(char *c, char **argv, int max)
{
    int n = 0;
    char *p = c;
    while (*p && n < max - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        char *o = p, *d = p;
        argv[n++] = d;
        while (*o && *o != ' ' && *o != '\t') {
            if (*o == '"' || *o == '\'') {
                char q = *o++;
                while (*o && *o != q) *d++ = *o++;
                if (*o) o++;
            } else *d++ = *o++;
        }
        p = *o ? o + 1 : o;
        *d = 0;
    }
    argv[n] = NULL;
    return n;
}

static bool achar_programa(const char *nome, char *caminho, int max)
{
    info_arquivo_t i;
    if (acha_char(nome, '/')) {
        copia_texto(caminho, nome, (size_t)max);
        return info_arquivo(caminho, &i) == 0;
    }
    static const char *const pastas[] = { "/bin", "/apps/bin" };
    for (int k = 0; k < 2; k++) {
        formatar_texto(caminho, (size_t)max, "%s/%s", pastas[k], nome);
        if (info_arquivo(caminho, &i) == 0) return true;
    }
    return false;
}

static bool tem_env(char **v, int n, const char *nome)
{
    size_t t = tam_texto(nome);
    for (int i = 0; i < n; i++)
        if (compara_texto_n(v[i], nome, t) == 0 && v[i][t] == '=') return true;
    return false;
}

static void falhou(servico_t *s, int codigo, unsigned vida);

static void lancar(servico_t *s)
{
    char cmd[256];
    char *argv[32];
    copia_texto(cmd, s->comando, sizeof cmd);
    int argc = dividir(cmd, argv, 32);
    char caminho[128];
    if (!argc || !achar_programa(argv[0], caminho, sizeof caminho)) {
        anotar("%s: no se encuentra el programa «%s»", s->nome, argc ? argv[0] : "");
        formatar_texto(s->detalhe, sizeof s->detalhe, "no existe %s", argc ? argv[0] : "el programa");
        s->querer = false;
        mudar_estado(s, E_FALLIDO);
        return;
    }
    /* El entorno: el del supervisor, un PATH y un HOME si faltan, y los del servicio. */
    char *envp[48];
    int ne = 0;
    for (int i = 0; environ && environ[i] && ne < 40; i++) envp[ne++] = environ[i];
    char ent[256];
    copia_texto(ent, s->entorno, sizeof ent);
    for (char *l = ent; *l && ne < 46;) {
        char *fim = acha_char(l, '\n');
        if (fim) *fim = 0;
        if (acha_char(l, '=')) envp[ne++] = l;
        if (!fim) break;
        l = fim + 1;
    }
    static char path_padrao[] = "PATH=" SAGUI_PATH_PADRAO, home_padrao[] = "HOME=/home";
    if (!tem_env(envp, ne, "PATH") && ne < 47) envp[ne++] = path_padrao;
    if (!tem_env(envp, ne, "HOME") && ne < 47) envp[ne++] = home_padrao;
    envp[ne] = NULL;

    int saida = abrir_registro(s->registro);
    if (saida < 0) anotar("%s: no se puede abrir el registro %s (%s)", s->nome, s->registro, nome_erro(saida));
    int fdo = saida >= 0 ? saida : fd_log;
    if (fdo >= 0) {
        fuso_data_t d;
        agora_local(&d);
        char h[32];
        formatar_data(h, sizeof h, "%Y-%m-%d %H:%M:%S", &d);
        imprimir_em(fdo, "== %s inicio de %s: %s ==\n", h, s->nome, s->comando);
    }
    int p[2];
    if (criar_pipe(p) < 0) {
        if (saida >= 0) fechar(saida);
        anotar("%s: sin descriptores para su entrada", s->nome);
        falhou(s, -1, 0);
        return;
    }
    duplicar(p[0], ENTRADA);
    if (fdo >= 0) { duplicar(fdo, SAIDA); duplicar(fdo, ERROS); }
    if (s->pasta[0] && mudar_pasta(s->pasta) < 0) anotar("%s: la carpeta %s no existe; se usa /", s->nome, s->pasta);
    usuario_op(USU_COMO, (int)s->uid, (int)s->gid);   /* D255: el hijo nace con su cuenta */
    int pid = executar_env(caminho, argv, envp);
    usuario_op(USU_COMO, -1, 0);
    mudar_pasta("/");
    /* Los del supervisor otra vez: entrada vacía y su registro. */
    if (fd_nulo >= 0) duplicar(fd_nulo, ENTRADA);
    if (fd_log >= 0) { duplicar(fd_log, SAIDA); duplicar(fd_log, ERROS); }
    fechar(p[0]);
    if (saida >= 0) fechar(saida);
    if (pid < 0) {
        fechar(p[1]);
        anotar("%s: no se pudo lanzar %s: %s", s->nome, caminho, nome_erro(pid));
        falhou(s, pid, 0);
        return;
    }
    definir_grupo(pid, 0);   /* su propio grupo: el Ctrl+C de un terminal no lo alcanza */
    s->pid = pid;
    copia_texto(s->programa, nome_base(caminho), sizeof s->programa);
    s->pipe_esc = p[1];
    s->inicio = tempo_ms();
    s->detalhe[0] = 0;
    mudar_estado(s, E_MARCHA);
    anotar("%s: en marcha (pid %d): %s (uid %u)", s->nome, pid, s->comando, s->uid);
}

/* ------------------------------------------------------------ al terminar */

static void texto_codigo(int codigo, char *t, int max)
{
    if (codigo == SAIDA_ENCERRADO_PELO_KERNEL) copia_texto(t, "terminado por el sistema", (size_t)max);
    else formatar_texto(t, (size_t)max, "código %d", codigo);
}

/* Un fallo (salida inesperada o no se pudo lanzar): espera creciente o «fallido». */
static void falhou(servico_t *s, int codigo, unsigned vida)
{
    char c[40];
    texto_codigo(codigo, c, sizeof c);
    if (vida >= VIDA_ESTAVEL_MS) s->falhas = 0;
    s->falhas++;
    if (s->falhas >= s->max_falhas) {
        s->querer = false;
        formatar_texto(s->detalhe, sizeof s->detalhe, "%d fallos seguidos (último: %s)", s->falhas, c);
        mudar_estado(s, E_FALLIDO);
        anotar("%s: fallido tras %d fallos seguidos (último: %s); no se reinicia más", s->nome, s->falhas, c);
        char aviso[128];
        formatar_texto(aviso, sizeof aviso, "El servicio «%s» ha fallado %d veces y se ha detenido.", s->nome, s->falhas);
        aviso_tela(aviso, NULL);   /* sin escritorio no hace nada */
        return;
    }
    unsigned espera = 1000u << (s->falhas - 1);
    if (espera > ESPERA_MAX_MS) espera = ESPERA_MAX_MS;
    s->proximo = tempo_ms() + espera;
    formatar_texto(s->detalhe, sizeof s->detalhe, "%s; vuelve en %u s", c, espera / 1000);
    mudar_estado(s, E_REINICIANDO);
    anotar("%s: terminó (%s) tras %u s; se reinicia en %u s (fallo %d de %d)", s->nome, c, vida / 1000, espera / 1000,
           s->falhas, s->max_falhas);
}

static void terminou(int pid, int codigo)
{
    int i = 0;
    while (i < qtd && serv[i].pid != pid) i++;
    if (i == qtd) { anotar("el proceso %d (no es un servicio) terminó con código %d", pid, codigo); return; }
    servico_t *s = &serv[i];
    if (s->pipe_esc >= 0) { fechar(s->pipe_esc); s->pipe_esc = -1; }
    unsigned vida = tempo_ms() - s->inicio;
    s->pid = 0;
    char c[40];
    texto_codigo(codigo, c, sizeof c);
    if (s->estado == E_PARANDO) {
        anotar("%s: parado (%s)", s->nome, c);
        if (s->remover) { quitar(i); return; }
        if (s->relancar) {
            s->relancar = false;
            s->falhas = 0;
            s->querer = true;
            mudar_estado(s, E_ESPERANDO);
        } else {
            s->querer = false;
            mudar_estado(s, E_PARADO);
        }
        return;
    }
    bool erro = codigo != 0;
    if (s->politica == R_SEMPRE || (s->politica == R_FALHA && erro)) { falhou(s, codigo, vida); return; }
    s->querer = false;
    formatar_texto(s->detalhe, sizeof s->detalhe, "terminó (%s)", c);
    mudar_estado(s, E_PARADO);
    anotar("%s: terminó (%s) tras %u s; reiniciar=%s: queda parado", s->nome, c, vida / 1000,
           s->politica == R_NAO ? "no" : "fallo");
}

static void recolher_filhos(void)
{
    int codigo, pid;
    while ((pid = recolher(&codigo)) > 0) terminou(pid, codigo);
}

/* ------------------------------------------------------------ el ciclo */

static void avancar(void)
{
    unsigned agora = tempo_ms();
    for (int i = 0; i < qtd; i++) {
        servico_t *s = &serv[i];
        switch (s->estado) {
        case E_PARADO:
        case E_FALLIDO:
            if (s->querer) { s->detalhe[0] = 0; mudar_estado(s, E_ESPERANDO); }
            break;
        case E_REINICIANDO:
            if (!s->querer) { mudar_estado(s, E_PARADO); break; }
            if ((int)(agora - s->proximo) < 0) break;
            s->reinicios++;
            mudar_estado(s, E_ESPERANDO);
            /* sigue: si ya está todo listo, se lanza ahora */
            /* fallthrough */
        case E_ESPERANDO: {
            if (!s->querer) { s->detalhe[0] = 0; mudar_estado(s, E_PARADO); break; }
            char falta[64];
            if (dependencias_prontas(s, falta, sizeof falta)) lancar(s);
            else if (compara_texto(falta, s->detalhe) != 0) {
                copia_texto(s->detalhe, falta, sizeof s->detalhe);
                mudou = true;
            }
            break;
        }
        case E_PARANDO:
            if (!s->matado && (int)(agora - s->limite_parar) >= 0) {
                s->matado = true;
                int r = matar(s->pid);
                anotar("%s: no terminó a tiempo: se termina a la fuerza (pid %d) -> %d", s->nome, s->pid, r);
            }
            break;
        case E_MARCHA:
            break;
        }
    }
}

/* Cuándo arrancó el sistema (UTC): distingue el estado de esta sesión del de
 * una anterior que quedara en un /tmp persistente (SAGUI-TMP). */
static unsigned arranque_utc(void)
{
    return hora_utc(NULL) - tempo_ms() / 1000u;
}

/* El estado para "servicio" y el Administrador de tareas (formato en servicos.h). */
static void limpar_campo(char *dst, const char *src, int max)
{
    copia_texto(dst, src, (size_t)max);
    for (char *p = dst; *p; p++)
        if (*p == '\t' || *p == '\n' || *p == '\r') *p = ' ';
}

static void publicar(void)
{
    int ativos = 0;
    for (int i = 0; i < qtd; i++)
        if (serv[i].estado == E_MARCHA || serv[i].estado == E_PARANDO) ativos++;
    if (ativos != ativos_ditos) {
        servicos_kernel(SERV_ATIVOS, ativos);
        ativos_ditos = ativos;
    }
    if (!mudou) return;
    mudou = false;
    unsigned cap = (unsigned)qtd * 420u + 64u;
    char *b = aloca(cap);
    if (!b) return;
    int n = formatar_texto(b, cap, "servicios %d %u\n", meu_pid(), arranque_utc());
    for (int i = 0; i < qtd; i++) {
        servico_t *s = &serv[i];
        char d[64], t[64], r[96];
        limpar_campo(d, s->descricao, sizeof d);
        limpar_campo(t, s->detalhe, sizeof t);
        limpar_campo(r, s->registro, sizeof r);
        n += formatar_texto(b + n, cap - (unsigned)n, "%s\t%s\t%d\t%u\t%u\t%d\t%d\t%s\t%s\t%s\t%s\n", s->nome,
                            nomes_estado[s->estado], s->pid, s->reinicios, s->desde, s->arrancar ? 1 : 0,
                            s->usuario ? 1 : 0, d, t, r, s->pid ? s->programa : "-");
    }
    n += formatar_texto(b + n, cap - (unsigned)n, "fin\n");
    int fd = abrir(SERV_ARQ_ESTADO, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd >= 0) {
        escrever_tudo(fd, b, (unsigned)n);
        fechar(fd);
    }
    libera(b);
}

/* ------------------------------------------------------------ órdenes */

static int gravar_activado(const char *nome, bool ligar, bool sistema)
{
    const char *arq = sistema ? ACTIVADOS_SIS : SERV_ARQ_ACTIVADOS;
    if (!sistema) servicos_criar_pastas(SERV_DIR_USUARIO);
    char *velho = ler_arquivo(arq);
    unsigned cap = (velho ? (unsigned)tam_texto(velho) : 0u) + 64u;
    char *novo = aloca(cap);
    if (!novo) { if (velho) libera(velho); return ERRO_SEM_MEMORIA; }
    novo[0] = 0;
    for (char *l = velho; l && *l;) {
        char *fim = acha_char(l, '\n');
        if (fim) *fim = 0;
        char nl[40];
        copia_texto(nl, l, sizeof nl);
        char *ig = acha_char(nl, '=');
        if (ig) *ig = 0;
        aparar(nl);
        if (*l && compara_texto(nl, nome) != 0) {
            junta_texto(novo, l, cap);
            junta_texto(novo, "\n", cap);
        }
        l = fim ? fim + 1 : NULL;
    }
    junta_texto(novo, nome, cap);
    junta_texto(novo, ligar ? "=si\n" : "=no\n", cap);
    if (velho) libera(velho);
    if (sistema) {   /* D255: en SAGUI-CONF (y en /etc) */
        int r = conf_gravar(ACTIVADOS_SIS_REL, novo, (int)tam_texto(novo));
        libera(novo);
        return r;
    }
    int fd = abrir(arq, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { libera(novo); return fd; }
    escrever_tudo(fd, novo, (unsigned)tam_texto(novo));
    fechar(fd);
    libera(novo);
    return 0;
}

static int ordem(const char *o, const char *nome, char *msg, int max)
{
    if (compara_texto(o, "recargar") == 0) {
        carregar(true);
        formatar_texto(msg, (size_t)max, "definiciones leídas de nuevo: %d servicios", qtd);
        return 0;
    }
    int i = achar(nome);
    if (i < 0) { formatar_texto(msg, (size_t)max, "no existe el servicio «%s» (mira «servicio listar»)", nome); return 1; }
    servico_t *s = &serv[i];
    if (compara_texto(o, "iniciar") == 0 || compara_texto(o, "reiniciar") == 0) {
        bool rein = o[0] == 'r';
        if (s->estado == E_MARCHA) {
            if (!rein) { formatar_texto(msg, (size_t)max, "%s ya está en marcha (pid %d)", s->nome, s->pid); return 0; }
            s->relancar = true;
            comecar_parar(s, s->parar_ms);
            formatar_texto(msg, (size_t)max, "reiniciando %s", s->nome);
            return 0;
        }
        if (s->estado == E_PARANDO) {
            s->relancar = true;
            formatar_texto(msg, (size_t)max, "%s se está parando; volverá a arrancar", s->nome);
            return 0;
        }
        s->querer = true;
        s->falhas = 0;
        s->detalhe[0] = 0;
        mudar_estado(s, E_ESPERANDO);
        anotar("%s: %s a pedido", s->nome, rein ? "reinicio" : "inicio");
        formatar_texto(msg, (size_t)max, "iniciando %s", s->nome);
        return 0;
    }
    if (compara_texto(o, "parar") == 0) {
        s->querer = false;
        s->relancar = false;
        if (s->estado == E_MARCHA) {
            comecar_parar(s, s->parar_ms);
            formatar_texto(msg, (size_t)max, "parando %s", s->nome);
        } else if (s->estado == E_PARANDO) formatar_texto(msg, (size_t)max, "%s ya se está parando", s->nome);
        else {
            if (s->estado != E_PARADO) { s->detalhe[0] = 0; mudar_estado(s, E_PARADO); anotar("%s: parado a pedido", s->nome); }
            formatar_texto(msg, (size_t)max, "%s está parado", s->nome);
        }
        return 0;
    }
    if (compara_texto(o, "activar") == 0 || compara_texto(o, "desactivar") == 0) {
        bool ligar = o[0] == 'a';
        int r = gravar_activado(s->nome, ligar, !s->usuario);
        if (r < 0) { formatar_texto(msg, (size_t)max, "no se pudo guardar en %s: %s", s->usuario ? SERV_ARQ_ACTIVADOS : ACTIVADOS_SIS,
                                   nome_erro(r)); return 1; }
        s->arrancar = ligar;
        mudou = true;
        anotar("%s: arranque automático %s", s->nome, ligar ? "activado" : "desactivado");
        formatar_texto(msg, (size_t)max, ligar ? "%s arrancará al iniciar el sistema" : "%s ya no arrancará al iniciar el sistema",
                       s->nome);
        if (s->usuario ? !servicos_montado("/home") : !conf_persistente())
            junta_texto(msg, s->usuario ? " (ojo: /home no es persistente, se olvidará al apagar)"
                                        : " (ojo: sin la partición SAGUI-CONF, se olvidará al apagar)", (size_t)max);
        return 0;
    }
    formatar_texto(msg, (size_t)max, "orden «%s» desconocida", o);
    return 1;
}

static void responder(int pid, int codigo, const char *msg)
{
    char tmp[64], res[64];
    formatar_texto(tmp, sizeof tmp, "%s/%d.rtmp", SERV_DIR_PEDIDOS, pid);
    formatar_texto(res, sizeof res, "%s/%d.res", SERV_DIR_PEDIDOS, pid);
    int fd = abrir(tmp, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    imprimir_em(fd, "%d\n%s\n", codigo, msg);
    fechar(fd);
    apagar(res);
    renomear(tmp, res);
}

static void atender_pedidos(void)
{
    char nomes[8][NOME_MAX];
    int n = 0;
    entrada_pasta_t e;
    for (int i = 0; n < 8 && listar(SERV_DIR_PEDIDOS, i, &e) == 1; i++) {
        int t = (int)tam_texto(e.nome);
        if (t > 4 && compara_texto(e.nome + t - 4, ".ped") == 0) copia_texto(nomes[n++], e.nome, NOME_MAX);
    }
    for (int k = 0; k < n; k++) {
        char arq[96];
        formatar_texto(arq, sizeof arq, "%s/%s", SERV_DIR_PEDIDOS, nomes[k]);
        char *b = ler_arquivo(arq);
        apagar(arq);
        int pid = texto_para_int(nomes[k]);
        if (!b) continue;
        char o[16], nome[40];
        int i = 0, j = 0;
        const char *p = b;
        while (*p && *p != ' ' && *p != '\n' && i < 15) o[i++] = *p++;
        o[i] = 0;
        while (*p == ' ') p++;
        while (*p && *p != ' ' && *p != '\n' && j < 39) nome[j++] = *p++;
        nome[j] = 0;
        libera(b);
        char msg[160];
        int r = ordem(o, nome, msg, sizeof msg);
        anotar("orden «%s %s» (pid %d): %s", o, nome, pid, msg);
        responder(pid, r, msg);
    }
}

/* ------------------------------------------------------------ apagado */

static void parar_todo(int motivo)
{
    unsigned inicio = tempo_ms(), fim_total = inicio + APAGADO_MS;
    anotar("el sistema se %s: parando los servicios", motivo == SERV_MOTIVO_REINICIAR ? "reinicia" : "apaga");
    int niv[MAX_SERV], maior = 0;
    for (int i = 0; i < qtd; i++) {
        niv[i] = nivel(i, 0);
        if (niv[i] < 0) niv[i] = 0;
        if (niv[i] > maior) maior = niv[i];
        serv[i].querer = false;
        serv[i].relancar = false;
    }
    /* Primero los que dependen de otros (nivel alto); al final, de los que dependen. */
    for (int n = maior; n >= 0; n--) {
        for (int i = 0; i < qtd; i++)
            if (niv[i] == n && serv[i].estado == E_MARCHA) {
                unsigned resto = fim_total - tempo_ms();
                if ((int)resto < 1000) resto = 1000;
                comecar_parar(&serv[i], serv[i].parar_ms < resto ? serv[i].parar_ms : resto);
            }
        publicar();
        for (;;) {
            recolher_filhos();
            bool falta = false;
            for (int i = 0; i < qtd; i++)
                if (niv[i] == n && serv[i].estado == E_PARANDO) falta = true;
            if (!falta) break;
            avancar();   /* mata a los que agotan su plazo */
            if ((int)(tempo_ms() - (fim_total + 2000)) >= 0) {
                anotar("no todos los servicios terminaron a tiempo; el apagado sigue");
                break;
            }
            dormir(50);
        }
    }
    for (int i = 0; i < qtd; i++)
        if (serv[i].estado != E_PARANDO && serv[i].estado != E_PARADO && serv[i].estado != E_MARCHA) {
            serv[i].detalhe[0] = 0;
            mudar_estado(&serv[i], E_PARADO);
        }
    mudou = true;
    publicar();
    anotar("servicios parados en %u ms", tempo_ms() - inicio);
}

/* ------------------------------------------------------------ arranque */

/* Si el supervisor anterior murió, sus servicios siguen vivos (huérfanos):
 * se terminan para no tenerlos dos veces. Solo si el estado es de este mismo
 * arranque (un /tmp persistente guarda el de la sesión anterior, con PID que
 * ahora son de otros programas) y si el proceso sigue siendo el mismo
 * programa, sin padre. */
static void matar_orfaos(void)
{
    char *b = ler_arquivo(SERV_ARQ_ESTADO);
    if (!b) return;
    unsigned arranque = 0;
    if (comeca_com(b, "servicios ")) {
        const char *p = b + 10;
        while (*p && *p != ' ' && *p != '\n') p++;
        arranque = (unsigned)texto_para_int(p);
    }
    unsigned agora = arranque_utc();
    if (!arranque || (arranque > agora ? arranque - agora : agora - arranque) > 3) { libera(b); return; }
    info_processo_t *ps = aloca(64 * sizeof(info_processo_t));
    int np = ps ? info_sistema(INFO_PROCESSOS, ps, 64) : 0;
    for (char *l = b; l && *l;) {
        char *fim = acha_char(l, '\n');
        if (fim) *fim = 0;
        char *c[11];
        int nc = 0;
        c[nc++] = l;
        for (char *p = l; *p && nc < 11; p++)
            if (*p == '\t') { *p = 0; c[nc++] = p + 1; }
        int pid = nc == 11 ? texto_para_int(c[2]) : 0;
        for (int i = 0; pid > 0 && i < np; i++)
            if (ps[i].pid == pid && ps[i].pai == 0 && ps[i].usuario && ps[i].estado != 4 && pid != meu_pid() &&
                compara_texto(ps[i].nome, c[10]) == 0) {
                anotar("%s: quedó huérfano del supervisor anterior (pid %d): se termina", c[0], pid);
                matar(pid);
            }
        l = fim ? fim + 1 : NULL;
    }
    if (ps) libera(ps);
    libera(b);
}

static void limpar_pedidos(void)
{
    entrada_pasta_t e;
    char arq[96];
    while (listar(SERV_DIR_PEDIDOS, 0, &e) == 1) {
        formatar_texto(arq, sizeof arq, "%s/%s", SERV_DIR_PEDIDOS, e.nome);
        if (apagar(arq) < 0) break;
    }
}

/* ------------------------------------------------------------ contraseñas (D255)
 *
 * Reglas:
 *   - root (quien ya lo es) puede hacerse cualquier cuenta y cambiar cualquier
 *     contraseña sin la actual;
 *   - los demás dan la contraseña de la cuenta de destino. Si la cuenta aún no
 *     tiene contraseña, solo la sesión local (no un servicio: SSH, cron, VNC)
 *     puede fijarla, y esa es la que se usa a partir de entonces (la primera
 *     vez que se usa admin, su o passwd);
 *   - las cuentas con "!" no admiten contraseña (las de servicio);
 *   - tras un fallo, la respuesta se retrasa 2 s; tras 5 seguidos del mismo
 *     usuario, se le niega todo durante un minuto sin mirar la contraseña.
 * Al registro va quién pidió qué y qué se respondió; nunca las contraseñas. */
#define FALHA_ESPERA_MS  2000
#define FALHAS_MAX       5
#define BLOQUEIO_MS      60000
#define SENHA_MIN        4
#define ATRASADAS_MAX    8

static struct { unsigned uid; int falhas; unsigned ate; bool usado; } tentativas[8];
static struct { unsigned id; int res; unsigned quando; bool usado; } atrasadas[ATRASADAS_MAX];

static int *falhas_de(unsigned uid, unsigned **ate)
{
    int livre = -1;
    for (int i = 0; i < 8; i++) {
        if (tentativas[i].usado && tentativas[i].uid == uid) { *ate = &tentativas[i].ate; return &tentativas[i].falhas; }
        if (!tentativas[i].usado && livre < 0) livre = i;
    }
    if (livre < 0) livre = (int)(tempo_ms() % 8);   /* tabla llena: se reutiliza una */
    tentativas[livre].usado = true;
    tentativas[livre].uid = uid;
    tentativas[livre].falhas = 0;
    tentativas[livre].ate = 0;
    *ate = &tentativas[livre].ate;
    return &tentativas[livre].falhas;
}

static void responder_depois(unsigned id, int res)
{
    for (int i = 0; i < ATRASADAS_MAX; i++)
        if (!atrasadas[i].usado) {
            atrasadas[i].usado = true;
            atrasadas[i].id = id;
            atrasadas[i].res = res;
            atrasadas[i].quando = tempo_ms() + FALHA_ESPERA_MS;
            return;
        }
    servicos_kernel3(SERV_RESPONDER, (int)id, res);   /* sin sitio: ya */
}

static void enviar_atrasadas(void)
{
    for (int i = 0; i < ATRASADAS_MAX; i++)
        if (atrasadas[i].usado && (int)(tempo_ms() - atrasadas[i].quando) >= 0) {
            servicos_kernel3(SERV_RESPONDER, (int)atrasadas[i].id, atrasadas[i].res);
            atrasadas[i].usado = false;
        }
}

static int fijar_senha(const conta_t *c, const char *senha, char *det, int max)
{
    if ((int)tam_texto(senha) < SENHA_MIN) { copia_texto(det, "contraseña demasiado corta", (size_t)max); return ERRO_INVALIDO; }
    char h[PBKDF2_TEXTO_MAX];
    int r = pbkdf2_gerar(senha, h, sizeof h);
    if (r == 0) r = sombra_gravar(c->nome, h);
    memset(h, 0, sizeof h);
    formatar_texto(det, (size_t)max, r == 0 ? "contraseña de %s cambiada" : "no se pudo guardar la sombra (%s)", c->nome,
                   nome_erro(r));
    return r == 0 ? 1 : r;
}

/* Un pedido: devuelve lo que se responde (1 = sí) o 0 si la respuesta va con retraso. */
static int tratar_pedido(pedido_recebido_t *p, char *det, int max)
{
    conta_t alvo;
    if (!conta_por_uid(p->uid_alvo, &alvo)) { copia_texto(det, "esa cuenta no existe", (size_t)max); return ERRO_NAO_EXISTE; }
    bool elevar = p->tipo == PED_ELEVAR;
    /* D340: entrar en el escritorio (lo pide el kernel, desde la pantalla de inicio de sesión). */
    bool entrar = p->tipo == PED_ENTRAR && p->pid == 0 && p->uid == PED_UID_NINGUEM;
    if (p->tipo == PED_ENTRAR && !entrar) { copia_texto(det, "pedido de inicio de sesión que no viene del kernel", (size_t)max); return ERRO_ACESSO; }
    if (entrar && (alvo.uid < UID_SAGUI || !alvo.shell[0])) { copia_texto(det, "esa cuenta no entra en el escritorio", (size_t)max); return ERRO_ACESSO; }
    if (!entrar && p->uid == UID_ROOT) {   /* root no necesita contraseñas */
        if (elevar) { copia_texto(det, "ya era root", (size_t)max); return 1; }
        return fijar_senha(&alvo, p->senha_nova, det, max);
    }
    unsigned *ate;
    /* Los fallos al entrar cuentan por la cuenta de destino (todos los pide «nadie»). */
    int *falhas = falhas_de(entrar ? 0x10000u + alvo.uid : p->uid, &ate);
    if (*ate && (int)(tempo_ms() - *ate) < 0) { copia_texto(det, "bloqueado por intentos fallidos", (size_t)max); return ERRO_ESPERA; }
    char h[SOMBRA_TEXTO_MAX] = "";
    int r = sombra_ler(alvo.nome, h, sizeof h);
    unsigned iter;
    unsigned char sal[PBKDF2_SAL], hh[SHA256_TAM];
    bool tem = r > 0 && h[0] && compara_texto(h, "!") != 0;
    if (r > 0 && compara_texto(h, "!") == 0) { copia_texto(det, "la cuenta no admite contraseña", (size_t)max); return ERRO_ACESSO; }
    if (tem && !pbkdf2_valido(h, &iter, sal, hh)) {
        anotar("contraseñas: la línea de %s en la sombra no se entiende: se trata como sin contraseña", alvo.nome);
        tem = false;
    }
    if (r < 0) anotar("contraseñas: no se puede leer la sombra (%s): %s queda sin contraseña", nome_erro(r), alvo.nome);
    /* D340: un administrador (grupo admin) pasa a root con su propia contraseña, si la tiene (como sudo). */
    if (elevar && alvo.uid == UID_ROOT) {
        conta_t quem;
        char hp[SOMBRA_TEXTO_MAX] = "";
        bool propia = conta_por_uid(p->uid, &quem) && conta_admin(quem.nome) && sombra_ler(quem.nome, hp, sizeof hp) > 0 &&
                      pbkdf2_valido(hp, &iter, sal, hh);
        if (propia) {
            if (!p->senha[0]) { memset(hp, 0, sizeof hp); memset(h, 0, sizeof h); copia_texto(det, "falta la contraseña (administrador)", (size_t)max); return ERRO_ACESSO; }
            bool ok = pbkdf2_conferir(p->senha, hp) || (tem && pbkdf2_conferir(p->senha, h));
            memset(hp, 0, sizeof hp);
            memset(h, 0, sizeof h);
            if (ok) { *falhas = 0; *ate = 0; formatar_texto(det, (size_t)max, "contraseña correcta (%s es administrador)", quem.nome); return 1; }
            if (++*falhas >= FALHAS_MAX) {
                *falhas = 0;
                *ate = tempo_ms() + BLOQUEIO_MS;
                if (!*ate) *ate = 1;
                copia_texto(det, "contraseña incorrecta: 5 fallos, bloqueado un minuto", (size_t)max);
            } else copia_texto(det, "contraseña incorrecta", (size_t)max);
            return 0;   /* con retraso */
        }
        memset(hp, 0, sizeof hp);
    }
    if (!tem && entrar) { copia_texto(det, "la cuenta no tiene contraseña", (size_t)max); return 1; }
    if (!tem) {
        /* Sin contraseña: solo la sesión local la fija (la primera vez). */
        if (p->servicio) { copia_texto(det, "sin contraseña y no es la sesión local", (size_t)max); return ERRO_SEM_SENHA; }
        /* Modo en vivo (sin SAGUI-CONF): root sin contraseña para la sesión local. */
        if (elevar && !p->senha_nova[0] && (usuario_op(USU_ESTADO, 0, 0) & USU_EST_EN_VIVO)) {
            copia_texto(det, "modo en vivo: sin contraseña desde la sesión local", (size_t)max);
            return 1;
        }
        if (!p->senha_nova[0]) { copia_texto(det, "sin contraseña: hay que elegir una", (size_t)max); return ERRO_SEM_SENHA; }
        r = fijar_senha(&alvo, p->senha_nova, det, max);
        if (r == 1) anotar("contraseñas: primera contraseña de %s fijada desde la sesión local (uid %u)", alvo.nome, p->uid);
        return r;
    }
    if (!p->senha[0]) { copia_texto(det, "falta la contraseña", (size_t)max); return ERRO_ACESSO; }   /* pregunta: no cuenta como fallo */
    bool ok = pbkdf2_conferir(p->senha, h);
    memset(h, 0, sizeof h);
    if (!ok) {
        if (++*falhas >= FALHAS_MAX) {
            *falhas = 0;
            *ate = tempo_ms() + BLOQUEIO_MS;
            if (!*ate) *ate = 1;
            copia_texto(det, "contraseña incorrecta: 5 fallos, bloqueado un minuto", (size_t)max);
        } else copia_texto(det, "contraseña incorrecta", (size_t)max);
        return 0;   /* con retraso */
    }
    *falhas = 0;
    *ate = 0;
    if (elevar || entrar) { formatar_texto(det, (size_t)max, "contraseña correcta"); return 1; }
    return fijar_senha(&alvo, p->senha_nova, det, max);
}

static void atender_contas(void)
{
    enviar_atrasadas();
    pedido_recebido_t p;
    for (int n = 0; n < 4 && servicos_kernel(SERV_PROXIMO, (int)&p) == 1; n++) {
        char det[96] = "", quem[CONTA_NOME_MAX], alvo[CONTA_NOME_MAX];
        int r = tratar_pedido(&p, det, sizeof det);
        conta_nome(p.uid, quem, sizeof quem);
        conta_nome(p.uid_alvo, alvo, sizeof alvo);
        anotar("%s: pid %d (%s%s) -> %s: %s: %s", p.tipo == PED_ELEVAR ? "ELEVAR" : p.tipo == PED_ENTRAR ? "ENTRAR" : "CONTRASEÑA", p.pid, quem,
               p.servicio ? ", desde un servicio" : "", alvo, r == 1 ? "CONCEDIDO" : "negado", det);
        if (r == 0) responder_depois(p.id, ERRO_ACESSO);
        else servicos_kernel3(SERV_RESPONDER, (int)p.id, r);
        memset(&p, 0, sizeof p);
    }
}

/* Al arrancar: cuentas y sombra en buen estado, y si root tiene contraseña. */
static void revisar_contas(void)
{
    conta_t v[CONTAS_MAX];
    bool danado;
    contas_ler(v, CONTAS_MAX, &danado);
    if (danado) anotar("contraseñas: %s falta o tiene líneas que no valen: se usan las cuentas de serie para lo que falte",
                       CONTAS_ARQ_USUARIOS);
    char h[SOMBRA_TEXTO_MAX] = "";
    int r = sombra_ler("root", h, sizeof h);
    unsigned iter;
    unsigned char sal[PBKDF2_SAL], hh[SHA256_TAM];
    bool tem = r > 0 && pbkdf2_valido(h, &iter, sal, hh);
    memset(h, 0, sizeof h);
    int est = usuario_op(USU_ESTADO, 0, 0);
    if (est & USU_EST_EN_VIVO) {
        anotar("contraseñas: modo en vivo (sin la partición SAGUI-CONF): nada del sistema se guarda; root sin contraseña "
               "desde la sesión local y nunca desde un servicio (SSH...)");
        if (!(est & USU_EST_PRUEBA))
            aviso_tela("Modo en vivo: la configuración del sistema no se guarda y el administrador no tiene contraseña.", NULL);
    } else if (!tem) {
        anotar("contraseñas: root no tiene contraseña: se fija la primera vez que se usa admin, su o passwd en la sesión local");
        /* El aviso en pantalla, solo en la sesión normal (las pruebas no lo esperan). */
        if (!(est & USU_EST_PRUEBA))
            aviso_tela("El administrador (root) no tiene contraseña. Elígela con «passwd root» en el Terminal.", NULL);
    }
}

static const char *AYUDA =
    "uso: servicios\n"
    "El supervisor de los servicios en segundo plano (D216). Lo lanza el sistema al\n"
    "arrancar; no hace falta ejecutarlo a mano. Para manejar los servicios: servicio.\n"
    "Definiciones: /etc/servicios/*.conf y /home/.config/servicios/*.conf.\n";

int principal(int argc, char **argv)
{
    if (argc > 1) {
        if (compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "--help") == 0 ||
            compara_texto(argv[1], "-h") == 0) { escrever_texto(SAIDA, AYUDA); return 0; }
        imprimir_em(ERROS, "servicios: no admite opciones (mira servicios --ayuda)\n");
        return 2;
    }
    int r = servicos_kernel(SERV_REGISTRAR, 0);
    if (r < 0) {
        imprimir_em(ERROS, "servicios: ya hay un supervisor en marcha (pid %d)\n", servicos_kernel(SERV_SUPERVISOR, 0));
        return 1;
    }
    mudar_pasta("/");
    preparar_descritores();
    servicos_criar_pastas(SERV_DIR_PEDIDOS);
    limpar_pedidos();
    anotar("supervisor en marcha (pid %d); registro en %s", meu_pid(), arq_log);
    matar_orfaos();
    revisar_contas();
    carregar(false);
    for (;;) {
        int ap = servicos_kernel(SERV_APAGANDO, 0);
        if (ap > 0) {
            parar_todo(ap);
            if (fd_log >= 0) {   /* los discos se desmontan después: nada abierto */
                fechar(SAIDA);
                fechar(ERROS);
                fechar(fd_log);
                fd_log = -1;
            }
            servicos_kernel(SERV_PRONTO, 0);
            for (;;) dormir(1000);
        }
        recolher_filhos();
        atender_contas();   /* D255 */
        atender_pedidos();
        avancar();
        /* Los que se iban a quitar y ya pararon. */
        for (int i = qtd - 1; i >= 0; i--)
            if (serv[i].remover && serv[i].estado != E_MARCHA && serv[i].estado != E_PARANDO) quitar(i);
        publicar();
        dormir(CICLO_MS);
    }
}
