// SPDX-License-Identifier: GPL-2.0-or-later
/* aesni.c - AES com as instruções AES-NI, quando a CPU as tem (D060).
 *
 * Único arquivo da libtls compilado sem -mgeneral-regs-only (regra própria no
 * Makefile, como o simd e o sse2.c): o GCC pode usar registradores XMM em
 * qualquer função daqui, por isso aes.c só chama isto depois de o CPUID dizer
 * que há SSE2 e AES-NI. O kernel guarda os XMM de cada processo (D032).
 *
 * AESENC faz uma rodada inteira (SubBytes, ShiftRows, MixColumns e
 * AddRoundKey) no hardware, em tempo constante e sem tabelas: além de muito
 * mais rápido, é a melhor proteção contra os ataques de cache. As chaves de
 * rodada são as mesmas da versão em C (aes_chave), na ordem de bytes da FIPS
 * 197, que é a que as instruções esperam. Sem cabeçalhos do sistema: os tipos
 * vetoriais e as instruções vêm das extensões do GCC. */
typedef long long v2di __attribute__((vector_size(16), may_alias));

void aesni_cifrar(const unsigned char (*rk)[16], int rodadas, const unsigned char *entrada,
                  unsigned char *saida, int blocos)
{
    v2di k[15];
    for (int r = 0; r <= rodadas; r++) __builtin_memcpy(&k[r], rk[r], 16);
    for (int b = 0; b < blocos; b++) {
        v2di s;
        __builtin_memcpy(&s, entrada + 16 * b, 16);
        s ^= k[0];
        for (int r = 1; r < rodadas; r++) s = __builtin_ia32_aesenc128(s, k[r]);
        s = __builtin_ia32_aesenclast128(s, k[rodadas]);
        __builtin_memcpy(saida + 16 * b, &s, 16);
    }
    volatile char *v = (volatile char *)k;   /* não deixa as chaves na pilha */
    for (unsigned i = 0; i < sizeof k; i++) v[i] = 0;
}
