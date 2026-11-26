#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Genera el reparto entre OpenGL ES 1.1 y 2.0 de la libsgl (D475): las funciones con el mismo nombre en
las dos API. Escribe userland/libsgl/sgl_es1_nomes.h (las de ES 1.1 pasan a llamarse es1_gl*),
userland/libsgl/sgl2_comuns.h (los prototipos de las dos versiones) y userland/libsgl/sgl2_despacho.c
(los gl* de verdad: van a la de ES 2.0 si el contexto actual es de ES 2.0)."""
import os

RAIZ = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
COMUNS = """
void ActiveTexture(GLenum a)
void BindBuffer(GLenum a, GLuint b)
void BindTexture(GLenum a, GLuint b)
void BlendFunc(GLenum a, GLenum b)
void BufferData(GLenum a, GLsizeiptr b, const void *c, GLenum d)
void BufferSubData(GLenum a, GLintptr b, GLsizeiptr c, const void *d)
void Clear(GLbitfield a)
void ClearColor(GLfloat a, GLfloat b, GLfloat c, GLfloat d)
void ClearDepthf(GLfloat a)
void ClearStencil(GLint a)
void ColorMask(GLboolean a, GLboolean b, GLboolean c, GLboolean d)
void CompressedTexImage2D(GLenum a, GLint b, GLenum c, GLsizei d, GLsizei e, GLint f, GLsizei g, const void *h)
void CompressedTexSubImage2D(GLenum a, GLint b, GLint c, GLint d, GLsizei e, GLsizei f, GLenum g, GLsizei h, const void *i)
void CopyTexImage2D(GLenum a, GLint b, GLenum c, GLint d, GLint e, GLsizei f, GLsizei g, GLint h)
void CopyTexSubImage2D(GLenum a, GLint b, GLint c, GLint d, GLint e, GLint f, GLsizei g, GLsizei h)
void CullFace(GLenum a)
void DeleteBuffers(GLsizei a, const GLuint *b)
void DeleteTextures(GLsizei a, const GLuint *b)
void DepthFunc(GLenum a)
void DepthMask(GLboolean a)
void DepthRangef(GLfloat a, GLfloat b)
void Disable(GLenum a)
void DrawArrays(GLenum a, GLint b, GLsizei c)
void DrawElements(GLenum a, GLsizei b, GLenum c, const void *d)
void Enable(GLenum a)
void FrontFace(GLenum a)
void GenBuffers(GLsizei a, GLuint *b)
void GenerateMipmap(GLenum a)
void GenTextures(GLsizei a, GLuint *b)
void GetBooleanv(GLenum a, GLboolean *b)
void GetBufferParameteriv(GLenum a, GLenum b, GLint *c)
GLenum GetError(void)
void GetFloatv(GLenum a, GLfloat *b)
void GetIntegerv(GLenum a, GLint *b)
const GLubyte * GetString(GLenum a)
void GetTexParameterfv(GLenum a, GLenum b, GLfloat *c)
void GetTexParameteriv(GLenum a, GLenum b, GLint *c)
void Hint(GLenum a, GLenum b)
GLboolean IsBuffer(GLuint a)
GLboolean IsEnabled(GLenum a)
GLboolean IsTexture(GLuint a)
void LineWidth(GLfloat a)
void PixelStorei(GLenum a, GLint b)
void PolygonOffset(GLfloat a, GLfloat b)
void ReadPixels(GLint a, GLint b, GLsizei c, GLsizei d, GLenum e, GLenum f, void *g)
void SampleCoverage(GLfloat a, GLboolean b)
void Scissor(GLint a, GLint b, GLsizei c, GLsizei d)
void StencilFunc(GLenum a, GLint b, GLuint c)
void StencilMask(GLuint a)
void StencilOp(GLenum a, GLenum b, GLenum c)
void TexImage2D(GLenum a, GLint b, GLint c, GLsizei d, GLsizei e, GLint f, GLenum g, GLenum h, const void *i)
void TexParameterf(GLenum a, GLenum b, GLfloat c)
void TexParameterfv(GLenum a, GLenum b, const GLfloat *c)
void TexParameteri(GLenum a, GLenum b, GLint c)
void TexParameteriv(GLenum a, GLenum b, const GLint *c)
void TexSubImage2D(GLenum a, GLint b, GLint c, GLint d, GLsizei e, GLsizei f, GLenum g, GLenum h, const void *i)
void Viewport(GLint a, GLint b, GLsizei c, GLsizei d)
"""


def funcoes():
    r = []
    for l in COMUNS.strip().splitlines():
        a = l.index("(")
        cab, params = l[:a], l[a + 1:-1]
        ret, nome = cab.rsplit(" ", 1)
        args = [] if params == "void" else [p.replace("*", " ").split()[-1] for p in params.split(",")]
        r.append((ret.strip(), nome, params, args))
    return r


CAB = "// SPDX-License-Identifier: GPL-3.0-only\n/* Generado por tools/sgl/gerar_despacho.py (D475): no tocar a mano. */\n"


def main():
    fs = funcoes()
    lib = os.path.join(RAIZ, "userland", "libsgl")
    with open(os.path.join(lib, "sgl_es1_nomes.h"), "w", encoding="utf-8") as o:
        o.write(CAB + "/* sgl_es1_nomes.h - con -DSGL_ES2, las funciones de OpenGL ES 1.1 que también son de ES 2.0 se\n"
                " * compilan como es1_gl* (sgl.c lo incluye el primero); los gl* los pone sgl2_despacho.c. */\n"
                "#ifndef SAGUI_SGL_ES1_NOMES_H\n#define SAGUI_SGL_ES1_NOMES_H\n")
        for _, n, _, _ in fs:
            o.write(f"#define gl{n} es1_gl{n}\n")
        o.write("#endif\n")
    with open(os.path.join(lib, "sgl2_comuns.h"), "w", encoding="utf-8") as o:
        o.write(CAB + "/* sgl2_comuns.h - las funciones con el mismo nombre en OpenGL ES 1.1 y 2.0: la versión de ES 1.1\n"
                " * (es1_gl*, sgl.c) y la de ES 2.0 (sgl2_gl*, sgl2.c). */\n"
                "#ifndef SAGUI_SGL2_COMUNS_H\n#define SAGUI_SGL2_COMUNS_H\n")
        for ret, n, p, _ in fs:
            o.write(f"{ret} es1_gl{n}({p});\n{ret} sgl2_gl{n}({p});\n")
        o.write("#endif\n")
    with open(os.path.join(lib, "sgl2_despacho.c"), "w", encoding="utf-8") as o:
        o.write(CAB + "/* sgl2_despacho.c - los gl* que comparten OpenGL ES 1.1 y 2.0: con un contexto de ES 2.0 actual van\n"
                " * a SaguiGL 2.0 (sgl2.c), si no a la ES 1.1 de siempre (sgl.c). */\n"
                "#include \"../inc/GLES2/gl2.h\"\n#include \"sgl2_comuns.h\"\n\n"
                "extern void *sgl2_atual;   /* sgl2.c: el contexto de ES 2.0 actual (NULL: ES 1.1) */\n\n")
        for ret, n, p, a in fs:
            ar = ", ".join(a)
            if ret == "void":
                o.write(f"{ret} gl{n}({p}) {{ if (sgl2_atual) sgl2_gl{n}({ar}); else es1_gl{n}({ar}); }}\n")
            else:
                o.write(f"{ret} gl{n}({p}) {{ return sgl2_atual ? sgl2_gl{n}({ar}) : es1_gl{n}({ar}); }}\n")


if __name__ == "__main__":
    main()
