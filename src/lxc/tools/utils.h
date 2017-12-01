/*
 * lxc: linux Container library
 *
 * (C) Copyright IBM Corp. 2007, 2008
 *
 * Authors:
 * Daniel Lezcano <daniel.lezcano at free.fr>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __TOOLS_UTILS_H
#define __TOOLS_UTILS_H

/* Properly support loop devices on 32bit systems. */
//#define _FILE_OFFSET_BITS 64

/*
#include "config.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <linux/loop.h>
#include <linux/magic.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/vfs.h>

#ifdef HAVE_LINUX_MEMFD_H
#include <linux/memfd.h>
#endif

#include "initutils.h"
*/
#include <stdbool.h>
#include <limits.h>

/////////////////////////////////////////// used in lxc_monitor.c, defined in monitor.h

typedef enum {
	lxc_msg_state,
	lxc_msg_priority,
	lxc_msg_exit_code,
} lxc_msg_type_t;

struct lxc_msg {
	lxc_msg_type_t type;
	char name[NAME_MAX+1];
	int value;
};

/*
 * Open the monitoring mechanism for a specific container
 * The function will return an fd corresponding to the events
 * Returns a file descriptor on success, < 0 otherwise
 */
extern int lxc_monitor_open(const char *lxcpath);

extern int lxc_monitord_spawn(const char *lxcpath);

/*
 * Blocking read from multiple monitors for the next container state
 * change with timeout
 * @fds     : struct pollfd descripting the fds to use
 * @nfds    : the number of entries in fds
 * @msg     : the variable which will be filled with the state
 * @timeout : the timeout in seconds to wait for a state change
 * Returns 0 if the monitored container has exited, > 0 if
 * data was read, < 0 otherwise
 */
extern int lxc_monitor_read_fdset(struct pollfd *fds, nfds_t nfds, struct lxc_msg *msg,
			   int timeout);


/////////////////////////////////////////// used in lxc_monitor.c, defined in state.h
typedef enum {
	STOPPED,
	STARTING,
	RUNNING,
	STOPPING,
	ABORTING,
	FREEZING,
	FROZEN,
	THAWED,
	MAX_STATE,
} lxc_state_t;

// used in lxc_monitor.c, defined in state.h
extern const char *lxc_state2str(lxc_state_t state);

///////////////////////////////////// from utils.h

/* some simple array manipulation utilities */
typedef void (*lxc_free_fn)(void *);

extern void remove_trailing_slashes(char *p);
int wait_for_pid(pid_t pid);
extern int lxc_read_from_file(const char *filename, void* buf, size_t count);
extern char **lxc_string_split_and_trim(const char *string, char sep);
extern size_t lxc_array_len(void **array);
extern void lxc_free_array(void **array, lxc_free_fn element_free_fn);
extern char *lxc_append_paths(const char *first, const char *second);
extern bool dir_exists(const char *path);
extern char *get_rundir(void);

/* Helper functions to parse numbers. */
extern int lxc_safe_uint(const char *numstr, unsigned int *converted);

/* Some simple string functions; if they return pointers, they are allocated
 * buffers.
 */
extern char *lxc_string_replace(const char *needle, const char *replacement,
				const char *haystack);
extern char *lxc_string_join(const char *sep, const char **parts,
			     bool use_as_prefix);

/* send and receive buffers completely */
extern ssize_t lxc_write_nointr(int fd, const void* buf, size_t count);
extern ssize_t lxc_read_nointr(int fd, void* buf, size_t count);

/* returns 1 on success, 0 if there were any failures */
extern int lxc_rmdir_onedev(char *path, const char *exclude);


#endif /* __LXC_UTILS_H */

