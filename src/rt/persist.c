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
					/* select call compatibility stuff */
#ifndef FD_SETSIZE
#include <sys/param.h>
#define FD_SETSIZE	NOFILE		/* maximum # select file descriptors */
#endif
#ifndef FD_SET
#ifndef NFDBITS
#define NFDBITS		(8*sizeof(int))	/* number of bits per fd_mask */
#endif
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	bzero((char *)(p), sizeof(*(p)))
#endif

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
	if (errfile == NULL)
		close(fileno(stderr));
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


io_process()		/* just act as go-between for actual process */
{
	register char	*cp;
	register int	nr, n;
	char	buf[BUFSIZ], *pfin, *pfout, *pferr;
	int	pid, nfds;
	int	fdin, fdout, fderr = -1;
	fd_set	readfds, writefds, excepfds;
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
					/* open named pipes, in order */
	if ((fdin = open(pfin, O_WRONLY)) < 0)
		error(SYSTEM, "cannot open input pipe in io_process");
	if ((fdout = open(pfout, O_RDONLY)) < 0)
		error(SYSTEM, "cannot open output pipe in io_process");
	if (pferr[0] && (fderr = open(pferr, O_RDONLY)) < 0)
		error(SYSTEM, "cannot open error pipe in io_process");
	for ( ; ; ) {			/* loop on select call */
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&excepfds);
		nfds = 0;
		if (fdin >= 0) {
			FD_SET(fdin, &writefds);
			FD_SET(fdin, &excepfds);
			nfds = fdin+1;
		}
		if (fdout >= 0) {
			FD_SET(fdout, &readfds);
			FD_SET(fdout, &excepfds);
			nfds = fdout+1;
		}
		if (fderr >= 0) {
			FD_SET(fderr, &readfds);
			FD_SET(fderr, &excepfds);
			nfds = fderr+1;
		}
		if (nfds == 0)
			break;			/* all done, exit */
		if (select(nfds, &readfds, &writefds, &excepfds, NULL) < 0)
			error(SYSTEM, "error in select call in io_process");
						/* renderer stderr */
		if (fderr >= 0 && (FD_ISSET(fderr, &readfds) ||
				FD_ISSET(fderr, &excepfds))) {
			nr = read(fderr, cp=buf, sizeof(buf));
			if (nr < 0)
				goto readerr;
			if (nr == 0) {
				close(fderr);
				close(2);
				fderr = -1;
			} else
				do {		/* write it all */
					if ((n = write(2, cp, nr)) <= 0)
						goto writerr;
					cp += n;
				} while ((nr -= n) > 0);
		}
						/* renderer stdout */
		if (fdout >= 0 && (FD_ISSET(fdout, &readfds) ||
				FD_ISSET(fdout, &excepfds))) {
			nr = read(fdout, cp=buf, sizeof(buf));
			if (nr < 0)
				goto readerr;
			if (nr == 0) {		/* EOF */
				close(fdout);
				close(1);
				fdout = -1;
			} else
				do {		/* write it all */
					if ((n = write(1, cp, nr)) <= 0)
						goto writerr;
					cp += n;
				} while ((nr -= n) > 0);
		}
						/* renderer stdin */
		if (fdin >= 0 && (FD_ISSET(fdin, &writefds) ||
				FD_ISSET(fdin, &excepfds))) {
			nr = read(0, buf, 512);	/* use minimal buffer */
			if (nr < 0)
				goto readerr;
			if (nr == 0) {
				close(0);
				close(fdin);
				fdin = -1;
			} else if (write(fdin, buf, nr) < nr)
				goto writerr;	/* what else to do? */
		}
	}
	_exit(0);		/* we ought to return renderer error status! */
formerr:
	error(USER, "format error in persist file");
readerr:
	error(SYSTEM, "read error in io_process");
writerr:
	error(SYSTEM, "write error in io_process");
}

#else

pfclean() {}

#endif
