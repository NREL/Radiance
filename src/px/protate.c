/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * prot.c - program to rotate picture file 90 degrees clockwise.
 *
 *	2/26/88
 */

#include "standard.h"

#include "color.h"

#include "resolu.h"

int	order;				/* input scanline order */
int	xres, yres;			/* input resolution */

int	correctorder = 0;		/* order correction? */

#ifdef BIGMEM
char	buf[1<<22];			/* output buffer */
#else
char	buf[1<<20];			/* output buffer */
#endif

int	nrows;				/* number of rows output at once */

#define scanbar		((COLR *)buf)

char	*progname;

#define neworder()	(correctorder ? order : \
			(order^(order&YMAJOR?YDECR:XDECR)^YMAJOR))


main(argc, argv)
int	argc;
char	*argv[];
{
	FILE	*fin;

	progname = argv[0];

	if (argc > 2 && !strcmp(argv[1], "-c")) {
		correctorder++;
		argc--; argv++;
	}
	if (argc != 2 && argc != 3) {
		fprintf(stderr, "Usage: %s [-c] infile [outfile]\n", progname);
		exit(1);
	}
	if ((fin = fopen(argv[1], "r")) == NULL) {
		fprintf(stderr, "%s: cannot open\n", argv[1]);
		exit(1);
	}
	if (argc == 3 && freopen(argv[2], "w", stdout) == NULL) {
		fprintf(stderr, "%s: cannot open\n", argv[2]);
		exit(1);
	}
					/* transfer header */
	if (checkheader(fin, COLRFMT, stdout) < 0) {
		fprintf(stderr, "%s: not a Radiance picture\n", progname);
		exit(1);
	}
					/* add new header info. */
	printf("%s%s\n\n", progname, correctorder?" -c":"");
					/* get picture size */
	if ((order = fgetresolu(&xres, &yres, fin)) < 0) {
		fprintf(stderr, "%s: bad picture size\n", progname);
		exit(1);
	}
					/* write new picture size */
	fputresolu(neworder(), yres, xres, stdout);
					/* compute buffer capacity */
	nrows = sizeof(buf)/sizeof(COLR)/yres;
	rotate(fin);			/* rotate the image */
	exit(0);
}


rotate(fp)			/* rotate picture */
FILE	*fp;
{
	register COLR	*inln;
	register int	xoff, inx, iny;
	long	start, ftell();

	if ((inln = (COLR *)malloc(xres*sizeof(COLR))) == NULL) {
		fprintf(stderr, "%s: out of memory\n", progname);
		exit(1);
	}
	start = ftell(fp);
	for (xoff = 0; xoff < xres; xoff += nrows) {
		if (fseek(fp, start, 0) < 0) {
			fprintf(stderr, "%s: seek error\n", progname);
			exit(1);
		}
		for (iny = yres-1; iny >= 0; iny--) {
			if (freadcolrs(inln, xres, fp) < 0) {
				fprintf(stderr, "%s: read error\n", progname);
				exit(1);
			}
			for (inx = 0; inx < nrows && xoff+inx < xres; inx++)
				bcopy((char *)inln[xoff+inx],
						(char *)scanbar[inx*yres+iny],
						sizeof(COLR));
		}
		for (inx = 0; inx < nrows && xoff+inx < xres; inx++)
			if (fwritecolrs(scanbar+inx*yres, yres, stdout) < 0) {
				fprintf(stderr, "%s: write error\n", progname);
				exit(1);
			}
	}
	free((char *)inln);
}
