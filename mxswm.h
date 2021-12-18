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
#include <X11/extensions/Xrender.h>

#ifdef TRACE
#include <stdio.h>
#include <time.h>
const char *str_event(XEvent *);
const char *current_event(void);
Window current_window(void);
time_t start_time();

#define TRACE_LOG(...)  \
	do { \
		fprintf(stderr, "%d %-16s 0x%08lx %-16s %-16s ", \
		    (int) (time(0) - start_time()), \
		    current_event(), current_window(), \
		    event_client() ? event_client()->name : \
		    "", __FUNCTION__); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "\n"); \
	} while(0)
#define TRACE_SET_CLIENT(client) set_event_client(client)
#define EVENT_STR(event) str_event(event)
#else
#define TRACE_LOG(...)   ((void) 0)
#define EVENT_STR(...)   ((void) 0)
#define TRACE_SET_CLIENT(...)	((void) 0)
#endif

#ifndef ARRLEN
#define ARRLEN(_x) sizeof((_x)) / sizeof((_x)[0])
#endif

#ifndef MIN
#define MIN(_x, _y) ((_x) < (_y) ? (_x) : (_y))
#endif

#ifndef MAX
#define MAX(_x, _y) ((_x) > (_y) ? (_x) : (_y))
#endif

enum ewmh {
	WM_PROTOCOLS=0,
	_NET_WM_NAME,
	UTF8_STRING,
	NUM_WMH
};

extern Atom wmh[NUM_WMH];

void init_wmh(void);

#include "fontnames.h"
#include "colornames.h"

/*
 * These are sane defaults for a 2560x1440 screen.
 * Modify freely locally.
 */
#define BORDERWIDTH 28

/*
 * For 80-column terminals.
 */
#if WANT_MAXWIDTH
#define FONTWIDTH 14
#define MAXWIDTH (FONTWIDTH * 80)
#endif

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
	int hidden;
	int sticky;
	int mapped;
};

struct client {
	Window window;
	char *name;
	int mapped;
#define CF_HAS_TAKEFOCUS (1 << 0)
#define CF_HAS_DELWIN (1 << 1)
#define CF_FOCUS_WHEN_MAPPED (1 << 2)
	int flags;
	struct stack *stack;
	struct stack *reappear;
	struct client *next;
	struct client *prev;
};

#define STACK_WIDTH(_x) (_x)->width
#define STACK_HEIGHT(_x) (_x)->height
#define STACK_X(_x) (_x)->x
#define STACK_Y(_x) (_x)->y

#define CLIENT_STACK(_x) (_x)->stack
#define CLIENT_REAPPEAR_STACK(_x) (_x)->reappear

struct stack *add_stack(struct stack *);
struct stack *have_stack(Window);
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
void resize_clients(struct stack *);
size_t count_clients(struct stack *);
void toggle_stacks_maxwidth_override(void);
void toggle_hide_other_stacks(void);
void toggle_sticky_stack(void);
void highlight_stacks(int);
void draw_stacks(void);
void move_stack(int);

void read_protocols(struct client *);
void send_delete_window(struct client *);
void send_take_focus(struct client *);

#if TRACE
void dump_stack(struct stack *);
void dump_stacks(void);
#else
#define dump_stacks(x) do { } while(0)
#define dump_stack(x) do { } while(0)
#endif

struct client *add_client(Window, struct client *, int);
struct client *have_client(Window);
void remove_client(struct client *);
void top_client(struct client *);
void focus_client(struct client *, struct stack *);
void focus_client_forward(void);
void focus_client_backward(void);
void focus_client_cycle_here(void);
struct client *current_client(void);
struct client *next_client(struct client *, struct stack *);
struct client *prev_client(struct client *, struct stack *);
char *client_name(struct client *);
struct client *find_top_client(struct stack *);
void unmap_clients(struct stack *);
void map_clients(struct stack *);
void update_client_name(struct client *);
void delete_client(void);
void destroy_client(void);

unsigned short display_height(void);
unsigned short display_width(void);
Display *display(void);

int handle_event(XEvent *);

void open_menu(void);
void draw_menu(void);
void close_menu(void);

void draw_global_menu(void);
void open_global_menu(void);
void close_global_menu(void);

void focus_menu_forward(void);
void focus_menu_backward(void);

void select_menu_item(void);
void select_next_global_menu(void);
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

void set_font_color(int);
void set_font(int);
int get_font_height(void);
void draw_font(Window, int, int, const char *);
void font_extents(const char *, XGlyphInfo *);

XColor query_color(int);

Time current_event_timestamp(void);

void open_prompt(void);

#ifdef TRACE
struct client *event_client();
void set_event_client(struct client *);
#endif

#if WANT_CTLSOCKET
int listen_ctlsocket(void);
void run_ctlsocket_event_loop(int);
#endif
void run_ctl_line(const char *);
void run_ctl_lines(void);

#if TRACE
void dump_client(struct client *);
void dump_clients(void);
#else
#define dump_clients(x) do { } while(0)
#define dump_client(x) do { } while(0)
#endif

#endif
