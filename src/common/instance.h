/* Copyright (c) 1988 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  instance.h - header file for routines using octree objects.
 *
 *	11/10/88
 */

#include  "octree.h"

typedef struct scene {
	char  *name;			/* octree name */
	int  ldflags;			/* what was loaded */
	CUBE  scube;			/* scene cube */
	struct scene  *next;		/* next in list */
}  SCENE;			/* loaded octree */

typedef struct {
	struct {
		double  sca;			/* scaling */
		double  xfm[4][4];		/* transform */
	} f, b;				/* forward and backward */
	SCENE  *obj;			/* loaded object */
}  INSTANCE;			/* instance of octree */

extern SCENE  *getscene();

extern INSTANCE  *getinstance();
