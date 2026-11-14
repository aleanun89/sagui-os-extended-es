// SPDX-License-Identifier: GPL-2.0-or-later
/* x509.c - leitura de certificados X.509 (DER e PEM) e validação da cadeia
 * (RFC 5280, partes 4 e 6, só o que um cliente HTTPS precisa). D062.
 *
 * O DER é lido no lugar: cert_t só guarda ponteiros para dentro do buffer
 * do certificado. Todo comprimento lido é comparado com o que sobra antes
 * de ser usado, e os laços andam sempre para a frente: um certificado
 * malicioso não lê fora do buffer nem trava o programa.
 *
 * Fica de fora: revogação (CRL, OCSP), nameConstraints e políticas (uma
 * extensão crítica desconhecida recusa o certificado, como manda o RFC),
 * certificados com chave RSA-PSS ou DSA. Ver D062. */
#include "tls.h"

/* ------------------------------------------------------------ DER */

typedef struct { const uint8_t *p, *fim; } der_t;

#define T_BOOL     0x01
#define T_INTEIRO  0x02
#define T_BITS     0x03
#define T_OCTETOS  0x04
#define T_NULO     0x05
#define T_OID      0x06
#define T_UTF8     0x0c
#define T_PRINT    0x13
#define T_T61      0x14
#define T_IA5      0x16
#define T_UTC      0x17
#define T_GEN      0x18
#define T_BMP      0x1e
#define T_SEQ      0x30
#define T_SET      0x31

/* Lê o próximo elemento: tag, conteúdo e (opcional) o elemento inteiro. */
static bool der_ler(der_t *d, int *tag, fatia_t *cont, fatia_t *todo)
{
    const uint8_t *ini = d->p;
    if (d->fim - d->p < 2) return false;
    int t = *d->p++;
    if ((t & 0x1f) == 0x1f) return false;   /* tags de vários bytes: não aparecem em certificados */
    unsigned n = *d->p++;
    if (n & 0x80) {
        unsigned k = n & 0x7f;
        if (k == 0 || k > 3 || (unsigned)(d->fim - d->p) < k) return false;   /* sem tamanho indefinido; até 16 MB */
        n = 0;
        while (k--) n = n << 8 | *d->p++;
    }
    if (n > (unsigned)(d->fim - d->p)) return false;
    if (tag) *tag = t;
    if (cont) { cont->p = d->p; cont->n = n; }
    d->p += n;
    if (todo) { todo->p = ini; todo->n = (unsigned)(d->p - ini); }
    return true;
}

static bool der_esperar(der_t *d, int tag_esperada, fatia_t *cont, fatia_t *todo)
{
    int t;
    der_t salvo = *d;
    if (!der_ler(d, &t, cont, todo) || t != tag_esperada) { *d = salvo; return false; }
    return true;
}

static der_t der_de(fatia_t f) { der_t d = { f.p, f.p + f.n }; return d; }
static bool der_fim(const der_t *d) { return d->p >= d->fim; }

static bool oid_igual(fatia_t o, const uint8_t *b, unsigned n) { return o.n == n && memcmp(o.p, b, n) == 0; }
#define OID_E(o, ...) oid_igual(o, (const uint8_t[]){ __VA_ARGS__ }, sizeof((const uint8_t[]){ __VA_ARGS__ }))

/* ------------------------------------------------------------ algoritmos */

static int alg_hash_oid(fatia_t o)
{
    if (OID_E(o, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01)) return 256;
    if (OID_E(o, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02)) return 384;
    if (OID_E(o, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03)) return 512;
    return 0;
}

/* AlgorithmIdentifier de assinatura -> ASSIN_* (0 = não suportado) */
static int ler_alg_assinatura(fatia_t seq)
{
    der_t d = der_de(seq);
    fatia_t oid, params;
    if (!der_esperar(&d, T_OID, &oid, NULL)) return 0;
    if (OID_E(oid, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b)) return ASSIN_RSA_SHA256;
    if (OID_E(oid, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0c)) return ASSIN_RSA_SHA384;
    if (OID_E(oid, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0d)) return ASSIN_RSA_SHA512;
    if (OID_E(oid, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05)) return ASSIN_RSA_SHA1;
    if (OID_E(oid, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02)) return ASSIN_ECDSA_SHA256;
    if (OID_E(oid, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x03)) return ASSIN_ECDSA_SHA384;
    if (OID_E(oid, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04)) return ASSIN_ECDSA_SHA512;
    if (OID_E(oid, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0a)) {
        /* RSASSA-PSS-params: [0] hashAlgorithm (o padrão, SHA-1, não é aceito) */
        if (!der_esperar(&d, T_SEQ, &params, NULL)) return 0;
        der_t p = der_de(params);
        fatia_t h0, hseq, hoid;
        if (!der_esperar(&p, 0xa0, &h0, NULL)) return 0;
        der_t q = der_de(h0);
        if (!der_esperar(&q, T_SEQ, &hseq, NULL)) return 0;
        der_t r = der_de(hseq);
        if (!der_esperar(&r, T_OID, &hoid, NULL)) return 0;
        int h = alg_hash_oid(hoid);
        return h == 256 ? ASSIN_PSS_SHA256 : h == 384 ? ASSIN_PSS_SHA384 : h == 512 ? ASSIN_PSS_SHA512 : 0;
    }
    return 0;
}

const char *x509_alg_texto(int alg)
{
    switch (alg) {
    case ASSIN_RSA_SHA256: return "RSA PKCS#1 v1.5 + SHA-256";
    case ASSIN_RSA_SHA384: return "RSA PKCS#1 v1.5 + SHA-384";
    case ASSIN_RSA_SHA512: return "RSA PKCS#1 v1.5 + SHA-512";
    case ASSIN_RSA_SHA1:   return "RSA + SHA-1 (no aceptado)";
    case ASSIN_PSS_SHA256: return "RSA-PSS + SHA-256";
    case ASSIN_PSS_SHA384: return "RSA-PSS + SHA-384";
    case ASSIN_PSS_SHA512: return "RSA-PSS + SHA-512";
    case ASSIN_ECDSA_SHA256: return "ECDSA + SHA-256";
    case ASSIN_ECDSA_SHA384: return "ECDSA + SHA-384";
    case ASSIN_ECDSA_SHA512: return "ECDSA + SHA-512";
    default: return "desconocido";
    }
}

/* ------------------------------------------------------------ datas */

static bool dig2(const uint8_t *p, unsigned *v)
{
    if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9') return false;
    *v = (unsigned)(p[0] - '0') * 10 + (unsigned)(p[1] - '0');
    return true;
}

/* UTCTime AAMMDDhhmmssZ (AA >= 50 é 19AA) ou GeneralizedTime AAAAMMDDhhmmssZ */
static bool ler_tempo(der_t *d, uint64_t *saida)
{
    int t;
    fatia_t c;
    if (!der_ler(d, &t, &c, NULL)) return false;
    unsigned ano, mes, dia, h, m, s, a1, a2;
    const uint8_t *p = c.p;
    if (t == T_UTC && c.n == 13) {
        if (!dig2(p, &a1)) return false;
        ano = a1 >= 50 ? 1900 + a1 : 2000 + a1;
        p += 2;
    } else if (t == T_GEN && c.n == 15) {
        if (!dig2(p, &a1) || !dig2(p + 2, &a2)) return false;
        ano = a1 * 100 + a2;
        p += 4;
    } else return false;
    if (!dig2(p, &mes) || !dig2(p + 2, &dia) || !dig2(p + 4, &h) || !dig2(p + 6, &m) || !dig2(p + 8, &s) || p[10] != 'Z')
        return false;
    *saida = (((((uint64_t)ano * 100 + mes) * 100 + dia) * 100 + h) * 100 + m) * 100 + s;
    return true;
}

/* A validade dos certificados está em UTC (o "Z" do UTCTime/GeneralizedTime).
 * Desde D080 o kernel guarda UTC e SIS_DATA dá a hora LOCAL: a comparação
 * usa o UTC de SIS_HORA, certo mesmo com o RTC em hora local (Windows). */
uint64_t data_agora(void)
{
    uint32_t t = hora_utc(NULL);
    if (!t) return 0;
    fuso_data_t d;
    fuso_decompor(t, &d);
    return (((((uint64_t)d.ano * 100 + d.mes) * 100 + d.dia) * 100 + d.hora) * 100 + d.minuto) * 100 + d.segundo;
}

/* d /= 100 e devolve o resto, em pedaços de 16 bits: sem a divisão de 64
 * bits da libgcc (__udivdi3), que os programas não ligam. */
static unsigned div100(uint64_t *d)
{
    uint64_t q = 0;
    unsigned r = 0;
    for (int i = 3; i >= 0; i--) {
        unsigned cur = r << 16 | (unsigned)((*d >> (16 * i)) & 0xFFFF);
        q |= (uint64_t)(cur / 100) << (16 * i);
        r = cur % 100;
    }
    *d = q;
    return r;
}

void x509_data_texto(uint64_t d, char *s, unsigned max)
{
    div100(&d);   /* segundos */
    unsigned min = div100(&d), h = div100(&d), dia = div100(&d), mes = div100(&d);
    formatar_texto(s, max, "%04u-%02u-%02u %02u:%02u", (unsigned)d, mes, dia, h, min);
}

/* ------------------------------------------------------------ certificado */

static bool ler_extensoes(cert_t *c, fatia_t exts, bool *critica_desconhecida)
{
    der_t d = der_de(exts);
    fatia_t lista;
    if (!der_esperar(&d, T_SEQ, &lista, NULL)) return false;
    der_t l = der_de(lista);
    while (!der_fim(&l)) {
        fatia_t ext, oid, valor;
        if (!der_esperar(&l, T_SEQ, &ext, NULL)) return false;
        der_t e = der_de(ext);
        if (!der_esperar(&e, T_OID, &oid, NULL)) return false;
        bool critica = false;
        fatia_t b;
        if (der_esperar(&e, T_BOOL, &b, NULL)) critica = b.n == 1 && b.p[0] != 0;
        if (!der_esperar(&e, T_OCTETOS, &valor, NULL)) return false;
        der_t v = der_de(valor);
        if (OID_E(oid, 0x55, 0x1d, 0x13)) {              /* basicConstraints */
            fatia_t seq;
            if (!der_esperar(&v, T_SEQ, &seq, NULL)) return false;
            der_t s = der_de(seq);
            c->tem_basico = true;
            if (der_esperar(&s, T_BOOL, &b, NULL)) c->eh_ca = b.n == 1 && b.p[0] != 0;
            fatia_t i;
            if (der_esperar(&s, T_INTEIRO, &i, NULL) && i.n >= 1 && i.n <= 2 && !(i.p[0] & 0x80))
                c->caminho_max = i.n == 1 ? i.p[0] : i.p[0] << 8 | i.p[1];
        } else if (OID_E(oid, 0x55, 0x1d, 0x0f)) {       /* keyUsage */
            fatia_t bits;
            if (!der_esperar(&v, T_BITS, &bits, NULL) || bits.n < 1) return false;
            c->tem_uso = true;
            c->uso = bits.n >= 2 ? bits.p[1] : 0;
        } else if (OID_E(oid, 0x55, 0x1d, 0x11)) {       /* subjectAltName */
            if (!der_esperar(&v, T_SEQ, &c->san, NULL)) return false;
        } else if (OID_E(oid, 0x55, 0x1d, 0x25)) {       /* extKeyUsage */
            fatia_t seq, o;
            if (!der_esperar(&v, T_SEQ, &seq, NULL)) return false;
            der_t s = der_de(seq);
            c->tem_eku = true;
            while (der_esperar(&s, T_OID, &o, NULL))
                if (OID_E(o, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01) || OID_E(o, 0x55, 0x1d, 0x25, 0x00))
                    c->eku_servidor = true;
        } else if (OID_E(oid, 0x55, 0x1d, 0x20) || OID_E(oid, 0x55, 0x1d, 0x0e) || OID_E(oid, 0x55, 0x1d, 0x23)) {
            /* políticas, identificadores de chave: sem efeito na validação */
        } else if (critica) {
            *critica_desconhecida = true;   /* nameConstraints, policyConstraints...: não sabemos aplicar */
        }
    }
    return true;
}

static bool ler_chave(cert_t *c, fatia_t spki)
{
    der_t d = der_de(spki);
    fatia_t alg, bits, oid, param;
    if (!der_esperar(&d, T_SEQ, &alg, NULL) || !der_esperar(&d, T_BITS, &bits, NULL) || bits.n < 2 || bits.p[0] != 0)
        return false;
    der_t a = der_de(alg);
    if (!der_esperar(&a, T_OID, &oid, NULL)) return false;
    fatia_t chave = { bits.p + 1, bits.n - 1 };
    if (OID_E(oid, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01)) {   /* rsaEncryption */
        der_t k = der_de(chave);
        fatia_t seq;
        if (!der_esperar(&k, T_SEQ, &seq, NULL)) return false;
        der_t s = der_de(seq);
        if (!der_esperar(&s, T_INTEIRO, &c->rsa_n, NULL) || !der_esperar(&s, T_INTEIRO, &c->rsa_e, NULL)) return false;
        while (c->rsa_n.n && !c->rsa_n.p[0]) { c->rsa_n.p++; c->rsa_n.n--; }
        while (c->rsa_e.n && !c->rsa_e.p[0]) { c->rsa_e.p++; c->rsa_e.n--; }
        if (!c->rsa_n.n || !c->rsa_e.n) return false;
        c->tipo_chave = CHAVE_RSA;
        c->bits = c->rsa_n.n * 8;
        for (uint8_t b = c->rsa_n.p[0]; !(b & 0x80); b <<= 1) c->bits--;
        return true;
    }
    if (OID_E(oid, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01)) {               /* id-ecPublicKey */
        if (!der_esperar(&a, T_OID, &param, NULL)) return false;
        if (OID_E(param, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07)) c->curva = CURVA_P256;
        else if (OID_E(param, 0x2b, 0x81, 0x04, 0x00, 0x22)) c->curva = CURVA_P384;
        else c->curva = 0;   /* P-521 e outras: o certificado lê, mas não verifica */
        c->tipo_chave = CHAVE_EC;
        c->ec_ponto = chave;
        c->bits = c->curva == CURVA_P256 ? 256 : c->curva == CURVA_P384 ? 384 : 0;
        return true;
    }
    return true;   /* outro tipo de chave: tipo_chave = 0, não serve para verificar */
}

int x509_ler(cert_t *c, const uint8_t *der, unsigned n)
{
    memset(c, 0, sizeof *c);
    c->der = der;
    c->caminho_max = -1;
    der_t d = { der, der + n };
    fatia_t cert, alg, assin, todo;
    if (!der_esperar(&d, T_SEQ, &cert, &todo)) return X509_FORMATO;
    c->nder = todo.n;
    der_t x = der_de(cert);
    if (!der_esperar(&x, T_SEQ, NULL, &c->tbs) || !der_esperar(&x, T_SEQ, &alg, NULL) ||
        !der_esperar(&x, T_BITS, &assin, NULL) || assin.n < 1 || assin.p[0] != 0)
        return X509_FORMATO;
    c->alg = ler_alg_assinatura(alg);
    c->assinatura.p = assin.p + 1;
    c->assinatura.n = assin.n - 1;
    /* TBSCertificate */
    der_t t = { c->tbs.p, c->tbs.p + c->tbs.n };
    fatia_t tbs;
    if (!der_esperar(&t, T_SEQ, &tbs, NULL)) return X509_FORMATO;
    t = der_de(tbs);
    fatia_t v, alg2, validade, spki;
    c->versao = 1;
    if (der_esperar(&t, 0xa0, &v, NULL)) {
        der_t vv = der_de(v);
        fatia_t i;
        if (!der_esperar(&vv, T_INTEIRO, &i, NULL) || i.n != 1) return X509_FORMATO;
        c->versao = i.p[0] + 1;
    }
    if (!der_esperar(&t, T_INTEIRO, &c->serial, NULL) || !der_esperar(&t, T_SEQ, &alg2, NULL) ||
        !der_esperar(&t, T_SEQ, NULL, &c->emissor) || !der_esperar(&t, T_SEQ, &validade, NULL) ||
        !der_esperar(&t, T_SEQ, NULL, &c->sujeito) || !der_esperar(&t, T_SEQ, NULL, &spki))
        return X509_FORMATO;
    /* o algoritmo de dentro tem de ser igual ao de fora (4.1.1.2) */
    if (alg2.n != alg.n || memcmp(alg2.p, alg.p, alg.n) != 0) return X509_FORMATO;
    der_t val = der_de(validade);
    if (!ler_tempo(&val, &c->antes) || !ler_tempo(&val, &c->depois)) return X509_FORMATO;
    c->spki = spki;
    fatia_t spki_cont = { spki.p, spki.n };
    {
        der_t s = der_de(spki_cont);
        fatia_t dentro;
        if (!der_esperar(&s, T_SEQ, &dentro, NULL) || !ler_chave(c, dentro)) return X509_FORMATO;
    }
    bool critica = false;
    while (!der_fim(&t)) {
        int tag;
        fatia_t cont;
        if (!der_ler(&t, &tag, &cont, NULL)) return X509_FORMATO;
        if (tag == 0xa3 && !ler_extensoes(c, cont, &critica)) return X509_FORMATO;
    }
    if (critica) return X509_CRITICA;
    return X509_OK;
}

const char *x509_erro_texto(int e)
{
    switch (e) {
    case X509_OK:           return "certificado válido";
    case X509_FORMATO:      return "certificado mal formado";
    case X509_CADUCADO:     return "certificado caducado";
    case X509_AINDA_NAO:    return "certificado aún no válido (¿fecha del sistema?)";
    case X509_NOME:         return "nombre no coincide";
    case X509_EMISSOR:      return "emisor desconocido";
    case X509_ASSINATURA:   return "firma del certificado no válida";
    case X509_NAO_CA:       return "el emisor no es una autoridad de certificación";
    case X509_USO:          return "uso de clave no permitido";
    case X509_ALGORITMO:    return "algoritmo de firma no soportado";
    case X509_CAMINHO:      return "cadena de certificados demasiado larga";
    case X509_CRITICA:      return "extensión crítica desconocida";
    case X509_DATA_SISTEMA: return "la fecha del sistema no es correcta (mira el comando data)";
    default:                return "error de certificado";
    }
}

/* ------------------------------------------------------------ nomes */

/* Copia um valor de texto do DER para Latin-1 (UTF-8 e BMP convertidos). */
static void texto_attr(int tag, fatia_t v, char *s, unsigned max)
{
    unsigned k = 0;
    if (!max) return;
    if (tag == T_BMP) {   /* UTF-16BE */
        for (unsigned i = 0; i + 1 < v.n && k + 1 < max; i += 2)
            s[k++] = v.p[i] ? '?' : (char)v.p[i + 1];
        s[k] = 0;
        return;
    }
    for (unsigned i = 0; i < v.n && k + 1 < max; i++) s[k++] = v.p[i] >= 32 ? (char)v.p[i] : '?';
    s[k] = 0;
    if (tag == T_UTF8) {
        size_t sobra;
        size_t m = utf8_para_latin1(s, k, &sobra);
        s[m] = 0;
    }
}

typedef void (*attr_f)(void *ctx, const char *rotulo, int tag, fatia_t valor);

static void percorrer_nome(fatia_t nome, attr_f f, void *ctx)
{
    der_t d = der_de(nome);
    fatia_t seq;
    if (!der_esperar(&d, T_SEQ, &seq, NULL)) return;
    der_t r = der_de(seq);
    fatia_t rdn;
    while (der_esperar(&r, T_SET, &rdn, NULL)) {
        der_t a = der_de(rdn);
        fatia_t atv;
        while (der_esperar(&a, T_SEQ, &atv, NULL)) {
            der_t x = der_de(atv);
            fatia_t oid, val;
            int tag;
            if (!der_esperar(&x, T_OID, &oid, NULL) || !der_ler(&x, &tag, &val, NULL)) return;
            const char *rot = NULL;
            if (oid.n == 3 && oid.p[0] == 0x55 && oid.p[1] == 0x04) {
                switch (oid.p[2]) {
                case 3: rot = "CN"; break;
                case 6: rot = "C"; break;
                case 7: rot = "L"; break;
                case 8: rot = "ST"; break;
                case 10: rot = "O"; break;
                case 11: rot = "OU"; break;
                }
            }
            if (rot) f(ctx, rot, tag, val);
        }
    }
}

typedef struct { char *s; unsigned max; } junta_t;

static void attr_juntar(void *ctx, const char *rot, int tag, fatia_t v)
{
    junta_t *j = ctx;
    char val[128], peca[160];
    texto_attr(tag, v, val, sizeof val);
    formatar_texto(peca, sizeof peca, "%s%s=%s", j->s[0] ? ", " : "", rot, val);
    junta_texto(j->s, peca, j->max);
}

void x509_nome_texto(fatia_t nome, char *s, unsigned max)
{
    junta_t j = { s, max };
    s[0] = 0;
    percorrer_nome(nome, attr_juntar, &j);
}

typedef struct { char cn[128], o[128]; } curto_t;

static void attr_curto(void *ctx, const char *rot, int tag, fatia_t v)
{
    curto_t *c = ctx;
    if (compara_texto(rot, "CN") == 0) texto_attr(tag, v, c->cn, sizeof c->cn);
    else if (compara_texto(rot, "O") == 0) texto_attr(tag, v, c->o, sizeof c->o);
}

void x509_nome_curto(fatia_t nome, char *s, unsigned max)
{
    curto_t c;
    c.cn[0] = c.o[0] = 0;
    percorrer_nome(nome, attr_curto, &c);
    copia_texto(s, c.cn[0] ? c.cn : c.o[0] ? c.o : "?", max);
}

void x509_huella(const cert_t *c, char *s, unsigned max)
{
    uint8_t h[32];
    sha256(c->der, c->nder, h);
    unsigned k = 0;
    for (int i = 0; i < 32 && k + 4 <= max; i++) {
        static const char hx[] = "0123456789ABCDEF";
        if (i) s[k++] = ':';
        s[k++] = hx[h[i] >> 4];
        s[k++] = hx[h[i] & 15];
    }
    s[k < max ? k : max - 1] = 0;
}

/* ------------------------------------------------------------ nome do servidor */

static char minusc(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

static bool igual_sem_caixa(const char *a, unsigned na, const char *b, unsigned nb)
{
    if (na != nb) return false;
    for (unsigned i = 0; i < na; i++)
        if (minusc(a[i]) != minusc(b[i])) return false;
    return true;
}

/* "*.exemplo.com" vale para "a.exemplo.com", não para "exemplo.com" nem
 * "a.b.exemplo.com"; o "*" só pode ser o rótulo inteiro mais à esquerda e
 * precisa de pelo menos dois rótulos depois (nada de "*.com"). */
static bool dns_confere(const char *padrao, unsigned np, const char *host)
{
    unsigned nh = (unsigned)tam_texto(host);
    if (nh && host[nh - 1] == '.') nh--;
    if (np && padrao[np - 1] == '.') np--;
    if (np >= 2 && padrao[0] == '*' && padrao[1] == '.') {
        const char *resto = padrao + 1;   /* ".exemplo.com" */
        unsigned nr = np - 1, pontos = 0;
        for (unsigned i = 1; i < nr; i++) if (resto[i] == '.') pontos++;
        if (pontos < 1) return false;
        const char *p = host;
        while (p < host + nh && *p != '.') p++;
        if (p == host || p >= host + nh) return false;   /* rótulo vazio ou sem ponto */
        return igual_sem_caixa(p, (unsigned)(host + nh - p), resto, nr);
    }
    for (unsigned i = 0; i < np; i++) if (padrao[i] == '*') return false;
    return igual_sem_caixa(padrao, np, host, nh);
}

static bool ip_de_texto(const char *s, uint8_t ip[4])
{
    for (int i = 0; i < 4; i++) {
        unsigned v = 0, dig = 0;
        while (*s >= '0' && *s <= '9') { v = v * 10 + (unsigned)(*s++ - '0'); if (++dig > 3) return false; }
        if (!dig || v > 255) return false;
        ip[i] = (uint8_t)v;
        if (i < 3 && *s++ != '.') return false;
    }
    return *s == 0;
}

void x509_san_listar(const cert_t *c, void (*f)(void *ctx, const char *nome), void *ctx)
{
    der_t d = der_de(c->san);
    int tag;
    fatia_t v;
    while (der_ler(&d, &tag, &v, NULL)) {
        char s[128];
        if (tag == 0x82) {   /* dNSName */
            unsigned k = v.n < sizeof s - 1 ? v.n : sizeof s - 1;
            memcpy(s, v.p, k);
            s[k] = 0;
            f(ctx, s);
        } else if (tag == 0x87 && v.n == 4) {   /* iPAddress */
            formatar_texto(s, sizeof s, "%u.%u.%u.%u", v.p[0], v.p[1], v.p[2], v.p[3]);
            f(ctx, s);
        }
    }
}

bool x509_nome_confere(const cert_t *c, const char *host)
{
    uint8_t ip[4];
    bool eh_ip = ip_de_texto(host, ip), tem_dns = false;
    der_t d = der_de(c->san);
    int tag;
    fatia_t v;
    while (der_ler(&d, &tag, &v, NULL)) {
        if (tag == 0x82) {
            tem_dns = true;
            if (!eh_ip && dns_confere((const char *)v.p, v.n, host)) return true;
        } else if (tag == 0x87) {
            if (eh_ip && v.n == 4 && memcmp(v.p, ip, 4) == 0) return true;
        }
    }
    if (tem_dns || c->san.n) return false;
    /* Sem SAN: o CN como último recurso (certificados antigos, RFC 6125 6.4.4) */
    char cn[128];
    curto_t cc;
    cc.cn[0] = cc.o[0] = 0;
    percorrer_nome(c->sujeito, attr_curto, &cc);
    copia_texto(cn, cc.cn, sizeof cn);
    if (!cn[0]) return false;
    if (eh_ip) return compara_texto(cn, host) == 0;
    return dns_confere(cn, (unsigned)tam_texto(cn), host);
}

/* ------------------------------------------------------------ assinaturas */

static const hash_alg_t *hash_do_esquema(int e)
{
    switch (e) {
    case ASSIN_RSA_SHA256: case ASSIN_PSS_SHA256: case ASSIN_ECDSA_SHA256: return &HASH_SHA256;
    case ASSIN_RSA_SHA384: case ASSIN_PSS_SHA384: case ASSIN_ECDSA_SHA384: return &HASH_SHA384;
    case ASSIN_RSA_SHA512: case ASSIN_PSS_SHA512: case ASSIN_ECDSA_SHA512: return &HASH_SHA512;
    default: return NULL;
    }
}

bool x509_verificar(const cert_t *c, int esquema, const uint8_t *dados, unsigned n, const uint8_t *assin, unsigned na)
{
    const hash_alg_t *h = hash_do_esquema(esquema);
    if (!h) return false;
    uint8_t dig[HASH_MAX];
    hash_calcular(h, dados, n, dig);
    if (esquema >= ASSIN_ECDSA_SHA256 && esquema <= ASSIN_ECDSA_SHA512) {
        if (c->tipo_chave != CHAVE_EC || !c->curva) return false;
        /* Ecdsa-Sig-Value ::= SEQUENCE { r INTEGER, s INTEGER } */
        der_t d = { assin, assin + na };
        fatia_t seq, r, s;
        if (!der_esperar(&d, T_SEQ, &seq, NULL)) return false;
        der_t q = der_de(seq);
        if (!der_esperar(&q, T_INTEIRO, &r, NULL) || !der_esperar(&q, T_INTEIRO, &s, NULL)) return false;
        return ecdsa_verificar(c->curva, c->ec_ponto.p, c->ec_ponto.n, dig, h->tam, r.p, r.n, s.p, s.n);
    }
    if (c->tipo_chave != CHAVE_RSA || c->bits < 2048) return false;   /* RSA < 2048 bits: fraco demais */
    if (esquema >= ASSIN_PSS_SHA256 && esquema <= ASSIN_PSS_SHA512)
        return rsa_verificar_pss(c->rsa_n.p, c->rsa_n.n, c->rsa_e.p, c->rsa_e.n, h, dig, assin, na, -1);
    return rsa_verificar_pkcs1(c->rsa_n.p, c->rsa_n.n, c->rsa_e.p, c->rsa_e.n, h, dig, assin, na);
}

bool x509_assinado_por(const cert_t *c, const cert_t *emissor)
{
    if (!c->alg || c->alg == ASSIN_RSA_SHA1) return false;
    return x509_verificar(emissor, c->alg, c->tbs.p, c->tbs.n, c->assinatura.p, c->assinatura.n);
}

/* ------------------------------------------------------------ cadeia */

static bool mesmo_nome(fatia_t a, fatia_t b) { return a.n == b.n && memcmp(a.p, b.p, a.n) == 0; }

static bool eh_raiz(const cert_t *c, const almacen_t *r, int *qual)
{
    for (int i = 0; r && i < r->n; i++)
        if (mesmo_nome(c->sujeito, r->c[i].sujeito) && c->spki.n == r->c[i].spki.n &&
            memcmp(c->spki.p, r->c[i].spki.p, c->spki.n) == 0) {
            *qual = i;
            return true;
        }
    return false;
}

int x509_validar(const cert_t *cadeia, int n, const almacen_t *raizes, const char *host, uint64_t agora, int *raiz_usada)
{
    if (raiz_usada) *raiz_usada = -1;
    if (n < 1) return X509_FORMATO;
    if (agora < 20250101000000ull) return X509_DATA_SISTEMA;
    const cert_t *folha = &cadeia[0];
    /* O certificado do servidor: datas, nome e usos */
    if (agora > folha->depois) return X509_CADUCADO;
    if (agora < folha->antes) return X509_AINDA_NAO;
    if (host && host[0] && !x509_nome_confere(folha, host)) return X509_NOME;
    if (folha->tem_eku && !folha->eku_servidor) return X509_USO;
    if (folha->tem_uso && !(folha->uso & USO_ASSINATURA)) return X509_USO;
    bool usado[16] = { false };
    const cert_t *atual = folha;
    int erro_achado = X509_EMISSOR;
    for (int prof = 0; prof < 10; prof++) {
        int r;
        /* o próprio certificado está no almacén (raiz, ou um certificado
         * autoassinado que o usuário adicionou): confiável */
        if (eh_raiz(atual, raizes, &r)) {
            if (raiz_usada) *raiz_usada = r;
            return X509_OK;
        }
        /* emitido por uma raiz do almacén? */
        for (int i = 0; raizes && i < raizes->n; i++) {
            const cert_t *a = &raizes->c[i];
            if (!mesmo_nome(atual->emissor, a->sujeito)) continue;
            if (!atual->alg || atual->alg == ASSIN_RSA_SHA1) return X509_ALGORITMO;
            if (x509_assinado_por(atual, a)) {
                if (raiz_usada) *raiz_usada = i;
                return agora > a->depois ? X509_CADUCADO : X509_OK;   /* raiz vencida: como o OpenSSL */
            }
            erro_achado = X509_ASSINATURA;
        }
        /* senão, um intermediário mandado pelo servidor */
        const cert_t *prox = NULL;
        for (int i = 1; i < n && i < 16 && !prox; i++) {
            const cert_t *c = &cadeia[i];
            if (usado[i] || !mesmo_nome(atual->emissor, c->sujeito)) continue;
            if (!atual->alg || atual->alg == ASSIN_RSA_SHA1) return X509_ALGORITMO;
            if (!x509_assinado_por(atual, c)) { erro_achado = X509_ASSINATURA; continue; }
            if (!c->tem_basico || !c->eh_ca) return X509_NAO_CA;
            if (c->tem_uso && !(c->uso & USO_CERT)) return X509_USO;
            if (c->caminho_max >= 0 && prof > c->caminho_max) return X509_CAMINHO;
            if (agora > c->depois) return X509_CADUCADO;
            if (agora < c->antes) return X509_AINDA_NAO;
            usado[i] = true;
            prox = c;
        }
        if (!prox) return erro_achado;
        atual = prox;
    }
    return X509_CAMINHO;
}

/* ------------------------------------------------------------ PEM */

static int b64(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

int pem_proximo(const char *t, unsigned n, unsigned *pos, uint8_t *der, unsigned max)
{
    static const char ini[] = "-----BEGIN CERTIFICATE-----", fim[] = "-----END CERTIFICATE-----";
    unsigned i = *pos, ni = sizeof ini - 1, nf = sizeof fim - 1;
    while (i + ni <= n && memcmp(t + i, ini, ni) != 0) i++;
    if (i + ni > n) { *pos = n; return 0; }
    i += ni;
    uint32_t acc = 0;
    int bits = 0;
    unsigned k = 0;
    while (i < n) {
        if (t[i] == '-') {
            if (i + nf > n || memcmp(t + i, fim, nf) != 0) return -1;
            *pos = i + nf;
            return (int)k;
        }
        int v = b64(t[i++]);
        if (v < 0) continue;   /* quebras de linha, espaços e '=' */
        acc = acc << 6 | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (k >= max) return -1;
            der[k++] = (uint8_t)(acc >> bits);
        }
    }
    return -1;   /* sem o END */
}

/* ------------------------------------------------------------ mostrar */

typedef struct { char s[600]; } lista_t;

static void san_juntar(void *ctx, const char *nome)
{
    lista_t *l = ctx;
    if (l->s[0]) junta_texto(l->s, ", ", sizeof l->s);
    junta_texto(l->s, nome, sizeof l->s);
}

void x509_mostrar(int fd, const cert_t *c, const char *p)
{
    char s[600], a[24], b[24];
    x509_nome_texto(c->sujeito, s, sizeof s);
    imprimir_em(fd, "%ssujeto:  %s\n", p, s);
    x509_nome_texto(c->emissor, s, sizeof s);
    imprimir_em(fd, "%semisor:  %s\n", p, s);
    x509_data_texto(c->antes, a, sizeof a);
    x509_data_texto(c->depois, b, sizeof b);
    uint64_t agora = data_agora();
    imprimir_em(fd, "%sválido:  del %s al %s UTC%s\n", p, a, b,
                agora > c->depois ? "  (CADUCADO)" : agora < c->antes ? "  (aún no válido)" : "");
    if (c->tipo_chave == CHAVE_RSA) imprimir_em(fd, "%sclave:   RSA de %u bits\n", p, c->bits);
    else if (c->tipo_chave == CHAVE_EC) imprimir_em(fd, "%sclave:   ECDSA %s\n", p, c->curva == CURVA_P256 ? "P-256" : c->curva == CURVA_P384 ? "P-384" : "(curva no soportada)");
    else imprimir_em(fd, "%sclave:   tipo no soportado\n", p);
    imprimir_em(fd, "%sfirma:   %s\n", p, x509_alg_texto(c->alg));
    if (c->tem_basico && c->eh_ca) {
        if (c->caminho_max >= 0) imprimir_em(fd, "%sCA:      sí (hasta %d intermedios)\n", p, c->caminho_max);
        else imprimir_em(fd, "%sCA:      sí\n", p);
    } else imprimir_em(fd, "%sCA:      no\n", p);
    lista_t l;
    l.s[0] = 0;
    x509_san_listar(c, san_juntar, &l);
    if (l.s[0]) imprimir_em(fd, "%snombres: %s\n", p, l.s);
    x509_huella(c, s, sizeof s);
    imprimir_em(fd, "%sSHA-256: %s\n", p, s);
}
