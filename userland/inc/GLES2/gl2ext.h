// SPDX-License-Identifier: GPL-2.0-or-later
/* GLES2/gl2ext.h - las extensiones de OpenGL ES 2.0 que tiene SaguiGL 2.0 (D475). Propia. */
#ifndef __gl2ext_h_
#define __gl2ext_h_ 1

#include "gl2platform.h"

/* GL_OES_standard_derivatives: dFdx, dFdy y fwidth en los sombreadores de fragmentos */
#define GL_OES_standard_derivatives 1
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES 0x8B8B

/* GL_OES_rgb8_rgba8: renderbuffers de 8 bits por canal */
#define GL_OES_rgb8_rgba8 1
#define GL_RGB8_OES  0x8051
#define GL_RGBA8_OES 0x8058

/* GL_OES_element_index_uint: índices de 32 bits en glDrawElements */
#define GL_OES_element_index_uint 1

/* GL_OES_texture_npot: texturas NPOT con mipmaps y repetición */
#define GL_OES_texture_npot 1

/* D486: GL_OES_depth_texture: texturas GL_DEPTH_COMPONENT (GL_UNSIGNED_SHORT o GL_UNSIGNED_INT) que se adjuntan como profundidad de un FBO
 * y se muestrean en el sombreador (devuelven (d, d, d, 1)); sin tokens nuevos. */
#define GL_OES_depth_texture 1

/* D486: GL_OES_mapbuffer: glMapBufferOES / glUnmapBufferOES para GL_ARRAY_BUFFER y GL_ELEMENT_ARRAY_BUFFER (solo escritura) */
#define GL_OES_mapbuffer 1
#ifndef GL_WRITE_ONLY_OES
#define GL_WRITE_ONLY_OES                 0x88B9
#define GL_BUFFER_ACCESS_OES              0x88BB
#define GL_BUFFER_MAPPED_OES              0x88BC
#define GL_BUFFER_MAP_POINTER_OES         0x88BD
#endif
GL_APICALL void *GL_APIENTRY glMapBufferOES(GLenum target, GLenum access);
GL_APICALL GLboolean GL_APIENTRY glUnmapBufferOES(GLenum target);
GL_APICALL void GL_APIENTRY glGetBufferPointervOES(GLenum target, GLenum pname, GLvoid **params);

/* D486: GL_OES_depth24: renderbuffers de profundidad de 24 bits */
#define GL_OES_depth24 1
#define GL_DEPTH_COMPONENT24_OES 0x81A6

/* D486: GL_OES_packed_depth_stencil: profundidad de 24 bits y esténcil de 8 en un solo renderbuffer o textura. Además de adjuntarlo a
 * GL_DEPTH_ATTACHMENT y GL_STENCIL_ATTACHMENT, SaguiGL 2.0 acepta GL_DEPTH_STENCIL_ATTACHMENT (como Mesa). */
#define GL_OES_packed_depth_stencil 1
#define GL_DEPTH_STENCIL_OES          0x84F9
#define GL_UNSIGNED_INT_24_8_OES      0x84FA
#define GL_DEPTH24_STENCIL8_OES       0x88F0
#define GL_DEPTH_STENCIL_ATTACHMENT   0x821A

#endif
