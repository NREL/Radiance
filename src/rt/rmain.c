/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  rmain.c - main for ray tracing programs
 *
 *     3/24/87
 */

					/* for flaky pre-processors */
#ifndef	 RPICT
#define	 RPICT		0
#endif
#ifndef	 RTRACE
#define	 RTRACE		0
#endif
#ifndef	 RVIEW
#define	 RVIEW		0
#endif

#include  "ray.h"

#include  "resolu.h"

#include  "octree.h"

#include  <signal.h>

#include  "view.h"

#include  "paths.h"

char  *progname;			/* argv[0] */

char  *octname;				/* octree name */

char  *libpath;				/* library directory list */

char  *sigerr[NSIG];			/* signal error messages */

extern char  VersionID[];		/* version ID string */

extern int  stderr_v();			/* standard error output */
int  (*errvec)() = stderr_v;		/* error output vector */
int  (*wrnvec)() = stderr_v;		/* warning output vector */
int  (*cmdvec)() = NULL;		/* command error vector */

int  (*trace)() = NULL;			/* trace call */
int  do_irrad = 0;			/* compute irradiance? */

extern long  time();
long  tstart;				/* start time */

extern int  ambnotify();		/* new object notify functions */
int  (*addobjnotify[])() = {ambnotify, NULL};

CUBE  thescene;				/* our scene */
OBJECT	nsceneobjs;			/* number of objects in our scene */

extern int  imm_irrad;			/* calculate immediate irradiance? */

extern int  ralrm;			/* seconds between reports */

extern int  greyscale;			/* map colors to brightness? */
extern char  *devname;			/* output device name */

extern char  *formstr();		/* string from format */
extern int  inform;			/* input format */
extern int  outform;			/* output format */
extern char  *outvals;			/* output values */

extern VIEW  ourview;			/* viewing parameters */

extern int  hresolu;			/* horizontal resolution */
extern int  vresolu;			/* vertical resolution */
extern double  pixaspect;		/* pixel aspect ratio */

extern int  psample;			/* pixel sample size */
extern double  maxdiff;			/* max. sample difference */
extern double  dstrpix;			/* square pixel distribution */

extern double  dstrsrc;			/* square source distribution */
extern double  shadthresh;		/* shadow threshold */
extern double  shadcert;		/* shadow testing certainty */
extern int  directrelay;		/* number of source relays */
extern int  vspretest;			/* virtual source pretest density */
extern int  directinvis;		/* light sources invisible to eye? */
extern double  srcsizerat;		/* maximum source size/dist. ratio */

extern double  specthresh;		/* specular sampling threshold */
extern double  specjitter;		/* specular sampling jitter */

extern int  maxdepth;			/* maximum recursion depth */
extern double  minweight;		/* minimum ray weight */

extern COLOR  ambval;			/* ambient value */
extern double  ambacc;			/* ambient accuracy */
extern int  ambres;			/* ambient resolution */
extern int  ambdiv;			/* ambient divisions */
extern int  ambssamp;			/* ambient super-samples */
extern int  ambounce;			/* ambient bounces */
extern char  *amblist[];		/* ambient include/exclude list */
extern int  ambincl;			/* include == 1, exclude == 0 */


main(argc, argv)
int  argc;
char  *argv[];
{
#define	 check(ol,al)		if (argv[i][ol] || \
				badarg(argc-i-1,argv+i+1,al)) \
				goto badopt
#define	 bool(olen,var)		switch (argv[i][olen]) { \
				case '\0': var = !var; break; \
				case 'y': case 'Y': case 't': case 'T': \
				case '+': case '1': var = 1; break; \
				case 'n': case 'N': case 'f': case 'F': \
				case '-': case '0': var = 0; break; \
				default: goto badopt; }
	char  *err;
	char  *recover = NULL;
	char  *outfile = NULL;
	char  *zfile = NULL;
	char  *errfile = NULL;
	char  *ambfile = NULL;
	char  **amblp = amblist;
	int  loadflags = ~IO_FILES;
	int  seqstart = 0;
	int  rval;
	int  i;
					/* record start time */
	tstart = time((long *)0);
					/* global program name */
	progname = argv[0] = fixargv0(argv[0]);
					/* get library path */
	if ((libpath = getenv(ULIBVAR)) == NULL)
		libpath = DEFPATH;
					/* initialize object types */
	initotypes();
					/* initialize urand */
	initurand(2048);
					/* option city */
	for (i = 1; i < argc; i++) {
						/* expand arguments */
		while (rval = expandarg(&argc, &argv, i))
			if (rval < 0) {
				sprintf(errmsg, "cannot expand '%s'", argv[i]);
				error(SYSTEM, errmsg);
			}
		if (argv[i][0] != '-')
			break;			/* break from options */
		if (!strcmp(argv[i], "-version")) {
			puts(VersionID);
			quit(0);
		}
		if (!strcmp(argv[i], "-defaults") ||
				!strcmp(argv[i], "-help")) {
			printdefaults();
			quit(0);
		}
#if  RVIEW
		if (!strcmp(argv[i], "-devices")) {
			printdevices();
			quit(0);
		}
#endif
#if  RPICT|RVIEW
		rval = getviewopt(&ourview, argc-i, argv+i);
		if (rval >= 0) {
			i += rval;
			continue;
		}
#endif
		switch (argv[i][1]) {
#if  RPICT|RVIEW
		case 'v':				/* view file */
			if (argv[i][2] != 'f')
				goto badopt;
			check(3,"s");
			rval = viewfile(argv[++i], &ourview, NULL);
			if (rval < 0) {
				sprintf(errmsg,
				"cannot open view file \"%s\"",
						argv[i]);
				error(SYSTEM, errmsg);
			} else if (rval == 0) {
				sprintf(errmsg,
					"bad view file \"%s\"",
						argv[i]);
				error(USER, errmsg);
			}
			break;
#endif
		case 'd':				/* direct */
			switch (argv[i][2]) {
			case 't':				/* threshold */
				check(3,"f");
				shadthresh = atof(argv[++i]);
				break;
			case 'c':				/* certainty */
				check(3,"f");
				shadcert = atof(argv[++i]);
				break;
			case 'j':				/* jitter */
				check(3,"f");
				dstrsrc = atof(argv[++i]);
				break;
			case 'r':				/* relays */
				check(3,"i");
				directrelay = atoi(argv[++i]);
				break;
			case 'p':				/* pretest */
				check(3,"i");
				vspretest = atoi(argv[++i]);
				break;
			case 'i':				/* invis. */
				bool(3,directinvis);
				break;
			case 's':				/* size */
				check(3,"f");
				srcsizerat = atof(argv[++i]);
				break;
			default:
				goto badopt;
			}
			break;
		case 's':				/* specular */
			switch (argv[i][2]) {
			case 't':				/* threshold */
				check(3,"f");
				specthresh = atof(argv[++i]);
				break;
			case 'j':				/* jitter */
				check(3,"f");
				specjitter = atof(argv[++i]);
				break;
			default:
				goto badopt;
			}
			break;
#if  RPICT|RVIEW
		case 'p':				/* pixel */
			switch (argv[i][2]) {
			case 's':				/* sample */
				check(3,"i");
				psample = atoi(argv[++i]);
				break;
			case 't':				/* threshold */
				check(3,"f");
				maxdiff = atof(argv[++i]);
				break;
#if  RPICT
			case 'j':				/* jitter */
				check(3,"f");
				dstrpix = atof(argv[++i]);
				break;
			case 'a':				/* aspect */
				check(3,"f");
				pixaspect = atof(argv[++i]);
				break;
#endif
			default:
				goto badopt;
			}
			break;
#endif
#if  RPICT|RTRACE
		case 'x':				/* x resolution */
			check(2,"i");
			hresolu = atoi(argv[++i]);
			break;
		case 'y':				/* y resolution */
			check(2,"i");
			vresolu = atoi(argv[++i]);
			break;
#endif
		case 'w':				/* warnings */
			rval = wrnvec != NULL;
			bool(2,rval);
			if (rval) wrnvec = stderr_v;
			else wrnvec = NULL;
			break;
		case 'e':				/* error file */
			check(2,"s");
			errfile = argv[++i];
			break;
		case 'l':				/* limit */
			switch (argv[i][2]) {
			case 'r':				/* recursion */
				check(3,"i");
				maxdepth = atoi(argv[++i]);
				break;
			case 'w':				/* weight */
				check(3,"f");
				minweight = atof(argv[++i]);
				break;
			default:
				goto badopt;
			}
			break;
		case 'i':				/* irradiance */
			bool(2,do_irrad);
			break;
#if  RPICT
		case 'S':				/* slave index */
			check(2,"i");
			seqstart = atoi(argv[++i]);
			break;
		case 'o':				/* output file */
			check(2,"s");
			outfile = argv[++i];
			break;
		case 'z':				/* z file */
			check(2,"s");
			zfile = argv[++i];
			break;
		case 'r':				/* recover file */
			check(2,"s");
			recover = argv[++i];
			break;
		case 't':				/* timer */
			check(2,"i");
			ralrm = atoi(argv[++i]);
			break;
#endif
		case 'a':				/* ambient */
			switch (argv[i][2]) {
			case 'v':				/* value */
				check(3,"fff");
				setcolor(ambval, atof(argv[i+1]),
						atof(argv[i+2]),
						atof(argv[i+3]));
				i += 3;
				break;
			case 'a':				/* accuracy */
				check(3,"f");
				ambacc = atof(argv[++i]);
				break;
			case 'r':				/* resolution */
				check(3,"i");
				ambres = atoi(argv[++i]);
				break;
			case 'd':				/* divisions */
				check(3,"i");
				ambdiv = atoi(argv[++i]);
				break;
			case 's':				/* super-samp */
				check(3,"i");
				ambssamp = atoi(argv[++i]);
				break;
			case 'b':				/* bounces */
				check(3,"i");
				ambounce = atoi(argv[++i]);
				break;
			case 'i':				/* include */
			case 'I':
				check(3,"s");
				if (ambincl != 1) {
					ambincl = 1;
					amblp = amblist;
				}
				if (argv[i][2] == 'I') {	/* file */
					rval = wordfile(amblp,
					getpath(argv[++i],libpath,R_OK));
					if (rval < 0) {
						sprintf(errmsg,
				"cannot open ambient include file \"%s\"",
								argv[i]);
						error(SYSTEM, errmsg);
					}
					amblp += rval;
				} else
					*amblp++ = argv[++i];
				break;
			case 'e':				/* exclude */
			case 'E':
				check(3,"s");
				if (ambincl != 0) {
					ambincl = 0;
					amblp = amblist;
				}
				if (argv[i][2] == 'E') {	/* file */
					rval = wordfile(amblp,
					getpath(argv[++i],libpath,R_OK));
					if (rval < 0) {
						sprintf(errmsg,
				"cannot open ambient exclude file \"%s\"",
								argv[i]);
						error(SYSTEM, errmsg);
					}
					amblp += rval;
				} else
					*amblp++ = argv[++i];
				break;
			case 'f':				/* file */
				check(3,"s");
				ambfile= argv[++i];
				break;
			default:
				goto badopt;
			}
			break;
#if  RTRACE
		case 'I':				/* immed. irradiance */
			bool(2,imm_irrad);
			break;
		case 'f':				/* format i/o */
			switch (argv[i][2]) {
			case 'a':				/* ascii */
			case 'f':				/* float */
			case 'd':				/* double */
				inform = argv[i][2];
				break;
			default:
				goto badopt;
			}
			switch (argv[i][3]) {
			case '\0':
				outform = inform;
				break;
			case 'a':				/* ascii */
			case 'f':				/* float */
			case 'd':				/* double */
			case 'c':				/* color */
				check(4,"");
				outform = argv[i][3];
				break;
			default:
				goto badopt;
			}
			break;
		case 'o':				/* output */
			outvals = argv[i]+2;
			break;
		case 'h':				/* header output */
			rval = loadflags & IO_INFO;
			bool(2,rval);
			loadflags = rval ? loadflags | IO_INFO :
					loadflags & ~IO_INFO;
			break;
#endif
#if  RVIEW
		case 'b':				/* black and white */
			bool(2,greyscale);
			break;
		case 'o':				/* output device */
			check(2,"s");
			devname = argv[++i];
			break;
#endif
		default:
			goto badopt;
		}
	}
	*amblp = NULL;
#if  RPICT|RVIEW
	err = setview(&ourview);	/* set viewing parameters */
	if (err != NULL)
		error(USER, err);
#endif
					/* set up signal handling */
	sigdie(SIGINT, "Interrupt");
	sigdie(SIGHUP, "Hangup");
	sigdie(SIGTERM, "Terminate");
	sigdie(SIGPIPE, "Broken pipe");
	sigdie(SIGALRM, "Alarm clock");
#ifdef	SIGXCPU
	sigdie(SIGXCPU, "CPU limit exceeded");
	sigdie(SIGXFSZ, "File size exceeded");
#endif
					/* open error file */
	if (errfile != NULL) {
		if (freopen(errfile, "a", stderr) == NULL)
			quit(2);
		fprintf(stderr, "**************\n*** PID %5d: ",
				getpid());
		printargs(argc, argv, stderr);
		putc('\n', stderr);
		fflush(stderr);
	}
#ifdef	NICE
	nice(NICE);			/* lower priority */
#endif
					/* get octree */
#if  RVIEW
	loadflags &= ~IO_INFO;
#endif
	if (i == argc)
		octname = NULL;
	else if (i == argc-1)
		octname = argv[i];
	else
		goto badopt;
	if (
#if  RPICT
		seqstart > 0 &&
#endif
			octname == NULL)
		error(USER, "missing octree argument");
#if  RPICT
	if (outfile != NULL)
		openheader();
#endif
#ifdef	MSDOS
#if  RTRACE
	if (outform != 'a')
#endif
	setmode(fileno(stdout), O_BINARY);
	if (octname == NULL)
		setmode(fileno(stdin), O_BINARY);
#endif
	readoct(octname, loadflags, &thescene, NULL);
	nsceneobjs = nobjects;

	if (loadflags & IO_INFO) {	/* print header */
		printargs(i, argv, stdout);
		printf("SOFTWARE= %s\n", VersionID);
#if  RTRACE
		fputformat(formstr(outform), stdout);
		putchar('\n');
#endif
	}

	marksources();			/* find and mark sources */

	setambient(ambfile);		/* initialize ambient calculation */

#if  RPICT
	rpict(seqstart, outfile, zfile, recover);
#endif
#if  RTRACE
	rtrace(NULL);
#endif
#if  RVIEW
	rview();
#endif
	quit(0);

badopt:
	sprintf(errmsg, "command line error at '%s'", argv[i]);
	error(USER, errmsg);

#undef	check
#undef	bool
}


eputs(s)				/* error output */
char  *s;
{
	if (errvec != NULL)
		(*errvec)(s);
}


wputs(s)				/* warning output */
char  *s;
{
	int  lasterrno = errno;		/* save errno */

	if (wrnvec != NULL)
		(*wrnvec)(s);

	errno = lasterrno;
}


cputs(s)				/* command error output */
char  *s;
{
	if (cmdvec != NULL)
		(*cmdvec)(s);
}


stderr_v(s)				/* put string to stderr */
register char  *s;
{
	static int  midline = 0;

	if (!*s)
		return;
	if (!midline++) {
		fputs(progname, stderr);
		fputs(": ", stderr);
	}
	fputs(s, stderr);
	if (s[strlen(s)-1] == '\n') {
		fflush(stderr);
		midline = 0;
	}
}


onsig(signo)				/* fatal signal */
int  signo;
{
	static int  gotsig = 0;

	if (gotsig++)			/* two signals and we're gone! */
		_exit(signo);

	alarm(15);			/* allow 15 seconds to clean up */
	signal(SIGALRM, SIG_DFL);	/* make certain we do die */
	eputs("signal - ");
	eputs(sigerr[signo]);
	eputs("\n");
	quit(3);
}


sigdie(signo, msg)			/* set fatal signal */
int  signo;
char  *msg;
{
	if (signal(signo, onsig) == SIG_IGN)
		signal(signo, SIG_IGN);
	sigerr[signo] = msg;
}


printdefaults()			/* print default values to stdout */
{
	register char  *cp;

#if  RTRACE
	if (imm_irrad)
		printf("-I+\t\t\t\t# immediate irradiance on\n");
	else
#endif
	printf(do_irrad ? "-i+\t\t\t\t# irradiance calculation on\n" :
			"-i-\t\t\t\t# irradiance calculation off\n");
#if  RVIEW
	printf(greyscale ? "-b+\t\t\t\t# greyscale on\n" :
			"-b-\t\t\t\t# greyscale off\n");
#endif
#if  RPICT|RVIEW
	printf("-vt%c\t\t\t\t# view type %s\n", ourview.type,
			ourview.type==VT_PER ? "perspective" :
			ourview.type==VT_PAR ? "parallel" :
			ourview.type==VT_HEM ? "hemispherical" :
			ourview.type==VT_ANG ? "angular" :
			"unknown");
	printf("-vp %f %f %f\t# view point\n",
			ourview.vp[0], ourview.vp[1], ourview.vp[2]);
	printf("-vd %f %f %f\t# view direction\n",
			ourview.vdir[0], ourview.vdir[1], ourview.vdir[2]);
	printf("-vu %f %f %f\t# view up\n",
			ourview.vup[0], ourview.vup[1], ourview.vup[2]);
	printf("-vh %f\t\t\t# view horizontal size\n", ourview.horiz);
	printf("-vv %f\t\t\t# view vertical size\n", ourview.vert);
	printf("-vs %f\t\t\t# view shift\n", ourview.hoff);
	printf("-vl %f\t\t\t# view lift\n", ourview.voff);
#endif
#if  RPICT|RTRACE
	printf("-x  %-9d\t\t\t# x resolution\n", hresolu);
	printf("-y  %-9d\t\t\t# y resolution\n", vresolu);
#endif
#if  RPICT
	printf("-pa %f\t\t\t# pixel aspect ratio\n", pixaspect);
	printf("-pj %f\t\t\t# pixel jitter\n", dstrpix);
#endif
#if  RPICT|RVIEW
	printf("-ps %-9d\t\t\t# pixel sample\n", psample);
	printf("-pt %f\t\t\t# pixel threshold\n", maxdiff);
#endif
	printf("-dt %f\t\t\t# direct threshold\n", shadthresh);
	printf("-dc %f\t\t\t# direct certainty\n", shadcert);
	printf("-dj %f\t\t\t# direct jitter\n", dstrsrc);
	printf("-ds %f\t\t\t# direct sampling\n", srcsizerat);
	printf("-dr %-9d\t\t\t# direct relays\n", directrelay);
	printf("-dp %-9d\t\t\t# direct pretest density\n", vspretest);
	printf(directinvis ? "-di+\t\t\t\t# direct invisibility on\n" :
			"-di-\t\t\t\t# direct invisibility off\n");
	printf("-sj %f\t\t\t# specular jitter\n", specjitter);
	printf("-st %f\t\t\t# specular threshold\n", specthresh);
	printf("-av %f %f %f\t# ambient value\n", colval(ambval,RED),
			colval(ambval,GRN), colval(ambval, BLU));
	printf("-ab %-9d\t\t\t# ambient bounces\n", ambounce);
	printf("-aa %f\t\t\t# ambient accuracy\n", ambacc);
	printf("-ar %-9d\t\t\t# ambient resolution\n", ambres);
	printf("-ad %-9d\t\t\t# ambient divisions\n", ambdiv);
	printf("-as %-9d\t\t\t# ambient super-samples\n", ambssamp);
	printf("-lr %-9d\t\t\t# limit reflection\n", maxdepth);
	printf("-lw %f\t\t\t# limit weight\n", minweight);
#if  RPICT
	printf("-t  %-9d\t\t\t# time between reports\n", ralrm);
#endif
#if  RVIEW
	printf("-o %s\t\t\t\t# output device\n", devname);
#endif
#if  RTRACE
	printf("-f%c%c\t\t\t\t# format input/output = %s/%s\n",
			inform, outform, formstr(inform), formstr(outform));
	printf("-o%s\t\t\t\t# output", outvals);
	for (cp = outvals; *cp; cp++)
		switch (*cp) {
		case 't': printf(" trace"); break;
		case 'o': printf(" origin"); break;
		case 'd': printf(" direction"); break;
		case 'v': printf(" value"); break;
		case 'l': printf(" length"); break;
		case 'p': printf(" point"); break;
		case 'n': printf(" normal"); break;
		case 's': printf(" surface"); break;
		case 'w': printf(" weight"); break;
		case 'm': printf(" modifier"); break;
		}
	putchar('\n');
#endif
	printf(wrnvec != NULL ? "-w+\t\t\t\t# warning messages on\n" :
			"-w-\t\t\t\t# warning messages off\n");
}
