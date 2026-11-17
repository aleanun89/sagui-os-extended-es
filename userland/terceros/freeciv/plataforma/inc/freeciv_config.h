/* freeciv_config.h - la configuración de Freeciv para SaguiOS (D374).
 * Lo que el configure de Freeciv genera en un anfitrión Unix, escrito a mano para el SaguiOS:
 * sin hilos, sin IPv6, sin jansson, con zlib real, con NLS por nuestro libintl. */
#ifndef FC__FREECIV_CONFIG_H
#define FC__FREECIV_CONFIG_H

#define FOLLOWTAG "stable"
#define FREECIV_AI_MOD_LAST 1
#define FREECIV_HAVE_SOCKLEN_T
#define FREECIV_STORAGE_DIR "/home/.freeciv"
#define FREECIV_META_URL "https://meta.freeciv.org/metaserver.php"
#define FREECIV_ENABLE_NLS
#define FREECIV_HAVE_LOCALE_H
#define FREECIV_HAVE_LIBINTL_H
#define FREECIV_HAVE_INTTYPES_H
#define FREECIV_HAVE_STDINT_H
#define FREECIV_HAVE_SYS_TYPES_H
#define FREECIV_HAVE_UNISTD_H
#define FREECIV_HAVE_SYS_TIME_H
#define FREECIV_HAVE_SYS_SOCKET_H
#define FREECIV_HAVE_SYS_SELECT_H
#define FREECIV_HAVE_NETINET_IN_H
#define FREECIV_HAVE_DIRENT_H
#define FREECIV_HAVE_STDBOOL_H
#define FREECIV_HAVE_LIBZ
#define FREECIV_DELTA_PROTOCOL
#define FREECIV_C11_STATIC_ASSERT
#define FREECIV_STATIC_STRLEN
#define FREECIV_HAVE_TINYCTHR
#define FREECIV_HAVE_THREAD_COND
#define FREECIV_NDEBUG

#endif /* FC__FREECIV_CONFIG_H */
