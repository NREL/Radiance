/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  psign.c - produce picture from text.
 *
 *	7/1/87
 */

#include  <stdio.h>

#include  "color.h"

#define  MAXLINE		512	/* longest allowable line */

char  *fontfile = "helvet.fnt";		/* our font file */

COLR  bgcolr = WHTCOLR;			/* background color */
COLR  fgcolr = BLKCOLR;			/* foreground color */

int  direct = 'r';			/* direction (right, up, left, down) */

int  cheight = 32;			/* character height */
double  aspect = 1.67;			/* height/width character aspect */
int  cwidth;				/* computed character width */

unsigned char  *ourbitmap;		/* our output bitmap */
int  xsiz, ysiz;			/* bitmap dimensions */
int  xdim;				/* size of horizontal scan (bytes) */

#define  bitop(x,y,op)		(ourbitmap[(y)*xdim+((x)>>3)] op (1<<((x)&7)))
#define  tstbit(x,y)		bitop(x,y,&)
#define  setbit(x,y)		bitop(x,y,|=)
#define  clrbit(x,y)		bitop(x,y,&=~)
#define  tglbit(x,y)		bitop(x,y,^=)

typedef unsigned char  GLYPH;

GLYPH  *ourfont[128];			/* our font */

typedef struct line {
	char  *s;		/* line w/o LF */
	struct line  *next;	/* next line up */
} LINE;

LINE  *ourtext;				/* our text */
int  nlines, maxline;			/* text dimensions */

extern char  *malloc(), *calloc();
extern FILE  *fropen();


main(argc, argv)
int  argc;
char  *argv[];
{
	int  an;

	for (an = 1; an < argc && argv[an][0] == '-'; an++)
		switch (argv[an][1]) {
		case 'c':			/* color */
			switch (argv[an][2]) {
			case 'f':			/* foreground */
				setcolr(fgcolr, atof(argv[an+1]),
						atof(argv[an+2]),
						atof(argv[an+3]));
				an += 3;
				break;
			case 'b':			/* background */
				setcolr(bgcolr, atof(argv[an+1]),
						atof(argv[an+2]),
						atof(argv[an+3]));
				an += 3;
				break;
			default:
				goto unkopt;
			}
			break;
		case 'f':			/* font */
			fontfile = argv[++an];
			break;
		case 'd':			/* direction */
			switch (argv[an][2]) {
			case 'r':			/* right */
			case 'u':			/* up */
			case 'l':			/* left */
			case 'd':			/* down */
				direct = argv[an][2];
				break;
			default:
				goto unkopt;
			}
			break;
		case 'h':			/* height of characters */
			cheight = atoi(argv[++an]);
			break;
		case 'a':			/* aspect ratio */
			aspect = atof(argv[++an]);
			break;
		default:;
unkopt:
			fprintf(stderr, "%s: unknown option: %s\n",
					argv[0], argv[an]);
			exit(1);
		}
					/* get text */
	if (an == argc)
		gettext(stdin);
	else
		arg_text(argc-an, argv+an);

					/* create bit map */
	makemap();
					/* load font file */
	loadfont();
					/* convert text to bitmap */
	maptext();
					/* print header */
	printargs(argc, argv, stdout);
	fputformat(COLRFMT, stdout);
	putchar('\n');
					/* write out bitmap */
	writemap(stdout);

	exit(0);
}


makemap()			/* create the bit map */
{
	cwidth = cheight/aspect + 0.5;
	if (direct == 'r' || direct == 'l') {
		xsiz = maxline*cwidth;
		ysiz = nlines*cheight;
	} else {			/* reverse orientation */
		xsiz = nlines*cheight;
		ysiz = maxline*cwidth;
	}
	xdim = (xsiz+7)/8;
	ourbitmap = (BYTE *)calloc(ysiz, xdim);
	if (ourbitmap == NULL) {
		fprintf(stderr, "out of memory in makemap\n");
		exit(1);
	}
}


loadfont()			/* load the font file */
{
	FILE  *fp;
	char  *err;
	int  gn, ngv, gv;
	register GLYPH  *g;

	if ((fp = fropen(fontfile)) == NULL) {
		fprintf(stderr, "cannot find font file \"%s\"\n",
				fontfile);
		exit(1);
	}
	while (fscanf(fp, "%d", &gn) == 1) {	/* get each glyph */
		if (gn < 0 || gn > 127) {
			err = "illegal";
			goto fonterr;
		}
		if (ourfont[gn] != NULL) {
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
		ourfont[gn] = g;
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
	return;
fonterr:
	fprintf(stderr, "%s character (%d) in font file \"%s\"\n",
			err, gn, fontfile);
	exit(1);
memerr:
	fprintf(stderr, "out of memory in loadfont\n");
	exit(1);
}


gettext(fp)			/* get text from a file */
FILE  *fp;
{
	char  *fgets();
	char  buf[MAXLINE];
	register LINE  *curl;
	int  len;

	maxline = 0;
	nlines = 0;
	while (fgets(buf, MAXLINE, fp) != NULL) {
		curl = (LINE *)malloc(sizeof(LINE));
		if (curl == NULL)
			goto memerr;
		len = strlen(buf);
		curl->s = malloc(len--);
		if (curl->s == NULL)
			goto memerr;
		strncpy(curl->s, buf, len);
		curl->s[len] = '\0';
		curl->next = ourtext;
		ourtext = curl;
		if (len > maxline)
			maxline = len;
		nlines++;
	}
	return;
memerr:
	fprintf(stderr, "out of memory in gettext\n");
	exit(1);
}


arg_text(ac, av)			/* get text from arguments */
int  ac;
char  *av[];
{
	register char  *cp;

	ourtext = (LINE *)malloc(sizeof(LINE));
	if (ourtext == NULL)
		goto memerr;
	ourtext->s = malloc(MAXLINE);
	if (ourtext->s == NULL)
		goto memerr;
	for (cp = ourtext->s; ac-- > 0; av++) {
		strcpy(cp, *av);
		cp += strlen(*av);
		*cp++ = ' ';
	}
	*--cp = '\0';
	ourtext->next = NULL;
	maxline = strlen(ourtext->s);
	nlines = 1;
	return;
memerr:
	fprintf(stderr, "out of memory in arg_text\n");
	exit(1);
}


maptext()			/* map our text */
{
	register LINE  *curl;
	int  l;
	register int  c;

	for (l = 0, curl = ourtext; curl != NULL; l++, curl = curl->next)
		for (c = strlen(curl->s)-1; c >= 0; c--)
			mapglyph(ourfont[curl->s[c]], c, l);
}


mapglyph(gl, tx0, ty0)		/* convert a glyph */
register GLYPH  *gl;
int  tx0, ty0;
{
	int  n;
	int  p0[2], p1[2];

	if (gl == NULL)
		return;

	tx0 <<= 8; ty0 <<= 8;
	n = *gl++;
	mapcoord(p0, gl[2*n-2]+tx0, gl[2*n-1]+ty0);
	while (n--) {
		mapcoord(p1, gl[0]+tx0, gl[1]+ty0);
		mapedge(p0[0], p0[1], p1[0]-p0[0], p1[1]-p0[1]);
		p0[0] = p1[0]; p0[1] = p1[1];
		gl += 2;
	}
}


mapcoord(p, tx, ty)		/* map text to picture coordinates */
int  p[2], tx, ty;
{
	tx = (long)tx*cwidth >> 8;
	ty = (long)ty*cheight >> 8;

	switch (direct) {
	case 'r':			/* right */
		p[0] = tx;
		p[1] = ty;
		return;
	case 'u':			/* up */
		p[0] = xsiz-1-ty;
		p[1] = tx;
		return;
	case 'l':			/* left */
		p[0] = xsiz-1-tx;
		p[1] = ysiz-1-ty;
		return;
	case 'd':			/* down */
		p[0] = ty;
		p[1] = ysiz-1-tx;
		return;
	}
}


mapedge(x, y, run, rise)		/* map an edge */
register int  x, y;
int  run, rise;
{
	int  xstep;
	int  rise2, run2;
	int  n;

	if (rise == 0)
		return;
						/* always draw up */
	if (rise < 0) {
		x += run;
		y += rise;
		rise = -rise;
		run = -run;
	}
	if (run < 0) {
		xstep = -1;
		run = -run;
	} else
		xstep = 1;
	n = rise;
	run2 = rise2 = 0;
	while (n)
		if (rise2 >= run2) {
			tglbit(x, y);
			n--;
			y++;
			run2 += run;
		} else {
			x += xstep;
			rise2 += rise;
		}
}


writemap(fp)			/* write out bitmap */
FILE  *fp;
{
	COLR  *scanout;
	int  y;
	register int  x;
	register int  inglyph;

	fprintf(fp, "-Y %d +X %d\n", ysiz, xsiz);

	scanout = (COLR *)malloc(xsiz*sizeof(COLR));
	if (scanout == NULL) {
		fprintf(stderr, "out of memory in writemap\n");
		exit(1);
	}
	for (y = ysiz-1; y >= 0; y--) {
		inglyph = 0;
		for (x = 0; x < xsiz; x++) {
			if (tstbit(x, y))
				inglyph ^= 1;
			if (inglyph)
				copycolr(scanout[x], fgcolr);
			else
				copycolr(scanout[x], bgcolr);
		}
		if (fwritecolrs(scanout, xsiz, fp) < 0) {
			fprintf(stderr, "write error in writemap\n");
			exit(1);
		}
	}
	free((char *)scanout);
}
