// SPDX-License-Identifier: GPL-2.0-or-later
/* miniz.h - sustituto mínimo de miniz para Gearsystem en SaguiOS (D321).
 *
 * El núcleo de Gearsystem abre ROM dentro de ficheros .zip con miniz (MIT). Aquí no se incluye miniz: este
 * fichero declara lo que el núcleo usa y todas las operaciones fallan, de modo que una ROM en un .zip
 * se rechaza («no es una ROM») y las ROM sueltas (.sms, .gg, .sg) funcionan igual. Se descomprime
 * con Archivos antes de jugar.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef int mz_bool;
typedef unsigned int mz_uint;
typedef struct { int vacio; } mz_zip_archive;
typedef struct {
    char m_filename[512];
    char m_comment[512];
    unsigned long long m_uncomp_size, m_comp_size;
    int m_is_directory;
} mz_zip_archive_file_stat;

static inline mz_bool mz_zip_reader_init_mem(mz_zip_archive *, void *, size_t, mz_uint) { return 0; }
static inline mz_bool mz_zip_reader_init_file(mz_zip_archive *, const char *, mz_uint) { return 0; }
static inline mz_uint mz_zip_reader_get_num_files(mz_zip_archive *) { return 0; }
static inline mz_bool mz_zip_reader_file_stat(mz_zip_archive *, mz_uint, mz_zip_archive_file_stat *) { return 0; }
static inline void *mz_zip_reader_extract_file_to_heap(mz_zip_archive *, const char *, size_t *, mz_uint) { return 0; }
static inline mz_bool mz_zip_reader_extract_to_file(mz_zip_archive *, mz_uint, const char *, mz_uint) { return 0; }
static inline mz_bool mz_zip_reader_end(mz_zip_archive *) { return 1; }
