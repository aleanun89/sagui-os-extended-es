#ifndef BRUMA_GL_API_H
#define BRUMA_GL_API_H

#include <GLES/gl.h>

#define GL_FUNCTIONS(X) X(const GLubyte *,GetString,(GLenum)) \
    X(GLenum,GetError,(void)) \
    X(void,Enable,(GLenum)) \
    X(void,Disable,(GLenum)) \
    X(void,ClearColor,(GLclampf,GLclampf,GLclampf,GLclampf)) \
    X(void,Clear,(GLbitfield)) \
    X(void,Viewport,(GLint,GLint,GLsizei,GLsizei)) \
    X(void,MatrixMode,(GLenum)) \
    X(void,LoadIdentity,(void)) \
    X(void,Frustumf,(GLfloat,GLfloat,GLfloat,GLfloat,GLfloat,GLfloat)) \
    X(void,Orthof,(GLfloat,GLfloat,GLfloat,GLfloat,GLfloat,GLfloat)) \
    X(void,Rotatef,(GLfloat,GLfloat,GLfloat,GLfloat)) \
    X(void,Translatef,(GLfloat,GLfloat,GLfloat)) \
    X(void,EnableClientState,(GLenum)) \
    X(void,DisableClientState,(GLenum)) \
    X(void,VertexPointer,(GLint,GLenum,GLsizei,const void *)) \
    X(void,ColorPointer,(GLint,GLenum,GLsizei,const void *)) \
    X(void,TexCoordPointer,(GLint,GLenum,GLsizei,const void *)) \
    X(void,DrawArrays,(GLenum,GLint,GLsizei)) \
    X(void,BindBuffer,(GLenum,GLuint)) \
    X(void,GenBuffers,(GLsizei,GLuint *)) \
    X(void,DeleteBuffers,(GLsizei,const GLuint *)) \
    X(void,BufferData,(GLenum,GLsizeiptr,const void *,GLenum)) \
    X(void,GenTextures,(GLsizei,GLuint *)) \
    X(void,DeleteTextures,(GLsizei,const GLuint *)) \
    X(void,BindTexture,(GLenum,GLuint)) \
    X(void,TexParameteri,(GLenum,GLenum,GLint)) \
    X(void,TexImage2D,(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void *)) \
    X(void,AlphaFunc,(GLenum,GLclampf)) \
    X(void,BlendFunc,(GLenum,GLenum)) \
    X(void,DepthMask,(GLboolean)) \
    X(void,Fogf,(GLenum,GLfloat)) \
    X(void,Fogfv,(GLenum,const GLfloat *)) \
    X(void,ReadPixels,(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void *))

#define GL_FIELD(type, name, parameters) type (GL_APIENTRY *name) parameters;
typedef struct { GL_FUNCTIONS(GL_FIELD) } DrawGL;
#undef GL_FIELD
#endif
