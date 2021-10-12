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
#include <stdio.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>

void
run_ctl_lines()
{
	FILE *fp;
	char *home;
	struct stat sb;
	char path[PATH_MAX], buf[1024];

	home = getenv("HOME");
	if (home == NULL)
		home = "/";

	if (snprintf(path, sizeof(path),
	    "%s/.mxswmrc", home) >= sizeof(path)) {
		warnx(".mxswmrc path truncated");
		return;
	}

	if (stat(path, &sb) == 0) {
		if (!S_ISREG(sb.st_mode)) {
			warnx("%s is not regular file", path);
			return;
		}
	}

	fp = fopen(path, "r");
	if (fp == NULL) {
		warn("%s", path);
		return;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		buf[strcspn(buf, "\r\n")] = '\0';
		run_ctl_line(buf);
	}
	if (ferror(fp))
		warn("%s", path);
	fclose(fp);
}

void
run_ctl_line(const char *str)
{
	int stackno, width;
	struct stack *stack;

	TRACE_LOG("\"%s\"", str);
	if (sscanf(str, "stack %d width %d", &stackno, &width) == 2) {
		stack = find_stack(stackno);
		if (stack != NULL)
			stack->prefer_width = width;
		resize_stacks();
		focus_stack(stack);
	} else if (strncmp(str, "add stack", strlen("add stack")) == 0) {
		add_stack(current_stack());
	}
}
