/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Make illum sources for optimizing rendering process
 */

#include  "mkillum.h"

#include  <ctype.h>

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

struct rtproc	rt;		/* our rtrace process */

struct illum_args  thisillum = {	/* our illum and default values */
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

FUN	ofun[NUMOTYPE] = INIT_OTYPE;	/* object types */

int	gargc;			/* global argc */
char	**gargv;		/* global argv */
#define  progname	gargv[0]

int	doneheader = 0;		/* printed header yet? */
#define  checkhead()	if (!doneheader++) printhead(gargc,gargv)

int	warnings = 1;		/* print warnings? */

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
	for (i = 1; i < argc-1; i++) {
		rtargv[rtargc++] = argv[i];
		if (argv[i][0] == '-' && argv[i][1] == 'w')
			warnings = !warnings;
	}
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
	filter(stdin, "standard input");
	quit(0);
}


init()				/* start rtrace and set up buffers */
{
	int	maxbytes;

	maxbytes = open_process(rt.pd, rtargv);
	if (maxbytes == 0) {
		eputs(rtargv[0]);
		eputs(": command not found\n");
		exit(1);
	}
	if (maxbytes < 0)
		error(SYSTEM, "cannot start rtrace process");
	rt.bsiz = maxbytes/(6*sizeof(float));
	rt.buf = (float *)malloc(rt.bsiz*(6*sizeof(float)));
	if (rt.buf == NULL)
		error(SYSTEM, "out of memory in init");
	rt.bsiz--;			/* allow for flush ray */
	rt.nrays = 0;
}


quit(status)			/* exit with status */
int  status;
{
	int	rtstat;

	rtstat = close_process(rt.pd);
	if (status == 0)
		if (rtstat < 0)
			error(WARNING,
			"unknown return status from rtrace process");
		else
			status = rtstat;
	exit(status);
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


wputs(s)			/* print warning if enabled */
char  *s;
{
	if (warnings)
		eputs(s);
}


filter(infp, name)		/* process stream */
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
			xoptions(buf, name);
		} else if (c == '!') {			/* command */
			buf[0] = c;
			fgetline(buf+1, sizeof(buf)-1, infp);
			if ((pfp = popen(buf+1, "r")) == NULL) {
				sprintf(errmsg, "cannot execute \"%s\"", buf);
				error(SYSTEM, errmsg);
			}
			filter(pfp, buf);
			pclose(pfp);
		} else {				/* object */
			ungetc(c, infp);
			xobject(infp, name);
		}
	}
}


xoptions(s, nm)			/* process options in string s */
char	*s;
char	*nm;
{
	extern FILE	*freopen();
	char	buf[64];
	int	nerrs = 0;
	register char	*cp;

	if (strncmp(s, "#@mkillum", 9) || !isspace(s[9])) {
		fputs(s, stdout);		/* not for us */
		return;
	}
	cp = s+10;
	while (*cp) {
		switch (*cp) {
		case ' ':
		case '\t':
		case '\n':
			cp++;
			continue;
		case 'm':			/* material name */
			if (*++cp != '=')
				break;
			if (!*++cp)
				break;
			atos(thisillum.matname, MAXSTR, cp);
			cp = sskip(cp);
			if (!(thisillum.flags & IL_DATCLB)) {
				strcpy(thisillum.datafile, thisillum.matname);
				thisillum.dfnum = 0;
			}
			continue;
		case 'f':			/* data file name */
			if (*++cp != '=')
				break;
			if (!*++cp) {
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
			matselect = (*cp == 'i') ? S_ELEM : S_COMPL;
			if (*++cp != '=')
				break;
			atos(matcheck, MAXSTR, ++cp);
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
			if (*++cp != '=')
				break;
			switch (*++cp) {
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
			if (*++cp != '=')
				break;
			if (!isintd(++cp, " \t\n"))
				break;
			sampdens = atoi(cp);
			cp = sskip(cp);
			continue;
		case 's':			/* point super-samples */
			if (*++cp != '=')
				break;
			if (!isintd(++cp, " \t\n"))
				break;
			thisillum.nsamps = atoi(cp);
			cp = sskip(cp);
			continue;
		case 'o':			/* output file */
			if (*++cp != '=')
				break;
			if (!*++cp)
				break;
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
						/* print header? */
	checkhead();
						/* issue warnings? */
	if (nerrs) {
		sprintf(errmsg, "(%s): %d error(s) in option line:",
				nm, nerrs);
		error(WARNING, errmsg);
		wputs(s);
		printf("# %s: the following option line has %d error(s):\n",
				progname, nerrs);
	}
						/* print pure comment */
	putchar('#'); putchar(' '); fputs(s+2, stdout);
}


printhead(ac, av)			/* print out header */
register int  ac;
register char  **av;
{
	putchar('#');
	while (ac-- > 0) {
		putchar(' ');
		fputs(*av++, stdout);
	}
	putchar('\n');
}


xobject(fp, nm)				/* translate an object from fp */
FILE  *fp;
char  *nm;
{
	OBJREC  thisobj;
	char  str[MAXSTR];
	int  doit;
					/* read the object */
	if (fgetword(thisillum.altmat, MAXSTR, fp) == NULL)
		goto readerr;
	if (fgetword(str, MAXSTR, fp) == NULL)
		goto readerr;
					/* is it an alias? */
	if (!strcmp(str, ALIASID)) {
		if (fgetword(str, MAXSTR, fp) == NULL)
			goto readerr;
		printf("\n%s %s %s", thisillum.altmat, ALIASID, str);
		if (fgetword(str, MAXSTR, fp) == NULL)
			goto readerr;
		printf(" %s\n", str);
		return;
	}
	thisobj.omod = OVOID;
	if ((thisobj.otype = otype(str)) < 0) {
		sprintf(errmsg, "(%s): unknown type \"%s\"", nm, str);
		error(USER, errmsg);
	}
	if (fgetword(str, MAXSTR, fp) == NULL)
		goto readerr;
	thisobj.oname = str;
	if (readfargs(&thisobj.oargs, fp) != 1)
		goto readerr;
	thisobj.os = NULL;
					/* check for translation */
	switch (matselect) {
	case S_NONE:
		doit = 0;
		break;
	case S_ALL:
		doit = issurface(thisobj.otype);
		break;
	case S_ELEM:
		doit = !strcmp(thisillum.altmat, matcheck);
		break;
	case S_COMPL:
		doit = strcmp(thisillum.altmat, matcheck);
		break;
	}
	if (doit)				/* make sure we can do it */
		switch (thisobj.otype) {
		case OBJ_SPHERE:
		case OBJ_FACE:
		case OBJ_RING:
			break;
		default:
			sprintf(errmsg,
				"(%s): cannot make illum for %s \"%s\"",
					nm, ofun[thisobj.otype].funame,
					thisobj.oname);
			error(WARNING, errmsg);
			doit = 0;
			break;
		}
						/* print header? */
	checkhead();
						/* process object */
	if (doit)
		mkillum(&thisobj, &thisillum, &rt);
	else
		printobj(thisillum.altmat, &thisobj);
						/* free arguments */
	freefargs(&thisobj.oargs);
	return;
readerr:
	sprintf(errmsg, "(%s): error reading scene", nm);
	error(USER, errmsg);
}


int
otype(ofname)			/* get object function number from its name */
char  *ofname;
{
	register int  i;

	for (i = 0; i < NUMOTYPE; i++)
		if (!strcmp(ofun[i].funame, ofname))
			return(i);

	return(-1);		/* not found */
}


o_default() {}
