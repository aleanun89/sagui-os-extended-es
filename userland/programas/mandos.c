// SPDX-License-Identifier: GPL-2.0-or-later
/* mandos - mandos y joysticks desde el terminal (D307; docs/mandos.md).
 *
 *   mandos [--lista]              los mandos conectados (USB, Xbox 360, puerto de juegos)
 *   mandos --probar [N] [SEG]     el estado en vivo del mando N (0) durante SEG segundos (15)
 *   mandos --estado [N]           una lectura del mando N, en una línea
 *   mandos --detectar             vuelve a buscar el joystick del puerto de juegos
 *   mandos --calibrar [SEG]       calibra el del puerto: mover a las esquinas y soltar en el centro
 *   mandos --calibracion          muestra la calibración del puerto de juegos
 *   mandos --calibracion auto     vuelve a la automática (y la quita de sagui.conf)
 *   mandos --calibracion VALORES  la pone y la guarda («min,centro,max/» por eje, en décimas de us)
 *   mandos --aplicar              pone la calibración guardada (lo hace «configuracion --aplicar»)
 *   mandos --inyectar N HEX       informe crudo para un mando de prueba (imagen --mandos-prueba)
 *   mandos --crudo [N] [SEG]      descriptor de informe e informes crudos (diagnóstico de mandos USB)
 *
 * La calibración se guarda en /home/.config/sagui.conf (clave mando_puerto=) si /home persiste. */
#include "sagui.h"
#include "mandos.h"
#include "conf_maquina.h"

static void ayuda(void)
{
    imprimir("uso: mandos [--lista | --probar [N] [SEG] | --estado [N] | --detectar | --calibrar [SEG]\n"
             "              | --calibracion [auto | VALORES] | --aplicar | --inyectar N HEX\n"
             "              | --crudo [N] [SEG]]\n"
             "  Mandos USB (HID y Xbox 360 con cable) y el joystick del puerto de juegos (0x201).\n"
             "  Los ejes van de -32768 a 32767; la calibración del puerto se guarda en sagui.conf.\n");
}

static void hats_texto(unsigned h, char *s)
{
    s[0] = 0;
    if (!h) { copia_texto(s, "centro", 16); return; }
    if (h & MANDO_HAT_CIMA) junta_texto(s, "arriba", 16);
    if (h & MANDO_HAT_BAIXO) junta_texto(s, "abajo", 16);
    if (h & MANDO_HAT_ESQUERDA) { if (s[0]) junta_texto(s, "-", 16); junta_texto(s, "izq", 16); }
    if (h & MANDO_HAT_DIREITA) { if (s[0]) junta_texto(s, "-", 16); junta_texto(s, "der", 16); }
}

static int listar_mandos(void)
{
    info_mando_t l[MANDOS_MAX];
    unsigned gen = 0;
    int n = mandos_listar(l, MANDOS_MAX, &gen);
    if (n < 0) { imprimir_em(ERROS, "mandos: el sistema no responde (%d)\n", n); return 1; }
    if (!n) { imprimir("No hay ningún mando ni joystick conectado.\n"); return 0; }
    for (int i = 0; i < n; i++) {
        imprimir("%d: %s\n   %s por %s; %u ejes, %u botones, %u hats", i, l[i].nome, mando_nome_tipo(l[i].tipo), l[i].via,
                 l[i].n_eixos, l[i].n_botoes, l[i].n_hats);
        if (l[i].fabricante || l[i].produto) imprimir("; %04x:%04x", l[i].fabricante, l[i].produto);
        if (l[i].mapa == MANDO_MAPA_XBOX360) imprimir("; mapeo Xbox 360");
        else if (l[i].mapa == MANDO_MAPA_DUALACTION) imprimir("; mapeo Logitech");
        imprimir("\n");
    }
    return 0;
}

static int linha_estado(int k, const info_mando_t *m, const estado_mando_t *e, const unsigned short *crudo)
{
    char t[300];
    int u = formatar_texto(t, sizeof t, "mando %d: ejes", k);
    for (int i = 0; i < m->n_eixos && i < MANDO_EIXOS_MAX; i++) u += formatar_texto(t + u, sizeof t - (unsigned)u, " %d", e->eixos[i]);
    u += formatar_texto(t + u, sizeof t - (unsigned)u, "; botones");
    if (!e->botoes) u += formatar_texto(t + u, sizeof t - (unsigned)u, " ninguno");
    for (int b = 0; b < 32; b++) if (e->botoes & (1u << b)) u += formatar_texto(t + u, sizeof t - (unsigned)u, " %d", b + 1);
    for (int h = 0; h < m->n_hats && h < MANDO_HATS_MAX; h++) {
        char s[16];
        hats_texto(e->hats[h], s);
        u += formatar_texto(t + u, sizeof t - (unsigned)u, "; hat %d %s", h, s);
    }
    if (m->tipo == MANDO_TIPO_PUERTO && crudo) {
        u += formatar_texto(t + u, sizeof t - (unsigned)u, "; tiempos");
        for (int i = 0; i < 4; i++) {
            if (crudo[i] == 0xFFFF) u += formatar_texto(t + u, sizeof t - (unsigned)u, " -");
            else u += formatar_texto(t + u, sizeof t - (unsigned)u, " %u.%u", crudo[i] / 10, crudo[i] % 10);
        }
        u += formatar_texto(t + u, sizeof t - (unsigned)u, " us");
    }
    imprimir("%s\n", t);
    return 0;
}

static void imprimir_hex(const char *pre, const unsigned char *b, unsigned n)
{
    imprimir("%s", pre);
    for (unsigned i = 0; i < n; i++) imprimir(" %02x", b[i]);
    imprimir("\n");
}

static int obter(int k, info_mando_t *m);

/* Diagnóstico de un mando USB: el descriptor de informe y, durante SEG segundos, cada informe
 * crudo nuevo con los ejes que salen de él. Con eso se ve en una máquina real si un eje mal
 * leído es culpa del analizador (kernel/lib/hid_informe.c) o del propio mando. */
static int crudo(int k, int seg)
{
    info_mando_t m;
    if (obter(k, &m) < 0) return 1;
    static unsigned char d[1024];
    int total = mando_descriptor(k, d, sizeof d);
    if (total < 0) { imprimir_em(ERROS, "mandos: no se puede leer el descriptor (%d)\n", total); return 1; }
    imprimir("%s (%04x:%04x): descriptor de informe de %d bytes\n", m.nome, m.fabricante, m.produto, total);
    for (int i = 0; i < total && i < (int)sizeof d; i += 16) {
        char pre[16];
        formatar_texto(pre, sizeof pre, "  %3d:", i);
        imprimir_hex(pre, d + i, (unsigned)(total - i < 16 ? total - i : 16));
    }
    imprimir("Informes (cambia cuando se mueve o se pulsa algo; %d s):\n", seg);
    unsigned char ant[64], inf[64];
    unsigned n_ant = 1000, n = 0;
    unsigned fim = tempo_ms() + (unsigned)seg * 1000u;
    do {
        estado_mando_t e;
        if (mando_ler_informe(k, &e, inf, &n) < 0) { imprimir("El mando se ha desconectado.\n"); return 1; }
        if (n != n_ant || memcmp(inf, ant, n) != 0) {
            imprimir_hex("informe:", inf, n);
            linha_estado(k, &m, &e, NULL);
            memcpy(ant, inf, n);
            n_ant = n;
        }
        if (seg > 0) dormir(50);
    } while ((int)(fim - tempo_ms()) > 0);
    return 0;
}

static int obter(int k, info_mando_t *m)
{
    info_mando_t l[MANDOS_MAX];
    int n = mandos_listar(l, MANDOS_MAX, NULL);
    if (k < 0 || k >= n) { imprimir_em(ERROS, "mandos: no hay mando %d (hay %d)\n", k, n < 0 ? 0 : n); return -1; }
    *m = l[k];
    return 0;
}

static int estado(int k)
{
    info_mando_t m;
    if (obter(k, &m) < 0) return 1;
    estado_mando_t e;
    unsigned short c[4];
    if (mando_ler_crudo(k, &e, c) < 0) { imprimir_em(ERROS, "mandos: no se puede leer el mando %d\n", k); return 1; }
    return linha_estado(k, &m, &e, c);
}

static int probar(int k, int seg)
{
    info_mando_t m;
    if (obter(k, &m) < 0) return 1;
    imprimir("%s: mueve los ejes y pulsa los botones (%d s).\n", m.nome, seg);
    estado_mando_t ant;
    memset(&ant, 0xFF, sizeof ant);
    unsigned fim = tempo_ms() + (unsigned)seg * 1000u;
    while ((int)(fim - tempo_ms()) > 0) {
        estado_mando_t e;
        unsigned short c[4];
        if (mando_ler_crudo(k, &e, c) < 0) { imprimir("El mando se ha desconectado.\n"); return 1; }
        e.informes = 0;
        if (memcmp(&e, &ant, sizeof e) != 0) { linha_estado(k, &m, &e, c); ant = e; }
        dormir(50);
    }
    return 0;
}

static void mostrar_cal(void)
{
    unsigned short c[4][3];
    int manual = 0;
    if (mandos_calibracao(c, &manual) < 0) { imprimir("No se puede leer la calibración.\n"); return; }
    imprimir("Calibración del puerto de juegos (%s), décimas de us: mínimo / centro / máximo\n", manual ? "a mano" : "automática");
    static const char *const nom[4] = { "X1", "Y1", "X2", "Y2" };
    for (int i = 0; i < 4; i++) imprimir("  %s: %u / %u / %u\n", nom[i], c[i][0], c[i][1], c[i][2]);
}

static int guardar_aviso(int g)
{
    if (g == 1) imprimir("/home no es un disco persistente: vale hasta apagar.\n");
    else if (g < 0) { imprimir_em(ERROS, "mandos: no se ha podido guardar (%d)\n", g); return 1; }
    else imprimir("Guardado en %s.\n", maquina_separada() ? "/etc/sistema.conf" : "/home/.config/sagui.conf");   /* D371 */
    return 0;
}

/* Calibración interactiva del joystick del puerto de juegos: durante SEG segundos se recogen
 * el mínimo y el máximo de cada eje; al final, el centro con el mando suelto. */
static int calibrar(int seg)
{
    info_mando_t l[MANDOS_MAX];
    int n = mandos_listar(l, MANDOS_MAX, NULL), k = -1;
    for (int i = 0; i < n; i++) if (l[i].tipo == MANDO_TIPO_PUERTO) k = i;
    if (k < 0) { imprimir_em(ERROS, "mandos: no hay joystick en el puerto de juegos (mandos --detectar)\n"); return 1; }
    unsigned short c[4][3];
    for (int i = 0; i < 4; i++) { c[i][0] = 0xFFFF; c[i][1] = 0; c[i][2] = 0; }
    imprimir("Mueve el joystick hasta las cuatro esquinas, varias veces, durante %d s...\n", seg);
    unsigned fim = tempo_ms() + (unsigned)seg * 1000u;
    estado_mando_t e;
    unsigned short t[4];
    while ((int)(fim - tempo_ms()) > 0) {
        if (mando_ler_crudo(k, &e, t) == 0)
            for (int i = 0; i < 4; i++) {
                if (t[i] == 0xFFFF) continue;
                if (t[i] < c[i][0]) c[i][0] = t[i];
                if (t[i] > c[i][2]) c[i][2] = t[i];
            }
        dormir(25);
    }
    imprimir("Ahora suéltalo en el centro (3 s)...\n");
    dormir(3000);
    unsigned soma[4] = { 0, 0, 0, 0 }, q = 0;
    for (int r = 0; r < 10; r++) {
        if (mando_ler_crudo(k, &e, t) == 0) {
            for (int i = 0; i < 4; i++) soma[i] += t[i] == 0xFFFF ? 0 : t[i];
            q++;
        }
        dormir(30);
    }
    bool ok = false;
    for (int i = 0; i < 4; i++) {
        c[i][1] = q ? (unsigned short)(soma[i] / q) : 0;
        if (c[i][0] == 0xFFFF || !(c[i][0] < c[i][1] && c[i][1] < c[i][2])) { c[i][0] = c[i][1] = c[i][2] = 0; continue; }
        ok = true;
    }
    if (!ok) { imprimir_em(ERROS, "mandos: no se ha movido ningún eje; la calibración no cambia\n"); return 1; }
    mandos_calibrar(c);
    mostrar_cal();
    return guardar_aviso(mandos_conf_gravar(c));
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int principal(int argc, char **argv)
{
    const char *op = argc >= 2 ? argv[1] : "--lista";
    if (compara_texto(op, "--ayuda") == 0 || compara_texto(op, "-h") == 0) { ayuda(); return 0; }
    if (compara_texto(op, "--lista") == 0) return listar_mandos();
    if (compara_texto(op, "--estado") == 0) return estado(argc >= 3 ? texto_para_int(argv[2]) : 0);
    if (compara_texto(op, "--probar") == 0)
        return probar(argc >= 3 ? texto_para_int(argv[2]) : 0, argc >= 4 ? texto_para_int(argv[3]) : 15);
    if (compara_texto(op, "--crudo") == 0)
        return crudo(argc >= 3 ? texto_para_int(argv[2]) : 0, argc >= 4 ? texto_para_int(argv[3]) : 0);
    if (compara_texto(op, "--detectar") == 0) {
        int r = mandos_detectar();
        imprimir(r > 0 ? "Hay un joystick en el puerto de juegos.\n" : "No hay joystick en el puerto de juegos.\n");
        return r > 0 ? 0 : 1;
    }
    if (compara_texto(op, "--calibrar") == 0) return calibrar(argc >= 3 ? texto_para_int(argv[2]) : 8);
    if (compara_texto(op, "--calibracion") == 0) {
        if (argc < 3) { mostrar_cal(); return 0; }
        if (compara_texto(argv[2], "auto") == 0) {
            mandos_calibrar(NULL);
            imprimir("Calibración automática.\n");
            return guardar_aviso(mandos_conf_gravar(NULL));
        }
        unsigned short c[4][3];
        if (mandos_cal_de_texto(argv[2], c) < 0) {
            imprimir_em(ERROS, "mandos: calibración mal escrita (min,centro,max/min,centro,max/... para los 4 ejes)\n");
            return 2;
        }
        mandos_calibrar(c);
        mostrar_cal();
        return guardar_aviso(mandos_conf_gravar(c));
    }
    if (compara_texto(op, "--aplicar") == 0) {
        unsigned short c[4][3];
        if (mandos_conf_ler(c) < 0) return 0;   /* nada guardado: la automática */
        int r = mandos_calibrar(c);
        imprimir("mandos: calibracion del puerto de juegos -> %d\n", r);
        return 0;
    }
    if (compara_texto(op, "--inyectar") == 0 && argc >= 4) {
        uint8_t b[64];
        unsigned n = 0;
        for (const char *s = argv[3]; s[0] && s[1] && n < sizeof b; s += 2) {
            int a = hexval(s[0]), c = hexval(s[1]);
            if (a < 0 || c < 0) { imprimir_em(ERROS, "mandos: hexadecimal mal escrito\n"); return 2; }
            b[n++] = (uint8_t)(a << 4 | c);
        }
        int r = mandos_inyectar(texto_para_int(argv[2]), b, n);
        if (r < 0) { imprimir_em(ERROS, "mandos: informe rechazado (%d)\n", r); return 1; }
        return 0;
    }
    ayuda();
    return 2;
}
