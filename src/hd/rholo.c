/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Radiance holodeck generation controller
 */

#include "rholo.h"
#include "paths.h"
#include <sys/types.h>

			/* the following must be consistent with rholo.h */
int	NVARS = NRHVARS;		/* total number of variables */

VARIABLE	vv[] = RHVINIT;		/* variable-value pairs */

char	*progname;		/* our program name */
char	*hdkfile;		/* holodeck file name */
char	froot[MAXPATH];		/* root file name */

int	nowarn = 0;		/* turn warnings off? */

double	expval = 1.;		/* global exposure value */

int	ncprocs = 0;		/* desired number of compute processes */

char	*outdev = NULL;		/* output device name */

time_t	starttime;		/* time we got started */
time_t	endtime;		/* time we should end by */
time_t	reporttime;		/* time for next report */

int	rtargc = 1;		/* rtrace command */
char	*rtargv[128] = {"rtrace", NULL};

long	nraysdone = 0L;		/* number of rays done */
long	npacksdone = 0L;	/* number of packets done */

PACKET	*freepacks;		/* available packets */

extern time_t	time();


main(argc, argv)
int	argc;
char	*argv[];
{
	HDGRID	hdg;
	int	i;
	int	force = 0;
						/* mark start time */
	starttime = time(NULL);
	progname = argv[0];			/* get arguments */
	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 'w':			/* turn off warnings */
			nowarn++;
			break;
		case 'f':			/* force overwrite */
			force++;
			break;
		case 'n':			/* compute processes */
			if (i >= argc-2)
				goto userr;
			ncprocs = atoi(argv[++i]);
			break;
		case 'o':			/* output display */
			if (i >= argc-2)
				goto userr;
			outdev = argv[++i];
			break;
		default:
			goto userr;
		}
						/* do we have a job? */
	if (outdev == NULL && ncprocs <= 0)
		goto userr;
						/* get root file name */
	rootname(froot, hdkfile=argv[i++]);
						/* load... */
	if (i < argc) {				/* variables */
		loadvars(argv[i++]);
							/* cmdline settings */
		for ( ; i < argc; i++)
			if (setvariable(argv[i], matchvar) < 0) {
				sprintf(errmsg, "unknown variable: %s",
						argv[i]);
				error(USER, errmsg);
			}
							/* check settings */
		checkvalues();
							/* load RIF if any */
		if (vdef(RIF))
			getradfile(vval(RIF));
							/* set defaults */
		setdefaults(&hdg);
							/* holodeck exists? */
		if (!force && access(hdkfile, R_OK|W_OK) == 0)
			error(USER,
				"holodeck file exists -- use -f to overwrite");
							/* create holodeck */
		creatholo(&hdg);
	} else {				/* else load holodeck */
		loadholo();
		if (vdef(RIF))				/* load RIF if any */
			getradfile(vval(RIF));
	}
						/* initialize */
	initrholo();
						/* run */
	while (rholo())
		;
						/* done */
	quit(0);
userr:
	fprintf(stderr,
"Usage: %s {-n nprocs|-o disp} [-w][-f] output.hdk [control.hif [VAR=val ..]]\n",
			progname);
	quit(1);
}


initrholo()			/* get our holodeck running */
{
	extern int	global_packet();
	register int	i;

	if (outdev != NULL)			/* open output device */
		disp_open(outdev);
	else if (ncprocs > 0)			/* else use global ray feed */
		init_global();
						/* record end time */
	if (!vdef(TIME) || vflt(TIME) <= FTINY)
		endtime = 0;
	else
		endtime = starttime + vflt(TIME)*3600.;
						/* set up memory cache */
	if (outdev == NULL)
		hdcachesize = 0;	/* manual flushing */
	else if (vdef(CACHE))
		hdcachesize = 1024.*1024.*vflt(CACHE);
						/* open report file */
	if (vdef(REPORT)) {
		register char	*s = sskip2(vval(REPORT), 1);
		if (*s && freopen(s, "a", stderr) == NULL)
			quit(2);
	}
						/* start rtrace */
	if (ncprocs > 0) {
		i = start_rtrace();
		if (i < 1)
			error(USER, "cannot start rtrace process");
		if (vdef(REPORT)) {		/* make first report */
			printargs(rtargc, rtargv, stderr);
			report(0);
		}
						/* allocate packets */
		freepacks = (PACKET *)bmalloc(i*sizeof(PACKET));
		if (freepacks == NULL)
			goto memerr;
		freepacks[--i].nr = 0;
		freepacks[i].next = NULL;
		if (!vbool(OBSTRUCTIONS)) {
			freepacks[i].offset = (float *)bmalloc(
					RPACKSIZ*sizeof(float)*(i+1) );
			if (freepacks[i].offset == NULL)
				goto memerr;
		} else
			freepacks[i].offset = NULL;
		while (i--) {
			freepacks[i].nr = 0;
			freepacks[i].offset = freepacks[i+1].offset == NULL ?
					NULL : freepacks[i+1].offset+RPACKSIZ ;
			freepacks[i].next = &freepacks[i+1];
		}
	}
	return;
memerr:
	error(SYSTEM, "out of memory in initrholo");
}


rholo()				/* holodeck main loop */
{
	static int	idle = 1;
	PACKET	*pl = NULL, *plend;
	register PACKET	*p;
	time_t	t;
	long	l;

	if (outdev != NULL)		/* check display */
		if (!disp_check(idle))
			return(0);
					/* display only? */
	if (ncprocs <= 0)
		return(1);
					/* check file size */
	if ((l = 1024.*1024.*vflt(DISKSPACE)) > 0 &&
			hdfiluse(hdlist[0]->fd, 0) + hdmemuse(0) >= l)
		return(0);
					/* check time */
	if (endtime > 0 || reporttime > 0)
		t = time(NULL);
	if (endtime > 0 && t >= endtime)
		return(0);
	if (reporttime > 0 && t >= reporttime)
		report(t);
					/* get packets to process */
	while (freepacks != NULL) {
		p = freepacks; freepacks = p->next; p->next = NULL;
		if (!next_packet(p)) {
			p->next = freepacks; freepacks = p;
			break;
		}
		if (pl == NULL) pl = p;
		else plend->next = p;
		plend = p;
	}
	idle = pl == NULL && freepacks != NULL;
					/* are we out of stuff to do? */
	if (idle && outdev == NULL)
		return(0);
					/* else process packets */
	done_packets(do_packets(pl));
	return(1);			/* and continue */
}


report(t)			/* report progress so far */
time_t	t;
{
	if (t == 0)
		t = time(NULL);
	fprintf(stderr, "%s: %ld packets (%ld rays) done after %.2f hours\n",
			progname, npacksdone, nraysdone, (t-starttime)/3600.);
	fflush(stderr);
	if (vdef(REPORT))
		reporttime = t + (time_t)(vflt(REPORT)*60.+.5);
}


setdefaults(gp)			/* set default values */
register HDGRID	*gp;
{
	extern char	*atos();
	register int	i;
	double	len[3], maxlen, d;
	char	buf[64];

	if (!vdef(SECTION)) {
		sprintf(errmsg, "%s must be defined", vnam(SECTION));
		error(USER, errmsg);
	}
	if (vdef(SECTION) > 1) {
		sprintf(errmsg, "ignoring all but first %s", vnam(SECTION));
		error(WARNING, errmsg);
	}
	if (sscanf(vval(SECTION),
			"%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
			&gp->orig[0], &gp->orig[1], &gp->orig[2],
			&gp->xv[0][0], &gp->xv[0][1], &gp->xv[0][2],
			&gp->xv[1][0], &gp->xv[1][1], &gp->xv[1][2],
			&gp->xv[2][0], &gp->xv[2][1], &gp->xv[2][2]) != 12)
		badvalue(SECTION);
	maxlen = 0.;
	for (i = 0; i < 3; i++)
		if ((len[i] = VLEN(gp->xv[i])) > maxlen)
			maxlen = len[i];
	if (!vdef(GRID)) {
		sprintf(buf, "%.4f", maxlen/8.);
		vval(GRID) = savqstr(buf);
		vdef(GRID)++;
	}
	if ((d = vflt(GRID)) <= FTINY)
		badvalue(GRID);
	for (i = 0; i < 3; i++)
		gp->grid[i] = len[i]/d + (1.-FTINY);
	if (!vdef(EXPOSURE)) {
		sprintf(errmsg, "%s must be defined", vnam(EXPOSURE));
		error(USER, errmsg);
	}
	expval = vval(EXPOSURE)[0] == '-' || vval(EXPOSURE)[0] == '+' ?
			pow(2., vflt(EXPOSURE)) : vflt(EXPOSURE);
	if (!vdef(OCTREE)) {
		if ((vval(OCTREE) = bmalloc(strlen(froot)+5)) == NULL)
			error(SYSTEM, "out of memory");
		sprintf(vval(OCTREE), "%s.oct", froot);
		vdef(OCTREE)++;
	}
	if (!vdef(DISKSPACE)) {
		sprintf(errmsg,
			"no %s setting, assuming 100 Mbytes available",
				vnam(DISKSPACE));
		error(WARNING, errmsg);
		vval(DISKSPACE) = "100";
		vdef(DISKSPACE)++;
	}
	if (!vdef(OBSTRUCTIONS)) {
		vval(OBSTRUCTIONS) = "T";
		vdef(OBSTRUCTIONS)++;
	}
	if (!vdef(OCCUPANCY)) {
		vval(OCCUPANCY) = "U";
		vdef(OCCUPANCY)++;
	}
				/* append rendering options */
	if (vdef(RENDER))
		rtargc += wordstring(rtargv+rtargc, vval(RENDER));
}


creatholo(gp)			/* create a holodeck output file */
HDGRID	*gp;
{
	long	endloc = 0;
	FILE	*fp;
					/* open & truncate file */
	if ((fp = fopen(hdkfile, "w+")) == NULL) {
		sprintf(errmsg, "cannot open \"%s\" for writing", hdkfile);
		error(SYSTEM, errmsg);
	}
					/* write information header */
	newheader("RADIANCE", fp);
	printvars(fp);
	fputformat(HOLOFMT, fp);
	fputc('\n', fp);
	putw(HOLOMAGIC, fp);		/* put magic number & terminus */
	fwrite(&endloc, sizeof(long), 1, fp);
	fflush(fp);			/* flush buffer */
	hdinit(fileno(fp), gp);		/* allocate and initialize index */
			/* we're dropping fp here.... */
}


headline(s)			/* process information header line */
char	*s;
{
	extern char	FMTSTR[];
	register char	*cp;
	char	fmt[32];

	if (formatval(fmt, s)) {
		if (strcmp(fmt, HOLOFMT)) {
			sprintf(errmsg, "%s file \"%s\" has %s%s",
					HOLOFMT, hdkfile, FMTSTR, fmt);
			error(USER, errmsg);
		}
		return;
	}
	for (cp = s; *cp; cp++)		/* take off any comments */
		if (*cp == '#') {
			*cp = '\0';
			break;
		}
	setvariable(s, matchvar);	/* don't flag errors */
}


loadholo()			/* start loading a holodeck from fname */
{
	FILE	*fp;
	long	endloc;
					/* open holodeck file */
	if ((fp = fopen(hdkfile, ncprocs>0 ? "r+" : "r")) == NULL) {
		sprintf(errmsg, "cannot open \"%s\" for %s", hdkfile,
				ncprocs>0 ? "appending" : "reading");
		error(SYSTEM, errmsg);
	}
					/* load variables from header */
	getheader(fp, headline, NULL);
					/* check magic number */
	if (getw(fp) != HOLOMAGIC) {
		sprintf(errmsg, "bad magic number in holodeck file \"%s\"",
				hdkfile);
		error(USER, errmsg);
	}
	fread(&endloc, sizeof(long), 1, fp);
	if (endloc != 0)
		error(WARNING, "ignoring multiple sections in holodeck file");
	fseek(fp, 0L, 1);			/* align system file pointer */
	hdinit(fileno(fp), NULL);		/* allocate and load index */
			/* we're dropping fp here.... */
}


done_packets(pl)		/* handle finished packets */
PACKET	*pl;
{
	static int	nunflushed = 0;
	register PACKET	*p;

	while (pl != NULL) {
		p = pl; pl = p->next; p->next = NULL;
		if (p->nr > 0) {		/* add to holodeck */
			bcopy((char *)p->ra,
				(char *)hdnewrays(hdlist[p->hd],p->bi,p->nr),
				p->nr*sizeof(RAYVAL));
			if (outdev != NULL)	/* display it */
				disp_packet(p);
			else
				nunflushed += p->nr;
		}
		nraysdone += p->nr;
		npacksdone++;
		p->nr = 0;			/* push onto free list */
		p->next = freepacks;
		freepacks = p;
	}
	if (nunflushed >= 256*RPACKSIZ) {
		hdflush(NULL);			/* flush holodeck buffers */
		nunflushed = 0;
	}
}


getradfile(rfargs)		/* run rad and get needed variables */
char	*rfargs;
{
	static short	mvar[] = {VIEW,OCTREE,EXPOSURE,-1};
	static char	tf1[] = TEMPLATE;
	char	tf2[64];
	char	combuf[256];
	char	*pippt;
	register int	i;
	register char	*cp;
					/* create rad command */
	mktemp(tf1);
	sprintf(tf2, "%s.rif", tf1);
	sprintf(combuf,
		"rad -v 0 -s -e -w %s OPTFILE=%s | egrep '^[ \t]*(NOMATCH",
			rfargs, tf1);
	cp = combuf;
	while (*cp){
		if (*cp == '|') pippt = cp;
		cp++;
	}				/* match unset variables */
	for (i = 0; mvar[i] >= 0; i++)
		if (!vdef(mvar[i])) {
			*cp++ = '|';
			strcpy(cp, vnam(mvar[i]));
			while (*cp) cp++;
			pippt = NULL;
		}
	if (pippt != NULL)
		strcpy(pippt, "> /dev/null");	/* nothing to match */
	else
		sprintf(cp, ")[ \t]*=' > %s", tf2);
	if (system(combuf)) {
		error(SYSTEM, "cannot execute rad command");
		unlink(tf2);			/* clean up */
		unlink(tf1);
		quit(1);
	}
	if (pippt == NULL) {
		loadvars(tf2);			/* load variables */
		unlink(tf2);
	}
	rtargc += wordfile(rtargv+rtargc, tf1);	/* get rtrace options */
	unlink(tf1);			/* clean up */
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


badvalue(vc)			/* report bad variable value and exit */
int	vc;
{
	sprintf(errmsg, "bad value for variable '%s'", vnam(vc));
	error(USER, errmsg);
}


eputs(s)			/* put error message to stderr */
register char  *s;
{
	static int  midline = 0;

	if (!*s)
		return;
	if (!midline++) {	/* prepend line with program name */
		fputs(progname, stderr);
		fputs(": ", stderr);
	}
	fputs(s, stderr);
	if (s[strlen(s)-1] == '\n') {
		fflush(stderr);
		midline = 0;
	}
}


wputs(s)			/* put warning string to stderr */
char	*s;
{
	if (!nowarn)
		eputs(s);
}


quit(ec)			/* exit program gracefully */
int	ec;
{
	int	status = 0;

	if (outdev != NULL)		/* close display */
		disp_close();
	if (hdlist[0] != NULL) {	/* flush holodeck */
		if (ncprocs > 0) {
			done_packets(flush_queue());
			status = end_rtrace();	/* close rtrace */
			hdflush(NULL);
			if (vdef(REPORT)) {
				long	fsiz, fuse;
				report(0);
				fsiz = lseek(hdlist[0]->fd, 0L, 2);
				fuse = hdfiluse(hdlist[0]->fd, 1);
				fprintf(stderr,
			"%s: %.1f Mbyte holodeck file, %.1f%% fragmentation\n",
						hdkfile, fsiz/(1024.*1024.),
						100.*(fsiz-fuse)/fsiz);
			}
		} else
			hdflush(NULL);
	}
	exit(ec ? ec : status);		/* exit */
}
