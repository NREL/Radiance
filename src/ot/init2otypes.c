/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Initialize ofun[] list for bounding box checker
 */

#include  "standard.h"

#include  "octree.h"

#include  "otypes.h"

FUN  ofun[NUMOTYPE] = INIT_OTYPE;


o_default()			/* default action is no intersection */
{
	return(O_MISS);
}
