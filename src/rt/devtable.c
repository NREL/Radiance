/* Copyright (c) 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  devtable.c - device table for rview.
 *
 *	3/30/88
 */

#include  "driver.h"

extern struct driver  *aed_init(), *x_init();

struct device  devtable[] = {			/* supported devices */
	{"aed", "AED 512 color graphics terminal", aed_init},
	{"sundev", "SunView color or greyscale screen", comm_init},
	{"X", "X10 color or greyscale display", x_init},
	{"x11dev", "X11 color or greyscale display", comm_init},
	{0}					/* terminator */
};
