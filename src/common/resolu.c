/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Read and write image resolutions.
 */

#include <stdio.h>

#include "resolu.h"


char  resolu_buf[RESOLU_BUFLEN];	/* resolution line buffer */


fputresolu(ord, sl, ns, fp)		/* put out picture dimensions */
int  ord;			/* scanline ordering */
int  sl, ns;			/* scanline length and number */
FILE  *fp;
{
	RESOLU  rs;

	if ((rs.or = ord) & YMAJOR) {
		rs.xr = sl;
		rs.yr = ns;
	} else {
		rs.xr = ns;
		rs.yr = sl;
	}
	fputsresolu(&rs, fp);
}


int
fgetresolu(sl, ns, fp)			/* get picture dimensions */
int  *sl, *ns;			/* scanline length and number */
FILE  *fp;
{
	RESOLU  rs;

	if (!fgetsresolu(&rs, fp))
		return(-1);
	if (rs.or & YMAJOR) {
		*sl = rs.xr;
		*ns = rs.yr;
	} else {
		*sl = rs.yr;
		*ns = rs.xr;
	}
	return(rs.or);
}


char *
resolu2str(buf, rp)		/* convert resolution struct to line */
char  *buf;
register RESOLU  *rp;
{
	if (rp->or&YMAJOR)
		sprintf(buf, "%cY %d %cX %d\n",
				rp->or&YDECR ? '-' : '+', rp->yr,
				rp->or&XDECR ? '-' : '+', rp->xr);
	else
		sprintf(buf, "%cX %d %cY %d\n",
				rp->or&XDECR ? '-' : '+', rp->xr,
				rp->or&YDECR ? '-' : '+', rp->yr);
	return(buf);
}


str2resolu(rp, buf)		/* convert resolution line to struct */
register RESOLU  *rp;
char  *buf;
{
	char  *xndx, *yndx;
	register char  *cp;

	if (buf == NULL)
		return(0);
	xndx = yndx = NULL;
	for (cp = buf+1; *cp; cp++)
		if (*cp == 'X')
			xndx = cp;
		else if (*cp == 'Y')
			yndx = cp;
	if (xndx == NULL || yndx == NULL)
		return(0);
	rp->or = 0;
	if (xndx > yndx) rp->or |= YMAJOR;
	if (xndx[-1] == '-') rp->or |= XDECR;
	if (yndx[-1] == '-') rp->or |= YDECR;
	if ((rp->xr = atoi(xndx+1)) <= 0)
		return(0);
	if ((rp->yr = atoi(yndx+1)) <= 0)
		return(0);
	return(1);
}
