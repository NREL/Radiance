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
	COLOR  scanline[NCOLS];
	int  i;

	if (fname == NULL) {
		input = stdin;
		fname = "<stdin>";
	} else if ((input = fopen(fname, "r")) == NULL) {
		fprintf(stderr, "%s: cannot open\n", fname);
		return(-1);
	}
				/* discard header */
	getheader(input, NULL);
				/* get picture dimensions */
	if (fscanf(input, "-Y %d +X %d\n", &yres, &xres) != 2) {
		fprintf(stderr, "%s: bad picture size\n", fname);
		return(-1);
	}
	if (xres > NCOLS || yres > NROWS) {
		fprintf(stderr, "%s: resolution mismatch\n", fname);
		return(-1);
	}

	fputs("\033A\014\0332", stdout);
	
	for (i = yres-1; i >= 0; i--) {
		if (freadscan(scanline, xres, input) < 0) {
			fprintf(stderr, "%s: read error (y=%d)\n", fname, i);
			return(-1);
		}
		plotscan(scanline, xres, i);
	}

	putchar('\f');
	
	fclose(input);

	return(0);
}


plotscan(scan, len, y)			/* plot a scanline */
COLOR  scan[];
int  len;
int  y;
{
	static long  pat[NCOLS][3];
	int  bpos;
	register long  c;
	register int  i, j;

	if (bpos = y % 24) {

		for (j = 0; j < 3; j++)
			for (i = 0; i < len; i++)
				pat[i][j] |= (long)colbit(scan[i],i,j) << bpos;

	} else {

		fputs("\033\031", stdout);

		for (j = 0; j < 3; j++) {
			fputs("\033%O", stdout);
			putchar(len & 255);
			putchar(len >> 8);
			for (i = 0; i < len; i++) {
				c = pat[i][j] | colbit(scan[i],i,j);
				pat[i][j] = 0;
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
COLOR  col;
register int  x;
int  s;
{
	static float  cerr[NCOLS][3];
	static double  err[3];
	double  b;
	register int  a, ison;

	a = sub_add(s);			/* use additive primary */
	b = colval(col,a);
	if (b > 1.0) b = 1.0;
	err[a] += b + cerr[x][a];
	ison = err[a] < 0.5;
	if (!ison) err[a] -= 1.0;
	cerr[x][a] = err[a] *= 0.5;
	return(ison);
}
