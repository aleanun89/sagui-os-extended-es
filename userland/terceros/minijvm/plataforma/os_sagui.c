// SPDX-License-Identifier: MIT
/* os_sagui.c - la parte del sistema operativo de miniJVM para SaguiOS (D376).
 * Sustituye a jvm/os_posix.c de miniJVM (MIT, (c) 2023 Gust): las funciones de idioma, conversión de
 * codificación y UUID son suyas, copiadas de ahí; lo demás (procesos, bibliotecas dinámicas) es
 * nuestro y devuelve «no soportado». */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

#include "jvm.h"
#include "jvm_util.h"

void os_setup_crash_handler() {
}

/* Lanzar procesos desde Java (Runtime.exec): no soportado todavía. */
s32 os_execute(Runtime *runtime, Instance *jstrArr, Instance *jlongArr, ArrayList *cstrList, const c8 *cmd) {
    (void) jstrArr; (void) jlongArr; (void) cstrList; (void) cmd;
    exception_throw(JVM_EXCEPTION_IO, runtime, NULL);
    return RUNTIME_STATUS_EXCEPTION;
}

s32 os_kill_process(s64 pid) {
    (void) pid;
    return 0;
}

s32 os_waitfor_process(Runtime *runtime, s64 pid, s64 tid, s32 *pExitCode) {
    (void) runtime; (void) pid; (void) tid;
    *pExitCode = -1;
    return 0;
}

Utf8String *os_get_tmp_dir() {
    Utf8String *tmps = utf8_create();
    utf8_append_c(tmps, "/tmp");
    return tmps;
}

void os_set_file_length(FILE *file, s64 len) {
    long current_pos = ftell(file);
    fseek(file, 0, SEEK_SET);
    ftruncate(fileno(file), (off_t) len);
    fseek(file, current_pos, SEEK_SET);
}

s32 os_mkdir(const c8 *path) {
    return mkdir(path, 0755);
}

s32 os_iswin() {
    return 0;
}

s32 os_fileno(FILE *fd) {
    return fileno(fd);
}

/* Sin dlopen: JNI nativo por bibliotecas dinámicas no existe en SaguiOS (los métodos nativos
 * propios van enlazados en el ejecutable, ver java_nativos.c). */
s32 os_append_libname(Utf8String *libname, const c8 *lib) {
    utf8_append_c(libname, "/lib");
    utf8_replace_c(libname, "//", "/");
    utf8_append_c(libname, lib);
    utf8_append_c(libname, ".so");
    return 0;
}

s32 os_load_lib_and_init(const c8 *libname, Runtime *runtime) {
    (void) runtime;
    jvm_printf(STR_JNI_LIB_NOT_FOUND, libname, "sin dlopen en SaguiOS");
    return 0;
}

void os_get_lang(Utf8String *buf) {
    char *lang = getenv("LC_ALL"); //linux
    if (lang == NULL) {
        lang = getenv("LANG");
    }
    if (lang == NULL) {
        lang = getenv("LC_CTYPE"); //mac os
    }
    if (lang != NULL) {
        //printf("language: %s\n", lang);
        utf8_append_c(buf, lang);
        utf8_replace_c(buf, "-", "_");
        s32 i = utf8_indexof_c(buf, "_");
        if (i > 0) {
            utf8_substring(buf, 0, i);
        }
    } else {
        //printf("无法获取系统语言环境。\n");
    }
}

/**
 * for posix system, implement convert utf8 path to platform encoding
 * macOS and linux use utf8 as default encoding, so copy src to dst
 */
s32 conv_utf8_2_platform_encoding(ByteBuf *dst, Utf8String *src) {
    // Check input parameters
    if (!dst || !src) {
        return -1;
    }

    // Get source string length
    s32 src_len = src->length;
    if (src_len == 0) {
        bytebuf_expand(dst, 1); // Allocate space for null terminator
        dst->buf[0] = 0;
        return 0;
    }

    // Expand destination buffer to fit source string plus null terminator
    bytebuf_expand(dst, src_len + 1);

    // Copy source string to destination buffer
    memcpy(dst->buf, src->data, src_len);
    dst->buf[src_len] = 0; // Add null terminator

    return src_len;
}

s32 conv_platform_encoding_2_utf8(Utf8String *dst, const c8 *src) {
    // Check input parameters
    if (!dst || !src) {
        return -1;
    }

    // Get source string length
    s32 src_len = strlen(src);
    if (src_len == 0) {
        utf8_clear(dst);
        return 0;
    }

    // Expand destination buffer to fit source string plus null terminator
    utf8_expand(dst, src_len + 1);

    // Copy source string to destination buffer
    memcpy(dst->data, src, src_len);
    dst->length = src_len;
    dst->data[src_len] = 0; // Ensure null termination

    return src_len;
}



void os_get_uuid(MiniJVM *jvm, Utf8String *buf) {
    utf8_clear(buf);

    // Get hostname using POSIX API
    char deviceId[256] = {0};
    if (gethostname(deviceId, sizeof(deviceId)) != 0) {
        // If gethostname fails, use a default value
        strcpy(deviceId, "UNKNOWN");
    }

    // Get absolute path of startup directory
    char absPath[PATH_MAX] = {0};
    if (realpath(utf8_cstr(jvm->startup_dir), absPath) == NULL) {
        // If realpath fails, use the original path
        strcpy(absPath, utf8_cstr(jvm->startup_dir));
    }

    u64 hash0 = 0;
    for (int i = 0; deviceId[i] != '\0'; i++) {
        hash0 = 31 * hash0 + deviceId[i];
    }

    // Generate hash1 using custom hash1 function
    u64 hash1 = 0;
    for (int i = 0; absPath[i] != '\0'; i++) {
        hash1 = 31 * hash1 + absPath[i];
    }

    // Format as UUID using the hash1 value
    char uuid[37] = {0};
    snprintf(uuid, sizeof(uuid),
             "%08llx-%04llx-%04llx-%04llx-%012llx",
             hash0 & 0xFFFFFFFF,
             (hash1 >> 16) & 0xFFFF,
             (hash1 >> 32) & 0xFFFF,
             (hash1 >> 48) & 0xFFFF,
             hash0 ^ hash1);

    utf8_append_c(buf, uuid);
}

