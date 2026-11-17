/* fc_config.h - lo que el configure de Freeciv deja en fc_config.h, para SaguiOS (D374). */
#ifndef FC__FC_CONFIG_H
#define FC__FC_CONFIG_H

#include "freeciv_config.h"
#include "version_gen.h"

#define LOCALEDIR "/apps/freeciv/locale"
#define BUG_URL "https://redmine.freeciv.org/projects/freeciv"
#define WIKI_URL "https://www.freeciv.org/wiki/"
#define DATASUBDIR "freeciv"
#define AI_MOD_DEFAULT "classic"
#define AI_MOD_STATIC_CLASSIC
#define DEFAULT_SOCK_PORT 5556
#define INIT_BRACE_BEGIN {
#define INIT_BRACE_END   }
#define DEFAULT_DATA_PATH ".:data:~/.freeciv/3.1:/apps/freeciv/data"
#define DEFAULT_SAVE_PATH ".:~/.freeciv/saves"
#define DEFAULT_SCENARIO_PATH ".:data/scenarios:~/.freeciv/3.1/scenarios:~/.freeciv/scenarios:/apps/freeciv/data/scenarios"
#define BINDIR "/apps/bin"
#define FC_CONF_PATH "/apps/freeciv"
#define ALWAYS_ROOT 1   /* getuid() da 0 en SaguiOS: no negarse a correr */
#define SIZEOF_INT 4
#define SIZE_T_PRINTF "%zu"
#define ENABLE_NLS 1

#define HAVE_FCNTL_H 1
#define HAVE_TIME_H 1
#define HAVE_NETDB_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_LOCALE_H 1
#define HAVE_LIBINTL_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_ARPA_INET_H 1
#define HAVE_DIRENT_H 1
#define HAVE_STDBOOL_H 1
#define HAVE_VSNPRINTF 1
#define HAVE_WORKING_VSNPRINTF 1
#define HAVE_VPRINTF 1
#define HAVE_VA_COPY 1
#define HAVE_BIND 1
#define HAVE_CONNECT 1
#define HAVE_FILENO 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_OPENDIR 1
#define HAVE_GETCWD 1
#define HAVE_SELECT 1
#define HAVE_WORKING_FORK 1   /* el cliente lanza el servidor con executar() (parche en connectdlg_common.c) */
#define HAVE_STRSTR 1
#define HAVE_STRERROR 1
#define HAVE_USLEEP 1
#define HAVE_GETADDRINFO 1
#define HAVE_GETNAMEINFO 1
#define HAVE_INET_NTOP 1
#define HAVE_INET_PTON 1
#define HAVE_LOCALTIME_R 1
#define HAVE_GMTIME_R 1
#define HAVE_FCNTL 1
#define NONBLOCKING_SOCKETS 1
#define HAVE_SETENV 1
#define HAVE_PUTENV 1
#define HAVE_STRCASESTR 1
#define MALLOC_ZERO_OK 1
#define FREECIV_NO_ICU 1   /* D374: sin ICU; support.c compara sin distinguir mayúsculas a mano */

#endif /* FC__FC_CONFIG_H */
