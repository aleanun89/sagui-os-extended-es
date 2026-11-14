/* dirent.h - lectura de carpetas (sobre listar() del SaguiOS) (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_DIRENT_H
#define NS_DIRENT_H

#include <sys/types.h>
#define DT_UNKNOWN 0
#define DT_DIR     4
#define DT_REG     8
struct dirent {
    ino_t d_ino;
    unsigned char d_type;
    char d_name[256];
};
typedef struct ns_pasta DIR;
DIR           *opendir(const char *caminho);
struct dirent *readdir(DIR *d);
int            closedir(DIR *d);
/* dirfd: un descriptor "de carpeta" que solo entienden fstatat/unlinkat de
 * esta libc (el SaguiOS no abre carpetas como ficheros). */
int            dirfd(DIR *d);
void           rewinddir(DIR *d);
int scandir(const char *caminho, struct dirent ***lista,
            int (*filtro)(const struct dirent *),
            int (*cmp)(const struct dirent **, const struct dirent **));
int alphasort(const struct dirent **a, const struct dirent **b);
#endif
