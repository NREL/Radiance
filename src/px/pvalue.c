/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  pvalue.c - program to print pixel values.
 *
 *     4/23/86
 */

#include  <stdio.h>

#include  "color.h"

#define  min(a,b)		((a)<(b)?(a):(b))

int  xres = 0;			/* resolution of input */
int  yres = 0;

int  uniq = 0;			/* unique values? */

int  original = 0;		/* original values? */

int  dataonly = 0;		/* data only format? */

int  brightonly = 0;		/* only brightness values? */

int  reverse = 0;		/* reverse conversion? */

int  format = 'a';		/* input/output format */

int  header = 1;		/* do header */

COLOR  exposure = WHTCOLOR;

char  *progname;

FILE  *fin;

int  (*getval)(), (*putval)();


main(argc, argv)
int  argc;
char  **argv;
{
	extern double  atof();
	extern int  checkhead();
	int  i;

	progname = argv[0];

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'h':		/* no header */
				header = 0;
				break;
			case 'u':		/* unique values */
				uniq = 1;
				break;
			case 'o':		/* original values */
				original = 1;
				break;
			case 'r':		/* reverse conversion */
				reverse = 1;
				break;
			case 'b':		/* brightness values */
				brightonly = 1;
				break;
			case 'd':		/* data only (no indices) */
				dataonly = 1;
				switch (argv[i][2]) {
				case '\0':
				case 'a':		/* ascii */
					format = 'a';
					break;
				case 'i':		/* integer */
				case 'b':		/* byte */
				case 'f':		/* float */
				case 'd':		/* double */
					format = argv[i][2];
					break;
				default:
					goto unkopt;
				}
				break;
			case 'x':		/* x resolution */
				xres = atoi(argv[++i]);
				break;
			case 'y':		/* y resolution */
				yres = atoi(argv[++i]);
				break;
			default:
unkopt:
				fprintf(stderr, "%s: unknown option: %s\n",
						progname, argv[i]);
				quit(1);
				break;
			}
		else
			break;
			
	if (i == argc) {
		fin = stdin;
	} else if (i == argc-1) {
		if ((fin = fopen(argv[i], "r")) == NULL) {
			fprintf(stderr, "%s: can't open file \"%s\"\n",
						progname, argv[i]);
			quit(1);
		}
	} else {
		fprintf(stderr, "%s: bad # file arguments\n", progname);
		quit(1);
	}

	set_io();

	if (reverse) {
		if (header)			/* get header */
			copyheader(fin, stdout);
						/* add to header */
		printargs(i, argv, stdout);
		printf("\n");
		if (yres <= 0 || xres <= 0) {
			fprintf(stderr, "%s: missing x and y resolution\n",
					progname);
			quit(1);
		}
		fputresolu(YMAJOR|YDECR, xres, yres, stdout);
		valtopix();
	} else {
						/* get header */
		getheader(fin, checkhead);

		if (xres <= 0 || yres <= 0)		/* get picture size */
			if (fgetresolu(&xres, &yres, fin) != (YMAJOR|YDECR)) {
				fprintf(stderr,
				"%s: missing x and y resolution\n",
						progname);
				quit(1);
			}
		if (header) {
			printargs(i, argv, stdout);
			printf("\n");
		}
		pixtoval();
	}

	quit(0);
}


checkhead(line)				/* deal with line from header */
char  *line;
{
	double	d;
	COLOR	ctmp;

	if (header)
		fputs(line, stdout);
	if (isexpos(line)) {
		d = 1.0/exposval(line);
		scalecolor(exposure, d);
	} else if (iscolcor(line)) {
		colcorval(ctmp, line);
		colval(exposure,RED) /= colval(ctmp,RED);
		colval(exposure,GRN) /= colval(ctmp,GRN);
		colval(exposure,BLU) /= colval(ctmp,BLU);
	}
}


pixtoval()				/* convert picture to values */
{
	COLOR  *scanln;
	COLOR  lastc;
	int  y;
	register int  x;

	scanln = (COLOR *)malloc(xres*sizeof(COLOR));
	if (scanln == NULL) {
		fprintf(stderr, "%s: out of memory\n", progname);
		quit(1);
	}
	setcolor(lastc, 0.0, 0.0, 0.0);
	for (y = yres-1; y >= 0; y--) {
		if (freadscan(scanln, xres, fin) < 0) {
			fprintf(stderr, "%s: read error\n", progname);
			quit(1);
		}
		for (x = 0; x < xres; x++) {
			if (uniq)
				if (	scanln[x][RED] == lastc[RED] &&
					scanln[x][GRN] == lastc[GRN] &&
					scanln[x][BLU] == lastc[BLU]	)
					continue;
				else
					copycolor(lastc, scanln[x]);
			if (original)
				multcolor(scanln[x], exposure);
			if (!dataonly)
				printf("%7d %7d ", x, y);
			if ((*putval)(scanln[x], stdout) < 0) {
				fprintf(stderr, "%s: write error\n", progname);
				quit(1);
			}
		}
	}
	free((char *)scanln);
}


valtopix()			/* convert values to a pixel file */
{
	COLOR  *scanln;
	int  y;
	register int  x;

	scanln = (COLOR *)malloc(xres*sizeof(COLOR));
	if (scanln == NULL) {
		fprintf(stderr, "%s: out of memory\n", progname);
		quit(1);
	}
	for (y = yres-1; y >= 0; y--) {
		for (x = 0; x < xres; x++) {
			if (!dataonly)
				fscanf(fin, "%*d %*d");
			if ((*getval)(scanln[x], fin) < 0) {
				fprintf(stderr, "%s: read error\n", progname);
				quit(1);
			}
		}
		if (fwritescan(scanln, xres, stdout) < 0
				|| fflush(stdout) < 0) {
			fprintf(stderr, "%s: write error\n", progname);
			quit(1);
		}
	}
	free((char *)scanln);
}


quit(code)
int  code;
{
	exit(code);
}


getcascii(col, fp)		/* get an ascii color value from fp */
COLOR  col;
FILE  *fp;
{
	double  vd[3];

	if (fscanf(fp, "%lf %lf %lf", &vd[0], &vd[1], &vd[2]) != 3)
		return(-1);
	setcolor(col, vd[0], vd[1], vd[2]);
	return(0);
}


getcdouble(col, fp)		/* get a double color value from fp */
COLOR  col;
FILE  *fp;
{
	double  vd[3];

	if (fread((char *)vd, sizeof(double), 3, fp) != 3)
		return(-1);
	setcolor(col, vd[0], vd[1], vd[2]);
	return(0);
}


getcfloat(col, fp)		/* get a float color value from fp */
COLOR  col;
FILE  *fp;
{
	float  vf[3];

	if (fread((char *)vf, sizeof(float), 3, fp) != 3)
		return(-1);
	setcolor(col, vf[0], vf[1], vf[2]);
	return(0);
}


getcint(col, fp)		/* get an int color value from fp */
COLOR  col;
FILE  *fp;
{
	int  vi[3];

	if (fscanf(fp, "%d %d %d", &vi[0], &vi[1], &vi[2]) != 3)
		return(-1);
	setcolor(col,(vi[0]+.5)/256.,(vi[1]+.5)/256.,(vi[2]+.5)/256.);
	return(0);
}


getcbyte(col, fp)		/* get a byte color value from fp */
COLOR  col;
FILE  *fp;
{
	BYTE  vb[3];

	if (fread((char *)vb, sizeof(BYTE), 3, fp) != 3)
		return(-1);
	setcolor(col,(vb[0]+.5)/256.,(vb[1]+.5)/256.,(vb[2]+.5)/256.);
	return(0);
}


getbascii(col, fp)		/* get an ascii brightness value from fp */
COLOR  col;
FILE  *fp;
{
	double  vd;

	if (fscanf(fp, "%lf", &vd) != 1)
		return(-1);
	setcolor(col, vd, vd, vd);
	return(0);
}


getbdouble(col, fp)		/* get a double brightness value from fp */
COLOR  col;
FILE  *fp;
{
	double  vd;

	if (fread((char *)&vd, sizeof(double), 1, fp) != 1)
		return(-1);
	setcolor(col, vd, vd, vd);
	return(0);
}


getbfloat(col, fp)		/* get a float brightness value from fp */
COLOR  col;
FILE  *fp;
{
	float  vf;

	if (fread((char *)&vf, sizeof(float), 1, fp) != 1)
		return(-1);
	setcolor(col, vf, vf, vf);
	return(0);
}


getbint(col, fp)		/* get an int brightness value from fp */
COLOR  col;
FILE  *fp;
{
	int  vi;
	double  d;

	if (fscanf(fp, "%d", &vi) != 1)
		return(-1);
	d = (vi+.5)/256.;
	setcolor(col, d, d, d);
	return(0);
}


getbbyte(col, fp)		/* get a byte brightness value from fp */
COLOR  col;
FILE  *fp;
{
	BYTE  vb;
	double  d;

	if (fread((char *)&vb, sizeof(BYTE), 1, fp) != 1)
		return(-1);
	d = (vb+.5)/256.;
	setcolor(col, d, d, d);
	return(0);
}


putcascii(col, fp)			/* put an ascii color to fp */
COLOR  col;
FILE  *fp;
{
	fprintf(fp, "%15.3e %15.3e %15.3e\n",
			colval(col,RED),
			colval(col,GRN),
			colval(col,BLU));

	return(ferror(fp) ? -1 : 0);
}


putcfloat(col, fp)			/* put a float color to fp */
COLOR  col;
FILE  *fp;
{
	float  vf[3];

	vf[0] = colval(col,RED);
	vf[1] = colval(col,GRN);
	vf[2] = colval(col,BLU);
	fwrite((char *)vf, sizeof(float), 3, fp);

	return(ferror(fp) ? -1 : 0);
}


putcdouble(col, fp)			/* put a double color to fp */
COLOR  col;
FILE  *fp;
{
	double  vd[3];

	vd[0] = colval(col,RED);
	vd[1] = colval(col,GRN);
	vd[2] = colval(col,BLU);
	fwrite((char *)vd, sizeof(double), 3, fp);

	return(ferror(fp) ? -1 : 0);
}


putcint(col, fp)			/* put an int color to fp */
COLOR  col;
FILE  *fp;
{
	fprintf(fp, "%d %d %d\n",
			(int)(colval(col,RED)*256.),
			(int)(colval(col,GRN)*256.),
			(int)(colval(col,BLU)*256.));

	return(ferror(fp) ? -1 : 0);
}


putcbyte(col, fp)			/* put a byte color to fp */
COLOR  col;
FILE  *fp;
{
	register int  i;
	BYTE  vb[3];

	i = colval(col,RED)*256.;
	vb[0] = min(i,255);
	i = colval(col,GRN)*256.;
	vb[1] = min(i,255);
	i = colval(col,BLU)*256.;
	vb[2] = min(i,255);
	fwrite((char *)vb, sizeof(BYTE), 3, fp);

	return(ferror(fp) ? -1 : 0);
}


putbascii(col, fp)			/* put an ascii brightness to fp */
COLOR  col;
FILE  *fp;
{
	fprintf(fp, "%15.3e\n", bright(col));

	return(ferror(fp) ? -1 : 0);
}


putbfloat(col, fp)			/* put a float brightness to fp */
COLOR  col;
FILE  *fp;
{
	float  vf;

	vf = bright(col);
	fwrite((char *)&vf, sizeof(float), 1, fp);

	return(ferror(fp) ? -1 : 0);
}


putbdouble(col, fp)			/* put a double brightness to fp */
COLOR  col;
FILE  *fp;
{
	double  vd;

	vd = bright(col);
	fwrite((char *)&vd, sizeof(double), 1, fp);

	return(ferror(fp) ? -1 : 0);
}


putbint(col, fp)			/* put an int brightness to fp */
COLOR  col;
FILE  *fp;
{
	fprintf(fp, "%d\n", (int)(bright(col)*256.));

	return(ferror(fp) ? -1 : 0);
}


putbbyte(col, fp)			/* put a byte brightness to fp */
COLOR  col;
FILE  *fp;
{
	register int  i;
	BYTE  vb;

	i = bright(col)*256.;
	vb = min(i,255);
	fwrite((char *)&vb, sizeof(BYTE), 1, fp);

	return(ferror(fp) ? -1 : 0);
}


set_io()			/* set put and get functions */
{
	switch (format) {
	case 'a':					/* ascii */
		if (brightonly) {
			getval = getbascii;
			putval = putbascii;
		} else {
			getval = getcascii;
			putval = putcascii;
		}
		return;
	case 'f':					/* binary float */
		if (brightonly) {
			getval = getbfloat;
			putval = putbfloat;
		} else {
			getval = getcfloat;
			putval = putcfloat;
		}
		return;
	case 'd':					/* binary double */
		if (brightonly) {
			getval = getbdouble;
			putval = putbdouble;
		} else {
			getval = getcdouble;
			putval = putcdouble;
		}
		return;
	case 'i':					/* integer */
		if (brightonly) {
			getval = getbint;
			putval = putbint;
		} else {
			getval = getcint;
			putval = putcint;
		}
		return;
	case 'b':					/* byte */
		if (brightonly) {
			getval = getbbyte;
			putval = putbbyte;
		} else {
			getval = getcbyte;
			putval = putcbyte;
		}
		return;
	}
}
