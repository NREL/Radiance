/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  oki20c.c - program to dump pixel file to OkiMate 20 color printer.
 *
 *     6/10/87
 */

#include  <stdio.h>

#include  "color.h"


#define  NROWS		1440		/* 10" at 144 dpi */
#define  NCOLS		960		/* 8" at 120 dpi */

/*
 *  Subtractive primaries are ordered:  Yellow, Magenta, Cyan.
 */

#define  sub_add(sub)	(2-(sub))	/* map subtractive to additive pri. */

#ifdef  BSD
#define  clearlbuf()	bzero((char *)lpat, sizeof(lpat))
#else
#define  clearlbuf()	(void)memset((char *)lpat, 0, sizeof(lpat))
#endif

long  lpat[NCOLS][3];


main(argc, argv)
int  argc;
char  *argv[];
{
	int  i, status = 0;
	
	if (argc < 2)
		status = printp(NULL) == -1;
	else
		for (i = 1; i < argc; i++)
			status += printp(argv[i]) == -1;
	exit(status);
}


printp(fname)				/* print a picture */
char  *fname;
{
	FILE  *input;
	int  xres, yres;
	COLR  scanline[NCOLS];
	int  i;

	if (fname == NULL) {
		input = stdin;
		fname = "<stdin>";
	} else if ((input = fopen(fname, "r")) == NULL) {
		fprintf(stderr, "%s: cannot open\n", fname);
		return(-1);
	}
				/* discard header */
	if (checkheader(input, COLRFMT, NULL) < 0) {
		fprintf(stderr, "%s: not a Radiance picture\n", fname);
		return(-1);
	}
				/* get picture dimensions */
	if (fgetresolu(&xres, &yres, input) != (YMAJOR|YDECR)) {
		fprintf(stderr, "%s: bad picture size\n", fname);
		return(-1);
	}
	if (xres > NCOLS || yres > NROWS) {
		fprintf(stderr, "%s: resolution mismatch\n", fname);
		return(-1);
	}
				/* set line spacing (overlap for knitting) */
	fputs("\0333\042", stdout);
				/* clear line buffer */
	clearlbuf();
				/* put out scanlines */
	for (i = yres-1; i >= 0; i--) {
		if (freadcolrs(scanline, xres, input) < 0) {
			fprintf(stderr, "%s: read error (y=%d)\n", fname, i);
			return(-1);
		}
		normcolrs(scanline, xres, 0);
		plotscan(scanline, xres, i);
	}
				/* advance page */
	putchar('\f');
	
	fclose(input);

	return(0);
}


plotscan(scan, len, y)			/* plot a scanline */
COLR  scan[];
int  len;
int  y;
{
	int  bpos;
	register long  c;
	register int  i, j;

	if (bpos = y % 23) {

		for (j = 0; j < 3; j++)
			for (i = 0; i < len; i++)
				lpat[i][j] |= (long)colbit(scan[i],i,j) << bpos;

	} else {

		fputs("\033\031", stdout);

		for (j = 0; j < 3; j++) {
			fputs("\033%O", stdout);
			putchar(len & 255);
			putchar(len >> 8);
			for (i = 0; i < len; i++) {
				c = lpat[i][j] | colbit(scan[i],i,j);
							/* repeat this row */
				lpat[i][j] = (c & 1) << 23;
				putchar(c>>16);
				putchar(c>>8 & 255);
				putchar(c & 255);
			}
			putchar('\r');
		}
		putchar('\n');
	}
}


colbit(col, x, s)		/* determine bit value for primary at x */
COLR  col;
register int  x;
int  s;
{
	static int  cerr[NCOLS][3];
	static int  err[3];
	int  b;
	register int  a, ison;

	a = sub_add(s);			/* use additive primary */
	b = col[a];
	err[a] += b + cerr[x][a];
	ison = err[a] < 128;
	if (!ison) err[a] -= 256;
	cerr[x][a] = err[a] /= 2;
	return(ison);
}
