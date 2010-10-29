#ifdef UWSGI_ASYNC

#include "uwsgi.h"

extern struct uwsgi_server uwsgi;


#ifdef __linux__

#include <sys/epoll.h>

int async_queue_init(int serverfd) {
	int epfd;
	struct epoll_event ee;

	epfd = epoll_create(256);

	if (epfd < 0) {
		uwsgi_error("epoll_create()");
		return -1;
	}

	memset(&ee, 0, sizeof(struct epoll_event));
	ee.events = EPOLLIN;
	ee.data.fd = serverfd;

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, serverfd, &ee)) {
		uwsgi_error("epoll_ctl()");
		close(epfd);
		return -1;
	}

	return epfd;
}

int async_wait(int queuefd, void *events, int nevents, int block, int timeout) {

	int ret;

	if (timeout <= 0) {
		timeout = block;
	}
	else {
		timeout = timeout*1000;
	}

	//uwsgi_log("waiting with timeout %d nevents %d\n", timeout, nevents);
	ret = epoll_wait(queuefd, (struct epoll_event *) events, nevents, timeout);
	if (ret < 0) {
		uwsgi_error("epoll_wait()");
	}
	return ret;
}

int async_add(int queuefd, int fd, int etype) {
	struct epoll_event ee;

	memset(&ee, 0, sizeof(struct epoll_event));
	ee.events = etype;
	ee.data.fd = fd;

	if (epoll_ctl(queuefd, EPOLL_CTL_ADD, fd, &ee)) {
		uwsgi_error("epoll_ctl()");
		return -1;
	}

	return 0;
}

int async_mod(int queuefd, int fd, int etype) {
	struct epoll_event ee;

	memset(&ee, 0, sizeof(struct epoll_event));
	ee.events = etype;
	ee.data.fd = fd;

	if (epoll_ctl(queuefd, EPOLL_CTL_MOD, fd, &ee)) {
		uwsgi_error("epoll_ctl()");
		return -1;
	}

	return 0;
}

int async_del(int queuefd, int fd, int etype) {
	struct epoll_event ee;

	memset(&ee, 0, sizeof(struct epoll_event));
	ee.events = etype;
	ee.data.fd = fd;

	if (epoll_ctl(queuefd, EPOLL_CTL_DEL, fd, &ee)) {
		uwsgi_error("epoll_ctl()");
		return -1;
	}

	return 0;
}

#elif defined(__sun__)

int async_queue_init(int serverfd) {
	int dpfd;
	struct pollfd dpev;


	dpfd = open("/dev/poll", O_RDWR);

	if (dpfd < 0) {
		uwsgi_error("open()");
		return -1;
	}


	dpev.fd = serverfd;
	dpev.events = POLLIN;
	dpev.revents = 0;

	if (write(dpfd, &dpev, sizeof(struct pollfd)) < 0) {
		uwsgi_error("write()");
		return -1;
	}


	return dpfd;
}

int async_wait(int queuefd, void *events, int nevents, int block, int timeout) {

	int ret;
	struct dvpoll dv;

	if (timeout <= 0) {
		timeout = block;
	}
	else {
		timeout = timeout*1000;
	}

	dv.dp_fds = (struct pollfd *) events;
	dv.dp_nfds = nevents;
	dv.dp_timeout = timeout;

	//uwsgi_log("waiting with timeout %d nevents %d\n", timeout, nevents);
	ret = ioctl(queuefd, DP_POLL, &dv);
	if (ret < 0) {
		uwsgi_error("ioctl()");
	}
	return ret;
}

int async_add(int queuefd, int fd, int etype) {
	struct pollfd pl;

	pl.fd = fd;
	pl.events = etype;
	pl.revents = 0;

	if (write(queuefd, &pl, sizeof(struct pollfd)) < 0) {
		uwsgi_error("write()");
		return -1;
	}

	return 0;
}

int async_mod(int queuefd, int fd, int etype) {
	// using the same fd will overwrite existing rule
	return async_add(queuefd, fd, etype);
}

int async_del(int queuefd, int fd, int etype) {
	// use POLLREMOVE to remove an fd
	return async_add(queuefd, fd, POLLREMOVE);
}

#else
int async_queue_init(int serverfd) {
	int kfd;
	struct kevent kev;

	kfd = kqueue();

	if (kfd < 0) {
		uwsgi_error("kqueue()");
		return -1;
	}

	EV_SET(&kev, serverfd, EVFILT_READ, EV_ADD, 0, 0, 0);
	if (kevent(kfd, &kev, 1, NULL, 0, NULL) < 0) {
		uwsgi_error("kevent()");
		return -1;
	}

	return kfd;
}

int async_wait(int queuefd, void *events, int nevents, int block, int timeout) {

	int ret;
	struct timespec ts;


	if (timeout <= 0) {
		if (!block) {
			memset(&ts, 0, sizeof(struct timespec));
			ts.tv_sec = timeout;
			ret = kevent(queuefd, NULL, 0, events, nevents, &ts);
		}
		else {
			ret = kevent(queuefd, NULL, 0, events, nevents, NULL);
		}
	}
	else {
		memset(&ts, 0, sizeof(struct timespec));
		ts.tv_sec = timeout;
		ret = kevent(queuefd, NULL, 0, events, nevents, &ts);
	}

	if (ret < 0) {
		uwsgi_error("kevent()");
	}

	return ret;
}

int async_add(int queuefd, int fd, int etype) {
	struct kevent kev;


	EV_SET(&kev, fd, etype, EV_ADD, 0, 0, 0);
	if (kevent(queuefd, &kev, 1, NULL, 0, NULL) < 0) {
		uwsgi_error("kevent()");
		return -1;
	}
	return 0;
}

int async_mod(int queuefd, int fd, int etype) {
	struct kevent kev;

	EV_SET(&kev, fd, ASYNC_OUT, EV_DISABLE, 0, 0, 0);
	if (kevent(queuefd, &kev, 1, NULL, 0, NULL) < 0) {
		uwsgi_error("kevent()");
		return -1;
	}

	EV_SET(&kev, fd, etype, EV_ADD, 0, 0, 0);
	if (kevent(queuefd, &kev, 1, NULL, 0, NULL) < 0) {
		uwsgi_error("kevent()");
		return -1;
	}
	return 0;
}

int async_del(int queuefd, int fd, int etype) {
	struct kevent kev;

	EV_SET(&kev, fd, etype, EV_DELETE, 0, 0, 0);
	if (kevent(queuefd, &kev, 1, NULL, 0, NULL) < 0) {
		uwsgi_error("kevent()");
		return -1;
	}

	return 0;
}

#endif

int async_get_timeout() {


	struct wsgi_request* wsgi_req = uwsgi.wsgi_requests[0];
	int i;
	time_t curtime, tdelta = 0;
	int ret = 0;

	if (!uwsgi.async_running) return 0;

	for(i=0;i<uwsgi.async;i++) {
		if (wsgi_req->async_status == UWSGI_AGAIN) {
			if (wsgi_req->async_timeout_expired) {
				return 0;
			}
			if (wsgi_req->async_timeout > 0) {
				if (tdelta <= 0 || tdelta > wsgi_req->async_timeout) {
					tdelta = wsgi_req->async_timeout;
				}
			}
		}
		wsgi_req = uwsgi.wsgi_requests[i+1];
	}

	curtime = time(NULL);

	ret = tdelta - curtime;
	if (ret > 0) {
		return ret;
	}

	return 0;
}

void async_expire_timeouts() {

	struct wsgi_request* wsgi_req = uwsgi.wsgi_requests[0];
	int i;
	time_t deadline = time(NULL);


	for(i=0;i<uwsgi.async;i++) {
		if (wsgi_req->async_status == UWSGI_AGAIN && wsgi_req->async_timeout > 0) {
			if (wsgi_req->async_timeout <= deadline) {
				wsgi_req->async_timeout = 0;
				wsgi_req->async_timeout_expired = 1;
			}
		}
		wsgi_req = uwsgi.wsgi_requests[i+1];
	}
}

struct wsgi_request *find_first_available_wsgi_req() {

	struct wsgi_request* wsgi_req = uwsgi.wsgi_requests[0];
	int i;

	for(i=0;i<uwsgi.async;i++) {
		if (wsgi_req->async_status == UWSGI_OK) {
			return wsgi_req;
		}
		wsgi_req = uwsgi.wsgi_requests[i+1];
	}

	return NULL;
}

struct wsgi_request *find_wsgi_req_by_fd(int fd, int etype) {

	struct wsgi_request* wsgi_req = uwsgi.wsgi_requests[0];
	int i;

	if (etype != -1) {
		for(i=0;i<uwsgi.async;i++) {
			if (wsgi_req->async_waiting_fd == fd && wsgi_req->async_waiting_fd_type == etype) {
				return wsgi_req;
			}
			wsgi_req = uwsgi.wsgi_requests[i+1];
		}
	}
	else {
		for(i=0;i<uwsgi.async;i++) {
			if (wsgi_req->async_waiting_fd == fd) {
				return wsgi_req;
			}
			wsgi_req = uwsgi.wsgi_requests[i+1];
		}
	}

	return NULL;

}

void async_set_timeout(struct wsgi_request *wsgi_req, time_t timeout) {

	wsgi_req->async_timeout = time(NULL);
	wsgi_req->async_timeout += timeout;
	wsgi_req->async_timeout_expired = 0;

}

void async_write_all(char *data, size_t len) {

	struct wsgi_request *wsgi_req = uwsgi.wsgi_requests[0];
	int i;
	ssize_t rlen;

	for(i=0;i<uwsgi.async;i++) {
		if (wsgi_req->async_status == UWSGI_PAUSED) {
			rlen = write(wsgi_req->poll.fd, data, len);
			if (rlen < 0) {
				uwsgi_error("write()");
			}
			else {
				wsgi_req->response_size += rlen;
			}
		}
		wsgi_req = uwsgi.wsgi_requests[i+1];
	}
}

void async_unpause_all() {

	struct wsgi_request *wsgi_req = uwsgi.wsgi_requests[0];
	int i;

	for(i=0;i<uwsgi.async;i++) {
		if (wsgi_req->async_status == UWSGI_PAUSED) {
			wsgi_req->async_status = UWSGI_AGAIN;
		}
		wsgi_req = uwsgi.wsgi_requests[i+1];
	}
}

struct wsgi_request * async_loop() {

	struct wsgi_request *wsgi_req;
	int i;

	uwsgi.async_running = -1;
	wsgi_req = uwsgi.wsgi_requests[0];


	for(i=0;i<uwsgi.async;i++) {
		if (wsgi_req->async_status == UWSGI_AGAIN) {
			if (wsgi_req->async_waiting_fd != -1 && !wsgi_req->async_waiting_fd_monitored) {
				// add fd to monitoring
				if (async_add(uwsgi.async_queue, wsgi_req->async_waiting_fd, wsgi_req->async_waiting_fd_type)) {
					// error adding fd to the async queue, better to close it...
					close(wsgi_req->async_waiting_fd);
					wsgi_req->async_status = UWSGI_OK;
					return wsgi_req;
				}
				wsgi_req->async_waiting_fd_monitored = 1;
				wsgi_req->async_status = UWSGI_AGAIN;
			}
			else if (wsgi_req->async_waiting_fd == -1 && wsgi_req->async_timeout <= 0) {
				uwsgi.async_running = 0;
				// st global wsgi_req for python functions
				uwsgi.wsgi_req = wsgi_req;
				wsgi_req->async_status = uwsgi.p[wsgi_req->uh.modifier1]->request(wsgi_req);

				if (wsgi_req->async_status < UWSGI_AGAIN) {
					return wsgi_req;
				}
			}
		}
		wsgi_req = uwsgi.wsgi_requests[i+1];
	}

	return NULL;

}
#endif
