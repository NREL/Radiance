/* Copyright (c) 1996 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Condition Radiance picture for display/output
 */

#include "pcond.h"

#include "random.h"


#define	LDMAX		100		/* default max. display luminance */
#define LDMINF		0.01		/* default min. display lum. factor */

int	what2do = 0;			/* desired adjustments */

double	ldmax = LDMAX;			/* maximum output luminance */
double	ldmin = 0.;			/* minimum output luminance */
double	Bldmin, Bldmax;			/* Bl(ldmin) and Bl(ldmax) */

char	*progname;			/* global argv[0] */

char	*infn;				/* input file name */
FILE	*infp;				/* input stream */
VIEW	ourview = STDVIEW;		/* picture view */
int	gotview = 0;			/* picture has view */
double	pixaspect = 1.0;		/* pixel aspect ratio */
RESOLU	inpres;				/* input picture resolution */

COLOR	*fovimg;			/* foveal (1 degree) averaged image */
short	fvxr, fvyr;			/* foveal image resolution */
int	bwhist[HISTRES];		/* luminance histogram */
long	histot;				/* total count of histogram */
double	bwmin, bwmax;			/* histogram limits */
double	bwavg;				/* mean brightness */

double	scalef = 0.;			/* linear scaling factor */


main(argc, argv)
int	argc;
char	*argv[];
{
	static RGBPRIMS	outprimS;
	int	i;
#define	bool(flg)		switch (argv[i][2]) { \
				case '\0': what2do ^= flg; break; \
				case 'y': case 'Y': case 't': case 'T': \
				case '+': case '1': what2do |= flg; break; \
				case 'n': case 'N': case 'f': case 'F': \
				case '-': case '0': what2do &= ~(flg); break; \
				default: goto userr; }

	progname = argv[0];

	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 'h':
			bool(DO_HUMAN);
			break;
		case 'a':
			bool(DO_ACUITY);
			break;
		case 'v':
			bool(DO_VEIL);
			break;
		case 's':
			bool(DO_HSENS);
			break;
		case 'c':
			bool(DO_COLOR);
			break;
		case 'w':
			bool(DO_CWEIGHT);
			break;
		case 'l':
			bool(DO_LINEAR);
			break;
		case 'p':
			if (i+8 >= argc) goto userr;
			outprimS[RED][CIEX] = atof(argv[++i]);
			outprimS[RED][CIEY] = atof(argv[++i]);
			outprimS[GRN][CIEX] = atof(argv[++i]);
			outprimS[GRN][CIEY] = atof(argv[++i]);
			outprimS[BLU][CIEX] = atof(argv[++i]);
			outprimS[BLU][CIEY] = atof(argv[++i]);
			outprimS[WHT][CIEX] = atof(argv[++i]);
			outprimS[WHT][CIEY] = atof(argv[++i]);
			outprims = outprimS;
			break;
		case 'e':
			if (i+1 >= argc) goto userr;
			scalef = atof(argv[++i]);
			if (argv[i][0] == '+' | argv[i][0] == '-')
				scalef = pow(2.0, scalef);
			what2do |= DO_LINEAR;
			break;
		case 'f':
			if (i+1 >= argc) goto userr;
			mbcalfile = argv[++i];
			break;
		case 't':
			if (i+1 >= argc) goto userr;
			ldmax = atof(argv[++i]);
			if (ldmax <= FTINY)
				goto userr;
			break;
		case 'b':
			if (i+1 >= argc) goto userr;
			ldmin = atof(argv[++i]);
			break;
		default:
			goto userr;
		}
	if (mbcalfile != NULL & outprims != stdprims) {
		fprintf(stderr, "%s: only one of -p or -f option supported\n",
				progname);
		exit(1);
	}
	if (outprims == stdprims & inprims != stdprims)
		outprims = inprims;
	if (ldmin <= FTINY)
		ldmin = ldmax*LDMINF;
	else if (ldmin >= ldmax) {
		fprintf(stderr, "%s: Ldmin (%f) >= Ldmax (%f)!\n", progname,
				ldmin, ldmax);
		exit(1);
	}
	Bldmin = Bl(ldmin);
	Bldmax = Bl(ldmax);
	if (i >= argc || i+2 < argc)
		goto userr;
	if ((infp = fopen(infn=argv[i], "r")) == NULL)
		syserror(infn);
	if (i+2 == argc && freopen(argv[i+1], "w", stdout) == NULL)
		syserror(argv[i+1]);
#ifdef MSDOS
	setmode(fileno(infp), O_BINARY);
	setmode(fileno(stdout), O_BINARY);
#endif
	getahead();			/* load input header */
	printargs(argc, argv, stdout);	/* add to output header */
	if (outprims != inprims)
		fputprims(outprims, stdout);
	mapimage();			/* map the picture */
	exit(0);
userr:
	fprintf(stderr, "Usage: %s [-{h|a|v|s|c|l|w}[+-]][-e ev][-p xr yr xg yg xb yb xw yw|-f mbf.cal][-t Ldmax][-b Ldmin] inpic [outpic]\n",
			progname);
	exit(1);
#undef bool
}


syserror(s)				/* report system error and exit */
char	*s;
{
	fprintf(stderr, "%s: ", progname);
	perror(s);
	exit(2);
}


headline(s)				/* process header line */
char	*s;
{
	static RGBPRIMS	inprimS;
	char	fmt[32];

	if (formatval(fmt, s)) {	/* check if format string */
		if (!strcmp(fmt,COLRFMT)) lumf = rgblum;
		else if (!strcmp(fmt,CIEFMT)) lumf = cielum;
		else lumf = NULL;
		return;			/* don't echo */
	}
	if (isprims(s)) {		/* get input primaries */
		primsval(inprimS, s);
		inprims= inprimS;
		return;			/* don't echo */
	}
	if (isexpos(s)) {		/* picture exposure */
		inpexp *= exposval(s);
		return;			/* don't echo */
	}
	if (isaspect(s))		/* pixel aspect ratio */
		pixaspect *= aspectval(s);
	if (isview(s))			/* image view */
		gotview += sscanview(&ourview, s);
	fputs(s, stdout);
}


getahead()			/* load picture header */
{
	char	*err;

	getheader(infp, headline, NULL);
	if (lumf == NULL || !fgetsresolu(&inpres, infp)) {
		fprintf(stderr, "%s: %s: not a Radiance picture\n",
			progname, infn);
		exit(1);
	}
	if (lumf == rgblum)
		comprgb2xyzmat(inrgb2xyz, inprims);
	else if (mbcalfile != NULL) {
		fprintf(stderr, "%s: macbethcal only works with RGB pictures\n",
				progname);
		exit(1);
	}
	if (!gotview || ourview.type == VT_PAR) {
		copystruct(&ourview, &stdview);
		ourview.type = VT_PER;
		if (pixaspect*inpres.yr < inpres.xr) {
			ourview.horiz = 40.0;
			ourview.vert = 2.*180./PI *
				atan(.3639702*pixaspect*inpres.yr/inpres.xr);
		} else {
			ourview.vert = 40.0;
			ourview.horiz = 2.*180./PI *
				atan(.3639702*inpres.xr/pixaspect/inpres.yr);
		}
	}
	if ((err = setview(&ourview)) != NULL) {
		fprintf(stderr, "%s: view error in picture \"%s\": %s\n",
			progname, infn, err);
		exit(1);
	}
}


mapimage()				/* map picture and send to stdout */
{
	COLOR	*scan;

#ifdef DEBUG
	fprintf(stderr, "%s: generating histogram...", progname);
#endif
	fovhist();			/* generate adaptation histogram */
#ifdef DEBUG
	fputs("done\n", stderr);
#endif
	check2do();			/* modify what2do flags */
	if (what2do&DO_VEIL) {
#ifdef DEBUG
		fprintf(stderr, "%s: computing veiling...", progname);
#endif
		compveil();
#ifdef DEBUG
		fputs("done\n", stderr);
#endif
	}
#ifdef DEBUG
	fprintf(stderr, "%s: computing brightness mapping...", progname);
#endif
	if (!(what2do&DO_LINEAR) && mkbrmap() < 0) {	/* make tone map */
		what2do |= DO_LINEAR;		/* use linear scaling */
#ifdef DEBUG
		fputs("failed!\n", stderr);
	} else
		fputs("done\n", stderr);
#else
	}
#endif
	if (what2do&DO_LINEAR) {
		if (scalef <= FTINY) {
			if (what2do&DO_HSENS)
				scalef = htcontrs(Lb(0.5*(Bldmax+Bldmin))) /
						htcontrs(Lb(bwavg));
			else
				scalef = Lb(0.5*(Bldmax+Bldmin)) / Lb(bwavg);
			scalef *= WHTEFFICACY/(inpexp*ldmax);
		}
#ifdef DEBUG
		fprintf(stderr, "%s: linear scaling factor = %f\n",
				progname, scalef);
#endif
		fputexpos(inpexp*scalef, stdout);	/* record exposure */
		if (lumf == cielum) scalef /= WHTEFFICACY;
	}
	putchar('\n');			/* complete header */
	fputsresolu(&inpres, stdout);	/* resolution doesn't change */

	for (scan = firstscan(); scan != NULL; scan = nextscan())
		if (fwritescan(scan, scanlen(&inpres), stdout) < 0) {
			fprintf(stderr, "%s: scanline write error\n",
					progname);
			exit(1);
		}
}


double
centprob(x, y)			/* center-weighting probability function */
int	x, y;
{
	double	xr, yr;

	xr = (x+.5)/fvxr - .5;
	yr = (y+.5)/fvyr - .5;
	return(1. - xr*xr - yr*yr);	/* radial, == 0.5 at corners */
}


fovhist()			/* create foveal sampled image and histogram */
{
	extern FILE	*popen();
	char	combuf[128];
	double	l, b, lwmin, lwmax;
	FILE	*fp;
	int	x, y;

	fvxr = sqrt(ourview.hn2)/FOVDIA + 0.5;
	if (fvxr < 2) fvxr = 2;
	fvyr = sqrt(ourview.vn2)/FOVDIA + 0.5;
	if (fvyr < 2) fvyr = 2;
	if (!(inpres.or & YMAJOR)) {		/* picture is rotated? */
		y = fvyr;
		fvyr = fvxr;
		fvxr = y;
	}
	if ((fovimg = (COLOR *)malloc(fvxr*fvyr*sizeof(COLOR))) == NULL)
		syserror("malloc");
	sprintf(combuf, "pfilt -1 -b -x %d -y %d %s", fvxr, fvyr, infn);
	if ((fp = popen(combuf, "r")) == NULL)
		syserror("popen");
	getheader(fp, NULL, NULL);	/* skip header */
	if (fgetresolu(&x, &y, fp) < 0 || x != fvxr | y != fvyr)
		goto readerr;
	for (y = 0; y < fvyr; y++)
		if (freadscan(fovscan(y), fvxr, fp) < 0)
			goto readerr;
	pclose(fp);
	lwmin = 1e10;			/* find extrema */
	lwmax = 0.;
	for (y = 0; y < fvyr; y++)
		for (x = 0; x < fvxr; x++) {
			l = plum(fovscan(y)[x]);
			if (l < lwmin) lwmin = l;
			if (l > lwmax) lwmax = l;
		}
	if (lwmin < LMIN) lwmin = LMIN;
	if (lwmax > LMAX) lwmax = LMAX;
					/* compute histogram */
	bwmin = Bl(lwmin)*(1. - .01/HISTRES);
	bwmax = Bl(lwmax)*(1. + .01/HISTRES);
	bwavg = 0.;
	for (y = 0; y < fvyr; y++)
		for (x = 0; x < fvxr; x++) {
			if (what2do & DO_CWEIGHT &&
					frandom() > centprob(x,y))
				continue;
			l = plum(fovscan(y)[x]);
			b = Bl(l);
			if (b < bwmin) continue;
			if (b > bwmax) continue;
			bwavg += b;
			bwhc(b)++;
			histot++;
		}
	bwavg /= (double)histot;
	return;
readerr:
	fprintf(stderr, "%s: error reading from pfilt process in fovimage\n",
			progname);
	exit(1);
}


check2do()		/* check histogram to see what isn't worth doing */
{
	long	sum;
	double	b, l;
	register int	i;

					/* check for within display range */
	l = Lb(bwmax)/Lb(bwmin);
	if (l <= ldmax/ldmin)
		what2do |= DO_LINEAR;
					/* determine if veiling significant */
	if (l < 100.)			/* heuristic */
		what2do &= ~DO_VEIL;

	if (!(what2do & (DO_ACUITY|DO_COLOR)))
		return;
					/* find 5th percentile */
	sum = histot*0.05 + .5;
	for (i = 0; i < HISTRES; i++)
		if ((sum -= bwhist[i]) <= 0)
			break;
	b = (i+.5)*(bwmax-bwmin)/HISTRES + bwmin;
	l = Lb(b);
					/* determine if acuity adj. useful */
	if (what2do&DO_ACUITY &&
			hacuity(l) >= (inpres.xr/sqrt(ourview.hn2) +
			inpres.yr/sqrt(ourview.vn2))/(2.*180./PI))
		what2do &= ~DO_ACUITY;
					/* color sensitivity loss? */
	if (l >= TopMesopic)
		what2do &= ~DO_COLOR;
}
