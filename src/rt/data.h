/* Copyright (c) 1996 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  data.h - header file for routines which interpolate data.
 */

#define  MAXDDIM	5		/* maximum data dimensions */

#define  DATATYPE	float		/* single precision to save space */
#define  DATATY		'f'		/* format for DATATYPE */

typedef struct datarray {
	char  *name;			/* name of our data */
	short  type;			/* DATATY, RED, GRN or BLU */
	short  nd;			/* number of dimensions */
	struct {
		DATATYPE  org, siz;		/* coordinate domain */
		int  ne;			/* number of elements */
		DATATYPE  *p;			/* point locations */
	} dim[MAXDDIM];			/* dimension specifications */
	union {
		DATATYPE  *d;			/* float data */
		COLR  *c;			/* RGB data */
	}  arr;				/* the data */
	struct datarray  *next;		/* next array in list */
} DATARRAY;			/* a data array */

extern DATARRAY  *getdata(), *getpict();

extern double  datavalue();
