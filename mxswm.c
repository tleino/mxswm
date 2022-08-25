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
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>

Display *_display;

static void select_root_events(Display *);
static int wm_rights_error(Display *, XErrorEvent *);
static int manageable(Window, int *);

char **Argv;

static void
select_root_events(Display *display)
{
	XSync(display, False);
	XSetErrorHandler(wm_rights_error);

	XSelectInput(display, DefaultRootWindow(display),
	             SubstructureNotifyMask |
	             KeyReleaseMask |
	             ButtonPressMask |
	             ButtonReleaseMask |
	             SubstructureRedirectMask |
	             PropertyChangeMask);

	/*
	 * Enable click-to-focus.
	 */
	XGrabButton(display, 1, 0, DefaultRootWindow(display), True,
	            ButtonPressMask | ButtonReleaseMask, GrabModeSync,
		    GrabModeAsync, None, None);

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
	int mapped;
	unsigned int nchildren;

	if (XQueryTree(display, DefaultRootWindow(display),
	               &root, &parent, &children, &nchildren) == 0) {
		warnx("did not capture existing windows");
		return;
	}

	for (i = 0; i < nchildren; i++) {
		if (manageable(children[i], &mapped)) {
			TRACE_LOG("capture %lx", children[i]);
			if (add_client(children[i], NULL, mapped, NULL, 1) ==
			    NULL)
				warn("add_client");
		} else
			warnx("did not capture %lx", children[i]);
	}

	if (nchildren > 0)
		XFree(children);
}

int
manageable(Window w, int *mapped)
{
	XWindowAttributes wa;
	int redirectable, output_window;
	Display *d = display();

	if (!XGetWindowAttributes(d, w, &wa)) {
		warnx("XGetWindowAttributes failed for %lx", w);
		return 0;
	}
	*mapped = (wa.map_state == IsViewable);
	redirectable = (wa.override_redirect != True);
	output_window = (wa.class != InputOnly);

	return redirectable && output_window;
}

static void
open_display()
{
	char *denv;
	int xkbmaj, xkbmin, xkb_op, xkb_event, xkb_error;

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

	/*
	 * We use XKB extension because XKeycodeToKeysym is deprecated,
	 * even though there would be no big harm.
	 */
	xkbmaj = XkbMajorVersion;
	xkbmin = XkbMinorVersion;
	if (XkbLibraryVersion(&xkbmaj, &xkbmin) == False)
		errx(1, "trouble with XKB extension; needed %d.%d got %d.%d",
		    XkbMajorVersion, XkbMinorVersion, xkbmaj, xkbmin);
	if (XkbQueryExtension(_display, &xkb_op, &xkb_event, &xkb_error,
	    &xkbmaj, &xkbmin) == False)
		errx(1, "trouble with XKB extension");

	init_wmh();
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
	int ctlfd;
	int status;

	/*
	 * Store argv so that we can restart the window manager.
	 */
	Argv = argv;

	if (!setlocale(LC_CTYPE, "en_US.UTF-8") || !XSupportsLocale())
		errx(1, "no locale support");

	mbtowc(NULL, NULL, MB_CUR_MAX);

	dpy = display();

	if (argc >= 2)
		if (strcmp(argv[1], "sync") == 0)
			XSynchronize(display(), True);

	select_root_events(dpy);

	dump_clients();

	dump_stacks();

	bind_keys();

	run_ctl_lines();

	resize_stacks();

	capture_existing_windows(dpy);

	focus_stack(find_stack(1));

#if WANT_CTLSOCKET
	ctlfd = listen_ctlsocket();
#else
	ctlfd = -1;
#endif
	if (ctlfd != -1) {
		run_ctlsocket_event_loop(ctlfd);
	} else {
		running = 1;
		while (running) {
#ifndef WAIT_MYPGRP
#define WAIT_MYPGRP 0
#endif
			(void) waitpid(WAIT_MYPGRP, &status, WNOHANG);
			XNextEvent(dpy, &event);
			if (handle_event(&event) <= 0)
				break;
		}
	}

	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XCloseDisplay(dpy);
	return 0;
}
