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

struct client {
	Window window;
	char *name;
	struct client *next;
	struct client *prev;
};

static struct client *_head;
static struct client *_focus;

static void transition_client_state(struct client *, unsigned long);

struct client *
add_client(Window window, struct client *after)
{
	struct client *client;

	client = malloc(sizeof(struct client));
	if (client == NULL)
		return NULL;

	client->window = window;

	client->prev = after;
	if (after != NULL) {
		client->next = after->next;
		after->next = client;
	} else {
		client->next = _head;
		if (_head != NULL)
			_head->prev = client;
		_head = client;
	}

	if (XFetchName(display(), window, &client->name) == 0)
		warnx("unable to fetch name");

	focus_client(client);
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
	struct client *np;

	focus_client_forward();

	for (np = _head; np != NULL; np = np->next) {
		if (np != client)
			continue;

		if (np->next != NULL)
			np->next->prev = np->prev;
		if (np->prev != NULL)
			np->prev->next = np->next;
		else if (np == _head)
			_head = np->next;
		free(client);
		break;
	}
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
focus_client(struct client *client)
{
	Display *dpy = display();
	struct stack *stack = current_stack();
	Window window;

	if (_focus != NULL) {
		window = _focus->window;
		XSetWindowBorderWidth(dpy, window, BORDERWIDTH);
		XSetWindowBorder(dpy, window, 5485488);
		if (stack->client == _focus)
			transition_client_state(_focus, IconicState);
	}

	_focus = client;
	if (_focus == NULL)
		return;

	window = _focus->window;
	transition_client_state(client, NormalState);
	XMoveWindow(dpy, window, STACK_X(stack), STACK_Y(stack));
	XResizeWindow(dpy, window, STACK_WIDTH(stack), STACK_HEIGHT(stack));
	XSetWindowBorderWidth(dpy, window, BORDERWIDTH);
	XSetWindowBorder(dpy, window, 434545456);
	XRaiseWindow(dpy, window);
	XSetInputFocus(dpy, window, RevertToPointerRoot, CurrentTime);
	stack->client = client;

	top_client(client);
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
	for (np = client->next; np != NULL; np = np->next) {
		if (find_stack(np) != NULL)
			continue;

		focus_client(np);
		return;
	}
	for (np = _head; np != client && np != NULL; np = np->next) {
		if (find_stack(np) != NULL)
			continue;

		focus_client(np);
		return;
	}
}

void
focus_client_forward()
{
	struct client *client;

	client = current_client();
	if (client == NULL)
		return;

	focus_client(client->next != NULL ? client->next : _head);
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

	focus_client(np);
}

struct client *
current_client()
{
	if (_head != NULL && _focus == NULL)
		focus_client(_head);

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
