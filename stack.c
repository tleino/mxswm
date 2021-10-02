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

static struct stack *_head;
static struct stack *_focus;

static GC _gc;

static void create_stack_titlebar(struct stack *);

static void
create_stack_titlebar(struct stack *stack)
{
	int x, y, w, h;
	XSetWindowAttributes a;
	unsigned long v;
	Display *dpy;
	XGCValues gcv;

	dpy = display();

	if (_gc == 0) {
		gcv.foreground = BlackPixel(dpy, DefaultScreen(dpy));
		gcv.font = XLoadFont(dpy, TITLEFONTNAME);
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

	dpy = display();
	client = find_top_client(stack);

	XClearWindow(dpy, stack->window);
	XRaiseWindow(dpy, stack->window);
	if (client != NULL && client->name != NULL)
		XDrawString(display(), stack->window, _gc, 0, 22,
		    client->name, strlen(client->name));
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

void
resize_stacks()
{
	struct stack *np;
	size_t n, x;
	unsigned short width;

	n = 0;
	for (np = _head; np != NULL; np = np->next)
		n++;
	assert(n > 0);

	width = display_width() - BORDERWIDTH;
	width /= n;
	width -= BORDERWIDTH;
	x = 0;
	for (np = _head; np != NULL; np = np->next) {
		x += BORDERWIDTH;
		np->x = x;
		x += width;
		resize_stack(np, width);
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

	stack = malloc(sizeof(struct stack));
	if (stack == NULL)
		return NULL;

	stack->height = display_height() - BORDERWIDTH;
	stack->width = 0;
	stack->x = 0;
	stack->y = 0;

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

	client = NULL;
	while ((client = next_client(client)) != NULL)
		if (CLIENT_STACK(client) == stack)
			CLIENT_STACK(client) = _focus;

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

	sp = current_stack();
	focus_stack(sp->next != NULL ? sp->next : _head);
}

void
focus_stack_backward()
{
	struct stack *sp, *np;

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
