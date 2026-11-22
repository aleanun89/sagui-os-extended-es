// SPDX-License-Identifier: GPL-2.0-or-later
/* EGL/eglplatform.h - tipos nativos de EGL en SaguiOS (D425). Propia.
 *
 * La ventana nativa es la de la libsagui (jan_t *, de gui.h); no hay pixmaps:
 * EGLNativePixmapType existe solo para compilar. */
#ifndef __eglplatform_h_
#define __eglplatform_h_

#include "../KHR/khrplatform.h"

#ifndef EGLAPI
#define EGLAPI KHRONOS_APICALL
#endif
#ifndef EGLAPIENTRY
#define EGLAPIENTRY KHRONOS_APIENTRY
#endif
#define EGLAPIENTRYP EGLAPIENTRY *

typedef void *EGLNativeDisplayType;
typedef void *EGLNativePixmapType;
typedef void *EGLNativeWindowType;   /* jan_t * */

typedef EGLNativeDisplayType NativeDisplayType;
typedef EGLNativePixmapType  NativePixmapType;
typedef EGLNativeWindowType  NativeWindowType;

typedef khronos_int32_t EGLint;

#endif
