/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  text.c - functions for text patterns and mixtures.
 *
 *     11/12/86
 */

#include  "ray.h"

#include  "otypes.h"

#include  "font.h"

/*
 *	A text pattern is specified as the text (a file or line),
 *  the upper left anchor point, the right motion vector, the down
 *  motion vector, and the foreground and background brightness.
 *  For a file, the description is as follows:
 *
 *	modifier brighttext id
 *	2 fontfile textfile
 *	0
 *	11
 *		Ax Ay Az
 *		Rx Ry Rz
 *		Dx Dy Dz
 *		foreground background
 *
 *  For a single line, we use:
 *
 *	modifier brighttext id
 *	N+2 fontfile . This is a line with N words...
 *	0
 *	11
 *		Ax Ay Az
 *		Rx Ry Rz
 *		Dx Dy Dz
 *		foreground background
 *
 *  Colortext is identical, except colors are given rather than
 *  brightnesses.  Mixtext has foreground and background modifiers:
 *
 *	modifier mixtext id
 *	4+ foremod backmod fontfile text..
 *	0
 *	9
 *		Ax Ay Az
 *		Rx Ry Rz
 *		Dx Dy Dz
 */

#define  fndx(m)	((m)->otype==MIX_TEXT ? 2 : 0)
#define  tndx(m)	((m)->otype==MIX_TEXT ? 3 : 1)

typedef struct tline {
	struct tline  *next;		/* pointer to next line */
					/* followed by the string */
}  TLINE;

#define  TLSTR(l)	((char *)((l)+1))

typedef struct {
	FVECT  right, down;		/* right and down unit vectors */
	FONT  *f;			/* our font */
	TLINE  tl;			/* line list */
}  TEXT;

extern char  *libpath;

extern char  *fgetword();

TEXT  *gettext();

TLINE  *tlalloc();


text(m, r)
register OBJREC  *m;
RAY  *r;
{
	FVECT  v;
	int  foreground;
				/* get transformed position */
	if (r->rox != NULL)
		multp3(v, r->rop, r->rox->b.xfm);
	else
		VCOPY(v, r->rop);
				/* check if we are within a text glyph */
	foreground = intext(v, m);
				/* modify */
	if (m->otype == MIX_TEXT) {
		OBJECT  omod;
		char  *modname = m->oargs.sarg[foreground ? 0 : 1];
		if (!strcmp(modname, VOIDID))
			omod = OVOID;
		else if ((omod = modifier(modname)) == OVOID) {
			sprintf(errmsg, "undefined modifier \"%s\"", modname);
			objerror(m, USER, errmsg);
		}
		raytexture(r, omod);
	} else if (m->otype == PAT_BTEXT) {
		if (foreground)
			scalecolor(r->pcol, m->oargs.farg[9]);
		else
			scalecolor(r->pcol, m->oargs.farg[10]);
	} else { /* PAT_CTEXT */
		COLOR  cval;
		if (foreground)
			setcolor(cval, m->oargs.farg[9],
					m->oargs.farg[10],
					m->oargs.farg[11]);
		else
			setcolor(cval, m->oargs.farg[12],
					m->oargs.farg[13],
					m->oargs.farg[14]);
		multcolor(r->pcol, cval);
	}
}


TLINE *
tlalloc(s)			/* allocate and assign text line */
char  *s;
{
	extern char  *strcpy();
	register TLINE  *tl;

	tl = (TLINE *)malloc(sizeof(TLINE)+1+strlen(s));
	if (tl == NULL)
		error(SYSTEM, "out of memory in tlalloc");
	tl->next = NULL;
	strcpy(TLSTR(tl), s);
	return(tl);
}


TEXT *
gettext(tm)			/* get text structure for material */
register OBJREC  *tm;
{
#define  R	(tm->oargs.farg+3)
#define  D	(tm->oargs.farg+6)
	extern char  *strcpy(), *fgets();
	FVECT  DxR;
	double  d;
	FILE  *fp;
	char  linbuf[512];
	TEXT  *t;
	register int  i;
	register TLINE  *tlp;
	register char  *s;

	if ((t = (TEXT *)tm->os) != NULL)
		return(t);
						/* check arguments */
	if (tm->oargs.nsargs - tndx(tm) < 1 ||
			tm->oargs.nfargs != (tm->otype == PAT_BTEXT ? 11 :
					tm->otype == PAT_CTEXT ? 15 : 9))
		objerror(tm, USER, "bad # arguments");
	if ((t = (TEXT *)malloc(sizeof(TEXT))) == NULL)
		error(SYSTEM, "out of memory in gettext");
						/* compute vectors */
	fcross(DxR, D, R);
	fcross(t->right, DxR, D);
	d = DOT(D,D)/DOT(t->right,t->right);
	for (i = 0; i < 3; i++)
		t->right[i] *= d;
	fcross(t->down, R, DxR);
	d = DOT(R,R)/DOT(t->down,t->down);
	for (i = 0; i < 3; i++)
		t->down[i] *= d;
						/* get text */
	tlp = &t->tl;
	if (tm->oargs.nsargs - tndx(tm) > 1) {	/* single line */
		s = linbuf;
		for (i = tndx(tm)+1; i < tm->oargs.nsargs; i++) {
			strcpy(s, tm->oargs.sarg[i]);
			s += strlen(s);
			*s++ = ' ';
		}
		*--s = '\0';
		tlp->next = tlalloc(linbuf);
		tlp = tlp->next;
	} else {				/* text file */
		if ((s = getpath(tm->oargs.sarg[tndx(tm)],
				libpath, R_OK)) == NULL) {
			sprintf(errmsg, "cannot find text file \"%s\"",
					tm->oargs.sarg[tndx(tm)]);
			error(USER, errmsg);
		}
		if ((fp = fopen(s, "r")) == NULL) {
			sprintf(errmsg, "cannot open text file \"%s\"",
					s);
			error(SYSTEM, errmsg);
		}
		while (fgets(linbuf, sizeof(linbuf), fp) != NULL) {
			s = linbuf + strlen(linbuf) - 1;
			if (*s == '\n')
				*s = '\0';
			tlp->next = tlalloc(linbuf);
			tlp = tlp->next;
		}
		fclose(fp);
	}
	tlp->next = NULL;
						/* get the font */
	t->f = getfont(tm->oargs.sarg[fndx(tm)]);
						/* we're done */
	tm->os = (char *)t;
	return(t);
#undef  R
#undef  D
}


freetext(m)			/* free text structures associated with m */
OBJREC  *m;
{
	register TEXT  *tp;
	register TLINE  *tlp;

	tp = (TEXT *)m->os;
	if (tp == NULL)
		return;
	for (tlp = tp->tl.next; tlp != NULL; tlp = tlp->next);
		free((char *)tlp);
	free((char *)tp);
	m->os = NULL;
}


intext(p, m)			/* check to see if p is in text glyph */
FVECT  p;
OBJREC  *m;
{
	register TEXT  *tp;
	register TLINE  *tlp;
	FVECT  v;
	double  y, x;
	int  col;
	register int  lno;
				/* first, compute position in text */
	tp = gettext(m);
	v[0] = p[0] - m->oargs.farg[0];
	v[1] = p[1] - m->oargs.farg[1];
	v[2] = p[2] - m->oargs.farg[2];
	col = x = DOT(v, tp->right);
	lno = y = DOT(v, tp->down);
	if (x < 0.0 || y < 0.0)
		return(0);
	x -= (double)col;
	y = (lno+1) - y;
				/* get the font character */
	for (tlp = tp->tl.next; tlp != NULL; tlp = tlp->next)
		if (--lno < 0)
			break;
	if (tlp == NULL || col >= strlen(TLSTR(tlp)))
		return(0);
	return(inglyph(x, y, tp->f->fg[TLSTR(tlp)[col]&0xff]));
}


inglyph(x, y, gl)		/* (x,y) within font glyph gl? */
double  x, y;
register GLYPH  *gl;
{
	int  n, ncross;
	int  xlb, ylb;
	register GORD  *p0, *p1;

	if (gl == NULL)
		return(0);
	x *= 256.0;			/* get glyph coordinates */
	y *= 256.0;
	xlb = x + 0.5;
	ylb = y + 0.5;
	if (gl->left > xlb || gl->right <= xlb ||
			gl->bottom > ylb || gl->top <= ylb)
		return(0);	/* outside extent */
	n = gl->nverts;			/* get # of vertices */
	p0 = gvlist(gl) + 2*(n-1);	/* connect last to first */
	p1 = gvlist(gl);
	ncross = 0;
					/* positive x axis cross test */
	while (n--) {
		if ((p0[1] > ylb) ^ (p1[1] > ylb))
			if (p0[0] > xlb && p1[0] > xlb)
				ncross++;
			else if (p0[0] > xlb || p1[0] > xlb)
				ncross += (p1[1] > p0[1]) ^
						((p0[1]-y)*(p1[0]-x) >
						(p0[0]-x)*(p1[1]-y));
		p0 = p1;
		p1 += 2;
	}
	return(ncross & 01);
}
