/* Copyright (c) 1992 Regents of the University of California */

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
#include  "resolu.h"

#define  NROWS		1440		/* 10" at 144 dpi */
#define  NCOLS		960		/* 8" at 120 dpi */

#define  ASPECT		(120./144.)	/* pixel aspect ratio */

#define  FILTER		"pfilt -1 -x %d -y %d -p %f %s",NCOLS,NROWS,ASPECT

#ifdef  BSD
#define  clearlbuf()	bzero((char *)lpat, sizeof(lpat))
#else
#define  clearlbuf()	(void)memset((char *)lpat, 0, sizeof(lpat))
#endif

long  lpat[NCOLS];

int  dofilter = 0;		/* filter through pfilt first? */


main(argc, argv)
int  argc;
char  *argv[];
{
	int  i, status = 0;
	
	if (argc > 1 && !strcmp(argv[1], "-p")) {
		dofilter++;
		argv++; argc--;
	}
#ifdef _IOLBF
	stdout->_flag &= ~_IOLBF;
#endif
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
	char  buf[64];
	FILE  *input;
	int  xres, yres;
	COLR  scanline[NCOLS];
	int  i;

	if (dofilter) {
		if (fname == NULL) {
			sprintf(buf, FILTER, "");
			fname = "<stdin>";
		} else
			sprintf(buf, FILTER, fname);
		if ((input = popen(buf, "r")) == NULL) {
			fprintf(stderr, "Cannot execute: %s\n", buf);
			return(-1);
		}
	} else if (fname == NULL) {
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
	if (fgetresolu(&xres, &yres, input) < 0) {
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
	
	if (dofilter)
		pclose(input);
	else
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
	register int  i;

	if (bpos = y % 23) {

		for (i = 0; i < len; i++)
			lpat[i] |= (long)bit(scan[i],i) << bpos;

	} else {

		fputs("\033%O", stdout);
		putchar(len & 255);
		putchar(len >> 8);
		for (i = 0; i < len; i++) {
			c = lpat[i] | bit(scan[i],i);
						/* repeat this row */
			lpat[i] = (c & 1) << 23;
			putchar(c>>16);
			putchar(c>>8 & 255);
			putchar(c & 255);
		}
		putchar('\r');
		putchar('\n');
		fflush(stdout);
	}
}


bit(col, x)		/* determine bit value for pixel at x */
COLR  col;
register int  x;
{
	static int  cerr[NCOLS];
	static int  err;
	int  b, errp;
	register int  ison;

	b = normbright(col);
	errp = err;
	err += b + cerr[x];
	ison = err < 128;
	if (!ison) err -= 256;
	err /= 3;
	cerr[x] = err + errp;
	return(ison);
}
