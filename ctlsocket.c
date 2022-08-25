/*
 * ISC License
 *
 * Copyright (c) 2021, Tommi Leino <namhas@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

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
#include <sys/wait.h>

static void process_xevents(void);
static void process_ctl_client(int);
static void accept_ctl_client(int);

static int clientfd[128];
static int nclients;

int running;

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

static void
process_ctl_client(int j)
{
	ssize_t n;
	static char buf[4096];
	Display *dpy = display();

	TRACE_LOG("");
	n = read(clientfd[j], buf, sizeof(buf));
	if (n == -1 || n == 0) {
		if (n == -1)
			warn("read");
		nclients--;
		close(clientfd[j]);
		memmove(&clientfd[j], &clientfd[j+1],
		    (nclients - j) * sizeof(int));
		TRACE_LOG("remove\n");
	} else {
		buf[n] = '\0';
		run_ctl_line(buf);
		TRACE_LOG("flush\n");
		XFlush(dpy);
	}
}

static void
accept_ctl_client(int ctlfd)
{
	int fd;

	TRACE_LOG("");
	fd = accept(ctlfd, NULL, NULL);
	if (nclients == 128) {
		warn("maximum ctl clients");
		close(fd);
	} else if (fd != -1) {
		clientfd[nclients++] = fd;
		TRACE_LOG("add ctl client\n");
	} else
		warn("accept");
}

static void
process_xevents()
{
	XEvent event;
	Display *dpy = display();

	while (XPending(dpy)) {
		XNextEvent(dpy, &event);
		handle_event(&event);
	}
	TRACE_LOG("flush\n");
	XFlush(dpy);
}

void
run_ctlsocket_event_loop(int ctlfd)
{
	fd_set rfds;
	int nready, i, j;
	int maxfd;
	int xfd;
	int status;
	Display *dpy;

	dpy = display();
	xfd = ConnectionNumber(dpy);
	nclients = 0;

	process_xevents();
	running = 1;
	while (running) {
#ifndef WAIT_MYPGRP
#define WAIT_MYPGRP 0
#endif
		(void) waitpid(WAIT_MYPGRP, &status, WNOHANG);

		FD_ZERO(&rfds);
		FD_SET(xfd, &rfds);
		maxfd = xfd;
		FD_SET(ctlfd, &rfds);
		if (ctlfd > maxfd)
			maxfd = ctlfd;
		for (i = 0; i < nclients; i++) {
			FD_SET(clientfd[i], &rfds);
			if (clientfd[i] > maxfd)
				maxfd = clientfd[i];
		}

		nready = select(maxfd + 1, &rfds, NULL, NULL, NULL);
		if (nready == -1 || nready == 0)
			err(1, "select");

		for (i = 0; i < nready; i++) {
			if (FD_ISSET(xfd, &rfds))
				process_xevents();
			else if (FD_ISSET(ctlfd, &rfds))
				accept_ctl_client(ctlfd);
			else for (j = 0; j < nclients; j++)
				if (FD_ISSET(clientfd[j], &rfds))
					process_ctl_client(j);
		}
	}
}
