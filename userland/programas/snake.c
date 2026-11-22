// SPDX-License-Identifier: GPL-3.0-only
/* snake - el juego de la serpiente de SaguiOS (D463).
 *
 * Origen: «aplicaciones-saguios/snake.c», un programa de ejemplo escrito para compilarse con TCC dentro de
 * SaguiOS (proyecto del mantenedor, GPL v3). Aquí usa la libsagui nativa y guarda el récord en el perfil del
 * usuario (/home/.config/snake.conf). La versión sencilla original, para estudiarla, está en /ejemplos/tcc/snake.c.
 *
 * Flechas o WASD cambian de dirección, Espacio empieza o repite, P o Espacio pausan, Esc cierra.
 * Opciones: snake [--semilla N] (partida repetible, para las pruebas) y snake --prueba (reglas del juego).
 * Todo es entero. */
#include "sagui.h"
#include "gui.h"

enum { COLUMNAS = 30, FILAS = 18, CASILLAS = COLUMNAS * FILAS };
enum { CELDA = 18, X_TABLERO = 24, Y_TABLERO = 105, ANCHO = 588, ALTO = 478 };
enum direccion { ARRIBA, ABAJO, IZQUIERDA, DERECHA, SIN_GIRO };
enum estado { INICIO, JUGANDO, PAUSA, DERROTA, VICTORIA };

typedef struct {
    int cuerpo[CASILLAS];
    bool ocupada[CASILLAS];
    int cabeza, longitud, comida, puntos, record;
    enum direccion direccion, giro;
    enum estado estado;
    uint32_t azar;
} partida_t;

static const int dx[] = { 0, 0, -1, 1 };
static const int dy[] = { -1, 1, 0, 0 };

static uint32_t siguiente_azar(partida_t *p)
{
    uint32_t x = p->azar;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    p->azar = x;
    return x;
}

static int elegir_comida(partida_t *p)
{
    int libres = CASILLAS - p->longitud;
    if (!libres)
        return -1;
    int seleccion = siguiente_azar(p) % libres;
    for (int i = 0; i < CASILLAS; i++)
        if (!p->ocupada[i] && seleccion-- == 0)
            return i;
    return -1;
}

static void preparar(partida_t *p, int record, uint32_t semilla)
{
    memset(p, 0, sizeof *p);
    p->record = record;
    p->longitud = 4;
    p->cabeza = 3;
    p->direccion = DERECHA;
    p->giro = SIN_GIRO;
    p->azar = semilla ? semilla : 1;
    for (int i = 0; i < p->longitud; i++) {
        int casilla = (FILAS / 2) * COLUMNAS + COLUMNAS / 2 - 5 + i;
        p->cuerpo[i] = casilla;
        p->ocupada[casilla] = true;
    }
    p->comida = elegir_comida(p);
    p->estado = INICIO;
}

/* Un giro por paso. Comparar con la dirección confirmada evita que dos
 * teclas rápidas permitan girar hacia el cuello de la serpiente. */
static void pedir_giro(partida_t *p, enum direccion giro)
{
    if (p->giro == SIN_GIRO && giro != p->direccion &&
        giro != (p->direccion ^ 1))
        p->giro = giro;
}

static void avanzar(partida_t *p)
{
    if (p->giro != SIN_GIRO) {
        p->direccion = p->giro;
        p->giro = SIN_GIRO;
    }

    int actual = p->cuerpo[p->cabeza];
    int x = actual % COLUMNAS + dx[p->direccion];
    int y = actual / COLUMNAS + dy[p->direccion];
    if (x < 0 || x >= COLUMNAS || y < 0 || y >= FILAS) {
        p->estado = DERROTA;
        return;
    }

    int destino = y * COLUMNAS + x;
    int cola = p->cuerpo[(p->cabeza + CASILLAS + 1 - p->longitud) % CASILLAS];
    bool crece = destino == p->comida;
    /* La cola se retira en el mismo paso, salvo al comer. */
    if (p->ocupada[destino] && (destino != cola || crece)) {
        p->estado = DERROTA;
        return;
    }
    if (!crece)
        p->ocupada[cola] = false;
    p->cabeza = (p->cabeza + 1) % CASILLAS;
    p->cuerpo[p->cabeza] = destino;
    p->ocupada[destino] = true;

    if (crece) {
        p->longitud++;
        p->puntos++;
        if (p->puntos > p->record)
            p->record = p->puntos;
        p->comida = elegir_comida(p);
        if (p->comida < 0)
            p->estado = VICTORIA;
    }
}

static int intervalo(const partida_t *p)
{
    int ms = 185 - (p->puntos / 4) * 13;
    return ms >= 68 ? ms : 68;
}

static void titulo(jan_t *j)
{
    static const uint8_t letras[][7] = {
        { 15, 16, 16, 14, 1, 1, 30 },
        { 17, 25, 25, 21, 19, 19, 17 },
        { 14, 17, 17, 31, 17, 17, 17 },
        { 17, 18, 20, 24, 20, 18, 17 },
        { 31, 16, 16, 30, 16, 16, 31 },
    };
    for (int letra = 0; letra < 5; letra++)
        for (int y = 0; y < 7; y++)
            for (int x = 0; x < 5; x++)
                if (letras[letra][y] & (16u >> x))
                    g_retangulo(j, 24 + letra * 25 + x * 4, 30 + y * 4, 4, 4,
                                0xbff66f);
}

static void marcador(jan_t *j, int x, const char *nombre, int valor)
{
    char texto[12];
    formatar_texto(texto, sizeof texto, "%02d", valor);
    g_retangulo(j, x, 20, 88, 49, 0x202e36);
    g_texto(j, x + 9, 24, nombre, 0x9daeae);
    g_texto(j, x + 9, 44, texto, 0xfff0be);
}

static void fruta(jan_t *j, int casilla)
{
    int x = X_TABLERO + (casilla % COLUMNAS) * CELDA;
    int y = Y_TABLERO + (casilla / COLUMNAS) * CELDA;
    g_retangulo(j, x + 4, y + 6, 10, 9, 0xf4774e);
    g_retangulo(j, x + 3, y + 8, 12, 5, 0xf4774e);
    g_retangulo(j, x + 6, y + 7, 3, 2, 0xffba73);
    g_retangulo(j, x + 9, y + 3, 4, 2, 0xcde786);
}

static void serpiente(jan_t *j, const partida_t *p)
{
    int cola = (p->cabeza + CASILLAS + 1 - p->longitud) % CASILLAS;
    for (int i = 0; i < p->longitud; i++) {
        int casilla = p->cuerpo[(cola + i) % CASILLAS];
        int x = X_TABLERO + (casilla % COLUMNAS) * CELDA;
        int y = Y_TABLERO + (casilla / COLUMNAS) * CELDA;
        bool cabeza = i == p->longitud - 1;
        uint32_t color = cabeza ? 0xcaf577 : (i % 2 ? 0x80bc65 : 0x72ad5d);
        g_retangulo(j, x + 2, y + 2, CELDA - 4, CELDA - 4, color);

        if (!cabeza) {
            int siguiente = p->cuerpo[(cola + i + 1) % CASILLAS];
            int lado_x = siguiente % COLUMNAS - casilla % COLUMNAS;
            int lado_y = siguiente / COLUMNAS - casilla / COLUMNAS;
            g_retangulo(j, x + 5 + lado_x * 6, y + 5 + lado_y * 6,
                        8, 8, color);
        } else {
            int frente_x = dx[p->direccion], frente_y = dy[p->direccion];
            int ojo_x = x + 8 + 3 * frente_x;
            int ojo_y = y + 8 + 3 * frente_y;
            g_retangulo(j, ojo_x - 3 * frente_y, ojo_y - 3 * frente_x, 2, 2, 0x203026);
            g_retangulo(j, ojo_x + 3 * frente_y, ojo_y + 3 * frente_x, 2, 2, 0x203026);
        }
    }
}

static void dibujar(jan_t *j, const partida_t *p)
{
    const int largo = COLUMNAS * CELDA, alto = FILAS * CELDA;
    g_retangulo(j, 0, 0, ANCHO, ALTO, 0x142127);
    titulo(j);
    marcador(j, 282, _("PUNTOS"), p->puntos);
    marcador(j, 379, _("RECORD"), p->record);
    marcador(j, 476, _("NIVEL"), 1 + p->puntos / 4);
    g_retangulo(j, X_TABLERO - 2, Y_TABLERO - 2, largo + 4, alto + 4, 0x64806e);
    for (int y = 0; y < FILAS; y++)
        for (int x = 0; x < COLUMNAS; x++)
            g_retangulo(j, X_TABLERO + x * CELDA, Y_TABLERO + y * CELDA,
                        CELDA, CELDA, (x + y) % 2 ? 0x273e38 : 0x2b433b);
    if (p->comida >= 0)
        fruta(j, p->comida);
    serpiente(j, p);
    g_texto(j, 24, 449, _("Flechas / WASD  Mover     Espacio / P  Pausa     Esc  Salir"), 0xb7c6be);

    if (p->estado != JUGANDO) {
        const char *texto = _("Pulsa Espacio para empezar");
        if (p->estado == PAUSA)
            texto = _("Pausa. Espacio para seguir");
        else if (p->estado == DERROTA)
            texto = _("Fin de partida. Espacio para repetir");
        else if (p->estado == VICTORIA)
            texto = _("Tablero completo. Espacio para repetir");
        g_retangulo(j, 127, 214, 334, 45, 0x142127);
        g_moldura(j, 127, 214, 334, 45, 0x83bf72);
        g_texto(j, (ANCHO - g_largura_texto(texto)) / 2, 229, texto, 0xfff0be);
    }
    jan_mostrar(j);
}

static enum direccion tecla_giro(int tecla)
{
    switch (tecla) {
    case TECLA_CIMA: case 'w': case 'W': return ARRIBA;
    case TECLA_BAIXO: case 's': case 'S': return ABAJO;
    case TECLA_ESQUERDA: case 'a': case 'A': return IZQUIERDA;
    case TECLA_DIREITA: case 'd': case 'D': return DERECHA;
    default: return SIN_GIRO;
    }
}

/* El récord va en el perfil del usuario: «record=N». Sin /home, en /tmp. */
static const char *ruta_prueba;   /* en --prueba, otro archivo: no toca el récord del usuario */

static const char *ruta_record(void)
{
    if (ruta_prueba)
        return ruta_prueba;
    info_arquivo_t inf;
    return info_arquivo("/home/.config", &inf) == 0 ? "/home/.config/snake.conf" : "/tmp/snake.conf";
}

static int leer_record(void)
{
    char buf[64];
    int fd = abrir(ruta_record(), ABRIR_LER);
    if (fd < 0)
        return 0;
    int n = ler(fd, buf, sizeof buf - 1);
    fechar(fd);
    if (n < 8 || compara_texto_n(buf, "record=", 7) != 0)
        return 0;
    buf[n] = 0;
    int v = texto_para_int(buf + 7);
    return v > 0 && v < CASILLAS ? v : 0;
}

static void guardar_record(int record)
{
    char buf[32];
    criar_pasta("/home/.config");
    int n = formatar_texto(buf, sizeof buf, "record=%d\n", record);
    int fd = abrir(ruta_record(), ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0)
        return;
    escrever(fd, buf, (unsigned)n);
    fechar(fd);
}

static int jugar(uint32_t semilla)
{
    partida_t p;
    int record_guardado = leer_record();
    preparar(&p, record_guardado, semilla ? semilla : tempo_ms());
    jan_t *j = jan_abrir("Snake", ANCHO, ALTO);
    if (!j) {
        imprimir_em(ERROS, "snake: no se pudo abrir la ventana\n");
        return 1;
    }
    dibujar(j, &p);
    registrar("snake: listo\n");

    uint32_t turno = 0;
    for (;;) {
        int espera = -1;
        if (p.estado == JUGANDO) {
            int32_t falta = (int32_t)(turno - tempo_ms());
            if (falta <= 0) {
                avanzar(&p);
                turno = tempo_ms() + intervalo(&p);
                dibujar(j, &p);
                if (p.estado == DERROTA || p.estado == VICTORIA) {
                    registrar("snake: fin, puntos %d\n", p.puntos);
                    if (p.record > record_guardado) {
                        record_guardado = p.record;
                        guardar_record(record_guardado);
                    }
                }
                continue;
            }
            espera = falta;
        }

        evento_t evento;
        if (esperar_evento(j->id, &evento, espera) != 1)
            continue;
        if (evento.tipo == EV_FECHAR ||
            (evento.tipo == EV_TECLA && evento.tecla == TECLA_ESC))
            break;
        if (evento.tipo != EV_TECLA)
            continue;

        int tecla = evento.tecla;
        if (tecla == ' ' || tecla == 'p' || tecla == 'P') {
            if ((p.estado == INICIO || p.estado == DERROTA || p.estado == VICTORIA) &&
                tecla != ' ')
                continue;
            if (p.estado == JUGANDO) {
                p.estado = PAUSA;
                registrar("snake: pausa\n");
            } else {
                if (p.estado == DERROTA || p.estado == VICTORIA)
                    preparar(&p, p.record, p.azar);
                p.estado = JUGANDO;
                turno = tempo_ms() + intervalo(&p);
                registrar("snake: jugando\n");
            }
            dibujar(j, &p);
        } else if (p.estado == JUGANDO) {
            enum direccion giro = tecla_giro(tecla);
            if (giro != SIN_GIRO)
                pedir_giro(&p, giro);
        }
    }

    jan_fechar(j);
    return 0;
}

#define COMPROBAR(condicion) do { \
    if (!(condicion)) { \
        imprimir_em(ERROS, "snake: fallo en la prueba de la linea %d\n", __LINE__); \
        return 1; \
    } \
} while (0)

static int probar(void)
{
    partida_t p;
    preparar(&p, 0, 19);
    p.estado = JUGANDO;
    COMPROBAR(p.comida >= 0 && !p.ocupada[p.comida]);
    int anterior = p.cuerpo[p.cabeza];
    p.comida = anterior + 1;
    pedir_giro(&p, IZQUIERDA);
    COMPROBAR(p.giro == SIN_GIRO);
    avanzar(&p);
    COMPROBAR(p.longitud == 5 && p.puntos == 1 && p.record == 1);
    COMPROBAR(p.cuerpo[p.cabeza] == anterior + 1 && !p.ocupada[p.comida]);

    pedir_giro(&p, ARRIBA);
    pedir_giro(&p, IZQUIERDA);
    avanzar(&p);
    COMPROBAR(p.direccion == ARRIBA && p.cuerpo[p.cabeza] == anterior + 1 - COLUMNAS);

    int cuerpo[] = { 0, 1, 1 + COLUMNAS, COLUMNAS };
    memset(p.ocupada, 0, sizeof p.ocupada);
    for (int i = 0; i < 4; i++) {
        p.cuerpo[i] = cuerpo[i];
        p.ocupada[cuerpo[i]] = true;
    }
    p.longitud = 4;
    p.cabeza = 3;
    p.comida = CASILLAS - 1;
    p.direccion = ARRIBA;
    avanzar(&p);
    COMPROBAR(p.estado == JUGANDO && p.cuerpo[p.cabeza] == 0 && p.ocupada[0]);
    p.direccion = DERECHA;
    avanzar(&p);
    COMPROBAR(p.estado == JUGANDO);
    p.direccion = IZQUIERDA;
    avanzar(&p);
    COMPROBAR(p.estado == DERROTA);
    p.estado = JUGANDO;
    p.direccion = ARRIBA;
    avanzar(&p);
    COMPROBAR(p.estado == DERROTA);

    preparar(&p, 5, 23);
    p.estado = JUGANDO;
    p.longitud = CASILLAS - 1;
    p.cabeza = CASILLAS - 2;
    for (int i = 0; i < p.longitud; i++) {
        p.cuerpo[i] = i;
        p.ocupada[i] = true;
    }
    p.comida = CASILLAS - 1;
    avanzar(&p);
    COMPROBAR(p.estado == VICTORIA && p.comida == -1);
    COMPROBAR(intervalo(&p) == 185);
    p.puntos = 90;
    COMPROBAR(intervalo(&p) == 68);
    ruta_prueba = "/tmp/snake-prueba.conf";   /* el récord se guarda y se lee */
    guardar_record(7);
    COMPROBAR(leer_record() == 7);
    apagar(ruta_prueba);
    COMPROBAR(leer_record() == 0);
    escrever_texto(SAIDA, "snake: pruebas de juego correctas\n");
    registrar("snake: pruebas de juego correctas\n");
    return 0;
}

int principal(int argc, char **argv)
{
    uint32_t semilla = 0;
    if (argc == 2 && compara_texto(argv[1], "--prueba") == 0)
        return probar();
    if (argc == 3 && compara_texto(argv[1], "--semilla") == 0) {
        semilla = (uint32_t)texto_para_int(argv[2]);
    } else if (argc != 1) {
        imprimir_em(ERROS, "Uso: snake [--semilla N | --prueba]\n");
        return 2;
    }
    return jugar(semilla);
}
