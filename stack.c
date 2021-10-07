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

#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static struct stack *_head;
static struct stack *_focus;
static XFontStruct *_fs;
static GC _gc;
static int _highlight;

static void create_stack_titlebar(struct stack *);
static int stack_width(int, int, int);

static int maxwidth_override;

void
move_stack(int dir)
{
	struct stack *stack;
	struct stack *tmp;

	stack = current_stack();

	switch (dir) {
	case -1:
		if (stack == _head || stack->prev == NULL)
			return;

		tmp = stack->prev;
		if (stack->next != NULL)
			stack->next->prev = tmp;
		if (tmp->prev != NULL)
			tmp->prev->next = stack;
		tmp->next = stack->next;
		stack->next = tmp;
		stack->prev = tmp->prev;
		tmp->prev = stack;
		if (tmp == _head)
			_head = stack;
		break;
	case 1:
		if (stack->next == NULL)
			return;

		tmp = stack->next;
		if (stack->prev != NULL)
			stack->prev->next = tmp;
		if (tmp->prev != NULL)
			tmp->prev->next = tmp;
		tmp->prev = stack->prev;
		stack->prev = tmp;
		stack->next = tmp->next;
		tmp->next = stack;
		if (stack == _head)
			_head = tmp;
		break;
	default:
		assert(0);
	}
	resize_stacks();
	draw_stacks();
}

void
highlight_stacks(int i)
{
	_highlight = i;
	draw_stacks();
}

void
toggle_stacks_maxwidth_override()
{
	maxwidth_override ^= 1;
	resize_stacks();
}

struct stack *
find_stack(int num)
{
	struct stack *np;

	for (np = _head; np != NULL; np = np->next)
		if (np->num == num)
			return np;

	return NULL;
}

static void
create_stack_titlebar(struct stack *stack)
{
	int x, y, w, h;
	XSetWindowAttributes a;
	unsigned long v;
	Display *dpy;
	XGCValues gcv;

	dpy = display();

	if (_fs == NULL) {
		_fs = XLoadQueryFont(display(), TITLEFONTNAME);
		if (_fs == NULL) {
			warnx("couldn't load font: %s", TITLEFONTNAME);
			_fs = XLoadQueryFont(display(), FALLBACKFONT);
			if (_fs == NULL)
				errx(1, "couldn't load font: %s",
				    FALLBACKFONT);
		}
	}

	if (_gc == 0) {
		gcv.foreground = BlackPixel(dpy, DefaultScreen(dpy));
		gcv.font = _fs->fid;
		_gc = XCreateGC(dpy, DefaultRootWindow(dpy),
		    GCForeground | GCFont, &gcv);
	}

	assert(BORDERWIDTH > 0);
	w = BORDERWIDTH;
	h = BORDERWIDTH;
	x = BORDERWIDTH;
	y = 0;
	v = CWBackPixel;
	a.background_pixel = TITLEBAR_NORMAL_COLOR;
	stack->window = XCreateWindow(dpy,
	    DefaultRootWindow(dpy),
	    x, y, w, h, 0, CopyFromParent,
	    InputOutput, CopyFromParent,
	    v, &a);
	XMapWindow(dpy, stack->window);
}

void
draw_stack(struct stack *stack)
{
	Display *dpy;
	struct client *client;
	int x, y;
	int font_width, font_height, font_x, font_y;
	char buf[1024];

	dpy = display();
	client = find_top_client(stack);

	XClearWindow(dpy, stack->window);
	XRaiseWindow(dpy, stack->window);
	if (client != NULL && client->name != NULL) {
		font_x = _fs->min_bounds.lbearing;
		font_y = _fs->max_bounds.ascent;
		font_width = _fs->max_bounds.rbearing -
		    _fs->min_bounds.lbearing;
		font_height = _fs->max_bounds.ascent +
		    _fs->max_bounds.descent;

		x = font_x;
		y = (0 * font_height) + font_y;

		if (_highlight)
			snprintf(buf, sizeof(buf), "stack %d",
			    stack->num);
		else
			snprintf(buf, sizeof(buf), "%s", client->name);

		XDrawString(display(), stack->window, _gc, x, y,
		    buf, strlen(buf));
	}
}

void
draw_stacks()
{
	struct stack *np;

	for (np = _head; np != NULL; np = np->next)
		draw_stack(np);
}

static void
renumber_stacks()
{
	struct stack *np;
	size_t i;

	/*
	 * TODO: Consider replacing the whole stack management
	 *       with a realloc()'d and memmove()'d array rather
	 *       than playing with linked lists and we'd get the
	 *       the numbers "for free".
	 */
	i = 0;
	for (np = _head; np != NULL; np = np->next)
		np->num = ++i;	
}

void
resize_stack(struct stack *stack, unsigned short width)
{
	Display *dpy;

	assert(stack != NULL);

	dpy = display();
	stack->width = width;

	XMoveWindow(dpy, stack->window, stack->x, 0);
	XResizeWindow(dpy, stack->window, stack->width, BORDERWIDTH);
	resize_client(find_top_client(stack));
	draw_stack(stack);
}

static int
stack_width(int avg, int prefer, int surplus)
{
	int width;

	width = avg;

	if (prefer > 0)
		width = MIN(width, prefer + BORDERWIDTH);
	else if (surplus > 0)
		width += surplus;

#ifdef MAXWIDTH
	if (!maxwidth_override)
		width = MIN(width, MAXWIDTH + BORDERWIDTH);
#endif

	return MAX(width, BORDERWIDTH * 2);
}

void
resize_stacks()
{
	struct stack *np;
	int avg, width, surplus, total, total_surplus;
	int x;
	size_t n, n_want_surplus;

	n = 0;
	for (np = _head; np != NULL; np = np->next)
		n++;

	/*
	 * Client width here is calculated to be its width + BORDERWIDTH,
	 * keeping things simple, but we need to account for the initial
	 * BORDERWIDTH.
	 */
	avg = (display_width() - BORDERWIDTH);
	avg /= n;

	/*
	 * Calculate surplus caused by prefer_width settings to be
	 * distributed to auto-resizing stacks.
	 */
	surplus = 0;
	n_want_surplus = 0;
	for (np = _head; np != NULL; np = np->next) {
		if (np->prefer_width > 0) {
			width = stack_width(avg, np->prefer_width, 0);
			surplus += (stack_width(avg, 0, 0) - width);
		} else
			n_want_surplus++;
	}
	if (n_want_surplus > 0)
		surplus /= n_want_surplus;

	/*
	 * Calculate total for finding out how to center the stacks.
	 */
	total = 0;
	for (np = _head; np != NULL; np = np->next) {
		width = stack_width(avg, np->prefer_width, surplus);
		total += width;
	}
	total_surplus = display_width() - BORDERWIDTH - total;

	/*
	 * Resize and move.
	 */
	x = BORDERWIDTH + (total_surplus/2);
	for (np = _head; np != NULL; np = np->next) {
		np->x = x;
		width = stack_width(avg, np->prefer_width, surplus);
		resize_stack(np, width - BORDERWIDTH);
		x += width;
	}
}

void
add_stack_here()
{
	struct stack *stack;

	stack = current_stack();
	(void)add_stack(stack);
}

void
remove_stack_here()
{
	struct stack *stack;

	stack = current_stack();
	(void)remove_stack(stack);
}

struct stack *
add_stack(struct stack *after)
{
	struct stack *stack;
	struct client *client;

	stack = malloc(sizeof(struct stack));
	if (stack == NULL)
		return NULL;

	stack->height = display_height() - BORDERWIDTH;
	stack->width = 0;
	stack->x = 0;
	stack->y = 0;
	stack->prefer_width = 0;

	stack->prev = after;
	if (after != NULL) {
		/*
		 * Insert after 'after'.
		 */
		stack->next = after->next;
		after->next = stack;
		if (stack->next != NULL)
			stack->next->prev = stack;
	} else {
		/*
		 * Insert to head.
		 */
		stack->next = _head;
		_head = stack;
		if (_head->next != NULL)
			_head->next->prev = _head;
	}

	create_stack_titlebar(stack);

	resize_stacks();
	renumber_stacks();

	if (_focus == NULL)
		focus_stack(stack);

	dump_stacks();

	client = NULL;
	while ((client = next_client(client)) != NULL)
		resize_client(client);

	focus_stack(stack);

	return stack;
}

void
remove_stack(struct stack *stack)
{
	struct client *client;

	focus_stack_backward();
	if (_focus == stack) {
		warnx("cannot remove last stack");
		return;
	}

	if (stack->next != NULL)
		stack->next->prev = stack->prev;

	if (stack->prev != NULL)
		stack->prev->next = stack->next;
	else if (stack == _head)
		_head = stack->next;

	XUnmapWindow(display(), stack->window);
	XDestroyWindow(display(), stack->window);
	free(stack);

	dump_stacks();
	resize_stacks();

	client = NULL;
	while ((client = next_client(client)) != NULL) {
		if (CLIENT_STACK(client) == stack) {
			CLIENT_STACK(client) = _focus;
		}
		resize_client(client);
	}

	renumber_stacks();
	resize_stacks();
}

void
focus_stack(struct stack *stack)
{
	struct stack *prev;

	prev = _focus;
	_focus = stack;

	dump_stacks();

	focus_client(find_top_client(stack), stack);

	if (prev != NULL) {
		XSetWindowBackground(display(), prev->window, TITLEBAR_NORMAL_COLOR);
		draw_stack(prev);
	}

	XSetWindowBackground(display(), stack->window, TITLEBAR_FOCUS_COLOR);
	draw_stack(stack);
}

void
focus_stack_forward()
{
	struct stack *sp;
	int was_visible;

	was_visible = is_menu_visible();

	sp = current_stack();
	focus_stack(sp->next != NULL ? sp->next : _head);

	if (was_visible) {
		hide_menu();
		show_menu();
	}
}

void
focus_stack_backward()
{
	struct stack *sp, *np;
	int was_visible;

	was_visible = is_menu_visible();

	sp = current_stack();
	if (sp->prev != NULL)
		focus_stack(sp->prev);
	else {
		np = sp;
		while (np->next != NULL)
			np = np->next;
		assert(np != NULL);

		focus_stack(np);
	}

	if (was_visible) {
		hide_menu();
		show_menu();
	}
}

struct stack *
current_stack()
{
	if (_focus == NULL) {
		_focus = add_stack(NULL);
		if (_focus == NULL)
			err(1, "add_stack");
		focus_stack(_focus);
	}

	assert(_focus != NULL);
	return _focus;
}

#if (TRACE || TEST)
#include <inttypes.h>
#include <stdio.h>
void
dump_stack(struct stack *sp)
{
	printf("stack: %ju (width: %d, +%d+%d)",
	    (uintmax_t) sp, sp->width, sp->x, sp->y);
	if (sp == _focus)
		printf(" (focused)");
	putchar('\n');
}

void
dump_stacks()
{
	struct stack *np;

	printf("focus: %ju\n", (uintmax_t) current_stack());
	for (np = _head; np != NULL; np = np->next)
		dump_stack(np);
}
#endif
#if TEST
int
main(int argc, char *argv[])
{
	add_stack(NULL);
	add_stack(NULL);
	remove_stack(current_stack());
	add_stack(current_stack());
	dump_stacks();
}
#endif
