#ifndef lint
static const char	RCSid[] = "$Id$";
#endif

#include "local4014.h"
#include "lib4014.h"

extern void
circle(
	int x,
	int y,
	int r
)
{
	arc(x,y,x+r,y,x+r,y);
}
