/* Copyright (c) 1993 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Executive program for oconv, rpict and pfilt
 */

#include "standard.h"
#include "paths.h"
#include <ctype.h>


typedef struct {
	char	*name;		/* variable name */
	short	nick;		/* # characters required for nickname */
	short	nass;		/* # assignments made */
	char	*value;		/* assigned value(s) */
	int	(*fixval)();	/* assignment checking function */
} VARIABLE;

int	onevalue(), catvalues();

				/* variables */
#define OBJECT		0		/* object files */
#define SCENE		1		/* scene files */
#define MATERIAL	2		/* material files */
#define RENDER		3		/* rendering options */
#define OCONV		4		/* oconv options */
#define PFILT		5		/* pfilt options */
#define VIEW		6		/* view(s) for picture(s) */
#define ZONE		7		/* simulation zone */
#define QUALITY		8		/* desired rendering quality */
#define OCTREE		9		/* octree file name */
#define PICTURE		10		/* picture file name */
#define AMBFILE		11		/* ambient file name */
#define OPTFILE		12		/* rendering options file */
#define EXPOSURE	13		/* picture exposure setting */
#define RESOLUTION	14		/* maximum picture resolution */
#define UP		15		/* view up (X, Y or Z) */
#define INDIRECT	16		/* indirection in lighting */
#define DETAIL		17		/* level of scene detail */
#define PENUMBRAS	18		/* shadow penumbras are desired */
#define VARIABILITY	19		/* level of light variability */
#define REPORT		20		/* report frequency and errfile */
				/* total number of variables */
#define NVARS		21

VARIABLE	vv[NVARS] = {		/* variable-value pairs */
	{"objects",	3,	0,	NULL,	catvalues},
	{"scene",	3,	0,	NULL,	catvalues},
	{"materials",	3,	0,	NULL,	catvalues},
	{"render",	3,	0,	NULL,	catvalues},
	{"oconv",	3,	0,	NULL,	catvalues},
	{"pfilt",	2,	0,	NULL,	catvalues},
	{"view",	2,	0,	NULL,	NULL},
	{"ZONE",	2,	0,	NULL,	onevalue},
	{"QUALITY",	3,	0,	NULL,	onevalue},
	{"OCTREE",	3,	0,	NULL,	onevalue},
	{"PICTURE",	3,	0,	NULL,	onevalue},
	{"AMBFILE",	3,	0,	NULL,	onevalue},
	{"OPTFILE",	3,	0,	NULL,	onevalue},
	{"EXPOSURE",	3,	0,	NULL,	onevalue},
	{"RESOLUTION",	3,	0,	NULL,	onevalue},
	{"UP",		2,	0,	NULL,	onevalue},
	{"INDIRECT",	3,	0,	NULL,	onevalue},
	{"DETAIL",	3,	0,	NULL,	onevalue},
	{"PENUMBRAS",	3,	0,	NULL,	onevalue},
	{"VARIABILITY",	3,	0,	NULL,	onevalue},
	{"REPORT",	3,	0,	NULL,	onevalue},
};

VARIABLE	*matchvar();
char	*nvalue();
int	vscale();

#define vnam(vc)	(vv[vc].name)
#define vdef(vc)	(vv[vc].nass)
#define vval(vc)	(vv[vc].value)
#define vint(vc)	atoi(vval(vc))
#define vlet(vc)	(vval(vc)[0]&~0x20)
#define vbool(vc)	(vlet(vc)=='T')

#define HIGH		2
#define MEDIUM		1
#define LOW		0

int	lowqopts(), medqopts(), hiqopts();
int	(*setqopts[3])() = {lowqopts, medqopts, hiqopts};

#define renderopts	(*setqopts[vscale(QUALITY)])

extern long	fdate();

long	scenedate;		/* date of latest scene or object file */
long	octreedate;		/* date of octree */

int	explicate = 0;		/* explicate variables */
int	silent = 0;		/* do work silently */
int	noaction = 0;		/* don't do anything */
char	*rvdevice = NULL;	/* rview output device */
char	*viewselect = NULL;	/* specific view only */

int	overture = 0;		/* overture calculation needed */

char	*progname;		/* global argv[0] */

char	radname[MAXPATH];	/* root Radiance file name */


main(argc, argv)
int	argc;
char	*argv[];
{
	char	ropts[512];
	int	i;

	progname = argv[0];
				/* get options */
	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 's':
			silent++;
			break;
		case 'n':
			noaction++;
			break;
		case 'e':
			explicate++;
			break;
		case 'o':
			rvdevice = argv[++i];
			break;
		case 'v':
			viewselect = argv[++i];
			break;
		default:
			goto userr;
		}
	if (i >= argc)
		goto userr;
				/* assign Radiance root file name */
	rootname(radname, argv[i]);
				/* load variable values */
	load(argv[i]);
				/* get any additional assignments */
	for (i++; i < argc; i++)
		setvariable(argv[i]);
				/* check assignments */
	checkvalues();
				/* check files and dates */
	checkfiles();
				/* set default values as necessary */
	setdefaults();
				/* print all values if requested */
	if (explicate)
		printvals();
				/* run simulation */
	oconv();
	renderopts(ropts);
	xferopts(ropts);
	if (rvdevice != NULL)
		rview(ropts);
	else
		rpict(ropts);
	exit(0);
userr:
	fprintf(stderr,
	"Usage: %s [-s][-n][-v view][-o dev] rfile [VAR=value ..]\n",
			progname);
	exit(1);
}


rootname(rn, fn)		/* remove tail from end of fn */
register char	*rn, *fn;
{
	char	*tp, *dp;

	for (tp = NULL, dp = rn; *rn = *fn++; rn++)
		if (ISDIRSEP(*rn))
			dp = rn;
		else if (*rn == '.')
			tp = rn;
	if (tp != NULL && tp > dp)
		*tp = '\0';
}


load(rfname)			/* load Radiance simulation file */
char	*rfname;
{
	FILE	*fp;
	char	buf[256];
	register char	*cp;

	if (rfname == NULL)
		fp = stdin;
	else if ((fp = fopen(rfname, "r")) == NULL) {
		perror(rfname);
		exit(1);
	}
	while (fgetline(buf, sizeof(buf), fp) != NULL) {
		for (cp = buf; *cp; cp++) {
			switch (*cp) {
			case '\\':
			case '\n':
				*cp = ' ';
				continue;
			case '#':
				*cp = '\0';
				break;
			}
			break;
		}
		setvariable(buf);
	}
	fclose(fp);
}


setvariable(ass)		/* assign variable according to string */
register char	*ass;
{
	char	varname[32];
	register char	*cp;
	register VARIABLE	*vp;
	register int	i;
	int	n;

	while (isspace(*ass))		/* skip leading space */
		ass++;
	cp = varname;			/* extract name */
	while (cp < varname+sizeof(varname)-1
			&& *ass && !isspace(*ass) && *ass != '=')
		*cp++ = *ass++;
	*cp = '\0';
	if (!varname[0])
		return;		/* no variable name! */
					/* trim value */
	while (isspace(*ass) || *ass == '=')
		ass++;
	cp = ass + strlen(ass);
	do
		*cp-- = '\0';
	while (cp >= ass && isspace(*cp));
	n = cp - ass + 1;
	if (!n) {
		fprintf(stderr, "%s: warning - missing value for variable '%s'\n",
				progname, varname);
		return;
	}
					/* match variable from list */
	vp = matchvar(varname);
	if (vp == NULL) {
		fprintf(stderr, "%s: unknown variable '%s'\n",
				progname, varname);
		exit(1);
	}
					/* assign new value */
	cp = vp->value; i = vp->nass;
	while (i--)
		while (*cp++)
			;
	i = cp - vp->value;
	vp->value = realloc(vp->value, i+n+1);
	if (vp->value == NULL) {
		perror(progname);
		exit(1);
	}
	strcpy(vp->value+i, ass);
	vp->nass++;
}


VARIABLE *
matchvar(nam)			/* match a variable by its name */
char	*nam;
{
	int	n = strlen(nam);
	register int	i;

	for (i = 0; i < NVARS; i++)
		if (n >= vv[i].nick && !strncmp(nam, vv[i].name, n))
			return(vv+i);
	return(NULL);
}


char *
nvalue(vp, n)			/* return nth variable value */
VARIABLE	*vp;
register int	n;
{
	register char	*cp;

	if (vp == NULL || n < 0 || n >= vp->nass)
		return(NULL);
	cp = vp->value;
	while (n--)
		while (*cp++)
			;
	return(cp);
}


int
vscale(vc)			/* return scale for variable vc */
int	vc;
{
	switch(vlet(vc)) {
	case 'H':
		return(HIGH);
	case 'M':
		return(MEDIUM);
	case 'L':
		return(LOW);
	}
	fprintf(stderr, "%s: illegal value for variable '%s' (%s)\n",
			progname, vnam(vc), vval(vc));
	exit(1);
}


checkvalues()			/* check assignments */
{
	register int	i;

	for (i = 0; i < NVARS; i++)
		if (vv[i].fixval != NULL)
			(*vv[i].fixval)(vv+i);
}


onevalue(vp)			/* only one assignment for this variable */
register VARIABLE	*vp;
{
	if (vp->nass < 2)
		return;
	fprintf(stderr, "%s: warning - multiple assignment of variable '%s'\n",
			progname, vp->name);
	while (vp->nass-- > 1)
		vp->value += strlen(vp->value)+1;
}


catvalues(vp)			/* concatenate variable values */
register VARIABLE	*vp;
{
	register char	*cp;

	if (vp->nass < 2)
		return;
	for (cp = vp->value; vp->nass > 1; vp->nass--) {
		while (*cp)
			cp++;
		*cp++ = ' ';
	}
}


long
checklast(fnames)			/* check files and find most recent */
register char	*fnames;
{
	char	thisfile[MAXPATH];
	long	thisdate, lastdate = -1;
	register char	*cp;

	while (*fnames) {
		while (isspace(*fnames)) fnames++;
		cp = thisfile;
		while (*fnames && !isspace(*fnames)) *cp++ = *fnames++;
		*cp = '\0';
		if ((thisdate = fdate(thisfile)) < 0) {
			perror(thisfile);
			exit(1);
		}
		if (thisdate > lastdate)
			lastdate = thisdate;
	}
	return(lastdate);
}


checkfiles()			/* check for existence and modified times */
{
	long	objdate;

	octreedate = vdef(OCTREE) ? fdate(vval(OCTREE)) : -1;
	scenedate = -1;
	if (vdef(SCENE)) {
		scenedate = checklast(vval(SCENE));
		if (vdef(OBJECT)) {
			objdate = checklast(vval(OBJECT));
			if (objdate > scenedate)
				scenedate = objdate;
		}
	}
	if (octreedate < 0 & scenedate < 0) {
		fprintf(stderr, "%s: need scene or octree\n", progname);
		exit(1);
	}
}	


setdefaults()			/* set default values for unassigned var's */
{
	FILE	*fp;
	double	xmin, ymin, zmin, size;
	char	buf[512];
	char	*cp;

	if (!vdef(OCTREE)) {
		sprintf(buf, "%s.oct", radname);
		vval(OCTREE) = savqstr(buf);
		vdef(OCTREE)++;
	}
	if (!vdef(ZONE)) {
		if (scenedate > octreedate) {
			sprintf(buf, "getbbox -w -h %s", vval(SCENE));
			if (!silent) {
				printf("\t%s\n", buf);
				fflush(stdout);
			}
			if ((fp = popen(buf, "r")) == NULL) {
				perror("getbbox");
				exit(1);
			}
			buf[0] = 'E'; buf[1] = ' ';
			fgetline(buf+2, sizeof(buf)-2, fp);
			pclose(fp);
		} else {
			sprintf(buf, "getinfo -d < %s", vval(OCTREE));
			if ((fp = popen(buf, "r")) == NULL) {
				perror("getinfo");
				exit(1);
			}
			fscanf(fp, "%lf %lf %lf %lf",
					&xmin, &ymin, &zmin, &size);
			sprintf(buf, "E %g %g %g %g %g %g", xmin, xmin+size,
					ymin, ymin+size, zmin, zmin+size);
			pclose(fp);
		}
		vval(ZONE) = savqstr(buf);
		vdef(ZONE)++;
	}
	if (!vdef(UP)) {
		vval(UP) = "Z";
		vdef(UP)++;
	}
	if (!vdef(INDIRECT)) {
		vval(INDIRECT) = "0";
		vdef(INDIRECT)++;
	}
	if (!vdef(QUALITY)) {
		vval(QUALITY) = "L";
		vdef(QUALITY)++;
	}
	if (!vdef(RESOLUTION)) {
		vval(RESOLUTION) = "512";
		vdef(RESOLUTION)++;
	}
	if (!vdef(PICTURE)) {
		vval(PICTURE) = radname;
		vdef(PICTURE)++;
	}
	if (!vdef(AMBFILE)) {
		sprintf(buf, "%s.amb", radname);
		vval(AMBFILE) = savqstr(buf);
		vdef(AMBFILE)++;
	}
	if (!vdef(VIEW)) {
		vval(VIEW) = "X";
		vdef(VIEW)++;
	}
	if (!vdef(DETAIL)) {
		vval(DETAIL) = "M";
		vdef(DETAIL)++;
	}
	if (!vdef(PENUMBRAS)) {
		vval(PENUMBRAS) = "F";
		vdef(PENUMBRAS)++;
	}
	if (!vdef(VARIABILITY)) {
		vval(VARIABILITY) = "L";
		vdef(VARIABILITY)++;
	}
}


printvals()			/* print variable values */
{
	register int	i, j;

	for (i = 0; i < NVARS; i++)
		for (j = 0; j < vv[i].nass; j++)
			printf("%s= %s\n", vv[i].name, nvalue(vv+i, j));
	fflush(stdout);
}


oconv()				/* run oconv if necessary */
{
	char	combuf[512], ocopts[64];

	if (octreedate > scenedate)	/* check dates */
		return;
					/* build command */
	oconvopts(ocopts);
	sprintf(combuf, "oconv%s %s > %s", ocopts, vval(SCENE), vval(OCTREE));
	if (!silent) {			/* echo it */
		printf("\t%s\n", combuf);
		fflush(stdout);
	}
	if (noaction)
		return;
	if (system(combuf)) {		/* run it */
		fprintf(stderr, "%s: error generating octree\n\t%s removed\n",
				progname, vval(OCTREE));
		unlink(vval(OCTREE));
		exit(1);
	}
}


char *
addarg(op, arg)				/* add argument and advance pointer */
register char	*op, *arg;
{
	*op = ' ';
	while (*++op = *arg++)
		;
	return(op);
}


oconvopts(oo)				/* get oconv options */
char	*oo;
{
	*oo = '\0';
	if (vdef(OCONV))
		addarg(oo, vval(OCONV));
}


lowqopts(ro)				/* low quality rendering options */
char	*ro;
{
	register char	*op = ro;

	*op = '\0';
	if (vdef(RENDER))
		op = addarg(op, vval(RENDER));
}


medqopts(ro)				/* medium quality rendering options */
char	*ro;
{
	register char	*op = ro;

	*op = '\0';
	if (vdef(RENDER))
		op = addarg(op, vval(RENDER));
}


hiqopts(ro)				/* high quality rendering options */
char	*ro;
{
	register char	*op = ro;

	*op = '\0';
	if (vdef(RENDER))
		op = addarg(op, vval(RENDER));
}


xferopts(ro)				/* transfer options if indicated */
char	*ro;
{
	int	fd, n;
	
	n = strlen(ro);
	if (n < 2)
		return;
	if (vdef(OPTFILE)) {
		if ((fd = open(vval(OPTFILE), O_WRONLY|O_CREAT|O_TRUNC, 0666)) == -1) {
			perror(vval(OPTFILE));
			exit(1);
		}
		if (write(fd, ro+1, n-1) != n-1) {
			perror(vval(OPTFILE));
			exit(1);
		}
		write(fd, "\n", 1);
		close(fd);
		ro[0] = ' ';
		ro[1] = '^';
		strcpy(ro+2, vval(OPTFILE));
	}
#ifdef MSDOS
	else if (n > 50) {
		register char	*evp = bmalloc(n+6);
		if (evp == NULL) {
			perror(progname);
			exit(1);
		}
		strcpy(evp, "ROPT=");
		strcat(evp, ro);
		putenv(evp);
		strcpy(ro, " $ROPT");
	}
#endif
}


pfiltopts(po)				/* get pfilt options */
register char	*po;
{
	*po = '\0';
	if (vdef(EXPOSURE)) {
		po = addarg(po, "-1 -e");
		po = addarg(po, vval(EXPOSURE));
	}
	if (vscale(QUALITY) == HIGH)
		po = addarg(po, "-r .65");
	if (vdef(PFILT))
		po = addarg(po, vval(PFILT));
}


matchword(s1, s2)			/* match white-delimited words */
register char	*s1, *s2;
{
	while (isspace(*s1)) s1++;
	while (isspace(*s2)) s2++;
	while (*s1 && !isspace(*s1))
		if (*s1++ != *s2++)
			return(0);
	return(!*s2 || isspace(*s2));
}


char *
specview(vs)				/* get proper view spec from vs */
register char	*vs;
{
	static char	viewopts[128];
	register char	*cp;
	int	xpos, ypos, zpos, viewtype;
	int	exterior;
	double	cent[3], dim[3], mult, d;

	if (vs == NULL || *vs == '-')
		return(vs);
					/* check standard view names */
	xpos = ypos = zpos = 0;
	viewtype = 0;
	if (*vs == 'X') {
		xpos = 1; vs++;
	} else if (*vs == 'x') {
		xpos = -1; vs++;
	}
	if (*vs == 'Y') {
		ypos = 1; vs++;
	} else if (*vs == 'y') {
		ypos = -1; vs++;
	}
	if (*vs == 'Z') {
		zpos = 1; vs++;
	} else if (*vs == 'z') {
		zpos = -1; vs++;
	}
	if (*vs == 'v' | *vs == 'l' | *vs == 'a' | *vs == 'h')
		viewtype = *vs++;
	else if (!*vs || isspace(*vs))
		viewtype = 'v';
	cp = viewopts;
	if (viewtype && (xpos|ypos|zpos)) {	/* got standard view */
		*cp++ = '-'; *cp++ = 'v'; *cp++ = 't'; *cp++ = viewtype;
		if (sscanf(vval(ZONE), "%*s %lf %lf %lf %lf %lf %lf",
				&cent[0], &dim[0], &cent[1], &dim[1],
				&cent[2], &dim[2]) != 6) {
			fprintf(stderr, "%s: bad zone specification\n",
					progname);
			exit(1);
		}
		dim[0] -= cent[0];
		dim[1] -= cent[1];
		dim[2] -= cent[2];
		exterior = vlet(ZONE) == 'E';
		mult = exterior ? 2. : .45 ;
		sprintf(cp, " -vp %.2g %.2g %.2g -vd %.2g %.2g %.2g",
				cent[0]+xpos*mult*dim[0],
				cent[1]+ypos*mult*dim[1],
				cent[2]+zpos*mult*dim[2],
				-xpos*dim[0], -ypos*dim[1], -zpos*dim[2]);
		cp += strlen(cp);
		switch (vlet(UP)) {
		case 'Z':
			if (xpos|ypos) {
				cp = addarg(cp, "-vu 0 0 1");
				break;
			}
		/* fall through */
		case 'Y':
			if (xpos|zpos) {
				cp = addarg(cp, "-vu 0 1 0");
				break;
			}
		/* fall through */
		case 'X':
			if (ypos|zpos)
				cp = addarg(cp, "-vu 1 0 0");
			else
				cp = addarg(cp, "-vu 0 0 1");
			break;
		default:
			fprintf(stderr, "%s: illegal value for variable '%s'\n",
					progname, vnam(UP));
			exit(1);
		}
		switch (viewtype) {
		case 'v':
			cp = addarg(cp, "-vh 45 -vv 45");
			break;
		case 'l':
			d = sqrt(dim[0]*dim[0]+dim[1]*dim[1]+dim[2]*dim[2]);
			sprintf(cp, " -vh %.2g -vv %.2g", d, d);
			cp += strlen(cp);
			break;
		case 'a':
		case 'h':
			cp = addarg(cp, "-vh 180 -vv 180");
			break;
		}
	} else
		while (*vs && !isspace(*vs))	/* else skip id */
			vs++;
					/* append any additional options */
	while (isspace(*vs)) vs++;
	strcpy(cp, vs);
	return(viewopts);
}


char *
getview(n, vn)				/* get view n, or NULL if none */
int	n;
char	*vn;
{
	register char	*mv;

	if (viewselect != NULL) {
		if (n)				/* only do one */
			return(NULL);
		if (viewselect[0] == '-') {	/* already specified */
			if (vn != NULL) *vn = '\0';
			return(viewselect);
		}
		if (vn != NULL) {
			for (mv = viewselect; *mv && !isspace(*mv);
					*vn++ = *mv++)
				;
			*vn = '\0';
		}
						/* check list */
		while ((mv = nvalue(vv+VIEW, n++)) != NULL)
			if (matchword(viewselect, mv))
				return(specview(mv));
		return(specview(viewselect));	/* standard view? */
	}
	if (vn != NULL && (mv = nvalue(vv+VIEW, n)) != NULL) {
		if (*mv != '-')
			while (*mv && !isspace(*mv))
				*vn++ = *mv++;
		*vn = '\0';
	}
	return(specview(nvalue(vv+VIEW, n)));	/* use view n */
}


rview(opts)				/* run rview with first view */
char	*opts;
{
	char	combuf[512];
					/* build command */
	sprintf(combuf, "rview %s%s ", getview(0, NULL), opts);
	if (rvdevice != NULL)
		sprintf(combuf+strlen(combuf), "-o %s ", rvdevice);
	strcat(combuf, vval(OCTREE));
	if (!silent) {			/* echo it */
		printf("\t%s\n", combuf);
		fflush(stdout);
	}
	if (noaction)
		return;
	if (system(combuf)) {		/* run it */
		fprintf(stderr, "%s: error running rview\n", progname);
		exit(1);
	}
}


rpict(opts)				/* run rpict and pfilt for each view */
char	*opts;
{
	char	combuf[512];
	char	rawfile[MAXPATH], picfile[MAXPATH], rep[MAXPATH], res[32];
	char	pfopts[64];
	char	vs[32], *vw;
	int	vn, mult;
					/* get pfilt options */
	pfiltopts(pfopts);
					/* get resolution, reporting */
	mult = vscale(QUALITY)+1;
	{
		int	xres, yres;
		double	aspect;
		int	n;
		n = sscanf(vval(RESOLUTION), "%d %d %lf", &xres, &yres, &aspect);
		if (n == 3)
			sprintf(res, "-x %d -y %d -pa %.3f",
					mult*xres, mult*yres, aspect);
		else if (n) {
			if (n == 1) yres = xres;
			sprintf(res, "-x %d -y %d", mult*xres, mult*yres);
		} else {
			fprintf(stderr, "%s: bad value for variable '%s'\n",
					progname, vnam(RESOLUTION));
			exit(1);
		}
	}
	rep[0] = '\0';
	if (vdef(REPORT)) {
		double	minutes;
		int	n;
		n = sscanf(vval(REPORT), "%lf %s", &minutes, rawfile);
		if (n == 2)
			sprintf(rep, " -t %d -e %s", (int)(minutes*60), rawfile);
		else if (n == 1)
			sprintf(rep, " -t %d", (int)(minutes*60));
		else {
			fprintf(stderr, "%s: bad value for variable '%s'\n",
					progname, vnam(REPORT));
			exit(1);
		}
	}
					/* do each view */
	vn = 0;
	while ((vw = getview(vn++, vs)) != NULL) {
		if (!vs[0])
			sprintf(vs, "%d", vn);
		sprintf(picfile, "%s_%s.pic", vval(PICTURE), vs);
						/* check date on picture */
		if (fdate(picfile) > octreedate)
			continue;
						/* build rpict command */
		sprintf(rawfile, "%s_%s.raw", vval(PICTURE), vs);
		if (fdate(rawfile) > octreedate)	/* recover */
			sprintf(combuf, "rpict%s%s -ro %s %s",
					rep, opts, rawfile, vval(OCTREE));
		else {
			if (overture) {		/* run overture calculation */
				sprintf(combuf,
				"rpict%s %s%s -x 64 -y 64 -ps 1 %s > %s",
						rep, vw, opts,
						vval(OCTREE), rawfile);
				if (!silent) {
					printf("\t%s\n", combuf);
					fflush(stdout);
				}
				if (!noaction && system(combuf)) {
					fprintf(stderr,
			"%s: error in overture for view %s\n\t%s removed\n",
						progname, vs, rawfile);
					unlink(rawfile);
					exit(1);
				}
			}
			sprintf(combuf, "rpict%s %s %s%s %s > %s",
					rep, vw, res, opts,
					vval(OCTREE), rawfile);
		}
		if (!silent) {			/* echo rpict command */
			printf("\t%s\n", combuf);
			fflush(stdout);
		}
		if (!noaction && system(combuf)) {	/* run rpict */
			fprintf(stderr, "%s: error rendering view %s\n",
					progname, vs);
			exit(1);
		}
						/* build pfilt command */
		if (mult > 1)
			sprintf(combuf, "pfilt%s -x /%d -y /%d %s > %s",
					pfopts, mult, mult, rawfile, picfile);
		else
			sprintf(combuf, "pfilt%s %s > %s", pfopts,
					rawfile, picfile);
		if (!silent) {			/* echo pfilt command */
			printf("\t%s\n", combuf);
			fflush(stdout);
		}
		if (!noaction && system(combuf)) {	/* run pfilt */
			fprintf(stderr,
			"%s: error filtering view %s\n\t%s removed\n",
					progname, vs, picfile);
			unlink(picfile);
			exit(1);
		}
						/* remove raw file */
		if (!silent)
#ifdef MSDOS
			printf("\tdel %s\n", rawfile);
#else
			printf("\trm %s\n", rawfile);
#endif
		if (!noaction)
			unlink(rawfile);
	}
}
