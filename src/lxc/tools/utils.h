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

