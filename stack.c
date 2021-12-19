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

#include <X11/Xft/Xft.h>

static struct stack *_head;
static struct stack *_focus;
static int _highlight;

static void create_stack_titlebar(struct stack *);
static int stack_width(int, int, int);
static struct stack *next_stack(struct stack *);
static struct stack *prev_stack(struct stack *);
static struct stack *first_stack(void);
static struct stack *last_stack(void);
static void hide_stack(struct stack *);
static void show_stack(struct stack *);

static int maxwidth_override;

static struct stack *
next_stack(struct stack *np)
{
	assert(np != NULL);

	np = np->next;
	while (np && np->hidden)
		np = np->next;

	return np;
}

struct stack *
have_stack(Window window)
{
	struct stack *np;

	for (np = _head; np != NULL; np = np->next) {
		if (np->window != window)
			continue;

		return np;
	}

	return NULL;
}

static void
hide_stack(struct stack *stack)
{
	unmap_clients(stack);
	if (stack->mapped)
		XUnmapWindow(display(), stack->window);
}

static void
show_stack(struct stack *stack)
{
	map_clients(stack);
	resize_clients(stack);
	if (!stack->mapped)
		XMapWindow(display(), stack->window);
}

static struct stack *
first_stack()
{
	struct stack *np;

	np = _head;
	assert(np != NULL);

	while (np && np->hidden)
		np = np->next;

	assert(np != NULL);
	return np;
}

static struct stack *
last_stack()
{
	struct stack *np;

	np = _head;
	assert(np != NULL);

	while (np->next)
		np = np->next;
	while (np && np->hidden)
		np = np->prev;

	assert(np != NULL);
	return np;
}

static struct stack *
prev_stack(struct stack *np)
{
	assert(np != NULL);

	np = np->prev;
	while (np && np->hidden)
		np = np->prev;

	return np;
}

static void
stack_copy(struct stack *dst, struct stack *src)
{
	struct stack *prev, *next;
	struct client *np;

	prev = dst->prev;
	next = dst->next;

	memcpy(dst, src, sizeof(struct stack));

	np = NULL;
	while ((np = next_client(np, src)) != NULL)
		CLIENT_STACK(np) = dst;

	dst->prev = prev;
	dst->next = next;
}

void
move_stack_right()
{
	move_stack(1);
}

void
move_stack_left()
{
	move_stack(-1);
}

void
move_stack(int dir)
{
	struct stack *stack, *target;
	struct stack tmp;

	stack = current_stack();

	target = (dir == -1) ? prev_stack(stack) : next_stack(stack);
	if (target == NULL)
		target = (dir == -1) ? last_stack() : first_stack();
	assert(target != NULL);

	if (target == stack)
		return;

	stack_copy(&tmp, target);
	stack_copy(target, stack);
	stack_copy(stack, &tmp);

	focus_stack(target);

	resize_stacks();
	draw_stacks();
}

void
highlight_stacks(int i)
{
	TRACE_LOG("* %d", i);

	_highlight = i;
	if (_highlight) {
		TRACE_LOG("draw current stack...");
		draw_stack(current_stack());
	} else {
		TRACE_LOG("draw all stacks");
		draw_stacks();
	}
}

void
toggle_stacks_maxwidth_override()
{
	maxwidth_override ^= 1;
	resize_stacks();
}

void
toggle_sticky_stack()
{
	current_stack()->sticky ^= 1;
	draw_stack(current_stack());
}

void
toggle_hide_other_stacks()
{
	struct stack *np;

	TRACE_LOG("*");

	for (np = _head; np != NULL; np = np->next) {
		if (np->sticky)
			np->hidden = 0;
		else
			np->hidden ^= 1;
	}
	if (current_stack()->sticky)
		current_stack()->hidden = 0;
	else
		current_stack()->hidden ^= 1;

	for (np = _head; np != NULL; np = np->next)
		if (np->hidden)
			hide_stack(np);
		else
			show_stack(np);

	resize_stacks();
	draw_menu();
}

struct stack *
find_stack_xy(unsigned short x, unsigned short y)
{
	struct stack *np;

	for (np = _head; np != NULL; np = np->next)
		if (x >= np->x && x < np->x + np->width &&
		    y >= np->y && y < np->y + np->height)
			return np;

	return NULL;
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

	dpy = display();

	assert(BORDERWIDTH > 0);
	set_font(FONT_NORMAL);
	w = BORDERWIDTH;
	h = get_font_height();
	x = BORDERWIDTH;
	y = 0;
	v = CWBackPixel | CWOverrideRedirect;
	a.background_pixel = query_color(COLOR_TITLE_BG_NORMAL).pixel;
	a.override_redirect = True;
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
	char buf[1024], flags[10];
	size_t nclients;
	XGlyphInfo flags_extents;
	int x;

	if (stack == NULL || stack->hidden) {
		TRACE_LOG("not drawing this stack...");
		return;
	}

	dpy = display();
	client = find_top_client(stack);

	nclients = count_clients(stack);

	if (!stack->mapped)
		XMapWindow(dpy, stack->window);

	XRaiseWindow(dpy, stack->window);

	set_font(FONT_TITLE);

	if (_highlight && client != NULL && client->name != NULL)
		snprintf(buf, sizeof(buf), " stack %d: %s ", stack->num,
		    client->name);
	else if (_highlight)
		snprintf(buf, sizeof(buf), " stack %d ", stack->num);
	else if (client != NULL && client->name != NULL)
		snprintf(buf, sizeof(buf), " %s ", client->name);
	else
		buf[0] = '\0';

	if (nclients > 1)
		snprintf(flags, sizeof(flags), "+%zu ", nclients-1);
	else
		flags[0] = '\0';

	if (_highlight)
		snprintf(flags, sizeof(flags), "%c%c ",
		    stack->sticky ? 's' : '-',
		    stack->prefer_width ? 'w' : '-');

	font_extents(flags, strlen(flags), &flags_extents);

	XClearArea(display(), stack->window, 0, 0, stack->width,
	    get_font_height(), False);
	set_font_color(COLOR_TITLE_FG_NORMAL);
	x = draw_font(stack->window, 0, 0, buf);
	if (x > (stack->width - flags_extents.xOff))
		x = (stack->width - flags_extents.xOff);
	XClearArea(display(), stack->window, x, 0, stack->width - x,
	    get_font_height(), False);

	set_font_color(COLOR_FLAGS);
	draw_font(stack->window, stack->width - flags_extents.xOff,
	    0, flags);
}

void
draw_stacks()
{
	struct stack *np;

	for (np = first_stack(); np != NULL; np = next_stack(np))
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

	set_font(FONT_TITLE);
	XMoveResizeWindow(dpy, stack->window, stack->x, 0, stack->width,
	    get_font_height());
	resize_clients(stack);
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
	for (np = first_stack(); np != NULL; np = next_stack(np))
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
	for (np = first_stack(); np != NULL; np = next_stack(np)) {
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
	for (np = first_stack(); np != NULL; np = next_stack(np)) {
		width = stack_width(avg, np->prefer_width, surplus);
		total += width;
	}
	total_surplus = display_width() - BORDERWIDTH - total;

	/*
	 * Resize and move.
	 */
	x = BORDERWIDTH + (total_surplus/2);
	for (np = first_stack(); np != NULL; np = next_stack(np)) {
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

	stack = calloc(1, sizeof(struct stack));
	if (stack == NULL)
		return NULL;

	set_font(FONT_TITLE);
	stack->height = display_height() - get_font_height();
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

	resize_clients(stack);
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
	while ((client = next_client(client, NULL)) != NULL) {
		if (CLIENT_STACK(client) == stack) {
			CLIENT_STACK(client) = _focus;
		}
		if (CLIENT_REAPPEAR_STACK(client) == stack) {
			CLIENT_REAPPEAR_STACK(client) = _focus;
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

	TRACE_LOG("focus stack wants to focus client...");
	focus_client(find_top_client(stack), stack);

	if (prev != NULL && prev != _focus) {
		XSetWindowBackground(display(), prev->window,
		    query_color(COLOR_TITLE_BG_NORMAL).pixel);
		draw_stack(prev);
	}

	XSetWindowBackground(display(), stack->window,
	    query_color(COLOR_TITLE_BG_FOCUS).pixel);
	draw_stack(stack);
}

void
focus_stack_forward()
{
	struct stack *sp;
	int was_visible;

	was_visible = is_menu_visible();

	sp = current_stack();
	focus_stack(next_stack(sp) != NULL ? next_stack(sp) : first_stack());

	if (was_visible) {
		hide_menu();
		show_menu();
	}
}

void
focus_stack_backward()
{
	struct stack *sp;
	int was_visible;

	was_visible = is_menu_visible();

	sp = current_stack();
	focus_stack(prev_stack(sp) != NULL ? prev_stack(sp) : last_stack());

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
	TRACE_LOG("stack %s%d (width: %d, +%d+%d, mapped: %d, hidden: %d)",
	    (sp == _focus) ? "*" : " ",
	    sp->num, sp->width, sp->x, sp->y, sp->mapped, sp->hidden);
}

void
dump_stacks()
{
	struct stack *np;

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
