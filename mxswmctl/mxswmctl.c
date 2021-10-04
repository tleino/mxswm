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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	char *home;
	char buf[256] = { 0 };
	struct stat sb;
	int fd;
	int n;
	struct sockaddr_un addr;

	if (argc >= 2) {
		argc--;
		argv++;
		n = 0;
		while (argc--) {
			n += snprintf(&buf[n], sizeof(buf) - n,
			    "%s ", *argv++);
			if (n >= sizeof(buf))
				errx(1, "argument truncated");
		}
	} else {
		fprintf(stderr, "usage: %s stack NUMBER width NUMBER\n",
		    argv[0]);
		return 1;
	}

	home = getenv("HOME");
	if (home == NULL)
		home = "/";

	memset(&addr, '\0', sizeof(addr));

	if (snprintf(addr.sun_path, sizeof(addr.sun_path),
	    "%s/.mxswm_socket", home) >= sizeof(addr.sun_path))
		errx(1, "socket path truncated");

	if (stat(addr.sun_path, &sb) == -1)
		err(1, "stat %s", addr.sun_path);

	if (!S_ISSOCK(sb.st_mode))
		errx(1, "%s is not socket", addr.sun_path);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1)
		err(1, "socket");

	addr.sun_family = AF_UNIX;
	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
		err(1, "connect");

	if (write(fd, buf, strlen(buf)) <= 0)
		err(1, "write");

	close(fd);
	return 0;
}
