/*
 * mxswm - MaXed Stacks Window Manager for X11
 * Copyright (c) 2022, Tommi Leino <namhas@gmail.com>
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
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <limits.h>

static char *command_history_file = ".mxswm_history";

static char **command_history;
static size_t command_history_sz;
static size_t command_history_alloc;

static void		 load_command_history(void);
static const char	*command_history_path();

int
add_command_to_history(const char *s, int reorder)
{
	int i;
	char *q;

	if (command_history != NULL) {
		for (i = 0; i < command_history_sz; i++) {
			if (strcmp(command_history[i], s) == 0) {
				if (reorder) {
					q = command_history[i];

					memmove(&command_history[i],
					    &command_history[i+1], 
					    sizeof(char *) *
					    (command_history_sz - i - 1));

					command_history[command_history_sz-1] =
					    q;
					save_command_history();
				}
				return 0;
			}
		}
	}

	if (command_history == NULL ||
	    command_history_sz == command_history_alloc) {
		command_history_alloc += 512;
		command_history = realloc(command_history, sizeof(char *) *
		    command_history_alloc);
		if (command_history == NULL) {
			command_history_alloc -= 512;
			warn("realloc");
			return 0;
		}
	}

	q = strdup(s);
	if (q == NULL) {
		warn("strdup");
		return 0;
	}
	command_history[command_history_sz++] = q;
	return 1;
}

const char *
match_command(const char *s)
{
	ssize_t i;
	size_t best_pts = 0, pts;
	const char *best_match = NULL, *p, *q;

	if (s == NULL)
		return NULL;

	if (command_history == NULL)
		load_command_history();

	if (command_history != NULL && command_history_sz > 0)
		for (i = command_history_sz-1; i >= 0; i--) {
			q = s;
			p = command_history[i];
			pts = 0;
			while (*p != '\0' && *q != '\0')
				if (*p++ == *q++)
					pts++;
				else {
					pts = 0;
					break;
				}
			if (*p == '\0' && *q != '\0')
				pts = 0;
			if (pts > best_pts) {
				best_pts = pts;
				best_match = command_history[i];
			}
		}

	return best_match;
}

static const char *
command_history_path()
{
	const char *home;
	static char path[PATH_MAX];

	home = getenv("HOME");
	if (home == NULL) {
		warn("getenv");
		home = "/";
	}

	if (snprintf(path, sizeof(path), "%s/%s", home, command_history_file)
	    >= sizeof(path)) {
		warnx("command history path name overflow");
		return NULL;
	}

	return path;
}

static void
load_command_history()
{
	const char *path;
	FILE *fp;
	char *line = NULL;
	size_t sz = 0;
	ssize_t len;

	if ((path = command_history_path()) == NULL ||
	    (fp = fopen(path, "r")) == NULL)
		return;

	while ((len = getline(&line, &sz, fp)) > 0) {
		line[strcspn(line, "\r\n")] = '\0';
		add_command_to_history(line, 0);
	}
	if (ferror(fp))
		warnx("getline %s", path);

	if (line != NULL)
		free(line);
	fclose(fp);
}

void
save_command_history()
{
	const char *path;
	size_t i;
	FILE *fp;

	if ((path = command_history_path()) == NULL ||
	    (fp = fopen(path, "w")) == NULL)
		return;

	for (i = 0; i < command_history_sz; i++)
		fprintf(fp, "%s\n", command_history[i]);

	if (ferror(fp))
		warn("%s", path);
	fclose(fp);
}
