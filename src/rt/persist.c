/* Copyright (c) 1993 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for persistent rtrace and rpict processes.
 */

#include "standard.h"
#include "paths.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef TIMELIM
#define TIMELIM		(8*3600)	/* time limit for holding pattern */
#endif

extern char	*strcpy(), *index();

extern int	headismine;	/* boolean true if header belongs to me */

static char	*persistfname = NULL;	/* persist file name */
static int	persistfd = -1;		/* persist file descriptor */

static char	inpname[TEMPLEN+1], outpname[TEMPLEN+1];


pfdetach()		/* release persist (and header) resources */
{
	if (persistfd >= 0)
		close(persistfd);
	persistfd = -1;
	persistfname = NULL;
	inpname[0] = '\0';
	outpname[0] = '\0';
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
	persistfd = open(pfn, O_WRONLY|O_CREAT|O_EXCL, 0666);
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


int sig_noop() {}


pfhold()		/* holding pattern for idle rendering process */
{
	char	buf[128];
	register int	n;
				/* close input and output descriptors */
	close(fileno(stdin));
	fflush(stdout);
	close(fileno(stdout));
				/* create named pipes for input and output */
	if (mknod(mktemp(strcpy(inpname,TEMPLATE)), S_IFIFO|0600) < 0)
		goto createrr;
	if (mknod(mktemp(strcpy(outpname,TEMPLATE)), S_IFIFO|0600) < 0)
		goto createrr;
	sprintf(buf, "%d\n%s\n%s\n", getpid(), inpname, outpname);
	if (lseek(persistfd, 0L, 0) < 0)
		error(SYSTEM, "seek error on persist file in pfhold");
	n = strlen(buf);
	if (write(persistfd, buf, n) < n)
		error(SYSTEM, "error writing persist file in pfhold");
				/* wait TIMELIM for someone to signal us */
	signal(SIGIO, sig_noop);
	alarm(TIMELIM);
	pflock(0);
	pause();
	alarm(0);
	signal(SIGIO, SIG_DFL);
	pflock(1);
				/* someone wants us; reopen stdin and stdout */
	if (freopen(inpname, "r", stdin) == NULL)
		goto openerr;
	if (freopen(outpname, "w", stdout) == NULL)
		goto openerr;
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
	char	buf[4096], *pfin, *pfout;
	int	pid;
				/* load and close persist file */
	nr = read(persistfd, buf, sizeof(buf));
	pfdetach();
	if (nr < 0)
		error(SYSTEM, "cannot read persist file");
	buf[nr] = '\0';
	if ((cp = index(buf, '\n')) == NULL)
		goto formerr;
	*cp++ = '\0';
	if ((pid = atoi(buf)) <= 0)
		goto formerr;
	pfin = cp;
	if ((cp = index(cp, '\n')) == NULL)
		goto formerr;
	*cp++ = '\0';
	pfout = cp;
	if ((cp = index(cp, '\n')) == NULL)
		goto formerr;
	*cp++ = '\0';
	if (cp-buf != nr)
		goto formerr;
				/* wake up rendering process */
	if (kill(pid, SIGIO) < 0)
		error(SYSTEM, "cannot signal rendering process in io_process");
	pid = fork();		/* fork i/o process */
	if (pid < 0)
		error(SYSTEM, "fork failed in io_process");
				/* connect to appropriate pipe */
	if (pid) {			/* parent passes renderer output */
		if (freopen(pfout, "r", stdin) == NULL)
			error(SYSTEM, "cannot open input pipe in io_process");
	} else {			/* child passes renderer input */
		if (freopen(pfin, "w", stdout) == NULL)
			error(SYSTEM, "cannot open input pipe in io_process");
	}
				/* pass input to output */
	cp = buf; n = sizeof(buf);
	while ((nr = read(fileno(stdin), cp, n)) > 0) {
		nr += cp-buf;
		if ((n = write(fileno(stdout), buf, nr)) <= 0)
			error(SYSTEM, "write error in io_process");
		cp = buf;
		while (n < nr)
			*cp++ = buf[n++];
		n = sizeof(buf) - (cp-buf);
	}
	fclose(stdin);
	fclose(stdout);
	if (pid)		/* parent waits for child */
		wait(0);
	exit(0);		/* all done, exit (not quit!) */
formerr:
	error(USER, "format error in persist file");
}
