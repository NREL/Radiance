/* Copyright (c) 1986 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  data.h - header file for routines which interpolate data.
 *
 *     6/4/86
 */

#define  MAXDIM		8		/* maximum dimensions for data array */

#define  DATATYPE	float		/* single precision to save space */

#define  DSCANF		"%f"		/* scan format for DATATYPE */

typedef struct datarray {
	char  *name;			/* name of our data */
	int  nd;			/* number of dimensions */
	struct {
		double  org, siz;		/* coordinate domain */
		int  ne;			/* number of elements */
	} dim[MAXDIM];			/* dimension specifications */
	DATATYPE  *arr;			/* the data */
	struct datarray  *next;		/* next array in list */
} DATARRAY;			/* a data array */

extern DATARRAY  *getdata(), *getpict();

extern double  datavalue();
