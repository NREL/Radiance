/* Copyright (c) 1998 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 * Sample data structures for holodeck display drivers.
 */
#include "color.h"
#include "tonemap.h"
#include "rhdriver.h"

#ifndef int2
#define int2	short
#endif
#ifndef int4
#define int4	int
#endif
				/* child ordering */
typedef struct rsamp {
	float		(*wp)[3];	/* world intersection point array */
	int4		*wd;		/* world direction array */
	TMbright	*brt;		/* encoded brightness array */
	BYTE		(*chr)[3];	/* encoded chrominance array */
	BYTE		(*rgb)[3];	/* tone-mapped color array */
	int		max_samp;	/* maximum number of samples */
	int             max_aux_pt;     /* maximum number of aux points */
	int             next_aux_pt;      /* next auxilliary point to add */
	int		replace_samp;      /* next sample to free  */
	int		num_samp;        /* reached end of list? */
	int             tone_map;        /* pointer to next value(s) to tonemap*/
	int             free_samp;       /* list of freed samples */
	char		*base;		/* base of allocated memory */
} RSAMP;

#define RS_W_PT(s)               ((s)->wp)
#define RS_W_DIR(s)              ((s)->wd)
#define RS_BRT(s)                ((s)->brt)
#define RS_CHR(s)                ((s)->chr)
#define RS_RGB(s)                ((s)->rgb)
#define RS_MAX_SAMP(s)           ((s)->max_samp)
#define RS_TONE_MAP(s)           ((s)->tone_map)
#define RS_MAX_POINTS(s)         ((s)->max_aux_pt)
#define RS_REPLACE_SAMP(s)       ((s)->replace_samp)
#define RS_NEXT_AUX_PT(s)        ((s)->next_aux_pt)
#define RS_MAX_AUX_PT(s)         ((s)->max_aux_pt)
#define RS_BASE(s)               ((s)->base)
#define RS_NUM_SAMP(s)           ((s)->num_samp)
#define RS_FREE_SAMP(s)          ((s)->free_samp)
#define RS_NTH_W_PT(s,n)         (RS_W_PT(s)[(n)])
#define RS_NTH_W_DIR(s,n)        (RS_W_DIR(s)[(n)])
#define RS_NTH_RGB(s,n)          (RS_RGB(s)[(n)])
#define RS_NTH_CHR(s,n)          (RS_CHR(s)[(n)])
#define RS_NTH_BRT(s,n)          (RS_BRT(s)[(n)])
#define RS_EOL(s)                (RS_NUM_SAMP(s) >= RS_MAX_SAMP(s))
extern	RSAMP rsL;			

extern double	rsDepthEps;	/* epsilon to compare depths (z fraction) */

extern int4	encodedir();
extern double	fdir2diff(), dir2diff();
extern void     decodedir();
/*
 * int
 * smInit(n)		: Initialize/clear data structures for n entries
 * int	n;
 *
 * Initialize sampL and other data structures for at least n samples.
 * If n is 0, then free data structures.  Return number actually allocated.
 *
 *
 * int
 * smNewSamp(c, p, v)	: register new sample point and return index
 * COLR	c;		: pixel color (RGBE)
 * FVECT	p;	: world intersection point
 * FVECT	v;	: ray direction vector
 *
 * Add new sample point to data structures, removing old values as necessary.
 * New sample representation will be output in next call to smUpdate().
 *
 *
 * int
 * smFindSamp(orig, dir): intersect ray with 3D rep. and find closest sample
 * FVECT	orig, dir;
 *
 * Find the closest sample to the given ray.  Return -1 on failure.
 *
 *
 * smClean()		: display has been wiped clean
 *
 * Called after display has been effectively cleared, meaning that all
 * geometry must be resent down the pipeline in the next call to smUpdate().
 *
 *
 * smUpdate(vp, qua)	: update OpenGL output geometry for view vp
 * VIEW	*vp;		: desired view
 * int	qua;		: quality level (percentage on linear time scale)
 *
 * Draw new geometric representation using OpenGL calls.  Assume that the
 * view has already been set up and the correct frame buffer has been
 * selected for drawing.  The quality level is on a linear scale, where 100%
 * is full (final) quality.  It is not necessary to redraw geometry that has
 * been output since the last call to smClean().  (The last view drawn will
 * be vp==&odev.v each time.)
 */

/* These values are set by the driver, and used in the OGL call for glFrustum*/
extern double dev_zmin,dev_zmax;







