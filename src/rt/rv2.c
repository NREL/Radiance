#ifndef lint
static const char	RCSid[] = "$Id: rv2.c,v 2.72 2021/02/12 00:41:19 greg Exp $";
#endif
/*
 *  rv2.c - command routines used in tracing a view.
 *
 *  External symbols declared in rpaint.h
 */

#include "copyright.h"

#include  <ctype.h>
#include  <string.h>

#include  "platform.h"
#include  "rtprocess.h"	/* win_popen() */
#include  "paths.h"
#include  "ray.h"
#include  "ambient.h"
#include  "otypes.h"
#include  "otspecial.h"
#include  "rpaint.h"

extern int  psample;			/* pixel sample size */
extern double  maxdiff;			/* max. sample difference */

#define  CTRL(c)	((c)-'@')

#define  sscanvec(s,v)	(sscanf(s,FVFORMAT,v,v+1,v+2)==3)

extern char  rifname[128];		/* rad input file name */

extern char  *progname;
extern char  *octname;


void
getframe(				/* get a new frame */
	char  *s
)
{
	if (getrect(s, &pframe) < 0)
		return;
	pdepth = 0;
}


void
getrepaint(				/* get area and repaint */
	char  *s
)
{
	RECT  box;

	if (getrect(s, &box) < 0)
		return;
	paintrect(&ptrunk, &box);
}


void
getview(				/* get/show/save view parameters */
	char  *s
)
{
	FILE  *fp;
	char  buf[128];
	char  *fname;
	int  change = 0;
	VIEW  nv = ourview;

	while (isspace(*s))
		s++;
	if (*s == '-') {			/* command line parameters */
		if (sscanview(&nv, s))
			newview(&nv);
		else
			error(COMMAND, "bad view option(s)");
		return;
	}
	if (nextword(buf, sizeof(buf), s) != NULL) {	/* write to a file */
		if ((fname = getpath(buf, NULL, 0)) == NULL ||
				(fp = fopen(fname, "a")) == NULL) {
			sprintf(errmsg, "cannot open \"%s\"", buf);
			error(COMMAND, errmsg);
			return;
		}
		fputs(progname, fp);
		fprintview(&ourview, fp);
		fputs(sskip(s), fp);
		putc('\n', fp);
		fclose(fp);
		return;
	}
	sprintf(buf, "view type (%c): ", ourview.type);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL('C')) return;
	if (buf[0] && buf[0] != ourview.type) {
		nv.type = buf[0];
		change++;
	}
	sprintf(buf, "view point (%.6g %.6g %.6g): ",
			ourview.vp[0], ourview.vp[1], ourview.vp[2]);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL('C')) return;
	if (sscanvec(buf, nv.vp))
		change++;
	sprintf(buf, "view direction (%.6g %.6g %.6g): ",
			ourview.vdir[0]*ourview.vdist,
			ourview.vdir[1]*ourview.vdist,
			ourview.vdir[2]*ourview.vdist);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL('C')) return;
	if (sscanvec(buf, nv.vdir)) {
		nv.vdist = 1.;
		change++;
	}
	sprintf(buf, "view up (%.6g %.6g %.6g): ",
			ourview.vup[0], ourview.vup[1], ourview.vup[2]);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL('C')) return;
	if (sscanvec(buf, nv.vup))
		change++;
	sprintf(buf, "view horiz and vert size (%.6g %.6g): ",
			ourview.horiz, ourview.vert);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL('C')) return;
	if (sscanf(buf, "%lf %lf", &nv.horiz, &nv.vert) == 2)
		change++;
	sprintf(buf, "fore and aft clipping plane (%.6g %.6g): ",
			ourview.vfore, ourview.vaft);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL('C')) return;
	if (sscanf(buf, "%lf %lf", &nv.vfore, &nv.vaft) == 2)
		change++;
	sprintf(buf, "view shift and lift (%.6g %.6g): ",
			ourview.hoff, ourview.voff);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL('C')) return;
	if (sscanf(buf, "%lf %lf", &nv.hoff, &nv.voff) == 2)
		change++;
	if (change)
		newview(&nv);
}


void
lastview(				/* return to a previous view */
	char  *s
)
{
	char  buf[128];
	char  *fname;
	int  success;
	VIEW  nv;
					/* get parameters from a file */
	if (nextword(buf, sizeof(buf), s) != NULL) {
		nv = stdview;
		if ((fname = getpath(buf, "", R_OK)) == NULL ||
				(success = viewfile(fname, &nv, NULL)) == -1) {
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
	nv = ourview;
	ourview = oldview;
	oldview = nv;
	newimage(NULL);
}


void
saveview(				/* save view to rad file */
	char  *s
)
{
	char  view[64];
	char  *fname;
	FILE  *fp;

	if (*atos(view, sizeof(view), s)) {
		if (isint(view)) {
			error(COMMAND, "cannot write view by number");
			return;
		}
		s = sskip(s);
	}
	if (nextword(rifname, sizeof(rifname), s) == NULL && !rifname[0]) {
		error(COMMAND, "no previous rad file");
		return;
	}
	if ((fname = getpath(rifname, NULL, 0)) == NULL ||
			(fp = fopen(fname, "a")) == NULL) {
		sprintf(errmsg, "cannot open \"%s\"", rifname);
		error(COMMAND, errmsg);
		return;
	}
	fputs("view= ", fp);
	fputs(view, fp);
	fprintview(&ourview, fp);
	putc('\n', fp);
	fclose(fp);
}


void
loadview(				/* load view from rad file */
	char  *s
)
{
	char  buf[512];
	char  *fname;
	FILE  *fp;
	VIEW  nv;

	strcpy(buf, "rad -n -s -V -v ");
	if (sscanf(s, "%s", buf+strlen(buf)) == 1)
		s = sskip(s);
	else
		strcat(buf, "1");
	if (nextword(rifname, sizeof(rifname), s) == NULL && !rifname[0]) {
		error(COMMAND, "no previous rad file");
		return;
	}
	if ((fname = getpath(rifname, "", R_OK)) == NULL) {
		sprintf(errmsg, "cannot access \"%s\"", rifname);
		error(COMMAND, errmsg);
		return;
	}
	sprintf(buf+strlen(buf), " %s", fname);
	if ((fp = popen(buf, "r")) == NULL) {
		error(COMMAND, "cannot run rad");
		return;
	}
	buf[0] = '\0';
	fgets(buf, sizeof(buf), fp);
	pclose(fp);
	nv = stdview;
	if (!sscanview(&nv, buf)) {
		error(COMMAND, "rad error -- no such view?");
		return;
	}
	newview(&nv);
}


void
getaim(				/* aim camera */
	char  *s
)
{
	VIEW  nv = ourview;
	double  zfact;

	if (getinterest(s, 1, nv.vdir, &zfact) < 0)
		return;
	zoomview(&nv, zfact);
	newview(&nv);
}


void
getfocus(				/* set focus distance */
	char *s
)
{
	char  buf[64];
	double	dist;

	if (sscanf(s, "%lf", &dist) < 1) {
		int	x, y;
		RAY	thisray;
		if (dev->getcur == NULL)
			return;
		(*dev->comout)("Pick focus point\n");
		if ((*dev->getcur)(&x, &y) == ABORT)
			return;
		if ((thisray.rmax = viewray(thisray.rorg, thisray.rdir,
			&ourview, (x+.5)/hresolu, (y+.5)/vresolu)) < -FTINY) {
			error(COMMAND, "not on image");
			return;
		}
		rayorigin(&thisray, PRIMARY, NULL, NULL);
		if (!localhit(&thisray, &thescene)) {
			error(COMMAND, "not a local object");
			return;
		}
		dist = thisray.rot;
	} else if (dist <= .0) {
		error(COMMAND, "focus distance must be positive");
		return;
	}
	ourview.vdist = dist;
	sprintf(buf, "Focus distance set to %f\n", dist);
	(*dev->comout)(buf);
}


void
getmove(				/* move camera */
	char  *s
)
{
	FVECT  vc;
	double  mag;

	if (getinterest(s, 0, vc, &mag) < 0)
		return;
	moveview(0.0, 0.0, mag, vc);
}


void
getrotate(				/* rotate camera */
	char  *s
)
{
	VIEW  nv = ourview;
	double  angle, elev, zfact;
	
	elev = 0.0; zfact = 1.0;
	if (sscanf(s, "%lf %lf %lf", &angle, &elev, &zfact) < 1) {
		error(COMMAND, "missing angle");
		return;
	}
	spinvector(nv.vdir, ourview.vdir, ourview.vup, angle*(PI/180.));
	if (elev != 0.0)
		geodesic(nv.vdir, nv.vdir, nv.vup, elev*(PI/180.), GEOD_RAD);
	
	zoomview(&nv, zfact);
	newview(&nv);
}


void
getpivot(				/* pivot viewpoint */
	char  *s
)
{
	FVECT  vc;
	double  angle, elev, mag;

	elev = 0.0;
	if (sscanf(s, "%lf %lf", &angle, &elev) < 1) {
		error(COMMAND, "missing angle");
		return;
	}
	if (getinterest(sskip2(s,2), 0, vc, &mag) < 0)
		return;
	moveview(angle, elev, mag, vc);
}


void
getorigin(				/* origin viewpoint */
	char  *s
)
{
	VIEW	nv = ourview;
	double	d;
					/* get new view origin */
	if (sscanf(s, "%lf %lf", &d, &d) == 1) {
					/* just moving some distance */
		VSUM(nv.vp, nv.vp, nv.vdir, d);
	} else if (!sscanvec(s, nv.vp)) {
		int	x, y;		/* need to pick origin */
		RAY	thisray;
		if (dev->getcur == NULL)
			return;
		(*dev->comout)("Pick point on surface for new origin\n");
		if ((*dev->getcur)(&x, &y) == ABORT)
			return;
		if ((thisray.rmax = viewray(thisray.rorg, thisray.rdir,
			&ourview, (x+.5)/hresolu, (y+.5)/vresolu)) < -FTINY) {
			error(COMMAND, "not on image");
			return;
		}
		rayorigin(&thisray, PRIMARY, NULL, NULL);
		if (!localhit(&thisray, &thescene)) {
			error(COMMAND, "not a local object");
			return;
		}
		if (thisray.rod < 0.0)	/* don't look through other side */
			flipsurface(&thisray);
		VSUM(nv.vp, thisray.rop, thisray.ron, 20.0*FTINY);
		VCOPY(nv.vdir, thisray.ron);
	} else if (!sscanvec(sskip2(s,3), nv.vdir) || normalize(nv.vdir) == 0.0)
		VCOPY(nv.vdir, ourview.vdir);

	d = DOT(nv.vdir, nv.vup);	/* need different up vector? */
	if (d*d >= 1.-2.*FTINY) {
		int	i;
		nv.vup[0] = nv.vup[1] = nv.vup[2] = 0.0;
		for (i = 3; i--; )
			if (nv.vdir[i]*nv.vdir[i] < 0.34)
				break;
		nv.vup[i] = 1.;
	}
	newview(&nv);
}


void
getexposure(				/* get new exposure */
	char  *s
)
{
	char  buf[128];
	char  *cp;
	int  x, y;
	PNODE  *p = &ptrunk;
	int  adapt = 0;
	double  e = 1.0;

	for (cp = s; isspace(*cp); cp++)
		;
	if (*cp == '@') {
		adapt++;
		while (isspace(*++cp))
			;
	}
	if (*cp == '\0') {		/* normalize to point */
		if (dev->getcur == NULL)
			return;
		(*dev->comout)("Pick point for exposure\n");
		if ((*dev->getcur)(&x, &y) == ABORT)
			return;
		p = findrect(x, y, &ptrunk, -1);
	} else {
		if (*cp == '=') {	/* absolute setting */
			p = NULL;
			e = 1.0/exposure;
			for (cp++; isspace(*cp); cp++)
				;
			if (*cp == '\0') {	/* interactive */
				sprintf(buf, "exposure (%f): ", exposure);
				(*dev->comout)(buf);
				(*dev->comin)(buf, NULL);
				for (cp = buf; isspace(*cp); cp++)
					;
				if (*cp == '\0')
					return;
			}
		}
		if (*cp == '+' || *cp == '-')	/* f-stops */
			e *= pow(2.0, atof(cp));
		else				/* multiplier */
			e *= atof(cp);
	}
	if (p != NULL) {		/* relative setting */
		compavg(p);
		if (bright(p->v) < 1e-15) {
			error(COMMAND, "cannot normalize to zero");
			return;
		}
		if (adapt)
			e *= 106./pow(1.219+pow(luminance(p->v)/exposure,.4),2.5)/exposure;
		else
			e *= 0.5 / bright(p->v);
	}
	if (e <= FTINY || fabs(1.0 - e) <= FTINY)
		return;
	scalepict(&ptrunk, e);
	exposure *= e;
	redraw();
}

typedef union {int i; double d; COLOR C;}	*MyUptr;

int
getparam(		/* get variable from user */
	char  *str,
	char  *dsc,
	int  typ,
	void  *p
)
{
	MyUptr  ptr = (MyUptr)p;
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
				return(0);
		}
		if (ptr->i == i0)
			return(0);
		ptr->i = i0;
		break;
	case 'r':			/* real */
		if (sscanf(str, "%lf", &d0) != 1) {
			(*dev->comout)(dsc);
			sprintf(buf, " (%.6g): ", ptr->d);
			(*dev->comout)(buf);
			(*dev->comin)(buf, NULL);
			if (sscanf(buf, "%lf", &d0) != 1)
				return(0);
		}
		if (FABSEQ(ptr->d, d0))
			return(0);
		ptr->d = d0;
		break;
	case 'b':			/* boolean */
		if (sscanf(str, "%1s", buf) != 1) {
			(*dev->comout)(dsc);
			sprintf(buf, "? (%c): ", ptr->i ? 'y' : 'n');
			(*dev->comout)(buf);
			(*dev->comin)(buf, NULL);
			if (buf[0] == '\0')
				return(0);
		}
		if (strchr("yY+1tTnN-0fF", buf[0]) == NULL)
			return(0);
		i0 = strchr("yY+1tT", buf[0]) != NULL;
		if (ptr->i == i0)
			return(0);
		ptr->i = i0;
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
				return(0);
		}
		if (FABSEQ(colval(ptr->C,RED), d0) &&
				FABSEQ(colval(ptr->C,GRN), d1) &&
				FABSEQ(colval(ptr->C,BLU), d2))
			return(0);
		setcolor(ptr->C, d0, d1, d2);
		break;
	default:
		return(0);		/* shouldn't happen */
	}
	newparam++;
	return(1);
}


void
setparam(				/* get/set program parameter */
	char  *s
)
{
	int  prev_newp = newparam;
	char  buf[128];
	
	if (s[0] == '\0') {
		(*dev->comout)(
		"aa ab ad ar as av aw b bv dc dv dj ds dt i lr lw me ma mg ms ps pt ss st u: ");
		(*dev->comin)(buf, NULL);
		s = buf;
	}
	switch (s[0]) {
	case 'u':			/* uncorrelated sampling */
		getparam(s+1, "uncorrelated sampling", 'b',
				(void *)&rand_samp);
		break;
	case 'l':			/* limit */
		switch (s[1]) {
		case 'w':			/* weight */
			getparam(s+2, "limit weight", 'r',
					(void *)&minweight);
			break;
		case 'r':			/* reflection */
			getparam(s+2, "limit reflection", 'i',
					(void *)&maxdepth);
			break;
		default:
			goto badparam;
		}
		break;
	case 'd':			/* direct */
		switch (s[1]) {
		case 'j':			/* jitter */
			getparam(s+2, "direct jitter", 'r',
					(void *)&dstrsrc);
			break;
		case 'c':			/* certainty */
			getparam(s+2, "direct certainty", 'r',
					(void *)&shadcert);
			break;
		case 't':			/* threshold */
			getparam(s+2, "direct threshold", 'r',
					(void *)&shadthresh);
			break;
		case 'v':			/* visibility */
			getparam(s+2, "direct visibility", 'b',
					(void *)&directvis);
			break;
		case 's':			/* sampling */
			getparam(s+2, "direct sampling", 'r',
					(void *)&srcsizerat);
			break;
		default:
			goto badparam;
		}
		break;
	case 'b':			/* back faces or black and white */
		switch (s[1]) {
		case 'v':			/* back face visibility */
			getparam(s+2, "back face visibility", 'b',
					(void *)&backvis);
			break;
		case '\0':			/* black and white */
		case ' ':
		case 'y': case 'Y': case 't': case 'T': case '1': case '+':
		case 'n': case 'N': case 'f': case 'F': case '0': case '-':
			getparam(s+1, "black and white", 'b',
					(void *)&greyscale);
			newparam = prev_newp;
			break;
		default:
			goto badparam;
		}
		break;
	case 'i':			/* irradiance */
		getparam(s+1, "irradiance", 'b',
				(void *)&do_irrad);
		break;
	case 'a':			/* ambient */
		switch (s[1]) {
		case 'v':			/* value */
			getparam(s+2, "ambient value", 'C',
					(void *)ambval);
			break;
		case 'w':			/* weight */
			getparam(s+2, "ambient value weight", 'i',
					(void *)&ambvwt);
			break;
		case 'a':			/* accuracy */
			if (getparam(s+2, "ambient accuracy", 'r',
					(void *)&ambacc))
				setambacc(ambacc);
			break;
		case 'd':			/* divisions */
			getparam(s+2, "ambient divisions", 'i',
					(void *)&ambdiv);
			break;
		case 's':			/* samples */
			getparam(s+2, "ambient super-samples", 'i',
					(void *)&ambssamp);
			break;
		case 'b':			/* bounces */
			getparam(s+2, "ambient bounces", 'i',
					(void *)&ambounce);
			break;
		case 'r':
			if (getparam(s+2, "ambient resolution", 'i',
					(void *)&ambres))
				setambres(ambres);
			break;
		default:
			goto badparam;
		}
		break;
	case 'm':			/* medium */
		switch (s[1]) {
		case 'e':			/* extinction coefficient */
			getparam(s+2, "extinction coefficient", 'C',
					(void *)cextinction);
			break;
		case 'a':			/* scattering albedo */
			getparam(s+2, "scattering albedo", 'C',
					(void *)salbedo);
			break;
		case 'g':			/* scattering eccentricity */
			getparam(s+2, "scattering eccentricity", 'r',
					(void *)&seccg);
			break;
		case 's':			/* sampling distance */
			getparam(s+2, "mist sampling distance", 'r',
					(void *)&ssampdist);
			break;
		default:
			goto badparam;
		}
		break;
	case 'p':			/* pixel */
		switch (s[1]) {
		case 's':			/* sample */
			if (getparam(s+2, "pixel sample", 'i',
					(void *)&psample))
				pdepth = 0;
			break;
		case 't':			/* threshold */
			if (getparam(s+2, "pixel threshold", 'r',
					(void *)&maxdiff))
				pdepth = 0;
			break;
		default:
			goto badparam;
		}
		newparam = prev_newp;
		break;
	case 's':			/* specular */
		switch (s[1]) {
		case 's':			/* sampling */
			getparam(s+2, "specular sampling", 'r',
					(void *)&specjitter);
			break;
		case 't':			/* threshold */
			getparam(s+2, "specular threshold", 'r',
					(void *)&specthresh);
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


void
traceray(				/* trace a single ray */
	char  *s
)
{
	RAY	thisray;
	char	buf[512];

	thisray.rmax = 0.0;

	if (!sscanvec(s, thisray.rorg) ||
			!sscanvec(sskip2(s,3), thisray.rdir)) {
		int  x, y;

		if (dev->getcur == NULL)
			return;
		(*dev->comout)("Pick ray\n");
		if ((*dev->getcur)(&x, &y) == ABORT)
			return;

		if ((thisray.rmax = viewray(thisray.rorg, thisray.rdir,
			&ourview, (x+.5)/hresolu, (y+.5)/vresolu)) < -FTINY) {
			error(COMMAND, "not on image");
			return;
		}

	} else if (normalize(thisray.rdir) == 0.0) {
		error(COMMAND, "zero ray direction");
		return;
	}

	ray_trace(&thisray);

	if (thisray.ro == NULL)
		(*dev->comout)("ray hit nothing");
	else {
		OBJREC	*mat = NULL;
		OBJREC	*mod = NULL;
		char	matspec[256];
		OBJREC	*ino;

		matspec[0] = '\0';
		if (thisray.ro->omod != OVOID) {
			mod = objptr(thisray.ro->omod);
			mat = findmaterial(thisray.ro);
		}
		if (thisray.rod < 0.0)
			strcpy(matspec, "back of ");
		if (mod != NULL) {
			strcat(matspec, mod->oname);
			if (mat != mod && mat != NULL)
				sprintf(matspec+strlen(matspec),
					" (%s)", mat->oname);
		} else
			strcat(matspec, VOIDID);
		sprintf(buf, "ray hit %s %s \"%s\"", matspec,
				ofun[thisray.ro->otype].funame,
				thisray.ro->oname);
		if ((ino = objptr(thisray.robj)) != thisray.ro)
			sprintf(buf+strlen(buf), " in %s \"%s\"",
					ofun[ino->otype].funame, ino->oname);
		(*dev->comout)(buf);
		(*dev->comin)(buf, NULL);
		if (thisray.rot >= FHUGE*.99)
			(*dev->comout)("at infinity");
		else {
			sprintf(buf, "at (%.6g %.6g %.6g) (%.6g)",
					thisray.rop[0], thisray.rop[1],
					thisray.rop[2], raydistance(&thisray));
			(*dev->comout)(buf);
		}
		(*dev->comin)(buf, NULL);
		sprintf(buf, "value (%.5g %.5g %.5g) (%.3gL)",
				colval(thisray.rcol,RED),
				colval(thisray.rcol,GRN),
				colval(thisray.rcol,BLU),
				luminance(thisray.rcol));
		(*dev->comout)(buf);
	}
	(*dev->comin)(buf, NULL);
}


void
writepict(				/* write the picture to a file */
	char  *s
)
{
	static char  buf[128];
	char  *fname;
	FILE  *fp;
	COLR  *scanline;
	int  y;
				/* XXX relies on words.c 2.11 behavior */
	if (nextword(buf, sizeof(buf), s) == NULL && !buf[0]) {
		error(COMMAND, "no file");
		return;
	}
	if ((fname = getpath(buf, NULL, 0)) == NULL ||
			(fp = fopen(fname, "w")) == NULL) {
		sprintf(errmsg, "cannot open \"%s\"", buf);
		error(COMMAND, errmsg);
		return;
	}
	SET_FILE_BINARY(fp);
	(*dev->comout)("writing \"");
	(*dev->comout)(fname);
	(*dev->comout)("\"...\n");
						/* write header */
	newheader("RADIANCE", fp);
	fputs(progname, fp);
	fprintview(&ourview, fp);
	if (octname != NULL)
		fprintf(fp, " %s\n", octname);
	else
		putc('\n', fp);
	fprintf(fp, "SOFTWARE= %s\n", VersionID);
	fputnow(fp);
	if (exposure != 1.0)
		fputexpos(exposure, fp);
	if (dev->pixaspect != 1.0)
		fputaspect(dev->pixaspect, fp);
	fputformat(COLRFMT, fp);
	putc('\n', fp);
	fprtresolu(hresolu, vresolu, fp);

	scanline = (COLR *)malloc(hresolu*sizeof(COLR));
	if (scanline == NULL) {
		error(COMMAND, "not enough memory!");
		fclose(fp);
		unlink(fname);
		return;
	}
	for (y = vresolu-1; y >= 0; y--) {
		getpictcolrs(y, scanline, &ptrunk, hresolu, vresolu);
		if (fwritecolrs(scanline, hresolu, fp) < 0)
			break;
	}
	free((void *)scanline);
	if (fclose(fp) < 0)
		error(COMMAND, "write error");
}
