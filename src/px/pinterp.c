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

VIEW	ourview = STDVIEW(512);		/* desired view */

double	zeps = 0.001;			/* allowed z epsilon */

COLR	*ourpict;			/* output picture */
float	*ourzbuf;			/* corresponding z-buffer */

char	*progname;

VIEW	theirview = STDVIEW(512);	/* input view */
int	gotview;			/* got input view? */


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


addscanline(y, pline, zline)		/* add scanline to output */
int	y;
COLR	*pline;
float	*zline;
{
	FVECT	p, dir;
	double	xnew, ynew, znew;
	register int	x;
	register int	xpos, ypos;

	for (x = 0; x < theirview.hresolu; x++) {
		rayview(p, dir, &theirview, x+.5, y+.5);
		p[0] += zline[x]*dir[0];
		p[1] += zline[x]*dir[1];
		p[2] += zline[x]*dir[2];
		pixelview(&xnew, &ynew, &znew, &ourview, p);
		if (znew <= 0.0 || xnew < 0 || xnew > ourview.hresolu
				|| ynew < 0 || ynew > ourview.vresolu)
			continue;
					/* check current value at position */
		xpos = xnew;
		ypos = ynew;
		if (zscan(ypos)[xpos] <= 0.0
				|| zscan(ypos)[xpos] - znew
					> zeps*zscan(ypos)[xpos]) {
			zscan(ypos)[xpos] = znew;
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
		yback[x] = -1;
							/* fill image */
	for (y = 0; y < ourview.vresolu; y++)
		for (x = 0; x < ourview.hresolu; x++)
			if (zscan(y)[x] <= 0.0) {	/* found hole */
				xback = x-1;
				do {			/* find boundary */
					if (yback[x] < 0) {
						for (i = y+1;
							i < ourview.vresolu;
								i++)
							if (zscan(i)[x] > 0.0)
								break;
						if (i < ourview.vresolu
				&& (y <= 0 || zscan(y-1)[x] < zscan(i)[x]))
							yback[x] = i;
						else
							yback[x] = y-1;
					}
				} while (++x < ourview.hresolu
						&& zscan(y)[x] <= 0.0);
				i = xback;		/* pick background */
				if (x < ourview.hresolu
			&& (i < 0 || zscan(y)[i] < zscan(y)[x]))
					xback = x;
							/* fill hole */
				if (xback < 0) {
					while (++i < x)
						if (yback[i] >= 0)
				copycolr(pscan(y)[i],pscan(yback[i])[i]);
				} else {
					while (++i < x)
						if (yback[i] < 0 ||
				zscan(yback[i])[i] < zscan(y)[xback])
					copycolr(pscan(y)[i],pscan(y)[xback]);
						else
					copycolr(pscan(y)[i],pscan(yback[i])[i]);
				}
		} else
			yback[x] = -1;			/* clear boundary */
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
