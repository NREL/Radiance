/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
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

#define GAMTSZ	1024

typedef struct {
	BYTE		gamb[GAMTSZ];	/* gamma lookup table */
	COLR		clfb;		/* encoded tm->clf */
	TMbright	inpsfb;		/* encoded tm->inpsf */
} COLRDATA;

static MEM_PTR	colrInit();
static void	colrNewSpace();
extern void	free();
static struct tmPackage	colrPkg = {	/* our package functions */
	colrInit, colrNewSpace, free
};
static int	colrReg = -1;		/* our package registration number */

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
	register COLRDATA	*cd;
	register int	i, bi, li;

	if (tmTop == NULL)
		returnErr(TM_E_TMINVAL);
	if (ls == NULL | scan == NULL | len < 0)
		returnErr(TM_E_ILLEGAL);
	if (tmNeedMatrix(tmTop)) {		/* need floating point */
		register COLOR	*newscan;
		newscan = (COLOR *)tempbuffer(len*sizeof(COLOR));
		if (newscan == NULL)
			returnErr(TM_E_NOMEM);
		for (i = len; i--; )
			colr_color(newscan[i], scan[i]);
		return(tmCvColors(ls, cs, newscan, len));
	}
	if (colrReg == -1) {			/* build tables if necessary */
		colrReg = tmRegPkg(&colrPkg);
		if (colrReg < 0)
			returnErr(TM_E_CODERR1);
		for (i = 256; i--; )
			logi[i] = TM_BRTSCALE*log((i+.5)/256.) - .5;
		for (i = 256; i < LOGISZ; i++)
			logi[i] = logi[255];
		for (i = BMESLOWER; i < BMESUPPER; i++)
			photofact[i-BMESLOWER] = 256. *
					(tmLuminance(i) - LMESLOWER) /
					(LMESUPPER - LMESLOWER);
	}
	if ((cd = (COLRDATA *)tmPkgData(tmTop,colrReg)) == NULL)
		returnErr(TM_E_NOMEM);
	for (i = len; i--; ) {
		copycolr(cmon, scan[i]);
							/* world luminance */
		li =  ( cd->clfb[RED]*cmon[RED] +
			cd->clfb[GRN]*cmon[GRN] +
			cd->clfb[BLU]*cmon[BLU] ) >> 8;
		bi = BRT2SCALE*(cmon[EXP]-COLXS) +
				logi[li] + cd->inpsfb;
		if (bi < MINBRT) {
			bi = MINBRT-1;			/* bogus value */
			li++;				/* avoid li==0 */
		}
		ls[i] = bi;
		if (cs == TM_NOCHROM)			/* no color? */
			continue;
		if (tmTop->flags & TM_F_BW)
			cmon[RED] = cmon[GRN] = cmon[BLU] = li;
							/* mesopic adj. */
		if (tmTop->flags & TM_F_MESOPIC && bi < BMESUPPER) {
			register int	pf, sli = normscot(cmon);
			if (bi < BMESLOWER)
				cmon[RED] = cmon[GRN] = cmon[BLU] = sli;
			else {
				pf = photofact[bi-BMESLOWER];
				sli *= 256 - pf;
				cmon[RED] = ( sli + pf*cmon[RED] ) >> 8;
				cmon[GRN] = ( sli + pf*cmon[GRN] ) >> 8;
				cmon[BLU] = ( sli + pf*cmon[BLU] ) >> 8;
			}
		}
		bi = ( (int4)GAMTSZ*cd->clfb[RED]*cmon[RED]/li ) >> 8;
		cs[3*i  ] = bi>=GAMTSZ ? 255 : cd->gamb[bi];
		bi = ( (int4)GAMTSZ*cd->clfb[GRN]*cmon[GRN]/li ) >> 8;
		cs[3*i+1] = bi>=GAMTSZ ? 255 : cd->gamb[bi];
		bi = ( (int4)GAMTSZ*cd->clfb[BLU]*cmon[BLU]/li ) >> 8;
		cs[3*i+2] = bi>=GAMTSZ ? 255 : cd->gamb[bi];
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
	scan = (COLR *)malloc(sizeof(COLR) * *xp);
	if (flags & TM_F_BW)
		rp = (BYTE *)malloc(sizeof(BYTE) * *xp * *yp);
	else
		rp = (BYTE *)malloc(3*sizeof(BYTE) * *xp * *yp);
	if ((*psp = rp) == NULL | scan == NULL) {
		pclose(infp);
		returnErr(TM_E_NOMEM);
	}
					/* read and gamma map file */
	for (y = 0; y < *yp; y++) {
		if (freadcolrs(scan, *xp, infp) < 0) {
			pclose(infp);
			free((char *)scan);
			free((char *)*psp);
			*psp = NULL;
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
		if (*psp == NULL) {
			free((char *)lp);
			tmDone(NULL);
			returnErr(TM_E_NOMEM);
		}
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


static void
colrNewSpace(tms)		/* color space changed for tone mapping */
register struct tmStruct	*tms;
{
	register COLRDATA	*cd;
	double	d;

	cd = (COLRDATA *)tms->pd[colrReg];
	cd->clfb[RED] = 256.*tms->clf[RED] + .5;
	cd->clfb[GRN] = 256.*tms->clf[GRN] + .5;
	cd->clfb[BLU] = 256.*tms->clf[BLU] + .5;
	cd->clfb[EXP] = COLXS;
	d = TM_BRTSCALE*log(tms->inpsf);
	cd->inpsfb = d<0. ? d-.5 : d+.5;
}


static MEM_PTR
colrInit(tms)			/* initialize private data for tone mapping */
register struct tmStruct	*tms;
{
	register COLRDATA	*cd;
	register int	i;
					/* allocate our data */
	cd = (COLRDATA *)malloc(sizeof(COLRDATA));
	if (cd == NULL)
		return(NULL);
	tms->pd[colrReg] = (MEM_PTR)cd;
					/* compute gamma table */
	for (i = GAMTSZ; i--; )
		cd->gamb[i] = 256.*pow((i+.5)/GAMTSZ, 1./tms->mongam);
					/* compute color and scale factors */
	colrNewSpace(tms);
	return((MEM_PTR)cd);
}
