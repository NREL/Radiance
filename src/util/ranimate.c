/* Copyright (c) 1995 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Radiance animation control program
 */

#include "standard.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "view.h"
#include "vars.h"
				/* input variables */
#define HOST		0		/* rendering host machine */
#define RENDER		1		/* rendering options */
#define PFILT		2		/* pfilt options */
#define PINTERP		3		/* pinterp options */
#define OCTREE		4		/* octree file name */
#define DIRECTORY	5		/* working (sub)directory */
#define BASENAME	6		/* output image base name */
#define VIEWFILE	7		/* animation frame views */
#define START		8		/* starting frame number */
#define END		9		/* ending frame number */
#define RIF		10		/* rad input file */
#define NEXTANIM	11		/* next animation file */
#define ANIMATE		12		/* animation command */
#define TRANSFER	13		/* frame transfer command */
#define ARCHIVE		14		/* archiving command */
#define INTERP		15		/* # frames to interpolate */
#define OVERSAMP	16		/* # times to oversample image */
#define MBLUR		17		/* samples for motion blur */
#define RTRACE		18		/* use rtrace with pinterp? */
#define DISKSPACE	19		/* how much disk space to use */
#define RESOLUTION	20		/* desired final resolution */
#define EXPOSURE	21		/* how to compute exposure */

int	NVARS = 22;		/* total number of variables */

VARIABLE	vv[] = {		/* variable-value pairs */
	{"host",	4,	0,	NULL,	NULL},
	{"render",	3,	0,	NULL,	catvalues},
	{"pfilt",	2,	0,	NULL,	catvalues},
	{"pinterp",	2,	0,	NULL,	catvalues},
	{"OCTREE",	3,	0,	NULL,	onevalue},
	{"DIRECTORY",	3,	0,	NULL,	onevalue},
	{"BASENAME",	3,	0,	NULL,	onevalue},
	{"VIEWFILE",	2,	0,	NULL,	onevalue},
	{"START",	2,	0,	NULL,	intvalue},
	{"END",		2,	0,	NULL,	intvalue},
	{"RIF",		3,	0,	NULL,	onevalue},
	{"NEXTANIM",	3,	0,	NULL,	onevalue},
	{"ANIMATE",	2,	0,	NULL,	onevalue},
	{"TRANSFER",	2,	0,	NULL,	onevalue},
	{"ARCHIVE",	2,	0,	NULL,	onevalue},
	{"INTERP",	3,	0,	NULL,	intvalue},
	{"OVERSAMP",	2,	0,	NULL,	fltvalue},
	{"MBLUR",	2,	0,	NULL,	onevalue},
	{"RTRACE",	2,	0,	NULL,	boolvalue},
	{"DISKSPACE",	3,	0,	NULL,	fltvalue},
	{"RESOLUTION",	3,	0,	NULL,	onevalue},
	{"EXPOSURE",	3,	0,	NULL,	onevalue},
};

#define SFNAME	"STATUS"		/* status file name */

struct {
	char	host[64];		/* control host name */
	int	pid;			/* control process id */
	char	cfname[128];		/* control file name */
	int	rnext;			/* next frame to render */
	int	fnext;			/* next frame to filter */
	int	tnext;			/* next frame to transfer */
}	astat;			/* animation status */

char	*progname;		/* our program name */
char	*cfname;		/* our control file name */

int	nowarn = 0;		/* turn warnings off? */
int	silent = 0;		/* silent mode? */
int	noaction = 0;		/* take no action? */

char	rendopt[2048] = "";	/* rendering options */
char	rresopt[32];		/* rendering resolution options */
char	fresopt[32];		/* filter resolution options */
int	pfiltalways;		/* always use pfilt? */

VIEW	*getview();
char	*getexp();


main(argc, argv)
int	argc;
char	*argv[];
{
	int	explicate = 0;
	int	i;

	progname = argv[0];			/* get arguments */
	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 'e':			/* print variables */
			explicate++;
			break;
		case 'w':			/* turn off warnings */
			nowarn++;
			break;
		case 's':			/* silent mode */
			silent++;
			break;
		case 'n':			/* take no action */
			noaction++;
			break;
		default:
			goto userr;
		}
	if (i != argc-1)
		goto userr;
	cfname = argv[i];
						/* load variables */
	loadvars(cfname);
						/* did we get DIRECTORY? */
	checkdir();
						/* check status */
	if (getastat() < 0) {
		fprintf(stderr, "%s: exiting\n", progname);
		quit(1);
	}
						/* pfilt always if options given */
	pfiltalways = vdef(PFILT);
						/* load RIF if any */
	if (vdef(RIF))
		getradfile(vval(RIF));
						/* set defaults */
	setdefaults();
						/* print variables */
	if (explicate)
		printvars(stdout);
						/* run animation */
	animate();
						/* all done */
	if (vdef(NEXTANIM)) {
		argv[i] = vval(NEXTANIM);	/* just change input file */
		if (!silent)
			printargs(argc, argv, stdout);
		if (!noaction) {
			execvp(progname, argv);		/* pass to next */
			quit(1);			/* shouldn't return */
		}
	}
	quit(0);
userr:
	fprintf(stderr, "Usage: %s [-s][-n][-w][-e] anim_file\n", progname);
	quit(1);
}


getastat()			/* check/set animation status */
{
	char	buf[256];
	FILE	*fp;

	sprintf(buf, "%s/%s", vval(DIRECTORY), SFNAME);
	if ((fp = fopen(buf, "r")) == NULL) {
		if (errno != ENOENT) {
			perror(buf);
			return(-1);
		}
		astat.rnext = astat.fnext = astat.tnext = 0;
		goto setours;
	}
	if (fscanf(fp, "Control host: %s\n", astat.host) != 1)
		goto fmterr;
	if (fscanf(fp, "Control PID: %d\n", &astat.pid) != 1)
		goto fmterr;
	if (fscanf(fp, "Control file: %s\n", astat.cfname) != 1)
		goto fmterr;
	if (fscanf(fp, "Next render: %d\n", &astat.rnext) != 1)
		goto fmterr;
	if (fscanf(fp, "Next filter: %d\n", &astat.fnext) != 1)
		goto fmterr;
	if (fscanf(fp, "Next transfer: %d\n", &astat.tnext) != 1)
		goto fmterr;
	fclose(fp);
	if (astat.pid != 0) {			/* thinks it's still running */
		gethostname(buf, sizeof(buf));
		if (strcmp(buf, astat.host)) {
			fprintf(stderr,
			"%s: process %d may still be running on host %s\n",
					progname, astat.pid, astat.host);
			return(-1);
		}
		if (kill(astat.pid, 0) != -1 || errno != ESRCH) {
			fprintf(stderr, "%s: process %d is still running\n",
					progname, astat.pid);
			return(-1);
		}
		/* assume it is dead */
	}
	if (strcmp(cfname, astat.cfname) && astat.tnext != 0) {	/* other's */
		fprintf(stderr, "%s: unfinished job \"%s\"\n",
				progname, astat.cfname);
		return(-1);
	}
setours:					/* set our values */
	gethostname(astat.host, sizeof(astat.host));
	astat.pid = getpid();
	strcpy(astat.cfname, cfname);
	return(0);
fmterr:
	fprintf(stderr, "%s: format error in status file \"%s\"\n",
			progname, buf);
	fclose(fp);
	return(-1);
}


putastat()			/* put out current status */
{
	char	buf[256];
	FILE	*fp;

	sprintf(buf, "%s/%s", vval(DIRECTORY), SFNAME);
	if ((fp = fopen(buf, "w")) == NULL) {
		perror(buf);
		quit(1);
	}
	fprintf(fp, "Control host: %s\n", astat.host);
	fprintf(fp, "Control PID: %d\n", astat.pid);
	fprintf(fp, "Control file: %s\n", astat.cfname);
	fprintf(fp, "Next render: %d\n", astat.rnext);
	fprintf(fp, "Next filter: %d\n", astat.fnext);
	fprintf(fp, "Next transfer: %d\n", astat.tnext);
	fclose(fp);
}


checkdir()			/* make sure we have our directory */
{
	struct stat	stb;

	if (!vdef(DIRECTORY)) {
		fprintf(stderr, "%s: %s undefined\n",
				progname, vnam(DIRECTORY));
		quit(1);
	}
	if (stat(vval(DIRECTORY), &stb) == -1) {
		if (errno == ENOENT && mkdir(vval(DIRECTORY), 0777) == 0)
			return;
		perror(vval(DIRECTORY));
		quit(1);
	}
	if (!(stb.st_mode & S_IFDIR)) {
		fprintf(stderr, "%s: not a directory\n", vval(DIRECTORY));
		quit(1);
	}
}


setdefaults()			/* set default values */
{
	char	buf[256];

	if (vdef(OCTREE) == vdef(ANIMATE)) {
		fprintf(stderr, "%s: either %s or %s must be defined\n",
				progname, vnam(OCTREE), vnam(ANIMATE));
		quit(1);
	}
	if (!vdef(VIEWFILE)) {
		fprintf(stderr, "%s: %s undefined\n", progname, vnam(VIEWFILE));
		quit(1);
	}
	if (!vdef(START)) {
		vval(START) = "1";
		vdef(START)++;
	}
	if (!vdef(END)) {
		sprintf(buf, "%d", countviews());
		vval(END) = savqstr(buf);
		vdef(END)++;
	}
	if (!vdef(BASENAME)) {
		sprintf(buf, "%s/frame%%03d", vval(DIRECTORY));
		vval(BASENAME) = savqstr(buf);
		vdef(BASENAME)++;
	}
	if (!vdef(RESOLUTION)) {
		vval(RESOLUTION) = "640";
		vdef(RESOLUTION)++;
	}
	if (!vdef(OVERSAMP)) {
		vval(OVERSAMP) = "2";
		vdef(OVERSAMP)++;
	}
	if (!vdef(INTERP)) {
		vval(INTERP) = "0";
		vdef(INTERP)++;
	}
	if (!vdef(MBLUR)) {
		vval(MBLUR) = "0";
		vdef(MBLUR)++;
	}
	if (!vdef(RTRACE)) {
		vval(RTRACE) = "F";
		vdef(RTRACE)++;
	}
	if (!vdef(DISKSPACE)) {
		if (!nowarn)
			fprintf(stderr,
		"%s: warning - no %s setting, assuming 100 Mbytes available\n",
					progname, vnam(DISKSPACE));
		vval(DISKSPACE) = "100";
		vdef(DISKSPACE)++;
	}
				/* append rendering options */
	if (vdef(RENDER))
		sprintf(rendopt+strlen(rendopt), " %s", vval(RENDER));
}


getradfile(rfname)		/* run rad and get needed variables */
char	*rfname;
{
	static short	mvar[] = {OCTREE,PFILT,RESOLUTION,EXPOSURE,-1};
	char	combuf[256];
	register int	i;
	register char	*cp;
					/* create rad command */
	sprintf(rendopt, " @%s/render.opt", vval(DIRECTORY));
	sprintf(combuf,
	"rad -v 0 -s -e -w %s OPTFILE=%s | egrep '^[ \t]*(NOMATCH",
			rfname, rendopt+2);
	cp = combuf;
	while (*cp) cp++;		/* match unset variables */
	for (i = 0; mvar[i] >= 0; i++)
		if (!vdef(mvar[i])) {
			*cp++ = '|';
			strcpy(cp, vnam(mvar[i]));
			while (*cp) cp++;
		}
	sprintf(cp, ")[ \t]*=' > %s/radset.var", vval(DIRECTORY));
	cp += 11;			/* point to file name */
	if (system(combuf)) {
		fprintf(stderr, "%s: bad rad input file \"%s\"\n",
				progname, rfname);
		quit(1);
	}
	loadvars(cp);			/* load variables and remove file */
	unlink(cp);
}


animate()			/* run animation */
{
	int	xres, yres;
	float	pa, mult;
	int	frames_batch;
	register int	i;
	double	d1, d2;
					/* compute rpict resolution */
	i = sscanf(vval(RESOLUTION), "%d %d %f", &xres, &yres, &pa);
	mult = vflt(OVERSAMP);
	if (i == 3) {
		sprintf(rresopt, "-x %d -y %d -pa %f", (int)(mult*xres),
				(int)(mult*yres), pa);
		sprintf(fresopt, "-x %d -y %d -pa %f", xres, yres, pa);
	} else if (i) {
		if (i == 1) yres = xres;
		sprintf(rresopt, "-x %d -y %d", (int)(mult*xres),
				(int)(mult*yres));
		sprintf(fresopt, "-x %d -y %d -pa 1", xres, yres);
	} else
		badvalue(RESOLUTION);
					/* consistency checks */
	if (vdef(ANIMATE)) {
		if (vint(INTERP)) {
			if (!nowarn)
				fprintf(stderr,
					"%s: resetting %s=0 for animation\n",
						progname, vnam(INTERP));
			vval(INTERP) = "0";
		}
		if (atoi(vval(MBLUR))) {	/* can't handle this yet */
			if (!nowarn)
				fprintf(stderr,
					"%s: resetting %s=0 for animation\n",
						progname, vnam(MBLUR));
			vval(MBLUR) = "0";
		}
	}
					/* figure # frames per batch */
	d1 = mult*xres*mult*yres*4;		/* space for orig. picture */
	if ((i=vint(INTERP)) || atoi(vval(MBLUR)))
		d1 += mult*xres*mult*yres*4;	/* space for z-buffer */
	d2 = xres*yres*4;			/* space for final picture */
	frames_batch = (i+1)*(vflt(DISKSPACE)*1048576.-d1)/(d1+i*d2);
	if (frames_batch < i+2) {
		fprintf(stderr, "%s: insufficient disk space allocated\n",
				progname);
		quit(1);
	}
					/* initialize status file */
	if (astat.rnext == 0)
		astat.rnext = astat.fnext = astat.tnext = vint(START);
	putastat();
					/* render in batches */
	while (astat.rnext <= vint(END)) {
		renderframes(frames_batch);
		filterframes();
		transferframes();
	}
					/* mark status as finished */
	astat.pid = 0;
	putastat();
					/* close open files */
	getview(0);
	getexp(0);
}


renderframes(nframes)		/* render next nframes frames */
int	nframes;
{
	static char	vendbuf[16];
	VIEW	*vp;
	FILE	*fp = NULL;
	char	vfname[128];
	int	lastframe;
	register int	i;

	if (astat.tnext < astat.rnext)	/* other work to do first */
		return;
					/* create batch view file */
	if (!vdef(ANIMATE)) {
		sprintf(vfname, "%s/anim.vf", vval(DIRECTORY));
		if ((fp = fopen(vfname, "w")) == NULL) {
			perror(vfname);
			quit(1);
		}
	}
					/* bound batch properly */
	lastframe = astat.rnext + nframes - 1;
	if ((lastframe-1) % (vint(INTERP)+1))	/* need even interval */
		lastframe += vint(INTERP)+1 - ((lastframe-1)%(vint(INTERP)+1));
	if (lastframe > vint(END))		/* check for end */
		lastframe = vint(END);
					/* render each view */
	for (i = astat.rnext; i <= lastframe; i++) {
		if ((vp = getview(i)) == NULL) {
			if (!nowarn)
				fprintf(stderr,
				"%s: ran out of views before last frame\n",
					progname);
			sprintf(vval(END)=vendbuf, "%d", i-1);
			lastframe = i - 1;
			break;
		}
		if (vdef(ANIMATE))		/* animate frame */
			animrend(i, vp);
		else {				/* else record it */
			fputs(VIEWSTR, fp);
			fprintview(vp, fp);
			putc('\n', fp);
		}
	}
	if (vdef(ANIMATE))		/* wait for renderings to finish */
		animwait(0);
	else {				/* else if walk-through */
		fclose(fp);		/* close view file */
		walkwait(astat.rnext, lastframe, vfname);	/* walk it */
		unlink(vfname);		/* remove view file */
	}
	if (vdef(ARCHIVE))		/* archive results */
		archive(astat.rnext, lastframe);
	astat.rnext = i;		/* update status */
	putastat();
}


filterframes()				/* catch up with filtering */
{
	VIEW	*vp;
	register int	i;

	if (astat.tnext < astat.fnext)	/* other work to do first */
		return;
					/* filter each view */
	for (i = astat.fnext; i < astat.rnext; i++) {
		if ((vp = getview(i)) == NULL) {	/* get view i */
			fprintf(stderr,
			"%s: unexpected error reading view for frame %d\n",
					progname, i);
			quit(1);
		}
		dofilt(i, vp, getexp(i));		/* filter frame */
	}
	filtwait(0);			/* wait for filter processes */
	astat.fnext = i;		/* update status */
	putastat();
}


transferframes()			/* catch up with picture transfers */
{
	char	combuf[10240];
	register char	*cp;
	register int	i;

	if (astat.tnext >= astat.fnext)	/* nothing to do, yet */
		return;
	if (!vdef(TRANSFER)) {		/* no transfer function -- leave 'em */
		astat.tnext = astat.fnext;
		putastat();		/* update status */
		return;
	}
	strcpy(combuf, vval(TRANSFER));	/* start transfer command */
	cp = combuf + strlen(combuf);
					/* make argument list */
	for (i = astat.tnext; i < astat.fnext; i++) {
		*cp++ = ' ';
		sprintf(cp, vval(BASENAME), i);
		while (*cp) cp++;
		strcpy(cp, ".pic");
		cp += 4;
	}
	if (runcom(combuf)) {		/* transfer frames */
		fprintf(stderr, "%s: error running transfer command\n",
				progname);
		quit(1);
	}
	astat.tnext = i;		/* update status */
	putastat();
}


animrend(frame, vp)			/* start animation frame */
int	frame;
VIEW	*vp;
{
	char	combuf[2048];
	char	fname[128];

	sprintf(fname, vval(BASENAME), frame);
	strcat(fname, ".unf");
	if (access(fname, F_OK) == 0)
		return;
	sprintf(combuf, "%s %d | rpict%s%s %s > %s", vval(ANIMATE), frame,
			rendopt, viewopt(vp), rresopt, fname);
	if (runcom(combuf)) {
		fprintf(stderr, "%s: error rendering frame %d\n",
				progname, frame);
		quit(1);
	}
}


animwait(nwait)				/* wait for renderings to finish */
int	nwait;
{
	/* currently does nothing since parallel rendering not working */
}


walkwait(first, last, vfn)		/* walk-through frames */
int	first, last;
char	*vfn;
{
	char	combuf[2048];
	register int	i;

	if (!noaction && vint(INTERP))		/* create dummy frames */
		for (i = first; i <= last; i++)
			if (i < vint(END) && (i-1) % (vint(INTERP)+1)) {
				sprintf(combuf, vval(BASENAME), i);
				strcat(combuf, ".unf");
				close(open(combuf, O_RDONLY|O_CREAT, 0666));
			}
					/* create command */
	sprintf(combuf, "rpict%s ", rendopt);
	if (vint(INTERP) || atoi(vval(MBLUR)))
		sprintf(combuf+strlen(combuf), "-z %s.zbf ", vval(BASENAME));
	sprintf(combuf+strlen(combuf), "-o %s.unf %s -S %d %s < %s",
			vval(BASENAME), rresopt, first, vval(OCTREE), vfn);
	if (runcom(combuf)) {
		fprintf(stderr,
		"%s: error rendering walk-through frames %d through %d\n",
				progname, first, last);
		quit(1);
	}
	if (!noaction && vint(INTERP))		/* remove dummy frames */
		for (i = first; i <= last; i++)
			if (i < vint(END) && (i-1) % (vint(INTERP)+1)) {
				sprintf(combuf, vval(BASENAME), i);
				strcat(combuf, ".unf");
				unlink(combuf);
			}
}


recover(frame)				/* recover the specified frame */
int	frame;
{
	char	combuf[2048];
	char	fname[128];
	register char	*cp;

	sprintf(fname, vval(BASENAME), frame);
	if (vdef(ANIMATE))
		sprintf(combuf, "%s %d | rpict%s",
				vval(ANIMATE), frame, rendopt);
	else
		sprintf(combuf, "rpict%s", rendopt);
	cp = combuf + strlen(combuf);
	if (vint(INTERP) || atoi(vval(MBLUR))) {
		sprintf(cp, " -z %s.zbf", fname);
		while (*cp) cp++;
	}
	sprintf(cp, " -ro %s.unf", fname);
	while (*cp) cp++;
	if (!vdef(ANIMATE)) {
		*cp++ = ' ';
		strcpy(cp, vval(OCTREE));
	}
	if (runcom(combuf)) {
		fprintf(stderr, "%s: error recovering frame %d\n",
				progname, frame);
		quit(1);
	}
}


archive(first, last)			/* archive finished renderings */
int	first, last;
{
	char	combuf[10240];
	int	offset;
	struct stat	stb;
	register char	*cp;
	register int	i;

	strcpy(cp=combuf, vval(ARCHIVE));
	while (*cp) cp++;
	offset = cp - combuf;
	*cp++ = ' ';				/* make argument list */
	for (i = first; i <= last; i++) {
		sprintf(cp, vval(BASENAME), i);
		strcat(cp, ".unf");
		if (stat(cp, &stb) == 0 && stb.st_size > 0) {	/* non-zero? */
			while (*cp) cp++;
			*cp++ = ' ';
			sprintf(cp, vval(BASENAME), i);
			strcat(cp, ".zbf");
			if (access(cp, F_OK) == 0) {		/* exists? */
				while (*cp) cp++;
				*cp++ = ' ';
			}
		}
	}
	*--cp = '\0';
	if (cp <= combuf + offset)		/* no files? */
		return;
	if (runcom(combuf)) {			/* run archive command */
		fprintf(stderr,
		"%s: error running archive command on frames %d through %d\n",
				progname, first, last);
		quit(1);
	}
}


dofilt(frame, vp, ep)				/* filter frame */
int	frame;
VIEW	*vp;
char	*ep;
{
	char	fnbefore[128], fnafter[128];
	char	combuf[1024], fname[128];
	int	usepinterp, usepfilt;
	int	frbefore, frafter, triesleft;
						/* check what is needed */
	usepinterp = atoi(vval(MBLUR));
	usepfilt = pfiltalways | ep==NULL;
						/* compute rendered views */
	frbefore = frame - ((frame-1) % (vint(INTERP)+1));
	frafter = frbefore + vint(INTERP) + 1;
	if (frafter > vint(END))
		frafter = vint(END);
	if (frafter == frame) {			/* pfilt only */
		frbefore = frafter;
		usepinterp = 0;			/* update what's needed */
		usepfilt |= vflt(OVERSAMP)>1.01 || strcmp(ep,"1");
		triesleft = 2;
	} else if (frbefore == frame) {		/* no interpolation */
						/* remove unneeded files */
		if (frbefore-vint(INTERP)-1 >= 1) {
			sprintf(fname, vval(BASENAME), frbefore-vint(INTERP)-1);
			sprintf(combuf, "rm -f %s.unf %s.zbf", fname, fname);
			runcom(combuf);
		}
						/* update what's needed */
		if (usepinterp)
			triesleft = 3;
		else {
			usepfilt |= vflt(OVERSAMP)>1.01 || strcmp(ep,"1");
			triesleft = 2;
		}
	} else {				/* interpolation needed */
		usepinterp++;
		triesleft = 3;
	}
	if (frafter >= astat.rnext) {		/* next batch unavailable */
		frafter = frbefore;
		if (triesleft > 2)
			triesleft = 2;
	}
	sprintf(fnbefore, vval(BASENAME), frbefore);
	sprintf(fnafter, vval(BASENAME), frafter);
tryit:						/* generate command */
	if (usepinterp) {			/* using pinterp */
		if (atoi(vval(MBLUR))) {
			FILE	*fp;		/* motion blurring */
			sprintf(fname, "%s/vw0", vval(DIRECTORY));
			if ((fp = fopen(fname, "w")) == NULL) {
				perror(fname); quit(1);
			}
			fputs(VIEWSTR, fp);
			fprintview(vp, fp);
			putc('\n', fp); fclose(fp);
			if ((vp = getview(frame+1)) == NULL) {
				fprintf(stderr,
			"%s: unexpected error reading view for frame %d\n",
						progname, frame+1);
				quit(1);
			}
			sprintf(fname, "%s/vw1", vval(DIRECTORY));
			if ((fp = fopen(fname, "w")) == NULL) {
				perror(fname); quit(1);
			}
			fputs(VIEWSTR, fp);
			fprintview(vp, fp);
			putc('\n', fp); fclose(fp);
			sprintf(combuf,
	"(pmblur %s %d %s/vw0 %s/vw1; rm -f %s/vw0 %s/vw1) | pinterp -B",
				*sskip(vval(MBLUR)) ? sskip(vval(MBLUR)) : "1",
					atoi(vval(MBLUR)), vval(DIRECTORY),
					vval(DIRECTORY), vval(DIRECTORY),
					vval(DIRECTORY), vval(DIRECTORY));
		} else				/* no blurring */
			strcpy(combuf, "pinterp");
		strcat(combuf, viewopt(vp));
		if (vbool(RTRACE))
			sprintf(combuf+strlen(combuf), " -ff -fr '%s %s'",
					rendopt, vval(OCTREE));
		if (vdef(PINTERP))
			sprintf(combuf+strlen(combuf), " %s", vval(PINTERP));
		if (usepfilt)
			sprintf(combuf+strlen(combuf), " %s", rresopt);
		else
			sprintf(combuf+strlen(combuf), " %s -e %s",
					fresopt, ep);
		sprintf(combuf+strlen(combuf), " %s.unf %s.zbf",
				fnbefore, fnbefore);
		if (frafter != frbefore)
			 sprintf(combuf+strlen(combuf), " %s.unf %s.zbf",
					fnafter, fnafter);
		if (usepfilt) {			/* also pfilt */
			if (vdef(PFILT))
				sprintf(combuf+strlen(combuf), " | pfilt %s",
						vval(PFILT));
			else
				strcat(combuf, " | pfilt");
			if (ep != NULL)
				sprintf(combuf+strlen(combuf), " -1 -e %s %s",
						ep, fresopt);
			else
				sprintf(combuf+strlen(combuf), " %s", fresopt);
		}
	} else if (usepfilt) {			/* pfilt only */
		if (vdef(PFILT))
			sprintf(combuf, "pfilt %s", vval(PFILT));
		else
			strcpy(combuf, "pfilt");
		if (ep != NULL)
			sprintf(combuf+strlen(combuf), " -1 -e %s %s %s.unf",
				ep, fresopt, fnbefore);
		else
			sprintf(combuf+strlen(combuf), " %s %s.unf",
					fresopt, fnbefore);
	} else {				/* else just check it */
		sprintf(combuf, "ra_rgbe -r %s.unf", fnbefore);
	}
						/* output file name */
	sprintf(fname, vval(BASENAME), frame);
	sprintf(combuf+strlen(combuf), " > %s.pic", fname);
	if (runcom(combuf))			/* run filter command */
		switch (--triesleft) {
		case 2:				/* try to recover frafter */
			recover(frafter);
			goto tryit;
		case 1:				/* try to recover frbefore */
			recover(frbefore);
			goto tryit;
		default:			/* we've really failed */
			fprintf(stderr,
			"%s: unrecoverable filtering error on frame %d\n",
					progname, frame);
			quit(1);
		}
}


filtwait(nwait)			/* wait for filtering processes to finish */
int	nwait;
{
	/* currently does nothing since parallel filtering not working */
}


VIEW *
getview(n)			/* get view number n */
int	n;
{
	static FILE	*viewfp = NULL;		/* view file pointer */
	static int	viewnum = 0;		/* current view number */
	static VIEW	curview = STDVIEW;	/* current view */
	char	linebuf[256];

	if (n == 0) {			/* signal to close file and clean up */
		if (viewfp != NULL) {
			fclose(viewfp);
			viewfp = NULL;
			viewnum = 0;
			copystruct(&curview, &stdview);
		}
		return(NULL);
	}
	if (viewfp == NULL) {		/* open file */
		if ((viewfp = fopen(vval(VIEWFILE), "r")) == NULL) {
			perror(vval(VIEWFILE));
			quit(1);
		}
	} else if (n < viewnum) {	/* rewind file */
		if (fseek(viewfp, 0L, 0) == EOF) {
			perror(vval(VIEWFILE));
			quit(1);
		}
		copystruct(&curview, &stdview);
		viewnum = 0;
	}
	while (n > viewnum) {		/* scan to desired view */
		if (fgets(linebuf, sizeof(linebuf), viewfp) == NULL)
			return(NULL);
		if (isview(linebuf) && sscanview(&curview, linebuf) > 0)
			viewnum++;
	}
	return(&curview);		/* return it */
}


int
countviews()			/* count views in view file */
{
	register int	n = 0;

	while (getview(n+1) != NULL)
		n++;
	return(n);
}


char *
getexp(n)			/* get exposure for nth frame */
int	n;
{
	extern char	*fskip();
	static char	expval[32];
	static FILE	*expfp = NULL;
	static long	*exppos;
	static int	curfrm;
	register char	*cp;

	if (n == 0) {				/* signal to close file */
		if (expfp != NULL) {
			fclose(expfp);
			expfp = NULL;
		}
		return(NULL);
	}
	if (!vdef(EXPOSURE))			/* no setting (auto) */
		return(NULL);
	if (isflt(vval(EXPOSURE)))		/* always the same */
		return(vval(EXPOSURE));
	if (expfp == NULL) {			/* open exposure file */
		if ((expfp = fopen(vval(EXPOSURE), "r")) == NULL) {
			fprintf(stderr,
			"%s: cannot open exposure file \"%s\"\n",
					progname, vval(EXPOSURE));
			quit(1);
		}
		curfrm = vint(END) + 1;		/* init lookup tab. */
		exppos = (long *)malloc(curfrm*sizeof(long *));
		if (exppos == NULL) {
			perror(progname);
			quit(1);
		}
		while (curfrm--)
			exppos[curfrm] = -1L;
		curfrm = 0;
	}
						/* find position in file */
	if (n-1 != curfrm && n != curfrm && exppos[n-1] >= 0 &&
				fseek(expfp, exppos[curfrm=n-1], 0) == EOF) {
		fprintf(stderr, "%s: seek error on exposure file\n", progname);
		quit(1);
	}
	while (n > curfrm) {			/* read exposure */
		if (exppos[curfrm] < 0)
			exppos[curfrm] = ftell(expfp);
		if (fgets(expval, sizeof(expval), expfp) == NULL) {
			fprintf(stderr, "%s: too few exposures\n",
					vval(EXPOSURE));
			quit(1);
		}
		curfrm++;
		cp = fskip(expval);			/* check format */
		if (cp == NULL || *cp != '\n') {
			fprintf(stderr,
				"%s: exposure format error on line %d\n",
					vval(EXPOSURE), curfrm);
			quit(1);
		}
		*cp = '\0';
	}
	return(expval);				/* return value */
}


runcom(cs)			/* run command */
char	*cs;
{
	if (!silent)		/* echo it */
		printf("\t%s\n", cs);
	if (noaction)
		return(0);
	fflush(stdout);		/* flush output and pass to shell */
	return(system(cs));
}


rmfile(fn)			/* remove a file */
char	*fn;
{
	if (!silent)
#ifdef MSDOS
		printf("\tdel %s\n", fn);
#else
		printf("\trm -f %s\n", fn);
#endif
	if (noaction)
		return(0);
	return(unlink(fn));
}


badvalue(vc)			/* report bad variable value and exit */
int	vc;
{
	fprintf(stderr, "%s: bad value for variable '%s'\n",
			progname, vnam(vc));
	quit(1);
}
