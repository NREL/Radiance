/* Copyright (c) 1995 Regents of the University of California */

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

#include  "resolu.h"

#include  <ctype.h>

#define  CTRL(c)	((c)-'@')

#ifdef  SMLFLT
#define  sscanvec(s,v)	(sscanf(s,"%f %f %f",v,v+1,v+2)==3)
#else
#define  sscanvec(s,v)	(sscanf(s,"%lf %lf %lf",v,v+1,v+2)==3)
#endif

extern char  *atos();

extern FILE  *popen();

extern char  rifname[128];		/* rad input file name */

extern char  VersionID[];
extern char  *progname;
extern char  *octname;


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

	while (isspace(*s))
		s++;
	if (*s == '-') {			/* command line parameters */
		copystruct(&nv, &ourview);
		if (sscanview(&nv, s))
			newview(&nv);
		else
			error(COMMAND, "bad view option(s)");
		return;
	}
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
	} else
		nv.type = ourview.type;
	sprintf(buf, "view point (%.6g %.6g %.6g): ",
			ourview.vp[0], ourview.vp[1], ourview.vp[2]);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL('C')) return;
	if (sscanvec(buf, nv.vp))
		change++;
	else
		VCOPY(nv.vp, ourview.vp);
	sprintf(buf, "view direction (%.6g %.6g %.6g): ",
			ourview.vdir[0], ourview.vdir[1], ourview.vdir[2]);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL('C')) return;
	if (sscanvec(buf, nv.vdir))
		change++;
	else
		VCOPY(nv.vdir, ourview.vdir);
	sprintf(buf, "view up (%.6g %.6g %.6g): ",
			ourview.vup[0], ourview.vup[1], ourview.vup[2]);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL('C')) return;
	if (sscanvec(buf, nv.vup))
		change++;
	else
		VCOPY(nv.vup, ourview.vup);
	sprintf(buf, "view horiz and vert size (%.6g %.6g): ",
			ourview.horiz, ourview.vert);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL('C')) return;
	if (sscanf(buf, "%lf %lf", &nv.horiz, &nv.vert) == 2)
		change++;
	else {
		nv.horiz = ourview.horiz; nv.vert = ourview.vert;
	}
	sprintf(buf, "fore and aft clipping plane (%.6g %.6g): ",
			ourview.vfore, ourview.vaft);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL('C')) return;
	if (sscanf(buf, "%lf %lf", &nv.vfore, &nv.vaft) == 2)
		change++;
	else {
		nv.vfore = ourview.vfore; nv.vaft = ourview.vaft;
	}
	sprintf(buf, "view shift and lift (%.6g %.6g): ",
			ourview.hoff, ourview.voff);
	(*dev->comout)(buf);
	(*dev->comin)(buf, NULL);
	if (buf[0] == CTRL('C')) return;
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
	copystruct(&nv, &ourview);
	copystruct(&ourview, &oldview);
	copystruct(&oldview, &nv);
	newimage();
}


saveview(s)				/* save view to rad file */
char  *s;
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
	while (isspace(*s))
		s++;
	if (*s)
		atos(rifname, sizeof(rifname), s);
	else if (rifname[0] == '\0') {
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


loadview(s)				/* load view from rad file */
char  *s;
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
	if (*s)
		atos(rifname, sizeof(rifname), s);
	else if (rifname[0] == '\0') {
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
	copystruct(&nv, &stdview);
	if (!sscanview(&nv, buf)) {
		error(COMMAND, "rad error -- no such view?");
		return;
	}
	newview(&nv);
}


getaim(s)				/* aim camera */
char  *s;
{
	double  zfact;
	VIEW  nv;

	if (getinterest(s, 1, nv.vdir, &zfact) < 0)
		return;
	nv.type = ourview.type;
	VCOPY(nv.vp, ourview.vp);
	VCOPY(nv.vup, ourview.vup);
	nv.horiz = ourview.horiz; nv.vert = ourview.vert;
	nv.vfore = ourview.vfore; nv.vaft = ourview.vaft;
	nv.hoff = ourview.hoff; nv.voff = ourview.voff;
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
	nv.vfore = ourview.vfore; nv.vaft = ourview.vaft;
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
	if (getinterest(sskip2(s,2), 0, vc, &mag) < 0)
		return;
	moveview(angle, elev, mag, vc);
}


getexposure(s)				/* get new exposure */
char  *s;
{
	char  buf[128];
	register char  *cp;
	RECT  r;
	int  x, y;
	register PNODE  *p = &ptrunk;
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
		r.l = r.d = 0;
		r.r = hresolu; r.u = vresolu;
		p = findrect(x, y, &ptrunk, &r, -1);
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
				return(0);
		}
		ptr->i = i0;
		return(1);
	case 'r':			/* real */
		if (sscanf(str, "%lf", &d0) != 1) {
			(*dev->comout)(dsc);
			sprintf(buf, " (%.6g): ", ptr->d);
			(*dev->comout)(buf);
			(*dev->comin)(buf, NULL);
			if (sscanf(buf, "%lf", &d0) != 1)
				return(0);
		}
		ptr->d = d0;
		return(1);
	case 'b':			/* boolean */
		if (sscanf(str, "%1s", buf) != 1) {
			(*dev->comout)(dsc);
			sprintf(buf, "? (%c): ", ptr->i ? 'y' : 'n');
			(*dev->comout)(buf);
			(*dev->comin)(buf, NULL);
			if (buf[0] == '\0' ||
					index("yY+1tTnN-0fF", buf[0]) == NULL)
				return(0);
		}
		ptr->i = index("yY+1tT", buf[0]) != NULL;
		return(1);
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
		setcolor(ptr->C, d0, d1, d2);
		return(1);
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
	extern int  ambvwt;
	extern double  ambacc;
	extern int  ambres;
	extern int  ambdiv;
	extern int  ambssamp;
	extern int  ambounce;
	extern COLOR  cextinction;
	extern COLOR  salbedo;
	extern double  seccg;
	extern double  ssampdist;
	extern int  directvis;
	extern double  srcsizerat;
	extern int  do_irrad;
	extern double  specjitter;
	extern double  specthresh;
	extern int  backvis;
	char  buf[128];
	
	if (s[0] == '\0') {
		(*dev->comout)(
		"aa ab ad ar as av aw b dc dv dj ds dt i lr lw me ma mg ms ps pt sj st bv: ");
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
		case 'v':			/* visibility */
			getparam(s+2, "direct visibility",
					'b', &directvis);
			break;
		case 's':			/* sampling */
			getparam(s+2, "direct sampling", 'r', &srcsizerat);
			break;
		default:
			goto badparam;
		}
		break;
	case 'b':			/* back faces or black and white */
		switch (s[1]) {
		case 'v':			/* back face visibility */
			getparam(s+2, "back face visibility", 'b', &backvis);
			break;
		case '\0':			/* black and white */
		case ' ':
		case 'y': case 'Y': case 't': case 'T': case '1': case '+':
		case 'n': case 'N': case 'f': case 'F': case '0': case '-':
			getparam(s+1, "black and white", 'b', &greyscale);
			break;
		default:
			goto badparam;
		}
		break;
	case 'i':			/* irradiance */
		getparam(s+1, "irradiance", 'b', &do_irrad);
		break;
	case 'a':			/* ambient */
		switch (s[1]) {
		case 'v':			/* value */
			getparam(s+2, "ambient value", 'C', (COLOR *)ambval);
			break;
		case 'w':			/* weight */
			getparam(s+2, "ambient value weight", 'i', &ambvwt);
			break;
		case 'a':			/* accuracy */
			if (getparam(s+2, "ambient accuracy", 'r', &ambacc))
				setambacc(ambacc);
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
			if (getparam(s+2, "ambient resolution", 'i', &ambres))
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
					(COLOR *)cextinction);
			break;
		case 'a':			/* scattering albedo */
			getparam(s+2, "scattering albedo", 'C',
					(COLOR *)salbedo);
			break;
		case 'g':			/* scattering eccentricity */
			getparam(s+2, "scattering eccentricity", 'r', &seccg);
			break;
		case 's':			/* sampling distance */
			getparam(s+2, "mist sampling distance", 'r',
					&ssampdist);
			break;
		default:
			goto badparam;
		}
		break;
	case 'p':			/* pixel */
		switch (s[1]) {
		case 's':			/* sample */
			if (getparam(s+2, "pixel sample", 'i', &psample))
				pdepth = 0;
			break;
		case 't':			/* threshold */
			if (getparam(s+2, "pixel threshold", 'r', &maxdiff))
				pdepth = 0;
			break;
		default:
			goto badparam;
		}
		break;
	case 's':			/* specular */
		switch (s[1]) {
		case 'j':			/* jitter */
			getparam(s+2, "specular jitter", 'r', &specjitter);
			break;
		case 't':			/* threshold */
			getparam(s+2, "specular threshold", 'r', &specthresh);
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
	OBJREC	*inst;
	RAY  thisray;

	thisray.rmax = 0.0;

	if (!sscanvec(s, thisray.rorg) ||
			!sscanvec(sskip2(s,3), thisray.rdir)) {

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

	rayorigin(&thisray, NULL, PRIMARY, 1.0);
	
	rayvalue(&thisray);

	if (thisray.ro == NULL)
		(*dev->comout)("ray hit nothing");
	else {
		sprintf(buf, "ray hit %s%s %s \"%s\"",
				thisray.rod < 0.0 ? "back of " : "",
				thisray.ro->omod == OVOID ? VOIDID :
					objptr(thisray.ro->omod)->oname,
				ofun[thisray.ro->otype].funame,
				thisray.ro->oname);
		if ((inst = objptr(thisray.robj)) != thisray.ro)
			sprintf(buf+strlen(buf), " in instance \"%s\"",
					inst->oname);
		(*dev->comout)(buf);
		(*dev->comin)(buf, NULL);
		if (thisray.rot >= FHUGE)
			(*dev->comout)("at infinity");
		else {
			sprintf(buf, "at (%.6g %.6g %.6g) (%.6g)",
					thisray.rop[0], thisray.rop[1],
					thisray.rop[2], thisray.rt);
			(*dev->comout)(buf);
		}
		(*dev->comin)(buf, NULL);
		sprintf(buf, "value (%.5g %.5g %.5g) (%.1fL)",
				colval(thisray.rcol,RED),
				colval(thisray.rcol,GRN),
				colval(thisray.rcol,BLU),
				luminance(thisray.rcol));
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

	while (isspace(*s))
		s++;
	if (*s)
		atos(buf, sizeof(buf), s);
	else if (buf[0] == '\0') {
		error(COMMAND, "no file");
		return;
	}
	if ((fname = getpath(buf, NULL, 0)) == NULL ||
			(fp = fopen(fname, "w")) == NULL) {
		sprintf(errmsg, "cannot open \"%s\"", buf);
		error(COMMAND, errmsg);
		return;
	}
#ifdef MSDOS
	setmode(fileno(fp), O_BINARY);
#endif
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
	free((char *)scanline);
	if (fclose(fp) < 0)
		error(COMMAND, "write error");
}
