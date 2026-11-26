// SPDX-License-Identifier: GPL-3.0-only
/* es20demo - demostración de OpenGL ES 2.0 con EGL (SaguiGL 2.0, D475).
 *
 * La parte gráfica usa SOLO las cabeceras estándar (GLES2/gl2.h y EGL/egl.h): es lo que se escribiría para
 * cualquier OpenGL ES 2.0. Lo único de SaguiOS es abrir la ventana (la «ventana nativa» de EGL, al final).
 * Cuatro escenas:
 *   1 triangulo  el triángulo de colores (atributos de posición y color, varying interpolado)
 *   2 cubo       cubo con textura (mipmaps generados) e iluminación por fragmento (Phong) con Z
 *   3 fbo        el cubo dibujado en una textura (FBO con Z en un renderbuffer) y esa textura en pantalla
 *                con un efecto (ondas y viñeta)
 *   4 plasma     un sombreador de fragmentos procedural (seno, coseno, bucle): mide la ALU
 *
 *   es20demo            ventana de 320x240; 1-4 o espacio cambian de escena, Esc cierra
 *   es20demo -c N       la escena N (cuadro fijo) en un pbuffer de EGL: escribe su suma
 *                       (el mismo programa en el anfitrión da la misma: tests/h475_saguigl2.py)
 *   es20demo -n N       la ventana durante N cuadros por escena y los fps de cada una
 *   es20demo -m N       sin ventana: N cuadros de cada escena en el pbuffer y los ms por cuadro
 *   es20demo -d N       la escena N por la CPU y por la GPU (si hay gpu3d v4): píxeles que difieren (D480)
 *   --cpu (al final)    sin GPU, aunque haya gpu3d v4
 * En el anfitrión (-DSGL_ANFITRIAO): «-c N», «-m N» y «-p N fichero.ppm» (la imagen, para docs/capturas). */
#include <GLES2/gl2.h>
#include <EGL/egl.h>

#define L 320
#define A 240

/* D480: SaguiGL 2.0 usa la GPU (gpu3d v4) cuando la hay; con --cpu, nunca. */
void sgl2_gpu_modo(int modo);
int  sgl2_gpu_estado(void);
void sgl2_gpu_contadores(unsigned *des, unsigned *lim, unsigned *cpu);

/* ------------------------------------------------------------ utilidades (sin libm) */
static float seno(float x)
{
    const float PI = 3.1415927f, DPI = 6.2831855f;
    while (x > PI) x -= DPI;
    while (x < -PI) x += DPI;
    float x2 = x * x;
    return x * (1.0f + x2 * (-1.0f / 6 + x2 * (1.0f / 120 + x2 * (-1.0f / 5040 + x2 * (1.0f / 362880)))));
}
static float coseno(float x) { return seno(x + 1.5707964f); }

/* matrices 4x4 por columnas */
static void m_ident(float *m) { for (int i = 0; i < 16; i++) m[i] = (i % 5) == 0; }
static void m_mul(float *r, const float *a, const float *b)
{
    float t[16];
    for (int c = 0; c < 4; c++) for (int l = 0; l < 4; l++) { float s = 0; for (int k = 0; k < 4; k++) s += a[k * 4 + l] * b[c * 4 + k]; t[c * 4 + l] = s; }
    for (int i = 0; i < 16; i++) r[i] = t[i];
}
static void m_persp(float *m, float f, float asp, float n, float lo)
{
    for (int i = 0; i < 16; i++) m[i] = 0;
    m[0] = f / asp; m[5] = f; m[10] = (lo + n) / (n - lo); m[11] = -1; m[14] = 2 * lo * n / (n - lo);
}
static void m_rot(float *m, float ang, float x, float y, float z)
{
    float c = coseno(ang), s = seno(ang), k = 1 - c;
    m_ident(m);
    m[0] = x * x * k + c;     m[4] = x * y * k - z * s; m[8] = x * z * k + y * s;
    m[1] = y * x * k + z * s; m[5] = y * y * k + c;     m[9] = y * z * k - x * s;
    m[2] = x * z * k - y * s; m[6] = y * z * k + x * s; m[10] = z * z * k + c;
}
static void m_trans(float *m, float x, float y, float z) { m_ident(m); m[12] = x; m[13] = y; m[14] = z; }

static GLuint compilar(GLenum tipo, const char *fonte)
{
    GLuint s = glCreateShader(tipo);
    glShaderSource(s, 1, &fonte, 0);
    glCompileShader(s);
    return s;
}
static GLuint programa(const char *vs, const char *fs)
{
    GLuint p = glCreateProgram();
    glAttachShader(p, compilar(GL_VERTEX_SHADER, vs));
    glAttachShader(p, compilar(GL_FRAGMENT_SHADER, fs));
    glBindAttribLocation(p, 0, "a_pos");
    glLinkProgram(p);
    return p;
}

/* ------------------------------------------------------------ escena 1: el triángulo */
static const char *VS_TRI =
    "attribute vec2 a_pos;\n"
    "attribute vec3 a_cor;\n"
    "uniform float u_ang;\n"
    "varying vec3 v_cor;\n"
    "void main() {\n"
    "    float c = cos(u_ang), s = sin(u_ang);\n"
    "    gl_Position = vec4(c * a_pos.x - s * a_pos.y, s * a_pos.x + c * a_pos.y, 0.0, 1.0);\n"
    "    v_cor = a_cor;\n"
    "}\n";
static const char *FS_TRI =
    "precision mediump float;\n"
    "varying vec3 v_cor;\n"
    "void main() { gl_FragColor = vec4(v_cor, 1.0); }\n";
static GLuint p_tri;
static void cena_triangulo(float t)
{
    static const float v[] = { 0.0f, 0.8f, 1, 0, 0,   -0.8f, -0.6f, 0, 1, 0,   0.8f, -0.6f, 0, 0, 1 };
    glViewport(0, 0, L, A);
    glClearColor(0.1f, 0.1f, 0.15f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(p_tri);
    glUniform1f(glGetUniformLocation(p_tri, "u_ang"), t * 0.5f);
    GLint ac = glGetAttribLocation(p_tri, "a_cor");
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * 4, v);
    glVertexAttribPointer((GLuint)ac, 3, GL_FLOAT, GL_FALSE, 5 * 4, v + 2);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray((GLuint)ac);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableVertexAttribArray((GLuint)ac);
}

/* ------------------------------------------------------------ escena 2: el cubo */
static const char *VS_CUBO =
    "attribute vec3 a_pos;\n"
    "attribute vec3 a_normal;\n"
    "attribute vec2 a_tc;\n"
    "uniform mat4 u_mvp, u_mv;\n"
    "varying vec3 v_n, v_p;\n"
    "varying vec2 v_tc;\n"
    "void main() {\n"
    "    gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
    "    v_p = (u_mv * vec4(a_pos, 1.0)).xyz;\n"
    "    v_n = (u_mv * vec4(a_normal, 0.0)).xyz;\n"
    "    v_tc = a_tc;\n"
    "}\n";
static const char *FS_CUBO =
    "precision mediump float;\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec3 u_luz;\n"
    "varying vec3 v_n, v_p;\n"
    "varying vec2 v_tc;\n"
    "void main() {\n"
    "    vec3 n = normalize(v_n);\n"
    "    vec3 l = normalize(u_luz - v_p);\n"
    "    vec3 e = normalize(-v_p);\n"
    "    vec3 h = normalize(l + e);\n"
    "    float d = max(dot(n, l), 0.0);\n"
    "    float s = pow(max(dot(n, h), 0.0), 32.0);\n"
    "    vec4 c = texture2D(u_tex, v_tc);\n"
    "    gl_FragColor = vec4(c.rgb * (0.15 + 0.85 * d) + vec3(0.6) * s, 1.0);\n"
    "}\n";
static GLuint p_cubo, tex_cubo, vbo_cubo, ibo_cubo;
static void criar_cubo(void)
{
    /* 6 caras de 4 vértices: posición, normal, coordenadas de textura */
    static float v[24 * 8];
    static unsigned short ind[36];
    /* normal y dos ejes de cada cara con u x v = n (así los triángulos van en sentido antihorario vistos de fuera) */
    static const float n[6][9] = {
        { 1, 0, 0, 0, 1, 0, 0, 0, 1 }, { -1, 0, 0, 0, 0, 1, 0, 1, 0 }, { 0, 1, 0, 0, 0, 1, 1, 0, 0 },
        { 0, -1, 0, 1, 0, 0, 0, 0, 1 }, { 0, 0, 1, 1, 0, 0, 0, 1, 0 }, { 0, 0, -1, 0, 1, 0, 1, 0, 0 } };
    for (int f = 0; f < 6; f++) {
        float nx = n[f][0], ny = n[f][1], nz = n[f][2];
        float ux = n[f][3], uy = n[f][4], uz = n[f][5], vx = n[f][6], vy = n[f][7], vz = n[f][8];
        for (int k = 0; k < 4; k++) {
            float a = (k == 1 || k == 2) ? 1.0f : -1.0f, b = (k >= 2) ? 1.0f : -1.0f;
            float *p = v + (f * 4 + k) * 8;
            p[0] = nx + a * ux + b * vx; p[1] = ny + a * uy + b * vy; p[2] = nz + a * uz + b * vz;
            p[3] = nx; p[4] = ny; p[5] = nz;
            p[6] = (a + 1) * 0.5f; p[7] = (b + 1) * 0.5f;
        }
        unsigned short b0 = (unsigned short)(f * 4);
        unsigned short q[6] = { b0, (unsigned short)(b0 + 1), (unsigned short)(b0 + 2), b0, (unsigned short)(b0 + 2), (unsigned short)(b0 + 3) };
        for (int k = 0; k < 6; k++) ind[f * 6 + k] = q[k];
    }
    glGenBuffers(1, &vbo_cubo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_cubo);
    glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_STATIC_DRAW);
    glGenBuffers(1, &ibo_cubo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_cubo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof ind, ind, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    /* textura: baldosas con junta, 64x64, con mipmaps */
    static unsigned char px[64 * 64 * 3];
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            int junta = (y % 16) < 2 || ((x + (y / 16 % 2) * 8) % 16) < 2;
            int r = ((x * 13 + y * 7) % 17) * 3;
            unsigned char *p = px + (y * 64 + x) * 3;
            if (junta) { p[0] = 210; p[1] = 205; p[2] = 190; }
            else { p[0] = (unsigned char)(40 + r); p[1] = (unsigned char)(110 + r); p[2] = (unsigned char)(170 + r / 2); }
        }
    glGenTextures(1, &tex_cubo);
    glBindTexture(GL_TEXTURE_2D, tex_cubo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
static void desenhar_cubo(float t, int larg, int alt)
{
    float p[16], r1[16], r2[16], tr[16], mv[16], mvp[16];
    m_persp(p, 1.8f, (float)larg / (float)alt, 0.5f, 20.0f);
    m_rot(r1, t * 0.7f, 0, 1, 0);
    m_rot(r2, t * 0.4f + 0.5f, 0.6f, 0, 0.8f);
    m_trans(tr, 0, 0, -4.2f);
    m_mul(mv, r1, r2);
    m_mul(mv, tr, mv);
    m_mul(mvp, p, mv);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glUseProgram(p_cubo);
    glUniformMatrix4fv(glGetUniformLocation(p_cubo, "u_mvp"), 1, GL_FALSE, mvp);
    glUniformMatrix4fv(glGetUniformLocation(p_cubo, "u_mv"), 1, GL_FALSE, mv);
    glUniform3f(glGetUniformLocation(p_cubo, "u_luz"), 2.0f, 3.0f, 1.0f);
    glUniform1i(glGetUniformLocation(p_cubo, "u_tex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_cubo);
    GLint an = glGetAttribLocation(p_cubo, "a_normal"), at = glGetAttribLocation(p_cubo, "a_tc");
    glBindBuffer(GL_ARRAY_BUFFER, vbo_cubo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (const void *)0);
    glVertexAttribPointer((GLuint)an, 3, GL_FLOAT, GL_FALSE, 32, (const void *)12);
    glVertexAttribPointer((GLuint)at, 2, GL_FLOAT, GL_FALSE, 32, (const void *)24);
    glEnableVertexAttribArray(0); glEnableVertexAttribArray((GLuint)an); glEnableVertexAttribArray((GLuint)at);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_cubo);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableVertexAttribArray((GLuint)an); glDisableVertexAttribArray((GLuint)at);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}
static void cena_cubo(float t)
{
    glViewport(0, 0, L, A);
    glClearColor(0.05f, 0.05f, 0.08f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    desenhar_cubo(t, L, A);
}

/* ------------------------------------------------------------ escena 3: FBO */
static const char *VS_TELA =
    "attribute vec2 a_pos;\n"
    "varying vec2 v_tc;\n"
    "void main() { v_tc = a_pos * 0.5 + 0.5; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";
static const char *FS_TELA =
    "precision mediump float;\n"
    "uniform sampler2D u_tex;\n"
    "uniform float u_t;\n"
    "varying vec2 v_tc;\n"
    "void main() {\n"
    "    vec2 tc = v_tc + vec2(sin(v_tc.y * 18.0 + u_t * 3.0), cos(v_tc.x * 14.0 + u_t * 2.0)) * 0.012;\n"
    "    vec3 c = texture2D(u_tex, tc).rgb;\n"
    "    float vig = 1.0 - 0.7 * dot(v_tc - 0.5, v_tc - 0.5) * 2.0;\n"
    "    gl_FragColor = vec4(c * vig + vec3(0.02, 0.0, 0.05), 1.0);\n"
    "}\n";
static GLuint p_tela, fbo, tex_fbo, rb_z;
#define FL 128
#define FA 128
static void criar_fbo(void)
{
    glGenTextures(1, &tex_fbo);
    glBindTexture(GL_TEXTURE_2D, tex_fbo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FL, FA, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &rb_z);
    glBindRenderbuffer(GL_RENDERBUFFER, rb_z);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, FL, FA);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_fbo, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb_z);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
static int fbo_completo(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    int ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return ok;
}
static const float QUAD[] = { -1, -1, 1, -1, 1, 1, -1, 1 };
static void cena_fbo(float t)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, FL, FA);
    glClearColor(0.9f, 0.6f, 0.2f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    desenhar_cubo(t, FL, FA);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, L, A);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(p_tela);
    glUniform1i(glGetUniformLocation(p_tela, "u_tex"), 1);
    glUniform1f(glGetUniformLocation(p_tela, "u_t"), t);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex_fbo);
    glActiveTexture(GL_TEXTURE0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, QUAD);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

/* ------------------------------------------------------------ escena 4: plasma */
static const char *FS_PLASMA =
    "precision mediump float;\n"
    "uniform float u_t;\n"
    "varying vec2 v_tc;\n"
    "vec3 paleta(float x) { return 0.5 + 0.5 * cos(6.2831 * (vec3(0.0, 0.33, 0.67) + x)); }\n"
    "void main() {\n"
    "    vec2 p = v_tc * 6.0 - 3.0;\n"
    "    float v = 0.0;\n"
    "    for (int i = 1; i <= 4; i++) {\n"
    "        float f = float(i);\n"
    "        v += sin(p.x * f + u_t) + sin(p.y * f * 0.7 - u_t * 1.3);\n"
    "        p = vec2(p.y, -p.x) * 0.9 + 0.3;\n"
    "    }\n"
    "    gl_FragColor = vec4(paleta(v * 0.125 + u_t * 0.1), 1.0);\n"
    "}\n";
static GLuint p_plasma;
static void cena_plasma(float t)
{
    glViewport(0, 0, L, A);
    glUseProgram(p_plasma);
    glUniform1f(glGetUniformLocation(p_plasma, "u_t"), t);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, QUAD);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

typedef void (*cena_f)(float);
#define N_CENAS 4
static const cena_f cenas[N_CENAS] = { cena_triangulo, cena_cubo, cena_fbo, cena_plasma };
static const char *nomes[N_CENAS] = { "triangulo", "cubo", "fbo", "plasma" };

static int criar_tudo(void)
{
    p_tri = programa(VS_TRI, FS_TRI);
    p_cubo = programa(VS_CUBO, FS_CUBO);
    p_tela = programa(VS_TELA, FS_TELA);
    p_plasma = programa(VS_TELA, FS_PLASMA);
    GLint ok = 0;
    GLuint ps[4] = { p_tri, p_cubo, p_tela, p_plasma };
    for (int i = 0; i < 4; i++) { glGetProgramiv(ps[i], GL_LINK_STATUS, &ok); if (!ok) return 0; }
    criar_cubo();
    criar_fbo();
    return fbo_completo();
}

/* ------------------------------------------------------------ EGL */
static EGLDisplay dpy;
static EGLSurface sup;
static EGLContext ctx;

static int egl_iniciar(EGLNativeWindowType w)
{
    EGLint maj, min, n;
    EGLConfig cfg;
    const EGLint at[] = { EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16,
                          EGL_SURFACE_TYPE, w ? EGL_WINDOW_BIT : EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
    const EGLint pb[] = { EGL_WIDTH, L, EGL_HEIGHT, A, EGL_NONE };
    const EGLint cx[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(dpy, &maj, &min)) return 0;
    if (!eglChooseConfig(dpy, at, &cfg, 1, &n) || n < 1) return 0;
    sup = w ? eglCreateWindowSurface(dpy, cfg, w, 0) : eglCreatePbufferSurface(dpy, cfg, pb);
    if (sup == EGL_NO_SURFACE) return 0;
    ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, cx);
    if (ctx == EGL_NO_CONTEXT) return 0;
    if (!eglMakeCurrent(dpy, sup, sup, ctx)) return 0;
    eglSwapInterval(dpy, 1);
    return criar_tudo();
}

static void egl_fim(void)
{
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(dpy, ctx);
    eglDestroySurface(dpy, sup);
    eglTerminate(dpy);
}

/* Suma FNV-1a del cuadro leído con glReadPixels (la misma en el anfitrión y en el SaguiOS). */
static unsigned soma_quadro(void)
{
    static unsigned char p[L * A * 4];
    glReadPixels(0, 0, L, A, GL_RGBA, GL_UNSIGNED_BYTE, p);
    unsigned h = 2166136261u;
    for (int i = 0; i < L * A; i++) for (int k = 0; k < 3; k++) { h ^= p[i * 4 + k]; h *= 16777619u; }
    return h;
}

#define T_FIXO 1.25f

/* D480: la escena n dibujada por la CPU y por la GPU (si la hay) en el mismo proceso: cuántos píxeles difieren más de 8 en
 * algún canal y la mayor diferencia. Devuelve 1 si la GPU dibujó algo. */
static int g_forma, g_media100;   /* píxeles con cobertura distinta (fondo en uno y no en el otro) y la diferencia media por 100 */
static int comparar(int n, int *dist, int *maxd)
{
    static unsigned char a[L * A * 4], b[L * A * 4];
    unsigned d0 = 0, l0 = 0, c0 = 0, d1 = 0, l1 = 0, c1 = 0;
    sgl2_gpu_modo(0);
    cenas[n](T_FIXO);
    glReadPixels(0, 0, L, A, GL_RGBA, GL_UNSIGNED_BYTE, a);
    sgl2_gpu_modo(1);
    sgl2_gpu_contadores(&d0, &l0, &c0);
    cenas[n](T_FIXO);
    glReadPixels(0, 0, L, A, GL_RGBA, GL_UNSIGNED_BYTE, b);
    sgl2_gpu_contadores(&d1, &l1, &c1);
    *dist = 0; *maxd = 0; g_forma = 0;
    long soma = 0;
    for (int i = 0; i < L * A; i++) {
        int m = 0, fa = 0, fb = 0;
        for (int k = 0; k < 3; k++) {
            int x = a[i * 4 + k] - b[i * 4 + k]; if (x < 0) x = -x; if (x > m) m = x; soma += x;
            int ya = a[i * 4 + k] - a[k]; if (ya < 0) ya = -ya; if (ya > 8) fa = 1;
            int yb = b[i * 4 + k] - b[k]; if (yb < 0) yb = -yb; if (yb > 8) fb = 1;
        }
        if (m > 8) (*dist)++;
        if (m > *maxd) *maxd = m;
        if (fa != fb) g_forma++;
    }
    g_media100 = (int)(soma * 100 / (L * A * 3));
    return d1 > d0;
}

/* ------------------------------------------------------------ plataforma */
#if defined(SGL_ANFITRIAO)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double agora_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000.0 + t.tv_nsec / 1e6; }

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[argc - 1], "--cpu")) { sgl2_gpu_modo(0); argc--; }
    if (!egl_iniciar(0)) { printf("es20demo: sin EGL o sin programas (%x)\n", glGetError()); return 1; }
    if (argc > 2 && !strcmp(argv[1], "-c")) {
        int n = atoi(argv[2]);
        if (n < 1 || n > N_CENAS) return 1;
        cenas[n - 1](T_FIXO);
        printf("%s %08x\n", nomes[n - 1], soma_quadro());
    } else if (argc > 3 && !strcmp(argv[1], "-p")) {
        int n = atoi(argv[2]);
        if (n < 1 || n > N_CENAS) return 1;
        cenas[n - 1](T_FIXO);
        static unsigned char p[L * A * 4];
        glReadPixels(0, 0, L, A, GL_RGBA, GL_UNSIGNED_BYTE, p);
        FILE *f = fopen(argv[3], "wb");
        if (!f) return 1;
        fprintf(f, "P6\n%d %d\n255\n", L, A);
        for (int y = A - 1; y >= 0; y--) for (int x = 0; x < L; x++) fwrite(p + (y * L + x) * 4, 1, 3, f);
        fclose(f);
    } else if (argc > 2 && !strcmp(argv[1], "-d")) {
        int n = atoi(argv[2]), dist, maxd;
        if (n < 1 || n > N_CENAS) return 1;
        int g = comparar(n - 1, &dist, &maxd);
        printf("es20demo: %s gpu-vs-cpu: %d pixeles distintos, maximo %d, silueta %d, media %d.%02d, gpu %d\n", nomes[n - 1], dist, maxd, g_forma, g_media100 / 100, g_media100 % 100, g);
    } else if (argc > 2 && !strcmp(argv[1], "-m")) {
        int q = atoi(argv[2]);
        for (int n = 0; n < N_CENAS; n++) {
            double t0 = agora_ms();
            for (int i = 0; i < q; i++) cenas[n](T_FIXO + i * 0.03f);
            glFinish();
            printf("es20demo: %s %.2f ms por cuadro\n", nomes[n], (agora_ms() - t0) / q);
        }
    } else {
        for (int n = 0; n < N_CENAS; n++) { cenas[n](T_FIXO); printf("%s %08x\n", nomes[n], soma_quadro()); }
    }
    egl_fim();
    return 0;
}
#else
#include "sagui.h"
#include "gui.h"

static int tem_sse2(void)
{
    unsigned a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    return (d >> 26) & 1;
}
static int numero(const char *s) { int n = 0; for (; *s >= '0' && *s <= '9'; s++) n = n * 10 + (*s - '0'); return n; }

static void gpu_resumo(void)
{
    unsigned d = 0, l = 0, c = 0;
    sgl2_gpu_contadores(&d, &l, &c);
    imprimir("es20demo: gpu estado %d, %u dibujos y %u limpiezas por la GPU, %u devueltos a la CPU\n", sgl2_gpu_estado(), d, l, c);
}

int principal(int argc, char **argv)
{
    if (argc > 1 && argv[argc - 1][0] == '-' && argv[argc - 1][1] == '-' && argv[argc - 1][2] == 'c') { sgl2_gpu_modo(0); argc--; }   /* --cpu */
    if (!tem_sse2()) { imprimir("es20demo: SaguiGL pide SSE2\n"); return 1; }
    if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'c') {
        int n = numero(argv[2]);
        if (n < 1 || n > N_CENAS) { imprimir("es20demo: escena 1 a %d\n", N_CENAS); return 1; }
        if (!egl_iniciar(0)) { imprimir("es20demo: sin EGL (%x)\n", eglGetError()); return 1; }
        cenas[n - 1](T_FIXO);
        imprimir("%s %08x\n", nomes[n - 1], soma_quadro());
        gpu_resumo();
        egl_fim();
        return 0;
    }
    if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'd') {
        int n = numero(argv[2]), dist, maxd;
        if (n < 1 || n > N_CENAS) { imprimir("es20demo: escena 1 a %d\n", N_CENAS); return 1; }
        if (!egl_iniciar(0)) { imprimir("es20demo: sin EGL (%x)\n", eglGetError()); return 1; }
        int g = comparar(n - 1, &dist, &maxd);
        imprimir("es20demo: %s gpu-vs-cpu: %d pixeles distintos, maximo %d, silueta %d, media %d.%02d, gpu %d\n", nomes[n - 1], dist, maxd, g_forma, g_media100 / 100, g_media100 % 100, g);
        gpu_resumo();
        egl_fim();
        return 0;
    }
    if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'm') {
        int q = numero(argv[2]);
        if (q < 1) q = 1;
        if (!egl_iniciar(0)) { imprimir("es20demo: sin EGL (%x)\n", eglGetError()); return 1; }
        for (int n = 0; n < N_CENAS; n++) {
            unsigned t0 = tempo_ms();
            for (int i = 0; i < q; i++) cenas[n](T_FIXO + (float)i * 0.03f);
            glFinish();
            unsigned ms = tempo_ms() - t0;
            imprimir("es20demo: %s %u ms por cuadro (%d cuadros de %dx%d)\n", nomes[n], ms / (unsigned)q, q, L, A);
        }
        gpu_resumo();
        egl_fim();
        return 0;
    }
    int por_cena = 0;
    if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'n') por_cena = numero(argv[2]);
    jan_t *j = jan_abrir("OpenGL ES 2.0 + EGL", L, A);
    if (!j) { imprimir("es20demo: sin ventana\n"); return 1; }
    if (!egl_iniciar((EGLNativeWindowType)j)) { imprimir("es20demo: sin EGL (%x)\n", eglGetError()); jan_fechar(j); return 1; }
    int cena = 0, quadros = 0, sair = 0;
    unsigned t0 = tempo_ms(), tc = t0;
    while (!sair) {
        evento_t e;
        while (proximo_evento(j->id, &e, false)) {
            if (e.tipo == EV_FECHAR || (e.tipo == EV_TECLA && e.tecla == 27)) sair = 1;
            else if (e.tipo == EV_TECLA && e.tecla >= '1' && e.tecla <= '0' + N_CENAS) cena = e.tecla - '1';
            else if (e.tipo == EV_TECLA && e.tecla == ' ') cena = (cena + 1) % N_CENAS;
        }
        float t = (float)(tempo_ms() - t0) * 0.001f;
        cenas[cena](t);
        eglSwapBuffers(dpy, sup);
        quadros++;
        if (por_cena && quadros >= por_cena) {
            unsigned ms = tempo_ms() - tc;
            if (!ms) ms = 1;
            imprimir("es20demo: %s %d cuadros en %u ms, %u fps%s\n", nomes[cena], quadros, ms, (unsigned)quadros * 1000u / ms, sgl2_gpu_estado() > 0 ? " (GPU)" : "");
            quadros = 0; tc = tempo_ms();
            if (++cena == N_CENAS) sair = 1;
        }
    }
    egl_fim();
    jan_fechar(j);
    return 0;
}
#endif
