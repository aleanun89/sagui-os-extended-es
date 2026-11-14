/* sys_sagui.c - el «sys» de Quake en el SaguiOS (D229): ficheros, reloj,
 * memoria, mensajes, errores y el bucle principal.
 *
 * Es el sitio de sys_linux.c/sys_null.c del WinQuake de id, escrito para el
 * SaguiOS sobre la libc común (D210) y la libsagui:
 *   - ficheros: con stdio; las rutas pasan por la superposición de
 *     arquivos_quake.c (/home/quake encima de los datos de /jogos);
 *   - reloj: tempo_ms() (milisegundos, de sobra para los 72 cuadros por
 *     segundo del motor);
 *   - memoria: el «hunk» de Quake es un bloque del malloc de D209, de 24 MB
 *     (o lo que se pueda hasta 8; «-mem N» lo fija);
 *   - mensajes: lo que el motor escribe en su consola va también al registro
 *     del sistema, con el prefijo [QUAKE] (lo leen las pruebas);
 *   - error fatal: el registro y una ventana con el mensaje.
 */
#include "quakedef.h"
#include "arquivos_quake.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

qboolean isDedicated = false;

/* ------------------------------------------------------------------ ficheros */

#define MAX_FICHEROS 16
static FILE *ficheros[MAX_FICHEROS];

static int ficheiro_livre(void)
{
    for (int i = 1; i < MAX_FICHEROS; i++)
        if (!ficheros[i]) return i;
    Sys_Error("sys_sagui: demasiados ficheros abiertos");
    return -1;
}

static int tamanho(FILE *f)
{
    long pos = ftell(f);
    fseek(f, 0, SEEK_END);
    long fim = ftell(f);
    fseek(f, pos, SEEK_SET);
    return (int)fim;
}

int Sys_FileOpenRead(char *path, int *hndl)
{
    char real[256];
    int i = ficheiro_livre();
    FILE *f = NULL;
    if (quake_resolver(path, real, sizeof real) >= 0) f = fopen(real, "rb");
    if (!f) {
        *hndl = -1;
        return -1;
    }
    ficheros[i] = f;
    *hndl = i;
    return tamanho(f);
}

int Sys_FileOpenWrite(char *path)
{
    int i = ficheiro_livre();
    FILE *f = quake_fopen(path, "wb");   /* siempre bajo /home/quake: crea la carpeta */
    if (!f) Sys_Error("Error opening %s", path);
    ficheros[i] = f;
    return i;
}

void Sys_FileClose(int handle)
{
    if (handle <= 0 || handle >= MAX_FICHEROS || !ficheros[handle]) return;
    fclose(ficheros[handle]);
    ficheros[handle] = NULL;
}

void Sys_FileSeek(int handle, int position)
{
    if (handle > 0 && handle < MAX_FICHEROS && ficheros[handle]) fseek(ficheros[handle], position, SEEK_SET);
}

int Sys_FileRead(int handle, void *dest, int count)
{
    if (handle <= 0 || handle >= MAX_FICHEROS || !ficheros[handle]) return -1;
    return (int)fread(dest, 1, (size_t)count, ficheros[handle]);
}

int Sys_FileWrite(int handle, void *data, int count)
{
    if (handle <= 0 || handle >= MAX_FICHEROS || !ficheros[handle]) return -1;
    return (int)fwrite(data, 1, (size_t)count, ficheros[handle]);
}

/* El motor solo compara fechas con -cachedir (que aquí no se usa): basta
 * con decir si existe. */
int Sys_FileTime(char *path)
{
    char real[256];
    if (quake_resolver(path, real, sizeof real) < 0) return -1;
    FILE *f = fopen(real, "rb");
    if (!f) return -1;
    fclose(f);
    return 1;
}

void Sys_mkdir(char *path)
{
    mkdir(path, 0777);   /* si ya existe, falla sin más */
}

void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length)
{
    (void)startaddr;
    (void)length;   /* solo lo usa el ensamblador (id386), que aquí no va */
}

/* ------------------------------------------------------------------ mensajes */

/* Una línea del registro, en ASCII: Quake marca texto con el bit 7 (letras
 * «doradas») y usa caracteres de control de su fuente para los adornos. */
static char linha[512];
static int  linha_n = 0;

static void linha_soltar(void)
{
    if (!linha_n) return;
    linha[linha_n] = 0;
    registrar("[QUAKE] %s", linha);
    linha_n = 0;
}

static void linha_acrescentar(const char *t)
{
    for (; *t; t++) {
        unsigned char c = (unsigned char)*t & 0x7F;
        if (c == '\n') { linha_soltar(); continue; }
        if (c < 32 || c == 127) c = (c == '\t') ? ' ' : 0;
        if (!c) continue;
        if (linha_n >= (int)sizeof linha - 1) linha_soltar();
        linha[linha_n++] = (char)c;
    }
}

void Sys_Printf(char *fmt, ...)
{
    char texto[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(texto, sizeof texto, fmt, ap);
    va_end(ap);
    linha_acrescentar(texto);
}

void Sys_DebugLog(char *file, char *fmt, ...)
{
    (void)file;
    (void)fmt;
}

void Sys_Error(char *error, ...)
{
    static int dentro = 0;
    char texto[1024];
    va_list ap;
    va_start(ap, error);
    vsnprintf(texto, sizeof texto, error, ap);
    va_end(ap);
    linha_soltar();
    if (dentro++) {   /* un error dentro del error: salir sin más */
        registrar("[QUAKE] Sys_Error (otra vez): %s", texto);
        exit(1);
    }
    registrar("[QUAKE] Sys_Error: %s", texto);
    /* Sin Host_Shutdown: el error puede venir de a medio arrancar. */
    quake_janela_erro(texto);
    registrar("[QUAKE] fin (1)");
    fflush(NULL);
    exit(1);
}

void Sys_Quit(void)
{
    Host_Shutdown();   /* guarda config.cfg en /home/quake/id1 */
    linha_soltar();
    registrar("[QUAKE] fin (0)");
    fflush(NULL);
    exit(0);
}

char *Sys_ConsoleInput(void)
{
    return NULL;
}

void Sys_Sleep(void)
{
    dormir(1);
}

void Sys_LowFPPrecision(void) {}
void Sys_HighFPPrecision(void) {}
void Sys_SetFPCW(void) {}

/* ------------------------------------------------------------------ reloj */

static unsigned tempo_inicio;

double Sys_FloatTime(void)
{
    return (double)(unsigned)(tempo_ms() - tempo_inicio) / 1000.0;
}

/* ------------------------------------------------------------------ arranque */

#define MB (1024 * 1024)

void quake_correr(int argc, char **argv)
{
    static quakeparms_t parms;
    double tempo, antes, agora;
    int j;

    tempo_inicio = tempo_ms() - 1000;   /* que el reloj no empiece en 0 */
    memset(&parms, 0, sizeof parms);
    COM_InitArgv(argc, argv);
    parms.argc = com_argc;
    parms.argv = com_argv;
    parms.basedir = (char *)quake_raiz;
    parms.cachedir = NULL;

    /* El «hunk» (toda la memoria del motor): 24 MB sobran para el juego
     * entero y los mods de id; si no hay, menos (el shareware cabe en 8). */
    j = COM_CheckParm("-mem");
    if (j && j + 1 < com_argc) {
        parms.memsize = (int)(Q_atof(com_argv[j + 1]) * MB);
        parms.membase = malloc((size_t)parms.memsize);
    } else {
        static const int tamanhos[] = { 24, 16, 12, 8 };
        for (j = 0; j < (int)(sizeof tamanhos / sizeof tamanhos[0]) && !parms.membase; j++) {
            parms.memsize = tamanhos[j] * MB;
            parms.membase = malloc((size_t)parms.memsize);
        }
    }
    if (!parms.membase) Sys_Error("No hay memoria para Quake (%d MB)", parms.memsize / MB);
    registrar("[QUAKE] memoria: %d MB; datos del usuario en %s", parms.memsize / MB, quake_raiz);

    Host_Init(&parms);
    linha_soltar();
    registrar("[QUAKE] motor iniciado");

    /* El bucle de sys_linux.c, pero sin quemar la CPU: el motor no dibuja
     * más de 72 cuadros por segundo (Host_FilterTime), así que entre uno y
     * otro se duerme. */
    antes = Sys_FloatTime() - 0.1;
    for (;;) {
        agora = Sys_FloatTime();
        tempo = agora - antes;
        if (tempo < 1.0 / 72.0 && !cls.timedemo) {
            int ms = (int)((1.0 / 72.0 - tempo) * 1000.0);
            dormir(ms > 0 ? (unsigned)ms : 1);
            continue;
        }
        Host_Frame((float)tempo);
        antes = agora;
    }
}
