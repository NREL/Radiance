/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Find glare sources in a scene or image.
 *
 *	Greg Ward	March 1991
 */

#include "glare.h"

#define FEQ(a,b)	((a)-(b)<=FTINY&&(a)-(b)<=FTINY)
#define VEQ(v1,v2)	(FEQ((v1)[0],(v2)[0])&&FEQ((v1)[1],(v2)[1]) \
				&&FEQ((v1)[2],(v2)[2]))

char	*rtargv[32] = {"rtrace", "-h", "-ov", "-fff"};
int	rtargc = 4;

VIEW	ourview = STDVIEW;		/* our view */
VIEW	pictview = STDVIEW;		/* picture view */
VIEW	leftview, rightview;		/* leftmost and rightmost views */

char	*picture = NULL;		/* picture file name */
char	*octree = NULL;			/* octree file name */

int	verbose = 0;			/* verbose reporting */
char	*progname;			/* global argv[0] */

double	threshold = 0.;			/* glare threshold */

int	sampdens = SAMPDENS;		/* sample density */
ANGLE	glarang[180] = {AEND};		/* glare calculation angles */
int	nglarangs = 0;
double	maxtheta;			/* maximum angle (in radians) */
int	hsize;				/* horizontal size */
int	hlim;				/* central limit of horizontal */

struct illum	*indirect;		/* array of indirect illuminances */


main(argc, argv)
int	argc;
char	*argv[];
{
	int	gotview = 0;
	int	rval, i;
	char	*err;

	progname = argv[0];
					/* process options */
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		rval = getviewopt(&ourview, argc-i, argv+i);
		if (rval >= 0) {
			i += rval;
			gotview++;
			continue;
		}
		switch (argv[i][1]) {
		case 't':
			threshold = atof(argv[++i]);
			break;
		case 'r':
			sampdens = atoi(argv[++i])/2;
			break;
		case 'v':
			if (argv[i][2] == '\0') {
				verbose++;
				break;
			}
			if (argv[i][2] != 'f')
				goto userr;
			rval = viewfile(argv[++i], &ourview, 0, 0);
			if (rval < 0) {
				fprintf(stderr,
				"%s: cannot open view file \"%s\"\n",
						progname, argv[i]);
				exit(1);
			} else if (rval == 0) {
				fprintf(stderr,
					"%s: bad view file \"%s\"\n",
						progname, argv[i]);
				exit(1);
			} else
				gotview++;
			break;
		case 'g':
			if (argv[i][2] != 'a')
				goto userr;
			if (setscan(glarang, argv[++i]) < 0) {
				fprintf(stderr, "%s: bad angle spec \"%s\"\n",
					progname, argv[i]);
				exit(1);
			}
			break;
		case 'p':
			picture = argv[++i];
			break;
		case 'd':
		case 'l':
			rtargv[rtargc++] = argv[i];
			rtargv[rtargc++] = argv[++i];
			break;
		case 'a':
			rtargv[rtargc++] = argv[i];
			if (argv[i][2] == 'v') {
				rtargv[rtargc++] = argv[++i];
				rtargv[rtargc++] = argv[++i];
			}
			rtargv[rtargc++] = argv[++i];
			break;
		default:
			goto userr;
		}
	}
						/* get octree */
	if (i < argc-1)
		goto userr;
	if (i == argc-1) {
		rtargv[rtargc++] = octree = argv[i];
		rtargv[rtargc] = NULL;
	}
						/* get view */
	if (picture != NULL) {
		rval = viewfile(picture, &pictview, 0, 0);
		if (rval < 0) {
			fprintf(stderr, "%s: cannot open picture file \"%s\"\n",
					progname, picture);
			exit(1);
		} else if (rval == 0) {
			fprintf(stderr,
				"%s: cannot get view from picture \"%s\"\n",
					progname, picture);
			exit(1);
		}
		if (pictview.type == VT_PAR) {
			fprintf(stderr, "%s: %s: cannot use parallel view\n",
					progname, picture);
			exit(1);
		}
		if ((err = setview(&pictview)) != NULL) {
			fprintf(stderr, "%s: %s\n", picture, err);
			exit(1);
		}
	}
	if (!gotview) {
		if (picture == NULL) {
			fprintf(stderr, "%s: must have view or picture\n",
					progname);
			exit(1);
		}
		copystruct(&ourview, &pictview);
	} else if (picture != NULL && !VEQ(ourview.vp, pictview.vp)) {
		fprintf(stderr, "%s: picture must have same viewpoint\n",
				progname);
		exit(1);
	}
	ourview.type = VT_HEM;
	ourview.horiz = ourview.vert = 180.0;
	ourview.hoff = ourview.voff = 0.0;
	fvsum(ourview.vdir, ourview.vdir, ourview.vup,
			-DOT(ourview.vdir,ourview.vup));
	if ((err = setview(&ourview)) != NULL) {
		fprintf(stderr, "%s: %s\n", progname, err);
		exit(1);
	}
	if (octree == NULL && picture == NULL) {
		fprintf(stderr,
		"%s: must specify at least one of picture or octree\n",
				progname);
		exit(1);
	}
	init();					/* initialize program */
	if (threshold <= FTINY)
		comp_thresh();			/* compute glare threshold */
	analyze();				/* analyze view */
	cleanup();				/* tidy up */
						/* print header */
	printargs(argc, argv, stdout);
	fputs(VIEWSTR, stdout);
	fprintview(&ourview, stdout);
	printf("\n\n");
	printsources();				/* print glare sources */
	printillum();				/* print illuminances */
	exit(0);
userr:
	fprintf(stderr,
"Usage: %s [view options][-ga angles][-p picture][[rtrace options] octree]\n",
			progname);
	exit(1);
}


init()				/* initialize global variables */
{
	double	d;
	register int	i;

	if (verbose)
		fprintf(stderr, "%s: initializing data structures...\n",
				progname);
						/* set direction vectors */
	for (i = 0; glarang[i] != AEND; i++)
		;
	if (i > 0 && glarang[0] <= 0 || glarang[i-1] >= 180) {
		fprintf(stderr, "%s: glare angles must be between 1 and 179\n",
				progname);
		exit(1);
	}
	nglarangs = i;
	/* nglardirs = 2*nglarangs + 1; */
	/* vsize = sampdens - 1; */
	if (nglarangs > 0)
		maxtheta = (PI/180.)*glarang[nglarangs-1];
	else
		maxtheta = 0.0;
	hlim = sampdens*maxtheta;
	hsize = hlim + sampdens - 1;
	if (hsize > (int)(PI*sampdens))
		hsize = PI*sampdens;
	indirect = (struct illum *)calloc(nglardirs, sizeof(struct illum));
	if (indirect == NULL)
		memerr("indirect illuminances");
	copystruct(&leftview, &ourview);
	copystruct(&rightview, &ourview);
	spinvector(leftview.vdir, ourview.vdir, ourview.vup, maxtheta);
	spinvector(rightview.vdir, ourview.vdir, ourview.vup, -maxtheta);
	setview(&leftview);
	setview(&rightview);
	indirect[nglarangs].lcos =
	indirect[nglarangs].rcos = cos(maxtheta);
	indirect[nglarangs].lsin =
	-(indirect[nglarangs].rsin = sin(maxtheta));
	indirect[nglarangs].theta = 0.0;
	for (i = 0; i < nglarangs; i++) {
		d = (glarang[nglarangs-1] - glarang[i])*(PI/180.);
		indirect[nglarangs-i-1].lcos =
		indirect[nglarangs+i+1].rcos = cos(d);
		indirect[nglarangs-i-1].lsin =
		-(indirect[nglarangs+i+1].rsin = sin(d));
		d = (glarang[nglarangs-1] + glarang[i])*(PI/180.);
		indirect[nglarangs-i-1].rcos =
		indirect[nglarangs+i+1].lcos = cos(d);
		indirect[nglarangs+i+1].lsin =
		-(indirect[nglarangs-i-1].rsin = sin(d));
		indirect[nglarangs-i-1].theta = -(PI/180.)*glarang[i];
		indirect[nglarangs+i+1].theta = (PI/180.)*glarang[i];
	}
						/* open picture */
	if (picture != NULL) {
		if (verbose)
			fprintf(stderr, "%s: opening picture file \"%s\"...\n",
					progname, picture);
		open_pict(picture);
	}
						/* start rtrace */
	if (octree != NULL) {
		if (verbose) {
			fprintf(stderr,
				"%s: starting luminance calculation...\n\t",
					progname);
			printargs(rtargc, rtargv, stderr);
		}
		fork_rtrace(rtargv);
	}
}


cleanup()				/* close files, wait for children */
{
	if (verbose)
		fprintf(stderr, "%s: cleaning up...\n", progname);
	if (picture != NULL)
		close_pict();
	if (octree != NULL)
		done_rtrace();
}


compdir(vd, x, y)			/* compute direction for x,y */
FVECT	vd;
int	x, y;
{
	FVECT	org;			/* dummy variable */

	if (x <= -hlim)			/* left region */
		return(viewray(org, vd, &leftview,
				(x+hlim)/(2.*sampdens)+.5,
				y/(2.*sampdens)+.5));
	if (x >= hlim)			/* right region */
		return(viewray(org, vd, &rightview,
				(x-hlim)/(2.*sampdens)+.5,
				y/(2.*sampdens)+.5));
						/* central region */
	if (viewray(org, vd, &ourview, .5, y/(2.*sampdens)+.5) < 0)
		return(-1);
	spinvector(vd, vd, ourview.vup, h_theta(x));
	return(0);
}


memerr(s)			/* malloc failure */
char	*s;
{
	fprintf(stderr, "%s: out of memory for %s\n", progname, s);
	exit(1);
}


printsources()			/* print out glare sources */
{
	register struct source	*sp;

	printf("BEGIN glare source\n");
	for (sp = donelist; sp != NULL; sp = sp->next)
		printf("\t%f %f %f\t%f\t%f\n",
				sp->dir[0], sp->dir[1], sp->dir[2],
				sp->dom, sp->brt);
	printf("END glare source\n");
}


printillum()			/* print out indirect illuminances */
{
	register int	i;

	printf("BEGIN indirect illuminance\n");
	for (i = 0; i < nglardirs; i++)
		printf("\t%.0f\t%f\n", (180.0/PI)*indirect[i].theta,
				PI * indirect[i].sum / (double)indirect[i].n);
	printf("END indirect illuminance\n");
}
