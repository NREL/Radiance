/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  ra_pr.c - program to convert between RADIANCE and pixrect picture format.
 *
 *	11/10/87
 *	3/1/88		Added Paul Heckbert's color map allocation
 */

#include  <stdio.h>

#include  <rasterfile.h>

#include  "color.h"

#include  "pic.h"

			/* descriptor for a picture file or frame buffer */
typedef struct {
	char	*name;			/* file name */
	FILE	*fp;			/* file pointer */
	int	nexty;			/* file positioning */
	int	bytes_line;		/* 0 == variable length lines */
	union {
		long	b;			/* initial scanline */
		long	*y;			/* individual scanline */
	} pos;				/* position(s) */
} pic;

extern pic	*openinput(), *openoutput();

extern char	*ecalloc(), *emalloc();

extern long  ftell();

extern double  atof(), pow();

double	gamma = 2.0;			/* gamma correction */

pic	*inpic, *outpic;

char  *progname;

char  errmsg[128];

COLR	*inline;

int  xmax, ymax;


main(argc, argv)
int  argc;
char  *argv[];
{
	colormap  rasmap;
	struct rasterfile  head;
	int  dither = 1;
	int  reverse = 0;
	int  ncolors = 256;
	int  greyscale = 0;
	int  i;
	
	progname = argv[0];

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'd':
				dither = !dither;
				break;
			case 'g':
				gamma = atof(argv[++i]);
				break;
			case 'b':
				greyscale = !greyscale;
				break;
			case 'r':
				reverse = !reverse;
				break;
			case 'c':
				ncolors = atoi(argv[++i]);
				break;
			default:
				goto userr;
			}
		else
			break;

	if (reverse) {
		if (i < argc-2)
			goto userr;
		if (i <= argc-1 && freopen(argv[i], "r", stdin) == NULL) {
			sprintf(errmsg, "can't open input \"%s\"", argv[i]);
			quiterr(errmsg);
		}
		if (i == argc-2 && freopen(argv[i+1], "w", stdout) == NULL) {
			sprintf(errmsg, "can't open output \"%s\"", argv[i+1]);
			quiterr(errmsg);
		}
					/* get header */
		if (fread(&head, sizeof(head), 1, stdin) != 1)
			quiterr("missing header");
		if (head.ras_magic != RAS_MAGIC)
			quiterr("bad raster format");
		xmax = head.ras_width;
		ymax = head.ras_height;
		if (head.ras_type != RT_STANDARD ||
				head.ras_maptype != RMT_EQUAL_RGB ||
				head.ras_depth != 8)
			quiterr("incompatible format");
					/* put header */
		printargs(argc, argv, stdout);
		putchar('\n');
		printf("-Y %d +X %d\n", ymax, xmax);
					/* convert file */
		pr2ra(&head);
	} else {
		if (i > argc-1 || i < argc-2)
			goto userr;
		if ((inpic = openinput(argv[i], &head)) == NULL) {
			sprintf(errmsg, "can't open input \"%s\"", argv[i]);
			quiterr(errmsg);
		}
		if ((outpic = openoutput(i==argc-2 ? argv[i+1] : (char *)NULL,
				&head)) == NULL) {
			sprintf(errmsg, "can't open output \"%s\"", argv[i+1]);
			quiterr(errmsg);
		}
					/* convert file */
		if (greyscale)
		    biq(dither,ncolors,1,rasmap);
		else
		    ciq(dither,ncolors,1,rasmap);
	}
	quiterr(NULL);
userr:
	fprintf(stderr,
	"Usage: %s [-d][-c ncolors][-b][-g gamma] input [output]\n",
			progname);
	fprintf(stderr, "   Or: %s -r [-g gamma] [input [output]]\n",
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


eputs(s)
char *s;
{
	fputs(s, stderr);
}


quit(code)
int code;
{
	exit(code);
}


pic *
openinput(fname, h)		/* open RADIANCE input file */
char  *fname;
register struct rasterfile  *h;
{
	register pic  *p;

	p = (pic *)emalloc(sizeof(pic));
	p->name = fname;
	if (fname == NULL)
		p->fp = stdin;
	else if ((p->fp = fopen(fname, "r")) == NULL)
		return(NULL);
					/* discard header */
	getheader(p->fp, NULL);
	if (fscanf(p->fp, "-Y %d +X %d\n", &ymax, &xmax) != 2)
		quiterr("bad picture size");
	p->nexty = 0;
	p->bytes_line = 0;		/* variable length lines */
	p->pos.y = (long *)ecalloc(ymax, sizeof(long));
	p->pos.y[0] = ftell(p->fp);
					/* assign header */
	h->ras_magic = RAS_MAGIC;
	h->ras_width = xmax + (xmax&1);	/* round to 16 bits */
	h->ras_height = ymax;
	h->ras_depth = 8;
	h->ras_type = RT_STANDARD;
	h->ras_length = h->ras_width*h->ras_height;
	h->ras_maptype = RMT_EQUAL_RGB;
	h->ras_maplength = 256*3;
					/* allocate scanline */
	inline = (COLR *)emalloc(xmax*sizeof(COLR));

	return(p);
}


pic *
openoutput(fname, h)		/* open output rasterfile */
char  *fname;
register struct rasterfile  *h;
{
	register pic  *p;

	p = (pic *)emalloc(sizeof(pic));
	p->name = fname;
	if (fname == NULL)
		p->fp = stdout;
	else if ((p->fp = fopen(fname, "w")) == NULL)
		return(NULL);
					/* write header */
	fwrite(h, sizeof(*h), 1, p->fp);
	p->nexty = -1;			/* needs color map */
	p->bytes_line = h->ras_width;
	p->pos.b = 0;

	return(p);
}


pr2ra(h)			/* pixrect file to RADIANCE file */
struct rasterfile  *h;
{
	BYTE  cmap[3][256];
	COLR  ctab[256];
	COLR  *scanline;
	register int  i, j, c;

	scanline = (COLR *)emalloc(xmax*sizeof(COLR));
					/* get color table */
	for (i = 0; i < 3; i ++)
		if (fread(cmap[i], h->ras_maplength/3, 1, stdin) != 1)
			quiterr("error reading color table");
					/* convert table */
	for (i = 0; i < h->ras_maplength/3; i++)
		setcolr(ctab[i],
				pow((cmap[0][i]+.5)/256.,gamma),
				pow((cmap[1][i]+.5)/256.,gamma),
				pow((cmap[2][i]+.5)/256.,gamma));
					/* convert file */
	for (i = 0; i < ymax; i++) {
		for (j = 0; j < xmax; j++) {
			if ((c = getc(stdin)) == EOF)
				quiterr("error reading rasterfile");
			copycolr(scanline[j], ctab[c]);
		}
		if (xmax & 1)		/* extra byte */
			getc(stdin);
		if (fwritecolrs(scanline, xmax, stdout) < 0)
			quiterr("error writing RADIANCE file");
	}
	free((char *)scanline);
}


picreadline3(y, l3)			/* read in 3-byte scanline */
int  y;
register rgbpixel  *l3;
{
	register BYTE	*l4;
	register int	shift, c;
	int	i;

	if (inpic->nexty != y) {				/* find scanline */
		if (inpic->bytes_line == 0) {
			if (inpic->pos.y[y] == 0) {
				while (inpic->nexty < y) {
					if (freadcolrs(inline, xmax, inpic->fp) < 0)
						quiterr("read error in picreadline3");
					inpic->pos.y[++inpic->nexty] = ftell(inpic->fp);
				}
			} else if (fseek(inpic->fp, inpic->pos.y[y], 0) == EOF)
				quiterr("seek error in picreadline3");
		} else if (fseek(inpic->fp, y*inpic->bytes_line+inpic->pos.b, 0) == EOF)
			quiterr("seek error in picreadline3");
	} else if (inpic->bytes_line == 0 && inpic->pos.y[inpic->nexty] == 0)
		inpic->pos.y[inpic->nexty] = ftell(inpic->fp);
	if (freadcolrs(inline, xmax, inpic->fp) < 0)	/* read scanline */
		quiterr("read error in picreadline3");
	inpic->nexty = y+1;
							/* convert scanline */
	for (l4=inline[0], i=xmax; i--; l4+=4, l3++) {
		shift = l4[EXP] - COLXS;
		if (shift >= 8) {
			l3->r = l3->g = l3->b = 255;
		} else if (shift <= -8) {
			l3->r = l3->g = l3->b = 0;
		} else if (shift > 0) {
			c = l4[RED] << shift;
			l3->r = c > 255 ? 255 : c;
			c = l4[GRN] << shift;
			l3->g = c > 255 ? 255 : c;
			c = l4[BLU] << shift;
			l3->b = c > 255 ? 255 : c;
		} else if (shift < 0) {
			l3->r = l4[RED] >> -shift;
			l3->g = l4[GRN] >> -shift;
			l3->b = l4[BLU] >> -shift;
		} else {
			l3->r = l4[RED];
			l3->g = l4[GRN];
			l3->b = l4[BLU];
		}
	}
}


picwriteline(y, l)			/* write out scanline */
int  y;
register pixel  *l;
{
	if (outpic->nexty != y) {			/* seek to scanline */
		if (outpic->bytes_line == 0) {
			if (outpic->pos.y[y] == 0)
				quiterr("cannot seek in picwriteline");
			else if (fseek(outpic->fp, outpic->pos.y[y], 0) == EOF)
				quiterr("seek error in picwriteline");
		} else if (fseek(outpic->fp, y*outpic->bytes_line+outpic->pos.b, 0) == EOF)
			quiterr("seek error in picwriteline");
	}
						/* write scanline */
	if (fwrite(l, sizeof(pixel), xmax, outpic->fp) != xmax)
		quiterr("write error in picwriteline");
	if (xmax&1)				/* on 16-bit boundary */
		putc(l[xmax-1], outpic->fp);
	outpic->nexty = y+1;
	if (outpic->bytes_line == 0 && outpic->pos.y[outpic->nexty] == 0)
		outpic->pos.y[outpic->nexty] = ftell(outpic->fp);
}


picwritecm(cm)			/* write out color map */
colormap  cm;
{
	register int  i, j;

	if (outpic->nexty != -1 &&
			fseek(outpic->fp, (long)sizeof(struct rasterfile), 0) == EOF)
		quiterr("seek error in picwritecm");
	for (i = 0; i < 3; i++)
		for (j = 0; j < 256; j++)
			putc(cm[i][j], outpic->fp);
	outpic->nexty = 0;
	if (outpic->bytes_line == 0)
		outpic->pos.y[0] = ftell(outpic->fp);
	else
		outpic->pos.b = ftell(outpic->fp);
}


picreadcm(map)			/* do gamma correction if requested */
colormap  map;
{
	register int  i, val;

	for (i = 0; i < 256; i++) {
		val = pow(i/256.0, 1.0/gamma) * 256.0;
		map[0][i] = map[1][i] = map[2][i] = val;
	}
}
