/* arquivos_quake.c - los ficheros de Quake en el SaguiOS: lo del usuario
 * encima de los datos del juego (D229).
 *
 * Quake lee y escribe todo bajo su «basedir»: los paquetes (id1/pak0.pak),
 * la configuración (id1/config.cfg), las partidas (id1/s0.sav), las demos y
 * las capturas. En el SaguiOS los datos del juego están en la partición de
 * juegos (/jogos/<carpeta>, o jogos/ de un pendrive) y lo que escribe el
 * jugador debe ir a /home/quake. Así que el motor recibe como basedir
 * /home/quake y aquí cada ruta de debajo se resuelve como una superposición:
 *
 *   1. si existe tal cual en /home/quake, esa (lo del usuario manda);
 *   2. si no, se busca en la carpeta de los datos: «id1/...» en la carpeta id1
 *      de los datos (o en la carpeta misma, si el pak0.pak está suelto, como
 *      lo pone input_share/games/Quake/), y «<mod>/...» en <datos>/<mod>;
 *      cada trozo de la ruta sin distinguir mayúsculas (el shareware de DOS
 *      trae ID1/PAK0.PAK);
 *   3. si tampoco, la ruta de /home/quake (el fallo lo ve el motor).
 *
 * Las escrituras van siempre a /home/quake (creando la carpeta si falta).
 * El motor llama a fopen() en sus ficheros: se compila con
 * -Dfopen=quake_fopen y la llamada llega aquí (sus fuentes no se tocan).
 *
 * Solo usa la libc (sin las cabeceras de Quake ni del SaguiOS): la prueba
 * h229 lo compila también en el PC.
 */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>

#include "arquivos_quake.h"

#define CAMINHO_MAX 256

static char raiz_usuario[CAMINHO_MAX];   /* /home/quake */
static char dados[CAMINHO_MAX];          /* /jogos/Quake */
static char dados_id1[CAMINHO_MAX];      /* /jogos/Quake/ID1, o /jogos/Quake si el pak está suelto */

static void copiar(char *d, const char *o)
{
    strncpy(d, o ? o : "", CAMINHO_MAX - 1);
    d[CAMINHO_MAX - 1] = 0;
}

void quake_arquivos_iniciar(const char *usuario, const char *pasta_dados, const char *pasta_id1)
{
    copiar(raiz_usuario, usuario);
    copiar(dados, pasta_dados);
    copiar(dados_id1, pasta_id1);
}

static int existe(const char *c)
{
    struct stat st;
    return stat(c, &st) == 0;
}

static int e_pasta(const char *c)
{
    struct stat st;
    return stat(c, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Añade «nome» a «base» (que acaba sin barra) buscando el nombre sin
 * distinguir mayúsculas. 1 = encontrado (base ampliada); 0 = no está. */
static int juntar_sem_caixa(char *base, size_t max, const char *nome, size_t n)
{
    size_t l = strlen(base);
    if (l + 1 + n + 1 > max) return 0;
    base[l] = '/';
    memcpy(base + l + 1, nome, n);
    base[l + 1 + n] = 0;
    if (existe(base)) return 1;
    base[l] = 0;
    DIR *d = opendir(base[0] ? base : "/");
    if (!d) return 0;
    struct dirent *e;
    int achou = 0;
    while ((e = readdir(d))) {
        if (strlen(e->d_name) == n && strncasecmp(e->d_name, nome, n) == 0) {
            base[l] = '/';
            memcpy(base + l + 1, e->d_name, n);
            base[l + 1 + n] = 0;
            achou = 1;
            break;
        }
    }
    closedir(d);
    return achou;
}

/* Resuelve «resto» (trozos separados por '/') debajo de «base». */
static int resolver_em(char *real, size_t max, const char *base, const char *resto)
{
    if (strlen(base) >= max) return 0;
    strcpy(real, base);
    while (*resto == '/') resto++;
    while (*resto) {
        const char *fim = strchr(resto, '/');
        size_t n = fim ? (size_t)(fim - resto) : strlen(resto);
        if (n > 0 && !juntar_sem_caixa(real, max, resto, n)) return 0;
        resto += n;
        while (*resto == '/') resto++;
    }
    return 1;
}

/* Lo que va debajo de /home/quake/, o NULL si la ruta es de otro sitio. */
static const char *debaixo_do_usuario(const char *caminho)
{
    size_t l = strlen(raiz_usuario);
    if (!l || strncmp(caminho, raiz_usuario, l) != 0 || caminho[l] != '/') return NULL;
    return caminho + l + 1;
}

/* Busca «resto» (lo de debajo de /home/quake/) en la carpeta de los datos. */
static int resolver_nos_dados(const char *resto, char *real, size_t max)
{
    /* El primer trozo es la carpeta del juego: id1 o un mod. */
    const char *fim = strchr(resto, '/');
    size_t n = fim ? (size_t)(fim - resto) : strlen(resto);
    const char *depois = fim ? fim + 1 : "";
    if (n == 3 && strncasecmp(resto, "id1", 3) == 0)
        return dados_id1[0] && resolver_em(real, max, dados_id1, depois);
    if (n == 0 || n >= CAMINHO_MAX) return 0;
    char base[CAMINHO_MAX];
    copiar(base, dados);
    return juntar_sem_caixa(base, sizeof base, resto, n) && e_pasta(base)
           && resolver_em(real, max, base, depois);
}

int quake_resolver(const char *caminho, char *real, size_t max)
{
    const char *resto = debaixo_do_usuario(caminho);
    if (resto && dados[0] && !existe(caminho) && resolver_nos_dados(resto, real, max))
        return 1;
    if (strlen(caminho) >= max) return -1;
    strcpy(real, caminho);
    return 0;
}

/* Crea las carpetas que faltan antes del último trozo. */
static void criar_pastas_de(const char *caminho)
{
    char c[CAMINHO_MAX];
    copiar(c, caminho);
    for (char *p = c + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        if (!e_pasta(c)) mkdir(c, 0777);
        *p = '/';
    }
}

#undef fopen
FILE *quake_fopen(const char *caminho, const char *modo)
{
    char real[CAMINHO_MAX];
    if (!caminho || !modo) return NULL;
    if (strchr(modo, 'w') || strchr(modo, 'a')) {
        if (debaixo_do_usuario(caminho)) criar_pastas_de(caminho);
        return fopen(caminho, modo);
    }
    if (quake_resolver(caminho, real, sizeof real) < 0) return NULL;
    return fopen(real, modo);
}
