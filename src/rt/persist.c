/* Copyright (c) 1996 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for persistent rtrace and rpict processes.
 */

#include "standard.h"

#ifdef F_SETLKW

#include "paths.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef TIMELIM
#define TIMELIM		(8*3600)	/* time limit for holding pattern */
#endif

extern char	*strcpy(), *index();

extern int	headismine;	/* boolean true if header belongs to me */

extern char	*progname;	/* global program name */

extern char	*errfile;	/* global error file name */

static char	*persistfname = NULL;	/* persist file name */
static int	persistfd = -1;		/* persist file descriptor */

static char	inpname[TEMPLEN+1], outpname[TEMPLEN+1], errname[TEMPLEN+1];


pfdetach()		/* release persist (and header) resources */
{
	if (persistfd >= 0)
		close(persistfd);
	persistfd = -1;
	persistfname = NULL;
	inpname[0] = '\0';
	outpname[0] = '\0';
	errname[0] = '\0';
	headismine = 0;
}


pfclean()		/* clean up persist files */
{
	if (persistfd >= 0)
		close(persistfd);
	if (persistfname != NULL)
		unlink(persistfname);
	if (inpname[0])
		unlink(inpname);
	if (outpname[0])
		unlink(outpname);
	if (errname[0])
		unlink(errname);
}


pflock(lf)		/* place or release exclusive lock on file */
int	lf;
{
	struct flock	fls;

	fls.l_type = lf ? F_WRLCK : F_UNLCK;
	fls.l_whence = 0;
	fls.l_start = 0L;
	fls.l_len = 0L;
	if (fcntl(persistfd, F_SETLKW, &fls) < 0)
		error(SYSTEM, "cannot (un)lock persist file");
}


persistfile(pfn)	/* open persist file and lock it */
char	*pfn;
{
	persistfd = open(pfn, O_WRONLY|O_CREAT|O_EXCL, 0644);
	if (persistfd >= 0) {
		persistfname = pfn;
		pflock(1);
		return;
	}
				/* file exists -- switch to i/o process */
	persistfd = open(pfn, O_RDWR);
	if (persistfd < 0) {
		sprintf(errmsg, "cannot open persist file \"%s\"", pfn);
		error(SYSTEM, errmsg);
	}
	pflock(1);
	io_process();	/* never returns */
}


static int	got_io;

static int sig_io() { got_io++; }

static int sig_alrm() { quit(0); }


pfhold()		/* holding pattern for idle rendering process */
{
	int	(*oldalrm)();
	char	buf[512];
	register int	n;
				/* close input and output descriptors */
	close(fileno(stdin));
	close(fileno(stdout));
				/* create named pipes for input and output */
	if (mknod(mktemp(strcpy(inpname,TEMPLATE)), S_IFIFO|0600, 0) < 0)
		goto createrr;
	if (mknod(mktemp(strcpy(outpname,TEMPLATE)), S_IFIFO|0600, 0) < 0)
		goto createrr;
	if (errfile == NULL &&
		mknod(mktemp(strcpy(errname,TEMPLATE)), S_IFIFO|0600, 0) < 0)
		goto createrr;
	sprintf(buf, "%s %d\n%s\n%s\n%s\n", progname, getpid(),
			inpname, outpname, errname);
	n = strlen(buf);
	if (write(persistfd, buf, n) < n)
		error(SYSTEM, "error writing persist file");
				/* wait TIMELIM for someone to signal us */
	got_io = 0;
	signal(SIGIO, sig_io);
	oldalrm = (int (*)())signal(SIGALRM, sig_alrm);
	alarm(TIMELIM);
	pflock(0);			/* unlock persist file for attach */
	while (!got_io)
		pause();		/* wait for attach */
	pflock(1);			/* grab persist file right back! */
	alarm(0);			/* turn off alarm */
	signal(SIGALRM, oldalrm);
	signal(SIGIO, SIG_DFL);
	if (lseek(persistfd, 0L, 0) < 0 || ftruncate(persistfd, 0L) < 0)
		error(SYSTEM, "seek/truncate error on persist file");
				/* someone wants us; reopen stdin and stdout */
	if (freopen(inpname, "r", stdin) == NULL)
		goto openerr;
	if (freopen(outpname, "w", stdout) == NULL)
		goto openerr;
	if (errname[0]) {
		if (freopen(errname, "w", stderr) == NULL)
			goto openerr;
		unlink(errname);
		errname[0] = '\0';
	}
	unlink(inpname);
	inpname[0] = '\0';
	unlink(outpname);
	outpname[0] = '\0';
	return;
createrr:
	error(SYSTEM, "cannot create named pipes in pfhold");
openerr:
	error(SYSTEM, "cannot open named pipes in pfhold");
}


io_process()		/* just act as conduits to and from actual process */
{
	register char	*cp;
	register int	nr, n;
	char	buf[512], *pfin, *pfout, *pferr;
	int	pid, pid2 = -1;
					/* load persist file */
	while ((nr = read(persistfd, buf, sizeof(buf)-1)) == 0) {
		pflock(0);
		sleep(15);		/* wait until ready */
		pflock(1);
	}
	pfdetach();			/* close persist file */
	if (nr < 0)
		error(SYSTEM, "error reading persist file");
	buf[nr] = '\0';
	if ((cp = index(buf, ' ')) == NULL)
		goto formerr;
	*cp++ = '\0';
	if ((pid = atoi(cp)) <= 0)
		goto formerr;
	if ((cp = index(cp, '\n')) == NULL)
		goto formerr;
	pfin = ++cp;
	if ((cp = index(cp, '\n')) == NULL)
		goto formerr;
	*cp++ = '\0';
	pfout = cp;
	if ((cp = index(cp, '\n')) == NULL)
		goto formerr;
	*cp++ = '\0';
	pferr = cp;
	if ((cp = index(cp, '\n')) == NULL)
		goto formerr;
	*cp++ = '\0';
	if (cp-buf != nr)
		goto formerr;
	if (strcmp(buf, progname)) {
		sprintf(errmsg, "persist file for %s, not %s", buf, progname);
		error(USER, errmsg);
	}
				/* wake up rendering process */
	if (kill(pid, SIGIO) < 0)
		error(SYSTEM, "cannot signal rendering process in io_process");
	pid = fork();		/* fork i/o process */
	if (pid < 0)
		goto forkerr;
				/* connect to appropriate pipe */
	if (pid) {			/* parent passes renderer output */
		close(0);
		if (pferr[0]) {
			pid2 = fork();		/* fork another for stderr */
			if (pid2 < 0)
				goto forkerr;
		}
		if (pid2) {			/* parent is still stdout */
			if (open(pfout, O_RDONLY) != 0)
				error(SYSTEM,
				"cannot open output pipe in io_process");
		} else {			/* second child is stderr */
			if (open(pferr, O_RDONLY) != 0)
				error(SYSTEM,
				"cannot open error pipe in io_process");
			dup2(2, 1);		/* attach stdout to stderr */
		}
	} else {			/* child passes renderer input */
		close(1);
		if (open(pfin, O_WRONLY) != 1)
			error(SYSTEM, "cannot open input pipe in io_process");
	}
				/* pass input to output */
				/* read as much as we can, write all of it */
	while ((nr = read(0, cp=buf, sizeof(buf))) > 0)
		do {
			if ((n = write(1, cp, nr)) <= 0)
				error(SYSTEM, "write error in io_process");
			cp += n;
		} while ((nr -= n) > 0);
	if (nr < 0)
		error(SYSTEM, "read error in io_process");
	close(0);		/* close input */
	close(1);		/* close output */
	if (pid)		/* parent waits for stdin child */
		wait(0);
	if (pid2 > 0)		/* wait for stderr child also */
		wait(0);
	_exit(0);		/* all done, exit (not quit!) */
formerr:
	error(USER, "format error in persist file");
forkerr:
	error(SYSTEM, "fork failed in io_process");
}

#else

pfclean() {}

#endif
