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

#include <limits.h>
#include <sys/ioctl.h>

/* Useful macros */
/* Maximum number for 64 bit integer is a string with 21 digits: 2^64 - 1 = 21 */
#define LXC_NUMSTRLEN64 21
#define LXC_LINELEN 4096
#define LXC_IDMAPLEN 4096


///////////////////////////////////////// defined in initUtils.c
extern void remove_trailing_slashes(char *p);
extern void lxc_setup_fs(void);
extern int setproctitle(char *title);


///////////////////////////////////// from utils.h


/* some simple array manipulation utilities */
typedef void (*lxc_free_fn)(void *);

int wait_for_pid(pid_t pid);
extern int lxc_read_from_file(const char *filename, void* buf, size_t count);
extern char **lxc_string_split(const char *string, char sep);
extern char **lxc_string_split_and_trim(const char *string, char sep);
extern char **lxc_string_split_quoted(char *string);
extern size_t lxc_array_len(void **array);
extern void lxc_free_array(void **array, lxc_free_fn element_free_fn);
extern char *lxc_append_paths(const char *first, const char *second);
extern bool dir_exists(const char *path);
extern char *get_rundir(void);
extern int is_dir(const char *path);
extern char *get_template_path(const char *t);
extern int mkdir_p(const char *dir, mode_t mode);
extern bool file_exists(const char *f);
extern bool switch_to_ns(pid_t pid, const char *ns);
extern int lxc_grow_array(void ***array, size_t *capacity, size_t new_size,
			  size_t capacity_increment);
extern int null_stdfds(void);
extern int set_stdfds(int fd);
extern int open_devnull(void);

/* Helper functions to parse numbers. */
extern int lxc_safe_uint(const char *numstr, unsigned int *converted);
extern int lxc_safe_int(const char *numstr, int *converted);
extern int lxc_safe_long(const char *numstr, long int *converted);

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

/* Normalize and split path: Leading and trailing / are removed, multiple
 * / are compactified, .. and . are resolved (.. on the top level is considered
 * identical to .).
 * Examples:
 *     /            ->   { NULL }
 *     foo/../bar   ->   { bar, NULL }
 *     ../../       ->   { NULL }
 *     ./bar/baz/.. ->   { bar, NULL }
 *     foo//bar     ->   { foo, bar, NULL }
 */
extern char **lxc_normalize_path(const char *path);

/*
 * wait on a child we forked
 */
extern int lxc_wait_for_pid_status(pid_t pid);

/* __typeof__ should be safe to use with all compilers. */
typedef __typeof__(((struct statfs *)NULL)->f_type) fs_type_magic;
extern bool has_fs_type(const char *path, fs_type_magic magic_val);
extern bool is_fs_type(const struct statfs *fs, fs_type_magic magic_val);

#define FNV1A_64_INIT ((uint64_t)0xcbf29ce484222325ULL)
extern uint64_t fnv_64a_buf(void *buf, size_t len, uint64_t hval);


#endif /* __LXC_UTILS_H */

