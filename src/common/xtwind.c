/* Copyright (c) 1987 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  xtwind.c - routines for X text windows.
 *
 *	10/30/87
 */

#include  <stdio.h>

#include  <X/Xlib.h>

#include  "xtwind.h"

#define checkcurs(t)		if ((t)->cursor) togglecurs(t)

#define restorecurs		checkcurs

#define togglecurs(t)		XPixFill((t)->w, (t)->c*(t)->f.width+LEFTMAR, \
				(t)->r*(t)->f.height, (t)->f.width, \
				(t)->f.height, 0, 0, GXinvert, 1)

extern char  *calloc(), *malloc();


TEXTWIND *
xt_open(parent, x, y, width, height, border, fontname)
Window  parent;
int  x, y;
int  width, height;
int  border;
char  *fontname;
{
	register int  i;
	register TEXTWIND  *t;

	if ((t = (TEXTWIND *)malloc(sizeof(TEXTWIND))) == NULL)
		return(NULL);
	t->w = XCreateWindow(parent, x, y, width, height,
			border, BlackPixmap, WhitePixmap);
	if (t->w == 0)
		return(NULL);
	XMapWindow(t->w);
	if ((i = XGetFont(fontname)) == 0)
		return(NULL);
	if (XQueryFont(i, &t->f) == 0)
		return(NULL);
	if (!t->f.fixedwidth)
		return(NULL);
	t->nc = (width - LEFTMAR) / t->f.width;
	t->nr = height / t->f.height;
	if (t->nc < 1 || t->nr < 1)
		return(NULL);
	if ((t->lp = (char **)calloc(t->nr+1, sizeof(char *))) == NULL)
		return(NULL);
	for (i = 0; i < t->nr; i++)
		if ((t->lp[i] = calloc(t->nc+1, 1)) == NULL)
			return(NULL);
	t->r = t->c = 0;
	t->cursor = TNOCURS;
	return(t);
}


xt_puts(s, t)				/* output a string */
register char  *s;
TEXTWIND  *t;
{
	int  oldcurs = xt_cursor(t, TNOCURS);

	while (*s)
		xt_putc(*s++, t);
	xt_cursor(t, oldcurs);
}


xt_putc(c, t)				/* output a character */
char  c;
register TEXTWIND  *t;
{
	int  oldcurs = xt_cursor(t, TNOCURS);

	switch (c) {
	case '\n':
		if (t->r >= t->nr - 1)
			xt_delete(t, 0);	/* scroll up 1 line */
		else if (t->r < t->nr - 1)
			t->r++;
	/* fall through */
	case '\r':
		t->c = 0;
		break;
	case '\b':
		while (t->c < 1 && t->r > 0)
			t->c = strlen(t->lp[--t->r]);
		if (t->c > 0)
			t->c--;
		break;
	default:
		if (t->c >= t->nc)
			xt_putc('\n', t);
		XText(t->w, LEFTMAR + t->c*t->f.width, t->r*t->f.height,
				&c, 1, t->f.id, BlackPixel, WhitePixel);
		t->lp[t->r][t->c++] = c;
		break;
	}
	xt_cursor(t, oldcurs);
}


xt_delete(t, r)				/* delete a line */
register TEXTWIND  *t;
int  r;
{
	char  *cp;
	register int  i;

	if (r < 0 || r >= t->nr)
		return;
	checkcurs(t);
					/* move lines */
	XMoveArea(t->w, LEFTMAR, (r+1)*t->f.height, LEFTMAR, r*t->f.height,
			t->nc*t->f.width, (t->nr-1-r)*t->f.height);
	cp = t->lp[r];
	for (i = r; i < t->nr-1; i++)
		t->lp[i] = t->lp[i+1];
	t->lp[t->nr-1] = cp;
					/* clear bottom */
	XPixSet(t->w, LEFTMAR, (t->nr-1)*t->f.height,
			t->nc*t->f.width, t->f.height, WhitePixel);
	bzero(cp, t->nc);
	restorecurs(t);			/* should we reposition cursor? */
}


xt_insert(t, r)				/* insert a line */
register TEXTWIND  *t;
int  r;
{
	char  *cp;
	register int  i;

	if (r < 0 || r >= t->nr)
		return;
	checkcurs(t);
					/* move lines */
	XMoveArea(t->w, LEFTMAR, r*t->f.height, LEFTMAR, (r+1)*t->f.height,
			t->nc*t->f.width, (t->nr-1-r)*t->f.height);
	cp = t->lp[t->nr-1];
	for (i = t->nr-1; i > r; i--)
		t->lp[i] = t->lp[i-1];
	t->lp[r] = cp;
					/* clear new line */
	XPixSet(t->w, LEFTMAR, r*t->f.height,
			t->nc*t->f.width, t->f.height, WhitePixel);
	bzero(cp, t->nc);
	restorecurs(t);			/* should we reposition cursor? */
}


xt_redraw(t)				/* redraw text window */
register TEXTWIND  *t;
{
	register int  i;

	checkcurs(t);
	XClear(t->w);
	for (i = 0; i < t->nr; i++)
		XText(t->w, LEFTMAR, i*t->f.height, t->lp[i], strlen(t->lp[i]),
				t->f.id, BlackPixel, WhitePixel);
	restorecurs(t);
}


xt_clear(t)				/* clear text window */
register TEXTWIND  *t;
{
	register int  i;

	checkcurs(t);
	XClear(t->w);
	for (i = 0; i < t->nr; i++)
		bzero(t->lp[i], t->nc);
	t->r = t->c = 0;
	restorecurs(t);
}


xt_move(t, r, c)			/* move to new position */
register TEXTWIND  *t;
int  r, c;
{
	if (r < 0 || c < 0 || r >= t->nr || c >= t->nc)
		return;
	checkcurs(t);
	t->r = r;
	t->c = c;
	restorecurs(t);
}


int
xt_cursor(t, curs)			/* change cursor */
register TEXTWIND  *t;
register int  curs;
{
	register int	oldcurs;

	if (curs != TNOCURS && curs != TBLKCURS)
		return(-1);
	oldcurs = t->cursor;
	if (curs != oldcurs)
		togglecurs(t);
	t->cursor = curs;
	return(oldcurs);
}


xt_close(t)				/* close text window */
register TEXTWIND  *t;
{
	register int  i;

	XFreeFont(t->f.id);
	XDestroyWindow(t->w);
	for (i = 0; i < t->nr; i++)
		free(t->lp[i]);
	free((char *)t->lp);
	free((char *)t);
}
