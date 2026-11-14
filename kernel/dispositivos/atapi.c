// SPDX-License-Identifier: GPL-2.0-or-later
/* atapi.c - lectores de CD/DVD (ATAPI), solo lectura (D207).
 *
 * Un lector ATAPI habla SCSI: cada orden es un "paquete" de 12 bytes que el
 * controlador le pasa (en el IDE con la orden PACKET 0xA0 por PIO, ata.c; en el
 * AHCI con el bit ATAPI de la cabecera de orden y el paquete en el ACMD, ahci.c).
 * Aquí está lo que no depende del controlador:
 *   - TEST UNIT READY hasta que el lector está listo (o dice que no hay disco),
 *     con la "atención de unidad" (sense 6) que da tras un reinicio o un cambio
 *     de disco; READ CAPACITY(10); READ(10) de bloques de 2048;
 *   - cuando el lector dice ERR (CHECK CONDITION), REQUEST SENSE y una línea
 *     "[ATAPI]" en el log con la sense key, el ASC/ASCQ y qué quiere decir: es lo
 *     que hace falta para entender un fallo en un PC real;
 *   - la vista en sectores de 512 para la capa de discos ("cd0"): así el ISO 9660,
 *     el acceso en bruto (blocos.c) y la imagen El Torito usan los mismos sectores
 *     que el resto de discos. Una lectura de bloques enteros va directa; un trozo
 *     de bloque pasa por un búfer de 2048 (el último bloque leído se recuerda);
 *   - la imagen de arranque El Torito con emulación de disco duro: otro disco
 *     entero ("cd0img") cuyos sectores son los del CD a partir del bloque de la
 *     imagen. Tiene el MBR y la cabecera de SaguiOS, así que la capa de discos
 *     lo reconoce como cualquier disco de arranque, y si se arrancó de este CD es
 *     el disco de arranque de verdad: de él copia el instalador (D179).
 * Nunca se escribe en un CD: ni "cd0" ni "cd0img" tienen función de escritura.
 * Toda espera tiene límite (prazo.h, por tiempo, no por vueltas).
 *
 * D233: el disco se puede meter y sacar con el sistema en marcha. "cd0" existe
 * aunque el lector esté vacío (0 sectores, "sin disco"); una tarea del kernel
 * ("cdrom") manda cada 2 s un TEST UNIT READY a cada lector (sin esperar nunca por
 * el cerrojo de los sistemas de archivos: si está ocupado, a la vuelta siguiente) y:
 *   - disco nuevo (listo tras "no hay disco", o un 6/28h "se ha cambiado el disco"
 *     con otra capacidad o con otros descriptores): lee la capacidad, la tabla de
 *     contenido (CD de audio, última sesión), el sistema de archivos (ISO 9660 o
 *     UDF, discos.c) y la imagen El Torito; si es de datos, lo monta en /cd (o en
 *     /cdN si /cd está ocupado) y avisa en el escritorio;
 *   - disco sacado (2/3Ah dos veces seguidas): desmonta lo suyo sin grabar (no hay
 *     nada que grabar), quita la imagen El Torito si nadie la tiene reservada y avisa;
 *   - "expulsar" (DBLOCO_EXPULSAR): desmonta y abre la bandeja (START STOP UNIT).
 * Lo que no cambia no se toca: un disco montado sigue montado mientras el lector
 * diga que está listo, y un desmontaje a mano no se deshace solo. */
#include "atapi.h"
#include "abi.h"
#include "bootinfo.h"
#include "cdda.h"
#include "console.h"
#include "cpu.h"
#include "iso9660.h"
#include "memoria.h"
#include "montagem.h"
#include "processo.h"
#include "relogio.h"
#include "texto.h"
#include "trava.h"

#define MAX_LEITORES 4
#define BLOCO 2048u
#define MAX_BLOCOS 16u               /* bloques por READ(10): 32 KB (el búfer del AHCI) */
#define ESPERA_ORDEM_MS   10000u     /* una orden normal; una lectura con el disco parado tarda en girar */
#define ESPERA_PRONTO_MS   8000u     /* el arranque espera como mucho esto a que el lector esté listo */
#define ESPERA_MONTAR_MS  10000u     /* D233: "mount" con un disco recién metido (el lector lo está leyendo) */
#define PERIODO_SONDA_MS   2000u     /* D233: cada cuánto mira la tarea si se ha metido o sacado un disco */

#define SCSI_TEST_UNIT_READY 0x00
#define SCSI_REQUEST_SENSE   0x03
#define SCSI_READ_CAPACITY   0x25
#define SCSI_READ10          0x28
#define SCSI_START_STOP      0x1B   /* D233: con LoEj, abrir o cerrar la bandeja */
#define SCSI_READ_TOC        0x43   /* D233: pistas (¿CD de audio?), sesiones y CD-TEXT */
#define SCSI_READ_CD         0xBE   /* D233: sectores de audio (2352 bytes) */
#define AUDIO_SETORES_ORDEM  13u    /* sectores de audio por READ CD: 30576 bytes (el búfer del AHCI es de 32 KB) */
#define CDTEXT_MAX           8192u  /* bytes de CD-TEXT que se piden (el bloque 0 cabe de sobra) */

static atapi_t *leitores[MAX_LEITORES];
static int qtd_leitores;

/* ------------------------------------------------------------ esperas */

#include "prazo.h"

/* Pausa de "ms" milisegundos (con las interrupciones como estén). */
static void pausa_ms(uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    while (!prazo_vencido(&p)) ;
}

/* ------------------------------------------------------------ sense */

static const char *nome_sense(uint8_t k)
{
    static const char *const n[16] = { "NO SENSE", "RECOVERED ERROR", "NOT READY", "MEDIUM ERROR", "HARDWARE ERROR",
                                       "ILLEGAL REQUEST", "UNIT ATTENTION", "DATA PROTECT", "BLANK CHECK", "VENDOR",
                                       "COPY ABORTED", "ABORTED COMMAND", "?", "VOLUME OVERFLOW", "MISCOMPARE", "?" };
    return n[k & 15];
}

static const char *nome_asc(uint8_t asc, uint8_t ascq)
{
    switch (asc) {
    case 0x04: return ascq == 1 ? "el lector se está preparando (girando el disco)" : "el lector no está listo";
    case 0x3A: return "no hay disco en el lector";
    case 0x28: return "se ha cambiado el disco";
    case 0x29: return "el lector se ha reiniciado";
    case 0x11: return "error de lectura: sector ilegible";
    case 0x02: return "no se encuentra la pista (disco sucio o rayado)";
    case 0x06: return "no se encuentra el principio del disco";
    case 0x15: return "error de posicionamiento";
    case 0x21: return "bloque fuera del disco";
    case 0x24: return "campo no válido en la orden";
    case 0x20: return "orden no válida para este lector";
    case 0x30: return "disco de formato desconocido o sin grabar";
    case 0x57: return "no se puede leer la tabla de contenido";
    case 0x64: return "modo de pista no válido (¿CD de audio?)";
    case 0x6F: return "protección de copia del DVD (CSS/región)";
    case 0x3E: return "el lector aún no se ha autoconfigurado";
    case 0x44: return "fallo interno del lector";
    case 0x00: return "sin información";
    default:   return "código no conocido";
    }
}

/* Pide el sense del último error y lo deja en a->sense/asc/ascq. */
static void pedir_sense(atapi_t *a)
{
    uint8_t cdb[12] = { SCSI_REQUEST_SENSE, 0, 0, 0, 18, 0 };
    uint8_t s[18];
    memset(s, 0, sizeof s);
    int r = a->pacote(a, cdb, s, sizeof s, 2000);
    if (r >= 14 && (s[0] & 0x7F) >= 0x70) {
        a->sense = s[2] & 0x0F;
        a->asc = s[12];
        a->ascq = s[13];
    } else {   /* sin sense: lo que dejó el registro de error (sense key en los 4 bits altos) */
        a->sense = a->erro_reg >> 4;
        a->asc = a->ascq = 0xFF;
    }
}

/* Una orden con el sense si falla. Devuelve los bytes recibidos o < 0.
 * "callar" no escribe en el log los sense esperados (preparar el lector). */
static int ordem(atapi_t *a, const uint8_t *cdb, void *buf, uint32_t bytes, uint32_t espera, bool callar)
{
    a->sense = a->asc = a->ascq = 0;
    int r = a->pacote(a, cdb, buf, bytes, espera);
    if (r == ATAPI_CHECK) {
        pedir_sense(a);
        if (!callar)
            klog("[ATAPI] %s: orden %x falló: sense %x (%s), ASC %x ASCQ %x: %s\n", a->onde, cdb[0], a->sense,
                 nome_sense(a->sense), a->asc, a->ascq, nome_asc(a->asc, a->ascq));
    } else if (r < 0) {
        a->sense = 0xFF;
        if (!callar) klog("[ATAPI] %s: orden %x sin respuesta (tiempo agotado o error del controlador)\n", a->onde, cdb[0]);
    }
    return r;
}

/* ------------------------------------------------------------ preparar y capacidad */

/* Lo que dice el lector de su disco (D233). */
enum { UNID_PRONTA, UNID_VAZIA, UNID_PREPARANDO, UNID_ERRO };

/* Un TEST UNIT READY, repetido en el acto lo que haga falta: la "atención de unidad"
 * (sense 6) se consume y se repite; "no hay disco" (2/3Ah) vale solo si sale dos veces
 * seguidas, porque tras meter un disco el QEMU dice una vez "no hay disco" y luego
 * "se ha cambiado el disco", y algunos lectores reales hacen algo parecido al cerrar
 * la bandeja. Un 6/28h ("se ha cambiado el disco") queda en a->mudou. */
static int estado_unidade(atapi_t *a)
{
    uint8_t cdb[12] = { SCSI_TEST_UNIT_READY };
    int vazios = 0;
    for (int t = 0; t < 8; t++) {
        int r = ordem(a, cdb, NULL, 0, 3000, true);
        if (r >= 0) return UNID_PRONTA;
        if (r != ATAPI_CHECK) return UNID_ERRO;
        if (a->sense == 6) {
            if (a->asc == 0x28) a->mudou = true;
            vazios = 0;
            continue;
        }
        if (a->sense == 2 && a->asc == 0x3A) { if (++vazios >= 2) return UNID_VAZIA; continue; }
        if (a->sense == 2 && (a->asc == 0x04 || a->asc == 0x3E)) return UNID_PREPARANDO;
        return UNID_ERRO;
    }
    return UNID_ERRO;
}

/* TEST UNIT READY hasta que el lector está listo o dice que no hay disco (como mucho
 * espera_ms mientras se prepara). Devuelve UNID_*. */
static int preparar(atapi_t *a, uint32_t espera_ms)
{
    prazo_t p = prazo_ms(espera_ms);
    uint8_t ult_asc = 0xFE;
    for (;;) {
        int e = estado_unidade(a);
        if (e == UNID_ERRO) {
            if (a->sense == 0xFF) klog("[ATAPI] %s: TEST UNIT READY sin respuesta\n", a->onde);
            else klog("[ATAPI] %s: no está listo: sense %x, ASC %x ASCQ %x: %s\n", a->onde, a->sense, a->asc, a->ascq,
                      nome_asc(a->asc, a->ascq));
            return e;
        }
        if (e != UNID_PREPARANDO) return e;
        if (a->asc != ult_asc) {   /* cada situación nueva, una vez en el log */
            klog("[ATAPI] %s: aún no está listo: sense %x, ASC %x ASCQ %x: %s\n", a->onde, a->sense, a->asc, a->ascq,
                 nome_asc(a->asc, a->ascq));
            ult_asc = a->asc;
        }
        if (prazo_vencido(&p)) {
            klog("[ATAPI] %s: el lector no está listo tras %u ms\n", a->onde, espera_ms);
            return e;
        }
        pausa_ms(100);
    }
}

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

static bool capacidade(atapi_t *a)
{
    uint8_t cdb[12] = { SCSI_READ_CAPACITY };
    uint8_t c[8];
    memset(c, 0, sizeof c);
    a->blocos = 0;
    for (int t = 0; t < 2; t++) {
        int r = ordem(a, cdb, c, sizeof c, 5000, false);
        if (r >= 8) break;
        if (r != ATAPI_CHECK || a->sense != 6) return false;      /* tras una atención, otra vez */
    }
    uint32_t ultimo = be32(c), tam = be32(c + 4);
    if (tam != BLOCO) {
        /* Algunos lectores dicen 2352 (sector en bruto) con un CD de audio, o 0. */
        klog("[ATAPI] %s: bloques de %u bytes (último %u): no se lee como CD de datos\n", a->onde, tam, ultimo);
        return false;
    }
    if (ultimo >= 0x3FFFFFFFu) return false;   /* la vista de 512 no cabe en 32 bits (no pasa con un DVD) */
    a->blocos = ultimo + 1;
    return true;
}

/* D233: la tabla de contenido. Formato 0 (las pistas): si ninguna es de datos (bit 2
 * del control), es un CD de audio. Formato 1 (sesiones): dónde empieza la última
 * sesión, que es donde están los descriptores del volumen de un CD multisesión. Un
 * DVD responde con una tabla de una pista de datos; si el lector no la da, se sigue
 * como un disco de una sesión. */
static void ler_toc(atapi_t *a)
{
    a->audio = false;
    a->sessao = 0;
    uint8_t *t = a->bloco;
    a->bloco_lba = 0xFFFFFFFFu;   /* se usa el búfer del bloque suelto */
    uint8_t c0[12] = { SCSI_READ_TOC, 0, 0, 0, 0, 0, 1, (uint8_t)(804 >> 8), (uint8_t)(804 & 0xFF), 0, 0, 0 };
    memset(t, 0, 804);
    int r = ordem(a, c0, t, 804, 5000, true);
    if (a->toc) memset(a->toc, 0, sizeof *a->toc);
    if (r >= 12 && a->toc) {
        /* D233: las pistas de audio, con su principio y su duración (cdda_logica.c). */
        cd_toc_t *toc = a->toc;
        cdda_toc_ler(t, (uint32_t)r, toc);
        if (toc->qtd && !toc->dados) {
            a->audio = true;
            klog("[ATAPI] %s: CD de audio (%d pistas, ninguna de datos)\n", a->onde, toc->qtd);
        } else if (toc->qtd)
            klog("[ATAPI] %s: %d pistas de audio y %d de datos (CD mixto)\n", a->onde, toc->qtd, toc->dados);
        if (toc->qtd) {   /* CD-TEXT (formato 5): títulos e intérpretes, si el disco los trae */
            uint8_t *c = kmalloc(CDTEXT_MAX);
            if (c) {
                uint8_t c5[12] = { SCSI_READ_TOC, 0, 5, 0, 0, 0, 0, (uint8_t)(CDTEXT_MAX >> 8), (uint8_t)(CDTEXT_MAX & 0xFF), 0, 0, 0 };
                memset(c, 0, CDTEXT_MAX);
                int k = ordem(a, c5, c, CDTEXT_MAX, 5000, true);
                if (k > 4) {
                    cdda_cdtext_ler(c, (uint32_t)k, toc);
                    if (toc->album[0] || toc->titulo[0][0])
                        klog("[ATAPI] %s: CD-TEXT: \"%s\" de \"%s\"\n", a->onde, toc->album, toc->artista);
                }
                kfree(c);
            }
        }
    }
    uint8_t c1[12] = { SCSI_READ_TOC, 0, 1, 0, 0, 0, 0, 0, 12, 0, 0, 0 };
    memset(t, 0, 12);
    r = ordem(a, c1, t, 12, 5000, true);
    if (r >= 12 && t[3] > 1) {   /* más de una sesión */
        uint32_t lba = be32(t + 8);
        if (lba < 0x3FFFFFFFu && (!a->blocos || lba + 32 < a->blocos)) {
            a->sessao = lba;
            klog("[ATAPI] %s: disco de %u sesiones, la última empieza en el bloque %u\n", a->onde, t[3], lba);
        }
    }
}

/* ------------------------------------------------------------ lectura */

static int ler_blocos(atapi_t *a, uint32_t lba, uint32_t n, void *buf)
{
    uint8_t cdb[12] = { SCSI_READ10, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba,
                        0, (uint8_t)(n >> 8), (uint8_t)n, 0, 0, 0 };
    for (int t = 0; t < 3; t++) {
        int r = ordem(a, cdb, buf, n * BLOCO, ESPERA_ORDEM_MS, false);
        if (r >= (int)(n * BLOCO)) return 0;
        if (r >= 0) {
            klog("[ATAPI] %s: READ(10) del bloque %u: %d bytes de %u\n", a->onde, lba, r, n * BLOCO);
            continue;
        }
        if (r != ATAPI_CHECK) return -1;
        if (a->sense == 6) {           /* disco cambiado o reinicio: el bloque recordado ya no vale */
            a->bloco_lba = 0xFFFFFFFFu;
            if (a->asc == 0x28) a->mudou = true;   /* D233: la tarea de vigilancia lo mira */
            continue;
        }
        if (a->sense == 2 && a->asc == 0x3A) return -1;                     /* sacaron el disco */
        if (a->sense == 2 && a->asc == 0x04) { pausa_ms(500); continue; }   /* aún girando */
        if (a->sense == 3 && t == 0) continue;                               /* error de lectura: otra vez */
        return -1;
    }
    return -1;
}

/* La capa de discos ve el lector en sectores de 512. */
static int cd_ler(disco_t *d, uint32_t lba, uint32_t n, void *buf)
{
    atapi_t *a = d->dono;
    uint8_t *p = buf;
    if (lba >= d->setores || n > d->setores - lba) return -1;   /* D233: sin disco, setores = 0 */
    while (n) {
        uint32_t b = lba / 4, off = lba % 4;
        if (off == 0 && n >= 4) {
            uint32_t k = n / 4;
            if (k > MAX_BLOCOS) k = MAX_BLOCOS;
            if (ler_blocos(a, b, k, p) < 0) return -1;
            lba += k * 4; n -= k * 4; p += k * BLOCO;
            continue;
        }
        if (a->bloco_lba != b) {
            a->bloco_lba = 0xFFFFFFFFu;
            if (ler_blocos(a, b, 1, a->bloco) < 0) return -1;
            a->bloco_lba = b;
        }
        uint32_t q = 4 - off;
        if (q > n) q = n;
        memcpy(p, a->bloco + off * 512, q * 512);
        lba += q; n -= q; p += q * 512;
    }
    return 0;
}

/* D233: n sectores de audio (CD-DA) con READ CD: 2352 bytes de datos de usuario por
 * sector, sin sincronismo, cabecera ni subcanal. Las muestras llegan en el orden del
 * disco: 16 bits little endian, izquierda y derecha, que es justo lo que pide un WAV.
 * 0 = bien, -1 = error de lectura, -2 = no hay disco. */
static int ler_cd_audio(atapi_t *a, uint32_t lba, uint32_t n, void *buf, bool callar)
{
    uint8_t cdb[12] = { SCSI_READ_CD, 1 << 2 /* tipo de sector esperado: CD-DA */,
                        (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba,
                        (uint8_t)(n >> 16), (uint8_t)(n >> 8), (uint8_t)n, 0x10 /* solo los datos de usuario */, 0, 0 };
    for (int t = 0; t < 2; t++) {
        int r = ordem(a, cdb, buf, n * CDDA_SETOR, ESPERA_ORDEM_MS, callar);
        if (r >= (int)(n * CDDA_SETOR)) return 0;
        if (r == ATAPI_CHECK && a->sense == 2 && a->asc == 0x3A) return -2;
        if (r == ATAPI_CHECK && a->sense == 6) { if (a->asc == 0x28) a->mudou = true; continue; }
        if (r == ATAPI_CHECK && a->sense == 2 && a->asc == 0x04) { pausa_ms(500); continue; }
    }
    return -1;
}

/* La lectura del volumen "cdda": nunca corta la pista por una raya. Si la orden de
 * varios sectores falla, se lee sector a sector (dos intentos cada uno) y el que no se
 * puede leer sale en silencio y queda en el log (los 20 primeros de cada disco). */
static int audio_ler(disco_t *d, uint32_t lba, uint32_t n, void *buf)
{
    atapi_t *a = d->dono;
    if (a->medio != 1 /* MEDIO_PRESENTE */ || n == 0 || n > AUDIO_SETORES_ORDEM) return -1;
    int r = ler_cd_audio(a, lba, n, buf, true);
    if (r == 0) return 0;
    if (r == -2) return -1;
    uint8_t *p = buf;
    for (uint32_t i = 0; i < n; i++, p += CDDA_SETOR) {
        r = ler_cd_audio(a, lba + i, 1, p, true);
        if (r == -2) return -1;
        if (r == 0) continue;
        memset(p, 0, CDDA_SETOR);
        if (a->erros_audio < 20)
            klog("[ATAPI] %s: sector de audio %u ilegible (sense %x, ASC %x ASCQ %x): silencio\n", d->nome, lba + i,
                 a->sense, a->asc, a->ascq);
        else if (a->erros_audio == 20)
            klog("[ATAPI] %s: más sectores de audio ilegibles: ya no se anotan\n", d->nome);
        if (a->erros_audio < 0xFFFF) a->erros_audio++;
    }
    return 0;
}

/* La imagen El Torito: sus sectores son los del CD a partir del inicio de la imagen.
 * dono = el atapi_t; el inicio está en a->imagem_inicio. */
typedef struct { atapi_t *a; uint32_t inicio; } imagem_t;

static int imagem_ler(disco_t *d, uint32_t lba, uint32_t n, void *buf)
{
    imagem_t *im = d->dono;
    if (!im->a->disco) return -1;
    return disco_ler(im->a->disco, im->inicio + lba, n, buf) < 0 ? -1 : 0;
}

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static int cd_expulsar(disco_t *d, bool fechar);

/* Busca la imagen El Torito y la registra como "cdNimg". El tamaño sale de la tabla de
 * particiones de su MBR (el final de la última partición), sin pasar del final del CD. */
static void detectar_imagem(atapi_t *a)
{
    disco_t *cd = a->disco;
    uint32_t ini;
    if (!cd || !cd->setores || !iso_el_torito(cd, &ini) || ini >= cd->setores) return;
    uint8_t *s = kmalloc(512);
    if (!s) return;
    uint32_t max = cd->setores - ini, tam = 0;
    if (disco_ler(cd, ini, 1, s) == 0 && s[510] == 0x55 && s[511] == 0xAA) {
        for (int i = 0; i < 4; i++) {
            const uint8_t *e = s + 446 + i * 16;
            uint32_t pi = le32(e + 8), pn = le32(e + 12);
            if (e[4] && pn && pi < max && pn <= max - pi && pi + pn > tam) tam = pi + pn;
        }
    }
    kfree(s);
    if (!tam) {
        klog("[ATAPI] %s: imagen El Torito en el sector %u sin MBR válido: no se usa\n", cd->nome, ini);
        return;
    }
    imagem_t *im = kmalloc(sizeof *im);
    if (!im) return;
    im->a = a;
    im->inicio = ini;
    klog("[ATAPI] %s: imagen de arranque El Torito (emulación de disco duro) en el bloque %u, %u sectores de 512\n",
         cd->nome, ini / 4, tam);
    a->imagem = disco_registrar_imagem_cd(cd, tam, im, imagem_ler);
    if (!a->imagem) { kfree(im); return; }
    a->imagem->expulsar = cd_expulsar;
    if (a->imagem->arranque_real) klog("[ATAPI] %s: es la imagen de la que ha arrancado SaguiOS\n", a->imagem->nome);
}

/* La imagen El Torito de antes ya no vale (otro disco, o ninguno): desaparece, salvo
 * que un programa la tenga reservada (el instalador copiando de ella: sus lecturas
 * fallarán y él lo dirá). */
static void soltar_imagem(atapi_t *a, const char *porque)
{
    if (!a->imagem) return;
    if (disco_reservado(a->imagem)) {
        klog("[ATAPI] %s: %s, pero %s está reservado: se queda\n", a->disco->nome, porque, a->imagem->nome);
        return;
    }
    imagem_t *im = a->imagem->dono;
    klog("[ATAPI] %s: %s, %s desaparece\n", a->disco->nome, porque, a->imagem->nome);
    disco_removido(a->imagem);   /* desmonta sin grabar lo que hubiera de ella */
    kfree(im);
    a->imagem = NULL;
}

/* ------------------------------------------------------------ meter y sacar el disco (D233) */

enum { MEDIO_AUSENTE, MEDIO_PRESENTE };

/* Las pistas de audio como el volumen "cdNaudio" (sistema de archivos cdda). */
static void registrar_audio(atapi_t *a)
{
    cd_toc_t *toc = a->toc;
    if (a->disco_audio || !toc || !toc->qtd) return;
    uint32_t s = cdda_segundos(toc), setores = 0;
    for (int i = 0; i < toc->qtd; i++) setores += cdda_tamanho(toc->pista[i].setores) / 512;
    char formato[40];
    snformatar(formato, sizeof formato, "Audio: %d pistas, %u:%02u", toc->qtd, s / 60, s % 60);
    a->erros_audio = 0;
    a->disco_audio = disco_registrar_audio_cd(a->disco, setores, a, toc, audio_ler,
                                              toc->album[0] ? toc->album : "CD de audio", formato);
    if (a->disco_audio) a->disco_audio->expulsar = cd_expulsar;
}

static void soltar_audio(atapi_t *a)
{
    if (!a->disco_audio) return;
    disco_removido(a->disco_audio);   /* desmonta lo que hubiera (no hay nada que grabar) */
    a->disco_audio = NULL;
}

/* Una suma de lo que identifica el disco: capacidad, sesión, el bloque 16 de la
 * sesión (el descriptor primario del ISO 9660, con su fecha) y, si hay UDF, el primer
 * descriptor de su secuencia (el primario, con la fecha de grabación). */
static uint32_t calcular_assinatura(atapi_t *a)
{
    uint32_t h = a->blocos * 31u + a->sessao;
    if (a->toc)   /* D233: las pistas de audio (un CD de audio no tiene descriptores) */
        for (int i = 0; i < a->toc->qtd; i++) h = h * 16777619u ^ a->toc->pista[i].lba ^ a->toc->pista[i].setores << 7;
    if (!a->blocos) return h;
    uint32_t sitios[2] = { a->sessao + 16, 0 };
    if (ler_blocos(a, a->sessao + 256 < a->blocos ? a->sessao + 256 : 0, 1, a->bloco) == 0 &&
        a->bloco[0] == 2 && a->bloco[1] == 0)   /* ancla del UDF (etiqueta 2) */
        sitios[1] = le32(a->bloco + 20);
    a->bloco_lba = 0xFFFFFFFFu;
    for (int i = 0; i < 2; i++) {
        if (i == 1 && !sitios[1]) break;
        if (sitios[i] >= a->blocos || ler_blocos(a, sitios[i], 1, a->bloco) < 0) continue;
        for (uint32_t k = 0; k < BLOCO; k++) h = h * 16777619u ^ a->bloco[k];
    }
    a->bloco_lba = 0xFFFFFFFFu;
    return h;
}

/* Lo que hay en el disco recién metido (o al arrancar): capacidad, tabla de
 * contenido, sistema de archivos y la imagen El Torito. */
static void ler_medio(atapi_t *a)
{
    disco_t *d = a->disco;
    a->bloco_lba = 0xFFFFFFFFu;
    a->mudou = false;
    bool cap = capacidade(a);
    ler_toc(a);
    if (a->audio) a->blocos = 0;   /* no hay nada que leer como datos */
    else if (!cap) a->blocos = 0;
    a->assinatura = calcular_assinatura(a);
    d->setores = a->blocos * 4;
    d->sessao = a->sessao;
    d->cd_audio = a->audio;
    disco_reconhecer_cd(d);
    a->medio = MEDIO_PRESENTE;
    uint32_t ini = 0;
    bool tem = a->blocos && iso_el_torito(d, &ini);
    if (a->imagem) {
        imagem_t *im = a->imagem->dono;
        if (!tem || im->inicio != ini) soltar_imagem(a, "el disco ha cambiado");
    }
    if (!a->imagem && tem) detectar_imagem(a);
    soltar_audio(a);
    registrar_audio(a);
    klog("[ATAPI] %s: medio: %u bloques de 2048, %s \"%s\"\n", d->nome, a->blocos, d->formato, d->rotulo);
}

/* El disco salió (o se va a sacar): se desmonta lo suyo, sin grabar (no hay nada
 * que grabar en un CD), y el lector queda "sin disco". */
static void medio_saiu(atapi_t *a, bool avisar)
{
    disco_t *d = a->disco;
    bool estava = d->montagem != NULL;
    char ponto[24] = "";
    if (d->montagem) {
        txt_copia_n(ponto, d->montagem->ponto, sizeof ponto);
        montagem_desmontar(d->montagem, false);
    }
    soltar_imagem(a, "no hay disco");
    if (a->disco_audio && a->disco_audio->montagem) {
        estava = true;
        txt_copia_n(ponto, a->disco_audio->montagem->ponto, sizeof ponto);
    }
    soltar_audio(a);
    a->blocos = 0;
    a->sessao = 0;
    a->audio = false;
    a->mudou = false;
    a->bloco_lba = 0xFFFFFFFFu;
    a->medio = MEDIO_AUSENTE;
    d->setores = 0;
    d->sessao = 0;
    d->cd_audio = false;
    disco_reconhecer_cd(d);
    klog("[ATAPI] %s: sin disco%s%s\n", d->nome, estava ? ", desmontado de " : "", ponto);
    if (avisar) {
        if (estava) disco_avisar(AVISO_REMOVIDO, "CD/DVD retirado: %s desmontado.", ponto);
        else disco_avisar(AVISO_REMOVIDO, "CD/DVD retirado.");
    }
}

/* Monta el volumen v (los datos del CD, o sus pistas de audio) en "preferido" o, si
 * está ocupado, en /<nombre> (/cd1, /cd0audio). */
static int automontar_vol(disco_t *v, const char *preferido)
{
    if (!v || !v->tipo_fs[0] || disco_montado(v) || disco_reservado(v)) return ERRO_FORMATO;
    char ponto[16];
    int r = montagem_montar(v, preferido, "auto", MONTAR_SO_LEITURA);
    if (r == ERRO_OCUPADO) {
        snformatar(ponto, sizeof ponto, "/%s", v->nome);
        r = montagem_montar(v, ponto, "auto", MONTAR_SO_LEITURA);
    }
    klog("[ATAPI] %s: montaje automático: %d\n", v->nome, r);
    return r;
}

/* Los datos en /cd; las pistas de audio, en /cd si es un CD de audio y en /cdaudio si el
 * disco es mixto (un montaje no puede ir dentro de otro). */
static int automontar(atapi_t *a)
{
    int r = automontar_vol(a->disco, "/cd");
    if (a->disco_audio) {
        int k = automontar_vol(a->disco_audio, a->disco->tipo_fs[0] ? "/cdaudio" : "/cd");
        if (!a->disco->tipo_fs[0]) r = k;
    }
    return r;
}

/* Disco nuevo: se lee, se avisa y, si es de datos o de audio, se monta solo. */
static void medio_entrou(atapi_t *a, bool montar, bool avisar)
{
    disco_t *d = a->disco, *au = NULL;
    ler_medio(a);
    au = a->disco_audio;
    if (!d->tipo_fs[0] && !au) {
        if (avisar) disco_avisar(AVISO_INFO, "CD/DVD en %s: %s.", d->nome, d->formato);
        return;
    }
    int r = montar ? automontar(a) : ERRO_FORMATO;
    if (!avisar) return;
    if (d->tipo_fs[0]) {
        if (r == 0 && d->montagem)
            disco_avisar(AVISO_MONTADO, "CD/DVD \"%s\" montado en %s (%s).", d->rotulo[0] ? d->rotulo : d->nome,
                         d->montagem->ponto, d->formato);
        else disco_avisar(AVISO_CONECTADO, "CD/DVD \"%s\" en %s (%s).", d->rotulo[0] ? d->rotulo : d->nome, d->nome, d->formato);
    }
    if (au) {
        uint32_t s = cdda_segundos(a->toc);
        if (au->montagem) {
            char lista[64];
            snformatar(lista, sizeof lista, "%s/Lista.m3u", au->montagem->ponto);
            disco_avisar_abrir(AVISO_AUDIO, lista, "CD de audio \"%s\" en %s: %d pistas, %u:%02u. Pulsa para escucharlo.",
                               au->rotulo, au->montagem->ponto, a->toc->qtd, s / 60, s % 60);
        } else
            disco_avisar(AVISO_CONECTADO, "CD de audio en %s: %d pistas, %u:%02u.", au->nome, a->toc->qtd, s / 60, s % 60);
    }
}

/* Un sondeo de la tarea de vigilancia: con trava_fs tomada y las interrupciones
 * encendidas (el TEST UNIT READY las enciende mientras espera). Lo que cambia (leer
 * el disco, montar, desmontar) se hace con ellas apagadas, como la conexión de un
 * pendrive (usb.c): el árbol de carpetas y la tabla de montajes no esperan que otro
 * proceso entre a la vez. */
static void vigiar(atapi_t *a)
{
    int e = estado_unidade(a);
    if (e == UNID_ERRO && a->sense == 0xFF) {   /* sin respuesta: se espacia y se avisa una vez */
        if (a->falhas < 255) a->falhas++;
        if (a->falhas == 3) klog("[ATAPI] %s: el lector no responde; se mirará cada 30 s\n", a->onde);
        return;
    }
    if (a->falhas >= 3) klog("[ATAPI] %s: el lector vuelve a responder\n", a->onde);
    a->falhas = 0;
    bool i0 = int_salva_desliga();
    if (e == UNID_PRONTA) {
        if (a->medio != MEDIO_PRESENTE) {
            klog("[ATAPI] %s: se ha metido un disco\n", a->disco->nome);
            medio_entrou(a, true, true);
        } else if (a->mudou) {
            uint32_t antes = a->assinatura;
            uint32_t blocos_antes = a->blocos;
            a->mudou = false;
            bool cap = capacidade(a);
            ler_toc(a);
            if (a->audio || !cap) a->blocos = 0;
            if (a->blocos == blocos_antes && calcular_assinatura(a) == antes) {
                klog("[ATAPI] %s: el lector avisó de un cambio, pero es el mismo disco\n", a->disco->nome);
                int_restaura(i0);
                return;
            }
            klog("[ATAPI] %s: se ha cambiado el disco\n", a->disco->nome);
            medio_saiu(a, false);
            medio_entrou(a, true, true);
        }
    } else if (e == UNID_VAZIA && a->medio == MEDIO_PRESENTE) {
        klog("[ATAPI] %s: se ha sacado el disco\n", a->disco->nome);
        medio_saiu(a, true);
    }
    /* UNID_PREPARANDO: el lector aún lee el disco; se verá en el próximo sondeo. */
    int_restaura(i0);
}

/* START STOP UNIT con LoEj: abre (o cierra) la bandeja. Antes se desmonta lo del CD. */
static int cd_expulsar(disco_t *d, bool fechar)
{
    atapi_t *a = d->imagem_cd ? ((imagem_t *)d->dono)->a : d->dono;   /* cd0, cd0img o cd0audio */
    disco_t *cd = a->disco;
    if (!fechar) {
        if (disco_reservado(cd) || (a->imagem && disco_reservado(a->imagem))) return ERRO_OCUPADO;
        int r = disco_desmontar(cd);
        if (r == 0 && a->imagem) r = disco_desmontar(a->imagem);
        if (r == 0 && a->disco_audio) r = disco_desmontar(a->disco_audio);   /* las pistas de audio */
        if (r < 0) return r;
    }
    /* Si la BIOS u otro sistema bloqueó la bandeja (PREVENT), se desbloquea. */
    uint8_t permitir[12] = { 0x1E, 0, 0, 0, 0, 0 };
    ordem(a, permitir, NULL, 0, 3000, true);
    uint8_t cdb[12] = { SCSI_START_STOP, 0, 0, 0, (uint8_t)(fechar ? 3 : 2), 0 };
    int r = ordem(a, cdb, NULL, 0, ESPERA_ORDEM_MS, false);
    klog("[ATAPI] %s: %s la bandeja: %s\n", cd->nome, fechar ? "cerrar" : "abrir", r >= 0 ? "hecho" : "el lector no ha podido");
    if (!fechar && a->medio == MEDIO_PRESENTE) medio_saiu(a, false);
    if (r < 0) return fechar ? ERRO_NAO_SUPORTADO : ERRO_ACESSO;   /* muchos portátiles no cierran solos */
    a->proxima = relogio_tiques() + TIQUES_POR_SEGUNDO;           /* mirar pronto qué ha pasado */
    return 0;
}

/* ------------------------------------------------------------ registro */

static void copiar_texto_ata(char *dest, const uint16_t *id, int ini, int palavras, int max)
{
    int k = 0;
    for (int i = 0; i < palavras && k + 2 < max; i++) {
        dest[k++] = (char)(id[ini + i] >> 8);
        dest[k++] = (char)(id[ini + i] & 0xFF);
    }
    while (k > 0 && (dest[k - 1] == ' ' || dest[k - 1] == 0)) k--;
    dest[k] = 0;
}

atapi_t *atapi_registrar(const atapi_t *modelo, const uint16_t *id)
{
    uint8_t tipo = (uint8_t)((id[0] >> 8) & 0x1F);
    char nome[48];
    copiar_texto_ata(nome, id, 27, 20, sizeof nome);
    klog("[ATAPI] %s: \"%s\", tipo %u%s, paquetes de %u bytes, DMA %s\n", modelo->onde, nome, tipo,
         tipo == 5 ? " (CD/DVD)" : "", (id[0] & 3) == 1 ? 16 : 12, (id[49] & (1 << 8)) ? "sí" : "no");
    if (tipo != 5) {
        klog("[ATAPI] %s: no es un lector de CD/DVD (tipo %u): ignorado\n", modelo->onde, tipo);
        return NULL;
    }
    if (qtd_leitores >= MAX_LEITORES) return NULL;
    atapi_t *a = kmalloc(sizeof *a);
    uint8_t *b = kmalloc(BLOCO);
    if (!a || !b) { if (a) kfree(a); if (b) kfree(b); return NULL; }
    *a = *modelo;
    memcpy(a->modelo, nome, sizeof a->modelo);
    a->bloco = b;
    a->bloco_lba = 0xFFFFFFFFu;
    a->blocos = 0;
    a->disco = a->imagem = NULL;
    a->medio = MEDIO_AUSENTE;
    a->mudou = a->audio = false;
    a->sessao = a->assinatura = 0;
    a->falhas = 0;
    a->proxima = 0;
    a->toc = kcalloc(sizeof *a->toc);   /* D233: si no hay memoria, sin pistas de audio */
    a->disco_audio = NULL;
    a->erros_audio = 0;
    /* Un registro "sin disco" primero; si hay disco, se lee como uno recién metido. */
    int e = preparar(a, ESPERA_PRONTO_MS);
    a->disco = disco_registrar_cd("ATAPI", a->modelo, 0, a, cd_ler);
    if (!a->disco) { kfree(b); kfree(a); return NULL; }
    a->disco->sondar = atapi_sondar;
    a->disco->expulsar = cd_expulsar;
    leitores[qtd_leitores++] = a;
    if (e == UNID_PRONTA) {
        ler_medio(a);
        if (a->blocos) klog("[ATAPI] %s: disco de %u bloques de 2048 (%u MB)\n", a->onde, a->blocos, a->blocos / 512);
    } else
        klog("[ATAPI] %s: sin disco legible (sense %x, ASC %x ASCQ %x)\n", a->onde, a->sense, a->asc, a->ascq);
    klog("[ATAPI] %s es %s: %s \"%s\"\n", a->onde, a->disco->nome, a->disco->formato, a->disco->rotulo);
    return a;
}

/* Antes de montar a mano: si la tarea de vigilancia aún no ha visto el disco (se
 * acaba de meter), se espera a que el lector esté listo y se lee. */
bool atapi_sondar(disco_t *d)
{
    atapi_t *a = d->dono;
    if (!d->cd || disco_montado(d)) return true;
    int e = preparar(a, ESPERA_MONTAR_MS);
    if (e == UNID_PRONTA) {
        if (a->medio != MEDIO_PRESENTE || a->mudou) {
            if (a->medio == MEDIO_PRESENTE) medio_saiu(a, false);
            medio_entrou(a, false, false);
        }
    } else if (e == UNID_VAZIA && a->medio == MEDIO_PRESENTE) {
        medio_saiu(a, true);
    }
    return a->blocos != 0;
}

int atapi_quantidade(void) { return qtd_leitores; }

bool atapi_montar_cd_arranque(void)
{
    if (!(info_boot.opcoes & OPC_BOOT_CD)) return false;
    for (int i = 0; i < qtd_leitores; i++) {
        atapi_t *a = leitores[i];
        if (!a->imagem || !a->imagem->arranque_real || !a->disco->tipo_fs[0]) continue;
        int r = montagem_montar(a->disco, "/cd", "iso9660", MONTAR_SO_LEITURA);
        klog("[ATAPI] %s: el CD de arranque se monta en /cd: %d\n", a->disco->nome, r);
        return r == 0;
    }
    klog("[ATAPI] se arrancó de un CD, pero no se ha encontrado ese CD en ningún lector%s\n",
         qtd_leitores ? " (los lectores vistos tienen otro disco o ninguno)" : " (ningún lector ATAPI visto: mira las líneas [ATA]/[AHCI])");
    return false;
}

/* ------------------------------------------------------------ vigilancia (D233) */

static void tarefa_cd(void *arg)
{
    (void)arg;
    static const char canal = 0;
    for (;;) {
        uint32_t agora = relogio_tiques();
        for (int i = 0; i < qtd_leitores; i++) {
            atapi_t *a = leitores[i];
            if ((int32_t)(agora - a->proxima) < 0) continue;
            /* Si otro proceso está en un sistema de archivos (o leyendo este CD), se
             * prueba en la próxima vuelta: nunca se espera por el cerrojo. */
            if (!trava_tentar(&trava_fs)) continue;
            vigiar(a);
            trava_soltar(&trava_fs);
            uint32_t periodo = a->falhas >= 3 ? 30 * TIQUES_POR_SEGUNDO : PERIODO_SONDA_MS * TIQUES_POR_SEGUNDO / 1000;
            a->proxima = relogio_tiques() + periodo;
        }
        processo_atual()->acordar_em = relogio_tiques() + TIQUES_POR_SEGUNDO / 2;
        int_desliga();
        bloquear(&canal);
        ceder();
        int_liga();
    }
}

void atapi_iniciar_vigilancia(void)
{
    if (!qtd_leitores) return;
    /* Un CD de datos que ya estaba en el lector (y no es el de arranque, ya montado). */
    for (int i = 0; i < qtd_leitores; i++) {
        atapi_t *a = leitores[i];
        if (a->medio == MEDIO_PRESENTE && (a->disco->tipo_fs[0] || a->disco_audio) && !disco_montado(a->disco)) automontar(a);
        a->proxima = relogio_tiques() + PERIODO_SONDA_MS * TIQUES_POR_SEGUNDO / 1000;
    }
    tarefa_kernel("cdrom", tarefa_cd, NULL);
    klog("[ATAPI] vigilancia de %d lector(es): disco metido o sacado cada %u ms\n", qtd_leitores, PERIODO_SONDA_MS);
}
