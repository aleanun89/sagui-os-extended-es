// SPDX-License-Identifier: GPL-2.0-or-later
/* cenas.c - escenas fijas de SaguiGL (ver cenas.h). */
#include "cenas.h"

static const float cubo_v[8][3] = {
    { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 },
    { -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 } };
static const float cubo_c[8][3] = {
    { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 0 },
    { 1, 0, 1 }, { 0, 1, 1 }, { 1, 1, 1 }, { 0.3f, 0.3f, 0.3f } };
/* 12 triángulos, sentido antihorario visto desde fuera. */
static const uint16_t cubo_i[36] = {
    0, 3, 2,  0, 2, 1,    /* atrás  (z = -1) */
    4, 5, 6,  4, 6, 7,    /* frente (z = +1) */
    0, 4, 7,  0, 7, 3,    /* izquierda */
    1, 2, 6,  1, 6, 5,    /* derecha */
    3, 7, 6,  3, 6, 2,    /* arriba */
    0, 1, 5,  0, 5, 4 };  /* abajo */

static void proyecao_tam(int l, int a)
{
    glViewport(0, 0, l, a);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustumf(-0.8f, 0.8f, -0.6f, 0.6f, 1.0f, 20.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void proyecao(void) { proyecao_tam(CENA_L, CENA_A); }

void cena_cubo_tam(float graus, int l, int a)
{
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);
    proyecao_tam(l, a);
    glTranslatef(0, 0, -5);
    glRotatef(graus, 1, 1, 0);
    glRotatef(graus * 0.5f, 0, 0, 1);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, cubo_v);
    glColorPointer(3, GL_FLOAT, 0, cubo_c);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, cubo_i);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}

void cena_cubo(float graus) { cena_cubo_tam(graus, CENA_L, CENA_A); }

void cena_triangulo(void)
{
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, CENA_L, CENA_A);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(-1.333f, 1.333f, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glBegin(GL_TRIANGLES);
    glColor3f(1, 0, 0); glVertex2f(-1.0f, -0.8f);
    glColor3f(0, 1, 0); glVertex2f(1.0f, -0.6f);
    glColor3f(0, 0, 1); glVertex2f(0.1f, 0.9f);
    glEnd();
    /* sombreado plano: color del último vértice */
    glShadeModel(GL_FLAT);
    glBegin(GL_TRIANGLES);
    glColor3f(1, 1, 0); glVertex2f(-1.2f, 0.3f);
    glColor3f(1, 0, 1); glVertex2f(-0.7f, 0.9f);
    glColor3f(0, 1, 1); glVertex2f(-0.4f, 0.3f);
    glEnd();
    glShadeModel(GL_SMOOTH);
}

void cena_z_cruzado(void)
{
    glClearColor(0.2f, 0.2f, 0.2f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    proyecao();
    glTranslatef(0, 0, -4);
    glBegin(GL_TRIANGLES);        /* inclinado hacia dentro */
    glColor3f(1, 0.2f, 0.2f);
    glVertex3f(-1.5f, -1.0f, 1.0f); glVertex3f(1.5f, -1.0f, -1.0f); glVertex3f(0.0f, 1.2f, 0.0f);
    glEnd();
    glBegin(GL_TRIANGLES);        /* el opuesto: se cruza con el anterior */
    glColor3f(0.2f, 0.4f, 1);
    glVertex3f(-1.5f, -1.0f, -1.0f); glVertex3f(1.5f, -1.0f, 1.0f); glVertex3f(0.0f, 1.2f, 0.0f);
    glEnd();
    glDisable(GL_DEPTH_TEST);
    /* líneas y abanico sin Z, para cubrir LINES y TRIANGLE_FAN */
    static const float fan[5][2] = { { -1.2f, -0.95f }, { -1.5f, -0.7f }, { -1.0f, -0.65f }, { -0.9f, -0.85f }, { -1.1f, -1.0f } };
    glEnableClientState(GL_VERTEX_ARRAY);
    glColor3f(0.9f, 0.9f, 0.2f);
    glVertexPointer(2, GL_FLOAT, 0, fan);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 5);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBegin(GL_LINES);
    glColor3f(1, 1, 1); glVertex3f(-2.0f, 1.2f, 0); glColor3f(0, 1, 0); glVertex3f(2.0f, 0.9f, 0);
    glVertex3f(-1.0f, 1.5f, 0); glVertex3f(3.0f, -0.5f, 0);   /* sale del frustum: se recorta */
    glEnd();
}

static int igual(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

int cena_por_nome(const char *n)
{
    if (igual(n, "triangulo")) { cena_triangulo(); return 1; }
    if (igual(n, "cubo")) { cena_cubo(30.0f); return 1; }
    if (igual(n, "zcruzado")) { cena_z_cruzado(); return 1; }
    return 0;
}
