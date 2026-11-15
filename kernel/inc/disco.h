// SPDX-License-Identifier: GPL-3.0-only
/* disco.h - discos y particiones vistos por el sistema de archivos (fase 6, D040).
 *
 * Un disco_t es un "dispositivo de bloques": un disco entero (pendrive USB
 * "usb0", disco ATA "hda") o una partición suya ("usb0p1", "hda1"). Las
 * particiones leen y graban por el disco padre, sumando el desplazamiento; quien usa
 * llama siempre a disco_ler/disco_escrever, con sectores relativos al inicio. */
#ifndef SAGUI_DISCO_H
#define SAGUI_DISCO_H
#include "tipos.h"

#define MAX_DISCOS 32
#define MAX_PRIMARIAS 4          /* las cuatro del MBR o las cuatro primeras de la GPT */
#define MAX_LOGICAS   16         /* lógicas de una partición extendida (D120) */
#define MAX_PARTICOES (MAX_PRIMARIAS + MAX_LOGICAS)   /* por disco */

struct montagem;

typedef struct disco {
    bool     usado;
    bool     presente;            /* false: fue retirado (el registro desaparece al desmontar) */
    char     nome[12];            /* "usb0", "usb0p1", "hda", "hda1" */
    char     fabricante[32], produto[48];
    uint32_t setores, tam_setor;
    void    *dono;
    /* Solo en el disco entero: el driver. Las particiones usan las del padre. */
    int    (*ler)(struct disco *d, uint32_t lba, uint32_t n, void *buf);
    int    (*escrever)(struct disco *d, uint32_t lba, uint32_t n, const void *buf);
    struct disco *pai;            /* NULL = disco entero */
    int      particao;            /* 0 = disco entero; 1..4 primarias, 5... lógicas */
    uint32_t inicio;              /* primer sector dentro del padre */
    uint8_t  tipo_mbr;            /* tipo de la partición en el MBR (0xEE = GPT) */
    bool     removivel;           /* pendrive USB: avisos e icono */
    bool     interno;             /* disco ATA: montado solo para lectura salvo "-o rw" (D040) */
    /* Rellenados por discos.c al reconocer el formato: */
    char     formato[40];         /* "FAT32", "ext2", "exFAT: formato no soportado"... */
    char     tipo_fs[8];          /* "fat32", "ext2", "ext3", "ext4" o "" (no monta) */
    char     rotulo[20];
    bool     de_boot;             /* disco de arranque de SaguiOS: sin aviso ni icono */
    struct disco *volume;         /* solo en el disco entero: lo que "usb0" monta (primer volumen válido) */
    struct montagem *montagem;    /* donde está montado este volumen (NULL = no está) */
    bool     rw_autorizado;       /* montado (o montándose) para escritura: el único caso en que se graba */
    bool     pai_gravando;        /* una partición montada para escritura está grabando por este disco */
    /* D179: TRIM, disco de arranque de verdad y reserva para el acceso en bruto (solo disco entero). */
    int    (*descartar)(struct disco *d, uint32_t lba, uint32_t n);   /* NULL = el driver no manda TRIM */
    uint8_t  trim;                /* TRIM_NAO, TRIM_SIM o TRIM_SEM_DRIVER (abi.h) */
    bool     arranque_real;       /* su LBA 1 es la cabecera con la que arrancó esta vez */
    uint32_t cab_lba;             /* D270: dónde está esa cabecera: 1, o el primer sector de la partición GPT de SaguiOS */
    bool     nvme;                /* D265: disco NVMe */
    uint16_t bloco_fisico;        /* D265: bloque real del namespace NVMe (512 o 4096); 0 = no se sabe */
    bool     gpt;                 /* la tabla es GPT: no se toca */
    int      reservado_pid;       /* proceso con el acceso en bruto (0 = nadie) */
    uint32_t reservado_flags;     /* DBLOCO_CONF_* con que se reservó */
    /* D207: CD/DVD. El lector ("cd0") se ve en sectores de 512 (atapi.c los lee de 2048);
     * su imagen de arranque El Torito ("cd0img") es otro disco entero, también de solo lectura. */
    bool     cd;                  /* lector de CD/DVD */
    bool     imagem_cd;           /* imagen El Torito dentro de un CD */
    uint8_t  usb;                 /* D214: DISCO_XHCI | DISCO_USB3 >> 8 (cómo está conectado el pendrive) */
    bool   (*sondar)(struct disco *d);   /* vuelve a mirar el medio (NULL = no hace falta) */
    /* D233: disco metido o sacado en caliente, UDF, multisesión y expulsar. */
    int    (*expulsar)(struct disco *d, bool fechar);   /* START STOP UNIT (NULL = no se puede) */
    uint32_t sessao;              /* bloque de 2048 donde empieza la última sesión (READ TOC) */
    bool     cd_audio;            /* CD de audio (ninguna pista de datos): no se monta */
    bool     tem_iso, tem_udf;    /* qué sistemas de archivos lleva el CD (UDF bridge: los dos) */
    /* D233: las pistas de audio de un CD ("cd0audio", sistema de archivos "cdda"). */
    bool     audio_cd;
    struct cd_toc *toc;           /* la tabla de contenido y el CD-TEXT (del lector) */
    int    (*ler_audio)(struct disco *d, uint32_t lba, uint32_t n, void *buf);   /* n sectores de 2352 */
} disco_t;

disco_t *disco_registrar(const char *prefixo, const char *fabricante, const char *produto, uint32_t setores,
                         uint32_t tam_setor, void *dono, bool removivel,
                         int (*ler)(disco_t *, uint32_t, uint32_t, void *),
                         int (*escrever)(disco_t *, uint32_t, uint32_t, const void *));
/* D265: namespace NVMe: "nvme0", "nvme1"...; bloco_fisico = 512 o 4096 (las tablas de particiones de
 * un disco de 4096 cuentan en bloques de 4096). */
disco_t *disco_registrar_nvme(const char *produto, uint32_t setores, uint16_t bloco_fisico, void *dono,
                              int (*ler)(disco_t *, uint32_t, uint32_t, void *),
                              int (*escrever)(disco_t *, uint32_t, uint32_t, const void *));
void     disco_removido(disco_t *d);
/* D207: un lector de CD/DVD ("cd0", "cd1"...) y la imagen El Torito de su disco ("cd0img").
 * Los dos son de solo lectura (sin función de escritura). */
disco_t *disco_registrar_cd(const char *fabricante, const char *produto, uint32_t setores, void *dono,
                            int (*ler)(disco_t *, uint32_t, uint32_t, void *));
disco_t *disco_registrar_imagem_cd(disco_t *cd, uint32_t setores, void *dono,
                                   int (*ler)(disco_t *, uint32_t, uint32_t, void *));
/* D233: las pistas de audio del CD ("cd0audio"): un volumen "cdda" sin sectores de 512. */
disco_t *disco_registrar_audio_cd(disco_t *cd, uint32_t setores, void *dono, struct cd_toc *toc,
                                  int (*ler_audio)(disco_t *, uint32_t, uint32_t, void *),
                                  const char *rotulo, const char *formato);
void     disco_reconhecer_cd(disco_t *d);         /* D207: ISO 9660 o no (tras cambiar el disco) */
disco_t *disco_por_nome(const char *nome);
disco_t *disco_resolver(const char *nome);        /* nombre, "LABEL=etiqueta" o "arranque" */
disco_t *disco_indice(int i);
disco_t *disco_raiz(disco_t *d);                  /* el disco entero de una partición */
int      disco_ler(disco_t *d, uint32_t lba, uint32_t n, void *buf);
int      disco_escrever(disco_t *d, uint32_t lba, uint32_t n, const void *buf);
int      disco_sincronizar(disco_t *d);           /* D281: vacía la caché de bloques (NULL = todos) */
bool     disco_montado(disco_t *d);               /* él o alguna partición suya */
disco_t *disco_pendrive_montado(void);            /* primer pendrive con algo montado */
void     disco_reler(disco_t *d);                  /* D179: vuelve a leer la tabla (nada montado) */
disco_t *disco_arranque_real(void);               /* D179: el disco del que arrancó (NULL = no se sabe) */
bool     disco_reservado(disco_t *d);              /* D179: alguien vivo lo tiene reservado */
bool     disco_pode_ser_arranque(disco_t *d);      /* D179: el de arranque, o uno que podría serlo */
/* D213: reserva con particiones montadas. Protegida = montada, o la de SaguiOS (0xDA) de un
 * disco que puede ser el de arranque. */
bool     disco_particao_protegida(disco_t *p);
/* ¿Algún sector de [lba, lba+n) del disco entero cae dentro de una partición protegida? */
disco_t *disco_toca_protegida(disco_t *d, uint32_t lba, uint32_t n);
/* ¿La tabla (MBR y cadena de EBR) que quedaría al escribir n sectores desde lba con el
 * contenido k conserva todas las particiones protegidas igual (inicio, tamaño y tipo)? */
bool     disco_tabela_conserva(disco_t *d, uint32_t lba, uint32_t n, const uint8_t *k);

/* Montar por el nombre antiguo ("usb0" en /usb) y desmontar todo de un disco. */
int      disco_montar(disco_t *d);
int      disco_desmontar(disco_t *d);

/* Avisos para el escritorio (H34). */
#define AVISO_CONECTADO   1
#define AVISO_REMOVIDO    2   /* retirado sin desmontar */
#define AVISO_DESMONTADO  3   /* se puede retirar */
#define AVISO_MONTADO     4
#define AVISO_INFO        5
#define AVISO_AUDIO       6   /* D233: CD de audio; al pulsar el aviso se reproduce "caminho" */
bool disco_proximo_aviso(int *tipo, char *texto, int max, char *caminho, int max_caminho);
void disco_avisar(int tipo, const char *fmt, ...);
void disco_avisar_abrir(int tipo, const char *caminho, const char *fmt, ...);   /* D233: aviso pulsable */

#endif
