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

extern struct driver  *aed_init(), *x_init(), *sun_init();

struct device  devtable[] = {			/* supported devices */
	{"aed", "AED 512 color graphics terminal", aed_init},
	{"sun", "SunView color or greyscale screen", sun_init},
	{"X", "X-window color or greyscale display", x_init},
	{0}					/* terminator */
};
