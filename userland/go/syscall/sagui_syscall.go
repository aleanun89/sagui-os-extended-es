// SPDX-License-Identifier: BSD-3-Clause
// Paquete syscall de SaguiOS (D364) para TinyGo. Sustituye al syscall de Go
// (que en Linux hace int 0x80 con números de Linux) por llamadas a la capa
// C de userland/go/runtime/glue.c y a la libc de SaguiOS. Se apoya en el
// diseño de src/syscall/syscall_libc.go de TinyGo (BSD-3-Clause).

package syscall

import "unsafe"

//go:extern errno
var libcErrno int32

func getErrno() error {
	return Errno(libcErrno)
}

// cstring convierte una cadena de Go en una de C.
func cstring(s string) []byte {
	data := make([]byte, len(s)+1)
	copy(data, s)
	return data
}

// BytePtrFromString devuelve un puntero a una copia terminada en cero.
func BytePtrFromString(s string) (*byte, error) {
	for i := 0; i < len(s); i++ {
		if s[i] == 0 {
			return nil, EINVAL
		}
	}
	return &cstring(s)[0], nil
}

// Estas dos las pone el runtime.
func runtimeSetenv(key, value string)
func runtimeUnsetenv(key string)

// ---- funciones de la capa C (glue.c): devuelven -1 y dejan errno ----

//export strlen
func libc_strlen(ptr unsafe.Pointer) uintptr

//export getenv
func libc_getenv(name *byte) *byte

//export sg_open
func sg_open(path *byte, flags int32, mode uint32) int32

//export sg_close
func sg_close(fd int32) int32

//export sg_read
func sg_read(fd int32, buf *byte, n uint32) int32

//export sg_write
func sg_write(fd int32, buf *byte, n uint32) int32

//export sg_lseek
func sg_lseek(fd int32, off int64, whence int32) int64

//export sg_pread
func sg_pread(fd int32, buf *byte, n uint32, off int64) int32

//export sg_pwrite
func sg_pwrite(fd int32, buf *byte, n uint32, off int64) int32

//export sg_fsync
func sg_fsync(fd int32) int32

//export sg_ftruncate
func sg_ftruncate(fd int32, n int64) int32

//export sg_truncate
func sg_truncate(path *byte, n int64) int32

//export sg_stat
func sg_stat(path *byte, st *Stat_t) int32

//export sg_fstat
func sg_fstat(fd int32, st *Stat_t) int32

//export sg_getdents
func sg_getdents(fd int32, buf *byte, n uint32) int32

//export sg_mkdir
func sg_mkdir(path *byte, mode uint32) int32

//export sg_rmdir
func sg_rmdir(path *byte) int32

//export sg_unlink
func sg_unlink(path *byte) int32

//export sg_rename
func sg_rename(from, to *byte) int32

//export sg_chdir
func sg_chdir(path *byte) int32

//export sg_getcwd
func sg_getcwd(buf *byte, n uint32) int32

//export sg_getpid
func sg_getpid() int32

//export sg_mmap
func sg_mmap(length uint32, fd int32, off uint32) unsafe.Pointer

//export sg_munmap
func sg_munmap(addr unsafe.Pointer, length uint32) int32

// ---- ficheros ----

func Open(path string, flag int, mode uint32) (fd int, err error) {
	data := cstring(path)
	fd = int(sg_open(&data[0], int32(flag), mode))
	if fd < 0 {
		err = getErrno()
	}
	return
}

func Close(fd int) (err error) {
	if sg_close(int32(fd)) < 0 {
		err = getErrno()
	}
	return
}

func Read(fd int, p []byte) (n int, err error) {
	n = int(sg_read(int32(fd), unsafe.SliceData(p), uint32(len(p))))
	if n < 0 {
		n = 0
		err = getErrno()
	}
	return
}

func Write(fd int, p []byte) (n int, err error) {
	n = int(sg_write(int32(fd), unsafe.SliceData(p), uint32(len(p))))
	if n < 0 {
		n = 0
		err = getErrno()
	}
	return
}

func Pread(fd int, p []byte, offset int64) (n int, err error) {
	n = int(sg_pread(int32(fd), unsafe.SliceData(p), uint32(len(p)), offset))
	if n < 0 {
		n = 0
		err = getErrno()
	}
	return
}

func Pwrite(fd int, p []byte, offset int64) (n int, err error) {
	n = int(sg_pwrite(int32(fd), unsafe.SliceData(p), uint32(len(p)), offset))
	if n < 0 {
		n = 0
		err = getErrno()
	}
	return
}

func Seek(fd int, offset int64, whence int) (newoffset int64, err error) {
	newoffset = sg_lseek(int32(fd), offset, int32(whence))
	if newoffset < 0 {
		err = getErrno()
	}
	return
}

func Fsync(fd int) (err error) {
	if sg_fsync(int32(fd)) < 0 {
		err = getErrno()
	}
	return
}

func Ftruncate(fd int, length int64) (err error) {
	if sg_ftruncate(int32(fd), length) < 0 {
		err = getErrno()
	}
	return
}

func Truncate(path string, length int64) (err error) {
	data := cstring(path)
	if sg_truncate(&data[0], length) < 0 {
		err = getErrno()
	}
	return
}

func Stat(path string, st *Stat_t) (err error) {
	data := cstring(path)
	if sg_stat(&data[0], st) < 0 {
		err = getErrno()
	}
	return
}

// SaguiOS no tiene enlaces simbólicos: Lstat es igual que Stat.
func Lstat(path string, st *Stat_t) (err error) { return Stat(path, st) }

func Fstat(fd int, st *Stat_t) (err error) {
	if sg_fstat(int32(fd), st) < 0 {
		err = getErrno()
	}
	return
}

// ReadDirent llena buf con registros Dirent (como getdents64 de Linux).
func ReadDirent(fd int, buf []byte) (n int, err error) {
	n = int(sg_getdents(int32(fd), unsafe.SliceData(buf), uint32(len(buf))))
	if n < 0 {
		n = 0
		err = getErrno()
	}
	return
}

func Mkdir(path string, mode uint32) (err error) {
	data := cstring(path)
	if sg_mkdir(&data[0], mode) < 0 {
		err = getErrno()
	}
	return
}

func Rmdir(path string) (err error) {
	data := cstring(path)
	if sg_rmdir(&data[0]) < 0 {
		err = getErrno()
	}
	return
}

func Unlink(path string) (err error) {
	data := cstring(path)
	if sg_unlink(&data[0]) < 0 {
		err = getErrno()
	}
	return
}

func Rename(from, to string) (err error) {
	a := cstring(from)
	b := cstring(to)
	if sg_rename(&a[0], &b[0]) < 0 {
		err = getErrno()
	}
	return
}

func Chdir(path string) (err error) {
	data := cstring(path)
	if sg_chdir(&data[0]) < 0 {
		err = getErrno()
	}
	return
}

func Fchdir(fd int) (err error) { return ENOSYS }

func Getwd() (string, error) {
	var buf [1024]byte
	n := sg_getcwd(&buf[0], uint32(len(buf)))
	if n < 0 {
		return "", getErrno()
	}
	return string(buf[:clen(buf[:])]), nil
}

// Permisos y propietarios: SaguiOS tiene su propio modelo (SIS_PERMISOS_ARQ);
// por ahora se aceptan sin cambiar nada.
func Chmod(path string, mode uint32) (err error) {
	var st Stat_t
	return Stat(path, &st)
}

func Fchmod(fd int, mode uint32) (err error)         { return nil }
func Chown(path string, uid, gid int) (err error)    { return nil }
func Lchown(path string, uid, gid int) (err error)   { return nil }
func Fchown(fd int, uid, gid int) (err error)        { return nil }

func Link(oldname, newname string) (err error)   { return ENOSYS }
func Symlink(from, to string) (err error)        { return ENOSYS }
func Readlink(path string, p []byte) (n int, err error) { return 0, EINVAL }
func Faccessat(dirfd int, path string, mode uint32, flags int) (err error) {
	var st Stat_t
	return Stat(path, &st)
}

func Dup(fd int) (fd2 int, err error) { return -1, ENOSYS }
func Pipe2(p []int, flags int) (err error) { return ENOSYS }
func Getpagesize() int { return 4096 }

// ---- procesos: sin fork ni señales ----

func Getuid() int  { return 0 }
func Geteuid() int { return 0 }
func Getgid() int  { return 0 }
func Getegid() int { return 0 }
func Getpid() int  { return int(sg_getpid()) }
func Getppid() int { return 1 }

func Fork() (err error) { return ENOSYS }
func Execve(pathname string, argv []string, envv []string) (err error) { return ENOSYS }
func Kill(pid int, sig Signal) (err error) { return ENOSYS }
func Wait4(pid int, wstatus *WaitStatus, options int, rusage uintptr) (wpid int, err error) {
	return 0, ENOSYS
}
func Syscall(trap, a1, a2, a3 uintptr) (r1, r2 uintptr, err Errno) { return 0, 0, ENOSYS }
func Syscall6(trap, a1, a2, a3, a4, a5, a6 uintptr) (r1, r2 uintptr, err Errno) {
	return 0, 0, ENOSYS
}

func Statfs(path string, buf *Statfs_t) (err error) { return ENOSYS }
func Fstatfs(fd int, buf *Statfs_t) (err error)     { return ENOSYS }

// ---- memoria ----

func Mmap(fd int, offset int64, length int, prot int, flags int) (data []byte, err error) {
	var f int32 = -1
	if flags&MAP_ANON == 0 {
		f = int32(fd)
	}
	addr := sg_mmap(uint32(length), f, uint32(offset))
	if addr == nil {
		return nil, getErrno()
	}
	return unsafe.Slice((*byte)(addr), length), nil
}

func Munmap(b []byte) (err error) {
	if sg_munmap(unsafe.Pointer(unsafe.SliceData(b)), uint32(len(b))) != 0 {
		err = getErrno()
	}
	return
}

func Mprotect(b []byte, prot int) (err error) { return nil }
