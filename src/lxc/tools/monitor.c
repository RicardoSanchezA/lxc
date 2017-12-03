#include <poll.h>

#include "tools/monitor.h"

/* From liblxc's monitor.c */
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


/* From liblxc's state.c  */
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

