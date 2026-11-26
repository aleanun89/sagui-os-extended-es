// SPDX-License-Identifier: GPL-3.0-only
/* native-state-sagui.h - el «estado nativo» de glmark2 sobre las ventanas de SaguiOS (D481).
 *
 * glmark2 separa el sistema de ventanas (NativeState: X11, Wayland, DRM...) de EGL/GL (GLState). Aquí la ventana
 * nativa es una jan_t de la libsagui y la superficie EGL de la SaguiGL 2.0 la toma directamente
 * (sgl_egl.c: eglCreateWindowSurface recibe la jan_t *). No hay display: el de EGL es EGL_DEFAULT_DISPLAY. */
#ifndef GLMARK2_NATIVE_STATE_SAGUI_H_
#define GLMARK2_NATIVE_STATE_SAGUI_H_

#include "native-state.h"

struct jan_t_;

class NativeStateSagui : public NativeState
{
public:
    NativeStateSagui() : win_(0), quit_(false), properties_() {}
    ~NativeStateSagui();

    bool init_display();
    void* display();
    bool create_window(WindowProperties const& properties);
    void* window(WindowProperties& properties);
    void visible(bool v);
    bool should_quit();
    void flip() { }

private:
    void* win_;          /* la jan_t * */
    bool quit_;          /* se pidió cerrar (EV_FECHAR o Esc) */
    WindowProperties properties_;
};

#endif /* GLMARK2_NATIVE_STATE_SAGUI_H_ */
