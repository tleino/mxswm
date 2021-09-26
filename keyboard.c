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
	void (*kbfun)(void);
};

/*
 * Assuming normal PC keyboard where Mod4Mask corresponds to Win key,
 * and Mod1Mask to Alt key.
 */
static const struct binding keybinding[] = {
	{ XK_Super_L, DONT_CHECK_MASK,
	    focus_stack_backward },
	{ XK_Super_R, DONT_CHECK_MASK,
	    focus_stack_forward },
	{ XK_Menu, 0, toggle_menu },
	{ XK_F1, 0, add_stack_here },
	{ XK_F2, 0, remove_stack_here },
};

static const struct binding menubinding[] = {
	{ XK_Menu, 0, toggle_menu },
	{ XK_Up, 0, focus_menu_backward },
	{ XK_Down, 0, focus_menu_forward },
	{ XK_Return, 0, select_menu_item },
};

static void _unbind_keys(Display *, Window, const struct binding *, size_t sz);
static void _bind_keys(Display *, Window, const struct binding *, size_t sz);
void _do_keyaction(XKeyEvent *xkey, const struct binding *bindings, size_t sz);

static int menu = 0;

void
toggle_menu()
{
	menu ^= 1;

	if (menu) {
		show_menu();
		_unbind_keys(display(),
		    DefaultRootWindow(display()), keybinding,
		    ARRLEN(keybinding));
		_bind_keys(display(),
		    DefaultRootWindow(display()), menubinding,
		    ARRLEN(menubinding));
	} else {
		hide_menu();
		_unbind_keys(display(),
		    DefaultRootWindow(display()), menubinding,
		    ARRLEN(menubinding));
		_bind_keys(display(),
		    DefaultRootWindow(display()), keybinding,
		    ARRLEN(keybinding));
	}
}

void
do_keyaction(XKeyEvent *xkey)
{
	if (menu) {
		_do_keyaction(xkey, menubinding, ARRLEN(menubinding));
	} else {
		_do_keyaction(xkey, keybinding, ARRLEN(keybinding));
	}
}

void
_do_keyaction(XKeyEvent *xkey, const struct binding *bindings, size_t sz)
{
	int i, mask;
	KeySym sym;
	const struct binding *kb;

	sym = XLookupKeysym(xkey, 0);
	mask = xkey->state;
	for (i = 0; i < sz; i++) {
		kb = &(bindings[i]);
		if (kb->keysym == sym &&
		   (kb->modifiermask == mask ||
		   kb->modifiermask == DONT_CHECK_MASK)) {
			kb->kbfun();
			return;
		}
	}

	warnx("binding not found");
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
	_unbind_keys(display(), DefaultRootWindow(display()), keybinding,
	    ARRLEN(keybinding));
}

void
bind_keys()
{
	_bind_keys(display(), DefaultRootWindow(display()), keybinding,
	    ARRLEN(keybinding));
}
