// SPDX-License-Identifier: GPL-2.0-or-later
/* acpi_pss.h - lector mínimo de _PSS, _PSD y _PCT de las tablas ACPI (D177).
 *
 * Solo lógica, sin hardware: recibe las direcciones de la DSDT y de las SSDT
 * y una función que dice si una dirección física se puede leer; recorre el
 * AML sin ejecutarlo (no hay intérprete) y devuelve, para cada procesador
 * (Processor o Device ACPI0007), los estados de rendimiento que declara la
 * BIOS. Las pruebas compilan el mismo archivo en el anfitrión
 * (tests/h147_eist_pss.py, -DSAGUI_HOST) con tablas de máquinas reales.
 *
 * No escribe nada en ningún sitio: la fase C del EIST antiguo (D177) usará
 * estos valores, y solo los que acpi_pss_validar() dé por fiables. */
#ifndef SAGUI_ACPI_PSS_H
#define SAGUI_ACPI_PSS_H
#ifdef SAGUI_HOST
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#else
#include "tipos.h"
#endif

#define PSS_MAX_CPUS      8
#define PSS_MAX_ESTADOS  16
#define PSS_MAX_TABELAS  24    /* DSDT, SSDT de la RSDT y las cargadas con Load() */

/* Un estado de _PSS, tal como lo escribe la BIOS. */
typedef struct {
    uint32_t mhz;         /* CoreFrequency */
    uint32_t mw;          /* Power */
    uint32_t lat_us;      /* TransitionLatency */
    uint32_t lat_bm_us;   /* BusMasterLatency */
    uint32_t controle;    /* Control: lo que se escribe (FFixedHW: IA32_PERF_CTL) */
    uint32_t status;      /* Status: lo que se compara con IA32_PERF_STATUS */
} pss_estado_t;

/* De dónde sale el _PSS de un procesador. */
#define PSS_SEM            0   /* no hay _PSS */
#define PSS_NOME           1   /* Name(_PSS, Package(){...}) */
#define PSS_METODO         2   /* Method(_PSS) que devuelve un paquete (Return(NPSS), Return(\_PR.CPU0._PSS)...) */
#define PSS_ILEGIVEL       3   /* hay _PSS pero no se ha podido leer (calculado, referencias, AML raro) */

/* _PSD, tipo de coordinación (ACPI 6.x, 8.4.5.5). */
#define PSD_SW_ALL 0xFC
#define PSD_SW_ANY 0xFD
#define PSD_HW_ALL 0xFE
/* Máscara psd_tipos. */
#define PSD_M_SW_ALL 1
#define PSD_M_SW_ANY 2
#define PSD_M_HW_ALL 4
#define PSD_M_OUTRO  8

/* Máscara pct_espacos: dónde dice el _PCT que se escribe. */
#define PCT_M_FFH    1     /* Functional Fixed Hardware (0x7F): el MSR IA32_PERF_CTL */
#define PCT_M_IO     2     /* SystemIO: un puerto (el SMI de la BIOS) */
#define PCT_M_OUTRO  4

typedef struct {
    char     caminho[40];        /* "\_PR_.CPU0" */
    uint8_t  tipo;               /* 1 = Processor(), 2 = Device ACPI0007 */
    uint8_t  id;                 /* ProcessorID de Processor() o _UID del Device */
    /* _PSS */
    uint8_t  pss_origem;         /* PSS_* */
    uint8_t  pss_variantes;      /* paquetes distintos que podía devolver el _PSS */
    uint8_t  pss_dinamica;       /* sale de una SSDT cargada con Load() */
    uint8_t  pss_incerto;        /* el método tiene algún Return que no se entiende */
    char     pss_de[40];         /* el objeto leído: "\_PR_.CPU0._PSS", "\_PR_.CPU0.NPSS" */
    char     pss_tabela[9];      /* OEM Table ID de la tabla de donde sale ("Cpu0Ist") */
    uint8_t  qtd;
    pss_estado_t est[PSS_MAX_ESTADOS];
    /* _PSD */
    uint8_t  psd_variantes;      /* 0 = sin _PSD legible */
    uint8_t  psd_tipos;          /* PSD_M_*: todos los tipos que puede devolver */
    uint8_t  psd_dominios_iguais;/* todas las variantes dicen el mismo dominio */
    uint8_t  psd_ilegivel;
    uint32_t psd_dominio, psd_tipo, psd_num;   /* la primera variante */
    /* _PCT */
    uint8_t  pct_variantes;
    uint8_t  pct_espacos;        /* PCT_M_* de todas las variantes (control) */
    /* acpi_pss_validar */
    uint8_t  validado;
    uint8_t  ausente;            /* su id no está entre los procesadores encendidos de la MADT */
    uint8_t  turbo;              /* el estado 0 es IDA (Core 2: P1 + 1 MHz), no se pide directamente */
    uint8_t  slfm;               /* el último estado lleva el bit 15 (SuperLFM: bus a la mitad) */
    uint8_t  fiavel;
    char     motivo[96];
} pss_cpu_t;

typedef struct {
    uint32_t tabelas;            /* tablas leídas */
    uint32_t dinamicas;          /* SSDT cargadas con Load() encontradas por su dirección */
    uint32_t rejeitadas;         /* direcciones de SSDT que no se pudieron leer o no eran válidas */
    uint32_t objetos;            /* objetos AML anotados */
    uint32_t objetos_perdidos;   /* no cabían en la tabla interna */
    uint32_t aml_incompleto;     /* listas de términos que el lector dejó a medias */
    uint32_t pss_bytes;          /* «_PSS» declarados en los bytes (para ver si se escapa alguno) */
    uint32_t pss_lidos;          /* _PSS anotados como objetos */
    uint32_t qtd_cpus;           /* procesadores anotados (hasta PSS_MAX_CPUS) */
    uint32_t cpus_total;         /* procesadores encontrados */
    struct {
        uint32_t fis, tam;
        uint8_t  dinamica;
        char     assinatura[5];
        char     oem[9];
    } tabela[PSS_MAX_TABELAS];
    pss_cpu_t cpu[PSS_MAX_CPUS];
} acpi_pss_t;

/* Devuelve un puntero legible a [fis, fis + tam), o NULL si esa memoria no se
 * debe leer. raiz = la tabla viene de la RSDT/XSDT o de la FADT (ya validada
 * por acpi.c); si no, es una SSDT dinámica encontrada en el AML. */
typedef const uint8_t *(*pss_mapear_t)(uint32_t fis, uint32_t tam, bool raiz);

/* Lee las tablas (DSDT y SSDT) y las SSDT que cargan con Load(). Una tabla
 * con firma o suma de verificación incorrecta no se lee. */
void acpi_pss_ler(acpi_pss_t *r, const uint32_t *tabelas, uint32_t qtd, pss_mapear_t mapear);

/* Lo que se sabe de la CPU para validar lo leído. */
typedef struct {
    uint32_t familia, modelo;
    bool     intel_fidvid;       /* Intel familia 6 con FID/VID en PERF_CTL (Pentium M a Core 2) */
    bool     tem_status;         /* perf_status leído */
    uint64_t perf_status;        /* IA32_PERF_STATUS (0x198) de la CPU de arranque */
    uint32_t tsc_khz;            /* 0 = sin medida o TSC que cambia con la frecuencia */
    uint32_t qtd_presentes;      /* 0 = no se sabe: todos cuentan */
    uint8_t  presentes[32];      /* ACPI Processor ID de los procesadores encendidos (MADT) */
} pss_cpu_info_t;

/* Valida cada procesador (fiavel, motivo) y la coherencia de los dominios
 * _PSD. Devuelve cuántos procesadores quedan fiables. */
uint32_t acpi_pss_validar(acpi_pss_t *r, const pss_cpu_info_t *i);

/* Ayudas para mostrar: FID (en medios, con el bit 14 de los Core 2) y VID. */
uint32_t pss_fid_x2(uint32_t controle, uint32_t modelo);
uint32_t pss_vid(uint32_t controle);
bool     pss_core2(uint32_t modelo);

#endif
