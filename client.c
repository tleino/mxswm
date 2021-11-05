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
#include <X11/Xatom.h>
#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

static struct client *_head;
static struct client *_focus;

static void try_utf8_name(struct client *);

static void
try_utf8_name(struct client *client)
{
	Atom u8name, u8type, type;
	unsigned char *name;
	int form;
	unsigned long remain, len;

	u8type = XInternAtom(display(), "UTF8_STRING", False);
	u8name = XInternAtom(display(), "_NET_WM_NAME", False);
	name = NULL;

	if (XGetWindowProperty(display(), client->window,
	    u8name, 0, 1024, False, u8type, &type, &form,
	    &len, &remain, &name) == Success) {
		TRACE_LOG("_NET_WM_NAME");
		if (name != NULL) {
			if (client->name != NULL)
				free(client->name);
			client->name = strdup(name);
		}
	}
}

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

	try_utf8_name(client);

	if (client->name != NULL && client->name[0] == '\0') {
		free(client->name);
		client->name = strdup("(no name)");
	}
}

struct client *
add_client(Window window, struct client *after, int mapped)
{
	struct client *client;

	TRACE_LOG("%lx mapped=%d", window, mapped);
	client = calloc(1, sizeof(struct client));
	if (client == NULL)
		return NULL;

	client->window = window;
	client->name = NULL;
	client->stack = current_stack();
	assert(mapped == 0 || mapped == 1);
	client->mapped = mapped;

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

	read_protocols(client);

	XSelectInput(display(), window, PropertyChangeMask);

	if (mapped) {
		XSetWindowBorderWidth(display(), window, 0);
		focus_client(client, NULL);
		draw_menu();
	} else
		client->flags |= CF_FOCUS_WHEN_MAPPED;

	update_client_name(client);
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
next_client(struct client *client, struct stack *stack)
{
	struct client *np;

	np = client;
	if (np == NULL)
		np = _head;
	else
		np = np->next;

	for (; np != NULL; np = np->next)
		if (np->mapped && (stack == NULL || np->stack == stack))
			break;

	return np;
}

struct client *
prev_client(struct client *client, struct stack *stack)
{
	struct client *np;

	np = client;
	if (np == NULL)
		return next_client(NULL, stack);

	for (np = np->prev; np != NULL; np = np->prev)
		if (np->mapped && (stack == NULL || np->stack == stack))
			break;

	return np;
}

void
remove_client(struct client *client)
{
	TRACE_LOG("");
	close_menu();

	if (client->next != NULL)
		client->next->prev = client->prev;

	if (client->prev != NULL)
		client->prev->next = client->next;
	else if (client == _head)
		_head = client->next;

	_focus = find_top_client(client->stack);
	focus_client(_focus, client->stack);

	TRACE_SET_CLIENT(NULL);
	if (client->name != NULL) {
		free(client->name);
		client->name = NULL;
	}
	free(client);

	TRACE_LOG("draw menu");
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

	if (client == NULL || !client->mapped)
		return;

	assert(client->stack != NULL);
	stack = client->stack;

	window = client->window;
	XMoveWindow(dpy, window, STACK_X(stack), STACK_Y(stack) + BORDERWIDTH);
	XResizeWindow(dpy, window, STACK_WIDTH(stack), STACK_HEIGHT(stack));
}

size_t
count_clients(struct stack *stack)
{
	struct client *np;
	size_t n;

	n = 0;
	for (np = _head; np != NULL; np = np->next)
		if (np->stack == stack && np->mapped)
			n++;

	return n;
}

void
resize_clients(struct stack *stack)
{
	struct client *np;

	for (np = _head; np != NULL; np = np->next)
		if (np->stack == stack && np->mapped)
			resize_client(np);
}

void
focus_client(struct client *client, struct stack *stack)
{
	Display *dpy = display();
	struct client *prev;
	Window window;

	if (client == NULL || !client->mapped)
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

	if (client->flags & CF_HAS_TAKEFOCUS) {
		TRACE_LOG("Sending TAKEFOCUS");
		send_take_focus(client);
	} else {
		TRACE_LOG("Normal input focus");
		XSetInputFocus(dpy, window, RevertToPointerRoot, CurrentTime);
	}

	resize_client(client);

	XRaiseWindow(dpy, window);

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
	struct client *prev, *client;

	prev = current_client();
	if (prev == NULL)
		return;

	client = prev->next != NULL ? prev->next : _head;

	focus_client(client, client->stack);
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

void
unmap_clients(struct stack *stack)
{
	struct client *np;

	for (np = _head; np != NULL; np = np->next)
		if (np->stack == stack && np->mapped)
			XUnmapWindow(display(), np->window);
}

void
map_clients(struct stack *stack)
{
	struct client *np;

	for (np = _head; np != NULL; np = np->next)
		if (np->stack == stack && !np->mapped)
			XMapWindow(display(), np->window);
}

struct client *
current_client()
{
	if (_head != NULL && _focus == NULL)
		focus_client(_head, NULL);

	return _focus;
}

#if (TRACE || TEST)
#include <inttypes.h>
#include <stdio.h>
void
dump_client(struct client *client)
{
	TRACE_LOG("0x%08lx (%c%c) %s (stack %llu)", client->window,
	    (client == _focus) ? 'f' : '-',
	    (client->mapped) ? 'm' : '-',
	    client->name != NULL ? client->name : "<no name>",
	    (unsigned long long) client->stack);
}

void
dump_clients()
{
	struct client *np;

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
