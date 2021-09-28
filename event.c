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
#include <err.h>

int
handle_event(XEvent *event)
{
	Window window;
	struct client *client, *current;

	switch (event->type) {
	case KeyRelease:
		do_keyaction(&(event->xkey));
		break;
	case DestroyNotify:
		window = event->xdestroywindow.window;
		client = have_client(window);
		current = current_client();
		if (client != NULL) {
			if (client == current)
				remove_client(client);
			else {
				remove_client(client);
				focus_client(current);
			}
		} else
			warnx("destroy of %lx observed without action",
			    window);
		break;
	case MapRequest:
		window = event->xmaprequest.window;
		if (manageable(window)) {
			client = add_client(window, NULL);
			if (client == NULL)
				warn("add_client");
			else
				focus_client(client);
		} else
			warnx("did not capture %lx", window);
		break;
	}
	return 1;
}
