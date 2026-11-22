/* SPDX-License-Identifier: GPL-3.0-only */
/* lua_sagui.h - lo que Lua (con LUA_USE_POSIX) pide a stdio y la libc de SaguiOS no tiene (D440). Se incluye con -include.
 * Sin hilos de usuario sobre stdio: getc_unlocked es getc y los cerrojos de FILE no hacen nada. */
#ifndef LUA_SAGUI_H
#define LUA_SAGUI_H
#include <stdio.h>
#define getc_unlocked(f) getc(f)
#define flockfile(f)     ((void)(f))
#define funlockfile(f)   ((void)(f))
#endif
