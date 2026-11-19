// SPDX-License-Identifier: GPL-2.0-or-later
/* h405_anfitriao.c - SaguiGL realista (D404) en el anfitrión, para tests/h405_saguigl.py.
 *
 *   h405_anfitriao unidad            pruebas del filtro, los mipmaps, la perspectiva, la luz,
 *                                    los formatos y el lector de OBJ; imprime «unidad: N bien»
 *   h405_anfitriao somas             las sumas de las escenas de «sgldemo -c paisaje|avion»
 *   h405_anfitriao ppm DIR L A [obj] imágenes (PPM) del paisaje y del avión, para las capturas
 *   h405_anfitriao obj FICH.obj      lee un OBJ y dice triángulos, materiales y caja
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../userland/libsgl/sgl.h"
#include "../../userland/libsgl/sgl_obj.h"
#include "../../userland/libsgl/paisaje.h"

static int bien, mal;

static void ver(int c, const char *m)
{
    if (c) bien++;
    else { mal++; printf("MAL: %s\n", m); }
}

#define L 64
#define A 64
static uint32_t px[L * A];

static int canal(uint32_t p, int k) { return (int)((p >> (16 - 8 * k)) & 0xFF); }
static int perto(int a, int b, int tol) { return a - b <= tol && b - a <= tol; }

/* Un cuadrado que llena la pantalla 64x64 (ortográfica), con uv de 0 a `rep`. */
static void quadrado(float rep)
{
    glViewport(0, 0, L, A);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrthof(0, 1, 0, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glColor4f(1, 1, 1, 1);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(0, rep); glVertex2f(0, 0);
    glTexCoord2f(rep, rep); glVertex2f(1, 0);
    glTexCoord2f(rep, 0); glVertex2f(1, 1);
    glTexCoord2f(0, 0); glVertex2f(0, 1);
    glEnd();
}

static void textura_xadrez(int lado, GLint fmin, GLint fmag)
{
    static unsigned char t[128 * 128 * 4];
    for (int y = 0; y < lado; y++)
        for (int x = 0; x < lado; x++) {
            unsigned char v = ((x ^ y) & 1) ? 255 : 0;
            unsigned char *q = t + (y * lado + x) * 4;
            q[0] = q[1] = q[2] = v; q[3] = 255;
        }
    glBindTexture(GL_TEXTURE_2D, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, fmin);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, fmag);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, lado, lado, 0, GL_RGBA, GL_UNSIGNED_BYTE, t);
    glEnable(GL_TEXTURE_2D);
}

static void unidade_mip(void)
{
    /* Un ajedrez de 1 texel, 128x128 en 64x64 píxeles (reducido 2 veces): sin mipmaps salen
     * blancos y negros; con NEAREST_MIPMAP_NEAREST, el nivel 1 = gris medio en todos. */
    sgl_ctx_t *c = sgl_criar_contexto_mem(px, L, A, 32);
    textura_xadrez(128, GL_NEAREST, GL_NEAREST);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    quadrado(1);
    int extremos = 0;
    for (int i = 0; i < L * A; i++) if (canal(px[i], 0) == 0 || canal(px[i], 0) == 255) extremos++;
    ver(extremos == L * A, "sin mipmaps, el ajedrez reducido es blanco y negro");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    quadrado(1);
    int gris = 0;
    for (int i = 0; i < L * A; i++) if (perto(canal(px[i], 0), 128, 1)) gris++;
    ver(gris == L * A, "con mipmaps (nivel 1), gris medio");
    /* 4 veces más pequeño (uv de 0 a 2 en 64 píxeles para 128 texels: rho = 4) -> nivel 2 */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    quadrado(2);
    gris = 0;
    for (int i = 0; i < L * A; i++) if (perto(canal(px[i], 0), 128, 2)) gris++;
    ver(gris == L * A, "trilineal, rho 4: gris medio");
    /* glGenerateMipmap y niveles dados a mano: el nivel 1 rojo puro se usa tal cual */
    static unsigned char rojo[64 * 64 * 4];
    for (int i = 0; i < 64 * 64; i++) { rojo[i * 4] = 255; rojo[i * 4 + 1] = 0; rojo[i * 4 + 2] = 0; rojo[i * 4 + 3] = 255; }
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, rojo);
    ver(glGetError() == GL_NO_ERROR, "glTexImage2D del nivel 1 sin error");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    quadrado(1);
    ver(canal(px[L * 10 + 10], 0) == 255 && canal(px[L * 10 + 10], 1) == 0, "el nivel 1 dado por el programa se usa");
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, rojo);
    ver(glGetError() == GL_INVALID_VALUE, "un nivel con el tamaño mal: GL_INVALID_VALUE");
    sgl_destruir_contexto(c);
}

static void unidade_bilinear(void)
{
    /* 2x2 (negro, blanco / blanco, negro) ampliada a 64x64 con borde: degradado suave. */
    sgl_ctx_t *c = sgl_criar_contexto_mem(px, L, A, 32);
    static const unsigned char t[16] = { 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255 };
    glBindTexture(GL_TEXTURE_2D, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, t);
    glEnable(GL_TEXTURE_2D);
    quadrado(1);
    int maxd = 0, niveles = 0;
    unsigned char visto[256] = { 0 };
    for (int y = 0; y < A; y++)
        for (int x = 0; x < L; x++) {
            /* lo que debe dar: s = u*2 - 0.5 sujeto, interpolación de los cuatro */
            float u = (x + 0.5f) / L, v = (y + 0.5f) / A;
            float s = u * 2 - 0.5f, r = v * 2 - 0.5f;
            s = s < 0 ? 0 : s > 1 ? 1 : s;
            r = r < 0 ? 0 : r > 1 ? 1 : r;
            float e = (0 * (1 - s) + 255 * s) * (1 - r) + (255 * (1 - s) + 0 * s) * r;
            int d = canal(px[y * L + x], 0) - (int)(e + 0.5f);
            if (d < 0) d = -d;
            if (d > maxd) maxd = d;
            if (!visto[canal(px[y * L + x], 0)]) { visto[canal(px[y * L + x], 0)] = 1; niveles++; }
        }
    ver(maxd <= 2, "bilineal: igual que la fórmula (+-2)");
    ver(niveles > 40, "bilineal: degradado (muchos valores distintos)");
    ver(canal(px[0], 0) == 0 && canal(px[(A - 1) * L], 0) == 255, "bilineal: esquinas con su texel");
    sgl_destruir_contexto(c);
}

static void unidade_perspectiva(void)
{
    /* Un suelo inclinado con una textura que va de negro (u = 0) a blanco (u = 1) en x.
     * El punto medio del mundo (x = 0, z = -4) se proyecta en x = 0 de la pantalla: con
     * perspectiva tiene u = 0,5 (gris); sin ella (afín) el gris cae en otro sitio. */
    sgl_ctx_t *c = sgl_criar_contexto_mem(px, L, A, 32);
    static unsigned char t[256 * 4];
    for (int i = 0; i < 256; i++) { t[i * 4] = t[i * 4 + 1] = t[i * 4 + 2] = (unsigned char)i; t[i * 4 + 3] = 255; }
    glBindTexture(GL_TEXTURE_2D, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, t);
    glEnable(GL_TEXTURE_2D);
    glViewport(0, 0, L, A);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glFrustumf(-1, 1, -1, 1, 1, 20);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    /* de (-2, z -2) a (2, z -6): el centro del mundo, (0, -4), cae en la columna 32 */
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(0, 0); glVertex3f(-2, -1, -2);
    glTexCoord2f(1, 0); glVertex3f(2, -1, -6);
    glTexCoord2f(1, 1); glVertex3f(2, 1, -6);
    glTexCoord2f(0, 1); glVertex3f(-2, 1, -2);
    glEnd();
    int v = canal(px[32 * L + 32], 0);
    ver(perto(v, 128, 6), "perspectiva: el centro del mundo tiene u = 0,5");
    /* sin perspectiva (GL_FASTEST y GL_NEAREST: la ruta de v6) el centro de la pantalla no es 0,5 */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(0, 0); glVertex3f(-2, -1, -2);
    glTexCoord2f(1, 0); glVertex3f(2, -1, -6);
    glTexCoord2f(1, 1); glVertex3f(2, 1, -6);
    glTexCoord2f(0, 1); glVertex3f(-2, 1, -2);
    glEnd();
    ver(!perto(canal(px[32 * L + 32], 0), 128, 20), "la ruta de v6 sigue siendo afín (no cambia de píxel)");
    sgl_destruir_contexto(c);
}

static void unidade_luz(void)
{
    sgl_ctx_t *c = sgl_criar_contexto_mem(px, L, A, 32);
    glViewport(0, 0, L, A);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrthof(0, 1, 0, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    static const float z0[4] = { 0, 0, 0, 1 }, dif[4] = { 0.5f, 0.5f, 0.5f, 1 }, luz[4] = { 0, 0, 1, 0 };
    static const float branca[4] = { 1, 1, 1, 1 };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, z0);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, z0);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, dif);
    glLightfv(GL_LIGHT0, GL_POSITION, luz);
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    glNormal3f(0, 0, 1);
    quadrado(1);
    ver(perto(canal(px[100], 0), 128, 1), "difusa de frente: 0,5 x 1 x cos 0 = 128");
    /* la luz a 60 grados: cos = 0,5 -> 64 */
    static const float l60[4] = { 0.8660254f, 0, 0.5f, 0 };
    glLightfv(GL_LIGHT0, GL_POSITION, l60);
    quadrado(1);
    ver(perto(canal(px[100], 0), 64, 1), "difusa a 60 grados: 64");
    /* especular: material 1, brillo 1, luz de frente: n.h = 1 -> blanco */
    glLightfv(GL_LIGHT0, GL_POSITION, luz);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, branca);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 1);
    quadrado(1);
    ver(canal(px[100], 0) == 255, "especular de frente: blanco");
    /* dos luces: la 1 roja, de frente, sin especular */
    static const float roja[4] = { 1, 0, 0, 1 };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, z0);
    glLightfv(GL_LIGHT0, GL_POSITION, l60);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, roja);
    glLightfv(GL_LIGHT1, GL_POSITION, luz);
    glEnable(GL_LIGHT1);
    quadrado(1);
    ver(perto(canal(px[100], 0), 191, 1) && perto(canal(px[100], 1), 64, 1), "dos luces se suman (191, 64)");
    /* atenuación de una luz puntual a distancia 2 con k1 = 1: 1/(1+2) */
    glDisable(GL_LIGHT1);
    static const float pt[4] = { 0.5f, 0.5f, 2.0f, 1 };
    glLightfv(GL_LIGHT0, GL_POSITION, pt);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 1);
    glBegin(GL_TRIANGLES);   /* un triangulito justo debajo de la luz */
    glVertex3f(0.49f, 0.49f, 0); glVertex3f(0.51f, 0.49f, 0); glVertex3f(0.5f, 0.52f, 0);
    glEnd();
    ver(perto(canal(px[32 * L + 32], 0), 43, 2), "atenuación: 128/3 = 43");
    ver(glGetError() == GL_NO_ERROR, "sin errores de GL");
    sgl_destruir_contexto(c);
}

static void unidade_formatos(void)
{
    sgl_ctx_t *c = sgl_criar_contexto_mem(px, L, A, 32);
    glBindTexture(GL_TEXTURE_2D, 1);
    glEnable(GL_TEXTURE_2D);
    uint16_t t565 = (uint16_t)((31u << 11) | (32u << 5) | 0u);   /* rojo pleno, verde medio */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, &t565);
    quadrado(1);
    ver(canal(px[5], 0) == 255 && canal(px[5], 1) == 130 && canal(px[5], 2) == 0, "RGB565");
    uint16_t t4 = 0x1F8Fu;   /* r 1, g 15, b 8, a 15 */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, &t4);
    quadrado(1);
    ver(canal(px[5], 0) == 17 && canal(px[5], 1) == 255 && canal(px[5], 2) == 136, "RGBA4444");
    unsigned char lum = 77;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, &lum);
    quadrado(1);
    ver(canal(px[5], 0) == 77 && canal(px[5], 1) == 77 && canal(px[5], 2) == 77, "luminancia");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, &t565);
    ver(glGetError() == GL_INVALID_OPERATION, "RGBA con 565: GL_INVALID_OPERATION");
    /* texturas grandes: 2048 vale, 4096 no */
    unsigned char *g = calloc(2048 * 2048, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2048, 2048, 0, GL_RGBA, GL_UNSIGNED_BYTE, g);
    ver(glGetError() == GL_NO_ERROR, "textura de 2048x2048");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4096, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, g);
    ver(glGetError() == GL_INVALID_VALUE, "4096: GL_INVALID_VALUE");
    free(g);
    /* muchas texturas: 300 distintas */
    GLuint ids[300];
    memset(ids, 0, sizeof ids);
    glGenTextures(300, ids);
    ver(ids[299] != 0, "300 texturas en un contexto");
    sgl_estat_t e;
    sgl_estatisticas(&e, 1);
    ver(e.tex_kb >= 16384, "las cuentas ven los 16 MB de la textura grande");
    sgl_destruir_contexto(c);
}

static const char obj_cubo[] =
    "# un cubo con caras de 4 vértices, índices negativos y dos materiales\n"
    "mtllib cubo.mtl\n"
    "v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
    "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
    "vn 0 0 -1\nvn 0 0 1\n"
    "usemtl rojo\n"
    "f 1/1/1 4/4/1 3/3/1 2/2/1\n"
    "f 5/1/2 6/2/2 7/3/2 8/4/2\n"
    "usemtl azul\n"
    "f 1//1 5//1 8//1 4//1\n"
    "f -7 -6 -2 -3\n"
    "f 4 8 7 3\n"
    "f 1 2 6 5\n";
static const char mtl_cubo[] = "newmtl rojo\nKd 1 0 0\nNs 500\nmap_Kd ladrillo.png\nnewmtl azul\nKd 0 0 1.0e0\nd 0.5\n";

static void unidade_obj(void)
{
    sgl_obj_t m;
    memset(&m, 0, sizeof m);
    int r = sgl_obj_ler(obj_cubo, (int)strlen(obj_cubo), &m);
    ver(r == 0, "OBJ: se lee sin error");
    ver(m.n_vert == 36, "OBJ: 6 caras de 4 = 12 triángulos");
    ver(m.n_grupos == 2 && m.grupo[0].n == 12 && m.grupo[1].n == 24, "OBJ: dos tramos de material");
    ver(m.min[0] == -1 && m.max[2] == 1, "OBJ: caja");
    ver(!strcmp(m.mtllib, "cubo.mtl"), "OBJ: mtllib");
    sgl_obj_ler_mtl(mtl_cubo, (int)strlen(mtl_cubo), &m);
    ver(m.n_mats == 2 && m.mat[0].kd[0] == 1 && m.mat[1].kd[2] == 1 && m.mat[1].d == 0.5f, "MTL: Kd y d");
    ver(m.mat[0].ns == 64 && !strcmp(m.mat[0].mapa, "ladrillo.png"), "MTL: Ns (a 0..128) y map_Kd");
    /* la cara con normales del fichero y la de abajo con la de su cara (0,-1,0) */
    ver(m.nor[2] == -1, "OBJ: normal del fichero");
    ver(m.nor[(30) * 3 + 1] < -0.99f, "OBJ: normal de la cara si falta");
    ver(m.uv[1] == 1.0f, "OBJ: v volteada (origen arriba)");
    sgl_obj_liberar(&m);
    memset(&m, 0, sizeof m);
    ver(sgl_obj_ler("v 0 0 0\nf 1 2 3\n", 16, &m) == -3, "OBJ: índice fuera: -3");
    memset(&m, 0, sizeof m);
    ver(sgl_obj_ler("# nada\n", 7, &m) == -2, "OBJ: sin triángulos: -2");
}

/* La escena del paisaje como en «sgldemo -c»: 160x120, cuadros 0 y 40, Z de 16 y 32. */
static void somas(void)
{
    static uint32_t p[160 * 120];
    const char *nomes[2] = { "paisaje", "avion" };
    for (int e = 0; e < 2; e++)
        for (int z = 16; z <= 32; z += 16) {
            sgl_ctx_t *c = sgl_criar_contexto_mem(p, 160, 120, z);
            pais_opcoes_t o;
            memset(&o, 0, sizeof o);
            o.mip = 1; o.detalle = 48; o.camara = e;
            paisaje_iniciar(&o);
            for (int q = 0; q <= 40; q += 40) {
                paisaje_cuadro(q, 160, 120);
                printf("%s z%d q%d %08x\n", nomes[e], z, q, sgl_soma(p, 160 * 120));
            }
            paisaje_fim();
            sgl_destruir_contexto(c);
        }
}

static void gravar(const char *n, const uint32_t *p, int l, int a)
{
    FILE *f = fopen(n, "wb");
    fprintf(f, "P6\n%d %d\n255\n", l, a);
    for (int k = 0; k < l * a; k++) { fputc(p[k] >> 16, f); fputc(p[k] >> 8, f); fputc(p[k], f); }
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "unidad")) {
        unidade_mip(); unidade_bilinear(); unidade_perspectiva(); unidade_luz(); unidade_formatos(); unidade_obj();
        printf("unidad: %d bien, %d mal\n", bien, mal);
        return mal != 0;
    }
    if (argc > 1 && !strcmp(argv[1], "somas")) { somas(); return 0; }
    if (argc > 2 && !strcmp(argv[1], "obj")) {
        sgl_obj_t m;
        memset(&m, 0, sizeof m);
        int r = sgl_obj_cargar(argv[2], &m);
        printf("obj %d triangulos %d materiales %d grupos %d uv %d caja %.2f %.2f %.2f / %.2f %.2f %.2f\n", r, m.n_vert / 3,
               m.n_mats, m.n_grupos, m.tem_uv, m.min[0], m.min[1], m.min[2], m.max[0], m.max[1], m.max[2]);
        sgl_obj_liberar(&m);
        return r < 0;
    }
    if (argc > 4 && !strcmp(argv[1], "ppm")) {
        int l = atoi(argv[3]), a = atoi(argv[4]);
        uint32_t *p = calloc((size_t)l * a, 4);
        sgl_obj_t m;
        memset(&m, 0, sizeof m);
        int com_obj = argc > 5 && sgl_obj_cargar(argv[5], &m) >= 0;
        for (int e = 0; e < 3; e++) {
            sgl_ctx_t *c = sgl_criar_contexto_mem(p, l, a, 32);
            pais_opcoes_t o;
            memset(&o, 0, sizeof o);
            o.mip = 1; o.camara = e == 0 ? 0 : 1; o.texturas_mb = 0;
            if (e == 2) { if (!com_obj) { sgl_destruir_contexto(c); break; } o.aviao = &m; o.giro_aviao = argc > 6 ? (float)atof(argv[6]) : 0; }
            paisaje_iniciar(&o);
            paisaje_cuadro(e == 0 ? 60 : 30, l, a);
            char n[512];
            snprintf(n, sizeof n, "%s/%s.ppm", argv[2], e == 0 ? "paisaje" : e == 1 ? "avion" : "avion_obj");
            gravar(n, p, l, a);
            paisaje_fim();
            sgl_destruir_contexto(c);
        }
        return 0;
    }
    fprintf(stderr, "uso: h405_anfitriao unidad | somas | ppm DIR L A [obj [giro]] | obj FICH\n");
    return 2;
}
