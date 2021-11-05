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

static Window window;

static Time timestamp;

static struct client *client;

Time
current_event_timestamp()
{
	return timestamp;
}

#ifdef TRACE
static XEvent *_current_event;

const char *
current_event()
{
	return str_event(_current_event);
}

struct client *
event_client()
{
	return client;
}

void
set_event_client(struct client *_client)
{
	client = _client;
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

static int
is_input_only(Window window)
{
	XWindowAttributes wa;
	Display *d = display();
	int input_only;

	input_only = 0;
	if (XGetWindowAttributes(d, window, &wa)) {
		input_only = (wa.class == InputOnly);
		TRACE_LOG("input_only: %d", input_only);
		return input_only;
	}

	return 0;
}

int
handle_event(XEvent *event)
{
	struct stack *stack;
	Atom a_protocols, a_name, a_u8_name;

#ifdef TRACE
	_current_event = event;
#endif
	timestamp = CurrentTime;
	window = 0;
	switch (event->type) {
	case ButtonPress:
		window = event->xbutton.window;
		timestamp = event->xbutton.time;
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
		timestamp = event->xkey.time;
		do_keyaction(&(event->xkey));
		break;
	case PropertyNotify:
		window = event->xproperty.window;
		client = have_client(window);
		a_protocols = XInternAtom(display(), "WM_PROTOCOLS", False);
		a_name = XInternAtom(display(), "WM_NAME", False);
		a_u8_name = XInternAtom(display(), "_NET_WM_NAME", False);
		if (client != NULL) {
			TRACE_LOG("update atom=%lu", event->xproperty.atom);
			if (event->xproperty.atom == a_protocols) {
				read_protocols(client);
				draw_stack(client->stack);
				draw_menu();
			} else if (event->xproperty.atom == a_name ||
			    event->xproperty.atom == a_u8_name) {
				update_client_name(client);
				draw_stack(client->stack);
				draw_menu();
			} else
				TRACE_LOG("unsupported atom");
		} else
			TRACE_LOG("ignore");
		break;
	case MapNotify:
	case UnmapNotify:
		window = event->xmap.window;
		client = have_client(window);
		if (client != NULL) {
			TRACE_LOG("mapped was %d", client->mapped);
			client->mapped = (event->type == MapNotify) ? 1 : 0;
			TRACE_LOG("mapped is now %d", client->mapped);
			if (client->mapped &&
			    client->flags & CF_FOCUS_WHEN_MAPPED) {
				client->flags &= ~CF_FOCUS_WHEN_MAPPED;
				focus_client(client, current_stack());
			} else if (client->mapped == 0) {
				stack = client->stack;
				CLIENT_STACK(client) = NULL;
				client->flags |= CF_FOCUS_WHEN_MAPPED;
				draw_stack(stack);
				draw_menu();
			}
		} else if ((stack = have_stack(window)) != NULL)
			stack->mapped = (event->type == MapNotify) ? 1 : 0;
		else
			TRACE_LOG("ignore");
		break;
	case DestroyNotify:
		if (event->type == UnmapNotify)
			window = event->xmap.window;
		else
			window = event->xdestroywindow.window;
		client = have_client(window);
		if (client != NULL) {
			TRACE_LOG("remove");
			stack = client->stack;
			remove_client(client);
			draw_stack(stack);
			draw_menu();
			client = NULL;
		} else
			TRACE_LOG("ignore");
		break;
	case ConfigureNotify:
		window = event->xconfigure.window;
		TRACE_LOG("%dx%d+%d+%d", event->xconfigure.width,
		    event->xconfigure.height, event->xconfigure.x,
		    event->xconfigure.y);
		break;
	case CreateNotify:
		window = event->xcreatewindow.window;
		TRACE_LOG("%dx%d+%d+%d bw=%d override=%d",
		    event->xcreatewindow.width, event->xcreatewindow.height,
		    event->xcreatewindow.x, event->xcreatewindow.y,
		    event->xcreatewindow.border_width,
		    event->xcreatewindow.override_redirect);
		if (event->xcreatewindow.override_redirect == True ||
		    is_input_only(window))
			TRACE_LOG("ignore");
		else {
			client = add_client(window, NULL, 0);
			if (client == NULL)
				warn("add_client");
			else
				XSetWindowBorderWidth(display(), window, 0);
			TRACE_LOG("created");
		}
		break;
	default:
		TRACE_LOG("unhandled");
		break;
	}
#ifdef TRACE
	_current_event = 0;
	window = 0;
	client = NULL;
#endif
	return 1;
}
