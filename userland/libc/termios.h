/* termios.h - modo del terminal (D477). El terminal de SaguiOS no tiene modos configurables desde el
 * programa: tcgetattr falla con ENOTTY y los programas que lo usan (linenoise de valkey-cli) caen a
 * leer líneas enteras con fgets, que es como funciona el terminal. tcsetattr acepta y no hace nada. */
#ifndef SAGUI_TERMIOS_H
#define SAGUI_TERMIOS_H
#include <sys/types.h>
typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;
#define NCCS 32
struct termios {
    tcflag_t c_iflag, c_oflag, c_cflag, c_lflag;
    cc_t c_cc[NCCS];
};
/* valores de Linux */
#define IGNBRK 0000001
#define BRKINT 0000002
#define IGNPAR 0000004
#define PARMRK 0000010
#define INPCK  0000020
#define ISTRIP 0000040
#define INLCR  0000100
#define IGNCR  0000200
#define ICRNL  0000400
#define IXON   0002000
#define IXOFF  0010000
#define OPOST  0000001
#define ONLCR  0000004
#define CSIZE  0000060
#define CS8    0000060
#define PARENB 0000400
#define CREAD  0000200
#define ISIG   0000001
#define ICANON 0000002
#define ECHO   0000010
#define ECHONL 0000100
#define IEXTEN 0100000
#define NOFLSH 0000200
#define VINTR  0
#define VQUIT  1
#define VERASE 2
#define VKILL  3
#define VEOF   4
#define VTIME  5
#define VMIN   6
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2
#define TCIFLUSH 0
#define TCOFLUSH 1
#define TCIOFLUSH 2
int tcgetattr(int fd, struct termios *t);
int tcsetattr(int fd, int cuando, const struct termios *t);
#endif
