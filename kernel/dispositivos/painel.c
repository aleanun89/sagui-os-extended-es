// SPDX-License-Identifier: GPL-3.0-only
/* painel.c - paneles táctiles PS/2 (touchpads) en modo absoluto, con gestos (D407).
 *
 * Hasta la v8.5.5 el panel de un portátil se usaba como un ratón PS/2: el firmware del panel hacía
 * el toque = clic (si lo hacía) y no había desplazamiento. Aquí, al arrancar (i8042.c, tras 0xF6 y
 * antes del «golpe» del IntelliMouse), se pregunta al ratón quién es:
 *   1. «golpe mágico» de Elantech (E6 E6 E6 E9 = 3C 03 C8) -> versión del firmware -> hardware v2, v3 o v4;
 *   2. «identificar» de Synaptics (orden especial 0x00 + E9: el byte del medio es 0x47) -> capacidades,
 *      capacidades extendidas, máximos y modelo; hace falta la versión 4 o más y «capacidades extendidas»;
 *   3. «informe E6» de ALPS (00 00 0A/64) -> «informe E7»: la firma de un modelo con protocolo v2.
 * Si se reconoce y se puede, se pasa al modo absoluto (paquetes de 6 bytes) y los paquetes van a
 * painel_logica.h, que los decodifica y hace los gestos; lo que sale entra en la cola del ratón por
 * mouse_painel (movimiento, botones, rueda vertical y horizontal). Si no se reconoce, una orden no
 * responde o la persona eligió «modo PS/2», el ratón queda con sus valores por defecto y todo sigue
 * como siempre (rueda IntelliMouse incluida).
 *
 * Vigilancia: si llegan más bytes que no encajan en el formato que válidos (el panel no entró de verdad
 * en el modo absoluto, por ejemplo), se deja de decodificar como panel y los bytes vuelven al ratón
 * PS/2 de 3 bytes: un panel mal reconocido nunca deja el sistema sin ratón. Ver D407. */
#include "painel.h"
#include "painel_logica.h"
#include "abi.h"
#include "console.h"
#include "cpu.h"
#include "i8042.h"
#include "mouse.h"
#include "relogio.h"
#include "sistema.h"
#include "teclado.h"
#include "texto.h"

uint32_t hid_ratos(void);   /* usb/hid.c */

static painel_modelo_t modelo;   /* lo reconocido (aunque se use como ratón PS/2) */
static int  reconhecido;         /* PAINEL_* */
static bool ativo;               /* los bytes del ratón son paquetes absolutos del panel */
static bool forcar_ps2;          /* la persona eligió el modo PS/2 */
static bool caido;               /* se volvió al PS/2 porque los paquetes no encajaban */
static bool pedir_reinicio;      /* y falta reiniciar el ratón para dejarlo en relativo (en el próximo tic) */
static bool sem_usb;             /* no usar el panel con un ratón USB enchufado */
static painel_conf_t conf;
static bool conf_pronta;
static painel_decod_t dec;
static painel_gestos_t ges;
static uint8_t  pac[6];
static int      pos;
static uint32_t tique_byte;
static uint32_t pacotes, bytes_ok, bytes_mal;   /* bytes_*: la ventana de la vigilancia */
static uint32_t alps_firma;
static char     nome[40] = "nenhum";

/* ------------------------------------------------------------ órdenes al panel */

static bool cmd(uint8_t c) { return i8042_rato_cmd(c) == 0xFA; }

/* E9 (estado): ACK y tres bytes. */
static bool estado3(uint8_t r[3])
{
    if (!cmd(0xE9)) return false;
    for (int i = 0; i < 3; i++) {
        int v = i8042_rato_ler();
        if (v < 0) return false;
        r[i] = (uint8_t)v;
    }
    return true;
}

/* Orden especial de Synaptics (y Elantech v2): el argumento en cuatro E8 de dos bits. */
static bool especial(uint8_t arg)
{
    uint8_t a[4];
    syn_argumentos(arg, a);
    for (int i = 0; i < 4; i++)
        if (!cmd(0xE8) || !cmd(a[i])) return false;
    return true;
}

static bool consulta_syn(uint8_t q, uint8_t r[3]) { return especial(q) && estado3(r); }

/* La misma orden tres veces y E9 (informes E6 y E7 de ALPS, golpe mágico de Elantech). */
static bool tres_vezes(uint8_t c, uint8_t r[3]) { return cmd(c) && cmd(c) && cmd(c) && estado3(r); }

/* Consulta de Elantech v3/v4: F8, la consulta, E9. */
static bool consulta_elan(uint8_t q, uint8_t r[3]) { return cmd(0xF8) && cmd(q) && estado3(r); }

/* Escribir un registro de Elantech (según la versión del hardware). */
static bool elan_registro(int hw, uint8_t reg, uint8_t val)
{
    if (hw == 2)
        return cmd(0xF8) && cmd(0x11) && cmd(0xF8) && cmd(reg) && cmd(0xF8) && cmd(val) && cmd(0xE6);
    if (hw == 3)
        return cmd(0xF8) && cmd(0x00) && cmd(0xF8) && cmd(reg) && cmd(0xF8) && cmd(val) && cmd(0xE6);
    return cmd(0xF8) && cmd(0x00) && cmd(0xF8) && cmd(reg) && cmd(0xF8) && cmd(0x00) && cmd(0xF8) && cmd(val) && cmd(0xE6);
}

/* El ratón como tras 0xF6; con «forte», antes un reinicio (0xFF: deja el panel en relativo). */
static void restaurar(bool forte)
{
    if (forte && cmd(0xFF)) { i8042_rato_ler(); i8042_rato_ler(); }   /* AA 00 */
    cmd(0xF6);
}

/* ------------------------------------------------------------ reconocer */

static void reconhecer(void)
{
    uint8_t r6[3], r[3];
    bool e6 = tres_vezes(0xE6, r6);
    if (e6 && elan_magico(r6)) {
        uint8_t fw[3], id[3];
        if (!consulta_syn(0x01, fw)) { klog("[PAINEL] Elantech: no responde a la version del firmware\n"); return; }
        uint32_t f = (uint32_t)fw[0] << 16 | (uint32_t)fw[1] << 8 | fw[2];
        int hw = elan_versao_hw(f);
        bool tem_id = hw >= 3 && consulta_elan(0x00, id);
        if (hw && elan_modelo(&modelo, f, tem_id ? id : NULL)) {
            reconhecido = PAINEL_ELANTECH;
            snformatar(nome, sizeof nome, "Elantech v%d (fw %06x)", hw, f);
        } else {
            snformatar(nome, sizeof nome, "Elantech fw %06x (no soportado)", f);
        }
        klog("[PAINEL] Elantech: firmware %06x, hardware v%d, %dx%d%s\n", f, hw, modelo.xmax, modelo.ymax,
             reconhecido ? "" : ": se queda como raton PS/2 (v1 o desconocido)");
        return;
    }
    int ma, me;
    if (consulta_syn(0x00, r) && syn_identificar(r, &ma, &me)) {
        uint8_t c[3], e[3] = { 0, 0, 0 }, mx[3], mo[3] = { 0, 0, 0 };
        if (ma < 4 || !consulta_syn(0x02, c)) {
            snformatar(nome, sizeof nome, "Synaptics %d.%d (antiguo)", ma, me);
            klog("[PAINEL] Synaptics %d.%d: version anterior a la 4, se queda como raton PS/2\n", ma, me);
            return;
        }
        uint32_t cap = (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];
        if (!SYN_CAP_EXTENDIDA(cap)) {
            snformatar(nome, sizeof nome, "Synaptics %d.%d (sin W)", ma, me);
            klog("[PAINEL] Synaptics %d.%d: capacidades %06x sin modo W, se queda como raton PS/2\n", ma, me, cap);
            return;
        }
        uint32_t e0c = 0;
        if (SYN_CAP_N_CONSULTAS(cap) >= 4 && consulta_syn(0x0C, e))
            e0c = (uint32_t)e[0] << 16 | (uint32_t)e[1] << 8 | e[2];
        bool tem_max = SYN_E0C_MAXIMOS(e0c) && consulta_syn(0x0D, mx);
        consulta_syn(0x03, mo);
        syn_modelo(&modelo, ma, me, cap, e0c, tem_max ? mx : NULL);
        reconhecido = PAINEL_SYNAPTICS;
        snformatar(nome, sizeof nome, "Synaptics %d.%d", ma, me);
        klog("[PAINEL] Synaptics %d.%d: modelo %02x%02x%02x, capacidades %06x, ext0c %06x, x %d-%d y 0-%d%s%s%s%s\n",
             ma, me, mo[0], mo[1], mo[2], cap, e0c, modelo.xmin, modelo.xmax, modelo.ymax,
             modelo.multidedo ? ", varios dedos" : "", modelo.agm ? ", AGM (dos dedos)" : "",
             modelo.boton_medio ? ", boton central" : "", modelo.passthrough ? ", palanca" : "");
        return;
    }
    if (e6 && alps_e6(r6)) {
        uint8_t e7[3];
        cmd(0xF6);   /* lo de Synaptics cambió la resolución */
        if (!tres_vezes(0xE7, e7)) { klog("[PAINEL] ALPS: sin informe E7\n"); return; }
        alps_firma = (uint32_t)e7[0] << 16 | (uint32_t)e7[1] << 8 | e7[2];
        if (alps_modelo(&modelo, e7)) {
            reconhecido = PAINEL_ALPS;
            snformatar(nome, sizeof nome, "ALPS v2 (%02x %02x %02x)", e7[0], e7[1], e7[2]);
        } else {
            snformatar(nome, sizeof nome, "ALPS %02x %02x %02x (no soportado)", e7[0], e7[1], e7[2]);
        }
        klog("[PAINEL] ALPS: E6 %02x %02x %02x, E7 %02x %02x %02x%s\n", r6[0], r6[1], r6[2], e7[0], e7[1], e7[2],
             reconhecido ? ": protocolo v2" : ": no es un v2 conocido, se queda como raton PS/2");
    }
}

/* Al modo absoluto. false = algo no respondió (quien llama reinicia el ratón). */
static bool ligar_absoluto(void)
{
    switch (reconhecido) {
    case PAINEL_SYNAPTICS: {
        if (!especial(SYN_MODO_SAGUI) || !cmd(0xF3) || !cmd(0x14)) return false;   /* byte de modo + tasa 20 */
        if (modelo.agm && !(especial(0x03) && cmd(0xF3) && cmd(0xC8))) {
            modelo.agm = false;
            klog("[PAINEL] Synaptics: no acepto el modo de dos dedos (AGM); sigue sin el\n");
        }
        uint8_t r[3];
        /* «leer modos» (0x01): 3B 47 y el byte de modo. Si responde y no es el pedido, no entró. */
        if (consulta_syn(0x01, r) && r[1] == 0x47 && r[2] != SYN_MODO_SAGUI) {
            klog("[PAINEL] Synaptics: modo leido %02x en vez de %02x\n", r[2], SYN_MODO_SAGUI);
            return false;
        }
        return true;
    }
    case PAINEL_ALPS:
        /* «golpe»: cuatro F5 y F4; luego modo remoto (F0) y otra vez flujo (EA) */
        return cmd(0xF5) && cmd(0xF5) && cmd(0xF5) && cmd(0xF5) && cmd(0xF4) && cmd(0xF0) && cmd(0xEA);
    case PAINEL_ELANTECH:
        if (modelo.versao == 2)
            return elan_registro(2, 0x10, 0x54) && elan_registro(2, 0x11, 0x88) && elan_registro(2, 0x21, 0x60);
        if (modelo.versao == 3) return elan_registro(3, 0x10, 0x0B);
        return elan_registro(4, 0x07, 0x01);
    default:
        return false;
    }
}

bool painel_detectar(bool permitir)
{
    if (!conf_pronta) { painel_conf_padrao(&conf); sem_usb = false; conf_pronta = true; }
    memset(&modelo, 0, sizeof modelo);
    reconhecido = PAINEL_NENHUM;
    ativo = false;
    caido = false;
    pos = 0;
    alps_firma = 0;
    snformatar(nome, sizeof nome, "nenhum");
    reconhecer();
    if (!reconhecido) { restaurar(false); return false; }
    if (!permitir || forcar_ps2) {
        restaurar(false);
        klog("[PAINEL] %s: modo PS/2 de siempre (%s)\n", nome, forcar_ps2 ? "lo pidio la persona" : "sin modo absoluto");
        return false;
    }
    if (!ligar_absoluto()) {
        klog("[PAINEL] %s: el modo absoluto no respondio; vuelve a raton PS/2\n", nome);
        restaurar(true);
        return false;
    }
    painel_decod_iniciar(&dec, &modelo);
    painel_gestos_iniciar(&ges);
    pacotes = bytes_ok = bytes_mal = 0;
    ativo = true;
    klog("[PAINEL] %s en modo absoluto: toque %d, dos dedos %d, borde %d, natural %d, sensibilidad %d, al escribir %d ms\n",
         nome, conf.toque, conf.dois_dedos, conf.borda, conf.natural, conf.sensib, conf.ms_teclado);
    return true;
}

void painel_desligar(void)
{
    if (ativo) klog("[PAINEL] %s: los bytes vuelven al raton PS/2\n", nome);
    ativo = false;
    pos = 0;
}

bool painel_ativo(void) { return ativo; }
const char *painel_nome(void) { return nome; }

/* ------------------------------------------------------------ los paquetes */

static void entregar(const painel_saida_t *s)
{
    for (int i = 0; i < s->n; i++)
        mouse_painel(s->e[i].dx, s->e[i].dy, s->e[i].rv, s->e[i].rh, s->e[i].botoes);
}

/* Vigilancia: en ventanas de 300 bytes, si se tiraron más de los que encajaron, el panel no habla el
 * formato esperado: los bytes vuelven al ratón PS/2 (paquetes de 3 bytes, sin rueda) y en el próximo tic se
 * reinicia el ratón (0xFF: relativo seguro, con su rueda), por si el panel sí estaba en absoluto pero en una
 * variante que no conocemos. */
static void vigiar(void)
{
    if (bytes_ok + bytes_mal < 300) return;
    if (bytes_mal > bytes_ok) {
        klog("[PAINEL] %s: %u bytes fuera de formato y %u validos: vuelve al raton PS/2 (D407)\n", nome, bytes_mal, bytes_ok);
        caido = true;
        ativo = false;
        pedir_reinicio = true;
        pos = 0;
        mouse_formato(0);
        /* soltar lo que tuviera pulsado un toque */
        mouse_painel(0, 0, 0, 0, 0);
    }
    bytes_ok = bytes_mal = 0;
}

void painel_byte(uint8_t b)
{
    uint32_t agora = relogio_tiques();
    if (pos > 0 && agora - tique_byte > 5) pos = 0;   /* paquete a medias de hace más de 50 ms */
    tique_byte = agora;
    if (!painel_byte_ok(&modelo, pos, b)) {
        bytes_mal += (uint32_t)pos + 1;
        pos = 0;
        if (painel_byte_ok(&modelo, 0, b)) { pac[pos++] = b; bytes_mal--; }   /* quizá empieza otro */
        vigiar();
        return;
    }
    pac[pos++] = b;
    if (pos < 6) return;
    pos = 0;
    pacotes++;
    painel_quadro_t q;
    int r = painel_decodificar(&dec, pac, &q);
    if (r < 0) bytes_mal += 6; else bytes_ok += 6;
    if (r == 1) {
        conf.desligado = sem_usb && hid_ratos() > 0;
        painel_saida_t s;
        painel_gestos_quadro(&ges, &modelo, &conf, &q, agora * 10, teclado_ultima_tecla() * 10, &s);
        entregar(&s);
    }
    vigiar();
}

void painel_tique(void)
{
    if (pedir_reinicio) {
        /* Desde el tic del reloj, con las interrupciones desactivadas: tarda un momento (como al arrancar), pero
         * pasa como mucho una vez y deja un ratón que funciona. Sin modo absoluto hasta el próximo arranque o
         * hasta que la persona lo pida («panel auto»). */
        pedir_reinicio = false;
        bool ok = i8042_rato_reiniciar(false);
        caido = true;
        klog("[PAINEL] raton reiniciado en modo PS/2 tras los paquetes fuera de formato (%s)\n", ok ? "responde" : "sin respuesta");
        return;
    }
    if (!ativo) return;
    painel_saida_t s;
    painel_gestos_tique(&ges, relogio_tiques() * 10, &s);
    entregar(&s);
}

/* ------------------------------------------------------------ INFO_PAINEL */

int32_t painel_info(uint32_t ptr, uint32_t n)
{
    if (!conf_pronta) { painel_conf_padrao(&conf); conf_pronta = true; }
    info_painel_t i;
    memset(&i, 0, sizeof i);
    bool ints = int_salva_desliga();
    i.versao = 1;
    i.toque = conf.toque; i.dois_dedos = conf.dois_dedos; i.borda = conf.borda; i.natural = conf.natural;
    i.sensib = (unsigned char)conf.sensib; i.sem_usb = sem_usb; i.modo_ps2 = forcar_ps2;
    i.ms_teclado = (unsigned)conf.ms_teclado;
    i.tipo = ativo ? (unsigned)reconhecido : PAINEL_TIPO_NENHUM;
    i.reconhecido = (unsigned)reconhecido;
    i.versao_hw = (unsigned)modelo.versao;
    i.firmware = reconhecido == PAINEL_ELANTECH ? modelo.firmware : (unsigned)(modelo.versao << 8 | modelo.menor);
    i.capacidades = reconhecido == PAINEL_ALPS ? alps_firma : modelo.capacidades;
    i.xmin = modelo.xmin; i.xmax = modelo.xmax; i.ymin = modelo.ymin; i.ymax = modelo.ymax;
    i.multidedo = modelo.multidedo;
    i.dois_dedos_pos = modelo.agm || reconhecido == PAINEL_ELANTECH;
    i.palanca = modelo.passthrough || modelo.alps_palanca;
    i.pacotes = pacotes; i.invalidos = dec.invalidos;
    i.toques = ges.toques; i.rolagens = ges.rolagens; i.palmas = ges.palmas; i.por_teclado = ges.teclado;
    i.ratos_usb = hid_ratos();
    i.desligado = caido || (ativo && sem_usb && i.ratos_usb > 0);
    txt_copia_n(i.nome, nome, sizeof i.nome);
    int_restaura(ints);
    if (n > sizeof i) n = sizeof i;
    if (!usuario_ok((void *)ptr, n, true)) return ERRO_ENDERECO;
    memcpy((void *)ptr, &i, n);
    return 0;
}

int32_t painel_gravar(uint32_t ptr, uint32_t n)
{
    info_painel_t i;
    if (n < sizeof i || !usuario_ok((void *)ptr, sizeof i, false)) return ERRO_ENDERECO;
    memcpy(&i, (void *)ptr, sizeof i);
    if (i.versao != 1) return ERRO_INVALIDO;
    bool ints = int_salva_desliga();
    if (!conf_pronta) { painel_conf_padrao(&conf); conf_pronta = true; }
    conf.toque = i.toque != 0;
    conf.dois_dedos = i.dois_dedos != 0;
    conf.borda = i.borda != 0;
    conf.natural = i.natural != 0;
    conf.sensib = i.sensib < 1 ? 1 : i.sensib > 10 ? 10 : i.sensib;
    conf.ms_teclado = i.ms_teclado > 2000 ? 2000 : (int)i.ms_teclado;
    sem_usb = i.sem_usb != 0;
    bool ps2 = i.modo_ps2 != 0, mudou_modo = ps2 != forcar_ps2;
    forcar_ps2 = ps2;
    int_restaura(ints);
    klog("[PAINEL] opciones: toque %d, dos dedos %d, borde %d, natural %d, sensibilidad %d, al escribir %d ms, sin USB %d, PS/2 %d\n",
         conf.toque, conf.dois_dedos, conf.borda, conf.natural, conf.sensib, conf.ms_teclado, sem_usb, forcar_ps2);
    /* el modo: solo si hay un panel reconocido y cambia lo que se usa */
    if (mudou_modo && reconhecido && ((ps2 && ativo) || (!ps2 && !ativo))) {
        bool ok = i8042_rato_reiniciar(!ps2);
        klog("[PAINEL] modo %s -> %s (raton %s)\n", ps2 ? "PS/2" : "absoluto", ativo ? "absoluto" : "PS/2", ok ? "bien" : "sin respuesta");
    }
    return 0;
}
