#ifndef __rt__
#define __rt__
#define RAYTRACING

#include <gl\glew.h>
#include <gl\GL.h>

void rtInit();

void rtFlush();
void rtGenBuffers(GLsizei n, GLuint* buffers);
void rtBindBuffer(GLenum target, GLuint buffer);
void rtBufferData(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage);
void rtVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void rtColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void rtNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer);
void rtDrawArrays(GLenum mode, GLint first, GLsizei count);

void rtEnableClientState(GLenum cap);
void rtDisableClientState(GLenum cap);
void rtEnable(GLenum cap);
void rtDisable(GLenum cap);

void rtPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);
void rtLookAt(GLdouble eyeX, GLdouble eyeY, GLdouble eyeZ,
				GLdouble centerX, GLdouble centerY, GLdouble centerZ,
				GLdouble upX, GLdouble upY, GLdouble upZ);

void rtLightfv(GLenum light, GLenum pname, const GLfloat *params);


/*
//four tpye: emissive, diffuse, dielectic, mirror,
// only dielectic type must provide a refractive index
*/
typedef enum { RT_MAT_DIFFUSE, RT_MAT_DIELECTRIC, RT_MAT_MIRROR } RTenum;

void rtMaterialEXT(RTenum type, float RefracIndex = 1);
void rtBuildKDtreeCurrentSceneEXT();

#define glGenBuffers rtGenBuffers
#define glBindBuffer rtBindBuffer
#define glBufferData rtBufferData
#define glVertexPointer rtVertexPointer
#define glColorPointer rtColorPointer
#define glNormalPointer rtNormalPointer
#define glFlush rtFlush
#define glDrawArrays rtDrawArrays
#define glEnableClientState rtEnableClientState
#define glDisableClientState rtDisableClientState
#define glEnable rtEnable
#define glDisable rtDisable
#define gluPerspective rtPerspective
#define gluLookAt rtLookAt
#define glLightfv rtLightfv

#endif 
