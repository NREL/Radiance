/* Copyright (c) 1987 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  xtwind.h - header for X text window routines.
 *
 *	10/30/87
 */

#define LEFTMAR	2			/* left margin width */

typedef struct {
	Window  w;			/* window */
	FontInfo  f;			/* font information */
	short  nc, nr;			/* text size */
	char  **lp;			/* null-terminated lines */
	short  c, r;			/* current position */
	short	cursor;			/* cursor type */
}  TEXTWIND;			/* a text window */

#define TNOCURS		0
#define TBLKCURS	1

extern TEXTWIND  *xt_open();
