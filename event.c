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

static Window window;

#ifdef TRACE
static XEvent *_current_event;

const char *
current_event()
{
	return str_event(_current_event);
}

Window
current_window()
{
	return window;
}
#endif

#ifdef TRACE
const char *
str_event(XEvent *event)
{
	static const struct event_str {
		int type;
		char *name;
	} es[] = {
		{ KeyPress, "KeyPress" },
		{ KeyRelease, "KeyRelease" },
		{ ButtonPress, "ButtonPress" },
		{ ButtonRelease, "ButtonRelease" },
		{ MotionNotify, "MotionNotify" },
		{ EnterNotify, "EnterNotify" },
		{ LeaveNotify, "LeaveNotify" },
		{ FocusIn, "FocusIn" },
		{ FocusOut, "FocusOut" },
		{ KeymapNotify, "KeymapNotify" },
		{ Expose, "Expose" },
		{ GraphicsExpose, "GraphicsExpose" },
		{ NoExpose, "NoExpose" },
		{ VisibilityNotify, "VisibilityNotify" },
		{ CreateNotify, "CreateNotify" },
		{ DestroyNotify, "DestroyNotify" },
		{ UnmapNotify, "UnmapNotify" },
		{ MapNotify, "MapNotify" },
		{ MapRequest, "MapRequest" },
		{ ReparentNotify, "ReparentNotify" },
		{ ConfigureNotify, "ConfigureNotify" },
		{ ConfigureRequest, "ConfigureRequest" },
		{ GravityNotify, "GravityNotify" },
		{ ResizeRequest, "ResizeRequest" },
		{ CirculateNotify, "CirculateNotify" },
		{ CirculateRequest, "CirculateRequest" },
		{ PropertyNotify, "PropertyNotify" },
		{ SelectionClear, "SelectionClear" },
		{ SelectionRequest, "SelectionRequest" },
		{ SelectionNotify, "SelectionNotify" },
		{ ColormapNotify, "ColormapNotify" },
		{ ClientMessage, "ClientMessage" },
		{ MappingNotify, "MappingNotify" },
		{ -1 }
	};
	int i;
	char *s;

	if (event == NULL)
		return "";

	for (i = 0; es[i].type != -1; i++)
		if (es[i].type == event->type)
			break;

	if (es[i].type != -1)
		s = es[i].name;
	else
		s = "<unknown event>";

	return s;
}
#endif

static void
do_map(Window window)
{
	XWindowAttributes wa;
	Display *dpy = display();

	TRACE_LOG("get attributes");
	XGetWindowAttributes(dpy, window, &wa);
	if (wa.override_redirect == True) {
		TRACE_LOG("had override_redirect");
		return;
	}
	TRACE_LOG("map window");
	XMapWindow(dpy, window);

	if (add_client(window, NULL) == NULL)
		warn("add_client");
}

int
handle_event(XEvent *event)
{
	struct client *client;
	struct stack *stack;

#ifdef TRACE
	_current_event = event;
#endif
	window = 0;
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
			TRACE_LOG("focus");
			focus_stack(stack);
		} else {
			XAllowEvents(display(), ReplayPointer, CurrentTime);
			TRACE_LOG("ignore");
		}
		break;
	case KeyRelease:
	case KeyPress:
		do_keyaction(&(event->xkey));
		break;
	case PropertyNotify:
		window = event->xproperty.window;
		client = have_client(window);
		if (client != NULL) {
			TRACE_LOG("update");
			update_client_name(client);
			draw_stack(client->stack);
			draw_menu();
		} else
			TRACE_LOG("ignore");
		break;
	case UnmapNotify:
	case DestroyNotify:
		if (event->type == UnmapNotify)
			window = event->xmap.window;
		else
			window = event->xdestroywindow.window;
		client = have_client(window);
		if (client != NULL) {
			TRACE_LOG("remove");
			remove_client(client);
		} else
			TRACE_LOG("ignore");
		break;
	case MapRequest:
		window = event->xmaprequest.window;
		do_map(window);
		break;
	default:
		TRACE_LOG("unhandled");
		break;
	}
#ifdef TRACE
	_current_event = 0;
	window = 0;
#endif
	return 1;
}
