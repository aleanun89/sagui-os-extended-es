// SPDX-License-Identifier: GPL-3.0-only
/* teclado.c - driver del teclado PS/2 (controlador i8042, IRQ 1), layouts
 * español (ES, el por defecto) y ABNT2.
 *
 * El controlador entrega un byte por interrupción en el puerto 0x60. Con la traducción
 * activada por la BIOS (por defecto), los bytes siguen el "scan code set 1": cada tecla
 * tiene un código al ser pulsada y el mismo código + 0x80 al ser soltada; algunas
 * teclas (flechas, Ctrl y Alt de la derecha...) llegan precedidas de 0xE0.
 *
 * El trabajo del driver es transformar esa secuencia en teclas con
 * significado: seguir Shift, Ctrl, Alt, AltGr y Caps Lock, aplicar el
 * mapa del layout elegido (ES o ABNT2, PRD 4.4) y combinar las teclas muertas (´ ` ~ ^ ¨) con la letra
 * siguiente. El resultado va a un búfer circular que lee el terminal. */
#include "teclado.h"
#include "acoes.h"
#include "console.h"
#include "cpu.h"
#include "portas.h"
#include "processo.h"
#include "sistema.h"
#include "texto.h"
#include "relogio.h"

#define PORTA_DADOS  0x60
#define PORTA_ESTADO 0x64
#define TAM_BUFFER   128

/* Mapas: [layout][scan code] = { normal, con Shift }. Las teclas muertas son
 * marcadas con el propio acento sumado a MORTA; 0 = tecla sin carácter.
 * El layout empieza en el español (ES) y cambia con la llamada SIS_TECLADO. */
#define MORTA 0x8000
static const uint16_t mapas[2][0x80][2] = {
    [TECLADO_ES] = {
        [0x29] = {0xBA /* º */, 0xAA /* ª */},
        [0x02] = {'1', '!'}, [0x03] = {'2', '"'}, [0x04] = {'3', 0xB7 /* · */}, [0x05] = {'4', '$'},
        [0x06] = {'5', '%'}, [0x07] = {'6', '&'}, [0x08] = {'7', '/'},
        [0x09] = {'8', '('}, [0x0A] = {'9', ')'}, [0x0B] = {'0', '='}, [0x0C] = {'\'', '?'},
        [0x0D] = {0xA1 /* ¡ */, 0xBF /* ¿ */}, [0x0E] = {'\b', '\b'}, [0x0F] = {'\t', '\t'},
        [0x10] = {'q', 'Q'}, [0x11] = {'w', 'W'}, [0x12] = {'e', 'E'}, [0x13] = {'r', 'R'},
        [0x14] = {'t', 'T'}, [0x15] = {'y', 'Y'}, [0x16] = {'u', 'U'}, [0x17] = {'i', 'I'},
        [0x18] = {'o', 'O'}, [0x19] = {'p', 'P'},
        [0x1A] = {MORTA | '`', MORTA | '^'}, [0x1B] = {'+', '*'}, [0x1C] = {'\n', '\n'},
        [0x1E] = {'a', 'A'}, [0x1F] = {'s', 'S'}, [0x20] = {'d', 'D'}, [0x21] = {'f', 'F'},
        [0x22] = {'g', 'G'}, [0x23] = {'h', 'H'}, [0x24] = {'j', 'J'}, [0x25] = {'k', 'K'},
        [0x26] = {'l', 'L'}, [0x27] = {0xF1 /* ñ */, 0xD1 /* Ñ */},
        [0x28] = {MORTA | 0xB4 /* ´ */, MORTA | 0xA8 /* ¨ */}, [0x2B] = {0xE7 /* ç */, 0xC7 /* Ç */},
        [0x2C] = {'z', 'Z'}, [0x2D] = {'x', 'X'}, [0x2E] = {'c', 'C'}, [0x2F] = {'v', 'V'},
        [0x30] = {'b', 'B'}, [0x31] = {'n', 'N'}, [0x32] = {'m', 'M'}, [0x33] = {',', ';'},
        [0x34] = {'.', ':'}, [0x35] = {'-', '_'}, [0x37] = {'*', '*'}, [0x39] = {' ', ' '},
        [0x4A] = {'-', '-'}, [0x4E] = {'+', '+'},
        [0x56] = {'<', '>'},
    },
    [TECLADO_ABNT2] = {
        [0x02] = {'1', '!'}, [0x03] = {'2', '@'}, [0x04] = {'3', '#'}, [0x05] = {'4', '$'},
        [0x06] = {'5', '%'}, [0x07] = {'6', MORTA | 0xA8 /* ¨ */}, [0x08] = {'7', '&'},
        [0x09] = {'8', '*'}, [0x0A] = {'9', '('}, [0x0B] = {'0', ')'}, [0x0C] = {'-', '_'},
        [0x0D] = {'=', '+'}, [0x0E] = {'\b', '\b'}, [0x0F] = {'\t', '\t'},
        [0x10] = {'q', 'Q'}, [0x11] = {'w', 'W'}, [0x12] = {'e', 'E'}, [0x13] = {'r', 'R'},
        [0x14] = {'t', 'T'}, [0x15] = {'y', 'Y'}, [0x16] = {'u', 'U'}, [0x17] = {'i', 'I'},
        [0x18] = {'o', 'O'}, [0x19] = {'p', 'P'},
        [0x1A] = {MORTA | 0xB4 /* ´ */, MORTA | '`'}, [0x1B] = {'[', '{'}, [0x1C] = {'\n', '\n'},
        [0x1E] = {'a', 'A'}, [0x1F] = {'s', 'S'}, [0x20] = {'d', 'D'}, [0x21] = {'f', 'F'},
        [0x22] = {'g', 'G'}, [0x23] = {'h', 'H'}, [0x24] = {'j', 'J'}, [0x25] = {'k', 'K'},
        [0x26] = {'l', 'L'}, [0x27] = {0xE7 /* ç */, 0xC7 /* Ç */},
        [0x28] = {MORTA | '~', MORTA | '^'}, [0x29] = {'\'', '"'}, [0x2B] = {']', '}'},
        [0x2C] = {'z', 'Z'}, [0x2D] = {'x', 'X'}, [0x2E] = {'c', 'C'}, [0x2F] = {'v', 'V'},
        [0x30] = {'b', 'B'}, [0x31] = {'n', 'N'}, [0x32] = {'m', 'M'}, [0x33] = {',', '<'},
        [0x34] = {'.', '>'}, [0x35] = {';', ':'}, [0x37] = {'*', '*'}, [0x39] = {' ', ' '},
        [0x4A] = {'-', '-'}, [0x4E] = {'+', '+'},
        [0x56] = {'\\', '|'}, [0x73] = {'/', '?'}, [0x7E] = {'.', '.'},
    },
};
static int layout = TECLADO_ES;

/* AltGr (Alt de la derecha): los símbolos grabados en la esquina de las teclas. */
static uint16_t com_altgr(uint8_t sc)
{
    if (layout == TECLADO_ES) {
        switch (sc) {
        case 0x29: return '\\';  case 0x02: return '|';   case 0x03: return '@';
        case 0x04: return '#';   case 0x05: return MORTA | '~'; case 0x07: return 0xAC; /* ¬ */
        case 0x1A: return '[';   case 0x1B: return ']';
        case 0x28: return '{';   case 0x2B: return '}';
        default: return 0;
        }
    }
    switch (sc) {
    case 0x10: return '/';   case 0x11: return '?';   case 0x12: return 0xB0; /* ° */
    case 0x02: return 0xB9;  case 0x03: return 0xB2;  case 0x04: return 0xB3; /* ¹ ² ³ */
    case 0x05: return 0xA3;  case 0x06: return 0xA2;  case 0x07: return 0xAC; /* £ ¢ ¬ */
    case 0x0D: return 0xA7;  case 0x1B: return 0xAA;  case 0x2B: return 0xBA; /* § ª º */
    case 0x73: return 0xB0;
    default: return 0;
    }
}

/* Teclado numérico con Num Lock desactivado y teclas extendidas (0xE0). */
static int navegacao(uint8_t sc)
{
    switch (sc) {
    case 0x48: return TECLA_CIMA;     case 0x50: return TECLA_BAIXO;
    case 0x4B: return TECLA_ESQUERDA; case 0x4D: return TECLA_DIREITA;
    case 0x47: return TECLA_HOME;     case 0x4F: return TECLA_END;
    case 0x49: return TECLA_PGUP;     case 0x51: return TECLA_PGDN;
    case 0x52: return TECLA_INSERT;   case 0x53: return TECLA_DELETE;
    default: return 0;
    }
}

/* Combinación de tecla muerta + letra. Cada línea: acento, letras base y
 * resultados en el mismo orden (Latin-1). */
static const struct { uint8_t acento; const char *base; const char *resultado; } combinacoes[] = {
    { 0xB4, "aeiouyAEIOUY", "\xE1\xE9\xED\xF3\xFA\xFD\xC1\xC9\xCD\xD3\xDA\xDD" },
    { '`',  "aeiouAEIOU",   "\xE0\xE8\xEC\xF2\xF9\xC0\xC8\xCC\xD2\xD9" },
    { '^',  "aeiouAEIOU",   "\xE2\xEA\xEE\xF4\xFB\xC2\xCA\xCE\xD4\xDB" },
    { '~',  "aonAON",       "\xE3\xF5\xF1\xC3\xD5\xD1" },
    { 0xA8, "aeiouyAEIOU",  "\xE4\xEB\xEF\xF6\xFC\xFF\xC4\xCB\xCF\xD6\xDC" },
};

static uint16_t buffer[TAM_BUFFER];
static uint8_t mods_buffer[TAM_BUFFER];   /* modificadores al pulsar cada tecla (D119) */
static volatile uint32_t ini, fim;
static uint32_t mods;
static bool estendida, num_lock;
static uint8_t morta;   /* acento esperando la próxima letra (0 = ninguno) */
/* D251: modificadores del escritorio remoto (inyectados), aparte de los del
 * teclado real para que uno no pise al otro; lo que se ve es la unión. */
static uint32_t mods_iny;

static void guardar(int tecla)
{
    uint32_t prox = (fim + 1) % TAM_BUFFER;
    if (prox == ini)
        return;   /* búfer lleno: la tecla se pierde en vez de sobrescribir las antiguas */
    buffer[fim] = (uint16_t)tecla;
    mods_buffer[fim] = (uint8_t)(mods | mods_iny);
    fim = prox;
    acordar(teclado_canal());   /* quien estaba esperando una tecla vuelve a quedar listo */
}

/* Canal en el que los procesos esperan por teclas (ver bloquear/acordar). */
static const char canal = 0;
const void *teclado_canal(void) { return &canal; }

static void atualizar_leds(void)
{
    /* Comando 0xED + máscara (bit 2 = Caps Lock, bit 1 = Num Lock). La
     * respuesta (0xFA) llega por la IRQ y se ignora allí. */
    for (int i = 0; i < 10000 && (porta_le8(PORTA_ESTADO) & 2); i++) ;
    porta_escreve8(PORTA_DADOS, 0xED);
    for (int i = 0; i < 10000 && (porta_le8(PORTA_ESTADO) & 2); i++) ;
    porta_escreve8(PORTA_DADOS, (uint8_t)(((mods & MOD_CAPS) ? 4 : 0) | (num_lock ? 2 : 0)));
}

static void caractere(int c)
{
    if (morta) {
        uint8_t acento = morta;
        morta = 0;
        if (c == ' ') { guardar(acento); return; }   /* acento + espacio = el propio acento */
        for (unsigned i = 0; i < sizeof combinacoes / sizeof combinacoes[0]; i++) {
            if (combinacoes[i].acento != acento) continue;
            for (int k = 0; combinacoes[i].base[k]; k++)
                if ((uint8_t)combinacoes[i].base[k] == c) {
                    guardar((uint8_t)combinacoes[i].resultado[k]);
                    return;
                }
        }
        guardar(acento);   /* combinación inválida: el acento y después la letra */
    }
    guardar(c);
}

/* Teclas sostenidas ahora (B4): un bit por código de rastreo, 0-127 para
 * las comunes y 128-255 para las extendidas (E0 xx). Los juegos necesitan saber
 * que la flecha sigue pulsada, y no solo que fue pulsada una vez; el
 * camino normal de caracteres no dice cuándo se suelta la tecla. */
static volatile uint8_t seguradas[32];
static uint8_t seguradas_iny[32];   /* D251: las sostenidas desde el escritorio remoto */

void teclado_seguradas(uint8_t mapa[32])
{
    bool i = int_salva_desliga();
    for (int k = 0; k < 32; k++) mapa[k] = (uint8_t)(seguradas[k] | seguradas_iny[k]);
    int_restaura(i);
}

static volatile uint32_t tique_tecla;   /* D407: tic de la última tecla pulsada (sin los modificadores solos) */

uint32_t teclado_ultima_tecla(void) { return tique_tecla; }

void teclado_byte(uint8_t b)
{
    if (b == 0xFA || b == 0xFE || b == 0x00 || b == 0xFF)
        return;   /* respuestas del teclado (ACK, reenviar, error) */
    if (b == 0xE0) { estendida = true; return; }
    if (b == 0xE1) return;   /* Pause: secuencia larga sin uso aquí */

    bool solta = b & 0x80;
    uint8_t sc = b & 0x7F;
    bool ext = estendida;
    estendida = false;
    if (!(ext && sc == 0x2A)) {   /* o "shift falso" de las flechas no cuenta */
        uint8_t k = (uint8_t)(sc | (ext ? 0x80 : 0));
        if (solta) seguradas[k >> 3] &= (uint8_t)~(1u << (k & 7));
        else seguradas[k >> 3] |= (uint8_t)(1u << (k & 7));
    }

    /* Modificadores. */
    uint32_t m = 0;
    if (sc == 0x2A || sc == 0x36) m = MOD_SHIFT;
    else if (sc == 0x1D) m = MOD_CTRL;
    else if (sc == 0x38) m = ext ? MOD_ALTGR : MOD_ALT;
    else if (ext && (sc == 0x5B || sc == 0x5C)) m = MOD_SUPER;   /* tecla Windows (D065, D119) */
    if (m) {
        if (ext && sc == 0x2A) return;   /* "shift falso" que algunos teclados envían junto a las flechas */
        if (solta) mods &= ~m; else mods |= m;
        /* La tecla Windows también entra en el buffer al pulsarla: el gestor de
         * ventanas decide al soltarla si era sola (menú) o Windows+otra (D119). */
        if (m == MOD_SUPER && !solta) guardar(TECLA_SUPER);
        return;
    }
    if (solta)
        return;
    tique_tecla = relogio_tiques();   /* D407: el panel táctil no hace caso un momento tras escribir */

    /* D403: teclas multimedia (silencio E0 20, bajar E0 2E y subir E0 30 el volumen), las del
     * teclado y las del panel táctil de los HP (QuickTouch, D405): no van al búfer, son acciones
     * del sistema (con su aviso en pantalla); aquí solo se encolan (acoes.c). */
    if (ext && (sc == 0x20 || sc == 0x2E || sc == 0x30)) {
        acao_sistema(sc == 0x20 ? ACAO_SILENCIO : sc == 0x2E ? ACAO_VOLUME_MENOS : ACAO_VOLUME_MAIS,
                     ORIGEM_ACAO_TECLADO);
        return;
    }

    /* Impr Pant: E0 37 (Alt+Impr Pant sale como SysRq, 0x54 sin E0). D125 */
    if ((ext && sc == 0x37) || (!ext && sc == 0x54)) { guardar(TECLA_IMPRPANT); return; }
    if (sc == 0x3A) { mods ^= MOD_CAPS; atualizar_leds(); return; }
    if (sc == 0x45) { num_lock = !num_lock; atualizar_leds(); return; }
    if (sc == 0x01) { morta = 0; guardar(TECLA_ESC); return; }
    if (sc >= 0x3B && sc <= 0x44) { guardar(TECLA_F1 + (sc - 0x3B)); return; }
    if (sc == 0x57 || sc == 0x58) { guardar(TECLA_F11 + (sc - 0x57)); return; }

    if (ext) {
        if (sc == 0x1C) { caractere('\n'); return; }   /* Enter del teclado numérico */
        if (sc == 0x35) { caractere('/'); return; }    /* / del teclado numérico */
        int n = navegacao(sc);
        if (n) guardar(n);
        return;
    }
    if (sc >= 0x47 && sc <= 0x53 && sc != 0x4A && sc != 0x4E) {
        if (!num_lock) { int n = navegacao(sc); if (n) guardar(n); return; }
        static const char numerico[] = "789-456+1230,";   /* coma: el "Del" del ABNT2 */
        caractere(sc == 0x53 && layout == TECLADO_ES ? '.' : numerico[sc - 0x47]);
        return;
    }

    uint16_t v;
    const uint16_t (*mapa)[2] = mapas[layout];
    uint8_t base = (uint8_t)mapa[sc][0];
    if (mods & MOD_ALTGR) {
        v = com_altgr(sc);
        if (!v) return;
    } else {
        bool shift = mods & MOD_SHIFT;
        v = mapa[sc][shift ? 1 : 0];
        if (!v)
            return;
        /* Caps Lock invierte solo las letras (y la ç y la ñ). */
        bool letra = (base >= 'a' && base <= 'z') || base == 0xE7 || base == 0xF1;
        if (letra && (mods & MOD_CAPS))
            v = mapa[sc][shift ? 0 : 1];
    }
    if (v & MORTA) {
        if (morta) { uint8_t a = morta; morta = 0; guardar(a); }   /* dos muertas seguidas */
        morta = (uint8_t)(v & 0xFF);
        return;
    }
    if ((mods & MOD_CTRL) && !(mods & MOD_ALTGR) && base >= 'a' && base <= 'z') {
        morta = 0;
        guardar(base & 0x1F);   /* Ctrl+letra: código de control */
        return;
    }
    caractere(v);
}

void teclado_iniciar(void)
{
    /* La lectura del controlador (puertos 0x60/0x64) la hace i8042.c, que
     * entrega cada byte del teclado a teclado_byte(). Ver D020. */
    ini = fim = 0;
    mods = 0;
    num_lock = false;
    atualizar_leds();
}

int teclado_tentar_mods(uint32_t *m)
{
    bool i = int_salva_desliga();
    int t = -1;
    if (ini != fim) {
        t = buffer[ini];
        if (m) *m = mods_buffer[ini];
        ini = (ini + 1) % TAM_BUFFER;
    }
    int_restaura(i);
    return t;
}

int teclado_tentar(void) { return teclado_tentar_mods(NULL); }

/* D317: ¿hay en el búfer, sin sacarla, la tecla "t"? */
bool teclado_contem(int t)
{
    bool i = int_salva_desliga(), achou = false;
    for (uint32_t k = ini; k != fim; k = (k + 1) % TAM_BUFFER)
        if (buffer[k] == t) { achou = true; break; }
    int_restaura(i);
    return achou;
}

int teclado_pendentes(void)
{
    bool i = int_salva_desliga();
    int n = (fim - ini + TAM_BUFFER) % TAM_BUFFER;
    int_restaura(i);
    return n;
}

int teclado_ler(void)
{
    for (;;) {
        int t = teclado_tentar();
        if (t >= 0) return t;
        cpu_espera();   /* duerme hasta la próxima interrupción (tecla o reloj) */
    }
}

uint32_t teclado_modificadores(void) { return mods | mods_iny; }

uint8_t teclado_leds_usb(void) { return (uint8_t)((num_lock ? 1 : 0) | ((mods & MOD_CAPS) ? 2 : 0)); }

int teclado_layout(int novo)
{
    if (novo == TECLADO_ES || novo == TECLADO_ABNT2) {
        bool i = int_salva_desliga();
        layout = novo;
        morta = 0;   /* un acento pendiente del layout antiguo no vale en el nuevo */
        int_restaura(i);
    }
    return layout;
}

/* SIS_TECLADO (layout o -1): cambia el layout y devuelve el actual. */
int32_t sis_teclado(uint32_t novo, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    if ((int32_t)novo != -1 && novo != TECLADO_ES && novo != TECLADO_ABNT2) return ERRO_INVALIDO;
    return teclado_layout((int)novo);
}

/* ------------------------------------------------ entrada inyectada (D251)
 *
 * El escritorio remoto (vncd, mejora 097) no manda códigos de rastreo sino
 * teclas ya traducidas (Latin-1 o TECLA_*) y el estado de sus modificadores.
 * Se meten en el mismo búfer que las del teclado real, con los modificadores
 * de ese momento, así que el gestor de ventanas no las distingue: Alt+Tab,
 * Alt+F4, la tecla Windows y Ctrl+Alt+Supr funcionan igual. Lo único que se
 * guarda aparte es el estado (modificadores y teclas sostenidas), para
 * soltarlo todo si el servidor se va con algo pulsado. */

/* El código de rastreo que daría la tecla en el layout actual, para el mapa de
 * teclas sostenidas (lo usan los juegos); -1 si no hay (letras acentuadas...). */
static int rastreo_de(int t)
{
    switch (t) {
    case TECLA_CIMA: return 0x80 | 0x48;     case TECLA_BAIXO: return 0x80 | 0x50;
    case TECLA_ESQUERDA: return 0x80 | 0x4B; case TECLA_DIREITA: return 0x80 | 0x4D;
    case TECLA_HOME: return 0x80 | 0x47;     case TECLA_END: return 0x80 | 0x4F;
    case TECLA_PGUP: return 0x80 | 0x49;     case TECLA_PGDN: return 0x80 | 0x51;
    case TECLA_INSERT: return 0x80 | 0x52;   case TECLA_DELETE: return 0x80 | 0x53;
    case TECLA_ESC: return 0x01;             case TECLA_SUPER: return 0x80 | 0x5B;
    case TECLA_IMPRPANT: return 0x80 | 0x37;
    case TECLA_F11: return 0x57;             case TECLA_F12: return 0x58;
    case '\n': return 0x1C;  case '\b': return 0x0E;  case '\t': return 0x0F;  case ' ': return 0x39;
    default: break;
    }
    if (t >= TECLA_F1 && t <= TECLA_F10) return 0x3B + (t - TECLA_F1);
    if (t >= 1 && t <= 26) t = 'a' + t - 1;   /* Ctrl+letra: la letra */
    if (t <= 0 || t > 0xFF) return -1;
    for (int sc = 1; sc < 0x80; sc++)
        if (mapas[layout][sc][0] == t || mapas[layout][sc][1] == t) return sc;
    return -1;
}

static void marcar_iny(int k, bool lig)
{
    if (k < 0 || k > 0xFF) return;
    if (lig) seguradas_iny[k >> 3] |= (uint8_t)(1u << (k & 7));
    else seguradas_iny[k >> 3] &= (uint8_t)~(1u << (k & 7));
}

/* tecla: 0 (solo cambian los modificadores) o un código válido (lo comprueba
 * quien llama); m: MOD_SHIFT | MOD_CTRL | MOD_ALT pulsados ahora en el remoto. */
void teclado_injetar(int tecla, uint32_t m, bool pulsada)
{
    bool i = int_salva_desliga();
    mods_iny = (mods_iny & MOD_SUPER) | (m & (MOD_SHIFT | MOD_CTRL | MOD_ALT));
    if (tecla == TECLA_SUPER) {
        if (pulsada) mods_iny |= MOD_SUPER; else mods_iny &= ~(uint32_t)MOD_SUPER;
    }
    marcar_iny(0x2A, mods_iny & MOD_SHIFT);
    marcar_iny(0x1D, mods_iny & MOD_CTRL);
    marcar_iny(0x38, mods_iny & MOD_ALT);
    marcar_iny(0x80 | 0x5B, mods_iny & MOD_SUPER);
    if (tecla) {
        marcar_iny(rastreo_de(tecla), pulsada);
        if (pulsada) {
            int t = tecla;
            /* Ctrl+letra da el código de control, como el teclado real. */
            if ((mods_iny & MOD_CTRL) && ((t >= 'a' && t <= 'z') || (t >= 'A' && t <= 'Z'))) t &= 0x1F;
            guardar(t);
        }
    }
    int_restaura(i);
}

/* El servidor se fue (o soltó el permiso): nada remoto queda pulsado. */
void teclado_injetar_soltar(void)
{
    bool i = int_salva_desliga();
    mods_iny = 0;
    memset(seguradas_iny, 0, sizeof seguradas_iny);
    int_restaura(i);
}
