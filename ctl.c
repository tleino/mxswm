#include "mxswm.h"
#include <stdio.h>

void
run_ctl_line(const char *str)
{
	int stackno, width;
	struct stack *stack;

	if (sscanf(str, "stack %d width %d", &stackno, &width) == 2) {
		stack = find_stack(stackno);
		if (stack != NULL)
			stack->prefer_width = width;
		resize_stacks();
		focus_stack(stack);
	}
}
