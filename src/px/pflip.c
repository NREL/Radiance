/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * flip picture file horizontally and/or vertically
 */

#include "standard.h"

#include "color.h"

int	xres, yres;			/* input resolution */

long	*scanpos;			/* scanline positions */

int	fhoriz, fvert;			/* flip flags */

FILE	*fin;				/* input file */

char	*progname;


main(argc, argv)
int	argc;
char	*argv[];
{
	int	i;

	progname = argv[0];

	for (i = 1; i < argc; i++)
		if (!strcmp(argv[i], "-h"))
			fhoriz++;
		else if (!strcmp(argv[i], "-v"))
			fvert++;
		else
			break;
	if (i >= argc || argv[i][0] == '-') {
		fprintf(stderr, "Usage: %s [-h][-v] infile [outfile]\n",
				progname);
		exit(1);
	}
	if ((fin = fopen(argv[i], "r")) == NULL) {
		fprintf(stderr, "%s: cannot open\n", argv[1]);
		exit(1);
	}
	if (i < argc-1 && freopen(argv[i+1], "w", stdout) == NULL) {
		fprintf(stderr, "%s: cannot open\n", argv[i+1]);
		exit(1);
	}
					/* transfer header */
	copyheader(fin, stdout);
					/* add new header info. */
	printargs(argc, argv, stdout);
	putchar('\n');
					/* get picture size */
	if (fgetresolu(&xres, &yres, fin) != (YMAJOR|YDECR)) {
		fprintf(stderr, "%s: bad picture size\n", progname);
		exit(1);
	}
					/* write new picture size */
	fputresolu(YMAJOR|YDECR, xres, yres, stdout);
					/* goto end if vertical flip */
	if (fvert)
		scanfile();
	flip();				/* flip the image */
	exit(0);
}


memerr()
{
	fprintf(stderr, "%s: out of memory\n", progname);
	exit(1);
}


scanfile()				/* scan to the end of file */
{
	extern long	ftell();
	COLR	*scanin;
	int	y;

	if ((scanpos = (long *)malloc(yres*sizeof(long))) == NULL)
		memerr();
	if ((scanin = (COLR *)malloc(xres*sizeof(COLR))) == NULL)
		memerr();
	for (y = yres-1; y >= 0; y--) {
		scanpos[y] = ftell(fin);
		if (freadcolrs(scanin, xres, fin) < 0) {
			fprintf(stderr, "%s: read error\n", progname);
			exit(1);
		}
	}
	free((char *)scanin);
}


flip()					/* flip the picture */
{
	COLR	*scanin, *scanout;
	int	y;
	register int	x;

	if ((scanin = (COLR *)malloc(xres*sizeof(COLR))) == NULL)
		memerr();
	if (fhoriz) {
		if ((scanout = (COLR *)malloc(xres*sizeof(COLR))) == NULL)
			memerr();
	} else
		scanout = scanin;
	for (y = yres-1; y >= 0; y--) {
		if (fvert && fseek(fin, scanpos[yres-1-y], 0) == EOF) {
			fprintf(stderr, "%s: seek error\n", progname);
			exit(1);
		}
		if (freadcolrs(scanin, xres, fin) < 0) {
			fprintf(stderr, "%s: read error\n", progname);
			exit(1);
		}
		if (fhoriz)
			for (x = 0; x < xres; x++)
				copycolr(scanout[x], scanin[xres-1-x]);
		if (fwritecolrs(scanout, xres, stdout) < 0) {
			fprintf(stderr, "%s: write error\n", progname);
			exit(1);
		}
	}
	free((char *)scanin);
	free((char *)scanout);
}
