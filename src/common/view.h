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

typedef struct {
	int  type;		/* view type */
	FVECT  vp;		/* view origin */
	FVECT  vdir;		/* view direction */
	FVECT  vup;		/* view up */
	double  horiz;		/* horizontal view size */
	double  vert;		/* vertical view size */
	int  hresolu;		/* horizontal resolution */
	int  vresolu;		/* vertical resolution */
	FVECT  vhinc;		/* computed horizontal increment */
	FVECT  vvinc;		/* computed vertical increment */
} VIEW;			/* view parameters */

extern VIEW  stdview;

extern char  *setview();

#define  STDVIEW(h)	{VT_PER,0.,0.,0.,0.,1.,0.,0.,0.,1.,45.,45.,h,h}

#define  VIEWSTR	"VIEW="
