/* Copyright (c) 1986 Regents of the University of California */

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

extern GLYPH  *getglyph();

extern FONT  *getfont();

static FONT  *fontlist = NULL;		/* our font list */


text(m, r)
register OBJREC  *m;
RAY  *r;
{
	double  v[3], y, x;
	int  col, lno;
	int  foreground;
	GLYPH  *g;
	register double  *ap;

	if (m->oargs.nsargs - tndx(m) < 1 ||
			m->oargs.nfargs != (m->otype == PAT_BTEXT ? 11 :
					m->otype == PAT_CTEXT ? 15 : 9))
		objerror(m, USER, "bad # arguments");

				/* first, discover position in text */
	ap = m->oargs.farg;
	multp3(v, r->rop, r->robx);
	v[0] -= ap[0];
	v[1] -= ap[1];
	v[2] -= ap[2];
	col = x = DOT(v, ap+3) / DOT(ap+3, ap+3);
	lno = y = DOT(v, ap+6) / DOT(ap+6, ap+6);
	x -= col;
	y = (lno+1) - y;
				/* get the font character, check it */
	if ((g = getglyph(m, lno, col)) == NULL)
		foreground = 0;
	else
		foreground = inglyph(x, y, g);
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
		raymixture(r, omod, OVOID, 1.0);
	} else if (m->otype == PAT_BTEXT) {
		if (foreground)
			scalecolor(r->pcol, ap[9]);
		else
			scalecolor(r->pcol, ap[10]);
	} else { /* PAT_CTEXT */
		COLOR  cval;
		if (foreground)
			setcolor(cval, ap[9], ap[10], ap[11]);
		else
			setcolor(cval, ap[12], ap[13], ap[14]);
		multcolor(r->pcol, cval);
	}
}


GLYPH *
getglyph(tm, lno, col)		/* get a glyph from a text description */
register OBJREC  *tm;
int  lno;
int  col;
{
	extern char  *strcpy(), *fgets();
	FILE  *fp;
	char  linbuf[512];
	register int  i;
	register char  **txt;
	register char  *s;

	if (lno < 0 || col < 0)
		return(NULL);
	if (tm->os == NULL) {
		txt = (char **)malloc(2*sizeof(char **));
		if (txt == NULL)
			goto memerr;
		if (tm->oargs.nsargs - tndx(tm) > 1) {	/* single line */
			s = linbuf;
			for (i = tndx(tm)+1; i < tm->oargs.nsargs; i++) {
				strcpy(s, tm->oargs.sarg[i]);
				s += strlen(s);
				*s++ = ' ';
			}
			*--s = '\0';
			txt[0] = savqstr(linbuf);
			txt[1] = NULL;
		} else {				/* text file */
			if ((s = getpath(tm->oargs.sarg[tndx(tm)],
					libpath)) == NULL) {
				sprintf(errmsg, "cannot find text file \"%s\"",
						tm->oargs.sarg[tndx(tm)]);
				error(USER, errmsg);
			}
			if ((fp = fopen(s, "r")) == NULL) {
				sprintf(errmsg, "cannot open text file \"%s\"",
						s);
				error(SYSTEM, errmsg);
			}
			for (i=0; fgets(linbuf,sizeof(linbuf),fp)!=NULL; i++) {
				s = linbuf + strlen(linbuf) - 1;
				if (*s == '\n')
					*s = '\0';
				txt=(char **)realloc(txt,(i+2)*sizeof(char **));
				if (txt == NULL)
					goto memerr;
				txt[i] = savqstr(linbuf);
			}
			txt[i] = NULL;
			fclose(fp);
		}
		tm->os = (char *)txt;
	}
	txt = (char **)tm->os;
	for (i = 0; i < lno; i++)
		if (txt[i] == NULL)
			break;
	if ((s = txt[i]) == NULL || col >= strlen(s))
		return(NULL);
	else
		return(getfont(tm->oargs.sarg[fndx(tm)])->fg[s[col]]);
memerr:
	error(SYSTEM, "out of memory in getglyph");
}


FONT *
getfont(fname)				/* return font fname */
char  *fname;
{
	FILE  *fp;
	char  *pathname, *err;
	int  gn, ngv, gv;
	register GLYPH  *g;
	register FONT  *f;

	for (f = fontlist; f != NULL; f = f->next)
		if (!strcmp(f->name, fname))
			return(f);
						/* load the font file */
	if ((pathname = getpath(fname, libpath)) == NULL) {
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
	while (fscanf(fp, "%d", &gn) == 1) {	/* get each glyph */
		if (gn < 0 || gn > 255) {
			err = "illegal";
			goto fonterr;
		}
		if (f->fg[gn] != NULL) {
			err = "duplicate";
			goto fonterr;
		}
		if (fscanf(fp, "%d", &ngv) != 1 ||
				ngv < 0 || ngv > 255) {
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
			if (fscanf(fp, "%d", &gv) != 1 ||
					gv < 0 || gv > 255) {
				err = "bad vertex for";
				goto fonterr;
			}
			*g++ = gv;
		}
	}
	fclose(fp);
	f->next = fontlist;
	return(fontlist = f);
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

	if (x < 0.0 || y < 0.0)
		return(0);
	xlb = x *= 255.0;		/* get glyph coordinates */
	ylb = y *= 255.0;
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
