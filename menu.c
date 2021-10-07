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
#include <err.h>

static Window _menu;
static GC _gc, _focus_gc, _highlight_gc;
static struct client *current;
static XFontStruct *_fs;
static int _menu_visible;
static int _highlight;

static void move_menu_item(int);

void
highlight_menu(int i)
{
	_highlight = i;
	draw_menu();
}

int
is_menu_visible()
{
	return _menu_visible;
}

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
	v = CWBackPixel;
	a.background_pixel = TITLEBAR_FOCUS_COLOR;
	_menu = XCreateWindow(display(),
	    DefaultRootWindow(display()),
	    x, y, w, h, 0, CopyFromParent,
	    InputOutput, CopyFromParent,
	    v, &a);
}

void
select_menu_item()
{
	if (current != NULL) {
		focus_client(current, current->stack);
		current = NULL;
	}
	close_menu();
}

static void
move_menu_item(int dir)
{
	struct client *client;
	struct stack *stack;

	client = current;
	stack = current_stack();

	if (client == NULL || !_menu_visible) {
		move_stack(dir);
		return;
	}

	if (dir == -1)
		focus_stack_backward();
	else
		focus_stack_forward();

	if (client != NULL) {
		client->stack = current_stack();
		focus_client(client, client->stack);
		client = NULL;
	}
	draw_menu();
}

void
move_menu_item_right()
{
	move_menu_item(1);
}

void
move_menu_item_left()
{
	move_menu_item(-1);
}

void
select_move_menu_item()
{
	highlight_stacks(0);
	if (current != NULL) {
		focus_client(current, current_stack());
		current = NULL;
	}
	close_menu();
}

void
select_menu_item_right()
{
	focus_stack_forward();
	if (current != NULL)
		focus_client(current, current_stack());
	close_menu();
}

void
select_menu_item_left()
{
	focus_stack_backward();
	if (current != NULL)
		focus_client(current, current_stack());
	close_menu();
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
	int font_x, font_y, font_width, font_height;
	int row;
	int nclients;

	if (_menu == 0)
		return;

	if (_fs == NULL) {
		_fs = XLoadQueryFont(display(), FONTNAME);
		if (_fs == NULL) {
			warnx("couldn't load font: %s", FONTNAME);
			_fs = XLoadQueryFont(display(), FALLBACKFONT);
			if (_fs == NULL)
				errx(1, "couldn't load font: %s",
				    FALLBACKFONT);
		}
	}

	if (_gc == 0) {
		v.foreground = WhitePixel(display(), DefaultScreen(display()));
		v.font = _fs->fid;
		_gc = XCreateGC(display(), DefaultRootWindow(display()),
		    GCForeground | GCFont, &v);

		v.foreground = 434838438;
		v.font = _fs->fid;
		_focus_gc = XCreateGC(display(), DefaultRootWindow(display()),
		    GCForeground | GCFont, &v);

		v.foreground = 6748778438;
		v.font = _fs->fid;
		_highlight_gc = XCreateGC(display(),
		    DefaultRootWindow(display()), GCForeground | GCFont, &v);
	}

	client = NULL;
	nclients = 0;
	while ((client = next_client(client)) != NULL) {
		nclients++;
	}

	font_x = _fs->min_bounds.lbearing;
	font_y = _fs->max_bounds.ascent;
	font_width = _fs->max_bounds.rbearing -
	    _fs->min_bounds.lbearing;
	font_height = _fs->max_bounds.ascent +
	    _fs->max_bounds.descent;

	XMoveWindow(display(), _menu, STACK_X(stack), BORDERWIDTH);
	XResizeWindow(display(), _menu, STACK_WIDTH(stack),
	    nclients * font_height);

	XRaiseWindow(display(), _menu);
	XSync(display(), False);

	client = NULL;
	x = 40;
	y = 40;
	row = 0;
	while ((client = next_client(client)) != NULL) {
#if 0
		if (client->stack != current_stack())
			continue;
#endif

		name = client_name(client);

#if 0
		if (client == current_client())
			c = '*';
		else if (client->stack != current_client()->stack &&
		    find_top_client(client->stack) == client)
			c = '^';
		else
			c = ' ';
#endif

		if (name == NULL)
			name = "???";
		if (CLIENT_STACK(client) == stack)
			snprintf(buf, sizeof(buf), "%s", name);
		else if (CLIENT_STACK(client) != NULL)
			snprintf(buf, sizeof(buf), "%s [%d]", name,
			CLIENT_STACK(client)->num);
		else
			snprintf(buf, sizeof(buf), "%s [??]", name);

		x = font_x;
		y = (row * font_height) + font_y;

		if (client == current) {
			if (!_highlight)
				XDrawString(display(), _menu, _focus_gc,
				    x, y, buf, strlen(buf));
			else
				XDrawString(display(), _menu,
				    _highlight_gc, x, y, buf, strlen(buf));
		} else
			XDrawString(display(), _menu, _gc, x, y, buf,
			    strlen(buf));
		row++;
	}
}

void
show_menu()
{
	if (_menu == 0)
		create_menu();
	current = current_client();
	if (!_menu_visible) {
		XMapWindow(display(), _menu);
		XSync(display(), False);
		draw_menu();
		_menu_visible = 1;
	}
}

void
hide_menu()
{
	if (_menu_visible) {
		XUnmapWindow(display(), _menu);
		_menu_visible = 0;
		current = NULL;
	}
}

void
focus_menu_backward()
{
	if (!_menu_visible)
		return;

	current = prev_client(current);
	if (current == NULL) {
		hide_menu();
		return;
	}
	draw_menu();
}

void
focus_menu_forward()
{
	if (!_menu_visible) {
		show_menu();
		return;
	}
	if (next_client(current) != NULL)
		current = next_client(current);
	draw_menu();
}
