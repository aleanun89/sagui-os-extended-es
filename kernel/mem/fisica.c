// SPDX-License-Identifier: GPL-3.0-only
/* fisica.c - gestor de páginas físicas (mapas de bits). Ver D008, D348 y D349.
 *
 * Tres zonas, cada una con su mapa de bits (1 = ocupada):
 *   baja     de 0 a MEMFIS_LIMITE: mapa estático en la BSS (60 KB), como siempre;
 *   alta     de MEMFIS_LIMITE hasta el fin de la RAM libre por debajo de 4 GB: mapa
 *            del tamaño justo, pedido a la zona baja al arrancar (nada si no hay alta);
 *   superior (D349, solo con PAE) de 4 GB hasta el fin de la RAM libre, como mucho
 *            64 GB (o lo que permitan los bits físicos de la CPU): igual que la alta
 *            (mapa de hasta 1,9 MB, pedido a la baja).
 * Las páginas de las zonas alta y superior no tienen dirección dentro del kernel:
 * solo valen para los programas (memfisica_alocar_usuario) y el kernel las toca
 * por las ventanas de memalta.c. Todo lo demás (memfisica_alocar*) sale de la baja. */
#include "memoria.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "texto.h"
#include "paginacao.h"

#define TOTAL_PAGINAS (MEMFIS_LIMITE / PAGINA)
#define PALAVRAS      (TOTAL_PAGINAS / 32)
#define LIMITE_32     0x100000000ull

extern char _kernel_inicio[], _kernel_fim[];

typedef struct {
    uint32_t *bits;      /* 1 = ocupada */
    uint32_t base;       /* primera página (número de página física) */
    uint32_t paginas;    /* páginas que cubre el mapa (múltiplo de 32) */
    uint32_t livres, geridas;
    uint32_t dica;       /* palabra donde la última búsqueda se detuvo */
} zona_t;

static uint32_t mapa_bits[PALAVRAS];
static zona_t baixa = { mapa_bits, 0, TOTAL_PAGINAS, 0, 0, 0 };
static zona_t alta;      /* paginas = 0: no hay zona alta */
static zona_t superior;  /* D349: paginas = 0: no hay (sin PAE o sin RAM por encima de 4 GB) */

static inline bool ocupada(zona_t *z, uint32_t p)  { return z->bits[p >> 5] & (1u << (p & 31)); }
static inline void ocupar(zona_t *z, uint32_t p)   { z->bits[p >> 5] |= 1u << (p & 31); }
static inline void desocupar(zona_t *z, uint32_t p){ z->bits[p >> 5] &= ~(1u << (p & 31)); }

/* La zona de una página física (número de página), o NULL si ninguna la gestiona. */
static zona_t *zona_de(uint32_t pag)
{
    if (pag < TOTAL_PAGINAS) return &baixa;
    if (alta.paginas && pag >= alta.base && pag - alta.base < alta.paginas) return &alta;
    if (superior.paginas && pag >= superior.base && pag - superior.base < superior.paginas) return &superior;
    return NULL;
}

/* Marca [ini, fin) de la zona con el estado dado, página a página, manteniendo la cuenta. */
static void marcar(zona_t *z, uint64_t ini, uint64_t fim, bool como_ocupada)
{
    uint64_t z_ini = (uint64_t)z->base * PAGINA, z_fim = z_ini + (uint64_t)z->paginas * PAGINA;
    if (ini < z_ini) ini = z_ini;
    if (fim > z_fim) fim = z_fim;
    for (uint64_t a = ini & ~(uint64_t)(PAGINA - 1); a < fim; a += PAGINA) {
        uint32_t p = (uint32_t)(a / PAGINA) - z->base;
        if (como_ocupada && !ocupada(z, p)) { ocupar(z, p); z->livres--; }
        else if (!como_ocupada && ocupada(z, p)) { desocupar(z, p); z->livres++; }
    }
}

/* Libera las regiones "libres" del E820 dentro de la zona (redondeadas hacia
 * dentro) y vuelve a ocupar lo reservado que se solape: en BIOS mal hechas una
 * región libre puede pisar una reservada, y lo reservado gana. */
static void preencher(zona_t *z)
{
    for (uint32_t i = 0; i < info_boot.mapa_qtd; i++) {
        regiao_e820_t *r = &mapa_memoria[i];
        if (r->tipo != E820_LIVRE || !(r->extra & 1)) continue;
        uint64_t ini = (r->base + PAGINA - 1) & ~(uint64_t)(PAGINA - 1);
        uint64_t fim = (r->base + r->tamanho) & ~(uint64_t)(PAGINA - 1);
        if (fim > ini) marcar(z, ini, fim, false);
    }
    for (uint32_t i = 0; i < info_boot.mapa_qtd; i++) {
        regiao_e820_t *r = &mapa_memoria[i];
        if (r->tipo != E820_LIVRE)
            marcar(z, r->base, r->base + r->tamanho, true);
    }
    /* El kernel y el disco en RAM están en la baja, pero se marcan en las dos:
     * con UEFI el ramdisk va donde dé el firmware (D264). */
    marcar(z, 0, 0x100000, true);                                      /* primer 1 MB */
    marcar(z, (uint32_t)_kernel_inicio, (uint32_t)_kernel_fim, true);   /* kernel + BSS */
    marcar(z, info_boot.ramdisk_inicio, (uint64_t)info_boot.ramdisk_inicio + info_boot.ramdisk_bytes, true);
}

/* n páginas seguidas de la baja, buscando desde la más alta (solo en el arranque). */
static uint32_t alocar_seguidas_do_topo(uint32_t n)
{
    uint32_t seguidas = 0;
    for (uint32_t p = TOTAL_PAGINAS; p-- > 0x100000 / PAGINA;) {
        if (ocupada(&baixa, p)) { seguidas = 0; continue; }
        if (++seguidas == n) {
            for (uint32_t k = p; k < p + n; k++) ocupar(&baixa, k);
            baixa.livres -= n;
            return p * PAGINA;
        }
    }
    return 0;
}

/* El fin de la RAM libre del E820 entre "ini" y "lim" (0 si no hay). */
static uint64_t fim_ram(uint64_t ini, uint64_t lim)
{
    uint64_t fim = 0;
    for (uint32_t i = 0; i < info_boot.mapa_qtd; i++) {
        regiao_e820_t *r = &mapa_memoria[i];
        if (r->tipo != E820_LIVRE || !(r->extra & 1) || r->base >= lim) continue;
        uint64_t f = (r->base + r->tamanho) & ~(uint64_t)(PAGINA - 1);
        if (f > lim) f = lim;
        if (f > ini && f > fim) fim = f;
    }
    return fim;
}

/* Prepara una zona solo para programas de [ini, fim) (alta o superior): el mapa
 * de bits sale de la baja, del tamaño justo. */
static void iniciar_zona(zona_t *z, uint64_t ini, uint64_t fim, const char *nome)
{
    if (fim <= ini) return;
    uint32_t paginas = (uint32_t)((fim - ini) / PAGINA);
    paginas = (paginas + 31) & ~31u;
    uint32_t bytes = paginas / 8;
    uint32_t pags_mapa = (bytes + PAGINA - 1) / PAGINA;
    /* El mapa sale de la baja: la paginación aún no está encendida, pero después
     * el kernel lo sigue viendo por el mapeo 1:1. Se pide desde arriba, para no
     * partir los huecos bajos (los 128 KB del DMA ISA bajo 16 MB, D302). */
    uint32_t m = alocar_seguidas_do_topo(pags_mapa);
    if (!m) { klog("[SAGUI] memoria %s: sin sitio para el mapa (%u KB): no se usa\n", nome, pags_mapa * 4); return; }
    z->bits = (uint32_t *)m;
    z->base = (uint32_t)(ini / PAGINA);
    z->paginas = paginas;
    z->livres = 0;
    z->dica = 0;
    memset(z->bits, 0xFF, pags_mapa * PAGINA);
    preencher(z);
    z->geridas = z->livres;
    if (!z->geridas) { memfisica_liberar_seguidas(m, pags_mapa); z->paginas = 0; return; }
    klog("[SAGUI] memoria %s: %u paginas livres (%u MB) de %x%08x a %x%08x, mapa de %u KB\n",
         nome, z->livres, z->livres / 256, (uint32_t)(ini >> 32), (uint32_t)ini,
         (uint32_t)((fim - 1) >> 32), (uint32_t)(fim - 1), pags_mapa * 4);
}

/* Depuración (rastro.h): la página de las marcas que sobreviven a un reinicio en caliente. La misma en cada
 * arranque de la misma máquina, lejos de lo que carga la etapa 2: 64 KB por debajo del fin de la región libre
 * más alta bajo 4 GB que tenga al menos 1 MB, y siempre DENTRO de esa región.
 *
 * Antes (dbg5) era «el fin de la RAM libre menos 64 KB» sin mirar en qué región caía: en el HP dv7 la última
 * región libre es una sola página (0xBFEFF000) justo detrás de las tablas ACPI, y la marca se escribía encima
 * de la DSDT (0xBFEF0000): uACPI no arrancaba («unable to find DSDT», la firma leída era «SRA1»). D402, D405. */
static uint64_t rastro_fis;
uint64_t rastro_pagina(void) { return rastro_fis; }

static uint64_t rastro_escolher(void)
{
    uint64_t melhor = 0;
    for (uint32_t i = 0; i < info_boot.mapa_qtd; i++) {
        regiao_e820_t *r = &mapa_memoria[i];
        if (r->tipo != E820_LIVRE || !(r->extra & 1) || r->base >= LIMITE_32) continue;
        uint64_t ini = (r->base + PAGINA - 1) & ~(uint64_t)(PAGINA - 1);
        uint64_t fim = (r->base + r->tamanho) & ~(uint64_t)(PAGINA - 1);
        if (fim > LIMITE_32) fim = LIMITE_32;
        if (ini < 0x100000) ini = 0x100000;
        if (fim <= ini || fim - ini < 0x100000) continue;   /* menos de 1 MB: no (las sueltas junto a ACPI) */
        uint64_t p = fim - 16u * PAGINA;
        if (p < 0x2000000u || p < ini) continue;
        if (p > melhor) melhor = p;
    }
    /* La página entera tiene que ser libre según TODO el mapa (un E820 con regiones solapadas). */
    for (uint32_t i = 0; melhor && i < info_boot.mapa_qtd; i++) {
        regiao_e820_t *r = &mapa_memoria[i];
        if (r->tipo == E820_LIVRE || !(r->extra & 1)) continue;
        if (melhor < r->base + r->tamanho && melhor + PAGINA > r->base) melhor = 0;
    }
    return melhor;
}

void memfisica_iniciar(void)
{
    memset(mapa_bits, 0xFF, sizeof mapa_bits);
    baixa.livres = 0;
    preencher(&baixa);
    baixa.dica = 0;
    rastro_fis = rastro_escolher();
    if (rastro_fis && rastro_fis < MEMFIS_LIMITE) marcar(&baixa, rastro_fis, rastro_fis + PAGINA, true);
    /* D348: la alta, hasta el fin de la RAM bajo 4 GB (sin PAE no se ve más allá). */
    iniciar_zona(&alta, MEMFIS_LIMITE, fim_ram(MEMFIS_LIMITE, LIMITE_32), "alta");
    if (rastro_fis >= MEMFIS_LIMITE && alta.paginas) marcar(&alta, rastro_fis, rastro_fis + PAGINA, true);
    /* D349: con PAE, la superior: de 4 GB al fin de la RAM, sin pasar del límite físico. */
    if (paginacao_com_pae) {
        uint64_t lim = paginacao_limite_fisico();
        iniciar_zona(&superior, LIMITE_32, fim_ram(LIMITE_32, lim), "superior");
        uint64_t mais = fim_ram(lim, ~0ull);
        if (mais > lim)
            klog("[SAGUI] memoria: %u MB por encima de %u GB no se usan (limite fisico)\n",
                 (uint32_t)((mais - lim) >> 20), (uint32_t)(lim >> 30));
    } else if (fim_ram(LIMITE_32, ~0ull)) {
        klog("[SAGUI] memoria: %u MB por encima de 4 GB no se usan (sin PAE)\n",
             (uint32_t)((fim_ram(LIMITE_32, ~0ull) - LIMITE_32) >> 20));
    }
    baixa.geridas = baixa.livres;   /* sin los mapas de la alta y la superior */
    klog("[SAGUI] memoria fisica: %u paginas livres (%u KB)\n", baixa.livres, baixa.livres * 4);
}

/* Una página libre de la zona (número de página relativo a la zona), o -1. */
static int32_t buscar(zona_t *z)
{
    uint32_t palavras = z->paginas / 32;
    for (uint32_t n = 0; n < palavras; n++) {
        uint32_t w = (z->dica + n) % palavras;
        if (z->bits[w] == 0xFFFFFFFFu)
            continue;
        uint32_t bit = (uint32_t)__builtin_ctz(~z->bits[w]);   /* primer bit 0 */
        uint32_t p = w * 32 + bit;
        ocupar(z, p);
        z->livres--;
        z->dica = w;
        return (int32_t)p;
    }
    return -1;
}

uint32_t memfisica_alocar(void)
{
    bool i = int_salva_desliga();
    int32_t p = baixa.livres ? buscar(&baixa) : -1;
    int_restaura(i);
    return p < 0 ? 0 : (uint32_t)p * PAGINA;
}

/* Una página de las zonas de "ordem" (las primeras n), en ese orden; 0 si no queda. */
static uint64_t alocar_de(zona_t *const *ordem, uint32_t n)
{
    bool i = int_salva_desliga();
    uint64_t r = 0;
    for (uint32_t k = 0; k < n && !r; k++) {
        zona_t *z = ordem[k];
        if (!z->paginas || !z->livres) continue;
        int32_t p = buscar(z);
        if (p >= 0) r = (uint64_t)(z->base + (uint32_t)p) * PAGINA;
    }
    int_restaura(i);
    return r;
}

/* D350: quien puede devolver memoria cuando se acaba (la caché de disco). Nunca duerme:
 * memfisica_alocar_usuario() se llama entre la comprobación de una entrada de la tabla de
 * páginas y su escritura (espaco_alocar), y otro hilo del mismo proceso no puede colarse. */
static uint32_t (*reclamador)(uint32_t paginas);

void memfisica_registar_reclamo(uint32_t (*f)(uint32_t paginas))
{
    reclamador = f;
}

#define RECLAMO_LOTE 256u   /* páginas que se piden de una vez (1 MB): una sola entrada en la caché */

static bool reclamar(void)
{
    return reclamador && reclamador(RECLAMO_LOTE) > 0;
}

uint32_t memfisica_reclamar(uint32_t paginas)
{
    return reclamador && paginas ? reclamador(paginas) : 0;
}

/* D352: la swap comprimida (troca.c) y la reserva baja que deja al kernel. */
static uint32_t (*trocador)(uint32_t paginas);
static uint32_t reserva_troca;

void memfisica_registar_troca(uint32_t (*f)(uint32_t paginas), uint32_t reserva_baixa)
{
    reserva_troca = reserva_baixa;
    trocador = f;
}

uint32_t memfisica_livres_programas(void)
{
    uint32_t b = baixa.livres > reserva_troca ? baixa.livres - reserva_troca : 0;
    return alta.livres + superior.livres + b;
}

/* D352: una página baja solo si quedan más de la reserva del kernel. */
static uint64_t baixa_sobre_reserva(void)
{
    static zona_t *const so_baixa[1] = { &baixa };
    return baixa.livres > reserva_troca ? alocar_de(so_baixa, 1) : 0;
}

#define TROCA_LOTE 32u   /* páginas que se sacan de una vez cuando un programa no encuentra memoria */

/* D348/D349: para las páginas de los programas. Primero la superior y la alta,
 * que el kernel no puede usar para otra cosa; la baja solo cuando se acabaron.
 * D350: antes de tocar la baja (y otra vez antes de fallar), la caché de disco
 * devuelve bloques limpios.
 * D352: con la swap comprimida, la baja hasta la reserva del kernel; después se sacan
 * páginas a la swap y se vuelve a probar; solo si la swap no libera nada, la baja entera
 * (lo de siempre). Sin swap, todo como antes. */
uint64_t memfisica_alocar_usuario(void)
{
    static zona_t *const altas[2] = { &superior, &alta };
    static zona_t *const todas[3] = { &superior, &alta, &baixa };
    uint64_t r = alocar_de(altas, 2);
    if (r) return r;
    if ((alta.paginas || superior.paginas) && reclamar()) {
        r = alocar_de(altas, 2);
        if (r) return r;
    }
    if (trocador) {
        r = baixa_sobre_reserva();
        if (r) return r;
        /* Sin alta, la caché de disco vive en la baja: antes de sacar nada, que devuelva. */
        if (!alta.paginas && !superior.paginas && reclamar()) {
            r = baixa_sobre_reserva();
            if (r) return r;
        }
        if (trocador(TROCA_LOTE)) {
            r = alocar_de(altas, 2);
            if (!r) r = baixa_sobre_reserva();
            if (r) return r;
        }
    }
    r = alocar_de(todas, 3);
    if (r) return r;
    if (reclamar()) r = alocar_de(todas, 3);
    return r;
}

/* D350: una página de la alta o la superior (nunca de la baja) para los bloques de la caché
 * de disco; 0 si no hay o si quedan menos de "reserva" libres (para los programas). */
uint64_t memfisica_alocar_alta(uint32_t reserva)
{
    static zona_t *const altas[2] = { &superior, &alta };
    if (alta.livres + superior.livres <= reserva) return 0;
    return alocar_de(altas, 2);
}

uint32_t memfisica_alocar_seguidas(uint32_t n)
{
    if (n == 0) return 0;
    if (n == 1) return memfisica_alocar();
    bool i = int_salva_desliga();
    uint32_t seguidas = 0;
    for (uint32_t p = 0; p < TOTAL_PAGINAS; p++) {
        if ((p & 31) == 0 && mapa_bits[p >> 5] == 0xFFFFFFFFu) {  /* salta 32 ocupadas de una vez */
            seguidas = 0;
            p += 31;
            continue;
        }
        if (ocupada(&baixa, p)) { seguidas = 0; continue; }
        if (++seguidas == n) {
            uint32_t ini = p + 1 - n;
            for (uint32_t k = ini; k <= p; k++) ocupar(&baixa, k);
            baixa.livres -= n;
            int_restaura(i);
            return ini * PAGINA;
        }
    }
    int_restaura(i);
    return 0;
}

/* D302: n páginas seguidas por debajo de "limite" (bytes) que empiezan en un múltiplo de "alinhamento"
 * (bytes, potencia de 2). Para el DMA ISA (8237): debajo de 16 MB y sin cruzar 64/128 KB. */
uint32_t memfisica_alocar_baixa(uint32_t n, uint32_t alinhamento, uint32_t limite)
{
    if (n == 0 || alinhamento < PAGINA) return 0;
    uint32_t passo = alinhamento / PAGINA, fim = limite / PAGINA;
    if (fim > TOTAL_PAGINAS) fim = TOTAL_PAGINAS;
    bool i = int_salva_desliga();
    for (uint32_t p = 0; p + n <= fim; p += passo) {
        uint32_t k = 0;
        while (k < n && !ocupada(&baixa, p + k)) k++;
        if (k < n) continue;
        for (k = 0; k < n; k++) ocupar(&baixa, p + k);
        baixa.livres -= n;
        int_restaura(i);
        return p * PAGINA;
    }
    int_restaura(i);
    return 0;
}

void memfisica_liberar_seguidas(uint64_t endereco, uint32_t n)
{
    bool i = int_salva_desliga();
    for (uint32_t k = 0; k < n; k++) {
        uint32_t p = (uint32_t)(endereco / PAGINA) + k;
        zona_t *z = zona_de(p);
        if (!z || !ocupada(z, p - z->base)) {
            klog("[SAGUI] AVISO: liberando pagina fisica invalida ou livre: %x%08x\n",
                 (uint32_t)((uint64_t)p * PAGINA >> 32), p * PAGINA);
            continue;
        }
        desocupar(z, p - z->base);
        z->livres++;
    }
    int_restaura(i);
}

void memfisica_liberar(uint64_t endereco)
{
    memfisica_liberar_seguidas(endereco, 1);
}

/* D348: ¿algo de [ini, fim) es RAM libre del E820 (de cualquier zona, o por
 * encima de 4 GB)? Para no dejar que un programa mapee RAM como si fuera un BAR. */
bool memfisica_sobre_ram(uint64_t ini, uint64_t fim)
{
    for (uint32_t i = 0; i < info_boot.mapa_qtd; i++) {
        regiao_e820_t *r = &mapa_memoria[i];
        if (r->tipo == E820_LIVRE && r->base < fim && r->base + r->tamanho > ini) return true;
    }
    return false;
}

uint32_t memfisica_livres(void)       { return baixa.livres; }
uint32_t memfisica_geridas(void)      { return baixa.geridas; }
uint32_t memfisica_livres_alta(void)  { return alta.livres + superior.livres; }
uint32_t memfisica_geridas_alta(void) { return alta.geridas + superior.geridas; }
uint32_t memfisica_livres_superior(void)  { return superior.livres; }
uint32_t memfisica_geridas_superior(void) { return superior.geridas; }
