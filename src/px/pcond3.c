/* Copyright (c) 1996 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for computing and applying brightness mapping.
 */

#include "pcond.h"


#define CVRATIO		0.025		/* fraction of pixels allowed > env. */

#define exp10(x)	exp(2.302585093*(x))

int	modhist[HISTRES];		/* modified histogram */
float	cumf[HISTRES+1];		/* cumulative distribution function */


mkcumf()			/* make cumulative distribution function */
{
	register int	i;
	register long	sum;

	cumf[0] = 0.;
	sum = modhist[0];
	for (i = 1; i < HISTRES; i++) {
		cumf[i] = (double)sum/histot;
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
	return(Bldmin + cf(Bl(Lw))*(Bldmax-Bldmin));
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
shiftdir(bw)		/* compute shift direction for histogram */
double	bw;
{
	if (what2do&DO_HSENS && cf(bw) - (bw - bwmin)/(Bldmax - bwmin))
		return(1);
	return(-1);
}


int
mkbrmap()			/* make dynamic range map */
{
	int	hdiffs[HISTRES], above, below;
	double	T, b, s;
	int	maxd, maxi, sd;
	register int	i;
					/* copy initial histogram */
	for (i = 0; i < HISTRES; i++)
		modhist[i] = bwhist[i];
	T = histot * (bwmax - bwmin) / HISTRES;
	s = (bwmax - bwmin)/HISTRES;
					/* loop until satisfactory */
	for ( ; ; ) {
		mkcumf();		/* sync brightness mapping */
		above = below = 0;	/* compute visibility overflow */
		for (i = 0, b = bwmin + .5*s; i < HISTRES; i++, b += s) {
			hdiffs[i] = modhist[i] - (int)(T*clampf(Lb(b)) + .5);
			if (hdiffs[i] > 0) above += hdiffs[i];
			else below -= hdiffs[i];
		}
		if (above <= histot*CVRATIO)
			break;		/* close enough */
		if (above-below >= 0)
			return(-1);	/* Houston, we have a problem.... */
		/* original looped here as well (BEGIN_L2) */
		maxd = 0;		/* find largest overvis */
		for (i = 0; i < HISTRES; i++)
			if (hdiffs[i] > maxd)
				maxd = hdiffs[maxi=i];
		/* broke loop here when (maxd == 0) (BREAK_L2) */
		for (sd = shiftdir((maxi+.5)/HISTRES*(bwmax-bwmin)+bwmin);
				hdiffs[maxi] == maxd; sd = -sd)
			for (i = maxi+sd; i >= 0 & i < HISTRES; i += sd)
				if (hdiffs[i] < 0) {
					if (hdiffs[i] <= -maxd) {
						modhist[i] += maxd;
						modhist[maxi] -= maxd;
						hdiffs[i] += maxd;
						hdiffs[maxi] = 0;
					} else {
						modhist[maxi] += hdiffs[i];
						modhist[i] -= hdiffs[i];
						hdiffs[maxi] += hdiffs[i];
						hdiffs[i] = 0;
					}
					break;
				}
		/* (END_L2) */
	}
	return(0);
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


#ifdef DEBUG
doplots()			/* generate debugging plots */
{
	double	T, b, s;
	FILE	*fp;
	char	fname[128];
	register int	i;

	T = histot * (bwmax - bwmin) / HISTRES;
	s = (bwmax - bwmin)/HISTRES;

	sprintf(fname, "%s_hist.plt", infn);
	if ((fp = fopen(fname, "w")) == NULL)
		syserror(fname);
	fputs("include=curve.plt\n", fp);
	fputs("title=\"Brightness Frequency Distribution\"\n", fp);
	fprintf(fp, "subtitle=%s\n", infn);
	fputs("ymin=0\n", fp);
	fputs("xlabel=\"Perceptual Brightness B(Lw)\"\n", fp);
	fputs("ylabel=\"Frequency Count\"\n", fp);
	fputs("Alabel=\"Histogram\"\n", fp);
	fputs("Alintype=0\n", fp);
	fputs("Blabel=\"Envelope\"\n", fp);
	fputs("Bsymsize=0\n", fp);
	fputs("Adata=\n", fp);
	for (i = 0, b = bwmin + .5*s; i < HISTRES; i++, b += s)
		fprintf(fp, "\t%f %d\n", b, modhist[i]);
	fputs(";\nBdata=\n", fp);
	for (i = 0, b = bwmin + .5*s; i < HISTRES; i++, b += s)
		fprintf(fp, "\t%f %f\n", b, T*clampf(Lb(b)));
	fputs(";\n", fp);
	fclose(fp);
}
#endif
