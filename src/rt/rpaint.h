/* Copyright (c) 1987 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  rpaint.h - header file for image painting.
 *
 *     1/30/87
 */

#include  "driver.h"

#include  "view.h"

typedef short  COORD;		/* an image coordinate */

typedef struct pnode {
	struct pnode  *kid;		/* children */
	COORD  x, y;			/* position */
	COLOR  v;			/* value */
}  PNODE;			/* a paint node */

				/* child ordering */
#define  DL		0		/* down left */
#define  DR		1		/* down right */
#define  UL		2		/* up left */
#define  UR		3		/* up right */

#define  newptree()	(PNODE *)calloc(4, sizeof(PNODE))

typedef struct {
	COORD  l, d, r, u;		/* left, down, right, up */
}  RECT;			/* a rectangle */

extern PNODE  ptrunk;		/* the base of the image tree */

extern VIEW  ourview;		/* current view parameters */
extern VIEW  oldview;		/* previous view parameters */
extern int  hresolu, vresolu;	/* image resolution */

extern int  greyscale;		/* map colors to brightness? */

extern int  pdepth;		/* image depth in current frame */
extern RECT  pframe;		/* current frame rectangle */

extern double  exposure;	/* exposure for scene */

extern struct driver  *dev;	/* driver functions */

extern PNODE  *findrect();
