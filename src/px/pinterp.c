/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Interpolate and extrapolate pictures with different view parameters.
 *
 *	Greg Ward	09Dec89
 */

#include "standard.h"

#include <fcntl.h>

#include "view.h"

#include "color.h"

#include "resolu.h"

#ifndef BSD
#define vfork		fork
#endif

#define pscan(y)	(ourpict+(y)*hresolu)
#define zscan(y)	(ourzbuf+(y)*hresolu)

#define F_FORE		1		/* fill foreground */
#define F_BACK		2		/* fill background */

#define PACKSIZ		42		/* calculation packet size */

#define RTCOM		"rtrace -h -ovl -fff %s"

#define ABS(x)		((x)>0?(x):-(x))

struct position {int x,y; float z;};

VIEW	ourview = STDVIEW;		/* desired view */
int	hresolu = 512;			/* horizontal resolution */
int	vresolu = 512;			/* vertical resolution */
double	pixaspect = 1.0;		/* pixel aspect ratio */

double	zeps = .02;			/* allowed z epsilon */

COLR	*ourpict;			/* output picture */
float	*ourzbuf;			/* corresponding z-buffer */

char	*progname;

int	fillo = F_FORE|F_BACK;		/* selected fill options */
int	fillsamp = 0;			/* sample separation (0 == inf) */
extern int	backfill(), rcalfill();	/* fill functions */
int	(*fillfunc)() = backfill;	/* selected fill function */
COLR	backcolr = BLKCOLR;		/* background color */
double	backz = 0.0;			/* background z value */
int	normdist = 1;			/* normalized distance? */
double	ourexp = -1;			/* output picture exposure */

VIEW	theirview = STDVIEW;		/* input view */
int	gotview;			/* got input view? */
int	wrongformat = 0;		/* input in another format? */
RESOLU	tresolu;			/* input resolution */
double	theirexp;			/* input picture exposure */
double	theirs2ours[4][4];		/* transformation matrix */
int	hasmatrix = 0;			/* has transformation matrix */

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
	int	i, rval;

	progname = argv[0];

	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		rval = getviewopt(&ourview, argc-i, argv+i);
		if (rval >= 0) {
			i += rval;
			continue;
		}
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
				fillo = 0;
				break;
			case 'f':				/* foreground */
				check(3,0);
				fillo = F_FORE;
				break;
			case 'b':				/* background */
				check(3,0);
				fillo = F_BACK;
				break;
			case 'a':				/* all */
				check(3,0);
				fillo = F_FORE|F_BACK;
				break;
			case 's':				/* sample */
				check(3,1);
				fillsamp = atoi(argv[++i]);
				break;
			case 'c':				/* color */
				check(3,3);
				fillfunc = backfill;
				setcolr(backcolr, atof(argv[i+1]),
					atof(argv[i+2]), atof(argv[i+3]));
				i += 3;
				break;
			case 'z':				/* z value */
				check(3,1);
				fillfunc = backfill;
				backz = atof(argv[++i]);
				break;
			case 'r':				/* rtrace */
				check(3,1);
				fillfunc = rcalfill;
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
			hresolu = atoi(argv[++i]);
			break;
		case 'y':				/* y resolution */
			check(2,1);
			vresolu = atoi(argv[++i]);
			break;
		case 'p':				/* pixel aspect */
			check(2,1);
			pixaspect = atof(argv[++i]);
			break;
		case 'v':				/* view file */
			if (argv[i][2] != 'f')
				goto badopt;
			check(3,1);
			gotvfile = viewfile(argv[++i], &ourview, 0, 0);
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
		badopt:
			fprintf(stderr, "%s: command line error at '%s'\n",
					progname, argv[i]);
			goto userr;
		}
	}
						/* check arguments */
	if ((argc-i)%2)
		goto userr;
						/* set view */
	if (err = setview(&ourview)) {
		fprintf(stderr, "%s: %s\n", progname, err);
		exit(1);
	}
	normaspect(viewaspect(&ourview), &pixaspect, &hresolu, &vresolu);
						/* allocate frame */
	ourpict = (COLR *)malloc(hresolu*vresolu*sizeof(COLR));
	ourzbuf = (float *)calloc(hresolu*vresolu,sizeof(float));
	if (ourpict == NULL || ourzbuf == NULL)
		syserror();
							/* get input */
	for ( ; i < argc; i += 2)
		addpicture(argv[i], argv[i+1]);
							/* fill in spaces */
	if (fillo&F_BACK)
		backpicture(fillfunc, fillsamp);
	else
		fillpicture(fillfunc);
							/* close calculation */
	caldone();
							/* add to header */
	printargs(argc, argv, stdout);
	if (gotvfile) {
		fputs(VIEWSTR, stdout);
		fprintview(&ourview, stdout);
		putc('\n', stdout);
	}
	if (pixaspect < .99 || pixaspect > 1.01)
		fputaspect(pixaspect, stdout);
	if (ourexp > 0 && (ourexp < .995 || ourexp > 1.005))
		fputexpos(ourexp, stdout);
	fputformat(COLRFMT, stdout);
	putc('\n', stdout);
							/* write picture */
	writepicture();
							/* write z file */
	if (zfile != NULL)
		writedistance(zfile);

	exit(0);
userr:
	fprintf(stderr,
	"Usage: %s [view opts][-t eps][-z zout][-fT][-n] pfile zspec ..\n",
			progname);
	exit(1);
#undef check
}


headline(s)				/* process header string */
char	*s;
{
	static char	*altname[] = {VIEWSTR,"rpict","rview","pinterp",NULL};
	register char	**an;
	char	fmt[32];

	if (isformat(s)) {
		formatval(fmt, s);
		wrongformat = strcmp(fmt, COLRFMT);
		return;
	}
	putc('\t', stdout);
	fputs(s, stdout);

	if (isexpos(s)) {
		theirexp *= exposval(s);
		return;
	}
	for (an = altname; *an != NULL; an++)
		if (!strncmp(*an, s, strlen(*an))) {
			if (sscanview(&theirview, s+strlen(*an)) > 0)
				gotview++;
			break;
		}
}


addpicture(pfile, zspec)		/* add picture to output */
char	*pfile, *zspec;
{
	extern double	atof();
	FILE	*pfp;
	int	zfd;
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
	getheader(pfp, headline, NULL);
	if (wrongformat || !gotview || !fgetsresolu(&tresolu, pfp)) {
		fprintf(stderr, "%s: picture format error\n", pfile);
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
	hasmatrix = pixform(theirs2ours, &theirview, &ourview);
					/* allocate scanlines */
	scanin = (COLR *)malloc(scanlen(&tresolu)*sizeof(COLR));
	zin = (float *)malloc(scanlen(&tresolu)*sizeof(float));
	plast = (struct position *)calloc(scanlen(&tresolu),
			sizeof(struct position));
	if (scanin == NULL || zin == NULL || plast == NULL)
		syserror();
					/* get z specification or file */
	if ((zfd = open(zspec, O_RDONLY)) == -1) {
		double	zvalue;
		register int	x;
		if (!isfloat(zspec) || (zvalue = atof(zspec)) <= 0.0) {
			perror(zspec);
			exit(1);
		}
		for (x = scanlen(&tresolu); x-- > 0; )
			zin[x] = zvalue;
	}
					/* load image */
	for (y = 0; y < numscans(&tresolu); y++) {
		if (freadcolrs(scanin, scanlen(&tresolu), pfp) < 0) {
			fprintf(stderr, "%s: read error\n", pfile);
			exit(1);
		}
		if (zfd != -1 && read(zfd,(char *)zin,
				scanlen(&tresolu)*sizeof(float))
				< scanlen(&tresolu)*sizeof(float)) {
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
	if (zfd != -1)
		close(zfd);
}


pixform(xfmat, vw1, vw2)		/* compute view1 to view2 matrix */
register double	xfmat[4][4];
register VIEW	*vw1, *vw2;
{
	double	m4t[4][4];

	if (vw1->type != VT_PER && vw1->type != VT_PAR)
		return(0);
	if (vw2->type != VT_PER && vw2->type != VT_PAR)
		return(0);
	setident4(xfmat);
	xfmat[0][0] = vw1->hvec[0];
	xfmat[0][1] = vw1->hvec[1];
	xfmat[0][2] = vw1->hvec[2];
	xfmat[1][0] = vw1->vvec[0];
	xfmat[1][1] = vw1->vvec[1];
	xfmat[1][2] = vw1->vvec[2];
	xfmat[2][0] = vw1->vdir[0];
	xfmat[2][1] = vw1->vdir[1];
	xfmat[2][2] = vw1->vdir[2];
	xfmat[3][0] = vw1->vp[0];
	xfmat[3][1] = vw1->vp[1];
	xfmat[3][2] = vw1->vp[2];
	setident4(m4t);
	m4t[0][0] = vw2->hvec[0]/vw2->hn2;
	m4t[1][0] = vw2->hvec[1]/vw2->hn2;
	m4t[2][0] = vw2->hvec[2]/vw2->hn2;
	m4t[3][0] = -DOT(vw2->vp,vw2->hvec)/vw2->hn2;
	m4t[0][1] = vw2->vvec[0]/vw2->vn2;
	m4t[1][1] = vw2->vvec[1]/vw2->vn2;
	m4t[2][1] = vw2->vvec[2]/vw2->vn2;
	m4t[3][1] = -DOT(vw2->vp,vw2->vvec)/vw2->vn2;
	m4t[0][2] = vw2->vdir[0];
	m4t[1][2] = vw2->vdir[1];
	m4t[2][2] = vw2->vdir[2];
	m4t[3][2] = -DOT(vw2->vp,vw2->vdir);
	multmat4(xfmat, xfmat, m4t);
	return(1);
}


addscanline(y, pline, zline, lasty)	/* add scanline to output */
int	y;
COLR	*pline;
float	*zline;
struct position	*lasty;		/* input/output */
{
	extern double	sqrt();
	FVECT	pos;
	struct position	lastx, newpos;
	register int	x;

	lastx.z = 0;
	for (x = scanlen(&tresolu); x-- > 0; ) {
		pix2loc(pos, &tresolu, x, y);
		pos[2] = zline[x];
		if (movepixel(pos) < 0) {
			lasty[x].z = lastx.z = 0;	/* mark invalid */
			continue;
		}
		newpos.x = pos[0] * hresolu;
		newpos.y = pos[1] * vresolu;
		newpos.z = zline[x];
					/* add pixel to our image */
		if (pos[0] >= 0 && newpos.x < hresolu
				&& pos[1] >= 0 && newpos.y < vresolu) {
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
	if (fillo&F_FORE && ABS(p1->z-p0->z) <= zt) {
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
	if (fillo&F_FORE && ABS(p2->z-p0->z) <= zt) {
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
			if (x < 0 || x >= hresolu)
				continue;
			y = y1 + c2*s2y/l2;
			if (y < 0 || y >= vresolu)
				continue;
			if (zscan(y)[x] <= 0 || zscan(y)[x]-z
						> zeps*zscan(y)[x]) {
				zscan(y)[x] = z;
				copycolr(pscan(y)[x], pix);
			}
		}
	}
}


movepixel(pos)				/* reposition image point */
FVECT	pos;
{
	FVECT	pt, direc;
	
	if (pos[2] <= 0)		/* empty pixel */
		return(-1);
	if (hasmatrix) {
		pos[0] += theirview.hoff - .5;
		pos[1] += theirview.voff - .5;
		if (theirview.type == VT_PER) {
			if (normdist)	/* adjust for eye-ray distance */
				pos[2] /= sqrt( 1.
					+ pos[0]*pos[0]*theirview.hn2
					+ pos[1]*pos[1]*theirview.vn2 );
			pos[0] *= pos[2];
			pos[1] *= pos[2];
		}
		multp3(pos, pos, theirs2ours);
		if (pos[2] <= 0)
			return(-1);
		if (ourview.type == VT_PER) {
			pos[0] /= pos[2];
			pos[1] /= pos[2];
		}
		pos[0] += .5 - ourview.hoff;
		pos[1] += .5 - ourview.voff;
		return(0);
	}
	if (viewray(pt, direc, &theirview, pos[0], pos[1]) < 0)
		return(-1);
	pt[0] += direc[0]*pos[2];
	pt[1] += direc[1]*pos[2];
	pt[2] += direc[2]*pos[2];
	viewloc(pos, &ourview, pt);
	if (pos[2] <= 0)
		return(-1);
	return(0);
}


backpicture(fill, samp)			/* background fill algorithm */
int	(*fill)();
int	samp;
{
	int	*yback, xback;
	int	y;
	register int	x, i;
							/* get back buffer */
	yback = (int *)malloc(hresolu*sizeof(int));
	if (yback == NULL)
		syserror();
	for (x = 0; x < hresolu; x++)
		yback[x] = -2;
	/*
	 * Xback and yback are the pixel locations of suitable
	 * background values in each direction.
	 * A value of -2 means unassigned, and -1 means
	 * that there is no suitable background in this direction.
	 */
							/* fill image */
	for (y = 0; y < vresolu; y++) {
		xback = -2;
		for (x = 0; x < hresolu; x++)
			if (zscan(y)[x] <= 0) {		/* empty pixel */
				/*
				 * First, find background from above or below.
				 * (farthest assigned pixel)
				 */
				if (yback[x] == -2) {
					for (i = y+1; i < vresolu; i++)
						if (zscan(i)[x] > 0)
							break;
					if (i < vresolu
				&& (y <= 0 || zscan(y-1)[x] < zscan(i)[x]))
						yback[x] = i;
					else
						yback[x] = y-1;
				}
				/*
				 * Next, find background from left or right.
				 */
				if (xback == -2) {
					for (i = x+1; i < hresolu; i++)
						if (zscan(y)[i] > 0)
							break;
					if (i < hresolu
				&& (x <= 0 || zscan(y)[x-1] < zscan(y)[i]))
						xback = i;
					else
						xback = x-1;
				}
				/*
				 * If we have no background for this pixel,
				 * use the given fill function.
				 */
				if (xback < 0 && yback[x] < 0)
					goto fillit;
				/*
				 * Compare, and use the background that is
				 * farther, unless one of them is next to us.
				 * If the background is too distant, call
				 * the fill function.
				 */
				if ( yback[x] < 0
					|| (xback >= 0 && ABS(x-xback) <= 1)
					|| ( ABS(y-yback[x]) > 1
						&& zscan(yback[x])[x]
						< zscan(y)[xback] ) ) {
					if (samp > 0 && ABS(x-xback) >= samp)
						goto fillit;
					copycolr(pscan(y)[x],pscan(y)[xback]);
					zscan(y)[x] = zscan(y)[xback];
				} else {
					if (samp > 0 && ABS(y-yback[x]) > samp)
						goto fillit;
					copycolr(pscan(y)[x],pscan(yback[x])[x]);
					zscan(y)[x] = zscan(yback[x])[x];
				}
				continue;
			fillit:
				(*fill)(x,y);
				if (fill == rcalfill) {		/* use it */
					clearqueue();
					xback = x;
					yback[x] = y;
				}
			} else {				/* full pixel */
				yback[x] = -2;
				xback = -2;
			}
	}
	free((char *)yback);
}


fillpicture(fill)		/* paint in empty pixels using fill */
int	(*fill)();
{
	register int	x, y;

	for (y = 0; y < vresolu; y++)
		for (x = 0; x < hresolu; x++)
			if (zscan(y)[x] <= 0)
				(*fill)(x,y);
}


writepicture()				/* write out picture */
{
	int	y;

	fprtresolu(hresolu, vresolu, stdout);
	for (y = vresolu-1; y >= 0; y--)
		if (fwritecolrs(pscan(y), hresolu, stdout) < 0)
			syserror();
}


writedistance(fname)			/* write out z file */
char	*fname;
{
	extern double	sqrt();
	int	donorm = normdist && ourview.type == VT_PER;
	int	fd;
	int	y;
	float	*zout;

	if ((fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0666)) == -1) {
		perror(fname);
		exit(1);
	}
	if (donorm
	&& (zout = (float *)malloc(hresolu*sizeof(float))) == NULL)
		syserror();
	for (y = vresolu-1; y >= 0; y--) {
		if (donorm) {
			double	vx, yzn2;
			register int	x;
			yzn2 = (y+.5)/vresolu + ourview.voff - .5;
			yzn2 = 1. + yzn2*yzn2*ourview.vn2;
			for (x = 0; x < hresolu; x++) {
				vx = (x+.5)/hresolu + ourview.hoff - .5;
				zout[x] = zscan(y)[x]
					* sqrt(vx*vx*ourview.hn2 + yzn2);
			}
		} else
			zout = zscan(y);
		if (write(fd, (char *)zout, hresolu*sizeof(float))
				< hresolu*sizeof(float)) {
			perror(fname);
			exit(1);
		}
	}
	if (donorm)
		free((char *)zout);
	close(fd);
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
	sprintf(combuf, prog, args);
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
	clearqueue();
	fclose(psend);
	fclose(precv);
	while ((pid = wait(0)) != -1 && pid != childpid)
		;
	childpid = -1;
}


rcalfill(x, y)				/* fill with ray-calculated pixel */
int	x, y;
{
	if (queuesiz >= PACKSIZ)	/* flush queue if needed */
		clearqueue();
					/* add position to queue */
	queue[queuesiz][0] = x;
	queue[queuesiz][1] = y;
	queuesiz++;
}


clearqueue()				/* process queue */
{
	FVECT	orig, dir;
	float	fbuf[6];
	register int	i;

	for (i = 0; i < queuesiz; i++) {
		viewray(orig, dir, &ourview,
				(queue[i][0]+.5)/hresolu,
				(queue[i][1]+.5)/vresolu);
		fbuf[0] = orig[0]; fbuf[1] = orig[1]; fbuf[2] = orig[2];
		fbuf[3] = dir[0]; fbuf[4] = dir[1]; fbuf[5] = dir[2];
		fwrite((char *)fbuf, sizeof(float), 6, psend);
	}
					/* flush output and get results */
	fbuf[3] = fbuf[4] = fbuf[5] = 0.0;		/* mark */
	fwrite((char *)fbuf, sizeof(float), 6, psend);
	if (fflush(psend) == EOF)
		syserror();
	for (i = 0; i < queuesiz; i++) {
		if (fread((char *)fbuf, sizeof(float), 4, precv) < 4) {
			fprintf(stderr, "%s: read error in clearqueue\n",
					progname);
			exit(1);
		}
		if (ourexp > 0 && ourexp != 1.0) {
			fbuf[0] *= ourexp;
			fbuf[1] *= ourexp;
			fbuf[2] *= ourexp;
		}
		setcolr(pscan(queue[i][1])[queue[i][0]],
				fbuf[0], fbuf[1], fbuf[2]);
		zscan(queue[i][1])[queue[i][0]] = fbuf[3];
	}
	queuesiz = 0;
}


syserror()			/* report error and exit */
{
	perror(progname);
	exit(1);
}
