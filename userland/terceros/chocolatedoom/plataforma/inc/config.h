/* SPDX-License-Identifier: GPL-2.0-or-later */
/* config.h - lo que el CMake de Chocolate Doom genera en el anfitrión, a mano para SaguiOS (D383). */
#ifndef CONFIG_H_SAGUI
#define CONFIG_H_SAGUI
#define PACKAGE_NAME "Chocolate Doom"
#define PACKAGE_TARNAME "chocolate-doom"
#define PACKAGE_VERSION "3.1.1"
#define PACKAGE_STRING "Chocolate Doom 3.1.1"
#define PROGRAM_PREFIX "chocolate-"
#define HAVE_DECL_STRCASECMP 1
#define HAVE_DECL_STRNCASECMP 1
char *Sagui_ConfigDir(const char *app);   /* principal_cd.c: $HOME/.config/<app>/ (D383) */
#endif
