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

#define F_FORE		1		/* fill foreground */
#define F_BACK		2		/* fill background */

#define PACKSIZ		42		/* calculation packet size */

#define RTCOM		"rtrace -h -ovl -fff -x %d %s"

#define ABS(x)		((x)>0?(x):-(x))

struct position {int x,y; float z;};

VIEW	ourview = STDVIEW(512);		/* desired view */

double	zeps = .02;			/* allowed z epsilon */

COLR	*ourpict;			/* output picture */
float	*ourzbuf;			/* corresponding z-buffer */

char	*progname;

int	fill = F_FORE|F_BACK;		/* selected fill algorithm */
extern int	backfill(), rcalfill();	/* fill functions */
int	(*deffill)() = backfill;	/* selected fill function */
COLR	backcolr = BLKCOLR;		/* background color */
double	backz = 0.0;			/* background z value */
int	normdist = 1;			/* normalized distance? */
double	ourexp = -1;			/* output picture exposure */

VIEW	theirview = STDVIEW(512);	/* input view */
int	gotview;			/* got input view? */
double	theirs2ours[4][4];		/* transformation matrix */
double	theirexp;			/* input picture exposure */

int	childpid = -1;			/* id of fill process */
FILE	*psend, *precv;			/* pipes to/from fill calculation */
int	queue[PACKSIZ][2];		/* pending pixels */
int	queuesiz;			/* number of pixels pending */


main(argc, argv)			/* interpolate pictures */
int	argc;
char	*argv[];
{
#define check(olen,narg)	if (argv[i][olen] || narg >= argc-i) goto badopt
	extern double	atof();
	int	gotvfile = 0;
	char	*zfile = NULL;
	char	*err;
	int	i;

	progname = argv[0];

	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 't':				/* threshold */
			check(2,1);
			zeps = atof(argv[++i]);
			break;
		case 'n':				/* dist. normalized? */
			check(2,0);
			normdist = !normdist;
			break;
		case 'f':				/* fill type */
			switch (argv[i][2]) {
			case '0':				/* none */
				check(3,0);
				fill = 0;
				break;
			case 'f':				/* foreground */
				check(3,0);
				fill = F_FORE;
				break;
			case 'b':				/* background */
				check(3,0);
				fill = F_BACK;
				break;
			case 'a':				/* all */
				check(3,0);
				fill = F_FORE|F_BACK;
				break;
			case 'c':				/* color */
				check(3,3);
				deffill = backfill;
				setcolr(backcolr, atof(argv[i+1]),
					atof(argv[i+2]), atof(argv[i+3]));
				i += 3;
				break;
			case 'z':				/* z value */
				check(3,1);
				deffill = backfill;
				backz = atof(argv[++i]);
				break;
			case 'r':				/* rtrace */
				check(3,1);
				deffill = rcalfill;
				calstart(RTCOM, argv[++i]);
				break;
			default:
				goto badopt;
			}
			break;
		case 'z':				/* z file */
			check(2,1);
			zfile = argv[++i];
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
			fprintf(stderr, "%s: command line error at '%s'\n",
					progname, argv[i]);
			goto userr;
		}
						/* check arguments */
	if ((argc-i)%2)
		goto userr;
						/* set view */
	if (err = setview(&ourview)) {
		fprintf(stderr, "%s: %s\n", progname, err);
		exit(1);
	}
						/* allocate frame */
	ourpict = (COLR *)malloc(ourview.hresolu*ourview.vresolu*sizeof(COLR));
	ourzbuf = (float *)calloc(ourview.hresolu*ourview.vresolu,sizeof(float));
	if (ourpict == NULL || ourzbuf == NULL)
		syserror();
							/* get input */
	for ( ; i < argc; i += 2)
		addpicture(argv[i], argv[i+1]);
							/* fill in spaces */
	if (fill&F_BACK)
		backpicture();
	else
		fillpicture();
							/* close calculation */
	caldone();
							/* add to header */
	printargs(argc, argv, stdout);
	if (gotvfile) {
		printf(VIEWSTR);
		fprintview(&ourview, stdout);
		printf("\n");
	}
	if (ourexp > 0 && ourexp != 1.0)
		fputexpos(ourexp, stdout);
	printf("\n");
							/* write picture */
	writepicture();
							/* write z file */
	if (zfile != NULL)
		writedistance(zfile);

	exit(0);
userr:
	fprintf(stderr,
	"Usage: %s [view opts][-t zthresh][-z zout][-fT][-n] pfile zspec ..\n",
			progname);
	exit(1);
#undef check
}


headline(s)				/* process header string */
char	*s;
{
	static char	*altname[] = {"rview","rpict","pinterp",VIEWSTR,NULL};
	register char	**an;

	printf("\t%s", s);

	if (isexpos(s)) {
		theirexp *= exposval(s);
		return;
	}
	for (an = altname; *an != NULL; an++)
		if (!strncmp(*an, s, strlen(*an))) {
			if (sscanview(&theirview, s+strlen(*an)) == 0)
				gotview++;
			break;
		}
}


addpicture(pfile, zspec)		/* add picture to output */
char	*pfile, *zspec;
{
	extern double	atof();
	FILE	*pfp, *zfp;
	char	*err;
	COLR	*scanin;
	float	*zin;
	struct position	*plast;
	int	y;
					/* open picture file */
	if ((pfp = fopen(pfile, "r")) == NULL) {
		perror(pfile);
		exit(1);
	}
					/* get header with exposure and view */
	theirexp = 1.0;
	gotview = 0;
	printf("%s:\n", pfile);
	getheader(pfp, headline);
	if (!gotview || fgetresolu(&theirview.hresolu, &theirview.vresolu, pfp)
			!= (YMAJOR|YDECR)) {
		fprintf(stderr, "%s: picture view error\n", pfile);
		exit(1);
	}
	if (ourexp <= 0)
		ourexp = theirexp;
	else if (ABS(theirexp-ourexp) > .01*ourexp)
		fprintf(stderr, "%s: different exposure (warning)\n", pfile);
	if (err = setview(&theirview)) {
		fprintf(stderr, "%s: %s\n", pfile, err);
		exit(1);
	}
					/* compute transformation */
	pixform(theirs2ours, &theirview, &ourview);
					/* allocate scanlines */
	scanin = (COLR *)malloc(theirview.hresolu*sizeof(COLR));
	zin = (float *)malloc(theirview.hresolu*sizeof(float));
	plast = (struct position *)calloc(theirview.hresolu,
						sizeof(struct position));
	if (scanin == NULL || zin == NULL || plast == NULL)
		syserror();
					/* get z specification or file */
	if ((zfp = fopen(zspec, "r")) == NULL) {
		double	zvalue;
		register int	x;
		if (!isfloat(zspec) || (zvalue = atof(zspec)) <= 0.0) {
			perror(zspec);
			exit(1);
		}
		for (x = 0; x < theirview.hresolu; x++)
			zin[x] = zvalue;
	}
					/* load image */
	for (y = theirview.vresolu-1; y >= 0; y--) {
		if (freadcolrs(scanin, theirview.hresolu, pfp) < 0) {
			fprintf(stderr, "%s: read error\n", pfile);
			exit(1);
		}
		if (zfp != NULL
			&& fread(zin,sizeof(float),theirview.hresolu,zfp)
				< theirview.hresolu) {
			fprintf(stderr, "%s: read error\n", zspec);
			exit(1);
		}
		addscanline(y, scanin, zin, plast);
	}
					/* clean up */
	free((char *)scanin);
	free((char *)zin);
	free((char *)plast);
	fclose(pfp);
	if (zfp != NULL)
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


addscanline(y, pline, zline, lasty)	/* add scanline to output */
int	y;
COLR	*pline;
float	*zline;
struct position	*lasty;		/* input/output */
{
	extern double	sqrt();
	double	pos[3];
	struct position	lastx, newpos;
	register int	x;

	for (x = theirview.hresolu-1; x >= 0; x--) {
		pos[0] = x - .5*(theirview.hresolu-1);
		pos[1] = y - .5*(theirview.vresolu-1);
		pos[2] = zline[x];
		if (theirview.type == VT_PER) {
			if (normdist)	/* adjust for eye-ray distance */
				pos[2] /= sqrt( 1.
					+ pos[0]*pos[0]*theirview.vhn2
					+ pos[1]*pos[1]*theirview.vvn2 );
			pos[0] *= pos[2];
			pos[1] *= pos[2];
		}
		multp3(pos, pos, theirs2ours);
		if (pos[2] <= 0) {
			lasty[x].z = lastx.z = 0;	/* mark invalid */
			continue;
		}
		if (ourview.type == VT_PER) {
			pos[0] /= pos[2];
			pos[1] /= pos[2];
		}
		pos[0] += .5*ourview.hresolu;
		pos[1] += .5*ourview.vresolu;
		newpos.x = pos[0];
		newpos.y = pos[1];
		newpos.z = zline[x];
					/* add pixel to our image */
		if (pos[0] >= 0 && newpos.x < ourview.hresolu
				&& pos[1] >= 0 && newpos.y < ourview.vresolu) {
			addpixel(&newpos, &lastx, &lasty[x], pline[x], pos[2]);
			lasty[x].x = lastx.x = newpos.x;
			lasty[x].y = lastx.y = newpos.y;
			lasty[x].z = lastx.z = newpos.z;
		} else
			lasty[x].z = lastx.z = 0;	/* mark invalid */
	}
}


addpixel(p0, p1, p2, pix, z)		/* fill in pixel parallelogram */
struct position	*p0, *p1, *p2;
COLR	pix;
double	z;
{
	double	zt = 2.*zeps*p0->z;		/* threshold */
	int	s1x, s1y, s2x, s2y;		/* step sizes */
	int	l1, l2, c1, c2;			/* side lengths and counters */
	int	p1isy;				/* p0p1 along y? */
	int	x1, y1;				/* p1 position */
	register int	x, y;			/* final position */

					/* compute vector p0p1 */
	if (fill&F_FORE && ABS(p1->z-p0->z) <= zt) {
		s1x = p1->x - p0->x;
		s1y = p1->y - p0->y;
		l1 = ABS(s1x);
		if (p1isy = (ABS(s1y) > l1))
			l1 = ABS(s1y);
	} else {
		l1 = s1x = s1y = 1;
		p1isy = -1;
	}
					/* compute vector p0p2 */
	if (fill&F_FORE && ABS(p2->z-p0->z) <= zt) {
		s2x = p2->x - p0->x;
		s2y = p2->y - p0->y;
		if (p1isy == 1)
			l2 = ABS(s2x);
		else {
			l2 = ABS(s2y);
			if (p1isy != 0 && ABS(s2x) > l2)
				l2 = ABS(s2x);
		}
	} else
		l2 = s2x = s2y = 1;
					/* fill the parallelogram */
	for (c1 = l1; c1-- > 0; ) {
		x1 = p0->x + c1*s1x/l1;
		y1 = p0->y + c1*s1y/l1;
		for (c2 = l2; c2-- > 0; ) {
			x = x1 + c2*s2x/l2;
			y = y1 + c2*s2y/l2;
			if (zscan(y)[x] <= 0 || zscan(y)[x]-z
						> zeps*zscan(y)[x]) {
				zscan(y)[x] = z;
				copycolr(pscan(y)[x], pix);
			}
		}
	}
}


backpicture()				/* background fill algorithm */
{
	int	*yback, xback;
	int	y;
	COLR	pfill;
	register int	x, i;
							/* get back buffer */
	yback = (int *)malloc(ourview.hresolu*sizeof(int));
	if (yback == NULL)
		syserror();
	for (x = 0; x < ourview.hresolu; x++)
		yback[x] = -2;
	/*
	 * Xback and yback are the pixel locations of suitable
	 * background values in each direction.
	 * A value of -2 means unassigned, and -1 means
	 * that there is no suitable background in this direction.
	 */
							/* fill image */
	for (y = 0; y < ourview.vresolu; y++) {
		xback = -2;
		for (x = 0; x < ourview.hresolu; x++)
			if (zscan(y)[x] <= 0) {		/* empty pixel */
				/*
				 * First, find background from above or below.
				 * (farthest assigned pixel)
				 */
				if (yback[x] == -2) {
					for (i = y+1; i < ourview.vresolu; i++)
						if (zscan(i)[x] > 0)
							break;
					if (i < ourview.vresolu
				&& (y <= 0 || zscan(y-1)[x] < zscan(i)[x]))
						yback[x] = i;
					else
						yback[x] = y-1;
				}
				/*
				 * Next, find background from left or right.
				 */
				if (xback == -2) {
					for (i = x+1; i < ourview.hresolu; i++)
						if (zscan(y)[i] > 0)
							break;
					if (i < ourview.hresolu
				&& (x <= 0 || zscan(y)[x-1] < zscan(y)[i]))
						xback = i;
					else
						xback = x-1;
				}
				/*
				 * Check to see if we have no background for
				 * this pixel.  If not, use background color.
				 */
				if (xback < 0 && yback[x] < 0) {
					(*deffill)(x,y);
					continue;
				}
				/*
				 * Compare, and use the background that is
				 * farther, unless one of them is next to us.
				 */
				if ( yback[x] < 0
					|| (xback >= 0 && ABS(x-xback) <= 1)
					|| ( ABS(y-yback[x]) > 1
						&& zscan(yback[x])[x]
						< zscan(y)[xback] ) ) {
					copycolr(pscan(y)[x],pscan(y)[xback]);
					zscan(y)[x] = zscan(y)[xback];
				} else {
					copycolr(pscan(y)[x],pscan(yback[x])[x]);
					zscan(y)[x] = zscan(yback[x])[x];
				}
			} else {				/* full pixel */
				yback[x] = -2;
				xback = -2;
			}
	}
	free((char *)yback);
}


fillpicture()				/* paint in empty pixels with default */
{
	register int	x, y;

	for (y = 0; y < ourview.vresolu; y++)
		for (x = 0; x < ourview.hresolu; x++)
			if (zscan(y)[x] <= 0)
				(*deffill)(x,y);
}


writepicture()				/* write out picture */
{
	int	y;

	fputresolu(YMAJOR|YDECR, ourview.hresolu, ourview.vresolu, stdout);
	for (y = ourview.vresolu-1; y >= 0; y--)
		if (fwritecolrs(pscan(y), ourview.hresolu, stdout) < 0)
			syserror();
}


writedistance(fname)			/* write out z file */
char	*fname;
{
	extern double	sqrt();
	int	donorm = normdist && ourview.type == VT_PER;
	FILE	*fp;
	int	y;
	float	*zout;

	if ((fp = fopen(fname, "w")) == NULL) {
		perror(fname);
		exit(1);
	}
	if (donorm
	&& (zout = (float *)malloc(ourview.hresolu*sizeof(float))) == NULL)
		syserror();
	for (y = ourview.vresolu-1; y >= 0; y--) {
		if (donorm) {
			double	vx, yzn2;
			register int	x;
			yzn2 = y - .5*(ourview.vresolu-1);
			yzn2 = 1. + yzn2*yzn2*ourview.vvn2;
			for (x = 0; x < ourview.hresolu; x++) {
				vx = x - .5*(ourview.hresolu-1);
				zout[x] = zscan(y)[x]
					* sqrt(vx*vx*ourview.vhn2 + yzn2);
			}
		} else
			zout = zscan(y);
		if (fwrite(zout, sizeof(float), ourview.hresolu, fp)
				< ourview.hresolu) {
			perror(fname);
			exit(1);
		}
	}
	if (donorm)
		free((char *)zout);
	fclose(fp);
}


isfloat(s)				/* see if string is floating number */
register char	*s;
{
	for ( ; *s; s++)
		if ((*s < '0' || *s > '9') && *s != '.' && *s != '-'
				&& *s != 'e' && *s != 'E' && *s != '+')
			return(0);
	return(1);
}


backfill(x, y)				/* fill pixel with background */
int	x, y;
{
	register BYTE	*dest = pscan(y)[x];

	copycolr(dest, backcolr);
	zscan(y)[x] = backz;
}


calstart(prog, args)			/* start fill calculation */
char	*prog, *args;
{
	char	combuf[512];
	int	p0[2], p1[2];

	if (childpid != -1) {
		fprintf(stderr, "%s: too many calculations\n", progname);
		exit(1);
	}
	sprintf(combuf, prog, PACKSIZ, args);
	if (pipe(p0) < 0 || pipe(p1) < 0)
		syserror();
	if ((childpid = vfork()) == 0) {	/* fork calculation */
		close(p0[1]);
		close(p1[0]);
		if (p0[0] != 0) {
			dup2(p0[0], 0);
			close(p0[0]);
		}
		if (p1[1] != 1) {
			dup2(p1[1], 1);
			close(p1[1]);
		}
		execl("/bin/sh", "sh", "-c", combuf, 0);
		perror("/bin/sh");
		_exit(127);
	}
	if (childpid == -1)
		syserror();
	close(p0[0]);
	close(p1[1]);
	if ((psend = fdopen(p0[1], "w")) == NULL)
		syserror();
	if ((precv = fdopen(p1[0], "r")) == NULL)
		syserror();
	queuesiz = 0;
}


caldone()				/* done with calculation */
{
	int	pid;

	if (childpid == -1)
		return;
	if (fclose(psend) == EOF)
		syserror();
	clearqueue();
	fclose(precv);
	while ((pid = wait(0)) != -1 && pid != childpid)
		;
	childpid = -1;
}


rcalfill(x, y)				/* fill with ray-calculated pixel */
int	x, y;
{
	FVECT	orig, dir;
	float	outbuf[6];

	if (queuesiz >= PACKSIZ) {	/* flush queue */
		if (fflush(psend) == EOF)
			syserror();
		clearqueue();
	}
					/* send new ray */
	rayview(orig, dir, &ourview, x+.5, y+.5);
	outbuf[0] = orig[0]; outbuf[1] = orig[1]; outbuf[2] = orig[2];
	outbuf[3] = dir[0]; outbuf[4] = dir[1]; outbuf[5] = dir[2];
	if (fwrite(outbuf, sizeof(float), 6, psend) < 6)
		syserror();
					/* remember it */
	queue[queuesiz][0] = x;
	queue[queuesiz][1] = y;
	queuesiz++;
}


clearqueue()				/* get results from queue */
{
	float	inbuf[4];
	register int	i;

	for (i = 0; i < queuesiz; i++) {
		if (fread(inbuf, sizeof(float), 4, precv) < 4) {
			fprintf(stderr, "%s: read error in clearqueue\n",
					progname);
			exit(1);
		}
		if (ourexp > 0 && ourexp != 1.0) {
			inbuf[0] *= ourexp;
			inbuf[1] *= ourexp;
			inbuf[2] *= ourexp;
		}
		setcolr(pscan(queue[i][1])[queue[i][0]],
				inbuf[0], inbuf[1], inbuf[2]);
		zscan(queue[i][1])[queue[i][0]] = inbuf[3];
	}
	queuesiz = 0;
}


syserror()			/* report error and exit */
{
	perror(progname);
	exit(1);
}
