// SPDX-License-Identifier: GPL-3.0-only
/* Generado por tools/sgl/gerar_despacho.py (D475): no tocar a mano. */
/* sgl2_despacho.c - los gl* que comparten OpenGL ES 1.1 y 2.0: con un contexto de ES 2.0 actual van
 * a SaguiGL 2.0 (sgl2.c), si no a la ES 1.1 de siempre (sgl.c). */
#include "../inc/GLES2/gl2.h"
#include "sgl2_comuns.h"

extern void *sgl2_atual;   /* sgl2.c: el contexto de ES 2.0 actual (NULL: ES 1.1) */

void glActiveTexture(GLenum a) { if (sgl2_atual) sgl2_glActiveTexture(a); else es1_glActiveTexture(a); }
void glBindBuffer(GLenum a, GLuint b) { if (sgl2_atual) sgl2_glBindBuffer(a, b); else es1_glBindBuffer(a, b); }
void glBindTexture(GLenum a, GLuint b) { if (sgl2_atual) sgl2_glBindTexture(a, b); else es1_glBindTexture(a, b); }
void glBlendFunc(GLenum a, GLenum b) { if (sgl2_atual) sgl2_glBlendFunc(a, b); else es1_glBlendFunc(a, b); }
void glBufferData(GLenum a, GLsizeiptr b, const void *c, GLenum d) { if (sgl2_atual) sgl2_glBufferData(a, b, c, d); else es1_glBufferData(a, b, c, d); }
void glBufferSubData(GLenum a, GLintptr b, GLsizeiptr c, const void *d) { if (sgl2_atual) sgl2_glBufferSubData(a, b, c, d); else es1_glBufferSubData(a, b, c, d); }
void glClear(GLbitfield a) { if (sgl2_atual) sgl2_glClear(a); else es1_glClear(a); }
void glClearColor(GLfloat a, GLfloat b, GLfloat c, GLfloat d) { if (sgl2_atual) sgl2_glClearColor(a, b, c, d); else es1_glClearColor(a, b, c, d); }
void glClearDepthf(GLfloat a) { if (sgl2_atual) sgl2_glClearDepthf(a); else es1_glClearDepthf(a); }
void glClearStencil(GLint a) { if (sgl2_atual) sgl2_glClearStencil(a); else es1_glClearStencil(a); }
void glColorMask(GLboolean a, GLboolean b, GLboolean c, GLboolean d) { if (sgl2_atual) sgl2_glColorMask(a, b, c, d); else es1_glColorMask(a, b, c, d); }
void glCompressedTexImage2D(GLenum a, GLint b, GLenum c, GLsizei d, GLsizei e, GLint f, GLsizei g, const void *h) { if (sgl2_atual) sgl2_glCompressedTexImage2D(a, b, c, d, e, f, g, h); else es1_glCompressedTexImage2D(a, b, c, d, e, f, g, h); }
void glCompressedTexSubImage2D(GLenum a, GLint b, GLint c, GLint d, GLsizei e, GLsizei f, GLenum g, GLsizei h, const void *i) { if (sgl2_atual) sgl2_glCompressedTexSubImage2D(a, b, c, d, e, f, g, h, i); else es1_glCompressedTexSubImage2D(a, b, c, d, e, f, g, h, i); }
void glCopyTexImage2D(GLenum a, GLint b, GLenum c, GLint d, GLint e, GLsizei f, GLsizei g, GLint h) { if (sgl2_atual) sgl2_glCopyTexImage2D(a, b, c, d, e, f, g, h); else es1_glCopyTexImage2D(a, b, c, d, e, f, g, h); }
void glCopyTexSubImage2D(GLenum a, GLint b, GLint c, GLint d, GLint e, GLint f, GLsizei g, GLsizei h) { if (sgl2_atual) sgl2_glCopyTexSubImage2D(a, b, c, d, e, f, g, h); else es1_glCopyTexSubImage2D(a, b, c, d, e, f, g, h); }
void glCullFace(GLenum a) { if (sgl2_atual) sgl2_glCullFace(a); else es1_glCullFace(a); }
void glDeleteBuffers(GLsizei a, const GLuint *b) { if (sgl2_atual) sgl2_glDeleteBuffers(a, b); else es1_glDeleteBuffers(a, b); }
void glDeleteTextures(GLsizei a, const GLuint *b) { if (sgl2_atual) sgl2_glDeleteTextures(a, b); else es1_glDeleteTextures(a, b); }
void glDepthFunc(GLenum a) { if (sgl2_atual) sgl2_glDepthFunc(a); else es1_glDepthFunc(a); }
void glDepthMask(GLboolean a) { if (sgl2_atual) sgl2_glDepthMask(a); else es1_glDepthMask(a); }
void glDepthRangef(GLfloat a, GLfloat b) { if (sgl2_atual) sgl2_glDepthRangef(a, b); else es1_glDepthRangef(a, b); }
void glDisable(GLenum a) { if (sgl2_atual) sgl2_glDisable(a); else es1_glDisable(a); }
void glDrawArrays(GLenum a, GLint b, GLsizei c) { if (sgl2_atual) sgl2_glDrawArrays(a, b, c); else es1_glDrawArrays(a, b, c); }
void glDrawElements(GLenum a, GLsizei b, GLenum c, const void *d) { if (sgl2_atual) sgl2_glDrawElements(a, b, c, d); else es1_glDrawElements(a, b, c, d); }
void glEnable(GLenum a) { if (sgl2_atual) sgl2_glEnable(a); else es1_glEnable(a); }
void glFrontFace(GLenum a) { if (sgl2_atual) sgl2_glFrontFace(a); else es1_glFrontFace(a); }
void glGenBuffers(GLsizei a, GLuint *b) { if (sgl2_atual) sgl2_glGenBuffers(a, b); else es1_glGenBuffers(a, b); }
void glGenerateMipmap(GLenum a) { if (sgl2_atual) sgl2_glGenerateMipmap(a); else es1_glGenerateMipmap(a); }
void glGenTextures(GLsizei a, GLuint *b) { if (sgl2_atual) sgl2_glGenTextures(a, b); else es1_glGenTextures(a, b); }
void glGetBooleanv(GLenum a, GLboolean *b) { if (sgl2_atual) sgl2_glGetBooleanv(a, b); else es1_glGetBooleanv(a, b); }
void glGetBufferParameteriv(GLenum a, GLenum b, GLint *c) { if (sgl2_atual) sgl2_glGetBufferParameteriv(a, b, c); else es1_glGetBufferParameteriv(a, b, c); }
GLenum glGetError(void) { return sgl2_atual ? sgl2_glGetError() : es1_glGetError(); }
void glGetFloatv(GLenum a, GLfloat *b) { if (sgl2_atual) sgl2_glGetFloatv(a, b); else es1_glGetFloatv(a, b); }
void glGetIntegerv(GLenum a, GLint *b) { if (sgl2_atual) sgl2_glGetIntegerv(a, b); else es1_glGetIntegerv(a, b); }
const GLubyte * glGetString(GLenum a) { return sgl2_atual ? sgl2_glGetString(a) : es1_glGetString(a); }
void glGetTexParameterfv(GLenum a, GLenum b, GLfloat *c) { if (sgl2_atual) sgl2_glGetTexParameterfv(a, b, c); else es1_glGetTexParameterfv(a, b, c); }
void glGetTexParameteriv(GLenum a, GLenum b, GLint *c) { if (sgl2_atual) sgl2_glGetTexParameteriv(a, b, c); else es1_glGetTexParameteriv(a, b, c); }
void glHint(GLenum a, GLenum b) { if (sgl2_atual) sgl2_glHint(a, b); else es1_glHint(a, b); }
GLboolean glIsBuffer(GLuint a) { return sgl2_atual ? sgl2_glIsBuffer(a) : es1_glIsBuffer(a); }
GLboolean glIsEnabled(GLenum a) { return sgl2_atual ? sgl2_glIsEnabled(a) : es1_glIsEnabled(a); }
GLboolean glIsTexture(GLuint a) { return sgl2_atual ? sgl2_glIsTexture(a) : es1_glIsTexture(a); }
void glLineWidth(GLfloat a) { if (sgl2_atual) sgl2_glLineWidth(a); else es1_glLineWidth(a); }
void glPixelStorei(GLenum a, GLint b) { if (sgl2_atual) sgl2_glPixelStorei(a, b); else es1_glPixelStorei(a, b); }
void glPolygonOffset(GLfloat a, GLfloat b) { if (sgl2_atual) sgl2_glPolygonOffset(a, b); else es1_glPolygonOffset(a, b); }
void glReadPixels(GLint a, GLint b, GLsizei c, GLsizei d, GLenum e, GLenum f, void *g) { if (sgl2_atual) sgl2_glReadPixels(a, b, c, d, e, f, g); else es1_glReadPixels(a, b, c, d, e, f, g); }
void glSampleCoverage(GLfloat a, GLboolean b) { if (sgl2_atual) sgl2_glSampleCoverage(a, b); else es1_glSampleCoverage(a, b); }
void glScissor(GLint a, GLint b, GLsizei c, GLsizei d) { if (sgl2_atual) sgl2_glScissor(a, b, c, d); else es1_glScissor(a, b, c, d); }
void glStencilFunc(GLenum a, GLint b, GLuint c) { if (sgl2_atual) sgl2_glStencilFunc(a, b, c); else es1_glStencilFunc(a, b, c); }
void glStencilMask(GLuint a) { if (sgl2_atual) sgl2_glStencilMask(a); else es1_glStencilMask(a); }
void glStencilOp(GLenum a, GLenum b, GLenum c) { if (sgl2_atual) sgl2_glStencilOp(a, b, c); else es1_glStencilOp(a, b, c); }
void glTexImage2D(GLenum a, GLint b, GLint c, GLsizei d, GLsizei e, GLint f, GLenum g, GLenum h, const void *i) { if (sgl2_atual) sgl2_glTexImage2D(a, b, c, d, e, f, g, h, i); else es1_glTexImage2D(a, b, c, d, e, f, g, h, i); }
void glTexParameterf(GLenum a, GLenum b, GLfloat c) { if (sgl2_atual) sgl2_glTexParameterf(a, b, c); else es1_glTexParameterf(a, b, c); }
void glTexParameterfv(GLenum a, GLenum b, const GLfloat *c) { if (sgl2_atual) sgl2_glTexParameterfv(a, b, c); else es1_glTexParameterfv(a, b, c); }
void glTexParameteri(GLenum a, GLenum b, GLint c) { if (sgl2_atual) sgl2_glTexParameteri(a, b, c); else es1_glTexParameteri(a, b, c); }
void glTexParameteriv(GLenum a, GLenum b, const GLint *c) { if (sgl2_atual) sgl2_glTexParameteriv(a, b, c); else es1_glTexParameteriv(a, b, c); }
void glTexSubImage2D(GLenum a, GLint b, GLint c, GLint d, GLsizei e, GLsizei f, GLenum g, GLenum h, const void *i) { if (sgl2_atual) sgl2_glTexSubImage2D(a, b, c, d, e, f, g, h, i); else es1_glTexSubImage2D(a, b, c, d, e, f, g, h, i); }
void glViewport(GLint a, GLint b, GLsizei c, GLsizei d) { if (sgl2_atual) sgl2_glViewport(a, b, c, d); else es1_glViewport(a, b, c, d); }
