/* Copyright (c) 1991 Regents of the University of California */

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
	if (getrect(s, &pframe) < 0)
		return;
	pdepth = 0;
}


getrepaint(s)				/* get area and repaint */
char  *s;
{
	RECT  box;

	if (getrect(s, &box) < 0)
		return;
	paintrect(&ptrunk, 0, 0, hresolu, vresolu, &box);
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
		fputs(sskip(s), fp);
		fputs("\n", fp);
		fclose(fp);
		return;
	}
	sprintf(buf, "view type (%c): ", ourview.type);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL(C)) return;
	if (buf[0] && buf[0] != ourview.type) {
		nv.type = buf[0];
		change++;
	} else
		nv.type = ourview.type;
	sprintf(buf, "view point (%.6g %.6g %.6g): ",
			ourview.vp[0], ourview.vp[1], ourview.vp[2]);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL(C)) return;
	if (sscanf(buf, "%lf %lf %lf", &nv.vp[0], &nv.vp[1], &nv.vp[2]) == 3)
		change++;
	else
		VCOPY(nv.vp, ourview.vp);
	sprintf(buf, "view direction (%.6g %.6g %.6g): ",
			ourview.vdir[0], ourview.vdir[1], ourview.vdir[2]);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL(C)) return;
	if (sscanf(buf,"%lf %lf %lf",&nv.vdir[0],&nv.vdir[1],&nv.vdir[2]) == 3)
		change++;
	else
		VCOPY(nv.vdir, ourview.vdir);
	sprintf(buf, "view up (%.6g %.6g %.6g): ",
			ourview.vup[0], ourview.vup[1], ourview.vup[2]);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL(C)) return;
	if (sscanf(buf,"%lf %lf %lf",&nv.vup[0],&nv.vup[1],&nv.vup[2]) == 3)
		change++;
	else
		VCOPY(nv.vup, ourview.vup);
	sprintf(buf, "view horiz and vert size (%.6g %.6g): ",
			ourview.horiz, ourview.vert);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL(C)) return;
	if (sscanf(buf, "%lf %lf", &nv.horiz, &nv.vert) == 2)
		change++;
	else {
		nv.horiz = ourview.horiz; nv.vert = ourview.vert;
	}
	sprintf(buf, "view shift and lift (%.6g %.6g): ",
			ourview.hoff, ourview.voff);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL(C)) return;
	if (sscanf(buf, "%lf %lf", &nv.hoff, &nv.voff) == 2)
		change++;
	else {
		nv.hoff = ourview.hoff; nv.voff = ourview.voff;
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
		copystruct(&nv, &stdview);
		if ((fname = getpath(buf, NULL, 0)) == NULL ||
				(success = viewfile(fname, &nv, 0, 0)) == -1) {
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
	if (oldview.type == 0) {	/* no old view! */
		error(COMMAND, "no previous view");
		return;
	}
	copystruct(&nv, &ourview);
	copystruct(&ourview, &oldview);
	copystruct(&oldview, &nv);
	newimage();
}


getaim(s)				/* aim camera */
char  *s;
{
	extern double  tan(), atan();
	double  zfact;
	VIEW  nv;

	if (getinterest(s, 1, nv.vdir, &zfact) < 0)
		return;
	nv.type = ourview.type;
	VCOPY(nv.vp, ourview.vp);
	VCOPY(nv.vup, ourview.vup);
	nv.hoff = ourview.hoff; nv.voff = ourview.voff;
	nv.horiz = ourview.horiz; nv.vert = ourview.vert;
	zoomview(&nv, zfact);
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
	nv.type = ourview.type;
	VCOPY(nv.vp, ourview.vp);
	VCOPY(nv.vup, ourview.vup);
	nv.hoff = ourview.hoff; nv.voff = ourview.voff;
	spinvector(nv.vdir, ourview.vdir, ourview.vup, angle*(PI/180.));
	if (elev != 0.0) {
		fcross(v1, nv.vdir, ourview.vup);
		normalize(v1);
		spinvector(nv.vdir, nv.vdir, v1, elev*(PI/180.));
	}
	nv.horiz = ourview.horiz; nv.vert = ourview.vert;
	zoomview(&nv, zfact);
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
		r.r = hresolu; r.u = vresolu;
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
				(*dev->comin)(buf, NULL);
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


getparam(str, dsc, typ, ptr)		/* get variable from user */
char  *str, *dsc;
int  typ;
register union {int i; double d; COLOR C;}  *ptr;
{
	extern char  *index();
	int  i0;
	double  d0, d1, d2;
	char  buf[48];

	switch (typ) {
	case 'i':			/* integer */
		if (sscanf(str, "%d", &i0) != 1) {
			(*dev->comout)(dsc);
			sprintf(buf, " (%d): ", ptr->i);
			(*dev->comout)(buf);
			(*dev->comin)(buf, NULL);
			if (sscanf(buf, "%d", &i0) != 1)
				break;
		}
		ptr->i = i0;
		break;
	case 'r':			/* real */
		if (sscanf(str, "%lf", &d0) != 1) {
			(*dev->comout)(dsc);
			sprintf(buf, " (%.6g): ", ptr->d);
			(*dev->comout)(buf);
			(*dev->comin)(buf, NULL);
			if (sscanf(buf, "%lf", &d0) != 1)
				break;
		}
		ptr->d = d0;
		break;
	case 'b':			/* boolean */
		if (sscanf(str, "%1s", buf) != 1) {
			(*dev->comout)(dsc);
			sprintf(buf, " (%c): ", ptr->i ? 'y' : 'n');
			(*dev->comout)(buf);
			(*dev->comin)(buf, NULL);
			if (buf[0] == '\0' ||
					index("yY+1tTnN-0fF", buf[0]) == NULL)
				break;
		}
		ptr->i = index("yY+1tT", buf[0]) != NULL;
		break;
	case 'C':			/* color */
		if (sscanf(str, "%lf %lf %lf", &d0, &d1, &d2) != 3) {
			(*dev->comout)(dsc);
			sprintf(buf, " (%.6g %.6g %.6g): ",
					colval(ptr->C,RED),
					colval(ptr->C,GRN),
					colval(ptr->C,BLU));
			(*dev->comout)(buf);
			(*dev->comin)(buf, NULL);
			if (sscanf(buf, "%lf %lf %lf", &d0, &d1, &d2) != 3)
				break;
		}
		setcolor(ptr->C, d0, d1, d2);
		break;
	}
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
	extern int  directinvis;
	extern int  do_irrad;
	char  buf[128];
	
	if (s[0] == '\0') {
		(*dev->comout)(
			"aa ab ad ar as av b dc di dj dt i lr lw sp st: ");
		(*dev->comin)(buf, NULL);
		s = buf;
	}
	switch (s[0]) {
	case 'l':			/* limit */
		switch (s[1]) {
		case 'w':			/* weight */
			getparam(s+2, "limit weight", 'r', &minweight);
			break;
		case 'r':			/* reflection */
			getparam(s+2, "limit reflection", 'i', &maxdepth);
			break;
		default:
			goto badparam;
		}
		break;
	case 'd':			/* direct */
		switch (s[1]) {
		case 'j':			/* jitter */
			getparam(s+2, "direct jitter", 'r', &dstrsrc);
			break;
		case 'c':			/* certainty */
			getparam(s+2, "direct certainty", 'r', &shadcert);
			break;
		case 't':			/* threshold */
			getparam(s+2, "direct threshold", 'r', &shadthresh);
			break;
		case 'i':			/* invisibility */
			getparam(s+2, "direct invisibility",
					'b', &directinvis);
			break;
		default:
			goto badparam;
		}
		break;
	case 'b':			/* black and white */
		getparam(s+1, "black and white", 'b', &greyscale);
		break;
	case 'i':			/* irradiance */
		getparam(s+1, "irradiance", 'b', &do_irrad);
		break;
	case 'a':			/* ambient */
		switch (s[1]) {
		case 'v':			/* value */
			getparam(s+2, "ambient value", 'C', ambval);
			break;
		case 'a':			/* accuracy */
			getparam(s+2, "ambient accuracy", 'r', &ambacc);
			break;
		case 'd':			/* divisions */
			getparam(s+2, "ambient divisions", 'i', &ambdiv);
			break;
		case 's':			/* samples */
			getparam(s+2, "ambient super-samples", 'i', &ambssamp);
			break;
		case 'b':			/* bounces */
			getparam(s+2, "ambient bounces", 'i', &ambounce);
			break;
		case 'r':
			getparam(s+2, "ambient resolution", 'i', &ambres);
			minarad = ambres > 0 ? thescene.cusize/ambres : 0.0;
			break;
		default:
			goto badparam;
		}
		break;
	case 's':			/* sample */
		switch (s[1]) {
		case 'p':			/* pixel */
			getparam(s+2, "sample pixel", 'i', &psample);
			pdepth = 0;
			break;
		case 't':			/* threshold */
			getparam(s+2, "sample threshold", 'r', &maxdiff);
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
		*sskip(s) = '\0';
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

		if (viewray(thisray.rorg, thisray.rdir, &ourview,
				(x+.5)/hresolu, (y+.5)/vresolu) < 0) {
			error(COMMAND, "not on image");
			return;
		}

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
		(*dev->comin)(buf, NULL);
		if (thisray.rot >= FHUGE)
			(*dev->comout)("at infinity");
		else {
			sprintf(buf, "at (%.6g %.6g %.6g)", thisray.rop[0],
					thisray.rop[1], thisray.rop[2]);
			(*dev->comout)(buf);
		}
		(*dev->comin)(buf, NULL);
		sprintf(buf, "with value (%.6g %.6g %.6g)",
				colval(thisray.rcol,RED),
				colval(thisray.rcol,GRN),
				colval(thisray.rcol,BLU));
		(*dev->comout)(buf);
	}
	(*dev->comin)(buf, NULL);
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
		fputexpos(exposure, fp);
	if (dev->pixaspect != 1.0)
		fputaspect(dev->pixaspect, fp);
	fputformat(COLRFMT, fp);
	putc('\n', fp);
	fputresolu(YMAJOR|YDECR, hresolu, vresolu, fp);

	scanline = (COLR *)malloc(hresolu*sizeof(COLR));
	if (scanline == NULL)
		error(SYSTEM, "out of memory in writepict");
	for (y = vresolu-1; y >= 0; y--) {
		getpictcolrs(y, scanline, &ptrunk, hresolu, vresolu);
		if (fwritecolrs(scanline, hresolu, fp) < 0)
			break;
	}
	if (fclose(fp) < 0)
		error(COMMAND, "write error");
	free((char *)scanline);
}
