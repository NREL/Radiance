/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Make illum sources for optimizing rendering process
 */

#include  "standard.h"

#include  "object.h"

#include  "otypes.h"

				/* default parameters */
#define  SAMPDENS	128		/* points per projected steradian */
#define  NSAMPS		32		/* samples per point */
#define  DFLMAT		"illum_mat"	/* material name */
				/* selection options */
#define  S_NONE		0		/* select none */
#define  S_ELEM		1		/* select specified element */
#define  S_COMPL	2		/* select all but element */
#define  S_ALL		3		/* select all */

				/* rtrace command and defaults */
char  *rtargv[64] = { "rtrace", "-dj", ".25", "-dr", "3", "-ab", "2",
		"-ad", "256", "-as", "128", "-aa", ".15" };
int  rtargc = 13;
				/* overriding rtrace options */
char  *myrtopts[] = { "-I-", "-i-", "-di+", "-ov", "-h-", "-fff", NULL };

				/* illum flags */
#define  IL_FLAT	0x1		/* flat surface */
#define  IL_LIGHT	0x2		/* light rather than illum */
#define  IL_COLDST	0x4		/* colored distribution */
#define  IL_COLAVG	0x8		/* compute average color */
#define  IL_DATCLB	0x10		/* OK to clobber data file */

struct illum_args {
	int	flags;			/* flags from list above */
	char	matname[MAXSTR];	/* illum material name */
	char	datafile[MAXSTR];	/* distribution data file name */
	int	dfnum;			/* data file number */
	char	altmatname[MAXSTR];	/* alternate material name */
	int	nsamps;			/* # of samples in each direction */
	int	nalt, nazi;		/* # of altitude and azimuth angles */
	FVECT	orient;			/* coordinate system orientation */
} thisillum = {			/* default values */
	0,
	DFLMAT,
	DFLMAT,
	0,
	VOIDID,
	NSAMPS,
};

int	sampdens = SAMPDENS;	/* sample point density */
char	matcheck[MAXSTR];	/* current material to include or exclude */
int	matselect = S_ALL;	/* selection criterion */

int	rt_pd[3];		/* rtrace pipe descriptors */
float	*rt_buf;		/* rtrace i/o buffer */
int	rt_bsiz;		/* maximum rays for rtrace buffer */
int	rt_nrays;		/* current length of rtrace buffer */

int	gargc;			/* global argc */
char	**gargv;		/* global argv */
#define  progname	gargv[0]

int	doneheader = 0;		/* printed header yet? */

extern char	*fgetline(), *fgetword(), *sskip(), 
		*atos(), *iskip(), *fskip();
extern FILE	*popen();


main(argc, argv)		/* compute illum distributions using rtrace */
int	argc;
char	*argv[];
{
	extern char	*getenv(), *getpath();
	char	*rtpath;
	register int	i;
				/* set global arguments */
	gargc = argc; gargv = argv;
				/* set up rtrace command */
	if (argc < 2)
		error(USER, "too few arguments");
	for (i = 1; i < argc-1; i++)
		rtargv[rtargc++] = argv[i];
	for (i = 0; myrtopts[i] != NULL; i++)
		rtargv[rtargc++] = myrtopts[i];
	rtargv[rtargc++] = argv[argc-1];
	rtargv[rtargc] = NULL;
				/* just asking for defaults? */
	if (!strcmp(argv[argc-1], "-defaults")) {
		rtpath = getpath(rtargv[0], getenv("PATH"), X_OK);
		if (rtpath == NULL) {
			eputs(rtargv[0]);
			eputs(": command not found\n");
			exit(1);
		}
		execv(rtpath, rtargv);
		perror(rtpath);
		exit(1);
	}
				/* else initialize and run our calculation */
	init();
	mkillum(stdin, "standard input");
	exit(cleanup());	/* cleanup returns rtrace status */
}


init()				/* start rtrace and set up buffers */
{
	int	maxbytes;

	maxbytes = open_process(rt_pd, rtargv);
	if (maxbytes == 0) {
		eputs(rtargv[0]);
		eputs(": command not found\n");
		exit(1);
	}
	if (maxbytes < 0)
		error(SYSTEM, "cannot start rtrace process");
	rt_bsiz = maxbytes/(6*sizeof(float));
	rt_buf = (float *)malloc(rt_bsiz*(6*sizeof(float)));
	if (rt_buf == NULL)
		error(SYSTEM, "out of memory in init");
	rt_bsiz--;			/* allow for flush ray */
	rt_nrays = 0;
}


int
cleanup()			/* close rtrace process and return status */
{
	int	status;

	free((char *)rt_buf);
	rt_bsiz = 0;
	status = close_process(rt_pd);
	if (status < 0)
		return(0);		/* unknown status */
	return(status);
}


eputs(s)				/* put string to stderr */
register char  *s;
{
	static int  midline = 0;

	if (!*s) return;
	if (!midline) {
		fputs(progname, stderr);
		fputs(": ", stderr);
	}
	fputs(s, stderr);
	midline = s[strlen(s)-1] != '\n';
}


mkillum(infp, name)		/* process stream */
register FILE	*infp;
char	*name;
{
	char	buf[512];
	FILE	*pfp;
	register int	c;

	while ((c = getc(infp)) != EOF) {
		if (isspace(c))
			continue;
		if (c == '#') {				/* comment/options */
			buf[0] = c;
			fgets(buf+1, sizeof(buf)-1, infp);
			fputs(buf, stdout);
			getoptions(buf, name);
		} else if (c == '!') {			/* command */
			buf[0] = c;
			fgetline(buf+1, sizeof(buf)-1, infp);
			if ((pfp = popen(buf+1, "r")) == NULL) {
				sprintf(errmsg, "cannot execute \"%s\"", buf);
				error(SYSTEM, errmsg);
			}
			mkillum(pfp, buf);
			pclose(pfp);
		} else {				/* object */
			ungetc(c, infp);
			xobject(infp, name);
		}
	}
}


getoptions(s, nm)		/* get options from string s */
char	*s;
char	*nm;
{
	extern FILE	*freopen();
	char	buf[64];
	int	nerrs = 0;
	register char	*cp;

	if (strncmp(s, "#@mkillum", 9) || !isspace(s[9]))
		return;
	cp = s+10;
	while (*cp) {
		switch (*cp) {
		case ' ':
		case '\t':
		case '\n':
			cp++;
			continue;
		case 'm':			/* material name */
			cp++;
			if (*cp != '=')
				break;
			cp++;
			atos(thisillum.matname, MAXSTR, cp);
			cp = sskip(cp);
			if (!(thisillum.flags & IL_DATCLB)) {
				strcpy(thisillum.datafile, thisillum.matname);
				thisillum.dfnum = 0;
			}
			continue;
		case 'f':			/* data file name */
			cp++;
			if (*cp != '=')
				break;
			cp++;
			if (*cp == '\0') {
				thisillum.flags &= ~IL_DATCLB;
				continue;
			}
			atos(thisillum.datafile, MAXSTR, cp);
			cp = sskip(cp);
			thisillum.dfnum = 0;
			thisillum.flags |= IL_DATCLB;
			continue;
		case 'i':			/* include material */
		case 'e':			/* exclude material */
			matselect = (*cp++ == 'i') ? S_ELEM : S_COMPL;
			if (*cp != '=')
				break;
			cp++;
			atos(matcheck, MAXSTR, cp);
			cp = sskip(cp);
			continue;
		case 'a':			/* use everything */
			cp = sskip(cp);
			matselect = S_ALL;
			continue;
		case 'n':			/* use nothing (passive) */
			cp = sskip(cp);
			matselect = S_NONE;
			continue;
		case 'c':			/* color calculation */
			cp++;
			if (*cp != '=')
				break;
			cp++;
			switch (*cp) {
			case 'a':			/* average */
				thisillum.flags |= IL_COLAVG;
				thisillum.flags &= ~IL_COLDST;
				break;
			case 'd':			/* distribution */
				thisillum.flags |= IL_COLDST;
				thisillum.flags &= ~IL_COLAVG;
				break;
			case 'n':			/* none */
				thisillum.flags &= ~(IL_COLAVG|IL_COLDST);
				break;
			default:
				goto opterr;
			}
			cp = sskip(cp);
			continue;
		case 'd':			/* point sample density */
			cp++;
			if (*cp != '=')
				break;
			cp++;
			if (!isintd(cp, " \t\n"))
				break;
			sampdens = atoi(cp);
			cp = sskip(cp);
			continue;
		case 's':			/* point super-samples */
			cp++;
			if (*cp != '=')
				break;
			cp++;
			if (!isintd(cp, " \t\n"))
				break;
			thisillum.nsamps = atoi(cp);
			cp = sskip(cp);
			continue;
		case 'o':			/* output file */
			cp++;
			if (*cp != '=')
				break;
			cp++;
			atos(buf, sizeof(buf), cp);
			cp = sskip(cp);
			if (freopen(buf, "w", stdout) == NULL) {
				sprintf(errmsg,
				"cannot open output file \"%s\"", buf);
				error(SYSTEM, errmsg);
			}
			doneheader = 0;
			continue;
		}
	opterr:					/* skip faulty option */
		cp = sskip(cp);
		nerrs++;
	}
	if (nerrs) {
		sprintf(errmsg, "(%s): %d error(s) in option line:",
				nm, nerrs);
		error(WARNING, errmsg);
		eputs(s);
	}
}
