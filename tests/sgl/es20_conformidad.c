// SPDX-License-Identifier: GPL-3.0-only
/* es20_conformidad.c - pruebas de conformidad de OpenGL ES 2.0 de SaguiGL 2.0 (D475).
 *
 * Escrito SOLO con las cabeceras estándar (GLES2/gl2.h, EGL/egl.h). Compilación (programas válidos y
 * errores con su log y su línea), enlace, consultas, uniformes, errores de la API, las funciones embutidas
 * de GLSL ES 1.00 (cada una pinta un color que se compara con el valor calculado aparte, ±1 o ±2 de
 * redondeo), control de flujo, y escenas comparadas píxel a píxel con lo que dice la especificación:
 * interpolación con perspectiva, Z, esténcil, tijera, mezcla, texturas (filtros, mipmaps, cubos), FBO,
 * discard, puntos, líneas, caras, índices. Termina alternando con un contexto de ES 1.1.
 *
 * Es determinista: la suma de todos los píxeles leídos debe ser la misma en el anfitrión y en el SaguiOS
 * (tests/h475_saguigl2.py). Salida: una línea por fallo, «conformidad: N bien, M mal» y «suma <hex>». */
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>

#if defined(SGL_ANFITRIAO)
#include <stdio.h>
#include <string.h>
#define IMPRIMIR(...) printf(__VA_ARGS__)
#else
#include "sagui.h"
#define IMPRIMIR(...) imprimir(__VA_ARGS__)
static const char *strstr(const char *h, const char *n)
{
    for (; *h; h++) { const char *a = h, *b = n; while (*a && *b && *a == *b) { a++; b++; } if (!*b) return h; }
    return 0;
}
static int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (unsigned char)*a - (unsigned char)*b; }
#endif

#define W 64
#define H 64
static int bien, mal;
static unsigned soma = 2166136261u;

static void ver(int ok, const char *que)
{
    if (ok) bien++;
    else { mal++; IMPRIMIR("MAL: %s\n", que); }
}
static unsigned char lido[W * H * 4];
static const unsigned char *ler_quadro(void)
{
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, lido);
    for (int i = 0; i < W * H * 4; i++) { soma ^= lido[i]; soma *= 16777619u; }
    return lido;
}
static const unsigned char *px(int x, int y) { return lido + (y * W + x) * 4; }
static int perto(int a, int b, int tol) { return a - b <= tol && b - a <= tol; }
static void verpx(int x, int y, int r, int g, int b, int tol, const char *que)
{
    const unsigned char *p = px(x, y);
    if (perto(p[0], r, tol) && perto(p[1], g, tol) && perto(p[2], b, tol)) { bien++; return; }
    mal++;
    IMPRIMIR("MAL: %s (%d,%d): %d %d %d, se esperaba %d %d %d\n", que, x, y, p[0], p[1], p[2], r, g, b);
}

/* ------------------------------------------------------------ sombreadores */
static char logbuf[1024];
static GLuint sombreador(GLenum tipo, const char *f, int *ok)
{
    GLuint s = glCreateShader(tipo);
    glShaderSource(s, 1, &f, 0);
    glCompileShader(s);
    GLint st = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &st);
    logbuf[0] = 0; glGetShaderInfoLog(s, sizeof logbuf, 0, logbuf);
    *ok = st;
    return s;
}
static GLuint programa(const char *vs, const char *fs)
{
    int a, b;
    GLuint p = glCreateProgram();
    GLuint v = sombreador(GL_VERTEX_SHADER, vs, &a);
    if (!a) IMPRIMIR("  (vértices) %s", logbuf);
    GLuint f = sombreador(GL_FRAGMENT_SHADER, fs, &b);
    if (!b) IMPRIMIR("  (fragmentos) %s", logbuf);
    glAttachShader(p, v); glAttachShader(p, f);
    glBindAttribLocation(p, 0, "a_pos");
    glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { logbuf[0] = 0; glGetProgramInfoLog(p, sizeof logbuf, 0, logbuf); IMPRIMIR("  (enlace) %s", logbuf); }
    return p;
}
static const char *VS_QUAD =
    "attribute vec2 a_pos;\n"
    "uniform float u_z;\n"
    "varying vec2 v_tc;\n"
    "void main() { v_tc = a_pos * 0.5 + 0.5; gl_Position = vec4(a_pos, u_z, 1.0); }\n";
static void quad_em(GLuint p, float x0, float y0, float x1, float y1, float z)
{
    float q[8] = { x0, y0, x1, y0, x1, y1, x0, y1 };
    glUseProgram(p);
    GLint lz = glGetUniformLocation(p, "u_z");
    if (lz >= 0) glUniform1f(lz, z);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, q);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
static void quad(GLuint p) { quad_em(p, -1, -1, 1, 1, 0); }
static GLuint cor_fixa(void)
{
    static GLuint p;
    if (!p) p = programa(VS_QUAD, "precision mediump float; uniform vec4 u_cor; void main() { gl_FragColor = u_cor; }\n");
    return p;
}
static void quad_cor(float x0, float y0, float x1, float y1, float z, float r, float g, float b, float a)
{
    GLuint p = cor_fixa();
    glUseProgram(p);
    glUniform4f(glGetUniformLocation(p, "u_cor"), r, g, b, a);
    quad_em(p, x0, y0, x1, y1, z);
}

/* ------------------------------------------------------------ compilación y enlace */
static int strlen_seguro(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int compila(GLenum tipo, const char *f) { int ok; GLuint s = sombreador(tipo, f, &ok); glDeleteShader(s); return ok; }
static void falha_compila(GLenum tipo, const char *f, const char *no_log, const char *que)
{
    int ok = compila(tipo, f);
    int tem = no_log ? strstr(logbuf, no_log) != 0 : 1;
    if (!ok && tem && strstr(logbuf, "ERROR: 0:")) bien++;
    else { mal++; IMPRIMIR("MAL: %s: compila=%d log=«%s»\n", que, ok, logbuf); }
}
static void t_compilacao(void)
{
    ver(compila(GL_VERTEX_SHADER, VS_QUAD), "compila un sombreador de vértices sencillo");
    ver(compila(GL_FRAGMENT_SHADER, "precision mediump float; void main() { gl_FragColor = vec4(1.0); }"), "compila un sombreador de fragmentos sencillo");
    ver(compila(GL_FRAGMENT_SHADER,
        "#version 100\n#ifdef GL_ES\nprecision highp float;\n#endif\n#define DOBRO(x) ((x) * 2.0)\n"
        "struct L { vec3 cor; float k[2]; };\nuniform L luzes[2];\nconst int N = 2;\n"
        "float soma(in float a, const in float b) { return a + b; }\n"
        "void main() { vec3 c = vec3(0.0); for (int i = 0; i < N; i++) c += luzes[i].cor * luzes[i].k[1];\n"
        "  gl_FragColor = vec4(c, DOBRO(soma(0.1, 0.2))); }\n"), "compila preprocesador, structs, arrays, const y funciones");
    falha_compila(GL_FRAGMENT_SHADER, "void main() {\n  float x = ;\n}\n", "0:2:", "error de sintaxis con su línea");
    falha_compila(GL_FRAGMENT_SHADER, "void main() { float x = 1; gl_FragColor = vec4(x); }", 0, "int no se convierte a float");
    falha_compila(GL_FRAGMENT_SHADER, "void main() { gl_FragColor = vec4(1.0) + 1; }", 0, "vec4 + int es un error");
    falha_compila(GL_FRAGMENT_SHADER, "void main() {\n\n  gl_FragColor = vec4(y);\n}\n", "0:3:", "identificador sin declarar (línea 3)");
    falha_compila(GL_FRAGMENT_SHADER, "uniform float u; void main() { u = 1.0; gl_FragColor = vec4(u); }", 0, "escribir en un uniforme");
    falha_compila(GL_FRAGMENT_SHADER, "void f() { gl_FragColor = vec4(1.0); }", "main", "falta main");
    falha_compila(GL_FRAGMENT_SHADER, "float f(float x) { return f(x); } void main() { gl_FragColor = vec4(f(1.0)); }", 0, "recursión");
    falha_compila(GL_VERTEX_SHADER, "void main() { discard; }", "discard", "discard en vértices");
    falha_compila(GL_VERTEX_SHADER, "attribute float a; void main() { gl_Position = vec4(dFdx(a)); }", 0, "derivadas en vértices");
    falha_compila(GL_FRAGMENT_SHADER, "void main() { vec3 v = vec3(1.0); v.xx = vec2(2.0); gl_FragColor = vec4(v, 1.0); }", 0, "swizzle repetido a la izquierda");
    falha_compila(GL_FRAGMENT_SHADER, "void main() { if (1.0) gl_FragColor = vec4(1.0); }", "bool", "condición que no es bool");
    falha_compila(GL_FRAGMENT_SHADER, "void main() { float a[2]; a[3] = 1.0; gl_FragColor = vec4(a[0]); }", 0, "índice constante fuera del array");
    falha_compila(GL_FRAGMENT_SHADER, "varying vec2 v; void main() { v = vec2(1.0); gl_FragColor = vec4(1.0); }", 0, "escribir un varying en fragmentos");
    /* consultas del sombreador */
    GLuint s = glCreateShader(GL_FRAGMENT_SHADER);
    const char *f = "void main() { gl_FragColor = vec4(0.5); }";
    glShaderSource(s, 1, &f, 0);
    GLint v = 0; glGetShaderiv(s, GL_SHADER_SOURCE_LENGTH, &v);
    ver(v == (GLint)strlen_seguro(f) + 1, "GL_SHADER_SOURCE_LENGTH");
    glGetShaderiv(s, GL_SHADER_TYPE, &v); ver(v == GL_FRAGMENT_SHADER, "GL_SHADER_TYPE");
    char b[128]; glGetShaderSource(s, sizeof b, 0, b); ver(!strcmp(b, f), "glGetShaderSource");
    glDeleteShader(s);
    ver(!glIsShader(s), "glDeleteShader sin programa lo borra");
}
static void t_ligacao(void)
{
    /* varying que falta en vértices */
    GLuint p = glCreateProgram();
    int a, b;
    GLuint v = sombreador(GL_VERTEX_SHADER, "attribute vec4 a_pos; void main() { gl_Position = a_pos; }", &a);
    GLuint f = sombreador(GL_FRAGMENT_SHADER, "precision mediump float; varying vec3 v_x; void main() { gl_FragColor = vec4(v_x, 1.0); }", &b);
    glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    GLint ok = 1; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    logbuf[0] = 0; glGetProgramInfoLog(p, sizeof logbuf, 0, logbuf);
    ver(a && b && !ok && strstr(logbuf, "v_x"), "enlace: varying sin escribir en vértices (con el nombre en el log)");
    glUseProgram(p);
    ver(glGetError() == GL_INVALID_OPERATION, "glUseProgram de un programa sin enlazar: GL_INVALID_OPERATION");
    glDeleteProgram(p); glDeleteShader(v); glDeleteShader(f);
    /* varyings con tipos distintos */
    p = programa("attribute vec4 a_pos; varying vec2 v_x; void main() { v_x = a_pos.xy; gl_Position = a_pos; }",
                 "precision mediump float; varying vec3 v_x; void main() { gl_FragColor = vec4(v_x, 1.0); }");
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    ver(!ok, "enlace: varying con tipos distintos falla");
    glDeleteProgram(p);
    /* atributos, uniformes y sus consultas */
    p = programa("attribute vec4 a_pos; attribute vec3 a_n; attribute mat2 a_m; uniform mat4 u_mvp; uniform vec4 u_arr[3];\n"
                 "struct S { vec3 c; float k; }; uniform S u_s;\n"
                 "varying vec4 v_c; void main() { v_c = u_arr[0] + u_arr[2] + vec4(a_n, u_s.k) + vec4(u_s.c, 0.0) + vec4(a_m[1], a_m[0]); gl_Position = u_mvp * a_pos; }",
                 "precision mediump float; varying vec4 v_c; uniform sampler2D u_tex; void main() { gl_FragColor = v_c + texture2D(u_tex, vec2(0.5)); }");
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    ver(ok, "enlace de un programa con atributos, matrices, arrays y structs");
    ver(glGetAttribLocation(p, "a_pos") == 0, "glBindAttribLocation(0, a_pos)");
    GLint ln = glGetAttribLocation(p, "a_n"), lm = glGetAttribLocation(p, "a_m");
    ver(ln > 0 && lm > 0 && ln != lm && lm + 1 != ln && ln < 16 && lm < 15, "atributos en posiciones distintas (mat2 ocupa dos)");
    GLint n = 0; glGetProgramiv(p, GL_ACTIVE_ATTRIBUTES, &n); ver(n == 3, "GL_ACTIVE_ATTRIBUTES");
    glGetProgramiv(p, GL_ACTIVE_UNIFORMS, &n); ver(n == 5, "GL_ACTIVE_UNIFORMS (mvp, arr, s.c, s.k, tex)");
    int achou = 0;
    for (int i = 0; i < n; i++) {
        char nome[64]; GLint sz; GLenum tp;
        glGetActiveUniform(p, (GLuint)i, sizeof nome, 0, &sz, &tp, nome);
        if (!strcmp(nome, "u_arr[0]") && sz == 3 && tp == GL_FLOAT_VEC4) achou |= 1;
        if (!strcmp(nome, "u_s.c") && sz == 1 && tp == GL_FLOAT_VEC3) achou |= 2;
        if (!strcmp(nome, "u_mvp") && tp == GL_FLOAT_MAT4) achou |= 4;
        if (!strcmp(nome, "u_tex") && tp == GL_SAMPLER_2D) achou |= 8;
    }
    ver(achou == 15, "glGetActiveUniform: nombres, tamaños y tipos");
    ver(glGetUniformLocation(p, "u_arr") >= 0 && glGetUniformLocation(p, "u_arr[0]") == glGetUniformLocation(p, "u_arr") &&
        glGetUniformLocation(p, "u_arr[2]") > glGetUniformLocation(p, "u_arr") && glGetUniformLocation(p, "nada") == -1, "glGetUniformLocation con arrays");
    glUseProgram(p);
    glUniform1f(glGetUniformLocation(p, "u_mvp"), 1.0f);
    ver(glGetError() == GL_INVALID_OPERATION, "glUniform1f en una mat4: GL_INVALID_OPERATION");
    glUniform1i(glGetUniformLocation(p, "u_tex"), 99);
    ver(glGetError() == GL_INVALID_VALUE, "sampler fuera de las unidades: GL_INVALID_VALUE");
    glUniform4f(-1, 1, 2, 3, 4);
    ver(glGetError() == GL_NO_ERROR, "posición -1 se ignora");
    float vals[8] = { 0.5f, 0.25f, 0.125f, 1, 2, 3, 4, 5 }, got[4];
    glUniform4fv(glGetUniformLocation(p, "u_arr[1]"), 2, vals);
    glGetUniformfv(p, glGetUniformLocation(p, "u_arr[2]"), got);
    ver(got[0] == 2 && got[3] == 5, "glUniform4fv en un array y glGetUniformfv");
    glUseProgram(0);
    glDeleteProgram(p);
}

/* ------------------------------------------------------------ funciones embutidas */
typedef struct { const char *expr; float r, g, b, a; int tol; } calc_t;
static const calc_t CALC[] = {
    { "vec4(sin(0.5), cos(0.5), tan(0.5), asin(0.5))", 0.479426f, 0.877583f, 0.546302f, 0.523599f, 1 },
    { "vec4(acos(0.5) / 2.0, atan(0.5), atan(1.0, -1.0) / 4.0, atan(-1.0, -1.0) / -4.0)", 0.523599f, 0.463648f, 0.589049f, 0.589049f, 1 },
    { "vec4(pow(0.5, 2.5), exp(-1.0), log(2.0), exp2(-0.5))", 0.176777f, 0.367879f, 0.693147f, 0.707107f, 1 },
    { "vec4(log2(1.5), sqrt(0.3), inversesqrt(4.0), abs(-0.25))", 0.584963f, 0.547723f, 0.5f, 0.25f, 1 },
    { "vec4(sign(-3.0) * -0.5, floor(1.7) / 2.0, ceil(0.2), fract(2.75))", 0.5f, 0.5f, 1.0f, 0.75f, 1 },
    { "vec4(mod(7.5, 2.0) / 2.0, min(0.3, 0.7), max(0.3, 0.7), clamp(1.5, 0.2, 0.6))", 0.75f, 0.3f, 0.7f, 0.6f, 1 },
    { "vec4(mix(0.2, 0.8, 0.25), step(0.5, 0.6), smoothstep(0.0, 1.0, 0.25), radians(45.0))", 0.35f, 1.0f, 0.15625f, 0.785398f, 1 },
    { "vec4(length(vec3(0.3, 0.4, 0.0)), distance(vec2(0.1), vec2(0.4, 0.5)), dot(vec3(0.1, 0.2, 0.3), vec3(0.4, 0.5, 0.6)), degrees(0.01))", 0.5f, 0.5f, 0.32f, 0.572958f, 1 },
    { "vec4(cross(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5, 1.0)", 0.5f, 0.5f, 1.0f, 1.0f, 1 },
    { "vec4(normalize(vec3(3.0, 0.0, 4.0)), 1.0)", 0.6f, 0.0f, 0.8f, 1.0f, 1 },
    { "vec4(reflect(vec3(0.6, -0.8, 0.0), vec3(0.0, 1.0, 0.0)), 1.0)", 0.6f, 0.8f, 0.0f, 1.0f, 1 },
    { "vec4(refract(normalize(vec3(1.0, -1.0, 0.0)), vec3(0.0, 1.0, 0.0), 0.5) * vec3(1.0, -1.0, 1.0), 1.0)", 0.353553f, 0.935414f, 0.0f, 1.0f, 1 },
    { "vec4(-faceforward(vec3(0.2, 0.4, 0.6), vec3(1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0)), 1.0)", 0.2f, 0.4f, 0.6f, 1.0f, 1 },
    { "vec4(mat3(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9) * vec3(1.0, 0.5, 0.25), 1.0)", 0.475f, 0.65f, 0.825f, 1.0f, 1 },
    { "vec4((vec3(1.0, 0.5, 0.25) * mat3(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9)) * 0.5, 1.0)", 0.1375f, 0.4f, 0.6625f, 1.0f, 1 },
    { "vec4((mat2(1.0, 2.0, 3.0, 4.0) * mat2(0.5, 0.0, 0.0, 0.25))[0] * 0.5, (mat2(1.0, 2.0, 3.0, 4.0) * mat2(0.5, 0.0, 0.0, 0.25))[1] * 0.5)", 0.25f, 0.5f, 0.375f, 0.5f, 1 },
    { "vec4(mat4(mat2(0.5, 0.25, 0.75, 1.0))[0].xy, mat4(mat2(0.5))[2].zw)", 0.5f, 0.25f, 1.0f, 0.0f, 1 },
    { "vec4(matrixCompMult(mat2(0.5, 0.5, 0.5, 0.5), mat2(0.2, 0.4, 0.6, 0.8))[1], 0.0, 1.0)", 0.3f, 0.4f, 0.0f, 1.0f, 1 },
    { "vec4(float(7 / 2) / 4.0, float(-7 / 2) / -4.0, float(int(2.9)) / 4.0, float(49 / 7) / 8.0)", 0.75f, 0.75f, 0.5f, 0.875f, 0 },
    { "vec4(vec3(lessThan(vec3(1.0, 2.0, 3.0), vec3(2.0))), float(any(lessThan(vec3(1.0, 2.0, 3.0), vec3(2.0)))))", 1.0f, 0.0f, 0.0f, 1.0f, 0 },
    { "vec4(vec3(not(lessThan(vec3(1.0, 2.0, 3.0), vec3(2.0)))), float(all(lessThan(vec3(1.0), vec3(2.0)))))", 0.0f, 1.0f, 1.0f, 1.0f, 0 },
    { "vec4(vec3(1.0, 2.0, 3.0) == vec3(1.0, 2.0, 3.0) ? 0.25 : 0.75, vec2(1.0) != vec2(1.0, 2.0) ? 1.0 : 0.0, true ^^ true ? 1.0 : 0.0, 1.0)", 0.25f, 1.0f, 0.0f, 1.0f, 0 },
    { "vec4(1.0 / 3.0, vec2(0.5, 0.75).yx, 2.0 / 8.0)", 0.333333f, 0.75f, 0.5f, 0.25f, 1 },
    { "vec4(vec4(0.1, 0.2, 0.3, 0.4).wzyx)", 0.4f, 0.3f, 0.2f, 0.1f, 1 },
    { "vec4(ivec2(3, 1).yx, bvec2(false, true)) * vec4(0.25)", 0.25f, 0.75f, 0.0f, 0.25f, 0 },
};
static void t_calculos(void)
{
    char fs[1024];
    for (unsigned i = 0; i < sizeof CALC / sizeof CALC[0]; i++) {
        const calc_t *c = &CALC[i];
        int k = 0; const char *a = "precision highp float;\nvoid main() { gl_FragColor = ";
        while (*a) fs[k++] = *a++;
        for (const char *e = c->expr; *e; ) fs[k++] = *e++;
        a = "; }\n"; while (*a) fs[k++] = *a++;
        fs[k] = 0;
        GLuint p = programa(VS_QUAD, fs);
        glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
        quad(p);
        ler_quadro();
        const unsigned char *q = px(5, 5);
        int er = (int)(c->r * 255 + 0.5f), eg = (int)(c->g * 255 + 0.5f), eb = (int)(c->b * 255 + 0.5f);
        if (perto(q[0], er, c->tol) && perto(q[1], eg, c->tol) && perto(q[2], eb, c->tol)) bien++;
        else { mal++; IMPRIMIR("MAL: %s = %d %d %d, se esperaba %d %d %d\n", c->expr, q[0], q[1], q[2], er, eg, eb); }
        glDeleteProgram(p);
    }
}

/* ------------------------------------------------------------ control de flujo */
static const char *FLUXO =
    "precision highp float;\n"
    "uniform int u_k;\n"
    "uniform vec4 u_v[3];\n"
    "struct S { vec2 a; float b; };\n"
    "uniform S u_s;\n"
    "float acha(float x) { for (int i = 0; i < 10; i++) { if (float(i) > x) return float(i); } return -1.0; }\n"
    "void dobra(inout float a, out float b) { a = a * 2.0; b = a + 1.0; }\n"
    "void main() {\n"
    "    float s = 0.0;\n"
    "    for (int i = 0; i < 20; i++) { if (i == 8) break; if (i - (i / 2) * 2 == 1) continue; s += float(i); }\n"
    "    float a = 0.25, b;\n"
    "    dobra(a, b);\n"
    "    float arr[4];\n"
    "    for (int i = 0; i < 4; i++) arr[i] = float(i) * 0.25;\n"
    "    vec4 v = vec4(0.1, 0.2, 0.3, 0.4);\n"
    "    float x = v[u_k];\n"
    "    v[u_k] = 0.9;\n"
    "    float y = 0.0; bool c = (y > 1.0) && ((y = 1.0) > 0.0);\n"
    "    int w = 0; do { w++; } while (w < 3);\n"
    "    if (gl_FragCoord.x < 16.0) gl_FragColor = vec4(s / 16.0, acha(3.5) / 8.0, a, b / 2.0);\n"
    "    else if (gl_FragCoord.x < 32.0) gl_FragColor = vec4(arr[u_k], x, v.z, c ? 1.0 : y);\n"
    "    else if (gl_FragCoord.x < 48.0) gl_FragColor = vec4(u_v[u_k].xy, u_s.a.y, u_s.b);\n"
    "    else gl_FragColor = vec4(float(w) / 4.0, 0.0, 0.0, 1.0);\n"
    "}\n";
static void t_fluxo(void)
{
    GLuint p = programa(VS_QUAD, FLUXO);
    glUseProgram(p);
    glUniform1i(glGetUniformLocation(p, "u_k"), 2);
    float uv[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0.6f, 0.7f, 0, 0 };
    glUniform4fv(glGetUniformLocation(p, "u_v"), 3, uv);
    glUniform2f(glGetUniformLocation(p, "u_s.a"), 0.1f, 0.8f);
    glUniform1f(glGetUniformLocation(p, "u_s.b"), 0.4f);
    ver(glGetError() == GL_NO_ERROR, "uniformes del programa de control de flujo");
    quad(p);
    ler_quadro();
    verpx(5, 5, 191, 128, 128, 1, "for con break y continue, return dentro de un bucle, inout/out");
    verpx(20, 5, 128, 77, 230, 1, "array local, componente dinámica (leer y escribir), && sin evaluar la derecha");
    verpx(40, 5, 153, 179, 204, 1, "array de uniformes con índice dinámico, uniforme struct");
    verpx(56, 5, 191, 0, 0, 1, "do-while");
    glDeleteProgram(p);
}

/* structs, arrays de structs con índice dinámico, columnas de matriz, inout, ++ en un swizzle, bucles anidados */
static const char *FLUXO2_BASE =
    "precision highp float;\n"
    "uniform int u_k;\n"
    "struct P { vec3 c; float k[3]; };\n"
    "P mk(float a) { P p; p.c = vec3(a); p.k[0] = a; p.k[1] = a * 2.0; p.k[2] = a * 3.0; return p; }\n"
    "void inc(inout vec3 v) { v.yz += vec2(0.25); }\n"
    "float g = 0.5;\n"
    "float f(int n) { float s = 0.0; for (int i = 0; i < 4; i++) { for (int j = 0; j < 4; j++) { if (j > i) break; if (j == n) continue; s += 1.0; } } return s; }\n"
    "void main() {\n"
    "    P a[2]; a[0] = mk(0.1); a[1] = mk(0.2);\n"
    "    float x = a[u_k - 1].k[u_k];\n"
    "    mat3 m = mat3(1.0); m[u_k] = vec3(0.3, 0.4, 0.5); vec3 col = m[2];\n"
    "    vec3 v = vec3(0.1); inc(v); v.x++;\n"
    "    vec2 w = vec2(0.5); w *= mat2(0.5, 0.0, 0.0, 1.0);\n"
    "    bvec2 b = bvec2(true, false); vec3 t = b.x ? vec3(0.9) : vec3(0.1);\n"
    "    P q = a[0]; q.c.y = g;\n"
    "    float acc = 0.0; int i = 0; while (true) { i++; if (i > 5) break; acc += 0.1; }\n"
    "    vec4 c = vec4(0.0); for (int k = 0; k < 3; k++) { if (k == 1) { c.y = 0.75; continue; } c.x += 0.25; }\n";
static void t_fluxo2(void)
{
    static const struct { const char *saida; int r, g, b; } casos[] = {
        { "gl_FragColor = vec4(x, col.z, v.y, 1.0);", 153, 128, 89 },
        { "gl_FragColor = vec4(v.x - 1.0, w.x, w.y, 1.0);", 26, 64, 128 },
        { "gl_FragColor = vec4(t.x, q.c.y, q.c.x, 1.0);", 230, 128, 26 },
        { "gl_FragColor = vec4(f(u_k) / 16.0, acc, c.x, c.y);", 128, 128, 128 },
    };
    for (unsigned n = 0; n < sizeof casos / sizeof casos[0]; n++) {
        static char fs[4096];
        int k = 0;
        for (const char *a = FLUXO2_BASE; *a; ) fs[k++] = *a++;
        for (const char *a = casos[n].saida; *a; ) fs[k++] = *a++;
        for (const char *a = "\n}\n"; *a; ) fs[k++] = *a++;
        fs[k] = 0;
        GLuint p = programa(VS_QUAD, fs);
        glUseProgram(p);
        glUniform1i(glGetUniformLocation(p, "u_k"), 2);
        quad(p); ler_quadro();
        verpx(5, 5, casos[n].r, casos[n].g, casos[n].b, 1, casos[n].saida);
        glDeleteProgram(p);
    }
}

/* ------------------------------------------------------------ escenas */
static void t_basico(void)
{
    glClearColor(0.2f, 0.4f, 0.6f, 1); glClear(GL_COLOR_BUFFER_BIT);
    ler_quadro(); verpx(10, 10, 51, 102, 153, 0, "glClear de color");
    /* interpolación lineal y con perspectiva */
    GLuint p = programa("attribute vec4 a_pos; attribute float a_v; varying float v_v; void main() { v_v = a_v; gl_Position = a_pos; }",
                        "precision highp float; varying float v_v; void main() { gl_FragColor = vec4(v_v, 0.0, 0.0, 1.0); }");
    glUseProgram(p);
    GLint lv = glGetAttribLocation(p, "a_v");
    float pos[16] = { -1, -1, 0, 1,   3, -3, 0, 3,   3, 3, 0, 3,   -1, 1, 0, 1 };
    float val[4] = { 0, 1, 1, 0 };
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, pos);
    glVertexAttribPointer((GLuint)lv, 1, GL_FLOAT, GL_FALSE, 0, val);
    glEnableVertexAttribArray(0); glEnableVertexAttribArray((GLuint)lv);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    ler_quadro();
    for (int x = 7; x < 64; x += 24) {
        float s = (x + 0.5f) / 64.0f, e = (s / 3) / ((1 - s) + s / 3);
        verpx(x, 32, (int)(e * 255 + 0.5f), 0, 0, 1, "varying con corrección de perspectiva");
    }
    float pos2[16] = { -1, -1, 0, 1,   1, -1, 0, 1,   1, 1, 0, 1,   -1, 1, 0, 1 };
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, pos2);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    ler_quadro();
    verpx(31, 32, (int)((31.5f / 64) * 255 + 0.5f), 0, 0, 1, "varying lineal");
    glDisableVertexAttribArray((GLuint)lv);
    glDeleteProgram(p);
    /* viewport */
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, 32, 32);
    quad_cor(-1, -1, 1, 1, 0, 1, 1, 0, 1);
    glViewport(0, 0, W, H);
    ler_quadro(); verpx(10, 10, 255, 255, 0, 0, "glViewport: dentro"); verpx(40, 40, 0, 0, 0, 0, "glViewport: fuera");
    /* tijera */
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST); glScissor(16, 16, 16, 16);
    glClearColor(1, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    ler_quadro(); verpx(20, 20, 255, 0, 0, 0, "tijera: dentro"); verpx(5, 5, 0, 0, 0, 0, "tijera: fuera"); verpx(32, 20, 0, 0, 0, 0, "tijera: borde excluido");
    /* máscara de color */
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
    quad_cor(-1, -1, 1, 1, 0, 1, 1, 1, 1);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    ler_quadro(); verpx(10, 10, 0, 255, 255, 0, "glColorMask");
    /* mezcla */
    glClearColor(0, 0, 1, 1); glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    quad_cor(-1, -1, 1, 1, 0, 1, 0, 0, 0.5f);
    ler_quadro(); verpx(10, 10, 128, 0, 128, 1, "mezcla SRC_ALPHA, ONE_MINUS_SRC_ALPHA");
    glBlendEquation(GL_FUNC_REVERSE_SUBTRACT); glBlendFunc(GL_ONE, GL_ONE);
    quad_cor(-1, -1, 1, 1, 0, 0.25f, 0, 0.25f, 1);
    ler_quadro(); verpx(10, 10, 128 - 64, 0, 128 - 64, 1, "mezcla FUNC_REVERSE_SUBTRACT");
    glBlendEquation(GL_FUNC_ADD); glBlendColor(0.5f, 0.5f, 0.5f, 0.5f); glBlendFunc(GL_CONSTANT_COLOR, GL_ZERO);
    quad_cor(-1, -1, 1, 1, 0, 1, 1, 1, 1);
    ler_quadro(); verpx(10, 10, 128, 128, 128, 1, "mezcla con color constante");
    glDisable(GL_BLEND);
}
static void t_profundidade(void)
{
    glClearColor(0, 0, 0, 1); glClearDepthf(1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    quad_cor(-1, -1, 1, 1, 0.5f, 1, 0, 0, 1);
    quad_cor(-1, -1, 0, 1, -0.5f, 0, 1, 0, 1);
    quad_cor(-1, -1, 1, 1, 0.8f, 0, 0, 1, 1);    /* más lejos: no pasa */
    ler_quadro(); verpx(10, 10, 0, 255, 0, 0, "Z: el de delante"); verpx(50, 10, 255, 0, 0, 0, "Z: el de detrás no tapa");
    glDepthFunc(GL_GREATER);
    quad_cor(-1, -1, 1, 1, 0.9f, 0, 0, 1, 1);
    glDepthFunc(GL_LESS);
    ler_quadro(); verpx(50, 10, 0, 0, 255, 0, "Z con GL_GREATER");
    glDepthMask(GL_FALSE);
    quad_cor(-1, -1, 1, 1, -0.9f, 1, 1, 1, 1);
    glDepthMask(GL_TRUE);
    quad_cor(-1, -1, 1, 1, -0.8f, 1, 0, 1, 1);
    ler_quadro(); verpx(50, 10, 255, 0, 255, 0, "glDepthMask(GL_FALSE) no escribe Z");
    glDisable(GL_DEPTH_TEST);
    /* gl_FragCoord.z con glDepthRangef */
    GLuint p = programa(VS_QUAD, "precision highp float; void main() { gl_FragColor = vec4(gl_FragCoord.z, gl_FragCoord.x / 64.0, gl_FragCoord.y / 64.0, 1.0); }");
    glDepthRangef(0.5f, 1.0f);
    quad_em(p, -1, -1, 1, 1, 0);
    glDepthRangef(0, 1);
    ler_quadro(); verpx(10, 20, 191, 42, 82, 1, "gl_FragCoord con glDepthRangef");
    glDeleteProgram(p);
}
static void t_estencil(void)
{
    glClearColor(0, 0, 0, 1); glClearStencil(0); glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    quad_cor(-1, -1, 0, 1, 0, 1, 1, 1, 1);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilFunc(GL_EQUAL, 1, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    quad_cor(-1, -1, 1, 1, 0, 1, 0, 0, 1);
    /* por cara: las traseras incrementan */
    glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0, 0xFF); glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_INCR);
    glStencilFuncSeparate(GL_FRONT, GL_NEVER, 0, 0xFF);
    GLuint p = cor_fixa();
    glUseProgram(p); glUniform4f(glGetUniformLocation(p, "u_cor"), 0, 1, 0, 1);
    float q[8] = { 0, -1, 0, 1, 1, 1, 1, -1 };   /* en sentido horario: trasera */
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, q);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glStencilFunc(GL_EQUAL, 2, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    quad_cor(-1, -1, 1, 1, 0, 0, 0, 1, 1);
    glDisable(GL_STENCIL_TEST);
    ler_quadro();
    verpx(10, 10, 255, 0, 0, 0, "esténcil: EQUAL 1 donde se marcó");
    verpx(50, 10, 0, 0, 255, 0, "esténcil por cara (traseras INCR dos veces = 2)");
    GLint v = 0; glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS, &v);
    ver(v == GL_INCR || v == GL_KEEP, "GL_STENCIL_BACK_PASS_DEPTH_PASS se consulta");
}
static GLuint textura_2x2(GLenum filtro)
{
    static const unsigned char t[16] = { 255, 0, 0, 255,  0, 255, 0, 255,  0, 0, 255, 255,  255, 255, 255, 255 };
    GLuint id; glGenTextures(1, &id); glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)filtro);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint)filtro);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return id;
}
static const char *FS_TEX = "precision mediump float; uniform sampler2D u_t; uniform float u_esc; varying vec2 v_tc; void main() { gl_FragColor = texture2D(u_t, v_tc * u_esc); }";
static void t_texturas(void)
{
    GLuint p = programa(VS_QUAD, FS_TEX);
    glUseProgram(p); glUniform1i(glGetUniformLocation(p, "u_t"), 0); glUniform1f(glGetUniformLocation(p, "u_esc"), 1);
    GLuint t = textura_2x2(GL_NEAREST);
    quad(p); ler_quadro();
    verpx(10, 10, 255, 0, 0, 0, "textura NEAREST: texel (0,0) abajo a la izquierda");
    verpx(50, 10, 0, 255, 0, 0, "textura NEAREST: texel (1,0)");
    verpx(10, 50, 0, 0, 255, 0, "textura NEAREST: texel (0,1)");
    verpx(50, 50, 255, 255, 255, 0, "textura NEAREST: texel (1,1)");
    glDeleteTextures(1, &t);
    t = textura_2x2(GL_LINEAR);
    quad(p); ler_quadro();
    verpx(32, 32, 128, 131, 131, 1, "textura LINEAR: pesos bilineales junto al centro");
    verpx(0, 0, 255, 0, 0, 2, "textura LINEAR con CLAMP_TO_EDGE: la esquina");
    glDeleteTextures(1, &t);
    /* mipmaps: cada nivel de un color, NEAREST_MIPMAP_NEAREST */
    static unsigned char n0[16 * 4], n1[4 * 4], n2[4];
    for (int i = 0; i < 16; i++) { n0[i * 4] = 255; n0[i * 4 + 1] = 0; n0[i * 4 + 2] = 0; n0[i * 4 + 3] = 255; }
    for (int i = 0; i < 4; i++) { n1[i * 4] = 0; n1[i * 4 + 1] = 255; n1[i * 4 + 2] = 0; n1[i * 4 + 3] = 255; }
    n2[0] = 0; n2[1] = 0; n2[2] = 255; n2[3] = 255;
    glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, n0);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, n1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    quad(p); ler_quadro();
    verpx(10, 10, 0, 0, 0, 0, "textura incompleta (falta el nivel 2): (0,0,0,1)");
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, n2);
    glUniform1f(glGetUniformLocation(p, "u_esc"), 32.0f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    quad(p); ler_quadro(); verpx(20, 20, 0, 255, 0, 0, "mipmap: nivel 1 (2 texels por píxel)");
    glUniform1f(glGetUniformLocation(p, "u_esc"), 64.0f);
    quad(p); ler_quadro(); verpx(20, 20, 0, 0, 255, 0, "mipmap: nivel 2");
    glUniform1f(glGetUniformLocation(p, "u_esc"), 4.0f);
    quad(p); ler_quadro(); verpx(20, 20, 255, 0, 0, 0, "mipmap: nivel 0 (aumento)");
    glDeleteTextures(1, &t);
    /* formatos: LUMINANCE_ALPHA y 565 */
    glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    unsigned char la[2] = { 100, 200 };
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 1, 1, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, la);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glUniform1f(glGetUniformLocation(p, "u_esc"), 1.0f);
    quad(p); ler_quadro(); verpx(5, 5, 100, 100, 100, 0, "formato LUMINANCE_ALPHA");
    unsigned short c565 = (unsigned short)((31 << 11) | (0 << 5) | 16);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, &c565);
    quad(p); ler_quadro(); verpx(5, 5, 255, 0, 132, 0, "formato RGB 565");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, la);
    ver(glGetError() == GL_INVALID_OPERATION, "glTexImage2D con internalformat distinto de format: GL_INVALID_OPERATION");
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glDeleteTextures(1, &t);
    glDeleteProgram(p);
    /* cubo: una cara de cada color */
    glGenTextures(1, &t); glBindTexture(GL_TEXTURE_CUBE_MAP, t);
    static const unsigned char cores[6][4] = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 255, 255, 0, 255 }, { 0, 255, 255, 255 }, { 255, 0, 255, 255 } };
    for (int f = 0; f < 6; f++) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)f, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, cores[f]);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    p = programa(VS_QUAD, "precision mediump float; uniform samplerCube u_c; uniform vec3 u_d; void main() { gl_FragColor = textureCube(u_c, u_d); }");
    glUseProgram(p);
    static const float dir[6][3] = { { 1, 0.2f, 0.1f }, { -1, 0.1f, 0.3f }, { 0.2f, 1, 0.1f }, { 0.1f, -1, 0.2f }, { 0.3f, 0.1f, 1 }, { 0.1f, 0.2f, -1 } };
    int ok = 1;
    for (int f = 0; f < 6; f++) {
        glUniform3f(glGetUniformLocation(p, "u_d"), dir[f][0], dir[f][1], dir[f][2]);
        quad(p); ler_quadro();
        const unsigned char *q = px(5, 5);
        if (q[0] != cores[f][0] || q[1] != cores[f][1] || q[2] != cores[f][2]) { ok = 0; IMPRIMIR("  cara %d: %d %d %d\n", f, q[0], q[1], q[2]); }
    }
    ver(ok, "textura de cubo: las seis caras");
    glDeleteTextures(1, &t);
    glDeleteProgram(p);
}
static void t_fbo(void)
{
    GLuint fb, tex, rb;
    glGenFramebuffers(1, &fb); glBindFramebuffer(GL_FRAMEBUFFER, fb);
    ver(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT, "FBO sin anexos: MISSING_ATTACHMENT");
    glClear(GL_COLOR_BUFFER_BIT);
    ver(glGetError() == GL_INVALID_FRAMEBUFFER_OPERATION, "glClear con un FBO incompleto: GL_INVALID_FRAMEBUFFER_OPERATION");
    glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 16, 16, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    ver(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT, "FBO con textura LUMINANCE: INCOMPLETE_ATTACHMENT");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenRenderbuffers(1, &rb); glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 8, 8);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb);
    ver(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS, "FBO con tamaños distintos: INCOMPLETE_DIMENSIONS");
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 16, 16);
    ver(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "FBO completo (textura RGBA + Z de 16)");
    GLint v = 0; glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_DEPTH_SIZE, &v); ver(v == 16, "GL_RENDERBUFFER_DEPTH_SIZE");
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &v); ver(v == (GLint)tex, "anexo: nombre de la textura");
    /* dibujar en la textura: mitad izquierda verde detrás de la derecha azul (con Z) */
    glViewport(0, 0, 16, 16);
    glClearColor(1, 0, 0, 0.5f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    quad_cor(-1, -1, 1, 1, 0.5f, 0, 0, 1, 1);
    quad_cor(-1, -1, 0, 1, 0.6f, 0, 1, 0, 1);
    glDisable(GL_DEPTH_TEST);
    unsigned char b4[4] = { 0, 0, 0, 0 };
    glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, b4);
    ver(b4[2] == 255 && b4[3] == 255, "glReadPixels del FBO (Z del renderbuffer)");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, W, H);
    GLuint p = programa(VS_QUAD, FS_TEX);
    glUseProgram(p); glUniform1i(glGetUniformLocation(p, "u_t"), 0); glUniform1f(glGetUniformLocation(p, "u_esc"), 1);
    glBindTexture(GL_TEXTURE_2D, tex);
    quad(p); ler_quadro();
    verpx(10, 10, 0, 0, 255, 0, "la textura del FBO en pantalla");
    /* glCopyTexImage2D y glCopyTexSubImage2D desde el framebuffer por defecto */
    glClearColor(0, 1, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    GLuint t2; glGenTextures(1, &t2); glBindTexture(GL_TEXTURE_2D, t2);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 4, 4, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 4, 4, 0);
    ver(glGetError() == GL_INVALID_OPERATION, "glCopyTexImage2D RGBA de un framebuffer sin alfa: GL_INVALID_OPERATION");
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    quad(p); ler_quadro();
    verpx(30, 30, 0, 255, 0, 0, "glCopyTexImage2D");
    glDeleteTextures(1, &t2);
    glDeleteProgram(p);
    glDeleteFramebuffers(1, &fb); glDeleteRenderbuffers(1, &rb); glDeleteTextures(1, &tex);
    ver(!glIsFramebuffer(fb) && !glIsRenderbuffer(rb) && !glIsTexture(tex), "borrar FBO, renderbuffer y textura");
}
static void t_primitivas(void)
{
    /* discard */
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    GLuint p = programa(VS_QUAD, "precision mediump float; void main() { if (gl_FragCoord.x < 32.0) discard; gl_FragColor = vec4(1.0); }");
    quad(p); ler_quadro();
    verpx(10, 10, 0, 0, 0, 0, "discard"); verpx(40, 10, 255, 255, 255, 0, "sin discard");
    glDeleteProgram(p);
    /* puntos */
    glClear(GL_COLOR_BUFFER_BIT);
    p = programa("attribute vec4 a_pos; void main() { gl_PointSize = 8.0; gl_Position = a_pos; }",
                 "precision mediump float; void main() { gl_FragColor = vec4(gl_PointCoord, 1.0, 1.0); }");
    glUseProgram(p);
    float pt[2] = { 0, 0 };
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pt); glEnableVertexAttribArray(0);
    glDrawArrays(GL_POINTS, 0, 1);
    ler_quadro();
    int n = 0; for (int i = 0; i < W * H; i++) if (lido[i * 4 + 2]) n++;
    ver(n == 64, "un punto de 8 píxeles de lado cubre 64");
    verpx(28, 35, 16, 16, 255, 1, "gl_PointCoord arriba a la izquierda");
    glDeleteProgram(p);
    /* líneas */
    glClear(GL_COLOR_BUFFER_BIT);
    float ln[4] = { -0.5f, 0.01f, 0.5f, 0.01f };
    GLuint pc = cor_fixa(); glUseProgram(pc); glUniform4f(glGetUniformLocation(pc, "u_cor"), 1, 1, 1, 1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, ln); glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINES, 0, 2);
    ler_quadro();
    n = 0; for (int i = 0; i < W * H; i++) if (lido[i * 4]) n++;
    ver(n >= 31 && n <= 33, "una línea horizontal de 32 píxeles");
    /* caras: gl_FrontFacing y glCullFace */
    glClear(GL_COLOR_BUFFER_BIT);
    p = programa(VS_QUAD, "precision mediump float; void main() { gl_FragColor = gl_FrontFacing ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0); }");
    glUseProgram(p);
    float ccw[6] = { -1, -1, 0, -1, -1, 1 }, cw[6] = { 0, -1, 0, 1, 1, -1 };
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, ccw); glDrawArrays(GL_TRIANGLES, 0, 3);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, cw); glDrawArrays(GL_TRIANGLES, 0, 3);
    ler_quadro(); verpx(5, 10, 0, 255, 0, 0, "gl_FrontFacing de frente"); verpx(40, 10, 255, 0, 0, 0, "gl_FrontFacing de espaldas");
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, cw); glDrawArrays(GL_TRIANGLES, 0, 3);
    glFrontFace(GL_CW); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, ccw); glDrawArrays(GL_TRIANGLES, 0, 3);
    glFrontFace(GL_CCW); glDisable(GL_CULL_FACE);
    ler_quadro(); verpx(40, 10, 0, 0, 0, 0, "glCullFace(GL_BACK) quita la trasera"); verpx(5, 10, 0, 0, 0, 0, "glFrontFace(GL_CW)");
    glDeleteProgram(p);
    /* índices en un búfer (UNSIGNED_BYTE y UNSIGNED_SHORT) y vértices en otro */
    glClear(GL_COLOR_BUFFER_BIT);
    float vs[8] = { -1, -1, 1, -1, 1, 1, -1, 1 };
    unsigned char ib[6] = { 0, 1, 2, 0, 2, 3 };
    unsigned short is[3] = { 0, 1, 2 };
    GLuint b[3]; glGenBuffers(3, b);
    glBindBuffer(GL_ARRAY_BUFFER, b[0]); glBufferData(GL_ARRAY_BUFFER, sizeof vs, vs, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b[1]); glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof ib, ib, GL_STATIC_DRAW);
    glUseProgram(pc); glUniform4f(glGetUniformLocation(pc, "u_cor"), 0, 0, 1, 1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(0);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
    ler_quadro(); verpx(5, 60, 0, 0, 255, 0, "glDrawElements con VBO e IBO (bytes)");
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b[2]); glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof is, is, GL_STATIC_DRAW);
    glUniform4f(glGetUniformLocation(pc, "u_cor"), 1, 0, 0, 1);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, 0);
    ler_quadro(); verpx(60, 5, 255, 0, 0, 0, "glDrawElements (shorts)"); verpx(5, 60, 0, 0, 255, 0, "glDrawElements: el otro triángulo sigue");
    GLint tam = 0; glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &tam); ver(tam == 6, "GL_BUFFER_SIZE");
    glBufferSubData(GL_ARRAY_BUFFER, 0, 4, vs);
    glBufferSubData(GL_ARRAY_BUFFER, 30, 4, vs);
    ver(glGetError() == GL_INVALID_VALUE, "glBufferSubData fuera: GL_INVALID_VALUE");
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDeleteBuffers(3, b);
    /* textura en el sombreador de vértices */
    glClear(GL_COLOR_BUFFER_BIT);
    GLuint t = textura_2x2(GL_NEAREST);
    p = programa("attribute vec2 a_pos; uniform sampler2D u_t; varying vec4 v_c; void main() { v_c = texture2DLod(u_t, vec2(0.75, 0.25), 0.0); gl_Position = vec4(a_pos, 0.0, 1.0); }",
                 "precision mediump float; varying vec4 v_c; void main() { gl_FragColor = v_c; }");
    quad(p); ler_quadro(); verpx(5, 5, 0, 255, 0, 0, "texture2DLod en vértices");
    glDeleteProgram(p); glDeleteTextures(1, &t);
    /* derivadas */
    p = programa(VS_QUAD, "#extension GL_OES_standard_derivatives : enable\nprecision highp float; varying vec2 v_tc; void main() { gl_FragColor = vec4(dFdx(gl_FragCoord.x), dFdy(gl_FragCoord.y), fwidth(gl_FragCoord.x) * 0.5, dFdx(v_tc.x) * 32.0); }");
    quad(p); ler_quadro(); verpx(9, 9, 255, 255, 128, 1, "dFdx, dFdy y fwidth");
    glDeleteProgram(p);
}
static void t_erros(void)
{
    while (glGetError() != GL_NO_ERROR) {}
    glEnable(0x1234); ver(glGetError() == GL_INVALID_ENUM, "glEnable(desconocido): GL_INVALID_ENUM");
    glDrawArrays(GL_TRIANGLES, 0, -1); ver(glGetError() == GL_INVALID_VALUE, "glDrawArrays con cuenta negativa: GL_INVALID_VALUE");
    glDrawArrays(0x99, 0, 3); ver(glGetError() == GL_INVALID_ENUM, "glDrawArrays con modo desconocido: GL_INVALID_ENUM");
    glVertexAttribPointer(16, 4, GL_FLOAT, GL_FALSE, 0, 0); ver(glGetError() == GL_INVALID_VALUE, "atributo 16: GL_INVALID_VALUE");
    glBindTexture(GL_TEXTURE_2D, 77); glBindTexture(GL_TEXTURE_CUBE_MAP, 77); ver(glGetError() == GL_INVALID_OPERATION, "textura 2D ligada como cubo: GL_INVALID_OPERATION");
    glBindTexture(GL_TEXTURE_2D, 0);
    GLint v = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &v); ver(v >= 8, "GL_MAX_VERTEX_ATTRIBS >= 8");
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &v); ver(v >= 8, "GL_MAX_VARYING_VECTORS >= 8");
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &v); ver(v >= 8, "GL_MAX_TEXTURE_IMAGE_UNITS >= 8");
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &v); ver(v >= 16, "GL_MAX_FRAGMENT_UNIFORM_VECTORS >= 16");
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &v); ver(v >= 128, "GL_MAX_VERTEX_UNIFORM_VECTORS >= 128");
    glGetIntegerv(GL_DEPTH_BITS, &v); ver(v >= 16, "GL_DEPTH_BITS");
    glGetIntegerv(GL_STENCIL_BITS, &v); ver(v == 8, "GL_STENCIL_BITS");
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &v); ver(v == GL_RGBA, "GL_IMPLEMENTATION_COLOR_READ_FORMAT");
    GLboolean sc = GL_FALSE; glGetBooleanv(GL_SHADER_COMPILER, &sc); ver(sc == GL_TRUE, "GL_SHADER_COMPILER");
    GLint faixa[2], prec; glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, faixa, &prec); ver(faixa[0] >= 62 && prec >= 16, "glGetShaderPrecisionFormat de highp");
    const char *s = (const char *)glGetString(GL_VERSION); ver(s && strstr(s, "OpenGL ES 2.0"), "GL_VERSION: «OpenGL ES 2.0 ...»");
    s = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION); ver(s && strstr(s, "OpenGL ES GLSL ES 1.00"), "GL_SHADING_LANGUAGE_VERSION");
    s = (const char *)glGetString(GL_EXTENSIONS); ver(s && strstr(s, "GL_OES_standard_derivatives"), "GL_EXTENSIONS");
    ver(glGetError() == GL_NO_ERROR, "sin errores pendientes");
}

/* ------------------------------------------------------------ EGL y ES 1.1 a la vez */
static EGLDisplay dpy;
static EGLSurface sup;
static EGLContext ctx;
typedef void (*fn_t)(void);
static int egl(void)
{
    EGLint n; EGLConfig cfg;
    const EGLint at[] = { EGL_DEPTH_SIZE, 16, EGL_STENCIL_SIZE, 8, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
    const EGLint pb[] = { EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE };
    const EGLint cx[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(dpy, 0, 0) || !eglChooseConfig(dpy, at, &cfg, 1, &n) || n < 1) return 0;
    sup = eglCreatePbufferSurface(dpy, cfg, pb);
    ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, cx);
    if (sup == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, sup, sup, ctx)) return 0;
    EGLint ver2 = 0; eglQueryContext(dpy, ctx, EGL_CONTEXT_CLIENT_VERSION, &ver2);
    return ver2 == 2;
}
/* un contexto de ES 1.1 con las funciones de siempre, alternando con el de ES 2.0 */
static void t_es11(void)
{
    EGLint n; EGLConfig cfg;
    const EGLint at[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE };
    const EGLint pb[] = { EGL_WIDTH, 8, EGL_HEIGHT, 8, EGL_NONE };
    eglChooseConfig(dpy, at, &cfg, 1, &n);
    EGLSurface s1 = eglCreatePbufferSurface(dpy, cfg, pb);
    EGLContext c1 = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, 0);
    glClearColor(0.25f, 0.5f, 0.75f, 1);   /* en el de ES 2.0 */
    ver(eglMakeCurrent(dpy, s1, s1, c1), "eglMakeCurrent de un contexto de ES 1.1");
    const char *v = (const char *)glGetString(GL_VERSION);
    ver(v && strstr(v, "OpenGL ES-CM 1.1"), "con ES 1.1 actual, glGetString dice 1.1");
    glClearColor(1, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    unsigned char b[4] = { 0, 0, 0, 0 };
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, b);
    ver(b[0] == 255 && b[1] == 0, "ES 1.1: glClear y glReadPixels");
    eglMakeCurrent(dpy, sup, sup, ctx);
    glClear(GL_COLOR_BUFFER_BIT);
    ler_quadro(); verpx(3, 3, 64, 128, 191, 1, "de vuelta en ES 2.0: su estado sigue (glClearColor)");
    v = (const char *)glGetString(GL_VERSION);
    ver(v && strstr(v, "OpenGL ES 2.0"), "de vuelta en ES 2.0: glGetString dice 2.0");
    eglDestroyContext(dpy, c1); eglDestroySurface(dpy, s1);
}

static int rodar(void)
{
    if (!egl()) { IMPRIMIR("conformidad: sin contexto de OpenGL ES 2.0\n"); return 1; }
    t_compilacao();
    t_ligacao();
    t_calculos();
    t_fluxo();
    t_fluxo2();
    t_basico();
    t_profundidade();
    t_estencil();
    t_texturas();
    t_fbo();
    t_primitivas();
    t_erros();
    t_es11();
    IMPRIMIR("conformidad: %d bien, %d mal\n", bien, mal);
    IMPRIMIR("suma %08x\n", soma);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(dpy, ctx); eglDestroySurface(dpy, sup); eglTerminate(dpy);
    return mal ? 1 : 0;
}

/* D480: con --cpu la GPU (gpu3d v4, si la hay) no se usa; la salida dice cuántos dibujos fueron por la GPU. */
void sgl2_gpu_modo(int modo);
int  sgl2_gpu_estado(void);
void sgl2_gpu_contadores(unsigned *des, unsigned *lim, unsigned *cpu);
static int rodar_gpu(int solo_cpu)
{
    if (solo_cpu) sgl2_gpu_modo(0);
    int r = rodar();
    unsigned d = 0, l = 0, c = 0;
    sgl2_gpu_contadores(&d, &l, &c);
    IMPRIMIR("gpu: estado %d, %u dibujos y %u limpiezas por la GPU, %u dibujos que la GPU devolvio a la CPU\n", sgl2_gpu_estado(), d, l, c);
    return r;
}

#if defined(SGL_ANFITRIAO)
int main(int argc, char **argv) { return rodar_gpu(argc > 1 && !strcmp(argv[1], "--cpu")); }
#else
int principal(int argc, char **argv) { return rodar_gpu(argc > 1 && !strcmp(argv[1], "--cpu")); }
#endif
