#ifndef lint
static const char	RCSid[] = "$Id$";
#endif

#include <stdio.h>

#include "local4014.h"
#include "lib4014.h"

void
closevt(void)
{
	putch(037);
	putch(030);
	fflush(stdout);
}

void
closepl(void)
{
	putch(037);
	putch(030);
	fflush(stdout);
}
