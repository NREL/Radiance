#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 *  devtable.c - device table for rview.
 */

#include "copyright.h"

#include  <stdio.h>

#include  "color.h"
#include  "driver.h"

char  dev_default[] = "x11";

extern dr_initf_t x11_init; /* XXX this should be in a seperate header file */

struct device  devtable[] = {			/* supported devices */
	{"slave", "Slave driver", slave_init},
	{"x11", "X11 color or greyscale display", x11_init},
	{"x11d", "X11 display using stdin/stdout", x11_init},
	{0}					/* terminator */
};
