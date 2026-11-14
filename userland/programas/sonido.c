// SPDX-License-Identifier: GPL-2.0-or-later
/* sonido - diagnóstico y pruebas del códec de sonido HDA (D107).
 *
 *   sonido                        resumen: códec, subsistema, ajuste, GPIO y pines encendidos
 *   sonido --codec                volcado de todos los nodos del códec (para un informe)
 *   sonido --tono                 tono de prueba: graves, agudos, izquierda y derecha
 *   sonido --gpio DATOS [MASCARA] pone los GPIO de la máscara (por defecto, los de DATOS)
 *                                 como salidas con esos datos y toca el tono
 *   sonido --pin NODO on|off      enciende (con camino a un DAC) o apaga un pin y toca el tono
 *   sonido --eapd NODO on|off     el bit EAPD del pin (amplificador externo) y el tono
 *   sonido --auto on|off          callar los altavoces con auriculares enchufados
 *   sonido --verbo NODO VERBO CARGA  manda un verbo y muestra la respuesta
 *   -n (delante de la orden)      sin tono después de cambiar algo
 *
 * Los NODO, VERBO, CARGA, DATOS y MASCARA van en hexadecimal (con o sin
 * 0x). Los cambios duran hasta reiniciar: sirven para descubrir en un
 * portátil qué pin o GPIO enciende cada altavoz sin recompilar el kernel.
 * Solo funciona con el driver hda (en QEMU: -device intel-hda). */
#include "sagui.h"

static bool com_tono = true;

static uint32_t v(unsigned no, unsigned verbo, unsigned carga)
{
    uint32_t r = 0xFFFFFFFFu;
    unsigned cmd = (no & 0x7F) << 20;
    if (verbo <= 0xF) cmd |= verbo << 16 | (carga & 0xFFFF);
    else cmd |= (verbo & 0xFFF) << 8 | (carga & 0xFF);
    if (audio_hda(HDA_OP_VERBO, cmd, (unsigned)&r) < 0) return 0xFFFFFFFFu;
    return r;
}

static uint32_t par(unsigned no, unsigned p) { return v(no, 0xF00, p); }

static bool hex(const char *s, unsigned *r)
{
    unsigned n = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (!*s) return false;
    for (; *s; s++) {
        int d = *s >= '0' && *s <= '9' ? *s - '0' : *s >= 'a' && *s <= 'f' ? *s - 'a' + 10 :
                *s >= 'A' && *s <= 'F' ? *s - 'A' + 10 : -1;
        if (d < 0 || n > 0x0FFFFFFF) return false;
        n = n * 16 + (unsigned)d;
    }
    *r = n;
    return true;
}

static int liga_desliga(const char *s)
{
    if (compara_texto(s, "on") == 0 || compara_texto(s, "1") == 0 || compara_texto(s, "si") == 0) return 1;
    if (compara_texto(s, "off") == 0 || compara_texto(s, "0") == 0 || compara_texto(s, "no") == 0) return 0;
    return -1;
}

/* ------------------------------------------------------------ nombres */

static const char *tipo_nome(unsigned t)
{
    static const char *n[16] = { "DAC", "ADC", "mezcla", "selector", "pin", "energia", "volumen", "pitido",
                                 "?8", "?9", "?a", "?b", "?c", "?d", "?e", "fabric." };
    return n[t & 0xF];
}

static const char *disp_nome(unsigned c)
{
    static const char *n[16] = { "linea", "altavoz", "auricul.", "CD", "SPDIF", "dig.sal", "modem", "modem2",
                                 "linea-ent", "aux", "micro", "telefono", "SPDIF-ent", "dig.ent", "reserv.", "otro" };
    return n[(c >> 20) & 0xF];
}

static const char *conexao_nome(unsigned c)
{
    static const char *n[4] = { "jack", "nada", "fijo", "ambos" };
    return n[c >> 30];
}

static const char *cor_nome(unsigned c)
{
    static const char *n[16] = { "?", "negro", "gris", "azul", "verde", "rojo", "naranja", "amarillo", "morado",
                                 "rosa", "?a", "?b", "?c", "?d", "blanco", "otro" };
    return n[(c >> 12) & 0xF];
}

static const char *papel_nome(unsigned p) { return p == 1 ? "altavoz" : p == 2 ? "auriculares" : "linea"; }

/* ------------------------------------------------------------ resumen y volcado */

static bool estado(info_hda_t *i)
{
    int r = audio_hda(HDA_OP_ESTADO, (unsigned)i, sizeof *i);
    if (r < 0) {
        imprimir_em(ERROS, "sonido: no hay tarjeta HDA con driver (solo AC'97, el altavoz del PC o nada)\n");
        return false;
    }
    return true;
}

static void resumo(const info_hda_t *i)
{
    imprimir("Codec HDA %04x:%04x rev %08x (direccion %u, grupo de audio %02x, nodos %02x-%02x)\n", i->id >> 16,
             i->id & 0xFFFF, i->revisao, i->codec, i->afg, i->primeiro_no, i->primeiro_no + i->n_nos - 1);
    imprimir("Subsistema: PCI %04x:%04x, codec %04x:%04x\n", i->ssid_pci >> 16, i->ssid_pci & 0xFFFF,
             i->ssid_codec >> 16, i->ssid_codec & 0xFFFF);
    if (i->ajuste[0]) imprimir("Ajuste: %s (%s)\n", i->ajuste, i->modelo);
    else imprimir("Ajuste: ninguno\n");
    uint32_t gd = v(i->afg, 0xF15, 0), gm = v(i->afg, 0xF16, 0), gdir = v(i->afg, 0xF17, 0);
    imprimir("GPIO: %u; escritos mascara %02x dir %02x datos %02x; ahora mascara %02x dir %02x datos %02x\n",
             i->n_gpio, i->gpio_mascara, i->gpio_direcao, i->gpio_dados, gm & 0xFF, gdir & 0xFF, gd & 0xFF);
    if ((i->id >> 16) == 0x111d) imprimir("Mapa de energia IDT (fec): %02x\n", v(i->afg, 0xFEC, 0) & 0xFF);
    imprimir("Autosilencio: %s; auriculares enchufados: %s\n", i->autosilencio ? "si" : "no",
             i->fones_presentes ? "si (altavoces callados)" : "no");
    imprimir("Pines encendidos:");
    for (unsigned k = 0; k < i->n_pinos; k++)
        imprimir("%s %02x %s (DAC %02x, config %08x)", k ? "," : "", i->pino[k], papel_nome(i->pino_papel[k]),
                 i->pino_dac[k], i->pino_config[k]);
    imprimir("%s\n", i->n_pinos ? "" : " ninguno");
}

/* Lista de conexiones (como en hda.c, con los rangos expandidos). */
static int conexoes(unsigned no, unsigned char *c, int max)
{
    uint32_t p = par(no, 0x0E);
    unsigned total = p & 0x7F, por = (p & 0x80) ? 2 : 4, bits = (p & 0x80) ? 16 : 8, ant = 0;
    int n = 0;
    for (unsigned i = 0; i < total && n < max; i += por) {
        uint32_t r = v(no, 0xF02, i);
        for (unsigned k = 0; k < por && i + k < total && n < max; k++) {
            unsigned e = (r >> (k * bits)) & ((1u << bits) - 1), faixa = e & (1u << (bits - 1));
            e &= (1u << (bits - 1)) - 1;
            if (faixa && ant)
                for (unsigned x = ant + 1; x <= e && n < max; x++) c[n++] = (unsigned char)x;
            else c[n++] = (unsigned char)e;
            ant = e;
        }
    }
    return n;
}

/* Amplificador: «L/R» en hex (bit 7 = silencio, 6:0 = ganancia). */
static void amp(const char *nome, unsigned no, bool saida, unsigned indice)
{
    unsigned base = (saida ? 0x8000u : 0) | (indice & 0xF);
    uint32_t l = v(no, 0xB, base | 0x2000), r = v(no, 0xB, base);
    imprimir(" %s%u %02x/%02x", nome, indice, l & 0xFF, r & 0xFF);
}

static void volcar(const info_hda_t *i)
{
    resumo(i);
    unsigned afg = i->afg;
    imprimir("Grupo %02x: tipos de flujo %08x, PCM %08x, amp. sal. %08x, amp. ent. %08x, GPIO cap %08x, energia %08x\n",
             afg, par(afg, 0x0B), par(afg, 0x0A), par(afg, 0x12), par(afg, 0x0D), par(afg, 0x11), v(afg, 0xF05, 0));
    imprimir("Nodo tipo     caps     conexiones (* = elegida)\n");
    uint32_t amp_s_afg = par(afg, 0x12), amp_e_afg = par(afg, 0x0D);
    for (unsigned n = i->primeiro_no; n < (unsigned)i->primeiro_no + i->n_nos; n++) {
        uint32_t caps = par(n, 0x09);
        unsigned t = (caps >> 20) & 0xF;
        unsigned char c[32];
        int nc = (caps & (1u << 8)) ? conexoes(n, c, 32) : 0;
        int sel = -1;
        if (nc > 1 && (t == 3 || t == 4)) sel = (int)(v(n, 0xF01, 0) & 0xFF);
        imprimir("%02x   %-8s %08x", n, tipo_nome(t), caps);
        if (nc) {
            imprimir(" [");
            for (int k = 0; k < nc; k++) imprimir("%s%02x%s", k ? " " : "", c[k], k == sel ? "*" : "");
            imprimir("]");
        }
        if (caps & (1u << 10)) imprimir(" D%u", v(n, 0xF05, 0) & 0xF);
        bool amp_s = caps & (1u << 2), amp_e = caps & (1u << 1);
        uint32_t cap_s = (caps & (1u << 3)) ? par(n, 0x12) : amp_s_afg;
        uint32_t cap_e = (caps & (1u << 3)) ? par(n, 0x0D) : amp_e_afg;
        if (amp_s) { amp("S", n, true, 0); imprimir("(cap %08x)", cap_s); }
        if (amp_e) {
            int ne = t == 2 ? (nc < 4 ? nc : 4) : 1;    /* mezcla: hasta 4 entradas */
            for (int k = 0; k < ne; k++) amp("E", n, false, (unsigned)k);
            imprimir("(cap %08x)", cap_e);
        }
        if (t == 0) imprimir(" flujo %02x fmt %04x", v(n, 0xF06, 0) & 0xFF, v(n, 0xA, 0) & 0xFFFF);
        if (t == 6) imprimir(" volumen %02x", v(n, 0xF0F, 0) & 0xFF);
        imprimir("\n");
        if (t == 4) {
            uint32_t pc = par(n, 0x0C), cfg = v(n, 0xF1C, 0);
            imprimir("       config %08x %s %s %s loc %02x asoc %u sec %u%s; pincap %08x ctl %02x",
                     cfg, disp_nome(cfg), conexao_nome(cfg), cor_nome(cfg), (cfg >> 24) & 0x3F, (cfg >> 4) & 0xF,
                     cfg & 0xF, (cfg >> 8) & 1 ? " sin-deteccion" : "", pc, v(n, 0xF07, 0) & 0xFF);
            if (pc & (1u << 16)) imprimir(" eapd %x", v(n, 0xF0C, 0) & 0xFF);
            if (pc & (1u << 2)) {
                if (pc & (1u << 1)) { v(n, 0x709, 0); dormir(1); }
                uint32_t s = v(n, 0xF09, 0);
                imprimir(" sensor %08x%s", s, (s & 0x80000000u) ? " (enchufado)" : "");
            }
            imprimir("\n");
        }
    }
}

/* ------------------------------------------------------------ tono de prueba */

/* seno aproximado (Bhaskara I) en grados, escala 32767 */
static int seno(unsigned g)
{
    g %= 360;
    int s = 1;
    if (g >= 180) { g -= 180; s = -1; }
    int x = (int)g * (180 - (int)g);
    return s * (32767 * 4 * x / (40500 - x));
}

static void tocar_tom(const char *que, unsigned freq, bool esq, bool dir, unsigned ms)
{
    enum { TAXA = 48000 };
    static int16_t buf[2 * 4800];
    imprimir("  %s: %u Hz, %s\n", que, freq, esq && dir ? "los dos canales" : esq ? "solo el izquierdo" : "solo el derecho");
    int fd = audio_abrir(TAXA, 2, 16);
    if (fd < 0) { imprimir_em(ERROS, "sonido: no se pudo abrir un flujo (%d)\n", fd); return; }
    unsigned total = TAXA * ms / 1000, feito = 0, fase = 0;   /* fase en milésimas de grado */
    while (feito < total) {
        unsigned n = total - feito < 4800 ? total - feito : 4800;
        for (unsigned k = 0; k < n; k++) {
            int s = seno(fase / 1000) / 2;                     /* -6 dB */
            buf[2 * k] = (int16_t)(esq ? s : 0);
            buf[2 * k + 1] = (int16_t)(dir ? s : 0);
            fase = (fase + freq * 360000u / TAXA) % 360000u;
        }
        audio_escrever(fd, buf, n * 4);
        feito += n;
    }
    audio_fechar(fd, 1);
}

static void tono(void)
{
    imprimir("Tono de prueba:\n");
    tocar_tom("graves (subwoofer)", 100, true, true, 1500);
    tocar_tom("agudos (altavoces)", 1000, true, true, 1500);
    tocar_tom("izquierda", 1000, true, false, 1000);
    tocar_tom("derecha", 1000, false, true, 1000);
}

static void depois(void)
{
    if (com_tono) tono();
}

/* ------------------------------------------------------------ principal */

static int uso(void)
{
    imprimir_em(ERROS, "uso: sonido [-n] [--codec | --tono | --gpio DATOS [MASCARA] | --pin NODO on|off |\n"
                       "              --eapd NODO on|off | --auto on|off | --verbo NODO VERBO CARGA]\n"
                       "     (numeros en hexadecimal; -n: sin tono despues de un cambio)\n");
    return 1;
}

int principal(int argc, char **argv)
{
    int a = 1;
    if (a < argc && compara_texto(argv[a], "-n") == 0) { com_tono = false; a++; }
    if (a < argc && (compara_texto(argv[a], "--ayuda") == 0 || compara_texto(argv[a], "-h") == 0)) return uso(), 0;
    if (a < argc && compara_texto(argv[a], "--tono") == 0) {
        if (audio_volume_mestre(-1) < 0) { imprimir_em(ERROS, "sonido: no hay tarjeta de sonido\n"); return 1; }
        tono();
        return 0;
    }
    info_hda_t i;
    if (!estado(&i)) return 1;
    if (a >= argc) { resumo(&i); return 0; }
    const char *op = argv[a++];
    unsigned x, y, z;
    if (compara_texto(op, "--codec") == 0) {
        volcar(&i);
        return 0;
    }
    if (compara_texto(op, "--gpio") == 0) {
        if (a >= argc || !hex(argv[a], &x) || x > 0xFF) return uso();
        y = x;
        if (a + 1 < argc && (!hex(argv[a + 1], &y) || y > 0xFF)) return uso();
        if (!y) { imprimir_em(ERROS, "sonido: la mascara no puede ser 0\n"); return 1; }
        audio_hda(HDA_OP_GPIO, x, y);
        estado(&i);
        imprimir("GPIO: mascara %02x dir %02x datos %02x\n", i.gpio_mascara, i.gpio_direcao, i.gpio_dados);
        depois();
        return 0;
    }
    if (compara_texto(op, "--pin") == 0 || compara_texto(op, "--eapd") == 0) {
        int on = a + 1 < argc ? liga_desliga(argv[a + 1]) : -1;
        if (a >= argc || !hex(argv[a], &x) || x > 0x7F || on < 0) return uso();
        if (op[2] == 'p') {
            int r = audio_hda(HDA_OP_PINO, x, (unsigned)on);
            if (r < 0) {
                imprimir_em(ERROS, "sonido: el nodo %02x no es un pin%s\n", x, r == ERRO_NAO_EXISTE ? " con camino a un DAC" : "");
                return 1;
            }
            imprimir("Pin %02x %s (ctl %02x)\n", x, on ? "encendido" : "apagado", v(x, 0xF07, 0) & 0xFF);
        } else {
            uint32_t e = v(x, 0xF0C, 0);
            if (e == 0xFFFFFFFFu) { imprimir_em(ERROS, "sonido: el codec no responde\n"); return 1; }
            e = on ? (e | 2) : (e & ~2u);
            v(x, 0x70C, e & 0xFF);
            imprimir("Pin %02x: EAPD %x\n", x, v(x, 0xF0C, 0) & 0xFF);
        }
        depois();
        return 0;
    }
    if (compara_texto(op, "--auto") == 0) {
        int on = a < argc ? liga_desliga(argv[a]) : -1;
        if (on < 0) return uso();
        audio_hda(HDA_OP_AUTOSILENCIO, (unsigned)on, 0);
        imprimir("Autosilencio: %s\n", on ? "si" : "no");
        return 0;
    }
    if (compara_texto(op, "--verbo") == 0) {
        if (a + 2 >= argc || !hex(argv[a], &x) || !hex(argv[a + 1], &y) || !hex(argv[a + 2], &z) || x > 0x7F ||
            y > 0xFFF || (y > 0xF && z > 0xFF) || z > 0xFFFF)
            return uso();
        imprimir("Nodo %02x verbo %x carga %x -> %08x\n", x, y, z, v(x, y, z));
        return 0;
    }
    return uso();
}
