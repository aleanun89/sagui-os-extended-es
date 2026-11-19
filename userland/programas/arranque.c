// SPDX-License-Identifier: GPL-3.0-only
/* arranque - probar un kernel arrancándolo UNA sola vez, y cambiar la etapa 2 del disco (D392).
 *
 *   arranque [estado]                       qué hay en el disco de arranque (kernel, ramdisk, etapa 2,
 *                                           ranura de prueba) y con qué arrancó esta vez
 *   arranque probar KERNEL [RAMDISK]        copia el kernel (y el ramdisk) a la ranura de prueba y
 *                                           marca «arrancarlo una vez» (aunque haya un último bueno)
 *   arranque confirmar [--forzar]           el kernel de prueba (ya arrancado) pasa a ser el último bueno
 *   arranque cancelar                       quita el de prueba: vuelve el último bueno (o el de la imagen)
 *   arranque original                       quita el de prueba y el último bueno: vuelve el de la imagen
 *   arranque etapa2 ESTAGIO2.BIN            pone otra etapa 2 en el disco
 *   [--disco NOMBRE]                        si el kernel ya no sabe cuál es el disco de arranque
 *
 * Cómo funciona (boot/cabecalho.inc, CAB_PR_*): la segunda mitad de la cabecera de arranque (LBA 1,
 * bytes 256-511) describe la ranura: dónde está el kernel de prueba (hasta 4 tramos), su tamaño, su
 * suma y la dirección del ramdisk detrás de su BSS, y opcionalmente un ramdisk propio. La etapa 2,
 * si ve la marca PR_PENDENTE, la BORRA en el disco antes de leer nada de la prueba y la arranca; si
 * la prueba se cuelga, el siguiente reinicio ya es el normal (como grub-reboot). Si los datos de la
 * prueba no cuadran, arranca el normal en ese mismo arranque.
 *
 * Tres kernels: el de la imagen (CAB_K_*, «original», que esto ya no cambia nunca), el ÚLTIMO BUENO
 * (segunda ranura, bytes 392-511, la misma disposición con la nota de 16 bytes: lo deja «confirmar») y
 * el EN PRUEBA (bytes 256-391). La etapa 2 arranca la prueba pendiente una vez; si no, el último bueno;
 * si no, el de la imagen. Una prueba que se cuelga o no suma vuelve al último bueno. Los datos de las
 * dos ranuras nunca se pisan: en la partición, la prueba va fuera de lo que usa el último bueno; en la
 * ESP, cada ranura en su fichero (CAB_PR_ONDE 1 o 2). El primer «confirmar» (sin último bueno) dejaba
 * la ranura de prueba con PR_FIXA: se sigue arrancando así y se pasa a la del último bueno al tocarla.
 *
 * Seguridad (lo mismo que el instalador, D179/D213): todo por SIS_DISCO_BLOCOS, con la reserva del
 * disco de arranque; solo se escribe la cabecera (LBA 1) y dentro de la partición de SaguiOS (0xDA),
 * en sitio libre (nunca encima de la etapa 2, el kernel o el ramdisk que arrancan ahora). Si la
 * partición no tiene sitio para el kernel, va como fichero en la ESP (FAT32, tipo 0xEF) del mismo
 * disco, a través del sistema de archivos, y la etapa 2 lee esos sectores. Cada escritura se relee y
 * se compara; la cabecera se cambia la última, en una sola escritura de un sector. */
#include "sagui.h"
#include "particoes.h"
#include "sha256.h"

#define CAB_ASSIN    0x44484753u   /* 'SGHD' */
#define PR_ASSIN     0x52504753u   /* 'SGPR' */
#define E2_MARCA     0x32454753u   /* 'SGE2' en el byte 2 de la etapa 2 (D392) */
#define CAB_E2_LBA   8
#define CAB_K_LBA    16
#define CAB_K_BYTES  24
#define CAB_K_ENDER  28
#define CAB_R_LBA    32
#define CAB_R_BYTES  40
#define CAB_R_ENDER  44
#define CAB_IMG_SET  48
#define CAB_K_ENTR   52
#define CAB_K_SOMA   56
#define CAB_R_SOMA   60
#define CAB_OPCOES   6
#define OPC_CD       8
#define PR           256         /* la ranura de prueba (CAB_PR_*), 136 bytes */
#define PB           392         /* la del último bueno (CAB_PB), 120 bytes: igual hasta la nota */
#define PR_TAM       (PB - PR)
#define PB_TAM       (512 - PB)
/* desplazamientos dentro de una ranura */
#define R_ESTADO     4
#define R_NK         5
#define R_NR         6
#define R_ONDE       7           /* 0 partición, 1 fichero A de la ESP, 2 fichero B */
#define R_K_BYTES    8
#define R_K_SOMA     12
#define R_K_ENTR     16
#define R_R_ENDER    20
#define R_R_BYTES    24
#define R_R_SOMA     28
#define R_TRAMOS     32
#define R_SOMA       96
#define R_HORA       100
#define R_NOTA       104         /* 32 bytes en la de prueba, 16 en la del último bueno */
#define PR_PENDENTE  1
#define PR_ARRANCADA 2
#define PR_FIXA      4
#define OPC_BOOT_KERNEL_PRUEBA 0x100000u   /* kernel/inc/bootinfo.h */
#define OPC_BOOT_KERNEL_FIXO   0x200000u
#define KERNEL_END   0x100000u
#define E2_MAX_SET   64          /* ESTAGIO2_MAX_SET: el MBR la lee a 0x8000-0xFFFF */
#define E2_LBA_MAX   16000u      /* el MBR por CHS (CD) divide con 8 bits: la etapa 2, en los primeros 8 MB */
#define MAX_TRAMOS   4
#define MAX_USADOS   24
#define E2_SABE_BOM  2           /* bit de la marca SGE2: la etapa 2 sabe del último bueno */
#define ESP_PONTO    "/.arranque-esp"   /* no dentro de /tmp: no se monta dentro de otro montaje */
/* dos juegos de ficheros en la ESP (R_ONDE 1 y 2): la prueba va al que no usa el último bueno */
static const char *const ESP_KERNEL[3] = { "", "sagui-prueba-kernel.bin", "sagui-prueba2-kernel.bin" };
static const char *const ESP_RAMDISK[3] = { "", "sagui-prueba-ramdisk.bin", "sagui-prueba2-ramdisk.bin" };

typedef struct { uint32_t lba, n; } tramo_t;

static struct {
    char     disco[16];
    char     part[16];            /* la partición de SaguiOS ("hda1") */
    uint32_t p_ini, p_fim;        /* [p_ini, p_fim) absolutos */
    uint8_t  cab[512];
    info_arranque_t arr;
    bool     arr_ok;
    bool     reservado;
} D;

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static int igual_sem_caixa(const char *a, const char *b)
{
    for (;; a++, b++) {
        char x = *a >= 'a' && *a <= 'z' ? (char)(*a - 32) : *a, y = *b >= 'a' && *b <= 'z' ? (char)(*b - 32) : *b;
        if (x != y) return 1;
        if (!x) return 0;
    }
}
static void po32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint32_t setores(uint32_t bytes) { return (bytes + 511) / 512; }

static uint32_t soma(const uint8_t *p, uint32_t n)
{
    uint32_t s = 0;
    for (uint32_t i = 0; i < n; i++) s += p[i];
    return s;
}

static uint32_t soma_ranura(const uint8_t *r) { return soma(r + R_NK, R_SOMA - R_NK); }

static int erro(const char *fmt, ...)
{
    char t[400];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(t, sizeof t, fmt, ap);
    va_end(ap);
    imprimir_em(ERROS, "arranque: %s\n", t);
    registrar("arranque: ERROR %s", t);
    return 1;
}

/* ------------------------------------------------------------------ el disco de arranque */

static bool disco_por_flags(char *nome, int max)
{
    info_disco_t d;
    char boot[16] = "";
    int nboot = 0;
    for (int i = 0; listar_discos(i, &d) == 1; i++) {
        if (d.particao) continue;
        if (d.flags & DISCO_ARRANQUE_REAL) {
            info_bloco_t b;
            if (blocos_info(d.nome, &b) == 0 && b.arranque_real) { copia_texto(nome, d.nome, (size_t)max); return true; }
        }
        if (d.flags & DISCO_BOOT) { copia_texto(boot, d.nome, sizeof boot); nboot++; }
    }
    /* Tras cambiar la cabecera (etapa2, confirmar) el kernel ya no la reconoce hasta reiniciar:
     * si solo hay un disco que pudo serlo, ese. */
    if (nboot == 1) { copia_texto(nome, boot, (size_t)max); return true; }
    return false;
}

/* Lee la cabecera y la tabla, y localiza la partición de SaguiOS. */
static bool abrir_disco(const char *pedido)
{
    memset(&D, 0, sizeof D);
    D.arr_ok = info_sistema(INFO_ARRANQUE, &D.arr, sizeof D.arr) == 0;
    if (pedido) copia_texto(D.disco, pedido, sizeof D.disco);
    else if (!disco_por_flags(D.disco, sizeof D.disco)) {
        erro("no se sabe cuál es el disco de arranque (di cuál con --disco NOMBRE; «discos» los lista)");
        return false;
    }
    info_bloco_t b;
    if (blocos_info(D.disco, &b) < 0 || b.inicio) { erro("%s no es un disco entero", D.disco); return false; }
    if (b.gpt) { erro("%s tiene GPT: el kernel de prueba solo existe para el arranque por BIOS con MBR", D.disco); return false; }
    if (b.bloco_fisico != 512) { erro("%s tiene bloques de %u bytes: no arranca por BIOS", D.disco, b.bloco_fisico); return false; }
    uint8_t mbr[512];
    if (blocos_ler(D.disco, 0, 1, mbr) < 0 || blocos_ler(D.disco, 1, 1, D.cab) < 0) { erro("no se puede leer %s", D.disco); return false; }
    if (le32(D.cab) != CAB_ASSIN) { erro("%s no tiene la cabecera de arranque de SaguiOS en el LBA 1", D.disco); return false; }
    if (mbr[510] != 0x55 || mbr[511] != 0xAA) { erro("%s no tiene tabla de particiones MBR", D.disco); return false; }
    uint32_t e2 = le32(D.cab + CAB_E2_LBA);
    for (int i = 0; i < 4; i++) {
        const uint8_t *e = mbr + 446 + 16 * i;
        uint32_t ini = le32(e + 8), n = le32(e + 12);
        if (e[4] == 0xDA && n && e2 >= ini && e2 - ini < n) {
            D.p_ini = ini; D.p_fim = ini + n;
            pt_nome_particao(D.disco, i + 1, D.part, sizeof D.part);
        }
    }
    if (!D.p_fim) { erro("en %s no hay una partición de SaguiOS (0xDA) con la etapa 2", D.disco); return false; }
    /* las tres partes que arrancan ahora, dentro de la partición */
    static const int partes[3] = { CAB_E2_LBA, CAB_K_LBA, CAB_R_LBA };
    for (int k = 0; k < 3; k++) {
        uint32_t ini = le32(D.cab + partes[k]), n = le32(D.cab + partes[k] + 4);
        if (!n || ini < D.p_ini || ini >= D.p_fim || n > D.p_fim - ini) {
            erro("la cabecera de %s es incoherente (una parte fuera de %s)", D.disco, D.part);
            return false;
        }
    }
    return true;
}

static bool ranura_valida(const uint8_t *r)
{
    return le32(r) == PR_ASSIN && le32(r + R_SOMA) == soma_ranura(r) && r[R_NK] >= 1 &&
           r[R_NK] <= MAX_TRAMOS && r[R_NR] <= MAX_TRAMOS;
}

/* El último bueno: el de su ranura o, del primer «confirmar», la de prueba con PR_FIXA. */
static const uint8_t *bom_de(const uint8_t *cab)
{
    if (ranura_valida(cab + PR) && (cab[PR + R_ESTADO] & PR_FIXA)) return cab + PR;
    if (ranura_valida(cab + PB) && (cab[PB + R_ESTADO] & PR_FIXA)) return cab + PB;
    return NULL;
}

/* El de prueba (sin fijar). */
static const uint8_t *prueba_de(const uint8_t *cab)
{
    return ranura_valida(cab + PR) && !(cab[PR + R_ESTADO] & PR_FIXA) ? cab + PR : NULL;
}

/* Pasa una ranura de prueba fijada (primer «confirmar») a la del último bueno. La suma no cambia:
 * la disposición es la misma hasta la nota, que se corta a 16. */
static void normalizar(uint8_t *cab)
{
    if (!(ranura_valida(cab + PR) && (cab[PR + R_ESTADO] & PR_FIXA))) return;
    memcpy(cab + PB, cab + PR, PB_TAM);
    cab[PB + R_ESTADO] = PR_FIXA;
    memset(cab + PR, 0, PR_TAM);
}

/* La reserva del disco: sin nada montado, la de siempre (cualquier kernel); con particiones montadas,
 * D213 + DBLOCO_CONF_SAGUI (D392: solo los kernels nuevos dejan escribir así en la partición de SaguiOS). */
static bool reservar(void)
{
    int r = blocos_reservar(D.disco, DBLOCO_CONF_ARRANQUE);
    if (r == ERRO_OCUPADO) r = blocos_reservar(D.disco, DBLOCO_CONF_ARRANQUE | DBLOCO_CONF_MONTADO | DBLOCO_CONF_SAGUI);
    if (r < 0) { erro("no se puede reservar %s: %s", D.disco, nome_erro(r)); return false; }
    D.reservado = true;
    return true;
}

static void soltar(void)
{
    if (D.reservado) blocos_liberar(D.disco);
    D.reservado = false;
}

/* Escribe y relee para comparar. Un kernel de antes rechaza (ERRO_OCUPADO) escribir en la partición
 * de SaguiOS con algo montado del disco: se dice qué hacer. */
static bool escrever_conferido(uint32_t lba, uint32_t n, const uint8_t *dados)
{
    uint8_t *volta = aloca(DBLOCO_MAX_SETORES * 512);
    if (!volta) { erro("sin memoria"); return false; }
    for (uint32_t f = 0; f < n;) {
        uint32_t q = n - f < DBLOCO_MAX_SETORES ? n - f : DBLOCO_MAX_SETORES;
        int r = blocos_escrever(D.disco, lba + f, q, dados + f * 512);
        if (r == ERRO_OCUPADO) {
            libera(volta);
            erro("el kernel que corre no deja escribir en %s con particiones de %s montadas (es de antes de D392). "
                 "Arranca con M pulsada y elige «2 todo a la RAM», desmonta lo que quede de %s (mount, umount) y repite",
                 D.part, D.disco, D.disco);
            return false;
        }
        if (r < 0 || blocos_ler(D.disco, lba + f, q, volta) < 0 || memcmp(volta, dados + f * 512, q * 512) != 0) {
            libera(volta);
            erro("no se ha podido escribir %s en los sectores %u-%u (%s)", D.disco, lba + f, lba + f + q - 1, r < 0 ? nome_erro(r) : "lo releído no es lo escrito");
            return false;
        }
        f += q;
    }
    libera(volta);
    return true;
}

/* La cabecera nueva: se vuelve a leer la del disco (otro programa, kconfig o pantalla, pudo cambiarla)
 * y solo se cambia si todo lo demás sigue igual que cuando se leyó. */
static bool gravar_cabecalho(const uint8_t *nova, const uint8_t *antes)
{
    uint8_t agora[512];
    if (blocos_ler(D.disco, 1, 1, agora) < 0 || memcmp(agora, antes, 512) != 0) {
        erro("la cabecera de %s ha cambiado mientras tanto: no se toca (repite la orden)", D.disco);
        return false;
    }
    return escrever_conferido(1, 1, nova);
}

/* ------------------------------------------------------------------ sitio libre en la partición */

static int tramos_ranura(tramo_t *u, int n, const uint8_t *r)
{
    if (!r || !ranura_valida(r) || r[R_ONDE] != 0) return n;
    for (int i = 0; i < r[R_NK] + r[R_NR] && n < MAX_USADOS; i++) {
        const uint8_t *t = r + R_TRAMOS + 8 * (i < r[R_NK] ? i : MAX_TRAMOS + i - r[R_NK]);
        u[n++] = (tramo_t){ le32(t), le32(t + 4) };
    }
    return n;
}

/* Lo ocupado de la partición según la cabecera «cab»: MBR y cabecera, etapa 2, kernel y ramdisk de la
 * imagen, el último bueno siempre y la prueba si com_pr. */
static int usados(tramo_t *u, const uint8_t *cab, bool com_pr, bool com_e2)
{
    int n = 0;
    u[n++] = (tramo_t){ 0, 2 };   /* MBR y cabecera */
    if (com_e2) u[n++] = (tramo_t){ le32(cab + CAB_E2_LBA), le32(cab + CAB_E2_LBA + 4) };
    u[n++] = (tramo_t){ le32(cab + CAB_K_LBA), le32(cab + CAB_K_LBA + 4) };
    u[n++] = (tramo_t){ le32(cab + CAB_R_LBA), le32(cab + CAB_R_LBA + 4) };
    n = tramos_ranura(u, n, bom_de(cab));
    if (com_pr) n = tramos_ranura(u, n, prueba_de(cab));
    return n;
}

static bool livre(uint32_t ini, uint32_t n, const tramo_t *u, int nu)
{
    if (ini < D.p_ini || ini >= D.p_fim || n > D.p_fim - ini) return false;
    for (int i = 0; i < nu; i++)
        if (u[i].n && ini < u[i].lba + u[i].n && u[i].lba < ini + n) return false;
    return true;
}

/* El primer hueco de n sectores (alineado a 8) en la partición, fuera de lo usado; 0 = no hay. */
static uint32_t achar_livre(uint32_t n, const tramo_t *u, int nu, uint32_t teto)
{
    uint32_t cand[MAX_USADOS + 1];
    int nc = 0;
    cand[nc++] = D.p_ini;
    for (int i = 0; i < nu; i++) cand[nc++] = u[i].lba + u[i].n;
    uint32_t melhor = 0;
    for (int i = 0; i < nc; i++) {
        uint32_t c = (cand[i] + 7) & ~7u;
        if (livre(c, n, u, nu) && c + n <= teto && (!melhor || c < melhor)) melhor = c;
    }
    return melhor;
}

static uint32_t maior_livre(const tramo_t *u, int nu)
{
    uint32_t mx = 0;
    for (uint32_t i = 0; i < (uint32_t)nu + 1; i++) {
        uint32_t c = i == 0 ? D.p_ini : u[i - 1].lba + u[i - 1].n;
        if (c < D.p_ini || c >= D.p_fim) continue;
        uint32_t fim = D.p_fim;
        for (int k = 0; k < nu; k++) if (u[k].lba >= c && u[k].lba < fim) fim = u[k].lba;
        bool dentro = false;
        for (int k = 0; k < nu; k++) if (c >= u[k].lba && c < u[k].lba + u[k].n) dentro = true;
        if (!dentro && fim - c > mx) mx = fim - c;
    }
    return mx;
}

/* ------------------------------------------------------------------ ficheros */

static uint8_t *ler_tudo(const char *caminho, uint32_t *n, uint32_t max)
{
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) { erro("no se puede abrir %s: %s", caminho, nome_erro(fd)); return NULL; }
    uint32_t cap = 1024 * 1024, k = 0;
    uint8_t *b = aloca(cap);
    for (;;) {
        if (!b) { fechar(fd); erro("sin memoria para %s", caminho); return NULL; }
        if (k == cap) {
            if (cap >= max) { fechar(fd); libera(b); erro("%s es demasiado grande (más de %u MB)", caminho, max >> 20); return NULL; }
            uint8_t *nb = aloca(cap * 2);
            if (nb) memcpy(nb, b, k);
            libera(b);
            b = nb;
            cap *= 2;
            continue;
        }
        int r = ler(fd, b + k, cap - k);
        if (r < 0) { fechar(fd); libera(b); erro("no se puede leer %s: %s", caminho, nome_erro(r)); return NULL; }
        if (!r) break;
        k += (uint32_t)r;
    }
    fechar(fd);
    *n = k;
    return b;
}

/* ¿Es un kernel.bin de SaguiOS? El binario plano empieza siempre por el código de kernel/arch/entrada.asm
 * (enlazado el primero en 1 MB, ligacao.ld): cli; mov esi,eax; mov edx,ebx; cld; mov edi,_bss_inicio;
 * mov ecx,_bss_fim. De ahí salen el fin de la BSS (dónde va el ramdisk, como mkimagem.py con _kernel_fim)
 * y la comprobación de que el fichero es el binario entero (la BSS empieza justo detrás). */
static bool validar_kernel(const uint8_t *k, uint32_t n, uint32_t *r_ender, char *por, int max)
{
    static const uint8_t ini[7] = { 0xFA, 0x89, 0xC6, 0x89, 0xDA, 0xFC, 0xBF };
    if (n < 64 || memcmp(k, ini, 7) != 0 || k[11] != 0xB9) {
        formatar_texto(por, (size_t)max, "no empieza como un kernel.bin de SaguiOS (kernel/arch/entrada.asm)");
        return false;
    }
    uint32_t bss = le32(k + 7), fim = le32(k + 12);
    if (bss < KERNEL_END + n || bss - (KERNEL_END + n) > 65536 || fim < bss || fim - bss > 512u * 1024 * 1024) {
        formatar_texto(por, (size_t)max, "su BSS (0x%x-0x%x) no cuadra con el tamaño del fichero (%u bytes): ¿cortado?", bss, fim, n);
        return false;
    }
    *r_ender = (fim + 0xFFF) & ~0xFFFu;
    return true;
}

static bool validar_ramdisk(const uint8_t *r, uint32_t n, char *por, int max)
{
    if (n < 32 || memcmp(r, "SGFS", 4) != 0 || le32(r + 20) != n) {
        formatar_texto(por, (size_t)max, "no es un ramdisk SGFS entero (mksgfs.py)");
        return false;
    }
    return true;
}

static void sha256_texto(const uint8_t *p, uint32_t n, char *s)
{
    sha256_t c;
    unsigned char h[SHA256_TAM];
    sha256_iniciar(&c);
    sha256_somar(&c, p, n);
    sha256_fim(&c, h);
    for (int i = 0; i < SHA256_TAM; i++) formatar_texto(s + 2 * i, 3, "%02x", h[i]);
}

/* Lee del disco tramos seguidos hasta "bytes". */
static uint8_t *ler_tramos(const uint8_t *t, int nt, uint32_t bytes)
{
    uint8_t *b = aloca(setores(bytes) * 512 + 512);
    if (!b) return NULL;
    uint32_t k = 0;
    for (int i = 0; i < nt && k < setores(bytes); i++) {
        uint32_t lba = le32(t + 8 * i), n = le32(t + 8 * i + 4);
        if (n > setores(bytes) - k) n = setores(bytes) - k;
        if (blocos_ler(D.disco, lba, n, b + k * 512) < 0) { libera(b); return NULL; }
        k += n;
    }
    if (k < setores(bytes)) { libera(b); return NULL; }
    return b;
}

/* ------------------------------------------------------------------ la ESP (FAT32) */

typedef struct { uint32_t ini, spc, rsv, fat_lba, data, raiz; } fat_t;

static bool fat_abrir(uint32_t p_ini, fat_t *f)
{
    uint8_t s[512];
    if (blocos_ler(D.disco, p_ini, 1, s) < 0 || s[510] != 0x55 || s[511] != 0xAA || le16(s + 11) != 512 || !s[13] || !s[16]) return false;
    f->ini = p_ini;
    f->spc = s[13];
    f->rsv = le16(s + 14);
    f->fat_lba = p_ini + f->rsv;
    f->data = f->fat_lba + s[16] * le32(s + 36);
    f->raiz = le32(s + 44);
    return true;
}

static uint32_t fat_seg(const fat_t *f, uint32_t c)
{
    static uint8_t s[512];
    static uint32_t lido = 0;
    uint32_t lba = f->fat_lba + c / 128;
    if (lba != lido) { if (blocos_ler(D.disco, lba, 1, s) < 0) return 0x0FFFFFFF; lido = lba; }
    return le32(s + (c % 128) * 4) & 0x0FFFFFFF;
}

/* Busca en la carpeta raíz el fichero de nombre largo «nome» (o corto): primer cluster y tamaño. */
static bool fat_achar(const fat_t *f, const char *nome, uint32_t *cl, uint32_t *tam)
{
    char longo[256];
    bool tem_longo = false;
    uint8_t *s = aloca(f->spc * 512);
    if (!s) return false;
    for (uint32_t c = f->raiz, voltas = 0; c >= 2 && c < 0x0FFFFFF8 && voltas < 65536; c = fat_seg(f, c), voltas++) {
        if (blocos_ler(D.disco, f->data + (c - 2) * f->spc, f->spc, s) < 0) break;
        for (uint32_t o = 0; o < f->spc * 512; o += 32) {
            const uint8_t *e = s + o;
            if (!e[0]) { libera(s); return false; }
            if (e[0] == 0xE5) { tem_longo = false; continue; }
            if (e[11] == 0x0F) {
                int seq = e[0] & 0x1F;
                if (e[0] & 0x40) { memset(longo, 0, sizeof longo); tem_longo = true; }
                static const int pos[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };
                for (int k = 0; k < 13 && seq >= 1 && (seq - 1) * 13 + k < 255; k++) {
                    uint16_t u = le16(e + pos[k]);
                    longo[(seq - 1) * 13 + k] = u == 0xFFFF ? 0 : (char)(u < 128 ? u : '?');
                }
                continue;
            }
            char curto[13];
            int k = 0;
            for (int i = 0; i < 8 && e[i] != ' '; i++) curto[k++] = (char)e[i];
            if (e[8] != ' ') { curto[k++] = '.'; for (int i = 8; i < 11 && e[i] != ' '; i++) curto[k++] = (char)e[i]; }
            curto[k] = 0;
            bool igual = tem_longo ? igual_sem_caixa(longo, nome) == 0 : igual_sem_caixa(curto, nome) == 0;
            tem_longo = false;
            if (igual && !(e[11] & 0x18)) {
                *cl = (uint32_t)le16(e + 20) << 16 | le16(e + 26);
                *tam = le32(e + 28);
                libera(s);
                return true;
            }
        }
    }
    libera(s);
    return false;
}

/* Los tramos (sectores absolutos del disco) que ocupa un fichero de la ESP: como mucho MAX_TRAMOS. */
static int fat_tramos(const fat_t *f, const char *nome, uint32_t bytes, uint8_t *t, char *por, int max)
{
    uint32_t c, tam;
    if (!fat_achar(f, nome, &c, &tam) || tam != bytes) { formatar_texto(por, (size_t)max, "no se encuentra %s en la ESP con %u bytes", nome, bytes); return -1; }
    uint32_t falta = setores(bytes), nt = 0, voltas = 0;
    while (falta) {
        if (c < 2 || c >= 0x0FFFFFF8 || ++voltas > 1u << 22) { formatar_texto(por, (size_t)max, "la cadena de clusters de %s en la ESP está rota", nome); return -1; }
        uint32_t lba = f->data + (c - 2) * f->spc, n = f->spc < falta ? f->spc : falta;
        if (nt && le32(t + 8 * (nt - 1)) + le32(t + 8 * (nt - 1) + 4) == lba) po32(t + 8 * (nt - 1) + 4, le32(t + 8 * (nt - 1) + 4) + n);
        else {
            if (nt == MAX_TRAMOS) { formatar_texto(por, (size_t)max, "%s está en más de %d trozos en la ESP (borra ficheros de ella o desfragméntala)", nome, MAX_TRAMOS); return -1; }
            po32(t + 8 * nt, lba);
            po32(t + 8 * nt + 4, n);
            nt++;
        }
        falta -= n;
        if (falta) c = fat_seg(f, c);
    }
    return (int)nt;
}

static bool gravar_arquivo(const char *caminho, const uint8_t *d, uint32_t n)
{
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { erro("no se puede crear %s: %s", caminho, nome_erro(fd)); return false; }
    for (uint32_t k = 0; k < n;) {
        int r = escrever(fd, d + k, n - k > 65536 ? 65536 : n - k);
        if (r <= 0) { fechar(fd); erro("no se puede escribir %s: %s", caminho, nome_erro(r)); return false; }
        k += (uint32_t)r;
    }
    sincronizar(fd);
    fechar(fd);
    return true;
}

/* La ESP del disco de arranque: su nombre ("hda2") y su primer sector. */
static bool achar_esp(char *nome, int max, uint32_t *ini)
{
    info_disco_t d;
    for (int i = 0; listar_discos(i, &d) == 1; i++) {
        info_bloco_t b;
        if (!d.particao || compara_texto(d.pai, D.disco) != 0 || blocos_info(d.nome, &b) < 0 || b.tipo_mbr != 0xEF) continue;
        copia_texto(nome, d.nome, (size_t)max);
        if (d.montado) { erro("la ESP %s está montada en %s: desmóntala antes (umount %s)", d.nome, d.ponto, d.ponto); return false; }
        *ini = b.inicio;
        return true;
    }
    return false;
}

/* Copia el kernel (y el ramdisk) como ficheros de la ESP (el juego «onde», 1 o 2: nunca el del último
 * bueno) y devuelve sus tramos en t (kernel en t, ramdisk en t + 32). Se escribe por el sistema de
 * archivos y se desmonta antes de reservar el disco. */
static bool para_esp(int onde, const uint8_t *k, uint32_t nk, const uint8_t *r, uint32_t nr, uint8_t *t, int *tk, int *tr)
{
    char esp[16] = "", por[160];
    uint32_t ini = 0;
    if (!achar_esp(esp, sizeof esp, &ini)) {
        tramo_t u[MAX_USADOS];
        int nu = usados(u, D.cab, false, true);
        if (!esp[0]) erro("%s no tiene sitio para el kernel de prueba (%u sectores libres seguidos; hacen falta %u) ni una ESP "
                          "(FAT32) donde ponerlo", D.part, maior_livre(u, nu), setores(nk) + setores(nr));
        return false;
    }
    criar_pasta(ESP_PONTO);
    int m = montar_em(esp, ESP_PONTO, MONTAR_LEITURA_ESCRITA);
    if (m < 0) { erro("no se puede montar la ESP %s: %s", esp, nome_erro(m)); return false; }
    char c1[64], c2[64];
    formatar_texto(c1, sizeof c1, "%s/%s", ESP_PONTO, ESP_KERNEL[onde]);
    formatar_texto(c2, sizeof c2, "%s/%s", ESP_PONTO, ESP_RAMDISK[onde]);
    apagar(c1);
    apagar(c2);
    bool ok = gravar_arquivo(c1, k, nk) && (!r || gravar_arquivo(c2, r, nr));
    int dm = desmontar(ESP_PONTO);
    if (dm >= 0) apagar(ESP_PONTO);
    if (!ok) return false;
    if (dm < 0) { erro("no se puede desmontar la ESP %s: %s", esp, nome_erro(dm)); return false; }
    fat_t f;
    if (!fat_abrir(ini, &f)) { erro("la ESP %s no es un FAT32 que se entienda", esp); return false; }
    *tk = fat_tramos(&f, ESP_KERNEL[onde], nk, t, por, sizeof por);
    *tr = r ? fat_tramos(&f, ESP_RAMDISK[onde], nr, t + 32, por, sizeof por) : 0;
    if (*tk < 0 || *tr < 0) { erro("%s", por); return false; }
    imprimir("Sin sitio en %s: el kernel de prueba va como fichero en la ESP %s (%s, %d trozo%s)\n",
             D.part, esp, ESP_KERNEL[onde], *tk, *tk == 1 ? "" : "s");
    return true;
}

/* ------------------------------------------------------------------ órdenes */

static int estagio2_versao(uint32_t lba)
{
    uint8_t s[512];
    if (blocos_ler(D.disco, lba, 1, s) < 0) return -1;
    return le32(s + 2) == E2_MARCA ? le16(s + 6) : 0;
}

/* Comprueba que el kernel (y el ramdisk) de una ranura siguen sumando lo que dicen; *sha, si no es NULL,
 * recibe el sha256 del kernel. */
static bool ranura_suma_bien(const uint8_t *r, char *sha)
{
    uint8_t *v = ler_tramos(r + R_TRAMOS, r[R_NK], le32(r + R_K_BYTES));
    bool ok = v && soma(v, le32(r + R_K_BYTES)) == le32(r + R_K_SOMA);
    if (v && sha) sha256_texto(v, le32(r + R_K_BYTES), sha);
    if (v) libera(v);
    if (ok && r[R_NR]) {
        v = ler_tramos(r + R_TRAMOS + 32, r[R_NR], le32(r + R_R_BYTES));
        ok = v && soma(v, le32(r + R_R_BYTES)) == le32(r + R_R_SOMA);
        if (v) libera(v);
    }
    return ok;
}

static void mostrar_parte(const char *nome, uint32_t lba, uint32_t set, uint32_t bytes, uint32_t sm)
{
    char h[2 * SHA256_TAM + 1] = "(no se puede leer)";
    uint8_t t[8];
    po32(t, lba);
    po32(t + 4, set);
    uint8_t *b = ler_tramos(t, 1, bytes);
    const char *ok = "";
    if (b) { sha256_texto(b, bytes, h); ok = soma(b, bytes) == sm ? "suma bien" : "SUMA MAL"; libera(b); }
    imprimir("  %-8s LBA %u, %u sectores, %u bytes, suma 0x%08x (%s)\n           sha256 %s\n", nome, lba, set, bytes, sm, ok, h);
}

static void mostrar_ranura(const uint8_t *r, int nota_max)
{
    char nota[33];
    memcpy(nota, r + R_NOTA, (size_t)nota_max);
    nota[nota_max] = 0;
    char h[2 * SHA256_TAM + 1] = "(no se puede leer)";
    bool ok = ranura_suma_bien(r, h);
    imprimir("  %s, %u bytes, suma 0x%08x (%s), ramdisk %s, en %s%s\n", nota[0] ? nota : "(sin nombre)", le32(r + R_K_BYTES),
             le32(r + R_K_SOMA), ok ? "suma bien" : "SUMA MAL", r[R_NR] ? "propio" : "el normal",
             r[R_ONDE] ? "la ESP, fichero " : "", r[R_ONDE] ? ESP_KERNEL[r[R_ONDE] == 2 ? 2 : 1] : D.part);
    for (int i = 0; i < r[R_NK]; i++)
        imprimir("  tramo %d: LBA %u, %u sectores\n", i + 1, le32(r + R_TRAMOS + 8 * i), le32(r + R_TRAMOS + 8 * i + 4));
    imprimir("  sha256 %s\n", h);
}

static int cmd_estado(void)
{
    imprimir("Disco de arranque: %s, partición de SaguiOS %s (sectores %u-%u, %u KB)\n", D.disco, D.part, D.p_ini, D.p_fim - 1,
             (D.p_fim - D.p_ini) / 2);
    uint32_t e2 = le32(D.cab + CAB_E2_LBA), e2n = le32(D.cab + CAB_E2_LBA + 4);
    int v = estagio2_versao(e2);
    imprimir("Etapa 2:  LBA %u, %u sectores; %s\n", e2, e2n,
             v < 0 ? "no se puede leer" :
             (v & E2_SABE_BOM) ? "sabe arrancar el kernel de prueba y el último bueno (D392)" :
             v ? "ANTIGUA: sabe del kernel de prueba pero no del último bueno (arranque etapa2)" :
                 "ANTIGUA: no sabe del kernel de prueba (arranque etapa2)");
    imprimir("Kernel original (el de la imagen):\n");
    mostrar_parte("Kernel:", le32(D.cab + CAB_K_LBA), le32(D.cab + CAB_K_LBA + 4), le32(D.cab + CAB_K_BYTES), le32(D.cab + CAB_K_SOMA));
    mostrar_parte("Ramdisk:", le32(D.cab + CAB_R_LBA), le32(D.cab + CAB_R_LBA + 4), le32(D.cab + CAB_R_BYTES), le32(D.cab + CAB_R_SOMA));
    const uint8_t *bom = bom_de(D.cab), *pr = prueba_de(D.cab);
    if (bom) {
        imprimir("Último bueno: confirmado, se arranca siempre (arranque original vuelve al de la imagen)%s\n",
                 bom == D.cab + PR ? " [formato de antes]" : "");
        mostrar_ranura(bom, bom == D.cab + PR ? 32 : 16);
    } else
        imprimir("Último bueno: ninguno (se arranca el original)%s\n",
                 le32(D.cab + PB) == PR_ASSIN ? "; hay uno descrito pero NO VÁLIDO (la etapa 2 lo ignora)" : "");
    if (pr) {
        uint8_t e = pr[R_ESTADO];
        imprimir("En prueba: %s\n", (e & PR_PENDENTE) ? "PENDIENTE: se arranca UNA vez en el siguiente reinicio" :
                 (e & PR_ARRANCADA) ? "ya se arrancó una vez; sin confirmar (arranque confirmar lo deja como último bueno)" :
                 "preparado sin marca (arranque cancelar)");
        mostrar_ranura(pr, 32);
    } else
        imprimir("En prueba: ninguno%s\n", le32(D.cab + PR) == PR_ASSIN && !bom ? "; hay uno descrito pero NO VÁLIDO (la etapa 2 lo ignora)" : "");
    tramo_t u[MAX_USADOS];
    int nu = usados(u, D.cab, true, true);
    imprimir("Libre en %s: %u sectores seguidos como mucho\n", D.part, maior_livre(u, nu));
    if (D.arr_ok)
        imprimir("Este arranque: %s\n", !(D.arr.opcoes & OPC_BOOT_KERNEL_PRUEBA) ? "el kernel original" :
                 (D.arr.opcoes & OPC_BOOT_KERNEL_FIXO) ? "el último bueno" : "el KERNEL DE PRUEBA (una vez)");
    return 0;
}

static int cmd_probar(const char *fk, const char *fr)
{
    if (D.arr_ok && D.arr.firmware != 0) return erro("este arranque es por UEFI: el kernel de prueba solo existe por BIOS (la etapa 2)");
    if (le16(D.cab + CAB_OPCOES) & OPC_CD) return erro("el disco de arranque es un CD");
    int v = estagio2_versao(le32(D.cab + CAB_E2_LBA));
    if (v <= 0 || !(v & E2_SABE_BOM))
        return erro("la etapa 2 de %s es antigua y no sabe del kernel de prueba y el último bueno: primero «arranque etapa2 estagio2.bin»", D.disco);
    char por[200];
    uint32_t nk = 0, nr = 0, r_ender;
    uint8_t *k = ler_tudo(fk, &nk, 64u << 20), *r = NULL;
    if (!k) return 1;
    if (!validar_kernel(k, nk, &r_ender, por, sizeof por)) return erro("%s: %s", fk, por);
    if (fr) {
        if (!(r = ler_tudo(fr, &nr, 256u << 20))) return 1;
        if (!validar_ramdisk(r, nr, por, sizeof por)) return erro("%s: %s", fr, por);
    }
    uint8_t antes[512], nova[512];
    memcpy(antes, D.cab, 512);
    memcpy(nova, D.cab, 512);
    normalizar(nova);              /* un fijado del primer «confirmar» pasa a ser el último bueno */
    memset(nova + PR, 0, PR_TAM);
    const uint8_t *bom = bom_de(nova);
    uint8_t *p = nova + PR;
    /* Sitio: en la partición de SaguiOS, fuera de lo que arranca ahora y del último bueno (la prueba
     * vieja se reutiliza). */
    tramo_t u[MAX_USADOS];
    int nu = usados(u, nova, false, true);
    uint32_t sk = setores(nk), sr = setores(nr);
    uint32_t lk = achar_livre(sk + sr, u, nu, 0xFFFFFFFFu);
    int tk = 1, tr = r ? 1 : 0;
    if (lk) {
        po32(p + R_TRAMOS, lk);
        po32(p + R_TRAMOS + 4, sk);
        if (r) { po32(p + R_TRAMOS + 32, lk + sk); po32(p + R_TRAMOS + 36, sr); }
        p[R_ONDE] = 0;
    } else {
        int onde = bom && bom[R_ONDE] == 1 ? 2 : 1;
        if (!para_esp(onde, k, nk, r, nr, p + R_TRAMOS, &tk, &tr)) return 1;
        p[R_ONDE] = (uint8_t)onde;
    }
    po32(p, PR_ASSIN);
    p[R_NK] = (uint8_t)tk;
    p[R_NR] = (uint8_t)tr;
    po32(p + R_K_BYTES, nk);
    po32(p + R_K_SOMA, soma(k, nk));
    po32(p + R_K_ENTR, KERNEL_END);
    po32(p + R_R_ENDER, r_ender);
    po32(p + R_R_BYTES, r ? nr : 0);
    po32(p + R_R_SOMA, r ? soma(r, nr) : 0);
    po32(p + R_HORA, hora_utc(NULL));
    const char *base = fk;
    for (const char *q = fk; *q; q++) if (*q == '/') base = q + 1;
    copia_texto((char *)p + R_NOTA, base, 32);
    po32(p + R_SOMA, soma_ranura(p));
    p[R_ESTADO] = PR_PENDENTE;
    if (!reservar()) return 1;
    /* 1. Sin marca mientras se copia (si había una prueba vieja pendiente, deja de estarlo). */
    if (prueba_de(antes) && (antes[PR + R_ESTADO] & PR_PENDENTE)) {
        uint8_t sem[512];
        memcpy(sem, antes, 512);
        sem[PR + R_ESTADO] = 0;
        if (!gravar_cabecalho(sem, antes)) { soltar(); return 1; }
        memcpy(antes, sem, 512);
    }
    /* 2. Los datos, releídos y comparados. */
    if (!lk) {
        /* en la ESP ya están (por el sistema de archivos): se relee lo que leerá la etapa 2 */
        uint8_t *v2 = ler_tramos(p + R_TRAMOS, tk, nk);
        bool ok = v2 && memcmp(v2, k, nk) == 0;
        if (v2) libera(v2);
        if (ok && r) { v2 = ler_tramos(p + R_TRAMOS + 32, tr, nr); ok = v2 && memcmp(v2, r, nr) == 0; if (v2) libera(v2); }
        if (!ok) { soltar(); return erro("lo que hay en los sectores de la ESP no es lo que se escribió (¿caché sin escribir?): no se marca nada"); }
    } else {
        uint8_t *buf = aloca((sk + sr) * 512);
        if (!buf) { soltar(); return erro("sin memoria"); }
        memset(buf, 0, (sk + sr) * 512);
        memcpy(buf, k, nk);
        if (r) memcpy(buf + sk * 512, r, nr);
        bool ok = escrever_conferido(lk, sk + sr, buf);
        libera(buf);
        if (!ok) { soltar(); return 1; }
    }
    /* 3. La marca: una escritura de un sector (con el último bueno, si se pasó de ranura, a la vez). */
    if (!gravar_cabecalho(nova, antes)) { soltar(); return 1; }
    soltar();
    registrar("arranque: kernel de prueba %s (%u bytes, suma 0x%08x) preparado en %s, %s", fk, nk, soma(k, nk),
              lk ? D.part : "la ESP", r ? "con su ramdisk" : "con el ramdisk normal");
    imprimir("Kernel de prueba preparado: %s (%u bytes, suma 0x%08x)%s%s\n", fk, nk, soma(k, nk), r ? " y ramdisk " : "", r ? fr : "");
    imprimir("En el siguiente reinicio arranca UNA vez (la etapa 2 borra la marca antes de cargarlo).\n"
             "Si se cuelga, reinicia y vuelve %s. Si va bien: «arranque confirmar».\n",
             bom ? "el último bueno" : "el original (el de la imagen)");
    return 0;
}

static int cmd_confirmar(bool forzar)
{
    const uint8_t *pr = prueba_de(D.cab);
    if (!pr) return erro(bom_de(D.cab) ? "no hay kernel de prueba sin confirmar (el último bueno ya está confirmado)" : "no hay kernel de prueba que confirmar");
    int v = estagio2_versao(le32(D.cab + CAB_E2_LBA));
    if (v <= 0 || !(v & E2_SABE_BOM))
        return erro("la etapa 2 de %s no sabe del último bueno: primero «arranque etapa2 estagio2.bin»", D.disco);
    bool agora = D.arr_ok && (D.arr.opcoes & OPC_BOOT_KERNEL_PRUEBA) && !(D.arr.opcoes & OPC_BOOT_KERNEL_FIXO);
    if (!agora && !forzar)
        return erro("este arranque no es el kernel de prueba: arráncalo antes (o --forzar, si ya se probó: %s)",
                    (pr[R_ESTADO] & PR_ARRANCADA) ? "la etapa 2 lo arrancó al menos una vez" : "NUNCA se ha arrancado");
    /* Que lo de la ranura siga siendo lo que se probó. */
    if (!ranura_suma_bien(pr, NULL)) return erro("lo que hay en la ranura de prueba ya no suma lo que tiene que sumar: no se confirma");
    /* La prueba pasa a la ranura del último bueno (el de antes, si había, queda como sitio libre) en una
     * escritura de un sector. La cabecera de la imagen (bytes 0-255) no cambia. */
    uint8_t antes[512], nova[512];
    memcpy(antes, D.cab, 512);
    memcpy(nova, D.cab, 512);
    memcpy(nova + PB, nova + PR, PB_TAM);
    nova[PB + R_ESTADO] = PR_FIXA;
    memset(nova + PR, 0, PR_TAM);
    if (!reservar()) return 1;
    bool g = gravar_cabecalho(nova, antes);
    soltar();
    if (!g) return 1;
    registrar("arranque: kernel de prueba confirmado en %s como último bueno", D.disco);
    imprimir("Confirmado: el kernel de prueba es ahora el último bueno y se arranca siempre.\n"
             "«arranque probar» puede probar otro encima (si falla, vuelve este); «arranque original» vuelve al de la imagen.\n");
    return 0;
}

/* cancelar: quita la prueba (vuelve el último bueno); original: quita también el último bueno. */
static int cmd_quitar(bool original)
{
    uint8_t antes[512], nova[512];
    memcpy(antes, D.cab, 512);
    memcpy(nova, D.cab, 512);
    if (original) memset(nova + PR, 0, 256);
    else { normalizar(nova); memset(nova + PR, 0, PR_TAM); }
    if (memcmp(nova, antes, 512) == 0) {
        imprimir(original ? "Ya se arranca el kernel original (el de la imagen).\n" : "No hay kernel de prueba.\n");
        return 0;
    }
    if (!reservar()) return 1;
    bool g = gravar_cabecalho(nova, antes);
    soltar();
    if (!g) return 1;
    bool bom = !original && bom_de(nova);
    registrar("arranque: %s en %s", original ? "vuelta al kernel original" : "kernel de prueba quitado", D.disco);
    if (original) imprimir("Kernel de prueba y último bueno quitados: el siguiente arranque es el kernel original (el de la imagen).\n");
    else imprimir("Kernel de prueba quitado: el siguiente arranque es %s.\n", bom ? "el último bueno" : "el original (el de la imagen)");
    return 0;
}

/* Etapa 2 nueva: se escribe en sitio libre de la partición, se relee y solo entonces la cabecera pasa a
 * apuntar a ella (una escritura de un sector). Un corte antes deja la vieja; nunca se escribe encima de
 * la etapa 2 que arranca ahora. */
static int cmd_etapa2(const char *fe)
{
    uint32_t n = 0;
    uint8_t *e = ler_tudo(fe, &n, 1u << 20);
    if (!e) return 1;
    if (n < 16 || setores(n) > E2_MAX_SET) return erro("%s: una etapa 2 tiene que medir entre 16 bytes y 32 KB (mide %u)", fe, n);
    if (le32(e + 2) != E2_MARCA || e[0] != 0xEB) return erro("%s no es una etapa 2 de SaguiOS con la marca SGE2 (boot/estagio2.asm, D392)", fe);
    uint32_t se = setores(n);
    tramo_t u[MAX_USADOS];
    int nu = usados(u, D.cab, true, true);
    uint32_t l = achar_livre(se, u, nu, E2_LBA_MAX);
    if (!l) return erro("no hay %u sectores libres seguidos en %s (en sus primeros 8 MB) para la etapa 2 nueva", se, D.part);
    uint8_t *buf = aloca(se * 512);
    if (!buf) return erro("sin memoria");
    memset(buf, 0, se * 512);
    memcpy(buf, e, n);
    uint8_t antes[512], nova[512];
    memcpy(antes, D.cab, 512);
    memcpy(nova, D.cab, 512);
    po32(nova + CAB_E2_LBA, l);
    po32(nova + CAB_E2_LBA + 4, se);
    uint32_t fim = 0;
    static const int partes[3] = { CAB_E2_LBA, CAB_K_LBA, CAB_R_LBA };
    for (int i = 0; i < 3; i++) if (le32(nova + partes[i]) + le32(nova + partes[i] + 4) > fim) fim = le32(nova + partes[i]) + le32(nova + partes[i] + 4);
    if (fim > le32(nova + CAB_IMG_SET)) po32(nova + CAB_IMG_SET, fim);
    if (!reservar()) { libera(buf); return 1; }
    bool ok = escrever_conferido(l, se, buf) && gravar_cabecalho(nova, antes);
    soltar();
    libera(buf);
    if (!ok) return 1;
    registrar("arranque: etapa 2 nueva (%u bytes) en %s, LBA %u (antes LBA %u)", n, D.part, l, le32(antes + CAB_E2_LBA));
    imprimir("Etapa 2 nueva en %s: LBA %u, %u sectores (la de antes, en el LBA %u, queda como sitio libre).\n"
             "Reinicia pronto: hasta entonces el sistema no reconoce la cabecera nueva.\n", D.part, l, se, le32(antes + CAB_E2_LBA));
    return 0;
}

/* Opciones de arranque de la cabecera (CAB_OPCOES, las de mkimagem.py --smp/--ioapic/--uacpi): verlas y
 * cambiarlas sin regrabar la imagen. Solo esos tres bits; el resto de la cabecera, igual. Se aplican al
 * siguiente arranque, con cualquier kernel (original, último bueno o de prueba). */
static const struct { const char *nome; uint16_t bit; const char *que; } opc_tab[] = {
    { "smp",    0x0040, "todos los procesadores (multinúcleo, D164)" },
    { "ioapic", 0x0080, "interrupciones por el IOAPIC (D166)" },
    { "uacpi",  0x1000, "intérprete de AML: batería y ACPI (D192)" },
};
#define N_OPC ((int)(sizeof opc_tab / sizeof opc_tab[0]))

static int cmd_opciones(int na, const char **a)
{
    uint8_t antes[512], nova[512];
    memcpy(antes, D.cab, 512);
    memcpy(nova, D.cab, 512);
    uint16_t o = le16(nova + CAB_OPCOES), n = o;
    for (int i = 1; i < na; i++) {
        const char *x = a[i], *eq = x;
        while (*eq && *eq != '=') eq++;
        int k = -1;
        for (int j = 0; j < N_OPC; j++)
            if ((size_t)(eq - x) == tam_texto(opc_tab[j].nome) && memcmp(x, opc_tab[j].nome, (size_t)(eq - x)) == 0) k = j;
        if (k < 0 || !*eq) return erro("opción no válida: %s (smp=si|no, ioapic=si|no, uacpi=si|no)", x);
        if (compara_texto(eq + 1, "si") == 0) n |= opc_tab[k].bit;
        else if (compara_texto(eq + 1, "no") == 0) n &= (uint16_t)~opc_tab[k].bit;
        else return erro("opción no válida: %s (si o no)", x);
    }
    if (n != o) {
        nova[CAB_OPCOES] = (uint8_t)n;
        nova[CAB_OPCOES + 1] = (uint8_t)(n >> 8);
        if (!reservar()) return 1;
        bool g = gravar_cabecalho(nova, antes);
        soltar();
        if (!g) return 1;
        registrar("arranque: opciones de la cabecera de %s: %x -> %x", D.disco, o, n);
    }
    imprimir("Opciones de arranque de %s%s:\n", D.disco, n != o ? " (cambiadas: valen desde el siguiente arranque)" : "");
    for (int j = 0; j < N_OPC; j++)
        imprimir("  %-7s %-3s %s\n", opc_tab[j].nome, (n & opc_tab[j].bit) ? "si" : "no", opc_tab[j].que);
    return 0;
}

static void uso(void)
{
    imprimir("uso: arranque [estado]\n"
             "     arranque probar KERNEL.BIN [RAMDISK]   arrancarlo UNA vez en el siguiente reinicio\n"
             "     arranque confirmar [--forzar]          el de prueba (ya arrancado) pasa a ser el último bueno\n"
             "     arranque cancelar                      quitar el de prueba: vuelve el último bueno\n"
             "     arranque original                      quitar también el último bueno: vuelve el de la imagen\n"
             "     arranque etapa2 ESTAGIO2.BIN           poner otra etapa 2 en el disco\n"
             "     arranque opciones [smp=si|no] [ioapic=si|no] [uacpi=si|no]   ver o cambiar las opciones de la imagen\n"
             "     --disco NOMBRE                         el disco de arranque, si no se sabe solo\n");
}

int principal(int argc, char **argv)
{
    const char *disco = NULL, *a[4] = { 0 };
    int na = 0;
    bool forzar = false;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--disco") == 0 && i + 1 < argc) disco = argv[++i];
        else if (compara_texto(argv[i], "--forzar") == 0) forzar = true;
        else if (compara_texto(argv[i], "-h") == 0 || compara_texto(argv[i], "--help") == 0 || compara_texto(argv[i], "--ayuda") == 0) { uso(); return 0; }
        else if (na < 4) a[na++] = argv[i];
        else { uso(); return 2; }
    }
    const char *o = na ? a[0] : "estado";
    if (!abrir_disco(disco)) return 1;
    if (compara_texto(o, "estado") == 0 && na <= 1) return cmd_estado();
    if (compara_texto(o, "probar") == 0 && (na == 2 || na == 3)) return cmd_probar(a[1], na == 3 ? a[2] : NULL);
    if (compara_texto(o, "confirmar") == 0 && na == 1) return cmd_confirmar(forzar);
    if (compara_texto(o, "cancelar") == 0 && na == 1) return cmd_quitar(false);
    if (compara_texto(o, "original") == 0 && na == 1) return cmd_quitar(true);
    if (compara_texto(o, "opciones") == 0) return cmd_opciones(na, a);
    if ((compara_texto(o, "etapa2") == 0 || compara_texto(o, "estagio2") == 0) && na == 2) return cmd_etapa2(a[1]);
    uso();
    return 2;
}
