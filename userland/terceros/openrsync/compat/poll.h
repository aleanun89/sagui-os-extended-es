/* SPDX-License-Identifier: GPL-3.0-only
 * poll.h - poll() sobre pipes y ficheros de SaguiOS (D314). Ver compat_sagui.c. */
#ifndef RSYNC_POLL_H
#define RSYNC_POLL_H
struct pollfd { int fd; short events; short revents; };
#define POLLIN   0x001
#define POLLOUT  0x004
#define POLLERR  0x008
#define POLLHUP  0x010
#define POLLNVAL 0x020
typedef unsigned nfds_t;
int poll(struct pollfd *fds, nfds_t n, int prazo_ms);
#endif
