// SPDX-License-Identifier: GPL-2.0-or-later
/* cargador.c - cargador UEFI de SaguiOS: BOOTX64.EFI y BOOTIA32.EFI (D264, mejora 062).
 *
 * Hace con los servicios del firmware lo que la etapa 2 hace con la BIOS, y entrega
 * al kernel la misma boot_info (boot/bootinfo.inc), ampliada con lo que solo da UEFI:
 *
 *   [1/6] cabecera   la del disco de la ESP: su LBA 1 (imagen híbrida, disco instalado con
 *                    MBR) o, en una GPT, el primer sector de la partición de SaguiOS (disco
 *                    instalado con GPT, D266/D270); es la que el kernel compara para saber
 *                    de qué disco arrancó y la que "pantalla --guardar" reescribe. Sin ella,
 *                    \SAGUI\CABECERA.BIN de la ESP
 *   [2/6] kernel     \SAGUI\KERNEL.BIN de la ESP si es el mismo sistema que el del disco
 *                    (imagen híbrida); si no, del disco, en los sectores de la cabecera
 *   [3/6] ramdisk    \SAGUI\RAMDISK.SGF, idem; las sumas de la cabecera lo comprueban
 *   [4/6] tablas     RSDP de ACPI y SMBIOS de la tabla de configuración (sin BIOS no
 *                    están en 0xE0000-0xFFFFF); la MADT y la HPET para el trampolín
 *   [5/6] vídeo      GOP: el mejor modo con framebuffer lineal (la resolución de la
 *                    cabecera, la nativa del panel por el EDID, 1024x768, 800x600...)
 *   [6/6] salida     mapa de memoria convertido a E820, ExitBootServices y el
 *                    trampolín (trampolim.asm): modo protegido de 32 bits y el kernel
 *
 * El kernel está enlazado en 1 MB y no se puede mover. Se intenta reservar esa
 * memoria (la imagen y la BSS) al firmware (AllocateAddress) y leerlo allí; si el
 * firmware la tiene ocupada con datos de los servicios de arranque (que dejan de valer
 * tras ExitBootServices), se lee en otro sitio por debajo de 4 GB y el trampolín lo
 * mueve después, con la paginación ya apagada. Si en ese rango hay algo que sigue vivo
 * después (servicios de ejecución, ACPI, MMIO), se para con un mensaje. El ramdisk no
 * se mueve: va donde la etapa 2 lo pondría si está libre o donde el firmware dé
 * memoria, y el kernel lo encuentra por la boot_info.
 *
 * Código propio, escrito con la especificación UEFI pública (uefi.h); sin gnu-efi
 * ni EDK2. Se compila como ELF independiente de la posición y tools/mkefi.py lo
 * convierte en PE32+/PE32. */
#include "uefi.h"

#if defined(__x86_64__)
#define ARQUITETURA L"x64"
#define FIRMWARE_TIPO 1u
#else
#define ARQUITETURA L"IA32"
#define FIRMWARE_TIPO 2u
#endif

/* ------------------------------------------------------------ boot_info (boot/bootinfo.inc) */

#define BOOT_MAGICO   0x53414755u
#define BOOTINFO_SGBI 0x49424753u
#define MAPA_MAX      64
#define MODOS_MAX     128

typedef struct __attribute__((packed)) {
    UINT32 assinatura, versao, disco, mapa_qtd, mapa_end;
    UINT32 kernel_inicio, kernel_bytes, ramdisk_inicio, ramdisk_bytes, a20_metodo;
    UINT32 vesa_ok, vesa_modo, vesa_largura, vesa_altura, vesa_pitch, vesa_bpp, vesa_fb, vesa_ativo;
    UINT8  verm_tam, verm_pos, verde_tam, verde_pos, azul_tam, azul_pos, reservado[2];
    UINT32 setores_lidos;
    char   teste[64];
    UINT32 opcoes, modos_qtd, modos_end, res_pref, bpp_pref, leitura_chs;
    /* D264: versión 2 de la boot_info; la etapa 2 los deja a cero. */
    UINT32 firmware;   /* 0 BIOS, 1 UEFI x64, 2 UEFI IA32 */
    UINT32 cab_end;    /* dónde está la copia de la cabecera (0 = 0x7E00, la de la BIOS) */
    UINT32 rsdp;       /* RSDP de ACPI dado por el firmware (0 = buscarlo en la memoria de la BIOS) */
    UINT32 smbios;     /* punto de entrada "_SM_" o "_SM3_" (0 = buscarlo) */
    UINT32 reservado2;
} info_boot_t;

_Static_assert(sizeof(info_boot_t) == 192, "boot_info: BOOTINFO_TAM");
_Static_assert(__builtin_offsetof(info_boot_t, teste) == 84, "boot_info: BI_TESTE");
_Static_assert(__builtin_offsetof(info_boot_t, opcoes) == 148, "boot_info: BI_OPCOES");
_Static_assert(__builtin_offsetof(info_boot_t, firmware) == 172, "boot_info: BI_FIRMWARE");

typedef struct __attribute__((packed)) {
    UINT64 base, tamanho;
    UINT32 tipo, extra;
} regiao_e820_t;

typedef struct __attribute__((packed)) {
    UINT16 modo, largura, altura;
    UINT8  bpp, reservado;
} modo_vbe_t;

/* Cabecera de arranque (boot/cabecalho.inc). */
#define CAB_ASSINATURA_VAL 0x44484753u
#define CAB_OPCOES     6
#define CAB_K_LBA      16
#define CAB_K_BYTES    24
#define CAB_K_ENDERECO 28
#define CAB_R_LBA      32
#define CAB_R_BYTES    40
#define CAB_R_ENDERECO 44
#define CAB_K_ENTRADA  52
#define CAB_K_SOMA     56
#define CAB_R_SOMA     60
#define CAB_TESTE      64
#define CAB_RES_LARG   128
#define CAB_RES_ALT    130
#define CAB_BPP_PREF   132
#define CAB_MOTOR      134
#define OPC_SGL_SOFTWARE 32
#define OPC_RESCATE      0x4000
#define OPC_ACEL_SEGURA  0x10000   /* D323: Mayús al arrancar */

/* La zona de transferencia: 4 páginas propias (LoaderCode, por debajo de 4 GB) con todo
 * lo que el kernel lee al empezar y el trampolín con su pila. */
#define ZONA_PAGINAS   4
#define Z_BOOTINFO     0x0000
#define Z_MAPA         0x0100   /* 64 x 24 = 0x600 */
#define Z_MODOS        0x0700   /* 128 x 8 = 0x400 */
#define Z_CABECERA     0x0C00   /* 512 */
#define Z_PARAMETROS   0x0E00
#define Z_TRAMPOLIM    0x1000
#define Z_PILHA        0x4000   /* cima de la pila provisional */

/* El bloque de parámetros del trampolín (P_* de trampolim.asm). */
typedef struct __attribute__((packed)) {
    UINT32 destino, origem, bytes, entrada, bootinfo, pilha, opcoes;
    UINT32 ioapic[4];
    UINT32 hpet;
} parametros_t;

extern const UINT8 trampolim_ini[] __attribute__((visibility("hidden")));
extern const UINT8 trampolim_fim[] __attribute__((visibility("hidden")));

/* ------------------------------------------------------------ estado */

static EFI_SYSTEM_TABLE *ST;
static EFI_BOOT_SERVICES *BS;
static EFI_HANDLE imagem;

/* gcc puede generar llamadas a estas (copias de estructuras). */
void *memcpy(void *d, const void *o, UINTN n)
{
    UINT8 *a = d;
    const UINT8 *b = o;
    while (n--) *a++ = *b++;
    return d;
}

void *memset(void *d, int v, UINTN n)
{
    UINT8 *a = d;
    while (n--) *a++ = (UINT8)v;
    return d;
}

int memcmp(const void *x, const void *y, UINTN n)
{
    const UINT8 *a = x, *b = y;
    for (; n; n--, a++, b++)
        if (*a != *b) return *a - *b;
    return 0;
}

/* ¿Cabe en 32 bits? (en IA32 siempre: el puntero ya es de 32 bits) */
static int abaixo_4g(const void *p) { UINT64 v = (UINTN)p; return v <= 0xFFFFFFFFull - 4096; }

static int guid_igual(const EFI_GUID *a, const EFI_GUID *b) { return memcmp(a, b, sizeof *a) == 0; }
static UINT32 le32(const UINT8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (UINT32)p[3] << 24; }
static UINT16 le16(const UINT8 *p) { return (UINT16)(p[0] | p[1] << 8); }

/* ------------------------------------------------------------ texto */

static void escrever(const CHAR16 *s)
{
    if (ST && ST->ConOut) ST->ConOut->OutputString(ST->ConOut, (CHAR16 *)s);
}

static void escrever_num(UINT64 v)
{
    CHAR16 b[24];
    int i = 23;
    b[i] = 0;
    do { b[--i] = (CHAR16)(L'0' + v % 10); v /= 10; } while (v);
    escrever(&b[i]);
}

static void escrever_hex(UINT64 v)
{
    CHAR16 b[20];
    int i = 19;
    b[i] = 0;
    do { b[--i] = (CHAR16)L"0123456789ABCDEF"[v & 15]; v >>= 4; } while (v);
    b[--i] = L'x';
    b[--i] = L'0';
    escrever(&b[i]);
}

/* Para con un mensaje: no se sale al firmware, para que la pantalla quede a la vista
 * (como la etapa 2). La foto de esta pantalla es lo que hay que mandar. */
static void parar(const CHAR16 *msg)
{
    escrever(L"\r\nERROR: ");
    escrever(msg);
    escrever(L"\r\nSaguiOS no puede arrancar por UEFI en esta maquina. Haz una foto de esta pantalla.\r\n");
    for (;;) {
        if (BS) BS->Stall(1000000);
        __asm__ volatile("pause");
    }
}

/* ------------------------------------------------------------ CPU (D310) */

#if defined(__i386__)
/* El kernel y los programas son i686 (CMOV/FCMOV). Casi toda CPU con UEFI de 32 bits
 * lo es, pero no todas: el Intel Quark X1000 (Galileo) es de clase Pentium y no tiene
 * CMOV. Por eso este cargador IA32 se compila con -march=i586 (Makefile): así nada de
 * lo que corre antes de esta comprobación lleva CMOV. Devuelve la causa o 0 si vale. */
static const CHAR16 *cpu_nao_i686(void)
{
    UINT32 antes, depois, a, b, c, d;
    __asm__ volatile("pushfl; popl %0; movl %0, %1; xorl $0x200000, %1; pushl %1; popfl;"
                     "pushfl; popl %1; pushl %0; popfl"
                     : "=&r"(antes), "=&r"(depois));
    if (!((antes ^ depois) & 0x200000))
        return L"no tiene CPUID, y por tanto tampoco CMOV";
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0), "c"(0));
    if (a < 1)
        return L"no tiene CPUID, y por tanto tampoco CMOV";
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    if (!(d & (1u << 15)))
        return L"falta la instruccion CMOV";
    return 0;
}
#endif

/* ------------------------------------------------------------ archivos de la ESP */

static EFI_FILE_PROTOCOL *raiz;

static EFI_FILE_PROTOCOL *abrir(const CHAR16 *nome, UINT64 *tamanho)
{
    EFI_FILE_PROTOCOL *f = 0;
    if (EFI_ERROR(raiz->Open(raiz, &f, (CHAR16 *)nome, EFI_FILE_MODE_READ, 0)) || !f) return 0;
    EFI_GUID g = GUID_FILE_INFO;
    UINT8 info[512];
    UINTN t = sizeof info;
    if (EFI_ERROR(f->GetInfo(f, &g, &t, info))) { f->Close(f); return 0; }
    *tamanho = ((EFI_FILE_INFO *)info)->FileSize;
    return f;
}

/* Lee el archivo entero en dest (bytes exactos), por trozos de 1 MB con un punto
 * cada 4 MB. Devuelve 0 si fue bien. */
static int ler_arquivo(const CHAR16 *nome, void *dest, UINT64 bytes)
{
    UINT64 tam = 0;
    EFI_FILE_PROTOCOL *f = abrir(nome, &tam);
    if (!f) { escrever(L" no se encuentra "); escrever(nome); return -1; }
    if (tam != bytes) {
        escrever(L" tamano distinto del de la cabecera: "); escrever_num(tam);
        f->Close(f);
        return -1;
    }
    UINT8 *p = dest;
    UINT64 lido = 0;
    while (lido < bytes) {
        UINTN trozo = (UINTN)(bytes - lido > 0x100000 ? 0x100000 : bytes - lido);
        UINTN n = trozo;
        if (EFI_ERROR(f->Read(f, &n, p + lido)) || n != trozo) {
            escrever(L" fallo de lectura");
            f->Close(f);
            return -1;
        }
        lido += n;
        if ((lido & 0x3FFFFF) == 0 || lido == bytes) escrever(L".");
    }
    f->Close(f);
    return 0;
}

static UINT32 somar(const UINT8 *p, UINT32 n)
{
    UINT32 s = 0;
    while (n--) s += *p++;
    return s;
}

/* ------------------------------------------------------------ cabecera */

/* D270: el disco entero de la ESP (Block I/O y, si lo hay, Disk I/O). La ruta del dispositivo de
 * la ESP acaba en el nodo de la partición: sin él es la del disco entero, que es la que
 * LocateDevicePath encuentra con Block I/O. */
static EFI_BLOCK_IO_PROTOCOL *disco_bio;
static EFI_DISK_IO_PROTOCOL *disco_dio;

static int abrir_disco(EFI_HANDLE dispositivo)
{
    EFI_GUID gdp = GUID_DEVICE_PATH, gbio = GUID_BLOCK_IO, gdio = GUID_DISK_IO;
    EFI_DEVICE_PATH_PROTOCOL *dp = 0;
    if (EFI_ERROR(BS->HandleProtocol(dispositivo, &gdp, (void **)&dp)) || !dp) return -1;
    UINTN tam = 0;
    UINT8 *p = (UINT8 *)dp, *ultimo = 0;
    for (int k = 0; k < 64; k++) {
        UINTN l = le16(p + 2);
        if (l < 4) return -1;
        if (p[0] == 0x7F && p[1] == 0xFF) { tam += l; break; }
        ultimo = p;
        p += l;
        tam += l;
    }
    if (!ultimo || tam > 1024) return -1;
    UINT8 copia[1024];
    memcpy(copia, dp, tam);
    UINT8 *fim = copia + (ultimo - (UINT8 *)dp);
    fim[0] = 0x7F; fim[1] = 0xFF; fim[2] = 4; fim[3] = 0;   /* el nodo de la partición pasa a ser el final */
    EFI_DEVICE_PATH_PROTOCOL *r = (EFI_DEVICE_PATH_PROTOCOL *)copia;
    EFI_HANDLE disco = 0;
    if (EFI_ERROR(BS->LocateDevicePath(&gbio, &r, &disco)) || !disco) return -1;
    if (r->Type != 0x7F) return -1;   /* no era el disco entero */
    EFI_BLOCK_IO_PROTOCOL *bio = 0;
    if (EFI_ERROR(BS->HandleProtocol(disco, &gbio, (void **)&bio)) || !bio || !bio->Media) return -1;
    UINT32 bs = bio->Media->BlockSize;
    if (bio->Media->LogicalPartition || bs < 512 || bs > 4096 || (bs & (bs - 1))) return -1;
    EFI_DISK_IO_PROTOCOL *dio = 0;
    if (EFI_ERROR(BS->HandleProtocol(disco, &gdio, (void **)&dio))) dio = 0;
    disco_bio = bio;
    disco_dio = dio;
    return 0;
}

/* Lee n bytes del disco desde el byte 'pos' (múltiplo de 512). Con Disk I/O, directo; sin él, por
 * bloques enteros a través de un búfer alineado (el Block I/O pide alineación y bloques enteros). */
#define REBOTE 65536
static UINT8 *rebote;   /* 16 páginas pedidas al firmware (estáticas, ocuparían 64 KB en el .EFI) */

static int ler_disco(UINT64 pos, UINTN n, void *dest)
{
    if (!disco_bio) return -1;
    UINT32 media = disco_bio->Media->MediaId;
    if (disco_dio) return EFI_ERROR(disco_dio->ReadDisk(disco_dio, media, pos, n, dest)) ? -1 : 0;
    UINT32 bs = disco_bio->Media->BlockSize;
    if (disco_bio->Media->IoAlign > 4096) return -1;   /* el búfer de rebote está alineado a 4 KB */
    if (!rebote) {
        EFI_PHYSICAL_ADDRESS a = 0xFFFFFFFFu;
        if (EFI_ERROR(BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, REBOTE / 4096, &a))) return -1;
        rebote = (UINT8 *)(UINTN)a;
    }
    UINT32 sh = 0;
    while ((1u << sh) < bs) sh++;   /* bs es potencia de 2: sin divisiones de 64 bits (IA32 no tiene libgcc) */
    UINT8 *d = dest;
    while (n) {
        UINT64 bloco = pos >> sh;
        UINTN desl = (UINTN)(pos & (bs - 1));
        UINTN q = REBOTE - desl;
        if (q > n) q = n;
        UINTN ler = (desl + q + bs - 1) / bs * bs;
        if (EFI_ERROR(disco_bio->ReadBlocks(disco_bio, media, bloco, ler, rebote))) return -1;
        memcpy(d, rebote + desl, q);
        d += q;
        pos += q;
        n -= q;
    }
    return 0;
}

/* Lee del disco una parte del sistema (kernel o ramdisk) por trozos de 1 MB, con un punto
 * cada 4 MB, como ler_arquivo. 'lba' en sectores de 512 (los de la cabecera). */
static UINT64 disco_base;   /* D270: en un CD, dónde empieza la imagen de disco de El Torito (bytes) */
static int disco_cd;

static int ler_do_disco(UINT32 lba, void *dest, UINT64 bytes)
{
    UINT8 *p = dest;
    UINT64 lido = 0;
    while (lido < bytes) {
        UINTN trozo = (UINTN)(bytes - lido > 0x100000 ? 0x100000 : bytes - lido);
        if (ler_disco(disco_base + (UINT64)lba * 512 + lido, trozo, p + lido) != 0) {
            escrever(L" fallo de lectura del disco en el sector ");
            escrever_num(lba + lido / 512);
            return -1;
        }
        lido += trozo;
        if ((lido & 0x3FFFFF) == 0 || lido == bytes) escrever(L".");
    }
    return 0;
}

static UINT64 le64(const UINT8 *p) { return le32(p) | (UINT64)le32(p + 4) << 32; }

/* El GUID de tipo de la partición de SaguiOS en una GPT (D266): 53414755-494F-4F53-8001-534147554953,
 * con los tres primeros campos en little-endian, como se guarda en la entrada. */
static const UINT8 GPT_TIPO_SAGUI[16] = { 0x55, 0x47, 0x41, 0x53, 0x4F, 0x49, 0x53, 0x4F,
                                          0x80, 0x01, 0x53, 0x41, 0x47, 0x55, 0x49, 0x53 };
#define ENTRADAS_MAX 16384   /* 128 entradas de 128 bytes */

/* D270: la cabecera de arranque del disco de la ESP. Primero el LBA 1 (la imagen híbrida y los
 * discos instalados con MBR); si no, en un disco GPT, el primer sector de la partición de SaguiOS
 * (tipo propio: así queda un disco instalado con GPT, D266). Devuelve el sector (de 512) donde
 * está, o 0 si no hay ninguna. */
static UINT32 cabecera_do_disco(UINT8 *dest)
{
    if (!disco_bio) return 0;
    if (ler_disco(512, 512, dest) == 0 && le32(dest) == CAB_ASSINATURA_VAL) return 1;
    UINT32 bs = disco_bio->Media->BlockSize;
    /* D270: un CD (ISO de tools/mkiso.py): la imagen de disco de SaguiOS es la entrada por defecto
     * del catálogo de El Torito (emulación de disco duro, la que usa la BIOS); su LBA 1 es la cabecera
     * y los sectores de la cabecera cuentan desde el inicio de esa imagen. */
    if (bs == 2048) {
        static UINT8 cd[2048];
        if (ler_disco(17 * 2048, 2048, cd) == 0 && memcmp(cd, "\0CD001\1EL TORITO SPECIFICATION", 30) == 0) {
            UINT32 cat = le32(cd + 71);
            if (cat && ler_disco((UINT64)cat * 2048, 2048, cd) == 0 && cd[0] == 1 && cd[30] == 0x55 && cd[31] == 0xAA &&
                cd[32] == 0x88 && (cd[33] & 0x0F) == 4) {
                UINT64 base = (UINT64)le32(cd + 40) * 2048;
                if (base && ler_disco(base + 512, 512, dest) == 0 && le32(dest) == CAB_ASSINATURA_VAL) {
                    disco_base = base;
                    disco_cd = 1;
                    return 1;
                }
            }
        }
        return 0;
    }
    static UINT8 cab_gpt[512];
    if (ler_disco(bs, 512, cab_gpt) != 0 || memcmp(cab_gpt, "EFI PART", 8) != 0) return 0;
    UINT64 lba_ent = le64(cab_gpt + 72);
    UINT32 qtd = le32(cab_gpt + 80), tam = le32(cab_gpt + 84);
    if (tam < 128 || tam > 1024 || (tam & 7) || !qtd || lba_ent < 2) return 0;
    if ((UINT64)qtd * tam > ENTRADAS_MAX) qtd = ENTRADAS_MAX / tam;
    UINT8 *ents = 0;
    if (EFI_ERROR(BS->AllocatePool(EfiLoaderData, ENTRADAS_MAX, (void **)&ents)) || !ents) return 0;
    UINT32 achado = 0;
    if (ler_disco(lba_ent * bs, qtd * tam, ents) == 0)
        for (UINT32 i = 0; i < qtd && !achado; i++) {
            const UINT8 *e = ents + i * tam;
            if (memcmp(e, GPT_TIPO_SAGUI, 16) != 0) continue;
            UINT64 setor = le64(e + 32) * (bs / 512);
            if (!setor || setor > 0xFFFFFFFFull) continue;   /* el kernel cuenta sectores de 32 bits */
            if (ler_disco(setor * 512, 512, dest) == 0 && le32(dest) == CAB_ASSINATURA_VAL) achado = (UINT32)setor;
        }
    BS->FreePool(ents);
    return achado;
}

/* Las dos cabeceras describen el mismo sistema (pueden diferir en lo que se graba
 * después: la resolución preferida, la marca de instalación...). */
/* v8.5.4.2 (D379): el menú de la tecla M: el modo de la raíz para este arranque (RAIZ_OPC_* del núcleo:
 * 3 live, 4 todo a la RAM, 2 instalado, 1 rescate; 0 = el de siempre). Vacía primero la M repetida. */
static UINT32 menu_raiz(void)
{
    EFI_INPUT_KEY k;
    while (!EFI_ERROR(ST->ConIn->ReadKeyStroke(ST->ConIn, &k))) {}
    escrever(L"M pulsada: modo de arranque\r\n"
             L"  1 live desde el medio (los cambios, en la RAM)\r\n"
             L"  2 todo a la RAM (despues se puede sacar el medio)\r\n"
             L"  3 instalado (la raiz del disco, con escritura)\r\n"
             L"  4 rescate (solo el sistema minimo)\r\n"
             L"  Intro: el de siempre.  Elige: ");
    static const UINT32 tabela[4] = { 3, 4, 2, 1 };
    for (;;) {
        if (EFI_ERROR(ST->ConIn->ReadKeyStroke(ST->ConIn, &k))) { BS->Stall(20000); continue; }
        if (k.UnicodeChar == L'\r' || k.ScanCode == 0x17 /* Esc */) { escrever(L"el de siempre\r\n"); return 0; }
        if (k.UnicodeChar >= L'1' && k.UnicodeChar <= L'4') {
            CHAR16 eco[4] = { k.UnicodeChar, L'\r', L'\n', 0 };
            escrever(eco);
            return tabela[k.UnicodeChar - L'1'];
        }
    }
}

static int mesma_cabecera(const UINT8 *a, const UINT8 *b)
{
    static const UINT8 campos[] = { 0, CAB_K_BYTES, CAB_K_ENDERECO, CAB_R_BYTES, CAB_R_ENDERECO,
                                    CAB_K_ENTRADA, CAB_K_SOMA, CAB_R_SOMA };
    for (UINTN i = 0; i < sizeof campos; i++)
        if (le32(a + campos[i]) != le32(b + campos[i])) return 0;
    return 1;
}

/* ------------------------------------------------------------ tablas del firmware */

static void *tabela_config(EFI_GUID g)
{
    for (UINTN i = 0; i < ST->NumberOfTableEntries; i++)
        if (guid_igual(&ST->ConfigurationTable[i].VendorGuid, &g))
            return ST->ConfigurationTable[i].VendorTable;
    return 0;
}

/* Busca una tabla ACPI por su firma en la XSDT (o la RSDT). */
static const UINT8 *acpi_tabela(const UINT8 *rsdp, const char *firma)
{
    if (!rsdp) return 0;
    const UINT8 *raiz_t = 0;
    UINTN passo = 4;
    if (rsdp[15] >= 2) {
        UINT64 x = le32(rsdp + 24) | (UINT64)le32(rsdp + 28) << 32;
        if (x && x < 0x100000000ull) { raiz_t = (const UINT8 *)(UINTN)x; passo = 8; }
    }
    if (!raiz_t) raiz_t = (const UINT8 *)(UINTN)le32(rsdp + 16);
    if (!raiz_t) return 0;
    UINT32 tam = le32(raiz_t + 4);
    if (tam < 36 || tam > 0x10000) return 0;
    for (UINT32 o = 36; o + passo <= tam; o += (UINT32)passo) {
        UINT64 e = le32(raiz_t + o);
        if (passo == 8) e |= (UINT64)le32(raiz_t + o + 4) << 32;
        if (!e || e >= 0x100000000ull) continue;
        const UINT8 *t = (const UINT8 *)(UINTN)e;
        if (memcmp(t, firma, 4) == 0) return t;
    }
    return 0;
}

/* ------------------------------------------------------------ vídeo (GOP) */

static UINT32 bits_baixos(UINT32 m) { UINT32 n = 0; if (!m) return 0; while (!(m & 1)) { m >>= 1; n++; } return n; }
static UINT32 bits_contar(UINT32 m) { UINT32 n = 0; while (m) { n += m & 1; m >>= 1; } return n; }

/* Formato de un modo en 32 bits por píxel: tamaño y posición de cada color. 0 = no sirve. */
static int formato_modo(const EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *i, UINT8 cor[6])
{
    switch (i->PixelFormat) {
    case PixelBlueGreenRedReserved8BitPerColor:   /* BGRx: el de VBE de siempre */
        cor[0] = 8; cor[1] = 16; cor[2] = 8; cor[3] = 8; cor[4] = 8; cor[5] = 0;
        return 1;
    case PixelRedGreenBlueReserved8BitPerColor:   /* RGBx */
        cor[0] = 8; cor[1] = 0; cor[2] = 8; cor[3] = 8; cor[4] = 8; cor[5] = 16;
        return 1;
    case PixelBitMask: {
        const EFI_PIXEL_BITMASK *m = &i->PixelInformation;
        UINT32 r = bits_contar(m->RedMask), g = bits_contar(m->GreenMask), b = bits_contar(m->BlueMask);
        if (!r || !g || !b || r > 8 || g > 8 || b > 8) return 0;
        cor[0] = (UINT8)r; cor[1] = (UINT8)bits_baixos(m->RedMask);
        cor[2] = (UINT8)g; cor[3] = (UINT8)bits_baixos(m->GreenMask);
        cor[4] = (UINT8)b; cor[5] = (UINT8)bits_baixos(m->BlueMask);
        return 1;
    }
    default:
        return 0;   /* PixelBltOnly: sin framebuffer, el kernel no puede dibujar */
    }
}

/* Resolución nativa del panel: el primer descriptor de temporización del EDID. */
static void edid_nativa(UINT32 *l, UINT32 *a)
{
    *l = *a = 0;
    EFI_GUID ggop = GUID_GOP;
    EFI_HANDLE *h = 0;
    UINTN n = 0;
    if (EFI_ERROR(BS->LocateHandleBuffer(ByProtocol, &ggop, 0, &n, &h)) || !h) return;
    for (UINTN k = 0; k < n && !*l; k++) {
        EFI_GUID gs[2] = { GUID_EDID_ACTIVE, GUID_EDID_DISCOVERED };
        for (int j = 0; j < 2 && !*l; j++) {
            EFI_EDID_PROTOCOL *e = 0;
            if (EFI_ERROR(BS->HandleProtocol(h[k], &gs[j], (void **)&e)) || !e || e->SizeOfEdid < 128 || !e->Edid)
                continue;
            const UINT8 *d = e->Edid + 54;
            if (!d[0] && !d[1]) continue;   /* no es una temporización */
            *l = d[2] | (UINT32)(d[4] & 0xF0) << 4;
            *a = d[5] | (UINT32)(d[7] & 0xF0) << 4;
        }
    }
    BS->FreePool(h);
}

typedef struct {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    UINT32 modo, largura, altura, pitch;
    UINT8  cor[6];
    int    ok;
} video_t;

/* Elige el modo como la etapa 2 (procurar_vesa) y llena la lista de modos. Puntos:
 * la resolución de la cabecera 40, la nativa del panel 35 (hasta 1920x1200), 1024x768 30,
 * 800x600 20, 640x480 10, otra de al menos 640x480 0. */
static void escolher_video(video_t *v, const UINT8 *cab, modo_vbe_t *modos, UINT32 *qtd)
{
    EFI_GUID ggop = GUID_GOP;
    v->ok = 0;
    v->gop = 0;
    *qtd = 0;
    /* La GOP de la consola (ConsoleOutHandle) suele ser la del "divisor de consola" del
     * firmware, que solo sabe Blt y no tiene framebuffer: se busca la de una tarjeta
     * de verdad, con el modo actual con framebuffer. */
    EFI_HANDLE *h = 0;
    UINTN n = 0;
    if (!EFI_ERROR(BS->LocateHandleBuffer(ByProtocol, &ggop, 0, &n, &h)) && h) {
        for (UINTN k = 0; k < n && !v->gop; k++) {
            EFI_GRAPHICS_OUTPUT_PROTOCOL *g = 0;
            if (EFI_ERROR(BS->HandleProtocol(h[k], &ggop, (void **)&g)) || !g || !g->Mode || !g->Mode->Info) continue;
            if (g->Mode->Info->PixelFormat != PixelBltOnly && g->Mode->FrameBufferBase) v->gop = g;
        }
        BS->FreePool(h);
    }
    if (!v->gop) return;
    UINT32 pl = le16(cab + CAB_RES_LARG), pa = le16(cab + CAB_RES_ALT), nl, na;
    edid_nativa(&nl, &na);
    if (nl > 1920 || na > 1200) nl = na = 0;
    if (nl) { escrever(L"panel "); escrever_num(nl); escrever(L"x"); escrever_num(na); escrever(L", "); }
    int melhor = -1;
    for (UINT32 m = 0; m < v->gop->Mode->MaxMode; m++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *i = 0;
        UINTN t = 0;
        if (EFI_ERROR(v->gop->QueryMode(v->gop, m, &t, &i)) || !i) continue;
        UINT8 cor[6];
        UINT32 l = i->HorizontalResolution, a = i->VerticalResolution, ppl = i->PixelsPerScanLine;
        int ok = formato_modo(i, cor) && l >= 640 && a >= 480 && ppl >= l && l <= 0xFFFF && a <= 0xFFFF;
        BS->FreePool(i);
        if (!ok) continue;
        if (*qtd < MODOS_MAX) {
            modo_vbe_t *e = &modos[(*qtd)++];
            e->modo = (UINT16)m; e->largura = (UINT16)l; e->altura = (UINT16)a; e->bpp = 32; e->reservado = 0;
        }
        int p = 0;
        if (pl && l == pl && a == pa) p = 40;
        else if (nl && l == nl && a == na) p = 35;
        else if (l == 1024 && a == 768) p = 30;
        else if (l == 800 && a == 600) p = 20;
        else if (l == 640 && a == 480) p = 10;
        if (p > melhor) {
            melhor = p;
            v->modo = m; v->largura = l; v->altura = a; v->pitch = ppl * 4;
            memcpy(v->cor, cor, 6);
            v->ok = 1;
        }
    }
}

/* ------------------------------------------------------------ mapa de memoria */

/* Tipo E820 (ACPI) de cada tipo de memoria UEFI, como dice la especificación de ACPI. */
static UINT32 tipo_e820(UINT32 t)
{
    switch (t) {
    case EfiLoaderCode: case EfiLoaderData: case EfiBootServicesCode: case EfiBootServicesData:
    case EfiConventionalMemory:
        return 1;
    case EfiACPIReclaimMemory: return 3;
    case EfiACPIMemoryNVS:     return 4;
    case EfiUnusableMemory:    return 5;
    case EfiPersistentMemory:  return 7;
    default:                   return 2;   /* reservada: servicios de ejecución, MMIO... */
    }
}

#define BRUTO_MAX 512
static regiao_e820_t bruto[BRUTO_MAX];

/* Convierte el mapa de UEFI (sin servicios del firmware: ya se salió de ellos):
 * ordenado por dirección, regiones contiguas del mismo tipo unidas y, si aún son más
 * de 64, sin las de más arriba (las primeras en sobrar son las de encima de 4 GB, que
 * un kernel de 32 bits no usa). Devuelve cuántas quedaron. */
static UINT32 converter_mapa(const UINT8 *mapa, UINTN tam, UINTN desc, regiao_e820_t *saida)
{
    UINT32 n = 0;
    for (UINTN o = 0; o + sizeof(EFI_MEMORY_DESCRIPTOR) <= tam && n < BRUTO_MAX; o += desc) {
        const EFI_MEMORY_DESCRIPTOR *d = (const EFI_MEMORY_DESCRIPTOR *)(mapa + o);
        if (!d->NumberOfPages) continue;
        regiao_e820_t r = { d->PhysicalStart, d->NumberOfPages << 12, tipo_e820(d->Type), 1 };
        UINT32 k = n++;
        while (k && bruto[k - 1].base > r.base) { bruto[k] = bruto[k - 1]; k--; }
        bruto[k] = r;
    }
    UINT32 m = 0;
    for (UINT32 i = 0; i < n; i++) {
        if (m && bruto[i].tipo == saida[m - 1].tipo && saida[m - 1].base + saida[m - 1].tamanho == bruto[i].base) {
            saida[m - 1].tamanho += bruto[i].tamanho;
            continue;
        }
        if (m == MAPA_MAX) break;
        saida[m++] = bruto[i];
    }
    return m;
}

/* ¿Está [ini, fim) entero en memoria que tras ExitBootServices es libre? */
static int faixa_livre(const UINT8 *mapa, UINTN tam, UINTN desc, UINT64 ini, UINT64 fim, UINT32 *tipo_mau, UINT64 *onde)
{
    UINT64 coberto = ini;
    for (int volta = 0; volta < BRUTO_MAX && coberto < fim; volta++) {
        int avancou = 0;
        for (UINTN o = 0; o + sizeof(EFI_MEMORY_DESCRIPTOR) <= tam; o += desc) {
            const EFI_MEMORY_DESCRIPTOR *d = (const EFI_MEMORY_DESCRIPTOR *)(mapa + o);
            UINT64 b = d->PhysicalStart, e = b + (d->NumberOfPages << 12);
            if (b > coberto || e <= coberto) continue;
            if (tipo_e820(d->Type) != 1) { *tipo_mau = d->Type; *onde = coberto; return 0; }
            coberto = e;
            avancou = 1;
        }
        if (!avancou) { *tipo_mau = 0xFFFFFFFFu; *onde = coberto; return 0; }   /* hueco sin describir */
    }
    return coberto >= fim;
}

/* Si algo falla después de ExitBootServices ya no hay texto: una franja roja arriba. */
static void parar_sem_firmware(const video_t *v, UINT64 fb)
{
    if (v->ok && fb && fb < 0x100000000ull) {
        UINT32 *p = (UINT32 *)(UINTN)fb;
        UINT32 vermelho = (0xFFu << v->cor[1]);
        for (UINT32 y = 0; y < 32 && y < v->altura; y++)
            for (UINT32 x = 0; x < v->largura; x++)
                p[y * (v->pitch / 4) + x] = vermelho;
    }
    for (;;) __asm__ volatile("cli; hlt");
}

/* ------------------------------------------------------------ principal */

EFI_STATUS EFIAPI efi_main(EFI_HANDLE img, EFI_SYSTEM_TABLE *st)
{
    imagem = img;
    ST = st;
    BS = st->BootServices;
    BS->SetWatchdogTimer(0, 0, 0, 0);   /* sin el reinicio de los 5 minutos */
    escrever(L"\r\nSaguiOS - cargador UEFI " ARQUITETURA L"\r\n");

#if defined(__i386__)
    /* D310: como la etapa 2 de la BIOS. En x86-64 no hace falta: toda CPU de 64 bits
     * tiene CMOV. Se para sin volver al firmware y sin reiniciar. */
    {
        const CHAR16 *causa = cpu_nao_i686();
        if (causa) {
            escrever(L"[0/6] CPU: Esta CPU no es i686 (");
            escrever(causa);
            escrever(L"): SaguiOS necesita un Pentium Pro / Pentium II / AMD Athlon o posterior. "
                     L"Las AMD K6, K6-2, K6-III y los Pentium/Pentium MMX no son compatibles.\r\n"
                     L"La maquina queda detenida; ya se puede apagar.\r\n");
            for (;;) __asm__ volatile("cli; hlt");
        }
    }
#endif

    /* D255: Ctrl pulsado al arrancar = modo rescate. UEFI solo da el estado de Ctrl con
     * una tecla en la cola: se mira si la hay, sin esperar. */
    UINT32 rescate = 0, acel_segura = 0, raiz_modo = 0;
    {
        EFI_GUID gx = GUID_TEXT_INPUT_EX;
        EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *tx = 0;
        if (!EFI_ERROR(BS->HandleProtocol(ST->ConsoleInHandle, &gx, (void **)&tx)) && tx) {
            EFI_KEY_DATA k;
            memset(&k, 0, sizeof k);
            if (!EFI_ERROR(tx->ReadKeyStrokeEx(tx, &k)) && (k.KeyState.KeyShiftState & EFI_SHIFT_STATE_VALID) &&
                (k.KeyState.KeyShiftState & (EFI_LEFT_CONTROL_PRESSED | EFI_RIGHT_CONTROL_PRESSED))) {
                rescate = 1;
                escrever(L"Ctrl pulsado: MODO RESCATE\r\n");
            }
            /* D323: Mayús = modo seguro de la aceleración */
            if ((k.KeyState.KeyShiftState & EFI_SHIFT_STATE_VALID) &&
                (k.KeyState.KeyShiftState & (EFI_RIGHT_SHIFT_PRESSED | EFI_LEFT_SHIFT_PRESSED))) {
                acel_segura = 1;
                escrever(L"Mayus pulsado: aceleracion en modo seguro\r\n");
            }
            /* v8.5.4.2 (D379): M = menú del modo de la raíz (como en la etapa 2 por BIOS). */
            if (k.Key.UnicodeChar == L'm' || k.Key.UnicodeChar == L'M') raiz_modo = menu_raiz();
        }
    }

    /* ---------------------------------------------------------- [1/6] cabecera */
    EFI_GUID gli = GUID_LOADED_IMAGE, gfs = GUID_SIMPLE_FS;
    EFI_LOADED_IMAGE_PROTOCOL *li = 0;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = 0;
    if (EFI_ERROR(BS->HandleProtocol(img, &gli, (void **)&li)) || !li)
        parar(L"el firmware no da la imagen cargada (Loaded Image)");
    if (EFI_ERROR(BS->HandleProtocol(li->DeviceHandle, &gfs, (void **)&fs)) || !fs ||
        EFI_ERROR(fs->OpenVolume(fs, &raiz)) || !raiz)
        parar(L"no se puede abrir la particion EFI (ESP) de la que arranco");

    /* D270: la cabecera buena es la del disco (LBA 1 o, en un disco GPT instalado, el primer sector
     * de la partición de SaguiOS): es la que el kernel compara para saber de qué disco arrancó y la
     * que "pantalla --guardar" reescribe. El kernel y el ramdisk se leen de los ficheros de la ESP
     * si son los mismos (la imagen híbrida) y, si no (un disco instalado: su ESP solo lleva el
     * cargador), del disco, en los sectores que dice la cabecera. Las sumas lo comprueban todo. */
    static UINT8 cab[512], cab_esp[512], cab_disco[512];
    escrever(L"[1/6] cabecera: ");
    UINT64 t_esp = 0;
    EFI_FILE_PROTOCOL *f_esp = abrir(L"\\SAGUI\\CABECERA.BIN", &t_esp);
    int tem_esp = 0;
    if (f_esp) {
        f_esp->Close(f_esp);
        tem_esp = ler_arquivo(L"\\SAGUI\\CABECERA.BIN", cab_esp, 512) == 0 && le32(cab_esp) == CAB_ASSINATURA_VAL;
    }
    UINT32 cab_lba = abrir_disco(li->DeviceHandle) == 0 ? cabecera_do_disco(cab_disco) : 0;
    int do_disco = 0;   /* 1 = kernel y ramdisk se leen del disco, no de la ESP */
    if (cab_lba) {
        memcpy(cab, cab_disco, 512);
        do_disco = !(tem_esp && mesma_cabecera(cab_esp, cab_disco));
        if (disco_cd) escrever(L" la de la imagen del CD (El Torito)");
        else if (cab_lba == 1) escrever(L" la del disco (LBA 1)");
        else { escrever(L" la de la particion de SaguiOS (GPT, sector "); escrever_num(cab_lba); escrever(L")"); }
        escrever(do_disco ? L"; sistema leido del disco\r\n" : L"\r\n");
    } else if (tem_esp) {
        memcpy(cab, cab_esp, 512);
        escrever(L" la de la ESP\r\n");
    } else {
        parar(L"no se encuentra SaguiOS: ni \\SAGUI\\CABECERA.BIN en la ESP ni la cabecera de arranque en el disco "
              L"(LBA 1 o primer sector de la particion GPT de SaguiOS)");
    }
    UINT32 k_ender = le32(cab + CAB_K_ENDERECO), k_bytes = le32(cab + CAB_K_BYTES);
    UINT32 r_ender = le32(cab + CAB_R_ENDERECO), r_bytes = le32(cab + CAB_R_BYTES);
    if (k_ender != 0x100000 || r_ender < k_ender + k_bytes || !k_bytes || !r_bytes || r_ender + r_bytes < r_ender)
        parar(L"la cabecera tiene direcciones de carga raras");
    /* Solo el kernel tiene que estar en su sitio: la imagen y la BSS, de 1 MB a la
     * dirección del ramdisk de la cabecera (mkimagem la pone tras el fin de la BSS). El
     * ramdisk va donde el firmware dé memoria: el kernel lo encuentra por la boot_info
     * (OVMF, por ejemplo, tiene memoria NVS en 8 MB, en medio de donde iría por BIOS). */
    UINT64 dest_fim = r_ender;
    UINT32 total = (UINT32)((dest_fim - k_ender + 0xFFF) & ~0xFFFull);
    UINT32 r_paginas = (r_bytes + 0xFFF) / 4096;

    /* La zona de transferencia: mejor por debajo de 1 MB (nunca pisa el kernel); si no
     * hay sitio, en cualquier parte por debajo de 4 GB fuera del destino. */
    EFI_PHYSICAL_ADDRESS zona = 0x9FFFF;
    if (EFI_ERROR(BS->AllocatePages(AllocateMaxAddress, EfiLoaderCode, ZONA_PAGINAS, &zona))) {
        int ok = 0;
        for (int t = 0; t < 8 && !ok; t++) {
            zona = 0xFFFFFFFFu;
            if (EFI_ERROR(BS->AllocatePages(AllocateMaxAddress, EfiLoaderCode, ZONA_PAGINAS, &zona))) break;
            ok = zona + ZONA_PAGINAS * 4096 <= k_ender || zona >= dest_fim;
            /* la que pisa el destino se queda reservada: la siguiente sale en otro sitio */
        }
        if (!ok) parar(L"no hay memoria para la zona de transferencia");
    }
    UINT8 *z = (UINT8 *)(UINTN)zona;
    memset(z, 0, ZONA_PAGINAS * 4096);

    /* El kernel: en su sitio si el firmware lo deja; si no, debajo de 4 GB y el trampolín lo mueve. */
    EFI_PHYSICAL_ADDRESS carga = k_ender;
    int no_sitio = !EFI_ERROR(BS->AllocatePages(AllocateAddress, EfiLoaderData, total / 4096, &carga));
    if (!no_sitio) {
        carga = 0xFFFFFFFFu;
        if (EFI_ERROR(BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, total / 4096, &carga)))
            parar(L"no hay memoria por debajo de 4 GB para el kernel");
    }
    UINT8 *buf = (UINT8 *)(UINTN)carga;
    /* El ramdisk: donde lo pone la etapa 2 si está libre; si no, en la RAM que el kernel mapea
     * (por debajo de 0x78000000, D049) y fuera del destino del kernel. Se queda ahí. */
    EFI_PHYSICAL_ADDRESS rd = r_ender;
    if (EFI_ERROR(BS->AllocatePages(AllocateAddress, EfiLoaderData, r_paginas, &rd))) {
        int ok = 0;
        for (int t = 0; t < 8 && !ok; t++) {
            rd = 0x77FFFFFFu;
            if (EFI_ERROR(BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, r_paginas, &rd))) break;
            ok = rd + (UINT64)r_paginas * 4096 <= k_ender || rd >= dest_fim;
        }
        if (!ok) parar(L"no hay memoria por debajo de 1,875 GB para el ramdisk");
    }

    /* ---------------------------------------------------------- [2/6] y [3/6] */
    escrever(L"[2/6] kernel ");
    if (do_disco ? ler_do_disco(le32(cab + CAB_K_LBA), buf, k_bytes) != 0 : ler_arquivo(L"\\SAGUI\\KERNEL.BIN", buf, k_bytes) != 0)
        parar(do_disco ? L"no se pudo leer el kernel del disco" : L"no se pudo leer \\SAGUI\\KERNEL.BIN");
    if (somar(buf, k_bytes) != le32(cab + CAB_K_SOMA)) parar(L"kernel: datos leidos corruptos (la suma no coincide)");
    escrever(L" ok\r\n[3/6] ramdisk ");
    if (do_disco ? ler_do_disco(le32(cab + CAB_R_LBA), (void *)(UINTN)rd, r_bytes) != 0
                 : ler_arquivo(L"\\SAGUI\\RAMDISK.SGF", (void *)(UINTN)rd, r_bytes) != 0)
        parar(do_disco ? L"no se pudo leer el ramdisk del disco" : L"no se pudo leer \\SAGUI\\RAMDISK.SGF");
    if (somar((const UINT8 *)(UINTN)rd, r_bytes) != le32(cab + CAB_R_SOMA))
        parar(L"ramdisk: datos leidos corruptos (la suma no coincide)");
    escrever(L" ok\r\n");
    if (!no_sitio) { escrever(L"      (kernel cargado en "); escrever_hex(carga); escrever(L": se movera a 1 MB)\r\n"); }
    if (rd != r_ender) { escrever(L"      (ramdisk en "); escrever_hex(rd); escrever(L")\r\n"); }

    /* ---------------------------------------------------------- [4/6] tablas */
    info_boot_t *bi = (info_boot_t *)(z + Z_BOOTINFO);
    parametros_t *par = (parametros_t *)(z + Z_PARAMETROS);
    escrever(L"[4/6] ACPI ");
    UINT8 *rsdp = tabela_config((EFI_GUID)GUID_ACPI_20);
    if (!rsdp) rsdp = tabela_config((EFI_GUID)GUID_ACPI_10);
    if (rsdp && abaixo_4g(rsdp) && memcmp(rsdp, "RSD PTR ", 8) == 0) {
        bi->rsdp = (UINT32)(UINTN)rsdp;
        escrever_hex(bi->rsdp);
    } else {
        rsdp = 0;
        escrever(L"no");
    }
    UINT8 *sm = tabela_config((EFI_GUID)GUID_SMBIOS3);
    if (!sm || !abaixo_4g(sm) || memcmp(sm, "_SM3_", 5) != 0) sm = tabela_config((EFI_GUID)GUID_SMBIOS);
    if (sm && abaixo_4g(sm) && (memcmp(sm, "_SM_", 4) == 0 || memcmp(sm, "_SM3_", 5) == 0)) {
        bi->smbios = (UINT32)(UINTN)sm;
        escrever(L", SMBIOS "); escrever_hex(bi->smbios);
    }
    par->opcoes = 1;   /* el APIC local en cable virtual, siempre */
    const UINT8 *madt = acpi_tabela(rsdp, "APIC");
    if (madt) {
        UINT32 tam = le32(madt + 4), n = 0;
        for (UINT32 o = 44; o + 2 <= tam && madt[o + 1] >= 2; o += madt[o + 1])
            if (madt[o] == 1 && madt[o + 1] >= 12 && n < 4) par->ioapic[n++] = le32(madt + o + 4);
        if (n) { par->opcoes |= 2; escrever(L", "); escrever_num(n); escrever(L" IOAPIC"); }
    }
    const UINT8 *hpet = acpi_tabela(rsdp, "HPET");
    if (hpet && le32(hpet + 4) >= 56 && hpet[40] == 0 && !le32(hpet + 48)) {   /* GAS en memoria, debajo de 4 GB */
        par->hpet = le32(hpet + 44);
        if (par->hpet) { par->opcoes |= 4; escrever(L", HPET"); }
    }
    escrever(L"\r\n");

    /* ---------------------------------------------------------- [5/6] vídeo */
    escrever(L"[5/6] video: ");
    video_t v;
    UINT32 qtd_modos = 0;
    escolher_video(&v, cab, (modo_vbe_t *)(z + Z_MODOS), &qtd_modos);
    if (v.ok) {
        escrever(L"GOP "); escrever_num(v.largura); escrever(L"x"); escrever_num(v.altura);
        escrever(L"x32 (modo "); escrever_num(v.modo); escrever(L" de "); escrever_num(qtd_modos); escrever(L")\r\n");
    } else {
        escrever(L"sin GOP con framebuffer: el kernel no podra dibujar (solo la serie)\r\n");
    }

    /* ---------------------------------------------------------- boot_info */
    bi->assinatura = BOOTINFO_SGBI;
    bi->versao = 2;
    bi->disco = 0x80;
    bi->a20_metodo = 0;   /* con UEFI la A20 siempre está activa */
    bi->kernel_inicio = k_ender;
    bi->kernel_bytes = k_bytes;
    bi->ramdisk_inicio = (UINT32)rd;
    bi->ramdisk_bytes = r_bytes;
    bi->setores_lidos = (k_bytes + 511) / 512 + (r_bytes + 511) / 512;
    memcpy(bi->teste, cab + CAB_TESTE, 64);
    bi->opcoes = le16(cab + CAB_OPCOES) | (cab[CAB_MOTOR] ? OPC_SGL_SOFTWARE : 0) | (rescate ? OPC_RESCATE : 0) | (acel_segura ? OPC_ACEL_SEGURA : 0) | raiz_modo << 17;
    bi->res_pref = le16(cab + CAB_RES_LARG) | (UINT32)le16(cab + CAB_RES_ALT) << 16;
    bi->bpp_pref = cab[CAB_BPP_PREF];
    bi->modos_qtd = qtd_modos;
    bi->modos_end = (UINT32)(zona + Z_MODOS);
    bi->mapa_end = (UINT32)(zona + Z_MAPA);
    bi->firmware = FIRMWARE_TIPO;
    bi->cab_end = (UINT32)(zona + Z_CABECERA);
    memcpy(z + Z_CABECERA, cab, 512);

    /* El modo de vídeo se cambia al final: después el texto del firmware puede cambiar de tamaño. */
    UINT64 fb = 0;
    if (v.ok) {
        if (v.gop->Mode->Mode != v.modo && EFI_ERROR(v.gop->SetMode(v.gop, v.modo))) {
            escrever(L"      no se pudo cambiar al modo elegido; sigue el actual\r\n");
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *i = v.gop->Mode->Info;
            v.ok = formato_modo(i, v.cor);
            v.modo = v.gop->Mode->Mode; v.largura = i->HorizontalResolution; v.altura = i->VerticalResolution;
            v.pitch = i->PixelsPerScanLine * 4;
        } else {
            v.pitch = v.gop->Mode->Info->PixelsPerScanLine * 4;
        }
        fb = v.gop->Mode->FrameBufferBase;
        if (fb >= 0x100000000ull) {
            escrever(L"AVISO: el framebuffer esta por encima de 4 GB ("); escrever_hex(fb);
            escrever(L"): el kernel de 32 bits no llega. Desactiva \"Above 4G Decoding\" y \"Resizable BAR\".\r\n");
            v.ok = 0;
        }
    }
    if (v.ok) {
        bi->vesa_ok = 1;
        bi->vesa_ativo = 1;
        bi->vesa_modo = v.modo;
        bi->vesa_largura = v.largura;
        bi->vesa_altura = v.altura;
        bi->vesa_pitch = v.pitch;
        bi->vesa_bpp = 32;
        bi->vesa_fb = (UINT32)fb;
        bi->verm_tam = v.cor[0]; bi->verm_pos = v.cor[1];
        bi->verde_tam = v.cor[2]; bi->verde_pos = v.cor[3];
        bi->azul_tam = v.cor[4]; bi->azul_pos = v.cor[5];
    }

    /* ---------------------------------------------------------- [6/6] salida */
    escrever(L"[6/6] saliendo de los servicios de arranque\r\n");
    UINTN tam_mapa = 0, chave = 0, desc = 0;
    UINT32 ver = 0;
    BS->GetMemoryMap(&tam_mapa, 0, &chave, &desc, &ver);
    if (!desc) parar(L"el firmware no da el mapa de memoria");
    UINTN cap = tam_mapa + 16 * desc;   /* sitio para lo que crezca con la reserva de abajo */
    UINT8 *mapa = 0;
    if (EFI_ERROR(BS->AllocatePool(EfiLoaderData, cap, (void **)&mapa)) || !mapa)
        parar(L"sin memoria para el mapa de memoria");

    /* Antes de salir, con texto: que el destino del kernel sea memoria que quede libre. */
    tam_mapa = cap;
    if (EFI_ERROR(BS->GetMemoryMap(&tam_mapa, (EFI_MEMORY_DESCRIPTOR *)mapa, &chave, &desc, &ver)))
        parar(L"el firmware no da el mapa de memoria");
    UINT32 tipo_mau = 0;
    UINT64 onde = 0;
    if (!no_sitio && !faixa_livre(mapa, tam_mapa, desc, k_ender, dest_fim, &tipo_mau, &onde)) {
        escrever(L"la memoria en "); escrever_hex(onde); escrever(L" es del firmware (tipo UEFI ");
        escrever_num(tipo_mau); escrever(L")");
        parar(L"el kernel de SaguiOS necesita la memoria desde 1 MB y el firmware la usa");
    }

    EFI_STATUS s = BS->ExitBootServices(img, chave);
    if (EFI_ERROR(s)) {
        /* El mapa cambió entre las dos llamadas (un temporizador del firmware): otra vez,
         * sin imprimir nada en medio (imprimir también puede cambiarlo). */
        tam_mapa = cap;
        s = BS->GetMemoryMap(&tam_mapa, (EFI_MEMORY_DESCRIPTOR *)mapa, &chave, &desc, &ver);
        if (!EFI_ERROR(s)) s = BS->ExitBootServices(img, chave);
        if (EFI_ERROR(s)) parar(L"ExitBootServices fallo dos veces");
    }
    /* Desde aquí no hay servicios del firmware: ni texto ni memoria. */
    BS = 0;
    ST = 0;

    if (!no_sitio && !faixa_livre(mapa, tam_mapa, desc, k_ender, dest_fim, &tipo_mau, &onde))
        parar_sem_firmware(&v, fb);
    bi->mapa_qtd = converter_mapa(mapa, tam_mapa, desc, (regiao_e820_t *)(z + Z_MAPA));

    par->destino = k_ender;
    par->origem = (UINT32)carga;
    par->bytes = (k_bytes + 3) & ~3u;   /* la BSS la pone a cero el kernel */
    par->entrada = le32(cab + CAB_K_ENTRADA);
    par->bootinfo = (UINT32)(zona + Z_BOOTINFO);
    par->pilha = (UINT32)(zona + Z_PILHA);
    UINTN tam_t = (UINTN)(trampolim_fim - trampolim_ini);
    if (tam_t > Z_PILHA - Z_TRAMPOLIM - 1024) parar_sem_firmware(&v, fb);
    memcpy(z + Z_TRAMPOLIM, trampolim_ini, tam_t);
#if defined(__x86_64__)
    void (EFIAPI *salto)(UINTN) = (void (EFIAPI *)(UINTN))(z + Z_TRAMPOLIM + le32(z + Z_TRAMPOLIM));
    salto((UINTN)(zona + Z_PARAMETROS));
#else
    void (*salto)(UINT32) = (void (*)(UINT32))(z + Z_TRAMPOLIM + le32(z + Z_TRAMPOLIM + 4));
    salto((UINT32)(zona + Z_PARAMETROS));
#endif
    parar_sem_firmware(&v, fb);
    return EFI_SUCCESS;
}
