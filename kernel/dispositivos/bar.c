// SPDX-License-Identifier: GPL-2.0-or-later
/* bar.c - los BAR (las ventanas de direcciones) de un dispositivo PCI (D158).
 *
 * Un BAR es la dirección física que un dispositivo pide en el espacio de
 * configuración para tener sus propias ventanas: los registros del dispositivo
 * (MMIO) y, a veces, su memoria de vídeo. Hasta ahora el kernel solo leía el
 * espacio de configuración (kernel/dispositivos/pci.c) y no había forma de
 * llegar a esos registros desde un programa: por eso no se podía identificar
 * una GPU por sus propios registros, solo por su identificador del PCI.
 *
 * Aquí está la pieza mínima del kernel que falta:
 *
 *   - el kernel elige QUÉ se mapea: solo las páginas de un BAR que el propio
 *     dispositivo declara en su espacio de configuración. El programa pide
 *     «mapea el BAR n del dispositivo (bus, dispositivo, función), desde el
 *     desplazamiento d, con t bytes» y recibe una dirección virtual. Nunca ve
 *     una dirección física, y no puede pedir páginas sueltas de la RAM;
 *   - el kernel sondea el tamaño del BAR (escribiendo unos en el registro y
 *     leyendo lo que devuelve) para no dejar pasar una ventana que se salga
 *     del BAR;
 *   - las páginas se mapean sin caché (PG_PCD), porque los registros de un
 *     dispositivo no pueden quedar en la caché de la CPU: si no, el programa
 *     leería valores viejos;
 *   - las páginas del mapa se marcan como prestadas (PG_EMPRESTADA) para que
 *     al terminar el proceso no se devuelvan al gestor de memoria: no son
 *     páginas del sistema, son del dispositivo;
 *   - un BAR que se solape con la RAM del sistema no se mapea: le daría al
 *     programa acceso a la memoria del kernel.
 *
 * Todo lo demás (qué registros leer, cómo hablar con la GPU, el vigilante, el
 * anillo de comandos) es cosa del programa (userland/libamd).
 */
#include "memoria.h"
#include "pci.h"
#include "paginacao.h"
#include "processo.h"
#include "sistema.h"
#include "texto.h"
#include "tipos.h"

#define BAR_REG_INICIO  0x10   /* el primer BAR está en el registro 0x10 */
#define BAR_COMANDO     0x04   /* comando: decodificación de memoria y bus master */
#define BAR_CMD_MEM     0x0002
#define BAR_CMD_MAESTRE 0x0004

/* El tamaño del BAR, con el truco de siempre: se escriben unos en el registro
 * y se lee lo que devuelve el dispositivo, que es la máscara del tamaño. Con la
 * decodificación de memoria apagada el dispositivo no hace caso del registro
 * (no puede estar viendo direcciones raras); al final se restauran el registro
 * y el comando. La pantalla sigue: el motor de barrido de la GPU lee la VRAM
 * por dentro, no por el bus. */
static bool bar_tamanho(const pci_disp_t *d, uint8_t reg, bool par64, uint64_t *tamanho)
{
    uint32_t original = pci_le32(d, reg);
    uint32_t original_alto = par64 ? pci_le32(d, (uint8_t)(reg + 4)) : 0;
    uint32_t comando = pci_le32(d, BAR_COMANDO);
    pci_escreve32(d, BAR_COMANDO, comando & ~(BAR_CMD_MEM | BAR_CMD_MAESTRE));
    pci_escreve32(d, reg, 0xFFFFFFFFu);
    if (par64) pci_escreve32(d, (uint8_t)(reg + 4), 0xFFFFFFFFu);
    uint32_t baixo = pci_le32(d, reg);
    uint32_t alto = par64 ? pci_le32(d, (uint8_t)(reg + 4)) : 0;
    pci_escreve32(d, reg, original);
    if (par64) pci_escreve32(d, (uint8_t)(reg + 4), original_alto);
    pci_escreve32(d, BAR_COMANDO, comando);
    if (baixo == 0xFFFFFFFFu && alto == 0xFFFFFFFFu) return false;   /* no implementado */
    uint64_t m = ((uint64_t)alto << 32) | baixo;
    m &= ~(uint64_t)0xF;    /* los cuatro bits bajos son el tipo del BAR, no dirección */
    uint64_t t = (~m) + 1;
    if (t == 0) return false;
    *tamanho = t;
    return true;
}

/* Dirección física y tamaño del BAR número `indice` del dispositivo. */
static bool bar_infos(const pci_disp_t *d, int indice, uint32_t *base, uint64_t *tamanho)
{
    if (indice < 0 || indice > 5) return false;
    uint8_t reg = (uint8_t)(BAR_REG_INICIO + 4 * indice);
    uint32_t v = pci_le32(d, reg);
    if (v & 1) return false;                        /* BAR de E/S: no se mapea en memoria */
    if (v == 0 || v == 0xFFFFFFFFu) return false;    /* no implementado */
    bool par64 = (v & 0x4) != 0;
    uint64_t b = v & ~(uint64_t)0xF;
    if (par64) {
        uint32_t alto = pci_le32(d, (uint8_t)(reg + 4));
        b |= (uint64_t)alto << 32;
    }
    if (!bar_tamanho(d, reg, par64, tamanho)) return false;
    if (b > 0xFFFFFFFFull) return false;    /* por encima de los 4 GB: no hay cómo llegar */
    *base = (uint32_t)b;
    return true;
}

/* La dirección virtual donde se coloca el mapa. Cada proceso tiene la suya:
 * la región de los mapas de dispositivos (USUARIO_MMIO) se reparte por
 * procesos, y dentro de un proceso va creciendo. */
static uint32_t mmio_proximo(uint32_t bytes)
{
    processo_t *atual = processo_recursos();   /* D167: los mapas son del proceso */
    uint32_t virt = atual->mmio_fim < USUARIO_MMIO ? USUARIO_MMIO : atual->mmio_fim;
    virt = (virt + PAGINA - 1) & ~(PAGINA - 1);
    if ((uint64_t)virt + bytes > USUARIO_MMIO_FIM) return 0;
    return virt;
}

/* SIS_MAPEAR_BAR: mapea una ventana de un BAR de un dispositivo PCI en el
 * espacio del programa. Devuelve 0 si todo fue bien (la dirección virtual se
 * deja en *saida) o un ERRO_*. La dirección no se devuelve como valor de
 * retorno porque es mayor que 0x80000000 y parecería un error. */
int32_t sis_mapear_bar(uint32_t ptr, uint32_t saida, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    if (!usuario_ok((const void *)ptr, sizeof(pedido_bar_t), false)) return ERRO_ENDERECO;
    if (!usuario_ok((void *)saida, sizeof(uint32_t), true)) return ERRO_ENDERECO;
    pedido_bar_t p = *(const pedido_bar_t *)ptr;
    if (p.barramento > 255 || p.dispositivo > 31 || p.funcao > 7 || p.indice > 5) return ERRO_INVALIDO;
    if (p.tamanho == 0 || p.tamanho > BAR_TAMANHO_MAX) return ERRO_INVALIDO;
    if (p.permissao != BAR_LEITURA && p.permissao != BAR_ESCRITA) return ERRO_INVALIDO;

    pci_disp_t disp = { (uint8_t)p.barramento, (uint8_t)p.dispositivo, (uint8_t)p.funcao, 0, 0, 0, 0, 0 };
    if (pci_le32(&disp, 0x00) == 0xFFFFFFFFu) return ERRO_NAO_EXISTE;   /* no hay nadie ahí */
    if (((pci_le32(&disp, 0x08) >> 16) & 0x7F) != 0) return ERRO_INVALIDO;   /* solo dispositivos normales */

    uint32_t base;
    uint64_t tam;
    if (!bar_infos(&disp, (int)p.indice, &base, &tam)) return ERRO_NAO_EXISTE;
    if ((uint64_t)p.desloc + p.tamanho > tam) return ERRO_ACESSO;   /* la ventana se sale del BAR */
    if ((uint64_t)base + p.desloc + p.tamanho > 0x100000000ull) return ERRO_ACESSO;

    uint32_t fis = base + p.desloc;
    uint32_t ini = fis & ~(PAGINA - 1);
    uint32_t fim = (fis + p.tamanho + PAGINA - 1) & ~(PAGINA - 1);
    if (fim <= ini) return ERRO_INVALIDO;
    /* Un BAR sobre la RAM del sistema le daría al programa la memoria del
     * kernel: no se mapea. */
    if (ini < paginacao_fim_mapa()) return ERRO_ACESSO;
    /* D348: ni la RAM alta, que es de otros programas. */
    if (memfisica_sobre_ram(ini, fim)) return ERRO_ACESSO;

    uint32_t bytes = fim - ini;
    uint32_t virt = mmio_proximo(bytes);
    if (!virt) return ERRO_SEM_MEMORIA;
    processo_t *atual = processo_recursos();   /* D167: los mapas son del proceso */
    int livre = -1;
    for (int i = 0; i < BAR_MAPAS_MAX; i++)
        if (!atual->bar_mapas[i].bytes) { livre = i; break; }
    if (livre < 0) return ERRO_LIMITE;

    uint32_t flags = PG_USUARIO | PG_PCD | PG_EMPRESTADA | PG_NX;   /* sin caché: son registros; no ejecutables (D349) */
    if (p.permissao == BAR_ESCRITA) flags |= PG_ESCRITA;
    uint32_t v = virt;
    for (uint32_t f = ini; v < virt + bytes; v += PAGINA, f = f + PAGINA) {
        if (!espaco_mapear(atual->espaco, v, f, flags)) {
            while (v > virt) { v -= PAGINA; espaco_desmapear(atual->espaco, v); }
            return ERRO_SEM_MEMORIA;
        }
    }
    atual->bar_mapas[livre].virt = virt;
    atual->bar_mapas[livre].bytes = bytes;
    atual->mmio_fim = virt + bytes;
    *(uint32_t *)saida = virt;
    return 0;
}

/* SIS_DESEJAR_BAR: deshace un mapa hecho por SIS_MAPEAR_BAR. */
int32_t sis_desejar_bar(uint32_t virt, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    processo_t *atual = processo_recursos();   /* D167: los mapas son del proceso */
    for (int i = 0; i < BAR_MAPAS_MAX; i++) {
        uint32_t ini = atual->bar_mapas[i].virt;
        uint32_t bytes = atual->bar_mapas[i].bytes;
        if (!bytes || ini != virt) continue;
        if (!espaco_pode_tirar()) return SIS_REPETIR;   /* D351 */
        for (uint32_t v = ini; v < ini + bytes; v += PAGINA) espaco_desmapear(atual->espaco, v);
        espaco_sincronizar(atual->espaco);   /* D167: otro hilo en otra CPU puede tenerlas en la TLB */
        atual->bar_mapas[i].bytes = 0;
        return 0;
    }
    return ERRO_NAO_EXISTE;
}
