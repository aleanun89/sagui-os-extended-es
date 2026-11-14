// SPDX-License-Identifier: GPL-2.0-or-later
/* sse2 - teste da H40 (D042): as versões rep e SSE2 de memcpy, memmove,
 * memset, preencher32, ampliar2x32 e escurecer32 dão exatamente o mesmo
 * resultado que os laços simples em C, em muitos tamanhos e alinhamentos, e
 * não escrevem fora do lugar (bytes de guarda em volta do destino). Com SSE2,
 * também a conversão para a placa em 16 e 15 bits (sse2_converter16, D078),
 * a mesma que o kernel usa na cópia do buffer de trás.
 *
 * Uso: /testes/sse2       imprime "sse2: ok (...)" ou "sse2: FALLO ..." */
#include "sagui.h"
#include "sse2.h"

#define TAM 20000   /* cada buffer; o maior teste usa 16 KB + desalinho + guardas */
#define GUARDA 48

static uint8_t *ref, *res, *orig;
static uint32_t semente = 12345;
static int comparacoes, falhas;

static uint32_t aleatorio(void)
{
    semente = semente * 1103515245u + 12345u;
    return semente >> 8;
}

static void encher(uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)aleatorio();
}

/* Os dois destinos começam iguais (lixo aleatório) nos primeiros n bytes;
 * a origem também é aleatória. */
static void preparar(size_t n)
{
    if (n > TAM) n = TAM;
    encher(ref, n);
    for (size_t i = 0; i < n; i++) res[i] = ref[i];
    encher(orig, n);
}

static void comparar(const char *op, int metodo, size_t n, int da, int oa, size_t ini, size_t fim)
{
    comparacoes++;
    for (size_t i = ini; i < fim; i++)
        if (ref[i] != res[i]) {
            if (falhas++ < 8)
                imprimir("sse2: FALLO %s metodo %d n=%u dst+%d src+%d: byte %d difiere (%x en vez de %x)\n",
                         op, metodo, (unsigned)n, da, oa, (int)i - (int)GUARDA - da, res[i], ref[i]);
            return;
        }
}

enum { OP_MEMCPY, OP_MEMMOVE_FRENTE, OP_MEMMOVE_TRAS, OP_MEMSET, OP_PREENCHER, OP_AMPLIAR, OP_AMPLIAR2, OP_ESCURECER, OP_TROCAR, OP_TROCAR_LUGAR, OP_MISTURAR, QTD_OP };
static const char *const nomes[QTD_OP] = { "memcpy", "memmove>", "memmove<", "memset", "preencher32",
                                           "ampliar2x32", "ampliar2x32x2", "escurecer32", "trocar_rb32", "trocar_rb32=", "misturar32" };

static void aplicar(int op, uint8_t *b, size_t n, int da, int oa)
{
    uint8_t *d = b + GUARDA + da;
    switch (op) {
    case OP_MEMCPY:  memcpy(d, orig + GUARDA + oa, n); break;
    case OP_MEMMOVE_FRENTE: memmove(d, d + 1 + oa, n); break;    /* origem depois do destino */
    case OP_MEMMOVE_TRAS:   memmove(d + 1 + oa, d, n); break;    /* destino depois da origem */
    case OP_MEMSET:  memset(d, (int)(n + (size_t)oa), n); break;
    case OP_PREENCHER: preencher32((uint32_t *)(b + GUARDA + (da & ~3)), 0x01020304u * (uint32_t)(oa + 1), n / 4); break;
    case OP_AMPLIAR: ampliar2x32((uint32_t *)(b + GUARDA + (da & ~3)), NULL, (const uint32_t *)(orig + GUARDA + oa), n / 8); break;
    case OP_AMPLIAR2: {
        uint32_t *d1 = (uint32_t *)(b + GUARDA + (da & ~3));
        ampliar2x32(d1, d1 + n / 4 + 8, (const uint32_t *)(orig + GUARDA + oa), n / 8);
        break;
    }
    case OP_TROCAR: trocar_rb32((uint32_t *)(b + GUARDA + (da & ~3)), (const uint32_t *)(orig + GUARDA + oa), n / 4); break;
    case OP_TROCAR_LUGAR: {   /* en el mismo bloque: d == o */
        uint32_t *p = (uint32_t *)(b + GUARDA + (da & ~3));
        trocar_rb32(p, p, n / 4);
        break;
    }
    case OP_MISTURAR: misturar32((uint32_t *)(b + GUARDA + (da & ~3)), (const uint32_t *)(orig + GUARDA + oa), n / 4,
                                 (unsigned)(oa * 17 + (int)(n & 7) * 31) % 257); break;
    case OP_ESCURECER: escurecer32((uint32_t *)(b + GUARDA + (da & ~3)), n / 4, 1 + oa % 3, 0x7F3F1F); break;
    }
}

static size_t alcance(int op, size_t n)
{
    if (op == OP_MEMMOVE_FRENTE || op == OP_MEMMOVE_TRAS) return n + 17;
    if (op == OP_AMPLIAR2) return 2 * n + 64;
    return n + 16;
}

static void testar(int op, int metodo, size_t n, int da, int oa)
{
    size_t fim = GUARDA + alcance(op, n) + GUARDA;
    preparar(fim + 32);
    mem_metodo(MEM_SIMPLES);
    aplicar(op, ref, n, da, oa);
    mem_metodo(metodo);
    aplicar(op, res, n, da, oa);
    mem_metodo(MEM_AUTO);
    comparar(nomes[op], metodo, n, da, oa, 0, fim);
}

static void rodar(int metodo)
{
    for (int op = 0; op < QTD_OP; op++) {
        /* Todos os tamanhos até 300 bytes com 4 alinhamentos de cada lado,
         * e todos os 16 x 16 alinhamentos em alguns tamanhos. */
        for (size_t n = 0; n <= 300; n++)
            for (int da = 0; da < 16; da += 5)
                for (int oa = 0; oa < 16; oa += 7)
                    testar(op, metodo, n, da, oa);
        static const size_t medios[] = { 15, 16, 17, 63, 64, 65, 255, 256, 257, 1000, 1024, 1029 };
        for (unsigned k = 0; k < sizeof medios / sizeof medios[0]; k++)
            for (int da = 0; da < 16; da++)
                for (int oa = 0; oa < 16; oa++)
                    testar(op, metodo, medios[k], da, oa);
        static const size_t grandes[] = { 4096, 4100, 8191, 8192 };
        for (unsigned k = 0; k < sizeof grandes / sizeof grandes[0]; k++)
            for (int da = 0; da < 16; da += 3)
                testar(op, metodo, grandes[k], da, 15 - da);
    }
}

/* D078: 0x00RRGGBB -> RGB565 ou RGB555, a conta de referência. */
static uint16_t pixel16(uint32_t c, int r555)
{
    return (uint16_t)(r555 ? ((c >> 9) & 0x7C00) | ((c >> 6) & 0x03E0) | ((c >> 3) & 0x1F)
                           : ((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x1F));
}

static void testar_converter16(void)
{
    static uint32_t o[1100];
    static uint16_t d[1100 + 2 * 16];
    for (int r555 = 0; r555 < 2; r555++)
        for (unsigned n = 0; n <= 1029; n += n < 40 ? 1 : 97)
            for (int da = 0; da < 8; da++) {
                for (unsigned i = 0; i < n + 4; i++) o[i] = aleatorio() ^ (aleatorio() << 24);
                for (unsigned i = 0; i < sizeof d / sizeof d[0]; i++) d[i] = 0xA5A5;
                sse2_converter16(d + 8 + da, o + (da & 3), n, r555);
                comparacoes++;
                bool ok = true;
                for (unsigned i = 0; i < sizeof d / sizeof d[0]; i++) {
                    bool dentro = i >= 8u + (unsigned)da && i < 8u + (unsigned)da + n;
                    uint16_t esperado = dentro ? pixel16(o[(da & 3) + i - 8 - (unsigned)da], r555) : 0xA5A5;
                    if (d[i] != esperado) ok = false;
                }
                if (!ok && falhas++ < 8)
                    imprimir("sse2: FALLO converter16 %s n=%u dst+%d\n", r555 ? "555" : "565", n, da);
            }
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    ref = aloca(TAM);
    res = aloca(TAM);
    orig = aloca(TAM);
    if (!ref || !res || !orig) { imprimir("sse2: FALLO sin memoria\n"); return 1; }
    bool tem = cpu_tem_sse2();
    imprimir("sse2: la CPU %s SSE2 (SSE %s, SSE3 %s, SSSE3 %s)\n", tem ? "tiene" : "no tiene",
             cpu_tem_sse() ? "si" : "no", cpu_tem_sse3() ? "si" : "no", cpu_tem_ssse3() ? "si" : "no");
    /* D208: cada variante que la CPU tenga, forzada, contra los bucles en C. */
    static const struct { int m; const char *nome; bool (*tiene)(void); } vars[] = {
        { MEM_SSE, "sse", cpu_tem_sse }, { MEM_SSE2, "sse2", cpu_tem_sse2 },
        { MEM_SSE3, "sse3", cpu_tem_sse3 }, { MEM_SSSE3, "ssse3", cpu_tem_ssse3 } };
    rodar(MEM_REP);
    char lista[64] = "rep";
    for (unsigned k = 0; k < sizeof vars / sizeof vars[0]; k++)
        if (vars[k].tiene()) {
            rodar(vars[k].m);
            junta_texto(lista, ", ", sizeof lista);
            junta_texto(lista, vars[k].nome, sizeof lista);
        }
    rodar(MEM_AUTO);
    if (tem) testar_converter16();
    /* Forçar uma variante que a CPU não tem não pode ter efeito. */
    for (unsigned k = 0; k < sizeof vars / sizeof vars[0]; k++) {
        if (vars[k].tiene()) continue;
        mem_metodo(vars[k].m);
        bool recusou = mem_metodo(MEM_AUTO) != vars[k].m;
        if (!recusou) { imprimir("sse2: FALLO mem_metodo aceptó %s sin tenerlo\n", vars[k].nome); falhas++; }
    }
    char t[200];
    mem_escolhas(t, sizeof t);
    registrar("sse2: escolhas %s", t);
    if (falhas) {
        imprimir("sse2: FALLO, %d de %d comparaciones distintas\n", falhas, comparacoes);
        return 1;
    }
    imprimir("sse2: ok (%d comparaciones, metodos %s y auto)\n", comparacoes, lista);
    return 0;
}
