/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  getbbox.c - compute bounding box for scene files
 *
 *  Adapted from oconv.c 29 May 1991
 */

#include  "standard.h"

#include  "octree.h"

#include  "object.h"

#ifndef  DEFPATH
#define  DEFPATH	":/usr/local/lib/ray"
#endif

char  *progname;			/* argv[0] */

char  *libpath;				/* library search path */

int  nowarn = 0;			/* supress warnings? */

int  (*addobjnotify[])() = {NULL};	/* new object notifier functions */

FVECT  bbmin, bbmax;			/* bounding box */

addobject(o)			/* add object to bounding box */
OBJREC	*o;
{
	add2bbox(o, bbmin, bbmax);
}


main(argc, argv)		/* read object files and compute bounds */
int  argc;
char  **argv;
{
	extern char  *getenv();
	int  nohead = 0;
	int  i;

	progname = argv[0];

	if ((libpath = getenv("RAYPATH")) == NULL)
		libpath = DEFPATH;

	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		switch (argv[i][1]) {
		case 'w':
			nowarn = 1;
			continue;
		case 'h':
			nohead = 1;
			continue;
		}
		break;
	}
						/* find bounding box */
	bbmin[0] = bbmin[1] = bbmin[2] = FHUGE;
	bbmax[0] = bbmax[1] = bbmax[2] = -FHUGE;
						/* read input */
	if (i >= argc)
		readobj(NULL, addobject);
	else
		for ( ; i < argc; i++)
			if (!strcmp(argv[i], "-"))	/* from stdin */
				readobj(NULL, addobject);
			else				/* from file */
				readobj(argv[i], addobject);
						/* print bounding box */
	if (!nohead)
		printf(
"     xmin      xmax      ymin      ymax      zmin      zmax\n");

	printf("%9g %9g %9g %9g %9g %9g\n", bbmin[0], bbmax[0],
			bbmin[1], bbmax[1], bbmin[2], bbmax[2]);
	quit(0);
}


quit(code)				/* exit program */
int  code;
{
	exit(code);
}


cputs()					/* interactive error */
{
	/* referenced, but not used */
}


wputs(s)				/* warning message */
char  *s;
{
	if (!nowarn)
		eputs(s);
}


eputs(s)				/* put string to stderr */
register char  *s;
{
	static int  inln = 0;

	if (!inln++) {
		fputs(progname, stderr);
		fputs(": ", stderr);
	}
	fputs(s, stderr);
	if (*s && s[strlen(s)-1] == '\n')
		inln = 0;
}
