
#ifndef __TOOLS_MONITOR_H
#define __TOOLS_MONITOR_H

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

/* From liblxc's state.h */
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

const char *lxc_state2str(lxc_state_t state);


#endif /* __TOOLS_MONITOR_H */
