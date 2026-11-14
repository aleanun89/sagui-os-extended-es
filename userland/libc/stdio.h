/* stdio.h - ficheros con búfer y salida formateada (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_STDIO_H
#define NS_STDIO_H

#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define BUFSIZ 4096
#define FILENAME_MAX 1024
#define FOPEN_MAX 64
#define L_tmpnam 64
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
typedef struct ns_ficheiro FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
FILE  *fopen(const char *caminho, const char *modo);
FILE  *fdopen(int fd, const char *modo);
FILE  *freopen(const char *caminho, const char *modo, FILE *f);
int    fclose(FILE *f);
int    fileno(FILE *f);
size_t fread(void *p, size_t tam, size_t n, FILE *f);
size_t fwrite(const void *p, size_t tam, size_t n, FILE *f);
int    fseek(FILE *f, long desloc, int origem);
int    fseeko(FILE *f, off_t desloc, int origem);
long   ftell(FILE *f);
off_t  ftello(FILE *f);
void   rewind(FILE *f);
int    fgetc(FILE *f);
int    getc(FILE *f);
int    getchar(void);
int    fputc(int c, FILE *f);
int    putc(int c, FILE *f);
int    ungetc(int c, FILE *f);
char  *fgets(char *s, int n, FILE *f);
ssize_t getline(char **linha, size_t *n, FILE *f);
ssize_t getdelim(char **linha, size_t *n, int delim, FILE *f);
int    fputs(const char *s, FILE *f);
int    fflush(FILE *f);
int    feof(FILE *f);
int    ferror(FILE *f);
void   clearerr(FILE *f);
void   setbuf(FILE *f, char *buf);
int    setvbuf(FILE *f, char *buf, int modo, size_t tam);
void   setlinebuf(FILE *f);
FILE  *popen(const char *cmd, const char *modo);
int    pclose(FILE *f);
int    remove(const char *caminho);
int    rename(const char *de, const char *para);
FILE  *tmpfile(void);
char  *tmpnam(char *s);
void   perror(const char *s);
int   printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int   fprintf(FILE *f, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int   sprintf(char *s, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int   snprintf(char *s, size_t n, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
int   asprintf(char **s, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int   vprintf(const char *fmt, va_list ap);
int   vfprintf(FILE *f, const char *fmt, va_list ap);
int   vsprintf(char *s, const char *fmt, va_list ap);
int   vsnprintf(char *s, size_t n, const char *fmt, va_list ap);
int   vasprintf(char **s, const char *fmt, va_list ap);
int   sscanf(const char *s, const char *fmt, ...);
int   vsscanf(const char *s, const char *fmt, va_list ap);
int   fscanf(FILE *f, const char *fmt, ...);
int   putchar(int c);
int   puts(const char *s);
#endif
