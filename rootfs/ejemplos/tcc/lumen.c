// SPDX-License-Identifier: GPL-3.0-only
/* lumen: reproductor de video y audio con cola, pantalla completa y visualizador (ejemplo de multimedia con la libsagui).
 * Origen: proyecto SaguiOS (ejemplos para aprender C compilando con tcc dentro del sistema).
 * SaguiOS ya trae el Reproductor de video y Sintonia; este es la version sencilla para estudiar. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui.h"
#include "video.h"
#include "audio_formato.h"

enum { MAX_PISTAS = 16, ANCHO_RUTA = 384, PCM = 2048, BARRAS = 28 };
enum { SIN_MEDIO, SONIDO, PELICULA };

#define FONDO   0x111520
#define PANEL   0x1c2230
#define TARJETA 0x292e40
#define BORDE   0x393f53
#define TEXTO   0xf0edf8
#define SUAVE   0xabb0c6
#define ACENTO  0xf3ba80
#define MENTA   0x8edccb

typedef struct {
    jan_t *j;
    char rutas[MAX_PISTAS][ANCHO_RUTA];
    char aviso[112];
    int numero, actual, tope;
    int tipo, flujo, volumen;
    int tasa, canales, fps, cuadro, cuadros;
    bool tocando, fin_decodificador, terminado, mudo;
    bool pantalla_completa, controles_visibles;
    uint32_t actividad_ms;
    uint32_t duracion, posicion, inicio_ms, inicio_pos, entregados;
    video_t *video;
    audio_arquivo_t *audio;
    imagem_t imagen;
    int16_t pcm[PCM * 2];
    unsigned pcm_total, pcm_enviado;
    int amplitud[BARRAS];
    int cuadros_decodificados;
} lumen_t;

static int menor(int a, int b) { return a < b ? a : b; }
static int limitar(int x, int max) { return x < 0 ? 0 : x > max ? max : x; }

static const char *nombre_archivo(const char *ruta)
{
    const char *barra = strrchr(ruta, '/');
    return barra ? barra + 1 : ruta;
}

static int anadir(lumen_t *p, const char *ruta)
{
    if (!ruta[0] || strlen(ruta) >= ANCHO_RUTA)
        return -1;
    for (int i = 0; i < p->numero; i++)
        if (strcmp(p->rutas[i], ruta) == 0)
            return i;
    if (p->numero == MAX_PISTAS)
        return -1;
    strcpy(p->rutas[p->numero], ruta);
    return p->numero++;
}

static void cerrar_flujo(lumen_t *p)
{
    if (p->flujo >= 0)
        audio_fechar(p->flujo, 0);
    p->flujo = -1;
    p->pcm_total = p->pcm_enviado = p->entregados = 0;
}

static void abrir_flujo(lumen_t *p)
{
    if (!p->tasa || !p->canales)
        return;
    p->flujo = audio_abrir(p->tasa, p->canales, 16);
    if (p->flujo < 0) {
        p->flujo = -1;
        snprintf(p->aviso, sizeof p->aviso, "No hay salida de audio.");
    } else {
        audio_volume(p->flujo, p->mudo ? 0 : p->volumen);
    }
}

static void cerrar_medio(lumen_t *p)
{
    cerrar_flujo(p);
    if (p->audio)
        audio_arquivo_fechar(p->audio);
    if (p->video)
        video_fechar(p->video);
    p->audio = NULL;
    p->video = NULL;
    imagem_liberar(&p->imagen);
    p->tipo = SIN_MEDIO;
    p->tocando = p->fin_decodificador = p->terminado = false;
    p->posicion = p->duracion = 0;
    p->tasa = p->canales = p->fps = p->cuadros = p->cuadros_decodificados = 0;
    memset(p->amplitud, 0, sizeof p->amplitud);
}

static void problema(lumen_t *p, const char *texto)
{
    snprintf(p->aviso, sizeof p->aviso, "%s", texto);
    p->tocando = false;
    cerrar_flujo(p);
}

static bool leer_cuadro(lumen_t *p)
{
    int r = video_quadro(p->video, &p->imagen);
    if (r == 0) {
        p->cuadro++;
        p->cuadros_decodificados++;
        return true;
    }
    if (r == VIDEO_ERRO_FIM)
        p->fin_decodificador = true;
    else
        problema(p, video_erro_texto(r));
    return false;
}

static bool buscar_instante(lumen_t *p, uint32_t ms, bool tocar)
{
    if (p->tipo == SIN_MEDIO)
        return false;
    cerrar_flujo(p);
    p->aviso[0] = 0;
    p->fin_decodificador = p->terminado = false;
    p->tocando = tocar;
    if (p->duracion && ms >= p->duracion)
        ms = p->duracion - 1;

    if (p->audio) {
        int r = audio_arquivo_buscar(p->audio, ms);
        if (r < 0) {
            problema(p, audio_erro_texto(r));
            return false;
        }
    } else {
        unsigned n = (unsigned)((uint64_t)ms * p->fps / 1000u);
        if (p->cuadros > 0 && n >= (unsigned)p->cuadros)
            n = p->cuadros - 1;
        if (video_buscar(p->video, n) != 0) {
            problema(p, "No se pudo buscar esa posicion.");
            return false;
        }
        p->cuadro = (int)n - 1;
        if (!leer_cuadro(p)) {
            if (!p->aviso[0])
                problema(p, "El video no contiene un cuadro en esa posicion.");
            return false;
        }
        ms = (uint32_t)((uint64_t)n * 1000u / p->fps);
    }

    p->posicion = p->inicio_pos = ms;
    p->inicio_ms = tempo_ms();
    if (tocar) {
        abrir_flujo(p);
        if (p->audio && p->flujo < 0)
            p->tocando = false;
    }
    return true;
}

static bool seleccionar(lumen_t *p, int indice, bool tocar)
{
    if (indice < 0 || indice >= p->numero)
        return false;
    cerrar_medio(p);
    p->actual = indice;
    p->aviso[0] = 0;
    const char *ruta = p->rutas[indice];
    int error_audio, error_video;
    p->audio = audio_arquivo_abrir(ruta, &error_audio);
    if (p->audio) {
        const audio_info_t *i = audio_arquivo_info(p->audio);
        p->tipo = SONIDO;
        p->tasa = i->taxa;
        p->canales = i->canais;
        p->duracion = i->duracao_ms;
    } else {
        p->video = video_abrir(ruta, &error_video);
        if (!p->video) {
            problema(p, "No se pudo abrir el archivo o su formato no es compatible.");
            return false;
        }
        const video_info_t *i = video_info(p->video);
        p->tipo = PELICULA;
        p->fps = i->fps > 0 ? i->fps : 25;
        p->cuadros = video_total_quadros(p->video);
        p->duracion = i->duracao_ms;
        if (!p->duracion && p->cuadros > 0)
            p->duracion = (uint32_t)((uint64_t)p->cuadros * 1000u / p->fps);
        if (i->tem_audio) {
            p->canales = i->canais_audio;
            p->tasa = i->taxa_audio;
        }
    }
    if (!buscar_instante(p, 0, tocar))
        return false;
    registrar("lumen: %s, %u ms, %s\n", p->tipo == SONIDO ? "audio" : "video",
              p->duracion, nombre_archivo(ruta));
    return true;
}

static void medir_pcm(lumen_t *p, int cuadros)
{
    for (int i = 0; i < BARRAS; i++) {
        int desde = cuadros * i / BARRAS, hasta = cuadros * (i + 1) / BARRAS;
        int pico = 0;
        for (int q = desde; q < hasta; q++) {
            int valor = p->pcm[q * p->canales];
            if (valor < 0)
                valor = -valor;
            if (valor > pico)
                pico = valor;
        }
        p->amplitud[i] = (3 * p->amplitud[i] + pico) / 4;
    }
}

/* Conserva las muestras que el flujo no aceptó. Así el bucle gráfico no
 * duerme por un búfer lleno ni pierde el final de un bloque. */
static void avanzar_audio(lumen_t *p)
{
    if (p->flujo < 0)
        return;

    for (int intento = 0; intento < 5; intento++) {
        int pendiente = audio_pendente(p->flujo);
        if (pendiente < 0) {
            problema(p, "Se perdio la salida de audio.");
            return;
        }
        if (pendiente >= 160)
            break;

        if (p->pcm_enviado == p->pcm_total) {
            if (p->audio && p->fin_decodificador)
                break;
            int n = p->audio ? audio_arquivo_ler(p->audio, p->pcm, PCM)
                             : video_audio(p->video, p->pcm, PCM);
            if (n < 0) {
                problema(p, p->audio ? audio_erro_texto(n) : video_erro_texto(n));
                return;
            }
            if (n == 0) {
                if (p->audio)
                    p->fin_decodificador = true;
                break;
            }
            medir_pcm(p, n);
            p->pcm_enviado = 0;
            p->pcm_total = (unsigned)n * p->canales * 2;
        }
        unsigned restantes = p->pcm_total - p->pcm_enviado;
        int n = audio_escrever_sem_bloquear(p->flujo,
                    (const char *)p->pcm + p->pcm_enviado, restantes);
        if (n < 0) {
            problema(p, "No se pudo enviar el sonido.");
            return;
        }
        if (n == 0)
            break;
        p->pcm_enviado += n;
        p->entregados += n / (p->canales * 2);
    }
}

static void terminar(lumen_t *p)
{
    p->tocando = false;
    p->terminado = true;
    if (p->duracion)
        p->posicion = p->duracion;
    cerrar_flujo(p);
    registrar("lumen: fin, %d cuadros decodificados\n", p->cuadros_decodificados);
}

static void paso(lumen_t *p)
{
    if (!p->tocando)
        return;
    if (p->video) {
        p->posicion = p->inicio_pos + (tempo_ms() - p->inicio_ms);
        uint32_t cuadro = (uint32_t)((uint64_t)p->posicion * p->fps / 1000u);
        for (int i = 0; i < 3 && !p->fin_decodificador && (uint32_t)p->cuadro < cuadro; i++)
            if (!leer_cuadro(p))
                break;
    }
    if (!p->tocando)
        return;
    avanzar_audio(p);
    if (!p->tocando)
        return;
    int pendiente = p->flujo >= 0 ? audio_pendente(p->flujo) : 0;
    if (pendiente < 0)
        pendiente = 0;
    if (p->audio) {
        uint32_t reproducido = (uint32_t)((uint64_t)p->entregados * 1000u / p->tasa);
        p->posicion = p->inicio_pos + (reproducido > (unsigned)pendiente
                      ? reproducido - (unsigned)pendiente : 0);
    }
    if (p->duracion && p->posicion > p->duracion)
        p->posicion = p->duracion;
    if (p->fin_decodificador && p->pcm_enviado == p->pcm_total && pendiente <= 0)
        terminar(p);
}

static void alternar(lumen_t *p)
{
    if (p->tipo == SIN_MEDIO)
        return;
    if (p->tocando) {
        p->tocando = false;
        cerrar_flujo(p);
        registrar("lumen: pausa\n");
    } else if (buscar_instante(p, p->terminado ? 0 : p->posicion, true) && p->tocando) {
        registrar("lumen: reproduciendo\n");
    }
}

static void salto(lumen_t *p, int delta)
{
    int64_t ms = (int64_t)p->posicion + delta;
    if (ms < 0)
        ms = 0;
    if (p->duracion && ms > p->duracion)
        ms = p->duracion;
    if (buscar_instante(p, (uint32_t)ms, p->tocando))
        registrar("lumen: posicion %u ms\n", p->posicion);
}

static void volumen(lumen_t *p, int valor)
{
    p->volumen = limitar(valor, 100);
    p->mudo = false;
    if (p->flujo >= 0)
        audio_volume(p->flujo, p->volumen);
}

static void circulo(jan_t *j, int x, int y, int radio, uint32_t color)
{
    for (int dy = -radio; dy <= radio; dy++) {
        int dx = radio;
        while (dx * dx + dy * dy > radio * radio)
            dx--;
        g_retangulo(j, x - dx, y + dy, 2 * dx + 1, 1, color);
    }
}

static void triangulo(jan_t *j, int x, int y, int radio, int lado, uint32_t color)
{
    for (int k = 0; k < radio * 2; k++) {
        int alto = radio - k / 2;
        g_retangulo(j, x + (k - radio) * lado, y - alto, 1, 2 * alto + 1, color);
    }
}

static void texto(jan_t *j, int x, int y, const char *s, int ancho, uint32_t color)
{
    int n = menor((int)strlen(s), ancho / LETRA_L);
    if (n > 0)
        g_texto_n(j, x, y, s, n, color);
}

static void centrado(jan_t *j, int x, int y, int w, const char *s, uint32_t color)
{
    int l = menor((int)strlen(s) * LETRA_L, w);
    texto(j, x + (w - l) / 2, y, s, l, color);
}

static void tiempo(char *s, size_t n, uint32_t ms)
{
    uint32_t segundos = ms / 1000;
    snprintf(s, n, "%02u:%02u", segundos / 60, segundos % 60);
}

static const char *titulo_pista(const lumen_t *p)
{
    if (p->audio) {
        const audio_info_t *i = audio_arquivo_info(p->audio);
        if (i->titulo[0])
            return i->titulo;
    }
    return p->actual < 0 ? "Ningun medio abierto" : nombre_archivo(p->rutas[p->actual]);
}

static int x_lista(const jan_t *j) { return j->larg - 237; }
static int y_controles(const jan_t *j) { return j->alt - 111; }
static int filas_lista(const jan_t *j) { return (y_controles(j) - 130) / 44; }

static void escenario(lumen_t *p, int x, int y, int w, int h)
{
    jan_t *j = p->j;
    g_retangulo(j, x, y, w, h, p->pantalla_completa ? 0 : 0x090d16);
    if (p->tipo == PELICULA && p->imagen.px) {
        int lw = w - 2, lh = lw * p->imagen.alt / p->imagen.larg;
        if (lh > h - 2) {
            lh = h - 2;
            lw = lh * p->imagen.larg / p->imagen.alt;
        }
        imagem_desenhar(j->px, j->larg, x + 1, y + 1, w - 2, h - 2, &p->imagen,
                        x + (w - lw) / 2, y + (h - lh) / 2, lw, lh, false);
        return;
    }

    for (int fila = 1; fila < h - 1; fila++) {
        unsigned luz = (unsigned)(h - fila) * 18 / h;
        g_retangulo(j, x + 1, y + fila, w - 2, 1,
                    ((21 + luz) << 16) | ((24 + luz) << 8) | (43 + luz * 2));
    }
    int cx = x + w / 2, cy = y + h / 2 - 12;
    int radio = menor(88, h / 3);
    circulo(j, cx, cy, radio, 0x453b66);
    circulo(j, cx, cy, radio - 2, 0x302843);
    circulo(j, cx, cy, radio * 2 / 3, 0x483d61);
    circulo(j, cx, cy, radio * 2 / 3 - 2, 0x312942);
    int ancho = menor(w - 30, 310);
    for (int i = 0; i < BARRAS; i++) {
        int a = 4 + p->amplitud[i] * (radio - 12) / 32768;
        int col = cx - ancho / 2 + ancho * i / BARRAS;
        g_retangulo(j, col, cy - a, 5, 2 * a, i % 3 ? ACENTO : MENTA);
    }
    centrado(j, x + 15, y + h - 41, w - 30, titulo_pista(p), TEXTO);
}

static void lista(lumen_t *p)
{
    jan_t *j = p->j;
    int x = x_lista(j), w = j->larg - x - 22, h = y_controles(j) - 90;
    g_retangulo(j, x, 72, w, h, PANEL);
    g_moldura(j, x, 72, w, h, BORDE);
    g_texto(j, x + 13, 86, "Cola", TEXTO);
    char cuantos[14];
    snprintf(cuantos, sizeof cuantos, "%d", p->numero);
    g_texto(j, x + w - 16 - g_largura_texto(cuantos), 86, cuantos, SUAVE);
    g_linha(j, x + 13, 111, x + w - 13, 111, BORDE);
    if (!p->numero)
        g_texto(j, x + 13, 126, "Abre un archivo", SUAVE);
    for (int fila = 0; fila < filas_lista(j) && p->tope + fila < p->numero; fila++) {
        int i = p->tope + fila, y = 119 + fila * 44;
        bool activo = i == p->actual;
        if (activo) {
            g_retangulo(j, x + 6, y, w - 12, 38, TARJETA);
            g_retangulo(j, x + 6, y, 3, 38, ACENTO);
        }
        char n[8];
        snprintf(n, sizeof n, "%02d", i + 1);
        g_texto(j, x + 14, y + 11, n, activo ? ACENTO : SUAVE);
        texto(j, x + 42, y + 11, nombre_archivo(p->rutas[i]), w - 51, TEXTO);
    }
}

static void controles(lumen_t *p)
{
    jan_t *j = p->j;
    int w = j->larg, y = y_controles(j), cx = w / 2;
    g_retangulo(j, 0, y, w, j->alt - y, PANEL);
    g_linha(j, 0, y, w - 1, y, BORDE);
    char actual[20], total[20], detalles[80];
    tiempo(actual, sizeof actual, p->posicion);
    tiempo(total, sizeof total, p->duracion);
    int inicio = 86, largo = w - 172;
    g_texto(j, 22, y + 12, actual, TEXTO);
    g_texto(j, w - 22 - g_largura_texto(total), y + 12, p->duracion ? total : "--:--", SUAVE);
    g_retangulo(j, inicio, y + 20, largo, 4, BORDE);
    int pasado = p->duracion ? (int)((uint64_t)largo * p->posicion / p->duracion) : 0;
    g_retangulo(j, inicio, y + 20, pasado, 4, ACENTO);
    if (p->duracion)
        circulo(j, inicio + pasado, y + 22, 5, ACENTO);

    texto(j, 23, y + 47, titulo_pista(p), cx - 147, TEXTO);
    const char *formato = p->video ? video_info(p->video)->nome_formato :
                          p->audio ? audio_arquivo_info(p->audio)->nome_formato : "Audio y video";
    if (p->video && p->imagen.px)
        snprintf(detalles, sizeof detalles, "%s  %dx%d", formato, p->imagen.larg, p->imagen.alt);
    else
        snprintf(detalles, sizeof detalles, "%s", formato);
    texto(j, 23, y + 71, p->aviso[0] ? p->aviso : detalles, cx - 147,
          p->aviso[0] ? ACENTO : SUAVE);

    int cy = y + 67;
    triangulo(j, cx - 69, cy, 7, -1, TEXTO);
    g_retangulo(j, cx - 79, cy - 7, 2, 15, TEXTO);
    g_retangulo(j, cx - 41, cy - 5, 11, 11, TEXTO);
    circulo(j, cx, cy, 24, ACENTO);
    if (p->tocando) {
        g_retangulo(j, cx - 6, cy - 8, 4, 17, FONDO);
        g_retangulo(j, cx + 3, cy - 8, 4, 17, FONDO);
    } else {
        triangulo(j, cx + 2, cy, 9, 1, FONDO);
    }
    triangulo(j, cx + 66, cy, 7, 1, TEXTO);
    g_retangulo(j, cx + 76, cy - 7, 2, 15, TEXTO);

    int vx = w - 128, vy = y + 67;
    g_texto(j, w - 204, vy - 8, p->mudo ? "Silencio" : "Volumen", SUAVE);
    g_retangulo(j, vx, vy, 101, 4, BORDE);
    if (!p->mudo)
        g_retangulo(j, vx, vy, p->volumen, 4, MENTA);
    circulo(j, vx + (p->mudo ? 0 : p->volumen), vy + 2, 4, MENTA);
}

static void boton_cabecera(jan_t *j, int x, int ancho, const char *nombre)
{
    g_retangulo(j, x, 21, ancho, 29, TARJETA);
    g_moldura(j, x, 21, ancho, 29, BORDE);
    centrado(j, x, 27, ancho, nombre, TEXTO);
}

static void dibujar(lumen_t *p)
{
    jan_t *j = p->j;
    int w = j->larg;
    if (p->pantalla_completa) {
        escenario(p, 0, 0, w, j->alt);
        if (p->controles_visibles) {
            boton_cabecera(j, w - 195, 173, "Salir de pantalla");
            controles(p);
        }
    } else {
        g_retangulo(j, 0, 0, w, j->alt, FONDO);
        circulo(j, 36, 35, 14, ACENTO);
        triangulo(j, 37, 35, 6, 1, FONDO);
        g_texto(j, 60, 27, "LUMEN", TEXTO);
        boton_cabecera(j, w - 354, 181, "Pantalla completa");
        boton_cabecera(j, w - 163, 141, "Abrir archivo");
        int ancho = x_lista(j) - 38, alto = y_controles(j) - 90;
        escenario(p, 22, 72, ancho, alto);
        g_moldura(j, 22, 72, ancho, alto, BORDE);
        lista(p);
        controles(p);
    }
    jan_mostrar(j);
}

static void abrir_archivo(lumen_t *p)
{
    if (p->tocando)
        alternar(p);
    char ruta[ANCHO_RUTA];
    if (!dialogo_abrir("Abrir un archivo multimedia", "Todos los archivos|*", "",
                      ruta, sizeof ruta))
        return;
    int indice = anadir(p, ruta);
    if (indice < 0) {
        problema(p, "La cola esta llena o la ruta es demasiado larga.");
        return;
    }
    seleccionar(p, indice, true);
    if (indice >= p->tope + filas_lista(p->j))
        p->tope = indice + 1 - filas_lista(p->j);
}

static void desplazar(lumen_t *p, int delta)
{
    int i = p->actual + delta;
    if (i < 0 || i >= p->numero)
        return;
    seleccionar(p, i, p->tocando);
    if (i < p->tope)
        p->tope = i;
    if (i >= p->tope + filas_lista(p->j))
        p->tope = i + 1 - filas_lista(p->j);
}

static void silencio(lumen_t *p)
{
    p->mudo = !p->mudo;
    if (p->flujo >= 0)
        audio_volume(p->flujo, p->mudo ? 0 : p->volumen);
}

static void detener(lumen_t *p)
{
    buscar_instante(p, 0, false);
    registrar("lumen: detenido\n");
}

static void clic(lumen_t *p, int x, int y)
{
    jan_t *j = p->j;
    int w = j->larg, cx = w / 2, base = y_controles(j);
    if (p->pantalla_completa && y >= 21 && y < 50 &&
        x >= w - 195 && x < w - 22) {
        jan_tela_cheia(j, false);
    } else if (!p->pantalla_completa && y >= 21 && y < 50 &&
               x >= w - 354 && x < w - 173) {
        jan_tela_cheia(j, true);
    } else if (!p->pantalla_completa && x >= w - 163 && x < w - 22 &&
               y >= 21 && y < 50) {
        abrir_archivo(p);
    } else if (!p->pantalla_completa && x >= x_lista(j) + 6 && x < w - 28 &&
               y >= 119 && y < base - 18) {
        int i = p->tope + (y - 119) / 44;
        if (i < p->numero)
            seleccionar(p, i, true);
    } else if (y >= base + 8 && y < base + 36 && x >= 83 && x <= w - 83 && p->duracion) {
        unsigned f = limitar(x - 86, w - 172);
        uint32_t ms = (uint32_t)((uint64_t)p->duracion * f / (w - 172));
        if (buscar_instante(p, ms, p->tocando))
            registrar("lumen: posicion %u ms\n", p->posicion);
    } else if (y >= base + 42 && y < base + 94) {
        if (x >= cx - 24 && x <= cx + 24) alternar(p);
        else if (x >= cx - 52 && x < cx - 24) detener(p);
        else if (x >= cx - 85 && x < cx - 52) desplazar(p, -1);
        else if (x >= cx + 49 && x < cx + 88) desplazar(p, 1);
        else if (x >= w - 136 && x <= w - 22) volumen(p, x - (w - 128));
        else if (x >= w - 210 && x < w - 136) silencio(p);
    }
}

static bool evento(lumen_t *p, const evento_t *e)
{
    if (e->tipo == EV_FECHAR)
        return false;
    if (e->tipo == EV_CLIQUE && (e->botoes & 1)) {
        bool sobre_medio = p->pantalla_completa ? e->y >= 55 && e->y < y_controles(p->j) :
                          e->x >= 22 && e->x < x_lista(p->j) - 16 &&
                          e->y >= 72 && e->y < y_controles(p->j) - 18;
        if (e->cliques == 2 && sobre_medio)
            jan_tela_cheia(p->j, !p->pantalla_completa);
        else
            clic(p, e->x, e->y);
    } else if (e->tipo == EV_RODA && !p->pantalla_completa && e->x >= x_lista(p->j)) {
        p->tope = limitar(p->tope + (int)e->cliques, p->numero > filas_lista(p->j)
                         ? p->numero - filas_lista(p->j) : 0);
    } else if (e->tipo == EV_REDIMENSIONAR) {
        jan_t *j = p->j;
        if (jan_redimensionar(j, e) || (e->x == j->larg && e->y == j->alt)) {
            bool pantalla = e->tecla == JAN_ESTADO_TELA_CHEIA;
            if (pantalla != p->pantalla_completa) {
                p->pantalla_completa = pantalla;
                p->controles_visibles = true;
                p->actividad_ms = tempo_ms();
                registrar("lumen: modo %s %dx%d\n", pantalla ? "pantalla completa" : "ventana",
                          j->larg, j->alt);
            }
            p->tope = 0;
        }
    } else if (e->tipo == EV_TECLA) {
        switch (e->tecla) {
        case TECLA_ESC: return false;
        case ' ': alternar(p); break;
        case 's': case 'S': detener(p); break;
        case 'm': case 'M': silencio(p); break;
        case '+': case '=': volumen(p, p->volumen + 5); break;
        case '-': volumen(p, p->volumen - 5); break;
        case TECLA_ESQUERDA: salto(p, -5000); break;
        case TECLA_DIREITA: salto(p, 5000); break;
        case TECLA_HOME: buscar_instante(p, 0, p->tocando); break;
        case TECLA_F11: jan_tela_cheia(p->j, !p->pantalla_completa); break;
        case 'o': case 'O': case 15:
            if (e->modificadores & 2)
                abrir_archivo(p);
            break;
        default: break;
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--ayuda") == 0) {
        puts("Uso: lumen [archivo ...]");
        puts("Espacio reproduce o pausa. Flechas saltan 5 s. Ctrl+O abre.");
        puts("S detiene. M silencia. +/- cambian el volumen. F11 cambia la pantalla.");
        return 0;
    }

    lumen_t *p = calloc(1, sizeof *p);
    if (!p)
        return 1;
    p->flujo = p->actual = -1;
    p->volumen = 70;
    for (int i = 1; i < argc; i++)
        if (anadir(p, argv[i]) < 0)
            fprintf(stderr, "lumen: no se pudo agregar %s\n", argv[i]);

    p->j = jan_abrir("Lumen - Reproductor", 884, 552);
    if (!p->j) {
        free(p);
        return 1;
    }
    jan_estilo(p->j, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, 672, 406);
    if (p->numero)
        seleccionar(p, 0, false);
    dibujar(p);
    registrar("lumen: listo\n");

    uint32_t ultimo = tempo_ms();
    for (;;) {
        evento_t e;
        int r = esperar_evento(p->j->id, &e, p->tocando ? 14 : -1);
        bool antes_controles = p->controles_visibles;
        if (r == 1 && p->pantalla_completa && (e.tipo == EV_MOVER || e.tipo == EV_CLIQUE ||
            e.tipo == EV_TECLA)) {
            p->controles_visibles = true;
            p->actividad_ms = tempo_ms();
        }
        if (r == 1 && !evento(p, &e))
            break;
        bool tocaba = p->tocando;
        int cuadro = p->cuadro;
        paso(p);
        uint32_t ahora = tempo_ms();
        if (p->pantalla_completa)
            p->controles_visibles = !p->tocando || ahora - p->actividad_ms < 2200;
        bool interactivo = r == 1 && (e.tipo == EV_TECLA || e.tipo == EV_CLIQUE ||
                            e.tipo == EV_RODA || e.tipo == EV_REDIMENSIONAR);
        if (interactivo || antes_controles != p->controles_visibles ||
            tocaba != p->tocando || cuadro != p->cuadro ||
            (p->tocando && ahora - ultimo >= 55)) {
            dibujar(p);
            ultimo = ahora;
        }
    }
    cerrar_medio(p);
    jan_fechar(p->j);
    free(p);
    return 0;
}
