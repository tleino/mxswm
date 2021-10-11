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

static void do_map(Window);

static void
do_map(Window window)
{
	XWindowAttributes wa;
	Display *dpy = display();

	XGetWindowAttributes(dpy, window, &wa);
	if (wa.override_redirect == True)
		return;
	XMapWindow(dpy, window);
	XSync(dpy, False);

	if (add_client(window, NULL) == NULL)
		warn("add_client");
}

int
handle_event(XEvent *event)
{
	Window window;
	struct client *client;
	struct stack *stack;

	switch (event->type) {
	case ButtonPress:
		window = event->xbutton.window;
		stack = find_stack_xy(event->xbutton.x, event->xbutton.y);

		/*
		 * We are in XGrabButton mode, do the correct thing for
		 * click-to-focus based on the current focus state and
		 * click location.
		 */
		if (stack != NULL && current_stack() != stack) {
			XAllowEvents(display(), SyncPointer, CurrentTime);
			focus_stack(stack);
		} else
			XAllowEvents(display(), ReplayPointer, CurrentTime);

		break;
	case KeyRelease:
	case KeyPress:
		do_keyaction(&(event->xkey));
		break;
	case PropertyNotify:
		window = event->xproperty.window;
		client = have_client(window);
		if (client != NULL) {
			update_client_name(client);
			draw_stack(client->stack);
			draw_menu();
		}
		break;
	case UnmapNotify:
	case DestroyNotify:
		if (event->type == UnmapNotify)
			window = event->xmap.window;
		else
			window = event->xdestroywindow.window;
		client = have_client(window);
		if (client != NULL)
			remove_client(client);
		else
#ifdef TRACE
			warnx("%s of %lx observed without action",
			    event->type == UnmapNotify ? "unmap" : "destroy",
			    window);
#endif
		break;
	case MapRequest:
		do_map(event->xmaprequest.window);
		break;
	}
	return 1;
}
