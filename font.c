/*
 * mxswm - MaXed Stacks Window Manager for X11
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

/* font.c: Implements Xft(3) font and text drawing support. */

#include "mxswm.h"

#include <X11/Xft/Xft.h>

#include <limits.h>
#include <assert.h>
#include <err.h>

#include "fontnames.c"

static XftColor ftcolor;
static XftFont *ftfont[NUM_FONT];
static XftFont *current_font;
static XftDraw *ftdraw;

static XftFont *load_font(int);

/*
 * Sets font color by reusing named/enum-defined colors from color.c so
 * that we can use same color defines in font and non-font stuff.
 */
void
set_font_color(int color)
{
	XColor xcolor;

	xcolor = query_color(color);

	ftcolor.pixel = xcolor.pixel;
	ftcolor.color.red = xcolor.red;
	ftcolor.color.green = xcolor.green;
	ftcolor.color.blue = xcolor.blue;
	ftcolor.color.alpha = USHRT_MAX;
}

int
get_font_height()
{
	assert(current_font != NULL);

	return current_font->height;	
}

void
set_font(int id)
{
	assert (id < NUM_FONT);

	if (ftfont[id] == NULL)
		ftfont[id] = load_font(id);

	current_font = ftfont[id];
}

void
font_extents(const char *text, XGlyphInfo *extents)
{
	XftTextExtentsUtf8(display(), current_font,
	    (const FcChar8 *) text, strlen(text), extents);
}

void
draw_font(Window window, int x, int y, const char *text)
{
	if (ftdraw == NULL)
		if ((ftdraw = XftDrawCreate(display(), window,
		    DefaultVisual(display(), DefaultScreen(display())),
		    DefaultColormap(display(), DefaultScreen(display())))) ==
		    NULL)
			errx(1, "XftDrawCreate failed");

	if (XftDrawDrawable(ftdraw) != window)
		XftDrawChange(ftdraw, window);

	XftDrawStringUtf8(ftdraw, &ftcolor, current_font, x,
	    y + current_font->ascent, (const FcChar8 *) text, strlen(text));
}

static XftFont *
load_font(int id)
{
	/*
	 * First try Xlfd form, then Xft font name form.
	 */
	ftfont[id] = XftFontOpenXlfd(display(),
	    DefaultScreen(display()), fontname[id]);
	if (ftfont[id] == NULL)
		ftfont[id] = XftFontOpenName(display(),
		    DefaultScreen(display()), fontname[id]);

	/*
	 * No success? Try the fallback font.
	 */
	if (ftfont[id] == NULL && id != FONT_FALLBACK) {
		warnx("couldn't load font: %s", fontname[id]);
		return load_font(FONT_FALLBACK);
	} else if (ftfont[id] == NULL)
		errx(1, "couldn't load fallback font: %s", fontname[id]);

	return ftfont[id];
}
