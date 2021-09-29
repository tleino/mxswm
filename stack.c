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

static struct stack *_head;
static struct stack *_focus;

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

struct stack *
find_stack(struct client *client)
{
	struct stack *np;

	for (np = _head; np != NULL; np = np->next) {
		if (np->client == client)
			return np;
	}

	return NULL;
}

void
resize_stack(struct stack *sp, unsigned short width)
{
	assert(sp != NULL);

	sp->width = width;
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

	width = display_width();
	width /= n;
	width -= (BORDERWIDTH * 2);
	x = 0;
	for (np = _head; np != NULL; np = np->next) {
		np->x = x;
		x += width;
		x += (BORDERWIDTH * 2);
		resize_stack(np, width);
		if (np->client != NULL)
			focus_client(np->client);
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
	struct stack *sp;

	sp = malloc(sizeof(struct stack));
	if (sp == NULL)
		return NULL;

	sp->height = display_height() - (BORDERWIDTH*2);
	sp->width = 0;
	sp->x = 0;
	sp->y = 0;
	sp->client = NULL;

	sp->prev = after;
	if (after != NULL) {
		sp->next = after->next;
		after->next = sp;
	} else {
		sp->next = _head;
		if (_head != NULL)
			_head->prev = sp;
		_head = sp;
	}

	if (_focus == NULL)
		focus_stack(sp);

	resize_stacks();
	renumber_stacks();
	return sp;
}

void
remove_stack(struct stack *sp)
{
	struct stack *np;
	struct client *client;

	focus_stack_forward();
	if (_focus == sp) {
		warnx("cannot remove last stack");
		return;
	}

	for (np = _head; np != NULL; np = np->next) {
		if (np != sp)
			continue;

		if (np->next != NULL)
			np->next->prev = np->prev;
		if (np->prev != NULL)
			np->prev->next = np->next;
		else if (np == _head)
			_head = np->next;
		free(sp);
		break;
	}

	client = NULL;
	while ((client = next_client(client)) != NULL)
		if (CLIENT_STACK(client) == sp)
			CLIENT_STACK(client) = NULL;

	resize_stacks();
	renumber_stacks();
}

void
focus_stack(struct stack *sp)
{
	_focus = sp;
	dump_stacks();
	focus_client(sp->client);
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
	struct stack *sp;

	if (_focus == NULL) {
		sp = add_stack(NULL);
		if (sp == NULL)
			err(1, "add_stack");
		focus_stack(sp);
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
