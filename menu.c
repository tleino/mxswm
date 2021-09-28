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
#include <stdio.h>
#include <string.h>

static Window _menu;
static GC _gc, _focus_gc;
static struct client *current;

void
create_menu()
{
	int x, y, w, h;
	XSetWindowAttributes a;
	unsigned long v;

	w = display_width() / 2;
	h = display_height() / 2;
	x = w / 2;
	y = h / 2;
	v = CWBackPixel | CWBorderPixel;
	a.background_pixel = 0;
	a.border_pixel = 434344;
	_menu = XCreateWindow(display(),
	    DefaultRootWindow(display()),
	    x, y, w, h, BORDERWIDTH, CopyFromParent,
	    InputOutput, CopyFromParent,
	    v, &a);
}

void
select_menu_item()
{
	close_menu();
	if (current != NULL) {
		focus_client(current);
		current = NULL;
	}
}

void
draw_menu()
{
	XGCValues v;
	struct client *client;
	unsigned short x, y;
	char *name;
	struct stack *stack = current_stack();
	char buf[256];
	char c;

	if (_gc == 0) {
		v.foreground = WhitePixel(display(), DefaultScreen(display()));
		v.font = XLoadFont(display(), FONTNAME);
		_gc = XCreateGC(display(), DefaultRootWindow(display()),
		    GCForeground | GCFont, &v);
		v.foreground = 434838438;
		v.font = XLoadFont(display(), FONTNAME);
		_focus_gc = XCreateGC(display(), DefaultRootWindow(display()),
		    GCForeground | GCFont, &v);
	}

	XMoveWindow(display(), _menu, STACK_X(stack), STACK_Y(stack));
	XResizeWindow(display(), _menu, STACK_WIDTH(stack), STACK_HEIGHT(stack));
	XRaiseWindow(display(), _menu);
	XSync(display(), False);

	client = NULL;
	x = 40;
	y = 40;
	while ((client = next_client(client)) != NULL) {
		name = client_name(client);

		if (client == stack->client)
			c = '*';
		else if (find_stack(client) != NULL)
			c = '^';
		else
			c = ' ';

		if (name == NULL)
			name = "???";
		snprintf(buf, sizeof(buf),
		    "%c %s", c, name);

		if (client == current)
			XDrawImageString(display(), _menu, _focus_gc, x, y,
			    buf, strlen(buf));
		else
			XDrawImageString(display(), _menu, _gc, x, y, buf,
			    strlen(buf));
		y += 30;
	}
}

void
show_menu()
{
	if (_menu == 0)
		create_menu();
	current = current_client();
	XMapWindow(display(), _menu);
	XSync(display(), False);
	draw_menu();
}

void
hide_menu()
{
	XUnmapWindow(display(), _menu);
}

void
focus_menu_backward()
{
	current = prev_client(current);
	if (current == NULL)
		current = next_client(NULL);
	draw_menu();
}

void
focus_menu_forward()
{
	current = next_client(current);
	if (current == NULL)
		current = next_client(NULL);
	draw_menu();
}
