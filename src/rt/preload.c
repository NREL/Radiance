/* Copyright (c) 1993 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Preload associated object structures to maximize memory sharing.
 */

#include "standard.h"
#include "object.h"
#include "otypes.h"
#include "face.h"
#include "cone.h"
#include "instance.h"


int
load_os(op)			/* load associated data for object */
register OBJREC	*op;
{
	switch (op->otype) {
	case OBJ_FACE:		/* polygon */
		getface(op);
		return(1);
	case OBJ_CONE:		/* cone */
	case OBJ_RING:		/* disk */
	case OBJ_CYLINDER:	/* cylinder */
	case OBJ_CUP:		/* inverted cone */
	case OBJ_TUBE:		/* inverted cylinder */
		getcone(op, 1);
		return(1);
	case OBJ_INSTANCE:	/* octree instance */
		getinstance(op, IO_ALL);
		return(1);
	}
			/* don't bother with non-surfaces -- too tricky */
	return(0);
}


preload_objs()		/* preload object data structures */
{
	register OBJECT on;
				/* note that nobjects may change during loop */
	for (on = 0; on < nobjects; on++)
		load_os(objptr(on));
}


clean_slate()		/* reset time and ray counters */
{
	extern long	tstart, time();
	extern long	raynum, nrays;
	register OBJECT	on;

	tstart = time(0);
	raynum = nrays = 0;
	for (on = 0; on < nobjects; on++)
		objptr(on)->lastrno = -1;
}
