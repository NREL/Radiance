#ifndef lint
static const char	RCSid[] = "$Id$";
#endif

#include "local4014.h"
#include "lib4014.h"

extern void
move(
	int xi,
	int yi
)
{
	putch(035);
	cont(xi,yi);
}
