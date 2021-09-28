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

#ifndef MXSWM_H
#define MXSWM_H

#include <X11/Xlib.h>

#ifndef ARRLEN
#define ARRLEN(_x) sizeof((_x)) / sizeof((_x)[0])
#endif

#define BORDERWIDTH 28
#define FONTNAME \
	"-xos4-terminus-medium-r-normal--28-280-72-72-c-140-iso10646-1"

struct client;

struct stack {
	unsigned short width;
	unsigned short height;
	unsigned short x;
	unsigned short y;
	struct stack *next;
	struct stack *prev;
	struct client *client;
};

#define STACK_WIDTH(_x) (_x)->width
#define STACK_HEIGHT(_x) (_x)->height
#define STACK_X(_x) (_x)->x
#define STACK_Y(_x) (_x)->y

struct stack *add_stack(struct stack *);
void remove_stack(struct stack *);
void add_stack_here(void);
void remove_stack_here(void);
void resize_stacks(void);
void focus_stack(struct stack *);
void focus_stack_forward(void);
void focus_stack_backward(void);
struct stack *current_stack(void);
void resize_stack(struct stack *, unsigned short);
struct stack *find_stack(struct client *);

#if TRACE
void dump_stack(struct stack *);
void dump_stacks(void);
#endif

struct client *add_client(Window, struct client *);
struct client *find_client(Window);
void remove_client(struct client *);
void focus_client(struct client *);
void focus_client_forward(void);
void focus_client_backward(void);
struct client *current_client(void);
struct client *next_client(struct client *);
struct client *prev_client(struct client *);
char *client_name(struct client *);

unsigned short display_height(void);
unsigned short display_width(void);
Display *display(void);

int handle_event(XEvent *);

void open_menu(void);
void close_menu(void);
void focus_menu_forward(void);
void focus_menu_backward(void);
void select_menu_item(void);
void show_menu(void);
void hide_menu(void);

void do_keyaction(XKeyEvent *);
void unbind_keys();
void bind_keys();

#if TRACE
void dump_client(struct client *);
void dump_clients(void);
#endif

#endif
