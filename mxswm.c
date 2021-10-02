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
#include <X11/Xutil.h>
#include <errno.h>
#include <err.h>
#include <stdlib.h>
#include <locale.h>

Display *_display;

static void select_root_events(Display *);
static int wm_rights_error(Display *, XErrorEvent *);
static int read_state(Window);

static void
select_root_events(Display *display)
{
	XSync(display, False);
	XSetErrorHandler(wm_rights_error);

	XSelectInput(display, DefaultRootWindow(display),
	             SubstructureNotifyMask |
	             SubstructureRedirectMask |
	             KeyReleaseMask);

	XSync(display, False);
	XSetErrorHandler(None);
}

static int
wm_rights_error(Display *display, XErrorEvent *event)
{
	errx(1, "couldn't obtain window manager rights, "
	     "is there another running?");
	return 1;
}

static void
capture_existing_windows(Display *display)
{
	Window root, parent, *children;
	int i;
	unsigned int nchildren;

	if (XQueryTree(display, DefaultRootWindow(display),
	               &root, &parent, &children, &nchildren) == 0) {
		warnx("did not capture existing windows");
		return;
	}

	for (i = 0; i < nchildren; i++) {
		if (manageable(children[i])) {
			if (add_client(children[i], NULL) == NULL)
				warn("add_client");
		} else
			warnx("did not capture %lx", children[i]);
	}

	if (nchildren > 0)
		XFree(children);
}

static int
read_state(Window window)
{
	Display *dpy = display();
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop_return = NULL;
	unsigned long data[2];
	Atom wmstate;

	wmstate = XInternAtom(dpy, "WM_STATE", False);
	if (XGetWindowProperty(dpy, window, wmstate, 0L, 2L, False, wmstate,
	    &actual_type, &actual_format, &nitems,
	    &bytes_after, &prop_return) != Success ||
            !prop_return || nitems > 2)
		return NormalState;

	data[0] = prop_return[0];
	XFree(prop_return);

	return data[0];
}

int
manageable(Window w)
{
	XWindowAttributes wa;
	int mapped, redirectable;
	Display *d = display();

	XGetWindowAttributes(d, w, &wa);
	mapped = (wa.map_state != IsUnmapped);
	redirectable = (wa.override_redirect != True);

	/*
	 * We wish to manage iconified windows that are unmapped.
	 */
	if (!mapped)
		mapped = (read_state(w) == IconicState);

	return (mapped && redirectable);
}

static void
open_display()
{
	char *denv;

	denv = getenv("DISPLAY");
	if (denv == NULL && errno != 0)
		err(1, "getenv DISPLAY");
	_display = XOpenDisplay(denv);
	if (_display == NULL) {
		if (denv == NULL)
			errx(1, "X11 connection failed; "
			    "DISPLAY environment variable not set?");
               	else
                       	errx(1, "failed X11 connection to '%s'", denv);
	}
}

Display *
display()
{
	if (_display == NULL)
		open_display();

	return _display;
}

unsigned short
display_width()
{
	Display *dpy = display();

	return DisplayWidth(dpy, DefaultScreen(dpy));
}

unsigned short
display_height()
{
	Display *dpy = display();

	return DisplayHeight(dpy, DefaultScreen(dpy));
}

int
main(int argc, char *argv[])
{
	int running;
	XEvent event;
	Display *dpy;

	setlocale(LC_ALL, "");

	dpy = display();
	select_root_events(dpy);

	capture_existing_windows(dpy);

	dump_clients();

	dump_stacks();

	bind_keys();

	running = 1;
	while (running) {
		XNextEvent(dpy, &event);
		if (handle_event(&event) <= 0)
			break;
	}

	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XCloseDisplay(dpy);
	return 0;
}
