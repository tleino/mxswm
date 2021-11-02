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
static Window _global_menu;
static GC _gc, _focus_gc, _highlight_gc;
static struct client *current;
static struct client *global_current;
static XFontStruct *_fs;
static int _menu_visible;
static int _global_menu_visible;
static int _highlight;
static int font_x, font_y, font_width, font_height;

static void move_menu_item(int);
static void ensure_font(void);

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
create_global_menu()
{
	int x, y, w, h;
	XSetWindowAttributes a;
	unsigned long v;

	ensure_font();

	w = display_width() / 2;
	h = font_height;
	x = w/2;
	y = display_height() / 2 - (h/2);
	v = CWBackPixel | CWOverrideRedirect;
	a.background_pixel = TITLEBAR_FOCUS_COLOR;
	a.override_redirect = True;
	_global_menu = XCreateWindow(display(),
	    DefaultRootWindow(display()),
	    x, y, w, h, 0, CopyFromParent,
	    InputOutput, CopyFromParent,
	    v, &a);
}

void
create_menu()
{
	int x, y, w, h;
	XSetWindowAttributes a;
	unsigned long v;

	ensure_font();

	w = 1;
	h = 1;
	x = 0;
	y = 0;
	v = CWBackPixel | CWOverrideRedirect;
	a.background_pixel = TITLEBAR_FOCUS_COLOR;
	a.override_redirect = True;
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
	if (_global_menu_visible) {
		close_global_menu();
		if (global_current != NULL)
			focus_client(global_current, global_current->stack);
		global_current = NULL;
	} else if (current != NULL) {
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

static void
ensure_font()
{
	XGCValues v;

	if (_fs == NULL) {
		_fs = XLoadQueryFont(display(), FONTNAME);
		if (_fs == NULL) {
			warnx("couldn't load font: %s", FONTNAME);
			_fs = XLoadQueryFont(display(), FALLBACKFONT);
			if (_fs == NULL)
				errx(1, "couldn't load font: %s",
				    FALLBACKFONT);
		}

		font_x = _fs->min_bounds.lbearing;
		font_y = _fs->max_bounds.ascent;
		font_width = _fs->max_bounds.rbearing -
		    _fs->min_bounds.lbearing;
		font_height = _fs->max_bounds.ascent +
		    _fs->max_bounds.descent;
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
}

void
open_global_menu()
{
	if (_global_menu_visible)
		return;

	global_current = current_client();
	if (_global_menu == 0)
		create_global_menu();
	_global_menu_visible = 1;
	XMapWindow(display(), _global_menu);
	draw_global_menu();
}

void
close_global_menu()
{
	XUnmapWindow(display(), _global_menu);
	_global_menu_visible = 0;
}

void
select_next_global_menu()
{
	global_current = next_client(global_current, NULL);
	if (global_current == NULL)
		global_current = next_client(NULL, NULL);
}

void
draw_global_menu()
{
	struct client *client;
	unsigned short x, y;
	char *name;
	char buf[256];
	int is_last, is_first;

	TRACE_LOG("visible=%d", _menu_visible);

	if (_global_menu_visible == 0 || _global_menu == 0)
		return;

	client = global_current;

	XClearWindow(display(), _global_menu);
	XRaiseWindow(display(), _global_menu);

	name = NULL;
	if (client != NULL)
		name = client_name(client);
	if (name == NULL)
		name = "???";

	is_last = (next_client(global_current, NULL) == NULL);
	is_first = (global_current == next_client(NULL, NULL));

	snprintf(buf, sizeof(buf), "%c%s%c",
	    is_first ? '<' : ' ', name, is_last ? '>' : ' ');

	x = font_x;
	y = font_y;
	XDrawString(display(), _global_menu, _gc, x, y, buf, strlen(buf));
}

void
draw_menu()
{
	struct client *client;
	unsigned short x, y;
	char *name;
	struct stack *stack = current_stack();
	char buf[256];
	int row;
	size_t nclients;

	TRACE_LOG("visible=%d", _menu_visible);

	if (_menu_visible == 0 || _menu == 0)
		return;

	client = NULL;
	nclients = count_clients(stack);

	if (nclients == 0) {
		close_menu();
		return;
	}

	XMoveWindow(display(), _menu, STACK_X(stack), BORDERWIDTH);
	XResizeWindow(display(), _menu, STACK_WIDTH(stack),
	    nclients * font_height);

	XRaiseWindow(display(), _menu);

	client = NULL;
	row = 0;
	while ((client = next_client(client, stack)) != NULL) {
		name = client_name(client);
		if (name == NULL)
			name = "???";

		snprintf(buf, sizeof(buf), "%s", name);

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
		TRACE_LOG("map menu window %lx", _menu);
		XMapWindow(display(), _menu);
		_menu_visible = 1;
		draw_menu();
	} else
		TRACE_LOG("ignore");
}

void
hide_menu()
{
	if (_menu_visible) {
		TRACE_LOG("hide menu window %lx", _menu);
		XUnmapWindow(display(), _menu);
		_menu_visible = 0;
		current = NULL;
	} else
		TRACE_LOG("ignore");
}

void
focus_menu_backward()
{
	if (!_menu_visible)
		return;

	current = prev_client(current, current_stack());
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
	if (next_client(current, current_stack()) != NULL)
		current = next_client(current, current_stack());
	draw_menu();
}
