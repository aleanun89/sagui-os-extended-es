// SPDX-License-Identifier: GPL-2.0-or-later
/* sgl_fixo.c - SaguiGL: las variantes de coma fija (GLfixed, 16.16) de OpenGL
 * ES 1.1 (OES_fixed_point, D425). Cada una convierte y llama a la de coma
 * flotante. Los parámetros que son nombres (GL_MODULATE, GL_LINEAR...) no se
 * convierten: en GL ES un enum pasado como GLfixed es el entero tal cual. */
#include "sgl.h"

#define X2F(x) ((GLfloat)(x) * (1.0f / 65536.0f))

static GLfixed f2x(GLfloat f)
{
    float v = f * 65536.0f;
    if (v >= 2147483647.0f) return 0x7FFFFFFF;
    if (v <= -2147483648.0f) return (GLfixed)0x80000000;
    return (GLfixed)(v + (v >= 0 ? 0.5f : -0.5f));
}

static void x2fv(const GLfixed *x, GLfloat *f, int n) { for (int i = 0; i < n; i++) f[i] = X2F(x[i]); }

void glAlphaFuncx(GLenum f, GLclampx r) { glAlphaFunc(f, X2F(r)); }
void glClearColorx(GLclampx r, GLclampx g, GLclampx b, GLclampx a) { glClearColor(X2F(r), X2F(g), X2F(b), X2F(a)); }
void glClearDepthx(GLclampx d) { glClearDepthf(X2F(d)); }
void glClipPlanex(GLenum p, const GLfixed *e) { GLfloat f[4]; x2fv(e, f, 4); glClipPlanef(p, f); }
void glColor4x(GLfixed r, GLfixed g, GLfixed b, GLfixed a) { glColor4f(X2F(r), X2F(g), X2F(b), X2F(a)); }
void glDepthRangex(GLclampx n, GLclampx f) { glDepthRangef(X2F(n), X2F(f)); }

void glFogx(GLenum n, GLfixed v) { glFogf(n, n == GL_FOG_MODE ? (GLfloat)v : X2F(v)); }
void glFogxv(GLenum n, const GLfixed *v)
{
    if (n == GL_FOG_COLOR) { GLfloat f[4]; x2fv(v, f, 4); glFogfv(n, f); }
    else glFogx(n, v[0]);
}

void glFrustumx(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f) { glFrustumf(X2F(l), X2F(r), X2F(b), X2F(t), X2F(n), X2F(f)); }
void glOrthox(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f) { glOrthof(X2F(l), X2F(r), X2F(b), X2F(t), X2F(n), X2F(f)); }

void glGetClipPlanex(GLenum p, GLfixed *e) { GLfloat f[4] = { 0, 0, 0, 0 }; glGetClipPlanef(p, f); for (int i = 0; i < 4; i++) e[i] = f2x(f[i]); }

void glGetLightxv(GLenum l, GLenum n, GLfixed *v)
{
    GLfloat f[4] = { 0, 0, 0, 0 };
    glGetLightfv(l, n, f);
    int k = n == GL_AMBIENT || n == GL_DIFFUSE || n == GL_SPECULAR || n == GL_POSITION ? 4 : n == GL_SPOT_DIRECTION ? 3 : 1;
    for (int i = 0; i < k; i++) v[i] = f2x(f[i]);
}

void glGetMaterialxv(GLenum fc, GLenum n, GLfixed *v)
{
    GLfloat f[4] = { 0, 0, 0, 0 };
    glGetMaterialfv(fc, n, f);
    int k = n == GL_SHININESS ? 1 : 4;
    for (int i = 0; i < k; i++) v[i] = f2x(f[i]);
}

/* Los nombres de TexEnv que son enums (y no números). */
static int env_enum(GLenum n)
{
    return n == GL_TEXTURE_ENV_MODE || n == GL_COMBINE_RGB || n == GL_COMBINE_ALPHA || n == GL_COORD_REPLACE_OES ||
           (n >= GL_SRC0_RGB && n <= GL_SRC2_RGB) || (n >= GL_SRC0_ALPHA && n <= GL_SRC2_ALPHA) ||
           (n >= GL_OPERAND0_RGB && n <= GL_OPERAND2_RGB) || (n >= GL_OPERAND0_ALPHA && n <= GL_OPERAND2_ALPHA);
}

void glGetTexEnvxv(GLenum e, GLenum n, GLfixed *v)
{
    GLfloat f[4] = { 0, 0, 0, 0 };
    glGetTexEnvfv(e, n, f);
    int k = n == GL_TEXTURE_ENV_COLOR ? 4 : 1;
    for (int i = 0; i < k; i++) v[i] = env_enum(n) ? (GLfixed)f[i] : f2x(f[i]);
}

void glGetTexParameterxv(GLenum a, GLenum n, GLfixed *v)
{
    GLfloat f[4] = { 0, 0, 0, 0 };
    glGetTexParameterfv(a, n, f);
    int k = n == GL_TEXTURE_CROP_RECT_OES ? 4 : 1;
    for (int i = 0; i < k; i++) v[i] = n == GL_TEXTURE_MAX_ANISOTROPY_EXT ? f2x(f[i]) : (GLfixed)f[i];
}

void glLightModelx(GLenum n, GLfixed v) { glLightModelf(n, n == GL_LIGHT_MODEL_TWO_SIDE ? (GLfloat)v : X2F(v)); }
void glLightModelxv(GLenum n, const GLfixed *v)
{
    if (n == GL_LIGHT_MODEL_TWO_SIDE) { glLightModelx(n, v[0]); return; }
    GLfloat f[4]; x2fv(v, f, 4); glLightModelfv(n, f);
}

void glLightx(GLenum l, GLenum n, GLfixed v) { glLightf(l, n, X2F(v)); }
void glLightxv(GLenum l, GLenum n, const GLfixed *v)
{
    int k = n == GL_AMBIENT || n == GL_DIFFUSE || n == GL_SPECULAR || n == GL_POSITION ? 4 : n == GL_SPOT_DIRECTION ? 3 : 1;
    GLfloat f[4] = { 0, 0, 0, 0 };
    x2fv(v, f, k);
    glLightfv(l, n, f);
}

void glLineWidthx(GLfixed w) { glLineWidth(X2F(w)); }
void glLoadMatrixx(const GLfixed *m) { GLfloat f[16]; x2fv(m, f, 16); glLoadMatrixf(f); }
void glMultMatrixx(const GLfixed *m) { GLfloat f[16]; x2fv(m, f, 16); glMultMatrixf(f); }

void glMaterialx(GLenum fc, GLenum n, GLfixed v) { glMaterialf(fc, n, X2F(v)); }
void glMaterialxv(GLenum fc, GLenum n, const GLfixed *v)
{
    GLfloat f[4] = { 0, 0, 0, 0 };
    x2fv(v, f, n == GL_SHININESS ? 1 : 4);
    glMaterialfv(fc, n, f);
}

void glMultiTexCoord4x(GLenum t, GLfixed s, GLfixed tt, GLfixed r, GLfixed q) { glMultiTexCoord4f(t, X2F(s), X2F(tt), X2F(r), X2F(q)); }
void glNormal3x(GLfixed x, GLfixed y, GLfixed z) { glNormal3f(X2F(x), X2F(y), X2F(z)); }

void glPointParameterx(GLenum n, GLfixed v) { glPointParameterf(n, X2F(v)); }
void glPointParameterxv(GLenum n, const GLfixed *v)
{
    GLfloat f[3] = { 0, 0, 0 };
    x2fv(v, f, n == GL_POINT_DISTANCE_ATTENUATION ? 3 : 1);
    glPointParameterfv(n, f);
}
void glPointSizex(GLfixed t) { glPointSize(X2F(t)); }
void glPolygonOffsetx(GLfixed f, GLfixed u) { glPolygonOffset(X2F(f), X2F(u)); }
void glRotatex(GLfixed a, GLfixed x, GLfixed y, GLfixed z) { glRotatef(X2F(a), X2F(x), X2F(y), X2F(z)); }
void glSampleCoveragex(GLclampx v, GLboolean inv) { glSampleCoverage(X2F(v), inv); }
void glScalex(GLfixed x, GLfixed y, GLfixed z) { glScalef(X2F(x), X2F(y), X2F(z)); }
void glTranslatex(GLfixed x, GLfixed y, GLfixed z) { glTranslatef(X2F(x), X2F(y), X2F(z)); }

void glTexEnvx(GLenum e, GLenum n, GLfixed v) { glTexEnvf(e, n, env_enum(n) ? (GLfloat)v : X2F(v)); }
void glTexEnvxv(GLenum e, GLenum n, const GLfixed *v)
{
    if (n == GL_TEXTURE_ENV_COLOR) { GLfloat f[4]; x2fv(v, f, 4); glTexEnvfv(e, n, f); }
    else glTexEnvx(e, n, v[0]);
}

void glTexParameterx(GLenum a, GLenum n, GLfixed v) { glTexParameterf(a, n, n == GL_TEXTURE_MAX_ANISOTROPY_EXT ? X2F(v) : (GLfloat)v); }
void glTexParameterxv(GLenum a, GLenum n, const GLfixed *v)
{
    if (n == GL_TEXTURE_CROP_RECT_OES) { GLint i[4] = { v[0], v[1], v[2], v[3] }; glTexParameteriv(a, n, i); }
    else glTexParameterx(a, n, v[0]);
}

void glTexGenxOES(GLenum c, GLenum n, GLfixed v) { glTexGeniOES(c, n, (GLint)v); }
void glTexGenxvOES(GLenum c, GLenum n, const GLfixed *v) { glTexGeniOES(c, n, (GLint)v[0]); }
void glGetTexGenxvOES(GLenum c, GLenum n, GLfixed *v) { GLint i = 0; glGetTexGenivOES(c, n, &i); v[0] = i; }
