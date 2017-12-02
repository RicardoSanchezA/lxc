//#include "config.h"

//#define __STDC_FORMAT_MACROS /* Required for PRIu64 to work. */
/*
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <inttypes.h>
#include <libgen.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "log.h"
#include "lxclock.h"
#include "namespace.h"
#include "parse.h"
#include "utils.h"
#ifndef O_PATH
#define O_PATH      010000000
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW  00400000
#endif
lxc_log_define(lxc_utils, lxc);
*/

/*
 * if path is btrfs, tries to remove it and any subvolumes beneath it
 */

#include "tools/utils.h"


////////////////////////////////////////////////////////////////////// from monitor.c
int lxc_monitor_open(const char *lxcpath)
{
	struct sockaddr_un addr;
	int fd;
	size_t retry;
	size_t len;
	int ret = -1;
	int backoff_ms[] = {10, 50, 100};

	if (lxc_monitor_sock_name(lxcpath, &addr) < 0)
		return -1;

	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		ERROR("Failed to create socket: %s.", strerror(errno));
		return -errno;
	}

	len = strlen(&addr.sun_path[1]);
	DEBUG("opening monitor socket %s with len %zu", &addr.sun_path[1], len);
	if (len >= sizeof(addr.sun_path) - 1) {
		errno = ENAMETOOLONG;
		ret = -errno;
		ERROR("name of monitor socket too long (%zu bytes): %s", len, strerror(errno));
		goto on_error;
	}

	for (retry = 0; retry < sizeof(backoff_ms) / sizeof(backoff_ms[0]); retry++) {
		fd = lxc_abstract_unix_connect(addr.sun_path);
		if (fd < 0 || errno != ECONNREFUSED)
			break;
		ERROR("Failed to connect to monitor socket. Retrying in %d ms: %s", backoff_ms[retry], strerror(errno));
		usleep(backoff_ms[retry] * 1000);
	}

	if (fd < 0) {
		ret = -errno;
		ERROR("Failed to connect to monitor socket: %s.", strerror(errno));
		goto on_error;
	}

	return fd;

on_error:
	close(fd);
	return ret;
}

/* Used to spawn a monitord either on startup of a daemon container, or when
 * lxc-monitor starts.
 */
int lxc_monitord_spawn(const char *lxcpath)
{
	int ret;
	int pipefd[2];
	char pipefd_str[LXC_NUMSTRLEN64];
	pid_t pid1, pid2;

	char *const args[] = {
	    LXC_MONITORD_PATH,
	    (char *)lxcpath,
	    pipefd_str,
	    NULL,
	};

	/* double fork to avoid zombies when monitord exits */
	pid1 = fork();
	if (pid1 < 0) {
		SYSERROR("Failed to fork().");
		return -1;
	}

	if (pid1) {
		DEBUG("Going to wait for pid %d.", pid1);
		if (waitpid(pid1, NULL, 0) != pid1)
			return -1;
		DEBUG("Finished waiting on pid %d.", pid1);
		return 0;
	}

	if (pipe(pipefd) < 0) {
		SYSERROR("Failed to create pipe.");
		exit(EXIT_FAILURE);
	}

	pid2 = fork();
	if (pid2 < 0) {
		SYSERROR("Failed to fork().");
		exit(EXIT_FAILURE);
	}

	if (pid2) {
		DEBUG("Trying to sync with child process.");
		char c;
		/* Wait for daemon to create socket. */
		close(pipefd[1]);

		/* Sync with child, we're ignoring the return from read
		 * because regardless if it works or not, either way we've
		 * synced with the child process. the if-empty-statement
		 * construct is to quiet the warn-unused-result warning.
		 */
		if (read(pipefd[0], &c, 1))
			;

		close(pipefd[0]);

		DEBUG("Successfully synced with child process.");
		exit(EXIT_SUCCESS);
	}

	if (setsid() < 0) {
		SYSERROR("Failed to setsid().");
		exit(EXIT_FAILURE);
	}

	lxc_check_inherited(NULL, true, &pipefd[1], 1);
	if (null_stdfds() < 0) {
		SYSERROR("Failed to dup2() standard file descriptors to /dev/null.");
		exit(EXIT_FAILURE);
	}

	close(pipefd[0]);

	ret = snprintf(pipefd_str, LXC_NUMSTRLEN64, "%d", pipefd[1]);
	if (ret < 0 || ret >= LXC_NUMSTRLEN64) {
		ERROR("Failed to create pid argument to pass to monitord.");
		exit(EXIT_FAILURE);
	}

	DEBUG("Using pipe file descriptor %d for monitord.", pipefd[1]);

	execvp(args[0], args);
	SYSERROR("failed to exec lxc-monitord");

	exit(EXIT_FAILURE);
}

int lxc_monitor_read_fdset(struct pollfd *fds, nfds_t nfds, struct lxc_msg *msg,
			   int timeout)
{
	long i;
	int ret;

	ret = poll(fds, nfds, timeout * 1000);
	if (ret == -1)
		return -1;
	else if (ret == 0)
		return -2;  /* timed out */

	/* Only read from the first ready fd, the others will remain ready for
	 * when this routine is called again.
	 */
	for (i = 0; i < nfds; i++) {
		if (fds[i].revents != 0) {
			fds[i].revents = 0;
			ret = recv(fds[i].fd, msg, sizeof(*msg), 0);
			if (ret <= 0) {
				SYSERROR("Failed to receive message. Did monitord die?: %s.", strerror(errno));
				return -1;
			}
			return ret;
		}
	}

	SYSERROR("No ready fd found.");

	return -1;
}


////////////////////////////////////////////////////////////////// from state.c
static const char * const strstate[] = {
	"STOPPED", "STARTING", "RUNNING", "STOPPING",
	"ABORTING", "FREEZING", "FROZEN", "THAWED",
};

const char *lxc_state2str(lxc_state_t state)
{
	if (state < STOPPED || state > MAX_STATE - 1)
		return NULL;
	return strstate[state];
}

/////////////////////////////////////////////////////// from utils.c
extern void remove_trailing_slashes(char *p)
{
	int l = strlen(p);
	while (--l >= 0 && (p[l] == '/' || p[l] == '\n'))
		p[l] = '\0';
}

int wait_for_pid(pid_t pid)
{
	int status, ret;

again:
	ret = waitpid(pid, &status, 0);
	if (ret == -1) {
		if (errno == EINTR)
			goto again;
		return -1;
	}
	if (ret != pid)
		goto again;
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return -1;
	return 0;
}

int lxc_read_from_file(const char *filename, void* buf, size_t count)
{
	int fd = -1, saved_errno;
	ssize_t ret;

	fd = open(filename, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;

	if (!buf || !count) {
		char buf2[100];
		size_t count2 = 0;
		while ((ret = read(fd, buf2, 100)) > 0)
			count2 += ret;
		if (ret >= 0)
			ret = count2;
	} else {
		memset(buf, 0, count);
		ret = read(fd, buf, count);
	}

	if (ret < 0)
		ERROR("read %s: %s", filename, strerror(errno));

	saved_errno = errno;
	close(fd);
	errno = saved_errno;
	return ret;
}

char **lxc_string_split(const char *string, char _sep)
{
	char *token, *str, *saveptr = NULL;
	char sep[2] = {_sep, '\0'};
	char **tmp = NULL, **result = NULL;
	size_t result_capacity = 0;
	size_t result_count = 0;
	int r, saved_errno;

	if (!string)
		return calloc(1, sizeof(char *));

	str = alloca(strlen(string) + 1);
	strcpy(str, string);
	for (; (token = strtok_r(str, sep, &saveptr)); str = NULL) {
		r = lxc_grow_array((void ***)&result, &result_capacity, result_count + 1, 16);
		if (r < 0)
			goto error_out;
		result[result_count] = strdup(token);
		if (!result[result_count])
			goto error_out;
		result_count++;
	}

	/* if we allocated too much, reduce it */
	tmp = realloc(result, (result_count + 1) * sizeof(char *));
	if (!tmp)
		goto error_out;
	result = tmp;
	/* Make sure we don't return uninitialized memory. */
	if (result_count == 0)
		*result = NULL;
	return result;
error_out:
	saved_errno = errno;
	lxc_free_array((void **)result, free);
	errno = saved_errno;
	return NULL;
}

char **lxc_string_split_and_trim(const char *string, char _sep)
{
	char *token, *str, *saveptr = NULL;
	char sep[2] = { _sep, '\0' };
	char **result = NULL;
	size_t result_capacity = 0;
	size_t result_count = 0;
	int r, saved_errno;
	size_t i = 0;

	if (!string)
		return calloc(1, sizeof(char *));

	str = alloca(strlen(string)+1);
	strcpy(str, string);
	for (; (token = strtok_r(str, sep, &saveptr)); str = NULL) {
		while (token[0] == ' ' || token[0] == '\t')
			token++;
		i = strlen(token);
		while (i > 0 && (token[i - 1] == ' ' || token[i - 1] == '\t')) {
			token[i - 1] = '\0';
			i--;
		}
		r = lxc_grow_array((void ***)&result, &result_capacity, result_count + 1, 16);
		if (r < 0)
			goto error_out;
		result[result_count] = strdup(token);
		if (!result[result_count])
			goto error_out;
		result_count++;
	}

	/* if we allocated too much, reduce it */
	return realloc(result, (result_count + 1) * sizeof(char *));
error_out:
	saved_errno = errno;
	lxc_free_array((void **)result, free);
	errno = saved_errno;
	return NULL;
}

/*
 * Given a a string 'one two "three four"', split into three words,
 * one, two, and "three four"
 */
char **lxc_string_split_quoted(char *string)
{
	char *nextword = string, *p, state;
	char **result = NULL;
	size_t result_capacity = 0;
	size_t result_count = 0;

	if (!string || !*string)
		return calloc(1, sizeof(char *));

	// TODO I'm *not* handling escaped quote
	state = ' ';
	for (p = string; *p; p++) {
		switch(state) {
		case ' ':
			if (isspace(*p))
				continue;
			else if (*p == '"' || *p == '\'') {
				nextword = p;
				state = *p;
				continue;
			}
			nextword = p;
			state = 'a';
			continue;
		case 'a':
			if (isspace(*p)) {
				complete_word(&result, nextword, p, &result_capacity, &result_count);
				state = ' ';
				continue;
			}
			continue;
		case '"':
		case '\'':
			if (*p == state) {
				complete_word(&result, nextword+1, p, &result_capacity, &result_count);
				state = ' ';
				continue;
			}
			continue;
		}
	}

	if (state == 'a')
		complete_word(&result, nextword, p, &result_capacity, &result_count);

	return realloc(result, (result_count + 1) * sizeof(char *));
}


size_t lxc_array_len(void **array)
{
	void **p;
	size_t result = 0;

	for (p = array; p && *p; p++)
		result++;

	return result;
}

void lxc_free_array(void **array, lxc_free_fn element_free_fn)
{
	void **p;
	for (p = array; p && *p; p++)
		element_free_fn(*p);
	free((void*)array);
}

char *lxc_append_paths(const char *first, const char *second)
{
	size_t len = strlen(first) + strlen(second) + 1;
	const char *pattern = "%s%s";
	char *result = NULL;

	if (second[0] != '/') {
		len += 1;
		pattern = "%s/%s";
	}

	result = calloc(1, len);
	if (!result)
		return NULL;

	snprintf(result, len, pattern, first, second);
	return result;
}

int lxc_safe_uint(const char *numstr, unsigned int *converted)
{
	char *err = NULL;
	unsigned long int uli;

	while (isspace(*numstr))
		numstr++;

	if (*numstr == '-')
		return -EINVAL;

	errno = 0;
	uli = strtoul(numstr, &err, 0);
	if (errno == ERANGE && uli == ULONG_MAX)
		return -ERANGE;

	if (err == numstr || *err != '\0')
		return -EINVAL;

	if (uli > UINT_MAX)
		return -ERANGE;

	*converted = (unsigned int)uli;
	return 0;
}

bool dir_exists(const char *path)
{
	struct stat sb;
	int ret;

	ret = stat(path, &sb);
	if (ret < 0)
		/* Could be something other than eexist, just say "no". */
		return false;
	return S_ISDIR(sb.st_mode);
}

char *get_rundir()
{
	char *rundir;
	const char *homedir;

	if (geteuid() == 0) {
		rundir = strdup(RUNTIME_PATH);
		return rundir;
	}

	rundir = getenv("XDG_RUNTIME_DIR");
	if (rundir) {
		rundir = strdup(rundir);
		return rundir;
	}

	INFO("XDG_RUNTIME_DIR isn't set in the environment.");
	homedir = getenv("HOME");
	if (!homedir) {
		ERROR("HOME isn't set in the environment.");
		return NULL;
	}

	rundir = malloc(sizeof(char) * (17 + strlen(homedir)));
	sprintf(rundir, "%s/.cache/lxc/run/", homedir);

	return rundir;
}

int is_dir(const char *path)
{
	struct stat statbuf;
	int ret = stat(path, &statbuf);
	if (ret == 0 && S_ISDIR(statbuf.st_mode))
		return 1;
	return 0;
}

/*
 * Given the '-t' template option to lxc-create, figure out what to
 * do.  If the template is a full executable path, use that.  If it
 * is something like 'sshd', then return $templatepath/lxc-sshd.
 * On success return the template, on error return NULL.
 */
char *get_template_path(const char *t)
{
	int ret, len;
	char *tpath;

	if (t[0] == '/' && access(t, X_OK) == 0) {
		tpath = strdup(t);
		return tpath;
	}

	len = strlen(LXCTEMPLATEDIR) + strlen(t) + strlen("/lxc-") + 1;
	tpath = malloc(len);
	if (!tpath)
		return NULL;
	ret = snprintf(tpath, len, "%s/lxc-%s", LXCTEMPLATEDIR, t);
	if (ret < 0 || ret >= len) {
		free(tpath);
		return NULL;
	}
	if (access(tpath, X_OK) < 0) {
		SYSERROR("bad template: %s", t);
		free(tpath);
		return NULL;
	}

	return tpath;
}

extern int mkdir_p(const char *dir, mode_t mode)
{
	const char *tmp = dir;
	const char *orig = dir;
	char *makeme;

	do {
		dir = tmp + strspn(tmp, "/");
		tmp = dir + strcspn(dir, "/");
		makeme = strndup(orig, dir - orig);
		if (*makeme) {
			if (mkdir(makeme, mode) && errno != EEXIST) {
				SYSERROR("failed to create directory '%s'", makeme);
				free(makeme);
				return -1;
			}
		}
		free(makeme);
	} while(tmp != dir);

	return 0;
}

bool file_exists(const char *f)
{
	struct stat statbuf;

	return stat(f, &statbuf) == 0;
}

bool switch_to_ns(pid_t pid, const char *ns) {
	int fd, ret;
	char nspath[MAXPATHLEN];

	/* Switch to new ns */
	ret = snprintf(nspath, MAXPATHLEN, "/proc/%d/ns/%s", pid, ns);
	if (ret < 0 || ret >= MAXPATHLEN)
		return false;

	fd = open(nspath, O_RDONLY);
	if (fd < 0) {
		SYSERROR("failed to open %s", nspath);
		return false;
	}

	ret = setns(fd, 0);
	if (ret) {
		SYSERROR("failed to set process %d to %s of %d.", pid, ns, fd);
		close(fd);
		return false;
	}
	close(fd);
	return true;
}

int lxc_safe_uint(const char *numstr, unsigned int *converted)
{
	char *err = NULL;
	unsigned long int uli;

	while (isspace(*numstr))
		numstr++;

	if (*numstr == '-')
		return -EINVAL;

	errno = 0;
	uli = strtoul(numstr, &err, 0);
	if (errno == ERANGE && uli == ULONG_MAX)
		return -ERANGE;

	if (err == numstr || *err != '\0')
		return -EINVAL;

	if (uli > UINT_MAX)
		return -ERANGE;

	*converted = (unsigned int)uli;
	return 0;
}

int lxc_safe_long(const char *numstr, long int *converted)
{
	char *err = NULL;
	signed long int sli;

	errno = 0;
	sli = strtol(numstr, &err, 0);
	if (errno == ERANGE && (sli == LONG_MAX || sli == LONG_MIN))
		return -ERANGE;

	if (errno != 0 && sli == 0)
		return -EINVAL;

	if (err == numstr || *err != '\0')
		return -EINVAL;

	*converted = sli;
	return 0;
}

char *lxc_string_replace(const char *needle, const char *replacement, const char *haystack)
{
	ssize_t len = -1, saved_len = -1;
	char *result = NULL;
	size_t replacement_len = strlen(replacement);
	size_t needle_len = strlen(needle);

	/* should be executed exactly twice */
	while (len == -1 || result == NULL) {
		char *p;
		char *last_p;
		ssize_t part_len;

		if (len != -1) {
			result = calloc(1, len + 1);
			if (!result)
				return NULL;
			saved_len = len;
		}

		len = 0;

		for (last_p = (char *)haystack, p = strstr(last_p, needle); p; last_p = p, p = strstr(last_p, needle)) {
			part_len = (ssize_t)(p - last_p);
			if (result && part_len > 0)
				memcpy(&result[len], last_p, part_len);
			len += part_len;
			if (result && replacement_len > 0)
				memcpy(&result[len], replacement, replacement_len);
			len += replacement_len;
			p += needle_len;
		}
		part_len = strlen(last_p);
		if (result && part_len > 0)
			memcpy(&result[len], last_p, part_len);
		len += part_len;
	}

	/* make sure we did the same thing twice,
	 * once for calculating length, the other
	 * time for copying data */
	if (saved_len != len) {
		free(result);
		return NULL;
	}
	/* make sure we didn't overwrite any buffer,
	 * due to calloc the string should be 0-terminated */
	if (result[len] != '\0') {
		free(result);
		return NULL;
	}

	return result;
}

char *lxc_string_join(const char *sep, const char **parts, bool use_as_prefix)
{
	char *result;
	char **p;
	size_t sep_len = strlen(sep);
	size_t result_len = use_as_prefix * sep_len;

	/* calculate new string length */
	for (p = (char **)parts; *p; p++)
		result_len += (p > (char **)parts) * sep_len + strlen(*p);

	result = calloc(result_len + 1, 1);
	if (!result)
		return NULL;

	if (use_as_prefix)
		strcpy(result, sep);
	for (p = (char **)parts; *p; p++) {
		if (p > (char **)parts)
			strcat(result, sep);
		strcat(result, *p);
	}

	return result;
}

ssize_t lxc_write_nointr(int fd, const void* buf, size_t count)
{
	ssize_t ret;
again:
	ret = write(fd, buf, count);
	if (ret < 0 && errno == EINTR)
		goto again;
	return ret;
}

ssize_t lxc_read_nointr(int fd, void* buf, size_t count)
{
	ssize_t ret;
again:
	ret = read(fd, buf, count);
	if (ret < 0 && errno == EINTR)
		goto again;
	return ret;
}

/* returns 0 on success, -1 if there were any failures */
extern int lxc_rmdir_onedev(char *path, const char *exclude)
{
	struct stat mystat;
	bool onedev = true;

	if (is_native_overlayfs(path)) {
		onedev = false;
	}

	if (lstat(path, &mystat) < 0) {
		if (errno == ENOENT)
			return 0;
		ERROR("Failed to stat %s", path);
		return -1;
	}

	return _recursive_rmdir(path, mystat.st_dev, exclude, 0, onedev);
}

char **lxc_normalize_path(const char *path)
{
	char **components;
	char **p;
	size_t components_len = 0;
	size_t pos = 0;

	components = lxc_string_split(path, '/');
	if (!components)
		return NULL;
	for (p = components; *p; p++)
		components_len++;

	/* resolve '.' and '..' */
	for (pos = 0; pos < components_len; ) {
		if (!strcmp(components[pos], ".") || (!strcmp(components[pos], "..") && pos == 0)) {
			/* eat this element */
			free(components[pos]);
			memmove(&components[pos], &components[pos+1], sizeof(char *) * (components_len - pos));
			components_len--;
		} else if (!strcmp(components[pos], "..")) {
			/* eat this and the previous element */
			free(components[pos - 1]);
			free(components[pos]);
			memmove(&components[pos-1], &components[pos+1], sizeof(char *) * (components_len - pos));
			components_len -= 2;
			pos--;
		} else {
			pos++;
		}
	}

	return components;
}

