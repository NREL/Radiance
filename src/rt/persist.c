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
#include "selcall.h"
#include <signal.h>
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
	lseek(persistfd, 0L, 0);
				/* wait TIMELIM for someone to signal us */
	got_io = 0;
	signal(SIGIO, sig_io);
	oldalrm = (int (*)())signal(SIGALRM, sig_alrm);
	alarm(TIMELIM);
	pflock(0);			/* unlock persist file for attach */
	while (!got_io)
		pause();		/* wait for attach */
	alarm(0);			/* turn off alarm */
	signal(SIGALRM, oldalrm);
	signal(SIGIO, SIG_DFL);
	pflock(1);			/* grab persist file back */
				/* someone wants us; reopen stdin and stdout */
	if (freopen(inpname, "r", stdin) == NULL)
		goto openerr;
	if (freopen(outpname, "w", stdout) == NULL)
		goto openerr;
	sleep(3);		/* give them a chance to open their pipes */
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
	int	fdout, fderr = -1;
	int	status = 0;
	fd_set	readfds, excepfds;
					/* load persist file */
	n = 40;
	while ((nr = read(persistfd, buf, sizeof(buf)-1)) == 0) {
		if (!n--)
			error(USER, "unattended persist file?");
		pflock(0);
		sleep(15);		/* wait until ready */
		pflock(1);
	}
	if (nr < 0)
		error(SYSTEM, "error reading persist file");
	ftruncate(persistfd, 0L);	/* truncate persist file */
	pfdetach();			/* close & release persist file */
	buf[nr] = '\0';			/* parse what we got */
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
					/* fork child feeder process */
	pid = fork();
	if (pid < 0)
		error(SYSTEM, "fork failed in io_process");
	if (pid == 0) {			/* feeder loop */
		int	fdin;
		close(1);		/* open input pipe */
		if ((fdin = open(pfin, O_WRONLY)) < 0)
			error(SYSTEM, "cannot open feed pipe in io_process");
						/* renderer stdin */
		while ((nr = read(0, cp=buf, sizeof(buf))) > 0) {
			do {
				if ((n = write(fdin, cp, nr)) <= 0)
					goto writerr;
				cp += n;
			} while ((nr -= n) > 0);
		}
		if (nr < 0)
			goto readerr;
		_exit(0);
	}
	close(0);
					/* open output pipes, in order */
	if ((fdout = open(pfout, O_RDONLY)) < 0)
		error(SYSTEM, "cannot open output pipe in io_process");
	if (pferr[0] && (fderr = open(pferr, O_RDONLY)) < 0)
		error(SYSTEM, "cannot open error pipe in io_process");
	for ( ; ; ) {			/* eater loop */
		FD_ZERO(&readfds);
		FD_ZERO(&excepfds);
		nfds = 0;
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
		if (select(nfds, &readfds, NULL, &excepfds, NULL) < 0)
			error(SYSTEM, "error in select call in io_process");
						/* renderer stderr */
		if (fderr >= 0 && (FD_ISSET(fderr, &readfds) ||
				FD_ISSET(fderr, &excepfds))) {
			nr = read(fderr, cp=buf, sizeof(buf));
			if (nr < 0)
				goto readerr;
			if (nr == 0) {
				close(fderr);
				/* close(2);	don't close stderr! */
				fderr = -1;
			} else
				cp[nr] = '\0';	/* deduce status if we can */
				n = strlen(progname);
				if (!strncmp(cp, progname, n) &&
						cp[n++] == ':' &&
						cp[n++] == ' ') {
					register struct erract	*ep;
					for (ep = erract; ep < erract+NERRS;
							ep++)
						if (ep->pre[0] &&
							!strncmp(cp+n, ep->pre,
							    strlen(ep->pre))) {
							status = ep->ec;
							break;
						}
				}
				do {		/* write message */
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
	}
	wait(0);		/* wait for feeder process */
	_exit(status);
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
