/* Copyright (c) 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  devcomm.c - communication routines for separate drivers.
 *
 *	10/5/88
 */

#include "standard.h"

#include "color.h"

#include "driver.h"

#ifndef DEVPATH
#define DEVPATH		getenv("PATH")	/* device search path */
#endif

#ifndef BSD
#define vfork		fork
#endif

extern char	*getpath(), *getenv();

int	onsigio();

int	comm_close(), comm_clear(), comm_paintr(), comm_errout(),
		comm_getcur(), comm_comout(), comm_comin(), comm_flush();

struct driver	comm_driver = {
	comm_close, comm_clear, comm_paintr, comm_getcur,
	comm_comout, comm_comin, comm_flush
};

FILE	*devin, *devout;

int	devchild;


struct driver *
comm_init(dname, id)			/* set up and execute driver */
char	*dname, *id;
{
	char	*devname;
	int	p1[2], p2[2];
	char	pin[16], pout[16];
						/* find driver program */
	if ((devname = getpath(dname, DEVPATH, 1)) == NULL) {
		stderr_v(dname);
		stderr_v(": not found\n");
		return(NULL);
	}
						/* open communication pipes */
	if (pipe(p1) == -1 || pipe(p2) == -1)
		goto syserr;
	if ((devchild = vfork()) == 0) {	/* fork driver process */
		close(p1[1]);
		close(p2[0]);
		sprintf(pin, "%d", p1[0]);
		sprintf(pout, "%d", p2[1]);
		execl(devname, dname, pin, pout, id, 0);
		perror(devname);
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
						/* verify & get resolution */
	putw(COM_SENDM, devout);
	fflush(devout);
	if (getw(devin) != COM_RECVM)
		return(NULL);
	fread((char *)&comm_driver.pixaspect,
			sizeof(comm_driver.pixaspect), 1, devin);
	comm_driver.xsiz = getw(devin);
	comm_driver.ysiz = getw(devin);
						/* set error vectors */
	cmdvec = comm_comout;
	if (wrnvec != NULL)
		wrnvec = comm_comout;
	return(&comm_driver);
syserr:
	perror(dname);
	return(NULL);
}


static
comm_close()			/* done with driver */
{
	int	pid;

	cmdvec = NULL;				/* reset error vectors */
	if (wrnvec != NULL)
		wrnvec = stderr_v;
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
	putw(xres, devout);
	putw(yres, devout);
	fflush(devout);
}


static
comm_paintr(col, xmin, ymin, xmax, ymax)	/* paint a rectangle */
COLOR	col;
int	xmin, ymin, xmax, ymax;
{
	putc(COM_PAINTR, devout);
	fwrite((char *)col, sizeof(COLOR), 1, devout);
	putw(xmin, devout);
	putw(ymin, devout);
	putw(xmax, devout);
	putw(ymax, devout);
}


static
comm_flush()				/* flush output to driver */
{
	putc(COM_FLUSH, devout);
	fflush(devout);
	if (getc(devin) != COM_FLUSH)
		reply_error("flush");
	comm_driver.inpready = getw(devin);
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
	*xp = getw(devin);
	*yp = getw(devin);
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
comm_comin(buf, prompt)			/* read string from command line */
char	*buf;
char	*prompt;
{
	putc(COM_COMIN, devout);
	if (prompt == NULL)
		putc(0, devout);
	else {
		putc(1, devout);
		myputs(prompt, devout);
	}
	fflush(devout);
	if (getc(devin) != COM_COMIN)
		reply_error("comin");
	mygets(buf, devin);
	comm_driver.inpready = getw(devin);
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
