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
#define  ULIBVAR	"RAYPATH"

char  *progname;			/* argv[0] */

char  *libpath;				/* library directory list */

char  *sigerr[NSIG];			/* signal error messages */

extern int  stderr_v();			/* standard error output */
int  (*errvec)() = stderr_v;		/* error output vector */
int  (*wrnvec)() = stderr_v;		/* warning output vector */
int  (*cmdvec)() = NULL;		/* command error vector */

int  (*trace)() = NULL;			/* trace call */

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

extern int  psample;			/* pixel sample size */
extern double  maxdiff;			/* max. sample difference */
extern double  dstrpix;			/* square pixel distribution */

extern double  dstrsrc;			/* square source distribution */
extern double  shadthresh;		/* shadow threshold */

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
#define  check(c,n)	if (argv[i][c] || n >= argc-i) goto badopt
	double  atof();
	char  *getenv();
	int  report();
	char  *err;
	char  *recover = NULL;
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
		if (!strcmp(argv[i], "-defaults")) {
			printdefaults();
			quit(0);
		}
#if  RVIEW
		if (!strcmp(argv[i], "-devices")) {
			printdevices();
			quit(0);
		}
#endif
		switch (argv[i][1]) {
#if  RPICT|RVIEW
		case 'v':				/* view */
			switch (argv[i][2]) {
			case 't':				/* type */
				check(4,0);
				ourview.type = argv[i][3];
				break;
			case 'p':				/* point */
				check(3,3);
				ourview.vp[0] = atof(argv[++i]);
				ourview.vp[1] = atof(argv[++i]);
				ourview.vp[2] = atof(argv[++i]);
				break;
			case 'd':				/* direction */
				check(3,3);
				ourview.vdir[0] = atof(argv[++i]);
				ourview.vdir[1] = atof(argv[++i]);
				ourview.vdir[2] = atof(argv[++i]);
				break;
			case 'u':				/* up */
				check(3,3);
				ourview.vup[0] = atof(argv[++i]);
				ourview.vup[1] = atof(argv[++i]);
				ourview.vup[2] = atof(argv[++i]);
				break;
			case 'h':				/* horizontal */
				check(3,1);
				ourview.horiz = atof(argv[++i]);
				break;
			case 'v':				/* vertical */
				check(3,1);
				ourview.vert = atof(argv[++i]);
				break;
			case 'f':				/* file */
				check(3,1);
				rval = viewfile(argv[++i], &ourview);
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
			default:
				goto badopt;
			}
			break;
#endif
		case 'd':				/* direct */
			switch (argv[i][2]) {
			case 't':				/* tolerance */
				check(3,1);
				shadthresh = atof(argv[++i]);
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
			case 'd':				/* difference */
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
		case 'x':				/* x resolution */
			check(2,1);
			ourview.hresolu = atoi(argv[++i]);
			break;
		case 'y':				/* y resolution */
			check(2,1);
			ourview.vresolu = atoi(argv[++i]);
			break;
#endif
#if  RTRACE
		case 'x':				/* x resolution */
			check(2,1);
			hresolu = atoi(argv[++i]);
			break;
		case 'y':				/* y resolution */
			check(2,1);
			vresolu = atoi(argv[++i]);
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
		case 'r':				/* recover file */
			check(2,1);
			recover = argv[++i];
			rval = viewfile(recover, &ourview);
			if (rval <= 0) {
				sprintf(errmsg,
			"cannot recover view parameters from \"%s\"", recover);
				error(WARNING, errmsg);
			} else
				gotvfile += rval;
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
badopt:
			sprintf(errmsg, "bad option: '%s'", argv[i]);
			error(USER, errmsg);
			break;
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
			quit(1);
		fprintf(stderr, "**************\n*** PID %5d: ",
				getpid());
		printargs(argc, argv, stderr);
		fputs("\n", stderr);
		fflush(stderr);
	}
#ifdef  NICE
	nice(NICE);			/* lower priority */
#endif
#if  RVIEW
	loadflags &= ~IO_INFO;
#endif
#if  RPICT
	if (i == argc)
		readoct(NULL, loadflags, &thescene, NULL);
	else
#endif
	if (i == argc-1)
		readoct(argv[i], loadflags, &thescene, NULL);
	else
		error(USER, "single octree required");

	if (loadflags & IO_INFO) {	/* print header */
		printargs(i, argv, stdout);
#if  RPICT
		if (gotvfile) {
			printf(VIEWSTR);
			fprintview(&ourview, stdout);
			printf("\n");
		}
#endif
		printf("\n");
	}

	marksources();			/* find and mark sources */

	*amblp = NULL;			/* initialize ambient calculation */
	setambient(ambfile);

#if  RPICT
	if (ralrm > 0)			/* report init time */
		report();
	render(recover);		/* render the scene */
#endif
#if  RTRACE
	rtrace(NULL);			/* trace rays from stdin */
#endif
#if  RVIEW
	rview();			/* go */
#endif
	quit(0);
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
	static int  inline = 0;

	if (!inline++) {
		fputs(progname, stderr);
		fputs(": ", stderr);
	}
	fputs(s, stderr);
	if (*s && s[strlen(s)-1] == '\n') {
		fflush(stderr);
		inline = 0;
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
	quit(1);
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
			"unknown");
	printf("-vp %f %f %f\t# view point\n",
			ourview.vp[0], ourview.vp[1], ourview.vp[2]);
	printf("-vd %f %f %f\t# view direction\n", 
			ourview.vdir[0], ourview.vdir[1], ourview.vdir[2]);
	printf("-vu %f %f %f\t# view up\n", 
			ourview.vup[0], ourview.vup[1], ourview.vup[2]);
	printf("-vh %f\t\t\t# view horizontal size\n", ourview.horiz);
	printf("-vv %f\t\t\t# view vertical size\n", ourview.vert);
	printf("-x  %-9d\t\t\t# x resolution\n", ourview.hresolu);
	printf("-y  %-9d\t\t\t# y resolution\n", ourview.vresolu);
#endif
#if  RTRACE
	printf("-x  %-9d\t\t\t# x resolution\n", hresolu);
	printf("-y  %-9d\t\t\t# y resolution\n", vresolu);
#endif
	printf("-dt %f\t\t\t# direct tolerance\n", shadthresh);
	printf("-dj %f\t\t\t# direct jitter\n", dstrsrc);
#if  RPICT|RVIEW
	printf("-sp %-9d\t\t\t# sample pixel\n", psample);
	printf("-sd %f\t\t\t# sample difference\n", maxdiff);
#if  RPICT
	printf("-sj %f\t\t\t# sample jitter\n", dstrpix);
#endif
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
