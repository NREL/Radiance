/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Holodeck display process communication
 */

#include "rholo.h"
#include "rhdisp.h"


open_display(dname)		/* open a display process */
char	*dname;
{
	error(INTERNAL, "display process unimplemented");
}


disp_check(block)		/* check display process */
int	block;
{
	return(1);
}


disp_packet(p)			/* display a packet */
register PACKET	*p;
{
}
