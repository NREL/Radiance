/* Copyright (c) 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  program to convert between RADIANCE and 24-bit rasterfiles.
 */

#include  <stdio.h>

#include  "rasterfile.h"

#include  "color.h"

extern double  atof(), pow();

double	gamma = 2.0;			/* gamma correction */

char  *progname;

int  xmax, ymax;


main(argc, argv)
int  argc;
char  *argv[];
{
	struct rasterfile  head;
	int  reverse = 0;
	int  i;
	
	progname = argv[0];

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'g':
				gamma = atof(argv[++i]);
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
	if (reverse) {
					/* get header */
		if (fread((char *)&head, sizeof(head), 1, stdin) != 1)
			quiterr("missing header");
		if (head.ras_magic != RAS_MAGIC)
			quiterr("bad raster format");
		xmax = head.ras_width;
		ymax = head.ras_height;
		if (head.ras_type != RT_STANDARD ||
				head.ras_maptype != RMT_NONE ||
				head.ras_depth != 24)
			quiterr("incompatible format");
					/* put header */
		printargs(i, argv, stdout);
		putchar('\n');
		fputresolu(YMAJOR|YDECR, xmax, ymax, stdout);
					/* convert file */
		pr2ra();
	} else {
					/* discard input header */
		getheader(stdin, NULL);
					/* get resolution */
		if (fgetresolu(&xmax, &ymax, stdin) != (YMAJOR|YDECR))
			quiterr("bad picture size");
					/* write rasterfile header */
		head.ras_magic = RAS_MAGIC;
		head.ras_width = xmax;
		head.ras_height = ymax;
		head.ras_depth = 24;
		head.ras_length = xmax*ymax*3;
		head.ras_type = RT_STANDARD;
		head.ras_maptype = RMT_NONE;
		head.ras_maplength = 0;
		fwrite((char *)&head, sizeof(head), 1, stdout);
					/* convert file */
		ra2pr();
	}
	exit(0);
userr:
	fprintf(stderr, "Usage: %s [-r][-g gamma] [input [output]]\n",
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


pr2ra()			/* convert 24-bit scanlines to Radiance picture */
{
	float	gmap[256];
	int	r, g, b;
	COLOR	*scanout;
	register int	x;
	int	y;
						/* allocate scanline */
	scanout = (COLOR *)malloc(xmax*sizeof(COLOR));
	if (scanout == NULL)
		quiterr("out of memory in pr2ra");
						/* compute gamma correction */
	for (x = 0; x < 256; x++)
		gmap[x] = pow((x+.5)/256., gamma);
						/* convert image */
	for (y = ymax-1; y >= 0; y--) {
		for (x = 0; x < xmax; x++) {
			r = getc(stdin); g = getc(stdin);
			if ((b = getc(stdin)) == EOF)
				quiterr("error reading rasterfile");
			setcolor(scanout[x], gmap[r], gmap[g], gmap[b]);
		}
		if (fwritescan(scanout, xmax, stdout) < 0)
			quiterr("error writing Radiance picture");
	}
						/* free scanline */
	free((char *)scanout);
}


ra2pr()			/* convert Radiance scanlines to 24-bit rasterfile */
{
#define map(v)	((v)>=1.0 ? 255 : gmap[(int)(1024.*(v))])
	unsigned char	gmap[1024];
	COLOR	*scanin;
	register int	x;
	register int	c;
	int	y;
						/* allocate scanline */
	scanin = (COLOR *)malloc(xmax*sizeof(COLOR));
	if (scanin == NULL)
		quiterr("out of memory in pr2ra");
						/* compute gamma correction */
	for (x = 0; x < 256; x++)
		gmap[x] = 256.*pow((x+.5)/1024., 1./gamma);
						/* convert image */
	for (y = ymax-1; y >= 0; y--) {
		if (freadscan(scanin, xmax, stdin) < 0)
			quiterr("error reading Radiance picture");
		for (x = 0; x < xmax; x++) {
			c = map(colval(scanin[x],RED));
			putc(c, stdout);
			c = map(colval(scanin[x],GRN));
			putc(c, stdout);
			c = map(colval(scanin[x],BLU));
			putc(c, stdout);
		}
		if (ferror(stdout))
			quiterr("error writing rasterfile");
	}
						/* free scanline */
	free((char *)scanin);
#undef map
}
