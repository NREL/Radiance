/* Copyright (c) 1996 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for computing and applying brightness mapping.
 */

#include "pcond.h"


#define CVRATIO		0.025		/* fraction of samples allowed > env. */

#define exp10(x)	exp(2.302585093*(x))

float	modhist[HISTRES];		/* modified histogram */
double	mhistot;			/* modified histogram total */
float	cumf[HISTRES+1];		/* cumulative distribution function */


mkcumf()			/* make cumulative distribution function */
{
	register int	i;
	register double	sum;

	mhistot = 0.;		/* compute modified total */
	for (i = 0; i < HISTRES; i++)
		mhistot += modhist[i];

	sum = 0.;		/* compute cumulative function */
	for (i = 0; i < HISTRES; i++) {
		cumf[i] = sum/mhistot;
		sum += modhist[i];
	}
	cumf[HISTRES] = 1.;
}


double
cf(b)				/* return cumulative function at b */
double	b;
{
	double	x;
	register int	i;

	i = x = HISTRES*(b - bwmin)/(bwmax - bwmin);
	x -= (double)i;
	return(cumf[i]*(1.-x) + cumf[i+1]*x);
}


double
BLw(Lw)				/* map world luminance to display brightness */
double	Lw;
{
	double	b;

	if (Lw <= LMIN || (b = Bl(Lw)) <= bwmin+FTINY)
		return(Bldmin);
	if (b >= bwmax-FTINY)
		return(Bldmax);
	return(Bldmin + cf(b)*(Bldmax-Bldmin));
}


double
htcontrs(La)		/* human threshold contrast sensitivity, dL(La) */
double	La;
{
	double	l10La, l10dL;
				/* formula taken from Ferwerda et al. [SG96] */
	if (La < 1.148e-4)
		return(1.38e-3);
	l10La = log10(La);
	if (l10La < -1.44)		/* rod response regime */
		l10dL = pow(.405*l10La + 1.6, 2.18) - 2.86;
	else if (l10La < -.0184)
		l10dL = l10La - .395;
	else if (l10La < 1.9)		/* cone response regime */
		l10dL = pow(.249*l10La + .65, 2.7) - .72;
	else
		l10dL = l10La - 1.255;

	return(exp10(l10dL));
}


double
clampf(Lw)		/* derivative clamping function */
double	Lw;
{
	double	bLw, ratio;

	bLw = BLw(Lw);		/* apply current brightness map */
	ratio = what2do&DO_HSENS ? htcontrs(Lb(bLw))/htcontrs(Lw) : Lb(bLw)/Lw;
	return(ratio/(Lb1(bLw)*(Bldmax-Bldmin)*Bl1(Lw)));
}


int
mkbrmap()			/* make dynamic range map */
{
	double	T, b, s;
	double	ceiling, trimmings;
	register int	i;
					/* copy initial histogram */
	bcopy((char *)bwhist, (char *)modhist, sizeof(modhist));
	s = (bwmax - bwmin)/HISTRES;
					/* loop until satisfactory */
	do {
		mkcumf();			/* sync brightness mapping */
		if (mhistot <= histot*CVRATIO)
			return(-1);		/* no compression needed! */
		T = mhistot * (bwmax - bwmin) / HISTRES;
		trimmings = 0.;			/* clip to envelope */
		for (i = 0, b = bwmin + .5*s; i < HISTRES; i++, b += s) {
			ceiling = T*clampf(Lb(b));
			if (modhist[i] > ceiling) {
				trimmings += modhist[i] - ceiling;
				modhist[i] = ceiling;
			}
		}
	} while (trimmings > histot*CVRATIO);

	return(0);			/* we got it */
}


scotscan(scan, xres)		/* apply scotopic color sensitivity loss */
COLOR	*scan;
int	xres;
{
	COLOR	ctmp;
	double	incolor, b, Lw;
	register int	i;

	for (i = 0; i < xres; i++) {
		Lw = plum(scan[i]);
		if (Lw >= TopMesopic)
			incolor = 1.;
		else if (Lw <= BotMesopic)
			incolor = 0.;
		else
			incolor = (Lw - BotMesopic) /
					(TopMesopic - BotMesopic);
		if (incolor < 1.-FTINY) {
			b = (1.-incolor)*slum(scan[i])*inpexp/SWNORM;
			if (lumf == rgblum) b /= WHTEFFICACY;
			setcolor(ctmp, b, b, b);
			if (incolor <= FTINY)
				setcolor(scan[i], 0., 0., 0.);
			else
				scalecolor(scan[i], incolor);
			addcolor(scan[i], ctmp);
		}
	}
}


mapscan(scan, xres)		/* apply tone mapping operator to scanline */
COLOR	*scan;
int	xres;
{
	double	mult, Lw, b;
	register int	i;

	for (i = 0; i < xres; i++) {
		Lw = plum(scan[i]);
		if (Lw < LMIN) {
			setcolor(scan[i], 0., 0., 0.);
			continue;
		}
		b = BLw(Lw);
		mult = (Lb(b) - ldmin)/(ldmax - ldmin) / (Lw*inpexp);
		if (lumf == rgblum) mult *= WHTEFFICACY;
		scalecolor(scan[i], mult);
	}
}


putmapping(fp)			/* put out mapping function */
FILE	*fp;
{
	double	b, s;
	register int	i;
	double	wlum, sf;

	sf = scalef*inpexp;
	if (lumf == cielum) sf *= WHTEFFICACY;
	s = (bwmax - bwmin)/HISTRES;
	for (i = 0, b = bwmin + .5*s; i < HISTRES; i++, b += s) {
		wlum = Lb(b);
		if (what2do&DO_LINEAR)
			fprintf(fp, "%e %e\n", wlum, sf*wlum);
		else
			fprintf(fp, "%e %e\n", wlum, Lb(BLw(wlum)));
	}
}
