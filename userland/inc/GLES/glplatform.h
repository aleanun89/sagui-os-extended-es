// SPDX-License-Identifier: GPL-2.0-or-later
/* GLES/glplatform.h - plataforma de OpenGL ES 1.x en SaguiOS (D425). Propia,
 * con los nombres de la especificación. */
#ifndef __glplatform_h_
#define __glplatform_h_

#include "../KHR/khrplatform.h"

#ifndef GL_API
#define GL_API      KHRONOS_APICALL
#endif
#ifndef GL_APIENTRY
#define GL_APIENTRY KHRONOS_APIENTRY
#endif
#ifndef GL_APIENTRYP
#define GL_APIENTRYP GL_APIENTRY *
#endif

#endif
