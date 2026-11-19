/* SPDX-License-Identifier: GPL-3.0-only */
/* git2_features.h - configuración de libgit2 para SaguiOS (D400).
 * Sustituye al que genera CMake. Sin hilos (un solo hilo), sin NSEC, regex de la libc,
 * zlib de deps/zlib (la libc solo trae la de lectura), SHA-1 con detección de colisiones y SHA-256
 * propios de libgit2, llhttp para HTTP. HTTPS sin OpenSSL/mbedTLS: la capa TLS de SaguiOS
 * (libtls) entra como flujo propio (git_stream_register, git_sagui_tls.c). SSH por el
 * transporte «exec» sobre /bin/ssh (git_process de plataforma/proceso.c). */
#ifndef INCLUDE_features_h__
#define INCLUDE_features_h__

#define GIT_ARCH_32 1

#define GIT_REGEX_REGCOMP 1

#define GIT_SSH 1
#define GIT_SSH_EXEC 1

#define GIT_HTTPS 1
#define GIT_HTTPPARSER_BUILTIN 1

#define GIT_SHA1_COLLISIONDETECT 1
#define GIT_SHA256_BUILTIN 1
#define GIT_COMPRESSION_BUILTIN 1

#define GIT_IO_SELECT 1

#endif
