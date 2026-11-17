// SPDX-License-Identifier: GPL-2.0-or-later
/* bloqueo - el bloqueo de publicidad y rastreo por DNS, al estilo de Pi-hole (D218).
 *
 *   bloqueo [estado]          activado o no, dominios, memoria, consultas y bloqueadas
 *   bloqueo activar           compila la tabla y la carga en el kernel (se recuerda)
 *   bloqueo desactivar        el kernel devuelve la memoria y resuelve todo
 *   bloqueo actualizar        descarga las listas con wget, las comprueba y recarga
 *   bloqueo permitir D        D (y sus subdominios) nunca se bloquean
 *   bloqueo bloquear D        bloquea D a mano
 *   bloqueo quitar D          lo quita de los permitidos y de los bloqueados a mano
 *   bloqueo ultimos           los últimos dominios bloqueados
 *   bloqueo probar D          dice qué pasaría con D, sin contarlo
 *   bloqueo subdominios si|no un dominio de la lista bloquea también sus subdominios
 *   bloqueo listas [agregar URL | quitar N]
 *
 * Todo vive en /home/.config/bloqueo/ (persistente si /home lo es):
 *   bloqueo.conf     activo 0|1, subdominios 0|1 y una línea «lista URL» por lista
 *   listas/N.txt     la lista N descargada (formato hosts)
 *   permitidos.txt, bloqueados.txt   un dominio por línea
 *   tabla.bin        lo que carga el kernel (kernel/inc/bloqueo_tabla.h)
 * Sin ninguna lista descargada se usa la pequeña de ejemplo, /etc/bloqueo/ejemplo.txt,
 * para que funcione sin red. El kernel solo lee bloqueo.conf y tabla.bin: todo
 * el texto que viene de internet se analiza aquí. */
#include "sagui.h"
#include "bloqueo_tabla.h"
#include "bloqueo_hosts.h"
#include "conf_maquina.h"

/* D371: el bloqueo es de la máquina: con SAGUI-CONF, la configuración en /etc/bloqueo.conf y la tabla en
 * SAGUI-CONF (MAQ_BLOQ_TABLA), solo el administrador. Las listas descargadas siguen en la carpeta de quien
 * las baja. El kernel lee de los mismos sitios (bloqueo.c). */
static const char *arq_conf(void) { return maquina_separada() ? "/etc/bloqueo.conf" : BLOQ_CONF; }
static const char *arq_tabla(void) { return maquina_separada() ? MAQ_BLOQ_TABLA : BLOQ_TABLA; }

#define LISTAS_DIR   BLOQ_DIR "/listas"
#define PERMITIDOS   BLOQ_DIR "/permitidos.txt"
#define BLOQUEADOS   BLOQ_DIR "/bloqueados.txt"
#define EJEMPLO      "/etc/bloqueo/ejemplo.txt"
#define LISTA_DEF    "https://raw.githubusercontent.com/StevenBlack/hosts/master/hosts"
#define MAX_LISTAS   8
#define LISTA_MAX_BYTES (32u * 1024 * 1024)   /* la de StevenBlack anda por 3,5 MB */

/* ------------------------------------------------------------ bloqueo.conf */

static struct {
    int  activo, subdominios;
    int  nlistas;
    char listas[MAX_LISTAS][256];
} conf;

static void conf_ler(void)
{
    conf.activo = 0;
    conf.subdominios = 1;
    conf.nlistas = 0;
    int fd = abrir(arq_conf(), ABRIR_LER);
    if (fd < 0) {                               /* nunca se configuró: la lista por defecto */
        copia_texto(conf.listas[0], LISTA_DEF, sizeof conf.listas[0]);
        conf.nlistas = 1;
        return;
    }
    char l[300];
    while (ler_linha(fd, l, sizeof l) >= 0) {
        if (comeca_com(l, "activo ")) conf.activo = l[7] == '1';
        else if (comeca_com(l, "subdominios ")) conf.subdominios = l[12] == '1';
        else if (comeca_com(l, "lista ") && conf.nlistas < MAX_LISTAS && l[6])
            copia_texto(conf.listas[conf.nlistas++], l + 6, sizeof conf.listas[0]);
    }
    fechar(fd);
}

static int conf_gravar(void)
{
    static char t[300 * (MAX_LISTAS + 1)];
    int n = formatar_texto(t, sizeof t, "# Bloqueo de publicidad y rastreo por DNS (D218). Lo escribe el programa «bloqueo».\n"
                    "# activo 1 = el kernel filtra los nombres; subdominios 1 = ejemplo.com bloquea ads.ejemplo.com.\n"
                    "activo %d\nsubdominios %d\n", conf.activo, conf.subdominios);
    for (int i = 0; i < conf.nlistas; i++) n += formatar_texto(t + n, sizeof t - (unsigned)n, "lista %s\n", conf.listas[i]);
    if (maquina_separada()) return maquina_gravar_arquivo("bloqueo.conf", t, n, "cambiar el bloqueo de publicidad");
    criar_pasta("/home/.config");
    criar_pasta(BLOQ_DIR);
    int fd = abrir(BLOQ_CONF, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int r = escrever_tudo(fd, t, (unsigned)n);
    fechar(fd);
    return r < 0 ? r : 0;
}

static void caminho_lista(char *s, int max, int i, const char *ext)
{
    formatar_texto(s, (size_t)max, "%s/%d.%s", LISTAS_DIR, i + 1, ext);
}

/* ------------------------------------------------------------ leer listas */

typedef struct {
    uint64_t *v;
    uint32_t  n, cap;
    bool      sem_memoria;
} vetor_t;

static void vetor_por(void *ctx, const char *d, int n)
{
    vetor_t *v = ctx;
    if (v->sem_memoria) return;
    if (v->n == v->cap) {
        uint32_t c = v->cap ? v->cap * 2 : 4096;
        uint64_t *nv = realoca(v->v, c * sizeof(uint64_t));
        if (!nv) { v->sem_memoria = true; return; }
        v->v = nv;
        v->cap = c;
    }
    v->v[v->n++] = bloqueo_hash(d, (uint32_t)n);
}

static void contar_so(void *ctx, const char *d, int n) { (void)d; (void)n; (*(uint32_t *)ctx)++; }

/* Analiza un archivo por bloques (ler_linha lee de a un byte: demasiado
 * lento para 3 MB). Devuelve 0, o <0 si no se pudo abrir. */
static int analisar(const char *caminho, bh_saida_t saida, void *ctx, uint32_t *boas, uint32_t *mas, uint32_t *bytes)
{
    static char bloco[65536];
    static char linha[1024];
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return fd;
    uint32_t nl = 0, total = 0;
    bool longa = false;
    *boas = *mas = 0;
    for (;;) {
        int r = ler(fd, bloco, sizeof bloco);
        if (r <= 0) break;
        total += (uint32_t)r;
        for (int i = 0; i < r; i++) {
            char c = bloco[i];
            if (c != '\n') {
                if (nl < sizeof linha) linha[nl++] = c; else longa = true;
                continue;
            }
            if (longa) (*mas)++;
            else {
                int k = bh_linha(linha, (int)nl, saida, ctx);
                if (k > 0) *boas += (uint32_t)k; else if (k < 0) (*mas)++;
            }
            nl = 0;
            longa = false;
        }
    }
    if (nl && !longa) {                          /* la última línea, sin '\n' */
        int k = bh_linha(linha, (int)nl, saida, ctx);
        if (k > 0) *boas += (uint32_t)k; else if (k < 0) (*mas)++;
    }
    fechar(fd);
    if (bytes) *bytes = total;
    return 0;
}

/* ¿Parece una lista? Comprueba el tamaño y el formato antes de usarla: que
 * no sea una página HTML de error, que tenga dominios y que no haya más líneas
 * malas que buenas. Devuelve los dominios o 0 (y explica por qué en «porque»). */
static uint32_t verificar_lista(const char *caminho, char *porque, int max)
{
    info_arquivo_t inf;
    if (info_arquivo(caminho, &inf) < 0) { copia_texto(porque, "no se descargó", (size_t)max); return 0; }
    if (inf.tamanho == 0) { copia_texto(porque, "está vacía", (size_t)max); return 0; }
    if (inf.tamanho > LISTA_MAX_BYTES) { formatar_texto(porque, (size_t)max, "es demasiado grande (%u MB)", inf.tamanho >> 20); return 0; }
    char ini[64];
    int fd = abrir(caminho, ABRIR_LER);
    int n = fd >= 0 ? ler(fd, ini, sizeof ini - 1) : 0;
    if (fd >= 0) fechar(fd);
    ini[n > 0 ? n : 0] = 0;
    for (int i = 0; i < n; i++)
        if (ini[i] == '<') { copia_texto(porque, "parece una página HTML, no una lista", (size_t)max); return 0; }
    uint32_t dominios = 0, boas, mas;
    analisar(caminho, contar_so, &dominios, &boas, &mas, NULL);
    if (!dominios) { copia_texto(porque, "no tiene ningún dominio", (size_t)max); return 0; }
    if (mas > boas) { formatar_texto(porque, (size_t)max, "tiene más líneas malas (%u) que buenas (%u)", mas, boas); return 0; }
    if (dominios > BLOQ_MAX_ENTRADAS) { formatar_texto(porque, (size_t)max, "tiene demasiados dominios (%u)", dominios); return 0; }
    return dominios;
}

/* ------------------------------------------------------------ compilar la tabla */

static int gravar_tudo(int fd, const void *p, uint32_t n)
{
    const char *c = p;
    while (n) {
        uint32_t t = n > 65536 ? 65536 : n;
        int r = escrever(fd, c, t);
        if (r <= 0) return -1;
        c += r;
        n -= (uint32_t)r;
    }
    return 0;
}

/* Lee las fuentes y escribe tabla.bin. Devuelve 0 o <0 (con el mensaje dicho). */
static int compilar(bool falar)
{
    vetor_t vb = { 0 }, vp = { 0 };
    uint32_t boas, mas, fontes = 0;
    unsigned t0 = tempo_ms();
    for (int i = 0; i < conf.nlistas; i++) {
        char c[96];
        caminho_lista(c, sizeof c, i, "txt");
        if (analisar(c, vetor_por, &vb, &boas, &mas, NULL) == 0) fontes++;
    }
    if (!fontes && analisar(EJEMPLO, vetor_por, &vb, &boas, &mas, NULL) == 0 && falar)
        imprimir("Sin listas descargadas: se usa la de ejemplo (%s, %u dominios). Para la completa: bloqueo actualizar\n", EJEMPLO, boas);
    analisar(BLOQUEADOS, vetor_por, &vb, &boas, &mas, NULL);
    analisar(PERMITIDOS, vetor_por, &vp, &boas, &mas, NULL);
    if (vb.sem_memoria || vp.sem_memoria) {
        imprimir_em(ERROS, "bloqueo: no hay memoria para las listas\n");
        libera(vb.v); libera(vp.v);
        return ERRO_SEM_MEMORIA;
    }
    uint32_t nb = bh_ordenar_unicos(vb.v, vb.n), np = bh_ordenar_unicos(vp.v, vp.n);
    if (nb + np > BLOQ_MAX_ENTRADAS) {
        imprimir_em(ERROS, "bloqueo: demasiados dominios (%u; el máximo es %u)\n", nb + np, BLOQ_MAX_ENTRADAS);
        libera(vb.v); libera(vp.v);
        return ERRO_LIMITE;
    }
    bloqueo_cabecera_t cab;
    memset(&cab, 0, sizeof cab);
    memcpy(cab.magia, BLOQ_MAGIA, 8);
    cab.versao = 1;
    cab.n_bloq = nb;
    cab.n_perm = np;
    const char *tmp = maquina_separada() ? MAQ_BLOQ_TABLA ".tmp" : BLOQ_DIR "/tabla.tmp";
    if (maquina_separada()) criar_pasta("/sistema/conf/bloqueo");
    else { criar_pasta("/home/.config"); criar_pasta(BLOQ_DIR); }
    int fd = abrir(tmp, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    int r = fd < 0 ? fd : 0;
    if (r == 0 && (gravar_tudo(fd, &cab, sizeof cab) < 0 || gravar_tudo(fd, vb.v, nb * 8u) < 0 ||
                   gravar_tudo(fd, vp.v, np * 8u) < 0)) r = ERRO_LIMITE;
    if (fd >= 0) fechar(fd);
    libera(vb.v); libera(vp.v);
    if (r == 0) {                                /* se cambia de una vez: nunca una tabla a medias */
        apagar(arq_tabla());
        r = renomear(tmp, arq_tabla());
    }
    if (r < 0) {
        apagar(tmp);
        imprimir_em(ERROS, "bloqueo: no se pudo escribir %s: %s\n", arq_tabla(), nome_erro(r));
        return r;
    }
    if (falar) imprimir("Tabla: %u dominios bloqueados y %u permitidos (%u KB, %u ms).\n",
                        nb, np, (unsigned)((sizeof cab + 8u * (nb + np) + 1023) / 1024), tempo_ms() - t0);
    return 0;
}

static int recarregar(void)
{
    int r = bloqueo_dns(BLOQ_RECARGAR, 0, 0);
    if (r < 0) imprimir_em(ERROS, "bloqueo: el kernel no cargó la tabla: %s\n", nome_erro(r));
    return r;
}

/* Tras cambiar algo: si está activado, la tabla nueva entra ya. */
static int aplicar(void)
{
    if (!conf.activo) return 0;
    if (compilar(false) < 0) return 1;
    return recarregar() < 0 ? 1 : 0;
}

/* ------------------------------------------------------------ órdenes */

static int estado(void)
{
    conf_ler();
    info_bloqueo_t i;
    if (bloqueo_dns(BLOQ_ESTADO, &i, sizeof i) < 0) { imprimir_em(ERROS, "bloqueo: el kernel no lo admite\n"); return 1; }
    imprimir("Bloqueo de publicidad y rastreo por DNS: %s\n", i.activo ? "activado" : "desactivado");
    if (i.pedido && !i.activo) imprimir("  Pedido en bloqueo.conf, pero el kernel no tiene tabla (error: %s).\n", nome_erro(i.erro));
    if (i.activo) {
        imprimir("  Tabla:        %u dominios bloqueados, %u permitidos (%u KB en el kernel)\n",
                 i.bloqueados, i.permitidos, (i.bytes + 1023) / 1024);
        if (i.erro) imprimir("  Aviso:        la última recarga falló (%s): sigue la tabla anterior\n", nome_erro(i.erro));
    }
    imprimir("  Subdominios:  %s\n", conf.subdominios ? "sí (ejemplo.com bloquea también ads.ejemplo.com)" : "no (solo el nombre exacto)");
    unsigned pct = i.consultas ? (unsigned)((uint64_t)i.bloqueadas * 100 / i.consultas) : 0;
    imprimir("  Consultas:    %u, bloqueadas %u (%u %%)\n", i.consultas, i.bloqueadas, pct);
    bool alguma = false;
    for (int k = 0; k < conf.nlistas; k++) {
        char c[96];
        caminho_lista(c, sizeof c, k, "txt");
        info_arquivo_t inf;
        bool tem = info_arquivo(c, &inf) == 0;
        alguma |= tem;
        if (tem) imprimir("  Lista %d:      %s (descargada, %u KB)\n", k + 1, conf.listas[k], (inf.tamanho + 1023) / 1024);
        else imprimir("  Lista %d:      %s (sin descargar)\n", k + 1, conf.listas[k]);
    }
    if (!alguma) imprimir("  Sin listas descargadas se usa la de ejemplo (%s): bloqueo actualizar\n", EJEMPLO);
    return 0;
}

static int activar(bool sim)
{
    conf_ler();
    conf.activo = sim;
    if (sim && compilar(true) < 0) return 1;
    int g = conf_gravar();
    if (g < 0) { imprimir_em(ERROS, "bloqueo: no se pudo guardar %s: %s\n", arq_conf(), nome_erro(g)); return 1; }
    if (recarregar() < 0) return 1;
    if (!sim) { imprimir("Bloqueo desactivado: se resuelven todos los nombres.\n"); return 0; }
    info_bloqueo_t i;
    bloqueo_dns(BLOQ_ESTADO, &i, sizeof i);
    imprimir("Bloqueo activado: %u dominios en el kernel (%u KB, cargados en %u us).\n",
             i.bloqueados, (i.bytes + 1023) / 1024, i.carga_us);
    return 0;
}

static int actualizar(void)
{
    conf_ler();
    if (!conf.nlistas) { imprimir_em(ERROS, "bloqueo: no hay listas (bloqueo listas agregar URL)\n"); return 1; }
    criar_pasta("/home/.config");
    criar_pasta(BLOQ_DIR);
    criar_pasta(LISTAS_DIR);
    int bem = 0;
    for (int i = 0; i < conf.nlistas; i++) {
        char tmp[96], fin[96], porque[96];
        caminho_lista(tmp, sizeof tmp, i, "tmp");
        caminho_lista(fin, sizeof fin, i, "txt");
        apagar(tmp);
        imprimir("Lista %d: %s\n", i + 1, conf.listas[i]);
        char *argv[] = { "wget", "-q", "-O", tmp, conf.listas[i], NULL };
        int pid = executar("/bin/wget", argv);
        int cod = -1;
        if (pid < 0 || esperar(pid, &cod) < 0 || cod != 0) {
            imprimir_em(ERROS, "  no se pudo descargar (wget terminó con %d): se queda la anterior\n", cod);
            apagar(tmp);
            continue;
        }
        uint32_t n = verificar_lista(tmp, porque, sizeof porque);
        if (!n) {
            imprimir_em(ERROS, "  la descarga %s: no se usa (se queda la anterior)\n", porque);
            apagar(tmp);
            continue;
        }
        apagar(fin);
        if (renomear(tmp, fin) < 0) { imprimir_em(ERROS, "  no se pudo guardar %s\n", fin); continue; }
        imprimir("  bien: %u dominios\n", n);
        bem++;
    }
    if (!bem) return 1;
    if (compilar(true) < 0) return 1;
    if (conf.activo) {
        if (recarregar() < 0) return 1;
        imprimir("Cargada en el kernel.\n");
    } else imprimir("El bloqueo está desactivado: bloqueo activar para usarla.\n");
    return 0;
}

/* Valida un dominio escrito por el usuario y lo deja en minúsculas, sin el punto final. */
static bool dominio_do_usuario(const char *s, char *o, int max)
{
    int n = (int)tam_texto(s);
    if (n > 0 && s[n - 1] == '.') n--;
    if (n >= max || !bh_dominio_valido(s, n)) return false;
    for (int i = 0; i < n; i++) o[i] = (char)bh_minuscula((unsigned char)s[i]);
    o[n] = 0;
    return true;
}

/* Quita «d» de un archivo de un dominio por línea; devuelve si estaba. */
static bool tirar_de(const char *caminho, const char *d)
{
    static char txt[65536];
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return false;
    int n = ler(fd, txt, sizeof txt - 1);
    fechar(fd);
    if (n <= 0 || n >= (int)sizeof txt - 1) return false;   /* demasiado grande para tocarlo aquí: se deja como está */
    txt[n] = 0;
    fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return false;
    bool estava = false;
    for (char *l = txt; *l;) {
        char *f = acha_char(l, '\n');
        if (f) *f = 0;
        if (compara_texto(l, d) == 0) estava = true;
        else if (*l) imprimir_em(fd, "%s\n", l);
        if (!f) break;
        l = f + 1;
    }
    fechar(fd);
    return estava;
}

static bool tem_em(const char *caminho, const char *d)
{
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return false;
    char l[300];
    bool achou = false;
    while (!achou && ler_linha(fd, l, sizeof l) >= 0) achou = compara_texto(l, d) == 0;
    fechar(fd);
    return achou;
}

static int por_em(const char *caminho, const char *d)
{
    criar_pasta("/home/.config");
    criar_pasta(BLOQ_DIR);
    if (tem_em(caminho, d)) return 0;
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_ACRESCENTAR);
    if (fd < 0) return fd;
    imprimir_em(fd, "%s\n", d);
    fechar(fd);
    return 0;
}

static int lista_propia(const char *ordem, const char *arg)
{
    char d[256];
    if (!dominio_do_usuario(arg, d, sizeof d)) { imprimir_em(ERROS, "bloqueo: dominio no válido: %s\n", arg); return 2; }
    conf_ler();
    int r = 0;
    if (!compara_texto(ordem, "permitir")) {
        tirar_de(BLOQUEADOS, d);
        r = por_em(PERMITIDOS, d);
        if (r == 0) imprimir("Permitido: %s (y sus subdominios)\n", d);
    } else if (!compara_texto(ordem, "bloquear")) {
        tirar_de(PERMITIDOS, d);
        r = por_em(BLOQUEADOS, d);
        if (r == 0) imprimir("Bloqueado a mano: %s\n", d);
    } else {
        bool a = tirar_de(PERMITIDOS, d), b = tirar_de(BLOQUEADOS, d);
        if (!a && !b) { imprimir("%s no estaba en los permitidos ni en los bloqueados a mano.\n", d); return 1; }
        imprimir("Quitado: %s\n", d);
    }
    if (r < 0) { imprimir_em(ERROS, "bloqueo: no se pudo guardar: %s\n", nome_erro(r)); return 1; }
    return aplicar();
}

static int ultimos(void)
{
    static char u[16][64];
    int n = bloqueo_dns(BLOQ_ULTIMOS, u, 16);
    if (n < 0) { imprimir_em(ERROS, "bloqueo: %s\n", nome_erro(n)); return 1; }
    if (!n) { imprimir("Aún no se ha bloqueado nada.\n"); return 0; }
    imprimir("Últimos dominios bloqueados (el más reciente primero):\n");
    for (int i = 0; i < n; i++) imprimir("  %s\n", u[i]);
    return 0;
}

static int probar(const char *arg)
{
    char d[256];
    if (!dominio_do_usuario(arg, d, sizeof d)) { imprimir_em(ERROS, "bloqueo: dominio no válido: %s\n", arg); return 2; }
    info_bloqueo_t i;
    bloqueo_dns(BLOQ_ESTADO, &i, sizeof i);
    if (!i.activo) { imprimir("El bloqueo está desactivado: %s se resuelve.\n", d); return 0; }
    int r = bloqueo_dns(BLOQ_CONSULTAR, d, 0);
    imprimir("%s: %s\n", d, r == BLOQ_BLOQUEADO ? "bloqueado" : r == BLOQ_PERMITIDO ? "permitido" : r < 0 ? nome_erro(r) : "libre");
    return 0;
}

static int listas(int argc, char **argv)
{
    conf_ler();
    if (argc >= 4 && !compara_texto(argv[2], "agregar")) {
        if (conf.nlistas >= MAX_LISTAS) { imprimir_em(ERROS, "bloqueo: como mucho %d listas\n", MAX_LISTAS); return 1; }
        if (!comeca_com(argv[3], "http://") && !comeca_com(argv[3], "https://")) {
            imprimir_em(ERROS, "bloqueo: la dirección tiene que empezar por http:// o https://\n");
            return 2;
        }
        copia_texto(conf.listas[conf.nlistas++], argv[3], sizeof conf.listas[0]);
        if (conf_gravar() < 0) return 1;
        imprimir("Lista %d agregada: bloqueo actualizar para descargarla.\n", conf.nlistas);
        return 0;
    }
    if (argc >= 4 && !compara_texto(argv[2], "quitar")) {
        int k = texto_para_int(argv[3]) - 1;
        if (k < 0 || k >= conf.nlistas) { imprimir_em(ERROS, "bloqueo: no hay lista %s\n", argv[3]); return 2; }
        /* las descargadas van por número: se corren las de detrás */
        char a[96], b[96];
        caminho_lista(a, sizeof a, k, "txt");
        apagar(a);
        for (int i = k + 1; i < conf.nlistas; i++) {
            copia_texto(conf.listas[i - 1], conf.listas[i], sizeof conf.listas[0]);
            caminho_lista(a, sizeof a, i, "txt");
            caminho_lista(b, sizeof b, i - 1, "txt");
            renomear(a, b);
        }
        conf.nlistas--;
        if (conf_gravar() < 0) return 1;
        imprimir("Lista quitada.\n");
        return aplicar();
    }
    if (argc > 2) return -1;
    if (!conf.nlistas) imprimir("No hay listas configuradas (se usa la de ejemplo, %s).\n", EJEMPLO);
    for (int i = 0; i < conf.nlistas; i++) imprimir("%d. %s\n", i + 1, conf.listas[i]);
    return 0;
}

static int uso(int cod)
{
    imprimir_em(cod ? ERROS : SAIDA,
        "uso: bloqueo [estado | activar | desactivar | actualizar | ultimos]\n"
        "     bloqueo permitir|bloquear|quitar|probar DOMINIO\n"
        "     bloqueo subdominios si|no\n"
        "     bloqueo listas [agregar URL | quitar N]\n"
        "Bloqueo de publicidad y rastreo por DNS (como Pi-hole): los dominios de las\n"
        "listas responden 0.0.0.0 a todos los programas. Desactivado por defecto.\n"
        "Los datos están en " BLOQ_DIR "/.\n");
    return cod;
}

int principal(int argc, char **argv)
{
    if (argc < 2 || !compara_texto(argv[1], "estado")) return estado();
    const char *o = argv[1];
    if (!compara_texto(o, "--ayuda") || !compara_texto(o, "--help") || !compara_texto(o, "-h")) return uso(0);
    if (!compara_texto(o, "activar")) return activar(true);
    if (!compara_texto(o, "desactivar")) return activar(false);
    if (!compara_texto(o, "actualizar")) return actualizar();
    if (!compara_texto(o, "ultimos") || !compara_texto(o, "últimos")) return ultimos();
    if (argc == 3 && (!compara_texto(o, "permitir") || !compara_texto(o, "bloquear") || !compara_texto(o, "quitar")))
        return lista_propia(o, argv[2]);
    if (argc == 3 && !compara_texto(o, "probar")) return probar(argv[2]);
    if (argc == 3 && !compara_texto(o, "subdominios")) {
        conf_ler();
        if (!compara_texto(argv[2], "si") || !compara_texto(argv[2], "sí")) conf.subdominios = 1;
        else if (!compara_texto(argv[2], "no")) conf.subdominios = 0;
        else return uso(2);
        if (conf_gravar() < 0) return 1;
        imprimir("Subdominios: %s\n", conf.subdominios ? "sí" : "no");
        if (!conf.activo) return 0;
        return recarregar() < 0 ? 1 : 0;
    }
    if (!compara_texto(o, "listas")) {
        int r = listas(argc, argv);
        return r < 0 ? uso(2) : r;
    }
    return uso(2);
}
