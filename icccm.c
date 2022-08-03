/*
 * mxswm - MaXed Stacks Window Manager for X11
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

/*
 * icccm.c:
 *   Implements ICCCM support plus some EWMH e.g. Extended Window Manager
 *   Hints a.k.a. NetWM support.
 *
 *   The 'wmh' global a.k.a. Window Manager Hints collects both ICCCM
 *   inter-client communication atoms and EWMH atoms to a single array.
 *
 * ICCCM:
 *   https://www.x.org/releases/X11R7.6/doc/xorg-docs/specs/ICCCM/icccm.html
 *
 * EWMH:
 *   https://specifications.freedesktop.org/wm-spec/wm-spec-latest.html
 */

#include "mxswm.h"

Atom wmh[NUM_WMH];

void
init_wmh()
{
	static char *atoms[] = {
		"WM_PROTOCOLS",
		"_NET_WM_NAME",
		"_NET_WM_VISIBLE_NAME",
		"UTF8_STRING",
	};

	XInternAtoms(display(), atoms, ARRLEN(atoms), False, wmh);
}

#include "mxswm.h"

static void send_message(Atom, Window);

/*
 * Send atom (a) message to a client window (w).
 * Follows ICCCM conventions.
 */
static void
send_message(Atom a, Window w)
{
	XClientMessageEvent e;
	Atom wmp;

	e.type = ClientMessage;
	e.window = w;

	wmp = XInternAtom(display(), "WM_PROTOCOLS", False);
	e.message_type = wmp;
	e.format = 32;
	e.data.l[0] = a;
	e.data.l[1] = current_event_timestamp();

	XSendEvent(display(), w, False, 0, (XEvent *) &e);
}

void
read_protocols(struct client *client)
{
	Atom delwin, takefocus, *protocols = NULL, *ap;
	int n, i;
	Display *dpy = display();

	TRACE_LOG("reading");

	delwin = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	takefocus = XInternAtom(dpy, "WM_TAKE_FOCUS", False);

	if (XGetWMProtocols(dpy, client->window, &protocols, &n)) {
		for (i = 0, ap = protocols; i < n; i++, ap++) {
			if (*ap == delwin)
				client->flags |= CF_HAS_DELWIN;
			if (*ap == takefocus)
				client->flags |= CF_HAS_TAKEFOCUS;
		}
		if (protocols != NULL)
			XFree(protocols);
	} else {
		TRACE_LOG("problem reading");
	}

	TRACE_LOG("flags: %d", client->flags);
}

void
send_delete_window(struct client *client)
{
	send_message(XInternAtom(display(), "WM_DELETE_WINDOW", False),
	    client->window);
}

void
send_take_focus(struct client *client)
{
	send_message(XInternAtom(display(), "WM_TAKE_FOCUS", False),
	    client->window);
}
