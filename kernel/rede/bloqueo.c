/* bloqueo.c - bloqueo de publicidad y rastreo por DNS (D218).
 *
 * Es un sumidero DNS como el de Pi-hole, pero dentro del resolvedor del
 * kernel (sis_resolver, rede.c): un nombre de las listas responde 0.0.0.0 sin
 * salir a la red, y eso vale para todos los programas (NetSurf, Cipó, wget,
 * Python...). Desactivado por defecto: lo enciende «bloqueo activar» o
 * Configuración > Red, que escriben /home/.config/bloqueo/bloqueo.conf.
 *
 * Reparto del trabajo:
 *   - el programa «bloqueo» lee las listas (formato hosts, texto que viene de
 *     internet) y compila tabla.bin: hashes de 64 bits ordenados;
 *   - el kernel solo lee bloqueo.conf («activo», «subdominios») y carga
 *     tabla.bin después de comprobarla entera (cabecera, tamaño exacto y
 *     orden); con cualquier fallo se queda con la tabla de antes (o sin
 *     ninguna) y lo dice en «erro». Nunca impide arrancar.
 *
 * Todo corre con las interrupciones apagadas (en las llamadas al sistema, que
 * ya entran así, y al arrancar), como el resto de la pila de red. */
#include "bloqueo.h"
#include "abi.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "prazo.h"
#include "sistema.h"
#include "texto.h"
#include "vfs.h"
#include "bloqueo_tabla.h"
#include "conf_sistema.h"

/* D371: con la configuración de la máquina separada, el bloqueo (de toda la máquina) se lee de /etc y de
 * SAGUI-CONF, que solo escribe el administrador; si no, de /home como antes. */
static const char *arq_conf(void) { return conf_sistema_separada() ? "/etc/bloqueo.conf" : BLOQ_CONF; }
static const char *arq_tabla(void) { return conf_sistema_separada() ? "/sistema/conf/bloqueo/tabla.bin" : BLOQ_TABLA; }

static uint8_t        *tabela;          /* el archivo entero, tal como se leyó */
static uint32_t        tabela_bytes;
static const uint64_t *bloq, *perm;
static uint32_t        n_bloq, n_perm;
static bool            pedido, subdominios = true;
static uint32_t        consultas, bloqueadas, cargas, carga_us;
static int             ultimo_erro;

#define ULTIMOS 16
static char     ultimos[ULTIMOS][64];
static uint32_t ultimos_qtd, ultimos_pos;

static bool ativo(void) { return pedido && tabela; }

/* ------------------------------------------------------------ carga */

/* bloqueo.conf: líneas «clave valor»; el resto (las listas...) es del programa. */
static void ler_conf(bool *p, bool *sub)
{
    *p = false;
    *sub = true;
    uint8_t *txt;
    uint32_t tam;
    if (vfs_ler_inteiro(arq_conf(), &txt, &tam) < 0) return;
    for (uint32_t i = 0; i < tam;) {
        uint32_t f = i;
        while (f < tam && txt[f] != '\n') f++;
        const char *l = (const char *)txt + i;
        uint32_t n = f - i;
        if (n >= 8 && !memcmp(l, "activo ", 7)) *p = l[7] == '1';
        else if (n >= 13 && !memcmp(l, "subdominios ", 12)) *sub = l[12] == '1';
        i = f + 1;
    }
    kfree(txt);
}

/* Comprueba la tabla leída (bloqueo_tabla.h); 0 = se puede usar. */
static int validar(const uint8_t *d, uint32_t tam)
{
    int r = bloqueo_validar(d, tam);
    return r == 0 ? 0 : r == BLOQ_MAL_LIMITE ? ERRO_LIMITE : ERRO_FORMATO;
}

static void liberar(void)
{
    if (tabela) kfree(tabela);
    tabela = NULL;
    tabela_bytes = 0;
    bloq = perm = NULL;
    n_bloq = n_perm = 0;
}

/* Relee bloqueo.conf y, si pide el bloqueo, tabla.bin. Con un error se queda
 * la tabla que hubiera. */
static int recargar(void)
{
    bool p, sub;
    ler_conf(&p, &sub);
    subdominios = sub;
    if (!p) {                       /* apagado: la memoria se devuelve */
        pedido = false;
        liberar();
        ultimo_erro = 0;
        return 0;
    }
    uint64_t t0 = prazo_tsc();
    uint8_t *d;
    uint32_t tam;
    int r = vfs_ler_inteiro(arq_tabla(), &d, &tam);
    if (r == 0) {
        r = validar(d, tam);
        if (r < 0) kfree(d);
    }
    if (r < 0) {
        ultimo_erro = r;
        pedido = true;               /* con la tabla de antes, si la había */
        klog("[BLOQUEO] no se pudo cargar %s (error %d)%s\n", arq_tabla(), r,
             tabela ? ": sigue la tabla anterior" : ": el bloqueo queda apagado");
        return r;
    }
    const bloqueo_cabecera_t *c = (const bloqueo_cabecera_t *)d;
    bool i0 = int_salva_desliga();
    liberar();
    tabela = d;
    tabela_bytes = tam;
    n_bloq = c->n_bloq;
    n_perm = c->n_perm;
    bloq = (const uint64_t *)(d + sizeof *c);
    perm = bloq + n_bloq;
    pedido = true;
    int_restaura(i0);
    uint32_t khz = hw_tsc_khz();
    carga_us = khz ? (uint32_t)((prazo_tsc() - t0) * 1000u / khz) : 0;
    cargas++;
    ultimo_erro = 0;
    klog("[BLOQUEO] activo: %u bloqueados, %u permitidos, %u KB, subdominios %s, %u us\n",
         n_bloq, n_perm, (tam + 1023) / 1024, subdominios ? "si" : "no", carga_us);
    return 0;
}

void bloqueo_iniciar(void)
{
    if (!vfs_existe_arquivo(arq_conf())) return;   /* lo normal: nunca se activó */
    recargar();
}

/* ------------------------------------------------------------ consulta */

static void anotar(const char *nome)
{
    txt_copia_n(ultimos[ultimos_pos], nome, sizeof ultimos[0]);
    ultimos_pos = (ultimos_pos + 1) % ULTIMOS;
    if (ultimos_qtd < ULTIMOS) ultimos_qtd++;
}

static int decidir(const char *nome, bool contar)
{
    if (contar) consultas++;
    int r = bloqueo_decidir(nome, (uint32_t)txt_tamanho(nome), bloq, n_bloq, perm, n_perm, subdominios);
    if (r == BLOQ_BLOQUEADO && contar) {
        bloqueadas++;
        anotar(nome);
    }
    return r;
}

bool bloqueo_filtrar(const char *nome)
{
    if (!ativo()) return false;
    bool i0 = int_salva_desliga();
    bool b = decidir(nome, true) == BLOQ_BLOQUEADO;
    int_restaura(i0);
    return b;
}

/* Copia y valida un nombre de un programa: letras, números, '-', '_' y
 * puntos, sin rótulos vacíos; en minúsculas y sin el punto final. */
static int nome_do_usuario(uint32_t p, char *nome, uint32_t max)
{
    int r = usuario_texto((const char *)p, nome, max);
    if (r < 0) return r;
    if (r > 0 && nome[r - 1] == '.') nome[--r] = 0;
    if (r == 0 || r > BLOQ_NOME_MAX) return ERRO_INVALIDO;
    int rotulo = 0;
    for (int i = 0; i < r; i++) {
        char c = nome[i];
        if (c >= 'A' && c <= 'Z') nome[i] = c = (char)(c + 32);
        if (c == '.') { if (!rotulo) return ERRO_INVALIDO; rotulo = 0; continue; }
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return ERRO_INVALIDO;
        if (++rotulo > 63) return ERRO_INVALIDO;
    }
    return r;
}

/* ------------------------------------------------------------ llamada */

int32_t sis_bloqueo(uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    (void)c; (void)d;
    switch (op) {
    case BLOQ_ESTADO: {
        info_bloqueo_t i;
        if (b < sizeof i) return ERRO_INVALIDO;
        if (!usuario_ok((void *)a, sizeof i, true)) return ERRO_ENDERECO;
        memset(&i, 0, sizeof i);
        i.activo = ativo();
        i.pedido = pedido;
        i.subdominios = subdominios;
        i.bloqueados = n_bloq;
        i.permitidos = n_perm;
        i.bytes = tabela_bytes;
        i.consultas = consultas;
        i.bloqueadas = bloqueadas;
        i.cargas = cargas;
        i.erro = ultimo_erro;
        i.carga_us = carga_us;
        memcpy((void *)a, &i, sizeof i);
        return 0;
    }
    case BLOQ_RECARGAR:
        return recargar();
    case BLOQ_ULTIMOS: {
        uint32_t n = b < ultimos_qtd ? b : ultimos_qtd;
        if (n && !usuario_ok((void *)a, n * 64u, true)) return ERRO_ENDERECO;
        char (*o)[64] = (char (*)[64])a;
        for (uint32_t k = 0; k < n; k++)      /* el más reciente primero */
            memcpy(o[k], ultimos[(ultimos_pos + ULTIMOS - 1 - k) % ULTIMOS], 64);
        return (int32_t)n;
    }
    case BLOQ_CONSULTAR: {
        char nome[BLOQ_NOME_MAX + 2];
        int r = nome_do_usuario(a, nome, sizeof nome);
        if (r < 0) return r;
        if (!tabela || (!pedido && !(b & BLOQC_SIEMPRE))) return BLOQ_LIBRE;
        return decidir(nome, (b & BLOQC_CONTAR) && ativo());
    }
    case BLOQ_CONTADORES:
        consultas = bloqueadas = 0;
        ultimos_qtd = ultimos_pos = 0;
        return 0;
    }
    return ERRO_INVALIDO;
}
