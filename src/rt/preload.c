/* Copyright (c) 1996 Regents of the University of California */

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
#include "color.h"
#include "data.h"


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
	case PAT_CPICT:		/* color picture */
		if (op->oargs.nsargs < 4)
			goto sargerr;
		getpict(op->oargs.sarg[3]);
		return(1);
	case PAT_CDATA:		/* color data */
		/* FALL THROUGH */
	case TEX_DATA:		/* texture data */
		if (op->oargs.nsargs < 6)
			goto sargerr;
		getdata(op->oargs.sarg[3]);
		getdata(op->oargs.sarg[4]);
		getdata(op->oargs.sarg[5]);
		return(1);
	case PAT_BDATA:		/* brightness data */
		/* FALL THROUGH */
	case MAT_PDATA:		/* plastic BRDF data */
	case MAT_MDATA:		/* metal BRDF data */
	case MAT_TDATA:		/* trans BRDF data */
		if (op->oargs.nsargs < 2)
			goto sargerr;
		getdata(op->oargs.sarg[1]);
		return(1);
	}
			/* don't bother with others -- too tricky */
	return(0);
sargerr:
	objerror(op, USER, "too few string arguments");
}


preload_objs()		/* preload object data structures */
{
	register OBJECT on;
				/* note that nobjects may change during loop */
	for (on = 0; on < nobjects; on++)
		load_os(objptr(on));
}
