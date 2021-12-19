# Contributing to mxswm

## Wishes

* Test with various X11 client programs and report crashes / problems (the
author is only running *vtsh*, *Firefox* and standard X11 programs like
*xclock*
* Test on different Unix-like systems and make pull request for portability
  fixes
* Package for different Unix-like systems (and tell about it e.g. using e-mail)

But you can make a pull request of anything you see useful and we can have
a chat about it.

## Reporting bugs

Unless it is easy to describe the bug otherwise, add -DTRACE to CFLAGS
in Makefile and send the trace log.

If you get X11 errors, you could run *mxswm* with the sync option i.e.
run *mxswm* like this:

	$ mxswm sync

## Style

* Follow OpenBSD style(9) a.k.a. KNF which is very close to Linux-kernel
  style and similar to K&R
