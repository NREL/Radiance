/* Copyright (c) 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  devcomm.c - communication routines for separate drivers.
 *
 *	10/5/88
 */

#include <stdio.h>

#include <signal.h>

#include "color.h"

#include "driver.h"

#ifndef DEVPATH
#define DEVPATH		getenv("PATH")	/* device search path */
#endif

#ifndef BSD
#define vfork		fork
#endif

#ifndef WFLUSH
#define WFLUSH		30		/* flush after this many rays */
#endif

extern char	*getpath(), *getenv();

int	onsigio();

int	comm_close(), comm_clear(), comm_paintr(), comm_errout(),
		comm_getcur(), comm_comout(), comm_comin();

struct driver	comm_driver, comm_default = {
	comm_close, comm_clear, comm_paintr, comm_getcur,
	comm_comout, comm_comin,
	MAXRES, MAXRES, 0
};

FILE	*devin, *devout;

int	devchild;


struct driver *
comm_init(argv)			/* set up and execute driver */
char	*argv[];
{
	char	*devname;
	int	p1[2], p2[2];

	if ((devname = getpath(argv[0], DEVPATH, 1)) == NULL) {
		stderr_v(argv[0]);
		stderr_v(": not found\n");
		return(NULL);
	}
	if (pipe(p1) == -1 || pipe(p2) == -1)
		goto syserr;
	if ((devchild = vfork()) == 0) {
		close(p1[1]);
		close(p2[0]);
		if (p1[0] != 0) {
			dup2(p1[0], 0);
			close(p1[0]);
		}
		if (p2[1] != 1) {
			dup2(p2[1], 1);
			close(p2[1]);
		}
		execv(devname, argv);
		stderr_v(devname);
		stderr_v(": cannot execute\n");
		_exit(127);
	}
	if (devchild == -1)
		goto syserr;
	close(p1[0]);
	close(p2[1]);
	if ((devout = fdopen(p1[1], "w")) == NULL)
		goto syserr;
	if ((devin = fdopen(p2[0], "r")) == NULL)
		goto syserr;
	bcopy(&comm_default, &comm_driver, sizeof(comm_driver));
	signal(SIGIO, onsigio);
	cmdvec = comm_comout;			/* set error vectors */
	if (wrnvec != NULL)
		wrnvec = comm_comout;
	return(&comm_driver);
syserr:
	perror(argv[0]);
	return(NULL);
}


static
comm_close()			/* done with driver */
{
	int	pid;

	cmdvec = NULL;				/* reset error vectors */
	if (wrnvec != NULL)
		wrnvec = stderr_v;
	signal(SIGIO, SIG_DFL);
	fclose(devout);
	fclose(devin);
	while ((pid = wait(0)) != -1 && pid != devchild)
		;
}


static
comm_clear(xres, yres)				/* clear screen */
int	xres, yres;
{
	putc(COM_CLEAR, devout);
	fwrite(&xres, sizeof(xres), 1, devout);
	fwrite(&yres, sizeof(yres), 1, devout);
	fflush(devout);
}


static
comm_paintr(col, xmin, ymin, xmax, ymax)	/* paint a rectangle */
COLOR	col;
int	xmin, ymin, xmax, ymax;
{
	extern long	nrays;		/* number of rays traced */
	static long	lastflush = 0;	/* ray count at last flush */

	putc(COM_PAINTR, devout);
	fwrite(col, sizeof(COLOR), 1, devout);
	fwrite(&xmin, sizeof(xmin), 1, devout);
	fwrite(&ymin, sizeof(ymin), 1, devout);
	fwrite(&xmax, sizeof(xmax), 1, devout);
	fwrite(&ymax, sizeof(ymax), 1, devout);
	if (nrays - lastflush >= WFLUSH) {
		fflush(devout);
		lastflush = nrays;
	}
}


static int
comm_getcur(xp, yp)			/* get and return cursor position */
int	*xp, *yp;
{
	int	c;

	putc(COM_GETCUR, devout);
	fflush(devout);
	if (getc(devin) != COM_GETCUR)
		reply_error("getcur");
	c = getc(devin);
	fread(xp, sizeof(*xp), 1, devin);
	fread(yp, sizeof(*yp), 1, devin);
	return(c);
}


static
comm_comout(str)			/* print string to command line */
char	*str;
{
	putc(COM_COMOUT, devout);
	myputs(str, devout);
	fflush(devout);
}


static
comm_comin(buf)				/* read string from command line */
char	*buf;
{
	putc(COM_COMIN, devout);
	fflush(devout);
	if (getc(devin) != COM_COMIN)
		reply_error("comin");
	mygets(buf, devin);
	comm_driver.inpready = 0;
}


static
comm_errout(str)			/* display an error message */
char	*str;
{
	comm_comout(str);
	stderr_v(str);			/* send to standard error also */
}


static
mygets(s, fp)				/* get string from file (with nul) */
register char	*s;
register FILE	*fp;
{
	register int	c;

	while ((c = getc(fp)) != EOF)
		if ((*s++ = c) == '\0')
			return;
	*s = '\0';
}


static
myputs(s, fp)				/* put string to file (with nul) */
register char	*s;
register FILE	*fp;
{
	do
		putc(*s, fp);
	while (*s++);
}


static
reply_error(routine)			/* what should we do here? */
char	*routine;
{
	stderr_v(routine);
	stderr_v(": driver reply error\n");
	quit(1);
}


static
onsigio()				/* input ready */
{
	comm_driver.inpready++;
}
