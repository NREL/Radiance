/* SCCSid "$SunId$ LBL" */

/* Copyright (c) 1989 Regents of the University of California */

/*
 *  xtwind.h - header for X text window routines.
 *
 *  Written by G. Ward
 *	10/30/87
 *
 *  Modified for X11 B. V. Smith
 *	9/26/88
 */

#define LEFTMAR	2			/* left margin width */

typedef struct {
	Display  *dpy;			/* the display */
	Window  w;			/* window */
	XFontStruct  *f;		/* font information */
	GC	gc;			/* graphics context */
	short  nc, nr;			/* text number of columns and rows */
	char  **lp;			/* null-terminated lines */
	short  c, r;			/* current position */
	short	cursor;			/* cursor type */
}  TEXTWIND;			/* a text window */

#define TNOCURS		0
#define TBLKCURS	1

extern TEXTWIND  *xt_open();
