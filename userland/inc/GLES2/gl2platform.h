// SPDX-License-Identifier: GPL-2.0-or-later
/* GLES2/gl2platform.h - plataforma de OpenGL ES 2.0 en SaguiOS (D475). Propia, con los nombres de la
 * especificación. */
#ifndef __gl2platform_h_
#define __gl2platform_h_

#include "../KHR/khrplatform.h"

#ifndef GL_APICALL
#define GL_APICALL  KHRONOS_APICALL
#endif
#ifndef GL_APIENTRY
#define GL_APIENTRY KHRONOS_APIENTRY
#endif
#ifndef GL_APIENTRYP
#define GL_APIENTRYP GL_APIENTRY *
#endif

#endif
