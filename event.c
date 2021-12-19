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
#include <X11/Xatom.h>

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

time_t
start_time()
{
	static time_t t;

	if (t == 0)
		t = time(NULL);

	return t;
}

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

static int disappear_error;

static int
handle_disappear_error(Display *display, XErrorEvent *event)
{
	TRACE_LOG("*");
	disappear_error = 1;
	return 1;
}

static void
pass_configure(XConfigureRequestEvent *event)
{
	XWindowChanges wc;
	Display *dpy;

	TRACE_LOG("*");

	dpy = display();

	wc.x = event->x;
	wc.y = event->y;
	wc.width = event->width;
	wc.height = event->height;
	wc.border_width = event->border_width;
	wc.stack_mode = Above;
	event->value_mask &= ~CWStackMode;

	XConfigureWindow(dpy, event->window, event->value_mask, &wc); 
}

static int
is_input_only(Window window)
{
	XWindowAttributes wa;
	Display *d = display();
	int input_only;

	input_only = 0;

	XSync(display(), False);
	XSetErrorHandler(handle_disappear_error);

	if (XGetWindowAttributes(d, window, &wa)) {
		input_only = (wa.class == InputOnly);
		TRACE_LOG("input_only: %d", input_only);
		return input_only;
	}

	if (disappear_error) {
		disappear_error = 0;
		return 1;
	}

	XSync(display(), False);
	XSetErrorHandler(None);

	return 0;
}

int
handle_event(XEvent *event)
{
	struct stack *stack;

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
		if (client != NULL) {
			TRACE_LOG("update atom=%lu", event->xproperty.atom);
			switch (event->xproperty.atom) {
			case XA_WM_NAME:
				update_client_name(client);
				draw_stack(client->stack);
				draw_menu();
				break;
			default:
				if (event->xproperty.atom ==
				    wmh[WM_PROTOCOLS]) {
					read_protocols(client);
					draw_stack(client->stack);
					draw_menu();
				} else if (event->xproperty.atom ==
				    wmh[_NET_WM_NAME]) {
					update_client_name(client);
					draw_stack(client->stack);
					draw_menu();
				} else {
					TRACE_LOG("unsupported atom %s",
					    XGetAtomName(display(),
					    event->xproperty.atom));
				}
				break;
			}
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
				focus_client(client, client->stack);
			} else if (client->mapped &&
			    client->stack == current_stack()) {
				focus_client(client, client->stack);
			} else if (client->mapped && client->stack != NULL) {
				draw_stack(client->stack);
			} else if (client->mapped && client->stack == NULL) {
				client->stack = current_stack();
				draw_stack(client->stack);
			} else if (client->mapped == 0) {
				stack = client->stack;
				CLIENT_STACK(client) = NULL;
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
			focus_stack(current_stack());
			client = NULL;
		} else
			TRACE_LOG("ignore");
		break;
	case MapRequest:
		window = event->xmaprequest.window;
		client = have_client(window);
		if (client != NULL) {
			TRACE_LOG("mapping");
			XMapWindow(display(), window);
			XSelectInput(display(), window, PropertyChangeMask);
			read_protocols(client);
			update_client_name(client);
		} else
			TRACE_LOG("ignore");
		break;
	case ConfigureRequest:
		window = event->xconfigurerequest.window;
		client = have_client(window);
		if (client != NULL) {
			TRACE_LOG("got %dx%d+%d+%d",
			    event->xconfigurerequest.width,
			    event->xconfigurerequest.height,
			    event->xconfigurerequest.x,
			    event->xconfigurerequest.y);

			/*
			 * We don't care about the window configuration
			 * before the window is mapped except for mild
			 * optimization reasons, but we cannot optimize
			 * because some clients such as 'xterm' behave
			 * erratically unless the requested configuration
			 * is passed as is.
			 *
			 * In other words, we intercept only whenever
			 * clients try to change window configuration of
			 * mapped windows in which case we simply do
			 * nothing because windows will always be maximized.
			 */
			if (!client->mapped)
				pass_configure(&event->xconfigurerequest);
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
			client = add_client(window, NULL, 0, current_stack(),
			    0);
			if (client == NULL)
				warn("add_client");
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
