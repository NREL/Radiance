/* Copyright (c) 1992 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Header file for font handling routines
 */

typedef unsigned char  GORD;

typedef struct {
	short  nverts;			/* number of vertices */
	GORD  start, width;		/* glyph size for prop. spacing */
					/* followed by vertex list */
}  GLYPH;

#define gvlist(g)	((GORD *)((g)+1))

typedef struct font {
	GLYPH  *fg[256];		/* font glyphs */
	char  *name;			/* font file name */
	struct font  *next;		/* next font in list */
}  FONT;

extern FONT  *getfont();
