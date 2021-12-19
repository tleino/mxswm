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

static struct client *currentp;

static int _menu_visible;
static int _global_menu_visible;
static int _highlight;

static void move_menu_item(int);

static struct client *
current(struct stack *stack)
{
	if (currentp == NULL) {
		return currentp;
	}

	while (currentp != NULL &&
	    ((stack != NULL && currentp->stack != stack) || !currentp->mapped))
		currentp = next_client(currentp, stack);

	if (currentp == NULL)
		currentp = next_client(NULL, stack);

	return currentp;
}

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

	w = display_width() / 2;

	set_font(FONT_NORMAL);
	h = get_font_height();
	x = w/2;
	y = display_height() / 2 - (h/2);
	v = CWBackPixel | CWOverrideRedirect;
	a.background_pixel = query_color(COLOR_TITLE_BG_FOCUS).pixel;
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

	w = 1;
	h = 1;
	x = 0;
	y = 0;
	v = CWBackPixel | CWOverrideRedirect;
	a.background_pixel = query_color(COLOR_TITLE_BG_FOCUS).pixel;
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
	struct client *client;

	client = current(current_stack());
	if (client != NULL) {
		focus_client(client, NULL);
		currentp = NULL;
	}
	close_menu();
}

static void
move_menu_item(int dir)
{
	struct client *client;

	if (_menu_visible && current(current_stack()) != NULL)
		client = current(current_stack());
	else
		client = current_client();
	if (client == NULL)
		return;

	if (dir == -1)
		focus_stack_backward();
	else
		focus_stack_forward();

	if (client != NULL) {
		client->stack = current_stack();
		focus_client(client, client->stack);
		client = NULL;
	}

	if (_menu_visible) {
		currentp = NULL;
		close_menu();
	}
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
	struct client *client;

	highlight_stacks(0);
	if (_global_menu_visible) {
		client = current(NULL);
		close_global_menu();
		if (client != NULL)
			focus_client(client, client->stack);
	} else {
		client = current(current_stack());
		close_menu();
		if (client != NULL)
			focus_client(client, NULL);
	}
	currentp = NULL;
}

void
select_menu_item_right()
{
	struct client *client;

	client = current(current_stack());
	focus_stack_forward();
	if (client != NULL)
		focus_client(client, NULL);
	close_menu();
}

void
select_menu_item_left()
{
	struct client *client;

	client = current(current_stack());
	focus_stack_backward();
	if (client != NULL)
		focus_client(client, NULL);
	close_menu();
}

void
open_global_menu()
{
	if (_global_menu_visible)
		return;

	currentp = NULL;
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
	struct client *client;

	client = current(NULL);
	currentp = next_client(client, NULL);

	if (currentp == NULL)
		currentp = next_client(NULL, NULL);
}

void
draw_global_menu()
{
	struct client *client;
	char *name;
	char buf[256];
	int is_last, is_first;

	TRACE_LOG("visible=%d", _menu_visible);

	if (_global_menu_visible == 0 || _global_menu == 0)
		return;

	client = current(NULL);

	XClearWindow(display(), _global_menu);
	XRaiseWindow(display(), _global_menu);

	name = NULL;
	if (client != NULL)
		name = client_name(client);
	if (name == NULL)
		name = "<no name>";

	is_last = (next_client(client, NULL) == NULL);
	is_first = (client == next_client(NULL, NULL));

	snprintf(buf, sizeof(buf), "%c%s%c",
	    is_first ? '<' : ' ', name, is_last ? '>' : ' ');

	set_font_color(COLOR_MENU_FG_NORMAL);
	set_font(FONT_NORMAL);
	draw_font(_global_menu, 0, 0, buf);
}

void
draw_menu()
{
	struct client *client, *cclient;
	char *name;
	struct stack *stack = current_stack();
	char buf[256], flags[10];
	int row_height;
	int y, x;
	size_t nclients;
	XGlyphInfo extents;

	if (_menu_visible == 0 || _menu == 0) {
		TRACE_LOG("not doing anything...");
		return;
	}

	client = NULL;
	nclients = count_clients(stack);
	if (nclients > 0)
		nclients--;

	if (nclients == 0) {
		TRACE_LOG("zero clients, closing menu");
		close_menu();
		return;
	}

	cclient = current(current_stack());

	set_font(FONT_NORMAL);
	row_height = get_font_height();

	XMoveResizeWindow(display(), _menu, STACK_X(stack), row_height,
	    STACK_WIDTH(stack), nclients * row_height);

	XRaiseWindow(display(), _menu);

	client = current_client();
	y = 0;
	while ((client = next_client(client, stack)) != NULL) {
		name = client_name(client);
		if (name == NULL)
			name = "???";

		snprintf(buf, sizeof(buf), " %s ", name);

		snprintf(flags, sizeof(flags), "%d%c%c%c%c ",
		    client->stack ? client->stack->num : 0,
		    (cclient == client) ? '*' : '-',
		    client->flags & CF_HAS_TAKEFOCUS ? 't' : '-',
		    client->flags & CF_HAS_DELWIN ? 'd' : '-',
		    client->mapped ? 'm' : '-');

		if (client == cclient) {
			if (_highlight)
				set_font_color(COLOR_MENU_FG_HIGHLIGHT);
			else
				set_font_color(COLOR_MENU_FG_FOCUS);
		} else
			set_font_color(COLOR_MENU_FG_NORMAL);

		XClearArea(display(), _menu, 0, y, STACK_WIDTH(stack),
		    get_font_height(), False);
		x = draw_font(_menu, 0, y, buf);

		set_font_color(COLOR_FLAGS);
		font_extents(flags, strlen(flags), &extents);

		if (x > STACK_WIDTH(stack) - extents.xOff)
			x = STACK_WIDTH(stack) - extents.xOff;
		XClearArea(display(), _menu, x, y, STACK_WIDTH(stack) - x,
		    get_font_height(), False);
		draw_font(_menu, STACK_WIDTH(stack) - extents.xOff, y, flags);

		y += row_height;
	}

	TRACE_LOG("%zu clients displayed...", nclients);
}

void
show_menu()
{
	TRACE_LOG("*");
	if (_menu == 0)
		create_menu();
	currentp = next_client(current_client(), current_stack());
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
		currentp = NULL;
	} else
		TRACE_LOG("ignore");
}

void
focus_menu_backward()
{
	struct client *client;

	if (!_menu_visible)
		return;

	client = current(current_stack());
	currentp = prev_client(client, current_stack());
	if (currentp == NULL || currentp == current_client()) {
		hide_menu();
		return;
	}
	draw_menu();
}

void
focus_menu_forward()
{
	struct client *client;

	if (!_menu_visible) {
		TRACE_LOG("showing menu because not visible");
		show_menu();
		return;
	}

	client = current(current_stack());
	client = next_client(client, current_stack());
	if (client != NULL)
		currentp = client;
	draw_menu();
}
