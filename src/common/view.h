/* Copyright (c) 1988 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  view.h - header file for image generation.
 *
 *     9/19/88
 */

				/* view types */
#define  VT_PER		'v'		/* perspective */
#define  VT_PAR		'l'		/* parallel */
#define  VT_ANG		'a'		/* angular fisheye */
#define  VT_HEM		'h'		/* hemispherical fisheye */

typedef struct {
	int  type;		/* view type */
	FVECT  vp;		/* view origin */
	FVECT  vdir;		/* view direction */
	FVECT  vup;		/* view up */
	double  horiz;		/* horizontal view size */
	double  vert;		/* vertical view size */
	double  hoff;		/* horizontal image offset */
	double  voff;		/* vertical image offset */
	FVECT  hvec;		/* computed horizontal image vector */
	FVECT  vvec;		/* computed vertical image vector */
	double  hn2;		/* DOT(hvec,hvec) */
	double  vn2;		/* DOT(vvec,vvec) */
} VIEW;			/* view parameters */

extern VIEW  stdview;

extern char  *setview();

#define  viewaspect(v)	sqrt((v)->vn2/(v)->hn2)

#define  STDVIEW	{VT_PER,{0.,0.,0.},{0.,1.,0.},{0.,0.,1.}, \
				45.,45.,0.,0.,{0.,0.,0.},{0.,0.,0.},0.,0.}

#define  VIEWSTR	"VIEW="
#define  VIEWSTRL	5
