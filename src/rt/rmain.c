/* Copyright (c) 1996 Regents of the University of California */

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

#include  <sys/types.h>

#include  <signal.h>

#include  "view.h"

#include  "paths.h"
					/* persistent processes define */
#ifdef  F_SETLKW
#if  RPICT|RTRACE
#define  PERSIST	1		/* normal persist */
#define  PARALLEL	2		/* parallel persist */
#define  PCHILD		3		/* child of normal persist */
#endif
#endif

char  *progname;			/* argv[0] */

char  *octname;				/* octree name */

char  *getlibpath();			/* library directory list */

char  *sigerr[NSIG];			/* signal error messages */

char  *shm_boundary = NULL;		/* boundary of shared memory */

extern char  VersionID[];		/* version ID string */

extern int  stderr_v();			/* standard error output */
int  (*errvec)() = stderr_v;		/* error output vector */
int  (*wrnvec)() = stderr_v;		/* warning output vector */
int  (*cmdvec)() = NULL;		/* command error vector */

int  (*trace)() = NULL;			/* trace call */
int  do_irrad = 0;			/* compute irradiance? */

char  *errfile = NULL;			/* error output file */

extern time_t  time();
time_t  tstart;				/* start time */

extern int  ambnotify();		/* new object notify functions */
#if  RTRACE
extern int  tranotify();
int  (*addobjnotify[])() = {ambnotify, tranotify, NULL};
extern char  *tralist[];		/* trace include/exclude list */
extern int  traincl;			/* include == 1, exclude == 0 */
#else
int  (*addobjnotify[])() = {ambnotify, NULL};
#endif

CUBE  thescene;				/* our scene */
OBJECT	nsceneobjs;			/* number of objects in our scene */

extern unsigned long  raynum, nrays;	/* ray counts */

extern int  imm_irrad;			/* calculate immediate irradiance? */

extern int  ralrm;			/* seconds between reports */

extern int  greyscale;			/* map colors to brightness? */
extern char  *devname;			/* output device name */
extern double  exposure;		/* exposure compensation */

extern char  *formstr();		/* string from format */
extern int  inform;			/* input format */
extern int  outform;			/* output format */
extern char  *outvals;			/* output values */

extern VIEW  ourview;			/* viewing parameters */

extern char  rifname[];			/* rad input file name */

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
extern int  directvis;			/* light sources visible to eye? */
extern double  srcsizerat;		/* maximum source size/dist. ratio */

extern double  specthresh;		/* specular sampling threshold */
extern double  specjitter;		/* specular sampling jitter */

extern COLOR  cextinction;		/* global extinction coefficient */
extern COLOR  salbedo;			/* global scattering albedo */
extern double  seccg;			/* global scattering eccentricity */
extern double  ssampdist;		/* scatter sampling distance */

extern int  backvis;			/* back face visibility */

extern int  maxdepth;			/* maximum recursion depth */
extern double  minweight;		/* minimum ray weight */

extern COLOR  ambval;			/* ambient value */
extern int  ambvwt;			/* initial weight for ambient value */
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
	char  *ambfile = NULL;
	int  loadflags = ~IO_FILES;
	int  seqstart = 0;
	int  persist = 0;
	char  **amblp;
	char  **tralp;
	int  duped1;
	int  rval;
	int  i;
					/* record start time */
	tstart = time((time_t *)NULL);
					/* global program name */
	progname = argv[0] = fixargv0(argv[0]);
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
		if (argv[i] == NULL || argv[i][0] != '-')
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
		case 'b':				/* back face vis. */
			if (argv[i][2] == 'v') {
				bool(3,backvis);
				break;
			}
#if  RVIEW
			bool(2,greyscale);
			break;
#else
			goto badopt;
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
			case 'v':				/* visibility */
				bool(3,directvis);
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
#if  RVIEW
			case 'e':				/* exposure */
				check(3,"f");
				exposure = atof(argv[++i]);
				if (argv[i][0] == '+' || argv[i][0] == '-')
					exposure = pow(2.0, exposure);
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
			if (argv[i][2] == 'o') {		/* +output */
				check(3,"s");
				outfile = argv[i+1];
			} else
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
			case 'w':				/* weight */
				check(3,"i");
				ambvwt = atoi(argv[++i]);
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
					getpath(argv[++i],getlibpath(),R_OK));
					if (rval < 0) {
						sprintf(errmsg,
				"cannot open ambient include file \"%s\"",
								argv[i]);
						error(SYSTEM, errmsg);
					}
					amblp += rval;
				} else {
					*amblp++ = argv[++i];
					*amblp = NULL;
				}
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
					getpath(argv[++i],getlibpath(),R_OK));
					if (rval < 0) {
						sprintf(errmsg,
				"cannot open ambient exclude file \"%s\"",
								argv[i]);
						error(SYSTEM, errmsg);
					}
					amblp += rval;
				} else {
					*amblp++ = argv[++i];
					*amblp = NULL;
				}
				break;
			case 'f':				/* file */
				check(3,"s");
				ambfile= argv[++i];
				break;
			default:
				goto badopt;
			}
			break;
		case 'm':				/* medium */
			switch (argv[i][2]) {
			case 'e':				/* extinction */
				check(3,"fff");
				setcolor(cextinction, atof(argv[i+1]),
						atof(argv[i+2]),
						atof(argv[i+3]));
				i += 3;
				break;
			case 'a':				/* albedo */
				check(3,"fff");
				setcolor(salbedo, atof(argv[i+1]),
						atof(argv[i+2]),
						atof(argv[i+3]));
				i += 3;
				break;
			case 'g':				/* eccentr. */
				check(3,"f");
				seccg = atof(argv[++i]);
				break;
			case 's':				/* sampling */
				check(3,"f");
				ssampdist = atof(argv[++i]);
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
		case 't':				/* trace */
			switch (argv[i][2]) {
			case 'i':				/* include */
			case 'I':
				check(3,"s");
				if (traincl != 1) {
					traincl = 1;
					tralp = tralist;
				}
				if (argv[i][2] == 'I') {	/* file */
					rval = wordfile(tralp,
					getpath(argv[++i],getlibpath(),R_OK));
					if (rval < 0) {
						sprintf(errmsg,
				"cannot open trace include file \"%s\"",
								argv[i]);
						error(SYSTEM, errmsg);
					}
					tralp += rval;
				} else {
					*tralp++ = argv[++i];
					*tralp = NULL;
				}
				break;
			case 'e':				/* exclude */
			case 'E':
				check(3,"s");
				if (traincl != 0) {
					traincl = 0;
					tralp = tralist;
				}
				if (argv[i][2] == 'E') {	/* file */
					rval = wordfile(tralp,
					getpath(argv[++i],getlibpath(),R_OK));
					if (rval < 0) {
						sprintf(errmsg,
				"cannot open trace exclude file \"%s\"",
								argv[i]);
						error(SYSTEM, errmsg);
					}
					tralp += rval;
				} else {
					*tralp++ = argv[++i];
					*tralp = NULL;
				}
				break;
			default:
				goto badopt;
			}
			break;
#endif
#if  RVIEW
		case 'o':				/* output device */
			check(2,"s");
			devname = argv[++i];
			break;
		case 'R':				/* render input file */
			check(2,"s");
			strcpy(rifname, argv[++i]);
			break;
#endif
#ifdef  PERSIST
		case 'P':				/* persist file */
			if (argv[i][2] == 'P') {
				check(3,"s");
				persist = PARALLEL;
			} else {
				check(2,"s");
				persist = PERSIST;
			}
			persistfile(argv[++i]);
			break;
#endif
		default:
			goto badopt;
		}
	}
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
					/* set up output */
#ifdef  PERSIST
	if (persist) {
#if  RPICT
		if (recover != NULL)
			error(USER, "persist option used with recover file");
		if (seqstart <= 0)
			error(USER, "persist option only for sequences");
		if (outfile == NULL)
#endif
		duped1 = dup(fileno(stdout));	/* don't lose our output */
		openheader();
	}
#if  RPICT
	else
#endif
#endif
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

#ifdef  PERSIST
	if (persist) {
		fflush(stdout);
		if (outfile == NULL) {		/* reconnect stdout */
			dup2(duped1, fileno(stdout));
			close(duped1);
		}
		if (persist == PARALLEL) {	/* multiprocessing */
			preload_objs();		/* preload scene */
			strcpy(shm_boundary=bmalloc(16), "SHM_BOUNDARY");
			while ((rval=fork()) == 0) {	/* keep on forkin' */
				pflock(1);
				pfhold();
				tstart = time((time_t *)NULL);
			}
			if (rval < 0)
				error(SYSTEM, "cannot fork child for persist function");
			pfdetach();		/* parent exits */
		}
	}
runagain:
	if (persist)
		if (outfile == NULL)			/* if out to stdout */
			dupheader();			/* send header */
		else					/* if out to file */
			duped1 = dup(fileno(stdout));	/* hang onto pipe */
#endif
#if  RPICT
	rpict(seqstart, outfile, zfile, recover);
#endif
#if  RTRACE
	rtrace(NULL);
#endif
#if  RVIEW
	rview();
#endif
	ambsync();			/* flush ambient file */
#ifdef  PERSIST
	if (persist == PERSIST) {	/* first run-through */
		if ((rval=fork()) == 0) {	/* child loops until killed */
			pflock(1);
			persist = PCHILD;
		} else {			/* original process exits */
			if (rval < 0)
				error(SYSTEM, "cannot fork child for persist function");
			pfdetach();		/* parent exits */
		}
	}
	if (persist == PCHILD) {	/* wait for a signal then go again */
		if (outfile != NULL)
			close(duped1);		/* release output handle */
		pfhold();
		tstart = time((time_t *)NULL);	/* reinitialize */
		raynum = nrays = 0;
		goto runagain;
	}
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
			ourview.type==VT_CYL ? "cylindrical" :
			"unknown");
	printf("-vp %f %f %f\t# view point\n",
			ourview.vp[0], ourview.vp[1], ourview.vp[2]);
	printf("-vd %f %f %f\t# view direction\n",
			ourview.vdir[0], ourview.vdir[1], ourview.vdir[2]);
	printf("-vu %f %f %f\t# view up\n",
			ourview.vup[0], ourview.vup[1], ourview.vup[2]);
	printf("-vh %f\t\t\t# view horizontal size\n", ourview.horiz);
	printf("-vv %f\t\t\t# view vertical size\n", ourview.vert);
	printf("-vo %f\t\t\t# view fore clipping plane\n", ourview.vfore);
	printf("-va %f\t\t\t# view aft clipping plane\n", ourview.vaft);
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
#if  RVIEW
	printf("-pe %f\t\t\t# pixel exposure\n", exposure);
#endif
#if  RPICT|RVIEW
	printf("-ps %-9d\t\t\t# pixel sample\n", psample);
	printf("-pt %f\t\t\t# pixel threshold\n", maxdiff);
#endif
	printf(backvis ? "-bv+\t\t\t\t# back face visibility on\n" :
			"-bv-\t\t\t\t# back face visibility off\n");
	printf("-dt %f\t\t\t# direct threshold\n", shadthresh);
	printf("-dc %f\t\t\t# direct certainty\n", shadcert);
	printf("-dj %f\t\t\t# direct jitter\n", dstrsrc);
	printf("-ds %f\t\t\t# direct sampling\n", srcsizerat);
	printf("-dr %-9d\t\t\t# direct relays\n", directrelay);
	printf("-dp %-9d\t\t\t# direct pretest density\n", vspretest);
	printf(directvis ? "-dv+\t\t\t\t# direct visibility on\n" :
			"-dv-\t\t\t\t# direct visibility off\n");
	printf("-sj %f\t\t\t# specular jitter\n", specjitter);
	printf("-st %f\t\t\t# specular threshold\n", specthresh);
	printf("-av %f %f %f\t# ambient value\n", colval(ambval,RED),
			colval(ambval,GRN), colval(ambval, BLU));
	printf("-aw %-9d\t\t\t# ambient value weight\n", ambvwt);
	printf("-ab %-9d\t\t\t# ambient bounces\n", ambounce);
	printf("-aa %f\t\t\t# ambient accuracy\n", ambacc);
	printf("-ar %-9d\t\t\t# ambient resolution\n", ambres);
	printf("-ad %-9d\t\t\t# ambient divisions\n", ambdiv);
	printf("-as %-9d\t\t\t# ambient super-samples\n", ambssamp);
	printf("-me %.2e %.2e %.2e\t# extinction coefficient\n",
			colval(cextinction,RED),
			colval(cextinction,GRN),
			colval(cextinction,BLU));
	printf("-ma %f %f %f\t# scattering albedo\n", colval(salbedo,RED),
			colval(salbedo,GRN), colval(salbedo,BLU));
	printf("-mg %f\t\t\t# scattering eccentricity\n", seccg);
	printf("-ms %f\t\t\t# mist sampling distance\n", ssampdist);
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
		case 'L': printf(" first_length"); break;
		case 'p': printf(" point"); break;
		case 'n': printf(" normal"); break;
		case 'N': printf(" unperturbed_normal"); break;
		case 's': printf(" surface"); break;
		case 'w': printf(" weight"); break;
		case 'm': printf(" modifier"); break;
		}
	putchar('\n');
#endif
	printf(wrnvec != NULL ? "-w+\t\t\t\t# warning messages on\n" :
			"-w-\t\t\t\t# warning messages off\n");
}
