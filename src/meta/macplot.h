/* RCSid: $Id$ */
/*
 *   Definitions for MacIntosh plotting routines
 */

#undef  TRUE

#undef  FALSE

#include  <quickdraw.h>


#define  mapx(x)  CONV(x, dxsize)

#define  mapy(y)  (dysize - 1 - CONV(y, dysize))


extern int  pati[];

extern Pattern  macpat[];	/* fill patterns */

extern  dxsize, dysize;		/* plot dimensions */
