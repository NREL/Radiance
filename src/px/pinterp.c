#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Interpolate and extrapolate pictures with different view parameters.
 *
 *	Greg Ward	09Dec89
 */

#include "standard.h"

#include "view.h"

#include "color.h"

#define pscan(y)	(ourpict+(y)*ourview.hresolu)
#define zscan(y)	(ourzbuf+(y)*ourview.hresolu)

#define ABS(x)		((x)>0?(x):-(x))

VIEW	ourview = STDVIEW(512);		/* desired view */

double	zeps = 0.001;			/* allowed z epsilon */

COLR	*ourpict;			/* output picture */
float	*ourzbuf;			/* corresponding z-buffer */

char	*progname;

VIEW	theirview = STDVIEW(512);	/* input view */
int	gotview;			/* got input view? */

double	theirs2ours[4][4];		/* transformation matrix */


main(argc, argv)			/* interpolate pictures */
int	argc;
char	*argv[];
{
#define check(olen,narg)	if (argv[i][olen] || narg >= argc-i) goto badopt
	int	gotvfile = 0;
	char	*err;
	int	i;

	progname = argv[0];

	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 't':				/* threshold */
			check(2,1);
			zeps = atof(argv[++i]);
			break;
		case 'x':				/* x resolution */
			check(2,1);
			ourview.hresolu = atoi(argv[++i]);
			break;
		case 'y':				/* y resolution */
			check(2,1);
			ourview.vresolu = atoi(argv[++i]);
			break;
		case 'v':				/* view */
			switch (argv[i][2]) {
			case 't':				/* type */
				check(4,0);
				ourview.type = argv[i][3];
				break;
			case 'p':				/* point */
				check(3,3);
				ourview.vp[0] = atof(argv[++i]);
				ourview.vp[1] = atof(argv[++i]);
				ourview.vp[2] = atof(argv[++i]);
				break;
			case 'd':				/* direction */
				check(3,3);
				ourview.vdir[0] = atof(argv[++i]);
				ourview.vdir[1] = atof(argv[++i]);
				ourview.vdir[2] = atof(argv[++i]);
				break;
			case 'u':				/* up */
				check(3,3);
				ourview.vup[0] = atof(argv[++i]);
				ourview.vup[1] = atof(argv[++i]);
				ourview.vup[2] = atof(argv[++i]);
				break;
			case 'h':				/* horizontal */
				check(3,1);
				ourview.horiz = atof(argv[++i]);
				break;
			case 'v':				/* vertical */
				check(3,1);
				ourview.vert = atof(argv[++i]);
				break;
			case 'f':				/* file */
				check(3,1);
				gotvfile = viewfile(argv[++i], &ourview);
				if (gotvfile < 0) {
					perror(argv[i]);
					exit(1);
				} else if (gotvfile == 0) {
					fprintf(stderr, "%s: bad view file\n",
							argv[i]);
					exit(1);
				}
				break;
			default:
				goto badopt;
			}
			break;
		default:
		badopt:
			fprintf(stderr, "%s: unknown option '%s'\n",
					progname, argv[i]);
			exit(1);
		}
						/* check arguments */
	if (argc-i < 2 || (argc-i)%2) {
		fprintf(stderr, "Usage: %s [view args] pfile zfile ..\n",
				progname);
		exit(1);
	}
						/* set view */
	if (err = setview(&ourview)) {
		fprintf(stderr, "%s: %s\n", progname, err);
		exit(1);
	}
						/* allocate frame */
	ourpict = (COLR *)calloc(ourview.hresolu*ourview.vresolu,sizeof(COLR));
	ourzbuf = (float *)calloc(ourview.hresolu*ourview.vresolu,sizeof(float));
	if (ourpict == NULL || ourzbuf == NULL) {
		perror(progname);
		exit(1);
	}
							/* get input */
	for ( ; i < argc; i += 2)
		addpicture(argv[i], argv[i+1]);
							/* fill in spaces */
	fillpicture();
							/* add to header */
	printargs(argc, argv, stdout);
	if (gotvfile) {
		printf(VIEWSTR);
		fprintview(&ourview, stdout);
		printf("\n");
	}
	printf("\n");
							/* write output */
	writepicture();

	exit(0);
#undef check
}


headline(s)				/* process header string */
char	*s;
{
	static char	*altname[] = {"rview","rpict","pinterp",VIEWSTR,NULL};
	register char	**an;

	printf("\t%s", s);

	for (an = altname; *an != NULL; an++)
		if (!strncmp(*an, s, strlen(*an))) {
			if (sscanview(&theirview, s+strlen(*an)) == 0)
				gotview++;
			break;
		}
}


addpicture(pfile, zfile)		/* add picture to output */
char	*pfile, *zfile;
{
	FILE	*pfp, *zfp;
	COLR	*scanin;
	float	*zin;
	char	*err;
	int	xres, yres;
	int	y;
					/* open input files */
	if ((pfp = fopen(pfile, "r")) == NULL) {
		perror(pfile);
		exit(1);
	}
	if ((zfp = fopen(zfile, "r")) == NULL) {
		perror(zfile);
		exit(1);
	}
					/* get header and view */
	printf("%s:\n", pfile);
	gotview = 0;
	getheader(pfp, headline);
	if (!gotview || fgetresolu(&xres, &yres, pfp) != (YMAJOR|YDECR)) {
		fprintf(stderr, "%s: picture view error\n", pfile);
		exit(1);
	}
	theirview.hresolu = xres;
	theirview.vresolu = yres;
	if (err = setview(&theirview)) {
		fprintf(stderr, "%s: %s\n", pfile, err);
		exit(1);
	}
					/* compute transformation */
	pixform(theirs2ours, &theirview, &ourview);
					/* allocate scanlines */
	scanin = (COLR *)malloc(xres*sizeof(COLR));
	zin = (float *)malloc(xres*sizeof(float));
	if (scanin == NULL || zin == NULL) {
		perror(progname);
		exit(1);
	}
					/* load image */
	for (y = yres-1; y >= 0; y--) {
		if (freadcolrs(scanin, xres, pfp) < 0) {
			fprintf(stderr, "%s: read error\n", pfile);
			exit(1);
		}
		if (fread(zin, sizeof(float), xres, zfp) < xres) {
			fprintf(stderr, "%s: read error\n", zfile);
			exit(1);
		}
		addscanline(y, scanin, zin);
	}
					/* clean up */
	free((char *)scanin);
	free((char *)zin);
	fclose(pfp);
	fclose(zfp);
}


pixform(xfmat, vw1, vw2)		/* compute view1 to view2 matrix */
register double	xfmat[4][4];
register VIEW	*vw1, *vw2;
{
	double	m4t[4][4];

	setident4(xfmat);
	xfmat[0][0] = vw1->vhinc[0];
	xfmat[0][1] = vw1->vhinc[1];
	xfmat[0][2] = vw1->vhinc[2];
	xfmat[1][0] = vw1->vvinc[0];
	xfmat[1][1] = vw1->vvinc[1];
	xfmat[1][2] = vw1->vvinc[2];
	xfmat[2][0] = vw1->vdir[0];
	xfmat[2][1] = vw1->vdir[1];
	xfmat[2][2] = vw1->vdir[2];
	xfmat[3][0] = vw1->vp[0];
	xfmat[3][1] = vw1->vp[1];
	xfmat[3][2] = vw1->vp[2];
	setident4(m4t);
	m4t[0][0] = vw2->vhinc[0]/vw2->vhn2;
	m4t[1][0] = vw2->vhinc[1]/vw2->vhn2;
	m4t[2][0] = vw2->vhinc[2]/vw2->vhn2;
	m4t[3][0] = -DOT(vw2->vp,vw2->vhinc)/vw2->vhn2;
	m4t[0][1] = vw2->vvinc[0]/vw2->vvn2;
	m4t[1][1] = vw2->vvinc[1]/vw2->vvn2;
	m4t[2][1] = vw2->vvinc[2]/vw2->vvn2;
	m4t[3][1] = -DOT(vw2->vp,vw2->vvinc)/vw2->vvn2;
	m4t[0][2] = vw2->vdir[0];
	m4t[1][2] = vw2->vdir[1];
	m4t[2][2] = vw2->vdir[2];
	m4t[3][2] = -DOT(vw2->vp,vw2->vdir);
	multmat4(xfmat, xfmat, m4t);
}


addscanline(y, pline, zline)		/* add scanline to output */
int	y;
COLR	*pline;
float	*zline;
{
	extern double	sqrt();
	double	pos[3];
	register int	x;
	register int	xpos, ypos;

	for (x = 0; x < theirview.hresolu; x++) {
		pos[0] = x - .5*(theirview.hresolu-1);
		pos[1] = y - .5*(theirview.vresolu-1);
		pos[2] = zline[x];
		if (theirview.type == VT_PER) {
			pos[2] /= sqrt( 1.
					+ pos[0]*pos[0]*theirview.vhn2
					+ pos[1]*pos[1]*theirview.vvn2 );
			pos[0] *= pos[2];
			pos[1] *= pos[2];
		}
		multp3(pos, pos, theirs2ours);
		if (pos[2] <= 0.0)
			continue;
		if (ourview.type == VT_PER) {
			pos[0] /= pos[2];
			pos[1] /= pos[2];
		}
		pos[0] += .5*ourview.hresolu;
		pos[1] += .5*ourview.vresolu;
		if (pos[0] < 0 || pos[0] > ourview.hresolu
				|| pos[1] < 0 || pos[1] > ourview.vresolu)
			continue;
					/* check current value at pos */
		xpos = pos[0];
		ypos = pos[1];
		if (zscan(ypos)[xpos] <= 0.0
				|| zscan(ypos)[xpos] - pos[2]
					> zeps*zscan(ypos)[xpos]) {
			zscan(ypos)[xpos] = pos[2];
			copycolr(pscan(ypos)[xpos], pline[x]);
		}
	}
}


fillpicture()				/* fill in empty spaces */
{
	int	*yback, xback;
	int	y;
	COLR	pfill;
	register int	x, i;
							/* get back buffer */
	yback = (int *)malloc(ourview.hresolu*sizeof(int));
	if (yback == NULL) {
		perror(progname);
		return;
	}
	for (x = 0; x < ourview.hresolu; x++)
		yback[x] = -2;
							/* fill image */
	for (y = 0; y < ourview.vresolu; y++) {
		xback = -2;
		for (x = 0; x < ourview.hresolu; x++)
			if (zscan(y)[x] <= 0.0) {	/* empty pixel */
				if (yback[x] == -2) {
					for (i = y+1; i < ourview.vresolu; i++)
						if (zscan(i)[x] > 0.0)
							break;
					if (i < ourview.vresolu
				&& (y <= 0 || zscan(y-1)[x] < zscan(i)[x]))
						yback[x] = i;
					else
						yback[x] = y-1;
				}
				if (xback == -2) {
					for (i = x+1; x < ourview.hresolu; i++)
						if (zscan(y)[i] > 0.0)
							break;
					if (i < ourview.hresolu
				&& (x <= 0 || zscan(y)[x-1] < zscan(y)[i]))
						xback = i;
					else
						xback = x-1;
				}
				if (xback < 0 && yback[x] < 0)
					continue;
				if (yback[x] < 0 || ABS(x-xback) <= 1
					|| ( ABS(y-yback[x]) > 1
				&& zscan(yback[x])[x] < zscan(y)[xback] ))
					copycolr(pscan(y)[x],pscan(y)[xback]);
				else
					copycolr(pscan(y)[x],pscan(yback[x])[x]);
			} else {				/* full pixel */
				yback[x] = -2;
				xback = -2;
			}
	}
	free((char *)yback);
}


writepicture()				/* write out picture */
{
	int	y;

	fputresolu(YMAJOR|YDECR, ourview.hresolu, ourview.vresolu, stdout);
	for (y = ourview.vresolu-1; y >= 0; y--)
		if (fwritecolrs(pscan(y), ourview.hresolu, stdout) < 0) {
			perror(progname);
			exit(1);
		}
}
