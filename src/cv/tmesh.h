/* Copyright (c) 1994 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Header file for triangle mesh routines using barycentric coordinates
 */

#define TCALNAME	"tmesh.cal"	/* the name of our auxiliary file */

typedef struct {
	int	ax;		/* major axis */
	FLOAT	tm[2][3];	/* transformation */
} BARYCCM;
