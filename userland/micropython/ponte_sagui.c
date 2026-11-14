/* ponte_sagui.c - libtls vista desde MicroPython (D069): TLS sobre un
 * socket ya conectado, el almacén de raíces y el SHA-384/512 de hashlib.
 *
 * Compilado como un programa común de SaguiOS (sagui.h, textos en
 * Latin-1); la interfaz, solo con tipos básicos, está en ponte_sagui.h. Los
 * textos de error salen como los escribe libtls (en español, Latin-1) y el
 * ssl_sagui.c los convierte a str. */
#include "tls.h"
#include "imagem.h"
#include "ponte_sagui.h"

/* ------------------------------------------------------------ raíces */

void *ponte_raizes_sistema(void)
{
    static almacen_t sistema;
    static bool carregado;
    if (!carregado) {
        almacen_carregar(&sistema);
        carregado = true;
    }
    return &sistema;
}

/* Cada contexto con raíces propias (cafile/cadata) tiene su copia del
 * almacén. Los certificados copiados del sistema apuntan al DER del
 * almacén del sistema, que nunca se libera; los sumados aquí tienen el DER
 * asignado uno a uno y se liberan con el almacén. */
typedef struct {
    almacen_t a;
    int do_sistema;   /* los primeros "do_sistema" no son nuestros */
} raizes_t;

void *ponte_raizes_novo(int com_sistema)
{
    raizes_t *r = aloca_zerado(sizeof *r);
    if (!r) return NULL;
    if (com_sistema) {
        r->a = *(almacen_t *)ponte_raizes_sistema();
        r->do_sistema = r->a.n;
    }
    return r;
}

static int somar_texto(raizes_t *r, const char *dados, unsigned n, const char *origem)
{
    int achados = 0;
    unsigned pos = 0;
    bool pem = false;
    for (unsigned i = 0; i + 10 < n && !pem; i++) pem = dados[i] == '-' && comeca_com(dados + i, "-----BEGIN");
    while (r->a.n < RAIZES_MAX) {
        uint8_t *der = aloca(n + 1);
        if (!der) break;
        int k;
        if (pem) k = pem_proximo(dados, n, &pos, der, n);
        else { k = pos ? 0 : (int)n; memcpy(der, dados, n); pos = n; }   /* un solo DER */
        if (k <= 0) { libera(der); break; }
        cert_t *c = &r->a.c[r->a.n];
        if (x509_ler(c, der, (unsigned)k) != X509_OK) { libera(der); if (!pem) break; continue; }
        copia_texto(r->a.arquivo[r->a.n], origem, sizeof r->a.arquivo[0]);
        r->a.n++;
        achados++;
        if (!pem) break;
    }
    return achados;
}

int ponte_raizes_somar(void *raizes, const char *dados, unsigned n)
{
    return somar_texto(raizes, dados, n, "cadata");
}

int ponte_raizes_somar_arquivo(void *raizes, const char *caminho)
{
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return fd;
    unsigned cap = 8192, n = 0;
    char *texto = aloca(cap);
    int k;
    while (texto && (k = ler(fd, texto + n, cap - n)) > 0) {
        n += (unsigned)k;
        if (n == cap) { cap *= 2; texto = realoca(texto, cap); }
    }
    fechar(fd);
    if (!texto) return ERRO_SEM_MEMORIA;
    int r = somar_texto(raizes, texto, n, caminho);
    libera(texto);
    return r;
}

int ponte_raizes_quantas(const void *raizes) { return ((const almacen_t *)raizes)->n; }

void ponte_raizes_liberar(void *raizes)
{
    raizes_t *r = raizes;
    if (!r || raizes == ponte_raizes_sistema()) return;
    for (int i = r->do_sistema; i < r->a.n; i++) libera((void *)r->a.c[i].der);
    libera(r);
}

/* ------------------------------------------------------------ TLS */

void *ponte_tls_abrir(int fd, const char *host, int opcoes, void *raizes, char *erro, unsigned max, int *cert)
{
    tls_config_t cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.host = host;
    cfg.raizes = raizes ? raizes : ponte_raizes_sistema();
    /* Sin comprobar el nombre (check_hostname=False), libtls valida en modo
     * "inseguro" y la cadena se comprueba aquí de nuevo sin el nombre. Así el
     * SNI sigue yendo al servidor. */
    cfg.inseguro = !(opcoes & PONTE_VERIFICAR) || !(opcoes & PONTE_CONFERIR_NOME);
    *cert = 0;
    tls_t *t = tls_novo(&cfg, fd);
    if (!t) { copia_texto(erro, "sin memoria para TLS", max); return NULL; }
    int r = tls_conectar(t);
    if (r == 0 && (opcoes & PONTE_VERIFICAR) && !(opcoes & PONTE_CONFERIR_NOME)) {
        const cert_t *c;
        int n = tls_cadeia(t, &c);
        int v = x509_validar(c, n, cfg.raizes, NULL, data_agora(), NULL);
        if (v != X509_OK) {
            copia_texto(erro, x509_erro_texto(v), max);
            *cert = 1;
            tls_fechar(t);
            return NULL;
        }
    }
    if (r < 0) {
        copia_texto(erro, tls_erro(t), max);
        *cert = r == TLS_ERRO_CERT;
        tls_fechar(t);
        return NULL;
    }
    return t;
}

int  ponte_tls_ler(void *t, void *buf, unsigned n) { return tls_ler(t, buf, n); }
int  ponte_tls_escrever(void *t, const void *buf, unsigned n) { return tls_escrever(t, buf, n); }
int  ponte_tls_pendente(void *t) { return tls_pendente(t); }
void ponte_tls_fechar(void *t) { tls_fechar(t); }
const char *ponte_tls_erro(void *t) { return tls_erro(t); }
const char *ponte_tls_versao(void *t) { return tls_versao_texto(t); }
const char *ponte_tls_suite(void *t) { return tls_suite_texto(t); }

int ponte_tls_certificado(void *t, const unsigned char **der, char *sujeito, unsigned max)
{
    const cert_t *c;
    if (tls_cadeia(t, &c) < 1) return 0;
    *der = c[0].der;
    x509_nome_texto(c[0].sujeito, sujeito, max);
    return (int)c[0].nder;
}

/* ------------------------------------------------------------ SHA-384/512 */

typedef char confere_tamanho[sizeof(sha512_t) <= PONTE_HASH_CTX ? 1 : -1];

void ponte_sha512_iniciar(void *ctx, int bits)
{
    if (bits == 384) sha384_iniciar(ctx);
    else sha512_iniciar(ctx);
}
void ponte_sha512_somar(void *ctx, const void *d, unsigned n) { sha512_somar(ctx, d, n); }
void ponte_sha512_fim(void *ctx, unsigned char *saida) { sha512_fim(ctx, saida); }

/* ------------------------------------------------------------ hora (D080) */

/* El time de MicroPython no puede incluir sagui.h (ver sagui_mp.h): la
 * conversión UTC <-> hora local de la zona del proceso (TZ o la del sistema)
 * pasa por aquí, en campos simples. c: año, mes, día, hora, minuto,
 * segundo, día de la semana (0 = lunes, como el time de Python), día del año
 * (1..366), horario de verano (0/1). */
static void ponte_campos(const fuso_data_t *d, int c[9])
{
    c[0] = d->ano; c[1] = d->mes; c[2] = d->dia;
    c[3] = d->hora; c[4] = d->minuto; c[5] = d->segundo;
    c[6] = (d->dia_semana + 6) % 7;
    c[7] = d->dia_ano + 1;
    c[8] = d->verao ? 1 : 0;
}

unsigned ponte_hora_utc(unsigned *ms)
{
    hora_t h;
    uint32_t s = hora_utc(&h);
    if (ms) *ms = h.ms;
    return s;
}

void ponte_hora_local(unsigned utc, int c[9])
{
    fuso_data_t d;
    hora_local(utc, &d);
    ponte_campos(&d, c);
}

void ponte_hora_utc_campos(unsigned utc, int c[9])
{
    fuso_data_t d;
    fuso_decompor(utc, &d);
    ponte_campos(&d, c);
}

unsigned ponte_local_para_utc(int ano, int mes, int dia, int hora, int minuto, int segundo)
{
    return local_para_utc(fuso_compor(ano, mes, dia, hora, minuto, segundo), NULL);
}

/* ------------------------------------------------------------ imágenes (módulo sagui) */

int ponte_imagem_ler(const char *caminho, int *larg, int *alt, unsigned **px, char *formato, unsigned max)
{
    imagem_t img;
    int r = imagem_ler(caminho, &img);
    if (r != 0) return r;
    *larg = img.larg;
    *alt = img.alt;
    *px = img.px;
    copia_texto(formato, img.formato, max);
    return 0;
}

int ponte_imagem_gravar(const char *caminho, int larg, int alt, const unsigned *px, int ppm)
{
    imagem_t img = { larg, alt, (uint32_t *)px, "" };
    return ppm ? imagem_gravar_ppm(caminho, &img) : imagem_gravar_bmp(caminho, &img);
}

void ponte_imagem_desenhar(unsigned *dst, int larg_dst, int rx, int ry, int rl, int ra,
                           int l_img, int a_img, const unsigned *px, int ox, int oy, int l, int a, int suave)
{
    imagem_t img = { l_img, a_img, (uint32_t *)px, "" };
    imagem_desenhar(dst, larg_dst, rx, ry, rl, ra, &img, ox, oy, l, a, suave);
}

const char *ponte_imagem_erro(int erro) { return imagem_erro(erro); }
