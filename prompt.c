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

/* prompt.c: Implements prompt for running commands. */

#include "mxswm.h"

#include <stdio.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef void (*PromptCallback)(const char *, void *);

static Window	 create_prompt(void);
static void	 draw_prompt(void);
static void	 close_prompt();
static int	 keycode(XKeyEvent *);
static void	 open_prompt(const char *, PromptCallback, void *);

static void	 command_callback(const char *, void *);
static void	 rename_callback(const char *, void *);

static Window window;
static wchar_t prompt[4096];
static size_t nprompt;

static XIC ic;

static PromptCallback callback;
static void *callback_udata;

static void
command_callback(const char *s, void *udata)
{
	char ex[4096];

	if (s == NULL || strlen(s) == 0)
		return;

	/* TODO: Use fork-exec here */
	if (snprintf(ex, sizeof(ex), "%s &", s) < sizeof(ex)) {
		TRACE_LOG("Running %s", ex);
		system(ex);
	}	
}

static void
rename_callback(const char *s, void *udata)
{
	struct client *client = udata;

	rename_client_name(client, s);
}

void
prompt_command()
{
	close_menu();
	open_prompt("", command_callback, NULL);
}

void
prompt_rename()
{
	if (is_menu_visible())
		select_menu_item();
	open_prompt(client_name(current_client()), rename_callback,
	    current_client());
}

static void
open_prompt(const char *initial, PromptCallback _callback, void *udata)
{
	XEvent e;
	XIM xim;

	callback = _callback;
	callback_udata = udata;

	if (initial != NULL) {
		nprompt = mbstowcs(prompt, initial, sizeof(prompt));
		prompt[nprompt] = '\0';
	} else {
		nprompt = 0;
		prompt[nprompt] = '\0';
	}

	if (window == 0)
		window = create_prompt();

	set_font(FONT_TITLE);
	XMoveResizeWindow(display(), window, STACK_X(current_stack()),
	    STACK_Y(current_stack()), STACK_WIDTH(current_stack()),
	    get_font_height());
	XRaiseWindow(display(), window);
	XMapWindow(display(), window);
	draw_prompt();

	XSync(display(), False);
	XGrabKeyboard(display(), window, True, GrabModeAsync, GrabModeAsync,
	    CurrentTime);

	xim = XOpenIM(display(), NULL, NULL, NULL);

	ic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
	    XNClientWindow, window, NULL);
	XSetICFocus(ic);

	for (;;) {
		XWindowEvent(display(), window, KeyPressMask, &e);

		switch (e.type) {
		case KeyPress:
			if (keycode(&e.xkey) == 0)
				goto out;
			draw_prompt();
			break;
		default:
			break;
		}
	}

	XUnsetICFocus(ic);
	XDestroyIC(ic);

out:
	close_prompt();
}

void
close_prompt()
{
	XUngrabKeyboard(display(), CurrentTime);
	XUnmapWindow(display(), window);
	XFlush(display());
}

static int
keycode(XKeyEvent *e)
{
	KeySym sym;
	char ch[4 + 1];
	char s[4096];
	int n, ret;

	sym = XkbKeycodeToKeysym(display(), e->keycode, 0,
	    (e->state & ShiftMask) ? 1 : 0);

	switch (sym) {
		case XK_KP_Enter:
		case XK_Return:
			wcstombs(s, prompt, sizeof(s));
			if (callback != NULL)
				callback(s, callback_udata);
			return 0;
		case XK_Escape:
			return 0;
		case XK_BackSpace:
		case XK_Delete:
			if (nprompt > 0)
				prompt[--nprompt] = '\0';
			return 1;
	}

	if ((n = Xutf8LookupString(ic, e, ch, sizeof(ch), &sym, NULL)) < 0) {
		TRACE_LOG("XLookupString failed");
		n = 0;
		return 1;
	} else {
		if ((ret = mbtowc(&prompt[nprompt], ch, n)) <= 0) {
			TRACE_LOG("trouble converting...");
		} else
			nprompt++;

		TRACE_LOG("mbtowc returns %d %zu\n", ret, MB_CUR_MAX);
		prompt[nprompt] = '\0';
	}

	return 1;
}

static Window
create_prompt()
{
	int x, y, w, h;
	XSetWindowAttributes a;
	unsigned long v;

	w = display_width() / 2;

	set_font(FONT_NORMAL);
	h = get_font_height();
	x = w/2;
	y = display_height() / 2 - (h/2);
	v = CWBackPixel | CWOverrideRedirect;
	a.background_pixel = query_color(COLOR_TITLE_BG_FOCUS).pixel;
	a.override_redirect = True;
	window = XCreateWindow(display(),
	    DefaultRootWindow(display()),
	    x, y, w, h, 0, CopyFromParent,
	    InputOutput, CopyFromParent,
	    v, &a);

	return window;
}

static void
draw_prompt()
{
	char s[4096];
	XGlyphInfo extents;

	XClearWindow(display(), window);

	wcstombs(s, prompt, sizeof(s));

	TRACE_LOG("s is: %s", s);

	font_extents(s, strlen(s), &extents);

	draw_font(window, 0, 0, -1, s);
	draw_font(window, extents.xOff, 0, COLOR_CURSOR, " ");
}
