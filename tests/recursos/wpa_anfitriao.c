// SPDX-License-Identifier: GPL-2.0-or-later
/* wpa_anfitriao.c - envoltorio de kernel/lib/wpa.c para las pruebas del
 * anfitrión (tests/h215_wifi.py, D215). Lee órdenes de la entrada estándar,
 * una por línea, y escribe los resultados en hexadecimal:
 *   sha1 <hex>                         -> hash
 *   hmac <clave> <datos>               -> HMAC-SHA1
 *   pmk <contraseña> <ssid>            -> PMK (texto tal cual, sin espacios)
 *   prf <clave> <rótulo> <datos> <n>   -> PRF-n bytes (el rótulo sin espacios: '_' = ' ')
 *   aes <clave> <bloque>               -> cifrado y descifrado
 *   envolver <kek> <datos> / desenvolver <kek> <datos>
 *   ies <privacidad 0/1> <hex>         -> campos de wpa_bss_t
 *   sup <pmk> <aa> <spa> <rsn_ap o -> <snonce>
 *   eapol <hex>                        -> código, respuesta y claves
 *
 * D220 (v7.3.2), kernel/lib/wpa_cifra.c, p256.c y sae.c:
 *   md5 <hex> / hmacmd5 <clave> <datos> / crc32 <hex>
 *   rc4 <clave> <desde> <n>            -> n bytes del flujo desde «desde»
 *   michael <clave> <msg>
 *   tkipmix <tk> <ta> <iv32> <iv16>    -> TTAK (fase 1) y la clave RC4 (fase 2)
 *   tkipcif <tk> <mic> <tsc> <cab> <trama> / tkipdec <tk> <mic> <cab> <trama>
 *   wepcif <clave> <iv> <keyid> <cab> <trama> / wepdec <clave> <cab> <trama>
 *   cmac <clave> <msg>
 *   ccmpcif <tk> <pn> <keyid> <cab> <trama> / ccmpdec <tk> <cab> <trama>
 *   bipass <igtk> <keyid> <ipn> <trama> / bipver <igtk> <keyid> <trama>
 *   hmac256 <clave> <datos> / kdf256 <clave> <rótulo> <contexto> <n> / hkdf <sal> <ikm> <info> <n>
 *   p256mul <k> <punto o -> / p256add <p> <q> / p256ok <p>
 *   saehp <contraseña> <mac_a> <mac_b> / saept <ssid> <contraseña> <id o -> / saepwept <pt> <mac_a> <mac_b>
 *   saecommit <pwe> <rand> <mask> / saepeer <escalar> <elemento> / saeconf <sc> / saever <sc> <confirm>
 *   ies2 <privacidad> <hex>            -> los campos nuevos de wpa_bss_t y lo que se elige (sae y psk)
 *   supx <wpa1,akm,par,grupo,pmf,h2e> <pmk> <aa> <spa> <ie_ap> <rsnxe o -> <pmkid o -> <snonce>
 *   chaves                             -> PTK entera, GTK, IGTK y el PMKID del mensaje 1
 *   falhamic <par 0/1>                 -> el aviso de un fallo de Michael
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wpa.h"
#include "wpa_cifra.h"
#include "p256.h"
#include "sae.h"

static wpa_sup_t sup;
static sae_t sae;

static int hex(const char *s, uint8_t *o, int max)
{
    int n = 0;
    if (!strcmp(s, "-")) return 0;
    while (s[0] && s[1] && n < max) {
        unsigned v;
        if (sscanf(s, "%2x", &v) != 1) break;
        o[n++] = (uint8_t)v;
        s += 2;
    }
    return n;
}

static void imprimir(const char *rot, const uint8_t *p, int n)
{
    printf("%s ", rot);
    for (int i = 0; i < n; i++) printf("%02x", p[i]);
    if (!n) printf("-");
    printf("\n");
}

int main(void)
{
    static char linha[8192];
    static uint8_t a[2048], b[2048], c[2048], d[2048];
    while (fgets(linha, sizeof linha, stdin)) {
        static char p1[4096], p2[4096], p3[4096], p4[4096], p5[4096], p6[512], p7[512], p8[512];
        char cmd[32];
        p1[0] = p2[0] = p3[0] = p4[0] = p5[0] = p6[0] = p7[0] = p8[0] = 0;
        int k = sscanf(linha, "%31s %4095s %4095s %4095s %4095s %4095s %511s %511s %511s", cmd, p1, p2, p3, p4, p5, p6,
                       p7, p8);
        if (k < 1) continue;
        if (!strcmp(cmd, "sha1")) {
            int n = hex(p1, a, sizeof a);
            uint8_t h[20];
            sha1(a, (uint32_t)n, h);
            imprimir("sha1", h, 20);
        } else if (!strcmp(cmd, "hmac")) {
            int nk = hex(p1, a, sizeof a), n = hex(p2, b, sizeof b);
            uint8_t h[20];
            hmac_sha1(a, (uint32_t)nk, b, (uint32_t)n, h);
            imprimir("hmac", h, 20);
        } else if (!strcmp(cmd, "pmk")) {
            uint8_t pmk[32];
            bool ok = wpa_pmk(p1, (const uint8_t *)p2, (uint32_t)strlen(p2), pmk);
            if (ok) imprimir("pmk", pmk, 32);
            else printf("pmk recusada\n");
        } else if (!strcmp(cmd, "prf")) {
            int nk = hex(p1, a, sizeof a), n = hex(p3, b, sizeof b), tam = atoi(p4);
            for (char *x = p2; *x; x++) if (*x == '_') *x = ' ';
            wpa_prf(a, (uint32_t)nk, p2, b, (uint32_t)n, c, (uint32_t)tam);
            imprimir("prf", c, tam);
        } else if (!strcmp(cmd, "aes")) {
            hex(p1, a, 16);
            hex(p2, b, 16);
            aes128_t x;
            aes128_chave(&x, a);
            aes128_cifrar(&x, b, c);
            aes128_decifrar(&x, c, d);
            imprimir("aes", c, 16);
            imprimir("inv", d, 16);
        } else if (!strcmp(cmd, "envolver")) {
            hex(p1, a, 16);
            int n = hex(p2, b, sizeof b);
            aes_envolver(a, b, (uint32_t)n, c);
            imprimir("envolto", c, n + 8);
        } else if (!strcmp(cmd, "desenvolver")) {
            hex(p1, a, 16);
            int n = hex(p2, b, sizeof b);
            if (aes_desenvolver(a, b, (uint32_t)n, c)) imprimir("claro", c, n - 8);
            else printf("claro falhou\n");
        } else if (!strcmp(cmd, "ies")) {
            int n = hex(p2, a, sizeof a);
            wpa_bss_t o;
            wpa_analisar_ies(a, (uint32_t)n, atoi(p1) != 0, &o);
            printf("ies ssid=%.*s canal=%u seg=%u grupo=%u par=%u psk=%u ht=%u ntasas=%u nrsn=%u\n", o.nssid,
                   (const char *)o.ssid, o.canal, o.seguranca, o.cifra_grupo, o.cifras_par, o.psk, o.ht, o.ntasas, o.nrsn);
        } else if (!strcmp(cmd, "sup")) {
            hex(p1, a, 32);
            uint8_t aa[6], spa[6];
            hex(p2, aa, 6);
            hex(p3, spa, 6);
            int nr = hex(p4, b, sizeof b);
            hex(p5, c, 32);
            wpa_sup_iniciar(&sup, a, aa, spa, b, (uint32_t)nr, c);
            imprimir("ie", sup.ie, WPA_RSN_IE_TAM);
        } else if (!strcmp(cmd, "eapol")) {
            int n = hex(p1, a, sizeof a);
            uint32_t nr = 0;
            int r = wpa_processar(&sup, a, (uint32_t)n, b, &nr);
            printf("codigo %d\n", r);
            imprimir("resp", b, (int)nr);
            imprimir("ptk", sup.ptk, sup.ptk_pronta ? WPA_PTK_TAM : 0);
            imprimir("gtk", sup.gtk, sup.ngtk);
            printf("gtkid %u\n", sup.gtk_id);
        } else if (!strcmp(cmd, "md5")) {
            int n = hex(p1, a, sizeof a);
            md5(a, (uint32_t)n, c);
            imprimir("md5", c, 16);
        } else if (!strcmp(cmd, "hmacmd5")) {
            int nk = hex(p1, a, sizeof a), n = hex(p2, b, sizeof b);
            hmac_md5(a, (uint32_t)nk, b, (uint32_t)n, c);
            imprimir("hmacmd5", c, 16);
        } else if (!strcmp(cmd, "crc32")) {
            int n = hex(p1, a, sizeof a);
            printf("crc32 %08x\n", wpa_crc32(a, (uint32_t)n));
        } else if (!strcmp(cmd, "rc4")) {
            int nk = hex(p1, a, sizeof a), desde = atoi(p2), n = atoi(p3);
            rc4_t r;
            rc4_chave(&r, a, (uint32_t)nk);
            rc4_pular(&r, (uint32_t)desde);
            memset(c, 0, (size_t)n);
            rc4_aplicar(&r, c, (uint32_t)n);
            imprimir("rc4", c, n);
        } else if (!strcmp(cmd, "michael")) {
            hex(p1, a, 8);
            int n = hex(p2, b, sizeof b);
            michael(a, b, (uint32_t)n, c);
            imprimir("michael", c, 8);
        } else if (!strcmp(cmd, "tkipmix")) {
            uint8_t ta[6], k[16];
            uint16_t t[5];
            hex(p1, a, 16);
            hex(p2, ta, 6);
            tkip_fase1(t, a, ta, (uint32_t)strtoul(p3, NULL, 16));
            printf("ttak %04x%04x%04x%04x%04x\n", t[0], t[1], t[2], t[3], t[4]);
            tkip_fase2(k, t, a, (uint16_t)strtoul(p4, NULL, 16));
            imprimir("rc4k", k, 16);
        } else if (!strcmp(cmd, "tkipcif")) {
            hex(p1, a, 16);
            hex(p2, b, 8);
            int n = hex(p5, c, (int)sizeof c - 32);
            uint32_t m = tkip_cifrar(c, (uint32_t)atoi(p4), (uint32_t)n, a, b, strtoull(p3, NULL, 16), 0);
            imprimir("quadro", c, (int)m);
        } else if (!strcmp(cmd, "tkipdec")) {
            hex(p1, a, 16);
            hex(p2, b, 8);
            int n = hex(p4, c, sizeof c);
            uint32_t cab = (uint32_t)atoi(p3), nc = 0;
            int r = tkip_decifrar(c, cab, (uint32_t)n, a, b, &nc);
            printf("codigo %d\n", r);
            imprimir("claro", c + cab + 8, (int)nc);
        } else if (!strcmp(cmd, "wepcif")) {
            int nk = hex(p1, a, sizeof a);
            int n = hex(p5, c, (int)sizeof c - 32);
            uint32_t m = wep_cifrar(c, (uint32_t)atoi(p4), (uint32_t)n, a, (uint32_t)nk, (uint32_t)strtoul(p2, NULL, 16),
                                    (uint8_t)atoi(p3));
            imprimir("quadro", c, (int)m);
        } else if (!strcmp(cmd, "wepdec")) {
            int nk = hex(p1, a, sizeof a);
            int n = hex(p3, c, sizeof c);
            uint32_t cab = (uint32_t)atoi(p2), nc = 0;
            int r = wep_decifrar(c, cab, (uint32_t)n, a, (uint32_t)nk, &nc);
            printf("codigo %d\n", r);
            imprimir("claro", c + cab + 4, (int)nc);
        } else if (!strcmp(cmd, "cmac")) {
            hex(p1, a, 16);
            int n = hex(p2, b, sizeof b);
            aes_cmac(a, b, (uint32_t)n, c);
            imprimir("cmac", c, 16);
        } else if (!strcmp(cmd, "ccmpcif")) {
            hex(p1, a, 16);
            int n = hex(p5, c, (int)sizeof c - 32);
            uint32_t m = ccmp_cifrar(c, (uint32_t)atoi(p4), (uint32_t)n, a, strtoull(p2, NULL, 16), (uint8_t)atoi(p3));
            imprimir("quadro", c, (int)m);
        } else if (!strcmp(cmd, "ccmpdec")) {
            hex(p1, a, 16);
            int n = hex(p3, c, sizeof c);
            uint32_t cab = (uint32_t)atoi(p2), nc = 0;
            uint64_t pn = 0;
            int r = ccmp_decifrar(c, cab, (uint32_t)n, a, &pn, &nc);
            printf("codigo %d\n", r);
            printf("pn %012llx\n", (unsigned long long)pn);
            imprimir("claro", c + cab + 8, (int)nc);
        } else if (!strcmp(cmd, "bipass")) {
            hex(p1, a, 16);
            int n = hex(p4, c, (int)sizeof c - 32);
            uint32_t m = bip_assinar(c, (uint32_t)n, a, (uint16_t)atoi(p2), strtoull(p3, NULL, 16));
            imprimir("quadro", c, (int)m);
        } else if (!strcmp(cmd, "bipver")) {
            hex(p1, a, 16);
            int n = hex(p3, c, sizeof c);
            uint64_t ipn = 0;
            bool ok = bip_verificar(c, (uint32_t)n, a, (uint16_t)atoi(p2), &ipn);
            printf("bip %d %012llx\n", ok, (unsigned long long)ipn);
        } else if (!strcmp(cmd, "hmac256")) {
            int nk = hex(p1, a, sizeof a), n = hex(p2, b, sizeof b);
            hmac_sha256(a, (uint32_t)nk, b, (uint32_t)n, c);
            imprimir("hmac256", c, 32);
        } else if (!strcmp(cmd, "kdf256")) {
            int nk = hex(p1, a, sizeof a), n = hex(p3, b, sizeof b), tam = atoi(p4);
            for (char *x = p2; *x; x++) if (*x == '_') *x = ' ';
            kdf_sha256(a, (uint32_t)nk, p2, b, (uint32_t)n, c, (uint32_t)tam);
            imprimir("kdf256", c, tam);
        } else if (!strcmp(cmd, "hkdf")) {
            int ns = hex(p1, a, sizeof a), ni = hex(p2, b, sizeof b), nf = hex(p3, d, sizeof d), tam = atoi(p4);
            uint8_t prk[32];
            hkdf_extrair(a, (uint32_t)ns, b, (uint32_t)ni, prk);
            imprimir("prk", prk, 32);
            hkdf_expandir(prk, d, (uint32_t)nf, c, (uint32_t)tam);
            imprimir("okm", c, tam);
        } else if (!strcmp(cmd, "p256mul")) {
            uint8_t k[32];
            hex(p1, k, 32);
            p256_ponto_t P;
            if (!strcmp(p2, "-")) p256_gerador(&P);
            else { hex(p2, a, 64); if (!p256_ler(&P, a)) { printf("ponto invalido\n"); fflush(stdout); continue; } }
            p256_mult(&P, &P, k);
            if (p256_gravar(&P, c)) imprimir("ponto", c, 64);
            else printf("ponto infinito\n");
        } else if (!strcmp(cmd, "p256add")) {
            p256_ponto_t P, Q;
            hex(p1, a, 64);
            hex(p2, b, 64);
            if (!p256_ler(&P, a) || !p256_ler(&Q, b)) { printf("ponto invalido\n"); fflush(stdout); continue; }
            p256_somar(&P, &P, &Q);
            if (p256_gravar(&P, c)) imprimir("ponto", c, 64);
            else printf("ponto infinito\n");
        } else if (!strcmp(cmd, "p256ok")) {
            p256_ponto_t P;
            hex(p1, a, 64);
            printf("valido %d\n", p256_ler(&P, a));
        } else if (!strcmp(cmd, "saehp")) {
            uint8_t ma[6], mb[6];
            hex(p2, ma, 6);
            hex(p3, mb, 6);
            if (sae_pwe_hp((const uint8_t *)p1, (uint32_t)strlen(p1), ma, mb, c)) imprimir("pwe", c, 64);
            else printf("pwe falhou\n");
        } else if (!strcmp(cmd, "saept")) {
            bool sem_id = !strcmp(p3, "-");
            if (sae_pt((const uint8_t *)p1, (uint32_t)strlen(p1), (const uint8_t *)p2, (uint32_t)strlen(p2),
                       (const uint8_t *)p3, sem_id ? 0 : (uint32_t)strlen(p3), c)) imprimir("pt", c, 64);
            else printf("pt falhou\n");
        } else if (!strcmp(cmd, "saepwept")) {
            uint8_t ma[6], mb[6];
            hex(p1, a, 64);
            hex(p2, ma, 6);
            hex(p3, mb, 6);
            if (sae_pwe_de_pt(a, ma, mb, c)) imprimir("pwe", c, 64);
            else printf("pwe falhou\n");
        } else if (!strcmp(cmd, "saecommit")) {
            hex(p1, a, 64);
            hex(p2, b, 32);
            hex(p3, d, 32);
            memset(&sae, 0, sizeof sae);
            if (sae_commit(&sae, a, b, d)) {
                memcpy(c, sae.escalar, 32);
                memcpy(c + 32, sae.elemento, 64);
                imprimir("commit", c, 96);
            } else printf("commit falhou\n");
        } else if (!strcmp(cmd, "saepeer")) {
            hex(p1, a, 32);
            hex(p2, b, 64);
            printf("codigo %d\n", sae_processar_commit(&sae, a, b));
            imprimir("kck", sae.kck, 32);
            imprimir("pmk", sae.pmk, 32);
            imprimir("pmkid", sae.pmkid, 16);
        } else if (!strcmp(cmd, "saeconf")) {
            sae_confirmar(&sae, (uint16_t)atoi(p1), c);
            imprimir("confirm", c, 32);
        } else if (!strcmp(cmd, "saever")) {
            hex(p2, a, 32);
            printf("verifica %d\n", sae_verificar(&sae, (uint16_t)atoi(p1), a));
        } else if (!strcmp(cmd, "ies2")) {
            int n = hex(p2, a, sizeof a);
            wpa_bss_t o;
            wpa_par_t par;
            wpa_analisar_ies(a, (uint32_t)n, atoi(p1) != 0, &o);
            printf("ies2 seg=%u akms=%u mfpc=%u mfpr=%u gestao=%u h2e=%u soh2e=%u nrsnxe=%u wpa1=%u wgrupo=%u wpares=%u "
                   "wpsk=%u nwpa1=%u\n", o.seguranca, o.akms, o.mfpc, o.mfpr, o.cifra_gestao, o.h2e, o.so_h2e, o.nrsnxe,
                   o.wpa1, o.wpa1_grupo, o.wpa1_pares, o.wpa1_psk, o.nwpa1_ie);
            for (int x = 0; x < 3; x++) {
                bool sae_ = x != 1, psk_ = x != 0;
                if (wpa_escolher(&o, sae_, psk_, &par))
                    printf("escolha%d wpa1=%u akm=%u par=%u grupo=%u pmf=%u\n", x, par.wpa1, par.akm, par.cifra_par,
                           par.cifra_grupo, par.pmf);
                else printf("escolha%d nada\n", x);
            }
        } else if (!strcmp(cmd, "supx")) {
            wpa_par_t par;
            unsigned v[6];
            memset(&par, 0, sizeof par);
            if (sscanf(p1, "%u,%u,%u,%u,%u,%u", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) { printf("ie -\nversao -\n"); fflush(stdout); continue; }
            par.wpa1 = (uint8_t)v[0]; par.akm = (uint8_t)v[1]; par.cifra_par = (uint8_t)v[2];
            par.cifra_grupo = (uint8_t)v[3]; par.pmf = (uint8_t)v[4]; par.h2e = (uint8_t)v[5];
            uint8_t pmk[32], aa[6], spa[6], rsnxe[16], pmkid[16], sn[32];
            hex(p2, pmk, 32);
            hex(p3, aa, 6);
            hex(p4, spa, 6);
            int nie = hex(p5, b, sizeof b);
            int nx = hex(p6, rsnxe, sizeof rsnxe);
            int np = hex(p7, pmkid, sizeof pmkid);
            hex(p8, sn, 32);
            wpa_sup_preparar(&sup, &par, pmk, aa, spa, b, (uint32_t)nie, nx ? rsnxe : NULL, (uint32_t)nx,
                             np == 16 ? pmkid : NULL, sn);
            imprimir("ie", sup.ie, sup.nie);
            printf("versao %u\n", sup.versao);
        } else if (!strcmp(cmd, "chaves")) {
            imprimir("ptk", sup.ptk, sup.ptk_pronta ? sup.nptk : 0);
            imprimir("gtk", sup.gtk, sup.ngtk);
            printf("gtkid %u\n", sup.gtk_id);
            imprimir("igtk", sup.igtk, sup.tem_igtk ? 16 : 0);
            printf("igtkid %u\n", sup.igtk_id);
            imprimir("ipn", sup.ipn, sup.tem_igtk ? 6 : 0);
            printf("pmkid %u %u\n", sup.pmkid_visto, sup.pmkid_ok);
        } else if (!strcmp(cmd, "falhamic")) {
            uint32_t m = wpa_falha_mic(&sup, atoi(p1) != 0, b);
            imprimir("resp", b, (int)m);
        }
        fflush(stdout);
    }
    return 0;
}
