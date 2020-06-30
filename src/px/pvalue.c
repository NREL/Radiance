#ifndef lint
static const char RCSid[] = "$Id: pvalue.c,v 2.40 2020/06/30 22:30:29 greg Exp $";
#endif
/*
 *  pvalue.c - program to print pixel values.
 *
 *     4/23/86
 */

#include  "platform.h"
#include  "standard.h"
#include  "color.h"
#include  "resolu.h"
#include  "view.h"

#define	 min(a,b)		((a)<(b)?(a):(b))

				/* what to put out (also RED, GRN, BLU) */
#define  ALL		3
#define  BRIGHT		4

RESOLU	picres;			/* resolution of picture */
int  uniq = 0;			/* print only unique values? */
int  doexposure = 0;		/* exposure change? (>100 to print) */
int  dataonly = 0;		/* data only format? */
int  putprim = ALL;		/* what to put out */
int  reverse = 0;		/* reverse conversion? */
int  format = 'a';		/* input/output format */
char  *fmtid = "ascii";		/* format identifier for header */
int  header = 1;		/* do header? */
long  skipbytes = 0;		/* skip bytes in input? */
int  swapbytes = 0;		/* swap bytes? */
int  interleave = 1;		/* file is interleaved? */
int  resolution = 1;		/* put/get resolution string? */
int  original = 0;		/* convert to original values? */
int  wrongformat = 0;		/* wrong input format? */
double	gamcor = 1.0;		/* gamma correction */

RGBPRIMP  outprims = stdprims;	/* output primaries for reverse conversion */
RGBPRIMS  myprims;

int  ord[3] = {RED, GRN, BLU};	/* RGB ordering */
int  rord[4];			/* reverse ordering */

COLOR  exposure = WHTCOLOR;

char  *progname;

FILE  *fin;
FILE  *fin2 = NULL, *fin3 = NULL;	/* for other color channels */


typedef int getfunc_t(COLOR  col);
typedef int putfunc_t(COLOR  col);
typedef double brightfunc_t(COLOR  col);

getfunc_t *getval;
putfunc_t *putval;
brightfunc_t *mybright;

static gethfunc checkhead;
static brightfunc_t rgb_bright, xyz_bright;
static getfunc_t getbascii, getbint, getbdouble, getbfloat, getbbyte, getbword;
static getfunc_t getcascii, getcint, getcdouble, getcfloat, getcbyte, getcword;
static putfunc_t putbascii, putbint, putbdouble, putbfloat, putbbyte, putbword;
static putfunc_t putcascii, putcint, putcdouble, putcfloat, putcbyte, putcword;
static putfunc_t putpascii, putpint, putpdouble, putpfloat, putpbyte, putpword;

static void set_io(void);
static void pixtoval(void);
static void valtopix(void);


static double
rgb_bright(
	COLOR  clr
)
{
	return(bright(clr));
}

static double
xyz_bright(
	COLOR  clr
)
{
	return(clr[CIEY]);
}

int
main(
	int  argc,
	char  **argv
)
{
	const char	*inpmode = "r";
	double		d, expval = 1.0;
	int		i;

	progname = argv[0];
	mybright = &rgb_bright; /* default */

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-' || argv[i][0] == '+')
			switch (argv[i][1]) {
			case 'h':		/* header */
				header = argv[i][0] == '+';
				break;
			case 'H':		/* resolution string */
				resolution = argv[i][0] == '+';
				break;
			case 's':		/* skip bytes in header */
				skipbytes = atol(argv[++i]);
				break;
			case 'u':		/* unique values */
				uniq = argv[i][0] == '-';
				break;
			case 'o':		/* original values */
				original = argv[i][0] == '-';
				break;
			case 'g':		/* gamma correction */
				gamcor = atof(argv[i+1]);
				if (argv[i][0] == '+')
					gamcor = 1.0/gamcor;
				i++;
				break;
			case 'e':		/* exposure correction */
				d = atof(argv[i+1]);
				if (argv[i+1][0] == '-' || argv[i+1][0] == '+')
					d = pow(2.0, d);
				if (argv[i][0] == '-')
					expval *= d;
				scalecolor(exposure, d);
				doexposure++;
				i++;
				break;
			case 'R':		/* reverse byte sequence */
				if (argv[i][0] == '-') {
					ord[0]=BLU; ord[1]=GRN; ord[2]=RED;
				} else {
					ord[0]=RED; ord[1]=GRN; ord[2]=BLU;
				}
				break;
			case 'r':		/* reverse conversion */
				reverse = argv[i][0] == '-';
				break;
			case 'n':		/* non-interleaved RGB */
				interleave = argv[i][0] == '+';
				break;
			case 'b':		/* brightness values */
				putprim = argv[i][0] == '-' ? BRIGHT : ALL;
				break;
			case 'p':		/* primary controls */
				switch (argv[i][2]) {
				/* these two options affect -r conversion */
				case '\0':
					myprims[RED][CIEX] = atof(argv[++i]);
					myprims[RED][CIEY] = atof(argv[++i]);
					myprims[GRN][CIEX] = atof(argv[++i]);
					myprims[GRN][CIEY] = atof(argv[++i]);
					myprims[BLU][CIEX] = atof(argv[++i]);
					myprims[BLU][CIEY] = atof(argv[++i]);
					myprims[WHT][CIEX] = atof(argv[++i]);
					myprims[WHT][CIEY] = atof(argv[++i]);
					outprims = myprims;
					break;
				case 'x': case 'X': outprims = NULL; break;
				/* the following options affect +r only */
				case 'r': case 'R': putprim = RED; break;
				case 'g': case 'G': putprim = GRN; break;
				case 'b': case 'B': putprim = BLU; break;
				default: goto unkopt;
				}
				break;
			case 'd':		/* data only (no indices) */
				dataonly = (argv[i][0] == '-');
				switch (argv[i][2]) {
				case '\0':
				case 'a':		/* ascii */
					format = 'a';
					fmtid = "ascii";
					break;
				case 'i':		/* integer */
					format = 'i';
					fmtid = "ascii";
					break;
				case 'b':		/* byte */
					dataonly = 1;
					format = 'b';
					fmtid = "byte";
					break;
				case 'W':		/* 16-bit swapped */
					swapbytes = 1;
				case 'w':		/* 16-bit */
					dataonly = 1;
					format = 'w';
					fmtid = "16-bit";
					break;
				case 'F':		/* swapped floats */
					swapbytes = 1;
				case 'f':		/* float */
					dataonly = 1;
					format = 'f';
					fmtid = "float";
					break;
				case 'D':		/* swapped doubles */
					swapbytes = 1;
				case 'd':		/* double */
					dataonly = 1;
					format = 'd';
					fmtid = "double";
					break;
				default:
					goto unkopt;
				}
				break;
			case 'x':		/* x resolution */
			case 'X':		/* x resolution */
				resolution = 0;
				if (argv[i][0] == '-')
					picres.rt |= XDECR;
				picres.xr = atoi(argv[++i]);
				break;
			case 'y':		/* y resolution */
			case 'Y':		/* y resolution */
				resolution = 0;
				if (argv[i][0] == '-')
					picres.rt |= YDECR;
				if (picres.xr == 0)
					picres.rt |= YMAJOR;
				picres.yr = atoi(argv[++i]);
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
					/* recognize special formats */
	if (dataonly && format == 'b') {
		if (putprim == ALL)
			fmtid = "24-bit_rgb";
		else
			fmtid = "8-bit_grey";
	}
	if (dataonly && format == 'w') {
		if (putprim == ALL)
			fmtid = "48-bit_rgb";
		else
			fmtid = "16-bit_grey";
	}
	if (!reverse || (format != 'a') & (format != 'i'))
		inpmode = "rb";
					/* assign reverse ordering */
	rord[ord[0]] = 0;
	rord[ord[1]] = 1;
	rord[ord[2]] = 2;
					/* get input */
	if (i == argc) {
		long	n = skipbytes;
		if (inpmode[1] == 'b')
			SET_FILE_BINARY(stdin);
		while (n-- > 0)
			if (getchar() == EOF) {
				fprintf(stderr,
				"%s: cannot skip %ld bytes on standard input\n",
						progname, skipbytes);
				quit(1);
			}
		fin = stdin;
	} else if (i < argc) {
		if ((fin = fopen(argv[i], inpmode)) == NULL) {
			fprintf(stderr, "%s: can't open file \"%s\"\n",
						progname, argv[i]);
			quit(1);
		}
		if (reverse && putprim != BRIGHT && i == argc-3) {
			if ((fin2 = fopen(argv[i+1], inpmode)) == NULL) {
				fprintf(stderr, "%s: can't open file \"%s\"\n",
						progname, argv[i+1]);
				quit(1);
			}
			if ((fin3 = fopen(argv[i+2], inpmode)) == NULL) {
				fprintf(stderr, "%s: can't open file \"%s\"\n",
						progname, argv[i+2]);
				quit(1);
			}
			interleave = -1;
		} else if (i != argc-1)
			fin = NULL;
		if (reverse && putprim != BRIGHT && !interleave) {
			fin2 = fopen(argv[i], inpmode);
			fin3 = fopen(argv[i], inpmode);
		}
		if (skipbytes && (fseek(fin, skipbytes, SEEK_SET) || (fin2 != NULL &&
				fseek(fin2, skipbytes, SEEK_SET) |
				fseek(fin3, skipbytes, SEEK_SET)))) {
			fprintf(stderr, "%s: cannot skip %ld bytes on input\n",
					progname, skipbytes);
			quit(1);
		}
	}
	if (fin == NULL) {
		fprintf(stderr, "%s: bad # file arguments\n", progname);
		quit(1);
	}

	if (reverse) {
		SET_FILE_BINARY(stdout);
					/* get header */
		if (header) {
			getheader(fin, checkhead, stdout);
			if (wrongformat) {
				fprintf(stderr, "%s: wrong input format (expected %s)\n",
						progname, fmtid);
				quit(1);
			}
			if (fin2 != NULL) {
				getheader(fin2, NULL, NULL);
				getheader(fin3, NULL, NULL);
			}
		} else
			newheader("RADIANCE", stdout);
					/* get resolution */
		if ((resolution && !fgetsresolu(&picres, fin)) ||
				picres.xr <= 0 || picres.yr <= 0) {
			fprintf(stderr, "%s: missing resolution\n", progname);
			quit(1);
		}
		if (resolution && fin2 != NULL) {
			RESOLU  pres2;
			if (!fgetsresolu(&pres2, fin2) ||
					pres2.rt != picres.rt ||
					pres2.xr != picres.xr ||
					pres2.yr != picres.yr ||
					!fgetsresolu(&pres2, fin3) ||
					pres2.rt != picres.rt ||
					pres2.xr != picres.xr ||
					pres2.yr != picres.yr) {
				fprintf(stderr, "%s: resolution mismatch\n",
						progname);
				quit(1);
			}
		}
						/* add to header */
		printargs(i, argv, stdout);
		if (expval < .99 || expval > 1.01)
			fputexpos(expval, stdout);
		if (outprims != NULL) {
			if (outprims != stdprims)
				fputprims(outprims, stdout);
			fputformat(COLRFMT, stdout);
		} else				/* XYZ data */
			fputformat(CIEFMT, stdout);
		putchar('\n');
		fputsresolu(&picres, stdout);	/* always put resolution */
		valtopix();
	} else {
		if ((format != 'a') & (format != 'i'))
			SET_FILE_BINARY(stdout);
						/* get header */
		getheader(fin, checkhead, header ? stdout : (FILE *)NULL);
		if (wrongformat) {
			fprintf(stderr,
				"%s: input not a Radiance RGBE picture\n",
					progname);
			quit(1);
		}
		if (!fgetsresolu(&picres, fin)) {
			fprintf(stderr, "%s: missing resolution\n", progname);
			quit(1);
		}
		if (header) {
			printargs(i, argv, stdout);
			printf("NCOMP=%d\n", putprim==ALL ? 3 : 1);
			if (!resolution && dataonly && !uniq)
				printf("NCOLS=%d\nNROWS=%d\n", scanlen(&picres),
						numscans(&picres));
			if (expval < .99 || expval > 1.01)
				fputexpos(expval, stdout);
			if (swapbytes) {
				if (nativebigendian())
					puts("BigEndian=0");
				else
					puts("BigEndian=1");
			} else if ((format != 'a') & (format != 'i') &
						(format != 'b'))
				fputendian(stdout);
			fputformat(fmtid, stdout);
			putchar('\n');
		}
		if (resolution)			/* put resolution */
			fputsresolu(&picres, stdout);
		pixtoval();
	}

	quit(0);
	return 0; /* pro forma return */
}


static int
checkhead(				/* deal with line from header */
	char	*line,
	void	*p
)
{
	FILE	*fout = (FILE *)p;
	char	fmt[MAXFMTLEN];
	double	d;
	COLOR	ctmp;
	int	rv;

	if (formatval(fmt, line)) {
		if (reverse)
			wrongformat = strcmp(fmt, fmtid);
		else if (!strcmp(fmt, CIEFMT))
			mybright = &xyz_bright;
		else if (!strcmp(fmt, COLRFMT))
			mybright = &rgb_bright;
		else
			wrongformat++;
		return(1);
	}
	if (original && isexpos(line)) {
		d = 1.0/exposval(line);
		scalecolor(exposure, d);
		doexposure++;
		return(1);
	}
	if (original && iscolcor(line)) {
		colcorval(ctmp, line);
		setcolor(exposure, colval(exposure,RED)/colval(ctmp,RED),
				colval(exposure,GRN)/colval(ctmp,GRN),
				colval(exposure,BLU)/colval(ctmp,BLU));
		doexposure++;
		return(1);
	}
	if ((rv = isbigendian(line)) >= 0) {
		if (reverse)
			swapbytes = (nativebigendian() != rv);
		return(1);
	}
	if (fout != NULL)
		fputs(line, fout);
	return(0);
}


static void
pixtoval(void)				/* convert picture to values */
{
	COLOR	*scanln;
	int  dogamma;
	COLOR  lastc;
	RREAL  hv[2];
	int  startprim, endprim;
	long  startpos;
	int  x, y;

	scanln = (COLOR *)malloc(scanlen(&picres)*sizeof(COLOR));
	if (scanln == NULL) {
		fprintf(stderr, "%s: out of memory\n", progname);
		quit(1);
	}
	dogamma = gamcor < .95 || gamcor > 1.05;
	if ((putprim == ALL) & !interleave) {
		startprim = RED; endprim = BLU;
		startpos = ftell(fin);
	} else {
		startprim = putprim; endprim = putprim;
	}
	for (putprim = startprim; putprim <= endprim; putprim++) {
		if (putprim != startprim && fseek(fin, startpos, SEEK_SET)) {
			fprintf(stderr, "%s: seek error on input file\n",
					progname);
			quit(1);
		}
		set_io();
		setcolor(lastc, 0.0, 0.0, 0.0);
		for (y = 0; y < numscans(&picres); y++) {
			if (freadscan(scanln, scanlen(&picres), fin) < 0) {
				fprintf(stderr, "%s: read error\n", progname);
				quit(1);
			}
			for (x = 0; x < scanlen(&picres); x++) {
				if (uniq) {
					if (	colval(scanln[x],RED) ==
							colval(lastc,RED) &&
						colval(scanln[x],GRN) ==
							colval(lastc,GRN) &&
						colval(scanln[x],BLU) ==
							colval(lastc,BLU)	)
						continue;
					else
						copycolor(lastc, scanln[x]);
				}
				if (doexposure)
					multcolor(scanln[x], exposure);
				if (dogamma)
					setcolor(scanln[x],
					pow(colval(scanln[x],RED), 1.0/gamcor),
					pow(colval(scanln[x],GRN), 1.0/gamcor),
					pow(colval(scanln[x],BLU), 1.0/gamcor));
				if (!dataonly) {
					pix2loc(hv, &picres, x, y);
					printf("%7d %7d ",
							(int)(hv[0]*picres.xr),
							(int)(hv[1]*picres.yr));
				}
				if ((*putval)(scanln[x]) < 0) {
					fprintf(stderr, "%s: write error\n",
							progname);
					quit(1);
				}
			}
		}
	}
	free((void *)scanln);
}


static void
valtopix(void)			/* convert values to a pixel file */
{
	int	dogamma;
	COLOR	*scanln;
	COLR	rgbe;
	int	x, y;

	scanln = (COLOR *)malloc(scanlen(&picres)*sizeof(COLOR));
	if (scanln == NULL) {
		fprintf(stderr, "%s: out of memory\n", progname);
		quit(1);
	}
	dogamma = gamcor < .95 || gamcor > 1.05;
	set_io();
	for (y = 0; y < numscans(&picres); y++) {
		for (x = 0; x < scanlen(&picres); x++) {
			if (!dataonly) {
				fscanf(fin, "%*d %*d");
				if (fin2 != NULL) {
					fscanf(fin2, "%*d %*d");
					fscanf(fin3, "%*d %*d");
				}
			}
			if ((*getval)(scanln[x]) < 0) {
				fprintf(stderr, "%s: read error\n", progname);
				quit(1);
			}
			if (dogamma)
				setcolor(scanln[x],
					pow(colval(scanln[x],RED), gamcor),
					pow(colval(scanln[x],GRN), gamcor),
					pow(colval(scanln[x],BLU), gamcor));
			if (doexposure)
				multcolor(scanln[x], exposure);
			if (uniq) {		/* uncompressed? */
				setcolr(rgbe,	scanln[x][RED],
						scanln[x][GRN],
						scanln[x][BLU]);
				if (putbinary(rgbe, sizeof(COLR), 1, stdout) != 1)
					goto writerr;
			}
		}
						/* write scan if compressed */
		if (!uniq && fwritescan(scanln, scanlen(&picres), stdout) < 0)
			goto writerr;
	}
	free((void *)scanln);
	return;
writerr:
	fprintf(stderr, "%s: write error\n", progname);
	quit(1);
}


void
quit(code)
int  code;
{
	exit(code);
}


static int
getcascii(		/* get an ascii color value from stream(s) */
	COLOR  col
)
{
	double	vd[3];

	if (fin2 == NULL) {
		if (fscanf(fin, "%lf %lf %lf", &vd[0], &vd[1], &vd[2]) != 3)
			return(-1);
	} else {
		if (fscanf(fin, "%lf", &vd[0]) != 1 ||
				fscanf(fin2, "%lf", &vd[1]) != 1 ||
				fscanf(fin3, "%lf", &vd[2]) != 1)
			return(-1);
	}
	setcolor(col, vd[rord[RED]], vd[rord[GRN]], vd[rord[BLU]]);
	return(0);
}


static int
getcdouble(		/* get a double color value from stream(s) */
	COLOR  col
)
{
	double	vd[3];

	if (fin2 == NULL) {
		if (getbinary(vd, sizeof(double), 3, fin) != 3)
			return(-1);
	} else {
		if (getbinary(vd, sizeof(double), 1, fin) != 1 ||
			getbinary(vd+1, sizeof(double), 1, fin2) != 1 ||
			getbinary(vd+2, sizeof(double), 1, fin3) != 1)
			return(-1);
	}
	if (swapbytes)
		swap64((char *)vd, 3);
	setcolor(col, vd[rord[RED]], vd[rord[GRN]], vd[rord[BLU]]);
	return(0);
}


static int
getcfloat(		/* get a float color value from stream(s) */
	COLOR  col
)
{
	float  vf[3];

	if (fin2 == NULL) {
		if (getbinary(vf, sizeof(float), 3, fin) != 3)
			return(-1);
	} else {
		if (getbinary(vf, sizeof(float), 1, fin) != 1 ||
			getbinary(vf+1, sizeof(float), 1, fin2) != 1 ||
			getbinary(vf+2, sizeof(float), 1, fin3) != 1)
			return(-1);
	}
	if (swapbytes)
		swap32((char *)vf, 3);
	setcolor(col, vf[rord[RED]], vf[rord[GRN]], vf[rord[BLU]]);
	return(0);
}


static int
getcint(		/* get an int color value from stream(s) */
	COLOR  col
)
{
	int  vi[3];

	if (fin2 == NULL) {
		if (fscanf(fin, "%d %d %d", &vi[0], &vi[1], &vi[2]) != 3)
			return(-1);
	} else {
		if (fscanf(fin, "%d", &vi[0]) != 1 ||
				fscanf(fin2, "%d", &vi[1]) != 1 ||
				fscanf(fin3, "%d", &vi[2]) != 1)
			return(-1);
	}
	setcolor(col, (vi[rord[RED]]+.5)/256.,
			(vi[rord[GRN]]+.5)/256., (vi[rord[BLU]]+.5)/256.);
	return(0);
}


static int
getcbyte(		/* get a byte color value from stream(s) */
	COLOR  col
)
{
	uby8  vb[3];

	if (fin2 == NULL) {
		if (getbinary(vb, sizeof(uby8), 3, fin) != 3)
			return(-1);
	} else {
		if (getbinary(vb, sizeof(uby8), 1, fin) != 1 ||
			getbinary(vb+1, sizeof(uby8), 1, fin2) != 1 ||
			getbinary(vb+2, sizeof(uby8), 1, fin3) != 1)
			return(-1);
	}
	setcolor(col, (vb[rord[RED]]+.5)/256.,
			(vb[rord[GRN]]+.5)/256., (vb[rord[BLU]]+.5)/256.);
	return(0);
}


static int
getcword(		/* get a 16-bit color value from stream(s) */
	COLOR  col
)
{
	uint16  vw[3];

	if (fin2 == NULL) {
		if (getbinary(vw, sizeof(uint16), 3, fin) != 3)
			return(-1);
	} else {
		if (getbinary(vw, sizeof(uint16), 1, fin) != 1 ||
			getbinary(vw+1, sizeof(uint16), 1, fin2) != 1 ||
			getbinary(vw+2, sizeof(uint16), 1, fin3) != 1)
			return(-1);
	}
	if (swapbytes)
		swap16((char *)vw, 3);
	setcolor(col, (vw[rord[RED]]+.5)/65536.,
			(vw[rord[GRN]]+.5)/65536., (vw[rord[BLU]]+.5)/65536.);
	return(0);
}


static int
getbascii(		/* get an ascii brightness value from fin */
	COLOR  col
)
{
	double	vd;

	if (fscanf(fin, "%lf", &vd) != 1)
		return(-1);
	setcolor(col, vd, vd, vd);
	return(0);
}


static int
getbdouble(		/* get a double brightness value from fin */
	COLOR  col
)
{
	double	vd;

	if (getbinary(&vd, sizeof(double), 1, fin) != 1)
		return(-1);
	if (swapbytes)
		swap64((char *)&vd, 1);
	setcolor(col, vd, vd, vd);
	return(0);
}


static int
getbfloat(		/* get a float brightness value from fin */
	COLOR  col
)
{
	float  vf;

	if (getbinary(&vf, sizeof(float), 1, fin) != 1)
		return(-1);
	if (swapbytes)
		swap32((char *)&vf, 1);
	setcolor(col, vf, vf, vf);
	return(0);
}


static int
getbint(		/* get an int brightness value from fin */
	COLOR  col
)
{
	int  vi;
	double	d;

	if (fscanf(fin, "%d", &vi) != 1)
		return(-1);
	d = (vi+.5)/256.;
	setcolor(col, d, d, d);
	return(0);
}


static int
getbbyte(		/* get a byte brightness value from fin */
	COLOR  col
)
{
	uby8  vb;
	double	d;

	if (getbinary(&vb, sizeof(uby8), 1, fin) != 1)
		return(-1);
	d = (vb+.5)/256.;
	setcolor(col, d, d, d);
	return(0);
}


static int
getbword(		/* get a 16-bit brightness value from fin */
	COLOR  col
)
{
	uint16  vw;
	double	d;

	if (getbinary(&vw, sizeof(uint16), 1, fin) != 1)
		return(-1);
	if (swapbytes)
		swap16((char *)&vw, 1);
	d = (vw+.5)/65536.;
	setcolor(col, d, d, d);
	return(0);
}


static int
putcascii(			/* put an ascii color to stdout */
	COLOR  col
)
{
	fprintf(stdout, "%15.3e %15.3e %15.3e\n",
			colval(col,ord[0]),
			colval(col,ord[1]),
			colval(col,ord[2]));

	return(ferror(stdout) ? -1 : 0);
}


static int
putcfloat(			/* put a float color to stdout */
	COLOR  col
)
{
	float  vf[3];

	vf[0] = colval(col,ord[0]);
	vf[1] = colval(col,ord[1]);
	vf[2] = colval(col,ord[2]);
	if (swapbytes)
		swap32((char *)vf, 3);
	putbinary(vf, sizeof(float), 3, stdout);

	return(ferror(stdout) ? -1 : 0);
}


static int
putcdouble(			/* put a double color to stdout */
	COLOR  col
)
{
	double	vd[3];

	vd[0] = colval(col,ord[0]);
	vd[1] = colval(col,ord[1]);
	vd[2] = colval(col,ord[2]);
	if (swapbytes)
		swap64((char *)vd, 3);
	putbinary(vd, sizeof(double), 3, stdout);

	return(ferror(stdout) ? -1 : 0);
}


static int
putcint(			/* put an int color to stdout */
	COLOR  col
)
{
	fprintf(stdout, "%d %d %d\n",
			(int)(colval(col,ord[0])*256.),
			(int)(colval(col,ord[1])*256.),
			(int)(colval(col,ord[2])*256.));

	return(ferror(stdout) ? -1 : 0);
}


static int
putcbyte(			/* put a byte color to stdout */
	COLOR  col
)
{
	long  i;
	uby8  vb[3];

	i = colval(col,ord[0])*256.;
	vb[0] = min(i,255);
	i = colval(col,ord[1])*256.;
	vb[1] = min(i,255);
	i = colval(col,ord[2])*256.;
	vb[2] = min(i,255);
	putbinary(vb, sizeof(uby8), 3, stdout);

	return(ferror(stdout) ? -1 : 0);
}


static int
putcword(			/* put a 16-bit color to stdout */
	COLOR  col
)
{
	long  i;
	uint16  vw[3];

	i = colval(col,ord[0])*65536.;
	vw[0] = min(i,65535);
	i = colval(col,ord[1])*65536.;
	vw[1] = min(i,65535);
	i = colval(col,ord[2])*65536.;
	vw[2] = min(i,65535);
	if (swapbytes)
		swap16((char *)vw, 3);
	putbinary(vw, sizeof(uint16), 3, stdout);

	return(ferror(stdout) ? -1 : 0);
}


static int
putbascii(			/* put an ascii brightness to stdout */
	COLOR  col
)
{
	fprintf(stdout, "%15.3e\n", (*mybright)(col));

	return(ferror(stdout) ? -1 : 0);
}


static int
putbfloat(			/* put a float brightness to stdout */
	COLOR  col
)
{
	float  vf;

	vf = (*mybright)(col);
	if (swapbytes)
		swap32((char *)&vf, 1);
	putbinary(&vf, sizeof(float), 1, stdout);

	return(ferror(stdout) ? -1 : 0);
}


static int
putbdouble(			/* put a double brightness to stdout */
	COLOR  col
)
{
	double	vd;

	vd = (*mybright)(col);
	if (swapbytes)
		swap64((char *)&vd, 1);
	putbinary(&vd, sizeof(double), 1, stdout);

	return(ferror(stdout) ? -1 : 0);
}


static int
putbint(			/* put an int brightness to stdout */
	COLOR  col
)
{
	fprintf(stdout, "%d\n", (int)((*mybright)(col)*256.));

	return(ferror(stdout) ? -1 : 0);
}


static int
putbbyte(			/* put a byte brightness to stdout */
	COLOR  col
)
{
	int  i;
	uby8  vb;

	i = (*mybright)(col)*256.;
	vb = min(i,255);
	putbinary(&vb, sizeof(uby8), 1, stdout);

	return(ferror(stdout) ? -1 : 0);
}


static int
putbword(			/* put a 16-bit brightness to stdout */
	COLOR  col
)
{
	long  i;
	uint16  vw;

	i = (*mybright)(col)*65536.;
	vw = min(i,65535);
	if (swapbytes)
		swap16((char *)&vw, 1);
	putbinary(&vw, sizeof(uint16), 1, stdout);

	return(ferror(stdout) ? -1 : 0);
}


static int
putpascii(			/* put an ascii primary to stdout */
	COLOR  col
)
{
	fprintf(stdout, "%15.3e\n", colval(col,putprim));

	return(ferror(stdout) ? -1 : 0);
}


static int
putpfloat(			/* put a float primary to stdout */
	COLOR  col
)
{
	float  vf;

	vf = colval(col,putprim);
	if (swapbytes)
		swap32((char *)&vf, 1);
	putbinary(&vf, sizeof(float), 1, stdout);

	return(ferror(stdout) ? -1 : 0);
}


static int
putpdouble(			/* put a double primary to stdout */
	COLOR  col
)
{
	double	vd;

	vd = colval(col,putprim);
	if (swapbytes)
		swap64((char *)&vd, 1);
	putbinary(&vd, sizeof(double), 1, stdout);

	return(ferror(stdout) ? -1 : 0);
}


static int
putpint(			/* put an int primary to stdout */
	COLOR  col
)
{
	fprintf(stdout, "%d\n", (int)(colval(col,putprim)*256.));

	return(ferror(stdout) ? -1 : 0);
}


static int
putpbyte(			/* put a byte primary to stdout */
	COLOR  col
)
{
	long  i;
	uby8  vb;

	i = colval(col,putprim)*256.;
	vb = min(i,255);
	putbinary(&vb, sizeof(uby8), 1, stdout);

	return(ferror(stdout) ? -1 : 0);
}


static int
putpword(			/* put a 16-bit primary to stdout */
	COLOR  col
)
{
	long  i;
	uint16  vw;

	i = colval(col,putprim)*65536.;
	vw = min(i,65535);
	if (swapbytes)
		swap16((char *)&vw, 1);
	putbinary(&vw, sizeof(uint16), 1, stdout);

	return(ferror(stdout) ? -1 : 0);
}


static void
set_io(void)			/* set put and get functions */
{
	switch (format) {
	case 'a':					/* ascii */
		if (putprim == BRIGHT) {
			getval = getbascii;
			putval = putbascii;
		} else if (putprim != ALL) {
			getval = getbascii;
			putval = putpascii;
		} else {
			getval = getcascii;
			putval = putcascii;
			if (reverse && !interleave) {
				fprintf(stderr,
				"%s: ASCII input files must be interleaved\n",
						progname);
				quit(1);
			}
		}
		return;
	case 'f':					/* binary float */
		if (putprim == BRIGHT) {
			getval = getbfloat;
			putval = putbfloat;
		} else if (putprim != ALL) {
			getval = getbfloat;
			putval = putpfloat;
		} else {
			getval = getcfloat;
			putval = putcfloat;
			if (reverse && !interleave) {
				if (fin2 == NULL)
					goto namerr;
				if (fseek(fin2,
				(long)sizeof(float)*picres.xr*picres.yr, SEEK_CUR))
					goto seekerr;
				if (fseek(fin3,
				(long)sizeof(float)*2*picres.xr*picres.yr, SEEK_CUR))
					goto seekerr;
			}
		}
		return;
	case 'd':					/* binary double */
		if (putprim == BRIGHT) {
			getval = getbdouble;
			putval = putbdouble;
		} else if (putprim != ALL) {
			getval = getbdouble;
			putval = putpdouble;
		} else {
			getval = getcdouble;
			putval = putcdouble;
			if (reverse && !interleave) {
				if (fin2 == NULL)
					goto namerr;
				if (fseek(fin2,
				(long)sizeof(double)*picres.xr*picres.yr, SEEK_CUR))
					goto seekerr;
				if (fseek(fin3,
				(long)sizeof(double)*2*picres.xr*picres.yr, SEEK_CUR))
					goto seekerr;
			}
		}
		return;
	case 'i':					/* integer */
		if (putprim == BRIGHT) {
			getval = getbint;
			putval = putbint;
		} else if (putprim != ALL) {
			getval = getbint;
			putval = putpint;
		} else {
			getval = getcint;
			putval = putcint;
			if (reverse && !interleave) {
				fprintf(stderr,
				"%s: integer input files must be interleaved\n",
						progname);
				quit(1);
			}
		}
		return;
	case 'b':					/* byte */
		if (putprim == BRIGHT) {
			getval = getbbyte;
			putval = putbbyte;
		} else if (putprim != ALL) {
			getval = getbbyte;
			putval = putpbyte;
		} else {
			getval = getcbyte;
			putval = putcbyte;
			if (reverse && !interleave) {
				if (fin2 == NULL)
					goto namerr;
				if (fseek(fin2,
				(long)sizeof(uby8)*picres.xr*picres.yr, SEEK_CUR))
					goto seekerr;
				if (fseek(fin3,
				(long)sizeof(uby8)*2*picres.xr*picres.yr, SEEK_CUR))
					goto seekerr;
			}
		}
		return;
	case 'w':					/* 16-bit */
		if (putprim == BRIGHT) {
			getval = getbword;
			putval = putbword;
		} else if (putprim != ALL) {
			getval = getbword;
			putval = putpword;
		} else {
			getval = getcword;
			putval = putcword;
			if (reverse && !interleave) {
				if (fin2 == NULL)
					goto namerr;
				if (fseek(fin2,
				(long)sizeof(uint16)*picres.xr*picres.yr, SEEK_CUR))
					goto seekerr;
				if (fseek(fin3,
				(long)sizeof(uint16)*2*picres.xr*picres.yr, SEEK_CUR))
					goto seekerr;
			}
		}
		return;
	}
/* badopt: */ /* label not used */
	fprintf(stderr, "%s: botched file type\n", progname);
	quit(1);
namerr:
	fprintf(stderr, "%s: non-interleaved file(s) must be named\n",
			progname);
	quit(1);
seekerr:
	fprintf(stderr, "%s: cannot seek on interleaved input file\n",
			progname);
	quit(1);
}
