/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Configuración de AoEN: dónde están los datos originales del juego. No necesita SDL; la comparten el juego y aoen-config. */
#ifndef AOEN_CONFIG_H
#define AOEN_CONFIG_H

#include <stddef.h>

/* los tres archivos que necesita el motor; la carpeta debe contenerlos todos */
extern const char *cfg_required_files[3];

/* ruta del fichero de configuración en uso (el fichero portable junto al ejecutable tiene prioridad sobre el perfil de usuario) */
void cfg_path(char *out, size_t n);
/* ruta del fichero de configuración del perfil de usuario (lo crea aoen-config) */
void cfg_user_path(char *out, size_t n);

/* lee/escribe la carpeta de datos configurada; devuelve 1 si tiene éxito */
int cfg_load_data_dir(char *out, size_t n);
int cfg_save_data_dir(const char *dir, int portable);

/*
 * Comprueba una carpeta indicada por el usuario. Acepta la propia carpeta de datos, la carpeta del juego o la raíz del CD
 * (prueba DATA, GAME/DATA, Data, data... sin distinguir mayúsculas). Si tiene éxito, escribe la carpeta real en
 * resolved y devuelve 1. Si no, devuelve 0 y lista en missing los ficheros que faltan.
 */
int cfg_check_dir(const char *dir, char *resolved, size_t n, char *missing, size_t mn);

/* busca la carpeta de datos: línea de órdenes, AOEN_DATA, fichero de configuración y luego algunas ubicaciones habituales */
int cfg_find_data(const char *cmdline, char *resolved, size_t n);

/* lista los ficheros de una carpeta cuyo nombre acaba en ext (sin distinguir mayúsculas); devuelve el número */
int cfg_list_files(const char *dir, const char *ext, char names[][260], int max);
/* carpeta con la música MIDI: AOEN_MUSIC, music_dir de la configuración, musica/ junto al programa o la carpeta de sonido del CD */
int cfg_find_music(const char *data_dir, char *out, size_t n);

/* ajustes enteros pequeños guardados en el fichero de configuración (volúmenes, pantalla completa...) */
int cfg_get_int(const char *key, int def);
int cfg_set_int(const char *key, int value);

/* carpeta de las partidas guardadas (se crea si falta) */
void cfg_saves_dir(char *out, size_t n);

/* texturas HD del terreno opcionales (PNG, 512x512): carpeta con g_gr2_00_color.png, g_des_..., g_wt5_..., g_wt3_... */
int cfg_find_textures(char *out, size_t n);
int cfg_save_textures_dir(const char *dir, int portable);

/* texto con las instrucciones sobre lo que debe contener la carpeta de datos */
const char *cfg_instructions(void);

#endif
