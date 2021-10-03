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

#include <X11/Xutil.h>
#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

static struct client *_head;
static struct client *_focus;

static void transition_client_state(struct client *, unsigned long);

void
update_client_name(struct client *client)
{
	XTextProperty text;

	if (XGetWMName(display(), client->window, &text) == 0) {
		warnx("unable to get name");
	} else {
		if (client->name != NULL)
			free(client->name);
		client->name = malloc(text.nitems + 1);
		if (client->name != NULL) {
			memcpy(client->name, text.value, text.nitems);
			client->name[text.nitems] = 0;
		}
	}
}

struct client *
add_client(Window window, struct client *after)
{
	struct client *client;

	client = malloc(sizeof(struct client));
	if (client == NULL)
		return NULL;

	client->window = window;
	client->name = NULL;
	client->stack = current_stack();

	client->prev = after;
	if (after != NULL) {
		/*
		 * Insert after 'after'.
		 */
		client->next = after->next;
		after->next = client;
		if (client->next != NULL)
			client->next->prev = client;
	} else {
		/*
		 * Insert to head.
		 */
		client->next = _head;
		_head = client;
		if (_head->next != NULL)
			_head->next->prev = _head;
	}

	XSetWindowBorderWidth(display(), window, 0);

	XSelectInput(display(), window, PropertyChangeMask);

	update_client_name(client);
	focus_client(client, NULL);
	draw_menu();

	return client;
}

char *
client_name(struct client *client)
{
	return client->name;
}

struct client *
have_client(Window window)
{
	struct client *np;

	for (np = _head; np != NULL; np = np->next) {
		if (np->window != window)
			continue;

		return np;
	}

	return NULL;
}

struct client *
find_client(Window window)
{
	struct client *np;

	for (np = _head; np != NULL; np = np->next) {
		if (np->window != window)
			continue;

		return np;
	}

	return add_client(window, NULL);
}

struct client *
next_client(struct client *prev)
{
	if (prev == NULL)
		return _head;
	else
		return prev->next;
}

struct client *
prev_client(struct client *client)
{
	struct client *np;

	if (client == NULL)
		return _head;
	if (client->prev == NULL) {
		np = client;
		while (np && np->next)
			np = np->next;
		return np;
	}
	return client->prev;
}

void
remove_client(struct client *client)
{
	focus_menu_forward();

	if (client->next != NULL)
		client->next->prev = client->prev;

	if (client->prev != NULL)
		client->prev->next = client->next;
	else if (client == _head)
		_head = client->next;

	_focus = find_top_client(client->stack);
	focus_client(_focus, client->stack);

	if (client->name != NULL)
		free(client->name);
	free(client);

	draw_menu();
}

struct client *
find_top_client(struct stack *stack)
{
	struct client *np;

	for (np = _head; np != NULL; np = np->next)
		if (np->stack == stack)
			return np;

	return NULL;
}

void
top_client(struct client *client)
{
	if (client == NULL)
		return;
	if (_head == client)
		return;

	if (client->next != NULL)
		client->next->prev = client->prev;
	if (client->prev != NULL)
		client->prev->next = client->next;

	client->prev = NULL;
	client->next = _head;
	_head->prev = client;
	_head = client;
}

void
resize_client(struct client *client)
{
	Display *dpy;
	struct stack *stack;
	Window window;

	dpy = display();

	if (client == NULL)
		return;

	assert(client->stack != NULL);
	stack = client->stack;

	window = client->window;
	XMoveWindow(dpy, window, STACK_X(stack), STACK_Y(stack) + BORDERWIDTH);
	XResizeWindow(dpy, window, STACK_WIDTH(stack), STACK_HEIGHT(stack));
}

void
focus_client(struct client *client, struct stack *stack)
{
	Display *dpy = display();
	struct client *prev;
	Window window;

	if (client == NULL)
		return;

	if (stack == NULL)
		stack = current_stack();

	client->stack = stack;
	top_client(client);

	if (stack != current_stack()) {
		focus_stack(stack);
		return;
	}

	prev = _focus;
	_focus = client;
	window = client->window;

	transition_client_state(client, NormalState);
	resize_client(client);

	XRaiseWindow(dpy, window);
	XSetInputFocus(dpy, window, RevertToPointerRoot, CurrentTime);

	if (prev != NULL && prev != client) {
		window = prev->window;
		/*
		 * Don't iconify previously focused client if it is still
		 * visible in another stack.
		 */
		if (find_top_client(prev->stack) != prev)
			transition_client_state(prev, IconicState);
	}

	draw_stack(client->stack);
}

void
focus_client_cycle_here()
{
	struct client *client, *np;
	struct stack *stack;

	client = current_client();
	if (client == NULL)
		return;

	stack = current_stack();
	/*
	 * Find next client that has the same stack.
	 */
	for (np = client->next; np != NULL; np = np->next) {
		if (np->stack == stack) {
			focus_client(np, NULL);
			return;
		}
	}
	for (np = _head; np != client && np != NULL; np = np->next) {
		if (np->stack == stack) {
			focus_client(np, NULL);
			return;
		}
	}
}

void
focus_client_forward()
{
	struct client *client;

	client = current_client();
	if (client == NULL)
		return;

	focus_client(client->next != NULL ? client->next : _head, NULL);
}

void
focus_client_backward()
{
	struct client *client, *np;

	client = current_client();
	if (client == NULL)
		return;

	if (client->prev != NULL)
		np = client->prev;
	else {
		np = client;
		while (np->next != NULL)
			np = np->next;
		assert(np != NULL);
	}

	focus_client(np, NULL);
}

struct client *
current_client()
{
	if (_head != NULL && _focus == NULL)
		focus_client(_head, NULL);

	return _focus;
}

static void
transition_client_state(struct client *client, unsigned long state)
{
	unsigned long data[2];
	Atom wmstate;
	Display *dpy = display();

	data[0] = state;
	data[1] = None;

	wmstate = XInternAtom(dpy, "WM_STATE", False);

	XChangeProperty (dpy, client->window, wmstate, wmstate, 32,
	    PropModeReplace, (unsigned char *) data, 2);
	if (state == IconicState)
		XUnmapWindow(dpy, client->window);
	else if (state == NormalState)
		XMapWindow(dpy, client->window);
	XSync(dpy, False);
}

#if (TRACE || TEST)
#include <inttypes.h>
#include <stdio.h>
void
dump_client(struct client *client)
{
	printf("client: %ju", (uintmax_t) client);
	if (client == _focus)
		printf(" (focused)");
	putchar('\n');
}

void
dump_clients()
{
	struct client *np;

	printf("focus: %ju\n", (uintmax_t) current_client());
	for (np = _head; np != NULL; np = np->next)
		dump_client(np);
}
#endif
#if TEST
int
main(int argc, char *argv[])
{
	add_client(NULL);
	add_client(NULL);
	remove_client(current_client());
	add_client(current_client());
	dump_clients();
}
#endif
