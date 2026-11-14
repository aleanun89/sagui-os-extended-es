// SPDX-License-Identifier: GPL-2.0-or-later
/* sagui_gl.c - galería 3D con el motor del SaguiGL: un ejemplo completo que
 * se compila dentro del propio SaguiOS.
 *
 *     cc /ejemplos/sagui_gl/sagui_gl.c -o /tmp/sagui_gl
 *     /tmp/sagui_gl
 *
 * El motor (sgl.c) está en la misma carpeta: el cc compila un único archivo,
 * así que el motor entra con un #include. La ventana se abre con las llamadas
 * al sistema del kernel, las mismas que usa la libsagui por debajo (crear la
 * ventana y actualizarla).
 *
 * Qué tiene de más que la demo del cubo (que está en el /bin/sgldemo):
 *   - cuatro escenas: cubo, pirámide, cruz y ciudad;
 *   - la escena gira con las flechas y se acerca y se aleja con + y -;
 *   - modo malla (líneas) o relleno, con la tecla w;
 *   - fondo claro u oscuro, con la tecla f;
 *   - los cuadros por segundo van en el título de la ventana.
 *
 * Teclas:
 *     flechas       girar la escena
 *     + y -         acercar y alejar
 *     1 2 3 4       elegir la escena
 *     w             malla o relleno
 *     f             fondo claro u oscuro
 *     Esc           salir
 *
 * Para la prueba h99 (que compara el motor compilado por el cc con el
 * compilado por el GCC, píxel a píxel):
 *     sagui_gl --soma N    dibuja N cuadros en memoria y escribe la suma FNV
 *                          del último ("soma xxxxxxxx"), sin abrir ventana.
 */
#include <stdint.h>   /* el cc ya trae los tipos; el <...> lo ignora */

/* ---- las llamadas al sistema (ver kernel/inc/abi.h) ---- */
#define SIS_CRIAR_JANELA     50
#define SIS_FECHAR_JANELA    51
#define SIS_ATUALIZAR_JANELA 52
#define SIS_PROXIMO_EVENTO   53
#define SIS_TITULO_JANELA    54

#define EV_TECLA  4
#define EV_FECHAR 5
#define TECLA_CIMA     0x100
#define TECLA_BAIXO    0x101
#define TECLA_ESQUERDA 0x102
#define TECLA_DIREITA  0x103
#define TECLA_ESC      0x116

#ifdef SAGUI_ANFITRIAO
/* En el anfitrión (la prueba h99) sólo se compila la parte de la suma: no hay
 * ventanas ni llamadas al sistema, y el motor usa la libc. */
#define SGL_ANFITRIAO 1
#include <stdio.h>
#include <time.h>
#include <unistd.h>
/* Apaños del anfitrión: la ventana no se puede abrir fuera del SaguiOS. */
static int  __syscall(int n, int a, int b, int c) { (void)n; (void)a; (void)b; (void)c; return -1; }
static void dormir(int ms) { usleep((useconds_t)ms * 1000); }
static unsigned tempo_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}
#else
/* Compilado por el cc de dentro del SaguiOS. */
#define SGL_POR_CC 1
static void dormir(int ms) { sleep_ms(ms); }
static unsigned tempo_ms(void) { return (unsigned)time_ms(); }
#endif
#include "sgl.c"       /* el motor del SaguiGL (está en la misma carpeta) */

/* El evento, como lo entrega el kernel (abi.h). */
typedef struct {
    unsigned int tipo;
    int janela;
    int x, y;
    unsigned int botoes;
    unsigned int cliques;
    unsigned int tecla;
    unsigned int modificadores;
} evento_t;

#define LARG 480
#define ALT  360
#define NCENAS 4

static const char *const nome_cena[NCENAS] = { "cubo", "piramide", "cruz", "cidade" };

static uint32_t *pixels = 0;
static int jid = -1;
static int cena = 0;
static int malha = 0;
static int fundo_claro = 0;
static float giro = 30.0f;
static float tom = 20.0f;
static float distancia = 6.0f;

/* ---- la geometría ---- */

/* El cubo: 8 vértices y 12 triángulos (en sentido antihorario visto de fuera). */
static const float cubo_v[24] = {
    -1, -1, -1,   1, -1, -1,   1,  1, -1,  -1,  1, -1,
    -1, -1,  1,   1, -1,  1,   1,  1,  1,  -1,  1,  1 };
static const unsigned char cubo_f[36] = {
     0, 3, 2,   0, 2, 1,    4, 5, 6,   4, 6, 7,
     0, 4, 7,   0, 7, 3,    1, 2, 6,   1, 6, 5,
     3, 7, 6,   3, 6, 2,    0, 1, 5,   0, 5, 4 };
/* Las aristas, para la malla. */
static const unsigned char cubo_e[24] = {
     0, 1,   1, 2,   2, 3,   3, 0,   4, 5,   5, 6,
     6, 7,   7, 4,   0, 4,   1, 5,   2, 6,   3, 7 };
/* Un color por cara (seis caras, tres componentes cada una). El orden es el
 * de las caras del cubo: fondo, frente, derecha, izquierda, arriba y abajo. */
static const float cubo_c[18] = {
    0.85f, 0.25f, 0.25f,   /* fondo    */
    0.25f, 0.80f, 0.35f,   /* frente   */
    0.30f, 0.45f, 0.95f,   /* derecha  */
    0.95f, 0.80f, 0.25f,   /* izquierda*/
    0.85f, 0.35f, 0.85f,   /* arriba   */
    0.35f, 0.85f, 0.85f }; /* abajo   */

/* ---- las escenas ---- */

static void fundo(void)
{
    if (fundo_claro) glClearColor(0.55f, 0.60f, 0.68f, 1.0f);
    else glClearColor(0.07f, 0.09f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/* La rejilla del suelo, siempre en líneas. */
static void grade(int n)
{
    int i;
    glBegin(GL_LINES);
    glColor3f(0.30f, 0.36f, 0.42f);
    for (i = -n; i <= n; i++) {
        glVertex3f((float)i, 0.0f, -(float)n);
        glVertex3f((float)i, 0.0f, (float)n);
        glVertex3f(-(float)n, 0.0f, (float)i);
        glVertex3f((float)n, 0.0f, (float)i);
    }
    glEnd();
}

static void desenha_cubo(void)
{
    int i;
    if (malha) {
        glBegin(GL_LINES);
        glColor3f(0.95f, 0.85f, 0.35f);
        for (i = 0; i < 24; i += 2) {
            glVertex3f(cubo_v[cubo_e[i] * 3], cubo_v[cubo_e[i] * 3 + 1], cubo_v[cubo_e[i] * 3 + 2]);
            glVertex3f(cubo_v[cubo_e[i + 1] * 3], cubo_v[cubo_e[i + 1] * 3 + 1], cubo_v[cubo_e[i + 1] * 3 + 2]);
        }
        glEnd();
        return;
    }
    glBegin(GL_TRIANGLES);
    for (i = 0; i < 36; i += 3) {
        int f = (i / 6) * 3;          /* dos triángulos por cara: i va de 3 en 3 */
        const float *a = &cubo_v[cubo_f[i] * 3];
        const float *b = &cubo_v[cubo_f[i + 1] * 3];
        const float *c = &cubo_v[cubo_f[i + 2] * 3];
        glColor3f(cubo_c[f], cubo_c[f + 1], cubo_c[f + 2]);
        glVertex3f(a[0], a[1], a[2]);
        glVertex3f(b[0], b[1], b[2]);
        glVertex3f(c[0], c[1], c[2]);
    }
    glEnd();
}

/* Una pirámide de base cuadrada. */
static void desenha_piramide(void)
{
    if (malha) {
        glBegin(GL_LINES);
        glColor3f(0.95f, 0.85f, 0.35f);
        glVertex3f(-1, 0, -1); glVertex3f(1, 0, -1);
        glVertex3f(1, 0, -1);  glVertex3f(1, 0, 1);
        glVertex3f(1, 0, 1);   glVertex3f(-1, 0, 1);
        glVertex3f(-1, 0, 1);  glVertex3f(-1, 0, -1);
        glVertex3f(-1, 0, -1); glVertex3f(0, 1.6f, 0);
        glVertex3f(1, 0, -1);  glVertex3f(0, 1.6f, 0);
        glVertex3f(1, 0, 1);   glVertex3f(0, 1.6f, 0);
        glVertex3f(-1, 0, 1);  glVertex3f(0, 1.6f, 0);
        glEnd();
        return;
    }
    glBegin(GL_TRIANGLES);
    glColor3f(0.85f, 0.35f, 0.25f);
    glVertex3f(-1, 0, -1); glVertex3f(1, 0, -1); glVertex3f(0, 1.6f, 0);
    glColor3f(0.95f, 0.60f, 0.20f);
    glVertex3f(1, 0, -1); glVertex3f(1, 0, 1); glVertex3f(0, 1.6f, 0);
    glColor3f(0.95f, 0.80f, 0.30f);
    glVertex3f(1, 0, 1); glVertex3f(-1, 0, 1); glVertex3f(0, 1.6f, 0);
    glColor3f(0.70f, 0.30f, 0.55f);
    glVertex3f(-1, 0, 1); glVertex3f(-1, 0, -1); glVertex3f(0, 1.6f, 0);
    glColor3f(0.45f, 0.40f, 0.45f);
    glVertex3f(-1, 0, 1); glVertex3f(1, 0, 1); glVertex3f(1, 0, -1);
    glVertex3f(-1, 0, 1); glVertex3f(1, 0, -1); glVertex3f(-1, 0, -1);
    glEnd();
}

/* Dos hojas que se cruzan, más un anillo de líneas. */
static void desenha_cruz(void)
{
    int i;
    glBegin(GL_TRIANGLES);
    glColor3f(0.85f, 0.25f, 0.25f);
    glVertex3f(-1.6f, -1.0f, 0.6f); glVertex3f(1.6f, -1.0f, -0.6f); glVertex3f(0.0f, 1.3f, 0.0f);
    glColor3f(0.25f, 0.45f, 0.95f);
    glVertex3f(-1.6f, -1.0f, -0.6f); glVertex3f(1.6f, -1.0f, 0.6f); glVertex3f(0.0f, 1.3f, 0.0f);
    glEnd();
    glBegin(GL_LINES);
    glColor3f(0.95f, 0.90f, 0.40f);
    for (i = 0; i < 24; i++) {
        float a = (float)i * 0.2618f;          /* 15 grados */
        float b = (float)(i + 1) * 0.2618f;
        glVertex3f(2.2f * sgl_cos(a), 0.0f, 2.2f * sgl_sen(a));
        glVertex3f(2.2f * sgl_cos(b), 0.0f, 2.2f * sgl_sen(b));
    }
    glEnd();
}

/* Una ciudad de cajas: muestra bien la prueba de profundidad. */
static void desenha_cidade(void)
{
    int i, j, k;
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
            float h = 0.4f + (float)((i * 7 + j * 13) % 5) * 0.45f;
            float x = (float)(i - 2) * 1.6f;
            float z = (float)(j - 2) * 1.6f;
            glPushMatrix();
            glTranslatef(x, 0.0f, z);
            glScalef(0.6f, h, 0.6f);
            if (malha) {
                glBegin(GL_LINES);
                glColor3f(0.95f, 0.85f, 0.35f);
                for (k = 0; k < 24; k += 2) {
                    glVertex3f(cubo_v[cubo_e[k] * 3], cubo_v[cubo_e[k] * 3 + 1], cubo_v[cubo_e[k] * 3 + 2]);
                    glVertex3f(cubo_v[cubo_e[k + 1] * 3], cubo_v[cubo_e[k + 1] * 3 + 1], cubo_v[cubo_e[k + 1] * 3 + 2]);
                }
                glEnd();
            } else {
                glBegin(GL_TRIANGLES);
                for (k = 0; k < 36; k += 3) {
                    const float *a = &cubo_v[cubo_f[k] * 3];
                    const float *b = &cubo_v[cubo_f[k + 1] * 3];
                    const float *c = &cubo_v[cubo_f[k + 2] * 3];
                    float v = 0.35f + 0.15f * (float)((k + j) % 4);
                    glColor3f(v * 0.9f, v, v * 1.2f);
                    glVertex3f(a[0], a[1], a[2]);
                    glVertex3f(b[0], b[1], b[2]);
                    glVertex3f(c[0], c[1], c[2]);
                }
                glEnd();
            }
            glPopMatrix();
        }
    }
}

/* ---- la ventana ---- */

static void camara(void)
{
    glViewport(0, 0, LARG, ALT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustumf(-0.8f, 0.8f, -0.6f, 0.6f, 1.0f, 60.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, -0.6f, -distancia);
    glRotatef(tom, 1.0f, 0.0f, 0.0f);
    glRotatef(giro, 0.0f, 1.0f, 0.0f);
}

static void desenha_cena(void)
{
    fundo();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);
    camara();
    if (cena == 0) desenha_cubo();
    else if (cena == 1) desenha_piramide();
    else if (cena == 2) desenha_cruz();
    else desenha_cidade();
    if (cena != 2) grade(4);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}

/* Los colores distintos del cuadro (tabla abierta de 65536 huecos): sirve para
 * comparar la imagen del cc con la del GCC, que no dan exactamente los mismos
 * píxeles (el cc y el GCC redondean el coma flotante de otra manera). */
#define NCORES 65536
static uint32_t cores_tab[NCORES];
static unsigned char cores_uso[NCORES];
static int n_cores;

static void contar_cor(uint32_t c)
{
    unsigned h = ((c * 2654435761u) >> 20) & (NCORES - 1);
    unsigned i;
    for (i = 0; i < NCORES; i++) {
        unsigned j = (h + i) & (NCORES - 1);
        if (cores_uso[j]) { if (cores_tab[j] == c) return; }
        else { cores_uso[j] = 1; cores_tab[j] = c; n_cores++; return; }
    }
}

/* El modo de la suma: dibuja N cuadros en memoria (girando la escena como si
 * se pulsara la flecha derecha) y escribe la suma FNV del último cuadro. La
 * usa la prueba h99 para comparar el motor del cc con el del GCC. */
static uint32_t quadro[LARG * ALT];

static int modo_soma(int n)
{
    sgl_ctx_t *c = sgl_criar_contexto_mem(quadro, LARG, ALT, 16);
    uint32_t fundo;
    int i, pixels = 0;
    if (!c) { printf("sagui_gl: sin memoria para el contexto\n"); return 1; }
    for (i = 0; i < n; i++) {
        desenha_cena();
        if (i + 1 < n) giro += 4.0f;
    }
    fundo = quadro[0];
    for (i = 0; i < LARG * ALT; i++) {
        if (quadro[i] != fundo) pixels++;
        contar_cor(quadro[i]);
    }
    printf("soma %08x pixels %d cores %d\n", sgl_soma(quadro, LARG * ALT), pixels, n_cores);
    sgl_destruir_contexto(c);
    return 0;
}

int main(int argc, char **argv)
{
    sgl_ctx_t *ctx;
    int quadros = 0, fps = 0;
    unsigned marca = tempo_ms();
    char titulo[64];

    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '-') {
        if (argv[1][2] == 's' && argv[1][3] == 'o') {
            int n = (argc > 2) ? atoi(argv[2]) : 1;
            if (n < 1) n = 1;
            if (argc > 3) cena = atoi(argv[3]) - 1;
            if (cena < 0 || cena >= NCENAS) cena = 0;
            return modo_soma(n);
        }
        printf("sagui_gl: opcion desconocida: %s\n", argv[1]);
        return 1;
    }
    if (argc > 1) cena = atoi(argv[1]) - 1;
    if (cena < 0 || cena >= NCENAS) cena = 0;

    jid = __syscall(SIS_CRIAR_JANELA, (int)"SaguiGL: galeria", LARG | (ALT << 16), (int)&pixels);
    if (jid < 0 || !pixels) {
        printf("sagui_gl: no se pudo abrir la ventana\n");
        return 1;
    }
    ctx = sgl_criar_contexto_mem(pixels, LARG, ALT, 16);
    if (!ctx) {
        printf("sagui_gl: sin memoria para el contexto\n");
        return 1;
    }

    for (;;) {
        evento_t e;
        int mudou = 0;
        unsigned agora;
        while (__syscall(SIS_PROXIMO_EVENTO, jid, (int)&e, 0) > 0) {
            if (e.tipo == EV_FECHAR) { __syscall(SIS_FECHAR_JANELA, jid, 0, 0); return 0; }
            if (e.tipo != EV_TECLA) continue;
            if (e.tecla == TECLA_ESC) { __syscall(SIS_FECHAR_JANELA, jid, 0, 0); return 0; }
            if (e.tecla == TECLA_ESQUERDA) { giro -= 4.0f; mudou = 1; }
            else if (e.tecla == TECLA_DIREITA) { giro += 4.0f; mudou = 1; }
            else if (e.tecla == TECLA_CIMA) { tom -= 3.0f; mudou = 1; }
            else if (e.tecla == TECLA_BAIXO) { tom += 3.0f; mudou = 1; }
            else if (e.tecla == '+' || e.tecla == '=') { distancia -= 0.5f; mudou = 1; }
            else if (e.tecla == '-' || e.tecla == '_') { distancia += 0.5f; mudou = 1; }
            else if (e.tecla == '1' || e.tecla == '2' || e.tecla == '3' || e.tecla == '4') {
                cena = (int)e.tecla - '1'; mudou = 1;
            }
            else if (e.tecla == 'w' || e.tecla == 'W') { malha = !malha; mudou = 1; }
            else if (e.tecla == 'f' || e.tecla == 'F') { fundo_claro = !fundo_claro; mudou = 1; }
        }
        if (distancia < 2.0f) distancia = 2.0f;
        if (distancia > 20.0f) distancia = 20.0f;

        desenha_cena();
        __syscall(SIS_ATUALIZAR_JANELA, jid, 0, 0);
        quadros++;
        agora = tempo_ms();
        if (agora - marca >= 1000) {
            fps = quadros * 1000 / (int)(agora - marca);
            quadros = 0;
            marca = agora;
            sprintf(titulo, "SaguiGL: %s - %d fps%s", nome_cena[cena], fps, malha ? " (malla)" : "");
            __syscall(SIS_TITULO_JANELA, jid, (int)titulo, 0);
        }
        if (!mudou) dormir(16);
        else dormir(1);
    }
}
