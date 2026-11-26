// SPDX-License-Identifier: GPL-3.0-only
/* shared-library.cpp - SharedLibrary sin dlopen para SaguiOS (D481).
 *
 * glmark2 abre libEGL y libGLESv2 con dlopen y saca de ellas cada función con dlsym (glad). En SaguiOS la SaguiGL 2.0
 * va enlazada dentro del programa y su eglGetProcAddress da todas las funciones de EGL y de OpenGL ES por su nombre
 * (sgl_egl.c): «abrir» la biblioteca siempre funciona y «cargar» es pedírselo a eglGetProcAddress. */
#include "shared-library.h"

extern "C" void *eglGetProcAddress(const char *nome);

SharedLibrary::SharedLibrary() : handle_(nullptr) {}

SharedLibrary::~SharedLibrary() { close(); }

bool SharedLibrary::open(const char *libName)
{
    (void)libName;
    handle_ = this;
    return true;
}

bool SharedLibrary::open_from_alternatives(std::initializer_list<const char*> alt_names)
{
    return open(alt_names.size() ? *alt_names.begin() : "");
}

void SharedLibrary::close() { handle_ = nullptr; }

void *SharedLibrary::handle() const { return handle_; }

void *SharedLibrary::load(const char *symbol) const { return eglGetProcAddress(symbol); }
