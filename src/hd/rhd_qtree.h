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

#define	BRF(i)		(0x1<<(i))	/* branch flag bit */
#define LFF(i)		(0x10<<(i))	/* leaf flag bit */
#define CHF(i)		(0x100<<(i))	/* change flag bit */
#define	CHBRF(i)	(0x101<<(i))	/* changed branch bit */
#define CHLFF(i)	(0x110<<(i))	/* changed leaf bit */
#define BR_ANY		0xf		/* flags for any branches */
#define LF_ANY		0xf0		/* flags for any leaves */
#define CH_ANY		0xf00		/* flags for any change */

typedef struct rtree {
	short	flgs;			/* content flags (defined above) */
	union {
		struct rtree	*b;		/* if branch */
		int	li;			/* if leaf */
	} k[4];				/* children */
}	RTREE;

extern struct rleaves {
	float		(*wp)[3];	/* world intersection point array */
	TMbright	*brt;		/* encoded brightness array */
	BYTE		(*chr)[3];	/* encoded chrominance array */
	BYTE		(*rgb)[3];	/* tone-mapped color array */
	int		nl;		/* count of leaves in our pile */
	int		bl, tl;		/* bottom and top (next) leaf index */
	int		tml;		/* next leaf needing tone-mapping */
	char		*base;		/* base of allocated memory */
}	qtL;			/* our pile of leaves */

extern RTREE	qtrunk;		/* trunk of quadtree */
extern double	qtDepthEps;	/* epsilon to compare depths (z fraction) */
extern int	qtMinNodesiz;	/* minimum node dimension (pixels) */


/************************************************************************
 * These driver support routines implement the dev_value() call, but
 * require the following callbacks:

dev_paintr(rgb, x0, y0, x1, y1)	: paint a rectangle
BYTE	rgb[3];			: rectangle color
int	x0, y0, x1, y1;		: rectangle boundaries

Draws an open rectangle between [x0,x1) and [y0,y1) with the color rgb.
This function is called many times by qtUpdate(), qtRedraw() and qtReplant().

 ************************************************************************/
