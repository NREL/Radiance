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

int  diemask = 0;			/* signals we catch */
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
	double  atof();
	char  *getenv();
	int  report();
	char  *err;
	char  *recover = NULL;
	char  *errfile = NULL;
	char  *ambfile = NULL;
	char  **amblp = amblist;
	int  loadflags = ~IO_FILES;
	int  gotvfile = 0;
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
				ourview.type = argv[i][3];
				break;
			case 'p':				/* point */
				ourview.vp[0] = atof(argv[++i]);
				ourview.vp[1] = atof(argv[++i]);
				ourview.vp[2] = atof(argv[++i]);
				break;
			case 'd':				/* direction */
				ourview.vdir[0] = atof(argv[++i]);
				ourview.vdir[1] = atof(argv[++i]);
				ourview.vdir[2] = atof(argv[++i]);
				break;
			case 'u':				/* up */
				ourview.vup[0] = atof(argv[++i]);
				ourview.vup[1] = atof(argv[++i]);
				ourview.vup[2] = atof(argv[++i]);
				break;
			case 'h':				/* horizontal */
				ourview.horiz = atof(argv[++i]);
				break;
			case 'v':				/* vertical */
				ourview.vert = atof(argv[++i]);
				break;
			case 'f':				/* file */
				gotvfile = viewfile(argv[++i], &ourview);
				if (gotvfile < 0) {
					sprintf(errmsg,
					"cannot open view file \"%s\"",
							argv[i]);
					error(SYSTEM, errmsg);
				} else if (gotvfile == 0) {
					sprintf(errmsg,
						"bad view file \"%s\"",
							argv[i]);
					error(USER, errmsg);
				}
				break;
			default:
				goto unkopt;
			}
			break;
#endif
		case 'd':				/* distribute */
			switch (argv[i][2]) {
#if  RPICT
			case 'p':				/* pixel */
				dstrpix = atof(argv[++i]);
				break;
#endif
			case 's':				/* source */
				dstrsrc = atof(argv[++i]);
				break;
			default:
				goto unkopt;
			}
			break;
#if  RPICT|RVIEW
		case 's':				/* sample */
			switch (argv[i][2]) {
			case 'p':				/* pixel */
				psample = atoi(argv[++i]);
				break;
			case 'd':				/* difference */
				maxdiff = atof(argv[++i]);
				break;
			default:
				goto unkopt;
			}
			break;
		case 'x':				/* x resolution */
			ourview.hresolu = atoi(argv[++i]);
			break;
		case 'y':				/* y resolution */
			ourview.vresolu = atoi(argv[++i]);
			break;
#endif
#if  RTRACE
		case 'x':				/* x resolution */
			hresolu = atoi(argv[++i]);
			break;
		case 'y':				/* y resolution */
			vresolu = atoi(argv[++i]);
			break;
#endif
		case 'w':				/* warnings */
			wrnvec = wrnvec==NULL ? stderr_v : NULL;
			break;
		case 'e':				/* error file */
			errfile = argv[++i];
			break;
		case 'l':				/* limit */
			switch (argv[i][2]) {
			case 'r':				/* recursion */
				maxdepth = atoi(argv[++i]);
				break;
			case 'w':				/* weight */
				minweight = atof(argv[++i]);
				break;
			default:
				goto unkopt;
			}
			break;
#if  RPICT
		case 'r':				/* recover file */
			recover = argv[++i];
			break;
		case 't':				/* timer */
			ralrm = atoi(argv[++i]);
			break;
#endif
		case 'a':				/* ambient */
			switch (argv[i][2]) {
			case 'v':				/* value */
				setcolor(ambval, atof(argv[i+1]),
						atof(argv[i+2]),
						atof(argv[i+3]));
				i += 3;
				break;
			case 'a':				/* accuracy */
				ambacc = atof(argv[++i]);
				break;
			case 'r':				/* resolution */
				ambres = atoi(argv[++i]);
				break;
			case 'd':				/* divisions */
				ambdiv = atoi(argv[++i]);
				break;
			case 's':				/* super-samp */
				ambssamp = atoi(argv[++i]);
				break;
			case 'b':				/* bounces */
				ambounce = atoi(argv[++i]);
				break;
			case 'i':				/* include */
				if (ambincl != 1) {
					ambincl = 1;
					amblp = amblist;
				}
				*amblp++ = argv[++i];
				break;
			case 'e':				/* exclude */
				if (ambincl != 0) {
					ambincl = 0;
					amblp = amblist;
				}
				*amblp++ = argv[++i];
				break;
			case 'f':				/* file */
				ambfile= argv[++i];
				break;
			default:
				goto unkopt;
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
				goto unkopt;
			}
			switch (argv[i][3]) {
			case '\0':
				outform = inform;
				break;
			case 'a':				/* ascii */
			case 'f':				/* float */
			case 'd':				/* double */
				outform = argv[i][3];
				break;
			default:
				goto unkopt;
			}
			break;
		case 'o':				/* output */
			outvals = argv[i]+2;
			break;
		case 'h':				/* no header */
			loadflags &= ~IO_INFO;
			break;
#endif
#if  RVIEW
		case 'b':				/* black and white */
			greyscale = !greyscale;
			break;
		case 'o':				/* output device */
			devname = argv[++i];
			break;
#endif
		default:
unkopt:
			sprintf(errmsg, "unknown option: '%s'", argv[i]);
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
	diemask |= sigmask(SIGALRM);
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
#ifdef  BSD
	sigblock(diemask);
#endif
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
	else
		diemask |= sigmask(signo);
	sigerr[signo] = msg;
}


printdefaults()			/* print default values to stdout */
{
	register char  *cp;

#if  RPICT|RVIEW
	printf("-vt%c\t\t\t\t# view type is %s\n", ourview.type,
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
#if  RPICT
	printf("-dp %f\t\t\t# distribute pixel\n", dstrpix);
#endif
	printf("-ds %f\t\t\t# distribute source\n", dstrsrc);
#if  RPICT|RVIEW
	printf("-sp %-9d\t\t\t# sample pixel\n", psample);
	printf("-sd %f\t\t\t# sample difference\n", maxdiff);
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
