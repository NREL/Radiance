/* Copyright (c) 1997 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Header file for OpenGL display process interface to rwalk.
 */

#include "view.h"

typedef struct {
	int	xres, yres;	/* display window size (renderable area) */
} GLDISP;			/* display structure */

extern GLDISP	ourgld;		/* our display structure */
				/* background views */
#define BG_XMIN		0
#define BG_XMAX		1
#define BG_YMIN		2
#define BG_YMAX		3
#define BG_ZMIN		4
#define BG_ZMAX		5

#define BGVWNAMES	{"xmin","xmax","ymin","ymax","zmin","zmax"}
