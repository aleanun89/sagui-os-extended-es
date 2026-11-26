// SPDX-License-Identifier: GPL-3.0-only
/* native-state-sagui.cpp - el «estado nativo» de glmark2 sobre las ventanas de SaguiOS (D481). */
#include "native-state-sagui.h"
#include "log.h"

#include "janela.h"

NativeStateSagui::~NativeStateSagui()
{
    if (win_)
        gm_jan_fechar(win_);
}

bool
NativeStateSagui::init_display()
{
    return true;   /* la libsagui ya está conectada al sistema de ventanas */
}

void*
NativeStateSagui::display()
{
    return 0;      /* EGL_DEFAULT_DISPLAY */
}

bool
NativeStateSagui::create_window(WindowProperties const& properties)
{
    /* Se recrea solo si cambió el tamaño o el modo, como en la versión X11. */
    if (win_) {
        if (properties_.fullscreen == properties.fullscreen &&
            (properties.fullscreen || (properties_.width == properties.width &&
                                       properties_.height == properties.height)))
            return true;
        gm_jan_fechar(win_);
        win_ = 0;
    }

    int w = properties.width, h = properties.height;
    if (properties.fullscreen || w <= 0 || h <= 0) {
        w = 800;
        h = 600;
    }

    void *j = gm_jan_abrir("glmark2", w, h);
    if (!j) {
        Log::error("Error: no se pudo abrir la ventana de SaguiOS\n");
        return false;
    }
    win_ = j;
    properties_.fullscreen = properties.fullscreen;
    properties_.visual_id = properties.visual_id;

    if (properties.fullscreen) {
        if (!gm_jan_completa(j))
            Log::info("Aviso: el sistema no dio la pantalla completa; sigue la ventana de %dx%d\n",
                      gm_jan_larg(j), gm_jan_alt(j));
    }
    properties_.width = gm_jan_larg(j);
    properties_.height = gm_jan_alt(j);
    return true;
}

void*
NativeStateSagui::window(WindowProperties& properties)
{
    properties = properties_;
    return win_;
}

void
NativeStateSagui::visible(bool v)
{
    if (v && win_)
        gm_jan_mostrar(win_);
}

bool
NativeStateSagui::should_quit()
{
    if (win_ && gm_jan_eventos(win_))
        quit_ = true;
    return quit_;
}
