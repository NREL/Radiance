/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  ttyimage.c - program to dump pixel file to dumb terminal.
 *
 *     8/15/85
 */

#include  <stdio.h>

#include  "color.h"


#define  NCOLS		133

char  shadech[] = " .,:;+?%&*$@#";

#define  NSHADES	(sizeof(shadech)-1)

#define  shade(col)	( bright(col)>=1.0 ? NSHADES-1 : \
				(int)(bright(col)*NSHADES) )

char  *progname;


main(argc, argv)
int  argc;
char  **argv;
{
	FILE  *input;
	int  xres, yres;
	COLOR  scanline[NCOLS];
	char  sbuf[256];
	register int  i, j;
	
	progname = argv[0];

	if (argc < 2)
		input = stdin;
	else if ((input = fopen(argv[1], "r")) == NULL) {
		fprintf(stderr, "%s: can't open file \"%s\"\n", progname, argv[1]);
		exit(1);
	}
	
				/* discard header */
	while (fgets(sbuf, sizeof(sbuf), input) != NULL && sbuf[0] != '\n')
		;
				/* get picture dimensions */
	if (fgets(sbuf, sizeof(sbuf), input) == NULL ||
			sscanf(sbuf, "-Y %d +X %d\n", &yres, &xres) != 2) {
		fprintf(stderr, "%s: bad picture size\n", progname);
		exit(1);
	}
	if (xres > NCOLS) {
		fprintf(stderr, "%s: resolution mismatch\n", progname);
		exit(1);
	}
	
	for (i = 0; i < yres; i++) {
		if (freadscan(scanline, xres, input) < 0) {
			fprintf(stderr, "%s: read error\n", progname);
			exit(1);
		}
		for (j = 0; j < xres; j++)
			putchar(shadech[shade(scanline[j])]);
		putchar('\n');
	}

	exit(0);
}
