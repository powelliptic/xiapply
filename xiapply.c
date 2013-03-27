/*
 * Copyright (c) 2013, David Powell
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * xiapply: applies changes when an XInput devices is enabled
 *
 * SYNOPSIS
 *	xiapply [-v] command [ command args ]
 *
 * DESCRIPTION
 *	Executes 'command' with once.  Subsequently, each time an XInput
 *	device is enabled 'command' is executed again.  xiapply will
 *	continue to wait for XInput events until an error occurs.
 *
 *	'command' should be idempotent, as it will be executed multiple
 *	times.
 *
 *	'command' must exit.  xiapply will wait for 'command' to complete
 *	before processing additional events and calling 'command' again.
 *
 * OPTIONS
 *	-v	enable verbose output
 *
 * EXIT STATUS
 *	1	an error occurred
 *	2	invalid options / operands were specified
 */

#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

static bool verbose = false;

static void
usage(void)
{
	errx(2, "Usage: xiapply [-v] command [ command args ]");
}

static bool
queryXI2(Display *d, int *opcode)
{
	int error, event;
	int major = 2;
	int minor = 0;

	return (XQueryExtension(d, "XInputExtension", opcode, &error, &event) &&
	    XIQueryVersion(d, &major, &minor) == Success);
}

static bool
selectXI2Event(Display *d, int event)
{
	unsigned char mask[XIMaskLen(XI_LASTEVENT)] = { 0 };
	XISetMask(mask, event);

	XIEventMask eventmask;
	eventmask.deviceid = XIAllDevices;
	eventmask.mask_len = sizeof (mask);
	eventmask.mask = mask;

	return (XISelectEvents(d, DefaultRootWindow(d), &eventmask, 1) ==
	    Success);
}

static void
warnstatus(int status)
{
	if (WIFEXITED(status))
		warnx("Child exited with status %d.", WEXITSTATUS(status));
	else
		warnx("Child exited on signal %d.", WTERMSIG(status));
}

static void
apply(char **argv)
{
	pid_t pid;

	if (verbose)
		warnx("Starting command: %s", argv[0]);

	if ((pid = fork()) == 0) {
		(void) execvp(argv[0], argv);
		warn("Failed to exec '%s'", argv[0]);
		_exit(1);
	} else if (pid != -1) {
		int status;
		(void) waitpid(pid, &status, 0);

		bool quit = !WIFEXITED(status) || WEXITSTATUS(status) != 0;
		if (verbose || quit) {
			warnstatus(status);
			if (quit)
				exit(1);
		}
	} else {
		err(1, "Failed to create child process");
	}
}

int
main(int argc, char **argv)
{
	int c, opcode;
	Display *d;
	char **cmd;
	extern int optind;

	/* YYY: GNU getopt isn't POSIX compatible */
	while ((c = getopt(argc, argv, "+:v")) != -1) {
		switch (c) {
		case 'v':
			verbose = true;
			break;
		default:
			usage();
		}
	}

	if (argc - optind == 0)
		usage();

	cmd = argv + optind;

	if ((d = XOpenDisplay(NULL)) == NULL)
		errx(1, "Failed to open display.");

	if (!queryXI2(d, &opcode))
		errx(1, "XInputExtension version 2 not available.");

	if (!selectXI2Event(d, XI_HierarchyChanged))
		errx(1, "Failed to listen to XInput 2 hierarchy events.");

	apply(cmd);

	for (;;) {
		XEvent ev;
		XNextEvent(d, &ev);
		if (ev.type == GenericEvent &&
		    ev.xgeneric.extension == opcode &&
		    ev.xgeneric.evtype == XI_HierarchyChanged &&
		    XGetEventData(d, &ev.xcookie)) {
			XIHierarchyEvent *hev = ev.xcookie.data;
			if ((hev->flags & XIDeviceEnabled) != 0)
				apply(cmd);
			XFreeEventData(d, &ev.xcookie);
		}
	}

	return (0);
}
