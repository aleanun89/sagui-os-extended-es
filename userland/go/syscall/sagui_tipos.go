// SPDX-License-Identifier: BSD-3-Clause
// Tipos y constantes del paquete syscall de SaguiOS (D364). Los nombres son
// los de Linux/386 de Go; los diseños son los de la capa C de
// userland/go/runtime/glue.c (que rellena estas estructuras).

package syscall

import "internal/itoa"

const (
	Stdin  = 0
	Stdout = 1
	Stderr = 2
)

// Banderas de open(): los valores de Linux, que son los de userland/libc.
const (
	O_RDONLY   = 0x0
	O_WRONLY   = 0x1
	O_RDWR     = 0x2
	O_CREAT    = 0x40
	O_EXCL     = 0x80
	O_NOCTTY   = 0x100
	O_TRUNC    = 0x200
	O_APPEND   = 0x400
	O_NONBLOCK = 0x800
	O_SYNC     = 0x101000
	O_CLOEXEC  = 0x80000
	O_LARGEFILE = 0
	O_DIRECTORY = 0x10000
)

const (
	S_IFMT   = 0xf000
	S_IFBLK  = 0x6000
	S_IFCHR  = 0x2000
	S_IFDIR  = 0x4000
	S_IFIFO  = 0x1000
	S_IFLNK  = 0xa000
	S_IFREG  = 0x8000
	S_IFSOCK = 0xc000
	S_ISGID  = 0x400
	S_ISUID  = 0x800
	S_ISVTX  = 0x200
)

const (
	DT_UNKNOWN = 0
	DT_FIFO    = 1
	DT_CHR     = 2
	DT_DIR     = 4
	DT_BLK     = 6
	DT_REG     = 8
	DT_LNK     = 10
	DT_SOCK    = 12
)

const (
	PROT_NONE  = 0
	PROT_READ  = 1
	PROT_WRITE = 2
	PROT_EXEC  = 4
	MAP_SHARED    = 1
	MAP_PRIVATE   = 2
	MAP_ANON      = 0x20
	MAP_ANONYMOUS = 0x20
)

type Signal int

const (
	_ Signal = iota
	SIGHUP
	SIGINT
	SIGQUIT
	SIGILL
	SIGTRAP
	SIGABRT
	_
	SIGFPE
	SIGKILL
	_
	SIGSEGV
	_
	SIGPIPE
	SIGALRM
	SIGTERM
	SIGCHLD = 17
)

func (s Signal) Signal() {}

func (s Signal) String() string {
	switch s {
	case SIGINT:
		return "interrupt"
	case SIGKILL:
		return "killed"
	case SIGTERM:
		return "terminated"
	case SIGHUP:
		return "hangup"
	}
	return "signal " + itoa.Itoa(int(s))
}

type Timespec struct {
	Sec  int64
	Nsec int64
}

func (ts *Timespec) Unix() (sec int64, nsec int64) { return ts.Sec, ts.Nsec }
func (ts *Timespec) Nano() int64                   { return ts.Sec*1e9 + ts.Nsec }

type Timeval struct {
	Sec  int64
	Usec int64
}

// Stat_t tiene el diseño fijo que rellena sg_stat() en glue.c.
type Stat_t struct {
	Dev     uint64
	Ino     uint64
	Mode    uint32
	Nlink   uint32
	Uid     uint32
	Gid     uint32
	Rdev    uint64
	Size    int64
	Blksize int32
	Blocks  int64
	Atim    Timespec
	Mtim    Timespec
	Ctim    Timespec
}

// Dirent es el registro linux_dirent64 que fabrica sg_getdents().
type Dirent struct {
	Ino    uint64
	Off    int64
	Reclen uint16
	Type   uint8
	Name   [256]int8
}

type Rusage struct{}

type SysProcAttr struct{}

// WaitStatus: SaguiOS no ofrece fork; los métodos existen solo por compatibilidad.
type WaitStatus uint32

func (w WaitStatus) Exited() bool       { return false }
func (w WaitStatus) ExitStatus() int    { return 0 }
func (w WaitStatus) Signaled() bool     { return false }
func (w WaitStatus) Signal() Signal     { return 0 }
func (w WaitStatus) CoreDump() bool     { return false }
func (w WaitStatus) Stopped() bool      { return false }
func (w WaitStatus) Continued() bool    { return false }
func (w WaitStatus) StopSignal() Signal { return 0 }
func (w WaitStatus) TrapCause() int     { return 0 }

type RawSockaddrInet4 struct{}
type RawSockaddrInet6 struct{}

type Statfs_t struct {
	Type    int64
	Bsize   int64
	Blocks  uint64
	Bfree   uint64
	Bavail  uint64
	Files   uint64
	Ffree   uint64
	Fsid    [2]int32
	Namelen int64
	Frsize  int64
	Flags   int64
	Spare   [4]int64
}

const SYS_EXECVE = 11
