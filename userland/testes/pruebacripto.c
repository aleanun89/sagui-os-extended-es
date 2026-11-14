// SPDX-License-Identifier: GPL-2.0-or-later
/* pruebacripto - confere as primitivas da libtls com os vetores oficiais (D060).
 *
 *   pruebacripto        roda todos os vetores e diz quantos passaram
 *   pruebacripto -r     também mede a velocidade (AES-GCM, ChaCha20, X25519...)
 *
 * Origem de cada vetor: FIPS 180-4 (exemplos do NIST), FIPS 197 (apêndice
 * C), GCM (casos 2, 4 e 16 do documento de McGrew e Viega que a SP 800-38D
 * cita), RFC 4231 (HMAC), RFC 5869 (HKDF), RFC 8439 (ChaCha20, Poly1305,
 * AEAD), RFC 7748 (X25519), RFC 6979 (chaves e assinaturas ECDSA P-256 e
 * P-384) e RSA gerado com o OpenSSL (pruebacripto_rsa.h). O tests/h59_tls.py
 * roda este programa e exige "pruebacripto: N/N vectores correctos". */
#include "cripto.h"
#include "pruebacripto_rsa.h"

static int total, certos;

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Converte hexadecimal (espaços ignorados) em bytes; devolve quantos. */
static unsigned hex(const char *s, uint8_t *b)
{
    unsigned n = 0;
    while (*s) {
        if (hexval(*s) < 0) { s++; continue; }
        b[n++] = (uint8_t)(hexval(s[0]) << 4 | hexval(s[1]));
        s += 2;
    }
    return n;
}

static void conferir(const char *nome, const uint8_t *obtido, const char *esperado_hex)
{
    static uint8_t esp[512];
    unsigned n = hex(esperado_hex, esp);
    total++;
    if (memcmp(obtido, esp, n) == 0) { certos++; return; }
    imprimir("FALLO: %s\n  esperado ", nome);
    for (unsigned i = 0; i < n; i++) imprimir("%02x", esp[i]);
    imprimir("\n  obtenido ");
    for (unsigned i = 0; i < n; i++) imprimir("%02x", obtido[i]);
    imprimir("\n");
}

static void conferir_bool(const char *nome, bool ok)
{
    total++;
    if (ok) certos++;
    else imprimir("FALLO: %s\n", nome);
}

static uint8_t buf[1024], buf2[1024], buf3[1024], saida[1024];

static void testar_hashes(void)
{
    uint8_t h[64];
    sha256("abc", 3, h);
    conferir("SHA-256 abc", h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    sha256("", 0, h);
    conferir("SHA-256 vacio", h, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const char *m448 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    sha256(m448, (unsigned)tam_texto(m448), h);
    conferir("SHA-256 448 bits", h, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    /* um milhão de 'a', em pedaços de tamanhos variados (testa o acúmulo) */
    sha256_t c;
    sha256_iniciar(&c);
    memset(buf, 'a', 1000);
    for (int i = 0, k = 0; i < 1000000; i += k) {
        k = 1 + (i % 997);
        if (i + k > 1000000) k = 1000000 - i;
        sha256_somar(&c, buf, (unsigned)k);
    }
    sha256_fim(&c, h);
    conferir("SHA-256 un millon de a", h, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

    hash_calcular(&HASH_SHA384, "abc", 3, h);
    conferir("SHA-384 abc", h, "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7");
    hash_calcular(&HASH_SHA512, "abc", 3, h);
    conferir("SHA-512 abc", h, "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
    const char *m896 = "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
    hash_calcular(&HASH_SHA512, m896, (unsigned)tam_texto(m896), h);
    conferir("SHA-512 896 bits", h, "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909");
    hash_calcular(&HASH_SHA384, m896, (unsigned)tam_texto(m896), h);
    conferir("SHA-384 896 bits", h, "09330c33f71147e83d192fc782cd1b4753111b173b3b05d22fa08086e3b0f712fcc7c71a557e2db966c3e9fa91746039");
}

static void testar_hmac_hkdf(void)
{
    uint8_t h[64], k[256];
    /* RFC 4231, casos 1, 2 e 6 */
    memset(k, 0x0b, 20);
    hmac(&HASH_SHA256, k, 20, "Hi There", 8, h);
    conferir("HMAC-SHA-256 caso 1", h, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    hmac(&HASH_SHA384, k, 20, "Hi There", 8, h);
    conferir("HMAC-SHA-384 caso 1", h, "afd03944d84895626b0825f4ab46907f15f9dadbe4101ec682aa034c7cebc59cfaea9ea9076ede7f4af152e8b2fa9cb6");
    hmac(&HASH_SHA512, k, 20, "Hi There", 8, h);
    conferir("HMAC-SHA-512 caso 1", h, "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cdedaa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854");
    const char *d2 = "what do ya want for nothing?";
    hmac(&HASH_SHA256, "Jefe", 4, d2, (unsigned)tam_texto(d2), h);
    conferir("HMAC-SHA-256 caso 2", h, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    memset(k, 0xaa, 131);
    const char *d6 = "Test Using Larger Than Block-Size Key - Hash Key First";
    hmac(&HASH_SHA256, k, 131, d6, (unsigned)tam_texto(d6), h);
    conferir("HMAC-SHA-256 caso 6 (clave larga)", h, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
    hmac(&HASH_SHA384, k, 131, d6, (unsigned)tam_texto(d6), h);
    conferir("HMAC-SHA-384 caso 6", h, "4ece084485813e9088d2c63a041bc5b44f9ef1012a2b588f3cd11f05033ac4c60c2ef6ab4030fe8296248df163f44952");
    hmac(&HASH_SHA512, k, 131, d6, (unsigned)tam_texto(d6), h);
    conferir("HMAC-SHA-512 caso 6", h, "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f3526b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598");
    /* RFC 5869, caso 1 */
    uint8_t ikm[22], sal[13], info[10], prk[32], okm[42];
    memset(ikm, 0x0b, 22);
    for (int i = 0; i < 13; i++) sal[i] = (uint8_t)i;
    for (int i = 0; i < 10; i++) info[i] = (uint8_t)(0xf0 + i);
    hkdf_extrair(&HASH_SHA256, sal, 13, ikm, 22, prk);
    conferir("HKDF caso 1 PRK", prk, "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5");
    hkdf_expandir(&HASH_SHA256, prk, info, 10, okm, 42);
    conferir("HKDF caso 1 OKM", okm, "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865");
    /* RFC 5869, caso 3 (sem sal nem info) */
    hkdf_extrair(&HASH_SHA256, NULL, 0, ikm, 22, prk);
    conferir("HKDF caso 3 PRK", prk, "19ef24a32c717b167f33a91d6f648bdf96596776afdb6377ac434c1c293ccb04");
    hkdf_expandir(&HASH_SHA256, prk, "", 0, okm, 42);
    conferir("HKDF caso 3 OKM", okm, "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8");
}

static void testar_aes_gcm(void)
{
    aes_t a;
    uint8_t k[32], p[64];
    /* FIPS 197, C.1 e C.3 */
    hex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", k);
    hex("00112233445566778899aabbccddeeff", p);
    aes_chave(&a, k, 16);
    aes_cifrar(&a, p, saida, 1);
    conferir("AES-128 FIPS-197 C.1", saida, "69c4e0d86a7b0430d8cdb78070b4c55a");
    aes_chave(&a, k, 32);
    aes_cifrar(&a, p, saida, 1);
    conferir("AES-256 FIPS-197 C.3", saida, "8ea2b7ca516745bfeafc49904b496089");
    /* dois blocos de uma vez dão o mesmo que um por um */
    memcpy(p + 16, p, 16);
    p[20] ^= 1;
    aes_cifrar(&a, p, saida, 2);
    aes_cifrar(&a, p, saida + 32, 1);
    aes_cifrar(&a, p + 16, saida + 64, 1);
    conferir_bool("AES dos bloques a la vez", memcmp(saida, saida + 32, 16) == 0 && memcmp(saida + 16, saida + 64, 16) == 0);

    gcm_t g;
    uint8_t iv[12], tag[16], aad[20];
    /* GCM caso 2: chave zero, 16 bytes zero */
    memset(k, 0, 16);
    memset(iv, 0, 12);
    memset(p, 0, 16);
    gcm_chave(&g, k, 16);
    gcm_cifrar(&g, iv, NULL, 0, p, saida, 16, tag);
    conferir("GCM caso 2 cifrado", saida, "0388dace60b6a392f328c2b971b2fe78");
    conferir("GCM caso 2 etiqueta", tag, "ab6e47d42cec13bdf53a67b21257bddf");
    /* casos 4 (AES-128) e 16 (AES-256): 60 bytes com AAD */
    unsigned np = hex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39", buf);
    hex("feedfacedeadbeeffeedfacedeadbeefabaddad2", aad);
    hex("cafebabefacedbaddecaf888", iv);
    hex("feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308", k);
    gcm_chave(&g, k, 16);
    gcm_cifrar(&g, iv, aad, 20, buf, saida, np, tag);
    conferir("GCM caso 4 cifrado", saida, "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091");
    conferir("GCM caso 4 etiqueta", tag, "5bc94fbc3221a5db94fae95ae7121a47");
    conferir_bool("GCM caso 4 descifra", gcm_decifrar(&g, iv, aad, 20, saida, buf2, np, tag) && memcmp(buf2, buf, np) == 0);
    tag[3] ^= 0x10;
    conferir_bool("GCM rechaza etiqueta alterada", !gcm_decifrar(&g, iv, aad, 20, saida, buf2, np, tag));
    gcm_chave(&g, k, 32);
    gcm_cifrar(&g, iv, aad, 20, buf, saida, np, tag);
    conferir("GCM caso 16 cifrado", saida, "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662");
    conferir("GCM caso 16 etiqueta", tag, "76fc6ece0f4e1768cddf8853bb2d551b");
}

static void testar_chacha(void)
{
    uint8_t k[32], n[12], tag[16], aad[12];
    for (int i = 0; i < 32; i++) k[i] = (uint8_t)i;
    hex("000000090000004a00000000", n);
    chacha20_bloco(k, 1, n, saida);
    conferir("ChaCha20 bloque (RFC 8439 2.3.2)", saida,
             "10f1e7e4d13b5915500fdd1fa32071c4c7d1f4c733c068030422aa9ac3d46c4ed2826446079faa0914c2d705d98b02a2b5129cd1de164eb9cbd083e8a2503c4e");
    hex("85d6be7857556d337f4452fe42d506a80103808afb0db2fd4abff6af4149f51b", k);
    const char *msg = "Cryptographic Forum Research Group";
    poly1305(k, (const uint8_t *)msg, (unsigned)tam_texto(msg), tag);
    conferir("Poly1305 (RFC 8439 2.5.2)", tag, "a8061dc1305136c6c22b8baf0c0127a9");
    const char *pt = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
    unsigned npt = (unsigned)tam_texto(pt);
    hex("50515253c0c1c2c3c4c5c6c7", aad);
    hex("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f", k);
    hex("070000004041424344454647", n);
    chachapoly_cifrar(k, n, aad, 12, (const uint8_t *)pt, saida, npt, tag);
    conferir("ChaCha20-Poly1305 cifrado (2.8.2)", saida,
             "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116");
    conferir("ChaCha20-Poly1305 etiqueta", tag, "1ae10b594f09e26a7e902ecbd0600691");
    conferir_bool("ChaCha20-Poly1305 descifra", chachapoly_decifrar(k, n, aad, 12, saida, buf, npt, tag) && memcmp(buf, pt, npt) == 0);
    saida[5] ^= 1;
    conferir_bool("ChaCha20-Poly1305 rechaza datos alterados", !chachapoly_decifrar(k, n, aad, 12, saida, buf, npt, tag));
}

static void testar_x25519(void)
{
    uint8_t e[32], u[32], r[32], a[32], b[32], pa[32], pb[32];
    hex("a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4", e);
    hex("e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c", u);
    x25519(r, e, u);
    conferir("X25519 vector 1 (RFC 7748 5.2)", r, "c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552");
    hex("4b66e9d4d1b4673c5ad22691957d6af5c11b6421e0ea01d42ca4169e7918ba0d", e);
    hex("e5210f12786811d3f4b7959d0538ae2c31dbe7106fc03c3efc4cd549c715a493", u);
    x25519(r, e, u);
    conferir("X25519 vector 2", r, "95cbde9476e8907d7aade45cb4b873f88b595a68799fa152e6f8f7647aac7957");
    /* 5.2, iterado: k = X25519(k, u), u = k antigo */
    memset(e, 0, 32);
    e[0] = 9;
    memcpy(u, e, 32);
    for (int i = 0; i < 1000; i++) {
        x25519(r, e, u);
        memcpy(u, e, 32);
        memcpy(e, r, 32);
        if (i == 0) conferir("X25519 1 iteracion", e, "422c8e7a6227d7bca1350b3e2bb7279f7897b87bb6854b783c60e80311ae3079");
    }
    conferir("X25519 1000 iteraciones", e, "684cf59ba83309552800ef566f2f4d3c1c3887c49360e3875f2eb94d99532c51");
    /* 6.1: Diffie-Hellman de Alice e Bob */
    hex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a", a);
    hex("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb", b);
    x25519_base(pa, a);
    conferir("X25519 clave publica de Alice", pa, "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
    x25519_base(pb, b);
    conferir("X25519 clave publica de Bob", pb, "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f");
    x25519(r, a, pb);
    conferir("X25519 secreto compartido", r, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
}

/* Ed25519 (RFC 8032, 7.1) y el cifrado de paquetes del SSH (D250). Los mismos
 * vectores, y más casos, se prueban en el anfitrión (tests/ssh_unidad.c);
 * aquí se comprueba el código de 32 bits que corre en SaguiOS. */
static void testar_ed25519(void)
{
    static const struct { const char *semente, *publica, *msg, *assin; int n; } v[] = {
        { "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
          "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "", 
          "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b", 0 },
        { "4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
          "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
          "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00", 1 },
        { "c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7",
          "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", "af82",
          "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a", 2 },
    };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        uint8_t sem[32], pub[32], msg[4], sig[64];
        hex(v[i].semente, sem);
        if (v[i].n) hex(v[i].msg, msg);
        ed25519_publica(pub, sem);
        conferir("Ed25519 clave publica (RFC 8032 7.1)", pub, v[i].publica);
        ed25519_assinar(sig, msg, (unsigned)v[i].n, sem, pub);
        conferir("Ed25519 firma", sig, v[i].assin);
        conferir_bool("Ed25519 la firma se verifica", ed25519_verificar(sig, msg, (unsigned)v[i].n, pub));
        sig[7] ^= 2;
        conferir_bool("Ed25519 rechaza una firma alterada", !ed25519_verificar(sig, msg, (unsigned)v[i].n, pub));
    }
    /* chacha20-poly1305@openssh.com: ida y vuelta, y rechazo de lo alterado */
    uint8_t chave[64], pkt[4 + 32 + 16], orig[4 + 32];
    for (int i = 0; i < 64; i++) chave[i] = (uint8_t)(i * 5 + 1);
    pkt[0] = pkt[1] = pkt[2] = 0;
    pkt[3] = 32;
    for (int i = 0; i < 32; i++) pkt[4 + i] = (uint8_t)(200 - i);
    memcpy(orig, pkt, sizeof orig);
    ssh_chapoly_selar(chave, 7, pkt, 32);
    conferir("chacha20-poly1305@openssh.com paquete cifrado", pkt,
             "e917e83922291122f7b9fa9660cb46d01ca98f5edd30b9f06ee46d0ccdb1572f2d5e53ec3ef2112a834a6e999da5602d19d5a48c");
    conferir_bool("chacha20-poly1305@openssh.com tamano", ssh_chapoly_tamanho(chave, 7, pkt) == 32);
    pkt[10] ^= 1;
    conferir_bool("chacha20-poly1305@openssh.com rechaza un paquete alterado", !ssh_chapoly_abrir(chave, 7, pkt, 32));
    pkt[10] ^= 1;
    conferir_bool("chacha20-poly1305@openssh.com rechaza otra secuencia", !ssh_chapoly_abrir(chave, 8, pkt, 32));
    conferir_bool("chacha20-poly1305@openssh.com abre", ssh_chapoly_abrir(chave, 7, pkt, 32) && memcmp(pkt + 4, orig + 4, 32) == 0);
}

static void testar_ecc(void)
{
    uint8_t x[48], pub[97], h[64], r[48], s[48], seg1[48], seg2[48], priv2[48], pub2[97];
    /* RFC 6979 A.2.5: a chave pública sai da privada e as assinaturas "sample" e "test" (SHA-256) conferem */
    hex("C9AFA9D845BA75166B5C215767B1D6934E50C3DB36E89B127B8A622B120F6721", x);
    conferir_bool("P-256 k*G", ec_multiplicar_base(CURVA_P256, x, pub));
    conferir("P-256 clave publica (RFC 6979 A.2.5)", pub + 1,
             "60FED4BA255A9D31C961EB74C6356D68C049B8923B61FA6CE669622E60F29FB6"
             "7903FE1008B8BC99A41AE9E95628BC64F2F1B20C2D7E9F5177A3C294D4462299");
    sha256("sample", 6, h);
    hex("EFD48B2AACB6A8FD1140DD9CD45E81D69D2C877B56AAF991C34D0EA84EAF3716", r);
    hex("F7CB1C942D657C41D436C7A1B6E29F65F3E900DBB9AFF4064DC4AB2F843ACDA8", s);
    conferir_bool("ECDSA P-256 SHA-256 \"sample\"", ecdsa_verificar(CURVA_P256, pub, 65, h, 32, r, 32, s, 32));
    h[0] ^= 1;
    conferir_bool("ECDSA P-256 rechaza hash alterado", !ecdsa_verificar(CURVA_P256, pub, 65, h, 32, r, 32, s, 32));
    hash_calcular(&HASH_SHA384, "sample", 6, h);
    hex("0EAFEA039B20E9B42309FB1D89E213057CBF973DC0CFC8F129EDDDC800EF7719", r);
    hex("4861F0491E6998B9455193E34E7B0D284DDD7149A74B95B9261F13ABDE940954", s);
    conferir_bool("ECDSA P-256 SHA-384 \"sample\" (hash truncado)", ecdsa_verificar(CURVA_P256, pub, 65, h, 48, r, 32, s, 32));
    sha256("test", 4, h);
    hex("F1ABB023518351CD71D881567B1EA663ED3EFCF6C5132B354F28D3B0B7D38367", r);
    hex("019F4113742A2B14BD25926B49C649155F267E60D3814B4C0CC84250E46F0083", s);
    conferir_bool("ECDSA P-256 SHA-256 \"test\"", ecdsa_verificar(CURVA_P256, pub, 65, h, 32, r, 32, s, 32));
    /* RFC 6979 A.2.6 (P-384) */
    hex("6B9D3DAD2E1B8C1C05B19875B6659F4DE23C3B667BF297BA9AA47740787137D896D5724E4C70A825F872C9EA60D2EDF5", x);
    conferir_bool("P-384 k*G", ec_multiplicar_base(CURVA_P384, x, pub));
    conferir("P-384 clave publica (RFC 6979 A.2.6)", pub + 1,
             "EC3A4E415B4E19A4568618029F427FA5DA9A8BC4AE92E02E06AAE5286B300C64DEF8F0EA9055866064A254515480BC13"
             "8015D9B72D7D57244EA8EF9AC0C621896708A59367F9DFB9F54CA84B3F1C9DB1288B231C3AE0D4FE7344FD2533264720");
    hash_calcular(&HASH_SHA384, "sample", 6, h);
    hex("94EDBB92A5ECB8AAD4736E56C691916B3F88140666CE9FA73D64C4EA95AD133C81A648152E44ACF96E36DD1E80FABE46", r);
    hex("99EF4AEB15F178CEA1FE40DB2603138F130E740A19624526203B6351D0A3A94FA329C145786E679E7B82C71A38628AC8", s);
    conferir_bool("ECDSA P-384 SHA-384 \"sample\"", ecdsa_verificar(CURVA_P384, pub, 97, h, 48, r, 48, s, 48));
    sha256("sample", 6, h);
    hex("21B13D1E013C7FA1392D03C5F99AF8B30C570C6F98D4EA8E354B63A21D3DAA33BDE1E888E63355D92FA2B3C36D8FB2CD", r);
    hex("F3AA443FB107745BF4BD77CB3891674632068A10CA67E3D45DB2266FA7D1FEEBEFDC63ECCD1AC42EC0CB8668A4FA0AB0", s);
    conferir_bool("ECDSA P-384 SHA-256 \"sample\"", ecdsa_verificar(CURVA_P384, pub, 97, h, 32, r, 48, s, 48));
    s[10] ^= 4;
    conferir_bool("ECDSA P-384 rechaza firma alterada", !ecdsa_verificar(CURVA_P384, pub, 97, h, 32, r, 48, s, 48));
    /* ECDH: os dois lados chegam ao mesmo segredo; um ponto fora da curva é recusado */
    for (int c = 0; c < 2; c++) {
        int id = c ? CURVA_P384 : CURVA_P256;
        unsigned t = ec_tam(id);
        conferir_bool("ECDH genera claves", ecdh_gerar(id, x, pub) && ecdh_gerar(id, priv2, pub2));
        conferir_bool(c ? "ECDH P-384 secreto igual" : "ECDH P-256 secreto igual",
                      ecdh_segredo(id, x, pub2, 1 + 2 * t, seg1) && ecdh_segredo(id, priv2, pub, 1 + 2 * t, seg2) &&
                      memcmp(seg1, seg2, t) == 0);
        pub2[5] ^= 1;
        conferir_bool("ECDH rechaza punto fuera de la curva", !ecdh_segredo(id, x, pub2, 1 + 2 * t, seg1));
    }
}

static void testar_rsa(void)
{
    static const uint8_t e[] = { 1, 0, 1 };
    uint8_t h[64];
    unsigned nm = (unsigned)tam_texto(RSA_MSG);
    sha256(RSA_MSG, nm, h);
    conferir_bool("RSA-2048 PKCS#1 v1.5 SHA-256", rsa_verificar_pkcs1(RSA2048_N, sizeof RSA2048_N, e, 3, &HASH_SHA256, h, RSA_ASSIN1, sizeof RSA_ASSIN1));
    conferir_bool("RSA-2048 PSS SHA-256", rsa_verificar_pss(RSA2048_N, sizeof RSA2048_N, e, 3, &HASH_SHA256, h, RSA_ASSIN2, sizeof RSA_ASSIN2, 32));
    conferir_bool("RSA-2048 PKCS#1 no acepta firma PSS", !rsa_verificar_pkcs1(RSA2048_N, sizeof RSA2048_N, e, 3, &HASH_SHA256, h, RSA_ASSIN2, sizeof RSA_ASSIN2));
    h[31] ^= 1;
    conferir_bool("RSA-2048 rechaza hash alterado", !rsa_verificar_pkcs1(RSA2048_N, sizeof RSA2048_N, e, 3, &HASH_SHA256, h, RSA_ASSIN1, sizeof RSA_ASSIN1) &&
                  !rsa_verificar_pss(RSA2048_N, sizeof RSA2048_N, e, 3, &HASH_SHA256, h, RSA_ASSIN2, sizeof RSA_ASSIN2, 32));
    hash_calcular(&HASH_SHA512, RSA_MSG, nm, h);
    conferir_bool("RSA-4096 PKCS#1 v1.5 SHA-512", rsa_verificar_pkcs1(RSA4096_N, sizeof RSA4096_N, e, 3, &HASH_SHA512, h, RSA_ASSIN3, sizeof RSA_ASSIN3));
    hash_calcular(&HASH_SHA384, RSA_MSG, nm, h);
    conferir_bool("RSA-4096 PSS SHA-384", rsa_verificar_pss(RSA4096_N, sizeof RSA4096_N, e, 3, &HASH_SHA384, h, RSA_ASSIN4, sizeof RSA_ASSIN4, -1));
}

/* O gerador do kernel (D061): dois pedidos seguidos diferem, e /dev/urandom funciona. */
static void testar_aleatorio(void)
{
#ifndef HOST_TESTE
    uint8_t a[32], b[32], c[64];
    conferir_bool("SIS_ALEATORIO entrega 32 bytes", bytes_aleatorios(a, 32) == 32 && bytes_aleatorios(b, 32) == 32);
    conferir_bool("SIS_ALEATORIO: dos pedidos distintos", memcmp(a, b, 32) != 0);
    int fd = abrir("/dev/urandom", ABRIR_LER);
    conferir_bool("/dev/urandom se abre y da 64 bytes", fd >= 0 && ler(fd, c, 64) == 64);
    if (fd >= 0) fechar(fd);
    conferir_bool("/dev/urandom distinto de lo anterior", memcmp(c, a, 32) != 0 && memcmp(c + 32, b, 32) != 0);
    info_aleatorio_t i;
    if (info_aleatorio(&i) == 0)
        imprimir("aleatorio: RDRAND %s, RDSEED %s, %u interrupciones mezcladas\n", (i.fontes & ALEA_RDRAND) ? "sí" : "no",
                 (i.fontes & ALEA_RDSEED) ? "sí" : "no", i.eventos);
#endif
}

/* ------------------------------------------------------------ velocidade */

static void medir(void)
{
    static uint8_t dados[65536];
    uint8_t k[32], n[12], tag[16];
    memset(k, 7, 32);
    memset(n, 1, 12);
    gcm_t g;
    gcm_chave(&g, k, 16);
    unsigned t0 = tempo_ms();
    for (int i = 0; i < 4; i++) gcm_cifrar(&g, n, NULL, 0, dados, dados, sizeof dados, tag);
    unsigned t1 = tempo_ms();
    for (int i = 0; i < 4; i++) chachapoly_cifrar(k, n, NULL, 0, dados, dados, sizeof dados, tag);
    unsigned t2 = tempo_ms();
    imprimir("AES-128-GCM: %u KB/s   ChaCha20-Poly1305: %u KB/s\n",
             256000 / (t1 - t0 + 1), 256000 / (t2 - t1 + 1));
    if (aes_hardware()) {
        aes_forcar_software(true);
        t0 = tempo_ms();
        for (int i = 0; i < 4; i++) gcm_cifrar(&g, n, NULL, 0, dados, dados, sizeof dados, tag);
        t1 = tempo_ms();
        aes_forcar_software(false);
        imprimir("AES-128-GCM sin AES-NI: %u KB/s\n", 256000 / (t1 - t0 + 1));
    }
    uint8_t r[32];
    t0 = tempo_ms();
    for (int i = 0; i < 4; i++) x25519_base(r, k);
    t1 = tempo_ms();
    uint8_t priv[48], pub[97], seg[48];
    ecdh_gerar(CURVA_P256, priv, pub);
    t2 = tempo_ms();
    ecdh_segredo(CURVA_P256, priv, pub, 65, seg);
    unsigned t3 = tempo_ms();
    static const uint8_t e[] = { 1, 0, 1 };
    uint8_t h[32];
    sha256(RSA_MSG, (unsigned)tam_texto(RSA_MSG), h);
    for (int i = 0; i < 4; i++) rsa_verificar_pkcs1(RSA2048_N, sizeof RSA2048_N, e, 3, &HASH_SHA256, h, RSA_ASSIN1, sizeof RSA_ASSIN1);
    unsigned t4 = tempo_ms();
    imprimir("X25519: %u ms   P-256 (clave + secreto): %u ms   RSA-2048 verificar: %u ms\n",
             (t1 - t0) / 4, t3 - t1, (t4 - t3) / 4);
}

int principal(int argc, char **argv)
{
    testar_hashes();
    testar_hmac_hkdf();
    testar_aes_gcm();
    if (aes_hardware()) {   /* com AES-NI, confere também a versão em C (sem tabelas) */
        aes_forcar_software(true);
        testar_aes_gcm();
        aes_forcar_software(false);
    }
    testar_chacha();
    testar_x25519();
    testar_ed25519();
    testar_ecc();
    testar_rsa();
    testar_aleatorio();
    imprimir("pruebacripto: %d/%d vectores correctos\n", certos, total);
    imprimir("AES: %s\n", aes_hardware() ? "instrucciones AES-NI de la CPU" : "en C, sin tablas (la CPU no tiene AES-NI)");
    if (argc > 1 && compara_texto(argv[1], "-r") == 0) medir();
    (void)buf3;
    return certos == total ? 0 : 1;
}

#ifdef HOST_TESTE
int main(int argc, char **argv) { return principal(argc, argv); }
#endif
