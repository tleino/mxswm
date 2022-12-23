/*
 * ISC License
 *
 * Copyright (c) 2022, Tommi Leino <namhas@gmail.com>
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Window _statusbar[16];
static int _nstatusbar;
static int _visible;

static void
create_statusbar()
{
	int x, y, w, h;
	XSetWindowAttributes a;
	unsigned long v;
	int i;

	for (i = 0; i < monitors(); i++) {
		w = display_width(i);
		set_font(FONT_NORMAL);
		h = get_font_height();
		x = monitor_x(i);
		y = display_height(i) - h;
		v = CWBackPixel | CWOverrideRedirect;
		a.background_pixel = query_color(COLOR_STATUSBAR_BG).pixel;
		a.override_redirect = True;
		_statusbar[i] = XCreateWindow(display(),
		    DefaultRootWindow(display()),
		    x, y, w, h, 0, CopyFromParent,
		    InputOutput, CopyFromParent,
		    v, &a);
		XSelectInput(display(), _statusbar[i], ExposureMask);
		_nstatusbar++;
	}
}

void
draw_statusbar()
{
	char *name, *buf;
	XTextProperty text;
	XGlyphInfo extents;
	int x;
	int i;

	if (!_visible)
		return;

	for (i = 0; i < monitors(); i++) {
		XClearWindow(display(), _statusbar[i]);

		set_font_color(COLOR_STATUSBAR_FG);

		get_utf8_property(DefaultRootWindow(display()),
		    wmh[_NET_WM_NAME], &name);

		if (name == NULL) {
			if (XGetWMName(display(), DefaultRootWindow(display()),
			    &text) != 0) {
				name = malloc(text.nitems + 1);
				if (name != NULL) {
					memcpy(name, text.value, text.nitems);
					name[text.nitems] = 0;
				}
			}
		}

		if (name != NULL) {
			buf = malloc(strlen(name) + 3);
			if (buf) {
				snprintf(buf, strlen(name) + 3, " %s ", name);

				font_extents(buf, strlen(buf), &extents);

				x = display_width(i) / 2;
				x -= (extents.xOff / 2);
				if (x < 0)
					x = 0;
				(void) draw_font(_statusbar[i], x, 0,
				    COLOR_STATUSBAR_BG, buf);
				free(buf);
			}
			free(name);
		}
	}
}

int
is_statusbar(Window window)
{
	int i;

	for (i = 0; i < monitors(); i++) {
		if (window == _statusbar[i])
			return 1;
	}
	return 0;
}

void
toggle_statusbar()
{
	if (_visible)
		hide_statusbar();
	else
		show_statusbar();
}

void
show_statusbar()
{
	int i;

	if (_nstatusbar == 0)
		create_statusbar();

	adjust_stacks_height(get_font_height() * 1.5);

	if (!_visible) {
		for (i = 0; i < monitors(); i++)
			XMapWindow(display(), _statusbar[i]);
	}
}

void
set_statusbar_mapped_status(int mapped)
{
	_visible = mapped;
	if (_visible)
		draw_statusbar();
}

void
hide_statusbar()
{
	int i;

	adjust_stacks_height(0);

	if (_visible) {
		for (i = 0; i < monitors(); i++)
			XUnmapWindow(display(), _statusbar[i]);
	}
}
