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
#include <ctype.h>
#include <err.h>
#include <unistd.h>

typedef void (*PromptCallback)(const char *, void *);

static Window	 create_prompt(void);
static void	 draw_prompt(void);
static void	 close_prompt();
static int	 keycode(XKeyEvent *);
static void	 open_prompt(const char *, PromptCallback, PromptCallback,
		    void *, int);

static void	 command_callback(const char *, void *);
static void	 command_step_callback(const char *, void *);
static void	 rename_callback(const char *, void *);
static void	 find_callback(const char *, void *);
static void	 find_step_callback(const char *, void *);

static Window window;
static wchar_t prompt[4096];
static size_t nprompt;
static size_t pos;
static int _want_centered;

static XIC ic;

static PromptCallback callback;
static PromptCallback step_callback;
static void *callback_udata;

static void
command_callback(const char *s, void *udata)
{
	char *q, *p, *sh;
	int i;
	pid_t pid;
	size_t sz;

	if (s == NULL || strlen(s) == 0)
		return;

	q = strdup(s);
	if (q == NULL) {
		warn("strdup");
		return;
	}

	/* Strip trailing spaces */
	for (i = strlen(q)-1; i >= 0 && isspace(q[i]); i--)
		q[i] = '\0';

	sz = strlen(q) + strlen("exec ") + 1;
	p = malloc(sz);
	if (p == NULL) {
		warn("malloc");
		return;
	}
	if (snprintf(p, sz, "exec %s", q) >= sz) {
		warnx("truncated '%s'", p);
		return;
	}

	if (add_command_to_history(q, 1))
		save_command_history();

	sh = getenv("SHELL");
	if (sh == NULL) {
		warn("getenv SHELL");
		sh = "/bin/sh";
	}
	pid = fork();
	if (pid == 0) {
#ifndef OPT_SH_FLAGS
#define OPT_SH_FLAGS "i"
#endif
		/*
		 * We use 'i' flag here, so that the shell would read
		 * a startup script, so that we can e.g. support aliases.
		 *
		 * This might be a good or a bad thing. Alternatively
		 * we would need to support our own aliases, which might
		 * be a good idea, because aliases for mxswm might be
		 * different than aliases for interactive shell.
		 *
		 * Please note you may have to set 'ENV' variable so
		 * that the shell would execute a non-login startup
		 * script.
		 */
		execl(sh, sh, "-" OPT_SH_FLAGS "c", p, NULL);
	} else if (pid == -1)
		warn("fork");

	free(q);
}

static void
command_step_callback(const char *s, void *udata)
{
	const char *q;

	q = match_command(s);
	if (q != NULL) {
		nprompt = mbstowcs(prompt, q, sizeof(prompt));
		prompt[nprompt] = '\0';
	} else if (s == NULL || s[0] == '\0') {
		nprompt = 0;
		prompt[nprompt] = '\0';
	} else {
		nprompt = pos;
		prompt[pos] = '\0';
	}
}

static void
rename_callback(const char *s, void *udata)
{
	struct client *client = udata;

	rename_client_name(client, s);
}

static void
find_callback(const char *s, void *udata)
{
	struct client *client;

	client = match_client(s);
	if (client != NULL)
		focus_client(client, client->stack);
}

static void
find_step_callback(const char *s, void *udata)
{
	struct client *client;
	char *q;

	client = match_client(s);
	if (client != NULL) {
		q = client->renamed_name;
		if (q == NULL)
			q = client->name;
		nprompt = mbstowcs(prompt, q, sizeof(prompt));
		prompt[nprompt] = '\0';
	} else if (s == NULL || s[0] == '\0') {
		nprompt = 0;
		prompt[nprompt] = '\0';
	}
}

void
prompt_find()
{
	close_menu();
	open_prompt("", find_callback, find_step_callback, NULL, 1);
}

void
prompt_command()
{
	close_menu();
	open_prompt("", command_callback, command_step_callback, NULL, 0);
}

void
prompt_rename()
{
	if (is_menu_visible())
		select_menu_item();
	open_prompt(client_name(current_client()), rename_callback, NULL,
	    current_client(), 0);
}

static void
open_prompt(const char *initial, PromptCallback _callback,
    PromptCallback _step_callback, void *udata, int center)
{
	XEvent e;
	XIM xim;

	callback = _callback;
	callback_udata = udata;

	step_callback = _step_callback;

	_want_centered = center;

	if (initial != NULL) {
		nprompt = mbstowcs(prompt, initial, sizeof(prompt));
		prompt[nprompt] = '\0';
		pos = nprompt;
	} else {
		nprompt = 0;
		pos = 0;
		prompt[nprompt] = '\0';
	}

	if (window == 0)
		window = create_prompt();

	set_font(FONT_TITLE);

	if (_want_centered)
		XMoveResizeWindow(display(), window, display_width() / 4,
		    display_height() / 2, display_width() / 2,
		    get_font_height());
	else
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
			if (nprompt > 0 && pos > 0) {
				prompt[--pos] = '\0';
				nprompt--;
			}
			if (step_callback != NULL) {
				wcstombs(s, prompt, sizeof(s));
				step_callback(s, callback_udata);
			}
			return 1;
	}

	if ((n = Xutf8LookupString(ic, e, ch, sizeof(ch), &sym, NULL)) < 0) {
		TRACE_LOG("XLookupString failed");
		n = 0;
		return 1;
	} else {
		if ((ret = mbtowc(&prompt[pos], ch, n)) <= 0) {
			TRACE_LOG("trouble converting...");
		} else {
			nprompt++;
			pos++;
		}

		TRACE_LOG("mbtowc returns %d %zu\n", ret, MB_CUR_MAX);
		prompt[pos] = '\0';
	}

	if (step_callback != NULL) {
		wcstombs(s, prompt, sizeof(s));
		step_callback(s, callback_udata);
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
	char s_until_pos[4096];
	wchar_t prompt_until_pos[4096];
	char ch[4 + 1];
	wchar_t prompt_cursor[2];
	XGlyphInfo extents;

	XClearWindow(display(), window);

	memcpy(prompt_until_pos, prompt, sizeof(prompt_until_pos));
	prompt_until_pos[pos] = '\0';
	wcstombs(s_until_pos, prompt_until_pos, sizeof(s_until_pos));
	wcstombs(s, prompt, sizeof(s));

	TRACE_LOG("s is: %s", s);

	if (pos < nprompt && nprompt > 0) {
		font_extents(s, strlen(s_until_pos), &extents);

		prompt_cursor[0] = prompt[pos];
		prompt_cursor[1] = '\0';
		wcstombs(ch, prompt_cursor, sizeof(ch));
		draw_font(window, extents.xOff, 0, COLOR_CURSOR, ch);
	} else {
		font_extents(s, strlen(s), &extents);
		draw_font(window, extents.xOff, 0, COLOR_CURSOR, " ");
	}

	draw_font(window, 0, 0, -1, s);
}
