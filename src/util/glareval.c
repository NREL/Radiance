/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Compute pixels for glare calculation
 */

#include "glare.h"
#include <sys/param.h>
					/* compute rtrace buffer size */
#ifndef PIPE_BUF
#define PIPE_BUF	512		/* hyperconservative */
#endif
#define MAXPIX		(PIPE_BUF/(6*sizeof(float)) - 1)

#ifndef BSD
#define vfork		fork
#endif

#define NSCANS		64		/* number of scanlines to buffer */

int	rt_pid = -1;		/* process id for rtrace */
int	fd_tort, fd_fromrt;	/* pipe descriptors */

FILE	*pictfp = NULL;		/* picture file pointer */
double	exposure;		/* picture exposure */
int	pxsiz, pysiz;		/* picture dimensions */
int	curpos;			/* current scanline */
long	*scanpos;		/* scanline positions */
struct {
	long	lused;		/* for LRU replacement */
	int	y;		/* scanline position */
	COLR	*sl;		/* scanline contents */
} scan[NSCANS];		/* buffered scanlines */

static long	ncall = 0L;	/* number of calls to getpictscan */
static long	nread = 0L;	/* number of scanlines read */

extern long	ftell();


COLR *
getpictscan(y)			/* get picture scanline */
int	y;
{
	int	minused;
	register int	i;
					/* first check our buffers */
	ncall++;
	minused = 0;
	for (i = 0; i < NSCANS; i++) {
		if (scan[i].y == y) {
			scan[i].lused = ncall;
			return(scan[i].sl);
		}
		if (scan[i].lused < scan[minused].lused)
			minused = i;
	}
					/* not there, read it in */
	if (scanpos[y] < 0) {			/* need to search */
		for (i = y+1; i < curpos; i++)
			if (scanpos[i] >= 0) {
				if (fseek(pictfp, scanpos[i], 0) < 0)
					goto seekerr;
				curpos = i;
				break;
			}
		while (curpos >= y) {
			scanpos[curpos] = ftell(pictfp);
			if (freadcolrs(scan[minused].sl, pxsiz, pictfp) < 0)
				goto readerr;
			nread++;
			curpos--;
		}
	} else {
		if (curpos != y && fseek(pictfp, scanpos[y], 0) < 0)
			goto seekerr;
		if (freadcolrs(scan[minused].sl, pxsiz, pictfp) < 0)
			goto readerr;
		nread++;
		curpos = y-1;
	}
	scan[minused].lused = ncall;
	scan[minused].y = y;
	return(scan[minused].sl);
readerr:
	fprintf(stderr, "%s: picture read error\n", progname);
	exit(1);
seekerr:
	fprintf(stderr, "%s: picture seek error\n", progname);
	exit(1);
}


pict_stats()			/* print out picture read statistics */
{
	static long	lastcall = 0L;	/* ncall at last report */
	static long	lastread = 0L;	/* nread at last report */

	if (ncall == lastcall)
		return;
	fprintf(stderr, "%s: %ld scanlines read, %ld reused\n",
			progname, nread-lastread,
			(ncall-lastcall)-(nread-lastread));
	lastcall = ncall;
	lastread = nread;
}


double
pict_val(vd)			/* find picture value for view direction */
FVECT	vd;
{
	FVECT	pp;
	double	vpx, vpy, vpz;
	COLOR	res;

	if (pictfp == NULL)
		return(-1.0);
	pp[0] = pictview.vp[0] + vd[0];
	pp[1] = pictview.vp[1] + vd[1];
	pp[2] = pictview.vp[2] + vd[2];
	viewpixel(&vpx, &vpy, &vpz, &pictview, pp);
	if (vpz <= FTINY || vpx < 0. || vpx >= 1. || vpy < 0. || vpy >= 1.)
		return(-1.0);
	colr_color(res, getpictscan((int)(vpy*pysiz))[(int)(vpx*pxsiz)]);
	return(luminance(res)/exposure);
}


double
getviewpix(vh, vv)		/* compute single view pixel */
int	vh, vv;
{
	FVECT	dir;
	float	rt_buf[6];
	double	res;

	if (compdir(dir, vh, vv) < 0)
		return(-1.0);
	if ((res = pict_val(dir)) >= 0.0)
		return(res);
	if (rt_pid == -1)
		return(-1.0);
	rt_buf[0] = ourview.vp[0];
	rt_buf[1] = ourview.vp[1];
	rt_buf[2] = ourview.vp[2];
	rt_buf[3] = dir[0];
	rt_buf[4] = dir[1];
	rt_buf[5] = dir[2];
	rt_compute(rt_buf, 1);
	return(luminance(rt_buf));
}


getviewspan(vv, vb)		/* compute a span of view pixels */
int	vv;
float	*vb;
{
	float	rt_buf[6*MAXPIX];	/* rtrace send/receive buffer */
	register int	n;		/* number of pixels in buffer */
	short	buf_vh[MAXPIX];		/* pixel positions */
	FVECT	dir;
	register int	vh;

#ifdef DEBUG
	if (verbose)
		fprintf(stderr, "%s: computing view span at %d...\n",
				progname, vv);
#endif
	n = 0;
	for (vh = -hsize; vh <= hsize; vh++) {
		if (compdir(dir, vh, vv) < 0) {	/* off viewable region */
			vb[vh+hsize] = -1.0;
			continue;
		}
		if ((vb[vh+hsize] = pict_val(dir)) >= 0.0)
			continue;
		if (rt_pid == -1)		/* missing information */
			continue;
						/* send to rtrace */
		if (n >= MAXPIX) {			/* flush */
			rt_compute(rt_buf, n);
			while (n-- > 0)
				vb[buf_vh[n]+hsize] = luminance(rt_buf+3*n);
		}
		rt_buf[6*n] = ourview.vp[0];
		rt_buf[6*n+1] = ourview.vp[1];
		rt_buf[6*n+2] = ourview.vp[2];
		rt_buf[6*n+3] = dir[0];
		rt_buf[6*n+4] = dir[1];
		rt_buf[6*n+5] = dir[2];
		buf_vh[n++] = vh;
	}
#ifdef DEBUG
	if (verbose)
		pict_stats();
#endif
	if (n > 0) {				/* process pending buffer */
		rt_compute(rt_buf, n);
		while (n-- > 0)
			vb[buf_vh[n]+hsize] = luminance(rt_buf+3*n);
	}
}


rt_compute(pb, np)		/* process buffer through rtrace */
float	*pb;
int	np;
{
	static float	nbuf[6] = {0.,0.,0.,0.,0.,0.};

#ifdef DEBUG
	if (verbose && np > 1)
		fprintf(stderr, "%s: sending %d samples to rtrace...\n",
				progname, np);
#endif
	if (writebuf(fd_tort,(char *)pb,6*sizeof(float)*np) < 6*sizeof(float)*np
		|| writebuf(fd_tort,(char *)nbuf,sizeof(nbuf)) < sizeof(nbuf)) {
		fprintf(stderr, "%s: error writing to rtrace process\n",
				progname);
		exit(1);
	}
	if (readbuf(fd_fromrt, (char *)pb, 3*sizeof(float)*np)
			< 3*sizeof(float)*np) {
		fprintf(stderr, "%s: error reading from rtrace process\n",
				progname);
		exit(1);
	}
}


getexpos(s)			/* get exposure from header line */
char	*s;
{
	if (isexpos(s))
		exposure *= exposval(s);
}


open_pict(fn)			/* open picture file */
char	*fn;
{
	register int	i;

	if ((pictfp = fopen(fn, "r")) == NULL) {
		fprintf("%s: cannot open\n", fn);
		exit(1);
	}
	exposure = 1.0;
	getheader(pictfp, getexpos);
	if (fgetresolu(&pxsiz, &pysiz, pictfp) != (YMAJOR|YDECR)) {
		fprintf("%s: bad picture resolution\n", fn);
		exit(1);
	}
	scanpos = (long *)malloc(pysiz*sizeof(long));
	if (scanpos == NULL)
		memerr("scanline positions");
	for (i = pysiz-1; i >= 0; i--)
		scanpos[i] = -1L;
	curpos = pysiz-1;
	for (i = 0; i < NSCANS; i++) {
		scan[i].lused = -1;
		scan[i].y = -1;
		scan[i].sl = (COLR *)malloc(pxsiz*sizeof(COLR));
		if (scan[i].sl == NULL)
			memerr("scanline buffers");
	}
}


close_pict()			/* done with picture */
{
	register int	i;

	if (pictfp == NULL)
		return;
	fclose(pictfp);
	free((char *)scanpos);
	for (i = 0; i < NSCANS; i++)
		free((char *)scan[i].sl);
	pictfp = NULL;
}


fork_rtrace(av)			/* open pipe and start rtrace */
char	*av[];
{
	int	p0[2], p1[2];

	if (pipe(p0) < 0 || pipe(p1) < 0) {
		perror(progname);
		exit(1);
	}
	if ((rt_pid = vfork()) == 0) {		/* if child */
		close(p0[1]);
		close(p1[0]);
		if (p0[0] != 0) {	/* connect p0 to stdin */
			dup2(p0[0], 0);
			close(p0[0]);
		}
		if (p1[1] != 0) {	/* connect p1 to stdout */
			dup2(p1[1], 1);
			close(p1[1]);
		}
		execvp(av[0], av);
		perror(av[0]);
		_exit(127);
	}
	if (rt_pid == -1) {
		perror(progname);
		exit(1);
	}
	close(p0[0]);
	close(p1[1]);
	fd_tort = p0[1];
	fd_fromrt = p1[0];
}


done_rtrace()			/* wait for rtrace to finish */
{
	int	pid, status;

	if (rt_pid == -1)
		return;
	close(fd_tort);
	close(fd_fromrt);
	while ((pid = wait(&status)) != -1 && pid != rt_pid)
		;
	if (pid == rt_pid && status != 0) {
		fprintf(stderr, "%s: bad status (%d) from rtrace\n",
				progname, status);
		exit(1);
	}
	rt_pid = -1;
}


int
readbuf(fd, bpos, siz)
int	fd;
char	*bpos;
int	siz;
{
	register int	cc, nrem = siz;

	while (nrem > 0 && (cc = read(fd, bpos, nrem)) > 0) {
		bpos += cc;
		nrem -= cc;
	}
	if (cc < 0)
		return(cc);
	return(siz-nrem);
}


int
writebuf(fd, bpos, siz)
char	*bpos;
int	siz;
{
	register int	cc, nrem = siz;

	while (nrem > 0 && (cc = write(fd, bpos, nrem)) > 0) {
		bpos += cc;
		nrem -= cc;
	}
	if (cc < 0)
		return(cc);
	return(siz-nrem);
}
