#ifndef lint
static const char	RCSid[] = "$Id$";
#endif

#include "local4014.h"
#include "lib4014.h"

extern void
point(
	int xi,
	int yi
)
{
	move(xi,yi);
	cont(xi,yi);
}
