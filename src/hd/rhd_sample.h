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

#ifndef INVALID
#define INVALID  -1
#endif

typedef struct samp {
	float		(*wp)[3];	/* world intersection point array */
	int4		*wd;		/* world direction array */
#ifndef HP_VERSION
	TMbright	*brt;		/* encoded brightness array */
#endif
	BYTE		(*chr)[3];	/* encoded chrominance array */
	BYTE		(*rgb)[3];	/* tone-mapped color array */
	int		max_samp;	/* maximum number of samples */
	int             max_base_pt;     /* maximum number of aux points */
	int             next_base_pt;    /* next auxilliary point to add */
	int		replace_samp;   /* next sample to free  */
	int		num_samp;       /* current number of samples */
	int             tone_map;       /* pointer to next value(s)t tonemap*/
        int             free_samp;      /* Available sample */
	char		*base;		/* base of allocated memory */
} SAMP;

/* Sample field access macros */
#define S_W_PT(s)               ((s)->wp)
#define S_W_DIR(s)              ((s)->wd)
#define S_BRT(s)                ((s)->brt)
#define S_CHR(s)                ((s)->chr)
#define S_RGB(s)                ((s)->rgb)
#define S_MAX_SAMP(s)           ((s)->max_samp)
#define S_MAX_BASE_PT(s)        ((s)->max_base_pt)
#define S_NEXT_BASE_PT(s)       ((s)->next_base_pt)
#define S_REPLACE_SAMP(s)       ((s)->replace_samp)
#define S_NUM_SAMP(s)           ((s)->num_samp)
#define S_TONE_MAP(s)           ((s)->tone_map)
#define S_FREE_SAMP(s)           ((s)->free_samp)
#define S_BASE(s)               ((s)->base)

#define S_MAX_POINTS(s)         ((s)->max_base_pt)
#define S_NTH_W_PT(s,n)         (S_W_PT(s)[(n)])
#define S_NTH_W_DIR(s,n)        (S_W_DIR(s)[(n)])
#define S_NTH_RGB(s,n)          (S_RGB(s)[(n)])
#define S_NTH_CHR(s,n)          (S_CHR(s)[(n)])
#ifndef HP_VERSION
#define S_NTH_BRT(s,n)          (S_BRT(s)[(n)])
#endif
/* Sample Flag macros */
#define S_IS_FLAG(s)		IS_FLAG(samp_flag,s)
#define S_SET_FLAG(s)		SET_FLAG(samp_flag,s)
#define S_CLR_FLAG(s)		CLR_FLAG(samp_flag,s)

#define S_NTH_NEXT(s,n)         S_NTH_W_DIR(s,n)
#define sUnalloc_samp(s,n) (S_NTH_NEXT(s,n) = S_FREE_SAMP(s),S_FREE_SAMP(s)=n)
#define sClear_base_points(s)  (S_NEXT_BASE_PT(s) = S_MAX_SAMP(s))
#define sClear(s)      sInit(s)
/* Max allowed angle of sample dir from current view */
#define MAXANG		20
#define MAXDIFF2	( MAXANG*MAXANG * (PI*PI/180./180.))
#define MAXQUALITY      10000   /* Maximum quality for rendering rep */

extern	SAMP rsL;			
extern double	sDepthEps;	/* epsilon to compare depths (z fraction) */
extern int4	encodedir();    /* Encodes FVECT direction */
extern void     decodedir();    /* Decodes dir-> FVECT */
extern double	fdir2diff(), dir2diff(); /* Compare dir and FVECT */
extern int4    *samp_flag;     /* Per/sample flags s */

/* These values are set by the driver, and used in the OGL call for glFrustum*/
extern double dev_zmin,dev_zmax;

extern SAMP *sAlloc();







