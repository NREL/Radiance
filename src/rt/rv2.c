/* Copyright (c) 1987 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  rv2.c - command routines used in tracing a view.
 *
 *     3/24/87
 */

#include  "ray.h"

#include  "octree.h"

#include  "otypes.h"

#include  "rpaint.h"

#include  <ctype.h>

#define  CTRL(c)	('c'-'@')

extern char  *progname;


getframe(s)				/* get a new frame */
char  *s;
{
	int  x0, y0, x1, y1;

	if (!strcmp("all", s)) {
		pframe.l = pframe.d = 0;
		pframe.r = ourview.hresolu;
		pframe.u = ourview.vresolu;
		pdepth = 0;
		return;
	}
	if (sscanf(s, "%d %d %d %d", &x0, &y0, &x1, &y1) != 4) {
		if (dev->getcur == NULL)
			return;
		(*dev->comout)("Pick first corner\n");
		if ((*dev->getcur)(&x0, &y0) == ABORT)
			return;
		(*dev->comout)("Pick second corner\n");
		if ((*dev->getcur)(&x1, &y1) == ABORT)
			return;
	}
	if (x0 < x1) {
		pframe.l = x0;
		pframe.r = x1;
	} else {
		pframe.l = x1;
		pframe.r = x0;
	}
	if (y0 < y1) {
		pframe.d = y0;
		pframe.u = y1;
	} else {
		pframe.d = y1;
		pframe.u = y0;
	}
	if (pframe.l < 0) pframe.l = 0;
	if (pframe.d < 0) pframe.d = 0;
	if (pframe.r > ourview.hresolu) pframe.r = ourview.hresolu;
	if (pframe.u > ourview.vresolu) pframe.u = ourview.vresolu;
	if (pframe.l > pframe.r) pframe.l = pframe.r;
	if (pframe.d > pframe.u) pframe.d = pframe.u;
	pdepth = 0;
}


getview(s)				/* get/show view parameters */
char  *s;
{
	FILE  *fp;
	char  buf[128];
	char  *fname;
	int  change = 0;
	VIEW  nv;

	if (sscanf(s, "%s", buf) == 1) {	/* write parameters to a file */
		if ((fname = getpath(buf, NULL, 0)) == NULL ||
				(fp = fopen(fname, "a")) == NULL) {
			sprintf(errmsg, "cannot open \"%s\"", buf);
			error(COMMAND, errmsg);
			return;
		}
		fputs(progname, fp);
		fprintview(&ourview, fp);
		fputs("\n", fp);
		fclose(fp);
		return;
	}
	sprintf(buf, "view type (%c): ", ourview.type);
	(*dev->comout)(buf);
	(*dev->comin)(buf);
	if (buf[0] == CTRL(C)) return;
	if (buf[0] && buf[0] != ourview.type) {
		nv.type = buf[0];
		change++;
	} else
		nv.type = ourview.type;
	sprintf(buf, "view point (%.6g %.6g %.6g): ",
			ourview.vp[0], ourview.vp[1], ourview.vp[2]);
	(*dev->comout)(buf);
	(*dev->comin)(buf);
	if (buf[0] == CTRL(C)) return;
	if (sscanf(buf, "%lf %lf %lf", &nv.vp[0], &nv.vp[1], &nv.vp[2]) == 3)
		change++;
	else
		VCOPY(nv.vp, ourview.vp);
	sprintf(buf, "view direction (%.6g %.6g %.6g): ",
			ourview.vdir[0], ourview.vdir[1], ourview.vdir[2]);
	(*dev->comout)(buf);
	(*dev->comin)(buf);
	if (buf[0] == CTRL(C)) return;
	if (sscanf(buf,"%lf %lf %lf",&nv.vdir[0],&nv.vdir[1],&nv.vdir[2]) == 3)
		change++;
	else
		VCOPY(nv.vdir, ourview.vdir);
	sprintf(buf, "view up (%.6g %.6g %.6g): ",
			ourview.vup[0], ourview.vup[1], ourview.vup[2]);
	(*dev->comout)(buf);
	(*dev->comin)(buf);
	if (buf[0] == CTRL(C)) return;
	if (sscanf(buf,"%lf %lf %lf",&nv.vup[0],&nv.vup[1],&nv.vup[2]) == 3)
		change++;
	else
		VCOPY(nv.vup, ourview.vup);
	sprintf(buf, "view horiz and vert size (%.6g %.6g): ",
			ourview.horiz, ourview.vert);
	(*dev->comout)(buf);
	(*dev->comin)(buf);
	if (buf[0] == CTRL(C)) return;
	if (sscanf(buf, "%lf %lf", &nv.horiz, &nv.vert) == 2)
		change++;
	else {
		nv.horiz = ourview.horiz; nv.vert = ourview.vert;
	}
	sprintf(buf, "x and y resolution (%d %d): ",
			ourview.hresolu, ourview.vresolu);
	(*dev->comout)(buf);
	(*dev->comin)(buf);
	if (buf[0] == CTRL(C)) return;
	if (sscanf(buf, "%d %d", &nv.hresolu, &nv.vresolu) == 2)
		change++;
	else {
		nv.hresolu = ourview.hresolu; nv.vresolu = ourview.vresolu;
	}
	if (change)
		newview(&nv);
}


lastview(s)				/* return to a previous view */
char  *s;
{
	char  buf[128];
	char  *fname;
	int  success;
	VIEW  nv;

	if (sscanf(s, "%s", buf) == 1) {	/* get parameters from a file */
		bcopy(&stdview, &nv, sizeof(VIEW));
		if ((fname = getpath(buf, NULL, 0)) == NULL ||
				(success = viewfile(fname, &nv)) == -1) {
			sprintf(errmsg, "cannot open \"%s\"", buf);
			error(COMMAND, errmsg);
			return;
		}
		if (!success)
			error(COMMAND, "wrong file format");
		else
			newview(&nv);
		return;
	}
	if (oldview.hresolu == 0) {	/* no old view! */
		error(COMMAND, "no previous view");
		return;
	}
	bcopy(&ourview, &nv, sizeof(VIEW));
	bcopy(&oldview, &ourview, sizeof(VIEW));
	bcopy(&nv, &oldview, sizeof(VIEW));
	newimage();
}


getinterest(s, direc, vec, mp)		/* get area of interest */
char  *s;
int  direc;
FVECT  vec;
double  *mp;
{
	int  x, y;
	RAY  thisray;
	register int  i;

	if (sscanf(s, "%lf", mp) != 1)
		*mp = 1.0;
	else if (*mp < -FTINY)		/* negative zoom is reduction */
		*mp = -1.0 / *mp;
	else if (*mp <= FTINY) {	/* too small */
		error(COMMAND, "illegal magnification");
		return(-1);
	}
	if (sscanf(s, "%*lf %lf %lf %lf", &vec[0], &vec[1], &vec[2]) != 3) {
		if (dev->getcur == NULL)
			return(-1);
		(*dev->comout)("Pick view center\n");
		if ((*dev->getcur)(&x, &y) == ABORT)
			return(-1);
		rayview(thisray.rorg, thisray.rdir, &ourview, x+.5, y+.5);
		if (!direc || ourview.type == VT_PAR) {
			rayorigin(&thisray, NULL, PRIMARY, 1.0);
			if (!localhit(&thisray, &thescene)) {
				error(COMMAND, "not a local object");
				return(-1);
			}
		}
		if (direc)
			if (ourview.type == VT_PAR)
				for (i = 0; i < 3; i++)
					vec[i] = thisray.rop[i] - ourview.vp[i];
			else
				VCOPY(vec, thisray.rdir);
		else
			VCOPY(vec, thisray.rop);
	} else if (direc)
			for (i = 0; i < 3; i++)
				vec[i] -= ourview.vp[i];
	return(0);
}


getaim(s)				/* aim camera */
char  *s;
{
	extern double  tan(), atan();
	double  zfact;
	VIEW  nv;

	if (getinterest(s, 1, nv.vdir, &zfact) < 0)
		return;
	VCOPY(nv.vp, ourview.vp);
	VCOPY(nv.vup, ourview.vup);
	nv.hresolu = ourview.hresolu; nv.vresolu = ourview.vresolu;
	if ((nv.type = ourview.type) == VT_PAR) {
		nv.horiz = ourview.horiz / zfact;
		nv.vert = ourview.vert / zfact;
	} else {
		nv.horiz = atan(tan(ourview.horiz*(PI/180./2.))/zfact) /
				(PI/180./2.);
		nv.vert = atan(tan(ourview.vert*(PI/180./2.))/zfact) /
				(PI/180./2.);
	}
	newview(&nv);
}


getmove(s)				/* move camera */
char  *s;
{
	FVECT  vc;
	double  mag;

	if (getinterest(s, 0, vc, &mag) < 0)
		return;
	moveview(0.0, 0.0, mag, vc);
}


getrotate(s)				/* rotate camera */
char  *s;
{
	extern double  normalize(), tan(), atan();
	VIEW  nv;
	FVECT  v1;
	double  angle, elev, zfact;
	
	elev = 0.0; zfact = 1.0;
	if (sscanf(s, "%lf %lf %lf", &angle, &elev, &zfact) < 1) {
		error(COMMAND, "missing angle");
		return;
	}
	VCOPY(nv.vp, ourview.vp);
	VCOPY(nv.vup, ourview.vup);
	nv.hresolu = ourview.hresolu; nv.vresolu = ourview.vresolu;
	spinvector(nv.vdir, ourview.vdir, ourview.vup, angle*(PI/180.));
	if (elev != 0.0) {
		fcross(v1, nv.vdir, ourview.vup);
		normalize(v1);
		spinvector(nv.vdir, nv.vdir, v1, elev*(PI/180.));
	}
	if ((nv.type = ourview.type) == VT_PAR) {
		nv.horiz = ourview.horiz / zfact;
		nv.vert = ourview.vert / zfact;
	} else {
		nv.horiz = atan(tan(ourview.horiz*(PI/180./2.))/zfact) /
				(PI/180./2.);
		nv.vert = atan(tan(ourview.vert*(PI/180./2.))/zfact) /
				(PI/180./2.);
	}
	newview(&nv);
}


getpivot(s)				/* pivot viewpoint */
register char  *s;
{
	FVECT  vc;
	double  angle, elev, mag;

	elev = 0.0;
	if (sscanf(s, "%lf %lf", &angle, &elev) < 1) {
		error(COMMAND, "missing angle");
		return;
	}
	if (getinterest(sskip(sskip(s)), 0, vc, &mag) < 0)
		return;
	moveview(angle, elev, mag, vc);
}


getexposure(s)				/* get new exposure */
char  *s;
{
	double  atof(), pow(), fabs();
	char  buf[128];
	register char  *cp;
	register PNODE  *p;
	RECT  r;
	int  x, y;
	double  e;

	for (cp = s; isspace(*cp); cp++)
		;
	if (*cp == '\0') {		/* normalize to point */
		if (dev->getcur == NULL)
			return;
		(*dev->comout)("Pick point for exposure\n");
		if ((*dev->getcur)(&x, &y) == ABORT)
			return;
		r.l = r.d = 0;
		r.r = ourview.hresolu; r.u = ourview.vresolu;
		p = findrect(x, y, &ptrunk, &r, -1);
		e = 1.0;
	} else {
		if (*cp == '=') {	/* absolute setting */
			p = NULL;
			e = 1.0/exposure;
			for (cp++; isspace(*cp); cp++)
				;
			if (*cp == '\0') {	/* interactive */
				sprintf(buf, "exposure (%lf): ", exposure);
				(*dev->comout)(buf);
				(*dev->comin)(buf);
				for (cp = buf; isspace(*cp); cp++)
					;
				if (*cp == '\0')
					return;
			}
		} else {		/* normalize to average */
			p = &ptrunk;
			e = 1.0;
		}
		if (*cp == '+' || *cp == '-')	/* f-stops */
			e *= pow(2.0, atof(cp));
		else				/* multiplier */
			e *= atof(cp);
	}
	if (p != NULL) {		/* relative setting */
		if (bright(p->v) <= FTINY) {
			error(COMMAND, "cannot normalize to zero");
			return;
		}
		e *= 0.5 / bright(p->v);
	}
	if (e <= FTINY || fabs(1.0 - e) <= FTINY)
		return;
	scalepict(&ptrunk, e);
	exposure *= e;
	redraw();
}


setparam(s)				/* get/set program parameter */
register char  *s;
{
	extern int  psample;
	extern double  maxdiff;
	extern double  minweight;
	extern int  maxdepth;
	extern double  dstrsrc;
	extern double  shadthresh;
	extern double  shadcert;
	extern COLOR  ambval;
	extern double  ambacc;
	extern double  minarad;
	extern int  ambres;
	extern int  ambdiv;
	extern int  ambssamp;
	extern int  ambounce;
	int  i0;
	double  d0, d1, d2;
	char  buf[128];
	
	if (s[0] == '\0') {
		(*dev->comout)("aa ab ad ar as av dc dj dt lr lw sp st: ");
		(*dev->comin)(buf);
		s = buf;
	}
	switch (s[0]) {
	case 'l':			/* limit */
		switch (s[1]) {
		case 'w':			/* weight */
			if (sscanf(s+2, "%lf", &d0) != 1) {
				sprintf(buf, "limit weight (%.6g): ",
						minweight);
				(*dev->comout)(buf);
				(*dev->comin)(buf);
				if (sscanf(buf, "%lf", &d0) != 1)
					break;
			}
			minweight = d0;
			break;
		case 'r':			/* reflection */
			if (sscanf(s+2, "%d", &i0) != 1) {
				sprintf(buf, "limit reflection (%d): ",
						maxdepth);
				(*dev->comout)(buf);
				(*dev->comin)(buf);
				if (sscanf(buf, "%d", &i0) != 1)
					break;
			}
			maxdepth = i0;
			break;
		default:
			goto badparam;
		}
		break;
	case 'd':			/* direct */
		switch (s[1]) {
		case 'j':			/* jitter */
			if (sscanf(s+2, "%lf", &d0) != 1) {
				sprintf(buf, "direct jitter (%.6g): ",
						dstrsrc);
				(*dev->comout)(buf);
				(*dev->comin)(buf);
				if (sscanf(buf, "%lf", &d0) != 1)
					break;
			}
			dstrsrc = d0;
			break;
		case 'c':			/* certainty */
			if (sscanf(s+2, "%lf", &d0) != 1) {
				sprintf(buf, "direct certainty (%.6g): ",
						shadcert);
				(*dev->comout)(buf);
				(*dev->comin)(buf);
				if (sscanf(buf, "%lf", &d0) != 1)
					break;
			}
			shadcert = d0;
			break;
		case 't':			/* threshold */
			if (sscanf(s+2, "%lf", &d0) != 1) {
				sprintf(buf, "direct threshold (%.6g): ",
						shadthresh);
				(*dev->comout)(buf);
				(*dev->comin)(buf);
				if (sscanf(buf, "%lf", &d0) != 1)
					break;
			}
			shadthresh = d0;
			break;
		default:
			goto badparam;
		}
		break;
	case 'a':			/* ambient */
		switch (s[1]) {
		case 'v':			/* value */
			if (sscanf(s+2, "%lf %lf %lf", &d0, &d1, &d2) != 3) {
				sprintf(buf,
					"ambient value (%.6g %.6g %.6g): ",
						colval(ambval,RED),
						colval(ambval,GRN),
						colval(ambval,BLU));
				(*dev->comout)(buf);
				(*dev->comin)(buf);
				if (sscanf(buf, "%lf %lf %lf",
						&d0, &d1, &d2) != 3)
					break;
			}
			setcolor(ambval, d0, d1, d2);
			break;
		case 'a':			/* accuracy */
			if (sscanf(s+2, "%lf", &d0) != 1) {
				sprintf(buf, "ambient accuracy (%.6g): ",
						ambacc);
				(*dev->comout)(buf);
				(*dev->comin)(buf);
				if (sscanf(buf, "%lf", &d0) != 1)
					break;
			}
			ambacc = d0;
			break;
		case 'd':			/* divisions */
			if (sscanf(s+2, "%d", &i0) != 1) {
				sprintf(buf, "ambient divisions (%d): ",
						ambdiv);
				(*dev->comout)(buf);
				(*dev->comin)(buf);
				if (sscanf(buf, "%d", &i0) != 1)
					break;
			}
			ambdiv = i0;
			break;
		case 's':			/* samples */
			if (sscanf(s+2, "%d", &i0) != 1) {
				sprintf(buf, "ambient super-samples (%d): ",
						ambssamp);
				(*dev->comout)(buf);
				(*dev->comin)(buf);
				if (sscanf(buf, "%d", &i0) != 1)
					break;
			}
			ambssamp = i0;
			break;
		case 'b':			/* bounces */
			if (sscanf(s+2, "%d", &i0) != 1) {
				sprintf(buf, "ambient bounces (%d): ",
						ambounce);
				(*dev->comout)(buf);
				(*dev->comin)(buf);
				if (sscanf(buf, "%d", &i0) != 1)
					break;
			}
			ambounce = i0;
			break;
		case 'r':
			if (sscanf(s+2, "%d", &i0) != 1) {
				sprintf(buf, "ambient resolution (%d): ",
						ambres);
				(*dev->comout)(buf);
				(*dev->comin)(buf);
				if (sscanf(buf, "%d", &i0) != 1)
					break;
			}
			ambres = i0;
			minarad = ambres > 0 ? thescene.cusize/ambres : 0.0;
			break;
		default:
			goto badparam;
		}
		break;
	case 's':			/* sample */
		switch (s[1]) {
		case 'p':			/* pixel */
			if (sscanf(s+2, "%d", &i0) != 1) {
				sprintf(buf, "sample pixel (%d): ", psample);
				(*dev->comout)(buf);
				(*dev->comin)(buf);
				if (sscanf(buf, "%d", &i0) != 1)
					break;
			}
			psample = i0;
			pdepth = 0;
			break;
		case 't':			/* threshold */
			if (sscanf(s+2, "%lf", &d0) != 1) {
				sprintf(buf, "sample threshold (%.6g): ",
						maxdiff);
				(*dev->comout)(buf);
				(*dev->comin)(buf);
				if (sscanf(buf, "%lf", &d0) != 1)
					break;
			}
			maxdiff = d0;
			pdepth = 0;
			break;
		default:
			goto badparam;
		}
		break;
	case '\0':			/* nothing */
		break;
	default:;
badparam:
		sprintf(errmsg, "%s: unknown variable", s);
		error(COMMAND, errmsg);
		break;
	}
}


traceray(s)				/* trace a single ray */
char  *s;
{
	char  buf[128];
	int  x, y;
	RAY  thisray;

	if (sscanf(s, "%lf %lf %lf %lf %lf %lf",
		&thisray.rorg[0], &thisray.rorg[1], &thisray.rorg[2],
		&thisray.rdir[0], &thisray.rdir[1], &thisray.rdir[2]) != 6) {

		if (dev->getcur == NULL)
			return;
		(*dev->comout)("Pick ray\n");
		if ((*dev->getcur)(&x, &y) == ABORT)
			return;

		rayview(thisray.rorg, thisray.rdir, &ourview, x+.5, y+.5);
		
	} else if (normalize(thisray.rdir) == 0.0) {
		error(COMMAND, "zero ray direction");
		return;
	}

	rayorigin(&thisray, NULL, PRIMARY, 1.0);
	
	rayvalue(&thisray);

	if (thisray.ro == NULL)
		(*dev->comout)("ray hit nothing");
	else {
		sprintf(buf, "ray hit %s %s \"%s\"",
				objptr(thisray.ro->omod)->oname,
				ofun[thisray.ro->otype].funame,
				thisray.ro->oname);
		(*dev->comout)(buf);
		(*dev->comin)(buf);
		if (thisray.rot >= FHUGE)
			(*dev->comout)("at infinity");
		else {
			sprintf(buf, "at (%.6g %.6g %.6g)", thisray.rop[0],
					thisray.rop[1], thisray.rop[2]);
			(*dev->comout)(buf);
		}
		(*dev->comin)(buf);
		sprintf(buf, "with value (%.6g %.6g %.6g)",
				colval(thisray.rcol,RED),
				colval(thisray.rcol,GRN),
				colval(thisray.rcol,BLU));
		(*dev->comout)(buf);
	}
	(*dev->comin)(buf);
}


writepict(s)				/* write the picture to a file */
char  *s;
{
	static char  buf[128];
	char  *fname;
	FILE  *fp;
	COLR  *scanline;
	int  y;

	if (sscanf(s, "%s", buf) != 1 && buf[0] == '\0') {
		error(COMMAND, "no file");
		return;
	}
	if ((fname = getpath(buf, NULL, 0)) == NULL ||
			(fp = fopen(fname, "w")) == NULL) {
		sprintf(errmsg, "cannot open \"%s\"", buf);
		error(COMMAND, errmsg);
		return;
	}
	(*dev->comout)("writing \"");
	(*dev->comout)(fname);
	(*dev->comout)("\"...\n");
						/* write header */
	fputs(progname, fp);
	fprintview(&ourview, fp);
	putc('\n', fp);
	if (exposure != 1.0)
		fprintf(fp, "EXPOSURE=%e\n", exposure);
	putc('\n', fp);
	fputresolu(YMAJOR|YDECR, ourview.hresolu, ourview.vresolu, fp);

	scanline = (COLR *)malloc(ourview.hresolu*sizeof(COLR));
	if (scanline == NULL)
		error(SYSTEM, "out of memory in writepict");
	for (y = ourview.vresolu-1; y >= 0; y--) {
		getpictcolrs(y, scanline, &ptrunk,
				ourview.hresolu, ourview.vresolu);
		if (fwritecolrs(scanline, ourview.hresolu, fp) < 0)
			break;
	}
	if (fclose(fp) < 0)
		error(COMMAND, "write error");
	free((char *)scanline);
}
