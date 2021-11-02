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
#include <X11/keysym.h>
#include <err.h>

#define DONT_CHECK_MASK -1

struct binding {
	KeySym keysym;
	int modifiermask;
	void (*press)(void);
	void (*release)(void);
};

static void _unbind_keys(Display *, Window, const struct binding *, size_t);
static void _bind_keys(Display *, Window, const struct binding *, size_t);
void _do_keyaction(XKeyEvent *, const struct binding *, size_t);
static void control_off(void);
static void control_on(void);
static void win_on(void);

/*
 * Assuming normal PC keyboard where Mod4Mask corresponds to Win key,
 * and Mod1Mask to Alt key.
 */
static const struct binding binding[] = {
	{
		XK_Left, Mod4Mask | ControlMask,
		NULL, move_menu_item_left
	},
	{
		XK_Right, Mod4Mask | ControlMask,
		NULL, move_menu_item_right
	},
	{
		XK_Left, Mod4Mask,
		NULL, focus_stack_backward
	},
	{
		XK_Right, Mod4Mask,
		NULL, focus_stack_forward
	},
	{
		XK_Control_L, Mod4Mask | ControlMask,
		NULL, control_off
	},
	{
		XK_Control_R, Mod4Mask | ControlMask,
		NULL, control_off
	},
	{
		XK_Control_L, Mod4Mask,
		control_on, NULL
	},
	{
		XK_Control_R, Mod4Mask,
		control_on, NULL
	},
	{
		XK_Super_L, 0,
		win_on, NULL
	},
	{
		XK_Super_R, 0,
		win_on, NULL
	},
	{
		XK_Super_L, Mod4Mask,
		NULL, select_move_menu_item
	},
	{
		XK_Super_R, Mod4Mask,
		NULL, select_move_menu_item
	},
	{
		XK_Menu, 0,
		NULL, focus_client_forward
	},
	{
		XK_Down, Mod4Mask,
		NULL, focus_menu_forward
	},
	{
		XK_Up, Mod4Mask,
		NULL, focus_menu_backward
	},
	{
		XK_F1, 0,
		NULL, remove_stack_here
	},
	{
		XK_F2, 0,
		NULL, add_stack_here
	},
	{
		XK_F3, 0,
		NULL, toggle_stacks_maxwidth_override
	},
	{
		XK_h, Mod4Mask,
		toggle_hide_other_stacks, NULL
	},
	{
		XK_s, Mod4Mask,
		NULL, toggle_sticky_stack
	},
};

static int menu;

static void
control_off()
{
	highlight_menu(0);
}

static void
control_on()
{
	highlight_menu(1);
}

static void
win_on()
{
	highlight_stacks(1);
}

void
close_menu()
{
	menu = 0;
	hide_menu();
}

void
open_menu()
{
	menu = 1;
	show_menu();
}

void
do_keyaction(XKeyEvent *xkey)
{
	_do_keyaction(xkey, binding, ARRLEN(binding));
}

void
_do_keyaction(XKeyEvent *xkey, const struct binding *bindings, size_t sz)
{
	int i, mask;
	KeySym sym;
	const struct binding *kb;

	sym = XLookupKeysym(xkey, 0);
	mask = xkey->state;

	TRACE_LOG("sym=%lu", sym);

	for (i = 0; i < sz; i++) {
		kb = &(bindings[i]);
		if (kb->keysym == sym &&
		   (kb->modifiermask == mask ||
		   kb->modifiermask == DONT_CHECK_MASK)) {
			if (xkey->type == KeyPress) {
				if (kb->press != NULL)
					kb->press();
			} else {
				if (kb->release != NULL)
					kb->release();
			}
			return;
		}
	}

	warnx("binding not found sym=%lx mask=%d type=%s", sym, mask,
	    xkey->type == KeyPress ? "KeyPress" : "KeyRelease");
}

static void
_bind_keys(Display *display, Window root, const struct binding *bindings,
    size_t sz)
{
	int i, code, mask, mouse, kbd;
	Bool owner_events;

	for (i = 0; i < sz; i++) {
		code = XKeysymToKeycode(display, bindings[i].keysym);
		if (bindings[i].modifiermask == DONT_CHECK_MASK)
			mask = 0;
		else
			mask = bindings[i].modifiermask;
		mouse = GrabModeSync;
		kbd = GrabModeAsync;
		owner_events = False;
		XGrabKey(display, code, mask, root, owner_events, mouse, kbd);
	}
}

static void
_unbind_keys(Display *display, Window root,
    const struct binding *bindings, size_t sz)
{
	int i, code, mask;

	for (i = 0; i < sz; i++) {
		code = XKeysymToKeycode(display, bindings[i].keysym);
		if (bindings[i].modifiermask == DONT_CHECK_MASK)
			mask = 0;
		else
			mask = bindings[i].modifiermask;
		XUngrabKey(display, code, mask, root);
	}
}

void
unbind_keys()
{
	_unbind_keys(display(), DefaultRootWindow(display()), binding,
	    ARRLEN(binding));
}

void
bind_keys()
{
	_bind_keys(display(), DefaultRootWindow(display()), binding,
	    ARRLEN(binding));
}
