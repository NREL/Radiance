/* Copyright (c) 1997 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 * Quadtree data structures for holodeck display drivers.
 */

#include "tonemap.h"
#include "rhdriver.h"
				/* quantity of leaves to free at a time */
#ifndef LFREEPCT
#define	LFREEPCT	15
#endif
				/* child ordering */
#define	DL		0		/* down left */
#define	DR		1		/* down right */
#define	UL		2		/* up left */
#define	UR		3		/* up right */

#define	BRF(i)		(1<<(i))	/* branch flag bit */
#define CHF(i)		(0x10<<(i))	/* change flag bit */
#define	CHBRF(i)	(0x11<<(i))	/* changed branch flags */
#define CH_ANY		0xf0		/* flags for any change */

typedef struct {
	float		wp[3];		/* world intersection point */
	TMbright	brt;		/* encoded brightness (LogY) */
	BYTE		chr[3];		/* encoded chrominance (RGB) */
}	RLEAF;			/* recorded ray (leaf) value */

typedef struct rtree {
	short	flgs;			/* branch flags */
	union {
		struct rtree	*b;		/* if branch */
		RLEAF	*l;			/* if leaf */
	} k[4];				/* children */
}	RTREE;

extern RTREE	qtrunk;		/* trunk of quadtree */
extern double	qtDepthEps;	/* epsilon to compare depths (z fraction) */
extern int	qtMinNodesiz;	/* minimum node dimension (pixels) */

extern RLEAF	*qtFindLeaf();


/************************************************************************
 * These driver support routines implement the dev_value() call, but
 * require the following callbacks:

dev_paintr(rgb, x0, y0, x1, y1)	: paint a rectangle
BYTE	rgb[3];			: rectangle color
int	x0, y0, x1, y1;		: rectangle boundaries

Draws an open rectangle between [x0,x1) and [y0,y1) with the color rgb.
This function is called many times by qtUpdate(), qtRedraw() and qtReplant().

 ************************************************************************/
