// SPDX-License-Identifier: BSD-3-Clause
// Parte derivada de TinyGo y de Go (Copyright The Go Authors / The TinyGo
// Authors, BSD-3-Clause; ver docs/terceros.md). La parte de SaguiOS es el
// texto de los errores y el método Is.

package syscall

import (
	"internal/itoa"
	"internal/oserror"
	"errors"
)

// Errno es un código de error de POSIX (los valores de Linux, que son los
// que usa la libc de SaguiOS, ver userland/libc/errno.h).
type Errno uintptr

// Mensajes de los errores más comunes (los mismos textos que Go en Linux).
var mensajes = map[Errno]string{
	EPERM:        "operation not permitted",
	ENOENT:       "no such file or directory",
	EINTR:        "interrupted system call",
	EIO:          "input/output error",
	EBADF:        "bad file descriptor",
	EAGAIN:       "resource temporarily unavailable",
	ENOMEM:       "cannot allocate memory",
	EACCES:       "permission denied",
	EFAULT:       "bad address",
	EBUSY:        "device or resource busy",
	EEXIST:       "file exists",
	ENOTDIR:      "not a directory",
	EISDIR:       "is a directory",
	EINVAL:       "invalid argument",
	EMFILE:       "too many open files",
	ENOTTY:       "inappropriate ioctl for device",
	ENOSPC:       "no space left on device",
	ESPIPE:       "illegal seek",
	EROFS:        "read-only file system",
	EPIPE:        "broken pipe",
	ENAMETOOLONG: "file name too long",
	ENOSYS:       "function not implemented",
	ENOTEMPTY:    "directory not empty",
	ENOTSUP:      "operation not supported",
	ECONNRESET:   "connection reset by peer",
	ETIMEDOUT:    "connection timed out",
	ECONNREFUSED: "connection refused",
	EHOSTUNREACH: "no route to host",
	EINPROGRESS:  "operation now in progress",
	EADDRINUSE:   "address already in use",
	ENOTCONN:     "transport endpoint is not connected",
}

func (e Errno) Error() string {
	if s, ok := mensajes[e]; ok {
		return s
	}
	return "errno " + itoa.Itoa(int(e))
}

func (e Errno) Temporary() bool {
	return e == EINTR || e == EMFILE || e.Timeout()
}

func (e Errno) Timeout() bool {
	return e == EAGAIN || e == EWOULDBLOCK || e == ETIMEDOUT
}

// Is hace que errors.Is(err, fs.ErrNotExist) y compañía funcionen.
func (e Errno) Is(target error) bool {
	switch target {
	case oserror.ErrPermission:
		return e == EACCES || e == EPERM
	case oserror.ErrExist:
		return e == EEXIST || e == ENOTEMPTY
	case oserror.ErrNotExist:
		return e == ENOENT
	case errors.ErrUnsupported:
		return e == ENOSYS || e == ENOTSUP || e == EOPNOTSUPP
	}
	return false
}
