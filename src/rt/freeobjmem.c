/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Free memory associated with object(s)
 */

#include "standard.h"
#include "object.h"
#include "otypes.h"


int
free_os(op)			/* free unneeded memory for object */
register OBJREC	*op;
{
	if (op->os == NULL)
		return(0);
	if (hasfunc(op->otype)) {
		freefunc(op);
		return(1);
	}
	switch (op->otype) {
	case OBJ_FACE:		/* polygon */
		freeface(op);
		return(1);
	case OBJ_CONE:		/* cone */
	case OBJ_RING:		/* disk */
	case OBJ_CYLINDER:	/* cylinder */
	case OBJ_CUP:		/* inverted cone */
	case OBJ_TUBE:		/* inverted cylinder */
		freecone(op);
		return(1);
	case OBJ_INSTANCE:	/* octree instance */
		freeinstance(op);
		return(1);
	case PAT_BTEXT:		/* monochromatic text */
	case PAT_CTEXT:		/* colored text */
	case MIX_TEXT:		/* mixing text */
		freetext(op);
		return(1);
	case MAT_CLIP:		/* clipping surface */
		free(op->os);
		op->os = NULL;
		return(1);
	case MAT_SPOT:		/* spot light source */
		return(0);
	default:
#ifdef DEBUG
		objerror(op, WARNING, "cannot free structure");
#endif
		return(0);
	}
}


int
free_objs(on, no)		/* free some object structures */
register OBJECT	on;
OBJECT	no;
{
	int	nfreed;
	register OBJREC	*op;

	for (nfreed = 0; no-- > 0; on++) {
		op = objptr(on);
		if (op->os != NULL)
			nfreed += free_os(op);
	}
	return(nfreed);
}


free_objmem()			/* free all object cache memory */
{
	free_objs(0, nobjects);
	freedata(NULL);
	freefont(NULL);
}
