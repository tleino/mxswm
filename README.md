# mxswm

*mxswm* is a MaXed Stacks Window Manager for X11. This window manager
keeps windows in a number of stacks of same size and
allows quick changing between the windows using keyboard.

This is useful for people who hate micromanaging the position and size
of windows using mouse.

If it is necessary to stack some windows vertically, you can use
[xvbox](https://github.com/tleino/xvbox).

If it is necessary to manually manage position and size of some windows,
run a separate window manager better suited for the job in Xephyr.

This window manager was originally created to be a simpler alternative
for [cocowm](https://github.com/tleino/cocowm) by the same author.
Reaping the full benefit of cocowm requires a specialized command shell
system, such as [cocovt](https://github.com/tleino/cocovt).
However, a simple maximizing window manager like *mxswm* is fine enough
for most purposes when working with programs that were designed for
the full screen view, such as [vtsh](https://github.com/tleino/vtsh)
which is an alternative to [cocovt](https://github.com/tleino/cocovt).

At the moment *mxswm* mostly works, but is not flawless. Also, requires manual
customization from mxswm.h for e.g. setting the fonts and borders. There
is some initial support for runtime configuration through a Unix domain
socket using 'mxswmctl' command.

## Feature TODO

* Choose window by typing letters of its name (similar to
reverse-search or ^R in shells).
* Close windows from menu / close foreground window.
* Xinerama (multi-screen) support (can be copied from cocowm).
* Runtime configuration of max stack width (useful for setting sane
maximums, or for setting the width for 80 column terminals).

## Default key bindings

### Stacks

* **F1** Remove window stack
* **F2** Add window stack
* **F3** Toggle overriding maxwidth setting
* **Win+Left** Select stack on the left
* **Win+Right** Select stack on the right
* **Shift+Win+Left** Move current stack to the left
* **Shift+Win+Right** Move current stack to the right
* **Win+S** Toggle sticky status for stack
* **Win+H** Toggle hide other stacks (not counting sticky stacks)
* **Win+Tab** Run command in this stack

### Clients

* **Win+Menu** Cycle to next client in any stack
* **Win+Down** Select client / raise selected client
* **Win+Up** Select client / raise selected client
* **Win+q** Delete client
* **Win+k** Destroy client forcibly (rarely needed / should not be used)
* **Ctrl+Win+Left** Move selected client to stack on the left
* **Ctrl+Win+Right** Move selected client to stack on the right
* **Win+Space** Rename current client's title (or reset to original)

### Prompt

* **BackSpace** Delete character
* **Esc** Exit prompt without submitting

### Statusbar

* **Win+t** Toggle display of root window title contents

### Debugging

* **Win+a** Dump stack and client information to trace log
* **Win+l** Restart window manager (useful e.g. for testing .mxswmrc
or after code modifications)
* **Win+Shift+q** Exit

## Build

	$ ./configure ~
	$ make
	$ make install

## Runtime configuration

Set first stack's width to 200px:

	$ mxswmctl stack 1 width 200

## Dependencies

* Practically none on a standard Unix/Linux system that uses
X11 Window System.

## Debugging

Enable synchronized (slow) behavior that help in debugging X11 errors.

	$ mxswm sync

By default mxswm does not have an error handler for X11 errors which makes
mxswm crash when getting unexpected client behavior. This is so that the
errors can be fixed from mxswm-side so that *mxswm* becomes robust and does
not need error handler for these sort of things. **Help is wanted** in
running *mxswm* with various client programs.

You can also add -DTRACE to CFLAGS in Makefile for getting debug messages.
When reporting bugs, sending the -DTRACE output would be good.

## Files

* *~/.mxswm_socket* For runtime configuration using mxswmctl
