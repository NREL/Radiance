#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 *  rvmain.c - main for rview interactive viewer
 */

#include "copyright.h"

#include  <signal.h>
#include  <time.h>

#include  "platform.h"
#include  "ray.h"
#include  "source.h"
#include  "ambient.h"
#include  "rpaint.h"
#include  "random.h"
#include  "paths.h"
#include  "view.h"
#include  "pmapray.h"

extern char  *progname;			/* global argv[0] */

VIEW  ourview = STDVIEW;		/* viewing parameters */
int  hresolu, vresolu;			/* image resolution */

int  psample = 8;			/* pixel sample size */
double	maxdiff = .15;			/* max. sample difference */

int  greyscale = 0;			/* map colors to brightness? */
char  *dvcname = dev_default;		/* output device name */

double	exposure = 1.0;			/* exposure for scene */

int  newparam = 1;			/* parameter setting changed */
 
struct driver  *dev = NULL;		/* driver functions */

char  rifname[128];			/* rad input file name */

VIEW  oldview;				/* previous view parameters */

PNODE  ptrunk;				/* the base of our image */
RECT  pframe;				/* current frame boundaries */
int  pdepth;				/* image depth in current frame */

char  *errfile = NULL;			/* error output file */

int  nproc = 1;				/* number of processes */

char  *sigerr[NSIG];			/* signal error messages */

static void onsig(int  signo);
static void sigdie(int  signo, char  *msg);
static void printdefaults(void);


int
main(int argc, char *argv[])
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
	char  *octnm = NULL;
	char  *err;
	int  rval;
	int  i;
					/* global program name */
	progname = argv[0] = fixargv0(argv[0]);
					/* set our defaults */
	shadthresh = .1;
	shadcert = .25;
	directrelay = 0;
	vspretest = 128;
	srcsizerat = 0.;
	specthresh = .3;
	specjitter = 1.;
	maxdepth = 6;
	minweight = 1e-2;
	ambacc = 0.3;
	ambres = 32;
	ambdiv = 256;
	ambssamp = 64;
					/* option city */
	for (i = 1; i < argc; i++) {
						/* expand arguments */
		while ((rval = expandarg(&argc, &argv, i)) > 0)
			;
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
		if (!strcmp(argv[i], "-devices")) {
			printdevices();
			quit(0);
		}
		rval = getrenderopt(argc-i, argv+i);
		if (rval >= 0) {
			i += rval;
			continue;
		}
		rval = getviewopt(&ourview, argc-i, argv+i);
		if (rval >= 0) {
			i += rval;
			continue;
		}
		switch (argv[i][1]) {
		case 'n':				/* # processes */
			check(2,"i");
			nproc = atoi(argv[++i]);
			if (nproc <= 0)
				error(USER, "bad number of processes");
			break;
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
		case 'b':				/* grayscale */
			bool(2,greyscale);
			break;
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
			case 'e':				/* exposure */
				check(3,"f");
				exposure = atof(argv[++i]);
				if (argv[i][0] == '+' || argv[i][0] == '-')
					exposure = pow(2.0, exposure);
				break;
			default:
				goto badopt;
			}
			break;
		case 'w':				/* warnings */
			rval = erract[WARNING].pf != NULL;
			bool(2,rval);
			if (rval) erract[WARNING].pf = wputs;
			else erract[WARNING].pf = NULL;
			break;
		case 'e':				/* error file */
			check(2,"s");
			errfile = argv[++i];
			break;
		case 'o':				/* output device */
			check(2,"s");
			dvcname = argv[++i];
			break;
		case 'R':				/* render input file */
			check(2,"s");
			strcpy(rifname, argv[++i]);
			break;
		default:
			goto badopt;
		}
	}
	err = setview(&ourview);	/* set viewing parameters */
	if (err != NULL)
		error(USER, err);
						/* set up signal handling */
	sigdie(SIGINT, "Interrupt");
	sigdie(SIGTERM, "Terminate");
#ifndef _WIN32
	sigdie(SIGHUP, "Hangup");
	sigdie(SIGPIPE, "Broken pipe");
	sigdie(SIGALRM, "Alarm clock");
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
	if (i == argc)
		octnm = NULL;
	else if (i == argc-1)
		octnm = argv[i];
	else
		goto badopt;
	if (octnm == NULL)
		error(USER, "missing octree argument");
					/* set up output & start process(es) */
	SET_FILE_BINARY(stdout);
	
	ray_init(octnm);		/* also calls ray_init_pmap() */
	
	rview();			/* run interactive viewer */

	devclose();			/* close output device */

	/* PMAP: free photon maps */
	ray_done_pmap();
	
	quit(0);

badopt:
	sprintf(errmsg, "command line error at '%s'", argv[i]);
	error(USER, errmsg);
	return 1; /* pro forma return */

#undef	check
#undef	bool
}


void
wputs(				/* warning output function */
	char	*s
)
{
	int  lasterrno = errno;
	eputs(s);
	errno = lasterrno;
}


void
eputs(				/* put string to stderr */
	char  *s
)
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


static void
onsig(				/* fatal signal */
	int  signo
)
{
	static int  gotsig = 0;

	if (gotsig++)			/* two signals and we're gone! */
		_exit(signo);

#ifndef _WIN32
	alarm(15);			/* allow 15 seconds to clean up */
	signal(SIGALRM, SIG_DFL);	/* make certain we do die */
#endif
	eputs("signal - ");
	eputs(sigerr[signo]);
	eputs("\n");
	devclose();
	quit(3);
}


static void
sigdie(			/* set fatal signal */
	int  signo,
	char  *msg
)
{
	if (signal(signo, onsig) == SIG_IGN)
		signal(signo, SIG_IGN);
	sigerr[signo] = msg;
}


static void
printdefaults(void)			/* print default values to stdout */
{
	printf("-n %-2d\t\t\t\t# number of rendering processes\n", nproc);
	printf(greyscale ? "-b+\t\t\t\t# greyscale on\n" :
			"-b-\t\t\t\t# greyscale off\n");
	printf("-vt%c\t\t\t\t# view type %s\n", ourview.type,
			ourview.type==VT_PER ? "perspective" :
			ourview.type==VT_PAR ? "parallel" :
			ourview.type==VT_HEM ? "hemispherical" :
			ourview.type==VT_ANG ? "angular" :
			ourview.type==VT_CYL ? "cylindrical" :
			ourview.type==VT_PLS ? "planisphere" :
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
	printf("-pe %f\t\t\t# pixel exposure\n", exposure);
	printf("-ps %-9d\t\t\t# pixel sample\n", psample);
	printf("-pt %f\t\t\t# pixel threshold\n", maxdiff);
	printf("-o %s\t\t\t\t# output device\n", dvcname);
	printf(erract[WARNING].pf != NULL ?
			"-w+\t\t\t\t# warning messages on\n" :
			"-w-\t\t\t\t# warning messages off\n");
	print_rdefaults();
}
