/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Polygonal font handling routines
 */

#include "standard.h"

#include "font.h"

#define galloc(nv)	(GLYPH *)malloc(sizeof(GLYPH)+2*sizeof(GORD)*(nv))


extern char  *libpath;			/* list of library directories */

static FONT	*fontlist = NULL;	/* list of loaded fonts */


FONT *
getfont(fname)				/* return font fname */
char  *fname;
{
	char  buf[16];
	FILE  *fp;
	char  *pathname, *err;
	int  gn, ngv;
	register int  gv;
	register GLYPH  *g;
	GORD  *gp;
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
				(ngv = atoi(buf)) < 0 || ngv > 32000) {
			err = "bad # vertices for";
			goto fonterr;
		}
		g = galloc(ngv);
		if (g == NULL)
			goto memerr;
		g->nverts = ngv;
		g->left = g->right = g->top = g->bottom = 128;
		ngv *= 2;
		gp = gvlist(g);
		while (ngv--) {
			if (fgetword(buf,sizeof(buf),fp) == NULL ||
					!isint(buf) ||
					(gv = atoi(buf)) < 0 || gv > 255) {
				err = "bad vertex for";
				goto fonterr;
			}
			*gp++ = gv;
			if (ngv & 1) {		/* follow x limits */
				if (gv < g->left)
					g->left = gv;
				else if (gv > g->right)
					g->right = gv;
			} else {		/* follow y limits */
				if (gv < g->bottom)
					g->bottom = gv;
				else if (gv > g->top)
					g->top = gv;
			}
		}
		f->fg[gn] = g;
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


int
squeeztext(sp, tp, f, cis)		/* squeeze text line */
short  *sp;			/* returned character spacing */
char  *tp;			/* text line */
FONT  *f;			/* font */
int  cis;			/* intercharacter spacing */
{
	int  linelen;
	register GLYPH  *gp;

	gp = NULL;
	while (*tp && (gp = f->fg[*tp++&0xff]) == NULL)
		*sp++ = 0;
	cis /= 2;
	linelen = *sp = 0;
	while (gp != NULL) {
		if (gp->nverts) {		/* regular character */
			linelen += *sp++ += cis - gp->left;
			*sp = gp->right + cis;
		} else {			/* space */
			linelen += *sp++;
			*sp = 256;
		}
		gp = NULL;
		while (*tp && (gp = f->fg[*tp++&0xff]) == NULL) {
			linelen += *sp++;
			*sp = 0;
		}
	}
	linelen += *sp;
	return(linelen);
}


proptext(sp, tp, f, cis, nsi)		/* space line proportionally */
short  *sp;			/* returned character spacing */
char  *tp;			/* text line */
FONT  *f;			/* font */
int  cis;			/* target intercharacter spacing */
int  nsi;			/* minimum number of spaces for indent */
{
}


