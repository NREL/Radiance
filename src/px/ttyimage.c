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


main(argc, argv)
int  argc;
char  **argv;
{
	FILE  *input;
	int  xres, yres;
	COLR  scanline[NCOLS];
	char  sbuf[256];
	register int  i, j;
	
	if (argc < 2)
		input = stdin;
	else if ((input = fopen(argv[1], "r")) == NULL) {
		fprintf(stderr, "%s: can't open file \"%s\"\n", argv[0], argv[1]);
		exit(1);
	}
	
				/* discard header */
	while (fgets(sbuf, sizeof(sbuf), input) != NULL && sbuf[0] != '\n')
		;
				/* get picture dimensions */
	if (fgetresolu(&xres, &yres, input) != (YMAJOR|YDECR)) {
		fprintf(stderr, "%s: bad picture size\n", argv[0]);
		exit(1);
	}
	if (xres > NCOLS) {
		fprintf(stderr, "%s: resolution mismatch\n", argv[0]);
		exit(1);
	}
	
	for (i = 0; i < yres; i++) {
		if (freadcolrs(scanline, xres, input) < 0) {
			fprintf(stderr, "%s: read error\n", argv[0]);
			exit(1);
		}
		for (j = 0; j < xres; j++)
			putchar(shade(scanline[j]));
		putchar('\n');
	}

	exit(0);
}


int
shade(clr)			/* return character for color */
COLR  clr;
{
#define NSHADES  13

	static char  shadech[NSHADES+1] = " .,:;+?%&*$@#";
	COLR  nclr;
	register int  b;

	colr_norm(clr, nclr);
	b = norm_bright(nclr);
	b = b*NSHADES/256;
	return(shadech[b]);

#undef NSHADES
}
