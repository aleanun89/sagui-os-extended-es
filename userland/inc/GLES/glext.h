// SPDX-License-Identifier: GPL-2.0-or-later
/* GLES/glext.h - extensiones de OpenGL ES 1.1 que da SaguiGL (D425).
 *
 * Cabecera propia con los nombres y valores de los registros de Khronos. Las
 * que salen en glGetString(GL_EXTENSIONS) tienen aquí sus funciones (con
 * GL_GLEXT_PROTOTYPES o sin él: siempre se declaran) y sus punteros PFN...
 * para eglGetProcAddress. */
#ifndef __gles1_glext_h_
#define __gles1_glext_h_ 1

#include "gl.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GL_APIENTRYP
#define GL_APIENTRYP GL_APIENTRY *
#endif

/* ---- OES_blend_equation_separate, OES_blend_func_separate, OES_blend_subtract ---- */
#define GL_OES_blend_equation_separate 1
#define GL_OES_blend_func_separate 1
#define GL_OES_blend_subtract 1
#define GL_BLEND_EQUATION_RGB_OES         0x8009
#define GL_BLEND_EQUATION_ALPHA_OES       0x883D
#define GL_BLEND_DST_RGB_OES              0x80C8
#define GL_BLEND_SRC_RGB_OES              0x80C9
#define GL_BLEND_DST_ALPHA_OES            0x80CA
#define GL_BLEND_SRC_ALPHA_OES            0x80CB
#define GL_BLEND_EQUATION_OES             0x8009
#define GL_FUNC_ADD_OES                   0x8006
#define GL_FUNC_SUBTRACT_OES              0x800A
#define GL_FUNC_REVERSE_SUBTRACT_OES      0x800B
GL_API void GL_APIENTRY glBlendEquationSeparateOES(GLenum modeRGB, GLenum modeAlpha);
GL_API void GL_APIENTRY glBlendFuncSeparateOES(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
GL_API void GL_APIENTRY glBlendEquationOES(GLenum mode);
typedef void (GL_APIENTRYP PFNGLBLENDEQUATIONSEPARATEOESPROC)(GLenum modeRGB, GLenum modeAlpha);
typedef void (GL_APIENTRYP PFNGLBLENDFUNCSEPARATEOESPROC)(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
typedef void (GL_APIENTRYP PFNGLBLENDEQUATIONOESPROC)(GLenum mode);

/* ---- OES_byte_coordinates, OES_fixed_point, OES_single_precision ---- */
#define GL_OES_byte_coordinates 1
#define GL_OES_fixed_point 1
#define GL_OES_single_precision 1
#define GL_FIXED_OES                      0x140C

/* ---- OES_draw_texture ---- */
#define GL_OES_draw_texture 1
#define GL_TEXTURE_CROP_RECT_OES          0x8B9D
GL_API void GL_APIENTRY glDrawTexsOES(GLshort x, GLshort y, GLshort z, GLshort width, GLshort height);
GL_API void GL_APIENTRY glDrawTexiOES(GLint x, GLint y, GLint z, GLint width, GLint height);
GL_API void GL_APIENTRY glDrawTexxOES(GLfixed x, GLfixed y, GLfixed z, GLfixed width, GLfixed height);
GL_API void GL_APIENTRY glDrawTexsvOES(const GLshort *coords);
GL_API void GL_APIENTRY glDrawTexivOES(const GLint *coords);
GL_API void GL_APIENTRY glDrawTexxvOES(const GLfixed *coords);
GL_API void GL_APIENTRY glDrawTexfOES(GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height);
GL_API void GL_APIENTRY glDrawTexfvOES(const GLfloat *coords);
typedef void (GL_APIENTRYP PFNGLDRAWTEXFOESPROC)(GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height);
typedef void (GL_APIENTRYP PFNGLDRAWTEXIOESPROC)(GLint x, GLint y, GLint z, GLint width, GLint height);

/* ---- OES_element_index_uint ---- */
#define GL_OES_element_index_uint 1
#define GL_UNSIGNED_INT                   0x1405

/* ---- OES_mapbuffer ---- */
#define GL_OES_mapbuffer 1
#define GL_WRITE_ONLY_OES                 0x88B9
#define GL_BUFFER_ACCESS_OES              0x88BB
#define GL_BUFFER_MAPPED_OES              0x88BC
#define GL_BUFFER_MAP_POINTER_OES         0x88BD
GL_API void *GL_APIENTRY glMapBufferOES(GLenum target, GLenum access);
GL_API GLboolean GL_APIENTRY glUnmapBufferOES(GLenum target);
GL_API void GL_APIENTRY glGetBufferPointervOES(GLenum target, GLenum pname, GLvoid **params);
typedef void *(GL_APIENTRYP PFNGLMAPBUFFEROESPROC)(GLenum target, GLenum access);
typedef GLboolean (GL_APIENTRYP PFNGLUNMAPBUFFEROESPROC)(GLenum target);
typedef void (GL_APIENTRYP PFNGLGETBUFFERPOINTERVOESPROC)(GLenum target, GLenum pname, GLvoid **params);

/* ---- OES_matrix_get ---- */
#define GL_OES_matrix_get 1
#define GL_MODELVIEW_MATRIX_FLOAT_AS_INT_BITS_OES  0x898D
#define GL_PROJECTION_MATRIX_FLOAT_AS_INT_BITS_OES 0x898E
#define GL_TEXTURE_MATRIX_FLOAT_AS_INT_BITS_OES    0x898F

/* ---- OES_matrix_palette ---- */
#define GL_OES_matrix_palette 1
#define GL_MAX_VERTEX_UNITS_OES           0x86A4
#define GL_MAX_PALETTE_MATRICES_OES       0x8842
#define GL_MATRIX_PALETTE_OES             0x8840
#define GL_MATRIX_INDEX_ARRAY_OES         0x8844
#define GL_WEIGHT_ARRAY_OES               0x86AD
#define GL_CURRENT_PALETTE_MATRIX_OES     0x8843
#define GL_MATRIX_INDEX_ARRAY_SIZE_OES    0x8846
#define GL_MATRIX_INDEX_ARRAY_TYPE_OES    0x8847
#define GL_MATRIX_INDEX_ARRAY_STRIDE_OES  0x8848
#define GL_MATRIX_INDEX_ARRAY_POINTER_OES 0x8849
#define GL_MATRIX_INDEX_ARRAY_BUFFER_BINDING_OES 0x8B9E
#define GL_WEIGHT_ARRAY_SIZE_OES          0x86AB
#define GL_WEIGHT_ARRAY_TYPE_OES          0x86A9
#define GL_WEIGHT_ARRAY_STRIDE_OES        0x86AA
#define GL_WEIGHT_ARRAY_POINTER_OES       0x86AC
#define GL_WEIGHT_ARRAY_BUFFER_BINDING_OES 0x889E
GL_API void GL_APIENTRY glCurrentPaletteMatrixOES(GLuint matrixpaletteindex);
GL_API void GL_APIENTRY glLoadPaletteFromModelViewMatrixOES(void);
GL_API void GL_APIENTRY glMatrixIndexPointerOES(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
GL_API void GL_APIENTRY glWeightPointerOES(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (GL_APIENTRYP PFNGLCURRENTPALETTEMATRIXOESPROC)(GLuint matrixpaletteindex);
typedef void (GL_APIENTRYP PFNGLLOADPALETTEFROMMODELVIEWMATRIXOESPROC)(void);
typedef void (GL_APIENTRYP PFNGLMATRIXINDEXPOINTEROESPROC)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (GL_APIENTRYP PFNGLWEIGHTPOINTEROESPROC)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);

/* ---- OES_query_matrix ---- */
#define GL_OES_query_matrix 1
GL_API GLbitfield GL_APIENTRY glQueryMatrixxOES(GLfixed *mantissa, GLint *exponent);
typedef GLbitfield (GL_APIENTRYP PFNGLQUERYMATRIXXOESPROC)(GLfixed *mantissa, GLint *exponent);

/* ---- OES_framebuffer_object, OES_depth24, OES_stencil8, OES_rgb8_rgba8 ---- */
#define GL_OES_framebuffer_object 1
#define GL_OES_depth24 1
#define GL_OES_stencil8 1
#define GL_OES_rgb8_rgba8 1
#define GL_NONE_OES                       0
#define GL_FRAMEBUFFER_OES                0x8D40
#define GL_RENDERBUFFER_OES               0x8D41
#define GL_RGBA4_OES                      0x8056
#define GL_RGB5_A1_OES                    0x8057
#define GL_RGB565_OES                     0x8D62
#define GL_RGB8_OES                       0x8051
#define GL_RGBA8_OES                      0x8058
#define GL_DEPTH_COMPONENT16_OES          0x81A5
#define GL_DEPTH_COMPONENT24_OES          0x81A6
#define GL_STENCIL_INDEX8_OES             0x8D48
#define GL_RENDERBUFFER_WIDTH_OES         0x8D42
#define GL_RENDERBUFFER_HEIGHT_OES        0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT_OES 0x8D44
#define GL_RENDERBUFFER_RED_SIZE_OES      0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE_OES    0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE_OES     0x8D52
#define GL_RENDERBUFFER_ALPHA_SIZE_OES    0x8D53
#define GL_RENDERBUFFER_DEPTH_SIZE_OES    0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE_OES  0x8D55
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_OES 0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_OES 0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_OES 0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_OES 0x8CD3
#define GL_COLOR_ATTACHMENT0_OES          0x8CE0
#define GL_DEPTH_ATTACHMENT_OES           0x8D00
#define GL_STENCIL_ATTACHMENT_OES         0x8D20
#define GL_FRAMEBUFFER_COMPLETE_OES       0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_OES 0x8CD9
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES 0x8CDA
#define GL_FRAMEBUFFER_UNSUPPORTED_OES    0x8CDD
#define GL_FRAMEBUFFER_BINDING_OES        0x8CA6
#define GL_RENDERBUFFER_BINDING_OES       0x8CA7
#define GL_MAX_RENDERBUFFER_SIZE_OES      0x84E8
#define GL_INVALID_FRAMEBUFFER_OPERATION_OES 0x0506
GL_API GLboolean GL_APIENTRY glIsRenderbufferOES(GLuint renderbuffer);
GL_API void GL_APIENTRY glBindRenderbufferOES(GLenum target, GLuint renderbuffer);
GL_API void GL_APIENTRY glDeleteRenderbuffersOES(GLsizei n, const GLuint *renderbuffers);
GL_API void GL_APIENTRY glGenRenderbuffersOES(GLsizei n, GLuint *renderbuffers);
GL_API void GL_APIENTRY glRenderbufferStorageOES(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
GL_API void GL_APIENTRY glGetRenderbufferParameterivOES(GLenum target, GLenum pname, GLint *params);
GL_API GLboolean GL_APIENTRY glIsFramebufferOES(GLuint framebuffer);
GL_API void GL_APIENTRY glBindFramebufferOES(GLenum target, GLuint framebuffer);
GL_API void GL_APIENTRY glDeleteFramebuffersOES(GLsizei n, const GLuint *framebuffers);
GL_API void GL_APIENTRY glGenFramebuffersOES(GLsizei n, GLuint *framebuffers);
GL_API GLenum GL_APIENTRY glCheckFramebufferStatusOES(GLenum target);
GL_API void GL_APIENTRY glFramebufferRenderbufferOES(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
GL_API void GL_APIENTRY glFramebufferTexture2DOES(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
GL_API void GL_APIENTRY glGetFramebufferAttachmentParameterivOES(GLenum target, GLenum attachment, GLenum pname, GLint *params);
GL_API void GL_APIENTRY glGenerateMipmapOES(GLenum target);
typedef GLboolean (GL_APIENTRYP PFNGLISRENDERBUFFEROESPROC)(GLuint renderbuffer);
typedef void (GL_APIENTRYP PFNGLBINDRENDERBUFFEROESPROC)(GLenum target, GLuint renderbuffer);
typedef void (GL_APIENTRYP PFNGLDELETERENDERBUFFERSOESPROC)(GLsizei n, const GLuint *renderbuffers);
typedef void (GL_APIENTRYP PFNGLGENRENDERBUFFERSOESPROC)(GLsizei n, GLuint *renderbuffers);
typedef void (GL_APIENTRYP PFNGLRENDERBUFFERSTORAGEOESPROC)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP PFNGLGETRENDERBUFFERPARAMETERIVOESPROC)(GLenum target, GLenum pname, GLint *params);
typedef GLboolean (GL_APIENTRYP PFNGLISFRAMEBUFFEROESPROC)(GLuint framebuffer);
typedef void (GL_APIENTRYP PFNGLBINDFRAMEBUFFEROESPROC)(GLenum target, GLuint framebuffer);
typedef void (GL_APIENTRYP PFNGLDELETEFRAMEBUFFERSOESPROC)(GLsizei n, const GLuint *framebuffers);
typedef void (GL_APIENTRYP PFNGLGENFRAMEBUFFERSOESPROC)(GLsizei n, GLuint *framebuffers);
typedef GLenum (GL_APIENTRYP PFNGLCHECKFRAMEBUFFERSTATUSOESPROC)(GLenum target);
typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERRENDERBUFFEROESPROC)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERTEXTURE2DOESPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (GL_APIENTRYP PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVOESPROC)(GLenum target, GLenum attachment, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP PFNGLGENERATEMIPMAPOESPROC)(GLenum target);

/* ---- OES_texture_cube_map ---- */
#define GL_OES_texture_cube_map 1
#define GL_NORMAL_MAP_OES                 0x8511
#define GL_REFLECTION_MAP_OES             0x8512
#define GL_TEXTURE_CUBE_MAP_OES           0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP_OES   0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X_OES 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y_OES 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_OES 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z_OES 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_OES 0x851A
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE_OES  0x851C
#define GL_TEXTURE_GEN_MODE_OES           0x2500
#define GL_TEXTURE_GEN_STR_OES            0x8D60
GL_API void GL_APIENTRY glTexGenfOES(GLenum coord, GLenum pname, GLfloat param);
GL_API void GL_APIENTRY glTexGenfvOES(GLenum coord, GLenum pname, const GLfloat *params);
GL_API void GL_APIENTRY glTexGeniOES(GLenum coord, GLenum pname, GLint param);
GL_API void GL_APIENTRY glTexGenivOES(GLenum coord, GLenum pname, const GLint *params);
GL_API void GL_APIENTRY glTexGenxOES(GLenum coord, GLenum pname, GLfixed param);
GL_API void GL_APIENTRY glTexGenxvOES(GLenum coord, GLenum pname, const GLfixed *params);
GL_API void GL_APIENTRY glGetTexGenfvOES(GLenum coord, GLenum pname, GLfloat *params);
GL_API void GL_APIENTRY glGetTexGenivOES(GLenum coord, GLenum pname, GLint *params);
GL_API void GL_APIENTRY glGetTexGenxvOES(GLenum coord, GLenum pname, GLfixed *params);
typedef void (GL_APIENTRYP PFNGLTEXGENFOESPROC)(GLenum coord, GLenum pname, GLfloat param);
typedef void (GL_APIENTRYP PFNGLTEXGENIOESPROC)(GLenum coord, GLenum pname, GLint param);

/* ---- OES_texture_mirrored_repeat, OES_texture_npot ---- */
#define GL_OES_texture_mirrored_repeat 1
#define GL_MIRRORED_REPEAT_OES            0x8370
#define GL_OES_texture_npot 1

/* ---- EXT_texture_compression_s3tc (y _dxt1) ---- */
#define GL_EXT_texture_compression_s3tc 1
#define GL_EXT_texture_compression_dxt1 1
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3

/* ---- EXT_texture_filter_anisotropic ---- */
#define GL_EXT_texture_filter_anisotropic 1
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

/* ---- OES_EGL_image: no ---- */

/* Punteros del núcleo que más se piden por eglGetProcAddress. */
typedef void (GL_APIENTRYP PFNGLPOINTSIZEPOINTEROESPROC)(GLenum type, GLsizei stride, const GLvoid *pointer);

#ifdef __cplusplus
}
#endif

#endif
