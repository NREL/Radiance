/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  rmain.c - main for ray tracing programs
 *
 *     3/24/87
 */

					/* for flaky pre-processors */
#ifndef  RPICT
#define  RPICT		0
#endif
#ifndef  RTRACE
#define  RTRACE		0
#endif
#ifndef  RVIEW
#define  RVIEW		0
#endif

#include  "ray.h"

#include  "octree.h"

#include  <signal.h>

#include  "view.h"

#ifndef  DEFPATH
#define  DEFPATH	":/usr/local/lib/ray"
#endif
#ifndef  ULIBVAR
#define  ULIBVAR	"RAYPATH"
#endif

char  *progname;			/* argv[0] */

char  *octname;				/* octree name */

char  *libpath;				/* library directory list */

char  *sigerr[NSIG];			/* signal error messages */

extern int  stderr_v();			/* standard error output */
int  (*errvec)() = stderr_v;		/* error output vector */
int  (*wrnvec)() = stderr_v;		/* warning output vector */
int  (*cmdvec)() = NULL;		/* command error vector */

int  (*trace)() = NULL;			/* trace call */

extern int  ambnotify();		/* new object notify functions */
int  (*addobjnotify[])() = {ambnotify, NULL};

CUBE  thescene;				/* our scene */

extern int  ralrm;			/* seconds between reports */
extern float  pctdone;			/* percentage done */

extern int  greyscale;			/* map colors to brightness? */
extern char  *devname;			/* output device name */

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

extern int  maxdepth;			/* maximum recursion depth */
extern double  minweight;		/* minimum ray weight */
extern long  nrays;			/* number of rays traced */

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
#define  check(olen,narg)	if (argv[i][olen] || narg >= argc-i) goto badopt
	double  atof();
	char  *getenv();
	int  report();
	char  *err;
	char  *recover = NULL;
	char  *zfile = NULL;
	char  *errfile = NULL;
	char  *ambfile = NULL;
	char  **amblp = amblist;
	int  loadflags = ~IO_FILES;
	int  rval, gotvfile = 0;
	int  i;
					/* global program name */
	progname = argv[0];
					/* get library path */
	if ((libpath = getenv(ULIBVAR)) == NULL)
		libpath = DEFPATH;
					/* option city */
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
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
			check(3,1);
			rval = viewfile(argv[++i], &ourview, 0, 0);
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
			} else
				gotvfile += rval;
			break;
#endif
		case 'd':				/* direct */
			switch (argv[i][2]) {
			case 't':				/* threshold */
				check(3,1);
				shadthresh = atof(argv[++i]);
				break;
			case 'c':				/* certainty */
				check(3,1);
				shadcert = atof(argv[++i]);
				break;
			case 'j':				/* jitter */
				check(3,1);
				dstrsrc = atof(argv[++i]);
				break;
			default:
				goto badopt;
			}
			break;
#if  RPICT|RVIEW
		case 's':				/* sample */
			switch (argv[i][2]) {
			case 'p':				/* pixel */
				check(3,1);
				psample = atoi(argv[++i]);
				break;
			case 't':				/* threshold */
				check(3,1);
				maxdiff = atof(argv[++i]);
				break;
#if  RPICT
			case 'j':				/* jitter */
				check(3,1);
				dstrpix = atof(argv[++i]);
				break;
#endif
			default:
				goto badopt;
			}
			break;
#endif
#if  RPICT|RTRACE
		case 'x':				/* x resolution */
			check(2,1);
			hresolu = atoi(argv[++i]);
			break;
		case 'y':				/* y resolution */
			check(2,1);
			vresolu = atoi(argv[++i]);
			break;
#endif
#if  RPICT	
		case 'p':				/* pixel aspect */
			check(2,1);
			pixaspect = atof(argv[++i]);
			break;
#endif
		case 'w':				/* warnings */
			check(2,0);
			wrnvec = wrnvec==NULL ? stderr_v : NULL;
			break;
		case 'e':				/* error file */
			check(2,1);
			errfile = argv[++i];
			break;
		case 'l':				/* limit */
			switch (argv[i][2]) {
			case 'r':				/* recursion */
				check(3,1);
				maxdepth = atoi(argv[++i]);
				break;
			case 'w':				/* weight */
				check(3,1);
				minweight = atof(argv[++i]);
				break;
			default:
				goto badopt;
			}
			break;
#if  RPICT
		case 'z':				/* z file */
			check(2,1);
			zfile = argv[++i];
			break;
		case 'r':				/* recover file */
			check(2,1);
			recover = argv[++i];
			rval = viewfile(recover, &ourview, &hresolu, &vresolu);
			if (rval <= 0) {
				sprintf(errmsg,
			"cannot recover view parameters from \"%s\"", recover);
				error(WARNING, errmsg);
			} else {
				gotvfile += rval;
				pixaspect = 0.0;
			}
			break;
		case 't':				/* timer */
			check(2,1);
			ralrm = atoi(argv[++i]);
			break;
#endif
		case 'a':				/* ambient */
			switch (argv[i][2]) {
			case 'v':				/* value */
				check(3,3);
				setcolor(ambval, atof(argv[i+1]),
						atof(argv[i+2]),
						atof(argv[i+3]));
				i += 3;
				break;
			case 'a':				/* accuracy */
				check(3,1);
				ambacc = atof(argv[++i]);
				break;
			case 'r':				/* resolution */
				check(3,1);
				ambres = atoi(argv[++i]);
				break;
			case 'd':				/* divisions */
				check(3,1);
				ambdiv = atoi(argv[++i]);
				break;
			case 's':				/* super-samp */
				check(3,1);
				ambssamp = atoi(argv[++i]);
				break;
			case 'b':				/* bounces */
				check(3,1);
				ambounce = atoi(argv[++i]);
				break;
			case 'i':				/* include */
				check(3,1);
				if (ambincl != 1) {
					ambincl = 1;
					amblp = amblist;
				}
				*amblp++ = argv[++i];
				break;
			case 'e':				/* exclude */
				check(3,1);
				if (ambincl != 0) {
					ambincl = 0;
					amblp = amblist;
				}
				*amblp++ = argv[++i];
				break;
			case 'f':				/* file */
				check(3,1);
				ambfile= argv[++i];
				break;
			default:
				goto badopt;
			}
			break;
#if  RTRACE
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
				check(4,0);
				outform = argv[i][3];
				break;
			default:
				goto badopt;
			}
			break;
		case 'o':				/* output */
			outvals = argv[i]+2;
			break;
		case 'h':				/* toggle header */
			check(2,0);
			loadflags ^= IO_INFO;
			break;
#endif
#if  RVIEW
		case 'b':				/* black and white */
			check(2,0);
			greyscale = !greyscale;
			break;
		case 'o':				/* output device */
			check(2,1);
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
#if  RPICT
	normaspect(viewaspect(&ourview), &pixaspect, &hresolu, &vresolu);
#endif
					/* set up signal handling */
	sigdie(SIGINT, "Interrupt");
	sigdie(SIGHUP, "Hangup");
	sigdie(SIGTERM, "Terminate");
	sigdie(SIGPIPE, "Broken pipe");
#ifdef  SIGXCPU
	sigdie(SIGXCPU, "CPU limit exceeded");
	sigdie(SIGXFSZ, "File size exceeded");
#endif
#if  RPICT
	signal(SIGALRM, report);
#else
	sigdie(SIGALRM, "Alarm clock");
#endif
					/* open error file */
	if (errfile != NULL) {
		if (freopen(errfile, "a", stderr) == NULL)
			quit(2);
		fprintf(stderr, "**************\n*** PID %5d: ",
				getpid());
		printargs(argc, argv, stderr);
		fputs("\n", stderr);
		fflush(stderr);
	}
#ifdef  NICE
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
#if  RVIEW|RTRACE
	if (octname == NULL)
		error(USER, "missing octree argument");
#endif
	readoct(octname, loadflags, &thescene, NULL);

	if (loadflags & IO_INFO) {	/* print header */
		printargs(i, argv, stdout);
#if  RPICT
		if (gotvfile) {
			printf(VIEWSTR);
			fprintview(&ourview, stdout);
			printf("\n");
		}
		if (pixaspect < .99 || pixaspect > 1.01)
			fputaspect(pixaspect, stdout);
#endif
		printf("\n");
	}

	marksources();			/* find and mark sources */

	setambient(ambfile);		/* initialize ambient calculation */

#if  RPICT
	if (ralrm > 0)			/* report init time */
		report();
	render(zfile, recover);		/* render the scene */
#endif
#if  RTRACE
	rtrace(NULL);			/* trace rays from stdin */
#endif
#if  RVIEW
	rview();			/* go */
#endif
	quit(0);

badopt:
	sprintf(errmsg, "command line error at '%s'", argv[i]);
	error(USER, errmsg);

#undef  check
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
	if (wrnvec != NULL)
		(*wrnvec)(s);
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
	printf("-p  %f\t\t\t# pixel aspect ratio\n", pixaspect);
#endif
	printf("-dt %f\t\t\t# direct threshold\n", shadthresh);
	printf("-dc %f\t\t\t# direct certainty\n", shadcert);
	printf("-dj %f\t\t\t# direct jitter\n", dstrsrc);
#if  RPICT|RVIEW
	printf("-sp %-9d\t\t\t# sample pixel\n", psample);
	printf("-st %f\t\t\t# sample threshold\n", maxdiff);
#endif
#if  RPICT
	printf("-sj %f\t\t\t# sample jitter\n", dstrpix);
#endif
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
			inform, outform, 
			inform=='a' ? "ascii" :
			inform=='f' ? "float" : "double",
			outform=='a' ? "ascii" :
			outform=='f' ? "float" : "double");
	printf("-o%s\t\t\t\t# output", outvals);
	for (cp = outvals; *cp; cp++)
		switch (*cp) {
		case 'i': printf(" irradiance"); break;
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
	printf("\n");
#endif
}
