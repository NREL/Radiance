/* Copyright (c) 1996 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Input and output conditioning routines for pcond.
 */

#include "pcond.h"


RGBPRIMP	inprims = stdprims;	/* input primaries */
COLORMAT	inrgb2xyz;		/* convert input RGB to XYZ */
RGBPRIMP	outprims = stdprims;	/* output primaries */

double	(*lumf)() = rgblum;		/* input luminance function */
double	inpexp = 1.0;			/* input exposure value */

char	*mbcalfile = NULL;		/* macbethcal mapping file */

static struct mbc {
	float	xa[3][6], ya[3][6];
	COLORMAT	cmat;
}	mbcond;				/* macbethcal conditioning struct */

static COLOR	*scanbuf;		/* scanline processing buffer */
static int	nread;			/* number of scanlines processed */


double
rgblum(clr, scotopic)		/* compute (scotopic) luminance of RGB color */
COLOR	clr;
int	scotopic;
{
	if (scotopic)		/* approximate */
		return( WHTSEFFICACY * (colval(clr,RED)*.062 +
			colval(clr,GRN)*.608 + colval(clr,BLU)*.330) );
	return( WHTEFFICACY * (colval(clr,RED)*inrgb2xyz[1][0] +
			colval(clr,GRN)*inrgb2xyz[1][1] +
			colval(clr,BLU)*inrgb2xyz[1][2]) );
}


double
cielum(xyz, scotopic)		/* compute (scotopic) luminance of CIE color */
COLOR	xyz;
int	scotopic;
{
	if (scotopic)		/* approximate */
		return(colval(xyz,CIEY) *
			(1.33*(1. + (colval(xyz,CIEY)+colval(xyz,CIEZ))/
			colval(xyz,CIEX)) - 1.68));
	return(colval(xyz,CIEY));
}


COLOR *
nextscan()				/* read and condition next scanline */
{
	if (nread >= numscans(&inpres)) {
#ifdef DEBUG
		fputs("done\n", stderr);
#endif
		free((char *)scanbuf);
		return(scanbuf = NULL);
	}
	if (what2do&DO_ACUITY)
		acuscan(scanbuf, nread);
	else if (freadscan(scanbuf, scanlen(&inpres), infp) < 0) {
		fprintf(stderr, "%s: %s: scanline read error\n",
				progname, infn);
		exit(1);
	}
	if (what2do&DO_VEIL)			/* add veiling */
		addveil(scanbuf, nread);
	if (what2do&DO_COLOR)			/* scotopic color loss */
		scotscan(scanbuf, scanlen(&inpres));
	if (what2do&DO_LINEAR)			/* map luminances */
		sfscan(scanbuf, scanlen(&inpres), scalef);
	else
		mapscan(scanbuf, scanlen(&inpres));
	if (mbcalfile != NULL)			/* device color correction */
		mbscan(scanbuf, scanlen(&inpres), &mbcond);
	else if (lumf == cielum | inprims != outprims)
		matscan(scanbuf, scanlen(&inpres), mbcond.cmat);
	nread++;
	return(scanbuf);
}


COLOR *
firstscan()				/* return first processed scanline */
{
	if (mbcalfile != NULL)		/* load macbethcal file */
		getmbcalfile(mbcalfile, &mbcond);
	else
		if (lumf == rgblum)
			comprgb2rgbmat(mbcond.cmat, inprims, outprims);
		else
			compxyz2rgbmat(mbcond.cmat, outprims);
	if (what2do&DO_ACUITY) {
#ifdef DEBUG
		fprintf(stderr, "%s: initializing acuity sampling...",
				progname);
#endif
		initacuity();
#ifdef DEBUG
		fprintf(stderr, "done\n");
#endif
	}
	scanbuf = (COLOR *)malloc(scanlen(&inpres)*sizeof(COLOR));
	if (scanbuf == NULL)
		syserror("malloc");
	nread = 0;
#ifdef DEBUG
	fprintf(stderr, "%s: processing image...", progname);
#endif
	return(nextscan());
}


sfscan(sl, len, sf)			/* apply scalefactor to scanline */
register COLOR	*sl;
int	len;
double	sf;
{
	while (len--) {
		scalecolor(sl[0], sf);
		sl++;
	}
}


matscan(sl, len, mat)			/* apply color matrix to scaline */
register COLOR	*sl;
int	len;
COLORMAT	mat;
{
	while (len--) {
		colortrans(sl[0], mat, sl[0]);
		sl++;
	}
}


mbscan(sl, len, mb)			/* apply macbethcal adj. to scaline */
COLOR	*sl;
int	len;
register struct mbc	*mb;
{
	double	d;
	register int	i, j;

	while (len--) {
		for (i = 0; i < 3; i++) {
			d = colval(sl[0],i);
			for (j = 0; j < 4 && mb->xa[i][j+1] <= d; j++)
				;
			colval(sl[0],i) = ( (mb->xa[i][j+1] - d)*mb->ya[i][j] +
					(d - mb->xa[i][j])*mb->ya[i][j+1] ) /
					(mb->xa[i][j+1] - mb->xa[i][j]);
		}
		colortrans(sl[0], mb->cmat, sl[0]);
		sl++;
	}
}


getmbcalfile(fn, mb)			/* load macbethcal file */
char	*fn;
register struct mbc	*mb;
{
	extern char	*fgets();
	char	buf[128];
	FILE	*fp;
	int	inpflags = 0;

	if ((fp = fopen(fn, "r")) == NULL)
		syserror(fn);
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (!(inpflags & 01) &&
				sscanf(buf,
				"rxa(i) : select(i,%f,%f,%f,%f,%f,%f)",
				&mb->xa[0][0], &mb->xa[0][1],
				&mb->xa[0][2], &mb->xa[0][3],
				&mb->xa[0][4], &mb->xa[0][5]
				) == 6)
			inpflags |= 01;
		else if (!(inpflags & 02) &&
				sscanf(buf,
				"rya(i) : select(i,%f,%f,%f,%f,%f,%f)",
				&mb->ya[0][0], &mb->ya[0][1],
				&mb->ya[0][2], &mb->ya[0][3],
				&mb->ya[0][4], &mb->ya[0][5]
				) == 6)
			inpflags |= 02;
		else if (!(inpflags & 04) &&
				sscanf(buf,
				"gxa(i) : select(i,%f,%f,%f,%f,%f,%f)",
				&mb->xa[1][0], &mb->xa[1][1],
				&mb->xa[1][2], &mb->xa[1][3],
				&mb->xa[1][4], &mb->xa[1][5]
				) == 6)
			inpflags |= 04;
		else if (!(inpflags & 010) &&
				sscanf(buf,
				"gya(i) : select(i,%f,%f,%f,%f,%f,%f)",
				&mb->ya[1][0], &mb->ya[1][1],
				&mb->ya[1][2], &mb->ya[1][3],
				&mb->ya[1][4], &mb->ya[1][5]
				) == 6)
			inpflags |= 010;
		else if (!(inpflags & 020) &&
				sscanf(buf,
				"bxa(i) : select(i,%f,%f,%f,%f,%f,%f)",
				&mb->xa[2][0], &mb->xa[2][1],
				&mb->xa[2][2], &mb->xa[2][3],
				&mb->xa[2][4], &mb->xa[2][5]
				) == 6)
			inpflags |= 020;
		else if (!(inpflags & 040) &&
				sscanf(buf,
				"bya(i) : select(i,%f,%f,%f,%f,%f,%f)",
				&mb->ya[2][0], &mb->ya[2][1],
				&mb->ya[2][2], &mb->ya[2][3],
				&mb->ya[2][4], &mb->ya[2][5]
				) == 6)
			inpflags |= 040;
		else if (!(inpflags & 0100) &&
				sscanf(buf,
				"ro = %f*rn + %f*gn + %f*bn",
				&mb->cmat[0][0], &mb->cmat[0][1],
				&mb->cmat[0][2]) == 3)
			inpflags |= 0100;
		else if (!(inpflags & 0200) &&
				sscanf(buf,
				"go = %f*rn + %f*gn + %f*bn",
				&mb->cmat[1][0], &mb->cmat[1][1],
				&mb->cmat[1][2]) == 3)
			inpflags |= 0200;
		else if (!(inpflags & 0400) &&
				sscanf(buf,
				"bo = %f*rn + %f*gn + %f*bn",
				&mb->cmat[2][0], &mb->cmat[2][1],
				&mb->cmat[2][2]) == 3)
			inpflags |= 0400;
	}
	if (inpflags != 0777) {
		fprintf(stderr,
		"%s: cannot grok macbethcal file \"%s\" (inpflags==0%o)\n",
			progname, fn, inpflags);
		exit(1);
	}
	fclose(fp);
}
