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

extern char  *libpath;			/* library search path */

typedef unsigned char  GLYPH;

typedef struct font {
	GLYPH  *fg[256];		/* font glyphs */
	char  *name;			/* font file name */
	struct font  *next;		/* next font in list */
}  FONT;

typedef struct tline {
	struct tline  *next;		/* pointer to next line */
					/* followed by the string */
}  TLINE;

#define  TLSTR(l)	((char *)(l+1))

typedef struct {
	FVECT  right, down;		/* right and down unit vectors */
	FONT  *f;			/* our font */
	TLINE  tl;			/* line list */
}  TEXT;

extern char  *fgetword();

TEXT  *gettext();

TLINE  *tlalloc();

FONT  *getfont();

static FONT  *fontlist = NULL;		/* our font list */


text(m, r)
register OBJREC  *m;
RAY  *r;
{
	double  v[3];
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
	d = DOT(D,D) / DOT(t->right,t->right);
	for (i = 0; i < 3; i++)
		t->right[i] *= d;
	fcross(t->down, R, DxR);
	d = DOT(R,R) / DOT(t->down,t->down);
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
		free(TLSTR(tlp));
	free((char *)tp);
	m->os = NULL;
}


intext(p, m)			/* check to see if p is in text glyph */
FVECT  p;
OBJREC  *m;
{
	register TEXT  *tp;
	register TLINE  *tlp;
	double  v[3], y, x;
	int  col;
	register int  lno;
				/* first, compute position in text */
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
	tp = gettext(m);
	for (tlp = tp->tl.next; tlp != NULL; tlp = tlp->next)
		if (--lno < 0)
			break;
	if (tlp == NULL || col >= strlen(TLSTR(tlp)))
		return(0);
	return(inglyph(x, y, tp->f->fg[TLSTR(tlp)[col]]));
}


FONT *
getfont(fname)				/* return font fname */
char  *fname;
{
	char  buf[16];
	FILE  *fp;
	char  *pathname, *err;
	int  gn, ngv, gv;
	register GLYPH  *g;
	register FONT  *f;

	for (f = fontlist; f != NULL; f = f->next)
		if (!strcmp(f->name, fname))
			return(f);
						/* load the font file */
	if ((pathname = getpath(fname, libpath, R_OK)) == NULL) {
		sprintf(errmsg, "cannot find font file \"%s\"", fname);
		error(USER, errmsg);
	}
	f = (FONT *)calloc(1, sizeof(FONT));
	if (f == NULL)
		goto memerr;
	f->name = savestr(fname);
	if ((fp = fopen(pathname, "r")) == NULL) {
		sprintf(errmsg, "cannot open font file \"%s\"",
				pathname);
		error(SYSTEM, errmsg);
	}
	while (fgetword(buf,sizeof(buf),fp) != NULL) {	/* get each glyph */
		if (!isint(buf))
			goto nonint;
		gn = atoi(buf);
		if (gn < 0 || gn > 255) {
			err = "illegal";
			goto fonterr;
		}
		if (f->fg[gn] != NULL) {
			err = "duplicate";
			goto fonterr;
		}
		if (fgetword(buf,sizeof(buf),fp) == NULL || !isint(buf) ||
				(ngv = atoi(buf)) < 0 || ngv > 255) {
			err = "bad # vertices for";
			goto fonterr;
		}
		g = (GLYPH *)malloc((2*ngv+1)*sizeof(GLYPH));
		if (g == NULL)
			goto memerr;
		f->fg[gn] = g;
		*g++ = ngv;
		ngv *= 2;
		while (ngv--) {
			if (fgetword(buf,sizeof(buf),fp) == NULL ||
					!isint(buf) ||
					(gv = atoi(buf)) < 0 || gv > 255) {
				err = "bad vertex for";
				goto fonterr;
			}
			*g++ = gv;
		}
	}
	fclose(fp);
	f->next = fontlist;
	return(fontlist = f);
nonint:
	sprintf(errmsg, "non-integer in font file \"%s\"", pathname);
	error(USER, errmsg);
fonterr:
	sprintf(errmsg, "%s character (%d) in font file \"%s\"",
			err, gn, pathname);
	error(USER, errmsg);
memerr:
	error(SYSTEM, "out of memory in fontglyph");
}


inglyph(x, y, gl)		/* (x,y) within font glyph gl? */
double  x, y;
GLYPH  *gl;
{
	int  n, ncross;
	int  xlb, ylb;
	register GLYPH  *p0, *p1;

	if (gl == NULL)
		return(0);
	x *= 256.0;			/* get glyph coordinates */
	y *= 256.0;
	xlb = x + 0.5;
	ylb = y + 0.5;
	n = *gl++;			/* get # of vertices */
	p0 = gl + 2*(n-1);		/* connect last to first */
	p1 = gl;
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
