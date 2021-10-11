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

#ifndef MIN
#define MIN(_x, _y) ((_x) < (_y) ? (_x) : (_y))
#endif

#ifndef MAX
#define MAX(_x, _y) ((_x) > (_y) ? (_x) : (_y))
#endif

/*
 * These are sane defaults for a 2560x1440 screen.
 * Modify freely locally.
 */
#define BORDERWIDTH 28
#define FONTNAME \
	"-xos4-terminus-medium-r-normal--28-280-72-72-c-140-iso10646-1"
#define TITLEFONTNAME \
	"-xos4-terminus-bold-r-normal--28-280-72-72-c-140-iso10646-1"
#define FALLBACKFONT \
	"-misc-fixed-medium-r-normal--20-200-75-75-c-100-iso10646-1"

/*
 * For 80-column terminals.
 */
#if WANT_MAXWIDTH
#define FONTWIDTH 14
#define MAXWIDTH (FONTWIDTH * 80)
#endif

/*
 * TODO: These are purely random placeholder colors,
 *       replace them with XAllocNamedColor or similar.
 */
#define TITLEBAR_FOCUS_COLOR 434545456
#define TITLEBAR_NORMAL_COLOR 5485488

struct client;

struct stack {
	unsigned short width;
	unsigned short height;
	unsigned short x;
	unsigned short y;
	unsigned short num;
	struct stack *next;
	struct stack *prev;
	Window window;
	int maxwidth_override;
	int prefer_width;
};

struct client {
	Window window;
	char *name;
	struct stack *stack;
	struct client *next;
	struct client *prev;
};

#define STACK_WIDTH(_x) (_x)->width
#define STACK_HEIGHT(_x) (_x)->height
#define STACK_X(_x) (_x)->x
#define STACK_Y(_x) (_x)->y

#define CLIENT_STACK(_x) (_x)->stack

struct stack *add_stack(struct stack *);
void remove_stack(struct stack *);
void draw_stack(struct stack *);
void add_stack_here(void);
void remove_stack_here(void);
void resize_stacks(void);
void focus_stack(struct stack *);
void focus_stack_forward(void);
void focus_stack_backward(void);
struct stack *current_stack(void);
void resize_stack(struct stack *, unsigned short);
struct stack *find_stack(int);
struct stack *find_stack_xy(unsigned short, unsigned short);
void resize_client(struct client *);
void toggle_stacks_maxwidth_override(void);
void highlight_stacks(int);
void draw_stacks(void);
void move_stack(int);

#if TRACE
void dump_stack(struct stack *);
void dump_stacks(void);
#else
#define dump_stacks(x) do { } while(0)
#define dump_stack(x) do { } while(0)
#endif

struct client *add_client(Window, struct client *);
struct client *find_client(Window);
struct client *have_client(Window);
void remove_client(struct client *);
void top_client(struct client *);
void focus_client(struct client *, struct stack *);
void focus_client_forward(void);
void focus_client_backward(void);
void focus_client_cycle_here(void);
struct client *current_client(void);
struct client *next_client(struct client *);
struct client *prev_client(struct client *);
char *client_name(struct client *);
struct client *find_top_client(struct stack *);
void update_client_name(struct client *);

unsigned short display_height(void);
unsigned short display_width(void);
Display *display(void);

int manageable(Window);

int handle_event(XEvent *);

void open_menu(void);
void draw_menu(void);
void close_menu(void);
void focus_menu_forward(void);
void focus_menu_backward(void);
void select_menu_item(void);
void select_move_menu_item(void);
void select_menu_item_right(void);
void select_menu_item_left(void);
void move_menu_item_right(void);
void move_menu_item_left(void);
void show_menu(void);
void hide_menu(void);
int is_menu_visible(void);
void highlight_menu(int);

void do_keyaction(XKeyEvent *);
void unbind_keys();
void bind_keys();

#if WANT_CTLSOCKET
int listen_ctlsocket(void);
void run_ctlsocket_event_loop(int);
void run_ctl_line(const char *);
#endif

#if TRACE
void dump_client(struct client *);
void dump_clients(void);
#else
#define dump_clients(x) do { } while(0)
#define dump_client(x) do { } while(0)
#endif

#endif
