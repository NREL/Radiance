/* Copyright (c) 1997 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for tone mapping on Radiance RGBE and XYZE pictures.
 * See tonemap.h for detailed function descriptions.
 */

#include	<stdio.h>
#include	<math.h>
#include	"tmprivat.h"
#include	"resolu.h"


extern char	*tempbuffer();

#define	LOGISZ	260
static TMbright	logi[LOGISZ];
static BYTE	photofact[BMESUPPER-BMESLOWER];


int
tmCvColrs(ls, cs, scan, len)		/* tone map RGBE/XYZE colors */
TMbright	*ls;
BYTE	*cs;
COLR	*scan;
int	len;
{
	static char	funcName[] = "tmCvColrs";
	COLR	cmon;
	register int	i, bi, li;

	if (tmTop == NULL)
		returnErr(TM_E_TMINVAL);
	if (ls == NULL | scan == NULL | len <= 0)
		returnErr(TM_E_ILLEGAL);
	if (tmTop->flags & TM_F_NEEDMAT) {	/* need floating point */
		register COLOR	*newscan;
		newscan = (COLOR *)tempbuffer(len*sizeof(COLOR));
		if (newscan == NULL)
			returnErr(TM_E_NOMEM);
		for (i = len; i--; )
			colr_color(newscan[i], scan[i]);
		return(tmCvColors(ls, cs, newscan, len));
	}
	if (logi[0] == 0) {			/* build tables if necessary */
		for (i = 256; i--; )
			logi[i] = TM_BRTSCALE*log((i+.5)/256.) - .5;
		for (i = 256; i < LOGISZ; i++)
			logi[i] = logi[255];
		for (i = BMESLOWER; i < BMESUPPER; i++)
			photofact[i-BMESLOWER] = 256. *
					(tmLuminance(i) - LMESLOWER) /
					(LMESUPPER - LMESLOWER);
	}
	for (i = len; i--; ) {
		copycolr(cmon, scan[i]);
							/* world luminance */
		li =  ( tmTop->clfb[RED]*cmon[RED] +
			tmTop->clfb[GRN]*cmon[GRN] +
			tmTop->clfb[BLU]*cmon[BLU] ) >> 8;
		bi = BRT2SCALE*(cmon[EXP]-COLXS) +
				logi[li] + tmTop->inpsfb;
		if (bi < MINBRT) {
			bi = MINBRT-1;			/* bogus value */
			li++;				/* avoid li==0 */
		}
		ls[i] = bi;
		if (cs == TM_NOCHROM)			/* no color? */
			continue;
							/* mesopic adj. */
		if (tmTop->flags & TM_F_MESOPIC && bi < BMESUPPER) {
			register int	pf, sli = normscot(cmon);
			if (bi < BMESLOWER)
				cmon[RED] = cmon[GRN] = cmon[BLU] = sli;
			else {
				if (tmTop->flags & TM_F_BW)
					cmon[RED] = cmon[GRN] = cmon[BLU] = li;
				pf = photofact[bi-BMESLOWER];
				sli *= 256 - pf;
				cmon[RED] = ( sli + pf*cmon[RED] ) >> 8;
				cmon[GRN] = ( sli + pf*cmon[GRN] ) >> 8;
				cmon[BLU] = ( sli + pf*cmon[BLU] ) >> 8;
			}
		} else if (tmTop->flags & TM_F_BW) {
			cmon[RED] = cmon[GRN] = cmon[BLU] = li;
		}
		bi = ( (int4)TM_GAMTSZ*tmTop->clfb[RED]*cmon[RED]/li ) >> 8;
		cs[3*i  ] = bi>=TM_GAMTSZ ? 255 : tmTop->gamb[bi];
		bi = ( (int4)TM_GAMTSZ*tmTop->clfb[GRN]*cmon[GRN]/li ) >> 8;
		cs[3*i+1] = bi>=TM_GAMTSZ ? 255 : tmTop->gamb[bi];
		bi = ( (int4)TM_GAMTSZ*tmTop->clfb[BLU]*cmon[BLU]/li ) >> 8;
		cs[3*i+2] = bi>=TM_GAMTSZ ? 255 : tmTop->gamb[bi];
	}
	returnOK;
}


#define	FMTRGB		1	/* Input is RGBE */
#define	FMTCIE		2	/* Input is CIE XYZE */
#define FMTUNK		3	/* Input format is unspecified */
#define FMTBAD		4	/* Input is not a recognized format */

static struct radhead {
	int	format;		/* FMTRGB, FMTCIE, FMTUNK, FMTBAD */
	double	expos;		/* input exposure value */
	RGBPRIMP	primp;	/* input primaries */
	RGBPRIMS	mypri;	/* custom primaries */
} rhdefault = {FMTUNK, 1., stdprims, STDPRIMS};


static int
headline(s, rh)			/* grok a header line */
register char	*s;
register struct radhead	*rh;
{
	char	fmt[32];

	if (formatval(fmt, s)) {
		if (!strcmp(fmt, COLRFMT))
			rh->format = FMTRGB;
		else if (!strcmp(fmt, CIEFMT))
			rh->format = FMTCIE;
		else
			rh->format = FMTBAD;
		return;
	}
	if (isexpos(s)) {
		rh->expos *= exposval(s);
		return;
	}
	if (isprims(s)) {
		primsval(rh->mypri, s);
		rh->primp = rh->mypri;
		return;
	}
}


int
tmLoadPicture(lpp, cpp, xp, yp, fname, fp)	/* convert Radiance picture */
TMbright	**lpp;
BYTE	**cpp;
int	*xp, *yp;
char	*fname;
FILE	*fp;
{
	char	*funcName = fname==NULL ? "tmLoadPicture" : fname;
	FILE	*inpf;
	struct radhead	info;
	int	err;
	COLR	*scanin = NULL;
	int	i;
						/* check arguments */
	if (tmTop == NULL)
		returnErr(TM_E_TMINVAL);
	if (lpp == NULL | xp == NULL | yp == NULL |
			(fname == NULL & fp == TM_GETFILE))
		returnErr(TM_E_ILLEGAL);
	*xp = *yp = 0;				/* error precaution */
	if ((inpf = fp) == TM_GETFILE && (inpf = fopen(fname, "r")) == NULL)
		returnErr(TM_E_BADFILE);
	info = rhdefault;			/* get our header */
	getheader(inpf, headline, (char *)&info);
	if (info.format == FMTBAD | info.expos <= 0. ||
			fgetresolu(xp, yp, inpf) < 0) {
		err = TM_E_BADFILE; goto done;
	}
	if (info.format == FMTUNK)		/* assume RGBE format */
		info.format = FMTRGB;
	if (info.format == FMTRGB)
		info.expos /= WHTEFFICACY;
	else if (info.format == FMTCIE)
		info.primp = TM_XYZPRIM;
						/* prepare library */
	if ((err = tmSetSpace(info.primp, 1./info.expos)) != TM_E_OK)
		goto done;
	err = TM_E_NOMEM;			/* allocate arrays */
	*lpp = (TMbright *)malloc(sizeof(TMbright) * *xp * *yp);
	if (*lpp == NULL)
		goto done;
	if (cpp != TM_NOCHROMP) {
		*cpp = (BYTE *)malloc(3*sizeof(BYTE) * *xp * *yp);
		if (*cpp == NULL)
			goto done;
	}
	scanin = (COLR *)malloc(sizeof(COLR) * *xp);
	if (scanin == NULL)
		goto done;
	err = TM_E_BADFILE;			/* read & convert scanlines */
	for (i = 0; i < *yp; i++) {
		if (freadcolrs(scanin, *xp, inpf) < 0) {
			err = TM_E_BADFILE; break;
		}
		err = tmCvColrs(*lpp + (i * *xp),
			cpp==TM_NOCHROMP ? TM_NOCHROM : *cpp + (i * 3 * *xp),
				scanin, *xp);
		if (err != TM_E_OK)
			break;
	}
done:						/* clean up */
	if (fp == NULL)
		fclose(inpf);
	if (scanin != NULL)
		free((char *)scanin);
	if (err != TM_E_OK)
		returnErr(err);
	returnOK;
}


int					/* run pcond to map picture */
dopcond(psp, xp, yp, flags, monpri, gamval, Lddyn, Ldmax, fname)
BYTE	**psp;
int	*xp, *yp;
int	flags;
RGBPRIMP	monpri;
double	gamval, Lddyn, Ldmax;
char	*fname;
{
	char	*funcName = fname;
	char	cmdbuf[512];
	FILE	*infp;
	register COLR	*scan;
	register BYTE	*rp;
	int	y;
	register int	x;
					/* set up gamma correction */
	if (setcolrcor(pow, 1./gamval) < 0)
		returnErr(TM_E_NOMEM);
					/* create command */
	strcpy(cmdbuf, "pcond ");
	if (flags & TM_F_HCONTR)
		strcat(cmdbuf, "-s ");
	if (flags & TM_F_MESOPIC)
		strcat(cmdbuf, "-c ");
	if (flags & TM_F_LINEAR)
		strcat(cmdbuf, "-l ");
	if (flags & TM_F_ACUITY)
		strcat(cmdbuf, "-a ");
	if (flags & TM_F_VEIL)
		strcat(cmdbuf, "-v ");
	if (flags & TM_F_CWEIGHT)
		strcat(cmdbuf, "-w ");
	sprintf(cmdbuf+strlen(cmdbuf),
			"-p %f %f %f %f %f %f %f %f -d %f -u %f %s",
			monpri[RED][CIEX], monpri[RED][CIEY],
			monpri[GRN][CIEX], monpri[GRN][CIEY],
			monpri[BLU][CIEX], monpri[BLU][CIEY],
			monpri[WHT][CIEX], monpri[WHT][CIEY],
			Lddyn, Ldmax, fname);
					/* start pcond */
	if ((infp = popen(cmdbuf, "r")) == NULL)
		returnErr(TM_E_BADFILE);
					/* check picture format and size */
	if (checkheader(infp, COLRFMT, NULL) < 0 ||
			fgetresolu(xp, yp, infp) < 0) {
		pclose(infp);
		returnErr(TM_E_BADFILE);
	}
					/* allocate arrays */
	if (flags & TM_F_BW)
		rp = (BYTE *)malloc(sizeof(BYTE) * *xp * *yp);
	else
		rp = (BYTE *)malloc(3*sizeof(BYTE) * *xp * *yp);
	scan = (COLR *)malloc(sizeof(COLR) * *xp);
	if ((*psp = rp) == NULL | scan == NULL) {
		pclose(infp);
		returnErr(TM_E_NOMEM);
	}
					/* read and gamma map file */
	for (y = 0; y < *yp; y++) {
		if (freadcolrs(scan, *xp, infp) < 0) {
			pclose(infp);
			returnErr(TM_E_BADFILE);
		}
		colrs_gambs(scan, *xp);
		if (flags & TM_F_BW)
			for (x = 0; x < *xp; x++)
				*rp++ = normbright(scan[x]);
		else
			for (x = 0; x < *xp; x++) {
				*rp++ = scan[x][RED];
				*rp++ = scan[x][GRN];
				*rp++ = scan[x][BLU];
			}
	}
	free((char *)scan);
	pclose(infp);
	returnOK;
}


int					/* map a Radiance picture */
tmMapPicture(psp, xp, yp, flags, monpri, gamval, Lddyn, Ldmax, fname, fp)
BYTE	**psp;
int	*xp, *yp;
int	flags;
RGBPRIMP	monpri;
double	gamval, Lddyn, Ldmax;
char	*fname;
FILE	*fp;
{
	char	*funcName = fname==NULL ? "tmMapPicture" : fname;
	FILE	*inpf;
	BYTE	*cp;
	TMbright	*lp;
	int	err;
						/* check arguments */
	if (psp == NULL | xp == NULL | yp == NULL | monpri == NULL |
			(fname == NULL & fp == TM_GETFILE))
		returnErr(TM_E_ILLEGAL);
						/* set defaults */
	if (gamval < MINGAM) gamval = DEFGAM;
	if (Lddyn < MINLDDYN) Lddyn = DEFLDDYN;
	if (Ldmax < MINLDMAX) Ldmax = DEFLDMAX;
	if (flags & TM_F_BW) monpri = stdprims;
						/* check for pcond run */
	if (fp == TM_GETFILE && flags & TM_F_UNIMPL)
		return( dopcond(psp, xp, yp, flags,
				monpri, gamval, Lddyn, Ldmax, fname) );
						/* initialize tone mapping */
	if (tmInit(flags, monpri, gamval) == NULL)
		returnErr(TM_E_NOMEM);
						/* load & convert picture */
	err = tmLoadPicture(&lp, (flags&TM_F_BW) ? TM_NOCHROMP : &cp,
			xp, yp, fname, fp);
	if (err != TM_E_OK) {
		tmDone(NULL);
		return(err);
	}
						/* allocate space for result */
	if (flags & TM_F_BW) {
		*psp = (BYTE *)malloc(sizeof(BYTE) * *xp * *yp);
		if (*psp == NULL)
			returnErr(TM_E_NOMEM);
		cp = TM_NOCHROM;
	} else
		*psp = cp;
						/* compute color mapping */
	err = tmAddHisto(lp, *xp * *yp, 1);
	if (err != TM_E_OK)
		goto done;
	err = tmComputeMapping(gamval, Lddyn, Ldmax);
	if (err != TM_E_OK)
		goto done;
						/* map colors */
	err = tmMapPixels(*psp, lp, cp, *xp * *yp);

done:						/* clean up */
	free((char *)lp);
	tmDone(NULL);
	if (err != TM_E_OK) {			/* free memory on error */
		free((char *)*psp);
		*psp = NULL;
		returnErr(err);
	}
	returnOK;
}
