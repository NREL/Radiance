/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  program to convert between RADIANCE and Poskanzer Pixmaps
 */

#include  <stdio.h>

#include  <ctype.h>

#include  "color.h"

extern double  atof(), pow();

int  agryscan(), bgryscan(), aclrscan(), bclrscan();

int  bradj = 0;				/* brightness adjustment */

int  maxval = 255;			/* maximum primary value */

char  *progname;

int  xmax, ymax;


main(argc, argv)
int  argc;
char  *argv[];
{
	double	gamma = 2.2;
	int  binflag = 1;
	int  reverse = 0;
	int  ptype;
	int  i;
	
	progname = argv[0];

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'g':
				gamma = atof(argv[++i]);
				break;
			case 'e':
				if (argv[i+1][0] != '+' && argv[i+1][0] != '-')
					goto userr;
				bradj = atoi(argv[++i]);
				break;
			case 'a':
				binflag = 0;
				break;
			case 'r':
				reverse = !reverse;
				break;
			default:
				goto userr;
			}
		else
			break;

	if (i < argc-2)
		goto userr;
	if (i <= argc-1 && freopen(argv[i], "r", stdin) == NULL) {
		fprintf(stderr, "%s: can't open input \"%s\"\n",
				progname, argv[i]);
		exit(1);
	}
	if (i == argc-2 && freopen(argv[i+1], "w", stdout) == NULL) {
		fprintf(stderr, "can't open output \"%s\"\n",
				progname, argv[i+1]);
		exit(1);
	}
	setcolrgam(gamma);
	if (reverse) {
					/* get header */
		if (getc(stdin) != 'P')
			quiterr("input not a Poskanzer Pixmap");
		ptype = getc(stdin);
		xmax = scanint(stdin);
		ymax = scanint(stdin);
		maxval = scanint(stdin);
					/* put header */
		printargs(i, argv, stdout);
		fputformat(COLRFMT, stdout);
		putchar('\n');
		fputresolu(YMAJOR|YDECR, xmax, ymax, stdout);
					/* convert file */
		switch (ptype) {
		case '2':
			ppm2ra(agryscan);
			break;
		case '5':
			ppm2ra(bgryscan);
			break;
		case '3':
			ppm2ra(aclrscan);
			break;
		case '6':
			ppm2ra(bclrscan);
			break;
		default:
			quiterr("unsupported Pixmap type");
		}
	} else {
					/* get header info. */
		if (checkheader(stdin, COLRFMT, NULL) < 0 ||
			fgetresolu(&xmax, &ymax, stdin) != (YMAJOR|YDECR))
			quiterr("bad picture format");
					/* write PPM header */
		printf("P%c\n%d %d\n%d\n", binflag ? '6' : '3',
				xmax, ymax, maxval);
					/* convert file */
		ra2ppm(binflag);
	}
	exit(0);
userr:
	fprintf(stderr,
		"Usage: %s [-r][-a][-g gamma][-e +/-stops] [input [output]]\n",
			progname);
	exit(1);
}


quiterr(err)		/* print message and exit */
char  *err;
{
	if (err != NULL) {
		fprintf(stderr, "%s: %s\n", progname, err);
		exit(1);
	}
	exit(0);
}


ppm2ra(getscan)		/* convert color Pixmap to Radiance picture */
int  (*getscan)();
{
	COLR	*scanout;
	register int	x;
	int	y;
						/* allocate scanline */
	scanout = (COLR *)malloc(xmax*sizeof(COLR));
	if (scanout == NULL)
		quiterr("out of memory in ppm2ra");
						/* convert image */
	for (y = ymax-1; y >= 0; y--) {
		if ((*getscan)(scanout, xmax, stdin) < 0)
			quiterr("error reading Pixmap");
		gambs_colrs(scanout, xmax);
		if (bradj)
			shiftcolrs(scanout, xmax, bradj);
		if (fwritecolrs(scanout, xmax, stdout) < 0)
			quiterr("error writing Radiance picture");
	}
						/* free scanline */
	free((char *)scanout);
}


ra2ppm(binary)		/* convert Radiance picture to Pixmap */
int  binary;
{
	COLR	*scanin;
	register int	x;
	int	y;
						/* allocate scanline */
	scanin = (COLR *)malloc(xmax*sizeof(COLR));
	if (scanin == NULL)
		quiterr("out of memory in ra2pr");
						/* convert image */
	for (y = ymax-1; y >= 0; y--) {
		if (freadcolrs(scanin, xmax, stdin) < 0)
			quiterr("error reading Radiance picture");
		if (bradj)
			shiftcolrs(scanin, xmax, bradj);
		colrs_gambs(scanin, xmax);
		if (binary)
			for (x = 0; x < xmax; x++) {
				putc(scanin[x][RED], stdout);
				putc(scanin[x][GRN], stdout);
				putc(scanin[x][BLU], stdout);
			}
		else
			for (x = 0; x < xmax; x++)
				printf("%d %d %d\n", scanin[x][RED],
						scanin[x][GRN],
						scanin[x][BLU]);
		if (ferror(stdout))
			quiterr("error writing Pixmap");
	}
						/* free scanline */
	free((char *)scanin);
}


agryscan(scan, len, fp)			/* get an ASCII greyscale scanline */
register COLR  *scan;
register int  len;
FILE  *fp;
{
	while (len-- > 0) {
		scan[0][RED] =
		scan[0][GRN] =
		scan[0][BLU] = normval(scanint(fp));
		scan++;
	}
	return(0);
}


bgryscan(scan, len, fp)			/* get a binary greyscale scanline */
register COLR  *scan;
int  len;
register FILE  *fp;
{
	register int  c;

	while (len-- > 0) {
		if ((c = getc(fp)) == EOF)
			return(-1);
		if (maxval != 255)
			c = normval(c);
		scan[0][RED] =
		scan[0][GRN] =
		scan[0][BLU] = c;
		scan++;
	}
	return(0);
}


aclrscan(scan, len, fp)			/* get an ASCII color scanline */
register COLR  *scan;
register int  len;
FILE  *fp;
{
	while (len-- > 0) {
		scan[0][RED] = normval(scanint(fp));
		scan[0][GRN] = normval(scanint(fp));
		scan[0][BLU] = normval(scanint(fp));
		scan++;
	}
	return(0);
}


bclrscan(scan, len, fp)			/* get a binary color scanline */
register COLR  *scan;
int  len;
register FILE  *fp;
{
	int  r, g, b;

	while (len-- > 0) {
		r = getc(fp);
		g = getc(fp);
		if ((b = getc(fp)) == EOF)
			return(-1);
		if (maxval == 255) {
			scan[0][RED] = r;
			scan[0][GRN] = g;
			scan[0][BLU] = b;
		} else {
			scan[0][RED] = normval(r);
			scan[0][GRN] = normval(g);
			scan[0][BLU] = normval(b);
		}
		scan++;
	}
	return(0);
}


int
scanint(fp)			/* scan the next positive integer value */
register FILE  *fp;
{
	register int  i, c;
tryagain:
	while (isspace(c = getc(fp)))
		;
	if (c == EOF)
		quiterr("unexpected end of file");
	if (c == '#') {		/* comment */
		while ((c = getc(fp)) != EOF && c != '\n')
			;
		goto tryagain;
	}
				/* should be integer */
	i = 0;
	do {
		if (!isdigit(c))
			quiterr("error reading integer");
		i = 10*i + c - '0';
		c = getc(fp);
	} while (c != EOF && !isspace(c));
	return(i);
}


int
normval(v)			/* normalize a value to [0,255] */
register int  v;
{
	if (v >= maxval)
		return(255);
	if (maxval == 255)
		return(v);
	return(v*255L/maxval);
}
