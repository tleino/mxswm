#include "mxswm.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>

int
listen_ctlsocket()
{
	char *home;
	struct stat sb;
	struct sockaddr_un addr;
	int fd;

	home = getenv("HOME");
	if (home == NULL)
		home = "/";

	memset(&addr, '\0', sizeof(addr));

	if (snprintf(addr.sun_path, sizeof(addr.sun_path),
	    "%s/.mxswm_socket", home) >= sizeof(addr.sun_path)) {
		warnx("socket path truncated");
		return -1;
	}

	if (stat(addr.sun_path, &sb) == 0) {
		if (!S_ISSOCK(sb.st_mode)) {
			warnx("%s is not socket", addr.sun_path);
			return -1;
		}
		warnx("removing stale socket %s", addr.sun_path);
		if (unlink(addr.sun_path) == -1) {
			warn("unlink %s", addr.sun_path);
			return -1;
		}
	}

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		warn("socket");
		return -1;
	}

	addr.sun_family = AF_UNIX;
	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		warn("bind");
		close(fd);
		return -1;
	}

	if (listen(fd, 128) == -1) {
		warn("listen");
		close(fd);
		return -1;
	}

	return fd;
}

void
run_ctlsocket_event_loop(int ctlfd)
{
	int clientfd[128];
	char buf[4096];
	fd_set rfds;
	int i, nready, nclients, j;
	ssize_t n;
	int fd;
	int maxfd;
	int xfd;
	Display *dpy;
	XEvent event;

	dpy = display();

	XSync(dpy, False);
	xfd = ConnectionNumber(dpy);
	nclients = 0;
	for (;;) {
		maxfd = 0;
		FD_ZERO(&rfds);
		if (xfd > maxfd)
			maxfd = xfd;
		FD_SET(xfd, &rfds);
		if (ctlfd > maxfd)
			maxfd = ctlfd;
		FD_SET(ctlfd, &rfds);
		for (i = 0; i < nclients; i++) {
			if (clientfd[i] > maxfd)
				maxfd = clientfd[i];
			FD_SET(clientfd[i], &rfds);
		}

		nready = select(maxfd + 1, &rfds, NULL, NULL, NULL);
		if (nready == -1 || nready == 0)
			err(1, "select");

		for (i = 0; i < nready; i++) {
			if (FD_ISSET(xfd, &rfds)) {
				j = XPending(dpy) + 1;
				while (j--) {
					XNextEvent(dpy, &event);
					handle_event(&event);
					XSync(dpy, False);
				}
			} else if (FD_ISSET(ctlfd, &rfds)) {
				fd = accept(ctlfd, NULL, NULL);
				if (nclients == 128)
					close(fd);
				else if (fd != -1)
					clientfd[nclients++] = fd;
				else
					warn("accept");
			} else for (j = 0; j < nclients; j++) {
				if (FD_ISSET(clientfd[j],
				    &rfds)) {
					n = read(clientfd[j], buf,
					    sizeof(buf));
					if (n == -1)
						err(1, "read");
					else if (n == 0) {
						nclients--;
						close(clientfd[j]);
						memmove(
						    &clientfd[i],
						    &clientfd[i+1],
						    (nclients - j) *
						    sizeof(int));
					} else {
						buf[n] = '\0';
						run_ctl_line(buf);
						XSync(dpy, False);
					}
				}
			}
		}
	}
}
