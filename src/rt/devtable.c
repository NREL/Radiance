/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  devtable.c - device table for rview.
 *
 *	3/30/88
 */

#include  "driver.h"

char  dev_default[] = "x11";

extern struct driver  *x11_init();

struct device  devtable[] = {			/* supported devices */
	{"aed", "AED 512 color graphics terminal", comm_init},
	{"news", "NeWS color or greyscale screen", comm_init},
	{"sun", "SunView color or greyscale screen", comm_init},
	{"x11", "X11 color or greyscale display", x11_init},
	{0}					/* terminator */
};
